#ifndef __TDTRAKCER_H__
#define __TDTRAKCER_H__

#include "stdint.h"

/* ============================================================================
 * 微分跟踪器（二阶线性 TD，临界阻尼）
 * ----------------------------------------------------------------------------
 * 连续形式：
 *        y'' = wn^2*(u - y) - 2*wn*y'
 *
 * 稳态时 y'' = y' = 0 -> y = u，所以它输出的是输入的【平滑版本】，
 * 同时白送一阶导 y'（将来的速度环可以直接拿它当加速度/前馈用）。
 *
 * 离散化时把 Ts 吸收进系数，运行时只剩乘加：
 *        y'  += K1*(u-y) - K2*y'      K1 = wn^2*Ts
 *         y  += K3*y'                 K2 = 2*wn*Ts , K3 = Ts
 *
 * 定点实现：状态 Q16（电压 x 65536），系数 Q20（保留 Ts 的精度）。
 * 实测：定点内核约 25 cycle（3 条 smull，零函数调用）；
 *       浮点版要 900 cycle（12 次软浮点调用）---- F103 没有 FPU
 * ==========================================================================*/
typedef struct
{
	/* ---------- 用户参数（可以在 Keil 的 Watch 窗口里直接改）---------- */
	float Input;        /* 目标值（V）---- debug 时改这个就行 */
	float Omega_n;      /* 收敛因子（rad/s）：越大跟得越快，100 约等于 16Hz 带宽 */
	float Ts;           /* 调用周期（s）---- 必须和【实际调用频率】一致 */

	/* ---------- 预计算系数（Q20）---------- */
	int32_t K1;         /* wn^2*Ts  x 2^20 */
	int32_t K2;         /* 2*wn*Ts  x 2^20 */
	int32_t K3;         /* Ts       x 2^20 */
	/* 记录系数是按哪组参数算的：Watch 里改了 Omega_n/Ts，下一拍自动重算 ✓ */
	float   Wn_Last;
	float   Ts_Last;

	/* ---------- 状态（Q16）---------- */
	int32_t y;          /* 跟踪值（= 输出）  单位：V   x 65536 */
	int32_t y_dot;      /* 一阶导            单位：V/s x 65536 */

	/* ---------- 观察量（float 镜像，方便 Watch / VOFA）---------- */
	/* y_dot 的 float 版本故意不常驻：常驻要多花 2 次软浮点(约 140 cycle)，
	 * 需要看的时候在 Watch 里写表达式 (float)y_dot/65536.0f 就行 ✓ */
	float y_f;          /* = y 的浮点值（也就是 UpData 的返回值）*/

}TdTrakcer_TypeDef;

#include "bsp_system.h"

void    TdTrakcer_Param_Init(TdTrakcer_TypeDef *state,float u,float wn,float ts);
float   TdTrakcer_UpData(TdTrakcer_TypeDef *state,float u);            /* 带浮点转换的外层，低频调用 */
int32_t TdTrakcer_Update_Q16(TdTrakcer_TypeDef *state,int32_t u_q16);  /* 纯整数内核，可进高频中断 */

#endif