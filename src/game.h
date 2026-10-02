#ifndef GAME_H
#define GAME_H
#include "fluid.h"
#include <stdbool.h>
#define STEP (1.0f/30.0f)
#define BAT_HALF 14.0f
#define BALL_RADIUS 3.0f
typedef struct { bool connected,a,z,start; float x,y; } Input;
typedef struct { float x,y,vx,vy,charge,burst; bool sucking; } Bat;
typedef enum { LOBBY, PLAY, PAUSED, FINISHED } Phase;
typedef struct {
    Fluid fluid;
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
