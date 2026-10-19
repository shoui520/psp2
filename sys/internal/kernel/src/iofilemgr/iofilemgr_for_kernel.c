/*
 * SceIofilemgr: the kernel calls made on behalf of a process (Sony's
 * iofilemgr_for_kernel.c), with the Fios2 overlay paths that firmware builds
 * leave switched off (g_ioFiosOpenEnabled, ...).
 */

#include <intrmgr/syscall.h>
#include "iofilemgr_internal.h"

int g_ioFiosOpenEnabled __attribute__((section(".data"))) = 0;
int g_ioFiosDopenEnabled __attribute__((section(".data"))) = 0;
int g_ioFiosDcloseEnabled __attribute__((section(".data"))) = 0;

/* the flags that may create or change the file: the overlay resolves them for writing */
#define IO_OPEN_WRITE_FLAGS	(SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC)

SceUID SceIofilemgrForDriver_C3D34965(SceUID pid, const char *path, int flags, SceMode mode)
{
	char *resolved;
	int forWrite;
	SceUID fd;
	int ret;

	if (g_ioFiosOpenEnabled == 0)
		return sceIoOpen(path, flags, mode);

	forWrite = (flags & IO_OPEN_WRITE_FLAGS) != 0;
	resolved = sceKernelAllocHeapMemory(g_ioVfsHeapUid, 0x400);
	if (resolved == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;

	ret = SceFios2KernelForDriver_0F456345(pid, forWrite, path, resolved, 0x400);
	if (ret < 0)
		goto error;
	fd = sceIoOpen(resolved, flags, mode);
	sceKernelFreeHeapMemory(g_ioVfsHeapUid, resolved);
	return fd;

error:
	sceKernelFreeHeapMemory(g_ioVfsHeapUid, resolved);
	return convertFiosError(ret);
}

/* a path's media type, through the process's path mapping; unknown media is 8 */
int SceIofilemgrForDriver_9C220246(SceUID pid, const char *path, int unused, SceUInt32 *type)
{
	IoPathMappingFunction map;
	char *mapped;
	int ret;

	if (g_ioPathMappingFunction == NULL || pid == SCE_KERNEL_PROCESS_ID) {
		ret = SceSblACMgrForKernel_sceSblACMgrGetMediaType(path, type);
	} else {
		mapped = sceKernelAllocHeapMemory(g_ioVfsHeapUid, 0x400);
		if (mapped == NULL)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		map = g_ioPathMappingFunction;
		mapped[0] = '\0';
		ret = map(path, pid, mapped, 0x400);
		if (ret < 0) {
			sceKernelFreeHeapMemory(g_ioVfsHeapUid, mapped);
			return ret;
		}
		ret = SceSblACMgrForKernel_sceSblACMgrGetMediaType(mapped, type);
		sceKernelFreeHeapMemory(g_ioVfsHeapUid, mapped);
	}
	if (ret < 0) {
		*type = 8;
		return 0;
	}
	return ret;
}

SceUID sceIoDopenInternal(const char *dirname)
{
	SceUID thid = sceKernelGetThreadId();
	SceUID pid = SceThreadmgrForDriver_FA54D49A();
	SceUInt32 syscall = sceSyscallEnter();
	IofilemgrSchedInfo param;
	SceInt32 priority, old;
	int handle;
	SceUID fd;
	int ret, res;

	if (g_ioFiosDopenEnabled == 0)
		goto plain;

	priority = getThreadIoPriority();
	old = swapThreadIoPriority(priority);
	if (old == SCE_ERROR_ERRNO_EINVAL) {
		fd = old;
		goto out;
	}

	ret = SceFios2KernelForDriver_3B329E86(&handle, dirname, 0, 0xff);
	if (ret < 0) {
		fd = convertFiosError(ret);
		if (fd != SCE_ERROR_ERRNO_ENODEV) {
			ret = check_limit_user_fd_count(pid, thid);
			if (ret < 0)
				fd = ret;
		}
		goto restore;
	}
	fd = check_limit_and_increment_user_fd_count(pid, thid);
	if (fd < 0)
		goto close;
	fd = vfsAllocateFileDummy(1, 1, dirname);
	if (fd < 0)
		goto decrement;
	ret = SceIofilemgrForDriver_6BEEDDB4(fd, handle);
	if (ret < 0) {
		/* Sony's code frees the dummy file by the error, not the fd */
		SceIofilemgrForDriver_3A79FAC9(ret);
		fd = ret;
		goto decrement;
	}
	if (sceKernelCheckDipsw(SCE_DIPSW_ENABLE_TOOL_PHYMEMPART))
		SceIofilemgrForDriver_2EFDFA12(fd, dirname);

	param.priority = priority;
	param.mntOpt = 0;
	param.pid = pid;
	setFileSchedParam(fd, &param);
	res = setThreadIoPriority(old);
	SCE_DBG_ASSERT(res == 0, 270);
	sceSyscallLeave(syscall);
	return fd;

decrement:
	decrement_user_fd_count(pid, thid, 0);
close:
	SceFios2KernelForDriver_DD7627EC(handle);
restore:
	res = setThreadIoPriority(old);
	SCE_DBG_ASSERT(res == 0, 288);
out:
	sceSyscallLeave(syscall);
	return fd;

plain:
	fd = check_limit_and_increment_user_fd_count(pid, thid);
	if (fd >= 0) {
		fd = sceIoDopen(dirname);
		if (fd < 0)
			decrement_user_fd_count(pid, thid, 0);
	}
	goto out;
}

int sceIoDreadInternal(SceUID fd, SceIoDirent *dir)
{
	IofilemgrSchedInfo param;
	SceInt32 old;
	int handle;
	int ret, res;

	handle = SceIofilemgrForDriver_338DCD68(fd, &ret);
	if (handle == 0)
		return sceIoDread(fd, dir);

	ret = getFileSchedParam(fd, &param, 0);
	if (ret < 0)
		return ret;
	old = swapThreadIoPriority(param.priority);
	if (old == SCE_ERROR_ERRNO_EINVAL)
		return ret;

	ret = SceFios2KernelForDriver_28E28A58(handle, dir);
	if (ret == 0)
		ret = 1;
	else if (ret == (int)0x80820010)	/* no more entries */
		ret = 0;
	else
		ret = convertFiosError(ret);
	res = setThreadIoPriority(old);
	SCE_DBG_ASSERT(res == 0, 338);
	return ret;
}

int sceIoDcloseInternal(SceUID fd)
{
	SceUID pid;
	int handle;
	int ret;

	if (g_ioFiosDcloseEnabled != 0) {
		handle = SceIofilemgrForDriver_338DCD68(fd, &ret);
		if (handle != 0) {
			ret = SceFios2KernelForDriver_DD7627EC(handle);
			if (ret != 0) {
				ret = convertFiosError(ret);
			} else {
				ret = SceIofilemgrForDriver_3A79FAC9(fd);
			}
		} else {
			ret = sceIoDclose(fd);
		}
	} else {
		ret = sceIoDclose(fd);
	}
	if (ret == 0) {
		pid = SceThreadmgrForDriver_FA54D49A();
		decrement_user_fd_count(pid, sceKernelGetThreadId(), 1);
	}
	return ret;
}
