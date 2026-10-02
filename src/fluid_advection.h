#ifndef FLUID_ADVECTION_H
#define FLUID_ADVECTION_H
#include "fluid.h"
/* Finite inputs, disjoint source/destination banks. Semi-Lagrangian sampling
   retains the float grid, separable interpolation and clamped edge policy.
   Keep these kernels in their own translation unit: the pinned MIPS compiler
   emits simpler addresses when bank selection is outside the hot loop. */
void fluid_advect_velocity(FluidVelocity *restrict next,
        const FluidVelocity *restrict velocity,float grid_dt,float decay);
void fluid_advect_dye(FluidDye *restrict next,const FluidDye *restrict ink,
        const FluidVelocity *restrict velocity,float grid_dt,float decay,float gold_decay);
#endif
