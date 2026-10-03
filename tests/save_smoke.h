/* Dedicated EEPROM test ROM only: never enabled in the playable build. */
static void save_smoke(Game *g) {
    assertf(g->save_available,"EEPROM save metadata was not detected");
    if(!g->highs[0].level) {
        g->highs[0]=(HighScore){.points=12345,.level=7,.initials="ACE"};
        g->flow_effect=FLOW_TAILS; g->frame_rate=FPS_30;
        g->scores_dirty=true; scores_store(g);
        assertf(!g->save_failed,"EEPROM save failed");
        debugf("SAVE SMOKE: wrote first-boot high score, flow and high resolution\n");
    } else {
        assertf((g->highs[0].points==12345 || g->highs[0].points==12346) && g->highs[0].level==7 &&
            g->highs[0].initials[0]=='A' && g->flow_effect==FLOW_TAILS &&
            g->frame_rate==(g->highs[0].points==12345?FPS_30:FPS_60),"EEPROM reboot score/settings mismatch");
        debugf("SAVE SMOKE: recovered high score, flow and %s resolution after reboot\n",g->frame_rate==FPS_30?"high":"low");
        g->highs[0].points=12346; g->frame_rate=FPS_60;
        scores_store(g); /* Exercise alternating slots on subsequent boots. */
        assertf(!g->save_failed,"EEPROM second-slot save failed");
    }
    /* Also exercise both generations without relying on emulator exit to
       flush its save file. Poison RAM so recovery must read the EEPROM. */
    for(unsigned pass=0;pass<2;pass++) {
        uint32_t points=g->highs[0].points;
        FrameRate mode=g->frame_rate;
        memset(g->highs,0,sizeof(g->highs));
        g->flow_effect=FLOW_NONE; g->frame_rate=mode==FPS_30?FPS_60:FPS_30;
        scores_load(g);
        assertf(g->highs[0].points==points && g->highs[0].level==7 &&
            !memcmp(g->highs[0].initials,"ACE",3) && g->flow_effect==FLOW_TAILS &&
            g->frame_rate==mode,"EEPROM fresh-read recovery mismatch");
        if(!pass) {
            g->frame_rate=mode==FPS_30?FPS_60:FPS_30;
            g->highs[0].points=g->frame_rate==FPS_30?12345:12346;
            scores_store(g);
            assertf(!g->save_failed,"EEPROM alternate-slot save failed");
        }
    }
    debugf("SAVE SMOKE PASS: both journal slots, high/low resolution and scores recovered from fresh EEPROM reads\n");
    g->phase=OPTIONS; g->options_selection=1;
}
