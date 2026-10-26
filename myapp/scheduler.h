#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include "bsp_system.h"
#include "FocAlgorithm.h"
#include "Encoder.h"
#include "TdTrakcer.h"
void scheduler_init();
void scheduler_run();
/**
 * @brief 电机角度状态枚举
 * 
 * 
 */
typedef enum
{
	THETA_MODE_ZERO = 0,            /* 固定角度零度 */
	THETA_MODE_VF,                  /* VF角度自增 */
	THETA_MODE_ENCODER,             /* 编码器角度 */
	
}MotorThetaMode_Enum;	

typedef struct 
{
	MotorThetaMode_Enum              Theta_Mode;   /* 角度状态 */
	Foc_TypeDef      Foc;   /* FOC 状态 */
	Vf_SpeedControl_TypeDef          Vf;          /* VF 状态 */
	Encoder_Typedef  Encoder;                     /* 编码器状态 */
	uint8_t          Td_Tick;                     /* TD 分频计数器（10.2kHz → 1kHz）*/
	TdTrakcer_TypeDef                Td_Track_Uq; /* uq信号跟踪器 */
}Motor_TypeDef;
void Motor_StateMachine_Init(Motor_TypeDef *state);
void Motor_StateMachine_Run(Motor_TypeDef *state);

extern Motor_TypeDef MotorSystem;

#endif
