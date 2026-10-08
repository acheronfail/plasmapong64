#ifndef FLUID_PRESSURE_H
#define FLUID_PRESSURE_H
#include "fluid.h"
/* Compact Q3 pressure sweeps with a public Q12 field and Neumann walls.
   Previous pressure seeds the next solve; initialize it to zero for a cold start.
   Nonoverlapping arrays contain FN words, aligned to 16 bytes.
   |divergence| <= 32760*4096. CPU and RSP share the configured sweep count. */
void fluid_pressure_cpu(int32_t *pressure,const int32_t *divergence);
#ifdef PLASMAPONG_FLUID_RSP
/* Synchronous at the public boundary; uses the shared RSPQ command queue. */
void fluid_pressure_rsp(int32_t *pressure,const int32_t *divergence);
/* Queue-only producer/consumer variant. Caller must finish the shared queue
   and invalidate pressure before reading it on CPU. */
void fluid_pressure_rsp_begin(int32_t *pressure,const int32_t *divergence);
/* Compact path: short_pressure supplies the warm Q3 state and is updated.
   pressure is its Q12 output mirror; initialize both consistently. */
void fluid_pressure_short_rsp_begin(int32_t *pressure,const int32_t *divergence,int16_t *short_pressure);
#endif
#endif
