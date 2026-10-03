#ifndef PERF_H
#define PERF_H
#include <stdbool.h>
#include <stdint.h>
/* Sample the displayed framebuffer once per non-interlaced VI refresh. */
typedef struct {
    uint32_t origin, refreshes, frames, repeats, gap, longest_gap;
    uint32_t allowed_gap, misses;
    uint64_t ticks;
    bool seeded;
} PresentationStats;
static inline void presentation_sample(volatile PresentationStats *s,uint32_t origin,uint64_t ticks) {
    s->ticks=ticks;
    if(!s->seeded) {
        s->origin=origin; s->seeded=true; s->gap=s->longest_gap=1;
        return;
    }
    s->refreshes++;
    if(origin!=s->origin) { s->frames++; s->origin=origin; s->gap=1; }
    else {
        s->repeats++; s->gap++;
        if(s->gap>(s->allowed_gap?s->allowed_gap:1)) s->misses++;
    }
    if(s->gap>s->longest_gap) s->longest_gap=s->gap;
}
#endif
