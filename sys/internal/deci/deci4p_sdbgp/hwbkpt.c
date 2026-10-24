/*
 * Hardware break points and the watch point: break point 0 and the watch point
 * on every core (through the cores' CoreSight pages), break point 5 in a thread
 * or process context.
 */

#include <string.h>
#include <scetypes.h>
#include <sysmem/cp15.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <pamgr/coresight_regs.h>
#include <processmgr/processmgr.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/threadmgr_debugger.h>
#include "sdbgp_internal.h"

/* the context break point of a thread or process */
#define HWBKPT_CONTEXT			5

/* the break point list, then the threads read to fill it (the work buffer) */
#define HWBKPT_LIST_SIZE		2000
#define HWBKPT_THREADS_SIZE		(SDBGP_WORK_BUF_SIZE - HWBKPT_LIST_SIZE)

#define HWBKPT_WRITE(address, var) do {						\
	result = sceKernelPhysicalMemWriteForDebugger((address), &(var), 4, 4);	\
	if (result < 0)								\
		return result;							\
} while (0)

/* Break point 0 of every core. */
int hwbkptWriteAllCpu(uint32_t value, uint32_t control)
{
	uint32_t key = DBG_LAR_KEY;
	int result;

	HWBKPT_WRITE(CORESIGHT_DBG_BASE(0) + DBG_LAR, key);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(1) + DBG_LAR, key);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(2) + DBG_LAR, key);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(3) + DBG_LAR, key);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(0) + DBG_BVR(0), value);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(0) + DBG_BCR(0), control);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(1) + DBG_BVR(0), value);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(1) + DBG_BCR(0), control);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(2) + DBG_BVR(0), value);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(2) + DBG_BCR(0), control);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(3) + DBG_BVR(0), value);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(3) + DBG_BCR(0), control);
	key = ~key;
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(0) + DBG_LAR, key);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(1) + DBG_LAR, key);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(2) + DBG_LAR, key);
	result = sceKernelPhysicalMemWriteForDebugger(CORESIGHT_DBG_BASE(3) + DBG_LAR, &key, 4, 4);
	if (result < 0)
		return result;
	return 0;
}

/* Whether the thread's context break point stops it at pc. */
int hwbkptCheckStop(SceUID pid, SceUID tid, uintptr_t pc, uint32_t cpsr)
{
	uint32_t value, control, mode;
	int result;

	if ((readDbgdscrExt() & DBGDSCR_MOE_MASK) != DBGDSCR_MOE_BREAKPOINT ||
	    sceKernelGetTHBP(tid, HWBKPT_CONTEXT, &value, &control) < 0)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	if (!(control & DBGBCR_ENABLE))
		return SDBGP_STOP_HWBKPT_PASS;
	if (control & DBGBCR_MISMATCH) {
		if (value == pc)
			return SDBGP_STOP_HWBKPT_PASS;
	} else if ((control & DBGBCR_BAS_MASK) == DBGBCR_BAS_THUMB_HIGH) {
		if (pc != value + 2)
			return SDBGP_STOP_HWBKPT_PASS;
	} else if (value != pc) {
		return SDBGP_STOP_HWBKPT_PASS;
	}
	mode = cpsr & PSR_MODE_MASK;
	switch (control & DBGBCR_PMC_MASK) {
	case 0:
		if (mode == PSR_MODE_SYS || mode == PSR_MODE_USR)
			result = SDBGP_STOP_HWBKPT;
		else if (mode == PSR_MODE_SVC)
			result = SDBGP_STOP_HWBKPT;
		else
			result = SDBGP_STOP_HWBKPT_PASS;
		break;
	case DBGBCR_PMC_PRIVILEGED:
		if (mode == PSR_MODE_USR)
			result = SDBGP_STOP_HWBKPT_PASS;
		else
			result = SDBGP_STOP_HWBKPT;
		break;
	case DBGBCR_PMC_USER:
		if (mode == PSR_MODE_USR)
			result = SDBGP_STOP_HWBKPT;
		else
			result = SDBGP_STOP_HWBKPT_PASS;
		break;
	case DBGBCR_PMC_ANY:
		result = SDBGP_STOP_HWBKPT;
		break;
	default:
		result = SDBGP_STOP_HWBKPT_PASS;
		break;
	}
	return result;
}

void hwbkptClear(int index)
{
	SdbgpState *state = sdbgpGetState();

	if (index == SDBGP_HWBKPT_MAX)
		SceDeci4pSDfMgrForKernel_8A3B06F2(state->hwbkpt, 0, sizeof(state->hwbkpt));
	else
		SceDeci4pSDfMgrForKernel_8A3B06F2(&state->hwbkpt[index], 0, sizeof(state->hwbkpt[0]));
}

/* The break point of a thread, or of a process or the system (tid -1). */
int _getHwbkpt(SdbgpHwbkptListReply *reply, SceUID pid, SceUID tid,
	       ThreadmgrThreadInfoForDebugger *info)
{
	SdbgpState *state = sdbgpGetState();
	uint32_t value, control;
	int result;

	if (tid != -1) {
		result = sceKernelGetTHBP(tid, HWBKPT_CONTEXT, &value, &control);
		if (result < 0)
			return result;
		info->size = sizeof(*info);
		SceThreadmgrForKernel_C976719E(tid, 0, info);
		reply->num = 1;
		reply->info[0].size = sizeof(reply->info[0]);
		if (info->processId != -1)
			reply->info[0].processId = info->processId;
		else
			reply->info[0].processId = pid;
	} else {
		if (pid == -1) {
			value = state->hwbkpt[0].value;
			control = state->hwbkpt[0].control;
		} else {
			result = sceKernelGetTHBP(tid, HWBKPT_CONTEXT, &value, &control);
			if (result < 0)
				return result;
		}
		reply->num = 1;
		reply->info[0].size = sizeof(reply->info[0]);
		reply->info[0].processId = pid;
	}
	reply->info[0].threadId = tid;
	reply->info[0].value = value;
	reply->info[0].control = control;
	return 0;
}

/* Every thread's break point and the system's; the threads are read behind the list. */
int hwbkptGetHwbkptAll(SdbgpHwbkptInfo *info, SceSize *num)
{
	ThreadmgrThreadInfoForDebugger *threads;
	SdbgpState *state;
	uint32_t value = 0, control = 0;
	SceSize count;
	int result, i, n;

	if (num == NULL || info == NULL)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	threads = (ThreadmgrThreadInfoForDebugger *)((char *)info + HWBKPT_LIST_SIZE);
	state = sdbgpGetState();
	result = SceThreadmgrForKernel_B011EA69(0, threads, HWBKPT_THREADS_SIZE, &count);
	if (result < 0)
		return result;
	if (result > (int)count)
		SCE_DBG_PRINTF_LEVEL(1, 0, 194,
			"Not enough memory, can't get all threads information\n");
	n = 0;
	for (i = 0; i < (int)count; i++) {
		if (sceKernelGetTHBP(threads[i].threadId, HWBKPT_CONTEXT, &value, &control) < 0)
			continue;
		if (value == 0 && control == 0)
			continue;
		info[n].size = sizeof(info[n]);
		info[n].processId = threads[i].processId;
		info[n].threadId = threads[i].threadId;
		info[n].value = value;
		info[n].control = control & ~DBGBCR_PMC_MASK;
		value = 0;
		control = 0;
		n++;
	}
	SceDeci4pSDfMgrForKernel_8A3B06F2(threads, 0, HWBKPT_THREADS_SIZE);
	if (state->hwbkpt[0].used) {
		info[n].size = sizeof(info[n]);
		info[n].processId = -1;
		info[n].threadId = -1;
		info[n].value = state->hwbkpt[0].value;
		info[n].control = state->hwbkpt[0].control;
		n++;
	}
	*num = n;
	return 0;
}

/* Set the break point in a thread, in a process's threads, or in the system. */
int hwbkptSetHwbkpt(const SdbgpHwbkptSetRequest *request)
{
	SdbgpCpuState *cpu = sdbgpGetCpuState();
	SdbgpState *state = sdbgpGetState();
	ThreadmgrThreadInfoForDebugger *threads;
	SceUID pid = request->processId;
	SceUID tid = request->threadId;
	uint32_t value = request->value;
	uint32_t control = request->control;
	uint32_t bcr = control | DBGBCR_ENABLE | DBGBCR_PMC_ANY;
	SceSize size;
	int count, result, i;

	if (tid != -1) {
		result = sceKernelSetTHBP(tid, HWBKPT_CONTEXT, value, bcr);
		return result < 0 ? SDBGP_ERROR_INVALID_ARGUMENT : 0;
	}
	if (pid == -1) {
		hwbkptWriteAllCpu(value, bcr);
		state->hwbkpt[0].threadId = pid;
		state->hwbkpt[0].processId = pid;
		state->hwbkpt[0].value = value;
		state->hwbkpt[0].control = control;
		state->hwbkpt[0].used = 1;
		return 0;
	}
	if (SceProcessmgrForKernel_FC80A606(pid, HWBKPT_CONTEXT, value, bcr) < 0)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	count = SceThreadmgrForKernel_6351A13A(pid, 0, NULL, 0, NULL);
	if (count < 0)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	size = count * sizeof(*threads);
	threads = sdbgpAllocWorkBuf(cpu, size);
	if (threads == NULL ||
	    SceThreadmgrForKernel_6351A13A(pid, 0, threads, size, (SceSize *)&count) < 0)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	for (i = 0; i < count; i++) {
		if (sceKernelSetTHBP(threads[i].threadId, HWBKPT_CONTEXT, value, bcr) < 0)
			return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	return 0;
}

int clearHwbkpt(SceUID pid, SceUID tid, uint32_t value)
{
	SdbgpState *state = sdbgpGetState();
	int result;

	if (tid != -1) {
		result = sceKernelSetTHBP(tid, HWBKPT_CONTEXT, 0, 0);
		if (result < 0)
			return result;
	}
	if (state->hwbkpt[0].value == value) {
		hwbkptWriteAllCpu(0, 0);
		hwbkptClear(0);
	}
	return 0;
}

/* Clear every thread's break point and the system's. */
int hwbkptClearHwbkptAll(ThreadmgrThreadInfoForDebugger *threads)
{
	SceSize count;
	int result, i;

	if (threads == NULL)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	result = SceThreadmgrForKernel_B011EA69(0, threads, SDBGP_WORK_BUF_SIZE, &count);
	if (result < 0)
		return result;
	if (result > (int)count)
		SCE_DBG_PRINTF_LEVEL(1, 0, 387,
			"Not enough memory, can't get all threads information\n");
	for (i = 0; i < (int)count; i++)
		sceKernelSetTHBP(threads[i].threadId, HWBKPT_CONTEXT, 0, 0);
	hwbkptWriteAllCpu(0, 0);
	hwbkptClear(0);
	return 0;
}

/* Take out the break point the core stopped at, to step over it. */
int hwbkptDisableHWBP(void)
{
	SdbgpState *state = sdbgpGetState();
	SdfMgrDebugCpuState *debugCpu;
	uint32_t value = 0, control = 0;
	SceUID pid, tid;
	uintptr_t pc;
	int cpu = sceKernelCpuId();

	if (state->debugCpu) {
		debugCpu = &state->debugCpu[cpu];
		pid = debugCpu->pid;
		tid = debugCpu->tid;
		pc = debugCpu->pc;
	} else {
		pc = -1;
		pid = -1;
		tid = -1;
	}
	SCE_DBG_PRINTF_LEVEL(2, 0, 586, "hwbkptDisableHWBP pc=0x%08x tid=0x%08x\n", pc, tid);
	value = readDbgbvr(0);
	if (state->hwbkpt[0].used)
		control = state->hwbkpt[0].control | DBGBCR_ENABLE | DBGBCR_PMC_ANY;
	if ((control & DBGBCR_MISMATCH) || pc == value) {
		state->disabledHwbkpt.value = value;
		state->disabledHwbkpt.control = control;
		control &= ~DBGBCR_ENABLE;
		state->disabledHwbkpt.where = SDBGP_HWBKPT_IN_BP0;
		writeDbgbcr(0, control);
		return 0;
	}
	value = readDbgbvr(1);
	control = readDbgbcr(1);
	if ((control & DBGBCR_MISMATCH) || value == pc) {
		state->disabledHwbkpt.value = value;
		state->disabledHwbkpt.control = control;
		control &= ~DBGBCR_ENABLE;
		state->disabledHwbkpt.where = SDBGP_HWBKPT_IN_BP1;
		writeDbgbcr(1, control);
		return 0;
	}
	sceKernelGetPHBP(pid, 2, &value, &control);
	sceKernelGetPHBP(pid, 3, &value, &control);
	sceKernelGetPHBP(pid, 4, &value, &control);
	if ((control & DBGBCR_MISMATCH) || value == pc) {
		state->disabledHwbkpt.value = value;
		state->disabledHwbkpt.control = control;
		state->disabledHwbkpt.processId = pid;
		state->disabledHwbkpt.where = SDBGP_HWBKPT_IN_PROCESS;
	}
	sceKernelGetTHBP(tid, HWBKPT_CONTEXT, &value, &control);
	if (((control & DBGBCR_MISMATCH) && value != pc) || (pc & ~3) == value) {
		state->disabledHwbkpt.control = control;
		state->disabledHwbkpt.value = value;
		state->disabledHwbkpt.threadId = tid;
		state->disabledHwbkpt.where = SDBGP_HWBKPT_IN_THREAD;
		control &= ~DBGBCR_ENABLE;
		sceKernelSetTHBP(tid, HWBKPT_CONTEXT, value, control);
	}
	return 0;
}

/* Put back the break point hwbkptDisableHWBP took out. */
int hwbkptEnableHWBP(int flags)
{
	SdbgpState *state = sdbgpGetState();
	uint32_t where = state->disabledHwbkpt.where;
	uint32_t value = state->disabledHwbkpt.value;
	uint32_t control = state->disabledHwbkpt.control;

	if (flags == SDBGP_HWBKPT_ENABLE_CLEAR) {
		state->disabledHwbkpt.value = 0;
		state->disabledHwbkpt.control = 0;
		state->disabledHwbkpt.where = SDBGP_HWBKPT_IN_BP0;
	}
	if (where == SDBGP_HWBKPT_IN_BP1) {
		writeDbgbvr(1, value);
		writeDbgbcr(1, control);
	} else if (where == SDBGP_HWBKPT_IN_BP0) {
		writeDbgbvr(0, value);
		writeDbgbcr(0, control);
	} else if (where == SDBGP_HWBKPT_IN_THREAD) {
		sceKernelSetTHBP(state->disabledHwbkpt.threadId, HWBKPT_CONTEXT, value, control);
	}
	return 0;
}

uint32_t hwbkptMakeControl(uint32_t control)
{
	return control | DBGBCR_ENABLE | DBGBCR_PMC_ANY;
}

/* The watch point of every core. */
int hwwtptWriteAllCpu(uint32_t value, uint32_t control)
{
	uint32_t key = DBG_LAR_KEY;
	int result;

	HWBKPT_WRITE(CORESIGHT_DBG_BASE(0) + DBG_LAR, key);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(1) + DBG_LAR, key);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(2) + DBG_LAR, key);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(3) + DBG_LAR, key);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(0) + DBG_WVR(0), value);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(0) + DBG_WCR(0), control);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(1) + DBG_WVR(0), value);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(1) + DBG_WCR(0), control);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(2) + DBG_WVR(0), value);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(2) + DBG_WCR(0), control);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(3) + DBG_WVR(0), value);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(3) + DBG_WCR(0), control);
	key = ~key;
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(0) + DBG_LAR, key);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(1) + DBG_LAR, key);
	HWBKPT_WRITE(CORESIGHT_DBG_BASE(2) + DBG_LAR, key);
	result = sceKernelPhysicalMemWriteForDebugger(CORESIGHT_DBG_BASE(3) + DBG_LAR, &key, 4, 4);
	if (result < 0)
		return result;
	return 0;
}

/* Whether the watch point stops the core at address. */
int hwwtptCheckStop(SceUID tid, uintptr_t address, int mode)
{
	SdbgpState *state;
	uint32_t value, control, mask, bytes;
	int bits, i;

	if (!(readDbgdscrExt() & 0x1c) || sceKernelGetPHWP(tid, 0, &value, &control) < 0)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	state = sdbgpGetState();
	if (!state->hwwatch[0].used)
		return SDBGP_STOP_HWWTPT_PASS;
	control = state->hwwatch[0].control;
	bits = (control >> 6) & 0x1f;
	mask = 0;
	for (i = 0; i < bits; i++)
		mask |= 1 << i;
	address &= ~mask;
	bytes = (control & 8) ? 3 : 0;
	if (control & 4)
		bytes |= 2;
	if (control & 2)
		bytes |= 1;
	if ((address & ~bytes) == state->hwwatch[0].value)
		return SDBGP_STOP_HWWTPT;
	return SDBGP_STOP_HWWTPT_PASS;
}

void hwwtptClear(int index)
{
	SdbgpState *state = sdbgpGetState();

	if (index == SDBGP_HWWATCH_MAX)
		SceDeci4pSDfMgrForKernel_8A3B06F2(state->hwwatch, 0, sizeof(state->hwwatch));
	else
		SceDeci4pSDfMgrForKernel_8A3B06F2(&state->hwwatch[index], 0, sizeof(state->hwwatch[0]));
}

int hwwtptIsSet(void)
{
	return sdbgpGetState()->hwwatch[0].used != 0;
}

int _getHwwtpt(SdbgpHwwtptGetReply *reply, uint32_t id)
{
	SdbgpState *state = sdbgpGetState();

	if (!state->hwwatch[0].used || (id != (uint32_t)-1 && state->hwwatch[0].owner != id))
		return 0;
	reply->info.size = sizeof(reply->info);
	reply->info.id = state->hwwatch[0].owner;
	reply->info.value = state->hwwatch[0].value;
	reply->info.control = state->hwwatch[0].control;
	reply->info.processId = state->hwwatch[0].processId;
	reply->info.threadId = state->hwwatch[0].threadId;
	return 0;
}

/* The host's watch point request as a DBGWCR. */
uint32_t hwwtptMakeControl(uint32_t control)
{
	uint32_t result = ((control >> 6) & 0x1f) << 24;

	if (control & 0x20)
		result |= 0x10;
	if (control & 0x10)
		result |= 0x08;
	if (!(control & 0xf))
		return (uint32_t)-2;
	return result | (control & 0xf) << 5 | DBGBCR_ENABLE | DBGBCR_PMC_ANY;
}

int hwwtptSetHwwtpt(const SdbgpHwwtptSetRequest *request, SdbgpHwwtptSetReply *reply)
{
	SdbgpState *state = sdbgpGetState();
	SceUID pid = request->processId;
	SceUID tid = request->threadId;
	uint32_t value = request->value;
	uint32_t control = request->control;
	int result;

	if (state->hwwatch[0].used)
		return 0;
	reply->id = SDBGP_HWWTPT_ID;
	state->hwwatch[0].threadId = tid;
	state->hwwatch[0].owner = SDBGP_HWWTPT_ID;
	state->hwwatch[0].value = value;
	state->hwwatch[0].control = control;
	state->hwwatch[0].processId = pid;
	state->hwwatch[0].used = 1;
	result = hwwtptWriteAllCpu(value, hwwtptMakeControl(control));
	if (result < 0)
		return result;
	return 0;
}

int clearHwwtpt(uint32_t id)
{
	SdbgpState *state = sdbgpGetState();
	int result;

	if (!state->hwwatch[0].used)
		return 0;
	if (id != (uint32_t)-1 && state->hwwatch[0].owner != id)
		return 0;
	result = hwwtptWriteAllCpu(0, 0);
	if (result < 0)
		return result;
	hwwtptClear(0);
	return 0;
}

/* Take out the watch point the core stopped at, to step over the access. */
int hwwtptDisableHWWP(void)
{
	SdbgpState *state = sdbgpGetState();
	uint32_t dfar = readDfar();
	uint32_t value, control;

	if (state->hwwatch[0].used) {
		control = hwwtptMakeControl(state->hwwatch[0].control);
		value = state->hwwatch[0].value;
	} else {
		value = 0;
		control = 0;
	}
	state->disabledHwwtpt.value = value;
	state->disabledHwwtpt.control = control;
	state->disabledHwwtpt.disabled = 1;
	if (dfar != value)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	writeDbgwcr(0, control & ~DBGBCR_ENABLE);
	return 0;
}

int hwwtptEnableHWWP(void)
{
	SdbgpState *state = sdbgpGetState();
	uint32_t value, control;

	if (state->disabledHwwtpt.disabled) {
		value = state->disabledHwwtpt.value;
		control = state->disabledHwwtpt.control;
	} else if (state->hwwatch[0].used) {
		control = hwwtptMakeControl(state->hwwatch[0].control);
		value = state->hwwatch[0].value;
	} else {
		value = 0;
		control = 0;
	}
	writeDbgwvr(0, value);
	writeDbgwcr(0, control);
	state->disabledHwwtpt.disabled = 0;
	return 0;
}

/* The watch point stays off while the debugger works on a packet. */
void hwwtptDisable(void)
{
	SdbgpState *state = sdbgpGetState();

	if (state->hwwatch[0].used)
		writeDbgwcr(0, hwwtptMakeControl(state->hwwatch[0].control) & ~DBGBCR_ENABLE);
}

void hwwtptEnable(SdbgpCpuState *cpu)
{
	SdbgpState *state = sdbgpGetState();

	if (cpu->hostMode & SDBGP_HOST_HWWTPT_OFF)
		cpu->hostMode &= ~SDBGP_HOST_HWWTPT_OFF;
	else if (state->hwwatch[0].used)
		writeDbgwcr(0, hwwtptMakeControl(state->hwwatch[0].control) | DBGBCR_ENABLE);
}
