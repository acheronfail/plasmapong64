/* Deterministic host preview, using the ROM's simulation and UI. Not an emulator. */
#include "draw.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
void draw_static(unsigned id,void (*draw)(void)) { (void)id; draw(); }
void rect(float x,float y,float w,float h,uint32_t c) {
    if(w>0 && h>0) printf("<rect x='%g' y='%g' width='%g' height='%g' fill='#%06x'/>\n",x,y,w,h,c);
}
void label(float x,float y,int style,const char *s) {
    const uint32_t colors[]={0xeaf6ff,0xa0b3c9,0x48dcff,0xff637e,0xffffff,0x02040a,0x737d8a};
    printf("<text xml:space='preserve' x='%g' y='%g' font-family='monospace' font-size='7' fill='#%06x'>",x,y,colors[style]);
    for(;*s;s++) {
        if(*s=='&') fputs("&amp;",stdout);
        else if(*s=='<') fputs("&lt;",stdout);
        else if(*s=='>') fputs("&gt;",stdout);
        else putchar(*s);
    }
    puts("</text>");
}
float label_width(const char *s) { return strlen(s)*4.2f; }
void label_edge(float x,float y,int style,const char *s,bool right) {
    label(right?x-label_width(s):x,y,style,s);
}
void draw_fluid(const Fluid *f,float ox,float oy,float width,float height) {
    puts("<g shape-rendering='crispEdges'>");
    for(int y=0;y<FH;y++) for(int x=0;x<FW;x++) rect(ox+x*width/FW,oy+y*height/FH,width/FW,height/FH,fluid_color(f,y*FW+x));
    puts("</g>");
}
int main(int argc,char **argv) {
    static Game g; game_init(&g); g.phase=LOBBY;
    Input in[2]={{.connected=true,.start=true},{.connected=true}};
    game_step(&g,in); in[0].start=false;
    for(int t=0,ticks=argc>2?atoi(argv[2]):540;t<ticks;t++) {
        for(int p=0;p<2;p++) {
            in[p].y=sinf(t*.037f+p*2.4f); in[p].z=t%110<90;
            in[p].a=(t+40*p)%145>118;
        }
        if(g.phase==FINISHED) in[0].start=true; else in[0].start=false;
        game_step(&g,in);
    }
    if(argc>1 && !strcmp(argv[1],"lobby")) { game_init(&g); g.phase=LOBBY; }
    if(argc>1 && !strcmp(argv[1],"paused")) g.phase=PAUSED;
    if(argc>1 && !strcmp(argv[1],"overcharge")) {
        g.phase=PLAY; g.bat[0].sucking=true; g.bat[0].charge=1;
        g.bat[1].sucking=false; g.bat[1].cooldown_ticks=90;
    }
    if(argc>1 && !strcmp(argv[1],"finished")) { g.phase=FINISHED; g.winner=1; g.score[1]=9; }
    if(argc>1 && !strcmp(argv[1],"menu")) {
        game_init(&g); Input idle[2]={0};
        for(int t=0;t<180;t++) game_step(&g,idle);
    }
    if(argc>1 && (!strcmp(argv[1],"arcade") || !strcmp(argv[1],"gameover") || !strcmp(argv[1],"scores") || !strcmp(argv[1],"level"))) {
        g.mode=ARCADE;
        g.arcade=(Arcade){.level=8,.lives=2,.goals=1,.points=18720};
        g.phase=PLAY;
        g.highs[0]=(HighScore){.level=8,.points=18720,.initials="ACE"};
        if(!strcmp(argv[1],"gameover")) { g.phase=FINISHED; g.arcade.lives=0; g.score_entry=0; }
        if(!strcmp(argv[1],"scores")) g.phase=SCORES;
        if(!strcmp(argv[1],"level")) g.arcade.transition=1;
    }
    puts("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 320 240' width='960' height='720'>");
    ui_draw(&g); puts("</svg>");
}
