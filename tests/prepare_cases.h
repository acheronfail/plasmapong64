#ifndef PREPARE_CASES_H
#define PREPARE_CASES_H
#include "../src/fluid_velocity_fixed.h"
static void prepare_pixels16_case(const Fluid *f,const uint32_t *expected) {
    static struct { _Alignas(16) uint16_t before[8],value[64*FH],after[8]; } packed;
    memset(&packed,0x5a,sizeof(packed));
    data_cache_hit_writeback_invalidate(&packed,sizeof(packed));
    fluid_pixels16_rsp_begin(f,packed.value,64);
    rdpq_set_fill_color(RGBA32(0,0,0,255));
    rspq_wait(); data_cache_hit_invalidate(&packed,sizeof(packed));
    for(unsigned y=0;y<FH;y++) for(unsigned x=0;x<64;x++) {
        uint32_t p=expected[y*64+x];
        uint16_t want=x<FW?((p>>16)&0xf800)|((p>>13)&0x07c0)|((p>>10)&0x003e)|1:0x5a5a;
        if(packed.value[y*64+x]!=want) {
            debugf("Packed pixels mismatch row %u col %u: %04x/%04x\n",y,x,want,packed.value[y*64+x]);
            assert(0);
        }
    }
    for(unsigned i=0;i<8;i++) assert(packed.before[i]==0x5a5a && packed.after[i]==0x5a5a);
}
static void prepare_cases(void) {
    static Fluid f,saved;
    static struct { _Alignas(16) uint32_t before[4],value[64*FH],after[4]; } pixels;
    static struct { _Alignas(16) uint32_t before[4]; int32_t value[FN]; uint32_t after[4]; } divergence;
    static uint32_t expected_pixels[64*FH];
    uint32_t rng=18237;
    for(unsigned trial=0;trial<80;trial++) {
        FluidVelocityFixed *v=fluid_velocity(&f);
        FluidDyeFixed *d=fluid_dye(&f);
        for(unsigned k=0;k<FN;k++) {
            rng=rng*1664525u+1013904223u;
            v->u[k]=(int)(rng%32767)-16383;
            rng=rng*1664525u+1013904223u;
            v->v[k]=(int)(rng%32767)-16383;
            if(trial<8) v->u[k]=v->v[k]=trial<4?0:(k&1?16383:-16383);
            if(trial>=8 && trial<24) {
                v->u[k]=(k%258)*16*(trial&1?-1:1);
                v->v[k]=((k%17)*16+trial%16)*(trial&2?-1:1);
            }
            rng=rng*1664525u+1013904223u;
            d->red[k]=rng%24577;
            rng=rng*1664525u+1013904223u;
            d->blue[k]=rng%24577;
            rng=rng*1664525u+1013904223u;
            d->gold[k]=rng%24577;
            if(trial<8) d->red[k]=d->blue[k]=d->gold[k]=trial;
        }
        saved=f;
        data_cache_hit_writeback_invalidate(&f,sizeof(f));
        memset(&divergence,0x3c,sizeof(divergence));
        data_cache_hit_writeback_invalidate(&divergence,sizeof(divergence));
        fluid_divergence_rsp(divergence.value,v);
        data_cache_hit_invalidate(&divergence,sizeof(divergence));
        for(int y=1;y<FH-1;y++) for(int x=1;x<FW-1;x++) {
            int k=y*FW+x;
            int32_t want=-(PLASMAPONG_CELL_Q4*8)*(v->u[k+1]-v->u[k-1]+v->v[k+FW]-v->v[k-FW]);
            assert(divergence.value[k]==want);
        }
        for(unsigned k=0;k<FW;k++) assert(divergence.value[k]==0x3c3c3c3c && divergence.value[(FH-1)*FW+k]==0x3c3c3c3c);
        for(unsigned k=0;k<4;k++) assert(divergence.before[k]==0x3c3c3c3c && divergence.after[k]==0x3c3c3c3c);
        static const float steps[]={0,1.0f/(30*6),1.0f/(60*6),.125f,.2f/6,.0625f,.062499f,.000001f};
        memset(expected_pixels,0x5a,sizeof(expected_pixels));
        fluid_pixels(&f,expected_pixels,64);
        data_cache_hit_writeback_invalidate(&pixels,sizeof(pixels));
        if(trial&1) {
            fluid_pixels_rsp_begin(&f,pixels.value,64);
            rdpq_set_fill_color(RGBA32(trial,0,0,255));
            rspq_wait();
        } else fluid_pixels_rsp(&f,pixels.value,64);
        data_cache_hit_invalidate(&pixels,sizeof(pixels));
        for(unsigned k=0;k<64*FH;k++) {
            if(expected_pixels[k]!=pixels.value[k]) {
                debugf("Pixels mismatch trial %u cell %u: %08lx/%08lx\n",trial,k,(unsigned long)expected_pixels[k],(unsigned long)pixels.value[k]);
                assert(0);
            }
        }
        prepare_pixels16_case(&f,expected_pixels);
        memset(&pixels,0x5a,sizeof(pixels));
        memset(expected_pixels,0x5a,sizeof(expected_pixels));
        fluid_speed_field_pixels(&f,expected_pixels,64);
        fluid_speed_field_rsp_begin(&f,pixels.value,64);
        rspq_wait(); data_cache_hit_invalidate(&pixels,sizeof(pixels));
        for(unsigned k=0;k<64*FH;k++) assert(expected_pixels[k]==pixels.value[k]);
        data_cache_hit_invalidate(&f,sizeof(f));
        assert(!memcmp(&f,&saved,sizeof(f)));
        for(unsigned k=0;k<4;k++) assert(pixels.before[k]==0x5a5a5a5a && pixels.after[k]==0x5a5a5a5a);
    }
    debugf("Prepare PASS: 80 exact dye-color/divergence fields, queued pixel producers, overlay switches, full timestep range, padded pixels and DMA guards\n");
    debugf("Speed field PASS: 80 exact CPU/RSP grayscale fields, padding and DMA guards\n");
    debugf("Packed pixels PASS: 80 dye fields exactly match RGBA5551 quantization, padding and DMA guards\n");
}
#endif
