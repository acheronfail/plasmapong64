#include <libdragon.h>
#include <assert.h>
#include "fluid_dye_fixed.h"
DEFINE_RSP_UCODE(rsp_dye);
static uint32_t overlay_id;
_Static_assert(FW==48 && FH==33 && FN%24==0,"RSP dye grid/chunk layout");
_Static_assert(DYE_SCALE==8192 && DYE_WEIGHT_SCALE==32768,"RSP dye Q13/Q15 layout");
_Static_assert(sizeof(FluidDyeFixed)%16==0 && FN*sizeof(FluidDyeTrace)%16==0,
        "DMA buffers must own complete cache lines");

void fluid_dye_fixed_rsp(FluidDyeFixed *next,const FluidDyeFixed *ink,
        const FluidDyeTrace *trace,unsigned decay,unsigned gold_decay,unsigned rounding) {
    assert(((uintptr_t)next&15)==0 && ((uintptr_t)ink&15)==0 && ((uintptr_t)trace&15)==0);
    assert(decay<=DYE_WEIGHT_SCALE && gold_decay<=DYE_WEIGHT_SCALE);
    assert(rounding<=65535);
    if(!overlay_id) {
        rspq_init();
        overlay_id=rspq_overlay_register(&rsp_dye);
    }
    data_cache_hit_writeback(ink,sizeof(*ink));
    data_cache_hit_writeback(trace,FN*sizeof(*trace));
    data_cache_hit_invalidate(next,sizeof(*next));
    rspq_write(overlay_id,0,PhysicalAddr(ink->red),PhysicalAddr(trace),PhysicalAddr(next->red),decay|(rounding<<16));
    rspq_write(overlay_id,0,PhysicalAddr(ink->blue),PhysicalAddr(trace),PhysicalAddr(next->blue),decay|(rounding<<16));
    rspq_write(overlay_id,0,PhysicalAddr(ink->gold),PhysicalAddr(trace),PhysicalAddr(next->gold),gold_decay|(rounding<<16));
    rspq_syncpoint_t done=rspq_syncpoint_new();
    rspq_flush();
    rspq_syncpoint_wait(done);
    data_cache_hit_invalidate(next,sizeof(*next));
}
void fluid_advect_ink_rsp(FluidDyeFixed *next,const FluidDyeFixed *ink,
        const FluidVelocity *velocity,float grid_dt,float decay,float gold_decay,unsigned rounding) {
    static _Alignas(16) FluidDyeTrace trace[FN];
    fluid_dye_trace(trace,velocity,grid_dt);
    fluid_dye_fixed_rsp(next,ink,trace,fluid_dye_decay(decay),fluid_dye_decay(gold_decay),rounding);
}
