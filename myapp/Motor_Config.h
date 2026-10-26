#ifndef __MOTOR_CONFIG_H__
#define __MOTOR_CONFIG_H__

#include "bsp_system.h"


/* =========== 系统时序常量 ============ */
#define FOC_HZ            10204                      /* FOC中断频率10KHZ */  
#define FOC_TS            (1/(float) FOC_HZ)
	
/* =========== 数学常量 ============ */
#define ANGLE_2PI         6.2831853f                 /* 2*pi，仅用于波形显示换算 */



/* =========== SVPWM常量 ============ */
#define UDC               12.0f
#define TPWM              49.0f

/* =========== 编码器常量 ============ */
#define ENCODER_LINE      4096                       /* AS5600 是 12 位，一圈 4096 个计数 */
#define Pn                7                          /* 电机极对数 —— 必须按你的电机改！（G431 那台是 10）*/

/* =========== 角度预测开关 ============ */
/* 1 = 用实测转速把角度推到当前时刻（默认）
 * 0 = 关掉预测，直接用 1kHz 的采样值 —— 用来做 A/B 对比，验证预测的效果 */
#define ENC_PRED_ENABLE   1

/* =========== 微分跟踪器（TD）============ */
/* 这里是【Vq 命令的软启动/平滑】，不是速度滤波器：
 * 上升时间(10~90%) ≈ 3.36/ωn —— ωn=20 约 168ms，ωn=100 约 34ms（太快）
 * TD 的极点只有 ωn 这么低，10kHz 跑和 1kHz 跑【响应完全一样】，所以分频到 1kHz ✓ */
#define TD_DIV            10                    /* 10.204kHz ÷ 10 ≈ 1.02kHz */
#define TD_TS             (FOC_TS * TD_DIV)     /* 实际调用周期 ≈ 0.98ms（必须用它，不能用 FOC_TS ✗）*/
#define TD_WN             20.0f                 /* 收敛因子 rad/s（20 ≈ 3Hz，上升约 168ms）✓ 现场可在 Watch 里改 */





#endif
