/* Includes ------------------------------------------------------------------*/
#include "music_player.h"
#include "es8388.h"
#include "fatfs.h"
#include "i2c.h"
#include "i2s.h"
#include "main.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>

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
#define AUDIO_BUFFER_SIZE 2048  // 优化：从4096减少到2048以节省RAM
#define MAX_PLAYLIST_SIZE 100

/* Private variables ---------------------------------------------------------*/
// --- Audio Buffer (in SRAM) ---
static uint16_t audio_buffer[AUDIO_BUFFER_SIZE];

// --- File System Objects ---
static FATFS fs;
static FIL wavFile;
static WAV_Header_TypeDef wavHeader;

// --- Playlist Data ---
static MusicSong_TypeDef playlist[MAX_PLAYLIST_SIZE];
static uint16_t music_count = 0;

// --- Control Flags & State ---
uint8_t isPlaying = 0;
volatile uint8_t audio_state = 0;
char current_song_name[64] = {0};   // 去掉static，供freertos.c访问
volatile uint8_t request_play = 0;  // 去掉static，供freertos.c访问

// --- RTOS Objects ---
osSemaphoreId_t audio_semHandle = NULL;

/* External variables --------------------------------------------------------*/
extern I2S_HandleTypeDef hi2s2;
extern I2C_HandleTypeDef hi2c1;

/* Private function prototypes -----------------------------------------------*/
static void Bulid_MusicList(void);

/* Function implementations --------------------------------------------------*/

/**
 * @brief  Initialize music player (ES8388, SD card, etc.)
 * @retval None
 */
void music_player_init(void)
{
    FRESULT res;
    isPlaying = 0;

    // Create binary semaphore for audio buffer synchronization
    audio_semHandle = osSemaphoreNew(1, 0, NULL);

    // 初始化ES8388
    if (ES8388_Init(&hi2c1) != 0)
    {
        // LED常亮表示ES8388初始化失败
        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);
        while (1);
    }

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
    Bulid_MusicList();

    // 最后确保DMA中断已使能（double check）
    extern DMA_HandleTypeDef hdma_spi2_tx;
    __HAL_DMA_ENABLE_IT(&hdma_spi2_tx, DMA_IT_TC);
    __HAL_DMA_ENABLE_IT(&hdma_spi2_tx, DMA_IT_HT);
    HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
}

/**
 * @brief  Non-blocking play interface - only sets request flag
 * @param  filename: Name of the song file to play
 * @retval None
 */
void music_player_play(const char *filename)
{
    strncpy(current_song_name, filename, sizeof(current_song_name) - 1);
    request_play = 1;
}

/**
 * @brief  Actual audio processing logic (runs in task)
 * @param  filename: Name of the song file to process
 * @retval None
 */
void music_player_process_song(const char *filename)  // 去掉static，供freertos.c调用
{
    FRESULT res;
    UINT bytesRead;
    char music_full_name[64] = "0:/music/";

    // 停止之前的播放（如果需要）
    if (isPlaying)
    {
        isPlaying = 0;
        HAL_I2S_DMAStop(&hi2s2);
        f_close(&wavFile);
        HAL_Delay(100);  // 等待DMA停止
    }

    // 打开WAV文件
    strcat(music_full_name, filename);
    res = f_open(&wavFile, music_full_name, FA_READ);
    if (res != FR_OK)
    {
        // 尝试不带路径打开（兼容根目录）
        res = f_open(&wavFile, filename, FA_READ);
        if (res != FR_OK)
        {
            return;  // 打开失败直接返回，不要死循环
        }
    }

    // 读取WAV头
    res = f_read(&wavFile, &wavHeader, sizeof(wavHeader), &bytesRead);
    if (res != FR_OK || bytesRead != sizeof(wavHeader))
    {
        f_close(&wavFile);
        return;
    }

    // 检查是否为WAV文件
    if (strncmp(wavHeader.ChunkID, "RIFF", 4) != 0 || strncmp(wavHeader.Format, "WAVE", 4) != 0)
    {
        f_close(&wavFile);
        return;
    }

    // 配置I2S采样率
    hi2s2.Init.AudioFreq = wavHeader.SampleRate;
    if (HAL_I2S_Init(&hi2s2) != HAL_OK)
    {
        f_close(&wavFile);
        return;
    }

    // 填充初始缓存
    f_read(&wavFile, audio_buffer, AUDIO_BUFFER_SIZE * 2, &bytesRead);

    // 启动I2S DMA传输
    HAL_StatusTypeDef dma_status = HAL_I2S_Transmit_DMA(&hi2s2, audio_buffer, AUDIO_BUFFER_SIZE);

    if (dma_status != HAL_OK)
    {
        // DMA启动失败，LED快闪3次
        for (int i = 0; i < 3; i++)
        {
            HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);
            HAL_Delay(200);
            HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_RESET);
            HAL_Delay(200);
        }
        f_close(&wavFile);
        return;
    }

    // DMA启动成功，LED保持常亮
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);

    // 手动确保I2S外设已使能（HAL_I2S_Transmit_DMA应该已经做了，但我们double check）
    __HAL_I2S_ENABLE(&hi2s2);

    isPlaying = 1;

    // 播放循环
    while (isPlaying)
    {
        // 检查是否有新的播放请求，如果有则立即退出当前播放
        if (request_play)
        {
            break;
        }

        // 等待半传输或传输完成中断信号
        if (osSemaphoreAcquire(audio_semHandle, 100) == osOK)  // 超时100ms，避免死锁
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
                // 文件结束
                isPlaying = 0;
                break;
            }
        }
    }

    // 停止传输
    HAL_I2S_DMAStop(&hi2s2);
    f_close(&wavFile);
    isPlaying = 0;
}

/**
 * @brief  Audio task main loop (runs in AudioTask)
 * @retval None
 */

/**
 * @brief  Set headphone volume
 * @param  volume: Volume level (0-33)
 * @retval None
 */
void music_player_set_headphone_volume(uint8_t volume)
{
    if (volume > 33)
    {
        volume = 33;
    }
    ES8388_Write_Reg(ES8388_DACCONTROL24, volume);  // LOUT1 (Headphone L)
    ES8388_Write_Reg(ES8388_DACCONTROL25, volume);  // ROUT1 (Headphone R)
}

/**
 * @brief  Set speaker volume
 * @param  volume: Volume level (0-33)
 * @retval None
 */
void music_player_set_speaker_volume(uint8_t volume)
{
    if (volume > 33)
    {
        volume = 33;
    }
    ES8388_Write_Reg(ES8388_DACCONTROL26, volume);  // LOUT2 (Speaker L)
    ES8388_Write_Reg(ES8388_DACCONTROL27, volume);  // ROUT2 (Speaker R)
}

/**
 * @brief  Get playlist pointer
 * @retval Pointer to the playlist array (read-only)
 */
const MusicSong_TypeDef *music_player_get_playlist(void)
{
    return playlist;
}

/**
 * @brief  Get song count in playlist
 * @retval Number of songs
 */
uint16_t music_player_get_song_count(void)
{
    return music_count;
}

/**
 * @brief  Build music list by scanning SD card
 * @retval None
 * @note   To be implemented
 */
static void Bulid_MusicList(void)
{
    DIR dir;
    FILINFO fno;
    FRESULT res;

    res = f_opendir(&dir, "0:/music");
    if (res != FR_OK)
    {
        return;
    }

    while (1)
    {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0)
        {
            break;
        }
        if (fno.fattrib & AM_DIR)
        {
            continue;
        }
        if (strcmp(fno.fname + strlen(fno.fname) - 4, ".wav") == 0)
        {
            strncpy(playlist[music_count].name, fno.fname, sizeof(playlist[music_count].name) - 1);
            playlist[music_count].name[255] = '\0';  // 确保 null 结尾
            playlist[music_count].format = MUSIC_FORMAT_WAV;
            music_count++;
        }
        else if (strcmp(fno.fname + strlen(fno.fname) - 4, ".mp3") == 0)
        {
            strncpy(playlist[music_count].name, fno.fname, sizeof(playlist[music_count].name) - 1);
            playlist[music_count].name[255] = '\0';  // 确保 null 结尾
            playlist[music_count].format = MUSIC_FORMAT_MP3;
            music_count++;
        }
    }
    f_closedir(&dir);
}
void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s == &hi2s2)
    {
        audio_state = 1;
        osSemaphoreRelease(audio_semHandle);
    }
}

void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s == &hi2s2)
    {
        audio_state = 2;
        osSemaphoreRelease(audio_semHandle);
    }
}
