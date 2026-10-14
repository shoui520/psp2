/*
 * The cores: who is running, the hardware semaphores, spin locks and the
 * dispatch lock.
 */
#include "kernel.h"

VkCore g_vkCores[VNZ_CORES] VK_LOCAL;

/* Record the core's type and index from the configuration (the primary core does it). */
void vkCoreInitialize(uint32_t primary)
{
	uint32_t index = vnzCoreIndex();

	if (primary) {
		g_vkCores[index].index = index;
		g_vkCores[index].type = VK_CONFIGURATION->coreTable->cores[index].type;
	}
}

uint32_t vkCurrentCoreId(void)
{
	return vnzCoreId();
}

uint32_t vkCoreType(uint32_t coreId)
{
	return VK_CONFIGURATION->coreTable->cores[coreId - VNZ_FIRST_CORE_ID].type;
}

VkCore *vkCurrentCore(uint32_t *index)
{
	*index = vnzCoreIndex();
	return &g_vkCores[*index];
}

/* Try a test-and-set lock: 0 when it was free (and is now taken). */
uint32_t vkSpinTry(volatile uint8_t *lock)
{
	return vnzTestAndSet(lock);
}

void vkSpinUnlock(volatile uint8_t *lock)
{
	mep_syncm();
	*lock = 0;
}

/* Take a test-and-set lock, backing off a little longer after each miss. */
void vkSpinLock(volatile uint8_t *lock)
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
int32_t vkTrySemaphore(volatile uint32_t *semaphore)
{
	return *semaphore == 0 ? 0 : -1;
}

void vkGiveSemaphore(volatile uint32_t *semaphore)
{
	*semaphore = 0;
}

void vkTakeSemaphore(volatile uint32_t *semaphore)
{
	volatile int32_t i;

	while (*semaphore != 0)
		for (i = 0; i < 10; i++)
			;
}

/* Hold off thread switches on this core; interrupts still run. */
void vkDisableDispatch(void)
{
	vnzDisableInterrupts();
	g_vkCores[vnzCoreIndex()].dispatchDisable++;
	vnzEnableInterrupts();
}

/* Allow them again, and switch now if a switch waited. */
void vkEnableDispatch(void)
{
	VkCore *core;

	vnzDisableInterrupts();
	core = &g_vkCores[vnzCoreIndex()];
	if (--core->dispatchDisable == 0 && core->reschedulePending == 1) {
		core->reschedulePending = 0;
		vkReschedule(core);
	}
	vnzEnableInterrupts();
}
