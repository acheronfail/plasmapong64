#include "mathutil.h"
#include "sc64_reload.h"
#include <libdragon.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include "draw.h"
#include "sound.h"
#include "music.h"
#include "save.h"
#include "fluid_profile.h"
#ifdef PLASMAPONG_FLUID_HIGHPRI
#include "fluid_queue.h"
bool fluid_highpri_active;
#ifdef PLASMAPONG_FLUID_HIGHPRI_YIELD
bool fluid_highpri_open;
#endif
#endif
#include "perf.h"
#ifdef PLASMAPONG_UPWIND_RSP
#include "fluid_upwind.h"
#endif
#ifdef PLASMAPONG_UPWIND_TEST
#include "../tests/upwind_cases.h"
#endif
#ifdef PLASMAPONG_PREPARE_TEST
#include "../tests/gradient_short_cases.h"
#endif
#ifdef PLASMAPONG_PREPARE_RSP
#include "fluid_velocity_fixed.h"
#endif
#ifdef PLASMAPONG_PREPARE_TEST
#ifdef PLASMAPONG_PREPARE_RSP
#include "../tests/prepare_cases.h"
#endif
#endif
#ifdef PLASMAPONG_RSP_TEST
#include "../tests/rsp_fluid_smoke.h"
#ifdef PLASMAPONG_VELOCITY_CHAIN
#include "../tests/velocity_chain_cases.h"
#endif
#endif
#ifdef PLASMAPONG_SAVE_SMOKE
#include "../tests/save_smoke.h"
#endif
#ifdef PLASMAPONG_SMOKE
#include "../tests/rom_smoke.h"
#endif
static Game game;
static Sound sound;
static Music music;
static uint32_t music_rom[MUSIC_TRACKS];
static void music_read(void *context,unsigned track,uint32_t frame,int16_t *out,unsigned frames) {
    const uint32_t *rom=context;
    data_cache_hit_invalidate(out,MUSIC_BUFFER_FRAMES*4);
    dma_read(out,rom[track]+frame*4,frames*4);
}
static unsigned draw_scale=1;
static uint8_t draw_font=1;
static FrameRate video_rate;
#ifdef PLASMAPONG_PIXELS_CHAIN
static rspq_syncpoint_t pixel_done;
static bool pixel_pending;
#endif
static volatile PresentationStats presentation;
static volatile uint32_t vi_count;
static PerfControls perf_controls;
static char perf_text[64]="FPS -- / --", perf_gap[64]="MISS 0  GAP 1 VI";
static PresentationStats perf_window;
static void perf_vi(void) {
    /* VI_ORIGIN identifies the actual scanned-out surface. Normalize the
       alternating one-row offset in 480i before counting framebuffer swaps.
       Register before display_init: pinned libdragon prepends handlers, so
       its framebuffer swap runs before this sampler. No work is fenced here. */
    uint32_t origin=*(volatile uint32_t *)0xA4400004;
    bool odd_field=(*(volatile uint32_t *)0xA4400010)&1;
    uint64_t now=get_ticks();
    vi_count++;
    presentation_sample(&presentation,presentation_origin(origin,draw_scale==2,odd_field,640*2),now);
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
    music_mix(&music,buffer,frames);
#ifdef PLASMAPONG_FLUID_PROFILE
    audio_ticks+=get_ticks()-begin; audio_frames+=frames;
#endif
}
#ifdef PLASMAPONG_AUDIO_STREAM
static short *audio_buffer;
static unsigned audio_cursor,audio_quota;
static uint64_t audio_debt,audio_clock;
static unsigned audio_generated,audio_underruns;
/* Main-thread producer. AI interrupts only submit completed buffers. A buffer
   remains private until every sample has been generated and write_end is called. */
void audio_background(void) {
    while(audio_quota) {
        if(!audio_buffer) {
            if(!audio_can_write()) return;
            audio_buffer=audio_write_begin(); audio_cursor=0;
        }
        unsigned remaining=audio_get_buffer_length()-audio_cursor;
        unsigned count=audio_quota<remaining?audio_quota:remaining;
        if(count>128) count=128;
        fill_audio(audio_buffer+audio_cursor*2,count);
        audio_generated+=count;
        audio_cursor+=count; audio_quota-=count;
        audio_debt-=(uint64_t)count*TICKS_FROM_US(1000000);
        if(audio_cursor==(unsigned)audio_get_buffer_length()) {
            audio_write_end(); audio_buffer=NULL;
        }
    }
}
static void audio_reserve(void) {
    /* Mode changes may wait for VI and allocate new framebuffers. Fill spare
       buffers before that deliberate stall; it is outside steady play timing. */
    for(unsigned i=0;i<4;i++) {
        if(!audio_buffer) {
            if(!audio_can_write()) break;
            audio_buffer=audio_write_begin(); audio_cursor=0;
        }
        fill_audio(audio_buffer+audio_cursor*2,audio_get_buffer_length()-audio_cursor);
        audio_write_end(); audio_buffer=NULL;
    }
}
void audio_storage_service(void) {
    if(!audio_clock) return;
    /* Preserve the existing intentional EEPROM mute while keeping AI fed.
       Each page write is short enough to service between pages. Do not
       advance voices for silence or accumulate sample debt across the save. */
    for(unsigned i=0;i<4;i++) {
        if(!audio_buffer) {
            if(!audio_can_write()) break;
            audio_buffer=audio_write_begin(); audio_cursor=0;
        }
        memset(audio_buffer+audio_cursor*2,0,(audio_get_buffer_length()-audio_cursor)*4);
        audio_write_end(); audio_buffer=NULL;
    }
    audio_debt=0; audio_clock=get_ticks(); audio_quota=0;
}
#endif
static surface_t ink;
#ifdef PLASMAPONG_INK16
static surface_t ink16;
#endif
static rspq_block_t *ink_blit;
static float ink_x,ink_y,ink_w,ink_h;
static bool fill_mode;
static uint32_t fill_color;
static _Alignas(16) uint64_t point_commands[DRAW_POINT_COMMAND_CAPACITY];
static unsigned point_commands_used;
#ifdef PLASMAPONG_MENU_TEMPLATE
#include "rdpq_internal.h"
extern rdpq_block_state_t rdpq_block_state;
static rspq_block_t *menu_template;
static unsigned menu_template_key;
static bool recording_menu_template,collecting_menu_points;
static uint64_t *menu_template_points;
static void menu_template_patch_points(void) {
    unsigned capacity=sizeof(point_commands)/sizeof(*point_commands);
    assert(menu_template_points && point_commands_used<=capacity);
    memcpy(menu_template_points,point_commands,point_commands_used*sizeof(*point_commands));
    for(unsigned i=point_commands_used;i<capacity;i++)
        menu_template_points[i]=0xC000000000000000ull;
    data_cache_hit_writeback(menu_template_points,sizeof(point_commands));
}
#endif
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
#ifdef PLASMAPONG_FRAME_WORK_PROFILE
static void profile_frame_work(uint64_t begin,uint64_t buffer_wait,Phase phase) {
    /* Include simulation, interrupts, buffer acquisition and completed RDP
       drawing, excluding deliberate rate-limiter sleep. Quantiles are upper
       bounds in 250-us buckets; maximum and average retain exact microseconds. */
    static unsigned bins[2][256],frames,maximum[2],wait_max;
    static uint64_t total[2],wait_total;
    static Phase measured_phase;
    static FrameRate measured_mode;
    if(phase!=measured_phase || game.frame_rate!=measured_mode) {
        memset(bins,0,sizeof(bins)); frames=wait_max=0;
        memset(maximum,0,sizeof(maximum)); memset(total,0,sizeof(total)); wait_total=0;
        measured_phase=phase;
        measured_mode=game.frame_rate;
    }
    unsigned us=TIMER_MICROS_LL(get_ticks()-begin);
    unsigned wait_us=TIMER_MICROS_LL(buffer_wait);
    wait_total+=wait_us; if(wait_us>wait_max) wait_max=wait_us;
    for(unsigned mode=0;mode<2;mode++) {
        unsigned value=mode?us-wait_us:us;
        unsigned bucket=(value+249)/250;
        bins[mode][bucket<256?bucket:255]++; total[mode]+=value;
        if(value>maximum[mode]) maximum[mode]=value;
    }
    frames++;
    if(frames==150) {
        for(unsigned mode=0;mode<2;mode++) {
            unsigned count=0,p50=0,p95=0,p99=0;
            for(unsigned i=0;i<256;i++) {
                count+=bins[mode][i];
                unsigned bound=i==255?maximum[mode]:i*250;
                if(!p50 && count>=75) p50=bound;
                if(!p95 && count>=143) p95=bound;
                if(!p99 && count>=149) p99=bound;
            }
            static const char *names[]={"MENU","LOBBY","PLAY","PAUSED","FINISHED","SCORES","OPTIONS"};
            debugf("Frame %s: average %u us, p50 <= %u us, p95 <= %u us, p99 <= %u us, max %u us (150 %s frames)\n",
                mode?"active":"work",(unsigned)(total[mode]/frames),p50,p95,p99,maximum[mode],names[phase]);
        }
        debugf("Buffer wait: average %u us, max %u us\n",(unsigned)(wait_total/frames),wait_max);
        memset(bins,0,sizeof(bins)); frames=wait_max=0;
        memset(maximum,0,sizeof(maximum)); memset(total,0,sizeof(total)); wait_total=0;
    }
}
#endif
static bool recording_static;
#ifdef PLASMAPONG_MENU_BUFFER_KIB
#include "rdpq_internal.h"
extern rdpq_block_state_t rdpq_block_state;
_Static_assert(PLASMAPONG_MENU_BUFFER_KIB>=8 && PLASMAPONG_MENU_BUFFER_KIB<=32,
    "experimental menu buffer must be 8-32 KiB");
#endif
#ifdef PLASMAPONG_MENU_LABEL_BLOCK
static rspq_block_t *menu_foreground;
static unsigned menu_foreground_key;
void draw_menu_foreground(const Game *g) {
#ifdef PLASMAPONG_MENU_TEMPLATE
    if(recording_menu_template) { ui_menu_foreground(g); return; }
#endif
    unsigned pads=0;
    for(unsigned p=0;p<MAX_PLAYERS;p++) pads+=!!g->connected[p];
    unsigned key=g->menu_selection|(g->players<<4)|(pads<<8);
    if(!menu_foreground || key!=menu_foreground_key) {
        /* The frame-start queue wait retires the previous use before rebuilding.
           Record title, triangles, shadows and labels into one static RDP stream. */
        if(menu_foreground) rspq_block_free(menu_foreground);
        menu_foreground_key=key;
        recording_static=true; fill_mode=false;
        rspq_block_begin();
#ifdef PLASMAPONG_MENU_BUFFER_KIB
        rdpq_block_state.bufsize=PLASMAPONG_MENU_BUFFER_KIB*1024/sizeof(uint32_t);
#endif
        ui_menu_foreground(g);
        menu_foreground=rspq_block_end();
        recording_static=false;
#ifdef PLASMAPONG_MENU_BUFFER_KIB
        unsigned buffers=0;
        for(rdpq_block_t *b=menu_foreground->rdp_block;b;b=b->next) buffers++;
        debugf("Menu foreground: %u RDP buffers, initial capacity %u KiB\n",
            buffers,PLASMAPONG_MENU_BUFFER_KIB);
#endif
    }
    rspq_block_run(menu_foreground); fill_mode=false;
}
#endif
#ifdef PLASMAPONG_FRAME_BLOCK
static bool recording_frame;
static rspq_block_t *frame_draw;
#endif
static rspq_block_t *static_draw[DRAW_STATIC_COUNT];
void draw_static(unsigned id,void (*draw)(void)) {
    assert(id<DRAW_STATIC_COUNT);
    fill_mode=false;
    if(recording_static) { draw(); fill_mode=false; return; }
#ifdef PLASMAPONG_FRAME_BLOCK
    if(recording_frame) { draw(); fill_mode=false; return; }
#endif
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
    rdpq_fill_rectangle(x*draw_scale,y*draw_scale,(x+w)*draw_scale,(y+h)*draw_scale);
}
void draw_points(const DrawPoint *points,unsigned count,uint32_t c) {
    if(!count) return;
#if defined(PLASMAPONG_FRAME_BLOCK) || defined(PLASMAPONG_DRAW_STREAM)
#ifdef PLASMAPONG_DRAW_STREAM
    {
#else
    if(recording_frame) {
#endif
        rdpq_set_mode_fill(color(c));
        for(unsigned i=0;i<count;i++) {
            assert(points[i].x<320 && points[i].y<240);
            unsigned x=points[i].x*draw_scale,y=points[i].y*draw_scale;
            rdpq_fill_rectangle(x,y,x+draw_scale,y+draw_scale);
        }
        fill_mode=false;
        return;
    }
#endif
    assert(point_commands_used+count+2<=sizeof(point_commands)/sizeof(*point_commands));
    if(!point_commands_used
#ifdef PLASMAPONG_MENU_TEMPLATE
        && !collecting_menu_points
#endif
        ) rdpq_set_mode_fill(RGBA32(0,0,0,255));
    uint64_t *commands=point_commands+point_commands_used;
    /* Pipe sync before each color change; 16-bit fill repeats the same pixel. */
    uint32_t pixel=color_to_packed16(color(c));
    *commands++=0xE700000000000000ull;
    *commands++=0xF700000000000000ull|((uint64_t)pixel<<16)|pixel;
    for(unsigned i=0;i<count;i++) {
        assert(points[i].x<320 && points[i].y<240);
        uint32_t x=points[i].x*draw_scale,y=points[i].y*draw_scale;
        uint32_t xy=(x<<14)|(y<<2);
        /* Fill-cycle lower-right is inclusive. One logical pixel becomes a
           2x2 square in high res, preserving particle brightness and size. */
        uint32_t end=((x+draw_scale-1)<<14)|((y+draw_scale-1)<<2);
        commands[i]=((uint64_t)(0xF6000000u|end)<<32)|xy;
    }
    point_commands_used+=count+2;
}
void draw_points_end(void) {
#ifdef PLASMAPONG_MENU_TEMPLATE
    if(collecting_menu_points) return;
    if(recording_menu_template) {
        if(!point_commands_used) rdpq_set_mode_fill(RGBA32(0,0,0,255));
        unsigned words=2*(sizeof(point_commands)/sizeof(*point_commands));
        while(!rdpq_block_state.wptr || rdpq_block_state.wptr+words+2>rdpq_block_state.wend)
            __rdpq_block_next_buffer();
        /* Isolate mutable slots on complete CPU cache lines, so writeback
           cannot overwrite neighboring commands fixed up by the RSP. */
        if(PhysicalAddr(rdpq_block_state.wptr)&15) __rdpq_write8(0,0,0);
        volatile uint32_t *begin=rdpq_block_state.wptr;
        menu_template_points=CachedAddr(begin);
        menu_template_patch_points();
        __rdpq_block_update(begin+words);
        rdpq_sync_pipe(); fill_mode=false;
        return;
    }
#endif
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
    if(!recording_static
#ifdef PLASMAPONG_FRAME_BLOCK
        && !recording_frame
#endif
        && strlen(s)<sizeof(text_cache[0].text)) {
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
            rdpq_text_print(&(rdpq_textparms_t){.style_id=style},draw_font,x*draw_scale,y*draw_scale,s);
            text_cache[oldest].block=rspq_block_end();
            rspq_block_run(text_cache[oldest].block);
            return;
        }
    }
    rdpq_text_print(&(rdpq_textparms_t){.style_id=style},draw_font,x*draw_scale,y*draw_scale,s);
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
static void draw_pixels(const Fluid *f,FlowEffect effect) {
#ifdef PLASMAPONG_FLUID_PROFILE
    uint64_t pixel_begin=get_ticks();
#endif
    bool speed=effect==FLOW_SPEED;
#if defined(PLASMAPONG_PREPARE_RSP) && !defined(PLASMAPONG_INK16)
    if(effect==FLOW_BANDS || effect==FLOW_RELIEF) {
        if(effect==FLOW_BANDS) fluid_bands_pixels_rsp_begin(f,ink.buffer,ink.stride/4);
        else fluid_relief_pixels_rsp_begin(f,ink.buffer,ink.stride/4);
#ifdef PLASMAPONG_PIXELS_CHAIN
        pixel_done=rspq_syncpoint_new(); pixel_pending=true;
        rspq_flush();
#else
        fluid_queue_wait();
#endif
#ifdef PLASMAPONG_FLUID_PROFILE
        pixel_ticks+=get_ticks()-pixel_begin;
#endif
        return;
    }
#endif
    if(effect>=FLOW_VORTEX) {
        FluidView view=flow_view(effect);
        void *pixels=CachedAddr(ink.buffer);
        fluid_view_pixels(f,pixels,ink.stride/sizeof(uint32_t),view);
        data_cache_hit_writeback(pixels,ink.stride*FH);
#ifdef PLASMAPONG_INK16_RSP
        /* The optional 16-bit renderer still consumes ink16 for CPU views. */
        uint16_t *destination=CachedAddr(ink16.buffer);
        for(unsigned row=0;row<FH;row++) for(unsigned col=0;col<FW;col++) {
            uint32_t p=((uint32_t *)pixels)[row*(ink.stride/4)+col];
            destination[row*(ink16.stride/2)+col]=((p>>16)&0xf800)|((p>>13)&0x07c0)|((p>>10)&0x003e)|1;
        }
        data_cache_hit_writeback(destination,ink16.stride*FH);
#endif
#ifdef PLASMAPONG_FLUID_PROFILE
        pixel_ticks+=get_ticks()-pixel_begin;
#endif
        return;
    }
    /* Generate all pixels together so bank selection and call overhead stay
       outside the cell loop. Preserve the padded RGBA32 upload layout. */
#ifdef PLASMAPONG_INK16_RSP
    if(speed) fluid_speed_pixels16_rsp_begin(f,ink16.buffer,ink16.stride/2);
    else fluid_pixels16_rsp_begin(f,ink16.buffer,ink16.stride/2);
    pixel_done=rspq_syncpoint_new(); pixel_pending=true;
    rspq_flush();
#ifdef PLASMAPONG_FLUID_PROFILE
    pixel_ticks+=get_ticks()-pixel_begin;
#endif
    return;
#endif
    if(speed) {
#ifdef PLASMAPONG_SPEED_RSP
#ifdef PLASMAPONG_PIXELS_CHAIN
        fluid_speed_pixels_rsp_begin(f,ink.buffer,ink.stride/sizeof(uint32_t));
        pixel_done=rspq_syncpoint_new(); pixel_pending=true;
        rspq_flush();
#else
        fluid_speed_pixels_rsp(f,ink.buffer,ink.stride/sizeof(uint32_t));
#endif
#else
        /* CPU writers use cached stores, then expose complete rows to RDP. */
        void *pixels=CachedAddr(ink.buffer);
        fluid_speed_pixels(f,pixels,ink.stride/sizeof(uint32_t));
        data_cache_hit_writeback(pixels,ink.stride*FH);
#endif
    } else {
#ifdef PLASMAPONG_PREPARE_RSP
#ifdef PLASMAPONG_PIXELS_CHAIN
        fluid_pixels_rsp_begin(f,ink.buffer,ink.stride/sizeof(uint32_t));
        pixel_done=rspq_syncpoint_new(); pixel_pending=true;
        rspq_flush();
#else
        fluid_pixels_rsp(f,ink.buffer,ink.stride/sizeof(uint32_t));
#endif
#else
        void *pixels=CachedAddr(ink.buffer);
        fluid_pixels(f,pixels,ink.stride/sizeof(uint32_t));
        data_cache_hit_writeback(pixels,ink.stride*FH);
#endif
    }
#ifdef PLASMAPONG_FLUID_PROFILE
    pixel_ticks+=get_ticks()-pixel_begin;
#endif
}
void draw_fluid(const Fluid *f,float x,float y,float width,float height,FlowEffect effect) {
    fill_mode=false;
#ifdef PLASMAPONG_FRAME_BLOCK
    if(!recording_frame)
#endif
#ifdef PLASMAPONG_MENU_TEMPLATE
    if(!recording_menu_template)
#endif
        draw_pixels(f,effect);
#ifdef PLASMAPONG_BENCH_FLAT_FLUID
    /* Diagnostic: retain producer/source synchronization, particles and UI,
       replacing only the textured backdrop's RDP work with a flat fill. */
    rect(x,y,width,height,0x070c17);
    return;
#endif
    surface_t *texture=&ink;
#if defined(PLASMAPONG_INK16) && !defined(PLASMAPONG_INK16_RSP)
    /* First measure the upload benefit using the established RGBA32 producer.
       If retained, packing can move to RSP rather than this CPU conversion. */
#ifdef PLASMAPONG_PIXELS_CHAIN
    if(pixel_pending) {
        rspq_flush(); rspq_syncpoint_wait(pixel_done); pixel_pending=false;
    }
#endif
    uint32_t *source=CachedAddr(ink.buffer);
    uint16_t *destination=CachedAddr(ink16.buffer);
    data_cache_hit_invalidate(source,ink.stride*FH);
    for(unsigned row=0;row<FH;row++) for(unsigned col=0;col<FW;col++) {
        uint32_t p=source[row*(ink.stride/4)+col];
        destination[row*(ink16.stride/2)+col]=((p>>16)&0xf800)|((p>>13)&0x07c0)|((p>>10)&0x003e)|1;
    }
    data_cache_hit_writeback(destination,ink16.stride*FH);
    texture=&ink16;
#elif defined(PLASMAPONG_INK16)
    texture=&ink16;
#endif
#ifdef PLASMAPONG_FLUID_PROFILE
    uint64_t texture_begin=get_ticks();
#endif
#if defined(PLASMAPONG_FRAME_BLOCK) || defined(PLASMAPONG_DRAW_STREAM) || defined(PLASMAPONG_MENU_TEMPLATE)
#ifdef PLASMAPONG_DRAW_STREAM
    {
#elif defined(PLASMAPONG_MENU_TEMPLATE)
    if(recording_menu_template) {
#else
    if(recording_frame) {
#endif
        /* Keep the texture, particles and text in the same command stream. */
        rdpq_set_mode_standard(); rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_tex_blit(texture,x*draw_scale,y*draw_scale,&(rdpq_blitparms_t){
            .width=FW,.height=FH,.scale_x=width*draw_scale/FW,.scale_y=height*draw_scale/FH,.filtering=true});
#ifdef PLASMAPONG_FLUID_PROFILE
        texture_ticks+=get_ticks()-texture_begin;
#endif
        return;
    }
#endif
    /* Geometry and texture address are constant until switching menu/court.
       Record upload/tiling commands once; pixel contents remain dynamic. The
       frame-start wait also makes freeing the previous block safe. */
    if(!ink_blit || x!=ink_x || y!=ink_y || width!=ink_w || height!=ink_h) {
        if(ink_blit) rspq_block_free(ink_blit);
        ink_x=x; ink_y=y; ink_w=width; ink_h=height;
        rspq_block_begin();
        rdpq_set_mode_standard(); rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_tex_blit(texture,x*draw_scale,y*draw_scale,&(rdpq_blitparms_t){
            .width=FW,.height=FH,.scale_x=width*draw_scale/FW,.scale_y=height*draw_scale/FH,.filtering=true});
        ink_blit=rspq_block_end();
    }
    /* The upload follows its pixel producer on the same queue. CPU tail
       preparation can overlap generation; frame-start wait protects reuse. */
    rspq_block_run(ink_blit);
#ifdef PLASMAPONG_FLUID_PROFILE
    texture_ticks+=get_ticks()-texture_begin;
#endif
}
static void video_configure(FrameRate rate) {
#ifdef PLASMAPONG_AUDIO_STREAM
    if(audio_clock) audio_reserve();
#endif
    /* All cached commands contain physical coordinates and font/texture
       state. Finish queued work before replacing them or the framebuffers. */
    rspq_wait();
#ifdef PLASMAPONG_MENU_TEMPLATE
    if(menu_template) rspq_block_free(menu_template);
    menu_template=NULL; menu_template_points=NULL;
#endif
#ifdef PLASMAPONG_MENU_LABEL_BLOCK
    if(menu_foreground) rspq_block_free(menu_foreground);
    menu_foreground=NULL;
#endif
#ifdef PLASMAPONG_FRAME_BLOCK
    if(frame_draw) rspq_block_free(frame_draw);
    frame_draw=NULL;
#endif
#ifdef PLASMAPONG_PIXELS_CHAIN
    pixel_pending=false;
#endif
    for(unsigned i=0;i<DRAW_STATIC_COUNT;i++) {
        if(static_draw[i]) rspq_block_free(static_draw[i]);
        static_draw[i]=NULL;
    }
    for(unsigned i=0;i<sizeof(text_cache)/sizeof(*text_cache);i++) {
        if(text_cache[i].block) rspq_block_free(text_cache[i].block);
        text_cache[i].block=NULL;
    }
    if(ink_blit) rspq_block_free(ink_blit);
    ink_blit=NULL;
    if(video_rate) display_close();
    draw_scale=rate==FPS_30?2:1;
    draw_font=draw_scale==2?2:1;
    filter_options_t video_filter=FILTERS_RESAMPLE;
#ifdef PLASMAPONG_HIRES_VI_POINT
    /* libdragon forbids unfiltered NTSC 16-bit scanout at widths <= 320. */
    if(draw_scale==2) video_filter=FILTERS_DISABLED;
#endif
    display_init(draw_scale==2?RESOLUTION_640x480:RESOLUTION_320x240,
        DEPTH_16_BPP,3,GAMMA_NONE,video_filter);
    video_rate=rate;
    perf_reset();
    debugf("Plasma Pong 64: video %ux%u%s, %u FPS\n",320*draw_scale,240*draw_scale,
        draw_scale==2?" interlaced":" progressive",GAME_HZ);
#ifdef PLASMAPONG_HIRES_VI_POINT
    debugf("Plasma Pong 64: VI filter %s\n",
        draw_scale==2?"disabled":"resample");
#endif
#ifdef PLASMAPONG_AUDIO_STREAM
    if(audio_clock) {
        audio_reserve();
        audio_debt=0; audio_clock=get_ticks(); audio_quota=0;
    }
#endif
}
int main(void) {
    debug_init_isviewer(); debug_init_emulog(); timer_init(); joypad_init();
#ifdef PLASMAPONG_USB_LOG
    debug_init_usblog();
#endif
    register_VI_handler(perf_vi);
    /* Pad rows so partial-width uploads use LoadTile. The pinned libdragon's
       RGBA32 LoadBlock path corrupts this non-power-of-two texture width. */
    rdpq_init(); ink=surface_alloc(FMT_RGBA32,64,FH);
#ifdef PLASMAPONG_INK16
    ink16=surface_alloc(FMT_RGBA16,64,FH);
#endif
#ifdef PLASMAPONG_FLUID_HIGHPRI
    /* Register every simulation overlay before entering high-priority mode. */
    fluid_pressure_rsp_init(); fluid_dye_rsp_init();
    fluid_prepare_rsp_init();
    fluid_gradient_short_rsp_init();
#ifdef PLASMAPONG_UPWIND_RSP
    fluid_upwind_rsp_init();
#endif
#endif
#ifdef PLASMAPONG_RDP_VALIDATE
    rdpq_debug_start();
#endif
#ifdef PLASMAPONG_UPWIND_TEST
    upwind_cases();
#endif
#ifdef PLASMAPONG_PREPARE_TEST
    gradient_short_cases();
#endif
#ifdef PLASMAPONG_RSP_TEST
    rsp_fluid_smoke();
#ifdef PLASMAPONG_VELOCITY_CHAIN
    velocity_chain_cases();
#endif
#endif
#ifdef PLASMAPONG_PREPARE_TEST
#ifdef PLASMAPONG_PREPARE_RSP
    prepare_cases();
#endif
#endif
    dfs_init(DFS_DEFAULT_LOCATION);
    rdpq_font_t *font=rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_VAR);
    rdpq_font_t *hi_font=rdpq_font_load("rom:/at01-2x.font64");
    const uint32_t colors[]={0xeaf6ff,0xa0b3c9,0x48dcff,0xff637e,0xffffff,0x02040a,0x737d8a,0x70ffd0,0xc28aff,MENU_GOLD};
    for(unsigned i=0;i<sizeof(colors)/sizeof(colors[0]);i++) {
        rdpq_font_style(font,i,&(rdpq_fontstyle_t){.color=color(colors[i])});
        rdpq_font_style(hi_font,i,&(rdpq_fontstyle_t){.color=color(colors[i])});
    }
    rdpq_text_register_font(2,hi_font);
    rdpq_text_register_font(1,font); game_init(&game); scores_load(&game); game.menu_rng=(uint32_t)get_ticks();
#ifdef PLASMAPONG_BENCH_MENU
    /* Repeatable hardware/emulator menu baseline, independent of EEPROM. */
    game.frame_rate=FPS_30; game.flow_effect=FLOW_PARTICLES;
    game.menu_rng=0x76a51c93u;
#endif
#ifdef PLASMAPONG_SMOKE_FPS
    _Static_assert(PLASMAPONG_SMOKE_FPS==30 || PLASMAPONG_SMOKE_FPS==60,"valid legacy benchmark resolution selector");
    game.frame_rate=(FrameRate)PLASMAPONG_SMOKE_FPS;
#endif
#ifdef PLASMAPONG_SMOKE_HIGH_RES
    game.frame_rate=FPS_30;
#endif
#ifdef PLASMAPONG_SAVE_SMOKE
    save_smoke(&game);
#endif
    perf_controls_step(&perf_controls,game.fps_meter,false,false);
    video_configure(game.frame_rate);
    /* Build the first scene's immutable/text commands during startup. This
       first visible menu is initialization, before the timed update loop. */
    uint64_t startup_begin=get_ticks();
    ui_init();
    surface_t *startup_frame=display_get();
    ui_measure_menu(&game);
    fill_mode=false;
    rdpq_attach(startup_frame,NULL); ui_draw(&game);
    if(game.fps_meter && perf_controls.visible) {
        rect(14,27,250,28,0x09111f);
        label(18,39,0,perf_text); label(18,51,0,perf_gap);
    }
    rdpq_detach_show(); rspq_wait();
    debugf("Plasma Pong 64: startup render %llu us (outside update budget)\n",
        (unsigned long long)TIMER_MICROS_LL(get_ticks()-startup_begin));
    /* Start playback after loading and the first render, so they cannot
       consume the producer's initial buffer reserve or create sample debt. */
    audio_init(SOUND_RATE,4); sound_init(&sound,audio_get_frequency());
    const char *music_paths[MUSIC_TRACKS]={"/intro_loop.pcm","/battle_loop.pcm"};
    uint32_t music_lengths[MUSIC_TRACKS];
    for(unsigned i=0;i<MUSIC_TRACKS;i++) {
        music_rom[i]=dfs_rom_addr(music_paths[i]);
        int bytes=dfs_rom_size(music_paths[i]);
        assert(music_rom[i] && bytes>0 && bytes%4==0);
        music_lengths[i]=(unsigned)bytes/4;
    }
    music_init(&music,audio_get_frequency(),music_lengths,music_read,music_rom);
    music_update(&music,&game);
#ifdef PLASMAPONG_AUDIO_STREAM
    audio_write_silence(); audio_write_silence();
    audio_clock=get_ticks();
#else
    audio_set_buffer_callback(fill_audio); audio_write_silence();
#endif
    /* The startup render already consumed part of the first field's time. */
    uint64_t previous=startup_begin; float accumulator=0;
    uint32_t previous_vi=vi_count;
    uint64_t sim_ticks=0; unsigned sim_steps=0;
    uint64_t draw_ticks=0; unsigned draw_frames=0;
    uint64_t frame_window=get_ticks(); unsigned frame_steps=0;
    Phase perf_phase=game.phase;
    FrameRate perf_rate=game.frame_rate;
    perf_reset();
    debugf("Plasma Pong 64: ready, %u-byte game state, %u MiB RDRAM\n",(unsigned)sizeof(game),(unsigned)get_memory_size()/(1024*1024));
    debugf("Fluid grid: %u x %u (%u cells), cell Q4 %u, pressure sweeps %u, compact %u\n",
        (unsigned)FW,(unsigned)FH,(unsigned)FN,(unsigned)PLASMAPONG_CELL_Q4,
        (unsigned)PLASMAPONG_PRESSURE_PASSES,1u);
#ifdef PLASMAPONG_USB_LOG
    debugf("Hardware benchmark: TV %s, completed-frame fence %u, fluid profile %u\n",
        get_tv_type()==TV_PAL?"PAL":get_tv_type()==TV_MPAL?"MPAL":"NTSC",
#ifdef PLASMAPONG_FRAME_WORK_PROFILE
        1u,
#else
        0u,
#endif
#ifdef PLASMAPONG_FLUID_PROFILE
        1u);
#else
        0u);
#endif
#ifdef PLASMAPONG_QUEUE_PROFILE
    debugf("Hardware benchmark: pre-advection RSP queue-wait split enabled\n");
#endif
#endif
    while(1) {
        sc64_reload_poll();
        /* Start NTSC/MPAL work after each real VI field. An estimated clock
           period can drift across a display deadline even when work fits.
           Physics retains its nominal 1/60 step; PAL uses clock catch-up. */
        const float frame_step=game_dt(&game);
        bool field_paced=get_tv_type()!=TV_PAL;
        uint32_t fields=0;
        if(field_paced) {
            while(vi_count==previous_vi) wait_ticks(TICKS_FROM_US(50));
            uint32_t current_vi=vi_count;
            fields=current_vi-previous_vi; previous_vi=current_vi;
        }
        uint64_t now=get_ticks();
        float elapsed=TIMER_MICROS_LL(now-previous)*.000001f; previous=now;
        /* Bound catch-up after stalls; fixed physics works on PAL and NTSC. */
        accumulator+=minf(field_paced?fields*frame_step:elapsed,.1f);
        /* There is no render interpolation: a frame with no new simulation
           step repeats the same image. Reserve that time for the next selected-rate
           update instead of spending it regenerating/uploading the texture.
           Interrupts remain enabled so audio continues during this wait. */
        if(accumulator<frame_step) {
            /* Round up: sub-microsecond remainders must still make progress. */
            wait_ticks(TICKS_FROM_US(1+(uint32_t)((frame_step-accumulator)*1000000.0f)));
            continue;
        }
#ifdef PLASMAPONG_FRAME_WORK_PROFILE
        uint64_t work_begin=get_ticks();
#endif
#ifdef PLASMAPONG_AUDIO_STREAM
        uint64_t audio_now=get_ticks();
        audio_debt+=(audio_now-audio_clock)*(unsigned)audio_get_frequency();
        audio_clock=audio_now;
        uint64_t owed=audio_debt/TICKS_FROM_US(1000000);
        /* Music must keep playing even when rendering falls below 42 FPS.
           The old 384-sample cap could never repay debt at slower frame rates.
           Bound catch-up by the four-buffer ring rather than a frame rate. */
        unsigned audio_capacity=4*(unsigned)audio_get_buffer_length();
        audio_quota=owed>audio_capacity?audio_capacity:(unsigned)owed;
        if(!(*(volatile uint32_t *)0xa450000c & (1u<<30))) audio_underruns++;
#endif
#ifdef PLASMAPONG_PIXELS_CHAIN
        /* The previous texture producer owns its source dye/velocity until
           this point. Finish that producer before CPU splats can dirty it;
           its subsequent RDP drawing can still overlap this simulation. */
        if(pixel_pending) {
            rspq_flush(); rspq_syncpoint_wait(pixel_done); pixel_pending=false;
        }
#endif
        joypad_poll(); Input input[MAX_PLAYERS]={0};
        for(int p=0;p<MAX_PLAYERS;p++) {
            joypad_inputs_t in=joypad_get_inputs((joypad_port_t)p);
            if(p==0) {
                if(perf_controls_step(&perf_controls,game.fps_meter,in.btn.l,in.btn.r)) perf_reset();
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
#ifdef PLASMAPONG_FLUID_HIGHPRI
            /* The pixel-producer guard above has released fluid source data.
               Simulation can now overtake queued drawing of its output texture. */
#ifndef PLASMAPONG_FLUID_HIGHPRI_YIELD
            rspq_highpri_begin();
#endif
            fluid_highpri_active=true;
#endif
            game_step(&game,input);
#ifdef PLASMAPONG_FLUID_HIGHPRI
            /* Lobby/paused steps may enqueue no commands. Force the normal
               rollover check before libdragon appends the batch epilogue. */
#ifdef PLASMAPONG_FLUID_HIGHPRI_YIELD
            fluid_queue_highpri_finish();
#else
            rspq_noop();
            rspq_highpri_end(); rspq_highpri_sync();
#endif
            fluid_highpri_active=false;
#endif
            accumulator-=frame_step;
            frame_steps++;
            disable_interrupts(); sound_update(&sound,&game); music_update(&music,&game); enable_interrupts();
            if(game.scores_dirty) {
                scores_store(&game);
                previous=get_ticks(); accumulator=0;
                previous_vi=vi_count;
            }
#ifdef PLASMAPONG_FLUID_PROFILE
            fluid_profile.enabled=false;
            if(game.phase!=PLAY
#ifdef PLASMAPONG_BENCH_MENU
                && game.phase!=MENU
#endif
            ) {
                memcpy(fluid_profile.ticks,profile_before,sizeof(profile_before));
                memcpy(fluid_profile.calls,calls_before,sizeof(calls_before));
            }
#endif
            if(game.phase==PLAY
#ifdef PLASMAPONG_BENCH_MENU
                || game.phase==MENU
#endif
            ) {
                sim_ticks+=get_ticks()-begin;
                if(++sim_steps==150) {
                    debugf("Plasma Pong 64: simulation average %llu us/step (budget %u us)\n",
                        (unsigned long long)(TIMER_MICROS_LL(sim_ticks)/sim_steps),(unsigned)(game_dt(&game)*1000000+.5f));
#ifdef PLASMAPONG_FLUID_PROFILE
                    static const char *names[PROFILE_COUNT]={
                        "velocity_advection","velocity_swap",
                        "divergence","pressure_solve","pressure_gradient",
                        "dye_advection","dye_swap","splat","pump","ball_dye","sample"
#ifdef PLASMAPONG_UPWIND_RSP
                        ,"upwind_limit_velocity","upwind_job_velocity","upwind_limit_dye","upwind_job_dye"
#endif
#ifdef PLASMAPONG_QUEUE_PROFILE
                        ,"queue_wait"
#endif
                    };
                    for(int i=0;i<PROFILE_COUNT;i++) {
                        debugf("Fluid profile: %s %llu us/step (%u calls)\n",names[i],
                            (unsigned long long)(TIMER_MICROS_LL(fluid_profile.ticks[i])/sim_steps),
                            fluid_profile.calls[i]);
                        fluid_profile.ticks[i]=0; fluid_profile.calls[i]=0;
                    }
#ifdef PLASMAPONG_QUEUE_PC_PROFILE
                    fluid_queue_pc_report();
#endif
#endif
                    sim_ticks=0; sim_steps=0;
                }
            }
        }
#ifdef PLASMAPONG_AUDIO_STREAM
        audio_background();
#endif
        if(video_rate!=game.frame_rate) {
            video_configure(game.frame_rate);
            /* Switching VI modes and EEPROM writes are outside play timing. */
            previous=get_ticks(); accumulator=0;
            previous_vi=vi_count;
            frame_window=previous; frame_steps=0;
            draw_ticks=0; draw_frames=0; sim_ticks=0; sim_steps=0;
#ifdef PLASMAPONG_FRAME_WORK_PROFILE
            work_begin=get_ticks();
#endif
        }
#ifdef PLASMAPONG_FRAME_WORK_PROFILE
        uint64_t buffer_begin=get_ticks();
#endif
        surface_t *frame=display_get();
#ifdef PLASMAPONG_FRAME_WORK_PROFILE
        uint64_t buffer_wait=get_ticks()-buffer_begin;
#endif
        uint64_t draw_begin=get_ticks();
        /* Finish the previous frame before overwriting its shared texture or
           freeing a blit block. The simulation above can run alongside RDP. */
        rspq_wait();
#ifdef PLASMAPONG_FLUID_PROFILE
        wait_draw_ticks+=get_ticks()-draw_begin;
#endif
        render_frame++;
#ifdef PLASMAPONG_FRAME_BLOCK
        if(frame_draw) rspq_block_free(frame_draw);
        frame_draw=NULL;
        /* Queue pixel production before recording: syncpoints cannot be
           created inside a block. The source and destination are protected
           by the ordinary previous-frame completion wait above. */
        draw_pixels(&game.fluid,game.flow_effect);
#endif
        /* The frame-start RSP/RDP wait also releases the raw point buffers. */
        point_commands_used=0;
        if(game.phase!=perf_phase || game.frame_rate!=perf_rate) {
            perf_reset(); perf_phase=game.phase; perf_rate=game.frame_rate;
        }
        perf_update();
        fill_mode=false;
        rdpq_attach(frame,NULL);
#ifdef PLASMAPONG_FRAME_BLOCK
        recording_frame=true;
        rspq_block_begin();
#endif
#ifdef PLASMAPONG_MENU_TEMPLATE
        if(game.phase==MENU &&
                (game.flow_effect==FLOW_PARTICLES || game.flow_effect==FLOW_TAILS)) {
            unsigned pads=0;
            for(unsigned p=0;p<MAX_PLAYERS;p++) pads+=!!game.connected[p];
            unsigned key=game.menu_selection|(game.players<<4)|(pads<<8)|(game.flow_effect<<12);
            draw_pixels(&game.fluid,game.flow_effect);
            if(!menu_template || key!=menu_template_key) {
                if(menu_template) rspq_block_free(menu_template);
                menu_template_key=key;
                recording_menu_template=recording_static=true;
                rspq_block_begin();
                rdpq_block_state.bufsize=32*1024/sizeof(uint32_t);
                ui_draw(&game);
                menu_template=rspq_block_end();
                recording_menu_template=recording_static=false;
                unsigned buffers=0;
                for(rdpq_block_t *b=menu_template->rdp_block;b;b=b->next) buffers++;
                debugf("Menu template: %u RDP buffers, %u particle slots\n",buffers,
                    (unsigned)(sizeof(point_commands)/sizeof(*point_commands)));
            } else {
                collecting_menu_points=true;
                ui_menu_particles(&game);
                collecting_menu_points=false;
                menu_template_patch_points();
            }
            rspq_block_run(menu_template); fill_mode=false;
        } else
#endif
        ui_draw(&game);
        if(game.fps_meter && perf_controls.visible) {
            rect(14,27,250,28,0x09111f);
            label(18,39,0,perf_text); label(18,51,0,perf_gap);
        }
#ifdef PLASMAPONG_FRAME_BLOCK
        frame_draw=rspq_block_end();
        recording_frame=false;
        rspq_block_run(frame_draw);
#endif
        rdpq_detach_show();
#if defined(PLASMAPONG_DRAW_SYNC_PROFILE) || defined(PLASMAPONG_FRAME_WORK_PROFILE)
        /* Diagnostic only: include RSP/RDP completion, not just submission. */
        rspq_wait();
#endif
        draw_ticks+=get_ticks()-draw_begin;
#ifdef PLASMAPONG_FRAME_WORK_PROFILE
        profile_frame_work(work_begin,buffer_wait,game.phase);
#endif
        if(++draw_frames==150) {
#ifdef PLASMAPONG_AUDIO_STREAM
            debugf("Audio stream: %u samples, %u underrun observations, %llu samples owed\n",
                audio_generated,audio_underruns,
                (unsigned long long)(audio_debt/TICKS_FROM_US(1000000)));
            audio_generated=audio_underruns=0;
#endif
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
                (unsigned long)shown.misses,GAME_HZ);
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
