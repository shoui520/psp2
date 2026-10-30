/*
 * The DBGP core dump group: start a core dump of an attached process, or
 * cancel one (Sony's group_coredump.c).
 */

#include <acmgr/acmgr.h>
#include <coredump/coredump.h>
#include <kernel/error.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysroot.h>
#include "dbgp_internal.h"

/* the dump levels the debugger asks for */
#define DBGP_COREDUMP_LEVEL_FULL	0x1ef0
#define DBGP_COREDUMP_LEVEL_MINI	0xf

/* the trigger data types SceCoredump takes */
#define DBGP_COREDUMP_DATA_NONE		0
#define DBGP_COREDUMP_DATA_TYPE1	1
#define DBGP_COREDUMP_DATA_TYPE2	2
#define DBGP_COREDUMP_DATA_TYPE10	10

typedef struct DbgpCoredumpRequest {
	DbgpPrefix prefix;
	SceUID pid;
	int taskId;			/* the cancel command */
	SceUInt32 triggerDataType;
	SceSize triggerDataSize;	/* 0x20 */
	SceUInt8 triggerData[];		/* then the dump level, word aligned */
} DbgpCoredumpRequest;

typedef struct DbgpCoredumpResponse {
	DbgpResponseHeader header;
	SceUID pid;
	int taskId;			/* 0x20 */
} DbgpCoredumpResponse;

_Static_assert(sizeof(DbgpCoredumpRequest) == 36, "DBGP core dump request");
_Static_assert(sizeof(DbgpCoredumpResponse) == 36, "DBGP core dump response");

int coredumpTriggerCoredumpCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpCoredumpRequest *request = (const DbgpCoredumpRequest *)packet;
	DbgpCoredumpResponse *response;
	SceCoredumpTriggerParam param;
	SceSize recvSize, size, dataSize;
	const SceUInt32 *levelp;
	SceUInt32 level;
	SceUID pid;
	int result, taskId;

	recvSize = _dbgpGetRecvSize(proc);
	pid = request->pid;
	if (recvSize < offsetof(DbgpCoredumpRequest, triggerDataSize) ||
	    recvSize < sizeof(*request) + ((request->triggerDataSize + 3) & ~3)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	if (checkInvalidProcess(request->pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20011);
		return (int)0x80080001;
	}
	if (checkAttachedProcess(proc, pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20051);
		return (int)0x80080001;
	}
	if (SceSblACMgrForDriver_4CBD6156(pid) == 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20053);
		return (int)0x80080001;
	}
	result = checkIllegalProcessStatus(pid);
	if (result != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20050);
		return (int)0x80080001;
	}
	size = request->triggerDataSize;
	dataSize = (size + 3) & ~3;
	param.size = sizeof(param);
	param.dumpLevel = 0;
	param.triggerDataType = 0;
	param.triggerDataSize = 0;
	param.triggerData = NULL;
	param.titleid_len = 0;
	param.titleid = NULL;
	param.app_name_len = 0;
	param.app_name = NULL;
	param.appVersion = 0;
	param.triggerReason = -1;
	param.crash_thid = 0;
	param.triggerFlags = 0;
	switch (request->triggerDataType) {
	case DBGP_COREDUMP_DATA_NONE:
		break;
	case DBGP_COREDUMP_DATA_TYPE1:
		param.triggerDataType = DBGP_COREDUMP_DATA_TYPE1;
		break;
	case DBGP_COREDUMP_DATA_TYPE2:
		param.triggerDataType = DBGP_COREDUMP_DATA_TYPE2;
		break;
	case DBGP_COREDUMP_DATA_TYPE10:
		param.triggerDataType = DBGP_COREDUMP_DATA_TYPE10;
		break;
	default:
		goto invalid;
	}
	param.triggerDataSize = size;
	param.triggerData = request->triggerData;
	if (recvSize >= sizeof(*request) + dataSize + sizeof(level)) {
		levelp = (const void *)(request->triggerData + dataSize);
		level = *levelp;
		if (level == 0)
			param.dumpLevel = DBGP_COREDUMP_LEVEL_FULL;
		else if (level == 1)
			param.dumpLevel = DBGP_COREDUMP_LEVEL_MINI;
		else
			goto invalid;
	}
	dbgpSetProcStatus(proc, DBGP_PROC_COREDUMP, 1);
	taskId = sceKernelSysrootCoredumpTrigger(pid,
						 coredumpHandlerProgressCallback,
						 coredumpHandlerResultCallback,
						 &param);
	if (taskId < 0) {
		SCE_PRINTF_WITH_INFO(15, 192, "0x%08x\n", taskId);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 7);
		dbgpSetProcStatus(proc, DBGP_PROC_COREDUMP, 0);
		return taskId;
	}
	response = _dbgpAllocSendBuf(proc, sizeof(*response));
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, sizeof(*response));
	_dbgpSetSendSize(proc, sizeof(*response));
	dbgpFillResponseHeader(proc, packet->group, 1, 0, 0);
	response->pid = pid;
	response->taskId = taskId;
	return 0;

invalid:
	dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 7);
	return (int)0x80080001;
}

int coredumpCancelCoredumpCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpCoredumpRequest *request = (const DbgpCoredumpRequest *)packet;
	DbgpCoredumpResponse *response;
	SceUID pid;
	int result;

	if (_dbgpGetRecvSize(proc) < offsetof(DbgpCoredumpRequest, triggerDataType)) {
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
	if (SceSblACMgrForDriver_4CBD6156(pid) == 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20053);
		return (int)0x80080001;
	}
	result = checkIllegalProcessStatus(pid);
	if (result != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20050);
		return (int)0x80080001;
	}
	result = sceKernelSysrootCoredumpCancel(request->taskId);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 298, "0x%08x\n", result);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x200e0);
		return result;
	}
	response = _dbgpAllocSendBuf(proc, sizeof(*response));
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, sizeof(*response));
	_dbgpSetSendSize(proc, sizeof(*response));
	dbgpFillResponseHeader(proc, packet->group, 3, 0, 0);
	response->pid = pid;
	response->taskId = request->taskId;
	return 0;
}
