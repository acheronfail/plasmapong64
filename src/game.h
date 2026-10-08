#ifndef GAME_H
#define GAME_H
#include "fluid.h"
#include <stdbool.h>
#define MAX_PLAYERS 4
#define MENU_FIRST_ROW 112
#define MENU_ROW_SPACING 24
#define MULTIPLAYER_LIVES 3
#define CORNER_SIZE 29.0f
/* Keep adjacent paddle outlines separated as the corner size changes. */
#define SQUARE_BAT_DEPTH (CORNER_SIZE-8.0f)
#define GAME_HZ 60
#define STEP (1.0f/GAME_HZ)
/* Tick counters use 60 Hz units in both selectable update modes. */
#define BAT_HALF 14.0f
#define BALL_RADIUS 3.0f
#define RUMBLE_HIT_TICKS 4u
#define RUMBLE_BURST_TICKS 15u
#define BALL_HOT_SPEED 240.0f
#define SUCTION_CHARGE_TICKS (1u*GAME_HZ)
/* About 67ms to release at full charge before breaking. */
#define SUCTION_BREAK_TICKS (SUCTION_CHARGE_TICKS+GAME_HZ/15u)
#define SUCTION_COOLDOWN_TICKS (5u*GAME_HZ)
typedef struct { bool connected,a,z,start,b; float x,y; uint16_t other_buttons; } Input;
typedef struct {
    float x,y,vx,vy,charge,burst;
    unsigned suction_ticks,cooldown_ticks;
    bool sucking,release_required;
} Bat;
typedef enum { MENU, LOBBY, PLAY, PAUSED, FINISHED, SCORES, OPTIONS } Phase;
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
       SOUND_BREAK1=128, SOUND_BREAK2=256, SOUND_BAT_OTHER=512, SOUND_BREAK_OTHER=1024 };
typedef enum { FPS_30=30, FPS_60=60 } FrameRate;
typedef enum { FLOW_NONE, FLOW_PARTICLES, FLOW_TAILS, FLOW_SPEED, FLOW_VORTEX, FLOW_BANDS, FLOW_PRESSURE, FLOW_COUNT } FlowEffect;
static inline FluidView flow_view(FlowEffect effect) {
    return effect>=FLOW_VORTEX?(FluidView)(effect-FLOW_VORTEX+FLUID_VIEW_VORTEX):
        effect==FLOW_SPEED?FLUID_VIEW_SPEED:FLUID_VIEW_DYE;
}
typedef enum { OPTION_FLOW, OPTION_RESOLUTION, OPTION_FPS_METER, OPTION_CLEAR_SAVE, OPTION_COUNT } GameOption;
#define FLOW_TRACERS 96
#define FLOW_SAMPLE_TICKS (GAME_HZ/15)
#define FLOW_HISTORY (4*FLOW_SAMPLE_TICKS+1)
/* Circular history preserves the 267ms trail without copying every sample. */
/* Paired coordinates keep both components in one cache line. History is
   time-major so simulation and rendering stream adjacent tracers. */
typedef struct { float x,y; } FlowPosition;
typedef struct { unsigned life; } FlowTracer;
typedef struct { float x,y,u,v; } MenuCurrent;
typedef struct {
    Fluid fluid;
    FlowEffect flow_effect;
    FlowTracer tracers[FLOW_TRACERS];
    _Alignas(16) FlowPosition tracer_history[FLOW_HISTORY][FLOW_TRACERS];
    uint32_t tracer_rng;
    unsigned tracer_head;
    MenuCurrent menu_current[3];
    float menu_label_widths[4]; /* Screen pixels, measured by the UI. */
    uint32_t menu_rng;
    unsigned menu_ticks, sound_events;
    unsigned rumble_ticks[MAX_PLAYERS];
    Bat bat[MAX_PLAYERS];
    Input previous[MAX_PLAYERS];
    float bx,by,bvx,bvy,serve,elapsed;
    unsigned score[MAX_PLAYERS],lives[MAX_PLAYERS];
    unsigned players,player_port[MAX_PLAYERS],player_side[MAX_PLAYERS];
    bool final_duel;
    int held,winner;
    Phase phase;
    bool connected[MAX_PLAYERS];
    bool save_available,save_failed,scores_dirty;
    GameMode mode;
    unsigned menu_selection,initial_cursor;
    int score_entry;
    Arcade arcade;
    HighScore highs[HIGH_SCORE_COUNT];
    /* Saved video mode: FPS_60 = 320x240 low res, FPS_30 = 640x480 high res.
       Both modes now update at GAME_HZ. Retain the legacy field and serialized
       values for compatibility with existing EEPROMs; these select resolution. */
    FrameRate frame_rate;
    unsigned options_selection;
    bool clear_save_dialog;
    bool fps_meter; /* Saved permission for the overlay and port-1 L/R controls. */
} Game;
/* Share the level palette between paddles, goals, labels and new fluid emissions. */
static inline unsigned game_player_palette(const Game *g,unsigned p) {
    static const unsigned pairs[4][2]={{0,1},{2,3},{1,0},{3,2}};
    if(g->mode!=ARCADE || p>=2) return p;
    unsigned level=g->arcade.level?g->arcade.level-1:0;
    return pairs[level%4][p];
}
static inline float game_bat_half(const Game *g,unsigned p) {
    if(g->mode!=ARCADE || p!=0 || g->arcade.level<=1) return BAT_HALF;
    /* Lose one pixel of total height per level, stopping at 16 pixels. */
    unsigned shrink=g->arcade.level-1;
    if(shrink>12) shrink=12;
    return BAT_HALF-.5f*shrink;
}
static inline unsigned game_tick_units(const Game *g) { (void)g; return 1u; }
static inline float game_dt(const Game *g) { (void)g; return STEP; }
static inline float game_emission(const Game *g) { (void)g; return .5f; }
static inline unsigned game_players(const Game *g) { return g->mode==ARCADE?2:g->players; }
static inline bool game_elimination(const Game *g) { return game_players(g)>2; }
static inline bool game_square(const Game *g) { return game_elimination(g) && !g->final_duel; }
static inline unsigned game_side(const Game *g,unsigned p) { return g->player_side[p]; }
static inline float game_left(const Game *g) { return game_square(g)?(ARENA_W-ARENA_H)*.5f:0; }
static inline float game_right(const Game *g) { return ARENA_W-game_left(g); }
static inline bool game_alive(const Game *g,unsigned p) { return !game_elimination(g) || g->lives[p]>0; }
static inline bool game_ready(const Game *g) {
    for(unsigned p=0;p<(g->mode==ARCADE?1:g->players);p++)
        if((g->phase==FINISHED || game_alive(g,p)) && !g->connected[p]) return false;
    return true;
}
static inline bool game_ball_hot(const Game *g) {
    return g->serve<=0 && g->held<0 &&
        g->bvx*g->bvx+g->bvy*g->bvy>BALL_HOT_SPEED*BALL_HOT_SPEED;
}
void game_init(Game *g);
void game_flow_step(Game *g);
void game_step(Game *g,const Input in[MAX_PLAYERS]);
float arcade_difficulty(const Game *g);
Input arcade_ai(Game *g);
void arcade_currents(Game *g);
void arcade_goal(Game *g,int scorer);
#endif
