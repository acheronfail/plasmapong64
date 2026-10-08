#ifndef FLUID_H
#define FLUID_H
#include <stdint.h>
#include "fluid_config.h"
#define FW PLASMAPONG_GRID_W
#define FH PLASMAPONG_GRID_H
#define FN (FW * FH)
#define CELL (PLASMAPONG_CELL_Q4*(1.0f/16))
/* Momentum loss per second, shared by CPU/RSP paths. */
#ifdef PLASMAPONG_FLOW_DAMPING
#define FLUID_DAMPING ((float)(PLASMAPONG_FLOW_DAMPING))
#else
#define FLUID_DAMPING .12f
#endif
#ifndef PLASMAPONG_FLOW_CONFINEMENT
#define PLASMAPONG_FLOW_CONFINEMENT .75f
#endif
#define FLUID_CONFINEMENT ((float)PLASMAPONG_FLOW_CONFINEMENT)
/* Spread restoration across four row bands; transport/projection still run at 60 Hz. */
#define FLUID_CONFINEMENT_BANDS 4
#ifndef PLASMAPONG_JET_RADIUS
#define PLASMAPONG_JET_RADIUS 14
#endif
#ifndef PLASMAPONG_JET_FORCE
#define PLASMAPONG_JET_FORCE 1800.0f
#endif
/* Double the compensated pigment dose to make transported dye more visible. */
#ifndef PLASMAPONG_JET_DYE
#define PLASMAPONG_JET_DYE 12.8f
#endif
#define ARENA_W (FW * CELL)
#define ARENA_H (FH * CELL)
/* Whole grids and rows start on 16-byte cache-line boundaries. Index-based
   ping-pong buffers keep Fluid/Game safe to copy by value (no self-pointers). */
enum { VELOCITY_SCALE=16, VELOCITY_LIMIT=16383 };
typedef struct { _Alignas(16) int16_t u[FN],v[FN]; } FluidVelocityFixed;
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
enum { DYE_SCALE=8192, DYE_WEIGHT_SCALE=32768 };
typedef struct { _Alignas(16) int16_t red[FN],blue[FN],gold[FN]; } FluidDyeFixed;
typedef FluidDyeFixed FluidInk;
typedef int16_t FluidInkValue;
static inline float fluid_ink_decode(FluidInkValue v) { return v*(1.0f/DYE_SCALE); }
static inline FluidInkValue fluid_ink_encode(float v) { return (int16_t)(v*DYE_SCALE+.5f); }
typedef struct {
    FluidFlow velocity[2];
    FluidInk dye[2];
    _Alignas(16) int32_t pressure[FN];
    _Alignas(16) int16_t pressure_short[FN];
    _Alignas(16) int32_t divergence[FN];
    _Alignas(16) int16_t curl_fixed[FN];
    unsigned velocity_bank,dye_bank;
    unsigned velocity_phase;
    unsigned dye_phase;
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
#ifdef PLASMAPONG_MENU_STAMPS
/* Fixed menu geometry; slot identifies one of the 26 label emitters. */
void fluid_menu_source(Fluid *f,unsigned slot,float x,float y,float u,float v,float gold);
#endif
void fluid_project(Fluid *f);
void fluid_sample(const Fluid *f, float x, float y, float *u, float *v);
void fluid_splat(Fluid *f, float x, float y, float radius, float u, float v, float dye, int player);
void fluid_pump(Fluid *f, float x, float y, float radius, float strength, float dt, int player);
#define FLUID_SPEED_PALETTE_SIZE 260 /* 257 colors plus complete DMA cache-line padding. */
void fluid_speed_palette(uint32_t *rgba);
void fluid_speed_field_pixels(const Fluid *f,uint32_t *pixels,unsigned stride);
void fluid_speed_pixels(const Fluid *f, uint32_t *pixels, unsigned stride);
uint32_t fluid_speed_color(const Fluid *f, int i);
typedef enum { FLUID_VIEW_DYE, FLUID_VIEW_SPEED, FLUID_VIEW_VORTEX,
    FLUID_VIEW_BANDS, FLUID_VIEW_PRESSURE } FluidView;
/* Visual-only colour producers. No changes to simulation fields. */
void fluid_view_palette(FluidView view, uint32_t *rgba);
uint32_t fluid_view_color(const Fluid *f, int i, FluidView view);
void fluid_view_pixels(const Fluid *f, uint32_t *pixels, unsigned stride, FluidView view);
uint32_t fluid_color(const Fluid *f, int i);
/* Write an RGBA32 texture; stride is in pixels and must be at least FW. */
void fluid_pixels(const Fluid *f, uint32_t *pixels, unsigned stride);
#endif
