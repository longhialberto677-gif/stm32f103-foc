#ifndef __HARDWARE_DRIVER_H__
#define __HARDWARE_DRIVER_H__

#include "bsp_system.h"


void Motor_EnablePWM();
void Motor_DisablePWM();
void Motor_Hardware_Init();
void Motor_SetPWM(uint16_t Tcmp1,uint16_t Tcmp2,uint16_t Tcmp3);
uint16_t AS5600_ReadData(void);
void I2C_BusRecover(void);        /* I2C 总线锁死时的恢复（故障处理，保留）*/
#endif