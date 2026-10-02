/* Deterministic host preview, using the ROM's simulation and UI. Not an emulator. */
#include "draw.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
void rect(float x,float y,float w,float h,uint32_t c) {
    if(w>0 && h>0) printf("<rect x='%g' y='%g' width='%g' height='%g' fill='#%06x'/>\n",x,y,w,h,c);
}
void label(float x,float y,int style,const char *s) {
    const uint32_t colors[]={0xeaf6ff,0xa0b3c9,0x48dcff,0xff637e};
    printf("<text x='%g' y='%g' font-family='monospace' font-size='7' fill='#%06x'>",x,y,colors[style]);
    for(;*s;s++) {
        if(*s=='&') fputs("&amp;",stdout);
        else if(*s=='<') fputs("&lt;",stdout);
        else if(*s=='>') fputs("&gt;",stdout);
        else putchar(*s);
    }
    puts("</text>");
}
void draw_fluid(const Fluid *f) {
    puts("<g shape-rendering='crispEdges'>");
    for(int y=0;y<FH;y++) for(int x=0;x<FW;x++) rect(16+x*6,34+y*6,6,6,fluid_color(f,y*FW+x));
    puts("</g>");
}
int main(int argc,char **argv) {
    static Game g; game_init(&g);
    Input in[2]={{.connected=true,.start=true},{.connected=true}};
    game_step(&g,in); in[0].start=false;
    for(int t=0;t<540;t++) {
        for(int p=0;p<2;p++) {
            in[p].y=sinf(t*.037f+p*2.4f); in[p].z=t%110<90;
            in[p].a=(t+40*p)%145>118;
        }
        if(g.phase==FINISHED) in[0].start=true; else in[0].start=false;
        game_step(&g,in);
    }
    if(argc>1 && !strcmp(argv[1],"lobby")) { game_init(&g); }
    if(argc>1 && !strcmp(argv[1],"paused")) g.phase=PAUSED;
    if(argc>1 && !strcmp(argv[1],"finished")) { g.phase=FINISHED; g.winner=1; g.score[1]=9; }
    puts("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 320 240' width='960' height='720'>");
    ui_draw(&g); puts("</svg>");
}
