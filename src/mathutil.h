#ifndef MATHUTIL_H
#define MATHUTIL_H
/* MIPS III has no floating min/max instruction. Avoid out-of-line C99 libm
   NaN-handling calls in tight loops: all simulation inputs are finite. */
static inline float minf(float a,float b) { return a<b?a:b; }
static inline float maxf(float a,float b) { return a>b?a:b; }
static inline float clampf(float a,float lo,float hi) { return a<lo?lo:(a>hi?hi:a); }
#endif
