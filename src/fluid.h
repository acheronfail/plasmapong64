#ifndef FLUID_H
#define FLUID_H
#include <stdint.h>
#define FW 48
#define FH 30
#define FN (FW * FH)
#define CELL 6.0f
#define ARENA_W (FW * CELL)
#define ARENA_H (FH * CELL)
typedef struct {
    float u[FN], v[FN], red[FN], blue[FN];
    float tu[FN], tv[FN], tr[FN], tb[FN], pressure[FN], divergence[FN];
} Fluid;
void fluid_init(Fluid *f);
void fluid_step(Fluid *f, float dt);
void fluid_project(Fluid *f);
void fluid_sample(const Fluid *f, float x, float y, float *u, float *v);
void fluid_splat(Fluid *f, float x, float y, float radius, float u, float v, float dye, int player);
void fluid_pump(Fluid *f, float x, float y, float radius, float strength, float dt, int player);
uint32_t fluid_color(const Fluid *f, int i);
#endif
