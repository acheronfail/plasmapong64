#ifndef VELOCITY_CASES_H
#define VELOCITY_CASES_H
#include "../src/fluid_velocity_fixed.h"
#include "../src/fluid_advection.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#ifdef PLASMAPONG_VELOCITY_RSP
#define VELOCITY_LOG debugf
#else
#define VELOCITY_LOG printf
#endif
static void velocity_cases(void) {
    static FluidVelocity source,actual,floating;
    static struct { _Alignas(16) unsigned char before[16]; FluidVelocityFixed value; unsigned char after[16]; } packed,output;
    static FluidVelocityFixed expected,saved;
    static _Alignas(16) FluidDyeTrace traces[FN],saved_traces[FN];
    uint32_t seed=8191;
    float max_error=0,max_game_error=0;
    for(int trial=0;trial<64;trial++) {
        float dt=trial%4==0?0:trial%4==1?1.0f/30:trial%4==2?1.0f/60:.2f;
        float decay=1-.16f*dt;
        for(int k=0;k<FN;k++) {
            seed=seed*1664525u+1013904223u;
            source.u[k]=((seed>>8)*(1.0f/16777216)-.5f)*840;
            seed=seed*1664525u+1013904223u;
            source.v[k]=((seed>>8)*(1.0f/16777216)-.5f)*840;
            if(trial<4) source.u[k]=source.v[k]=0;
            if(trial>=4 && trial<8) source.u[k]=-210.25f,source.v[k]=110.5f;
            if(trial>=8 && trial<12) source.u[k]=(k%2?1:-1)*420,source.v[k]=-source.u[k];
            if(trial>=60) source.u[k]=(k%2?1:-1)*1000000,source.v[k]=-source.u[k];
        }
        memset(&packed,0xa5,sizeof(packed)); memset(&output,0x5a,sizeof(output));
        fluid_velocity_pack(&packed.value,traces,&source,dt/CELL);
        saved=packed.value; memcpy(saved_traces,traces,sizeof(traces));
        for(int k=0;k<FN;k++) assert(traces[k].offset/2+FW+1<FN);
        unsigned d=fluid_dye_decay(decay),rounding=(trial*40503u)&65535u;
        if(trial==59) d=0,decay=0;
        fluid_velocity_fixed_reference(&expected,&packed.value,traces,d,rounding);
#ifdef PLASMAPONG_VELOCITY_RSP
        data_cache_hit_writeback_invalidate(&packed,sizeof(packed));
        data_cache_hit_writeback_invalidate(&output,sizeof(output));
        data_cache_hit_writeback_invalidate(traces,sizeof(traces));
        rdpq_set_fill_color(RGBA32(trial,0,0,255));
        unsigned decays[2]={d,d};
        fluid_channels_rsp(output.value.u,packed.value.u,traces,2,decays,rounding);
        data_cache_hit_invalidate(&packed,sizeof(packed));
        data_cache_hit_invalidate(&output,sizeof(output));
        data_cache_hit_invalidate(traces,sizeof(traces));
#else
        fluid_velocity_fixed_reference(&output.value,&packed.value,traces,d,rounding);
#endif
        assert(!memcmp(&expected,&output.value,sizeof(expected)));
        /* Exercise the complete trace -> interpolation path as well as the
           explicit-trace kernel, including its single-wait queued variant. */
#ifdef PLASMAPONG_VELOCITY_FIXED
        fluid_advect_velocity_fixed(&output.value,&packed.value,dt/CELL,decay,rounding);
        assert(!memcmp(&expected,&output.value,sizeof(expected)));
#endif
        assert(!memcmp(&saved,&packed.value,sizeof(saved)));
        assert(!memcmp(saved_traces,traces,sizeof(traces)));
        for(int i=0;i<16;i++) assert(output.before[i]==0x5a && output.after[i]==0x5a && packed.before[i]==0xa5 && packed.after[i]==0xa5);
        fluid_velocity_unpack(&actual,&output.value);
        fluid_advect_velocity(&floating,&source,dt/CELL,decay);
        for(int k=0;k<FN;k++) {
            assert(isfinite(actual.u[k]) && isfinite(actual.v[k]));
            assert(fabsf(actual.u[k])<=1023.9375f && fabsf(actual.v[k])<=1023.9375f);
            if(trial<60) {
                float e=fmaxf(fabsf(actual.u[k]-floating.u[k]),fabsf(actual.v[k]-floating.v[k]));
                if(e>max_error) max_error=e;
                if(dt<=1.0f/30 && e>max_game_error) max_game_error=e;
            }
        }
    }
    VELOCITY_LOG("Velocity PASS: 64 signed fields, zero/unity decay, saturation, DMA guards and source integrity; max in-range error %.6f pixels/s (normal timestep %.6f)\n",(double)max_error,(double)max_game_error);
    assert(max_error<1.5f && max_game_error<.5f);
}
#endif
