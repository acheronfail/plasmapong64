#include <libdragon.h>
#include <assert.h>
#include "fluid_velocity_fixed.h"
#include "fluid_pressure.h"
#include "fluid_queue.h"
DEFINE_RSP_UCODE(rsp_prepare);
static uint32_t overlay_id;
#ifdef PLASMAPONG_AUDIO_STREAM
extern void audio_background(void);
#endif
_Static_assert(FW==48 && FH==33 && sizeof(FluidDyeTrace)==48,"RSP preparation grid/trace layout");
_Static_assert(FLUID_SPEED_PALETTE_SIZE==260,"RSP speed palette DMA layout");
_Static_assert(DYE_SCALE==8192 && VELOCITY_LIMIT==16383,"RSP preparation lane ranges");
_Static_assert(sizeof(FluidVelocityFixed)%16==0 && sizeof(FluidDyeFixed)%16==0 &&
        FLUID_TRACE_BATCHES*sizeof(FluidDyeTrace)%16==0,"DMA buffers must own complete cache lines");
static void prepare_init(void) {
    if(!overlay_id) { rspq_init(); overlay_id=rspq_overlay_register(&rsp_prepare); }
}
void fluid_prepare_rsp_init(void) { prepare_init(); }
static void prepare_wait(void) {
    fluid_queue_wait();
}
void fluid_velocity_trace_rsp_begin(FluidDyeTrace *trace,const FluidVelocityFixed *velocity,float grid_dt) {
    assert(((uintptr_t)trace&15)==0 && ((uintptr_t)velocity&15)==0);
    assert(grid_dt>=0 && grid_dt<=.125f);
    prepare_init();
    data_cache_hit_writeback(velocity,sizeof(*velocity));
    data_cache_hit_invalidate(trace,FLUID_TRACE_BATCHES*sizeof(*trace));
    unsigned step=(unsigned)(grid_dt*1048576+.5f);
    fluid_queue_begin();
    rspq_write(overlay_id,0,PhysicalAddr(velocity->u),PhysicalAddr(velocity->v),PhysicalAddr(trace),step);
}
void fluid_velocity_trace_rsp(FluidDyeTrace *trace,const FluidVelocityFixed *velocity,float grid_dt) {
    fluid_velocity_trace_rsp_begin(trace,velocity,grid_dt);
    prepare_wait();
    data_cache_hit_invalidate(trace,FLUID_TRACE_BATCHES*sizeof(*trace));
}
void fluid_pixels_rsp_begin(const Fluid *f,uint32_t *pixels,unsigned stride) {
    assert(((uintptr_t)pixels&15)==0 && stride>=FW && stride%4==0);
    const FluidDyeFixed *ink=fluid_dye(f);
    prepare_init();
    data_cache_hit_writeback(ink,sizeof(*ink));
    /* surface_alloc returns an uncached buffer; also support cacheable test
       buffers. The DMA writes only the first FW pixels of each padded row. */
    data_cache_hit_writeback_invalidate(CachedAddr(pixels),stride*FH*sizeof(*pixels));
    fluid_queue_begin();
    rspq_write(overlay_id,1,PhysicalAddr(ink),PhysicalAddr(pixels),stride*sizeof(*pixels),0);
}
void fluid_pixels_rsp(const Fluid *f,uint32_t *pixels,unsigned stride) {
    fluid_pixels_rsp_begin(f,pixels,stride);
    prepare_wait();
    data_cache_hit_invalidate(CachedAddr(pixels),stride*FH*sizeof(*pixels));
}
void fluid_pixels16_rsp_begin(const Fluid *f,uint16_t *pixels,unsigned stride) {
    assert(((uintptr_t)pixels&15)==0 && stride>=FW && stride%8==0);
    const FluidDyeFixed *ink=fluid_dye(f);
    prepare_init();
    data_cache_hit_writeback(ink,sizeof(*ink));
    data_cache_hit_writeback_invalidate(CachedAddr(pixels),stride*FH*sizeof(*pixels));
    fluid_queue_begin();
    rspq_write(overlay_id,5,PhysicalAddr(ink),PhysicalAddr(pixels),stride*sizeof(*pixels),1);
}

void fluid_divergence_rsp_begin(int32_t *divergence,const FluidVelocityFixed *velocity) {
    assert(((uintptr_t)divergence&15)==0 && ((uintptr_t)velocity&15)==0);
    prepare_init();
    data_cache_hit_writeback(velocity,sizeof(*velocity));
    /* Only complete interior rows are replaced. */
    data_cache_hit_invalidate(divergence+FW,(FH-2)*FW*sizeof(*divergence));
    fluid_queue_begin();
    rspq_write(overlay_id,2,PhysicalAddr(velocity->u),PhysicalAddr(velocity->v),PhysicalAddr(divergence));
}
void fluid_divergence_rsp(int32_t *divergence,const FluidVelocityFixed *velocity) {
    fluid_divergence_rsp_begin(divergence,velocity);
    prepare_wait();
    data_cache_hit_invalidate(divergence+FW,(FH-2)*FW*sizeof(*divergence));
}

void fluid_gradient_rsp(FluidVelocityFixed *velocity,const int32_t *pressure) {
    assert(((uintptr_t)pressure&15)==0 && ((uintptr_t)velocity&15)==0);
    prepare_init();
    data_cache_hit_writeback(pressure,FN*sizeof(*pressure));
    data_cache_hit_writeback_invalidate(velocity,sizeof(*velocity));
    fluid_queue_begin();
    rspq_write(overlay_id,3,PhysicalAddr(velocity->u),PhysicalAddr(velocity->v),PhysicalAddr(pressure));
#ifdef PLASMAPONG_AUDIO_STREAM
    /* Projection is queued; use its RSP execution time for bounded mixing. */
    rspq_flush();
    audio_background();
#endif
    prepare_wait();
    data_cache_hit_invalidate(velocity,sizeof(*velocity));
}
#ifdef PLASMAPONG_FLUID_RSP
void fluid_projection_rsp(FluidVelocityFixed *velocity,int32_t *divergence,int32_t *pressure) {
    /* Begin calls invalidate the intermediate outputs before their producer
       commands. Later consumer writebacks find no dirty CPU cache lines. */
    fluid_divergence_rsp_begin(divergence,velocity);
    fluid_pressure_rsp_begin(pressure,divergence);
    fluid_gradient_rsp(velocity,pressure);
    data_cache_hit_invalidate(divergence+FW,(FH-2)*FW*sizeof(*divergence));
    data_cache_hit_invalidate(pressure,FN*sizeof(*pressure));
}
#endif

void fluid_speed_pixels_rsp_begin(const Fluid *f,uint32_t *pixels,unsigned stride) {
    static _Alignas(16) uint32_t palette[FLUID_SPEED_PALETTE_SIZE];
    static bool ready;
    assert(((uintptr_t)pixels&15)==0 && stride>=FW && stride%4==0);
    prepare_init();
    if(!ready) {
        fluid_speed_palette(palette);
        data_cache_hit_writeback(palette,sizeof(palette));
        ready=true;
    }
    const FluidVelocityFixed *v=fluid_velocity(f);
    data_cache_hit_writeback(v,sizeof(*v));
    data_cache_hit_writeback_invalidate(CachedAddr(pixels),stride*FH*sizeof(*pixels));
    fluid_queue_begin();
    rspq_write(overlay_id,4,PhysicalAddr(v),PhysicalAddr(pixels),stride*sizeof(*pixels),PhysicalAddr(palette));
}
void fluid_speed_pixels_rsp(const Fluid *f,uint32_t *pixels,unsigned stride) {
    fluid_speed_pixels_rsp_begin(f,pixels,stride);
    prepare_wait();
    data_cache_hit_invalidate(CachedAddr(pixels),stride*FH*sizeof(*pixels));
}
void fluid_speed_pixels16_rsp_begin(const Fluid *f,uint16_t *pixels,unsigned stride) {
    static _Alignas(16) uint32_t palette[FLUID_SPEED_PALETTE_SIZE];
    static bool ready;
    assert(((uintptr_t)pixels&15)==0 && stride>=FW && stride%8==0 && stride*2<0x8000);
    prepare_init();
    if(!ready) {
        fluid_speed_palette(palette);
        for(unsigned i=0;i<FLUID_SPEED_PALETTE_SIZE;i++) {
            uint32_t p=palette[i];
            palette[i]=((p>>16)&0xf800)|((p>>13)&0x07c0)|((p>>10)&0x003e)|1;
        }
        data_cache_hit_writeback(palette,sizeof(palette)); ready=true;
    }
    const FluidVelocityFixed *v=fluid_velocity(f);
    data_cache_hit_writeback(v,sizeof(*v));
    data_cache_hit_writeback_invalidate(CachedAddr(pixels),stride*FH*sizeof(*pixels));
    fluid_queue_begin();
    rspq_write(overlay_id,6,PhysicalAddr(v),PhysicalAddr(pixels),stride*sizeof(*pixels),PhysicalAddr(palette));
}

void fluid_bands_pixels_rsp_begin(const Fluid *f,uint32_t *pixels,unsigned stride) {
    static _Alignas(16) uint32_t palette[FLUID_SPEED_PALETTE_SIZE];
    static bool ready;
    assert(((uintptr_t)pixels&15)==0 && stride>=FW && stride%4==0);
    prepare_init();
    if(!ready) {
        fluid_view_palette(FLUID_VIEW_BANDS,palette);
        data_cache_hit_writeback(palette,sizeof(palette)); ready=true;
    }
    const FluidVelocityFixed *v=fluid_velocity(f);
    data_cache_hit_writeback(v,sizeof(*v));
    data_cache_hit_writeback_invalidate(CachedAddr(pixels),stride*FH*sizeof(*pixels));
    fluid_queue_begin();
    rspq_write(overlay_id,4,PhysicalAddr(v),PhysicalAddr(pixels),stride*sizeof(*pixels),PhysicalAddr(palette));
}
void fluid_relief_pixels_rsp_begin(const Fluid *f,uint32_t *pixels,unsigned stride) {
    static _Alignas(16) int16_t shades[FN];
    assert(((uintptr_t)pixels&15)==0 && stride>=FW && stride%4==0);
    prepare_init();
    fluid_relief_shades(f,shades);
    const FluidDyeFixed *ink=fluid_dye(f);
    data_cache_hit_writeback(shades,sizeof(shades));
    data_cache_hit_writeback(ink,sizeof(*ink));
    data_cache_hit_writeback_invalidate(CachedAddr(pixels),stride*FH*sizeof(*pixels));
    fluid_queue_begin();
    rspq_write(overlay_id,7,PhysicalAddr(ink),PhysicalAddr(pixels),stride*sizeof(*pixels),PhysicalAddr(shades));
}
