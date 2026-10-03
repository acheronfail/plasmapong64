/* Explicit test-ROM-only controller replay. */
#ifndef PLASMAPONG_SMOKE_PLAYERS
#define PLASMAPONG_SMOKE_PLAYERS 2
#endif
static void smoke_input(Game *g,Input in[MAX_PLAYERS]) {
    static unsigned steps;
#ifdef PLASMAPONG_SMOKE_VIDEO
    /* Use real menu inputs to switch both ways while rendering, save the
       choice, then return to ordinary scripted gameplay in high res. */
    static unsigned video_stage,video_wait,video_pulse;
    if(video_stage<7) {
        memset(in,0,sizeof(Input)*MAX_PLAYERS);
        in[0].connected=in[1].connected=true;
        bool pulse=(video_pulse++%2)==0;
        switch(video_stage) {
        case 0:
            if(g->menu_selection!=3) in[0].y=pulse?-1:0;
            else { in[0].start=pulse; if(g->phase==OPTIONS) video_stage++; }
            break;
        case 1:
            if(g->options_selection!=1) in[0].y=pulse?-1:0;
            else { video_stage++; video_wait=0; }
            break;
        case 2: case 4:
            if(g->frame_rate!=FPS_30) in[0].x=pulse?1:0;
            else if(++video_wait==150) { video_stage++; video_wait=0; }
            break;
        case 3:
            if(g->frame_rate!=FPS_60) in[0].x=pulse?1:0;
            else if(++video_wait==150) { video_stage++; video_wait=0; }
            break;
        case 5:
            if(g->phase==OPTIONS) in[0].b=pulse;
            else video_stage++;
            break;
        case 6:
            debugf("VIDEO SMOKE PASS: low/high/low/high via Options, returned to menu\n");
            video_stage++;
            break;
        }
        return;
    }
#endif
    /* Keep the same input/effect timeline in seconds as the 30 Hz replay. */
    unsigned tick=steps*30/GAME_HZ;
#ifdef PLASMAPONG_SMOKE_EFFECT
    _Static_assert(PLASMAPONG_SMOKE_EFFECT>=0 && PLASMAPONG_SMOKE_EFFECT<FLOW_COUNT,"valid benchmark effect");
    g->flow_effect=(FlowEffect)PLASMAPONG_SMOKE_EFFECT;
#endif
#ifdef PLASMAPONG_SMOKE_FLOW
    if(steps%(15*GAME_HZ)==0) {
        g->flow_effect=(FlowEffect)((tick/450)%FLOW_COUNT);
        memset(g->tracers,0,sizeof(g->tracers));
        debugf("FLOW SMOKE: effect %u\n",(unsigned)g->flow_effect);
    }
#endif
    memset(in,0,sizeof(Input)*MAX_PLAYERS);
    for(int p=0;p<PLASMAPONG_SMOKE_PLAYERS;p++) {
        in[p]=(Input){.connected=true,.x=sinf(tick*.021f+p),
            .y=sinf(tick*.037f+p*2.4f),.z=tick%110<90,.a=(tick+40*p)%145>118};
    }
#ifdef PLASMAPONG_SMOKE_STRESS
    /* Test-only worst case: retain all players and exercise every jet/pump
       continuously, including bypassing grab-break cooldowns. */
    if(g->phase==PLAY) for(int p=0;p<PLASMAPONG_SMOKE_PLAYERS;p++) {
        g->lives[p]=MULTIPLAYER_LIVES;
        g->bat[p].cooldown_ticks=0; g->bat[p].release_required=false;
        in[p].z=in[p].a=true;
    }
#endif
#ifdef PLASMAPONG_SMOKE_ARCADE
    in[1]=(Input){0};
    static bool level_set;
    if(g->phase==PLAY && !level_set) { g->arcade.level=PLASMAPONG_SMOKE_LEVEL; level_set=true; }
    if(g->phase==MENU) level_set=false;
    const unsigned selection=1;
#else
    const unsigned selection=0;
#endif
    if(g->phase!=PLAY) {
        in[0]=(Input){.connected=true};
        for(int p=1;p<PLASMAPONG_SMOKE_PLAYERS;p++) {
            in[p].x=in[p].y=0; in[p].a=in[p].z=false;
        }
        if(g->phase==MENU && g->menu_selection!=selection)
            in[0].y=tick%2==0?-1:0;
        else if(g->phase==MENU && selection==0 && g->players<PLASMAPONG_SMOKE_PLAYERS)
            in[0].x=tick%2==0?1:0;
        else in[0].start=tick%2==0;
    }
    steps+=game_tick_units(g);
}
