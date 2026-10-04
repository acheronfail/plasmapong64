#ifndef FLUID_QUEUE_H
#define FLUID_QUEUE_H
#include <libdragon.h>

#ifdef PLASMAPONG_FLUID_HIGHPRI
extern bool fluid_highpri_active;
void fluid_pressure_rsp_init(void);
void fluid_dye_rsp_init(void);
void fluid_prepare_rsp_init(void);
void fluid_confinement_rsp_init(void);
#endif

#ifdef PLASMAPONG_FLUID_HIGHPRI_YIELD
extern bool fluid_highpri_open;
static inline void fluid_queue_begin(void) {
    if(fluid_highpri_active && !fluid_highpri_open) {
        rspq_highpri_begin();
        fluid_highpri_open=true;
    }
}
static inline void fluid_queue_highpri_finish(void) {
    if(fluid_highpri_open) {
        rspq_noop();
        rspq_highpri_end(); rspq_highpri_sync();
        fluid_highpri_open=false;
    }
}
#else
static inline void fluid_queue_begin(void) {}
#endif

/* High-priority queues cannot contain syncpoints. Close and synchronize the
   current batch before the CPU reads DMA output. The yielding scheduler leaves
   it closed; the historical comparison reopens its queue context.
   Drawing and boot fixtures retain their ordinary queue synchronization. */
static inline void fluid_queue_wait(void) {
#ifdef PLASMAPONG_FLUID_HIGHPRI
    if(fluid_highpri_active) {
#ifdef PLASMAPONG_FLUID_HIGHPRI_YIELD
        fluid_queue_highpri_finish();
#else
        /* Public writes check buffer rollover; highpri_end appends directly. */
        rspq_noop();
        rspq_highpri_end(); rspq_highpri_sync(); rspq_highpri_begin();
#endif
        return;
    }
#endif
    rspq_syncpoint_t done=rspq_syncpoint_new();
    rspq_flush(); rspq_syncpoint_wait(done);
}
#endif
