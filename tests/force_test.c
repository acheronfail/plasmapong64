/* Golden hash captured from the pre-optimisation scalar implementation.
   Mix numeric halfword values so the fixture is independent of endianness. */
#include "fluid.h"
#include <assert.h>
#include <stdio.h>
static unsigned rng=731;
static unsigned random_u(void) { rng=rng*1664525u+1013904223u; return rng; }
static float random_f(float a,float b) { return a+(b-a)*(random_u()>>8)*(1.0f/16777216); }
int main(void) {
    static Fluid a; unsigned hash=2166136261u;
    for(int trial=0;trial<2000;trial++) {
        for(int k=0;k<FN;k++) {
            fluid_velocity(&a)->u[k]=(int)(random_u()%32767)-16383;
            fluid_velocity(&a)->v[k]=(int)(random_u()%32767)-16383;
            fluid_dye(&a)->red[k]=random_u()%24577;
            fluid_dye(&a)->blue[k]=random_u()%24577;
            fluid_dye(&a)->gold[k]=random_u()%24577;
        }
        float x=random_f(-30,ARENA_W+30),y=random_f(-30,ARENA_H+30);
        float radius=random_f(1,50),u=random_f(-1500,1500),v=random_f(-1500,1500),ink=random_f(0,3);
        fluid_splat(&a,x,y,radius,u,v,ink,trial%4);
        for(int k=0;k<FN;k++) {
            hash=(hash^(uint16_t)fluid_velocity(&a)->u[k])*16777619u;
            hash=(hash^(uint16_t)fluid_velocity(&a)->v[k])*16777619u;
            hash=(hash^(uint16_t)fluid_dye(&a)->red[k])*16777619u;
            hash=(hash^(uint16_t)fluid_dye(&a)->blue[k])*16777619u;
            hash=(hash^(uint16_t)fluid_dye(&a)->gold[k])*16777619u;
        }
    }
    assert(hash==0x00cd871bu);
    puts("PASS: 2000 randomized splats retain the original fixed-storage hash, clamps and four player inks");
    rng=731; hash=2166136261u;
    for(int trial=0;trial<2000;trial++) {
        for(int k=0;k<FN;k++) {
            fluid_velocity(&a)->u[k]=(int)(random_u()%32767)-16383;
            fluid_velocity(&a)->v[k]=(int)(random_u()%32767)-16383;
            fluid_dye(&a)->red[k]=random_u()%24577;
            fluid_dye(&a)->blue[k]=random_u()%24577;
            fluid_dye(&a)->gold[k]=random_u()%24577;
        }
        float x=random_f(-30,ARENA_W+30),y=random_f(-30,ARENA_H+30);
        float radius=random_f(1,50),strength=random_f(-1500,1500);
        float dt=trial%3==0?1.0f/60:trial%3==1?1.0f/30:random_f(0,.1f);
        if(trial%7==0) strength=0;
        fluid_pump(&a,x,y,radius,strength,dt,trial%4);
        for(int k=0;k<FN;k++) {
            hash=(hash^(uint16_t)fluid_velocity(&a)->u[k])*16777619u;
            hash=(hash^(uint16_t)fluid_velocity(&a)->v[k])*16777619u;
            hash=(hash^(uint16_t)fluid_dye(&a)->red[k])*16777619u;
            hash=(hash^(uint16_t)fluid_dye(&a)->blue[k])*16777619u;
            hash=(hash^(uint16_t)fluid_dye(&a)->gold[k])*16777619u;
        }
    }
    assert(hash==0xde066d41u);
    puts("PASS: 2000 randomized pumps retain the original fixed-storage hash, suction/emission, clamps and four player inks");
}
