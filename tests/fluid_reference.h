/* Frozen pre-optimization scalar solver interface for differential tests. */
#ifndef REFERENCE_FLUID_H
#define REFERENCE_FLUID_H
#include <stdint.h>
#define FW 48
#define FH 30
#define FN (FW * FH)
#define CELL 6.0f
#define ARENA_W (FW * CELL)
#define ARENA_H (FH * CELL)
typedef struct {
    float u[FN], v[FN], red[FN], blue[FN], gold[FN];
    float tu[FN], tv[FN], tr[FN], tb[FN], pressure[FN], divergence[FN];
} ReferenceFluid;
void reference_fluid_init(ReferenceFluid *f);
void reference_fluid_velocity_step(ReferenceFluid *f, float dt);
/* Apply pumps between velocity and dye steps so their flow transports dye. */
void reference_fluid_dye_step(ReferenceFluid *f, float dt);
void reference_fluid_ball_dye(ReferenceFluid *f, float x, float y, float amount);
void reference_fluid_project(ReferenceFluid *f);
void reference_fluid_sample(const ReferenceFluid *f, float x, float y, float *u, float *v);
void reference_fluid_splat(ReferenceFluid *f, float x, float y, float radius, float u, float v, float dye, int player);
void reference_fluid_pump(ReferenceFluid *f, float x, float y, float radius, float strength, float dt, int player);
uint32_t reference_fluid_color(const ReferenceFluid *f, int i);
#endif
