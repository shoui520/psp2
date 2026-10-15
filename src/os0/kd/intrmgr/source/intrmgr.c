/* SceKernelIntrMgr: GIC interrupts, subinterrupts and CPU exception state. */
#include "intrmgr.h"
#include "intrmgr_readonly.h"
#include "sce/fast_mutex.h"
#include "sce/compiler.h"

_Static_assert(sizeof(IntrHandler) == 64, "interrupt record ABI");
_Static_assert(sizeof(IntrState) == 0x4200, "handler and CPU state ABI");
_Static_assert(sizeof(SceKernelIntrHandlerInfo) == 0x70, "interrupt query ABI");
_Static_assert(sizeof(IntrIrqFrame) == 64, "IRQ frame ABI");
_Static_assert(sizeof(IntrLevelStack) == INTR_LEVEL_STACK_SIZE, "nesting stack ABI");

int CurrentNesting(void);

/* name unknown: identify interrupt context without a thread context. */
static inline int IllegalContext(void)
{
	return !getCurrentThreadCB() && CurrentNesting();
}

/* name unknown: update the interrupt configuration field. */
static inline void SetConfiguration(unsigned int intrCode, unsigned int config)
{
	unsigned int index = intrCode >> 4;
	unsigned int shift = (intrCode & 15) * 2;
	unsigned int value = g_intrDistributor->config[index];

	g_intrDistributor->config[index] = value ^ (((value >> shift) ^ config) & 3) << shift;
}

/* name unknown: write one instruction into the entry code and clean its lines. */
static inline __attribute__((always_inline)) void PatchCode(void *address, const void *code,
							    unsigned int size)
{
	unsigned int dacr, start;

	start = (unsigned int)address & ~31U;
	dacr = readDacr();
	writeDacr(0xffff0000);
	if (size == 2)
		*(volatile SceUInt16 *)address = *(const SceUInt16 *)code;
	else
		*(volatile SceUInt32 *)address = *(const SceUInt32 *)code;
	ksceKernelDcacheCleanRangeForL1WBWA((void *)start,
		(((unsigned int)address + size + 31) & ~31U) - start);
	writeDacr(dacr);
}

/* name unknown: convert the two accepted CPU mask encodings. */
static inline __attribute__((always_inline)) int TargetMask(unsigned int target, unsigned int *mask)
{
	if (!target)
		return SCE_KERNEL_ERROR_ILLEGAL_TARGET_CPU;
	if (target & 0xf0000) {
		if (target & 15)
			return SCE_KERNEL_ERROR_ILLEGAL_TARGET_CPU;
		*mask = target >> 16;
	} else if (target & 15) {
		if ((int)target < 0)
			return SCE_KERNEL_ERROR_ILLEGAL_TARGET_CPU;
		*mask = target;
	} else {
		*mask = target >> 16;
	}
	return 0;
}

/* names unknown: suspend/resume registration callbacks. */
int SuspendController(void)
{
	return 0;
}

/* name unknown: reset the GIC interfaces on the current core. */
void ResetController(void)
{
	unsigned int blocks, i;

	g_intrCpuInterface->control = 0;
	if (!ksceKernelCpuId())
		g_intrDistributor->control = 0;
	blocks = (g_intrDistributor->type & 31) + 1;
	i = 0;
	do {
		g_intrDistributor->clearEnable[i] = ~0U;
		i++;
	} while (i != blocks);
	g_intrCpuInterface->control = 1;
	if (!ksceKernelCpuId())
		g_intrDistributor->control = 1;
}

int ResumeController(void)
{
	unsigned int paddr;

	sceKernelSysrootCorelockLock(0);
	ResetController();
	sceKernelSysrootCorelockUnlock();
	sceKernelSysrootCorelockLock(0);
	if (!ksceKernelCpuId()) {
		for (unsigned int i = 0; i < INTR_COUNT; i++) {
			IntrHandler *info = &g_intrHandlers[i];
			g_intrDistributor->priority[i] = info->priority;
			g_intrDistributor->target[i] = info->targetCpu;
			if (info->configuration == 3)
				SetConfiguration(i, 3);
		}
		ksceKernelVAtoPA(ksceKernelSysrootGetSharedMemory(), &paddr);
		intrmgrSmc(paddr, 0xa000);
	}
	sceKernelSysrootCorelockUnlock();
	return 0;
}

/* name unknown: interrupt nesting depth of the current core. */
int __attribute__((noinline)) CurrentNesting(void)
{
	return g_intrCpu[readMpidr() & 3].nesting;
}

/* Thumb B (T2) from the patched slot to a hook stub. */
#define THUMB_BRANCH(from, to) \
	(0xe000 | (((((int)(to) - (int)(from)) >> 1) - 2) & 0x7ff))
/* ARM B from the patched slot to a hook stub (the pc reads 8 ahead). */
#define ARM_BRANCH(from, to) \
	(0xea000000 | ((((int)(to) - 8 - (int)(from)) >> 2) & 0xffffff))

void SceIntrmgrForKernel_217AC350(void *enter, void *leave)
{
	SceUInt32 code;

	g_intrEnterSyscall = enter;
	g_intrLeaveSyscall = leave;
	code = enter ? THUMB_BRANCH(&syscallEnterBranch, syscallEnterHook) : 0xbf00;
	PatchCode(&syscallEnterBranch, &code, sizeof(SceUInt16));
	code = leave ? THUMB_BRANCH(&syscallLeaveBranch, syscallLeaveHook) : 0xbf00;
	PatchCode(&syscallLeaveBranch, &code, sizeof(SceUInt16));
}

void SceIntrmgrForKernel_8D6E1289(void *enter, void *leave)
{
	SceUInt32 code;

	g_intrEnterIntr = enter;
	g_intrLeaveIntr = leave;
	code = enter ? ARM_BRANCH(&intrEnterBranch, intrEnterHook) : 0xe320f000;
	PatchCode(&intrEnterBranch, &code, sizeof(code));
	code = leave ? ARM_BRANCH(&intrLeaveBranch, intrLeaveHook) : 0xe320f000;
	PatchCode(&intrLeaveBranch, &code, sizeof(code));
}

void SceIntrmgrForKernel_04DBA732(void *enter, void *leave)
{
	g_intrEnterSubIntr = enter;
	g_intrLeaveSubIntr = leave;
}

int ksceKernelIsIntrAllowedInCurrentContext(void)
{
	if (getCurrentThreadCB())
		return 0;
	return CurrentNesting();
}

void SceIntrmgrForKernel_6F5B8AF2(void (*registered)(unsigned int), void (*released)(unsigned int))
{
	g_intrRegistered = registered;
	g_intrReleased = released;
}

void SceIntrmgrForKernel_E3B670BF(int (*event)(unsigned int, unsigned int, void *))
{
	g_intrEvent = event;
}

int callIntrEvent(unsigned int event, unsigned int size, void *data)
{
	return g_intrEvent ? g_intrEvent(event, size, data) : 0;
}

int SceIntrmgrForDriver_C568F145(unsigned int value)
{
	return g_intrEvent(0x100000, 4, &value);
}

int sceKernelRegisterIntrHandler(unsigned int intrCode, const char *name, unsigned int type,
				void *handler, void *arg, unsigned int priority,
				unsigned int target, const SceKernelIntrRegisterOpt *opt)
{
	IntrHandler *info;
	unsigned int mask, config = 1;
	SceKernelIntrStatus state;
	int result;

	if (IllegalContext()) {
		result = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto Return;
	}
	if (intrCode >= INTR_COUNT) {
		result = SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
		SCE_DBG_PRINTF(0xb, 390, INTR_TEXT(g_intrUnknownInterruptCode, "Unknown interrupt code. %d\n"), intrCode);
		goto Return;
	}
	if (type & ~0x11U) {
		result = SCE_KERNEL_ERROR_ILLEGAL_INTRTYPE;
		SCE_DBG_PRINTF(0xb, 395, INTR_TEXT(g_intrUnknownType, "Unknown type. %d\n"), type);
		goto Return;
	}
	if (!handler) {
		result = SCE_KERNEL_ERROR_ILLEGAL_HANDLER;
		SCE_DBG_PRINTF(0xb, 399, INTR_TEXT(g_intrInvalidHandlerAddress, "invalid handler address. %p\n"), handler);
		goto Return;
	}
	result = priority & ~0xf0U;
	if (result) {
		result = SCE_KERNEL_ERROR_ILLEGAL_INTRPRIORITY;
		SCE_DBG_PRINTF(0xb, 415, INTR_TEXT(g_intrInvalidPriority, "invalid priority. %d\n"), priority);
		goto Return;
	}
	if (TargetMask(target, &mask)) {
		result = SCE_KERNEL_ERROR_ILLEGAL_TARGET_CPU;
		SCE_DBG_PRINTF(0xb, 420, INTR_TEXT(g_intrInvalidTargetCpu, "invalid target cpu. %d\n"), target);
		goto Return;
	}
	if (opt) {
		if (opt->size != sizeof(*opt)) {
			result = SCE_KERNEL_ERROR_ILLEGAL_INTRPARAM;
			goto Return;
		}
		if (opt->attr & SCE_KERNEL_INTR_OPT_HAS_CONFIG) {
			config = opt->config;
			if ((config & ~2U) != SCE_KERNEL_INTR_CONFIG_LEVEL) {	/* level or edge */
				result = SCE_KERNEL_ERROR_ILLEGAL_INTRPARAM;
				goto Return;
			}
			SetConfiguration(intrCode, config);
		}
	}
	info = &g_intrHandlers[intrCode];
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&info->lock);
	if (info->handler) {
		result = SCE_KERNEL_ERROR_FOUND_HANDLER;
		SCE_DBG_PRINTF(0xb, 452, INTR_TEXT(g_intrAlreadyRegistered, "already registered: %d %s %p\n"),
			       intrCode, name, info->handler);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		goto Return;
	}
	if (name)
		SceSysmemForKernel_B628B46B(name, 0, &info->name);
	else
		info->name = NULL;
	info->handler = handler;
	info->priority = priority;
	info->intrCode = intrCode;
	info->type = type;
	info->arg = arg;
	memset(info->count, 0, sizeof(info->count));
	info->oldHandler = NULL;
	if (opt && opt->num) {
		unsigned int size = opt->num * sizeof(IntrHandler);
		info->sub = ksceKernelAllocHeapMemory(g_intrHeap, size);
		if (!info->sub) {
			result = SCE_KERNEL_ERROR_NO_MEMORY;
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
			goto Return;
		}
		info->callbacks = opt->handlers;
		info->subCount = opt->num;
		memset(info->sub, 0, size);
	} else {
		info->sub = NULL;
		info->callbacks = NULL;
		info->subCount = 0;
	}
	if ((int)intrCode > 15) {
		g_intrDistributor->priority[intrCode] = priority;
		g_intrDistributor->target[intrCode] = mask;
	}
	info->targetCpu = g_intrDistributor->target[intrCode];
	info->configuration = config;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
	if (g_intrRegistered)
		g_intrRegistered(intrCode);
Return:
	return result;
}

int sceKernelReleaseIntrHandler(unsigned int intrCode)
{
	IntrHandler *info;
	SceKernelIntrStatus state;

	if (IllegalContext())
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	if (intrCode >= INTR_COUNT) {
		SCE_DBG_PRINTF(0xb, 533, INTR_TEXT(g_intrUnknownInterruptCode, "Unknown interrupt code. %d\n"), intrCode);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	info = &g_intrHandlers[intrCode];
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&info->lock);
	if (!info->handler) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		SCE_DBG_PRINTF(0xb, 543, INTR_TEXT(g_intrInterruptHandlerIsNotRegistered, "interrupt handler is not registered. %d\n"), intrCode);
		return SCE_KERNEL_ERROR_NOTFOUND_HANDLER;
	}
	if (intrCode > 15 && info->priority > 63)
		g_intrDistributor->clearEnable[intrCode >> 5] = 1U << (intrCode & 31);
	info->handler = NULL;
	info->oldHandler = NULL;
	info->intrCode = 0;
	info->priority = 0;
	info->type = 0;
	info->arg = NULL;
	info->targetCpu = 0;
	info->configuration = 0;
	if (info->sub) {
		ksceKernelFreeHeapMemory(g_intrHeap, info->sub);
		info->sub = NULL;
		info->callbacks = NULL;
		info->subCount = 0;
	}
	SceSysmemForKernel_634D0D0C(info->name, 0);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
	if (g_intrReleased)
		g_intrReleased(intrCode);
	return 0;
}

int sceKernelRegisterIntrHookHandler(int intrCode, void *handler, void **oldHandler)
{
	IntrHandler *info;
	SceKernelIntrStatus state;

	if (IllegalContext())
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	if ((unsigned int)intrCode >= INTR_COUNT) {
		SCE_DBG_PRINTF(0xb, 599, INTR_TEXT(g_intrUnknownInterruptCode, "Unknown interrupt code. %d\n"), intrCode);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	if (!handler) {
		SCE_DBG_PRINTF(0xb, 603, INTR_TEXT(g_intrInvalidNewHandlerAddress, "invalid new handler address. %p\n"), handler);
		return SCE_KERNEL_ERROR_ILLEGAL_HANDLER;
	}
	if (!oldHandler) {
		SCE_DBG_PRINTF(0xb, 607, INTR_TEXT(g_intrInvalidOldHandlerAddress, "invalid old handler address. %p\n"), oldHandler);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	info = &g_intrHandlers[intrCode];
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&info->lock);
	if (!info->handler) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_NOTFOUND_HANDLER;
	}
	if (info->oldHandler) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_FOUND_HANDLER;
	}
	info->oldHandler = info->handler;
	*oldHandler = info->handler;
	info->handler = handler;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
	return 0;
}

int sceKernelReleaseIntrHookHandler(unsigned int intrCode)
{
	IntrHandler *info;
	SceKernelIntrStatus state;

	if (IllegalContext())
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	if (intrCode >= INTR_COUNT) {
		SCE_DBG_PRINTF(0xb, 643, INTR_TEXT(g_intrUnknownInterruptCode, "Unknown interrupt code. %d\n"), intrCode);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	info = &g_intrHandlers[intrCode];
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&info->lock);
	if (!info->handler || !info->oldHandler) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_NOTFOUND_HANDLER;
	}
	info->handler = info->oldHandler;
	info->oldHandler = NULL;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
	return 0;
}

int sceKernelEnableIntr(unsigned int intrCode)
{
	if (intrCode - 16 >= 240) {
		SCE_DBG_PRINTF(0xb, 670, INTR_TEXT(g_intrIllegalInterruptCode, "Illegal interrupt code. %d\n"), intrCode);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	if (!g_intrHandlers[intrCode].handler) {
		SCE_DBG_PRINTF(0xb, 674, INTR_TEXT(g_intrInterruptHandlerIsNotRegisteredIntrcode, "interrupt handler is not registered. intrcode %d\n"), intrCode);
		return SCE_KERNEL_ERROR_NOTFOUND_HANDLER;
	}
	g_intrDistributor->setEnable[intrCode >> 5] = 1U << (intrCode & 31);
	return 0;
}

int sceKernelDisableIntr(unsigned int intrCode)
{
	if (intrCode - 16 >= 240) {
		SCE_DBG_PRINTF(0xb, 687, INTR_TEXT(g_intrIllegalInterruptCode, "Illegal interrupt code. %d\n"), intrCode);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	if (!g_intrHandlers[intrCode].handler) {
		SCE_DBG_PRINTF(0xb, 691, INTR_TEXT(g_intrInterruptHandlerIsNotRegistered2, "interrupt handler is not registered\n"));
		return SCE_KERNEL_ERROR_NOTFOUND_HANDLER;
	}
	g_intrDistributor->clearEnable[intrCode >> 5] = 1U << (intrCode & 31);
	return 0;
}

int sceKernelSuspendIntr(unsigned int intrCode, unsigned int *enabled)
{
	unsigned int mask, active;

	if (intrCode - 16 >= 240) {
		SCE_DBG_PRINTF(0xb, 705, INTR_TEXT(g_intrIllegalInterruptCode, "Illegal interrupt code. %d\n"), intrCode);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	if (!enabled) {
		SCE_DBG_PRINTF(0xb, 709, INTR_TEXT(g_intrInvalidArgument, "Invalid argument\n"));
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	mask = 1U << (intrCode & 31);
	active = g_intrDistributor->setEnable[intrCode >> 5] & mask;
	if (active) {
		*enabled = 1;
		g_intrDistributor->clearEnable[intrCode >> 5] = mask;
		active = 0;
	} else {
		*enabled = active;
	}
	return active;
}

int sceKernelResumeIntr(unsigned int intrCode, unsigned int enabled)
{
	if (intrCode - 16 >= 240) {
		SCE_DBG_PRINTF(0xb, 743, INTR_TEXT(g_intrIllegalInterruptCode, "Illegal interrupt code. %d\n"), intrCode);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	if (enabled > 1) {
		SCE_DBG_PRINTF(0xb, 747, INTR_TEXT(g_intrInvalidArgument, "Invalid argument\n"));
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (enabled)
		g_intrDistributor->setEnable[intrCode >> 5] = 1U << (intrCode & 31);
	else
		g_intrDistributor->clearEnable[intrCode >> 5] = 1U << (intrCode & 31);
	return 0;
}

int sceKernelSetIntrPriority(unsigned int intrCode, unsigned int priority)
{
	IntrHandler *info;
	SceKernelIntrStatus state;
	int result;

	if (intrCode >= INTR_COUNT) {
		result = SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
		SCE_DBG_PRINTF(0xb, 776, INTR_TEXT(g_intrUnknownInterruptCode, "Unknown interrupt code. %d\n"), intrCode);
		goto Return;
	}
	if ((int)intrCode < 16) {
		result = SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
		SCE_DBG_PRINTF(0xb, 780, INTR_TEXT(g_intrCanTChangeSGIPriority, "Can't change SGI priority. %d\n"), intrCode);
		goto Return;
	}
	info = &g_intrHandlers[intrCode];
	if (!info->handler) {
		result = SCE_KERNEL_ERROR_NOTFOUND_HANDLER;
		SCE_DBG_PRINTF(0xb, 785, INTR_TEXT(g_intrInterruptHandlerIsNotRegistered2, "interrupt handler is not registered\n"));
		goto Return;
	}
	result = priority & ~0xf0U;
	if (result) {
		result = SCE_KERNEL_ERROR_ILLEGAL_INTRPRIORITY;
		SCE_DBG_PRINTF(0xb, 789, INTR_TEXT(g_intrInvalidPriority, "invalid priority. %d\n"), priority);
		goto Return;
	}
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&info->lock);
	info->priority = priority;
	((GicDistributor *)g_intrDistributor)->priority[intrCode] = priority;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
Return:
	return result;
}

int sceKernelGetIntrPriority(unsigned int intrCode, unsigned int *priority)
{
	if (intrCode >= INTR_COUNT) {
		SCE_DBG_PRINTF(0xb, 807, INTR_TEXT(g_intrUnknownInterruptCode, "Unknown interrupt code. %d\n"), intrCode);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	if (!g_intrHandlers[intrCode].handler) {
		SCE_DBG_PRINTF(0xb, 811, INTR_TEXT(g_intrInterruptHandlerIsNotRegistered2, "interrupt handler is not registered\n"));
		return SCE_KERNEL_ERROR_NOTFOUND_HANDLER;
	}
	*priority = g_intrHandlers[intrCode].priority;
	return 0;
}

int sceKernelSetIntrTargetCpu(unsigned int intrCode, unsigned int target)
{
	IntrHandler *info;
	unsigned int mask;
	SceKernelIntrStatus state;

	if (intrCode >= INTR_COUNT) {
		SCE_DBG_PRINTF(0xb, 833, INTR_TEXT(g_intrUnknownInterruptCode, "Unknown interrupt code. %d\n"), intrCode);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	info = &g_intrHandlers[intrCode];
	if (!info->handler) {
		SCE_DBG_PRINTF(0xb, 838, INTR_TEXT(g_intrInterruptHandlerIsNotRegistered2, "interrupt handler is not registered\n"));
		return SCE_KERNEL_ERROR_NOTFOUND_HANDLER;
	}
	if (TargetMask(target, &mask)) {
		SCE_DBG_PRINTF(0xb, 843, INTR_TEXT(g_intrInvalidTargetCpu, "invalid target cpu. %d\n"), target);
		return SCE_KERNEL_ERROR_ILLEGAL_TARGET_CPU;
	}
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&info->lock);
	info->targetCpu = mask;
	((GicDistributor *)g_intrDistributor)->target[intrCode] = mask;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
	return 0;
}

int sceKernelGetIntrTargetCpu(unsigned int intrCode, unsigned int *target)
{
	if (intrCode >= INTR_COUNT) {
		SCE_DBG_PRINTF(0xb, 860, INTR_TEXT(g_intrUnknownInterruptCode, "Unknown interrupt code. %d\n"), intrCode);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	if (!g_intrHandlers[intrCode].handler) {
		SCE_DBG_PRINTF(0xb, 864, INTR_TEXT(g_intrInterruptHandlerIsNotRegistered2, "interrupt handler is not registered\n"));
		return SCE_KERNEL_ERROR_NOTFOUND_HANDLER;
	}
	*target = g_intrHandlers[intrCode].targetCpu;
	return 0;
}

int sceKernelGenerateSoftIntr(unsigned int intrCode, unsigned int filter, unsigned int target)
{
	unsigned int mask;

	if (intrCode > 15) {
		SCE_DBG_PRINTF(0xb, 885, INTR_TEXT(g_intrUnknownInterruptCode, "Unknown interrupt code. %d\n"), intrCode);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	if (filter > 2) {
		SCE_DBG_PRINTF(0xb, 891, INTR_TEXT(g_intrUnknownFilter, "Unknown filter. %d\n"), filter);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRFILTER;
	}
	if (filter) {
		intrCode |= filter << 24;
	} else {
		if (TargetMask(target, &mask)) {
			SCE_DBG_PRINTF(0xb, 897, INTR_TEXT(g_intrUnknownTargetX, "Unknown target. 0x%x\n"), target);
			return SCE_KERNEL_ERROR_ILLEGAL_TARGET_CPU;
		}
		intrCode |= mask << 16;
	}
	g_intrDistributor->softIntr = intrCode;
	return 0;
}

int sceKernelGetIntrPendingStatusForDebugger(int intrCode)
{
	if ((unsigned int)intrCode >= INTR_COUNT) {
		SCE_DBG_PRINTF(0xb, 927, INTR_TEXT(g_intrUnknownInterruptCode, "Unknown interrupt code. %d\n"), intrCode);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	return g_intrDistributor->setPending[(unsigned int)intrCode >> 5] >> (intrCode & 31) & 1;
}

int sceKernelClearIntrPendingStatusForDebugger(int intrCode)
{
	if ((unsigned int)intrCode >= INTR_COUNT) {
		SCE_DBG_PRINTF(0xb, 937, INTR_TEXT(g_intrUnknownInterruptCode, "Unknown interrupt code. %d\n"), intrCode);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	g_intrDistributor->clearPending[(unsigned int)intrCode >> 5] = 1U << (intrCode & 31);
	return 0;
}

void *SceIntrmgrForKernel_807B2004(void) { return (void *)g_intrCpuInterface; }
int sceKernelRegisterSubIntrHandler(unsigned int intrCode, unsigned int subCode,
				   const char *name, void *handler, void *arg)
{
	IntrHandler *info, *sub;
	SceKernelIntrStatus state;
	int result;

	if (IllegalContext())
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	if (intrCode >= INTR_COUNT) {
		SCE_DBG_PRINTF(0xb, 967, INTR_TEXT(g_intrUnknownInterruptCode, "Unknown interrupt code. %d\n"), intrCode);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	if (!handler) {
		SCE_DBG_PRINTF(0xb, 971, INTR_TEXT(g_intrInvalidHandlerAddress2, "invalid handler address.\n"));
		return SCE_KERNEL_ERROR_ILLEGAL_HANDLER;
	}
	info = &g_intrHandlers[intrCode];
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&info->lock);
	if (!info->handler) {
		SCE_DBG_PRINTF(0xb, 992, INTR_TEXT(g_intrNotYetRegistered, "not yet registered. %d\n"), intrCode);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_NOTFOUND_HANDLER;
	}
	if (!info->callbacks) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_ERROR;
	}
	if ((int)subCode < 0 || info->subCount <= subCode) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	sub = &info->sub[subCode];
	if (sub->handler) {
		SCE_DBG_PRINTF(0xb, 1005, INTR_TEXT(g_intrAlreadyRegistered2, "already registered: %d-%d %s %p\n"),
			       intrCode, subCode, name, sub->handler);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_FOUND_HANDLER;
	}
	if (info->callbacks[1]) {
		result = info->callbacks[1](intrCode, subCode, handler, arg);
		if (result < 0) {
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
			return result;
		}
	}
	if (name)
		SceSysmemForKernel_B628B46B(name, 0, &sub->name);
	else
		sub->name = NULL;
	sub->handler = handler;
	sub->targetCpu = info->targetCpu;
	sub->priority = info->priority;
	sub->type = info->type;
	sub->arg = arg;
	sub->sub = NULL;
	sub->callbacks = NULL;
	sub->subCount = 0;
	memset(sub->count, 0, sizeof(sub->count));
	if (info->callbacks[2]) {
		result = info->callbacks[2](intrCode, subCode, handler, arg);
		if (result < 0) {
			sub->handler = NULL;
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
			return result;
		}
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
	return 0;
}

int sceKernelReleaseSubIntrHandler(int intrCode, int subCode)
{
	IntrHandler *info, *sub;
	SceKernelIntrStatus state;
	void *handler;
	SubIntrCallback callback;
	int result;

	if (IllegalContext())
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	if ((unsigned int)intrCode >= INTR_COUNT) {
		SCE_DBG_PRINTF(0xb, 1065, INTR_TEXT(g_intrUnknownInterruptCode, "Unknown interrupt code. %d\n"), intrCode);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	info = &g_intrHandlers[intrCode];
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&info->lock);
	if (!info->handler) {
		SCE_DBG_PRINTF(0xb, 1074, INTR_TEXT(g_intrInterruptHandlerIsNotRegistered, "interrupt handler is not registered. %d\n"), intrCode);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_NOTFOUND_HANDLER;
	}
	if (subCode < 0 || info->subCount <= (unsigned int)subCode) {
		SCE_DBG_PRINTF(0xb, 1079, INTR_TEXT(g_intrUnknownSubinterruptCode, "Unknown subinterrupt code. %d-%d\n"), intrCode, subCode);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	sub = &info->sub[subCode];
	handler = sub->handler;
	if (!handler) {
		SCE_DBG_PRINTF(0xb, 1086, INTR_TEXT(g_intrSubInterruptHandlerIsNotRegistered, "sub interrupt handler is not registered. %d-%d\n"),
			       intrCode, subCode);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
		return SCE_KERNEL_ERROR_NOTFOUND_HANDLER;
	}
	if (info->callbacks[3]) {
		result = info->callbacks[3](intrCode, subCode);
		if (result < 0) {
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
			return result;
		}
		handler = sub->handler;
	}
	callback = info->callbacks[4];
	sub->handler = NULL;
	if (callback) {
		result = callback(intrCode, subCode);
		if (result < 0) {
			sub->handler = handler;
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
			return result;
		}
	}
	SceSysmemForKernel_634D0D0C(sub->name, 0);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state);
	return 0;
}

#define SUB_INTR_OPERATION(_name, _slot, _codeLine, _subLine, _args, _nullCheck, ...) \
int _name(int intrCode, int subCode _args) \
{ \
	IntrHandler *info; \
	SceKernelIntrStatus state; \
	int result; \
	if ((unsigned int)intrCode >= INTR_COUNT) { \
		SCE_DBG_PRINTF(0xb, _codeLine, INTR_TEXT(g_intrUnknownInterruptCode, "Unknown interrupt code. %d\n"), intrCode); \
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE; \
	} \
	_nullCheck \
	info = &g_intrHandlers[intrCode]; \
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&info->lock); \
	if (subCode < 0 || (unsigned int)subCode >= info->subCount) { \
		SCE_DBG_PRINTF(0xb, _subLine, INTR_TEXT(g_intrUnknownSubinterruptCode, "Unknown subinterrupt code. %d-%d\n"), intrCode, subCode); \
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state); \
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE; \
	} \
	if (!info->handler || !info->callbacks || !info->callbacks[_slot]) { \
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state); \
		return SCE_KERNEL_ERROR_NOTFOUND_HANDLER; \
	} \
	result = info->callbacks[_slot](intrCode, subCode, ##__VA_ARGS__); \
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, state); \
	return result; \
}

#define SUB_STATE_ARG , unsigned int stateValue
#define SUB_OUT_ARG , unsigned int *enabled
SUB_INTR_OPERATION(sceKernelEnableSubIntr, 5, 1127, 1136, , )
SUB_INTR_OPERATION(sceKernelDisableSubIntr, 6, 1162, 1171, , )
SUB_INTR_OPERATION(sceKernelSuspendSubIntr, 7, 1197, 1215, SUB_OUT_ARG,
		   if (!enabled) return SCE_KERNEL_ERROR_ERROR;, enabled)
SUB_INTR_OPERATION(sceKernelResumeSubIntr, 8, 1244, 1253, SUB_STATE_ARG, , stateValue)
SUB_INTR_OPERATION(sceKernelIsSubInterruptOccurred, 9, 1279, 1288, , )

int sceKernelCallSubIntrHandler(int intrCode, int subCode, unsigned int value)
{
	IntrHandler *info, *sub;
	int (*handler)(unsigned int, void *, unsigned int);
	int result;
	void (*enter)(int, unsigned int, unsigned int, unsigned int, void *);

	if ((unsigned int)intrCode >= INTR_COUNT) {
		SCE_DBG_PRINTF(0xb, 1315, INTR_TEXT(g_intrUnknownInterruptCode, "Unknown interrupt code. %d\n"), intrCode);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	info = &g_intrHandlers[intrCode];
	if (subCode < 0 || info->subCount <= (unsigned int)subCode) {
		SCE_DBG_PRINTF(0xb, 1323, INTR_TEXT(g_intrUnknownSubinterruptCode, "Unknown subinterrupt code. %d-%d\n"), intrCode, subCode);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	sub = &info->sub[subCode];
	handler = sub->handler;
	if (!handler)
		return SCE_KERNEL_ERROR_NOTFOUND_HANDLER;
	sub->count[readMpidr() & 3]++;
	enter = g_intrEnterSubIntr;
	if (unlikely(enter != NULL))
		enter(4, intrCode, subCode, value, sub->arg);
	result = handler(value, sub->arg, (SceUInt8)info->priority);
	if (unlikely(g_intrLeaveSubIntr != NULL))
		((void (*)(int, unsigned int, unsigned int, unsigned int))g_intrLeaveSubIntr)
			(3, intrCode, subCode, value);
	if (checkStackGuard() < 0) {
		SCE_DBG_PRINTF(0xb, 1347,
			       INTR_TEXT(g_intrStackOverflowedIntrNameSubintrName, " stack overflowed. intr name [ %s (%d) ], subintr name [ %s (%d) ]\n"),
			       info->name, intrCode, sub->name, subCode);
		for (;;)
			;
	}
	return result;
}

int sceKernelAllocSystemCallTable(unsigned int count, void **table)
{
	int result;

	if (!count || g_intrSyscallBlock || g_intrSyscallTable)
		return -1;
	result = ksceKernelAllocPartitionMemBlock(SCE_KERNEL_PARTITION_ROOT_ID, INTR_TEXT(g_intrSceSyscallTable, "SceSyscallTable"), SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_RW, count * 4, NULL);
	if (result < 0) {
		SCE_DBG_PRINTF(6, 1446, INTR_TEXT(g_intrAllocateSystemCallTableFailedX, "allocate system call table failed. 0x%08x\n"), result);
		return result;
	}
	g_intrSyscallBlock = result;
	ksceKernelGetMemBlockBase(result, table);
	g_intrSyscallTable = *table;
	return 0;
}

int sceKernelQueryIntrHandlerInfo(unsigned int intrCode, unsigned int subCode,
				SceKernelIntrHandlerInfo *out)
{
	IntrHandler *info;
	unsigned int enabled, saved;

	if (intrCode >= INTR_COUNT) {
		SCE_DBG_PRINTF(0xb, 1467, INTR_TEXT(g_intrUnknownInterruptCode, "Unknown interrupt code. %d\n"), intrCode);
		return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
	}
	if (out->size != sizeof(*out))
		return SCE_KERNEL_ERROR_ILLEGAL_INTRPARAM;
	info = &g_intrHandlers[intrCode];
	if (info->subCount) {
		if ((int)subCode < 0) {
			if (subCode != ~0U)
				return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
		} else {
			if (subCode >= info->subCount)
				return SCE_KERNEL_ERROR_ILLEGAL_INTRCODE;
			if (info->sub)
				info = &info->sub[subCode];
			else
				subCode = 0;
		}
	}
	if (info->oldHandler)
		out->handler = info->oldHandler;
	else
		out->handler = info->handler;
	out->arg = info->arg;
	enabled = g_intrDistributor->clearEnable[intrCode >> 5] >> (intrCode & 31) & 1;
	out->intrCode = intrCode;
	out->type = info->type;
	out->priority = info->priority >> 4;
	out->targetCpu = info->targetCpu;
	out->enabled = enabled;
	if (enabled) {
		if ((int)subCode < 0)
			goto CopyCounts;
		if (!info->handler) {
			out->enabled = 0;
		} else if (info->callbacks && info->callbacks[7] && info->callbacks[8]) {
			info->callbacks[7](intrCode, subCode, &saved);
			info->callbacks[8](intrCode, subCode, saved);
			out->enabled = !!saved;
		}
	}
CopyCounts:
	out->subCount = info->subCount;
	__builtin_memcpy(out->count, info->count, sizeof(info->count));
	memset(out->reserved0, 0, sizeof(out->reserved0));
	memset(out->reserved1, 0, sizeof(out->reserved1));
	memset(out->reserved2, 0, sizeof(out->reserved2));
	if (out->name) {
		char *name = out->name;
		if (!info->name)
			name[0] = 0;
		else
			__builtin_memcpy(name, info->name, 32);
	}
	if (info->type & 0x10)
		out->typeFlag = 0x10;
	return 0;
}

void SceIntrmgrForKernel_ABEB5A69(void *handler) { g_intrKernelBreakpoints = handler; }
void SceIntrmgrForKernel_7E3D4560(void *handler) { g_intrKernelWatchpoints = handler; }

void intrStackOverflow(unsigned int depth, unsigned int interrupt)
{
	ksceKernelPrintf(INTR_TEXT(g_intrDetectIntrStackOverflow, "!!! detect intr stack overflow !!!\n"));
	ksceKernelPrintf(INTR_TEXT(g_intrCPUIntrNameXNestCount, "     CPU = %d, intr name %s (0x%08x:%d), nest count = %d\n"),
		ksceKernelCpuId(), g_intrHandlers[interrupt & 0x3ff].name,
		interrupt, interrupt & 0x3ff, depth);
	for (;;)
		;
}

static inline __attribute__((always_inline)) int Start(void)
{
	int result;
	SceKernelHeapCreateOpt options;

	result = ksceExcpmgrRegisterHandler(6, 0, interruptEntry);
	if (result < 0) {
		SCE_DBG_PRINTF(6, 1998, INTR_TEXT(g_intrRegisterExceptionHandlerFailedX, "register exception handler failed. 0x%08x\n"), result);
		return result;
	}
	options.size = sizeof(options);
	options.attr = 1;
	result = ksceKernelCreateHeap(INTR_TEXT(g_intrSceIntrMgr, "SceIntrMgr"), 0x8000, &options);
	if (result < 0) {
		SCE_DBG_PRINTF(0xb, 2012, INTR_TEXT(g_intrCreateHeapFailedX, "Create Heap failed. 0x%08x\n"), result);
		return result;
	}
	g_intrHeap = result;
	result = ksceExcpmgrRegisterHandler(2, 0, syscallEntry);
	if (result < 0) {
		SCE_DBG_PRINTF(6, 2024, INTR_TEXT(g_intrRegisterSyscallEntryFailedX, "register syscall entry failed. 0x%08x\n"), result);
		SCE_DBG_ASSERT_FAIL(2025);
	}
	SceKernelSuspendForDriver_CE7A2207(0x1d, SuspendController, NULL);
	SceKernelSuspendForDriver_105C5752(0x1d, ResumeController, NULL);
	g_intrSysroot = SceSysrootForKernel_118657C6();
	g_intrKernelBreakpoints = NULL;
	g_intrKernelWatchpoints = NULL;
	for (unsigned int i = 0; i < INTR_COUNT; i++) {
		g_intrHandlers[i].handler = NULL;
		g_intrHandlers[i].lock = 0;
	}
	return 0;
}

int sceKernelIntrMgrModuleStart(SceSize arglen, void *argp)
{
	SceKernelAllocMemBlockKernelOpt options = {
		.size = 0x58,
		.attr = 2,
		.paddr = PERIPH_REG_BASE,
	};
	void *base;
	int result, cpu;

	(void)arglen;
	(void)argp;
	result = ksceKernelAllocMemBlock(INTR_TEXT(g_intrSceInterruptControllerReg, "SceInterruptControllerReg"), SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_SO_RW, PERIPH_REG_SIZE, &options);
	if (result < 0) {
		SCE_DBG_PRINTF(0xb, 2250, INTR_TEXT(g_intrAllocateIntcRegisterAreaFailedX, "allocate intc register area failed. 0x%08x\n"), result);
		SCE_DBG_ASSERT_FAIL(2251);
		return result;
	}
	ksceKernelGetMemBlockBase(result, &base);
	g_intrCpuInterface = (GicCpuInterface *)((char *)base + GIC_CPU_INTERFACE_OFFSET);
	g_intrDistributor = (GicDistributor *)((char *)base + GIC_DISTRIBUTOR_OFFSET);
	ResetController();
	cpu = ksceKernelCpuId();
	g_intrCpu[cpu].nesting = 0;
	if (likely(ksceKernelCpuId() == 0)) {
		result = Start();
		if (result < 0) {
			SCE_DBG_ASSERT(0, 2264);
			return result;
		}
	}
	sceKernelSysrootSetModulePrivate(5, g_intrHandlers);
	g_intrEnterSyscall = NULL;
	g_intrLeaveSyscall = NULL;
	g_intrEnterIntr = NULL;
	g_intrLeaveIntr = NULL;
	g_intrEnterSubIntr = NULL;
	g_intrLeaveSubIntr = NULL;
	initStackGuards(0x1ab4e8df);
	return 0;
}

int module_bootstart(SceSize arglen, void *argp)
{
	char *base = ksceKernelSysrootGetVbaseResetVector();
	IntrCpuState *volatile state;

	(void)arglen;
	(void)argp;
	base += ksceKernelCpuId() * 0x40;
	state = &g_intrCpu[ksceKernelCpuId()];
	/* the vector page word 0x28 of this core points at its nesting state */
	PatchCode(base + 0x28, (const void *)&state, sizeof(state));
	return 0;
}

void invalidApiStatCfunc(const ThreadSyscallFrame *frame, void *result, unsigned int state)
{
	(void)result;
	SCE_DBG_PRINTF(0, 2321,
		INTR_TEXT(g_intrWarningDetectInvalidApiStatXLrX, "warning: detect invalid api stat(0x%08X), lr = 0x%08X, syscall_entry 0x%08x\n"),
		state, frame->pc, frame->entry);
}

void invalidFastMutexOwnerCfunc(void)
{
	ThreadCB *thread = getCurrentThreadCB();
	struct ThreadmgrFastMutex *mutex;

	SCE_DBG_PRINTF(0, 2333, INTR_TEXT(g_intrFastMutexHasNotBeenUnlockedAfterSyscall, "\n*** FastMutex has not been unlocked after syscall.\n"));
	SCE_DBG_PRINTF(0, 2334, INTR_TEXT(g_intrProcessIDX, "** ProcessID: 0x%08X [%s]\n"),
		thread->processId, ksceGUIDName(thread->processId));
	SCE_DBG_PRINTF(0, 2335, INTR_TEXT(g_intrThreadIDX, "** ThreadID:  0x%08X [%s]\n"),
		thread->threadId, ksceGUIDName(thread->threadId));
	for (mutex = (struct ThreadmgrFastMutex *)thread->ownedFastMutexNext;
	     mutex != (struct ThreadmgrFastMutex *)&thread->ownedFastMutexNext;
	     mutex = mutex->next)
		SCE_DBG_PRINTF(0, 2338, INTR_TEXT(g_intrFastMutexX, "*  FastMutex: 0x%08X [%s]\n"),
			mutex->uid, ksceGUIDName(mutex->uid));
	SCE_PANIC(2341);
}

void illegalUserDACRCfunc(const ThreadCB *thread, unsigned int dacr)
{
	SCE_DBG_PRINTF(0, 2350, INTR_TEXT(g_intrIllegalUserDACRX, "\n*** Illegal User DACR = 0x%08X.\n"), dacr);
	SCE_DBG_PRINTF(0, 2351, INTR_TEXT(g_intrProcessIDX, "** ProcessID: 0x%08X [%s]\n"),
		thread->processId, ksceGUIDName(thread->processId));
	SCE_DBG_PRINTF(0, 2352, INTR_TEXT(g_intrThreadIDX, "** ThreadID:  0x%08X [%s]\n"),
		thread->threadId, ksceGUIDName(thread->threadId));
	SCE_PANIC(2354);
}

/* name unknown: validate the acknowledged GIC interrupt before CPU entry. */
IntrHandler *intrLookupInterrupt(SceUInt32 *acknowledge)
{
	unsigned int code;
	IntrHandler *info;

	*acknowledge = g_intrCpuInterface->acknowledge;
	code = *acknowledge & 0x3ff;
	if (code >= INTR_COUNT)
		return NULL;
	info = &g_intrHandlers[code];
	return info->handler ? info : NULL;
}

/* name unknown: invoke the registered handler with nested IRQs enabled. */
int intrDispatchInterrupt(IntrHandler *info)
{
	int (*handler)(unsigned int, void *, unsigned int);
	int result;
	SceUInt32 apiStat = 0;

	writeTpidruro(apiStat);
	enableIrq();
	handler = info->handler;
	result = handler(info->intrCode, info->arg, info->priority);
	disableIrq();
	return result;
}

/* name unknown: choose a nesting level or the current thread's saved context.
 * Called while still in IRQ mode, before exposing the banked user SP to C.
 */
unsigned int intrEnterInterrupt(IntrHandler *info, const IntrIrqFrame *frame)
{
	unsigned int cpu = readMpidr() & 15;
	unsigned int nesting;

	info->count[cpu]++;
	nesting = ++g_intrCpu[cpu].nesting;
	if ((int)(nesting - 2) < 0 && frame->thread)
		return 0;
	return nesting;
}

/* name unknown: finish one interrupt before deciding whether to reschedule. */
unsigned int intrCompleteInterrupt(int result, unsigned int acknowledge)
{
	unsigned int intrCode = acknowledge & ~0x1c00U;
	unsigned int cpu, nesting;

	if (result != -1 && intrCode > 31)
		g_intrDistributor->clearEnable[intrCode >> 5] =
			1U << (intrCode & 31);
	g_intrCpuInterface->endOfIntr = acknowledge;
	cpu = readMpidr() & 15;
	nesting = g_intrCpu[cpu].nesting;
	if (*StackGuard(cpu, nesting) != INTR_STACK_MAGIC) {
		intrStackOverflow(nesting, acknowledge);
		for (;;)
			;
	}
	g_intrCpu[cpu].nesting = --nesting;
	return nesting;
}
