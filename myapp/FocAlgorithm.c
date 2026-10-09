#include "FocAlgorithm.h"

//Foc_TypeDef Foc;           //Foc结构体变量

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
//void Rev_Park_Transf(Foc_TypeDef *state)
//{
//    state->Valpha = state->Vd * cosf(state->Theta) - state->Vq * sinf(state->Theta);
//    state->Vbeta  = state->Vd * sinf(state->Theta) + state->Vq * cosf(state->Theta);
//}

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
