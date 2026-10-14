/* Copied into the boot ROM window before both passes over main memory. */

#include "ipl.h"

void __attribute__((section(".text.halt"), noreturn))
haltStub(void *memBase, SceSize memSize)
{
	volatile u32 *first = memBase;
	volatile u32 *last = (volatile u32 *)((u32)memBase + memSize - 4);
	volatile u32 *address = first;

	do {
		*address = 0;
	} while (address++ != last);

	address = first;
	do {
		*address = 0;
	} while (address++ != last);

	__asm__ __volatile__("break" ::: "memory");
	__builtin_unreachable();
}
