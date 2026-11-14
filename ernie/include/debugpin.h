/*
 * P4.0, the chip's TOOL0 (its on-chip debugger's line): since 0x0100060B a level the firmware
 * drives for a probe to watch (app/debugpin.c), toggled at each of task 3's input scans and
 * high while hdwinit waits for the main oscillator.
 */
#ifndef ERNIE_DEBUGPIN_H
#define ERNIE_DEBUGPIN_H

#include "ernie.h"

extern void debugPinInit(void);
extern void debugPinReserved(unsigned int value);
extern void debugPinSet(unsigned int level);

#endif
