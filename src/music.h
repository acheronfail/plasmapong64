#ifndef MUSIC_H
#define MUSIC_H
#include "game.h"
#include "sound.h"

enum { MUSIC_MENU, MUSIC_BATTLE, MUSIC_TRACKS, MUSIC_BUFFER_FRAMES=1024 };
/* Read native-endian stereo PCM at SOUND_RATE. Reads never cross a track end. */
typedef void (*MusicRead)(void *context,unsigned track,uint32_t frame,int16_t *out,unsigned frames);
typedef struct {
    MusicRead read;
    void *context;
    uint32_t length[MUSIC_TRACKS],position,fraction,step,buffer_start;
    unsigned track,target,buffer_frames,gain;
    _Alignas(16) int16_t buffer[MUSIC_BUFFER_FRAMES*2];
} Music;
void music_init(Music *m,unsigned rate,const uint32_t lengths[MUSIC_TRACKS],MusicRead read,void *context);
/* Only changes the requested track; cartridge I/O happens in music_mix. */
void music_update(Music *m,Phase phase);
void music_mix(Music *m,int16_t *stereo,size_t frames);
#endif
