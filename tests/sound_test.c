#include "sound.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sound_reference.h"
static Sound s;
static Game g;
static int16_t pcm[SOUND_RATE*2];
static long long energy(int channel) {
    long long sum=0; for(int i=0;i<SOUND_RATE;i++) { long long v=pcm[2*i+channel]; sum+=v*v; }
    return sum;
}
static int peak(void) {
    int p=0; for(int i=0;i<SOUND_RATE*2;i++) if(abs(pcm[i])>p) p=abs(pcm[i]); return p;
}
static void second(void) { sound_render(&s,pcm,SOUND_RATE); }
static void le16(FILE *f,unsigned v) { fputc(v&255,f); fputc((v>>8)&255,f); }
static void le32(FILE *f,unsigned v) { le16(f,v); le16(f,v>>16); }
static void preview(void) {
    FILE *f=fopen("build/sound-demo.wav","wb"); assert(f);
    unsigned seconds=18,bytes=seconds*SOUND_RATE*4;
    fwrite("RIFF",1,4,f); le32(f,bytes+36); fwrite("WAVEfmt ",1,8,f); le32(f,16);
    le16(f,1); le16(f,2); le32(f,SOUND_RATE); le32(f,SOUND_RATE*4); le16(f,4); le16(f,16);
    fwrite("data",1,4,f); le32(f,bytes);
    sound_init(&s,SOUND_RATE); g=(Game){.phase=PLAY};
    for(unsigned n=0;n<seconds;n++) {
        g.sound_events=n==0?SOUND_BAT1:n==1?SOUND_BAT2:n==2?SOUND_WALL:n==3?SOUND_GOAL:n==12?SOUND_WIN:n==14?SOUND_SELECT:n==15?SOUND_BACK:n==16?SOUND_BREAK1:n==17?SOUND_BREAK2:0;
        g.bat[0].sucking=n>=4 && n<8;
        g.previous[1].z=n==9 || n==10;
        sound_update(&s,&g); second();
        for(int i=0;i<SOUND_RATE*2;i++) le16(f,(uint16_t)pcm[i]);
    }
    fclose(f);
}
static void exact_mixer(void) {
    Sound a,b;
    Game game={.phase=PLAY,.players=4,.lives={3,3,3,3}};
    int16_t out[1024*2],reference[1024*2];
    uint32_t rng=91;
    sound_init(&a,16001); b=a;
    for(unsigned n=0;n<1200;n++) {
        rng=rng*1664525u+1013904223u;
        game.phase=n%97<80?PLAY:PAUSED;
        game.sound_events=rng&0x7ff;
        for(unsigned p=0;p<MAX_PLAYERS;p++) {
            game.bat[p].sucking=(rng>>(p+12))&1;
            game.previous[p].z=(rng>>(p+16))&1;
        }
        sound_update(&a,&game); sound_update(&b,&game);
        unsigned count=n%1025;
        sound_render(&a,out,count); sound_render_reference(&b,reference,count);
        assert(!memcmp(out,reference,count*2*sizeof(*out)));
        assert(!memcmp(&a,&b,sizeof(a)));
    }
    puts("PASS: blocked mixer matches original PCM and voice state over 1200 variable-size callbacks");
}
int main(void) {
    exact_mixer();
    sound_init(&s,SOUND_RATE); second(); assert(energy(0)==0 && energy(1)==0);
    g=(Game){.phase=PLAY,.sound_events=SOUND_BAT1}; sound_update(&s,&g); second();
    int hit_peak=peak(); long long hit=energy(0); assert(hit>0 && hit>energy(1)*2);
    second(); assert(energy(0)==0); /* Finite one-shot; never retrigger in callback. */
    const unsigned events[]={SOUND_BAT2,SOUND_WALL,SOUND_GOAL,SOUND_WIN,SOUND_SELECT,SOUND_BACK};
    for(unsigned i=0;i<6;i++) {
        sound_init(&s,SOUND_RATE); g.sound_events=events[i]; sound_update(&s,&g); second();
        assert(energy(0)>0 && energy(1)>0);
    }
    for(int p=0;p<2;p++) {
        sound_init(&s,SOUND_RATE); g=(Game){.phase=PLAY,.sound_events=p?SOUND_BREAK2:SOUND_BREAK1};
        sound_update(&s,&g); second();
        assert(energy(p)>2*energy(1-p) && energy(1-p)>0 && peak()<32767);
        g.sound_events=0; sound_update(&s,&g); second();
        assert(energy(0)==0 && energy(1)==0);
    }
    for(int type=0;type<2;type++) {
        sound_init(&s,SOUND_RATE); g=(Game){.phase=PLAY};
        g.bat[0].sucking=type==0; g.previous[0].z=type==1;
        sound_update(&s,&g);
        for(int n=0;n<4;n++) {
            second(); assert(energy(0)>0 && peak()<hit_peak/4); /* Quiet, lasts across wraps. */
        }
        g.phase=PAUSED; sound_update(&s,&g); second(); second(); assert(energy(0)==0);
    }
    sound_init(&s,SOUND_RATE); g=(Game){.phase=PLAY}; g.bat[0].sucking=true;
    sound_update(&s,&g); second(); long long attack=energy(0);
    second(); second(); long long sustain=energy(0);
    assert(sustain>0 && sustain<attack);
    assert((s.voice[0].position>>16)>=s.voice[0].loop_start);
    uint32_t pos=s.voice[0].position; sound_update(&s,&g);
    assert(s.voice[0].position==pos); /* Holding never restarts the attack. */
    g.bat[0].sucking=false; sound_update(&s,&g); second();
    g.bat[0].sucking=true; sound_update(&s,&g); assert(s.voice[0].position==0);
    sound_init(&s,SOUND_RATE); g=(Game){.phase=LOBBY,.sound_events=SOUND_SELECT};
    sound_update(&s,&g); sound_render(&s,pcm,320);
    g.sound_events=0; sound_update(&s,&g);
    assert(s.voice[4].target>0); /* Subsequent menu ticks must not truncate it. */
    /* Rapid toggling and all effects/loops at once remain bounded. */
    sound_init(&s,16001); g=(Game){.phase=PLAY,.sound_events=31};
    for(int p=0;p<2;p++) { g.bat[p].sucking=true; g.previous[p].z=true; }
    sound_update(&s,&g); second();
    int clipped=0; for(int i=0;i<SOUND_RATE*2;i++) if(abs(pcm[i])>=32767) clipped++;
    assert(clipped==0);
    g.phase=MENU; g.sound_events=0; sound_update(&s,&g); second(); second();
    assert(energy(0)==0 && energy(1)==0);
    g=(Game){.phase=PLAY,.mode=ARCADE}; g.arcade.transition=1;
    g.bat[0].sucking=true; g.previous[1].z=true;
    sound_update(&s,&g); second(); second();
    assert(energy(0)==0 && energy(1)==0); /* No power loops during level cards. */
    /* Top/bottom powers have centered loops and stop on elimination. */
    for(int p=0;p<MAX_PLAYERS;p++) {
        sound_init(&s,SOUND_RATE); g=(Game){.phase=PLAY,.players=4,.lives={3,3,3,3}};
        g.bat[p].sucking=true; g.previous[p].z=true;
        sound_update(&s,&g); second(); assert(energy(0)>0 && energy(1)>0);
        if(p>=2) assert(energy(0)==energy(1));
        g.lives[p]=0; sound_update(&s,&g); second(); second();
        assert(!energy(0) && !energy(1));
    }
    preview();
    puts("PASS: sampled effects, stereo panning, quiet sustained loops, fade-out, headroom; build/sound-demo.wav");
}
