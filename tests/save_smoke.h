/* Dedicated EEPROM test ROM only: never enabled in the playable build. */
static void save_smoke(Game *g) {
    assertf(g->save_available,"EEPROM save metadata was not detected");
    if(!g->highs[0].level) {
        g->highs[0]=(HighScore){.points=12345,.level=7,.initials="ACE"};
        g->flow_effect=FLOW_TAILS; g->frame_rate=FPS_30;
        g->scores_dirty=true; scores_store(g);
        assertf(!g->save_failed,"EEPROM save failed");
        debugf("SAVE SMOKE: wrote first-boot high score, flow and 30 FPS\n");
    } else {
        assertf((g->highs[0].points==12345 || g->highs[0].points==12346) && g->highs[0].level==7 &&
            g->highs[0].initials[0]=='A' && g->flow_effect==FLOW_TAILS &&
            g->frame_rate==(g->highs[0].points==12345?FPS_30:FPS_60),"EEPROM reboot score/settings mismatch");
        debugf("SAVE SMOKE: recovered high score, flow and %u FPS after reboot\n",(unsigned)g->frame_rate);
        g->highs[0].points=12346; g->frame_rate=FPS_60;
        scores_store(g); /* Exercise alternating slots on subsequent boots. */
        assertf(!g->save_failed,"EEPROM second-slot save failed");
    }
    g->phase=OPTIONS; g->options_selection=1;
}
