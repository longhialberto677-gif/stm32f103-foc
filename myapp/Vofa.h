#ifndef __VOFA_H__
#define __VOFA_H__


#include "bsp_system.h"


/*======================= 宏定义 =======================*/
#define data_num 9                                   /* 定义发送数量 */
#define data_len (data_num * 4 + 4)                  /* 定义发送长度：帧头(数据) + 帧尾(4字节) */

/*======================= 结构体 =======================*/
typedef struct
{
    float Send_Data_Array[data_num];                 /* 实际发送的数据，小端 IEEE-754 共占 data_num*4 字节 */
    uint8_t Byte_Data_Array[data_len];               /* 整帧缓冲区：0 ~ data_num*4-1 为数据区，最后 4 字节为帧尾 */
} VofaSend_TypeDef;

/*======================= 函数声明 =======================*/
void Vofa_Send_Task(void);
void Vofa_Data_Process(void);



#endif