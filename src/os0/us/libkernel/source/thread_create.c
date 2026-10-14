#include <psp2/kernel/threadmgr.h>

struct ThreadCreateOptions {
	SceSize size;
	SceSize stackSize;
	SceUInt attributes;
	int cpuAffinityMask;
	const SceKernelThreadOptParam *option;
	void *caller;
};

extern SceUID sceKernelCreateThreadForUser(const char *name,
		SceKernelThreadEntry entry, int priority,
		const struct ThreadCreateOptions *options);

SceUID sceKernelCreateThread(const char *name, SceKernelThreadEntry entry,
		int priority, SceSize stackSize, SceUInt attributes,
		int cpuAffinityMask, const SceKernelThreadOptParam *option)
{
	struct ThreadCreateOptions options = {
		sizeof(options), stackSize, attributes, cpuAffinityMask, option,
		__builtin_return_address(0)
	};

	return sceKernelCreateThreadForUser(name, entry, priority, &options);
}
