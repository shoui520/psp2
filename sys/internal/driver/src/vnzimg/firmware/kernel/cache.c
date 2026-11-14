/*
 * The caches.
 *
 * Each core has its own data cache; the cores share the L2. A line operation
 * acts on the lines of a range; past a size, the whole cache is cheaper (or a
 * walk of the tags, which clears the valid bit of the lines in the range).
 *
 * Another core's data cache is reached by a request: each core has a worker
 * thread and a ring of four requests per cache, filled by the other cores
 * (vnzKernelCachePurgeAllCores, vnzKernelL2PurgeAllCores).
 *
 * The L2's mode is changed and the L2 turned off by code in local RAM, with
 * the other cores parked there (their workers spin in local RAM while it
 * changes).
 */
#include "kernel.h"

#define VNZ_KERNEL_DCACHE_TAGS		VNZ_DATA_RAM
#define VNZ_KERNEL_L2_TAGS		0x310000
#define VNZ_KERNEL_CACHE_LINE		0x40

/* the L2 controller (VNZ_L2_CONTROL) */
#define VNZ_KERNEL_L2_ADDRESS		vnzRegister(VNZ_L2_CONTROL + 0x00)
/* size | operation; low bits: busy */
#define VNZ_KERNEL_L2_COMMAND		vnzRegister(VNZ_L2_CONTROL + 0x04)
#define VNZ_KERNEL_L2_BUSY		vnzRegister(VNZ_L2_CONTROL + 0x0c)
#define VNZ_KERNEL_L2_STOP		vnzRegister(VNZ_L2_CONTROL + 0x10)
#define VNZ_KERNEL_L2_MODE		vnzRegister(VNZ_L2_CONTROL + 0x14)
#define VNZ_KERNEL_L2_ENABLE		vnzRegister(VNZ_L2_CONTROL + 0x18)
#define VNZ_KERNEL_L2_LINE_COMMAND	vnzRegister(VNZ_DMA_FLUSH)	/* line | operation */

/* a request's operation (the low four bits of its size) */
#define VNZ_KERNEL_REQUEST_WRITEBACK	0x2
#define VNZ_KERNEL_REQUEST_L2_TAGS	0xe
#define VNZ_KERNEL_REQUEST_L1_TAGS	0xf
#define VNZ_KERNEL_REQUEST_MASK		0xf
#define VNZ_KERNEL_REQUESTS		4

/* a core's requests for one cache (0x40 bytes, physical) */
typedef struct VnzKernelCacheRequests {
	VnzKernelThread *worker;
	uint32_t read;
	uint32_t write;
	struct {
		uint32_t address;
		uint32_t sizeOperation;
	} entries[VNZ_KERNEL_REQUESTS];
	uint32_t reserved2c[5];
} VnzKernelCacheRequests;

static const int16_t dcacheSizes[] = {
	0, 0x400, 0x800, 0, 0x1000, 0, 0, 0, 0x2000, 0,
};

VnzKernelCacheRequests g_vnzKernelL1Requests[VNZ_CORES] VNZ_KERNEL_LOCAL __attribute__((aligned(VNZ_KERNEL_CACHE_LINE)));
VnzKernelCacheRequests g_vnzKernelL2Requests[VNZ_CORES] VNZ_KERNEL_LOCAL __attribute__((aligned(VNZ_KERNEL_CACHE_LINE)));
VnzKernelThread *g_vnzKernelParkWorkers[VNZ_CORES] VNZ_KERNEL_LOCAL __attribute__((aligned(VNZ_KERNEL_CACHE_LINE)));
volatile uint8_t *g_vnzKernelL1RequestLocks[VNZ_CORES] VNZ_KERNEL_SHARED;
volatile uint8_t *g_vnzKernelL2RequestLocks[VNZ_CORES] VNZ_KERNEL_SHARED;
volatile uint8_t *g_vnzKernelL2Lock VNZ_KERNEL_SHARED;
volatile uint32_t g_vnzKernelL2TagWalks VNZ_KERNEL_SHARED;

/* a flag per core: 1 asks it to park in local RAM, 2 while it is parked */
volatile uint8_t g_vnzKernelParkFlags[VNZ_CORES] VNZ_KERNEL_SECTION(".localdata");

/*
 * Data cache line operations.
 */
void vnzKernelCachePurge(uint32_t address, uint32_t size)
{
	uint32_t n;

	if (size > 0xed80) {
		vnzKernelCacheInvalidateTags(address, size, 0);
		return;
	}
	for (n = size >> 6; n; n--, address += VNZ_KERNEL_CACHE_LINE)
		vnzCacheLine(VNZ_CACHE_PURGE, address);
}

void vnzKernelCacheFlush(uint32_t address, uint32_t size)
{
	uint32_t n;

	for (n = size >> 6; n; n--, address += VNZ_KERNEL_CACHE_LINE)
		vnzCacheLine(VNZ_CACHE_FLUSH, address);
}

void vnzKernelCacheWriteback(uint32_t address, uint32_t size)
{
	uint32_t n;

	if (size >= 0x8000) {
		vnzKernelCacheWritebackAll();
		return;
	}
	for (n = size >> 6; n; n--, address += VNZ_KERNEL_CACHE_LINE)
		vnzCacheLine(VNZ_CACHE_WRITEBACK, address);
}

void vnzKernelCachePrefetch(uint32_t address, uint32_t size)
{
	uint32_t n;

	for (n = size >> 6; n; n--, address += VNZ_KERNEL_CACHE_LINE)
		vnzPrefetch(address);
}

void vnzKernelCacheInvalidate(uint32_t address, uint32_t size)
{
	uint32_t n;

	for (n = size >> 6; n; n--, address += VNZ_KERNEL_CACHE_LINE)
		vnzCacheLine(VNZ_CACHE_INVALIDATE, address);
}

/* the data cache's size, from the configuration register */
uint32_t vnzKernelCacheSize(void)
{
	uint32_t index = vnzReadControl(VNZ_CR_CCFG) & 0x7f;

	if (index == 0x10)
		return 0x4000;
	if (index >= 9)
		index = 9;
	return dcacheSizes[index];
}

/* the whole data cache by index (operations 1 and 3 act on the line an index selects) */
void vnzKernelCacheInvalidateAll(void)
{
	uint32_t size = vnzKernelCacheSize(), offset;

	for (offset = 0; offset < size; offset += VNZ_KERNEL_CACHE_LINE)
		vnzCacheLine(1, VNZ_DRAM + offset);
}

void vnzKernelCacheWritebackAll(void)
{
	uint32_t size = vnzKernelCacheSize(), offset;

	for (offset = 0; offset < size; offset += VNZ_KERNEL_CACHE_LINE)
		vnzCacheLine(3, VNZ_DRAM + offset);
}

/*
 * Drop the data cache's lines of [address, address + size) by their tags,
 * with the cache off; with keepDirty, written lines stay.
 */
void vnzKernelCacheInvalidateTags(uint32_t address, uint32_t size, uint32_t keepDirty)
{
	uint32_t cacheSize = (vnzReadControl(VNZ_CR_CCFG) & 0x7f) << 9;
	uint32_t indexMask = (cacheSize - 1) & ~(VNZ_KERNEL_CACHE_LINE - 1);
	uint32_t tagMask = ~(cacheSize - 1);
	uint32_t end = address + size, groups = ((cacheSize >> 6) << 1) >> 4;
	volatile uint32_t *tag = (volatile uint32_t *)VNZ_KERNEL_DCACHE_TAGS;
	uint32_t psw = vnzReadControl(VNZ_CR_PSW), cfg, value, line, group;
	int i;

	for (group = 0; group < groups; group++) {
		vnzDisableInterrupts();
		cfg = vnzReadControl(VNZ_CR_CFG);
		vnzWriteControl(VNZ_CR_CFG, cfg & ~VNZ_CFG_DCACHE);
		mep_syncm();
		for (i = 0; i < 16; i++, tag += VNZ_KERNEL_CACHE_LINE / 4) {
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
	vnzKernelCacheInvalidateTags(address, size, 1);
}

/*
 * Requests to the other cores.
 */
static void postRequests(VnzKernelCacheRequests *requests, volatile uint8_t **locks,
	uint32_t address,
	uint32_t sizeOperation, uint32_t fallbackAddress, uint32_t fallbackOperation)
{
	uint32_t self, core, read, write;
	VnzKernelCacheRequests *queue;
	VnzKernelThread *worker;
	volatile uint8_t *lock;

	vnzKernelDisableDispatch();
	self = vnzKernelCurrentCoreId() - VNZ_FIRST_CORE_ID;
	for (core = 0; core < VNZ_CORES; core++) {
		if (!(vnzKernelPowerActiveCores() & (1 << core)) || core == self)
			continue;
		queue = &requests[core];
		vnzCacheLine(VNZ_CACHE_PURGE, queue);
		worker = queue->worker;
		if (worker == NULL)
			continue;
		lock = locks[core];
		vnzKernelSpinLock(lock);
		vnzCacheLine(VNZ_CACHE_PURGE, queue);
		read = queue->read;
		write = queue->write;
		if (read + VNZ_KERNEL_REQUESTS == write) {
			/* full: one request for everything instead */
			queue->read = 0;
			queue->write = 1;
			queue->entries[0].address = fallbackAddress;
			queue->entries[0].sizeOperation = fallbackOperation;
		} else {
			queue->entries[write & (VNZ_KERNEL_REQUESTS - 1)].address = address;
			queue->entries[write & (VNZ_KERNEL_REQUESTS - 1)].sizeOperation = sizeOperation;
			queue->write = write + 1;
		}
		vnzCacheLine(VNZ_CACHE_WRITEBACK, queue);
		vnzKernelSpinUnlock(lock);
		vnzKernelThreadSetFlags(worker, 1);
	}
	vnzKernelEnableDispatch();
}

/* Purge a range from every core's data cache. */
void vnzKernelCachePurgeAllCores(uint32_t address, uint32_t size)
{
	uint32_t operation = size < 0xed80 ? VNZ_KERNEL_REQUEST_WRITEBACK : VNZ_KERNEL_REQUEST_L1_TAGS;

	postRequests(g_vnzKernelL1Requests, g_vnzKernelL1RequestLocks, address, size | operation,
		VNZ_DRAM, 0xa8000000 | VNZ_KERNEL_REQUEST_L1_TAGS);
	vnzKernelCachePurge(address, size);
}

/* Drop a range's L2 lines by their tags, here and through the other cores. */
void vnzKernelL2PurgeAllCores(uint32_t address, uint32_t size)
{
	postRequests(g_vnzKernelL2Requests, g_vnzKernelL2RequestLocks, address,
		size | VNZ_KERNEL_REQUEST_L2_TAGS,
		0, ~(VNZ_KERNEL_CACHE_LINE - 1) | VNZ_KERNEL_REQUEST_L2_TAGS);
	vnzKernelL2InvalidateTags(address, size);
}

void vnzKernelL2InvalidateTags(uint32_t address, uint32_t size)
{
	uint32_t l2Size = ((vnzReadControl(VNZ_CR_CCFG) & 0x7f0000) >> 16) << 9;
	uint32_t indexMask = (l2Size - 1) & ~(VNZ_KERNEL_CACHE_LINE - 1);
	uint32_t tagMask = ~(l2Size - 1);
	uint32_t groups = ((l2Size >> 6) << 1) >> 4, group;
	uint32_t tags = VNZ_KERNEL_L2_TAGS;
	void (*walk)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

	/* the walk runs through the public window */
	walk = (void *)VNZ_PUBLIC_OF((uint32_t)vnzKernelL2WalkTags);
	for (group = 0; group < groups; group++, tags += 0x400)
		walk(address, size, tags, tagMask, indexMask);
}

static void runRequest(uint32_t address, uint32_t sizeOperation)
{
	uint32_t size = sizeOperation & ~VNZ_KERNEL_REQUEST_MASK;

	switch (sizeOperation & VNZ_KERNEL_REQUEST_MASK) {
	case VNZ_KERNEL_REQUEST_L1_TAGS:
		cacheInvalidateCleanTags(address, size);
		return;
	case VNZ_KERNEL_REQUEST_L2_TAGS:
		vnzKernelL2InvalidateTags(address, size);
		return;
	case VNZ_KERNEL_REQUEST_WRITEBACK:
		vnzKernelCacheWriteback(address, size);
		return;
	}
}

static void runRequests(VnzKernelCacheRequests *queue, volatile uint8_t *lock)
{
	uint32_t read, address, sizeOperation, empty;

	for (;;) {
		vnzKernelDisableDispatch();
		vnzKernelSpinLock(lock);
		vnzCacheLine(VNZ_CACHE_PURGE, queue);
		read = queue->read;
		if (read != queue->write) {
			empty = 0;
			address = queue->entries[read & (VNZ_KERNEL_REQUESTS - 1)].address;
			sizeOperation = queue->entries[read & (VNZ_KERNEL_REQUESTS - 1)].sizeOperation;
			queue->read = read + 1;
			vnzCacheLine(VNZ_CACHE_WRITEBACK, queue);
		} else {
			empty = 1;
			address = 0;
			sizeOperation = 0;
			vnzCacheLine(VNZ_CACHE_PURGE, queue);
		}
		vnzKernelSpinUnlock(lock);
		vnzKernelEnableDispatch();
		if (empty)
			return;
		runRequest(address, sizeOperation);
	}
}

static void requestWorker(VnzKernelCacheRequests *queue, volatile uint8_t *lock)
{
	for (;;)
		if (vnzKernelThreadWaitFlags(1) & 1)
			runRequests(queue, lock);
}

static void requestWorkerCreate(VnzKernelCacheRequests *queue, volatile uint8_t **lock,
	uint32_t coreId)
{
	VnzKernelThread *worker;
	int i;

	worker = (VnzKernelThread *)vnzKernelThreadCreate(requestWorker, coreId, 9, 0x100,
		vnzKernelSystemHeap());
	*lock = vnzKernelLockPageAllocate();
	vnzCacheLine(VNZ_CACHE_INVALIDATE, queue);
	queue->worker = worker;
	queue->read = 0;
	queue->write = 0;
	for (i = 0; i < VNZ_KERNEL_REQUESTS; i++) {
		queue->entries[i].address = 0;
		queue->entries[i].sizeOperation = 0;
	}
	vnzCacheLine(VNZ_CACHE_WRITEBACK, queue);
	mep_syncm();
}

void vnzKernelCacheWorkersInitialize(void)
{
	uint32_t core;

	for (core = 0; core < VNZ_CORES; core++) {
		requestWorkerCreate(&g_vnzKernelL1Requests[core], &g_vnzKernelL1RequestLocks[core],
			VNZ_FIRST_CORE_ID + core);
		requestWorkerCreate(&g_vnzKernelL2Requests[core], &g_vnzKernelL2RequestLocks[core],
			VNZ_FIRST_CORE_ID + core);
	}
}

/* Start this core's two workers. */
void vnzKernelCacheStartWorkers(uint32_t coreId)
{
	uint32_t core = coreId - VNZ_FIRST_CORE_ID;
	VnzKernelCacheRequests *queue;
	volatile uint8_t *lock;
	VnzKernelThread *worker;
	int i;

	for (i = 0; i < 2; i++) {
		if (i == 0) {
			queue = &g_vnzKernelL1Requests[core];
			lock = g_vnzKernelL1RequestLocks[core];
		} else {
			queue = &g_vnzKernelL2Requests[core];
			lock = g_vnzKernelL2RequestLocks[core];
		}
		vnzCacheLine(VNZ_CACHE_PURGE, queue);
		worker = queue->worker;
		vnzKernelThreadStart(worker, (uint32_t)queue, (uint32_t)lock);
		vnzKernelThreadDetach(worker);
	}
}

/*
 * The L2.
 */

/* An operation on a range, through the controller. */
void vnzKernelL2Operation(uint32_t address, uint32_t size, uint32_t operation)
{
	volatile int i;

	if (address >= 0xc0000000 && address < 0xd8000000)
		address = vnzKernelPublicToPhysical(address);
	vnzKernelDisableDispatch();
	vnzKernelSpinLock(g_vnzKernelL2Lock);
	VNZ_KERNEL_L2_ADDRESS = address;
	VNZ_KERNEL_L2_COMMAND = size + operation;
	while (VNZ_KERNEL_L2_COMMAND & 7)
		for (i = 0; i < 50; i++)
			;
	vnzKernelSpinUnlock(g_vnzKernelL2Lock);
	vnzKernelEnableDispatch();
}

/* An operation on each 0x100-byte line of a range. */
void vnzKernelL2LineOperation(uint32_t address, uint32_t size, uint32_t operation)
{
	uint32_t line, n;

	if (address >= 0xc0000000 && address < 0xd8000000)
		address = vnzKernelPublicToPhysical(address);
	line = address;
	for (n = size >> 8; n; n--, line += 0x100)
		VNZ_KERNEL_L2_LINE_COMMAND = line + operation;
	/* the last line once more, for these */
	if (operation == VNZ_KERNEL_L2_WRITEBACK || operation == VNZ_KERNEL_L2_PURGE)
		VNZ_KERNEL_L2_LINE_COMMAND = line - 0x100 + operation;
}

/* An operation on the whole L2. */
void vnzKernelL2OperationAll(uint32_t operation)
{
	uint32_t lock = (uint32_t)g_vnzKernelL2Lock;

	vnzKernelL2Operation(VNZ_DRAM, 0xa8000000, operation);
	if (lock < VNZ_LOCAL_RAM || lock >= VNZ_LOCAL_RAM + 0x2000)
		vnzKernelL2LineOperation(lock & ~0xff, 0x100, VNZ_KERNEL_L2_PURGE);
}

void vnzKernelL2SetMode(uint32_t mode)
{
	vnzKernelDisableDispatch();
	vnzKernelSpinLock(g_vnzKernelL2Lock);
	if (VNZ_KERNEL_L2_MODE != mode)
		vnzKernelLocalL2SetMode(mode);
	vnzKernelSpinUnlock(g_vnzKernelL2Lock);
	vnzKernelEnableDispatch();
}

/* Turn the L2 off, the other cores parked meanwhile. */
void vnzKernelL2Disable(void)
{
	uint32_t self, core;
	volatile uint32_t delay;

	if (VNZ_KERNEL_L2_ENABLE == 0)
		return;
	vnzKernelDisableDispatch();
	vnzKernelSpinLock(g_vnzKernelL2Lock);
	self = vnzKernelCurrentCoreId() - VNZ_FIRST_CORE_ID;
	for (core = 0; core < VNZ_CORES; core++) {
		if (!(vnzKernelPowerActiveCores() & (1 << core)) || core == self)
			continue;
		g_vnzKernelParkFlags[core] = 1;
		vnzKernelThreadSetFlags(g_vnzKernelParkWorkers[core], 2);
	}
restart:
	for (core = 0; core < VNZ_CORES; core++) {
		if (!(vnzKernelPowerActiveCores() & (1 << core)) || core == self)
			continue;
		if (g_vnzKernelParkFlags[core] != 2) {
			for (delay = 0; delay < 0xff; delay++)
				;
			goto restart;
		}
	}
	vnzKernelLocalL2Disable();
	for (core = 0; core < VNZ_CORES; core++) {
		if (!(vnzKernelPowerActiveCores() & (1 << core)) || core == self)
			continue;
		g_vnzKernelParkFlags[core] = 0;
	}
	vnzKernelSpinUnlock(g_vnzKernelL2Lock);
	vnzKernelEnableDispatch();
}

void vnzKernelL2Enable(void)
{
	VNZ_KERNEL_L2_ENABLE = 1;
}

/* a core's worker parks it in local RAM when asked */
static void parkWorker(void)
{
	uint32_t core;

	vnzKernelDisableDispatch();
	core = vnzKernelCurrentCoreId() - VNZ_FIRST_CORE_ID;
	for (;;) {
		vnzKernelThreadWaitFlags(2);
		vnzKernelLocalPark(&g_vnzKernelParkFlags[core]);
	}
}

static void parkWorkersCreate(void)
{
	uint32_t core;

	vnzKernelCacheInvalidate((uint32_t)g_vnzKernelParkWorkers, sizeof(g_vnzKernelParkWorkers));
	for (core = 0; core < VNZ_CORES; core++)
		g_vnzKernelParkWorkers[core] = (VnzKernelThread *)vnzKernelThreadCreate(parkWorker,
			VNZ_FIRST_CORE_ID + core, 9, 0x100, vnzKernelSystemHeap());
	vnzKernelCacheWriteback((uint32_t)g_vnzKernelParkWorkers, sizeof(g_vnzKernelParkWorkers));
	mep_syncm();
}

void vnzKernelLocalCodeStartWorker(uint32_t coreId)
{
	VnzKernelThread *worker = g_vnzKernelParkWorkers[coreId - VNZ_FIRST_CORE_ID];

	vnzKernelThreadStart(worker, 0, 0);
	vnzKernelThreadDetach(worker);
}

void vnzKernelL2Initialize(void)
{
	int i;

	g_vnzKernelL2Lock = vnzKernelLockPageAllocate();
	for (i = 0; i < VNZ_CORES; i++)
		g_vnzKernelParkFlags[i] = 0;
	parkWorkersCreate();
}

/* the address if it is in DRAM's physical window, otherwise 0 */
uint32_t vnzKernelDramAddress(uint32_t address)
{
	if (address >= VNZ_DRAM && address < VNZ_DRAM + 0xa8000000)
		return address;
	return 0;
}

/*
 * At boot, before the caches are used: clear the L2's tags and turn it on in
 * the configuration register.
 */
void vnzKernelCacheBootConfigure(void)
{
	uint32_t ccfg = vnzReadControl(VNZ_CR_CCFG), size, bits;
	volatile uint32_t *tag = (volatile uint32_t *)VNZ_KERNEL_L2_TAGS;

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
	for (; size >= VNZ_KERNEL_CACHE_LINE; size -= VNZ_KERNEL_CACHE_LINE,
		tag += VNZ_KERNEL_CACHE_LINE / 4)
		*tag = 0;
	vnzWriteControl(VNZ_CR_CFG, (vnzReadControl(VNZ_CR_CFG) & ~0x402) | bits);
}

void vnzKernelL2TagWalkMark(void)
{
	g_vnzKernelL2TagWalks = 0;
}

/* One group of 16 L2 tags, run through the public window with the L2 off. */
void vnzKernelL2WalkTags(uint32_t address, uint32_t size, uint32_t tags, uint32_t tagMask,
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
	mark = (void *)VNZ_PUBLIC_OF((uint32_t)vnzKernelL2TagWalkMark);
	mark();
	for (i = 0; i < 16; i++, tag += VNZ_KERNEL_CACHE_LINE / 4) {
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
#define VNZ_KERNEL_LOCAL_CODE	VNZ_KERNEL_SECTION(".localcode") __attribute__((noinline))

VNZ_KERNEL_LOCAL_CODE void vnzKernelLocalL2SetMode(uint32_t mode)
{
	mep_di();
	mep_syncm();
	VNZ_KERNEL_L2_STOP = 1;
	while (VNZ_KERNEL_L2_BUSY)
		;
	if (mode) {
		VNZ_KERNEL_L2_ADDRESS = VNZ_DRAM;
		VNZ_KERNEL_L2_COMMAND = VNZ_KERNEL_L2_FLUSH;
		while (VNZ_KERNEL_L2_COMMAND & 7)
			;
	}
	VNZ_KERNEL_L2_MODE = mode;
	VNZ_KERNEL_L2_STOP = 0;
	mep_ei();
}

VNZ_KERNEL_LOCAL_CODE void vnzKernelLocalPark(volatile uint8_t *flag)
{
	if (*flag == 0)
		return;
	mep_di();
	*flag = 2;
	while (*flag)
		;
	mep_ei();
}

VNZ_KERNEL_LOCAL_CODE void vnzKernelLocalL2Disable(void)
{
	mep_di();
	mep_syncm();
	while (VNZ_KERNEL_L2_BUSY)
		;
	VNZ_KERNEL_L2_ADDRESS = VNZ_DRAM;
	VNZ_KERNEL_L2_COMMAND = VNZ_KERNEL_L2_PURGE;
	while (VNZ_KERNEL_L2_COMMAND & 7)
		;
	VNZ_KERNEL_L2_ENABLE = 0;
	mep_ei();
}
