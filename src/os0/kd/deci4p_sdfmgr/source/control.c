/* Per-core transport, hook and control-component registration. */
#include "sdfmgr_internal.h"
#include <sce/intrmgr.h>
#include <sce/sysroot.h>
#include <sce/excpmgr_cpu.h>
#include <sce/deci4p_packet.h>
#include <sce/deci4p_memory.h>
#include <psp2kern/kernel/cpu/spinlock.h>
#include <psp2kern/kernel/sysmem/memtype.h>

int ksceKernelSignalSema(SceUID semaphore, int count);
int ksceKernelSetEventFlag(SceUID eventFlag, uint32_t pattern);
int ksceSysrootIsManufacturingMode(void);	/* SceSysrootForKernel 0x55392965 */

#define SDFMGR_INVALID_ARGUMENT ((int)0x80090001)
#define SDFMGR_NO_SLOT ((int)0x80090003)

const uint32_t s_sdfMgrCpuTargets[4] = { 1, 2, 4, 8 };

/* The native startup paths call this exported fill routine out of line. */
__attribute__((noinline))
void SceDeci4pSDfMgrForKernel_8A3B06F2(void *destination, int value, SceSize size)
{
	deci4pFillMemory(destination, value, size);
}

void SceDeci4pSDfMgrForKernel_81E7C5BD(void *destination, uint32_t value,
				   SceSize size)
{
	volatile uint32_t *words = destination;
	uint32_t pattern = value | value << 8 | value << 16 | value << 24;
	SceSize i;

	for (i = 0; i != size >> 2; i++)
		*words++ = pattern;
}

void SceDeci4pSDfMgrForKernel_5996E2DB(void *destination, const void *source,
				   SceSize size)
{
	deci4pCopyBytes(destination, source, size);
}

int module_start(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
#if FW_DEBUG
	ksceKernelPrintfLevelWithInfo(2, 15,
		SDFMGR_DEBUG_INFO(198, 0x31602988, 0x93c704bf,
				 "sceSDfMgrModuleStart"), "\n");
#endif
	if (!ksceKernelCheckDipsw(0xc1)) {
#if FW_DEBUG
		ksceKernelPrintfLevelWithInfo(1, 11,
			SDFMGR_DEBUG_INFO(202, 0x0f5d7c87, 0x93c704bf,
					 "sceSDfMgrModuleStart"), "disabled by dipsw\n");
#endif
		return 1;
	}
	SceDeci4pSDfMgrForKernel_8A3B06F2(&s_sdfMgrState.global, 0,
					 sizeof(s_sdfMgrState.global));
	s_sdfMgrState.global.phase = 11;
	return 0;
}

/* Inferred names: the exception-return assembly writes BVR1/BCR1 with these. */
uint32_t sdfMgrGetResumeBreakpointValue(void)
{
	unsigned cpu = ksceKernelCpuId();
	SdfMgrDebugCpuState *debugCpu = s_sdfMgrState.debugCpu;
	SdfMgrDebugCpuState *state;

	if (!debugCpu)
		return 0;
	state = &debugCpu[cpu];
	if (state->stepMode == SDFMGR_STEP_ARM || state->stepMode == SDFMGR_STEP_ARM_POLL)
		return state->pc;
	if (state->stepMode == SDFMGR_STEP_THUMB || state->stepMode == SDFMGR_STEP_THUMB_POLL)
		return state->stepAddress;
	return 0;
}

uint32_t sdfMgrTakeResumeBreakpointControl(void)
{
	unsigned cpu = ksceKernelCpuId();
	SdfMgrDebugCpuState *debugCpu = s_sdfMgrState.debugCpu;
	SdfMgrDebugCpuState *state;
	uint32_t control = 0;

	if (debugCpu) {
		state = &debugCpu[cpu];
		if (state->stepMode == SDFMGR_STEP_ARM || state->stepMode == SDFMGR_STEP_ARM_POLL)
			control = 0x004001e1;
		else if (state->stepMode == SDFMGR_STEP_THUMB || state->stepMode == SDFMGR_STEP_THUMB_POLL)
			control = state->stepAddress & 2 ? 0x181 : 0x61;
	}
	/* Native code clears the captured array even when its base is NULL. */
	cpu = ksceKernelCpuId();
	state = &debugCpu[cpu];
	state->stepMode = 0;
	return control;
}

SceSize SceDeci4pSDfMgrForKernel_68903C1D(const SceDeci4pHeader *header)
{
	return deci4pGetSize(header);
}

void SceDeci4pSDfMgrForKernel_FFC95B63(SceDeci4pHeader *header, SceSize size)
{
	deci4pSetSize(header, size);
}

/* Names inferred from the registered suspend/resume callbacks. */
int sdfMgrSuspend(void)
{
	SdfMgrTransport *transport = s_sdfMgrState.cpu[sdfMgrCoreIndex()].transport;

	sceKernelSysrootCorelockLock(0);
	dcmpSendStatusPower(0x8e, 0, 0);
	if (ksceSysrootIsManufacturingMode()) {
		sdfMgrDisconnect();
	} else {
		while (s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 2)
			transport->processReceive(transport);
	}
	sceKernelSysrootCorelockUnlock();
	sceKernelSysrootCorelockLock(0);
	if (!ksceKernelCpuId())
		transport->disableInterrupts(transport);
	sceKernelSysrootCorelockUnlock();
	return 0;
}

int sdfMgrResume(void)
{
	SdfMgrCpuState *cpu;
	SdfMgrTransport *transport;
	unsigned retries = 1000000;

	if (ksceKernelCheckDipsw(0xe4)) {
		unsigned debugControl = CP_READ(p14, 0, c0, c2, 2);

		if (!(debugControl & 0x8000))
			CP_WRITE(p14, 0, c0, c2, 2, debugControl | 0x8000);
	}
	sceKernelSysrootCorelockLock(0);
	cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	if (!ksceKernelCpuId()) {
		transport = s_sdfMgrState.cpu[sdfMgrCoreIndex()].transport;
		transport->enableInterrupts(transport);
	}
	sceKernelSysrootCorelockUnlock();
	dcmpSendStatusPower(0x0e, 1, 0);
	if (ksceSysrootIsManufacturingMode())
		sdfMgrConnect();
	transport = cpu->transport;
	transport->poll(transport);
	while (!(s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 2)) {
		transport = cpu->transport;
		transport->poll(transport);
		if (!--retries) {
			sdfMgrConnect();
			break;
		}
	}
	return 0;
}

/* Inferred name: Sysroot registers this native CPU0 power-status callback. */
void sdfMgrReportPowerState(int state)
{
	unsigned intr = ksceKernelCpuSuspendIntr();

	if (!ksceKernelCpuId()) {
		if (!state) {
			dcmpSendStatusPower(0, 1, 0);
		} else if (s_sdfMgrState.global.phase <= 49) {
			dcmpSendStatusPower(0, 2, s_sdfMgrState.global.phase);
			s_sdfMgrState.global.phase =
				(uint32_t)s_sdfMgrState.global.phase + 1;
		}
	}
	ksceKernelCpuResumeIntr(intr);
}

int sdfMgrUpdateGlobalFlags(uint32_t mask, int enabled)
{
	SceKernelIntrStatus intr = ksceKernelSpinlockLowLockCpuSuspendIntr(
							&s_sdfMgrState.lock);

	if (enabled)
		s_sdfMgrState.global.flags |= mask;
	else
		s_sdfMgrState.global.flags &= ~mask;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_sdfMgrState.lock, intr);
	return 0;
}

void sdfMgrPollCurrentBreakHooks(void)
{
	sdfMgrPollBreakHooks(&s_sdfMgrState.cpu[sdfMgrCoreIndex()]);
}

int _threadKicker(void)
{
	unsigned cpu = sdfMgrCoreIndex();
	unsigned index;

	sdfMgrUpdateCpuFlags(8, 0);
	for (index = 0; index < 8; index++) {
		SdfMgrProtocolState *protocol = &s_sdfMgrState.cpu[cpu].protocols[index];
		int result;

		if (!protocol->handler)
			continue;
		if (protocol->semaphore > 0) {
			result = ksceKernelSignalSema(protocol->semaphore,
						      protocol->semaphoreCount);
#if FW_DEBUG
			if (result < 0)
				SceDeci4pSDfMgrForKernel_501CE61D(15,
					SDFMGR_DEBUG_INFO(1833, 0x70c17232, 0xb9a27ce2,
							"_threadKicker"),
					"sceKernelSignalSema(0x%08X,%d) failed: 0x%08X\n",
					protocol->semaphore, protocol->semaphoreCount, result);
#else
			(void)result;
#endif
			protocol->semaphore = 0;
			protocol->semaphoreCount = 0;
		}
		if (protocol->eventFlag > 0) {
			result = ksceKernelSetEventFlag(protocol->eventFlag,
							protocol->eventPattern);
#if FW_DEBUG
			if (result < 0)
				SceDeci4pSDfMgrForKernel_501CE61D(15,
					SDFMGR_DEBUG_INFO(1843, 0x361e1a6e, 0xb9a27ce2,
							"_threadKicker"),
					"sceKernelSetEventFlag(0x%08X,0x%08X) failed: 0x%08X\n",
					protocol->eventFlag, protocol->eventPattern, result);
#else
			(void)result;
#endif
			protocol->eventFlag = 0;
			protocol->eventPattern = 0;
		}
	}
	return -1;
}

SdfMgrDebugCpuState **SceDeci4pSDfMgrForKernel_BFB482D8(void)
{
	return &s_sdfMgrState.debugCpu;
}

void SceDeci4pSDfMgrForKernel_4EA65698(uint32_t value)
{
	SdfMgrDebugCpuState *debugCpu = s_sdfMgrState.debugCpu;

	debugCpu[ksceKernelCpuId()].stepMode = value;
}

void SceDeci4pSDfMgrForKernel_11340BD4(void)
{
	SdfMgrDebugCpuState *debugCpu = s_sdfMgrState.debugCpu;

	debugCpu[ksceKernelCpuId()].stepMode = 0;
}

uint32_t SceDeci4pSDfMgrForKernel_C38EA67A(void)
{
	SdfMgrDebugCpuState *debugCpu = s_sdfMgrState.debugCpu;

	if (!debugCpu)
		return 0;
	return debugCpu[ksceKernelCpuId()].stepMode;
}

uint32_t SceDeci4pSDfMgrForKernel_0A6437B6(void)
{
	return s_sdfMgrState.global.flags & 1;
}

int sceSDfMgrPoll(void)
{
	int result = ksceKernelTriggerSGI(1, 0, s_sdfMgrCpuTargets[ksceKernelCpuId()]);

#if FW_DEBUG
	if (result < 0) {
		static const SceKernelDebugInfo info = {
			.fileHash = 0x6364a003,
			.lineHash = 0x416dabb6,
			.funcHash = 0x534b252d,
			.file = "sdfmgr.c",
			.line = 1044,
			.func = "sceSDfMgrPoll",
		};

		return SceDeci4pSDfMgrForKernel_501CE61D(15, &info,
					 "generate SGI failed. (0x%08x)\n", result);
	}
#endif
	return result;
}

uint32_t sdfMgrGetCpuFlags(uint32_t mask)
{
	return s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & mask;
}

void sdfMgrPollBreakHooks(SdfMgrCpuState *cpu)
{
	SdfMgrTransport *transport;
	unsigned i;

	for (i = 0; i < 4; i++) {
		if (cpu->breakHooks[i])
			cpu->breakHooks[i]();
	}
	transport = cpu->transport;
	transport->poll(transport);
}

void sdfMgrHandleControlInterrupt(void)
{
	SdfMgrCpuState *cpu;

	if (!sceKernelGetIntrPendingStatusForDebugger(0))
		return;
	cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	if (s_sdfMgrState.global.flags & 1) {
		if (!(s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 4)) {
			sdfMgrUpdateCpuFlags(4, 1);
			_execControlComponent(cpu, 1);
		}
	} else if (s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 4) {
		sdfMgrUpdateCpuFlags(4, 0);
		_execControlComponent(cpu, 0);
	}
	sceKernelClearIntrPendingStatusForDebugger(0);
}

int sdfMgrWaitForControl(void)
{
	SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];

	if (s_sdfMgrState.global.flags & 1) {
		if (!(s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 4)) {
			sdfMgrUpdateCpuFlags(4, 1);
			_execControlComponent(cpu, 1);
		}
	} else if (s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 4) {
		sdfMgrUpdateCpuFlags(4, 0);
		_execControlComponent(cpu, 0);
	}
	sceKernelClearIntrPendingStatusForDebugger(0);
	while (s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 4)
		sdfMgrPollBreakHooks(cpu);
	return -1;
}

int sdfMgrUpdateCpuFlags(uint32_t mask, int enabled)
{
	unsigned state = ksceKernelCpuSuspendIntr();
	SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];

	if (enabled)
		cpu->flags |= mask;
	else
		cpu->flags &= ~mask;
	ksceKernelCpuResumeIntr(state);
	return 0;
}

int sdfMgrSetTransport(void *unused, SdfMgrTransport *transport)
{
	(void)unused;
	s_sdfMgrState.cpu[sdfMgrCoreIndex()].transport = transport;
	return 0;
}

int sdfMgrSetProtocolStatus(SdfMgrProtocolState *protocol, uint32_t mask,
			    int enabled)
{
	unsigned state = ksceKernelCpuSuspendIntr();

	if (enabled)
		protocol->flags |= mask;
	else
		protocol->flags &= ~mask;
	ksceKernelCpuResumeIntr(state);
	return 0;
}

int SceDeci4pSDfMgrForKernel_682D0EFB(void)
{
	sdfMgrUpdateCpuFlags(4, 1);
	return 0;
}

int SceDeci4pSDfMgrForKernel_58D7597A(void)
{
	sdfMgrUpdateCpuFlags(4, 0);
	return 0;
}

void _execControlComponent(SdfMgrCpuState *cpu, unsigned id)
{
#if FW_DEBUG
	if (id > 1) {
		static const SceKernelDebugInfo info = {
			.fileHash = 0x6364a003,
			.lineHash = 0xcd1b5094,
			.funcHash = 0xf7048dcc,
			.file = "sdfmgr.c",
			.line = 2758,
			.func = "_execControlComponent",
		};

		SceDeci4pSDfMgrForKernel_501CE61D(15, &info,
						 "unknown control ID (%d)\n", id);
		return;
	}
#endif
	if (cpu->controlComponents[0])
		cpu->controlComponents[0](id);
	if (cpu->controlComponents[1])
		cpu->controlComponents[1](id);
}

void SceDeci4pSDfMgrForKernel_98AEC60E(void)
{
	SdfMgrTransport *transport = s_sdfMgrState.cpu[sdfMgrCoreIndex()].transport;

	transport->poll(transport);
}

SceUInt32 SceDeci4pSDfMgrForKernel_35238B8A(void)
{
	return ksceKernelCheckDipsw(0xd2) ? SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW : SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_GAME_RW;
}

int SceDeci4pSDfMgrForKernel_6BF5553C(SdfMgrHook hook, unsigned index)
{
	SdfMgrCpuState *cpu;

	if (index >= 4)
		return SDFMGR_INVALID_ARGUMENT;
	cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	if (cpu->indexedHooks[index])
		return SDFMGR_NO_SLOT;
	cpu->indexedHooks[index] = hook;
	return 0;
}

int SceDeci4pSDfMgrForKernel_B050A0D5(SdfMgrHook hook, unsigned index)
{
	SdfMgrCpuState *cpu;

	if (index >= 4)
		return SDFMGR_INVALID_ARGUMENT;
	cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	if (cpu->indexedHooks[index] != hook)
		return SDFMGR_INVALID_ARGUMENT;
	cpu->indexedHooks[index] = NULL;
	return 0;
}

int SceDeci4pSDfMgrForKernel_B22B42FB(SdfMgrHook hook)
{
	unsigned state = ksceKernelCpuSuspendIntr();
	SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	unsigned i;

	for (i = 0; i < 4; i++) {
		if (!cpu->breakHooks[i]) {
			cpu->breakHooks[i] = hook;
			ksceKernelCpuResumeIntr(state);
			return 0;
		}
	}
	ksceKernelCpuResumeIntr(state);
	return SDFMGR_NO_SLOT;
}

int SceDeci4pSDfMgrForKernel_E53400AE(SdfMgrHook hook)
{
	unsigned state = ksceKernelCpuSuspendIntr();
	SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	unsigned i;

	for (i = 0; i < 4; i++) {
		if (cpu->breakHooks[i] == hook) {
			cpu->breakHooks[i] = NULL;
			ksceKernelCpuResumeIntr(state);
			return 0;
		}
	}
	ksceKernelCpuResumeIntr(state);
	return SDFMGR_INVALID_ARGUMENT;
}

int SceDeci4pSDfMgrForKernel_3A25594F(SdfMgrControl control)
{
	unsigned state = ksceKernelCpuSuspendIntr();
	SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	unsigned index = 0;

	if (cpu->controlComponents[0]) {
		if (cpu->controlComponents[1]) {
			ksceKernelCpuResumeIntr(state);
			return SDFMGR_NO_SLOT;
		}
		index = 1;
	}
	cpu->controlComponents[index] = control;
	ksceKernelCpuResumeIntr(state);
	return 0;
}

int SceDeci4pSDfMgrForKernel_59C077B3(SdfMgrControl control)
{
	unsigned state = ksceKernelCpuSuspendIntr();
	SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	unsigned index;

	if (cpu->controlComponents[0] == control)
		index = 0;
	else if (cpu->controlComponents[1] == control)
		index = 1;
	else {
		ksceKernelCpuResumeIntr(state);
		return SDFMGR_INVALID_ARGUMENT;
	}
	cpu->controlComponents[index] = NULL;
	ksceKernelCpuResumeIntr(state);
	return 0;
}
