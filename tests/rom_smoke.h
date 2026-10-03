/* Explicit test-ROM-only controller replay. */
#ifndef PLASMAPONG_SMOKE_PLAYERS
#define PLASMAPONG_SMOKE_PLAYERS 2
#endif
static void smoke_input(Game *g,Input in[MAX_PLAYERS]) {
    static unsigned tick;
#ifdef PLASMAPONG_SMOKE_EFFECT
    _Static_assert(PLASMAPONG_SMOKE_EFFECT>=0 && PLASMAPONG_SMOKE_EFFECT<FLOW_COUNT,"valid benchmark effect");
    g->flow_effect=(FlowEffect)PLASMAPONG_SMOKE_EFFECT;
#endif
#ifdef PLASMAPONG_SMOKE_FLOW
    if(tick%450==0) {
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
    tick++;
}
