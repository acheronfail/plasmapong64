#include "mathutil.h"
#include <libdragon.h>
#include <math.h>
#include "draw.h"
#ifdef PLASMAPONG_SMOKE
#include "../tests/rom_smoke.h"
#endif
static Game game;
static surface_t ink;
static color_t color(uint32_t c) { return RGBA32(c>>16,(c>>8)&255,c&255,255); }
void rect(float x,float y,float w,float h,uint32_t c) {
    if(w<=0 || h<=0) return;
    rdpq_set_mode_fill(color(c)); rdpq_fill_rectangle(x,y,x+w,y+h);
}
void label(float x,float y,int style,const char *s) {
    rdpq_text_print(&(rdpq_textparms_t){.style_id=style},1,x,y,s);
}
void draw_fluid(const Fluid *f) {
    uint16_t *pixels=ink.buffer;
    for(int i=0;i<FN;i++) {
        uint32_t c=fluid_color(f,i);
        pixels[i]=((c>>19)&31)<<11 | ((c>>11)&31)<<6 | ((c>>3)&31)<<1 | 1;
    }
    rdpq_set_mode_standard(); rdpq_mode_filter(FILTER_BILINEAR);
    rdpq_tex_blit(&ink,16,34,&(rdpq_blitparms_t){.scale_x=6,.scale_y=6});
}
int main(void) {
    debug_init_isviewer(); debug_init_emulog(); timer_init(); joypad_init();
    display_init(RESOLUTION_320x240,DEPTH_16_BPP,3,GAMMA_NONE,FILTERS_RESAMPLE);
    rdpq_init(); ink=surface_alloc(FMT_RGBA16,FW,FH);
    rdpq_font_t *font=rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_VAR);
    const uint32_t colors[]={0xeaf6ff,0xa0b3c9,0x48dcff,0xff637e};
    for(int i=0;i<4;i++) rdpq_font_style(font,i,&(rdpq_fontstyle_t){.color=color(colors[i])});
    rdpq_text_register_font(1,font); game_init(&game);
    uint64_t previous=get_ticks(); float accumulator=0;
    uint64_t sim_ticks=0; unsigned sim_steps=0;
    debugf("Plasma Pong: ready, %u-byte game state\n",(unsigned)sizeof(game));
    while(1) {
        surface_t *frame=display_get();
        uint64_t now=get_ticks();
        float elapsed=TIMER_MICROS_LL(now-previous)*.000001f; previous=now;
        /* Bound catch-up after stalls; fixed physics works on PAL and NTSC. */
        accumulator+=minf(elapsed,.1f);
        joypad_poll(); Input input[2]={0};
        for(int p=0;p<2;p++) {
            joypad_inputs_t in=joypad_get_inputs((joypad_port_t)p);
            input[p]=(Input){.connected=joypad_get_style((joypad_port_t)p)==JOYPAD_STYLE_N64,
                .a=in.btn.a,.z=in.btn.z,.start=in.btn.start,
                .x=in.stick_x/80.0f,.y=in.stick_y/80.0f};
            if(in.btn.d_up) input[p].y=1;
            if(in.btn.d_down) input[p].y=-1;
            if(in.btn.d_left) input[p].x=-1;
            if(in.btn.d_right) input[p].x=1;
        }
        while(accumulator>=STEP) {
            uint64_t begin=get_ticks();
#ifdef PLASMAPONG_SMOKE
            smoke_input(&game,input);
#endif
            game_step(&game,input); accumulator-=STEP;
            if(game.phase==PLAY) {
                sim_ticks+=get_ticks()-begin;
                if(++sim_steps==150) {
                    debugf("Plasma Pong: simulation average %llu us/step (budget 33333 us)\n",
                        (unsigned long long)(TIMER_MICROS_LL(sim_ticks)/sim_steps));
                    sim_ticks=0; sim_steps=0;
                }
            }
        }
        rdpq_attach(frame,NULL); ui_draw(&game); rdpq_detach_show();
        /* The CPU updates a shared dye texture next frame. Wait for its RDP read. */
        rspq_wait();
    }
}
