/*
 * Power: which cores are up.
 *
 * vkPowerSetCores asks for a set of cores. Each core has a power thread; a
 * core leaving the set is told by it and goes down (vkPowerSuspendCore, in
 * kernel/vectors.S), sleeping until a doorbell finds it back in the set. A
 * core coming back resumes the modules and is counted up again. When the
 * active set reaches the target, the threads waiting on the notify list are
 * told and the callbacks are called.
 *
 * When the last core goes down, the local RAM region is saved to the heap and
 * the L2 written back; the first core up restores it.
 */
#include "kernel.h"

#define VK_POWER_LOCAL_NONE	1	/* the local RAM save, by state */
#define VK_POWER_LOCAL_SAVED	2
#define VK_POWER_LOCAL_RESTORED	3
#define VK_POWER_LOCAL_FREED	4

#define VK_POWER_REGISTER	vnzRegister(0x4fd568)	/* kept over a power down */

/* a core's state over a power down (0x20 bytes) */
typedef struct VkPowerCore {
	uint32_t controller[6];		/* control bus registers 2 to 7 */
	uint32_t resumePrimary;
	uint32_t resumeFirstStart;
} VkPowerCore;

typedef struct VkPower {
	uint32_t active;		/* a bit per core up */
	uint32_t expected;		/* what the caller of vkPowerSetCores saw */
	uint32_t target;
	uint32_t changing;
	VkThread *threads[VNZ_CORES];
	VkPowerCore cores[VNZ_CORES];
	VkPowerList notify;
	VkPowerList callbacks;
	uint32_t savedRegister;
	void *localSave;		/* public */
	uint32_t localSaveSize;
	uint32_t localState;
} VkPower;

VkPower g_vkPower VK_SHARED;

static volatile uint32_t *const powerSemaphore = vkSemaphore(VK_SEMAPHORE_POWER);

static void localSave(void);
static void powerNotify(void);
static void powerCallbacks(uint32_t cores);

static uint32_t currentCore(void)
{
	return vkCurrentCoreId() - VNZ_FIRST_CORE_ID;
}

/* firstBoot: the image's first start (not a resume of every core) */
void vkPowerInitialize(uint32_t firstBoot, uint32_t primary)
{
	VkPower *power = &g_vkPower;
	int i;

	if (firstBoot) {
		if (!primary)
			return;
		vkGiveSemaphore(powerSemaphore);
		power->expected = 0;
		power->active = 0;
		power->notify.head = NULL;
		power->notify.tail = NULL;
		power->callbacks.head = NULL;
		power->callbacks.tail = NULL;
		power->changing = 1;
		power->target = VK_CONFIGURATION->cores;
		for (i = 0; i < VNZ_CORES; i++)
			power->threads[i] = NULL;
		power->localSave = NULL;
		power->localSaveSize = 0;
		power->localState = 0;
		return;
	}
	vkTakeSemaphore(powerSemaphore);
	if (power->changing == 0 && power->active == 0) {
		power->expected = 0;
		power->changing = 1;
		power->target = VK_CONFIGURATION->cores;
	}
	vkGiveSemaphore(powerSemaphore);
}

/* a core's power thread: down when told, then up again */
static void powerThread(void)
{
	uint32_t index;
	VkCore *core;
	VkThread *thread;

	vkDisableDispatch();
	core = vkCurrentCore(&index);
	thread = vkThreadCurrent();
	vnzWriteControl(VNZ_CR_PSW, vnzReadControl(VNZ_CR_PSW) & ~VNZ_PSW_IEP);
	vkPowerCoreUp();
	for (;;) {
		vnzWriteControl(VNZ_CR_PSW, vnzReadControl(VNZ_CR_PSW) | VNZ_PSW_IEP);
		vkThreadWaitFlags(1);
		vnzWriteControl(VNZ_CR_PSW, vnzReadControl(VNZ_CR_PSW) & ~VNZ_PSW_IEP);
		vkPowerSuspendCore(thread, core);
		vkPowerResume();
	}
}

/* At a core's start: its power thread, or (coming back up) a word to it. */
void vkPowerStart(uint32_t primary, uint32_t firstStart)
{
	uint32_t coreId = vkCurrentCoreId(), core = coreId - VNZ_FIRST_CORE_ID;
	VkThread *thread;

	g_vkPower.cores[core].resumeFirstStart = firstStart;
	g_vkPower.cores[core].resumePrimary = primary;
	if (firstStart) {
		thread = (VkThread *)vkThreadCreateHere(powerThread, coreId, 9, 0x200);
		if (thread == (VkThread *)-1)
			return;
		g_vkPower.threads[core] = thread;
		vkThreadStart(thread, 0, 0);
		vkThreadResume(thread);
		vkModulesResume();
		return;
	}
	thread = g_vkPower.threads[core];
	vkThreadSetFlags(thread, 1);
	vkThreadResume(thread);
}

void vkPowerSaveInterrupts(void)
{
	VkPowerCore *core = &g_vkPower.cores[currentCore()];
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

void vkPowerRestoreInterrupts(void)
{
	VkPowerCore *core = &g_vkPower.cores[currentCore()];
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
void vkPowerCoreDown(void)
{
	uint32_t bit = 1 << currentCore(), active;

	vkTakeSemaphore(powerSemaphore);
	active = g_vkPower.active & ~bit;
	g_vkPower.active = active;
	if (active == g_vkPower.target) {
		g_vkPower.changing = 0;
		powerNotify();
		if (active == 0) {
			/* the last one */
			vkGiveSemaphore(powerSemaphore);
			localSave();
			vkL2OperationAll(VK_L2_WRITEBACK);
		}
		powerCallbacks(active);
	}
	if (active != 0)
		vkGiveSemaphore(powerSemaphore);
}

/* Sleep until this core is in the target again. */
void vkPowerWaitUp(void)
{
	uint32_t core = currentCore(), bit = 1 << core;

	while (!(g_vkPower.target & bit)) {
		mep_halt();
		vnzRegister(VNZ_DOORBELLS + core * 0x20) = 0;
	}
}

/*
 * At boot, a core outside the cores to start waits until another core has
 * started (its barrier word set), then sleeps until it is wanted.
 */
void vkPowerBootWait(uint32_t primary, uint32_t firstStart, uint32_t starting, uint32_t core)
{
	uint32_t bit = 1 << core, psw, enabled;
	volatile int i;
	int first = -1, n;

	(void)primary;
	if (firstStart) {
		if (starting & bit)
			return;
		for (n = 0; n < VNZ_CORES; n++) {
			if (g_vkBootStarted[n]) {
				first = n;
				break;
			}
		}
		while (g_vkBootReady[first] == 0)
			for (i = 0; i < 100; i++)
				;
	}
	psw = vnzReadControl(VNZ_CR_PSW);
	enabled = vnzReadControlBus(VNZ_CB_INTERRUPT_ENABLE);
	vnzDisableInterrupts();
	vkDoorbellInitialize();
	vkInterruptSetEdge(VNZ_INTERRUPT_DOORBELL, 0);
	vkInterruptSetLevel(VNZ_INTERRUPT_DOORBELL, 10);
	vkInterruptSetEnable(VNZ_INTERRUPT_DOORBELL, 1);
	while (!(g_vkPower.target & bit)) {
		mep_sleep();
		vnzRegister(VNZ_DOORBELLS + core * 0x20) = 0;
	}
	vnzWriteControlBus(VNZ_CB_INTERRUPT_ENABLE, enabled);
	vnzWriteControl(VNZ_CR_PSW, psw);
}

/* This core is up. */
void vkPowerCoreUp(void)
{
	uint32_t bit = 1 << currentCore(), active;

	vkTakeSemaphore(powerSemaphore);
	active = g_vkPower.active | bit;
	g_vkPower.active = active;
	if (active == g_vkPower.target) {
		g_vkPower.changing = 0;
		powerNotify();
		powerCallbacks(active);
	}
	vkGiveSemaphore(powerSemaphore);
}

/* back from a power down (kernel/vectors.S) */
void vkPowerResume(void)
{
	vkPowerRestoreInterrupts();
	vkModulesResume();
	vkPowerCoreUp();
}

/*
 * The local RAM region over a power down.
 */

/* a buffer for it; returns 0 only when there is no memory for one */
static uint32_t localSaveAllocate(void)
{
	uint32_t base, size = 0, result = 1;
	void *buffer = NULL;

	base = vkMemoryRegion(VK_MEMORY_LOCAL_RAM, &size);
	if (base == 0 || base < VNZ_LOCAL_RAM || base >= VNZ_LOCAL_RAM + 0x2000)
		size = 0;
	if (size) {
		buffer = vkHeapAllocate(vkSystemHeap(), size, 0x40);
		if (buffer == NULL) {
			size = 0;
			result = 0;
		} else {
			buffer = (void *)vkPhysicalToPublic((uint32_t)buffer);
			result = (uint32_t)buffer;
		}
	}
	g_vkPower.localState = VK_POWER_LOCAL_NONE;
	g_vkPower.localSave = buffer;
	g_vkPower.localSaveSize = size;
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

	g_vkPower.savedRegister = VK_POWER_REGISTER;
	if (g_vkPower.localSave) {
		buffer = (uint32_t *)vkPublicToPhysical((uint32_t)g_vkPower.localSave);
		size = g_vkPower.localSaveSize;
		copyWords(buffer, (const uint32_t *)vkMemoryRegion(VK_MEMORY_LOCAL_RAM, NULL), size);
		vkCacheWriteback((uint32_t)buffer, (size + 63) & ~63);
	}
	g_vkPower.localState = VK_POWER_LOCAL_SAVED;
}

/* The first core up puts local RAM back; the others wait for it. */
void vkPowerLocalRestore(void)
{
	uint32_t *buffer, size;
	volatile int i;

	if (vkTrySemaphore(powerSemaphore) != 0) {
		while (g_vkPower.localState < VK_POWER_LOCAL_RESTORED)
			for (i = 0; i < 50; i++)
				;
		return;
	}
	if (g_vkPower.localState < VK_POWER_LOCAL_RESTORED) {
		VK_POWER_REGISTER = g_vkPower.savedRegister;
		if (g_vkPower.localSave) {
			buffer = (uint32_t *)vkPublicToPhysical((uint32_t)g_vkPower.localSave);
			size = g_vkPower.localSaveSize;
			copyWords((uint32_t *)vkMemoryRegion(VK_MEMORY_LOCAL_RAM, NULL), buffer, size);
			vkCachePurge((uint32_t)buffer, (size + 63) & ~63);
		}
		vkLocalCodeCopy(1);
		g_vkPower.localState = VK_POWER_LOCAL_RESTORED;
	}
	vkGiveSemaphore(powerSemaphore);
}

/* Free the buffer once the cores are back. */
void vkPowerLocalFree(void)
{
	volatile int i;

	if (vkTrySemaphore(powerSemaphore) != 0) {
		while (g_vkPower.localState < VK_POWER_LOCAL_FREED)
			for (i = 0; i < 50; i++)
				;
		return;
	}
	if (g_vkPower.localState < VK_POWER_LOCAL_FREED) {
		if (g_vkPower.localSave)
			vkHeapFree(vkSystemHeap(),
				(void *)vkPublicToPhysical((uint32_t)g_vkPower.localSave),
				g_vkPower.localSaveSize, 0x40);
		g_vkPower.localState = VK_POWER_LOCAL_FREED;
	}
	vkGiveSemaphore(powerSemaphore);
}

/*
 * Ask for the cores in cores (a bit each), when expected is the set up now.
 * Returns 0, or the set up now when it is not expected (or a change is
 * under way).
 */
uint32_t vkPowerSetCores(uint32_t expected, uint32_t cores)
{
	VkPower *power = &g_vkPower;
	uint32_t active, result, core, bit;
	VkThread *thread;

	vkDisableDispatch();
	vkTakeSemaphore(powerSemaphore);
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
			vkThreadSetFlags(thread, 1);
			vkThreadResume(thread);
		} else {
			/* not started yet: ring it */
			vnzRegister(VNZ_DOORBELLS + 0x10 + vnzCoreIndex() * 0x20) = bit;
		}
	}
out:
	vkGiveSemaphore(powerSemaphore);
	vkEnableDispatch();
	return result;
}

uint32_t vkPowerActiveCores(void)
{
	return g_vkPower.active;
}

/*
 * The notify list (threads sent flags) and the callbacks (called with the
 * new set), when the active set reaches the target.
 */
static VkPowerHook *hookCreate(VkThread *thread, uint32_t flags,
	void (*function)(uint32_t cores, uint32_t argument), uint32_t argument)
{
	VkPowerHook *hook = vkHeapAllocate(vkCurrentHeap(), sizeof(VkPowerHook), 0x40);

	if (hook == NULL)
		return NULL;
	hook->thread = thread;
	hook->flags = flags;
	hook->function = function;
	hook->argument = argument;
	hook->next = NULL;
	hook->previous = NULL;
	hook->registered = 0;
	hook->magic = VK_HOOK_MAGIC;
	vkCacheWriteback((uint32_t)hook, 0x40);
	return (VkPowerHook *)vkPhysicalToPublic((uint32_t)hook);
}

VkPowerHook *vkPowerNotifyCreate(VkThread *thread, uint32_t flags)
{
	return hookCreate(thread, flags, NULL, 0);
}

VkPowerHook *vkPowerCallbackCreate(void (*function)(uint32_t cores, uint32_t argument),
	uint32_t argument)
{
	return hookCreate(NULL, 0, function, argument);
}

static void hookAppend(VkPowerList *list, VkPowerHook *hook)
{
	if (list->head) {
		hook->next = NULL;
		hook->previous = list->tail;
		list->tail->next = hook;
	} else {
		hook->next = NULL;
		hook->previous = NULL;
		list->head = hook;
	}
	list->tail = hook;
}

void vkPowerHookPrepend(VkPowerList *list, VkPowerHook *hook)
{
	if (list->head) {
		hook->previous = NULL;
		hook->next = list->head;
		list->head->previous = hook;
		list->head = hook;
	} else {
		hook->next = NULL;
		hook->previous = NULL;
		list->tail = hook;
		list->head = hook;
	}
}

static void hookRemove(VkPowerList *list, VkPowerHook *hook)
{
	if (hook->next == NULL && hook->previous == NULL) {
		list->head = NULL;
		list->tail = NULL;
	} else if (hook->previous == NULL) {
		list->head = hook->next;
		hook->next->previous = NULL;
	} else if (hook->next == NULL) {
		hook->previous->next = NULL;
		list->tail = hook->previous;
	} else {
		hook->previous->next = hook->next;
		hook->next->previous = hook->previous;
	}
}

static void hookRegister(VkPowerList *list, VkPowerHook *hook)
{
	vkDisableDispatch();
	vkTakeSemaphore(powerSemaphore);
	if (!hook->registered) {
		hookAppend(list, hook);
		hook->registered = 1;
	}
	vkGiveSemaphore(powerSemaphore);
	vkEnableDispatch();
}

static void hookUnregister(VkPowerList *list, VkPowerHook *hook)
{
	vkDisableDispatch();
	vkTakeSemaphore(powerSemaphore);
	if (hook->registered == 1) {
		hookRemove(list, hook);
		hook->registered = 0;
	}
	vkGiveSemaphore(powerSemaphore);
	vkEnableDispatch();
}

void vkPowerNotifyRegister(VkPowerHook *hook)
{
	hookRegister(&g_vkPower.notify, hook);
}

void vkPowerCallbackRegister(VkPowerHook *hook)
{
	hookRegister(&g_vkPower.callbacks, hook);
}

void vkPowerNotifyUnregister(VkPowerHook *hook)
{
	hookUnregister(&g_vkPower.notify, hook);
}

void vkPowerCallbackUnregister(VkPowerHook *hook)
{
	hookUnregister(&g_vkPower.callbacks, hook);
}

static void hookFree(VkPowerHook *hook)
{
	hook->magic = 0;
	vkHeapFree(vkCurrentHeap(), (void *)vkPublicToPhysical((uint32_t)hook),
		sizeof(VkPowerHook), 0x40);
}

void vkPowerNotifyDelete(VkPowerHook *hook)
{
	hookUnregister(&g_vkPower.notify, hook);
	hookFree(hook);
}

void vkPowerCallbackDelete(VkPowerHook *hook)
{
	hookUnregister(&g_vkPower.callbacks, hook);
	hookFree(hook);
}

static void powerNotify(void)
{
	VkPowerHook *hook;

	for (hook = g_vkPower.notify.head; hook; hook = hook->next)
		vkThreadSetFlags(hook->thread, hook->flags);
}

static void powerCallbacks(uint32_t cores)
{
	VkPowerHook *hook;

	for (hook = g_vkPower.callbacks.head; hook; hook = hook->next)
		hook->function(cores, hook->argument);
}

/*
 * The modules: resumed in order at a core's start, suspended in reverse
 * before it goes down.
 */
void vkModulesResume(void)
{
	const VkModuleTable *table = VK_CONFIGURATION->modules;
	VkPowerCore *power;
	uint32_t core, firstStart, primary = 0, i;

	if (table == NULL)
		return;
	core = currentCore();
	power = &g_vkPower.cores[core];
	firstStart = power->resumeFirstStart;
	if (firstStart)
		primary = power->resumePrimary;
	for (i = 0; i < table->count; i++)
		if (table->modules[i]->resume)
			table->modules[i]->resume(primary, firstStart);
	if (firstStart) {
		vkBootBarrier(primary, core, g_vkPower.expected, 4, 5);
		power->resumeFirstStart = 0;
		power->resumePrimary = 0;
	}
}

void vkModulesSuspend(void)
{
	const VkModuleTable *table = VK_CONFIGURATION->modules;
	uint32_t count, i;

	if (table == NULL)
		return;
	count = table->count;
	for (i = 0; i < count; i++)
		if (table->modules[count - i - 1]->suspend)
			table->modules[count - i - 1]->suspend();
}
