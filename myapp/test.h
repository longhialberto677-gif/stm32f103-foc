#ifndef __TEST_H__
#define __TEST_H__

#include "bsp_system.h"

/* ===================== 中断占用率检测 =====================
 *
 * 用 Cortex-M3 自带的 DWT 周期计数器（72MHz 自由运行）直接数周期：
 *   中断占用率 = 本次中断执行周期数 / 中断周期对应的周期预算
 *
 * 不需要标定、不需要空闲计数，测出来就是硬邦邦的 cycle 数。
 * 用法：Init 一次，在中断入口/出口各插一行，Watch 变量 CpuStat。
 * ========================================================= */

#define CPU_STAT_CPU_HZ   72000000U                       /* 主频 72MHz */
#define CPU_STAT_ISR_HZ   20000U                          /* TIM4 中断 20kHz */
/* 每次中断的周期预算：72MHz / 20kHz = 3600 cycle */
#define CPU_STAT_BUDGET   (CPU_STAT_CPU_HZ / CPU_STAT_ISR_HZ)
/* 结算窗口：1000 次中断 = 50ms */
#define CPU_STAT_WINDOW   1000U

/* 中断占用观察结构体：Keil 调试时把 CpuStat 加进 Watch 窗口即可 */
typedef struct
{
	uint32_t cycles_last;   /* 上一次中断花了多少 cycle（看这个最快） */
	uint32_t cycles_avg;    /* 窗口内平均 cycle */
	uint32_t cycles_max;    /* 历史最长的一次中断 cycle */
	uint32_t cycles_sum;    /* 窗口内累计，内部使用 */
	uint32_t isr_cnt;       /* 窗口内中断次数，内部使用 */
	uint16_t load_x10;      /* 当前占用率 x10，300 表示 30.0% */
	uint16_t load_max_x10;  /* 历史最高占用率 x10 */
} Cpu_Stat_Typedef;

extern Cpu_Stat_Typedef CpuStat;

/* DWT 使能 + 清零，必须在中断启动前调用（电机初始化之前） */
void CpuStat_Init(void);

/* 中断入口第一行调用 */
void CpuStat_IsrBegin(void);

/* 中断出口最后一行调用 */
void CpuStat_IsrEnd(void);

#endif
