/*
 * The DBGP module group: a process's modules with their segments, a
 * module's metadata, and the libraries a process exports with their entries
 * and clients (Sony's group_module.c).
 */

#include <kernel/error.h>
#include <modulemgr/modulemgr.h>
#include <modulemgr/modulemgr_debugger.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include <sysmem/uid_guid.h>
#include "dbgp_internal.h"

/* the module list and the library list use a larger buffer */
#define DBGP_LIBRARY_BUF_SIZE	0x32000

/* the bit of a UID that marks it a PUID */
#define DBGP_PUID_FLAG		0x40000000

typedef struct DbgpModuleRequest {
	DbgpPrefix prefix;
	SceUID pid;
	SceUID modid;			/* the metadata command */
} DbgpModuleRequest;

typedef struct DbgpModuleInfoResponse {
	DbgpResponseHeader header;
	SceUID pid;
	int count;			/* 0x20 */
	SceUInt32 info[];		/* SceKernelModuleInfoForDebugger records */
} DbgpModuleInfoResponse;

typedef struct DbgpMetaDataResponse {
	DbgpResponseHeader header;
	SceUID pid;
	SceUID modid;			/* 0x20 */
	SceSize size;
	SceUInt8 data[];		/* 0x28 */
} DbgpMetaDataResponse;

/* one library: its entries, its clients and its name follow */
typedef struct DbgpLibraryInfo {
	SceSize size;
	SceUID libid;			/* -1 when not a PUID */
	SceUID modid;
	SceUInt32 attr;
	SceUInt32 reserved;		/* 0x10 */
	SceUInt32 nfunc;
	SceUInt32 nvar;
	SceUInt32 reserved2;		/* never written */
	int clientCount;		/* 0x20 */
	SceKernelLibraryExportEntry entry[];
} DbgpLibraryInfo;

typedef struct DbgpLibraryListResponse {
	DbgpResponseHeader header;
	SceUID pid;
	int count;			/* 0x20 */
	DbgpLibraryInfo info[];
} DbgpLibraryListResponse;

_Static_assert(sizeof(DbgpModuleInfoResponse) == 36, "DBGP module list");
_Static_assert(sizeof(DbgpMetaDataResponse) == 40, "DBGP module metadata");
_Static_assert(sizeof(DbgpLibraryInfo) == 36, "DBGP library record");

static __attribute__((noinline, noclone))
int _getLibraryInfo(DbgpLibraryInfo *out, SceUID pid, SceUID *libids,
			   SceSize num, SceSize *size)
{
	SceKernelLibraryInfoForDebugger info;
	SceKernelLibraryExportEntry *entries;
	SceSize entryNum = 0, clientNum = 0, length, i, j;
	SceUID *list, *clients;
	int count, n;

	list = libids + num;
	*size = 0;
	count = 0;
	for (i = 0; i < num; i++) {
		memset(&info, 0, sizeof(info));
		info.size = sizeof(info);
		if (sceKernelGetLibraryInfoForDebugger(pid, libids[i], &info) < 0)
			continue;
		n = info.nfunc + info.nvar;
		out->nfunc = info.nfunc;
		out->size = (sizeof(*out) / 4 + info.clientCount + n * 2) * 4;
		out->libid = (info.libid & DBGP_PUID_FLAG) != 0 ? info.libid : -1;
		out->modid = info.modid;
		out->nvar = info.nvar;
		out->clientCount = info.clientCount;
		out->attr = info.attr;
		out->reserved = 0;
		entryNum = n;
		*size += out->size;
		entries = (SceKernelLibraryExportEntry *)list;
		memset(entries, 0, n * sizeof(*entries));
		if (sceKernelGetLibraryExportInfoForDebugger(pid, info.libid, entries,
							     &entryNum, 0) < 0) {
			n = info.nfunc + info.nvar;
			for (j = 0; j < (SceSize)n; j++) {
				out->entry[j].nid = 0;
				out->entry[j].entry = NULL;
			}
		} else {
			n = info.nfunc + info.nvar;
			for (j = 0; j < (SceSize)n; j++)
				out->entry[j] = entries[j];
		}
		clients = (SceUID *)&out->entry[n];
		clientNum = info.clientCount;
		memset(list, 0, info.clientCount * sizeof(SceUID));
		if (sceKernelGetLibraryClientList(pid, info.libid, list, &clientNum,
						  0) < 0) {
			for (j = 0; j < (SceSize)info.clientCount; j++)
				*clients++ = 0;
		} else {
			for (j = 0; j < (SceSize)info.clientCount; j++)
				*clients++ = list[j];
		}
		*clients = strnlen(info.libname, 256) + 1;
		count++;
		memset(clients + 1, 0, (strnlen(info.libname, 256) + 4) & ~3);
		memcpy(clients + 1, info.libname, strnlen(info.libname, 256));
		out->size += 4 + ((strnlen(info.libname, 256) + 4) & ~3);
		length = (strnlen(info.libname, 256) + 4) & ~3;
		out = (DbgpLibraryInfo *)((char *)(clients + 1) + length);
		*size += length + 4;
	}
	return count;
}

static inline int _getSegmentInfoSize(SceKernelModuleInfoForDebugger *info,
				      int num)
{
	int size, i;

	size = 0;
	for (i = 0; i < num; i++) {
		size += info->nSegments * sizeof(SceKernelSegmentInfoForDebugger);
		info = (SceKernelModuleInfoForDebugger *)((char *)info + info->size);
	}
	return size;
}

int moduleGetModInfoCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpModuleRequest *request = (const DbgpModuleRequest *)packet;
	DbgpModuleInfoResponse *response;
	SceKernelModuleInfoForDebugger *info;
	SceUID pid;
	SceSize size;
	int num, result, segments;

	if (_dbgpGetRecvSize(proc) < offsetof(DbgpModuleRequest, modid)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->pid;
	if (checkInvalidProcess(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20011);
		return (int)0x80080001;
	}
	result = checkModuleInfoProcess(pid);
	if (result != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20054);
		return (int)0x80080001;
	}
	num = sceKernelGetModuleInfoForDebugger(pid, NULL, NULL);
	info = _dbgpAllocWorkBuf(proc, DBGP_WORK_BUF_SIZE);
	if (info == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(info, 0, DBGP_WORK_BUF_SIZE);
	result = sceKernelGetModuleInfoForDebugger(pid, info, &num);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 104, "0x%08x\n", result);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return result;
	}
	segments = _getSegmentInfoSize(info, num);
	if (segments < 0) {
		SCE_PRINTF_WITH_INFO(15, 113, "0x%08x\n", segments);
		return segments;
	}
	size = sizeof(*response) + num * (sizeof(SceKernelModuleInfoForDebugger) +
					  sizeof(SceKernelModuleExidxInfo)) + segments;
	response = _dbgpAllocSendBuf(proc, size);
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, size);
	_dbgpSetSendSize(proc, size);
	dbgpFillResponseHeader(proc, packet->group, 1, 0, 0);
	response->pid = pid;
	response->count = num;
	sceKernelGetModuleInfoForDebugger(pid,
		(SceKernelModuleInfoForDebugger *)response->info, &num);
	return 0;
}

int moduleGetMetaDataCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpModuleRequest *request = (const DbgpModuleRequest *)packet;
	DbgpMetaDataResponse *response;
	void *metadata = NULL;
	SceSize metaSize = 0, size;
	SceUID pid, modid;
	int result;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->pid;
	modid = request->modid;
	if (checkInvalidProcess(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20011);
		return (int)0x80080001;
	}
	result = checkInvalidModule(pid, modid);
	if (result != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20013);
		return (int)0x80080001;
	}
	result = sceKernelGetMetaDataForDebugger(pid, modid, &metadata, &metaSize);
	if (result < 0 && result != SCE_KERNEL_ERROR_MODULEMGR_NOENT) {
		SCE_PRINTF_WITH_INFO(15, 209, "0x%08x\n", result);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080001;
	}
	size = sizeof(*response) + ((metaSize + 3) & ~3);
	response = _dbgpAllocSendBuf(proc, size);
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, size);
	_dbgpSetSendSize(proc, size);
	dbgpFillResponseHeader(proc, packet->group, 3, 0, 0);
	response->pid = pid;
	response->modid = request->modid;
	response->size = metaSize;
	memcpy(response->data, metadata, metaSize);
	return 0;
}

int moduleGetModuleListCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpModuleRequest *request = (const DbgpModuleRequest *)packet;
	DbgpLibraryListResponse *response;
	SceSize num, size;
	SceUID *libids;
	SceUID pid;
	int result;

	if (_dbgpGetRecvSize(proc) < offsetof(DbgpModuleRequest, modid)) {
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
	if (result != 0)
		goto illegal;
	result = sceKernelGetLibraryList(pid, NULL, NULL);
	if (result < 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      result);
		return (int)0x80080001;
	}
	num = result;
	size = DBGP_LIBRARY_BUF_SIZE;
	response = _dbgpAllocSendBuf(proc, DBGP_LIBRARY_BUF_SIZE);
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, size);
	/* Sony does not check the work buffer */
	libids = _dbgpAllocWorkBuf(proc, DBGP_LIBRARY_BUF_SIZE);
	if (sceKernelGetLibraryList(pid, libids, &num) < 0)
		goto illegal;
	size = 0;
	response->count = _getLibraryInfo(response->info, pid, libids, num,
					  &size);
	response->pid = pid;
	size += sizeof(*response);
	_dbgpSetSendSize(proc, size);
	dbgpFillResponseHeader(proc, packet->group, 0x11, 0, 0);
	return 0;

illegal:
	dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 0x20050);
	return (int)0x80080001;
}
