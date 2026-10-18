#include "libkernel_backends.h"
#include <kernel/modulemgr.h>
#include <modulemgr/module_info.h>
#include <libkernel/libkernel.h>

struct ModuleCallOptions {
	SceUInt flags;
	const void *option;
	int *status;
};

SceUID sceKernelLoadModule(const char *path, SceUInt flags,
		const SceKernelLoadModuleOpt *option)
{
	return _sceKernelLoadModule(path, flags, option);
}

int sceKernelUnloadModule(SceUID id, SceUInt flags,
		const SceKernelUnloadModuleOpt *option)
{
	return _sceKernelUnloadModule(id, flags, option);
}

int sceKernelStartModule(SceUID id, SceSize size, const void *argument,
		SceUInt flags, const SceKernelStartModuleOpt *option, int *status)
{
	struct ModuleCallOptions options = { flags, option, status };

	return _sceKernelStartModule(id, size, argument, &options);
}

int sceKernelStopModule(SceUID id, SceSize size, const void *argument,
		SceUInt flags, const SceKernelStopModuleOpt *option, int *status)
{
	struct ModuleCallOptions options = { flags, option, status };

	return _sceKernelStopModule(id, size, argument, &options);
}

SceUID sceKernelLoadStartModule(const char *path, SceSize size, const void *argument,
		SceUInt flags, const SceKernelLoadModuleOpt *option, int *status)
{
	struct ModuleCallOptions options = { flags, option, status };

	return _sceKernelLoadStartModule(path, size, argument, &options);
}

int sceKernelStopUnloadModule(SceUID id, SceSize size, const void *argument,
		SceUInt flags, const SceKernelUnloadModuleOpt *option, int *status)
{
	struct ModuleCallOptions options = { flags, option, status };

	return _sceKernelStopUnloadModule(id, size, argument, &options);
}

SceUID sceKernelOpenModule(const char *path, SceSize size, void *argument,
		SceUInt flags, const SceKernelLoadModuleOpt *option, int *status)
{
	struct ModuleCallOptions options = { flags, option, status };

	return _sceKernelOpenModule(path, size, argument, &options);
}

int sceKernelCloseModule(SceUID id, SceSize size, void *argument,
		SceUInt flags, const SceKernelUnloadModuleOpt *option, int *status)
{
	struct ModuleCallOptions options = { flags, option, status };

	return _sceKernelCloseModule(id, size, argument, &options);
}

int sceKernelGetModuleInfoByAddr(const void *address, SceKernelModuleInfo *info)
{
	SceUID id = sceKernelGetModuleIdByAddr(address);

	if (id < 0)
		return id;
	return sceKernelGetModuleInfo(id, info);
}
