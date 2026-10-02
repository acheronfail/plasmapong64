#ifndef SAVE_H
#define SAVE_H
#include "game.h"
#define SCORE_SAVE_BYTES 144
/* Two versioned, checksummed records fit in a 4-kilobit EEPROM. */
void score_save_encode(uint8_t data[SCORE_SAVE_BYTES],const HighScore scores[HIGH_SCORE_COUNT],uint32_t generation);
bool score_save_decode(const uint8_t data[SCORE_SAVE_BYTES],HighScore scores[HIGH_SCORE_COUNT],uint32_t *generation);
void scores_load(Game *g);
void scores_store(Game *g);
#endif
