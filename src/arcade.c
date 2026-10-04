#include "game.h"
#include "mathutil.h"
#include <math.h>
#include <limits.h>
#include <string.h>

float arcade_difficulty(const Game *g) {
    /* Diminishing increases; retain finite movement and collision speeds. */
    float n=(float)(g->arcade.level-1);
    return n/(n+12);
}
static float random_signed(Arcade *a) {
    uint32_t x=a->rng?a->rng:1;
    x^=x<<13; x^=x>>17; x^=x<<5; a->rng=x;
    return (x&65535)/32767.5f-1;
}
Input arcade_ai(Game *g) {
    Arcade *a=&g->arcade;
    float d=arcade_difficulty(g);
    a->ai_wait-=game_dt(g);
    if(a->ai_wait<=0) {
        /* Observe only at reaction intervals, then commit to that target.
           No extra fluid solve or perfect prediction of future currents. */
        a->ai_wait=.40f-.32f*d;
        float target=ARENA_H*.5f;
        if(g->bvx>0 && g->held<0) {
            float flight=clampf((g->bat[1].x-6-g->bx)/maxf(45,g->bvx),0,1.5f);
            target=g->by+g->bvy*flight*(.25f+.65f*d);
            float span=ARENA_H-2*BALL_RADIUS;
            target=fmodf(target-BALL_RADIUS,2*span);
            if(target<0) target+=2*span;
            if(target>span) target=2*span-target;
            target+=BALL_RADIUS;
        }
        a->ai_y=clampf(target+random_signed(a)*(25-20*d),BAT_HALF+2,ARENA_H-BAT_HALF-2);
        a->ai_speed=.50f+.48f*d;
    }
    Input in={.connected=true};
    float dy=a->ai_y-g->bat[1].y;
    in.y=fabsf(dy)<3?0:clampf(-dy/18,-a->ai_speed,a->ai_speed);
    /* Powers share exactly the human controls and catch rules. */
    in.z=a->level>=2 && g->serve<=0 && g->held<0 &&
        g->bx>ARENA_W*.55f && fabsf(g->by-g->bat[1].y)<32;
    in.a=a->level>=4 && g->serve<=0 && !g->bat[1].cooldown_ticks && !g->bat[1].release_required &&
        ((g->held==1 && a->hold_time<.65f) ||
         (g->held<0 && g->bx>g->bat[1].x-32 && fabsf(g->by-g->bat[1].y)<25));
    return in;
}
static void add_points(Arcade *a,uint64_t amount) {
    uint64_t sum=(uint64_t)a->points+amount;
    a->points=sum>UINT32_MAX?UINT32_MAX:(uint32_t)sum;
}
void arcade_currents(Game *g) {
    if(g->arcade.level<6) return;
    unsigned ticks=g->arcade.ticks; g->arcade.ticks+=game_tick_units(g);
    if(ticks%(GAME_HZ/10)>=game_tick_units(g)) return;
    float force=35*arcade_difficulty(g),wave=sinf(g->elapsed*.8f);
    switch((g->arcade.level-6)%3) {
    case 0:
        fluid_splat(&g->fluid,ARENA_W*.5f,ARENA_H*.5f,24,force*wave,0,.018f,game_player_palette(g,0));
        break;
    case 1:
        fluid_splat(&g->fluid,ARENA_W*.5f,ARENA_H*.7f,24,0,-force,.018f,game_player_palette(g,1));
        break;
    default:
        fluid_splat(&g->fluid,ARENA_W*.4f,ARENA_H*.35f,20,force,force*.4f,.012f,game_player_palette(g,0));
        fluid_splat(&g->fluid,ARENA_W*.6f,ARENA_H*.65f,20,-force,-force*.4f,.012f,game_player_palette(g,1));
        break;
    }
}
void arcade_goal(Game *g,int scorer) {
    Arcade *a=&g->arcade;
    if(scorer==0) {
        add_points(a,(uint64_t)100*a->level);
        if(++a->goals==3) {
            add_points(a,(uint64_t)500*a->level+(unsigned)(maxf(0,60-a->level_time)*10));
            if(a->level%5==0 && a->lives<5) a->lives++;
            if(a->level<UINT32_MAX) a->level++;
            a->goals=0; a->level_time=0; a->transition=1.5f;
            a->ai_wait=0;
            g->sound_events|=SOUND_WIN;
        }
        return;
    }
    if(--a->lives) return;
    g->phase=FINISHED; g->winner=1; g->score_entry=-1;
    /* Stable ties: an equal run does not displace an earlier score. */
    for(int i=0;i<HIGH_SCORE_COUNT;i++) {
        if(a->points>g->highs[i].points ||
           (a->points==g->highs[i].points && a->level>g->highs[i].level)) {
            memmove(&g->highs[i+1],&g->highs[i],(HIGH_SCORE_COUNT-i-1)*sizeof(HighScore));
            g->highs[i]=(HighScore){.points=a->points,.level=a->level,.initials="AAA"};
            g->score_entry=i; g->initial_cursor=0; break;
        }
    }
}
