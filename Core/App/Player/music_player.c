/* Includes ------------------------------------------------------------------*/
#include "music_player.h"
#include "es8388.h"
#include "fatfs.h"
#include "i2c.h"
#include "i2s.h"
#include "main.h"

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
static FIL musicFile;
static WAV_Header_TypeDef wavHeader;

// --- Playlist Data ---
static MusicSong_TypeDef playlist[MAX_PLAYLIST_SIZE];
static uint16_t music_count = 0;
static char current_song_name[64] = {0};
static uint16_t current_song_index = 0;
// --- Control Flags & State ---
uint8_t isPlaying = 0;
volatile uint8_t audio_state = 0;

// --- RTOS Objects ---
static osSemaphoreId_t audio_semHandle = NULL;
static osMessageQueueId_t audio_data_queueHandle = NULL;

osMessageQueueId_t music_eventQueueHandle = NULL;

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
    audio_data_queueHandle = osMessageQueueNew(4, sizeof(uint8_t), NULL);
    music_eventQueueHandle = osMessageQueueNew(5, sizeof(Music_Event), NULL);
    // 初始化ES8388
    if (ES8388_Init(&hi2c1) != 0)
    {
        // LED常亮表示ES8388初始化失败
        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);
        while (1);
    }

    // 设置初始音量 (与GUI默认值同步: vol_headphone=30 对应硬件10)
    music_player_set_headphone_volume(20);  // 30% -> 10/33
    music_player_set_speaker_volume(0);     // 初始禁用喇叭

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
    extern DMA_HandleTypeDef hdma_spi2_tx;
    __HAL_DMA_ENABLE_IT(&hdma_spi2_tx, DMA_IT_TC);
    __HAL_DMA_ENABLE_IT(&hdma_spi2_tx, DMA_IT_HT);
    HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
    Bulid_MusicList();
}

/**
 * @brief  Non-blocking play interface - only sets request flag
 * @param  filename: Name of the song file to play
 * @retval None
 */
void music_player_play(const char *filename) {}

/**
 * @brief  Actual audio processing logic (runs in task)
 * @param  filename: Name of the song file to process
 * @retval None
 */
void music_player_process_song()  // 去掉static，供freertos.c调用
{
    FRESULT res;
    UINT bytesRead;
    char music_full_name[128] = "0:/music/";

    // 停止之前的播放（如果需要）
    if (isPlaying)
    {
        isPlaying = 0;
        HAL_I2S_DMAStop(&hi2s2);
        f_close(&musicFile);
        HAL_Delay(100);  // 等待DMA停止
    }

    // 打开WAV文件
    strncpy(current_song_name, playlist[current_song_index].name, sizeof(current_song_name) - 1);
    current_song_name[sizeof(current_song_name) - 1] = '\0';

    snprintf(music_full_name, sizeof(music_full_name), "0:/music/%s", playlist[music_player_get_currentIndex()].name);
    if (playlist[music_player_get_currentIndex()].format == MUSIC_FORMAT_WAV)
    {
        res = f_open(&musicFile, music_full_name, FA_READ);
    }
    else
    {
        // mp3
    }
    if (res != FR_OK)
    {
        // 尝试不带路径打开（兼容根目录）
        res = f_open(&musicFile, music_full_name, FA_READ);
        if (res != FR_OK)
        {
            return;  // 打开失败直接返回，不要死循环
        }
    }

    // 读取WAV头
    res = f_read(&musicFile, &wavHeader, sizeof(wavHeader), &bytesRead);
    if (res != FR_OK || bytesRead != sizeof(wavHeader))
    {
        f_close(&musicFile);
        return;
    }

    // 检查是否为WAV文件
    if (strncmp(wavHeader.ChunkID, "RIFF", 4) != 0 || strncmp(wavHeader.Format, "WAVE", 4) != 0)
    {
        f_close(&musicFile);
        return;
    }

    // 配置I2S采样率
    //   HAL_I2S_DeInit(&hi2s2);
    hi2s2.Init.AudioFreq = wavHeader.SampleRate;
    if (HAL_I2S_Init(&hi2s2) != HAL_OK)
    {
        f_close(&musicFile);
        return;
    }

    // 填充初始缓存
    f_read(&musicFile, audio_buffer, AUDIO_BUFFER_SIZE * 2, &bytesRead);

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
        f_close(&musicFile);
        return;
    }
    // DMA启动成功，LED保持常亮
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);

    // 手动确保I2S外设已使能（匹配正常工作版本的逻辑）
    __HAL_I2S_ENABLE(&hi2s2);

    // 清空信号量，确保从干净状态开始
    while (osSemaphoreAcquire(audio_semHandle, 0) == osOK)
    {
    }

    taskENTER_CRITICAL();
    isPlaying = 1;
    taskEXIT_CRITICAL();

    // 立即进入数据填充循环，不要有延迟！
    music_player_update();
}

/**
 * @brief  Audio update loop - runs tight loop until stopped or control command arrives
 * @retval None
 */
void music_player_update(void)
{
    UINT bytesRead;
    // 紧密循环，保证数据及时填充
    while (isPlaying)
    {
        // 等待 DMA 中断信号
        if (osSemaphoreAcquire(audio_semHandle, 10) == osOK)
        {
            if (audio_state == 1)  // 半传输完成，填充前半部分
            {
                f_read(&musicFile, audio_buffer, AUDIO_BUFFER_SIZE, &bytesRead);
            }
            else if (audio_state == 2)  // 传输完成，填充后半部分
            {
                f_read(&musicFile, &audio_buffer[AUDIO_BUFFER_SIZE / 2], AUDIO_BUFFER_SIZE, &bytesRead);
            }

            if (bytesRead < AUDIO_BUFFER_SIZE)
            {
                // 文件结束
                music_player_stop();
                break;
            }
        }
    }
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
char *music_player_get_currentName(void)
{
    return current_song_name;
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

const uint16_t music_player_get_song_count(void)
{
    return music_count;
}
const uint16_t music_player_get_currentIndex(void)
{
    return current_song_index;
}

void music_player_set_currentIndex(uint16_t index)
{
    current_song_index = index;
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
            playlist[music_count].name[sizeof(playlist[music_count].name) - 1] = '\0';  // 确保 null 结尾
            playlist[music_count].format = MUSIC_FORMAT_WAV;
            music_count++;
        }
        else if (strcmp(fno.fname + strlen(fno.fname) - 4, ".mp3") == 0)
        {
            strncpy(playlist[music_count].name, fno.fname, sizeof(playlist[music_count].name) - 1);
            playlist[music_count].name[sizeof(playlist[music_count].name) - 1] = '\0';  // 确保 null 结尾
            playlist[music_count].format = MUSIC_FORMAT_MP3;
            music_count++;
        }
    }
    f_closedir(&dir);
}
void music_player_pause(void)
{
    HAL_I2S_DMAPause(&hi2s2);
    taskENTER_CRITICAL();
    isPlaying = 0;
    taskEXIT_CRITICAL();
}
void music_player_resume(void)
{
    HAL_I2S_DMAResume(&hi2s2);
    taskENTER_CRITICAL();
    isPlaying = 1;
    taskEXIT_CRITICAL();
}
void music_player_stop(void)
{
    HAL_I2S_DMAStop(&hi2s2);
    f_close(&musicFile);
    taskENTER_CRITICAL();
    isPlaying = 0;
    taskEXIT_CRITICAL();
    // 清空队列中的残留消息
    osMessageQueueReset(audio_data_queueHandle);
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
