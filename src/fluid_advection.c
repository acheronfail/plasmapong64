#include "fluid_advection.h"
#include <float.h>
#include <string.h>
_Static_assert(sizeof(float)==sizeof(uint32_t) && FLT_RADIX==2 && FLT_MANT_DIG==24 && FLT_MAX_EXP==128,
        "advection coordinates use IEEE binary32");

static inline float advection_coord(float value,float upper) {
    /* For finite IEEE floats, positive bit patterns have numeric ordering.
       An unsigned comparison catches either boundary with one common-path
       branch, avoiding two serial FPU compares on the R4300. */
    uint32_t bits,limit;
    memcpy(&bits,&value,sizeof(bits));
    memcpy(&limit,&upper,sizeof(limit));
    if(bits>limit) return (bits>>31)?0:upper;
    return value;
}
/* Ping-pong banks never overlap. Express that at the kernel boundary so stores
   cannot alias the source samples and serialize independent channels. Float
   loop coordinates avoid per-cell integer conversions; these small integer
   coordinates and increments are exactly representable in binary32. */
void fluid_advect_velocity(FluidVelocity *restrict next,
        const FluidVelocity *restrict velocity,float grid_dt,float decay) {
    float fy=0;
    for(int y=0;y<FH;y++,fy+=1) {
        float fx=0;
        for(int x=0;x<FW;x++,fx+=1) {
            int k=y*FW+x;
            float px=advection_coord(fx-grid_dt*velocity->u[k],FW-1.001f);
            float py=advection_coord(fy-grid_dt*velocity->v[k],FH-1.001f);
            int ix=(int)px,iy=(int)py,j=iy*FW+ix;
            float tx=px-ix,ty=py-iy;
            float ut=velocity->u[j]+tx*(velocity->u[j+1]-velocity->u[j]);
            float ub=velocity->u[j+FW]+tx*(velocity->u[j+FW+1]-velocity->u[j+FW]);
            float vt=velocity->v[j]+tx*(velocity->v[j+1]-velocity->v[j]);
            float vb=velocity->v[j+FW]+tx*(velocity->v[j+FW+1]-velocity->v[j+FW]);
            next->u[k]=(ut+ty*(ub-ut))*decay;
            next->v[k]=(vt+ty*(vb-vt))*decay;
        }
    }
}
void fluid_advect_dye(FluidDye *restrict next,const FluidDye *restrict ink,
        const FluidVelocity *restrict velocity,float grid_dt,float decay,float gold_decay) {
    float fy=0;
    for(int y=0;y<FH;y++,fy+=1) {
        float fx=0;
        for(int x=0;x<FW;x++,fx+=1) {
            int k=y*FW+x;
            float px=advection_coord(fx-grid_dt*velocity->u[k],FW-1.001f);
            float py=advection_coord(fy-grid_dt*velocity->v[k],FH-1.001f);
            int ix=(int)px,iy=(int)py,j=iy*FW+ix;
            float tx=px-ix,ty=py-iy;
            float rt=ink->red[j]+tx*(ink->red[j+1]-ink->red[j]);
            float rb=ink->red[j+FW]+tx*(ink->red[j+FW+1]-ink->red[j+FW]);
            float bt=ink->blue[j]+tx*(ink->blue[j+1]-ink->blue[j]);
            float bb=ink->blue[j+FW]+tx*(ink->blue[j+FW+1]-ink->blue[j+FW]);
            float gt=ink->gold[j]+tx*(ink->gold[j+1]-ink->gold[j]);
            float gb=ink->gold[j+FW]+tx*(ink->gold[j+FW+1]-ink->gold[j+FW]);
            next->red[k]=(rt+ty*(rb-rt))*decay;
            next->blue[k]=(bt+ty*(bb-bt))*decay;
            next->gold[k]=(gt+ty*(gb-gt))*gold_decay;
        }
    }
}
