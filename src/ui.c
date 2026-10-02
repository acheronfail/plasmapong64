#include "mathutil.h"
#include "draw.h"
#include <math.h>
#include <stdio.h>
#define OX 16
#define OY 34
#define WHITE 0xeaf6ff
#define CYAN 0x48dcff
#define CORAL 0xff637e
static void ring(float x,float y,float radius,uint32_t color) {
    for(int i=0;i<20;i++) {
        float a=i*6.2831853f/20;
        float px=x+cosf(a)*radius-1,py=y+sinf(a)*radius-1;
        if(px>=OX && px<=OX+ARENA_W-2 && py>=OY && py<=OY+ARENA_H-2)
            rect(px,py,2,2,color);
    }
}
static void panel(const char *title,const char *line1,const char *line2) {
    rect(37,82,246,78,0x243449); rect(39,84,242,74,0x09111f);
    label(52,102,0,title); label(52,125,1,line1); label(52,146,1,line2);
}
void ui_draw(const Game *g) {
    rect(0,0,320,240,0x070c17);
    label(16,20,0,"PLASMA PONG");
    char s[48]; snprintf(s,sizeof(s),"%u  :  %u",g->score[0],g->score[1]);
    label(145,20,0,s); label(233,20,1,"FIRST TO 9");
    draw_fluid(&g->fluid);
    rect(16,32,288,1,0x304c65); rect(16,214,288,1,0x304c65);
    for(int y=39;y<211;y+=12) rect(159,y,1,4,0x23374e);
    rect(13,34,2,180,0x24566c); rect(305,34,2,180,0x71334c);
    for(int p=0;p<2;p++) {
        const Bat *b=&g->bat[p]; uint32_t c=p?CORAL:CYAN;
        float x=OX+b->x,y=OY+b->y;
        if(b->sucking) {
            ring(x,y,24+2*sinf(g->elapsed*7),c);
            rect(x-9,y+BAT_HALF+5,18*b->charge,2,c);
        }
        if(b->burst>0) ring(x,y,12+(1-b->burst/.25f)*30,c);
        rect(x-5,y-BAT_HALF-2,10,BAT_HALF*2+4,p?0x642739:0x164658);
        rect(x-3,y-BAT_HALF,6,BAT_HALF*2,c);
        rect(x-1,y-BAT_HALF+2,2,BAT_HALF*2-4,WHITE);
        label(p?250:21,46,p?3:2,p?"P2":"P1");
    }
    float bx=OX+g->bx,by=OY+g->by;
    if(g->serve<=0 && g->held<0) {
        for(int i=4;i>0;i--) {
            float x=maxf(OX,minf(OX+ARENA_W-2,bx-g->bvx*i*.016f));
            float y=maxf(OY,minf(OY+ARENA_H-2,by-g->bvy*i*.016f));
            rect(x-1,y-1,2,2,0x556b84);
        }
    }
    rect(bx-4,by-4,8,8,g->held<0?0x447486:0xffd875);
    rect(bx-2,by-3,4,6,WHITE); rect(bx-3,by-2,6,4,WHITE);
    label(16,230,1,"STICK MOVE   Z JET   HOLD A GRAB / RELEASE");
    if(g->phase==LOBBY) {
        if(!g->connected[0] || !g->connected[1])
            panel("TWO PLAYERS REQUIRED","Connect N64 pads to ports 1 + 2","Then press START to play");
        else panel("STIR. GRAB. LAUNCH.","Cyan vs coral. First to 9 wins.","Press START to play");
    } else if(g->phase==PAUSED) {
        if(!g->connected[0] || !g->connected[1])
            panel("CONTROLLER DISCONNECTED","Reconnect pads to ports 1 + 2","Press START to resume");
        else panel("PAUSED","Z: jet    Hold A: suction","Press START to resume");
    } else if(g->phase==FINISHED) {
        snprintf(s,sizeof(s),"PLAYER %d WINS!",g->winner+1);
        panel(s,"The currents have a champion.","Press START for a rematch");
    } else if(g->serve>0) {
        label(126,83,0,g->serve>.6f?"GET READY":"SERVE!");
    }
}
