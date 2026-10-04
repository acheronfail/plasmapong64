#include "mathutil.h"
#include "draw.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define OX 16
#define OY 34
#define WHITE 0xeaf6ff
#define CYAN 0x48dcff
#define CORAL 0xff637e
#define MINT 0x70ffd0
#define VIOLET 0xc28aff
static const uint32_t player_colors[]={CYAN,CORAL,MINT,VIOLET};
static const uint32_t player_outlines[]={0x164658,0x642739,0x245b50,0x49345f};
static const int player_styles[]={2,3,7,8};
/* Match the ball dye's RGB contribution in fluid_color(). */
#define GOLD MENU_GOLD
static float ring_direction[20][2];
void ui_init(void) {
    /* Angles are invariant. Evaluate them once with the platform's own math
       implementation, retaining identical coordinates on subsequent frames. */
    static bool initialized;
    if(!initialized) {
        for(int i=0;i<20;i++) {
            float a=i*6.2831853f/20;
            ring_direction[i][0]=cosf(a); ring_direction[i][1]=sinf(a);
        }
        initialized=true;
    }
}
static void ring(const Game *g,float x,float y,float radius,uint32_t color) {
    ui_init();
    const float world_left=game_left(g),world_right=game_right(g);
    const bool square=game_square(g);
    for(int i=0;i<20;i++) {
        float px=x+ring_direction[i][0]*radius-1,py=y+ring_direction[i][1]*radius-1;
        if(px<OX+world_left || px>OX+world_right-2 || py<OY || py>OY+ARENA_H-2) continue;
        if(square) {
            float edge_x=minf(px-OX-world_left,OX+world_right-px-2);
            float edge_y=minf(py-OY,OY+ARENA_H-py-2);
            if(edge_x+edge_y<CORNER_SIZE) continue;
        }
        rect(px,py,2,2,color);
    }
}
/* Bracketed control names become N64-coloured keycaps everywhere. */
static float hint_layout(float x,float y,const char *s,bool draw) {
    float start=x;
    while(*s) {
        bool button=*s=='[';
        if(button) s++;
        char part[96]; unsigned n=0;
        while(*s && *s!=(button?']':'[') && n<sizeof(part)-1) part[n++]=*s++;
        part[n]=0;
        if(button && *s==']') s++;
        float w=label_width(part);
        if(button) {
            uint32_t c=!strcmp(part,"A")?0x285dce:!strcmp(part,"START")?0xc63743:!strcmp(part,"B")?0x258846:0x626b78;
            if(draw) {
                rect(x,y-9,w+6,12,c);
                label(x+3,y,4,part);
            }
            x+=w+6;
        } else { if(draw) label(x,y,1,part); x+=w; }
    }
    return x-start;
}
static void hint(float x,float y,const char *s) {
    hint_layout(roundf(x),roundf(y),s,true);
}
static void centered_hint(float y,const char *s) {
    hint(160-hint_layout(0,0,s,false)*.5f,y,s);
}
static void name_entry(const Game *g,float y) {
    const char *initials=g->highs[g->score_entry].initials;
    const char *title="HIGH SCORE!  ";
    /* Two four-pixel gaps keep the active character's highlight separate. */
    float x=roundf(160-(label_width(title)+label_width(initials)+8)*.5f);
    label(x,y,2,title); x+=label_width(title);
    for(unsigned i=0;i<3;i++) {
        char letter[]={initials[i],0};
        float w=label_width(letter);
        bool selected=i==g->initial_cursor;
        /* Keep the glyph bright: dark text merges with the built-in outline.
           Put the selection marker below it so every stroke stays visible. */
        label(x,y,selected?4:2,letter);
        if(selected) rect(x-1,y+3,w+2,2,CYAN);
        x+=w+4;
    }
}
static void panel(const char *title,const char *line1,const char *line2) {
    rect(37,82,246,78,0x243449); rect(39,84,242,74,0x09111f);
    label(52,102,0,title); hint(52,125,line1); hint(52,146,line2);
}
static void menu_label(float y,int style,const char *text) {
    float x=160-label_width(text)*.5f;
    /* A solid offset shadow plus the font outline survives bright dye swirls. */
    label(x+2,y+2,5,text);
    label(x,y,style,text);
}
static void menu_triangle(float x,float y,bool right,uint32_t color) {
    for(int column=0;column<4;column++) {
        float height=right?7-column*2:1+column*2;
        rect(x+column,y-height*.5f,1,height,color);
    }
}
static const char *menu_items[]={"MULTI-PLAYER","SINGLE PLAYER","HIGH SCORES","OPTIONS"};
static float multiplayer_width(const char *count) {
    return label_width(menu_items[0])+14+4+6+label_width(count)+6+4;
}
void ui_measure_menu(Game *g) {
    char count[8]; snprintf(count,sizeof(count),"%uP",g->players);
    g->menu_label_widths[0]=multiplayer_width(count);
    for(unsigned i=1;i<4;i++) g->menu_label_widths[i]=label_width(menu_items[i]);
}
static void multiplayer_label(const Game *g,unsigned pads,float y,int style) {
    const char *title=menu_items[0];
    char count[8]; snprintf(count,sizeof(count),"%uP",g->players);
    float title_width=label_width(title),count_width=label_width(count);
    float x=roundf(160-multiplayer_width(count)*.5f);
    label(x+2,y+2,5,title); label(x,y,style,title);
    x+=title_width+14;
    uint32_t bright=style==MENU_SELECTED_STYLE?GOLD:style==6?0x737d8a:0xa0b3c9;
    menu_triangle(x+2,y-2,false,0x02040a);
    menu_triangle(x,y-4,false,pads>=2 && g->players>2?bright:0x58616e);
    x+=10;
    label(x+2,y+2,5,count); label(x,y,style,count);
    x+=count_width+6;
    menu_triangle(x+2,y-2,true,0x02040a);
    menu_triangle(x,y-4,true,g->players<pads?bright:0x58616e);
}
/* Compact 5x7 block alphabet for the three-colour title. */
static void block_word(float x,float y,const char *s,uint32_t color) {
    const char *alphabet="PLASMONG64";
    static const unsigned char glyphs[][7]={
        {30,17,17,30,16,16,16}, {16,16,16,16,16,16,31},
        {14,17,17,31,17,17,17}, {15,16,16,14,1,1,30},
        {17,27,21,21,17,17,17}, {14,17,17,17,17,17,14},
        {17,25,25,21,19,19,17}, {14,17,16,23,17,17,14},
        {14,16,16,30,17,17,14}, {2,6,10,18,31,2,2}
    };
    for(;*s;s++,x+=18) {
        const char *found=strchr(alphabet,*s); if(!found) continue;
        const unsigned char *rows=glyphs[found-alphabet];
        for(int row=0;row<7;row++) for(int col=0;col<5;) {
            if(!(rows[row]&(16>>col))) { col++; continue; }
            int first=col;
            while(col<5 && (rows[row]&(16>>col))) col++;
            rect(x+first*3,y+row*3,(col-first)*3,3,color);
        }
    }
}
static void menu_title(void) {
    /* Render all shadow pixels first so adjacent blocks form one clear shadow. */
    block_word(50,61,"PLASMA",0x02040a); block_word(164,61,"PONG",0x02040a);
    block_word(242,61,"64",0x02040a);
    block_word(48,59,"PLASMA",CYAN); block_word(162,59,"PONG",CORAL);
    block_word(240,59,"64",GOLD);
}
static void court(void) {
    rect(OX,OY-2,ARENA_W,1,0x304c65); rect(OX,OY+ARENA_H,ARENA_W,1,0x304c65);
    for(int y=OY+5;y<OY+ARENA_H-3;y+=12) rect(OX+ARENA_W*.5f-1,y,1,4,0x23374e);
    rect(OX-3,OY,2,ARENA_H,0x24566c); rect(OX+ARENA_W+1,OY,2,ARENA_H,0x71334c);
}
static void square_mask(void) {
    const float left=OX+(ARENA_W-ARENA_H)*.5f,right=left+ARENA_H;
    /* Crop the full fluid field without stretching paddles, ball or currents. */
    rect(0,OY-2,left,ARENA_H+5,0x070c17);
    rect(right,OY-2,320-right,ARENA_H+5,0x070c17);
    /* Opaque corner wedges mask dye and tracers; the inset edge is a wall. */
    for(int row=0;row<(int)CORNER_SIZE;row++) {
        float width=CORNER_SIZE-row;
        rect(left,OY+row,width,1,0x070c17);
        rect(right-width,OY+row,width,1,0x070c17);
        rect(left,OY+ARENA_H-row-1,width,1,0x070c17);
        rect(right-width,OY+ARENA_H-row-1,width,1,0x070c17);
    }
    /* Rows do not overlap: batch the wall color after all background wedges.
       The cached block otherwise replays two fill-mode changes per row. */
    for(int row=0;row<(int)CORNER_SIZE;row++) {
        float width=CORNER_SIZE-row;
        rect(left+width-1,OY+row,2,1,0x737d8a);
        rect(right-width-1,OY+row,2,1,0x737d8a);
        rect(left+width-1,OY+ARENA_H-row-1,2,1,0x737d8a);
        rect(right-width-1,OY+ARENA_H-row-1,2,1,0x737d8a);
    }
}
static void square_court(const Game *g) {
    float left=OX+game_left(g),right=OX+game_right(g);
    draw_static(DRAW_SQUARE_MASK,square_mask);
    rect(left-2,OY+CORNER_SIZE,2,ARENA_H-2*CORNER_SIZE,game_alive(g,0)?CYAN:0x737d8a);
    rect(right,OY+CORNER_SIZE,2,ARENA_H-2*CORNER_SIZE,game_alive(g,1)?CORAL:0x737d8a);
    rect(left+CORNER_SIZE,OY+ARENA_H,right-left-2*CORNER_SIZE,2,game_alive(g,2)?MINT:0x737d8a);
    rect(left+CORNER_SIZE,OY-2,right-left-2*CORNER_SIZE,2,g->players==4 && game_alive(g,3)?VIOLET:0x737d8a);
}
/* Rotate a paddle's details, including its charge and recovery meters. */
static void bat_rect(int p,float x,float y,float dx,float dy,float w,float h,uint32_t c) {
    if(p<2) rect(x+dx,y+dy,w,h,c);
    else rect(x+dy,y+dx,h,w,c);
}
#ifdef PLASMAPONG_FLUID_PROFILE
#include <libdragon.h>
#endif
static void flow_points(const Game *g,float x,float y,float w,float h,int layers) {
#ifdef PLASMAPONG_FLUID_PROFILE
    uint64_t flow_begin=get_ticks();
#endif
    /* A small, saturated palette preserves batching: at most six color
       changes per layer, rather than one per particle. Read nearest-cell dye
       once per head; tails share its tint. Recognise the mint and violet ink
       blends as well as the three base pigments. Clear fluid stays neutral. */
    enum { TINT_NEUTRAL, TINT_CYAN, TINT_CORAL, TINT_GOLD, TINT_MINT, TINT_VIOLET, TINT_COUNT };
    _Static_assert((unsigned)TINT_COUNT==DRAW_POINT_TINTS,"point command capacity must cover every tint");
    static const uint32_t colors[TINT_COUNT][DRAW_POINT_LAYERS]={
        {0x526d82,0x465d70,0x3b4e5e,0x30404c,0x25313a}, /* neutral */
        {0x29afd2,0x2397b5,0x1d7f99,0x18677c,0x124f60}, /* cyan */
        {0xdb506b,0xbd455c,0x9f3a4e,0x812f3f,0x632430}, /* coral */
        {0xc7a333,0xac8d2c,0x917725,0x75611e,0x5a4a17}, /* gold */
        {0x50d6ad,0x45b995,0x3a9b7e,0x2f7e66,0x24604e}, /* mint */
        {0xa16dd6,0x8b5eb9,0x75509b,0x5f417e,0x493160}  /* violet */
    };
    unsigned head=g->tracer_head,history[DRAW_POINT_LAYERS];
    for(unsigned j=0;j<DRAW_POINT_LAYERS;j++) history[j]=(head+j*FLOW_SAMPLE_TICKS)%FLOW_HISTORY;
    const float sx=w/ARENA_W,sy=h/ARENA_H;
    /* Gameplay masks the side strips after drawing the fluid. Do not submit
       tracer dots that will be completely covered. Menus remain full width. */
    const bool cropped=game_square(g) && g->phase!=MENU && g->phase!=OPTIONS && g->phase!=SCORES;
    const float clip_left=cropped?x+game_left(g)*sx:x;
    const float clip_right=cropped?x+game_right(g)*sx:x+w;
    /* Points are integer pixels: ceil gives exactly the original float clip
       comparisons without converting every point back to float. */
    const int left=(int)ceilf(clip_left),right=(int)ceilf(clip_right);
    const int top=(int)ceilf(y),bottom=(int)ceilf(y+h);
    /* Prepare every dot for one tracer while its history is in cache. Keep
       the existing back-to-front, color-grouped draw order in compact batches. */
    static DrawPoint points[DRAW_POINT_LAYERS][TINT_COUNT][FLOW_TRACERS];
    unsigned counts[DRAW_POINT_LAYERS][TINT_COUNT]={{0}};
    /* Tail offsets are capped at 24 world units. A one-unit guard covers
       float rounding: beyond this halo every dot is hidden by the side mask. */
    const float halo=layers?25:1;
    const float hidden_left=game_left(g)-halo,hidden_right=game_right(g)+halo;
    const FluidInk *ink=fluid_dye(&g->fluid);
    const FluidInkValue minimum=fluid_ink_encode(.04f);
    for(unsigned i=0;i<FLOW_TRACERS;i++) {
        const FlowTracer *t=&g->tracers[i]; if(!t->life) continue;
        float hx=g->tracer_history[head][i].x,hy=g->tracer_history[head][i].y;
        if(cropped && (hx<hidden_left || hx>hidden_right)) continue;
        int k=(int)(hy/CELL)*FW+(int)(hx/CELL);
        FluidInkValue r=ink->red[k],b=ink->blue[k],gold=ink->gold[k];
        unsigned tint=TINT_NEUTRAL;
        /* Mint is blue plus gold; violet is comparable red and blue. Follow
           the actual dye so previous-level trails keep their fading colour. */
        if(b>minimum && gold>minimum && b>gold && gold>b/4 &&
           b>r+r/2 && gold>r+r/2) tint=TINT_MINT;
        else if(b>minimum && r>minimum && b<r+r/2 && r<b+b/2 &&
                b>gold+gold/2 && r>gold+gold/2) tint=TINT_VIOLET;
        else if(b>minimum && b>r+r/2 && b>gold+gold/2) tint=TINT_CYAN;
        else if(r>minimum && r>b+b/2 && r>gold+gold/2) tint=TINT_CORAL;
        else if(gold>minimum && gold>r+r/2 && gold>b+b/2) tint=TINT_GOLD;
        float dx[DRAW_POINT_LAYERS],dy[DRAW_POINT_LAYERS],length[DRAW_POINT_LAYERS];
        dx[0]=dy[0]=0;
        float extent=0;
        for(int j=1;j<=layers;j++) {
            dx[j]=g->tracer_history[history[j]][i].x-hx; dy[j]=g->tracer_history[history[j]][i].y-hy;
            length[j]=maxf(fabsf(dx[j]),fabsf(dy[j]));
            extent=maxf(extent,length[j]);
        }
        /* Most tails fit the cap. Specialise that case so every visible
           dot avoids three multiplies by one, with identical float rounding. */
        if(extent<=24) {
            for(int j=0;j<=layers;j++) {
                if(j && length[j]<1) continue;
                /* Histories and their convex combinations stay in the positive
                   screen domain. Integer truncation is therefore floor. */
                int px=(int)(x+(hx+dx[j])*sx);
                int py=(int)(y+(hy+dy[j])*sy);
                if(px>=left && py>=top && px<right && py<bottom)
                    points[j][tint][counts[j][tint]++]=(DrawPoint){px,py};
            }
        } else {
            float scale=24/extent;
            for(int j=0;j<=layers;j++) {
                if(j && length[j]*scale<1) continue;
                /* Histories and their convex combinations stay in the positive
                   screen domain. Integer truncation is therefore floor. */
                int px=(int)(x+(hx+dx[j]*scale)*sx);
                int py=(int)(y+(hy+dy[j]*scale)*sy);
                if(px>=left && py>=top && px<right && py<bottom)
                    points[j][tint][counts[j][tint]++]=(DrawPoint){px,py};
            }
        }
    }
#ifdef PLASMAPONG_FLUID_PROFILE
    flow_prepare_ticks+=get_ticks()-flow_begin; flow_begin=get_ticks();
#endif
    for(int j=layers;j>=0;j--)
        for(unsigned tint=0;tint<TINT_COUNT;tint++)
            draw_points(points[j][tint],counts[j][tint],colors[tint][j]);
    draw_points_end();
#ifdef PLASMAPONG_FLUID_PROFILE
    flow_emit_ticks+=get_ticks()-flow_begin;
#endif
}
static void flow_background(const Game *g,float x,float y,float w,float h) {
    /* Separate the two fixed layer counts so the compiler can specialise
       the history and point loops while preserving every dot and its order. */
    draw_fluid(&g->fluid,x,y,w,h,g->flow_effect);
    if(g->flow_effect==FLOW_TAILS) flow_points(g,x,y,w,h,DRAW_POINT_LAYERS-1);
    else if(g->flow_effect==FLOW_PARTICLES) flow_points(g,x,y,w,h,0);
}
void ui_menu_foreground(const Game *g) {
    draw_static(DRAW_MENU_TITLE,menu_title);
    unsigned pads=0; for(unsigned p=0;p<MAX_PLAYERS;p++) if(g->connected[p]) pads++;
    for(unsigned i=0;i<4;i++) {
        int style=i==0 && pads<2?6:g->menu_selection==i?MENU_SELECTED_STYLE:1;
        float y=MENU_FIRST_ROW+i*MENU_ROW_SPACING;
        if(i==0) multiplayer_label(g,pads,y,style);
        else menu_label(y,style,menu_items[i]);
    }
}
void ui_menu_particles(const Game *g) {
    if(g->flow_effect==FLOW_TAILS) flow_points(g,0,0,320,240,DRAW_POINT_LAYERS-1);
    else if(g->flow_effect==FLOW_PARTICLES) flow_points(g,0,0,320,240,0);
}
void ui_draw(const Game *g) {
    if(g->phase==OPTIONS) {
        flow_background(g,0,0,320,240);
        rect(12,64,296,117,0x09111f);
        menu_label(87,4,"OPTIONS");
        rect(20,g->options_selection?124:98,280,20,0x142c3b);
        rect(20,g->options_selection?124:98,2,20,CYAN);
        const char *effects[]={"NONE","PARTICLES","PARTICLE TAILS","SPEED","VORTEX","RELIEF","BANDS","PRESSURE"};
        char setting[64]; snprintf(setting,sizeof(setting),"< FLOW EFFECT: %s >",effects[g->flow_effect]);
        menu_label(113,g->options_selection==0?2:1,setting);
        snprintf(setting,sizeof(setting),"< RESOLUTION: %s >",g->frame_rate==FPS_60?"LOW RES":"HIGH RES");
        menu_label(139,g->options_selection==1?2:1,setting);
        if(!g->save_available || g->save_failed)
            menu_label(169,1,!g->save_available?"NO SAVE STORAGE - SESSION ONLY":"SAVE FAILED - SESSION ONLY");
        return;
    }
    if(g->phase==MENU) {
        flow_background(g,0,0,320,240);
#ifdef PLASMAPONG_MENU_LABEL_BLOCK
        draw_menu_foreground(g);
#else
        ui_menu_foreground(g);
#endif
        return;
    }
    /* The opaque fluid blit replaces every court pixel. Clear only its
       surrounding strips, avoiding a redundant full-screen framebuffer write.
       Scores have no fluid background and still need the complete clear. */
    if(g->phase==SCORES) rect(0,0,320,240,0x070c17);
    else {
        rect(0,0,320,OY,0x070c17);
        rect(0,OY+ARENA_H,320,240-OY-ARENA_H,0x070c17);
        rect(0,OY,OX,ARENA_H,0x070c17);
        rect(OX+ARENA_W,OY,320-OX-ARENA_W,ARENA_H,0x070c17);
    }
    char s[64];
    if(g->phase==SCORES) {
        menu_label(26,4,"HIGH SCORES");
        label(24,49,1,"#"); label(55,49,1,"NAME");
        label_edge(201,49,1,"SCORE",true); label_edge(296,49,1,"LEVEL",true);
        for(int i=0;i<HIGH_SCORE_COUNT;i++) {
            const HighScore *h=&g->highs[i];
            snprintf(s,sizeof(s),"%2d",i+1); label(24,67+i*13,1,s);
            if(!h->level) { label(55,67+i*13,1,"---"); continue; }
            int style=i==g->score_entry?2:0;
            label(55,67+i*13,style,h->initials);
            snprintf(s,sizeof(s),"%u",(unsigned)h->points); label_edge(201,67+i*13,style,s,true);
            snprintf(s,sizeof(s),"%u",(unsigned)h->level); label_edge(296,67+i*13,style,s,true);
        }
        if(!g->save_available || g->save_failed)
            menu_label(212,1,!g->save_available?"NO SAVE STORAGE - SESSION SCORES":"SAVE FAILED - SESSION SCORES");
        hint(107,230,"[B] BACK TO MENU"); return;
    }
    if(g->mode==ARCADE) {
        snprintf(s,sizeof(s),"SCORE %u",(unsigned)g->arcade.points); label(16,20,0,s);
        snprintf(s,sizeof(s),"LV %u",(unsigned)g->arcade.level); label(167,20,0,s);
        snprintf(s,sizeof(s),"LIVES %u",g->arcade.lives); label_edge(304,20,player_styles[game_player_palette(g,0)],s,true);
    } else if(game_square(g)) {
        for(unsigned p=0;p<g->players;p++) {
            snprintf(s,sizeof(s),g->lives[p]?"P%u %u":"P%u OUT",p+1,g->lives[p]);
            label(20+p*76,20,g->lives[p]?player_styles[p]:6,s);
        }
    } else {
        snprintf(s,sizeof(s),"%u  :  %u",g->score[0],g->score[1]);
        label(145,20,0,s); label(233,20,1,"FIRST TO 9");
    }
    flow_background(g,OX,OY,ARENA_W,ARENA_H);
    if(game_square(g)) square_court(g); else draw_static(DRAW_COURT,court);
    float suction_radius=0;
    bool radius_ready=false;
    for(unsigned p=0;p<game_players(g);p++) {
        if(!game_alive(g,p)) continue;
        unsigned palette=game_player_palette(g,p);
        const Bat *b=&g->bat[p]; uint32_t c=player_colors[palette];
        float half=game_bat_half(g,p);
        bool broken=b->cooldown_ticks>0;
        if(broken) c=0x737d8a;
        float x=OX+b->x,y=OY+b->y;
        if(b->sucking) {
            if(!radius_ready) { suction_radius=24+2*sinf(g->elapsed*7); radius_ready=true; }
            ring(g,x,y,suction_radius,c);
            if(g->held==(int)p)
                bat_rect(p,x,y,-9,half+5,18*b->charge,2,b->charge>=1?0x40ff70:c);
        }
        if(b->burst>0) ring(g,x,y,12+(1-b->burst/.25f)*30,c);
        bat_rect(p,x,y,-5,-half-2,10,half*2+4,broken?0x36323c:player_outlines[palette]);
        bat_rect(p,x,y,-3,-half,6,half*2,c);
        bat_rect(p,x,y,-1,-half+2,2,half*2-4,broken?0x434753:WHITE);
        if(broken) {
            bat_rect(p,x,y,-3,-4,4,2,0xff3030); bat_rect(p,x,y,-1,-2,4,2,0xff3030);
            bat_rect(p,x,y,-3,0,4,2,0xff3030);
            bat_rect(p,x,y,-9,half+5,18,2,0x36323c);
            bat_rect(p,x,y,-9,half+5,18*(float)b->cooldown_ticks/SUCTION_COOLDOWN_TICKS,2,0xff3030);
        }
    }
    if(!game_square(g)) {
        label_edge(OX+5,46,player_styles[game_player_palette(g,0)],"P1",false);
        label_edge(OX+ARENA_W-5,46,player_styles[game_player_palette(g,1)],g->mode==ARCADE?"CPU":"P2",true);
    }
    if(g->mode==ARCADE) {
        snprintf(s,sizeof(s),"GOALS %u / 3",g->arcade.goals); label(130,46,0,s);
    }
    float bx=OX+g->bx,by=OY+g->by;
    bool hot=game_ball_hot(g);
    if(g->serve<=0 && g->held<0) {
        for(int i=4;i>0;i--) {
            float x=maxf(OX+game_left(g),minf(OX+game_right(g)-2,bx-g->bvx*i*.016f));
            float y=maxf(OY,minf(OY+ARENA_H-2,by-g->bvy*i*.016f));
            rect(x-1,y-1,2,2,hot?0xff3030:0x556b84);
        }
    }
    /* An eight-pixel filled circle, snapped so its strips share pixel edges. */
    float ball_x=roundf(bx),ball_y=roundf(by);
    uint32_t ball_color=hot?0xff3030:GOLD;
    rect(ball_x-2,ball_y-4,4,1,ball_color);
    rect(ball_x-3,ball_y-3,6,1,ball_color);
    rect(ball_x-4,ball_y-2,8,4,ball_color);
    rect(ball_x-3,ball_y+2,6,1,ball_color);
    rect(ball_x-2,ball_y+3,4,1,ball_color);
    if(g->phase==LOBBY) {
        if(g->mode==ARCADE) {
            if(!g->connected[0]) panel("ONE PLAYER REQUIRED","Connect a pad to port 1","[START] PLAY  [B] MENU");
            else panel("ENDLESS ARCADE","3 goals per level. 3 lives.","[START] PLAY  [B] MENU");
        } else if(!game_ready(g))
            panel("CONTROLLERS REQUIRED","Reconnect the selected players' pads","[START] PLAY  [B] MENU");
        else if(game_square(g)) panel("LAST PLAYER STANDING","3 lives each. Miss a side, lose a life.","[START] PLAY  [B] MENU");
        else panel("STIR. GRAB. LAUNCH.","Cyan vs coral. First to 9 wins.","[START] PLAY  [B] MENU");
    } else if(g->phase==PAUSED) {
        if(!game_ready(g))
            panel("CONTROLLER DISCONNECTED","Reconnect the missing player's pad","[START] RESUME  [B] MENU");
        else panel("PAUSED","[Z] jet    Hold [A] suction","[START] RESUME  [B] MENU");
    } else if(g->phase==FINISHED) {
        if(g->mode==ARCADE) {
            rect(37,72,246,120,0x243449); rect(39,74,242,116,0x09111f);
            menu_label(91,4,"RUN OVER");
            snprintf(s,sizeof(s),"SCORE %u   LEVEL %u",(unsigned)g->arcade.points,(unsigned)g->arcade.level);
            menu_label(111,0,s);
            if(g->score_entry>=0) {
                name_entry(g,133);
                centered_hint(150,"[UP] / [DOWN] CHANGE LETTER");
                centered_hint(166,"[LEFT] / [RIGHT] CHOOSE LETTER");
            } else menu_label(145,1,"TRY AGAIN FOR THE TOP TEN");
            hint(103,183,"[A] HIGH SCORES");
        } else {
            snprintf(s,sizeof(s),"PLAYER %d WINS!",g->winner+1);
            panel(s,game_ready(g)?"The currents have a champion.":"Reconnect all pads for a rematch.","[START] REMATCH  [B] MENU");
        }
    } else if(g->mode==ARCADE && g->arcade.transition>0) {
        snprintf(s,sizeof(s),"LEVEL %u",(unsigned)g->arcade.level);
        const char *challenge=g->arcade.level==2?"Opponent jets unlocked":
            g->arcade.level==4?"Opponent suction unlocked":"Quicker reactions. Better aim.";
        if(g->arcade.level>=6) {
            const char *currents[]={"Cross currents","Rising currents","Swirling currents"};
            challenge=currents[(g->arcade.level-6)%3];
        }
        panel(s,challenge,"3 more goals to advance");
    } else if(g->serve>0) {
        label(126,83,0,g->serve>.6f?"GET READY":"SERVE!");
    }
}
