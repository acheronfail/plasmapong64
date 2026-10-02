#include "mathutil.h"
#include "fluid.h"
#include <math.h>
#include <string.h>
static void sample_pair(const float *a,const float *b,float x,float y,float *va,float *vb) {
    x=clampf(x,0,FW-1.001f); y=clampf(y,0,FH-1.001f);
    int ix=(int)x,iy=(int)y,k=iy*FW+ix;
    float tx=x-ix,ty=y-iy;
    float w0=(1-tx)*(1-ty),w1=tx*(1-ty),w2=(1-tx)*ty,w3=tx*ty;
    *va=a[k]*w0+a[k+1]*w1+a[k+FW]*w2+a[k+FW+1]*w3;
    *vb=b[k]*w0+b[k+1]*w1+b[k+FW]*w2+b[k+FW+1]*w3;
}
void fluid_init(Fluid *f) { memset(f,0,sizeof(*f)); }
void fluid_sample(const Fluid *f,float x,float y,float *u,float *v) {
    sample_pair(f->u,f->v,x/CELL-.5f,y/CELL-.5f,u,v);
}
static void walls(Fluid *f) {
    for(int y=0;y<FH;y++) f->u[y*FW]=f->u[y*FW+FW-1]=0;
    for(int x=0;x<FW;x++) f->v[x]=f->v[(FH-1)*FW+x]=0;
}
void fluid_project(Fluid *f) {
    walls(f);
    memset(f->pressure,0,sizeof(f->pressure));
    for(int y=0;y<FH;y++) for(int x=0;x<FW;x++) {
        int k=y*FW+x;
        float l=x?f->u[k-1]:-f->u[k], r=x<FW-1?f->u[k+1]:-f->u[k];
        float t=y?f->v[k-FW]:-f->v[k], b=y<FH-1?f->v[k+FW]:-f->v[k];
        f->divergence[k]=-.5f*CELL*(r-l+b-t);
    }
    /* Solve the branch-free interior, then copy Neumann boundary pressure.
       Keep the hot loop small enough for the R4300's instruction cache. */
    for(int pass=0;pass<8;pass++) {
        for(int y=1;y<FH-1;y++) for(int x=1;x<FW-1;x++) {
            int k=y*FW+x;
            f->pressure[k]=(f->divergence[k]+f->pressure[k-1]+f->pressure[k+1]+
                            f->pressure[k-FW]+f->pressure[k+FW])*.25f;
        }
        for(int y=1;y<FH-1;y++) {
            f->pressure[y*FW]=f->pressure[y*FW+1];
            f->pressure[y*FW+FW-1]=f->pressure[y*FW+FW-2];
        }
        for(int x=0;x<FW;x++) {
            f->pressure[x]=f->pressure[FW+x];
            f->pressure[(FH-1)*FW+x]=f->pressure[(FH-2)*FW+x];
        }
    }
    for(int y=0;y<FH;y++) for(int x=0;x<FW;x++) {
        int k=y*FW+x;
        f->u[k]-=(f->pressure[x<FW-1?k+1:k]-f->pressure[x?k-1:k])*(.5f/CELL);
        f->v[k]-=(f->pressure[y<FH-1?k+FW:k]-f->pressure[y?k-FW:k])*(.5f/CELL);
    }
    walls(f);
}
void fluid_splat(Fluid *f,float x,float y,float radius,float u,float v,float dye,int player) {
    int x0=(int)clampf((x-radius)/CELL,0,FW-1),x1=(int)clampf((x+radius)/CELL,0,FW-1);
    int y0=(int)clampf((y-radius)/CELL,0,FH-1),y1=(int)clampf((y+radius)/CELL,0,FH-1);
    for(int iy=y0;iy<=y1;iy++) for(int ix=x0;ix<=x1;ix++) {
        float dx=(ix+.5f)*CELL-x,dy=(iy+.5f)*CELL-y;
        float w=maxf(0,1-(dx*dx+dy*dy)/(radius*radius)); w*=w;
        int k=iy*FW+ix;
        f->u[k]=clampf(f->u[k]+u*w,-420,420); f->v[k]=clampf(f->v[k]+v*w,-420,420);
        float *ink=player?f->red:f->blue;
        ink[k]=minf(3,ink[k]+dye*w);
    }
}
void fluid_pump(Fluid *f,float x,float y,float radius,float strength,float dt,int player) {
    /* A pump is an intentional local source/sink. Apply after projection, so
       pressure does not immediately cancel suction; next step redistributes it. */
    int x0=(int)clampf((x-radius)/CELL,0,FW-1),x1=(int)clampf((x+radius)/CELL,0,FW-1);
    int y0=(int)clampf((y-radius)/CELL,0,FH-1),y1=(int)clampf((y+radius)/CELL,0,FH-1);
    for(int iy=y0;iy<=y1;iy++) for(int ix=x0;ix<=x1;ix++) {
        float dx=(ix+.5f)*CELL-x,dy=(iy+.5f)*CELL-y,d2=dx*dx+dy*dy;
        if(d2>=radius*radius) continue;
        float d=sqrtf(d2+9),w=1-d2/(radius*radius);
        int k=iy*FW+ix;
        f->u[k]=clampf(f->u[k]+dx/d*strength*w*dt,-420,420);
        f->v[k]=clampf(f->v[k]+dy/d*strength*w*dt,-420,420);
        float *ink=player?f->red:f->blue;
        if(strength>0) ink[k]=minf(3,ink[k]+strength*dt*.0015f*w);
    }
}
void fluid_velocity_step(Fluid *f,float dt) {
    /* Semi-Lagrangian advection: bounded even during a strong jet. */
    for(int y=0;y<FH;y++) for(int x=0;x<FW;x++) {
        int k=y*FW+x;
        float px=x-dt*f->u[k]/CELL,py=y-dt*f->v[k]/CELL;
        sample_pair(f->u,f->v,px,py,&f->tu[k],&f->tv[k]);
        f->tu[k]*=1-.16f*dt; f->tv[k]*=1-.16f*dt;
    }
    memcpy(f->u,f->tu,sizeof(f->u)); memcpy(f->v,f->tv,sizeof(f->v));
    /* Curl confinement returns small vortices lost to coarse-grid advection. */
    for(int y=1;y<FH-1;y++) for(int x=1;x<FW-1;x++) {
        int k=y*FW+x;
        f->tr[k]=(f->v[k+1]-f->v[k-1]-f->u[k+FW]+f->u[k-FW])*.5f/CELL;
    }
    for(int y=2;y<FH-2;y++) for(int x=2;x<FW-2;x++) {
        int k=y*FW+x;
        float nx=fabsf(f->tr[k+1])-fabsf(f->tr[k-1]);
        float ny=fabsf(f->tr[k+FW])-fabsf(f->tr[k-FW]);
        float inv=1/sqrtf(nx*nx+ny*ny+.00001f);
        f->u[k]+=ny*inv*f->tr[k]*CELL*1.1f*dt;
        f->v[k]-=nx*inv*f->tr[k]*CELL*1.1f*dt;
    }
    fluid_project(f);
}
void fluid_dye_step(Fluid *f,float dt) {
    for(int y=0;y<FH;y++) for(int x=0;x<FW;x++) {
        int k=y*FW+x;
        float px=x-dt*f->u[k]/CELL,py=y-dt*f->v[k]/CELL;
        px=clampf(px,0,FW-1.001f); py=clampf(py,0,FH-1.001f);
        int ix=(int)px,iy=(int)py,j=iy*FW+ix;
        float tx=px-ix,ty=py-iy;
        float w0=(1-tx)*(1-ty),w1=tx*(1-ty),w2=(1-tx)*ty,w3=tx*ty;
        f->tr[k]=(f->red[j]*w0+f->red[j+1]*w1+f->red[j+FW]*w2+f->red[j+FW+1]*w3)*(1-.22f*dt);
        f->tb[k]=(f->blue[j]*w0+f->blue[j+1]*w1+f->blue[j+FW]*w2+f->blue[j+FW+1]*w3)*(1-.22f*dt);
        /* Velocity scratch is free now; reuse it for the third dye channel. */
        f->tu[k]=(f->gold[j]*w0+f->gold[j+1]*w1+f->gold[j+FW]*w2+f->gold[j+FW+1]*w3)*(1-1.1f*dt);
    }
    memcpy(f->red,f->tr,sizeof(f->red)); memcpy(f->blue,f->tb,sizeof(f->blue));
    memcpy(f->gold,f->tu,sizeof(f->gold));
}
void fluid_ball_dye(Fluid *f,float x,float y,float amount) {
    const float radius=8;
    int x0=(int)clampf((x-radius)/CELL,0,FW-1),x1=(int)clampf((x+radius)/CELL,0,FW-1);
    int y0=(int)clampf((y-radius)/CELL,0,FH-1),y1=(int)clampf((y+radius)/CELL,0,FH-1);
    for(int iy=y0;iy<=y1;iy++) for(int ix=x0;ix<=x1;ix++) {
        float dx=(ix+.5f)*CELL-x,dy=(iy+.5f)*CELL-y;
        float w=maxf(0,1-(dx*dx+dy*dy)/(radius*radius));
        int k=iy*FW+ix;
        f->gold[k]=minf(.65f,f->gold[k]+amount*w*w);
    }
}
uint32_t fluid_color(const Fluid *f,int k) {
    float r=f->red[k],b=f->blue[k],g=f->gold[k];
    /* Cyan and coral currents mix with a small, faster-fading gold ball trail. */
    int red=(int)clampf(5+210*r+20*b+255*g,0,255);
    int green=(int)clampf(9+64*r+155*b+205*g,0,255);
    int blue=(int)clampf(22+92*r+225*b+25*g,0,255);
    return (uint32_t)((red<<16)|(green<<8)|blue);
}
