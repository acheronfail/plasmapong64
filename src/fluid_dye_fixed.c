#include "fluid_dye_fixed.h"
#ifdef PLASMAPONG_VELOCITY_FIXED
#include "fluid_velocity_fixed.h"
#endif
#include <assert.h>
#include <float.h>
#include <string.h>
_Static_assert(sizeof(float)==sizeof(uint32_t) && FLT_RADIX==2 && FLT_MANT_DIG==24 && FLT_MAX_EXP==128,
        "trace coordinates use IEEE binary32");
_Static_assert(sizeof(FluidDyeTrace)==48,"RSP trace stride");
_Static_assert((-1>>1)==-1,"fixed-point reference requires arithmetic shifts");
static inline float trace_coord(float value,float upper) {
    uint32_t bits,limit;
    memcpy(&bits,&value,sizeof(bits)); memcpy(&limit,&upper,sizeof(limit));
    if(bits>limit) return (bits>>31)?0:upper;
    return value;
}

void fluid_dye_trace(FluidDyeTrace *restrict trace,const FluidVelocity *restrict velocity,float grid_dt) {
    float fy=0;
    for(int y=0;y<FH;y++,fy+=1) {
        float fx=0;
        for(int x=0;x<FW;x++,fx+=1) {
            int k=y*FW+x;
            float px=trace_coord(fx-grid_dt*velocity->u[k],FW-1.001f);
            float py=trace_coord(fy-grid_dt*velocity->v[k],FH-1.001f);
            int ix=(int)px,iy=(int)py;
            trace[k/8].offset[k%8]=(iy*FW+ix)*2;
            trace[k/8].tx[k%8]=(uint16_t)((px-ix)*DYE_WEIGHT_SCALE);
            trace[k/8].ty[k%8]=(uint16_t)((py-iy)*DYE_WEIGHT_SCALE);
        }
    }
}
void fluid_dye_pack(FluidDyeFixed *restrict packed,FluidDyeTrace *restrict trace,
        const FluidDye *restrict ink,const FluidVelocity *restrict velocity,float grid_dt) {
    fluid_dye_trace(trace,velocity,grid_dt);
    for(int k=0;k<FN;k++) {
        packed->red[k]=(int16_t)(ink->red[k]*DYE_SCALE+.5f);
        packed->blue[k]=(int16_t)(ink->blue[k]*DYE_SCALE+.5f);
        packed->gold[k]=(int16_t)(ink->gold[k]*DYE_SCALE+.5f);
    }
}
void fluid_dye_unpack(FluidDye *restrict ink,const FluidDyeFixed *restrict packed) {
    for(int k=0;k<FN;k++) {
        ink->red[k]=packed->red[k]*(1.0f/DYE_SCALE);
        ink->blue[k]=packed->blue[k]*(1.0f/DYE_SCALE);
        ink->gold[k]=packed->gold[k]*(1.0f/DYE_SCALE);
    }
}
unsigned fluid_dye_decay(float decay) {
    assert(decay>=0 && decay<=1);
    return (unsigned)(decay*DYE_WEIGHT_SCALE+.5f);
}
static int lerp(int a,int b,unsigned weight) {
    return a+(((b-a)*(int)weight+16384)>>15);
}
static void channel(int16_t *restrict next,const int16_t *restrict ink,
        const FluidDyeTrace *restrict trace,unsigned decay,unsigned rounding) {
    for(int k=0;k<FN;k++) {
        int j=trace[k/8].offset[k%8]/2;
        int top=lerp(ink[j],ink[j+1],trace[k/8].tx[k%8]);
        int bottom=lerp(ink[j+FW],ink[j+FW+1],trace[k/8].tx[k%8]);
        int value=lerp(top,bottom,trace[k/8].ty[k%8]);
        /* A rotating rounding threshold avoids both permanent faint residues
           and the downward bias of truncating every frame. Product fits int32. */
        next[k]=(int16_t)((value*(int)decay*2+(int)rounding)>>16);
    }
}
void fluid_dye_fixed_reference(FluidDyeFixed *restrict next,const FluidDyeFixed *restrict ink,
        const FluidDyeTrace *restrict trace,unsigned decay,unsigned gold_decay,unsigned rounding) {
    assert(decay<=DYE_WEIGHT_SCALE && gold_decay<=DYE_WEIGHT_SCALE && rounding<=65535);
    channel(next->red,ink->red,trace,decay,rounding);
    channel(next->blue,ink->blue,trace,decay,rounding);
    channel(next->gold,ink->gold,trace,gold_decay,rounding);
}
void fluid_advect_ink_reference(FluidDyeFixed *next,const FluidDyeFixed *ink,
        const FluidFlow *velocity,float grid_dt,float decay,float gold_decay,unsigned rounding) {
    static _Alignas(16) FluidDyeTrace trace[FLUID_TRACE_BATCHES];
#ifdef PLASMAPONG_VELOCITY_FIXED
    fluid_velocity_trace(trace,velocity,grid_dt);
#else
    fluid_dye_trace(trace,velocity,grid_dt);
#endif
    fluid_dye_fixed_reference(next,ink,trace,fluid_dye_decay(decay),fluid_dye_decay(gold_decay),rounding);
}
