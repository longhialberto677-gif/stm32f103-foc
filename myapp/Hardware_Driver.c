#include "Hardware_Driver.h"



void Motor_EnablePWM()
{
  HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_3);
  HAL_TIMEx_PWMN_Start(&htim1,TIM_CHANNEL_1);
  HAL_TIMEx_PWMN_Start(&htim1,TIM_CHANNEL_2);
  HAL_TIMEx_PWMN_Start(&htim1,TIM_CHANNEL_3);
}

void Motor_DisablePWM()
{
	HAL_TIM_PWM_Stop(&htim1,TIM_CHANNEL_1);
	HAL_TIM_PWM_Stop(&htim1,TIM_CHANNEL_2);
	HAL_TIM_PWM_Stop(&htim1,TIM_CHANNEL_3);
	HAL_TIMEx_PWMN_Stop(&htim1,TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Stop(&htim1,TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Stop(&htim1,TIM_CHANNEL_3);
}

void Motor_Hardware_Init()
{
	HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_4);
	/* 使能定时器一通道四捕获中断 */
	__HAL_TIM_ENABLE_IT(&htim1,TIM_IT_CC4);
	Motor_EnablePWM();
}

void Motor_SetPWM(uint16_t Tcmp1,uint16_t Tcmp2,uint16_t Tcmp3)
{
	__HAL_TIM_SetCompare(&htim1,TIM_CHANNEL_1,Tcmp1);
	__HAL_TIM_SetCompare(&htim1,TIM_CHANNEL_2,Tcmp2);
	__HAL_TIM_SetCompare(&htim1,TIM_CHANNEL_3,Tcmp3);
}

/* ============================================================================
 * AS5600 磁编码器读取（I2C1，重映射到 PB8=SCL / PB9=SDA）
 * ----------------------------------------------------------------------------
 * 分工与 G431 工程一致：这里只负责"把原始值读出来"（对应那边的 MT6701_ReadData），
 * 角度换算在 Encoder.c 的 Encoder_UpData 里做。
 *
 * 一次 2 字节读在 400kHz 下约 120us 且全程阻塞，
 * 只能放主循环调用，绝不能进 10kHz 的 FOC 中断。
 * ==========================================================================*/
#define AS5600_I2C_ADDR    (0x36 << 1)   /* HAL 接口要 8 位形式 */
#define AS5600_REG_STATUS  0x0B          /* bit5=MD 检测到磁铁, bit4=ML 太弱, bit3=MH 太强 */
#define AS5600_REG_RAW     0x0C          /* 原始角度，低 12 位有效 */
#define AS5600_REG_AGC     0x1A          /* 自动增益 */
#define AS5600_TIMEOUT_MS  5
#define AS5600_RETRY       3             /* 读失败重试次数 */

#define AS5600_STATUS_MD   (1U << 5)
#define AS5600_STATUS_ML   (1U << 4)
#define AS5600_STATUS_MH   (1U << 3)

/**
 * @brief 读取AS5600原始角度
 * @param 无
 * @return 12 位角度 0~4095；读失败返回 0xFFFF（超出范围，一眼可辨）
 */
uint16_t AS5600_ReadData(void)
{
  uint8_t rx[2] = {0};
  uint8_t try = 0;

  for(try = 0; try < AS5600_RETRY; try++)
  {
    if(HAL_I2C_Mem_Read(&hi2c1,AS5600_I2C_ADDR,AS5600_REG_RAW,I2C_MEMADD_SIZE_8BIT,rx,2,AS5600_TIMEOUT_MS) == HAL_OK)
    {
      /* AS5600 的 RAW ANGLE 寄存器 bit15~12 保留为 0，所以高字节的高 4 位必须是 0。
         读到 0xFF 这种说明这一帧是总线浮空的全 1，不是真值 —— 当失败处理。
         注意：HAL 只检查协议层（地址有没有被应答），它发现不了"数据全 1" */
      if((rx[0] & 0xF0) == 0)
      {
        return (uint16_t)(((rx[0] & 0x0F) << 8) | rx[1]);
      }
    }
  }

  return 0xFFFF;
}

/**
 * @brief I2C 总线恢复：把被从机拉死的总线救回来
 * @note  标准套路：把 SCL 切成普通 GPIO 手动打 9 个脉冲，让"正发到一半"的从机
 *        把 8 位数据发完并释放 SDA，再补一个 STOP，最后重新初始化 I2C。
 *        只在总线卡住（ErrorCode 里出现 0x200 WRONG_START / 总线一直 BUSY）时才有意义；
 *        如果是没上拉、短路、器件没供电，它救不回来。
 */
void I2C_BusRecover(void)
{
  GPIO_InitTypeDef g = {0};
  uint8_t i = 0;
  volatile uint32_t d = 0;

  HAL_I2C_DeInit(&hi2c1);

  /* 先都放开成输入（带内部弱上拉，只为了能看电平）*/
  g.Pin   = GPIO_PIN_8 | GPIO_PIN_9;
  g.Mode  = GPIO_MODE_INPUT;
  g.Pull  = GPIO_PULLUP;
  g.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &g);

  /* SCL(PB8) 切成开漏输出，打 9 个脉冲 */
  g.Pin  = GPIO_PIN_8;
  g.Mode = GPIO_MODE_OUTPUT_OD;
  HAL_GPIO_Init(GPIOB, &g);

  for(i = 0; i < 9; i++)
  {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
    for(d = 60; d > 0; d--) { }
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
    for(d = 60; d > 0; d--) { }
  }

  /* 补一个 STOP：SCL 保持高，SDA 由低变高 */
  g.Pin  = GPIO_PIN_9;
  g.Mode = GPIO_MODE_OUTPUT_OD;
  HAL_GPIO_Init(GPIOB, &g);

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
  for(d = 60; d > 0; d--) { }
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
  for(d = 60; d > 0; d--) { }
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);
  for(d = 60; d > 0; d--) { }

  /* 恢复成 I2C 引脚并重新初始化 */
  MX_I2C1_Init();
}

/**
 * @brief 磁铁安装自检（第一步验证用；G431 那套没有这个函数，不需要可直接删）
 * @param pAgc：可选，带回自动增益（传 0 表示不需要）
 * @return 0=正常 1=没检测到磁铁 2=太弱 3=太强 0xFF=I2C失败
 * @note  AGC 在中间值（约 128）说明间隙合适
 */
uint8_t AS5600_CheckMagnet(uint8_t *pAgc)
{
  uint8_t st = 0;
  uint8_t agc = 0;

  if(HAL_I2C_Mem_Read(&hi2c1,AS5600_I2C_ADDR,AS5600_REG_STATUS,I2C_MEMADD_SIZE_8BIT,&st,1,AS5600_TIMEOUT_MS) != HAL_OK)
  {
    return 0xFF;
  }
  if(pAgc != 0)
  {
    if(HAL_I2C_Mem_Read(&hi2c1,AS5600_I2C_ADDR,AS5600_REG_AGC,I2C_MEMADD_SIZE_8BIT,&agc,1,AS5600_TIMEOUT_MS) == HAL_OK)
    {
      *pAgc = agc;
    }
  }
  if((st & AS5600_STATUS_MD) == 0U)  return 1U;   /* 没检测到磁铁 */
  if((st & AS5600_STATUS_ML) != 0U)  return 2U;   /* 磁铁太弱 */
  if((st & AS5600_STATUS_MH) != 0U)  return 3U;   /* 磁铁太强 */
  return 0U;                                      /* 正常 */
}









