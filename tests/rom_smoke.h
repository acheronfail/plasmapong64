/* Explicit test-ROM-only input replay. The normal ROM always needs two players. */
static void smoke_input(const Game *g,Input in[2]) {
    static unsigned tick;
    for(int p=0;p<2;p++) {
        in[p]=(Input){.connected=true,.x=sinf(tick*.021f+p),
            .y=sinf(tick*.037f+p*2.4f),.z=tick%110<90,.a=(tick+40*p)%145>118};
    }
    in[0].start=(g->phase==LOBBY || g->phase==FINISHED);
    tick++;
}
