/*
 * es8388.c
 *
 *  Created on: Nov 26, 2025
 *      Author: Administrator
 */

#include "es8388.h"

static I2C_HandleTypeDef *es_i2c;

/**
 * @brief  写寄存器
 */
uint8_t ES8388_Write_Reg(uint8_t reg, uint8_t data)
{
    return HAL_I2C_Mem_Write(es_i2c, ES8388_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
}

/**
 * @brief  初始化 ES8388
 * @param  hi2c: I2C句柄 (例如 &hi2c1)
 * @return 0: 成功, 其他: 失败
 */
uint8_t ES8388_Init(I2C_HandleTypeDef *hi2c)
{
    es_i2c = hi2c;
    uint8_t res = 0;

    /* 1. 复位芯片 */
    ES8388_Write_Reg(0x00, 0x80);  // 软复位
    ES8388_Write_Reg(0x00, 0x00);  // 退出复位
    HAL_Delay(100);

    /* 2. 电源管理配置 */
    ES8388_Write_Reg(0x01, 0x58);  // 禁用所有电源，准备配置
    ES8388_Write_Reg(0x02, 0xF0);  // 关闭ADC电源，只使用DAC
    ES8388_Write_Reg(0x03, 0x00);  // 关闭所有输出，包括喇叭功放

    /* 3. 使能参考和DAC电源 */
    ES8388_Write_Reg(0x00, 0x06);  // 使能参考，500K驱动
    ES8388_Write_Reg(0x01, 0x50);  // 使能VMID，VREF
    ES8388_Write_Reg(0x03, 0x00);  // 关闭所有ADC相关电源
    ES8388_Write_Reg(0x04,
                     0x3C);  // 使能所有输出 (LOUT1/ROUT1 + LOUT2/ROUT2)

    /* 3.1 额外确保喇叭功放关闭 */
    ES8388_Write_Reg(0x38, 0x00);  // 关闭左喇叭功放
    ES8388_Write_Reg(0x39, 0x00);  // 关闭右喇叭功放

    /* 4. 时钟配置 */
    ES8388_Write_Reg(0x08, 0x00);  // MCLK不分频，主模式

    /* 5. DAC接口配置 (I2S格式，16位，44.1kHz) */
    ES8388_Write_Reg(0x17, 0x18);  // DAC I2S格式，16位
    ES8388_Write_Reg(0x18, 0x02);  // MCLK/LRCK = 256 (44.1kHz)
    ES8388_Write_Reg(0x2B, 0x80);  // DACLRC与ADCLRC相同

    /* 6. DAC数字音量 (0x00 = 0dB，不衰减) */
    ES8388_Write_Reg(0x1A, 0x00);  // Left DAC数字音量 0dB
    ES8388_Write_Reg(0x1B, 0x00);  // Right DAC数字音量 0dB

    /* 7. DAC控制 */
    ES8388_Write_Reg(0x19, 0x00);  // 取消静音，正常工作

    /* 8. 混音器配置 - 参考正点原子驱动 */
    ES8388_Write_Reg(0x26, 0x00);  // Left DAC to Left Mixer
    ES8388_Write_Reg(0x27, 0xB8);  // L混频器 (正点原子配置)
    ES8388_Write_Reg(0x28, 0x00);  // 关闭LOUT2混音器
    ES8388_Write_Reg(0x29, 0x00);  // 关闭ROUT2混音器
    ES8388_Write_Reg(0x2A, 0xB8);  // R混频器 (正点原子配置)

    /* 9. 输出音量设置 - 初始为0，由应用层设置 */
    ES8388_Write_Reg(0x2E, 0x00);  // LOUT1初始为0
    ES8388_Write_Reg(0x2F, 0x00);  // ROUT1初始为0
    ES8388_Write_Reg(0x30, 0x00);  // LOUT2初始为0
    ES8388_Write_Reg(0x31, 0x00);  // ROUT2初始为0

    /* 10. 启动DAC */
    ES8388_Write_Reg(0x02, 0x00);  // 启动DAC电源

    HAL_Delay(50);  // 等待稳定

    return res;
}

/**
 * @brief  设置音量
 * @param  volume: 0 ~ 33
 */
void ES8388_SetVolume(uint8_t volume)
{
    if (volume > 33) volume = 33;

    // ES8388 的 LOUT1/ROUT1 音量寄存器是 0x26 和 0x27
    // 范围通常是 0x00 (最大衰减) 到 0x21 (+3dB)
    // 我们可以直接映射
    ES8388_Write_Reg(ES8388_DACCONTROL24, volume);  // LOUT1 (Headphone L)
    ES8388_Write_Reg(ES8388_DACCONTROL25, volume);  // ROUT1 (Headphone R)

    // 如果使用的是 LOUT2/ROUT2，则操作 0x2E, 0x2F
    // ES8388_Write_Reg(ES8388_DACCONTROL24, volume);
    // ES8388_Write_Reg(ES8388_DACCONTROL25, volume);
}

/**
 * @brief  静音控制
 * @param  mute: 1=静音, 0=正常
 */
void ES8388_SetMute(uint8_t mute)
{
    uint8_t reg = 0x04;  // 默认值
    if (mute)
    {
        reg |= 0x04;  // Set mute bit inside Reg 0x19 if needed, or simply zero volume
        // ES8388 0x19 bit 2 is DAC Mute
        ES8388_Write_Reg(ES8388_DACCONTROL3, 0x04 | 0x04);
    }
    else
    {
        ES8388_Write_Reg(ES8388_DACCONTROL3, 0x04);
    }
}

/**
 * @brief  喇叭输出控制
 * @param  enable: 1=启用喇叭, 0=禁用喇叭
 */
void ES8388_SetSpeakerEnable(uint8_t enable)
{
    if (enable)
    {
        // 启用喇叭输出 (LOUT2/ROUT2)
        ES8388_Write_Reg(0x04, 0x3C);  // 启用所有输出
        ES8388_Write_Reg(0x27, 0x90);  // 路由到LOUT1和LOUT2
        ES8388_Write_Reg(0x2A, 0x90);  // 路由到ROUT1和ROUT2
        ES8388_Write_Reg(0x28, 0x38);  // LOUT2混音器
        ES8388_Write_Reg(0x29, 0x38);  // ROUT2混音器
        ES8388_Write_Reg(0x30, 0x10);  // LOUT2音量
        ES8388_Write_Reg(0x31, 0x10);  // ROUT2音量
    }
    else
    {
        // 禁用喇叭输出，只保留耳机
        ES8388_Write_Reg(0x04, 0x30);  // 只启用LOUT1/ROUT1
        ES8388_Write_Reg(0x27, 0x80);  // 只路由到LOUT1
        ES8388_Write_Reg(0x2A, 0x80);  // 只路由到ROUT1
        ES8388_Write_Reg(0x28, 0x00);  // 关闭LOUT2混音器
        ES8388_Write_Reg(0x29, 0x00);  // 关闭ROUT2混音器
        ES8388_Write_Reg(0x30, 0x00);  // LOUT2音量为0
        ES8388_Write_Reg(0x31, 0x00);  // ROUT2音量为0
    }
}
