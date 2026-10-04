#include <libdragon.h>
#include <assert.h>
#include <string.h>
#include "fluid_confinement.h"
#include "fluid_queue.h"
DEFINE_RSP_UCODE(rsp_confinement);
static uint32_t overlay;
static void init(void) {
    if(!overlay) { rspq_init(); overlay=rspq_overlay_register(&rsp_confinement); }
}
void fluid_confinement_rsp_init(void) { init(); }
static void finish(void) {
    fluid_queue_wait();
}
void fluid_curl_rsp(int16_t *curl,const FluidVelocityFixed *v) {
    assert(!((uintptr_t)curl&15) && !((uintptr_t)v&15));
    init();
    /* Only interior rows are written by the command; define the two borders
       before invalidating complete, independently owned cache lines. */
    memset(curl,0,FW*sizeof(*curl)); memset(curl+(FH-1)*FW,0,FW*sizeof(*curl));
    data_cache_hit_writeback(curl,FW*sizeof(*curl));
    data_cache_hit_writeback(curl+(FH-1)*FW,FW*sizeof(*curl));
    data_cache_hit_invalidate(curl,FN*sizeof(*curl));
    data_cache_hit_writeback(v,sizeof(*v));
    fluid_queue_begin();
    rspq_write(overlay,0,PhysicalAddr(v->u),PhysicalAddr(v->v),PhysicalAddr(curl));
    finish(); data_cache_hit_invalidate(curl,FN*sizeof(*curl));
}
void fluid_confinement_rsp(FluidVelocityFixed *v,const int16_t *curl,unsigned strength) {
    assert(!((uintptr_t)curl&15) && !((uintptr_t)v&15) && strength<=fluid_confinement_strength(.25f));
    init();
    data_cache_hit_writeback(curl,FN*sizeof(*curl));
    data_cache_hit_writeback_invalidate(v,sizeof(*v));
    fluid_queue_begin();
    rspq_write(overlay,1,PhysicalAddr(v->u),PhysicalAddr(v->v),PhysicalAddr(curl),strength);
    finish(); data_cache_hit_invalidate(v,sizeof(*v));
}
void fluid_curl_confinement_rsp(FluidVelocityFixed *v,int16_t *curl,unsigned strength) {
    assert(!((uintptr_t)curl&15) && !((uintptr_t)v&15) && strength<=fluid_confinement_strength(.25f));
    init();
    memset(curl,0,FW*sizeof(*curl)); memset(curl+(FH-1)*FW,0,FW*sizeof(*curl));
    data_cache_hit_writeback(curl,FW*sizeof(*curl));
    data_cache_hit_writeback(curl+(FH-1)*FW,FW*sizeof(*curl));
    data_cache_hit_invalidate(curl,FN*sizeof(*curl));
    /* Both commands share the queue. Curl's DMA output goes straight to the
       next command; no CPU can dirty it between producer and consumer. */
    data_cache_hit_writeback_invalidate(v,sizeof(*v));
    fluid_queue_begin();
    rspq_write(overlay,0,PhysicalAddr(v->u),PhysicalAddr(v->v),PhysicalAddr(curl));
    rspq_write(overlay,1,PhysicalAddr(v->u),PhysicalAddr(v->v),PhysicalAddr(curl),strength);
    finish();
    data_cache_hit_invalidate(curl,FN*sizeof(*curl));
    data_cache_hit_invalidate(v,sizeof(*v));
}
