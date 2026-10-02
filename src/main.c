#include "mathutil.h"
#include <libdragon.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include "draw.h"
#include "sound.h"
#include "save.h"
#include "fluid_profile.h"
#ifdef PLASMAPONG_DYE_TEST
#include "../tests/dye_cases.h"
#endif
#ifdef PLASMAPONG_ADVECTION_TEST
#include "../tests/advection_cases.h"
#endif
#ifdef PLASMAPONG_RSP_TEST
#include "../tests/rsp_fluid_smoke.h"
#endif
#ifdef PLASMAPONG_SAVE_SMOKE
#include "../tests/save_smoke.h"
#endif
#ifdef PLASMAPONG_SMOKE
#include "../tests/rom_smoke.h"
#endif
static Game game;
static Sound sound;
#ifdef PLASMAPONG_FLUID_PROFILE
static uint64_t audio_ticks;
static unsigned audio_frames;
#endif
static void fill_audio(short *buffer,size_t frames) {
#ifdef PLASMAPONG_FLUID_PROFILE
    uint64_t begin=get_ticks();
#endif
    sound_render(&sound,buffer,frames);
#ifdef PLASMAPONG_FLUID_PROFILE
    audio_ticks+=get_ticks()-begin; audio_frames+=frames;
#endif
}
static surface_t ink;
static rspq_block_t *ink_blit;
static float ink_x,ink_y,ink_w,ink_h;
static bool fill_mode;
static uint32_t fill_color;
static rspq_block_t *static_draw[DRAW_STATIC_COUNT];
void draw_static(unsigned id,void (*draw)(void)) {
    assert(id<DRAW_STATIC_COUNT);
    fill_mode=false;
    if(!static_draw[id]) {
        rspq_block_begin(); draw(); static_draw[id]=rspq_block_end();
    }
    rspq_block_run(static_draw[id]);
    fill_mode=false;
}
static color_t color(uint32_t c) { return RGBA32(c>>16,(c>>8)&255,c&255,255); }
void rect(float x,float y,float w,float h,uint32_t c) {
    if(w<=0 || h<=0) return;
    if(!fill_mode || fill_color!=c) {
        rdpq_set_mode_fill(color(c)); fill_color=c; fill_mode=true;
    }
    rdpq_fill_rectangle(x,y,x+w,y+h);
}
void label(float x,float y,int style,const char *s) {
    fill_mode=false;
    /* The built-in bitmap font has integer advances. Centering can put its
       origin on a half pixel, where RDP coverage/point sampling clips strokes.
       Snap every text origin, including shadows and edge-aligned labels. */
    rdpq_text_print(&(rdpq_textparms_t){.style_id=style},1,roundf(x),roundf(y),s);
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
    fill_mode=false;
    /* Generate all pixels together so bank selection and call overhead stay
       outside the cell loop. Preserve the padded RGBA32 upload layout. */
    fluid_pixels(f,ink.buffer,ink.stride/sizeof(uint32_t));
    /* Geometry and texture address are constant until switching menu/court.
       Record upload/tiling commands once; pixel contents remain dynamic. The
       frame-start wait also makes freeing the previous block safe. */
    if(!ink_blit || x!=ink_x || y!=ink_y || width!=ink_w || height!=ink_h) {
        if(ink_blit) rspq_block_free(ink_blit);
        ink_x=x; ink_y=y; ink_w=width; ink_h=height;
        rspq_block_begin();
        rdpq_set_mode_standard(); rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_tex_blit(&ink,x,y,&(rdpq_blitparms_t){
            .width=FW,.height=FH,.scale_x=width/FW,.scale_y=height/FH,.filtering=true});
        ink_blit=rspq_block_end();
    }
    rspq_block_run(ink_blit);
}
int main(void) {
    debug_init_isviewer(); debug_init_emulog(); timer_init(); joypad_init();
    display_init(RESOLUTION_320x240,DEPTH_16_BPP,3,GAMMA_NONE,FILTERS_RESAMPLE);
    audio_init(SOUND_RATE,4); sound_init(&sound,audio_get_frequency());
    audio_set_buffer_callback(fill_audio); audio_write_silence();
    /* Pad rows so partial-width uploads use LoadTile. The pinned libdragon's
       RGBA32 LoadBlock path corrupts this non-power-of-two texture width. */
    rdpq_init(); ink=surface_alloc(FMT_RGBA32,64,FH);
#ifdef PLASMAPONG_RDP_VALIDATE
    rdpq_debug_start();
#endif
#ifdef PLASMAPONG_DYE_TEST
    dye_cases();
#endif
#ifdef PLASMAPONG_ADVECTION_TEST
    advection_cases();
    debugf("Advection PASS: 64 exact float-reference fields; source banks unchanged\n");
#endif
#ifdef PLASMAPONG_RSP_TEST
    rsp_fluid_smoke();
#endif
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
    uint64_t frame_window=get_ticks(); unsigned frame_steps=0;
    debugf("Plasma Pong 64: ready, %u-byte game state\n",(unsigned)sizeof(game));
    while(1) {
        uint64_t now=get_ticks();
        float elapsed=TIMER_MICROS_LL(now-previous)*.000001f; previous=now;
        /* Bound catch-up after stalls; fixed physics works on PAL and NTSC. */
        accumulator+=minf(elapsed,.1f);
        /* There is no render interpolation: a frame with no new simulation
           step repeats the same image. Reserve that time for the next 30 Hz
           update instead of spending it regenerating/uploading the texture.
           Interrupts remain enabled so audio continues during this wait. */
        if(accumulator<STEP) {
            /* Round up: sub-microsecond remainders must still make progress. */
            wait_ticks(TICKS_FROM_US(1+(uint32_t)((STEP-accumulator)*1000000.0f)));
            continue;
        }
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
            frame_steps++;
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
        surface_t *frame=display_get();
        uint64_t draw_begin=get_ticks();
        /* Finish the previous frame before overwriting its shared texture or
           freeing a blit block. The simulation above can run alongside RDP. */
        rspq_wait();
        fill_mode=false;
        rdpq_attach(frame,NULL); ui_draw(&game); rdpq_detach_show();
        draw_ticks+=get_ticks()-draw_begin;
        if(++draw_frames==150) {
            debugf("Plasma Pong 64: draw average %llu us/frame\n",
                (unsigned long long)(TIMER_MICROS_LL(draw_ticks)/draw_frames));
            uint64_t window_end=get_ticks();
            debugf("Plasma Pong 64: frame interval %llu us (%u steps / %u frames)\n",
                (unsigned long long)(TIMER_MICROS_LL(window_end-frame_window)/draw_frames),
                frame_steps,draw_frames);
            frame_window=window_end; frame_steps=0;
#ifdef PLASMAPONG_FLUID_PROFILE
            disable_interrupts();
            uint64_t audio_time=audio_ticks; unsigned samples=audio_frames;
            audio_ticks=0; audio_frames=0;
            enable_interrupts();
            debugf("Plasma Pong 64: audio %llu us for %u samples\n",
                (unsigned long long)TIMER_MICROS_LL(audio_time),samples);
#endif
            draw_ticks=0; draw_frames=0;
        }
    }
}
