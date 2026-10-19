/* SceNgs system memory sizing. */

#include <scetypes.h>
#include <intrmgr/syscall.h>
#include "ngs_module_memory.h"
#include "ngs_system.h"
#include "ngs_platform.h"

int sceNgsSystemGetRequiredMemorySizeInternal(
	const SceNgsSystemInitParams *params, SceUInt32 *size)
{
	SceNgsSystemInitParams init;
	SceSize alignedSize = 0;
	SceSize extraSize;
	SceSize systemSize;
	SceUInt32 syscallState;
	int copyUserData;
	int voiceListSize;
	int rackListSize;
	int unusedSize;
	int nMaxRacks;
	int nMaxVoices;
	int result;

	syscallState = sceSyscallEnter();
	copyUserData = ngsGetSystemInstanceCount();
	if (size == NULL || params == NULL ||
	    ngsCopyFromCaller(&init, params, sizeof(init), copyUserData) != 0 ||
	    ngsIsCallerRangeAccessible(size, sizeof(*size), copyUserData) == 0 ||
	    (unsigned int)(init.nGranularity - 64) > 448 ||
	    ((unsigned int)init.nGranularity << 26) != 0 ||
	    init.nSampleRate != 48000 || init.nMaxModules < 0 ||
	    (unsigned int)((nMaxVoices = init.nMaxVoices) - 1) > 255 ||
	    (unsigned int)((nMaxRacks = init.nMaxRacks) - 1) > 63) {
		result = (int)0x804a0002;
	} else {
		result = ngsGetSystemExtraMemorySize(&extraSize);
		if (result == 0) {
			systemSize = ngsAlignMemorySize(extraSize + 8632 +
				8 * nMaxRacks + 4 * nMaxRacks + 4 * nMaxVoices);
			voiceListSize = ngsAlignSize(
				12 * (init.nMaxRacks + init.nMaxVoices + 33), 256);
			rackListSize = ngsAlignSize(32 * init.nMaxRacks, 256);
			ngsAlignMemorySize(rackListSize + voiceListSize + 256);
			unusedSize = ngsAlignMemorySize(0);
			result = ngsGetAlignedMemorySize(28 * init.nMaxRacks +
				288 + 8 * (2 * init.nMaxRacks + 1 +
				2 * init.nMaxVoices + init.nMaxVoices),
				unusedSize, 0, systemSize, &alignedSize);
			if (result == 0)
				result = ngsCopyToCaller(size, &alignedSize,
					sizeof(*size), copyUserData);
		}
	}
	sceSyscallLeave(syscallState);
	return result;
}
