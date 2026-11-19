/*
 * ScePromoterUtil (vs0:sys/external/libScePromoterUtil.suprx): the application installer's client.
 * Each call is a request to the shell's SceShellSvc IPMI service (SceShellSvc_B31E7F1C returns its
 * client); the installing calls start the work, poll its state until it is done and read the
 * result. No Sony record names the library's files, calls or methods; the names are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include "../../../../vsh/lowlayer/src/ipmi/client.h"

#define PROMOTER_UTIL_ERROR_PARAM		-2136014705	/* 0x80AF008F */
#define PROMOTER_UTIL_ERROR_BUSY		-2136014815	/* 0x80AF0021 */

/* the service's methods */
#define PROMOTER_UTIL_METHOD_ADD_PATH		0x130000
#define PROMOTER_UTIL_METHOD_ADD_PKG		0x130001
#define PROMOTER_UTIL_METHOD_ADD_RIF		0x130002
#define PROMOTER_UTIL_METHOD_GET_STATE		0x130003
#define PROMOTER_UTIL_METHOD_GET_RESULT		0x130004
#define PROMOTER_UTIL_METHOD_GET_SIZE		0x130005
#define PROMOTER_UTIL_METHOD_GET_STATUS		0x130006
#define PROMOTER_UTIL_METHOD_GET_INFO		0x130007
#define PROMOTER_UTIL_METHOD_GET_VALUE		0x130008
#define PROMOTER_UTIL_METHOD_DELETE_PKG		0x130009
#define PROMOTER_UTIL_METHOD_SET_FLAG		0x13000A
#define PROMOTER_UTIL_METHOD_START_GET_SIZE	0x13000B
#define PROMOTER_UTIL_METHOD_UPDATE_UPGRADABLE	0x13000C
#define PROMOTER_UTIL_METHOD_PROMOTE_IMPORT	0x13000D
#define PROMOTER_UTIL_METHOD_UPDATE_LIVE_AREA	0x13000E
#define PROMOTER_UTIL_METHOD_INIT		0x13000F
#define PROMOTER_UTIL_METHOD_EXIT		0x130010
#define PROMOTER_UTIL_METHOD_GET_FLAGS		0x130011
#define PROMOTER_UTIL_METHOD_GET_RESULTS	0x130012
#define PROMOTER_UTIL_METHOD_CHECK_EXIST	0x130013
#define PROMOTER_UTIL_METHOD_START_STATUS	0x130014
#define PROMOTER_UTIL_METHOD_END_STATUS		0x130015
#define PROMOTER_UTIL_METHOD_START_NAME		0x130016
#define PROMOTER_UTIL_METHOD_END_NAME		0x130017
#define PROMOTER_UTIL_METHOD_GET_PKG_INFO	0x130018
#define PROMOTER_UTIL_METHOD_GET_PKG_STATUS	0x130019
#define PROMOTER_UTIL_METHOD_START_INFO		0x13001A
#define PROMOTER_UTIL_METHOD_END_INFO		0x13001B
#define PROMOTER_UTIL_METHOD_START_PROGRESS	0x13001C
#define PROMOTER_UTIL_METHOD_END_PROGRESS	0x13001D
#define PROMOTER_UTIL_METHOD_REMOVE_SAVEDATA	0x13001E

/* the data of the requests */
#define PROMOTER_UTIL_PKG_REQUEST_SIZE		0x120
#define PROMOTER_UTIL_RIF_REQUEST_SIZE		0x8C
#define PROMOTER_UTIL_IMPORT_PARAM_SIZE		176
#define PROMOTER_UTIL_PKG_INFO_SIZE		232
#define PROMOTER_UTIL_PKG_STATUS_SIZE		72

extern "C" IPMI::Client *SceShellSvc_B31E7F1C(void);
extern "C" int SceShellSvc_A0B067AC(IPMI::EventFlagInfo *pEventFlag, int event);
extern "C" int SceShellSvc_8D4F9F2E(int event, SceUInt32 *pTimeout);
extern "C" void *sce_paf_memset(void *dst, int ch, SceSize len);
extern "C" char *sce_paf_strncpy(char *dst, const char *src, SceSize len);
extern "C" int sceKernelDelayThread(SceUInt32 usec);

/* the application's request to be promoted, from a path */
typedef struct PromoterUtilPkgRequest {
	char path[0x80];
	SceUInt8 enable;
	SceUInt8 reserved81;
	SceUInt8 sync;
	SceUInt8 flag;
	SceInt32 mode;
	SceUInt8 reserved88[0x98];
} PromoterUtilPkgRequest;

typedef struct PromoterUtilRifRequest {
	char path[0x80];
	SceUInt8 reserved80[0xC];
} PromoterUtilRifRequest;

/* ScePromoterUtilityLAUpdate */
typedef struct PromoterUtilLiveAreaUpdate {
	char titleId[0xC];
	char path[1];
} PromoterUtilLiveAreaUpdate;

/* the length of the string with its terminator */
static inline SceSize promoterUtilSize(const char *str)
{
	SceSize len = 0;

	if (*str != '\0') {
		do {
			len++;
		} while (*++str != '\0');
	}
	return len + 1;
}

/* the service's methods, one function each (the library calls them out of line) */
static int promoterUtilIpcAddPath(IPMI::Client *client, const char *path);
static int promoterUtilIpcAddPkg(IPMI::Client *client, const PromoterUtilPkgRequest *request);
static int promoterUtilIpcAddRif(IPMI::Client *client, const PromoterUtilRifRequest *request);
static int promoterUtilIpcGetState(IPMI::Client *client, int *pState);
static int promoterUtilIpcGetResult(IPMI::Client *client, int *pResult);
static int promoterUtilIpcGetSize(IPMI::Client *client, int *pResult, void *pSize);
static int promoterUtilIpcGetStatus(IPMI::Client *client, int *pResult, void *pStatus);
static int promoterUtilIpcGetInfo(IPMI::Client *client, int *pResult, void *pInfo);
static int promoterUtilIpcGetValue(IPMI::Client *client, int *pValue);
static int promoterUtilIpcDeletePkg(IPMI::Client *client, const char *titleId);
static int promoterUtilIpcSetFlag(IPMI::Client *client, const char *titleId, int flag);
static int promoterUtilIpcStartGetSize(IPMI::Client *client, const char *titleId, void *pSize);
static int promoterUtilIpcUpdateUpgradable(IPMI::Client *client, const char *titleId);
static int promoterUtilIpcPromoteImport(IPMI::Client *client, const void *params);
static int promoterUtilIpcUpdateLiveArea(IPMI::Client *client, const char *titleId, const char *path);
static int promoterUtilIpcInit(IPMI::Client *client);
static int promoterUtilIpcExit(IPMI::Client *client);
static int promoterUtilIpcGetFlags(IPMI::Client *client, int *pValue);
static int promoterUtilIpcGetResults(IPMI::Client *client, void *in0, void *out, int size);
static int promoterUtilIpcCheckExist(IPMI::Client *client, const char *titleId, int *pExist);
static int promoterUtilIpcStartStatus(IPMI::Client *client, int *pRequestId,
				      const IPMI::EventFlagInfo *pEventFlag, const char *titleId, int arg);
static int promoterUtilIpcEndStatus(IPMI::Client *client, int requestId);
static int promoterUtilIpcStartName(IPMI::Client *client, int *pRequestId,
				    const IPMI::EventFlagInfo *pEventFlag, const char *titleId);
static int promoterUtilIpcEndName(IPMI::Client *client, int requestId);
static int promoterUtilIpcGetPkgInfo(IPMI::Client *client, const char *titleId, void *pInfo, int size);
static int promoterUtilIpcGetPkgStatus(IPMI::Client *client, const char *titleId, void *pStatus, int size);
static int promoterUtilIpcStartInfo(IPMI::Client *client, int *pRequestId,
				    const IPMI::EventFlagInfo *pEventFlag, const char *titleId, int arg);
static int promoterUtilIpcEndInfo(IPMI::Client *client, int requestId);
static int promoterUtilIpcStartProgress(IPMI::Client *client, int *pRequestId,
					const IPMI::EventFlagInfo *pEventFlag, const char *titleId, int arg);
static int promoterUtilIpcEndProgress(IPMI::Client *client, int requestId);
static int promoterUtilIpcRemoveSavedata(IPMI::Client *client, const char *titleId, int arg1, int arg2);

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

/* the service's methods, in the order of the library's code */
static __attribute__((noinline)) int promoterUtilIpcAddPath(IPMI::Client *client, const char *path)
{
	int result = 0;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = path;
	in[0].dataSize = promoterUtilSize(path);
	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_ADD_PATH, in, 1, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcAddPkg(IPMI::Client *client,
							    const PromoterUtilPkgRequest *request)
{
	int result = 0;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = request;
	in[0].dataSize = PROMOTER_UTIL_PKG_REQUEST_SIZE;
	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_ADD_PKG, in, 1, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcAddRif(IPMI::Client *client,
							    const PromoterUtilRifRequest *request)
{
	int result = 0;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = request;
	in[0].dataSize = PROMOTER_UTIL_RIF_REQUEST_SIZE;
	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_ADD_RIF, in, 1, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcGetState(IPMI::Client *client, int *pState)
{
	int result = 0;
	IPMI::BufferInfo out[1];
	int ret;

	out[0].pBuffer = pState;
	out[0].bufferSize = sizeof(*pState);
	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_GET_STATE, NULL, 0, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcGetResult(IPMI::Client *client, int *pResult)
{
	int result = 0;
	IPMI::BufferInfo out[1];
	int ret;

	out[0].pBuffer = pResult;
	out[0].bufferSize = sizeof(*pResult);
	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_GET_RESULT, NULL, 0, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcGetSize(IPMI::Client *client, int *pResult, void *pSize)
{
	int result = 0;
	IPMI::BufferInfo out[2];
	int ret;

	out[0].pBuffer = pResult;
	out[0].bufferSize = sizeof(*pResult);
	out[1].pBuffer = pSize;
	out[1].bufferSize = 8;
	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_GET_SIZE, NULL, 0, &result, out, 2);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcGetStatus(IPMI::Client *client, int *pResult, void *pStatus)
{
	int result = 0;
	IPMI::BufferInfo out[2];
	int ret;

	out[0].pBuffer = pResult;
	out[0].bufferSize = sizeof(*pResult);
	out[1].pBuffer = pStatus;
	out[1].bufferSize = 48;
	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_GET_STATUS, NULL, 0, &result, out, 2);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcGetInfo(IPMI::Client *client, int *pResult, void *pInfo)
{
	int result = 0;
	IPMI::BufferInfo out[2];
	int ret;

	out[0].pBuffer = pResult;
	out[0].bufferSize = sizeof(*pResult);
	out[1].pBuffer = pInfo;
	out[1].bufferSize = 16;
	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_GET_INFO, NULL, 0, &result, out, 2);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcGetValue(IPMI::Client *client, int *pValue)
{
	int result = 0;
	IPMI::BufferInfo out[1];
	int ret;

	out[0].pBuffer = pValue;
	out[0].bufferSize = sizeof(*pValue);
	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_GET_VALUE, NULL, 0, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcDeletePkg(IPMI::Client *client, const char *titleId)
{
	int result = 0;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = titleId;
	in[0].dataSize = promoterUtilSize(titleId);
	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_DELETE_PKG, in, 1, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcSetFlag(IPMI::Client *client, const char *titleId,
							     int flag)
{
	int result = 0;
	int data = flag;
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = &data;
	in[0].dataSize = sizeof(data);
	in[1].pData = titleId;
	in[1].dataSize = promoterUtilSize(titleId);
	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_SET_FLAG, in, 2, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcStartGetSize(IPMI::Client *client,
								  const char *titleId, void *pSize)
{
	int result = 0;
	IPMI::DataInfo in[1];
	IPMI::BufferInfo out[1];
	int ret;

	in[0].pData = titleId;
	in[0].dataSize = promoterUtilSize(titleId);
	out[0].pBuffer = pSize;
	out[0].bufferSize = 8;
	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_START_GET_SIZE, in, 1, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcUpdateUpgradable(IPMI::Client *client,
								      const char *titleId)
{
	int result = 0;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = titleId;
	in[0].dataSize = promoterUtilSize(titleId);
	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_UPDATE_UPGRADABLE, in, 1, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcPromoteImport(IPMI::Client *client,
								   const void *params)
{
	int result = 0;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = params;
	in[0].dataSize = PROMOTER_UTIL_IMPORT_PARAM_SIZE;
	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_PROMOTE_IMPORT, in, 1, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcUpdateLiveArea(IPMI::Client *client,
								    const char *titleId, const char *path)
{
	int result = 0;
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = titleId;
	in[0].dataSize = promoterUtilSize(titleId);
	in[1].pData = path;
	in[1].dataSize = promoterUtilSize(path);
	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_UPDATE_LIVE_AREA, in, 2, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcInit(IPMI::Client *client)
{
	int result = 0;
	int ret;

	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_INIT, NULL, 0, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcExit(IPMI::Client *client)
{
	int result = 0;
	int ret;

	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_EXIT, NULL, 0, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcGetFlags(IPMI::Client *client, int *pValue)
{
	int result = 0;
	IPMI::BufferInfo out[1];
	int ret;

	out[0].pBuffer = pValue;
	out[0].bufferSize = sizeof(*pValue);
	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_GET_FLAGS, NULL, 0, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcGetResults(IPMI::Client *client, void *in0,
								void *out, int size)
{
	int result = 0;
	int value = size;
	IPMI::DataInfo in[2];
	IPMI::BufferInfo outList[1];
	int ret;

	in[0].pData = &value;
	in[0].dataSize = sizeof(value);
	in[1].pData = in0;
	in[1].dataSize = sizeof(int);
	outList[0].pBuffer = out;
	outList[0].bufferSize = 12;
	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_GET_RESULTS, in, 2, &result, outList, 1);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcCheckExist(IPMI::Client *client,
								const char *titleId, int *pExist)
{
	int result = 0;
	IPMI::DataInfo in[1];
	IPMI::BufferInfo out[1];
	int ret;

	in[0].pData = titleId;
	in[0].dataSize = promoterUtilSize(titleId);
	out[0].pBuffer = pExist;
	out[0].bufferSize = sizeof(*pExist);
	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_CHECK_EXIST, in, 1, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcStartStatus(IPMI::Client *client, int *pRequestId,
					     const IPMI::EventFlagInfo *pEventFlag,
					     const char *titleId, int arg)
{
	int value = arg;
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = &value;
	in[0].dataSize = sizeof(value);
	in[1].pData = titleId;
	in[1].dataSize = promoterUtilSize(titleId);
	ret = client->invokeAsyncMethod(PROMOTER_UTIL_METHOD_START_STATUS, in, 2, pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

static __attribute__((noinline)) int promoterUtilIpcEndStatus(IPMI::Client *client, int requestId)
{
	int result = 0;
	int ret;

	ret = client->tryGetResult(PROMOTER_UTIL_METHOD_END_STATUS, requestId, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcStartName(IPMI::Client *client, int *pRequestId,
							       const IPMI::EventFlagInfo *pEventFlag,
							       const char *titleId)
{
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = titleId;
	in[0].dataSize = promoterUtilSize(titleId);
	ret = client->invokeAsyncMethod(PROMOTER_UTIL_METHOD_START_NAME, in, 1, pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

static __attribute__((noinline)) int promoterUtilIpcEndName(IPMI::Client *client, int requestId)
{
	int result = 0;
	int ret;

	ret = client->tryGetResult(PROMOTER_UTIL_METHOD_END_NAME, requestId, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcGetPkgInfo(IPMI::Client *client, const char *titleId,
					     void *pBuffer, int size)
{
	int result = 0;
	int value = size;
	IPMI::DataInfo in[2];
	IPMI::BufferInfo out[1];
	int ret;

	in[0].pData = &value;
	in[0].dataSize = sizeof(value);
	in[1].pData = titleId;
	in[1].dataSize = promoterUtilSize(titleId);
	out[0].pBuffer = pBuffer;
	out[0].bufferSize = PROMOTER_UTIL_PKG_INFO_SIZE;
	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_GET_PKG_INFO, in, 2, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcGetPkgStatus(IPMI::Client *client, const char *titleId,
					     void *pBuffer, int size)
{
	int result = 0;
	int value = size;
	IPMI::DataInfo in[2];
	IPMI::BufferInfo out[1];
	int ret;

	in[0].pData = &value;
	in[0].dataSize = sizeof(value);
	in[1].pData = titleId;
	in[1].dataSize = promoterUtilSize(titleId);
	out[0].pBuffer = pBuffer;
	out[0].bufferSize = PROMOTER_UTIL_PKG_STATUS_SIZE;
	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_GET_PKG_STATUS, in, 2, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcStartInfo(IPMI::Client *client, int *pRequestId,
					     const IPMI::EventFlagInfo *pEventFlag,
					     const char *titleId, int arg)
{
	int value = arg;
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = &value;
	in[0].dataSize = sizeof(value);
	in[1].pData = titleId;
	in[1].dataSize = promoterUtilSize(titleId);
	ret = client->invokeAsyncMethod(PROMOTER_UTIL_METHOD_START_INFO, in, 2, pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

static __attribute__((noinline)) int promoterUtilIpcEndInfo(IPMI::Client *client, int requestId)
{
	int result = 0;
	int ret;

	ret = client->tryGetResult(PROMOTER_UTIL_METHOD_END_INFO, requestId, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcStartProgress(IPMI::Client *client, int *pRequestId,
					     const IPMI::EventFlagInfo *pEventFlag,
					     const char *titleId, int arg)
{
	int value = arg;
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = &value;
	in[0].dataSize = sizeof(value);
	in[1].pData = titleId;
	in[1].dataSize = promoterUtilSize(titleId);
	ret = client->invokeAsyncMethod(PROMOTER_UTIL_METHOD_START_PROGRESS, in, 2, pRequestId, pEventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

static __attribute__((noinline)) int promoterUtilIpcEndProgress(IPMI::Client *client, int requestId)
{
	int result = 0;
	int ret;

	ret = client->tryGetResult(PROMOTER_UTIL_METHOD_END_PROGRESS, requestId, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int promoterUtilIpcRemoveSavedata(IPMI::Client *client,
								    const char *titleId, int arg1, int arg2)
{
	int result = 0;
	int args[2];
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = args;
	in[0].dataSize = sizeof(args);
	in[1].pData = titleId;
	in[1].dataSize = promoterUtilSize(titleId);
	args[0] = arg1;
	args[1] = arg2;
	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_REMOVE_SAVEDATA, in, 2, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}
