#ifndef GRADIENT_SHORT_CASES_H
#define GRADIENT_SHORT_CASES_H
#include "../src/fluid_velocity_fixed.h"
#include "../src/fluid_pressure.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static int short_expected(int velocity,int a,int b) {
    int d=(a-b)*512,divisor=PLASMAPONG_CELL_Q4*32,half=divisor/2;
    int delta=d>=0?(d+half)/divisor:-((-d+half)/divisor);
    int value=velocity-delta;
    return value>16383?16383:value<-16383?-16383:value;
}
static void gradient_short_cases(void) {
    static struct { uint8_t before[16]; FluidVelocityFixed value; uint8_t after[16]; } output;
    static struct { _Alignas(16) uint8_t before[16]; int16_t value[FN]; uint8_t after[16]; } pressure,saved;
    static struct { _Alignas(16) uint8_t before[16]; int32_t value[FN]; uint8_t after[16]; } words;
    static _Alignas(16) int32_t divergence[FN];
    static FluidVelocityFixed expected;
    static int32_t expected_pressure[FN];
    unsigned rng=173,max_error=0;
    for(unsigned trial=0;trial<96;trial++) {
        memset(&output,0xa5,sizeof(output)); memset(&pressure,0x3c,sizeof(pressure));
        memset(&words,0x5a,sizeof(words));
        for(int k=0;k<FN;k++) {
            rng=rng*1664525u+1013904223u;
            pressure.value[k]=(int16_t)(rng>>16);
            if(trial<4) pressure.value[k]=0;
            if(trial>=4 && trial<8) pressure.value[k]=k&1?32767:-32768;
            if(trial>=8 && trial<12) pressure.value[k]=(k/FW)&1?32767:-32768;
            rng=rng*1664525u+1013904223u;
            output.value.u[k]=(int)(rng%32767)-16383;
            rng=rng*1664525u+1013904223u;
            output.value.v[k]=(int)(rng%32767)-16383;
            divergence[k]=((int)(rng%65521)-32760)*4096;
#if PLASMAPONG_PRESSURE_WARM_START
            words.value[k]=expected_pressure[k]=(int32_t)pressure.value[k]*512;
#endif
        }
        data_cache_hit_writeback_invalidate(&output,sizeof(output));
        data_cache_hit_writeback_invalidate(&pressure,sizeof(pressure));
        data_cache_hit_writeback_invalidate(&words,sizeof(words));
        if(trial>=64) {
            /* Verify both pressure outputs and their chained ownership. */
            fluid_pressure_cpu(expected_pressure,divergence);
            fluid_pressure_short_rsp_begin(words.value,divergence,pressure.value);
            /* A gradient consumer below also completes the pressure producer. */
        } else saved=pressure;
        expected=output.value;
        if(trial>=64) {
            for(int k=0;k<FN;k++) saved.value[k]=(int16_t)(expected_pressure[k]>>9);
        }
        for(int y=0;y<FH;y++) for(int x=0;x<FW;x++) {
            int k=y*FW+x;
            expected.u[k]=x==0 || x==FW-1?0:(int16_t)short_expected(expected.u[k],saved.value[k+1],saved.value[k-1]);
            expected.v[k]=y==0 || y==FH-1?0:(int16_t)short_expected(expected.v[k],saved.value[k+FW],saved.value[k-FW]);
        }
        fluid_gradient_short_rsp(&output.value,pressure.value);
        data_cache_hit_invalidate(&pressure,sizeof(pressure));
        data_cache_hit_invalidate(&output,sizeof(output));
        if(trial>=64) {
            data_cache_hit_invalidate(&words,sizeof(words));
            for(int k=0;k<FN;k++) if(words.value[k]!=expected_pressure[k]) {
                debugf("Warm pressure mismatch trial %u cell %d: RSP %ld CPU %ld\n",trial,k,
                    (long)words.value[k],(long)expected_pressure[k]);
                break;
            }
            assert(!memcmp(words.value,expected_pressure,sizeof(expected_pressure)));
        }
        assert(!memcmp(pressure.value,saved.value,sizeof(pressure.value)));
        for(int k=0;k<FN;k++) {
            unsigned error=(unsigned)abs(output.value.u[k]-expected.u[k]);
            unsigned ev=(unsigned)abs(output.value.v[k]-expected.v[k]);
            if(ev>error) error=ev;
            if(error>max_error) max_error=error;
            if(error>1) debugf("Short gradient mismatch trial %u cell %d: u %d/%d v %d/%d\n",trial,k,
                output.value.u[k],expected.u[k],output.value.v[k],expected.v[k]);
            assert(error<=1);
        }
        for(int k=0;k<16;k++) {
            assert(output.before[k]==0xa5 && output.after[k]==0xa5);
            assert(pressure.before[k]==0x3c && pressure.after[k]==0x3c);
            assert(words.before[k]==0x5a && words.after[k]==0x5a);
        }
    }
    debugf("Short gradient PASS: 96 fields, <=%u Q4 unit error, dual pressure outputs, boundaries, cache ownership and guards\n",max_error);
}
#endif
