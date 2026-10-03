#ifndef DYE_CASES_H
#define DYE_CASES_H
#include "../src/fluid_dye_fixed.h"
#include "../src/fluid_advection.h"
#include "../src/mathutil.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#ifdef PLASMAPONG_DYE_RSP
#define DYE_TEST_LOG debugf
#else
#define DYE_TEST_LOG printf
#endif

typedef struct { uint8_t before[16]; FluidDyeFixed value; uint8_t after[16]; } DyeGuarded;
typedef struct { _Alignas(16) uint8_t before[16]; FluidDyeTrace value[FLUID_TRACE_BATCHES]; uint8_t after[16]; } TraceGuarded;
static uint32_t dye_hash(const void *data,size_t n) {
    const unsigned char *p=data;
    uint32_t h=2166136261u;
    while(n--) h=(h^*p++)*16777619u;
    return h;
}
static float dye_random(uint32_t *seed) {
    *seed=*seed*1664525u+1013904223u;
    return (*seed>>8)*(1.0f/16777216.0f);
}
static float dye_error(const FluidDye *a,const FluidDye *b) {
    float e=0;
    for(int k=0;k<FN;k++) {
        e=maxf(e,fabsf(a->red[k]-b->red[k]));
        e=maxf(e,fabsf(a->blue[k]-b->blue[k]));
        e=maxf(e,fabsf(a->gold[k]-b->gold[k]));
    }
    return e;
}
static unsigned dye_color_error(const FluidDye *a,const FluidDye *b) {
    unsigned error=0;
    static const float weights[3][4]={{5,210,20,255},{9,64,155,205},{22,92,225,25}};
    for(int k=0;k<FN;k++) for(int c=0;c<3;c++) {
        const float *w=weights[c];
        int r=(int)(a->red[k]*DYE_SCALE),bl=(int)(a->blue[k]*DYE_SCALE),g=(int)(a->gold[k]*DYE_SCALE);
        int x=((int)w[0]*DYE_SCALE+(int)w[1]*r+(int)w[2]*bl+(int)w[3]*g)>>13;
        if(x>255) x=255;
        int y=(int)clampf(w[0]+w[1]*b->red[k]+w[2]*b->blue[k]+w[3]*b->gold[k],0,255);
        unsigned d=x>y?x-y:y-x;
        if(d>error) error=d;
    }
    return error;
}
static void dye_cases(void) {
    static FluidVelocity velocity;
    static FluidDye ink,expected_float,actual_float;
    static DyeGuarded input,output;
    static FluidDyeFixed expected;
    static TraceGuarded traces;
    uint32_t seed=917324;
    float max_error=0;
    unsigned max_color=0;
    for(unsigned trial=0;trial<64;trial++) {
        float dt=trial%4==0?0:trial%4==1?1.0f/30:trial%4==2?1.0f/60:.2f;
        float decay=1-.22f*dt,gold_decay=1-1.1f*dt;
        memset(&input,0xa5,sizeof(input));
        memset(&output,0x5a,sizeof(output));
        memset(&traces,0x3c,sizeof(traces));
        for(int k=0;k<FN;k++) {
            float speed=trial<32?420:1000000;
            velocity.u[k]=(2*dye_random(&seed)-1)*speed;
            velocity.v[k]=(2*dye_random(&seed)-1)*speed;
            ink.red[k]=3*dye_random(&seed);
            ink.blue[k]=3*dye_random(&seed);
            ink.gold[k]=.65f*dye_random(&seed);
            if(trial<4) velocity.u[k]=velocity.v[k]=0;
            if(trial>=4 && trial<8) ink.red[k]=ink.blue[k]=3,ink.gold[k]=.65f;
            if(trial>=8 && trial<12) ink.red[k]=ink.blue[k]=ink.gold[k]=0;
            if(trial>=12 && trial<16) {
                ink.red[k]=(k%2)?3:0; ink.blue[k]=(k%3)?0:3; ink.gold[k]=(k%2)?.65f:0;
            }
            if(trial>=20 && trial<24) ink.red[k]=ink.blue[k]=ink.gold[k]=1.0f/DYE_SCALE;
            if(trial>=24 && trial<28) ink.red[k]=ink.blue[k]=ink.gold[k]=67.0f/DYE_SCALE;
        }
        fluid_dye_pack(&input.value,traces.value,&ink,&velocity,dt/CELL);
        unsigned d=fluid_dye_decay(decay),g=fluid_dye_decay(gold_decay);
        /* Include a completely extinguished field and exact unity. */
        if(trial==60) d=g=0,decay=gold_decay=0;
        unsigned rounding=(trial*40503u)&65535u;
        fluid_dye_fixed_reference(&expected,&input.value,traces.value,d,g,rounding);
        uint32_t ih=dye_hash(&input,sizeof(input)),th=dye_hash(&traces,sizeof(traces));
#ifdef PLASMAPONG_DYE_RSP
        data_cache_hit_writeback_invalidate(&input,sizeof(input));
        data_cache_hit_writeback_invalidate(&traces,sizeof(traces));
        data_cache_hit_writeback_invalidate(&output,sizeof(output));
        rdpq_set_fill_color(RGBA32(trial,0,0,255));
        fluid_dye_fixed_rsp(&output.value,&input.value,traces.value,d,g,rounding);
        data_cache_hit_invalidate(&input,sizeof(input));
        data_cache_hit_invalidate(&traces,sizeof(traces));
        data_cache_hit_invalidate(&output,sizeof(output));
#else
        fluid_dye_fixed_reference(&output.value,&input.value,traces.value,d,g,rounding);
#endif
        assert(memcmp(&output.value,&expected,sizeof(expected))==0);
        assert(dye_hash(&input,sizeof(input))==ih && dye_hash(&traces,sizeof(traces))==th);
        for(int i=0;i<16;i++) assert(output.before[i]==0x5a && output.after[i]==0x5a);
        fluid_dye_unpack(&actual_float,&output.value);
        fluid_advect_dye(&expected_float,&ink,&velocity,dt/CELL,decay,gold_decay);
        float error=dye_error(&actual_float,&expected_float);
        max_error=maxf(max_error,error);
        unsigned color=dye_color_error(&actual_float,&expected_float);
        if(color>max_color) max_color=color;
        assert(error<.0005f && color<=1);
    }
    DYE_TEST_LOG("Dye PASS: 64 fixed-reference fields, source/trace integrity and guards; max float error %.8f, RGB error %u\n",
        (double)max_error,max_color);
}
#ifndef PLASMAPONG_DYE_RSP
static void dye_long_run(void) {
    static FluidDye fixed[2],reference[2];
    static FluidVelocity velocity;
    static FluidDyeFixed packed,result;
    static _Alignas(16) FluidDyeTrace trace[FLUID_TRACE_BATCHES];
    uint32_t seed=78314;
    const float dt=1.0f/30,decay=1-.22f*dt,gold_decay=1-1.1f*dt;
    float max_error=0;
    unsigned max_color=0;
    for(int k=0;k<FN;k++) {
        fixed[0].red[k]=reference[0].red[k]=3*dye_random(&seed);
        fixed[0].blue[k]=reference[0].blue[k]=3*dye_random(&seed);
        fixed[0].gold[k]=reference[0].gold[k]=.65f*dye_random(&seed);
        velocity.u[k]=((k/FW)%5-2)*40;
        velocity.v[k]=(k%5-2)*40;
    }
    for(int step=0;step<3600;step++) {
        int a=step%2,b=a^1;
        /* First minute stresses continued injections; second minute fading. */
        if(step<1800 && step%8==0) {
            unsigned k=(unsigned)(dye_random(&seed)*FN);
            fixed[a].red[k]=reference[a].red[k]=3;
            fixed[a].blue[(k+FW)%FN]=reference[a].blue[(k+FW)%FN]=3;
            fixed[a].gold[k]=reference[a].gold[k]=.65f;
        }
        fluid_dye_pack(&packed,trace,&fixed[a],&velocity,dt/CELL);
        fluid_dye_fixed_reference(&result,&packed,trace,fluid_dye_decay(decay),fluid_dye_decay(gold_decay),((step+1)*40503u)&65535u);
        fluid_dye_unpack(&fixed[b],&result);
        fluid_advect_dye(&reference[b],&reference[a],&velocity,dt/CELL,decay,gold_decay);
        float error=dye_error(&fixed[b],&reference[b]);
        max_error=maxf(max_error,error);
        unsigned color=dye_color_error(&fixed[b],&reference[b]);
        if(color>max_color) max_color=color;
        if(!(error<.006f && color<=3))
            fprintf(stderr,"Dye long-run mismatch: step %d, error %.8f, RGB %u\n",step,(double)error,color);
        assert(error<.006f && color<=3);
    }
    DYE_TEST_LOG("Dye PASS: 3600-step injection/fade comparison; max float error %.8f, RGB error %u\n",
        (double)max_error,max_color);
    for(int k=0;k<FN;k++) assert(fixed[0].red[k]==0 && fixed[0].blue[k]==0 && fixed[0].gold[k]==0);
}
#endif
#endif
