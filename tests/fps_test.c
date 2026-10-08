#include "game.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static Game g;
static Input in[MAX_PLAYERS];
static void ready(FrameRate fps) {
    game_init(&g); g.frame_rate=fps; g.phase=PLAY; g.serve=100;
    memset(in,0,sizeof(in)); in[0].connected=in[1].connected=true;
    g.connected[0]=g.connected[1]=true;
}
int main(void) {
    for(int fps=30;fps<=60;fps+=30) {
        ready((FrameRate)fps);
        assert(game_tick_units(&g)==1 && game_dt(&g)==STEP && game_emission(&g)==.5f);
        const int hz=GAME_HZ;
        float x=g.bat[0].x; in[0].x=1; game_step(&g,in);
        assert(fabsf(g.bat[0].x-x-210.0f/hz)<.001f);
        ready((FrameRate)fps); g.held=0; in[0].a=true;
        for(int i=0;i<hz;i++) game_step(&g,in);
        assert(g.bat[0].charge==1 && g.bat[0].suction_ticks==60);
        assert(fabsf(g.elapsed-1)<.00001f);
        for(int i=1;i<hz/15;i++) game_step(&g,in);
        assert(g.held==0);
        game_step(&g,in); assert(g.held==-1 && g.bat[0].cooldown_ticks==300);
        g.phase=PAUSED; game_step(&g,in); assert(g.bat[0].cooldown_ticks==300);
        g.phase=PLAY;
        for(int i=0;i<5*hz;i++) game_step(&g,in);
        assert(!g.bat[0].cooldown_ticks && g.bat[0].release_required);
        ready((FrameRate)fps); g.flow_effect=FLOW_TAILS;
        for(int i=0;i<FN;i++) fluid_velocity(&g.fluid)->u[i]=fluid_flow_encode(30);
        for(int j=0;j<FLOW_HISTORY;j++) g.tracer_history[j][0].x=g.tracer_history[j][0].y=100;
        g.tracers[0].life=1000;
        for(int i=0;i<hz*4/15;i++) game_flow_step(&g);
        assert(fabsf(g.tracer_history[g.tracer_head][0].x-108)<.001f);
        assert(g.tracer_history[(g.tracer_head+FLOW_HISTORY-1)%FLOW_HISTORY][0].x==100);
        assert(g.tracers[0].life==984);
    }
    ready(FPS_60); g.phase=OPTIONS;
    assert(!g.fps_meter);
    in[0].y=-1; game_step(&g,in);
    assert(g.options_selection==1 && !g.scores_dirty);
    in[0].y=0; in[0].x=1; game_step(&g,in);
    assert(g.frame_rate==FPS_30 && g.scores_dirty);
    g.scores_dirty=false; game_step(&g,in);
    assert(g.frame_rate==FPS_30 && !g.scores_dirty);
    in[0].x=0; game_step(&g,in); in[0].x=-1; game_step(&g,in);
    assert(g.frame_rate==FPS_60 && g.scores_dirty);
    in[0].x=0; game_step(&g,in); in[0].x=1; game_step(&g,in);
    in[0].x=0; in[0].y=-1; game_step(&g,in);
    assert(g.options_selection==OPTION_FPS_METER);
    in[0].y=0; in[0].x=1; game_step(&g,in);
    assert(g.fps_meter && g.scores_dirty);
    g.scores_dirty=false; game_step(&g,in);
    assert(g.fps_meter && !g.scores_dirty);
    in[0].x=0; game_step(&g,in); in[0].x=-1; game_step(&g,in);
    assert(!g.fps_meter && g.scores_dirty);
    in[0].x=0; game_step(&g,in); in[0].x=1; game_step(&g,in);
    assert(g.fps_meter);
    in[0].x=0; in[0].y=-1; game_step(&g,in);
    assert(g.options_selection==OPTION_CLEAR_SAVE);
    in[0].y=0; game_step(&g,in); in[0].y=-1; game_step(&g,in);
    assert(g.options_selection==OPTION_FLOW);
    in[0].y=0; game_step(&g,in); in[0].y=1; game_step(&g,in);
    assert(g.options_selection==OPTION_CLEAR_SAVE);
    in[0].y=0;
    in[0].x=0; in[0].b=true; game_step(&g,in);
    assert(g.phase==MENU && g.frame_rate==FPS_30);
    g.highs[0]=(HighScore){.points=42,.level=1,.initials="ACE"};
    in[0].b=false; in[0].start=true; game_step(&g,in); assert(g.phase==LOBBY);
    in[0].start=false; game_step(&g,in); in[0].start=true; game_step(&g,in);
    assert(g.phase==PLAY && g.frame_rate==FPS_30 && g.fps_meter && g.highs[0].points==42);
    g.phase=FINISHED; in[0].start=false; game_step(&g,in); in[0].start=true; game_step(&g,in);
    assert(g.phase==PLAY && g.frame_rate==FPS_30 && g.fps_meter && g.highs[0].points==42);
    puts("PASS: low/high resolution timing, held-input debounce, restart retention, movement, charge/grace/cooldown seconds and tracer history");
}
