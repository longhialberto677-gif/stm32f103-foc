#include "FocAlgorithm.h"



/* ============================================================================
 * 反 Park 变换（查表版）
 *
 * 原来的 cosf/sinf 每次要 2500 个周期，F103 没有 FPU，
 * 20kHz 中断里根本跑不完。改成查表后约 1.7us。
 * ==========================================================================*/
void Rev_Park_Transf(Foc_TypeDef *state)
{
    float s, c;

    /* 从结构体里取 uint16 角度，一次查表同时得到 sin/cos */
    Foc_SinCos_U16(state->Theta_U16, &s, &c);

    state->Valpha = state->Vd * c - state->Vq * s;
    state->Vbeta  = state->Vd * s + state->Vq * c;
}

/* ============================================================================
 * 反 Clark 变换
 * ==========================================================================*/
void Rev_Clark_Transf(Foc_TypeDef *state)
{
    state->Vu = state->Valpha;
    state->Vv = -0.5f * state->Valpha + 0.8660254f * state->Vbeta;
    state->Vw = -0.5f * state->Valpha - 0.8660254f * state->Vbeta;
}

/* ============================================================================
 * 零序分量注入 + 转三相 PWM 比较值
 * ==========================================================================*/
void SVPWM_ZeroSqInject(Foc_TypeDef *state)
{
    float Max;
    float Min;
    float V0;

    /* 寻找三相最大值 */
    Max = (state->Vu > state->Vv) ? ((state->Vu > state->Vw) ? state->Vu : state->Vw)
                                  : ((state->Vv > state->Vw) ? state->Vv : state->Vw);

    /* 寻找三相最小值 */
    Min = (state->Vu < state->Vv) ? ((state->Vu < state->Vw) ? state->Vu : state->Vw)
                                  : ((state->Vv < state->Vw) ? state->Vv : state->Vw);

    V0 = -0.5f * (Max + Min);

    /* 计算三相马鞍波电压 */
    state->Vu_Mod = V0 + state->Vu;
    state->Vv_Mod = V0 + state->Vv;
    state->Vw_Mod = V0 + state->Vw;

    /* 将三相马鞍电压转化成三相PWM比较值
     *   (Vm/Udc + 0.5)*Tpwm  ≡  Vm*(Tpwm/Udc) + 0.5*Tpwm
     * 展开成"1乘1加"之后，每周期 3 次软浮点除法彻底消失（Kpwm/HalfTpwm 在初始化时算好）*/
    state->Tcmp1 = (int)(state->Vu_Mod * state->Kpwm + state->HalfTpwm);
    state->Tcmp2 = (int)(state->Vv_Mod * state->Kpwm + state->HalfTpwm);
    state->Tcmp3 = (int)(state->Vw_Mod * state->Kpwm + state->HalfTpwm);

}
/* ============================================================================
 * 设置母线电压和PWM周期函数
 * ==========================================================================*/
void Set_Udc_Tpwm_Param(Foc_TypeDef*state,float udc_temp,float Tpwm_temp)
{
	state->Udc  = udc_temp;
	state->Tpwm = Tpwm_temp;
	/* 预存下面两个常量：全工程唯一一次除法，只在初始化/改参数时做一次
	 * （以后用 ADC 逐周期补偿母线电压时，这里改成每周期算一次，仍然是 1 次除法换掉 3 次）*/
	state->Kpwm     = Tpwm_temp / udc_temp;
	state->HalfTpwm = 0.5f * Tpwm_temp;
}
/* ============================================================================
 * VF速度控制参数初始化函数
 * ==========================================================================*/
void Vf_SpeedControl_Param_Init(Vf_SpeedControl_TypeDef*state,float acc,float target_speed,float dead_zone,float ts)
{
	state->Acc = acc;
	state->Target_Speed = target_speed;
	state->Dead_Zone = dead_zone;
	state->Ts = ts;
	/* 每 (rad/s) 每周期要加多少 Q16 角度计数：Ts × (65536/2π × 65536)
	 * 初始化预存下来，每个 FOC 周期就只剩 1 次浮点乘法 */
	state->Step_K = ts * ANGLE_Q16_PER_RAD;
}
/* ============================================================================
 * VF速度控制周期函数
 * ==========================================================================*/
void Vf_SpeedControl_Update(Vf_SpeedControl_TypeDef*state,uint16_t *Target_Theta_U16)
{
	/* 计算速度误差 */
	state->Speed_Error = state->Target_Speed - state->Speed_Integrator;
	/* 死区处理 */
	if(fabs(state->Speed_Error) < state->Dead_Zone)
	{
		state->Speed_Error_Dead = 0;
	}
	else
	{
		state->Speed_Error_Dead = state->Speed_Error;
	}
	/* 判断加速度方向 */
	if(state->Speed_Error_Dead > 0)
	{
		state->Acc_Dir = 1;
	}
	else if(state->Speed_Error_Dead < 0)
	{
		state->Acc_Dir = -1;
	}
	else
	{
		state->Acc_Dir = 0;
	}
	/* 加速度积分得到速度 */
	state->Speed_Integrator += state->Acc_Dir * state->Acc * state->Ts;

	/* 速度积分得到角度：累加器用 Q16，高 16 位就是 uint16 角度
	 * 回绕完全靠 uint32 溢出自动完成 —— 不需要 if/while，
	 * 也不会因为 inf/NaN 死循环；正反转（负步长）同样正确（补码回绕）*/
	state->Theta_Acc_Q16 += (uint32_t)(int32_t)(state->Speed_Integrator * state->Step_K);
	state->Theta_U16      = (uint16_t)(state->Theta_Acc_Q16 >> 16);
	state->Speed          = state->Speed_Integrator;

	/* 函数输出：与编码器同一套刻度（uint16 一整圈），切角度源时可以直接赋值 */
	*Target_Theta_U16 = state->Theta_U16;
}

/* ============================================================================
 * 坐标变换电压更新函数
 * ==========================================================================*/
void Foc_VoltageUpdate(Foc_TypeDef*state)
{
	/* 反Park变换 */
	Rev_Park_Transf(state);
	/* 反Clark变换 */
    Rev_Clark_Transf(state);
	/* SVPWM */
    SVPWM_ZeroSqInject(state);
}
