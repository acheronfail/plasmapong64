#ifndef CONFINEMENT_CASES_H
#define CONFINEMENT_CASES_H
#include "../src/fluid_confinement.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#ifdef PLASMAPONG_CONFINEMENT_RSP
#define CONF_LOG debugf
#else
#define CONF_LOG printf
#endif
static int conf_clamp(int v) { return v>VELOCITY_LIMIT?VELOCITY_LIMIT:v<-VELOCITY_LIMIT?-VELOCITY_LIMIT:v; }
/* Frozen float curl/confinement arithmetic from fae1bf7, with Q4 velocity. */
static void confinement_float(FluidVelocityFixed *v,float dt) {
    static float curl[FN];
    for(int y=1;y<FH-1;y++) for(int x=1;x<FW-1;x++) {
        int k=y*FW+x;
        curl[k]=(v->v[k+1]-v->v[k-1]-v->u[k+FW]+v->u[k-FW])*(.5f/(CELL*VELOCITY_SCALE));
    }
    float confinement=CELL*1.1f*dt;
    for(int y=2;y<FH-2;y++) for(int x=2;x<FW-2;x++) {
        int k=y*FW+x;
        float nx=fabsf(curl[k+1])-fabsf(curl[k-1]);
        float ny=fabsf(curl[k+FW])-fabsf(curl[k-FW]);
        float inv=1/sqrtf(nx*nx+ny*ny+.00001f);
        float du=ny*(inv*curl[k]*confinement)*VELOCITY_SCALE;
        float dv=-nx*(inv*curl[k]*confinement)*VELOCITY_SCALE;
        v->u[k]=(int16_t)conf_clamp(v->u[k]+(int)(du+(du<0?-.5f:.5f)));
        v->v[k]=(int16_t)conf_clamp(v->v[k]+(int)(dv+(dv<0?-.5f:.5f)));
    }
}
static void confinement_cases(void) {
    static struct { _Alignas(16) unsigned char pre[16]; FluidVelocityFixed value; unsigned char post[16]; } input,actual;
    static struct { _Alignas(16) unsigned char pre[16]; int16_t value[FN]; unsigned char post[16]; } curls;
    static FluidVelocityFixed expected,floating,saved;
    static _Alignas(16) int16_t expected_curl[FN];
    uint32_t seed=378991;
    float maximum=0,normal_max=0;
    for(int trial=0;trial<64;trial++) {
        float dt=trial%4==0?0:trial%4==1?1.0f/30:trial%4==2?1.0f/60:.2f;
        memset(&input,0xa5,sizeof(input)); memset(&actual,0x5a,sizeof(actual));
        memset(&curls,0x3c,sizeof(curls));
        for(int k=0;k<FN;k++) {
            seed=seed*1664525u+1013904223u;
            int u=(int)(seed%13441)-6720;
            seed=seed*1664525u+1013904223u;
            int v=(int)(seed%13441)-6720;
            if(trial<4) u=v=0;
            if(trial>=4 && trial<8) u=1200,v=-640;
            if(trial>=8 && trial<12) u=((k/FW)%4<2?-1:1)*VELOCITY_LIMIT,v=(k%4<2?1:-1)*VELOCITY_LIMIT;
            if(trial>=12 && trial<16) u=(k%FW-24)*13,v=(k/FW-16)*11;
            if(trial>=16 && trial<20) u=(int)(k%3)-1,v=(int)(k%5)-2;
            if(trial>=20 && trial<24) u=(k==FW*16+24)?VELOCITY_LIMIT:0,v=0;
            if(trial>=24 && trial<28) {
                /* Strong almost-uniform curl with a one-LSB perturbation. */
                u=-(k/FW-16)*200; v=(k%FW-24)*200;
                if(k==FW*16+24) u-=1;
            }
            input.value.u[k]=(int16_t)u; input.value.v[k]=(int16_t)v;
        }
        saved=expected=floating=actual.value=input.value;
        fluid_curl_fixed(expected_curl,&input.value);
        unsigned strength=fluid_confinement_strength(dt);
        fluid_confinement_fixed(&expected,expected_curl,strength);
        confinement_float(&floating,dt);
#ifdef PLASMAPONG_CONFINEMENT_RSP
        data_cache_hit_writeback_invalidate(&input,sizeof(input));
        data_cache_hit_writeback_invalidate(&actual,sizeof(actual));
        data_cache_hit_writeback_invalidate(&curls,sizeof(curls));
        rdpq_set_fill_color(RGBA32(trial,0,0,255));
        fluid_curl_rsp(curls.value,&input.value);
        rdpq_set_fill_color(RGBA32(0,trial,0,255));
        fluid_confinement_rsp(&actual.value,curls.value,strength);
        data_cache_hit_invalidate(&input,sizeof(input));
        data_cache_hit_invalidate(&actual,sizeof(actual));
        data_cache_hit_invalidate(&curls,sizeof(curls));
#else
        fluid_curl_fixed(curls.value,&input.value);
        fluid_confinement_fixed(&actual.value,curls.value,strength);
#endif
        assert(!memcmp(&saved,&input.value,sizeof(saved)));
        for(int k=0;k<FN;k++) {
#ifdef PLASMAPONG_CONFINEMENT_RSP
            assertf(curls.value[k]==expected_curl[k],"curl trial %d cell %d got %d expected %d",trial,k,curls.value[k],expected_curl[k]);
            assertf(actual.value.u[k]==expected.u[k] && actual.value.v[k]==expected.v[k],"force trial %d cell %d got %d,%d expected %d,%d",trial,k,actual.value.u[k],actual.value.v[k],expected.u[k],expected.v[k]);
#else
            assert(curls.value[k]==expected_curl[k]);
            assert(actual.value.u[k]==expected.u[k] && actual.value.v[k]==expected.v[k]);
#endif
            float error=fmaxf(fabsf((float)(actual.value.u[k]-floating.u[k])),fabsf((float)(actual.value.v[k]-floating.v[k])))/VELOCITY_SCALE;
            maximum=fmaxf(maximum,error);
            if(dt<=1.0f/30) normal_max=fmaxf(normal_max,error);
            int x=k%FW,y=k/FW;
            if(x<2 || x>=FW-2 || y<2 || y>=FH-2 || dt==0) {
                assert(actual.value.u[k]==saved.u[k] && actual.value.v[k]==saved.v[k]);
            }
        }
        for(int i=0;i<16;i++) {
            assert(input.pre[i]==0xa5 && input.post[i]==0xa5);
            assert(actual.pre[i]==0x5a && actual.post[i]==0x5a);
            assert(curls.pre[i]==0x3c && curls.post[i]==0x3c);
        }
    }
    CONF_LOG("Confinement PASS: 64 fixed-reference fields, boundaries, input integrity and DMA guards; max float delta %.6f px/s (normal dt %.6f)\n",(double)maximum,(double)normal_max);
    assert(maximum<=8 && normal_max<=2);
}
#endif
