/*
 * SceNpWebApi: the web API client of the NP libraries. Sony built it with SNC 3.57 in C++. The
 * exports' names are the SDK's (np/np_webapi.h) where it has one, the rest are the NID's; the
 * internal names are ours, taken from what the code does.
 */
#ifndef NP_WEBAPI_INTERNAL_H
#define NP_WEBAPI_INTERNAL_H

#include <stddef.h>
#include <fw.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <kernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <kernel/sysmem.h>
#include <libkernel/libkernel.h>
#include <libhttp.h>
#include <np/np_common.h>
#include <np/np_webapi.h>

/* the errors the exports return (SCE_NP_WEBAPI_ERROR_*, SDK doc NpWebApi-Reference) */
#define NP_WEBAPI_ERROR_OUT_OF_MEMORY		-2141902591	/* 0x80552901 */
#define NP_WEBAPI_ERROR_INVALID_ARGUMENT	-2141902590	/* 0x80552902 */
#define NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND	-2141902588	/* 0x80552904 */
#define NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND	-2141902587	/* 0x80552905 */
#define NP_WEBAPI_ERROR_REQUEST_NOT_FOUND	-2141902586	/* 0x80552906 */
#define NP_WEBAPI_ERROR_NOT_INITIALIZED		-2141902335	/* 0x80552a01 */
#define NP_WEBAPI_ERROR_ALREADY_INITIALIZED	-2141902334	/* 0x80552a02 */

/* the library's own firmware number, read from libkernel at start (0x03650011 for 3.65) */
extern SceUInt32 g_sdkVersion;
extern SceUInt32 npWebApiGetSdkVersion(void);

/* the library's global state */
extern volatile int g_initState;		/* 0x81010000 */
extern volatile int g_initCount;		/* 0x81010004 */
extern volatile int g_defaultLibCtxId;	/* 0x81010008 */

/* the library context, the user context and the request, by the IDs the exports take */
extern int npWebApiIsShellMode(void);
extern int npWebApiCreateLibContext(SceSize poolSize, void *pool, const char *name, int flags);
extern int npWebApiDeleteLibContext(int libCtxId);
extern int npWebApiStartLibContext(int libCtxId, int a1, unsigned char a2);
extern int npWebApiStopLibContext(int libCtxId);
extern int npWebApiResolveUserContext(int libCtxId, unsigned char needSignedIn);
extern int npWebApiCreateRequestObject(int userCtxId, const char *apiGroup, const char *path, unsigned int method,
	const SceNpWebApiContentParameter *content, int a6, SceInt64 *pRequestId, unsigned char multipart);
extern int npWebApiDeleteRequest(SceInt64 requestId);
extern int npWebApiAbortRequest(SceInt64 requestId);
extern int npWebApiSend(SceInt64 requestId, int partIndex, const void *data, SceSize size, unsigned char useRespInfo,
	void *respInfoOrStatus);
extern int npWebApiGetHttpStatusCode(SceInt64 requestId, int *pStatus);
extern int npWebApiReadData(SceInt64 requestId, void *data, SceSize size);
extern int npWebApiGetErrorCode(void);
extern int npWebApiSetMultipartContentType(SceInt64 requestId, const char *type, const char *boundary);
extern int npWebApiAddMultipartPart(SceInt64 requestId, const SceNpWebApiMultipartPartParameter *param, int *pIndex);
extern int npWebApiAddHttpRequestHeader(SceInt64 requestId, const char *name, const char *value);
extern int npWebApiGetHttpResponseHeader(SceInt64 requestId, const char *name, char *value, SceSize size, SceSize *pLength);
extern int npWebApiCreatePushEventFilter(int libCtxId, const SceNpWebApiPushEventDataType *types, SceSize num);
extern int npWebApiDeletePushEventFilter(int libCtxId, int filterId);
extern int npWebApiRegisterPushEventCallback(int userCtxId, int filterId, SceNpWebApiPushEventCallback cb, void *arg);
extern int npWebApiUnregisterPushEventCallback(int userCtxId, int callbackId);
extern int npWebApiCreateHandle(int libCtxId);
extern int npWebApiDeleteHandle(int libCtxId, int handleId);
extern int npWebApiAbortHandle(int libCtxId, int handleId);
extern int npWebApiCreateServicePushEventFilter(int libCtxId, int handleId, const char *service, SceNpServiceLabel label,
	const SceNpWebApiPushEventDataType *types, SceSize num);
extern int npWebApiDeleteServicePushEventFilter(int libCtxId, int filterId);
extern int npWebApiRegisterServicePushEventCallback(int userCtxId, int filterId, SceNpWebApiServicePushEventCallback cb, void *arg);
extern int npWebApiUnregisterServicePushEventCallback(int userCtxId, int callbackId);
extern int npWebApiCreateExtdPushEventFilter(int libCtxId, int handleId, const char *service, SceNpServiceLabel label,
	const SceNpWebApiExtdPushEventFilterParameter *param, SceSize num);
extern int npWebApiDeleteExtdPushEventFilter(int libCtxId, int filterId);
extern int npWebApiRegisterExtdPushEventCallback(int userCtxId, int filterId, SceNpWebApiExtdPushEventCallback cb, void *arg);
extern int npWebApiUnregisterExtdPushEventCallback(int userCtxId, int callbackId);
extern int npWebApiCheckCallback(void);
extern int npWebApiGetMemoryPoolStats(int libCtxId, SceNpWebApiMemoryPoolStats *stats);

/* what the SDK number gates (g_sdkVersion) */
#define NP_WEBAPI_SDK_3_30	0x3300000
#define NP_WEBAPI_SDK_3_50	0x3500000
#define NP_WEBAPI_SDK_3_55	0x3550000

#endif
