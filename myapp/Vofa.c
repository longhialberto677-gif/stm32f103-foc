#include "Vofa.h"

/* Vofa发送结构体实例 */
VofaSend_TypeDef VofaSend;

/**
 * @brief Vofa+发送函数（非阻塞：只填数据 + 启动DMA发送，不等待发送完成）
 * @param 无
 * @return 无
 */
void Vofa_Send_Task(void)
{
    /* 填入需要发送的数据 */
	VofaSend.Send_Data_Array[0] = MotorSystem.Foc.Vq;
    VofaSend.Send_Data_Array[1] = MotorSystem.Td_Track_Uq.Input;
    VofaSend.Send_Data_Array[2] = MotorSystem.Vf.Speed;
    VofaSend.Send_Data_Array[3] = ANGLE_TO_RAD_U16(MotorSystem.Vf.Theta_U16);    /* V/F角度（弧度，只为显示）*/
    VofaSend.Send_Data_Array[4] = MotorSystem.Foc.Vv;
	VofaSend.Send_Data_Array[5] = MotorSystem.Foc.Vw;
	VofaSend.Send_Data_Array[6] = MotorSystem.Encoder.Theta_U16;
    VofaSend.Send_Data_Array[7] = MotorSystem.Encoder.Theta_e_U16;
	VofaSend.Send_Data_Array[8] = MotorSystem.Encoder.Pulse_Data;
	/* ---- 角度预测诊断（[9][10]）---- */
	/* [9] 解析滞后（电角度计数）= 速度 × 一个采样周期(1ms)
	 *     = "如果不做预测，这一拍的角度会旧多少" —— 稳定、不抖、随转速线性增长
	 *     128rpm→约 984（5.4°），1200rpm→约 9400（51.6°），3000rpm→约 23000（126°）
	 *     （比看瞬时补偿量直观：瞬时值取决于采样时刻，总在主循环刚读完时偏小）*/
	VofaSend.Send_Data_Array[9] = (float)((MotorSystem.Encoder.Speed_e_Q12 * 72000) >> 12);
	/* [10] 实测电角速度（rad/s）：Q12 速度 × (72MHz/4096/10430.378) = ×1.68529
	 *      顺便能看速度估计的质量（应该跟实际转速吻合、且不抖）*/
	VofaSend.Send_Data_Array[10] = (float)MotorSystem.Encoder.Speed_e_Q12 * 1.68529f;
    Vofa_Data_Process();
}

/**
 * @brief Vofa+数据处理函数（组装一帧并启动DMA发送，立即返回）
 * @param 无
 * @return 无
 */
void Vofa_Data_Process(void)
{
    /* 上一帧还在DMA搬运中就直接丢弃本帧，避免改写正在发送的缓冲区 */
    if (huart1.gState != HAL_UART_STATE_READY)
    {
        return;
    }

    /* 通过拷贝将浮点数数据转换成单字节数据（小端 IEEE-754 原样搬运） */
    memcpy(VofaSend.Byte_Data_Array, VofaSend.Send_Data_Array, data_num * 4);

    /* 添加通信协议的帧尾：0x7F800000，即 +INF，小端字节序为 00 00 80 7F */
    VofaSend.Byte_Data_Array[data_len - 4] = 0x00;
    VofaSend.Byte_Data_Array[data_len - 3] = 0x00;
    VofaSend.Byte_Data_Array[data_len - 2] = 0x80;
    VofaSend.Byte_Data_Array[data_len - 1] = 0x7f;

    /* 非阻塞发送：24字节交给 DMA1_Channel4 搬运，CPU 立刻返回 */
    HAL_UART_Transmit_DMA(&huart1, VofaSend.Byte_Data_Array, data_len);
}