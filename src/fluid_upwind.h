#ifndef FLUID_UPWIND_H
#define FLUID_UPWIND_H
#include "fluid.h"
/* Local-grid advection. The shared limiter bounds combined speed
   so |u|dt/h + |v|dt/h <=1. Dye and ball then see the same bounded velocity. */
unsigned fluid_upwind_step(float grid_dt);
unsigned fluid_upwind_limit(unsigned step);
unsigned fluid_upwind_safe_limit(unsigned limit);
void fluid_upwind_clamp(FluidVelocityFixed *velocity,unsigned limit);
void fluid_upwind_velocity_cpu(FluidVelocityFixed *next,FluidVelocityFixed *velocity,
    float grid_dt,float decay,unsigned rounding);
void fluid_upwind_ink_cpu(FluidDyeFixed *next,const FluidDyeFixed *ink,FluidVelocityFixed *velocity,
    float grid_dt,float decay,float gold_decay,unsigned rounding);
#ifdef PLASMAPONG_UPWIND_RSP
void fluid_upwind_rsp_init(void);
/* Leaves both velocity banks RSP-owned until the caller synchronizes. */
void fluid_upwind_velocity_rsp_begin(FluidVelocityFixed *next,FluidVelocityFixed *velocity,
    float grid_dt,float decay,unsigned rounding);
void fluid_velocity_chain_rsp(Fluid *f,float dt);
void fluid_upwind_velocity_rsp(FluidVelocityFixed *next,FluidVelocityFixed *velocity,
    float grid_dt,float decay,unsigned rounding);
void fluid_upwind_ink_rsp(FluidDyeFixed *next,const FluidDyeFixed *ink,FluidVelocityFixed *velocity,
    float grid_dt,float decay,float gold_decay,unsigned rounding);
#endif
#endif
