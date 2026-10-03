#include <libdragon.h>
#include <assert.h>
#include "fluid_velocity_fixed.h"
DEFINE_RSP_UCODE(rsp_prepare);
static uint32_t overlay_id;
_Static_assert(FW==48 && FH==33 && sizeof(FluidDyeTrace)==48,"RSP preparation grid/trace layout");
_Static_assert(FLUID_SPEED_PALETTE_SIZE==260,"RSP speed palette DMA layout");
_Static_assert(DYE_SCALE==8192 && VELOCITY_LIMIT==16383,"RSP preparation lane ranges");
_Static_assert(sizeof(FluidVelocityFixed)%16==0 && sizeof(FluidDyeFixed)%16==0 &&
        FLUID_TRACE_BATCHES*sizeof(FluidDyeTrace)%16==0,"DMA buffers must own complete cache lines");
static void prepare_init(void) {
    if(!overlay_id) { rspq_init(); overlay_id=rspq_overlay_register(&rsp_prepare); }
}
static void prepare_wait(void) {
    rspq_syncpoint_t done=rspq_syncpoint_new();
    rspq_flush(); rspq_syncpoint_wait(done);
}
void fluid_velocity_trace_rsp_begin(FluidDyeTrace *trace,const FluidVelocityFixed *velocity,float grid_dt) {
    assert(((uintptr_t)trace&15)==0 && ((uintptr_t)velocity&15)==0);
    assert(grid_dt>=0 && grid_dt<=.125f);
    prepare_init();
    data_cache_hit_writeback(velocity,sizeof(*velocity));
    data_cache_hit_invalidate(trace,FLUID_TRACE_BATCHES*sizeof(*trace));
    unsigned step=(unsigned)(grid_dt*1048576+.5f);
    rspq_write(overlay_id,0,PhysicalAddr(velocity->u),PhysicalAddr(velocity->v),PhysicalAddr(trace),step);
}
void fluid_velocity_trace_rsp(FluidDyeTrace *trace,const FluidVelocityFixed *velocity,float grid_dt) {
    fluid_velocity_trace_rsp_begin(trace,velocity,grid_dt);
    prepare_wait();
    data_cache_hit_invalidate(trace,FLUID_TRACE_BATCHES*sizeof(*trace));
}
void fluid_pixels_rsp(const Fluid *f,uint32_t *pixels,unsigned stride) {
    assert(((uintptr_t)pixels&15)==0 && stride>=FW && stride%4==0);
    const FluidDyeFixed *ink=fluid_dye(f);
    prepare_init();
    data_cache_hit_writeback(ink,sizeof(*ink));
    /* surface_alloc returns an uncached buffer; also support cacheable test
       buffers. The DMA writes only the first FW pixels of each padded row. */
    data_cache_hit_writeback_invalidate(CachedAddr(pixels),stride*FH*sizeof(*pixels));
    rspq_write(overlay_id,1,PhysicalAddr(ink),PhysicalAddr(pixels),stride*sizeof(*pixels),0);
    prepare_wait();
    data_cache_hit_invalidate(CachedAddr(pixels),stride*FH*sizeof(*pixels));
}

void fluid_divergence_rsp(int32_t *divergence,const FluidVelocityFixed *velocity) {
    assert(((uintptr_t)divergence&15)==0 && ((uintptr_t)velocity&15)==0);
    prepare_init();
    data_cache_hit_writeback(velocity,sizeof(*velocity));
    /* Only complete interior rows are replaced. */
    data_cache_hit_invalidate(divergence+FW,(FH-2)*FW*sizeof(*divergence));
    rspq_write(overlay_id,2,PhysicalAddr(velocity->u),PhysicalAddr(velocity->v),PhysicalAddr(divergence));
    prepare_wait();
    data_cache_hit_invalidate(divergence+FW,(FH-2)*FW*sizeof(*divergence));
}

void fluid_gradient_rsp(FluidVelocityFixed *velocity,const int32_t *pressure) {
    assert(((uintptr_t)pressure&15)==0 && ((uintptr_t)velocity&15)==0);
    prepare_init();
    data_cache_hit_writeback(pressure,FN*sizeof(*pressure));
    data_cache_hit_writeback_invalidate(velocity,sizeof(*velocity));
    rspq_write(overlay_id,3,PhysicalAddr(velocity->u),PhysicalAddr(velocity->v),PhysicalAddr(pressure));
    prepare_wait();
    data_cache_hit_invalidate(velocity,sizeof(*velocity));
}

void fluid_speed_pixels_rsp(const Fluid *f,uint32_t *pixels,unsigned stride) {
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
    rspq_write(overlay_id,4,PhysicalAddr(v),PhysicalAddr(pixels),stride*sizeof(*pixels),PhysicalAddr(palette));
    prepare_wait();
    data_cache_hit_invalidate(CachedAddr(pixels),stride*FH*sizeof(*pixels));
}
