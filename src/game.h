#ifndef GAME_H
#define GAME_H
#include "fluid.h"
#include <stdbool.h>
#define STEP (1.0f/30.0f)
#define BAT_HALF 14.0f
#define BALL_RADIUS 3.0f
#define SUCTION_CHARGE_TICKS 30u
/* Round 1000ms + 150ms up to the next 30Hz simulation tick. */
#define SUCTION_BREAK_TICKS 35u
#define SUCTION_COOLDOWN_TICKS 150u
typedef struct { bool connected,a,z,start,b; float x,y; } Input;
typedef struct {
    float x,y,vx,vy,charge,burst;
    unsigned suction_ticks,cooldown_ticks;
    bool sucking,release_required;
} Bat;
typedef enum { MENU, LOBBY, PLAY, PAUSED, FINISHED, SCORES } Phase;
typedef enum { MULTIPLAYER, ARCADE } GameMode;
#define HIGH_SCORE_COUNT 10
typedef struct { uint32_t points,level; char initials[4]; } HighScore;
typedef struct {
    uint32_t level,points;
    unsigned lives,goals,ticks;
    float level_time,transition,hold_time;
    float ai_wait,ai_y,ai_speed;
    uint32_t rng;
} Arcade;
enum { SOUND_BAT1=1, SOUND_BAT2=2, SOUND_WALL=4, SOUND_GOAL=8, SOUND_WIN=16, SOUND_SELECT=32, SOUND_BACK=64,
       SOUND_BREAK1=128, SOUND_BREAK2=256 };
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
    bool save_available,save_failed,scores_dirty;
    GameMode mode;
    unsigned menu_selection,initial_cursor;
    int score_entry;
    Arcade arcade;
    HighScore highs[HIGH_SCORE_COUNT];
} Game;
void game_init(Game *g);
void game_step(Game *g,const Input in[2]);
float arcade_difficulty(const Game *g);
Input arcade_ai(Game *g);
void arcade_currents(Game *g);
void arcade_goal(Game *g,int scorer);
#endif
