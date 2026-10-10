#ifndef __FOCALGORITHM_H__
#define __FOCALGORITHM_H__

#include "stdint.h"
/**
 * @brief FOC参数结构体
 *
 */
typedef struct
{
	float Udc;        /* 母线电压 */
	float Tpwm;       /* PWM周期（= TIM1 ARR） */
	float Kpwm;       /* 预存 Tpwm/Udc：把每周期 3 次除法折成常量，中断里只做乘法 */
	float HalfTpwm;   /* 预存 0.5f*Tpwm：SVPWM 的直流偏置项 */
    float Vd;        /* d轴电压 */
    float Vq;        /* q轴电压 */
    float Theta;     /* 电机角度（弧度 0~2*pi，给 VOFA 看波形用）*/
    uint16_t Theta_U16;  /* 电机角度（uint16角，查表运算用，中断里直接累加）*/

    float Valpha;    /* alpha电压 */
    float Vbeta;     /* beta电压 */

    float Vu;        /* U相电压 */
    float Vv;        /* V相电压 */
    float Vw;        /* W相电压 */

    float Vu_Mod;    /* U相调制电压 */
    float Vv_Mod;    /* V相调制电压 */
    float Vw_Mod;    /* W相调制电压 */
	
	int Tcmp1;       /* U相比较值 */
	int Tcmp2;       /* V相比较值 */
	int Tcmp3;       /* W相比较值 */
}Foc_TypeDef;

/**
 * @brief Vf角度生成器参数结构体
 *
 */
typedef struct
{
	/* 输入参数 */
	float Acc;               /* 加速度 */
	float Target_Speed;      /* 目标速度 */
	float Dead_Zone;         /* 死区阈值 */
	float Ts;                /* 步长 */
	
	/* 中间参数 */
	float Speed_Error;       /* 速度误差 */
	float Speed_Error_Dead;  /* 带死区的速度误差 */
	float Acc_Dir;           /* 加速度方向 */
	float Speed_Integrator;  /* 速度积分器 */
	float Theta_Integrator;  /* 角度积分器 */
	
	/* 输出参数 */
	float Speed;             /* 实际速度 */
	float Theta;             /* 实际角度 */
	
	
}Vf_SpeedControl_TypeDef;

#include "bsp_system.h"


/* ============================================================================
 * 角度表示约定（超重要，整套 FOC 统一用这个，别再混用弧度）
 * ----------------------------------------------------------------------------
 *   角度用 uint16_t 表示一整圈 2*pi：
 *       uint16_t 0      <->   0    rad
 *       uint16_t 16384  <->   pi/2
 *       uint16_t 32768  <->   pi
 *       加满 65536      <->   2*pi（靠整数溢出自动回绕成 0，零开销）
 *   好处：不需要 if (Theta >= 2*PI) Theta -= 2*PI; 这种判断
 * ==========================================================================*/
#define ANGLE_FROM_RAD(r) ((uint16_t)((r) * (65536.0f / 6.2831853f)))
#define ANGLE_TO_RAD_U16(a) ((float)(a) * (6.2831853f / 65536.0f))







void Rev_Park_Transf(Foc_TypeDef *state);
void Rev_Clark_Transf(Foc_TypeDef *state);
void SVPWM_ZeroSqInject(Foc_TypeDef *state);
void Set_Udc_Tpwm_Param(Foc_TypeDef*state,float udc_temp,float Tpwm_temp);
void Foc_VoltageUpdate(Foc_TypeDef*state);
void Vf_SpeedControl_Param_Init(Vf_SpeedControl_TypeDef*state,float acc,float target_speed,float dead_zone,float ts);
void Vf_SpeedControl_Update(Vf_SpeedControl_TypeDef*state,float *Target_Theta_Acc);
//extern Foc_TypeDef Foc; 

#endif
