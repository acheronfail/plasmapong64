#include <libdragon.h>
#include <assert.h>
#include "fluid_pressure.h"

DEFINE_RSP_UCODE(rsp_fluid);
static uint32_t overlay_id;
_Static_assert(FW==48 && FH==33,"RSP pressure row layout must match the grid");
_Static_assert(FN*sizeof(int32_t)%16==0,"DMA buffers must cover whole cache lines");

void fluid_pressure_rsp(int32_t *pressure,const int32_t *divergence) {
    assert(((uintptr_t)pressure&15)==0 && ((uintptr_t)divergence&15)==0);
    if(!overlay_id) {
        rspq_init();
        overlay_id=rspq_overlay_register(&rsp_fluid);
    }
    /* RSP reads divergence and overwrites every pressure word. Invalidate
       before DMA so eviction cannot write old dirty pressure over RSP output.
       These buffers own complete cache lines and cannot overlap. */
    data_cache_hit_writeback(divergence,FN*sizeof(*divergence));
    data_cache_hit_invalidate(pressure,FN*sizeof(*pressure));
    /* All eight dependency-preserving passes stay inside one DMEM wavefront. */
    rspq_write(overlay_id,0,PhysicalAddr(pressure),PhysicalAddr(divergence),0);
    rspq_syncpoint_t done=rspq_syncpoint_new();
    rspq_flush();
    rspq_syncpoint_wait(done);
    data_cache_hit_invalidate(pressure,FN*sizeof(*pressure));
}
