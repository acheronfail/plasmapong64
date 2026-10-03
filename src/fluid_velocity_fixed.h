#ifndef FLUID_VELOCITY_FIXED_H
#define FLUID_VELOCITY_FIXED_H
#include "fluid_dye_fixed.h"
/* Signed Q4, restricted to +/-16383 so every interpolation difference fits
   a signed RSP lane. Saturation range is +/-1023.9375 pixels/second. */
void fluid_velocity_pack(FluidVelocityFixed *packed,FluidDyeTrace *trace,
    const FluidVelocity *velocity,float grid_dt);
void fluid_velocity_unpack(FluidVelocity *next,const FluidVelocityFixed *packed);
void fluid_velocity_fixed_reference(FluidVelocityFixed *next,const FluidVelocityFixed *packed,
    const FluidDyeTrace *trace,unsigned decay,unsigned rounding);
void fluid_velocity_trace(FluidDyeTrace *trace,const FluidVelocityFixed *velocity,float grid_dt);
void fluid_advect_velocity_fixed(FluidVelocityFixed *next,const FluidVelocityFixed *velocity,
    float grid_dt,float decay,unsigned rounding);
#ifdef PLASMAPONG_PREPARE_RSP
/* Main-thread-only DMA APIs; synchronous except for the queue-only variant.
   Inputs/outputs own full cache lines.
   Trace preserves the CPU Q20 coefficient, Q12 clamp and Q15 fractions.
   Pixels require a 16-byte-aligned buffer with stride divisible by four.
   Divergence replaces interior rows (unused side columns become zero). */
/* Queue-only variant for trace -> interpolation on the same RSPQ queue.
   Do not access trace on CPU until the consuming interpolation call finishes. */
void fluid_velocity_trace_rsp_begin(FluidDyeTrace *trace,const FluidVelocityFixed *velocity,float grid_dt);
void fluid_velocity_trace_rsp(FluidDyeTrace *trace,const FluidVelocityFixed *velocity,float grid_dt);
/* Gradient accepts |pressure| <= 32760*4096 and enforces zero normal walls. */
void fluid_gradient_rsp(FluidVelocityFixed *velocity,const int32_t *pressure);
void fluid_divergence_rsp(int32_t *divergence,const FluidVelocityFixed *velocity);
void fluid_divergence_rsp_begin(int32_t *divergence,const FluidVelocityFixed *velocity);
/* Chained divergence -> pressure -> gradient, synchronous at this boundary. */
void fluid_projection_rsp(FluidVelocityFixed *velocity,int32_t *divergence,int32_t *pressure);
void fluid_speed_pixels_rsp(const Fluid *f,uint32_t *pixels,unsigned stride);
void fluid_pixels_rsp(const Fluid *f,uint32_t *pixels,unsigned stride);
#endif
#endif
