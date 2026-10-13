#include "Encoder.h"

/* ============================================================================
 * 角度预测（时间戳 + 实测转速）—— 可调参数
 * ==========================================================================*/
/* 预测最多外推多久：超过就冻结在极限位置。
 * 作用：万一主循环卡住或者编码器掉线，角度不会拿着旧速度无限外推下去 */
#define ENC_PRED_MAX_CYC   (2U * 72000U)   /* 2 ms */
/* 采样间隔超过这个值就重新起算速度（连续几次读失败后，旧速度已经不可信）*/
#define ENC_REBASE_CYC     (5U * 72000U)   /* 5 ms */
/* 执行器提前量：占空比在本拍写下去，要到下一个更新事件才真正作用到电机
 * （载波 20.408kHz 中心对齐 → 更新事件在 24.5us / 49.5us 两处）
 * 这里取 24.5us —— 把角度多推到"电压真正生效"的时刻，能再抵消一点滞后 */
#define ENC_ACT_LEAD_CYC   (1764U)         /* 24.5us x 72MHz */
/* 速度一阶低通：新值占 1/4。
 * 理由：编码器 1 个计数 = 0.088° 机械，直接两帧差分出来的速度噪声很大 */
#define ENC_SPD_FILT_SHIFT 2

/**
 * @brief 使能 DWT 周期计数器（幂等）
 * @note  CpuStat 里也开过，这里再保险一次：
 *        角度预测依赖 DWT 做时间戳，不该因为别处删掉观测代码就失效
 */
static void Encoder_DWT_Enable(void)
{
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
 * @brief 编码器参数初始化
 * @param line：编码器线数
 * @param dir：编码器方向 （0同向，1反向）
 * @param MotorPn：电机极对数
 * @param Phase：零点偏差
 * @return 无
 */
void Encoder_Param_Init(Encoder_Typedef *state,uint32_t line,uint8_t dir,uint8_t MotorPn,uint32_t Phase)
{
	state->Line = line;
	state->Dir = dir;
	state->MotorPn = MotorPn;
	state->Phase = Phase;

	/* 机械计数 → 电角度计数 的增益：1 个机械计数 = (65536/Line) 个机械角度计数，
	 * 再乘极对数就是电角度计数（Line=4096、Pn=7 时 = 16 x 7 = 112）*/
	state->Elec_Gain = (int32_t)((65536U / line) * (uint32_t)MotorPn);

	/* 预测相关全部清零，等第一个有效样本 */
	state->Theta_Mech_Last = 0;
	state->Time_Last       = 0;
	state->Theta_e_Last    = 0;
	state->Speed_e_Q12     = 0;
	state->Pred_Ok         = 0;

	Encoder_DWT_Enable();
}

/**
 * @brief 编码器角度更新函数
 * @param Data：AS5600 读到的原始角度（12 位，0~4095）
 * @return 无
 * @note  与 G431 的差别只在实现：那边是
 *            Theta   = (float)Temp / Line * ANGLE_2PI;
 *            Theta_e = fmod(Theta * MotorPn, ANGLE_2PI);
 *        在 F103 上没有 FPU，一次浮点除法约 307 cycle、一次双精度 fmod 约 612 cycle；
 *        这里换成整数：AS5600 是 12 位，4096 x 16 = 65536，
 *        机械角度就是"除以 Line 再左移 16 位"，电角度就是一次整数乘法，
 *        uint16 溢出天然完成回绕，不需要任何取模或限幅。
 */
void Encoder_UpData(Encoder_Typedef *state,uint32_t Data)
{
	/* 判断编码器方向 */
	if(state->Dir == 0)
	{
		state->Pulse_Data = Data;
	}
	else
	{
		state->Pulse_Data = state->Line - Data;
	}

	/* 减去零点偏差并回绕到一圈之内（先加 Line 防止下溢）*/
	uint32_t Temp = (state->Pulse_Data + state->Line - state->Phase) % state->Line;

	/* 计算电机 机械角度：Temp/Line 圈 映射成 uint16 一整圈
	 * 写成通用式便于以后换编码器；Line=4096 时等价于 Temp<<4 */
	state->Theta_U16 = (uint16_t)(((uint32_t)Temp << 16) / state->Line);

	/* 计算电机 电角度：机械角度 x 极对数，uint16 溢出即自动回绕，零开销 */
	state->Theta_e_U16 = (uint16_t)((uint32_t)state->Theta_U16 * state->MotorPn);
}

/**
 * @brief 采样成功后调用（主循环，约 1kHz）：时间戳 + 实测转速 + 发布
 * @param state      : 编码器结构体
 * @param now_cycles : 本次采样完成的 DWT 周期数（DWT->CYCCNT）
 * @return 无
 * @note  调用前必须已经调用过 Encoder_UpData（要用到最新的 Pulse_Data / Theta_e_U16）
 */
void Encoder_Speed_Update(Encoder_Typedef *state,uint32_t now_cycles)
{
	int32_t  d_mech;
	int32_t  dt;
	int32_t  half;
	uint32_t primask;

	/* ① 机械角度增量：必须先处理跨圈（4095→0 这种），
	 *    直接相减会得到 -4095 的假速度 —— 按最短路径折到 ±半圈 */
	half   = (int32_t)(state->Line / 2U);
	d_mech = (int32_t)state->Pulse_Data - (int32_t)state->Theta_Mech_Last;
	if(d_mech >  half) { d_mech -= (int32_t)state->Line; }
	if(d_mech < -half) { d_mech += (int32_t)state->Line; }

	/* ② 采样间隔：DWT 无符号相减天然处理 32 位回绕（间隔 < 59.6s 都是对的）*/
	dt = (int32_t)(now_cycles - state->Time_Last);

	if((state->Pred_Ok == 0U) || (dt <= 0) || (dt > (int32_t)ENC_REBASE_CYC))
	{
		/* 第一次采样，或者间隔太大（连续读失败）：这次只登记基准，不估速度 */
		state->Speed_e_Q12 = 0;
	}
	else
	{
		/* ③ 电角度增量 = 机械增量 x 增益 */
		/* ④ 速度 = 电角度增量 / 时间间隔，用 Q12 表示 */
		int32_t spd_new = (int32_t)(((int32_t)(d_mech * state->Elec_Gain) << 12) / dt);

		/* ⑤ 一阶低通，压掉量化噪声 */
		state->Speed_e_Q12 += (spd_new - state->Speed_e_Q12) >> ENC_SPD_FILT_SHIFT;
	}

	/* ⑥ 原子发布：FOC 中断在 10kHz 里读这三个量。
	 *    必须一起更新 —— 否则中断可能读到"新时间戳 + 旧角度"，
	 *    反而制造出我们正要消除的那个 1ms 误差。
	 *    （写侧关中断就够了：中断不会在主循环写的过程中进来）*/
	primask = __get_PRIMASK();
	__disable_irq();
	state->Theta_e_Last = state->Theta_e_U16;
	state->Time_Last    = now_cycles;
	state->Pred_Ok      = 1U;
	__set_PRIMASK(primask);

	/* 供下一次差分用（这个量只有主循环会读，不影响中断）*/
	state->Theta_Mech_Last = state->Pulse_Data;
}

/**
 * @brief 取出预测到当前时刻的电角度（在 FOC 中断里调用，10kHz）
 * @param state      : 编码器结构体
 * @param now_cycles : 当前 DWT 周期数（DWT->CYCCNT）
 * @return 电角度（uint16 一整圈）
 * @note  中断里读共享量不需要关中断：写侧（主循环）已经关中断写入，
 *        三个量对中断来说是同时可见的。
 *        成本：2 次乘法 + 几次加减，约 10~20 cycle
 */
uint16_t Encoder_Get_Predicted(Encoder_Typedef *state,uint32_t now_cycles)
{
	uint32_t dt;
	int32_t  adv;

	if(state->Pred_Ok == 0U)
	{
		return state->Theta_e_U16;      /* 还没有有效样本：直接用采样值 */
	}

	/* 从采样时刻推到"这一拍电压实际生效的时刻" */
	dt = (uint32_t)(now_cycles - state->Time_Last) + ENC_ACT_LEAD_CYC;
	if(dt > ENC_PRED_MAX_CYC)
	{
		dt = ENC_PRED_MAX_CYC;          /* 超时：冻结在极限位置，不再继续外推 */
	}

	/* Q12 速度 x 周期数 → 电角度计数。
	 * 反转时 adv 为负，uint16 加法靠补码自动回绕 ✓
	 * 溢出边界：|速度| 要超过 34000rpm 机械才可能溢出，实际到不了 */
	adv = (state->Speed_e_Q12 * (int32_t)dt) >> 12;

	return (uint16_t)(state->Theta_e_Last + (uint16_t)(int16_t)adv);
}
