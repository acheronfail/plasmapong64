#include "mathutil.h"
#include "fluid.h"
#include "fluid_profile.h"
#include "fluid_pressure.h"
#include "fluid_upwind.h"
#include <math.h>
#include <string.h>
#include "fluid_velocity_fixed.h"
#include "fluid_dye_fixed.h"
#ifdef PLASMAPONG_FLUID_PROFILE
FluidProfile fluid_profile;
#endif
#ifdef PLASMAPONG_QUEUE_PC_PROFILE
#ifdef PLASMAPONG_RDP_WAIT_TRACE
#include <plasmapong_rdp_trace.h>
static unsigned queue_wait_masks[1024],queue_wait_callers[1024];
#endif
static unsigned queue_samples,queue_halted;
static unsigned queue_dp_busy,queue_dma_busy,queue_end_valid;
static void queue_sample_wait(rspq_syncpoint_t point) {
    uint64_t begin=get_ticks();
    while(!rspq_syncpoint_check(point)) {
        /* Do not halt a running RDP queue: it can disrupt full-sync handling.
           Read only status here; active PC values are not reliable. */
        unsigned sp=*SP_STATUS;
        unsigned dp=*(volatile uint32_t *)0xa410000c;
#ifdef PLASMAPONG_RDP_WAIT_TRACE
        unsigned mask=*(volatile uint32_t *)PLASMAPONG_RDP_WAIT_MASK_ADDRESS;
        unsigned caller=*(volatile uint32_t *)PLASMAPONG_RDP_WAIT_CALLER_ADDRESS;
        queue_wait_masks[mask&0x3ff]++;
        if(mask) queue_wait_callers[(caller&0xffc)>>2]++;
#endif
        queue_samples++;
        queue_halted+=!!(sp&SP_STATUS_HALTED);
        queue_dp_busy+=!!(dp&0x40);
        queue_dma_busy+=!!(dp&0x100);
        queue_end_valid+=!!(dp&0x200);
        assertf(get_ticks()-begin<TICKS_FROM_US(2000000),"queue sampler timeout");
        /* Leave interrupts enabled between samples to service audio. */
        wait_ticks(TICKS_FROM_US(100));
    }
}
void fluid_queue_pc_report(void) {
    debugf("Queue PC sampler: status only, period 100 us\n");
    debugf("Queue PC: total %u, halted %u, DP busy %u, DMA busy %u, end valid %u\n",
        queue_samples,queue_halted,queue_dp_busy,queue_dma_busy,queue_end_valid);
#ifdef PLASMAPONG_RDP_WAIT_TRACE
    for(unsigned i=0;i<1024;i++) if(queue_wait_masks[i])
        debugf("RSP wait mask: 0x%03x samples %u\n",i,queue_wait_masks[i]);
    for(unsigned rank=0;rank<6;rank++) {
        unsigned best=0;
        for(unsigned i=1;i<1024;i++) if(queue_wait_callers[i]>queue_wait_callers[best]) best=i;
        if(!queue_wait_callers[best]) break;
        debugf("RSP wait caller: 0x%03x samples %u\n",best*4,queue_wait_callers[best]);
        queue_wait_callers[best]=0;
    }
    memset(queue_wait_masks,0,sizeof(queue_wait_masks));
    memset(queue_wait_callers,0,sizeof(queue_wait_callers));
#endif
    queue_samples=queue_halted=queue_dp_busy=queue_dma_busy=queue_end_valid=0;
}
#endif
#ifdef PLASMAPONG_MENU_STAMPS
#include <stdbool.h>
#include <assert.h>
#endif
_Static_assert((FW-2)%2==0,"pressure loop handles two interior cells at a time");
_Static_assert((-1>>1)==-1,"fixed-point pressure requires arithmetic right shift");
/* Public pressure is Q12; the warm-started solver uses compact Q3 lanes. */
#define PRESSURE_SCALE 4096
static inline void ink_add(FluidInkValue *ink,float amount,float limit) {
    int value=*ink+(int)(amount*DYE_SCALE+.5f);
    int ceiling=(int)(limit*DYE_SCALE+.5f);
    *ink=(int16_t)(value>ceiling?ceiling:value<0?0:value);
}
/* Separable interpolation shares coordinates across channels and needs only
   three multiplies per channel. */
static inline float bilerp(const FluidFlowValue *a,int k,float tx,float ty) {
    float top=a[k]+tx*(a[k+1]-a[k]);
    float bottom=a[k+FW]+tx*(a[k+FW+1]-a[k+FW]);
    return (top+ty*(bottom-top))*(1.0f/VELOCITY_SCALE);
}
static void sample_pair(const FluidFlowValue *a,const FluidFlowValue *b,float x,float y,float *va,float *vb) {
    x=clampf(x,0,FW-1.001f); y=clampf(y,0,FH-1.001f);
    int ix=(int)x,iy=(int)y,k=iy*FW+ix;
    float tx=x-ix,ty=y-iy;
    *va=bilerp(a,k,tx,ty);
    *vb=bilerp(b,k,tx,ty);
}
void fluid_init(Fluid *f) { memset(f,0,sizeof(*f)); }
void fluid_sample(const Fluid *f,float x,float y,float *u,float *v) {
    const FluidFlow *velocity=fluid_velocity(f);
    PROFILE_BEGIN();
    sample_pair(velocity->u,velocity->v,x/CELL-.5f,y/CELL-.5f,u,v);
    PROFILE_END(PROFILE_SAMPLE);
}
static void walls(Fluid *f) {
    FluidFlow *velocity=fluid_velocity(f);
    for(int y=0;y<FH;y++) velocity->u[y*FW]=velocity->u[y*FW+FW-1]=0;
    for(int x=0;x<FW;x++) velocity->v[x]=velocity->v[(FH-1)*FW+x]=0;
}
#include "fluid_pressure_q3.h"
void fluid_pressure_cpu(int32_t *restrict pressure,const int32_t *restrict divergence) {
    fluid_pressure_q3_cpu(pressure,divergence);
}
static inline void subtract_gradient(FluidFlowValue *v,int32_t difference) {
    /* pressure Q12 -> velocity Q4: divide by 2*CELL*4096/16. */
    const int divisor=PLASMAPONG_CELL_Q4*32,half=divisor/2;
    int change=difference>=0?(difference+half)/divisor:-((-difference+half)/divisor);
    *v=fluid_flow_clamp(*v-change);
}
void fluid_project(Fluid *f) {
    FluidFlow *velocity=fluid_velocity(f);
    PROFILE_BEGIN();
    walls(f);
#ifdef PLASMAPONG_PROJECTION_CHAIN
    fluid_projection_short_rsp(velocity,f->divergence,f->pressure,f->pressure_short);
    /* Attribute the complete chained operation to projection's solve stage. */
    PROFILE_END(PROFILE_PRESSURE);
#else
#ifdef PLASMAPONG_PREPARE_RSP
    fluid_divergence_rsp(f->divergence,velocity);
#else
    /* Only interior divergence is consumed by the Neumann pressure solve. */
    for(int y=1;y<FH-1;y++) for(int x=1;x<FW-1;x++) {
        int k=y*FW+x;
        f->divergence[k]=-(PLASMAPONG_CELL_Q4*8)*(velocity->u[k+1]-velocity->u[k-1]+velocity->v[k+FW]-velocity->v[k-FW]);
    }
#endif
    PROFILE_END(PROFILE_DIVERGENCE);
#ifdef PLASMAPONG_FLUID_RSP
    fluid_pressure_rsp(f->pressure,f->divergence);
#else
    fluid_pressure_cpu(f->pressure,f->divergence);
#endif
    PROFILE_END(PROFILE_PRESSURE);
    for(int y=1;y<FH-1;y++) for(int x=1;x<FW-1;x++) {
        int k=y*FW+x;
        subtract_gradient(&velocity->u[k],f->pressure[k+1]-f->pressure[k-1]);
        subtract_gradient(&velocity->v[k],f->pressure[k+FW]-f->pressure[k-FW]);
    }
    /* Tangential edge velocity survives; wall-normal velocity is zero. */
    for(int x=1;x<FW-1;x++) {
        subtract_gradient(&velocity->u[x],f->pressure[x+1]-f->pressure[x-1]);
        int k=(FH-1)*FW+x;
        subtract_gradient(&velocity->u[k],f->pressure[k+1]-f->pressure[k-1]);
    }
    for(int y=1;y<FH-1;y++) {
        int k=y*FW;
        subtract_gradient(&velocity->v[k],f->pressure[k+FW]-f->pressure[k-FW]);
        k+=FW-1;
        subtract_gradient(&velocity->v[k],f->pressure[k+FW]-f->pressure[k-FW]);
    }
    walls(f);
    PROFILE_END(PROFILE_GRADIENT);
#endif
}
/* Blend existing pigment channels to keep all four jets in the plasma palette. */
typedef struct {
    int16_t *a,*b;
    float wa,wb;
    int ca,cb;
} SplatInk;
static inline SplatInk splat_ink_plan(FluidInk *ink,int player) {
    return player==2?(SplatInk){ink->blue,ink->gold,.65f,.35f,3*DYE_SCALE,(int)(.65f*DYE_SCALE+.5f)}:
        player==3?(SplatInk){ink->blue,ink->red,.55f,.65f,3*DYE_SCALE,3*DYE_SCALE}:
        (SplatInk){player?ink->red:ink->blue,NULL,1,0,3*DYE_SCALE,0};
}
static inline void splat_ink_add(const SplatInk *p,int k,float amount) {
    int a=p->a[k]+(int)(amount*p->wa*DYE_SCALE+.5f);
    p->a[k]=a>p->ca?p->ca:a<0?0:a;
    if(p->b) {
        int b=p->b[k]+(int)(amount*p->wb*DYE_SCALE+.5f);
        p->b[k]=b>p->cb?p->cb:b<0?0:b;
    }
}
/* Work in storage units on the fixed backend. Power-of-two scaling keeps
   the original float operations and nearest rounding exactly equivalent,
   while avoiding decode/encode and the redundant wider storage clamp. */
static inline void force_add(FluidFlowValue *cell,float delta) {
    float value=*cell+delta;
    if(value>420*VELOCITY_SCALE) *cell=420*VELOCITY_SCALE;
    else if(value<-420*VELOCITY_SCALE) *cell=-420*VELOCITY_SCALE;
    else *cell=(int16_t)(value+(value<0?-.5f:.5f));
}
#include "fluid_force_fixed.h"
void fluid_splat(Fluid *f,float x,float y,float radius,float u,float v,float dye,int player) {
    PROFILE_BEGIN();
    fluid_force_splat(f,x,y,radius,u,v,dye,player);
    PROFILE_END(PROFILE_SPLAT);
}
void fluid_pump(Fluid *f,float x,float y,float radius,float strength,float dt,int player) {
    PROFILE_BEGIN();
    fluid_force_pump(f,x,y,radius,strength,dt,player);
    PROFILE_END(PROFILE_PUMP);
}
void fluid_velocity_step(Fluid *f,float dt) {
#ifdef PLASMAPONG_VELOCITY_CHAIN
    PROFILE_BEGIN();
    fluid_velocity_chain_rsp(f,dt);
    PROFILE_END(PROFILE_VELOCITY_ADVECTION);
    return;
#else
    FluidFlow *velocity=fluid_velocity(f);
    PROFILE_BEGIN();
#ifdef PLASMAPONG_QUEUE_PROFILE
    /* Diagnostic split: retire earlier RSP commands before timing advection.
       A syncpoint does not request full RDP completion. This extra CPU wait
       changes scheduling, so compare its total against the stage-only run. */
    rspq_syncpoint_t queued=rspq_syncpoint_new();
    rspq_flush();
#ifdef PLASMAPONG_QUEUE_PC_PROFILE
    queue_sample_wait(queued);
#else
    rspq_syncpoint_wait(queued);
#endif
    PROFILE_END(PROFILE_QUEUE_WAIT);
#endif
    FluidFlow *next=&f->velocity[f->velocity_bank^1];
    const float grid_dt=dt/CELL,decay=1-FLUID_DAMPING*dt;
    /* Local upwind transport with a shared velocity bound. */
#ifdef PLASMAPONG_UPWIND_RSP
    fluid_upwind_velocity_rsp(next,velocity,grid_dt,decay,(f->velocity_phase+=40503u)&65535u);
#else
    fluid_upwind_velocity_cpu(next,velocity,grid_dt,decay,(f->velocity_phase+=40503u)&65535u);
#endif
    PROFILE_END(PROFILE_VELOCITY_ADVECTION);
    f->velocity_bank^=1; velocity=next;
    PROFILE_END(PROFILE_VELOCITY_SWAP);
    fluid_project(f);
#endif
}
void fluid_dye_step(Fluid *f,float dt) {
    FluidFlow *velocity=fluid_velocity(f);
    FluidInk *ink_grid=fluid_dye(f);
    PROFILE_BEGIN();
    FluidInk *next=&f->dye[f->dye_bank^1];
    const float grid_dt=dt/CELL,decay=1-.22f*dt,gold_decay=1-1.1f*dt;
    /* Deterministic temporal rounding; state remains safe to copy by value. */
    unsigned rounding=(f->dye_phase+=40503u)&65535u;
#ifdef PLASMAPONG_UPWIND_RSP
    fluid_upwind_ink_rsp(next,ink_grid,velocity,grid_dt,decay,gold_decay,rounding);
#else
    fluid_upwind_ink_cpu(next,ink_grid,velocity,grid_dt,decay,gold_decay,rounding);
#endif
    PROFILE_END(PROFILE_DYE_ADVECTION);
    f->dye_bank^=1;
    PROFILE_END(PROFILE_DYE_SWAP);
}
static void ball_dye(Fluid *f,float x,float y,float amount,int hot) {
    FluidInk *ink_grid=fluid_dye(f);
    FluidInkValue *ink=hot?ink_grid->red:ink_grid->gold;
    PROFILE_BEGIN();
    const float radius=8;
    int x0=(int)clampf((x-radius)/CELL,0,FW-1),x1=(int)clampf((x+radius)/CELL,0,FW-1);
    int y0=(int)clampf((y-radius)/CELL,0,FH-1),y1=(int)clampf((y+radius)/CELL,0,FH-1);
    for(int iy=y0;iy<=y1;iy++) for(int ix=x0;ix<=x1;ix++) {
        float dx=(ix+.5f)*CELL-x,dy=(iy+.5f)*CELL-y;
        float w=maxf(0,1-(dx*dx+dy*dy)/(radius*radius));
        int k=iy*FW+ix;
        ink_add(&ink[k],amount*w*w,hot?3:.65f);
    }
    PROFILE_END(PROFILE_BALL_DYE);
}
void fluid_ball_dye(Fluid *f,float x,float y,float amount) { ball_dye(f,x,y,amount,0); }
void fluid_hot_ball_dye(Fluid *f,float x,float y,float amount) { ball_dye(f,x,y,amount,1); }
#ifdef PLASMAPONG_MENU_STAMPS
enum { MENU_FOOTPRINT_SIDE=(24*16+PLASMAPONG_CELL_Q4-1)/PLASMAPONG_CELL_Q4+1,
    MENU_FOOTPRINT_CAPACITY=MENU_FOOTPRINT_SIDE*MENU_FOOTPRINT_SIDE };
typedef struct {
    unsigned count;
    uint16_t index[MENU_FOOTPRINT_CAPACITY];
    float weight[MENU_FOOTPRINT_CAPACITY];
} MenuFootprint;
static struct {
    bool valid;
    float x,y;
    MenuFootprint velocity,gold;
} menu_sources[26];
static void menu_footprint(MenuFootprint *p,float x,float y,float radius,bool squared) {
    int x0=(int)clampf((x-radius)/CELL,0,FW-1),x1=(int)clampf((x+radius)/CELL,0,FW-1);
    int y0=(int)clampf((y-radius)/CELL,0,FH-1),y1=(int)clampf((y+radius)/CELL,0,FH-1);
    const float inv_radius2=1/(radius*radius);
    p->count=0;
    for(int iy=y0;iy<=y1;iy++) for(int ix=x0;ix<=x1;ix++) {
        float dx=(ix+.5f)*CELL-x,dy=(iy+.5f)*CELL-y;
        /* Radius 8 division is an exact power-of-two scaling. Preserve
           radius 12's original multiply by its precomputed reciprocal. */
        float w=maxf(0,1-(squared?(dx*dx+dy*dy)*inv_radius2:(dx*dx+dy*dy)/(radius*radius)));
        assert(p->count<MENU_FOOTPRINT_CAPACITY);
        p->index[p->count]=iy*FW+ix;
        p->weight[p->count++]=squared?w*w:w;
    }
}
void fluid_menu_source(Fluid *f,unsigned slot,float x,float y,float u,float v,float gold) {
    assert(slot<26);
    if(!menu_sources[slot].valid || menu_sources[slot].x!=x || menu_sources[slot].y!=y) {
        menu_footprint(&menu_sources[slot].velocity,x,y,12,true);
        menu_footprint(&menu_sources[slot].gold,x,y,8,false);
        menu_sources[slot].x=x; menu_sources[slot].y=y; menu_sources[slot].valid=true;
    }
    FluidFlow *velocity=fluid_velocity(f);
    FluidInk *ink=fluid_dye(f);
    u*=VELOCITY_SCALE; v*=VELOCITY_SCALE;
    PROFILE_BEGIN();
    const MenuFootprint *p=&menu_sources[slot].velocity;
    for(unsigned i=0;i<p->count;i++) {
        unsigned k=p->index[i]; float w=p->weight[i];
        if(w==0) {
            const int limit=420*VELOCITY_SCALE;
            int a=velocity->u[k],b=velocity->v[k];
            velocity->u[k]=a<-limit?-limit:a>limit?limit:a;
            velocity->v[k]=b<-limit?-limit:b>limit?limit:b;
        } else {
            force_add(&velocity->u[k],u*w); force_add(&velocity->v[k],v*w);
        }
        /* The original zero-dye splat still clamps its blue channel. */
        int b=ink->blue[k]; ink->blue[k]=b<0?0:b>3*DYE_SCALE?3*DYE_SCALE:b;
    }
    PROFILE_END(PROFILE_SPLAT);
    p=&menu_sources[slot].gold;
    for(unsigned i=0;i<p->count;i++) {
        float w=p->weight[i]; ink_add(&ink->gold[p->index[i]],gold*w*w,.65f);
    }
    PROFILE_END(PROFILE_BALL_DYE);
}
#endif
static inline uint32_t dye_color(const FluidInk *ink_grid,int k) {
    int r=ink_grid->red[k],b=ink_grid->blue[k],g=ink_grid->gold[k];
    int red=(5*DYE_SCALE+210*r+20*b+255*g)>>13;
    int green=(9*DYE_SCALE+64*r+155*b+205*g)>>13;
    int blue=(22*DYE_SCALE+92*r+225*b+25*g)>>13;
    red=red>255?255:red; green=green>255?255:green; blue=blue>255?255:blue;
    return (uint32_t)((red<<16)|(green<<8)|blue);
}
uint32_t fluid_color(const Fluid *f,int k) { return dye_color(fluid_dye(f),k); }
void fluid_pixels(const Fluid *f,uint32_t *pixels,unsigned stride) {
    const FluidInk *ink_grid=fluid_dye(f);
    for(int y=0;y<FH;y++) for(int x=0;x<FW;x++)
        pixels[y*stride+x]=(dye_color(ink_grid,y*FW+x)<<8)|255;
}

static inline uint32_t speed_palette_color(unsigned speed) {
    /* Fixed speed stops (pixels/second): 0, 16, 32, 64, 128, 256.
       Wider high-speed bands retain detail in weak currents. No dye dependency,
       auto-exposure, square root, or per-cell division is needed. */
    static const uint32_t colors[]={0x050916,0x244bce,0x17bdd4,0x35cb63,0xf3cf3a,0xf04b36};
    if(speed>=256) return colors[5];
    unsigned band,base,shift;
    if(speed<16) { band=0; base=0; shift=4; }
    else if(speed<32) { band=1; base=16; shift=4; }
    else if(speed<64) { band=2; base=32; shift=5; }
    else if(speed<128) { band=3; base=64; shift=6; }
    else { band=4; base=128; shift=7; }
    unsigned t=((unsigned)speed-base)<<(8-shift);
    uint32_t a=colors[band],b=colors[band+1];
    unsigned r=(((a>>16)&255)*(256-t)+((b>>16)&255)*t)>>8;
    unsigned g=(((a>>8)&255)*(256-t)+((b>>8)&255)*t)>>8;
    unsigned blue=((a&255)*(256-t)+(b&255)*t)>>8;
    return (r<<16)|(g<<8)|blue;
}

static inline uint32_t speed_color(const FluidFlow *flow,int k) {
    int u=flow->u[k],v=flow->v[k];
    u=u<0?-u:u; v=v<0?-v:v;
    int speed=(u>v?u+v/2:v+u/2)/VELOCITY_SCALE;
    return speed_palette_color((unsigned)speed);
}

void fluid_speed_palette(uint32_t *rgba) {
    for(unsigned i=0;i<FLUID_SPEED_PALETTE_SIZE;i++) rgba[i]=(speed_palette_color(i)<<8)|255;
}

uint32_t fluid_speed_color(const Fluid *f,int k) {
    return speed_color(fluid_velocity(f),k);
}
void fluid_speed_pixels(const Fluid *f,uint32_t *pixels,unsigned stride) {
    const FluidFlow *flow=fluid_velocity(f);
    for(int y=0;y<FH;y++) for(int x=0;x<FW;x++)
        pixels[y*stride+x]=(speed_color(flow,y*FW+x)<<8)|255;
}

/* These views only replace colour conversion; the simulation is untouched.
   Work stays at FW*FH, with fixed exposure and bounded neighbour stencils. */
static uint32_t view_mix(uint32_t a,uint32_t b,unsigned t) {
    unsigned r=(((a>>16)&255)*(256-t)+((b>>16)&255)*t)>>8;
    unsigned g=(((a>>8)&255)*(256-t)+((b>>8)&255)*t)>>8;
    unsigned blue=((a&255)*(256-t)+(b&255)*t)>>8;
    return (r<<16)|(g<<8)|blue;
}
static uint32_t view_palette_color(FluidView view,unsigned index) {
    if(view==FLUID_VIEW_BANDS) {
        /* Broad repeating ribbons, not geometric contour overlays. */
        unsigned t=(index&63)*8;
        if(t>256) t=512-t;
        return view_mix(0x102b53,0x6ef3cf,t);
    }
    /* Signed fields: neutral navy, negative cyan, positive coral. */
    int n=(int)index-128;
    unsigned t=(unsigned)(n<0?-n:n)*2;
    uint32_t end=n<0?0x24cde0:0xff6956;
    return view_mix(0x070c1c,end,t);
}
void fluid_view_palette(FluidView view,uint32_t *rgba) {
    for(unsigned i=0;i<FLUID_SPEED_PALETTE_SIZE;i++) {
        unsigned index=i>256?256:i;
        if(view==FLUID_VIEW_BANDS && index>224) index=224;
        rgba[i]=(view_palette_color(view,index)<<8)|255;
    }
}
static const uint32_t *view_palette(FluidView view) {
    static uint32_t signed_colors[257],bands[257];
    static int ready;
    if(!ready) {
        for(unsigned i=0;i<=256;i++) {
            signed_colors[i]=view_palette_color(FLUID_VIEW_VORTEX,i);
            bands[i]=view_palette_color(FLUID_VIEW_BANDS,i);
        }
        ready=1;
    }
    return view==FLUID_VIEW_BANDS?bands:signed_colors;
}
static int view_flow(FluidFlowValue value) {
    return value;
}
static int view_density(const FluidInk *ink,int k) {
    return ink->red[k]+ink->blue[k]+ink->gold[k];
}
static uint32_t view_color(const Fluid *f,int k,FluidView view,const uint32_t *palette) {
    const FluidFlow *v=fluid_velocity(f);
    if(view==FLUID_VIEW_DYE) return dye_color(fluid_dye(f),k);
    if(view==FLUID_VIEW_SPEED) return speed_color(v,k);
    if(view==FLUID_VIEW_BANDS) {
        int u=view_flow(v->u[k]),w=view_flow(v->v[k]);
        u=u<0?-u:u; w=w<0?-w:w;
        unsigned speed=(unsigned)(u>w?u+w/2:w+u/2)/VELOCITY_SCALE;
        /* Keep the strongest flows on a bright plateau to avoid aliasing. */
        return palette[speed>224?224:speed];
    }
    int x=k%FW,y=k/FW;
    int left=x?k-1:k,right=x<FW-1?k+1:k;
    int above=y?k-FW:k,below=y<FH-1?k+FW:k;
    if(view==FLUID_VIEW_RELIEF) {
        const FluidInk *ink=fluid_dye(f);
        int dx=view_density(ink,right)/4-view_density(ink,left)/4;
        int dy=view_density(ink,below)/4-view_density(ink,above)/4;
        /* Fixed upper-left light. Approximate relief, no normalisation/sqrt. */
        int shade=224+(dx-dy)/32;
        shade=shade<72?72:shade>352?352:shade;
        uint32_t c=dye_color(ink,k);
        unsigned r=((c>>16)&255)*shade>>10;
        unsigned g=((c>>8)&255)*shade>>10;
        unsigned b=(c&255)*shade>>10;
        r*=4; g*=4; b*=4;
        r=r>255?255:r; g=g>255?255:g; b=b>255?255:b;
        return (r<<16)|(g<<8)|b;
    }
    int n;
    if(view==FLUID_VIEW_VORTEX) {
        /* Recompute from current projected velocity: pressure scratch has
           already been overwritten by divergence. Screen y increases down. */
        n=(view_flow(v->v[right])-view_flow(v->v[left])-
           view_flow(v->u[below])+view_flow(v->u[above]))/16;
    } else n=f->pressure[k]/8192; /* Q12 solver pressure; fixed visual gain. */
    n=n<-128?-128:n>128?128:n;
    return palette[n+128];
}
uint32_t fluid_view_color(const Fluid *f,int k,FluidView view) {
    return view_color(f,k,view,view_palette(view));
}
void fluid_view_pixels(const Fluid *f,uint32_t *pixels,unsigned stride,FluidView view) {
    const uint32_t *palette=view_palette(view);
    for(int y=0;y<FH;y++) for(int x=0;x<FW;x++)
        pixels[y*stride+x]=(view_color(f,y*FW+x,view,palette)<<8)|255;
}

void fluid_relief_shades(const Fluid *f,int16_t *shades) {
    int16_t density[FN];
    const FluidInk *ink=fluid_dye(f);
    for(int k=0;k<FN;k++) density[k]=(int16_t)(view_density(ink,k)/4);
    for(int y=0;y<FH;y++) for(int x=0;x<FW;x++) {
        int k=y*FW+x;
        int dx=density[x<FW-1?k+1:k]-density[x?k-1:k];
        int dy=density[y<FH-1?k+FW:k]-density[y?k-FW:k];
        int shade=224+(dx-dy)/32;
        shades[k]=(int16_t)((shade<72?72:shade>352?352:shade)*64);
    }
}
