#ifndef FLUID_DYE_FIXED_H
#define FLUID_DYE_FIXED_H
#include "fluid.h"
/* Eight-cell blocks match RSP vector loads/stores. Source byte offsets and
   Q15 fractions retain the same values; only their memory order changes. */
enum { FLUID_TRACE_LANES=8, FLUID_TRACE_BATCHES=FN/FLUID_TRACE_LANES };
typedef struct { uint16_t offset[8],tx[8],ty[8]; } FluidDyeTrace;
_Static_assert(FN%FLUID_TRACE_LANES==0,"complete trace vectors");
/* Finite dye in [0,3], finite velocities, non-overlapping buffers. */
void fluid_dye_pack(FluidDyeFixed *restrict packed,FluidDyeTrace *restrict trace,
        const FluidDye *restrict ink,const FluidVelocity *restrict velocity,float grid_dt);
void fluid_dye_unpack(FluidDye *restrict ink,const FluidDyeFixed *restrict packed);
void fluid_dye_trace(FluidDyeTrace *restrict trace,const FluidVelocity *restrict velocity,float grid_dt);
void fluid_advect_ink_reference(FluidDyeFixed *next,const FluidDyeFixed *ink,
        const FluidFlow *velocity,float grid_dt,float decay,float gold_decay,unsigned rounding);
/* Rounded Q15, including the exactly represented unity value 32768. */
unsigned fluid_dye_decay(float decay);
/* rounding is a Q16 threshold in [0,65535], varied deterministically per step.
   Interpolation uses nearest rounding; decay adds this threshold before shift. */
void fluid_dye_fixed_reference(FluidDyeFixed *restrict next,const FluidDyeFixed *restrict ink,
        const FluidDyeTrace *restrict trace,unsigned decay,unsigned gold_decay,unsigned rounding);
#ifdef PLASMAPONG_DYE_RSP
/* Signed samples must stay within +/-16383, or nonnegative samples within
   [0,32767], so interpolation differences never overflow signed lanes. */
void fluid_channels_rsp(int16_t *next,const int16_t *source,const FluidDyeTrace *trace,
    unsigned channels,const unsigned *decays,unsigned rounding);
/* Synchronous DMA API. Disjoint buffers must own complete 16-byte cache lines. */
void fluid_dye_fixed_rsp(FluidDyeFixed *next,const FluidDyeFixed *ink,
        const FluidDyeTrace *trace,unsigned decay,unsigned gold_decay,unsigned rounding);
/* Main-thread-only; trace scratch is shared between synchronous calls. */
void fluid_advect_ink_rsp(FluidDyeFixed *next,const FluidDyeFixed *ink,
        const FluidFlow *velocity,float grid_dt,float decay,float gold_decay,unsigned rounding);
#endif
#endif
