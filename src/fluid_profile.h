#ifndef FLUID_PROFILE_H
#define FLUID_PROFILE_H
/* Opt-in N64 elapsed-time instrumentation; absent from production builds.
   Primary stages are disjoint. Named upwind components are nested diagnostics
   and must not be added to those primary totals. Interrupt/audio time is included. */
#ifdef PLASMAPONG_FLUID_PROFILE
#include <libdragon.h>
enum {
    PROFILE_VELOCITY_ADVECTION, PROFILE_VELOCITY_SWAP, PROFILE_DIVERGENCE, PROFILE_PRESSURE,
    PROFILE_GRADIENT, PROFILE_DYE_ADVECTION, PROFILE_DYE_SWAP,
    PROFILE_SPLAT, PROFILE_PUMP, PROFILE_BALL_DYE, PROFILE_SAMPLE,
#ifdef PLASMAPONG_UPWIND_RSP
    PROFILE_UPWIND_LIMIT_VELOCITY, PROFILE_UPWIND_JOB_VELOCITY,
    PROFILE_UPWIND_LIMIT_DYE, PROFILE_UPWIND_JOB_DYE,
#endif
#ifdef PLASMAPONG_QUEUE_PROFILE
    PROFILE_QUEUE_WAIT,
#endif
    PROFILE_COUNT
};
typedef struct {
    bool enabled;
    uint64_t ticks[PROFILE_COUNT];
    unsigned calls[PROFILE_COUNT];
} FluidProfile;
extern FluidProfile fluid_profile;
#ifdef PLASMAPONG_QUEUE_PC_PROFILE
void fluid_queue_pc_report(void);
#endif
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
