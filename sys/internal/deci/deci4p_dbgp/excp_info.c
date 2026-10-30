/*
 * The queue of stops the exception hooks record for a debugged process,
 * and the stop notification sent for each (Sony's excp_info.c).
 */

#include <sceerror.h>
#include <kernel/error.h>
#include <sysmem/cp15.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include <threadmgr/threadmgr.h>
#include "dbgp_internal.h"

#define EXCP_INFO_MAX		32

/* a data abort that came from the debug unit (DFSR bit 1) */
#define DFSR_DEBUG_EVENT	0x2

/* the stop notification: DBGP group 0x10, type 0xc0 */
#define DBGP_STOP_GROUP		0x10
#define DBGP_STOP_TYPE		0xc0

/* the most threads one notification reports */
#define DBGP_STOP_MAX_THREADS	192

static SceKernelSpinlock s_stopInfoLock;

static int excpInfoNext(int index)
{
	index++;
	if (index > EXCP_INFO_MAX - 1)
		index = 0;
	return index;
}

int excpInfoSave(DbgpProcCB *proc, const DbgpThreadIdentity *thread,
		 SceUInt32 reason, SceUInt32 pc, int kind)
{
	ExcpInfoBuf *buf;
	ExcpInfo *info;
	SceKernelIntrStatus intr;
	SceUInt64 time;
	int write, result;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_stopInfoLock);
	if (dbgpTestProcStatus(proc, 0x20) != 0 &&
	    dbgpTestProcStatus(proc, 0x100) == 0 &&
	    dbgpTestProcStatus(proc, 0x1000) == 0) {
		result = 0;
		goto out;
	}
	dbgpSetProcStatus(proc, 0x20, 1);
	buf = &proc->stopInfoBuf;
	if (buf->count >= EXCP_INFO_MAX) {
		result = (int)0x8008000b;
		goto out;
	}
	proc->stoppedThread = thread->thid;
	time = sceKernelGetSystemTimeWide();
	proc->time.low = (SceUInt32)time;
	proc->time.high = (SceUInt32)(time >> 32);
	if ((readDfsr() & DFSR_DEBUG_EVENT) != 0)
		proc->faultAddress = readDfar();
	if (kind == 2)
		dbgpSetProcStatus(proc, 0x200, 1);
	write = buf->write;
	buf->count++;
	info = &buf->info[write];
	info->thread = *thread;
	info->reason = reason;
	info->pc = pc;
	info->valid = 1;
	info->deferred = 0;
	buf->write = excpInfoNext(write);
	if (buf->count - buf->cancelled - buf->deferred <= 0) {
		result = 0;
		goto out;
	}
	result = sceKernelSetEventFlag(proc->eventFlag, 1);
	if (result < 0)
		SCE_PRINTF_WITH_INFO(15, 122, "0x%08x\n", result);
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_stopInfoLock, intr);
	return result;
}

int excpInfoLoad(DbgpProcCB *proc, DbgpThreadIdentity *thread,
		 SceUInt32 *reason, SceUInt32 *pc)
{
	ExcpInfoBuf *buf;
	ExcpInfo *info, *last;
	SceKernelIntrStatus intr;
	int i, read, result;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_stopInfoLock);
	buf = &proc->stopInfoBuf;
	if (buf->count <= 0) {
		result = (int)0x80080008;
		goto out;
	}
	for (i = 0; i < EXCP_INFO_MAX; i++) {
		read = buf->read;
		info = &buf->info[read];
		if (info->valid == 0) {
			/* cancelled: drop it */
			buf->read = excpInfoNext(read);
			buf->count--;
			buf->cancelled--;
			continue;
		}
		if (info->deferred != 0) {
			/* deferred: queue it again behind the others */
			last = &buf->info[buf->write];
			*last = *info;
			info->valid = 0;
			buf->read = excpInfoNext(read);
			last->valid = 1;
			buf->write = excpInfoNext(buf->write);
			continue;
		}
		*thread = info->thread;
		*pc = info->pc;
		*reason = buf->info[buf->read].reason;
		read = buf->read;
		buf->info[read].valid = 0;
		buf->read = excpInfoNext(read);
		buf->count--;
		result = 0;
		goto out;
	}
	result = SCE_KERNEL_ERROR_NOT_QUEUED;
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_stopInfoLock, intr);
	return result;
}

int excpInfoCancel(DbgpProcCB *proc, SceUID pid, SceUID thid,
		   const void *pc)
{
	ExcpInfoBuf *buf;
	ExcpInfo *info;
	int i;

	buf = &proc->stopInfoBuf;
	if (thid == -1) {
		for (i = 0; i < EXCP_INFO_MAX; i++) {
			info = &buf->info[i];
			if (info->thread.pid == pid && info->pc == (SceUInt32)pc &&
			    info->valid == 1) {
				info->valid = 0;
				buf->cancelled++;
			}
		}
	} else {
		for (i = 0; i < EXCP_INFO_MAX; i++) {
			info = &buf->info[i];
			if (info->thread.pid == pid && info->thread.thid == thid &&
			    info->pc == (SceUInt32)pc && info->valid == 1) {
				info->valid = 0;
				buf->cancelled++;
			}
		}
	}
	return 0;
}

int excpInfoClearAll(DbgpProcCB *proc, SceUID pid)
{
	ExcpInfoBuf *buf;
	ExcpInfo *info;
	int i, result;

	buf = &proc->stopInfoBuf;
	for (i = 0; i < EXCP_INFO_MAX; i++) {
		info = &buf->info[i];
		if (info->thread.pid == pid && info->valid == 1) {
			info->valid = 0;
			buf->cancelled++;
		}
	}
	dbgpSetProcStatus(proc, 0x200, 0);
	result = sceKernelClearEventFlag(proc->eventFlag, ~1);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 325, "0x%08x\n", result);
		return result;
	}
	return 0;
}

int excpInfoGetCount(DbgpProcCB *proc)
{
	SceKernelIntrStatus intr;
	int count;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_stopInfoLock);
	count = proc->stopInfoBuf.count - proc->stopInfoBuf.cancelled -
		proc->stopInfoBuf.deferred;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_stopInfoLock, intr);
	return count;
}

static SceUInt32 excpInfoStopReason(SceUInt32 reason)
{
	if (reason == 0x1000)
		return 48;
	if (reason == 0x10000)
		return 49;
	if (reason == 0x20000)
		return 50;
	if (reason == 0x40000)
		return 51;
	if (reason == 0x80000)
		return 52;
	if (reason == 0x100000)
		return 53;
	if ((reason & 0x200000) != 0)
		return 54;
	return 0;
}

int excpInfoCreateStopNotification(DbgpProcCB *proc,
				   const DbgpThreadIdentity *thread,
				   SceUInt32 reason, SceUInt32 pc)
{
	DbgpStopThreads *threads;
	DbgpTime *time;
	void *work;
	SceUInt8 *packet;
	SceUID pid, thid;
	int count, size, result;

	pid = thread->pid;
	if ((reason & 0x3f1000) != 0) {
		result = procSuspendProcess(proc, thread->pid, thread->thid,
					    excpInfoStopReason(reason));
		if (result < 0)
			return result;
	}
	work = _dbgpAllocWorkBuf(proc, DBGP_THREAD_LIST_SIZE);
	if (work == NULL)
		return (int)0x8008000b;
	size = procGetProcInfoForPacket(proc, pid);
	if (size < 0) {
		SCE_PRINTF_WITH_INFO(15, 416, "0x%08x\n", size);
		return size;
	}
	packet = _dbgpAllocSendBuf(proc, size + sizeof(DbgpResponseHeader) +
				   sizeof(DbgpStopThreads) +
				   DBGP_STOP_MAX_THREADS * sizeof(DbgpThreadInfo) +
				   sizeof(DbgpTime));
	if (packet == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	memset(packet, 0, size + sizeof(DbgpResponseHeader) +
	       sizeof(DbgpStopThreads) +
	       DBGP_STOP_MAX_THREADS * sizeof(DbgpThreadInfo) +
	       sizeof(DbgpTime));
	dbgpFillResponseHeader(proc, DBGP_STOP_GROUP, DBGP_STOP_TYPE, 0, 0);
	result = procGetProcessInfo(proc, packet + sizeof(DbgpResponseHeader),
				    thread->pid, reason, pc);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 454, "0x%08x\n", result);
		return result;
	}
	threads = (DbgpStopThreads *)(packet + sizeof(DbgpResponseHeader) + size);
	thid = SceThreadmgrForDriver_F311808F(thread->thid);
	threads->reserved = 0;
	threads->thid = thid;
	result = procGetThreadInfo(proc, threads->info, &count, thread->pid,
				   work, reason);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 486, "0x%08x\n", result);
		return result;
	}
	threads->count = count;
	time = (DbgpTime *)&threads->info[count];
	time->low = proc->time.low;
	time->high = proc->time.high;
	_dbgpSetSendSize(proc, size + sizeof(DbgpResponseHeader) +
			 sizeof(DbgpStopThreads) +
			 count * sizeof(DbgpThreadInfo) + sizeof(DbgpTime));
	return 0;
}
