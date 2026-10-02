#include "fluid_confinement.h"
#include <assert.h>
#include <string.h>
#include "rsp_rsqrt_table.inc"
_Static_assert((-1>>1)==-1,"fixed-point confinement requires arithmetic shifts");
_Static_assert(FW==48 && FH==33,"confinement grid layout");
void fluid_curl_fixed(int16_t *curl,const FluidVelocityFixed *v) {
    memset(curl,0,FW*sizeof(*curl));
    memset(curl+(FH-1)*FW,0,FW*sizeof(*curl));
    for(int y=1;y<FH-1;y++) {
        curl[y*FW]=curl[y*FW+FW-1]=0;
        for(int x=1;x<FW-1;x++) {
            int k=y*FW+x;
            int raw=v->v[k+1]-v->v[k-1]+v->u[k-FW]-v->u[k+FW];
            curl[k]=(int16_t)((raw+1)>>1);
        }
    }
}
unsigned fluid_confinement_strength(float dt) {
    assert(CELL==6.0f && dt>=0 && dt<=.25f);
    /* Curl/96 * CELL * 1.1 * dt * VELOCITY_SCALE = curl * 1.1 * dt. */
    return (unsigned)(dt*1.1f*32768+.5f);
}
static int absolute(int v) { return v<0?-v:v; }
static int clamp_velocity(int v) { return v>VELOCITY_LIMIT?VELOCITY_LIMIT:v<-VELOCITY_LIMIT?-VELOCITY_LIMIT:v; }
/* Positive 32-bit VRSQ lookup model: output approximately 2^31/sqrt(n).
   Generated ROM constants and leading-bit normalization reproduce the hardware
   lookup; no floating point or integer division in the per-cell loop. */
static uint32_t inverse_root(uint32_t n) {
    if(!n) return 0x7fffffffu;
    unsigned leading=(unsigned)__builtin_clz(n);
    unsigned index=(((n<<leading)&0x7fc00000u)>>22)&0x1feu;
    index|=leading&1u;
    return ((0x10000u|rsp_rsqrt_table[index])<<14)>>((31-leading)>>1);
}
static int direction(int n,uint32_t inverse) {
    int value=(int)(((int64_t)n*inverse)>>16);
    return value>32767?32767:value<-32768?-32768:value;
}
void fluid_confinement_fixed(FluidVelocityFixed *v,const int16_t *curl,unsigned strength) {
    assert(strength<=9011);
    for(int y=2;y<FH-2;y++) for(int x=2;x<FW-2;x++) {
        int k=y*FW+x;
        int nx=absolute(curl[k+1])-absolute(curl[k-1]);
        int ny=absolute(curl[k+FW])-absolute(curl[k-FW]);
        uint32_t length=(uint32_t)(nx*nx)+(uint32_t)(ny*ny);
        uint32_t inverse=inverse_root(length);
        int dx=direction(nx,inverse),dy=direction(ny,inverse);
        int amplitude=(curl[k]*(int)strength+16384)>>15;
        int du=(dy*amplitude+16384)>>15,dv=(dx*amplitude+16384)>>15;
        v->u[k]=(int16_t)clamp_velocity(v->u[k]+du);
        v->v[k]=(int16_t)clamp_velocity(v->v[k]-dv);
    }
}
