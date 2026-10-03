#include <libdragon.h>
#include <assert.h>
#include "fluid_dye_fixed.h"
#ifdef PLASMAPONG_VELOCITY_FIXED
#include "fluid_velocity_fixed.h"
#endif
DEFINE_RSP_UCODE(rsp_dye);
static uint32_t overlay_id;
_Static_assert(FW==48 && FH==33 && FN%24==0,"RSP dye grid/chunk layout");
_Static_assert(DYE_SCALE==8192 && DYE_WEIGHT_SCALE==32768,"RSP dye Q13/Q15 layout");
_Static_assert(sizeof(FluidDyeFixed)%16==0 && FN*sizeof(FluidDyeTrace)%16==0,
        "DMA buffers must own complete cache lines");

void fluid_channels_rsp(int16_t *next,const int16_t *source,const FluidDyeTrace *trace,
        unsigned channels,const unsigned *decays,unsigned rounding) {
    assert(((uintptr_t)next&15)==0 && ((uintptr_t)source&15)==0 && ((uintptr_t)trace&15)==0);
    assert(channels>=1 && channels<=3 && rounding<=65535);
    if(!overlay_id) {
        rspq_init();
        overlay_id=rspq_overlay_register(&rsp_dye);
    }
    unsigned bytes=channels*FN*sizeof(int16_t);
    data_cache_hit_writeback(source,bytes);
    data_cache_hit_writeback(trace,FN*sizeof(*trace));
    data_cache_hit_invalidate(next,bytes);
    for(unsigned c=0;c<channels;c++) {
        assert(decays[c]<=DYE_WEIGHT_SCALE);
        rspq_write(overlay_id,0,PhysicalAddr((const char *)source+c*FN*sizeof(int16_t)),PhysicalAddr(trace),
            PhysicalAddr((char *)next+c*FN*sizeof(int16_t)),decays[c]|(rounding<<16));
    }
    rspq_syncpoint_t done=rspq_syncpoint_new();
    rspq_flush();
    rspq_syncpoint_wait(done);
    data_cache_hit_invalidate(next,bytes);
}
void fluid_dye_fixed_rsp(FluidDyeFixed *next,const FluidDyeFixed *ink,
        const FluidDyeTrace *trace,unsigned decay,unsigned gold_decay,unsigned rounding) {
    unsigned decays[3]={decay,decay,gold_decay};
    fluid_channels_rsp(next->red,ink->red,trace,3,decays,rounding);
}
void fluid_advect_ink_rsp(FluidDyeFixed *next,const FluidDyeFixed *ink,
        const FluidFlow *velocity,float grid_dt,float decay,float gold_decay,unsigned rounding) {
    static _Alignas(16) FluidDyeTrace trace[FN];
#ifdef PLASMAPONG_VELOCITY_FIXED
#ifdef PLASMAPONG_PREPARE_RSP
    fluid_velocity_trace_rsp(trace,velocity,grid_dt);
#else
    fluid_velocity_trace(trace,velocity,grid_dt);
#endif
#else
    fluid_dye_trace(trace,velocity,grid_dt);
#endif
    fluid_dye_fixed_rsp(next,ink,trace,fluid_dye_decay(decay),fluid_dye_decay(gold_decay),rounding);
}
