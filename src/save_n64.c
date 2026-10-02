#include <libdragon.h>
#include "save.h"
#include <string.h>
static int active_slot=-1;
static uint32_t generation;
void scores_load(Game *g) {
    g->save_available=eeprom_present()!=EEPROM_NONE;
    active_slot=-1; generation=0;
    if(!g->save_available) return;
    for(int slot=0;slot<2;slot++) {
        uint8_t data[SCORE_SAVE_BYTES]; HighScore scores[HIGH_SCORE_COUNT]; uint32_t serial; FlowEffect effect;
        eeprom_read_bytes(data,slot*SCORE_SAVE_BYTES,sizeof(data));
        if(!score_save_decode(data,scores,&serial,&effect)) continue;
        uint32_t newer=serial-generation;
        if(active_slot<0 || (newer && newer<0x80000000u)) {
            g->flow_effect=effect; memcpy(g->highs,scores,sizeof(scores)); active_slot=slot; generation=serial;
        }
    }
    debugf("Plasma Pong 64: high scores loaded from EEPROM slot %d\n",active_slot);
}
void scores_store(Game *g) {
    g->scores_dirty=false;
    if(!g->save_available) return;
    int slot=active_slot==0?1:0,base=slot*(SCORE_SAVE_BYTES/8);
    uint8_t data[SCORE_SAVE_BYTES],check[SCORE_SAVE_BYTES],invalid[8]={0};
    score_save_encode(data,g->highs,generation+1,g->flow_effect);
    audio_pause(true);
    /* Invalidate the inactive slot, write its body, commit its header last.
       The previous complete record remains usable after an interrupted write. */
    bool ok=eeprom_write(base,invalid)==0;
    for(int block=1;ok && block<SCORE_SAVE_BYTES/8;block++)
        ok=eeprom_write(base+block,data+block*8)==0;
    if(ok) ok=eeprom_write(base,data)==0;
    if(ok) { eeprom_read_bytes(check,slot*SCORE_SAVE_BYTES,sizeof(check)); ok=!memcmp(check,data,sizeof(data)); }
    audio_pause(false);
    g->save_failed=!ok;
    if(ok) { active_slot=slot; generation++; }
    debugf("Plasma Pong 64: high scores %s\n",ok?"saved to EEPROM":"EEPROM write failed");
}
