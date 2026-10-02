#include "draw.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static unsigned calls;
void rect(float x,float y,float w,float h,uint32_t color) {
    (void)color;
    assert(isfinite(x) && isfinite(y) && isfinite(w));
    assert(x>=16 && y>=34 && x+w<=304 && y+h<=232);
    assert(w>=1 && h==1);
    calls++;
}
void draw_static(unsigned id,void (*draw)(void)) { (void)id; (void)draw; }
void label(float x,float y,int style,const char *s) { (void)x;(void)y;(void)style;(void)s; }
void label_edge(float x,float y,int style,const char *s,bool right) { (void)right;label(x,y,style,s); }
float label_width(const char *s) { (void)s;return 0; }
void draw_fluid(const Fluid *f,float x,float y,float w,float h,bool speed) {
    (void)f;(void)x;(void)y;(void)w;(void)h;(void)speed;
}
int main(void) {
    uint32_t pixels[FH*64];
    /* Padding is deliberately bright: it must never enter the contour field. */
    for(int i=0;i<FH*64;i++) pixels[i]=0xffffffff;
    for(int y=0;y<FH;y++) for(int x=0;x<FW;x++) pixels[y*64+x]=0x282828ff;
    draw_flow_contours(pixels,64,16,34,288,198); assert(calls==0);
    for(int y=0;y<FH;y++) for(int x=0;x<FW;x++) {
        unsigned c=x*3; pixels[y*64+x]=(c<<24)|(c<<16)|(c<<8)|255;
    }
    draw_flow_contours(pixels,64,16,34,288,198); assert(calls>0 && calls<=900);
    calls=0;
    for(int y=0;y<FH;y++) for(int x=0;x<FW;x++) pixels[y*64+x]=(x/2+y/2)%2?0xffffffff:255;
    draw_flow_contours(pixels,64,16,34,288,198); assert(calls==900);
    puts("PASS: contour uniform fields, ramps, padded rows, saddle edges and drawing budget");
}
