/*
 * KernelThread.cpp: a kernel thread's side of the waits: the kernel's thread ID and signal,
 * and a table of the kernel threads' slots.
 */
#include "AbstractThread.h"

#define KERNEL_THREAD_ASSERT(line, cond) \
	ULT_ASSERT_AT("KernelThread.cpp", line, #cond, cond)

/* the slots in use, one bit each (0 is never given out) */
static uint32_t s_slotBitmap[128 / 32];

int allocateKernelThreadSlot()
{
	for (int i = 1; i < 128; i++) {
		volatile uint32_t *word = &s_slotBitmap[i / 32];
		uint32_t bit = 1 << (i % 32);
		uint32_t old;

		do {
			old = __ldrex(word);
			if ((old & bit) != 0)
				goto next;
		} while (__strex(old | bit, word) != 0);
		KERNEL_THREAD_ASSERT(42, i<64);
		return i;
next:
		;
	}
	KERNEL_THREAD_ASSERT(47, 0);
	return -1;
}

void freeKernelThreadSlot(int i)
{
	volatile uint32_t *word = &s_slotBitmap[i / 32];
	uint32_t bit = 1 << (i % 32);
	uint32_t old;

	do
		old = __ldrex(word);
	while (__strex(old & ~bit, word) != 0);
}

namespace KernelThread {

uint32_t getID()
{
	return sceKernelGetThreadId();
}

void release(uint32_t tid)
{
}

int waitSignal(uint32_t tid)
{
	return sceKernelWaitSignal(0, NULL);
}

int sendSignal(uint32_t tid)
{
	return sceKernelSendSignal(tid);
}

}
