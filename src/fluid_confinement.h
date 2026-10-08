#ifndef FLUID_CONFINEMENT_H
#define FLUID_CONFINEMENT_H
#include "fluid.h"
/* Curl stores round((dv/dx - du/dy)/2) in velocity Q4 grid units.
   Physical curl = sample / (CELL * VELOCITY_SCALE). Inputs stay within +/-VELOCITY_LIMIT. */
void fluid_curl_fixed(int16_t *curl,const FluidVelocityFixed *velocity);
unsigned fluid_confinement_strength(float dt);
static inline unsigned fluid_confinement_first(unsigned phase) {
    return 2+(FH-4)*(phase%FLUID_CONFINEMENT_BANDS)/FLUID_CONFINEMENT_BANDS;
}
static inline unsigned fluid_confinement_rows(unsigned phase) {
    unsigned band=phase%FLUID_CONFINEMENT_BANDS;
    return (FH-4)*(band+1)/FLUID_CONFINEMENT_BANDS-(FH-4)*band/FLUID_CONFINEMENT_BANDS;
}
void fluid_confinement_window_fixed(FluidVelocityFixed *velocity,const int16_t *curl,
    unsigned strength,unsigned first,unsigned rows);
void fluid_confinement_fixed(FluidVelocityFixed *velocity,const int16_t *curl,unsigned strength);
#ifdef PLASMAPONG_CONFINEMENT_RSP
void fluid_confinement_rsp_init(void);
void fluid_curl_rsp(int16_t *curl,const FluidVelocityFixed *velocity);
void fluid_confinement_rsp(FluidVelocityFixed *velocity,const int16_t *curl,unsigned strength);
/* Consume RSP-owned velocity; caller owns the final synchronization. */
void fluid_curl_confinement_window_rsp_begin(FluidVelocityFixed *velocity,int16_t *curl,
    unsigned strength,unsigned first,unsigned rows);
void fluid_curl_confinement_rsp_begin(FluidVelocityFixed *velocity,int16_t *curl,unsigned strength);
/* Curl -> confinement without an intermediate CPU synchronization. */
void fluid_curl_confinement_rsp(FluidVelocityFixed *velocity,int16_t *curl,unsigned strength);
#endif
#endif
