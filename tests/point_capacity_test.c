#include "draw.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned commands,dots,batches,ends;
void draw_points(const DrawPoint *points,unsigned count,uint32_t c) {
    (void)c;
    if(!count) return;
    commands+=count+2;
    dots+=count; batches++;
    assert(commands<=DRAW_POINT_COMMAND_CAPACITY);
    for(unsigned i=0;i<count;i++) assert(points[i].x<320 && points[i].y<240);
}
void draw_points_end(void) { ends++; }
void draw_static(unsigned id,void (*draw)(void)) { (void)id; draw(); }
void rect(float x,float y,float w,float h,uint32_t c) {
    (void)x; (void)y; (void)w; (void)h; (void)c;
}
void label(float x,float y,int style,const char *s) {
    (void)x; (void)y; (void)style; (void)s;
}
void label_edge(float x,float y,int style,const char *s,bool right) {
    (void)right; label(x,y,style,s);
}
float label_width(const char *s) { return strlen(s)*4.2f; }
void draw_fluid(const Fluid *f,float x,float y,float w,float h,FlowEffect effect) {
    (void)f; (void)x; (void)y; (void)w; (void)h; (void)effect;
}

int main(void) {
    static Game g;
    game_init(&g); g.phase=MENU;
    memset(fluid_dye(&g.fluid),0,sizeof(*fluid_dye(&g.fluid)));
    /* Fill every layer with all six tints, including the newer blends.
       Head cells are distinct; every tail remains visible and separated. */
    const float pigments[6][3]={{0,0,0},{0,1,0},{1,0,0},{0,0,1},{0,1,.5f},{1,1,0}};
    for(unsigned i=0;i<FLOW_TRACERS;i++) {
        unsigned cx=4+i%24,cy=4+i/24;
        unsigned k=cy*FW+cx,tint=i%6;
        fluid_dye(&g.fluid)->red[k]=fluid_ink_encode(pigments[tint][0]);
        fluid_dye(&g.fluid)->blue[k]=fluid_ink_encode(pigments[tint][1]);
        fluid_dye(&g.fluid)->gold[k]=fluid_ink_encode(pigments[tint][2]);
        g.tracers[i].life=1;
        for(unsigned j=0;j<DRAW_POINT_LAYERS;j++) {
            unsigned history=(g.tracer_head+j*FLOW_SAMPLE_TICKS)%FLOW_HISTORY;
            g.tracer_history[history][i]=(FlowPosition){cx*CELL+j*2,cy*CELL};
        }
    }
    g.flow_effect=FLOW_TAILS;
    ui_draw(&g);
    assert(dots==DRAW_POINT_LAYERS*FLOW_TRACERS);
    assert(batches==DRAW_POINT_LAYERS*6 && ends==1);
    assert(commands==540); /* Old 520-command buffer overflowed here. */
    commands=dots=batches=ends=0;
    g.flow_effect=FLOW_PARTICLES;
    ui_menu_particles(&g);
    assert(dots==FLOW_TRACERS && batches==6 && ends==1);
    commands=dots=batches=ends=0;
    memset(g.tracers,0,sizeof(g.tracers));
    ui_menu_particles(&g);
    assert(commands==0 && ends==1);
    puts("PASS: full six-tint tails and particle batches fit the ROM command buffer");
}
