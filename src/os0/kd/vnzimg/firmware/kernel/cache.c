/*
 * The caches.
 *
 * Each core has its own data cache; the cores share the L2. A line operation
 * acts on the lines of a range; past a size, the whole cache is cheaper (or a
 * walk of the tags, which clears the valid bit of the lines in the range).
 *
 * Another core's data cache is reached by a request: each core has a worker
 * thread and a ring of four requests per cache, filled by the other cores
 * (vkCachePurgeAllCores, vkL2PurgeAllCores).
 *
 * The L2's mode is changed and the L2 turned off by code in local RAM, with
 * the other cores parked there (their workers spin in local RAM while it
 * changes).
 */
#include "kernel.h"

#define VK_DCACHE_TAGS		VNZ_DATA_RAM
#define VK_L2_TAGS		0x310000
#define VK_CACHE_LINE		0x40

/* the L2 controller (VNZ_L2_CONTROL) */
#define VK_L2_ADDRESS		vnzRegister(VNZ_L2_CONTROL + 0x00)
#define VK_L2_COMMAND		vnzRegister(VNZ_L2_CONTROL + 0x04)	/* size | operation; low bits: busy */
#define VK_L2_BUSY		vnzRegister(VNZ_L2_CONTROL + 0x0c)
#define VK_L2_STOP		vnzRegister(VNZ_L2_CONTROL + 0x10)
#define VK_L2_MODE		vnzRegister(VNZ_L2_CONTROL + 0x14)
#define VK_L2_ENABLE		vnzRegister(VNZ_L2_CONTROL + 0x18)
#define VK_L2_LINE_COMMAND	vnzRegister(VNZ_DMA_FLUSH)		/* line | operation */

/* a request's operation (the low four bits of its size) */
#define VK_REQUEST_WRITEBACK	0x2
#define VK_REQUEST_L2_TAGS	0xe
#define VK_REQUEST_L1_TAGS	0xf
#define VK_REQUEST_MASK		0xf
#define VK_REQUESTS		4

/* a core's requests for one cache (0x40 bytes, physical) */
typedef struct VkCacheRequests {
	VkThread *worker;
	uint32_t read;
	uint32_t write;
	struct {
		uint32_t address;
		uint32_t sizeOperation;
	} entries[VK_REQUESTS];
	uint32_t reserved2c[5];
} VkCacheRequests;

static const int16_t dcacheSizes[] = {
	0, 0x400, 0x800, 0, 0x1000, 0, 0, 0, 0x2000, 0,
};

VkCacheRequests g_vkL1Requests[VNZ_CORES] VK_LOCAL __attribute__((aligned(VK_CACHE_LINE)));
VkCacheRequests g_vkL2Requests[VNZ_CORES] VK_LOCAL __attribute__((aligned(VK_CACHE_LINE)));
VkThread *g_vkParkWorkers[VNZ_CORES] VK_LOCAL __attribute__((aligned(VK_CACHE_LINE)));
volatile uint8_t *g_vkL1RequestLocks[VNZ_CORES] VK_SHARED;
volatile uint8_t *g_vkL2RequestLocks[VNZ_CORES] VK_SHARED;
volatile uint8_t *g_vkL2Lock VK_SHARED;
volatile uint32_t g_vkL2TagWalks VK_SHARED;

/* a flag per core: 1 asks it to park in local RAM, 2 while it is parked */
volatile uint8_t g_vkParkFlags[VNZ_CORES] VK_SECTION(".localdata");

/*
 * Data cache line operations.
 */
void vkCachePurge(uint32_t address, uint32_t size)
{
	uint32_t n;

	if (size > 0xed80) {
		vkCacheInvalidateTags(address, size, 0);
		return;
	}
	for (n = size >> 6; n; n--, address += VK_CACHE_LINE)
		vnzCacheLine(VNZ_CACHE_PURGE, address);
}

void vkCacheFlush(uint32_t address, uint32_t size)
{
	uint32_t n;

	for (n = size >> 6; n; n--, address += VK_CACHE_LINE)
		vnzCacheLine(VNZ_CACHE_FLUSH, address);
}

void vkCacheWriteback(uint32_t address, uint32_t size)
{
	uint32_t n;

	if (size >= 0x8000) {
		vkCacheWritebackAll();
		return;
	}
	for (n = size >> 6; n; n--, address += VK_CACHE_LINE)
		vnzCacheLine(VNZ_CACHE_WRITEBACK, address);
}

void vkCachePrefetch(uint32_t address, uint32_t size)
{
	uint32_t n;

	for (n = size >> 6; n; n--, address += VK_CACHE_LINE)
		vnzPrefetch(address);
}

void vkCacheInvalidate(uint32_t address, uint32_t size)
{
	uint32_t n;

	for (n = size >> 6; n; n--, address += VK_CACHE_LINE)
		vnzCacheLine(VNZ_CACHE_INVALIDATE, address);
}

/* the data cache's size, from the configuration register */
uint32_t vkCacheSize(void)
{
	uint32_t index = vnzReadControl(VNZ_CR_CCFG) & 0x7f;

	if (index == 0x10)
		return 0x4000;
	if (index >= 9)
		index = 9;
	return dcacheSizes[index];
}

/* the whole data cache by index (operations 1 and 3 act on the line an index selects) */
void vkCacheInvalidateAll(void)
{
	uint32_t size = vkCacheSize(), offset;

	for (offset = 0; offset < size; offset += VK_CACHE_LINE)
		vnzCacheLine(1, VNZ_DRAM + offset);
}

void vkCacheWritebackAll(void)
{
	uint32_t size = vkCacheSize(), offset;

	for (offset = 0; offset < size; offset += VK_CACHE_LINE)
		vnzCacheLine(3, VNZ_DRAM + offset);
}

/*
 * Drop the data cache's lines of [address, address + size) by their tags,
 * with the cache off; with keepDirty, written lines stay.
 */
void vkCacheInvalidateTags(uint32_t address, uint32_t size, uint32_t keepDirty)
{
	uint32_t cacheSize = (vnzReadControl(VNZ_CR_CCFG) & 0x7f) << 9;
	uint32_t indexMask = (cacheSize - 1) & ~(VK_CACHE_LINE - 1);
	uint32_t tagMask = ~(cacheSize - 1);
	uint32_t end = address + size, groups = ((cacheSize >> 6) << 1) >> 4;
	volatile uint32_t *tag = (volatile uint32_t *)VK_DCACHE_TAGS;
	uint32_t psw = vnzReadControl(VNZ_CR_PSW), cfg, value, line, group;
	int i;

	for (group = 0; group < groups; group++) {
		vnzDisableInterrupts();
		cfg = vnzReadControl(VNZ_CR_CFG);
		vnzWriteControl(VNZ_CR_CFG, cfg & ~VNZ_CFG_DCACHE);
		mep_syncm();
		for (i = 0; i < 16; i++, tag += VK_CACHE_LINE / 4) {
			value = *tag;
			if (!(value & 1))
				continue;
			if (keepDirty && (value & 2))
				continue;
			line = (value & tagMask) + ((uint32_t)tag & indexMask);
			if (line < address || line >= end)
				continue;
			*tag = 0;
		}
		vnzWriteControl(VNZ_CR_CFG, cfg);
		vnzWriteControl(VNZ_CR_PSW, psw);
	}
}

static void cacheInvalidateCleanTags(uint32_t address, uint32_t size)
{
	vkCacheInvalidateTags(address, size, 1);
}

/*
 * Requests to the other cores.
 */
static void postRequests(VkCacheRequests *requests, volatile uint8_t **locks, uint32_t address,
	uint32_t sizeOperation, uint32_t fallbackAddress, uint32_t fallbackOperation)
{
	uint32_t self, core, read, write;
	VkCacheRequests *queue;
	VkThread *worker;
	volatile uint8_t *lock;

	vkDisableDispatch();
	self = vkCurrentCoreId() - VNZ_FIRST_CORE_ID;
	for (core = 0; core < VNZ_CORES; core++) {
		if (!(vkPowerActiveCores() & (1 << core)) || core == self)
			continue;
		queue = &requests[core];
		vnzCacheLine(VNZ_CACHE_PURGE, queue);
		worker = queue->worker;
		if (worker == NULL)
			continue;
		lock = locks[core];
		vkSpinLock(lock);
		vnzCacheLine(VNZ_CACHE_PURGE, queue);
		read = queue->read;
		write = queue->write;
		if (read + VK_REQUESTS == write) {
			/* full: one request for everything instead */
			queue->read = 0;
			queue->write = 1;
			queue->entries[0].address = fallbackAddress;
			queue->entries[0].sizeOperation = fallbackOperation;
		} else {
			queue->entries[write & (VK_REQUESTS - 1)].address = address;
			queue->entries[write & (VK_REQUESTS - 1)].sizeOperation = sizeOperation;
			queue->write = write + 1;
		}
		vnzCacheLine(VNZ_CACHE_WRITEBACK, queue);
		vkSpinUnlock(lock);
		vkThreadSetFlags(worker, 1);
	}
	vkEnableDispatch();
}

/* Purge a range from every core's data cache. */
void vkCachePurgeAllCores(uint32_t address, uint32_t size)
{
	uint32_t operation = size < 0xed80 ? VK_REQUEST_WRITEBACK : VK_REQUEST_L1_TAGS;

	postRequests(g_vkL1Requests, g_vkL1RequestLocks, address, size | operation,
		VNZ_DRAM, 0xa8000000 | VK_REQUEST_L1_TAGS);
	vkCachePurge(address, size);
}

/* Drop a range's L2 lines by their tags, here and through the other cores. */
void vkL2PurgeAllCores(uint32_t address, uint32_t size)
{
	postRequests(g_vkL2Requests, g_vkL2RequestLocks, address, size | VK_REQUEST_L2_TAGS,
		0, ~(VK_CACHE_LINE - 1) | VK_REQUEST_L2_TAGS);
	vkL2InvalidateTags(address, size);
}

void vkL2InvalidateTags(uint32_t address, uint32_t size)
{
	uint32_t l2Size = ((vnzReadControl(VNZ_CR_CCFG) & 0x7f0000) >> 16) << 9;
	uint32_t indexMask = (l2Size - 1) & ~(VK_CACHE_LINE - 1);
	uint32_t tagMask = ~(l2Size - 1);
	uint32_t groups = ((l2Size >> 6) << 1) >> 4, group;
	uint32_t tags = VK_L2_TAGS;
	void (*walk)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

	/* the walk runs through the public window */
	walk = (void *)VNZ_PUBLIC_OF((uint32_t)vkL2WalkTags);
	for (group = 0; group < groups; group++, tags += 0x400)
		walk(address, size, tags, tagMask, indexMask);
}

static void runRequest(uint32_t address, uint32_t sizeOperation)
{
	uint32_t size = sizeOperation & ~VK_REQUEST_MASK;

	switch (sizeOperation & VK_REQUEST_MASK) {
	case VK_REQUEST_L1_TAGS:
		cacheInvalidateCleanTags(address, size);
		return;
	case VK_REQUEST_L2_TAGS:
		vkL2InvalidateTags(address, size);
		return;
	case VK_REQUEST_WRITEBACK:
		vkCacheWriteback(address, size);
		return;
	}
}

static void runRequests(VkCacheRequests *queue, volatile uint8_t *lock)
{
	uint32_t read, address, sizeOperation, empty;

	for (;;) {
		vkDisableDispatch();
		vkSpinLock(lock);
		vnzCacheLine(VNZ_CACHE_PURGE, queue);
		read = queue->read;
		if (read != queue->write) {
			empty = 0;
			address = queue->entries[read & (VK_REQUESTS - 1)].address;
			sizeOperation = queue->entries[read & (VK_REQUESTS - 1)].sizeOperation;
			queue->read = read + 1;
			vnzCacheLine(VNZ_CACHE_WRITEBACK, queue);
		} else {
			empty = 1;
			address = 0;
			sizeOperation = 0;
			vnzCacheLine(VNZ_CACHE_PURGE, queue);
		}
		vkSpinUnlock(lock);
		vkEnableDispatch();
		if (empty)
			return;
		runRequest(address, sizeOperation);
	}
}

static void requestWorker(VkCacheRequests *queue, volatile uint8_t *lock)
{
	for (;;)
		if (vkThreadWaitFlags(1) & 1)
			runRequests(queue, lock);
}

static void requestWorkerCreate(VkCacheRequests *queue, volatile uint8_t **lock, uint32_t coreId)
{
	VkThread *worker;
	int i;

	worker = (VkThread *)vkThreadCreate(requestWorker, coreId, 9, 0x100, vkSystemHeap());
	*lock = vkLockPageAllocate();
	vnzCacheLine(VNZ_CACHE_INVALIDATE, queue);
	queue->worker = worker;
	queue->read = 0;
	queue->write = 0;
	for (i = 0; i < VK_REQUESTS; i++) {
		queue->entries[i].address = 0;
		queue->entries[i].sizeOperation = 0;
	}
	vnzCacheLine(VNZ_CACHE_WRITEBACK, queue);
	mep_syncm();
}

void vkCacheWorkersInitialize(void)
{
	uint32_t core;

	for (core = 0; core < VNZ_CORES; core++) {
		requestWorkerCreate(&g_vkL1Requests[core], &g_vkL1RequestLocks[core],
			VNZ_FIRST_CORE_ID + core);
		requestWorkerCreate(&g_vkL2Requests[core], &g_vkL2RequestLocks[core],
			VNZ_FIRST_CORE_ID + core);
	}
}

/* Start this core's two workers. */
void vkCacheStartWorkers(uint32_t coreId)
{
	uint32_t core = coreId - VNZ_FIRST_CORE_ID;
	VkCacheRequests *queue;
	volatile uint8_t *lock;
	VkThread *worker;
	int i;

	for (i = 0; i < 2; i++) {
		if (i == 0) {
			queue = &g_vkL1Requests[core];
			lock = g_vkL1RequestLocks[core];
		} else {
			queue = &g_vkL2Requests[core];
			lock = g_vkL2RequestLocks[core];
		}
		vnzCacheLine(VNZ_CACHE_PURGE, queue);
		worker = queue->worker;
		vkThreadStart(worker, (uint32_t)queue, (uint32_t)lock);
		vkThreadDetach(worker);
	}
}

/*
 * The L2.
 */

/* An operation on a range, through the controller. */
void vkL2Operation(uint32_t address, uint32_t size, uint32_t operation)
{
	volatile int i;

	if (address >= 0xc0000000 && address < 0xd8000000)
		address = vkPublicToPhysical(address);
	vkDisableDispatch();
	vkSpinLock(g_vkL2Lock);
	VK_L2_ADDRESS = address;
	VK_L2_COMMAND = size + operation;
	while (VK_L2_COMMAND & 7)
		for (i = 0; i < 50; i++)
			;
	vkSpinUnlock(g_vkL2Lock);
	vkEnableDispatch();
}

/* An operation on each 0x100-byte line of a range. */
void vkL2LineOperation(uint32_t address, uint32_t size, uint32_t operation)
{
	uint32_t line, n;

	if (address >= 0xc0000000 && address < 0xd8000000)
		address = vkPublicToPhysical(address);
	line = address;
	for (n = size >> 8; n; n--, line += 0x100)
		VK_L2_LINE_COMMAND = line + operation;
	/* the last line once more, for these */
	if (operation == VK_L2_WRITEBACK || operation == VK_L2_PURGE)
		VK_L2_LINE_COMMAND = line - 0x100 + operation;
}

/* An operation on the whole L2. */
void vkL2OperationAll(uint32_t operation)
{
	uint32_t lock = (uint32_t)g_vkL2Lock;

	vkL2Operation(VNZ_DRAM, 0xa8000000, operation);
	if (lock < VNZ_LOCAL_RAM || lock >= VNZ_LOCAL_RAM + 0x2000)
		vkL2LineOperation(lock & ~0xff, 0x100, VK_L2_PURGE);
}

void vkL2SetMode(uint32_t mode)
{
	vkDisableDispatch();
	vkSpinLock(g_vkL2Lock);
	if (VK_L2_MODE != mode)
		vkLocalL2SetMode(mode);
	vkSpinUnlock(g_vkL2Lock);
	vkEnableDispatch();
}

/* Turn the L2 off, the other cores parked meanwhile. */
void vkL2Disable(void)
{
	uint32_t self, core;
	volatile uint32_t delay;

	if (VK_L2_ENABLE == 0)
		return;
	vkDisableDispatch();
	vkSpinLock(g_vkL2Lock);
	self = vkCurrentCoreId() - VNZ_FIRST_CORE_ID;
	for (core = 0; core < VNZ_CORES; core++) {
		if (!(vkPowerActiveCores() & (1 << core)) || core == self)
			continue;
		g_vkParkFlags[core] = 1;
		vkThreadSetFlags(g_vkParkWorkers[core], 2);
	}
restart:
	for (core = 0; core < VNZ_CORES; core++) {
		if (!(vkPowerActiveCores() & (1 << core)) || core == self)
			continue;
		if (g_vkParkFlags[core] != 2) {
			for (delay = 0; delay < 0xff; delay++)
				;
			goto restart;
		}
	}
	vkLocalL2Disable();
	for (core = 0; core < VNZ_CORES; core++) {
		if (!(vkPowerActiveCores() & (1 << core)) || core == self)
			continue;
		g_vkParkFlags[core] = 0;
	}
	vkSpinUnlock(g_vkL2Lock);
	vkEnableDispatch();
}

void vkL2Enable(void)
{
	VK_L2_ENABLE = 1;
}

/* a core's worker parks it in local RAM when asked */
static void parkWorker(void)
{
	uint32_t core;

	vkDisableDispatch();
	core = vkCurrentCoreId() - VNZ_FIRST_CORE_ID;
	for (;;) {
		vkThreadWaitFlags(2);
		vkLocalPark(&g_vkParkFlags[core]);
	}
}

static void parkWorkersCreate(void)
{
	uint32_t core;

	vkCacheInvalidate((uint32_t)g_vkParkWorkers, sizeof(g_vkParkWorkers));
	for (core = 0; core < VNZ_CORES; core++)
		g_vkParkWorkers[core] = (VkThread *)vkThreadCreate(parkWorker,
			VNZ_FIRST_CORE_ID + core, 9, 0x100, vkSystemHeap());
	vkCacheWriteback((uint32_t)g_vkParkWorkers, sizeof(g_vkParkWorkers));
	mep_syncm();
}

void vkLocalCodeStartWorker(uint32_t coreId)
{
	VkThread *worker = g_vkParkWorkers[coreId - VNZ_FIRST_CORE_ID];

	vkThreadStart(worker, 0, 0);
	vkThreadDetach(worker);
}

void vkL2Initialize(void)
{
	int i;

	g_vkL2Lock = vkLockPageAllocate();
	for (i = 0; i < VNZ_CORES; i++)
		g_vkParkFlags[i] = 0;
	parkWorkersCreate();
}

/* the address if it is in DRAM's physical window, otherwise 0 */
uint32_t vkDramAddress(uint32_t address)
{
	if (address >= VNZ_DRAM && address < VNZ_DRAM + 0xa8000000)
		return address;
	return 0;
}

/*
 * At boot, before the caches are used: clear the L2's tags and turn it on in
 * the configuration register.
 */
void vkCacheBootConfigure(void)
{
	uint32_t ccfg = vnzReadControl(VNZ_CR_CCFG), size, bits;
	volatile uint32_t *tag = (volatile uint32_t *)VK_L2_TAGS;

	switch (ccfg & 0x7f0000) {
	case 0x100000:
		size = 0x4000;
		break;
	case 0x080000:
		size = 0x2000;
		break;
	case 0x040000:
		size = 0x1000;
		break;
	case 0x020000:
		size = 0x800;
		break;
	case 0x010000:
		size = 0x400;
		break;
	default:
		return;
	}
	bits = (ccfg & 0x40000000) ? 0x402 : 0x2;
	for (; size >= VK_CACHE_LINE; size -= VK_CACHE_LINE, tag += VK_CACHE_LINE / 4)
		*tag = 0;
	vnzWriteControl(VNZ_CR_CFG, (vnzReadControl(VNZ_CR_CFG) & ~0x402) | bits);
}

void vkL2TagWalkMark(void)
{
	g_vkL2TagWalks = 0;
}

/* One group of 16 L2 tags, run through the public window with the L2 off. */
void vkL2WalkTags(uint32_t address, uint32_t size, uint32_t tags, uint32_t tagMask,
	uint32_t indexMask)
{
	uint32_t end = address + size, psw, cfg, value, line;
	volatile uint32_t *tag = (volatile uint32_t *)tags;
	void (*mark)(void);
	int i;

	psw = vnzReadControl(VNZ_CR_PSW);
	vnzDisableInterrupts();
	cfg = vnzReadControl(VNZ_CR_CFG);
	vnzWriteControl(VNZ_CR_CFG, cfg & ~0x2);
	mep_syncm();
	mark = (void *)VNZ_PUBLIC_OF((uint32_t)vkL2TagWalkMark);
	mark();
	for (i = 0; i < 16; i++, tag += VK_CACHE_LINE / 4) {
		value = *tag;
		if (!(value & 1))
			continue;
		line = (value & tagMask) + ((uint32_t)tag & indexMask);
		if (line < address || line >= end)
			continue;
		*tag = 0;
	}
	vnzWriteControl(VNZ_CR_CFG, cfg);
	vnzWriteControl(VNZ_CR_PSW, psw);
}

/*
 * The local RAM code (copied to VNZ_LOCAL_RAM + 0x1000 at boot): it runs
 * while the L2 changes, so it reaches nothing outside local RAM.
 */
#define VK_LOCAL_CODE	VK_SECTION(".localcode") __attribute__((noinline))

VK_LOCAL_CODE void vkLocalL2SetMode(uint32_t mode)
{
	mep_di();
	mep_syncm();
	VK_L2_STOP = 1;
	while (VK_L2_BUSY)
		;
	if (mode) {
		VK_L2_ADDRESS = VNZ_DRAM;
		VK_L2_COMMAND = VK_L2_FLUSH;
		while (VK_L2_COMMAND & 7)
			;
	}
	VK_L2_MODE = mode;
	VK_L2_STOP = 0;
	mep_ei();
}

VK_LOCAL_CODE void vkLocalPark(volatile uint8_t *flag)
{
	if (*flag == 0)
		return;
	mep_di();
	*flag = 2;
	while (*flag)
		;
	mep_ei();
}

VK_LOCAL_CODE void vkLocalL2Disable(void)
{
	mep_di();
	mep_syncm();
	while (VK_L2_BUSY)
		;
	VK_L2_ADDRESS = VNZ_DRAM;
	VK_L2_COMMAND = VK_L2_PURGE;
	while (VK_L2_COMMAND & 7)
		;
	VK_L2_ENABLE = 0;
	mep_ei();
}
