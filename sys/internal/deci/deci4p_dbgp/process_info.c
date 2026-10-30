/*
 * The queue of process events in a debugged process, and the notifications
 * sent for them (Sony's process_info.c).
 */

#include <kernel/error.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include <threadmgr/threadmgr.h>
#include "dbgp_internal.h"

#define PROCESS_INFO_MAX	4

/* the event flag bits that wake the worker */
#define PROCESS_EVENT_START_RESUME	0x40
#define PROCESS_EVENT_OTHER		0x80
#define PROCESS_EVENT_SPONTANEOUS_EXIT	0x4000000
#define PROCESS_EVENT_APP_ABORT		0x8000000

/* DBGP group 0x10: the process notifications */
#define DBGP_PROCESS_GROUP		0x10
#define DBGP_PROCESS_START_TYPE		0xc6
#define DBGP_PROCESS_EXIT_TYPE		0xc8
#define DBGP_PROCESS_KILL_TYPE		0xca
#define DBGP_PROCESS_SUSPEND_TYPE	0xd0
#define DBGP_PROCESS_RESUME_TYPE	0xd1
#define DBGP_PROCESS_EXIT_TO_SPAWN_TYPE	0xd4
#define DBGP_PROCESS_EXIT_SPAWN_TYPE	0xd5

typedef struct DbgpProcessStartNotification {
	DbgpResponseHeader header;
	SceUID parentPid;
	SceUID pid;
	DbgpTime time;
} DbgpProcessStartNotification;

typedef struct DbgpProcessExitNotification {
	DbgpResponseHeader header;
	SceUID pid;
	int exitStatus;
	DbgpTime time;
} DbgpProcessExitNotification;

/* kill, suspend and resume */
typedef struct DbgpProcessNotification {
	DbgpResponseHeader header;
	SceUID pid;
	DbgpTime time;
} DbgpProcessNotification;

typedef struct DbgpProcessSpawnNotification {
	DbgpResponseHeader header;
	SceUID parentPid;
	DbgpTime time;
	SceUID pid;
} DbgpProcessSpawnNotification;

typedef struct DbgpProcessExitSpawnNotification {
	DbgpResponseHeader header;
	SceUID parentPid;
	DbgpTime time;
	SceUID pid;
	SceUInt32 result;		/* dbgpKernelErrToDbgpErr */
} DbgpProcessExitSpawnNotification;

_Static_assert(sizeof(DbgpProcessStartNotification) == 0x2c, "process start notification");
_Static_assert(sizeof(DbgpProcessExitNotification) == 0x2c, "process exit notification");
_Static_assert(sizeof(DbgpProcessNotification) == 0x28, "process notification");
_Static_assert(sizeof(DbgpProcessSpawnNotification) == 0x2c, "process spawn notification");
_Static_assert(sizeof(DbgpProcessExitSpawnNotification) == 0x30, "exit spawn notification");

static SceKernelSpinlock s_processInfoLock;

int processInfoSave(DbgpProcCB *proc, const ProcessInfo *info)
{
	ProcessInfoBuf *buf;
	SceKernelIntrStatus intr;
	SceUInt32 event;
	int write, result;

	buf = &proc->procInfoBuf;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_processInfoLock);
	if (buf->count > PROCESS_INFO_MAX - 1) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_processInfoLock, intr);
		return (int)0x8008000b;
	}
	write = buf->write;
	buf->count++;
	buf->info[write] = *info;
	if ((info->type & ~PROCESS_INFO_RESUME) == PROCESS_INFO_START)
		event = PROCESS_EVENT_START_RESUME;
	else if (info->type == PROCESS_INFO_SPONTANEOUS_EXIT)
		event = PROCESS_EVENT_SPONTANEOUS_EXIT;
	else if (info->type == PROCESS_INFO_APP_ABORT)
		event = PROCESS_EVENT_APP_ABORT;
	else
		event = PROCESS_EVENT_OTHER;
	write++;
	if (write > PROCESS_INFO_MAX - 1)
		write = 0;
	buf->write = write;
	if (buf->count > 0) {
		result = sceKernelSetEventFlag(proc->eventFlag, event);
		if (result < 0)
			SCE_PRINTF_WITH_INFO(15, 109, "0x%08x\n", result);
	} else {
		result = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_processInfoLock, intr);
	return result;
}

int processInfoLoad(DbgpProcCB *proc, ProcessInfo *info)
{
	ProcessInfoBuf *buf;
	SceKernelIntrStatus intr;
	int read, count, result;

	buf = &proc->procInfoBuf;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_processInfoLock);
	count = buf->count;
	if (count <= 0) {
		result = (int)0x80080008;
	} else {
		read = buf->read;
		*info = buf->info[read];
		read++;
		if (read > PROCESS_INFO_MAX - 1)
			read = 0;
		buf->read = read;
		buf->count = count - 1;
		result = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_processInfoLock, intr);
	return result;
}

int processInfoGetCount(DbgpProcCB *proc)
{
	return proc->procInfoBuf.count;
}

static void processInfoFillTime(DbgpTime *time, SceUInt64 value)
{
	time->low = (SceUInt32)value;
	time->high = (SceUInt32)(value >> 32);
}

int processInfoCreateProcessNotification(DbgpProcCB *proc, void *packet,
					 const ProcessInfo *info)
{
	DbgpProcessStartNotification *start;
	DbgpProcessExitNotification *exit;
	DbgpProcessNotification *event;
	DbgpProcessSpawnNotification *spawn;
	DbgpProcessExitSpawnNotification *exitSpawn;
	DbgpSpawnInfo spawnInfo;
	DbgpCB *dbgp;

	if (info == NULL || packet == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	switch (info->type) {
	case PROCESS_INFO_START:
		dbgp = dbgpGetDbgpCB();
		if (dbgp == NULL)
			return (int)0x80080001;
		if (dbgp->spawn.pid == info->parentPid)
			__builtin_memset(&dbgp->spawn, 0, sizeof(dbgp->spawn));
		start = packet;
		memset(start, 0, sizeof(*start));
		_dbgpSetSendSize(proc, sizeof(*start));
		dbgpFillResponseHeader(proc, DBGP_PROCESS_GROUP,
				       DBGP_PROCESS_START_TYPE, 0, 0);
		start->parentPid = info->parentPid;
		start->pid = info->pid;
		processInfoFillTime(&start->time, info->time);
		return 0;
	case PROCESS_INFO_EXIT:
		exit = packet;
		memset(exit, 0, sizeof(*exit));
		_dbgpSetSendSize(proc, sizeof(*exit));
		dbgpFillResponseHeader(proc, DBGP_PROCESS_GROUP,
				       DBGP_PROCESS_EXIT_TYPE, 0, 0);
		exit->pid = info->pid;
		exit->exitStatus = info->exitStatus;
		processInfoFillTime(&exit->time, info->time);
		return 0;
	case PROCESS_INFO_KILL:
		event = packet;
		memset(event, 0, sizeof(*event));
		_dbgpSetSendSize(proc, sizeof(*event));
		dbgpFillResponseHeader(proc, DBGP_PROCESS_GROUP,
				       DBGP_PROCESS_KILL_TYPE, 0, 0);
		event->pid = info->pid;
		processInfoFillTime(&event->time, info->time);
		return 0;
	case PROCESS_INFO_SUSPEND:
		event = packet;
		memset(event, 0, sizeof(*event));
		_dbgpSetSendSize(proc, sizeof(*event));
		dbgpFillResponseHeader(proc, DBGP_PROCESS_GROUP,
				       DBGP_PROCESS_SUSPEND_TYPE, 0, 0);
		event->pid = info->pid;
		processInfoFillTime(&event->time, info->time);
		return 0;
	case PROCESS_INFO_RESUME:
		event = packet;
		memset(event, 0, sizeof(*event));
		_dbgpSetSendSize(proc, sizeof(*event));
		dbgpFillResponseHeader(proc, DBGP_PROCESS_GROUP,
				       DBGP_PROCESS_RESUME_TYPE, 0, 0);
		event->pid = info->pid;
		processInfoFillTime(&event->time, info->time);
		return 0;
	case PROCESS_INFO_EXIT_TO_SPAWN:
		dbgp = dbgpGetDbgpCB();
		if (dbgp == NULL)
			return (int)0x80080001;
		spawnInfo.pid = info->parentPid;
		spawnInfo.prxDebugFlag = proc->prxDebugFlag;
		dbgp->spawn = spawnInfo;
		spawn = packet;
		memset(spawn, 0, sizeof(*spawn));
		_dbgpSetSendSize(proc, sizeof(*spawn));
		dbgpFillResponseHeader(proc, DBGP_PROCESS_GROUP,
				       DBGP_PROCESS_EXIT_TO_SPAWN_TYPE, 0, 0);
		spawn->parentPid = info->parentPid;
		processInfoFillTime(&spawn->time, info->time);
		spawn->pid = info->pid;
		return 0;
	case PROCESS_INFO_EXIT_SPAWN:
		dbgp = dbgpGetDbgpCB();
		if (dbgp == NULL)
			return (int)0x80080001;
		if (dbgp->spawn.pid == info->parentPid)
			__builtin_memset(&dbgp->spawn, 0, sizeof(dbgp->spawn));
		exitSpawn = packet;
		memset(exitSpawn, 0, sizeof(*exitSpawn));
		_dbgpSetSendSize(proc, sizeof(*exitSpawn));
		dbgpFillResponseHeader(proc, DBGP_PROCESS_GROUP,
				       DBGP_PROCESS_EXIT_SPAWN_TYPE, 0, 0);
		exitSpawn->parentPid = info->parentPid;
		processInfoFillTime(&exitSpawn->time, info->time);
		exitSpawn->pid = info->pid;
		exitSpawn->result = dbgpKernelErrToDbgpErr(info->exitStatus);
		return 0;
	default:
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
}
