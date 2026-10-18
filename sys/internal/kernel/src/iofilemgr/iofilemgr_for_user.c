/*
 * SceIofilemgr: the calls of the SceIofilemgr (user) library (Sony's
 * iofilemgr_for_user.c).
 *
 * Each call is refused while the process holds the VM domain, turns the
 * process's fd (a PUID) into the kernel's, and runs the kernel call. A path
 * goes through the Fios2 overlay first when g_ioFiosPathEnabled is set (it
 * never is), and a buffer is mapped into the kernel for the call.
 */

#include <intrmgr/syscall.h>
#include "iofilemgr_internal.h"

int g_ioFiosPathEnabled __attribute__((section(".data"))) = 0;

/* the "critical" bit of an SCE error: a bad fd from a process (name unknown) */
#define IO_ERROR_CRITICAL	0x40000000

/* the size of a path the overlay resolves */
#define IO_FIOS_PATH_SIZE	0x400

/* ksceFiosKernelOverlayResolveSync: resolve for reading or for writing (names unknown) */
#define IO_FIOS_RESOLVE_READ	0
#define IO_FIOS_RESOLVE_WRITE	1

/* every call first checks the process does not hold the VM domain (name unknown) */
#define IO_CHECK_VM_DOMAIN(line) do {						\
	int vmDomain = SceSysmemForKernel_4E6D8BC3();				\
										\
	if (vmDomain > 0) {							\
		SCE_DBG_PRINTF_LEVEL(1, 0xb, line,				\
				     "sceKernelCheckOpenVMDomain returns 0x%08x\n",	\
				     vmDomain);					\
		SCE_DBG_ASSERT_FAIL(line);					\
		return (int)SCE_ERROR_ERRNO_ENOSYS;				\
	}									\
} while (0)

/* name unknown: a process's fd SceSysmemForDriver_45D22597 refused, as the error the call returns */
static inline int ioPuidError(SceUID err)
{
	return err != SCE_KERNEL_ERROR_INVALID_UID ? err | IO_ERROR_CRITICAL :
						     SCE_ERROR_ERRNO_EBADF | IO_ERROR_CRITICAL;
}

/* name unknown: a Fios2 error as an errno error (convertFiosError) */
static inline int ioFiosError(int err)
{
	if (((unsigned int)err << 4) >> 20 == 0x082)
		err = fiosErrorToErrno(err);
	return err;
}

SceUID sceIoOpenForUser(const char *file, int flags, SceMode mode)
{
	SceUInt32 tls = sceSyscallEnter();
	SceUID fd;

	IO_CHECK_VM_DOMAIN(252);
	fd = ksceIoOpenForPid(sceKernelGetProcessId(), file, flags, mode);
	if (fd > 0) {
		if (SceDipswForDriver_A98FC2FD(210))
			SceIofilemgrForDriver_2EFDFA12(fd, file);
		fd = SceSysmemForDriver_BF209859(0, fd);
	}
	sceSyscallLeave(tls);
	return fd;
}

int sceIoCloseForUser(SceUID fd)
{
	SceUID guid;
	int ret;

	IO_CHECK_VM_DOMAIN(280);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0) {
		ret = ioPuidError(guid);
	} else {
		ret = ksceIoClose(guid);
		if (ret == 0)
			SceSysmemForDriver_84A4AF5E(0, fd);
	}
	return ret;
}

SceSSize sceIoReadForUser(SceUID fd, void *data, SceSize size)
{
	void *kernelPage;
	SceSize kernelSize;
	SceUInt32 kernelOffset;
	char *kernelData;
	SceUID guid, mapUid;
	SceSSize ret, done;

	IO_CHECK_VM_DOMAIN(312);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0)
		return ioPuidError(guid);
	kernelData = NULL;
	if (size != 0) {
		mapUid = SceSysmemForDriver_7D4F8B5F("SceIoUserMapRead", IO_USER_MAP_WRITE,
						    SceSblACMgrForKernel_sceSblACMgrIsGameProgram(0) == 1 ?
						    IO_USER_MAP_FLAGS_GAME : IO_USER_MAP_FLAGS,
						    data, size, &kernelPage, &kernelSize,
						    &kernelOffset);
		if (mapUid < 0) {
			if (mapUid == SCE_KERNEL_ERROR_VA2PA_FAULT)
				return SCE_ERROR_ERRNO_EFAULT;
			return mapUid;
		}
		kernelData = (char *)kernelPage + kernelOffset;
	}
	ret = ksceIoRead(guid, kernelData, size);
	done = ret < 0 ? 0 : ret;
	if (kernelData != NULL)
		ioUserUnmap(mapUid, data, size);
	return ret < 0 ? ret : done;
}

SceSSize sceIoWriteForUser(SceUID fd, const void *data, SceSize size)
{
	void *kernelPage;
	SceSize kernelSize;
	SceUInt32 kernelOffset;
	char *kernelData;
	SceUID guid, mapUid;
	SceSSize ret, done;

	IO_CHECK_VM_DOMAIN(381);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0)
		return ioPuidError(guid);
	kernelData = NULL;
	if (size != 0) {
		mapUid = SceSysmemForDriver_7D4F8B5F("SceIoUserMapWrite", IO_USER_MAP_READ,
						    SceSblACMgrForKernel_sceSblACMgrIsGameProgram(0) == 1 ?
						    IO_USER_MAP_FLAGS_GAME : IO_USER_MAP_FLAGS,
						    data, size, &kernelPage, &kernelSize,
						    &kernelOffset);
		if (mapUid < 0) {
			if (mapUid == SCE_KERNEL_ERROR_VA2PA_FAULT)
				return SCE_ERROR_ERRNO_EFAULT;
			return mapUid;
		}
		kernelData = (char *)kernelPage + kernelOffset;
	}
	ret = ksceIoWrite(guid, kernelData, size);
	done = ret < 0 ? 0 : ret;
	if (kernelData != NULL)
		ioUserUnmap(mapUid, data, size);
	return ret < 0 ? ret : done;
}

SceOff sceIoLseekForUser(SceUID fd, SceOff offset, int whence)
{
	SceUID guid;

	IO_CHECK_VM_DOMAIN(559);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0)
		return ioPuidError(guid);
	return ksceIoLseek(guid, offset, whence);
}

int sceIoLseek32ForUser(SceUID fd, int offset, int whence)
{
	SceUID guid;

	IO_CHECK_VM_DOMAIN(576);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0)
		return ioPuidError(guid);
	return sceIoLseek32(guid, offset, whence);
}

int sceIoRemoveForUser(const char *file)
{
	char *resolved;
	SceUID pid;
	int ret;

	IO_CHECK_VM_DOMAIN(594);
	if (g_ioFiosPathEnabled == 0)
		return ksceIoRemove(file);

	pid = sceKernelGetProcessId();
	resolved = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, IO_FIOS_PATH_SIZE);
	if (resolved == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = ksceFiosKernelOverlayResolveSync(pid, IO_FIOS_RESOLVE_WRITE, file, resolved,
					       IO_FIOS_PATH_SIZE);
	if (ret < 0)
		ret = ioFiosError(ret);
	else
		ret = ksceIoRemove(resolved);
	sceKernelFreeHeapMemory(g_ioVfsHeapUid, resolved);
	return ret;
}

int sceIoRenameForUser(const char *oldname, const char *newname)
{
	char *resolvedOld, *resolvedNew;
	SceUID pid;
	int ret;

	IO_CHECK_VM_DOMAIN(625);
	if (g_ioFiosPathEnabled == 0)
		return ksceIoRename(oldname, newname);

	pid = sceKernelGetProcessId();
	resolvedOld = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, IO_FIOS_PATH_SIZE);
	if (resolvedOld == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = ksceFiosKernelOverlayResolveSync(pid, IO_FIOS_RESOLVE_WRITE, oldname, resolvedOld,
					       IO_FIOS_PATH_SIZE);
	if (ret < 0) {
		ret = ioFiosError(ret);
	} else {
		resolvedNew = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, IO_FIOS_PATH_SIZE);
		if (resolvedNew == NULL) {
			sceKernelFreeHeapMemory(g_ioVfsHeapUid, resolvedOld);
			return SCE_KERNEL_ERROR_NO_MEMORY;
		}
		ret = ksceFiosKernelOverlayResolveSync(pid, IO_FIOS_RESOLVE_WRITE, newname,
						       resolvedNew, IO_FIOS_PATH_SIZE);
		if (ret < 0)
			ret = ioFiosError(ret);
		else
			ret = ksceIoRename(resolvedOld, resolvedNew);
		sceKernelFreeHeapMemory(g_ioVfsHeapUid, resolvedNew);
	}
	sceKernelFreeHeapMemory(g_ioVfsHeapUid, resolvedOld);
	return ret;
}

int sceIoMkdirForUser(const char *dir, SceMode mode)
{
	char *resolved;
	SceUID pid;
	int ret;

	IO_CHECK_VM_DOMAIN(671);
	if (g_ioFiosPathEnabled == 0)
		return ksceIoMkdir(dir, mode);

	pid = sceKernelGetProcessId();
	resolved = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, IO_FIOS_PATH_SIZE);
	if (resolved == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = ksceFiosKernelOverlayResolveSync(pid, IO_FIOS_RESOLVE_WRITE, dir, resolved,
					       IO_FIOS_PATH_SIZE);
	if (ret < 0)
		ret = ioFiosError(ret);
	else
		ret = ksceIoMkdir(resolved, mode);
	sceKernelFreeHeapMemory(g_ioVfsHeapUid, resolved);
	return ret;
}

int sceIoRmdirForUser(const char *path)
{
	char *resolved;
	SceUID pid;
	int ret;

	IO_CHECK_VM_DOMAIN(702);
	if (g_ioFiosPathEnabled == 0)
		return ksceIoRmdir(path);

	pid = sceKernelGetProcessId();
	resolved = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, IO_FIOS_PATH_SIZE);
	if (resolved == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = ksceFiosKernelOverlayResolveSync(pid, IO_FIOS_RESOLVE_WRITE, path, resolved,
					       IO_FIOS_PATH_SIZE);
	if (ret < 0)
		ret = ioFiosError(ret);
	else
		ret = ksceIoRmdir(resolved);
	sceKernelFreeHeapMemory(g_ioVfsHeapUid, resolved);
	return ret;
}

SceUID sceIoDopenForUser(const char *dirname)
{
	SceUID fd;

	IO_CHECK_VM_DOMAIN(731);
	fd = sceIoDopenInternal(dirname);
	if (fd > 0)
		return SceSysmemForDriver_BF209859(0, fd);
	return fd;
}

int sceIoDcloseForUser(SceUID fd)
{
	SceUInt32 tls = sceSyscallEnter();
	SceUID guid;
	int ret;

	IO_CHECK_VM_DOMAIN(749);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0) {
		ret = ioPuidError(guid);
	} else {
		ret = sceIoDcloseInternal(guid);
		if (ret == 0)
			SceSysmemForDriver_84A4AF5E(0, fd);
	}
	sceSyscallLeave(tls);
	return ret;
}

int sceIoDreadForUser(SceUID fd, SceIoDirent *dir)
{
	SceUInt32 tls = sceSyscallEnter();
	SceUID guid;
	int ret;

	IO_CHECK_VM_DOMAIN(777);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0)
		ret = ioPuidError(guid);
	else
		ret = sceIoDreadInternal(guid, dir);
	sceSyscallLeave(tls);
	return ret;
}

int sceIoGetstatForUser(const char *file, SceIoStat *stat)
{
	char *resolved;
	SceUID pid;
	int ret;

	IO_CHECK_VM_DOMAIN(800);
	if (g_ioFiosPathEnabled == 0)
		return ksceIoGetstat(file, stat);

	pid = sceKernelGetProcessId();
	resolved = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, IO_FIOS_PATH_SIZE);
	if (resolved == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = ksceFiosKernelOverlayResolveSync(pid, IO_FIOS_RESOLVE_READ, file, resolved,
					       IO_FIOS_PATH_SIZE);
	if (ret < 0)
		ret = ioFiosError(ret);
	else
		ret = ksceIoGetstat(resolved, stat);
	sceKernelFreeHeapMemory(g_ioVfsHeapUid, resolved);
	return ret;
}

int sceIoChstatForUser(const char *file, const SceIoStat *stat, int bits)
{
	char *resolved;
	SceUID pid;
	int ret;

	IO_CHECK_VM_DOMAIN(831);
	if (g_ioFiosPathEnabled == 0)
		return ksceIoChstat(file, stat, bits);

	pid = sceKernelGetProcessId();
	resolved = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, IO_FIOS_PATH_SIZE);
	if (resolved == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = ksceFiosKernelOverlayResolveSync(pid, IO_FIOS_RESOLVE_WRITE, file, resolved,
					       IO_FIOS_PATH_SIZE);
	if (ret < 0)
		ret = ioFiosError(ret);
	else
		ret = ksceIoChstat(resolved, stat, bits);
	sceKernelFreeHeapMemory(g_ioVfsHeapUid, resolved);
	return ret;
}

int sceIoSyncForUser(const char *device, unsigned int flag)
{
	char *resolved;
	SceUID pid;
	int ret;

	IO_CHECK_VM_DOMAIN(862);
	if (g_ioFiosPathEnabled == 0)
		return ksceIoSync(device, flag);

	pid = sceKernelGetProcessId();
	resolved = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, IO_FIOS_PATH_SIZE);
	if (resolved == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = ksceFiosKernelOverlayResolveSync(pid, IO_FIOS_RESOLVE_WRITE, device, resolved,
					       IO_FIOS_PATH_SIZE);
	if (ret < 0)
		ret = ioFiosError(ret);
	else
		ret = ksceIoSync(resolved, flag);
	sceKernelFreeHeapMemory(g_ioVfsHeapUid, resolved);
	return ret;
}

int sceIoDevctlForUser(const char *dev, unsigned int cmd, const void *indata, SceSize inlen,
			void *outdata, SceSize outlen)
{
	void *kernelIn, *kernelOut = NULL;
	int inputHeap = 0, ret;

	IO_CHECK_VM_DOMAIN(898);
	if (cmd & IOFILEMGR_IOCTL_ACCESS_CHECK) {
		if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
			SCE_DBG_PRINTF_LEVEL(1, 0xf, 902, "error : sceIoDevctlForUser() failed!\n");
			SCE_DBG_PRINTF_LEVEL(1, 0xf, 903, "process id = 0x%08x\n", sceKernelGetProcessId());
			SCE_DBG_PRINTF_LEVEL(1, 0xf, 905, "sceSblACMgrIsSystemProgram() = %d\n", SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0));
			SCE_DBG_PRINTF_LEVEL(1, 0xf, 906, "cmd=%x\n", cmd);
			return SCE_ERROR_ERRNO_EUNSUP;
		}
	} else if (cmd & (IOFILEMGR_IOCTL_ACCESS_SYSTEM | IOFILEMGR_IOCTL_DISABLED)) {
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 911, "error : sceIoDevctlForUser() failed!\n");
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 912, "process id = 0x%08x\n", sceKernelGetProcessId());
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 914, "sceSblACMgrIsSystemProgram() = %d\n", SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0));
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 915, "cmd=%x\n", cmd);
		return SCE_ERROR_ERRNO_EUNSUP;
	}
	if (indata == NULL) {
		kernelIn = NULL;
	} else if (inlen > 0x400) {
		kernelIn = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, inlen);
		if (kernelIn == NULL)
			return SCE_ERROR_ERRNO_ENOMEM;
		ret = SceSysmemForDriver_BC996A7A(kernelIn, indata, inlen);
		if (ret < 0) {
			sceKernelFreeHeapMemory(g_ioVfsHeapUid, kernelIn);
			return ret;
		}
		inputHeap = 1;
	} else {
		kernelIn = __builtin_alloca(inlen);
		ret = SceSysmemForDriver_BC996A7A(kernelIn, indata, inlen);
		if (ret < 0)
			return ret;
	}
	if (outdata == NULL) {
		ret = ksceIoDevctl(dev, cmd, kernelIn, inlen, NULL, outlen);
		goto cleanup;
	}
	{
		if (outlen > 0x400) {
			kernelOut = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, outlen);
			if (kernelOut == NULL) {
				ret = SCE_ERROR_ERRNO_ENOMEM;
				goto cleanup;
			}
			memset(kernelOut, 0xff, outlen);
		} else {
			kernelOut = __builtin_alloca(outlen);
			memset(kernelOut, 0xff, outlen);
		}
	}
	ret = ksceIoDevctl(dev, cmd, kernelIn, inlen, kernelOut, outlen);
	if (ret >= 0) {
		int err = SceSysmemForDriver_6D88EF8A(outdata, kernelOut, outlen);

		if (err < 0)
			ret = err;
	}
cleanup:
	if (inputHeap) {
		memset(kernelIn, 0xff, inlen);
		sceKernelFreeHeapMemory(g_ioVfsHeapUid, kernelIn);
	}
	if (kernelOut != NULL && outlen > 0x400) {
		memset(kernelOut, 0xff, outlen);
		sceKernelFreeHeapMemory(g_ioVfsHeapUid, kernelOut);
	}
	return ret;
}

int sceIoIoctlForUser(SceUID fd, unsigned int cmd, const void *indata, SceSize inlen,
			void *outdata, SceSize outlen)
{
	void *kernelIn, *kernelOut = NULL;
	int inputHeap = 0, ret;
	SceUID guid;

	IO_CHECK_VM_DOMAIN(1009);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0)
		return ioPuidError(guid);
	if (cmd & IOFILEMGR_IOCTL_ACCESS_CHECK) {
		if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
			SCE_DBG_PRINTF_LEVEL(1, 0xf, 1022, "error : sceIoIoctlForUser() failed!\n");
			SCE_DBG_PRINTF_LEVEL(1, 0xf, 1023, "process id = 0x%08x\n", sceKernelGetProcessId());
			SCE_DBG_PRINTF_LEVEL(1, 0xf, 1025, "sceSblACMgrIsSystemProgram() = %d\n", SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0));
			SCE_DBG_PRINTF_LEVEL(1, 0xf, 1026, "cmd=%x\n", cmd);
			return SCE_ERROR_ERRNO_EUNSUP;
		}
	} else if (cmd & (IOFILEMGR_IOCTL_ACCESS_SYSTEM | IOFILEMGR_IOCTL_DISABLED)) {
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 1031, "error : sceIoIoctlForUser() failed!\n");
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 1032, "process id = 0x%08x\n", sceKernelGetProcessId());
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 1034, "sceSblACMgrIsSystemProgram() = %d\n", SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0));
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 1035, "cmd=%x\n", cmd);
		return SCE_ERROR_ERRNO_EUNSUP;
	}
	if (indata == NULL) {
		kernelIn = NULL;
	} else if (inlen <= 0x400) {
		kernelIn = __builtin_alloca(inlen);
		ret = SceSysmemForDriver_BC996A7A(kernelIn, indata, inlen);
		if (ret < 0)
			return ret;
	} else {
		kernelIn = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, inlen);
		if (kernelIn == NULL)
			return SCE_ERROR_ERRNO_ENOMEM;
		ret = SceSysmemForDriver_BC996A7A(kernelIn, indata, inlen);
		if (ret < 0) {
			sceKernelFreeHeapMemory(g_ioVfsHeapUid, kernelIn);
			return ret;
		}
		inputHeap = 1;
	}
	if (outdata == NULL) {
		ret = ksceIoIoctl(guid, cmd, kernelIn, inlen, NULL, outlen);
		goto cleanup;
	}
	{
		if (outlen <= 0x400) {
			kernelOut = __builtin_alloca(outlen);
			memset(kernelOut, 0xff, outlen);
		} else {
			kernelOut = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, outlen);
			if (kernelOut == NULL) {
				ret = SCE_ERROR_ERRNO_ENOMEM;
				goto cleanup;
			}
			memset(kernelOut, 0xff, outlen);
		}
		ret = SceSysmemForDriver_BC996A7A(kernelOut, outdata, outlen);
		if (ret < 0)
			goto cleanup;
	}
	ret = ksceIoIoctl(guid, cmd, kernelIn, inlen, kernelOut, outlen);
	if (ret >= 0) {
		int err = SceSysmemForDriver_6D88EF8A(outdata, kernelOut, outlen);

		if (err < 0)
			ret = err;
	}
cleanup:
	if (inputHeap) {
		memset(kernelIn, 0xff, inlen);
		sceKernelFreeHeapMemory(g_ioVfsHeapUid, kernelIn);
	}
	if (kernelOut != NULL && outlen > 0x400) {
		memset(kernelOut, 0xff, outlen);
		sceKernelFreeHeapMemory(g_ioVfsHeapUid, kernelOut);
	}
	return ret;
}

int sceIoGetstatByFdForUser(SceUID fd, SceIoStat *stat)
{
	SceUInt32 tls = sceSyscallEnter();
	SceUID guid;
	int handle, ret;

	IO_CHECK_VM_DOMAIN(1135);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0) {
		ret = ioPuidError(guid);
	} else {
		handle = SceIofilemgrForDriver_338DCD68(guid, &ret);
		if (handle == 0) {
			ret = ksceIoGetstatByFd(guid, stat);
		} else {
			ret = SceFios2KernelForDriver_2649408B(handle, stat);
			if (ret < 0)
				ret = ioFiosError(ret);
		}
	}
	sceSyscallLeave(tls);
	return ret;
}

int sceIoChstatByFdForUser(SceUID fd, const SceIoStat *stat, int bits)
{
	SceUInt32 tls = sceSyscallEnter();
	SceUID guid;
	int handle, ret;

	IO_CHECK_VM_DOMAIN(1169);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0) {
		ret = ioPuidError(guid);
	} else {
		handle = SceIofilemgrForDriver_338DCD68(guid, &ret);
		if (handle == 0) {
			ret = ksceIoChstatByFd(guid, stat, bits);
		} else {
			ret = SceFios2KernelForDriver_35E7E75C(handle, stat, bits);
			if (ret < 0)
				ret = ioFiosError(ret);
		}
	}
	sceSyscallLeave(tls);
	return ret;
}

int sceIoSyncByFdForUser(SceUID fd, unsigned int flag)
{
	SceUInt32 tls = sceSyscallEnter();
	SceUID guid;
	int handle, ret;

	IO_CHECK_VM_DOMAIN(1203);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0) {
		ret = ioPuidError(guid);
	} else {
		handle = SceIofilemgrForDriver_338DCD68(guid, &ret);
		if (handle == 0) {
			ret = ksceIoSyncByFd2(guid, flag);
		} else {
			ret = SceFios2KernelForDriver_990F46A5(handle, flag);
			if (ret < 0)
				ret = ioFiosError(ret);
		}
	}
	sceSyscallLeave(tls);
	return ret;
}

SceUID sceIoOpenAsyncForUser(const char *file, int flags, SceMode mode, void *asyncParam)
{
	char *resolved;
	SceUID pid, fd;
	int resolveMode;

	IO_CHECK_VM_DOMAIN(1241);
	if (g_ioFiosPathEnabled != 0) {
		resolveMode = flags & 0x602;
		pid = sceKernelGetProcessId();
		if (resolveMode != 0)
			resolveMode = IO_FIOS_RESOLVE_WRITE;
		resolved = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, IO_FIOS_PATH_SIZE);
		if (resolved == NULL)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		fd = ksceFiosKernelOverlayResolveSync(pid, resolveMode, file, resolved,
						      IO_FIOS_PATH_SIZE);
		if (fd < 0) {
			fd = ioFiosError(fd);
		} else {
			fd = ksceIoOpenAsync(resolved, flags, mode, asyncParam);
			if (fd > 0)
				fd = SceSysmemForDriver_BF209859(0, fd);
		}
		sceKernelFreeHeapMemory(g_ioVfsHeapUid, resolved);
		return fd;
	} else {
		fd = ksceIoOpenAsync(file, flags, mode, asyncParam);
		if (fd > 0)
			fd = SceSysmemForDriver_BF209859(0, fd);
	}
	return fd;
}

SceUID sceIoCloseAsyncForUser(SceUID fd, void *asyncParam)
{
	SceUID ret;

	IO_CHECK_VM_DOMAIN(1280);
	ret = ksceIoCloseAsync(fd, asyncParam);
	if (ret > 0)
		ret = SceSysmemForDriver_BF209859(0, ret);
	return ret;
}

SceUID sceIoReadAsyncForUser(SceUID fd, void *data, SceSize size, void *asyncParam)
{
	SceUID guid, ret;

	IO_CHECK_VM_DOMAIN(1299);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0)
		return ioPuidError(guid);
	ret = ksceIoReadAsync(guid, data, size, asyncParam);
	if (ret > 0)
		ret = SceSysmemForDriver_BF209859(0, ret);
	return ret;
}

SceUID sceIoWriteAsyncForUser(SceUID fd, const void *data, SceSize size, void *asyncParam)
{
	SceUID guid, ret;

	IO_CHECK_VM_DOMAIN(1326);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0)
		return ioPuidError(guid);
	ret = ksceIoWriteAsync(guid, data, size, asyncParam);
	if (ret > 0)
		ret = SceSysmemForDriver_BF209859(0, ret);
	return ret;
}

SceUID sceIoLseekAsyncForUser(SceUID fd, SceOff offset, int whence, void *asyncParam)
{
	SceUID guid, ret;

	IO_CHECK_VM_DOMAIN(1353);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0)
		return ioPuidError(guid);
	ret = ksceIoLseekAsync(guid, offset, whence, asyncParam);
	if (ret > 0)
		ret = SceSysmemForDriver_BF209859(0, ret);
	return ret;
}

SceUID sceIoIoctlAsyncForUser(SceUID fd, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen, void *asyncParam)
{
	SceUID guid, ret;

	IO_CHECK_VM_DOMAIN(1382);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0)
		return ioPuidError(guid);
	ret = ksceIoIoctlAsync(guid, cmd, indata, inlen, outdata, outlen, asyncParam);
	if (ret > 0)
		ret = SceSysmemForDriver_BF209859(0, ret);
	return ret;
}

SceUID sceIoRemoveAsyncForUser(const char *file, void *asyncParam)
{
	char *resolved;
	SceUID pid, fd;

	IO_CHECK_VM_DOMAIN(1406);
	if (g_ioFiosPathEnabled != 0) {
		pid = sceKernelGetProcessId();
		resolved = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, IO_FIOS_PATH_SIZE);
		if (resolved == NULL)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		fd = ksceFiosKernelOverlayResolveSync(pid, IO_FIOS_RESOLVE_WRITE, file, resolved,
						      IO_FIOS_PATH_SIZE);
		if (fd < 0) {
			fd = ioFiosError(fd);
		} else {
			fd = ksceIoRemoveAsync(resolved, asyncParam);
			if (fd > 0)
				fd = SceSysmemForDriver_BF209859(0, fd);
		}
		sceKernelFreeHeapMemory(g_ioVfsHeapUid, resolved);
		return fd;
	} else {
		fd = ksceIoRemoveAsync(file, asyncParam);
		if (fd > 0)
			fd = SceSysmemForDriver_BF209859(0, fd);
	}
	return fd;
}

SceUID sceIoDopenAsyncForUser(const char *dirname, void *asyncParam)
{
	IoAsyncEvent *event = NULL;
	SceUInt32 tls;
	SceUID uid;
	char *path;
	int ret;

	IO_CHECK_VM_DOMAIN(1443);
	tls = sceSyscallEnter();
	uid = sceIoAsyncEventCreate(&event);
	if (uid < 0) {
		ret = uid;
		goto out;
	}
	path = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, IO_FIOS_PATH_SIZE);
	if (path == NULL) {
		ret = SCE_KERNEL_ERROR_NO_MEMORY;
		goto destroy;
	}
	ret = SceSysmemForDriver_DB3EC244(path, dirname, IO_FIOS_PATH_SIZE);
	if (ret < 0)
		goto freePath;
	if (ret == IO_FIOS_PATH_SIZE && path[IO_FIOS_PATH_SIZE - 1] != 0) {
		ret = SCE_KERNEL_ERROR_NO_MEMORY;
		goto freePath;
	}
	ret = sceIoRequestCreateIoRequestForDopenAsync(&event->request,
						    getThreadIoPriority(), uid,
						    asyncParam, path);
	if (ret < 0)
		goto freePath;
	sceIoSchedulerAddAsyncIoRequest(g_schedDispatcher.other, &event->request);
	ret = SceSysmemForDriver_BF209859(0, uid);
	sceSyscallLeave(tls);
	return ret;
freePath:
	sceKernelFreeHeapMemory(g_ioVfsHeapUid, path);
destroy:
	sceIoAsyncEventDestroy(uid);
out:
	sceSyscallLeave(tls);
	return ret;
}

SceUID sceIoDcloseAsyncForUser(SceUID fd, void *asyncParam)
{
	IoAsyncEvent *event = NULL;
	SceUInt32 tls;
	SceUID uid;
	int ret;

	IO_CHECK_VM_DOMAIN(1507);
	tls = sceSyscallEnter();
	uid = sceIoAsyncEventCreate(&event);
	if (uid < 0) {
		ret = uid;
		goto out;
	}
	ret = sceIoRequestCreateIoRequestForDcloseAsync(&event->request,
						     getThreadIoPriority(), uid,
						     asyncParam, fd);
	if (ret < 0) {
		sceIoAsyncEventDestroy(uid);
	} else {
		sceIoSchedulerAddAsyncIoRequest(g_schedDispatcher.other, &event->request);
		ret = SceSysmemForDriver_BF209859(0, uid);
	}
out:
	sceSyscallLeave(tls);
	return ret;
}

SceUID sceIoDreadAsyncForUser(SceUID fd, SceIoDirent *dir, void *asyncParam)
{
	IoAsyncEvent *event = NULL;
	SceUInt32 tls;
	SceUID uid;
	int ret;

	IO_CHECK_VM_DOMAIN(1550);
	tls = sceSyscallEnter();
	uid = sceIoAsyncEventCreate(&event);
	if (uid < 0) {
		ret = uid;
		goto out;
	}
	ret = sceIoRequestCreateIoRequestForDreadAsync(&event->request,
						     getThreadIoPriority(), uid,
						     asyncParam, fd, dir);
	if (ret < 0) {
		sceIoAsyncEventDestroy(uid);
	} else {
		sceIoSchedulerAddAsyncIoRequest(g_schedDispatcher.other, &event->request);
		ret = SceSysmemForDriver_BF209859(0, uid);
	}
out:
	sceSyscallLeave(tls);
	return ret;
}

SceUID sceIoMkdirAsyncForUser(const char *file, SceMode mode, void *asyncParam)
{
	char *resolved;
	SceUID pid, fd;

	IO_CHECK_VM_DOMAIN(1594);
	if (g_ioFiosPathEnabled != 0) {
		pid = sceKernelGetProcessId();
		resolved = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, IO_FIOS_PATH_SIZE);
		if (resolved == NULL)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		fd = ksceFiosKernelOverlayResolveSync(pid, IO_FIOS_RESOLVE_WRITE, file, resolved,
						      IO_FIOS_PATH_SIZE);
		if (fd < 0) {
			fd = ioFiosError(fd);
		} else {
			fd = ksceIoMkdirAsync(resolved, mode, asyncParam);
			if (fd > 0)
				fd = SceSysmemForDriver_BF209859(0, fd);
		}
		sceKernelFreeHeapMemory(g_ioVfsHeapUid, resolved);
		return fd;
	} else {
		fd = ksceIoMkdirAsync(file, mode, asyncParam);
		if (fd > 0)
			fd = SceSysmemForDriver_BF209859(0, fd);
	}
	return fd;
}

SceUID sceIoRmdirAsyncForUser(const char *file, void *asyncParam)
{
	char *resolved;
	SceUID pid, fd;

	IO_CHECK_VM_DOMAIN(1629);
	if (g_ioFiosPathEnabled != 0) {
		pid = sceKernelGetProcessId();
		resolved = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, IO_FIOS_PATH_SIZE);
		if (resolved == NULL)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		fd = ksceFiosKernelOverlayResolveSync(pid, IO_FIOS_RESOLVE_WRITE, file, resolved,
						      IO_FIOS_PATH_SIZE);
		if (fd < 0) {
			fd = ioFiosError(fd);
		} else {
			fd = ksceIoRmdirAsync(resolved, asyncParam);
			if (fd > 0)
				fd = SceSysmemForDriver_BF209859(0, fd);
		}
		sceKernelFreeHeapMemory(g_ioVfsHeapUid, resolved);
		return fd;
	} else {
		fd = ksceIoRmdirAsync(file, asyncParam);
		if (fd > 0)
			fd = SceSysmemForDriver_BF209859(0, fd);
	}
	return fd;
}

SceUID sceIoRenameAsyncForUser(const char *oldname, const char *newname, void *asyncParam)
{
	char *resolvedOld, *resolvedNew;
	SceUID pid;
	int ret;

	IO_CHECK_VM_DOMAIN(1663);
	if (g_ioFiosPathEnabled != 0) {
		pid = sceKernelGetProcessId();
		resolvedOld = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, IO_FIOS_PATH_SIZE);
		if (resolvedOld == NULL)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		ret = ksceFiosKernelOverlayResolveSync(pid, IO_FIOS_RESOLVE_WRITE, oldname, resolvedOld,
						       IO_FIOS_PATH_SIZE);
		if (ret < 0) {
			ret = ioFiosError(ret);
		} else {
			resolvedNew = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, IO_FIOS_PATH_SIZE);
			if (resolvedNew == NULL) {
				sceKernelFreeHeapMemory(g_ioVfsHeapUid, resolvedOld);
				return SCE_KERNEL_ERROR_NO_MEMORY;
			}
			ret = ksceFiosKernelOverlayResolveSync(pid, IO_FIOS_RESOLVE_WRITE, newname,
							       resolvedNew, IO_FIOS_PATH_SIZE);
			if (ret < 0)
				ret = ioFiosError(ret);
			else {
				ret = ksceIoRenameAsync(resolvedOld, resolvedNew, asyncParam);
				if (ret > 0)
					ret = SceSysmemForDriver_BF209859(0, ret);
			}
			sceKernelFreeHeapMemory(g_ioVfsHeapUid, resolvedNew);
		}
		sceKernelFreeHeapMemory(g_ioVfsHeapUid, resolvedOld);
		return ret;
	} else {
		ret = ksceIoRenameAsync(oldname, newname, asyncParam);
		if (ret > 0)
			ret = SceSysmemForDriver_BF209859(0, ret);
	}
	return ret;
}

SceUID sceIoChstatAsyncForUser(const char *file, SceIoStat *stat, int bits,
			     void *asyncParam)
{
	char *resolved;
	SceIoStat *kernelStat;
	SceUID pid;
	int ret, copied;

	IO_CHECK_VM_DOMAIN(1712);
	if (g_ioFiosPathEnabled != 0) {
		pid = sceKernelGetProcessId();
		resolved = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, IO_FIOS_PATH_SIZE);
		if (resolved == NULL)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		ret = ksceFiosKernelOverlayResolveSync(pid, IO_FIOS_RESOLVE_WRITE, file, resolved,
						       IO_FIOS_PATH_SIZE);
		if (ret < 0) {
			ret = ioFiosError(ret);
			goto freePath;
		}
		kernelStat = SceSysmemForDriver_7B4CB60A(g_ioSchedHeap, sizeof(*kernelStat));
		if (kernelStat == NULL) {
			sceKernelFreeHeapMemory(g_ioVfsHeapUid, resolved);
			return SCE_KERNEL_ERROR_NO_MEMORY;
		}
		copied = SceSysmemForDriver_BC996A7A(kernelStat, stat, sizeof(*kernelStat));
		if (copied < 0) {
			sceKernelFreeHeapMemory(g_ioSchedHeap, kernelStat);
			sceKernelFreeHeapMemory(g_ioVfsHeapUid, resolved);
			return copied;
		}
		ret = ksceIoChstatAsync(resolved, kernelStat, bits, asyncParam);
		if (ret > 0)
			ret = SceSysmemForDriver_BF209859(0, ret);
freePath:
		sceKernelFreeHeapMemory(g_ioVfsHeapUid, resolved);
		return ret;
	}
	ret = ksceIoChstatAsync(file, stat, bits, asyncParam);
	if (ret > 0)
		ret = SceSysmemForDriver_BF209859(0, ret);
	return ret;
}

SceUID sceIoGetstatAsyncForUser(const char *file, SceIoStat *stat, void *asyncParam)
{
	char *resolved;
	SceUID pid, fd;

	IO_CHECK_VM_DOMAIN(1761);
	if (g_ioFiosPathEnabled != 0) {
		pid = sceKernelGetProcessId();
		resolved = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, IO_FIOS_PATH_SIZE);
		if (resolved == NULL)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		fd = ksceFiosKernelOverlayResolveSync(pid, IO_FIOS_RESOLVE_READ, file, resolved,
						      IO_FIOS_PATH_SIZE);
		if (fd < 0) {
			fd = ioFiosError(fd);
		} else {
			fd = ksceIoGetstatAsync(resolved, stat, asyncParam);
			if (fd > 0)
				fd = SceSysmemForDriver_BF209859(0, fd);
		}
		sceKernelFreeHeapMemory(g_ioVfsHeapUid, resolved);
		return fd;
	} else {
		fd = ksceIoGetstatAsync(file, stat, asyncParam);
		if (fd > 0)
			fd = SceSysmemForDriver_BF209859(0, fd);
	}
	return fd;
}

SceUID sceIoDevctlAsyncForUser(const char *dev, unsigned int cmd, void *indata, int inlen,
			     void *outdata, int outlen, void *asyncParam)
{
	SceUID ret;

	IO_CHECK_VM_DOMAIN(1800);
	ret = ksceIoDevctlAsync(dev, cmd, indata, inlen, outdata, outlen, asyncParam);
	if (ret > 0)
		ret = SceSysmemForDriver_BF209859(0, ret);
	return ret;
}

SceUID sceIoSyncAsyncForUser(const char *device, unsigned int flag, void *asyncParam)
{
	SceUID ret;

	IO_CHECK_VM_DOMAIN(1822);
	ret = ksceIoSyncAsync(device, flag, asyncParam);
	if (ret > 0)
		ret = SceSysmemForDriver_BF209859(0, ret);
	return ret;
}

int sceIoCancelForUser(SceUID uid)
{
	int ret;

	IO_CHECK_VM_DOMAIN(1834);
	ret = ksceIoCancel(SceSysmemForDriver_45D22597(0, uid));
	if (ret == 0)
		SceSysmemForDriver_84A4AF5E(0, uid);
	return ret;
}

int sceIoCompleteForUser(SceUID uid)
{
	int ret;

	IO_CHECK_VM_DOMAIN(1876);
	ret = sceIoComplete(SceSysmemForDriver_45D22597(0, uid));
	if (ret == 0)
		SceSysmemForDriver_84A4AF5E(0, uid);
	return ret;
}

int sceIoCompleteMultipleForUser(void *params, int num, int *numCompleted)
{
	IofilemgrKpls *kpls;
	IoAsyncParam *kernelParams;
	SceUID pid;
	int limit, ret, i;

	IO_CHECK_VM_DOMAIN(1902);
	pid = sceKernelGetProcessId();
	ret = SceProcessmgrForDriver_AF80F39C(pid, g_iofilemgrKplsKey,
						       (void **)&kpls, 1);
	if (ret < 0)
		return ret;
	limit = kpls->fdMax;
	if (limit < 0x5c)
		limit = 0x5c;
	if ((unsigned int)num > (unsigned int)limit)
		return SCE_ERROR_ERRNO_EINVAL;
	kernelParams = SceSysmemForDriver_7B4CB60A(g_ioVfsHeapUid, num * sizeof(*kernelParams));
	if (kernelParams == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = sceKernelCopyFromUserProc(pid, kernelParams, params, num * sizeof(*kernelParams));
	if (ret < 0) {
		sceKernelFreeHeapMemory(g_ioVfsHeapUid, kernelParams);
		return ret;
	}
	for (i = 0; i < num; i++) {
		SceUID uid = kernelParams[i].event;

		ret = sceIoComplete(SceSysmemForDriver_45D22597(0, uid));
		if (ret < 0)
			break;
		SceSysmemForDriver_84A4AF5E(0, uid);
	}
	*numCompleted = i;
	sceKernelFreeHeapMemory(g_ioVfsHeapUid, kernelParams);
	return ret;
}

SceSSize sceIoPreadForUser(SceUID fd, void *data, SceSize size, SceOff offset)
{
	void *kernelPage;
	SceSize kernelSize;
	SceUInt32 kernelOffset;
	char *kernelData;
	SceUID guid, mapUid;
	SceSSize ret, done;

	IO_CHECK_VM_DOMAIN(1976);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0)
		return ioPuidError(guid);
	kernelData = NULL;
	if (size != 0) {
		mapUid = SceSysmemForDriver_7D4F8B5F("SceIoUserMapPRead", IO_USER_MAP_WRITE,
						    SceSblACMgrForKernel_sceSblACMgrIsGameProgram(0) == 1 ?
						    IO_USER_MAP_FLAGS_GAME : IO_USER_MAP_FLAGS,
						    data, size, &kernelPage, &kernelSize,
						    &kernelOffset);
		if (mapUid < 0) {
			if (mapUid == SCE_KERNEL_ERROR_VA2PA_FAULT)
				return SCE_ERROR_ERRNO_EFAULT;
			return mapUid;
		}
		kernelData = (char *)kernelPage + kernelOffset;
	}
	ret = ksceIoPread(guid, kernelData, size, offset);
	done = ret < 0 ? 0 : ret;
	if (kernelData != NULL)
		ioUserUnmap(mapUid, data, size);
	return ret < 0 ? ret : done;
}

SceSSize sceIoPwriteForUser(SceUID fd, const void *data, SceSize size, SceOff offset)
{
	void *kernelPage;
	SceSize kernelSize;
	SceUInt32 kernelOffset;
	char *kernelData;
	SceUID guid, mapUid;
	SceSSize ret, done;

	IO_CHECK_VM_DOMAIN(2036);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0)
		return ioPuidError(guid);
	kernelData = NULL;
	if (size != 0) {
		mapUid = SceSysmemForDriver_7D4F8B5F("SceIoUserMapPwrite", IO_USER_MAP_READ,
						    SceSblACMgrForKernel_sceSblACMgrIsGameProgram(0) == 1 ?
						    IO_USER_MAP_FLAGS_GAME : IO_USER_MAP_FLAGS,
						    data, size, &kernelPage, &kernelSize,
						    &kernelOffset);
		if (mapUid < 0) {
			if (mapUid == SCE_KERNEL_ERROR_VA2PA_FAULT)
				return SCE_ERROR_ERRNO_EFAULT;
			return mapUid;
		}
		kernelData = (char *)kernelPage + kernelOffset;
	}
	ret = ksceIoPwrite(guid, kernelData, size, offset);
	done = ret < 0 ? 0 : ret;
	if (kernelData != NULL)
		ioUserUnmap(mapUid, data, size);
	return ret < 0 ? ret : done;
}

SceUID sceIoPreadAsyncForUser(SceUID fd, void *data, SceSize size, SceOff offset, void *asyncParam)
{
	SceUID guid, ret;

	IO_CHECK_VM_DOMAIN(2211);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0)
		return ioPuidError(guid);
	ret = ksceIoPreadAsync(guid, data, size, offset, asyncParam);
	if (ret > 0)
		ret = SceSysmemForDriver_BF209859(0, ret);
	return ret;
}

SceUID sceIoPwriteAsyncForUser(SceUID fd, const void *data, SceSize size, SceOff offset, void *asyncParam)
{
	SceUID guid, ret;

	IO_CHECK_VM_DOMAIN(2239);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0)
		return ioPuidError(guid);
	ret = ksceIoPwriteAsync(guid, data, size, offset, asyncParam);
	if (ret > 0)
		ret = SceSysmemForDriver_BF209859(0, ret);
	return ret;
}

SceUID sceIoChstatByFdAsyncForUser(SceUID fd, const SceIoStat *stat, int bits,
				 void *asyncParam)
{
	SceIoStat *kernelStat;
	SceUID guid;
	int ret;

	IO_CHECK_VM_DOMAIN(2265);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0)
		return ioPuidError(guid);
	kernelStat = SceSysmemForDriver_7B4CB60A(g_ioSchedHeap, sizeof(*kernelStat));
	if (kernelStat == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = SceSysmemForDriver_BC996A7A(kernelStat, stat, sizeof(*kernelStat));
	if (ret < 0) {
		sceKernelFreeHeapMemory(g_ioSchedHeap, kernelStat);
		return ret;
	}
	ret = ksceIoChstatByFdAsync(guid, kernelStat, bits, asyncParam);
	if (ret < 0)
		sceKernelFreeHeapMemory(g_ioSchedHeap, kernelStat);
	else if (ret > 0)
		ret = SceSysmemForDriver_BF209859(0, ret);
	return ret;
}

SceUID sceIoGetstatByFdAsyncForUser(SceUID fd, SceIoStat *stat, void *asyncParam)
{
	SceUID guid, ret;

	IO_CHECK_VM_DOMAIN(2305);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0)
		return ioPuidError(guid);
	ret = ksceIoGetstatByFdAsync(guid, stat, asyncParam);
	if (ret > 0)
		ret = SceSysmemForDriver_BF209859(0, ret);
	return ret;
}

SceUID sceIoSyncByFdAsyncForUser(SceUID fd, int flag, void *asyncParam)
{
	SceUID guid, ret;

	IO_CHECK_VM_DOMAIN(2329);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0)
		return ioPuidError(guid);
	ret = ksceIoSyncByFdAsync(guid, flag, asyncParam);
	if (ret > 0)
		ret = SceSysmemForDriver_BF209859(0, ret);
	return ret;
}

int sceIoSetProcessDefaultPriorityForUser(int priority)
{
	IO_CHECK_VM_DOMAIN(2347);
	return sceIoSetProcessDefaultPriorityInternal(priority, 0);
}

int sceIoGetProcessDefaultPriorityForUser(void)
{
	IO_CHECK_VM_DOMAIN(2354);
	return sceIoGetProcessDefaultPriorityInternal(0);
}

int sceIoSetThreadDefaultPriorityForUser(int priority)
{
	IO_CHECK_VM_DOMAIN(2360);
	return sceIoSetThreadDefaultPriorityInternal(priority, 0);
}

int sceIoGetThreadDefaultPriorityForUser(void)
{
	IO_CHECK_VM_DOMAIN(2367);
	return sceIoGetThreadDefaultPriorityInternal(0);
}

int sceIoSetPriorityForUser(SceUID fd, int priority)
{
	SceUID guid;

	IO_CHECK_VM_DOMAIN(2373);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0)
		return ioPuidError(guid);
	return sceIoSetPriorityInternal(guid, priority, 0);
}

int sceIoGetPriorityForUser(SceUID fd)
{
	SceUID guid;

	IO_CHECK_VM_DOMAIN(2388);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0)
		return ioPuidError(guid);
	return sceIoGetPriorityInternal(guid, 0);
}

int sceIoSetThreadDefaultPriorityForSystemForUser(int priority)
{
	IO_CHECK_VM_DOMAIN(2416);
	return sceIoSetThreadDefaultPriorityInternal(priority, 1);
}

int sceIoGetThreadDefaultPriorityForSystemForUser(void)
{
	IO_CHECK_VM_DOMAIN(2423);
	return sceIoGetThreadDefaultPriorityInternal(1);
}

int sceIoSetPriorityForSystemForUser(SceUID fd, int priority)
{
	SceUID guid;

	IO_CHECK_VM_DOMAIN(2429);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0)
		return ioPuidError(guid);
	return sceIoSetPriorityInternal(guid, priority, 1);
}

int sceIoGetPriorityForSystemForUser(SceUID fd)
{
	SceUID guid;

	IO_CHECK_VM_DOMAIN(2444);
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0)
		return ioPuidError(guid);
	return sceIoGetPriorityInternal(guid, 1);
}

int sceIoFlockForSystemForUser(SceUID fd, int operation)
{
	SceUInt32 tls;
	SceUID guid;
	int ret;

	IO_CHECK_VM_DOMAIN(2463);
	tls = sceSyscallEnter();
	guid = SceSysmemForDriver_45D22597(0, fd);
	if (guid < 0)
		ret = guid | IO_ERROR_CRITICAL;
	else
		ret = ksceIoFlock(guid, operation);
	sceSyscallLeave(tls);
	return ret;
}
