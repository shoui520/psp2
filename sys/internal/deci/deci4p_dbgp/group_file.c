/*
 * The DBGP file group: the files a debugged process has open
 * (Sony's group_file.c, as in deci4p_sdbgp).
 */

#include <kernel/error.h>
#include <iofilemgr/iofilemgr.h>
#include <sysmem/sysclib.h>
#include "dbgp_internal.h"

typedef struct DbgpFileRequest {
	DbgpPrefix prefix;
	SceUID pid;
} DbgpFileRequest;

/* one open file; its two paths follow, each after its length */
typedef struct DbgpFdInfo {
	SceSize size;
	SceUID fd;
	int isDir;
	int flags;
	SceUID pid;			/* 0x10 */
	int mode;
	SceOff position __attribute__((packed, aligned(4)));
	SceOff fileSize __attribute__((packed, aligned(4)));	/* 0x20 */
	SceUInt32 ioSchedData;		/* 0x28 */
	SceUInt32 reserved[7];
	SceSize pathLength;		/* 0x48: the resolved path, then the open path */
	char path[];
} DbgpFdInfo;

typedef struct DbgpFdInfoResponse {
	DbgpResponseHeader header;
	SceUID pid;
	int count;			/* 0x20 */
	DbgpFdInfo info[];
} DbgpFdInfoResponse;

/* the work buffer: the file information, then the descriptor list */
typedef struct DbgpFdWork {
	SceIofileInfo info;
	SceIoFdInfo fds[];		/* 0x828 */
} DbgpFdWork;

_Static_assert(sizeof(DbgpFdInfo) == 76, "DBGP file record");

int fileGetFdInfoCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpFileRequest *request = (const DbgpFileRequest *)packet;
	DbgpFdInfoResponse *response;
	DbgpFdInfo *out;
	DbgpFdWork *work;
	SceSize num, size, length;
	SceUInt32 *next;
	SceUID pid;
	char *clear;
	int count, result, n, i, j;

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
	work = _dbgpAllocWorkBuf(proc, DBGP_WORK_BUF_SIZE);
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(work, 0, DBGP_WORK_BUF_SIZE);
	n = sceIoGetPUIDFdListForDebugger(3, NULL, 0, NULL);
	if (n < 0)
		return n;
	memset(work->fds, 0, n * sizeof(SceIoFdInfo));
	result = sceIoGetPUIDFdListForDebugger(3, work->fds, n * sizeof(SceIoFdInfo),
					       &num);
	if (result < 0)
		return result;
	n = num;
	size = sizeof(*response) + n * (sizeof(DbgpFdInfo) + 2 * 0x400 + 4);
	response = _dbgpAllocSendBuf(proc, size);
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, size);
	_dbgpSetSendSize(proc, size);
	count = 0;
	out = response->info;
	for (i = 0; i < n; i++) {
		if (work->fds[i].pid != pid)
			continue;
		/* Sony clears only the first 8 bytes of the information */
		clear = (char *)&work->info;
		for (j = 0; j < 8; j++)
			clear[j] = 0;
		if (SceIofilemgrForDriver_A1EF2648(work->fds[i].fd,
						   work->fds[i].pid, &work->info) < 0)
			return (int)0x80080006;
		out->fd = work->fds[i].fd;
		out->isDir = work->info.isDir;
		out->flags = work->info.flags;
		out->pid = work->info.pid;
		out->mode = work->info.mode;
		out->position = work->info.position;
		out->fileSize = work->info.size;
		out->ioSchedData = work->info.ioSchedData;
		count++;
		out->pathLength = strnlen(work->info.resolvedOpenPath, 0x400) + 1;
		memset(out->path, 0,
		       (strnlen(work->info.resolvedOpenPath, 0x400) + 4) & ~3);
		memcpy(out->path, work->info.resolvedOpenPath,
		       strnlen(work->info.resolvedOpenPath, 0x400));
		length = (strnlen(work->info.resolvedOpenPath, 0x400) + 4) & ~3;
		next = (SceUInt32 *)(out->path + length);
		*next = strnlen(work->info.openPath, 0x400) + 1;
		memset(next + 1, 0, (strnlen(work->info.openPath, 0x400) + 4) & ~3);
		memcpy(next + 1, work->info.openPath,
		       strnlen(work->info.openPath, 0x400));
		out->size = sizeof(*out) + 4 +
			    ((strnlen(work->info.resolvedOpenPath, 0x400) + 4) & ~3) +
			    ((strnlen(work->info.openPath, 0x400) + 4) & ~3);
		out = (DbgpFdInfo *)((char *)out + out->size);
	}
	_dbgpSetSendSize(proc, (char *)out - (char *)response);
	response->pid = pid;
	response->count = count;
	dbgpFillResponseHeader(proc, packet->group, 3, 0, 0);
	return 0;
}
