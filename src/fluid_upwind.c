#include "fluid_upwind.h"
#include "fluid_dye_fixed.h"
#include <assert.h>
#ifdef PLASMAPONG_UPWIND_GPU_LIMIT
#include "rsp_reciprocal_table.inc"
static uint32_t reciprocal(unsigned n) {
    if(!n) return 0x7fffffffu;
    unsigned shift=(unsigned)__builtin_clz(n);
    unsigned index=((n<<shift)&0x7fc00000u)>>22;
    return ((0x10000u|rsp_reciprocal_table[index])<<14)>>(31-shift);
}
static int limited_component(int value,unsigned inverse,unsigned safe_limit) {
    unsigned magnitude=(unsigned)(value<0?-value:value);
    unsigned direction=(unsigned)(((uint64_t)magnitude*inverse)>>16);
    if(direction>32767) direction=32767;
    int result=(int)((direction*safe_limit)>>15);
    return value<0?-result:result;
}
#endif
unsigned fluid_upwind_step(float grid_dt) {
    assert(grid_dt>=0 && grid_dt<=.125f);
    return (unsigned)(grid_dt*1048576+.5f);
}
unsigned fluid_upwind_limit(unsigned step) {
    unsigned limit=step?(16777216u-512u)/step:2*VELOCITY_LIMIT;
    return limit>2*VELOCITY_LIMIT?2*VELOCITY_LIMIT:limit;
}
unsigned fluid_upwind_safe_limit(unsigned limit) {
    /* More than 1/256 relative headroom covers the reciprocal mantissa bin
       error (<1/512); floor each positive component before restoring sign. */
    return limit>4?limit-(limit>>8)-4:0;
}
void fluid_upwind_clamp(FluidVelocityFixed *velocity,unsigned limit) {
    for(int k=0;k<FN;k++) {
        int u=velocity->u[k],v=velocity->v[k];
        int magnitude=(u<0?-u:u)+(v<0?-v:v);
        if(magnitude>(int)limit) {
#ifdef PLASMAPONG_UPWIND_GPU_LIMIT
            if(!v) velocity->u[k]=(int16_t)(u<0?-(int)limit:(int)limit);
            else if(!u) velocity->v[k]=(int16_t)(v<0?-(int)limit:(int)limit);
            else {
                unsigned inverse=reciprocal((unsigned)magnitude),safe=fluid_upwind_safe_limit(limit);
                velocity->u[k]=(int16_t)limited_component(u,inverse,safe);
                velocity->v[k]=(int16_t)limited_component(v,inverse,safe);
            }
#else
            velocity->u[k]=(int16_t)(u*(int)limit/magnitude);
            velocity->v[k]=(int16_t)(v*(int)limit/magnitude);
#endif
        }
    }
}
static int absolute(int value) { return value<0?-value:value; }
static void channel(int16_t *next,const int16_t *source,const FluidVelocityFixed *velocity,
        unsigned step,unsigned decay,unsigned rounding) {
    for(int y=0;y<FH;y++) for(int x=0;x<FW;x++) {
        int k=y*FW+x,u=velocity->u[k],v=velocity->v[k];
        int sx=u>=0?(x? k-1:k):(x<FW-1?k+1:k);
        int sy=v>=0?(y?k-FW:k):(y<FH-1?k+FW:k);
        int wx=(absolute(u)*(int)step+256)>>9,wy=(absolute(v)*(int)step+256)>>9;
        assert(wx+wy<=32768);
        int value=source[k]+(((source[sx]-source[k])*wx+(source[sy]-source[k])*wy+16384)>>15);
        next[k]=(int16_t)((value*(int)decay*2+(int)rounding)>>16);
    }
}
void fluid_upwind_velocity_cpu(FluidVelocityFixed *next,FluidVelocityFixed *velocity,
        float grid_dt,float decay,unsigned rounding) {
    unsigned step=fluid_upwind_step(grid_dt);
    fluid_upwind_clamp(velocity,fluid_upwind_limit(step));
    unsigned d=fluid_dye_decay(decay);
    channel(next->u,velocity->u,velocity,step,d,rounding);
    channel(next->v,velocity->v,velocity,step,d,rounding);
}
void fluid_upwind_ink_cpu(FluidDyeFixed *next,const FluidDyeFixed *ink,FluidVelocityFixed *velocity,
        float grid_dt,float decay,float gold_decay,unsigned rounding) {
    unsigned step=fluid_upwind_step(grid_dt);
    fluid_upwind_clamp(velocity,fluid_upwind_limit(step));
    unsigned d=fluid_dye_decay(decay),g=fluid_dye_decay(gold_decay);
    channel(next->red,ink->red,velocity,step,d,rounding);
    channel(next->blue,ink->blue,velocity,step,d,rounding);
    channel(next->gold,ink->gold,velocity,step,g,rounding);
}
