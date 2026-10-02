/* N64-only differential test. Run with FLUID_RSP=1 RSP_TEST=1. */
#include "../src/fluid_pressure.h"
static void rsp_fluid_smoke(void) {
    static _Alignas(16) int32_t input[FN+8],output[FN+8],expected[FN];
    uint32_t seed=0x12345678;
    const int32_t limit=32760*4096,guard=0x12345678;
    uint64_t cpu_ticks=0,rsp_ticks=0;
    for(unsigned trial=0;trial<64;trial++) {
        for(unsigned k=0;k<FN+8;k++) input[k]=output[k]=guard;
        for(unsigned k=0;k<FN;k++) {
            seed=seed*1664525u+1013904223u;
            int32_t d=(int32_t)(seed%((uint32_t)limit*2+1))-limit;
            if(trial==0) d=0;
            if(trial==1) d=limit;
            if(trial==2) d=-limit;
            if(trial==3) d=(k%2)?limit:-limit;
            if(trial>=4 && trial<8) d=(k==(trial-4)*FW+1)?limit:0;
            if(trial>=8 && trial<32) d/=128;
            input[k+4]=d;
            expected[k]=guard;
        }
        uint64_t begin=get_ticks();
        fluid_pressure_cpu(expected,input+4);
        cpu_ticks+=get_ticks()-begin;
        uint32_t input_hash=0;
        for(unsigned k=0;k<FN+8;k++) input_hash=input_hash*31u+(uint32_t)input[k];
        /* Guards must reach RDRAM and leave the cache, otherwise an out-of-
           bounds DMA could be hidden behind a stale cached guard value. */
        data_cache_hit_writeback_invalidate(input,sizeof(input));
        data_cache_hit_writeback_invalidate(output,sizeof(output));
        /* Force an overlay transition between repeated invocations. */
        rdpq_set_fill_color(RGBA32(trial,0,0,255));
        begin=get_ticks();
        fluid_pressure_rsp(output+4,input+4);
        rsp_ticks+=get_ticks()-begin;
        data_cache_hit_invalidate(input,sizeof(input));
        data_cache_hit_invalidate(output,sizeof(output));
        uint32_t after_hash=0;
        for(unsigned k=0;k<FN+8;k++) after_hash=after_hash*31u+(uint32_t)input[k];
        assert(after_hash==input_hash);
        for(unsigned k=0;k<FN;k++)
            assertf(output[k+4]==expected[k],
                "pressure trial %u cell %u: RSP %ld CPU %ld",trial,k,
                (long)output[k+4],(long)expected[k]);
        for(unsigned k=0;k<4;k++) {
            assert(input[k]==guard && input[FN+4+k]==guard);
            assert(output[k]==guard && output[FN+4+k]==guard);
        }
    }
    debugf("RSP pressure PASS: 64 bit-exact fields, DMA guards, overlay switches; CPU %llu us, RSP %llu us per solve\n",
        (unsigned long long)(TIMER_MICROS_LL(cpu_ticks)/64),
        (unsigned long long)(TIMER_MICROS_LL(rsp_ticks)/64));
}
