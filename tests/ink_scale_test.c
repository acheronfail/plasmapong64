#include <assert.h>
#include <stdio.h>
#include "ink_scale.h"
int main(void) {
    uint32_t input[5*4],output[6*4];
    unsigned seed=7;
    for(unsigned i=0;i<20;i++) {
        seed=1664525*seed+1013904223;
        input[i]=(seed&0xffffff00)|255;
    }
    for(unsigned i=0;i<24;i++) output[i]=0x12345678;
    ink_blur(input,5,output,6,4,4);
    for(int y=0;y<4;y++) for(int x=0;x<4;x++) {
        for(unsigned shift=8;shift<=24;shift+=8) {
            unsigned expected=0;
            for(int dy=-1;dy<=1;dy++) {
                int cy=y+dy; cy=cy<0?0:cy>3?3:cy;
                unsigned row=0;
                for(int dx=-1;dx<=1;dx++) {
                    int cx=x+dx; cx=cx<0?0:cx>3?3:cx;
                    row+=((input[cy*5+cx]>>shift)&255)*(dx==0?2:1);
                }
                expected+=(row/4)*(dy==0?2:1);
            }
            assert(((output[y*6+x]>>shift)&255)==expected/4);
        }
        assert((output[y*6+x]&255)==255);
    }
    for(unsigned y=0;y<4;y++) for(unsigned x=4;x<6;x++) assert(output[y*6+x]==0x12345678);
    uint32_t single=0xabcdefFF,small;
    ink_blur(&single,1,&small,1,1,1);
    assert(small==single);
    puts("Display blur: 3x3 reference, clamped edges and padding PASS");
}
