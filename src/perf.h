#ifndef PERF_H
#define PERF_H
#include <stdbool.h>
#include <stdint.h>
/* Normalize the one-row offset used by libdragon's even interlaced field.
   Refreshes still count fields; frames count distinct framebuffer bases. */
static inline uint32_t presentation_origin(uint32_t origin,bool interlaced,bool odd_field,uint32_t stride) {
    return interlaced && !odd_field && origin>=stride?origin-stride:origin;
}
/* Sample the displayed framebuffer once per VI refresh/field. */
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
