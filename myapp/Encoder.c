#include "Encoder.h"

/**
 * @brief 编码器参数初始化
 * @param line：编码器线数
 * @param dir：编码器方向 （0同向，1反向）
 * @param MotorPn：电机极对数
 * @param Phase：零点偏差
 * @return 无
 */
void Encoder_Param_Init(Encoder_Typedef *state,uint32_t line,uint8_t dir,uint8_t MotorPn,uint32_t Phase)
{
	state->Line = line;
	state->Dir = dir;
	state->MotorPn = MotorPn;
	state->Phase = Phase;
}

/**
 * @brief 编码器角度更新函数
 * @param Data：AS5600 读到的原始角度（12 位，0~4095）
 * @return 无
 * @note  与 G431 的差别只在实现：那边是
 *            Theta   = (float)Temp / Line * ANGLE_2PI;
 *            Theta_e = fmod(Theta * MotorPn, ANGLE_2PI);
 *        在 F103 上没有 FPU，一次浮点除法约 307 cycle、一次双精度 fmod 约 612 cycle；
 *        这里换成整数：AS5600 是 12 位，4096 x 16 = 65536，
 *        机械角度就是"除以 Line 再左移 16 位"，电角度就是一次整数乘法，
 *        uint16 溢出天然完成回绕，不需要任何取模或限幅。
 */
void Encoder_UpData(Encoder_Typedef *state,uint32_t Data)
{
	/* 判断编码器方向 */
	if(state->Dir == 0)
	{
		state->Pulse_Data = Data;
	}
	else
	{
		state->Pulse_Data = state->Line - Data;
	}

	/* 减去零点偏差并回绕到一圈之内（先加 Line 防止下溢）*/
	uint32_t Temp = (state->Pulse_Data + state->Line - state->Phase) % state->Line;

	/* 计算电机 机械角度：Temp/Line 圈 映射成 uint16 一整圈
	 * 写成通用式便于以后换编码器；Line=4096 时等价于 Temp<<4 */
	state->Theta_U16 = (uint16_t)(((uint32_t)Temp << 16) / state->Line);

	/* 计算电机 电角度：机械角度 x 极对数，uint16 溢出即自动回绕，零开销 */
	state->Theta_e_U16 = (uint16_t)((uint32_t)state->Theta_U16 * state->MotorPn);
}
