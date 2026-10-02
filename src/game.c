#include "mathutil.h"
#include "game.h"
#include <math.h>
#include <string.h>
static float axis(float a) { return fabsf(a)<.12f?0:clampf(a,-1,1); }
static void serve(Game *g,int dir) {
    g->bx=ARENA_W*.5f; g->by=ARENA_H*.5f;
    g->bvx=dir*108; g->bvy=(g->rally%2?1:-1)*31;
    g->serve=1.2f; g->serve_dir=dir; g->held=-1; g->rally++;
}
void game_init(Game *g) {
    memset(g,0,sizeof(*g));
    g->bat[0]=(Bat){.x=20,.y=ARENA_H*.5f};
    g->bat[1]=(Bat){.x=ARENA_W-20,.y=ARENA_H*.5f};
    g->phase=MENU; g->menu_rng=0x76a51c93u; g->winner=-1; serve(g,1);
}
static float menu_random(Game *g) {
    uint32_t x=g->menu_rng?g->menu_rng:1;
    x^=x<<13; x^=x>>17; x^=x<<5; g->menu_rng=x;
    return (x&65535)/65535.0f;
}
static void menu_step(Game *g) {
    /* Slowly drifting emitters keep the menu alive even with no controllers. */
    if(g->menu_ticks%60==0) for(int p=0;p<3;p++) {
        float angle=menu_random(g)*6.2831853f;
        g->menu_current[p]=(MenuCurrent){24+menu_random(g)*(ARENA_W-48),
            24+menu_random(g)*(ARENA_H-48),cosf(angle)*32,sinf(angle)*32};
    }
    for(int p=0;p<3;p++) {
        MenuCurrent *c=&g->menu_current[p];
        c->x=clampf(c->x+c->u*STEP*.2f,16,ARENA_W-16);
        c->y=clampf(c->y+c->v*STEP*.2f,16,ARENA_H-16);
        fluid_splat(&g->fluid,c->x,c->y,26,c->u*.65f,c->v*.65f,p==2?0:.10f,p&1);
        if(p==2) fluid_ball_dye(&g->fluid,c->x,c->y,.20f);
    }
    fluid_velocity_step(&g->fluid,STEP); fluid_dye_step(&g->fluid,STEP);
    g->menu_ticks++; g->elapsed+=STEP;
}
static void limit_ball(Game *g) {
    float speed=sqrtf(g->bvx*g->bvx+g->bvy*g->bvy);
    if(speed>290) { g->bvx*=290/speed; g->bvy*=290/speed; }
}
static void ball_step(Game *g) {
    if(g->serve>0) { g->serve=maxf(0,g->serve-STEP); return; }
    if(g->held>=0) {
        Bat *b=&g->bat[g->held];
        g->bx=b->x+(g->held?-1:1)*8; g->by=b->y;
        g->bvx=b->vx; g->bvy=b->vy; return;
    }
    /* Four short collision steps prevent tunnelling through bats at jet speed. */
    const float dt=STEP/4;
    for(int step=0;step<4;step++) {
        float u,v; fluid_sample(&g->fluid,g->bx,g->by,&u,&v);
        /* Lift/drag from the actual velocity field, with enough inertia for Pong. */
        g->bvx+=(u-g->bvx*.22f)*1.35f*dt;
        g->bvy+=(v-g->bvy*.22f)*1.35f*dt;
        if(fabsf(g->bvx)<45) g->bvx+=(g->bvx<0?-1:1)*24*dt;
        limit_ball(g);
        /* Deposit a little dye along the travelled path, never during a serve
           countdown or while held. It will be carried by the same current. */
        fluid_ball_dye(&g->fluid,g->bx,g->by,.065f);
        float oldx=g->bx;
        g->bx+=g->bvx*dt; g->by+=g->bvy*dt;
        if(g->by<BALL_RADIUS) { g->by=BALL_RADIUS; g->bvy=fabsf(g->bvy); g->sound_events|=SOUND_WALL; }
        if(g->by>ARENA_H-BALL_RADIUS) { g->by=ARENA_H-BALL_RADIUS; g->bvy=-fabsf(g->bvy); g->sound_events|=SOUND_WALL; }
        for(int p=0;p<2;p++) {
            Bat *b=&g->bat[p]; int dir=p?-1:1;
            float dx=g->bx-b->x,dy=g->by-b->y;
            float rvx=g->bvx-b->vx,rvy=g->bvy-b->vy;
            /* Strong currents and fast shots beat the grab; suction must slow them. */
            if(b->sucking && dx*dir>=0 && dx*dx+dy*dy<18*18 &&
               rvx*rvx+rvy*rvy<145*145 && u*u+v*v<210*210) {
                g->held=p; g->bx=b->x+dir*8; g->by=b->y; return;
            }
            float face=b->x+dir*6;
            bool crossed=(oldx-face)*dir>=0 && (g->bx-face)*dir<=0;
            bool overlap=fabsf(dx)<6 && fabsf(dy)<BAT_HALF+BALL_RADIUS;
            if((crossed||overlap) && fabsf(dy)<BAT_HALF+BALL_RADIUS && rvx*dir<0) {
                g->sound_events|=p?SOUND_BAT2:SOUND_BAT1;
                g->bx=face; g->bvx=dir*maxf(108,fabsf(g->bvx)*1.04f);
                g->bvy+=dy*3.8f+b->vy*.3f; limit_ball(g);
                fluid_splat(&g->fluid,g->bx,g->by,13,dir*35,b->vy*.2f,.35f,p);
            }
        }
        if(g->bx<-BALL_RADIUS || g->bx>ARENA_W+BALL_RADIUS) {
            int scorer=g->bx<0?1:0;
            g->score[scorer]++; g->sound_events|=SOUND_GOAL;
            if(g->score[scorer]>=9) { g->phase=FINISHED; g->winner=scorer; g->sound_events|=SOUND_WIN; }
            serve(g,scorer?1:-1); return;
        }
    }
}
void game_step(Game *g,const Input in[2]) {
    g->sound_events=0;
    bool start=false,confirm=false,back=false;
    for(int p=0;p<2;p++) {
        g->connected[p]=in[p].connected;
        start|=in[p].connected && in[p].start && !g->previous[p].start;
        confirm|=in[p].connected && in[p].a && !g->previous[p].a;
        back|=in[p].connected && in[p].b && !g->previous[p].b;
    }
    bool both=in[0].connected && in[1].connected;
    if(g->phase==MENU) {
        menu_step(g);
        if(start || confirm) g->phase=LOBBY;
        memcpy(g->previous,in,sizeof(g->previous)); return;
    }
    if(back && (g->phase==LOBBY || g->phase==PAUSED || g->phase==FINISHED)) {
        g->phase=MENU; g->held=-1;
        for(int p=0;p<2;p++) g->bat[p].sucking=false;
        memcpy(g->previous,in,sizeof(g->previous)); return;
    }
    if(!both && g->phase==PLAY) g->phase=PAUSED;
    if(start && both) {
        if(g->phase==LOBBY || g->phase==FINISHED) {
            game_init(g); g->phase=PLAY;
            g->connected[0]=g->connected[1]=true;
        } else g->phase=g->phase==PLAY?PAUSED:PLAY;
        memcpy(g->previous,in,sizeof(g->previous)); return;
    }
    if(g->phase!=PLAY) { memcpy(g->previous,in,sizeof(g->previous)); return; }
    g->elapsed+=STEP;
    for(int p=0;p<2;p++) {
        Bat *b=&g->bat[p]; int dir=p?-1:1;
        float x=b->x,y=b->y;
        b->x=clampf(x+axis(in[p].x)*92*STEP,p?ARENA_W-64:12,p?ARENA_W-12:64);
        b->y=clampf(y-axis(in[p].y)*125*STEP,BAT_HALF+2,ARENA_H-BAT_HALF-2);
        b->vx=(b->x-x)/STEP; b->vy=(b->y-y)/STEP;
        fluid_splat(&g->fluid,b->x,b->y,20,b->vx*.20f,b->vy*.20f,
                    (fabsf(b->vx)+fabsf(b->vy))*STEP*.008f,p);
        b->burst=maxf(0,b->burst-STEP);
        if(in[p].z) {
            fluid_splat(&g->fluid,b->x+dir*14,b->y,22,dir*1150*STEP,b->vy*.08f,2.6f*STEP,p);
        }
    }
    fluid_velocity_step(&g->fluid,STEP);
    for(int p=0;p<2;p++) {
        Bat *b=&g->bat[p]; int dir=p?-1:1;
        if(in[p].a) {
            b->sucking=true; b->charge=minf(1,b->charge+STEP);
            fluid_pump(&g->fluid,b->x+dir*7,b->y,35,-1150,STEP,p);
        } else if(b->sucking) {
            b->sucking=false; b->burst=.25f;
            fluid_pump(&g->fluid,b->x,b->y,40,2800,.07f+b->charge*.06f,p);
            fluid_splat(&g->fluid,b->x+dir*12,b->y,27,dir*(130+b->charge*110),b->vy*.35f,1,p);
            if(g->held==p) {
                g->held=-1; g->bx=b->x+dir*9; g->by=b->y;
                g->bvx=dir*(180+b->charge*70); g->bvy=b->vy*.55f;
            }
            b->charge=0;
        }
    }
    fluid_dye_step(&g->fluid,STEP);
    ball_step(g);
    memcpy(g->previous,in,sizeof(g->previous));
}
