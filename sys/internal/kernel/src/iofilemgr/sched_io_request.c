/*
 * SceIofilemgr: the I/O scheduler's requests (Sony's sched_io_request.c).
 *
 * A request holds one I/O call: who made it, its priority and device, the
 * call's arguments and result. A read or write is split into chunks the
 * scheduler runs one at a time; the request keeps the chunks not yet run.
 */

#include <excpmgr/excpmgr_cpu.h>
#include "iofilemgr_internal.h"

/* the bit TPIDRURO has while the kernel works for a user thread's syscall */
#define IO_REQUEST_FROM_USER(tpidruro)	(((tpidruro) >> 16) & 1)

#if FW_DEBUG
int sceIoRequestIsValid(IoRequest *req)
{
	return req != NULL;
}
#endif

/*
 * Sony's sceIoRequestPrintIoRequest is not in the module (the linker dropped it, nothing
 * called it); the debug record of its line 956 stayed behind.
 */
#if FW_DEBUG
SCE_DBGINFO_DEFINE(static, dbginfoPrintIoRequest, 956, "sceIoRequestPrintIoRequest");
#endif

int sceIoRequestCreateIoRequest(IoRequest *req, int type, int isAsync, SceUID asyncEvent,
				IoAsyncParam *asyncParam, int mntOpt, SceInt32 priority, IoChunk *chunks,
				int chunkNum)
{
	IoAsyncParam param;
	SceUID thid;
	int ret;

	thid = sceKernelGetThreadId();
	memset(req, 0, sizeof(*req));
	req->isCanceled = 0;
	req->isRunning = 0;
	req->priority = priority;
	req->isAsync = isAsync;
	req->type = type;
	req->mntOpt = mntOpt;
	req->pid = sceKernelGetProcessId();
	req->thid = isAsync == 1 ? 0 : thid;
	req->processId = SceThreadmgrForDriver_FA54D49A();
	req->threadAcl = SceThreadmgrForDriver_332E127C();
	req->cpuAffinityMask = sceKernelGetThreadCpuAffinityMask(thid);
	req->threadPriority = sceKernelGetThreadCurrentPriority();
	req->time = sceKernelGetSystemTimeWide();

	ret = sceIoChunkCreateIoChunkPool(chunks, chunkNum, &req->chunkPool);
	if (ret < 0)
		return ret;

	req->asyncEvent = asyncEvent;
	req->fromUser = IO_REQUEST_FROM_USER(readTpidruro());
	req->asyncParam = asyncParam;
	if (asyncParam != NULL) {
		if (req->fromUser) {
			ret = sceKernelCopyFromUser(&param, asyncParam, sizeof(param));
			if (ret >= 0)
				req->asyncPriority = param.priority;
		} else {
			req->asyncPriority = asyncParam->priority;
		}
	}
	return ret;
}

/* removable devices cache what is read and written (name unknown) */
void sceIoRequestSetCacheMode(IoRequest *req, int isWrite, int isDataIo)
{
	if (req->priority >= IO_PRIORITY_NON_GAME) {
		switch ((SceUInt16)req->mntOpt) {
		case IO_SCHED_TYPE_REMOVABLE:
			if (isWrite == 1) {
				req->isWrite = isWrite;
				if (isDataIo == 1)
					req->cacheFlags = 0x8000;
				else
					req->cacheFlags = 0;
			} else {
				req->isWrite = 0;
				if (isDataIo == 1)
					req->cacheFlags = 0x8000;
				else
					req->cacheFlags = 0;
			}
			break;
		case IO_SCHED_TYPE_GAME_CARD:
		case IO_SCHED_TYPE_GAME_CARD2:
		case IO_SCHED_TYPE_HOST:
			req->cacheFlags = 0;
			break;
		}
	}
}

#define IO_REQUEST_CREATE(type, chunks, chunkNum)				\
	sceIoRequestCreateIoRequest(req, (type), isAsync, asyncEvent, asyncParam, \
				    mntOpt, priority, (chunks), (chunkNum))

int sceIoRequestCreateIoRequestForOpen(IO_REQUEST_CREATE_ARGS, const char *path, int flags,
				       SceMode mode)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_OPEN, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 1, 0);
		req->args.open.path = path;
		req->args.open.flags = flags;
		req->args.open.mode = mode;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForClose(IO_REQUEST_CREATE_ARGS, SceUID fd)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_CLOSE, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 1, 0);
		req->args.fd.fd = fd;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForRead(IO_REQUEST_CREATE_ARGS, IoChunk *chunks, int chunkNum,
				       SceUID fd, void *data, SceSize size)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_READ, chunks, chunkNum);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 0, 1);
		req->args.rw.fd = fd;
		req->args.rw.data = data;
		req->args.rw.size = size;
		req->args.rw.offset = -1;
		req->ioData = data;
		req->ioDone = 0;
		req->ioRemain = size;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForWrite(IO_REQUEST_CREATE_ARGS, IoChunk *chunks, int chunkNum,
					SceUID fd, const void *data, SceSize size)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_WRITE, chunks, chunkNum);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 1, 1);
		req->args.write.fd = fd;
		req->args.write.data = data;
		req->args.write.size = size;
		req->ioData = (void *)data;
		req->ioDone = 0;
		req->ioRemain = size;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForLseek(IO_REQUEST_CREATE_ARGS, SceUID fd, SceOff offset,
					int whence)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_LSEEK, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 0, 0);
		req->args.lseek.fd = fd;
		req->args.lseek.whence = whence;
		req->args.lseek.offset = offset;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForLseek32(IO_REQUEST_CREATE_ARGS, SceUID fd, int offset,
					  int whence)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_LSEEK32, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 0, 0);
		req->args.lseek32.fd = fd;
		req->args.lseek32.whence = whence;
		req->args.lseek32.offset = offset;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForRemove(IO_REQUEST_CREATE_ARGS, const char *file)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_REMOVE, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 1, 0);
		req->args.path.path = file;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForRename(IO_REQUEST_CREATE_ARGS, const char *oldname,
					 const char *newname)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_RENAME, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 1, 0);
		req->args.rename.oldname = oldname;
		req->args.rename.newname = newname;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForMkdir(IO_REQUEST_CREATE_ARGS, const char *dir, SceMode mode)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_MKDIR, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 1, 0);
		req->args.mkdir.path = dir;
		req->args.mkdir.mode = mode;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForRmdir(IO_REQUEST_CREATE_ARGS, const char *path)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_RMDIR, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 1, 0);
		req->args.path.path = path;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForDopen(IO_REQUEST_CREATE_ARGS, const char *dirname)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_DOPEN, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 0, 0);
		req->args.path.path = dirname;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForDclose(IO_REQUEST_CREATE_ARGS, SceUID fd)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_DCLOSE, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 0, 0);
		req->args.fd.fd = fd;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForDread(IO_REQUEST_CREATE_ARGS, SceUID fd, SceIoDirent *dir)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_DREAD, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 0, 0);
		req->args.dread.fd = fd;
		req->args.dread.dir = dir;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForDreadForVsh(IO_REQUEST_CREATE_ARGS, SceUID fd,
					      SceIoDirent *dir)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_DREAD_FOR_VSH, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 0, 0);
		req->args.dread.fd = fd;
		req->args.dread.dir = dir;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForGetstat(IO_REQUEST_CREATE_ARGS, const char *file,
					  SceIoStat *stat)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_GETSTAT, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 0, 0);
		req->args.getstat.path = file;
		req->args.getstat.stat = stat;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForGetstatByFd(IO_REQUEST_CREATE_ARGS, SceUID fd,
					      SceIoStat *stat)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_GETSTAT_BY_FD, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 0, 0);
		req->args.getstatByFd.fd = fd;
		req->args.getstatByFd.stat = stat;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForChstat(IO_REQUEST_CREATE_ARGS, const char *file,
					 SceIoStat *stat, int bits)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_CHSTAT, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 1, 0);
		req->args.chstat.path = file;
		req->args.chstat.stat = stat;
		req->args.chstat.bits = bits;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForChstatByFd(IO_REQUEST_CREATE_ARGS, SceUID fd,
					     SceIoStat *stat, int bits)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_CHSTAT_BY_FD, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 1, 0);
		req->args.chstatByFd.fd = fd;
		req->args.chstatByFd.stat = stat;
		req->args.chstatByFd.bits = bits;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForGetstatForVsh(IO_REQUEST_CREATE_ARGS, const char *file,
						SceIoStat *stat)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_GETSTAT_FOR_VSH, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 0, 0);
		req->args.getstat.path = file;
		req->args.getstat.stat = stat;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForChstatForVsh(IO_REQUEST_CREATE_ARGS, const char *file,
					       SceIoStat *stat, int bits)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_CHSTAT_FOR_VSH, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 1, 0);
		req->args.chstat.path = file;
		req->args.chstat.stat = stat;
		req->args.chstat.bits = bits;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForSync(IO_REQUEST_CREATE_ARGS, const char *device,
				       unsigned int flag)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_SYNC, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 1, 0);
		req->args.sync.device = device;
		req->args.sync.flag = flag;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForSyncByFd(IO_REQUEST_CREATE_ARGS, SceUID fd, int flag)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_SYNC_BY_FD, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 1, 0);
		req->args.syncByFd.fd = fd;
		req->args.syncByFd.flag = flag;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForDevctl(IO_REQUEST_CREATE_ARGS, const char *dev,
					 unsigned int cmd, const void *indata, SceSize inlen,
					 void *outdata, SceSize outlen)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_DEVCTL, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 1, 0);
		req->args.devctl.dev = dev;
		req->args.devctl.cmd = cmd;
		req->args.devctl.indata = indata;
		req->args.devctl.inlen = inlen;
		req->args.devctl.outdata = outdata;
		req->args.devctl.outlen = outlen;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForIoctl(IO_REQUEST_CREATE_ARGS, SceUID fd, unsigned int cmd,
					const void *indata, SceSize inlen, void *outdata,
					SceSize outlen)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_IOCTL, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 1, 0);
		req->args.ioctl.fd = fd;
		req->args.ioctl.cmd = cmd;
		req->args.ioctl.indata = indata;
		req->args.ioctl.inlen = inlen;
		req->args.ioctl.outdata = outdata;
		req->args.ioctl.outlen = outlen;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForPread(IO_REQUEST_CREATE_ARGS, IoChunk *chunks, int chunkNum,
					SceUID fd, void *data, SceSize size, SceOff offset)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_PREAD, chunks, chunkNum);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 0, 1);
		req->args.rw.fd = fd;
		req->args.rw.data = data;
		req->args.rw.size = size;
		req->ioData = data;
		req->ioDone = 0;
		req->ioRemain = size;
		req->args.rw.offset = offset;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForPwrite(IO_REQUEST_CREATE_ARGS, IoChunk *chunks,
					 int chunkNum, SceUID fd, const void *data, SceSize size,
					 SceOff offset)
{
	int ret;

	ret = IO_REQUEST_CREATE(IO_REQUEST_PWRITE, chunks, chunkNum);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 1, 1);
		req->args.rw.fd = fd;
		req->args.rw.data = (void *)data;
		req->args.rw.size = size;
		req->ioData = (void *)data;
		req->ioDone = 0;
		req->ioRemain = size;
		req->args.rw.offset = offset;
	}
	return ret;
}

/* the directory calls of an asynchronous user call run without a device (names unknown) */
int sceIoRequestCreateIoRequestForDopenAsync(IoRequest *req, SceInt32 priority,
					     SceUID asyncEvent, IoAsyncParam *asyncParam,
					     const char *dirname)
{
	int ret;

	ret = sceIoRequestCreateIoRequest(req, IO_REQUEST_DOPEN_ASYNC, 1, asyncEvent, asyncParam,
					  0, priority, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 0, 0);
		req->args.path.path = dirname;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForDcloseAsync(IoRequest *req, SceInt32 priority,
					      SceUID asyncEvent, IoAsyncParam *asyncParam, SceUID fd)
{
	int ret;

	ret = sceIoRequestCreateIoRequest(req, IO_REQUEST_DCLOSE_ASYNC, 1, asyncEvent,
					  asyncParam, 0, priority, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 0, 0);
		req->args.fd.fd = fd;
	}
	return ret;
}

int sceIoRequestCreateIoRequestForDreadAsync(IoRequest *req, SceInt32 priority,
					     SceUID asyncEvent, IoAsyncParam *asyncParam, SceUID fd,
					     SceIoDirent *dir)
{
	int ret;

	ret = sceIoRequestCreateIoRequest(req, IO_REQUEST_DREAD_ASYNC, 1, asyncEvent, asyncParam,
					  0, priority, NULL, 0);
	if (ret >= 0) {
		sceIoRequestSetCacheMode(req, 0, 0);
		req->args.dread.fd = fd;
		req->args.dread.dir = dir;
	}
	return ret;
}

/* every chunk has run, or the request is done with its data (name unknown) */
SceBool sceIoRequestIsFinished(IoRequest *req)
{
	if (req->runningChunkNum != 0)
		return 0;
	if (req->doneChunkNum == 0 && req->ioRemain == 0)
		return 1;
	return req->isFinished == 1;
}

IoChunk *sceIoRequestPopIoChunk(IoRequest *req)
{
	IoChunk *chunk;

	chunk = req->chunkHead;
	if (chunk == NULL)
		return NULL;

	SCE_DBG_ASSERT(chunk->request == req, 1075);
	SCE_DBG_ASSERT(sceIoChunkIsInPool(&chunk->request->chunkPool, chunk), 1080);
	req->chunkHead = chunk->next;
	SCE_DBG_ASSERT(req->chunkNum != 0, 1084);
	req->chunkNum--;
	if (chunk->next == NULL) {
		SCE_DBG_ASSERT(req->chunkNum == 0, 1088);
		req->chunkTail = NULL;
	}
	req->runningChunkNum++;
	return chunk;
}

void sceIoRequestPushIoChunk(IoRequest *req, IoChunk *chunk)
{
	SCE_DBG_ASSERT(req != NULL && chunk != NULL, 1098);
	SCE_DBG_ASSERT(chunk->request == req, 1099);
	if (req->chunkHead == NULL) {
		SCE_DBG_ASSERT(req->chunkNum == 0, 1102);
		req->chunkHead = chunk;
		req->chunkTail = chunk;
		chunk->next = NULL;
	} else {
		chunk->next = req->chunkHead;
		req->chunkHead = chunk;
	}
	req->chunkNum++;
	req->runningChunkNum--;
}
