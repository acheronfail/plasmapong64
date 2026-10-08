#include "music.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint32_t lengths[MUSIC_TRACKS]={2501,70003};
static unsigned reads,wraps;
static void read_pcm(void *context,unsigned track,uint32_t start,int16_t *out,unsigned count) {
    (void)context;
    assert(track<MUSIC_TRACKS && count && count<=MUSIC_BUFFER_FRAMES);
    assert(start%MUSIC_BUFFER_FRAMES==0 && start+count<=lengths[track]);
    reads++; if(!start) wraps++;
    for(unsigned i=0;i<count;i++) {
        out[i*2]=(int16_t)(4000+(start+i)%2000);
        out[i*2+1]=-out[i*2];
    }
}
int main(void) {
    Music a,b;
    int16_t whole[4096*2],parts[4096*2];
    music_init(&a,15926,lengths,read_pcm,NULL); b=a;
    /* More than a complete battle loop, crossing the effect cursor's 64K limit.
       Partitioning must preserve playback, ramps and stereo exactly. */
    for(unsigned n=0;n<80;n++) {
        Phase phase=n<3?MENU:n<5?OPTIONS:n<6?SCORES:n<30?PLAY:n<50?PAUSED:n<70?FINISHED:MENU;
        music_update(&a,phase); music_update(&b,phase);
        memset(whole,0,sizeof(whole)); memset(parts,0,sizeof(parts));
        music_mix(&a,whole,4096);
        for(unsigned offset=0;offset<4096;) {
            unsigned count=1+(offset+n*13)%128;
            if(count>4096-offset) count=4096-offset;
            music_mix(&b,parts+offset*2,count); offset+=count;
        }
        assert(!memcmp(whole,parts,sizeof(whole)));
        assert(a.position==b.position && a.fraction==b.fraction && a.gain==b.gain);
        assert(a.track==(n>=6 && n<70?MUSIC_BATTLE:MUSIC_MENU));
        for(unsigned i=0;i<4096;i++) assert(whole[i*2]==-whole[i*2+1]);
    }
    assert(reads>300 && wraps>20);
    /* Saturation when music and effects sum past either signed PCM limit. */
    music_init(&a,SOUND_RATE,lengths,read_pcm,NULL);
    for(unsigned i=0;i<4096;i++) { whole[i*2]=32700; whole[i*2+1]=-32700; }
    music_mix(&a,whole,4096);
    assert(whole[4094*2]==32767 && whole[4094*2+1]==-32767);
    /* A missing track remains silent without reading outside its asset. */
    uint32_t empty[MUSIC_TRACKS]={0,0};
    music_init(&a,SOUND_RATE,empty,read_pcm,NULL);
    memset(whole,0,sizeof(whole)); unsigned before=reads;
    music_mix(&a,whole,4096); assert(reads==before && !whole[0]);
    puts("PASS: stereo music, long-track looping, bounded reads, phase transitions, fractional rate, partitioned playback and clipping");
}
