/*
 * The queue of thread creations and exits in a debugged process, and the
 * notifications sent for them (Sony's thread_info.c).
 */

#include <kernel/error.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/threadmgr_debugger.h>
#include "dbgp_internal.h"

#define THREAD_INFO_MAX		64

/* the event flag bits that wake the worker */
#define THREAD_EVENT_CREATE	0x10
#define THREAD_EVENT_EXIT	0x20

/* DBGP group 0x10: thread created, thread exited */
#define DBGP_THREAD_GROUP	0x10
#define DBGP_THREAD_CREATE_TYPE	0xc2
#define DBGP_THREAD_EXIT_TYPE	0xc4

typedef struct DbgpThreadCreateNotification {
	DbgpResponseHeader header;
	SceUID pid;
	SceUID thid;
	DbgpTime time;
} DbgpThreadCreateNotification;

typedef struct DbgpThreadExitNotification {
	DbgpResponseHeader header;
	SceUID pid;
	SceUID thid;
	int exitStatus;
	DbgpTime time;
} DbgpThreadExitNotification;

_Static_assert(sizeof(DbgpThreadCreateNotification) == 0x2c, "thread creation notification");
_Static_assert(sizeof(DbgpThreadExitNotification) == 0x30, "thread exit notification");

static SceKernelSpinlock s_threadInfoLock;

int threadInfoSave(DbgpProcCB *proc, const ThreadInfo *info)
{
	ThreadInfoBuf *buf;
	ThreadInfo *last;
	SceKernelIntrStatus intr;
	SceUInt32 event;
	int write, result;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_threadInfoLock);
	buf = &proc->threadInfoBuf;
	if (buf->count >= THREAD_INFO_MAX) {
		SCE_PRINTF_WITH_INFO(15, 42, "0x%08x\n", 0);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_threadInfoLock, intr);
		return (int)0x8008000b;
	}
	write = buf->write;
	buf->count++;
	last = &buf->info[write];
	last->pid = info->pid;
	last->thid = info->thid;
	last->type = info->type;
	last->time = info->time;
	if (info->type == DEBUG_THREAD_EVENT_EXIT)
		last->exitStatus = info->exitStatus;
	else
		last->exitStatus = 0;
	event = info->type == DEBUG_THREAD_EVENT_EXIT ? THREAD_EVENT_EXIT :
							 THREAD_EVENT_CREATE;
	write++;
	if (write > THREAD_INFO_MAX - 1)
		write = 0;
	buf->write = write;
	if (buf->count > 0) {
		result = sceKernelSetEventFlag(proc->eventFlag, event);
		if (result < 0)
			SCE_PRINTF_WITH_INFO(15, 84, "0x%08x\n", result);
	} else {
		result = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_threadInfoLock, intr);
	return result;
}

int threadInfoLoad(DbgpProcCB *proc, ThreadInfo *info)
{
	ThreadInfoBuf *buf;
	ThreadInfo *first;
	SceKernelIntrStatus intr;
	int read, count, result;

	buf = &proc->threadInfoBuf;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_threadInfoLock);
	count = buf->count;
	if (count <= 0) {
		result = (int)0x80080008;
	} else {
		read = buf->read;
		first = &buf->info[read];
		info->time = first->time;
		info->pid = first->pid;
		info->thid = first->thid;
		info->type = first->type;
		if (first->type == DEBUG_THREAD_EVENT_EXIT)
			info->exitStatus = first->exitStatus;
		else
			info->exitStatus = 0;
		read++;
		if (read > THREAD_INFO_MAX - 1)
			read = 0;
		buf->read = read;
		buf->count = count - 1;
		result = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_threadInfoLock, intr);
	return result;
}

int threadInfoGetCount(DbgpProcCB *proc)
{
	return proc->threadInfoBuf.count;
}

int threadInfoCreateThreadNotification(DbgpProcCB *proc, void *packet,
				       const ThreadInfo *info)
{
	DbgpThreadCreateNotification *notification;

	if (info == NULL || packet == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	notification = packet;
	memset(notification, 0, sizeof(*notification));
	_dbgpSetSendSize(proc, sizeof(*notification));
	dbgpFillResponseHeader(proc, DBGP_THREAD_GROUP, DBGP_THREAD_CREATE_TYPE,
			       0, 0);
	notification->pid = info->pid;
	notification->thid = info->thid;
	notification->time.low = (SceUInt32)info->time;
	notification->time.high = (SceUInt32)(info->time >> 32);
	return 0;
}

int threadInfoCreateExitNotification(DbgpProcCB *proc, void *packet,
				     const ThreadInfo *info)
{
	DbgpThreadExitNotification *notification;

	if (info == NULL || packet == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	notification = packet;
	memset(notification, 0, sizeof(*notification));
	_dbgpSetSendSize(proc, sizeof(*notification));
	dbgpFillResponseHeader(proc, DBGP_THREAD_GROUP, DBGP_THREAD_EXIT_TYPE,
			       0, 0);
	notification->pid = info->pid;
	notification->thid = info->thid;
	notification->exitStatus = info->exitStatus;
	notification->time.low = (SceUInt32)info->time;
	notification->time.high = (SceUInt32)(info->time >> 32);
	return 0;
}
