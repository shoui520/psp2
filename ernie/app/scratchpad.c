/*
 * The scratch pad: SCRATCH_PAD_SIZE bytes of the kept RAM for the host, which a reset keeps
 * and a trap's or the watchdog's clears (task1.c).
 */
#include <string.h>

#include "keepram.h"
#include "kernel.h"
#include "scratchpad.h"

void scratchPadClear(void)
{
	dis_dsp();
	memset(keepRam.scratchPad, 0, SCRATCH_PAD_SIZE);
	ena_dsp();
}

/* copies size bytes at offset into buf; 1 if they are not all in the pad, or too many */
unsigned char scratchPadRead(unsigned int offset, unsigned int size, unsigned char *buf)
{
	if (offset + size > SCRATCH_PAD_SIZE)
		return 1;
	if (size > SCRATCH_PAD_MAX)
		return 1;
	dis_dsp();
	memcpy(buf, &keepRam.scratchPad[offset], size);
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
	memcpy(&keepRam.scratchPad[offset], buf, size);
	ena_dsp();
	return 0;
}
