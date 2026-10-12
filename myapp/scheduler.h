#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include "bsp_system.h"
#include "FocAlgorithm.h"
#include "Encoder.h"
void scheduler_init();
void scheduler_run();

typedef struct 
{
	Foc_TypeDef      Foc;   /* FOC 状态 */
	Vf_SpeedControl_TypeDef          Vf;          /* VF 状态 */
	Encoder_Typedef  Encoder;                     /* 编码器状态 */
}Motor_TypeDef;
void Motor_StateMachine_Init(Motor_TypeDef *state);
void Motor_StateMachine_Run(Motor_TypeDef *state);

extern Motor_TypeDef MotorSystem;

#endif
