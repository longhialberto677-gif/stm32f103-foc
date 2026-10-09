#include "interrupt.h"

/* myapp/interrupt.c —— 触发点就是 CH4 比较匹配，硬件 20kHz */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    static uint8_t n = 0;
    if (htim->Instance == TIM1 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_4)
    {
        if (++n & 1) return;                    /* ← 奇数拍：直接返回，什么也不干 */
        Motor_StateMachine_Run(&MotorSystem);   /* ← 偶数拍：真正跑 FOC */
    }
}
//void TIM4_IRQHandler(void)
//{
//  /* USER CODE BEGIN TIM4_IRQn 0 */
//  CpuStat_IsrBegin();
//  /* USER CODE END TIM4_IRQn 0 */
//  HAL_TIM_IRQHandler(&htim4);
//  /* USER CODE BEGIN TIM4_IRQn 1 */
//  CpuStat_IsrEnd();
//  /* USER CODE END TIM4_IRQn 1 */
//}
