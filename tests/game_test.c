#include "game.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static uint32_t hash_float(uint32_t hash,float value) {
    uint32_t bits; memcpy(&bits,&value,sizeof(bits));
    return (hash^bits)*16777619u;
}
static Game g;
static Input in[MAX_PLAYERS];
static void ready(void) {
    game_init(&g); g.phase=LOBBY; in[0]=(Input){.connected=true}; in[1]=in[0];
    in[0].start=true; game_step(&g,in); in[0].start=false;
    assert(g.phase==PLAY); g.serve=0;
}
static void analog_movement_tests(void) {
    const float tilt[]={0,.10f,.12f,.13f,.56f,1,1.25f};
    float movement[sizeof(tilt)/sizeof(tilt[0])];
    /* Check both axes and directions on all four sides, away from limits. */
    for(unsigned p=0;p<MAX_PLAYERS;p++) for(unsigned axis=0;axis<2;axis++)
        for(int sign=-1;sign<=1;sign+=2) {
            for(unsigned i=0;i<sizeof(tilt)/sizeof(tilt[0]);i++) {
                ready(); g.players=4;
                for(unsigned q=0;q<MAX_PLAYERS;q++) in[q]=(Input){.connected=true};
                g.bat[p].x=p<2?(p?game_right(&g)-16.5f:game_left(&g)+16.5f):ARENA_W*.5f;
                g.bat[p].y=p<2?ARENA_H*.5f:(p==2?ARENA_H-16.5f:16.5f);
                float before=axis?g.bat[p].y:g.bat[p].x;
                if(axis) in[p].y=sign*tilt[i]; else in[p].x=sign*tilt[i];
                game_step(&g,in);
                float after=axis?g.bat[p].y:g.bat[p].x;
                movement[i]=(after-before)*sign*(axis?-1:1);
            }
            assert(movement[0]==0 && movement[1]==0 && movement[2]==0);
            assert(movement[3]>0 && movement[3]<movement[5]*.02f);
            assert(fabsf(movement[4]-movement[5]*.5f)<.0001f);
            assert(fabsf(movement[6]-movement[5])<.0001f);
            float speed=movement[5]/STEP;
            bool along_side=(p<2)==(axis==1);
            assert(fabsf(speed-(along_side?140:210))<.001f);
        }
    memset(in,0,sizeof(in));
    puts("PASS: analog dead zone, smooth ramp, half/full tilt, input clamp and four-side movement speeds");
}
static void forward_hit_tests(void) {
    for(unsigned p=0;p<MAX_PLAYERS;p++) {
        float rebound[2];
        float nx=p<2?(p?-1:1):0,ny=p<2?0:(p==2?-1:1);
        for(unsigned moving=0;moving<2;moving++) {
            ready(); g.players=4;
            for(unsigned q=0;q<MAX_PLAYERS;q++) in[q]=(Input){.connected=true};
            g.bat[p].x=p<2?(p?game_right(&g)-16.5f:game_left(&g)+16.5f):ARENA_W*.5f;
            g.bat[p].y=p<2?ARENA_H*.5f:(p==2?ARENA_H-16.5f:16.5f);
            g.bx=g.bat[p].x+nx*5; g.by=g.bat[p].y+ny*5;
            g.bvx=-nx*60; g.bvy=-ny*60;
            if(moving) { in[p].x=nx; in[p].y=-ny; }
            game_step(&g,in);
            assert(g.sound_events&(p==0?SOUND_BAT1:p==1?SOUND_BAT2:SOUND_BAT_OTHER));
            rebound[moving]=g.bvx*nx+g.bvy*ny;
            assert(rebound[moving]>0 && hypotf(g.bvx,g.bvy)<=290.001f);
        }
        assert(rebound[1]>rebound[0]+100);
    }
    memset(in,0,sizeof(in));
    puts("PASS: forward strokes add rebound power on all four sides within the ball-speed cap");
}
static float energy(const Fluid *f) {
    float e=0; for(int i=0;i<FN;i++) e+=fluid_velocity(f)->u[i]*fluid_velocity(f)->u[i]+fluid_velocity(f)->v[i]*fluid_velocity(f)->v[i]; return e;
}
static float divergence(const Fluid *f) {
    float sum=0;
    for(int y=1;y<FH-1;y++) for(int x=1;x<FW-1;x++) {
        int k=y*FW+x; float d=fluid_velocity(f)->u[k+1]-fluid_velocity(f)->u[k-1]+fluid_velocity(f)->v[k+FW]-fluid_velocity(f)->v[k-FW];
        sum+=d*d;
    }
    return sum;
}
static float dye_x(const Fluid *f) {
    float mass=0,moment=0;
    for(int y=0;y<FH;y++) for(int x=0;x<FW;x++) {
        float d=fluid_ink_decode(fluid_dye(f)->red[y*FW+x]); mass+=d; moment+=d*(x+.5f)*CELL;
    }
    assert(mass>0); return moment/mass;
}
static float gold_sum(const Fluid *f) {
    float sum=0; for(int i=0;i<FN;i++) sum+=fluid_ink_decode(fluid_dye(f)->gold[i]); return sum;
}
static float red_sum(const Fluid *f) {
    float sum=0; for(int i=0;i<FN;i++) sum+=fluid_ink_decode(fluid_dye(f)->red[i]); return sum;
}
static void flow_tests(void) {
    static Game a,b;
    Input input[MAX_PLAYERS]={{.connected=true},{.connected=true}};
    game_init(&a);
    input[0].y=1; game_step(&a,input); assert(a.menu_selection==3);
    input[0].y=0; input[0].a=true; game_step(&a,input); assert(a.phase==OPTIONS);
    input[0].a=false; input[0].x=-1; game_step(&a,input);
    assert(a.flow_effect==FLOW_PRESSURE && a.scores_dirty);
    a.scores_dirty=false; game_step(&a,input); assert(!a.scores_dirty);
    input[0].x=0; game_step(&a,input); input[0].x=1; game_step(&a,input);
    assert(a.flow_effect==FLOW_NONE);
    input[0].x=0; game_step(&a,input); input[0].x=1; game_step(&a,input);
    assert(a.flow_effect==FLOW_PARTICLES);
    for(int i=0;i<FLOW_COUNT;i++) {
        input[0].x=0; game_step(&a,input); input[0].x=1; game_step(&a,input);
        assert(a.flow_effect==(FlowEffect)((FLOW_PARTICLES+i+1)%FLOW_COUNT));
    }
    input[0].x=0; input[0].b=true; game_step(&a,input); assert(a.phase==MENU);
    a.phase=LOBBY; input[0].b=false; input[0].start=true; game_step(&a,input);
    assert(a.phase==PLAY && a.flow_effect==FLOW_PARTICLES);
    input[0].start=false;
    b=a; b.flow_effect=FLOW_NONE;
    for(int i=0;i<120;i++) { game_step(&a,input); game_step(&b,input); }
    assert(!memcmp(&a.fluid,&b.fluid,sizeof(Fluid)) && a.bx==b.bx && a.by==b.by);
    assert(a.menu_rng==b.menu_rng && a.arcade.rng==b.arcade.rng);
    a.phase=PAUSED; FlowTracer saved[FLOW_TRACERS]; memcpy(saved,a.tracers,sizeof(saved));
    FlowPosition saved_history[FLOW_HISTORY][FLOW_TRACERS]; memcpy(saved_history,a.tracer_history,sizeof(saved_history));
    game_step(&a,input); assert(!memcmp(saved,a.tracers,sizeof(saved)));
    assert(!memcmp(saved_history,a.tracer_history,sizeof(saved_history)));
    game_init(&a); a.flow_effect=FLOW_TAILS; game_flow_step(&a);
    for(int i=0;i<FN;i++) fluid_velocity(&a.fluid)->u[i]=fluid_flow_encode(30);
    a.tracer_history[a.tracer_head][0].x=100; a.tracer_history[a.tracer_head][0].y=100;
    game_flow_step(&a); assert(fabsf(a.tracer_history[a.tracer_head][0].x-(100+30*STEP))<.001f && a.tracer_history[(a.tracer_head+1)%FLOW_HISTORY][0].x==100);
    for(int i=1;i<FLOW_HISTORY-1;i++) game_flow_step(&a);
    assert(fabsf(a.tracer_history[a.tracer_head][0].x-(100+(FLOW_HISTORY-1)*30*STEP))<.001f && a.tracer_history[(a.tracer_head+FLOW_HISTORY-1)%FLOW_HISTORY][0].x==100);
    a.tracers[0].life=0; game_flow_step(&a);
    for(int i=1;i<FLOW_HISTORY;i++) {
        assert(a.tracer_history[i][0].x==a.tracer_history[0][0].x && a.tracer_history[i][0].y==a.tracer_history[0][0].y);
    }
    /* Every ring slot must retain its age across several cursor wraps. */
    for(int j=0;j<FLOW_HISTORY;j++) a.tracer_history[j][0].x=a.tracer_history[j][0].y=100;
    a.tracers[0].life=4*FLOW_HISTORY;
    for(int step=1;step<=3*FLOW_HISTORY;step++) {
        game_flow_step(&a);
        for(int age=0;age<FLOW_HISTORY;age++) {
            unsigned slot=(a.tracer_head+age)%FLOW_HISTORY;
            float expected=100+(step>age?step-age:0)*30*STEP;
            assert(fabsf(a.tracer_history[slot][0].x-expected)<.001f);
            assert(a.tracer_history[slot][0].y==100);
        }
    }
    for(int i=0;i<400;i++) game_flow_step(&a);
    for(int i=0;i<FLOW_TRACERS;i++) if(a.tracers[i].life) {
        assert(a.tracer_history[a.tracer_head][i].x>=0 && a.tracer_history[a.tracer_head][i].x<ARENA_W);
        assert(a.tracer_history[a.tracer_head][i].y>=0 && a.tracer_history[a.tracer_head][i].y<ARENA_H);
    }
    fluid_init(&a.fluid); uint32_t dark=fluid_color(&a.fluid,0);
    assert(fluid_speed_color(&a.fluid,0)==dark);
    fluid_velocity(&a.fluid)->u[0]=fluid_flow_encode(120);
    uint32_t light=fluid_speed_color(&a.fluid,0); assert(light>dark);
    fluid_velocity(&a.fluid)->u[0]=fluid_flow_encode(-120);
    assert(fluid_speed_color(&a.fluid,0)==light);
    const int speeds[]={0,16,32,64,128,256,512};
    const uint32_t colors[]={0x050916,0x244bce,0x17bdd4,0x35cb63,0xf3cf3a,0xf04b36,0xf04b36};
    for(unsigned i=0;i<sizeof(speeds)/sizeof(speeds[0]);i++) {
        fluid_velocity(&a.fluid)->u[0]=fluid_flow_encode(speeds[i]);
        assert(fluid_speed_color(&a.fluid,0)==colors[i]);
        fluid_velocity(&a.fluid)->u[0]=fluid_flow_encode(-speeds[i]);
        assert(fluid_speed_color(&a.fluid,0)==colors[i]);
    }
    fluid_velocity(&a.fluid)->u[0]=fluid_flow_encode(8);
    assert(fluid_speed_color(&a.fluid,0)==0x142a72); /* Interpolated, not banded. */
    fluid_velocity(&a.fluid)->u[0]=fluid_flow_encode(32);
    fluid_velocity(&a.fluid)->v[0]=fluid_flow_encode(32);
    assert(fluid_speed_color(&a.fluid,0)==0x26c49b);
    fluid_velocity(&a.fluid)->u[0]=fluid_flow_encode(512);
    fluid_dye(&a.fluid)->red[0]=fluid_ink_encode(2);
    assert(fluid_speed_color(&a.fluid,0)==colors[6]);
    puts("PASS: options navigation, effect retention, tracer transport/pause, visual-only physics, speed spectrum");
}
static void menu_confirm_tests(void) {
    static Game menu;
    const Phase destinations[]={LOBBY,LOBBY,SCORES,OPTIONS};
    for(unsigned button=0;button<3;button++) for(unsigned row=0;row<4;row++) {
        Input input[MAX_PLAYERS]={{.connected=true},{.connected=true}};
        game_init(&menu); menu.menu_selection=row;
        input[1].a=button==0; input[1].z=button==1; input[1].start=button==2;
        game_step(&menu,input);
        assert(menu.phase==destinations[row] && (menu.sound_events&SOUND_SELECT));
        /* Holding the select button must not immediately leave the new screen. */
        game_step(&menu,input);
        assert(menu.phase==destinations[row] && !menu.sound_events);
        game_init(&menu); menu.menu_selection=row; input[1].connected=false;
        game_step(&menu,input); assert(menu.phase==MENU);
        if(row==0) {
            input[0]=input[1]; input[0].connected=true;
            game_step(&menu,input); assert(menu.phase==MENU);
        }
    }
    puts("PASS: A/Z/START main-menu selection, held-button debounce, disconnected input and multiplayer gating");
}
int main(void) {
    analog_movement_tests();
    forward_hit_tests();
    menu_confirm_tests();
    flow_tests();
    /* A serve stays at rest through the countdown and in still water. */
    ready(); g.serve=1.2f;
    for(int t=0;t<90;t++) {
        game_step(&g,in);
        assert(g.bvx==0 && g.bvy==0);
        assert(g.bx==ARENA_W*.5f && g.by==ARENA_H*.5f);
    }
    assert(g.serve==0);
    /* Either player's jet can start the ball from rest. */
    for(int p=0;p<2;p++) {
        ready(); in[p].z=true;
        for(int t=0;t<60;t++) game_step(&g,in);
        assert(g.bvx*(p?-1:1)>0);
        assert((g.bx-ARENA_W*.5f)*(p?-1:1)>0);
    }
    memset(in,0,sizeof(in));
    game_init(&g); Input idle[MAX_PLAYERS]={0};
    assert(g.phase==MENU);
    for(int t=0;t<90;t++) game_step(&g,idle);
    assert(g.phase==MENU && energy(&g.fluid)>0);
    float menu_dye=0; for(int i=0;i<FN;i++) menu_dye+=fluid_dye(&g.fluid)->red[i]+fluid_dye(&g.fluid)->blue[i]+fluid_dye(&g.fluid)->gold[i];
    assert(menu_dye>0 && g.score[0]==0 && g.score[1]==0);
    idle[0]=(Input){.connected=true,.a=true}; game_step(&g,idle); assert(g.phase==MENU && !g.sound_events);
    idle[0].a=false; idle[1].connected=true; game_step(&g,idle);
    idle[0].a=true; game_step(&g,idle); assert(g.phase==LOBBY && (g.sound_events&SOUND_SELECT));
    idle[1].connected=false;
    game_step(&g,idle); assert(g.sound_events==0);
    idle[0].a=false; idle[0].start=true; game_step(&g,idle); assert(g.phase==LOBBY);
    idle[1].connected=true; game_step(&g,idle); assert(g.phase==LOBBY); /* Release to confirm. */
    idle[0].start=false; game_step(&g,idle); idle[0].start=true; game_step(&g,idle); assert(g.phase==PLAY);
    idle[0].start=false; game_step(&g,idle); idle[0].start=true; game_step(&g,idle); assert(g.phase==PAUSED);
    idle[0].b=true; game_step(&g,idle); assert(g.phase==MENU && (g.sound_events&SOUND_BACK));

    game_init(&g); g.phase=LOBBY; in[0]=(Input){.connected=true,.start=true};
    game_step(&g,in); assert(g.phase==LOBBY);
    ready(); float y=g.bat[0].y; in[0].y=1; game_step(&g,in);
    assert(g.bat[0].y<y && energy(&g.fluid)>0);
    for(int i=0;i<100;i++) game_step(&g,in);
    assert(g.bat[0].y>=BAT_HALF);
    ready(); in[0].z=true; for(int i=0;i<20;i++) game_step(&g,in);
    float u,v; fluid_sample(&g.fluid,44,90,&u,&v); assert(u>5);
    float dye=0; for(int i=0;i<FN;i++) dye+=fluid_ink_decode(fluid_dye(&g.fluid)->blue[i]); assert(dye>1);
    ready(); in[1].z=true; for(int i=0;i<20;i++) game_step(&g,in);
    fluid_sample(&g.fluid,ARENA_W-44,90,&u,&v); assert(u<-5);
    ready(); in[0].a=true; g.bx=g.bat[0].x+12; g.by=g.bat[0].y; g.bvx=-50; g.bvy=0;
    game_step(&g,in); assert(g.held==0);
    in[0].y=1; game_step(&g,in); assert(fabsf(g.by-g.bat[0].y)<.01f);
    in[0].a=false; game_step(&g,in); assert(g.held==-1 && g.bvx>0 && g.bvx<40 && g.bat[0].burst>0);
    ready(); in[1].a=true; g.bx=g.bat[1].x-12; g.by=g.bat[1].y; g.bvx=50; g.bvy=0;
    game_step(&g,in); assert(g.held==1);
    game_step(&g,in); /* One tick holding the ball charges the release. */
    in[1].a=false; game_step(&g,in); assert(g.held==-1 && g.bvx<0 && g.bvx>-40);
    ready(); in[0].a=true; g.bx=g.bat[0].x+15; g.by=g.bat[0].y; g.bvx=-280; g.bvy=0;
    game_step(&g,in); assert(g.held==-1);
    ready(); g.bx=g.bat[0].x+8; g.by=g.bat[0].y; g.bvx=-200; g.bvy=0;
    game_step(&g,in); assert(g.bvx>0 && g.score[1]==0 && (g.sound_events&SOUND_BAT1));
    ready(); g.bx=g.bat[1].x-8; g.by=g.bat[1].y; g.bvx=200; g.bvy=0;
    game_step(&g,in); assert(g.bvx<0 && g.score[0]==0 && (g.sound_events&SOUND_BAT2));
    ready(); g.by=3; g.bvy=-150; game_step(&g,in); assert(g.bvy>0 && (g.sound_events&SOUND_WALL));
    ready(); g.bx=-2; g.by=12; g.bvx=-200; game_step(&g,in);
    assert(g.score[1]==1 && g.serve>0 && (g.sound_events&SOUND_GOAL));
    assert(g.bvx==0 && g.bvy==0);
    assert(g.bx==ARENA_W*.5f && g.by==ARENA_H*.5f);
    game_step(&g,in); assert(g.sound_events==0);
    g.serve=0; g.score[0]=8; g.bx=ARENA_W+2; g.bvx=200;
    game_step(&g,in); assert(g.phase==FINISHED && g.winner==0 && (g.sound_events&SOUND_WIN));
    in[0].start=true; game_step(&g,in); assert(g.score[0]==0 && g.phase==PLAY);
    in[0].start=false; game_step(&g,in);
    in[1].connected=false; float bx=g.bx; game_step(&g,in);
    assert(g.phase==PAUSED && g.bx==bx);
    in[1].connected=true; game_step(&g,in); assert(g.phase==PAUSED);
    in[0].start=true; game_step(&g,in); assert(g.phase==PLAY);
    game_step(&g,in); assert(g.phase==PLAY); /* Held START does not toggle. */
    ready(); fluid_splat(&g.fluid,144,90,30,130,-70,1,0);
    float d=divergence(&g.fluid); fluid_project(&g.fluid);
    assert(divergence(&g.fluid)<d*.8f);
    float e=energy(&g.fluid);
    for(int i=0;i<90;i++) { fluid_velocity_step(&g.fluid,STEP); fluid_dye_step(&g.fluid,STEP); }
    assert(energy(&g.fluid)<e);
    ready(); g.bx=145; g.by=90; g.bvx=100; g.bvy=0;
    for(int i=0;i<FN;i++) fluid_velocity(&g.fluid)->v[i]=fluid_flow_encode(110);
    game_step(&g,in); assert(g.bvy>165*STEP && g.bvy<210*STEP); /* Cross-current bends the ball. */
    /* A sustained jet leaves useful momentum four seconds after release.
       Measure decoded speed so every storage backend uses the same units. */
    for(int p=0;p<2;p++) {
        ready(); g.serve=100; in[p].z=true;
        for(int t=0;t<3*GAME_HZ;t++) game_step(&g,in);
        in[p].z=false;
        for(int t=0;t<4*GAME_HZ;t++) game_step(&g,in);
        float residual=0;
        for(int k=0;k<FN;k++) {
            float fu=fluid_flow_decode(fluid_velocity(&g.fluid)->u[k]);
            float fv=fluid_flow_decode(fluid_velocity(&g.fluid)->v[k]);
            residual+=fu*fu+fv*fv;
        }
        float speed=sqrtf(residual/FN);
        /* At 60 Hz, shorter backtraces lose less momentum to interpolation.
           Keep the four-second residual within the measured float/fixed band. */
        assert(speed>18 && speed<24);
    }
    ready(); in[0].a=true; game_step(&g,in);
    fluid_sample(&g.fluid,g.bat[0].x+22,g.bat[0].y,&u,&v); assert(u<0);
    /* Empty suction never charges or breaks, and releasing it injects no burst. */
    for(int p=0;p<2;p++) {
        ready(); g.serve=100; in[p].a=true;
        for(unsigned t=0;t<SUCTION_BREAK_TICKS+10;t++) {
            game_step(&g,in);
            assert(g.bat[p].sucking && g.bat[p].charge==0);
            assert(!g.bat[p].suction_ticks && !g.bat[p].cooldown_ticks);
        }
        static Game idle_release; idle_release=g;
        idle_release.bat[p].sucking=false;
        in[p].a=false;
        game_step(&idle_release,in); game_step(&g,in);
        assert(!g.bat[p].sucking && g.bat[p].burst==0);
        for(int k=0;k<FN;k++) {
            assert(fluid_velocity(&g.fluid)->u[k]==fluid_velocity(&idle_release.fluid)->u[k]);
            assert(fluid_velocity(&g.fluid)->v[k]==fluid_velocity(&idle_release.fluid)->v[k]);
        }
        /* A catch after a long search starts with no banked charge. */
        in[p].a=true;
        for(unsigned t=0;t<SUCTION_BREAK_TICKS;t++) game_step(&g,in);
        g.serve=0; g.bx=g.bat[p].x+(p?-8:8); g.by=g.bat[p].y;
        g.bvx=0; g.bvy=0;
        game_step(&g,in);
        assert(g.held==p && g.bat[p].charge==0);
        game_step(&g,in);
        assert(g.bat[p].suction_ticks==1 && fabsf(g.bat[p].charge-STEP)<.00001f);
        in[p].a=false; game_step(&g,in);
        assert(g.held==-1 && g.bat[p].burst>0 && g.bat[p].charge==0);
    }
    /* Isolate the release impulse after real holds: half charge gives half
       the velocity away from the fluid speed cap, and zero charge gives none. */
    for(int p=0;p<2;p++) {
        float release_energy[4];
        float release_speed[4];
        int hold_ticks[4]={0,1,GAME_HZ/2,GAME_HZ};
        for(int h=0;h<4;h++) {
            ready(); g.serve=100; g.held=p; in[p].a=true;
            for(int t=0;t<hold_ticks[h];t++) game_step(&g,in);
            assert(fabsf(g.bat[p].charge-hold_ticks[h]*STEP)<.00001f);
            fluid_init(&g.fluid); g.bat[p].sucking=true;
            /* The serve countdown isolates launch speed from subsequent drag. */
            g.held=p;
            in[p].a=false; game_step(&g,in);
            assert(g.held==-1);
            float launch=hold_ticks[h]==GAME_HZ?290:200*hold_ticks[h]*STEP;
            assert(fabsf(g.bvx-(p?-1:1)*launch)<.001f);
            release_energy[h]=energy(&g.fluid);
            fluid_sample(&g.fluid,g.bat[p].x+(p?-22:22),g.bat[p].y+24,&u,&v);
            release_speed[h]=fabsf(u);
            assert(g.bat[p].charge==0 && !g.bat[p].sucking);
            game_step(&g,in); assert(g.bat[p].charge==0);
        }
        assert(release_energy[0]==0 && release_energy[3]>0);
        assert(release_energy[1]<release_energy[2] && release_energy[2]<release_energy[3]);
#ifdef PLASMAPONG_VELOCITY_FIXED
        /* Independently quantized injections can differ by one velocity LSB. */
        assert(fabsf(release_speed[1]-release_speed[3]*STEP)<1.0f/VELOCITY_SCALE);
        assert(fabsf(release_speed[2]-release_speed[3]*.5f)<1.0f/VELOCITY_SCALE);
#else
        assert(fabsf(release_speed[1]/release_speed[3]-STEP)<.00001f);
        assert(fabsf(release_speed[2]/release_speed[3]-.5f)<.00001f);
#endif
        /* Repeated one-frame taps must stay weaker than a continuous Z jet. */
        ready(); g.serve=100;
        for(int t=0;t<60;t++) { in[p].a=t%2==0; game_step(&g,in); }
        float tap_energy=energy(&g.fluid);
        ready(); g.serve=100; in[p].z=true;
        for(int t=0;t<60;t++) game_step(&g,in);
        assert(tap_energy<energy(&g.fluid));
    }
    /* All ticks in the grace window give the perfect bonus, with a sharp jump from the
       last undercharged tick. The following held tick breaks instead. */
    for(int p=0;p<2;p++) {
        for(unsigned ticks=SUCTION_CHARGE_TICKS-1;ticks<SUCTION_BREAK_TICKS;ticks++) {
            ready(); g.serve=100; g.held=p; in[p].a=true;
            for(unsigned t=0;t<ticks;t++) game_step(&g,in);
            in[p].a=false; game_step(&g,in);
            float speed=g.bvx*(p?-1:1);
            assert(g.held==-1 && !g.bat[p].cooldown_ticks);
            if(ticks<SUCTION_CHARGE_TICKS) assert(speed>190 && speed<200);
            else assert(speed==290);
        }
        /* Exact 99% also stays below the bonus, even off the tick grid. */
        ready(); g.serve=100; g.held=p;
        g.bat[p].sucking=true; g.bat[p].charge=.99f;
        game_step(&g,in); assert(fabsf(g.bvx*(p?-1:1)-198)<.001f);
    }
    /* Full charge has a 67ms grace window; breaking drops into the flow
       without injecting a burst, then locks suction for five active seconds. */
    for(int p=0;p<2;p++) {
        ready(); g.serve=100; in[p].a=true;
        g.held=p;
        for(unsigned t=0;t<SUCTION_CHARGE_TICKS;t++) game_step(&g,in);
        assert(g.bat[p].charge==1 && g.bat[p].sucking && !g.bat[p].cooldown_ticks);
        for(unsigned t=SUCTION_CHARGE_TICKS;t<SUCTION_BREAK_TICKS-1;t++) game_step(&g,in);
        assert(g.bat[p].charge==1 && !g.bat[p].cooldown_ticks);
        static Game without_burst; without_burst=g;
        without_burst.bat[p].sucking=false; without_burst.held=-1;
        Input idle[MAX_PLAYERS]={{.connected=true},{.connected=true}};
        game_step(&without_burst,idle); game_step(&g,in);
        assert(!g.bat[p].sucking && g.bat[p].charge==0 && g.bat[p].burst==0);
        assert(g.bat[p].cooldown_ticks==SUCTION_COOLDOWN_TICKS && g.bat[p].release_required);
        assert(g.sound_events==(p?SOUND_BREAK2:SOUND_BREAK1) && g.held==-1);
        for(int k=0;k<FN;k++) {
            assert(fluid_velocity(&g.fluid)->u[k]==fluid_velocity(&without_burst.fluid)->u[k]);
            assert(fluid_velocity(&g.fluid)->v[k]==fluid_velocity(&without_burst.fluid)->v[k]);
        }
        fluid_sample(&g.fluid,g.bx,g.by,&u,&v);
        assert(g.bvx==u && g.bvy==v);
        g.phase=PAUSED;
        for(int t=0;t<10;t++) game_step(&g,in);
        assert(g.bat[p].cooldown_ticks==SUCTION_COOLDOWN_TICKS);
        g.phase=PLAY;
        for(unsigned t=1;t<=SUCTION_COOLDOWN_TICKS;t++) {
            game_step(&g,in);
            assert(g.bat[p].cooldown_ticks==SUCTION_COOLDOWN_TICKS-t);
            assert(!g.bat[p].sucking && g.bat[p].charge==0 && g.sound_events==0);
        }
        game_step(&g,in); assert(!g.bat[p].sucking); /* Must release A to rearm. */
        in[p].a=false; game_step(&g,in); assert(!g.bat[p].release_required);
        in[p].a=true; game_step(&g,in); assert(g.bat[p].sucking && g.bat[p].suction_ticks==0);
        ready(); g.serve=100; g.held=p; in[p].a=true;
        for(unsigned t=0;t<SUCTION_BREAK_TICKS-1;t++) game_step(&g,in);
        in[p].a=false; game_step(&g,in);
        assert(g.bat[p].burst>0 && !g.bat[p].cooldown_ticks && !g.bat[p].suction_ticks);
        /* Releasing during recovery rearms A, but tapping cannot bypass the
           cooldown; movement and jets remain available throughout it. */
        ready(); g.serve=100; g.bat[p].cooldown_ticks=SUCTION_COOLDOWN_TICKS;
        g.bat[p].release_required=true;
        float old_y=g.bat[p].y; in[p].y=1; in[p].z=true;
        for(unsigned t=1;t<SUCTION_COOLDOWN_TICKS;t++) {
            in[p].a=t%2==0; game_step(&g,in);
            assert(!g.bat[p].sucking && g.bat[p].charge==0 && g.bat[p].burst==0);
        }
        assert(g.bat[p].y<old_y && energy(&g.fluid)>0);
        in[p].a=true; game_step(&g,in);
        assert(!g.bat[p].cooldown_ticks && g.bat[p].sucking && g.bat[p].suction_ticks==0);
    }
    /* Suction must transport EXISTING dye, not just influence the ball or
       paint a new coloured patch. Check both mirrored ends against no suction. */
    for(int p=0;p<2;p++) {
        ready(); g.serve=100;
        int x=p?FW-9:8;
        fluid_dye(&g.fluid)->red[15*FW+x]=fluid_ink_encode(1);
        static Game without_suction; without_suction=g;
        in[p].a=true;
        Input idle[MAX_PLAYERS]={{.connected=true},{.connected=true}};
        for(int t=0;t<12;t++) {
            game_step(&g,in); game_step(&without_suction,idle);
        }
        float moved=(dye_x(&without_suction.fluid)-dye_x(&g.fluid))*(p?-1:1);
        assert(moved>1.0f);
    }
    ready(); game_step(&g,in); assert(gold_sum(&g.fluid)>0);
    /* Speed, not ownership or a launch flag, drives both ball and dye colour. */
    ready(); g.bvx=BALL_HOT_SPEED; g.bvy=0; assert(!game_ball_hot(&g));
    g.bvy=1; assert(game_ball_hot(&g));
    g.held=0; assert(!game_ball_hot(&g));
    g.held=-1; g.serve=1; assert(!game_ball_hot(&g));
    for(int p=0;p<2;p++) {
        ready(); g.bx=ARENA_W*.5f; g.bvx=(p?-1:1)*290; g.bvy=0;
        game_step(&g,in);
        assert(game_ball_hot(&g) && red_sum(&g.fluid)>0 && gold_sum(&g.fluid)==0);
        /* Cooling down restores gold emission without recolouring old dye. */
        g.bvx=(p?-1:1)*198; game_step(&g,in);
        assert(!game_ball_hot(&g) && gold_sum(&g.fluid)>0 && red_sum(&g.fluid)>0);
        ready(); g.bvx=(p?-1:1)*198; g.bvy=0; game_step(&g,in);
        assert(!game_ball_hot(&g) && gold_sum(&g.fluid)>0 && red_sum(&g.fluid)==0);
        /* A real perfect release remains hot after fluid drag and speed limits. */
        ready(); g.held=p; g.bat[p].sucking=true; g.bat[p].charge=1;
        game_step(&g,in); assert(g.held==-1 && game_ball_hot(&g));
    }
    ready();
    fluid_hot_ball_dye(&g.fluid,123,93,.4f);
    int hot_trail=15*FW+20;
    for(int i=0;i<FN;i++) fluid_velocity(&g.fluid)->u[i]=fluid_flow_encode(CELL/STEP);
    fluid_dye_step(&g.fluid,STEP);
    assert(fluid_ink_decode(fluid_dye(&g.fluid)->red[hot_trail+1])>.35f);
    assert(fluid_ink_decode(fluid_dye(&g.fluid)->red[hot_trail])<.15f);
    assert(gold_sum(&g.fluid)==0);
    ready(); g.serve=1; game_step(&g,in); assert(gold_sum(&g.fluid)==0);
    ready(); g.held=0; game_step(&g,in); assert(gold_sum(&g.fluid)==0);
    ready();
    int trail=15*FW+20; fluid_dye(&g.fluid)->gold[trail]=fluid_ink_encode(.5f);
    for(int i=0;i<FN;i++) fluid_velocity(&g.fluid)->u[i]=fluid_flow_encode(CELL/STEP);
    fluid_dye_step(&g.fluid,STEP);
    assert(fluid_ink_decode(fluid_dye(&g.fluid)->gold[trail+1])>.45f && fluid_ink_decode(fluid_dye(&g.fluid)->gold[trail])<.001f);
    assert(gold_sum(&g.fluid)<.5f); /* It advects and fades as a third dye. */
    ready();
    uint32_t physics_hash=2166136261u;
    for(int t=0;t<120*GAME_HZ;t++) {
        for(int p=0;p<2;p++) {
            in[p].x=sinf(t*.043f+p); in[p].y=cosf(t*.081f+p);
            in[p].z=t%90<60; in[p].a=t%70<32;
        }
        if(g.phase==FINISHED) { in[0].start=true; } else in[0].start=false;
        game_step(&g,in);
        physics_hash=hash_float(physics_hash,g.bx);
        physics_hash=hash_float(physics_hash,g.by);
        physics_hash=hash_float(physics_hash,g.bvx);
        physics_hash=hash_float(physics_hash,g.bvy);
        physics_hash=(physics_hash^(unsigned)g.phase)*16777619u;
        physics_hash=(physics_hash^(unsigned)g.score[0])*16777619u;
        physics_hash=(physics_hash^(unsigned)g.score[1])*16777619u;
        assert(isfinite(g.bx) && isfinite(g.by));
        for(int i=0;i<FN;i++) {
            physics_hash=hash_float(physics_hash,fluid_velocity(&g.fluid)->u[i]);
            physics_hash=hash_float(physics_hash,fluid_velocity(&g.fluid)->v[i]);
            assert(isfinite(fluid_flow_decode(fluid_velocity(&g.fluid)->u[i])) && isfinite(fluid_flow_decode(fluid_velocity(&g.fluid)->v[i])));
            assert(fabsf(fluid_flow_decode(fluid_velocity(&g.fluid)->u[i]))<1000 && fabsf(fluid_flow_decode(fluid_velocity(&g.fluid)->v[i]))<1000);
            assert(fluid_ink_decode(fluid_dye(&g.fluid)->red[i])>=0 && fluid_ink_decode(fluid_dye(&g.fluid)->red[i])<=3.001f);
            assert(fluid_ink_decode(fluid_dye(&g.fluid)->blue[i])>=0 && fluid_ink_decode(fluid_dye(&g.fluid)->blue[i])<=3.001f);
            assert(fluid_ink_decode(fluid_dye(&g.fluid)->gold[i])>=0 && fluid_ink_decode(fluid_dye(&g.fluid)->gold[i])<=.651f);
        }
    }
    puts("PASS: two-player gating, movement, jets, suction, grab/release, collisions, scoring, pause, fluid projection/coupling, suction dye transport, gold trail, 120s stability");
    printf("Physics trace hash: %08x\n",physics_hash);
    return 0;
}
