/*
 * The SDBGP protocol: one SDfMgr protocol per core, its receive, send and work
 * buffers, and the dispatch of the host's commands to the command groups.
 */

#include <string.h>
#include <scetypes.h>
#include <sysmem/cp15.h>
#include <sysmem/dipsw.h>
#include <sysmem/qafmgr.h>
#include <sysmem/suspend.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <sysmem/cpu.h>
#include <excpmgr/excpmgr.h>
#include "sdbgp_internal.h"

static const SceSDfMgrProtocol s_sdbgpProtocols[4] = {
	{ sizeof(SceSDfMgrProtocol), 0x10020000, 0, 0x01000105, 0x00010000,
	  { 0x00010000 }, "SDBGP0" },
	{ sizeof(SceSDfMgrProtocol), 0x10020001, 0, 0x01000105, 0x00010000,
	  { 0x00010000 }, "SDBGP1" },
	{ sizeof(SceSDfMgrProtocol), 0x10020002, 0, 0x01000105, 0x00010000,
	  { 0x00010000 }, "SDBGP2" },
	{ sizeof(SceSDfMgrProtocol), 0x10020003, 0, 0x01000105, 0x00010000,
	  { 0x00010000 }, "SDBGP3" },
};

static volatile SceKernelSpinlock s_sdbgpBufLock;
static volatile SceKernelSpinlock s_sdbgpFlagsLock;
static volatile SceKernelSpinlock s_sdbgpStatusLock;

SdbgpState s_sdbgpState;
SdbgpCpuState s_sdbgpCpu[4];

static inline SdbgpCpuState *sdbgpCurrentCpu(void)
{
	return &s_sdbgpCpu[readMpidr() & 3];
}

int sdbgpSuspend(int value, void *args)
{
	return 0;
}

int _sdbgpSendData(int handle, const void *buf, SceSize size)
{
	SceSize left;
	int result;

	for (left = size; left; left -= result) {
		result = sceSDfMgrExSend(handle, buf, left);
		buf = (const char *)buf + result;
		if (result < 0) {
			SceDeci4pSDfMgrForKernel_501CE61D(15, SCE_DBGINFO(750),
				"sceSDfMgrExSend() failed (0x%08X)\n", result);
			return result;
		}
	}
	return size;
}

int sdbgpUpdateBufStatus(uint32_t *status, uint32_t bits, int set)
{
	SceKernelIntrStatus intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_sdbgpBufLock);

	if (set)
		*status |= bits;
	else
		*status &= ~bits;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_sdbgpBufLock, intr);
	return 0;
}

__attribute__((noinline))
int _sdbgpFreeSendBuf(SdbgpCpuState *cpu)
{
	return sdbgpUpdateBufStatus(&cpu->sendStatus,
		SDBGP_BUF_LOCKED | SDBGP_BUF_FILLED | SDBGP_BUF_HEADER, 0);
}

/* Put the hardware break and watch point 0 back after a resume. */
int sdbgpResume(int value, void *args)
{
	SdbgpHwSlot *slot;
	uint32_t control;

	if (s_sdbgpState.hwbkpt[0].used) {
		uint32_t address = s_sdbgpState.hwbkpt[0].value;

		control = hwbkptMakeControl(s_sdbgpState.hwbkpt[0].control);
		writeDbgbvr(0, address);
		writeDbgbcr(0, control);
	} else {
		writeDbgbvr(0, 0);
		writeDbgbcr(0, 0);
	}
	slot = &s_sdbgpState.hwwatch[0];
	if (!slot->used) {
		writeDbgwvr(0, 0);
		writeDbgwcr(0, 0);
		return 0;
	}
	{
		uint32_t address = slot->value;

		control = hwwtptMakeControl(slot->control);
		writeDbgwvr(0, address);
		writeDbgwcr(0, control);
	}
	return 0;
}

SdbgpState *sdbgpGetState(void)
{
	return &s_sdbgpState;
}

SdbgpCpuState *sdbgpGetCpuState(void)
{
	return sdbgpCurrentCpu();
}

int SDbgpModuleStart(SceSize args, const void *argp)
{
	void **debugCpu;
	void *base;
	SceUID uid;
	int result;

	if (!sceKernelCheckDipsw(SDBGP_DIPSW_SYSTEM_DEBUG)) {
		SCE_DBG_PRINTF_LEVEL(1, 11, 154, "disabled by dipsw\n");
		return SCE_KERNEL_START_NO_RESIDENT;
	}
	if (!SceQafMgrForDriver_382C71E8())
		return SCE_KERNEL_START_NO_RESIDENT;
	debugCpu = (void **)SceDeci4pSDfMgrForKernel_BFB482D8();
	uid = sceKernelAllocMemBlockForKernel("SceDeci4pSDbgp",
		SceDeci4pSDfMgrForKernel_35238B8A(), SDBGP_MEMORY_SIZE, NULL);
	if (uid < 0)
		return uid;
	s_sdbgpState.memblock = uid;
	result = sceKernelGetMemBlockBase(uid, &base);
	if (result < 0)
		return result;
	s_sdbgpState.base = base;
	s_sdbgpState.debugCpu = base;
	*debugCpu = base;
	s_sdbgpState.enabled = 1;
	SceDeci4pSDfMgrForKernel_8A3B06F2(&s_sdbgpState.leakCheck, 0,
					  sizeof(s_sdbgpState.leakCheck));
	SceSysrootForKernel_21F5790B(SceDeci4pSDfMgrForKernel_98AEC60E);
	SceDebugForKernel_A465A31A(0);
	sdbgpInitProfiler(&s_sdbgpProfiler);
	if (sceKernelCheckDipsw(SDBGP_DIPSW_BACKGROUND_CHECK))
		bgCheckStart();
	SceSysrootForKernel_14D81797(&s_sdbgpProfiler);
	if (sceKernelCheckDipsw(SDBGP_DIPSW_NO_TRACE))
		return SCE_KERNEL_START_SUCCESS;
	uid = sceKernelAllocMemBlockForKernel("SceDeci4pSDbgpTrace",
		SceDeci4pSDfMgrForKernel_35238B8A(), SDBGP_TRACE_MEMORY_SIZE, NULL);
	if (uid < 0)
		return uid;
	s_sdbgpState.traceMemblock = uid;
	result = sceKernelGetMemBlockBase(uid, &base);
	if (result < 0)
		return result;
	s_sdbgpState.traceSize = SDBGP_TRACE_MEMORY_SIZE;
	s_sdbgpState.traceBase = base;
	return SCE_KERNEL_START_SUCCESS;
}

int SDbgpModuleBoot(SceSize args, const void *argp)
{
	SdbgpCpuState *cpu = sdbgpCurrentCpu();
	char *area;
	int result;

	SceDeci4pSDfMgrForKernel_8A3B06F2(cpu, 0, sizeof(cpu->status));
	area = (char *)s_sdbgpState.base + SDBGP_CPU_AREA_SIZE * sceKernelCpuId();
	cpu->recvBuf = (SdbgpHeader *)(area + SDBGP_RECV_BUF_OFFSET);
	cpu->sendBuf = (SdbgpHeader *)(area + SDBGP_SEND_BUF_OFFSET);
	cpu->workBuf = area + SDBGP_WORK_BUF_OFFSET;
	if (!sceKernelCheckDipsw(SDBGP_DIPSW_NO_TRACE)) {
		cpu->traceBuf = (char *)s_sdbgpState.traceBase +
				(sceKernelCpuId() << 20);
		SceDeci4pSDfMgrForKernel_8A3B06F2(cpu->traceBuf, 0,
						  SDBGP_TRACE_CPU_SIZE);
		traceInit();
	}
	result = sceSDfMgrOpen(_sdbgpRecvData,
			       &s_sdbgpProtocols[sceKernelCpuId()], cpu);
	if (result < 0)
		return result;
	cpu->handle = result;
	result = SceDeci4pSDfMgrForKernel_3A25594F(sdbgpControl);
	if (result < 0)
		return result;
	result = excpHandlerInit(cpu);
	if (result < 0)
		return result;
	cpu->hostMode = 1;
	if (sceKernelCpuId() != 0)
		return 0;
	hwwtptClear(SDBGP_HWWATCH_MAX);
	hwbkptClear(SDBGP_HWBKPT_MAX);
	sdbgpInitPhysicalAccess(swbkptClear(SDBGP_SWBKPT_MAX));
	SceExcpmgrForKernel_44CE04B8((uintptr_t)sdbgpProbeStart, (uintptr_t)sdbgpProbeEnd);
	SceKernelSuspendForDriver_CE7A2207(SDBGP_SUSPEND_INDEX, sdbgpSuspend, NULL);
	SceKernelSuspendForDriver_105C5752(SDBGP_SUSPEND_INDEX, sdbgpResume, NULL);
	return 0;
}

/* Copy a reply header into the send buffer. */
int sdbgpSendErrorHeader(SdbgpCpuState *cpu, uint8_t group, uint8_t command,
			 uint8_t tag, uint32_t result)
{
	SdbgpHeader header;

	header.tag = tag;
	header.group = group;
	header.command = command;
	header.result = result;
	header.type = 1;
	header.fragment = 0;
	header.fragments = 0;
	header.flags = 0;
	SceDeci4pSDfMgrForKernel_5996E2DB(cpu->sendBuf, &header, sizeof(header));
	return sdbgpUpdateBufStatus(&cpu->sendStatus, SDBGP_BUF_HEADER, 1);
}

uint32_t sdbgpGetFlags(SdbgpState *state, uint32_t mask)
{
	return state->flags & mask;
}

void sdbgpUpdateFlags(SdbgpState *state, uint32_t bits, int set)
{
	SceKernelIntrStatus intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_sdbgpFlagsLock);

	if (set)
		state->flags |= bits;
	else
		state->flags &= ~bits;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_sdbgpFlagsLock, intr);
}

uint32_t sdbgpGetStatus(SdbgpCpuState *cpu, uint32_t mask)
{
	return cpu->status & mask;
}

void sdbgpUpdateStatus(SdbgpCpuState *cpu, uint32_t bits, int set)
{
	SceKernelIntrStatus intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_sdbgpStatusLock);

	if (set)
		cpu->status |= bits;
	else
		cpu->status &= ~bits;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_sdbgpStatusLock, intr);
}

SceSize sdbgpGetRecvSize(SdbgpCpuState *cpu)
{
	return cpu->recvSize;
}

void *sdbgpAllocSendBuf(SdbgpCpuState *cpu, SceSize size)
{
	if (cpu->sendStatus & SDBGP_BUF_LOCKED)
		return NULL;
	if (size > SDBGP_SEND_BUF_SIZE)
		return NULL;
	sdbgpUpdateBufStatus(&cpu->sendStatus, SDBGP_BUF_LOCKED, 1);
	return cpu->sendBuf;
}

/* SDfMgr's control component: tell the host the system stopped or runs. */
void sdbgpControl(int id)
{
	int index = readMpidr() & 3;
	SdbgpCpuState *cpu = &s_sdbgpCpu[index];
	void *buf;
	int set;

	if (cpu->status & SDBGP_STATUS_STOPPED)
		return;
	if (id == 1) {
		buf = sdbgpAllocSendBuf(&s_sdbgpCpu[index], 0x5c);
		if (!buf)
			return;
		SceDeci4pSDfMgrForKernel_8A3B06F2(buf, 0, 0x5c);
		cpu->sendSize = 0x5c;
		sdbgpUpdateBufStatus(&cpu->sendStatus, SDBGP_BUF_FILLED, id);
		sdbgpSendErrorHeader(&s_sdbgpCpu[index], SDBGP_GROUP_SYSTEM,
				     SDBGP_NOTIFY_BREAK, 0, 0);
		set = id;
	} else {
		if (id != 0)
			return;
		buf = sdbgpAllocSendBuf(&s_sdbgpCpu[index], 0x5c);
		if (!buf)
			return;
		SceDeci4pSDfMgrForKernel_8A3B06F2(buf, id, 0x5c);
		cpu->sendSize = 0x5c;
		sdbgpUpdateBufStatus(&cpu->sendStatus, SDBGP_BUF_FILLED, 1);
		sdbgpSendErrorHeader(&s_sdbgpCpu[index], SDBGP_GROUP_SYSTEM,
				     SDBGP_NOTIFY_CONTINUE, id, id);
		set = 1;
	}
	sdbgpUpdateStatus(&s_sdbgpCpu[index], set, set);
	{
		int result = sceSDfMgrExReqSend(cpu->handle);

		if (result >= 0) {
			SceDeci4pSDfMgrForKernel_98AEC60E();
			return;
		}
		if (result == SDFMGR_ERROR_ALREADY_REQUESTED)
			return;
		if (cpu->sendStatus & SDBGP_BUF_LOCKED)
			_sdbgpFreeSendBuf(&s_sdbgpCpu[index]);
		sdbgpUpdateStatus(&s_sdbgpCpu[index], SDBGP_STATUS_SENDING, 0);
	}
}

/* Tell the host about an exception, with the fault registers. */
int sdbgpSendNotification(SdbgpCpuState *cpu, SceUID pid, SceUID tid, uint32_t pc,
			  uint32_t cpsr, const SdbgpExcpDetail *detail)
{
	SdbgpExcpNotification *packet = sdbgpAllocSendBuf(cpu, sizeof(*packet));
	int result;

	if (!packet)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(packet, 0, sizeof(*packet));
	cpu->sendSize = sizeof(*packet);
	sdbgpUpdateBufStatus(&cpu->sendStatus, SDBGP_BUF_FILLED, 1);
	packet->processId = pid;
	packet->threadId = tid;
	packet->pc = pc;
	packet->ifsr = readIfsr();
	packet->ifar = readIfar();
	packet->dfsr = readDfsr();
	packet->dfar = readDfar();
	packet->cpsr = cpsr;
	packet->fpscr = readFpscr();
	packet->detail = *detail;
	sdbgpSendErrorHeader(cpu, SDBGP_GROUP_SYSTEM, SDBGP_NOTIFY_EXCEPTION, 0, 0);
	sdbgpUpdateStatus(cpu, SDBGP_STATUS_SENDING, 1);
	result = sceSDfMgrExReqSend(cpu->handle);
	if (result >= 0) {
		SceDeci4pSDfMgrForKernel_98AEC60E();
		return 0;
	}
	if (result == SDFMGR_ERROR_ALREADY_REQUESTED)
		return 0;
	if (cpu->sendStatus & SDBGP_BUF_LOCKED)
		_sdbgpFreeSendBuf(cpu);
	sdbgpUpdateStatus(cpu, SDBGP_STATUS_SENDING, 0);
	return result;
}

/* Reply with an error header alone. */
int sdbgpSendError(SdbgpCpuState *cpu, uint8_t group, uint8_t command, uint8_t tag,
		   uint32_t result)
{
	void *buf = sdbgpAllocSendBuf(cpu, sizeof(SdbgpHeader));
	int ret;

	if (!buf) {
		if (cpu->sendStatus & SDBGP_BUF_LOCKED)
			_sdbgpFreeSendBuf(cpu);
		buf = sdbgpAllocSendBuf(cpu, sizeof(SdbgpHeader));
		if (!buf)
			return 0;
	}
	SceDeci4pSDfMgrForKernel_8A3B06F2(buf, 0, sizeof(SdbgpHeader));
	cpu->sendSize = sizeof(SdbgpHeader);
	sdbgpUpdateBufStatus(&cpu->sendStatus, SDBGP_BUF_FILLED, 1);
	sdbgpSendErrorHeader(cpu, group, command, tag, result);
	sdbgpUpdateStatus(cpu, SDBGP_STATUS_SENDING, 1);
	ret = sceSDfMgrExReqSend(cpu->handle);
	if (ret >= 0 || ret == SDFMGR_ERROR_ALREADY_REQUESTED)
		return ret;
	if (cpu->sendStatus & SDBGP_BUF_LOCKED)
		_sdbgpFreeSendBuf(cpu);
	sdbgpUpdateStatus(cpu, SDBGP_STATUS_SENDING, 0);
	return ret;
}

/* Answer a command no handler takes. */
static inline int sdbgpNotSupported(SdbgpCpuState *cpu, const SdbgpHeader *packet,
				    uint8_t group)
{
	sdbgpSendError(cpu, group, packet->command + 1, packet->tag,
		       SDBGP_RESULT_NOT_SUPPORTED);
	return SCE_KERNEL_ERROR_UNSUP;
}

static inline int _sdbgpParseRecvPacket(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	switch (packet->group) {
	case SDBGP_GROUP_SYSTEM:
		switch (packet->command) {
		case 0x00: return sdbgpCmd10_00(cpu, packet, packet->command);
		case 0x08: return sdbgpCmd10_08(cpu, packet, packet->command);
		case 0x0a: return sdbgpCmd10_0a(cpu, packet, packet->command);
		case 0x10: return sdbgpCmd10_10(cpu, packet, packet->command);
		case 0x12: return sdbgpCmd10_12(cpu, packet, packet->command);
		case 0x1a: return sdbgpCmd10_1a(cpu, packet, packet->command);
		case 0x1c: return sdbgpCmd10_1c(cpu, packet, packet->command);
		case 0x30: return sdbgpCmd10_30(cpu, packet, packet->command);
		case 0x32: return sdbgpCmd10_32(cpu, packet, packet->command);
		case 0x34: return sdbgpCmd10_34(cpu, packet, packet->command);
		case 0x40: return sdbgpCmd10_40(cpu, packet, packet->command);
		case 0x42: return sdbgpCmd10_42(cpu, packet, packet->command);
		case 0x44: return sdbgpCmd10_44(cpu, packet, packet->command);
		case 0x48: return sdbgpCmd10_48(cpu, packet, packet->command);
		case 0x4a: return sdbgpCmd10_4a(cpu, packet, packet->command);
		case 0x4c: return sdbgpCmd10_4c(cpu, packet, packet->command);
		case 0x50: return sdbgpCmd10_50(cpu, packet, packet->command);
		case 0x52: return sdbgpCmd10_52(cpu, packet, packet->command);
		case 0x54: return sdbgpCmd10_54(cpu, packet, packet->command);
		case 0x62: return sdbgpCmd10_62(cpu, packet, packet->command);
		case 0x64: return sdbgpCmd10_64(cpu, packet, packet->command);
		case 0x72: return sdbgpCmd10_72(cpu, packet, packet->command);
		case 0x90: return sdbgpCmd10_90(cpu, packet, packet->command);
		case 0x92: return sdbgpCmd10_92(cpu, packet, packet->command);
		case 0x94: return sdbgpCmd10_94(cpu, packet, packet->command);
		case 0x96: return sdbgpCmd10_96(cpu, packet, packet->command);
		case 0x98: return sdbgpCmd10_98(cpu, packet, packet->command);
		case 0x9a: return sdbgpCmd10_9a(cpu, packet, packet->command);
		}
		return sdbgpNotSupported(cpu, packet, SDBGP_GROUP_SYSTEM);
	case SDBGP_GROUP_PROCESS:
		switch (packet->command) {
		case 0x00: return sdbgpCmd20_00(cpu, packet, packet->command);
		case 0x10: return sdbgpCmd20_10(cpu, packet, packet->command);
		case 0x12: return sdbgpCmd20_12(cpu, packet, packet->command);
		case 0x20: return sdbgpCmd20_20(cpu, packet, packet->command);
		case 0x22: return sdbgpCmd20_22(cpu, packet, packet->command);
		case 0x52: return sdbgpCmd20_52(cpu, packet, packet->command);
		case 0x54: return _getHeapInfoAll(cpu, packet, packet->command);
		case 0x56: return sdbgpCmd20_56(cpu, packet, packet->command);
		case 0x58: return sdbgpCmd20_58(cpu, packet, packet->command);
		case 0x5a: return _getFixedHeapInfo(cpu, packet, packet->command);
		case 0x60: return sdbgpCmd20_60(cpu, packet, packet->command);
		case 0x80: return sdbgpCmd20_80(cpu, packet, packet->command);
		case 0x82: return sdbgpCmd20_82(cpu, packet, packet->command);
		case 0x84: return sdbgpCmd20_84(cpu, packet, packet->command);
		}
		return sdbgpNotSupported(cpu, packet, SDBGP_GROUP_PROCESS);
	case SDBGP_GROUP_THREAD:
		switch (packet->command) {
		case 0x00: return sdbgpCmd30_00(cpu, packet);
		case 0x02: return sdbgpCmd30_02(cpu, packet);
		case 0x10: return sdbgpCmd30_10(cpu, packet);
		case 0x12: return sdbgpCmd30_12(cpu, packet);
		case 0x20: return sdbgpCmd30_20(cpu, packet);
		case 0x40: return sdbgpCmd30_40(cpu, packet);
		case 0x50: return sdbgpCmd30_50(cpu, packet);
		}
		return sdbgpNotSupported(cpu, packet, SDBGP_GROUP_THREAD);
	case SDBGP_GROUP_MODULE:
		switch (packet->command) {
		case 0x00: return sdbgpCmd40_00(cpu, packet);
		case 0x10: return sdbgpCmd40_10(cpu, packet);
		case 0x12: return sdbgpCmd40_12(cpu, packet);
		case 0x30: return sdbgpCmd40_30(cpu, packet);
		}
		return sdbgpNotSupported(cpu, packet, SDBGP_GROUP_MODULE);
	case SDBGP_GROUP_NOP:
		return 0;
	case SDBGP_GROUP_FILE:
		switch (packet->command) {
		case 0x00: return sdbgpCmd60_00(cpu, packet);
		case 0x12: return sdbgpCmd60_12(cpu, packet);
		case 0x14: return sdbgpCmd60_14(cpu, packet);
		case 0x16: return sdbgpCmd60_16(cpu, packet);
		case 0x18: return sdbgpCmd60_18(cpu, packet);
		case 0x1a: return sdbgpCmd60_1a(cpu, packet);
		}
		return sdbgpNotSupported(cpu, packet, SDBGP_GROUP_FILE);
	case SDBGP_GROUP_MISC:
		switch (packet->command) {
		case 0x00: return sdbgpCmd70_00(cpu, packet);
		case 0x02: return sdbgpCmd70_02(cpu, packet);
		case 0x04: return sdbgpCmd70_04(cpu, packet);
		case 0x06: return sdbgpCmd70_06(cpu, packet);
		}
		return sdbgpNotSupported(cpu, packet, SDBGP_GROUP_MISC);
	case SDBGP_GROUP_TRACE:
		if (sceKernelCheckDipsw(SDBGP_DIPSW_NO_TRACE)) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 11, 1169, "_sdbgpParseRecvPacket",
						  "disabled by dipsw\n");
			sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
				       SDBGP_RESULT_NOT_SUPPORTED);
			return SCE_KERNEL_ERROR_UNSUP;
		}
		switch (packet->command) {
		case 0x00: return sdbgpCmd90_00(cpu, packet);
		case 0x02: return sdbgpCmd90_02(cpu, packet);
		case 0x04: return sdbgpCmd90_04(cpu, packet);
		}
		return sdbgpNotSupported(cpu, packet, packet->group);
	case SDBGP_GROUP_KERNEL:
		switch (packet->command) {
		case 0x02: return sdbgpCmdF0_02(cpu, packet);
		case 0x08: return sdbgpCmdF0_08(cpu, packet);
		case 0x0a: return sdbgpCmdF0_0a(cpu, packet);
		case 0x0c: return sdbgpCmdF0_0c(cpu, packet);
		case 0x0e: return sdbgpCmdF0_0e(cpu, packet);
		case 0x10: return sdbgpCmdF0_10(cpu, packet);
		case 0x12: return sdbgpCmdF0_12(cpu, packet);
		case 0x14: return sdbgpCmdF0_14(cpu, packet);
		case 0x16: return sdbgpCmdF0_16(cpu, packet);
		case 0x18: return sdbgpCmdF0_18(cpu, packet);
		case 0x1a: return sdbgpCmdF0_1a(cpu, packet);
		case 0x1c: return sdbgpCmdF0_1c(cpu, packet);
		case 0x20: return sdbgpCmdF0_20(cpu, packet);
		case 0x30: return sdbgpCmdF0_30(cpu, packet);
		}
		return sdbgpNotSupported(cpu, packet, SDBGP_GROUP_KERNEL);
	}
	return sdbgpNotSupported(cpu, packet, packet->group);
}

/* Send the next part of a reply too large for one packet. */
static inline int _sdbgpEventSend(SdbgpCpuState *cpu)
{
	SdbgpHeader *packet = cpu->sendBuf;
	SceSize size, total;
	int result;

	if (!(cpu->sendStatus & SDBGP_BUF_LOCKED))
		return 0;
	if ((cpu->sendStatus & (SDBGP_BUF_FILLED | SDBGP_BUF_HEADER)) !=
	    (SDBGP_BUF_FILLED | SDBGP_BUF_HEADER))
		return 0;
	size = cpu->sendSize;
	if (!size)
		return 0;
	if (!(cpu->sendStatus & SDBGP_BUF_FRAGMENTED)) {
		if (size > SDBGP_PACKET_MAX) {
			sdbgpUpdateBufStatus(&cpu->sendStatus, SDBGP_BUF_FRAGMENTED, 1);
			cpu->sendResult = packet->result;
			packet->flags |= SDBGP_PACKET_CONTINUED;
			cpu->sendFragments = (cpu->sendSize - sizeof(SdbgpHeader)) /
					     SDBGP_PAYLOAD_MAX + 1;
			packet->fragments = cpu->sendFragments;
			if (sdbgpIsNotification(packet->group, packet->command))
				packet->result = cpu->flags;
			else
				packet->result = 0;
			size = SDBGP_PACKET_MAX;
			cpu->sendOffset = SDBGP_PACKET_MAX;
			cpu->sendFragment = 0;
		} else {
			cpu->sendFragment = 0;
			cpu->sendFragments = 0;
		}
		SceDeci4pSDfMgrForKernel_FFC95B63(&packet->hdr, size);
		return _sdbgpSendData(cpu->handle, packet, size);
	}
	size -= cpu->sendOffset;
	cpu->sendFragment++;
	if (size <= SDBGP_PAYLOAD_MAX) {
		packet->flags &= ~SDBGP_PACKET_CONTINUED;
		if (sdbgpIsNotification(packet->group, packet->command))
			packet->result = cpu->flags;
		else
			packet->result = cpu->sendResult;
		sdbgpUpdateBufStatus(&cpu->sendStatus, SDBGP_BUF_FRAGMENTED, 0);
	}
	total = size + sizeof(SdbgpHeader);
	if (total > SDBGP_PACKET_MAX) {
		total = SDBGP_PACKET_MAX;
		size = SDBGP_PAYLOAD_MAX;
	}
	SceDeci4pSDfMgrForKernel_FFC95B63(&packet->hdr, total);
	packet->fragment = cpu->sendFragment;
	packet->fragments = cpu->sendFragments;
	result = _sdbgpSendData(cpu->handle, packet, sizeof(SdbgpHeader));
	if (result < 0)
		return result;
	packet = (SdbgpHeader *)((char *)packet + cpu->sendOffset);
	cpu->sendOffset += size;
	return _sdbgpSendData(cpu->handle, packet, size);
}

void _sdbgpRecvData(int event, void *arg, void *context)
{
	SdbgpCpuState *cpu = context;
	const SceDfMgrStatus *status;
	SdbgpHeader *packet;
	char *p;
	SceSize size, left;
	int result;

	switch (event) {
	case SCE_DFMGR_EVENT_RECV:
		hwwtptDisable();
		if (cpu->recvStatus & SDBGP_BUF_LOCKED)
			hwwtptEnable(cpu);
			return;
		sdbgpUpdateBufStatus(&cpu->recvStatus, SDBGP_BUF_LOCKED, 1);
		packet = cpu->recvBuf;
		if (!packet)
			hwwtptEnable(cpu);
			return;
		size = (SceSize)arg;
		if (size) {
			p = (char *)packet;
			for (left = size; left; left -= result) {
				result = sceSDfMgrExRecv(cpu->handle, p, left);
				p += result;
				if (result < 0) {
					SceDeci4pSDfMgrForKernel_501CE61D(15, SCE_DBGINFO(728),
						"sceSDfMgrExRecv() failed (0x%08X)\n", result);
					hwwtptEnable(cpu);
					return;
				}
			}
			if ((int)size < 0)
				hwwtptEnable(cpu);
				return;
			packet = cpu->recvBuf;
		}
		cpu->recvSize = size;
		result = _sdbgpParseRecvPacket(cpu, packet);
		if (cpu->workStatus & SDBGP_BUF_LOCKED)
			sdbgpUpdateBufStatus(&cpu->workStatus, SDBGP_BUF_LOCKED, 0);
		if (cpu->recvStatus & SDBGP_BUF_LOCKED)
			sdbgpUpdateBufStatus(&cpu->recvStatus, SDBGP_BUF_LOCKED, 0);
		if (result >= 0) {
			sdbgpUpdateStatus(cpu, SDBGP_STATUS_SENDING, 1);
			result = sceSDfMgrExReqSend(cpu->handle);
			if (result < 0 && result != SDFMGR_ERROR_ALREADY_REQUESTED) {
				if (cpu->sendStatus & SDBGP_BUF_LOCKED)
					_sdbgpFreeSendBuf(cpu);
				sdbgpUpdateStatus(cpu, SDBGP_STATUS_SENDING, 0);
			}
		}
		hwwtptEnable(cpu);
		return;
	case SCE_DFMGR_EVENT_SEND:
		_sdbgpEventSend(cpu);
		return;
	case SCE_DFMGR_EVENT_SENT:
		if (cpu->sendStatus & SDBGP_BUF_FRAGMENTED)
			sceSDfMgrExReqSend(cpu->handle);
			return;
		if (cpu->sendStatus & SDBGP_BUF_LOCKED)
			_sdbgpFreeSendBuf(cpu);
		result = SceDeci4pSDfMgrForKernel_C38EA67A();
		if (result)
			SceDeci4pSDfMgrForKernel_58D7597A();
			return;
		return;
	case SCE_DFMGR_EVENT_STATUS:
		status = arg;
		if (status->kind == 0) {
			result = status->connected;
		} else {
			if (status->kind != 1)
				return;
			result = status->enabled;
		}
		if (result == 0)
			cpu->hostMode = 0;
		else if (result == 1)
			cpu->hostMode = 1;
		return;
	}
}

void sdbgpSetSendSize(SdbgpCpuState *cpu, SceSize size)
{
	cpu->sendSize = size;
	sdbgpUpdateBufStatus(&cpu->sendStatus, SDBGP_BUF_FILLED, 1);
}

void *sdbgpAllocWorkBuf(SdbgpCpuState *cpu, SceSize size)
{
	if (cpu->workStatus & SDBGP_BUF_LOCKED)
		return NULL;
	if (size > SDBGP_WORK_BUF_SIZE)
		return NULL;
	sdbgpUpdateBufStatus(&cpu->workStatus, SDBGP_BUF_LOCKED, 1);
	return cpu->workBuf;
}

/* Read 2 or 4 bytes of a process's memory, the kernel's first. */
int sdbgpReadMemory(SceUID pid, const void *addr, SceSize size, void *out)
{
	int result;

	if (size != 2 && size != 4)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	if (size == 4) {
		if (sceKernelIsAccessibleRangeProc(SDBGP_KERNEL_PROCESS_ID, 1, addr, 4) >= 0) {
			SceDeci4pSDfMgrForKernel_5996E2DB(out, addr, size);
			return 0;
		}
	} else if (sceKernelIsAccessibleRangeProc(SDBGP_KERNEL_PROCESS_ID, 1, addr, 2) >= 0) {
		SceDeci4pSDfMgrForKernel_5996E2DB(out, addr, 2);
		return 0;
	}
	if (pid == SDBGP_KERNEL_PROCESS_ID)
		return SDBGP_ERROR_ILLEGAL_ADDRESS;
	if (size == 4) {
		result = sceKernelIsAccessibleRangeProc(pid, 1, addr, 4);
		if (result < 0)
			return result;
		size = 4;
	} else {
		result = sceKernelIsAccessibleRangeProc(pid, 1, addr, 2);
		if (result < 0)
			return result;
		size = 2;
	}
	result = sceKernelCopyFromUserProc(pid, out, addr, size);
	return result & (result >> 31);
}

/* Write 2 or 4 bytes of code, the kernel's directly, a process's through sysmem. */
int sdbgpWriteMemory(SceUID pid, void *addr, SceSize size, const void *in)
{
	uint32_t dacr;
	SceSize count;
	int result;

	if (size != 2 && size != 4)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	if (sceKernelIsAccessibleRangeProc(SDBGP_KERNEL_PROCESS_ID, 1, addr,
					size == 4 ? 4 : 2) >= 0) {
		count = size == 4 ? 4 : 2;
		dacr = readDacr();
		writeDacr(0xffff0000);
		memcpy(addr, in, count);
		sceKernelDcacheCleanRangeForL1WBWA((void *)((uintptr_t)addr & ~0x1f),
			(((uintptr_t)addr + count + 31) & ~0x1f) - ((uintptr_t)addr & ~0x1f));
		writeDacr(dacr);
		sceKernelL1IcacheInvalidateEntireAllCore();
		return 0;
	}
	if (pid == SDBGP_KERNEL_PROCESS_ID)
		return SDBGP_ERROR_ILLEGAL_ADDRESS;
	if (size == 4) {
		result = sceKernelIsAccessibleRangeProc(pid, 1, addr, 4);
		if (result < 0)
			return result;
		count = 4;
	} else {
		result = sceKernelIsAccessibleRangeProc(pid, 1, addr, 2);
		if (result < 0)
			return result;
		count = 2;
	}
	result = sceKernelCopyToUserProcTextDomainForDBGP(pid, addr, in, count);
	if (result < 0)
		return result;
	sceKernelL1IcacheInvalidateEntireAllCore();
	return 0;
}
