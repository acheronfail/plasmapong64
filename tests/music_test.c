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
        Game g={.phase=phase};
        music_update(&a,&g); music_update(&b,&g);
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
    /* Single-player advancement restarts once at the new speed, including
       after reaching the cap. Pauses and level cards preserve the cursor. */
    music_init(&a,15926,lengths,read_pcm,NULL);
    Game g={.phase=PLAY,.mode=ARCADE,.arcade={.level=1}};
    music_update(&a,&g); memset(whole,0,sizeof(whole)); music_mix(&a,whole,4096);
    assert(a.step==a.base_step && !a.restart);
    for(uint32_t level=2;level<=15;level++) {
        g.arcade.level=level; g.arcade.transition=1.5f;
        music_update(&a,&g); assert(a.restart);
        unsigned bonus=level<=11?(level-1)*3:30;
        assert(a.target_step==(uint64_t)a.base_step*(100+bonus)/100);
        memset(whole,0,sizeof(whole)); music_mix(&a,whole,256);
        assert(!a.restart && a.step==a.target_step && a.position<200);
        uint32_t position=a.position,fraction=a.fraction;
        music_update(&a,&g); assert(!a.restart && a.position==position && a.fraction==fraction);
        g.phase=PAUSED; music_update(&a,&g); assert(!a.restart);
        g.phase=PLAY; music_update(&a,&g); assert(!a.restart);
        memset(whole,0,sizeof(whole)); music_mix(&a,whole,4096);
    }
    g.arcade.level=UINT32_MAX; music_update(&a,&g);
    assert(a.target_step==(uint64_t)a.base_step*130/100);
    music_mix(&a,whole,256); assert(!a.restart);
    g.arcade.level=1; music_update(&a,&g); music_mix(&a,whole,256);
    assert(a.step==a.base_step && a.position<200); /* Fresh run. */
    g.mode=MULTIPLAYER; music_update(&a,&g); music_mix(&a,whole,256);
    assert(a.step==a.base_step); /* Arcade level never speeds up multiplayer. */
    g.phase=MENU; music_update(&a,&g); music_mix(&a,whole,256);
    assert(a.track==MUSIC_MENU && a.step==a.base_step);
    puts("PASS: level restarts, +3% tempo/pitch per level, 130% cap, pause continuity and new-run/menu/multiplayer reset");
    for(unsigned players=3;players<=4;players++) {
        music_init(&a,SOUND_RATE,lengths,read_pcm,NULL);
        g=(Game){.phase=PLAY,.mode=MULTIPLAYER,.players=players,.lives={3,3,3,3}};
        music_update(&a,&g); music_mix(&a,whole,4096);
        assert(a.step==a.base_step && a.eliminations==0);
        g.lives[0]=1; g.connected[0]=false; music_update(&a,&g);
        assert(!a.restart && a.step==a.base_step); /* Life loss/disconnect isn't elimination. */
        for(unsigned eliminated=1;eliminated<=players-2;eliminated++) {
            g.lives[eliminated-1]=0; music_update(&a,&g);
            assert(a.restart && a.eliminations==eliminated);
            assert(a.target_step==(uint64_t)a.base_step*(100+15*eliminated)/100);
            memset(whole,0,sizeof(whole)); music_mix(&a,whole,256);
            assert(!a.restart && a.position<200 && a.step==a.target_step);
            uint32_t position=a.position,fraction=a.fraction;
            music_update(&a,&g);
            assert(!a.restart && a.position==position && a.fraction==fraction);
            g.phase=PAUSED; music_update(&a,&g); assert(!a.restart);
            g.phase=PLAY; music_update(&a,&g); assert(!a.restart);
            music_mix(&a,whole,4096);
        }
        g.lives[players-2]=0; g.phase=FINISHED; music_update(&a,&g);
        assert(!a.restart && a.eliminations==players-2); /* Final goal keeps current song. */
        uint32_t final_step=a.step;
        music_mix(&a,whole,256); assert(a.step==final_step);
        g.phase=PLAY;
        for(unsigned p=0;p<players;p++) g.lives[p]=3;
        music_update(&a,&g); assert(a.restart);
        music_mix(&a,whole,256); assert(a.step==a.base_step && a.eliminations==0);
    }
    g.players=2; g.lives[0]=0; music_update(&a,&g);
    assert(!a.restart && a.target_step==a.base_step); /* Ordinary two-player remains unchanged. */
    puts("PASS: 3P/4P elimination restarts, +15% steps, 130% cap, life-loss/disconnect exclusion, final goal and rematch");
    puts("PASS: stereo music, long-track looping, bounded reads, phase transitions, fractional rate, partitioned playback and clipping");
}
