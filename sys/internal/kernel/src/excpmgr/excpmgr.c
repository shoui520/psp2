/* ARM exception vectors, handler chains and saved-context diagnostics. */
#define SCE_DBGINFO_SECTION ".rodata"
#include "excpmgr.h"
#include <sysmem/uid_guid.h>
#include "excpmgr_readonly.h"
#include <sysmem/compiler.h>

/* Shared definition groups retain Sony's original diagnostic source name. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbuiltin-macro-redefined"
#undef __FILE_NAME__
#define __FILE_NAME__ "excpmgr.c"
#pragma GCC diagnostic pop

/* GCC emits these zero-initialized definitions in reverse declaration order. */
#if FW_DEBUG
unsigned int g_vmEnd = 0;
unsigned int g_codeStart = 0;
ExcpHandlerTable g_excpHandlers = { 0 };
unsigned int g_vmStart = 0;
unsigned char g_excpStack[4][EXCP_STACK_SIZE] __attribute__((aligned(32))) = { 0 };
unsigned int g_codeEnd = 0;
#else
unsigned int g_codeStart = 0;
unsigned int g_vmStart = 0;
unsigned int g_codeEnd = 0;
unsigned int g_vmEnd = 0;
ExcpHandlerTable g_excpHandlers = { 0 };
unsigned char g_excpStack[4][EXCP_STACK_SIZE] __attribute__((aligned(32))) = { 0 };
#endif
SceExcpmgrCallback g_dabtCallback = NULL;
SceExcpmgrCallback g_pabtCallback = NULL;
SceExcpmgrCallback g_undefCallback = NULL;
SceExcpmgrData g_excpData = { 0 };

/* Handler definitions are shared with the KBL and emitted in Sony's order. */
#include "excpmgr_handler_cpu.h"

#if !FW_DEBUG
static const unsigned int armNop = 0xe320f000;
static const unsigned int thumbNop = 0xbf00bf00;
#endif

void printVfpRegisters(SceExcpmgrContext *context);
void printWPT(SceExcpmgrContext *context);

#if !FW_DEBUG
static const char *const exceptionNames[] = {
	"",
	"Halting Debug-mode",
	"Secure Monitor",
	"3",
	"Imprecise Data Abort",
	"T2EE Trap",
	"6",
	"7",
	"Reset",
	"Undefined",
	"SVC",
	"Prefetch Abort",
	"Precise Data Abort",
	"Software Watchpoint",
	"IRQ",
	"FIQ",
};
static const char *const waypointNames[] = {
	"Direct   branch",
	"Indirect branch",
	"Exception",
	"DMB/DSB/ISB",
	"Debug entry",
	"Debug exit",
	"6",
	"7",
};
#endif

/* name unknown: suspend callback, nothing to save */
static int excpSuspend(int value, void *args)
{
	return 0;
}

/* name unknown: resume callback, re-arm the trace area of this core */
static int excpResume(int value, void *args)
{
	ExcpTraceArea *trace = ((const ExcpVectorBank *)readVbar())->trace;

	if (trace != NULL)
		trace->control = 1;
	return 0;
}

#if FW_DEBUG
#define EXCP_REPORT_EXTERNAL_DATA
#define EXCP_IT_TEXT(_index) g_excpmgrITText##_index
#define EXCP_REPORT_LINKAGE
#endif
#include "excpmgr_handler_report.h"
#undef EXCP_REPORT_LINKAGE

void printVfpRegisters(SceExcpmgrContext *context)
{
	unsigned int regs[64];
	unsigned int fpscr = context->fpscr, fpexc = context->fpexc;
	int i;

	if (!(context->cpacr & 0xf00000) || !(fpexc & 0x40000000))
		return;
	memcpy(regs, context->vfp, sizeof(regs));
	for (i = 0; i < 32; i += 2)
		SCE_DBG_PRINTF(0, 778,
			EXCP_REPORT_TEXT(vfpFormat, "d%-2d,d%-2d(q%-2d) : 0x%08X%08X 0x%08X%08X\n"),
			i, i + 1, i >> 1, regs[i * 2 + 1], regs[i * 2],
			regs[i * 2 + 3], regs[i * 2 + 2]);
	SCE_DBG_PRINTF(0, 781, EXCP_REPORT_TEXT(fpexcFormat, "FPEXC: 0x%08X\n"), fpexc);
	SCE_DBG_PRINTF(0, 782, EXCP_REPORT_TEXT(fpscrFormat, "FPSCR: 0x%08X\n"), fpscr);
	SCE_DBG_PRINTF(0, 783, EXCP_REPORT_TEXT(separator, "\n"));
}

void printWPT(SceExcpmgrContext *context)
{
	ThreadCB *thread = context->thread;
	SceExcpmgrWaypoint *wp;
	unsigned int first, i, index, flags, moduleBase;
	const char *encoding;
	char name[28];
	SceUID pid, uid;

	pid = thread != NULL ? thread->processId : SCE_KERNEL_PROCESS_ID;
	SCE_PRINTF_WITH_INFO(0, 998,
		EXCP_REPORT_TEXT(waypointHeader, "=== Print Waypoint ===\n"));
	first = (context->waypointControl >> 8 & 31) + 1;
	for (i = first; i != first + 32; i++) {
		index = i & 31;
		wp = &context->waypoint[index];
		flags = wp->flags;
		if (!(flags & 0x80000000))
			continue;
		if ((flags & 7) == 2) {
			SCE_PRINTF_WITH_INFO(0, 1015,
				EXCP_REPORT_TEXT(waypointExceptionFormat, "(%2d) PC:0x%08X, TARGETPC:0x%08X <<< %s Exception >>>\n"),
				index, wp->pc, wp->targetPc, exceptionNames[flags >> 4 & 15]);
			if ((flags & 0xf0) != 0x50)
				break;
			continue;
		}
		if ((flags & 7) > 1) {
			SCE_PRINTF_WITH_INFO(0, 1026,
				EXCP_REPORT_TEXT(waypointEventFormat, "(%2d) PC:0x%08X, TARGETPC:0x%08X <<< %s >>>\n"),
				index, wp->pc, wp->targetPc, waypointNames[flags & 7]);
			continue;
		}
		switch (flags & 0x1800) {
		case 0:
			encoding = EXCP_REPORT_TEXT(arm, "ARM");
			break;
		case 0x800:
			encoding = EXCP_REPORT_TEXT(jazelle, "Jzl");
			break;
		case 0x1000:
			encoding = flags & 0x200 ? EXCP_REPORT_TEXT(thumb32, "T32") : EXCP_REPORT_TEXT(thumb16, "T16");
			break;
		default:
			encoding = flags & 0x200 ? EXCP_REPORT_TEXT(thumbEE32, "E32") : EXCP_REPORT_TEXT(thumbEE16, "E16");
			break;
		}
		if (SceSysrootForKernel_1D84C4D4(wp->targetPc < 0x40000000 ? SCE_KERNEL_PROCESS_ID : pid,
				wp->targetPc, &uid, &moduleBase, name) < 0) {
			SCE_PRINTF_WITH_INFO(0, 1078,
				EXCP_REPORT_TEXT(waypointNoModuleFormat, "(%2d) PC:0x%08X, TARGETPC:0x%08X %s%c%c [no module info]\n"),
				index, wp->pc, wp->targetPc, encoding,
				flags & 0x100 ? 'L' : 'B', flags & 0x400 ? 't' : 'n');
			continue;
		}
		if ((pid == SCE_KERNEL_PROCESS_ID || wp->targetPc < 0x4000000) && !SceQafMgrForDriver_382C71E8())
			snprintf(name, 27, EXCP_REPORT_TEXT(uidFormat, "0x%08X"), uid);
		SCE_PRINTF_WITH_INFO(0, 1069,
			EXCP_REPORT_TEXT(waypointModuleFormat, "(%2d) PC:0x%08X, TARGETPC:0x%08X %s%c%c [%s + 0x%08X]\n"),
			index, wp->pc, wp->targetPc, encoding,
			flags & 0x100 ? 'L' : 'B', flags & 0x400 ? 't' : 'n',
			name, wp->targetPc - moduleBase);
	}
}

void _sceKernelPanicHandlerCommon(SceExcpmgrContext *context, unsigned int reason,
				  const SceKernelDebugInfo *info, const char *message)
{
#if FW_DEBUG
	ThreadCB *thread = context->thread;
	const char *processName, *threadName;
	SceUID id;

	if (thread != NULL) {
		id = thread->processId;
		if (sceGUIDGetName(id, &processName) < 0)
			SCE_DBG_PRINTF(0, 1135, EXCP_REPORT_TEXT(processIdFormat, "ProcessId: 0x%08X\n"), id);
		else
			SCE_DBG_PRINTF(0, 1137,
				EXCP_REPORT_TEXT(processIdNameFormat, "ProcessId: 0x%08X [%s]\n"), id, processName);
		id = thread->threadId;
		if (sceGUIDGetName(id, &threadName) < 0)
			SCE_DBG_PRINTF(0, 1143, EXCP_REPORT_TEXT(threadIdFormat, "ThreadId:  0x%08X\n"), id);
		else
			SCE_DBG_PRINTF(0, 1145,
				EXCP_REPORT_TEXT(threadIdNameFormat, "ThreadId:  0x%08X [%s]\n"), id, threadName);
	} else {
		SCE_DBG_PRINTF(0, 1148,
			EXCP_REPORT_TEXT(interruptContext, "  Context:Interrupt/Exception Handler\n"));
	}
	printExcpSavedRegisters(context);
	if (reason == 4)
		printIFSR(context);
	else if (reason == 1)
		printDFSR(context);
	printVfpRegisters(context);
	printWPT(context);
#endif
	if (readTpidrprw() == 0 && (int)g_excpData.nestCount[SceCpuId()] > 1)
		for (;;)
			;
	SceDebugForKernel_CCABDD98(info, context, reason, message);
	__builtin_unreachable();	/* sysmem's report stops the CPU */
}

void SceExcpmgrForKernel_58F7212B(SceExcpmgrCallback callback)
{
	g_pabtCallback = callback;
}

void SceExcpmgrForKernel_9EE59C6E(SceExcpmgrCallback callback)
{
	g_dabtCallback = callback;
}

void SceExcpmgrForKernel_1FBF5654(SceExcpmgrCallback callback)
{
	g_undefCallback = callback;
}

#include "excpmgr_handler_register.h"

int sceKernelReleaseExceptionHandler(unsigned int code, SceExcpmgrHandler *handler)
{
	SceExcpmgrHandler **first;
	SceExcpmgrHandler *head, *last, *prev, *next;
	unsigned int *words;
	unsigned int i, j;
	int flags;

	SCE_DBG_PRINTF(0xb, 276,
		EXCP_REPORT_TEXT(handlerFormat, "(excode %d, handler %p)\n"), code, handler);
	if (handler == NULL) {
		SCE_DBG_ASSERT_FAIL(278);
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}
	words = (unsigned int *)((unsigned int)handler & ~3U);
	if (words[0] == 0) {
		SCE_DBG_ASSERT_FAIL(283);
		return SCE_KERNEL_ERROR_ILLEGAL_EXCPHANDLER;
	}
	if (code > 7) {
		SCE_DBG_ASSERT_FAIL(288);
		return SCE_KERNEL_ERROR_ILLEGAL_EXCPCODE;
	}

	flags = sceKernelSpinlockLowLockCpuSuspendIntr(&g_excpHandlers.lock);
	first = g_excpHandlers.first[code];
	head = g_excpHandlers.head[code];
	if (head == NULL) {
		SCE_DBG_ASSERT_FAIL(295);
		goto notFound;
	}
	if (handler == head) {
		next = (SceExcpmgrHandler *)words[0];
		for (i = 0; i < 8; i++)
			if (first[i] == handler) {
				first[i] = NULL;
				break;
			}
		g_excpHandlers.head[code] = next;
		excpSetVector(code, next);
	} else {
		/* the chain of this code ends at its last registered handler */
		last = head;
		for (i = 0; i < 8; i++)
			if (first[i] != NULL)
				last = first[i];
		prev = head;
		for (;;) {
			if (prev == last)
				goto notFound;
			next = *(SceExcpmgrHandler **)((unsigned int)prev & ~3U);
			if (next == handler)
				break;
			prev = next;
		}
		excpPokeWord((unsigned int)prev, words[0]);
		for (i = 0; i < 8; i++)
			if (first[i] == handler)
				break;
		if (i < 8) {
			/* prev takes over the slot unless it already starts its own */
			for (j = 0; j <= i; j++) {
				if (first[j] == prev) {
					first[i] = NULL;
					break;
				}
				if (j == i)
					first[i] = prev;
			}
		}
	}
	excpPokeWord((unsigned int)handler, 0);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_excpHandlers.lock, flags);
	return 0;

notFound:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_excpHandlers.lock, flags);
	return SCE_KERNEL_ERROR_NOTFOUND_EXCPHANDLER;
}

SceExcpmgrHandler *SceExcpmgrForKernel_4337DD78(void)
{
	return g_excpHandlers.defaultHandler;
}

#include "excpmgr_handler_default.h"

/*
 * name unknown: data aborts on unprivileged Thumb stores (STRBT/STRT, LDRSBT)
 * inside the vmaccess window are resumed at the r12 the caller staged (12),
 * aborts inside the registered code range return an error to the caller.
 */
int ClassifyAbortInstruction(unsigned int pc, unsigned int cpsr)
{
	const unsigned short *code;
	unsigned int instruction;

	if (!(cpsr & 0x20))
		return 0;
	if (g_vmStart <= pc && pc < g_vmEnd) {
		code = (const unsigned short *)(pc & ~1U);
		instruction = code[0] << 16 | code[1];
		if ((instruction & 0xf0000) == 0xf0000)
			return 0;
		if ((instruction & 0xffc00f00) == 0xf8000e00 ||
		    (instruction & 0xffe00f00) == 0xf8400e00)
			return 12;
		return (instruction & 0xffd00f00) == 0xf9100e00 ? 12 : 0;
	}
	if (pc < g_codeStart)
		return 0;
	return pc < g_codeEnd ? SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS : 0;
}

/*
 * name unknown: a user process hit its debug trap instruction; replace it with
 * a NOP in the process text and resume there. Returns the pc to resume at.
 */
unsigned int PatchUserInstruction(SceExcpmgrContext *context)
{
	unsigned int cpsr = context->cpsr;
	unsigned int pc = context->pc;
	ThreadCB *thread = context->thread;
	unsigned int instruction;
	SceUID pid;
	int res;

	if ((cpsr & 0x1f) != 0x10)
		return 0;
	if ((cpsr & 0x1000020) == 0x1000000 || thread == NULL)
		return 0;
	pid = thread->processId;
	if (pid == SCE_KERNEL_PROCESS_ID)
		return 0;
	if (sceKernelCopyFromUserProc(pid, &instruction, (void *)pc, 4) < 0)
		return 0;
	if (cpsr & 0x20) {
		instruction = instruction >> 16 | instruction << 16;
		if ((instruction & 0x0ff00fff) != 0x0c400f0b)
			return 0;
		res = SceSysmemForKernel_2995558D(pid, (void *)pc, &thumbNop, 4);
	} else {
		if ((instruction & 0x0ff00fff) != 0x0c400f0b)
			return 0;
		res = SceSysmemForKernel_2995558D(pid, (void *)pc, &armNop, 4);
	}
	if (res < 0)
		for (;;)
			;
	return pc;
}

/* exception nesting depth of this core, 0 in thread context */
int SceExcpmgrForKernel_B615A7DA(void)
{
	if (getCurrentThreadCB() != NULL)
		return 0;
	return g_excpData.nestCount[SceCpuId()];
}

SceExcpmgrData *SceExcpmgrForKernel_96C2869C(void)
{
	return &g_excpData;
}

void *SceExcpmgrForKernel_5420ED8F(int cpu)
{
	void *const *stackTop = g_excpData.stackTop;

	return stackTop[cpu];
}

void SceExcpmgrForKernel_293DFA04(ProcessHwBreakpoints *regs)
{
	g_excpData.kernelBreakpoints = regs;
}

void SceExcpmgrForKernel_E7487AFD(ProcessHwWatchpoints *regs)
{
	g_excpData.kernelWatchpoints = regs;
}

int Init(SceSize args, void *argp)
{
	SceKernelAllocMemBlockKernelOpt opt;
	unsigned int dacr, code, cpu, offset, value;
	unsigned int start, size, i;
	char *trace;
	unsigned char *stack;
	unsigned int *vectors;
	SceUID uid;

	g_excpHandlers.lock = 0;
	vectors = SceSysrootForKernel_CC85905B();
	g_excpHandlers.vectors = vectors;
	size = (char *)&defaultHandler - (char *)exceptionVectors;
	g_excpHandlers.vectorSize = size;
	dacr = readDacr();
	writeDacr(DACR_ALL_MANAGER);
	start = (unsigned int)memcpy(vectors, exceptionVectors, size);
	sceKernelDcacheCleanRangeForL1WBWA((void *)cacheLineDown(start),
		cacheLineDown(start + size + 31) - cacheLineDown(start));
	writeDacr(dacr);
	SceCpuForKernel_803C84BF();
	SceCpuForKernel_C32687D0(&g_vmStart, &g_vmEnd);

	g_excpHandlers.defaultHandler = NULL;
	for (code = 0; code < SCE_EXCP_MAX; code++) {
		g_excpHandlers.head[code] = NULL;
		for (i = 0; i < SCE_EXCP_PRIORITY_MAX; i++)
			g_excpHandlers.first[code][i] = NULL;
	}
	sceKernelRegisterDefaultExceptionHandler(&defaultHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_RESET, 7, &resetHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_SVC, 7, &svcHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_RESERVED, 7, &reservedHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_IRQ, 7, &irqHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_UNDEF, 7, &panicUndefHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_PABT, 7, &panicPabtHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_DABT, 7, &panicDabtHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_UNDEF, 0, &initialUndefHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_PABT, 0, &initialPabtHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_DABT, 0, &initialDabtHandler);

	/* per-core vector words: 0x20 sysroot per-core block, 0x24 trace area */
	for (cpu = 0, offset = 0x20; cpu < 4; cpu++, offset += 0x40) {
		value = (unsigned int)SceSysrootForKernel_06182D59(cpu);
		excpPokeVector((unsigned int *)((char *)g_excpHandlers.vectors + offset), &value);
	}
	stack = g_excpStack[0];
	for (cpu = 0; cpu < 4; cpu++) {
		g_excpData.nestCount[cpu] = 0;
		g_excpData.stackBase[cpu] = stack;
		stack += EXCP_STACK_SIZE;
		g_excpData.stackTop[cpu] = stack;
	}
	g_excpData.kernelMmu = SceSysrootForKernel_118657C6();
	g_excpData.kernelBreakpoints = NULL;
	g_excpData.kernelWatchpoints = NULL;
	sceKernelSysrootSetModulePrivate(4, &g_excpData);

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 2;
	opt.paddr = VFP_INT_REG_BASE;
	uid = sceKernelAllocMemBlockForKernel(EXCP_REPORT_TEXT(vfpBlockName, "SceVfpIntRegs"), SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_SO_RW, 0x4000, &opt);
	if (uid < 0)
		SCE_DBG_ASSERT_FAIL(1649);
	else
		sceKernelGetMemBlockBase(uid, (void **)&trace);
	for (i = 0, offset = 0x24; i < 0x4000; i += 0x1000, offset += 0x40) {
		value = (unsigned int)(trace + i);
		((ExcpTraceArea *)value)->control = 1;
		excpPokeVector((unsigned int *)((char *)g_excpHandlers.vectors + offset), &value);
	}
	SceKernelSuspendForDriver_CE7A2207(0x1e, excpSuspend, NULL);
	SceKernelSuspendForDriver_105C5752(0x1e, excpResume, NULL);
	g_pabtCallback = NULL;
	g_dabtCallback = NULL;
	g_undefCallback = NULL;
	return 0;
}

int module_bootstart(SceSize args, void *argp)
{
	unsigned int cpu = sceKernelCpuId();

	writeVbar((unsigned int)SceSysrootForKernel_CC85905B() + cpu * 0x40);
	return 0;
}

void sceKernelInitialHandlerDebugUndefCfunc(SceExcpmgrContext *context)
{
	ThreadCB *thread = context->thread;
	struct SceKernelBacktraceFrameForKernel frames[64];
	unsigned int count;
	const char *threadName, *processName;
	SceUID pid, tid;
	int res;

	SCE_DBG_PRINTF(0, 1813,
		EXCP_REPORT_TEXT(undefHeader, "\n*** UNDEF Exception at CPU%d\n"), SceCpuId());
	if (readTpidrprw() == 0 && (int)g_excpData.nestCount[SceCpuId()] > 1)
		for (;;)
			;
	if (thread != NULL) {
		SCE_DBG_PRINTF(0, 1822,
			EXCP_REPORT_TEXT(processIdFormat, "ProcessId: 0x%08X\n"), thread->processId);
		tid = thread->threadId;
		SCE_DBG_PRINTF(0, 1825, EXCP_REPORT_TEXT(threadIdFormat, "ThreadId:  0x%08X\n"), tid);
	} else {
		SCE_DBG_PRINTF(0, 1827,
			EXCP_REPORT_TEXT(interruptContext, "  Context:Interrupt/Exception Handler\n"));
	}
	printExcpSavedRegisters(context);
	printVfpRegisters(context);
	printWPT(context);
	memset(frames, 0, sizeof(frames));
	if (sceKernelSysrootBacktrace(0xfffffff0, frames, 256, &count, 3) >= 0)
		sceKernelSysrootPrintBacktrace(SCE_KERNEL_PROCESS_ID, frames, count, 0);
	if (thread == NULL)
		return;
	pid = thread->processId;
	memset(frames, 0, sizeof(frames));
	if (sceKernelSysrootBacktrace(0xfffffff0, frames, 256, &count, 10) >= 0)
		sceKernelSysrootPrintBacktrace(pid, frames, count, 0);
	res = sceGUIDGetName(pid, &processName);
	if (res < 0)
		SCE_DBG_PRINTF(0, 1861,
			EXCP_REPORT_TEXT(processNameErrorFormat, "ProcessName: error 0x%08X (pid=0x%08X)\n"), res, pid);
	else
		SCE_DBG_PRINTF(0, 1863,
			EXCP_REPORT_TEXT(processNameFormat, "ProcessName: %p[%s] (pid=0x%08X)\n"),
			processName, processName, pid);
	res = sceGUIDGetName(tid, &threadName);
	if (res < 0)
		SCE_DBG_PRINTF(0, 1867,
			EXCP_REPORT_TEXT(threadNameErrorFormat, "ThreadName:  error 0x%08X (tid=0x%08X)\n"), res, tid);
	else
		SCE_DBG_PRINTF(0, 1869,
			EXCP_REPORT_TEXT(threadNameFormat, "ThreadName:  %p[%s] (tid=0x%08X)\n"),
			threadName, threadName, tid);
}

void sceKernelInitialHandlerDebugPabtCfunc(SceExcpmgrContext *context)
{
	ThreadCB *thread = context->thread;
	struct SceKernelBacktraceFrameForKernel frames[64];
	unsigned int count;
	const char *threadName, *processName;
	SceUID pid, tid;
	int res;

	SCE_DBG_PRINTF(0, 1893,
		EXCP_REPORT_TEXT(pabtHeader, "\n*** PABT Exception at CPU%d\n"), SceCpuId());
	if (readTpidrprw() == 0 && (int)g_excpData.nestCount[SceCpuId()] > 1)
		for (;;)
			;
	if (thread != NULL) {
		SCE_DBG_PRINTF(0, 1902,
			EXCP_REPORT_TEXT(processIdFormat, "ProcessId: 0x%08X\n"), thread->processId);
		tid = thread->threadId;
		SCE_DBG_PRINTF(0, 1905, EXCP_REPORT_TEXT(threadIdFormat, "ThreadId:  0x%08X\n"), tid);
	} else {
		SCE_DBG_PRINTF(0, 1907,
			EXCP_REPORT_TEXT(interruptContext, "  Context:Interrupt/Exception Handler\n"));
	}
	printExcpSavedRegisters(context);
	printVfpRegisters(context);
	printIFSR(context);
	printWPT(context);
	memset(frames, 0, sizeof(frames));
	if (sceKernelSysrootBacktrace(0xfffffff0, frames, 256, &count, 3) >= 0)
		sceKernelSysrootPrintBacktrace(SCE_KERNEL_PROCESS_ID, frames, count, 0);
	pid = SCE_KERNEL_PROCESS_ID;
	if (thread != NULL) {
		pid = thread->processId;
		memset(frames, 0, sizeof(frames));
		if (sceKernelSysrootBacktrace(0xfffffff0, frames, 256, &count, 10) >= 0)
			sceKernelSysrootPrintBacktrace(pid, frames, count, 0);
		res = sceGUIDGetName(pid, &processName);
		if (res < 0)
			SCE_DBG_PRINTF(0, 1944,
				EXCP_REPORT_TEXT(processNameErrorFormat, "ProcessName: error 0x%08X (pid=0x%08X)\n"), res, pid);
		else
			SCE_DBG_PRINTF(0, 1946,
				EXCP_REPORT_TEXT(processNameFormat, "ProcessName: %p[%s] (pid=0x%08X)\n"),
				processName, processName, pid);
		res = sceGUIDGetName(tid, &threadName);
		if (res < 0)
			SCE_DBG_PRINTF(0, 1950,
				EXCP_REPORT_TEXT(threadNameErrorFormat, "ThreadName:  error 0x%08X (tid=0x%08X)\n"), res, tid);
		else
			SCE_DBG_PRINTF(0, 1952,
				EXCP_REPORT_TEXT(threadNameFormat, "ThreadName:  %p[%s] (tid=0x%08X)\n"),
				threadName, threadName, tid);
	}
	if (context->pc == 0)
		SceSysrootForKernel_0A12227B(pid, context->r[12]);
}

void sceKernelInitialHandlerDebugDabtCfunc(SceExcpmgrContext *context)
{
	ThreadCB *thread = context->thread;
	struct SceKernelBacktraceFrameForKernel frames[64];
	unsigned int count, dacr;
	const char *threadName, *processName;
	SceUID pid, tid;
	int res;

	SCE_DBG_PRINTF(0, 1982,
		EXCP_REPORT_TEXT(dabtHeader, "\n*** DABT Exception at CPU%d\n"), SceCpuId());
	if (unlikely(readTpidrprw() == 0) &&
	    (int)g_excpData.nestCount[SceCpuId()] > 1)
		for (;;)
			;
	dacr = readDacr();
	writeDacr(dacr | DACR_ALL_CLIENT);
	if (thread != NULL) {
		SCE_DBG_PRINTF(0, 1994,
			EXCP_REPORT_TEXT(processIdFormat, "ProcessId: 0x%08X\n"), thread->processId);
		tid = thread->threadId;
		SCE_DBG_PRINTF(0, 1998, EXCP_REPORT_TEXT(threadIdFormat, "ThreadId:  0x%08X\n"), tid);
	} else {
		SCE_DBG_PRINTF(0, 2000,
			EXCP_REPORT_TEXT(interruptContext, "  Context:Interrupt/Exception Handler\n"));
	}
	printExcpSavedRegisters(context);
	printVfpRegisters(context);
	printDFSR(context);
	printWPT(context);
	memset(frames, 0, sizeof(frames));
	if (sceKernelSysrootBacktrace(0xfffffff0, frames, 256, &count, 3) >= 0)
		sceKernelSysrootPrintBacktrace(SCE_KERNEL_PROCESS_ID, frames, count, 0);
	if (unlikely(thread != NULL)) {
		pid = thread->processId;
		memset(frames, 0, sizeof(frames));
		if (sceKernelSysrootBacktrace(0xfffffff0, frames, 256, &count, 10) >= 0)
			sceKernelSysrootPrintBacktrace(pid, frames, count, 0);
		res = sceGUIDGetName(pid, &processName);
		if (res < 0)
			SCE_DBG_PRINTF(0, 2035,
				EXCP_REPORT_TEXT(processNameErrorFormat, "ProcessName: error 0x%08X (pid=0x%08X)\n"), res, pid);
		else
			SCE_DBG_PRINTF(0, 2037,
				EXCP_REPORT_TEXT(processNameFormat, "ProcessName: %p[%s] (pid=0x%08X)\n"),
				processName, processName, pid);
		res = sceGUIDGetName(tid, &threadName);
		if (res < 0)
			SCE_DBG_PRINTF(0, 2041,
				EXCP_REPORT_TEXT(threadNameErrorFormat, "ThreadName:  error 0x%08X (tid=0x%08X)\n"), res, tid);
		else
			SCE_DBG_PRINTF(0, 2043,
				EXCP_REPORT_TEXT(threadNameFormat, "ThreadName:  %p[%s] (tid=0x%08X)\n"),
				threadName, threadName, tid);
	}
	writeDacr(dacr);
}

int SceExcpmgrForKernel_44CE04B8(unsigned int start, unsigned int end)
{
	g_codeStart = start;
	g_codeEnd = end;
	return 0;
}

