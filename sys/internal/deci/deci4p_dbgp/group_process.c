/*
 * The DBGP process group: the processes the host can see, attaching to and
 * detaching from one, suspending and resuming it or its application, and the
 * memory the host allocates in it (Sony's group_process.c).
 */

#include <kernel/error.h>
#include <appmgr/appmgr_rdrfp.h>
#include <acmgr/acmgr.h>
#include <modulemgr/modulemgr.h>
#include <processmgr/process.h>
#include <processmgr/processmgr.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <sysmem/uid_class.h>
#include <sysmem/uid_guid.h>
#include <kernel/threadmgr.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/threadmgr_debugger.h>
#include <ulobjmgr/ulobjmgr.h>
#include "dbgp_internal.h"

/* the most processes the host is told about */
#define DBGP_PROCESS_MAX	16

/* a request naming a process; attach may add the PRX debug flags */
typedef struct DbgpProcessRequest {
	DbgpPrefix prefix;
	SceUID pid;
	SceUInt32 prxDebugFlag;		/* attach, when sent */
} DbgpProcessRequest;

typedef struct DbgpProcessResponse {
	DbgpResponseHeader header;
	SceUID pid;
} DbgpProcessResponse;

typedef struct DbgpProcessInfoResponse {
	DbgpResponseHeader header;
	int count;
	DbgpProcessInfo info[];		/* each followed by its path and unwind tables */
} DbgpProcessInfoResponse;

static inline int _getProcessInfo(DbgpProcCB *proc, SceUID pid,
			   SceKernelProcessInfo *info, DbgpProcessInfo *out,
			   SceSize *size)
{
	SceKernelModuleInfo modinfo;
	DbgpProcessUnwind *unwind;
	DbgpProcCB *target;
	int result, length, padding;

	*size = 0;
	memset(info, 0, sizeof(*info));
	info->size = sizeof(*info);
	result = sceKernelGetProcessInfo(pid, info);
	if (result < 0)
		return result;
	if ((SceUInt8)info->status == 4)
		return (int)0x80080001;
	if (SceModulemgrForKernel_79E761B5(info->mainModule, out->path, 256) < 0)
		return (int)0x80080001;
	length = strnlen(out->path, 256) + 1;
	padding = length & 3;
	if (padding != 0)
		padding = 4 - padding;
	out->size = sizeof(*out) + length + padding + sizeof(*unwind);
	out->pid = pid;
	out->param = info->param;
	out->cpuAffinityMask = info->cpuAffinity;
	out->entryPoint = info->entryPoint;
	SceModulemgrForKernel_337A3908(info->mainModule, &out->fingerprint);
	out->pathLength = length;
	out->parentPid = info->parentPid;
	*(DbgpName *)out->name = *(DbgpName *)info->name;
	if (out->parentPid == SCE_KERNEL_PROCESS_ID)
		out->parentPid = -1;
	*size = out->size;
	result = procGetProcessAttribute(proc, pid, &out->attr);
	if (result != 0) {
		SCE_PRINTF_WITH_INFO(15, 1594, "0x%08x\n", result);
		return (int)0x80080001;
	}
	target = dbgpGetProcCB(pid);
	if (target == NULL)
		target = proc;
	procGetProcessDbgpStopReason(target, &out->stopReason, pid, 0, -1);
	out->path[out->pathLength - 1] = 0;
	unwind = (DbgpProcessUnwind *)(out->path + length);
	if (padding != 0)
		unwind = (DbgpProcessUnwind *)((char *)unwind + padding);
	if (sceKernelGetModuleInfo(pid, info->mainModule, &modinfo) < 0)
		return (int)0x80080001;
	unwind->exidxTop = modinfo.exidx_top;
	unwind->exidxBtm = modinfo.exidx_btm;
	unwind->extabTop = modinfo.extab_top;
	unwind->extabBtm = modinfo.extab_btm;
	return 0;
}

int processGetProcessInfoCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	DbgpProcessInfoResponse *response;
	SceKernelProcessInfo *info;
	DbgpProcessInfo *out;
	SceUID pids[DBGP_PROCESS_MAX];
	SceSize num = 0, size, recordSize;
	int count, i;

	if (_dbgpGetRecvSize(proc) < sizeof(*packet)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	info = _dbgpAllocWorkBuf(proc, DBGP_WORK_BUF_SIZE);
	if (info == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(pids, 0, sizeof(pids));
	if (SceProcessmgrForKernel_EFFF2975(3, pids, sizeof(pids), &num) < 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 4);
		return (int)0x80080001;
	}
	response = _dbgpAllocSendBuf(proc, DBGP_WORK_BUF_SIZE);
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, DBGP_WORK_BUF_SIZE);
	out = response->info;
	count = 0;
	size = sizeof(*response);
	for (i = 0; i < (int)num; i++) {
		if (pids[i] == SCE_KERNEL_PROCESS_ID)
			continue;
		if (_getProcessInfo(proc, pids[i], info, out, &recordSize) != 0)
			continue;
		out = (DbgpProcessInfo *)((char *)out + recordSize);
		count++;
		size += recordSize;
	}
	response->count = count;
	_dbgpSetSendSize(proc, size);
	dbgpFillResponseHeader(proc, packet->group, 1, 0, 0);
	return 0;
}

int processAttachProcessCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpProcessRequest *request = (const DbgpProcessRequest *)packet;
	DbgpProcCB *target;
	void *response;
	SceSize recvSize;
	SceUID pid;
	int invalid, result;

	recvSize = _dbgpGetRecvSize(proc);
	invalid = recvSize != offsetof(DbgpProcessRequest, prxDebugFlag);
	if (recvSize >= sizeof(*request))
		invalid = 0;
	if (invalid) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->pid;
	if (checkInvalidProcess(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20011);
		return (int)0x80080001;
	}
	result = checkAlreadyAttachedProcess(pid);
	if (result != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20052);
		return (int)0x80080001;
	}
	if (SceSblACMgrForDriver_4CBD6156(pid) == 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20053);
		return (int)0x80080001;
	}
	if (dbgpCreateWorkerThread(pid) < 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 1);
		return (int)0x80080001;
	}
	target = dbgpGetProcCB(pid);
	if (target == NULL)
		return (int)0x80080001;
	target->pid = pid;
	SceUlobjMgrForDriver_332F2E58(pid, dbgpGetDbgpCB(), target, 1, 0);
	if (recvSize >= sizeof(*request))
		target->prxDebugFlag = request->prxDebugFlag;
	response = _dbgpAllocSendBuf(proc, sizeof(DbgpResponseHeader));
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, sizeof(DbgpResponseHeader));
	_dbgpSetSendSize(proc, sizeof(DbgpResponseHeader));
	dbgpFillResponseHeader(proc, packet->group, 0x11, 0, 0);
	return result;
}

int processSuspendApplicationCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpProcessRequest *request = (const DbgpProcessRequest *)packet;
	DbgpProcessResponse *response;
	DbgpCB *dbgp;
	SceUID pid;
	int result, ret;

	dbgp = dbgpGetDbgpCB();
	if (_dbgpGetRecvSize(proc) < offsetof(DbgpProcessRequest, prxDebugFlag)) {
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
	if (checkApplicationIsSuspendedAlready(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20080);
		return (int)0x80080001;
	}
	result = checkProcessIsSuspendedAlready(pid);
	if (result != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20056);
		return (int)0x80080001;
	}
	if (dbgpTestStatus(dbgp, 4) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20058);
		return (int)0x80080001;
	}
	dbgpSetProcStatus(proc, 0x40000, 1);
	ret = sceAppMgrSuspendAppForDebugger(pid);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 940, "0x%08x\n", ret);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20081);
		dbgpSetProcStatus(proc, 0x40000, 0);
		return (int)0x80080001;
	}
	response = _dbgpAllocSendBuf(proc, sizeof(*response));
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, sizeof(*response));
	_dbgpSetSendSize(proc, sizeof(*response));
	response->pid = pid;
	dbgpFillResponseHeader(proc, packet->group, 0x25, 0, 0);
	return result;
}

int processResumeApplicationCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpProcessRequest *request = (const DbgpProcessRequest *)packet;
	DbgpProcessResponse *response;
	SceUID pid;
	int result, ret;

	if (_dbgpGetRecvSize(proc) < offsetof(DbgpProcessRequest, prxDebugFlag)) {
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
	result = checkIllegalApplicationStatus(pid);
	if (result != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20081);
		return (int)0x80080001;
	}
	dbgpSetProcStatus(proc, 0x40020, 0);
	ret = sceAppMgrResumeAppForDebugger(pid);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 1042, "0x%08x\n", ret);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20081);
		dbgpSetProcStatus(proc, 0x40020, 1);
		return (int)0x80080001;
	}
	response = _dbgpAllocSendBuf(proc, sizeof(*response));
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, sizeof(*response));
	_dbgpSetSendSize(proc, sizeof(*response));
	response->pid = pid;
	dbgpFillResponseHeader(proc, packet->group, 0x27, 0, 0);
	return result;
}

/* a page the host asks to have mapped in the process */
typedef struct DbgpAllocPageRequest {
	DbgpPrefix prefix;
	SceUID pid;
	SceSize size;			/* a multiple of 1 MiB */
} DbgpAllocPageRequest;

typedef struct DbgpAllocPageResponse {
	DbgpResponseHeader header;
	SceUID pid;
	void *base;
} DbgpAllocPageResponse;

/* the records of the executable pages the host allocated */
#define DBGP_PROCESS_DEBUG_INFO_MAX	128

int processAllocCodePageCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpAllocPageRequest *request =
		(const DbgpAllocPageRequest *)packet;
	SceKernelAllocMemBlockKernelOpt opt;
	DbgpAllocPageResponse *response;
	DbgpProcessDebugInfo *entry;
	DbgpCB *dbgp;
	SceUID pid, memid;
	SceSize size;
	void *base;
	char *clear;
	int result, i;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->pid;
	size = request->size;
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
	if (checkIllegalProcessStatus(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20050);
		return (int)0x80080001;
	}
	result = checkInvalidAllocPageSize(size);
	if (result != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20055);
		return (int)0x80080001;
	}
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.pid = pid;
	opt.attr = 0x80;
	memid = sceKernelAllocMemBlockForKernel("SceDbgpAllocExecPage", 0x0cf0d050,
						 size, &opt);
	if (memid < 0 || sceKernelGetMemBlockBase(memid, &base) < 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 1);
		return (int)0x80080001;
	}
	dbgp = dbgpGetDbgpCB();
	if (dbgp->processDebugInfo != NULL) {
		for (i = 0; i < DBGP_PROCESS_DEBUG_INFO_MAX; i++) {
			entry = &dbgp->processDebugInfo[i];
			if (entry->pid == 0) {
				entry->pid = pid;
				entry->codeBase = base;
				entry->codeSize = size;
				break;
			}
		}
	} else {
		memid = sceKernelAllocMemBlockForKernel("SceDbgpAllocExecInfo",
							 0x10f0d006, 0x1000, NULL);
		if (memid < 0 ||
		    sceKernelGetMemBlockBase(memid,
					     (void **)&dbgp->processDebugInfo) < 0) {
			dbgpSendError(proc, packet->group, packet->type + 1,
				      packet->code, 0x20000);
			return (int)0x80080001;
		}
		/* Sony clears only the size of the pointer, not the table */
		clear = (char *)dbgp->processDebugInfo;
		for (i = 0; i < (int)sizeof(dbgp->processDebugInfo); i++)
			clear[i] = 0;
		entry = dbgp->processDebugInfo;
		entry->pid = pid;
		entry->codeBase = base;
		entry->codeSize = size;
	}
	response = _dbgpAllocSendBuf(proc, sizeof(*response));
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, sizeof(*response));
	response->pid = pid;
	response->base = base;
	_dbgpSetSendSize(proc, sizeof(*response));
	dbgpFillResponseHeader(proc, packet->group, 0x31, 0, 0);
	return result;
}

int processAllocDataPageCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpAllocPageRequest *request =
		(const DbgpAllocPageRequest *)packet;
	SceKernelAllocMemBlockKernelOpt opt;
	DbgpAllocPageResponse *response;
	SceUID pid, memid;
	SceSize size;
	void *base;
	int result;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->pid;
	size = request->size;
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
	if (checkIllegalProcessStatus(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20050);
		return (int)0x80080001;
	}
	result = checkInvalidAllocPageSize(size);
	if (result != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20055);
		return (int)0x80080001;
	}
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.pid = pid;
	opt.attr = 0x80;
	memid = sceKernelAllocMemBlockForKernel("SceDbgpAllocDataPage", 0x0cf0d060,
						 size, &opt);
	if (memid < 0 || sceKernelGetMemBlockBase(memid, &base) < 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 1);
		return (int)0x80080001;
	}
	response = _dbgpAllocSendBuf(proc, sizeof(*response));
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, sizeof(*response));
	response->pid = pid;
	response->base = base;
	_dbgpSetSendSize(proc, sizeof(*response));
	dbgpFillResponseHeader(proc, packet->group, 0x33, 0, 0);
	return result;
}

/* a memory block of the process, as the host sees it */
typedef struct DbgpMemBlockInfo {
	SceSize size;
	SceUID uid;			/* the process's ID for it */
	DbgpName name;			/* 0x08 */
	SceUInt32 type;			/* 0x28 */
	uintptr_t vbase;
	SceSize blockSize;		/* 0x30 */
	SceUInt32 reserved[2];
	SceSize mapSize;		/* 0x3c */
	SceSize headFree;
	SceSize tailFree;		/* 0x44 */
} DbgpMemBlockInfo;

typedef struct DbgpMemBlockInfoResponse {
	DbgpResponseHeader header;
	SceUID pid;
	SceSize count;			/* 0x20 */
	DbgpMemBlockInfo info[];
} DbgpMemBlockInfoResponse;

_Static_assert(sizeof(DbgpMemBlockInfo) == 72, "DBGP memory block record");

/* the memblock IDs scePUIDGetUIDVectorByClass returns (bit 30 is a PUID) */
#define DBGP_PUID_FLAG		0x40000000

int processGetMemBlockInfoCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpProcessRequest *request = (const DbgpProcessRequest *)packet;
	DbgpMemBlockInfoResponse *response;
	DbgpMemBlockInfo *out;
	MemBlockPhyInfo info;
	SceUID *uids;
	SceUID pid, guid;
	SceSize num, n, count, size, i;
	int result;

	if (_dbgpGetRecvSize(proc) < offsetof(DbgpProcessRequest, prxDebugFlag)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->pid;
	uids = _dbgpAllocWorkBuf(proc, DBGP_WORK_BUF_SIZE);
	if (uids == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
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
	if (result == 0) {
		num = 0;
		if (scePUIDGetUIDVectorByClass(pid, SceSysmemForKernel_86681B64(), 3,
					       uids,
					       DBGP_WORK_BUF_SIZE / sizeof(SceUID),
					       &num) >= 0)
			goto list;
	}
	dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
		      0x20050);
	return (int)0x80080001;
list:
	n = num;
	size = sizeof(*response) + n * sizeof(DbgpMemBlockInfo);
	response = _dbgpAllocSendBuf(proc, size);
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, size);
	count = 0;
	out = response->info;
	for (i = 0; i < n; i++, out++) {
		if ((uids[i] & DBGP_PUID_FLAG) == 0)
			continue;
		guid = scePUIDtoGUID(pid, uids[i]);
		if (guid < 0)
			continue;
		info.ex.size = sizeof(info);
		if (sceKernelGetMemBlockInfo(guid, 3, &info) < 0)
			continue;
		out->size = sizeof(*out);
		out->uid = uids[i];
		out->type = info.ex.details.type;
		out->vbase = info.ex.details.vbase;
		out->blockSize = info.ex.details.size;
		out->reserved[0] = 0;
		out->reserved[1] = 0;
		out->mapSize = info.ex.details.mapSize;
		out->headFree = info.ex.details.headFree;
		out->tailFree = info.ex.details.tailFree;
		out->name = *(const DbgpName *)info.ex.details.name;
		count++;
	}
	if (count > n)
		count = n;
	response->pid = pid;
	response->count = count;
	_dbgpSetSendSize(proc, sizeof(*response) + count * sizeof(DbgpMemBlockInfo));
	dbgpFillResponseHeader(proc, packet->group, 0x41, 0, 0);
	return result;
}

/* what a process's suspension leaves in the work buffer */
typedef struct DbgpSuspendWork {
	ThreadmgrDebugRegisters regs[2];	/* the process information before them */
	SceUID running[DBGP_THREAD_MAX];	/* 0x90: threads not debug-suspended */
	ThreadmgrThreadInfoForDebugger threads[];	/* 0x390 */
} DbgpSuspendWork;

typedef struct DbgpSuspendResponse {
	DbgpResponseHeader header;
	int threadCount;
	int runningCount;		/* 0x20 */
	DbgpProcessInfo process;	/* 0x24, then the thread records */
} DbgpSuspendResponse;

_Static_assert(offsetof(DbgpSuspendWork, threads) == 0x390,
	       "DBGP suspend work buffer");

#define DBGP_SUSPEND_THREADS_SIZE \
	(DBGP_WORK_BUF_SIZE - offsetof(DbgpSuspendWork, threads))

static int _getSuspendThreadInfo(DbgpProcCB *proc, SceUID pid,
					DbgpSuspendWork *work,
					DbgpSuspendResponse *response)
{
	ThreadmgrThreadInfoForDebugger *thread;
	DbgpThreadInfo *records, *out;
	SceSize processSize = 0, num, offset;
	SceUID thid;
	int result, status, count, running;
	SceSize i;

	result = _getProcessInfo(proc, pid, (SceKernelProcessInfo *)work,
				 &response->process, &processSize);
	if (result < 0)
		return result;
	num = 0;
	offset = processSize + offsetof(DbgpSuspendResponse, process);
	processSize = 0;
	records = (DbgpThreadInfo *)((char *)response + offset);
	memset(work->threads, 0, DBGP_SUSPEND_THREADS_SIZE);
	result = SceThreadmgrForDriver_F2C9DC97(pid, 0, work->threads,
						DBGP_SUSPEND_THREADS_SIZE, &num);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1680, "0x%08x\n", result);
		return result;
	}
	count = 0;
	running = 0;
	thread = work->threads;
	for (i = 0; i < num; i++, thread++) {
		thid = scePUIDtoGUID(pid, thread->threadId);
		if (thid < 0)
			continue;
		status = sceKernelIsThreadDebugSuspended(thid);
		if (status == SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID)
			continue;
		if (status < 0) {
			SCE_PRINTF_WITH_INFO(15, 1697, "0x%08x\n", status);
			return status;
		}
		if (status == 0) {
			work->running[running++] = thread->threadId;
			continue;
		}
		result = SceThreadmgrForDriver_5022689D(thid, work->regs);
		if (result == SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID)
			continue;
		out = &records[count];
		if (result < 0) {
			out->pc = 0;
		} else if (thread->userFrame != 0) {
			out->pc = work->regs[1].pc;
			out->status |= DBGP_THREAD_STATUS_SYSCALL;
		} else {
			out->pc = work->regs[0].pc;
		}
		if (procGetThreadDbgpStopReason(proc, &out->stopReason, thid, pid,
						0) ==
		    SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID) {
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
		if (thid == proc->currentThread) {
			out->waitType = 0;
			out->waitId = 0;
			out->status |= DBGP_THREAD_STATUS_STOPPED;
		} else {
			out->status |= thread->status;
			out->waitType = thread->waitType;
			out->waitId = thread->waitId;
		}
		*(DbgpName *)out->name = *(DbgpName *)thread->name;
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
		count++;
	}
	/* Sony copies the running list into the work buffer, not the packet */
	if (running != 0)
		memcpy(&work->threads[count], work->running,
		       running * sizeof(SceUID));
	_dbgpSetSendSize(proc, offset + count * sizeof(DbgpThreadInfo) +
			 running * sizeof(SceUID));
	response->process.pid = pid;
	response->threadCount = count;
	response->runningCount = running;
	return 0;
}

int processSuspendProcessCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpProcessRequest *request = (const DbgpProcessRequest *)packet;
	DbgpSuspendResponse *response;
	DbgpSuspendWork *work;
	SceUID pid;
	int result, count;

	if (_dbgpGetRecvSize(proc) < offsetof(DbgpProcessRequest, prxDebugFlag)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->pid;
	if (checkInvalidProcess(pid) != 0 || request->pid == SCE_KERNEL_PROCESS_ID) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20011);
		return (int)0x80080001;
	}
	if (checkAttachedProcess(proc, pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20051);
		return (int)0x80080001;
	}
	if (checkProcessIsSuspendedAlready(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20056);
		return (int)0x80080001;
	}
	if (checkApplicationIsSuspended(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20082);
		return (int)0x80080001;
	}
	work = _dbgpAllocWorkBuf(proc, DBGP_WORK_BUF_SIZE);
	if (work == NULL)
		return (int)0x8008000b;
	result = sceKernelProcessDebugSuspend(pid, 17);
	if (result < 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20058);
		return result;
	}
	dbgpSetProcStatus(proc, 0x10, 1);
	processWaitThreadSuspendForProcess(pid, (SceUID *)work, 1);
	count = excpInfoGetCount(proc);
	if (count > 0) {
		SCE_PRINTF_WITH_INFO(15, 575, "0x%08x\n", count);
		excpInfoClearAll(proc, pid);
	}
	response = _dbgpAllocSendBuf(proc, DBGP_WORK_BUF_SIZE);
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, DBGP_WORK_BUF_SIZE);
	result = _getSuspendThreadInfo(proc, pid, work, response);
	if (result < 0)
		return result;
	dbgpFillResponseHeader(proc, packet->group, 0x21, 0, 0);
	return 0;
}

__attribute__((noinline, noclone))
int processWaitThreadSuspendForProcess(SceUID pid, SceUID *threads, int mode)
{
	ThreadmgrThreadInfoForDebugger *info;
	SceSize num, size, i;
	SceUID thid;
	int result, status;

	result = sceKernelGetThreadIdList(pid, NULL, 0, NULL);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1829, "0x%08x\n", result);
		return result;
	}
	size = result * sizeof(SceUID);
	num = result;
	memset(threads, 0, size);
	result = sceKernelGetThreadIdList(pid, threads, size, &num);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1842, "0x%08x\n", result);
		return result;
	}
	/* Sony scales the list size by the record size: far past the list */
	info = (ThreadmgrThreadInfoForDebugger *)threads + size;
	if (mode == 2) {
		for (i = 0; i < num; i++) {
			thid = threads[i];
			status = sceKernelIsThreadDebugSuspended(thid);
			if (status == SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID)
				continue;
			if (status < 0)
				return status;
			if (status != 0) {
				if ((status & 0x100) == 0)
					continue;
			} else {
				memset(info, 0, sizeof(*info));
				info->size = sizeof(*info);
				result = SceThreadmgrForDriver_555CA6BB(thid, status, info);
				if (result == SCE_KERNEL_ERROR_UID_NOT_VISIBLE)
					continue;
				if (result < 0)
					return 0;
				if (info->status == 0x10)
					return SCE_KERNEL_ERROR_DELETED;
				if (info->status == 0x100)
					continue;
			}
			i = -1;
		}
		return 0;
	}
	for (i = 0; i < num; i++) {
		thid = threads[i];
		status = sceKernelIsThreadDebugSuspended(thid);
		if (status == SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID)
			continue;
		if (status < 0)
			return status;
		if (status != 0) {
			if ((status & 0x100) == 0)
				continue;
		} else {
			memset(info, 0, sizeof(*info));
			info->size = sizeof(*info);
			result = SceThreadmgrForDriver_555CA6BB(thid, status, info);
			if (result == SCE_KERNEL_ERROR_UID_NOT_VISIBLE)
				continue;
			if (result < 0)
				return 0;
			if (info->status == 0x100)
				continue;
		}
		i = -1;
	}
	return 0;
}

int processChangeStatusThreadResumeForProcess(SceUID pid, SceUID *threads)
{
	SceSize num;
	SceUID thid;
	int count, result, status, i;

	count = sceKernelGetThreadIdList(pid, NULL, 0, NULL);
	if (count < 0) {
		SCE_PRINTF_WITH_INFO(15, 1925, "0x%08x\n", count);
		return count;
	}
	memset(threads, 0, count * sizeof(SceUID));
	result = sceKernelGetThreadIdList(pid, threads, count * sizeof(SceUID),
					  &num);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1938, "0x%08x\n", result);
		return result;
	}
	for (i = 0; i < count; i++) {
		thid = threads[i];
		status = sceKernelIsThreadDebugSuspended(thid);
		if (status == SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID)
			continue;
		if (status < 0) {
			SCE_PRINTF_WITH_INFO(15, 1954, "0x%08x\n", status);
			return status;
		}
		if ((status & 1) != 0)
			continue;
		result = SceThreadmgrForDriver_04C6764B(thid, status & 3);
		if (result != SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID && result < 0) {
			SCE_PRINTF_WITH_INFO(15, 1971, "0x%08x\n", result);
			return result;
		}
	}
	return 0;
}

int processResumeProcessCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpThreadListRequest *request =
		(const DbgpThreadListRequest *)packet;
	DbgpThreadListResponse *response;
	SceUInt32 status;
	SceUInt32 count, i;
	SceUID *work;
	SceUID pid;
	int isBreakpoint = 0, result;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->header.pid;
	count = request->count;
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
	result = checkIllegalProcessStatusResumeProcess(pid);
	if (result != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20050);
		return (int)0x80080001;
	}
	for (i = 0; i < count; i++) {
		if (checkInvalidThread(pid, request->thid[i]) != 0) {
			dbgpSendError(proc, packet->group, packet->type + 1,
				      packet->code, 0x20012);
			return (int)0x80080001;
		}
	}
	for (i = 0; i < count; i++) {
		if (checkIllegalThreadStatusResumeProcess(pid, request->thid[i],
							  &isBreakpoint) != 0) {
			dbgpSendError(proc, packet->group, packet->type + 1,
				      packet->code, 0x20060);
			return (int)0x80080001;
		}
	}
	if (dbgpTestProcStatus(proc, 0x80000) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20050);
		return (int)0x80080001;
	}
	if (isBreakpoint != 0) {
		/* step the threads off their breakpoints first */
		dbgpSetProcStatus(proc, 0x1000, 1);
		proc->skipIndex = 0;
		result = stepAutoSkipBpPre(proc, request);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 734, "0x%08x\n", result);
			dbgpSetProcStatus(proc, 0x8000, 1);
			return result;
		}
		return 0;
	}
	work = _dbgpAllocWorkBuf(proc, DBGP_WORK_BUF_SIZE);
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	if (excpInfoGetCount(proc) != 0) {
		/* another stop is queued: report it instead of resuming */
		dbgpSetProcStatus(proc, 0x20, 0);
		dbgpSetProcStatus(proc, 0x2, 1);
		result = sceKernelSetEventFlag(proc->eventFlag, 1);
		if (result < 0)
			return result;
	} else {
		result = processWaitThreadSuspendForProcess(pid, work, 1);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 761, "0x%08x\n", result);
			return (int)0x80080001;
		}
		result = processChangeStatusThreadResumeForProcess(pid, work);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 767, "0x%08x\n", result);
			return (int)0x80080001;
		}
		dbgpSetProcStatus(proc, 0x30, 0);
		result = sceKernelGetProcessStatus(pid, &status);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 779, "0x%08x\n", result);
			return result;
		}
		status = (SceUInt8)status;
		result = sceKernelProcessDebugResume(pid, status);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 787, "0x%08x\n", result);
			dbgpSendError(proc, packet->group, packet->type + 1,
				      packet->code, 4);
			return result;
		}
		if (dbgpTestProcStatus(proc, 0x100000) != 0)
			dbgpClearPrxDebug(proc);
	}
	response = _dbgpAllocSendBuf(proc, sizeof(*response));
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, sizeof(*response));
	_dbgpSetSendSize(proc, sizeof(*response));
	response->pid = pid;
	response->count = 0;
	dbgpFillResponseHeader(proc, packet->group, 0x23, 0, 0);
	return 0;
}

int processThreadStatusIntegration(SceUID pid, int mode, SceUID *threads)
{
	SceSize num;
	int count, result, i;

	count = sceKernelGetThreadIdList(pid, NULL, 0, NULL);
	if (count < 0) {
		SCE_PRINTF_WITH_INFO(15, 1992, "0x%08x\n", count);
		return count;
	}
	memset(threads, 0, count * sizeof(SceUID));
	result = sceKernelGetThreadIdList(pid, threads, count * sizeof(SceUID),
					  &num);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 2005, "0x%08x\n", result);
		return result;
	}
	for (i = 0; i < count; i++) {
		result = SceThreadmgrForDriver_04C6764B(threads[i], mode);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 2018, "0x%08x\n", result);
			return result;
		}
	}
	return 0;
}

/* detach; resume asks for the process to run again afterwards */
typedef struct DbgpDetachRequest {
	DbgpRequestHeader header;
	SceUInt32 resume;
} DbgpDetachRequest;

int processDetachProcessCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpDetachRequest *request = (const DbgpDetachRequest *)packet;
	DbgpProcessDebugInfo *info;
	DbgpProcCB *target;
	DbgpCB *dbgp;
	void *response;
	SceUID *work;
	SceUID pid;
	char *clear;
	int alreadySuspended, deleted, result, ret, i, j;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->header.pid;
	result = checkInvalidProcess(pid);
	if (result != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20011);
		return (int)0x80080001;
	}
	if (checkAlreadyAttachedProcess(pid) != 0x20052) {
		/* Sony returns the error code it sends */
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20051);
		return 0x20051;
	}
	target = dbgpGetProcCB(pid);
	if (target == NULL)
		return (int)0x80080001;
	work = _dbgpAllocWorkBuf(proc, DBGP_WORK_BUF_SIZE);
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	deleted = 0;
	ret = processSuspendProcessByDBGP(pid, &alreadySuspended);
	if (ret < 0) {
		if (ret != SCE_KERNEL_ERROR_WAIT_DELETE)
			return (int)0x80080001;
		/* the process is going */
		alreadySuspended = 0;
		deleted = 1;
	}
	ret = processWaitThreadSuspendForProcess(pid, work, 1);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 360, "0x%08x\n", ret);
		goto error;
	}
	ret = swbkptClearSwbrkptDetach(target, pid);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 368, "0x%08x\n", ret);
		goto error;
	}
	ret = hwwtptClearHwwtpt(target, pid, -1, deleted);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 376, "0x%08x\n", ret);
		goto error;
	}
	dbgp = dbgpGetDbgpCB();
	if (dbgp->processDebugInfo != NULL) {
		for (i = 0; i < DBGP_PROCESS_DEBUG_INFO_MAX; i++) {
			info = &dbgp->processDebugInfo[i];
			if (info->pid != pid)
				continue;
			clear = (char *)info;
			for (j = 0; j < (int)sizeof(*info); j++)
				clear[j] = 0;
		}
	}
	SceUlobjMgrForDriver_332F2E58(pid, dbgp, target, 0, 1);
	if (request->resume == 1) {
		if (processThreadStatusIntegration(pid, 2, work) < 0) {
			result = (int)0x80080001;
			goto resume;
		}
		if (processResumeProcessByDBGP(pid, 0) < 0)
			return (int)0x80080001;
		alreadySuspended = 1;
	}
	response = _dbgpAllocSendBuf(proc, sizeof(DbgpResponseHeader));
	if (response == NULL) {
		result = SCE_KERNEL_ERROR_NO_MEMORY;
		goto resume;
	}
	memset(response, 0, sizeof(DbgpResponseHeader));
	_dbgpSetSendSize(proc, sizeof(DbgpResponseHeader));
	dbgpFillResponseHeader(proc, packet->group, 0x13, 0, 0);
	if (deleted == 0 && processResumeProcessByDBGP(pid, alreadySuspended) < 0)
		return (int)0x80080001;
	if (dbgpDeleteWorkerThread(pid) < 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 4);
		return (int)0x80080001;
	}
	return result;

error:
	result = (int)0x80080001;
resume:
	processResumeProcessByDBGP(pid, alreadySuspended);
	return result;
}

inline int processSuspendProcessByDBGP(SceUID pid, int *alreadySuspended)
{
	SceUInt32 status;
	int result;

	result = sceKernelGetProcessStatus(pid, &status);
	if (result < 0)
		return result;
	status = (SceUInt8)status;
	if ((status & 0x10) != 0) {
		*alreadySuspended = 1;
		return 0;
	}
	result = sceKernelProcessDebugSuspend(pid, 17);
	if (result < 0)
		return result;
	*alreadySuspended = 0;
	return 0;
}

inline int processResumeProcessByDBGP(SceUID pid, int alreadySuspended)
{
	SceUInt32 status;
	int result;

	if (alreadySuspended != 0)
		return 0;
	result = sceKernelGetProcessStatus(pid, &status);
	if (result < 0)
		return result;
	status = (SceUInt8)status;
	if ((status & 0x10) == 0)
		return (int)0x80080001;
	result = sceKernelProcessDebugResume(pid, status);
	if (result < 0)
		return result;
	return 0;
}

inline void processClearDebugInfo(SceUID pid)
{
	DbgpProcessDebugInfo *info;
	DbgpCB *dbgp;
	char *clear;
	int i, j;

	dbgp = dbgpGetDbgpCB();
	if (dbgp->processDebugInfo == NULL)
		return;
	for (i = 0; i < DBGP_PROCESS_DEBUG_INFO_MAX; i++) {
		info = &dbgp->processDebugInfo[i];
		if (info->pid != pid)
			continue;
		clear = (char *)info;
		for (j = 0; j < (int)sizeof(*info); j++)
			clear[j] = 0;
	}
}
