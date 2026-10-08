#ifndef FLUID_PRESSURE_H
#define FLUID_PRESSURE_H
#include "fluid.h"
/* Configured lexicographic Q12 passes (default eight) from zero pressure,
   including Neumann walls. CPU and RSP use the same configured pass count.
   Nonoverlapping arrays contain FN words, aligned to 16 bytes.
   |divergence| <= 32760*4096. */
/* Experimental PRESSURE_WARM_START uses the previous public Q12 output as
   its initial guess; initialize that array to zero for a cold first solve. */
void fluid_pressure_cpu(int32_t *pressure,const int32_t *divergence);
#ifdef PLASMAPONG_FLUID_RSP
/* Synchronous at the public boundary; uses the shared RSPQ command queue. */
void fluid_pressure_rsp(int32_t *pressure,const int32_t *divergence);
/* Queue-only producer/consumer variant. Caller must finish the shared queue
   and invalidate pressure before reading it on CPU. */
void fluid_pressure_rsp_begin(int32_t *pressure,const int32_t *divergence);
#if PLASMAPONG_PRESSURE_FAST_GRADIENT
void fluid_pressure_short_rsp_begin(int32_t *pressure,const int32_t *divergence,int16_t *short_pressure);
#endif
#endif
#endif
