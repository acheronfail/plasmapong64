#ifndef VELOCITY_CHAIN_CASES_H
#define VELOCITY_CHAIN_CASES_H
#include "../src/fluid_upwind.h"
#include "../src/fluid_confinement.h"
#include <string.h>
#include "../src/fluid_queue.h"
/* The established synchronous stages are an independent scheduling oracle.
   This checks complete state, including the limited old bank and public fields. */
static void velocity_chain_cases(void) {
    static struct { _Alignas(16) uint8_t pre[16]; Fluid f; uint8_t post[16]; } actual;
    static Fluid expected;
    uint32_t seed=17812;
    for(unsigned trial=0;trial<32;trial++) {
        memset(&actual,0,sizeof(actual));
        memset(actual.pre,0xa5,16); memset(actual.post,0x5a,16);
        actual.f.velocity_bank=trial&1;
        actual.f.velocity_phase=trial*17131u;
        for(unsigned k=0;k<FN;k++) {
            seed=seed*1664525u+1013904223u;
            fluid_velocity(&actual.f)->u[k]=(int)(seed%16383)-8191;
            seed=seed*1664525u+1013904223u;
            fluid_velocity(&actual.f)->v[k]=(int)(seed%16383)-8191;
            actual.f.pressure_short[k]=(int)(seed%401)-200;
            actual.f.pressure[k]=(int32_t)actual.f.pressure_short[k]*512;
        }
        expected=actual.f;
        float dt=trial%4==0?0:trial%4==1?1.0f/60:trial%4==2?1.0f/30:.2f;
        FluidVelocityFixed *old=fluid_velocity(&expected),*next=&expected.velocity[expected.velocity_bank^1];
        fluid_upwind_velocity_rsp(next,old,dt/CELL,1-FLUID_DAMPING*dt,
            (expected.velocity_phase+=40503u)&65535u);
        if(FLUID_CONFINEMENT>0) {
            fluid_curl_fixed(expected.curl_fixed,next);
            fluid_confinement_window_fixed(next,expected.curl_fixed,
                fluid_confinement_strength(dt)*FLUID_CONFINEMENT_BANDS,
                fluid_confinement_first(expected.velocity_phase),fluid_confinement_rows(expected.velocity_phase));
        }
        expected.velocity_bank^=1;
        fluid_project(&expected);
        rdpq_set_fill_color(RGBA32(trial,0,0,255));
        fluid_highpri_active=(trial&1)!=0;
        fluid_velocity_chain_rsp(&actual.f,dt);
        fluid_highpri_active=false;
        const unsigned char *a=(const unsigned char *)&actual.f,*b=(const unsigned char *)&expected;
        for(unsigned k=0;k<sizeof(expected);k++)
            assertf(a[k]==b[k],"velocity chain trial %u byte %u got %u expected %u",trial,k,a[k],b[k]);
        for(unsigned k=0;k<16;k++) assert(actual.pre[k]==0xa5 && actual.post[k]==0x5a);
    }
    debugf("Velocity chain PASS: 32 exact full-state comparisons, both banks, dirty caches, warm pressure, normal/high-priority queues, walls and guards\n");
}
#endif
