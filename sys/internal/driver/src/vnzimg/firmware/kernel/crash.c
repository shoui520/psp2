/*
 * Exceptions, and the record of a crash.
 *
 * An exception (kernel/vectors.S) saves the core's registers and stops the
 * core here: the crash hook runs, the core's state and the kernel's are written
 * to the crash region (VK_MEMORY_CRASH) for the ARM side, the registered
 * copies are made, and the core sleeps for good.
 */
#include "kernel.h"

#define VK_CRASH_MAGIC		0xdead0001
#define VK_CRASH_VERSION	1
#define VK_CRASH_SIZE		0xd58

/* the registers an exception saves (kernel/vectors.S) */
typedef struct VkExceptionFrame {
	uint32_t registers[16];
	uint32_t lp;
} VkExceptionFrame;

/* a core's state in the record (0xe0 bytes) */
typedef struct VkCrashCore {
	uint32_t registers[16];
	uint32_t control[32];		/* the control registers, by number */
	uint32_t interruptController[8];
} VkCrashCore;

typedef struct VkCrashHardware {
	uint32_t l2Control;
	uint32_t reserved4;
	uint32_t dmaFlush;
	uint32_t reservedC;
	uint32_t l2Registers[5];	/* 0x4f0008-0x4f0018 */
	uint32_t interconnect[2];	/* 0x4f3000, 0x4f3004 */
	uint32_t reserved2c;
	struct {
		uint32_t pending;
		uint32_t mask;
	} doorbells[2][VNZ_CORES];
} VkCrashHardware;

typedef struct VkCrashKernelCore {
	VkThread *current;
	VkThread *idle;
	uint8_t reserved8;
	uint8_t reschedulePending;
	uint16_t reservedA;
	uint32_t inKernel;
	VkInterruptHandler *eventsHead;
	VkInterruptHandler *eventsTail;
	uint32_t dispatchDisable;
	uint32_t type;
	uint32_t reserved20[8];
} VkCrashKernelCore;

typedef struct VkCrashKernel {
	VkCrashKernelCore cores[VNZ_CORES];
	void *handlers[VNZ_CORES][8];	/* the handlers of the interrupts a core records */
	void *lockPagesHead;
	void *lockPagesTail;
	VkQueue runQueues[VNZ_CORES][VK_PRIORITIES];
	uint32_t threadCounts[VNZ_CORES];
	VkThread *nextThreads[VNZ_CORES];
} VkCrashKernel;

/* the crash region's contents */
typedef struct VkCrashRecord {
	uint32_t magic;
	uint32_t version;
	uint32_t cores;
	uint32_t coreId;		/* that crashed */
	uint32_t exception;
	uint32_t reserved14[3];
	VkCrashCore core[VNZ_CORES];
	VkCrashHardware hardware;
	VkCrashKernel kernel;
} VkCrashRecord;

/* a copy made when the kernel crashes (vkCrashCopyRegister) */
typedef struct VkCrashCopy {
	struct VkCrashCopy *next;
	const void *source;
	uint32_t size;
} VkCrashCopy;

static const uint32_t doorbellBanks[2] = { VNZ_DOORBELLS, VNZ_DOORBELLS2 };

volatile uint8_t *g_vkCrashLock VK_SHARED;
VkCrashHook g_vkCrashHook VK_SHARED;
uint32_t g_vkCrashHookArgument VK_SHARED;
uint32_t g_vkCrashHookArgument2 VK_SHARED;
VkCrashCopy *g_vkCrashCopies VK_SHARED;
volatile uint8_t *g_vkCrashCopiesLock VK_SHARED;
const uint32_t *g_vkConfigurationReserved VK_SHARED;	/* recorded, never read */

void vkCrashInitialize(uint32_t primary)
{
	volatile uint8_t *lock;

	if (!primary)
		return;
	lock = vkLockPageAllocate();
	if (lock == NULL)
		return;
	g_vkCrashLock = lock;
	g_vkCrashHook = NULL;
	g_vkExitedThreads = 0;
	vkGiveSemaphore(vkSemaphore(VK_SEMAPHORE_EXITS));
	g_vkExitedLimit = VK_CONFIGURATION->maxExited;
	g_vkConfigurationReserved = &VK_CONFIGURATION->reserved8;
	g_vkCrashCopies = NULL;
	g_vkCrashCopiesLock = vkLockPageAllocate();
}

static void recordHeader(VkCrashRecord *record, uint32_t exception)
{
	record->version = VK_CRASH_VERSION;
	record->cores = VNZ_CORES;
	record->magic = VK_CRASH_MAGIC;
	record->exception = exception;
	record->coreId = vkCurrentCoreId();
	record->reserved14[2] = 0;
	record->reserved14[1] = 0;
	record->reserved14[0] = 0;
}

static void recordRegisters(uint32_t *to, const uint32_t *from)
{
	uint32_t i;

	for (i = 0; i < 16; i++)
		to[i] = from[i];
}

static void recordControl(uint32_t *control, const VkExceptionFrame *frame)
{
	control[VNZ_CR_PC] = vnzReadControl(VNZ_CR_PC);
	control[VNZ_CR_LP] = frame->lp;
	control[VNZ_CR_SAR] = vnzReadControl(VNZ_CR_SAR);
	control[3] = 0;
	control[VNZ_CR_RPB] = vnzReadControl(VNZ_CR_RPB);
	control[VNZ_CR_RPE] = vnzReadControl(VNZ_CR_RPE);
	control[VNZ_CR_RPC] = vnzReadControl(VNZ_CR_RPC);
	control[VNZ_CR_HI] = vnzReadControl(VNZ_CR_HI);
	control[VNZ_CR_LO] = vnzReadControl(VNZ_CR_LO);
	control[VNZ_CR_MB0] = vnzReadControl(VNZ_CR_MB0);
	control[11] = 0;
	control[VNZ_CR_ME0] = vnzReadControl(VNZ_CR_ME0);
	control[10] = 0;
	control[VNZ_CR_MB1] = vnzReadControl(VNZ_CR_MB1);
	control[9] = 0;
	control[VNZ_CR_ME1] = vnzReadControl(VNZ_CR_ME1);
	control[VNZ_CR_PSW] = vnzReadControl(VNZ_CR_PSW);
	control[VNZ_CR_ID] = vnzReadControl(VNZ_CR_ID);
	control[VNZ_CR_TMP] = vnzReadControl(VNZ_CR_TMP);
	control[VNZ_CR_EPC] = vnzReadControl(VNZ_CR_EPC);
	control[VNZ_CR_EXC] = vnzReadControl(VNZ_CR_EXC);
	control[22] = 0;
	control[VNZ_CR_CFG] = vnzReadControl(VNZ_CR_CFG);
	control[VNZ_CR_NPC] = vnzReadControl(VNZ_CR_NPC);
	control[VNZ_CR_DBG] = vnzReadControl(VNZ_CR_DBG);
	control[VNZ_CR_DEPC] = vnzReadControl(VNZ_CR_DEPC);
	control[VNZ_CR_OPT] = vnzReadControl(VNZ_CR_OPT);
	control[VNZ_CR_RCFG] = vnzReadControl(VNZ_CR_RCFG);
	control[29] = 0;
	control[VNZ_CR_CCFG] = vnzReadControl(VNZ_CR_CCFG);
	control[VNZ_CR_RES30] = vnzReadControl(VNZ_CR_RES30);
	control[VNZ_CR_RES31] = vnzReadControl(VNZ_CR_RES31);
}

static void recordInterruptController(uint32_t *to)
{
	to[0] = vnzReadControlBus(VNZ_CB_INTERRUPT_STATUS);
	to[1] = vnzReadControlBus(VNZ_CB_INTERRUPT_CLEAR);
	to[2] = vnzReadControlBus(VNZ_CB_INTERRUPT_ENABLE);
	to[3] = vnzReadControlBus(VNZ_CB_INTERRUPT_EDGE);
	to[4] = vnzReadControlBus(VNZ_CB_INTERRUPT_LEVEL0);
	to[5] = vnzReadControlBus(VNZ_CB_INTERRUPT_LEVEL1);
	to[6] = vnzReadControlBus(VNZ_CB_INTERRUPT_LEVEL2);
	to[7] = vnzReadControlBus(VNZ_CB_INTERRUPT_LEVEL3);
}

static void recordHardware(VkCrashHardware *hardware)
{
	uint32_t banks[2] = { doorbellBanks[0], doorbellBanks[1] };
	uint32_t bank, core;

	hardware->l2Control = vnzRegister(VNZ_L2_CONTROL);
	hardware->dmaFlush = vnzRegister(VNZ_DMA_FLUSH);
	hardware->l2Registers[0] = vnzRegister(VNZ_L2_CONTROL + 0x08);
	hardware->l2Registers[1] = vnzRegister(VNZ_L2_CONTROL + 0x0c);
	hardware->l2Registers[2] = vnzRegister(VNZ_L2_CONTROL + 0x10);
	hardware->l2Registers[3] = vnzRegister(VNZ_L2_CONTROL + 0x14);
	hardware->l2Registers[4] = vnzRegister(VNZ_L2_CONTROL + 0x18);
	hardware->interconnect[0] = vnzRegister(0x4f3000);
	hardware->interconnect[1] = vnzRegister(0x4f3004);
	for (bank = 0; bank < 2; bank++) {
		for (core = 0; core < VNZ_CORES; core++) {
			hardware->doorbells[bank][core].mask = vnzRegister(banks[bank] + core * 0x20 + 8);
			hardware->doorbells[bank][core].pending = vnzRegister(banks[bank] + core * 0x20);
		}
	}
}

/* this core's state in the record */
static void recordCore(uint32_t index, VkCrashRecord *record, const VkExceptionFrame *frame)
{
	VkCrashCore *core = &record->core[index];

	recordRegisters(core->registers, frame->registers);
	recordControl(core->control, frame);
	recordInterruptController(core->interruptController);
	recordHardware(&record->hardware);
}

/* the handlers of the interrupts the core's configuration says to record */
static void recordHandlers(uint32_t index, VkCrashKernel *kernel)
{
	uint32_t interrupts = VK_CONFIGURATION->coreTable->cores[index].interrupts;
	VkInterruptHandler *handler;
	uint32_t bit = 1, recorded = 0, i;

	for (i = 0; i < VNZ_INTERRUPTS && recorded < 8; i++, bit <<= 1) {
		if (!(interrupts & bit))
			continue;
		handler = g_vkInterruptTables[index].handlers[i];
		kernel->handlers[index][recorded++] = handler ? (void *)handler->handler : NULL;
	}
}

static void recordKernelCore(const VkCore *core, VkCrashKernelCore *to)
{
	to->current = core->current;
	to->idle = core->idle;
	to->dispatchDisable = core->dispatchDisable;
	to->reschedulePending = core->reschedulePending;
	to->inKernel = core->inKernel;
	to->eventsHead = core->eventsHead;
	to->eventsTail = core->eventsTail;
	to->type = core->type;
}

static void recordKernel(uint32_t index, VkCrashKernel *kernel)
{
	uint32_t core, priority;

	recordKernelCore(&g_vkCores[index], &kernel->cores[index]);
	recordHandlers(index, kernel);
	kernel->lockPagesHead = g_vkLockPages.head;
	kernel->lockPagesTail = g_vkLockPages.tail;
	for (core = 0; core < VNZ_CORES; core++)
		for (priority = 0; priority < VK_PRIORITIES; priority++)
			kernel->runQueues[core][priority] = g_vkRunQueues[core][priority];
	for (core = 0; core < VNZ_CORES; core++)
		kernel->threadCounts[core] = g_vkThreadCounts[core];
	for (core = 0; core < VNZ_CORES; core++)
		kernel->nextThreads[core] = g_vkNextThreads[core];
}

static void recordCrash(uint32_t index, const VkExceptionFrame *frame, uint32_t exception)
{
	VkCrashRecord *record;
	uint32_t size;
	uint32_t base = vkMemoryRegion(VK_MEMORY_CRASH, &size);

	if (base == 0 || size < VK_CRASH_SIZE)
		return;
	record = (VkCrashRecord *)vkToUncached(base);
	if (record == NULL)
		return;
	recordHeader(record, exception);
	recordCore(index, record, frame);
	recordKernel(index, &record->kernel);
}

static void copyWords(const uint32_t *from, uint32_t size, uint32_t *to)
{
	for (size >>= 2; size; size--)
		*to++ = *from++;
}

static void makeCopies(void)
{
	VkCrashCopy *copy, *next;

	vnzTestAndSet(g_vkCrashCopiesLock);
	for (copy = g_vkCrashCopies; copy; copy = next) {
		next = copy->next;
		copyWords(copy->source, copy->size, (uint32_t *)copy);
	}
}

/* An exception (from kernel/vectors.S): the core stops here. */
void vkCrash(uint32_t index, VkExceptionFrame *frame, uint32_t exception)
{
	vnzTestAndSet(g_vkCrashLock);
	if (g_vkCrashHook)
		g_vkCrashHook(index + VNZ_FIRST_CORE_ID, exception, g_vkCrashHookArgument,
			g_vkCrashHookArgument2);
	recordCrash(index, frame, exception);
	makeCopies();
	for (;;)
		mep_sleep();
}

/* Set the function a crash calls first. Returns 0, or -1 when one is set. */
int32_t vkCrashSetHook(VkCrashHook hook, uint32_t argument, uint32_t argument2)
{
	int32_t result = -1;

	vkDisableDispatch();
	vkSpinLock(g_vkCrashLock);
	if (g_vkCrashHook == NULL) {
		result = 0;
		g_vkCrashHookArgument2 = argument2;
		g_vkCrashHookArgument = argument;
		g_vkCrashHook = hook;
	}
	vkSpinUnlock(g_vkCrashLock);
	vkEnableDispatch();
	return result;
}

int32_t vkCrashClearHook(void)
{
	int32_t result = -1;

	vkDisableDispatch();
	vkSpinLock(g_vkCrashLock);
	if (g_vkCrashHook) {
		result = 0;
		g_vkCrashHook = NULL;
	}
	vkSpinUnlock(g_vkCrashLock);
	vkEnableDispatch();
	return result;
}

/*
 * Have a crash copy size bytes from source into the memory region id (which
 * holds the copy's header first). Returns 0, or -1 when the region is too small.
 */
int32_t vkCrashCopyRegister(uint32_t id, const void *source, uint32_t size)
{
	VkCrashCopy *copy;
	uint32_t base, regionSize;

	base = vkMemoryRegion(id, &regionSize);
	if (base == 0 || regionSize < size || regionSize < 0x10)
		return -1;
	copy = (VkCrashCopy *)vkPhysicalToUncached(base);
	copy->source = source;
	copy->size = size;
	vkDisableDispatch();
	vkSpinLock(g_vkCrashCopiesLock);
	copy->next = g_vkCrashCopies;
	g_vkCrashCopies = copy;
	vkSpinUnlock(g_vkCrashCopiesLock);
	vkEnableDispatch();
	return 0;
}
