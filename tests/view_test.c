#include "fluid.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static Fluid f,before;
static uint32_t pixels[FH*(FW+4)];
int main(void) {
    fluid_init(&f);
    int k=16*FW+24;
    uint32_t neutral=fluid_view_color(&f,k,FLUID_VIEW_VORTEX);
    fluid_velocity(&f)->v[k+1]=fluid_flow_encode(32);
    fluid_velocity(&f)->v[k-1]=fluid_flow_encode(-32);
    uint32_t clockwise=fluid_view_color(&f,k,FLUID_VIEW_VORTEX);
    assert(clockwise!=neutral && ((clockwise>>16)&255)>(clockwise&255));
    fluid_velocity(&f)->v[k+1]=fluid_flow_encode(-32);
    fluid_velocity(&f)->v[k-1]=fluid_flow_encode(32);
    uint32_t counter=fluid_view_color(&f,k,FLUID_VIEW_VORTEX);
    assert(counter!=neutral && (counter&255)>((counter>>16)&255));
    /* Scratch must not influence VORTEX; it belongs to divergence by now. */
    memset(f.divergence,0x7f,sizeof(f.divergence));
    assert(fluid_view_color(&f,k,FLUID_VIEW_VORTEX)==counter);
    f.pressure[k]=64*32768;
    uint32_t positive=fluid_view_color(&f,k,FLUID_VIEW_PRESSURE);
    f.pressure[k]=-64*32768;
    uint32_t negative=fluid_view_color(&f,k,FLUID_VIEW_PRESSURE);
    assert(positive!=negative && ((positive>>16)&255)>(positive&255) && (negative&255)>((negative>>16)&255));
    /* Verify all cell edges, strides, opacity and immutable simulation state. */
    for(int i=0;i<FN;i++) {
        fluid_dye(&f)->red[i]=fluid_ink_encode((i%7)*.2f);
        fluid_dye(&f)->blue[i]=fluid_ink_encode((i%9)*.2f);
        fluid_dye(&f)->gold[i]=fluid_ink_encode((i%3)*.1f);
    }
    before=f;
    for(FluidView view=FLUID_VIEW_DYE;view<=FLUID_VIEW_PRESSURE;view++) {
        for(unsigned i=0;i<sizeof(pixels)/sizeof(*pixels);i++) pixels[i]=0xdeadbeef;
        fluid_view_pixels(&f,pixels,FW+4,view);
        for(int y=0;y<FH;y++) {
            for(int x=0;x<FW;x++) assert((pixels[y*(FW+4)+x]&255)==255);
            for(int x=FW;x<FW+4;x++) assert(pixels[y*(FW+4)+x]==0xdeadbeef);
        }
        assert(!memcmp(&f,&before,sizeof(f)));
    }
    /* A linear velocity ramp has known curl, including clamped border stencils. */
    for(int sign=-1;sign<=1;sign+=2) {
        for(int y=0;y<FH;y++) for(int x=0;x<FW;x++) {
            fluid_velocity(&f)->u[y*FW+x]=-sign*y*16;
            fluid_velocity(&f)->v[y*FW+x]=sign*x*16;
        }
        before=f;
        for(unsigned i=0;i<sizeof(pixels)/sizeof(*pixels);i++) pixels[i]=0xdeadbeef;
        fluid_vortex_field_pixels(&f,pixels,FW+4);
        for(int y=0;y<FH;y++) {
            for(int x=0;x<FW;x++) {
                unsigned index=128+sign*((x==0 || x==FW-1?1:2)+(y==0 || y==FH-1?1:2));
                assert(pixels[y*(FW+4)+x]==((index*0x01010100u)|255));
            }
            for(int x=FW;x<FW+4;x++) assert(pixels[y*(FW+4)+x]==0xdeadbeef);
        }
        assert(!memcmp(&f,&before,sizeof(f)));
    }
    fluid_velocity(&f)->v[k+1]=16383;
    fluid_velocity(&f)->v[k-1]=-16383;
    fluid_vortex_field_pixels(&f,pixels,FW+4);
    assert(pixels[16*(FW+4)+24]==0xffffffff);
    fluid_velocity(&f)->v[k+1]=-16383;
    fluid_velocity(&f)->v[k-1]=16383;
    fluid_vortex_field_pixels(&f,pixels,FW+4);
    assert(pixels[16*(FW+4)+24]==0x000000ff);
    puts("PASS: vortex rotation/scratch independence, signed pressure, visual source integrity, texture edges/stride/opacity");
}
