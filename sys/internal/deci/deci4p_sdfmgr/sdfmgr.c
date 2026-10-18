/* SceDeci4pSDfMgr: Sony's sdfmgr.c translation unit. */
#include <psp2/types.h>
#include "sdfmgr_internal.h"
#include <intrmgr/intrmgr.h>
#include <sysmem/sysroot.h>
#include <excpmgr/excpmgr_cpu.h>
#include <deci4p_dfmgr/deci4p_packet.h>
#include <deci4p_sdfmgr/deci4p_memory.h>
#include <sysmem/cpu.h>
#include <excpmgr/excpmgr.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/suspend.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/threadmgr/debugger.h>
#include <threadmgr/arm_thread_context.h>
#include <excpmgr/arm_psr_text.h>
#include <modulemgr/modulemgr_control.h>
#include <pamgr/coresight_regs.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/threadmgr/semaphores.h>
#include <modulemgr/modulemgr.h>

SdfMgrState s_sdfMgrState;

/* Per-core transport, hook and control-component registration. */

#define SDFMGR_INVALID_ARGUMENT ((int)0x80090001)
#define SDFMGR_NO_SLOT ((int)0x80090003)

const uint32_t s_sdfMgrCpuTargets[4] = { 1, 2, 4, 8 };

/* The native startup paths call this exported fill routine out of line. */
__attribute__((noinline))
void SceDeci4pSDfMgrForKernel_8A3B06F2(void *destination, int value, SceSize size)
{
	deci4pFillMemory(destination, value, size);
}

void *SceDeci4pSDfMgrForKernel_81E7C5BD(void *destination, uint32_t value,
				   SceSize size)
{
	uint32_t *words = destination;
	uint32_t pattern = value << 16 | value << 24 | value | value << 8;
	SceSize i;

	for (i = 0; i != size >> 2; i++)
		*words++ = pattern;
	return destination;
}

__attribute__((noinline))
void *SceDeci4pSDfMgrForKernel_5996E2DB(void *destination, const void *source,
				   SceSize size)
{
	deci4pCopyBytes(destination, source, size);
	return destination;
}

int module_start(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
#if FW_DEBUG
	SceDebugForDriver_D9703808(2, 15,
		SDFMGR_DEBUG_INFO(198, 0x31602988, 0x93c704bf,
				 "sceSDfMgrModuleStart"), "\n");
#endif
	if (!SceDipswForDriver_A98FC2FD(0xc1)) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 11,
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
	unsigned cpu = sceKernelCpuId();
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
	unsigned cpu = sceKernelCpuId();
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
	cpu = sceKernelCpuId();
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

void __attribute__((noinline)) sdfMgrDisconnect(void)
{
	DcmpConnectionStatus packet;
	unsigned i;

	sdfMgrUpdateCpuFlags(2, 0);
	dcmpMakeStatusConnection(0, &packet);
	for (i = 2; i < 8; i++) {
		SdfMgrProtocolState *protocol = _getPdCB(i);

		if (protocol->flags & 1)
			protocol->handler(5, &packet, protocol->context);
	}
}

/* Names inferred from the registered suspend/resume callbacks. */
int sdfMgrSuspend(void)
{
	SdfMgrTransport *transport = s_sdfMgrState.cpu[sdfMgrCoreIndex()].transport;

	sceKernelSysrootCorelockLock(0);
	dcmpSendStatusPower(0x8e, 0, 0);
	if (SceSysrootForKernel_55392965()) {
		sdfMgrDisconnect();
	} else {
		while (s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 2)
			transport->operations.processReceive(transport);
	}
	sceKernelSysrootCorelockUnlock();
	sceKernelSysrootCorelockLock(0);
	if (!sceKernelCpuId())
		transport->operations.disableInterrupts(transport);
	sceKernelSysrootCorelockUnlock();
	return 0;
}

void __attribute__((noinline)) sdfMgrReportProtocols(void)
{
	unsigned i;

	for (i = 2; i < 8; i++) {
		SdfMgrProtocolState *protocol = _getPdCB(i);
		uint32_t flags = protocol->flags;
		uint32_t status;

		if (!(flags & 1))
			continue;
		status = flags & 2 ? 3 : 1;
		if (protocol->registration.flags & 2)
			status |= 4;
		dcmpSendStatusProtocol(status, &protocol->registration);
		if (protocol->registration.flags & 2)
			dcmpSendStatusProtocolAlias(&protocol->registration);
	}
}

void sdfMgrConnect(void)
{
	DcmpConnectionStatus packet;
	unsigned i;

	sdfMgrUpdateCpuFlags(2, 1);
	sdfMgrReportProtocols();
	dcmpMakeStatusConnection(1, &packet);
	for (i = 2; i < 8; i++) {
		SdfMgrProtocolState *protocol = _getPdCB(i);

		if (protocol->flags & 1)
			protocol->handler(5, &packet, protocol->context);
	}
}

int sdfMgrResume(void)
{
	SdfMgrCpuState *cpu;
	SdfMgrTransport *transport;
	unsigned retries = 1000000;

	if (SceDipswForDriver_A98FC2FD(0xe4)) {
		unsigned debugControl = CP_READ(p14, 0, c0, c2, 2);

		if (!(debugControl & 0x8000))
			CP_WRITE(p14, 0, c0, c2, 2, debugControl | 0x8000);
	}
	sceKernelSysrootCorelockLock(0);
	cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	if (!sceKernelCpuId()) {
		transport = s_sdfMgrState.cpu[sdfMgrCoreIndex()].transport;
		transport->operations.enableInterrupts(transport);
	}
	sceKernelSysrootCorelockUnlock();
	dcmpSendStatusPower(0x0e, 1, 0);
	if (SceSysrootForKernel_55392965())
		sdfMgrConnect();
	transport = cpu->transport;
	transport->operations.poll(transport);
	while (!(s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 2)) {
		transport = cpu->transport;
		transport->operations.poll(transport);
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
	unsigned intr = SceCpuForDriver_821FC0EE();

	if (!sceKernelCpuId()) {
		if (!state) {
			dcmpSendStatusPower(0, 1, 0);
		} else if (s_sdfMgrState.global.phase <= 49) {
			dcmpSendStatusPower(0, 2, s_sdfMgrState.global.phase);
			s_sdfMgrState.global.phase =
				(uint32_t)s_sdfMgrState.global.phase + 1;
		}
	}
	sceKernelCpuResumeIntr(intr);
}

int sdfMgrUpdateGlobalFlags(uint32_t mask, int enabled)
{
	SceKernelIntrStatus intr = SceCpuForDriver_D32ACE9E(
							&s_sdfMgrState.lock);

	if (enabled)
		s_sdfMgrState.global.flags |= mask;
	else
		s_sdfMgrState.global.flags &= ~mask;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_sdfMgrState.lock, intr);
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
			result = sceKernelSignalSema(protocol->semaphore,
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
			result = sceKernelSetEventFlag(protocol->eventFlag,
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

	debugCpu[sceKernelCpuId()].stepMode = value;
}

void SceDeci4pSDfMgrForKernel_11340BD4(void)
{
	SdfMgrDebugCpuState *debugCpu = s_sdfMgrState.debugCpu;

	debugCpu[sceKernelCpuId()].stepMode = 0;
}

uint32_t SceDeci4pSDfMgrForKernel_C38EA67A(void)
{
	SdfMgrDebugCpuState *debugCpu = s_sdfMgrState.debugCpu;

	if (!debugCpu)
		return 0;
	return debugCpu[sceKernelCpuId()].stepMode;
}

uint32_t SceDeci4pSDfMgrForKernel_0A6437B6(void)
{
	return s_sdfMgrState.global.flags & 1;
}

int sceSDfMgrPoll(void)
{
	int result = sceKernelGenerateSoftIntr(1, 0, s_sdfMgrCpuTargets[sceKernelCpuId()]);

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
	transport->operations.poll(transport);
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
	unsigned state = SceCpuForDriver_821FC0EE();
	SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];

	if (enabled)
		cpu->flags |= mask;
	else
		cpu->flags &= ~mask;
	sceKernelCpuResumeIntr(state);
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
	unsigned state = SceCpuForDriver_821FC0EE();

	if (enabled)
		protocol->flags |= mask;
	else
		protocol->flags &= ~mask;
	sceKernelCpuResumeIntr(state);
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

__attribute__((noinline)) void _execControlComponent(SdfMgrCpuState *cpu, unsigned id)
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

	transport->operations.poll(transport);
}

SceUInt32 SceDeci4pSDfMgrForKernel_35238B8A(void)
{
	return SceDipswForDriver_A98FC2FD(0xd2) ? SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW : SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_GAME_RW;
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
	unsigned state = SceCpuForDriver_821FC0EE();
	SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	unsigned i;

	for (i = 0; i < 4; i++) {
		if (!cpu->breakHooks[i]) {
			cpu->breakHooks[i] = hook;
			sceKernelCpuResumeIntr(state);
			return 0;
		}
	}
	sceKernelCpuResumeIntr(state);
	return SDFMGR_NO_SLOT;
}

int SceDeci4pSDfMgrForKernel_E53400AE(SdfMgrHook hook)
{
	unsigned state = SceCpuForDriver_821FC0EE();
	SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	unsigned i;

	for (i = 0; i < 4; i++) {
		if (cpu->breakHooks[i] == hook) {
			cpu->breakHooks[i] = NULL;
			sceKernelCpuResumeIntr(state);
			return 0;
		}
	}
	sceKernelCpuResumeIntr(state);
	return SDFMGR_INVALID_ARGUMENT;
}

int SceDeci4pSDfMgrForKernel_3A25594F(SdfMgrControl control)
{
	unsigned state = SceCpuForDriver_821FC0EE();
	SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	unsigned index = 0;

	if (cpu->controlComponents[0]) {
		if (cpu->controlComponents[1]) {
			sceKernelCpuResumeIntr(state);
			return SDFMGR_NO_SLOT;
		}
		index = 1;
	}
	cpu->controlComponents[index] = control;
	sceKernelCpuResumeIntr(state);
	return 0;
}

int SceDeci4pSDfMgrForKernel_59C077B3(SdfMgrControl control)
{
	unsigned state = SceCpuForDriver_821FC0EE();
	SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	unsigned index;

	if (cpu->controlComponents[0] == control)
		index = 0;
	else if (cpu->controlComponents[1] == control)
		index = 1;
	else {
		sceKernelCpuResumeIntr(state);
		return SDFMGR_INVALID_ARGUMENT;
	}
	cpu->controlComponents[index] = NULL;
	sceKernelCpuResumeIntr(state);
	return 0;
}

/* Per-core DECI initialization and CPU0 exception/debugger registration. */

extern SceExcpmgrHandler sdfMgrDataAbortEntry;
extern SceExcpmgrHandler sdfMgrPrefetchAbortEntry;
extern SceExcpmgrHandler sdfMgrUndefinedEntry;

/* Sysroot keeps these handlers for the bus error, system error and power paths. */
int dcmpInit(SdfMgrProtocolState *protocol);
int tsmpInit(SdfMgrProtocolState *protocol);
int sdioInit(void);
void dcmpSendStatusSystemError(uint32_t type, SceSize size, const void *source);
int _threadKicker(void);
int sdfMgrSuspend(void);
int sdfMgrResume(void);

static const struct {
	uint32_t physical;
	const char *name;
} debugMappings[4] = {
	{ CORESIGHT_DBG_BASE(0), "SceDbg0RegForSdfmgr" },
	{ CORESIGHT_DBG_BASE(1), "SceDbg1RegForSdfmgr" },
	{ CORESIGHT_DBG_BASE(2), "SceDbg2RegForSdfmgr" },
	{ CORESIGHT_DBG_BASE(3), "SceDbg3RegForSdfmgr" },
};

#if FW_DEBUG
#define BOOT_CHECK(_result, _line, _hash, _assertLine, _assertHash, _format) do { \
	if ((_result) < 0) { \
		SceDeci4pSDfMgrForKernel_501CE61D(15, \
			SDFMGR_DEBUG_INFO(_line, _hash, 0xaaf0498e, \
					 "sceSDfMgrModuleBoot"), _format, _result); \
		if (SceDebugForDriver_374B7868() > 0) \
			SceDebugForDriver_FD753E7A(1, 0, \
				SDFMGR_DEBUG_INFO(_assertLine, _assertHash, 0xaaf0498e, \
						 "sceSDfMgrModuleBoot"), caller); \
		return _result; \
	} \
} while (0)
#else
#define BOOT_CHECK(_result, _line, _hash, _assertLine, _assertHash, _format) do { \
	if ((_result) < 0) \
		return _result; \
} while (0)
#endif

int module_bootstart(SceSize args, void *argp)
{
	SceKernelAllocMemBlockKernelOpt options;
	uint32_t debugControl;
	unsigned i;
	int result;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	(void)args;
	(void)argp;
	if (!SceDipswForDriver_A98FC2FD(0xc1)) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 11,
			SDFMGR_DEBUG_INFO(224, 0xdb4fcfbc, 0xaaf0498e,
					 "sceSDfMgrModuleBoot"), "disabled by dipsw\n");
#endif
		return 1;
	}
	SceDeci4pSDfMgrForKernel_8A3B06F2(&s_sdfMgrState.cpu[sdfMgrCoreIndex()],
					 0, sizeof(SdfMgrCpuState));
	sdfMgrUpdateCpuFlags(2, 1);
	result = dcmpInit(_getPdCB(0));
	BOOT_CHECK(result, 251, 0xd78ad3ac, 252, 0x72ec2305,
		   "dcmpInit failed (0x%08x)\n");
	result = tsmpInit(_getPdCB(1));
	BOOT_CHECK(result, 259, 0x11bd48c8, 260, 0xae9f4ebd,
		   "tsmpInit failed (0x%08x)\n");
	result = sdioInit();
	BOOT_CHECK(result, 267, 0x8bc042c7, 268, 0x5805c178,
		   "sdioInit failed (0x%08x)\n");
	if (SceDipswForDriver_A98FC2FD(0xe4)) {
		CP_WRITE(p14, 0, c0, c0, 4, 0);
		CP_WRITE(p14, 0, c0, c1, 4, 0);
		CP_WRITE(p14, 0, c0, c2, 4, 0);
		CP_WRITE(p14, 0, c0, c3, 4, 0);
		CP_WRITE(p14, 0, c0, c4, 4, 0);
		CP_WRITE(p14, 0, c0, c5, 4, 0);
		CP_WRITE(p14, 0, c0, c0, 5, 0);
		CP_WRITE(p14, 0, c0, c1, 5, 0);
		CP_WRITE(p14, 0, c0, c2, 5, 0);
		CP_WRITE(p14, 0, c0, c3, 5, 0);
		CP_WRITE(p14, 0, c0, c4, 5, 0);
		CP_WRITE(p14, 0, c0, c5, 5, 0);
		CP_WRITE(p14, 0, c0, c0, 6, 0);
		CP_WRITE(p14, 0, c0, c1, 6, 0);
		CP_WRITE(p14, 0, c0, c2, 6, 0);
		CP_WRITE(p14, 0, c0, c3, 6, 0);
		CP_WRITE(p14, 0, c0, c0, 7, 0);
		CP_WRITE(p14, 0, c0, c1, 7, 0);
		CP_WRITE(p14, 0, c0, c2, 7, 0);
		CP_WRITE(p14, 0, c0, c3, 7, 0);
		debugControl = CP_READ(p14, 0, c0, c2, 2);
		if (!(debugControl & 0x8000))
			CP_WRITE(p14, 0, c0, c2, 2, debugControl | 0x8000);
		if (!sceKernelCpuId()) {
			memset(&options, 0, sizeof(options));
			options.size = sizeof(options);
			options.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
			for (i = 0; i < 4; i++) {
				options.paddr = debugMappings[i].physical;
				result = SceSysmemForDriver_C94850C9(debugMappings[i].name,
								 SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW, 0x1000, &options);
				if (result < 0)
					return result;
				s_sdfMgrState.global.debugMemblocks[i] = result;
				SceSysmemForDriver_A841EDDA(result,
					(void **)&s_sdfMgrState.global.debugRegisters[i]);
			}
		}
	}
	if (!sceKernelCpuId()) {
		result = sceKernelRegisterExceptionHandler(3, 2, &sdfMgrPrefetchAbortEntry);
		BOOT_CHECK(result, 291, 0x3ca87a6f, 292, 0x40a8c17c,
			   "sceKernelRegisterExceptionHandler() failed (0x%08x)\n");
		result = sceKernelRegisterExceptionHandler(4, 2, &sdfMgrDataAbortEntry);
		BOOT_CHECK(result, 303, 0x6a2d38af, 304, 0x292cf543,
			   "sceKernelRegisterExceptionHandler() failed (0x%08x)\n");
		result = sceKernelRegisterExceptionHandler(1, 2, &sdfMgrUndefinedEntry);
		BOOT_CHECK(result, 315, 0xda54a773, 316, 0xfa6cbe59,
			   "sceKernelRegisterExceptionHandler() failed (0x%08x)\n");
		/* The registered routines ignore the interrupt code and context. */
		result = sceKernelRegisterIntrHandler(0, "SceSDfMgrBreakSgiHandler", 0,
			(SceKernelIntrHandler)sdfMgrWaitForControl, NULL, 0x10, 15, NULL);
		BOOT_CHECK(result, 331, 0xb54914e4, 332, 0xe368e1fb,
			   "sceKernelRegisterIntrHandler() failed (0x%08x)\n");
		result = sceKernelRegisterIntrHandler(1, "SceSDfMgrThreadSgiHandler", 0,
			(SceKernelIntrHandler)_threadKicker, NULL, 0x20, 15, NULL);
		BOOT_CHECK(result, 347, 0x9d408fa5, 348, 0xdc18cd4a,
			   "sceKernelRegisterIntrHandler() failed (0x%08x)\n");
		SceSysrootForKernel_8747D415(busErrorHandler);
		SceSysrootForKernel_2D6B2A79(dcmpSendStatusSystemError);
		SceSysrootForKernel_C5EAF5F7(sdfMgrReportPowerState);
	}
	result = SceDeci4pSDfMgrForKernel_B22B42FB(sdfMgrHandleControlInterrupt);
	BOOT_CHECK(result, 366, 0xda2c4ab9, 367, 0x7cebaae2,
		   "RegisterBreakHook failed (0x%08x)\n");
	SceKernelSuspendForDriver_CE7A2207(25, (int (*)(void *))sdfMgrSuspend, NULL);
	SceKernelSuspendForDriver_105C5752(24, (int (*)(void *))sdfMgrResume, NULL);
	sdfMgrUpdateCpuFlags(1, 1);
	dcmpSendStatusPower(15, 1, 0);
	if (!sceKernelCpuId())
		dcmpSendStatusPower(0, 2, 10);
	return 0;
}

#undef BOOT_CHECK

/* System-error capture and debugger handoff. */

void sdfMgrFormatCpsr(char *buffer, uint32_t cpsr);
void dumpModuleInfo(uint32_t address);

typedef int (*SdfMgrExceptionHook)(SceExcpmgrContext *context,
				 uint32_t address, uint32_t cpsr);

int sdfMgrPrefetchAbortHandler(SceExcpmgrContext *context,
			      uint32_t address, uint32_t cpsr)
{
	SceKernelThreadContextInfo thread;
	char status[64];
	SdfMgrCpuState *cpu;
	SdfMgrExceptionHook hook;
	uint32_t fault;
	int result = 2;

	SceThreadmgrForKernel_6C1F092F(&thread);
	cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	fault = readIfsr();
	sdfMgrFormatCpsr(status, cpsr);
	s_sdfMgrState.exceptionDepth = (uint32_t *)SceExcpmgrForKernel_96C2869C();
	if (((fault & 15) | ((fault >> 10) & 1) << 4) != 2 &&
	    thread.process_id == SCE_KERNEL_PROCESS_ID)
		dumpModuleInfo(address);
	hook = (SdfMgrExceptionHook)cpu->indexedHooks[0];
	if (hook)
		result = hook(context, address, cpsr);
	while (s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 4)
		sdfMgrPollBreakHooks(cpu);
	return result;
}

int sdfMgrDataAbortHandler(SceExcpmgrContext *context,
			  uint32_t address, uint32_t cpsr)
{
	SceKernelThreadContextInfo thread;
	char status[64];
	SdfMgrCpuState *cpu;
	SdfMgrExceptionHook hook;
	uint32_t fault;
	int result = 2;

	SceThreadmgrForKernel_6C1F092F(&thread);
	cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	fault = readDfsr();
	sdfMgrFormatCpsr(status, cpsr);
	s_sdfMgrState.exceptionDepth = (uint32_t *)SceExcpmgrForKernel_96C2869C();
	if (((fault & 15) | ((fault >> 10) & 1) << 4) != 2 &&
	    thread.process_id == SCE_KERNEL_PROCESS_ID)
		dumpModuleInfo(address);
	hook = (SdfMgrExceptionHook)cpu->indexedHooks[1];
	if (hook)
		result = hook(context, address, cpsr);
	while (s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 4)
		sdfMgrPollBreakHooks(cpu);
	return result;
}

int sdfMgrUndefinedHandler(SceExcpmgrContext *context,
			   uint32_t address, uint32_t cpsr)
{
	SceKernelThreadContextInfo thread;
	char status[64];
	SdfMgrCpuState *cpu;
	SdfMgrExceptionHook hook;
	int result = 2;

	SceThreadmgrForKernel_6C1F092F(&thread);
	sdfMgrFormatCpsr(status, cpsr);
	cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	if (thread.process_id == SCE_KERNEL_PROCESS_ID)
		dumpModuleInfo(address);
	hook = (SdfMgrExceptionHook)cpu->indexedHooks[2];
	if (hook)
		result = hook(context, address, cpsr);
	while (s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 4)
		sdfMgrPollBreakHooks(cpu);
	return result;
}

void dcmpSendStatusSystemError(uint32_t type, SceSize size, const void *source);
void *dcmpGetSystemErrorBuffer(void);
SceSize dcmpGetSystemErrorBufferSize(void);

typedef void (*SdfMgrBusErrorHook)(uint32_t status0, uint32_t status1, uint32_t status2);

void busErrorHandler(uint32_t status0, uint32_t status1, uint32_t status2)
{
	SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	SdfMgrBusErrorHook hook;
	void *buffer;
	SceSize size;

#if FW_DEBUG
	SceDeci4pSDfMgrForKernel_501CE61D(15,
		SDFMGR_DEBUG_INFO(2377, 0xfdbcceec, 0x4a0dff49, "busErrorHandler"),
		"****** BusError Intr ******\n", sizeof(SdfMgrCpuState));
#endif
	dcmpSendStatusSystemError(2, 0, NULL);
	buffer = dcmpGetSystemErrorBuffer();
	size = dcmpGetSystemErrorBufferSize();
	size = SceSysrootForKernel_CD4B84F7(buffer, size);
	if (size)
		dcmpSendStatusSystemError(2, size, NULL);
	hook = (SdfMgrBusErrorHook)cpu->indexedHooks[3];
	if (hook)
		hook(status0, status1, status2);
	while (s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 4)
		sdfMgrPollBreakHooks(cpu);
}

/* Saved system-register policy; banked and live registers stay in the veneer. */

/* The debugger only needs the GIC CPU-interface prefix returned by Intrmgr. */
typedef struct SdfMgrCpuInterface {
	SceUInt32 control;
	SceUInt32 priorityMask;
	SceUInt32 binaryPoint;
	SceUInt32 acknowledge;
	SceUInt32 endOfIntr;
} SdfMgrCpuInterface;

extern void sceSDfMgrExcpAssert(void);

/* A handled exception may return only after leaving the outermost level. */
void sdfMgrFinishException(const SceExcpmgrContext *context)
{
	SceUInt32 *depth = &s_sdfMgrState.exceptionDepth[readMpidr() & 15];
	SceUInt32 nesting = *depth - 1;

	if (nesting) {
		for (;;)
			sceSDfMgrExcpAssert();
	}
	*depth = nesting;
	writeDfsr(0);
	writeIfsr(0);
	writeAdfsr(0);
	writeAifsr(0);
	writeDfar(0);
	writeIfar(0);
	/* The native condition tests the depth pointer, not the saved thread. */
	if (depth)
		writeTpidrprw(context->thread);
}

typedef struct SdfMgrIrqStack {
	SceUInt32 guard;
	unsigned char storage[0x2000 - sizeof(SceUInt32)];
} SdfMgrIrqStack;

_Static_assert(sizeof(SdfMgrIrqStack) == 0x2000, "interrupt stack stride");

void sdfMgrMarkTracePending(void)
{
	const ExcpVectorBank *vectors = (const ExcpVectorBank *)readVbar();
	ExcpTraceArea *trace = vectors->trace;

	if (trace)
		trace->control = 1;
}

void sdfMgrRestoreExceptionAddressSpace(const SceExcpmgrContext *context)
{
	SceUInt32 translationBase = context->ttbr1;
	SceUInt32 contextId = context->contextId;

	writeTpidruro(context->tpidruro);
	writeContextId(0);
	isb();
	writeTtbr1(translationBase);
	isb();
	writeContextId(contextId);
	writeDacr(context->dacr);
	writePar(context->par);
}

void sdfMgrRestoreThreadDacr(const ThreadRegisterContext *context, SceUInt32 status)
{
	SceUInt32 dacr = (status & 31) == 16 ? context->userDacr : context->kernelDacr;

	writeDacr(dacr);
}

/* Complete the SDIO return without Intrmgr's ordinary interrupt-disable policy. */
SceUInt32 sdfMgrCompleteInterrupt(SceUInt32 acknowledge)
{
	volatile SdfMgrCpuInterface *cpu = SceIntrmgrForKernel_807B2004();
	const ExcpVectorBank *vectors;
	const SdfMgrIrqStack *stack;
	SceUInt32 *depth;
	SceUInt32 nesting, core;
	int level;

	cpu->endOfIntr = acknowledge;
	vectors = (const ExcpVectorBank *)readVbar();
	depth = vectors->nesting;
	nesting = *depth;
	core = readMpidr() & 15;
	level = SceIntrmgrForDriver_182EE3E3();
	stack = SceIntrmgrForKernel_01911612(core, level);
	if ((stack - nesting)->guard != 0x1ab4e8dfU) {
		for (;;)
			;
	}
	*depth = --nesting;
	return nesting;
}

void sdfMgrRestoreSystemContext(const ThreadRegisterContext *context)
{
	SceUInt32 priorityMask = context->priorityMask;
	volatile SdfMgrCpuInterface *cpu = SceIntrmgrForKernel_807B2004();

	cpu->priorityMask = priorityMask;
	armRestoreThreadSystemContext(context, 1);
}

/* Native SDfMgr restores these slots directly, without Intrmgr's table locks. */
void sdfMgrRestoreDebugContext(const ThreadCB *thread,
			      SceUInt32 resumeValue, SceUInt32 resumeControl,
			      int threadReturn)
{
	const ProcessHwBreakpoints *regs;
	const ProcessHwWatchpoints *watch;

	if (thread)
		regs = thread->process->phbp;
	else
		regs = ((SceExcpmgrData *)s_sdfMgrState.exceptionDepth)->kernelBreakpoints;
	if (!regs)
		return;
	CP_WRITE(p14, 0, c0, c1, 4, resumeValue);
	CP_WRITE(p14, 0, c0, c1, 5, resumeControl);
	if (threadReturn) {
		CP_WRITE(p14, 0, c0, c0, 4, regs->reg[0].value);
		CP_WRITE(p14, 0, c0, c0, 5, regs->reg[0].control);
	}
	CP_WRITE(p14, 0, c0, c2, 4, regs->reg[2].value);
	CP_WRITE(p14, 0, c0, c2, 5, regs->reg[2].control);
	CP_WRITE(p14, 0, c0, c3, 4, regs->reg[3].value);
	CP_WRITE(p14, 0, c0, c3, 5, regs->reg[3].control);
	CP_WRITE(p14, 0, c0, c4, 4, regs->reg[4].value);
	CP_WRITE(p14, 0, c0, c4, 5, regs->reg[4].control);
	if (thread)
		watch = thread->process->phwp;
	else
		watch = ((SceExcpmgrData *)s_sdfMgrState.exceptionDepth)->kernelWatchpoints;
	if (!watch)
		return;
	CP_WRITE(p14, 0, c0, c1, 6, watch->reg[1].value);
	CP_WRITE(p14, 0, c0, c1, 7, watch->reg[1].control);
	CP_WRITE(p14, 0, c0, c2, 6, watch->reg[2].value);
	CP_WRITE(p14, 0, c0, c2, 7, watch->reg[2].control);
	CP_WRITE(p14, 0, c0, c3, 6, watch->reg[3].value);
	CP_WRITE(p14, 0, c0, c3, 7, watch->reg[3].control);
	if (thread) {
		CP_WRITE(p14, 0, c0, c5, 4, thread->thbp[0]);
		CP_WRITE(p14, 0, c0, c5, 5, thread->thbp[1]);
	}
}

/* System break/continue and the mapped ARM debug register interface. */

#if FW_DEBUG
#define MODULE_INFO(_line, _hash) \
	SDFMGR_DEBUG_INFO(_line, _hash, 0xd2bbeef4, "dumpModuleInfo")
#else
#define MODULE_INFO(_line, _hash) ({ \
	static const SceKernelDebugInfo info = { \
		.fileHash = 0x6364a003, .lineHash = (_hash), .funcHash = 0xd2bbeef4, \
	}; \
	&info; \
})
#endif

void dumpModuleInfo(uint32_t address)
{
	SceModuleCB *module, *snapshot;
	char name[27];

	if (SceModulemgrForKernel_1728612F(SCE_KERNEL_PROCESS_ID, address, &module) < 0)
		return;
	SceDebugForDriver_02B04343(0, MODULE_INFO(2808, 0x3099e3a6),
		"\n**** Module Info ****\n  ModuleID    0x%08x\n", module->modid);
	snapshot = module;
	strncpy(name, snapshot->name, 26);
	name[26] = 0;
	SceDebugForDriver_02B04343(0, MODULE_INFO(2812, 0x25789f2e),
		"  Name        %s (%d.%d)\n", name,
		snapshot->version[1], snapshot->version[0]);
	SceDebugForDriver_02B04343(0, MODULE_INFO(2813, 0x3d30d411),
		"  Filename    %s\n", module->path);
	SceDebugForDriver_02B04343(0, MODULE_INFO(2814, 0xed38cbf5),
		"  FingerPrint 0x%08x\n", module->fingerprint);
	snapshot = module;
	SceDebugForDriver_02B04343(0, MODULE_INFO(2818, 0xeae37fc4),
		"  text        base=0x%08x, size=0x%08x\n",
		snapshot->segment[0].vaddr, snapshot->segment[0].memsz);
	snapshot = module;
	if (snapshot->nSegments > 1)
		SceDebugForDriver_02B04343(0, MODULE_INFO(2822, 0x6eab9e96),
			"  data        base=0x%08x, size=0x%08x\n",
			snapshot->segment[1].vaddr, snapshot->segment[1].memsz);
	SceDebugForDriver_02B04343(0, MODULE_INFO(2824, 0xf85f551d),
		"  Pc          0x%08x\n", address);
	SceDebugForDriver_02B04343(0, MODULE_INFO(2825, 0x0b786902), "\n");
}

void sdfMgrFormatCpsr(char *buffer, uint32_t cpsr)
{
	armFormatCpsr(cpsr, buffer);
}

int SceDeci4pSDfMgrForKernel_501CE61D(SceUInt32 level,
				   const SceKernelDebugInfo *info,
				   const char *format, ...)
{
	va_list args;
	SdfMgrCpuState *cpu;
	SdfMgrPrintHandler handler;
	int result = 0;

	va_start(args, format);
	cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	handler = cpu->printHandler;
	if (handler)
		result = handler(cpu->printContext, cpu->reserved, level, info, format, args);
	va_end(args);
	return result;
}

struct SdfMgrDebugRegisters {
	uint8_t reserved0[0x100];
	volatile uint32_t breakpointValue[6];
	uint8_t reserved1[0x28];
	volatile uint32_t breakpointControl[6];
	uint8_t reserved2[0xe48];
	volatile uint32_t claimSet;
	volatile uint32_t claimClear;
	uint8_t reserved3[8];
	volatile uint32_t lockAccess;
	volatile uint32_t lockStatus;
	uint8_t reserved4[0x48];
};

_Static_assert(sizeof(struct SdfMgrDebugRegisters) == 0x1000, "debug register page");
_Static_assert(offsetof(struct SdfMgrDebugRegisters, breakpointValue) == 0x100,
	       "BVR0 register");
_Static_assert(offsetof(struct SdfMgrDebugRegisters, breakpointControl) == 0x140,
	       "BCR0 register");
_Static_assert(offsetof(struct SdfMgrDebugRegisters, claimSet) == 0xfa0,
	       "debug claim set register");
_Static_assert(offsetof(struct SdfMgrDebugRegisters, lockAccess) == 0xfb0,
	       "debug lock access register");
_Static_assert(offsetof(struct SdfMgrDebugRegisters, lockStatus) == 0xfb4,
	       "debug lock status register");

static inline int sdfMgrUnlockDebugRegisters(struct SdfMgrDebugRegisters *regs)
{
	if (!(regs->lockStatus & 2))
		return 0;
	regs->lockAccess = 0xc5acce55;
	(void)regs->lockAccess;
	dsb();
	return 1;
}

static inline void sdfMgrLockDebugRegisters(struct SdfMgrDebugRegisters *regs)
{
	regs->lockAccess = 0x3a5331aa;
	(void)regs->lockAccess;
	dsb();
}

void sceSDfMgrExcpAssert(void)
{
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
	unsigned cpu = sceKernelCpuId();

	SceDeci4pSDfMgrForKernel_501CE61D(0,
		SDFMGR_DEBUG_INFO(2843, 0xf14ac88a, 0xce8e563c,
				 "sceSDfMgrExcpAssert"),
		"nestCount%d\n", s_sdfMgrState.exceptionDepth[cpu]);
	SceDebugForDriver_35A35322(0,
		SDFMGR_DEBUG_INFO(2846, 0xd31e11b0, 0xce8e563c,
				 "sceSDfMgrExcpAssert"), caller);
#endif
}

int sceSDfMgrBreakSystem(void)
{
	unsigned cpu = sdfMgrCoreIndex();
	unsigned i;
	int result;

	if (!(s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 4)) {
		sdfMgrUpdateCpuFlags(4, 1);
		_execControlComponent(&s_sdfMgrState.cpu[cpu], 1);
	}
	sdfMgrUpdateGlobalFlags(1, 1);
	result = sceKernelGenerateSoftIntr(0, 1, 0);
#if FW_DEBUG
	if (result < 0)
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDFMGR_DEBUG_INFO(2583, 0x63163c46, 0x2ef59665,
					 "sceSDfMgrBreakSystem"),
			"generate break SGI failed. (0x%08x)\n", result);
#else
	(void)result;
#endif
	if (SceDipswForDriver_A98FC2FD(0xe4)) {
		for (i = 0; i < 4; i++) {
			struct SdfMgrDebugRegisters *regs =
				s_sdfMgrState.global.debugRegisters[i];
			int locked = sdfMgrUnlockDebugRegisters(regs);

			regs->claimSet = 1;
			dsb();
			if (locked)
				sdfMgrLockDebugRegisters(regs);
		}
		sev();
	}
	return 0;
}

int sceSDfMgrContinueSystem(void)
{
	unsigned cpu = sdfMgrCoreIndex();
	int result;

	if (s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 4) {
		sdfMgrUpdateCpuFlags(4, 0);
		_execControlComponent(&s_sdfMgrState.cpu[cpu], 0);
	}
	sdfMgrUpdateGlobalFlags(1, 0);
	result = sceKernelGenerateSoftIntr(0, 1, 0);
#if FW_DEBUG
	if (result < 0)
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDFMGR_DEBUG_INFO(2663, 0x483507b9, 0xb949ecd3,
					 "sceSDfMgrContinueSystem"),
			"generate break SGI failed. (0x%08x)\n", result);
#else
	(void)result;
#endif
	return 0;
}

int SceDeci4pSDfMgrForKernel_FCF211FB(uint32_t value, uint32_t control)
{
	unsigned index;

	if (!SceDipswForDriver_A98FC2FD(0xe4))
		return 0;
	for (index = 0; index < 4; index++) {
		struct SdfMgrDebugRegisters *regs =
			s_sdfMgrState.global.debugRegisters[index];

		if (regs->lockStatus & 2) {
			regs->lockAccess = 0xc5acce55;
			(void)regs->lockAccess;
			dsb();
			regs->breakpointValue[0] = value;
			regs->breakpointControl[0] = control;
			dsb();
			sdfMgrLockDebugRegisters(regs);
		} else {
			regs->breakpointValue[0] = value;
			regs->breakpointControl[0] = control;
			dsb();
		}
	}
	return 0;
}

/* Polling transport and per-core protocol operations. */

int sceSDfMgrIf2MgrEvent(SdfMgrTransport *transport, int event,
			uint32_t arg, uint32_t protocol, uint32_t destination)
{
	SdfMgrProtocolState *current;
	uint32_t size, progress;

	switch (event) {
	case 1:
		current = transport->receiveProtocol;
		if (!current) {
			SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
			unsigned index;
			int result;

			transport->receiveSize = 0;
			transport->receiveProgress = 0;
			if ((destination & ~0x2000u) != 0x8000 &&
			    destination != 0xa200 && destination != sdfMgrCpuAddress()) {
#if FW_DEBUG
				SceDeci4pSDfMgrForKernel_501CE61D(15,
					SDFMGR_DEBUG_INFO(1913, 0x9c2dae5f, 0x87394e08,
							"_deliverRecvPacket"),
					"invalid destination (0x%04x)\n", destination);
#endif
				return (int)0x8009000c;
			}
			for (index = 0; index < 8; index++)
				if (cpu->protocols[index].registration.protocol == protocol)
					break;
			if (index == 8) {
				dcmpSendError(_getPdCB(0), 2, &transport->receiveHeader);
				return (int)0x8009000e;
			}
			transport->receiveProtocol = &cpu->protocols[index];
			result = transport->operations.beginReceive(transport);
			if (result < 0)
				return result;
			current = transport->receiveProtocol;
		} else if (!transport->receiveSize) {
			transport->receiveSize =
				*(const uint16_t *)transport->receiveHeader.size |
				(uint32_t)transport->receiveHeader.size[2] << 16;
		}
		transport->flags |= 4;
		current->handler(1, (void *)(uintptr_t)arg, current->context);
		transport->flags &= ~4u;
		break;
	case 2:
		if (!transport->receiveProtocol) {
#if FW_DEBUG
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				SDFMGR_DEBUG_INFO(1228, 0x302f85c8, 0xfa325223,
						"sceSDfMgrIf2MgrEvent"),
				"IF_EVENT_INDONE: receiving PD was not found.\n");
#endif
			break;
		}
		size = transport->receiveSize;
		if (!size) {
			size = *(const uint16_t *)transport->receiveHeader.size |
				(uint32_t)transport->receiveHeader.size[2] << 16;
			transport->receiveSize = size;
		}
		progress = transport->receiveProgress;
		if (progress > size) {
#if FW_DEBUG
			if (progress > size + 3) {
				SceDeci4pSDfMgrForKernel_501CE61D(15,
					SDFMGR_DEBUG_INFO(1237, 0xf44d4c2f, 0xfa325223,
							"sceSDfMgrIf2MgrEvent"),
					"IFM_INDONE: Receive packet too large %d > %d.\n",
					progress, size);
				size = transport->receiveSize;
				progress = transport->receiveProgress;
			}
#endif
			arg = size - progress;
		}
		progress += arg;
		transport->receiveProgress = progress;
		if (progress >= size) {
			current = transport->receiveProtocol;
			current->handler(2, (void *)(uintptr_t)arg, current->context);
			transport->receiveProtocol = NULL;
			transport->operations.finishReceive(transport);
		}
		break;
	case 3:
		current = transport->sendProtocol;
		if (!current) {
#if FW_DEBUG
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				SDFMGR_DEBUG_INFO(1252, 0xabe1986e, 0xfa325223,
						"sceSDfMgrIf2MgrEvent"),
				"IFM_OUT: Sending PD was not found.\n");
#endif
			break;
		}
		transport->operationFlags = (transport->operationFlags & ~0x10u) | 8;
		current->handler(3, (void *)(uintptr_t)arg, current->context);
		transport->operationFlags &= ~8u;
		break;
	case 4:
		current = transport->sendProtocol;
		if (!current) {
#if FW_DEBUG
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				SDFMGR_DEBUG_INFO(1264, 0x4b3bf246, 0xfa325223,
						"sceSDfMgrIf2MgrEvent"),
				"IFM_OUTDONE: Sending PD was not found.\n");
#endif
			break;
		}
		size = transport->sendSize;
		progress = transport->sendProgress + arg;
		transport->sendProgress = progress;
		if (size && progress >= size) {
			unsigned cpu = sdfMgrCoreIndex();

			transport->sendProtocol = NULL;
			transport->operations.reset(transport);
			current->handler(4, NULL, current->context);
			sdfMgrScheduleSend(&s_sdfMgrState.cpu[cpu]);
		}
		break;
	case 5:
		transport->operationFlags |= 1;
		break;
	case 6:
		transport->operationFlags &= ~1u;
		break;
	}
	return 0;
}

uint32_t sdfMgrCpuAddress(void)
{
	return 0xa200 + (1U << sceKernelCpuId());
}

__attribute__((noinline)) int sdfMgrQueuePacket(SdfMgrPacketQueue *queue,
			const void *packet, SceSize size, int direct)
{
	unsigned state;
	int result, write, count;

	if (size > 0x100 && !direct)
		return (int)0x80090001;
	state = SceCpuForDriver_821FC0EE();
	count = queue->count;
	if (count > 15) {
		result = (int)0x8009000b;
		goto out;
	}
	if (direct) {
		queue->directMode = 1;
	} else {
		write = queue->write;
		queue->directMode = 0;
		SceDeci4pSDfMgrForKernel_5996E2DB(queue->packets[write], packet, size);
		count = queue->count;
	}
	write = queue->write;
	queue->sizes[write] = size;
	queue->count = (int)((uint32_t)count + 1);
	write = (int)((uint32_t)write + 1);
	queue->write = write > 15 ? 0 : write;
	result = 0;
out:
	sceKernelCpuResumeIntr(state);
	return result;
}

#if FW_DEBUG
__attribute__((noinline)) SdfMgrProtocolState *_getPdCB(unsigned index)
{
	void *caller = __builtin_return_address(0);
	unsigned core = sdfMgrCoreIndex();

	if (index < 8)
		return &s_sdfMgrState.cpu[core].protocols[index];
	SceDeci4pSDfMgrForKernel_501CE61D(15,
		SDFMGR_DEBUG_INFO(1630, 0xbf05bcf0, 0x89e87bb7, "_getPdCB"),
		"invalid argument (id: %d)\n", index);
	if (SceDebugForDriver_374B7868() > 0)
		SceDebugForDriver_FD753E7A(1, 0,
			SDFMGR_DEBUG_INFO(1631, 0xea012585, 0x89e87bb7, "_getPdCB"),
			caller);
	return NULL;
}
#endif

int sceSDfMgrExOpen(SceSDfMgrHandler handler, const SceSDfMgrProtocol *registration,
		   void *context)
{
	SdfMgrProtocolState *protocol;
	SdfMgrCpuState *cpu;
	uint16_t flags;
	uint32_t handle, status;
	unsigned core, i;

	if (!registration) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDFMGR_DEBUG_INFO(630, 0x3c31089b, 0xaefc5866, "sceSDfMgrExOpen"),
			"invalid argument\n");
#endif
		return (int)0x80090001;
	}
	if (!registration->protocol) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDFMGR_DEBUG_INFO(634, 0xd6748032, 0xaefc5866, "sceSDfMgrExOpen"),
			"invalid argument\n");
#endif
		return (int)0x80090001;
	}
	if (registration->size != sizeof(*registration)) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDFMGR_DEBUG_INFO(638, 0xf4c8fc85, 0xaefc5866, "sceSDfMgrExOpen"),
			"invalid argument\n");
#endif
		return (int)0x80090001;
	}
	flags = registration->flags;
	core = sdfMgrCoreIndex();
	for (i = 0; i < 8; i++) {
		protocol = _getPdCB(i);
		if ((protocol->flags & 1) &&
		    protocol->registration.protocol == registration->protocol)
			return (int)0x80090003;
	}
	for (i = 2; i < 8; i++) {
		protocol = _getPdCB(i);
		if (!(protocol->flags & 1))
			break;
	}
	if (i == 8) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDFMGR_DEBUG_INFO(690, 0xe26bb55d, 0xaefc5866, "sceSDfMgrExOpen"),
			"no entry\n");
#endif
		return (int)0x80090004;
	}
	SceDeci4pSDfMgrForKernel_8A3B06F2(protocol, 0, sizeof(*protocol));
	cpu = &s_sdfMgrState.cpu[core];
	handle = cpu->nextHandle;
	protocol->handler = handler;
	protocol->registration = *registration;
	protocol->context = context;
	cpu->nextHandle = handle + 1;
	protocol->handle = (int)handle;
	status = flags & 1 ? 3 : 1;
	protocol->flags = status;
	if (registration->flags & 2)
		status |= 4;
	dcmpSendStatusProtocol(status, registration);
	if (registration->flags & 2)
		dcmpSendStatusProtocolAlias(registration);
	return (int)handle;
}

int SceDeci4pSDfMgrForKernel_A55F5F0D(SceSDfMgrHandler handler,
				  const SceSDfMgrProtocol *registration, void *context)
{
	unsigned state = SceCpuForDriver_821FC0EE();
	int result = sceSDfMgrExOpen(handler, registration, context);

	sceKernelCpuResumeIntr(state);
	return result;
}

int sceSDfMgrExClose(int handle)
{
	SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	unsigned i;

	for (i = 0; i < 8; i++) {
		SdfMgrProtocolState *protocol = &cpu->protocols[i];

		if ((protocol->flags & 1) && protocol->handle == handle) {
			SceSDfMgrProtocol registration = protocol->registration;

			SceDeci4pSDfMgrForKernel_8A3B06F2(protocol, 0, sizeof(*protocol));
			dcmpSendStatusProtocol(0, &registration);
			return 0;
		}
	}
#if FW_DEBUG
	SceDeci4pSDfMgrForKernel_501CE61D(15,
		SDFMGR_DEBUG_INFO(739, 0xa6449b1f, 0xba595424, "sceSDfMgrExClose"),
		"invalid argument (sock: 0x%08X)\n", handle);
#endif
	return (int)0x80090002;
}

int SceDeci4pSDfMgrForKernel_86CFC8C6(int handle)
{
	unsigned state = SceCpuForDriver_821FC0EE();
	int result = sceSDfMgrExClose(handle);

	sceKernelCpuResumeIntr(state);
	return result;
}

int sdfmgrStartProtocol(uint32_t id)
{
	SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	DcmpProtocolStatus packet;
	unsigned i;

	for (i = 0; i < 8; i++) {
		SdfMgrProtocolState *protocol = &cpu->protocols[i];
		int result;

		if (!(protocol->flags & 1) || protocol->registration.protocol != id)
			continue;
		result = sdfMgrSetProtocolStatus(protocol, 2, 1);
		if (result < 0) {
#if FW_DEBUG
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				SDFMGR_DEBUG_INFO(1360, 0x6dd3d97d, 0x9a31b4b9,
						"sdfmgrStartProtocol"),
				"SetProtocolStatus failed (0x%08X)\n", result);
#endif
			return result;
		}
		result = dcmpMakeStatusProtocol(1, &protocol->registration, &packet);
		if (result < 0) {
#if FW_DEBUG
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				SDFMGR_DEBUG_INFO(1368, 0x1ebe38be, 0x9a31b4b9,
						"sdfmgrStartProtocol"),
				"couldn't make DCMP packet (0x%08x)\n", result);
#endif
			return result;
		}
		protocol->handler(5, &packet, protocol->context);
		return 0;
	}
	return (int)0x8009000e;
}

int sdfmgrStopProtocol(uint32_t id)
{
	SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	DcmpProtocolStatus packet;
	unsigned i;

	for (i = 0; i < 8; i++) {
		SdfMgrProtocolState *protocol = &cpu->protocols[i];
		int result;

		if (!(protocol->flags & 1) || protocol->registration.protocol != id)
			continue;
		result = sdfMgrSetProtocolStatus(protocol, 2, 0);
		if (result < 0) {
#if FW_DEBUG
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				SDFMGR_DEBUG_INFO(1395, 0xd54a5173, 0xdfedc380,
						"sdfmgrStopProtocol"),
				"SetProtocolStatus failed (0x%08X)\n", result);
#endif
			return result;
		}
		result = dcmpMakeStatusProtocol(0, &protocol->registration, &packet);
		if (result < 0) {
#if FW_DEBUG
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				SDFMGR_DEBUG_INFO(1403, 0x77029b80, 0xdfedc380,
						"sdfmgrStopProtocol"),
				"couldn't make DCMP packet (0x%08x)\n", result);
#endif
			return result;
		}
		protocol->handler(5, &packet, protocol->context);
		return 0;
	}
	return (int)0x80090001;
}

int sceSDfMgrSendSpace(int handle)
{
	SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	unsigned i;

	for (i = 0; i < 8; i++) {
		SdfMgrProtocolState *protocol = &cpu->protocols[i];

		if ((protocol->flags & 1) && protocol->handle == handle) {
			dcmpSendStatusSpace(protocol->registration.protocol);
			return 0;
		}
	}
#if FW_DEBUG
	SceDeci4pSDfMgrForKernel_501CE61D(15,
		SDFMGR_DEBUG_INFO(1573, 0x1c9f20a9, 0x27f1eed5,
				 "sceSDfMgrSendSpace"),
		"invalid argument (sock: 0x%08X)\n", handle);
#endif
	return (int)0x80090002;
}

int _threadSignal(int handle)
{
	unsigned state = SceCpuForDriver_821FC0EE();
	int result = sceSDfMgrExReqSend(handle);
	SdfMgrTransport *transport;

	if (result < 0)
		goto out;
	transport = s_sdfMgrState.cpu[sdfMgrCoreIndex()].transport;
	if (transport && (transport->operationFlags & 0x10))
		transport->operations.poll(transport);
	if (s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 8) {
		int signalResult = sceKernelGenerateSoftIntr(1, 0,
					s_sdfMgrCpuTargets[sceKernelCpuId()]);

#if FW_DEBUG
		if (signalResult < 0)
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				SDFMGR_DEBUG_INFO(1866, 0x3066a038, 0x0e527fa8,
						 "_threadSignal"),
				"generate SGI failed. (0x%08x)\n", signalResult);
#else
		(void)signalResult;
#endif
	}
	result = 0;
out:
	sceKernelCpuResumeIntr(state);
	return result;
}

int sceSDfMgrExSend(int handle, const void *buffer, SceSize size)
{
	SdfMgrCpuState *cpu;
	SdfMgrProtocolState *protocol = NULL;
	SdfMgrTransport *transport;
	SceDeci4pHeader *header = (SceDeci4pHeader *)(uintptr_t)buffer;
	unsigned i;

	if (!buffer) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDFMGR_DEBUG_INFO(950, 0xc00fa542, 0x730426c3, "sceSDfMgrExSend"),
			"invalid argument\n");
#endif
		return (int)0x80090005;
	}
	cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	for (i = 0; i < 8; i++) {
		SdfMgrProtocolState *candidate = &cpu->protocols[i];

		if ((candidate->flags & 1) && candidate->handle == handle) {
			protocol = candidate;
			break;
		}
	}
	if (!protocol) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDFMGR_DEBUG_INFO(957, 0x2442ea6b, 0x730426c3, "sceSDfMgrExSend"),
			"invalid argument (sock: 0x%08X)\n", handle);
#endif
		return (int)0x80090001;
	}
	transport = s_sdfMgrState.cpu[sdfMgrCoreIndex()].transport;
	if (!transport) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDFMGR_DEBUG_INFO(964, 0x1dcfadcb, 0x730426c3, "sceSDfMgrExSend"),
			"no host if\n");
#endif
		return (int)0x8009000d;
	}
	if (!(transport->operationFlags & 8)) {
#if FW_DEBUG
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDFMGR_DEBUG_INFO(969, 0x16de41f4, 0x730426c3, "sceSDfMgrExSend"),
			"blocked\n");
#endif
		return (int)0x80090007;
	}
	if (protocol->registration.protocol != 0x100 &&
	    protocol->registration.protocol != 0x2000) {
		if (!(s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 2) ||
		    !(protocol->flags & 2))
			return (int)0x8009000a;
	}
	if (!transport->sendSize) {
		unsigned shift;
		uint32_t cpuMask, wireProtocol, destinationProtocol;

		if (size < sizeof(*header)) {
#if FW_DEBUG
			SceDeci4pSDfMgrForKernel_501CE61D(15,
				SDFMGR_DEBUG_INFO(999, 0x6dde7b56, 0x730426c3,
						 "sceSDfMgrExSend"),
				"invalid packet size %d (%s)\n", size, protocol->registration.name);
#endif
			return (int)0x80090006;
		}
		transport->sendSize = *(const uint16_t *)header->size |
				      (uint32_t)header->size[2] << 16;
		header->version = SCE_DECI4P_VERSION;
		shift = sceKernelCpuId();
		wireProtocol = protocol->registration.protocol;
		header->reserved0 = 0;
		cpuMask = 1U << shift;
		header->reserved1 = 0;
		destinationProtocol = protocol->registration.protocol;
		header->protocol = wireProtocol;
		header->src = (uint16_t)(0xa200 + cpuMask);
		header->reserved2 = 0;
		if (destinationProtocol != 0x100)
			header->dst = protocol->registration.destination;
	}
	return transport->operations.send(transport, buffer, size);
}

void sdfMgrScheduleSend(SdfMgrCpuState *cpu)
{
	SdfMgrTransport *transport = cpu->transport;
	SdfMgrProtocolState *protocol;
	unsigned i;

	if (transport->sendProtocol)
		return;
	protocol = &cpu->protocols[cpu->lastSendIndex];
	for (i = 1; i <= 8; i++) {
		uint32_t flags = protocol->flags;

		if (flags & 4) {
			uint32_t operationFlags = transport->operationFlags;
			SdfMgrTransport *current = cpu->transport;

			protocol->flags = flags & ~4;
			transport->sendProtocol = protocol;
			transport->sendSize = 0;
			transport->operationFlags = operationFlags | 0x10;
			transport->sendProgress = 0;
			current->operations.beginSend(transport);
			cpu->lastSendIndex = i == 8 ? 0 : i;
		}
		protocol++;
		if (protocol >= cpu->protocols + 8)
			protocol = cpu->protocols;
	}
}

int sceSDfMgrExReqSend(int handle)
{
	SdfMgrCpuState *cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	SdfMgrCpuState *lookupCpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	SdfMgrProtocolState *protocol = NULL;
	uint32_t flags;
	unsigned i;

	for (i = 0; i < 8; i++) {
		SdfMgrProtocolState *candidate = &lookupCpu->protocols[i];

		if ((candidate->flags & 1) && candidate->handle == handle) {
			protocol = candidate;
			break;
		}
	}
	if (!protocol) {
#if FW_DEBUG
		static const SceKernelDebugInfo info = {
			.fileHash = 0x6364a003,
			.lineHash = 0xfee5ad50,
			.funcHash = 0xbe51d1c0,
			.file = "sdfmgr.c",
			.line = 848,
			.func = "sceSDfMgrExReqSend",
		};

		SceDeci4pSDfMgrForKernel_501CE61D(15, &info,
					 "invalid argument (sock: 0x%08X)\n", handle);
#endif
		return (int)0x80090002;
	}
	if (!s_sdfMgrState.cpu[sdfMgrCoreIndex()].transport) {
#if FW_DEBUG
		static const SceKernelDebugInfo info = {
			.fileHash = 0x6364a003,
			.lineHash = 0xab2b2ccb,
			.funcHash = 0xbe51d1c0,
			.file = "sdfmgr.c",
			.line = 855,
			.func = "sceSDfMgrExReqSend",
		};

		SceDeci4pSDfMgrForKernel_501CE61D(15, &info, "no host if\n");
#endif
		return (int)0x8009000d;
	}
	flags = protocol->flags;
	if (flags & 4)
		return (int)0x80090007;
	if (protocol->registration.protocol != 0x100) {
		if (!(s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 2) || !(flags & 2))
			return (int)0x8009000a;
	}
	protocol->flags = flags | 4;
	sdfMgrScheduleSend(cpu);
	return 0;
}

int sceSDfMgrExRecv(int handle, void *buffer, SceSize size)
{
	SdfMgrCpuState *cpu;
	SdfMgrTransport *transport;
	int (*receive)(SdfMgrTransport *, void *, SceSize);
	uint32_t receiveSize, receiveProgress;
	unsigned i;

	if (!buffer) {
#if FW_DEBUG
		static const SceKernelDebugInfo info = {
			.fileHash = 0x6364a003,
			.lineHash = 0xcf5df67e,
			.funcHash = 0xda0b64b9,
			.file = "sdfmgr.c",
			.line = 773,
			.func = "sceSDfMgrExRecv",
		};

		SceDeci4pSDfMgrForKernel_501CE61D(15, &info, "illegal buffer\n");
#endif
		return (int)0x80090005;
	}
	cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	for (i = 0; i < 8; i++) {
		if ((cpu->protocols[i].flags & 1) && cpu->protocols[i].handle == handle)
			break;
	}
	if (i == 8) {
#if FW_DEBUG
		static const SceKernelDebugInfo info = {
			.fileHash = 0x6364a003,
			.lineHash = 0xc25f26e9,
			.funcHash = 0xda0b64b9,
			.file = "sdfmgr.c",
			.line = 780,
			.func = "sceSDfMgrExRecv",
		};

		SceDeci4pSDfMgrForKernel_501CE61D(15, &info,
					 "invalid argument (sock: 0x%08X)\n", handle);
#endif
		return (int)0x80090002;
	}
	transport = s_sdfMgrState.cpu[sdfMgrCoreIndex()].transport;
	if (!transport) {
#if FW_DEBUG
		static const SceKernelDebugInfo info = {
			.fileHash = 0x6364a003,
			.lineHash = 0x76e351b2,
			.funcHash = 0xda0b64b9,
			.file = "sdfmgr.c",
			.line = 787,
			.func = "sceSDfMgrExRecv",
		};

		SceDeci4pSDfMgrForKernel_501CE61D(15, &info, "no host if\n");
#endif
		return (int)0x8009000d;
	}
	if (!(transport->flags & 4))
		return (int)0x80090007;
	if (transport->receiveSize)
		return transport->operations.receive(transport, buffer, size);
	if (size < sizeof(transport->receiveHeader))
		return (int)0x80090006;
	for (i = 0; i < sizeof(transport->receiveHeader); i++)
		((uint8_t *)buffer)[i] = ((const uint8_t *)&transport->receiveHeader)[i];
	receiveSize = *(const uint16_t *)transport->receiveHeader.size |
		      (uint32_t)transport->receiveHeader.size[2] << 16;
	receiveProgress = transport->receiveProgress;
	receive = transport->operations.receive;
	transport->receiveSize = receiveSize;
	transport->receiveProgress = receiveProgress + sizeof(transport->receiveHeader);
	return (int)((uint32_t)receive(transport,
			(uint8_t *)buffer + sizeof(transport->receiveHeader),
			size - sizeof(transport->receiveHeader)) +
		    sizeof(transport->receiveHeader));
}
