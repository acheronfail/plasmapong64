#ifndef FLUID_VELOCITY_FIXED_H
#define FLUID_VELOCITY_FIXED_H
#include "fluid_dye_fixed.h"
#ifdef PLASMAPONG_PREPARE_RSP
/* DMA buffers own complete cache lines. Queue-only calls require a final wait. */
void fluid_divergence_rsp(int32_t *divergence,const FluidVelocityFixed *velocity);
#ifdef PLASMAPONG_VELOCITY_CHAIN
void fluid_divergence_walled_rsp_begin(int32_t *divergence,const FluidVelocityFixed *velocity);
#endif
void fluid_divergence_rsp_begin(int32_t *divergence,const FluidVelocityFixed *velocity);
void fluid_gradient_short_rsp_init(void);
void fluid_gradient_short_rsp(FluidVelocityFixed *velocity,const int16_t *pressure);
void fluid_projection_short_rsp(FluidVelocityFixed *velocity,int32_t *divergence,int32_t *pressure,int16_t *short_pressure);
void fluid_pixels_rsp(const Fluid *f,uint32_t *pixels,unsigned stride);
/* Queue-only texture producers. Subsequent RDP uploads must use the same
   RSPQ queue; finish it before CPU access or reusing the destination. */
void fluid_pixels_rsp_begin(const Fluid *f,uint32_t *pixels,unsigned stride);
void fluid_speed_field_rsp_begin(const Fluid *f,uint32_t *pixels,unsigned stride);
void fluid_pixels16_rsp_begin(const Fluid *f,uint16_t *pixels,unsigned stride);
#endif
#endif
