/*
 * The application's start, its threads and the RPC servers.
 *
 * The ARM calls a function of the table (g_rpcFunctions, 0x200 entries) by
 * filling a slot of the shared RAM (SPRAM) and ringing one of eight
 * doorbells; a ninth doorbell is for the system functions (g_systemFunctions).
 * The interrupt thread takes the doorbells and wakes the slot's server thread,
 * which runs the function on the slot's arguments and rings back.
 */
#include "application.h"

#define SPRAM			0x1f840000	/* physically */
#define SPRAM_UNCACHED		0xf1840000
#define SPRAM_SLOTS		0x400
#define SPRAM_SLOT_SIZE		0x100

/* the doorbells, by slot: from the ARM, and back to it */
#define DOORBELL_IN		0xe3106110
#define DOORBELL_IN_SYSTEM	0xe3106130
#define DOORBELL_OUT		0xe3106010

#define DOORBELL_INTERRUPT	15
#define DOORBELL_LEVEL		15

/* the application's state for the ARM (SPRAM word 0) */
#define STATE_READY		0x8
#define STATE_POWERED_DOWN	0x10000
#define STATE_SUSPENDED		0x80000

/* the threads */
#define SERVERS			8	/* one per slot */
#define SYSTEM_SERVER		8
#define INTERRUPT_THREAD	9
#define THREADS			10

/* thread flags */
#define FLAG_SLOT(slot)		(1 << (slot))	/* to the interrupt thread */
#define FLAG_SYSTEM		0x100
#define FLAG_REPLY		0x10000
#define FLAG_DOORBELL		0x4000000
#define FLAG_EXIT		0x8000000

/* a call's slot in the SPRAM */
typedef struct RpcSlot {
	uint32_t command;		/* the function, and the RPC_* bits */
	uint32_t reserved4;
	uint32_t arguments[4];
	int32_t result;
	uint32_t status;		/* RPC_STARTED, RPC_DONE */
} RpcSlot;

#define RPC_FUNCTION		0x1ff
#define RPC_SYSTEM_FUNCTION	0x1f
#define RPC_NOTIFY_DONE		0x10000
#define RPC_NOTIFY_STARTED	0x20000
#define RPC_STARTED		0x1
#define RPC_DONE		0x2

#define RPC_NOT_FOUND		(-2003)

/* the doorbells the interrupt took, by slot, and the last slot served */
typedef struct Doorbells {
	uint32_t values[SERVERS + 1];
	uint32_t reserved24[7];
	uint32_t last;
} Doorbells;

typedef int32_t (*RpcFunction)(uint32_t, uint32_t, uint32_t, uint32_t);

typedef struct ThreadEntry {
	void (*entry)(uint32_t argument, uint32_t argument2);
	uint32_t priority;
	uint32_t stackSize;
} ThreadEntry;

extern const RpcFunction g_rpcFunctions[0x200];
static const RpcFunction g_systemFunctions[0x20];
static void serverThread(uint32_t slot, uint32_t argument2);
static void systemServerThread(uint32_t slot, uint32_t argument2);
static void interruptThread(uint32_t argument, uint32_t argument2);

static const ThreadEntry g_threadEntries[THREADS] = {
	{ serverThread, 4, 0x400 },
	{ serverThread, 4, 0x400 },
	{ serverThread, 4, 0x400 },
	{ serverThread, 4, 0x400 },
	{ serverThread, 4, 0x400 },
	{ serverThread, 4, 0x400 },
	{ serverThread, 4, 0x400 },
	{ serverThread, 4, 0x400 },
	{ systemServerThread, 4, 0x400 },
	{ interruptThread, 8, 0x400 },
};

static int32_t createThreads(int32_t *threads)
{
	const ThreadEntry *entry;
	int32_t result = 0;
	uint32_t i;

	for (i = 0; i < THREADS; i++)
		threads[i] = -1;
	for (i = 0; i < THREADS; i++) {
		entry = &g_threadEntries[i];
		threads[i] = vnzThreadCreate(entry->entry, vnzCurrentCoreId(), entry->priority,
			entry->stackSize);
		if (threads[i] == -1) {
			result = -10001;
			break;
		}
	}
	return result;
}

static void deleteThreads(int32_t *threads)
{
	uint32_t i;

	while (vnzThreadWaitChildren() != 0)
		;
	for (i = 0; i < THREADS; i++) {
		if (threads[i] != -1) {
			vnzThreadDelete((VkThread *)threads[i]);
			threads[i] = -1;
		}
	}
}

/* the SPRAM's state words */
uint32_t getState(void)
{
	return *(volatile uint32_t *)vnzToUncached(SPRAM);
}

void setState(uint32_t state)
{
	*(volatile uint32_t *)vnzToUncached(SPRAM) = state;
}

uint32_t getCoreCount(void)
{
	return ((volatile uint32_t *)vnzToUncached(SPRAM))[1];
}

void setCoreCount(uint32_t count)
{
	((volatile uint32_t *)vnzToUncached(SPRAM))[1] = count;
}

static volatile RpcSlot *rpcSlot(uint32_t slot)
{
	return (volatile RpcSlot *)(vnzToUncached(SPRAM) + SPRAM_SLOTS + slot * SPRAM_SLOT_SIZE);
}

static RpcFunction rpcFunction(uint32_t id)
{
	if (id < 0x200)
		return g_rpcFunctions[id];
	return 0;
}

static void ringBack(uint32_t slot, uint32_t value)
{
	((volatile uint32_t *)DOORBELL_OUT)[slot] = value;
}

/* The ARM went down to no core: tell it. */
static void powerCallback(uint32_t cores, uint32_t argument)
{
	if (cores == 0)
		*(volatile uint32_t *)SPRAM_UNCACHED = STATE_POWERED_DOWN;
}

/*
 * The doorbell interrupt: take the system doorbell, or the next slot's
 * after the last one served, and wake the interrupt thread for it.
 */
static uint32_t doorbellInterrupt(uint32_t argument, uint32_t argument2)
{
	Doorbells *doorbells = (Doorbells *)argument;
	volatile uint32_t *doorbell = (volatile uint32_t *)DOORBELL_IN_SYSTEM;
	uint32_t value, slot, i;

	value = *doorbell;
	if (value) {
		slot = SYSTEM_SERVER;
		*doorbell = value;
		doorbells->values[SYSTEM_SERVER] = value;
		value = *doorbell;
	} else {
		slot = doorbells->last;
		for (i = 0; i < SERVERS; i++) {
			slot = (slot + 1) & 7;
			doorbell = (volatile uint32_t *)DOORBELL_IN + slot;
			value = *doorbell;
			if (value) {
				*doorbell = value;
				doorbells->values[slot] = value;
				doorbells->last = slot;
				value = *doorbell;
				break;
			}
		}
	}
	vnzWriteControlBus(VNZ_CB_INTERRUPT_CLEAR,
		vnzReadControlBus(VNZ_CB_INTERRUPT_CLEAR) & ~(1 << DOORBELL_INTERRUPT));
	return FLAG_DOORBELL | FLAG_SLOT(slot);
}

/*
 * The interrupt thread: sets the heaps up, starts the servers, then passes
 * each doorbell to its server until told to exit.
 */
static void interruptThread(uint32_t argument, uint32_t argument2)
{
	VkThread **threads = (VkThread **)argument;
	VkThread *parent = (VkThread *)argument2;
	VkInterruptHandler *interrupt = NULL;
	Doorbells *doorbells = NULL;
	VkPowerHook *power;
	uint32_t flags, slots, slot, size, base, i;

	power = vnzPowerCallbackCreate(powerCallback, 0);
	if (power == NULL)
		goto exit;
	vnzPowerCallbackRegister(power);

	base = vnzMemoryRegion(VK_MEMORY_HEAP, &size);
	if (vnzHeapCreatePool(base, size) == NULL)
		goto release;
	base = vnzMemoryRegion(VK_MEMORY_WORK, &size);
	if (vnzHeapCreatePool(base, size) == NULL)
		goto release;
	doorbells = vnzAllocateStack(sizeof(Doorbells));
	if (doorbells == NULL)
		goto release;
	memset(doorbells, 0, sizeof(Doorbells));
	doorbells->last = 7;
	for (i = 0; i < SERVERS + 1; i++)
		vnzThreadStart(threads[i], i, (uint32_t)doorbells);

	interrupt = vnzInterruptRegister(DOORBELL_INTERRUPT, DOORBELL_LEVEL, 0, doorbellInterrupt,
		(uint32_t)doorbells, 0);
	if (interrupt == NULL)
		goto exit;
	if (vnzInterruptEnable(interrupt) != 0)
		goto release;
	vnzThreadSetFlags(parent, FLAG_DOORBELL);

	do {
		flags = vnzThreadWaitFlags(FLAG_EXIT | FLAG_DOORBELL | FLAG_SYSTEM | 0xff);
		if (flags & FLAG_DOORBELL) {
			if (flags & FLAG_SYSTEM)
				vnzThreadSetFlags(threads[SYSTEM_SERVER], FLAG_DOORBELL);
			slots = flags & 0xff;
			if (slots) {
				slot = 0;
				for (i = 0; i < SERVERS; i++) {
					if (slots & (1 << slot))
						vnzThreadSetFlags(threads[slot], FLAG_DOORBELL);
					slot = (slot + 1) & 7;
				}
			}
		}
	} while (!(flags & FLAG_EXIT));

release:
	if (interrupt) {
		vnzInterruptDisable(interrupt);
		vnzInterruptRelease(interrupt);
	}
exit:
	for (i = 0; i < SERVERS + 1; i++)
		vnzThreadSetFlags(threads[i], FLAG_EXIT);
	if (doorbells)
		vnzFreeStack(doorbells, sizeof(Doorbells));
	base = vnzMemoryRegion(VK_MEMORY_WORK, &size);
	vnzHeapDelete(vnzHeapFind(base, size));
	base = vnzMemoryRegion(VK_MEMORY_HEAP, &size);
	vnzHeapDelete(vnzHeapFind(base, size));
	if (power) {
		vnzPowerCallbackUnregister(power);
		vnzPowerCallbackDelete(power);
	}
	vnzThreadSetFlags(parent, FLAG_EXIT);
}

/* A slot's server: run each call the slot's doorbell brings. */
static void serverThread(uint32_t slot, uint32_t argument2)
{
	Doorbells *doorbells = (Doorbells *)argument2;
	volatile RpcSlot *call = rpcSlot(slot);
	int32_t result = RPC_NOT_FOUND;
	uint32_t flags, command;
	RpcFunction function;

	command = 0;
	do {
		flags = vnzThreadWaitFlags(FLAG_EXIT | FLAG_DOORBELL | FLAG_REPLY);
		if ((flags & FLAG_REPLY) && (command & RPC_NOTIFY_STARTED))
			ringBack(slot, doorbells->values[slot] & 0xffff0000);
		if (flags & FLAG_DOORBELL) {
			call->status |= RPC_STARTED;
			command = call->command & ~RPC_FUNCTION;
			function = rpcFunction(call->command & RPC_FUNCTION);
			if (function)
				result = function(call->arguments[0], call->arguments[1],
					call->arguments[2], call->arguments[3]);
			call->result = result;
			call->status |= RPC_DONE;
			(void)call->result;
			if (command & RPC_NOTIFY_DONE)
				ringBack(slot, doorbells->values[slot] & 0xffff);
		}
	} while (!(flags & FLAG_EXIT));
}

static RpcFunction systemFunction(uint32_t id)
{
	if (id < 0x20)
		return g_systemFunctions[id];
	return 0;
}

/* The system slot's server. */
static void systemServerThread(uint32_t slot, uint32_t argument2)
{
	Doorbells *doorbells = (Doorbells *)argument2;
	volatile RpcSlot *call = rpcSlot(slot);
	int32_t result = RPC_NOT_FOUND;
	uint32_t flags, command;
	RpcFunction function;

	do {
		flags = vnzThreadWaitFlags(FLAG_EXIT | FLAG_DOORBELL);
		if (flags & FLAG_DOORBELL) {
			call->status |= RPC_STARTED;
			command = call->command & ~RPC_SYSTEM_FUNCTION;
			function = systemFunction(call->command & RPC_SYSTEM_FUNCTION);
			if (function)
				result = function(call->arguments[0], call->arguments[1],
					call->arguments[2], call->arguments[3]);
			call->result = result;
			call->status |= RPC_DONE;
			(void)call->result;
			if (command & RPC_NOTIFY_DONE)
				ringBack(slot, doorbells->values[slot] & 0xffff);
		}
	} while (!(flags & FLAG_EXIT));
}

/*
 * The performance counters (system functions 0-4): a cycle count and one
 * busy count per core, read into a buffer of seven pairs.
 */
#define COUNTERS_ENABLE		0x4fd540
#define COUNTERS_CYCLES		0x4fd548
#define COUNTERS_CONTROL	0x4fd568
#define COUNTERS_CORE(core)	(0x4fd580 + (core) * 8)
#define COUNTERS_RUN		0x1
#define COUNTERS_RESET		0x3
#define COUNTERS_SCALE		41

static volatile uint32_t *const g_coreCounters[VNZ_CORES] = {
	(volatile uint32_t *)COUNTERS_CORE(0), (volatile uint32_t *)COUNTERS_CORE(1),
	(volatile uint32_t *)COUNTERS_CORE(2), (volatile uint32_t *)COUNTERS_CORE(3),
	(volatile uint32_t *)COUNTERS_CORE(4), (volatile uint32_t *)COUNTERS_CORE(5),
	(volatile uint32_t *)COUNTERS_CORE(6), (volatile uint32_t *)COUNTERS_CORE(7),
};

static void countersWait(void)
{
	volatile uint32_t delay;

	while (*(volatile uint32_t *)COUNTERS_CONTROL & COUNTERS_RUN)
		for (delay = 0x1000; delay; delay--)
			;
}

static int32_t countersStart(void)
{
	*(volatile uint32_t *)COUNTERS_CONTROL = COUNTERS_RUN;
	return 0;
}

static int32_t countersStop(void)
{
	*(volatile uint32_t *)COUNTERS_CONTROL = 0;
	return 0;
}

static int32_t countersReset(void)
{
	volatile uint32_t delay;

	*(volatile uint32_t *)COUNTERS_CONTROL = COUNTERS_RESET;
	while (!(*(volatile uint32_t *)COUNTERS_CONTROL & COUNTERS_RUN))
		for (delay = 0x1000; delay; delay--)
			;
	return 0;
}

static int32_t countersRead(uint32_t buffer, uint32_t divisor)
{
	long *counts;
	uint32_t i;

	if (buffer == 0)
		return -1001;
	*(volatile uint32_t *)COUNTERS_CONTROL = 0;
	countersWait();
	counts = (long *)enterMemoryAccessProtocol(buffer, 0x40);
	if (counts == NULL)
		return -10001;
	if (divisor == COUNTERS_SCALE) {
		for (i = 0; i < 7; i++) {
			mep_saddu(&counts[i * 2], *g_coreCounters[i]);
			mep_saddu(&counts[i * 2 + 1], *(volatile uint32_t *)COUNTERS_CYCLES);
		}
	} else if (divisor != 0) {
		for (i = 0; i < 7; i++) {
			mep_saddu(&counts[i * 2], *g_coreCounters[i] / divisor * COUNTERS_SCALE);
			mep_saddu(&counts[i * 2 + 1],
				*(volatile uint32_t *)COUNTERS_CYCLES / divisor * COUNTERS_SCALE);
		}
	}
	if (leaveMemoryAccessProtocol((uint32_t)counts, 0x40) == 0)
		return -10001;
	return 0;
}

static int32_t countersEnable(void)
{
	*(volatile uint32_t *)COUNTERS_ENABLE = 0x1ff;
	return 0;
}

static const RpcFunction g_systemFunctions[0x20] = {
	(RpcFunction)countersStart,
	(RpcFunction)countersStop,
	(RpcFunction)countersReset,
	(RpcFunction)countersRead,
	(RpcFunction)countersEnable,
};

/*
 * The system's RPC functions (496-509): memory from the heaps for the ARM,
 * the VThread resources, the cores the VThreads run on.
 */
#define HEAP_SYSTEM	0
#define HEAP_KERNEL	1	/* VK_MEMORY_HEAP's */
#define HEAP_WORK	2	/* VK_MEMORY_WORK's */

static VkHeap *heapOf(uint32_t kind)
{
	uint32_t base, size;

	if (kind == HEAP_SYSTEM)
		return vnzSystemHeap();
	if (kind == HEAP_KERNEL)
		base = vnzMemoryRegion(VK_MEMORY_HEAP, &size);
	else if (kind == HEAP_WORK)
		base = vnzMemoryRegion(VK_MEMORY_WORK, &size);
	else
		return NULL;
	return vnzHeapFind(base, size);
}

/* memory for the ARM: neither cache holds any of it */
uint32_t rpcAllocate(uint32_t kind, uint32_t size)
{
	VkHeap *heap;
	uint32_t memory;

	if (kind > HEAP_WORK)
		return 0;
	heap = heapOf(kind);
	if (heap == NULL)
		return 0;
	size = (size + 0xff) & ~0xff;
	memory = (uint32_t)vnzHeapAllocate(heap, size, 0x100);
	vnzCachePurgeAllCores(memory, size);
	vnzL2Invalidate(memory, size);
	return memory;
}

int32_t rpcFree(uint32_t kind, uint32_t memory, uint32_t size)
{
	VkHeap *heap;

	if (kind > HEAP_WORK)
		return -1003;
	heap = heapOf(kind);
	if (heap == NULL)
		return 0;
	vnzHeapFree(heap, (void *)memory, (size + 0xff) & ~0xff, 0x100);
	return 0;
}

uint32_t rpcAllocatePublic(uint32_t kind, uint32_t size)
{
	VkHeap *heap;

	if (kind > HEAP_WORK)
		return 0;
	heap = heapOf(kind);
	if (heap == NULL)
		return 0;
	return (uint32_t)vnzHeapAllocatePublic(heap, (size + 0x3f) & ~0x3f);
}

int32_t rpcFreePublic(uint32_t kind, uint32_t memory, uint32_t size)
{
	VkHeap *heap;

	if (kind > HEAP_WORK)
		return -1003;
	heap = heapOf(kind);
	if (heap == NULL)
		return 0;
	vnzHeapFreePublic(heap, (void *)memory, (size + 0x3f) & ~0x3f);
	return 0;
}

int32_t rpcInvalidate(uint32_t memory, uint32_t size)
{
	vnzCachePurgeAllCores(memory, size);
	vnzL2Invalidate(memory, size);
	return 0;
}

uint32_t rpcOpenPublic(uint32_t memory, uint32_t size)
{
	uint32_t opened = enterMemoryAccessProtocol(memory, size);

	if (opened)
		opened = openPublicMemory(opened, size);
	return opened;
}

uint32_t rpcClosePublic(uint32_t memory, uint32_t size)
{
	uint32_t closed = closePublicMemory(memory, size);

	if (closed)
		closed = leaveMemoryAccessProtocol(closed, size);
	return closed;
}

/* a VThread resource, its handle kept in the ARM's word */
int32_t rpcVThreadResourceDelete(uint32_t holder);

int32_t rpcVThreadResourceCreate(uint32_t holder)
{
	VkVThreadResource **resource;

	if (holder == 0)
		return -1001;
	resource = (VkVThreadResource **)enterMemoryAccessProtocol(holder, 4);
	if (resource) {
		if (*resource == NULL) {
			*resource = vkVThreadResourceCreate(0x4000);
			if (*resource == NULL)
				goto fail;
		}
		leaveMemoryAccessProtocol((uint32_t)resource, 4);
		return 0;
	}
fail:
	rpcVThreadResourceDelete((uint32_t)resource);
	return -10001;
}

int32_t rpcVThreadResourceDelete(uint32_t holder)
{
	VkVThreadResource **resource;

	if (holder == 0)
		return -1001;
	resource = (VkVThreadResource **)enterMemoryAccessProtocol(holder, 4);
	if (resource == NULL)
		return -10001;
	if (*resource) {
		vkVThreadResourceDelete(*resource);
		*resource = NULL;
	}
	leaveMemoryAccessProtocol((uint32_t)resource, 4);
	return 0;
}

/* Run the VThreads on the first count cores (at most 7). */
int32_t rpcVThreadSetCores(uint32_t count)
{
	VkCoreEvent *event;
	uint32_t cores, i;

	event = vkVThreadCoreEventCreate(vnzThreadCurrent(), 1);
	if (event == NULL)
		return 0;
	vkVThreadCoreEventRegister(event);
	cores = 0;
	for (i = 0; i < (count < 7 ? count : 7); i++)
		cores |= 1 << i;
	if (vkVThreadSetCores(vkVThreadCores(), cores) != 0)
		return 0;
	vnzThreadWaitFlags(1);
	if (event) {
		vkVThreadCoreEventUnregister(event);
		vkVThreadCoreEventDelete(event);
	}
	if (count)
		setCoreCount(count);
	return 0;
}

/* Power the cores down: no VThread cores, then none at all. */
int32_t rpcPowerDown(void)
{
	rpcVThreadSetCores(0);
	if (vnzPowerSetCores(vnzPowerActiveCores(), 0) == 0) {
		rpcVThreadSetCores(getCoreCount());
		setState(STATE_SUSPENDED);
	}
	return 0;
}

int32_t rpcNothing(void)
{
	return 0;
}

/* the application: its threads run until the interrupt thread exits */
static void applicationMain(void)
{
	int32_t *threads;

	threads = vnzAllocateStack(THREADS * sizeof(int32_t));
	if (threads == NULL)
		return;
	if (createThreads(threads) >= 0) {
		vnzThreadStart((VkThread *)threads[INTERRUPT_THREAD], (uint32_t)threads,
			(uint32_t)vnzThreadCurrent());
		vnzThreadWaitFlags(FLAG_DOORBELL);
		setState(STATE_READY);
		vnzThreadWaitFlags(FLAG_EXIT);
	}
	if (threads) {
		deleteThreads(threads);
		vnzFreeStack(threads, THREADS * sizeof(int32_t));
	}
}

void applicationStart(void)
{
	applicationMain();
}

const VkModule g_vkApplicationModule VK_SECTION(".application.module") = {
	.magic = VK_MODULE_APPLICATION,
	.self = VNZ_PHYSICAL_OF(0x811000),
	.size = 0x115b14,
	.entry = applicationStart,
	.attributes = VK_MODULE_ATTRIBUTES,
	.version = VK_MODULE_VERSION,
};
