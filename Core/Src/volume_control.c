/**
 * @file volume_control.c
 * @brief ES8388音量控制的补充实现
 */

#include "es8388.h"

/**
 * @brief  智能设置喇叭音量(带完全静音控制)
 * @param  volume: 音量等级 (0-33)
 *         0 = 完全静音(禁用输出通道)
 *         1-33 = 正常音量
 */
void ES8388_SetSpeakerVolume_Smart(uint8_t volume)
{
    if (volume > 33) volume = 33;

    if (volume == 0)
    {
        // === 完全静音喇叭 ===

        // 1. DAC软静音 (最关键,从源头切断)
        ES8388_Write_Reg(0x19, 0x04);  // Bit 2: DAC Mute

        // 2. 禁用喇叭输出通道
        ES8388_Write_Reg(0x04, 0x30);  // 只保留LOUT1/ROUT1(耳机), 禁用LOUT2/ROUT2(喇叭)

        // 3. 关闭喇叭混音器
        ES8388_Write_Reg(0x28, 0x00);  // LOUT2混音器关闭
        ES8388_Write_Reg(0x29, 0x00);  // ROUT2混音器关闭

        // 4. 音量设为0
        ES8388_Write_Reg(0x30, 0x00);  // LOUT2音量
        ES8388_Write_Reg(0x31, 0x00);  // ROUT2音量
    }
    else
    {
        // === 启用喇叭 ===

        // 1. 取消DAC静音
        ES8388_Write_Reg(0x19, 0x00);  // DAC Unmute

        // 2. 启用喇叭输出通道
        ES8388_Write_Reg(0x04, 0x3C);  // 启用所有输出

        // 3. 启用喇叭混音器
        ES8388_Write_Reg(0x28, 0x38);  // LOUT2混音器
        ES8388_Write_Reg(0x29, 0x38);  // ROUT2混音器

        // 4. 设置音量
        ES8388_Write_Reg(0x30, volume);  // LOUT2音量
        ES8388_Write_Reg(0x31, volume);  // ROUT2音量
    }
}
