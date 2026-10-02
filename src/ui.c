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
/* Match the ball dye's RGB contribution in fluid_color(). */
#define GOLD 0xffcd19
static void ring(float x,float y,float radius,uint32_t color) {
    for(int i=0;i<20;i++) {
        float a=i*6.2831853f/20;
        float px=x+cosf(a)*radius-1,py=y+sinf(a)*radius-1;
        if(px>=OX && px<=OX+ARENA_W-2 && py>=OY && py<=OY+ARENA_H-2)
            rect(px,py,2,2,color);
    }
}
/* Bracketed control names become N64-coloured keycaps everywhere. */
static void hint(float x,float y,const char *s) {
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
            rect(x,y-9,w+6,12,c);
            label(x+3,y,4,part); x+=w+6;
        } else { label(x,y,1,part); x+=w; }
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
    rect(16,32,288,1,0x304c65); rect(16,214,288,1,0x304c65);
    for(int y=39;y<211;y+=12) rect(159,y,1,4,0x23374e);
    rect(13,34,2,180,0x24566c); rect(305,34,2,180,0x71334c);
}
void ui_draw(const Game *g) {
    if(g->phase==MENU) {
        draw_fluid(&g->fluid,0,0,320,240);
        draw_static(DRAW_MENU_TITLE,menu_title);
        const char *items[]={"MULTI-PLAYER","SINGLE PLAYER","HIGH SCORES"};
        for(unsigned i=0;i<3;i++) {
            char item[40]; snprintf(item,sizeof(item),g->menu_selection==i?"> %s <":"%s",items[i]);
            menu_label(124+i*25,g->menu_selection==i?4:1,item);
        }
        menu_label(216,1,"UP / DOWN TO SELECT    A TO PLAY");
        return;
    }
    rect(0,0,320,240,0x070c17);
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
        menu_label(212,1,!g->save_available?"NO SAVE STORAGE - SESSION SCORES":
            g->save_failed?"SAVE FAILED - SESSION SCORES":"HIGH SCORES SAVED TO CARTRIDGE");
        hint(107,230,"[B] BACK TO MENU"); return;
    }
    if(g->mode==ARCADE) {
        snprintf(s,sizeof(s),"SCORE %u",(unsigned)g->arcade.points); label(16,20,0,s);
        snprintf(s,sizeof(s),"LV %u",(unsigned)g->arcade.level); label(167,20,0,s);
        snprintf(s,sizeof(s),"LIVES %u",g->arcade.lives); label_edge(304,20,2,s,true);
    } else {
        snprintf(s,sizeof(s),"%u  :  %u",g->score[0],g->score[1]);
        label(145,20,0,s); label(233,20,1,"FIRST TO 9");
    }
    draw_fluid(&g->fluid,OX,OY,ARENA_W,ARENA_H);
    draw_static(DRAW_COURT,court);
    for(int p=0;p<2;p++) {
        const Bat *b=&g->bat[p]; uint32_t c=p?CORAL:CYAN;
        float x=OX+b->x,y=OY+b->y;
        if(b->sucking) {
            ring(x,y,24+2*sinf(g->elapsed*7),c);
            rect(x-9,y+BAT_HALF+5,18*b->charge,2,c);
        }
        if(b->burst>0) ring(x,y,12+(1-b->burst/.25f)*30,c);
        rect(x-5,y-BAT_HALF-2,10,BAT_HALF*2+4,p?0x642739:0x164658);
        rect(x-3,y-BAT_HALF,6,BAT_HALF*2,c);
        rect(x-1,y-BAT_HALF+2,2,BAT_HALF*2-4,WHITE);
    }
    /* Match the visible text edges, excluding the font's trailing advance. */
    label_edge(OX+5,46,2,"P1",false);
    label_edge(OX+ARENA_W-5,46,3,g->mode==ARCADE?"CPU":"P2",true);
    if(g->mode==ARCADE) {
        snprintf(s,sizeof(s),"GOALS %u / 3",g->arcade.goals); label(130,46,0,s);
    }
    float bx=OX+g->bx,by=OY+g->by;
    if(g->serve<=0 && g->held<0) {
        for(int i=4;i>0;i--) {
            float x=maxf(OX,minf(OX+ARENA_W-2,bx-g->bvx*i*.016f));
            float y=maxf(OY,minf(OY+ARENA_H-2,by-g->bvy*i*.016f));
            rect(x-1,y-1,2,2,0x556b84);
        }
    }
    rect(bx-4,by-4,8,8,g->held<0?0x447486:0xffd875);
    rect(bx-2,by-3,4,6,WHITE); rect(bx-3,by-2,6,4,WHITE);
    hint(16,230,"[STICK] MOVE  [Z] JET  HOLD [A] GRAB / RELEASE");
    if(g->phase==LOBBY) {
        if(g->mode==ARCADE) {
            if(!g->connected[0]) panel("ONE PLAYER REQUIRED","Connect a pad to port 1","[START] PLAY  [B] MENU");
            else panel("ENDLESS ARCADE","3 goals per level. 3 lives.","[START] PLAY  [B] MENU");
        } else if(!g->connected[0] || !g->connected[1])
            panel("TWO PLAYERS REQUIRED","Connect N64 pads to ports 1 + 2","[START] PLAY  [B] MENU");
        else panel("STIR. GRAB. LAUNCH.","Cyan vs coral. First to 9 wins.","[START] PLAY  [B] MENU");
    } else if(g->phase==PAUSED) {
        if(!g->connected[0] || (g->mode==MULTIPLAYER && !g->connected[1]))
            panel("CONTROLLER DISCONNECTED",g->mode==ARCADE?"Reconnect pad to port 1":"Reconnect pads to ports 1 + 2","[START] RESUME  [B] MENU");
        else panel("PAUSED","[Z] jet    Hold [A] suction","[START] RESUME  [B] MENU");
    } else if(g->phase==FINISHED) {
        if(g->mode==ARCADE) {
            rect(37,72,246,120,0x243449); rect(39,74,242,116,0x09111f);
            menu_label(91,4,"RUN OVER");
            snprintf(s,sizeof(s),"SCORE %u   LEVEL %u",(unsigned)g->arcade.points,(unsigned)g->arcade.level);
            menu_label(111,0,s);
            if(g->score_entry>=0) {
                snprintf(s,sizeof(s),"HIGH SCORE!  %s",g->highs[g->score_entry].initials); menu_label(133,2,s);
                snprintf(s,sizeof(s),"EDIT LETTER %u: UP / DOWN",g->initial_cursor+1); menu_label(150,1,s);
                menu_label(166,1,"LEFT / RIGHT: CHOOSE LETTER");
            } else menu_label(145,1,"TRY AGAIN FOR THE TOP TEN");
            hint(103,183,"[A] HIGH SCORES");
        } else {
            snprintf(s,sizeof(s),"PLAYER %d WINS!",g->winner+1);
            panel(s,"The currents have a champion.","[START] REMATCH  [B] MENU");
        }
    } else if(g->mode==ARCADE && g->arcade.transition>0) {
        snprintf(s,sizeof(s),"LEVEL %u",(unsigned)g->arcade.level);
        const char *challenge=g->arcade.level==2?"Opponent jets unlocked":
            g->arcade.level==4?"Opponent suction unlocked":"Quicker reactions. Faster serves.";
        if(g->arcade.level>=6) {
            const char *currents[]={"Cross currents","Rising currents","Swirling currents"};
            challenge=currents[(g->arcade.level-6)%3];
        }
        panel(s,challenge,"3 more goals to advance");
    } else if(g->serve>0) {
        label(126,83,0,g->serve>.6f?"GET READY":"SERVE!");
    }
}
