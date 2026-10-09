#ifndef __HARDWARE_DRIVER_H__
#define __HARDWARE_DRIVER_H__

#include "bsp_system.h"


void Motor_EnablePWM();
void Motor_DisablePWM();
void Motor_Hardware_Init();
void Motor_SetPWM(uint16_t Tcmp1,uint16_t Tcmp2,uint16_t Tcmp3);
#endif