#include "mathutil.h"
#include "game.h"
#include <math.h>
#include <string.h>
static float axis(float a) { return fabsf(a)<.12f?0:clampf(a,-1,1); }
static void serve(Game *g,int dir) {
    g->bx=ARENA_W*.5f; g->by=ARENA_H*.5f;
    g->bvx=dir*(g->mode==ARCADE?108+72*arcade_difficulty(g):108); g->bvy=(g->rally%2?1:-1)*31;
    g->serve=1.2f; g->serve_dir=dir; g->held=-1; g->rally++;
}
void game_init(Game *g) {
    memset(g,0,sizeof(*g));
    g->bat[0]=(Bat){.x=20,.y=ARENA_H*.5f};
    g->bat[1]=(Bat){.x=ARENA_W-20,.y=ARENA_H*.5f};
    g->score_entry=-1; g->arcade.level=1;
    g->phase=MENU; g->menu_rng=0x76a51c93u; g->winner=-1; serve(g,1);
}
/* Visual-only state and RNG never affect the fluid or gameplay randomness. */
static float tracer_random(Game *g) {
    uint32_t x=g->tracer_rng?g->tracer_rng:0x513ad291u;
    x^=x<<13; x^=x>>17; x^=x<<5; g->tracer_rng=x;
    return (x&65535)/65536.0f;
}
void game_flow_step(Game *g) {
    if(g->flow_effect!=FLOW_PARTICLES && g->flow_effect!=FLOW_TAILS) return;
    for(unsigned i=0;i<FLOW_TRACERS;i++) {
        FlowTracer *t=&g->tracers[i];
        if(!t->life) {
            /* Stratified respawns retain coverage while allowing free advection. */
            t->x[0]=(i%12+tracer_random(g))*(ARENA_W/12);
            t->y[0]=(i/12+tracer_random(g))*(ARENA_H/8);
            t->life=45+(unsigned)(tracer_random(g)*90);
            for(int j=1;j<3;j++) { t->x[j]=t->x[0]; t->y[j]=t->y[0]; }
            continue;
        }
        float u,v; fluid_sample(&g->fluid,t->x[0],t->y[0],&u,&v);
        for(int j=2;j>0;j--) { t->x[j]=t->x[j-1]; t->y[j]=t->y[j-1]; }
        t->x[0]+=u*STEP; t->y[0]+=v*STEP; t->life--;
        if(t->x[0]<0 || t->x[0]>=ARENA_W || t->y[0]<0 || t->y[0]>=ARENA_H) t->life=0;
    }
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
    game_flow_step(g);
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
        if(game_ball_hot(g)) fluid_hot_ball_dye(&g->fluid,g->bx,g->by,.13f);
        else fluid_ball_dye(&g->fluid,g->bx,g->by,.065f);
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
            if(g->mode==ARCADE) arcade_goal(g,scorer);
            else if(g->score[scorer]>=9) { g->phase=FINISHED; g->winner=scorer; g->sound_events|=SOUND_WIN; }
            serve(g,scorer?1:-1); return;
        }
    }
}
static int direction(float value) { return value>.5f?1:value<-.5f?-1:0; }
static void start_game(Game *g) {
    HighScore saved[HIGH_SCORE_COUNT]; memcpy(saved,g->highs,sizeof(saved));
    FlowEffect effect=g->flow_effect;
    GameMode mode=g->mode; uint32_t seed=g->menu_rng;
    bool available=g->save_available,failed=g->save_failed;
    game_init(g); memcpy(g->highs,saved,sizeof(saved));
    g->flow_effect=effect;
    g->save_available=available; g->save_failed=failed;
    g->mode=mode; g->menu_selection=mode==ARCADE?1:0;
    g->menu_rng=seed; g->phase=PLAY;
    g->arcade=(Arcade){.level=1,.lives=3,.rng=0x706f6e67u,.ai_y=ARENA_H*.5f};
    g->connected[0]=true; g->connected[1]=mode==MULTIPLAYER;
}
void game_step(Game *g,const Input physical[2]) {
    Input effective[2]={physical[0],physical[1]};
    const Input *in=effective;
    g->sound_events=0;
    bool start=false,confirm=false,back=false;
    for(int p=0;p<2;p++) {
        g->connected[p]=physical[p].connected;
        if(g->mode==ARCADE && g->phase!=MENU && g->phase!=OPTIONS && p==1) continue;
        start|=in[p].connected && in[p].start && !g->previous[p].start;
        confirm|=in[p].connected && in[p].a && !g->previous[p].a;
        back|=in[p].connected && in[p].b && !g->previous[p].b;
    }
    bool both=in[0].connected && (g->mode==ARCADE || in[1].connected);
    if(g->phase==MENU) {
        menu_step(g);
        for(int p=0;p<2;p++) if(in[p].connected) {
            int nav=direction(in[p].y);
            if(nav && nav!=direction(g->previous[p].y)) {
                g->menu_selection=(g->menu_selection+(nav>0?3:1))%4;
                g->sound_events|=SOUND_SELECT; break;
            }
        }
        if(start || confirm) {
            if(g->menu_selection==3) g->phase=OPTIONS;
            else if(g->menu_selection==2) g->phase=SCORES;
            else { g->mode=g->menu_selection==1?ARCADE:MULTIPLAYER; g->phase=LOBBY; }
            g->sound_events|=SOUND_SELECT;
        }
        memcpy(g->previous,in,sizeof(g->previous)); return;
    }
    if(g->phase==OPTIONS) {
        menu_step(g);
        if(back || start) { g->phase=MENU; g->sound_events|=SOUND_BACK; }
        else for(int p=0;p<2;p++) if(in[p].connected) {
            int nav=direction(in[p].x);
            if(nav && nav!=direction(g->previous[p].x)) {
                g->flow_effect=(g->flow_effect+(nav>0?1:FLOW_COUNT-1))%FLOW_COUNT;
                memset(g->tracers,0,sizeof(g->tracers));
                g->scores_dirty=true; g->sound_events|=SOUND_SELECT; break;
            }
        }
        memcpy(g->previous,in,sizeof(g->previous)); return;
    }
    if(g->phase==SCORES) {
        if(back || start || confirm) { g->phase=MENU; g->sound_events|=SOUND_BACK; }
        memcpy(g->previous,in,sizeof(g->previous)); return;
    }
    if(g->mode==ARCADE && g->phase==FINISHED) {
        if(g->score_entry>=0 && in[0].connected) {
            int x=direction(in[0].x),y=direction(in[0].y);
            if(x && x!=direction(g->previous[0].x)) g->initial_cursor=(g->initial_cursor+(x>0?1:2))%3;
            if(y && y!=direction(g->previous[0].y)) {
                char *c=&g->highs[g->score_entry].initials[g->initial_cursor];
                *c='A'+(*c-'A'+(y>0?1:25))%26;
            }
        }
        if(start || confirm || back) {
            g->scores_dirty=g->score_entry>=0;
            g->phase=SCORES; g->sound_events|=SOUND_SELECT;
        }
        memcpy(g->previous,in,sizeof(g->previous)); return;
    }
    if(back && (g->phase==LOBBY || g->phase==PAUSED || g->phase==FINISHED)) {
        g->phase=MENU; g->held=-1; g->sound_events|=SOUND_BACK;
        for(int p=0;p<2;p++) g->bat[p].sucking=false;
        memcpy(g->previous,in,sizeof(g->previous)); return;
    }
    if(!both && g->phase==PLAY) g->phase=PAUSED;
    if(start && both) {
        if(g->phase==LOBBY || g->phase==FINISHED) {
            start_game(g);
        } else g->phase=g->phase==PLAY?PAUSED:PLAY;
        g->sound_events|=g->phase==PAUSED?SOUND_BACK:SOUND_SELECT;
        memcpy(g->previous,in,sizeof(g->previous)); return;
    }
    if(g->phase!=PLAY) { memcpy(g->previous,in,sizeof(g->previous)); return; }
    if(g->mode==ARCADE) {
        if(g->arcade.transition>0) {
            g->arcade.transition=maxf(0,g->arcade.transition-STEP);
            memcpy(g->previous,in,sizeof(g->previous)); return;
        }
        effective[1]=arcade_ai(g);
        if(g->serve<=0) g->arcade.level_time+=STEP;
        if(g->held>=0) g->arcade.hold_time+=STEP; else g->arcade.hold_time=0;
    }
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
    if(g->mode==ARCADE) arcade_currents(g);
    fluid_velocity_step(&g->fluid,STEP);
    for(int p=0;p<2;p++) {
        Bat *b=&g->bat[p]; int dir=p?-1:1;
        if(b->cooldown_ticks) b->cooldown_ticks--;
        if(!in[p].a) b->release_required=false;
        if(b->cooldown_ticks || b->release_required) continue;
        if(in[p].a) {
            b->suction_ticks++;
            if(b->suction_ticks>=SUCTION_BREAK_TICKS) {
                b->sucking=false; b->charge=0; b->burst=0; b->suction_ticks=0;
                b->cooldown_ticks=SUCTION_COOLDOWN_TICKS; b->release_required=true;
                g->sound_events|=p?SOUND_BREAK2:SOUND_BREAK1;
                if(g->held==p) {
                    g->held=-1; g->bx=b->x+dir*9; g->by=b->y;
                    /* Drop into the existing current without a release impulse. */
                    fluid_sample(&g->fluid,g->bx,g->by,&g->bvx,&g->bvy);
                }
                continue;
            }
            b->sucking=true; b->charge=minf(1,(float)b->suction_ticks/SUCTION_CHARGE_TICKS);
            fluid_pump(&g->fluid,b->x+dir*7,b->y,35,-1150,STEP,p);
        } else if(b->sucking) {
            b->sucking=false; b->burst=.25f;
            /* Spend only stored charge: tapping must not create a free impulse. */
            fluid_pump(&g->fluid,b->x,b->y,40,2800,.13f*b->charge,p);
            fluid_splat(&g->fluid,b->x+dir*12,b->y,27,dir*240*b->charge,
                        b->vy*.35f*b->charge,b->charge,p);
            if(g->held==p) {
                g->held=-1; g->bx=b->x+dir*9; g->by=b->y;
                /* Perfect timing earns a clear jump to the ball's speed cap. */
                g->bvx=dir*(b->charge>=1?290:200*b->charge); g->bvy=b->vy*.55f;
            }
            b->charge=0; b->suction_ticks=0;
        }
    }
    fluid_dye_step(&g->fluid,STEP);
    ball_step(g);
    game_flow_step(g);
    memcpy(g->previous,physical,sizeof(g->previous));
    if(g->mode==ARCADE) g->previous[1]=effective[1];
}
