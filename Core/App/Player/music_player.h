#ifndef MUSIC_PLAYER_H
#define MUSIC_PLAYER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

    extern uint8_t isPlaying;

    void music_player_init(void);
    void music_player_play(const char *filename);
    void music_player_task(void);  // Audio processing task loop
    void music_player_setVolume(uint8_t volume);
    void music_player_set_headphone_volume(uint8_t volume);
    void music_player_set_speaker_volume(uint8_t volume);
#ifdef __cplusplus
}
#endif

#endif  // MUSIC_PLAYER_H
