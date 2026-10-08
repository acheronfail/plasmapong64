#include <libdragon.h>
#include <assert.h>
#include "fluid_pressure.h"
#include "fluid_queue.h"

DEFINE_RSP_UCODE(rsp_fluid);
static uint32_t overlay_id;
void fluid_pressure_rsp_init(void) {
    if(!overlay_id) { rspq_init(); overlay_id=rspq_overlay_register(&rsp_fluid); }
}
_Static_assert((FW==48 || (FW==64 && PLASMAPONG_PRESSURE_Q3)) && FH>=5,"RSP pressure row layout must match the grid");
_Static_assert(FN*sizeof(int32_t)%16==0,"DMA buffers must cover whole cache lines");

static void pressure_begin(int32_t *pressure,const int32_t *divergence,int16_t *short_pressure) {
    assert(((uintptr_t)pressure&15)==0 && ((uintptr_t)divergence&15)==0);
    fluid_pressure_rsp_init();
    /* RSP reads divergence and overwrites every pressure word. Invalidate
       before DMA so eviction cannot write old dirty pressure over RSP output.
       These buffers own complete cache lines and cannot overlap. */
    data_cache_hit_writeback(divergence,FN*sizeof(*divergence));
#if PLASMAPONG_PRESSURE_WARM_START
    data_cache_hit_writeback_invalidate(pressure,FN*sizeof(*pressure));
#else
    data_cache_hit_invalidate(pressure,FN*sizeof(*pressure));
#endif
    if(short_pressure) {
        assert(!((uintptr_t)short_pressure&15));
        data_cache_hit_invalidate(short_pressure,FN*sizeof(*short_pressure));
    }
    /* All eight dependency-preserving passes stay inside one DMEM wavefront. */
    fluid_queue_begin();
    rspq_write(overlay_id,0,PhysicalAddr(pressure),PhysicalAddr(divergence),short_pressure?PhysicalAddr(short_pressure):0);
}
void fluid_pressure_rsp_begin(int32_t *pressure,const int32_t *divergence) {
    pressure_begin(pressure,divergence,NULL);
}
#if PLASMAPONG_PRESSURE_FAST_GRADIENT
void fluid_pressure_short_rsp_begin(int32_t *pressure,const int32_t *divergence,int16_t *short_pressure) {
    pressure_begin(pressure,divergence,short_pressure);
}
#endif
void fluid_pressure_rsp(int32_t *pressure,const int32_t *divergence) {
    fluid_pressure_rsp_begin(pressure,divergence);
    fluid_queue_wait();
    data_cache_hit_invalidate(pressure,FN*sizeof(*pressure));
}
