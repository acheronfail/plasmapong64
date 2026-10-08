#ifndef FLUID_FORCE_FIXED_H
#define FLUID_FORCE_FIXED_H
#include <assert.h>
#include "fluid_force_table.inc"

/* Experimental force boundary: quantize positions to 1/16 pixel, then use
   only 32-bit integer arithmetic inside the affected-cell loops. Radius <=64
   keeps squared distances, reciprocal weights and products in range. */
static int force_quantize(float v,int scale) {
    v*=scale;
    return (int)(v+(v<0?-.5f:.5f));
}
static int force_round_shift(int v,unsigned bits) {
    int half=1<<(bits-1);
    return v<0?-((-v+half)>>bits):(v+half)>>bits;
}
static void force_integer_add(int16_t *cell,int delta) {
    int value=*cell+delta;
    const int limit=420*VELOCITY_SCALE;
    *cell=(int16_t)(value>limit?limit:value<-limit?-limit:value);
}
static void force_integer_ink(const SplatInk *ink,int k,int a,int b,int w) {
    int value=ink->a[k]+((a*w+16384)>>15);
    ink->a[k]=(int16_t)(value>ink->ca?ink->ca:value<0?0:value);
    if(ink->b) {
        value=ink->b[k]+((b*w+16384)>>15);
        ink->b[k]=(int16_t)(value>ink->cb?ink->cb:value<0?0:value);
    }
}
typedef struct { int x,y,r2,inverse,x0,x1,y0,y1; } ForceFootprint;
static ForceFootprint force_footprint(float x,float y,float radius) {
    assert(radius>=1 && radius<=64);
    int r=force_quantize(radius,16);
    return (ForceFootprint){force_quantize(x,16),force_quantize(y,16),r*r,
        (1<<30)/(r*r),
        (int)clampf((x-radius)/CELL,0,FW-1),(int)clampf((x+radius)/CELL,0,FW-1),
        (int)clampf((y-radius)/CELL,0,FH-1),(int)clampf((y+radius)/CELL,0,FH-1)};
}
static int force_weight(const ForceFootprint *p,int d2) {
    return d2>=p->r2?0:32768-((d2*p->inverse)>>15);
}
static void fluid_force_splat(Fluid *f,float x,float y,float radius,float u,float v,float dye,int player) {
    FluidFlow *velocity=fluid_velocity(f);
    SplatInk ink=splat_ink_plan(fluid_dye(f),player);
    ForceFootprint p=force_footprint(x,y,radius);
    int uq=force_quantize(u,16),vq=force_quantize(v,16);
    int a=force_quantize(dye*ink.wa,DYE_SCALE),b=force_quantize(dye*ink.wb,DYE_SCALE);
    for(int iy=p.y0;iy<=p.y1;iy++) for(int ix=p.x0;ix<=p.x1;ix++) {
        int dx=(ix*PLASMAPONG_CELL_Q4+PLASMAPONG_CELL_Q4/2)-p.x,dy=(iy*PLASMAPONG_CELL_Q4+PLASMAPONG_CELL_Q4/2)-p.y;
        int w=force_weight(&p,dx*dx+dy*dy);
        w=(w*w+16384)>>15;
        int k=iy*FW+ix;
        force_integer_add(&velocity->u[k],force_round_shift(uq*w,15));
        force_integer_add(&velocity->v[k],force_round_shift(vq*w,15));
        force_integer_ink(&ink,k,a,b,w);
    }
}
static void fluid_force_pump(Fluid *f,float x,float y,float radius,float strength,float dt,int player) {
    FluidFlow *velocity=fluid_velocity(f);
    SplatInk ink=splat_ink_plan(fluid_dye(f),player);
    ForceFootprint p=force_footprint(x,y,radius);
    int amplitude=force_quantize(strength*dt,16);
    /* The common gameplay suction has no pigment and a fixed 35-pixel
       radius. Fold falloff into the softened radial direction lookup. Keep
       Q4 velocity and subpixel positions; other pump shapes use the general
       path below. Reordered rounding changes small increments by ~1 Q4 unit. */
    if(radius==35 && strength<0) {
        for(int iy=p.y0;iy<=p.y1;iy++) for(int ix=p.x0;ix<=p.x1;ix++) {
            int dx=ix*PLASMAPONG_CELL_Q4+PLASMAPONG_CELL_Q4/2-p.x;
            int dy=iy*PLASMAPONG_CELL_Q4+PLASMAPONG_CELL_Q4/2-p.y;
            int d2=dx*dx+dy*dy;
            if(d2>=p.r2) continue;
            int index,fraction,bits;
            if(d2<64*256) { index=d2>>8; fraction=d2&255; bits=8; }
            else { index=64+((d2-64*256)>>10); fraction=d2&1023; bits=10; }
            int gain=force_suction35_inverse[index];
            gain+=((force_suction35_inverse[index+1]-gain)*fraction+(1<<(bits-1)))>>bits;
            int nx=force_round_shift(dx*gain,6),ny=force_round_shift(dy*gain,6);
            int k=iy*FW+ix;
            force_integer_add(&velocity->u[k],force_round_shift(nx*amplitude,14));
            force_integer_add(&velocity->v[k],force_round_shift(ny*amplitude,14));
        }
        return;
    }
    int a=force_quantize(strength>0?strength*dt*.0015f*ink.wa:0,DYE_SCALE);
    int b=force_quantize(strength>0?strength*dt*.0015f*ink.wb:0,DYE_SCALE);
    for(int iy=p.y0;iy<=p.y1;iy++) for(int ix=p.x0;ix<=p.x1;ix++) {
        int dx=(ix*PLASMAPONG_CELL_Q4+PLASMAPONG_CELL_Q4/2)-p.x,dy=(iy*PLASMAPONG_CELL_Q4+PLASMAPONG_CELL_Q4/2)-p.y,d2=dx*dx+dy*dy;
        if(d2>=p.r2) continue;
        int w=force_weight(&p,d2),index,fraction,bits;
        if(d2<64*256) { index=d2>>8; fraction=d2&255; bits=8; }
        else { index=64+((d2-64*256)>>10); fraction=d2&1023; bits=10; }
        int inverse=force_inverse_length[index];
        inverse+=((force_inverse_length[index+1]-inverse)*fraction+(1<<(bits-1)))>>bits;
        /* dx is Q4; inverse is Q16. Convert direction to Q14 before
           multiplying by the Q4 force amplitude, avoiding 64-bit products. */
        int nx=force_round_shift(dx*inverse,6),ny=force_round_shift(dy*inverse,6);
        int amount=force_round_shift(amplitude*w,15),k=iy*FW+ix;
        force_integer_add(&velocity->u[k],force_round_shift(nx*amount,14));
        force_integer_add(&velocity->v[k],force_round_shift(ny*amount,14));
        if(strength>0) force_integer_ink(&ink,k,a,b,w);
    }
}
#endif
