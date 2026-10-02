#include "mathutil.h"
#include <libdragon.h>
#include <math.h>
#include <string.h>
#include "draw.h"
#include "sound.h"
#include "save.h"
#include "fluid_profile.h"
#ifdef PLASMAPONG_SAVE_SMOKE
#include "../tests/save_smoke.h"
#endif
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
void label_edge(float x,float y,int style,const char *s,bool right) {
    const rdpq_font_t *font=rdpq_text_get_font(1);
    float pen=0,left=0,end=0;
    bool first=true;
    /* These plain ASCII HUD labels have no markup or kerning pairs. Use the
       glyph bounds so both court edges have the same visible inset. */
    for(const char *p=s;*p;p++) {
        rdpq_font_gmetrics_t m;
        if(!rdpq_font_get_glyph_metrics(font,(unsigned char)*p,&m)) continue;
        if(first) { left=m.x0; first=false; }
        end=pen+m.x1;
        pen+=m.xadvance;
    }
    label(x-(right?end:left),y,style,s);
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
    /* Generate all pixels together so bank selection and call overhead stay
       outside the cell loop. Preserve the padded RGBA32 upload layout. */
    fluid_pixels(f,ink.buffer,ink.stride/sizeof(uint32_t));
    rdpq_set_mode_standard(); rdpq_mode_filter(FILTER_BILINEAR);
    rdpq_tex_blit(&ink,x,y,&(rdpq_blitparms_t){
        .width=FW,.height=FH,.scale_x=width/FW,.scale_y=height/FH,.filtering=true});
}
int main(void) {
    debug_init_isviewer(); debug_init_emulog(); timer_init(); joypad_init();
    display_init(RESOLUTION_320x240,DEPTH_16_BPP,3,GAMMA_NONE,FILTERS_RESAMPLE);
    audio_init(SOUND_RATE,4); sound_init(&sound,audio_get_frequency());
    audio_set_buffer_callback(fill_audio); audio_write_silence();
    /* Pad rows so partial-width uploads use LoadTile. The pinned libdragon's
       RGBA32 LoadBlock path corrupts this non-power-of-two texture width. */
    rdpq_init(); ink=surface_alloc(FMT_RGBA32,64,FH);
    rdpq_font_t *font=rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_VAR);
    const uint32_t colors[]={0xeaf6ff,0xa0b3c9,0x48dcff,0xff637e,0xffffff,0x02040a,0x737d8a};
    for(int i=0;i<7;i++) rdpq_font_style(font,i,&(rdpq_fontstyle_t){.color=color(colors[i])});
    rdpq_text_register_font(1,font); game_init(&game); scores_load(&game); game.menu_rng=(uint32_t)get_ticks();
#ifdef PLASMAPONG_SAVE_SMOKE
    save_smoke(&game);
#endif
    uint64_t previous=get_ticks(); float accumulator=0;
    uint64_t sim_ticks=0; unsigned sim_steps=0;
    uint64_t draw_ticks=0; unsigned draw_frames=0;
    debugf("Plasma Pong 64: ready, %u-byte game state\n",(unsigned)sizeof(game));
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
#ifdef PLASMAPONG_FLUID_PROFILE
            fluid_profile.enabled=true;
            uint64_t profile_before[PROFILE_COUNT];
            unsigned calls_before[PROFILE_COUNT];
            memcpy(profile_before,fluid_profile.ticks,sizeof(profile_before));
            memcpy(calls_before,fluid_profile.calls,sizeof(calls_before));
#endif
            game_step(&game,input); accumulator-=STEP;
            disable_interrupts(); sound_update(&sound,&game); enable_interrupts();
            if(game.scores_dirty) {
                scores_store(&game);
                previous=get_ticks(); accumulator=0;
            }
#ifdef PLASMAPONG_FLUID_PROFILE
            fluid_profile.enabled=false;
            if(game.phase!=PLAY) {
                memcpy(fluid_profile.ticks,profile_before,sizeof(profile_before));
                memcpy(fluid_profile.calls,calls_before,sizeof(calls_before));
            }
#endif
            if(game.phase==PLAY) {
                sim_ticks+=get_ticks()-begin;
                if(++sim_steps==150) {
                    debugf("Plasma Pong 64: simulation average %llu us/step (budget 33333 us)\n",
                        (unsigned long long)(TIMER_MICROS_LL(sim_ticks)/sim_steps));
#ifdef PLASMAPONG_FLUID_PROFILE
                    static const char *names[PROFILE_COUNT]={
                        "velocity_advection","velocity_swap","curl","confinement",
                        "divergence","pressure_solve","pressure_gradient",
                        "dye_advection","dye_swap","splat","pump","ball_dye","sample"};
                    for(int i=0;i<PROFILE_COUNT;i++) {
                        debugf("Fluid profile: %s %llu us/step (%u calls)\n",names[i],
                            (unsigned long long)(TIMER_MICROS_LL(fluid_profile.ticks[i])/sim_steps),
                            fluid_profile.calls[i]);
                        fluid_profile.ticks[i]=0; fluid_profile.calls[i]=0;
                    }
#endif
                    sim_ticks=0; sim_steps=0;
                }
            }
        }
        uint64_t draw_begin=get_ticks();
        rdpq_attach(frame,NULL); ui_draw(&game); rdpq_detach_show();
        /* The CPU updates a shared dye texture next frame. Wait for its RDP read. */
        rspq_wait();
        draw_ticks+=get_ticks()-draw_begin;
        if(++draw_frames==150) {
            debugf("Plasma Pong 64: draw average %llu us/frame\n",
                (unsigned long long)(TIMER_MICROS_LL(draw_ticks)/draw_frames));
            draw_ticks=0; draw_frames=0;
        }
    }
}
