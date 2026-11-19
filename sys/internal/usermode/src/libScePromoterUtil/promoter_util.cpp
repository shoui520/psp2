/*
 * ScePromoterUtil (vs0:sys/external/libScePromoterUtil.suprx): the application installer's client
 * (see promoter_util_internal.h).
 */
#include "promoter_util_internal.h"

static PromoterUtilPropertyIds promoterUtilPropertyIds;

extern "C" SceInt32 scePromoterUtilityInit(void)
{
	promoterUtilIpcInit(SceShellSvc_B31E7F1C());
	return 0;
}

extern "C" SceInt32 scePromoterUtilityExit(void)
{
	int state;
	int value;
	promoterUtilIpcExit(SceShellSvc_B31E7F1C());
	return 0;
}

/* waits for the started work: the state is 0 when it is done */
static inline __attribute__((always_inline)) int promoterUtilWait(SceUInt32 usec, int *pState)
{
	int result;

	*pState = 1;
	for (;;) {
		result = promoterUtilIpcGetState(SceShellSvc_B31E7F1C(), pState);
		if (result < 0)
			return result;
		if (*pState != 0)
			sceKernelDelayThread(usec);
		if (*pState == 0)
			break;
	}
	return 0;
}

static inline __attribute__((always_inline)) int promoterUtilGetResult(int *pValue)
{
	int result;

	result = promoterUtilIpcGetResult(SceShellSvc_B31E7F1C(), pValue);
	if (result >= 0)
		result = *pValue;
	return result;
}

extern "C" SceInt32 ScePromoterUtil_FDCCBE33(const char *path)
{
	int state;
	int value;
	int result;

	if (path == NULL)
		return PROMOTER_UTIL_ERROR_PARAM;
	result = promoterUtilIpcAddPath(SceShellSvc_B31E7F1C(), path);
	if (result >= 0) {
		result = promoterUtilWait(1000000, &state);
		if (result >= 0)
			result = promoterUtilGetResult(&value);
	}
	return result;
}

static inline __attribute__((always_inline)) int promoterUtilPromotePkg(const char *path, int sync)
{
	PromoterUtilPkgRequest request;
	int state;
	int result;

	if (path == NULL)
		return PROMOTER_UTIL_ERROR_PARAM;
	state = 0;
	result = promoterUtilIpcGetState(SceShellSvc_B31E7F1C(), &state);
	if (result < 0)
		return result;
	if (state != 0)
		return PROMOTER_UTIL_ERROR_BUSY;
	sce_paf_memset(&request, 0, PROMOTER_UTIL_PKG_REQUEST_SIZE);
	sce_paf_strncpy(request.path, path, 0x7E);
	request.sync = sync;
	request.flag = 1;
	request.enable = 1;
	request.mode = 3;
	return promoterUtilIpcAddPkg(SceShellSvc_B31E7F1C(), &request);
}

extern "C" SceInt32 scePromoterUtilityPromotePkg(const char *path, int sync)
{
	return promoterUtilPromotePkg(path, sync);
}

extern "C" SceInt32 scePromoterUtilityPromotePkgWithRif(const char *path, int sync)
{
	int state;
	int value;
	int result;

	result = promoterUtilPromotePkg(path, sync);
	if (result >= 0) {
		result = promoterUtilWait(1000000, &state);
		if (result >= 0)
			result = promoterUtilGetResult(&value);
	}
	return result;
}

extern "C" SceInt32 ScePromoterUtil_D4E87DCB(const char *path)
{
	PromoterUtilRifRequest request;
	int state;
	int result;

	if (path == NULL)
		return PROMOTER_UTIL_ERROR_PARAM;
	state = 0;
	result = promoterUtilIpcGetState(SceShellSvc_B31E7F1C(), &state);
	if (result < 0)
		return result;
	if (state != 0)
		return PROMOTER_UTIL_ERROR_BUSY;
	sce_paf_memset(&request, 0, PROMOTER_UTIL_RIF_REQUEST_SIZE);
	request.reserved80[0] = 0;
	sce_paf_strncpy(request.path, path, 0x7F);
	return promoterUtilIpcAddRif(SceShellSvc_B31E7F1C(), &request);
}

extern "C" SceInt32 scePromoterUtilityGetState(int *pState)
{
	if (pState == NULL)
		return PROMOTER_UTIL_ERROR_PARAM;
	return promoterUtilIpcGetState(SceShellSvc_B31E7F1C(), pState);
}

extern "C" SceInt32 scePromoterUtilityGetResult(int *pResult)
{
	if (pResult == NULL)
		return PROMOTER_UTIL_ERROR_PARAM;
	return promoterUtilIpcGetResult(SceShellSvc_B31E7F1C(), pResult);
}

extern "C" SceInt32 ScePromoterUtil_395739DF(int *pValue)
{
	if (pValue == NULL)
		return PROMOTER_UTIL_ERROR_PARAM;
	return promoterUtilIpcGetValue(SceShellSvc_B31E7F1C(), pValue);
}

extern "C" SceInt32 scePromoterUtilityDeletePkg(const char *titleId)
{
	int state;
	int value;
	int result;

	if (titleId == NULL)
		return PROMOTER_UTIL_ERROR_PARAM;
	result = promoterUtilIpcDeletePkg(SceShellSvc_B31E7F1C(), titleId);
	if (result >= 0) {
		result = promoterUtilWait(200000, &state);
		if (result >= 0)
			result = promoterUtilGetResult(&value);
	}
	return result;
}

extern "C" SceInt32 ScePromoterUtil_30A36AAF(const char *titleId, int flag)
{
	int state;
	int value;
	int result;

	if (titleId == NULL)
		return PROMOTER_UTIL_ERROR_PARAM;
	result = promoterUtilIpcSetFlag(SceShellSvc_B31E7F1C(), titleId, flag);
	if (result >= 0) {
		result = promoterUtilWait(200000, &state);
		if (result >= 0)
			result = promoterUtilGetResult(&value);
	}
	return result;
}

extern "C" SceInt32 scePromoterUtilityGetSize(const char *titleId, void *pSize)
{
	int state;
	int value;
	int result;

	if (titleId == NULL || pSize == NULL)
		return PROMOTER_UTIL_ERROR_PARAM;
	result = promoterUtilIpcStartGetSize(SceShellSvc_B31E7F1C(), titleId, pSize);
	if (result >= 0) {
		result = promoterUtilWait(200000, &state);
		if (result >= 0) {
			result = promoterUtilIpcGetSize(SceShellSvc_B31E7F1C(), &value, pSize);
			if (result >= 0)
				result = value;
		}
	}
	return result;
}

extern "C" SceInt32 scePromoterUtilityUpdateUpgradableStatus(const char *titleId)
{
	int state;
	int value;
	int result;

	if (titleId == NULL)
		return PROMOTER_UTIL_ERROR_PARAM;
	result = promoterUtilIpcUpdateUpgradable(SceShellSvc_B31E7F1C(), titleId);
	if (result >= 0) {
		result = promoterUtilWait(200000, &state);
		if (result >= 0)
			result = promoterUtilGetResult(&value);
	}
	return result;
}

extern "C" SceInt32 ScePromoterUtil_8494C742(const void *params)
{
	if (params == NULL)
		return PROMOTER_UTIL_ERROR_PARAM;
	return promoterUtilIpcPromoteImport(SceShellSvc_B31E7F1C(), params);
}

extern "C" SceInt32 scePromoterUtilityPromoteImport(const void *params)
{
	int state;
	int value;
	int result;

	result = PROMOTER_UTIL_ERROR_PARAM;
	if (params != NULL)
		result = promoterUtilIpcPromoteImport(SceShellSvc_B31E7F1C(), params);
	if (result >= 0) {
		result = promoterUtilWait(1000000, &state);
		if (result >= 0)
			result = promoterUtilGetResult(&value);
	}
	return result;
}

extern "C" SceInt32 scePromoterUtilityUpdateLiveArea(const PromoterUtilLiveAreaUpdate *args)
{
	int state;
	int value;
	int result;

	if (args == NULL)
		return PROMOTER_UTIL_ERROR_PARAM;
	result = promoterUtilIpcUpdateLiveArea(SceShellSvc_B31E7F1C(), args->titleId, args->path);
	if (result >= 0) {
		result = promoterUtilWait(1000000, &state);
		if (result >= 0)
			result = promoterUtilGetResult(&value);
	}
	return result;
}

extern "C" SceInt32 ScePromoterUtil_59A6CDAE(int *pValue)
{
	if (pValue == NULL)
		return PROMOTER_UTIL_ERROR_PARAM;
	return promoterUtilIpcGetFlags(SceShellSvc_B31E7F1C(), pValue);
}

extern "C" SceInt32 ScePromoterUtil_999AD6CE(void *in0, void *out, int size)
{
	if (in0 == NULL)
		return PROMOTER_UTIL_ERROR_PARAM;
	if (out == NULL)
		return PROMOTER_UTIL_ERROR_PARAM;
	if (size == 0)
		return PROMOTER_UTIL_ERROR_PARAM;
	return promoterUtilIpcGetResults(SceShellSvc_B31E7F1C(), in0, out, size);
}

extern "C" SceInt32 scePromoterUtilityCheckExist(const char *titleId, int *pExist)
{
	if (titleId == NULL)
		return PROMOTER_UTIL_ERROR_PARAM;
	if (pExist == NULL)
		return PROMOTER_UTIL_ERROR_PARAM;
	return promoterUtilIpcCheckExist(SceShellSvc_B31E7F1C(), titleId, pExist);
}

extern "C" SceInt32 ScePromoterUtil_739BC292(const char *titleId, int arg, void *pInfo)
{
	IPMI::EventFlagInfo eventFlag;
	int requestId;
	int value;
	int result;

	if (titleId == NULL || pInfo == NULL)
		return PROMOTER_UTIL_ERROR_PARAM;
	requestId = -1;
	result = SceShellSvc_A0B067AC(&eventFlag, 8);
	if (result < 0)
		return result;
	result = promoterUtilIpcStartStatus(SceShellSvc_B31E7F1C(), &requestId, &eventFlag, titleId, arg);
	if (result < 0)
		return result;
	result = SceShellSvc_8D4F9F2E(8, NULL);
	if (result < 0)
		return result;
	result = promoterUtilIpcEndStatus(SceShellSvc_B31E7F1C(), requestId);
	if (result < 0)
		return result;
	result = promoterUtilIpcGetSize(SceShellSvc_B31E7F1C(), &value, pInfo);
	if (result >= 0)
		result = value;
	return result;
}

extern "C" SceInt32 ScePromoterUtil_B27DA268(const char *titleId, void *pInfo)
{
	IPMI::EventFlagInfo eventFlag;
	int requestId;
	int value;
	int result;

	if (titleId == NULL || pInfo == NULL)
		return PROMOTER_UTIL_ERROR_PARAM;
	requestId = -1;
	result = SceShellSvc_A0B067AC(&eventFlag, 8);
	if (result < 0)
		return result;
	result = promoterUtilIpcStartName(SceShellSvc_B31E7F1C(), &requestId, &eventFlag, titleId);
	if (result < 0)
		return result;
	result = SceShellSvc_8D4F9F2E(8, NULL);
	if (result < 0)
		return result;
	result = promoterUtilIpcEndName(SceShellSvc_B31E7F1C(), requestId);
	if (result < 0)
		return result;
	result = promoterUtilIpcGetInfo(SceShellSvc_B31E7F1C(), &value, pInfo);
	if (result >= 0)
		result = value;
	return result;
}

extern "C" SceInt32 ScePromoterUtil_5FF5CACF(const char *titleId, void *pInfo, int size)
{
	if (titleId == NULL || pInfo == NULL || size != PROMOTER_UTIL_PKG_INFO_SIZE)
		return PROMOTER_UTIL_ERROR_PARAM;
	return promoterUtilIpcGetPkgInfo(SceShellSvc_B31E7F1C(), titleId, pInfo, PROMOTER_UTIL_PKG_INFO_SIZE);
}

extern "C" SceInt32 ScePromoterUtil_0D76CD38(const char *titleId, void *pStatus, int size)
{
	if (titleId == NULL || pStatus == NULL || size != PROMOTER_UTIL_PKG_STATUS_SIZE)
		return PROMOTER_UTIL_ERROR_PARAM;
	return promoterUtilIpcGetPkgStatus(SceShellSvc_B31E7F1C(), titleId, pStatus, PROMOTER_UTIL_PKG_STATUS_SIZE);
}

extern "C" SceInt32 ScePromoterUtil_6A547384(const char *titleId, int arg, void *pStatus)
{
	IPMI::EventFlagInfo eventFlag;
	int requestId;
	int value;
	int result;

	if (titleId == NULL || pStatus == NULL)
		return PROMOTER_UTIL_ERROR_PARAM;
	requestId = -1;
	result = SceShellSvc_A0B067AC(&eventFlag, 8);
	if (result < 0)
		return result;
	result = promoterUtilIpcStartInfo(SceShellSvc_B31E7F1C(), &requestId, &eventFlag, titleId, arg);
	if (result < 0)
		return result;
	result = SceShellSvc_8D4F9F2E(8, NULL);
	if (result < 0)
		return result;
	result = promoterUtilIpcEndInfo(SceShellSvc_B31E7F1C(), requestId);
	if (result < 0)
		return result;
	result = promoterUtilIpcGetStatus(SceShellSvc_B31E7F1C(), &value, pStatus);
	if (result >= 0)
		result = value;
	return result;
}

extern "C" SceInt32 ScePromoterUtil_59DA665E(const char *titleId, int arg, void *pStatus)
{
	IPMI::EventFlagInfo eventFlag;
	int requestId;
	int value;
	int result;

	if (titleId == NULL || pStatus == NULL)
		return PROMOTER_UTIL_ERROR_PARAM;
	requestId = -1;
	result = SceShellSvc_A0B067AC(&eventFlag, 8);
	if (result < 0)
		return result;
	result = promoterUtilIpcStartProgress(SceShellSvc_B31E7F1C(), &requestId, &eventFlag, titleId, arg);
	if (result < 0)
		return result;
	result = SceShellSvc_8D4F9F2E(8, NULL);
	if (result < 0)
		return result;
	result = promoterUtilIpcEndProgress(SceShellSvc_B31E7F1C(), requestId);
	if (result < 0)
		return result;
	result = promoterUtilIpcGetStatus(SceShellSvc_B31E7F1C(), &value, pStatus);
	if (result >= 0)
		result = value;
	return result;
}

extern "C" SceInt32 scePromoterUtilityRemoveSavedata(const char *titleId, int arg1, int arg2)
{
	int state;
	int value;
	int result;

	result = promoterUtilIpcRemoveSavedata(SceShellSvc_B31E7F1C(), titleId, arg1, arg2);
	if (result >= 0) {
		state = 1;
		for (;;) {
			result = promoterUtilIpcGetState(SceShellSvc_B31E7F1C(), &state);
			if (result < 0)
				break;
			if (state != 0)
				sceKernelDelayThread(1000000);
			if (state == 0) {
				value = 0;
				result = promoterUtilIpcGetResult(SceShellSvc_B31E7F1C(), &value);
				if (result >= 0)
					result = value;
				break;
			}
		}
	}
	return result;
}

/* sets the property ids; nothing calls it */
extern "C" SceUInt32 promoterUtilInitPropertyIds(void)
{
	promoterUtilPropertyIds.liveAreaPath = promoterUtilHash15((const unsigned char *)"#_livearea_path");
	promoterUtilPropertyIds.ctime = promoterUtilHash7((const unsigned char *)"#_ctime");
	promoterUtilPropertyIds.lltime = promoterUtilHash8((const unsigned char *)"#_lltime");
	promoterUtilPropertyIds.promoteAttr = promoterUtilHash14((const unsigned char *)"#_promote_attr");
	promoterUtilPropertyIds.upStatus = promoterUtilHash11((const unsigned char *)"#_up_status");
	promoterUtilPropertyIds.libLocation = promoterUtilHash12((const unsigned char *)"#LIBLOCATION");
	promoterUtilPropertyIds.appconf = promoterUtilHash8((const unsigned char *)"#appconf");
	return promoterUtilPropertyIds.appconf;
}
