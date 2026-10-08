#include "fluid.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
/* Compare the specialised suction to the continuous softened radial field,
   including clipped edges, off-centre emitters and both gameplay tick sizes. */
int main(void) {
    static Fluid f;
    int worst=0;
    for(int trial=0;trial<64;trial++) for(int hz=30;hz<=60;hz+=30) {
        float x=(trial*917%4608)/16.0f,y=(trial*617%3168)/16.0f;
        fluid_init(&f);
        fluid_pump(&f,x,y,35,-1150,1.0f/hz,trial%4);
        int amplitude=(int)(-1150.0f/hz*16-.5f);
        const FluidFlow *v=fluid_velocity(&f);
        const FluidInk *ink=fluid_dye(&f);
        for(int iy=0;iy<FH;iy++) for(int ix=0;ix<FW;ix++) {
            float dx=(ix+.5f)*CELL-x,dy=(iy+.5f)*CELL-y;
            float d2=dx*dx+dy*dy;
            float gain=fmaxf(0,1-d2/(35*35))/sqrtf(d2+9);
            int expected_u=(int)roundf(dx*gain*amplitude);
            int expected_v=(int)roundf(dy*gain*amplitude);
            int k=iy*FW+ix;
            int du=abs(v->u[k]-expected_u),dv=abs(v->v[k]-expected_v);
            if(du>worst) worst=du;
            if(dv>worst) worst=dv;
            assert(du<=3 && dv<=3);
            assert(v->u[k]*dx<=0 && v->v[k]*dy<=0);
            assert(!ink->red[k] && !ink->blue[k] && !ink->gold[k]);
        }
    }
    printf("PASS: subpixel suction matches softened radial field within %d Q4 units\n",worst);
}
