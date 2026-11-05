/*
 * SceDeci4pSDbgp (os0:kd/deci4p_sdbgp.skprx): synchronization object
 * information for the system debugger (sync.c).
 */

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <processmgr/processmgr.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <sysmem/uid_class.h>
#include <threadmgr/lw_mutex.h>
#include <threadmgr/mutex.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/threadmgr_callback.h>
#include <threadmgr/threadmgr_debugger.h>
#include <threadmgr/threadmgr_event.h>
#include <threadmgr/threadmgr_timer.h>
#include "sdbgp_internal.h"

/* the synchronization object types */
#define SDBGP_SYNC_SEMA			0x10
#define SDBGP_SYNC_EVENT_FLAG		0x20
#define SDBGP_SYNC_MUTEX		0x30
#define SDBGP_SYNC_LW_MUTEX		0x31
#define SDBGP_SYNC_FAST_MUTEX		0x32
#define SDBGP_SYNC_MSG_PIPE		0x50
#define SDBGP_SYNC_CALLBACK		0x60
#define SDBGP_SYNC_THREAD_EVENT		0x70
#define SDBGP_SYNC_TIMER		0x81
#define SDBGP_SYNC_ALARM		0x82
#define SDBGP_SYNC_RW_LOCK		0x90
#define SDBGP_SYNC_COND			0xa0
#define SDBGP_SYNC_LW_COND		0xa1
#define SDBGP_SYNC_EVENT		0xb0
#define SDBGP_SYNC_WORK_TASK		0xc0

#define SDBGP_SYNC_WORK_SIZE		0x78000

typedef struct SdbgpWaitingThread {
	SceUID pid;
	SceUID thid;
} SdbgpWaitingThread;

static __attribute__((noinline)) int
sdbgpGetWaitingThreadList(SceUID guid, uint32_t type, int numWaitThreads,
			  void *out, void *work)
{
	ThreadmgrWaitingThread *list = (ThreadmgrWaitingThread *)work;
	SceSize count = 0;
	int i;
	int rem;
	int res;

	if ((guid & 1) == 0)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	if (numWaitThreads == 0 || work == NULL || out == NULL)
		return SDBGP_ERROR_INVALID_ARGUMENT;

	SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0,
					  numWaitThreads * sizeof(ThreadmgrWaitingThread));
	res = sceKernelGetWaitingThreadList(guid, list,
					    numWaitThreads * sizeof(ThreadmgrWaitingThread),
					    &count);
	if (res < 0)
		return res;

	if (type == SDBGP_SYNC_LW_MUTEX) {
		SceUID *thids = (SceUID *)out;

		for (i = 0; i < (int)count; i++)
			*thids++ = list[i].threadId;
		rem = numWaitThreads - (int)count;
		for (i = 0; i < rem; i++)
			*thids++ = 0;
	} else {
		SdbgpWaitingThread *waiting = (SdbgpWaitingThread *)out;

		for (i = 0; i < (int)count; i++) {
			waiting[i].pid = list[i].processId;
			waiting[i].thid = list[i].threadId;
		}
		waiting += (int)count;
		rem = numWaitThreads - (int)count;
		for (i = 0; i < rem; i++) {
			waiting[i].pid = 0;
			waiting[i].thid = 0;
		}
	}
	return 0;
}

static __attribute__((noinline)) int
sdbgpFillSemaInfo(const SceKernelSemaInfo *info, uint32_t *out, SceUID pid, void *work)
{
	int waitCount;
	int res;

	out[0] = SDBGP_SYNC_SEMA;
	out[1] = info->semaId;
	if (pid != 0)
		out[2] = pid;
	else
		out[2] = sceGUIDGetPID(out[1]);
	out[11] = info->attr;
	out[12] = 0;
	out[13] = info->initCount;
	out[14] = info->currentCount;
	out[15] = info->maxCount;
	out[16] = info->numWaitThreads;
	SceDeci4pSDfMgrForKernel_5996E2DB(&out[3], info->name, 32);
	waitCount = (int)info->numWaitThreads;
	if (waitCount <= 0)
		return 0;
	res = sdbgpGetWaitingThreadList(out[1], out[0], waitCount, &out[17], work);
	return res < 0 ? res : 0;
}

static __attribute__((noinline)) int
sdbgpFillEventFlagInfo(const SceKernelEventFlagInfo *info, uint32_t *out, SceUID pid, void *work)
{
	int waitCount;
	int res;

	out[0] = SDBGP_SYNC_EVENT_FLAG;
	out[1] = info->evfId;
	if (pid != 0)
		out[2] = pid;
	else
		out[2] = sceGUIDGetPID(out[1]);
	out[11] = info->attr;
	out[12] = 0;
	out[13] = info->initPattern;
	out[14] = info->currentPattern;
	out[15] = info->numWaitThreads;
	SceDeci4pSDfMgrForKernel_5996E2DB(&out[3], info->name, 32);
	waitCount = (int)info->numWaitThreads;
	if (waitCount <= 0)
		return 0;
	res = sdbgpGetWaitingThreadList(out[1], out[0], waitCount, &out[16], work);
	return res < 0 ? res : 0;
}

static __attribute__((noinline)) int
sdbgpFillMutexInfo(const SceKernelMutexInfo *info, uint32_t *out, SceUID pid, void *work)
{
	int waitCount;
	int res;

	out[0] = SDBGP_SYNC_MUTEX;
	out[1] = info->mutexId;
	if (pid != 0)
		out[2] = pid;
	else
		out[2] = sceGUIDGetPID(out[1]);
	out[11] = info->attr;
	out[12] = 0;
	out[13] = info->initCount;
	out[14] = info->currentCount;
	out[15] = info->currentOwnerId;
	out[16] = info->ceilingPriority;
	out[17] = info->numWaitThreads;
	SceDeci4pSDfMgrForKernel_5996E2DB(&out[3], info->name, 32);
	waitCount = (int)info->numWaitThreads;
	if (waitCount <= 0)
		return 0;
	res = sdbgpGetWaitingThreadList(out[1], out[0], waitCount, &out[18], work);
	return res < 0 ? res : 0;
}

static __attribute__((noinline)) int
sdbgpFillFastMutexInfo(const ThreadmgrFastMutexInfo *info, uint32_t *out, void *work)
{
	int waitCount;
	int res;

	out[0] = SDBGP_SYNC_FAST_MUTEX;
	out[1] = info->uid;
	out[2] = SDBGP_KERNEL_PROCESS_ID;
	out[11] = info->attr;
	out[12] = info->lockCount;
	out[13] = info->currentOwnerId;
	out[14] = info->ceilingPriority;
	out[15] = info->numWaitThreads;
	out[16] = info->numSpinThreads;
	SceDeci4pSDfMgrForKernel_5996E2DB(&out[3], info->name, 32);
	waitCount = info->numWaitThreads;
	if (waitCount <= 0 ||
	    (res = sdbgpGetWaitingThreadList(out[1], out[0], waitCount, &out[17], work)) >= 0) {
		out[17] = (uintptr_t)info->workArea;
		return 0;
	}
	return res;
}

static __attribute__((noinline)) int
sdbgpFillMsgPipeInfo(const SceKernelMsgPipeInfo *info, uint32_t *out, SceUID pid, void *work)
{
	int sendWait;
	int recvWait;
	int res;

	out[0] = SDBGP_SYNC_MSG_PIPE;
	out[1] = info->msgPipeId;
	if (pid != 0)
		out[2] = pid;
	else
		out[2] = sceGUIDGetPID(out[1]);
	out[11] = info->attr;
	out[12] = 0;
	out[13] = info->bufferSize;
	out[14] = info->freeSize;
	out[15] = info->numSendWaitThreads;
	out[16] = info->numReceiveWaitThreads;
	SceDeci4pSDfMgrForKernel_5996E2DB(&out[3], info->name, 32);
	sendWait = (int)info->numSendWaitThreads < 0 ? 0 : (int)info->numSendWaitThreads;
	recvWait = (int)info->numReceiveWaitThreads < 0 ? 0 : (int)info->numReceiveWaitThreads;
	if ((sendWait | recvWait) != 0) {
		res = sdbgpGetWaitingThreadList(out[1], out[0], sendWait + recvWait, &out[17],
						work);
		return res < 0 ? res : 0;
	}
	return 0;
}

static __attribute__((noinline)) int
sdbgpFillCondInfo(const SceKernelCondInfo *info, uint32_t *out, SceUID pid, void *work)
{
	int waitCount;
	int res;

	out[0] = SDBGP_SYNC_COND;
	out[1] = info->condId;
	if (pid != 0)
		out[2] = pid;
	else
		out[2] = sceGUIDGetPID(out[1]);
	out[11] = info->attr;
	out[12] = 0;
	out[13] = info->mutexId;
	out[14] = info->numWaitThreads;
	SceDeci4pSDfMgrForKernel_5996E2DB(&out[3], info->name, 32);
	waitCount = (int)info->numWaitThreads;
	if (waitCount <= 0)
		return 0;
	res = sdbgpGetWaitingThreadList(out[1], out[0], waitCount, &out[15], work);
	return res < 0 ? res : 0;
}

static __attribute__((noinline)) int
sdbgpFillLwMutexInfo(const SceKernelLwMutexInfo *info, uint32_t *out, SceUID pid, void *work)
{
	SceUID owner;
	int waitCount;
	int res;

	out[0] = SDBGP_SYNC_LW_MUTEX;
	out[1] = scePUIDtoGUID(pid, info->uid);
	out[2] = pid;
	out[11] = info->attr;
	out[12] = (uintptr_t)info->pWork;
	out[13] = info->initCount;
	out[14] = info->currentCount;
	owner = info->currentOwnerId;
	if ((owner & 0x40000000) != 0) {
		owner = scePUIDtoGUID(pid, owner);
		if (owner < 0)
			owner = info->currentOwnerId;
	}
	out[15] = owner;
	out[16] = info->numWaitThreads;
	SceDeci4pSDfMgrForKernel_5996E2DB(&out[3], info->name, 32);
	waitCount = (int)info->numWaitThreads;
	if (waitCount <= 0)
		return 0;
	res = sdbgpGetWaitingThreadList(out[1], out[0], waitCount, &out[17], work);
	return res < 0 ? res : 0;
}

static __attribute__((noinline)) int
sdbgpFillRWLockInfo(const SceKernelRWLockInfo *info, uint32_t *out, SceUID pid, void *work)
{
	SceUID owner;
	int readWait;
	int writeWait;
	int res;

	out[0] = SDBGP_SYNC_RW_LOCK;
	out[1] = info->rwLockId;
	if (pid != 0)
		out[2] = pid;
	else
		out[2] = sceGUIDGetPID(out[1]);
	out[11] = info->attr;
	out[12] = 0;
	out[13] = info->lockCount;
	owner = info->writeOwnerId;
	if ((owner & 0x40000000) != 0) {
		owner = scePUIDtoGUID(pid, owner);
		if (owner < 0)
			owner = info->writeOwnerId;
	}
	out[14] = owner;
	out[15] = info->numReadWaitThreads;
	out[16] = info->numWriteWaitThreads;
	SceDeci4pSDfMgrForKernel_5996E2DB(&out[3], info->name, 32);
	readWait = (int)info->numReadWaitThreads < 0 ? 0 : (int)info->numReadWaitThreads;
	writeWait = (int)info->numWriteWaitThreads < 0 ? 0 : (int)info->numWriteWaitThreads;
	if ((readWait | writeWait) != 0) {
		res = sdbgpGetWaitingThreadList(out[1], out[0], readWait + writeWait, &out[17],
						work);
		return res < 0 ? res : 0;
	}
	return 0;
}

static __attribute__((noinline)) int
sdbgpFillCallbackInfo(const SceKernelCallbackInfo *info, uint32_t *out, SceUID pid)
{
	SceSize count = 0;
	SceUID puid;

	out[0] = SDBGP_SYNC_CALLBACK;
	out[1] = info->callbackId;
	if (pid != 0) {
		out[2] = pid;
	} else {
		pid = sceGUIDGetPID(out[1]);
		out[2] = pid;
	}
	out[3] = info->threadId;
	out[12] = info->attr;
	out[13] = 0;
	out[14] = (uintptr_t)info->callbackFunc;
	out[15] = (uintptr_t)info->pCommon;
	out[16] = info->notifyCount;
	out[17] = info->notifyArg;
	if (pid != SDBGP_KERNEL_PROCESS_ID &&
	    PUIDGetPUIDVectorByGUID(pid, out[1], 5, &puid, sizeof(puid), &count) >= 0 &&
	    count != 0)
		out[18] = puid;
	else
		out[18] = -1;
	SceDeci4pSDfMgrForKernel_5996E2DB(&out[4], info->name, 32);
	return 0;
}

static __attribute__((noinline)) int
sdbgpFillThreadEventInfo(const ThreadmgrThreadEventInfo *info, uint32_t *out)
{
	SceSize count = 0;
	SceUID puid;
	SceUID pid;

	out[0] = SDBGP_SYNC_THREAD_EVENT;
	out[1] = info->uid;
	out[11] = info->attr;
	out[12] = info->mask;
	out[13] = info->target;
	pid = info->pid;
	out[14] = pid;
	out[15] = info->handler;
	out[16] = info->arg;
	if (pid != SDBGP_KERNEL_PROCESS_ID &&
	    PUIDGetPUIDVectorByGUID(pid, info->uid, 5, &puid, sizeof(puid), &count) >= 0 &&
	    count != 0)
		out[2] = puid;
	else
		out[2] = -1;
	SceDeci4pSDfMgrForKernel_5996E2DB(&out[3], info->name, 32);
	return 0;
}

static __attribute__((noinline)) int
sdbgpFillTimerInfo(const SceKernelTimerInfo *info, uint32_t *out, SceUID pid, void *work)
{
	SceSize count = 0;
	SceUID puid;
	int waitCount;
	int res;

	out[0] = SDBGP_SYNC_TIMER;
	out[1] = info->timerId;
	if (pid != 0) {
		out[2] = pid;
	} else {
		pid = sceGUIDGetPID(out[1]);
		out[2] = pid;
	}
	out[11] = info->attr;
	out[12] = 0;
	out[13] = info->fActive;
	*(SceKernelSysClock *)&out[14] = info->baseTime;
	*(SceKernelSysClock *)&out[16] = info->currentTime;
	*(SceKernelSysClock *)&out[18] = info->schedule;
	*(SceKernelSysClock *)&out[20] = info->interval;
	out[22] = info->type;
	out[23] = info->fRepeat;
	out[24] = info->numWaitThreads;
	if (pid != SDBGP_KERNEL_PROCESS_ID &&
	    PUIDGetPUIDVectorByGUID(pid, out[1], 5, &puid, sizeof(puid), &count) >= 0 &&
	    count != 0)
		out[25] = puid;
	else
		out[25] = -1;
	SceDeci4pSDfMgrForKernel_5996E2DB(&out[3], info->name, 32);
	waitCount = (int)info->numWaitThreads;
	if (waitCount <= 0)
		return 0;
	res = sdbgpGetWaitingThreadList(out[1], out[0], waitCount, &out[26], work);
	return res < 0 ? res : 0;
}

static __attribute__((noinline)) int
sdbgpFillLwCondInfo(const SceKernelLwCondInfo *info, uint32_t *out, SceUID pid, void *work)
{
	uint8_t buf[4];
	int waitCount;
	int res;
	int i;

	SceDeci4pSDfMgrForKernel_8A3B06F2(buf, 0, sizeof(buf));
	out[0] = SDBGP_SYNC_LW_COND;
	out[1] = scePUIDtoGUID(pid, info->uid);
	out[2] = pid;
	out[11] = info->attr;
	out[12] = (uintptr_t)info->pWork;
	out[13] = (uintptr_t)info->pLwMutex;
	out[14] = 0;
	if (sceKernelCopyFromUserProc(pid, buf,
				      &((const ThreadmgrLwMutexWork *)info->pLwMutex)->uid,
				      sizeof(buf)) >= 0) {
		SceUID puid = 0;

		for (i = 0; i < 4; i++)
			puid += (uint32_t)buf[i] << (i * 8);
		out[15] = scePUIDtoGUID(pid, puid);
	} else {
		out[15] = -1;
	}
	out[16] = info->numWaitThreads;
	SceDeci4pSDfMgrForKernel_5996E2DB(&out[3], info->name, 32);
	waitCount = (int)info->numWaitThreads;
	if (waitCount <= 0)
		return 0;
	res = sdbgpGetWaitingThreadList(out[1], out[0], waitCount, &out[17], work);
	return res < 0 ? res : 0;
}

static __attribute__((noinline)) int
sdbgpFillEventInfo(const SceKernelEventInfo *info, uint32_t *out, void *work)
{
	int waitCount;
	int res;

	waitCount = (int)info->numWaitThreads;
	out[2] = info->eventId;
	out[11] = info->attr;
	out[12] = info->eventPattern;
	out[13] = (uint32_t)info->userData;
	out[14] = (uint32_t)(info->userData >> 32);
	out[0] = (waitCount + 8) * 8;
	out[15] = waitCount;
	SceDeci4pSDfMgrForKernel_5996E2DB(&out[3], info->name, 32);
	waitCount = (int)info->numWaitThreads;
	if (waitCount <= 0)
		return 0;
	res = sdbgpGetWaitingThreadList(out[2], out[1], waitCount, &out[16], work);
	return res < 0 ? res : 0;
}

int sdbgpGetSyncInfoList(const SdbgpThreadListRequest *req, void *outBuf, void *work,
			 SceSize *size)
{
	uint32_t type = ((const uint32_t *)req)[8];
	SceUID pid = req->processId;
	uint32_t *out = (uint32_t *)outBuf;
	SceUID pids[16];
	SceSize count;
	SceSize subCount;
	void *waitWork;
	uintptr_t limit;
	int i;
	int j;
	int res;

	switch (type) {
	case SDBGP_SYNC_SEMA: {
		SceKernelSemaInfo *infos = (SceKernelSemaInfo *)work;

		count = 0;
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_SYNC_WORK_SIZE);
		waitWork = (uint8_t *)work + SDBGP_SYNC_WORK_SIZE;
		res = SceThreadmgrForDriver_C6741986(5, infos, SDBGP_SYNC_WORK_SIZE, &count);
		if (res < 0)
			return res;
		limit = (uintptr_t)out + 0x87ed0;
		for (i = 0; i < (int)count; i++) {
			int w = (int)infos[i].numWaitThreads;
			uint32_t waitBytes = w < 0 ? 0 : (uint32_t)w * 8;
			uint32_t bodySize;

			if (pid != 0 && pid != sceGUIDGetPID(infos[i].semaId))
				continue;
			if ((uintptr_t)out + 0x44 + waitBytes > limit)
				return SDBGP_RESULT_ADDRESS_OVERFLOW;
			bodySize = 0x44 + waitBytes;
			out[0] = SDBGP_SYNC_SEMA;
			out[1] = bodySize;
			*size += 8 + bodySize;
			res = sdbgpFillSemaInfo(&infos[i], &out[2], pid, waitWork);
			if (res < 0)
				return res;
			out = (uint32_t *)((uintptr_t)out + 8 + bodySize);
		}
		return 0;
	}
	case SDBGP_SYNC_EVENT_FLAG: {
		SceKernelEventFlagInfo *infos = (SceKernelEventFlagInfo *)work;

		count = 0;
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_SYNC_WORK_SIZE);
		waitWork = (uint8_t *)work + SDBGP_SYNC_WORK_SIZE;
		res = SceThreadmgrForDriver_5870C73A(5, infos, SDBGP_SYNC_WORK_SIZE, &count);
		if (res < 0)
			return res;
		limit = (uintptr_t)out + 0x87ed4;
		for (i = 0; i < (int)count; i++) {
			int w = (int)infos[i].numWaitThreads;
			uint32_t waitBytes = w < 0 ? 0 : (uint32_t)w * 8;
			uint32_t bodySize;

			if (pid != 0 && pid != sceGUIDGetPID(infos[i].evfId))
				continue;
			if ((uintptr_t)out + 0x40 + waitBytes > limit)
				return SDBGP_RESULT_ADDRESS_OVERFLOW;
			bodySize = 0x40 + waitBytes;
			out[0] = SDBGP_SYNC_EVENT_FLAG;
			out[1] = bodySize;
			*size += 8 + bodySize;
			res = sdbgpFillEventFlagInfo(&infos[i], &out[2], pid, waitWork);
			if (res < 0)
				return res;
			out = (uint32_t *)((uintptr_t)out + 8 + bodySize);
		}
		return 0;
	}
	case SDBGP_SYNC_MUTEX: {
		SceKernelMutexInfo *infos = (SceKernelMutexInfo *)work;

		count = 0;
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_SYNC_WORK_SIZE);
		waitWork = (uint8_t *)work + SDBGP_SYNC_WORK_SIZE;
		res = SceThreadmgrForDriver_B6DA4669(5, infos, SDBGP_SYNC_WORK_SIZE, &count);
		if (res < 0)
			return res;
		limit = (uintptr_t)out + 0x87ecc;
		for (i = 0; i < (int)count; i++) {
			int w = (int)infos[i].numWaitThreads;
			uint32_t waitBytes = w < 0 ? 0 : (uint32_t)w * 8;
			uint32_t bodySize;

			if (pid != 0 && pid != sceGUIDGetPID(infos[i].mutexId))
				continue;
			if ((uintptr_t)out + 0x48 + waitBytes > limit)
				return SDBGP_RESULT_ADDRESS_OVERFLOW;
			bodySize = 0x48 + waitBytes;
			out[0] = SDBGP_SYNC_MUTEX;
			out[1] = bodySize;
			*size += 8 + bodySize;
			res = sdbgpFillMutexInfo(&infos[i], &out[2], pid, waitWork);
			if (res < 0)
				return res;
			out = (uint32_t *)((uintptr_t)out + 8 + bodySize);
		}
		return 0;
	}
	case SDBGP_SYNC_LW_MUTEX: {
		SceKernelLwMutexInfo *infos = (SceKernelLwMutexInfo *)work;

		memset(pids, 0, sizeof(pids));
		res = SceProcessmgrForKernel_EFFF2975(5, pids, 16, &count);
		if (res < 0)
			return res;
		limit = (uintptr_t)out + 0x77f94;
		waitWork = (uint8_t *)work + SDBGP_SYNC_WORK_SIZE;
		for (i = 0; i < (int)count; i++) {
			if (pids[i] == SDBGP_KERNEL_PROCESS_ID || (pid != 0 && pid != pids[i]))
				continue;
			SceDeci4pSDfMgrForKernel_8A3B06F2(infos, 0, SDBGP_SYNC_WORK_SIZE);
			subCount = 0;
			res = sceKernelGetLwMutexInfoAllProc(pids[i], 5, infos,
							     SDBGP_SYNC_WORK_SIZE, &subCount);
			if (res < 0)
				return res;
			for (j = 0; j < (int)subCount; j++) {
				int w = (int)infos[j].numWaitThreads;
				uint32_t waitBytes = w < 0 ? 0 : (uint32_t)w * 4;
				uint32_t bodySize;

				if ((uintptr_t)out + 0x44 + waitBytes > limit)
					return SDBGP_RESULT_ADDRESS_OVERFLOW;
				bodySize = 0x44 + waitBytes;
				out[0] = SDBGP_SYNC_LW_MUTEX;
				out[1] = bodySize;
				*size += 8 + bodySize;
				res = sdbgpFillLwMutexInfo(&infos[j], &out[2], pids[i], waitWork);
				if (res < 0)
					return res;
				out = (uint32_t *)((uintptr_t)out + 8 + bodySize);
			}
		}
		return 0;
	}
	case SDBGP_SYNC_FAST_MUTEX: {
		ThreadmgrFastMutexInfo *infos = (ThreadmgrFastMutexInfo *)work;

		count = 0;
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_SYNC_WORK_SIZE);
		waitWork = (uint8_t *)work + SDBGP_SYNC_WORK_SIZE;
		res = SceThreadmgrForDriver_6D4C1EB7(5, infos, SDBGP_SYNC_WORK_SIZE, &count);
		if (res < 0)
			return res;
		limit = (uintptr_t)out + 0x87ed0;
		for (i = 0; i < (int)count; i++) {
			int w = infos[i].numWaitThreads;
			uint32_t waitBytes = w < 0 ? 0 : (uint32_t)w * 8;
			uint32_t bodySize;

			if ((uintptr_t)out + 0x44 + waitBytes > limit)
				return SDBGP_RESULT_ADDRESS_OVERFLOW;
			bodySize = 0x48 + waitBytes;
			out[0] = SDBGP_SYNC_FAST_MUTEX;
			out[1] = bodySize;
			*size += 0x50 + waitBytes;
			res = sdbgpFillFastMutexInfo(&infos[i], &out[2], waitWork);
			if (res < 0)
				return res;
			out = (uint32_t *)((uintptr_t)out + 8 + bodySize);
		}
		return 0;
	}
	case SDBGP_SYNC_MSG_PIPE: {
		SceKernelMsgPipeInfo *infos = (SceKernelMsgPipeInfo *)work;

		count = 0;
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_SYNC_WORK_SIZE);
		waitWork = (uint8_t *)work + SDBGP_SYNC_WORK_SIZE;
		res = SceThreadmgrForDriver_C7F5FFE0(5, infos, SDBGP_SYNC_WORK_SIZE, &count);
		if (res < 0)
			return res;
		limit = (uintptr_t)out + 0x77f94;
		for (i = 0; i < (int)count; i++) {
			int sw = (int)infos[i].numSendWaitThreads;
			int rw = (int)infos[i].numReceiveWaitThreads;
			uint32_t swBytes = sw < 0 ? 0 : (uint32_t)sw * 8;
			uint32_t rwBytes = rw < 0 ? 0 : (uint32_t)rw * 8;
			uint32_t bodySize;

			if (pid != 0 && pid != sceGUIDGetPID(infos[i].msgPipeId))
				continue;
			if ((uintptr_t)out + 0x44 + swBytes + rwBytes > limit)
				return SDBGP_RESULT_ADDRESS_OVERFLOW;
			bodySize = 0x44 + swBytes + rwBytes;
			out[0] = SDBGP_SYNC_MSG_PIPE;
			out[1] = bodySize;
			*size += 0x4c + swBytes + rwBytes;
			res = sdbgpFillMsgPipeInfo(&infos[i], &out[2], pid, waitWork);
			if (res < 0)
				return res;
			out = (uint32_t *)((uintptr_t)out + 8 + bodySize);
		}
		return 0;
	}
	case SDBGP_SYNC_CALLBACK: {
		SceKernelCallbackInfo *infos = (SceKernelCallbackInfo *)work;

		count = 0;
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_SYNC_WORK_SIZE);
		res = SceThreadmgrForDriver_767CA30C(5, infos, SDBGP_SYNC_WORK_SIZE, &count);
		if (res < 0)
			return res;
		limit = (uintptr_t)out + 0x77f8c;
		for (i = 0; i < (int)count; i++) {
			if (pid != 0 && pid != sceGUIDGetPID(infos[i].callbackId))
				continue;
			if ((uintptr_t)out + 0x4c > limit)
				return SDBGP_RESULT_ADDRESS_OVERFLOW;
			out[0] = type;
			out[1] = 0x4c;
			*size += 0x54;
			res = sdbgpFillCallbackInfo(&infos[i], &out[2], pid);
			if (res < 0)
				return res;
			out += 21;
		}
		return 0;
	}
	case SDBGP_SYNC_THREAD_EVENT: {
		ThreadmgrThreadEventInfo *infos = (ThreadmgrThreadEventInfo *)work;

		count = 0;
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_SYNC_WORK_SIZE);
		res = SceThreadmgrForDriver_5D8F88B1(5, infos, SDBGP_SYNC_WORK_SIZE, &count);
		if (res < 0)
			return res;
		limit = (uintptr_t)out + 0x87ed0;
		for (i = 0; (uint32_t)i < count; i++) {
			if (pid != 0 && pid != infos[i].pid)
				continue;
			if ((uintptr_t)out + 0x44 > limit)
				return SDBGP_RESULT_ADDRESS_OVERFLOW;
			out[0] = type;
			out[1] = 0x44;
			*size += 0x4c;
			res = sdbgpFillThreadEventInfo(&infos[i], &out[2]);
			if (res < 0)
				return res;
			out += 19;
		}
		return 0;
	}
	case SDBGP_SYNC_TIMER: {
		SceKernelTimerInfo *infos = (SceKernelTimerInfo *)work;

		count = 0;
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_SYNC_WORK_SIZE);
		waitWork = (uint8_t *)work + SDBGP_SYNC_WORK_SIZE;
		res = SceThreadmgrForDriver_3C297724(5, infos, SDBGP_SYNC_WORK_SIZE, &count);
		if (res < 0)
			return res;
		limit = (uintptr_t)out + 0x77f94;
		for (i = 0; i < (int)count; i++) {
			int w = (int)infos[i].numWaitThreads;
			uint32_t waitBytes = w < 0 ? 0 : (uint32_t)w * 8;
			uint32_t bodySize;

			if (pid != 0 && pid != sceGUIDGetPID(infos[i].timerId))
				continue;
			if ((uintptr_t)out + 0x68 + waitBytes > limit)
				return SDBGP_RESULT_ADDRESS_OVERFLOW;
			bodySize = 0x68 + waitBytes;
			out[0] = SDBGP_SYNC_TIMER;
			out[1] = bodySize;
			*size += 8 + bodySize;
			res = sdbgpFillTimerInfo(&infos[i], &out[2], pid, waitWork);
			if (res < 0)
				return res;
			out = (uint32_t *)((uintptr_t)out + 8 + bodySize);
		}
		return 0;
	}
	case SDBGP_SYNC_ALARM: {
		ThreadmgrAlarmInfo *infos = (ThreadmgrAlarmInfo *)work;

		count = 0;
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_SYNC_WORK_SIZE);
		res = SceThreadmgrForDriver_2828F884(5, infos, SDBGP_SYNC_WORK_SIZE, &count);
		if (res < 0)
			return res;
		limit = (uintptr_t)out + 0x87ee4;
		for (i = 0; i < (int)count; i++) {
			if ((uintptr_t)out + 0x30 > limit)
				return SDBGP_RESULT_ADDRESS_OVERFLOW;
			out[0] = SDBGP_SYNC_ALARM;
			out[1] = 0x30;
			*size += 0x38;
			out[2] = SDBGP_SYNC_ALARM;
			out[3] = infos[i].alarmId;
			out[12] = (uintptr_t)infos[i].handler;
			out[13] = (uintptr_t)infos[i].arg;
			SceDeci4pSDfMgrForKernel_5996E2DB(&out[4], infos[i].name, 32);
			out += 14;
		}
		return 0;
	}
	case SDBGP_SYNC_RW_LOCK: {
		SceKernelRWLockInfo *infos = (SceKernelRWLockInfo *)work;

		count = 0;
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_SYNC_WORK_SIZE);
		waitWork = (uint8_t *)work + SDBGP_SYNC_WORK_SIZE;
		res = SceThreadmgrForDriver_E2B57231(5, infos, SDBGP_SYNC_WORK_SIZE, &count);
		if (res < 0)
			return res;
		limit = (uintptr_t)out + 0x77f94;
		for (i = 0; i < (int)count; i++) {
			int rw = (int)infos[i].numReadWaitThreads;
			int ww = (int)infos[i].numWriteWaitThreads;
			int total = (ww < 0 ? 0 : ww) + (rw < 0 ? 0 : rw);
			uint32_t waitBytes = (uint32_t)total * 8;
			uint32_t bodySize;

			if (pid != 0 && pid != sceGUIDGetPID(infos[i].rwLockId))
				continue;
			if ((uintptr_t)out + 0x44 + waitBytes > limit)
				return SDBGP_RESULT_ADDRESS_OVERFLOW;
			bodySize = 0x44 + waitBytes;
			out[0] = SDBGP_SYNC_RW_LOCK;
			out[1] = bodySize;
			*size += 0x4c;
			res = sdbgpFillRWLockInfo(&infos[i], &out[2], pid, waitWork);
			if (res < 0)
				return res;
			out = (uint32_t *)((uintptr_t)out + 8 + bodySize);
		}
		return 0;
	}
	case SDBGP_SYNC_COND: {
		SceKernelCondInfo *infos = (SceKernelCondInfo *)work;

		count = 0;
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_SYNC_WORK_SIZE);
		waitWork = (uint8_t *)work + SDBGP_SYNC_WORK_SIZE;
		res = SceThreadmgrForDriver_67022B18(5, infos, SDBGP_SYNC_WORK_SIZE, &count);
		if (res < 0)
			return res;
		limit = (uintptr_t)out + 0x87ed8;
		for (i = 0; i < (int)count; i++) {
			int w = (int)infos[i].numWaitThreads;
			uint32_t waitBytes = w < 0 ? 0 : (uint32_t)w * 8;
			uint32_t bodySize;

			if (pid != 0 && pid != sceGUIDGetPID(infos[i].condId))
				continue;
			if ((uintptr_t)out + 0x3c + waitBytes > limit)
				return SDBGP_RESULT_ADDRESS_OVERFLOW;
			bodySize = 0x3c + waitBytes;
			out[0] = SDBGP_SYNC_COND;
			out[1] = bodySize;
			*size += 8 + bodySize;
			res = sdbgpFillCondInfo(&infos[i], &out[2], pid, waitWork);
			if (res < 0)
				return res;
			out = (uint32_t *)((uintptr_t)out + 8 + bodySize);
		}
		return 0;
	}
	case SDBGP_SYNC_LW_COND: {
		SceKernelLwCondInfo *infos = (SceKernelLwCondInfo *)work;

		memset(pids, 0, sizeof(pids));
		res = SceProcessmgrForKernel_EFFF2975(5, pids, 16, &count);
		if (res < 0)
			return res;
		limit = (uintptr_t)out + 0x77f94;
		waitWork = (uint8_t *)work + SDBGP_SYNC_WORK_SIZE;
		for (i = 0; i < (int)count; i++) {
			if (pids[i] == SDBGP_KERNEL_PROCESS_ID || (pid != 0 && pid != pids[i]))
				continue;
			SceDeci4pSDfMgrForKernel_8A3B06F2(infos, 0, SDBGP_SYNC_WORK_SIZE);
			subCount = 0;
			res = SceThreadmgrForDriver_09346D1A(pids[i], 5, infos,
							     SDBGP_SYNC_WORK_SIZE, &subCount);
			if (res < 0)
				return res;
			for (j = 0; j < (int)subCount; j++) {
				int w = (int)infos[j].numWaitThreads;
				uint32_t waitBytes = w < 0 ? 0 : (uint32_t)w * 8;
				uint32_t bodySize;

				if ((uintptr_t)out + 0x44 + waitBytes > limit)
					return SDBGP_RESULT_ADDRESS_OVERFLOW;
				bodySize = 0x44 + waitBytes;
				out[0] = SDBGP_SYNC_LW_COND;
				out[1] = bodySize;
				*size += 8 + bodySize;
				res = sdbgpFillLwCondInfo(&infos[j], &out[2], pids[i], waitWork);
				if (res < 0)
					return res;
				out = (uint32_t *)((uintptr_t)out + 8 + bodySize);
			}
		}
		return 0;
	}
	case SDBGP_SYNC_EVENT: {
		SceKernelEventInfo *infos = (SceKernelEventInfo *)work;

		count = 0;
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_SYNC_WORK_SIZE);
		waitWork = (uint8_t *)work + SDBGP_SYNC_WORK_SIZE;
		res = SceThreadmgrForDriver_995F32E1(5, infos, SDBGP_SYNC_WORK_SIZE, &count);
		if (res < 0)
			return res;
		limit = (uintptr_t)out + 0x87ed4;
		for (i = 0; i < (int)count; i++) {
			int w;
			uint32_t waitBytes;
			uint32_t bodySize;

			if (pid != 0 && pid != sceGUIDGetPID(infos[i].eventId))
				continue;
			if (infos[i].eventId == 0)
				continue;
			w = (int)infos[i].numWaitThreads;
			waitBytes = w < 0 ? 0 : (uint32_t)w * 8;
			if ((uintptr_t)out + 0x40 + waitBytes > limit)
				return SDBGP_RESULT_ADDRESS_OVERFLOW;
			bodySize = 0x40 + waitBytes;
			out[0] = type;
			out[1] = bodySize;
			*size += 8 + bodySize;
			res = sdbgpFillEventInfo(&infos[i], &out[2], waitWork);
			if (res < 0)
				return res;
			out = (uint32_t *)((uintptr_t)out + 8 + bodySize);
		}
		return 0;
	}
	case SDBGP_SYNC_WORK_TASK: {
		ThreadmgrWorkTaskInfo *infos = (ThreadmgrWorkTaskInfo *)work;

		count = 0;
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_SYNC_WORK_SIZE);
		res = SceThreadmgrForDriver_E605ED7A(5, infos, SDBGP_SYNC_WORK_SIZE, &count);
		if (res < 0)
			return res;
		limit = (uintptr_t)out + 0x87ebc;
		for (i = 0; (uint32_t)i < count; i++) {
			if ((uintptr_t)out + 0x58 > limit)
				return SDBGP_RESULT_ADDRESS_OVERFLOW;
			out[0] = SDBGP_SYNC_WORK_TASK;
			out[1] = 0x58;
			*size += 0x60;
			out[2] = SDBGP_SYNC_WORK_TASK;
			out[3] = infos[i].workTaskId;
			out[4] = infos[i].creatorPid;
			out[13] = infos[i].puid;
			out[14] = infos[i].entry;
			out[15] = (uintptr_t)infos[i].arg;
			out[16] = infos[i].workQueueId;
			out[17] = infos[i].threadId;
			out[18] = infos[i].creatorThreadId;
			out[19] = (uintptr_t)infos[i].caller;
			out[20] = infos[i].priority;
			out[21] = infos[i].permission;
			out[22] = infos[i].tlsProcessId;
			out[23] = infos[i].ioFlags;
			SceDeci4pSDfMgrForKernel_5996E2DB(&out[5], infos[i].name, 32);
			out += 24;
		}
		return 0;
	}
	default:
		return 0;
	}
}

SceSize sdbgpGetSyncAllCount(const SdbgpThreadListRequest *req, void *work)
{
	uint32_t type = ((const uint32_t *)req)[8];
	SceUID uid = (SceUID)((const uint32_t *)req)[9];
	uint32_t *info = (uint32_t *)work;

	switch (type) {
	case SDBGP_SYNC_SEMA:
		info[0] = sizeof(SceKernelSemaInfo);
		sceKernelGetSemaInfo(uid, (SceKernelSemaInfo *)info);
		return info[14] * 8 + 76;
	case SDBGP_SYNC_EVENT_FLAG:
		info[0] = sizeof(SceKernelEventFlagInfo);
		sceKernelGetEventFlagInfo(uid, (SceKernelEventFlagInfo *)info);
		return info[13] * 8 + 72;
	case SDBGP_SYNC_MUTEX:
		info[0] = sizeof(SceKernelMutexInfo);
		sceKernelGetMutexInfo(uid, (SceKernelMutexInfo *)info);
		return info[14] * 8 + 80;
	case SDBGP_SYNC_LW_MUTEX:
		info[0] = sizeof(SceKernelLwMutexInfo);
		sceKernelGetLwMutexInfoById(uid, (SceKernelLwMutexInfo *)info);
		return info[15] * 4 + 76;
	case SDBGP_SYNC_FAST_MUTEX:
		info[0] = sizeof(ThreadmgrFastMutexInfo);
		SceThreadmgrForDriver_AF302193(uid, (ThreadmgrFastMutexInfo *)info);
		return info[15] * 8 + 76;
	case SDBGP_SYNC_MSG_PIPE:
		info[0] = sizeof(SceKernelMsgPipeInfo);
		sceKernelGetMsgPipeInfo(uid, (SceKernelMsgPipeInfo *)info);
		return (info[13] + info[14]) * 8 + 76;
	case SDBGP_SYNC_CALLBACK:
		info[0] = sizeof(SceKernelCallbackInfo);
		sceKernelGetCallbackInfo(uid, (SceKernelCallbackInfo *)info);
		return 84;
	case SDBGP_SYNC_TIMER:
		info[0] = sizeof(SceKernelTimerInfo);
		sceKernelGetTimerInfo(uid, (SceKernelTimerInfo *)info);
		return 112;
	case SDBGP_SYNC_ALARM:
		info[0] = sizeof(ThreadmgrAlarmInfo);
		SceThreadmgrForDriver_4A038803(uid, (ThreadmgrAlarmInfo *)info);
		return 56;
	case SDBGP_SYNC_RW_LOCK:
		info[0] = sizeof(SceKernelRWLockInfo);
		sceKernelGetRWLockInfo(uid, (SceKernelRWLockInfo *)info);
		return (info[13] + info[14]) * 8 + 76;
	case SDBGP_SYNC_COND:
		info[0] = sizeof(SceKernelCondInfo);
		SceThreadmgrForDriver_D1FBDFA6(uid, (SceKernelCondInfo *)info);
		return info[12] * 8 + 68;
	case SDBGP_SYNC_LW_COND:
		info[0] = sizeof(SceKernelLwCondInfo);
		SceThreadmgrForDriver_C2AFFBD0(uid, (SceKernelLwCondInfo *)info);
		return info[13] * 8 + 76;
	case SDBGP_SYNC_EVENT:
		info[0] = sizeof(SceKernelEventInfo);
		SceThreadmgrForDriver_CBA7FAAA(uid, (SceKernelEventInfo *)info);
		return info[14] * 8 + 72;
	case SDBGP_SYNC_WORK_TASK:
		info[0] = sizeof(ThreadmgrWorkTaskInfoShort);
		SceThreadmgrForDriver_0B604A3C(uid, (ThreadmgrWorkTaskInfoShort *)info);
		return 96;
	default:
		return 8;
	}
}

int sdbgpGetSyncAllInfo(const SdbgpThreadListRequest *req, void *outBuf, void *work)
{
	uint32_t type = ((const uint32_t *)req)[8];
	SceUID uid = (SceUID)((const uint32_t *)req)[9];
	uint32_t *out = (uint32_t *)outBuf;
	uint32_t *info = (uint32_t *)work;
	SceUID pids[16];
	SceSize count;
	SceSize subCount;
	int i;
	uint32_t j;
	int res = (int)(uintptr_t)req;

	switch (type) {
	case SDBGP_SYNC_SEMA:
		SceDeci4pSDfMgrForKernel_8A3B06F2(info, 0, sizeof(SceKernelSemaInfo));
		info[0] = sizeof(SceKernelSemaInfo);
		res = sceKernelGetSemaInfo(uid, (SceKernelSemaInfo *)info);
		if (res >= 0) {
			out[0] = SDBGP_SYNC_SEMA;
			if ((int)info[14] < 0)
				out[1] = 68;
			else
				out[1] = info[14] * 8 + 68;
			res = sdbgpFillSemaInfo((SceKernelSemaInfo *)info, &out[2], 0, &info[15]);
		}
		break;
	case SDBGP_SYNC_EVENT_FLAG:
		SceDeci4pSDfMgrForKernel_8A3B06F2(info, 0, sizeof(SceKernelEventFlagInfo));
		info[0] = sizeof(SceKernelEventFlagInfo);
		res = sceKernelGetEventFlagInfo(uid, (SceKernelEventFlagInfo *)info);
		if (res >= 0) {
			out[0] = SDBGP_SYNC_EVENT_FLAG;
			if ((int)info[13] < 0)
				out[1] = 64;
			else
				out[1] = (info[13] + 8) * 8;
			res = sdbgpFillEventFlagInfo((SceKernelEventFlagInfo *)info, &out[2], 0,
						     &info[14]);
		}
		break;
	case SDBGP_SYNC_MUTEX:
		SceDeci4pSDfMgrForKernel_8A3B06F2(info, 0, sizeof(SceKernelMutexInfo));
		info[0] = sizeof(SceKernelMutexInfo);
		res = sceKernelGetMutexInfo(uid, (SceKernelMutexInfo *)info);
		if (res >= 0) {
			out[0] = SDBGP_SYNC_MUTEX;
			if ((int)info[14] < 0)
				out[1] = 72;
			else
				out[1] = (info[14] + 9) * 8;
			res = sdbgpFillMutexInfo((SceKernelMutexInfo *)info, &out[2], 0, &info[16]);
		}
		break;
	case SDBGP_SYNC_LW_MUTEX: {
		SceKernelLwMutexInfo *lwInfos = (SceKernelLwMutexInfo *)info;

		memset(pids, 0, sizeof(pids));
		res = SceProcessmgrForKernel_EFFF2975(5, pids, 16, &count);
		if (res < 0 || (int)count <= 0)
			break;
		for (i = 0; i < (int)count; i++) {
			if (pids[i] == SDBGP_KERNEL_PROCESS_ID)
				continue;
			subCount = 0;
			SceDeci4pSDfMgrForKernel_8A3B06F2(info, 0, SDBGP_SYNC_WORK_SIZE);
			res = sceKernelGetLwMutexInfoAllProc(pids[i], 5, lwInfos,
							     SDBGP_SYNC_WORK_SIZE, &subCount);
			if (res < 0)
				return res;
			for (j = 0; j < subCount; j++) {
				res = scePUIDtoGUID(pids[i], lwInfos[j].uid);
				if (res == uid) {
					out[0] = SDBGP_SYNC_LW_MUTEX;
					if ((int)lwInfos[0].numWaitThreads < 0)
						out[1] = 68;
					else
						out[1] = (lwInfos[j].numWaitThreads + 17) * 4;
					return sdbgpFillLwMutexInfo(&lwInfos[j], &out[2], pids[i],
								    (uint8_t *)info + SDBGP_SYNC_WORK_SIZE);
				}
			}
		}
		break;
	}
	case SDBGP_SYNC_FAST_MUTEX:
		SceDeci4pSDfMgrForKernel_8A3B06F2(info, 0, sizeof(ThreadmgrFastMutexInfo));
		info[0] = sizeof(ThreadmgrFastMutexInfo);
		res = SceThreadmgrForDriver_AF302193(uid, (ThreadmgrFastMutexInfo *)info);
		if (res >= 0) {
			out[0] = SDBGP_SYNC_FAST_MUTEX;
			if ((int)info[15] < 0)
				out[1] = 72;
			else
				out[1] = (info[15] + 9) * 8;
			res = sdbgpFillFastMutexInfo((ThreadmgrFastMutexInfo *)info, &out[2],
						     &info[17]);
		}
		break;
	case SDBGP_SYNC_MSG_PIPE:
		SceDeci4pSDfMgrForKernel_8A3B06F2(info, 0, sizeof(SceKernelMsgPipeInfo));
		info[0] = sizeof(SceKernelMsgPipeInfo);
		res = sceKernelGetMsgPipeInfo(uid, (SceKernelMsgPipeInfo *)info);
		if (res >= 0) {
			out[0] = SDBGP_SYNC_MSG_PIPE;
			out[1] = 68;
			if ((int)info[13] > 0)
				out[1] = info[13] * 8 + 68;
			if ((int)info[14] < 0)
				out[1] += info[14] * 8;
			res = sdbgpFillMsgPipeInfo((SceKernelMsgPipeInfo *)info, &out[2], 0,
						   &info[15]);
		}
		break;
	case SDBGP_SYNC_CALLBACK:
		SceDeci4pSDfMgrForKernel_8A3B06F2(info, 0, sizeof(SceKernelCallbackInfo));
		info[0] = sizeof(SceKernelCallbackInfo);
		res = sceKernelGetCallbackInfo(uid, (SceKernelCallbackInfo *)info);
		if (res >= 0) {
			out[0] = SDBGP_SYNC_CALLBACK;
			out[1] = 76;
			res = sdbgpFillCallbackInfo((SceKernelCallbackInfo *)info, &out[2], 0);
		}
		break;
	case SDBGP_SYNC_THREAD_EVENT:
		SceDeci4pSDfMgrForKernel_8A3B06F2(info, 0, sizeof(ThreadmgrThreadEventInfo));
		info[0] = sizeof(ThreadmgrThreadEventInfo);
		res = SceThreadmgrForDriver_6657429E(uid, (ThreadmgrThreadEventInfo *)info);
		if (res >= 0) {
			out[0] = SDBGP_SYNC_THREAD_EVENT;
			out[1] = 68;
			res = sdbgpFillThreadEventInfo((ThreadmgrThreadEventInfo *)info, &out[2]);
		}
		break;
	case SDBGP_SYNC_TIMER:
		SceDeci4pSDfMgrForKernel_8A3B06F2(info, 0, sizeof(SceKernelTimerInfo));
		info[0] = sizeof(SceKernelTimerInfo);
		res = sceKernelGetTimerInfo(uid, (SceKernelTimerInfo *)info);
		if (res >= 0) {
			out[0] = SDBGP_SYNC_TIMER;
			if ((int)info[22] < 0)
				out[1] = 104;
			else
				out[1] = (info[22] + 13) * 8;
			res = sdbgpFillTimerInfo((SceKernelTimerInfo *)info, &out[2], 0,
						 &info[24]);
		}
		break;
	case SDBGP_SYNC_ALARM:
		SceDeci4pSDfMgrForKernel_8A3B06F2(info, 0, sizeof(ThreadmgrAlarmInfo));
		info[0] = sizeof(ThreadmgrAlarmInfo);
		res = SceThreadmgrForDriver_4A038803(uid, (ThreadmgrAlarmInfo *)info);
		if (res >= 0) {
			out[0] = SDBGP_SYNC_ALARM;
			out[1] = 48;
			out[2] = SDBGP_SYNC_ALARM;
			out[3] = info[1];
			out[12] = info[12];
			out[13] = info[13];
			res = (int)(uintptr_t)SceDeci4pSDfMgrForKernel_5996E2DB(&out[4], &info[2],
										32);
		}
		break;
	case SDBGP_SYNC_RW_LOCK:
		SceDeci4pSDfMgrForKernel_8A3B06F2(info, 0, sizeof(SceKernelRWLockInfo));
		info[0] = sizeof(SceKernelRWLockInfo);
		res = sceKernelGetRWLockInfo(uid, (SceKernelRWLockInfo *)info);
		if (res >= 0) {
			out[0] = SDBGP_SYNC_RW_LOCK;
			out[1] = 68;
			if ((int)info[13] < 0)
				out[1] = info[13] * 8 + 68;
			if ((int)info[14] < 0)
				out[1] += info[14] * 8;
			res = sdbgpFillRWLockInfo((SceKernelRWLockInfo *)info, &out[2], 0,
						  &info[15]);
		}
		break;
	case SDBGP_SYNC_COND:
		SceDeci4pSDfMgrForKernel_8A3B06F2(info, 0, sizeof(SceKernelCondInfo));
		info[0] = sizeof(SceKernelCondInfo);
		res = SceThreadmgrForDriver_D1FBDFA6(uid, (SceKernelCondInfo *)info);
		if (res >= 0) {
			out[0] = SDBGP_SYNC_COND;
			if ((int)info[12] < 0)
				out[1] = 60;
			else
				out[1] = info[12] * 8 + 60;
			res = sdbgpFillCondInfo((SceKernelCondInfo *)info, &out[2], 0, &info[13]);
		}
		break;
	case SDBGP_SYNC_LW_COND: {
		SceKernelLwCondInfo *lwInfos = (SceKernelLwCondInfo *)info;

		memset(pids, 0, sizeof(pids));
		res = SceProcessmgrForKernel_EFFF2975(5, pids, 16, &count);
		if (res < 0 || (int)count <= 0)
			break;
		for (i = 0; i < (int)count; i++) {
			if (pids[i] == SDBGP_KERNEL_PROCESS_ID)
				continue;
			SceDeci4pSDfMgrForKernel_8A3B06F2(info, 0, SDBGP_SYNC_WORK_SIZE);
			res = SceThreadmgrForDriver_09346D1A(pids[i], 5, lwInfos,
							     SDBGP_SYNC_WORK_SIZE, &subCount);
			if (res < 0)
				return res;
			for (j = 0; j < subCount; j++) {
				res = scePUIDtoGUID(pids[i], lwInfos[j].uid);
				if (res == uid) {
					out[0] = SDBGP_SYNC_LW_COND;
					if ((int)lwInfos[0].numWaitThreads < 0)
						out[1] = 68;
					else
						out[1] = lwInfos[j].numWaitThreads * 8 + 68;
					return sdbgpFillLwCondInfo(&lwInfos[j], &out[2], pids[i],
								   (uint8_t *)info + SDBGP_SYNC_WORK_SIZE);
				}
			}
		}
		break;
	}
	case SDBGP_SYNC_EVENT:
		SceDeci4pSDfMgrForKernel_8A3B06F2(info, 0, sizeof(SceKernelEventInfo));
		info[0] = sizeof(SceKernelEventInfo);
		res = SceThreadmgrForDriver_CBA7FAAA(uid, (SceKernelEventInfo *)info);
		if (res >= 0) {
			out[0] = SDBGP_SYNC_EVENT;
			if ((int)info[14] < 0)
				out[1] = 64;
			else
				out[1] = (info[14] + 8) * 8;
			res = sdbgpFillEventInfo((SceKernelEventInfo *)info, &out[2], &info[16]);
		}
		break;
	case SDBGP_SYNC_WORK_TASK: {
		ThreadmgrWorkTaskInfo *wtInfos = (ThreadmgrWorkTaskInfo *)info;

		SceDeci4pSDfMgrForKernel_8A3B06F2(info, 0, SDBGP_SYNC_WORK_SIZE);
		res = SceThreadmgrForDriver_E605ED7A(5, wtInfos, SDBGP_SYNC_WORK_SIZE, &count);
		if (res < 0 || count == 0)
			break;
		for (i = 0; i < (int)count; i++) {
			if (wtInfos[i].workTaskId == uid) {
				out[0] = SDBGP_SYNC_WORK_TASK;
				out[1] = 88;
				out[2] = SDBGP_SYNC_WORK_TASK;
				out[3] = wtInfos[i].workTaskId;
				out[4] = wtInfos[i].creatorPid;
				out[13] = wtInfos[i].puid;
				out[14] = wtInfos[i].entry;
				out[15] = (uintptr_t)wtInfos[i].arg;
				out[16] = wtInfos[i].workQueueId;
				out[17] = wtInfos[i].threadId;
				out[18] = wtInfos[i].creatorThreadId;
				out[19] = (uintptr_t)wtInfos[i].caller;
				out[20] = wtInfos[i].priority;
				out[21] = wtInfos[i].permission;
				out[22] = wtInfos[i].tlsProcessId;
				out[23] = wtInfos[i].ioFlags;
				res = (int)(uintptr_t)SceDeci4pSDfMgrForKernel_5996E2DB(
					&out[5], wtInfos[i].name, 32);
				break;
			}
		}
		break;
	}
	default:
		break;
	}
	return res;
}
