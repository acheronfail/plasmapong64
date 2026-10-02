#ifndef DRAW_H
#define DRAW_H
#include "game.h"
void rect(float x,float y,float w,float h,uint32_t c);
void label(float x,float y,int style,const char *s);
void draw_fluid(const Fluid *f);
void ui_draw(const Game *g);
#endif
