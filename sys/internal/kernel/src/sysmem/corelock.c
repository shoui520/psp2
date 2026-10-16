/* Four-core rendezvous. */
#include <stdint.h>
#include <psp2kern/kernel/cpu/corelock.h>
#include <sce/barrier.h>
#include "../../../../../common/sysclib/atomic.h"

extern int ksceKernelCpuId(void);
extern void ksceKernelSpinlockLowLock(int *lock);
extern void ksceKernelSpinlockLowUnlock(int *lock);
extern int16_t ksceKernelAtomicGetAndSub16(int16_t *address, int16_t value);

void ksceKernelCorelockInitialize(SceCorelockContext *context)
{
	context->lock = 1;
	context->core_count = 4;
	context->last_wait_core = 4;
}

void ksceKernelCorelockLock(SceCorelockContext *context, SceUInt32 core)
{
	int current = ksceKernelCpuId();
	volatile int16_t *owner = &context->last_wait_core;

	AtomicFullBarrier();
	if ((SceUInt32)current == core) {
		*owner = current;
		return;
	}
	while (*owner != (int16_t)current)
		wfe();
	ksceKernelSpinlockLowLock(&context->lock);
}

void ksceKernelCorelockUnlock(SceCorelockContext *context)
{
	int next = ksceKernelCpuId() + 1;
	volatile int16_t *count = &context->core_count;
	int16_t previous;

	if (next == 4)
		next = 0;
	AtomicFullBarrier();
	context->last_wait_core = next;
	previous = ksceKernelAtomicGetAndSub16(&context->core_count, 1);
	ksceKernelSpinlockLowUnlock(&context->lock);
	dsb();
	sev();
	if (previous == 4) {
		while (*count > 0)
			wfe();
		context->lock = 1;
		context->core_count = 4;
		context->last_wait_core = 4;
		dsb();
		sev();
	} else {
		while (*count != 4)
			wfe();
	}
}
