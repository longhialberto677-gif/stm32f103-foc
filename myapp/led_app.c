#include "led_app.h"

void led_proc()
{
	HAL_GPIO_TogglePin(led_green_GPIO_Port,led_green_Pin);
}