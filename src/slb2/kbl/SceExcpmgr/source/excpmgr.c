/* ARM and secure-monitor exception vectors of the secure kernel. */
#include "excpmgr.h"

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

static void _sceKernelPanicHandlerCommon(SceExcpmgrContext *context, unsigned int reason,
					 const SceKernelDebugInfo *info, const char *message)
	__attribute__((noreturn));

/* registration, default/panic handlers and register report, shared with os0 */
#include "sce/excpmgr_handler.h"
/* the state save and return path of the vector stubs, shared with os0 */
#include "sce/excpmgr_context.h"

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
	ksceKernelDcacheCleanRangeForL1WBWA((void *)cacheLineDown(start),
		cacheLineDown(start + size + 31) - cacheLineDown(start));
	writeDacr(dacr);
	SceCpuForTZS_A5965CBF();
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

void sceKernelDefaultHandlerFiqCfunc(void)
{
	SCE_DBG_PRINTF(0xb, 550, "()\n");
	sceKernelDefaultHandlerCfunc();
}

int sceKernelRegisterMonitorEntry(unsigned int code, SceExcpmgrHandler *handler)
{
	unsigned int entry;
	int flags;

	if (handler == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (ksceKernelIsColdBoot()) {
		if (handler->next != NULL || handler->reserved != 0) {
			SCE_DBG_ASSERT_FAIL(1433);
			return SCE_KERNEL_ERROR_ILLEGAL_EXCPHANDLER;
		}
	}
	if (code > 7) {
		SCE_DBG_ASSERT_FAIL(1439);
		return SCE_KERNEL_ERROR_ILLEGAL_EXCPCODE;
	}

	flags = ksceKernelSpinlockLowLockCpuSuspendIntr(&g_excpHandlers.lock);
	if (g_excpHandlers.monitorFirst == NULL)
		g_excpHandlers.monitorFirst = handler;
	excpPokeWord((unsigned int)handler, (unsigned int)g_excpHandlers.monitorHead);
	g_excpHandlers.monitorHead = handler;
	/* the monitor vectors load their target from the slot table at +0x20 */
	entry = (unsigned int)handler + 8;
	excpPokeVector(&g_excpHandlers.monitorVectors[8 + code], &entry);
	SceCpuForTZS_17A88E69(g_excpHandlers.monitorVectors, g_excpHandlers.monitorVectorSize);
	SceCpuForTZS_B421FAFD(g_excpHandlers.monitorVectors, g_excpHandlers.monitorVectorSize);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&g_excpHandlers.lock, flags);
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

int module_start(SceSize args, void *argp)
{
	unsigned int code, priority, cpu, value;
	unsigned char *stack;

	g_excpHandlers.lock = 0;
	g_excpHandlers.vectors = SceSysrootForTZS_2460ADFA();
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
		value = SceSysrootForTZS_317E7556(cpu);
		excpPokeVector(&g_excpHandlers.vectors[cpu * 16 + 8], &value);
	}

	stack = g_excpStack[0];
	for (cpu = 0; cpu < 4; cpu++) {
		g_excpData.nestCount[cpu] = 0;
		g_excpData.stackBase[cpu] = stack;
		stack += EXCP_STACK_SIZE;
		g_excpData.stackTop[cpu] = stack;
	}
	g_excpData.kernelMmu = SceSysrootForTZS_A8B7DAF9();
	g_excpData.kernelBreakpoints = NULL;
	g_excpData.kernelWatchpoints = NULL;
	SceSysrootForTZS_EA572BD9(4, &g_excpData);

	g_excpHandlers.monitorVectors = SceSysrootForTZS_D4E6D8E8();
	g_excpHandlers.monitorVectorSize = (unsigned int)monitorVectorsEnd - (unsigned int)monitorVectors;
	excpWriteText(g_excpHandlers.monitorVectors, monitorVectors, g_excpHandlers.monitorVectorSize);
	g_excpHandlers.monitorHead = NULL;
	g_excpHandlers.monitorFirst = NULL;
	g_pabtCallback = NULL;
	g_dabtCallback = NULL;
	g_undefCallback = NULL;
	return 0;
}

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
	unsigned int cpu = SceCpuForTZS_44C423D3();

	g_irqStack[cpu] = modeStackPointer(18);
	g_svcStack[cpu] = modeStackPointer(19);
	g_abortStack[cpu] = modeStackPointer(23);
	g_undefStack[cpu] = modeStackPointer(27);
	writeVbar((unsigned int)SceSysrootForTZS_2460ADFA() + cpu * 0x40);
	return 0;
}

/*
 * Entry functions, called by the initial-handler stubs on the exception-mode
 * stack: save the rest of the interrupted state, switch to the kernel and
 * return the top of this core's exception stack, where the stub continues in
 * System mode and walks the handler chain.
 */
void *excpUndefEntry(SceExcpmgrContext *context)
{
	excpSaveState(context, SCE_EXCP_UNDEF);
	excpEnterKernel();
	return excpEnterNested();
}

void *excpPabtEntry(SceExcpmgrContext *context)
{
	excpSaveState(context, SCE_EXCP_PABT);
	excpEnterKernel();
	return excpEnterNested();
}

void *excpDabtEntry(SceExcpmgrContext *context)
{
	excpSaveState(context, SCE_EXCP_DABT);
	excpEnterKernel();
	return excpEnterNested();
}

/*
 * Last handlers of the undef/pabt/dabt chains (priority 7). The stub passes
 * the chain argument the initial handlers set (3); any other request leaves
 * the exception with the context's thread installed and stops this core.
 */
static void __attribute__((noreturn)) excpPanicLeave(SceExcpmgrContext *context)
{
	unsigned int cpu = excpCpu();

	if (g_excpData.nestCount[cpu] - 1 != 0)
		for (;;)
			;
	g_excpData.nestCount[cpu] = 0;
	writeDfsr(0);
	writeIfsr(0);
	writeAdfsr(0);
	writeAifsr(0);
	writeDfar(0);
	writeIfar(0);
	writeTpidrprw((unsigned int)context->thread);
	for (;;)
		;
}

static inline void __attribute__((noreturn, always_inline))
excpPanicDispatch(SceExcpmgrContext *context, unsigned int arg,
		  void (*panic)(SceExcpmgrContext *context))
{
	switch (arg & 0xffff) {
	case 3:
		panic(context);
		for (;;)
			;
	case 0:
	case 1:
	case 2:
	case 4:
		excpPanicLeave(context);
	default:
		for (;;)
			;
	}
}

void excpPanicUndef(SceExcpmgrContext *context, unsigned int arg)
{
	excpPanicDispatch(context, arg, sceKernelPanicHandlerUndefCfunc);
}

void excpPanicPabt(SceExcpmgrContext *context, unsigned int arg)
{
	excpPanicDispatch(context, arg, sceKernelPanicHandlerPabtCfunc);
}

void excpPanicDabt(SceExcpmgrContext *context, unsigned int arg)
{
	excpPanicDispatch(context, arg, sceKernelPanicHandlerDabtCfunc);
}
