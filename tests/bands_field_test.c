#include "fluid.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static Fluid f,before;
static uint32_t pixels[FH*(FW+4)];
int main(void) {
    fluid_init(&f);
    FluidFlow *v=fluid_velocity(&f);
    for(int k=0;k<FN;k++) {
        v->u[k]=(k*137%32767)-16383;
        v->v[k]=(k*593%32767)-16383;
    }
    before=f;
    for(unsigned k=0;k<sizeof(pixels)/sizeof(*pixels);k++) pixels[k]=0xdeadbeef;
    fluid_speed_field_pixels(&f,pixels,FW+4);
    for(int y=0;y<FH;y++) {
        for(int x=0;x<FW;x++) {
            int k=y*FW+x;
            int u=v->u[k]<0?-v->u[k]:v->u[k],w=v->v[k]<0?-v->v[k]:v->v[k];
            double a=u/(double)VELOCITY_SCALE,b=w/(double)VELOCITY_SCALE;
            unsigned speed=(unsigned)(a>b?a+.5*b:b+.5*a);
            if(speed>255) speed=255;
            assert(pixels[y*(FW+4)+x]==((speed*0x01010100u)|255));
        }
        for(int x=FW;x<FW+4;x++) assert(pixels[y*(FW+4)+x]==0xdeadbeef);
    }
    assert(!memcmp(&f,&before,sizeof(f)));
    /* Equal endpoint colours must not erase the ribbon between speeds 0/64. */
    fluid_init(&f);
    v=fluid_velocity(&f); v->u[1]=fluid_flow_encode(64);
    assert(fluid_view_color(&f,0,FLUID_VIEW_BANDS)==fluid_view_color(&f,1,FLUID_VIEW_BANDS));
    fluid_speed_field_pixels(&f,pixels,FW+4);
    assert(pixels[0]==0x000000ff && pixels[1]==0x404040ff);
    v->u[2]=fluid_flow_encode(32);
    assert(fluid_view_color(&f,2,FLUID_VIEW_BANDS)!=fluid_view_color(&f,0,FLUID_VIEW_BANDS));
    puts("PASS: scalar speed field, fractional velocity rounding, saturation, stride, immutable state and pre-palette ribbon preservation");
}
