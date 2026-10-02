#include "fluid.h"
#include "fluid_reference.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static Fluid f,copy;
static ReferenceFluid ref;
static uint32_t rng=12345;
static float random_unit(void) {
    rng=rng*1664525u+1013904223u;
    return (rng>>8)*(1.0f/16777216.0f);
}
static float max_velocity_error,max_dye_error;
static void compare(void) {
    const FluidVelocity *v=fluid_velocity(&f);
    const FluidInk *d=fluid_dye(&f);
    for(int k=0;k<FN;k++) {
        float error=fmaxf(fabsf(v->u[k]-ref.u[k]),fabsf(v->v[k]-ref.v[k]));
        max_velocity_error=fmaxf(max_velocity_error,error);
        assert(error<.005f);
        error=fmaxf(fabsf(fluid_ink_decode(d->red[k])-ref.red[k]),fmaxf(fabsf(fluid_ink_decode(d->blue[k])-ref.blue[k]),fabsf(fluid_ink_decode(d->gold[k])-ref.gold[k])));
        max_dye_error=fmaxf(max_dye_error,error);
#ifdef PLASMAPONG_DYE_FIXED
        /* Four rounded fixed-point steps plus the initial Q13 conversion. */
        assert(error<.002f);
#else
        assert(error<.00003f);
#endif
    }
}
int main(void) {
    _Static_assert(_Alignof(Fluid)>=16,"Fluid alignment");
    _Static_assert(FW*sizeof(float)%16==0,"row alignment");
    for(int bank=0;bank<2;bank++) {
        assert(((uintptr_t)f.velocity[bank].u&15)==0);
        assert(((uintptr_t)f.velocity[bank].v&15)==0);
        assert(((uintptr_t)f.dye[bank].red&15)==0);
        assert(((uintptr_t)f.dye[bank].blue&15)==0);
        assert(((uintptr_t)f.dye[bank].gold&15)==0);
    }
    for(int trial=0;trial<32;trial++) {
        fluid_init(&f); reference_fluid_init(&ref);
        for(int k=0;k<FN;k++) {
            fluid_velocity(&f)->u[k]=ref.u[k]=(random_unit()*2-1)*420;
            fluid_velocity(&f)->v[k]=ref.v[k]=(random_unit()*2-1)*420;
            fluid_dye(&f)->red[k]=fluid_ink_encode(ref.red[k]=random_unit()*3);
            fluid_dye(&f)->blue[k]=fluid_ink_encode(ref.blue[k]=random_unit()*3);
            fluid_dye(&f)->gold[k]=fluid_ink_encode(ref.gold[k]=random_unit()*.65f);
        }
        /* Include zero dt, boundary-crossing backtraces, and both bank parities. */
        float dt=trial%4==0?0:1.0f/30;
        for(int step=0;step<4;step++) {
            fluid_velocity_step(&f,dt); reference_fluid_velocity_step(&ref,dt);
            fluid_pump(&f,20,90,35,-1150,dt,0);
            reference_fluid_pump(&ref,20,90,35,-1150,dt,0);
            fluid_dye_step(&f,dt); reference_fluid_dye_step(&ref,dt);
            compare();
            /* A value copy must retain bank selection and own its buffers. */
            copy=f;
            fluid_velocity_step(&copy,dt); fluid_dye_step(&copy,dt);
            compare();
            assert(fluid_velocity(&copy)!=fluid_velocity(&f));
        }
    }
    /* Extreme finite input exercises the fixed-point saturation/overflow bound. */
    for(int k=0;k<FN;k++) {
        fluid_velocity(&f)->u[k]=(k%3-1)*100000.0f;
        fluid_velocity(&f)->v[k]=(k%5-2)*100000.0f;
    }
    fluid_project(&f);
    for(int k=0;k<FN;k++) {
        assert(isfinite(fluid_velocity(&f)->u[k]));
        assert(isfinite(fluid_velocity(&f)->v[k]));
        assert(llabs((long long)f.pressure[k])<INT32_MAX/2);
    }
    /* RGBA output must match scalar conversion and preserve padded columns. */
    uint32_t pixels[FH*64];
    for(int k=0;k<FH*64;k++) pixels[k]=0x12345678;
    fluid_pixels(&f,pixels,64);
    for(int y=0;y<FH;y++) for(int x=0;x<64;x++)
        assert(pixels[y*64+x]==(x<FW?(fluid_color(&f,y*FW+x)<<8)|255:0x12345678));
    fluid_speed_pixels(&f,pixels,64);
    for(int y=0;y<FH;y++) for(int x=0;x<64;x++)
        assert(pixels[y*64+x]==(x<FW?(fluid_speed_color(&f,y*FW+x)<<8)|255:0x12345678));
    /* Integer texture generation must agree with float color conversion to
       within one display level when fed the same decoded dye values. */
    for(int k=0;k<FN;k++) {
        ref.red[k]=fluid_ink_decode(fluid_dye(&f)->red[k]);
        ref.blue[k]=fluid_ink_decode(fluid_dye(&f)->blue[k]);
        ref.gold[k]=fluid_ink_decode(fluid_dye(&f)->gold[k]);
        uint32_t a=fluid_color(&f,k),b=reference_fluid_color(&ref,k);
        for(int shift=0;shift<=16;shift+=8)
            assert(abs((int)((a>>shift)&255)-(int)((b>>shift)&255))<=1);
    }
    printf("PASS: aligned banks, value copies, reference solver (max velocity error %.8g, dye %.8g)\n",max_velocity_error,max_dye_error);
}
