/*
 * The handlers of the input groups' bits (app/inputbits.c): for each group a constant table,
 * one entry per bit, and how many entries it has.
 */
#ifndef ERNIE_INPUTBITS_H
#define ERNIE_INPUTBITS_H

#include "board.h"
#include "input.h"

/* group 3's bits: DOL-1002 has two */
#if BOARD_DOL1002
#define INPUT_BITS3	2
#else
#define INPUT_BITS3	6
#endif

extern const struct InputBit inputBits1[8];
extern const struct InputBit inputBits2[6];
extern const struct InputBit inputBits3[INPUT_BITS3];

extern unsigned char inputBits1Count(void);
extern unsigned char inputBits2Count(void);
extern unsigned char inputBits3Count(void);

#endif
