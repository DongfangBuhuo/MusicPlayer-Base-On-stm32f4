#include "music_player.h"
#include "es8388.h"
#include "fatfs.h"
#include "i2c.h"
#include "i2s.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

#include "cmsis_os.h"

/* Private typedef -----------------------------------------------------------*/
typedef struct
{
    char ChunkID[4];  // "RIFF"
    uint32_t ChunkSize;
    char Format[4];       // "WAVE"
    char Subchunk1ID[4];  // "fmt "
    uint32_t Subchunk1Size;
    uint16_t AudioFormat;
    uint16_t NumChannels;
    uint32_t SampleRate;
    uint32_t ByteRate;
    uint16_t BlockAlign;
    uint16_t BitsPerSample;
    char Subchunk2ID[4];  // "data"
    uint32_t Subchunk2Size;
} WAV_Header_TypeDef;

/* Private define ------------------------------------------------------------*/
#define AUDIO_BUFFER_SIZE 4096  // 4KB 缓存

/* Private variables ---------------------------------------------------------*/
static uint16_t audio_buffer[AUDIO_BUFFER_SIZE];
static FATFS fs;
static FIL wavFile;
static WAV_Header_TypeDef wavHeader;
uint8_t isPlaying;
osSemaphoreId_t audio_semHandle;
volatile uint8_t audio_state = 0;

/* External variables --------------------------------------------------------*/
extern I2S_HandleTypeDef hi2s2;
extern I2C_HandleTypeDef hi2c1;
//
void music_player_init(void)
{
    FRESULT res;
    isPlaying = 0;

    // Create binary semaphore
    const osSemaphoreAttr_t audio_sem_attributes = {.name = "audio_sem"};
    audio_semHandle = osSemaphoreNew(1, 0, &audio_sem_attributes);

    // 初始化ES8388
    if (ES8388_Init(&hi2c1) != 0)
    {
        // LED常亮表示ES8388初始化失败
        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);
        while (1);
    }

    // 明确开启喇叭输出
    ES8388_SetSpeakerEnable(0);

    // LED闪1次表示ES8388初始化成功
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);
    HAL_Delay(300);
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_RESET);
    HAL_Delay(300);

    // 挂载SD卡
    res = f_mount(&fs, "0:/", 1);
    if (res != FR_OK)
    {
        // LED慢闪表示SD卡挂载失败
        while (1)
        {
            HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);
            HAL_Delay(500);
        }
    }

    // LED闪2次表示SD卡挂载成功
    for (int i = 0; i < 2; i++)
    {
        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);
        HAL_Delay(200);
        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_RESET);
        HAL_Delay(200);
    }
}

void music_player_play(const char *filename)
{
    FRESULT res;
    UINT bytesRead;

    char music_full_name[40] = "0:/music/";
    // 打开WAV文件
    strcat(music_full_name, filename);
    res = f_open(&wavFile, music_full_name, FA_READ);
    if (res != FR_OK)
    {
        // LED快速闪烁表示文件打开失败
        while (1)
        {
            HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);
            HAL_Delay(100);
        }
    }

    // 读取WAV头
    res = f_read(&wavFile, &wavHeader, sizeof(wavHeader), &bytesRead);
    if (res != FR_OK || bytesRead != sizeof(wavHeader))
    {
        // 读取头失败
        f_close(&wavFile);
        return;
    }

    // 检查是否为WAV文件
    if (strncmp(wavHeader.ChunkID, "RIFF", 4) != 0 || strncmp(wavHeader.Format, "WAVE", 4) != 0)
    {
        // 不是WAV文件
        f_close(&wavFile);
        return;
    }

    // 配置I2S采样率
    hi2s2.Init.AudioFreq = wavHeader.SampleRate;
    if (HAL_I2S_Init(&hi2s2) != HAL_OK)
    {
        // I2S初始化失败
        f_close(&wavFile);
        return;
    }

    // 填充初始缓存
    f_read(&wavFile, audio_buffer, AUDIO_BUFFER_SIZE * 2, &bytesRead);

    // 启动I2S DMA传输
    HAL_I2S_Transmit_DMA(&hi2s2, audio_buffer, AUDIO_BUFFER_SIZE);

    isPlaying = 1;

    // 播放循环
    while (isPlaying)
    {
        // 等待半传输或传输完成中断信号
        if (osSemaphoreAcquire(audio_semHandle, osWaitForever) == osOK)
        {
            if (audio_state == 1)  // 半传输完成，填充前半部分
            {
                f_read(&wavFile, audio_buffer, AUDIO_BUFFER_SIZE, &bytesRead);
            }
            else if (audio_state == 2)  // 传输完成，填充后半部分
            {
                f_read(&wavFile, &audio_buffer[AUDIO_BUFFER_SIZE / 2], AUDIO_BUFFER_SIZE, &bytesRead);
            }

            if (bytesRead < AUDIO_BUFFER_SIZE)
            {
                // 文件结束，重新播放或停止
                // f_lseek(&wavFile, sizeof(wavHeader)); // 循环播放
                // 或者停止
                isPlaying = 0;
                break;
            }
        }
    }

    // 停止传输
    HAL_I2S_DMAStop(&hi2s2);
    f_close(&wavFile);
}


void music_player_set_headphone_volume(uint8_t volume)
{
    if (volume > 33) volume = 33;
    ES8388_Write_Reg(ES8388_DACCONTROL24, volume);  // LOUT1 (Headphone L)
    ES8388_Write_Reg(ES8388_DACCONTROL25, volume);  // ROUT1 (Headphone R)
}

void music_player_set_speaker_volume(uint8_t volume)
{
    if (volume > 33) volume = 33;
    ES8388_Write_Reg(ES8388_DACCONTROL26, volume);  // LOUT2 (Speaker L)
    ES8388_Write_Reg(ES8388_DACCONTROL27, volume);  // ROUT2 (Speaker R)
}

void music_player_task(void)
{
    UINT bytesRead;

    // 等待信号量初始化，防止高优先级任务饿死低优先级初始化任务
    while (audio_semHandle == NULL)
    {
        osDelay(10);
    }

    for (;;)
    {
        if (osSemaphoreAcquire(audio_semHandle, osWaitForever) == osOK)
        {
            if (audio_state == 1)
            {
                // Fill first half
                f_read(&wavFile, &audio_buffer[0], AUDIO_BUFFER_SIZE, &bytesRead);
                if (bytesRead < AUDIO_BUFFER_SIZE)
                {
                    memset(&audio_buffer[0], 0, AUDIO_BUFFER_SIZE - bytesRead);
                    if (bytesRead == 0) f_lseek(&wavFile, sizeof(WAV_Header_TypeDef));
                }
            }
            else if (audio_state == 2)
            {
                // Fill second half
                f_read(&wavFile, &audio_buffer[AUDIO_BUFFER_SIZE / 2], AUDIO_BUFFER_SIZE, &bytesRead);
                if (bytesRead < AUDIO_BUFFER_SIZE)
                {
                    memset(&audio_buffer[AUDIO_BUFFER_SIZE / 2], 0, AUDIO_BUFFER_SIZE - bytesRead);
                    if (bytesRead == 0) f_lseek(&wavFile, sizeof(WAV_Header_TypeDef));
                }
            }
        }
    }
}
