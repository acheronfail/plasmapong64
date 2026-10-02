#include "game.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static Game g;
static Input in[2];
static void ready(void) {
    game_init(&g); g.phase=LOBBY; in[0]=(Input){.connected=true}; in[1]=in[0];
    in[0].start=true; game_step(&g,in); in[0].start=false;
    assert(g.phase==PLAY); g.serve=0;
}
static float energy(const Fluid *f) {
    float e=0; for(int i=0;i<FN;i++) e+=f->u[i]*f->u[i]+f->v[i]*f->v[i]; return e;
}
static float divergence(const Fluid *f) {
    float sum=0;
    for(int y=1;y<FH-1;y++) for(int x=1;x<FW-1;x++) {
        int k=y*FW+x; float d=f->u[k+1]-f->u[k-1]+f->v[k+FW]-f->v[k-FW];
        sum+=d*d;
    }
    return sum;
}
static float dye_x(const Fluid *f) {
    float mass=0,moment=0;
    for(int y=0;y<FH;y++) for(int x=0;x<FW;x++) {
        float d=f->red[y*FW+x]; mass+=d; moment+=d*(x+.5f)*CELL;
    }
    assert(mass>0); return moment/mass;
}
static float gold_sum(const Fluid *f) {
    float sum=0; for(int i=0;i<FN;i++) sum+=f->gold[i]; return sum;
}
int main(void) {
    game_init(&g); Input idle[2]={0};
    assert(g.phase==MENU);
    for(int t=0;t<90;t++) game_step(&g,idle);
    assert(g.phase==MENU && energy(&g.fluid)>0);
    float menu_dye=0; for(int i=0;i<FN;i++) menu_dye+=g.fluid.red[i]+g.fluid.blue[i]+g.fluid.gold[i];
    assert(menu_dye>0 && g.score[0]==0 && g.score[1]==0);
    idle[0]=(Input){.connected=true,.a=true}; game_step(&g,idle); assert(g.phase==LOBBY && (g.sound_events&SOUND_SELECT));
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
    float dye=0; for(int i=0;i<FN;i++) dye+=g.fluid.blue[i]; assert(dye>1);
    ready(); in[1].z=true; for(int i=0;i<20;i++) game_step(&g,in);
    fluid_sample(&g.fluid,ARENA_W-44,90,&u,&v); assert(u<-5);
    ready(); in[0].a=true; g.bx=g.bat[0].x+12; g.by=g.bat[0].y; g.bvx=-50; g.bvy=0;
    game_step(&g,in); assert(g.held==0);
    in[0].y=1; game_step(&g,in); assert(fabsf(g.by-g.bat[0].y)<.01f);
    in[0].a=false; game_step(&g,in); assert(g.held==-1 && g.bvx>170 && g.bat[0].burst>0);
    ready(); in[1].a=true; g.bx=g.bat[1].x-12; g.by=g.bat[1].y; g.bvx=50; g.bvy=0;
    game_step(&g,in); assert(g.held==1);
    in[1].a=false; game_step(&g,in); assert(g.held==-1 && g.bvx<-170);
    ready(); in[0].a=true; g.bx=g.bat[0].x+15; g.by=g.bat[0].y; g.bvx=-280; g.bvy=0;
    game_step(&g,in); assert(g.held==-1);
    ready(); g.bx=g.bat[0].x+8; g.by=g.bat[0].y; g.bvx=-200; g.bvy=0;
    game_step(&g,in); assert(g.bvx>0 && g.score[1]==0 && (g.sound_events&SOUND_BAT1));
    ready(); g.bx=g.bat[1].x-8; g.by=g.bat[1].y; g.bvx=200; g.bvy=0;
    game_step(&g,in); assert(g.bvx<0 && g.score[0]==0 && (g.sound_events&SOUND_BAT2));
    ready(); g.by=3; g.bvy=-150; game_step(&g,in); assert(g.bvy>0 && (g.sound_events&SOUND_WALL));
    ready(); g.bx=-2; g.by=12; g.bvx=-200; game_step(&g,in);
    assert(g.score[1]==1 && g.serve>0 && (g.sound_events&SOUND_GOAL));
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
    for(int i=0;i<FN;i++) g.fluid.v[i]=110;
    game_step(&g,in); assert(g.bvy>1); /* Real fluid-to-ball coupling. */
    ready(); in[0].a=true; game_step(&g,in);
    fluid_sample(&g.fluid,g.bat[0].x+22,g.bat[0].y,&u,&v); assert(u<0);
    in[0].a=false; game_step(&g,in);
    fluid_sample(&g.fluid,g.bat[0].x+22,g.bat[0].y,&u,&v); assert(u>0);
    /* Suction must transport EXISTING dye, not just influence the ball or
       paint a new coloured patch. Check both mirrored ends against no suction. */
    for(int p=0;p<2;p++) {
        ready(); g.serve=100;
        int x=p?FW-9:8;
        g.fluid.red[15*FW+x]=1;
        static Game without_suction; without_suction=g;
        in[p].a=true;
        Input idle[2]={{.connected=true},{.connected=true}};
        for(int t=0;t<12;t++) {
            game_step(&g,in); game_step(&without_suction,idle);
        }
        float moved=(dye_x(&without_suction.fluid)-dye_x(&g.fluid))*(p?-1:1);
        assert(moved>1.0f);
    }
    ready(); game_step(&g,in); assert(gold_sum(&g.fluid)>0);
    ready(); g.serve=1; game_step(&g,in); assert(gold_sum(&g.fluid)==0);
    ready(); g.held=0; game_step(&g,in); assert(gold_sum(&g.fluid)==0);
    ready();
    int trail=15*FW+20; g.fluid.gold[trail]=.5f;
    for(int i=0;i<FN;i++) g.fluid.u[i]=CELL/STEP;
    fluid_dye_step(&g.fluid,STEP);
    assert(g.fluid.gold[trail+1]>.45f && g.fluid.gold[trail]<.001f);
    assert(gold_sum(&g.fluid)<.5f); /* It advects and fades as a third dye. */
    ready();
    for(int t=0;t<3600;t++) {
        for(int p=0;p<2;p++) {
            in[p].x=sinf(t*.043f+p); in[p].y=cosf(t*.081f+p);
            in[p].z=t%90<60; in[p].a=t%70<32;
        }
        if(g.phase==FINISHED) { in[0].start=true; } else in[0].start=false;
        game_step(&g,in);
        assert(isfinite(g.bx) && isfinite(g.by));
        for(int i=0;i<FN;i++) {
            assert(isfinite(g.fluid.u[i]) && isfinite(g.fluid.v[i]));
            assert(fabsf(g.fluid.u[i])<1000 && fabsf(g.fluid.v[i])<1000);
            assert(g.fluid.red[i]>=0 && g.fluid.red[i]<=3.001f);
            assert(g.fluid.blue[i]>=0 && g.fluid.blue[i]<=3.001f);
            assert(g.fluid.gold[i]>=0 && g.fluid.gold[i]<=.651f);
        }
    }
    puts("PASS: two-player gating, movement, jets, suction, grab/release, collisions, scoring, pause, fluid projection/coupling, suction dye transport, gold trail, 120s stability");
    return 0;
}
