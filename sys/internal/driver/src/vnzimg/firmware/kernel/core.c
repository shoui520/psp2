/*
 * The cores: who is running, the hardware semaphores, spin locks and the
 * dispatch lock.
 */
#include "kernel.h"

VnzKernelCore g_vnzKernelCores[VNZ_CORES] VNZ_KERNEL_LOCAL;

/* Record the core's type and index from the configuration (the primary core does it). */
void vnzKernelCoreInitialize(uint32_t primary)
{
	uint32_t index = vnzCoreIndex();

	if (primary) {
		g_vnzKernelCores[index].index = index;
		g_vnzKernelCores[index].type = VNZ_KERNEL_CONFIGURATION->coreTable->cores[index].type;
	}
}

uint32_t vnzKernelCurrentCoreId(void)
{
	return vnzCoreId();
}

uint32_t vnzKernelCoreType(uint32_t coreId)
{
	return VNZ_KERNEL_CONFIGURATION->coreTable->cores[coreId - VNZ_FIRST_CORE_ID].type;
}

VnzKernelCore *vnzKernelCurrentCore(uint32_t *index)
{
	*index = vnzCoreIndex();
	return &g_vnzKernelCores[*index];
}

/* Try a test-and-set lock: 0 when it was free (and is now taken). */
uint32_t vnzKernelSpinTry(volatile uint8_t *lock)
{
	return vnzTestAndSet(lock);
}

void vnzKernelSpinUnlock(volatile uint8_t *lock)
{
	mep_syncm();
	*lock = 0;
}

/* Take a test-and-set lock, backing off a little longer after each miss. */
void vnzKernelSpinLock(volatile uint8_t *lock)
{
	int32_t backoff = 10;
	volatile int32_t i;

	while (vnzTestAndSet(lock)) {
		for (i = 0; i < backoff; i++)
			;
		backoff++;
	}
}

/* A hardware semaphore: reading it takes it, and reads 0 when it was free. */
int32_t vnzKernelTrySemaphore(volatile uint32_t *semaphore)
{
	return *semaphore == 0 ? 0 : -1;
}

void vnzKernelGiveSemaphore(volatile uint32_t *semaphore)
{
	*semaphore = 0;
}

void vnzKernelTakeSemaphore(volatile uint32_t *semaphore)
{
	volatile int32_t i;

	while (*semaphore != 0)
		for (i = 0; i < 10; i++)
			;
}

/* Hold off thread switches on this core; interrupts still run. */
void vnzKernelDisableDispatch(void)
{
	vnzDisableInterrupts();
	g_vnzKernelCores[vnzCoreIndex()].dispatchDisable++;
	vnzEnableInterrupts();
}

/* Allow them again, and switch now if a switch waited. */
void vnzKernelEnableDispatch(void)
{
	VnzKernelCore *core;

	vnzDisableInterrupts();
	core = &g_vnzKernelCores[vnzCoreIndex()];
	if (--core->dispatchDisable == 0 && core->reschedulePending == 1) {
		core->reschedulePending = 0;
		vnzKernelReschedule(core);
	}
	vnzEnableInterrupts();
}
