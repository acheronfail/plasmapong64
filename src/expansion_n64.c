#include <libdragon.h>

/* Opt-in hardware experiment. Linker wrapping keeps the pinned display
   implementation (and framebuffer lifetime/swap logic) intact. Only 640x480
   16-bit surfaces get bank alignment, and only with an Expansion Pak present.
   Each 614,400-byte framebuffer then lies within one distinct 1-MiB bank.
   This may reduce VI/RDP page conflicts; it does not increase bus bandwidth.
   Keep disabled by default until measured on a real console. */
void *__real_malloc_uncached_aligned(int align,size_t size);
void *__wrap_malloc_uncached_aligned(int align,size_t size) {
    if(align==64 && size==640u*480*2 && is_memory_expanded()) {
        void *buffer=__real_malloc_uncached_aligned(1024*1024,size);
        if(buffer) {
            debugf("Expansion experiment: bank-aligned framebuffer at 0x%08lx\n",
                (unsigned long)PhysicalAddr(buffer));
            return buffer;
        }
        debugf("Expansion experiment: bank allocation unavailable; using standard alignment\n");
    }
    return __real_malloc_uncached_aligned(align,size);
}
