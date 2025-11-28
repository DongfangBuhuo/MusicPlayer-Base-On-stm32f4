#include "music_player.h"
#include "es8388.h"
#include "fatfs.h"
#include "i2c.h"
#include "i2s.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

/* Private typedef -----------------------------------------------------------*/
typedef struct {
  char ChunkID[4]; // "RIFF"
  uint32_t ChunkSize;
  char Format[4];      // "WAVE"
  char Subchunk1ID[4]; // "fmt "
  uint32_t Subchunk1Size;
  uint16_t AudioFormat;
  uint16_t NumChannels;
  uint32_t SampleRate;
  uint32_t ByteRate;
  uint16_t BlockAlign;
  uint16_t BitsPerSample;
  char Subchunk2ID[4]; // "data"
  uint32_t Subchunk2Size;
} WAV_Header_TypeDef;

/* Private define ------------------------------------------------------------*/
#define AUDIO_BUFFER_SIZE 4096 // 4KB 缓存

/* Private variables ---------------------------------------------------------*/
static uint16_t audio_buffer[AUDIO_BUFFER_SIZE];
static FATFS fs;
static FIL wavFile;
static WAV_Header_TypeDef wavHeader;
static uint8_t isPlaying = 0;

/* External variables --------------------------------------------------------*/
extern I2S_HandleTypeDef hi2s2;
extern I2C_HandleTypeDef hi2c1;

void music_player_init(void) {
  FRESULT res;

  // 初始化ES8388
  if (ES8388_Init(&hi2c1) != 0) {
    // LED常亮表示ES8388初始化失败
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);
    while (1)
      ;
  }

  // 明确关闭喇叭输出，只使用耳机
  ES8388_SetSpeakerEnable(0);

  // LED闪1次表示ES8388初始化成功
  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);
  HAL_Delay(300);
  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_RESET);
  HAL_Delay(300);

  // 挂载SD卡
  res = f_mount(&fs, "0:/", 1);
  if (res != FR_OK) {
    // LED慢闪表示SD卡挂载失败
    while (1) {
      HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);
      HAL_Delay(500);
    }
  }

  // LED闪2次表示SD卡挂载成功
  for (int i = 0; i < 2; i++) {
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);
    HAL_Delay(200);
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_RESET);
    HAL_Delay(200);
  }
}

void music_player_play(const char *filename) {
  FRESULT res;
  UINT bytesRead;

  // 打开WAV文件
  res = f_open(&wavFile, filename, FA_READ);
  if (res != FR_OK) {
    // LED快速闪烁表示文件打开失败
    while (1) {
      HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);
      HAL_Delay(100);
    }
  }

  // 读取WAV头
  f_read(&wavFile, &wavHeader, sizeof(wavHeader), &bytesRead);

  // 简单检查是否为 WAV 格式
  if (strncmp(wavHeader.ChunkID, "RIFF", 4) == 0 &&
      strncmp(wavHeader.Format, "WAVE", 4) == 0) {

    // 根据WAV文件的采样率重新配置I2S
    uint32_t sampleRate = wavHeader.SampleRate;

    // 停止I2S（如果正在运行）
    HAL_I2S_DMAStop(&hi2s2);

    // 根据采样率重新初始化I2S
    if (sampleRate == 8000) {
      hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_8K;
    } else if (sampleRate == 11025) {
      hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_11K;
    } else if (sampleRate == 16000) {
      hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_16K;
    } else if (sampleRate == 22050) {
      hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_22K;
    } else if (sampleRate == 32000) {
      hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_32K;
    } else if (sampleRate == 44100) {
      hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_44K;
    } else if (sampleRate == 48000) {
      hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_48K;
    } else if (sampleRate == 96000) {
      hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_96K;
    } else if (sampleRate == 192000) {
      hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_192K;
    } else {
      // 不支持的采样率，默认使用44.1kHz
      hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_44K;
    }

    // 重新初始化I2S
    HAL_I2S_Init(&hi2s2);

    // 读取第一块音频数据
    f_read(&wavFile, audio_buffer, AUDIO_BUFFER_SIZE * 2, &bytesRead);

    // LED关闭表示准备播放
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_RESET);
    HAL_Delay(500);

    // 启动 DMA 循环传输
    HAL_I2S_Transmit_DMA(&hi2s2, audio_buffer, AUDIO_BUFFER_SIZE);

    isPlaying = 1;
  } else {
    // 文件格式错误，LED持续快闪
    f_close(&wavFile);
    while (1) {
      HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);
      HAL_Delay(50);
    }
  }
}

void music_player_process(void) {
  static uint32_t led_timer = 0;
  if (isPlaying && (HAL_GetTick() - led_timer > 1000)) {
    led_timer = HAL_GetTick();
    HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);
  }
}

/**
 * @brief  Tx Transfer Half completed callback
 * @param  hi2s: I2S handle
 * @retval None
 */
void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s) {
  if (hi2s->Instance == SPI2) {
    UINT bytesRead;
    // 填充缓冲区的前半部分
    f_read(&wavFile, &audio_buffer[0], AUDIO_BUFFER_SIZE, &bytesRead);

    if (bytesRead < AUDIO_BUFFER_SIZE) {
      // 播放结束或循环播放
      // 这里简单实现循环播放：回到数据区开始
      // 注意：这里需要知道数据区的偏移，简单起见重新打开或seek
      // 为了演示，我们填充0
      memset(&audio_buffer[0], 0, AUDIO_BUFFER_SIZE - bytesRead);
      // 如果完全读完了，seek回开头（这里简化处理，实际应该解析wav头找到data位置）
      if (bytesRead == 0) {
        f_lseek(&wavFile, sizeof(WAV_Header_TypeDef));
      }
    }
  }
}

/**
 * @brief  Tx Transfer completed callback
 * @param  hi2s: I2S handle
 * @retval None
 */
void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s) {
  if (hi2s->Instance == SPI2) {
    UINT bytesRead;
    // 填充缓冲区的后半部分
    // 注意：audio_buffer是uint16_t数组，AUDIO_BUFFER_SIZE是元素个数
    // 缓冲区总大小是 AUDIO_BUFFER_SIZE * 2 字节
    // 前半部分是 0 到 AUDIO_BUFFER_SIZE/2 - 1
    // 后半部分是 AUDIO_BUFFER_SIZE/2 到 AUDIO_BUFFER_SIZE - 1
    // 每次填充一半，即 AUDIO_BUFFER_SIZE/2 个元素，即 AUDIO_BUFFER_SIZE 字节

    f_read(&wavFile, &audio_buffer[AUDIO_BUFFER_SIZE / 2], AUDIO_BUFFER_SIZE,
           &bytesRead);

    if (bytesRead < AUDIO_BUFFER_SIZE) {
      memset(&audio_buffer[AUDIO_BUFFER_SIZE / 2], 0,
             AUDIO_BUFFER_SIZE - bytesRead);
      if (bytesRead == 0) {
        f_lseek(&wavFile, sizeof(WAV_Header_TypeDef));
      }
    }
  }
}
