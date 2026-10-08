#include "game.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static Game g;
static Input in[MAX_PLAYERS];
static void ready(unsigned players) {
    game_init(&g); memset(in,0,sizeof(in));
    for(unsigned p=0;p<players;p++) in[p].connected=true;
    for(unsigned p=2;p<players;p++) {
        in[0].x=1; game_step(&g,in); in[0].x=0; game_step(&g,in);
    }
    assert(g.players==players);
    in[0].a=true; game_step(&g,in); assert(g.phase==LOBBY);
    in[0].a=false; in[0].start=true; game_step(&g,in);
    assert(g.phase==PLAY && g.players==players);
    in[0].start=false; g.serve=0;
}
static void miss(unsigned p) {
    fluid_init(&g.fluid); g.serve=0; g.held=-1;
    g.bx=ARENA_W*.5f; g.by=ARENA_H*.5f; g.bvx=g.bvy=0;
    if(p==0) { g.bx=game_left(&g)-2; g.bvx=-200; }
    if(p==1) { g.bx=game_right(&g)+2; g.bvx=200; }
    if(p==2) { g.by=ARENA_H+2; g.bvy=200; }
    if(p==3) { g.by=-2; g.bvy=-200; }
    game_step(&g,in);
}
int main(void) {
    game_init(&g); in[0]=(Input){.connected=true};
    game_step(&g,in); assert(g.phase==MENU && g.menu_selection==1 && g.players==2 && !g.sound_events);
    in[0].a=false; in[0].x=1; game_step(&g,in); assert(g.players==2);
    in[1].connected=true; in[0].x=0; game_step(&g,in);
    in[0].y=1; game_step(&g,in); assert(g.menu_selection==0); in[0].y=0;
    in[0].x=1; game_step(&g,in); assert(g.players==2);
    in[2].connected=true; in[0].x=0; game_step(&g,in);
    in[0].x=1; game_step(&g,in); assert(g.players==3);
    game_step(&g,in); assert(g.players==3); /* Held input never repeats. */
    in[3].connected=true; in[0].x=0; game_step(&g,in);
    in[0].x=1; game_step(&g,in); assert(g.players==4);
    in[0].x=-1; game_step(&g,in); assert(g.players==3);
    in[2].connected=false; in[3].connected=false; game_step(&g,in); assert(g.players==2);

    /* Sparse physical ports are assigned in port order, without phantom input edges. */
    game_init(&g); memset(in,0,sizeof(in)); in[1].connected=in[3].connected=true;
    in[3].start=true; game_step(&g,in); assert(g.phase==LOBBY);
    assert(g.player_port[0]==1 && g.player_port[1]==3);
    game_step(&g,in); assert(g.phase==LOBBY);
    in[3].start=false; game_step(&g,in); in[3].start=true; game_step(&g,in);
    assert(g.phase==PLAY); in[3].start=false;
    float old=g.bat[1].y; in[3].y=1; game_step(&g,in); assert(g.bat[1].y<old);

    for(unsigned players=3;players<=4;players++) {
        ready(players);
        assert(game_right(&g)-game_left(&g)==ARENA_H);
        assert(g.bat[2].y==ARENA_H-20);
        if(players==4) assert(g.bat[3].y==20);
        for(unsigned p=0;p<players;p++) {
            ready(players); in[p].connected=false; float bx=g.bx;
            game_step(&g,in); assert(g.phase==PAUSED && g.bx==bx);
            in[p].connected=true; game_step(&g,in); assert(g.phase==PAUSED);
            in[p].start=true; game_step(&g,in); assert(g.phase==PLAY);
        }
        for(unsigned p=0;p<players;p++) {
            ready(players); miss(p);
            assert(g.lives[p]==2 && g.phase==PLAY && g.serve>0);
            miss(p); miss(p); assert(!g.lives[p]);
            miss(p); assert(g.phase==PLAY && !g.lives[p] && g.serve==0);
            assert(g.sound_events&SOUND_WALL); /* Eliminated side reflects. */
            in[p].connected=false; game_step(&g,in); assert(g.phase==PLAY);
        }
        ready(players);
        for(unsigned p=0;p<players-1;p++) for(int life=0;life<3;life++) miss(p);
        assert(g.phase==FINISHED && g.winner==(int)players-1 && (g.sound_events&SOUND_WIN));
        in[0].connected=false; in[players-1].start=true; game_step(&g,in);
        assert(g.phase==FINISHED); /* Rematch needs even the eliminated pads. */
        in[0].connected=true; in[players-1].start=false; game_step(&g,in);
        in[players-1].start=true; game_step(&g,in); assert(g.phase==PLAY && g.players==players);
        for(unsigned p=0;p<players;p++) assert(g.lives[p]==3);
    }
    ready(3); g.by=3; g.bvy=-150; game_step(&g,in);
    assert(g.bvy>0 && (g.sound_events&SOUND_WALL));
    for(unsigned p=2;p<4;p++) {
        ready(4); float x=g.bat[p].x; in[p].x=1; game_step(&g,in); assert(g.bat[p].x>x);
        for(int t=0;t<90;t++) game_step(&g,in);
        assert(g.bat[p].x<=game_right(&g)-BAT_HALF-2);
        ready(4); float ny=p==2?-1:1;
        g.bx=g.bat[p].x; g.by=g.bat[p].y+ny*8; g.bvy=-ny*200;
        game_step(&g,in); assert(g.bvy*ny>0 && (g.sound_events&SOUND_BAT_OTHER));
        assert(g.rumble_ticks[p]==RUMBLE_HIT_TICKS);
        for(unsigned t=0;t<RUMBLE_HIT_TICKS;t++) game_step(&g,in);
        assert(!g.rumble_ticks[p]);
        ready(4); in[p].a=true; g.bx=g.bat[p].x; g.by=g.bat[p].y+ny*12; g.bvy=-ny*50;
        game_step(&g,in); assert(g.held==(int)p);
        in[p].x=1; game_step(&g,in); assert(g.bx==g.bat[p].x);
        in[p].x=0;
        for(unsigned t=1;t<SUCTION_CHARGE_TICKS;t++) game_step(&g,in);
        assert(g.bat[p].charge==1);
        in[p].a=false; game_step(&g,in); assert(g.held==-1 && g.bvy*ny>240);
        assert(g.rumble_ticks[p]==RUMBLE_BURST_TICKS);
        for(unsigned t=0;t<RUMBLE_BURST_TICKS;t++) game_step(&g,in);
        assert(!g.rumble_ticks[p]);
        ready(4); g.serve=100; g.held=p; in[p].a=true;
        for(unsigned t=0;t<SUCTION_BREAK_TICKS;t++) game_step(&g,in);
        assert(g.held==-1 && g.bat[p].cooldown_ticks==SUCTION_COOLDOWN_TICKS);
        assert(g.sound_events&SOUND_BREAK_OTHER);
        assert(!g.rumble_ticks[p]);
        ready(4); g.serve=100; in[p].z=true;
        for(int t=0;t<30;t++) game_step(&g,in);
        float u,v; fluid_sample(&g.fluid,g.bat[p].x,g.bat[p].y+ny*24,&u,&v); assert(v*ny>0);
        float red=0,blue=0,gold=0;
        for(int k=0;k<FN;k++) { red+=fluid_ink_decode(fluid_dye(&g.fluid)->red[k]); blue+=fluid_ink_decode(fluid_dye(&g.fluid)->blue[k]); gold+=fluid_ink_decode(fluid_dye(&g.fluid)->gold[k]); }
        assert(blue>0 && (p==2?gold>0 && red==0:red>0 && gold==0));
    }
    for(int corner=0;corner<4;corner++) {
        ready(4);
        float nx=corner&1?-1:1,ny=corner&2?-1:1;
        g.bx=(corner&1?game_right(&g):game_left(&g))+nx*(CORNER_SIZE*.5f+3);
        g.by=(corner&2?ARENA_H:0)+ny*(CORNER_SIZE*.5f+3);
        g.bvx=-nx*150; g.bvy=-ny*150;
        game_step(&g,in);
        assert(g.bvx*nx>0 && g.bvy*ny>0 && (g.sound_events&SOUND_WALL));
        for(int p=0;p<4;p++) assert(g.lives[p]==3);
    }
    /* At their closest legal corners, adjacent paddle outlines retain a gap. */
    ready(4);
    for(int t=0;t<120;t++) {
        g.serve=100;
        in[0].x=1; in[0].y=1;
        in[1].x=-1; in[1].y=-1;
        in[2].x=1; in[2].y=1;
        in[3].x=-1; in[3].y=-1;
        game_step(&g,in);
    }
    assert(g.bat[0].x+5<g.bat[3].x-BAT_HALF-2);
    assert(g.bat[3].y+5<g.bat[0].y-BAT_HALF-2);
    assert(g.bat[2].x+BAT_HALF+2<g.bat[1].x-5);
    assert(g.bat[1].y+BAT_HALF+2<g.bat[2].y-5);
    puts("PASS: 2P/3P/4P gating, sparse ports, square arena, diagonal reflections/paddle clearance, four-side collisions, lives, elimination walls, rematch, disconnects, rotated movement/jet/grab/launch/break, player pigments");
}
