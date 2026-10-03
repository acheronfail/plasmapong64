#include <libdragon.h>
#include <assert.h>
#include "fluid_velocity_fixed.h"
DEFINE_RSP_UCODE(rsp_prepare);
static uint32_t overlay_id;
_Static_assert(FW==48 && FH==33 && sizeof(FluidDyeTrace)==6,"RSP preparation grid/trace layout");
_Static_assert(DYE_SCALE==8192 && VELOCITY_LIMIT==16383,"RSP preparation lane ranges");
_Static_assert(sizeof(FluidVelocityFixed)%16==0 && sizeof(FluidDyeFixed)%16==0 &&
        FN*sizeof(FluidDyeTrace)%16==0,"DMA buffers must own complete cache lines");
static void prepare_init(void) {
    if(!overlay_id) { rspq_init(); overlay_id=rspq_overlay_register(&rsp_prepare); }
}
static void prepare_wait(void) {
    rspq_syncpoint_t done=rspq_syncpoint_new();
    rspq_flush(); rspq_syncpoint_wait(done);
}
void fluid_velocity_trace_rsp(FluidDyeTrace *trace,const FluidVelocityFixed *velocity,float grid_dt) {
    assert(((uintptr_t)trace&15)==0 && ((uintptr_t)velocity&15)==0);
    assert(grid_dt>=0 && grid_dt<=.125f);
    prepare_init();
    data_cache_hit_writeback(velocity,sizeof(*velocity));
    data_cache_hit_invalidate(trace,FN*sizeof(*trace));
    unsigned step=(unsigned)(grid_dt*1048576+.5f);
    rspq_write(overlay_id,0,PhysicalAddr(velocity->u),PhysicalAddr(velocity->v),PhysicalAddr(trace),step);
    prepare_wait();
    data_cache_hit_invalidate(trace,FN*sizeof(*trace));
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
