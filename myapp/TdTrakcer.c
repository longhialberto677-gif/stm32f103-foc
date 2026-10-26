#include "TdTrakcer.h"

/* Q 格式：状态 Q16（电压 x 65536），系数 Q20（让毫秒级的 Ts 也有足够精度）*/
#define TD_ONE_STATE     65536.0f            /* 2^16 */
#define TD_Q_COEF        20                   /* 2^20 */

/**
 * @brief 按当前的 Omega_n / Ts 重算 Q20 系数
 * @note  原来只在 Param_Init 里算一次，导致【运行时改 Omega_n 完全没有效果】✗
 *        现在 UpData 会检测变化并重算，方便在 Keil 的 Watch 窗口里现场调参 ✓
 */
static void TdTrakcer_Recalc(TdTrakcer_TypeDef *state)
{
	state->Wn_Last = state->Omega_n;
	state->Ts_Last = state->Ts;

	state->K1 = (int32_t)(state->Omega_n * state->Omega_n * state->Ts * 1048576.0f);
	state->K2 = (int32_t)(2.0f * state->Omega_n * state->Ts * 1048576.0f);
	state->K3 = (int32_t)(state->Ts * 1048576.0f);
}

/**
 * @brief 微分跟踪器参数初始化（只在上电跑一次，这里用浮点无所谓）
 * @param state ：跟踪器结构体指针
 * @param u     ：输入目标初值
 * @param wn    ：收敛因子（rad/s）
 * @param ts    ：调用周期（s）---- 必须等于实际调用间隔
 * @return 无
 */
void TdTrakcer_Param_Init(TdTrakcer_TypeDef *state,float u,float wn,float ts)
{
	state->Input   = u;
	state->Omega_n = wn;
	state->Ts      = ts;

	TdTrakcer_Recalc(state);

	/* 状态清零 ---- 以前漏了这一步，只是靠 MotorSystem 是全局变量（自动清零）碰巧对了 */
	state->y       = 0;
	state->y_dot   = 0;
	state->y_f     = 0.0f;
}

/**
 * @brief 微分跟踪器纯整数内核
 * @param state ：跟踪器结构体指针
 * @param u_q16 ：输入（Q16，即 电压 x 65536）
 * @return 跟踪值（Q16）
 * @note  实测约 25 cycle：3 条 smull + 移位/加法，没有一次函数调用
 */
int32_t TdTrakcer_Update_Q16(TdTrakcer_TypeDef *state,int32_t u_q16)
{
	int32_t err = u_q16 - state->y;

	/* y' += K1*(u-y) - K2*y'   （系数 Q20 x 状态 Q16，右移 20 位转回 Q16）*/
	state->y_dot += (int32_t)(((int64_t)state->K1 * err)           >> TD_Q_COEF)
	              - (int32_t)(((int64_t)state->K2 * state->y_dot) >> TD_Q_COEF);

	/* y += K3*y' */
	state->y += (int32_t)(((int64_t)state->K3 * state->y_dot) >> TD_Q_COEF);

	return state->y;
}

/**
 * @brief 微分跟踪器周期函数（带浮点转换的外层）
 * @param state ：跟踪器结构体指针
 * @param u     ：输入（V）
 * @return 跟踪值 y（V）
 * @note  约 280 cycle，基本都花在 3 次浮点转换上
 *        ---- 所以这个函数只适合 1kHz 左右的低频调用，不要放进 10kHz 的 FOC 中断
 */
float TdTrakcer_UpData(TdTrakcer_TypeDef *state,float u)
{
	int32_t u_q16;
	/* 每 128 拍（约 125ms）才检查一次参数有没有被改 ——
	 * 浮点比较本身要 ~140 cycle，每拍都做太浪费；而人工调参不在乎这 125ms 延迟 ✓ */
	static uint8_t chk_cnt = 0;

	if(++chk_cnt >= 128U)
	{
		chk_cnt = 0;
		if((state->Omega_n != state->Wn_Last) || (state->Ts != state->Ts_Last))
		{
			TdTrakcer_Recalc(state);      /* Watch 里改了参数 → 立刻重算系数 ✓ */
		}
	}

	u_q16 = (int32_t)(u * TD_ONE_STATE);

	TdTrakcer_Update_Q16(state,u_q16);

	/* 刷新 float 镜像，方便 Watch 窗口 / VOFA 观察 */
	state->y_f = (float)state->y * (1.0f / TD_ONE_STATE);

	return state->y_f;
}