/* SceIofilemgr's scheduler worker (Sony's sched_scheduler.c). */
#include "iofilemgr_internal.h"

static inline __attribute__((always_inline)) SceUID getDataIoFd(IoRequest *req)
{
	switch (req->type) {
	case IO_REQUEST_READ:
	case IO_REQUEST_WRITE:
	case IO_REQUEST_PREAD:
	case IO_REQUEST_PWRITE:
		return req->args.rw.fd;
	default:
		return SCE_ERROR_ERRNO_EINVAL;
	}
}

SceOff getFilePosition(IoRequest *req)
{
	IofilemgrLseekParam param;

	param.fd = getDataIoFd(req);
	SCE_DBG_ASSERT(param.fd >= 0, 497);
	param.whence = SCE_SEEK_CUR;
	param.offset = 0;
	return sceKernelExtendKernelStackWide(0x2000, (SceInt64 (*)(void *))vfsLseek, &param);
}

/* Names unknown: the optional PFS stat hooks; initialized to zero. */
int g_ioPfsChstatEnabled __attribute__((section(".data"))) = 0;
int g_ioPfsChstatByFdEnabled __attribute__((section(".data"))) = 0;
int ScePfsFacadeForKernel_7F9F4E49(IofilemgrChstatParam *param);
int ScePfsFacadeForKernel_04352D52(IofilemgrChstatByFdParam *param);

void sceIoSchedulerExecuteIoCommand(IoRequest *req)
{
	int ret, closed, priority;
	SceOff result;

	SCE_DBG_ASSERT(req != NULL, 1369);
	switch (req->type) {
	case IO_REQUEST_OPEN: {
		IofilemgrOpenParam param = {
			req->args.open.path, req->args.open.flags, req->args.open.mode
		};
		IofilemgrSchedInfo schedInfo;

		ret = check_limit_and_increment_user_fd_count(req->processId, req->thid);
		if (ret >= 0) {
			req->args.open.result = vfsOpen(&param);
			if (req->args.open.result >= 0) {
				req->mntOpt = getPathMntOpt(req->args.open.path, 0);
				schedInfo.priority = getThreadIoPriority();
				schedInfo.mntOpt = req->mntOpt;
				schedInfo.pid = req->processId;
				setFileSchedParam(req->args.open.result, &schedInfo);
				goto openComplete;
			}
			decrement_user_fd_count(req->processId, req->thid, 0);
			ret = req->args.open.result;
		} else {
			req->args.open.result = ret;
		}
		if (ret == (int)0x80142318)
			break;
openComplete:
		if (req->isAsync == 1) {
			ret = sceIoAsyncFreePath(req->args.open.path);
			SCE_DBG_ASSERT(ret == 0, 1427);
			ret = req->args.open.result;
			if (ret >= 0) {
				SCE_DBG_ASSERT(ret > 256, 1430);
				if (req->processId != 0x10005)
					req->args.open.result = kscePUIDOpenByGUID(req->processId,
									 req->args.open.result);
				ret = req->args.open.result;
			}
			req->asyncResult = ret;
		}
		break;
	}
	case IO_REQUEST_CLOSE: {
		IofilemgrCloseParam param;

		if (req->isAsync == 1 && req->fromUser == 1) {
			ret = kscePUIDtoGUID(req->processId, req->args.fd.fd);
			param.fd = ret;
			if (ret < 0)
				goto closeComplete;
		} else {
			param.fd = req->args.fd.fd;
		}
		ret = vfsClose(&param);
		if (ret == (int)0x80024501) {
			req->args.fd.result = SCE_ERROR_ERRNO_EBADF;
			if (req->isAsync != 1)
				break;
			goto closeBadFd;
		}
		req->args.fd.result = ret;
		if (ret == 0) {
			decrement_user_fd_count(req->processId, req->thid, 1);
			if (req->isAsync != 1)
				break;
			result = 0;
			goto closePuid;
		}
closeComplete:
		if (req->isAsync != 1)
			break;
		if (ret == SCE_ERROR_ERRNO_EBADF) {
			goto closeBadFd;
		}
		result = ret;
closePuid:
		if (req->fromUser == 1) {
			closed = kscePUIDClose(req->processId, req->args.fd.fd);
			SCE_DBG_ASSERT(closed == 0, 1479);
		}
closeResult:
		req->asyncResult = result;
		break;
closeBadFd:
		result = SCE_ERROR_ERRNO_EBADF;
		goto closeResult;
	}
	case IO_REQUEST_LSEEK: {
		IofilemgrLseekParam param = {
			req->args.lseek.fd, req->args.lseek.whence, req->args.lseek.offset
		};

		req->args.lseek.result = vfsLseek(&param);
		if (req->isAsync == 1)
			req->asyncResult = req->args.lseek.result;
		break;
	}
	case IO_REQUEST_LSEEK32: {
		IofilemgrLseekParam param;

		SCE_DBG_ASSERT(req->isAsync == 0, 1509);
		param.fd = req->args.lseek32.fd;
		param.whence = req->args.lseek32.whence;
		param.offset = req->args.lseek32.offset;
		/* The original stores a wide result in the shared lseek result slot. */
		req->args.lseek.offset = vfsLseek(&param);
		break;
	}
	case IO_REQUEST_IOCTL: {
		IofilemgrIoctlParam param;

		SCE_DBG_ASSERT(req->isAsync == 0, 1527);
		param.fd = req->args.ioctl.fd;
		param.cmd = req->args.ioctl.cmd;
		param.indata = req->args.ioctl.indata;
		param.inlen = req->args.ioctl.inlen;
		param.outdata = req->args.ioctl.outdata;
		param.outlen = req->args.ioctl.outlen;
		req->args.ioctl.result = vfsIoctl(&param);
		break;
	}
	case IO_REQUEST_REMOVE: {
		IofilemgrRemoveParam param = { req->args.path.path, 0 };

		req->args.path.result = vfsRemove(&param);
		if (req->isAsync == 1) {
			ret = sceIoAsyncFreePath(req->args.path.path);
			SCE_DBG_ASSERT(ret == 0, 1552);
		}
		break;
	}
	case IO_REQUEST_DOPEN: {
		IofilemgrDopenParam param = { req->args.path.path };
		IofilemgrSchedInfo schedInfo;

		req->args.path.result = vfsDopen(&param);
		if (req->args.path.result >= 0) {
			schedInfo.priority = getThreadIoPriority();
			schedInfo.mntOpt = req->mntOpt;
			schedInfo.pid = req->processId;
			setFileSchedParam(req->args.path.result, &schedInfo);
		}
		if (req->isAsync == 1) {
			ret = sceIoAsyncFreePath(req->args.path.path);
			SCE_DBG_ASSERT(ret == 0, 1589);
			ret = req->args.path.result;
			if (ret >= 0) {
				SCE_DBG_ASSERT(ret > 256, 1592);
				if (req->processId != 0x10005)
					req->args.path.result = kscePUIDOpenByGUID(req->processId,
									 req->args.path.result);
				ret = req->args.path.result;
			}
			req->asyncResult = ret;
		}
		break;
	}
	case IO_REQUEST_DCLOSE: {
		IofilemgrDcloseParam param;

		if (req->isAsync == 1 && req->fromUser == 1) {
			ret = kscePUIDtoGUID(req->processId, req->args.fd.fd);
			param.fd = ret;
			if (ret < 0)
				goto dcloseComplete;
		} else {
			param.fd = req->args.fd.fd;
		}
		ret = vfsDclose(&param);
		if (ret == (int)0x80024501)
			ret = SCE_ERROR_ERRNO_EBADF;
		req->args.fd.result = ret;
dcloseComplete:
		if (req->isAsync == 1) {
			if (ret != SCE_ERROR_ERRNO_EBADF && req->fromUser == 1) {
				closed = kscePUIDClose(req->processId, req->args.fd.fd);
				SCE_DBG_ASSERT(closed == 0, 1635);
			}
			req->asyncResult = ret;
		}
		break;
	}
	case IO_REQUEST_DREAD: {
		IofilemgrDreadParam param;

		param.fd = req->args.dread.fd;
		if (req->isAsync == 1 && req->fromUser == 1) {
			param.dir = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(SceIoDirent));
			if (param.dir == NULL) {
				req->args.dread.result = SCE_ERROR_ERRNO_ENOMEM;
				break;
			}
		} else {
			param.dir = req->args.dread.dir;
		}
		param.preserveAttributes = 0;
		req->args.dread.result = vfsDread(&param);
		if (req->isAsync == 1) {
			req->asyncResult = req->args.dread.result;
			if (req->fromUser == 1) {
				if (req->args.dread.result > 0)
					ksceKernelCopyToUserProc(req->pid, req->args.dread.dir,
							       param.dir, sizeof(SceIoDirent));
				ret = ksceKernelFreeHeapMemory(g_ioSchedHeap, param.dir);
				SCE_DBG_ASSERT(ret == 0, 1683);
			}
		}
		break;
	}
	case IO_REQUEST_MKDIR: {
		IofilemgrMkdirParam param = { req->args.mkdir.path, req->args.mkdir.mode };

		ret = vfsMkdir(&param);
		req->args.mkdir.result = ret;
		if (ret == 0)
			req->mntOpt = getPathMntOpt(req->args.mkdir.path, 0);
		if (req->isAsync == 1) {
			req->asyncResult = ret;
			ret = sceIoAsyncFreePath(req->args.mkdir.path);
			SCE_DBG_ASSERT(ret == 0, 1705);
		}
		break;
	}
	case IO_REQUEST_RMDIR: {
		IofilemgrRmdirParam param = { req->args.path.path };

		ret = vfsRmdir(&param);
		req->args.path.result = ret;
		if (req->isAsync == 1) {
			req->asyncResult = ret;
			ret = sceIoAsyncFreePath(req->args.path.path);
			SCE_DBG_ASSERT(ret == 0, 1719);
		}
		break;
	}
	case IO_REQUEST_RENAME: {
		IofilemgrRenameParam param = { req->args.rename.oldname, req->args.rename.newname };

		ret = vfsRename(&param);
		req->args.rename.result = ret;
		if (req->isAsync == 1) {
			req->asyncResult = ret;
			ret = sceIoAsyncFreePath(req->args.rename.oldname);
			SCE_DBG_ASSERT(ret == 0, 1735);
			ret = sceIoAsyncFreePath(req->args.rename.newname);
			SCE_DBG_ASSERT(ret == 0, 1737);
		}
		break;
	}
	case IO_REQUEST_CHSTAT: {
		IofilemgrChstatParam param = {
			req->args.chstat.path, req->args.chstat.stat, req->args.chstat.bits, 0
		};

		getIofilemgrTls()->request = req;
		if (req->mntOpt & 0x10000) {
			SCE_DBG_ASSERT(g_ioPfsChstatEnabled != 0, 1754);
			ret = ScePfsFacadeForKernel_7F9F4E49(&param);
		} else {
			ret = vfsChstat(&param);
		}
		req->args.chstat.result = ret;
		if (ret == (int)0x80142318)
			break;
		if (req->isAsync == 1) {
			req->asyncResult = ret;
			if (req->fromUser == 1) {
				ret = ksceKernelFreeHeapMemory(g_ioSchedHeap, req->args.chstat.stat);
				SCE_DBG_ASSERT(ret == 0, 1783);
			}
			ret = sceIoAsyncFreePath(req->args.chstat.path);
			SCE_DBG_ASSERT(ret == 0, 1786);
		}
		break;
	}
	case IO_REQUEST_CHSTAT_BY_FD: {
		IofilemgrChstatByFdParam param = {
			req->args.chstatByFd.fd, req->args.chstatByFd.stat, req->args.chstatByFd.bits
		};

		getIofilemgrTls()->request = req;
		if (req->mntOpt & 0x10000) {
			SCE_DBG_ASSERT(g_ioPfsChstatByFdEnabled != 0, 1802);
			ret = ScePfsFacadeForKernel_04352D52(&param);
		} else {
			ret = vfsChstatByFd(&param);
		}
		req->args.chstatByFd.result = ret;
		if (ret == (int)0x80142318)
			break;
		if (req->isAsync == 1) {
			req->asyncResult = ret;
			if (req->fromUser == 1) {
				ret = ksceKernelFreeHeapMemory(g_ioSchedHeap, req->args.chstatByFd.stat);
				SCE_DBG_ASSERT(ret == 0, 1829);
			}
		}
		break;
	}
	case IO_REQUEST_GETSTAT: {
		IofilemgrGetstatParam param;

		param.file = req->args.getstat.path;
		if (req->isAsync == 1 && req->fromUser == 1) {
			param.stat = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(SceIoStat));
			if (param.stat == NULL) {
				req->args.getstat.result = SCE_ERROR_ERRNO_ENOMEM;
				break;
			}
		} else {
			param.stat = req->args.getstat.stat;
		}
		param.preserveAttributes = 0;
		ret = vfsGetstat(&param);
		req->args.getstat.result = ret;
		if (req->isAsync == 1) {
			req->asyncResult = ret;
			closed = sceIoAsyncFreePath(req->args.getstat.path);
			SCE_DBG_ASSERT(closed == 0, 1859);
			if (req->fromUser == 1) {
				if (ret >= 0)
					ksceKernelCopyToUserProc(req->pid, req->args.getstat.stat,
							       param.stat, sizeof(SceIoStat));
				ret = ksceKernelFreeHeapMemory(g_ioSchedHeap, param.stat);
				SCE_DBG_ASSERT(ret == 0, 1874);
			}
		}
		break;
	}
	case IO_REQUEST_GETSTAT_BY_FD: {
		IofilemgrGetstatByFdParam param;

		param.fd = req->args.getstatByFd.fd;
		if (req->isAsync == 1 && req->fromUser == 1) {
			param.stat = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(SceIoStat));
			if (param.stat == NULL) {
				req->args.getstatByFd.result = SCE_ERROR_ERRNO_ENOMEM;
				break;
			}
		} else {
			param.stat = req->args.getstatByFd.stat;
		}
		req->args.getstatByFd.result = vfsGetstatByFd(&param);
		if (req->isAsync == 1) {
			req->asyncResult = req->args.getstatByFd.result;
			if (req->fromUser == 1) {
				if (req->args.getstatByFd.result >= 0)
					ksceKernelCopyToUserProc(req->pid, req->args.getstatByFd.stat,
							       param.stat, sizeof(SceIoStat));
				ret = ksceKernelFreeHeapMemory(g_ioSchedHeap, param.stat);
				SCE_DBG_ASSERT(ret == 0, 1916);
			}
		}
		break;
	}
	case IO_REQUEST_DEVCTL: {
		IofilemgrDevctlParam param;

		SCE_DBG_ASSERT(req->isAsync == 0, 1924);
		param.dev = req->args.devctl.dev;
		param.cmd = req->args.devctl.cmd;
		param.indata = req->args.devctl.indata;
		param.inlen = req->args.devctl.inlen;
		param.outdata = req->args.devctl.outdata;
		param.outlen = req->args.devctl.outlen;
		req->args.devctl.result = vfsDevctl(&param);
		break;
	}
	case IO_REQUEST_SYNC: {
		IofilemgrSyncParam param = { req->args.sync.device, req->args.sync.flag };

		ret = vfsSync(&param);
		req->args.sync.result = ret;
		if (req->isAsync == 1) {
			req->asyncResult = ret;
			ret = sceIoAsyncFreePath(req->args.sync.device);
			SCE_DBG_ASSERT(ret == 0, 1949);
		}
		break;
	}
	case IO_REQUEST_SYNC_BY_FD: {
		IofilemgrSyncByFdParam param = { req->args.syncByFd.fd, req->args.syncByFd.flag };

		ret = vfsSyncByFd(&param);
		req->args.syncByFd.result = ret;
		if (req->isAsync == 1)
			req->asyncResult = ret;
		break;
	}
	case IO_REQUEST_GETSTAT_FOR_VSH: {
		IofilemgrGetstatParam param;

		SCE_DBG_ASSERT(req->isAsync == 0, 1969);
		param.file = req->args.getstat.path;
		param.stat = req->args.getstat.stat;
		param.preserveAttributes = 1;
		req->args.getstat.result = vfsGetstat(&param);
		break;
	}
	case IO_REQUEST_CHSTAT_FOR_VSH: {
		IofilemgrChstatParam param;

		SCE_DBG_ASSERT(req->isAsync == 0, 1983);
		param.file = req->args.chstat.path;
		param.stat = req->args.chstat.stat;
		param.bits = req->args.chstat.bits;
		param.forVshbridge = 1;
		req->args.chstat.result = vfsChstat(&param);
		break;
	}
	case IO_REQUEST_DREAD_FOR_VSH: {
		IofilemgrDreadParam param;

		SCE_DBG_ASSERT(req->isAsync == 0, 1998);
		param.fd = req->args.dread.fd;
		param.dir = req->args.dread.dir;
		param.preserveAttributes = 1;
		req->args.dread.result = vfsDread(&param);
		break;
	}
	case IO_REQUEST_DOPEN_ASYNC:
		getIofilemgrTls()->inAsyncCall = 1;
		priority = swapThreadIoPriority(req->priority);
		SCE_DBG_ASSERT((unsigned int)priority <= 15 || priority == (int)0x80000000, 2021);
		req->args.path.result = sceIoDopenInternal(req->args.path.path);
		if (req->args.path.result > 0)
			req->args.path.result = kscePUIDOpenByGUID(req->processId, req->args.path.result);
		ret = setThreadIoPriority(priority);
		SCE_DBG_ASSERT(ret == 0, 2033);
		req->asyncResult = req->args.path.result;
		ret = ksceKernelFreeHeapMemory(g_ioVfsHeapUid, (void *)req->args.path.path);
		SCE_DBG_ASSERT(ret == 0, 2044);
		getIofilemgrTls()->inAsyncCall = 0;
		break;
	case IO_REQUEST_DREAD_ASYNC: {
		SceIoDirent *dir = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(SceIoDirent));

		if (dir == NULL) {
			req->args.dread.result = SCE_ERROR_ERRNO_ENOMEM;
			break;
		}
		getIofilemgrTls()->inAsyncCall = 1;
		req->args.dread.result = sceIoDreadInternal(
			kscePUIDtoGUID(req->processId, req->args.dread.fd), dir);
		ret = ksceKernelCopyToUserProc(req->pid, req->args.dread.dir, dir, sizeof(*dir));
		SCE_DBG_ASSERT(ret == 0, 2092);
		req->asyncResult = req->args.dread.result;
		getIofilemgrTls()->inAsyncCall = 0;
		ret = ksceKernelFreeHeapMemory(g_ioVfsHeapUid, dir);
		SCE_DBG_ASSERT(ret == 0, 2105);
		break;
	}
	case IO_REQUEST_DCLOSE_ASYNC:
		getIofilemgrTls()->inAsyncCall = 1;
		ret = sceIoDcloseInternal(kscePUIDtoGUID(req->processId, req->args.fd.fd));
		req->args.fd.result = ret;
		req->asyncResult = ret;
		if (ret != SCE_ERROR_ERRNO_EBADF) {
			ret = kscePUIDClose(req->processId, req->args.fd.fd);
			SCE_DBG_ASSERT(ret == 0, 2066);
		}
		getIofilemgrTls()->inAsyncCall = 0;
		break;
	default:
		SCE_DBG_ASSERT(0, 2111);
		break;
	}
}

int schedWorkerEntry(SceSize args, void *argp)
{
	IoSchedulerWorker *worker;

	SCE_DBG_ASSERT((int)args == 4, 316);
	worker = *(IoSchedulerWorker **)argp;
	sceIoSchedDispatcherWorker(worker->sched->dispatcher, worker->sched);
	return 0;
}

void forgetIoReq(IoRequest **head, IoRequest **tail, IoRequest *req)
{
	IoRequest *prev = *head;

	if (prev == NULL)
		return;
	if (prev == req) {
		SCE_DBG_ASSERT(req == *head, 531);
		*head = req->waitNext;
		if (req->waitNext == NULL) {
			SCE_DBG_ASSERT(req == *tail, 536);
			*tail = NULL;
		}
	} else {
		while (prev->waitNext != NULL) {
			if (req == prev->waitNext) {
				if (req->waitNext == NULL) {
					SCE_DBG_ASSERT(req == *tail, 536);
					*tail = prev;
					prev->waitNext = NULL;
				}
				if (req->waitNext != NULL)
					prev->waitNext = req->waitNext;
				goto removed;
			}
			prev = prev->waitNext;
		}
		return;
	}
removed:
	req->waitNext = NULL;
}

void sceIoSchedulerSignalCond(IoScheduler *sched, IoRequest *req)
{
	int ret;

	if (req->isWaiting == 1) {
		req->isWaiting = 0;
		if (req->isAsync == 1)
			sched->asyncWaiting = 0;
		ret = ksceKernelSignalCondTo(sched->cond, req->thid);
		if (ret != 0)
			SCE_DBG_PRINTF_LEVEL(1, 0, 595, "%s %d : %x : %d %d\n",
				"sceIoSchedulerSignalCond", 595, ret, req->isWaiting, req->isWaitingIo);
		if (req->priority <= 10 || (req->cacheFlags == 0 && req->isWrite != 1))
			SCE_DBG_ASSERT(ret == 0, 599);
	} else if (req->isAsync == 1 && sched->asyncWaiting == 1) {
		sched->asyncWaiting = 0;
		ret = ksceKernelSignalCondTo(sched->cond, req->thid);
		SCE_DBG_ASSERT(ret == 0, 606);
	}
}

int ksceKernelSetEvent(SceUID event, unsigned int pattern, SceUInt64 data);

int sceIoSchedulerCompleteAsyncIoRequest(IoScheduler *sched, IoRequest *req)
{
	IoAsyncParam param;
	IoAsyncParam *asyncParam;
	int ret = 0, err;

	SCE_DBG_ASSERT(req != NULL, 1243);
	SCE_DBG_ASSERT(req->isAsync == 1, 1244);
	SCE_DBG_ASSERT(req->isRunning == 0, 1245);
	sched->asyncRequestNum--;
	switch (req->type) {
	case IO_REQUEST_READ:
		if (req->args.rw.mapUid != 0) {
			err = ioUserUnmap(req->args.rw.mapUid, req->args.rw.data, req->args.rw.size);
			SCE_DBG_ASSERT(err == 0, 1257);
		}
		req->asyncResult = req->args.rw.result;
		break;
	case IO_REQUEST_WRITE:
		if (req->args.write.mapUid != 0) {
			err = ioUserUnmap(req->args.write.mapUid, req->args.write.data, req->args.write.size);
			SCE_DBG_ASSERT(err == 0, 1269);
		}
		req->asyncResult = req->args.write.result;
		break;
	case IO_REQUEST_PREAD:
		if (req->args.rw.mapUid != 0) {
			err = ioUserUnmap(req->args.rw.mapUid, req->args.rw.data, req->args.rw.size);
			SCE_DBG_ASSERT(err == 0, 1281);
		}
		req->asyncResult = req->args.rw.result;
		break;
	case IO_REQUEST_PWRITE:
		if (req->args.rw.mapUid != 0) {
			err = ioUserUnmap(req->args.rw.mapUid, req->args.rw.data, req->args.rw.size);
			SCE_DBG_ASSERT(err == 0, 1293);
		}
		req->asyncResult = req->args.rw.result;
		break;
	default:
		break;
	}
	asyncParam = req->asyncParam;
	if (asyncParam != NULL) {
		if (req->fromUser) {
			param.result = req->asyncResult;
			param.priority = req->asyncPriority;
			param.completionStatus = 0;
			ret = ksceKernelCopyToUserProc(req->pid, asyncParam, &param, sizeof(param));
			if (ret < 0)
				SCE_DBG_ASSERT(0, 1321);
			asyncParam = req->asyncParam;
		} else {
			asyncParam->result = req->asyncResult;
			asyncParam->completionStatus = 0;
			asyncParam->priority = req->asyncPriority;
		}
	}
	err = ksceKernelSetEvent(req->asyncEvent, 1,
			asyncParam != NULL ? (SceUInt32)asyncParam : req->asyncResult);
	SCE_DBG_ASSERT(err == 0, 1333);
	return ret;
}

int sceIoSchedulerCompleteIoRequest(IoScheduler *sched, IoRequest *req)
{
	int ret;

	SCE_DBG_ASSERT(req != NULL, 1217);
	SCE_DBG_ASSERT(req->isRunning == 0, 1218);
	SCE_DBG_ASSERT((unsigned int)req->priority <= 15, 1220);
	if (req->chunkPool.base != NULL) {
		ret = ksceKernelFreeHeapMemory(g_ioSchedHeap, req->chunkPool.base);
		SCE_DBG_ASSERT(ret == 0, 1225);
	}
	if (req->isAsync == 1)
		return sceIoSchedulerCompleteAsyncIoRequest(sched, req);
	return 0;
}

IoRequest *sceIoRequestQueueGetIoRequest(IoRequestQueue *queue, int *waiting);
int sceIoRequestQueueFinishIoRequest(IoScheduler *sched, IoRequest *req);

/* name unknown: find the highest-priority request or waiting queue. */
IoRequest *sceIoSchedulerGetIoRequest(IoScheduler *sched)
{
	IoRequest *req;
	int waiting = 0, i;

	for (i = 0; i < IO_SCHED_PRIORITY_NUM; i++) {
		req = sceIoRequestQueueGetIoRequest(sched->queue[i], &waiting);
		if (req != NULL || waiting == 1)
			break;
	}
	return req;
}

int sceIoSchedulerRemoveIoRequest(IoScheduler *sched, IoRequest *req)
{
	if (req->isCanceled != 0)
		return SCE_ERROR_ERRNO_EINVAL;
	if (req->isRunning == 1)
		return sceIoRequestQueueFinishIoRequest(sched, req);
	return SCE_ERROR_ERRNO_EBUSY;
}

int sceIoSchedulerIssueAnotherIoRequestInternal(IoScheduler *sched)
{
	IoRequest *req;
	int ret;

	ret = ksceKernelLockMutex(sched->mutex, 1, NULL);
	SCE_DBG_ASSERT(ret == 0, 2272);
	sched->issueCount++;
	req = sceIoSchedulerGetIoRequest(sched);
	if (req != NULL && req->isWaitingIo == 1) {
		if (req->isWaiting == 1) {
			req->isWaiting = 0;
			if (req->isAsync == 1)
				sched->asyncWaiting = 0;
			ret = ksceKernelSignalCondTo(sched->cond, req->thid);
			SCE_DBG_ASSERT(ret == 0, 2288);
		} else if (req->isAsync == 1 && sched->asyncWaiting == 1) {
			sched->asyncWaiting = 0;
			ret = ksceKernelSignalCondTo(sched->cond, req->thid);
			SCE_DBG_ASSERT(ret == 0, 2294);
		}
	} else {
		ret = ksceKernelSetEventFlag(sched->dispatcher->evf, sched->eventBits & 0xff00);
		SCE_DBG_ASSERT(ret == 0, 2301);
	}
	ret = ksceKernelUnlockMutex(sched->mutex, 1);
	SCE_DBG_ASSERT(ret == 0, 2306);
	return 0;
}

int getSchedWorkerName(IoScheduler *sched)
{
	char name[32];
	int index;

	SCE_DBG_ASSERT(strnlen("SceIoSchedWorker", 32) + 2 <= 32, 2316);
	switch (sched->type) {
	case 0x100:
		index = 0;
		break;
	case 0x201:
	case 0x203:
		index = 1;
		break;
	case 0x202:
		index = 2;
		break;
	case 0x300:
		index = 3;
		break;
	default:
		index = 4;
		break;
	}
	snprintf(name, sizeof(name), "%s%d", "SceIoSchedWorker", index);
	sched->worker.thid = ksceKernelCreateThread(name, schedWorkerEntry, 100, 0x2000,
						 0x10000000, 0x80000, NULL);
	if (sched->worker.thid < 0)
		return sched->worker.thid;
	sched->worker.sched = sched;
	sched->asyncRequestNum = 0;
	sched->workerResult = 0;
	sched->asyncWaiting = 0;
	sched->worker.mutex = sched->mutex;
	sched->worker.cond = sched->cond;
	return 0;
}

/* name unknown: allocates one priority queue. */
IoRequestQueue *sceIoRequestQueueCreateIoRequestQueue(IoScheduler *sched, int priority);

IoScheduler *sceIoSchedulerCreateIoScheduler(SchedDispatcher *dispatcher,
					   const IoSchedulerAttr *attr)
{
	IoScheduler *sched;
	IoSchedulerWorker *worker;
	int i, type, ret;

	sched = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*sched));
	if (sched == NULL)
		return NULL;
	memset(sched, 0, sizeof(*sched));
	sched->type = attr->type;
	for (i = 0; i < IO_SCHED_PRIORITY_NUM; i++)
		sched->queue[i] = sceIoRequestQueueCreateIoRequestQueue(sched, i);
	sched->dispatcher = dispatcher;
	sched->readChunkSize = attr->readChunkSize;
	sched->writeChunkSize = attr->writeChunkSize;
	sched->mutex = ksceKernelCreateMutex("SceIoScheduler", 0x2002, 0, NULL);
	if (sched->mutex < 0)
		goto free;
	sched->cond = ksceKernelCreateCond("SceIoScheduler", 0x2000, sched->mutex, NULL);
	if (sched->cond < 0)
		goto deleteMutex;
	sched->runningNum = 0;
	sched->reserved0 = 0;
	sched->issueCount = 0;
	sched->reserved2 = 0;
	ret = getSchedWorkerName(sched);
	if (ret < 0)
		goto deleteCond;
	worker = &sched->worker;
	ret = ksceKernelStartThread(sched->worker.thid, sizeof(worker), &worker);
	if (ret < 0)
		goto deleteCond;
	sched->writeDelay = 0;
	sched->runningRequest = NULL;
	sched->priorityWaitStart = 0;
	sched->writeDelayStart = 0;
	sched->reserved3 = 0;
	sched->runningHead = NULL;
	sched->runningTail = NULL;
	type = sched->type;
	if (type >= 0)
		type = (SceUInt16)type;
	switch (type) {
	case 0x100:
		sched->eventBits |= 0x101;
		break;
	case 0x201:
		sched->eventBits |= 0x202;
		break;
	case 0x202:
		sched->eventBits |= 0x404;
		break;
	case 0x300:
		sched->eventBits |= 0x808;
		break;
	default:
		sched->eventBits |= 0x1010;
		break;
	}
	return sched;
deleteCond:
	ksceKernelDeleteCond(sched->cond);
deleteMutex:
	ksceKernelDeleteMutex(sched->mutex);
free:
	ksceKernelFreeHeapMemory(g_ioSchedHeap, sched);
	return NULL;
}

#if FW_VERSION >= 0x04000000
/* name unknown: the operations that transfer file data. */
int sceIoSchedulerIsDataIo(IoRequest *req)
{
	switch (req->type) {
	case IO_REQUEST_READ:
	case IO_REQUEST_WRITE:
	case IO_REQUEST_PREAD:
	case IO_REQUEST_PWRITE:
		return 1;
	default:
		return 0;
	}
}
#endif

/* name unknown: the worker's identity before it adopts the requester's. */
typedef struct IoSchedulerAcl {
	int isSet;
	SceUID processId;
	int permission;
} IoSchedulerAcl;

void sceIoSchedulerPretendRequesterAcl(IoRequest *req, IoSchedulerAcl *acl)
{
	acl->isSet = 1;
	acl->processId = ksceKernelSetProcessIdToTLS(req->processId);
	SCE_DBG_ASSERT(acl->processId >= 0, 2488);
	acl->permission = ksceKernelSetPermission(req->threadAcl);
	SCE_DBG_ASSERT(acl->permission >= 0, 2492);
}

void sceIoSchedulerRestoreAcl(IoSchedulerAcl *acl)
{
	int ret;

	if (acl->isSet) {
		ret = ksceKernelSetProcessIdToTLS(acl->processId);
		SCE_DBG_ASSERT(ret >= 0, 2506);
		ret = ksceKernelSetPermission(acl->permission);
		SCE_DBG_ASSERT(ret >= 0, 2509);
	}
}

int sceIoRequestQueueRemoveIoRequest(IoRequestQueue *queue, IoRequest *req);

int sceIoSchedulerCancelAsyncIoRequest(IoScheduler *sched, IoRequest *req)
{
	IoRequestQueue *queue;
	int ret, err;

	err = ksceKernelLockMutex(sched->mutex, 1, NULL);
	SCE_DBG_ASSERT(err == 0, 2521);
	if ((unsigned int)req->priority <= 15) {
		queue = sched->queue[req->priority];
		SCE_DBG_ASSERT(queue != NULL, 2525);
		ret = sceIoRequestQueueRemoveIoRequest(queue, req);
		if (ret == 0)
			sched->asyncRequestNum--;
	} else {
		ret = SCE_ERROR_ERRNO_EINVAL;
	}
	err = ksceKernelUnlockMutex(sched->mutex, 1);
	SCE_DBG_ASSERT(err == 0, 2537);
	return ret;
}

/* name unknown: storage schedulers use the 0x100 and 0x200 groups. */
int sceIoSchedulerIsStorageIo(IoRequest *req)
{
	int type = req->mntOpt & 0xf00;

	switch (type) {
	case 0x100:
	case 0x200:
		return 1;
	default:
		return 0;
	}
}

/* name unknown: whether the data transfer exceeds its chunk size. */
int sceIoSchedulerNeedsIoChunks(IoScheduler *sched, IoRequest *req)
{
	SceSize size = 0;

	if (req->priority > 10 && (req->cacheFlags != 0 || req->isWrite == 1) &&
	    req->chunkAlignment == 1)
		size = req->cacheFlags;
	switch (req->type) {
	case IO_REQUEST_READ:
	case IO_REQUEST_PREAD:
		if (size == 0)
			size = sched->readChunkSize;
		break;
	case IO_REQUEST_WRITE:
	case IO_REQUEST_PWRITE:
		if (size == 0)
			size = sched->writeChunkSize;
		break;
	default:
		return 0;
	}
	if (size != 0)
		return req->args.rw.size > size;
	return 0;
}

int sceIoRequestQueueAddIoRequest(IoRequestQueue *queue, IoRequest *req);

int sceIoSchedulerAddIoRequest(IoScheduler *sched, IoRequest *req)
{
	SceSize chunkSize, offset;
	int priority = req->priority;
	int ret;

	if (req->next != NULL)
		SCE_DBG_ASSERT(0, 2138);
	if (priority > 10 && (req->cacheFlags != 0 || req->isWrite == 1) &&
	    sched->dispatcher->chunkAlignmentOverride > 0) {
		req->chunkAlignment = 1;
		goto aligned;
	}
	if ((req->mntOpt & 0x10000) &&
	    (req->type == IO_REQUEST_READ || req->type == IO_REQUEST_PREAD)) {
		ret = sceIoSchedulerNeedsIoChunks(sched, req);
		priority = req->priority;
		if (ret == 1) {
			ret = ksceKernelCheckDipsw(0xe7);
			priority = req->priority;
			if (ret == 0) {
				if (priority > 10 && (req->cacheFlags != 0 || req->isWrite == 1))
					req->chunkAlignment = 1;
				goto aligned;
			}
		}
	}
	switch (req->type) {
	case IO_REQUEST_READ:
	case IO_REQUEST_PREAD:
		chunkSize = sched->readChunkSize;
		break;
	case IO_REQUEST_WRITE:
	case IO_REQUEST_PWRITE:
		chunkSize = sched->writeChunkSize;
		break;
	default:
		req->doneChunkNum = 0;
		goto add;
	}
	if (chunkSize != 0)
		req->doneChunkNum = (chunkSize + req->ioRemain - 1) / chunkSize;
	else
		req->doneChunkNum = req->ioRemain != 0;
	goto add;
aligned:
	switch (req->type) {
	case IO_REQUEST_READ:
	case IO_REQUEST_WRITE:
	case IO_REQUEST_PREAD:
	case IO_REQUEST_PWRITE:
		break;
	default:
		goto add;
	}
	chunkSize = 0;
	if (req->priority > 10 && (req->cacheFlags != 0 || req->isWrite == 1)) {
		chunkSize = req->cacheFlags;
	} else {
		switch (req->type) {
		case IO_REQUEST_READ:
		case IO_REQUEST_PREAD:
			chunkSize = sched->readChunkSize;
			break;
		case IO_REQUEST_WRITE:
		case IO_REQUEST_PWRITE:
			chunkSize = sched->writeChunkSize;
			break;
		default:
			SCE_DBG_ASSERT(0, 2169);
			break;
		}
	}
	switch (req->type) {
	case IO_REQUEST_READ:
	case IO_REQUEST_WRITE:
		offset = (SceUInt32)getFilePosition(req) & (chunkSize - 1);
		priority = req->priority;
		break;
	case IO_REQUEST_PREAD:
	case IO_REQUEST_PWRITE:
		offset = (SceUInt32)req->args.rw.offset & (chunkSize - 1);
		break;
	default:
		SCE_DBG_ASSERT(0, 2188);
		offset = 0;
		break;
	}
	if (offset != 0 && req->ioRemain != 0) {
		SceUInt32 chunks = (req->ioRemain - 1 + offset) / chunkSize;

		req->chunkOffset = offset;
		req->doneChunkNum = chunks + 1;
	} else {
		req->doneChunkNum = (req->ioRemain + chunkSize - 1) / chunkSize;
		req->chunkOffset = 0;
	}
add:
	return sceIoRequestQueueAddIoRequest(sched->queue[priority], req);
}

void sceIoSchedulerAddAsyncIoRequest(IoScheduler *sched, IoRequest *req)
{
	SchedDispatcher *dispatcher = sched->dispatcher;
	int ret;

	SCE_DBG_ASSERT(req->isAsync == 1, 272);
	ret = ksceKernelLockMutex(sched->mutex, 1, NULL);
	SCE_DBG_ASSERT(ret == 0, 275);
	SCE_DBG_ASSERT(req->thid == 0, 277);
	req->thid = sched->worker.thid;
	ret = sceIoSchedulerAddIoRequest(sched, req);
	SCE_DBG_ASSERT(ret == 0, 281);
	sched->asyncRequestNum++;
	ret = ksceKernelUnlockMutex(sched->mutex, 1);
	SCE_DBG_ASSERT(ret == 0, 298);
	ret = ksceKernelSetEventFlag(dispatcher->evf, (SceUInt8)sched->eventBits);
	SCE_DBG_ASSERT(ret == 0, 308);
}

/* name unknown: storage and host requests may need several chunks. */
int sceIoSchedulerNeedsStorageIoChunks(IoScheduler *sched, IoRequest *req)
{
	int type = req->mntOpt & 0xf00;

	switch (type) {
	case 0x200:
	case 0x300:
	case 0x100:
		return sceIoSchedulerNeedsIoChunks(sched, req);
	default:
		return 0;
	}
}

int sceKernelChangeThreadPriorityAndCpuAffinityMask(SceUID thid, int priority,
						 int affinity, int reserved, int flags);
int sceIoRequestQueueChunkIoRequest(IoScheduler *sched, IoRequest *req, int count);
int sceIoRequestQueueExecuteIoChunk(IoRequest *req, IoChunk *chunk);
int sceIoRequestQueueFinishAndDestroyIoChunk(IoRequest *req, IoChunk *chunk, int result);

static inline __attribute__((always_inline)) int isDataIoReq(IoRequest *req)
{
	switch (req->type) {
	case IO_REQUEST_READ:
	case IO_REQUEST_WRITE:
	case IO_REQUEST_PREAD:
	case IO_REQUEST_PWRITE:
		return 1;
	default:
		return 0;
	}
}

static inline __attribute__((always_inline)) void rememberIoReq(IoRequest **head,
							     IoRequest **tail, IoRequest *req)
{
	if (*head == NULL) {
		SCE_DBG_ASSERT(*tail == NULL, 509);
		*head = req;
	} else {
		SCE_DBG_ASSERT((*tail)->waitNext == NULL, 513);
		(*tail)->waitNext = req;
	}
	*tail = req;
	req->waitNext = NULL;
}

static inline __attribute__((always_inline)) int isIoReqStarted(IoRequest *req)
{
	switch (req->type) {
	case IO_REQUEST_READ:
	case IO_REQUEST_WRITE:
	case IO_REQUEST_PREAD:
	case IO_REQUEST_PWRITE:
		return req->ioRemain < req->args.rw.size;
	default:
		SCE_DBG_ASSERT(0, 575);
		return 0;
	}
}

int sceIoSchedulerExecuteIoRequestCore(IoScheduler *sched, IoRequest *request)
{
	IoSchedulerAcl acl;
	IoRequest *req, *next;
	IoChunk *chunk;
	IofilemgrTls *tls;
	SceUID thid, oldThid;
	SceUInt64 now;
	SceUInt32 timeout, delay;
	int ret, result, finished, stackSize, removed;

	thid = ksceKernelGetThreadId();
	vfs_increment_io_count();
	for (;;) {
		req = sceIoSchedulerGetIoRequest(sched);
		if (req == NULL)
			break;
		if (req->thid != thid) {
			if (!getIofilemgrTls()->isCountedWorker) {
				SCE_DBG_ASSERT(req->thid != 0, 663);
				sceIoSchedulerSignalCond(sched, req);
				if (request != NULL) {
					request->isWaiting = 1;
					SCE_DBG_ASSERT(request->thid == thid, 670);
				}
				if (sched->worker.thid == thid)
					sched->asyncWaiting = 1;
				ret = ksceKernelWaitCond(sched->cond, NULL);
				SCE_DBG_ASSERT(ret == 0, 678);
				if (sched->worker.thid == thid)
					SCE_DBG_ASSERT(sched->asyncWaiting == 0, 682);
				if (request != NULL)
					SCE_DBG_ASSERT(request->isWaiting == 0, 686);
				continue;
			}
			if (!isDataIoReq(req) && req->isWaiting == 1) {
				sceIoSchedulerSignalCond(sched, req);
				break;
			}
			SCE_DBG_ASSERT(req->priority <= 10 ||
				       (req->cacheFlags == 0 && req->isWrite != 1), 699);
			if (sched->runningNum > 1) {
				SCE_DBG_ASSERT_CHECK(sched->runningNum <= 5, 704);
				break;
			}
			SCE_DBG_ASSERT(sched->issueCount >= 0, 708);
			if (sched->issueCount == 0)
				break;
		} else if (sched->runningNum > 0) {
			if (sched->runningNum == 1 && sched->issueCount > 0 &&
			    (req->priority <= 10 || (req->cacheFlags == 0 && req->isWrite != 1)))
				goto execute;
			SCE_DBG_ASSERT(1, 719);
			if (req->priority <= 10 && sched->priorityWaitStart == 0 &&
			    sched->runningRequest != NULL && sched->runningRequest->priority > 10 &&
			    sched->runningRequest->isWrite == 1)
				sched->priorityWaitStart = sceKernelGetSystemTimeWide();
			req->isWaiting = 1;
			req->isWaitingIo = 1;
			ret = ksceKernelWaitCond(sched->cond, NULL);
			SCE_DBG_ASSERT(ret == 0, 744);
			SCE_DBG_ASSERT(req->isWaiting == 0, 747);
			req->isWaitingIo = 0;
			continue;
		} else if (req->priority > 10 && (req->cacheFlags != 0 || req->isWrite == 1)) {
			now = sceKernelGetSystemTimeWide();
			if (req->chunkAlignment != 1)
				goto execute;
			if (req->doneChunkNum == 0 && sceIoSchedulerNeedsStorageIoChunks(sched, req))
				SCE_DBG_ASSERT(0, 768);
			timeout = 0;
			if (sched->writeDelayStart != 0 && now - sched->writeDelayStart < sched->writeDelay)
				timeout = sched->writeDelayStart + sched->writeDelay - now;
			if (timeout == 0) {
				sched->writeDelay = 0;
				sched->writeDelayStart = 0;
				goto execute;
			}
			req->isWaiting = 1;
			if (sched->worker.thid == thid)
				sched->asyncWaiting = 1;
			ret = ksceKernelWaitCond(sched->cond, &timeout);
			SCE_DBG_ASSERT(ret == 0 || ret == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT, 796);
			if (sched->worker.thid == thid)
				SCE_DBG_ASSERT(sched->asyncWaiting == 0, 800);
			if (ret == 0) {
				SCE_DBG_ASSERT(req->isWaiting == 0, 805);
			} else {
				req->isWaiting = 0;
				if (sched->worker.thid == thid)
					sched->asyncWaiting = 0;
			}
			continue;
		}
execute:
		if (getIofilemgrTls()->isCountedWorker == 1) {
			ret = sceKernelChangeThreadPriorityAndCpuAffinityMask(thid, req->threadPriority,
								    req->cpuAffinityMask, 0, 0);
			if (ret < 0)
				SCE_DBG_PRINTF_LEVEL(1, 0, 829,
					"%s %d : sceKernelChangeThreadCpuAffinityMask() failed. %x\n",
					"sceIoSchedulerExecuteIoRequestCore", 829, ret);
		}
		if (sched->issueCount > 0)
			sched->issueCount--;
		SCE_DBG_ASSERT(1, 837);
		if (request != NULL)
			SCE_DBG_ASSERT(req == request, 839);
		sched->runningNum++;
		if ((req->priority <= 10 || (req->cacheFlags == 0 && req->isWrite != 1)) &&
		    sched->priorityWaitStart != 0) {
			now = sceKernelGetSystemTimeWide();
			sched->writeDelayStart = now;
			delay = 10 * ((SceUInt32)now - (SceUInt32)sched->priorityWaitStart);
			sched->priorityWaitStart = 0;
			sched->writeDelay = delay < 400000 ? delay : 400000;
		}
		req->isRunning = 1;
		sched->runningRequest = req;
		rememberIoReq(&sched->runningHead, &sched->runningTail, req);
		chunk = NULL;
		if (isDataIoReq(req)) {
			if (req->type == IO_REQUEST_READ && req->args.rw.offset < 0)
				req->args.rw.offset = getFilePosition(req);
			ret = sceIoRequestQueueChunkIoRequest(sched, req, 8);
			if (ret > 8)
				SCE_DBG_ASSERT(0, 910);
			chunk = sceIoRequestPopIoChunk(req);
		}
		iofile_daemon_notify_write(req->mntOpt);
		ret = ksceKernelUnlockMutex(sched->mutex, 1);
		SCE_DBG_ASSERT(ret == 0, 922);
		sceIoSchedulerPretendRequesterAcl(req, &acl);
		getIofilemgrTls()->request = req;
		vfs_set_sync_pending();
		tls = getIofilemgrTls();
		oldThid = tls->requesterThreadId;
		tls->requesterThreadId = req->thid;
		if (isDataIoReq(req)) {
			result = sceIoRequestQueueExecuteIoChunk(req, chunk);
		} else {
			stackSize = 0x2000;
			if ((req->mntOpt & 0x10000) && req->type == IO_REQUEST_IOCTL &&
			    (req->args.ioctl.cmd & 0x4000))
				stackSize = 0x3000;
			ksceKernelRunWithStack(stackSize, (int (*)(void *))sceIoSchedulerExecuteIoCommand, req);
			result = 0;
		}
		tls->requesterThreadId = oldThid;
		getIofilemgrTls()->request = NULL;
		sceIoSchedulerRestoreAcl(&acl);
		ret = ksceKernelLockMutex(sched->mutex, 1, NULL);
		SCE_DBG_ASSERT(ret == 0, 996);
		if (isDataIoReq(req)) {
			if (result == (int)0x80142318) {
				sceIoRequestPushIoChunk(req, chunk);
				if (!isIoReqStarted(req)) {
					forgetIoReq(&sched->runningHead, &sched->runningTail, req);
					req->isRunning = 0;
				}
				if (getIofilemgrTls()->isCountedWorker == 1)
					sched->issueCount++;
			} else {
				ret = sceIoRequestQueueFinishAndDestroyIoChunk(req, chunk, result);
				SCE_DBG_ASSERT(ret == 0, 1024);
			}
		} else if ((req->type == IO_REQUEST_CHSTAT && (req->args.chstat.bits & 4) &&
			    req->args.chstat.result == (int)0x80142318) ||
			   (req->type == IO_REQUEST_CHSTAT_BY_FD && (req->args.chstatByFd.bits & 4) &&
			    req->args.chstatByFd.result == (int)0x80142318) ||
			   (req->type == IO_REQUEST_OPEN && (req->args.open.flags & 0x400) &&
			    req->args.open.result == (int)0x80142318)) {
			sched->runningNum--;
			continue;
		}
		if (req->doneChunkNum != 0 && req->runningChunkNum == req->doneChunkNum &&
		    req->thid == thid) {
			do {
				req->isWaiting = 1;
				if (sched->worker.thid == thid)
					sched->asyncWaiting = 1;
				ret = ksceKernelWaitCond(sched->cond, NULL);
				SCE_DBG_ASSERT(ret == 0, 1071);
				if (sched->worker.thid == thid)
					SCE_DBG_ASSERT(sched->asyncWaiting == 0, 1075);
				SCE_DBG_ASSERT(req->isWaiting == 0, 1077);
				finished = sceIoRequestIsFinished(req);
			} while (!finished);
		} else {
			finished = sceIoRequestIsFinished(req);
		}
		if (finished == 1) {
			if (req->thid == thid) {
				removed = 0;
				if (req->isCanceled == 0 && req->isRunning == 1)
					removed = sceIoRequestQueueFinishIoRequest(sched, req) == 0;
				SCE_DBG_ASSERT(removed, 1096);
				req->isCanceled = 1;
				if (sched->runningRequest == req)
					sched->runningRequest = NULL;
				forgetIoReq(&sched->runningHead, &sched->runningTail, req);
				if (request != NULL)
					SCE_DBG_ASSERT(req == request, 1108);
				ret = sceIoSchedulerCompleteIoRequest(sched, req);
				if (ret != 0)
					SCE_DBG_ASSERT(0, 1115);
				next = sceIoSchedulerGetIoRequest(sched);
				if (next != NULL) {
					SCE_DBG_ASSERT(next != request, 1120);
					if (next->thid != thid)
						sceIoSchedulerSignalCond(sched, next);
				}
				sched->runningNum--;
				break;
			}
			if (req->isCanceled == 0)
				sceIoSchedulerSignalCond(sched, req);
		}
		sched->runningNum--;
		if (getIofilemgrTls()->isCountedWorker == 1) {
			if (sched->runningNum == 0) {
				next = sceIoSchedulerGetIoRequest(sched);
				if (next != NULL)
					sceIoSchedulerSignalCond(sched, next);
			}
			if (result != (int)0x80142318)
				break;
		}
	}
	if (getIofilemgrTls()->isCountedWorker == 1) {
		ret = sceKernelChangeThreadPriorityAndCpuAffinityMask(thid, 100, 0x80000, 0, 0);
		if (ret < 0)
			SCE_DBG_PRINTF_LEVEL(1, 0, 1203,
				"%s %d : sceKernelChangeThreadCpuAffinityMask() failed. %x\n",
				"sceIoSchedulerExecuteIoRequestCore", 1203, ret);
	}
	vfs_decrement_io_count();
	return 0;
}

int sceIoSchedulerExecuteSyncIoRequest(IoScheduler *sched, IoRequest *req)
{
	int ret, result;

	if (req->next != NULL)
		SCE_DBG_ASSERT(0, 233);
	ret = ksceKernelLockMutex(sched->mutex, 1, NULL);
	SCE_DBG_ASSERT(ret == 0, 237);
	ret = sceIoSchedulerAddIoRequest(sched, req);
	SCE_DBG_ASSERT(ret == 0, 240);
	result = sceIoSchedulerExecuteIoRequestCore(sched, req);
	ret = ksceKernelUnlockMutex(sched->mutex, 1);
	SCE_DBG_ASSERT(ret == 0, 262);
	return result;
}

int sceIoSchedulerExecuteIoRequestWorkerForDispatcher(IoScheduler *sched)
{
	int ret, result;

	ret = ksceKernelLockMutex(sched->mutex, 1, NULL);
	SCE_DBG_ASSERT(ret == 0, 345);
	for (;;) {
		if (getIofilemgrTls()->isCountedWorker == 1) {
			if (sched->runningNum != 1 && sched->asyncRequestNum == 0)
				break;
		} else if (sched->asyncRequestNum == 0) {
			break;
		}
		ret = sceIoSchedulerExecuteIoRequestCore(sched, NULL);
		SCE_DBG_ASSERT(ret == 0, 359);
		if (getIofilemgrTls()->isCountedWorker == 1)
			break;
	}
	result = sched->workerResult;
	ret = ksceKernelUnlockMutex(sched->mutex, 1);
	SCE_DBG_ASSERT(ret == 0, 372);
	return result;
}
