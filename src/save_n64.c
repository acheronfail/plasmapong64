#include <libdragon.h>
#include "save.h"
#include <string.h>
static int active_slot=-1;
static uint32_t generation;
#ifdef PLASMAPONG_AUDIO_STREAM
extern void audio_storage_service(void);
#endif
static bool score_write_block(int block,const uint8_t data[8]) {
    bool ok=eeprom_write(block,data)==0;
#ifdef PLASMAPONG_AUDIO_STREAM
    audio_storage_service();
#endif
    return ok;
}
void scores_load(Game *g) {
    g->save_available=eeprom_present()!=EEPROM_NONE;
    active_slot=-1; generation=0;
    if(!g->save_available) return;
    for(int slot=0;slot<2;slot++) {
        uint8_t data[SCORE_SAVE_BYTES]; HighScore scores[HIGH_SCORE_COUNT]; uint32_t serial; FlowEffect effect; FrameRate frame_rate;
        eeprom_read_bytes(data,slot*SCORE_SAVE_BYTES,sizeof(data));
        if(!score_save_decode(data,scores,&serial,&effect,&frame_rate)) continue;
        uint32_t newer=serial-generation;
        if(active_slot<0 || (newer && newer<0x80000000u)) {
            g->flow_effect=effect; g->frame_rate=frame_rate; memcpy(g->highs,scores,sizeof(scores)); active_slot=slot; generation=serial;
        }
    }
    debugf("Plasma Pong 64: high scores loaded from EEPROM slot %d\n",active_slot);
}
void scores_store(Game *g) {
    g->scores_dirty=false;
    if(!g->save_available) return;
    int slot=active_slot==0?1:0,base=slot*(SCORE_SAVE_BYTES/8);
    uint8_t data[SCORE_SAVE_BYTES],check[SCORE_SAVE_BYTES],invalid[8]={0};
    score_save_encode(data,g->highs,generation+1,g->flow_effect,g->frame_rate);
#ifdef PLASMAPONG_AUDIO_STREAM
    audio_storage_service();
#else
    audio_pause(true);
#endif
    /* Invalidate the inactive slot, write its body, commit its header last.
       The previous complete record remains usable after an interrupted write. */
    bool ok=score_write_block(base,invalid);
    for(int block=1;ok && block<SCORE_SAVE_BYTES/8;block++)
        ok=score_write_block(base+block,data+block*8);
    if(ok) ok=score_write_block(base,data);
    if(ok) {
#ifdef PLASMAPONG_AUDIO_STREAM
        for(int block=0;block<SCORE_SAVE_BYTES/8;block++) {
            eeprom_read(base+block,check+block*8);
            audio_storage_service();
        }
#else
        eeprom_read_bytes(check,slot*SCORE_SAVE_BYTES,sizeof(check));
#endif
        ok=!memcmp(check,data,sizeof(data));
    }
#ifdef PLASMAPONG_AUDIO_STREAM
    audio_storage_service();
#else
    audio_pause(false);
#endif
    g->save_failed=!ok;
    if(ok) { active_slot=slot; generation++; }
    debugf("Plasma Pong 64: high scores %s\n",ok?"saved to EEPROM":"EEPROM write failed");
}
