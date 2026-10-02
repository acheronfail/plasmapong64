#include "mathutil.h"
#include <libdragon.h>
#include <math.h>
#include "draw.h"
#include "sound.h"
#ifdef PLASMAPONG_SMOKE
#include "../tests/rom_smoke.h"
#endif
static Game game;
static Sound sound;
static void fill_audio(short *buffer,size_t frames) { sound_render(&sound,buffer,frames); }
static surface_t ink;
static color_t color(uint32_t c) { return RGBA32(c>>16,(c>>8)&255,c&255,255); }
void rect(float x,float y,float w,float h,uint32_t c) {
    if(w<=0 || h<=0) return;
    rdpq_set_mode_fill(color(c)); rdpq_fill_rectangle(x,y,x+w,y+h);
}
void label(float x,float y,int style,const char *s) {
    rdpq_text_print(&(rdpq_textparms_t){.style_id=style},1,x,y,s);
}
float label_width(const char *s) {
    float w=0;
    const rdpq_font_t *font=rdpq_text_get_font(1);
    for(;*s;s++) {
        rdpq_font_gmetrics_t m;
        if(rdpq_font_get_glyph_metrics(font,(unsigned char)*s,&m)) w+=m.xadvance;
    }
    return w;
}
void draw_fluid(const Fluid *f,float x,float y,float width,float height) {
    uint16_t *pixels=ink.buffer;
    for(int i=0;i<FN;i++) {
        uint32_t c=fluid_color(f,i);
        pixels[i]=((c>>19)&31)<<11 | ((c>>11)&31)<<6 | ((c>>3)&31)<<1 | 1;
    }
    rdpq_set_mode_standard(); rdpq_mode_filter(FILTER_BILINEAR);
    rdpq_tex_blit(&ink,x,y,&(rdpq_blitparms_t){.scale_x=width/FW,.scale_y=height/FH});
}
int main(void) {
    debug_init_isviewer(); debug_init_emulog(); timer_init(); joypad_init();
    display_init(RESOLUTION_320x240,DEPTH_16_BPP,3,GAMMA_NONE,FILTERS_RESAMPLE);
    audio_init(SOUND_RATE,4); sound_init(&sound,audio_get_frequency());
    audio_set_buffer_callback(fill_audio); audio_write_silence();
    rdpq_init(); ink=surface_alloc(FMT_RGBA16,FW,FH);
    rdpq_font_t *font=rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_VAR);
    const uint32_t colors[]={0xeaf6ff,0xa0b3c9,0x48dcff,0xff637e,0xffffff,0x02040a};
    for(int i=0;i<6;i++) rdpq_font_style(font,i,&(rdpq_fontstyle_t){.color=color(colors[i])});
    rdpq_text_register_font(1,font); game_init(&game); game.menu_rng=(uint32_t)get_ticks();
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
                .a=in.btn.a,.z=in.btn.z,.start=in.btn.start,.b=in.btn.b,
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
            disable_interrupts(); sound_update(&sound,&game); enable_interrupts();
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
