/*
 * The DBGP thread group: a process's threads, their stacks and names,
 * suspending and resuming one, and the synchronization objects they wait
 * on (Sony's group_thread.c).
 */

#include <kernel/error.h>
#include <kernel/threadmgr.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <sysmem/uid_guid.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/threadmgr_debugger.h>
#include <threadmgr/lw_mutex.h>
#include <threadmgr/threadmgr_event.h>
#include <ulobjmgr/ulobjmgr.h>
#include "dbgp_internal.h"

/* the thread record types the host asks for */
#define DBGP_THREAD_TYPE_NORMAL	0
#define DBGP_THREAD_TYPE_EX	2

/* what _getThreadInfo is asked and answers */
typedef struct DbgpThreadQuery {
	SceUID pid;
	int type;			/* DBGP_THREAD_TYPE_* */
	int count;			/* the records written */
} DbgpThreadQuery;

typedef struct DbgpThreadRequest {
	DbgpPrefix prefix;
	SceUID pid;
	SceUID thid;
} DbgpThreadRequest;

typedef struct DbgpThreadInfoResponse {
	DbgpResponseHeader header;
	SceUID pid;
	SceUInt32 count;
	DbgpThreadInfo info[];		/* or DbgpThreadInfoEx */
} DbgpThreadInfoResponse;

typedef struct DbgpThreadResponse {
	DbgpResponseHeader header;
	SceUID pid;
	SceUID thid;
	SceUInt32 status;		/* threadSuspendThreadCmd */
} DbgpThreadResponse;

typedef struct DbgpThreadNamesResponse {
	DbgpResponseHeader header;
	SceUInt32 count;
	ThreadmgrThreadName names[];
} DbgpThreadNamesResponse;

typedef struct DbgpStackInfo {
	SceSize size;
	SceUID thid;
	SceSize stackMaxUsed;
	SceSize stackUsed;
} DbgpStackInfo;

typedef struct DbgpStackInfoResponse {
	DbgpResponseHeader header;
	SceUInt32 count;
	DbgpStackInfo info[];
} DbgpStackInfoResponse;

/* the synchronization object kinds of _getSyncPrimitiveInfo */
#define DBGP_SYNC_SEMAPHORE	0x10
#define DBGP_SYNC_EVENT_FLAG	0x20
#define DBGP_SYNC_MUTEX		0x30
#define DBGP_SYNC_LW_MUTEX	0x31
#define DBGP_SYNC_MSG_PIPE	0x50
#define DBGP_SYNC_SIMPLE_EVENT	0x51
#define DBGP_SYNC_CALLBACK	0x60
#define DBGP_SYNC_TIMER		0x81
#define DBGP_SYNC_RW_LOCK	0x90
#define DBGP_SYNC_COND		0xa0
#define DBGP_SYNC_LW_COND	0xa1

/* how the work buffer is split: the object list, then one waiting list */
#define DBGP_SYNC_LIST_SIZE	0x30000
#define DBGP_SYNC_WAIT_SIZE	0x2000

/* a thread waiting on an object */
typedef struct DbgpWaitingThread {
	SceUID pid;
	SceUID thid;
} DbgpWaitingThread;

/* what every object record starts with */
typedef struct DbgpSyncHeader {
	SceUInt32 kind;			/* DBGP_SYNC_* */
	SceSize size;			/* of the record after these two */
	SceUInt32 type;			/* the kind again */
	SceUID uid;
} DbgpSyncHeader;

typedef struct DbgpSemaphoreRecord {
	DbgpSyncHeader header;
	SceUID pid;
	char name[32];
	SceUInt32 attr;			/* 0x34 */
	SceUInt32 reserved;
	int initCount;
	int currentCount;
	int maxCount;
	SceUInt32 numWaitThreads;	/* 0x48 */
	DbgpWaitingThread waiting[];
} DbgpSemaphoreRecord;

typedef struct DbgpEventFlagRecord {
	DbgpSyncHeader header;
	SceUID pid;
	char name[32];
	SceUInt32 attr;			/* 0x34 */
	SceUInt32 reserved;
	SceUInt32 initPattern;
	SceUInt32 currentPattern;
	SceUInt32 numWaitThreads;	/* 0x44 */
	DbgpWaitingThread waiting[];
} DbgpEventFlagRecord;

typedef struct DbgpMutexRecord {
	DbgpSyncHeader header;
	SceUID pid;
	char name[32];
	SceUInt32 attr;			/* 0x34 */
	SceUInt32 reserved;
	int initCount;
	int currentCount;
	SceUID currentOwnerId;
	SceUInt32 numWaitThreads;	/* 0x48 */
	DbgpWaitingThread waiting[];	/* then the ceiling priority */
} DbgpMutexRecord;

typedef struct DbgpLwMutexRecord {
	DbgpSyncHeader header;
	char name[32];
	SceUInt32 attr;			/* 0x30 */
	SceUInt32 work;			/* the lightweight mutex's user work area */
	int initCount;
	int currentCount;
	SceUID currentOwnerId;
	SceUInt32 numWaitThreads;	/* 0x44 */
	SceUID waiting[];		/* the user thread IDs */
} DbgpLwMutexRecord;

typedef struct DbgpMsgPipeRecord {
	DbgpSyncHeader header;
	SceUID pid;
	char name[32];
	SceUInt32 attr;			/* 0x34 */
	SceUInt32 reserved;
	SceSize bufferSize;
	SceSize freeSize;
	SceUInt32 numSendWaitThreads;
	SceUInt32 numReceiveWaitThreads; /* 0x48 */
	DbgpWaitingThread waiting[];	/* then a DbgpMsgPipeEvent */
} DbgpMsgPipeRecord;

/* the event state behind a message pipe */
typedef struct DbgpMsgPipeEvent {
	SceUInt32 eventPattern;
	SceUInt32 userDataLow;
	SceUInt32 userDataHigh;
} DbgpMsgPipeEvent;

typedef struct DbgpSimpleEventRecord {
	DbgpSyncHeader header;
	SceUID pid;
	char name[32];
	SceUInt32 attr;			/* 0x34 */
	SceUInt32 eventPattern;
	SceUInt32 userDataLow;		/* 0x3c */
	SceUInt32 userDataHigh;
	SceUInt32 numWaitThreads;	/* 0x44 */
	DbgpWaitingThread waiting[];
} DbgpSimpleEventRecord;

typedef struct DbgpCallbackRecord {
	DbgpSyncHeader header;
	SceUID pid;
	SceUID threadId;
	char name[32];			/* 0x18 */
	SceUInt32 attr;			/* 0x38 */
	SceUInt32 reserved;
	SceUInt32 callbackFunc;		/* 0x40 */
	SceUInt32 common;
	SceInt32 notifyCount;
	SceInt32 notifyArg;
} DbgpCallbackRecord;

typedef struct DbgpCondRecord {
	DbgpSyncHeader header;
	SceUID pid;
	char name[32];
	SceUInt32 attr;			/* 0x34 */
	SceUInt32 reserved;
	SceUID mutexId;
	SceUInt32 numWaitThreads;	/* 0x40 */
	DbgpWaitingThread waiting[];
} DbgpCondRecord;

/* a SceKernelSysClock, word aligned */
typedef struct DbgpSysClock {
	SceUInt32 low;
	SceUInt32 hi;
} DbgpSysClock;

typedef struct DbgpTimerRecord {
	DbgpSyncHeader header;
	SceUID pid;
	char name[32];
	SceUInt32 attr;			/* 0x34 */
	SceUInt32 reserved;
	SceInt32 fActive;
	DbgpSysClock baseTime;		/* 0x40 */
	DbgpSysClock currentTime;
	DbgpSysClock schedule;		/* 0x50 */
	DbgpSysClock interval;
	SceInt32 type;			/* 0x60 */
	SceInt32 fRepeat;
	SceUInt32 numWaitThreads;
	DbgpWaitingThread waiting[];	/* then a DbgpMsgPipeEvent */
} DbgpTimerRecord;

typedef struct DbgpRWLockRecord {
	DbgpSyncHeader header;
	SceUID pid;
	char name[32];
	SceUInt32 attr;			/* 0x34 */
	SceUInt32 reserved;
	SceUInt32 numReadWaitThreads;
	SceUInt32 numWriteWaitThreads;	/* 0x40 */
	DbgpWaitingThread waiting[];	/* then a DbgpRWLockState */
} DbgpRWLockRecord;

/* what follows a reader-writer lock's waiting threads */
typedef struct DbgpRWLockState {
	SceUID writeOwnerId;
	SceInt32 lockCount;
} DbgpRWLockState;

typedef struct DbgpLwCondRecord {
	DbgpSyncHeader header;
	char name[32];
	SceUInt32 attr;			/* 0x30 */
	SceUInt32 reserved;
	SceUID lwMutexId;		/* read from the lightweight mutex's work area */
	SceUInt32 numWaitThreads;	/* 0x3c */
	DbgpWaitingThread waiting[];	/* then a DbgpLwCondWork */
} DbgpLwCondRecord;

/* what follows a lightweight condition's waiting threads */
typedef struct DbgpLwCondWork {
	SceUInt32 work;			/* the user work areas */
	SceUInt32 lwMutexWork;
} DbgpLwCondWork;

/* _getSyncPrimitiveInfo's request and answer */
typedef struct DbgpSyncRequest {
	DbgpPrefix prefix;
	SceUID pid;
	int count;
	SceUInt32 types[];		/* DBGP_SYNC_* or ULOBJ_INFO_* */
} DbgpSyncRequest;

typedef struct DbgpSyncResponse {
	DbgpResponseHeader header;
	SceUID pid;
	SceUInt8 records[];
} DbgpSyncResponse;

_Static_assert(sizeof(DbgpSemaphoreRecord) == 0x4c, "DBGP semaphore record");
_Static_assert(sizeof(DbgpEventFlagRecord) == 0x48, "DBGP event flag record");
_Static_assert(sizeof(DbgpMutexRecord) == 0x4c, "DBGP mutex record");
_Static_assert(sizeof(DbgpLwMutexRecord) == 0x48, "DBGP lightweight mutex record");
_Static_assert(sizeof(DbgpMsgPipeRecord) == 0x4c, "DBGP message pipe record");
_Static_assert(sizeof(DbgpSimpleEventRecord) == 0x48, "DBGP simple event record");
_Static_assert(sizeof(DbgpCallbackRecord) == 0x50, "DBGP callback record");
_Static_assert(sizeof(DbgpCondRecord) == 0x44, "DBGP condition record");
_Static_assert(sizeof(DbgpTimerRecord) == 0x6c, "DBGP timer record");
_Static_assert(sizeof(DbgpRWLockRecord) == 0x44, "DBGP reader-writer lock record");
_Static_assert(sizeof(DbgpLwCondRecord) == 0x40, "DBGP lightweight condition record");
_Static_assert(sizeof(DbgpSyncResponse) == 0x20, "DBGP sync primitive response");

/* the kinds the host may ask for, and the libult ones of them */
static const int s_ultSyncTypes[] = {
	ULOBJ_INFO_SEMA, ULOBJ_INFO_MUTEX, ULOBJ_INFO_QUEUE_DATA_RESOURCE_POOL,
	ULOBJ_INFO_WAITING_QUEUE_RESOURCE_POOL, ULOBJ_INFO_QUEUE, ULOBJ_INFO_RWLOCK,
	ULOBJ_INFO_COND,
};

static const int s_syncTypes[] = {
	DBGP_SYNC_SEMAPHORE, DBGP_SYNC_EVENT_FLAG, DBGP_SYNC_MUTEX,
	DBGP_SYNC_LW_MUTEX, DBGP_SYNC_MSG_PIPE, DBGP_SYNC_SIMPLE_EVENT,
	DBGP_SYNC_CALLBACK, DBGP_SYNC_COND, DBGP_SYNC_LW_COND, DBGP_SYNC_TIMER,
	DBGP_SYNC_RW_LOCK, 0x40, 0x41, 0x70, 0x80,
	ULOBJ_INFO_SEMA, ULOBJ_INFO_MUTEX, ULOBJ_INFO_QUEUE_DATA_RESOURCE_POOL,
	ULOBJ_INFO_WAITING_QUEUE_RESOURCE_POOL, ULOBJ_INFO_QUEUE, ULOBJ_INFO_RWLOCK,
	ULOBJ_INFO_COND,
};

#define DBGP_SYNC_TYPE_NUM	(sizeof(s_syncTypes) / sizeof(s_syncTypes[0]))

int _getThreadInfo(DbgpProcCB *proc, DbgpThreadQuery *query,
		   DbgpThreadInfo *out, ThreadmgrThreadInfoForDebugger *infos);

_Static_assert(sizeof(DbgpThreadRequest) == 0x1c, "DBGP thread request");
_Static_assert(sizeof(DbgpThreadInfoResponse) == 0x24, "DBGP thread list");
_Static_assert(sizeof(DbgpThreadResponse) == 0x28, "DBGP thread response");
_Static_assert(sizeof(DbgpThreadNamesResponse) == 0x20, "DBGP thread names");
_Static_assert(sizeof(DbgpStackInfoResponse) == 0x20, "DBGP stack list");

int _getThreadInfo(DbgpProcCB *proc, DbgpThreadQuery *query,
		   DbgpThreadInfo *out, ThreadmgrThreadInfoForDebugger *infos)
{
	ThreadmgrThreadInfoForDebugger *info;
	ThreadmgrDebugRegisters regs[2];
	DbgpThreadInfoEx *ex;
	SceSize count;
	SceUID guid;
	int i, n, result;

	info = infos;
	memset(infos, 0, DBGP_THREAD_LIST_SIZE);
	result = SceThreadmgrForDriver_F2C9DC97(query->pid, 0, infos,
						DBGP_THREAD_LIST_SIZE, &count);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1108, "0x%08x\n", result);
		return result;
	}
	n = 0;
	for (i = 0; i < (int)count; i++, info++) {
		guid = scePUIDtoGUID(query->pid, info->threadId);
		if (guid < 0)
			continue;
		result = SceThreadmgrForDriver_5022689D(guid, regs);
		if (result == SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID)
			continue;
		if (result < 0) {
			out->pc = 0;
		} else if (info->userFrame != 0) {
			out->status |= DBGP_THREAD_STATUS_SYSCALL;
			out->pc = regs[1].pc;
		} else {
			out->pc = regs[0].pc;
		}
		if (procGetThreadDbgpStopReason(proc, &out->stopReason, guid,
						query->pid, 0) ==
		    SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID) {
			out->pc = 0;
			out->status = 0;
			continue;
		}
		out->size = query->type != DBGP_THREAD_TYPE_NORMAL ?
			    sizeof(DbgpThreadInfoEx) : sizeof(DbgpThreadInfo);
		out->thid = info->threadId;
		out->initAttr = info->initAttr;
		out->attr = info->attr;
		out->entry = info->entry;
		out->stackBase = info->userStackBase;
		out->stackSize = info->userStackSize;
		out->stackMaxUsed = info->userStackMaxUsed;
		out->argSize = info->argSize;
		out->argBlock = info->pArgBlock;
		out->initPriority = info->initPriority;
		out->currentPriority = info->currentPriority;
		out->initCpuAffinityMask = info->initCpuAffinityMask;
		out->cpuAffinityMask = info->cpuAffinityMask;
		out->lastCpuId = info->lastCpuId;
		out->runClocksHigh = info->runClocksHigh;
		out->runClocksLow = info->runClocksLow;
		out->exitStatus = info->exitStatus;
		out->intrPreemptCount = info->intrPreemptCount;
		out->threadPreemptCount = info->threadPreemptCount;
		out->threadReleaseCount = info->threadReleaseCount;
		out->changeCpuCount = info->changeCpuCount;
		out->valid = 1;
		out->timeout = info->timeout;
		if (proc->currentThread == guid) {
			out->waitType = 0;
			out->waitId = 0;
			out->status |= DBGP_THREAD_STATUS_STOPPED;
		} else {
			out->status |= info->status;
			out->waitType = info->waitType;
			out->waitId = info->waitId;
		}
		__builtin_memcpy(out->name, info->name, sizeof(out->name));
		if (query->type != DBGP_THREAD_TYPE_NORMAL) {
			ex = (DbgpThreadInfoEx *)out;
			if (info->status != SCE_KERNEL_THREAD_STATUS_WAITING)
				goto next;
			switch (info->waitType) {
			case SCE_KERNEL_WAITTYPE_DELAY:
			case SCE_KERNEL_WAITTYPE_SIGNAL:
			case SCE_KERNEL_WAITTYPE_EVENTFLAG:
			case SCE_KERNEL_WAITTYPE_DELAY_CB:
			case SCE_KERNEL_WAITTYPE_SIGNAL_CB:
			case SCE_KERNEL_WAITTYPE_EVENTFLAG_CB:
				ex->waitDetail[0] = info->waitDetail[0];
				ex->waitDetail[1] = info->waitDetail[1];
				break;
			case SCE_KERNEL_WAITTYPE_SEMAPHORE:
			case SCE_KERNEL_WAITTYPE_RW_LOCK:
			case SCE_KERNEL_WAITTYPE_EVENT:
			case WAIT_TYPE_EXCEPTION:
			case SCE_KERNEL_WAITTYPE_SEMAPHORE_CB:
			case SCE_KERNEL_WAITTYPE_RW_LOCK_CB:
			case SCE_KERNEL_WAITTYPE_EVENT_CB:
			case SCE_KERNEL_WAITTYPE_CB | WAIT_TYPE_EXCEPTION:
				ex->waitDetail[0] = info->waitDetail[0];
				break;
			case SCE_KERNEL_WAITTYPE_MUTEX:
			case SCE_KERNEL_WAITTYPE_LW_MUTEX:
			case SCE_KERNEL_WAITTYPE_MUTEX_CB:
			case SCE_KERNEL_WAITTYPE_LW_MUTEX_CB:
				ex->waitDetail[0] = info->waitDetail[0];
				ex->waitDetail[1] = 0;
				break;
			case SCE_KERNEL_WAITTYPE_COND_SIGNAL:
			case SCE_KERNEL_WAITTYPE_COND_MUTEX:
			case SCE_KERNEL_WAITTYPE_LW_COND_SIGNAL:
			case SCE_KERNEL_WAITTYPE_LW_COND_LW_MUTEX:
			case SCE_KERNEL_WAITTYPE_COND_SIGNAL_CB:
			case SCE_KERNEL_WAITTYPE_COND_MUTEX_CB:
			case SCE_KERNEL_WAITTYPE_LW_COND_SIGNAL_CB:
			case SCE_KERNEL_WAITTYPE_LW_COND_LW_MUTEX_CB:
				ex->waitDetail[0] = info->waitDetail[2];
				ex->waitDetail[8] = info->waitDetail[1];
				break;
			case SCE_KERNEL_WAITTYPE_MP_EVENTS:
			case SCE_KERNEL_WAITTYPE_MP_EVENTS_CB:
				ex->waitDetail[0] = info->waitDetail[1];
				ex->waitDetail[1] = info->waitDetail[2];
				ex->waitDetail[2] = info->waitDetail[3];
				break;
			case SCE_KERNEL_WAITTYPE_MSG_PIPE:
			case SCE_KERNEL_WAITTYPE_MSG_PIPE_CB:
				ex->waitDetail[0] = info->waitDetail[0];
				ex->waitDetail[1] = info->waitDetail[1];
				ex->waitDetail[2] = info->waitDetail[2];
				break;
			}
		} else if (info->status == SCE_KERNEL_THREAD_STATUS_WAITING) {
			switch (info->waitType) {
			case SCE_KERNEL_WAITTYPE_DELAY:
			case SCE_KERNEL_WAITTYPE_EVENTFLAG:
			case SCE_KERNEL_WAITTYPE_DELAY_CB:
			case SCE_KERNEL_WAITTYPE_EVENTFLAG_CB:
				out->waitDetail[0] = info->waitDetail[0];
				out->waitDetail[1] = info->waitDetail[1];
				break;
			case SCE_KERNEL_WAITTYPE_MUTEX:
			case SCE_KERNEL_WAITTYPE_MUTEX_CB:
				out->waitDetail[0] = info->waitDetail[0];
				break;
			default:
				out->waitDetail[0] = 0;
				out->waitDetail[1] = 0;
				break;
			}
		}
next:
		out = (DbgpThreadInfo *)((SceUInt8 *)out + out->size);
		n++;
	}
	query->count = n;
	return 0;
}

static int _getSemaphoreInfo(SceUID pid, void *out, void *work, SceSize workSize,
			     SceSize *size)
{
	ThreadmgrWaitingThread *list =
		(ThreadmgrWaitingThread *)((SceUInt8 *)work + DBGP_SYNC_LIST_SIZE);
	SceKernelSemaInfo *info = work;
	DbgpSemaphoreRecord *record = out;
	SceSize count = 0, got, listSize;
	SceUInt32 i, j;
	int result;

	memset(work, 0, workSize);
	*size = 0;
	result = SceThreadmgrForDriver_477F7C8A(pid, 3, info, DBGP_SYNC_LIST_SIZE,
						&count);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1316, "0x%08x\n", result);
		return result;
	}
	for (i = 0; i < count; i++, info++) {
		record->header.kind = DBGP_SYNC_SEMAPHORE;
		if (info->numWaitThreads != 0) {
			record->header.size = sizeof(*record) - 8 +
				info->numWaitThreads * sizeof(DbgpWaitingThread);
			*size += sizeof(*record) +
				 info->numWaitThreads * sizeof(DbgpWaitingThread);
		} else {
			record->header.size = sizeof(*record) - 8;
			*size += sizeof(*record);
		}
		record->header.type = DBGP_SYNC_SEMAPHORE;
		record->header.uid = info->semaId;
		record->pid = pid;
		record->attr = info->attr;
		record->reserved = 0;
		record->initCount = info->initCount;
		record->currentCount = info->currentCount;
		record->maxCount = info->maxCount;
		record->numWaitThreads = info->numWaitThreads;
		__builtin_memcpy(record->name, info->name, sizeof(record->name));
		if (info->numWaitThreads != 0) {
			listSize = info->numWaitThreads * sizeof(*list);
			if (listSize >= DBGP_SYNC_WAIT_SIZE)
				listSize = DBGP_SYNC_WAIT_SIZE;
			memset(list, 0, listSize);
			result = sceKernelGetWaitingThreadList(
				scePUIDtoGUID(pid, info->semaId), list, listSize,
				&got);
			if (result < 0) {
				SCE_PRINTF_WITH_INFO(15, 1375, "0x%08x\n", result);
				return result;
			}
			for (j = 0; j < got; j++) {
				record->waiting[j].pid = list[j].processId;
				record->waiting[j].thid = list[j].userThreadId;
			}
			for (; j < info->numWaitThreads; j++) {
				record->waiting[j].pid = 0;
				record->waiting[j].thid = 0;
			}
		}
		record = (DbgpSemaphoreRecord *)&record->waiting[info->numWaitThreads];
	}
	return 0;
}

static int _getEventFlagInfo(SceUID pid, void *out, void *work, SceSize workSize,
			     SceSize *size)
{
	ThreadmgrWaitingThread *list =
		(ThreadmgrWaitingThread *)((SceUInt8 *)work + DBGP_SYNC_LIST_SIZE);
	SceKernelEventFlagInfo *info = work;
	DbgpEventFlagRecord *record = out;
	SceSize count = 0, got, listSize;
	SceUInt32 i, j;
	int result;

	memset(work, 0, workSize);
	*size = 0;
	result = SceThreadmgrForDriver_0EB0ABF0(pid, 3, info, DBGP_SYNC_LIST_SIZE,
						&count);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1426, "0x%08x\n", result);
		return result;
	}
	for (i = 0; i < count; i++, info++) {
		record->header.kind = DBGP_SYNC_EVENT_FLAG;
		if (info->numWaitThreads != 0) {
			record->header.size = sizeof(*record) - 8 +
				info->numWaitThreads * sizeof(DbgpWaitingThread);
			*size += sizeof(*record) +
				 info->numWaitThreads * sizeof(DbgpWaitingThread);
		} else {
			record->header.size = sizeof(*record) - 8;
			*size += sizeof(*record);
		}
		record->header.type = DBGP_SYNC_EVENT_FLAG;
		record->header.uid = info->evfId;
		record->pid = pid;
		record->attr = info->attr;
		record->reserved = 0;
		record->initPattern = info->initPattern;
		record->currentPattern = info->currentPattern;
		record->numWaitThreads = info->numWaitThreads;
		__builtin_memcpy(record->name, info->name, sizeof(record->name));
		if (info->numWaitThreads != 0) {
			listSize = info->numWaitThreads * sizeof(*list);
			if (listSize >= DBGP_SYNC_WAIT_SIZE)
				listSize = DBGP_SYNC_WAIT_SIZE;
			memset(list, 0, listSize);
			result = sceKernelGetWaitingThreadList(
				scePUIDtoGUID(pid, info->evfId), list, listSize,
				&got);
			if (result < 0) {
				SCE_PRINTF_WITH_INFO(15, 1485, "0x%08x\n", result);
				return result;
			}
			for (j = 0; j < got; j++) {
				record->waiting[j].pid = list[j].processId;
				record->waiting[j].thid = list[j].userThreadId;
			}
			for (; j < info->numWaitThreads; j++) {
				record->waiting[j].pid = 0;
				record->waiting[j].thid = 0;
			}
		}
		record = (DbgpEventFlagRecord *)&record->waiting[info->numWaitThreads];
	}
	return 0;
}

static int _getMutexInfo(SceUID pid, void *out, void *work, SceSize workSize,
			 SceSize *size)
{
	ThreadmgrWaitingThread *list =
		(ThreadmgrWaitingThread *)((SceUInt8 *)work + DBGP_SYNC_LIST_SIZE);
	SceKernelMutexInfo *info = work;
	DbgpMutexRecord *record = out;
	SceInt32 *ceilingPriority;
	SceSize count = 0, got, listSize;
	SceUInt32 i, j;
	int result;

	memset(work, 0, workSize);
	*size = 0;
	result = sceKernelGetMutexInfoAllProc(pid, 3, info, DBGP_SYNC_LIST_SIZE,
					      &count);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1538, "0x%08x\n", result);
		return result;
	}
	for (i = 0; i < count; i++, info++) {
		record->header.kind = DBGP_SYNC_MUTEX;
		if (info->numWaitThreads != 0) {
			record->header.size = sizeof(*record) - 8 + sizeof(SceInt32) +
				info->numWaitThreads * sizeof(DbgpWaitingThread);
			*size += sizeof(*record) + sizeof(SceInt32) +
				 info->numWaitThreads * sizeof(DbgpWaitingThread);
		} else {
			record->header.size = sizeof(*record) - 8 + sizeof(SceInt32);
			*size += sizeof(*record) + sizeof(SceInt32);
		}
		record->header.type = DBGP_SYNC_MUTEX;
		record->header.uid = info->mutexId;
		record->pid = pid;
		record->attr = info->attr;
		record->reserved = 0;
		record->initCount = info->initCount;
		record->currentCount = info->currentCount;
		record->currentOwnerId = info->currentOwnerId;
		record->numWaitThreads = info->numWaitThreads;
		__builtin_memcpy(record->name, info->name, sizeof(record->name));
		if (info->numWaitThreads != 0) {
			listSize = info->numWaitThreads * sizeof(*list);
			if (listSize >= DBGP_SYNC_WAIT_SIZE)
				listSize = DBGP_SYNC_WAIT_SIZE;
			memset(list, 0, listSize);
			result = sceKernelGetWaitingThreadList(
				scePUIDtoGUID(pid, info->mutexId), list, listSize,
				&got);
			if (result < 0) {
				SCE_PRINTF_WITH_INFO(15, 1600, "0x%08x\n", result);
				return result;
			}
			for (j = 0; j < got; j++) {
				record->waiting[j].pid = list[j].processId;
				record->waiting[j].thid = list[j].userThreadId;
			}
			for (; j < info->numWaitThreads; j++) {
				record->waiting[j].pid = 0;
				record->waiting[j].thid = 0;
			}
		}
		ceilingPriority = (SceInt32 *)&record->waiting[info->numWaitThreads];
		*ceilingPriority = info->ceilingPriority;
		record = (DbgpMutexRecord *)(ceilingPriority + 1);
	}
	return 0;
}

static int _getLwMutexInfo(SceUID pid, void *out, void *work, SceSize workSize,
			   SceSize *size)
{
	ThreadmgrWaitingThread *list =
		(ThreadmgrWaitingThread *)((SceUInt8 *)work + DBGP_SYNC_LIST_SIZE);
	SceKernelLwMutexInfo *info = work;
	DbgpLwMutexRecord *record = out;
	SceSize count = 0, got, listSize;
	SceUInt32 i, j;
	int result;

	memset(work, 0, workSize);
	*size = 0;
	result = sceKernelGetLwMutexInfoAllProc(pid, 3, info, DBGP_SYNC_LIST_SIZE,
						&count);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1654, "0x%08x\n", result);
		return result;
	}
	for (i = 0; i < count; i++, info++) {
		record->header.kind = DBGP_SYNC_LW_MUTEX;
		record->header.size = sizeof(*record) - 8 +
			info->numWaitThreads * sizeof(SceUID);
		*size += sizeof(*record) + info->numWaitThreads * sizeof(SceUID);
		record->header.type = DBGP_SYNC_LW_MUTEX;
		record->header.uid = info->uid;
		record->attr = info->attr;
		record->work = (SceUInt32)info->pWork;
		record->initCount = info->initCount;
		record->currentCount = info->currentCount;
		record->currentOwnerId = info->currentOwnerId;
		record->numWaitThreads = info->numWaitThreads;
		__builtin_memcpy(record->name, info->name, sizeof(record->name));
		if (info->numWaitThreads != 0) {
			listSize = info->numWaitThreads * sizeof(*list);
			if (listSize >= DBGP_SYNC_WAIT_SIZE)
				listSize = DBGP_SYNC_WAIT_SIZE;
			memset(list, 0, listSize);
			result = sceKernelGetWaitingThreadList(
				scePUIDtoGUID(pid, info->uid), list, listSize, &got);
			if (result < 0) {
				SCE_PRINTF_WITH_INFO(15, 1708, "0x%08x\n", result);
				return result;
			}
			for (j = 0; j < got; j++)
				record->waiting[j] = list[j].userThreadId;
			for (; j < info->numWaitThreads; j++)
				record->waiting[j] = 0;
		}
		record = (DbgpLwMutexRecord *)&record->waiting[info->numWaitThreads];
	}
	return 0;
}

static int _getMsgPipeInfo(SceUID pid, void *out, void *work, SceSize workSize,
			   SceSize *size)
{
	ThreadmgrWaitingThread *list =
		(ThreadmgrWaitingThread *)((SceUInt8 *)work + DBGP_SYNC_LIST_SIZE);
	SceKernelEventInfo *event = (SceKernelEventInfo *)list;
	SceKernelMsgPipeInfo *info = work;
	DbgpMsgPipeRecord *record = out;
	DbgpMsgPipeEvent *eventState;
	SceSize count = 0, got, listSize;
	SceUInt32 i, j, waiting;
	int result;

	memset(work, 0, workSize);
	*size = 0;
	result = SceThreadmgrForDriver_E9E50096(pid, 3, info, DBGP_SYNC_LIST_SIZE,
						&count);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1762, "0x%08x\n", result);
		return result;
	}
	for (i = 0; i < count; i++, info++) {
		record->header.kind = DBGP_SYNC_MSG_PIPE;
		if (info->numSendWaitThreads != 0 ||
		    info->numReceiveWaitThreads != 0)
			record->header.size = sizeof(*record) - 8 +
				sizeof(DbgpMsgPipeEvent) +
				(info->numSendWaitThreads +
				 info->numReceiveWaitThreads) *
				sizeof(DbgpWaitingThread);
		else
			record->header.size = sizeof(*record) - 8 +
				sizeof(DbgpMsgPipeEvent);
		*size += record->header.size + 8;
		record->header.type = DBGP_SYNC_MSG_PIPE;
		record->header.uid = info->msgPipeId;
		record->pid = pid;
		record->attr = info->attr;
		record->reserved = 0;
		record->bufferSize = info->bufferSize;
		record->freeSize = info->freeSize;
		record->numSendWaitThreads = info->numSendWaitThreads;
		record->numReceiveWaitThreads = info->numReceiveWaitThreads;
		__builtin_memcpy(record->name, info->name, sizeof(record->name));
		waiting = info->numSendWaitThreads;
		if (waiting == 0)
			waiting = info->numReceiveWaitThreads;
		if (waiting == 0) {
			eventState = (DbgpMsgPipeEvent *)record->waiting;
		} else {
			listSize = waiting * sizeof(*list);
			if (listSize >= DBGP_SYNC_WAIT_SIZE)
				listSize = DBGP_SYNC_WAIT_SIZE;
			memset(list, 0, listSize);
			result = sceKernelGetWaitingThreadList(
				scePUIDtoGUID(pid, info->msgPipeId), list, listSize,
				&got);
			if (result < 0) {
				SCE_PRINTF_WITH_INFO(15, 1837, "0x%08x\n", result);
				return result;
			}
			for (j = 0; j < got; j++) {
				record->waiting[j].pid = list[j].processId;
				record->waiting[j].thid = list[j].userThreadId;
			}
			for (; j < waiting; j++) {
				record->waiting[j].pid = 0;
				record->waiting[j].thid = 0;
			}
			eventState = (DbgpMsgPipeEvent *)&record->waiting[waiting];
		}
		memset(event, 0, sizeof(*event));
		event->size = sizeof(*event);
		result = SceThreadmgrForDriver_CBA7FAAA(
			scePUIDtoGUID(pid, info->msgPipeId), event);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 1863, "0x%08x\n", result);
			eventState->eventPattern = 0;
			eventState->userDataLow = 0;
		} else {
			eventState->eventPattern = event->eventPattern;
			eventState->userDataLow = (SceUInt32)event->userData;
			eventState->userDataHigh = (SceUInt32)(event->userData >> 32);
		}
		record = (DbgpMsgPipeRecord *)(eventState + 1);
	}
	return 0;
}

static int _getSimpleEventInfo(SceUID pid, void *out, void *work,
			       SceSize workSize, SceSize *size)
{
	ThreadmgrWaitingThread *list =
		(ThreadmgrWaitingThread *)((SceUInt8 *)work + DBGP_SYNC_LIST_SIZE);
	SceKernelEventInfo *info = work;
	DbgpSimpleEventRecord *record = out;
	SceSize count = 0, got, listSize;
	SceUInt32 i, j;
	int result;

	memset(work, 0, workSize);
	*size = 0;
	result = SceThreadmgrForDriver_E0C7BCBB(pid, 3, info, DBGP_SYNC_LIST_SIZE,
						&count);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1911, "0x%08x\n", result);
		return result;
	}
	for (i = 0; i < count; i++, info++) {
		record->header.kind = DBGP_SYNC_SIMPLE_EVENT;
		if ((SceInt32)info->numWaitThreads > 0)
			record->header.size = sizeof(*record) - 8 +
				info->numWaitThreads * sizeof(DbgpWaitingThread);
		else
			record->header.size = sizeof(*record) - 8;
		*size += record->header.size + 8;
		record->header.type = DBGP_SYNC_SIMPLE_EVENT;
		record->header.uid = info->eventId;
		record->pid = pid;
		record->attr = info->attr;
		record->eventPattern = info->eventPattern;
		record->userDataLow = (SceUInt32)info->userData;
		record->userDataHigh = (SceUInt32)(info->userData >> 32);
		record->numWaitThreads = info->numWaitThreads;
		__builtin_memcpy(record->name, info->name, sizeof(record->name));
		if ((SceInt32)info->numWaitThreads <= 0) {
			record = (DbgpSimpleEventRecord *)record->waiting;
			continue;
		}
		listSize = info->numWaitThreads * sizeof(*list);
		if (listSize >= DBGP_SYNC_WAIT_SIZE)
			listSize = DBGP_SYNC_WAIT_SIZE;
		memset(list, 0, listSize);
		result = sceKernelGetWaitingThreadList(
			scePUIDtoGUID(pid, info->eventId), list, listSize, &got);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 1969, "0x%08x\n", result);
			return result;
		}
		for (j = 0; j < got; j++) {
			record->waiting[j].pid = list[j].processId;
			record->waiting[j].thid = list[j].userThreadId;
		}
		for (; j < info->numWaitThreads; j++) {
			record->waiting[j].pid = 0;
			record->waiting[j].thid = 0;
		}
		record = (DbgpSimpleEventRecord *)&record->waiting[j];
	}
	return 0;
}

static int _getCallbackInfo(SceUID pid, void *out, void *work, SceSize workSize,
			    SceSize *size)
{
	SceKernelCallbackInfo *info = work;
	DbgpCallbackRecord *record = out;
	SceSize count = 0;
	SceUInt32 i;
	int result;

	memset(work, 0, workSize);
	*size = 0;
	result = SceThreadmgrForDriver_FF8DA217(pid, 3, info, DBGP_SYNC_LIST_SIZE,
						&count);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 2015, "0x%08x\n", result);
		return result;
	}
	for (i = 0; i < count; i++, info++, record++) {
		record->header.kind = DBGP_SYNC_CALLBACK;
		record->header.size = sizeof(*record) - 8;
		*size += sizeof(*record);
		record->header.type = DBGP_SYNC_CALLBACK;
		record->header.uid = info->callbackId;
		record->pid = pid;
		record->threadId = info->threadId;
		record->attr = info->attr;
		record->reserved = 0;
		record->callbackFunc = (SceUInt32)info->callbackFunc;
		record->common = (SceUInt32)info->pCommon;
		record->notifyCount = info->notifyCount;
		record->notifyArg = info->notifyArg;
		__builtin_memcpy(record->name, info->name, sizeof(record->name));
	}
	return 0;
}

static int _getCondInfo(SceUID pid, void *out, void *work, SceSize workSize,
			SceSize *size)
{
	ThreadmgrWaitingThread *list =
		(ThreadmgrWaitingThread *)((SceUInt8 *)work + DBGP_SYNC_LIST_SIZE);
	SceKernelCondInfo *info = work;
	DbgpCondRecord *record = out;
	SceSize count = 0, got, listSize;
	SceUInt32 i, j;
	int result;

	memset(work, 0, workSize);
	*size = 0;
	result = SceThreadmgrForDriver_CA704239(pid, 3, info, DBGP_SYNC_LIST_SIZE,
						&count);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 2082, "0x%08x\n", result);
		return result;
	}
	for (i = 0; i < count; i++, info++) {
		record->header.kind = DBGP_SYNC_COND;
		if (info->numWaitThreads != 0) {
			record->header.size = sizeof(*record) - 8 +
				info->numWaitThreads * sizeof(DbgpWaitingThread);
			*size += sizeof(*record) +
				 info->numWaitThreads * sizeof(DbgpWaitingThread);
		} else {
			record->header.size = sizeof(*record) - 8;
			*size += sizeof(*record);
		}
		record->header.type = DBGP_SYNC_COND;
		record->header.uid = info->condId;
		record->pid = pid;
		record->attr = info->attr;
		record->reserved = 0;
		record->mutexId = info->mutexId;
		record->numWaitThreads = info->numWaitThreads;
		__builtin_memcpy(record->name, info->name, sizeof(record->name));
		if (info->numWaitThreads != 0) {
			listSize = info->numWaitThreads * sizeof(*list);
			if (listSize >= DBGP_SYNC_WAIT_SIZE)
				listSize = DBGP_SYNC_WAIT_SIZE;
			memset(list, 0, listSize);
			result = sceKernelGetWaitingThreadList(
				scePUIDtoGUID(pid, info->condId), list, listSize,
				&got);
			if (result < 0) {
				SCE_PRINTF_WITH_INFO(15, 2141, "0x%08x\n", result);
				return result;
			}
			for (j = 0; j < got; j++) {
				record->waiting[j].pid = list[j].processId;
				record->waiting[j].thid = list[j].userThreadId;
			}
			for (; j < info->numWaitThreads; j++) {
				record->waiting[j].pid = 0;
				record->waiting[j].thid = 0;
			}
		}
		record = (DbgpCondRecord *)&record->waiting[info->numWaitThreads];
	}
	return 0;
}

static int _getTimerInfo(SceUID pid, void *out, void *work, SceSize workSize,
			 SceSize *size)
{
	ThreadmgrWaitingThread *list =
		(ThreadmgrWaitingThread *)((SceUInt8 *)work + DBGP_SYNC_LIST_SIZE);
	SceKernelEventInfo *event = (SceKernelEventInfo *)list;
	SceKernelTimerInfo *info = work;
	DbgpTimerRecord *record = out;
	DbgpMsgPipeEvent *eventState;
	SceSize count = 0, got, listSize;
	SceUInt32 i, j, waiting;
	int result;

	memset(work, 0, workSize);
	*size = 0;
	result = SceThreadmgrForDriver_C327CA7C(pid, 3, info, DBGP_SYNC_LIST_SIZE,
						&count);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 2328, "0x%08x\n", result);
		return result;
	}
	for (i = 0; i < count; i++, info++) {
		waiting = info->numWaitThreads;
		record->header.kind = DBGP_SYNC_TIMER;
		record->header.size = sizeof(*record) - 8 +
			sizeof(DbgpMsgPipeEvent) +
			waiting * sizeof(DbgpWaitingThread);
		*size += sizeof(*record) + sizeof(DbgpMsgPipeEvent) +
			 waiting * sizeof(DbgpWaitingThread);
		record->header.type = DBGP_SYNC_TIMER;
		record->header.uid = info->timerId;
		record->pid = pid;
		record->attr = info->attr;
		record->reserved = 0;
		record->fActive = info->fActive;
		record->type = info->type;
		record->fRepeat = info->fRepeat;
		record->numWaitThreads = waiting;
		record->baseTime.low = info->baseTime.u.low;
		record->baseTime.hi = info->baseTime.u.hi;
		record->currentTime.low = info->currentTime.u.low;
		record->currentTime.hi = info->currentTime.u.hi;
		record->schedule.low = info->schedule.u.low;
		record->schedule.hi = info->schedule.u.hi;
		record->interval.low = info->interval.u.low;
		record->interval.hi = info->interval.u.hi;
		__builtin_memcpy(record->name, info->name, sizeof(record->name));
		eventState = (DbgpMsgPipeEvent *)record->waiting;
		if (waiting != 0) {
			listSize = waiting * sizeof(*list);
			if (listSize >= DBGP_SYNC_WAIT_SIZE)
				listSize = DBGP_SYNC_WAIT_SIZE;
			memset(list, 0, listSize);
			result = sceKernelGetWaitingThreadList(
				scePUIDtoGUID(pid, info->timerId), list, listSize,
				&got);
			if (result < 0) {
				SCE_PRINTF_WITH_INFO(15, 2390, "0x%08x\n", result);
				return result;
			}
			for (j = 0; j < got; j++) {
				record->waiting[j].pid = list[j].processId;
				record->waiting[j].thid = list[j].userThreadId;
			}
			for (; j < waiting; j++) {
				record->waiting[j].pid = 0;
				record->waiting[j].thid = 0;
			}
			eventState = (DbgpMsgPipeEvent *)&record->waiting[waiting];
		}
		memset(event, 0, sizeof(*event));
		event->size = sizeof(*event);
		result = SceThreadmgrForDriver_CBA7FAAA(
			scePUIDtoGUID(pid, info->timerId), event);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 2416, "0x%08x\n", result);
			eventState->eventPattern = 0;
			eventState->userDataLow = 0;
		} else {
			eventState->eventPattern = event->eventPattern;
			eventState->userDataLow = (SceUInt32)event->userData;
			eventState->userDataHigh = (SceUInt32)(event->userData >> 32);
		}
		record = (DbgpTimerRecord *)(eventState + 1);
	}
	return 0;
}

static int _getRWLockInfo(SceUID pid, void *out, void *work, SceSize workSize,
			  SceSize *size)
{
	ThreadmgrWaitingThread *list =
		(ThreadmgrWaitingThread *)((SceUInt8 *)work + DBGP_SYNC_LIST_SIZE);
	SceKernelRWLockInfo *info = work;
	DbgpRWLockRecord *record = out;
	DbgpRWLockState *state;
	SceSize count = 0, got, listSize;
	SceUInt32 i, j, waiting;
	int result;

	memset(work, 0, workSize);
	*size = 0;
	result = sceKernelGetRWLockInfoAllProc(pid, 3, info, DBGP_SYNC_LIST_SIZE,
					       &count);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 2473, "0x%08x\n", result);
		return result;
	}
	for (i = 0; i < count; i++, info++) {
		waiting = info->numReadWaitThreads + info->numWriteWaitThreads;
		record->header.kind = DBGP_SYNC_RW_LOCK;
		record->header.size = sizeof(*record) - 8 + sizeof(*state) +
			waiting * sizeof(DbgpWaitingThread);
		*size += sizeof(*record) + sizeof(*state) +
			 waiting * sizeof(DbgpWaitingThread);
		record->header.type = DBGP_SYNC_RW_LOCK;
		record->header.uid = info->rwLockId;
		record->pid = pid;
		record->numReadWaitThreads = info->numReadWaitThreads;
		record->numWriteWaitThreads = info->numWriteWaitThreads;
		record->attr = info->attr;
		record->reserved = 0;
		__builtin_memcpy(record->name, info->name, sizeof(record->name));
		state = (DbgpRWLockState *)record->waiting;
		if (waiting != 0) {
			listSize = waiting * sizeof(*list);
			if (listSize >= DBGP_SYNC_WAIT_SIZE)
				listSize = DBGP_SYNC_WAIT_SIZE;
			memset(list, 0, listSize);
			result = sceKernelGetWaitingThreadList(
				scePUIDtoGUID(pid, info->rwLockId), list, listSize,
				&got);
			if (result < 0) {
				SCE_PRINTF_WITH_INFO(15, 2530, "0x%08x\n", result);
				return result;
			}
			for (j = 0; j < got; j++) {
				record->waiting[j].pid = list[j].processId;
				record->waiting[j].thid = list[j].userThreadId;
			}
			for (; j < waiting; j++) {
				record->waiting[j].pid = 0;
				record->waiting[j].thid = 0;
			}
			state = (DbgpRWLockState *)&record->waiting[waiting];
		}
		state->writeOwnerId = info->writeOwnerId;
		state->lockCount = info->lockCount;
		record = (DbgpRWLockRecord *)(state + 1);
	}
	return 0;
}

static int _getLwCondInfo(SceUID pid, void *out, void *work, SceSize workSize,
			  SceSize *size)
{
	ThreadmgrWaitingThread *list =
		(ThreadmgrWaitingThread *)((SceUInt8 *)work + DBGP_SYNC_LIST_SIZE);
	SceKernelLwCondInfo *info = work;
	DbgpLwCondRecord *record = out;
	DbgpLwCondWork *state;
	SceSize count = 0, got, listSize;
	SceUID lwMutexId;
	SceUInt32 i, j;
	int result;

	memset(work, 0, workSize);
	*size = 0;
	result = SceThreadmgrForDriver_09346D1A(pid, 3, info, DBGP_SYNC_LIST_SIZE,
						&count);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 2197, "0x%08x\n", result);
		return result;
	}
	for (i = 0; i < count; i++, info++) {
		record->header.kind = DBGP_SYNC_LW_COND;
		record->header.size = sizeof(*record) - 8 + sizeof(*state) +
			info->numWaitThreads * sizeof(DbgpWaitingThread);
		*size += sizeof(*record) + sizeof(*state) +
			 info->numWaitThreads * sizeof(DbgpWaitingThread);
		record->header.type = DBGP_SYNC_LW_COND;
		record->header.uid = info->uid;
		record->attr = info->attr;
		record->reserved = 0;
		lwMutexId = 0;
		result = sceKernelCopyFromUserProc(pid, &lwMutexId,
			&((ThreadmgrLwMutexWork *)info->pLwMutex)->uid,
			sizeof(lwMutexId));
		if (result >= 0)
			result = lwMutexId;
		record->lwMutexId = result;
		record->numWaitThreads = info->numWaitThreads;
		__builtin_memcpy(record->name, info->name, sizeof(record->name));
		state = (DbgpLwCondWork *)record->waiting;
		if (info->numWaitThreads != 0) {
			listSize = info->numWaitThreads * sizeof(*list);
			if (listSize >= DBGP_SYNC_WAIT_SIZE)
				listSize = DBGP_SYNC_WAIT_SIZE;
			memset(list, 0, listSize);
			result = sceKernelGetWaitingThreadList(
				scePUIDtoGUID(pid, info->uid), list, listSize, &got);
			if (result < 0) {
				SCE_PRINTF_WITH_INFO(15, 2266, "0x%08x\n", result);
				return result;
			}
			for (j = 0; j < got; j++) {
				record->waiting[j].pid = list[j].processId;
				record->waiting[j].thid = list[j].userThreadId;
			}
			for (; j < info->numWaitThreads; j++) {
				record->waiting[j].pid = 0;
				record->waiting[j].thid = 0;
			}
			state = (DbgpLwCondWork *)&record->waiting[j];
		}
		state->work = (SceUInt32)info->pWork;
		state->lwMutexWork = (SceUInt32)info->pLwMutex;
		record = (DbgpLwCondRecord *)(state + 1);
	}
	return 0;
}

int _getThreadInfoNormal(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpRequestHeader *request = (const DbgpRequestHeader *)packet;
	DbgpThreadInfoResponse *response;
	ThreadmgrThreadInfoForDebugger *work;
	DbgpThreadQuery query;
	SceUID pid;
	int result, ret;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->pid;
	if (checkInvalidProcess(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20011);
		return (int)0x80080001;
	}
	if (checkAttachedProcess(proc, pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20051);
		return (int)0x80080001;
	}
	result = checkIllegalProcessStatus(pid);
	if (result != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20050);
		return (int)0x80080001;
	}
	work = _dbgpAllocWorkBuf(proc, DBGP_THREAD_LIST_SIZE);
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	response = _dbgpAllocSendBuf(proc, sizeof(*response) +
				   DBGP_THREAD_MAX * sizeof(DbgpThreadInfo));
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, sizeof(*response) +
	       DBGP_THREAD_MAX * sizeof(DbgpThreadInfo));
	query.type = DBGP_THREAD_TYPE_NORMAL;
	query.count = 0;
	query.pid = pid;
	ret = _getThreadInfo(proc, &query, response->info, work);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 1056, "0x%08x\n", ret);
	response->pid = pid;
	response->count = query.count;
	_dbgpSetSendSize(proc, sizeof(*response) +
			 query.count * sizeof(DbgpThreadInfo));
	dbgpFillResponseHeader(proc, packet->group, 1, 0, 0);
	return result;
}

int _getThreadInfoEx(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpRequestHeader *request = (const DbgpRequestHeader *)packet;
	DbgpThreadInfoResponse *response;
	ThreadmgrThreadInfoForDebugger *work;
	DbgpThreadQuery query;
	SceUID pid;
	int result, ret;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->pid;
	if (checkInvalidProcess(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20011);
		return (int)0x80080001;
	}
	if (checkAttachedProcess(proc, pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20051);
		return (int)0x80080001;
	}
	result = checkIllegalProcessStatus(pid);
	if (result != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20050);
		return (int)0x80080001;
	}
	work = _dbgpAllocWorkBuf(proc, DBGP_THREAD_LIST_SIZE);
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	response = _dbgpAllocSendBuf(proc, sizeof(*response) +
				   DBGP_THREAD_MAX * sizeof(DbgpThreadInfoEx));
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, sizeof(*response) +
	       DBGP_THREAD_MAX * sizeof(DbgpThreadInfoEx));
	query.count = 0;
	query.pid = pid;
	query.type = DBGP_THREAD_TYPE_EX;
	ret = _getThreadInfo(proc, &query, response->info, work);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 1074, "0x%08x\n", ret);
	response->pid = pid;
	response->count = query.count;
	_dbgpSetSendSize(proc, sizeof(*response) +
			 query.count * sizeof(DbgpThreadInfoEx));
	dbgpFillResponseHeader(proc, packet->group, 3, 0, 0);
	return result;
}

int _getSyncPrimitiveInfo(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpSyncRequest *request = (const DbgpSyncRequest *)packet;
	int counts[DBGP_SYNC_TYPE_NUM], types[DBGP_SYNC_TYPE_NUM];
	SceSize workSize, sendSize, used, size;
	DbgpSyncResponse *response;
	int ultAllocated, result, num, i, j;
	SceUInt8 *out;
	void *work;
	SceUID pid;

	if (_dbgpGetRecvSize(proc) < sizeof(*request) + sizeof(request->types[0])) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->pid;
	if (checkInvalidProcess(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20011);
		return (int)0x80080001;
	}
	if (checkAttachedProcess(proc, pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20051);
		return (int)0x80080001;
	}
	result = checkIllegalProcessStatus(pid);
	if (result != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20050);
		return (int)0x80080001;
	}

	/* the kinds asked for, each once, in s_syncTypes's order */
	memset(counts, 0, sizeof(counts));
	for (i = 0; i < request->count; i++) {
		for (j = 0; j < (int)DBGP_SYNC_TYPE_NUM; j++) {
			if (s_syncTypes[j] == (int)request->types[i])
				break;
		}
		if (j == DBGP_SYNC_TYPE_NUM) {
			dbgpSendError(proc, packet->group, packet->type + 1,
				      packet->code, 7);
			return (int)0x80080001;
		}
		counts[j]++;
	}
	num = 0;
	for (j = 0; j < (int)DBGP_SYNC_TYPE_NUM; j++) {
		if (counts[j] != 0)
			types[num++] = s_syncTypes[j];
	}

	/* room for the libult objects too, else they go only where they fit */
	workSize = _getUltSyncObjInfoSize(pid, num, types) + DBGP_SYNC_LIST_SIZE +
		   DBGP_SYNC_WAIT_SIZE;
	work = _dbgpAllocWorkBuf(proc, workSize);
	if (work != NULL) {
		ultAllocated = 1;
		sendSize = workSize + sizeof(*response);
	} else {
		work = _dbgpAllocWorkBuf(proc, DBGP_SYNC_LIST_SIZE + DBGP_SYNC_WAIT_SIZE);
		if (work == NULL)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		ultAllocated = 0;
		sendSize = DBGP_SYNC_LIST_SIZE + DBGP_SYNC_WAIT_SIZE +
			   sizeof(*response);
		workSize = DBGP_SYNC_LIST_SIZE + DBGP_SYNC_WAIT_SIZE;
	}
	response = _dbgpAllocSendBuf(proc, sendSize);
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, sendSize);
	response->pid = pid;
	pid = request->pid;
	out = response->records;
	used = sizeof(*response);

	if (num != 0) {
		for (i = 0; i < num; i++) {
			switch (types[i]) {
			case DBGP_SYNC_SEMAPHORE:
				result = _getSemaphoreInfo(pid, out, work, workSize,
							   &size);
				break;
			case DBGP_SYNC_EVENT_FLAG:
				result = _getEventFlagInfo(pid, out, work, workSize,
							   &size);
				break;
			case DBGP_SYNC_MUTEX:
				result = _getMutexInfo(pid, out, work, workSize, &size);
				break;
			case DBGP_SYNC_LW_MUTEX:
				result = _getLwMutexInfo(pid, out, work, workSize,
							 &size);
				break;
			case DBGP_SYNC_MSG_PIPE:
				result = _getMsgPipeInfo(pid, out, work, workSize,
							 &size);
				break;
			case DBGP_SYNC_SIMPLE_EVENT:
				result = _getSimpleEventInfo(pid, out, work, workSize,
							     &size);
				break;
			case DBGP_SYNC_CALLBACK:
				result = _getCallbackInfo(pid, out, work, workSize,
							  &size);
				break;
			case DBGP_SYNC_TIMER:
				result = _getTimerInfo(pid, out, work, workSize, &size);
				break;
			case DBGP_SYNC_RW_LOCK:
				result = _getRWLockInfo(pid, out, work, workSize,
							&size);
				break;
			case DBGP_SYNC_COND:
				result = _getCondInfo(pid, out, work, workSize, &size);
				break;
			case DBGP_SYNC_LW_COND:
				result = _getLwCondInfo(pid, out, work, workSize,
							&size);
				break;
			case ULOBJ_INFO_SEMA:
			case ULOBJ_INFO_MUTEX:
			case ULOBJ_INFO_QUEUE_DATA_RESOURCE_POOL:
			case ULOBJ_INFO_WAITING_QUEUE_RESOURCE_POOL:
			case ULOBJ_INFO_QUEUE:
			case ULOBJ_INFO_RWLOCK:
			case ULOBJ_INFO_COND:
				if (!ultAllocated &&
				    (SceSize)_getUltSyncObjInfoSizeOfType(pid, types[i]) >
				    workSize - used)
					continue;
				size = _getUltSyncObjInfo(pid, types[i],
							  (SceUInt32 *)out);
				out += size;
				used += size;
				continue;
			case 0x40:
			case 0x41:
			case 0x70:
			case 0x80:
				continue;
			default:
				SCE_DBG_ASSERT(0, 2834);
				continue;
			}
			if (result < 0)
				return result;
			out += size;
			used += size;
		}
	} else {
		result = _getSemaphoreInfo(pid, out, work, workSize, &size);
		if (result < 0)
			return result;
		out += size;
		used += size;
		result = _getEventFlagInfo(pid, out, work, workSize, &size);
		if (result < 0)
			return result;
		out += size;
		used += size;
		result = _getMutexInfo(pid, out, work, workSize, &size);
		if (result < 0)
			return result;
		out += size;
		used += size;
		result = _getLwMutexInfo(pid, out, work, workSize, &size);
		if (result < 0)
			return result;
		out += size;
		used += size;
		result = _getMsgPipeInfo(pid, out, work, workSize, &size);
		if (result < 0)
			return result;
		out += size;
		used += size;
		result = _getSimpleEventInfo(pid, out, work, workSize, &size);
		if (result < 0)
			return result;
		out += size;
		used += size;
		result = _getCallbackInfo(pid, out, work, workSize, &size);
		if (result < 0)
			return result;
		out += size;
		used += size;
		result = _getTimerInfo(pid, out, work, workSize, &size);
		if (result < 0)
			return result;
		out += size;
		used += size;
		result = _getRWLockInfo(pid, out, work, workSize, &size);
		if (result < 0)
			return result;
		out += size;
		used += size;
		result = _getCondInfo(pid, out, work, workSize, &size);
		if (result < 0)
			return result;
		out += size;
		used += size;
		result = _getLwCondInfo(pid, out, work, workSize, &size);
		if (result < 0)
			return result;
		out += size;
		used += size;
		for (j = 0; j < (int)(sizeof(s_ultSyncTypes) / sizeof(s_ultSyncTypes[0]));
		     j++) {
			if (!ultAllocated &&
			    (SceSize)_getUltSyncObjInfoSizeOfType(pid, s_ultSyncTypes[j]) >
			    workSize - used)
				continue;
			size = _getUltSyncObjInfo(pid, s_ultSyncTypes[j],
						  (SceUInt32 *)out);
			out += size;
			used += size;
		}
	}
	_dbgpSetSendSize(proc, used);
	dbgpFillResponseHeader(proc, packet->group, 0x21, 0, 0);
	return 0;
}

int threadSuspendThreadCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpThreadRequest *request = (const DbgpThreadRequest *)packet;
	ThreadmgrThreadInfoForDebugger info;
	DbgpThreadResponse *response;
	SceUID pid, thid, guid;
	int result, ret;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->pid;
	thid = request->thid;
	if (checkInvalidProcess(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20011);
		return (int)0x80080001;
	}
	if (checkAttachedProcess(proc, pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20051);
		return (int)0x80080001;
	}
	if (checkInvalidThread(pid, thid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20012);
		return (int)0x80080001;
	}
	if (checkIllegalProcessStatus(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20050);
		return (int)0x80080001;
	}
	result = checkThreadIsSuspendedAlready(pid, thid);
	if (result != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20061);
		return (int)0x80080001;
	}
	response = _dbgpAllocSendBuf(proc, sizeof(*response));
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, sizeof(*response));
	_dbgpSetSendSize(proc, sizeof(*response));
	guid = scePUIDtoGUID(pid, thid);
	info.size = sizeof(info);
	ret = SceThreadmgrForDriver_555CA6BB(guid, 0, &info);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 528, "0x%08x\n", ret);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20000);
		return ret;
	}
	ret = sceKernelDebugSuspendThread(guid, 1);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 538, "0x%08x\n", ret);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20000);
		return ret;
	}
	dbgpFillResponseHeader(proc, packet->group, 0x11, 0, 0);
	response->pid = pid;
	response->thid = thid;
	response->status = info.status;
	return result;
}

int threadResumeThreadCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpThreadRequest *request = (const DbgpThreadRequest *)packet;
	DbgpThreadResponse *response;
	SceUID pid, thid;
	int result, ret;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->pid;
	thid = request->thid;
	if (checkInvalidProcess(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20011);
		return (int)0x80080001;
	}
	if (checkAttachedProcess(proc, pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20051);
		return (int)0x80080001;
	}
	if (checkInvalidThread(pid, thid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20012);
		return (int)0x80080001;
	}
	if (checkIllegalProcessStatus(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20050);
		return (int)0x80080001;
	}
	result = checkIllegalThreadStatusResumeThread(pid, thid);
	if (result != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20060);
		return (int)0x80080001;
	}
	ret = sceKernelDebugResumeThread(scePUIDtoGUID(pid, thid), 1);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 643, "0x%08x\n", ret);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20000);
		return ret;
	}
	response = _dbgpAllocSendBuf(proc, offsetof(DbgpThreadResponse, status));
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, offsetof(DbgpThreadResponse, status));
	_dbgpSetSendSize(proc, offsetof(DbgpThreadResponse, status));
	dbgpFillResponseHeader(proc, packet->group, 0x13, 0, 0);
	response->pid = pid;
	response->thid = thid;
	return result;
}

int threadGetThreadNamesCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpRequestHeader *request = (const DbgpRequestHeader *)packet;
	DbgpThreadNamesResponse *response;
	ThreadmgrThreadName *names;
	SceSize size, count;
	SceUID pid;
	int result, ret, n, i;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->pid;
	result = checkUnknownProcess(pid);
	if (result != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20011);
		return (int)0x80080001;
	}
	for (;;) {
		/* the count first; again if threads came or went meanwhile */
		n = SceThreadmgrForDriver_FD87586C(pid, NULL, 0, NULL);
		if (n < 0) {
			SCE_PRINTF_WITH_INFO(15, 745, "0x%08x\n", n);
			return n;
		}
		names = _dbgpAllocWorkBuf(proc, n * sizeof(*names));
		if (names == NULL)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		memset(names, 0, n * sizeof(*names));
		ret = SceThreadmgrForDriver_FD87586C(pid, names, n * sizeof(*names),
						     &count);
		if (ret < 0) {
			SCE_PRINTF_WITH_INFO(15, 771, "0x%08x\n", ret);
			return ret;
		}
		if (n == (int)count)
			break;
		dbgpFreeWorkBuf(proc);
	}
	size = sizeof(*response) + n * sizeof(*names);
	response = _dbgpAllocSendBuf(proc, size);
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, size);
	_dbgpSetSendSize(proc, size);
	response->count = count;
	if ((int)count > 0) {
		if (pid == SCE_KERNEL_PROCESS_ID) {
			for (i = 0; i < (int)count; i++)
				response->names[i] = names[i];
		} else {
			for (i = 0; i < (int)count; i++) {
				response->names[i].threadId =
					SceThreadmgrForDriver_F311808F(names[i].threadId);
				__builtin_memcpy(response->names[i].name,
						 names[i].name,
						 sizeof(names[i].name));
			}
		}
	}
	dbgpFillResponseHeader(proc, packet->group, 0x15, 0, 0);
	return result;
}

int _getStackInfo(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpRequestHeader *request = (const DbgpRequestHeader *)packet;
	ThreadmgrThreadInfoForDebugger *work;
	DbgpStackInfoResponse *response;
	SceSize count;
	SceUID pid;
	int result, ret, i;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->pid;
	if (checkInvalidProcess(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20011);
		return (int)0x80080001;
	}
	if (checkIllegalProcessStatus(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20050);
		return (int)0x80080001;
	}
	result = checkAttachedProcess(proc, pid);
	if (result != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20051);
		return (int)0x80080001;
	}
	work = _dbgpAllocWorkBuf(proc, DBGP_THREAD_LIST_SIZE);
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	response = _dbgpAllocSendBuf(proc, sizeof(*response) +
				   DBGP_THREAD_MAX * sizeof(DbgpStackInfo));
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, sizeof(*response) +
	       DBGP_THREAD_MAX * sizeof(DbgpStackInfo));
	memset(work, 0, DBGP_THREAD_LIST_SIZE);
	/* flag 1: SceThreadmgr measures each stack's use */
	ret = SceThreadmgrForDriver_F2C9DC97(pid, 1, work, DBGP_THREAD_LIST_SIZE,
					     &count);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 1263, "0x%08x\n", ret);
		return ret;
	}
	for (i = 0; i < (int)count; i++) {
		response->info[i].size = sizeof(response->info[i]);
		response->info[i].thid = work[i].threadId;
		response->info[i].stackMaxUsed = work[i].userStackMaxUsed;
		response->info[i].stackUsed = work[i].userStackUsed;
	}
	response->count = count;
	_dbgpSetSendSize(proc, sizeof(*response) + count * sizeof(DbgpStackInfo));
	dbgpFillResponseHeader(proc, packet->group, 0x17, 0, 0);
	return result;
}
