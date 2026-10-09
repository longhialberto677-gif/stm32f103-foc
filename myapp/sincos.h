#ifndef __SINCOS_H__
#define __SINCOS_H__

#include "bsp_system.h"

/* 正弦表大小：1024 点，必须是 2 的幂 */
#define SIN_TAB_BITS      10
#define SIN_TAB_SIZE      (1 << SIN_TAB_BITS)        /* 1024 索引数 */
#define SIN_TAB_MASK      (SIN_TAB_SIZE - 1)         /* 1023 表索引掩码 */

/* 小数部分掩码：高 10 位当索引，剩下低 6 位当小数
 * 一个索引步长 = 65536/1024 = 64，所以小数范围 0~63
 * 不能错用 SIN_TAB_SIZE-1 (=1023)，否则插值变 16 倍外推 */
#define SIN_TAB_FRAC_BITS (16 - SIN_TAB_BITS)              /* 6 */
#define SIN_TAB_FRAC_MASK ((1 << SIN_TAB_FRAC_BITS) - 1)   /* 63 */


extern const float sin_tab[SIN_TAB_SIZE];

void Foc_SinCos_U16(uint16_t angle, float *ps, float *pc);
void Foc_Sin_U16(uint16_t angle);           /* 如果还留着单点版就加上 */

#endif