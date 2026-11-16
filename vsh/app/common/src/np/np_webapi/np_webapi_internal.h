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
#include <appmgr.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <regmgr/regmgr.h>
#ifdef __cplusplus
}
#endif

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

/* SceNpCommonPs4's mutex (lock object, 0x2c bytes) and its calls, named by their NIDs */
typedef struct NpWebApiMutex {
	unsigned char reserved[0x2c];
} NpWebApiMutex;

#ifdef __cplusplus
extern "C" {
#endif
extern int SceNpCommonPs4_4858EFB6(NpWebApiMutex *mutex, const char *name, int recursive);
extern int SceNpCommonPs4_F5840D6E(NpWebApiMutex *mutex);	/* lock */
extern int SceNpCommonPs4_4A1C1C3C(NpWebApiMutex *mutex);	/* unlock */
extern int SceNpCommonPs4_29895EBB(NpWebApiMutex *mutex);	/* destroy */
#ifdef __cplusplus
}
#endif

/* a library context (176 bytes); the contexts of the process are a list (g_libContexts) */
typedef struct NpWebApiLibContext {
	const void *vtable;
	struct NpWebApiLibContext *next;
	unsigned int state;
	int refCount;
	int id;
	void *heapBlock;
	NpWebApiMutex lock;
	void *userContexts;
	unsigned char reserved1[48];
	unsigned char aborted;
	unsigned char flag121;
	unsigned char reserved2[51];
	unsigned char deleting;
	unsigned char reserved3[2];
} NpWebApiLibContext;

typedef char NpWebApiLibContextSizeCheck[sizeof(NpWebApiLibContext) == 176 ? 1 : -1];

extern NpWebApiMutex g_globalLock;		/* 0x81010048 */
extern NpWebApiLibContext *g_libContexts;	/* 0x81010014 */

extern int npWebApiLibContextLock(NpWebApiLibContext *ctx);
extern int npWebApiLibContextUnlock(NpWebApiLibContext *ctx);
extern int npWebApiLibContextRelease(NpWebApiLibContext *ctx);
extern int npWebApiLibContextIsDeleting(NpWebApiLibContext *ctx);
extern int npWebApiLibContextIsIdle(NpWebApiLibContext *ctx);
extern int npWebApiLibContextIsStopped(NpWebApiLibContext *ctx);
extern int npWebApiLibContextHasOtherUsers(NpWebApiLibContext *ctx);
extern int npWebApiLibContextGetAborted(NpWebApiLibContext *ctx, unsigned char *aborted);
extern int npWebApiLibContextSetAborted(NpWebApiLibContext *ctx);
extern int npWebApiLibContextClearAborted(NpWebApiLibContext *ctx);
extern int npWebApiLibContextSetDeleting(NpWebApiLibContext *ctx);
extern int npWebApiLibContextIsStarted(void *unused, NpWebApiLibContext *ctx);
extern int npWebApiLibContextSetStarted(void *unused, NpWebApiLibContext *ctx);
extern NpWebApiLibContext *npWebApiLibContextSetFlag121(NpWebApiLibContext *ctx, char value);
extern int npWebApiLibContextGetFlag121(NpWebApiLibContext *ctx);
/* the library's heap: an mspace over a 0x2000-byte buffer, made at module start */
extern void *npWebApiHeapMspace(void);
extern int npWebApiGlobalLockDestroy(void);
extern void *npWebApiMspaceAlloc(SceSize size, void *mspace);
extern void npWebApiMspaceFree(void *ptr, void *mspace);
extern void *npWebApiMspaceRealloc(void *ptr, SceSize size, void *mspace);
extern void npWebApiMspaceGetStats(void *mspace, SceSize *stats);
extern int npWebApiHeapCreate(void);
extern void npWebApiHeapDestroy(void);
extern int npWebApiHeapGet(void **pMspace);
extern NpWebApiLibContext *npWebApiFindLibContext(int libCtxId, unsigned char allowDeleting);
extern int npWebApiGlobalLockCreate(void);
extern int npWebApiGlobalLock(void) __attribute__((noinline));
extern int npWebApiGlobalUnlock(void) __attribute__((noinline));

/* a user context: the library context it belongs to, its reference count, its ID and its user */
typedef struct NpWebApiUserContext {
	unsigned char reserved0[4];
	NpWebApiLibContext *libContext;
	int refCount;
	int id;
	SceNpOnlineId onlineId;
	unsigned char reserved1[4];
	void *requests;
	unsigned char reserved2[4];
	struct NpWebApiUserContext *next;
	unsigned char reserved3[106];
	unsigned char flag158;
	unsigned char reserved4[1];
} NpWebApiUserContext;

extern volatile int g_systemParam;		/* 0x81010010 */
extern int npWebApiConvertError(int error);
extern int npWebApiLoadSystemParam(void);
extern NpWebApiUserContext *npWebApiUserContextNext(NpWebApiUserContext *userCtx);
extern void *npWebApiUserContextRequests(NpWebApiUserContext *userCtx);
extern int npWebApiUserContextGetId(NpWebApiUserContext *userCtx);
extern NpWebApiUserContext *npWebApiUserContextSetNext(NpWebApiUserContext *userCtx, NpWebApiUserContext *next);
extern SceNpOnlineId *npWebApiUserContextOnlineId(NpWebApiUserContext *userCtx);
extern int npWebApiUserContextAddRef(NpWebApiUserContext *userCtx);
extern int npWebApiUserContextFlag158(NpWebApiUserContext *userCtx);
extern int npWebApiIsShellProcess(void);

#endif
