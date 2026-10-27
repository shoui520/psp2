/*
 * intrmgr.c, secure-world build: the GIC as the secure world sees it, the
 * monitor call table, and the monitor calls this module serves itself (shared
 * memory, the kernel boot area, the L2 cache controller). The same source as
 * os0's intrmgr.c; the secure build drops sub-interrupts and the heap.
 */

#include "intrmgr.h"
#include <sysmem/pl310_regs.h>
#include <threadmgr/threadcb.h>
#include <sysmem/cp15.h>
#include <sysmem/uid_guid.h>

#define INTR_MONCALL_SET_SHARED_MEMORY	0x101	/* SCE_MONCALL_SET_SHARED_MEMORY */
#define INTR_MONCALL_UNMAP_KBL		0x103	/* SCE_MONCALL_UNMAP_KBL */
#define INTR_MONCALL_L2CC_FLUSH		0x10f
#define INTR_MONCALL_L2CC_CONTROL	0x16a
#define INTR_MONCALL_L2CC_AUX_CONTROL	0x16b

#define MONCALL_FIRST			0x100
#define MONCALL_LAST			0x500
#define MONCALL_TABLE_SIZE		0x1000

#if !defined(INTRMGR_SKBL)
IntrState g_intrState;
#endif
volatile GicCpuInterface *volatile g_intrCpuInterface;
GicDistributor *volatile g_intrDistributor;
SceKernelMonitorCallHandler *g_moncallTable;
SceUID g_moncallTableUid;
void *g_intrEnterSyscall;
void *g_intrLeaveSyscall;
void *g_intrEnterIntr;
void *g_intrLeaveIntr;
void *g_intrEnterSubIntr;
void *g_intrLeaveSubIntr;

#if !defined(INTRMGR_SKBL)
/* name unknown: each CPU's stack for monitor calls, CPU 0's on top; it ends
 * where g_smcHandlerTable starts, and intrmgr_entry.S takes that as its top */
char g_monitorStack[4][0x1000];
/* name unknown: the SMC fast calls below 0x100; nothing in the module fills it */
void *g_smcHandlerTable[0x100];
#endif

/* name unknown: the priority as the secure world writes it; a Non-secure
 * (group 1) interrupt's sits in the lower half of the range */
void SetPriority(unsigned int intrCode, unsigned char priority)
{
	if (g_intrDistributor->group[intrCode >> 5] & (1 << (intrCode & 31)))
		g_intrDistributor->priority[intrCode] = (priority >> 1) | 0x80;
	else
		g_intrDistributor->priority[intrCode] = priority >> 1;
}

#if !defined(INTRMGR_SKBL)
static int UnmapKblArea(int arg0, int arg1, int arg2, int arg3)
{
	if (sceKernelAddressSpaceUnmap(SCE_KERNEL_ADDRESS_SPACE_ID, 0xf7, 0xc,
					     (void *)0x40000000, 0x100000) < 0)
		SCE_DBG_ASSERT_FAIL(1673);
	if (sceKernelAddressSpaceUnmap(SCE_KERNEL_ADDRESS_SPACE_ID, 0xf7, 0xc,
					     (void *)0x50000000, 0x1000000) < 0)
		SCE_DBG_ASSERT_FAIL(1684);
	if (sceKernelAddressSpaceUnmap(SCE_KERNEL_ADDRESS_SPACE_ID, 0xf7, 0xc,
					     (void *)0x51000000, 0x1000000) < 0)
		SCE_DBG_ASSERT_FAIL(1694);
	return 0;
}

/* paddr, size: the kernel's shared memory block, mapped here as well */
static int SetSharedMemory_S(int arg0, int arg1, int arg2, int arg3)
{
	SceUInt32 paddr = arg0;
	SceSize size = arg1;
	SceKernelAllocMemBlockKernelOpt opt;
	SceUID memBlockId;
	void *base;

	if (sceKernelSysrootGetSharedMemory())
		return SCE_KERNEL_ERROR_ERROR;
	if (size >= 0x100000)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (sceKernelCheckDipsw(SCE_DIPSW_ENABLE_TOOL_PHYMEMPART)) {
		if (paddr <= 0x401fffff || (int)(paddr + size) < 0)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	} else {
		if (paddr <= 0x401fffff || paddr + size >= 0x60000000)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	opt.paddr = paddr;
	memBlockId = sceKernelAllocPartitionMemBlock(0x10009, "SceSharedMemoryBlock_S", 0x1020d006,
						     size, &opt);
	if (memBlockId < 0) {
		SCE_DBG_PRINTF(0xb, 1643, "allocate shared memory block failed. 0x%08x\n", memBlockId);
		SCE_DBG_ASSERT_FAIL(1644);
		return memBlockId;
	}
	sceKernelGetMemBlockBase(memBlockId, &base);
	sceKernelSysrootSetSharedMemory(base);
	return 0;
}

static int L2ccFlush(int arg0, int arg1, int arg2, int arg3)
{
	sceKernelPleFlushRequest();
	sceKernelL1DcacheCleanInvalidateAll();
	return 0;
}

/* map the PL310 for one write: offset into its registers */
static int SetL2ccReg(SceSize offset, SceUInt32 val)
{
	SceKernelAllocMemBlockKernelOpt opt;
	SceUID memBlockId;
	char *base;

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	opt.paddr = PL310_REG_BASE;
	memBlockId = sceKernelAllocMemBlockForKernel("SceL2Cache", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_SO_RW,
					     0x1000, &opt);
	if (memBlockId < 0) {
		SCE_DBG_PRINTF(0xb, 1798, "remap l2 cache area failed. 0x%08x\n", memBlockId);
		return memBlockId;
	}
	sceKernelGetMemBlockBase(memBlockId, (void **)&base);
	SCE_DBG_PRINTF(0xb, 1803, "base: %p, addr 0x%08x, val 0x%08x\n", base, base + offset, val);
	*(volatile SceUInt32 *)(base + offset) = val;
	sceKernelFreeMemBlock(memBlockId);
	return 0;
}

static int SetL2ccAuxControl(int arg0, int arg1, int arg2, int arg3)
{
	return SetL2ccReg(offsetof(ScePl310Registers, auxControl), arg0);
}

static int SetL2ccControl(int arg0, int arg1, int arg2, int arg3)
{
	CpuL2CleanInvalidateAll();
	CpuL2CacheSyncCore();
	return SetL2ccReg(offsetof(ScePl310Registers, control), arg0);
}
#endif

/* name unknown: interrupt context without a thread context */
#if defined(INTRMGR_SKBL)
static int IllegalContext(void)
#else
int IllegalContext(void)
#endif
{
	if (getCurrentThreadCB())
		return 0;
	return g_intrCpu[readMpidr() & 3].nesting;
}

int sceKernelRegisterIntrHandler(unsigned int intrCode, const char *name, unsigned int type,
				 void *handler, void *arg, unsigned int priority,
				 unsigned int target, const SceKernelIntrRegisterOpt *opt)
{
	IntrHandler *info;
	unsigned int mask;
	unsigned char config = 1;
	SceKernelIntrStatus state;
	int result;

	if (IllegalContext()) {
		result = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto out;
	}
	if (intrCode >= INTR_COUNT) {
		result = SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
		SCE_DBG_PRINTF(0xb, 390, "Unknown interrupt code. %d\n", intrCode);
		goto out;
	}
	if (type & ~0x11U) {
		result = SCE_KERNEL_ERROR_ILLEGAL_INTRTYPE;
		SCE_DBG_PRINTF(0xb, 395, "Unknown type. %d\n", type);
		goto out;
	}
	if (!handler) {
		result = SCE_KERNEL_ERROR_ILLEGAL_HANDLER;
		SCE_DBG_PRINTF(0xb, 399, "invalid handler address. %p\n", handler);
		goto out;
	}
	result = priority & ~0xf0U;
	if (result) {
		result = SCE_KERNEL_ERROR_ILLEGAL_INTRPRIORITY;
		SCE_DBG_PRINTF(0xb, 415, "invalid priority. %d\n", priority);
		goto out;
	}
	if (TargetMask(target, &mask)) {
		result = SCE_KERNEL_ERROR_ILLEGAL_TARGET_CPU;
		SCE_DBG_PRINTF(0xb, 420, "invalid target cpu. %d\n", target);
		goto out;
	}
	if (opt) {
		if (opt->size != sizeof(*opt)) {
			result = SCE_KERNEL_ERROR_ILLEGAL_INTRPARAM;
			goto out;
		}
		if (opt->attr & SCE_KERNEL_INTR_OPT_HAS_CONFIG) {
			if ((opt->config & ~2U) != SCE_KERNEL_INTR_CONFIG_LEVEL) {	/* level or edge */
				result = SCE_KERNEL_ERROR_ILLEGAL_INTRPARAM;
				goto out;
			}
			config = opt->config;
			SetConfiguration(intrCode, opt->config);
		}
	}
	info = &g_intrHandlers[intrCode];
	state = sceKernelSpinlockLowLockCpuSuspendIntr(&info->lock);
	if (info->handler) {
		result = SCE_KERNEL_ERROR_FOUND_HANDLER;
		SCE_DBG_PRINTF(0xb, 452, "already registered: %d %s %p\n", intrCode, name,
			       info->handler);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		goto out;
	}
	if (name)
		sceKernelNameHeapInsert(name, 0, &info->name);
	else
		info->name = NULL;
	info->handler = handler;
	info->intrCode = intrCode;
	info->priority = priority;
	info->type = type;
	info->arg = arg;
	memset(info->count, 0, sizeof(info->count));
	info->oldHandler = NULL;
	info->sub = NULL;
	info->callbacks = NULL;
	info->subCount = 0;
	if ((int)intrCode > 15) {
		SetPriority(intrCode, priority);
		g_intrDistributor->target[intrCode] = mask;
	}
	info->targetCpu = g_intrDistributor->target[intrCode];
	info->configuration = config;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
out:
	return result;
}

#if !defined(INTRMGR_SKBL)
int sceKernelReleaseIntrHandler(unsigned int intrCode)
{
	IntrHandler *info;
	SceKernelIntrStatus state;
	char *name;

	if (IllegalContext())
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	if (intrCode >= INTR_COUNT) {
		SCE_DBG_PRINTF(0xb, 533, "Unknown interrupt code. %d\n", intrCode);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	info = &g_intrHandlers[intrCode];
	state = sceKernelSpinlockLowLockCpuSuspendIntr(&info->lock);
	if (!info->handler) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		SCE_DBG_PRINTF(0xb, 543, "interrupt handler is not registered. %d\n", intrCode);
		return SCE_KERNEL_ERROR_NOTFOUND_HANDLER;
	}
	if (intrCode > 15 && info->priority > 63)
		g_intrDistributor->clearEnable[intrCode >> 5] = 1U << (intrCode & 31);
	name = info->name;
	info->handler = NULL;
	info->oldHandler = NULL;
	info->intrCode = 0;
	info->priority = 0;
	info->type = 0;
	info->arg = NULL;
	info->targetCpu = 0;
	info->configuration = 0;
	sceKernelNameHeapDelete(name, 0);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
	return 0;
}
#endif

int sceKernelEnableIntr(unsigned int intrCode)
{
	if (intrCode - 16 >= 240) {
		SCE_DBG_PRINTF(0xb, 670, "Illegal interrupt code. %d\n", intrCode);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	if (!g_intrHandlers[intrCode].handler) {
		SCE_DBG_PRINTF(0xb, 674, "interrupt handler is not registered. intrcode %d\n",
			       intrCode);
		return SCE_KERNEL_ERROR_NOTFOUND_HANDLER;
	}
	g_intrDistributor->setEnable[intrCode >> 5] = 1U << (intrCode & 31);
	return 0;
}

#if !defined(INTRMGR_SKBL)
int sceKernelDisableIntr(unsigned int intrCode)
{
	if (intrCode - 16 >= 240) {
		SCE_DBG_PRINTF(0xb, 687, "Illegal interrupt code. %d\n", intrCode);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	if (!g_intrHandlers[intrCode].handler) {
		SCE_DBG_PRINTF(0xb, 691, "interrupt handler is not registered\n");
		return SCE_KERNEL_ERROR_NOTFOUND_HANDLER;
	}
	g_intrDistributor->clearEnable[intrCode >> 5] = 1U << (intrCode & 31);
	return 0;
}
#endif

int sceKernelSetIntrTargetCpu(unsigned int intrCode, unsigned int target)
{
	IntrHandler *info;
	unsigned int mask;
	SceKernelIntrStatus state;

	if (intrCode >= INTR_COUNT) {
		SCE_DBG_PRINTF(0xb, 833, "Unknown interrupt code. %d\n", intrCode);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	info = &g_intrHandlers[intrCode];
	if (!info->handler) {
		SCE_DBG_PRINTF(0xb, 838, "interrupt handler is not registered\n");
		return SCE_KERNEL_ERROR_NOTFOUND_HANDLER;
	}
	if (TargetMask(target, &mask)) {
		SCE_DBG_PRINTF(0xb, 843, "invalid target cpu. %d\n", target);
		return SCE_KERNEL_ERROR_ILLEGAL_TARGET_CPU;
	}
	state = sceKernelSpinlockLowLockCpuSuspendIntr(&info->lock);
	info->targetCpu = mask;
	g_intrDistributor->target[intrCode] = mask;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
	return 0;
}

#if !defined(INTRMGR_SKBL)
int sceKernelGenerateSoftIntr(unsigned int intrCode, unsigned int filter, unsigned int target)
{
	unsigned int mask, value;

	if (intrCode > 15) {
		SCE_DBG_PRINTF(0xb, 885, "Unknown interrupt code. %d\n", intrCode);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	if (filter > 2) {
		SCE_DBG_PRINTF(0xb, 891, "Unknown filter. %d\n", filter);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRFILTER;
	}
	if (filter) {
		value = intrCode | filter << 24;
	} else {
		if (TargetMask(target, &mask)) {
			SCE_DBG_PRINTF(0xb, 897, "Unknown target. 0x%x\n", target);
			return SCE_KERNEL_ERROR_ILLEGAL_TARGET_CPU;
		}
		value = intrCode | mask << 16;
	}
	/* NSATT: a Non-secure (group 1) software interrupt */
	if (g_intrDistributor->group[0] & (1 << intrCode))
		value |= 0x8000;
	g_intrDistributor->softIntr = value;
	return 0;
}

int sceKernelRegisterMonitorCallHandler(unsigned int callId, SceKernelMonitorCallHandler handler)
{
	if (callId - MONCALL_FIRST > MONCALL_LAST - MONCALL_FIRST)
		SCE_DBG_ASSERT(0, 1409);
	if (g_moncallTable[callId - MONCALL_FIRST]) {
		SCE_DBG_PRINTF(0xb, 1414, "already registered. monitor call number %d\n", callId);
		return -1;
	}
	g_moncallTable[callId - MONCALL_FIRST] = handler;
	return 0;
}

static inline int sceKernelAllocMonitorCallTable(void)
{
	SceUID memBlockId;
	void *base;

	if (g_moncallTableUid || g_moncallTable)
		return -1;
	memBlockId = sceKernelAllocPartitionMemBlock(0x10009, "SceMoncallTable", 0x1020d006,
						     MONCALL_TABLE_SIZE, NULL);
	if (memBlockId < 0) {
		SCE_DBG_PRINTF(6, 1382, "allocate monitor call table failed. 0x%08x\n", memBlockId);
		return memBlockId;
	}
	g_moncallTableUid = memBlockId;
	sceKernelGetMemBlockBase(memBlockId, &base);
	g_moncallTable = base;
	memset(base, 0, MONCALL_TABLE_SIZE);
	return 0;
}
#endif

static inline int Start(void)
{
	int ret, i;

#if !defined(INTRMGR_SKBL)
	ret = sceKernelAllocMonitorCallTable();
	if (ret < 0) {
		SCE_DBG_PRINTF(6, 1957, "alloc monitor call table. 0x%08x\n", ret);
		SCE_DBG_ASSERT_FAIL(1958);
	}
#endif
	ret = sceKernelRegisterMonitorEntry(2, &monitorSmcEntry);
	if (ret < 0) {
		SCE_DBG_PRINTF(6, 1965, "register smc entry handler failed. 0x%08x\n", ret);
		SCE_DBG_ASSERT_FAIL(1966);
	}
	ret = sceKernelRegisterMonitorEntry(7, &monitorFiqEntry);
	if (ret < 0) {
		SCE_DBG_PRINTF(6, 1972, "register Monitor FIQ entry handler failed. 0x%08x\n", ret);
		SCE_DBG_ASSERT_FAIL(1973);
	}
#if !defined(INTRMGR_SKBL)
	ret = sceKernelRegisterMonitorCallHandler(INTR_MONCALL_SET_SHARED_MEMORY, SetSharedMemory_S);
	if (ret < 0) {
		SCE_DBG_PRINTF(6, 1980, "Register SCE_MONCALL_SET_SHARED_MEMORY failed. 0x%08x\n",
			       ret);
		SCE_DBG_ASSERT_FAIL(1981);
	}
	ret = sceKernelRegisterMonitorCallHandler(INTR_MONCALL_UNMAP_KBL, UnmapKblArea);
	if (ret < 0) {
		SCE_DBG_PRINTF(6, 1987, "Register SCE_MONCALL_UNMAP_KBL failed. 0x%08x\n", ret);
		SCE_DBG_ASSERT_FAIL(1988);
	}
#endif
	for (i = 0; i < INTR_COUNT; i++) {
		g_intrHandlers[i].handler = NULL;
		g_intrHandlers[i].lock = 0;
	}
	return 0;
}

#if defined(INTRMGR_SKBL)
/* names unknown: each SGI's group (1: Non-secure) and priority */
typedef struct IntrSgiSetting {
	unsigned char group;
	unsigned char priority;
} IntrSgiSetting;

/* names unknown: the group of every interrupt, 1 for Non-secure; the SGIs'
 * come from s_sgiSetting and the PPIs' are all Non-secure */
static const signed char s_intrGroup[INTR_COUNT / 32][32] = {
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1,
	  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
	  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1,
	  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
};

static const IntrSgiSetting s_sgiSetting[16] = {
	{ 1, 0x10 }, { 1, 0x20 }, { 1, 0x30 }, { 1, 0x80 },
	{ 1, 0x80 }, { 1, 0x80 }, { 1, 0x40 }, { 1, 0x50 },
	{ 1, 0x80 }, { 1, 0x80 }, { 1, 0x80 }, { 0, 0x00 },
	{ 1, 0x80 }, { 1, 0x70 }, { 1, 0x80 }, { 1, 0xd0 },
};

/* SKBL: the GIC at its physical address, the groups and SGI priorities set
 * here, and no monitor calls of its own */
int sceKernelIntrMgrModuleStart(SceSize arglen, void *argp)
{
	volatile GicCpuInterface *cpuInterface;
	unsigned int blocks, mask;
	int i, j;

	sceKernelSysrootCorelockLock(0);
	cpuInterface = (GicCpuInterface *)(PERIPH_REG_BASE + GIC_CPU_INTERFACE_OFFSET);
	g_intrCpuInterface = cpuInterface;
	g_intrDistributor = (GicDistributor *)(PERIPH_REG_BASE + GIC_DISTRIBUTOR_OFFSET);

	cpuInterface->control = 0;
	if (!sceKernelCpuId())
		g_intrDistributor->control = 0;
	blocks = (g_intrDistributor->type & 31) + 1;
	i = 0;
	do {
		g_intrDistributor->clearEnable[i] = ~0U;
		i++;
	} while (i != blocks);

	/* the PPIs are Non-secure */
	mask = 0;
	for (i = 0; i < 16; i++)
		if (s_sgiSetting[i].group == 1)
			mask |= 1 << i;
	g_intrDistributor->group[0] = mask | 0xffff0000;
	for (i = 1; i < blocks; i++) {
		mask = 0;
		for (j = 0; j < 32; j++)
			if (s_intrGroup[i][j] == 1)
				mask |= 1 << j;
		g_intrDistributor->group[i] = mask;
	}
	for (i = 0; i < 16; i++)
		SetPriority(i, s_sgiSetting[i].priority);

	g_intrCpuInterface->priorityMask = 0xf8;
	g_intrCpuInterface->binaryPoint = 2;
	g_intrCpuInterface->aliasedBinaryPoint = 3;
	g_intrCpuInterface->control = 9;	/* enable, FIQ for secure interrupts */
	if (!sceKernelCpuId())
		g_intrDistributor->control = 1;

	g_intrCpu[sceKernelCpuId()].nesting = 0;
	if (!sceKernelCpuId())
		Start();
	sceKernelSysrootSetModulePrivate(5, g_intrHandlers);
	g_intrEnterSyscall = NULL;
	g_intrLeaveSyscall = NULL;
	g_intrEnterIntr = NULL;
	g_intrLeaveIntr = NULL;
	g_intrEnterSubIntr = NULL;
	g_intrLeaveSubIntr = NULL;
	InitUsleepTimer(0);
	sceKernelSysrootCorelockUnlock();
	enableInterrupts();
	return 0;
}
#else
static inline int RegisterMoncallHandler(void)
{
	int ret;

	ret = sceKernelRegisterMonitorCallHandler(INTR_MONCALL_L2CC_FLUSH, L2ccFlush);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 1833, "Error:sceKernelRegisterMonitorCallHandler failed 0x%08x\n",
			       ret);
		return ret;
	}
	ret = sceKernelRegisterMonitorCallHandler(INTR_MONCALL_L2CC_CONTROL, SetL2ccControl);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 1840, "Error:sceKernelRegisterMonitorCallHandler failed 0x%08x\n",
			       ret);
		return ret;
	}
	ret = sceKernelRegisterMonitorCallHandler(INTR_MONCALL_L2CC_AUX_CONTROL, SetL2ccAuxControl);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 1847, "Error:sceKernelRegisterMonitorCallHandler failed 0x%08x\n",
			       ret);
		return ret;
	}
	return 0;
}

int sceKernelIntrMgrModuleStart(SceSize arglen, void *argp)
{
	SceKernelAllocMemBlockKernelOpt opt;
	unsigned int blocks, i;
	volatile GicCpuInterface *cpuInterface;
	void *base;
	int ret;

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	opt.paddr = PERIPH_REG_BASE;
	ret = sceKernelAllocMemBlockForKernel("SceInterruptControllerReg",
				      SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_SO_RW, PERIPH_REG_SIZE, &opt);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 2250, "allocate intc register area failed. 0x%08x\n", ret);
		SCE_DBG_ASSERT_FAIL(2251);
		return ret;
	}
	sceKernelGetMemBlockBase(ret, &base);
	cpuInterface = (GicCpuInterface *)((char *)base + GIC_CPU_INTERFACE_OFFSET);
	g_intrCpuInterface = cpuInterface;
	g_intrDistributor = (GicDistributor *)((char *)base + GIC_DISTRIBUTOR_OFFSET);

	/* reset the GIC: the secure world takes its interrupts as FIQs */
	cpuInterface->control = 0;
	if (!sceKernelCpuId())
		g_intrDistributor->control = 0;
	blocks = (g_intrDistributor->type & 31) + 1;
	i = 0;
	do {
		g_intrDistributor->clearEnable[i] = ~0U;
		i++;
	} while (i != blocks);
	g_intrCpuInterface->control = 9;	/* enable, FIQ for secure interrupts */
	if (!sceKernelCpuId())
		g_intrDistributor->control = 1;

	g_intrCpu[sceKernelCpuId()].nesting = 0;
	if (!sceKernelCpuId())
		Start();
	sceKernelSysrootSetModulePrivate(5, g_intrHandlers);
	g_intrEnterSyscall = NULL;
	g_intrLeaveSyscall = NULL;
	g_intrEnterIntr = NULL;
	g_intrLeaveIntr = NULL;
	g_intrEnterSubIntr = NULL;
	g_intrLeaveSubIntr = NULL;
	InitUsleepTimer(1);
	enableInterrupts();
	ret = RegisterAccessModeMonitorCall();
	if (ret < 0) {
		SCE_DBG_ASSERT(0, 2275);
		return ret;
	}
	ret = RegisterMoncallHandler();
	if (ret < 0) {
		SCE_DBG_ASSERT(0, 2281);
		return ret;
	}
	initDmacSecure();
	return 0;
}

int module_bootstart(SceSize arglen, void *argp)
{
	return 0;
}
#endif
