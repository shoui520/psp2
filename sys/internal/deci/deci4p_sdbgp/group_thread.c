/*
 * SceDeci4pSDbgp (os0:kd/deci4p_sdbgp.skprx): SDBGP_GROUP_THREAD (0x30)
 * command handlers.
 */

#include <scetypes.h>
#include <kernel/error.h>
#include <sysmem/cpu.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/threadmgr_debugger.h>
#include "sdbgp_internal.h"

typedef struct SdbgpThreadListReply {
	SdbgpHeader header;
	SceUID processId;			/* 0x1c */
	uint32_t count;				/* 0x20 */
	uint8_t info[];				/* 0x24 */
} SdbgpThreadListReply;

typedef struct SdbgpWorkQueueListRequest {
	SdbgpRequest header;
	SceUID processId;			/* 0x14 */
} SdbgpWorkQueueListRequest;

typedef struct SdbgpWorkQueueListReply {
	SdbgpHeader header;
	SceUID processId;			/* 0x1c */
	uint32_t count;				/* 0x20 */
	uint8_t info[];				/* 0x24 */
} SdbgpWorkQueueListReply;

typedef struct SdbgpThreadSuspendRequest {
	SdbgpRequest header;
	SceUID processId;			/* 0x14 */
	SceUID threadId;			/* 0x18 */
} SdbgpThreadSuspendRequest;

typedef struct SdbgpThreadSuspendReply {
	SdbgpHeader header;
	SceUID processId;			/* 0x1c */
	SceUID threadId;			/* 0x20 */
	uint32_t status;			/* 0x24 */
} SdbgpThreadSuspendReply;

typedef struct SdbgpThreadResumeReply {
	SdbgpHeader header;
	SceUID processId;			/* 0x1c */
	SceUID threadId;			/* 0x20 */
} SdbgpThreadResumeReply;

typedef struct SdbgpSyncListReply {
	SdbgpHeader header;
	SceUID processId;			/* 0x1c */
	uint8_t info[];				/* 0x20 */
} SdbgpSyncListReply;

typedef struct SdbgpWaypointTraceRequest {
	SdbgpRequest header;
	SceUID processId;			/* 0x14 */
} SdbgpWaypointTraceRequest;

typedef struct SdbgpWaypointTraceReply {
	SdbgpHeader header;
	SceUID processId;			/* 0x1c */
	uint32_t count;				/* 0x20 */
	uint8_t info[];				/* 0x24 */
} SdbgpWaypointTraceReply;

int sdbgpCmd30_00(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	const SdbgpThreadListRequest *req = (const SdbgpThreadListRequest *)packet;
	SdbgpThreadListReply *reply;
	void *work;
	SceSize recvSize;
	SceUID pid;
	uint32_t detail;
	SceSize size;
	SceSize count;
	int ret;

	size = 0;
	count = 0;
	recvSize = sdbgpGetRecvSize(cpu);
	if (recvSize <= 0x1b && recvSize != 24) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	pid = req->processId;
	if (recvSize <= 0x1b)
		detail = 1;
	else
		detail = req->mode;
	if (pid != SDBGP_KERNEL_PROCESS_ID && pid != 0 && isIllegalProcessId(pid) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	reply = sdbgpAllocSendBuf(cpu, SDBGP_SEND_BUF_SIZE);
	if (!reply)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, SDBGP_SEND_BUF_SIZE);
	work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
	ret = sdbgpGetThreadInfoList(cpu, reply->info, work, pid, detail, &size, &count);
	if (ret < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, ret);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	reply->processId = pid;
	reply->count = count;
	sdbgpSetSendSize(cpu, size + sizeof(SdbgpThreadListReply));
	sdbgpSendErrorHeader(cpu, packet->group, 1, 0, 0);
	return 0;
}

int sdbgpCmd30_02(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	const SdbgpWorkQueueListRequest *req = (const SdbgpWorkQueueListRequest *)packet;
	SdbgpWorkQueueListReply *reply;
	ThreadmgrWorkQueueInfoFull *work;
	SceUID pid;
	SceSize size;
	SceSize replySize;
	int kernelOnly;
	int byPid;
	int ret;

	if (sdbgpGetRecvSize(cpu) <= 0x17) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	pid = req->processId;
	if (pid == SDBGP_KERNEL_PROCESS_ID || pid == 0) {
		kernelOnly = (pid == SDBGP_KERNEL_PROCESS_ID);
		if (SceThreadmgrForDriver_920EA1CA(5, NULL, 0, &size) < 0)
			return SCE_KERNEL_ERROR_THREADMGR_ERROR;
		byPid = 0;
	} else {
		kernelOnly = isIllegalProcessId(pid);
		if (kernelOnly != 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
				       SDBGP_RESULT_ILLEGAL_PROCESS_ID);
			return SDBGP_ERROR_INVALID_ARGUMENT;
		}
		if (SceThreadmgrForDriver_8C8A76EF(pid, 5, NULL, 0, &size) < 0)
			return SCE_KERNEL_ERROR_THREADMGR_ERROR;
		byPid = 1;
	}
	if (size <= SDBGP_PACKET_MAX) {
		replySize = size + sizeof(SdbgpWorkQueueListReply);
	} else {
		replySize = SDBGP_PACKET_MAX;
		size = SDBGP_PACKET_MAX - sizeof(SdbgpWorkQueueListReply);
	}
	reply = sdbgpAllocSendBuf(cpu, replySize);
	if (!reply)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, replySize);
	sdbgpSetSendSize(cpu, replySize);
	reply->processId = pid;
	work = sdbgpAllocWorkBuf(cpu, size);
	if (byPid == 0)
		ret = sdbgpGetKernelWorkQueueInfo(cpu, reply, work, size, kernelOnly);
	else
		ret = sdbgpGetProcessWorkQueueInfo(cpu, reply, work, size);
	if (ret >= 0) {
		sdbgpSendErrorHeader(cpu, packet->group, 3, 0, 0);
		ret = 0;
	}
	return ret;
}

int sdbgpCmd30_20(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	const SdbgpThreadListRequest *req = (const SdbgpThreadListRequest *)packet;
	SdbgpSyncListReply *reply;
	void *work;
	SceUID pid;
	uint32_t mode;
	uint32_t result;
	SceSize size;
	int ret;

	size = 0;
	if (sdbgpGetRecvSize(cpu) <= 0x1f) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	pid = req->processId;
	if (pid != 0 && isIllegalProcessId(pid) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	ret = isIllegalThreadListMode(req);
	if (ret != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_VALUE);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
	mode = req->mode;
	if (mode != 1) {
		result = 0;
		if (mode == 0) {
			sdbgpSendErrorHeader(cpu, packet->group, 33, 0, result);
			return ret;
		}
	} else if (req->detail == 1) {
		size = sdbgpGetSyncAllCount(req, work) + 40;
		reply = sdbgpAllocSendBuf(cpu, size);
		if (!reply)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, size);
		sdbgpSetSendSize(cpu, size);
		result = 0;
		reply->processId = req->processId;
		SceDeci4pSDfMgrForKernel_8A3B06F2(reply->info, 0, size);
		sdbgpGetSyncAllInfo(req, reply->info, work);
		sdbgpSendErrorHeader(cpu, packet->group, 33, 0, result);
		return ret;
	}
	reply = sdbgpAllocSendBuf(cpu, 0x87f1c);
	if (!reply)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, 0x87f1c);
	reply->processId = req->processId;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply->info, 0, 0x87f1c);
	result = sdbgpGetSyncInfoList(req, reply->info, work, &size);
	size += sizeof(SdbgpSyncListReply);
	sdbgpSetSendSize(cpu, size);
	sdbgpSendErrorHeader(cpu, packet->group, 33, 0, result);
	return ret;
}

int sdbgpCmd30_10(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	const SdbgpThreadSuspendRequest *req = (const SdbgpThreadSuspendRequest *)packet;
	SdbgpThreadSuspendReply *reply;
	ThreadmgrThreadInfoForDebugger info;
	SceUID tid;
	uint32_t result;
	int ret;

	if (sdbgpGetRecvSize(cpu) <= 0x1b) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	tid = req->threadId;
	if (tid == 0 || isIllegalThreadId(tid) != 0) {
		result = SDBGP_RESULT_ILLEGAL_THREAD_ID;
	} else if (isThreadIsSuspendedAlready(tid) == 0) {
		reply = sdbgpAllocSendBuf(cpu, sizeof(*reply));
		if (!reply)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, sizeof(*reply));
		sdbgpSetSendSize(cpu, sizeof(*reply));
		info.size = sizeof(info);
		ret = SceThreadmgrForKernel_C976719E(tid, 0, &info);
		if (ret < 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1,
				       packet->tag, ret);
			return ret;
		}
		sceKernelCpuResumeIntr(0);
		ret = sceKernelDebugSuspendThread(tid, 1);
		sceKernelCpuSuspendIntr();
		if (ret == (int)0x8002802b) {
			sdbgpSendError(cpu, packet->group, packet->command + 1,
				       packet->tag, 0x8002802b);
			return (int)0x8002802b;
		}
		if (ret < 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1,
				       packet->tag, ret);
			return ret;
		}
		reply->processId = info.processId;
		reply->threadId = info.threadId;
		reply->status = info.status;
		sdbgpSendErrorHeader(cpu, packet->group, 17, 0, 0);
		return 0;
	} else {
		result = SDBGP_RESULT_PROCESS_SUSPENDED;
	}
	sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, result);
	return SDBGP_ERROR_INVALID_ARGUMENT;
}

int sdbgpCmd30_12(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	const SdbgpThreadSuspendRequest *req = (const SdbgpThreadSuspendRequest *)packet;
	SdbgpThreadResumeReply *reply;
	ThreadmgrThreadInfoForDebugger info;
	SceUID tid;
	uint32_t result;
	int ret;

	if (sdbgpGetRecvSize(cpu) <= 0x1b) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	tid = req->threadId;
	if (tid == 0 || isIllegalThreadId(tid) != 0) {
		result = SDBGP_RESULT_ILLEGAL_THREAD_ID;
		goto invalid;
	}
	ret = isThreadNotSuspended(tid);
	if (ret != 0) {
		result = SDBGP_RESULT_THREAD_NOT_SUSPENDED;
		goto invalid;
	}
	info.size = sizeof(info);
	ret = SceThreadmgrForKernel_C976719E(tid, 0, &info);
	if (ret < 0 || (ret = sceKernelDebugResumeThread(tid, 1)) < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, ret);
		return ret;
	}
	reply = sdbgpAllocSendBuf(cpu, sizeof(*reply));
	if (!reply)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, sizeof(*reply));
	sdbgpSetSendSize(cpu, sizeof(*reply));
	reply->processId = info.processId;
	reply->threadId = info.threadId;
	sdbgpSendErrorHeader(cpu, packet->group, 19, 0, 0);
	return 0;

invalid:
	sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, result);
	return SDBGP_ERROR_INVALID_ARGUMENT;
}

int sdbgpCmd30_40(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	const SdbgpThreadPriorityRequest *req = (const SdbgpThreadPriorityRequest *)packet;
	void *reply;
	SceUID tid;
	int ret;

	if (sdbgpGetRecvSize(cpu) <= 0x53) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	tid = req->threadId;
	if (isIllegalThreadId(tid) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_THREAD_ID);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	ret = isIllegalThreadPriority(req);
	if (ret != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_VALUE);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	if ((req->cpuAffinityMask != 0 &&
	     sceKernelChangeThreadCpuAffinityMask(tid, req->cpuAffinityMask) < 0) ||
	    (req->priority != 0 &&
	     sceKernelChangeThreadPriority2(tid, req->priority) < 0)) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_VALUE);
		return SCE_KERNEL_ERROR_THREADMGR_ERROR;
	}
	reply = sdbgpAllocSendBuf(cpu, 0x5c);
	if (!reply)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, 0x5c);
	sdbgpSetSendSize(cpu, 0x5c);
	sdbgpSendErrorHeader(cpu, packet->group, 65, 0, 0);
	return ret;
}

int sdbgpCmd30_50(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	const SdbgpWaypointTraceRequest *req = (const SdbgpWaypointTraceRequest *)packet;
	SdbgpWaypointTraceReply *reply;
	void *work;
	SceUID pid;
	SceSize count;
	SceSize size;
	int ret;

	count = 0;
	size = 0;
	if (sdbgpGetRecvSize(cpu) <= 0x33) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	pid = req->processId;
	if (pid != 0 && isIllegalProcessId(pid) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
	reply = sdbgpAllocSendBuf(cpu, SDBGP_SEND_BUF_SIZE);
	if (!reply)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, SDBGP_SEND_BUF_SIZE);
	ret = sdbgpGetThreadWaypointTrace(&pid, work, reply->info, &count, &size);
	if (ret < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, ret);
		return ret;
	}
	sdbgpSetSendSize(cpu, size + sizeof(SdbgpWaypointTraceReply));
	if (pid == 0)
		pid = SDBGP_KERNEL_PROCESS_ID;
	reply->processId = pid;
	reply->count = count;
	sdbgpSendErrorHeader(cpu, packet->group, 81, 0, 0);
	return 0;
}
