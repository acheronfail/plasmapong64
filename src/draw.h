#ifndef DRAW_H
#define DRAW_H
#include "game.h"
/* Match the gold dye contribution in fluid_color(). */
#define MENU_GOLD 0xffcd19
#define MENU_SELECTED_STYLE 9
#ifdef PLASMAPONG_FLUID_PROFILE
extern uint64_t flow_prepare_ticks,flow_emit_ticks;
#endif
void rect(float x,float y,float w,float h,uint32_t c);
typedef struct { uint16_t x,y; } DrawPoint;
enum {
    DRAW_POINT_LAYERS=5,
    DRAW_POINT_TINTS=6,
    /* Each nonempty tint batch adds a pipe sync and fill-color command. */
    DRAW_POINT_COMMAND_CAPACITY=DRAW_POINT_LAYERS*(FLOW_TRACERS+2*DRAW_POINT_TINTS)
};
/* At most five layers of FLOW_TRACERS in a frame; screen-clipped pixels. */
void draw_points(const DrawPoint *points,unsigned count,uint32_t c);
void draw_points_end(void);
void label(float x,float y,int style,const char *s);
void label_edge(float x,float y,int style,const char *s,bool right);
float label_width(const char *s);
void draw_fluid(const Fluid *f,float x,float y,float width,float height,FlowEffect effect);
enum { DRAW_MENU_TITLE, DRAW_COURT, DRAW_SQUARE_MASK, DRAW_STATIC_COUNT };
/* Callbacks must contain only immutable drawing commands. */
void draw_static(unsigned id,void (*draw)(void));
void ui_measure_menu(Game *g);
void ui_init(void);
void ui_draw(const Game *g);
void ui_menu_foreground(const Game *g);
void ui_menu_particles(const Game *g);
#ifdef PLASMAPONG_MENU_LABEL_BLOCK
void draw_menu_foreground(const Game *g);
#endif
#endif
