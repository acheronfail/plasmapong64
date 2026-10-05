#include "game.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
static Game g;
static Input in[MAX_PLAYERS];
static void tick(void) { game_step(&g,in); }
static void ready(void) {
    game_init(&g); memset(in,0,sizeof(in)); in[0].connected=true;
    in[0].y=-1; tick(); assert(g.menu_selection==1);
    tick(); assert(g.menu_selection==1); /* No held-stick menu repeat. */
    in[0].y=0; in[0].a=true; tick(); assert(g.phase==LOBBY && g.mode==ARCADE);
    in[0].a=false; in[0].start=true; tick();
    assert(g.phase==PLAY && g.arcade.lives==3 && g.arcade.level==1);
    in[0].start=false; g.serve=0;
}
static void goal(int scorer) {
    fluid_init(&g.fluid); g.arcade.transition=0; g.serve=0; g.held=-1;
    g.bx=scorer==0?ARENA_W+4:-4; g.by=10;
    g.bvx=scorer==0?120:-120; g.bvy=0;
    tick();
}
static float current_ball_speed(GameMode mode,unsigned level,float flow,float speed) {
    ready(); g.mode=mode; g.arcade.level=level;
    /* Isolate ball response from AI movement and ambient current emissions. */
    in[1].connected=true; g.connected[1]=true;
    g.arcade.ai_wait=10; g.arcade.ai_y=g.bat[1].y; g.arcade.ticks=1;
    g.bvx=speed; g.bvy=0;
    for(int i=0;i<FN;i++) {
        fluid_velocity(&g.fluid)->u[i]=fluid_flow_encode(flow);
        fluid_velocity(&g.fluid)->v[i]=0;
    }
    tick();
    return g.bvx;
}
static float jet_current(GameMode mode,unsigned level,unsigned player) {
    ready(); g.mode=mode; g.arcade.level=level;
    in[1].connected=true; g.connected[1]=true; in[player].z=true;
    g.arcade.ai_wait=10; g.arcade.ai_y=g.bat[1].y; g.arcade.ticks=1;
    /* Trigger the AI jet away from suction range, or keep it off for P1. */
    g.bx=player==1?ARENA_W*.65f:ARENA_W*.5f;
    float u,v;
    tick();
    fluid_sample(&g.fluid,g.bat[player].x+(player?-14:14),g.bat[player].y,&u,&v);
    return player?-u:u;
}
static void paddle_ball(unsigned level,float offset,bool suction) {
    ready(); g.arcade.level=level; g.arcade.ai_wait=10;
    g.arcade.ai_y=g.bat[1].y; g.arcade.ticks=1;
    g.bx=g.bat[0].x+(suction?8:6.2f); g.by=g.bat[0].y+offset;
    g.bvx=suction?0:-100; g.bvy=0; in[0].a=suction;
    tick();
}
int main(void) {
    ready();
    assert(game_bat_half(&g,0)==14);
    for(unsigned level=2;level<=13;level++) {
        g.arcade.level=level;
        assert(game_bat_half(&g,0)==14-.5f*(level-1));
        assert(game_bat_half(&g,1)==14);
    }
    g.arcade.level=UINT32_MAX; assert(game_bat_half(&g,0)==8);
    g.mode=MULTIPLAYER;
    for(unsigned p=0;p<MAX_PLAYERS;p++) assert(game_bat_half(&g,p)==14);
    paddle_ball(1,12,false); assert(g.bvx>0); /* Opening paddle returns it. */
    paddle_ball(13,12,false); assert(g.bvx<0); /* Smaller paddle misses it. */
    paddle_ball(13,9,false); assert(g.bvx>0); /* Ball radius still overlaps. */
    paddle_ball(1,14,true); assert(g.held==0);
    paddle_ball(13,14,true); assert(g.held<0);
    paddle_ball(13,0,true); assert(g.held==0);
    ready(); g.arcade.level=13; g.bat[0].y=0;
    tick(); assert(g.bat[0].y==10);
    g.bat[0].y=ARENA_H; tick(); assert(g.bat[0].y==ARENA_H-10);

    float human_jet=jet_current(ARCADE,1,0);
    assert(human_jet>0);
    assert(fabsf(jet_current(ARCADE,UINT32_MAX,0)-human_jet)<.001f);
    float multiplayer_jet=jet_current(MULTIPLAYER,1,1);
    assert(multiplayer_jet>0);
    assert(fabsf(jet_current(MULTIPLAYER,UINT32_MAX,1)-multiplayer_jet)<.001f);
    assert(fabsf(jet_current(ARCADE,1,1))<.001f); /* Not unlocked yet. */
    float previous_jet=multiplayer_jet;
    const unsigned jet_levels[]={2,5,13,25,101,UINT32_MAX};
    for(unsigned i=0;i<sizeof(jet_levels)/sizeof(jet_levels[0]);i++) {
        float jet=jet_current(ARCADE,jet_levels[i],1);
        assert(jet>previous_jet && jet<=multiplayer_jet*2.01f);
        previous_jet=jet;
    }

    float opening=current_ball_speed(ARCADE,1,20,0);
    assert(opening>0);
    assert(fabsf(current_ball_speed(MULTIPLAYER,101,20,0)-opening)<.001f);
    float previous=opening;
    const unsigned response_levels[]={5,13,25,101,UINT32_MAX};
    for(unsigned i=0;i<sizeof(response_levels)/sizeof(response_levels[0]);i++) {
        float speed=current_ball_speed(ARCADE,response_levels[i],20,0);
        assert(speed>previous && speed<=opening*3.01f);
        previous=speed;
    }
    float drag=current_ball_speed(ARCADE,1,0,100);
    assert(drag<100 && fabsf(current_ball_speed(ARCADE,101,0,100)-drag)<.001f);
    assert(current_ball_speed(ARCADE,UINT32_MAX,500,289)<=290.001f);

    ready(); tick(); assert(g.phase==PLAY && !g.connected[1]);
    in[1]=(Input){.connected=true,.start=true,.b=true}; tick();
    assert(g.phase==PLAY); /* Controller 2 cannot interfere with a solo run. */
    in[1]=(Input){0};
    float y=g.bat[1].y;
    g.bx=180; g.by=25; g.bvx=120; g.bvy=0; g.arcade.ai_wait=0;
    for(int t=0;t<8;t++) tick();
    assert(g.bat[1].y<y);
    assert(fabsf(g.bat[1].vy)<=140);
    g.arcade.ai_wait=.3f; float target=g.arcade.ai_y;
    g.by=160; (void)arcade_ai(&g); assert(g.arcade.ai_y==target);

    g.arcade.level=4; g.held=1; g.arcade.hold_time=.8f;
    assert(!arcade_ai(&g).a); /* CPU releases a caught ball instead of camping. */
    g.held=-1; g.bx=g.bat[1].x-12; g.by=g.bat[1].y; g.serve=0;
    assert(arcade_ai(&g).a && arcade_ai(&g).z);
    g.arcade.level=1; assert(!arcade_ai(&g).a && !arcade_ai(&g).z);
    g.arcade.level=100; assert(arcade_difficulty(&g)>0.8f && arcade_difficulty(&g)<1);
    g.arcade.level=1;

    float elapsed=g.arcade.level_time;
    in[0].connected=false; tick(); assert(g.phase==PAUSED);
    for(int i=0;i<10;i++) tick();
    assert(g.arcade.level_time==elapsed);
    in[0].connected=true; tick(); assert(g.phase==PAUSED);
    in[0].start=true; tick(); assert(g.phase==PLAY); in[0].start=false;

    ready(); g.held=0; in[0].a=true;
    for(int t=0;t<65;t++) tick();
    assert(g.held!=0 && g.bat[0].release_required && g.bat[0].cooldown_ticks && !g.bat[0].sucking);
    assert(g.arcade.points==0); /* No holding or survival score farming. */
    in[0].a=false; tick(); assert(!g.bat[0].release_required && g.bat[0].cooldown_ticks);
    unsigned cooldown=g.bat[0].cooldown_ticks;
    g.arcade.transition=1; tick(); assert(g.bat[0].cooldown_ticks==cooldown);

    ready(); g.arcade.level=4; g.serve=0; g.held=1;
    g.bat[1].suction_ticks=SUCTION_BREAK_TICKS-1; g.bat[1].charge=1; g.bat[1].sucking=true;
    tick(); assert(g.held!=1 && g.bat[1].cooldown_ticks==SUCTION_COOLDOWN_TICKS);
    assert(g.sound_events&SOUND_BREAK2);
    tick(); assert(!g.bat[1].sucking && !g.bat[1].release_required);

    ready(); goal(0); assert(g.arcade.points==100 && g.arcade.goals==1);
    goal(1); assert(g.arcade.lives==2 && g.arcade.goals==1);
    goal(0); goal(0);
    assert(g.arcade.level==2 && g.arcade.goals==0 && g.arcade.transition>0);
    assert(g.arcade.points>=800 && g.arcade.points<=1400);
    assert(g.arcade.lives==2 && g.phase==PLAY);
    float bx=g.bx; elapsed=g.arcade.level_time; tick();
    assert(g.bx==bx && g.arcade.level_time==elapsed);
    for(int t=0;t<2*GAME_HZ;t++) tick();
    assert(g.arcade.transition==0);
    g.arcade.level=5; g.arcade.goals=2; goal(0); assert(g.arcade.lives==3 && g.arcade.level==6);
    g.arcade.level=10; g.arcade.goals=2; g.arcade.lives=5; goal(0); assert(g.arcade.lives==5);
    g.arcade.level=UINT32_MAX; g.arcade.goals=2; goal(0);
    assert(g.arcade.level==UINT32_MAX && g.arcade.points==UINT32_MAX);

    ready(); goal(0); goal(1); goal(1); goal(1);
    assert(g.phase==FINISHED && g.arcade.lives==0 && g.score_entry==0);
    assert(g.highs[0].points==100 && g.highs[0].level==1);
    tick(); assert(g.highs[1].level==0); /* Record once only. */
    in[0].y=1; tick(); tick(); assert(!strcmp(g.highs[0].initials,"BAA"));
    in[0].y=0; in[0].x=1; tick();
    in[0].x=0; in[0].y=-1; tick(); assert(!strcmp(g.highs[0].initials,"BZA"));
    in[0].y=0; in[0].a=true; tick(); assert(g.phase==SCORES && g.scores_dirty);
    in[0].a=false; tick(); in[0].b=true; tick(); assert(g.phase==MENU);
    in[0].b=false; in[0].a=true; tick(); assert(g.phase==LOBBY);
    g.save_available=true; g.save_failed=true;
    in[0].a=false; in[0].start=true; tick(); assert(g.phase==PLAY);
    assert(g.save_available && g.save_failed && !g.scores_dirty);
    assert(g.highs[0].points==100 && !strcmp(g.highs[0].initials,"BZA"));
    assert(g.arcade.points==0 && g.arcade.level==1 && g.arcade.lives==3);

    /* Ranking and bounded insertion, including ties and a full table. */
    for(int i=0;i<HIGH_SCORE_COUNT;i++) g.highs[i]=(HighScore){.points=1000-i*10,.level=2,.initials="ABC"};
    g.arcade.points=950; g.arcade.level=2; g.arcade.lives=1; arcade_goal(&g,1);
    assert(g.score_entry==6 && g.highs[5].points==950 && g.highs[7].points==940);
    g.arcade.points=1; g.arcade.lives=1; arcade_goal(&g,1); assert(g.score_entry==-1);

    /* Long-run physics and real AI inputs at several difficulty levels. */
    for(unsigned level=1;level<=101;level+=25) {
        ready(); g.arcade.level=level; g.arcade.lives=5;
        for(int t=0;t<30*GAME_HZ;t++) {
            in[0].y=sinf(t*.071f); in[0].z=t%90<60; in[0].a=t%120>100;
            tick();
            assert(isfinite(g.bx) && isfinite(g.by));
            assert(g.bat[1].y>=BAT_HALF && g.bat[1].y<=ARENA_H-BAT_HALF);
            for(int i=0;i<FN;i++) assert(isfinite(fluid_flow_decode(fluid_velocity(&g.fluid)->u[i])) && isfinite(fluid_flow_decode(fluid_velocity(&g.fluid)->v[i])));
            if(g.phase==FINISHED) break;
        }
    }
    puts("PASS: solo menu/controller gating, AI reactions, forced release, levels, lives, scoring, initials, ranking, restart preservation, stability");
}
