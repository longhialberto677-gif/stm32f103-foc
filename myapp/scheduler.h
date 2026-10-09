#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include "bsp_system.h"
#include "FocAlgorithm.h"
void scheduler_init();
void scheduler_run();

typedef struct 
{
	Foc_TypeDef      Foc;   /* FOC ״̬ */
}Motor_TypeDef;
void Motor_StateMachine_Init(Motor_TypeDef *state);
void Motor_StateMachine_Run(Motor_TypeDef *state);

extern Motor_TypeDef MotorSystem;

#endif
