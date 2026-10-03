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
    assert(a.flow_effect==FLOW_SPEED && a.scores_dirty);
    a.scores_dirty=false; game_step(&a,input); assert(!a.scores_dirty);
    input[0].x=0; game_step(&a,input); input[0].x=1; game_step(&a,input);
    assert(a.flow_effect==FLOW_NONE);
    input[0].x=0; game_step(&a,input); input[0].x=1; game_step(&a,input);
    assert(a.flow_effect==FLOW_PARTICLES);
    input[0].x=0; input[0].b=true; game_step(&a,input); assert(a.phase==MENU);
    a.phase=LOBBY; input[0].b=false; input[0].start=true; game_step(&a,input);
    assert(a.phase==PLAY && a.flow_effect==FLOW_PARTICLES);
    input[0].start=false;
    b=a; b.flow_effect=FLOW_NONE;
    for(int i=0;i<120;i++) { game_step(&a,input); game_step(&b,input); }
    assert(!memcmp(&a.fluid,&b.fluid,sizeof(Fluid)) && a.bx==b.bx && a.by==b.by);
    assert(a.menu_rng==b.menu_rng && a.arcade.rng==b.arcade.rng);
    a.phase=PAUSED; FlowTracer saved[FLOW_TRACERS]; memcpy(saved,a.tracers,sizeof(saved));
    game_step(&a,input); assert(!memcmp(saved,a.tracers,sizeof(saved)));
    game_init(&a); a.flow_effect=FLOW_TAILS; game_flow_step(&a);
    for(int i=0;i<FN;i++) fluid_velocity(&a.fluid)->u[i]=fluid_flow_encode(30);
    a.tracers[0].x[0]=100; a.tracers[0].y[0]=100;
    game_flow_step(&a); assert(fabsf(a.tracers[0].x[0]-101)<.001f && a.tracers[0].x[1]==100);
    for(int i=1;i<FLOW_HISTORY-1;i++) game_flow_step(&a);
    assert(fabsf(a.tracers[0].x[0]-108)<.001f && a.tracers[0].x[FLOW_HISTORY-1]==100);
    a.tracers[0].life=0; game_flow_step(&a);
    for(int i=1;i<FLOW_HISTORY;i++) {
        assert(a.tracers[0].x[i]==a.tracers[0].x[0] && a.tracers[0].y[i]==a.tracers[0].y[0]);
    }
    for(int i=0;i<400;i++) game_flow_step(&a);
    for(int i=0;i<FLOW_TRACERS;i++) if(a.tracers[i].life) {
        assert(a.tracers[i].x[0]>=0 && a.tracers[i].x[0]<ARENA_W);
        assert(a.tracers[i].y[0]>=0 && a.tracers[i].y[0]<ARENA_H);
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
int main(void) {
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
    game_step(&g,in); assert(g.bvy>5.5f && g.bvy<7); /* Cross-current bends the ball. */
    /* A sustained jet leaves useful momentum four seconds after release.
       Measure decoded speed so every storage backend uses the same units. */
    for(int p=0;p<2;p++) {
        ready(); g.serve=100; in[p].z=true;
        for(int t=0;t<90;t++) game_step(&g,in);
        in[p].z=false;
        for(int t=0;t<120;t++) game_step(&g,in);
        float residual=0;
        for(int k=0;k<FN;k++) {
            float fu=fluid_flow_decode(fluid_velocity(&g.fluid)->u[k]);
            float fv=fluid_flow_decode(fluid_velocity(&g.fluid)->v[k]);
            residual+=fu*fu+fv*fv;
        }
        float speed=sqrtf(residual/FN);
        assert(speed>13 && speed<20);
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
        int hold_ticks[4]={0,1,15,30};
        for(int h=0;h<4;h++) {
            ready(); g.serve=100; g.held=p; in[p].a=true;
            for(int t=0;t<hold_ticks[h];t++) game_step(&g,in);
            assert(fabsf(g.bat[p].charge-hold_ticks[h]*STEP)<.00001f);
            fluid_init(&g.fluid); g.bat[p].sucking=true;
            /* The serve countdown isolates launch speed from subsequent drag. */
            g.held=p;
            in[p].a=false; game_step(&g,in);
            assert(g.held==-1);
            float launch=hold_ticks[h]==30?290:200*hold_ticks[h]*STEP;
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
    /* Both green ticks give the perfect bonus, with a sharp jump from the
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
    /* Full charge has a two-tick grace window; breaking drops into the flow
       without injecting a burst, then locks suction for 150 active ticks. */
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
    for(int t=0;t<3600;t++) {
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
