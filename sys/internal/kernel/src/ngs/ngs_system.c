/* SceNgs systems: requests applied at an update, player buffers, sizing. */

#include <scetypes.h>
#include <intrmgr/syscall.h>
#include <sysmem/sysmem.h>
#include <ngs/error.h>
#include "ngs_module_memory.h"
#include "ngs_system.h"
#include "ngs_platform.h"
#include "ngs_rack.h"

/* Marks the voices whose patches or parameters the caller changed. */
void ngsApplyRackUpdateRequests(unsigned int *rackCount, NgsSynRack ***racks)
{
	NgsSynRack **list = *racks;
	NgsSynRack *rack;
	NgsVoice *voice;
	unsigned int *requested, *patchMasks, *paramsMasks;
	unsigned int i, j, voiceCount;

	for (i = 0; i < *rackCount; i++) {
		rack = list[i];
		if (rack == NULL || rack->state != NGS_RACK_STATE_ACTIVE)
			continue;
		requested = rack->updateRequested;
		if (*requested == 0)
			continue;
		voiceCount = rack->voiceCount;
		patchMasks = rack->patchUpdateMasks;
		paramsMasks = rack->paramsUpdateMasks;
		for (j = 0; j < voiceCount; j++) {
			if (patchMasks[j] != 0) {
				voice = &rack->voices[j];
				voice->patchDirtyMask |= patchMasks[j];
				voice->dirtyFlags |= 1;
				voice->dirtyFlags2 |= 1;
			}
			if (paramsMasks[j] != 0) {
				voice = &rack->voices[j];
				voice->paramsDirtyMask |= paramsMasks[j];
				voice->dirtyFlags |= 4;
				voice->dirtyFlags2 |= 4;
			}
		}
		*requested = 0;
	}
}

/* Locks the memory block holding a player buffer, once per block. */
static inline int ngsLockPlayerBuffer(NgsSynSystem *system, const void *buffer,
				      SceInt32 size)
{
	const unsigned char *start = buffer;
	const unsigned char *end = start + size;
	NgsLockedMemoryRange *range;
	MemBlockPhyInfo info;
	unsigned int i;
	SceUID uid;
	int result;

	if (start >= end)
		return system->compiledSdkVersion < 0x1800000 ?
			SCE_NGS_ERROR : SCE_NGS_ERROR_INVALID_BUFFER;
	for (i = 0; i < system->lockedRangeCount; i++) {
		range = &system->lockedRanges[i];
		if (start >= (unsigned char *)range->address &&
		    end <= (unsigned char *)range->address + range->size)
			return 0;
	}
	if (i == 1024)
		return SCE_NGS_ERROR_OUT_OF_ASSETS;
	uid = sceKernelFindMemBlockByAddr((void *)start, 0);
	if (uid < 0)
		goto invalid;
	info.ex.size = sizeof(info);
	if (SceSysmemForDriver_24A99FFF(uid, &info) < 0)
		goto invalid;
	range = &system->lockedRanges[system->lockedRangeCount];
	range->address = (void *)info.ex.details.vbase;
	range->size = info.ex.details.size;
	if (info.ex.details.size < (uintptr_t)buffer + size -
	    info.ex.details.vbase)
		return SCE_NGS_ERROR_INVALID_BUFFER;
	result = SceSysmemForDriver_59A4402F((void *)info.ex.details.vbase,
					     info.ex.details.size);
	if (result != 0)
		return result;
	result = sceKernelIsAccessibleRange(0x10, buffer, size);
	if (result != 0) {
		sceKernelUnlockRange((void *)info.ex.details.vbase,
				     info.ex.details.size);
		return result;
	}
	system->lockedRangeCount++;
	return 0;
invalid:
	if (system->compiledSdkVersion < 0x1800000)
		return SCE_NGS_ERROR;
	return SCE_NGS_ERROR_INVALID_BUFFER;
}

int ngsLockAt9PlayerBuffers(NgsSynSystem *system, const SceNgsAT9Params *params)
{
	const SceNgsAT9BufferParams *buffer;
	int i, result;

	for (i = 0; i < SCE_NGS_AT9_MAX_BUFFERS; i++) {
		buffer = &params->buffs[i];
		if (buffer->pBuffer == NULL || buffer->nNumBytes <= 0)
			continue;
		result = ngsLockPlayerBuffer(system, buffer->pBuffer,
					     buffer->nNumBytes);
		if (result != 0)
			return result;
	}
	return 0;
}

int ngsLockPcmPlayerBuffers(NgsSynSystem *system,
			    const SceNgsPlayerParams *params)
{
	const SceNgsPlayerBufferParams *buffer;
	int i, result;

	for (i = 0; i < SCE_NGS_PLAYER_MAX_BUFFERS; i++) {
		buffer = &params->buffs[i];
		if (buffer->pBuffer == NULL || buffer->nNumBytes <= 0)
			continue;
		result = ngsLockPlayerBuffer(system, buffer->pBuffer,
					     buffer->nNumBytes);
		if (result != 0)
			return result;
	}
	return 0;
}

unsigned int ngsGetNextRackId(NgsSynSystem *system)
{
	return system->nextRackId++;
}

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
