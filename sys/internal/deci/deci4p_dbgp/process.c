/* Debugger process control (Sony's process.c). */

#include <sceerror.h>
#include <kernel/error.h>
#include <kernel/threadmgr.h>
#include <acmgr/acmgr.h>
#include <modulemgr/modulemgr.h>
#include <modulemgr/modulemgr_control.h>
#include <processmgr/process.h>
#include <processmgr/processmgr.h>
#include <regmgr/regmgr.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <sysmem/uid_guid.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/threadmgr_debugger.h>
#include "dbgp_internal.h"

/* the stop notification: DBGP group 0x10, type 0xc0 */
#define DBGP_STOP_GROUP		0x10
#define DBGP_STOP_TYPE		0xc0

/* why the debugger stopped a process or thread (DbgpStopReason.reason) */
#define DBGP_STOP_NONE			0
#define DBGP_STOP_STATUS_17		0x10001
#define DBGP_STOP_THREAD_SUSPENDED	0x10002
#define DBGP_STOP_STATUS_3		0x10003
#define DBGP_STOP_REQUEST_8000		0x10004
#define DBGP_STOP_BREAK			0x10005	/* stopped on proc->currentThread */
#define DBGP_STOP_STACK_OVERFLOW	0x10006
#define DBGP_STOP_ILLEGAL_CONTEXT	0x10007
#define DBGP_STOP_CRITICAL_USAGE	0x10008
#define DBGP_STOP_ILLEGAL_NUMBER	0x10009
#define DBGP_STOP_HWWTPT		0x20001
#define DBGP_STOP_SWBKPT		0x20004
#define DBGP_STOP_STEP			0x20007
#define DBGP_STOP_EXCEPTION_21		0x30002
#define DBGP_STOP_EXCEPTION		0x30003
#define DBGP_STOP_EXCEPTION_20		0x30004
#define DBGP_STOP_VFP			0x40001
#define DBGP_STOP_GPU_EXCEPTION		0x50001
#define DBGP_STOP_DECLARE_SUICIDE	0x80000	/* + the code the thread declared */

/* the stop reasons excpInfoSave records (intr_handler.c, excp_handler.c) */
#define STOP_REASON_VFP			0x1000
#define STOP_REASON_PRX			0x2000
#define STOP_REASON_BREAK		0x4000
#define STOP_REASON_8000		0x8000
#define STOP_REASON_STACK_OVERFLOW	0x10000
#define STOP_REASON_ILLEGAL_CONTEXT	0x20000
#define STOP_REASON_CRITICAL_USAGE	0x40000
#define STOP_REASON_GPU_EXCEPTION	0x80000
#define STOP_REASON_ILLEGAL_NUMBER	0x100000
#define STOP_REASON_DECLARE_SUICIDE	0x200000

/* sceKernelIsThreadDebugSuspended's flags beside the suspend state (bits 0, 1) */
#define THREAD_DEBUG_SUSPENDED		0x1

int procSuspendProcess(DbgpProcCB *proc, SceUID pid, SceUID thid,
		       SceUInt32 reason)
{
	SceUInt32 status;
	int newStatus, threadStatus, result;

	(void)proc;
	if (sceKernelGetProcessStatus(pid, &status) < 0)
		status = 0;
	else
		status = (SceUInt8)status;
	switch (reason & 0xf0) {
	case 0:
		switch (reason) {
		case 1:
			result = SceThreadmgrForDriver_04C6764B(thid, 0x800);
			if (result < 0) {
				SCE_PRINTF_WITH_INFO(15, 63, "0x%08x\n", result);
				return result;
			}
			newStatus = 19;
			break;
		case 2:
			newStatus = 20;
			break;
		case 3:
			newStatus = 21;
			break;
		default:
			return reason;
		}
		if (status == PROCESS_STATE_RUNNING)
			goto debug_suspend;
		result = sceKernelSetProcessStatus(pid, newStatus);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 87, "0x%08x\n", result);
			return result;
		}
		goto update_status;
	case 0x20:
		if (status == PROCESS_STATE_RUNNING) {
			if (reason - 0x28 <= 1) {
				threadStatus = 0x4000;
				newStatus = 28;
			} else {
				threadStatus = 0x1000;
				newStatus = 18;
			}
		} else {
			newStatus = 0;
			threadStatus = reason - 0x28 <= 1 ? 0x4002 : 0x1002;
		}
		result = SceThreadmgrForDriver_04C6764B(thid, threadStatus);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 129, "0x%08x:0x%08x\n",
					     result, threadStatus);
			return result;
		}
		if (status == PROCESS_STATE_RUNNING)
			goto debug_suspend;
		if (newStatus == 0)
			return 0;
		goto set_status;
	case 0x30:
		switch (reason) {
		case 0x30: newStatus = 23; break;
		case 0x31: newStatus = 24; break;
		case 0x32: newStatus = 25; break;
		case 0x33: newStatus = 26; break;
		case 0x34: newStatus = 27; break;
		case 0x35: newStatus = 29; break;
		case 0x36: newStatus = 31; break;
		default: return reason;
		}
		goto update_status;
	case 0x40:
		result = SceThreadmgrForDriver_04C6764B(thid, 0x1002);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 145, "0x%08x\n", result);
			return result;
		}
		if (status != PROCESS_STATE_RUNNING)
			return 0;
		newStatus = 0;
		goto debug_suspend;
	case 0x50:
		threadStatus = status == PROCESS_STATE_RUNNING ? 0x2000 : 0x2002;
		result = SceThreadmgrForDriver_04C6764B(thid, threadStatus);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 168, "0x%08x:0x%08x\n",
					     result, threadStatus);
			return result;
		}
		newStatus = 22;
		goto update_status;
	default:
		return (int)0x80080001;
	}
update_status:
	if (status == PROCESS_STATE_RUNNING)
		goto debug_suspend;
set_status:
	result = sceKernelSetProcessStatus(pid, newStatus);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 229, "0x%08x:0x%08x\n", result, newStatus);
		return result;
	}
	return 0;
debug_suspend:
	result = sceKernelProcessDebugSuspend(pid, newStatus);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 222, "0x%08x:0x%08x\n", result, newStatus);
		return result;
	}
	return 0;
}

int procGetProcInfoForPacket(DbgpProcCB *proc, SceUID pid)
{
	SceKernelProcessInfo info;
	char path[256];
	int result, length, padding;

	(void)proc;
	info.size = sizeof(info);
	result = sceKernelGetProcessInfo(pid, &info);
	if (result != 0) {
		SCE_PRINTF_WITH_INFO(15, 250, "0x%08x\n", result);
		return result;
	}
	result = SceModulemgrForKernel_79E761B5(info.mainModule, path,
						sizeof(path));
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 258, "0x%08x\n", result);
		return result;
	}
	length = strnlen(path, sizeof(path));
	padding = (length + 1) & 3;
	if (padding != 0)
		padding = 4 - padding;
	return sizeof(DbgpProcessInfo) + length + 1 + padding +
	       sizeof(DbgpProcessUnwind);
}

void procGetProcessDbgpStopReason(DbgpProcCB *proc, DbgpStopReason *stop,
				  SceUID pid, SceUInt32 reason, SceUInt32 pc)
{
	ThreadmgrDebugRegisters regs[2];
	SceUInt32 status;
	int result, id;

	(void)pc;
	stop->reserved[0] = 0;
	stop->reserved[1] = 0;
	stop->size = sizeof(*stop);
	stop->reason = DBGP_STOP_NONE;
	stop->data = 0;
	result = sceKernelGetProcessStatus(pid, &status);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 299, "0x%08x\n", result);
		return;
	}
	if (reason == STOP_REASON_PRX) {
		SceModuleCB *module;

		stop->reason = proc->prxStopReason;
		result = SceModulemgrForKernel_37512E29(proc->prxModid, &module);
		if (result >= 0)
			stop->data = module->puid;
		proc->prxStopReason = -1;
	} else if (reason == STOP_REASON_BREAK) {
		stop->reason = DBGP_STOP_BREAK;
	} else if (reason == STOP_REASON_8000) {
		stop->reason = DBGP_STOP_REQUEST_8000;
	} else if (reason == STOP_REASON_VFP) {
		stop->reason = DBGP_STOP_VFP;
	} else if (reason == STOP_REASON_STACK_OVERFLOW) {
		stop->reason = DBGP_STOP_STACK_OVERFLOW;
	} else if (reason == STOP_REASON_ILLEGAL_CONTEXT) {
		stop->reason = DBGP_STOP_ILLEGAL_CONTEXT;
	} else if (reason == STOP_REASON_CRITICAL_USAGE) {
		stop->reason = DBGP_STOP_CRITICAL_USAGE;
	} else if (reason == STOP_REASON_GPU_EXCEPTION) {
		stop->reason = DBGP_STOP_GPU_EXCEPTION;
	} else if (reason == STOP_REASON_ILLEGAL_NUMBER) {
		stop->reason = DBGP_STOP_ILLEGAL_NUMBER;
	} else if ((reason & STOP_REASON_DECLARE_SUICIDE) != 0) {
		stop->reason = (SceUInt8)reason + DBGP_STOP_DECLARE_SUICIDE;
	} else {
		DbgpThreadIdentity thread;

		status = (SceUInt8)status;
		switch (status) {
		case 1:
		case 2:
			stop->reason = DBGP_STOP_NONE;
			break;
		case 3:
			stop->reason = DBGP_STOP_STATUS_3;
			break;
		case 4:
			break;
		case 17:
			stop->reason = DBGP_STOP_STATUS_17;
			break;
		case 18:
			stop->reason = DBGP_STOP_SWBKPT;
			break;
		case 19:
			stop->reason = DBGP_STOP_EXCEPTION;
			break;
		case 20:
			stop->reason = DBGP_STOP_EXCEPTION_20;
			break;
		case 21:
			stop->reason = DBGP_STOP_EXCEPTION_21;
			break;
		case 22:
			stop->reason = DBGP_STOP_HWWTPT;
			break;
		case 28:
			stop->reason = DBGP_STOP_STEP;
			break;
		}
		if (status != 18 && status != 22)
			return;
		SceThreadmgrForDriver_5022689D(proc->stoppedThread, regs);
		thread.pid = pid;
		thread.thid = proc->stoppedThread;
		if (status == 18) {
			if (dbgpTestProcStatus(proc, 0x20000) != 0) {
				dbgpSetProcStatus(proc, 0x20000, 0);
			} else {
				id = swbkptGetAlignId(proc, (void *)regs[0].pc,
						      thread, NULL);
				if (id >= 0) {
					stop->data = proc->swbkpt[id].id;
					return;
				}
			}
			stop->reason = DBGP_STOP_EXCEPTION;
		} else if (status == 22) {
			id = hwwtptGetHitId(proc, thread);
			if (id >= 0)
				stop->data = proc->hwwtpt[id].id;
		}
	}
}

int procGetThreadDbgpStopReason(DbgpProcCB *proc, DbgpStopReason *stop,
				SceUID thid, SceUID pid, SceUInt32 reason)
{
	ThreadmgrDebugRegisters regs[2];
	DbgpThreadIdentity thread;
	SceUInt32 pc, data;
	SceUID stopped;
	int status, id;

	stop->data = 0;
	stop->reserved[0] = 0;
	stop->size = sizeof(*stop);
	stop->reason = DBGP_STOP_NONE;
	stop->reserved[1] = 0;
	status = sceKernelIsThreadDebugSuspended(thid);
	if (status < 0) {
		SCE_PRINTF_WITH_INFO(15, 497, "0x%08x\n", status);
		return status;
	}
	if (reason == STOP_REASON_BREAK) {
		stop->reason = DBGP_STOP_BREAK;
		return 0;
	}
	if ((status & THREAD_DEBUG_SUSPENDED) != 0) {
		stop->reason = DBGP_STOP_THREAD_SUSPENDED;
		return 0;
	}
	switch (status & ~3) {
	case 0:
		stop->reason = DBGP_STOP_NONE;
		break;
	case 0x10:
		stop->reason = DBGP_STOP_VFP;
		break;
	case 0x100:
	case 0x800:
		stop->reason = DBGP_STOP_EXCEPTION;
		break;
	case 0x200:
		stop->reason = DBGP_STOP_EXCEPTION_20;
		break;
	case 0x400:
		stop->reason = DBGP_STOP_EXCEPTION_21;
		break;
	case 0x1000:
		stop->reason = DBGP_STOP_SWBKPT;
		break;
	case 0x2000:
		stop->reason = DBGP_STOP_HWWTPT;
		break;
	case 0x4000:
		stop->reason = DBGP_STOP_STEP;
		break;
	case 0x10000:
		stop->reason = DBGP_STOP_STACK_OVERFLOW;
		break;
	case 0x20000:
		stop->reason = DBGP_STOP_ILLEGAL_CONTEXT;
		break;
	case 0x40000:
		stop->reason = DBGP_STOP_CRITICAL_USAGE;
		break;
	case 0x80000:
		stop->reason = DBGP_STOP_ILLEGAL_NUMBER;
		break;
	case 0x100000:
		stop->reason = (SceUInt8)reason + DBGP_STOP_DECLARE_SUICIDE;
		break;
	}
	if (stop->reason == DBGP_STOP_STEP) {
		if (proc->stoppedThread != thid)
			stop->reason = DBGP_STOP_NONE;
		return 0;
	}
	if (stop->reason != DBGP_STOP_SWBKPT && stop->reason != DBGP_STOP_HWWTPT)
		return 0;
	SceThreadmgrForDriver_5022689D(thid, regs);
	pc = regs[0].pc;
	thread.pid = pid;
	thread.thid = thid;
	if (stop->reason == DBGP_STOP_SWBKPT) {
		id = swbkptGetAlignId(proc, (void *)pc, thread, NULL);
		if (id < 0) {
			stop->reason = DBGP_STOP_EXCEPTION;
			return 0;
		}
		stopped = proc->stoppedThread;
		data = proc->swbkpt[id].id;
		if (stopped != thid)
			stop->reason = DBGP_STOP_NONE;
		stop->data = data;
		if (stopped == thid && stop->reason == DBGP_STOP_SWBKPT &&
		    data == 0)
			stop->reason = DBGP_STOP_EXCEPTION;
	} else if (stop->reason == DBGP_STOP_HWWTPT) {
		if (proc->stoppedThread != thid)
			stop->reason = DBGP_STOP_NONE;
		id = hwwtptGetHitId(proc, thread);
		if (id >= 0)
			stop->data = proc->hwwtpt[id].id;
	}
	return 0;
}

int procGetProcessAttribute(DbgpProcCB *proc, SceUID pid, SceUInt32 *attr)
{
	DbgpProcCB *target;
	SceUInt32 status;
	int result;

	target = dbgpGetProcCB(pid);
	if (target != NULL)
		proc = target;
	result = sceKernelGetProcessStatus(pid, &status);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 673, "0x%08x\n", result);
		return result;
	}
	status = (SceUInt8)status;
	if (threadAppObjectIsSupported(pid) == 0)
		*attr |= DBGP_PROCESS_ATTR_ULT_INFO;
	if (proc->pid == pid)
		*attr |= DBGP_PROCESS_ATTR_ATTACHED;
	if (SceSblACMgrForDriver_4CBD6156(pid) != 0)
		*attr |= DBGP_PROCESS_ATTR_DEBUGGABLE;
	if (status == 3)
		*attr |= DBGP_PROCESS_ATTR_STOPPED;
	if (sceSblACMgrIsSystemProgram(pid) != 0)
		*attr |= DBGP_PROCESS_ATTR_SYSTEM;
	if (status == 2)
		*attr |= DBGP_PROCESS_ATTR_SUSPENDED;
	*attr |= DBGP_PROCESS_ATTR_VALID;
	return 0;
}

int procGetPrxDebugFlag(SceUID pid, SceUInt32 *flag)
{
	DbgpCB *dbgp;
	DbgpProcCB *proc;
	int value;

	dbgp = dbgpGetDbgpCB();
	if (dbgp == NULL)
		return (int)0x80080001;
	if (dbgp->spawn.pid == pid) {
		*flag = dbgp->spawn.prxDebugFlag;
	} else {
		proc = dbgpGetProcCB(pid);
		if (proc == NULL)
			return (int)0x80080001;
		*flag = proc->prxDebugFlag;
	}
	if (sceRegMgrGetKeyInt("/DEVENV/TOOL", "prx_debug", &value) >= 0 &&
	    value == 0)
		*flag &= ~1;
	return 0;
}

int procDebugSuspendProcess(SceUID pid)
{
	SceUInt32 status;
	int result;

	result = sceKernelGetProcessStatus(pid, &status);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 782, "0x%08x\n", result);
		return result;
	}
	status = (SceUInt8)status;
	if ((status & 0x10) != 0)
		return 0;
	result = sceKernelProcessDebugSuspend(pid, 17);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 796, "0x%08x\n", result);
		return result;
	}
	return 0;
}

int procGetProcessInfo(DbgpProcCB *proc, void *info, SceUID pid,
		       SceUInt32 reason, SceUInt32 pc)
{
	union {
		SceKernelProcessInfo process;
		SceKernelModuleInfo module;
	} buf;
	DbgpProcessInfo *out = info;
	DbgpProcessUnwind *unwind;
	int result, length, padding;

	buf.process.size = sizeof(buf.process);
	result = sceKernelGetProcessInfo(pid, &buf.process);
	if (result != 0) {
		SCE_PRINTF_WITH_INFO(15, 947, "0x%08x\n", result);
		return result;
	}
	result = SceModulemgrForKernel_79E761B5(buf.process.mainModule,
						out->path, 256);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 959, "0x%08x\n", result);
		return (int)0x80010002;
	}
	length = strnlen(out->path, 256);
	padding = (length + 1) & 3;
	if (padding != 0)
		padding = 4 - padding;
	out->size = sizeof(*out) + length + 1 + padding + sizeof(*unwind);
	out->pid = pid;
	out->param = buf.process.param;
	out->cpuAffinityMask = buf.process.cpuAffinity;
	out->entryPoint = buf.process.entryPoint;
	SceModulemgrForKernel_337A3908(buf.process.mainModule, &out->fingerprint);
	out->pathLength = length + 1;
	out->parentPid = buf.process.parentPid;
	if (buf.process.parentPid == SCE_KERNEL_PROCESS_ID)
		out->parentPid = -1;
	result = procGetProcessAttribute(proc, pid, &out->attr);
	if (result != 0)
		SCE_PRINTF_WITH_INFO(15, 985, "0x%08x\n", result);
	*(DbgpName *)out->name = *(DbgpName *)buf.process.name;
	procGetProcessDbgpStopReason(proc, &out->stopReason, pid, reason, pc);
	unwind = (DbgpProcessUnwind *)(out->path + length + 1);
	if (padding != 0) {
		memset(out->path + length + 1, 0, padding);
		unwind = (DbgpProcessUnwind *)((char *)unwind + padding);
	}
	result = sceKernelGetModuleInfo(pid, buf.process.mainModule,
					&buf.module);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1006, "0x%08x\n", result);
		return (int)0x80010002;
	}
	unwind->exidxTop = buf.module.exidx_top;
	unwind->exidxBtm = buf.module.exidx_btm;
	unwind->extabTop = buf.module.extab_top;
	unwind->extabBtm = buf.module.extab_btm;
	return 0;
}

int procGetThreadInfo(DbgpProcCB *proc, DbgpThreadInfo *info, int *count,
		      SceUID pid, void *work, SceUInt32 reason)
{
	ThreadmgrThreadInfoForDebugger *thread = work;
	ThreadmgrDebugRegisters regs[2];
	DbgpThreadInfo *out;
	SceSize num = 0;
	SceUID thid;
	int result, n, i, j;

	memset(work, 0, DBGP_THREAD_LIST_SIZE);
	result = SceThreadmgrForDriver_F2C9DC97(pid, 0, work,
						DBGP_THREAD_LIST_SIZE, &num);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1062, "0x%08x\n", result);
		return result;
	}
	n = 0;
	for (i = 0; i < (int)num; i++, thread++) {
		thid = scePUIDtoGUID(pid, thread->threadId);
		if (thid < 0)
			continue;
		result = SceThreadmgrForDriver_5022689D(thid, regs);
		if (result == (int)SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID)
			continue;
		out = &info[n];
		if (result < 0) {
			out->pc = 0;
		} else if (thread->userFrame != 0) {
			out->status |= DBGP_THREAD_STATUS_SYSCALL;
			out->pc = regs[1].pc;
		} else {
			out->pc = regs[0].pc;
		}
		if (procGetThreadDbgpStopReason(proc, &out->stopReason, thid,
						pid, reason) ==
		    (int)SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID) {
			out->pc = 0;
			out->status = 0;
			continue;
		}
		out->size = sizeof(*out);
		out->thid = thread->threadId;
		out->initAttr = thread->initAttr;
		out->attr = thread->attr;
		out->entry = thread->entry;
		out->stackBase = thread->userStackBase;
		out->stackSize = thread->userStackSize;
		out->stackMaxUsed = thread->userStackMaxUsed;
		out->argSize = thread->argSize;
		out->argBlock = thread->pArgBlock;
		out->initPriority = thread->initPriority;
		out->currentPriority = thread->currentPriority;
		out->initCpuAffinityMask = thread->initCpuAffinityMask;
		out->cpuAffinityMask = thread->cpuAffinityMask;
		out->lastCpuId = thread->lastCpuId;
		out->runClocksHigh = thread->runClocksHigh;
		out->runClocksLow = thread->runClocksLow;
		out->exitStatus = thread->exitStatus;
		out->intrPreemptCount = thread->intrPreemptCount;
		out->threadPreemptCount = thread->threadPreemptCount;
		out->threadReleaseCount = thread->threadReleaseCount;
		out->changeCpuCount = thread->changeCpuCount;
		out->valid = 1;
		out->timeout = thread->timeout;
		if (proc->currentThread == thid) {
			out->waitType = 0;
			out->waitId = 0;
			out->status |= DBGP_THREAD_STATUS_STOPPED;
		} else {
			out->status |= thread->status;
			out->waitType = thread->waitType;
			out->waitId = thread->waitId;
		}
		if (thread->status == SCE_KERNEL_THREAD_STATUS_WAITING) {
			switch (thread->waitType) {
			case SCE_KERNEL_WAITTYPE_DELAY:
			case SCE_KERNEL_WAITTYPE_EVENTFLAG:
			case SCE_KERNEL_WAITTYPE_EVENTFLAG_CB:
			case SCE_KERNEL_WAITTYPE_DELAY_CB:
				out->waitDetail[0] = thread->waitDetail[0];
				out->waitDetail[1] = thread->waitDetail[1];
				break;
			case SCE_KERNEL_WAITTYPE_MUTEX:
			case SCE_KERNEL_WAITTYPE_MUTEX_CB:
				out->waitDetail[0] = thread->waitDetail[0];
				break;
			default:
				out->waitDetail[0] = 0;
				out->waitDetail[1] = 0;
				break;
			}
		}
		for (j = 0; j < 32; j++)
			out->name[j] = thread->name[j];
		n++;
	}
	*count = n;
	return 0;
}

int procCreateStopNotification(DbgpProcCB *proc, SceUInt32 reason)
{
	DbgpStopThreads *threads;
	DbgpTime *time;
	SceUInt64 now;
	void *work;
	SceUInt8 *packet;
	SceUID pid;
	int count, size, result;

	pid = proc->pid;
	work = _dbgpAllocWorkBuf(proc, DBGP_THREAD_LIST_SIZE);
	if (work == NULL)
		return (int)0x8008000b;
	size = procGetProcInfoForPacket(proc, pid);
	if (size < 0) {
		SCE_PRINTF_WITH_INFO(15, 834, "0x%08x\n", size);
		return size;
	}
	now = sceKernelGetSystemTimeWide();
	packet = _dbgpAllocSendBuf(proc, size + sizeof(DbgpResponseHeader) +
				   sizeof(DbgpStopThreads) +
				   DBGP_THREAD_MAX * sizeof(DbgpThreadInfo) +
				   sizeof(DbgpTime));
	if (packet == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	memset(packet, 0, size + sizeof(DbgpResponseHeader) +
	       sizeof(DbgpStopThreads) +
	       DBGP_THREAD_MAX * sizeof(DbgpThreadInfo) +
	       sizeof(DbgpTime));
	dbgpFillResponseHeader(proc, DBGP_STOP_GROUP, DBGP_STOP_TYPE, 0, 0);
	result = procGetProcessInfo(proc, packet + sizeof(DbgpResponseHeader),
				    pid, reason, -1);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 873, "0x%08x\n", result);
		return result;
	}
	threads = (DbgpStopThreads *)(packet + sizeof(DbgpResponseHeader) + size);
	threads->reserved = 0;
	switch (reason) {
	case STOP_REASON_PRX:
		threads->thid = -1;
		break;
	case STOP_REASON_BREAK:
		threads->thid = SceThreadmgrForDriver_F311808F(proc->currentThread);
		break;
	default:
		threads->thid = -1;
		break;
	}
	result = procGetThreadInfo(proc, threads->info, &count, pid, work,
				   reason);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 907, "0x%08x\n", result);
		return result;
	}
	threads->count = count;
	time = (DbgpTime *)&threads->info[count];
	time->low = (SceUInt32)now;
	time->high = (SceUInt32)(now >> 32);
	_dbgpSetSendSize(proc, size + sizeof(DbgpResponseHeader) +
			 sizeof(DbgpStopThreads) +
			 count * sizeof(DbgpThreadInfo) + sizeof(DbgpTime));
	return 0;
}

int procInit(DbgpCB *dbgp)
{
	return 0;
}
