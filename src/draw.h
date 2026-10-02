#ifndef DRAW_H
#define DRAW_H
#include "game.h"
void rect(float x,float y,float w,float h,uint32_t c);
void label(float x,float y,int style,const char *s);
void label_edge(float x,float y,int style,const char *s,bool right);
float label_width(const char *s);
void draw_flow_contours(const uint32_t *pixels,unsigned stride,float x,float y,float width,float height);
void draw_fluid(const Fluid *f,float x,float y,float width,float height,bool speed);
enum { DRAW_MENU_TITLE, DRAW_COURT, DRAW_STATIC_COUNT };
/* Callbacks must contain only immutable drawing commands. */
void draw_static(unsigned id,void (*draw)(void));
void ui_draw(const Game *g);
#endif
