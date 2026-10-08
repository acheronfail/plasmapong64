#include <libdragon.h>
#include <assert.h>
#include "fluid_velocity_fixed.h"
#include "fluid_pressure.h"
#include "fluid_queue.h"
DEFINE_RSP_UCODE(rsp_gradient_short);
static uint32_t overlay;
#ifdef PLASMAPONG_AUDIO_STREAM
extern void audio_background(void);
#endif
void fluid_gradient_short_rsp_init(void) {
    if(!overlay) { rspq_init(); overlay=rspq_overlay_register(&rsp_gradient_short); }
}
void fluid_gradient_short_rsp(FluidVelocityFixed *velocity,const int16_t *pressure) {
    assert(!((uintptr_t)velocity&15) && !((uintptr_t)pressure&15));
    fluid_gradient_short_rsp_init();
    data_cache_hit_writeback(pressure,FN*sizeof(*pressure));
    data_cache_hit_writeback_invalidate(velocity,sizeof(*velocity));
    fluid_queue_begin();
    rspq_write(overlay,0,PhysicalAddr(velocity->u),PhysicalAddr(velocity->v),PhysicalAddr(pressure));
#ifdef PLASMAPONG_AUDIO_STREAM
    rspq_flush(); audio_background();
#endif
    fluid_queue_wait();
    data_cache_hit_invalidate(velocity,sizeof(*velocity));
}
void fluid_projection_short_rsp(FluidVelocityFixed *velocity,int32_t *divergence,
        int32_t *pressure,int16_t *short_pressure) {
    fluid_divergence_rsp_begin(divergence,velocity);
    fluid_pressure_short_rsp_begin(pressure,divergence,short_pressure);
    fluid_gradient_short_rsp(velocity,short_pressure);
    data_cache_hit_invalidate(divergence+FW,(FH-2)*FW*sizeof(*divergence));
    data_cache_hit_invalidate(pressure,FN*sizeof(*pressure));
    data_cache_hit_invalidate(short_pressure,FN*sizeof(*short_pressure));
}

#ifdef PLASMAPONG_VELOCITY_CHAIN
#include "fluid_upwind.h"
#include "fluid_confinement.h"
void fluid_velocity_chain_rsp(Fluid *f,float dt) {
    /* Register every overlay before opening the producer/consumer batch. */
    fluid_upwind_rsp_init();
    if(FLUID_CONFINEMENT>0) fluid_confinement_rsp_init();
    fluid_prepare_rsp_init(); fluid_pressure_rsp_init(); fluid_gradient_short_rsp_init();
    FluidVelocityFixed *old=fluid_velocity(f),*next=&f->velocity[f->velocity_bank^1];
    fluid_upwind_velocity_rsp_begin(next,old,dt/CELL,1-FLUID_DAMPING*dt,
        (f->velocity_phase+=40503u)&65535u);
    if(FLUID_CONFINEMENT>0)
        fluid_curl_confinement_window_rsp_begin(next,f->curl_fixed,
            fluid_confinement_strength(dt)*FLUID_CONFINEMENT_BANDS,
            fluid_confinement_first(f->velocity_phase),fluid_confinement_rows(f->velocity_phase));
    fluid_divergence_walled_rsp_begin(f->divergence,next);
    fluid_pressure_short_rsp_begin(f->pressure,f->divergence,f->pressure_short);
    /* All intermediate lines were invalidated before their producer. No CPU
       access, cache writeback, or queue wait is needed between these jobs. */
    rspq_write(overlay,0,PhysicalAddr(next->u),PhysicalAddr(next->v),PhysicalAddr(f->pressure_short));
#ifdef PLASMAPONG_AUDIO_STREAM
    rspq_flush(); audio_background();
#endif
    fluid_queue_wait();
    if(FLUID_CONFINEMENT>0) data_cache_hit_invalidate(f->curl_fixed,sizeof(f->curl_fixed));
    data_cache_hit_invalidate(old,sizeof(*old));
    data_cache_hit_invalidate(next,sizeof(*next));
    data_cache_hit_invalidate(f->divergence,sizeof(f->divergence));
    data_cache_hit_invalidate(f->pressure,sizeof(f->pressure));
    data_cache_hit_invalidate(f->pressure_short,sizeof(f->pressure_short));
    f->velocity_bank^=1;
}
#endif
