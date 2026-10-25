/*
 * The scratch pad: SCRATCH_PAD_SIZE bytes of the kept RAM for the host, which a reset keeps
 * and a trap's or the watchdog's clears. The updater's, the application's without the
 * dispatch lock.
 */
#include <string.h>

#include "keepram.h"
#include "scratchpad.h"

void scratchPadClear(void)
{
	memset(keepRam.scratchPad, 0, SCRATCH_PAD_SIZE);
}

/* copies size bytes at offset into buf; 1 if they are not all in the pad, or too many */
unsigned char scratchPadRead(unsigned int offset, unsigned int size, unsigned char *buf)
{
	if (offset + size > SCRATCH_PAD_SIZE)
		return 1;
	if (size > SCRATCH_PAD_MAX)
		return 1;
	memcpy(buf, &keepRam.scratchPad[offset], size);
	return 0;
}

/* copies size bytes from buf to offset; 1 if they are not all in the pad, or too many */
unsigned char scratchPadWrite(unsigned int offset, unsigned int size, unsigned char *buf)
{
	if (offset + size > SCRATCH_PAD_SIZE)
		return 1;
	if (size > SCRATCH_PAD_MAX)
		return 1;
	memcpy(&keepRam.scratchPad[offset], buf, size);
	return 0;
}
