/*
 * syscall_stub.c - the entry points of the module loading system calls
 * (Sony's file name is unknown)
 *
 * User arguments past the third come in a block that is copied in first;
 * options and the start/stop status are copied onto the kernel stack.
 */

#include "modulemgr_internal.h"

SceUID _sceKernelLoadModule(const char *path, SceUInt32 flags, const SceKernelLoadModuleOpt *uopt)
{
	SceKernelLoadModuleOpt *opt = NULL;
	int ret;

	if (uopt != NULL) {
		opt = __builtin_alloca(sizeof(*opt));
		ret = sceKernelCopyFromUser(opt, uopt, 4);
		if (ret < 0)
			return ret;
	}
	return sceKernelLoadModuleForUser(path, flags, opt);
}

/*
 * The wrappers with a status pointer start/stop with a kernel copy of it
 * and copy the status out afterwards.
 */
int _sceKernelStartModule(SceUID modid, SceSize args, void *argp,
			  const SceKernelModuleSyscallParam *uparam)
{
	SceKernelModuleSyscallParam p;
	SceKernelStartModuleOpt *opt = NULL;
	int *status, *ustatus, ret, res;
	SceUInt32 flags;

	ret = sceKernelCopyFromUser(&p, uparam, sizeof(p));
	if (ret < 0)
		return ret;
	flags = p.flags;
	ustatus = p.status;
	if (p.opt != NULL) {
		opt = __builtin_alloca(0x10);
		ret = sceKernelCopyFromUser(opt, p.opt, 0x10);
		if (ret < 0)
			return ret;
	}
	if (ustatus == NULL)
		return sceKernelStartModuleForUser(modid, args, argp, flags, opt, NULL);
	status = __builtin_alloca(sizeof(*status));
	*status = 0;
	ret = sceKernelStartModuleForUser(modid, args, argp, flags, opt, status);
	res = sceKernelCopyToUser(ustatus, status, sizeof(*status));
	return res < 0 ? res : ret;
}

int _sceKernelStopModule(SceUID modid, SceSize args, void *argp,
			 const SceKernelModuleSyscallParam *uparam)
{
	SceKernelModuleSyscallParam p;
	SceKernelStopModuleOpt *opt = NULL;
	int *status, *ustatus, ret, res;
	SceUInt32 flags;

	ret = sceKernelCopyFromUser(&p, uparam, sizeof(p));
	if (ret < 0)
		return ret;
	flags = p.flags;
	ustatus = p.status;
	if (p.opt != NULL) {
		opt = __builtin_alloca(0x10);
		ret = sceKernelCopyFromUser(opt, p.opt, 0x10);
		if (ret < 0)
			return ret;
	}
	if (ustatus == NULL)
		return sceKernelStopModuleForUser(modid, args, argp, flags, opt, NULL);
	status = __builtin_alloca(sizeof(*status));
	*status = 0;
	ret = sceKernelStopModuleForUser(modid, args, argp, flags, opt, status);
	res = sceKernelCopyToUser(ustatus, status, sizeof(*status));
	return res < 0 ? res : ret;
}

int _sceKernelUnloadModule(SceUID modid, SceUInt32 flags, const SceKernelUnloadModuleOpt *uopt)
{
	SceKernelUnloadModuleOpt *opt = NULL;
	int ret;

	if (uopt != NULL) {
		opt = __builtin_alloca(sizeof(*opt));
		ret = sceKernelCopyFromUser(opt, uopt, 4);
		if (ret < 0)
			return ret;
	}
	return sceKernelUnloadModuleForUser(modid, flags, opt);
}

SceUID _sceKernelLoadStartModule(const char *path, SceSize args, void *argp,
				 const SceKernelModuleSyscallParam *uparam)
{
	SceKernelModuleSyscallParam p;
	SceKernelLoadModuleOpt *opt = NULL;
	int *status, *ustatus, ret, res;
	SceUInt32 flags;

	ret = sceKernelCopyFromUser(&p, uparam, sizeof(p));
	if (ret < 0)
		return ret;
	flags = p.flags;
	ustatus = p.status;
	if (p.opt != NULL) {
		opt = __builtin_alloca(sizeof(*opt));
		ret = sceKernelCopyFromUser(opt, p.opt, sizeof(*opt));
		if (ret < 0)
			return ret;
	}
	if (ustatus == NULL)
		return sceKernelLoadStartModuleForUser(path, args, argp, flags, opt, NULL);
	status = __builtin_alloca(sizeof(*status));
	*status = 0;
	ret = sceKernelLoadStartModuleForUser(path, args, argp, flags, opt, status);
	res = sceKernelCopyToUser(ustatus, status, sizeof(*status));
	return res < 0 ? res : ret;
}

int _sceKernelStopUnloadModule(SceUID modid, SceSize args, void *argp,
			       const SceKernelModuleSyscallParam *uparam)
{
	SceKernelModuleSyscallParam p;
	SceKernelUnloadModuleOpt *opt = NULL;
	int *status, *ustatus, ret, res;
	SceUInt32 flags;

	ret = sceKernelCopyFromUser(&p, uparam, sizeof(p));
	if (ret < 0)
		return ret;
	flags = p.flags;
	ustatus = p.status;
	if (p.opt != NULL) {
		opt = __builtin_alloca(sizeof(*opt));
		ret = sceKernelCopyFromUser(opt, p.opt, sizeof(*opt));
		if (ret < 0)
			return ret;
	}
	if (ustatus == NULL)
		return sceKernelStopUnloadModuleForUser(modid, args, argp, flags, opt, NULL);
	status = __builtin_alloca(sizeof(*status));
	*status = 0;
	ret = sceKernelStopUnloadModuleForUser(modid, args, argp, flags, opt, status);
	res = sceKernelCopyToUser(ustatus, status, sizeof(*status));
	return res < 0 ? res : ret;
}

SceUID _sceKernelOpenModule(const char *path, SceSize args, void *argp,
			    const SceKernelModuleSyscallParam *uparam)
{
	SceKernelModuleSyscallParam p;
	SceKernelLoadModuleOpt *opt = NULL;
	int *status, *ustatus, ret, res;
	SceUInt32 flags;

	ret = sceKernelCopyFromUser(&p, uparam, sizeof(p));
	if (ret < 0)
		return ret;
	flags = p.flags;
	ustatus = p.status;
	if (p.opt != NULL) {
		opt = __builtin_alloca(sizeof(*opt));
		ret = sceKernelCopyFromUser(opt, p.opt, sizeof(*opt));
		if (ret < 0)
			return ret;
	}
	if (ustatus == NULL)
		return sceKernelOpenModuleForUser(path, args, argp, flags, opt, NULL);
	status = __builtin_alloca(sizeof(*status));
	*status = 0;
	ret = sceKernelOpenModuleForUser(path, args, argp, flags, opt, status);
	res = sceKernelCopyToUser(ustatus, status, sizeof(*status));
	return res < 0 ? res : ret;
}

int _sceKernelCloseModule(SceUID modid, SceSize args, void *argp,
			  const SceKernelModuleSyscallParam *uparam)
{
	SceKernelModuleSyscallParam p;
	SceKernelUnloadModuleOpt *opt = NULL;
	int *status, *ustatus, ret, res;
	SceUInt32 flags;

	ret = sceKernelCopyFromUser(&p, uparam, sizeof(p));
	if (ret < 0)
		return ret;
	flags = p.flags;
	ustatus = p.status;
	if (p.opt != NULL) {
		opt = __builtin_alloca(sizeof(*opt));
		ret = sceKernelCopyFromUser(opt, p.opt, sizeof(*opt));
		if (ret < 0)
			return ret;
	}
	if (ustatus == NULL)
		return sceKernelCloseModuleForUser(modid, args, argp, flags, opt, NULL);
	status = __builtin_alloca(sizeof(*status));
	*status = 0;
	ret = sceKernelCloseModuleForUser(modid, args, argp, flags, opt, status);
	res = sceKernelCopyToUser(ustatus, status, sizeof(*status));
	return res < 0 ? res : ret;
}
