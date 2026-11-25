/*
 * SceNgsUser public API exports and Bugzilla 89940 system lock management.
 */
#include "ngs_internal.h"

NgsSystemLockSlot g_ngsSystemLocks[NGS_MAX_SYSTEMS];
NgsSulphaObjectNotifyFn g_ngsSulphaObjectNotify;
NgsSulphaApiTraceFn g_ngsSulphaApiTrace;

static SceInt32 ngsFindSystemLockSlot(SceNgsHSynSystem hSystemHandle)
{
	for (SceInt32 i = 0; i < NGS_MAX_SYSTEMS; i++) {
		if (g_ngsSystemLocks[i].systemHandle == hSystemHandle)
			return i;
	}
	return -1;
}

static SceInt32 ngsFindFreeSystemLockSlot(void)
{
	for (SceInt32 i = 0; i < NGS_MAX_SYSTEMS; i++) {
		if (g_ngsSystemLocks[i].systemHandle == SCE_NGS_INVALID_HANDLE)
			return i;
	}
	return -1;
}

SceInt32 sceNgsSystemGetRequiredMemorySize(const SceNgsSystemInitParams *pSynthParams, SceUInt32 *pnSize)
{
	SceInt32 ret = sceNgsSystemGetRequiredMemorySizeInternal(pSynthParams, pnSize);
	if (g_ngsSulphaApiTrace != NULL) {
		SceUInt32 sizeVal = (pnSize != NULL) ? *pnSize : 0U;
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[0], ret, pSynthParams, sizeVal);
	}
	return ret;
}

SceInt32 sceNgsSystemInit(void *pSynthSysMemory, const SceUInt32 uMemSize,
			  const SceNgsSystemInitParams *pSynthParams, SceNgsHSynSystem *pSystemHandle)
{
	SceNgsBufferInfo memBuf;
	SceUInt32 sdkVer = SceLibKernel_9F793F84();
	memBuf.data = pSynthSysMemory;
	memBuf.size = uMemSize;

	SceInt32 ret = sceNgsSystemInitInternal(&memBuf, sdkVer, pSynthParams, pSystemHandle);
	if (ret == SCE_NGS_OK && pSystemHandle != NULL) {
		if (g_ngsSulphaObjectNotify != NULL) {
			NgsSulphaRegistration reg;
			reg.type = NGS_SULPHA_REG_SYSTEM;
			reg.handle = *pSystemHandle;
			reg.isCreate = 1;
			g_ngsSulphaObjectNotify(&reg);
		}

		ngsSystemRegisterUser(*pSystemHandle, (NgsSystemShared *)pSynthSysMemory);

		SceNgsHSynSystem hSys = *pSystemHandle;
		if (sceNgsSystemIsFixForBugzilla89940(hSys) != 0) {
			SceInt32 slot = ngsFindFreeSystemLockSlot();
			if (slot < 0 ||
			    sceKernelCreateLwMutex(&g_ngsSystemLocks[slot].lwMutex,
						   "NGS System Processing Lock", 2U, 0, NULL) != 0) {
				ret = SCE_NGS_ERROR;
				if (sceNgsSystemReleaseInternal((SceNgsHSynSystem)(SceUIntPtr)pSystemHandle) == 0)
					ngsSystemUnregisterUser(*pSystemHandle);
			} else {
				g_ngsSystemLocks[slot].systemHandle = hSys;
			}
		}
	}

	if (g_ngsSulphaApiTrace != NULL) {
		SceNgsHSynSystem outHandle = (pSystemHandle != NULL) ? *pSystemHandle : SCE_NGS_INVALID_HANDLE;
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[1], ret, pSynthSysMemory, uMemSize,
				    pSynthParams, outHandle);
	}
	return ret;
}

SceInt32 sceNgsSystemUpdate(SceNgsHSynSystem hSystemHandle)
{
	if (sceNgsSystemIsFixForBugzilla89940(hSystemHandle) != 0) {
		SceInt32 slot = ngsFindSystemLockSlot(hSystemHandle);
		if (slot < 0 || sceKernelLockLwMutex(&g_ngsSystemLocks[slot].lwMutex, 1, NULL) != 0)
			return SCE_NGS_ERROR;
	}

	ngsSystemPullDataUser(hSystemHandle);
	SceInt32 ret = sceNgsSystemUpdateInternal(hSystemHandle);
	ngsSystemPushDataUser(hSystemHandle);

	if (sceNgsSystemIsFixForBugzilla89940(hSystemHandle) != 0) {
		SceInt32 slot = ngsFindSystemLockSlot(hSystemHandle);
		if (slot < 0 || sceKernelUnlockLwMutex(&g_ngsSystemLocks[slot].lwMutex, 1) != 0)
			return SCE_NGS_ERROR;
	}

	NgsSulphaSynthUpdateFn updateCb = NULL;
	union {
		SceNgsBufferInfo buf;
		NgsCallbackEntry *cbList;
	} shared;
	if (sceNgsSulphaGetSynthUpdateCallbackInternal(hSystemHandle, &updateCb, &shared.buf) == 0)
		updateCb(&shared.buf);

	SceUInt32 unusedCount = 0;
	SceInt32 cbCount = sceNgsSystemGetCallbackListInternal(hSystemHandle, &shared.cbList, &unusedCount);
	if (cbCount > 0) {
		for (SceInt32 i = 0; i < cbCount; i++) {
			if (shared.cbList[i].callback != NULL)
				shared.cbList[i].callback(shared.cbList[i].info);
		}
	}

	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[2], ret, hSystemHandle);
	return ret;
}

SceInt32 sceNgsSystemRelease(SceNgsHSynSystem hSystemHandle)
{
	SceInt32 isBz89940 = sceNgsSystemIsFixForBugzilla89940(hSystemHandle);
	SceInt32 ret = sceNgsSystemReleaseInternal(hSystemHandle);

	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[3], ret, hSystemHandle);

	if (ret == SCE_NGS_OK) {
		if (g_ngsSulphaObjectNotify != NULL) {
			NgsSulphaRegistration reg;
			reg.type = NGS_SULPHA_REG_SYSTEM;
			reg.handle = hSystemHandle;
			reg.isCreate = 0;
			g_ngsSulphaObjectNotify(&reg);
		}
		ngsSystemUnregisterUser(hSystemHandle);

		if (isBz89940 == 1) {
			SceInt32 slot = ngsFindSystemLockSlot(hSystemHandle);
			if (slot >= 0 && sceKernelDeleteLwMutex(&g_ngsSystemLocks[slot].lwMutex) == 0)
				g_ngsSystemLocks[slot].systemHandle = SCE_NGS_INVALID_HANDLE;
		}
	}
	return ret;
}

SceInt32 sceNgsSystemLock(SceNgsHSynSystem hSystemHandle)
{
	SceInt32 ret = sceNgsSystemLockInternal(hSystemHandle);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[4], ret, hSystemHandle);
	return ret;
}

SceInt32 sceNgsSystemUnlock(SceNgsHSynSystem hSystemHandle)
{
	SceInt32 ret = sceNgsSystemUnlockInternal(hSystemHandle);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[5], ret, hSystemHandle);
	return ret;
}

SceInt32 sceNgsSystemSetParamErrorCallback(SceNgsHSynSystem hSystemHandle,
					   const SceNgsParamsErrorCallbackFunc callbackFuncPtr)
{
	SceInt32 ret = sceNgsSystemSetParamErrorCallbackInternal(hSystemHandle, callbackFuncPtr);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[6], ret, hSystemHandle, callbackFuncPtr);
	return ret;
}

SceInt32 sceNgsSystemSetFlags(SceNgsHSynSystem hSystemHandle, const SceUInt32 uSystemFlags)
{
	SceInt32 ret = sceNgsSystemSetFlagsInternal(hSystemHandle, uSystemFlags);
	if (ret == SCE_NGS_OK)
		ngsSystemSetFlagsUser(hSystemHandle, uSystemFlags);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[43], ret, hSystemHandle, uSystemFlags);
	return ret;
}

SceInt32 ngsSetSulphaObjectNotify(NgsSulphaObjectNotifyFn fn)
{
	g_ngsSulphaObjectNotify = fn;
	return 0;
}

SceInt32 ngsSetSulphaApiTrace(NgsSulphaApiTraceFn fn)
{
	g_ngsSulphaApiTrace = fn;
	return 0;
}

SceInt32 ngsGetApiTraceTable(const NgsSulphaApiTraceEventDesc **outTable, SceUInt8 *outCount)
{
	if (outTable == NULL || outCount == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;
	*outTable = g_ngsApiTraceEvents;
	*outCount = 63;
	return SCE_NGS_OK;
}

SceInt32 ngsSulphaSetSynthUpdateCallback(SceNgsHSynSystem hSystemHandle,
					 NgsSulphaSynthUpdateFn callback, SceNgsBufferInfo *pBuf)
{
	return sceNgsSulphaSetSynthUpdateCallbackInternal(hSystemHandle, callback, pBuf);
}

SceInt32 ngsSulphaGetModuleList(SceUInt32 *moduleIds, SceUInt32 maxCount, SceUInt32 *outCount)
{
	return sceNgsSulphaGetModuleListInternal(moduleIds, maxCount, outCount);
}

SceInt32 ngsSulphaGetInfo(const NgsSulphaRegistration *reg, SceNgsBufferInfo *pBuf)
{
	return sceNgsSulphaGetInfoInternal(reg, pBuf);
}

SceInt32 ngsSulphaQueryModule(SceUInt32 queryType, void *queryBuf)
{
	return sceNgsSulphaQueryModuleInternal(queryType, queryBuf);
}

SceInt32 sceNgsModuleGetNumPresets(SceNgsHSynSystem hSystemHandle, const SceNgsModuleID uModuleID,
				   SceUInt32 *puNumPresets)
{
	SceInt32 ret = sceNgsModuleGetNumPresetsInternal(hSystemHandle, uModuleID, puNumPresets);
	if (g_ngsSulphaApiTrace != NULL) {
		SceUInt32 num = (puNumPresets != NULL) ? *puNumPresets : 0U;
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[9], ret, hSystemHandle, uModuleID, num);
	}
	return ret;
}

SceInt32 sceNgsModuleGetPreset(SceNgsHSynSystem hSystemHandle, const SceNgsModuleID uModuleID,
			       const SceUInt32 uPresetIndex, SceNgsBufferInfo *pParamsBuffer)
{
	SceInt32 ret = sceNgsModuleGetPresetInternal(hSystemHandle, uModuleID, uPresetIndex, pParamsBuffer);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[10], ret, hSystemHandle, uModuleID, uPresetIndex, pParamsBuffer);
	return ret;
}

SceInt32 sceNgsRackGetRequiredMemorySize(SceNgsHSynSystem hSystemHandle,
					 const SceNgsRackDescription *pRackDesc, SceUInt32 *pnSize)
{
	SceInt32 ret = sceNgsRackGetRequiredMemorySizeInternal(hSystemHandle, pRackDesc, pnSize);
	if (g_ngsSulphaApiTrace != NULL) {
		SceUInt32 sizeVal = (pnSize != NULL) ? *pnSize : 0U;
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[11], ret, hSystemHandle, pRackDesc, sizeVal);
	}
	return ret;
}

SceInt32 sceNgsRackInit(SceNgsHSynSystem hSystemHandle, SceNgsBufferInfo *pRackBuffer,
			const SceNgsRackDescription *pRackDesc, SceNgsHRack *pRackHandle)
{
	SceInt32 ret = (pRackBuffer != NULL) ? SCE_NGS_OK : SCE_NGS_ERROR_INVALID_PARAM;
	SceBool initialized = SCE_FALSE;

	if (sceNgsSystemIsFixForBugzilla89940(hSystemHandle) != 0) {
		SceInt32 slot = ngsFindSystemLockSlot(hSystemHandle);
		if (slot < 0 || sceKernelLockLwMutex(&g_ngsSystemLocks[slot].lwMutex, 1, NULL) != 0)
			return SCE_NGS_ERROR;
	}

	if (ret == SCE_NGS_OK) {
		ret = sceNgsRackInitInternal(hSystemHandle, pRackBuffer, pRackDesc, pRackHandle);
		if (ret == SCE_NGS_OK) {
			initialized = SCE_TRUE;
			if (g_ngsSulphaObjectNotify != NULL && pRackHandle != NULL) {
				NgsSulphaRegistration reg;
				reg.type = NGS_SULPHA_REG_RACK;
				reg.handle = *pRackHandle;
				reg.isCreate = 1;
				g_ngsSulphaObjectNotify(&reg);
			}
		}
	}

	if (g_ngsSulphaApiTrace != NULL) {
		SceNgsHRack outHandle = (pRackHandle != NULL) ? *pRackHandle : SCE_NGS_INVALID_HANDLE;
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[12], ret, hSystemHandle, pRackBuffer, pRackDesc, outHandle);
	}

	if (initialized && pRackHandle != NULL)
		ngsRackRegisterUser(*pRackHandle, (NgsRackShared *)pRackBuffer->data);

	if (sceNgsSystemIsFixForBugzilla89940(hSystemHandle) != 0) {
		SceInt32 slot = ngsFindSystemLockSlot(hSystemHandle);
		if (slot < 0 || sceKernelUnlockLwMutex(&g_ngsSystemLocks[slot].lwMutex, 1) != 0)
			return SCE_NGS_ERROR;
	}

	return ret;
}

SceInt32 sceNgsRackGetVoiceHandle(SceNgsHRack hRackHandle, const SceUInt32 uIndex,
				  SceNgsHVoice *pVoiceHandle)
{
	SceInt32 ret = sceNgsRackGetVoiceHandleInternal(hRackHandle, uIndex, pVoiceHandle);
	if (g_ngsSulphaApiTrace != NULL) {
		SceNgsHVoice outHandle = (pVoiceHandle != NULL) ? *pVoiceHandle : SCE_NGS_INVALID_HANDLE;
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[13], ret, hRackHandle, uIndex, outHandle);
	}
	return ret;
}

SceInt32 sceNgsRackRelease(SceNgsHRack hRackHandle,
			   const SceNgsRackReleaseCallbackFunc callbackFuncPtr)
{
	SceNgsHSynSystem hSys = sceNgsSystemGetSysHandleFromRack(hRackHandle);

	if (sceNgsSystemIsFixForBugzilla89940(hSys) != 0) {
		SceInt32 slot = ngsFindSystemLockSlot(hSys);
		if (slot < 0 || sceKernelLockLwMutex(&g_ngsSystemLocks[slot].lwMutex, 1, NULL) != 0)
			return SCE_NGS_ERROR;
	}

	SceInt32 ret = sceNgsRackReleaseInternal(hRackHandle, callbackFuncPtr);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[14], ret, hRackHandle, callbackFuncPtr);

	if (ret == SCE_NGS_OK) {
		if (g_ngsSulphaObjectNotify != NULL) {
			NgsSulphaRegistration reg;
			reg.type = NGS_SULPHA_REG_RACK;
			reg.handle = hRackHandle;
			reg.isCreate = 0;
			g_ngsSulphaObjectNotify(&reg);
		}
		ngsRackUnregisterUser(hRackHandle);
	}

	if (sceNgsSystemIsFixForBugzilla89940(hSys) != 0) {
		SceInt32 slot = ngsFindSystemLockSlot(hSys);
		if (slot < 0 || sceKernelUnlockLwMutex(&g_ngsSystemLocks[slot].lwMutex, 1) != 0)
			return SCE_NGS_ERROR;
	}

	return ret;
}

SceInt32 sceNgsRackSetParamErrorCallback(SceNgsHRack hRackHandle,
					 const SceNgsParamsErrorCallbackFunc callbackFuncPtr)
{
	SceInt32 ret = sceNgsRackSetParamErrorCallbackInternal(hRackHandle, callbackFuncPtr);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[15], ret, hRackHandle, callbackFuncPtr);
	return ret;
}

SceInt32 sceNgsVoiceInit(SceNgsHVoice hVoiceHandle, const SceNgsVoicePreset *pPreset,
			 const SceUInt32 uInitFlags)
{
	SceInt32 ret = ngsVoiceInitUser(hVoiceHandle, pPreset, uInitFlags);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[16], ret, hVoiceHandle, pPreset, uInitFlags);
	return ret;
}

SceInt32 sceNgsVoicePlay(SceNgsHVoice hVoiceHandle)
{
	SceInt32 ret = sceNgsVoicePlayInternal(hVoiceHandle);
	if (ret == SCE_NGS_OK)
		ngsVoiceMarkPlayUser(hVoiceHandle);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[17], ret, hVoiceHandle);
	return ret;
}

SceInt32 sceNgsVoiceKeyOff(SceNgsHVoice hVoiceHandle)
{
	SceUInt32 sdkVer = SceLibKernel_9F793F84();
	SceInt32 ret = sceNgsVoiceKeyOffInternal(hVoiceHandle);

	if (ret != SCE_NGS_OK) {
		if (sdkVer >= 0x02100000U && ret == SCE_NGS_ERROR_INVALID_STATE)
			ret = SCE_NGS_OK;
	} else {
		ngsVoiceMarkKeyOffUser(hVoiceHandle);
	}

	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[18], ret, hVoiceHandle);
	return ret;
}

SceInt32 sceNgsVoiceKill(SceNgsHVoice hVoiceHandle)
{
	SceInt32 ret = sceNgsVoiceKillInternal(hVoiceHandle);
	if (ret == SCE_NGS_OK)
		ngsVoiceMarkKillUser(hVoiceHandle);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[19], ret, hVoiceHandle);
	return ret;
}

SceInt32 sceNgsVoicePause(SceNgsHVoice hVoiceHandle)
{
	SceInt32 ret = sceNgsVoicePauseInternal(hVoiceHandle);
	if (ret == SCE_NGS_OK)
		ngsVoiceMarkPauseUser(hVoiceHandle);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[20], ret, hVoiceHandle);
	return ret;
}

SceInt32 sceNgsVoiceResume(SceNgsHVoice hVoiceHandle)
{
	SceInt32 ret = sceNgsVoiceResumeInternal(hVoiceHandle);
	if (ret == SCE_NGS_OK)
		ngsVoiceMarkResumeUser(hVoiceHandle);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[21], ret, hVoiceHandle);
	return ret;
}

SceInt32 sceNgsVoiceSetPreset(SceNgsHVoice hVoiceHandle, const SceNgsVoicePreset *pVoicePreset)
{
	SceInt32 ret = ngsVoiceSetPresetUser(hVoiceHandle, pVoicePreset);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[22], ret, hVoiceHandle, pVoicePreset);
	return ret;
}

SceInt32 sceNgsVoiceLockParams(SceNgsHVoice hVoiceHandle, const SceUInt32 uModule,
			       const SceNgsParamsID uParamsInterfaceId, SceNgsBufferInfo *pParamsBuffer)
{
	SceInt32 ret;
	if (pParamsBuffer != NULL)
		ret = ngsVoiceLockParamsUser(hVoiceHandle, uModule, (SceUInt16)uParamsInterfaceId, pParamsBuffer);
	else
		ret = SCE_NGS_ERROR_INVALID_PARAM;

	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[23], ret, hVoiceHandle, uModule, uParamsInterfaceId, pParamsBuffer);
	return ret;
}

SceInt32 sceNgsVoiceUnlockParams(SceNgsHVoice hVoiceHandle, const SceUInt32 uModule)
{
	SceInt32 ret = ngsVoiceUnlockParamsUser(hVoiceHandle, uModule);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[24], ret, hVoiceHandle, uModule);
	return ret;
}

SceInt32 sceNgsVoiceSetParamsBlock(SceNgsHVoice hVoiceHandle, const SceNgsModuleParamHeader *pParamData,
				   const SceUInt32 uSize, SceInt32 *pnErrorCount)
{
	SceInt32 ret = ngsVoiceSetParamsBlockUser(hVoiceHandle, pParamData, uSize, pnErrorCount);
	if (g_ngsSulphaApiTrace != NULL) {
		SceInt32 errVal = (pnErrorCount != NULL) ? *pnErrorCount : 0;
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[25], ret, hVoiceHandle, pParamData, uSize, errVal);
	}
	return ret;
}

SceInt32 sceNgsVoiceBypassModule(SceNgsHVoice hVoiceHandle, const SceUInt32 uModule,
				 const SceUInt32 uBypassFlag)
{
	SceInt32 ret = sceNgsVoiceBypassModuleInternal(hVoiceHandle, uModule, uBypassFlag);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[26], ret, hVoiceHandle, uModule, uBypassFlag);
	return ret;
}

SceInt32 sceNgsVoiceSetModuleCallback(SceNgsHVoice hVoiceHandle, const SceUInt32 uModule,
				      const SceNgsModuleCallbackFunc callbackFuncPtr, void *pUserData)
{
	SceInt32 ret = sceNgsVoiceSetModuleCallbackInternal(hVoiceHandle, uModule, callbackFuncPtr, pUserData);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[27], ret, hVoiceHandle, uModule, callbackFuncPtr, pUserData);
	return ret;
}

SceInt32 sceNgsVoiceSetFinishedCallback(SceNgsHVoice hVoiceHandle,
					const SceNgsCallbackFunc callbackFuncPtr, void *pUserData)
{
	SceInt32 ret = sceNgsVoiceSetFinishedCallbackInternal(hVoiceHandle, callbackFuncPtr, pUserData);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[28], ret, hVoiceHandle, callbackFuncPtr, pUserData);
	return ret;
}

SceInt32 sceNgsVoiceGetStateData(SceNgsHVoice hVoiceHandle, const SceUInt32 uModule, void *pMem,
				 const SceUInt32 uMemSize)
{
	SceInt32 ret = ngsVoiceGetStateDataUser(hVoiceHandle, uModule, pMem, uMemSize);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[29], ret, hVoiceHandle, uModule, pMem, uMemSize);
	return ret;
}

SceInt32 sceNgsVoiceGetInfo(SceNgsHVoice hVoiceHandle, SceNgsVoiceInfo *pInfo)
{
	SceInt32 ret = ngsVoiceGetInfoUser(hVoiceHandle, pInfo);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[30], ret, hVoiceHandle, pInfo);
	return ret;
}

SceInt32 sceNgsVoiceGetModuleType(SceNgsHVoice hVoiceHandle, const SceUInt32 uModule,
				  SceNgsModuleID *pModuleType)
{
	SceInt32 ret = ngsVoiceGetModuleTypeUser(hVoiceHandle, uModule, pModuleType);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[31], ret, hVoiceHandle, uModule, pModuleType);
	return ret;
}

SceInt32 sceNgsVoiceGetModuleBypass(SceNgsHVoice hVoiceHandle, const SceUInt32 uModule,
				    SceUInt32 *puBypassFlag)
{
	SceInt32 ret = sceNgsVoiceGetModuleBypassInternal(hVoiceHandle, uModule, puBypassFlag);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[32], ret, hVoiceHandle, uModule, puBypassFlag);
	return ret;
}

SceInt32 sceNgsVoiceGetParamsOutOfRange(SceNgsHVoice hVoiceHandle, const SceUInt32 uModule,
					char *pszMessageBuffer)
{
	SceInt32 ret = sceNgsVoiceGetParamsOutOfRangeBufferedInternal(hVoiceHandle, uModule, pszMessageBuffer);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[33], ret, hVoiceHandle, uModule, pszMessageBuffer);
	return ret;
}

SceInt32 sceNgsPatchCreateRouting(const SceNgsPatchSetupInfo *pPatchInfo, SceNgsHPatch *pPatchHandle)
{
	SceInt32 ret = sceNgsPatchCreateRoutingInternal(pPatchInfo, pPatchHandle);
	if (ret == SCE_NGS_OK && pPatchHandle != NULL)
		ngsPatchResetVolumesUser(*pPatchHandle);
	if (g_ngsSulphaApiTrace != NULL) {
		SceNgsHPatch outHandle = (pPatchHandle != NULL) ? *pPatchHandle : SCE_NGS_INVALID_HANDLE;
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[34], ret, pPatchInfo, outHandle);
	}
	return ret;
}

SceInt32 sceNgsPatchGetInfo(SceNgsHPatch hPatchHandle, SceNgsPatchRouteInfo *pRouteInfo,
			    SceNgsPatchSetupInfo *pSetup)
{
	SceInt32 ret = ngsPatchGetInfoUser(hPatchHandle, pRouteInfo, pSetup);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[35], ret, hPatchHandle, pRouteInfo, pSetup);
	return ret;
}

SceInt32 sceNgsVoiceGetOutputPatch(SceNgsHVoice hVoiceHandle, const SceInt32 nOutputIndex,
				   const SceInt32 nSubIndex, SceNgsHPatch *pPatchHandle)
{
	SceInt32 ret = sceNgsVoiceGetOutputPatchInternal(hVoiceHandle, nOutputIndex, nSubIndex, pPatchHandle);
	if (g_ngsSulphaApiTrace != NULL) {
		SceNgsHPatch outHandle = (pPatchHandle != NULL) ? *pPatchHandle : SCE_NGS_INVALID_HANDLE;
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[35], ret, hVoiceHandle, nOutputIndex, nSubIndex, outHandle);
	}
	return ret;
}

SceInt32 sceNgsPatchRemoveRouting(SceNgsHPatch hPatchHandle)
{
	SceInt32 ret = sceNgsPatchRemoveRoutingInternal(hPatchHandle);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[37], ret, hPatchHandle);
	return ret;
}

SceInt32 sceNgsVoicePatchSetVolume(SceNgsHPatch hPatchHandle, const SceInt32 nOutputChannel,
				   const SceInt32 nInputChannel, const SceFloat32 fVol)
{
	SceInt32 ret = ngsPatchSetVolumeUser(hPatchHandle, nOutputChannel, nInputChannel, fVol);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[38], ret, hPatchHandle, nOutputChannel, nInputChannel, (double)fVol);
	return ret;
}

SceInt32 sceNgsVoicePatchSetVolumes(SceNgsHPatch hPatchHandle, const SceInt32 nOutputChannel,
				    const SceFloat32 *pVolumes, const SceInt32 nVols)
{
	SceInt32 ret = ngsPatchSetVolumesUser(hPatchHandle, nOutputChannel, pVolumes, nVols);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[39], ret, hPatchHandle, nOutputChannel, pVolumes, nVols);
	return ret;
}

SceInt32 sceNgsVoicePatchSetVolumesMatrix(SceNgsHPatch hPatchHandle, const SceNgsVolumeMatrix *pMatrix)
{
	SceInt32 ret = ngsPatchSetVolumesMatrixUser(hPatchHandle, pMatrix);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[40], ret, hPatchHandle, pMatrix);
	return ret;
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetCompressorBuss(void)
{
	const struct SceNgsVoiceDefinition *def = sceNgsVoiceDefGetCompressorBussInternal();
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[44], def);
	return def;
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetCompressorSideChainBuss(void)
{
	const struct SceNgsVoiceDefinition *def = sceNgsVoiceDefGetCompressorSideChainBussInternal();
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[45], def);
	return def;
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetDistortionBuss(void)
{
	const struct SceNgsVoiceDefinition *def = sceNgsVoiceDefGetDistortionBussInternal();
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[47], def);
	return def;
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetEnvelopeBuss(void)
{
	const struct SceNgsVoiceDefinition *def = sceNgsVoiceDefGetEnvelopeBussInternal();
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[48], def);
	return def;
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetEqBuss(void)
{
	const struct SceNgsVoiceDefinition *def = sceNgsVoiceDefGetEqBussInternal();
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[49], def);
	return def;
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetMasterBuss(void)
{
	const struct SceNgsVoiceDefinition *def = sceNgsVoiceDefGetMasterBussInternal();
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[50], def);
	return def;
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetMixerBuss(void)
{
	const struct SceNgsVoiceDefinition *def = sceNgsVoiceDefGetMixerBussInternal();
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[51], def);
	return def;
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetPauserBuss(void)
{
	const struct SceNgsVoiceDefinition *def = sceNgsVoiceDefGetPauserBussInternal();
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[52], def);
	return def;
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetReverbBuss(void)
{
	const struct SceNgsVoiceDefinition *def = sceNgsVoiceDefGetReverbBussInternal();
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[53], def);
	return def;
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetSimpleVoice(void)
{
	const struct SceNgsVoiceDefinition *def = sceNgsVoiceDefGetSimpleVoiceInternal();
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[54], def);
	return def;
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetTemplate1(void)
{
	const struct SceNgsVoiceDefinition *def = sceNgsVoiceDefGetTemplate1Internal();
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[55], def);
	return def;
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetDelayBuss(void)
{
	const struct SceNgsVoiceDefinition *def = sceNgsVoiceDefGetDelayBussInternal();
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[46], def);
	return def;
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetSasEmuVoice(void)
{
	const struct SceNgsVoiceDefinition *def = sceNgsVoiceDefGetSasEmuVoiceInternal();
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[56], def);
	return def;
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetAtrac9Voice(void)
{
	const struct SceNgsVoiceDefinition *def = sceNgsVoiceDefGetAtrac9VoiceInternal();
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[57], def);
	return def;
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetSimpleAtrac9Voice(void)
{
	const struct SceNgsVoiceDefinition *def = sceNgsVoiceDefGetSimpleAtrac9VoiceInternal();
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[58], def);
	return def;
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetScreamVoice(void)
{
	const struct SceNgsVoiceDefinition *def = sceNgsVoiceDefGetScreamVoiceInternal();
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[59], def);
	return def;
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetScreamAtrac9Voice(void)
{
	const struct SceNgsVoiceDefinition *def = sceNgsVoiceDefGetScreamVoiceAT9Internal();
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[60], def);
	return def;
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetPitchShiftBuss(void)
{
	const struct SceNgsVoiceDefinition *def = sceNgsVoiceDefGetPitchshiftBussInternal();
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[62], def);
	return def;
}

SceInt32 sceNgsAT9GetSectionDetails(const SceInt32 nStartSampleOffset, const SceInt32 nNumSamples,
				    const SceInt32 nConfigData, SceNgsAT9SkipBufferInfo *pAt9InfoBuffer)
{
	SceInt32 ret = ngsAt9GetSectionDetailsImpl(nStartSampleOffset, nNumSamples, nConfigData, pAt9InfoBuffer);
	if (g_ngsSulphaApiTrace != NULL)
		g_ngsSulphaApiTrace(&g_ngsApiTraceEvents[61], ret, nStartSampleOffset, nNumSamples,
				    nConfigData, pAt9InfoBuffer);
	return ret;
}
