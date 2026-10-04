#include "fluid.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned rng=7143;
static unsigned random_u(void) { rng=rng*1664525u+1013904223u; return rng; }
static float random_f(float lo,float hi) { return lo+(hi-lo)*(random_u()>>8)*(1.0f/16777216); }
int main(void) {
    static Fluid expected,actual;
    float x[26],y[26];
    for(unsigned trial=0;trial<1000;trial++) {
        for(unsigned slot=0;slot<26;slot++) {
            /* Reuse geometry, then change it to exercise cache invalidation. */
            if(trial%8==0) {
                x[slot]=random_f(-36,ARENA_W+36);
                y[slot]=random_f(-36,ARENA_H+36);
            }
            expected.velocity_bank=trial%2; expected.dye_bank=(trial/2)%2;
            for(unsigned k=0;k<FN;k++) {
                fluid_velocity(&expected)->u[k]=(int)(random_u()%32767)-16383;
                fluid_velocity(&expected)->v[k]=(int)(random_u()%32767)-16383;
                fluid_dye(&expected)->red[k]=(int)(random_u()%32767)-16383;
                fluid_dye(&expected)->blue[k]=(int)(random_u()%32767)-16383;
                fluid_dye(&expected)->gold[k]=(int)(random_u()%32767)-16383;
            }
            actual=expected;
            float u=random_f(-1500,1500),v=random_f(-1500,1500),gold=random_f(0,3);
            fluid_splat(&expected,x[slot],y[slot],12,u,v,0,0);
            fluid_ball_dye(&expected,x[slot],y[slot],gold);
            fluid_menu_source(&actual,slot,x[slot],y[slot],u,v,gold);
            assert(!memcmp(&expected,&actual,sizeof(actual)));
        }
    }
    puts("PASS: 26000 cached menu sources exactly match scalar splat/gold, cache invalidation, source banks, zero weights and clamps");
}
