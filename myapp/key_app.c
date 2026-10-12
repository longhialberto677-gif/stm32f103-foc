#include "key_app.h"


uint8_t key_id = 0;
uint8_t key_down = 0;
uint8_t key_up = 0;
uint8_t key_old = 0;

uint8_t key_read()
{
	uint8_t temp = 0;
	if(HAL_GPIO_ReadPin(KEY1_GPIO_Port,KEY1_Pin) == RESET)
		temp = 1;
	if(HAL_GPIO_ReadPin(KEY2_GPIO_Port,KEY2_Pin) == RESET)
		temp = 2;
	if(HAL_GPIO_ReadPin(KEY3_GPIO_Port,KEY3_Pin) == RESET)
		temp = 3;
	if(HAL_GPIO_ReadPin(KEY4_GPIO_Port,KEY4_Pin) == RESET)
		temp = 4;
	return temp;
	
}
uint8_t longpush_flag = 0;
uint8_t push_flag = 0;
void key_proc()
{
	static uint32_t time_now = 0;
	key_id = key_read();
	key_down = key_id & (key_id ^ key_old);
	key_up = ~key_id & (key_id ^ key_old);
	key_old = key_id;
	if(key_down == 1)
	{
		 MotorSystem.Theta_Mode = (MotorThetaMode_Enum)
         ((MotorSystem.Theta_Mode + 1) % 3); 
	}
	if(key_down == 2)
	{
		if(MotorSystem.Foc.Vd == 0.0f)
			MotorSystem.Foc.Vd = 1.0f;
		else if(MotorSystem.Foc.Vd == 1.0f)
			MotorSystem.Foc.Vd = 0.0f;
	}
	if(key_down == 3)
	{
		if(MotorSystem.Foc.Vq == 0.0f)
			MotorSystem.Foc.Vq = 1.0f;
		else if(MotorSystem.Foc.Vq == 1.0f)
			MotorSystem.Foc.Vq = 0.0f;
	}
	if(key_down == 4)
	{

	}
	
	
}