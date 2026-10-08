#include "music.h"
#include <string.h>

void music_init(Music *m,unsigned rate,const uint32_t lengths[MUSIC_TRACKS],MusicRead read,void *context) {
    memset(m,0,sizeof(*m));
    memcpy(m->length,lengths,sizeof(m->length));
    m->read=read; m->context=context;
    m->step=(uint32_t)(((uint64_t)SOUND_RATE<<16)/(rate?rate:SOUND_RATE));
}
void music_update(Music *m,Phase phase) {
    m->target=phase==MENU || phase==OPTIONS || phase==SCORES?MUSIC_MENU:MUSIC_BATTLE;
}
static int16_t saturate(int value) {
    return (int16_t)(value<-32767?-32767:value>32767?32767:value);
}
void music_mix(Music *m,int16_t *stereo,size_t frames) {
    if(!m->read) return;
    for(size_t i=0;i<frames;i++) {
        /* Eight-ms fade at track changes; restart the incoming song once. */
        if(m->track!=m->target) {
            if(m->gain) m->gain-=128;
            if(!m->gain) {
                m->track=m->target; m->position=m->fraction=0;
                m->buffer_frames=0;
            }
        } else if(m->gain<64*256) m->gain+=128;
        if(!m->length[m->track]) continue;
        if(!m->buffer_frames || m->position<m->buffer_start ||
            m->position>=m->buffer_start+m->buffer_frames) {
            m->buffer_start=m->position/MUSIC_BUFFER_FRAMES*MUSIC_BUFFER_FRAMES;
            uint32_t remaining=m->length[m->track]-m->buffer_start;
            m->buffer_frames=remaining<MUSIC_BUFFER_FRAMES?remaining:MUSIC_BUFFER_FRAMES;
            m->read(m->context,m->track,m->buffer_start,m->buffer,m->buffer_frames);
        }
        unsigned offset=(m->position-m->buffer_start)*2;
        for(unsigned ch=0;ch<2;ch++)
            stereo[i*2+ch]=saturate(stereo[i*2+ch]+m->buffer[offset+ch]*(int)m->gain/65536);
        /* Separate whole/fractional cursors: these multi-minute tracks exceed
           the 65536-frame range of the effect mixer's Q16 position. */
        m->fraction+=m->step;
        m->position+=m->fraction>>16;
        if(m->position>=m->length[m->track]) m->position%=m->length[m->track];
        m->fraction&=65535;
    }
}
