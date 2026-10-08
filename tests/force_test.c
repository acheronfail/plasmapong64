#include "fluid.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    static Fluid f;
    for(int player=0;player<4;player++) for(int sign=-1;sign<=1;sign+=2) {
        fluid_init(&f);
        for(int step=0;step<120;step++) fluid_pump(&f,147,99,35,sign*2800,1.0f/60,player);
        const FluidFlow *v=fluid_velocity(&f);
        const FluidInk *d=fluid_dye(&f);
        int moving=0;
        for(int y=0;y<FH;y++) for(int x=0;x<FW;x++) {
            int k=y*FW+x;
            assert(v->u[k]>=-6720 && v->u[k]<=6720 && v->v[k]>=-6720 && v->v[k]<=6720);
            assert(v->u[k]*((x+.5f)*CELL-147)*sign>=0);
            assert(v->v[k]*((y+.5f)*CELL-99)*sign>=0);
            moving+=v->u[k]!=0 || v->v[k]!=0;
            assert(d->red[k]>=0 && d->red[k]<=3*DYE_SCALE);
            assert(d->blue[k]>=0 && d->blue[k]<=3*DYE_SCALE);
            assert(d->gold[k]>=0 && d->gold[k]<=3*DYE_SCALE);
            if(sign<0) assert(!d->red[k] && !d->blue[k] && !d->gold[k]);
        }
        assert(moving>0);
    }
    puts("PASS: sustained four-player jets/suction have expected directions, bounded fields and no suction pigment");
}
