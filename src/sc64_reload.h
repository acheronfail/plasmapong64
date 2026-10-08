#ifndef PLASMAPONG_SC64_RELOAD_H
#define PLASMAPONG_SC64_RELOAD_H
#ifdef PLASMAPONG_SC64_RELOAD
void sc64_reload_poll(void);
#else
static inline void sc64_reload_poll(void) {}
#endif
#endif
