#include "mathutil.h"
#include <libdragon.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include "draw.h"
#include "sound.h"
#include "save.h"
#include "fluid_profile.h"
#include "perf.h"
#ifdef PLASMAPONG_PREPARE_RSP
#include "fluid_velocity_fixed.h"
#endif
#ifdef PLASMAPONG_CONFINEMENT_TEST
#include "../tests/confinement_cases.h"
#endif
#ifdef PLASMAPONG_VELOCITY_TEST
#include "../tests/velocity_cases.h"
#ifdef PLASMAPONG_PREPARE_RSP
#include "../tests/prepare_cases.h"
#include "../tests/gradient_cases.h"
#endif
#endif
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
static volatile PresentationStats presentation;
static bool perf_visible=true;
static char perf_text[64]="FPS -- / --", perf_gap[64]="MISS 0  GAP 1 VI";
static PresentationStats perf_window;
static void perf_vi(void) {
    /* VI_ORIGIN identifies the actual scanned-out surface. We use 240p, so
       alternating interlaced field offsets cannot masquerade as new frames.
       Register before display_init: pinned libdragon prepends handlers, so
       its framebuffer swap runs before this sampler. No work is fenced here. */
    presentation_sample(&presentation,*(volatile uint32_t *)0xA4400004,get_ticks());
}
static PresentationStats perf_snapshot(void) {
    disable_interrupts();
    PresentationStats s=presentation;
    enable_interrupts();
    return s;
}
static void perf_reset(void) {
    disable_interrupts();
    presentation=(PresentationStats){.allowed_gap=game_tick_units(&game)};
    enable_interrupts();
    perf_window=(PresentationStats){0};
    strcpy(perf_text,"FPS -- / --"); strcpy(perf_gap,"MISS 0  GAP 1 VI");
}
static void perf_update(void) {
    PresentationStats s=perf_snapshot();
    if(!s.seeded) return;
    if(!perf_window.seeded) { perf_window=s; return; }
    uint64_t us=TIMER_MICROS_LL(s.ticks-perf_window.ticks);
    if(us<500000) return;
    unsigned fps=(uint64_t)(s.frames-perf_window.frames)*10000000/us;
    unsigned hz=(uint64_t)(s.refreshes-perf_window.refreshes)*10000000/us;
    snprintf(perf_text,sizeof(perf_text),"FPS %u.%u / %u.%u",fps/10,fps%10,hz/10,hz%10);
    snprintf(perf_gap,sizeof(perf_gap),"MISS %lu  GAP %lu VI",(unsigned long)s.misses,(unsigned long)s.longest_gap);
    perf_window=s;
}
#ifdef PLASMAPONG_FLUID_PROFILE
static uint64_t audio_ticks;
static uint64_t pixel_ticks, texture_ticks, wait_draw_ticks;
static unsigned audio_frames;
uint64_t flow_prepare_ticks,flow_emit_ticks;
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
static _Alignas(16) uint64_t point_commands[5*(FLOW_TRACERS+8)];
static unsigned point_commands_used;
/* Bounded text command cache. Entries used this frame cannot be evicted;
   frame-start rspq_wait guarantees older entries are no longer in flight. */
static struct {
    rspq_block_t *block;
    float x,y;
    int style;
    unsigned frame;
    char text[64];
} text_cache[32];
static unsigned render_frame=1;
static bool recording_static;
static rspq_block_t *static_draw[DRAW_STATIC_COUNT];
void draw_static(unsigned id,void (*draw)(void)) {
    assert(id<DRAW_STATIC_COUNT);
    fill_mode=false;
    if(!static_draw[id]) {
        recording_static=true;
        rspq_block_begin(); draw(); static_draw[id]=rspq_block_end();
        recording_static=false;
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
void draw_points(const DrawPoint *points,unsigned count,uint32_t c) {
    if(!count) return;
    assert(point_commands_used+count+2<=sizeof(point_commands)/sizeof(*point_commands));
    if(!point_commands_used) rdpq_set_mode_fill(RGBA32(0,0,0,255));
    uint64_t *commands=point_commands+point_commands_used;
    /* Pipe sync before each color change; 16-bit fill repeats the same pixel. */
    uint32_t pixel=color_to_packed16(color(c));
    *commands++=0xE700000000000000ull;
    *commands++=0xF700000000000000ull|((uint64_t)pixel<<16)|pixel;
    for(unsigned i=0;i<count;i++) {
        assert(points[i].x<320 && points[i].y<240);
        uint32_t xy=((uint32_t)points[i].x<<14)|((uint32_t)points[i].y<<2);
        /* Fill-cycle lower-right is inclusive: equal corners cover one pixel. */
        commands[i]=((uint64_t)(0xF6000000u|xy)<<32)|xy;
    }
    point_commands_used+=count+2;
}
void draw_points_end(void) {
    if(!point_commands_used) return;
    data_cache_hit_writeback(point_commands,point_commands_used*sizeof(*point_commands));
    rdpq_exec(point_commands,point_commands_used*sizeof(*point_commands));
    /* Raw draws bypass RDPQ's pipe tracking. Fence state changes explicitly. */
    rdpq_sync_pipe();
    fill_mode=false;
}
void label(float x,float y,int style,const char *s) {
    fill_mode=false;
    /* The built-in bitmap font has integer advances. Centering can put its
       origin on a half pixel, where RDP coverage/point sampling clips strokes.
       Snap every text origin, including shadows and edge-aligned labels. */
    x=roundf(x); y=roundf(y);
    if(!recording_static && strlen(s)<sizeof(text_cache[0].text)) {
        int oldest=-1;
        for(unsigned i=0;i<sizeof(text_cache)/sizeof(text_cache[0]);i++) {
            if(text_cache[i].block && text_cache[i].x==x && text_cache[i].y==y &&
                    text_cache[i].style==style && !strcmp(text_cache[i].text,s)) {
                text_cache[i].frame=render_frame;
                rspq_block_run(text_cache[i].block);
                return;
            }
            if(text_cache[i].frame!=render_frame &&
                    (oldest<0 || text_cache[i].frame<text_cache[oldest].frame)) oldest=(int)i;
        }
        if(oldest>=0) {
            if(text_cache[oldest].block) rspq_block_free(text_cache[oldest].block);
            text_cache[oldest].x=x; text_cache[oldest].y=y;
            text_cache[oldest].style=style; text_cache[oldest].frame=render_frame;
            strcpy(text_cache[oldest].text,s);
            rspq_block_begin();
            rdpq_text_print(&(rdpq_textparms_t){.style_id=style},1,x,y,s);
            text_cache[oldest].block=rspq_block_end();
            rspq_block_run(text_cache[oldest].block);
            return;
        }
    }
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
void draw_fluid(const Fluid *f,float x,float y,float width,float height,bool speed) {
    fill_mode=false;
    /* Generate all pixels together so bank selection and call overhead stay
       outside the cell loop. Preserve the padded RGBA32 upload layout. */
#ifdef PLASMAPONG_FLUID_PROFILE
    uint64_t pixel_begin=get_ticks();
#endif
    if(speed) {
#ifdef PLASMAPONG_SPEED_RSP
        fluid_speed_pixels_rsp(f,ink.buffer,ink.stride/sizeof(uint32_t));
#else
        /* CPU writers use cached stores, then expose complete rows to RDP. */
        void *pixels=CachedAddr(ink.buffer);
        fluid_speed_pixels(f,pixels,ink.stride/sizeof(uint32_t));
        data_cache_hit_writeback(pixels,ink.stride*FH);
#endif
    } else {
#ifdef PLASMAPONG_PREPARE_RSP
        fluid_pixels_rsp(f,ink.buffer,ink.stride/sizeof(uint32_t));
#else
        void *pixels=CachedAddr(ink.buffer);
        fluid_pixels(f,pixels,ink.stride/sizeof(uint32_t));
        data_cache_hit_writeback(pixels,ink.stride*FH);
#endif
    }
#ifdef PLASMAPONG_FLUID_PROFILE
    pixel_ticks+=get_ticks()-pixel_begin;
    uint64_t texture_begin=get_ticks();
#endif
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
#ifdef PLASMAPONG_FLUID_PROFILE
    texture_ticks+=get_ticks()-texture_begin;
#endif
}
int main(void) {
    debug_init_isviewer(); debug_init_emulog(); timer_init(); joypad_init();
    register_VI_handler(perf_vi);
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
#ifdef PLASMAPONG_VELOCITY_TEST
    velocity_cases();
#ifdef PLASMAPONG_PREPARE_RSP
    prepare_cases();
    gradient_cases();
#endif
#endif
#ifdef PLASMAPONG_CONFINEMENT_TEST
    confinement_cases();
#endif
    rdpq_font_t *font=rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_VAR);
    const uint32_t colors[]={0xeaf6ff,0xa0b3c9,0x48dcff,0xff637e,0xffffff,0x02040a,0x737d8a,0x70ffd0,0xc28aff,MENU_GOLD};
    for(unsigned i=0;i<sizeof(colors)/sizeof(colors[0]);i++) rdpq_font_style(font,i,&(rdpq_fontstyle_t){.color=color(colors[i])});
    rdpq_text_register_font(1,font); game_init(&game); scores_load(&game); game.menu_rng=(uint32_t)get_ticks();
#ifdef PLASMAPONG_SMOKE_FPS
    _Static_assert(PLASMAPONG_SMOKE_FPS==30 || PLASMAPONG_SMOKE_FPS==60,"valid benchmark frame rate");
    game.frame_rate=(FrameRate)PLASMAPONG_SMOKE_FPS;
#endif
#ifdef PLASMAPONG_SAVE_SMOKE
    save_smoke(&game);
#endif
    uint64_t previous=get_ticks(); float accumulator=0;
    uint64_t sim_ticks=0; unsigned sim_steps=0;
    uint64_t draw_ticks=0; unsigned draw_frames=0;
    uint64_t frame_window=get_ticks(); unsigned frame_steps=0;
    Phase perf_phase=game.phase;
    FrameRate perf_rate=game.frame_rate;
    bool perf_l=false,perf_r=false;
    perf_reset();
    debugf("Plasma Pong 64: ready, %u-byte game state\n",(unsigned)sizeof(game));
    while(1) {
        const float frame_step=game_dt(&game);
        uint64_t now=get_ticks();
        float elapsed=TIMER_MICROS_LL(now-previous)*.000001f; previous=now;
        /* Bound catch-up after stalls; fixed physics works on PAL and NTSC. */
        accumulator+=minf(elapsed,.1f);
        /* There is no render interpolation: a frame with no new simulation
           step repeats the same image. Reserve that time for the next selected-rate
           update instead of spending it regenerating/uploading the texture.
           Interrupts remain enabled so audio continues during this wait. */
        if(accumulator<frame_step) {
            /* Round up: sub-microsecond remainders must still make progress. */
            wait_ticks(TICKS_FROM_US(1+(uint32_t)((frame_step-accumulator)*1000000.0f)));
            continue;
        }
        joypad_poll(); Input input[MAX_PLAYERS]={0};
        for(int p=0;p<MAX_PLAYERS;p++) {
            joypad_inputs_t in=joypad_get_inputs((joypad_port_t)p);
            if(p==0) {
                if(in.btn.l && !perf_l) perf_visible=!perf_visible;
                if(in.btn.r && !perf_r) perf_reset();
                perf_l=in.btn.l; perf_r=in.btn.r;
            }
            input[p]=(Input){.connected=joypad_get_style((joypad_port_t)p)==JOYPAD_STYLE_N64,
                .a=in.btn.a,.z=in.btn.z,.start=in.btn.start,.b=in.btn.b,
                .x=in.stick_x/80.0f,.y=in.stick_y/80.0f};
            if(in.btn.d_up) input[p].y=1;
            if(in.btn.d_down) input[p].y=-1;
            if(in.btn.d_left) input[p].x=-1;
            if(in.btn.d_right) input[p].x=1;
        }
        while(accumulator>=frame_step) {
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
            if(game.phase==MENU) ui_measure_menu(&game);
            game_step(&game,input); accumulator-=frame_step;
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
                    debugf("Plasma Pong 64: simulation average %llu us/step (budget %u us)\n",
                        (unsigned long long)(TIMER_MICROS_LL(sim_ticks)/sim_steps),(unsigned)(game_dt(&game)*1000000+.5f));
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
#ifdef PLASMAPONG_FLUID_PROFILE
        wait_draw_ticks+=get_ticks()-draw_begin;
#endif
        render_frame++;
        /* The frame-start RSP/RDP wait also releases the raw point buffers. */
        point_commands_used=0;
        if(game.phase!=perf_phase || game.frame_rate!=perf_rate) {
            perf_reset(); perf_phase=game.phase; perf_rate=game.frame_rate;
        }
        perf_update();
        fill_mode=false;
        rdpq_attach(frame,NULL);
        ui_draw(&game);
        if(perf_visible) {
            rect(14,27,250,28,0x09111f);
            label(18,39,0,perf_text); label(18,51,0,perf_gap);
        }
        rdpq_detach_show();
#ifdef PLASMAPONG_DRAW_SYNC_PROFILE
        /* Diagnostic only: include RSP/RDP completion, not just submission. */
        rspq_wait();
#endif
        draw_ticks+=get_ticks()-draw_begin;
        if(++draw_frames==150) {
            debugf("Plasma Pong 64: draw average %llu us/frame\n",
                (unsigned long long)(TIMER_MICROS_LL(draw_ticks)/draw_frames));
            uint64_t window_end=get_ticks();
            debugf("Plasma Pong 64: frame interval %llu us (%u steps / %u frames)\n",
                (unsigned long long)(TIMER_MICROS_LL(window_end-frame_window)/draw_frames),
                frame_steps,draw_frames);
            frame_window=window_end; frame_steps=0;
            PresentationStats shown=perf_snapshot();
            debugf("Presentation: %lu new / %lu VI, %lu repeats, longest %lu VI gap (%lu missed, target %u FPS)\n",
                (unsigned long)shown.frames,(unsigned long)shown.refreshes,
                (unsigned long)shown.repeats,(unsigned long)shown.longest_gap,
                (unsigned long)shown.misses,(unsigned)game.frame_rate);
#ifdef PLASMAPONG_FLUID_PROFILE
            debugf("Draw profile: pixels %llu us, texture %llu us, wait %llu us, other %llu us/frame\n",
                (unsigned long long)TIMER_MICROS_LL(pixel_ticks)/draw_frames,
                (unsigned long long)TIMER_MICROS_LL(texture_ticks)/draw_frames,
                (unsigned long long)TIMER_MICROS_LL(wait_draw_ticks)/draw_frames,
                (unsigned long long)TIMER_MICROS_LL(draw_ticks-pixel_ticks-texture_ticks-wait_draw_ticks)/draw_frames);
            debugf("Flow draw: prepare %llu us, emit %llu us/frame\n",
                (unsigned long long)TIMER_MICROS_LL(flow_prepare_ticks)/draw_frames,
                (unsigned long long)TIMER_MICROS_LL(flow_emit_ticks)/draw_frames);
            flow_prepare_ticks=flow_emit_ticks=0;
            pixel_ticks=texture_ticks=wait_draw_ticks=0;
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
