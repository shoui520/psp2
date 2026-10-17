#include <sysmem/cp15.h>
#include "update_internal.h"

int UpdateUtilCreateThread(const char *name, SceKernelThreadEntry entry,
		int priority, SceSize stackSize, unsigned int attr,
		int cpuAffinityMask, const SceKernelThreadOptParam *option)
{
	unsigned int state;
	int result;

	state = readTpidruro();
	writeTpidruro(0);
	result = ksceKernelCreateThread(name, entry, priority, stackSize,
			attr, cpuAffinityMask, option);
	writeTpidruro(state);
	return result;
}
