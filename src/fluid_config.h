#ifndef FLUID_CONFIG_H
#define FLUID_CONFIG_H
#ifndef PLASMAPONG_GRID_W
#define PLASMAPONG_GRID_W 48
#endif
#ifndef PLASMAPONG_GRID_H
#define PLASMAPONG_GRID_H 33
#endif
#ifndef PLASMAPONG_CELL_Q4
#define PLASMAPONG_CELL_Q4 96
#endif
#ifndef PLASMAPONG_PRESSURE_Q3
#define PLASMAPONG_PRESSURE_Q3 0
#endif
#ifndef PLASMAPONG_PRESSURE_FAST_GRADIENT
#define PLASMAPONG_PRESSURE_FAST_GRADIENT 0
#endif
#ifndef PLASMAPONG_PRESSURE_WARM_START
#define PLASMAPONG_PRESSURE_WARM_START 0
#endif
/* Shared C/RSP compile-time settings; eight passes remain the shipping control. */
#ifndef PLASMAPONG_PRESSURE_PASSES
#define PLASMAPONG_PRESSURE_PASSES 8
#endif
#if PLASMAPONG_PRESSURE_PASSES < 1 || PLASMAPONG_PRESSURE_PASSES > 8
#error pressure passes must be between one and eight
#endif
#endif
