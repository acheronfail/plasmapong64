#ifndef GAME_H
#define GAME_H
#include "fluid.h"
#include <stdbool.h>
#define STEP (1.0f/30.0f)
#define BAT_HALF 14.0f
#define BALL_RADIUS 3.0f
typedef struct { bool connected,a,z,start,b; float x,y; } Input;
typedef struct { float x,y,vx,vy,charge,burst; bool sucking; } Bat;
typedef enum { MENU, LOBBY, PLAY, PAUSED, FINISHED } Phase;
enum { SOUND_BAT1=1, SOUND_BAT2=2, SOUND_WALL=4, SOUND_GOAL=8, SOUND_WIN=16 };
typedef struct { float x,y,u,v; } MenuCurrent;
typedef struct {
    Fluid fluid;
    MenuCurrent menu_current[3];
    uint32_t menu_rng;
    unsigned menu_ticks, sound_events;
    Bat bat[2];
    Input previous[2];
    float bx,by,bvx,bvy,serve,elapsed;
    unsigned score[2],rally;
    int held,serve_dir,winner;
    Phase phase;
    bool connected[2];
} Game;
void game_init(Game *g);
void game_step(Game *g,const Input in[2]);
#endif
