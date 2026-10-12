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
	{led_proc,100,0},
	{key_proc,10,0},
	
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
	  /* 先设置电压/PWM 周期参数，再开中断 ——
	   * 顺序反了的话，中断刚使能的那几个周期里 Kpwm/HalfTpwm 还是 0（Udc=0 会算出 inf），
	   * Tcmp 会被算成垃圾值。上电时电机没转，实际无害，但不该依赖这个巧合 */
	  Set_Udc_Tpwm_Param(&state->Foc,UDC,TPWM);
	/* 初始化VF参数 */
	Vf_SpeedControl_Param_Init(&state->Vf,50,0,0.001,FOC_TS);
	  /* 硬件初始化（内含启动 PWM + 使能 TIM1_CH4 比较中断）*/
	  Motor_Hardware_Init();
		/* 初始化编码器参数 */
	Encoder_Param_Init(&state->Encoder,ENCODER_LINE,0,Pn,3108);
	/* 上电默认跑 V/F 模式。
	 * 不初始化的话默认值 0 = THETA_MODE_ZERO，电机一上电就被固定矢量吸住不动 */
	state->Theta_Mode = THETA_MODE_VF;
	/* 初始电压：Vq=0 是安全的（上电不转）。
	 * 要让它转：按 KEY3 把 Vq 切到 1V，或把这里直接改成 2.0f */
	state->Foc.Vd = 0.0f;
	state->Foc.Vq = 0.0f;
}
/**
 * @brief 电机状态机运行函数
 * @param state : 电机状态机结构体指针
 * @return 无
 */
void Motor_StateMachine_Run(Motor_TypeDef *state)
{
	/* 角度模式选择 */
	switch(state->Theta_Mode)
	{
		/* 定位状态 */
		case THETA_MODE_ZERO:
			state->Foc.Theta_U16 = 0;
		break;
		/* VF角度自增状态 */
		case THETA_MODE_VF:
			/* V/F 直接输出 uint16 角度，与编码器同一套刻度 —— 不需要任何浮点转换 */
			Vf_SpeedControl_Update(&state->Vf,&state->Foc.Theta_U16);
		break;
		/* 编码器 */
		case THETA_MODE_ENCODER:
			state->Foc.Theta_U16 = state->Encoder.Theta_e_U16;
		break;
		default:
		break;
		
	}
//    state->Foc.Theta     = Theta;
//	/* sin查表 */
//    state->Foc.Theta_U16 = ANGLE_FROM_RAD(Theta);
	/* 设置Vd,Vq */
//    state->Foc.Vd = 0.0f;
//    state->Foc.Vq = 0.0f;

	/* 坐标变换更新 */
	Foc_VoltageUpdate(&state->Foc);
	/* 将计算出的三相CCR比较值赋值给三路CCR比较值寄存器 */
	Motor_SetPWM(state->Foc.Tcmp1,state->Foc.Tcmp2,state->Foc.Tcmp3);
}









