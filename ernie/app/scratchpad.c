/*
 * The scratch pad: SCRATCH_PAD_SIZE bytes of the kept RAM for the host, which a reset keeps
 * and a trap's or the watchdog's clears (task1.c).
 */
#include <string.h>

#include "ernie.h"
#include "keepram.h"
#include "kernel.h"
#include "scratchpad.h"

#if !FW_SINCE(0x00080302)
/* 0x00080003 keeps the pad in the RAM cstart clears, not in the kept block */
__far unsigned char scratchPad[SCRATCH_PAD_SIZE];
#define SCRATCH_PAD	scratchPad
#elif FW_0302
/* 0x00080302's pad: RAM of its own, not kept over a reset, and no clear */
static unsigned char scratchPad[SCRATCH_PAD_SIZE];
#define SCRATCH_PAD	scratchPad
#else
#define SCRATCH_PAD	keepRam.scratchPad
#endif

#if !FW_0302
void scratchPadClear(void)
{
	dis_dsp();
	memset(SCRATCH_PAD, 0, SCRATCH_PAD_SIZE);
	ena_dsp();
}
#endif

/* copies size bytes at offset into buf; 1 if they are not all in the pad, or too many */
unsigned char scratchPadRead(unsigned int offset, unsigned int size, unsigned char *buf)
{
	if (offset + size > SCRATCH_PAD_SIZE)
		return 1;
	if (size > SCRATCH_PAD_MAX)
		return 1;
	dis_dsp();
	memcpy(buf, &SCRATCH_PAD[offset], size);
	ena_dsp();
	return 0;
}

/* copies size bytes from buf to offset; 1 if they are not all in the pad, or too many */
unsigned char scratchPadWrite(unsigned int offset, unsigned int size, unsigned char *buf)
{
	if (offset + size > SCRATCH_PAD_SIZE)
		return 1;
	if (size > SCRATCH_PAD_MAX)
		return 1;
	dis_dsp();
	memcpy(&SCRATCH_PAD[offset], buf, size);
	ena_dsp();
	return 0;
}
