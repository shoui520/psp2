/*
 * SceVoiceQoS: the C library function the compiler calls, on SceLibKernel.
 */

#include <kernel/libkernel.h>

void *memset(void *dest, int value, SceSize size)
{
	return sceClibMemset(dest, value, size);
}
