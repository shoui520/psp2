/* Stepping a stopped thread over a breakpoint (Sony's single_step.c). */

#include <kernel/error.h>
#include <processmgr/processmgr.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysclib.h>
#include <sysmem/debug.h>
#include "dbgp_internal.h"

#define INLINE static inline __attribute__((always_inline))

/* Hardware breakpoint 2 stops on the next instruction after the PC. */
#define STEP_BKPT_INDEX		2
#define STEP_BKPT_CONTROL	0x4001e5

/* Remember the breakpoint lifted for the step, to put it back after. */
INLINE void stepSaveSwbkpt(DbgpProcCB *proc, SwbkptSpecificInfo info)
{
	if (info.size != sizeof(info))
		return;
	proc->stepSwbkpt[info.index].address = (SceUInt32)info.address;
	proc->stepSwbkpt[info.index].thumb = info.thumb;
	proc->stepSwbkpt[info.index].pid = info.pid;
	proc->stepSwbkpt[info.index].thid = info.thid;
	if (info.thumb != 0) {
		proc->stepSwbkpt[info.index].original.bytes[0] = info.original.bytes[0];
		proc->stepSwbkpt[info.index].original.bytes[1] = info.original.bytes[1];
	} else {
		proc->stepSwbkpt[info.index].original.opcode = info.original.opcode;
	}
	proc->stepSwbkpt[info.index].active = 1;
}

/* The breakpoint lifted for the step, if any. */
INLINE void stepLoadSwbkpt(const DbgpProcCB *proc, SwbkptSpecificInfo *info)
{
	if (proc->stepSwbkpt[0].active == 0)
		return;
	info->address = (void *)proc->stepSwbkpt[0].address;
	info->pid = proc->stepSwbkpt[0].pid;
	info->thid = proc->stepSwbkpt[0].thid;
	info->thumb = proc->stepSwbkpt[0].thumb;
	if (info->thumb != 0) {
		info->original.bytes[0] = proc->stepSwbkpt[0].original.bytes[0];
		info->original.bytes[1] = proc->stepSwbkpt[0].original.bytes[1];
	} else {
		info->original.opcode = proc->stepSwbkpt[0].original.opcode;
	}
}

int stepThreadSpecificBpPre(DbgpProcCB *proc)
{
	DbgpThreadIdentity thread;
	SwbkptSpecificInfo info;
	SceUInt32 pc, reason;
	SceUID pid, thid;
	SceUID *threads;
	int result, suspend;

	result = excpInfoLoad(proc, &thread, &reason, &pc);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 78, "0x%08x\n", result);
		return result;
	}
	pid = thread.pid;
	thid = thread.thid;
	threads = _dbgpAllocWorkBuf(proc, 0x11000);
	if (threads == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	if (processWaitThreadSuspendForProcess(pid, threads, 1) < 0)
		return (int)0x80080001;
	dbgpSetProcStatus(proc, 0x100, 1);

	info = (SwbkptSpecificInfo){ .size = sizeof(info) };
	result = swbkptGetSpecificSwbkpt(proc, pid, thid, (void *)pc, &info);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 119, "0x%08x:0x%08x\n", pid,
				     proc->stoppedThread);
		return result;
	}
	stepSaveSwbkpt(proc, info);
	if (info.thumb == 0)
		result = sceKernelCopyToUserProcTextDomainForDBGP(pid,
				info.address, &info.original, 4);
	else
		result = sceKernelCopyToUserProcTextDomainForDBGP(pid,
				info.address, &info.original, 2);
	if (result < 0)
		return result;
	dbgpFlushCache(&info.address);

	result = sceKernelSetPHBP(pid, STEP_BKPT_INDEX, pc, STEP_BKPT_CONTROL);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 165, "0x%08x\n", result);
		return result;
	}
	dbgpSetProcStatus(proc, 0x20, 0);
	suspend = sceKernelIsThreadDebugSuspended(thid);
	if (suspend < 0) {
		SCE_PRINTF_WITH_INFO(15, 253, "0x%08x\n", suspend);
		return suspend;
	}
	result = sceKernelDebugResumeThread(thid, suspend);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 261, "0x%08x\n", result);
		return result;
	}
	return 0;
}

int stepThreadSpecificBpPost(DbgpProcCB *proc)
{
	DbgpThreadIdentity thread;
	SwbkptSpecificInfo info;
	SceUInt32 pc, reason, status;
	SceUID pid, thid;
	void *threads;
	int result;

	result = excpInfoLoad(proc, &thread, &reason, &pc);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 289, "0x%08x\n", result);
		return result;
	}
	info = (SwbkptSpecificInfo){ .size = sizeof(info) };
	pid = thread.pid;
	thid = thread.thid;
	stepLoadSwbkpt(proc, &info);
	result = swbkptSetSpecificSwbkpt(proc, info);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 310, "0x%08x\n", result);
		return result;
	}
	result = sceKernelSetPHBP(pid, STEP_BKPT_INDEX, pc, 0);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 321, "0x%08x\n", result);
		return result;
	}
	result = sceKernelIsThreadDebugSuspended(thid);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 340, "0x%08x\n", result);
		return result;
	}
	if ((result & 0x1000) == 0) {
		dbgpSetProcStatus(proc, 0x20, 0);
		dbgpSetProcStatus(proc, 0x100, 0);
		excpInfoSave(proc, &thread, reason, pc, 0);
		return 0;
	}

	result = sceKernelIsThreadDebugSuspended(thid);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 364, "0x%08x\n", result);
		return result;
	}
	result = SceThreadmgrForDriver_04C6764B(thid, 2);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 372, "0x%08x\n", result);
		return result;
	}
	threads = _dbgpAllocWorkBuf(proc, 0x11000);
	if (threads == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	result = processChangeStatusThreadResumeForProcess(pid, threads);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 389, "0x%08x\n", result);
		return (int)0x80080001;
	}
	result = sceKernelGetProcessStatus(pid, &status);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 396, "0x%08x\n", result);
		return result;
	}
	status = (SceUInt8)status;
	result = sceKernelProcessDebugResume(pid, status);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 405, "0x%08x\n", result);
		return result;
	}
	return 0;
}

int stepAutoSkipBpPre(DbgpProcCB *proc, const DbgpThreadListRequest *packet)
{
	ThreadmgrDebugRegisters registers[2];
	SwbkptSpecificInfo info;
	SceUInt32 status, pc;
	SceUID pid, userThid, thid;
	int result, suspend;

	proc->resumePid = 0;
	proc->resumeThid = 0;
	status = 0;
	if (_dbgpAllocWorkBuf(proc, 0x11000) == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;

	/* The first listed thread stopped on a breakpoint is stepped. */
	for (;;) {
		if (packet != NULL) {
			pid = packet->header.pid;
			userThid = packet->thid[proc->skipIndex];
			proc->resumePacket = packet;
		} else {
			if (proc->resumePacket == NULL)
				SCE_PANIC(487);
			pid = proc->resumePacket->header.pid;
			userThid = proc->resumePacket->thid[proc->skipIndex];
		}
		thid = scePUIDtoGUID(pid, userThid);
		suspend = sceKernelIsThreadDebugSuspended(thid);
		if (suspend < 0) {
			SCE_PRINTF_WITH_INFO(15, 503, "0x%08x\n", suspend);
			return suspend;
		}
		if (suspend == 0) {
			SCE_PRINTF_WITH_INFO(15, 507, "0x%08x\n", userThid);
			return (int)0x80080001;
		}
		result = SceThreadmgrForDriver_5022689D(thid, registers);
		if (result < 0)
			SCE_PRINTF_WITH_INFO(15, 514, "0x%08x\n", result);
		pc = registers[0].pc;
		if (isDbgpThreadPcBkpt(pid, (const void *)pc, registers[0].cpsr))
			break;
		++proc->skipIndex;
		if (packet != NULL) {
			if (proc->skipIndex > packet->count)
				SCE_PANIC(530);
		} else if (proc->skipIndex > proc->resumePacket->count) {
			return 0;
		}
	}

	result = sceKernelGetProcessStatus(pid, &status);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 550, "0x%08x\n", result);
		return result;
	}
	if (status == 3)
		return (int)0x80080001;

	info = (SwbkptSpecificInfo){ .size = sizeof(info) };
	result = swbkptGetSpecificSwbkpt(proc, pid, userThid, (void *)pc, &info);
	if (result != -5) {
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 580, "0x%08x:0x%08x\n", pid,
					     proc->stoppedThread);
			return result;
		}
		stepSaveSwbkpt(proc, info);
		if (info.thumb == 0)
			result = sceKernelCopyToUserProcTextDomainForDBGP(pid,
					info.address, &info.original, 4);
		else
			result = sceKernelCopyToUserProcTextDomainForDBGP(pid,
					info.address, &info.original, 2);
		if (result < 0)
			return result;
		dbgpFlushCache(&info.address);
	}

	result = sceKernelSetPHBP(pid, STEP_BKPT_INDEX, pc, STEP_BKPT_CONTROL);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 623, "0x%08x\n", result);
		return result;
	}
	dbgpSetProcStatus(proc, 0x20, 0);
	suspend = sceKernelIsThreadDebugSuspended(thid);
	if (suspend < 0) {
		SCE_PRINTF_WITH_INFO(15, 709, "0x%08x\n", suspend);
		return suspend;
	}
	dbgpSetProcStatus(proc, 0x20, 0);
	result = sceKernelDebugResumeThread(thid, suspend);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 720, "0x%08x\n", result);
		return result;
	}
	return 0;
}

int stepAutoSkipBpMid(DbgpProcCB *proc)
{
	DbgpThreadIdentity thread;
	SwbkptSpecificInfo info;
	SceUInt32 pc, reason;
	SceUID pid;
	int result, suspend;

	result = excpInfoLoad(proc, &thread, &reason, &pc);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 756, "0x%08x\n", result);
		return result;
	}
	if (proc->resumePacket == NULL) {
		SCE_PRINTF_WITH_INFO(15, 762, "\n");
		return (int)0x80080001;
	}
	pid = thread.pid;
	proc->resumePid = thread.pid;
	proc->resumeThid = thread.thid;
	suspend = sceKernelIsThreadDebugSuspended(thread.thid);
	if (suspend < 0) {
		SCE_PRINTF_WITH_INFO(15, 777, "0x%08x\n", suspend);
		return suspend;
	}
	info = (SwbkptSpecificInfo){ .size = sizeof(info) };
	if (proc->stepSwbkpt[0].active != 0) {
		stepLoadSwbkpt(proc, &info);
		result = swbkptSetSpecificSwbkpt(proc, info);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 799, "0x%08x\n", result);
			return result;
		}
	}
	result = sceKernelSetPHBP(pid, STEP_BKPT_INDEX, pc, 0);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 811, "0x%08x\n", result);
		return result;
	}
	if ((suspend & 0x1000) == 0 || dbgpTestProcStatus(proc, 0x20000) != 0) {
		dbgpSetProcStatus(proc, 0x20, 0);
		dbgpSetProcStatus(proc, 0x4000, 1);
		excpInfoSave(proc, &thread, reason, pc, 0);
		proc->skipIndex = 0;
		return 0;
	}

	/* Step the next listed thread, or report that all have been. */
	++proc->skipIndex;
	if (proc->skipIndex == proc->resumePacket->count) {
		dbgpSetProcStatus(proc, 0x2000, 1);
		return 0;
	}
	result = stepAutoSkipBpPre(proc, NULL);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 850, "0x%08x\n", result);
		return result;
	}
	return 0;
}

int stepAutoSkipBpPost(DbgpProcCB *proc)
{
	DbgpThreadListResponse *reply;
	SceUInt32 status, code;
	SceUID pid, thid;
	SceSize size;
	void *threads;
	int result;

	if (proc->resumePacket == NULL) {
		SCE_PRINTF_WITH_INFO(15, 874, "\n");
		return (int)0x80080001;
	}
	if (proc->resumePid != 0) {
		pid = proc->resumePid;
		thid = proc->resumeThid;
	} else if (proc->resumeThid != 0) {
		pid = proc->resumePid;
		thid = proc->resumeThid;
	} else {
		pid = proc->resumePacket->header.pid;
		thid = scePUIDtoGUID(pid,
				proc->resumePacket->thid[proc->skipIndex]);
	}
	proc->skipIndex = 0;

	if (dbgpTestProcStatus(proc, 0x2000) != 0) {
		size = sizeof(*reply);
		code = 0;
	} else {
		size = sizeof(*reply) + sizeof(reply->thid[0]);
		code = 0x20057;
	}
	reply = _dbgpAllocSendBuf(proc, size);
	if (reply == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(reply, 0, size);
	_dbgpSetSendSize(proc, size);

	if (dbgpTestProcStatus(proc, 0x2000) != 0) {
		result = sceKernelIsThreadDebugSuspended(thid);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 937, "0x%08x\n", result);
			return result;
		}
		result = SceThreadmgrForDriver_04C6764B(thid, 2);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 945, "0x%08x\n", result);
			return result;
		}
		threads = _dbgpAllocWorkBuf(proc, 0x11000);
		if (threads == NULL)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		result = processChangeStatusThreadResumeForProcess(pid, threads);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 962, "0x%08x\n", result);
			return (int)0x80080001;
		}
		dbgpSetProcStatus(proc, 0x20, 0);
		result = sceKernelGetProcessStatus(pid, &status);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 972, "0x%08x\n", result);
			return result;
		}
		status = (SceUInt8)status;
		result = sceKernelProcessDebugResume(pid, status);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 981, "0x%08x\n", result);
			return result;
		}
	}

	reply->pid = pid;
	if (dbgpTestProcStatus(proc, 0x2000) != 0) {
		reply->count = 0;
	} else {
		reply->count = 1;
		reply->thid[0] = SceThreadmgrForDriver_F311808F(thid);
	}
	dbgpFillResponseHeader(proc, proc->resumePacket->header.prefix.group,
			       0x23, 0, code);
	return 0;
}
