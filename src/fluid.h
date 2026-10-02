#ifndef FLUID_H
#define FLUID_H
#include <stdint.h>
#define FW 48
#define FH 30
#define FN (FW * FH)
#define CELL 6.0f
#define ARENA_W (FW * CELL)
#define ARENA_H (FH * CELL)
/* Whole grids and rows start on 16-byte cache-line boundaries. Index-based
   ping-pong buffers keep Fluid/Game safe to copy by value (no self-pointers). */
typedef struct { _Alignas(16) float u[FN],v[FN]; } FluidVelocity;
typedef struct { _Alignas(16) float red[FN],blue[FN],gold[FN]; } FluidDye;
typedef struct {
    FluidVelocity velocity[2];
    FluidDye dye[2];
    _Alignas(16) int32_t pressure[FN];
    _Alignas(16) union { float curl[FN]; int32_t divergence[FN]; };
    unsigned velocity_bank,dye_bank;
} Fluid;
/* Reacquire these views after the corresponding velocity/dye step. */
#define fluid_velocity(f) (&(f)->velocity[(f)->velocity_bank])
#define fluid_dye(f) (&(f)->dye[(f)->dye_bank])
void fluid_init(Fluid *f);
void fluid_velocity_step(Fluid *f, float dt);
/* Apply pumps between velocity and dye steps so their flow transports dye. */
void fluid_dye_step(Fluid *f, float dt);
void fluid_ball_dye(Fluid *f, float x, float y, float amount);
void fluid_project(Fluid *f);
void fluid_sample(const Fluid *f, float x, float y, float *u, float *v);
void fluid_splat(Fluid *f, float x, float y, float radius, float u, float v, float dye, int player);
void fluid_pump(Fluid *f, float x, float y, float radius, float strength, float dt, int player);
uint32_t fluid_color(const Fluid *f, int i);
/* Write an RGBA32 texture; stride is in pixels and must be at least FW. */
void fluid_pixels(const Fluid *f, uint32_t *pixels, unsigned stride);
#endif
