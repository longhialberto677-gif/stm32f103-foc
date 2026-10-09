#include "Hardware_Driver.h"



void Motor_EnablePWM()
{
  HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_3);
  HAL_TIMEx_PWMN_Start(&htim1,TIM_CHANNEL_1);
  HAL_TIMEx_PWMN_Start(&htim1,TIM_CHANNEL_2);
  HAL_TIMEx_PWMN_Start(&htim1,TIM_CHANNEL_3);
}

void Motor_DisablePWM()
{
	HAL_TIM_PWM_Stop(&htim1,TIM_CHANNEL_1);
	HAL_TIM_PWM_Stop(&htim1,TIM_CHANNEL_2);
	HAL_TIM_PWM_Stop(&htim1,TIM_CHANNEL_3);
	HAL_TIMEx_PWMN_Stop(&htim1,TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Stop(&htim1,TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Stop(&htim1,TIM_CHANNEL_3);
}

void Motor_Hardware_Init()
{
	Motor_EnablePWM();
	HAL_TIM_Base_Start_IT(&htim4);
}

void Motor_SetPWM(uint16_t Tcmp1,uint16_t Tcmp2,uint16_t Tcmp3)
{
	__HAL_TIM_SetCompare(&htim1,TIM_CHANNEL_1,Tcmp1);
	__HAL_TIM_SetCompare(&htim1,TIM_CHANNEL_2,Tcmp2);
	__HAL_TIM_SetCompare(&htim1,TIM_CHANNEL_3,Tcmp3);
}









