#include "fluid.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static unsigned rng=7143;
static unsigned random_u(void) { rng=rng*1664525u+1013904223u; return rng; }
static float random_f(float lo,float hi) { return lo+(hi-lo)*(random_u()>>8)*(1.0f/16777216); }
/* Scalar menu force oracle: menu geometry retains its float radial weight,
   independently of gameplay's integer injection kernel. */
static void scalar_menu_force(Fluid *f,float x,float y,float u,float v) {
    FluidFlow *flow=fluid_velocity(f);
    int x0=(int)fmaxf(0,fminf(FW-1,(x-12)/CELL)),x1=(int)fmaxf(0,fminf(FW-1,(x+12)/CELL));
    int y0=(int)fmaxf(0,fminf(FH-1,(y-12)/CELL)),y1=(int)fmaxf(0,fminf(FH-1,(y+12)/CELL));
    for(int iy=y0;iy<=y1;iy++) for(int ix=x0;ix<=x1;ix++) {
        int k=iy*FW+ix;
        float dx=(ix+.5f)*CELL-x,dy=(iy+.5f)*CELL-y;
        float w=fmaxf(0,1-(dx*dx+dy*dy)*(1.0f/144)); w*=w;
        float a=fmaxf(-6720,fminf(6720,flow->u[k]+u*VELOCITY_SCALE*w));
        float b=fmaxf(-6720,fminf(6720,flow->v[k]+v*VELOCITY_SCALE*w));
        flow->u[k]=(int16_t)(a+(a<0?-.5f:.5f));
        flow->v[k]=(int16_t)(b+(b<0?-.5f:.5f));
        FluidInk *ink=fluid_dye(f);
        ink->blue[k]=ink->blue[k]<0?0:ink->blue[k]>3*DYE_SCALE?3*DYE_SCALE:ink->blue[k];
    }
}
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
            scalar_menu_force(&expected,x[slot],y[slot],u,v);
            fluid_ball_dye(&expected,x[slot],y[slot],gold);
            fluid_menu_source(&actual,slot,x[slot],y[slot],u,v,gold);
            assert(!memcmp(&expected,&actual,sizeof(actual)));
        }
    }
    puts("PASS: 26000 cached menu sources exactly match scalar splat/gold, cache invalidation, source banks, zero weights and clamps");
}
