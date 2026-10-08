#ifndef PLASMAPONG_INK_SCALE_H
#define PLASMAPONG_INK_SCALE_H
#include <stdint.h>

/* Separable [1 2 1]/4 display blur. Packed lanes have two spare bits,
   so channels cannot carry into each other. Alpha stays opaque. */
static inline uint32_t ink_blend(uint32_t a,uint32_t b,uint32_t c) {
    uint32_t rb=(((a>>8)&0xff00ff)+2*((b>>8)&0xff00ff)+((c>>8)&0xff00ff))>>2;
    uint32_t g=(((a>>16)&255)+2*((b>>16)&255)+((c>>16)&255))>>2;
    return ((rb&0xff00ff)<<8)|(g<<16)|255;
}
static inline void ink_blur(const uint32_t *src,unsigned stride,
        uint32_t *dst,unsigned out_stride,unsigned w,unsigned h) {
    uint32_t rows[3][w];
    for(unsigned y=0;y<=h;y++) {
        if(y<h) {
            uint32_t *row=rows[y%3];
            for(unsigned x=0;x<w;x++)
                row[x]=ink_blend(src[y*stride+(x?x-1:x)],
                    src[y*stride+x],src[y*stride+(x+1<w?x+1:x)]);
        }
        if(!y) continue;
        unsigned centre=y-1;
        const uint32_t *row=rows[centre%3];
        const uint32_t *above=rows[(centre?centre-1:centre)%3];
        const uint32_t *below=rows[(y<h?y:centre)%3];
        for(unsigned x=0;x<w;x++)
            dst[centre*out_stride+x]=ink_blend(above[x],row[x],below[x]);
    }
}
#endif
