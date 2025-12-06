/*
 * mp3_decoder.h
 * MP3 解码器接口 (基于 Helix MP3 Decoder)
 *
 * Created on: Dec 6, 2025
 */

#ifndef APP_PLAYER_MP3_DECODER_H_
#define APP_PLAYER_MP3_DECODER_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include "fatfs.h"

    /* MP3 解码器错误码 */
    typedef enum
    {
        MP3_OK = 0,
        MP3_ERR_INDATA_UNDERFLOW = -1,
        MP3_ERR_MAINDATA_UNDERFLOW = -2,
        MP3_ERR_FREE_BITRATE_SYNC = -3,
        MP3_ERR_OUT_OF_MEMORY = -4,
        MP3_ERR_NULL_POINTER = -5,
        MP3_ERR_INVALID_FILE = -6,
        MP3_ERR_FILE_READ = -7,
        MP3_ERR_DECODER_INIT = -8,
    } MP3_Error;

    /* MP3 帧信息 */
    typedef struct
    {
        uint32_t bitrate;        // 比特率 (bps)
        uint32_t sampleRate;     // 采样率 (Hz)
        uint8_t channels;        // 声道数 (1=mono, 2=stereo)
        uint16_t bitsPerSample;  // 采样位深 (16)
        uint32_t outputSamps;    // 输出采样数
    } MP3_FrameInfo;

    /* MP3 解码器句柄 (不透明类型) */
    typedef void *MP3_DecoderHandle;

    /**
     * @brief  初始化 MP3 解码器
     * @retval MP3 解码器句柄，NULL 表示失败
     */
    MP3_DecoderHandle MP3_Decoder_Init(void);

    /**
     * @brief  释放 MP3 解码器
     * @param  decoder: 解码器句柄
     */
    void MP3_Decoder_Free(MP3_DecoderHandle decoder);

    /**
     * @brief  解码一帧 MP3 数据
     * @param  decoder: 解码器句柄
     * @param  inBuffer: 输入 MP3 数据缓冲区 (指针会被更新)
     * @param  bytesLeft: 剩余字节数 (会被更新)
     * @param  outBuffer: 输出 PCM 缓冲区 (16-bit signed)
     * @param  frameInfo: 输出帧信息
     * @retval MP3_Error 错误码
     */
    MP3_Error MP3_Decoder_DecodeFrame(MP3_DecoderHandle decoder, uint8_t **inBuffer, int *bytesLeft, int16_t *outBuffer,
                                      MP3_FrameInfo *frameInfo);

    /**
     * @brief  查找下一个 MP3 帧同步头
     * @param  buffer: 输入缓冲区
     * @param  bufferSize: 缓冲区大小
     * @retval 帧同步头偏移量，-1 表示未找到
     */
    int MP3_FindSyncWord(uint8_t *buffer, int bufferSize);

    /**
     * @brief  跳过 ID3 标签
     * @param  file: 文件句柄
     * @retval 跳过的字节数
     */
    uint32_t MP3_SkipID3Tag(FIL *file);

#ifdef __cplusplus
}
#endif

#endif /* APP_PLAYER_MP3_DECODER_H_ */
