#include "game.h"
#include "save.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static Game g,fresh;
static Input in[MAX_PLAYERS];
static void open_dialog(void) {
    game_init(&g); g.phase=OPTIONS; g.options_selection=OPTION_CLEAR_SAVE;
    g.flow_effect=FLOW_TAILS; g.frame_rate=FPS_30; g.fps_meter=true;
    g.highs[0]=(HighScore){.points=1234,.level=5,.initials="ACE"};
    memset(in,0,sizeof(in)); in[0].connected=true; in[0].a=true;
    game_step(&g,in); assert(g.clear_save_dialog && !g.scores_dirty);
    game_step(&g,in); assert(g.clear_save_dialog && g.highs[0].points==1234);
    in[0].a=false; game_step(&g,in);
}
int main(void) {
    game_init(&fresh);
    /* Every physical button other than A cancels; cancellation wins chords. */
    for(int button=0;button<15;button++) {
        open_dialog();
        in[0].a=true;
        if(button==0) in[0].b=true;
        else if(button==1) in[0].start=true;
        else if(button==2) in[0].z=true;
        else if(button==3) in[0].x=1;
        else if(button==4) in[0].y=-1;
        else in[0].other_buttons=1u<<(button-5);
        game_step(&g,in);
        assert(!g.clear_save_dialog && !g.scores_dirty && g.phase==OPTIONS);
        assert(g.highs[0].points==1234 && g.flow_effect==FLOW_TAILS && g.frame_rate==FPS_30 && g.fps_meter);
    }
    open_dialog(); in[2].connected=true; in[2].a=true; game_step(&g,in);
    assert(!g.clear_save_dialog && g.scores_dirty && g.phase==OPTIONS);
    assert(!memcmp(g.highs,fresh.highs,sizeof(g.highs)));
    assert(g.flow_effect==fresh.flow_effect && g.frame_rate==fresh.frame_rate && g.fps_meter==fresh.fps_meter);
    assert(g.score_entry==-1);
    /* The normal persistence path must round-trip the cleared state. */
    uint8_t data[SCORE_SAVE_BYTES]; uint32_t generation;
    score_save_encode(data,g.highs,2,g.flow_effect,g.frame_rate,g.fps_meter);
    assert(score_save_decode(data,g.highs,&generation,&g.flow_effect,&g.frame_rate,&g.fps_meter));
    assert(!memcmp(g.highs,fresh.highs,sizeof(g.highs)) && g.flow_effect==fresh.flow_effect && g.frame_rate==fresh.frame_rate && g.fps_meter==fresh.fps_meter);
    puts("PASS: clear-save confirmation debounce, all cancel buttons, chords, defaults and persistence record");
}
