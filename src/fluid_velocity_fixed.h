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
#endif
