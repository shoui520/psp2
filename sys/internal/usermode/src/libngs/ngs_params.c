/*
 * Userspace shared-memory parameter, patch, voice state, and handle table management for SceNgsUser.
 */
#include "ngs_internal.h"

NgsSystemSlot g_ngsSystemSlots[NGS_MAX_SYSTEMS];

SceInt32 ngsPatchSetVolumeUser(SceNgsHPatch hPatchHandle, SceInt32 outputChannel,
			       SceInt32 inputChannel, SceFloat32 vol)
{
	SceUInt32 voiceIdx;
	SceUInt32 patchIdx;
	NgsRackShared *rack;
	SceInt32 ret = ngsResolvePatchHandle(hPatchHandle, &voiceIdx, &patchIdx, &rack);

	if (ret != SCE_NGS_OK)
		return ret;

	if (outputChannel < 0 || outputChannel >= rack->numChannelsPerVoice || (SceUInt32)inputChannel > 1U)
		return SCE_NGS_ERROR_INVALID_PARAM;

	SceFloat32 *volBase = rack->patchVolumes + (patchIdx + rack->patchesPerVoice * voiceIdx) *
			      (2U * (SceUInt32)rack->numChannelsPerVoice);

	ngsRackParamLock(rack);
	volBase[inputChannel + 2 * outputChannel] = vol;
	rack->patchDirtyMask[voiceIdx] |= (1U << patchIdx);
	(*rack->dirtyCounter)++;
	ngsRackParamUnlock(rack);

	return SCE_NGS_OK;
}

SceInt32 ngsPatchSetVolumesUser(SceNgsHPatch hPatchHandle, SceInt32 outputChannel,
				const SceFloat32 *volumes, SceInt32 numVols)
{
	SceUInt32 voiceIdx;
	SceUInt32 patchIdx;
	NgsRackShared *rack;
	SceInt32 ret = ngsResolvePatchHandle(hPatchHandle, &voiceIdx, &patchIdx, &rack);

	if (ret != SCE_NGS_OK)
		return ret;

	if (outputChannel < 0 || outputChannel >= rack->numChannelsPerVoice || numVols > 2)
		return SCE_NGS_ERROR_INVALID_PARAM;

	SceFloat32 *volBase = rack->patchVolumes + (patchIdx + rack->patchesPerVoice * voiceIdx) *
			      (2U * (SceUInt32)rack->numChannelsPerVoice);
	SceFloat32 *row = &volBase[2 * outputChannel];

	ngsRackParamLock(rack);
	row[0] = volumes[0];
	if (numVols == 2)
		row[1] = volumes[1];
	rack->patchDirtyMask[voiceIdx] |= (1U << patchIdx);
	(*rack->dirtyCounter)++;
	ngsRackParamUnlock(rack);

	return SCE_NGS_OK;
}

SceInt32 ngsPatchSetVolumesMatrixUser(SceNgsHPatch hPatchHandle, const SceNgsVolumeMatrix *matrix)
{
	SceUInt32 voiceIdx;
	SceUInt32 patchIdx;
	NgsRackShared *rack;
	SceInt32 ret;

	if (matrix == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;

	ret = ngsResolvePatchHandle(hPatchHandle, &voiceIdx, &patchIdx, &rack);
	if (ret != SCE_NGS_OK)
		return ret;

	SceFloat32 *volBase = rack->patchVolumes + (patchIdx + rack->patchesPerVoice * voiceIdx) *
			      (2U * (SceUInt32)rack->numChannelsPerVoice);

	ngsRackParamLock(rack);
	volBase[0] = matrix->m[0][0];
	volBase[1] = matrix->m[0][1];
	if ((SceUInt32)rack->numChannelsPerVoice > 1U) {
		volBase[2] = matrix->m[1][0];
		volBase[3] = matrix->m[1][1];
	}
	rack->patchDirtyMask[voiceIdx] |= (1U << patchIdx);
	(*rack->dirtyCounter)++;
	ngsRackParamUnlock(rack);

	return SCE_NGS_OK;
}

SceInt32 ngsPatchGetInfoUser(SceNgsHPatch hPatchHandle, SceNgsPatchRouteInfo *routeInfo,
			     SceNgsPatchSetupInfo *setup)
{
	SceUInt32 voiceIdx;
	SceUInt32 patchIdx;
	NgsRackShared *rack;
	SceInt32 ret = ngsResolvePatchHandle(hPatchHandle, &voiceIdx, &patchIdx, &rack);

	if (routeInfo == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;
	if (ret != SCE_NGS_OK)
		return ret;

	SceUInt32 flatIdx = patchIdx + rack->patchesPerVoice * voiceIdx;
	SceInt32 inChannels = rack->patchInputChannels[flatIdx];
	if (inChannels == 0)
		return SCE_NGS_ERROR_PATCH_NOT_AVAIL;

	SceInt32 outChannels = rack->numChannelsPerVoice;
	const SceFloat32 *volBase = rack->patchVolumes + flatIdx * (2U * (SceUInt32)outChannels);

	routeInfo->nOutputChannels = outChannels;
	routeInfo->nInputChannels = inChannels;
	routeInfo->vols.m[0][0] = volBase[0];
	if (outChannels == 1) {
		routeInfo->vols.m[0][1] = volBase[1];
		routeInfo->vols.m[1][0] = 0.0f;
		routeInfo->vols.m[1][1] = 0.0f;
	} else {
		routeInfo->vols.m[0][1] = volBase[1];
		routeInfo->vols.m[1][0] = volBase[2];
		routeInfo->vols.m[1][1] = volBase[3];
	}

	if (setup != NULL)
		*setup = rack->patchSetupInfos[flatIdx];

	return SCE_NGS_OK;
}

SceInt32 ngsVoiceGetInfoUser(SceNgsHVoice hVoiceHandle, SceNgsVoiceInfo *info)
{
	SceUInt32 voiceIdx;
	NgsRackShared *rack;
	SceInt32 ret;

	if (info == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;

	ret = ngsResolveVoiceHandle(hVoiceHandle, &voiceIdx, &rack);
	if (ret != SCE_NGS_OK)
		return ret;

	info->uVoiceState = rack->voiceStates[voiceIdx];
	info->uNumModules = (SceUInt32)rack->numModules;
	info->uNumInputs = rack->numInputs;
	info->uNumOutputs = rack->numOutputs;
	info->uNumPatchesPerOutput = rack->patchesPerOutput;
	info->uUpdateCallsActive = rack->voiceUpdateCounts[voiceIdx];
	return SCE_NGS_OK;
}

SceInt32 ngsVoiceGetStateDataUser(SceNgsHVoice hVoiceHandle, SceUInt32 moduleIdx,
				  void *mem, SceUInt32 memSize)
{
	SceUInt32 voiceIdx;
	NgsRackShared *rack;
	SceInt32 ret = ngsResolveVoiceHandle(hVoiceHandle, &voiceIdx, &rack);

	if (ret != SCE_NGS_OK)
		return ret;

	if (mem == NULL || moduleIdx >= rack->maxModuleIndex)
		return SCE_NGS_ERROR_INVALID_PARAM;

	SceInt32 slot = rack->moduleIndexMap[moduleIdx];
	if (slot < 0 || slot > rack->numModules)
		return SCE_NGS_ERROR_INVALID_PARAM;

	SceUInt32 expectedSize = rack->stateModuleSizes[slot];
	if (expectedSize != memSize)
		return SCE_NGS_ERROR_SIZE_MISMATCH;

	const void *src = rack->stateBufferBase + rack->stateModuleOffsets[slot] +
			  rack->stateVoiceStride * voiceIdx;

	ngsRackStateLock(rack);
	ngsMemcpy(mem, src, expectedSize);
	ngsRackStateUnlock(rack);

	return SCE_NGS_OK;
}

SceInt32 ngsVoiceGetModuleTypeUser(SceNgsHVoice hVoiceHandle, SceUInt32 moduleIdx,
				   SceNgsModuleID *moduleType)
{
	SceUInt32 voiceIdx;
	NgsRackShared *rack;
	SceInt32 ret = ngsResolveVoiceHandle(hVoiceHandle, &voiceIdx, &rack);

	if (ret != SCE_NGS_OK)
		return ret;

	if (moduleType == NULL || moduleIdx >= rack->maxModuleIndex)
		return SCE_NGS_ERROR_INVALID_PARAM;

	SceInt32 slot = rack->moduleIndexMap[moduleIdx];
	if (slot < 0 || slot > rack->numModules)
		return SCE_NGS_ERROR_INVALID_PARAM;

	*moduleType = rack->moduleTypes[slot];
	return SCE_NGS_OK;
}

SceInt32 ngsVoiceLockParamsUser(SceNgsHVoice hVoiceHandle, SceUInt32 moduleIdx,
				SceUInt16 paramsInterfaceId, SceNgsBufferInfo *paramsBuffer)
{
	SceUInt32 voiceIdx;
	NgsRackShared *rack;
	SceInt32 ret = ngsResolveVoiceHandle(hVoiceHandle, &voiceIdx, &rack);

	if (ret != SCE_NGS_OK)
		return ret;

	if (paramsBuffer == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;

	paramsBuffer->data = NULL;
	paramsBuffer->size = 0;

	if (moduleIdx >= rack->maxModuleIndex)
		return SCE_NGS_ERROR_INVALID_PARAM;

	SceInt32 slot = rack->moduleIndexMap[moduleIdx];
	if (slot < 0 || slot > rack->numModules)
		return SCE_NGS_ERROR_INVALID_PARAM;

	if (paramsInterfaceId != (SceUInt16)rack->moduleTypes[slot])
		return SCE_NGS_ERROR_PARAM_TYPE_MISMATCH;

	ngsRackParamLock(rack);
	SceUInt32 bit = 1U << slot;
	if ((rack->paramLockMask[voiceIdx] & bit) != 0) {
		ret = SCE_NGS_ERROR_RESOURCE_LOCKED;
	} else {
		rack->paramLockMask[voiceIdx] |= bit;
		rack->paramDirtyMask[voiceIdx] &= ~bit;
		paramsBuffer->data = rack->paramBufferBase + rack->paramVoiceStride * voiceIdx +
				     rack->paramModuleOffsets[slot];
		paramsBuffer->size = rack->paramModuleSizes[slot];
	}
	ngsRackParamUnlock(rack);

	return ret;
}

SceInt32 ngsVoiceUnlockParamsUser(SceNgsHVoice hVoiceHandle, SceUInt32 moduleIdx)
{
	SceUInt32 voiceIdx;
	NgsRackShared *rack;
	SceInt32 ret = ngsResolveVoiceHandle(hVoiceHandle, &voiceIdx, &rack);

	if (ret != SCE_NGS_OK)
		return ret;

	NgsSystemShared *sys = ngsResolveSystemFromVoice(hVoiceHandle);
	if (sys == NULL)
		return SCE_NGS_ERROR_INVALID_HANDLE;

	if (moduleIdx >= rack->maxModuleIndex)
		return SCE_NGS_ERROR_INVALID_PARAM;

	SceInt32 slot = rack->moduleIndexMap[moduleIdx];
	if (slot < 0 || slot > rack->numModules)
		return SCE_NGS_ERROR_INVALID_PARAM;

	SceUInt32 bit = 1U << slot;
	if ((rack->paramLockMask[voiceIdx] & bit) == 0)
		return SCE_NGS_ERROR;

	if ((sys->systemFlags & SCE_NGS_SYSTEM_FLAG_MODULE_PARAM_CHECKING) != 0) {
		const SceNgsParamsDescriptor *params = (const SceNgsParamsDescriptor *)(
			rack->paramBufferBase + rack->paramModuleOffsets[slot] +
			rack->paramVoiceStride * voiceIdx);
		SceInt32 checkRet = sceNgsModuleCheckParamsInRangeInternal(
			hVoiceHandle, rack->moduleTypes[slot], params, rack->paramModuleSizes[slot]);
		if (checkRet != SCE_NGS_OK) {
			ngsRackParamLock(rack);
			rack->paramLockMask[voiceIdx] &= ~bit;
			rack->paramDirtyMask[voiceIdx] &= ~bit;
			sceNgsVoiceClearDirtyFlagInternal(hVoiceHandle, bit);
			ngsRackParamUnlock(rack);
			return checkRet;
		}
	}

	ngsRackParamLock(rack);
	rack->paramLockMask[voiceIdx] &= ~bit;
	rack->paramDirtyMask[voiceIdx] |= bit;
	(*rack->dirtyCounter)++;
	ngsRackParamUnlock(rack);

	return SCE_NGS_OK;
}

SceInt32 ngsVoiceSetParamsBlockUser(SceNgsHVoice hVoiceHandle, const SceNgsModuleParamHeader *paramData,
				    SceUInt32 size, SceInt32 *errorCount)
{
	SceUInt32 voiceIdx;
	NgsRackShared *rack;
	SceInt32 ret = ngsResolveVoiceHandle(hVoiceHandle, &voiceIdx, &rack);

	if (ret != SCE_NGS_OK)
		return ret;

	NgsSystemShared *sys = ngsResolveSystemFromVoice(hVoiceHandle);
	if (sys == NULL)
		return SCE_NGS_ERROR_INVALID_HANDLE;

	if (size == 0 || paramData == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;

	const SceUInt8 *base = (const SceUInt8 *)paramData;
	SceUInt32 offset = 0;
	SceInt32 errors = 0;
	SceBool checkParams = (sys->systemFlags & SCE_NGS_SYSTEM_FLAG_MODULE_PARAM_CHECKING) != 0;

	ngsRackParamLock(rack);

	while (offset < size) {
		const SceNgsModuleParamHeader *hdr = (const SceNgsModuleParamHeader *)(base + offset);
		const SceNgsParamsDescriptor *desc = (const SceNgsParamsDescriptor *)(base + offset + 8);
		SceUInt32 bodyOffset = offset + 8;
		SceUInt32 modIdx = (SceUInt32)hdr->moduleId;
		SceInt32 slot;

		if (modIdx >= rack->maxModuleIndex ||
		    (slot = rack->moduleIndexMap[modIdx]) < 0 ||
		    slot > rack->numModules) {
			ret = SCE_NGS_ERROR_INVALID_PARAM;
			errors++;
			offset = bodyOffset + desc->size;
			continue;
		}

		SceUInt32 bit = 1U << slot;
		SceInt32 chan = hdr->chan;

		if ((rack->paramLockMask[voiceIdx] & bit) != 0) {
			ret = SCE_NGS_ERROR_RESOURCE_LOCKED;
			errors++;
			offset = bodyOffset + desc->size;
			continue;
		}

		if ((((SceUInt16)desc->id) != (SceUInt16)rack->moduleTypes[slot]) ||
		    (chan != SCE_NGS_MODULE_ALL_CHANNELS && (chan < 0 || chan >= rack->numChannelsPerVoice))) {
			ret = SCE_NGS_ERROR_INVALID_PARAM;
			errors++;
			offset = bodyOffset + desc->size;
			continue;
		}

		SceUInt32 descSize = desc->size;
		if (descSize == 0) {
			ret = SCE_NGS_ERROR;
			errors++;
			offset = bodyOffset + descSize;
			continue;
		}

		SceUInt32 modParamSize = rack->paramModuleSizes[slot];
		SceInt32 maxSlots = (SceInt32)(modParamSize / descSize);
		SceInt32 startChan;
		SceInt32 endChan;

		if (chan == SCE_NGS_MODULE_ALL_CHANNELS) {
			startChan = 0;
			endChan = (maxSlots >= rack->numChannelsPerVoice) ? rack->numChannelsPerVoice : maxSlots;
		} else {
			if (chan >= maxSlots) {
				ret = SCE_NGS_ERROR;
				errors++;
				offset = bodyOffset + descSize;
				continue;
			}
			startChan = chan;
			endChan = chan + 1;
		}

		SceUInt8 *dstBase = rack->paramBufferBase + voiceIdx * rack->paramVoiceStride +
				    rack->paramModuleOffsets[slot];
		for (SceInt32 ch = startChan; ch < endChan; ch++) {
			if (descSize != 0 && descSize * (SceUInt32)(ch + 1) <= modParamSize) {
				ngsMemcpy(dstBase + descSize * (SceUInt32)ch, desc, descSize);
				descSize = desc->size;
			}
		}

		offset = bodyOffset + descSize;

		if (checkParams) {
			SceInt32 checkRet = sceNgsModuleCheckParamsInRangeInternal(
				hVoiceHandle, rack->moduleTypes[slot],
				(const SceNgsParamsDescriptor *)dstBase, modParamSize);
			if (checkRet != SCE_NGS_OK) {
				ret = checkRet;
				errors++;
				rack->paramDirtyMask[voiceIdx] &= ~bit;
				sceNgsVoiceClearDirtyFlagInternal(hVoiceHandle, bit);
				continue;
			}
		}

		rack->paramDirtyMask[voiceIdx] |= bit;
		(*rack->dirtyCounter)++;
	}

	ngsRackParamUnlock(rack);

	if (errorCount != NULL)
		*errorCount = errors;
	return ret;
}

SceInt32 ngsVoiceApplyPresetUser(SceNgsHVoice hVoiceHandle, const SceNgsVoicePreset *preset)
{
	SceUInt32 sdkVer = SceLibKernel_9F793F84();
	SceInt32 ret = SCE_NGS_OK;

	if (preset->nPresetDataOffset != 0) {
		const SceNgsModuleParamHeader *paramData =
			(const SceNgsModuleParamHeader *)((const SceUInt8 *)preset + preset->nPresetDataOffset);
		ret = ngsVoiceSetParamsBlockUser(hVoiceHandle, paramData, preset->uSizePresetData, NULL);
		if (ret != SCE_NGS_OK)
			return ret;
	}

	if (preset->nBypassFlagsOffset != 0) {
		SceUInt32 alignedCopy[6];
		const SceUInt32 *bypassIds;
		SceUInt32 voiceIdx;
		NgsRackShared *rack;

		sceClibPrintf("Version is [0x%x]\n", sdkVer);

		if ((preset->nBypassFlagsOffset & 3) != 0 && sdkVer >= 0x03500000U) {
			const SceUInt8 *raw = (const SceUInt8 *)preset + preset->nBypassFlagsOffset;
			ngsMemcpy(alignedCopy, raw, 16);
			bypassIds = alignedCopy;
		} else {
			bypassIds = (const SceUInt32 *)((const SceUInt8 *)preset + preset->nBypassFlagsOffset);
		}

		ret = ngsResolveVoiceHandle(hVoiceHandle, &voiceIdx, &rack);
		if (ret == SCE_NGS_OK) {
			SceUInt32 bypassMask = 0;
			for (SceUInt32 i = 0; i < preset->uNumBypassFlags; i++) {
				SceUInt32 modIdx = bypassIds[i];
				if (modIdx < rack->maxModuleIndex) {
					SceInt32 slot = rack->moduleIndexMap[modIdx];
					if (slot >= 0 && slot <= rack->numModules) {
						bypassMask |= (1U << slot);
						continue;
					}
				}
				ret = SCE_NGS_ERROR_INVALID_PARAM;
			}
			if (ret == SCE_NGS_OK)
				ret = sceNgsVoiceSetAllBypassesInternal(hVoiceHandle, bypassMask);
		}
	}

	return ret;
}

SceInt32 ngsVoiceSetPresetUser(SceNgsHVoice hVoiceHandle, const SceNgsVoicePreset *preset)
{
	SceInt32 ret = sceNgsVoiceSetPresetInternal(hVoiceHandle, NULL);
	if (ret == SCE_NGS_OK && preset != NULL)
		return ngsVoiceApplyPresetUser(hVoiceHandle, preset);
	return ret;
}

SceInt32 ngsVoiceInitUser(SceNgsHVoice hVoiceHandle, const SceNgsVoicePreset *preset, SceUInt32 initFlags)
{
	SceInt32 ret = sceNgsVoiceInitInternal(hVoiceHandle, NULL, initFlags);
	if (ret == SCE_NGS_OK) {
		SceUInt32 voiceIdx;
		NgsRackShared *rack;
		if (ngsResolveVoiceHandle(hVoiceHandle, &voiceIdx, &rack) == SCE_NGS_OK) {
			rack->voiceStates[voiceIdx] = SCE_NGS_VOICE_STATE_AVAILABLE;
			rack->voiceUpdateCounts[voiceIdx] = 0;
		}
		if (preset != NULL && (initFlags & SCE_NGS_VOICE_INIT_PRESET) != 0)
			ret = ngsVoiceApplyPresetUser(hVoiceHandle, preset);
	}
	return ret;
}

SceInt32 ngsPatchResetVolumesUser(SceNgsHPatch hPatchHandle)
{
	SceUInt32 voiceIdx;
	SceUInt32 patchIdx;
	NgsRackShared *rack;
	SceInt32 ret = ngsResolvePatchHandle(hPatchHandle, &voiceIdx, &patchIdx, &rack);

	if (ret != SCE_NGS_OK)
		return ret;

	SceFloat32 *volBase = rack->patchVolumes + (patchIdx + rack->patchesPerVoice * voiceIdx) *
			      (2U * (SceUInt32)rack->numChannelsPerVoice);

	ngsRackParamLock(rack);
	volBase[0] = 0.0f;
	volBase[1] = 0.0f;
	if ((SceUInt32)rack->numChannelsPerVoice > 1U) {
		volBase[2] = 0.0f;
		volBase[3] = 0.0f;
	}
	rack->patchDirtyMask[voiceIdx] |= (1U << patchIdx);
	(*rack->dirtyCounter)++;
	return (SceInt32)ngsRackParamUnlock(rack);
}

SceInt32 ngsVoiceMarkPlayUser(SceNgsHVoice hVoiceHandle)
{
	SceUInt32 voiceIdx;
	NgsRackShared *rack;
	SceInt32 ret = ngsResolveVoiceHandle(hVoiceHandle, &voiceIdx, &rack);

	if (ret == SCE_NGS_OK) {
		SceUInt32 state = (rack->voiceStates[voiceIdx] & SCE_NGS_VOICE_STATE_PAUSED) != 0
			? (SCE_NGS_VOICE_STATE_ACTIVE | SCE_NGS_VOICE_STATE_PENDING | SCE_NGS_VOICE_STATE_PAUSED)
			: (SCE_NGS_VOICE_STATE_ACTIVE | SCE_NGS_VOICE_STATE_PENDING);
		rack->voiceStates[voiceIdx] = state;
		rack->voiceUpdateCounts[voiceIdx] = 0;
	}
	return ret;
}

SceInt32 ngsVoiceMarkKillUser(SceNgsHVoice hVoiceHandle)
{
	SceUInt32 voiceIdx;
	NgsRackShared *rack;
	SceInt32 ret = ngsResolveVoiceHandle(hVoiceHandle, &voiceIdx, &rack);

	if (ret == SCE_NGS_OK)
		rack->voiceStates[voiceIdx] &= SCE_NGS_VOICE_STATE_PAUSED;
	return ret;
}

SceInt32 ngsVoiceMarkKeyOffUser(SceNgsHVoice hVoiceHandle)
{
	SceUInt32 voiceIdx;
	NgsRackShared *rack;
	SceInt32 ret = ngsResolveVoiceHandle(hVoiceHandle, &voiceIdx, &rack);

	if (ret == SCE_NGS_OK) {
		SceUInt32 cur = rack->voiceStates[voiceIdx];
		SceUInt32 next = SCE_NGS_VOICE_STATE_FINALIZING | SCE_NGS_VOICE_STATE_KEY_OFF;
		if ((cur & SCE_NGS_VOICE_STATE_PAUSED) != 0)
			next |= SCE_NGS_VOICE_STATE_PAUSED;
		if ((cur & SCE_NGS_VOICE_STATE_PENDING) != 0)
			next |= SCE_NGS_VOICE_STATE_PENDING;
		rack->voiceStates[voiceIdx] = next;
	}
	return ret;
}

SceInt32 ngsVoiceMarkPauseUser(SceNgsHVoice hVoiceHandle)
{
	SceUInt32 voiceIdx;
	NgsRackShared *rack;
	SceInt32 ret = ngsResolveVoiceHandle(hVoiceHandle, &voiceIdx, &rack);

	if (ret == SCE_NGS_OK)
		rack->voiceStates[voiceIdx] |= SCE_NGS_VOICE_STATE_PAUSED;
	return ret;
}

SceInt32 ngsVoiceMarkResumeUser(SceNgsHVoice hVoiceHandle)
{
	SceUInt32 voiceIdx;
	NgsRackShared *rack;
	SceInt32 ret = ngsResolveVoiceHandle(hVoiceHandle, &voiceIdx, &rack);

	if (ret == SCE_NGS_OK)
		rack->voiceStates[voiceIdx] &= ~SCE_NGS_VOICE_STATE_PAUSED;
	return ret;
}

NgsSystemShared *ngsSystemSetFlagsUser(SceNgsHSynSystem hSystemHandle, SceUInt32 flags)
{
	NgsSystemShared *sys = ngsResolveSystemHandle(hSystemHandle);
	if (sys != NULL)
		sys->systemFlags = flags;
	return sys;
}

SceInt32 ngsSystemRegisterUser(SceNgsHSynSystem hSystemHandle, NgsSystemShared *shared)
{
	SceUInt32 sysIdx = NGS_MAX_SYSTEMS;

	if (ngsDecodeSystemHandle(hSystemHandle, &sysIdx) != SCE_NGS_OK ||
	    sysIdx >= NGS_MAX_SYSTEMS ||
	    g_ngsSystemSlots[sysIdx].shared != NULL) {
		return SCE_NGS_ERROR_INVALID_HANDLE;
	}

	g_ngsSystemSlots[sysIdx].processId = sceKernelGetProcessId();
	g_ngsSystemSlots[sysIdx].shared = shared;
	shared->systemFlags = SCE_NGS_SYSTEM_FLAG_DEFAULT;
	return SCE_NGS_OK;
}

SceInt32 ngsSystemUnregisterUser(SceNgsHSynSystem hSystemHandle)
{
	SceUInt32 sysIdx = NGS_MAX_SYSTEMS;

	if (ngsDecodeSystemHandle(hSystemHandle, &sysIdx) != SCE_NGS_OK ||
	    sysIdx >= NGS_MAX_SYSTEMS ||
	    g_ngsSystemSlots[sysIdx].shared == NULL) {
		return SCE_NGS_ERROR_INVALID_HANDLE;
	}

	g_ngsSystemSlots[sysIdx].processId = 0;
	g_ngsSystemSlots[sysIdx].shared = NULL;
	return SCE_NGS_OK;
}

SceInt32 ngsRackRegisterUser(SceNgsHRack hRackHandle, NgsRackShared *shared)
{
	SceUInt32 sysIdx = NGS_MAX_SYSTEMS;
	SceUInt32 rackIdx = 0;

	if (ngsDecodeRackHandle(hRackHandle, &sysIdx, &rackIdx) != 0 ||
	    sysIdx >= NGS_MAX_SYSTEMS ||
	    g_ngsSystemSlots[sysIdx].shared == NULL ||
	    rackIdx >= NGS_MAX_RACKS_PER_SYSTEM) {
		return SCE_NGS_ERROR_INVALID_HANDLE;
	}

	NgsSystemShared *sys = g_ngsSystemSlots[sysIdx].shared;
	if (sys->racks[rackIdx] == NULL) {
		sys->racks[rackIdx] = shared;
		shared->rackHandle = hRackHandle;
		shared->stateSpinLock = 0;
		shared->paramSpinLock = 0;
	}
	return SCE_NGS_OK;
}

SceInt32 ngsRackUnregisterUser(SceNgsHRack hRackHandle)
{
	SceUInt32 sysIdx = NGS_MAX_SYSTEMS;
	SceUInt32 rackIdx = 0;

	if (ngsDecodeRackHandle(hRackHandle, &sysIdx, &rackIdx) != 0 ||
	    sysIdx >= NGS_MAX_SYSTEMS ||
	    g_ngsSystemSlots[sysIdx].shared == NULL ||
	    rackIdx >= NGS_MAX_RACKS_PER_SYSTEM) {
		return SCE_NGS_ERROR_INVALID_HANDLE;
	}

	NgsSystemShared *sys = g_ngsSystemSlots[sysIdx].shared;
	if (sys->racks[rackIdx] == NULL)
		return SCE_NGS_ERROR_INVALID_HANDLE;

	sys->racks[rackIdx] = NULL;
	return SCE_NGS_OK;
}

SceInt32 ngsSystemPullDataUser(SceNgsHSynSystem hSystemHandle)
{
	NgsSystemShared *sys = ngsResolveSystemHandle(hSystemHandle);
	if (sys == NULL)
		return SCE_NGS_OK;

	SceUInt32 dirtyLo = 0;
	SceUInt32 dirtyHi = 0;

	for (SceUInt32 i = 0; i < NGS_MAX_RACKS_PER_SYSTEM; i++) {
		NgsRackShared *rack = sys->racks[i];
		if (rack != NULL) {
			ngsRackParamLock(rack);
			if (*rack->dirtyCounter != 0) {
				if (i >= 32U)
					dirtyHi |= (1U << (i - 32U));
				else
					dirtyLo |= (1U << i);
			}
		}
	}

	sceNgsSystemPullDataInternal(hSystemHandle, dirtyLo, dirtyHi);

	for (SceUInt32 i = 0; i < NGS_MAX_RACKS_PER_SYSTEM; i++) {
		NgsRackShared *rack = sys->racks[i];
		if (rack != NULL) {
			ngsMemset(rack->patchDirtyMask, 0, sizeof(SceUInt32) * rack->numVoices);
			ngsMemset(rack->paramDirtyMask, 0, sizeof(SceUInt32) * rack->numVoices);
			*rack->dirtyCounter = 0;
			ngsRackParamUnlock(rack);
		}
	}

	return SCE_NGS_OK;
}

SceInt32 ngsSystemPushDataUser(SceNgsHSynSystem hSystemHandle)
{
	NgsSystemShared *sys = ngsResolveSystemHandle(hSystemHandle);
	if (sys == NULL)
		return SCE_NGS_OK;

	for (SceUInt32 i = 0; i < NGS_MAX_RACKS_PER_SYSTEM; i++) {
		if (sys->racks[i] != NULL)
			ngsRackStateLock(sys->racks[i]);
	}

	sceNgsSystemPushDataInternal(hSystemHandle);

	for (SceUInt32 i = 0; i < NGS_MAX_RACKS_PER_SYSTEM; i++) {
		if (sys->racks[i] != NULL)
			ngsRackStateUnlock(sys->racks[i]);
	}

	return SCE_NGS_OK;
}

SceInt32 ngsResolvePatchHandle(SceNgsHPatch hPatchHandle, SceUInt32 *outVoiceIdx,
			       SceUInt32 *outPatchIdx, NgsRackShared **outRack)
{
	SceUInt32 sysIdx = hPatchHandle >> 29;
	SceUInt32 rackIdx = (hPatchHandle >> 23) & 0x3FU;
	SceUInt32 voiceIdx = (hPatchHandle >> 15) & 0xFFU;
	SceUInt32 patchIdx = (hPatchHandle >> 7) & 0xFFU;
	SceUInt32 magic = hPatchHandle & NGS_HANDLE_PATCH_MASK;
	NgsSystemShared *sys = g_ngsSystemSlots[sysIdx].shared;

	if (sys == NULL)
		return SCE_NGS_ERROR_INVALID_HANDLE;

	NgsRackShared *rack = sys->racks[rackIdx];
	if (rack == NULL || magic != NGS_HANDLE_PATCH_MAGIC ||
	    voiceIdx >= rack->numVoices || patchIdx >= rack->patchesPerVoice) {
		return SCE_NGS_ERROR_INVALID_HANDLE;
	}

	*outVoiceIdx = voiceIdx;
	*outPatchIdx = patchIdx;
	*outRack = rack;
	return SCE_NGS_OK;
}

SceInt32 ngsResolveVoiceHandle(SceNgsHVoice hVoiceHandle, SceUInt32 *outVoiceIdx,
			       NgsRackShared **outRack)
{
	SceUInt32 sysIdx = hVoiceHandle >> 29;
	SceUInt32 rackIdx = (hVoiceHandle >> 23) & 0x3FU;
	SceUInt32 voiceIdx = (hVoiceHandle >> 15) & 0xFFU;
	SceUInt32 magic = hVoiceHandle & NGS_HANDLE_VOICE_MASK;
	NgsSystemShared *sys = g_ngsSystemSlots[sysIdx].shared;

	if (sys == NULL)
		return SCE_NGS_ERROR_INVALID_HANDLE;

	NgsRackShared *rack = sys->racks[rackIdx];
	if (rack == NULL || magic != NGS_HANDLE_VOICE_MAGIC || voiceIdx >= rack->numVoices)
		return SCE_NGS_ERROR_INVALID_HANDLE;

	*outVoiceIdx = voiceIdx;
	*outRack = rack;
	return SCE_NGS_OK;
}

NgsSystemShared *ngsResolveSystemHandle(SceNgsHSynSystem hSystemHandle)
{
	if ((hSystemHandle & NGS_HANDLE_SYSTEM_MASK) != NGS_HANDLE_SYSTEM_MAGIC)
		return NULL;
	return g_ngsSystemSlots[hSystemHandle >> 29].shared;
}

NgsSystemShared *ngsResolveSystemFromVoice(SceNgsHVoice hVoiceHandle)
{
	if ((hVoiceHandle & NGS_HANDLE_VOICE_MASK) != NGS_HANDLE_VOICE_MAGIC)
		return NULL;
	return g_ngsSystemSlots[hVoiceHandle >> 29].shared;
}

SceInt32 ngsDecodeSystemHandle(SceNgsHSynSystem hSystemHandle, SceUInt32 *outSysIdx)
{
	if ((hSystemHandle & NGS_HANDLE_SYSTEM_MASK) != NGS_HANDLE_SYSTEM_MAGIC)
		return SCE_NGS_ERROR_INVALID_HANDLE;
	*outSysIdx = hSystemHandle >> 29;
	return SCE_NGS_OK;
}

SceInt32 ngsDecodeRackHandle(SceNgsHRack hRackHandle, SceUInt32 *outSysIdx, SceUInt32 *outRackIdx)
{
	if ((hRackHandle & NGS_HANDLE_RACK_MASK) != NGS_HANDLE_RACK_MAGIC)
		return SCE_NGS_ERROR_INVALID_HANDLE;
	*outSysIdx = hRackHandle >> 29;
	*outRackIdx = (hRackHandle >> 23) & 0x3FU;
	return SCE_NGS_OK;
}

SceUInt32 ngsRackParamLock(NgsRackShared *rack)
{
	volatile SceUInt32 *lock = &rack->paramSpinLock;
	for (SceInt32 retries = 100001; retries > 0; retries--) {
		for (;;) {
			if (ngsAtomicLoad32(lock) != 0)
				break;
			if (ngsAtomicStore32(1U, lock) == 0)
				return 0;
		}
		if (retries == 1)
			break;
		sceKernelDelayThread(10);
	}
	return (SceUInt32)-1;
}

SceUInt32 ngsRackParamUnlock(NgsRackShared *rack)
{
	volatile SceUInt32 *lock = &rack->paramSpinLock;
	for (;;) {
		if (ngsAtomicLoad32(lock) != 1U)
			return (SceUInt32)-1;
		if (ngsAtomicStore32(0U, lock) == 0)
			return 0;
	}
}

SceUInt32 ngsRackStateLock(NgsRackShared *rack)
{
	volatile SceUInt32 *lock = &rack->stateSpinLock;
	for (SceInt32 retries = 100001; retries > 0; retries--) {
		for (;;) {
			if (ngsAtomicLoad32(lock) != 0)
				break;
			if (ngsAtomicStore32(1U, lock) == 0)
				return 0;
		}
		if (retries == 1)
			break;
		sceKernelDelayThread(10);
	}
	return (SceUInt32)-1;
}

SceUInt32 ngsRackStateUnlock(NgsRackShared *rack)
{
	volatile SceUInt32 *lock = &rack->stateSpinLock;
	for (;;) {
		if (ngsAtomicLoad32(lock) != 1U)
			return (SceUInt32)-1;
		if (ngsAtomicStore32(0U, lock) == 0)
			return 0;
	}
}
