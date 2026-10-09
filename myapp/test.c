#include "test.h"

/* 中断占用观察结构体，全局仅此一个 */
Cpu_Stat_Typedef CpuStat;

/* 中断入口时刻。只被 TIM4 一个中断使用，同优先级不会自我嵌套，单个变量够用 */
static uint32_t isr_start;

/**
 * @brief  中断占用检测初始化
 * @param  无
 * @return 无
 * @note   使能内核 DWT 周期计数器，必须在 TIM4 中断启动之前调用
 */
void CpuStat_Init(void)
{
	/* 打开内核调试跟踪，再开 DWT 的 CYCCNT 计数 */
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

	CpuStat.cycles_last  = 0;
	CpuStat.cycles_avg   = 0;
	CpuStat.cycles_max   = 0;
	CpuStat.cycles_sum   = 0;
	CpuStat.isr_cnt      = 0;
	CpuStat.load_x10     = 0;
	CpuStat.load_max_x10 = 0;
	isr_start            = 0;
}

/**
 * @brief  中断入口打点（放中断第一行）
 * @param  无
 * @return 无
 */
void CpuStat_IsrBegin(void)
{
	isr_start = DWT->CYCCNT;
}

/**
 * @brief  中断出口结算（放中断最后一行）
 * @param  无
 * @return 无
 */
void CpuStat_IsrEnd(void)
{
	/* 32 位减法天然处理计数器回绕（59.6 秒一圈） */
	uint32_t used = DWT->CYCCNT - isr_start;

	CpuStat.cycles_last = used;
	if (used > CpuStat.cycles_max)
	{
		CpuStat.cycles_max = used;
	}

	CpuStat.cycles_sum += used;

	/* 每 1000 次中断结算一次平均占用率 */
	if (++CpuStat.isr_cnt >= CPU_STAT_WINDOW)
	{
		CpuStat.cycles_avg = CpuStat.cycles_sum / CpuStat.isr_cnt;

		/* 占用率 x10：平均周期 / 预算周期 * 1000 */
		CpuStat.load_x10 = (uint16_t)(CpuStat.cycles_avg * 1000U / CPU_STAT_BUDGET);
		if (CpuStat.load_x10 > CpuStat.load_max_x10)
		{
			CpuStat.load_max_x10 = CpuStat.load_x10;
		}

		CpuStat.isr_cnt    = 0;
		CpuStat.cycles_sum = 0;
	}
}
