#include "mathutil.h"
#include "fluid.h"
#include "fluid_profile.h"
#include "fluid_pressure.h"
#include "fluid_advection.h"
#ifdef PLASMAPONG_DYE_FIXED
#include "fluid_dye_fixed.h"
#endif
#ifdef PLASMAPONG_FLUID_PROFILE
FluidProfile fluid_profile;
#endif
#include <math.h>
#include <string.h>
_Static_assert((FW-2)%2==0,"pressure loop handles two interior cells at a time");
_Static_assert((-1>>1)==-1,"fixed-point pressure requires arithmetic right shift");
/* Q12 pressure: max |divergence| <= 32760 * 4096. Starting from zero,
   each lexicographic pass increases max |pressure| by at most D/2 plus
   rounding. Eight passes and their pre-shift sums stay inside int32_t.
   The clamp is far outside ordinary play (even +/-1000 velocity gives
   at most 12000 divergence). Velocity and advection remain floating point. */
#define PRESSURE_SCALE 4096
static inline void ink_add(FluidInkValue *ink,float amount,float limit) {
#ifdef PLASMAPONG_DYE_FIXED
    int value=*ink+(int)(amount*DYE_SCALE+.5f);
    int ceiling=(int)(limit*DYE_SCALE+.5f);
    *ink=(int16_t)(value>ceiling?ceiling:value<0?0:value);
#else
    *ink=minf(limit,*ink+amount);
#endif
}
/* Separable interpolation shares coordinates across channels and needs only
   three multiplies per channel. */
static inline float bilerp(const float *a,int k,float tx,float ty) {
    float top=a[k]+tx*(a[k+1]-a[k]);
    float bottom=a[k+FW]+tx*(a[k+FW+1]-a[k+FW]);
    return top+ty*(bottom-top);
}
static void sample_pair(const float *a,const float *b,float x,float y,float *va,float *vb) {
    x=clampf(x,0,FW-1.001f); y=clampf(y,0,FH-1.001f);
    int ix=(int)x,iy=(int)y,k=iy*FW+ix;
    float tx=x-ix,ty=y-iy;
    *va=bilerp(a,k,tx,ty);
    *vb=bilerp(b,k,tx,ty);
}
void fluid_init(Fluid *f) { memset(f,0,sizeof(*f)); }
void fluid_sample(const Fluid *f,float x,float y,float *u,float *v) {
    const FluidVelocity *velocity=fluid_velocity(f);
    PROFILE_BEGIN();
    sample_pair(velocity->u,velocity->v,x/CELL-.5f,y/CELL-.5f,u,v);
    PROFILE_END(PROFILE_SAMPLE);
}
static void walls(Fluid *f) {
    FluidVelocity *velocity=fluid_velocity(f);
    for(int y=0;y<FH;y++) velocity->u[y*FW]=velocity->u[y*FW+FW-1]=0;
    for(int x=0;x<FW;x++) velocity->v[x]=velocity->v[(FH-1)*FW+x]=0;
}
void fluid_pressure_cpu(int32_t *restrict pressure,const int32_t *restrict divergence) {
    /* Solve the branch-free interior, then copy Neumann boundary pressure.
       Keep the hot loop small enough for the R4300's instruction cache. */
    memset(pressure,0,FW*sizeof(pressure[0]));
    for(int pass=0;pass<8;pass++) {
        if(pass==0) {
            /* Zero initial pressure makes the right/bottom neighbors zero.
               Fill every interior cell without clearing the whole grid. */
            for(int y=1;y<FH-1;y++) {
                int32_t left=0;
                for(int x=1;x<FW-1;x++) {
                    int k=y*FW+x;
                    pressure[k]=left=(divergence[k]+left+pressure[k-FW])>>2;
                }
            }
        } else for(int y=1;y<FH-1;y++) {
            int k=y*FW+1;
            int32_t left=pressure[k-1];
            for(int x=1;x<FW-1;x+=2,k+=2) {
                /* Prepare independent neighbor sums before the serial left
                   dependency. Keep the same lexicographic eight-pass solve. */
                int32_t a=(divergence[k]+pressure[k+1])+
                        (pressure[k-FW]+pressure[k+FW]);
                int32_t b=(divergence[k+1]+pressure[k+2])+
                        (pressure[k-FW+1]+pressure[k+FW+1]);
                pressure[k]=left=(a+left)>>2;
                pressure[k+1]=left=(b+left)>>2;
            }
        }
        for(int y=1;y<FH-1;y++) {
            pressure[y*FW]=pressure[y*FW+1];
            pressure[y*FW+FW-1]=pressure[y*FW+FW-2];
        }
        /* Both row starts are 16-byte aligned. Give GCC the alignment at
           the copy site to avoid MIPS unaligned load/store pairs. */
        memcpy(__builtin_assume_aligned(pressure,16),
               __builtin_assume_aligned(pressure+FW,16),FW*sizeof(pressure[0]));
        memcpy(__builtin_assume_aligned(pressure+(FH-1)*FW,16),
               __builtin_assume_aligned(pressure+(FH-2)*FW,16),FW*sizeof(pressure[0]));
    }
}
void fluid_project(Fluid *f) {
    FluidVelocity *velocity=fluid_velocity(f);
    PROFILE_BEGIN();
    walls(f);
    /* Only interior divergence is consumed by the Neumann pressure solve. */
    for(int y=1;y<FH-1;y++) for(int x=1;x<FW-1;x++) {
        int k=y*FW+x;
        float divergence=-.5f*CELL*(velocity->u[k+1]-velocity->u[k-1]+velocity->v[k+FW]-velocity->v[k-FW]);
        f->divergence[k]=(int32_t)(clampf(divergence,-32760,32760)*PRESSURE_SCALE);
    }
    PROFILE_END(PROFILE_DIVERGENCE);
#ifdef PLASMAPONG_FLUID_RSP
    fluid_pressure_rsp(f->pressure,f->divergence);
#else
    fluid_pressure_cpu(f->pressure,f->divergence);
#endif
    PROFILE_END(PROFILE_PRESSURE);
    for(int y=1;y<FH-1;y++) for(int x=1;x<FW-1;x++) {
        int k=y*FW+x;
        velocity->u[k]-=(f->pressure[k+1]-f->pressure[k-1])*(.5f/(CELL*PRESSURE_SCALE));
        velocity->v[k]-=(f->pressure[k+FW]-f->pressure[k-FW])*(.5f/(CELL*PRESSURE_SCALE));
    }
    /* Tangential edge velocity survives; wall-normal velocity is zero. */
    for(int x=1;x<FW-1;x++) {
        velocity->u[x]-=(f->pressure[x+1]-f->pressure[x-1])*(.5f/(CELL*PRESSURE_SCALE));
        int k=(FH-1)*FW+x;
        velocity->u[k]-=(f->pressure[k+1]-f->pressure[k-1])*(.5f/(CELL*PRESSURE_SCALE));
    }
    for(int y=1;y<FH-1;y++) {
        int k=y*FW;
        velocity->v[k]-=(f->pressure[k+FW]-f->pressure[k-FW])*(.5f/(CELL*PRESSURE_SCALE));
        k+=FW-1;
        velocity->v[k]-=(f->pressure[k+FW]-f->pressure[k-FW])*(.5f/(CELL*PRESSURE_SCALE));
    }
    walls(f);
    PROFILE_END(PROFILE_GRADIENT);
}
void fluid_splat(Fluid *f,float x,float y,float radius,float u,float v,float dye,int player) {
    FluidVelocity *velocity=fluid_velocity(f);
    FluidInk *ink_grid=fluid_dye(f);
    PROFILE_BEGIN();
    const float inv_radius2=1/(radius*radius);
    int x0=(int)clampf((x-radius)/CELL,0,FW-1),x1=(int)clampf((x+radius)/CELL,0,FW-1);
    int y0=(int)clampf((y-radius)/CELL,0,FH-1),y1=(int)clampf((y+radius)/CELL,0,FH-1);
    for(int iy=y0;iy<=y1;iy++) for(int ix=x0;ix<=x1;ix++) {
        float dx=(ix+.5f)*CELL-x,dy=(iy+.5f)*CELL-y;
        float w=maxf(0,1-(dx*dx+dy*dy)*inv_radius2); w*=w;
        int k=iy*FW+ix;
        velocity->u[k]=clampf(velocity->u[k]+u*w,-420,420); velocity->v[k]=clampf(velocity->v[k]+v*w,-420,420);
        FluidInkValue *ink=player?ink_grid->red:ink_grid->blue;
        ink_add(&ink[k],dye*w,3);
    }
    PROFILE_END(PROFILE_SPLAT);
}
void fluid_pump(Fluid *f,float x,float y,float radius,float strength,float dt,int player) {
    FluidVelocity *velocity=fluid_velocity(f);
    FluidInk *ink_grid=fluid_dye(f);
    PROFILE_BEGIN();
    const float inv_radius2=1/(radius*radius);
    /* A pump is an intentional local source/sink. Apply after projection, so
       pressure does not immediately cancel suction; next step redistributes it. */
    int x0=(int)clampf((x-radius)/CELL,0,FW-1),x1=(int)clampf((x+radius)/CELL,0,FW-1);
    int y0=(int)clampf((y-radius)/CELL,0,FH-1),y1=(int)clampf((y+radius)/CELL,0,FH-1);
    for(int iy=y0;iy<=y1;iy++) for(int ix=x0;ix<=x1;ix++) {
        float dx=(ix+.5f)*CELL-x,dy=(iy+.5f)*CELL-y,d2=dx*dx+dy*dy;
        if(d2>=radius*radius) continue;
        float w=1-d2*inv_radius2;
        float force=strength*w*dt/sqrtf(d2+9);
        int k=iy*FW+ix;
        velocity->u[k]=clampf(velocity->u[k]+dx*force,-420,420);
        velocity->v[k]=clampf(velocity->v[k]+dy*force,-420,420);
        FluidInkValue *ink=player?ink_grid->red:ink_grid->blue;
        if(strength>0) ink_add(&ink[k],strength*dt*.0015f*w,3);
    }
    PROFILE_END(PROFILE_PUMP);
}
void fluid_velocity_step(Fluid *f,float dt) {
    FluidVelocity *velocity=fluid_velocity(f);
    PROFILE_BEGIN();
    FluidVelocity *next=&f->velocity[f->velocity_bank^1];
    const float grid_dt=dt/CELL,decay=1-.16f*dt,confinement=CELL*1.1f*dt;
    /* Semi-Lagrangian advection: bounded even during a strong jet. */
    fluid_advect_velocity(next,velocity,grid_dt,decay);
    PROFILE_END(PROFILE_VELOCITY_ADVECTION);
    f->velocity_bank^=1; velocity=next;
    PROFILE_END(PROFILE_VELOCITY_SWAP);
    /* Curl confinement returns small vortices lost to coarse-grid advection. */
    for(int y=1;y<FH-1;y++) for(int x=1;x<FW-1;x++) {
        int k=y*FW+x;
        f->curl[k]=(velocity->v[k+1]-velocity->v[k-1]-velocity->u[k+FW]+velocity->u[k-FW])*.5f/CELL;
    }
    PROFILE_END(PROFILE_CURL);
    for(int y=2;y<FH-2;y++) for(int x=2;x<FW-2;x++) {
        int k=y*FW+x;
        float nx=fabsf(f->curl[k+1])-fabsf(f->curl[k-1]);
        float ny=fabsf(f->curl[k+FW])-fabsf(f->curl[k-FW]);
        float inv=1/sqrtf(nx*nx+ny*ny+.00001f);
        velocity->u[k]+=ny*(inv*f->curl[k]*confinement);
        velocity->v[k]-=nx*(inv*f->curl[k]*confinement);
    }
    PROFILE_END(PROFILE_CONFINEMENT);
    fluid_project(f);
}
void fluid_dye_step(Fluid *f,float dt) {
    FluidVelocity *velocity=fluid_velocity(f);
    FluidInk *ink_grid=fluid_dye(f);
    PROFILE_BEGIN();
    FluidInk *next=&f->dye[f->dye_bank^1];
    const float grid_dt=dt/CELL,decay=1-.22f*dt,gold_decay=1-1.1f*dt;
#ifdef PLASMAPONG_DYE_FIXED
    /* Deterministic temporal rounding; state remains safe to copy by value. */
    unsigned rounding=(f->dye_phase+=40503u)&65535u;
#endif
#ifdef PLASMAPONG_DYE_RSP
    fluid_advect_ink_rsp(next,ink_grid,velocity,grid_dt,decay,gold_decay,rounding);
#elif defined(PLASMAPONG_DYE_FIXED)
    fluid_advect_ink_reference(next,ink_grid,velocity,grid_dt,decay,gold_decay,rounding);
#else
    fluid_advect_dye(next,ink_grid,velocity,grid_dt,decay,gold_decay);
#endif
    PROFILE_END(PROFILE_DYE_ADVECTION);
    f->dye_bank^=1;
    PROFILE_END(PROFILE_DYE_SWAP);
}
void fluid_ball_dye(Fluid *f,float x,float y,float amount) {
    FluidInk *ink_grid=fluid_dye(f);
    PROFILE_BEGIN();
    const float radius=8;
    int x0=(int)clampf((x-radius)/CELL,0,FW-1),x1=(int)clampf((x+radius)/CELL,0,FW-1);
    int y0=(int)clampf((y-radius)/CELL,0,FH-1),y1=(int)clampf((y+radius)/CELL,0,FH-1);
    for(int iy=y0;iy<=y1;iy++) for(int ix=x0;ix<=x1;ix++) {
        float dx=(ix+.5f)*CELL-x,dy=(iy+.5f)*CELL-y;
        float w=maxf(0,1-(dx*dx+dy*dy)/(radius*radius));
        int k=iy*FW+ix;
        ink_add(&ink_grid->gold[k],amount*w*w,.65f);
    }
    PROFILE_END(PROFILE_BALL_DYE);
}
static inline uint32_t dye_color(const FluidInk *ink_grid,int k) {
#ifdef PLASMAPONG_DYE_FIXED
    int r=ink_grid->red[k],b=ink_grid->blue[k],g=ink_grid->gold[k];
    int red=(5*DYE_SCALE+210*r+20*b+255*g)>>13;
    int green=(9*DYE_SCALE+64*r+155*b+205*g)>>13;
    int blue=(22*DYE_SCALE+92*r+225*b+25*g)>>13;
    red=red>255?255:red; green=green>255?255:green; blue=blue>255?255:blue;
#else
    float r=ink_grid->red[k],b=ink_grid->blue[k],g=ink_grid->gold[k];
    /* Cyan and coral currents mix with a small, faster-fading gold ball trail. */
    int red=(int)clampf(5+210*r+20*b+255*g,0,255);
    int green=(int)clampf(9+64*r+155*b+205*g,0,255);
    int blue=(int)clampf(22+92*r+225*b+25*g,0,255);
#endif
    return (uint32_t)((red<<16)|(green<<8)|blue);
}
uint32_t fluid_color(const Fluid *f,int k) { return dye_color(fluid_dye(f),k); }
void fluid_pixels(const Fluid *f,uint32_t *pixels,unsigned stride) {
    const FluidInk *ink_grid=fluid_dye(f);
    for(int y=0;y<FH;y++) for(int x=0;x<FW;x++)
        pixels[y*stride+x]=(dye_color(ink_grid,y*FW+x)<<8)|255;
}
