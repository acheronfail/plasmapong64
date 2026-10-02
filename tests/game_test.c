#include "game.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static Game g;
static Input in[2];
static void ready(void) {
    game_init(&g); in[0]=(Input){.connected=true}; in[1]=in[0];
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
int main(void) {
    game_init(&g); in[0]=(Input){.connected=true,.start=true};
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
    game_step(&g,in); assert(g.bvx>0 && g.score[1]==0);
    ready(); g.bx=g.bat[1].x-8; g.by=g.bat[1].y; g.bvx=200; g.bvy=0;
    game_step(&g,in); assert(g.bvx<0 && g.score[0]==0);
    ready(); g.by=3; g.bvy=-150; game_step(&g,in); assert(g.bvy>0);
    ready(); g.bx=-2; g.by=12; g.bvx=-200; game_step(&g,in);
    assert(g.score[1]==1 && g.serve>0);
    g.serve=0; g.score[0]=8; g.bx=ARENA_W+2; g.bvx=200;
    game_step(&g,in); assert(g.phase==FINISHED && g.winner==0);
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
    for(int i=0;i<90;i++) fluid_step(&g.fluid,STEP);
    assert(energy(&g.fluid)<e);
    ready(); g.bx=145; g.by=90; g.bvx=100; g.bvy=0;
    for(int i=0;i<FN;i++) g.fluid.v[i]=110;
    game_step(&g,in); assert(g.bvy>1); /* Real fluid-to-ball coupling. */
    ready(); in[0].a=true; game_step(&g,in);
    fluid_sample(&g.fluid,g.bat[0].x+22,g.bat[0].y,&u,&v); assert(u<0);
    in[0].a=false; game_step(&g,in);
    fluid_sample(&g.fluid,g.bat[0].x+22,g.bat[0].y,&u,&v); assert(u>0);
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
        }
    }
    puts("PASS: two-player gating, movement, jets, suction, grab/release, collisions, scoring, pause, fluid projection/coupling, 120s stability");
    return 0;
}
