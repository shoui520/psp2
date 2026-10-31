/*
 * SceDeci4pSDbgp (os0:kd/deci4p_sdbgp.skprx): file descriptor, VFS mount, PFS
 * mount and process KPLS queries (group 0x60, group_file.c).
 */

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <iofilemgr/iofilemgr.h>
#include <iofilemgr/vfs.h>
#include <processmgr/processmgr.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include "sdbgp_internal.h"

#define SDBGP_ERROR_OUT_OF_MEMORY	((int)0x8002710b)

typedef struct SdbgpFileInfo {
	SceSize size;				/* 0x000: 0x850 */
	SceUID pid;				/* 0x004 */
	SceUID guidFd;				/* 0x008 */
	SceUID puidFd;				/* 0x00c */
	uint32_t reserved[8];			/* 0x010 */
	int flags;				/* 0x030 */
	SceUID ownerPid;			/* 0x034 */
	int mode;				/* 0x038 */
	SceUInt32 ioSchedData;			/* 0x03c */
	SceOff position;			/* 0x040 */
	SceOff fileSize;			/* 0x048 */
	char resolvedOpenPath[0x400];		/* 0x050 */
	char openPath[0x400];			/* 0x450 */
} SdbgpFileInfo;

_Static_assert(sizeof(SdbgpFileInfo) == 0x850, "SdbgpFileInfo");

typedef struct SdbgpFdEntry {
	SceUID pid;
	SceUID guidFd;
	SceUID puidFd;
} SdbgpFdEntry;

_Static_assert(sizeof(SdbgpFdEntry) == 12, "SdbgpFdEntry");

typedef struct SdbgpFdListReply {
	SdbgpHeader header;
	uint32_t count;				/* 0x1c */
	SdbgpFdEntry entries[];			/* 0x20 */
} SdbgpFdListReply;

typedef struct SdbgpFileInfoRequest {
	SdbgpRequest header;
	SceUID processId;			/* 0x14 */
	SceUID fd;				/* 0x18 */
} SdbgpFileInfoRequest;

typedef struct SdbgpFileInfoReply {
	SdbgpHeader header;
	uint32_t reserved;			/* 0x1c */
	uint32_t count;				/* 0x20 */
	uint8_t data[];				/* 0x24 */
} SdbgpFileInfoReply;

_Static_assert(sizeof(SdbgpFileInfoReply) == 0x24, "SdbgpFileInfoReply");

typedef struct SdbgpMountListEntry {
	SceSize size;				/* 0x38 */
	VfsMountListEntry entry;
} SdbgpMountListEntry;

_Static_assert(sizeof(SdbgpMountListEntry) == 0x38, "SdbgpMountListEntry");

typedef struct SdbgpMountListReply {
	SdbgpHeader header;
	uint32_t count;				/* 0x1c */
	SdbgpMountListEntry entries[];		/* 0x20 */
} SdbgpMountListReply;

typedef struct SdbgpMountInfoRequest {
	SdbgpRequest header;
	int flag;				/* 0x14: -1 all */
	uint32_t count;				/* 0x18 */
	SceVfsMount *mounts[];			/* 0x1c */
} SdbgpMountInfoRequest;

typedef struct SdbgpMountInfoEntry {
	SceSize size;				/* 0x550 */
	VfsMountInfo info;
} SdbgpMountInfoEntry;

_Static_assert(sizeof(SdbgpMountInfoEntry) == 0x550, "SdbgpMountInfoEntry");

typedef struct SdbgpMountInfoReply {
	SdbgpHeader header;
	uint32_t reserved;			/* 0x1c */
	uint32_t count;				/* 0x20 */
	SdbgpMountInfoEntry entries[];		/* 0x24 */
} SdbgpMountInfoReply;

typedef struct SdbgpPfsMountHeader {
	SceSize size;				/* 0x000 */
	SceVfsMount *mount;			/* 0x004 */
	char path[0x420];			/* 0x008 */
	char rootPath[0x24];			/* 0x428 */
	char assignName[0x24];			/* 0x44c */
	SceUInt16 mode;				/* 0x470 */
	SceUInt16 pad;				/* 0x472 */
	SceUInt32 quota;			/* 0x474 */
	SceUInt32 version;			/* 0x478 */
	SceInt32 approvedMax;			/* 0x47c */
	SceUInt64 approved[];			/* 0x480 */
} SdbgpPfsMountHeader;

_Static_assert(sizeof(SdbgpPfsMountHeader) == 0x480, "SdbgpPfsMountHeader");

typedef struct SdbgpPfsMountReply {
	SdbgpHeader header;
	uint32_t reserved;			/* 0x1c */
	uint32_t count;				/* 0x20 */
	uint8_t data[];				/* 0x24 */
} SdbgpPfsMountReply;

typedef struct SdbgpKplsRequest {
	SdbgpRequest header;
	SceUID processId;			/* 0x14 */
} SdbgpKplsRequest;

typedef struct SdbgpKplsEntry {
	SceSize size;				/* 0x00: 0x24 */
	SceUID pid;				/* 0x04 */
	uint8_t data[28];			/* 0x08 */
} SdbgpKplsEntry;

_Static_assert(sizeof(SdbgpKplsEntry) == 0x24, "SdbgpKplsEntry");

typedef struct SdbgpKplsReply {
	SdbgpHeader header;
	uint32_t count;				/* 0x1c */
	SdbgpKplsEntry entries[];		/* 0x20 */
} SdbgpKplsReply;

void _copyFileInfo(SceUID pid, SceUID guidFd, SdbgpFileInfo *dst,
		   const SceIofileInfo *src)
{
	dst->flags = src->flags;
	dst->pid = pid;
	dst->guidFd = guidFd;
	dst->puidFd = -1;
	dst->ioSchedData = src->ioSchedData;
	dst->size = sizeof(*dst);
	dst->ownerPid = src->pid;
	dst->mode = src->mode;
	dst->position = src->position;
	dst->fileSize = src->size;
	memcpy(dst->resolvedOpenPath, src->resolvedOpenPath,
	       sizeof(dst->resolvedOpenPath));
	memcpy(dst->openPath, src->openPath, sizeof(dst->openPath));
}

int sdbgpCmd60_12(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	SceIoFdInfo *guidList, *puidList;
	SdbgpFdListReply *reply;
	SceSize count = 0;
	SceSize guidNum, puidNum;
	SceSize guidBytes, puidBytes, sendSize;
	int ret;
	int i, j;

	if (sdbgpGetRecvSize(cpu) <= 0x23) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	guidList = sdbgpAllocWorkBuf(cpu, SDBGP_PACKET_MAX);
	ret = sceIoGetGUIDFdListForDebugger(5, NULL, 0, NULL);
	if (ret < 0)
		return SDBGP_ERROR_PACKET_SIZE;
	guidNum = ret;
	ret = sceIoGetPUIDFdListForDebugger(5, NULL, 0, NULL);
	if (ret < 0)
		return SDBGP_ERROR_PACKET_SIZE;
	guidBytes = sizeof(SceIoFdInfo) * guidNum;
	puidBytes = sizeof(SceIoFdInfo) * ret;
	SceDeci4pSDfMgrForKernel_8A3B06F2(guidList, 0, guidBytes);
	sendSize = sizeof(SdbgpFdEntry) * guidNum + sizeof(SdbgpFdListReply);
	puidList = (SceIoFdInfo *)((char *)guidList + guidBytes);
	SceDeci4pSDfMgrForKernel_8A3B06F2(puidList, 0, puidBytes);
	if (sendSize > SDBGP_PACKET_MAX) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_LARGE);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	if (sceIoGetGUIDFdListForDebugger(5, guidList, guidBytes, &count) < 0)
		return SDBGP_ERROR_PACKET_SIZE;
	guidNum = count;
	if (sceIoGetPUIDFdListForDebugger(5, puidList, puidBytes, &count) < 0)
		return SDBGP_ERROR_PACKET_SIZE;
	puidNum = count;
	reply = sdbgpAllocSendBuf(cpu, sendSize);
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, sendSize);
	sdbgpSetSendSize(cpu, sendSize);
	reply->count = guidNum;
	for (i = 0; i < (int)guidNum; i++) {
		reply->entries[i].guidFd = guidList[i].fd;
		for (j = 0; j < (int)puidNum; j++) {
			if (scePUIDtoGUID(puidList[j].pid, puidList[j].fd) ==
			    guidList[i].fd) {
				reply->entries[i].puidFd = puidList[j].fd;
				reply->entries[i].pid = puidList[j].pid;
				break;
			}
		}
		if (j == (int)puidNum)
			reply->entries[i].pid = guidList[i].pid;
	}
	sdbgpSendErrorHeader(cpu, packet->group, 0x13, 0, 0);
	return 0;
}

int sdbgpCmd60_00(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	const SdbgpFileInfoRequest *request = (const SdbgpFileInfoRequest *)packet;
	SceIofileInfo *info;
	SceIoFdInfo *guidList, *puidList;
	SdbgpFileInfoReply *reply;
	SdbgpFileInfo *dst;
	SceUID pid, reqFd, puidFd, guidFd;
	SceSize count;
	SceSize guidBytes, puidBytes, sendSize;
	int single;
	int guidNum, puidNum;
	int err, ret;
	int i, j, outCount;

	if (sdbgpGetRecvSize(cpu) <= 0x2b) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	pid = request->processId;
	reqFd = request->fd;
	if (reqFd == -1) {
		puidFd = -1;
		guidFd = -1;
		single = 0;
	} else {
		single = 1;
		if (reqFd & 0x40000000) {
			puidFd = reqFd;
			guidFd = -1;
		} else {
			puidFd = -1;
			guidFd = reqFd;
		}
	}
	if (pid != -1 && isIllegalProcessId(pid) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	info = sdbgpAllocWorkBuf(cpu, SDBGP_PACKET_MAX);
	if (reqFd != -1) {
		if (reqFd & 0x40000000)
			err = isIllegalFd(pid, reqFd, info);
		else
			err = isIllegalSysFd(reqFd, info);
		if (err != 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1,
				       packet->tag, SDBGP_RESULT_ILLEGAL_FD);
			return SDBGP_ERROR_INVALID_ARGUMENT;
		}
		if (puidFd != -1 && pid != -1)
			guidFd = scePUIDtoGUID(pid, puidFd);
	}
	SceDeci4pSDfMgrForKernel_8A3B06F2(info, 0, SDBGP_PACKET_MAX);
	ret = sceIoGetGUIDFdListForDebugger(5, NULL, 0, NULL);
	if (ret < 0)
		return SDBGP_ERROR_PACKET_SIZE;
	guidNum = ret;
	ret = sceIoGetPUIDFdListForDebugger(5, NULL, 0, NULL);
	if (ret < 0)
		return SDBGP_ERROR_PACKET_SIZE;
	puidNum = ret;
	if ((unsigned int)guidNum > 30 && !single) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_LARGE);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	guidBytes = sizeof(SceIoFdInfo) * guidNum;
	puidBytes = sizeof(SceIoFdInfo) * puidNum;
	guidList = (SceIoFdInfo *)((uint32_t *)info + 522);
	puidList = (SceIoFdInfo *)((char *)guidList + guidBytes);
	SceDeci4pSDfMgrForKernel_8A3B06F2(guidList, 0, guidBytes);
	SceDeci4pSDfMgrForKernel_8A3B06F2(puidList, 0, puidBytes);
	if (sceIoGetGUIDFdListForDebugger(5, guidList, guidBytes, &count) < 0)
		return SDBGP_ERROR_PACKET_SIZE;
	guidNum = count;
	if (sceIoGetPUIDFdListForDebugger(5, puidList, puidBytes, &count) < 0)
		return SDBGP_ERROR_PACKET_SIZE;
	puidNum = count;
	if (single) {
		sendSize = sizeof(SdbgpFileInfoReply) + sizeof(SdbgpFileInfo);
		guidNum = 1;
	} else {
		sendSize = sizeof(SdbgpFileInfoReply) + sizeof(SdbgpFileInfo) * guidNum;
		if (sendSize > SDBGP_PACKET_MAX) {
			sdbgpSendError(cpu, packet->group, packet->command + 1,
				       packet->tag, SDBGP_RESULT_PACKET_TOO_LARGE);
			return SDBGP_ERROR_PACKET_SIZE;
		}
	}
	reply = sdbgpAllocSendBuf(cpu, sendSize);
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, sendSize);
	dst = (SdbgpFileInfo *)reply->data;
	if (single) {
		SceDeci4pSDfMgrForKernel_8A3B06F2(info, 0, 8);
		if (pid == -1) {
			for (j = 0; j < puidNum; j++) {
				if (guidFd == scePUIDtoGUID(puidList[j].pid,
							    puidList[j].fd)) {
					pid = puidList[j].pid;
					break;
				}
			}
			if (j == puidNum)
				pid = SDBGP_KERNEL_PROCESS_ID;
		}
		if (SceIofilemgrForDriver_A1EF2648(guidFd, SDBGP_KERNEL_PROCESS_ID,
						   info) < 0)
			return SDBGP_ERROR_PACKET_SIZE;
		_copyFileInfo(pid, guidFd, dst, info);
		if (puidFd == -1) {
			for (j = 0; j < puidNum; j++) {
				if (guidFd == scePUIDtoGUID(puidList[j].pid,
							    puidList[j].fd)) {
					dst->puidFd = puidList[j].fd;
					dst->pid = puidList[j].pid;
					break;
				}
			}
		} else {
			dst->puidFd = puidFd;
		}
		sendSize = sizeof(SdbgpFileInfoReply) + sizeof(SdbgpFileInfo);
		outCount = 1;
	} else {
		outCount = 0;
		for (i = 0; i < guidNum; i++) {
			SceDeci4pSDfMgrForKernel_8A3B06F2(info, 0, 8);
			if (SceIofilemgrForDriver_A1EF2648(guidList[i].fd,
							   guidList[i].pid,
							   info) < 0)
				return SDBGP_ERROR_PACKET_SIZE;
			_copyFileInfo(guidList[i].pid, guidList[i].fd, dst, info);
			for (j = 0; j < puidNum; j++) {
				if (guidList[i].fd == scePUIDtoGUID(puidList[j].pid,
								    puidList[j].fd)) {
					dst->puidFd = puidList[j].fd;
					dst->pid = puidList[j].pid;
					break;
				}
			}
			if (pid == -1 || dst->pid == pid) {
				dst++;
				outCount++;
			}
		}
		sendSize = sizeof(SdbgpFileInfoReply) + sizeof(SdbgpFileInfo) * outCount;
	}
	sdbgpSetSendSize(cpu, sendSize);
	reply->count = outCount;
	sdbgpSendErrorHeader(cpu, packet->group, 0x01, 0, 0);
	return 0;
}

int sdbgpCmd60_14(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	VfsMountListEntry *work;
	SdbgpMountListReply *reply;
	SceSize sendSize;
	int num;
	int ret;
	int i;

	if (!cpu || !packet)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	if (sdbgpGetRecvSize(cpu) <= 0x23) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	work = sdbgpAllocWorkBuf(cpu, SDBGP_PACKET_MAX);
	ret = vfsGetMntList(NULL, 0, NULL);
	if (ret < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, ret);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	num = ret;
	SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, sizeof(VfsMountListEntry) * num);
	sendSize = sizeof(SdbgpMountListEntry) * num + sizeof(SdbgpMountListReply);
	if (sendSize > SDBGP_PACKET_MAX) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_LARGE);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	ret = vfsGetMntList(work, SDBGP_PACKET_MAX, &num);
	if (ret < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, ret);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	reply = sdbgpAllocSendBuf(cpu, sendSize);
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, sendSize);
	sdbgpSetSendSize(cpu, sendSize);
	reply->count = num;
	for (i = 0; i < num; i++) {
		reply->entries[i].size = sizeof(SdbgpMountListEntry);
		SceDeci4pSDfMgrForKernel_5996E2DB(&reply->entries[i].entry, &work[i],
						  sizeof(VfsMountListEntry));
	}
	sdbgpSendErrorHeader(cpu, packet->group, 0x15, 0, 0);
	return 0;
}

int sdbgpCmd60_16(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	const SdbgpMountInfoRequest *request = (const SdbgpMountInfoRequest *)packet;
	VfsMountInfo *work;
	VfsMountListEntry *list;
	SdbgpMountInfoReply *reply;
	SceSize sendSize;
	int num, dummy;
	int ret;
	int i;

	if (!cpu || !packet)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	if (sdbgpGetRecvSize(cpu) <= 0x1b) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	if (request->flag == -1) {
		work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_WORK_BUF_SIZE);
		ret = vfsGetMntList(NULL, 0, NULL);
		if (ret < 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1,
				       packet->tag, ret);
			return SDBGP_ERROR_INVALID_ARGUMENT;
		}
		num = ret;
		if ((unsigned int)num > 48) {
			sdbgpSendError(cpu, packet->group, packet->command + 1,
				       packet->tag, SDBGP_RESULT_ILLEGAL_VALUE);
			return SDBGP_ERROR_INVALID_ARGUMENT;
		}
		list = (VfsMountListEntry *)(work + num);
		sendSize = sizeof(SdbgpMountInfoEntry) * num + sizeof(SdbgpMountInfoReply);
		ret = vfsGetMntList(list, sizeof(VfsMountListEntry) * num, &dummy);
		if (ret < 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1,
				       packet->tag, ret);
			return SDBGP_ERROR_INVALID_ARGUMENT;
		}
		for (i = 0; i < num; i++) {
			ret = vfsGetMntInfo(list[i].mnt, &work[i]);
			if (ret < 0) {
				sdbgpSendError(cpu, packet->group, packet->command + 1,
					       packet->tag, ret);
				return SDBGP_ERROR_INVALID_ARGUMENT;
			}
		}
	} else {
		work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_WORK_BUF_SIZE);
		num = request->count;
		if (isIllegalMountNum(num) != 0 ||
		    isIllegalMountList(request->mounts, num,
				       (VfsMountListEntry *)work) != 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1,
				       packet->tag, SDBGP_RESULT_ILLEGAL_VALUE);
			return SDBGP_ERROR_INVALID_ARGUMENT;
		}
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_WORK_BUF_SIZE);
		sendSize = sizeof(SdbgpMountInfoEntry) * num + sizeof(SdbgpMountInfoReply);
		if (sendSize > SDBGP_PACKET_MAX) {
			sdbgpSendError(cpu, packet->group, packet->command + 1,
				       packet->tag, SDBGP_RESULT_PACKET_TOO_LARGE);
			return SDBGP_ERROR_PACKET_SIZE;
		}
		for (i = 0; i < num; i++) {
			ret = vfsGetMntInfo(request->mounts[i], &work[i]);
			if (ret < 0) {
				sdbgpSendError(cpu, packet->group, packet->command + 1,
					       packet->tag, ret);
				return SDBGP_ERROR_INVALID_ARGUMENT;
			}
		}
	}
	reply = sdbgpAllocSendBuf(cpu, sendSize);
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, sendSize);
	sdbgpSetSendSize(cpu, sendSize);
	reply->count = num;
	for (i = 0; i < num; i++) {
		reply->entries[i].size = sizeof(SdbgpMountInfoEntry);
		SceDeci4pSDfMgrForKernel_5996E2DB(&reply->entries[i].info, &work[i],
						  sizeof(VfsMountInfo));
	}
	sdbgpSendErrorHeader(cpu, packet->group, 0x17, 0, 0);
	return 0;
}

int sdbgpCmd60_18(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	const SdbgpMountInfoRequest *request = (const SdbgpMountInfoRequest *)packet;
	ScePfsMountInfo *work;
	VfsMountListEntry *list;
	SdbgpPfsMountReply *reply;
	SdbgpPfsMountHeader *dst;
	SceSize payloadSize;
	int num, dummy, pfsNum;
	int ret;
	int i, j;

	if (!cpu || !packet)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	if (sdbgpGetRecvSize(cpu) <= 0x1b) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	if (request->flag == -1) {
		work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_WORK_BUF_SIZE);
		ret = vfsGetMntList(NULL, 0, NULL);
		if (ret < 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1,
				       packet->tag, ret);
			return SDBGP_ERROR_INVALID_ARGUMENT;
		}
		if ((unsigned int)ret > 48) {
			sdbgpSendError(cpu, packet->group, packet->command + 1,
				       packet->tag, SDBGP_RESULT_ILLEGAL_VALUE);
			return SDBGP_ERROR_INVALID_ARGUMENT;
		}
		list = (VfsMountListEntry *)(work + ret);
		if (vfsGetMntList(list, sizeof(VfsMountListEntry) * ret, &dummy) < 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1,
				       packet->tag, ret);
			return SDBGP_ERROR_INVALID_ARGUMENT;
		}
		pfsNum = 0;
		for (i = 0; i < ret; i++) {
			if (SceSysrootForKernel_4F57A13F(list[i].mnt,
							 &work[pfsNum]) >= 0)
				pfsNum++;
		}
		num = pfsNum;
	} else {
		work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_WORK_BUF_SIZE);
		num = request->count;
		if (isIllegalMountNum(num) != 0 ||
		    isIllegalMountList(request->mounts, num,
				       (VfsMountListEntry *)work) != 0 ||
		    isIllegalMountAccess(request->mounts, num, work) != 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1,
				       packet->tag, SDBGP_RESULT_ILLEGAL_VALUE);
			return SDBGP_ERROR_INVALID_ARGUMENT;
		}
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_WORK_BUF_SIZE);
		for (i = 0; i < num; i++) {
			ret = SceSysrootForKernel_4F57A13F(request->mounts[i],
							   &work[i]);
			if (ret < 0) {
				sdbgpSendError(cpu, packet->group, packet->command + 1,
					       packet->tag, ret);
				return SDBGP_ERROR_INVALID_ARGUMENT;
			}
		}
	}
	reply = sdbgpAllocSendBuf(cpu, SDBGP_SEND_BUF_SIZE);
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, SDBGP_SEND_BUF_SIZE);
	reply->count = num;
	if (num <= 0) {
		sdbgpSetSendSize(cpu, sizeof(SdbgpPfsMountReply));
	} else {
		dst = (SdbgpPfsMountHeader *)reply->data;
		payloadSize = 0;
		for (i = 0; i < num; i++) {
			dst->mount = work[i].mount;
			dst->mode = work[i].mode;
			dst->quota = work[i].quota;
			dst->version = work[i].version;
			SceDeci4pSDfMgrForKernel_5996E2DB(dst->path, work[i].path,
							  sizeof(dst->path));
			SceDeci4pSDfMgrForKernel_5996E2DB(dst->rootPath,
							  work[i].rootPath, 0x21);
			SceDeci4pSDfMgrForKernel_5996E2DB(dst->assignName,
							  work[i].assignName, 0x22);
			dst->approvedMax = work[i].approvedMax;
			SceDeci4pSDfMgrForKernel_5996E2DB(dst->approved,
				work[i].approved,
				sizeof(SceUInt64) * work[i].approvedMax);
			uint32_t *acidCount =
				(uint32_t *)&dst->approved[dst->approvedMax];
			uint8_t *acidDirs = (uint8_t *)(acidCount + 1);

			*acidCount = work[i].acidDirCount;
			for (j = 0; j < work[i].acidDirCount; j++) {
				SceDeci4pSDfMgrForKernel_5996E2DB(
					acidDirs + 0x40 * j,
					work[i].acidDirs[j], 0x40);
			}
			dst->size = sizeof(SdbgpPfsMountHeader) +
				    sizeof(SceUInt64) * dst->approvedMax +
				    sizeof(uint32_t) + 0x40 * (*acidCount);
			payloadSize += dst->size;
			dst = (SdbgpPfsMountHeader *)(acidDirs + 0x40 * (*acidCount));
		}
		sdbgpSetSendSize(cpu, payloadSize + sizeof(SdbgpPfsMountReply));
	}
	sdbgpSendErrorHeader(cpu, packet->group, 0x19, 0, 0);
	return 0;
}

int sdbgpCmd60_1a(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	const SdbgpKplsRequest *request = (const SdbgpKplsRequest *)packet;
	SceUID *pids;
	uint8_t *kplsBuf;
	SceKernelProcessInfo *procInfo;
	SdbgpKplsReply *reply;
	SceSize procCount = 0;
	SceSize sendSize;
	SceUID pid;
	int count;
	int ret;
	int i;

	if (!cpu || !packet)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	if (sdbgpGetRecvSize(cpu) <= 0x17) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	pid = request->processId;
	if (pid == -1) {
		pids = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
		kplsBuf = (uint8_t *)pids + 0x40;
		SceDeci4pSDfMgrForKernel_8A3B06F2(pids, 0, SDBGP_WORK_BUF_SIZE);
		procInfo = (SceKernelProcessInfo *)((uint8_t *)pids + 0x240);
		ret = SceProcessmgrForDriver_BA0A06A4(5, procInfo, 0xe80,
						      &procCount);
		if (ret < 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1,
				       packet->tag, SDBGP_RESULT_ILLEGAL_PROCESS_ID);
			return ret;
		}
		if ((int)procCount <= 0) {
			count = 0;
			sendSize = sizeof(SdbgpKplsReply);
		} else {
			count = 0;
			for (i = 0; i < (int)procCount; i++) {
				if (procInfo[i].pid != SDBGP_KERNEL_PROCESS_ID)
					pids[count++] = procInfo[i].pid;
			}
			count = 0;
			for (i = 0; i < (int)procCount; i++) {
				if (pids[i] != SDBGP_KERNEL_PROCESS_ID &&
				    SceIofilemgrForDriver_44EDCE57(
					    pids[i], kplsBuf + 0x20 * i) >= 0)
					count++;
			}
			sendSize = sizeof(SdbgpKplsEntry) * count +
				   sizeof(SdbgpKplsReply);
			if (sendSize > SDBGP_PACKET_MAX) {
				sdbgpSendError(cpu, packet->group, packet->command + 1,
					       packet->tag,
					       SDBGP_RESULT_PACKET_TOO_LARGE);
				return SDBGP_ERROR_PACKET_SIZE;
			}
		}
	} else {
		if (pid == SDBGP_KERNEL_PROCESS_ID) {
			sdbgpSendError(cpu, packet->group, packet->command + 1,
				       packet->tag, SDBGP_RESULT_ILLEGAL_VALUE);
			return SDBGP_ERROR_INVALID_ARGUMENT;
		}
		if (isIllegalProcessId(pid) != 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1,
				       packet->tag, SDBGP_RESULT_ILLEGAL_PROCESS_ID);
			return SDBGP_ERROR_INVALID_ARGUMENT;
		}
		pids = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
		kplsBuf = (uint8_t *)pids + 0x40;
		SceDeci4pSDfMgrForKernel_8A3B06F2(pids, 0, SDBGP_WORK_BUF_SIZE);
		if (SceIofilemgrForDriver_44EDCE57(pid, kplsBuf) < 0)
			return SDBGP_ERROR_INVALID_ARGUMENT;
		pids[0] = pid;
		sendSize = sizeof(SdbgpKplsReply) + sizeof(SdbgpKplsEntry);
		count = 1;
	}
	reply = sdbgpAllocSendBuf(cpu, sendSize);
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, sendSize);
	reply->count = count;
	for (i = 0; i < count; i++) {
		reply->entries[i].size = sizeof(SdbgpKplsEntry);
		reply->entries[i].pid = pids[i];
		SceDeci4pSDfMgrForKernel_5996E2DB(reply->entries[i].data,
						  kplsBuf + 0x20 * i,
						  sizeof(reply->entries[i].data));
	}
	sdbgpSetSendSize(cpu, sendSize);
	sdbgpSendErrorHeader(cpu, packet->group, 0x1b, 0, 0);
	return 0;
}
