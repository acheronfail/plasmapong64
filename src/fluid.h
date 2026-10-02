#ifndef FLUID_H
#define FLUID_H
#include <stdint.h>
#define FW 48
#define FH 33
#define FN (FW * FH)
#define CELL 6.0f
#define ARENA_W (FW * CELL)
#define ARENA_H (FH * CELL)
/* Whole grids and rows start on 16-byte cache-line boundaries. Index-based
   ping-pong buffers keep Fluid/Game safe to copy by value (no self-pointers). */
typedef struct { _Alignas(16) float u[FN],v[FN]; } FluidVelocity;
enum { VELOCITY_SCALE=16, VELOCITY_LIMIT=16383 };
typedef struct { _Alignas(16) int16_t u[FN],v[FN]; } FluidVelocityFixed;
#ifdef PLASMAPONG_VELOCITY_FIXED
typedef FluidVelocityFixed FluidFlow;
typedef int16_t FluidFlowValue;
static inline FluidFlowValue fluid_flow_clamp(int v) {
    return (int16_t)(v>VELOCITY_LIMIT?VELOCITY_LIMIT:v<-VELOCITY_LIMIT?-VELOCITY_LIMIT:v);
}
static inline float fluid_flow_decode(FluidFlowValue v) { return v*(1.0f/VELOCITY_SCALE); }
static inline FluidFlowValue fluid_flow_encode(float v) {
    v*=VELOCITY_SCALE;
    if(v>VELOCITY_LIMIT) return VELOCITY_LIMIT;
    if(v<-VELOCITY_LIMIT) return -VELOCITY_LIMIT;
    return (int16_t)(v+(v<0?-.5f:.5f));
}
#else
typedef FluidVelocity FluidFlow;
typedef float FluidFlowValue;
static inline float fluid_flow_decode(FluidFlowValue v) { return v; }
static inline FluidFlowValue fluid_flow_encode(float v) { return v; }
#endif
typedef struct { _Alignas(16) float red[FN],blue[FN],gold[FN]; } FluidDye;
enum { DYE_SCALE=8192, DYE_WEIGHT_SCALE=32768 };
typedef struct { _Alignas(16) int16_t red[FN],blue[FN],gold[FN]; } FluidDyeFixed;
#ifdef PLASMAPONG_DYE_FIXED
typedef FluidDyeFixed FluidInk;
typedef int16_t FluidInkValue;
static inline float fluid_ink_decode(FluidInkValue v) { return v*(1.0f/DYE_SCALE); }
static inline FluidInkValue fluid_ink_encode(float v) { return (int16_t)(v*DYE_SCALE+.5f); }
#else
typedef FluidDye FluidInk;
typedef float FluidInkValue;
static inline float fluid_ink_decode(FluidInkValue v) { return v; }
static inline FluidInkValue fluid_ink_encode(float v) { return v; }
#endif
typedef struct {
    FluidFlow velocity[2];
    FluidInk dye[2];
    _Alignas(16) int32_t pressure[FN];
    _Alignas(16) union { float curl[FN]; int32_t divergence[FN]; };
    unsigned velocity_bank,dye_bank;
#ifdef PLASMAPONG_VELOCITY_FIXED
    unsigned velocity_phase;
#endif
#ifdef PLASMAPONG_DYE_FIXED
    unsigned dye_phase;
#endif
} Fluid;
/* Reacquire these views after the corresponding velocity/dye step. */
#define fluid_velocity(f) (&(f)->velocity[(f)->velocity_bank])
#define fluid_dye(f) (&(f)->dye[(f)->dye_bank])
void fluid_init(Fluid *f);
void fluid_velocity_step(Fluid *f, float dt);
/* Apply pumps between velocity and dye steps so their flow transports dye. */
void fluid_dye_step(Fluid *f, float dt);
void fluid_ball_dye(Fluid *f, float x, float y, float amount);
void fluid_hot_ball_dye(Fluid *f, float x, float y, float amount);
void fluid_project(Fluid *f);
void fluid_sample(const Fluid *f, float x, float y, float *u, float *v);
void fluid_splat(Fluid *f, float x, float y, float radius, float u, float v, float dye, int player);
void fluid_pump(Fluid *f, float x, float y, float radius, float strength, float dt, int player);
uint32_t fluid_color(const Fluid *f, int i);
/* Write an RGBA32 texture; stride is in pixels and must be at least FW. */
void fluid_pixels(const Fluid *f, uint32_t *pixels, unsigned stride);
#endif
