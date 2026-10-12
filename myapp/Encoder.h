#ifndef __ENCODER_H__
#define __ENCODER_H__

#include "stdint.h"
/**
 * @brief 编码器结构体
 *
 */
typedef struct
{
	/* 编码器参数 */
	uint32_t Line;         /* 编码器线数（AS5600 是 12 位，一圈 4096）*/
	uint8_t  MotorPn;      /* 极对数 */
	uint8_t  Dir;          /* 编码器方向：0 同向，1 反向 */

	/* 编码器数据与误差 */
	uint32_t Pulse_Data;   /* 编码器原始数据 */
	uint32_t Phase;        /* 零点偏差（第二步标定得到）*/

	/* 角度计算
	 * 沿用工程里 "uint16 = 一整圈(65536)" 的约定，替代 G431 那边的 float Theta/Theta_e：
	 * F103 没有 FPU，那边用浮点除法和 fmod 算一次约 1100 cycle，这里全程整数（约 30 cycle）*/
	uint16_t Theta_U16;    /* 机械角度 */
	uint16_t Theta_e_U16;  /* 电角度 = 机械角度 x 极对数 */

}Encoder_Typedef;

#include "bsp_system.h"

void Encoder_Param_Init(Encoder_Typedef *state,uint32_t line,uint8_t dir,uint8_t MotorPn,uint32_t Phase);
void Encoder_UpData(Encoder_Typedef *state,uint32_t Data);

#endif
