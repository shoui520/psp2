/*
 * Interrupts.
 *
 * A thread registers a handler for one of its core's interrupts. The handler
 * runs in the interrupt and returns the event flags to set in that thread; the
 * flags are set once the core leaves the kernel (vkProcessEvents) or, when it
 * is not in the kernel, straight away. Another core's doorbell makes the core
 * look at its run queue again.
 *
 * The entries that save the interrupted registers are kernel/vectors.S.
 */
#include "kernel.h"

VkInterruptTable g_vkInterruptTables[VNZ_CORES] VK_SHARED;

#define VK_HANDLER_REGISTERING	((VkInterruptHandler *)-1)

static volatile uint32_t *const interruptSemaphore = vkSemaphore(VK_SEMAPHORE_INTERRUPTS);

/* the interrupt controller */
void vkInterruptSetEdge(uint32_t interrupt, uint32_t edge)
{
	uint32_t bit = 1 << interrupt;
	uint32_t value = vnzReadControlBus(VNZ_CB_INTERRUPT_EDGE);

	if (edge)
		value |= bit;
	else
		value &= ~bit;
	vnzWriteControlBus(VNZ_CB_INTERRUPT_EDGE, value);
}

void vkInterruptSetLevel(uint32_t interrupt, uint32_t level)
{
	int32_t bank = (int32_t)interrupt >> 3;
	uint32_t shift = (interrupt & 7) * 4;
	uint32_t value;

	if (bank == 1)
		value = vnzReadControlBus(VNZ_CB_INTERRUPT_LEVEL1);
	else if (bank == 0)
		value = vnzReadControlBus(VNZ_CB_INTERRUPT_LEVEL0);
	else
		value = vnzReadControlBus(VNZ_CB_INTERRUPT_LEVEL2);

	value = (value & ~(0xf << shift)) | (level << shift);

	if (bank == 1)
		vnzWriteControlBus(VNZ_CB_INTERRUPT_LEVEL1, value);
	else if (bank == 0)
		vnzWriteControlBus(VNZ_CB_INTERRUPT_LEVEL0, value);
	else
		vnzWriteControlBus(VNZ_CB_INTERRUPT_LEVEL2, value);
}

void vkInterruptSetEnable(uint32_t interrupt, uint32_t enable)
{
	uint32_t bit = 1 << interrupt;
	uint32_t value = vnzReadControlBus(VNZ_CB_INTERRUPT_ENABLE);

	if (enable == 1)
		value |= bit;
	else
		value &= ~bit;
	vnzWriteControlBus(VNZ_CB_INTERRUPT_ENABLE, value);
}

void vkInterruptClear(uint32_t interrupt)
{
	vnzWriteControlBus(VNZ_CB_INTERRUPT_CLEAR, ~(1 << interrupt));
}

/*
 * Register a handler for one of this core's interrupts, for the current thread.
 * Returns the handler, or NULL when the interrupt is taken, the thread is being
 * moved, or there is no memory.
 */
VkInterruptHandler *vkInterruptRegister(uint32_t interrupt, uint32_t level, uint32_t edge,
	uint32_t (*handler)(uint32_t argument, uint32_t argument2),
	uint32_t argument, uint32_t argument2)
{
	VkInterruptTable *table;
	VkInterruptHandler **slot, *registered;
	VkThread *thread;
	uint32_t index, bit;

	vkDisableDispatch();
	index = vnzCoreIndex();
	thread = vkThreadCurrent();
	if (vkThreadAttributes(thread)) {
		vkEnableDispatch();
		return NULL;
	}

	vkTakeSemaphore(interruptSemaphore);
	table = &g_vkInterruptTables[index];
	slot = &table->handlers[interrupt];
	if (*slot) {
		vkGiveSemaphore(interruptSemaphore);
		vkEnableDispatch();
		return NULL;
	}
	*slot = VK_HANDLER_REGISTERING;
	vkGiveSemaphore(interruptSemaphore);

	registered = vkHeapAllocate(vkCurrentHeap(), sizeof(*registered), 0x40);
	bit = 0;
	if (registered) {
		registered->next = NULL;
		registered->core = index;
		registered->interrupt = interrupt;
		registered->level = level;
		registered->edge = edge;
		registered->enabled = 0;
		registered->handler = handler;
		registered->argument = argument;
		registered->argument2 = argument2;
		registered->thread = thread;
		registered->events = 0;
		registered->magic = VK_HANDLER_MAGIC;
		vkCacheFlush((uint32_t)registered, sizeof(*registered));
		mep_syncm();

		vnzDisableInterrupts();
		vkInterruptSetEnable(interrupt, 0);
		vkInterruptSetEdge(interrupt, edge);
		vkInterruptSetLevel(interrupt, level);
		vkInterruptClear(interrupt);
		vnzEnableInterrupts();
		bit = 1 << interrupt;
	}

	vkTakeSemaphore(interruptSemaphore);
	*slot = registered;
	table->registered |= bit;
	vkGiveSemaphore(interruptSemaphore);
	if (bit)
		vkThreadSetInterrupts(bit);
	vkEnableDispatch();
	return registered;
}

void vkInterruptRelease(VkInterruptHandler *handler)
{
	uint32_t interrupt = handler->interrupt;
	uint32_t bit = 1 << interrupt;
	VkInterruptTable *table;

	vkDisableDispatch();
	vkThreadClearInterrupts(bit);
	vkTakeSemaphore(interruptSemaphore);
	table = &g_vkInterruptTables[vnzCoreIndex()];
	table->handlers[interrupt] = NULL;
	table->registered &= ~bit;
	vkGiveSemaphore(interruptSemaphore);
	vkEnableDispatch();

	handler->magic = 0;
	vkCacheWriteback((uint32_t)handler, sizeof(*handler));
	vkHeapFree(vkCurrentHeap(), handler, sizeof(*handler), 0x40);
}

/* Enable or disable a handler's interrupt; only on the handler's own core. */
static int32_t setHandlerEnabled(VkInterruptHandler *handler, int8_t enable)
{
	vkDisableDispatch();
	if (handler->core != vnzCoreIndex()) {
		vkEnableDispatch();
		return -1;
	}
	if (vkThreadAttributes(handler->thread)) {
		vkEnableDispatch();
		return -2;
	}
	if (handler->enabled != enable) {
		vnzDisableInterrupts();
		vkInterruptSetEnable(handler->interrupt, enable);
		vnzEnableInterrupts();
		handler->enabled = enable;
	}
	vkEnableDispatch();
	return 0;
}

int32_t vkInterruptEnable(VkInterruptHandler *handler)
{
	return setHandlerEnabled(handler, 1);
}

int32_t vkInterruptDisable(VkInterruptHandler *handler)
{
	return setHandlerEnabled(handler, 0);
}

/* Copy the exception vectors (at +0x500 in the image) to the image's start. */
/* the copied lines out to memory, when the copy went through a cache */
static void writebackIfCached(uint32_t address, uint32_t size)
{
	uint32_t done;

	if ((address >= VNZ_IMAGE && address < 2 * VNZ_IMAGE) ||
	    (address >= VNZ_DRAM && address < VNZ_DRAM + 0xa8000000)) {
		for (done = 0; done < size; done += 0x40, address += 0x40)
			vnzCacheLine(VNZ_CACHE_WRITEBACK, address);
		mep_syncm();
	}
}

static void copyVectors(void)
{
	extern const uint32_t vkVectors[], vkVectorsEnd[];
	const uint32_t *from = (const uint32_t *)VNZ_PHYSICAL_OF((uint32_t)vkVectors);
	uint32_t *to = (uint32_t *)VNZ_IMAGE;
	uint32_t size = (uint32_t)vkVectorsEnd - (uint32_t)vkVectors;
	uint32_t done;

	for (done = 0; done < size; done += 4)
		*to++ = *from++;
	writebackIfCached(VNZ_IMAGE, size);
}

/* Set up this core's interrupts: the vectors, its interrupt table, its doorbells. */
void vkInterruptsInitialize(uint32_t primary, uint32_t firstStart)
{
	uint32_t index;
	VkCore *core = vkCurrentCore(&index);
	uint32_t i;

	vnzWriteControl(VNZ_CR_CFG, (vnzReadControl(VNZ_CR_CFG) & 0xff3fffe7) | 0xc00018);
	if (primary)
		copyVectors();
	vnzWriteControl(VNZ_CR_RES31, VNZ_IMAGE);	/* the vector base */

	if (firstStart) {
		VkInterruptTable *table = &g_vkInterruptTables[index];

		table->registered = 0;
		for (i = 0; i < VNZ_INTERRUPTS; i++)
			table->handlers[i] = NULL;
		core->eventsTail = NULL;
		core->eventsHead = NULL;
		core->inKernel = 0;
	}
	vkDoorbellInitialize();
	vkDoorbell2Initialize();
}

/* Take the doorbell interrupt for the kernel. */
void vkEnableDoorbell(void)
{
	g_vkInterruptTables[vnzCoreIndex()].handlers[VNZ_INTERRUPT_DOORBELL] = VK_HANDLER_REGISTERING;
	vnzDisableInterrupts();
	vkInterruptSetEdge(VNZ_INTERRUPT_DOORBELL, 0);
	vkInterruptSetLevel(VNZ_INTERRUPT_DOORBELL, 10);
	vkInterruptSetEnable(VNZ_INTERRUPT_DOORBELL, 1);
	vnzEnableInterrupts();
}

/* A core's doorbells: none rung, all masked, none pending. */
void vkDoorbellInitialize(void)
{
	uint32_t bank = VNZ_DOORBELLS + vnzCoreIndex() * 0x20;

	vnzRegister(bank + 0x10) = 0;
	vnzRegister(bank + 0x08) = 0xff;
	vnzRegister(bank + 0x00) = 0;
}

void vkDoorbell2Initialize(void)
{
	uint32_t bank = VNZ_DOORBELLS2 + vnzCoreIndex() * 0x20;

	vnzRegister(bank + 0x10) = 0;
	vnzRegister(bank + 0x08) = 0xff;
	vnzRegister(bank + 0x00) = 0;
}

/*
 * An interrupt (from kernel/vectors.S): run its handler. An interrupt nobody
 * handles is disabled.
 */
void vkInterruptDispatch(uint32_t interrupt)
{
	uint32_t index = vnzCoreIndex() & 7;
	VkInterruptHandler *handler = g_vkInterruptTables[index].handlers[interrupt];
	uint32_t events, bit;
	VkCore *core;

	if (handler == VK_HANDLER_REGISTERING || handler == NULL) {
		bit = ~(1 << interrupt);
		vnzWriteControlBus(VNZ_CB_INTERRUPT_ENABLE,
			vnzReadControlBus(VNZ_CB_INTERRUPT_ENABLE) & bit);
		vnzWriteControlBus(VNZ_CB_INTERRUPT_EDGE,
			vnzReadControlBus(VNZ_CB_INTERRUPT_EDGE) & bit);
		vnzWriteControlBus(VNZ_CB_INTERRUPT_CLEAR, bit);
		return;
	}

	events = handler->handler(handler->argument, handler->argument2);
	if (events == 0)
		return;

	core = &g_vkCores[index];
	if (handler->events == 0) {
		if (core->eventsHead)
			core->eventsTail->next = handler;
		else
			core->eventsHead = handler;
		core->eventsTail = handler;
	}
	handler->events |= events;
	if (!core->inKernel)
		vkInterruptEvents(core);
}

/* Another core rang (from kernel/vectors.S). */
void vkDoorbellDispatch(void)
{
	uint32_t index = vnzCoreIndex() & 7;
	uint32_t bank = VNZ_DOORBELLS + index * 0x20;
	VkCore *core = &g_vkCores[index];

	vnzRegister(bank) = 0;
	(void)vnzRegister(bank);
	if (core->dispatchDisable == 0)
		vkReschedule(core);
	else
		core->reschedulePending = 1;
}

/* Pass on the events of the handlers that ran while the core was outside the kernel. */
void vkInterruptEvents(VkCore *core)
{
	uint32_t index;

	vkCurrentCore(&index);
	core->dispatchDisable++;
	core->inKernel = 1;
	vkDeliverEvents(index, core);
	core->inKernel = 0;
	if (--core->dispatchDisable == 0 && core->reschedulePending == 1) {
		core->reschedulePending = 0;
		vkReschedule(core);
	}
}

/*
 * Set the queued handlers' events in their threads; called with interrupts
 * disabled, and runs each with them enabled.
 */
void vkDeliverEvents(uint32_t index, VkCore *core)
{
	volatile uint32_t *semaphore = vkSemaphore(VK_SEMAPHORE_THREADS + index);
	VkInterruptHandler *handler;
	VkThread *thread;
	uint32_t events, readied = 0;

	do {
		handler = core->eventsHead;
		core->eventsHead = handler->next;
		events = handler->events;
		thread = handler->thread;
		handler->events = 0;
		handler->next = NULL;
		vnzEnableInterrupts();

		vkTakeSemaphore(semaphore);
		readied |= vkWakeFromInterrupt(index, thread, thread->physical, events);
		mep_syncm();
		vkGiveSemaphore(semaphore);
		vnzDisableInterrupts();
	} while (core->eventsHead);

	if (readied)
		core->reschedulePending = 1;
}

/* Leaving the kernel: pass on the events that waited meanwhile. */
void vkProcessEvents(VkCore *core)
{
	vnzDisableInterrupts();
	if (core->eventsHead)
		vkDeliverEvents(core->index, core);
	core->inKernel = 0;
	vnzEnableInterrupts();
}

/*
 * Reserve interrupts on a core for a thread moving there (kernel/service.c).
 * Returns 0, or the core's registered interrupts when one is taken.
 */
uint32_t vkInterruptsReserve(uint32_t index, uint32_t interrupts)
{
	VkInterruptTable *table;
	uint32_t registered, i;

	vkTakeSemaphore(interruptSemaphore);
	table = &g_vkInterruptTables[index];
	registered = table->registered;
	if (registered & interrupts) {
		vkGiveSemaphore(interruptSemaphore);
		return registered;
	}
	for (i = 0; i < VNZ_INTERRUPTS; i++)
		if (interrupts & (1 << i))
			table->handlers[i] = VK_HANDLER_REGISTERING;
	table->registered |= interrupts;
	vkGiveSemaphore(interruptSemaphore);
	return 0;
}

/*
 * Stand in for a moving thread's handlers, so their events wait in the handler
 * until the thread arrives (vkInterruptsMove).
 */
void vkInterruptsHook(VkMigration *migration)
{
	uint32_t interrupts = migration->thread->interrupts;
	uint32_t index = vnzCoreIndex();
	VkInterruptHandler **slot, *handler, *shared;
	uint32_t i;

	for (i = 0; i < VNZ_INTERRUPTS; i++) {
		if (!(interrupts & (1 << i)))
			continue;
		slot = &g_vkInterruptTables[index].handlers[i];
		handler = *slot;
		shared = (VkInterruptHandler *)vkPhysicalToPublic((uint32_t)handler);
		handler->hookedArgument = handler->argument;
		handler->hookedHandler = handler->handler;
		handler->hookedArgument2 = handler->argument2;
		vnzDisableInterrupts();
		handler->argument = (uint32_t)migration;
		handler->argument2 = (uint32_t)handler;
		handler->handler = vkInterruptHookHandler;
		vkCacheWriteback((uint32_t)handler, sizeof(*handler));
		mep_syncm();
		*slot = shared;
		vnzEnableInterrupts();
	}
}

/* The stand-in handler: run the real one, and keep its events. */
uint32_t vkInterruptHookHandler(uint32_t argument, uint32_t argument2)
{
	VkMigration *migration = (VkMigration *)argument;
	VkInterruptHandler *handler = (VkInterruptHandler *)argument2;
	volatile uint8_t *lock = migration->lock;
	uint32_t events;
	volatile int32_t i;

	while (vnzTestAndSet(lock))
		for (i = 100; i; i--)
			;
	if (handler->hookedHandler) {
		events = handler->hookedHandler(handler->hookedArgument, handler->hookedArgument2);
		if (events)
			handler->events |= events;
	}
	vnzCacheLine(VNZ_CACHE_WRITEBACK, handler);
	mep_syncm();
	*lock = 0;
	return 0;
}

/* Move a thread's handlers to the core it moved to, and restore them. */
void vkInterruptsMove(VkMigration *migration)
{
	uint32_t interrupts = migration->thread->interrupts;
	uint32_t from = migration->from;
	uint32_t to = migration->to;
	volatile uint8_t *lock = migration->lock;
	VkInterruptTable *fromTable = &g_vkInterruptTables[from];
	VkInterruptTable *toTable;
	VkInterruptHandler *handler;
	VkCore *core;
	uint32_t bit, i;

	for (i = 0; i < VNZ_INTERRUPTS; i++) {
		bit = 1 << i;
		if (!(interrupts & bit))
			continue;

		vnzDisableInterrupts();
		vkSpinLock(lock);
		handler = (VkInterruptHandler *)vkPublicToPhysical(
			(uint32_t)fromTable->handlers[i]);
		vkInterruptSetEdge(i, handler->edge);
		vkInterruptSetLevel(i, handler->level);
		vkInterruptClear(i);

		vkTakeSemaphore(interruptSemaphore);
		fromTable->handlers[i] = NULL;
		fromTable->registered &= ~bit;

		if (handler->events) {
			core = &g_vkCores[to];
			if (core->eventsHead)
				core->eventsTail->next = handler;
			else
				core->eventsHead = handler;
			core->eventsTail = handler;
		}

		handler->argument = handler->hookedArgument;
		handler->handler = handler->hookedHandler;
		handler->hookedArgument = 0;
		handler->hookedHandler = NULL;
		handler->argument2 = handler->hookedArgument2;
		handler->hookedArgument2 = 0;
		handler->core = to;
		toTable = &g_vkInterruptTables[to];
		toTable->handlers[i] = handler;
		toTable->registered |= bit;
		vkCacheWriteback((uint32_t)handler, sizeof(*handler));
		mep_syncm();
		vkGiveSemaphore(interruptSemaphore);

		vkInterruptSetEnable(i, handler->enabled);
		vkSpinUnlock(lock);
		vnzEnableInterrupts();
	}
}
