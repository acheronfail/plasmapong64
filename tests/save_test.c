#include "save.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* Independent checksum to construct legacy and malformed-but-valid records. */
static void reseal(uint8_t *data) {
    uint32_t crc=~0u;
    for(int i=0;i<SCORE_SAVE_BYTES;i++) if(i<8 || i>=12) {
        crc^=data[i];
        for(int b=0;b<8;b++) crc=(crc>>1)^(0xedb88320u&(0u-(crc&1)));
    }
    crc=~crc;
    for(int i=11;i>=8;i--) { data[i]=(uint8_t)crc; crc>>=8; }
}
int main(void) {
    HighScore scores[HIGH_SCORE_COUNT]={0},decoded[HIGH_SCORE_COUNT]={0};
    scores[0]=(HighScore){.points=4294967295u,.level=123,.initials="ACE"};
    scores[1]=(HighScore){.points=800,.level=2,.initials="BOB"};
    uint8_t data[SCORE_SAVE_BYTES],bad[SCORE_SAVE_BYTES]; uint32_t generation=0; FlowEffect effect=FLOW_NONE; FrameRate frame_rate=FPS_60;
    score_save_encode(data,scores,42,FLOW_TAILS,FPS_30);
    assert(score_save_decode(data,decoded,&generation,&effect,&frame_rate));
    assert(effect==FLOW_TAILS && frame_rate==FPS_30 && generation==42 && !memcmp(scores,decoded,sizeof(scores)));
    for(unsigned i=0;i<sizeof(data);i++) {
        memcpy(bad,data,sizeof(bad)); bad[i]^=1;
        assert(!score_save_decode(bad,decoded,&generation,&effect,&frame_rate));
        assert(effect==FLOW_TAILS && frame_rate==FPS_30 && generation==42 && !memcmp(scores,decoded,sizeof(scores)));
    }
    /* Every partial body/header write must reject the incomplete new slot. */
    for(unsigned bytes=0;bytes<sizeof(data);bytes+=8) {
        memset(bad,0xff,sizeof(bad)); memcpy(bad+8,data+8,bytes);
        assert(!score_save_decode(bad,decoded,&generation,&effect,&frame_rate));
    }
    for(int choice=0;choice<FLOW_COUNT;choice++) {
        score_save_encode(data,scores,43,(FlowEffect)choice,FPS_60);
        assert(score_save_decode(data,decoded,&generation,&effect,&frame_rate) && effect==(FlowEffect)choice);
    }
    for(int fps=30;fps<=60;fps+=30) {
        score_save_encode(data,scores,44,FLOW_SPEED,(FrameRate)fps);
        assert(score_save_decode(data,decoded,&generation,&effect,&frame_rate) && frame_rate==(FrameRate)fps);
    }
    data[133]=45; reseal(data);
    assert(!score_save_decode(data,decoded,&generation,&effect,&frame_rate));
    memcpy(data,"PPH2",4); reseal(data);
    assert(score_save_decode(data,decoded,&generation,&effect,&frame_rate));
    assert(effect==FLOW_SPEED && frame_rate==FPS_60 && !memcmp(scores,decoded,sizeof(scores)));
    memcpy(data,"PPH1",4); data[132]=255; reseal(data);
    assert(score_save_decode(data,decoded,&generation,&effect,&frame_rate) && effect==FLOW_NONE && frame_rate==FPS_60);
    assert(!memcmp(scores,decoded,sizeof(scores)));
    memcpy(data,"PPH2",4); reseal(data);
    assert(!score_save_decode(data,decoded,&generation,&effect,&frame_rate));
    scores[0].initials[0]='!'; score_save_encode(data,scores,43,FLOW_NONE,FPS_60);
    assert(!score_save_decode(data,decoded,&generation,&effect,&frame_rate));
    memset(scores,0,sizeof(scores)); score_save_encode(data,scores,0,FLOW_NONE,FPS_60);
    assert(score_save_decode(data,decoded,&generation,&effect,&frame_rate) && generation==0);
    puts("PASS: EEPROM record round-trip, corruption, interrupted records, invalid initials, empty table");
}
