/*
 * The queue of core dump events in a debugged process, and the progress
 * and result notifications sent for them (Sony's coredump_info.c).
 */

#include <kernel/error.h>
#include <sceerror.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include <threadmgr/threadmgr.h>
#include "dbgp_internal.h"

#define COREDUMP_INFO_MAX	16

/* the event flag bit that wakes the worker */
#define COREDUMP_EVENT		0x400

/* DBGP group 0x80: the core dump notifications */
#define DBGP_COREDUMP_GROUP		0x80
#define DBGP_COREDUMP_RESULT_TYPE	0x20
#define DBGP_COREDUMP_PROGRESS_TYPE	0x22

/* DbgpCoredumpResultNotification.result */
#define DBGP_COREDUMP_FAILED		0x200e1
#define DBGP_COREDUMP_CANCELLED		0x200e2

#define ALIGN4(x)		(((x) + 3) & ~3)

typedef struct DbgpCoredumpResultNotification {
	DbgpResponseHeader header;
	SceUID pid;
	int task_id;
	SceSize size;			/* of the fields below */
	SceUInt32 result;
	SceSize pathSize;
	char path[];
} DbgpCoredumpResultNotification;

typedef struct DbgpCoredumpProgressNotification {
	DbgpResponseHeader header;
	SceUID pid;
	int task_id;
	SceUInt32 progress;
} DbgpCoredumpProgressNotification;

_Static_assert(sizeof(DbgpCoredumpResultNotification) == 0x30, "core dump result notification");
_Static_assert(sizeof(DbgpCoredumpProgressNotification) == 0x28, "core dump progress notification");

static SceKernelSpinlock s_coredumpInfoLock;

int coredumpInfoSave(DbgpProcCB *proc, const CoredumpInfo *info)
{
	CoredumpInfoBuf *buf;
	SceKernelIntrStatus intr;
	int write, result;

	buf = &proc->coredumpInfoBuf;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_coredumpInfoLock);
	if (buf->count > COREDUMP_INFO_MAX - 1) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_coredumpInfoLock, intr);
		return (int)0x8008000b;
	}
	write = buf->write;
	buf->count++;
	buf->info[write] = *info;
	write++;
	if (write > COREDUMP_INFO_MAX - 1)
		write = 0;
	buf->write = write;
	if (buf->count > 0) {
		result = sceKernelSetEventFlag(proc->eventFlag, COREDUMP_EVENT);
		if (result < 0)
			SCE_PRINTF_WITH_INFO(15, 78, "0x%08x\n", result);
	} else {
		result = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_coredumpInfoLock, intr);
	return result;
}

int coredumpInfoLoad(DbgpProcCB *proc, CoredumpInfo *info)
{
	CoredumpInfoBuf *buf;
	SceKernelIntrStatus intr;
	int read, count, result;

	buf = &proc->coredumpInfoBuf;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_coredumpInfoLock);
	count = buf->count;
	if (count <= 0) {
		result = (int)0x80080008;
	} else {
		read = buf->read;
		*info = buf->info[read];
		read++;
		if (read > COREDUMP_INFO_MAX - 1)
			read = 0;
		buf->read = read;
		buf->count = count - 1;
		result = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_coredumpInfoLock, intr);
	return result;
}

int coredumpInfoGetCount(DbgpProcCB *proc)
{
	return proc->coredumpInfoBuf.count;
}

int coredumpInfoCreateCoredumpNotification(DbgpProcCB *proc, void *packet,
					   const CoredumpInfo *info)
{
	DbgpCoredumpResultNotification *done;
	DbgpCoredumpProgressNotification *progress;
	SceUInt32 result;
	SceSize size, pathSize;
	SceUID pid;

	if (packet == NULL || info == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (info->type == COREDUMP_INFO_RESULT) {
		size = ALIGN4(info->pathSize);
		done = packet;
		memset(done, 0, sizeof(*done) + size);
		_dbgpSetSendSize(proc, sizeof(*done) + size);
		dbgpFillResponseHeader(proc, DBGP_COREDUMP_GROUP,
				       DBGP_COREDUMP_RESULT_TYPE, 0, 0);
		if (info->result == SCE_ERROR_ERRNO_ECANCELED)
			result = DBGP_COREDUMP_CANCELLED;
		else if (info->result != 0)
			result = DBGP_COREDUMP_FAILED;
		else
			result = 0;
		pid = info->pid;
		done->task_id = info->task_id;
		pathSize = info->pathSize;
		done->size = size + sizeof(done->result) + sizeof(done->pathSize) +
			     sizeof(done->size);
		done->pid = pid;
		done->result = result;
		done->pathSize = pathSize;
		strncpy(done->path, info->path, pathSize);
		return 0;
	}
	if (info->type == COREDUMP_INFO_PROGRESS) {
		progress = packet;
		memset(progress, 0, sizeof(*progress));
		_dbgpSetSendSize(proc, sizeof(*progress));
		dbgpFillResponseHeader(proc, DBGP_COREDUMP_GROUP,
				       DBGP_COREDUMP_PROGRESS_TYPE, 0, 0);
		progress->pid = info->pid;
		progress->task_id = info->task_id;
		progress->progress = info->progress;
		return 0;
	}
	return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
}
