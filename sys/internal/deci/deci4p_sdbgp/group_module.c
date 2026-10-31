/*
 * SceDeci4pSDbgp (os0:kd/deci4p_sdbgp.skprx): module, library and stub queries
 * (group 0x40, group_module.c).
 */

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <modulemgr/modulemgr.h>
#include <modulemgr/modulemgr_debugger.h>
#include <processmgr/processmgr.h>
#include <sysmem/sysmem.h>
#include "sdbgp_internal.h"

#define SDBGP_ERROR_OUT_OF_MEMORY	((int)0x8002710b)

typedef struct SdbgpModuleRequest {
	SdbgpRequest header;
	SceUID processId;			/* 0x14 */
} SdbgpModuleRequest;

typedef struct SdbgpModuleReply {
	SdbgpHeader header;
	SceUID processId;			/* 0x1c */
	uint32_t count;				/* 0x20 */
	uint8_t data[];				/* 0x24 */
} SdbgpModuleReply;

typedef struct SdbgpLostLibraryInfo {
	SceSize size;				/* 0x00 */
	SceUID pid;				/* 0x04 */
	SceNID libnid;				/* 0x08 */
	SceUID modid;				/* 0x0c */
	uint32_t reserved0;			/* 0x10 */
	char libname[0x100];			/* 0x14 */
	uint32_t reserved1[4];			/* 0x114 */
	uint32_t version;			/* 0x124 */
	uint32_t nfunc;				/* 0x128 */
	uint32_t nvar;				/* 0x12c */
	SceKernelLibraryExportEntry entry[];	/* 0x130 */
} SdbgpLostLibraryInfo;

_Static_assert(sizeof(SdbgpLostLibraryInfo) == 0x130, "SdbgpLostLibraryInfo");

typedef struct SdbgpStubInfo {
	SceSize size;				/* 0x00 */
	SceUID pid;				/* 0x04 */
	SceUID stubid;				/* 0x08 */
	SceUID modid;				/* 0x0c */
	char libname[32];			/* 0x10 */
	SceUID puid;				/* 0x30 */
	uint32_t attr;				/* 0x34 */
	uint32_t reserved[5];			/* 0x38 */
	uint32_t nfunc;				/* 0x4c */
	uint32_t nvar;				/* 0x50 */
	SceUID libPid;				/* 0x54 */
	SceUID libModid;			/* 0x58 */
	SceUID libid;				/* 0x5c */
	SceKernelStubNidEntry entry[];		/* 0x60 */
} SdbgpStubInfo;

_Static_assert(sizeof(SdbgpStubInfo) == 0x60, "SdbgpStubInfo");

typedef struct SdbgpLibraryInfo {
	SceSize size;				/* 0x00 */
	SceUID pid;				/* 0x04 */
	SceUID libid;				/* 0x08 */
	SceUID modid;				/* 0x0c */
	char libname[32];			/* 0x10 */
	SceUID puid;				/* 0x30 */
	uint32_t attr;				/* 0x34 */
	uint32_t reserved[5];			/* 0x38 */
	uint32_t nfunc;				/* 0x4c */
	uint32_t nvar;				/* 0x50 */
	uint32_t clientCount;			/* 0x54 */
	SceKernelLibraryExportEntry entry[];	/* 0x58 */
} SdbgpLibraryInfo;

_Static_assert(sizeof(SdbgpLibraryInfo) == 0x58, "SdbgpLibraryInfo");

int _getLostLibraryInfo(SdbgpLostLibraryInfo *out, SceUID pid,
			const SceKernelLostLibraryEntry *entries, SceSize num,
			SceSize *size)
{
	SceKernelLostLibraryInfo info;
	SceSize i, j;
	int count;
	int result;

	SceDeci4pSDfMgrForKernel_8A3B06F2(&info, 0, sizeof(info));
	info.size = sizeof(info);
	*size = 0;
	if (num == 0)
		return 0;
	count = 0;
	for (i = 0; i < num; i++, entries++) {
		if (sceKernelGetLostLibraryInfo(pid, entries->modid,
						entries->libnid, &info) < 0)
			continue;
		out->libnid = info.libnid;
		out->size = sizeof(*out) +
			    sizeof(SceKernelLibraryExportEntry) * (info.nfunc + info.nvar);
		if (info.modid & 0x40000000)
			out->modid = scePUIDtoGUID(pid, info.modid);
		else
			out->modid = info.modid;
		out->nfunc = info.nfunc;
		out->nvar = info.nvar;
		out->pid = pid;
		out->version = info.version;
		out->reserved1[0] = 0;
		out->reserved1[1] = 0;
		out->reserved1[2] = 0;
		out->reserved1[3] = 0;
		SceDeci4pSDfMgrForKernel_5996E2DB(out->libname, info.libname,
						  0x20);
		SceKernelLibraryExportEntry *entry = out->entry;

		if (pid == SDBGP_KERNEL_PROCESS_ID) {
			for (j = 0; j < out->nfunc; j++, entry++) {
				SceDeci4pSDfMgrForKernel_5996E2DB(&entry->nid,
								  &info.funcnid[j], 4);
				SceDeci4pSDfMgrForKernel_5996E2DB(&entry->entry,
								  &info.funcent[j], 4);
			}
			for (j = 0; j < out->nvar; j++, entry++) {
				SceDeci4pSDfMgrForKernel_5996E2DB(&entry->nid,
								  &info.varnid[j], 4);
				SceDeci4pSDfMgrForKernel_5996E2DB(&entry->entry,
								  &info.varent[j], 4);
			}
		} else {
			for (j = 0; j < out->nfunc; j++, entry++) {
				result = sceKernelCopyFromUserProc(pid, &entry->nid,
								   &info.funcnid[j], 4);
				if (result < 0)
					return result;
				result = sceKernelCopyFromUserProc(pid, &entry->entry,
								   &info.funcent[j], 4);
				if (result < 0)
					return result;
			}
			for (j = 0; j < out->nvar; j++, entry++) {
				result = sceKernelCopyFromUserProc(pid, &entry->nid,
								   &info.varnid[j], 4);
				if (result < 0)
					return result;
				result = sceKernelCopyFromUserProc(pid, &entry->entry,
								   &info.varent[j], 4);
				if (result < 0)
					return result;
			}
		}
		count++;
		out = (SdbgpLostLibraryInfo *)entry;
		*size += sizeof(*out) +
			 sizeof(SceKernelLibraryExportEntry) * (info.nfunc + info.nvar);
	}
	return count;
}

int _getStubInfo(SdbgpStubInfo *out, SceUID pid, const SceUID *stubids,
		 SceSize num, SceSize *size)
{
	SceKernelStubNidEntry *entries = (SceKernelStubNidEntry *)(stubids + num);
	SceUInt32 entryNum = 0;
	SceKernelStubInfoForDebugger info;
	SceSize i, j;
	int count;

	SceDeci4pSDfMgrForKernel_8A3B06F2(&info, 0, sizeof(info));
	*size = 0;
	info.size = sizeof(info);
	if (num == 0)
		return 0;
	count = 0;
	for (i = 0; i < num; i++) {
		if (sceKernelGetStubInfoForDebugger(pid, stubids[i], &info) < 0)
			continue;
		out->size = sizeof(*out) +
			    sizeof(SceKernelStubNidEntry) * (info.nfunc + info.nvar);
		if (info.stubid & 0x40000000) {
			out->stubid = scePUIDtoGUID(pid, info.stubid);
			out->puid = info.stubid;
		} else {
			out->stubid = info.stubid;
			out->puid = -1;
		}
		if (info.modid & 0x40000000)
			out->modid = scePUIDtoGUID(pid, info.modid);
		else
			out->modid = info.modid;
		out->libPid = info.libPid;
		if (info.libModid & 0x40000000)
			out->libModid = scePUIDtoGUID(info.libPid, info.libModid);
		else
			out->libModid = info.libModid;
		if (info.libid & 0x40000000)
			out->libid = scePUIDtoGUID(info.libPid, info.libid);
		else
			out->libid = info.libid;
		out->attr = info.attr;
		out->reserved[0] = 0;
		out->reserved[1] = 0;
		out->reserved[2] = 0;
		out->reserved[3] = 0;
		out->nfunc = info.nfunc;
		out->nvar = info.nvar;
		out->pid = pid;
		out->reserved[4] = 0;
		SceDeci4pSDfMgrForKernel_5996E2DB(out->libname, info.libname,
						  sizeof(out->libname));
		entryNum = info.nfunc + info.nvar;
		*size += sizeof(*out) +
			 sizeof(SceKernelStubNidEntry) * (info.nfunc + info.nvar);
		SceDeci4pSDfMgrForKernel_8A3B06F2(entries, 0,
			sizeof(SceKernelStubNidEntry) * (info.nfunc + info.nvar));
		sceKernelGetStubNidTableForDebugger(pid, info.stubid, entries,
						    &entryNum, 0);
		for (j = 0; j < entryNum; j++)
			out->entry[j] = entries[j];
		out = (SdbgpStubInfo *)&out->entry[entryNum];
		count++;
	}
	return count;
}

int _getLibraryInfo(SdbgpLibraryInfo *out, SceUID pid, const SceUID *libids,
		    SceSize num, SceSize *size)
{
	SceKernelLibraryExportEntry *entries =
		(SceKernelLibraryExportEntry *)(libids + num);
	SceSize entryNum = 0;
	SceSize clientNum = 0;
	SceKernelLibraryInfoForDebugger info;
	SceSize i, j;
	int count;

	SceDeci4pSDfMgrForKernel_8A3B06F2(&info, 0, sizeof(info));
	info.size = sizeof(info);
	*size = 0;
	if (num == 0)
		return 0;
	count = 0;
	for (i = 0; i < num; i++) {
		if (sceKernelGetLibraryInfoForDebugger(pid, libids[i], &info) < 0)
			continue;
		out->size = sizeof(*out) +
			    sizeof(SceKernelLibraryExportEntry) * (info.nfunc + info.nvar) +
			    sizeof(SceUID) * info.clientCount;
		if (info.libid & 0x40000000) {
			out->libid = scePUIDtoGUID(pid, info.libid);
			out->puid = info.libid;
		} else {
			out->libid = info.libid;
			out->puid = -1;
		}
		if (info.modid & 0x40000000)
			out->modid = scePUIDtoGUID(pid, info.modid);
		else
			out->modid = info.modid;
		out->reserved[0] = 0;
		out->reserved[1] = 0;
		out->reserved[2] = 0;
		out->reserved[3] = 0;
		out->nfunc = info.nfunc;
		out->nvar = info.nvar;
		out->attr = info.attr;
		out->pid = pid;
		out->clientCount = info.clientCount;
		out->reserved[4] = 0;
		SceDeci4pSDfMgrForKernel_5996E2DB(out->libname, info.libname,
						  sizeof(out->libname));
		entryNum = info.nfunc + info.nvar;
		*size += sizeof(*out) +
			 sizeof(SceKernelLibraryExportEntry) * entryNum +
			 sizeof(SceUID) * info.clientCount;
		SceDeci4pSDfMgrForKernel_8A3B06F2(entries, 0,
			sizeof(SceKernelLibraryExportEntry) * entryNum);
		sceKernelGetLibraryExportInfoForDebugger(pid, info.libid, entries,
							 &entryNum, 0);
		SceKernelLibraryExportEntry *dst = out->entry;

		for (j = 0; j < (SceSize)(info.nfunc + info.nvar); j++)
			dst[j] = entries[j];
		SceUID *clients = (SceUID *)&dst[info.nfunc + info.nvar];
		SceUID *clientBuf = (SceUID *)entries;

		clientNum = info.clientCount;
		SceDeci4pSDfMgrForKernel_8A3B06F2(clientBuf, 0,
						  sizeof(SceUID) * info.clientCount);
		sceKernelGetLibraryClientList(pid, info.libid, clientBuf,
					      &clientNum, 0);
		for (j = 0; j < (SceSize)info.clientCount; j++) {
			if (clientBuf[j] & 0x40000000)
				clients[j] = scePUIDtoGUID(pid, clientBuf[j]);
			else
				clients[j] = clientBuf[j];
		}
		out = (SdbgpLibraryInfo *)&clients[info.clientCount];
		count++;
	}
	return count;
}

int sdbgpCmd40_00(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	const SdbgpModuleRequest *request = (const SdbgpModuleRequest *)packet;
	SceKernelModuleInfoForDebugger *work, *p;
	SdbgpModuleReply *reply;
	SceUID pid;
	SceSize sendSize;
	SceSize num;
	int segBytes;
	int ret;
	SceSize i;

	if (sdbgpGetRecvSize(cpu) <= 0x17) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	pid = request->processId;
	if (pid == 0) {
		pid = SDBGP_KERNEL_PROCESS_ID;
	} else if (pid != SDBGP_KERNEL_PROCESS_ID &&
		   isIllegalProcessId(request->processId) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	num = sceKernelGetModuleInfoForDebugger(pid, NULL, NULL);
	work = sdbgpAllocWorkBuf(cpu, SDBGP_PACKET_MAX);
	SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_PACKET_MAX);
	ret = sceKernelGetModuleInfoForDebugger(pid, work, (int *)&num);
	if (ret < 0)
		return ret;
	if (!work)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (num != 0) {
		p = work;
		segBytes = 0;
		for (i = 0; i < num; i++) {
			segBytes += sizeof(SceKernelSegmentInfoForDebugger) * p->nSegments;
			p = (SceKernelModuleInfoForDebugger *)((char *)p + p->size);
		}
		if (segBytes < 0)
			return segBytes;
		sendSize = segBytes + 96 * num + sizeof(SdbgpModuleReply);
		if (sendSize > SDBGP_PACKET_MAX) {
			sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
				       SDBGP_RESULT_PACKET_TOO_LARGE);
			return SDBGP_ERROR_PACKET_SIZE;
		}
	} else {
		sendSize = sizeof(SdbgpModuleReply);
	}
	reply = sdbgpAllocSendBuf(cpu, sendSize);
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, sendSize);
	sdbgpSetSendSize(cpu, sendSize);
	reply->processId = pid;
	reply->count = num;
	p = (SceKernelModuleInfoForDebugger *)reply->data;
	sceKernelGetModuleInfoForDebugger(pid, p, (int *)&num);
	if (pid != SDBGP_KERNEL_PROCESS_ID) {
		for (i = 0; (int)i < (int)num; i++) {
			p->modid = scePUIDtoGUID(pid, p->modid);
			p = (SceKernelModuleInfoForDebugger *)((char *)p + p->size);
		}
	}
	sdbgpSendErrorHeader(cpu, packet->group, 0x01, 0, 0);
	return 0;
}

int sdbgpCmd40_10(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	const SdbgpModuleRequest *request = (const SdbgpModuleRequest *)packet;
	SdbgpModuleReply *reply;
	SceUID *work;
	SceUID pid;
	SceSize size, num;
	int err, ret;

	if (sdbgpGetRecvSize(cpu) <= 0x37) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	pid = request->processId;
	err = isIllegalProcessId(pid);
	if (pid == -1)
		pid = SDBGP_KERNEL_PROCESS_ID;
	if (err != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	ret = sceKernelGetLibraryList(pid, NULL, NULL);
	if (ret < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, ret);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	num = ret;
	size = SDBGP_SEND_BUF_SIZE;
	reply = sdbgpAllocSendBuf(cpu, SDBGP_SEND_BUF_SIZE);
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, size);
	work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
	ret = sceKernelGetLibraryList(pid, work, &num);
	if (ret < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, ret);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	size = 0;
	reply->count = _getLibraryInfo((SdbgpLibraryInfo *)reply->data, pid,
				       work, num, &size);
	reply->processId = pid;
	size += sizeof(SdbgpModuleReply);
	sdbgpSetSendSize(cpu, size);
	sdbgpSendErrorHeader(cpu, packet->group, 0x11, 0, 0);
	return 0;
}

int sdbgpCmd40_12(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	const SdbgpModuleRequest *request = (const SdbgpModuleRequest *)packet;
	SdbgpModuleReply *reply;
	SceKernelLostLibraryEntry *work;
	SceUID pid;
	SceSize procCount = 0;
	SceSize oneSize = 0, num, size;
	int total = 0;
	int err, ret;

	if (sdbgpGetRecvSize(cpu) <= 0x37) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	pid = request->processId;
	if (pid == -1) {
		SceUID *pids = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);

		work = (SceKernelLostLibraryEntry *)(pids + 128);
		ret = SceProcessmgrForKernel_EFFF2975(5, pids, 64, &procCount);
		if (ret < 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1,
				       packet->tag, ret);
			return SDBGP_ERROR_INVALID_ARGUMENT;
		}
		size = SDBGP_SEND_BUF_SIZE;
		reply = sdbgpAllocSendBuf(cpu, SDBGP_SEND_BUF_SIZE);
		if (!reply)
			return SDBGP_ERROR_OUT_OF_MEMORY;
		SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, size);
		size = sizeof(SdbgpModuleReply);
		uint8_t *dst = reply->data;
		SceSize i;

		for (i = 0; (int)i < (int)procCount; i++) {
			SceUID curPid = pids[i];

			oneSize = 0;
			ret = sceKernelGetLostLibraryList(curPid, NULL, NULL);
			if (ret < 0) {
				sdbgpSendError(cpu, packet->group, packet->command + 1,
					       packet->tag, ret);
				return SDBGP_ERROR_INVALID_ARGUMENT;
			}
			num = ret;
			ret = sceKernelGetLostLibraryList(curPid, work, &num);
			if (ret < 0) {
				sdbgpSendError(cpu, packet->group, packet->command + 1,
					       packet->tag, ret);
				return SDBGP_ERROR_INVALID_ARGUMENT;
			}
			ret = _getLostLibraryInfo((SdbgpLostLibraryInfo *)dst, curPid,
						  work, num, &oneSize);
			if (ret < 0) {
				sdbgpSendError(cpu, packet->group, packet->command + 1,
					       packet->tag, ret);
				return SDBGP_ERROR_INVALID_ARGUMENT;
			}
			total += ret;
			dst += oneSize;
			size += oneSize;
		}
		reply->processId = -1;
		reply->count = total;
		sdbgpSetSendSize(cpu, size);
		sdbgpSendErrorHeader(cpu, packet->group, 0x13, 0, 0);
		return 0;
	}
	err = isIllegalProcessId(request->processId);
	if (err != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	ret = sceKernelGetLostLibraryList(pid, NULL, NULL);
	if (ret < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, ret);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	num = ret;
	size = SDBGP_SEND_BUF_SIZE;
	reply = sdbgpAllocSendBuf(cpu, SDBGP_SEND_BUF_SIZE);
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, size);
	work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
	ret = sceKernelGetLostLibraryList(pid, work, &num);
	if (ret < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, ret);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	size = 0;
	reply->count = _getLostLibraryInfo((SdbgpLostLibraryInfo *)reply->data, pid,
					   work, num, &size);
	reply->processId = pid;
	size += sizeof(SdbgpModuleReply);
	sdbgpSetSendSize(cpu, size);
	sdbgpSendErrorHeader(cpu, packet->group, 0x13, 0, 0);
	return 0;
}

int sdbgpCmd40_30(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	const SdbgpModuleRequest *request = (const SdbgpModuleRequest *)packet;
	SdbgpModuleReply *reply;
	SceUID *work;
	SceUID pid;
	SceSize size;
	SceUInt32 num;
	int err, ret;

	if (sdbgpGetRecvSize(cpu) <= 0x37) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	pid = request->processId;
	err = isIllegalProcessId(pid);
	if (pid == -1)
		pid = SDBGP_KERNEL_PROCESS_ID;
	if (err != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	ret = sceKernelGetStubList(pid, NULL, NULL);
	if (ret < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, ret);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	num = ret;
	size = SDBGP_SEND_BUF_SIZE;
	reply = sdbgpAllocSendBuf(cpu, SDBGP_SEND_BUF_SIZE);
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, size);
	work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
	ret = sceKernelGetStubList(pid, work, &num);
	if (ret < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, ret);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	size = 0;
	reply->count = _getStubInfo((SdbgpStubInfo *)reply->data, pid,
				    work, num, &size);
	reply->processId = pid;
	size += sizeof(SdbgpModuleReply);
	sdbgpSetSendSize(cpu, size);
	sdbgpSendErrorHeader(cpu, packet->group, 0x31, 0, 0);
	return 0;
}
