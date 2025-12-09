/*
 * es8388.c
 *
 *  Created on: Nov 26, 2025
 *      Author: Administrator
 */

#include "es8388.h"

static I2C_HandleTypeDef *es_i2c;

// 音量副本 (shadow) - ES8388 寄存器可能是只写的
// 存储 0-33 的值
static uint8_t headphone_vol = 16;  // 默认值 16 (约 50%)
static uint8_t speaker_vol = 0;

/**
 * @brief  写寄存器
 */
uint8_t ES8388_Write_Reg(uint8_t reg, uint8_t data)
{
    return HAL_I2C_Mem_Write(es_i2c, ES8388_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &data, 1, 10);
}

/**
 * @brief  读寄存器
 */
uint8_t ES8388_Read_Reg(uint8_t reg, uint8_t *data)
{
    return HAL_I2C_Mem_Read(es_i2c, ES8388_ADDR, reg, I2C_MEMADD_SIZE_8BIT, data, 1, 100);
}

/**
 * @brief  初始化 ES8388
 * @param  hi2c: I2C句柄 (例如 &hi2c1)
 * @return 0: 成功, 其他: 失败
 */
uint8_t ES8388_Init(I2C_HandleTypeDef *hi2c)
{
    es_i2c = hi2c;

    /* 完全按照正点原子的配置 */

    /* 1. 上电复位序列 (Atom 原版) */
    ES8388_Write_Reg(0x01, 0x58);
    ES8388_Write_Reg(0x01, 0x50);
    ES8388_Write_Reg(0x02, 0xF3);
    ES8388_Write_Reg(0x02, 0xF0);

    /* 2. 基本配置 */
    ES8388_Write_Reg(0x03, 0x09); /* 麦克风偏置电源关闭 */
    ES8388_Write_Reg(0x00, 0x06); /* 使能参考 500K驱动使能 */
    ES8388_Write_Reg(0x04, 0x00); /* DAC电源管理，不打开任何通道 */
    ES8388_Write_Reg(0x08, 0x00); /* MCLK不分频 */
    ES8388_Write_Reg(0x2B, 0x80); /* DAC控制 DACLRC与ADCLRC相同 */

    /* 3. ADC 配置 */
    ES8388_Write_Reg(0x09, 0x88); /* ADC L/R PGA增益配置为+24dB */
    ES8388_Write_Reg(0x0C, 0x4C); /* ADC 数据选择为left data = left ADC, right data = left ADC 音频数据为16bit */
    ES8388_Write_Reg(0x0D, 0x02); /* ADC配置 MCLK/采样率=256 */
    ES8388_Write_Reg(0x10, 0x00); /* ADC数字音量控制将信号衰减 L 设置为最小 */
    ES8388_Write_Reg(0x11, 0x00); /* ADC数字音量控制将信号衰减 R 设置为最小 */

    /* 4. DAC 配置 (关键！) */
    ES8388_Write_Reg(0x17, 0x18); /* DAC 音频数据为16bit I2S */
    ES8388_Write_Reg(0x18, 0x02); /* DAC 配置 MCLK/采样率=256 */
    ES8388_Write_Reg(0x1A, 0x00); /* DAC数字音量控制将信号衰减 L 设置为最小 */
    ES8388_Write_Reg(0x1B, 0x00); /* DAC数字音量控制将信号衰减 R 设置为最小 */

    /* 5. 混音器配置 (关键！与原来不同) */
    ES8388_Write_Reg(0x27, 0xB8); /* L混频器 - 使用 Atom 的值 0xB8 */
    ES8388_Write_Reg(0x2A, 0xB8); /* R混频器 - 使用 Atom 的值 0xB8 */

    /* 6. 启用 DAC (正点原子: es8388_adda_cfg(1, 0)) */
    ES8388_Write_Reg(0x02, 0x00); /* 0x00 启用 DAC，禁用 ADC */

    /* 7. 启用输出通道 (正点原子: es8388_output_cfg(1, 1)) */
    ES8388_Write_Reg(0x04, 0x3C); /* 启用所有输出通道 (LOUT1/ROUT1/LOUT2/ROUT2) */

    /* 8. 设置默认输出音量 */
    ES8388_Write_Reg(0x2E, 0x1E); /* LOUT1 音量 */
    ES8388_Write_Reg(0x2F, 0x1E); /* ROUT1 音量 */
    ES8388_Write_Reg(0x30, 0x1E); /* LOUT2 音量 */
    ES8388_Write_Reg(0x31, 0x1E); /* ROUT2 音量 */

    HAL_Delay(100);

    return 0;
}

/**
 * @brief  设置耳机音量
 * @param  volume: 0 ~ 33 (0 = 静音)
 */
void ES8388_SetVolume(uint8_t volume)
{
    if (volume > 33) volume = 33;

    // 更新副本
    headphone_vol = volume;

    if (volume == 0)
    {
        // 音量为 0 时启用 DAC 静音
        ES8388_Write_Reg(0x19, 0x04);  // DAC 软静音
    }
    else
    {
        // 取消静音，设置音量
        ES8388_Write_Reg(0x19, 0x00);    // 取消 DAC 静音
        ES8388_Write_Reg(0x2E, volume);  // LOUT1 (左耳机)
        ES8388_Write_Reg(0x2F, volume);  // ROUT1 (右耳机)
    }
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
 * @brief  设置喇叭音量
 * @param  volume: 0 ~ 33
 */
void ES8388_SetSpeakerVol(uint8_t volume)
{
    if (volume > 33) volume = 33;

    // 更新副本
    speaker_vol = volume;

    // 设置 LOUT2/ROUT2 音量
    ES8388_Write_Reg(ES8388_DACCONTROL26, volume);  // LOUT2 (Speaker L)
    ES8388_Write_Reg(ES8388_DACCONTROL27, volume);  // ROUT2 (Speaker R)
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
        ES8388_Write_Reg(0x31, 0x10	);  // ROUT2音量
    }
    else
    {
        // 禁用喇叭输出,只保留耳机
        ES8388_Write_Reg(0x04, 0x30);  // 只启用LOUT1/ROUT1
        ES8388_Write_Reg(0x27, 0x80);  // 只路由到LOUT1
        ES8388_Write_Reg(0x2A, 0x80);  // 只路由到ROUT1
        ES8388_Write_Reg(0x28, 0x00);  // 关闭LOUT2混音器
        ES8388_Write_Reg(0x29, 0x00);  // 关闭ROUT2混音器
        ES8388_Write_Reg(0x30, 0x00);  // LOUT2音量为0
        ES8388_Write_Reg(0x31, 0x00);  // ROUT2音量为0
    }
}

/**
 * @brief  读取耳机音量
 * @retval 音量值 0~33
 */
uint8_t ES8388_GetHeadphoneVolume(void)
{
    HAL_I2C_Mem_Read(es_i2c, ES8388_ADDR, 0x2E, I2C_MEMADD_SIZE_8BIT, &headphone_vol, 1, 10);
    return headphone_vol;
}

/**
 * @brief  读取喜叭音量
 * @retval 音量值 0~33
 */
uint8_t ES8388_GetSpeakerVolume(void)
{
    HAL_I2C_Mem_Read(es_i2c, ES8388_ADDR, 0x30, I2C_MEMADD_SIZE_8BIT, &speaker_vol, 1, 10);
    return speaker_vol;
}
