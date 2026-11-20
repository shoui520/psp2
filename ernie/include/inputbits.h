/*
 * The handlers of the input groups' bits (app/inputbits.c): for each group a constant table,
 * one entry per bit, and how many entries it has.
 */
#ifndef ERNIE_INPUTBITS_H
#define ERNIE_INPUTBITS_H

#include "board.h"
#include "input.h"

/* group 3's bits: DOL-1002 and the slim's 0x0105 (USS-1002) have two */
#if BOARD_DOL1002 || BOARD_USS1001 && SOURCE_0104
#define INPUT_BITS3	2
#else
#define INPUT_BITS3	6
#endif
/* group 2's bits: the slim's 0x0105 has four, none with handlers (its BBMC is a stub) */
#if BOARD_USS1001 && SOURCE_0104
#define INPUT_BITS2	4
#define INPUT_BITS2_HANDLERS	0
#else
#define INPUT_BITS2	6
#define INPUT_BITS2_HANDLERS	1
#endif

extern const struct InputBit inputBits1[8];
extern const struct InputBit inputBits2[INPUT_BITS2];
extern const struct InputBit inputBits3[INPUT_BITS3];

extern unsigned char inputBits1Count(void);
extern unsigned char inputBits2Count(void);
extern unsigned char inputBits3Count(void);

#endif
