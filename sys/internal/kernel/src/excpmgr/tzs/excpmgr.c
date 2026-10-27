/* ARM and secure-monitor exception vectors of the secure kernel. */
#include "excpmgr.h"

#if defined(EXCPMGR_SKBL)
/* GCC emits zero-initialized definitions in reverse declaration order;
 * g_excpData and the mode stack tables sit in excpmgr_vector.S. */
ExcpHandlerTable g_excpHandlers = { 0 };
unsigned char g_excpStack[4][EXCP_STACK_SIZE] __attribute__((aligned(32))) = { { 0 } };
SceExcpmgrCallback g_undefCallback;
SceExcpmgrCallback g_pabtCallback;
SceExcpmgrCallback g_dabtCallback;
#else
SceExcpmgrData g_excpData;
void *g_irqStack[4];
void *g_svcStack[4];
void *g_abortStack[4];
void *g_undefStack[4];
SceExcpmgrCallback g_undefCallback;	/* os0 callbacks; never set in this build */
SceExcpmgrCallback g_pabtCallback;
SceExcpmgrCallback g_dabtCallback;
unsigned char g_excpStack[4][EXCP_STACK_SIZE];
ExcpHandlerTable g_excpHandlers;
#endif

static void _sceKernelPanicHandlerCommon(SceExcpmgrContext *context, unsigned int reason,
					 const SceKernelDebugInfo *info, const char *message)
	__attribute__((noreturn));

/* registration, default/panic handlers and register report, shared with os0 */
#include "../excpmgr_handler.h"

/*
 * name unknown: copy code into a read-only page (the vector templates) and
 * make it visible to instruction fetch.
 */
static int excpWriteText(void *dst, const void *src, SceSize size)
{
	unsigned int dacr = readDacr();
	unsigned int start;

	writeDacr(DACR_ALL_MANAGER);
	start = (unsigned int)memcpy(dst, src, size);
	sceKernelDcacheCleanRangeForL1WBWA((void *)cacheLineDown(start),
		cacheLineDown(start + size + 31) - cacheLineDown(start));
	writeDacr(dacr);
	sceKernelL1IcacheInvalidateEntireAllCore();
	return 0;
}

static void _sceKernelPanicHandlerCommon(SceExcpmgrContext *context, unsigned int reason,
					 const SceKernelDebugInfo *info, const char *message)
{
	SCE_PRINTF_WITH_INFO(0, 1177, "*** Kernel %s Exception at CPU%d <0x%08x%08x:0x%08x>\n",
		message, readMpidr() & 3, info->fileHash, info->lineHash, info->funcHash);
#if FW_DEBUG
	printExcpSavedRegisters(context);
	if (reason == 4)
		printIFSR(context);
	else if (reason == 1)
		printDFSR(context);
#endif
	for (;;)
		;
}

#if !defined(EXCPMGR_SKBL)
void sceKernelDefaultHandlerFiqCfunc(void)
{
	SCE_DBG_PRINTF(0xb, 550, "()\n");
	sceKernelDefaultHandlerCfunc();
}
#endif

int sceKernelRegisterMonitorEntry(unsigned int code, SceExcpmgrHandler *handler)
{
	unsigned int entry;
	int flags;

	if (handler == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (sceKernelIsColdBoot()) {
		if (handler->next != NULL || handler->reserved != 0) {
			SCE_DBG_ASSERT_FAIL(1433);
			return SCE_KERNEL_ERROR_ILLEGAL_EXCPHANDLER;
		}
	}
	if (code > 7) {
		SCE_DBG_ASSERT_FAIL(1439);
		return SCE_KERNEL_ERROR_ILLEGAL_EXCPCODE;
	}

	flags = sceKernelSpinlockLowLockCpuSuspendIntr(&g_excpHandlers.lock);
	if (g_excpHandlers.monitorFirst == NULL)
		g_excpHandlers.monitorFirst = handler;
	excpPokeWord((unsigned int)handler, (unsigned int)g_excpHandlers.monitorHead);
	g_excpHandlers.monitorHead = handler;
	/* the monitor vectors load their target from the slot table at +0x20 */
	entry = (unsigned int)handler + 8;
	excpPokeVector(&g_excpHandlers.monitorVectors[8 + code], &entry);
	sceKernelL1DcacheCleanRange(g_excpHandlers.monitorVectors, g_excpHandlers.monitorVectorSize);
	sceKernelL1IcacheInvalidateRange(g_excpHandlers.monitorVectors, g_excpHandlers.monitorVectorSize);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_excpHandlers.lock, flags);
	return 0;
}

void sceKernelDefaultMonitorHandlerCfunc(void)
{
	SCE_DBG_PRINTF(0xb, 1462, "()\n");
	for (;;)
		;
}

void sceKernelDefaultHandlerSmcCfunc(void)
{
	SCE_DBG_PRINTF(0xb, 1469, "()\n");
	for (;;)
		;
}

#if defined(EXCPMGR_SKBL)
static __attribute__((noinline)) int Init(SceSize args, void *argp)
#else
int module_start(SceSize args, void *argp)
#endif
{
	unsigned int code, priority, cpu, value;
	unsigned char *stack;

	g_excpHandlers.lock = 0;
	g_excpHandlers.vectors = sceKernelSysrootGetVbaseResetVector();
	g_excpHandlers.vectorSize = (unsigned int)monitorVectors - (unsigned int)exceptionVectors;
	excpWriteText(g_excpHandlers.vectors, exceptionVectors, g_excpHandlers.vectorSize);
	g_excpHandlers.defaultHandler = NULL;
	for (code = 0; code < SCE_EXCP_MAX; code++) {
		g_excpHandlers.head[code] = NULL;
		for (priority = 0; priority < SCE_EXCP_PRIORITY_MAX; priority++)
			g_excpHandlers.first[code][priority] = NULL;
	}

	sceKernelRegisterDefaultExceptionHandler(&defaultHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_RESET, 7, &resetHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_SVC, 7, &svcHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_RESERVED, 7, &reservedHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_IRQ, 7, &irqHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_FIQ, 7, &fiqHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_UNDEF, 7, &panicUndefHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_PABT, 7, &panicPabtHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_DABT, 7, &panicDabtHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_UNDEF, 0, &initialUndefHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_PABT, 0, &initialPabtHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_DABT, 0, &initialDabtHandler);

	/* word 0x20 of each core's vector bank: the sysroot per-core block */
	for (cpu = 0; cpu < 4; cpu++) {
#if defined(EXCPMGR_SKBL)
		value = (unsigned int)SceSysrootForKernel_06182D59(cpu);
#else
		value = SceSysrootForTZS_317E7556(cpu);
#endif
		excpPokeVector(&g_excpHandlers.vectors[cpu * 16 + 8], &value);
	}

	stack = g_excpStack[0];
#if defined(EXCPMGR_SKBL)
	cpu = 0;
	for (;;) {
		unsigned int *entry;

		g_excpData.nestCount[cpu] = 0;
		entry = &g_excpData.nestCount[cpu++];
		((unsigned char **)entry)[8] = stack;
		((unsigned char **)entry)[12] = stack + EXCP_STACK_SIZE;
		if (cpu == 4)
			break;
		stack += EXCP_STACK_SIZE;
	}
	g_excpData.kernelMmu = SceSysrootForKernel_118657C6();
#else
	for (cpu = 0; cpu < 4; cpu++) {
		g_excpData.nestCount[cpu] = 0;
		g_excpData.stackBase[cpu] = stack;
		stack += EXCP_STACK_SIZE;
		g_excpData.stackTop[cpu] = stack;
	}
	g_excpData.kernelMmu = sceKernelSysrootGetKernelMMUContext();
#endif
	g_excpData.kernelBreakpoints = NULL;
	g_excpData.kernelWatchpoints = NULL;
	sceKernelSysrootSetModulePrivate(4, &g_excpData);

	g_excpHandlers.monitorVectors = sceKernelSysrootGetVbaseMonitorVector();
	g_excpHandlers.monitorVectorSize = (unsigned int)monitorVectorsEnd - (unsigned int)monitorVectors;
	excpWriteText(g_excpHandlers.monitorVectors, monitorVectors, g_excpHandlers.monitorVectorSize);
	g_excpHandlers.monitorHead = NULL;
	g_excpHandlers.monitorFirst = NULL;
	g_pabtCallback = NULL;
	g_dabtCallback = NULL;
	g_undefCallback = NULL;
	return 0;
}

#if defined(EXCPMGR_SKBL)
/* a processor mode's stack pointer (C cannot name banked registers) */
#define setModeStackPointer(mode, sp) do {					\
	unsigned int __psr;							\
	__asm__ volatile("mrs %0, cpsr\n\t"					\
			 "cpsid aif, #" #mode "\n\t"				\
			 "mov sp, %1\n\t"					\
			 "msr cpsr_fc, %0"					\
			 : "=&r" (__psr) : "r" (sp) : "memory");		\
} while (0)

/* name ours: one 8K stack from the kernel partition, its top 0x40 bytes left free */
static inline void *excpAllocModeStack(const char *name)
{
	void *base;
	SceUID uid;

	sceKernelSysrootCorelockLock(0);
	uid = sceKernelAllocPartitionStackMemBlock(name, 0x2000, 0x1000);
	sceKernelSysrootCorelockUnlock();
	sceKernelGetMemBlockVBase(uid, &base);
	return (char *)base + 0x1fc0;
}

/*
 * name ours: SKBL's excpmgr start, on every CPU: the IRQ, SVC, abort,
 * undefined, FIQ and monitor-mode stacks of this CPU, then (CPU 0) the vectors
 * and handlers.
 */
int ExcpmgrModuleStart(void)
{
	setModeStackPointer(18, excpAllocModeStack("SceKernelIRQStack"));
	setModeStackPointer(19, excpAllocModeStack("SceKernelSvcStack"));
	setModeStackPointer(23, excpAllocModeStack("SceKernelAbtStack"));
	setModeStackPointer(27, excpAllocModeStack("SceKernelUndStack"));
	setModeStackPointer(17, excpAllocModeStack("SceKernelFIQStack"));
	setModeStackPointer(22, excpAllocModeStack("SceKernelMonStack"));
	sceKernelSysrootCorelockLock(0);
	if (sceKernelCpuId() == 0)
		Init(0, NULL);
	sceKernelSysrootCorelockUnlock();
	return 0;
}
#else
/* the banked SP of another processor mode (C cannot name banked registers) */
#define modeStackPointer(mode) ({						\
	unsigned int __psr;							\
	void *__sp;								\
	__asm__ volatile("mrs %0, cpsr\n\t"					\
			 "cpsid aif, #" #mode "\n\t"				\
			 "mov %1, sp\n\t"					\
			 "msr cpsr_fc, %0"					\
			 : "=&r" (__psr), "=&r" (__sp) :: "memory");		\
	__sp;									\
})

int module_bootstart(SceSize args, void *argp)
{
	unsigned int cpu = sceKernelCpuId();

	g_irqStack[cpu] = modeStackPointer(18);
	g_svcStack[cpu] = modeStackPointer(19);
	g_abortStack[cpu] = modeStackPointer(23);
	g_undefStack[cpu] = modeStackPointer(27);
	writeVbar((unsigned int)sceKernelSysrootGetVbaseResetVector() + cpu * 0x40);
	return 0;
}
#endif
