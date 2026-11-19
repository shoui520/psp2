/*
 * USS-1001's chip ID (app/chipid.c): four bytes of the flash's extra area.
 */
#ifndef CHIPID_H
#define CHIPID_H

#include "board.h"

#if SOURCE_0103
/* the extra area's eight bytes at 0x850-0x857, read by the flash's sequencer from RAM */
extern void flashReadExtra(unsigned char __near *out);

extern __near unsigned char chipIdExtra[8];

extern unsigned char chipIdInit(void);
extern unsigned char chipIdGet(unsigned char *out);
#endif

#endif
