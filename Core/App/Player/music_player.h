#ifndef MUSIC_PLAYER_H
#define MUSIC_PLAYER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include "cmsis_os.h"
    typedef enum
    {
        MUSIC_FORMAT_WAV,
        MUSIC_FORMAT_MP3,
    } MusicSong_Format;

    typedef struct
    {
        char name[64];
        MusicSong_Format format;
    } MusicSong_TypeDef;

    typedef enum
    {
        MUSIC_RELOAD,
        MUSIC_PAUSE,
        MUSIC_RESUME,
        MUSIC_STOP,
        MUSIC_SET_HEADPHONE_VOL,
        MUSIC_SET_SPEAKER_VOL,
    } Music_EventType;

    typedef struct
    {
        Music_EventType type;
        uint8_t param;  // 用于音量值等参数
    } Music_Event;

    extern uint8_t isPlaying;
    extern osMessageQueueId_t music_eventQueueHandle;

    void music_player_init(void);
    void music_player_resume(void);
    void music_player_pause(void);
    void music_player_stop(void);

    void music_player_setVolume(uint8_t volume);
    void music_player_set_headphone_volume(uint8_t volume);
    void music_player_set_speaker_volume(uint8_t volume);
    void music_player_process_song();
    void music_player_update(void);

    void music_player_set_currentIndex(uint16_t index);

    const uint16_t music_player_get_currentIndex(void);
    char *music_player_get_currentName();
    const uint16_t music_player_get_song_count(void);
    const MusicSong_TypeDef *music_player_get_playlist(void);
#ifdef __cplusplus
}
#endif

#endif  // MUSIC_PLAYER_H
