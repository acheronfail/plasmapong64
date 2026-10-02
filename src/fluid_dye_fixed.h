#ifndef FLUID_DYE_FIXED_H
#define FLUID_DYE_FIXED_H
#include "fluid.h"
/* Big-endian halfwords on N64; source byte offset, then Q15 fractions. */
typedef struct { uint16_t offset,tx,ty; } FluidDyeTrace;
/* Finite dye in [0,3], finite velocities, non-overlapping buffers. */
void fluid_dye_pack(FluidDyeFixed *restrict packed,FluidDyeTrace *restrict trace,
        const FluidDye *restrict ink,const FluidVelocity *restrict velocity,float grid_dt);
void fluid_dye_unpack(FluidDye *restrict ink,const FluidDyeFixed *restrict packed);
void fluid_dye_trace(FluidDyeTrace *restrict trace,const FluidVelocity *restrict velocity,float grid_dt);
void fluid_advect_ink_reference(FluidDyeFixed *next,const FluidDyeFixed *ink,
        const FluidVelocity *velocity,float grid_dt,float decay,float gold_decay,unsigned rounding);
/* Rounded Q15, including the exactly represented unity value 32768. */
unsigned fluid_dye_decay(float decay);
/* rounding is a Q16 threshold in [0,65535], varied deterministically per step.
   Interpolation uses nearest rounding; decay adds this threshold before shift. */
void fluid_dye_fixed_reference(FluidDyeFixed *restrict next,const FluidDyeFixed *restrict ink,
        const FluidDyeTrace *restrict trace,unsigned decay,unsigned gold_decay,unsigned rounding);
#ifdef PLASMAPONG_DYE_RSP
/* Synchronous DMA API. Disjoint buffers must own complete 16-byte cache lines. */
void fluid_dye_fixed_rsp(FluidDyeFixed *next,const FluidDyeFixed *ink,
        const FluidDyeTrace *trace,unsigned decay,unsigned gold_decay,unsigned rounding);
/* Main-thread-only; trace scratch is shared between synchronous calls. */
void fluid_advect_ink_rsp(FluidDyeFixed *next,const FluidDyeFixed *ink,
        const FluidVelocity *velocity,float grid_dt,float decay,float gold_decay,unsigned rounding);
#endif
#endif
