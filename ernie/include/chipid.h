/*
 * USS-1001's chip ID (app/chipid.c): four bytes of the flash's extra area.
 */
#ifndef CHIPID_H
#define CHIPID_H

#include "board.h"

#if SOURCE_0103
/* USS-1002's 0x0105 reads the area at each chipIdGet: no chipIdInit, no checksum */
#define CHIP_ID_ON_DEMAND	(BOARD_USS1001 && SOURCE_0104)

/* the extra area's eight bytes at 0x850-0x857, read by the flash's sequencer from RAM */
extern void flashReadExtra(unsigned char __near *out);

extern __near unsigned char chipIdExtra[8];

#if !CHIP_ID_ON_DEMAND
extern unsigned char chipIdInit(void);
#endif
extern unsigned char chipIdGet(unsigned char *out);
#endif

#endif
