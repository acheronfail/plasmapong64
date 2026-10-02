#ifndef FLUID_CONFINEMENT_H
#define FLUID_CONFINEMENT_H
#include "fluid.h"
/* Curl stores round((dv/dx - du/dy)/2) in velocity Q4 grid units.
   Physical curl = sample / 96. Inputs stay within +/-VELOCITY_LIMIT. */
void fluid_curl_fixed(int16_t *curl,const FluidVelocityFixed *velocity);
unsigned fluid_confinement_strength(float dt);
void fluid_confinement_fixed(FluidVelocityFixed *velocity,const int16_t *curl,unsigned strength);
#ifdef PLASMAPONG_CONFINEMENT_RSP
void fluid_curl_rsp(int16_t *curl,const FluidVelocityFixed *velocity);
void fluid_confinement_rsp(FluidVelocityFixed *velocity,const int16_t *curl,unsigned strength);
#endif
#endif
