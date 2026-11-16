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
#define NP_WEBAPI_ERROR_USER_CONTEXT_ALREADY_EXISTS	-2141902582	/* 0x8055290a */
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

/* SceNpCommonPs4's mutex (lock object, 0x20 bytes) and its calls, named by their NIDs */
typedef struct NpWebApiMutex {
	unsigned char reserved[0x20];
} NpWebApiMutex;

#ifdef __cplusplus
extern "C" {
#endif
extern int SceNpCommonPs4_4858EFB6(NpWebApiMutex *mutex, const char *name, int recursive);
extern int SceNpCommonPs4_F5840D6E(NpWebApiMutex *mutex);	/* lock */
extern int SceNpCommonPs4_4A1C1C3C(NpWebApiMutex *mutex);	/* unlock */
extern int SceNpCommonPs4_29895EBB(NpWebApiMutex *mutex);	/* destroy */
extern int SceNpCommon_8C21F489(const SceNpOnlineId *a, const SceNpOnlineId *b);	/* compare online IDs */
extern int SceNpCommonPs4_38CC9AF5(void *pool);
extern int SceNpCommonPs4_299ADFC3(void *pool, int size);
extern int SceNpCommonPs4_31DB2D97(void *pool);
extern const void *g_userContextVtable[];	/* 0x8100eddc */
extern const void *g_connectionPoolVtable[];	/* 0x8100eb7c */
extern const void *g_baseVtable[];		/* 0x8100eb8c */
extern int SceNpCommonPs4_BB5282C8(void *object);
extern void *SceNpCommonPs4_D154D84F(int size, void *pool);	/* allocate from a pool */
extern int SceNpCommonPs4_6B428829(void *allocator);
extern int SceNpCommonPs4_CE924EB3(int a, void *object);
#ifdef __cplusplus
}
#endif

struct NpWebApiUserContext;

/* what a library context's pool is made of (0x28 bytes): its memory block, its mspace, and the pool
   object the SceNpCommonPs4 containers allocate from */
typedef struct NpWebApiHeapBlock {
	SceUID memBlock;
	void *mspace;
	void *owner;
	unsigned char reserved0[8];
	unsigned char poolObject[20];
} NpWebApiHeapBlock;

/* a library context (176 bytes); the contexts of the process are a list (g_libContexts) */
typedef struct NpWebApiLibContext {
	const void *vtable;
	struct NpWebApiLibContext *next;
	unsigned int state;
	int refCount;
	int id;
	NpWebApiHeapBlock *heapBlock;
	NpWebApiMutex lock;
	struct NpWebApiUserContext *userContextHead;
	int userContextCount;
	void *pushEventObject;
	void *requestList;
	unsigned char reserved1[44];
	int reserved116;
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
extern int npWebApiLibContextAddUserContext(NpWebApiLibContext *ctx, NpWebApiUserContext *userCtx);
extern int npWebApiLibContextRemoveUserContext(NpWebApiLibContext *ctx, NpWebApiUserContext *userCtx);
extern NpWebApiUserContext *npWebApiLibContextFindUserContext(NpWebApiLibContext *ctx, int userCtxId);
extern NpWebApiUserContext *npWebApiLibContextFindUserContextByOnlineId(NpWebApiLibContext *ctx,
	const SceNpOnlineId *onlineId);
extern int npWebApiGlobalLockCreate(void);
extern int npWebApiGlobalLock(void) __attribute__((noinline));
extern int npWebApiGlobalUnlock(void) __attribute__((noinline));

/* a user context: the library context it belongs to, its reference count, its ID and its user */
typedef struct NpWebApiUserContext {
	const void *vtable;
	NpWebApiLibContext *libContext;
	int refCount;
	int id;
	SceNpOnlineId onlineId;
	int httpTemplateId;
	void *requests;
	void *object44;
	struct NpWebApiUserContext *next;
	struct NpWebApiRequest *requestHead;
	int requestCount;
	int field60;
	int field64;
	int containerA;
	int field72;
	int field76;
	int containerB;
	int field84;
	int field88;
	int containerC;
	unsigned char flag96;
	unsigned char flag97;
	unsigned char flag98;
	unsigned char reserved5[59];
	unsigned char flag158;
	unsigned char reserved6[1];
	int param0;
	int param1;
} NpWebApiUserContext;

typedef char NpWebApiUserContextSizeCheck[sizeof(NpWebApiUserContext) == 168 ? 1 : -1];

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
extern int npWebApiUserContextRelease(NpWebApiUserContext *userCtx);
extern int npWebApiUserContextHasOtherRefs(NpWebApiUserContext *userCtx);
extern NpWebApiUserContext *npWebApiUserContextSetFlag158(NpWebApiUserContext *userCtx, char value);
extern int npWebApiIsShellProcess(void);

/* a request: the library context it belongs to, its reference count and its 64-bit ID (the library
   context's ID is in the upper half) */
/* a node of the request's lists (a ring around the head node) */
typedef struct NpWebApiListNode {
	unsigned char reserved0[4];
	struct NpWebApiListNode *next;
	int value8;
	int value12;
} NpWebApiListNode;

struct NpWebApiRequest;

typedef struct NpWebApiRequest {
	unsigned char reserved0[4];
	NpWebApiLibContext *libContext;
	int refCount;
	unsigned char reserved1[4];
	SceInt64 requestId;
	unsigned char reserved2a[24];
	struct NpWebApiListNode *listAHead;
	int listACount;
	void *listAAllocator;
	unsigned char reserved2b[4];
	struct NpWebApiListNode *listBHead;
	int listBCount;
	void *listBAllocator;
	unsigned char reserved2c[164];
	struct NpWebApiRequest *next;
	unsigned char reserved3[36];
	unsigned char object280[32];
	int field312;
	int field316;
	unsigned char reserved4[8];
	unsigned char flag328;
	unsigned char reserved5[60];
	unsigned char flag389;
	unsigned char reserved6[2];
} NpWebApiRequest;

extern NpWebApiLibContext *npWebApiUserContextGetLibContext(NpWebApiUserContext *userCtx);
extern int npWebApiUserContextGetHttpTemplateId(NpWebApiUserContext *userCtx);
extern void *npWebApiUserContextGetObject44(NpWebApiUserContext *userCtx);
extern NpWebApiUserContext *npWebApiUserContextSetFlag97(NpWebApiUserContext *userCtx, char value);
extern int npWebApiUserContextGetFlag97(NpWebApiUserContext *userCtx);
extern int npWebApiUserContextGetParam0(NpWebApiUserContext *userCtx);
extern int npWebApiUserContextGetParam1(NpWebApiUserContext *userCtx);
extern int npWebApiUserContextUnlockLib(NpWebApiUserContext *userCtx);
extern int npWebApiUserContextLockLib(NpWebApiUserContext *userCtx);
extern int npWebApiRequestGetFixedSize(void);
extern NpWebApiRequest *npWebApiRequestSetField316(NpWebApiRequest *request, int value);
extern int npWebApiRequestGetField312(NpWebApiRequest *request);
extern int npWebApiRequestGetFlag328(NpWebApiRequest *request);
extern NpWebApiRequest *npWebApiRequestSetFlag328(NpWebApiRequest *request, char value);
extern int npWebApiRequestGetFlag389(NpWebApiRequest *request);
extern NpWebApiRequest *npWebApiRequestSetFlag389(NpWebApiRequest *request, char value);
extern void *npWebApiRequestGetObject280(NpWebApiRequest *request);
extern void *npWebApiLibContextGetPool(NpWebApiLibContext *ctx);
extern int npWebApiLibContextDestroyPool(NpWebApiLibContext *ctx);
extern void *npWebApiLibContextGetPool2(NpWebApiLibContext *ctx);
extern int npWebApiLibContextPoolAlloc(NpWebApiLibContext *ctx, int size);
extern int npWebApiLibContextGetId(NpWebApiLibContext *ctx);
extern int npWebApiLibContextZero(void);
extern void *npWebApiLibContextGetPushEventObject(NpWebApiLibContext *ctx);
extern int npWebApiLibContextGetField116(NpWebApiLibContext *ctx);
extern NpWebApiUserContext *npWebApiUserContextConstruct(NpWebApiUserContext *userCtx, NpWebApiLibContext *libCtx,
	const SceNpOnlineId *onlineId);
extern int npWebApiUserContextAssignId(NpWebApiUserContext *userCtx);
extern int npWebApiUserContextAddRequest(NpWebApiUserContext *userCtx, NpWebApiRequest *request);
extern int npWebApiUserContextRemoveRequest(NpWebApiUserContext *userCtx, NpWebApiRequest *request);
extern NpWebApiRequest *npWebApiUserContextFindRequest(NpWebApiUserContext *userCtx, SceInt64 requestId);
extern NpWebApiRequest *npWebApiRequestSetNext(NpWebApiRequest *request, NpWebApiRequest *next);
/* a user context's HTTP connections: a list of connections, each one's ID and the next */
typedef struct NpWebApiConnection {
	unsigned char reserved0[32];
	int connId;
	struct NpWebApiConnection *next;
} NpWebApiConnection;

typedef struct NpWebApiConnectionPool {
	const void *vtable;
	NpWebApiLibContext *libContext;
	NpWebApiUserContext *userContext;
	NpWebApiConnection *head;
	int count;
} NpWebApiConnectionPool;

extern int npWebApiDestroyObject(void *object);
extern int npWebApiConnectionPoolCloseAll(NpWebApiConnectionPool *pool);
extern NpWebApiConnectionPool *npWebApiConnectionPoolConstruct(NpWebApiConnectionPool *pool, NpWebApiUserContext *userCtx);
extern NpWebApiConnectionPool *npWebApiConnectionPoolDestruct(NpWebApiConnectionPool *pool);
extern int npWebApiConnectionPoolDeleteThis(NpWebApiConnectionPool *pool);

extern int npWebApiRequestClearListB(NpWebApiRequest *request, NpWebApiLibContext *libCtx);
extern int npWebApiRequestClearListA(NpWebApiRequest *request, NpWebApiLibContext *libCtx);
extern int npWebApiRequestRelease(NpWebApiRequest *request);
extern SceInt64 npWebApiRequestGetId(NpWebApiRequest *request);
extern NpWebApiRequest *npWebApiRequestNext(NpWebApiRequest *request);
extern int npWebApiRequestAddRef(NpWebApiRequest *request);
extern int npWebApiRequestHasOtherRefs(NpWebApiRequest *request);
extern void *npWebApiUserContextSetOnlineId(NpWebApiUserContext *userCtx, const SceNpOnlineId *onlineId);

/* the push notification library context: one object, made at start (np_push/src) */
typedef struct NpPushLibContext {
	const void **vtable;
} NpPushLibContext;

extern NpPushLibContext *g_pushLibContext;	/* 0x81010038 */
extern void npPushLibContextDelete(void);
extern NpPushLibContext *npPushLibContextGet(void);
extern int npPushSetCallbackParam(const int *param);
extern const char *npPushFilterName(const unsigned char *filter);

/* the NP service's IPMI client: slot 5 of its table makes a synchronous call */
typedef struct NpWebApiIpmiIn {
	const void *data;
	SceSize size;
} NpWebApiIpmiIn;

typedef struct NpWebApiIpmiOut {
	void *data;
	SceSize size;
	SceSize actualSize;
} NpWebApiIpmiOut;

struct NpWebApiIpmiClient;

typedef struct NpWebApiIpmiClientVtbl {
	void *slots[5];
	int (*invoke)(struct NpWebApiIpmiClient *client, int command, const NpWebApiIpmiIn *in, int inNum, int *result,
		NpWebApiIpmiOut *out, int outNum);
} NpWebApiIpmiClientVtbl;

typedef struct NpWebApiIpmiClient {
	const NpWebApiIpmiClientVtbl *vtbl;
} NpWebApiIpmiClient;

extern int npPushIpcSendId(NpWebApiIpmiClient *client, const void *data);
extern int npPushRegisterUser(SceNpOnlineId *onlineId);	/* 0x8100987a, not written yet */
extern int npPushUnregisterUser(SceNpOnlineId *onlineId);	/* 0x810098d4, not written yet */
extern int npWebApiUserContextReleaseResources(NpWebApiUserContext *userCtx);
extern int npWebApiCreateUserContextObject(int libCtxId, const SceNpOnlineId *onlineId, unsigned char failIfExists);
extern int npWebApiUserContextHasBusyRequests(NpWebApiUserContext *userCtx);
extern int npWebApiUserContextAbortRequests(NpWebApiUserContext *userCtx);
extern int npWebApiDeleteUserContextObject(int userCtxId);

#endif
