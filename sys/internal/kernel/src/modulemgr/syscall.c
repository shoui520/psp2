/*
 * syscall.c - the SceModulemgr system calls (user mode module loading and
 * module queries); Sony's file name is unknown
 */

#include "modulemgr_internal.h"

#define SYSCALL_START_FLAGS		0x08000000	/* entry points get the user arguments */
#define SYSCALL_ERROR_PUID		0x40000000	/* marks a failed PUID lookup */

/* name unknown: a user path copied into the kernel */
int ModulemgrCopyPathFromUser(const char *upath, char **path)
{
	char *buf;
	int len, ret;

	len = SceSysmemForDriver_B429D419(upath, 0x400);
	if (len < 0)
		return len;
	if (len >= 0x400)
		return SCE_KERNEL_ERROR_MODULEMGR_NAMETOOLONG;
	buf = ModulemgrAlloc(0, len + 1);
	if (buf == NULL)
		return SCE_KERNEL_ERROR_MODULEMGR_NOMEM;
	buf[len] = '\0';
	if (len != 0) {
		ret = SceSysmemForDriver_BC996A7A(buf, upath, len);
		if (ret < 0) {
			ModulemgrFree(0, buf);
			return ret;
		}
	}
	*path = buf;
	return 0;
}

/* name unknown */
int ModulemgrLoadModuleForUser(const char *upath, SceUInt32 flags,
				      const SceKernelLMOption *opt, SceUID *modid, SceUID *puid)
{
	SceModuleObject *obj;
	char *path;
	SceUID uid;
	int ret;

	if ((flags & ~0xf0) != 0 || (flags & ~0x10) != 0)
		return SCE_KERNEL_ERROR_INVALID_FLAGS;
	ret = ModulemgrCopyPathFromUser(upath, &path);
	if (ret < 0)
		return ret;
	uid = loadModuleCommon(0, path, flags | LOAD_FLAG_1000, opt);
	ModulemgrFree(0, path);
	if (uid < 0)
		return uid;
	obj = ModulemgrReferModule(uid);
	if (obj == NULL)
		return SCE_KERNEL_ERROR_ERROR;
	if (modid != NULL)
		*modid = uid;
	if (puid != NULL)
		*puid = obj->cb.puid;
	ModulemgrReleaseModule(uid);
	return 0;
}

/* name unknown */
SceUID sceKernelLoadModuleForUser(const char *path, SceUInt32 flags,
					 const SceKernelLMOption *opt)
{
	SceUID puid = 0;
	SceUInt32 state;
	int ret;

	state = ModulemgrSyscallEnter();
	ret = ModulemgrLoadModuleForUser(path, flags, opt, NULL, &puid);
	ModulemgrSyscallLeave(state);
	if (ret >= 0)
		ret = puid;
	return ret;
}

/* name unknown */
int sceKernelStartModuleForUser(SceUID modid, SceSize args, void *argp, SceUInt32 flags,
				       const SceKernelLMOption *opt, int *status)
{
	SceUInt32 state;
	int ret;

	state = ModulemgrSyscallEnter();
	ret = SceSysmemForDriver_45D22597(0, modid);
	if (ret < 0) {
		ret |= SYSCALL_ERROR_PUID;
	} else if (flags != 0) {
		ret = SCE_KERNEL_ERROR_INVALID_FLAGS;
	} else if (status == NULL) {
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	} else {
		ret = startModuleCommon(0, ret, args, argp, SYSCALL_START_FLAGS, opt, status);
	}
	ModulemgrSyscallLeave(state);
	return ret;
}

/* name unknown */
int sceKernelStopModuleForUser(SceUID modid, SceSize args, void *argp, SceUInt32 flags,
				      const SceKernelLMOption *opt, int *status)
{
	SceUInt32 state;
	int ret;

	state = ModulemgrSyscallEnter();
	ret = SceSysmemForDriver_45D22597(0, modid);
	if (ret < 0) {
		ret |= SYSCALL_ERROR_PUID;
	} else if (flags != 0) {
		ret = SCE_KERNEL_ERROR_INVALID_FLAGS;
	} else if (status == NULL) {
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	} else {
		ret = stopModuleCommon(0, ret, args, argp, SYSCALL_START_FLAGS, opt, status);
	}
	ModulemgrSyscallLeave(state);
	return ret;
}

/* name unknown */
int sceKernelUnloadModuleForUser(SceUID modid, SceUInt32 flags,
					const SceKernelULMOption *opt)
{
	SceUInt32 state;
	int ret;

	state = ModulemgrSyscallEnter();
	ret = SceSysmemForDriver_45D22597(0, modid);
	if (ret < 0)
		ret |= SYSCALL_ERROR_PUID;
	else if (flags != 0)
		ret = SCE_KERNEL_ERROR_INVALID_FLAGS;
	else
		ret = unloadModuleCommon(0, ret, 0, opt);
	ModulemgrSyscallLeave(state);
	return ret;
}

/* name unknown */
SceUID sceKernelLoadStartModuleForUser(const char *path, SceSize args, void *argp,
					      SceUInt32 flags, const SceKernelLMOption *opt,
					      int *status)
{
	SceUID modid = 0, puid = 0;
	SceUInt32 state;
	int st, ret, res;

	state = ModulemgrSyscallEnter();
	ret = ModulemgrLoadModuleForUser(path, flags, opt, &modid, &puid);
	if (ret < 0) {
		ModulemgrSyscallLeave(state);
		return ret;
	}
	if (status == NULL)
		status = &st;
	ret = startModuleCommon(0, modid, args, argp, SYSCALL_START_FLAGS, NULL, status);
	if (ret == 0) {
		ModulemgrSyscallLeave(state);
		return puid;
	}
	res = unloadModuleCommon(0, modid, 0, NULL);
	ModulemgrSyscallLeave(state);
	if (res < 0)
		return res;
	return ret == 1 ? 0 : ret;
}

/* name unknown */
int sceKernelStopUnloadModuleForUser(SceUID modid, SceSize args, void *argp,
					    SceUInt32 flags, const SceKernelULMOption *opt,
					    int *status)
{
	SceUInt32 state;
	int st, ret;
	SceUID uid;

	state = ModulemgrSyscallEnter();
	uid = SceSysmemForDriver_45D22597(0, modid);
	if (uid < 0) {
		ret = uid | SYSCALL_ERROR_PUID;
	} else if (flags != 0) {
		ret = SCE_KERNEL_ERROR_INVALID_FLAGS;
	} else {
		if (status == NULL)
			status = &st;
		ret = stopModuleCommon(0, uid, args, argp, SYSCALL_START_FLAGS, NULL, status);
		if (ret >= 0)
			ret = unloadModuleCommon(0, uid, 0, opt);
	}
	ModulemgrSyscallLeave(state);
	return ret;
}

/* name unknown */
SceUID sceKernelOpenModuleForUser(const char *upath, SceSize args, void *argp,
					 SceUInt32 flags, const SceKernelLMOption *opt, int *status)
{
	SceModuleObject *obj;
	SceUInt32 state;
	char *path;
	SceUID uid;
	int ret;

	state = ModulemgrSyscallEnter();
	if ((flags & 0xff0fff0f) != 0 || ((flags & ~0x10) != 0 && flags != 0x100000)) {
		ModulemgrSyscallLeave(state);
		return SCE_KERNEL_ERROR_INVALID_FLAGS;
	}
	ret = ModulemgrCopyPathFromUser(upath, &path);
	if (ret < 0) {
		ModulemgrSyscallLeave(state);
		return ret;
	}
	uid = loadStartSharedModuleCommon(0, path, args, argp,
					  flags | SYSCALL_START_FLAGS | LOAD_FLAG_8000, opt, status);
	ModulemgrFree(0, path);
	ret = uid;
	if (uid >= 0) {
		obj = ModulemgrReferModule(uid);
		if (obj == NULL) {
			ModulemgrSyscallLeave(state);
			return SCE_KERNEL_ERROR_ERROR;
		}
		ret = obj->cb.puid;
		ModulemgrReleaseModule(uid);
	}
	ModulemgrSyscallLeave(state);
	return ret;
}

/* name unknown */
int sceKernelCloseModuleForUser(SceUID modid, SceSize args, void *argp, SceUInt32 flags,
				       const SceKernelULMOption *opt, int *status)
{
	SceUInt32 state;
	int ret;

	state = ModulemgrSyscallEnter();
	ret = SceSysmemForDriver_45D22597(0, modid);
	if (ret < 0)
		ret |= SYSCALL_ERROR_PUID;
	else if (flags != 0)
		ret = SCE_KERNEL_ERROR_INVALID_FLAGS;
	else
		ret = ksceKernelStopUnloadSharedModuleForPid(sceKernelGetProcessId(), ret, args,
							     argp, 0, opt, status);
	ModulemgrSyscallLeave(state);
	return ret;
}

int sceKernelGetModuleListForUser(SceUInt32 flags, SceUID *ulist, SceSize *unum)
{
	SceSize count, n;
	SceUInt32 state;
	SceUID *list;
	int ret;

	state = ModulemgrSyscallEnter();
	if (flags == 0)
		flags = 1;
	count = 0;
	if (ulist == NULL) {
		ret = sceKernelGetModuleList(0, flags, 0, NULL, NULL);
		ModulemgrSyscallLeave(state);
		return ret;
	}
	ret = SceSysmemForDriver_BC996A7A(&count, unum, sizeof(count));
	if (ret < 0) {
		ModulemgrSyscallLeave(state);
		return ret;
	}
	list = SceSysmemForKernel_85571907(count * sizeof(SceUID));
	if (list == NULL) {
		ModulemgrSyscallLeave(state);
		return SCE_KERNEL_ERROR_MODULEMGR_NOMEM;
	}
	n = count;
	ret = sceKernelGetModuleList(0, flags, 2, list, &n);
	if (ret >= 0) {
		ret = SceSysmemForDriver_6D88EF8A(ulist, list, n * sizeof(SceUID));
		if (ret >= 0)
			ret = SceSysmemForDriver_6D88EF8A(unum, &n, sizeof(n));
	}
	SceSysmemForKernel_4233C16D(list);
	ModulemgrSyscallLeave(state);
	return ret;
}

int sceKernelGetModuleInfo(SceUID modid, SceKernelModuleInfo *uinfo)
{
	SceKernelModuleInfo info;
	SceUInt32 state;
	int ret;

	state = ModulemgrSyscallEnter();
	ret = SceSysmemForDriver_45D22597(0, modid);
	if (ret < 0) {
		ret |= SYSCALL_ERROR_PUID;
	} else {
		memset(&info, 0, sizeof(info));
		info.size = sizeof(info);
		ret = LibraryDBGetModuleInfoById(sceKernelGetProcessId(), ret, &info);
		if (ret >= 0)
			ret = SceSysmemForDriver_6D88EF8A(uinfo, &info, sizeof(info));
	}
	ModulemgrSyscallLeave(state);
	return ret;
}

SceUID sceKernelGetModuleIdByAddr(const void *addr)
{
	SceUInt32 state;
	SceUID ret;

	state = ModulemgrSyscallEnter();
	ret = LibraryDBGetModuleIdByAddr(0, (SceUInt32)addr);
	ModulemgrSyscallLeave(state);
	return ret;
}

int _sceKernelGetSystemSwVersion(SceKernelSystemSwVersion *uinfo)
{
	SceUInt32 state;
	int ret;

	state = ModulemgrSyscallEnter();
	SceKernelSystemSwVersion info __attribute__((aligned(8))) = { 0 };

	ret = SceSysmemForDriver_BC996A7A(&info, uinfo, 4);
	if (ret >= 0) {
		ret = sceKernelGetSystemSwVersion(&info);
		if (ret >= 0)
			ret = SceSysmemForDriver_6D88EF8A(uinfo, &info, sizeof(info));
	}
	ModulemgrSyscallLeave(state);
	return ret;
}

SceUInt32 sceKernelGetAllowedSdkVersionOnSystem(void)
{
	SceUInt32 state, ret;

	sceKernelApiDeclareTypeUInt32();
	state = ModulemgrSyscallEnter();
	SceKernelSystemSwVersion info __attribute__((aligned(8))) = { .size = sizeof(info) };

	ret = 0;
	if (sceKernelGetSystemSwVersion(&info) == 0)
		ret = info.version | 0xfff;
	ModulemgrSyscallLeave(state);
	return ret;
}

int sceKernelGetLibraryInfoByNID(SceUID modid, SceNID libnid, SceKernelLibraryInfo *uinfo)
{
	SceUInt32 state;
	SceUID pid;
	int ret;

	state = ModulemgrSyscallEnter();
	pid = sceKernelGetProcessId();
	ret = SceSysmemForDriver_45D22597(pid, modid);
	if (ret < 0) {
		ret |= SYSCALL_ERROR_PUID;
	} else {
		SceKernelLibraryInfo info __attribute__((aligned(8))) = { .size = sizeof(info) };

		ret = LibraryDBGetLibraryInfoByNID(pid, ret, libnid, &info);
		if (ret >= 0)
			ret = SceSysmemForDriver_6D88EF8A(uinfo, &info, sizeof(info));
	}
	ModulemgrSyscallLeave(state);
	return ret;
}

int sceKernelIsCalledFromSysModule(const void *addr)
{
	SceUInt32 state;
	int ret;

	state = ModulemgrSyscallEnter();
	ret = ksceKernelGetModuleIsSharedByAddr(sceKernelGetProcessId(), (SceUInt32)addr);
	ModulemgrSyscallLeave(state);
	return ret;
}

int sceKernelInhibitLoadingModule(SceUInt32 flags)
{
	SceUInt32 state;
	int ret;

	state = ModulemgrSyscallEnter();
	ret = ModulemgrInhibitLoadingModule(flags);
	ModulemgrSyscallLeave(state);
	return ret;
}
