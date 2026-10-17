/* Four-core rendezvous. */
#include <stdint.h>
#include <sysmem/cpu.h>
#include <sysmem/barrier.h>
#include <sysmem/atomic.h>

extern int sceKernelCpuId(void);
extern void SceCpuForDriver_BF82DEB2(int *lock);
extern int16_t SceCpuForDriver_3EE9B5B8(int16_t *address, int16_t value);

void SceCpuForKernel_A65F6F14(SceCorelockContext *context)
{
	context->lock = 1;
	context->core_count = 4;
	context->last_wait_core = 4;
}

void SceCpuForKernel_2FF54320(SceCorelockContext *context, SceUInt32 core)
{
	int current = sceKernelCpuId();
	volatile int16_t *owner = &context->last_wait_core;

	AtomicFullBarrier();
	if ((SceUInt32)current == core) {
		*owner = current;
		return;
	}
	while (*owner != (int16_t)current)
		wfe();
	SceCpuForDriver_BF82DEB2(&context->lock);
}

void SceCpuForKernel_FE2B77A3(SceCorelockContext *context)
{
	int next = sceKernelCpuId() + 1;
	volatile int16_t *count = &context->core_count;
	int16_t previous;

	if (next == 4)
		next = 0;
	AtomicFullBarrier();
	context->last_wait_core = next;
	previous = SceCpuForDriver_3EE9B5B8(&context->core_count, 1);
	sceKernelSpinlockLowUnlock(&context->lock);
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
