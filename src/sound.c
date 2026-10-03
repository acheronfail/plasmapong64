#include "sound.h"
#include <string.h>
#include "sound_bank.inc"
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
static int suction_slot(int p) { return p<2?p:8+(p-2)*2; }
static int jet_slot(int p) { return p<2?p+2:9+(p-2)*2; }
static SoundVoice voice(const int16_t *pcm,unsigned count,int gain,int pan,bool loop) {
    return (SoundVoice){.pcm=pcm,.length=count,.gain=gain*256,.target=gain*256,
        .left=pan>0?256-pan:256,.right=pan<0?256+pan:256,.loop=loop};
}
void sound_init(Sound *s,unsigned rate) {
    memset(s,0,sizeof(*s)); s->step=(uint32_t)(((uint64_t)SOUND_RATE<<16)/(rate?rate:SOUND_RATE));
    for(int p=0;p<MAX_PLAYERS;p++) {
        s->voice[suction_slot(p)]=voice(pcm_suction,COUNT(pcm_suction),0,p>=2?0:p?100:-100,true);
        s->voice[jet_slot(p)]=voice(pcm_jet,COUNT(pcm_jet),0,p>=2?0:p?100:-100,true);
        s->voice[suction_slot(p)].loop_start=SUCTION_LOOP_START;
        if(p) s->voice[jet_slot(p)].position=317u<<16;
    }
}
static void trigger(Sound *s,const int16_t *pcm,unsigned count,int gain,int pan) {
    /* Four effect voices: sustained loops have their own slots and cannot
       steal a goal or victory cue. Reuse idle voices before replacing oldest. */
    unsigned slot=4+s->next;
    for(unsigned i=4;i<8;i++) if(!s->voice[i].pcm) { slot=i; break; }
    s->voice[slot]=voice(pcm,count,gain,pan,false); s->next=(slot-3)%4;
}
void sound_update(Sound *s,const Game *g) {
    bool active=g->phase==PLAY && (g->mode!=ARCADE || g->arcade.transition<=0);
    for(int p=0;p<MAX_PLAYERS;p++) {
        bool playing=active && (p<2 || p<(int)game_players(g)) && game_alive(g,p);
        int target=playing && g->bat[p].sucking?18*256:0;
        if(target && !s->voice[suction_slot(p)].target) s->voice[suction_slot(p)].position=0;
        s->voice[suction_slot(p)].target=target;
        s->voice[jet_slot(p)].target=playing && g->previous[p].z?16*256:0;
    }
    if(g->phase!=s->phase && (g->phase==MENU || g->phase==LOBBY || g->phase==PAUSED)) {
        for(int i=4;i<8;i++) s->voice[i].target=0;
    }
    s->phase=g->phase;
    if(g->sound_events&SOUND_SELECT) trigger(s,pcm_select,COUNT(pcm_select),85,0);
    if(g->sound_events&SOUND_BACK) trigger(s,pcm_back,COUNT(pcm_back),75,0);
    if(g->sound_events&SOUND_BREAK1) trigger(s,pcm_break,COUNT(pcm_break),110,-100);
    if(g->sound_events&SOUND_BREAK2) trigger(s,pcm_break,COUNT(pcm_break),110,100);
    if(g->sound_events&SOUND_BAT1) trigger(s,pcm_bat,COUNT(pcm_bat),110,-100);
    if(g->sound_events&SOUND_BAT2) trigger(s,pcm_bat,COUNT(pcm_bat),110,100);
    if(g->sound_events&SOUND_BAT_OTHER) trigger(s,pcm_bat,COUNT(pcm_bat),110,0);
    if(g->sound_events&SOUND_BREAK_OTHER) trigger(s,pcm_break,COUNT(pcm_break),110,0);
    if(g->sound_events&SOUND_WALL) trigger(s,pcm_wall,COUNT(pcm_wall),80,0);
    if(g->sound_events&SOUND_GOAL) trigger(s,pcm_goal,COUNT(pcm_goal),115,0);
    if(g->sound_events&SOUND_WIN) trigger(s,pcm_win,COUNT(pcm_win),135,0);
}
void sound_render(Sound *s,int16_t *stereo,size_t frames) {
    /* Mix a small cache-resident block voice-first. Inactive voices are tested
       once per block rather than once per sample. Keep integer truncation,
       gain ramps, loop positions and final saturation exactly unchanged. */
    while(frames) {
        size_t count=frames<64?frames:64;
        int mix[64][2]={{0}};
        for(int i=0;i<SOUND_VOICES;i++) {
            SoundVoice *v=&s->voice[i];
            if(!v->pcm || (!v->gain && !v->target && v->loop)) continue;
#ifdef PLASMAPONG_SOUND_STEADY
            if(v->gain>0 && v->gain==v->target) {
                /* Long-lived jets and already-started effects spend most of
                   their time at steady gain. Keep their cursor in a register
                   and omit sample-by-sample ramp tests and structure stores. */
                const int16_t *pcm=v->pcm;
                uint32_t position=v->position,step=s->step;
                unsigned length=v->length;
                uint32_t wrap=(v->length-v->loop_start)<<16;
                int gain=v->gain>>8,left=v->left,right=v->right;
                for(size_t frame=0;frame<count;frame++) {
                    int value=pcm[position>>16]*gain/256;
                    mix[frame][0]+=left==256?value:value*left/256;
                    mix[frame][1]+=right==256?value:value*right/256;
                    position+=step;
                    if((position>>16)>=length) {
                        if(v->loop) position-=wrap;
                        else { v->pcm=NULL; break; }
                    }
                }
                v->position=position;
                continue;
            }
#endif
            for(size_t frame=0;frame<count && v->pcm;frame++) {
                if(v->gain<v->target) { v->gain+=12; if(v->gain>v->target) v->gain=v->target; }
                if(v->gain>v->target) { v->gain-=12; if(v->gain<v->target) v->gain=v->target; }
                if(!v->gain && !v->target) { if(!v->loop) v->pcm=NULL; continue; }
                unsigned pos=v->position>>16;
                int value=v->pcm[pos]*(v->gain>>8)/256;
                mix[frame][0]+=value*v->left/256;
                mix[frame][1]+=value*v->right/256;
                v->position+=s->step;
                if((v->position>>16)>=v->length) {
                    if(v->loop) v->position-=(v->length-v->loop_start)<<16;
                    else v->pcm=NULL;
                }
            }
        }
        for(size_t frame=0;frame<count;frame++) {
            int left=mix[frame][0],right=mix[frame][1];
            stereo[frame*2]=(int16_t)(left<-32767?-32767:left>32767?32767:left);
            stereo[frame*2+1]=(int16_t)(right<-32767?-32767:right>32767?32767:right);
        }
        stereo+=count*2;
        frames-=count;
    }
}
