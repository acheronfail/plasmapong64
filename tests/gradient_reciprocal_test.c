#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void) {
    for(uint64_t n=0;n<98304;n++) assert((n*43691>>17)==n/3);
    for(uint64_t n=0;n<(1u<<21);n++) {
        /* Model the unsigned-half accumulator used by the RSP, independent
           of the full product identity. Only product bits above 16 matter. */
        uint64_t low=n&65535,high=n>>16;
        uint64_t accumulator=(low*36409>>16)+high*36409+low*3+(high*3<<16);
        assert((accumulator>>5)==n/9);
        assert((n*233017>>21)==n/9);
    }
    puts("PASS: exhaustive divide-by-three/nine identities and RSP halfword decomposition");
}
