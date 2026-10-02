#include "save.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    HighScore scores[HIGH_SCORE_COUNT]={0},decoded[HIGH_SCORE_COUNT]={0};
    scores[0]=(HighScore){.points=4294967295u,.level=123,.initials="ACE"};
    scores[1]=(HighScore){.points=800,.level=2,.initials="BOB"};
    uint8_t data[SCORE_SAVE_BYTES],bad[SCORE_SAVE_BYTES]; uint32_t generation=0;
    score_save_encode(data,scores,42);
    assert(score_save_decode(data,decoded,&generation));
    assert(generation==42 && !memcmp(scores,decoded,sizeof(scores)));
    for(unsigned i=0;i<sizeof(data);i++) {
        memcpy(bad,data,sizeof(bad)); bad[i]^=1;
        assert(!score_save_decode(bad,decoded,&generation));
        assert(generation==42 && !memcmp(scores,decoded,sizeof(scores)));
    }
    /* Every partial body/header write must reject the incomplete new slot. */
    for(unsigned bytes=0;bytes<sizeof(data);bytes+=8) {
        memset(bad,0xff,sizeof(bad)); memcpy(bad+8,data+8,bytes);
        assert(!score_save_decode(bad,decoded,&generation));
    }
    scores[0].initials[0]='!'; score_save_encode(data,scores,43);
    assert(!score_save_decode(data,decoded,&generation));
    memset(scores,0,sizeof(scores)); score_save_encode(data,scores,0);
    assert(score_save_decode(data,decoded,&generation) && generation==0);
    puts("PASS: EEPROM record round-trip, corruption, interrupted records, invalid initials, empty table");
}
