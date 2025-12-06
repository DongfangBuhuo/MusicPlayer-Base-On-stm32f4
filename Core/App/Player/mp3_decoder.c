/*
 * mp3_decoder.c
 * MP3 解码器实现 (基于 Helix MP3 Decoder)
 *
 * Created on: Dec 6, 2025
 */

#include "mp3_decoder.h"
#include <string.h>

// === 关键配置 ===
#define HELIX_MP3_AVAILABLE

#ifdef HELIX_MP3_AVAILABLE
// 包含 Helix 库头文件
// 由于我们已经将头文件复制到了 Core/Inc，直接包含即可
#include "mp3dec.h"

// Helix 定义的结构体是 MP3FrameInfo，我们需要在内部使用
typedef MP3FrameInfo MP3FrameInfo_Helix;
#else
#error "Helix MP3 Decoder library not found! Please check installation."
#endif

// ============================================================================
// MP3 解码器接口实现
// ============================================================================

MP3_DecoderHandle MP3_Decoder_Init(void)
{
    // 调用 Helix 的初始化函数
    // 注意：Helix 的实现中，MP3InitDecoder 返回一个 HMP3Decoder 句柄
    HMP3Decoder decoder = MP3InitDecoder();
    return (MP3_DecoderHandle)decoder;
}

void MP3_Decoder_Free(MP3_DecoderHandle decoder)
{
    if (decoder)
    {
        MP3FreeDecoder((HMP3Decoder)decoder);
    }
}

MP3_Error MP3_Decoder_DecodeFrame(MP3_DecoderHandle decoder, uint8_t **inBuffer, int *bytesLeft, int16_t *outBuffer,
                                  MP3_FrameInfo *frameInfo)
{
    if (!decoder || !inBuffer || !bytesLeft || !outBuffer)
    {
        return MP3_ERR_NULL_POINTER;
    }

    // 查找同步字
    // Helix 提供的函数原型: int MP3FindSyncWord(unsigned char *buf, int nBytes);
    int offset = MP3FindSyncWord(*inBuffer, *bytesLeft);
    if (offset < 0)
    {
        return MP3_ERR_INVALID_FILE;
    }

    // 跳过无效数据
    *inBuffer += offset;
    *bytesLeft -= offset;

    // 解码一帧
    // Helix 原型: int MP3Decode(HMP3Decoder hMP3Decoder, unsigned char **inbuf, int *bytesLeft, short *outbuf, int
    // useSize);
    int err = MP3Decode((HMP3Decoder)decoder, inBuffer, bytesLeft, outBuffer, 0);
    if (err != 0)
    {
        // 返回对应错误码
        return (MP3_Error)err;
    }

    // 获取帧信息
    if (frameInfo)
    {
        MP3FrameInfo_Helix helixInfo;
        MP3GetLastFrameInfo((HMP3Decoder)decoder, &helixInfo);

        frameInfo->bitrate = helixInfo.bitrate;
        frameInfo->sampleRate = helixInfo.samprate;
        frameInfo->channels = helixInfo.nChans;
        frameInfo->bitsPerSample = helixInfo.bitsPerSample;
        frameInfo->outputSamps = helixInfo.outputSamps;
    }

    return MP3_OK;
}

int MP3_FindSyncWord(uint8_t *buffer, int bufferSize)
{
    return MP3FindSyncWord(buffer, bufferSize);
}

/**
 * @brief  跳过 ID3v2 标签
 * @note   ID3v2 标签在 MP3 文件开头，格式:
 *         - 3 bytes: "ID3"
 *         - 2 bytes: 版本
 *         - 1 byte:  标志
 *         - 4 bytes: 大小 (syncsafe integer)
 */
uint32_t MP3_SkipID3Tag(FIL *file)
{
    uint8_t header[10];
    UINT bytesRead;

    // 保存当前位置
    FSIZE_t startPos = f_tell(file);

    // 读取 ID3 头
    if (f_read(file, header, 10, &bytesRead) != FR_OK || bytesRead != 10)
    {
        f_lseek(file, startPos);
        return 0;
    }

    // 检查 ID3v2 标签
    if (header[0] == 'I' && header[1] == 'D' && header[2] == '3')
    {
        // 计算标签大小 (syncsafe integer)
        uint32_t tagSize =
            ((header[6] & 0x7F) << 21) | ((header[7] & 0x7F) << 14) | ((header[8] & 0x7F) << 7) | (header[9] & 0x7F);

        // 跳过标签 (10 bytes header + tagSize)
        uint32_t totalSkip = 10 + tagSize;
        f_lseek(file, startPos + totalSkip);

        return totalSkip;
    }

    // 没有 ID3 标签，恢复位置
    f_lseek(file, startPos);
    return 0;
}
