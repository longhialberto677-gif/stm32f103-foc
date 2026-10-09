#include "scheduler.h"

typedef struct
{
	void (*pointer)(void);
	uint32_t time_work;
	uint32_t time_last;
}task_t;

uint8_t task_num = 0;

task_t task[]=
{
	{led_proc,100,0}
	
};


void scheduler_init()
{
	task_num = sizeof(task)/sizeof(task_t);
}

void scheduler_run()
{
	uint32_t time_now = HAL_GetTick();
	for(uint8_t i=0;i<task_num;i++)
	{
		if(time_now >= task[i].time_last+task[i].time_work)
		{
			task[i].pointer();
			task[i].time_last = time_now;
		}
	}
}



Motor_TypeDef MotorSystem;
/**
 * @brief 电机状态机初始化
 * @param state : 电机状态机结构体指针
 * @return 无
 */
void Motor_StateMachine_Init(Motor_TypeDef *state)
{
	  /* 硬件初始化 */
	  Motor_Hardware_Init();
	  /* 设置电机电压和母线电压PWM周期 */
	  Set_Udc_Tpwm_Param(&state->Foc,UDC,TPWM);
}
/**
 * @brief 电机状态机运行函数
 * @param state : 电机状态机结构体指针
 * @return 无
 */
void Motor_StateMachine_Run(Motor_TypeDef *state)
{
	/* 创建虚拟角度变量 */
	static float Theta = 0.0f;
	/* 角度变量累加 */
    Theta += 0.001f;
	/* 角度限幅 */
    if (Theta >= ANGLE_2PI)
    {
        Theta -= ANGLE_2PI;
    }
	/* 设置电机角度 */
    state->Foc.Theta     = Theta;
	/* sin查表 */
    state->Foc.Theta_U16 = ANGLE_FROM_RAD(Theta);
	/* 设置Vd,Vq */
    state->Foc.Vd = 0.0f;
    state->Foc.Vq = 2.0f;

	/* 坐标变换更新 */
	Foc_VoltageUpdate(&state->Foc);
	/* 将计算出的三相CCR比较值赋值给三路CCR比较值寄存器 */
	Motor_SetPWM(state->Foc.Tcmp1,state->Foc.Tcmp2,state->Foc.Tcmp3);
}









