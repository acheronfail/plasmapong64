#ifndef FLUID_PROFILE_H
#define FLUID_PROFILE_H
/* Opt-in N64 elapsed-time instrumentation; absent from production builds.
   Stages are disjoint. Interrupt/audio time remains included. */
#ifdef PLASMAPONG_FLUID_PROFILE
#include <libdragon.h>
enum {
    PROFILE_VELOCITY_ADVECTION, PROFILE_VELOCITY_SWAP, PROFILE_CURL,
    PROFILE_CONFINEMENT, PROFILE_DIVERGENCE, PROFILE_PRESSURE,
    PROFILE_GRADIENT, PROFILE_DYE_ADVECTION, PROFILE_DYE_SWAP,
    PROFILE_SPLAT, PROFILE_PUMP, PROFILE_BALL_DYE, PROFILE_SAMPLE,
    PROFILE_COUNT
};
typedef struct {
    bool enabled;
    uint64_t ticks[PROFILE_COUNT];
    unsigned calls[PROFILE_COUNT];
} FluidProfile;
extern FluidProfile fluid_profile;
#define PROFILE_BEGIN() uint64_t profile_start=get_ticks()
#define PROFILE_END(stage) do { \
    uint64_t profile_end=get_ticks(); \
    if(fluid_profile.enabled) { \
        fluid_profile.ticks[stage]+=profile_end-profile_start; \
        fluid_profile.calls[stage]++; \
    } \
    profile_start=get_ticks(); \
} while(0)
#else
#define PROFILE_BEGIN() ((void)0)
#define PROFILE_END(stage) ((void)0)
#endif
#endif
