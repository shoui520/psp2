/*
 * syscall_stub.c - the entry points of the module loading system calls
 * (Sony's file name is unknown). A file of its own: the original keeps these
 * apart from modulemgr_for_user.c's functions, which one file would interleave
 * with them (each stub calls one)
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
	return _sceKernelLoadModuleWithoutStartForUser(path, flags, opt);
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
		return _sceKernelStartModuleForUser(modid, args, argp, flags, opt, NULL);
	status = __builtin_alloca(sizeof(*status));
	*status = 0;
	ret = _sceKernelStartModuleForUser(modid, args, argp, flags, opt, status);
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
		return _sceKernelStopModuleForUser(modid, args, argp, flags, opt, NULL);
	status = __builtin_alloca(sizeof(*status));
	*status = 0;
	ret = _sceKernelStopModuleForUser(modid, args, argp, flags, opt, status);
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
	return _sceKernelUnloadModuleForUser(modid, flags, opt);
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
		return _sceKernelLoadStartModuleForUser(path, args, argp, flags, opt, NULL);
	status = __builtin_alloca(sizeof(*status));
	*status = 0;
	ret = _sceKernelLoadStartModuleForUser(path, args, argp, flags, opt, status);
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
		return _sceKernelStopUnloadModuleForUser(modid, args, argp, flags, opt, NULL);
	status = __builtin_alloca(sizeof(*status));
	*status = 0;
	ret = _sceKernelStopUnloadModuleForUser(modid, args, argp, flags, opt, status);
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
		return _sceKernelOpenModuleForUser(path, args, argp, flags, opt, NULL);
	status = __builtin_alloca(sizeof(*status));
	*status = 0;
	ret = _sceKernelOpenModuleForUser(path, args, argp, flags, opt, status);
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
		return _sceKernelCloseModuleForUser(modid, args, argp, flags, opt, NULL);
	status = __builtin_alloca(sizeof(*status));
	*status = 0;
	ret = _sceKernelCloseModuleForUser(modid, args, argp, flags, opt, status);
	res = sceKernelCopyToUser(ustatus, status, sizeof(*status));
	return res < 0 ? res : ret;
}
