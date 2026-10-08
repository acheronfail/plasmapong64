#ifndef FLUID_CONFIG_H
#define FLUID_CONFIG_H
#ifndef PLASMAPONG_GRID_W
#define PLASMAPONG_GRID_W 64
#endif
#ifndef PLASMAPONG_GRID_H
#define PLASMAPONG_GRID_H 44
#endif
#ifndef PLASMAPONG_CELL_Q4
#define PLASMAPONG_CELL_Q4 72
#endif
/* Shared official defaults for portable checks and ROM builds. */
#ifndef PLASMAPONG_PRESSURE_PASSES
#define PLASMAPONG_PRESSURE_PASSES 1
#endif
#if PLASMAPONG_PRESSURE_PASSES < 1 || PLASMAPONG_PRESSURE_PASSES > 8
#error pressure passes must be between one and eight
#endif
#endif
