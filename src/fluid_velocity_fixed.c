#include "fluid_velocity_fixed.h"
#include <assert.h>
_Static_assert((-1>>1)==-1,"fixed-point trace requires arithmetic shifts");
_Static_assert(FW%8==0 && FH>=5,"complete Q4 trace vectors");
static int quantize(float v) {
    v*=VELOCITY_SCALE;
    if(v>VELOCITY_LIMIT) return VELOCITY_LIMIT;
    if(v<-VELOCITY_LIMIT) return -VELOCITY_LIMIT;
    return (int)(v+(v<0?-.5f:.5f));
}
void fluid_velocity_pack(FluidVelocityFixed *packed,FluidDyeTrace *trace,
        const FluidVelocity *velocity,float grid_dt) {
    for(int k=0;k<FN;k++) {
        packed->u[k]=(int16_t)quantize(velocity->u[k]);
        packed->v[k]=(int16_t)quantize(velocity->v[k]);
    }
    fluid_velocity_trace(trace,packed,grid_dt);
}
void fluid_velocity_trace(FluidDyeTrace *trace,const FluidVelocityFixed *velocity,float grid_dt) {
    /* Q12 coordinates, Q20 timestep. No per-cell float backtrace or fractional
       conversions. The coefficient is shared by the whole grid. */
    assert(grid_dt>=0 && grid_dt<=.125f);
    int step=(int)(grid_dt*1048576+.5f);
    for(int y=0;y<FH;y++) for(int x=0;x<FW;x++) {
        int k=y*FW+x;
        int u=velocity->u[k],v=velocity->v[k];
        int px=x*4096-((u*step+2048)>>12),py=y*4096-((v*step+2048)>>12);
        if(px<0) px=0; else if(px>(FW-1)*4096-5) px=(FW-1)*4096-5;
        if(py<0) py=0; else if(py>(FH-1)*4096-5) py=(FH-1)*4096-5;
        trace[k/8].offset[k%8]=((py>>12)*FW+(px>>12))*2;
        trace[k/8].tx[k%8]=(px&4095)*8; trace[k/8].ty[k%8]=(py&4095)*8;
    }
}
void fluid_velocity_unpack(FluidVelocity *next,const FluidVelocityFixed *packed) {
    for(int k=0;k<FN;k++) {
        next->u[k]=packed->u[k]*(1.0f/VELOCITY_SCALE);
        next->v[k]=packed->v[k]*(1.0f/VELOCITY_SCALE);
    }
}
static int lerp(int a,int b,unsigned weight) { return a+(((b-a)*(int)weight+16384)>>15); }
void fluid_velocity_fixed_reference(FluidVelocityFixed *next,const FluidVelocityFixed *packed,
        const FluidDyeTrace *trace,unsigned decay,unsigned rounding) {
    assert(decay<=32768 && rounding<=65535);
    for(int c=0;c<2;c++) for(int k=0;k<FN;k++) {
        int j=trace[k/8].offset[k%8]/2;
        const int16_t *p=c?packed->v:packed->u;
        int16_t *out=c?next->v:next->u;
        int top=lerp(p[j],p[j+1],trace[k/8].tx[k%8]),bottom=lerp(p[j+FW],p[j+FW+1],trace[k/8].tx[k%8]);
        int value=lerp(top,bottom,trace[k/8].ty[k%8]);
        out[k]=(int16_t)((value*(int)decay*2+(int)rounding)>>16);
    }
}
#ifndef PLASMAPONG_UPWIND_RSP
void fluid_advect_velocity_fixed(FluidVelocityFixed *next,const FluidVelocityFixed *velocity,
        float grid_dt,float decay,unsigned rounding) {
    static _Alignas(16) FluidDyeTrace trace[FLUID_TRACE_BATCHES];
#ifdef PLASMAPONG_PREPARE_RSP
#ifdef PLASMAPONG_ADVECTION_CHAIN
    fluid_velocity_trace_rsp_begin(trace,velocity,grid_dt);
#else
    fluid_velocity_trace_rsp(trace,velocity,grid_dt);
#endif
#else
    fluid_velocity_trace(trace,velocity,grid_dt);
#endif
    unsigned d=fluid_dye_decay(decay);
#ifdef PLASMAPONG_VELOCITY_RSP
    unsigned decays[2]={d,d};
    fluid_channels_rsp(next->u,velocity->u,trace,2,decays,rounding);
#else
    fluid_velocity_fixed_reference(next,velocity,trace,d,rounding);
#endif
}
#endif
