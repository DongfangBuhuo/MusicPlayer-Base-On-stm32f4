/* Includes ------------------------------------------------------------------*/
#include "music_player.h"
#include "mp3_decoder.h"
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
// 音频缓冲区大小设置为 4608 (2304 stereo samples * 2 bytes = 9216 bytes)
// 这样做是为了匹配 Helix MP3 解码器一帧的输出大小 (1152 stereo samples * 2 = 2304 samples)
// 每次半传输中断(2304 samples)刚好对应一帧解码数据，避免数据断流和杂音
#define AUDIO_BUFFER_SIZE 4608  // +9KB RAM usage (需要减小 FreeRTOS Heap)
#define MAX_PLAYLIST_SIZE 100
#define MP3_INBUF_SIZE 5120   // MP3 输入缓冲区大小 (5KB, 与正点原子 MP3_FILE_BUF_SZ 一致)
#define MP3_OUTBUF_SIZE 2304  // MP3 输出缓冲区大小 (1152 samples * 2 channels)

/* Private variables ---------------------------------------------------------*/
// --- Audio Buffer (WAV 和 MP3 共用) ---
// 强制 4 字节对齐，优化 DMA 和解码访问
static uint16_t audio_buffer[AUDIO_BUFFER_SIZE] __attribute__((aligned(4)));

// --- File System Objects ---
static FATFS fs;
static FIL musicFile;
static WAV_Header_TypeDef wavHeader;

// --- MP3 Decoder ---
static MP3_DecoderHandle mp3Decoder = NULL;
static uint8_t mp3InBuffer[MP3_INBUF_SIZE] __attribute__((aligned(4)));
static int16_t mp3OutBuffer[MP3_OUTBUF_SIZE] __attribute__((aligned(4)));
static int mp3BytesLeft = 0;
static uint8_t *mp3ReadPtr = NULL;
static int pcm_offset = 0;     // Offset in mp3OutBuffer for next read (Moved from mp3_fill_buffer)
static int pcm_available = 0;  // Number of samples available in mp3OutBuffer (Moved from mp3_fill_buffer)
static MusicSong_Format currentFormat = MUSIC_FORMAT_WAV;

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
static void music_player_process_wav(void);
static void music_player_process_mp3(void);

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
    }

    // 设置初始音量 (0-33 范围, 16 约 50%)
    ES8388_SetSpeakerEnable(1);
    ES8388_SetVolume(16);

    // LED闪1次表示ES8388初始化成功
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);
    HAL_Delay(300);
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_RESET);
    HAL_Delay(300);

    // === 初始化 MP3 解码器 ===
    mp3Decoder = MP3_Decoder_Init();
    if (!mp3Decoder)
    {
        // MP3 解码器初始化失败（可能是 Helix 库未安装）
        // LED 闪 2 次警告
        for (int i = 0; i < 2; i++)
        {
            HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);
            HAL_Delay(150);
            HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_RESET);
            HAL_Delay(150);
        }
    }

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
/**
 * @brief  Fill audio buffer with MP3 decoded data
 * @param  dst: Destination buffer (pointer to int16_t)
 * @param  num_samples: Number of samples to fill
 * @retval Number of samples actually filled
 */
static int mp3_fill_buffer(int16_t *dst, int num_samples)
{
    int samples_filled = 0;
    int loop_guard = 0;

    while (samples_filled < num_samples)
    {
        // Watchdog: Avoid infinite loop on corrupted files
        if (++loop_guard > 500) return samples_filled;

        // 1. If we have decoded data available, copy it
        if (pcm_available > 0)
        {
            int to_copy =
                (pcm_available < (num_samples - samples_filled)) ? pcm_available : (num_samples - samples_filled);
            memcpy(dst + samples_filled, mp3OutBuffer + pcm_offset, to_copy * sizeof(int16_t));

            samples_filled += to_copy;
            pcm_offset += to_copy;
            pcm_available -= to_copy;
        }
        else
        {
            // 2. Decode next frame
            // Refill input buffer if needed (参考正点原子: bytesleft < MAINBUF_SIZE * 2)
            // MAINBUF_SIZE = 1940, so threshold ≈ 3880. We use 2000 for safety.
            if (mp3BytesLeft < 2000)
            {
                // Move remaining data to beginning
                memmove(mp3InBuffer, mp3ReadPtr, mp3BytesLeft);
                mp3ReadPtr = mp3InBuffer;

                // Read more data
                UINT br;
                f_read(&musicFile, mp3InBuffer + mp3BytesLeft, MP3_INBUF_SIZE - mp3BytesLeft, &br);
                mp3BytesLeft += br;

                if (br == 0 && mp3BytesLeft == 0) return samples_filled;  // End of file
            }

            // Decode
            MP3_FrameInfo frameInfo;
            MP3_Error err = MP3_Decoder_DecodeFrame(mp3Decoder, &mp3ReadPtr, &mp3BytesLeft, mp3OutBuffer, &frameInfo);

            if (err == MP3_OK)
            {
                pcm_offset = 0;
                pcm_available = frameInfo.outputSamps;
            }
            else if (err == MP3_ERR_INDATA_UNDERFLOW || err == MP3_ERR_MAINDATA_UNDERFLOW)
            {
                // 数据不足，需要更多数据 - 强制触发 refill
                // 不跳过数据，只是让下一次迭代读取更多
                if (mp3BytesLeft > 0)
                {
                    memmove(mp3InBuffer, mp3ReadPtr, mp3BytesLeft);
                    mp3ReadPtr = mp3InBuffer;

                    UINT br;
                    f_read(&musicFile, mp3InBuffer + mp3BytesLeft, MP3_INBUF_SIZE - mp3BytesLeft, &br);
                    mp3BytesLeft += br;

                    if (br == 0) return samples_filled;  // EOF
                }
                else
                {
                    return samples_filled;  // EOF
                }
            }
            else
            {
                // 其他错误: 跳过损坏数据
                if (mp3BytesLeft > 0)
                {
                    int offset = MP3_FindSyncWord(mp3ReadPtr + 1, mp3BytesLeft - 1);
                    if (offset >= 0)
                    {
                        mp3ReadPtr += (offset + 1);
                        mp3BytesLeft -= (offset + 1);
                    }
                    else
                    {
                        mp3BytesLeft = 0;
                    }
                }
                else
                {
                    return samples_filled;  // EOF
                }
            }
        }
    }
    return samples_filled;
}

static void music_player_process_wav(void)
{
    FRESULT res;
    UINT bytesRead;
    char music_full_name[128];

    snprintf(music_full_name, sizeof(music_full_name), "0:/music/%s", playlist[current_song_index].name);

    res = f_open(&musicFile, music_full_name, FA_READ);
    if (res != FR_OK) return;

    res = f_read(&musicFile, &wavHeader, sizeof(wavHeader), &bytesRead);
    if (res != FR_OK || strncmp(wavHeader.Format, "WAVE", 4) != 0)
    {
        f_close(&musicFile);
        return;
    }

    hi2s2.Init.AudioFreq = wavHeader.SampleRate;
    HAL_I2S_Init(&hi2s2);

    // Initial fill (读取 AUDIO_BUFFER_SIZE * 2 字节 = 填满整个缓冲区)
    f_read(&musicFile, audio_buffer, AUDIO_BUFFER_SIZE * 2, &bytesRead);

    if (HAL_I2S_Transmit_DMA(&hi2s2, audio_buffer, AUDIO_BUFFER_SIZE) != HAL_OK)
    {
        f_close(&musicFile);
        return;
    }

    // LED ON
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);

    taskENTER_CRITICAL();
    isPlaying = 1;
    taskEXIT_CRITICAL();
}

static void music_player_process_mp3(void)
{
    FRESULT res;
    UINT br;
    char music_full_name[128];

    snprintf(music_full_name, sizeof(music_full_name), "0:/music/%s", playlist[current_song_index].name);

    res = f_open(&musicFile, music_full_name, FA_READ);
    if (res != FR_OK) return;

    // 重置解码器以清除旧状态
    if (mp3Decoder)
    {
        MP3_Decoder_Free(mp3Decoder);
    }
    mp3Decoder = MP3_Decoder_Init();
    if (!mp3Decoder)
    {
        f_close(&musicFile);
        return;
    }

    // Reset MP3 state variables
    pcm_offset = 0;
    pcm_available = 0;

    // init mp3 state
    mp3BytesLeft = 0;
    mp3ReadPtr = mp3InBuffer;

    // Skip ID3
    MP3_SkipID3Tag(&musicFile);

    // Fill input buffer
    f_read(&musicFile, mp3InBuffer, MP3_INBUF_SIZE, &br);
    mp3BytesLeft = br;
    mp3ReadPtr = mp3InBuffer;

    // Find valid frame to get info
    MP3_FrameInfo frameInfo;
    int retries = 0;
    const int MAX_RETRIES = 100;  // 防止无限循环

    // Decode until we get a valid frame or run out
    while (retries < MAX_RETRIES)
    {
        int offset = MP3_FindSyncWord(mp3ReadPtr, mp3BytesLeft);
        if (offset < 0)
        {
            // Refill
            memmove(mp3InBuffer, mp3ReadPtr, mp3BytesLeft);
            mp3ReadPtr = mp3InBuffer;
            f_read(&musicFile, mp3InBuffer + mp3BytesLeft, MP3_INBUF_SIZE - mp3BytesLeft, &br);
            if (br == 0)
            {
                f_close(&musicFile);
                return;
            }  // EOF
            mp3BytesLeft += br;
            retries++;
            continue;
        }
        mp3ReadPtr += offset;
        mp3BytesLeft -= offset;

        MP3_Error err = MP3_Decoder_DecodeFrame(mp3Decoder, &mp3ReadPtr, &mp3BytesLeft, mp3OutBuffer, &frameInfo);
        if (err == MP3_OK)
        {
            // Valid frame found
            break;
        }
        else
        {
            // 解码失败，跳过一个字节继续寻找
            if (mp3BytesLeft > 0)
            {
                mp3ReadPtr++;
                mp3BytesLeft--;
            }
            retries++;
        }
    }

    // 如果超过最大重试次数，放弃
    if (retries >= MAX_RETRIES)
    {
        f_close(&musicFile);
        return;
    }

    // Init I2S
    hi2s2.Init.AudioFreq = frameInfo.sampleRate;
    HAL_I2S_Init(&hi2s2);

    // 重新定位文件到 ID3 标签之后，重新开始解码
    f_lseek(&musicFile, 0);
    MP3_SkipID3Tag(&musicFile);

    // 重新初始化解码器以清除 bit reservoir 状态
    if (mp3Decoder)
    {
        MP3_Decoder_Free(mp3Decoder);
    }
    mp3Decoder = MP3_Decoder_Init();
    if (!mp3Decoder)
    {
        f_close(&musicFile);
        return;
    }

    // 重置 MP3 缓冲区状态
    mp3BytesLeft = 0;
    mp3ReadPtr = mp3InBuffer;
    pcm_offset = 0;
    pcm_available = 0;

    // Pre-fill buffer
    mp3_fill_buffer((int16_t *)audio_buffer, AUDIO_BUFFER_SIZE);

    // Start DMA
    HAL_I2S_Transmit_DMA(&hi2s2, audio_buffer, AUDIO_BUFFER_SIZE);

    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);
    taskENTER_CRITICAL();
    isPlaying = 1;
    taskEXIT_CRITICAL();
}

/**
 * @brief  Actual audio processing logic (runs in task)
 */
void music_player_process_song()
{
    // Stop previous
    if (isPlaying)
    {
        isPlaying = 0;
        HAL_I2S_DMAStop(&hi2s2);
        f_close(&musicFile);
        osDelay(10);
    }

    // Determine format
    current_song_index = music_player_get_currentIndex();
    if (playlist[current_song_index].format == MUSIC_FORMAT_WAV)
    {
        currentFormat = MUSIC_FORMAT_WAV;
        music_player_process_wav();
    }
    else
    {
        currentFormat = MUSIC_FORMAT_MP3;
        music_player_process_mp3();
    }
}

void music_player_update(void)
{
    UINT bytesRead;

    if (osSemaphoreAcquire(audio_semHandle, 10) == osOK)
    {
        int16_t *target_buffer = NULL;

        if (audio_state == 1)
        {
            target_buffer = (int16_t *)audio_buffer;
            audio_state = 0;
        }
        else if (audio_state == 2)
        {
            target_buffer = (int16_t *)&audio_buffer[AUDIO_BUFFER_SIZE / 2];
            audio_state = 0;
        }

        if (target_buffer)
        {
            // 每次回调填充半个缓冲区 = AUDIO_BUFFER_SIZE / 2 个采样
            int half_buffer_samples = AUDIO_BUFFER_SIZE / 2;

            if (currentFormat == MUSIC_FORMAT_WAV)
            {
                // WAV: 读取 half_buffer_samples * 2 字节
                f_read(&musicFile, target_buffer, half_buffer_samples * sizeof(int16_t), &bytesRead);
                if (bytesRead < half_buffer_samples * sizeof(int16_t)) music_player_stop();
            }
            else
            {
                // MP3: 填充半个缓冲区的采样
                int filled = mp3_fill_buffer(target_buffer, half_buffer_samples);
                if (filled < half_buffer_samples) music_player_stop();
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
    ES8388_SetVolume(volume);
}

/**
 * @brief  Set speaker volume
 * @param  volume: Volume level (0-33)
 * @retval None
 */
void music_player_set_speaker_volume(uint8_t volume)
{
    if (volume > 33) volume = 33;

    // 使用 ES8388_SetSpeakerVol 确保副本被更新
    ES8388_SetSpeakerVol(volume);
}
/**
 * @brief  Get song count in playlist
 * @retval Number of songs
 */

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
    current_song_index = index % music_count;
}

char *music_player_get_currentName()
{
    return current_song_name;
}

const MusicSong_TypeDef *music_player_get_playlist(void)
{
    return playlist;
}

/**
 * @brief  读取耳机音量 (百分比)
 * @retval 音量值 0~100
 */
uint8_t music_player_get_headphone_volume(void)
{
    uint8_t hw_vol = ES8388_GetHeadphoneVolume();
    // 硬件值 0~33 转换为 0~100
    return (uint8_t)(hw_vol * 100 / 33);
}

/**
 * @brief  读取喇叭音量 (百分比)
 * @retval 音量值 0~100
 */
uint8_t music_player_get_speaker_volume(void)
{
    uint8_t hw_vol = ES8388_GetSpeakerVolume();
    // 硬件值 0~33 转换为 0~100
    return (uint8_t)(hw_vol * 100 / 33);
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
