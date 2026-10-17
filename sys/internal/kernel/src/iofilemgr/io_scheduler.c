/*
 * SceIofilemgr: I/O priorities and the calls the I/O scheduler runs for the
 * vfs (Sony's io_scheduler.c).
 *
 * Each sceIoScheduler* call builds an IoRequest on the scheduler heap, hands it
 * to the dispatcher and waits for its result.
 */

#include <intrmgr/syscall.h>
#include "iofilemgr_internal.h"

SceUID g_ioSchedHeap;

/* 4.00 builds this file with _FORTIFY_SOURCE; Sony's heap declaration gives the size it checks */
#if FW_VERSION >= 0x04000000
void *ksceKernelAllocHeapMemory(SceUID uid, SceSize size) __attribute__((alloc_size(2)));
#define strncpy(dst, src, len)	__builtin___strncpy_chk(dst, src, len, __builtin_object_size(dst, 1))
#endif

/* the hook events of a scheduler call: its vop's event minus two, then the result */
#define IO_SCHED_HOOK_ENTER(vop)	((vop) - 2)
#define IO_SCHED_HOOK_EXIT(vop)		((vop) - 1)

/* each request reads or writes in this many chunks */
#define IO_SCHED_CHUNK_BUFF_SIZE	(IO_REQUEST_CHUNK_NUM * sizeof(IoChunk))

int ksceSblACMgrIsNonGameOrGameProgram2(SceUID pid);
SceUID sceKernelGetProcessId(void);
int SceThreadmgrForDriver_1096042E(SceUID thid);

SceInt32 g_ioDefaultPriority = 1;	/* the system processes' default I/O priority (name unknown) */

/* the system processes share one default; the others keep theirs in the KPLS */
static inline __attribute__((always_inline)) int isSystemIoProcess(SceUID pid)
{
	return pid == SCE_KERNEL_DUMMY_PROCESS_GAME_ID || pid == SCE_KERNEL_DUMMY_PROCESS_SYSTEM_ID;
}

int sceIoSetProcessDefaultPriorityInternal(SceInt32 priority, int fromKernel)
{
	IofilemgrKpls *kpls = NULL;
	SceInt32 *defaultPriority;
	SceUInt32 syscall;
	SceUID pid;
	int nonGame, ret;

	pid = ksceKernelGetProcessIdFromTLS();
	syscall = sceSyscallEnter();
	if (ksceSblACMgrIsRootProgram2(ksceKernelGetProcessIdFromTLS()) == 1 ||
	    isSystemIoProcess(pid)) {
		defaultPriority = &g_ioDefaultPriority;
	} else {
		ret = ksceKernelGetProcessLocalStorageAddrForPid(ksceKernelGetProcessIdFromTLS(),
								 g_iofilemgrKplsKey, (void **)&kpls,
								 1);
		if (ret < 0)
			goto out;
		defaultPriority = &kpls->priority;
	}

	if (ksceSblACMgrIsNonGameOrGameProgram2(ksceKernelGetProcessIdFromTLS()) == 1) {
		if (fromKernel == 1) {
			if (SceThreadmgrForDriver_1096042E(0) == 0 &&
			    !IO_PRIORITY_IS_CDIALOG(priority)) {
				SCE_DBG_ASSERT(0, 113);
				sceSyscallLeave(syscall);
				return SCE_ERROR_ERRNO_EPERM;
			}
			if (!IO_PRIORITY_IS_GAME(priority) && !IO_PRIORITY_IS_SYSTEM(priority) &&
			    !IO_PRIORITY_IS_CDIALOG(priority)) {
				if (priority != IO_PRIORITY_NON_GAME)
					goto invalid;
				goto set;
			}
		} else {
			nonGame = ksceSblACMgrIsNonGameProgram2(ksceKernelGetProcessIdFromTLS());
			priority += IO_PRIORITY_GAME_BASE;
			if (nonGame == 1) {
				if (priority != IO_PRIORITY_NON_GAME)
					goto invalid;
			} else if ((SceUInt32)(priority - IO_PRIORITY_GAME_BASE) > IO_PRIORITY_GAME_MAX) {
				goto invalid;
			}
			goto set;
		}
	}
	if ((SceUInt32)priority > IO_PRIORITY_MAX) {
		ret = SCE_ERROR_ERRNO_EINVAL;
		goto out;
	}
set:
	*defaultPriority = priority;
	ret = 0;
out:
	sceSyscallLeave(syscall);
	return ret;

invalid:
	sceSyscallLeave(syscall);
	return SCE_ERROR_ERRNO_EINVAL;
}

int ksceIoSetProcessDefaultPriorityForSystem(SceInt32 priority)
{
	return sceIoSetProcessDefaultPriorityInternal(priority, 1);
}

SceInt32 sceIoGetProcessDefaultPriorityInternal(int fromKernel)
{
	IofilemgrKpls *kpls = NULL;
	SceUInt32 syscall;
	SceInt32 priority;

	syscall = sceSyscallEnter();
	if (ksceSblACMgrIsRootProgram2(ksceKernelGetProcessIdFromTLS()) == 1 ||
	    ksceKernelGetProcessIdFromTLS() == SCE_KERNEL_DUMMY_PROCESS_GAME_ID ||
	    ksceKernelGetProcessIdFromTLS() == SCE_KERNEL_DUMMY_PROCESS_SYSTEM_ID) {
		priority = g_ioDefaultPriority;
	} else {
		priority = ksceKernelGetProcessLocalStorageAddrForPid(ksceKernelGetProcessIdFromTLS(),
								      g_iofilemgrKplsKey,
								      (void **)&kpls, 1);
		if (priority < 0) {
			sceSyscallLeave(syscall);
			return priority;
		}
		priority = kpls->priority;
	}

	if (ksceSblACMgrIsNonGameOrGameProgram2(ksceKernelGetProcessIdFromTLS()) == 1) {
		if (fromKernel == 1) {
			SCE_DBG_ASSERT(IO_PRIORITY_IS_SYSTEM(priority) ||
				       (SceUInt32)(priority - 6) <= 8, 183);
		} else if (ksceSblACMgrIsNonGameProgram2(ksceKernelGetProcessIdFromTLS()) == 1) {
			SCE_DBG_ASSERT(priority == IO_PRIORITY_NON_GAME, 198);
			priority -= IO_PRIORITY_GAME_BASE;
		} else {
			priority -= IO_PRIORITY_GAME_BASE;
			SCE_DBG_ASSERT((SceUInt32)priority <= IO_PRIORITY_GAME_MAX, 201);
		}
	} else {
		SCE_DBG_ASSERT((SceUInt32)priority <= IO_PRIORITY_MAX, 208);
	}
	sceSyscallLeave(syscall);
	return priority;
}

SceInt32 ksceIoGetProcessDefaultPriorityForSystem(void)
{
	return sceIoGetProcessDefaultPriorityInternal(1);
}

int sceIoSetThreadDefaultPriorityInternal(SceInt32 priority, int fromKernel)
{
	IofilemgrTls *tls;
	SceUInt32 syscall;
	int nonGame;

	syscall = sceSyscallEnter();
	tls = getIofilemgrTls();
	if (ksceSblACMgrIsNonGameOrGameProgram2(ksceKernelGetProcessIdFromTLS()) == 1) {
		if (fromKernel == 1) {
			if (SceThreadmgrForDriver_1096042E(0) == 0 &&
			    !IO_PRIORITY_IS_CDIALOG(priority)) {
				SCE_DBG_ASSERT(0, 234);
				sceSyscallLeave(syscall);
				return SCE_ERROR_ERRNO_EPERM;
			}
			if (!IO_PRIORITY_IS_GAME(priority) && !IO_PRIORITY_IS_SYSTEM(priority) &&
			    !IO_PRIORITY_IS_CDIALOG(priority)) {
				if (priority != IO_PRIORITY_NON_GAME)
					goto invalid;
				goto set;
			}
		} else {
			nonGame = ksceSblACMgrIsNonGameProgram2(ksceKernelGetProcessIdFromTLS());
			priority += IO_PRIORITY_GAME_BASE;
			if (nonGame == 1) {
				if (priority != IO_PRIORITY_NON_GAME)
					goto invalid;
			} else if ((SceUInt32)(priority - IO_PRIORITY_GAME_BASE) > IO_PRIORITY_GAME_MAX) {
				goto invalid;
			}
			goto set;
		}
	}
	if ((SceUInt32)priority > IO_PRIORITY_MAX) {
		sceSyscallLeave(syscall);
		return SCE_ERROR_ERRNO_EINVAL;
	}
set:
	tls->priority = priority;
	sceSyscallLeave(syscall);
	return 0;

invalid:
	sceSyscallLeave(syscall);
	return SCE_ERROR_ERRNO_EINVAL;
}

int ksceIoSetThreadDefaultPriorityForSystem(SceInt32 priority)
{
	return sceIoSetThreadDefaultPriorityInternal(priority, 1);
}

SceInt32 sceIoGetThreadDefaultPriorityInternal(int fromKernel)
{
	IofilemgrKpls *kpls = NULL;
	IofilemgrTls *tls;
	SceUInt32 syscall;
	SceInt32 priority;
	SceUID pid;

	pid = ksceKernelGetProcessIdFromTLS();
	syscall = sceSyscallEnter();
	tls = getIofilemgrTls();
	if (tls == NULL || (priority = tls->priority) == IO_PRIORITY_UNSET) {
		if (ksceSblACMgrIsRootProgram2(pid) == 1 || isSystemIoProcess(pid)) {
			priority = g_ioDefaultPriority;
		} else {
			priority = ksceKernelGetProcessLocalStorageAddrForPid(
				ksceKernelGetProcessIdFromTLS(), g_iofilemgrKplsKey, (void **)&kpls, 1);
			if (priority < 0) {
				sceSyscallLeave(syscall);
				return priority;
			}
			priority = kpls->priority;
		}
	}

	if (ksceSblACMgrIsNonGameOrGameProgram2(pid) == 1) {
		if (fromKernel == 1) {
			if (!IO_PRIORITY_IS_SYSTEM(priority) && (SceUInt32)(priority - 6) > 8)
				priority = SCE_ERROR_ERRNO_EINVAL;
		} else if (ksceSblACMgrIsNonGameProgram2(ksceKernelGetProcessIdFromTLS()) == 1) {
			SCE_DBG_ASSERT(priority == IO_PRIORITY_NON_GAME, 334);
			priority -= IO_PRIORITY_GAME_BASE;
		} else {
			SCE_DBG_ASSERT((SceUInt32)(priority - IO_PRIORITY_GAME_BASE) <=
				       IO_PRIORITY_GAME_MAX, 337);
			priority -= IO_PRIORITY_GAME_BASE;
		}
	} else if ((SceUInt32)priority > IO_PRIORITY_MAX) {
		priority = SCE_ERROR_ERRNO_EINVAL;
	}
	sceSyscallLeave(syscall);
	return priority;
}

SceInt32 ksceIoGetThreadDefaultPriorityForSystem(void)
{
	return sceIoGetThreadDefaultPriorityInternal(1);
}

int sceIoSetPriorityInternal(SceUID fd, SceInt32 priority, int fromKernel)
{
	IofilemgrSchedInfo info;
	SceUInt32 syscall;
	int ret;

	syscall = sceSyscallEnter();
	ret = getFileSchedParam(fd, &info, 0);
	if (ret != 0)
		goto out;

	if (ksceSblACMgrIsNonGameOrGameProgram2(ksceKernelGetProcessIdFromTLS()) == 1) {
		if (fromKernel == 1) {
			if (SceThreadmgrForDriver_1096042E(0) == 0 &&
			    !IO_PRIORITY_IS_CDIALOG(priority)) {
				SCE_DBG_ASSERT(0, 376);
				ret = SCE_ERROR_ERRNO_EPERM;
				goto out;
			}
			if (!IO_PRIORITY_IS_SYSTEM(priority) && (SceUInt32)(priority - 6) > 8)
				goto invalid;
		} else if (ksceSblACMgrIsNonGameProgram2(ksceKernelGetProcessIdFromTLS()) == 1) {
			if (priority + IO_PRIORITY_GAME_BASE != IO_PRIORITY_NON_GAME) {
				ret = SCE_ERROR_ERRNO_EINVAL;
				SCE_DBG_ASSERT(info.priority == IO_PRIORITY_NON_GAME, 401);
				goto out;
			}
			SCE_DBG_ASSERT(info.priority == IO_PRIORITY_NON_GAME, 401);
			priority = info.priority;
		} else {
			if ((SceUInt32)priority > IO_PRIORITY_GAME_MAX)
				goto invalid;
			priority += IO_PRIORITY_GAME_BASE;
		}
	} else if ((SceUInt32)priority > IO_PRIORITY_MAX) {
		goto invalid;
	}
	info.priority = priority;
	ret = setFileSchedParam(fd, &info);
	goto out;

invalid:
	ret = SCE_ERROR_ERRNO_EINVAL;
out:
	sceSyscallLeave(syscall);
	return ret;
}

int SceIofilemgrForDriver_CCE94599(SceUID fd, SceInt32 priority)
{
	return sceIoSetPriorityInternal(fd, priority, 1);
}

SceInt32 sceIoGetPriorityInternal(SceUID fd, int fromKernel)
{
	IofilemgrSchedInfo info;
	SceUInt32 syscall;
	SceInt32 ret;

	syscall = sceSyscallEnter();
	ret = getFileSchedParam(fd, &info, 0);
	if (ret == 0) {
		if (ksceSblACMgrIsNonGameOrGameProgram2(ksceKernelGetProcessIdFromTLS()) == 1) {
			if (fromKernel == 1) {
				SCE_DBG_ASSERT(IO_PRIORITY_IS_SYSTEM(info.priority) ||
					       (SceUInt32)(info.priority - 6) <= 8, 452);
				ret = info.priority;
			} else {
				if (ksceSblACMgrIsNonGameProgram2(ksceKernelGetProcessIdFromTLS()) == 1)
					SCE_DBG_ASSERT(info.priority == IO_PRIORITY_NON_GAME, 472);
				else
					SCE_DBG_ASSERT((SceUInt32)(info.priority - IO_PRIORITY_GAME_BASE) <=
						       IO_PRIORITY_GAME_MAX, 475);
				ret = info.priority - IO_PRIORITY_GAME_BASE;
			}
		} else {
			SCE_DBG_ASSERT((SceUInt32)info.priority <= IO_PRIORITY_MAX, 482);
			ret = info.priority;
		}
	}
	sceSyscallLeave(syscall);
	return ret;
}

SceInt32 SceIofilemgrForDriver_9FCDCE62(SceUID fd)
{
	return sceIoGetPriorityInternal(fd, 1);
}

int setThreadIoPriority(SceInt32 priority)
{
	int ret;

	if (priority == IO_PRIORITY_UNSET || (SceUInt32)priority <= IO_PRIORITY_MAX) {
		getIofilemgrTls()->priority = priority;
		ret = 0;
	} else {
		ret = SCE_ERROR_ERRNO_EINVAL;
	}
	return ret;
}

SceInt32 swapThreadIoPriority(SceInt32 priority)
{
	SceInt32 old;

	old = getIofilemgrTls()->priority;
	if ((SceUInt32)priority > IO_PRIORITY_MAX)
		return SCE_ERROR_ERRNO_EINVAL;

	getIofilemgrTls()->priority = priority;
	return old;
}

SceInt32 getThreadIoPriority(void)
{
	IofilemgrKpls *kpls;
	IofilemgrTls *tls;
	SceInt32 ret;

	tls = getIofilemgrTls();
	if (tls != NULL && tls->priority != IO_PRIORITY_UNSET)
		return tls->priority;

	if (ksceSblACMgrIsRootProgram2(ksceKernelGetProcessIdFromTLS()) == 1 ||
	    ksceKernelGetProcessIdFromTLS() == SCE_KERNEL_DUMMY_PROCESS_GAME_ID ||
	    ksceKernelGetProcessIdFromTLS() == SCE_KERNEL_DUMMY_PROCESS_SYSTEM_ID)
		return g_ioDefaultPriority;

	ret = ksceKernelGetProcessLocalStorageAddrForPid(ksceKernelGetProcessIdFromTLS(),
							 g_iofilemgrKplsKey, (void **)&kpls, 1);
	if (ret < 0)
		return ret;
	return kpls->priority;
}

int sceIoSchedulerRemove(const char *file, int mntOpt)
{
	IoRequest *req;
	int ret;

	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL) {
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_REMOVE), 2, NULL,
				  SCE_ERROR_ERRNO_ENOMEM);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	ret = sceIoRequestCreateIoRequestForRemove(req, mntOpt, getThreadIoPriority(), 0, 0,
						   NULL, file);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_ENTER(VFS_VOP_HOOK_REMOVE), 3, req, mntOpt, file);
	if (ret >= 0) {
		ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
		if (ret >= 0)
			ret = req->args.path.result;
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_REMOVE), 2, req, ret);
	return ret;
}

/*
 * name unknown: the mount option of a path's device, which says how the
 * scheduler treats it. sdstor0: in safe or update mode is not scheduled (0);
 * full: reduce the path to its device or first absolute component.
 */
int getPathMntOpt(const char *path, int full)
{
	VfsPathParam param;
	char devName[0x20];
	const char *colon;
	char *dir;
	int len, ret;

	if (path == NULL)
		return SCE_ERROR_ERRNO_EFAULT;
	if (strnlen(path, 0x400) == 0x400)
		return SCE_ERROR_ERRNO_ENAMETOOLONG;

	colon = strchr(path, ':');
	if (colon != NULL) {
		len = colon - path;
		if (len > 0x1e)
			return 0;
		strncpy(devName, path, len + 1);
		devName[len + 1] = '\0';
		if (strncmp("sdstor0:", devName, 9) == 0) {
			if (SceSysrootForKernel_ksceSysrootIsSafeMode() == 1 ||
			    SceSysrootForKernel_ksceSysrootIsUpdateMode() == 1)
				return 0;
		} else if (full == 1) {
			param.path = devName;
			return ksceKernelRunWithStack(0x2000,
						      (int (*)(void *))vfs_get_vnode_mnt_opt,
						      &param);
		}
	} else if (full == 1) {
		if (path[0] != '/')
			return SCE_ERROR_ERRNO_EINVAL;
		colon = strchr(path + 1, '/');
		if (colon == NULL)
			return 0;
		dir = ksceKernelAllocHeapMemory(g_ioSchedHeap, 0x400);
		if (dir == NULL)
			return SCE_ERROR_ERRNO_ENOMEM;
		len = colon - path;
		strncpy(dir, path, len);
		param.path = dir;
		dir[len] = '\0';
		ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfs_get_vnode_mnt_opt, &param);
		ksceKernelFreeHeapMemory(g_ioSchedHeap, dir);
		return ret;
	}
	param.path = path;
	return ksceKernelRunWithStack(0x2000, (int (*)(void *))vfs_get_vnode_mnt_opt, &param);
}

int sceIoSchedulerMkdir(const char *dir, SceMode mode, int mntOpt)
{
	IoRequest *req;
	int ret;

	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL) {
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_MKDIR), 2, NULL,
				  SCE_ERROR_ERRNO_ENOMEM);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	ret = sceIoRequestCreateIoRequestForMkdir(req, mntOpt, getThreadIoPriority(), 0, 0,
						  NULL, dir, mode);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_ENTER(VFS_VOP_HOOK_MKDIR), 4, req, mode, mntOpt, dir);
	if (ret >= 0) {
		ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
		if (ret >= 0)
			ret = req->args.mkdir.result;
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_MKDIR), 2, req, ret);
	return ret;
}

int sceIoSchedulerRmdir(const char *path, int mntOpt)
{
	IoRequest *req;
	int ret;

	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL) {
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_RMDIR), 2, NULL,
				  SCE_ERROR_ERRNO_ENOMEM);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	ret = sceIoRequestCreateIoRequestForRmdir(req, mntOpt, getThreadIoPriority(), 0, 0,
						  NULL, path);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_ENTER(VFS_VOP_HOOK_RMDIR), 3, req, mntOpt, path);
	if (ret >= 0) {
		ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
		if (ret >= 0)
			ret = req->args.path.result;
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_RMDIR), 2, req, ret);
	return ret;
}

int sceIoSchedulerRename(const char *oldname, const char *newname, int mntOpt)
{
	IoRequest *req;
	int ret;

	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL) {
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_RENAME), 2, NULL,
				  SCE_ERROR_ERRNO_ENOMEM);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	ret = sceIoRequestCreateIoRequestForRename(req, mntOpt, getThreadIoPriority(), 0, 0,
						   NULL, oldname, newname);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_ENTER(VFS_VOP_HOOK_RENAME), 4, req, mntOpt, oldname,
			  newname);
	if (ret >= 0) {
		ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
		if (ret >= 0)
			ret = req->args.rename.result;
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_RENAME), 2, req, ret);
	return ret;
}

int sceIoSchedulerDevctl(const char *dev, unsigned int cmd, const void *indata, SceSize inlen,
			 void *outdata, SceSize outlen, int mntOpt)
{
	IoRequest *req;
	int ret;

	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = sceIoRequestCreateIoRequestForDevctl(req, mntOpt, getThreadIoPriority(), 0, 0,
						   NULL, dev, cmd, indata, inlen, outdata, outlen);
	if (ret >= 0) {
		ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
		if (ret >= 0)
			ret = req->args.devctl.result;
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	return ret;
}

int sceIoSchedulerSync(const char *device, unsigned int flag, int mntOpt)
{
	IoRequest *req;
	int ret;

	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL) {
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_SYNC), 2, NULL,
				  SCE_ERROR_ERRNO_ENOMEM);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	ret = sceIoRequestCreateIoRequestForSync(req, mntOpt, getThreadIoPriority(), 0, 0,
						 NULL, device, flag);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_ENTER(VFS_VOP_HOOK_SYNC), 4, req, flag, mntOpt, device);
	if (ret >= 0) {
		ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
		if (ret >= 0)
			ret = req->args.sync.result;
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_SYNC), 2, req, ret);
	return ret;
}

SceUID sceIoSchedulerOpen(const char *path, int flags, SceMode mode, int mntOpt)
{
	IoRequest *req;
	int ret;

	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL) {
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_OPEN), 2, NULL,
				  SCE_ERROR_ERRNO_ENOMEM);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	ret = sceIoRequestCreateIoRequestForOpen(req, mntOpt, getThreadIoPriority(), 0, 0,
						 NULL, path, flags, mode);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_ENTER(VFS_VOP_HOOK_OPEN), 5, req, flags, mode, mntOpt,
			  path);
	if (ret >= 0) {
		ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
		if (ret >= 0)
			ret = req->args.open.result;
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_OPEN), 2, req, ret);
	return ret;
}

int sceIoSchedulerClose(SceUID fd, const IofilemgrSchedInfo *info)
{
	IoRequest *req;
	int ret;

	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL) {
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_CLOSE), 2, NULL,
				  SCE_ERROR_ERRNO_ENOMEM);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	ret = sceIoRequestCreateIoRequestForClose(req, info->mntOpt, info->priority, 0, 0,
						  NULL, fd);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_ENTER(VFS_VOP_HOOK_CLOSE), 4, req, fd, info->priority,
			  info->mntOpt);
	if (ret >= 0) {
		ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
		if (ret >= 0)
			ret = req->args.fd.result;
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_CLOSE), 2, req, ret);
	return ret;
}

int sceIoSchedulerIoctl(SceUID fd, unsigned int cmd, const void *indata, SceSize inlen,
			void *outdata, SceSize outlen, const IofilemgrSchedInfo *info)
{
	IoRequest *req;
	int ret;

	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL) {
		if (g_vfsVopHook == NULL)
			return SCE_ERROR_ERRNO_ENOMEM;
		g_vfsVopHook(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_IOCTL), 2, NULL,
			     SCE_ERROR_ERRNO_ENOMEM);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	ret = sceIoRequestCreateIoRequestForIoctl(req, info->mntOpt, info->priority, 0, 0,
						  NULL, fd, cmd, indata, inlen, outdata, outlen);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_ENTER(VFS_VOP_HOOK_IOCTL), 8, req, fd, cmd, indata, inlen,
			  outdata, outlen, info->priority, info->mntOpt);
	if (ret >= 0) {
		ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
		if (ret >= 0)
			ret = req->args.ioctl.result;
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_IOCTL), 2, req, ret);
	return ret;
}

SceOff sceIoSchedulerLseek(SceUID fd, SceOff offset, int whence, const IofilemgrSchedInfo *info)
{
	IoRequest *req;
	SceOff ret;

	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL) {
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_LSEEK), 3, NULL, 0,
				  SCE_ERROR_ERRNO_ENOMEM);
		return (int)SCE_ERROR_ERRNO_ENOMEM;
	}
	ret = sceIoRequestCreateIoRequestForLseek(req, info->mntOpt, info->priority, 0, 0,
						  NULL, fd, offset, whence);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_ENTER(VFS_VOP_HOOK_LSEEK), 6, req, (int)(offset >> 32),
			  (int)offset, whence, info->priority, info->mntOpt);
	if (ret >= 0) {
		ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
		if (ret >= 0)
			ret = req->args.lseek.result;
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_LSEEK), 3, req, (int)(ret >> 32),
			  (int)ret);
	return ret;
}

int sceIoSchedulerLseek32(SceUID fd, int offset, int whence, const IofilemgrSchedInfo *info)
{
	IoRequest *req;
	int ret;

	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = sceIoRequestCreateIoRequestForLseek32(req, info->mntOpt, info->priority, 0, 0,
						    NULL, fd, offset, whence);
	if (ret >= 0) {
		ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
		if (ret >= 0)
			ret = req->args.lseek32.result;
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	return ret;
}

int sceIoSchedulerRead(SceUID fd, void *data, SceSize size, const IofilemgrSchedInfo *info)
{
	IoRequest *req;
	IoChunk *chunks;
	int ret;

	if (size != 0 && data == NULL) {
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_READ), 2, NULL,
				  SCE_ERROR_ERRNO_EFAULT);
		return SCE_ERROR_ERRNO_EFAULT;
	}
	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL) {
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_READ), 2, NULL,
				  SCE_ERROR_ERRNO_ENOMEM);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	chunks = ksceKernelAllocHeapMemory(g_ioSchedHeap, IO_SCHED_CHUNK_BUFF_SIZE);
	if (chunks == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
	} else {
		ret = sceIoRequestCreateIoRequestForRead(req, info->mntOpt, info->priority, 0, 0,
							 NULL, chunks, IO_REQUEST_CHUNK_NUM, fd,
							 data, size);
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_ENTER(VFS_VOP_HOOK_READ), 6, req, fd, data, size,
				  info->priority, info->mntOpt);
		if (ret >= 0) {
			ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
			if (ret >= 0)
				ret = req->args.rw.result;
		}
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_READ), 2, req, ret);
	return ret;
}

int sceIoSchedulerWrite(SceUID fd, const void *data, SceSize size,
			const IofilemgrSchedInfo *info)
{
	IoRequest *req;
	IoChunk *chunks;
	int ret;

	if (size != 0 && data == NULL) {
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_WRITE), 2, NULL,
				  SCE_ERROR_ERRNO_EFAULT);
		return SCE_ERROR_ERRNO_EFAULT;
	}
	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL) {
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_WRITE), 2, NULL,
				  SCE_ERROR_ERRNO_ENOMEM);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	chunks = ksceKernelAllocHeapMemory(g_ioSchedHeap, IO_SCHED_CHUNK_BUFF_SIZE);
	if (chunks == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
	} else {
		ret = sceIoRequestCreateIoRequestForWrite(req, info->mntOpt, info->priority, 0,
							  0, NULL, chunks, IO_REQUEST_CHUNK_NUM,
							  fd, data, size);
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_ENTER(VFS_VOP_HOOK_WRITE), 6, req, fd, data, size,
				  info->priority, info->mntOpt);
		if (ret >= 0) {
			ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
			if (ret >= 0)
				ret = req->args.write.result;
		}
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_WRITE), 2, req, ret);
	return ret;
}

int sceIoSchedulerPread(SceUID fd, void *data, SceSize size, SceOff offset,
			const IofilemgrSchedInfo *info)
{
	IoRequest *req;
	IoChunk *chunks;
	int ret;

	if (size != 0 && data == NULL) {
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_PREAD), 2, NULL,
				  SCE_ERROR_ERRNO_EFAULT);
		return SCE_ERROR_ERRNO_EFAULT;
	}
	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL) {
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_PREAD), 2, NULL,
				  SCE_ERROR_ERRNO_ENOMEM);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	chunks = ksceKernelAllocHeapMemory(g_ioSchedHeap, IO_SCHED_CHUNK_BUFF_SIZE);
	if (chunks == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
	} else {
		ret = sceIoRequestCreateIoRequestForPread(req, info->mntOpt, info->priority, 0,
							  0, NULL, chunks, IO_REQUEST_CHUNK_NUM,
							  fd, data, size, offset);
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_ENTER(VFS_VOP_HOOK_PREAD), 8, req, fd, data, size,
				  info->priority, info->mntOpt, (int)(offset >> 32), (int)offset);
		if (ret >= 0) {
			ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
			if (ret >= 0)
				ret = req->args.rw.result;
		}
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_PREAD), 2, req, ret);
	return ret;
}

int sceIoSchedulerPwrite(SceUID fd, const void *data, SceSize size, SceOff offset,
			 const IofilemgrSchedInfo *info)
{
	IoRequest *req;
	IoChunk *chunks;
	int ret;

	if (size != 0 && data == NULL) {
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_PWRITE), 2, NULL,
				  SCE_ERROR_ERRNO_EFAULT);
		return SCE_ERROR_ERRNO_EFAULT;
	}
	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL) {
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_PWRITE), 2, NULL,
				  SCE_ERROR_ERRNO_ENOMEM);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	chunks = ksceKernelAllocHeapMemory(g_ioSchedHeap, IO_SCHED_CHUNK_BUFF_SIZE);
	if (chunks == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
	} else {
		ret = sceIoRequestCreateIoRequestForPwrite(req, info->mntOpt, info->priority, 0,
							   0, NULL, chunks, IO_REQUEST_CHUNK_NUM,
							   fd, data, size, offset);
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_ENTER(VFS_VOP_HOOK_PWRITE), 8, req, fd, data, size,
				  info->priority, info->mntOpt, (int)(offset >> 32), (int)offset);
		if (ret >= 0) {
			ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
			if (ret >= 0)
				ret = req->args.rw.result;
		}
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_PWRITE), 2, req, ret);
	return ret;
}

int sceIoSchedulerSyncByFd(SceUID fd, int flag, const IofilemgrSchedInfo *info)
{
	IoRequest *req;
	int ret;

	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = sceIoRequestCreateIoRequestForSyncByFd(req, info->mntOpt, info->priority, 0, 0,
						     NULL, fd, flag);
	if (ret >= 0) {
		ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
		if (ret >= 0)
			ret = req->args.syncByFd.result;
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	return ret;
}

SceUID sceIoSchedulerDopen(const char *dirname, int mntOpt)
{
	IoRequest *req;
	int ret;

	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL) {
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_DOPEN), 2, NULL,
				  SCE_ERROR_ERRNO_ENOMEM);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	ret = sceIoRequestCreateIoRequestForDopen(req, mntOpt, getThreadIoPriority(), 0, 0,
						  NULL, dirname);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_ENTER(VFS_VOP_HOOK_DOPEN), 3, req, mntOpt, dirname);
	if (ret >= 0) {
		ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
		if (ret >= 0)
			ret = req->args.path.result;
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_DOPEN), 2, req, ret);
	return ret;
}

int sceIoSchedulerDclose(SceUID fd, const IofilemgrSchedInfo *info)
{
	IoRequest *req;
	int ret;

	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL) {
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_DCLOSE), 2, NULL,
				  SCE_ERROR_ERRNO_ENOMEM);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	ret = sceIoRequestCreateIoRequestForDclose(req, info->mntOpt, info->priority, 0, 0,
						   NULL, fd);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_ENTER(VFS_VOP_HOOK_DCLOSE), 4, req, fd, info->priority,
			  info->mntOpt);
	if (ret >= 0) {
		ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
		if (ret >= 0)
			ret = req->args.fd.result;
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_DCLOSE), 2, req, ret);
	return ret;
}

int sceIoSchedulerDread(SceUID fd, SceIoDirent *dir, const IofilemgrSchedInfo *info)
{
	IoRequest *req;
	int ret;

	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL) {
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_DREAD), 2, NULL,
				  SCE_ERROR_ERRNO_ENOMEM);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	ret = sceIoRequestCreateIoRequestForDread(req, info->mntOpt, info->priority, 0, 0,
						  NULL, fd, dir);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_ENTER(VFS_VOP_HOOK_DREAD), 5, req, fd, dir, info->priority,
			  info->mntOpt);
	if (ret >= 0) {
		ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
		if (ret >= 0)
			ret = req->args.dread.result;
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_DREAD), 2, req, ret);
	return ret;
}

/* name unknown: sceIoSchedulerDread for ksceIoDreadForVshbridge */
int sceIoSchedulerDreadForVsh(SceUID fd, SceIoDirent *dir, const IofilemgrSchedInfo *info)
{
	IoRequest *req;
	int ret;

	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = sceIoRequestCreateIoRequestForDreadForVsh(req, info->mntOpt, info->priority, 0,
							0, NULL, fd, dir);
	if (ret >= 0) {
		ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
		if (ret >= 0)
			ret = req->args.dread.result;
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	return ret;
}

int sceIoSchedulerChstat(const char *file, SceIoStat *stat, int bits, int mntOpt)
{
	IoRequest *req;
	int ret;

	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL) {
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_CHSTAT), 2, NULL,
				  SCE_ERROR_ERRNO_ENOMEM);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	ret = sceIoRequestCreateIoRequestForChstat(req, mntOpt, getThreadIoPriority(), 0, 0,
						   NULL, file, stat, bits);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_ENTER(VFS_VOP_HOOK_CHSTAT), 5, req, stat, bits, mntOpt,
			  file);
	if (ret >= 0) {
		ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
		if (ret >= 0)
			ret = req->args.chstat.result;
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_CHSTAT), 2, req, ret);
	return ret;
}

int sceIoSchedulerGetstat(const char *file, SceIoStat *stat, int mntOpt)
{
	IoRequest *req;
	int ret;

	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL) {
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_GETSTAT), 2, NULL,
				  SCE_ERROR_ERRNO_ENOMEM);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	ret = sceIoRequestCreateIoRequestForGetstat(req, mntOpt, getThreadIoPriority(), 0, 0,
						    NULL, file, stat);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_ENTER(VFS_VOP_HOOK_GETSTAT), 4, req, stat, mntOpt, file);
	if (ret >= 0) {
		ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
		if (ret >= 0)
			ret = req->args.getstat.result;
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_GETSTAT), 2, req, ret);
	return ret;
}

int sceIoSchedulerGetstatByFd(SceUID fd, SceIoStat *stat, const IofilemgrSchedInfo *info)
{
	IoRequest *req;
	int ret;

	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL) {
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_FGETSTAT), 2, NULL,
				  SCE_ERROR_ERRNO_ENOMEM);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	ret = sceIoRequestCreateIoRequestForGetstatByFd(req, info->mntOpt, info->priority, 0,
							0, NULL, fd, stat);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_ENTER(VFS_VOP_HOOK_FGETSTAT), 5, req, fd, stat,
			  info->priority, info->mntOpt);
	if (ret >= 0) {
		ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
		if (ret >= 0)
			ret = req->args.getstatByFd.result;
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_FGETSTAT), 2, req, ret);
	return ret;
}

int sceIoSchedulerChstatByFd(SceUID fd, SceIoStat *stat, int bits,
			     const IofilemgrSchedInfo *info)
{
	IoRequest *req;
	int ret;

	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL) {
		VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_FCHSTAT), 2, NULL,
				  SCE_ERROR_ERRNO_ENOMEM);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	ret = sceIoRequestCreateIoRequestForChstatByFd(req, info->mntOpt, info->priority, 0,
						       0, NULL, fd, stat, bits);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_ENTER(VFS_VOP_HOOK_FCHSTAT), 6, req, fd, stat, bits,
			  info->priority, info->mntOpt);
	if (ret >= 0) {
		ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
		if (ret >= 0)
			ret = req->args.chstatByFd.result;
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	VFS_VOP_HOOK_CALL(IO_SCHED_HOOK_EXIT(VFS_VOP_HOOK_FCHSTAT), 2, req, ret);
	return ret;
}

/* name unknown: sceIoSchedulerChstat for ksceIoChstatForVshbridge */
int sceIoSchedulerChstatForVsh(const char *file, SceIoStat *stat, int bits, int mntOpt)
{
	IoRequest *req;
	int ret;

	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = sceIoRequestCreateIoRequestForChstatForVsh(req, mntOpt, getThreadIoPriority(), 0,
							 0, NULL, file, stat, bits);
	if (ret >= 0) {
		ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
		if (ret >= 0)
			ret = req->args.chstat.result;
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	return ret;
}

/* name unknown: sceIoSchedulerGetstat for ksceIoGetstatForVshbridge */
int sceIoSchedulerGetstatForVsh(const char *file, SceIoStat *stat, int mntOpt)
{
	IoRequest *req;
	int ret;

	req = ksceKernelAllocHeapMemory(g_ioSchedHeap, sizeof(*req));
	if (req == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = sceIoRequestCreateIoRequestForGetstatForVsh(req, mntOpt, getThreadIoPriority(), 0,
							  0, NULL, file, stat);
	if (ret >= 0) {
		ret = sceIoSchedDispatcherDispatchIoRequest(&g_schedDispatcher, req);
		if (ret >= 0)
			ret = req->args.getstat.result;
	}
	ksceKernelFreeHeapMemory(g_ioSchedHeap, req);
	return ret;
}

int sceIoSchedulerInitialize(void)
{
	SceKernelHeapCreateOpt opt;

	opt.size = sizeof(opt);
	opt.attr = 1;
	g_ioSchedHeap = ksceKernelCreateHeap("SceIoScheduler", 0x10000, &opt);
	return g_ioSchedHeap < 0 ? g_ioSchedHeap : 0;
}

int SceIofilemgrForDriver_C722DF35(SceUInt32 weight)
{
	return sceIoSchedDispatcherFsResourceAvailable(&g_schedDispatcher, weight);
}

int SceIofilemgrForDriver_3675ECB9(SceUInt32 weight)
{
	return sceIoSchedDispatcherFsResourceUnavailable(&g_schedDispatcher, weight);
}

int SceIofilemgrForDriver_15C17487(SceInt32 weight)
{
	return sceIoSchedDispatcherIssueAnotherIoRequest(&g_schedDispatcher, weight);
}

/* the request the calling thread runs for the scheduler */
IoRequest *SceIofilemgrForDriver_12F8D58E(void)
{
	return getIofilemgrTls()->request;
}

int setFileSchedParam(SceUID fd, const IofilemgrSchedInfo *info)
{
	IofilemgrIoctlParam param;

	param.fd = fd;
	param.cmd = VFS_IOCTL_SET_IO_SCHED_DATA;
	param.indata = NULL;
	param.inlen = 0;
	param.outdata = (void *)info;
	param.outlen = sizeof(*info);
	return vfsIoctl(&param);
}

int getFileSchedParam(SceUID fd, IofilemgrSchedInfo *info, int flag)
{
	IofilemgrIoctlParam param;

	param.fd = fd;
	param.cmd = VFS_IOCTL_GET_IO_SCHED_DATA;
	param.indata = &flag;
	param.inlen = sizeof(flag);
	param.outdata = info;
	param.outlen = sizeof(*info);
	return vfsIoctl(&param);
}
