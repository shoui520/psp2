/* SceNgs systems: requests applied at an update, player buffers, sizing. */

#include <scetypes.h>
#include <string.h>
#include <kernel/error.h>
#include <sysmem/sysroot.h>
#include <vnz_wrapper/vnz_wrapper.h>
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

SceInt32 sceNgsSystemReleaseInternal(SceNgsHSynSystem handle)
{
	unsigned int syscall = sceSyscallEnter();
	NgsProcessStorage *storage = NULL;
	NgsSynSystem *system;
	SceNgsCallbackInfo *info, *callerInfo;
	SceNgsRackReleaseCallbackFunc callback;
	NgsSynRack *rack;
	unsigned int index;
	int result;

	result = SceCodecEngineWrapperForDriver_CB8703D8();
	if (result < 0)
		goto leave;
	if (ngsGetProcessStorage(&storage) != 0 ||
	    ngsLockProcess(storage) != 0) {
		result = SCE_NGS_ERROR;
		goto unlock;
	}
	system = ngsGetSystemFromHandle(handle);
	if (system == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto unlockProcess;
	}
	system->state = NGS_SYSTEM_STATE_RELEASING;
	ngsLockSystemMutex(system->context);
	ngsUnlockSystemMutex(system->context);
	result = ngsCheckContextUnlocked(system->context);
	if (result != 0)
		goto unlockProcess;

	for (index = 0; index < system->MaxRacks; index++) {
		rack = system->racks[index];
		if (rack == NULL)
			continue;
		callback = rack->releaseCallback;
		info = &system->rackReleaseInfo[index];
		info->pUserData = rack->userReleaseData;
		info->hRackHandle = ngsGetRackHandle(system->racks[index]);
		result = ngsReleaseRack(ngsGetRackHandle(system->racks[index]),
			NULL);
		if (result == SCE_KERNEL_ERROR_THREAD_STOPPED ||
		    result == SCE_KERNEL_ERROR_THREAD_SUSPENDED)
			goto unlockProcess;
		if (callback != NULL &&
		    system->callbackCount < system->callbackCapacity) {
			info = &system->rackReleaseInfo[index];
			callerInfo = (SceNgsCallbackInfo *)((char *)info +
				((char *)system->callbackUserMemory -
				 (char *)system->callbackKernelMemory));
			system->callbacks[system->callbackCount].callback =
				(void *)callback;
			system->callbacks[system->callbackCount].info = callerInfo;
			system->callbackCount++;
			ngsCopyToCaller(callerInfo, info, sizeof(*info),
				system->copyUserData);
		}
	}
	system->signature = 0;
	result = ngsReleaseSystem(system);
	system->state = NGS_SYSTEM_STATE_NONE;
	ngsReleaseDebugBuffer(&system->debugBuffer, system->debugMemBlock);
	ngsFreeProcessMemory(handle, system->coherentAllocation,
		system->coherentAllocationSize, system->dspHeapAllocation,
		system->dspHeapAllocationSize, system->systemAllocation,
		system->systemAllocationSize, system->systemMemBlock);
	if (ngsUnlockProcess(storage) != 0 && result == 0)
		result = SCE_NGS_ERROR;
	goto unlock;
unlockProcess:
	ngsUnlockProcess(storage);
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsSystemLockInternal(SceNgsHSynSystem handle)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemFromHandle(handle);
	SceInt32 result;

	if (system == NULL)
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	else if (ngsLockSystem(system->context) != 0)
		result = SCE_NGS_ERROR;
	else
		result = 0;
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsSystemUnlockInternal(SceNgsHSynSystem handle)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemFromHandle(handle);
	SceInt32 result;

	if (system == NULL)
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	else if (ngsUnlockSystem(system->context) != 0)
		result = SCE_NGS_ERROR;
	else
		result = 0;
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsSystemSetParamErrorCallbackInternal(SceNgsHSynSystem handle,
	const SceNgsParamsErrorCallbackFunc callback)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemFromHandle(handle);
	SceInt32 result;

	if (system != NULL) {
		system->paramErrorCallback = callback;
		result = 0;
	} else {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	}
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsSystemGetCallbackListInternal(SceNgsHSynSystem handle,
	NgsCallbackList **array, SceUInt32 *count)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemFromHandle(handle);
	SceInt32 result;

	if (system == NULL || array == NULL) {
		result = system != NULL ? 0 : SCE_NGS_ERROR_INVALID_HANDLE;
		sceSyscallLeave(syscall);
		return result;
	}
	result = system->callbackCount;
	if (result != 0) {
		if (ngsCopyToCaller(system->callerCallbacks, system->callbacks,
			sizeof(NgsCallbackList) * result,
			system->copyUserData) != 0 ||
		    ngsCopyToCaller(array, &system->callerCallbacks,
			sizeof(*array), system->copyUserData) != 0)
			result = 0;
		else
			result = system->callbackCount;
	}
	system->callbackCount = 0;
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsSystemSetFlagsInternal(SceNgsHSynSystem handle,
	const SceUInt32 flags)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemFromHandle(handle);
	SceInt32 result;

	if (system != NULL) {
		system->flags = flags;
		result = 0;
	} else {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	}
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsSulphaSetSynthUpdateCallbackInternal(SceNgsHSynSystem handle,
	void *callback, const SceNgsBufferInfo *info)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemFromHandle(handle);
	SceNgsBufferInfo buffer;
	SceInt32 result = SCE_NGS_ERROR_INVALID_HANDLE;

	if (system != NULL) {
		system->synthUpdateCallback = callback;
		if (info != NULL) {
			result = SCE_NGS_ERROR_INVALID_PARAM;
			if (ngsCopyFromCaller(&buffer, info, sizeof(buffer),
				system->copyUserData) == 0) {
				system->callerDebugBuffer = buffer;
				ngsReleaseDebugBuffer(&system->debugBuffer,
					system->debugMemBlock);
				ngsAllocateDebugBuffer(&system->debugBuffer,
					&system->debugMemBlock, buffer.size);
				result = 0;
			}
		} else {
			system->callerDebugBuffer.data = NULL;
			system->callerDebugBuffer.size = 0;
			ngsReleaseDebugBuffer(&system->debugBuffer,
				system->debugMemBlock);
			result = 0;
		}
	}
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsSulphaGetSynthUpdateCallbackInternal(SceNgsHSynSystem handle,
	void **callback, SceNgsBufferInfo *info)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemFromHandle(handle);
	SceNgsBufferInfo buffer;
	SceInt32 result;

	if (system == NULL || callback == NULL) {
		if (system == NULL) {
			result = SCE_NGS_ERROR_INVALID_HANDLE;
			goto leave;
		}
	} else if (info != NULL) {
		result = SCE_NGS_ERROR;
		if (system->synthUpdateCallback == NULL)
			goto leave;
		buffer = system->callerDebugBuffer;
		if (ngsCopyToCaller(callback, &system->synthUpdateCallback,
			sizeof(*callback), system->copyUserData) == 0 &&
		    ngsCopyToCaller(info, &buffer, sizeof(buffer),
			system->copyUserData) == 0 &&
		    ngsCopyToCaller(buffer.data, system->debugBuffer.data,
			buffer.size, system->copyUserData) == 0) {
			result = system->synthUpdateCallback != NULL ?
				0 : SCE_NGS_ERROR;
			goto leave;
		}
	}
	result = SCE_NGS_ERROR_INVALID_PARAM;
leave:
	sceSyscallLeave(syscall);
	return result;
}

/* Titles released before the fix that rely on it too. */
void ngsCheckFixForBugzilla89940(NgsSynSystem *system)
{
	char titleId[32];

	system->fixForBugzilla89940 = 0;
	if (SceSysrootForKernel_EC3124A3(0, titleId, sizeof(titleId)) < 0)
		return;
	if (strncmp(titleId, "PCSG00140", sizeof(titleId)) == 0 ||
	    strncmp(titleId, "PCSH00044", sizeof(titleId)) == 0 ||
	    strncmp(titleId, "PCSH00073", sizeof(titleId)) == 0 ||
	    strncmp(titleId, "PCSE00300", sizeof(titleId)) == 0 ||
	    strncmp(titleId, "PCSB00471", sizeof(titleId)) == 0 ||
	    strncmp(titleId, "PCSG90080", sizeof(titleId)) == 0 ||
	    strncmp(titleId, "PCSG90123", sizeof(titleId)) == 0 ||
	    strncmp(titleId, "PCSH00046", sizeof(titleId)) == 0 ||
	    strncmp(titleId, "PCSE00375", sizeof(titleId)) == 0 ||
	    strncmp(titleId, "PCSB00493", sizeof(titleId)) == 0)
		system->fixForBugzilla89940 = 1;
}

unsigned int sceNgsSystemGetSysHandleFromRack(SceNgsHRack handle)
{
	NgsSynRack *rack = ngsGetRackFromHandle(handle);

	if (rack == NULL)
		return 0;
	return ngsGetSystemHandle(rack->system);
}

int sceNgsSystemIsFixForBugzilla89940(SceNgsHSynSystem handle)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system;
	int result;

	/* Sony's returns these without leaving the syscall state. */
	if (handle == 0)
		return 0;
	system = ngsGetSystemFromHandle(handle);
	if (system == NULL)
		return 0;
	result = system->compiledSdkVersion >= 0x3550000 ||
		system->fixForBugzilla89940 == 1;
	sceSyscallLeave(syscall);
	return result;
}

/* Copies the voices' states for the caller, unless the system is updating. */
static inline void ngsPullVoiceStates(NgsSynSystem *system, int clearFlags)
{
	NgsSynRack *rack;
	unsigned int index, voice;

	if (ngsTryLockSystem(system->context) != 0) {
		system->pullDataDeferred = 1;
		return;
	}
	ngsUnlockSystem(system->context);
	system->pullDataDeferred = 0;
	for (index = 0; index < system->MaxRacks; index++) {
		rack = system->racks[index];
		if (rack == NULL || rack->state != NGS_RACK_STATE_ACTIVE)
			continue;
		for (voice = 0; voice < rack->voiceCount; voice++) {
			if (!clearFlags) {
				rack->voices[voice].callerPlayState =
					rack->voices[voice].playState;
				continue;
			}
			rack->voices[voice].callerPlayState =
				rack->voices[voice].playState;
			rack->voices[voice].callerDirtyFlags =
				rack->voices[voice].dirtyFlags;
			rack->voices[voice].callerFlags =
				rack->voices[voice].flags;
			if (rack->voices[voice].callerPlayState.pauseState != 2 &&
			    rack->voices[voice].callerPlayState.runState - 1 <= 1) {
				rack->voices[voice].flags = 0;
				rack->voices[voice].dirtyFlags = 0;
			}
		}
	}
}

/* Reads the caller's update block of a rack. */
static inline int ngsPullRackUpdates(NgsSynRack *rack)
{
	return ngsCopyFromCaller(rack->updateCount, rack->updateCountCaller,
		rack->updateBlockSize, rack->system->copyUserData);
}

SceInt32 sceNgsSystemPullDataInternal(SceNgsHSynSystem handle,
	const SceUInt32 dirtyFlagsA, const SceUInt32 dirtyFlagsB)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemFromHandle(handle);
	NgsSynRack *rack;
	unsigned int count, index;
	SceInt32 result, status;

	if (system == NULL) {
		status = SCE_NGS_ERROR_INVALID_HANDLE;
		goto leave;
	}
	count = system->MaxRacks;
	if (system->compiledSdkVersion >= 0x2000000) {
		result = ngsLockVoice(system->context);
	} else {
		result = ngsLockVoice(system->context);
		if (result != 0) {
			status = result;
			goto leave;
		}
		ngsPullVoiceStates(system, 0);
		result = ngsUnlockVoice(system->context);
	}
	if (result != 0) {
		status = result;
		goto leave;
	}

	if (count >= 32)
		count = 32;
	status = 0;
	for (index = 0; index != count; index++) {
		rack = system->racks[index];
		if (rack != NULL && ((dirtyFlagsA >> index) & 1) != 0 &&
		    rack->state == NGS_RACK_STATE_ACTIVE &&
		    ngsPullRackUpdates(rack) != 0)
			status = SCE_NGS_ERROR;
	}
	for (index = count; index < system->MaxRacks; index++) {
		rack = system->racks[index];
		/* Sony's shifts by the rack index here too. */
		if (rack != NULL && ((dirtyFlagsB >> index) & 1) != 0 &&
		    rack->state == NGS_RACK_STATE_ACTIVE &&
		    ngsPullRackUpdates(rack) != 0)
			status = SCE_NGS_ERROR;
	}

	if (system->compiledSdkVersion >= 0x2000000) {
		ngsApplyRackUpdateRequests(&system->MaxRacks, &system->racks);
		ngsPullVoiceStates(system, 1);
		status = 0;
		ngsUnlockVoice(system->context);
	}
leave:
	sceSyscallLeave(syscall);
	return status;
}

SceInt32 sceNgsSystemPushDataInternal(SceNgsHSynSystem handle)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemFromHandle(handle);
	NgsSynRack *rack;
	unsigned int index;
	SceInt32 result;
	int copied;

	if (system == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto leave;
	}
	result = 0;
	if (system->MaxRacks == 0)
		goto leave;
	index = 0;
	do {
		rack = system->racks[index++];
		if (rack == NULL || rack->state != NGS_RACK_STATE_ACTIVE ||
		    rack->requestsDirty == 0)
			continue;
		copied = ngsCopyToCaller(rack->voiceResultsCaller,
			rack->voiceResults, rack->requestBlockSize,
			system->copyUserData);
		rack->requestsDirty = 0;
		if (copied != 0)
			result = SCE_NGS_ERROR;
	} while (index < system->MaxRacks);
leave:
	sceSyscallLeave(syscall);
	return result;
}
