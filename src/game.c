#include "mathutil.h"
#include "game.h"
#include <math.h>
#include <string.h>
static float axis(float a) { return fabsf(a)<.12f?0:clampf(a,-1,1); }
/* Normal points into the court; tangent follows screen coordinates. */
static float normal_x(int p) { return p<2?(p?-1:1):0; }
static float normal_y(int p) { return p<2?0:(p==2?-1:1); }
static void place_bats(Game *g) {
    g->bat[0]=(Bat){.x=game_left(g)+20,.y=ARENA_H*.5f};
    g->bat[1]=(Bat){.x=game_right(g)-20,.y=ARENA_H*.5f};
    g->bat[2]=(Bat){.x=ARENA_W*.5f,.y=ARENA_H-20};
    g->bat[3]=(Bat){.x=ARENA_W*.5f,.y=20};
}
static void attach_ball(Game *g,int p,float distance) {
    g->bx=g->bat[p].x+normal_x(p)*distance;
    g->by=g->bat[p].y+normal_y(p)*distance;
}
static void serve(Game *g) {
    g->bx=ARENA_W*.5f; g->by=ARENA_H*.5f;
    /* Let the fluid and players' jets determine the opening motion. */
    g->bvx=0; g->bvy=0;
    g->serve=1.2f; g->held=-1;
}
void game_init(Game *g) {
    memset(g,0,sizeof(*g));
    g->players=2; g->frame_rate=FPS_60;
    const float menu_widths[]={134,99,89,59};
    memcpy(g->menu_label_widths,menu_widths,sizeof(menu_widths));
    for(unsigned p=0;p<MAX_PLAYERS;p++) { g->player_port[p]=p; g->lives[p]=MULTIPLAYER_LIVES; }
    place_bats(g);
    g->score_entry=-1; g->arcade.level=1;
    g->phase=MENU; g->menu_rng=0x76a51c93u; g->winner=-1; serve(g);
}
/* Visual-only state and RNG never affect the fluid or gameplay randomness. */
static float tracer_random(Game *g) {
    uint32_t x=g->tracer_rng?g->tracer_rng:0x513ad291u;
    x^=x<<13; x^=x>>17; x^=x<<5; g->tracer_rng=x;
    return (x&65535)/65536.0f;
}
void game_flow_step(Game *g) {
    const float step=game_dt(g);
    if(g->flow_effect!=FLOW_PARTICLES && g->flow_effect!=FLOW_TAILS) return;
    unsigned old=g->tracer_head;
    unsigned ticks=game_tick_units(g);
    unsigned head=g->tracer_head=(old+FLOW_HISTORY-ticks)%FLOW_HISTORY;
    for(unsigned i=0;i<FLOW_TRACERS;i++) {
        FlowTracer *t=&g->tracers[i];
        if(!t->life) {
            /* Stratified respawns retain coverage while allowing free advection. */
            g->tracer_history[0][i].x=(i%12+tracer_random(g))*(ARENA_W/12);
            g->tracer_history[0][i].y=(i/12+tracer_random(g))*(ARENA_H/8);
            t->life=GAME_HZ*3/2+(unsigned)(tracer_random(g)*(3*GAME_HZ));
            for(int j=1;j<FLOW_HISTORY;j++) { g->tracer_history[j][i].x=g->tracer_history[0][i].x; g->tracer_history[j][i].y=g->tracer_history[0][i].y; }
            continue;
        }
        float u,v; fluid_sample(&g->fluid,g->tracer_history[old][i].x,g->tracer_history[old][i].y,&u,&v);
        g->tracer_history[head][i].x=g->tracer_history[old][i].x+u*step; g->tracer_history[head][i].y=g->tracer_history[old][i].y+v*step; t->life=t->life>ticks?t->life-ticks:0;
        if(g->tracer_history[head][i].x<0 || g->tracer_history[head][i].x>=ARENA_W || g->tracer_history[head][i].y<0 || g->tracer_history[head][i].y>=ARENA_H) t->life=0;
    }
}
static float menu_random(Game *g) {
    uint32_t x=g->menu_rng?g->menu_rng:1;
    x^=x<<13; x^=x>>17; x^=x<<5; g->menu_rng=x;
    return (x&65535)/65535.0f;
}
static void menu_step(Game *g) {
    const float step=game_dt(g); const float emission=game_emission(g);
    /* Soften menu dye without changing the currents or gameplay effects. */
    const float dye_strength=g->phase==MENU?.70f:1.0f;
    /* Slowly drifting emitters keep the menu alive even with no controllers. */
    if(g->menu_ticks%(2*GAME_HZ)<game_tick_units(g)) for(int p=0;p<3;p++) {
        float angle=menu_random(g)*6.2831853f;
        g->menu_current[p]=(MenuCurrent){24+menu_random(g)*(ARENA_W-48),
            24+menu_random(g)*(ARENA_H-48),cosf(angle)*32,sinf(angle)*32};
    }
    for(int p=0;p<3;p++) {
        MenuCurrent *c=&g->menu_current[p];
        c->x=clampf(c->x+c->u*step*.2f,16,ARENA_W-16);
        c->y=clampf(c->y+c->v*step*.2f,16,ARENA_H-16);
        fluid_splat(&g->fluid,c->x,c->y,26,c->u*.65f*emission,c->v*.65f*emission,p==2?0:.10f*emission*dye_strength,p&1);
        if(p==2 && g->phase!=MENU) fluid_ball_dye(&g->fluid,c->x,c->y,.20f*emission);
    }
    if(g->phase==MENU) {
        /* The active row is the menu's only gold source. Old trails fade so
           moving the selection leaves one clear, continuously emitting row. */
        FluidInk *ink=fluid_dye(&g->fluid);
        /* sqrt(.94): same menu fade per second at twice the update rate. */
        for(int k=0;k<FN;k++) ink->gold[k]=fluid_ink_encode(fluid_ink_decode(ink->gold[k])*.969535971f);
        /* Inset sources by their eight-unit radius, so their footprint fits
           the measured label instead of extending beyond both ends. */
        float width=maxf(0,g->menu_label_widths[g->menu_selection]-16*(320/ARENA_W));
        float y=(MENU_FIRST_ROW+MENU_ROW_SPACING*g->menu_selection-4)*(ARENA_H/240);
        for(int i=0;i<=12;i++) {
            float t=i/12.0f;
            float x=(160+(t-.5f)*width)*(ARENA_W/320);
            float drift=sinf(g->elapsed*2.3f+i*.9f);
            for(int side=-1;side<=1;side+=2) {
                float sy=y+side*3;
#ifdef PLASMAPONG_MENU_STAMPS
                fluid_menu_source(&g->fluid,i*2+(side>0),x,sy,(t-.5f)*5*emission,
                    (side*4+drift*2)*emission,.48f*emission*dye_strength);
#else
                fluid_splat(&g->fluid,x,sy,12,(t-.5f)*5*emission,(side*4+drift*2)*emission,0,0);
                fluid_ball_dye(&g->fluid,x,sy,.48f*emission*dye_strength);
#endif
            }
        }
    }
    fluid_velocity_step(&g->fluid,step); fluid_dye_step(&g->fluid,step);
    if(g->phase==MENU) {
        /* Currents can carry gold beyond the label. Fade its edges inside the
           measured bounds, including half a texture cell for filtering. */
        FluidInk *ink=fluid_dye(&g->fluid);
        float half_width=g->menu_label_widths[g->menu_selection]*.5f*(ARENA_W/320);
        for(int x=0;x<FW;x++) {
            float edge=half_width-fabsf((x+.5f)*CELL-ARENA_W*.5f)-CELL*.5f;
            float fade=clampf(edge/CELL,0,1);
            if(fade==0) {
                for(int y=0;y<FH;y++) ink->gold[y*FW+x]=0;
                continue;
            }
            /* Positive menu dye is unchanged at unit fade. Avoid decoding
               and re-encoding the entire interior of the selected label. */
            if(fade==1) continue;
            for(int y=0;y<FH;y++) {
                int k=y*FW+x;
                ink->gold[k]=fluid_ink_encode(fluid_ink_decode(ink->gold[k])*fade);
            }
        }
    }
    game_flow_step(g);
    g->menu_ticks+=game_tick_units(g); g->elapsed+=step;
}
static void limit_ball(Game *g) {
    float speed=sqrtf(g->bvx*g->bvx+g->bvy*g->bvy);
    if(speed>290) { g->bvx*=290/speed; g->bvy*=290/speed; }
}
static void corner_bounce(Game *g) {
    if(!game_square(g)) return;
    for(int corner=0;corner<4;corner++) {
        float nx=corner&1?-1:1,ny=corner&2?-1:1;
        float x=corner&1?game_right(g)-g->bx:g->bx-game_left(g);
        float y=corner&2?ARENA_H-g->by:g->by;
        /* Unit diagonal normal is (nx,ny)/sqrt(2). Keep the whole ball inside. */
        float penetration=CORNER_SIZE+BALL_RADIUS*1.41421356f-x-y;
        if(penetration<=0) continue;
        g->bx+=nx*penetration*.5f; g->by+=ny*penetration*.5f;
        float velocity=g->bvx*nx+g->bvy*ny;
        if(velocity<0) {
            g->bvx-=nx*velocity; g->bvy-=ny*velocity;
            g->sound_events|=SOUND_WALL;
        }
    }
}
static void ball_step(Game *g) {
    const float step=game_dt(g); const float emission=game_emission(g);
    if(g->serve>0) { g->serve=maxf(0,g->serve-step); return; }
    if(g->held>=0) {
        Bat *b=&g->bat[g->held];
        attach_ball(g,g->held,8);
        g->bvx=b->vx; g->bvy=b->vy; return;
    }
    /* Four short collision steps prevent tunnelling through bats at jet speed. */
    const float dt=step/4;
    /* Arcade currents accelerate the ball more as levels rise, approaching
       three times the opening response. Keep still-water drag unchanged. */
    const float current_response=1.7f*(g->mode==ARCADE?1+2*arcade_difficulty(g):1);
    for(int step=0;step<4;step++) {
        float u,v; fluid_sample(&g->fluid,g->bx,g->by,&u,&v);
        g->bvx+=(u*current_response-g->bvx*.297f)*dt;
        g->bvy+=(v*current_response-g->bvy*.297f)*dt;
        limit_ball(g);
        /* Deposit a little dye along the travelled path, never during a serve
           countdown or while held. It will be carried by the same current. */
        if(game_ball_hot(g)) fluid_hot_ball_dye(&g->fluid,g->bx,g->by,.13f*emission);
        else fluid_ball_dye(&g->fluid,g->bx,g->by,.065f*emission);
        float oldx=g->bx,oldy=g->by;
        g->bx+=g->bvx*dt; g->by+=g->bvy*dt;
        bool top=game_players(g)<4 || !game_alive(g,3);
        bool bottom=game_players(g)<3 || !game_alive(g,2);
        if(top && g->by<BALL_RADIUS) { g->by=BALL_RADIUS; g->bvy=fabsf(g->bvy); g->sound_events|=SOUND_WALL; }
        if(bottom && g->by>ARENA_H-BALL_RADIUS) { g->by=ARENA_H-BALL_RADIUS; g->bvy=-fabsf(g->bvy); g->sound_events|=SOUND_WALL; }
        if(!game_alive(g,0) && g->bx<game_left(g)+BALL_RADIUS) {
            g->bx=game_left(g)+BALL_RADIUS; g->bvx=fabsf(g->bvx); g->sound_events|=SOUND_WALL;
        }
        if(!game_alive(g,1) && g->bx>game_right(g)-BALL_RADIUS) {
            g->bx=game_right(g)-BALL_RADIUS; g->bvx=-fabsf(g->bvx); g->sound_events|=SOUND_WALL;
        }
        for(unsigned p=0;p<game_players(g);p++) {
            if(!game_alive(g,p)) continue;
            Bat *b=&g->bat[p]; float nx=normal_x(p),ny=normal_y(p);
            float dx=g->bx-b->x,dy=g->by-b->y;
            float rvx=g->bvx-b->vx,rvy=g->bvy-b->vy;
            float normal=dx*nx+dy*ny,tangent=p<2?dy:dx;
            float half=game_bat_half(g,p),capture=half+4;
            if(b->sucking && normal>=0 && dx*dx+dy*dy<capture*capture &&
               rvx*rvx+rvy*rvy<145*145 && u*u+v*v<210*210) {
                g->held=p; attach_ball(g,p,8); return;
            }
            float oldnormal=(oldx-b->x)*nx+(oldy-b->y)*ny;
            bool crossed=oldnormal>=6 && normal<=6;
            bool overlap=fabsf(normal)<6;
            if((crossed||overlap) && fabsf(tangent)<half+BALL_RADIUS && rvx*nx+rvy*ny<0) {
                g->sound_events|=p==0?SOUND_BAT1:p==1?SOUND_BAT2:SOUND_BAT_OTHER;
                float bounce=maxf(108,fabsf(g->bvx*nx+g->bvy*ny)*1.04f);
                if(p<2) { g->bx=b->x+nx*6; g->bvx=nx*bounce; g->bvy+=tangent*3.8f+b->vy*.3f; }
                else { g->by=b->y+ny*6; g->bvy=ny*bounce; g->bvx+=tangent*3.8f+b->vx*.3f; }
                limit_ball(g);
                fluid_splat(&g->fluid,g->bx,g->by,13,nx*35+(p<2?0:b->vx*.2f),ny*35+(p<2?b->vy*.2f:0),.35f,game_player_palette(g,p));
            }
        }
        corner_bounce(g);
        int missed=g->bx<game_left(g)-BALL_RADIUS?0:g->bx>game_right(g)+BALL_RADIUS?1:
            g->by>ARENA_H+BALL_RADIUS?2:g->by<-BALL_RADIUS?3:-1;
        if(missed>=0) {
            g->sound_events|=SOUND_GOAL;
            if(game_square(g)) {
                if(g->lives[missed]) g->lives[missed]--;
                if(!g->lives[missed]) g->bat[missed]=(Bat){0};
                unsigned remaining=0; int survivor=-1;
                for(unsigned p=0;p<g->players;p++) if(g->lives[p]) { remaining++; survivor=p; }
                if(remaining==1) { g->phase=FINISHED; g->winner=survivor; g->sound_events|=SOUND_WIN; }
            } else {
                int scorer=1-missed;
                g->score[scorer]++;
                if(g->mode==ARCADE) arcade_goal(g,scorer);
                else if(g->score[scorer]>=9) { g->phase=FINISHED; g->winner=scorer; g->sound_events|=SOUND_WIN; }
            }
            serve(g); return;
        }
    }
}
static int direction(float value) { return value>.5f?1:value<-.5f?-1:0; }
static void start_game(Game *g) {
    HighScore saved[HIGH_SCORE_COUNT]; memcpy(saved,g->highs,sizeof(saved));
    FlowEffect effect=g->flow_effect; FrameRate frame_rate=g->frame_rate;
    GameMode mode=g->mode; uint32_t seed=g->menu_rng;
    bool available=g->save_available,failed=g->save_failed;
    unsigned players=g->players,ports[MAX_PLAYERS]; memcpy(ports,g->player_port,sizeof(ports));
    game_init(g); memcpy(g->highs,saved,sizeof(saved));
    g->players=players; memcpy(g->player_port,ports,sizeof(ports));
    g->flow_effect=effect; g->frame_rate=frame_rate;
    g->save_available=available; g->save_failed=failed;
    g->mode=mode; place_bats(g); g->menu_selection=mode==ARCADE?1:0;
    g->menu_rng=seed; g->phase=PLAY;
    g->arcade=(Arcade){.level=1,.lives=3,.rng=0x706f6e67u,.ai_y=ARENA_H*.5f};
    for(unsigned p=0;p<game_players(g);p++) g->connected[p]=mode==MULTIPLAYER || p==0;
}
void game_step(Game *g,const Input physical[MAX_PLAYERS]) {
    const float step=game_dt(g); const float emission=game_emission(g);
    const float ticks_per_second=GAME_HZ;
    Input effective[MAX_PLAYERS];
    bool menu=g->phase==MENU || g->phase==OPTIONS || g->phase==SCORES;
    unsigned count=0;
    for(unsigned p=0;p<MAX_PLAYERS;p++) {
        if(physical[p].connected) count++;
        effective[p]=physical[menu?p:g->player_port[p]];
        g->connected[p]=effective[p].connected;
    }
    const Input *in=effective;
    g->sound_events=0;
    bool start=false,confirm=false,back=false;
    for(unsigned p=0;p<MAX_PLAYERS;p++) {
        if(!menu && (p>=(g->mode==ARCADE?1:g->players) || (g->phase!=FINISHED && !game_alive(g,p)))) continue;
        start|=in[p].connected && in[p].start && !g->previous[p].start;
        confirm|=in[p].connected && in[p].a && !g->previous[p].a;
        if(g->phase==MENU) confirm|=in[p].connected && in[p].z && !g->previous[p].z;
        back|=in[p].connected && in[p].b && !g->previous[p].b;
    }
    bool all_connected=game_ready(g);
    if(g->phase==MENU) {
        if(g->players>count) g->players=count<2?2:count;
        for(int p=0;p<MAX_PLAYERS;p++) if(in[p].connected) {
            int horizontal=direction(in[p].x);
            if(g->menu_selection==0 && count>=2 && horizontal && horizontal!=direction(g->previous[p].x)) {
                unsigned selected=g->players;
                if(horizontal>0 && selected<count) selected++;
                if(horizontal<0 && selected>2) selected--;
                if(selected!=g->players) { g->players=selected; g->sound_events|=SOUND_SELECT; }
            }
            int nav=direction(in[p].y);
            if(nav && nav!=direction(g->previous[p].y)) {
                g->menu_selection=(g->menu_selection+(nav>0?3:1))%4;
                g->sound_events|=SOUND_SELECT; break;
            }
        }
        menu_step(g);
        if((start || confirm) && (g->menu_selection!=0 || count>=2)) {
            if(g->menu_selection==3) { g->phase=OPTIONS; g->options_selection=0; }
            else if(g->menu_selection==2) g->phase=SCORES;
            else {
                g->mode=g->menu_selection==1?ARCADE:MULTIPLAYER;
                for(unsigned p=0;p<MAX_PLAYERS;p++) g->player_port[p]=p;
                if(g->mode==MULTIPLAYER) for(unsigned p=0,n=0;p<MAX_PLAYERS;p++)
                    if(physical[p].connected) g->player_port[n++]=p;
                for(unsigned p=0;p<MAX_PLAYERS;p++) { g->lives[p]=MULTIPLAYER_LIVES; g->connected[p]=physical[g->player_port[p]].connected; }
                place_bats(g); serve(g); g->phase=LOBBY;
                for(unsigned p=0;p<MAX_PLAYERS;p++) effective[p]=physical[g->player_port[p]];
            }
            g->sound_events|=SOUND_SELECT;
        }
        memcpy(g->previous,in,sizeof(g->previous)); return;
    }
    if(g->phase==OPTIONS) {
        menu_step(g);
        if(back || start) { g->phase=MENU; g->sound_events|=SOUND_BACK; }
        else for(int p=0;p<MAX_PLAYERS;p++) if(in[p].connected) {
            int vertical=direction(in[p].y);
            if(vertical && vertical!=direction(g->previous[p].y)) {
                g->options_selection^=1; g->sound_events|=SOUND_SELECT; break;
            }
            int nav=direction(in[p].x);
            if(nav && nav!=direction(g->previous[p].x)) {
                if(g->options_selection==0)
                    g->flow_effect=(g->flow_effect+(nav>0?1:FLOW_COUNT-1))%FLOW_COUNT;
                else g->frame_rate=g->frame_rate==FPS_60?FPS_30:FPS_60;
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
        for(int p=0;p<MAX_PLAYERS;p++) g->bat[p].sucking=false;
        memcpy(g->previous,physical,sizeof(g->previous)); return;
    }
    if(!all_connected && g->phase==PLAY) g->phase=PAUSED;
    if(start && all_connected) {
        if(g->phase==LOBBY || g->phase==FINISHED) {
            start_game(g);
        } else g->phase=g->phase==PLAY?PAUSED:PLAY;
        g->sound_events|=g->phase==PAUSED?SOUND_BACK:SOUND_SELECT;
        memcpy(g->previous,in,sizeof(g->previous)); return;
    }
    if(g->phase!=PLAY) { memcpy(g->previous,in,sizeof(g->previous)); return; }
    if(g->mode==ARCADE) {
        if(g->arcade.transition>0) {
            g->arcade.transition=maxf(0,g->arcade.transition-step);
            memcpy(g->previous,in,sizeof(g->previous)); return;
        }
        effective[1]=arcade_ai(g);
        if(g->serve<=0) g->arcade.level_time+=step;
        if(g->held>=0) g->arcade.hold_time+=step; else g->arcade.hold_time=0;
    }
    g->elapsed+=step;
    for(unsigned p=0;p<game_players(g);p++) {
        if(!game_alive(g,p)) continue;
        Bat *b=&g->bat[p]; float nx=normal_x(p),ny=normal_y(p);
        float x=b->x,y=b->y;
        float end=game_bat_half(g,p)+2+(game_square(g)?CORNER_SIZE:0);
        float depth=game_square(g)?SQUARE_BAT_DEPTH:64;
        if(p<2) {
            b->x=clampf(x+axis(in[p].x)*92*step,p?game_right(g)-depth:game_left(g)+12,p?game_right(g)-12:game_left(g)+depth);
            b->y=clampf(y-axis(in[p].y)*125*step,end,ARENA_H-end);
        } else {
            b->x=clampf(x+axis(in[p].x)*125*step,game_left(g)+end,game_right(g)-end);
            b->y=clampf(y-axis(in[p].y)*92*step,p==2?ARENA_H-depth:12,p==2?ARENA_H-12:depth);
        }
        b->vx=(b->x-x)*ticks_per_second; b->vy=(b->y-y)*ticks_per_second;
        fluid_splat(&g->fluid,b->x,b->y,20,b->vx*.40f*emission,b->vy*.40f*emission,
                    (fabsf(b->vx)+fabsf(b->vy))*step*.008f,game_player_palette(g,p));
        b->burst=maxf(0,b->burst-step);
        if(in[p].z) {
            /* The arcade opponent's jet approaches twice the human strength. */
            float jet=1150*step*(g->mode==ARCADE && p==1?1+arcade_difficulty(g):1);
            fluid_splat(&g->fluid,b->x+nx*14,b->y+ny*14,22,nx*jet+(p<2?0:b->vx*.08f*emission),ny*jet+(p<2?b->vy*.08f*emission:0),2.6f*step,game_player_palette(g,p));
        }
    }
    if(g->mode==ARCADE) arcade_currents(g);
    fluid_velocity_step(&g->fluid,step);
    for(unsigned p=0;p<game_players(g);p++) {
        if(!game_alive(g,p)) continue;
        Bat *b=&g->bat[p]; float nx=normal_x(p),ny=normal_y(p);
        if(b->cooldown_ticks) b->cooldown_ticks=b->cooldown_ticks>game_tick_units(g)?b->cooldown_ticks-game_tick_units(g):0;
        if(!in[p].a) b->release_required=false;
        if(b->cooldown_ticks || b->release_required) continue;
        if(in[p].a) {
            /* Only time spent holding the ball contributes to charge. */
            if(g->held==(int)p) b->suction_ticks+=game_tick_units(g);
            else b->suction_ticks=0;
            if(b->suction_ticks>=SUCTION_BREAK_TICKS) {
                b->sucking=false; b->charge=0; b->burst=0; b->suction_ticks=0;
                b->cooldown_ticks=SUCTION_COOLDOWN_TICKS; b->release_required=true;
                g->sound_events|=p==0?SOUND_BREAK1:p==1?SOUND_BREAK2:SOUND_BREAK_OTHER;
                if(g->held==(int)p) {
                    g->held=-1; attach_ball(g,p,9);
                    /* Drop into the existing current without a release impulse. */
                    fluid_sample(&g->fluid,g->bx,g->by,&g->bvx,&g->bvy);
                }
                continue;
            }
            b->sucking=true; b->charge=minf(1,(float)b->suction_ticks/SUCTION_CHARGE_TICKS);
            fluid_pump(&g->fluid,b->x+nx*7,b->y+ny*7,35,-1150,step,game_player_palette(g,p));
        } else if(b->sucking) {
            b->sucking=false;
            if(g->held==(int)p) {
                b->burst=.25f;
                /* Spend stored charge only when releasing a caught ball. */
                fluid_pump(&g->fluid,b->x,b->y,40,2800,.13f*b->charge,game_player_palette(g,p));
                fluid_splat(&g->fluid,b->x+nx*12,b->y+ny*12,27,(nx*240+(p<2?0:b->vx*.35f))*b->charge,
                            (ny*240+(p<2?b->vy*.35f:0))*b->charge,b->charge,game_player_palette(g,p));
                g->held=-1; attach_ball(g,p,9);
                /* Perfect timing earns a clear jump to the ball's speed cap. */
                float speed=b->charge>=1?290:200*b->charge;
                g->bvx=nx*speed+(p<2?0:b->vx*.55f); g->bvy=ny*speed+(p<2?b->vy*.55f:0);
            }
            b->charge=0; b->suction_ticks=0;
        }
    }
    fluid_dye_step(&g->fluid,step);
    ball_step(g);
    game_flow_step(g);
    memcpy(g->previous,in,sizeof(g->previous));
    if(g->mode==ARCADE) g->previous[1]=effective[1];
}
