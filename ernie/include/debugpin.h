/*
 * P4.0, the chip's TOOL0 (its on-chip debugger's line): since 0x0100060B a level the firmware
 * drives for a probe to watch (app/debugpin.c), toggled at each of task 3's input scans and
 * high while hdwinit waits for the main oscillator.
 */
#ifndef ERNIE_DEBUGPIN_H
#define ERNIE_DEBUGPIN_H

#include "ernie.h"
#include "board.h"

extern void debugPinInit(void);
extern void debugPinReserved(unsigned int value);
extern void debugPinSet(unsigned int level);
extern void debugPinCode(unsigned int code);
#if SOURCE_0103
extern void debugPinWrite(unsigned int value);
extern unsigned int debugPinRead(void);
extern void debugPinWriteBit(unsigned char pin, unsigned char level);
#endif

#endif
