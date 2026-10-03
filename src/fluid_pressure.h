#ifndef FLUID_PRESSURE_H
#define FLUID_PRESSURE_H
#include "fluid.h"
/* Eight lexicographic Q12 passes from zero pressure, including Neumann walls.
   Nonoverlapping arrays contain FN words, aligned to 16 bytes.
   |divergence| <= 32760*4096. */
void fluid_pressure_cpu(int32_t *pressure,const int32_t *divergence);
#ifdef PLASMAPONG_FLUID_RSP
/* Synchronous at the public boundary; uses the shared RSPQ command queue. */
void fluid_pressure_rsp(int32_t *pressure,const int32_t *divergence);
/* Queue-only producer/consumer variant. Caller must finish the shared queue
   and invalidate pressure before reading it on CPU. */
void fluid_pressure_rsp_begin(int32_t *pressure,const int32_t *divergence);
#endif
#endif
