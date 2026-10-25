/*
 * The scratch pad (app/scratchpad.c): bytes of the kept RAM the host reads and writes
 * (sceSysconReadScratchPad, sceSysconWriteScratchPad), at most SCRATCH_PAD_MAX at a time.
 */
#ifndef ERNIE_SCRATCHPAD_H
#define ERNIE_SCRATCHPAD_H

#define SCRATCH_PAD_SIZE	0x100
#define SCRATCH_PAD_MAX		0x20	/* the most bytes one request moves */

extern void scratchPadClear(void);
extern unsigned char scratchPadRead(unsigned int offset, unsigned int size, unsigned char *buf);
extern unsigned char scratchPadWrite(unsigned int offset, unsigned int size, unsigned char *buf);

#endif
