#ifndef __MOTOR_CONFIG_H__
#define __MOTOR_CONFIG_H__

#include "bsp_system.h"


/* =========== 系统时序常量 ============ */
#define FOC_HZ            10204                      /* FOC中断频率10KHZ */  
#define FOC_TS            (1/(float) FOC_HZ)
	
/* =========== 数学常量 ============ */
#define ANGLE_2PI         6.2831853f                 /* 2*pi，仅用于波形显示换算 */



/* =========== SVPWM常量 ============ */
#define UDC               12.0f
#define TPWM              49.0f





#endif
