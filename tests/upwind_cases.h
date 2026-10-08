#ifndef UPWIND_CASES_H
#define UPWIND_CASES_H
#include "../src/fluid_upwind.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint8_t before[16]; FluidVelocityFixed value; uint8_t after[16]; } UpwindVelocityGuard;
typedef struct { uint8_t before[16]; FluidDyeFixed value; uint8_t after[16]; } UpwindDyeGuard;
static void upwind_cases(void) {
    static UpwindVelocityGuard a,b,expected,actual;
    static UpwindDyeGuard ink,wanted,got;
    unsigned rng=731;
    for(unsigned trial=0;trial<96;trial++) {
        memset(&a,0x3c,sizeof(a)); memset(&b,0x3c,sizeof(b));
        memset(&expected,0xa5,sizeof(expected)); memset(&actual,0xa5,sizeof(actual));
        memset(&ink,0x5a,sizeof(ink)); memset(&wanted,0xa5,sizeof(wanted)); memset(&got,0xa5,sizeof(got));
        for(int k=0;k<FN;k++) {
            rng=rng*1664525u+1013904223u;
            int u=(int)(rng%32767)-16383;
            rng=rng*1664525u+1013904223u;
            int v=(int)(rng%32767)-16383;
            if(trial<8) u=v=0;
            if(trial>=8 && trial<16) { u=trial&1?3200:-3200; v=0; }
            if(trial>=16 && trial<24) { u=0; v=trial&1?3200:-3200; }
            a.value.u[k]=b.value.u[k]=(int16_t)u; a.value.v[k]=b.value.v[k]=(int16_t)v;
            rng=rng*1664525u+1013904223u;
            ink.value.red[k]=(int16_t)(rng%24577);
            ink.value.blue[k]=(int16_t)((rng>>8)%24577);
            ink.value.gold[k]=(int16_t)((rng>>16)%5325);
        }
        float grid_dt=trial%4==0?0:trial%4==1?(1.0f/60)/CELL:trial%4==2?(1.0f/30)/CELL:.125f;
        float decay=trial%7==0?1:trial%7==1?0:.995f;
        unsigned rounding=(trial*40503u)&65535u;
        fluid_upwind_velocity_cpu(&expected.value,&a.value,grid_dt,decay,rounding);
#ifdef PLASMAPONG_UPWIND_RSP
        fluid_upwind_velocity_rsp(&actual.value,&b.value,grid_dt,decay,rounding);
#else
        fluid_upwind_velocity_cpu(&actual.value,&b.value,grid_dt,decay,rounding);
#endif
        for(int k=0;k<FN;k++) {
#ifdef PLASMAPONG_UPWIND_RSP
            if(expected.value.u[k]!=actual.value.u[k] || expected.value.v[k]!=actual.value.v[k])
                debugf("Upwind mismatch trial %u cell %d: u %d/%d v %d/%d\n",trial,k,
                    actual.value.u[k],expected.value.u[k],actual.value.v[k],expected.value.v[k]);
#endif
            assert(expected.value.u[k]==actual.value.u[k] && expected.value.v[k]==actual.value.v[k]);
        }
        assert(!memcmp(&a.value,&b.value,sizeof(a.value)));
        fluid_upwind_ink_cpu(&wanted.value,&ink.value,&a.value,grid_dt,decay,.98f,rounding);
#ifdef PLASMAPONG_UPWIND_RSP
        fluid_upwind_ink_rsp(&got.value,&ink.value,&b.value,grid_dt,decay,.98f,rounding);
#else
        fluid_upwind_ink_cpu(&got.value,&ink.value,&b.value,grid_dt,decay,.98f,rounding);
#endif
        for(int k=0;k<FN;k++) {
#ifdef PLASMAPONG_UPWIND_RSP
            if(wanted.value.red[k]!=got.value.red[k] || wanted.value.blue[k]!=got.value.blue[k] || wanted.value.gold[k]!=got.value.gold[k])
                debugf("Upwind dye mismatch trial %u cell %d: red %d/%d blue %d/%d gold %d/%d\n",trial,k,
                    got.value.red[k],wanted.value.red[k],got.value.blue[k],wanted.value.blue[k],got.value.gold[k],wanted.value.gold[k]);
#endif
            assert(wanted.value.red[k]==got.value.red[k] && wanted.value.blue[k]==got.value.blue[k] && wanted.value.gold[k]==got.value.gold[k]);
            assert(got.value.red[k]>=0 && got.value.red[k]<=24576);
            assert(got.value.blue[k]>=0 && got.value.blue[k]<=24576);
            int u=b.value.u[k],v=b.value.v[k];
            unsigned magnitude=(unsigned)((u<0?-u:u)+(v<0?-v:v));
            assert(magnitude<=fluid_upwind_limit(fluid_upwind_step(grid_dt)));
        }
        assert(!memcmp(&a.value,&b.value,sizeof(a.value)));
        for(int k=0;k<16;k++) {
            assert(actual.before[k]==0xa5 && actual.after[k]==0xa5);
            assert(got.before[k]==0xa5 && got.after[k]==0xa5);
            assert(b.before[k]==0x3c && b.after[k]==0x3c);
            assert(ink.before[k]==0x5a && ink.after[k]==0x5a);
        }
    }
#ifdef PLASMAPONG_UPWIND_RSP
    debugf("Upwind PASS: 96 exact CPU/RSP velocity/dye fields, all directions, zero/unity decay, limiter consistency and DMA guards\n");
#else
    puts("PASS: 96 upwind velocity/dye fields, bounded convex transport, limiter consistency and guards");
#endif
}
#endif
