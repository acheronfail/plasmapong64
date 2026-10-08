#include <libdragon.h>
#include <assert.h>
#include "fluid_upwind.h"
#include "fluid_dye_fixed.h"
#include "fluid_queue.h"
#include "fluid_profile.h"
DEFINE_RSP_UCODE(rsp_upwind);
static uint32_t overlay;
void fluid_upwind_rsp_init(void) {
    if(!overlay) { rspq_init(); overlay=rspq_overlay_register(&rsp_upwind); }
}
/* Keep the backend initialization contract when the streaming overlay replaces
   the complete-grid dye/velocity overlay. */
void fluid_dye_rsp_init(void) { fluid_upwind_rsp_init(); }
typedef struct { uint32_t step,unused,channels,rounding,decay[3],pad; } UpwindParams;
static void channels(int16_t *next,const int16_t *source,FluidVelocityFixed *velocity,
        unsigned count,float grid_dt,const unsigned *decay,unsigned rounding,bool wait) {
    static _Alignas(16) UpwindParams params;
    assert(!((uintptr_t)next&15) && !((uintptr_t)source&15) && !((uintptr_t)velocity&15));
    assert(count>=1 && count<=3 && rounding<=65535);
    fluid_upwind_rsp_init();
    PROFILE_BEGIN();
    unsigned step=fluid_upwind_step(grid_dt);
#ifndef PLASMAPONG_UPWIND_GPU_LIMIT
    fluid_upwind_clamp(velocity,fluid_upwind_limit(step));
#endif
    PROFILE_END(count==3?PROFILE_UPWIND_LIMIT_DYE:PROFILE_UPWIND_LIMIT_VELOCITY);
    params=(UpwindParams){.step=step,.channels=count,.rounding=rounding};
#ifdef PLASMAPONG_UPWIND_INLINE_LIMIT
    unsigned limit=fluid_upwind_limit(step);
    params.unused=limit|(fluid_upwind_safe_limit(limit)<<16);
#endif
    for(unsigned i=0;i<count;i++) { assert(decay[i]<=32768); params.decay[i]=decay[i]; }
    data_cache_hit_writeback(&params,sizeof(params));
#ifdef PLASMAPONG_UPWIND_GPU_LIMIT
    data_cache_hit_writeback_invalidate(velocity,sizeof(*velocity));
#else
    data_cache_hit_writeback(velocity,sizeof(*velocity));
#endif
    data_cache_hit_writeback(source,count*FN*sizeof(*source));
    data_cache_hit_invalidate(next,count*FN*sizeof(*next));
    fluid_queue_begin();
#ifdef PLASMAPONG_UPWIND_GPU_LIMIT
#ifndef PLASMAPONG_UPWIND_INLINE_LIMIT
    unsigned limit=fluid_upwind_limit(step);
    if(limit<2*VELOCITY_LIMIT)
        rspq_write(overlay,1,PhysicalAddr(velocity->u),PhysicalAddr(velocity->v),limit,fluid_upwind_safe_limit(limit));
#endif
#endif
    rspq_write(overlay,0,PhysicalAddr(source),PhysicalAddr(velocity),PhysicalAddr(next),PhysicalAddr(&params));
    if(!wait) return;
    fluid_queue_wait();
    data_cache_hit_invalidate(next,count*FN*sizeof(*next));
#ifdef PLASMAPONG_UPWIND_GPU_LIMIT
    data_cache_hit_invalidate(velocity,sizeof(*velocity));
#endif
    PROFILE_END(count==3?PROFILE_UPWIND_JOB_DYE:PROFILE_UPWIND_JOB_VELOCITY);
}
void fluid_upwind_velocity_rsp(FluidVelocityFixed *next,FluidVelocityFixed *velocity,
        float grid_dt,float decay,unsigned rounding) {
    unsigned d=fluid_dye_decay(decay),decays[2]={d,d};
    channels(next->u,velocity->u,velocity,2,grid_dt,decays,rounding,true);
}
void fluid_upwind_ink_rsp(FluidDyeFixed *next,const FluidDyeFixed *ink,FluidVelocityFixed *velocity,
        float grid_dt,float decay,float gold_decay,unsigned rounding) {
    unsigned d=fluid_dye_decay(decay),decays[3]={d,d,fluid_dye_decay(gold_decay)};
    channels(next->red,ink->red,velocity,3,grid_dt,decays,rounding,true);
}

void fluid_upwind_velocity_rsp_begin(FluidVelocityFixed *next,FluidVelocityFixed *velocity,
        float grid_dt,float decay,unsigned rounding) {
    unsigned d=fluid_dye_decay(decay),decays[2]={d,d};
    channels(next->u,velocity->u,velocity,2,grid_dt,decays,rounding,false);
}
