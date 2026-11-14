/*
 * Power: which cores are up.
 *
 * vnzKernelPowerSetCores asks for a set of cores. Each core has a power thread; a
 * core leaving the set is told by it and goes down (vnzKernelPowerSuspendCore, in
 * kernel/vectors.S), sleeping until a doorbell finds it back in the set. A
 * core coming back resumes the modules and is counted up again. When the
 * active set reaches the target, the threads waiting on the notify list are
 * told and the callbacks are called.
 *
 * When the last core goes down, the local RAM region is saved to the heap and
 * the L2 written back; the first core up restores it.
 */
#include "kernel.h"

#define VNZ_KERNEL_POWER_LOCAL_NONE	1	/* the local RAM save, by state */
#define VNZ_KERNEL_POWER_LOCAL_SAVED	2
#define VNZ_KERNEL_POWER_LOCAL_RESTORED	3
#define VNZ_KERNEL_POWER_LOCAL_FREED	4

#define VNZ_KERNEL_POWER_REGISTER	vnzRegister(0x4fd568)	/* kept over a power down */

/* a core's state over a power down (0x20 bytes) */
typedef struct VnzKernelPowerCore {
	uint32_t controller[6];		/* control bus registers 2 to 7 */
	uint32_t resumePrimary;
	uint32_t resumeFirstStart;
} VnzKernelPowerCore;

typedef struct VnzKernelPower {
	uint32_t active;		/* a bit per core up */
	uint32_t expected;		/* what the caller of vnzKernelPowerSetCores saw */
	uint32_t target;
	uint32_t changing;
	VnzKernelThread *threads[VNZ_CORES];
	VnzKernelPowerCore cores[VNZ_CORES];
	VnzKernelPowerList notify;
	VnzKernelPowerList callbacks;
	uint32_t savedRegister;
	void *localSave;		/* public */
	uint32_t localSaveSize;
	uint32_t localState;
} VnzKernelPower;

VnzKernelPower g_vnzKernelPower VNZ_KERNEL_SHARED;

static volatile uint32_t *const powerSemaphore = vnzKernelSemaphore(VNZ_KERNEL_SEMAPHORE_POWER);

static void localSave(void);
static void powerNotify(void);
static void powerCallbacks(uint32_t cores);

static uint32_t currentCore(void)
{
	return vnzKernelCurrentCoreId() - VNZ_FIRST_CORE_ID;
}

/* firstBoot: the image's first start (not a resume of every core) */
void vnzKernelPowerInitialize(uint32_t firstBoot, uint32_t primary)
{
	VnzKernelPower *power = &g_vnzKernelPower;
	int i;

	if (firstBoot) {
		if (!primary)
			return;
		vnzKernelGiveSemaphore(powerSemaphore);
		power->expected = 0;
		power->active = 0;
		power->notify.head = NULL;
		power->notify.tail = NULL;
		power->callbacks.head = NULL;
		power->callbacks.tail = NULL;
		power->changing = 1;
		power->target = VNZ_KERNEL_CONFIGURATION->cores;
		for (i = 0; i < VNZ_CORES; i++)
			power->threads[i] = NULL;
		power->localSave = NULL;
		power->localSaveSize = 0;
		power->localState = 0;
		return;
	}
	vnzKernelTakeSemaphore(powerSemaphore);
	if (power->changing == 0 && power->active == 0) {
		power->expected = 0;
		power->changing = 1;
		power->target = VNZ_KERNEL_CONFIGURATION->cores;
	}
	vnzKernelGiveSemaphore(powerSemaphore);
}

/* a core's power thread: down when told, then up again */
static void powerThread(void)
{
	uint32_t index;
	VnzKernelCore *core;
	VnzKernelThread *thread;

	vnzKernelDisableDispatch();
	core = vnzKernelCurrentCore(&index);
	thread = vnzKernelThreadCurrent();
	vnzWriteControl(VNZ_CR_PSW, vnzReadControl(VNZ_CR_PSW) & ~VNZ_PSW_IEP);
	vnzKernelPowerCoreUp();
	for (;;) {
		vnzWriteControl(VNZ_CR_PSW, vnzReadControl(VNZ_CR_PSW) | VNZ_PSW_IEP);
		vnzKernelThreadWaitFlags(1);
		vnzWriteControl(VNZ_CR_PSW, vnzReadControl(VNZ_CR_PSW) & ~VNZ_PSW_IEP);
		vnzKernelPowerSuspendCore(thread, core);
		vnzKernelPowerResume();
	}
}

/* At a core's start: its power thread, or (coming back up) a word to it. */
void vnzKernelPowerStart(uint32_t primary, uint32_t firstStart)
{
	uint32_t coreId = vnzKernelCurrentCoreId(), core = coreId - VNZ_FIRST_CORE_ID;
	VnzKernelThread *thread;

	g_vnzKernelPower.cores[core].resumeFirstStart = firstStart;
	g_vnzKernelPower.cores[core].resumePrimary = primary;
	if (firstStart) {
		thread = (VnzKernelThread *)vnzKernelThreadCreateHere(powerThread, coreId, 9,
			0x200);
		if (thread == (VnzKernelThread *)-1)
			return;
		g_vnzKernelPower.threads[core] = thread;
		vnzKernelThreadStart(thread, 0, 0);
		vnzKernelThreadResume(thread);
		vnzKernelModulesResume();
		return;
	}
	thread = g_vnzKernelPower.threads[core];
	vnzKernelThreadSetFlags(thread, 1);
	vnzKernelThreadResume(thread);
}

void vnzKernelPowerSaveInterrupts(void)
{
	VnzKernelPowerCore *core = &g_vnzKernelPower.cores[currentCore()];
	uint32_t psw = vnzReadControl(VNZ_CR_PSW);

	vnzDisableInterrupts();
	core->controller[0] = vnzReadControlBus(2);
	core->controller[1] = vnzReadControlBus(3);
	core->controller[2] = vnzReadControlBus(4);
	core->controller[3] = vnzReadControlBus(5);
	core->controller[4] = vnzReadControlBus(6);
	core->controller[5] = vnzReadControlBus(7);
	vnzWriteControl(VNZ_CR_PSW, psw);
}

void vnzKernelPowerRestoreInterrupts(void)
{
	VnzKernelPowerCore *core = &g_vnzKernelPower.cores[currentCore()];
	uint32_t psw = vnzReadControl(VNZ_CR_PSW);

	vnzDisableInterrupts();
	vnzWriteControlBus(2, core->controller[0]);
	vnzWriteControlBus(3, core->controller[1]);
	vnzWriteControlBus(4, core->controller[2]);
	vnzWriteControlBus(5, core->controller[3]);
	vnzWriteControlBus(6, core->controller[4]);
	vnzWriteControlBus(7, core->controller[5]);
	vnzWriteControl(VNZ_CR_PSW, psw);
}

/* This core is going down. */
void vnzKernelPowerCoreDown(void)
{
	uint32_t bit = 1 << currentCore(), active;

	vnzKernelTakeSemaphore(powerSemaphore);
	active = g_vnzKernelPower.active & ~bit;
	g_vnzKernelPower.active = active;
	if (active == g_vnzKernelPower.target) {
		g_vnzKernelPower.changing = 0;
		powerNotify();
		if (active == 0) {
			/* the last one */
			vnzKernelGiveSemaphore(powerSemaphore);
			localSave();
			vnzKernelL2OperationAll(VNZ_KERNEL_L2_WRITEBACK);
		}
		powerCallbacks(active);
	}
	if (active != 0)
		vnzKernelGiveSemaphore(powerSemaphore);
}

/* Sleep until this core is in the target again. */
void vnzKernelPowerWaitUp(void)
{
	uint32_t core = currentCore(), bit = 1 << core;

	while (!(g_vnzKernelPower.target & bit)) {
		mep_halt();
		vnzRegister(VNZ_DOORBELLS + core * 0x20) = 0;
	}
}

/*
 * At boot, a core outside the cores to start waits until another core has
 * started (its barrier word set), then sleeps until it is wanted.
 */
void vnzKernelPowerBootWait(uint32_t primary, uint32_t firstStart, uint32_t starting, uint32_t core)
{
	uint32_t bit = 1 << core, psw, enabled;
	volatile int i;
	int first = -1, n;

	(void)primary;
	if (firstStart) {
		if (starting & bit)
			return;
		for (n = 0; n < VNZ_CORES; n++) {
			if (g_vnzKernelBootStarted[n]) {
				first = n;
				break;
			}
		}
		while (g_vnzKernelBootReady[first] == 0)
			for (i = 0; i < 100; i++)
				;
	}
	psw = vnzReadControl(VNZ_CR_PSW);
	enabled = vnzReadControlBus(VNZ_CB_INTERRUPT_ENABLE);
	vnzDisableInterrupts();
	vnzKernelDoorbellInitialize();
	vnzKernelInterruptSetEdge(VNZ_INTERRUPT_DOORBELL, 0);
	vnzKernelInterruptSetLevel(VNZ_INTERRUPT_DOORBELL, 10);
	vnzKernelInterruptSetEnable(VNZ_INTERRUPT_DOORBELL, 1);
	while (!(g_vnzKernelPower.target & bit)) {
		mep_sleep();
		vnzRegister(VNZ_DOORBELLS + core * 0x20) = 0;
	}
	vnzWriteControlBus(VNZ_CB_INTERRUPT_ENABLE, enabled);
	vnzWriteControl(VNZ_CR_PSW, psw);
}

/* This core is up. */
void vnzKernelPowerCoreUp(void)
{
	uint32_t bit = 1 << currentCore(), active;

	vnzKernelTakeSemaphore(powerSemaphore);
	active = g_vnzKernelPower.active | bit;
	g_vnzKernelPower.active = active;
	if (active == g_vnzKernelPower.target) {
		g_vnzKernelPower.changing = 0;
		powerNotify();
		powerCallbacks(active);
	}
	vnzKernelGiveSemaphore(powerSemaphore);
}

/* back from a power down (kernel/vectors.S) */
void vnzKernelPowerResume(void)
{
	vnzKernelPowerRestoreInterrupts();
	vnzKernelModulesResume();
	vnzKernelPowerCoreUp();
}

/*
 * The local RAM region over a power down.
 */

/* a buffer for it; returns 0 only when there is no memory for one */
static uint32_t localSaveAllocate(void)
{
	uint32_t base, size = 0, result = 1;
	void *buffer = NULL;

	base = vnzKernelMemoryRegion(VNZ_KERNEL_MEMORY_LOCAL_RAM, &size);
	if (base == 0 || base < VNZ_LOCAL_RAM || base >= VNZ_LOCAL_RAM + 0x2000)
		size = 0;
	if (size) {
		buffer = vnzKernelHeapAllocate(vnzKernelSystemHeap(), size, 0x40);
		if (buffer == NULL) {
			size = 0;
			result = 0;
		} else {
			buffer = (void *)vnzKernelPhysicalToPublic((uint32_t)buffer);
			result = (uint32_t)buffer;
		}
	}
	g_vnzKernelPower.localState = VNZ_KERNEL_POWER_LOCAL_NONE;
	g_vnzKernelPower.localSave = buffer;
	g_vnzKernelPower.localSaveSize = size;
	return result;
}

static void copyWords(uint32_t *destination, const uint32_t *source, uint32_t size)
{
	uint32_t n;

	for (n = 0; n < size; n += 4)
		*destination++ = *source++;
}

static void localSave(void)
{
	uint32_t *buffer, size;

	g_vnzKernelPower.savedRegister = VNZ_KERNEL_POWER_REGISTER;
	if (g_vnzKernelPower.localSave) {
		buffer = (uint32_t *)vnzKernelPublicToPhysical((uint32_t)g_vnzKernelPower.localSave);
		size = g_vnzKernelPower.localSaveSize;
		copyWords(buffer,
			(const uint32_t *)vnzKernelMemoryRegion(VNZ_KERNEL_MEMORY_LOCAL_RAM, NULL),
			size);
		vnzKernelCacheWriteback((uint32_t)buffer, (size + 63) & ~63);
	}
	g_vnzKernelPower.localState = VNZ_KERNEL_POWER_LOCAL_SAVED;
}

/* The first core up puts local RAM back; the others wait for it. */
void vnzKernelPowerLocalRestore(void)
{
	uint32_t *buffer, size;
	volatile int i;

	if (vnzKernelTrySemaphore(powerSemaphore) != 0) {
		while (g_vnzKernelPower.localState < VNZ_KERNEL_POWER_LOCAL_RESTORED)
			for (i = 0; i < 50; i++)
				;
		return;
	}
	if (g_vnzKernelPower.localState < VNZ_KERNEL_POWER_LOCAL_RESTORED) {
		VNZ_KERNEL_POWER_REGISTER = g_vnzKernelPower.savedRegister;
		if (g_vnzKernelPower.localSave) {
			buffer = (uint32_t *)vnzKernelPublicToPhysical((uint32_t)g_vnzKernelPower.localSave);
			size = g_vnzKernelPower.localSaveSize;
			copyWords((uint32_t *)vnzKernelMemoryRegion(VNZ_KERNEL_MEMORY_LOCAL_RAM,
				NULL), buffer, size);
			vnzKernelCachePurge((uint32_t)buffer, (size + 63) & ~63);
		}
		vnzKernelLocalCodeCopy(1);
		g_vnzKernelPower.localState = VNZ_KERNEL_POWER_LOCAL_RESTORED;
	}
	vnzKernelGiveSemaphore(powerSemaphore);
}

/* Free the buffer once the cores are back. */
void vnzKernelPowerLocalFree(void)
{
	volatile int i;

	if (vnzKernelTrySemaphore(powerSemaphore) != 0) {
		while (g_vnzKernelPower.localState < VNZ_KERNEL_POWER_LOCAL_FREED)
			for (i = 0; i < 50; i++)
				;
		return;
	}
	if (g_vnzKernelPower.localState < VNZ_KERNEL_POWER_LOCAL_FREED) {
		if (g_vnzKernelPower.localSave)
			vnzKernelHeapFree(vnzKernelSystemHeap(),
				(void *)vnzKernelPublicToPhysical((uint32_t)g_vnzKernelPower.localSave),
				g_vnzKernelPower.localSaveSize, 0x40);
		g_vnzKernelPower.localState = VNZ_KERNEL_POWER_LOCAL_FREED;
	}
	vnzKernelGiveSemaphore(powerSemaphore);
}

/*
 * Ask for the cores in cores (a bit each), when expected is the set up now.
 * Returns 0, or the set up now when it is not expected (or a change is
 * under way).
 */
uint32_t vnzKernelPowerSetCores(uint32_t expected, uint32_t cores)
{
	VnzKernelPower *power = &g_vnzKernelPower;
	uint32_t active, result, core, bit;
	VnzKernelThread *thread;

	vnzKernelDisableDispatch();
	vnzKernelTakeSemaphore(powerSemaphore);
	active = power->active;
	if (active == expected && power->changing == 0 && (cores != 0 || localSaveAllocate() != 0)) {
		result = 0;
	} else {
		result = active;
		if (active != 0)
			goto out;
	}
	power->changing = 1;
	if (expected == cores) {
		power->changing = 0;
		powerNotify();
		powerCallbacks(cores);
		goto out;
	}
	power->expected = expected;
	power->target = cores;
	for (core = 0; core < VNZ_CORES; core++) {
		bit = 1 << core;
		if ((active & bit) == (cores & bit))
			continue;
		thread = power->threads[core];
		if (thread) {
			vnzKernelThreadSetFlags(thread, 1);
			vnzKernelThreadResume(thread);
		} else {
			/* not started yet: ring it */
			vnzRegister(VNZ_DOORBELLS + 0x10 + vnzCoreIndex() * 0x20) = bit;
		}
	}
out:
	vnzKernelGiveSemaphore(powerSemaphore);
	vnzKernelEnableDispatch();
	return result;
}

uint32_t vnzKernelPowerActiveCores(void)
{
	return g_vnzKernelPower.active;
}

/*
 * The notify list (threads sent flags) and the callbacks (called with the
 * new set), when the active set reaches the target.
 */
static VnzKernelPowerHook *hookCreate(VnzKernelThread *thread, uint32_t flags,
	void (*function)(uint32_t cores, uint32_t argument), uint32_t argument)
{
	VnzKernelPowerHook *hook = vnzKernelHeapAllocate(vnzKernelCurrentHeap(),
		sizeof(VnzKernelPowerHook), 0x40);

	if (hook == NULL)
		return NULL;
	hook->thread = thread;
	hook->flags = flags;
	hook->function = function;
	hook->argument = argument;
	hook->next = NULL;
	hook->previous = NULL;
	hook->registered = 0;
	hook->magic = VNZ_KERNEL_HOOK_MAGIC;
	vnzKernelCacheWriteback((uint32_t)hook, 0x40);
	return (VnzKernelPowerHook *)vnzKernelPhysicalToPublic((uint32_t)hook);
}

VnzKernelPowerHook *vnzKernelPowerNotifyCreate(VnzKernelThread *thread, uint32_t flags)
{
	return hookCreate(thread, flags, NULL, 0);
}

VnzKernelPowerHook *vnzKernelPowerCallbackCreate(void (*function)(uint32_t cores,
	uint32_t argument),
	uint32_t argument)
{
	return hookCreate(NULL, 0, function, argument);
}

/*
 * A hook list is linked like a run queue (next and previous first, then head and
 * tail), and Sony's kernel links it with the run queues' functions.
 */
static void hookRegister(VnzKernelPowerList *list, VnzKernelPowerHook *hook)
{
	vnzKernelDisableDispatch();
	vnzKernelTakeSemaphore(powerSemaphore);
	if (!hook->registered) {
		vnzKernelQueueAppend((VnzKernelQueue *)list, (VnzKernelThread *)hook);
		hook->registered = 1;
	}
	vnzKernelGiveSemaphore(powerSemaphore);
	vnzKernelEnableDispatch();
}

static void hookUnregister(VnzKernelPowerList *list, VnzKernelPowerHook *hook)
{
	vnzKernelDisableDispatch();
	vnzKernelTakeSemaphore(powerSemaphore);
	if (hook->registered == 1) {
		vnzKernelQueueRemove((VnzKernelQueue *)list, (VnzKernelThread *)hook);
		hook->registered = 0;
	}
	vnzKernelGiveSemaphore(powerSemaphore);
	vnzKernelEnableDispatch();
}

void vnzKernelPowerNotifyRegister(VnzKernelPowerHook *hook)
{
	hookRegister(&g_vnzKernelPower.notify, hook);
}

void vnzKernelPowerCallbackRegister(VnzKernelPowerHook *hook)
{
	hookRegister(&g_vnzKernelPower.callbacks, hook);
}

void vnzKernelPowerNotifyUnregister(VnzKernelPowerHook *hook)
{
	hookUnregister(&g_vnzKernelPower.notify, hook);
}

void vnzKernelPowerCallbackUnregister(VnzKernelPowerHook *hook)
{
	hookUnregister(&g_vnzKernelPower.callbacks, hook);
}

static void hookFree(VnzKernelPowerHook *hook)
{
	hook->magic = 0;
	vnzKernelHeapFree(vnzKernelCurrentHeap(), (void *)vnzKernelPublicToPhysical((uint32_t)hook),
		sizeof(VnzKernelPowerHook), 0x40);
}

void vnzKernelPowerNotifyDelete(VnzKernelPowerHook *hook)
{
	hookUnregister(&g_vnzKernelPower.notify, hook);
	hookFree(hook);
}

void vnzKernelPowerCallbackDelete(VnzKernelPowerHook *hook)
{
	hookUnregister(&g_vnzKernelPower.callbacks, hook);
	hookFree(hook);
}

static void powerNotify(void)
{
	VnzKernelPowerHook *hook;

	for (hook = g_vnzKernelPower.notify.head; hook; hook = hook->next)
		vnzKernelThreadSetFlags(hook->thread, hook->flags);
}

static void powerCallbacks(uint32_t cores)
{
	VnzKernelPowerHook *hook;

	for (hook = g_vnzKernelPower.callbacks.head; hook; hook = hook->next)
		hook->function(cores, hook->argument);
}

/*
 * The modules: resumed in order at a core's start, suspended in reverse
 * before it goes down.
 */
void vnzKernelModulesResume(void)
{
	const VnzKernelModuleTable *table = VNZ_KERNEL_CONFIGURATION->modules;
	VnzKernelPowerCore *power;
	uint32_t core, firstStart, primary = 0, i;

	if (table == NULL)
		return;
	core = currentCore();
	power = &g_vnzKernelPower.cores[core];
	firstStart = power->resumeFirstStart;
	if (firstStart)
		primary = power->resumePrimary;
	for (i = 0; i < table->count; i++)
		if (table->modules[i]->resume)
			table->modules[i]->resume(primary, firstStart);
	if (firstStart) {
		vnzKernelBootBarrier(primary, core, g_vnzKernelPower.expected, 4, 5);
		power->resumeFirstStart = 0;
		power->resumePrimary = 0;
	}
}

void vnzKernelModulesSuspend(void)
{
	const VnzKernelModuleTable *table = VNZ_KERNEL_CONFIGURATION->modules;
	uint32_t count, i;

	if (table == NULL)
		return;
	count = table->count;
	for (i = 0; i < count; i++)
		if (table->modules[count - i - 1]->suspend)
			table->modules[count - i - 1]->suspend();
}
