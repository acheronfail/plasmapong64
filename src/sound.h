#ifndef SOUND_H
#define SOUND_H
#include "game.h"
#include <stddef.h>
#include <stdint.h>
#define SOUND_RATE 16000
#define SOUND_VOICES 12
typedef struct {
    const int16_t *pcm;
    unsigned length,loop_start;
    uint32_t position;
    int gain,target,left,right;
    bool loop;
} SoundVoice;
typedef struct { SoundVoice voice[SOUND_VOICES]; uint32_t step; unsigned next; Phase phase; } Sound;
void sound_init(Sound *s,unsigned rate);
/* Call with interrupts disabled when sharing with the N64 audio callback. */
void sound_update(Sound *s,const Game *g);
void sound_render(Sound *s,int16_t *stereo,size_t frames);
#endif
