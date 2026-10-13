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

	/* ==================== 角度预测（时间戳 + 实测转速）====================
	 * 问题：编码器 1kHz 采样，而 FOC 是 10kHz；两次采样之间角度是"冻住"的，
	 *       电机却一直在转 —— 1200rpm 时那 1ms 就对应 46° 电角度。
	 * 做法：每次读成功时记下【电角度 + DWT 时刻 + 实测转速】，
	 *       之后 FOC 每一拍自己把角度推到"这一拍电压实际生效的时刻"。
	 * 注意：必须用实测转速，不能用目标速度（一加载/一堵转就全错）。 */
	int32_t  Elec_Gain;       /* 机械计数→电角度计数 的增益 =(65536/Line)xPn（4096,7 → 112）*/
	uint32_t Theta_Mech_Last; /* 上次成功采样的机械原始值（差分用；只有主循环会读）*/
	uint32_t Time_Last;       /* 上次成功采样的 DWT 时刻 */
	uint16_t Theta_e_Last;    /* 上次成功采样的电角度（预测起点）*/
	int32_t  Speed_e_Q12;     /* 实测电角速度：每 1 个 DWT cycle 前进多少 Q12 电角度计数 */
	uint8_t  Pred_Ok;         /* 1 = 已有有效样本，可以预测 */

}Encoder_Typedef;

#include "bsp_system.h"

void Encoder_Param_Init(Encoder_Typedef *state,uint32_t line,uint8_t dir,uint8_t MotorPn,uint32_t Phase);
void Encoder_UpData(Encoder_Typedef *state,uint32_t Data);
/* 读成功后调用（主循环）：打时间戳、解算实测转速、原子发布给中断用 */
void Encoder_Speed_Update(Encoder_Typedef *state,uint32_t now_cycles);
/* FOC 中断里调用：返回预测到当前时刻的电角度（uint16 一整圈）*/
uint16_t Encoder_Get_Predicted(Encoder_Typedef *state,uint32_t now_cycles);

#endif
