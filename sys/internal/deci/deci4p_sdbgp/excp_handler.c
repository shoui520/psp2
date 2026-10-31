/*
 * SceDeci4pSDbgp exception handlers (excp_handler.c).
 */

#include <scetypes.h>
#include <excpmgr/excpmgr.h>
#include <intrmgr/intrmgr.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/cp15.h>
#include <sysmem/cpu.h>
#include <sysmem/dipsw.h>
#include "sdbgp_internal.h"

void _excpGetStopReason(int cause, uint32_t *stopReason)
{
	switch (cause) {
	case 1:
	case 64:
		*stopReason = 0x30003;
		break;
	case 2:
	case 65:
		*stopReason = 0x30004;
		break;
	case 3:
	case 66:
		*stopReason = 0x30002;
		break;
	case 16:
	case 17:
	case 18:
	case 19:
		*stopReason = 0x20004;
		break;
	case 32:
	case 33:
	case 34:
		*stopReason = 0x20003;
		break;
	case 48:
	case 50:
		*stopReason = 0x20001;
		break;
	default:
		break;
	}
}

int irqHandler(void)
{
	SceDeci4pSDfMgrForKernel_EDFBA012();
	return 0;
}

void _getExcpInfo(uint32_t cpsr, SdbgpExcpInfo *info)
{
	ThreadCBInfo thread;
	int ret;

	ret = sceKernelIsIntrContext();
	if (ret > 0) {
		info->processId = SDBGP_KERNEL_PROCESS_ID;
		info->threadId = 0;
		info->addressSpace = 1;
	} else if (ret == 0) {
		sceKernelGetThreadContextInfo(&thread);
		info->processId = thread.processId;
		if (thread.processId == SDBGP_KERNEL_PROCESS_ID ||
		    (cpsr & PSR_MODE_MASK) != PSR_MODE_SYS)
			info->addressSpace = 16;
		else
			info->addressSpace = 32;
		info->threadId = thread.threadId;
	}
}

void _saveExcpContext(SdbgpState *state, SceExcpmgrContext *ctx, SceUID pid,
		      SceUID tid, uintptr_t pc, uint32_t stopReason)
{
	SdfMgrDebugCpuState *dbg;
	const uint32_t (*vfp)[4];
	uint32_t (*dbgVfp)[4];
	unsigned int cpu;
	int i, j;

	cpu = sceKernelCpuId();
	if (cpu > 3)
		return;
	dbg = &state->debugCpu[cpu];
	dbg->state = 1;
	state->stoppedTid[cpu] = tid;
	state->stopReason[cpu] = stopReason;
	dbg->pid = pid;
	dbg->tid = tid;
	dbg->gpr[0] = ctx->r[0];
	dbg->gpr[1] = ctx->r[1];
	dbg->gpr[2] = ctx->r[2];
	dbg->gpr[3] = ctx->r[3];
	dbg->gpr[4] = ctx->r[4];
	dbg->gpr[5] = ctx->r[5];
	dbg->gpr[6] = ctx->r[6];
	dbg->gpr[7] = ctx->r[7];
	dbg->gpr[8] = ctx->r[8];
	dbg->gpr[9] = ctx->r[9];
	dbg->gpr[10] = ctx->r[10];
	dbg->gpr[11] = ctx->r[11];
	dbg->gpr[12] = ctx->r[12];
	dbg->gpr[13] = ctx->sp;
	dbg->gpr[14] = ctx->lr;
	dbg->gpr[15] = ctx->pc;
	dbg->pc = pc;
	dbg->cpsr = ctx->cpsr;
	vfp = (const uint32_t (*)[4])ctx->vfp;
	dbgVfp = (uint32_t (*)[4])dbg->vfp;
	for (i = 0; i < 16; i++) {
		for (j = 0; j < 4; j++)
			dbgVfp[i][j] = vfp[i][j];
	}
	state->debugCpu[cpu].fpscr = ctx->fpscr;
	state->debugCpu[cpu].fpexc = ctx->fpexc;
	state->debugCpu[cpu].ifsr = readIfsr();
	state->debugCpu[cpu].ifar = readIfar();
	state->debugCpu[cpu].aifsr = readAifsr();
	state->debugCpu[cpu].dfsr = readDfsr();
	state->debugCpu[cpu].dfar = readDfar();
	state->debugCpu[cpu].adfsr = readAdfsr();
	state->debugCpu[cpu].cpacr = readCpacr();
	state->debugCpu[cpu].dacr = readDacr();
	state->debugCpu[cpu].ttbr1 = readTtbr1();
	state->debugCpu[cpu].tpidrurw = readTpidrurw();
	dbg = &state->debugCpu[cpu];
	dbg->tpidruro = ctx->tpidruro;
	dbg->tpidrprw = (uint32_t)ctx->thread;
	state->debugCpu[cpu].contextId = readContextId();
	state->debugCpu[cpu].pmselr = readPmselr();
	state->debugCpu[cpu].pmcntenset = readPmcntenset();
	dbg = &state->debugCpu[cpu];
	dbg->context = ctx;
	dbg->state = 2;
}

int dataAbortHandler(SceExcpmgrContext *ctx, uintptr_t pc, uint32_t cpsr)
{
	SdbgpCpuState *cpu;
	SdbgpState *state;
	SdbgpExcpDetail detail;
	SdbgpExcpInfo info;
	int mode;
	int cause;

	cpu = sdbgpGetCpuState();
	state = sdbgpGetState();
	if (state->enabled == 0x8000) {
		mode = 1;
	} else {
		if (!state->enabled)
			return 3;
		if ((cpsr & PSR_MODE_MASK) == PSR_MODE_USR)
			return 2;
		mode = 0;
	}
	SceDeci4pSDfMgrForKernel_8A3B06F2(&info, 0, sizeof(info));
	info.context = ctx;
	info.pc = pc;
	info.cpsr = cpsr;
	info.stopReason = 0x30004;
	_getExcpInfo(cpsr, &info);
	cause = checkSdbgpTargetAddrStopCauseDabt(readDfar(), info, mode);
	if (cause < 0)
		return 3;
	_excpGetStopReason(cause, &info.stopReason);
	if (cause == SDBGP_STOP_DABT)
		sceKernelInitialHandlerDebugDabtCfunc(ctx);
	_saveExcpContext(state, info.context, info.processId, info.threadId,
			 info.pc, info.stopReason);
	cpu->stoppedTid = info.threadId;
	SceDeci4pSDfMgrForKernel_EDFBA012();
	if (excpGetExcpStopReason(cpu, &detail, info.stopReason) < 0)
		return 3;
	if (!cpu->hostMode)
		return 0;
	if (sdbgpSendNotification(cpu, info.processId, info.threadId, info.pc,
				  info.cpsr, &detail) < 0)
		return 3;
	return 0;
}

int prefetchAbortHandler(SceExcpmgrContext *ctx, uintptr_t pc, uint32_t cpsr)
{
	SdbgpCpuState *cpu;
	SdbgpState *state;
	SdbgpExcpDetail detail;
	SdbgpExcpInfo info;
	int isCall;
	int mode;
	int cause;
	int index;
	uint32_t status100;
	uint32_t status400;
	uint32_t status800;

	cpu = sdbgpGetCpuState();
	state = sdbgpGetState();
	writeDbgdscrExt(readDbgdscrExt() & ~0x800u);
	if (state->enabled == 0x8000) {
		mode = 1;
	} else {
		if (!state->enabled)
			return 3;
		if ((cpsr & PSR_MODE_MASK) == PSR_MODE_USR)
			return 2;
		mode = 0;
	}
	SceDeci4pSDfMgrForKernel_8A3B06F2(&info, 0, sizeof(info));
	info.context = ctx;
	info.pc = pc;
	info.cpsr = cpsr;
	info.stopReason = 0x30003;
	if (sceKernelCheckDipsw(SDBGP_DIPSW_HW_DEBUG)) {
		writeDbgclaimclr(1);
		if ((readDbgdscrExt() & 0x34) != 4)
			sceKernelInitialHandlerDebugPabtCfunc(ctx);
	} else {
		sceKernelInitialHandlerDebugPabtCfunc(ctx);
	}
	_getExcpInfo(cpsr, &info);
	cause = checkSdbgpTargetAddrStopCausePabt(info, mode);
	if (cause < 0)
		return 3;
	_excpGetStopReason(cause, &info.stopReason);
	if (cause == SDBGP_STOP_SWBKPT &&
	    swbkptGetImmediate(pc, cpsr) == 0x85) {
		ctx->pc = (cpsr & PSR_THUMB) ? pc + 2 : pc + 4;
	}
	_saveExcpContext(state, info.context, info.processId, info.threadId,
			 info.pc, info.stopReason);
	cpu->stoppedTid = info.threadId;
	if (sdbgpGetStatus(cpu, 0x400)) {
		SceDeci4pSDfMgrForKernel_682D0EFB();
	} else if (!sdbgpGetStatus(cpu, 0x100)) {
		SceDeci4pSDfMgrForKernel_EDFBA012();
	}
	excpGetExcpStopReason(cpu, &detail, info.stopReason);
	status100 = sdbgpGetStatus(cpu, 0x100);
	if (status100) {
		sdbgpUpdateStatus(cpu, 0x100, 0);
		if (sdbgpGetStatus(cpu, 0x1000)) {
			hwbkptEnableHWBP(SDBGP_HWBKPT_ENABLE_CLEAR);
			sdbgpUpdateStatus(cpu, 0x1000, 0);
		}
		if (sdbgpGetStatus(cpu, 0x2000)) {
			hwwtptEnableHWWP();
			sdbgpUpdateStatus(cpu, 0x2000, 0);
		}
		if (sdbgpGetStatus(cpu, 0x4000)) {
			swbkptEnableSWBP();
			sdbgpUpdateStatus(cpu, 0x4000, 0);
		}
		if (sdbgpGetStatus(cpu, 0x8000)) {
			swbkptEnableSysSWBP();
			sdbgpUpdateStatus(cpu, 0x8000, 0);
		}
		state->stoppedTid[0] = 0;
		state->stoppedTid[1] = 0;
		state->stoppedTid[2] = 0;
		state->stoppedTid[3] = 0;
		cpu->stoppedTid = 0;
		if (sdbgpGetFlags(state, 0x10))
			swbkptWriteBkpt(&state->stepSwbkpt);
		SceDeci4pSDfMgrForKernel_9A653F19();
		return 0;
	}
	if (sdbgpGetFlags(state, 0x10)) {
		status400 = sdbgpGetStatus(cpu, 0x400);
		if (status400) {
			sdbgpUpdateStatus(cpu, 0x400, status100);
			if (sdbgpGetStatus(cpu, 0x4000)) {
				swbkptEnableSWBP();
				sdbgpUpdateStatus(cpu, 0x4000, status100);
			}
			state->stoppedTid[0] = 0;
			state->stoppedTid[1] = 0;
			state->stoppedTid[2] = 0;
			state->stoppedTid[3] = 0;
			cpu->stoppedTid = 0;
			SceDeci4pSDfMgrForKernel_9A653F19();
			return 0;
		}
		if (info.pc == state->stepSwbkpt.address) {
			sdbgpUpdateFlags(state, 0x10, 0);
			swbkptRemove(&state->stepSwbkpt);
			goto notify;
		}
		index = swbkptFindByAddress(info.processId, info.pc);
		if (index >= 0 && swbkptDisableSWBP(state->swbkpt[index]) >= 0) {
			sdbgpUpdateStatus(cpu, 0x4000, 1);
			sdbgpUpdateStatus(cpu, 0x400, 1);
			SceDeci4pSDfMgrForKernel_4EA65698(1);
			SceDeci4pSDfMgrForKernel_58D7597A();
			return status400;
		}
		goto notify;
	}
	if (sdbgpGetStatus(cpu, 0x400)) {
		status800 = sdbgpGetStatus(cpu, 0x800);
		if (status800) {
			sdbgpUpdateStatus(cpu, 0x800, 0);
			status800 = 1;
		}
		sdbgpUpdateStatus(cpu, 0x400, 0);
		if (sdbgpGetStatus(cpu, 0x1000)) {
			hwbkptEnableHWBP(0x400);
			sdbgpUpdateStatus(cpu, 0x1000, 0);
		}
		if (sdbgpGetStatus(cpu, 0x2000))
			hwwtptEnableHWWP();
		if (sdbgpGetStatus(cpu, 0x4000)) {
			swbkptEnableSWBP();
			sdbgpUpdateStatus(cpu, 0x4000, 0);
		}
		if (sdbgpGetStatus(cpu, 0x8000)) {
			swbkptEnableSysSWBP();
			sdbgpUpdateStatus(cpu, 0x8000, 0);
		}
		if (--state->stepCount != 0) {
			isCall = 0;
			if (status800) {
				sdbgpUpdateStatus(cpu, 0xc00, 1);
				if (sdbgpCheckStepOverCall(state, info.processId,
							   info.pc, info.cpsr,
							   &isCall) < 0)
					goto notify;
			} else {
				sdbgpUpdateStatus(cpu, 0x400, 1);
			}
			sdbgpUpdateStatus(cpu, SDBGP_STATUS_STOPPED, 1);
			if (isCall)
				SceDeci4pSDfMgrForKernel_4EA65698(2);
			else
				SceDeci4pSDfMgrForKernel_4EA65698(1);
			sdbgpUpdateStatus(cpu, SDBGP_STATUS_STOPPED, 0);
		}
	}
	if (cause == SDBGP_STOP_BKPT_E0) {
		index = swbkptFindByAddress(info.processId, info.pc);
		if (index >= 0) {
			state->swbkpt[index].reserved++;
			if (state->swbkpt[index].reserved == state->swbkpt[index].attr) {
				state->swbkpt[index].reserved = 0;
			} else {
				if (swbkptDisableSWBP(state->swbkpt[index]) < 0)
					return SDBGP_ERROR_INVALID_ARGUMENT;
				sdbgpUpdateStatus(cpu, 0x4100, 1);
				sdbgpUpdateStatus(cpu, SDBGP_STATUS_STOPPED, 1);
				SceDeci4pSDfMgrForKernel_4EA65698(1);
				sdbgpUpdateStatus(cpu, SDBGP_STATUS_STOPPED, 0);
			}
		}
	}
notify:
	if (!cpu->hostMode)
		return 0;
	if (sdbgpSendNotification(cpu, info.processId, info.threadId, info.pc,
				  info.cpsr, &detail) < 0)
		return 3;
	return 0;
}

int undefinedHandler(SceExcpmgrContext *ctx, uintptr_t pc, uint32_t cpsr)
{
	SdbgpCpuState *cpu;
	SdbgpState *state;
	SdbgpExcpDetail detail;
	SdbgpExcpInfo info;

	cpu = sdbgpGetCpuState();
	state = sdbgpGetState();
	if (state->enabled != 0x8000) {
		if (!state->enabled)
			return 3;
		if ((cpsr & PSR_MODE_MASK) == PSR_MODE_USR)
			return 2;
	}
	SceDeci4pSDfMgrForKernel_8A3B06F2(&info, 0, sizeof(info));
	info.context = ctx;
	info.pc = pc;
	info.cpsr = cpsr;
	_getExcpInfo(cpsr, &info);
	sceKernelInitialHandlerDebugUndefCfunc(ctx);
	info.stopReason = 0x30002;
	_saveExcpContext(state, info.context, info.processId, info.threadId,
			 info.pc, info.stopReason);
	cpu->stoppedTid = info.threadId;
	SceDeci4pSDfMgrForKernel_EDFBA012();
	if (excpGetExcpStopReason(cpu, &detail, info.stopReason) < 0)
		return 3;
	if (!cpu->hostMode)
		return 0;
	if (sdbgpSendNotification(cpu, info.processId, info.threadId, info.pc,
				  info.cpsr, &detail) < 0)
		return 3;
	return 0;
}

int excpHandlerInit(SdbgpCpuState *cpu)
{
	int ret;

	ret = SceDeci4pSDfMgrForKernel_6BF5553C(prefetchAbortHandler, 0);
	if (ret != 0)
		return ret;
	ret = SceDeci4pSDfMgrForKernel_6BF5553C(dataAbortHandler, 1);
	if (ret != 0)
		return ret;
	ret = SceDeci4pSDfMgrForKernel_6BF5553C(undefinedHandler, 2);
	if (ret != 0)
		return ret;
	return SceDeci4pSDfMgrForKernel_6BF5553C(irqHandler, 3);
}
