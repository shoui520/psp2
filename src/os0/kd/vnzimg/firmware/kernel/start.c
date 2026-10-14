/*
 * A core's start (from vkReset, kernel/vectors.S).
 *
 * The first core of the configuration's set to take the election word is the
 * primary: it sets up what the cores share. The cores meet at barriers, each
 * telling the stage it reached in g_vkBootStages. A core's first start sets
 * its thread system up; a start after a power down (g_vkBootReady set) finds
 * it in place.
 */
#include "kernel.h"

uint32_t g_vkBootFirst VK_SHARED;		/* the image's first start, until the primary is done */
uint32_t g_vkBootStarted[VNZ_CORES] VK_SHARED;	/* the core that won the election */
uint32_t g_vkBootReady[VNZ_CORES] VK_SHARED;	/* a core has started once */
volatile uint8_t g_vkBootStages[VNZ_CORES] VK_SHARED;

/* the configuration's words the cores meet on */
#define VK_BOOT_ELECTION(configuration)	((volatile uint8_t *)&(configuration)->election)
#define VK_BOOT_DONE			1

static uint32_t electionWon(volatile uint8_t *election)
{
	return vnzTestAndSet(election) == 0;
}

/* Turn the data cache on with no line valid. */
static void cacheClearTags(void)
{
	uint32_t size = vkCacheSize();
	volatile uint32_t *tag = (volatile uint32_t *)VNZ_DATA_RAM;

	if (size == 0)
		return;
	vnzWriteControl(VNZ_CR_CFG, vnzReadControl(VNZ_CR_CFG) & ~VNZ_CFG_DCACHE);
	mep_syncm();
	for (; size >= 0x40; size -= 0x40, tag += 0x10)
		*tag = 0;
	vnzWriteControl(VNZ_CR_CFG, vnzReadControl(VNZ_CR_CFG) | VNZ_CFG_DCACHE);
}

static uint32_t coreInSet(uint32_t cores, uint32_t core)
{
	return core < 9 && ((cores >> core) & 1);
}

static uint32_t coreCount(uint32_t cores)
{
	uint32_t core, count = 0;

	for (core = 0; core < VNZ_CORES; core++)
		if (coreInSet(cores, core))
			count++;
	return count;
}

/*
 * A barrier: each core says it reached stage; the primary waits for all of
 * cores to, then lets them on to next.
 */
void vkBootBarrier(uint32_t primary, uint32_t core, uint32_t cores, uint32_t stage, int32_t next)
{
	uint32_t count, reached, i;

	if ((int32_t)g_vkBootStages[core] < next)
		g_vkBootStages[core] = stage;
	if (primary) {
		count = coreCount(cores);
		do {
			reached = 0;
			for (i = 0; i < VNZ_CORES; i++)
				if (g_vkBootStages[i] == stage)
					reached++;
		} while (reached < count);
		for (i = 0; i < VNZ_CORES; i++)
			g_vkBootStages[i] = next;
		return;
	}
	while ((int32_t)g_vkBootStages[core] < next)
		;
}

static void bootStartBarrier(uint32_t primary, uint32_t core, uint32_t cores)
{
	int i;

	if (primary) {
		for (i = VNZ_CORES - 1; i >= 0; i--)
			g_vkBootStages[i] = 1;
	} else {
		while (((volatile int8_t *)g_vkBootStages)[0] == 0)
			;
	}
	vkBootBarrier(primary, core, cores, 2, 3);
}

/* Copy the local RAM code (kernel/cache.c) to local RAM. */
void vkLocalCodeCopy(uint32_t copy)
{
	extern const uint32_t vkLocalCodeLoad[], vkLocalCodeSize[];
	const uint32_t *from = (const uint32_t *)VNZ_PHYSICAL_OF((uint32_t)vkLocalCodeLoad);
	uint32_t *to = (uint32_t *)(VNZ_LOCAL_RAM + 0x1000);
	uint32_t size = (uint32_t)vkLocalCodeSize, done;

	if (!copy)
		return;
	for (done = 0; done < size; done += 4)
		*to++ = *from++;
}

void vkKernelStart(void)
{
	uint32_t coreId, core, primary, firstStart, starting;
	VkConfiguration *configuration;
	void (*bootConfigure)(void);

	vnzWriteControl(VNZ_CR_PSW, vnzReadControl(VNZ_CR_PSW) & ~VNZ_PSW_IEP);
	bootConfigure = (void *)VNZ_PUBLIC_OF((uint32_t)vkCacheBootConfigure);
	bootConfigure();
	cacheClearTags();
	coreId = vkCurrentCoreId();
	core = vnzCoreIndex();
	configuration = (VkConfiguration *)vkToPublic((uint32_t)VK_CONFIGURATION);

	if (((configuration->cores >> core) & 1) && electionWon(VK_BOOT_ELECTION(configuration))) {
		int i;

		for (i = 0; i < VNZ_CORES; i++) {
			g_vkBootStarted[i] = 0;
			g_vkBootReady[i] = 0;
			g_vkBootStages[i] = 0;
		}
		primary = 1;
		g_vkBootStarted[core] = 1;
		g_vkBootFirst = 1;
		configuration->bootDone = VK_BOOT_DONE;
	} else {
		while (((volatile VkConfiguration *)configuration)->bootDone != VK_BOOT_DONE)
			;
		primary = 0;
		g_vkBootStarted[core] = 0;
	}

	firstStart = g_vkBootReady[core] == 0;
	starting = VK_CONFIGURATION->cores & 0xff;
	if (g_vkBootFirst == 0 && vkPowerActiveCores() == 0)
		vkPowerLocalRestore();
	vkPowerInitialize(g_vkBootFirst, primary);
	vkPowerBootWait(primary, firstStart, starting, core);
	vkTraceInitialize(primary);
	vkThreadsInitialize(primary);
	vkIdleInitialize(primary, firstStart, core);
	vkHeapInitialize(primary);
	if (g_vkBootFirst == 0 && vkPowerActiveCores() == 0)
		vkPowerLocalFree();
	vkLockPagesInitialize(primary);
	vkCrashInitialize(primary);
	vkCoreInitialize(primary);
	vkLocalCodeCopy(primary);
	vkInterruptsInitialize(primary, firstStart);
	if (primary) {
		vkVThreadInitialize();
		vkL2Initialize();
		vkCacheWorkersInitialize();
	}
	if (firstStart) {
		g_vkBootReady[core] = 1;
		bootStartBarrier(primary, core, starting);
	}
	if (primary)
		g_vkBootFirst = 0;
	vnzWriteControl(VNZ_CR_PSW, vnzReadControl(VNZ_CR_PSW) | VNZ_PSW_IEP);
	vnzEnableInterrupts();
	vkIdleEntry(coreId, VK_CONFIGURATION, primary, firstStart);
}
