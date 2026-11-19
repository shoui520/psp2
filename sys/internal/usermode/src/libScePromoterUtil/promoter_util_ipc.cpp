/*
 * ScePromoterUtil (vs0:sys/external/libScePromoterUtil.suprx): the application installer's client
 * (see promoter_util_internal.h).
 */
#include "promoter_util_internal.h"

static PromoterUtilPropertyIds promoterUtilPropertyIds2;

/* the service's methods, in the order of the library's code */
__attribute__((noinline)) int promoterUtilIpcAddPath(IPMI::Client *client, const char *path)
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

__attribute__((noinline)) int promoterUtilIpcAddPkg(IPMI::Client *client,
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

__attribute__((noinline)) int promoterUtilIpcAddRif(IPMI::Client *client,
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

__attribute__((noinline)) int promoterUtilIpcGetState(IPMI::Client *client, int *pState)
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

__attribute__((noinline)) int promoterUtilIpcGetResult(IPMI::Client *client, int *pResult)
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

__attribute__((noinline)) int promoterUtilIpcGetSize(IPMI::Client *client, int *pResult, void *pSize)
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

__attribute__((noinline)) int promoterUtilIpcGetStatus(IPMI::Client *client, int *pResult, void *pStatus)
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

__attribute__((noinline)) int promoterUtilIpcGetInfo(IPMI::Client *client, int *pResult, void *pInfo)
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

__attribute__((noinline)) int promoterUtilIpcGetValue(IPMI::Client *client, int *pValue)
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

__attribute__((noinline)) int promoterUtilIpcDeletePkg(IPMI::Client *client, const char *titleId)
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

__attribute__((noinline)) int promoterUtilIpcSetFlag(IPMI::Client *client, const char *titleId,
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

__attribute__((noinline)) int promoterUtilIpcStartGetSize(IPMI::Client *client,
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

__attribute__((noinline)) int promoterUtilIpcUpdateUpgradable(IPMI::Client *client,
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

__attribute__((noinline)) int promoterUtilIpcPromoteImport(IPMI::Client *client,
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

__attribute__((noinline)) int promoterUtilIpcUpdateLiveArea(IPMI::Client *client,
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

__attribute__((noinline)) int promoterUtilIpcInit(IPMI::Client *client)
{
	int result = 0;
	int ret;

	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_INIT, NULL, 0, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

__attribute__((noinline)) int promoterUtilIpcExit(IPMI::Client *client)
{
	int result = 0;
	int ret;

	ret = client->invokeSyncMethod(PROMOTER_UTIL_METHOD_EXIT, NULL, 0, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

__attribute__((noinline)) int promoterUtilIpcGetFlags(IPMI::Client *client, int *pValue)
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

__attribute__((noinline)) int promoterUtilIpcGetResults(IPMI::Client *client, void *in0,
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

__attribute__((noinline)) int promoterUtilIpcCheckExist(IPMI::Client *client,
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

__attribute__((noinline)) int promoterUtilIpcStartStatus(IPMI::Client *client, int *pRequestId,
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

__attribute__((noinline)) int promoterUtilIpcEndStatus(IPMI::Client *client, int requestId)
{
	int result = 0;
	int ret;

	ret = client->tryGetResult(PROMOTER_UTIL_METHOD_END_STATUS, requestId, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

__attribute__((noinline)) int promoterUtilIpcStartName(IPMI::Client *client, int *pRequestId,
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

__attribute__((noinline)) int promoterUtilIpcEndName(IPMI::Client *client, int requestId)
{
	int result = 0;
	int ret;

	ret = client->tryGetResult(PROMOTER_UTIL_METHOD_END_NAME, requestId, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

__attribute__((noinline)) int promoterUtilIpcGetPkgInfo(IPMI::Client *client, const char *titleId,
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

__attribute__((noinline)) int promoterUtilIpcGetPkgStatus(IPMI::Client *client, const char *titleId,
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

__attribute__((noinline)) int promoterUtilIpcStartInfo(IPMI::Client *client, int *pRequestId,
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

__attribute__((noinline)) int promoterUtilIpcEndInfo(IPMI::Client *client, int requestId)
{
	int result = 0;
	int ret;

	ret = client->tryGetResult(PROMOTER_UTIL_METHOD_END_INFO, requestId, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

__attribute__((noinline)) int promoterUtilIpcStartProgress(IPMI::Client *client, int *pRequestId,
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

__attribute__((noinline)) int promoterUtilIpcEndProgress(IPMI::Client *client, int requestId)
{
	int result = 0;
	int ret;

	ret = client->tryGetResult(PROMOTER_UTIL_METHOD_END_PROGRESS, requestId, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

__attribute__((noinline)) int promoterUtilIpcRemoveSavedata(IPMI::Client *client,
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

/* sets the property ids; nothing calls it */
extern "C" SceUInt32 promoterUtilInitPropertyIds2(void)
{
	promoterUtilPropertyIds2.liveAreaPath = promoterUtilHash15((const unsigned char *)"#_livearea_path");
	promoterUtilPropertyIds2.ctime = promoterUtilHash7((const unsigned char *)"#_ctime");
	promoterUtilPropertyIds2.lltime = promoterUtilHash8((const unsigned char *)"#_lltime");
	promoterUtilPropertyIds2.promoteAttr = promoterUtilHash14((const unsigned char *)"#_promote_attr");
	promoterUtilPropertyIds2.upStatus = promoterUtilHash11((const unsigned char *)"#_up_status");
	promoterUtilPropertyIds2.libLocation = promoterUtilHash12((const unsigned char *)"#LIBLOCATION");
	promoterUtilPropertyIds2.appconf = promoterUtilHash8((const unsigned char *)"#appconf");
	return promoterUtilPropertyIds2.appconf;
}

/* the call of a missing function: its result is 9 (nothing sets the function) */
extern "C" int promoterUtilNotSupported(void)
{
	int (*func)(int) = NULL;

	if (func == NULL)
		return 9;
	return func(0);
}

SceUInt32 promoterUtilHash7(const unsigned char *a)
{
	return (((((((((((((a[0] ^ 0x811C9DC5U) * 16777619U) ^ a[1]) * 16777619U) ^ a[2]) * 16777619U) ^ a[3]) * 16777619U) ^ a[4]) * 16777619U) ^ a[5]) * 16777619U) ^ a[6]) * 16777619U;
}

SceUInt32 promoterUtilHash8(const unsigned char *a)
{
	return (((((((((((((((a[0] ^ 0x811C9DC5U) * 16777619U) ^ a[1]) * 16777619U) ^ a[2]) * 16777619U) ^ a[3]) * 16777619U) ^ a[4]) * 16777619U) ^ a[5]) * 16777619U) ^ a[6]) * 16777619U) ^ a[7]) * 16777619U;
}

SceUInt32 promoterUtilHash11(const unsigned char *a)
{
	return (((((((((((((((((((((a[0] ^ 0x811C9DC5U) * 16777619U) ^ a[1]) * 16777619U) ^ a[2]) * 16777619U) ^ a[3]) * 16777619U) ^ a[4]) * 16777619U) ^ a[5]) * 16777619U) ^ a[6]) * 16777619U) ^ a[7]) * 16777619U) ^ a[8]) * 16777619U) ^ a[9]) * 16777619U) ^ a[10]) * 16777619U;
}

SceUInt32 promoterUtilHash12(const unsigned char *a)
{
	return (((((((((((((((((((((((a[0] ^ 0x811C9DC5U) * 16777619U) ^ a[1]) * 16777619U) ^ a[2]) * 16777619U) ^ a[3]) * 16777619U) ^ a[4]) * 16777619U) ^ a[5]) * 16777619U) ^ a[6]) * 16777619U) ^ a[7]) * 16777619U) ^ a[8]) * 16777619U) ^ a[9]) * 16777619U) ^ a[10]) * 16777619U) ^ a[11]) * 16777619U;
}

SceUInt32 promoterUtilHash14(const unsigned char *a)
{
	return (((((((((((((((((((((((((((a[0] ^ 0x811C9DC5U) * 16777619U) ^ a[1]) * 16777619U) ^ a[2]) * 16777619U) ^ a[3]) * 16777619U) ^ a[4]) * 16777619U) ^ a[5]) * 16777619U) ^ a[6]) * 16777619U) ^ a[7]) * 16777619U) ^ a[8]) * 16777619U) ^ a[9]) * 16777619U) ^ a[10]) * 16777619U) ^ a[11]) * 16777619U) ^ a[12]) * 16777619U) ^ a[13]) * 16777619U;
}

SceUInt32 promoterUtilHash15(const unsigned char *a)
{
	return (((((((((((((((((((((((((((((a[0] ^ 0x811C9DC5U) * 16777619U) ^ a[1]) * 16777619U) ^ a[2]) * 16777619U) ^ a[3]) * 16777619U) ^ a[4]) * 16777619U) ^ a[5]) * 16777619U) ^ a[6]) * 16777619U) ^ a[7]) * 16777619U) ^ a[8]) * 16777619U) ^ a[9]) * 16777619U) ^ a[10]) * 16777619U) ^ a[11]) * 16777619U) ^ a[12]) * 16777619U) ^ a[13]) * 16777619U) ^ a[14]) * 16777619U;
}
