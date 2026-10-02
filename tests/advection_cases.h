/* Shared host/N64 regression against the pre-optimization float sampler. */
#ifndef ADVECTION_CASES_H
#define ADVECTION_CASES_H
#include "../src/fluid_advection.h"
#include "../src/mathutil.h"
#include <assert.h>
#include <string.h>

static float advection_reference_sample(const float *a,float x,float y) {
    x=clampf(x,0,FW-1.001f); y=clampf(y,0,FH-1.001f);
    int ix=(int)x,iy=(int)y,k=iy*FW+ix;
    float tx=x-ix,ty=y-iy;
    float top=a[k]+tx*(a[k+1]-a[k]);
    float bottom=a[k+FW]+tx*(a[k+FW+1]-a[k+FW]);
    return top+ty*(bottom-top);
}
static float advection_random(uint32_t *seed) {
    *seed=*seed*1664525u+1013904223u;
    return (*seed>>8)*(1.0f/16777216.0f);
}
static void advection_cases(void) {
    static FluidVelocity v,velocity_copy,out_v;
    static FluidDye ink,ink_copy,out_ink;
    uint32_t seed=0x14539;
    for(unsigned trial=0;trial<64;trial++) {
        float dt=trial%4==0?0:trial%4==1?1.0f/30:trial%4==2?1.0f/60:.2f;
        float grid_dt=dt/CELL,decay=1-.16f*dt;
        float dye_decay=1-.22f*dt,gold_decay=1-1.1f*dt;
        for(int y=0;y<FH;y++) for(int x=0;x<FW;x++) {
            int k=y*FW+x;
            float speed=trial<32?420:1000000;
            v.u[k]=(2*advection_random(&seed)-1)*speed;
            v.v[k]=(2*advection_random(&seed)-1)*speed;
            ink.red[k]=3*advection_random(&seed);
            ink.blue[k]=3*advection_random(&seed);
            ink.gold[k]=.65f*advection_random(&seed);
            if(trial<4) {
                v.u[k]=x%2?0.0f:-0.0f;
                v.v[k]=y%2?-0.0f:0.0f;
            }
            if(trial>=4 && trial<8) {
                ink.red[k]=2; ink.blue[k]=1; ink.gold[k]=.5f;
            }
            if(trial>=8 && trial<12) {
                ink.red[k]=(float)x/FW;
                ink.blue[k]=(float)y/FH;
                ink.gold[k]=(float)(x+y)/(FW+FH)*.65f;
            }
            if(trial>=12 && trial<16) {
                ink.red[k]=(x+y)%2?3:0;
                ink.blue[k]=x%2?3:0;
                ink.gold[k]=y%2?.65f:0;
            }
        }
        velocity_copy=v; ink_copy=ink;
        memset(&out_v,0xff,sizeof(out_v));
        memset(&out_ink,0xff,sizeof(out_ink));
        fluid_advect_velocity(&out_v,&v,grid_dt,decay);
        fluid_advect_dye(&out_ink,&ink,&v,grid_dt,dye_decay,gold_decay);
        assert(memcmp(&v,&velocity_copy,sizeof(v))==0);
        assert(memcmp(&ink,&ink_copy,sizeof(ink))==0);
        for(int y=0;y<FH;y++) for(int x=0;x<FW;x++) {
            int k=y*FW+x;
            float px=x-grid_dt*v.u[k],py=y-grid_dt*v.v[k];
            assert(out_v.u[k]==advection_reference_sample(v.u,px,py)*decay);
            assert(out_v.v[k]==advection_reference_sample(v.v,px,py)*decay);
            assert(out_ink.red[k]==advection_reference_sample(ink.red,px,py)*dye_decay);
            assert(out_ink.blue[k]==advection_reference_sample(ink.blue,px,py)*dye_decay);
            assert(out_ink.gold[k]==advection_reference_sample(ink.gold,px,py)*gold_decay);
        }
    }
}
#endif
