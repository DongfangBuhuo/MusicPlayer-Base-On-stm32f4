#ifndef MUSIC_PLAYER_H
#define MUSIC_PLAYER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void music_player_init(void);
void music_player_play(const char *filename);
void music_player_process(void);

#ifdef __cplusplus
}
#endif

#endif // MUSIC_PLAYER_H
