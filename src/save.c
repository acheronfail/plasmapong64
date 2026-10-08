#include "save.h"
#include <string.h>
static void put32(uint8_t *p,uint32_t n) { for(int i=3;i>=0;i--) { p[i]=(uint8_t)n; n>>=8; } }
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3]; }
static uint32_t checksum(const uint8_t *p) {
    uint32_t crc=~0u;
    for(int i=0;i<SCORE_SAVE_BYTES;i++) {
        if(i>=8 && i<12) continue;
        crc^=p[i];
        for(int bit=0;bit<8;bit++) crc=(crc>>1)^(0xedb88320u&(0u-(crc&1)));
    }
    return ~crc;
}
void score_save_encode(uint8_t data[SCORE_SAVE_BYTES],const HighScore scores[HIGH_SCORE_COUNT],uint32_t generation,FlowEffect effect,FrameRate frame_rate,bool fps_meter) {
    memset(data,0,SCORE_SAVE_BYTES); memcpy(data,"PPH4",4); put32(data+4,generation);
    for(int i=0;i<HIGH_SCORE_COUNT;i++) {
        uint8_t *p=data+12+i*12;
        put32(p,scores[i].points); put32(p+4,scores[i].level);
        memcpy(p+8,scores[i].initials,3);
    }
    data[132]=(uint8_t)effect; data[133]=(uint8_t)frame_rate; data[134]=(uint8_t)fps_meter;
    put32(data+8,checksum(data));
}
bool score_save_decode(const uint8_t data[SCORE_SAVE_BYTES],HighScore scores[HIGH_SCORE_COUNT],uint32_t *generation,FlowEffect *effect,FrameRate *frame_rate,bool *fps_meter) {
    bool legacy=!memcmp(data,"PPH1",4);
    bool current=!memcmp(data,"PPH4",4);
    bool video=current || !memcmp(data,"PPH3",4);
    if((!legacy && !video && memcmp(data,"PPH2",4)) || get32(data+8)!=checksum(data)) return false;
    if(video && data[133]!=FPS_30 && data[133]!=FPS_60) return false;
    if(current && data[134]>1) return false;
    if(!legacy && data[132]>=FLOW_COUNT) return false;
    HighScore decoded[HIGH_SCORE_COUNT]={0};
    for(int i=0;i<HIGH_SCORE_COUNT;i++) {
        const uint8_t *p=data+12+i*12;
        decoded[i].points=get32(p); decoded[i].level=get32(p+4);
        memcpy(decoded[i].initials,p+8,3);
        if(decoded[i].level) {
            for(int j=0;j<3;j++) if(p[8+j]<'A' || p[8+j]>'Z') return false;
        } else if(decoded[i].points) return false;
        if(i && (decoded[i].points>decoded[i-1].points ||
           (decoded[i].points==decoded[i-1].points && decoded[i].level>decoded[i-1].level))) return false;
    }
    memcpy(scores,decoded,sizeof(decoded)); *effect=legacy?FLOW_NONE:(FlowEffect)data[132]; *fps_meter=current && data[134]!=0; *frame_rate=video?(FrameRate)data[133]:FPS_60; *generation=get32(data+4); return true;
}
