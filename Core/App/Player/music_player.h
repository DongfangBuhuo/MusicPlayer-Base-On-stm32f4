#ifndef MUSIC_PLAYER_H
#define MUSIC_PLAYER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

    typedef enum
    {
        MUSIC_FORMAT_WAV,
        MUSIC_FORMAT_MP3,
    } MusicSong_Format;

    typedef struct
    {
        char name[256];
        MusicSong_Format format;
    } MusicSong_TypeDef;

    extern uint8_t isPlaying;

    void music_player_init(void);
    void music_player_play(const char *filename);
    void music_player_setVolume(uint8_t volume);
    void music_player_set_headphone_volume(uint8_t volume);
    void music_player_set_speaker_volume(uint8_t volume);
    void music_player_process_song(const char *filename);

    uint16_t music_player_get_song_count(void);
    const MusicSong_TypeDef *music_player_get_playlist(void);
#ifdef __cplusplus
}
#endif

#endif  // MUSIC_PLAYER_H
