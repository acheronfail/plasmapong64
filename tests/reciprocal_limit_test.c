#include "fluid_upwind.h"
#include "rsp_reciprocal_table.inc"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static FluidVelocityFixed current,original;
static unsigned count,cap;
static void check_batch(void) {
    fluid_upwind_clamp(&current,cap);
    for(unsigned k=0;k<count;k++) {
        int u=original.u[k],v=original.v[k],mag=abs(u)+abs(v);
        int a=current.u[k],b=current.v[k];
        assert((unsigned)(abs(a)+abs(b))<=cap);
        assert((u>=0?a>=0:a<=0) && (v>=0?b>=0:b<=0));
        if((unsigned)mag<=cap) assert(a==u && b==v);
        else {
            int eu=u*(int)cap/mag,ev=v*(int)cap/mag;
            assert(abs(a-eu)<=(int)(cap>>8)+8);
            assert(abs(b-ev)<=(int)(cap>>8)+8);
            if(!u || !v) assert(a==eu && b==ev);
        }
    }
    count=0;
}
static void append(int u,int v) {
    if(abs(u)>VELOCITY_LIMIT || abs(v)>VELOCITY_LIMIT) return;
    current.u[count]=original.u[count]=(int16_t)u;
    current.v[count]=original.v[count]=(int16_t)v;
    if(++count==FN) check_batch();
}
int main(void) {
    uint64_t maximum=0;
    for(unsigned n=1;n<=2*VELOCITY_LIMIT;n++) {
        unsigned shift=(unsigned)__builtin_clz(n),index=((n<<shift)&0x7fc00000u)>>22;
        uint32_t inverse=((0x10000u|rsp_reciprocal_table[index])<<14)>>(31-shift);
        uint64_t product=(uint64_t)n*inverse;
        if(product>maximum) maximum=product;
        assert(product*512 < (UINT64_C(1)<<31)*513);
    }
    for(unsigned limit=127;limit<=2*VELOCITY_LIMIT;limit++)
        assert((uint64_t)fluid_upwind_safe_limit(limit)*maximum < (uint64_t)limit*(UINT64_C(1)<<31));
    const unsigned caps[]={127,4318,5759,16383,32766};
    for(unsigned c=0;c<sizeof(caps)/sizeof(caps[0]);c++) {
        cap=caps[c]; memset(&current,0,sizeof(current)); memset(&original,0,sizeof(original));
        for(int n=1;n<=2*VELOCITY_LIMIT;n++) {
            const int parts[]={0,n/7,n/2,n-n/7,n};
            for(unsigned i=0;i<5;i++) for(int su=-1;su<=1;su+=2) for(int sv=-1;sv<=1;sv+=2)
                append(su*parts[i],sv*(n-parts[i]));
        }
        if(count) check_batch();
    }
    puts("PASS: exhaustive reciprocal headroom, all magnitudes/signs, exact axis clipping, bounded diagonal error and unchanged ordinary currents");
}
