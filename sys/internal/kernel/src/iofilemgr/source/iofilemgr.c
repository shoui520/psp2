/*
 * SceIofilemgr: module entry, process events and the driver I/O entry points
 * (Sony's iofilemgr.c).
 *
 * Functions follow the original's address order.
 */
#include <sce/syscall_copy.h>
#include "iofilemgr_internal.h"

int g_iofilemgrTlsKey = -1;
SceUID g_iofilemgrKplsKey = -1;
SceInt32 g_gameProgramCount;
SceInt32 g_iofilemgrPid[IOFILEMGR_PROCESS_MAX];
SceKernelFastMutex g_iofilemgrMutex;
int g_iofilemgrResourceLimitSet;
IoPathMappingFunction g_ioPathMappingFunction;
VfsVopHook g_vfsVopHook __attribute__((section(".data"))) = NULL;
SceUID g_ioVfsHeapUid __attribute__((section(".data"))) = 0;

/* name unknown: init callback (level 5) registered by init_iofm */
__attribute__((section(".iofilemgrEntry.iofilemgrInitDummyTty")))
int iofilemgrInitDummyTty(int unused0, void *unused1)
{
	DummyTtyStart();
	return 0;
}

__attribute__((section(".iofilemgrEntry.IofilemgrModuleProcResume")))
int IofilemgrModuleProcResume(SceUID pid, int phase, SceProcEventInvokeParam1 *param, int unused)
{
	IofilemgrKpls *kpls;
	int ret;

	switch (phase) {
	case SCE_PROC_EVENT_PHASE_RESUME:
		if (ksceSblACMgrIsGameProgram2(pid) == 1)
			ksceKernelAtomicAddAndGet32(&g_gameProgramCount, 1);
		ret = ksceKernelGetProcessLocalStorageAddrForPid(pid, g_iofilemgrKplsKey,
								 (void **)&kpls, 1);
		if (ret < 0)
			return ret;
		kpls->phase = IOFILEMGR_KPLS_PHASE_RESUMED;
		break;
	default:
		break;
	}
	SCE_DBG_PRINTF_LEVEL(2, 0xf, 1815, "Resume phase=0x%x\n", phase);
	return 0;
}

__attribute__((section(".iofilemgrEntry.IofilemgrModuleProcSuspend")))
int IofilemgrModuleProcSuspend(SceUID pid, int phase, SceProcEventInvokeParam1 *param, int unused)
{
	IofilemgrKpls *kpls;
	int ret, num;

	switch (phase) {
	case SCE_PROC_EVENT_PHASE_SUSPEND:
		if (ksceSblACMgrIsGameProgram2(pid) == 1)
			ksceKernelAtomicSubAndGet32(&g_gameProgramCount, 1);
		ret = ksceKernelGetProcessLocalStorageAddrForPid(pid, g_iofilemgrKplsKey,
								 (void **)&kpls, 1);
		if (ret < 0)
			return ret;
		if (kpls->phase != IOFILEMGR_KPLS_PHASE_SUSPENDED) {
			num = SceSysmemForKernel_08C05493(pid, &g_vfsFileClass, 7, NULL, 0, NULL);
			if (num > 0)
				vfs_process_suspend(pid, num);
		}
		kpls->phase = IOFILEMGR_KPLS_PHASE_SUSPENDED;
		return 0;
	default:
		break;
	}
	SCE_DBG_PRINTF_LEVEL(2, 0xf, 1783, "Suspend phase=0x%x\n", phase);
	return 0;
}

static inline __attribute__((always_inline)) int iofilemgrProcessFinalize(SceUID pid)
{
	IofilemgrKpls *kpls;
	SceUID oldPid;
	int ret, i;

	ret = ksceKernelGetProcessLocalStorageAddrForPid(pid, g_iofilemgrKplsKey, (void **)&kpls, 1);
	if (ret < 0)
		return ret;
	oldPid = ksceKernelSetProcessIdToTLS(pid);
	kpls->phase = IOFILEMGR_KPLS_PHASE_FINALIZED;
	ksceKernelSetProcessIdToTLS(oldPid);
	sceIoAsyncProcessFinalize(pid);
	vfs_process_finalize(pid);
	ioerror_process_finalize(pid);
	mount_event_process_finalize(pid);
	for (i = 0; i < IOFILEMGR_PROCESS_MAX; i++) {
		if (g_iofilemgrPid[i] == pid) {
			g_iofilemgrPid[i] = -1;
			break;
		}
	}
	return 0;
}

/* name unknown: the process event handler's exit */
__attribute__((section(".iofilemgrEntry.IofilemgrModuleProcExit")))
int IofilemgrModuleProcExit(SceUID pid, SceProcEventInvokeParam1 *param, int unused)
{
	return iofilemgrProcessFinalize(pid);
}

/* name unknown: the process event handler's kill */
__attribute__((section(".iofilemgrEntry.IofilemgrModuleProcKill")))
int IofilemgrModuleProcKill(SceUID pid, SceProcEventInvokeParam1 *param, int unused)
{
	return iofilemgrProcessFinalize(pid);
}

__attribute__((section(".iofilemgrEntry.IofilemgrModuleProcCreate")))
int IofilemgrModuleProcCreate(SceUID pid, SceProcEventInvokeParam2 *param, int unused)
{
	SceKernelProcessResourceLimit limit;
	IofilemgrKpls *kpls;
	SceUInt64 paid;
	SceUID key;
	int ret, i;

	key = g_iofilemgrKplsKey;
	if (key == -1) {
		ret = ksceKernelCreateProcessLocalStorage("SceIofilemgr", sizeof(IofilemgrKpls));
		g_iofilemgrKplsKey = ret;
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xb, 1551, "sceKernelRegisterKPLS() failed err=%d 0x%x\n",
					     ret, ret);
			return -1;
		}
		key = ret;
	}
	ret = ksceKernelGetProcessLocalStorageAddrForPid(pid, key, (void **)&kpls, 1);
	if (ret < 0)
		return ret;
	memset(kpls, 0, sizeof(*kpls));
	kpls->fdMax = 0x100;
	kpls->dialogFdMax = 0x14;
	kpls->phase = IOFILEMGR_KPLS_PHASE_RESUMED;
	kpls->depth = 0x10;
	for (i = 0; i < IOFILEMGR_PROCESS_MAX; i++) {
		if (g_iofilemgrPid[i] == -1 &&
		    ksceKernelAtomicCompareAndSet32(&g_iofilemgrPid[i], -1, pid) == -1)
			break;
	}
	if (i != IOFILEMGR_PROCESS_MAX)
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 1573, "Create PID = 0x%x\n", g_iofilemgrPid[i]);
	if (ksceSblACMgrIsRootProgram2(pid) == 1 || ksceSblACIsSystemProgram(pid) == 1) {
		kpls->priority = 0xe;
	} else if (ksceSblACMgrIsNonGameProgram2(pid) == 1) {
		kpls->priority = 0xb;
	} else {
		kpls->priority = 8;
		if (ksceSblACMgrIsGameProgram2(pid) == 1)
			ksceKernelAtomicAddAndGet32(&g_gameProgramCount, 1);
	}
	limit.size = sizeof(limit);
	limit.limit0 = 0;
	limit.limit1 = 0;
	limit.budgetType = 0;
	ret = sceKernelGetProcessResourceLimit(pid, &limit);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 1598, "failed ... %x\n", ret);
	else
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 1602, "size=%x num=%d depth=%d\n",
				     limit.size, limit.limit0, limit.limit1);
	if (limit.budgetType == 0x2000000) {
		if (g_iofilemgrResourceLimitSet == 0) {
			ksceKernelLockFastMutex(&g_iofilemgrMutex);
			if (g_iofilemgrResourceLimitSet == 0) {
				sceKernelObjectHeapSetResourceLimit(pid, 0, 3, 0x170, 0x170);
				sceKernelObjectHeapSetResourceLimit(pid, 0, 4, 0x5c, 0x5c);
				g_iofilemgrResourceLimitSet = 1;
			}
			ksceKernelUnlockFastMutex(&g_iofilemgrMutex);
		}
	} else {
		sceKernelObjectHeapSetResourceLimit(pid, 0, 3, limit.limit0 * 4, limit.limit0 * 4);
		sceKernelObjectHeapSetResourceLimit(pid, 0, 4, limit.limit0, limit.limit0);
	}
	kpls->fdMax = limit.limit0;
	paid = 0;
	ret = ksceSblACMgrGetProcessProgramAuthId2(pid, &paid);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 6, 1658, "sceSblACMgrGetPaid() failed. %x\n", ret);
	if (paid == 0x2800000000007001ULL)
		kpls->depth = 0x10;
	else
		kpls->depth = limit.limit1;
	return 0;
}

/* name unknown: registered with SceSysrootForKernel_CBD377B7 by init_iofm */
int iofilemgrStartMount(void)
{
	int ret;

	ret = mnt_start();
	if (ret < 0)
		return ret;
	ret = sceIoSchedDispatcherInitializeSchedDispatcher(&g_schedDispatcher);
	iofile_daemon_start();
	return ret;
}

void SceIofilemgrForDriver_F4F8C59A(void *arg)
{
	g_vfsVopHook = (VfsVopHook)arg;
}

int ksceIoClose(SceUID fd)
{
	IofilemgrSchedInfo info __attribute__((aligned(8)));
	IofilemgrCloseParam param;
	SceUInt32 tls;
	int ret;

	tls = sceSyscallEnter();
	if (getFileSchedParam(fd, &info, 1) != 0) {
		info.priority = 8;
		info.mntOpt = 0;
		info.pid = ksceKernelGetProcessIdFromTLS();
	}
	if (g_ioSchedEnabled == 1 && info.mntOpt >= 0) {
		ret = sceIoSchedulerClose(fd, &info);
	} else {
		param.fd = fd;
		vfs_increment_io_count();
		vfs_set_sync_pending();
		ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsClose, &param);
		vfs_decrement_io_count();
	}
	sceSyscallLeave(tls);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 257, "sceIoClose",
					  "call org ret = %d 0x%08x\n", ret, ret);
	return ret;
}

SceUID ksceIoOpen(const char *path, int flags, SceMode mode)
{
	IofilemgrOpenParam param __attribute__((aligned(8)));
	SceUInt32 tls;
	int ret;

	tls = sceSyscallEnter();
	ret = getPathMntOpt(path, 1);
	if (ret >= 0) {
		if (g_ioSchedEnabled != 1) {
			param.path = path;
			param.flags = flags;
			param.mode = mode;
			vfs_increment_io_count();
			vfs_set_sync_pending();
			ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsOpen, &param);
			vfs_decrement_io_count();
		} else {
			ret = sceIoSchedulerOpen(path, flags, mode, ret);
		}
	}
	sceSyscallLeave(tls);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 308, "sceIoOpen",
					  "call org ret = %d 0x%08x\n", ret, ret);
	return ret;
}

int ksceIoRead(SceUID fd, void *data, SceSize size)
{
	IofilemgrSchedInfo info;
	SceUInt32 tls;
	int ret;

	tls = sceSyscallEnter();
	ret = getFileSchedParam(fd, &info, 0);
	if (g_ioSchedEnabled == 1 && ret >= 0 && info.mntOpt >= 0 &&
	    (info.mntOpt & 0xffff) != 0x400) {
		ret = sceIoSchedulerRead(fd, data, size, &info);
	} else {
		IofilemgrReadParam param;

		param.fd = fd;
		param.data = data;
		param.size = size;
		vfs_increment_io_count();
		vfs_set_sync_pending();
		ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsRead, &param);
		vfs_decrement_io_count();
	}
	sceSyscallLeave(tls);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 360, "sceIoRead",
					  "call org ret = %d 0x%08x\n", ret, ret);
	return ret;
}

int ksceIoWrite(SceUID fd, const void *data, SceSize size)
{
	IofilemgrSchedInfo info;
	SceUInt32 tls;
	int ret;

	tls = sceSyscallEnter();
	ret = getFileSchedParam(fd, &info, 0);
	if (g_ioSchedEnabled == 1 && ret >= 0 && info.mntOpt >= 0) {
		ret = sceIoSchedulerWrite(fd, data, size, &info);
	} else {
		IofilemgrWriteParam param;

		param.fd = fd;
		param.data = data;
		param.size = size;
		vfs_increment_io_count();
		vfs_set_sync_pending();
		ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsWrite, &param);
		vfs_decrement_io_count();
	}
	sceSyscallLeave(tls);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 416, "sceIoWrite",
					  "call org ret = %d 0x%08x\n", ret, ret);
	return ret;
}

SceOff ksceIoLseek(SceUID fd, SceOff offset, int whence)
{
	IofilemgrLseekParam param;
	IofilemgrSchedInfo info;
	SceUInt32 tls;
	SceOff ret;
	int err;

	tls = sceSyscallEnter();
	err = getFileSchedParam(fd, &info, 0);
	if (g_ioSchedEnabled == 1 && err >= 0 && info.mntOpt >= 0) {
		ret = sceIoSchedulerLseek(fd, offset, whence, &info);
	} else {
		param.fd = fd;
		param.whence = whence;
		param.offset = offset;
		vfs_increment_io_count();
		vfs_set_sync_pending();
		ret = sceKernelExtendKernelStackWide(0x2000, (SceInt64 (*)(void *))vfsLseek, &param);
		vfs_decrement_io_count();
	}
	sceSyscallLeave(tls);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 464, "sceIoLseek",
					  "call org ret = %d 0x%08x\n", (int)ret, (int)ret);
	return ret;
}

/* sceIoLseek32ForUser is the SceIofilemgr export */
int sceIoLseek32(SceUID fd, int offset, int whence)
{
	IofilemgrLseekParam param;
	IofilemgrSchedInfo info;
	SceUInt32 tls;
	SceOff ret;
	int err;

	tls = sceSyscallEnter();
	err = getFileSchedParam(fd, &info, 0);
	if (g_ioSchedEnabled == 1 && err >= 0 && info.mntOpt >= 0) {
		ret = sceIoSchedulerLseek32(fd, offset, whence, &info);
	} else {
		param.fd = fd;
		param.whence = whence;
		param.offset = offset;
		vfs_increment_io_count();
		vfs_set_sync_pending();
		ret = sceKernelExtendKernelStackWide(0x2000, (SceInt64 (*)(void *))vfsLseek, &param);
		vfs_decrement_io_count();
	}
	sceSyscallLeave(tls);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 508, "call org ret = %d 0x%08x\n", (int)ret, (int)ret);
	return ret;
}

int ksceIoIoctl(SceUID fd, unsigned int cmd, const void *indata, SceSize inlen, void *outdata,
		SceSize outlen)
{
	IofilemgrIoctlParam param;
	IofilemgrSchedInfo info;
	SceUID impersonateId;
	SceUInt32 tls;
	int ret;

	impersonateId = ksceKernelGetProcessIdFromTLS();
	tls = sceSyscallEnter();
	ret = getFileSchedParam(fd, &info, 0);
	if (g_ioSchedEnabled == 1 && ret >= 0 && info.mntOpt >= 0) {
		if (cmd & IOFILEMGR_IOCTL_ACCESS_CHECK) {
			if (cmd & IOFILEMGR_IOCTL_ACCESS_SYSTEM) {
				if (!ksceSblACIsSystemProgram(impersonateId) &&
				    !ksceSblACMgrIsRootProgram2(impersonateId)) {
					SCE_DBG_PRINTF_FUNC(0xf, 538, "sceIoIoctl",
							    "error : sceIoIoctl() failed\n");
					SCE_DBG_PRINTF_FUNC(0xf, 539, "sceIoIoctl",
							    "impersonateId = 0x%08x\n", impersonateId);
					SCE_DBG_PRINTF_FUNC(0xf, 541, "sceIoIoctl",
							    "sceSblACMgrIsSystemProgram() = %d\n",
							    ksceSblACIsSystemProgram(impersonateId));
					SCE_DBG_PRINTF_FUNC(0xf, 543, "sceIoIoctl",
							    "sceSblACMgrIsKernelProgram() = %d\n",
							    ksceSblACMgrIsRootProgram2(impersonateId));
					SCE_DBG_PRINTF_FUNC(0xf, 544, "sceIoIoctl",
							    "cmd=%x\n", cmd);
					sceSyscallLeave(tls);
					return SCE_ERROR_ERRNO_EUNSUP;
				}
			} else if (!ksceSblACIsSystemProgram(impersonateId) ||
				   ksceSblACMgrIsRootProgram2(impersonateId)) {
				SCE_DBG_PRINTF_FUNC(0xf, 551, "sceIoIoctl",
						    "error : sceIoIoctl() failed!\n");
				SCE_DBG_PRINTF_FUNC(0xf, 552, "sceIoIoctl",
						    "impersonateId = 0x%08x\n", impersonateId);
				SCE_DBG_PRINTF_FUNC(0xf, 554, "sceIoIoctl",
						    "sceSblACMgrIsSystemProgram() = %d\n",
						    ksceSblACIsSystemProgram(impersonateId));
				SCE_DBG_PRINTF_FUNC(0xf, 556, "sceIoIoctl",
						    "sceSblACMgrIsKernelProgram() = %d\n",
						    ksceSblACMgrIsRootProgram2(impersonateId));
				SCE_DBG_PRINTF_FUNC(0xf, 557, "sceIoIoctl", "cmd=%x\n", cmd);
				sceSyscallLeave(tls);
				return SCE_ERROR_ERRNO_EUNSUP;
			}
		}
		if (cmd & IOFILEMGR_IOCTL_DISABLED) {
			SCE_DBG_PRINTF_FUNC(0xf, 564, "sceIoIoctl",
					    "error : sceIoIoctl() failed!\n");
			SCE_DBG_PRINTF_FUNC(0xf, 565, "sceIoIoctl",
					    "impersonateId = 0x%08x\n", impersonateId);
			SCE_DBG_PRINTF_FUNC(0xf, 566, "sceIoIoctl", "cmd=%x\n", cmd);
			sceSyscallLeave(tls);
			return SCE_ERROR_ERRNO_EUNSUP;
		}
		ret = sceIoSchedulerIoctl(fd, cmd, indata, inlen, outdata, outlen, &info);
	} else {
		if (cmd & IOFILEMGR_IOCTL_ACCESS_CHECK) {
			if (cmd & IOFILEMGR_IOCTL_ACCESS_SYSTEM) {
				if (!ksceSblACIsSystemProgram(impersonateId) &&
				    !ksceSblACMgrIsRootProgram2(impersonateId)) {
					SCE_DBG_PRINTF_FUNC(0xf, 577, "sceIoIoctl",
							    "error : sceIoIoctl() failed\n");
					SCE_DBG_PRINTF_FUNC(0xf, 578, "sceIoIoctl",
							    "impersonateId = 0x%08x\n", impersonateId);
					SCE_DBG_PRINTF_FUNC(0xf, 580, "sceIoIoctl",
							    "sceSblACMgrIsSystemProgram() = %d\n",
							    ksceSblACIsSystemProgram(impersonateId));
					SCE_DBG_PRINTF_FUNC(0xf, 582, "sceIoIoctl",
							    "sceSblACMgrIsKernelProgram() = %d\n",
							    ksceSblACMgrIsRootProgram2(impersonateId));
					SCE_DBG_PRINTF_FUNC(0xf, 583, "sceIoIoctl",
							    "cmd=%x\n", cmd);
					sceSyscallLeave(tls);
					return SCE_ERROR_ERRNO_EUNSUP;
				}
			} else if (!ksceSblACIsSystemProgram(impersonateId) ||
				   ksceSblACMgrIsRootProgram2(impersonateId)) {
				SCE_DBG_PRINTF_FUNC(0xf, 590, "sceIoIoctl",
						    "error : sceIoIoctl() failed!\n");
				SCE_DBG_PRINTF_FUNC(0xf, 591, "sceIoIoctl",
						    "impersonateId = 0x%08x\n", impersonateId);
				SCE_DBG_PRINTF_FUNC(0xf, 593, "sceIoIoctl",
						    "sceSblACMgrIsSystemProgram() = %d\n",
						    ksceSblACIsSystemProgram(impersonateId));
				SCE_DBG_PRINTF_FUNC(0xf, 595, "sceIoIoctl",
						    "sceSblACMgrIsKernelProgram() = %d\n",
						    ksceSblACMgrIsRootProgram2(impersonateId));
				SCE_DBG_PRINTF_FUNC(0xf, 596, "sceIoIoctl", "cmd=%x\n", cmd);
				sceSyscallLeave(tls);
				return SCE_ERROR_ERRNO_EUNSUP;
			}
		}
		if (cmd & IOFILEMGR_IOCTL_DISABLED) {
			SCE_DBG_PRINTF_FUNC(0xf, 603, "sceIoIoctl",
					    "error : sceIoIoctl() failed!\n");
			SCE_DBG_PRINTF_FUNC(0xf, 604, "sceIoIoctl",
					    "impersonateId = 0x%08x\n", impersonateId);
			SCE_DBG_PRINTF_FUNC(0xf, 605, "sceIoIoctl", "cmd=%x\n", cmd);
			sceSyscallLeave(tls);
			return SCE_ERROR_ERRNO_EUNSUP;
		}
		param.fd = fd;
		param.cmd = cmd;
		param.indata = indata;
		param.inlen = inlen;
		param.outdata = outdata;
		param.outlen = outlen;
		vfs_increment_io_count();
		vfs_set_sync_pending();
		ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsIoctl, &param);
		vfs_decrement_io_count();
	}
	sceSyscallLeave(tls);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 631, "sceIoIoctl",
					  "call org ret = %d 0x%08x\n", ret, ret);
	return ret;
}

SceUID ksceIoDopen(const char *dirname)
{
	IofilemgrDopenParam param;
	SceUInt32 tls;
	int ret;

	tls = sceSyscallEnter();
	ret = getPathMntOpt(dirname, 1);
	if (ret >= 0) {
		if (g_ioSchedEnabled != 1) {
			param.dirname = dirname;
			vfs_increment_io_count();
			vfs_set_sync_pending();
			ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsDopen, &param);
			vfs_decrement_io_count();
		} else {
			ret = sceIoSchedulerDopen(dirname, ret);
		}
	}
	sceSyscallLeave(tls);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 681, "sceIoDopen",
					  "call org ret = %d 0x%08x\n", ret, ret);
	return ret;
}

int ksceIoDread(SceUID fd, SceIoDirent *dir)
{
	IofilemgrSchedInfo info;
	SceUInt32 tls;
	int ret;

	tls = sceSyscallEnter();
	ret = getFileSchedParam(fd, &info, 0);
	if (g_ioSchedEnabled == 1 && ret >= 0 && info.mntOpt >= 0) {
		ret = sceIoSchedulerDread(fd, dir, &info);
	} else {
		IofilemgrDreadParam param;

		param.fd = fd;
		param.dir = dir;
		param.preserveAttributes = 0;
		vfs_increment_io_count();
		vfs_set_sync_pending();
		ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsDread, &param);
		vfs_decrement_io_count();
	}
	sceSyscallLeave(tls);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 730, "sceIoDread",
					  "call org ret = %d 0x%08x\n", ret, ret);
	return ret;
}

int ksceIoDclose(SceUID fd)
{
	SceUInt32 tls;
	IofilemgrDcloseParam param;
	IofilemgrSchedInfo info;
	int ret;

	tls = sceSyscallEnter();
	ret = getFileSchedParam(fd, &info, 1);
	if (g_ioSchedEnabled == 1 && ret >= 0 && info.mntOpt >= 0) {
		ret = sceIoSchedulerDclose(fd, &info);
	} else {
		param.reserved = 0;
		param.fd = fd;
		vfs_increment_io_count();
		vfs_set_sync_pending();
		ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsDclose, &param);
		vfs_decrement_io_count();
	}
	sceSyscallLeave(tls);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 776, "sceIoDclose",
					  "call org ret = %d 0x%08x\n", ret, ret);
	return ret;
}

int ksceIoRemove(const char *file)
{
	IofilemgrRemoveParam param;
	SceUInt32 tls;
	int ret;

	tls = sceSyscallEnter();
	ret = getPathMntOpt(file, 1);
	if (ret >= 0) {
		if (g_ioSchedEnabled != 1) {
			param.file = file;
			param.flags = 0;
			vfs_increment_io_count();
			vfs_set_sync_pending();
			ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsRemove, &param);
			vfs_decrement_io_count();
		} else {
			ret = sceIoSchedulerRemove(file, ret);
		}
	}
	sceSyscallLeave(tls);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 828, "sceIoRemove",
					  "call org ret = %d 0x%08x\n", ret, ret);
	return ret;
}

int ksceIoMkdir(const char *dir, SceMode mode)
{
	IofilemgrMkdirParam param;
	SceUInt32 tls;
	int ret;

	tls = sceSyscallEnter();
	ret = getPathMntOpt(dir, 1);
	if (ret >= 0) {
		if (g_ioSchedEnabled != 1) {
			param.dir = dir;
			param.mode = mode;
			vfs_increment_io_count();
			vfs_set_sync_pending();
			ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsMkdir, &param);
			vfs_decrement_io_count();
		} else {
			ret = sceIoSchedulerMkdir(dir, mode, ret);
		}
	}
	sceSyscallLeave(tls);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 880, "sceIoMkdir",
					  "call org ret = %d 0x%08x\n", ret, ret);
	return ret;
}

int ksceIoRmdir(const char *path)
{
	IofilemgrRmdirParam param;
	SceUInt32 tls;
	int ret;

	tls = sceSyscallEnter();
	ret = getPathMntOpt(path, 1);
	if (ret >= 0) {
		if (g_ioSchedEnabled != 1) {
			param.path = path;
			vfs_increment_io_count();
			vfs_set_sync_pending();
			ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsRmdir, &param);
			vfs_decrement_io_count();
		} else {
			ret = sceIoSchedulerRmdir(path, ret);
		}
	}
	sceSyscallLeave(tls);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 930, "sceIoRmdir",
					  "call org ret = %d 0x%08x\n", ret, ret);
	return ret;
}

int ksceIoGetstat(const char *file, SceIoStat *stat)
{
	IofilemgrGetstatParam param;
	SceUInt32 tls;
	int ret;

	tls = sceSyscallEnter();
	ret = getPathMntOpt(file, 1);
	if (ret >= 0) {
		if (g_ioSchedEnabled != 1) {
			param.file = file;
			param.stat = stat;
			param.preserveAttributes = 0;
			vfs_increment_io_count();
			vfs_set_sync_pending();
			ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsGetstat, &param);
			vfs_decrement_io_count();
		} else {
			ret = sceIoSchedulerGetstat(file, stat, ret);
		}
	}
	sceSyscallLeave(tls);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 982, "sceIoGetstat",
					  "call org ret = %d 0x%08x\n", ret, ret);
	return ret;
}

int ksceIoChstat(const char *file, SceIoStat *stat, int bits)
{
	IofilemgrChstatParam param;
	SceUInt32 tls;
	int ret;

	tls = sceSyscallEnter();
	ret = getPathMntOpt(file, ((bits ^ SCE_CST_SIZE) >> 2) & 1);
	if (ret >= 0) {
		if (g_ioSchedEnabled != 1) {
			param.file = file;
			param.stat = stat;
			param.bits = bits;
			param.forVshbridge = 0;
			vfs_increment_io_count();
			vfs_set_sync_pending();
			ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsChstat, &param);
			vfs_decrement_io_count();
		} else {
			ret = sceIoSchedulerChstat(file, stat, bits, ret);
		}
	}
	sceSyscallLeave(tls);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 1037, "sceIoChstat",
					  "call org ret = %d 0x%08x\n", ret, ret);
	return ret;
}

int ksceIoRename(const char *oldname, const char *newname)
{
	IofilemgrRenameParam param;
	SceUInt32 tls;
	int ret;

	tls = sceSyscallEnter();
	ret = getPathMntOpt(oldname, 1);
	if (ret >= 0) {
		if (g_ioSchedEnabled != 1) {
			param.oldname = oldname;
			param.newname = newname;
			vfs_increment_io_count();
			vfs_set_sync_pending();
			ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsRename, &param);
			vfs_decrement_io_count();
		} else {
			ret = sceIoSchedulerRename(oldname, newname, ret);
		}
	}
	sceSyscallLeave(tls);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 1088, "sceIoRename",
					  "call org ret = %d 0x%08x\n", ret, ret);
	return ret;
}

int ksceIoDevctl(const char *dev, unsigned int cmd, void *indata, int inlen, void *outdata,
		 int outlen)
{
	IofilemgrDevctlParam param;
	SceUID impersonateId;
	SceUInt32 tls;
	int ret;

	impersonateId = ksceKernelGetProcessIdFromTLS();
	tls = sceSyscallEnter();
	ret = getPathMntOpt(dev, 0);
	if (ret < 0) {
		sceSyscallLeave(tls);
		goto error;
	}
	if (g_ioSchedEnabled == 1) {
		if (cmd & IOFILEMGR_IOCTL_ACCESS_CHECK) {
			if (cmd & IOFILEMGR_IOCTL_ACCESS_SYSTEM) {
				if (!ksceSblACIsSystemProgram(impersonateId) &&
				    !ksceSblACMgrIsRootProgram2(impersonateId)) {
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0xf, 1121, "sceIoDevctl",
								  "error : sceIoDevctl() failed\n");
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0xf, 1122, "sceIoDevctl",
								  "impersonateId = 0x%08x\n",
								  impersonateId);
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0xf, 1124, "sceIoDevctl",
								  "sceSblACMgrIsSystemProgram() = %d\n",
								  ksceSblACIsSystemProgram(impersonateId));
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0xf, 1126, "sceIoDevctl",
								  "sceSblACMgrIsKernelProgram() = %d\n",
								  ksceSblACMgrIsRootProgram2(impersonateId));
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0xf, 1127, "sceIoDevctl",
								  "cmd=%x\n", cmd);
					sceSyscallLeave(tls);
					return SCE_ERROR_ERRNO_EUNSUP;
				}
			} else if (!ksceSblACIsSystemProgram(impersonateId) ||
				   ksceSblACMgrIsRootProgram2(impersonateId)) {
				SCE_DBG_PRINTF(0xf, 1134, "error : sceIoDevctl() failed!\n");
				SCE_DBG_PRINTF(0xf, 1135, "impersonateId = 0x%08x\n", impersonateId);
				SCE_DBG_PRINTF(0xf, 1137, "sceSblACMgrIsSystemProgram() = %d\n",
					       ksceSblACIsSystemProgram(impersonateId));
				SCE_DBG_PRINTF(0xf, 1139, "sceSblACMgrIsKernelProgram() = %d\n",
					       ksceSblACMgrIsRootProgram2(impersonateId));
				SCE_DBG_PRINTF(0xf, 1140, "cmd=%x\n", cmd);
				sceSyscallLeave(tls);
				return SCE_ERROR_ERRNO_EUNSUP;
			}
		}
		if (cmd & IOFILEMGR_IOCTL_DISABLED) {
			SCE_DBG_PRINTF(0xf, 1147, "error : sceIoDevctl() failed!\n");
			SCE_DBG_PRINTF(0xf, 1148, "impersonateId = 0x%08x\n", impersonateId);
			SCE_DBG_PRINTF(0xf, 1149, "cmd=%x\n", cmd);
			sceSyscallLeave(tls);
			return SCE_ERROR_ERRNO_EUNSUP;
		}
		ret = sceIoSchedulerDevctl(dev, cmd, indata, inlen, outdata, outlen, ret);
	} else {
		if (cmd & IOFILEMGR_IOCTL_ACCESS_CHECK) {
			if (cmd & IOFILEMGR_IOCTL_ACCESS_SYSTEM) {
				if (!ksceSblACIsSystemProgram(impersonateId) &&
				    !ksceSblACMgrIsRootProgram2(impersonateId)) {
					SCE_DBG_PRINTF(0xf, 1160, "error : sceIoDevctl() failed\n");
					SCE_DBG_PRINTF(0xf, 1161, "impersonateId = 0x%08x\n",
						       impersonateId);
					SCE_DBG_PRINTF(0xf, 1163, "sceSblACMgrIsSystemProgram() = %d\n",
						       ksceSblACIsSystemProgram(impersonateId));
					SCE_DBG_PRINTF(0xf, 1165, "sceSblACMgrIsKernelProgram() = %d\n",
						       ksceSblACMgrIsRootProgram2(impersonateId));
					SCE_DBG_PRINTF(0xf, 1166, "cmd=%x\n", cmd);
					sceSyscallLeave(tls);
					return SCE_ERROR_ERRNO_EUNSUP;
				}
			} else if (!ksceSblACIsSystemProgram(impersonateId) ||
				   ksceSblACMgrIsRootProgram2(impersonateId)) {
				SCE_DBG_PRINTF(0xf, 1173, "error : sceIoDevctl() failed!\n");
				SCE_DBG_PRINTF(0xf, 1174, "impersonateId = 0x%08x\n", impersonateId);
				SCE_DBG_PRINTF(0xf, 1176, "sceSblACMgrIsSystemProgram() = %d\n",
					       ksceSblACIsSystemProgram(impersonateId));
				SCE_DBG_PRINTF(0xf, 1178, "sceSblACMgrIsKernelProgram() = %d\n",
					       ksceSblACMgrIsRootProgram2(impersonateId));
				SCE_DBG_PRINTF(0xf, 1179, "cmd=%x\n", cmd);
				sceSyscallLeave(tls);
				return SCE_ERROR_ERRNO_EUNSUP;
			}
		}
		if (cmd & IOFILEMGR_IOCTL_DISABLED) {
			SCE_DBG_PRINTF(0xf, 1186, "error : sceIoDevctl() failed!\n");
			SCE_DBG_PRINTF(0xf, 1187, "impersonateId = 0x%08x\n", impersonateId);
			SCE_DBG_PRINTF(0xf, 1188, "cmd=%x\n", cmd);
			sceSyscallLeave(tls);
			return SCE_ERROR_ERRNO_EUNSUP;
		}
		param.dev = dev;
		param.cmd = cmd;
		param.indata = indata;
		param.inlen = inlen;
		param.outdata = outdata;
		param.outlen = outlen;
		vfs_increment_io_count();
		vfs_set_sync_pending();
		ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsDevctl,
					     &param);
		vfs_decrement_io_count();
	}
	sceSyscallLeave(tls);
error:
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 1217, "sceIoDevctl",
					  "call org ret = %d 0x%08x\n", ret, ret);
	return ret;
}

int ksceIoSync(const char *device, unsigned int flag)
{
	IofilemgrSyncParam param;
	SceUInt32 tls;
	int ret;

	tls = sceSyscallEnter();
	ret = getPathMntOpt(device, 1);
	if (ret >= 0) {
		if (g_ioSchedEnabled != 1) {
			param.device = device;
			param.flag = flag;
			vfs_increment_io_count();
			vfs_set_sync_pending();
			ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsSync, &param);
			vfs_decrement_io_count();
		} else {
			ret = sceIoSchedulerSync(device, flag, ret);
		}
	}
	sceSyscallLeave(tls);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 1268, "sceIoSync",
					  "call org ret = %d 0x%08x\n", ret, ret);
	return ret;
}

int ksceIoGetGUIDFdListForDebugger(int vis_level, SceIoFdInfo *dst, SceSize max_size,
				   SceSize *res_size)
{
	SceUID *vector;
	SceSize num;
	int ret;
	int i;

	ret = ksceGUIDGetUIDVectorByClass(&g_vfsFileClass, vis_level, NULL, 0, &num);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 1291, "sceIoGetGUIDFdListForDebugger",
					  "sceGUIDGetUIDVectorByClass failed ... 0x%x %d\n",
					  ret, ret);
		return ret;
	}
	if (dst == NULL)
		return ret;
	vector = ksceKernelAllocHeapMemory(g_ioVfsHeapUid, ret * sizeof(SceUID));
	if (vector == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = ksceGUIDGetUIDVectorByClass(&g_vfsFileClass, vis_level, vector, ret, &num);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 1317, "sceIoGetGUIDFdListForDebugger",
					  "sceGUIDGetUIDVectorByClass failed ... 0x%x %d\n",
					  ret, ret);
		ksceKernelFreeHeapMemory(g_ioVfsHeapUid, vector);
		return ret;
	}
	for (i = 0; i < (int)num; i++) {
		dst[i].fd = vector[i];
		dst[i].pid = SCE_KERNEL_PROCESS_ID;
	}
	*res_size = (int)max_size < (int)num ? max_size : num;
	ksceKernelFreeHeapMemory(g_ioVfsHeapUid, vector);
	return num;
}

int ksceIoGetPUIDFdListForDebugger(int vis_level, SceIoFdInfo *dst, SceSize max_size,
				   SceSize *res_size)
{
	SceUID pidlist[IOFILEMGR_PROCESS_MAX];
	int fdnum[IOFILEMGR_PROCESS_MAX];
	SceUID *readbuf;
	SceUID pid;
	SceSize num;
	int total;
	int ret;
	int i, j;

	total = 0;
	for (i = 0; i < IOFILEMGR_PROCESS_MAX; i++) {
		pid = g_iofilemgrPid[i];
		pidlist[i] = pid;
		if (pid != -1) {
			ret = SceSysmemForKernel_08C05493(pid, &g_vfsFileClass, vis_level, NULL, 0, &num);
			if (ret >= 0) {
				fdnum[i] = ret;
				total += ret;
				SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 1374, "sceIoGetPUIDFdListForDebugger",
							  "pid=0x%x fdnum[%d]=%d num=%d ret=%d\n", pid, i,
							  ret, num, ret);
			} else {
				SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 1369, "sceIoGetPUIDFdListForDebugger",
							  "scePUIDGetUIDVectorByClass failed ... 0x%x %d\n", ret, ret);
				fdnum[i] = 0;
			}
		} else {
			fdnum[i] = 0;
		}
	}
	if (dst == NULL)
		return total;
	readbuf = ksceKernelAllocHeapMemory(g_ioVfsHeapUid, total * sizeof(SceUID));
	if (readbuf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	total = 0;
	for (i = 0; i < IOFILEMGR_PROCESS_MAX; i++) {
		pid = pidlist[i];
		if (pid == -1)
			continue;
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 1398, "sceIoGetPUIDFdListForDebugger",
					  "i=%d pidlist[%d]=0x%x\n", i, i, pid);
		ret = SceSysmemForKernel_08C05493(pid, &g_vfsFileClass, vis_level, &readbuf[total],
						  fdnum[i], &num);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 1407, "sceIoGetPUIDFdListForDebugger",
						  "scePUIDGetUIDVectorByClass failed ... 0x%x %d\n", ret, ret);
			continue;
		}
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 1410, "sceIoGetPUIDFdListForDebugger",
					  "fdnum[%d]=%d num=%d ret=%d\n", i, fdnum[i], num,
					  ret);
		for (j = 0; j < (int)num; j++) {
			dst[total + j].fd = readbuf[total + j];
			dst[total + j].pid = pid;
		}
		total += num;
	}
	if (total < (int)max_size) {
		*res_size = total;
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 1430, "sceIoGetPUIDFdListForDebugger",
					  "readbuf=%p size=0x%x\n", dst,
					  max_size * sizeof(SceUID));
		*res_size = max_size;
	}
	ksceKernelFreeHeapMemory(g_ioVfsHeapUid, readbuf);
	return total;
}

int ksceIoGetFileInfo(SceUID fd, SceUID pid, SceIofileInfo *info)
{
	if (pid != SCE_KERNEL_PROCESS_ID)
		fd = kscePUIDtoGUID(pid, fd);
	return vfsGetFdInfoForDebugger(fd, info);
}

static inline int init_iofm(void)
{
	SceKernelHeapCreateOpt opt;
	int ret;
	int i;

	opt.size = sizeof(opt);
	opt.attr = 1;
	g_ioVfsHeapUid = ksceKernelCreateHeap("SceIofile", 0x10000, &opt);
	if (g_ioVfsHeapUid < 0) {
		SCE_DBG_PRINTF(6, 155, "sceKernelCreateHeap failed: 0x%08x\n", g_ioVfsHeapUid);
		SCE_DBG_ASSERT_FAIL(156);
		return g_ioVfsHeapUid;
	}
	if (g_iofilemgrTlsKey == -1) {
		g_iofilemgrTlsKey = SceThreadmgrForDriver_B645C7EF(0x14, 0, 0);
		if (g_iofilemgrTlsKey < 0)
			return g_iofilemgrTlsKey;
	}
	ret = vfs_flock_init();
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 173, "ret=%x\n", ret);
		return ret;
	}
	SceSysrootForKernel_CBD377B7(iofilemgrStartMount);
	ret = vfsInitialize();
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 181, "ret=%x\n", ret);
		return ret;
	}
	for (i = 0; i < IOFILEMGR_PROCESS_MAX; i++)
		g_iofilemgrPid[i] = -1;
	ret = SceSysmemForKernel_B591E49F(3, 0x48);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 192, "ret=%x\n", ret);
		return ret;
	}
	ret = SceSysmemForKernel_B591E49F(4, 0xf8);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 199, "ret=%x\n", ret);
		return ret;
	}
	return ret;
}

int IofilemgrModuleStart(SceSize args, void *argp)
{
	SceProcEventHandler handler;

	if (ksceKernelInitializeFastMutex(&g_iofilemgrMutex, "SceIofile", 0, 0) < 0)
		SCE_PANIC(1475);
	if (init_iofm() < 0)
		SCE_PANIC(1481);
	handler.size = sizeof(handler);
	handler.create = IofilemgrModuleProcCreate;
	handler.exit = IofilemgrModuleProcExit;
	handler.kill = IofilemgrModuleProcKill;
	handler.stop = IofilemgrModuleProcSuspend;
	handler.start = IofilemgrModuleProcResume;
	handler.switch_process = NULL;
	if (ksceKernelRegisterProcEventHandler("SceIofilemgr", &handler, 0) < 0)
		SCE_PANIC(1496);
	if (sceIoSchedulerInitialize() < 0)
		SCE_PANIC(1503);
	if (sceIoAsyncInitialize() < 0)
		SCE_PANIC(1508);
	sceKernelRegisterInitCallback(iofilemgrInitDummyTty, 5, 0, NULL);
	g_vfsVopHook = NULL;
	return SCE_KERNEL_START_SUCCESS;
}
int module_start(SceSize args, void *argp) __attribute__((alias("IofilemgrModuleStart")));

int ksceIoPread(SceUID fd, void *data, SceSize size, SceOff offset)
{
	IofilemgrSchedInfo info;
	IofilemgrPreadParam param;
	SceUInt32 tls;
	int ret;

	tls = sceSyscallEnter();
	ret = getFileSchedParam(fd, &info, 0);
	if (g_ioSchedEnabled == 1 && ret >= 0 && info.mntOpt >= 0) {
		ret = sceIoSchedulerPread(fd, data, size, offset, &info);
	} else {
		param.fd = fd;
		param.data = data;
		param.size = size;
		param.padding = 0;
		param.offset = offset;
		vfs_increment_io_count();
		vfs_set_sync_pending();
		ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsPread, &param);
		vfs_decrement_io_count();
	}
	sceSyscallLeave(tls);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 1865, "sceIoPread",
					  "call org ret = %d 0x%08x\n", ret, ret);
	return ret;
}

int ksceIoPwrite(SceUID fd, const void *data, SceSize size, SceOff offset)
{
	IofilemgrSchedInfo info;
	IofilemgrPwriteParam param;
	SceUInt32 tls;
	int ret;

	tls = sceSyscallEnter();
	ret = getFileSchedParam(fd, &info, 0);
	if (g_ioSchedEnabled == 1 && ret >= 0 && info.mntOpt >= 0) {
		ret = sceIoSchedulerPwrite(fd, data, size, offset, &info);
	} else {
		param.fd = fd;
		param.data = data;
		param.size = size;
		param.padding = 0;
		param.offset = offset;
		vfs_increment_io_count();
		vfs_set_sync_pending();
		ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsPwrite, &param);
		vfs_decrement_io_count();
	}
	sceSyscallLeave(tls);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 1913, "sceIoPwrite",
					  "call org ret = %d 0x%08x\n", ret, ret);
	return ret;
}

int ksceIoGetstatByFd(SceUID fd, SceIoStat *stat)
{
	IofilemgrGetstatByFdParam param;
	IofilemgrSchedInfo info;
	SceUInt32 tls;
	int ret;

	tls = sceSyscallEnter();
	ret = getFileSchedParam(fd, &info, 0);
	if (g_ioSchedEnabled == 1 && ret >= 0 && info.mntOpt >= 0) {
		ret = sceIoSchedulerGetstatByFd(fd, stat, &info);
	} else {
		param.fd = fd;
		param.stat = stat;
		vfs_increment_io_count();
		vfs_set_sync_pending();
		ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsGetstatByFd, &param);
		vfs_decrement_io_count();
	}
	sceSyscallLeave(tls);
	return ret;
}

int ksceIoChstatByFd(SceUID fd, SceIoStat *stat, int bits)
{
	IofilemgrChstatByFdParam param;
	IofilemgrSchedInfo info;
	SceUInt32 tls;
	int ret;

	tls = sceSyscallEnter();
	ret = getFileSchedParam(fd, &info, 0);
	if (g_ioSchedEnabled == 1 && ret >= 0 && info.mntOpt >= 0) {
		ret = sceIoSchedulerChstatByFd(fd, stat, bits, &info);
	} else {
		param.fd = fd;
		param.stat = stat;
		param.bits = bits;
		vfs_increment_io_count();
		vfs_set_sync_pending();
		ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsChstatByFd, &param);
		vfs_decrement_io_count();
	}
	sceSyscallLeave(tls);
	return ret;
}

int ksceIoSyncByFd2(SceUID fd, int flag)
{
	IofilemgrSyncByFdParam param;
	IofilemgrSchedInfo info;
	SceUInt32 tls;
	int ret;

	tls = sceSyscallEnter();
	ret = getFileSchedParam(fd, &info, 0);
	if (g_ioSchedEnabled == 1 && ret >= 0 && info.mntOpt >= 0) {
		ret = sceIoSchedulerSyncByFd(fd, flag, &info);
	} else {
		param.fd = fd;
		param.flag = flag;
		vfs_increment_io_count();
		vfs_set_sync_pending();
		ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsSyncByFd, &param);
		vfs_decrement_io_count();
	}
	sceSyscallLeave(tls);
	return ret;
}

int ksceIoFlock(SceUID fd, int op)
{
	IofilemgrFlockParam param;
	int ret;

	param.fd = fd;
	param.op = op;
	vfs_increment_io_count();
	vfs_set_sync_pending();
	ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsFlock, &param);
	vfs_decrement_io_count();
	return ret;
}

int ksceIoGetstatForVshbridge(const char *file, SceIoStat *stat)
{
	IofilemgrGetstatParam param;
	SceUInt32 tls;
	int ret;

	tls = sceSyscallEnter();
	ret = getPathMntOpt(file, 1);
	if (ret >= 0) {
		if (g_ioSchedEnabled != 1) {
			param.file = file;
			param.stat = stat;
			param.preserveAttributes = 1;
			vfs_increment_io_count();
			vfs_set_sync_pending();
			ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsGetstat, &param);
			vfs_decrement_io_count();
		} else {
			ret = sceIoSchedulerGetstatForVsh(file, stat, ret);
		}
	}
	sceSyscallLeave(tls);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 2110, "sceIoGetstatForVshbridge",
					  "call org ret = %d 0x%08x\n", ret, ret);
	return ret;
}

int ksceIoChstatForVshbridge(const char *file, SceIoStat *stat, int bits)
{
	IofilemgrChstatParam param;
	SceUInt32 tls;
	int ret;

	tls = sceSyscallEnter();
	ret = getPathMntOpt(file, ((bits ^ SCE_CST_SIZE) >> 2) & 1);
	if (ret >= 0) {
		if (g_ioSchedEnabled != 1) {
			param.file = file;
			param.stat = stat;
			param.bits = bits;
			param.forVshbridge = 1;
			vfs_increment_io_count();
			vfs_set_sync_pending();
			ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsChstat, &param);
			vfs_decrement_io_count();
		} else {
			ret = sceIoSchedulerChstatForVsh(file, stat, bits, ret);
		}
	}
	sceSyscallLeave(tls);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 2165, "sceIoChstatForVshbridge",
					  "call org ret = %d 0x%08x\n", ret, ret);
	return ret;
}

int ksceIoDreadForVshbridge(SceUID fd, SceIoDirent *dir)
{
	IofilemgrSchedInfo info;
	SceUInt32 tls;
	int ret;

	tls = sceSyscallEnter();
	ret = getFileSchedParam(fd, &info, 0);
	if (g_ioSchedEnabled == 1 && ret >= 0 && info.mntOpt >= 0) {
		ret = sceIoSchedulerDreadForVsh(fd, dir, &info);
	} else {
		IofilemgrDreadParam param;

		param.fd = fd;
		param.dir = dir;
		param.preserveAttributes = 1;
		vfs_increment_io_count();
		vfs_set_sync_pending();
		ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfsDread, &param);
		vfs_decrement_io_count();
	}
	sceSyscallLeave(tls);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 2212, "sceIoDreadForVshbridge",
					  "call org ret = %d 0x%08x\n", ret, ret);
	return ret;
}

/* the number of fds the debug dump lists when a process reaches its limit */
#define IOFILEMGR_FD_DUMP_MAX	128

int check_limit_and_increment_user_fd_count(SceUID pid, SceUID thid)
{
	IofilemgrKpls *kpls;
	volatile SceInt32 old, new;
#if FW_DEBUG
	SceUID fdlist[IOFILEMGR_FD_DUMP_MAX];
	SceSize num;
	SceVfsFile *file;
	struct DebugPath *debugPath;
	SceUID fd;
	int i;
#endif
	int ret;

	if (pid == SCE_KERNEL_DUMMY_PROCESS_GAME_ID || pid == SCE_KERNEL_PROCESS_ID ||
	    pid == SCE_KERNEL_DUMMY_PROCESS_SYSTEM_ID)
		return 0;
	SCE_DBG_ASSERT(g_iofilemgrKplsKey != -1, 2237);
	ret = ksceKernelGetProcessLocalStorageAddrForPid(pid, g_iofilemgrKplsKey, (void **)&kpls, 1);
	if (ret < 0)
		return ret;
	for (;;) {
		if (SceThreadmgrForDriver_1096042E(thid)) {
			old = new = kpls->dialogFdCount;
			if (kpls->dialogFdMax <= old) {
#if FW_DEBUG
			ret = SceSysmemForKernel_08C05493(pid, &g_vfsFileClass, 7, fdlist,
							  IOFILEMGR_FD_DUMP_MAX, &num);
			if (ret < 0) {
				SCE_DBG_PRINTF(0xf, 2275,
					       "scePUIDGetUIDVectorByClass failed : 0x%08x\n", ret);
				return SCE_ERROR_ERRNO_EMFILE;
			}
			SCE_DBG_PRINTF(0xf, 2279, "This thread is Common Dialog.\n");
			SCE_DBG_PRINTF(0xf, 2280, "process id = 0x%08x\n", pid);
			for (i = 0; i < (int)num; i++) {
				fd = kscePUIDtoGUID(pid, fdlist[i]);
				if (fd < 0) {
					SCE_DBG_PRINTF(0xf, 2287,
						       "PUIDtoGUID failed err= %d 0x%x puid=0x%x\n", fd,
						       fd, fdlist[i]);
					continue;
				}
				ret = vfs_convert_file(fd, &file);
				if (ret == 0) {
					vfsLockFd(file);
					debugPath = file->debug_path;
					if (debugPath != NULL) {
						SCE_DBG_PRINTF(0xf, 2302, "openPath = %s\n",
							       debugPath->openPath);
						SCE_DBG_PRINTF(0xf, 2303, "resolvedOpenPath = %s\n",
							       debugPath->resolvedOpenPath);
						vfsUnlockFd(file);
						ret = vfs_release_file(fd);
						if (ret < 0)
							SCE_DBG_PRINTF(0xf, 2308,
							       "vfs_release_file failed : fd=%d 0x%x ret=%d 0x%x\n",
							       fd, fd, ret, ret);
					} else {
						vfsUnlockFd(file);
						vfs_release_file(fd);
					}
				} else {
					SCE_DBG_PRINTF(0xf, 2313,
						       "vfs_convert_file failed : fd=%d 0x%x ret=%d 0x%x\n",
						       fd, fd, ret, ret);
				}
			}
#endif
			return SCE_ERROR_ERRNO_EMFILE;
			}
			new++;
			if (ksceKernelAtomicCompareAndSet32(&kpls->dialogFdCount, old, new) != old)
				continue;
			if (kpls->dialogFdPeak < new)
				ksceKernelAtomicAddAndGet32(&kpls->dialogFdPeak, 1);
		} else {
			old = new = kpls->fdCount;
			if (kpls->fdMax <= old) {
#if FW_DEBUG
			ret = SceSysmemForKernel_08C05493(pid, &g_vfsFileClass, 7, fdlist,
							  IOFILEMGR_FD_DUMP_MAX, &num);
			if (ret < 0) {
				SCE_DBG_PRINTF(0xf, 2347,
					       "scePUIDGetUIDVectorByClass failed : 0x%08x\n", ret);
				return SCE_ERROR_ERRNO_EMFILE;
			}
			SCE_DBG_PRINTF(0xf, 2351, "This thread is not Common Dialog.\n");
			SCE_DBG_PRINTF(0xf, 2352, "process id = 0x%08x\n", pid);
			for (i = 0; i < (int)num; i++) {
				fd = kscePUIDtoGUID(pid, fdlist[i]);
				if (fd < 0) {
					SCE_DBG_PRINTF(0xf, 2359,
						       "PUIDtoGUID failed err= %d 0x%x puid=0x%x\n", fd,
						       fd, fdlist[i]);
					continue;
				}
				ret = vfs_convert_file(fd, &file);
				if (ret == 0) {
					vfsLockFd(file);
					debugPath = file->debug_path;
					if (debugPath != NULL) {
						SCE_DBG_PRINTF(0xf, 2374, "openPath = %s\n",
							       debugPath->openPath);
						SCE_DBG_PRINTF(0xf, 2375, "resolvedOpenPath = %s\n",
							       debugPath->resolvedOpenPath);
						vfsUnlockFd(file);
						ret = vfs_release_file(fd);
						if (ret < 0)
							SCE_DBG_PRINTF(0xf, 2380,
							       "vfs_release_file failed : fd=%d 0x%x ret=%d 0x%x\n",
							       fd, fd, ret, ret);
					} else {
						vfsUnlockFd(file);
						vfs_release_file(fd);
					}
				} else {
					SCE_DBG_PRINTF(0xf, 2385,
						       "vfs_convert_file failed : fd=%d 0x%x ret=%d 0x%x\n",
						       fd, fd, ret, ret);
				}
			}
#endif
			return SCE_ERROR_ERRNO_EMFILE;
			}
			new++;
			if (ksceKernelAtomicCompareAndSet32(&kpls->fdCount, old, new) != old)
				continue;
			if (kpls->fdPeak < new)
				ksceKernelAtomicAddAndGet32(&kpls->fdPeak, 1);
		}
		return 0;
	}
}

/* name unknown: undo check_limit_and_increment_user_fd_count (keepPeak: leave the peak) */
void decrement_user_fd_count(SceUID pid, SceUID thid, int keepPeak)
{
	IofilemgrKpls *kpls;

	if (pid == SCE_KERNEL_DUMMY_PROCESS_GAME_ID || pid == SCE_KERNEL_PROCESS_ID ||
	    pid == SCE_KERNEL_DUMMY_PROCESS_SYSTEM_ID)
		return;
	if (ksceKernelGetProcessLocalStorageAddrForPid(pid, g_iofilemgrKplsKey, (void **)&kpls, 1) < 0)
		return;
	if (SceThreadmgrForDriver_1096042E(thid)) {
		ksceKernelAtomicSubAndGet32(&kpls->dialogFdCount, 1);
		if (!keepPeak)
			ksceKernelAtomicSubAndGet32(&kpls->dialogFdPeak, 1);
	} else {
		ksceKernelAtomicSubAndGet32(&kpls->fdCount, 1);
		if (!keepPeak)
			ksceKernelAtomicSubAndGet32(&kpls->fdPeak, 1);
	}
}

int check_limit_user_fd_count(SceUID pid, SceUID thid)
{
	IofilemgrKpls *kpls;
	int ret;

	if (pid == SCE_KERNEL_DUMMY_PROCESS_GAME_ID || pid == SCE_KERNEL_PROCESS_ID ||
	    pid == SCE_KERNEL_DUMMY_PROCESS_SYSTEM_ID)
		return 0;
	SCE_DBG_ASSERT(g_iofilemgrKplsKey != -1, 2449);
	ret = ksceKernelGetProcessLocalStorageAddrForPid(pid, g_iofilemgrKplsKey, (void **)&kpls, 1);
	if (ret < 0)
		return ret;
	if (SceThreadmgrForDriver_1096042E(thid)) {
		if (kpls->dialogFdCount < kpls->dialogFdMax)
			return 0;
	} else {
		if (kpls->fdCount < kpls->fdMax)
			return 0;
	}
	return SCE_ERROR_ERRNO_EMFILE;
}

int ksceIoSetPathMappingFunction(IoPathMappingFunction func)
{
	if (g_ioPathMappingFunction != NULL && func != NULL)
		return SCE_ERROR_ERRNO_EBUSY;
	g_ioPathMappingFunction = func;
	return 0;
}

/* name unknown: the calling thread's IofilemgrTls, created and initialized on first use */
IofilemgrTls *getIofilemgrTls(void)
{
	IofilemgrTls *tls;

	tls = SceThreadmgrForDriver_89CA5698(g_iofilemgrTlsKey, 0, 0);
	if (tls == NULL) {
		tls = SceThreadmgrForDriver_89CA5698(g_iofilemgrTlsKey, 0, 1);
		tls->priority = IO_PRIORITY_UNSET;
		tls->pathLength = 0;
		tls->request = NULL;
		tls->requesterThreadId = 0;
		tls->inAsyncCall = 0;
		tls->isCountedWorker = 0;
	}
	return tls;
}

/* set a process's fd limit */
int SceIofilemgrForDriver_AC0E9AAA(SceUID pid, SceInt32 fdMax)
{
	IofilemgrKpls *kpls;
	int ret;

	ret = ksceKernelGetProcessLocalStorageAddrForPid(pid, g_iofilemgrKplsKey, (void **)&kpls, 1);
	if (ret >= 0) {
		kpls->fdMax = fdMax;
		ret = 0;
	}
	return ret;
}

int SceIofilemgrForDriver_03F6A684(SceUID pid, SceInt32 depth)
{
	IofilemgrKpls *kpls;
	int ret;

	ret = ksceKernelGetProcessLocalStorageAddrForPid(pid, g_iofilemgrKplsKey, (void **)&kpls, 1);
	if (ret >= 0) {
		kpls->depth = depth;
		ret = 0;
	}
	return ret;
}

int vfsGetMntList(VfsMountListEntry *list, int max, int *num)
{
	return vfs_get_mount_list(list, max, num);
}

int vfsGetMntInfo(SceVfsMount *mnt, VfsMountInfo *info)
{
	return vfs_get_mount_info(mnt, info);
}

int ksceIoGetRemoteKPLSData(SceUID pid, void *dst)
{
	IofilemgrRemoteKplsData *data = dst;
	IofilemgrKpls *kpls;
	int ret;

	if (pid == SCE_KERNEL_DUMMY_PROCESS_GAME_ID || pid == SCE_KERNEL_PROCESS_ID ||
	    pid == SCE_KERNEL_DUMMY_PROCESS_SYSTEM_ID)
		return SCE_ERROR_ERRNO_ENOENT;
	ret = ksceKernelGetProcessLocalStorageAddrForPid(pid, g_iofilemgrKplsKey, (void **)&kpls, 1);
	if (ret < 0)
		return ret;
	data->fdCount = kpls->fdCount;
	data->fdPeak = kpls->fdPeak;
	data->dialogFdCount = kpls->dialogFdCount;
	data->dialogFdPeak = kpls->dialogFdPeak;
	data->fdMax = kpls->fdMax;
	data->depth = kpls->depth;
	data->priority = kpls->priority;
	return ret;
}

/*
 * The system calls: the arguments are copied from user memory (sce/syscall_copy.h)
 * and passed to the sceIo*ForUser functions. Only the argument blocks named below
 * are used; the others are read and ignored.
 */

typedef struct SyscallLseekArgs {
	SceOff offset;
	int whence;
	SceUInt32 reserved;
} SyscallLseekArgs;

typedef struct SyscallCtlArgs {
	int inlen;
	void *outdata;
	int outlen;
	SceUInt32 reserved;
} SyscallCtlArgs;

typedef struct SyscallCtlAsyncArgs {
	int inlen;
	void *outdata;
	int outlen;
	void *asyncParam;
	SceUInt32 reserved[2];
} SyscallCtlAsyncArgs;

typedef struct SyscallAsyncArgs {
	void *asyncParam;
	SceUInt32 reserved[3];
} SyscallAsyncArgs;

typedef struct SyscallLseekAsyncArgs {
	SceOff offset;
	int whence;
	void *asyncParam;
	SceUInt32 reserved[2];
} SyscallLseekAsyncArgs;

typedef struct SyscallPreadArgs {
	SceOff offset;
	SceUInt32 reserved[2];
} SyscallPreadArgs;

typedef struct SyscallPreadAsyncArgs {
	SceOff offset;
	void *asyncParam;
	SceUInt32 reserved;
} SyscallPreadAsyncArgs;

SceUID _sceIoOpen(const char *file, int flags, SceMode mode, const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *kfile = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(kfile, file, 0x400);
	return sceIoOpenForUser(kfile, flags, mode);
}

SceOff _sceIoLseek(SceUID fd, const SyscallLseekArgs *uargs)
{
	SyscallLseekArgs args;
	int ret;

	sceKernelApiDeclareTypeInt64();
	ARGS_IN(args, uargs);
	return sceIoLseekForUser(fd, args.offset, args.whence);
}

int _sceIoRemove(const char *file, const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *kfile = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(kfile, file, 0x400);
	return sceIoRemoveForUser(kfile);
}

int _sceIoRename(const char *oldname, const char *newname, const SyscallAsyncArgs *uargs)
{
	SyscallAsyncArgs args;
	char *kold = NULL;
	char *knew = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(kold, oldname, 0x400);
	STRING_IN(knew, newname, 0x400);
	return sceIoRenameForUser(kold, knew);
}

int _sceIoMkdir(const char *dir, SceMode mode, const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *kdir = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(kdir, dir, 0x400);
	return sceIoMkdirForUser(kdir, mode);
}

int _sceIoRmdir(const char *path, const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *kpath = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(kpath, path, 0x400);
	return sceIoRmdirForUser(kpath);
}

SceUID _sceIoDopen(const char *dirname, const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *kdirname = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(kdirname, dirname, 0x400);
	return sceIoDopenForUser(kdirname);
}

/* the dirent is read in first: its private data passes through */
int _sceIoDread(SceUID fd, SceIoDirent *dir)
{
	SceIoDirent *kdir;
	int ret;

	if (dir == NULL)
		return sceIoDreadForUser(fd, NULL);
	kdir = __builtin_alloca(sizeof(*kdir));
	__builtin_memset(kdir, 0, sizeof(*kdir));
	ret = ksceKernelCopyFromUser(kdir, dir, sizeof(*kdir));
	if (ret < 0)
		return ret;
	ret = sceIoDreadForUser(fd, kdir);
	return copyResult(ret, ksceKernelCopyToUser(dir, kdir, sizeof(*kdir)));
}

int _sceIoGetstat(const char *file, SceIoStat *stat, const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *kfile = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(kfile, file, 0x400);
	if (stat != NULL) {
		SceIoStat *kstat = __builtin_alloca(sizeof(*kstat));

		__builtin_memset(kstat, 0, sizeof(*kstat));
		ret = sceIoGetstatForUser(kfile, kstat);
		return copyResult(ret, ksceKernelCopyToUser(stat, kstat, sizeof(*kstat)));
	}
	return sceIoGetstatForUser(kfile, NULL);
}

int _sceIoChstat(const char *file, const SceIoStat *stat, int bits, const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *kfile = NULL;
	SceIoStat *kstat = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(kfile, file, 0x400);
	BLOCK_IN(kstat, stat, sizeof(*kstat));
	return sceIoChstatForUser(kfile, kstat, bits);
}

int _sceIoSync(const char *device, unsigned int flag, const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *kdevice = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(kdevice, device, 0x400);
	return sceIoSyncForUser(kdevice, flag);
}

int _sceIoDevctl(const char *dev, unsigned int cmd, void *indata, const SyscallCtlAsyncArgs *uargs)
{
	SyscallCtlAsyncArgs args;
	char *kdev = NULL;
	void *outdata;
	int inlen, outlen;
	int ret;

	ARGS_IN(args, uargs);
	inlen = args.inlen;
	outdata = args.outdata;
	outlen = args.outlen;
	STRING_IN(kdev, dev, 0x400);
	return sceIoDevctlForUser(kdev, cmd, indata, inlen, outdata, outlen);
}

int _sceIoIoctl(SceUID fd, unsigned int cmd, void *indata, const SyscallCtlArgs *uargs)
{
	SyscallCtlArgs args;
	int ret;

	ARGS_IN(args, uargs);
	return sceIoIoctlForUser(fd, cmd, indata, args.inlen, args.outdata, args.outlen);
}

int _sceIoGetstatByFd(SceUID fd, SceIoStat *stat)
{
	int ret;

	BLOCK_OUT(stat, sizeof(SceIoStat), sceIoGetstatByFdForUser(fd, NULL),
		  sceIoGetstatByFdForUser(fd, (SceIoStat *)k));
}

int _sceIoChstatByFd(SceUID fd, const SceIoStat *stat, int bits)
{
	SceIoStat *kstat = NULL;
	int ret;

	BLOCK_IN(kstat, stat, sizeof(*kstat));
	return sceIoChstatByFdForUser(fd, kstat, bits);
}

SceUID _sceIoOpenAsync(const char *file, int flags, SceMode mode, const SyscallAsyncArgs *uargs)
{
	SyscallAsyncArgs args;
	char *kfile = NULL;
	void *asyncParam;
	int ret;

	ARGS_IN(args, uargs);
	asyncParam = args.asyncParam;
	STRING_IN(kfile, file, 0x400);
	return sceIoOpenAsyncForUser(kfile, flags, mode, asyncParam);
}

SceUID _sceIoLseekAsync(SceUID fd, const SyscallLseekAsyncArgs *uargs)
{
	SyscallLseekAsyncArgs args;
	int ret;

	ARGS_IN(args, uargs);
	return sceIoLseekAsyncForUser(fd, args.offset, args.whence, args.asyncParam);
}

SceUID _sceIoIoctlAsync(SceUID fd, unsigned int cmd, void *indata, const SyscallCtlAsyncArgs *uargs)
{
	SyscallCtlAsyncArgs args;
	int ret;

	ARGS_IN(args, uargs);
	return sceIoIoctlAsyncForUser(fd, cmd, indata, args.inlen, args.outdata, args.outlen,
				      args.asyncParam);
}

SceUID _sceIoRemoveAsync(const char *file, void *asyncParam, const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *kfile = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(kfile, file, 0x400);
	return sceIoRemoveAsyncForUser(kfile, asyncParam);
}

SceUID _sceIoMkdirAsync(const char *dir, SceMode mode, void *asyncParam, const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *kdir = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(kdir, dir, 0x400);
	return sceIoMkdirAsyncForUser(kdir, mode, asyncParam);
}

SceUID _sceIoRmdirAsync(const char *path, void *asyncParam, const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *kpath = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(kpath, path, 0x400);
	return sceIoRmdirAsyncForUser(kpath, asyncParam);
}

SceUID _sceIoRenameAsync(const char *oldname, const char *newname, void *asyncParam,
			 const SyscallAsyncArgs *uargs)
{
	SyscallAsyncArgs args;
	char *kold = NULL;
	char *knew = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(kold, oldname, 0x400);
	STRING_IN(knew, newname, 0x400);
	return sceIoRenameAsyncForUser(kold, knew, asyncParam);
}

SceUID _sceIoChstatAsync(const char *file, SceIoStat *stat, int bits, const SyscallAsyncArgs *uargs)
{
	SyscallAsyncArgs args;
	char *kfile = NULL;
	void *asyncParam;
	int ret;

	ARGS_IN(args, uargs);
	asyncParam = args.asyncParam;
	STRING_IN(kfile, file, 0x400);
	return sceIoChstatAsyncForUser(kfile, stat, bits, asyncParam);
}

SceUID _sceIoGetstatAsync(const char *file, SceIoStat *stat, void *asyncParam,
			  const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *kfile = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(kfile, file, 0x400);
	return sceIoGetstatAsyncForUser(kfile, stat, asyncParam);
}

SceUID _sceIoDevctlAsync(const char *dev, unsigned int cmd, void *indata, const SyscallCtlAsyncArgs *uargs)
{
	SyscallCtlAsyncArgs args;
	char *kdev = NULL;
	void *outdata, *asyncParam;
	int inlen, outlen;
	int ret;

	ARGS_IN(args, uargs);
	inlen = args.inlen;
	outdata = args.outdata;
	outlen = args.outlen;
	asyncParam = args.asyncParam;
	STRING_IN(kdev, dev, 0x400);
	return sceIoDevctlAsyncForUser(kdev, cmd, indata, inlen, outdata, outlen, asyncParam);
}

SceUID _sceIoSyncAsync(const char *device, unsigned int flag, void *asyncParam,
		       const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *kdevice = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(kdevice, device, 0x400);
	return sceIoSyncAsyncForUser(kdevice, flag, asyncParam);
}

SceSSize _sceIoPread(SceUID fd, void *data, SceSize size, const SyscallPreadArgs *uargs)
{
	SyscallPreadArgs args;
	int ret;

	ARGS_IN(args, uargs);
	return sceIoPreadForUser(fd, data, size, args.offset);
}

SceSSize _sceIoPwrite(SceUID fd, const void *data, SceSize size, const SyscallPreadArgs *uargs)
{
	SyscallPreadArgs args;
	int ret;

	ARGS_IN(args, uargs);
	return sceIoPwriteForUser(fd, data, size, args.offset);
}

SceUID _sceIoPreadAsync(SceUID fd, void *data, SceSize size, const SyscallPreadAsyncArgs *uargs)
{
	SyscallPreadAsyncArgs args;
	int ret;

	ARGS_IN(args, uargs);
	return sceIoPreadAsyncForUser(fd, data, size, args.offset, args.asyncParam);
}

SceUID _sceIoPwriteAsync(SceUID fd, const void *data, SceSize size, const SyscallPreadAsyncArgs *uargs)
{
	SyscallPreadAsyncArgs args;
	int ret;

	ARGS_IN(args, uargs);
	return sceIoPwriteAsyncForUser(fd, data, size, args.offset, args.asyncParam);
}

int _sceIoCompleteMultiple(void *a0, int a1, int *result)
{
	int ret;

	BLOCK_OUT(result, sizeof(int), sceIoCompleteMultipleForUser(a0, a1, NULL),
		  sceIoCompleteMultipleForUser(a0, a1, (int *)k));
}
