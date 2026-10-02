/* Dedicated EEPROM test ROM only: never enabled in the playable build. */
static void save_smoke(Game *g) {
    assertf(g->save_available,"EEPROM save metadata was not detected");
    if(!g->highs[0].level) {
        g->highs[0]=(HighScore){.points=12345,.level=7,.initials="ACE"};
        g->scores_dirty=true; scores_store(g);
        assertf(!g->save_failed,"EEPROM save failed");
        debugf("SAVE SMOKE: wrote first-boot high score\n");
    } else {
        assertf(g->highs[0].points==12345 && g->highs[0].level==7 &&
            g->highs[0].initials[0]=='A',"EEPROM reboot score mismatch");
        debugf("SAVE SMOKE: recovered high score after reboot\n");
        scores_store(g); /* Exercise alternating slots on subsequent boots. */
        assertf(!g->save_failed,"EEPROM second-slot save failed");
    }
    g->phase=SCORES;
}
