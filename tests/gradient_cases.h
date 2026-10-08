#ifndef GRADIENT_CASES_H
#define GRADIENT_CASES_H
#include "../src/fluid_velocity_fixed.h"
#include "../src/fluid_pressure.h"
static int16_t gradient_expected(int16_t velocity,int32_t a,int32_t b) {
    int64_t d=(int64_t)a-b;
    const int divisor=PLASMAPONG_CELL_Q4*32,half=divisor/2;
    int64_t change=d>=0?(d+half)/divisor:-((-d+half)/divisor);
    int64_t value=velocity-change;
    return value>16383?16383:value<-16383?-16383:(int16_t)value;
}
static void gradient_cases(void) {
    static struct { _Alignas(16) uint8_t before[16]; FluidVelocityFixed value; uint8_t after[16]; } output;
    static struct { _Alignas(16) uint8_t before[16]; int32_t value[FN]; uint8_t after[16]; } pressure,saved;
    static FluidVelocityFixed expected;
    /* Exhaust the reciprocal identity over its full unsaturated domain. */
    for(uint64_t n=0;n<98304;n++) assert((n*43691>>17)==n/3);
    uint32_t rng=931;
    const int32_t edge[]={0,1,-1,1535,1536,1537,3071,3072,3073,
        65535,65536,65537,67107327,67107328,67107329,
        100655615,100655616,100655617,100658687,100658688,100658689,
        100660223,100660224,100660225,100661759,100661760,100661761,134184960};
    enum { EDGE_COUNT=sizeof(edge)/sizeof(edge[0]), TRIALS=EDGE_COUNT*4+50 };
    for(unsigned trial=0;trial<TRIALS;trial++) {
        memset(&output,0xa5,sizeof(output));
        memset(&pressure,0x5a,sizeof(pressure));
        for(int k=0;k<FN;k++) {
            rng=rng*1664525u+1013904223u;
            static const unsigned ranges[]={4096,65536,1048576,16777216,134184960};
            unsigned range=trial<EDGE_COUNT*4+8?134184960:ranges[trial%5];
            pressure.value[k]=(int32_t)(rng%(2*range+1))-(int32_t)range;
            if(trial<EDGE_COUNT*3) {
                int32_t d=edge[trial%EDGE_COUNT];
                pressure.value[k]=(k%4<2?0:d)*(trial&1?-1:1);
            }
            if(trial>=EDGE_COUNT*3 && trial<EDGE_COUNT*4) {
                int32_t d=edge[trial%EDGE_COUNT];
                pressure.value[k]=(k/FW%4<2?0:d)*(trial&1?-1:1);
            }
            if(trial>=EDGE_COUNT*4 && trial<EDGE_COUNT*4+8) pressure.value[k]=(k%4<2?1:-1)*134184960;
            rng=rng*1664525u+1013904223u;
            output.value.u[k]=(int)(rng%32767)-16383;
            rng=rng*1664525u+1013904223u;
            output.value.v[k]=(int)(rng%32767)-16383;
            if(trial<EDGE_COUNT*3) output.value.u[k]=output.value.v[k]=trial<EDGE_COUNT?0:trial<EDGE_COUNT*2?16383:-16383;
        }
        saved=pressure;
        expected=output.value;
        for(int y=0;y<FH;y++) for(int x=0;x<FW;x++) {
            int k=y*FW+x;
            expected.u[k]=x==0 || x==FW-1?0:gradient_expected(expected.u[k],pressure.value[k+1],pressure.value[k-1]);
            expected.v[k]=y==0 || y==FH-1?0:gradient_expected(expected.v[k],pressure.value[k+FW],pressure.value[k-FW]);
        }
        data_cache_hit_writeback_invalidate(&pressure,sizeof(pressure));
        data_cache_hit_writeback_invalidate(&output,sizeof(output));
        /* Force an overlay transition before every case. */
        rdpq_set_fill_color(RGBA32(trial,0,0,255));
        fluid_gradient_rsp(&output.value,pressure.value);
        data_cache_hit_invalidate(&pressure,sizeof(pressure));
        data_cache_hit_invalidate(&output,sizeof(output));
        for(int k=0;k<FN;k++) {
            if(expected.u[k]!=output.value.u[k] || expected.v[k]!=output.value.v[k]) {
                debugf("Gradient mismatch trial %u cell %d: u %d/%d v %d/%d\n",trial,k,expected.u[k],output.value.u[k],expected.v[k],output.value.v[k]);
                assert(0);
            }
        }
        assert(!memcmp(&pressure,&saved,sizeof(saved)));
        for(int k=0;k<16;k++) assert(output.before[k]==0xa5 && output.after[k]==0xa5);
    }
    debugf("Gradient PASS: %u exact signed pressure fields, rounded division thresholds, saturation, all walls, source integrity and DMA guards\n",(unsigned)TRIALS);
#ifdef PLASMAPONG_PROJECTION_CHAIN
    static struct { _Alignas(16) uint8_t before[16]; int32_t value[FN]; uint8_t after[16]; } div;
    static _Alignas(16) int32_t expected_pressure[FN],expected_div[FN];
    for(unsigned trial=0;trial<64;trial++) {
        memset(&output,0xa5,sizeof(output)); memset(&pressure,0x5a,sizeof(pressure)); memset(&div,0x3c,sizeof(div));
        memset(expected_div,0x3c,sizeof(expected_div));
        for(int k=0;k<FN;k++) {
            rng=rng*1664525u+1013904223u;
            output.value.u[k]=(int)(rng%32767)-16383;
            rng=rng*1664525u+1013904223u;
            output.value.v[k]=(int)(rng%32767)-16383;
            if(trial<4) output.value.u[k]=output.value.v[k]=trial&1?16383:-16383;
        }
        for(int y=0;y<FH;y++) output.value.u[y*FW]=output.value.u[y*FW+FW-1]=0;
        for(int x=0;x<FW;x++) output.value.v[x]=output.value.v[(FH-1)*FW+x]=0;
        expected=output.value;
        for(int y=1;y<FH-1;y++) for(int x=0;x<FW;x++) {
            int k=y*FW+x;
            expected_div[k]=x==0 || x==FW-1?0:-(PLASMAPONG_CELL_Q4*8)*(expected.u[k+1]-expected.u[k-1]+expected.v[k+FW]-expected.v[k-FW]);
        }
#if PLASMAPONG_PRESSURE_WARM_START
        memcpy(expected_pressure,pressure.value,sizeof(expected_pressure));
#endif
        fluid_pressure_cpu(expected_pressure,expected_div);
        for(int y=0;y<FH;y++) for(int x=0;x<FW;x++) {
            int k=y*FW+x;
            expected.u[k]=x==0 || x==FW-1?0:gradient_expected(expected.u[k],expected_pressure[k+1],expected_pressure[k-1]);
            expected.v[k]=y==0 || y==FH-1?0:gradient_expected(expected.v[k],expected_pressure[k+FW],expected_pressure[k-FW]);
        }
        /* Discard dirty divergence before producer DMA; a warm pressure guess
           is input/output and must instead reach RDRAM before its producer. */
        rdpq_set_fill_color(RGBA32(trial,trial,0,255));
        fluid_projection_rsp(&output.value,div.value,pressure.value);
        assert(!memcmp(&expected,&output.value,sizeof(expected)));
        assert(!memcmp(expected_div,div.value,sizeof(expected_div)));
        assert(!memcmp(expected_pressure,pressure.value,sizeof(expected_pressure)));
        for(int i=0;i<16;i++) {
            assert(output.before[i]==0xa5 && output.after[i]==0xa5);
            assert(pressure.before[i]==0x5a && pressure.after[i]==0x5a);
            assert(div.before[i]==0x3c && div.after[i]==0x3c);
        }
    }
    debugf("Projection chain PASS: 64 CPU-reference fields, walls, dirty caches, overlay switches and DMA guards\n");
#endif
}
#endif
