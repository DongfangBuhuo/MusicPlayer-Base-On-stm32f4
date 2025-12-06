/*
 * es8388.h
 *
 *  Created on: Nov 26, 2025
 *      Author: Administrator
 */

#ifndef INC_ES8388_H_
#define INC_ES8388_H_

#include "i2c.h"
#include "main.h" // 确保包含 STM32 的 HAL 库定义
#include <stdint.h>

// ES8388 I2C 地址 (7-bit address 0x10, 左移一位为 0x20)
// 注意：如果你的硬件地址不同，请修改这里
#define ES8388_ADDR 0x20

// 常用寄存器定义
#define ES8388_CONTROL1 0x00
#define ES8388_CONTROL2 0x01
#define ES8388_CHIPPOWER 0x02
#define ES8388_ADCPOWER 0x03
#define ES8388_DACPOWER 0x04
#define ES8388_CHIPLOPOW1 0x05
#define ES8388_CHIPLOPOW2 0x06
#define ES8388_ANAVOLMANAG 0x07
#define ES8388_MASTERMODE 0x08
#define ES8388_ADCCONTROL1 0x09
#define ES8388_DACCONTROL1 0x17
#define ES8388_DACCONTROL2 0x18
#define ES8388_DACCONTROL3 0x19
#define ES8388_DACCONTROL16 0x26 // Mixer Control L
#define ES8388_DACCONTROL17 0x27 // Mixer Control R
#define ES8388_DACCONTROL24 0x2E // LOUT1 Volume (Headphone L)
#define ES8388_DACCONTROL25 0x2F // ROUT1 Volume (Headphone R)
#define ES8388_DACCONTROL26 0x30 // LOUT2 Volume (Speaker L)
#define ES8388_DACCONTROL27 0x31 // ROUT2 Volume (Speaker R)

/* 函数声明 */
uint8_t ES8388_Init(I2C_HandleTypeDef *hi2c);
void ES8388_SetVolume(uint8_t volume);        // 0~33 (对应 -30dB 到 +3dB)
void ES8388_SetMute(uint8_t mute);            // 1: Mute, 0: Unmute
void ES8388_SetSpeakerEnable(uint8_t enable); // 1: 启用喇叭, 0: 禁用喇叭
uint8_t ES8388_Write_Reg(uint8_t reg, uint8_t data);
uint8_t ES8388_Read_Reg(uint8_t reg, uint8_t *data);  // 读寄存器

#endif /* INC_ES8388_H_ */
