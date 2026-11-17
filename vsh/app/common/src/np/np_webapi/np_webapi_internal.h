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
#define NP_WEBAPI_ERROR_MULTIPART_PART_NOT_FOUND	-2141902569	/* 0x80552917 */
#define NP_WEBAPI_HTTP_ERROR_SSL	-2143088523	/* 0x80431075, SCE_HTTP_ERROR_SSL */
#define NP_WEBAPI_ERROR_HANDLE_NOT_FOUND	-2141902579	/* 0x8055290d */
#define NP_WEBAPI_ERROR_OUT_OF_MEMORY		-2141902591	/* 0x80552901 */
#define NP_WEBAPI_ERROR_INVALID_ARGUMENT	-2141902590	/* 0x80552902 */
#define NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND	-2141902588	/* 0x80552904 */
#define NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND	-2141902587	/* 0x80552905 */
#define NP_WEBAPI_ERROR_USER_CONTEXT_ALREADY_EXISTS	-2141902582	/* 0x8055290a */
#define NP_WEBAPI_ERROR_REQUEST_NOT_FOUND	-2141902586	/* 0x80552906 */
#define NP_WEBAPI_ERROR_REQUEST_BUSY		-2141902583	/* 0x80552909 */
#define NP_WEBAPI_ERROR_MULTIPART_NOT_ALLOWED	-2141902570	/* 0x80552916 */
#define NP_WEBAPI_ERROR_INVALID_CONTENT_TYPE	-2141902568	/* 0x80552918 */
#define NP_WEBAPI_ERROR_HEADER_NOT_ALLOWED	-2141902562	/* 0x8055291e */
#define NP_WEBAPI_ERROR_REQUEST_REFERENCED	-2141902573	/* 0x80552913 */
#define NP_WEBAPI_ERROR_FILTER_NOT_FOUND	-2141902581	/* 0x8055290b */
#define NP_WEBAPI_ERROR_SERVICE_FILTER_NOT_FOUND	-2141902578	/* 0x8055290e */
#define NP_WEBAPI_ERROR_EXTD_FILTER_NOT_FOUND	-2141902564	/* 0x8055291c */
#define NP_WEBAPI_ERROR_CALLBACK_NOT_FOUND	-2141902580	/* 0x8055290c */
#define NP_WEBAPI_ERROR_SERVICE_CALLBACK_NOT_FOUND	-2141902577	/* 0x8055290f */
#define NP_WEBAPI_ERROR_EXTD_CALLBACK_NOT_FOUND	-2141902563	/* 0x8055291d */
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
extern int npWebApiAbortHandle(int libCtxId, int handleId) __attribute__((noinline));
extern int npWebApiCreateServicePushEventFilter(int libCtxId, int handleId, const char *service, SceNpServiceLabel label,
	const SceNpWebApiPushEventDataType *types, SceSize num);
extern int npWebApiDeleteServicePushEventFilter(int libCtxId, int filterId);
extern int npWebApiRegisterServicePushEventCallback(int userCtxId, int filterId, SceNpWebApiServicePushEventCallback cb,
	SceNpWebApiServicePushEventCallback cbEx, void *arg);
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
extern int SceNpCommon_0624AE44(char *version);	/* the library's version text */
extern int sceClibSnprintf(char *dst, SceSize size, const char *fmt, ...);
extern int sceClibStrncmp(const char *s1, const char *s2, SceSize len);
extern int sceClibStrncasecmp(const char *s1, const char *s2, SceSize len);
extern int SceNpManager_E45E4003(void *buffer);
extern int SceNpCommonPs4_FB63A152(void);
extern int SceNpCommonPs4_CF591432(void);
extern int SceNpCommon_8C21F489(const SceNpOnlineId *a, const SceNpOnlineId *b);	/* compare online IDs */
extern int SceNpCommonPs4_38CC9AF5(void *pool, void *block);
extern int SceNpCommonPs4_299ADFC3(void *pool, int size);
extern int SceNpCommonPs4_31DB2D97(void *pool);
extern const void *g_userContextVtable[];	/* 0x8100eddc */
extern const void *g_connectionPoolVtable[];	/* 0x8100eb7c */
extern const void *g_baseVtable[];		/* 0x8100eb6c */
extern int SceNpCommonPs4_BB5282C8(void *object);
extern int SceNpCommonPs4_9AC24306(void);
extern int SceNpCommonPs4_68D08C92(...);
extern int SceNpManager_737B2820(void);
extern void *SceNpCommonPs4_D154D84F(int size, void *pool);	/* allocate from a pool */
extern int SceNpCommonPs4_6B428829(void *allocator, void *node);
extern int sceNpManagerGetNpId(void *npId);
extern SceSize sceClibStrlcpy(char *dst, const char *src, SceSize size);
extern int SceNpManager_A775A452(int state);
extern int SceNpCommonPs4_FB0B3FBD(void *params);
extern int SceNpCommonPs4_639086A3(void *params);
extern int SceNpCommonPs4_17EB36C2(void *object, int arg);
extern int SceNpCommonPs4_AB41A67D(int handle, void *npId, const char *name, int arg, void *out);
extern int SceNpCommonPs4_6E443E9B(int handle, void *npId, const char *name, void *object, int zero, int arg, void *out);
extern int SceNpCommonPs4_9C4F1236(int handle);
extern int SceNpCommonPs4_530CAFDA(int handle);
extern int SceNpCommonPs4_CE924EB3(int a, void *object);
struct NpPushKey;
struct NpWebApiIpmiClient;
extern void SceNpCommonPs4_B0BAB9BE(struct NpPushKey *key);			/* key: default constructor */
extern void SceNpCommonPs4_1543D7EE(struct NpPushKey *key);			/* key: destructor */
extern void SceNpCommonPs4_AEC0DAC5(struct NpPushKey *dst, const struct NpPushKey *src);	/* key: copy */
extern struct NpPushKey *SceNpCommonPs4_D0787194(struct NpPushKey *key, const void *onlineId);	/* key: from online ID */
extern int SceNpCommonPs4_21B2C0B7(const struct NpPushKey *a, const struct NpPushKey *b);	/* key: equal */
extern void *SceNpCommonPs4_AA980113(void *allocator, int size);
extern int SceNpCommonPs4_A2A8493B(void *mutex);				/* lock */
extern int SceNpCommonPs4_6BA5A164(void *mutex);				/* unlock */
extern int SceNpCommonPs4_D84BF4B7(int arg);
extern struct NpWebApiIpmiClient *SceNpCommonPs4_A0599F24(void);			/* the NP service's IPMI client */
extern void SceNpCommonPs4_F50BD96A(int result);				/* halt after a failed assertion */

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

/* a handle: its ID, its state bits (2: aborted) and how many are using it */
typedef struct NpWebApiHandle {
	int id;
	int object;
	unsigned int state;
	int refCount;
} NpWebApiHandle;

typedef struct NpWebApiHandleNode {
	struct NpWebApiHandleNode *prev;
	struct NpWebApiHandleNode *next;
	NpWebApiHandle *handle;
} NpWebApiHandleNode;

typedef struct NpWebApiHandleList {
	NpWebApiHandleNode *sentinel;
	unsigned long count;
	void *allocator;
} NpWebApiHandleList;

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
	NpWebApiHandleList handles;
	NpWebApiHandleList filters;
	NpWebApiHandleList serviceFilters;
	NpWebApiHandleList extdFilters;
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

extern void npWebApiHandleListInsert(NpWebApiHandleNode **pResult, NpWebApiHandleList *list,
	NpWebApiHandle **pHandle) __attribute__((noinline));
extern int npWebApiHandleListErase(NpWebApiHandleNode **pResult, NpWebApiHandleList *list,
	NpWebApiHandleNode **pAt) __attribute__((noinline));
/* a push event filter (20 bytes): its ID, the types it listens for (65-byte records) and its library context */
typedef struct NpWebApiPushFilter {
	const void *const *vtable;
	int id;
	void *types;
	int typeCount;
	NpWebApiLibContext *libContext;
} NpWebApiPushFilter;

/* a service push event filter: its vtable and ID */
typedef struct NpWebApiServiceFilter {
	const void *const *vtable;
	int id;
	unsigned char all;
	unsigned char reserved1[3];
	NpWebApiHandleList list;
	void *types;
	unsigned int typeCount;
	char name[33];
	unsigned char reserved2[3];
	int handle;
	NpWebApiLibContext *libContext;
} NpWebApiServiceFilter;

typedef char NpWebApiServiceFilterSizeCheck[sizeof(NpWebApiServiceFilter) == 76 ? 1 : -1];

extern const void *g_serviceFilterVtable[];	/* 0x8100edbc */
extern NpWebApiServiceFilter *npWebApiServiceFilterConstruct(NpWebApiServiceFilter *filter,
	NpWebApiLibContext *libCtx) __attribute__((noinline));
extern NpWebApiServiceFilter *npWebApiServiceFilterDestruct(NpWebApiServiceFilter *filter) __attribute__((noinline));
extern int npWebApiServiceFilterDelete(NpWebApiServiceFilter *filter) __attribute__((noinline));
extern void npWebApiFilterRecordListInsert(NpWebApiHandleNode **pResult, NpWebApiHandleList *list,
	const unsigned char *record) __attribute__((noinline));
extern int npWebApiServiceFilterResolve(NpWebApiServiceFilter *filter, NpWebApiHandle *handle,
	const char *name, int arg) __attribute__((noinline));
extern int npWebApiServiceFilterInit(NpWebApiServiceFilter *filter, NpWebApiHandle *handle, const char *name,
	int arg, const char *types, unsigned int typeCount) __attribute__((noinline));
extern int npWebApiLibContextCreateServicePushEventFilter(NpWebApiLibContext *ctx, int handleId, const char *name,
	int arg, const char *types, unsigned int typeCount) __attribute__((noinline));

extern void npWebApiServiceFilterListInsert(NpWebApiHandleNode **pResult, NpWebApiHandleList *list,
	NpWebApiServiceFilter **pFilter) __attribute__((noinline));
extern int npWebApiServiceFilterListErase(NpWebApiHandleNode **pResult, NpWebApiHandleList *list,
	NpWebApiHandleNode **pAt) __attribute__((noinline));
extern int npWebApiServiceFilterGetId(NpWebApiServiceFilter *filter) __attribute__((noinline));
extern int npWebApiLibContextDeleteServicePushEventFilter(NpWebApiLibContext *ctx, int filterId) __attribute__((noinline));
extern int npWebApiDeleteServicePushEventFilter(int libCtxId, int filterId);

/* what an extended filter keeps for one service: its name, the names it listens to and the service object */
typedef struct NpWebApiExtdFilterRecord {
	char service[65];
	unsigned char reserved1[3];
	char *names;
	unsigned int nameCount;
	struct NpWebApiExtdService *serviceObject;
} NpWebApiExtdFilterRecord;

/* an extended push event filter (76 bytes) */
typedef struct NpWebApiExtdFilter {
	const void *const *vtable;
	int id;
	unsigned char all;
	unsigned char reserved1[3];
	NpWebApiHandleList list;
	NpWebApiExtdFilterRecord *records;
	unsigned int recordCount;
	char name[33];
	unsigned char reserved2[3];
	int handle;
	NpWebApiLibContext *libContext;
} NpWebApiExtdFilter;

typedef char NpWebApiExtdFilterSizeCheck[sizeof(NpWebApiExtdFilter) == 76 ? 1 : -1];

extern void npWebApiExtdFilterListInsert(NpWebApiHandleNode **pResult, NpWebApiHandleList *list,
	NpWebApiExtdFilter **pFilter) __attribute__((noinline));
extern int npWebApiExtdFilterListErase(NpWebApiHandleNode **pResult, NpWebApiHandleList *list,
	NpWebApiHandleNode **pAt) __attribute__((noinline));
extern int npWebApiExtdFilterGetId(NpWebApiExtdFilter *filter) __attribute__((noinline));
extern int npWebApiLibContextDeleteExtdPushEventFilter(NpWebApiLibContext *ctx, int filterId) __attribute__((noinline));
extern NpWebApiExtdFilter *npWebApiLibContextFindExtdPushEventFilter(NpWebApiLibContext *ctx, int filterId) __attribute__((noinline));
extern int npWebApiDeleteExtdPushEventFilter(int libCtxId, int filterId);

extern const void *g_pushFilterVtable[];	/* 0x8100ed9c */
extern void npWebApiFilterListInsert(NpWebApiHandleNode **pResult, NpWebApiHandleList *list,
	NpWebApiPushFilter **pFilter) __attribute__((noinline));
extern int npWebApiFilterListErase(NpWebApiHandleNode **pResult, NpWebApiHandleList *list,
	NpWebApiHandleNode **pAt) __attribute__((noinline));
extern NpWebApiPushFilter *npWebApiPushFilterConstruct(NpWebApiPushFilter *filter, NpWebApiLibContext *libCtx) __attribute__((noinline));
extern int npWebApiPushFilterInit(NpWebApiPushFilter *filter, const char *types, int typeCount) __attribute__((noinline));
extern int npWebApiPushFilterGetId(NpWebApiPushFilter *filter) __attribute__((noinline));
extern int npWebApiLibContextCreatePushEventFilter(NpWebApiLibContext *ctx, const char *types, int typeCount) __attribute__((noinline));
extern int npWebApiLibContextDeletePushEventFilter(NpWebApiLibContext *ctx, int filterId) __attribute__((noinline));
extern int npWebApiCreatePushEventFilter(int libCtxId, const SceNpWebApiPushEventDataType *pDataType, unsigned int num);
extern int npWebApiDeletePushEventFilter(int libCtxId, int filterId);
extern int npWebApiLibContextCreateHandle(NpWebApiLibContext *ctx);
extern int npWebApiLibContextIsHandleUnusable(NpWebApiLibContext *ctx, NpWebApiHandle *handle)
	__attribute__((noinline));
extern int npWebApiLibContextAcquireHandle(NpWebApiLibContext *ctx, int handleId, NpWebApiHandle **pHandle)
	__attribute__((noinline));
extern int npWebApiLibContextReleaseHandle(NpWebApiLibContext *ctx, NpWebApiHandle *handle)
	__attribute__((noinline));
extern int npWebApiLibContextDeleteHandle(NpWebApiLibContext *ctx, int handleId);
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
struct NpWebApiListNode;

/* the callbacks a user context keeps: rings of nodes around a sentinel made when the first comes. A
   node is the callback's ID, its filter's ID, the callbacks and the argument, and a state byte (2 while
   the callback runs; the unregister waits for it) */
typedef struct NpWebApiCallbackNode {
	struct NpWebApiCallbackNode *prev;
	struct NpWebApiCallbackNode *next;
	int id;
	int filterId;
	void *callback;
	void *argument;
	unsigned char state;
	unsigned char reserved25[3];
} NpWebApiCallbackNode;

typedef struct NpWebApiCallbackList {
	NpWebApiCallbackNode *sentinel;
	unsigned long count;
	void *allocator;
} NpWebApiCallbackList;

typedef struct NpWebApiServiceCallbackNode {
	struct NpWebApiServiceCallbackNode *prev;
	struct NpWebApiServiceCallbackNode *next;
	int id;
	int filterId;
	void *callback;
	void *callbackEx;
	void *argument;
	unsigned char state;
	unsigned char reserved29[3];
} NpWebApiServiceCallbackNode;

typedef struct NpWebApiServiceCallbackList {
	NpWebApiServiceCallbackNode *sentinel;
	unsigned long count;
	void *allocator;
} NpWebApiServiceCallbackList;

/* what is inserted: the node without its links */
typedef struct NpWebApiCallbackRecord {
	int id;
	int filterId;
	void *callback;
	void *argument;
	unsigned char state;
} NpWebApiCallbackRecord;

typedef struct NpWebApiServiceCallbackRecord {
	int id;
	int filterId;
	void *callback;
	void *callbackEx;
	void *argument;
	unsigned char state;
} NpWebApiServiceCallbackRecord;

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
	NpWebApiCallbackList pushCallbacks;
	NpWebApiServiceCallbackList serviceCallbacks;
	NpWebApiCallbackList extdCallbacks;
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
	const void *const *vtable;
	NpWebApiLibContext *libContext;
	int refCount;
	unsigned char reserved1[4];
	SceInt64 requestId;
	char *path;
	char *query;
	SceSize queryLength;
	unsigned int method;
	int totalSize;
	char *contentType;
	struct NpWebApiListNode *listAHead;
	int listACount;
	void *listAAllocator;
	unsigned char multipart;
	unsigned char reserved2b[3];
	struct NpWebApiListNode *listBHead;
	int listBCount;
	void *listBAllocator;
	char buffer76[64];
	char boundary[64];
	unsigned char flag204;
	unsigned char reserved2c[3];
	int kind;
	unsigned char busy;
	unsigned char reserved2d[3];
	int httpRequestId;
	int field220;
	int field224;
	int field228;
	int field232;
	int field236;
	struct NpWebApiRequest *next;
	void *extraData;
	unsigned char data[64];
	int field312;
	int field316;
	int readOffset;
	int sentSize;
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
extern int npWebApiLibContextDestroyPool(NpWebApiLibContext *ctx, void *block);
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

/* the library context's push event object (np_push): its library context and its requests */
typedef struct NpWebApiPushObject {
	const void *vtable;
	NpWebApiLibContext *libContext;
} NpWebApiPushObject;

extern int npWebApiConnectionPoolAbortRequest(NpWebApiConnectionPool *pool, NpWebApiRequest *request,
	unsigned char flag);
extern int npWebApiPushObjectAbortRequest(NpWebApiPushObject *object, NpWebApiRequest *request);
extern int npWebApiDestroyObject(void *object);
extern int npWebApiConnectionPoolCloseAll(NpWebApiConnectionPool *pool);
extern NpWebApiConnectionPool *npWebApiConnectionPoolConstruct(NpWebApiConnectionPool *pool, NpWebApiUserContext *userCtx);
extern NpWebApiConnectionPool *npWebApiConnectionPoolDestruct(NpWebApiConnectionPool *pool);
extern int npWebApiConnectionPoolDeleteThis(NpWebApiConnectionPool *pool);

extern int npWebApiRequestClearListB(NpWebApiRequest *request, NpWebApiLibContext *libCtx);
extern int npWebApiRequestClearListA(NpWebApiRequest *request, NpWebApiLibContext *libCtx);
extern int npWebApiRequestIdGetUserContextId(SceInt64 requestId);
extern unsigned int npWebApiRequestIdGetLibContextId(SceInt64 requestId);
extern int npWebApiRequestGetKind(NpWebApiRequest *request);
extern int npWebApiRequestGetBusy(NpWebApiRequest *request);
extern NpWebApiRequest *npWebApiRequestSetKind(NpWebApiRequest *request, int value);
extern NpWebApiRequest *npWebApiRequestSetBusy(NpWebApiRequest *request, char value);
extern int npWebApiRequestGetField232(NpWebApiRequest *request);
extern int npWebApiRequestGetHttpRequestId(NpWebApiRequest *request);
extern int npWebApiRequestGetField228(NpWebApiRequest *request);
extern int npWebApiRequestGetField236(NpWebApiRequest *request);
extern SceSize npWebApiRequestReadBuffered(NpWebApiRequest *request, void *dst, SceSize len);
extern int npWebApiRequestIsSendComplete(NpWebApiRequest *request);
extern int npWebApiRequestRelease(NpWebApiRequest *request);
extern int npWebApiDeleteHttpRequest(int httpRequestId) __attribute__((noinline));

/* a ring list's header: its sentinel, its size and the allocator its nodes come from */
typedef struct NpWebApiList {
	NpWebApiListNode *sentinel;
	unsigned long count;
	void *allocator;
} NpWebApiList;

typedef struct NpWebApiListPair {
	int first;
	int second;
} NpWebApiListPair;

extern void npWebApiPairListInsert(NpWebApiListNode **pResult, NpWebApiList *list,
	const NpWebApiListPair *pair) __attribute__((noinline));
extern int npWebApiRequestGetMultipart(NpWebApiRequest *request) __attribute__((noinline));
extern int npWebApiRequestGetFlag204(NpWebApiRequest *request) __attribute__((noinline));
extern int npWebApiRequestSetMultipartContentType(NpWebApiRequest *request, const char *contentType,
	const char *boundary) __attribute__((noinline));
extern int npWebApiRequestAddHttpHeader(NpWebApiRequest *request, const char *name, const char *value)
	__attribute__((noinline));
extern int npWebApiSetMultipartContentType(SceInt64 requestId, const char *contentType, const char *boundary);
/* a part of a multipart request: its ID, its header text and the number of bytes it announced and
   has sent */
typedef struct NpWebApiMultipartPart {
	int partId;
	char header[1024];
	int contentLength;
	int sentLength;
} NpWebApiMultipartPart;

extern void npWebApiPartListInsert(NpWebApiHandleNode **pResult, NpWebApiHandleList *list,
	NpWebApiMultipartPart **pPart) __attribute__((noinline));
extern int npWebApiRequestAddMultipartPart(NpWebApiRequest *request, const SceNpWebApiMultipartPartParameter *param,
	int *pIndex) __attribute__((noinline));
extern int npWebApiRequestGetResponseHeaderValue(NpWebApiRequest *request, const char *fieldStr, void *buffer,
	SceSize size, SceSize *pLength) __attribute__((noinline));
extern int npWebApiAddMultipartPart(SceInt64 requestId, const SceNpWebApiMultipartPartParameter *param, int *pIndex);
extern int npWebApiGetHttpResponseHeader(SceInt64 requestId, const char *name, char *value, SceSize size, SceSize *pLength);
extern int npWebApiRequestSendMultipartPart(NpWebApiRequest *request, const void *data, SceSize size, int partId);
/* the 12-byte handle the user context creation hands out: it names the user context the library
   context has at hand (the library context's field at +56) */
typedef struct NpWebApiUserContextHandle {
	const void *const *vtable;
	NpWebApiUserContext *userContext;
	int reserved;
} NpWebApiUserContextHandle;

/* the 1320-byte object a user context keeps at +44 */
typedef struct NpWebApiUserObject {
	const void *const *vtable;
	NpWebApiLibContext *libContext;
	SceNpOnlineId *onlineId;
	unsigned char reserved12[4];
	unsigned char buffer16[264];
	unsigned char buffer280[1040];
} NpWebApiUserObject;

extern const void *g_userContextHandleVtable[];	/* 0x8100eb9c */
extern const void *g_userObjectVtable[];	/* 0x8100edcc */
extern NpWebApiUserContextHandle *npWebApiLibContextCreateUserContextHandle(NpWebApiLibContext *libCtx);
extern NpWebApiUserObject *npWebApiUserObjectConstruct(NpWebApiUserObject *object, NpWebApiLibContext *libCtx,
	NpWebApiUserContext *userCtx);
extern int npWebApiUserContextSetup(NpWebApiUserContext *userCtx);
extern NpWebApiUserContext *npWebApiUserContextDestruct(NpWebApiUserContext *userCtx);
extern NpWebApiRequest *npWebApiRequestConstruct(NpWebApiRequest *request, NpWebApiLibContext *libCtx);
extern void npWebApiRequestDestruct(NpWebApiRequest *request);
extern void npWebApiRequestDelete(NpWebApiRequest *request);
extern void npWebApiRequestDestroy(NpWebApiRequest *request, int *pHttpRequestId);
extern int npWebApiRequestCreate(NpWebApiUserContext *userCtx, const char *path, const char *query,
	unsigned int method, const SceNpWebApiContentParameter *content, const void *extraData,
	NpWebApiRequest **pRequest, unsigned char multipart);
extern const void *g_requestVtable[];		/* 0x8100edac */
extern SceInt64 npWebApiRequestGetId(NpWebApiRequest *request);
extern NpWebApiRequest *npWebApiRequestNext(NpWebApiRequest *request);
extern int npWebApiRequestAddRef(NpWebApiRequest *request);
extern int npWebApiRequestHasOtherRefs(NpWebApiRequest *request);
extern void *npWebApiUserContextSetOnlineId(NpWebApiUserContext *userCtx, const SceNpOnlineId *onlineId);

/* the push notification library context: one object, made at start (np_push/src) */
/* a user the library listens for: its key (the 20-byte ID at +4) and how many registered it */
typedef struct NpPushKey {
	unsigned char reserved0[4];
	unsigned char id[20];
} NpPushKey;

typedef struct NpPushUserNode {
	struct NpPushUserNode *prev;
	struct NpPushUserNode *next;
	NpPushKey key;
	int refCount;
} NpPushUserNode;

typedef struct NpPushUserList {
	NpPushUserNode *sentinel;
	int count;
	void *allocator;
} NpPushUserList;

typedef struct NpWebApiExtdName {
	const void *const *vtable;
	struct NpWebApiExtdName *next;
	struct NpWebApiExtdName *prev;
	char name[33];
	unsigned char reserved[3];
} NpWebApiExtdName;

typedef struct NpWebApiExtdEntry {
	const void *const *vtable;
	void *pool;
	struct NpWebApiExtdEntry *next;
	struct NpWebApiExtdEntry *prev;
	char service[65];
	unsigned char reserved[3];
	NpWebApiExtdName *head;
	NpWebApiExtdName *tail;
} NpWebApiExtdEntry;

typedef struct NpWebApiExtdEntryList {
	void *reserved;
	NpWebApiExtdEntry *head;
	NpWebApiExtdEntry *tail;
} NpWebApiExtdEntryList;

typedef struct NpPushLibContext {
	const void **vtable;
	unsigned char reserved4[12];
	unsigned char mutex[24];
	NpWebApiExtdEntryList services;
	unsigned char reserved52[12];
	NpPushUserList users;
	unsigned char reserved76[12];
	unsigned int flags;
} NpPushLibContext;

extern NpPushLibContext *g_pushLibContext;	/* 0x81010038 */
extern void npPushLibContextDelete(void);
extern NpPushLibContext *npPushLibContextGet(void) __attribute__((noinline));
extern int npPushSetCallbackParam(const int *param);
typedef struct NpPushUserValue {
	NpPushKey key;
	int refCount;
} NpPushUserValue;

extern void *operator new(unsigned int size, void *where);
extern void npPushUserListInsert(NpPushUserNode **pResult, NpPushUserList *list,
	const NpPushUserValue *value) __attribute__((noinline));
extern int npPushUserListErase(NpPushUserNode **pResult, NpPushUserList *list, NpPushUserNode **pAt)
	__attribute__((noinline));
extern int npPushLibContextAcquireUser(NpPushLibContext *ctx, const NpPushKey *key, int *pCount)
	__attribute__((noinline));
extern int npPushLibContextReleaseUser(NpPushLibContext *ctx, const NpPushKey *key, int *pCount)
	__attribute__((noinline));
extern int npPushLibContextStartNotification(NpPushLibContext *ctx, const NpPushKey *key);
extern int npPushLibContextStopNotification(NpPushLibContext *ctx, const NpPushKey *key);
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
extern int npPushIpcRegisterId(NpWebApiIpmiClient *client, const void *data);
extern int npPushRegisterUser(SceNpOnlineId *onlineId);	/* 0x8100987a, not written yet */
extern int npPushUnregisterUser(SceNpOnlineId *onlineId);	/* 0x810098d4, not written yet */
extern int npWebApiUserContextReleaseResources(NpWebApiUserContext *userCtx);
extern int npWebApiCreateUserContextObject(int libCtxId, const SceNpOnlineId *onlineId, unsigned char failIfExists);
extern int npWebApiUserContextHasBusyRequests(NpWebApiUserContext *userCtx);
extern int npWebApiUserContextAbortRequests(NpWebApiUserContext *userCtx);
extern int npWebApiDeleteUserContextObject(int userCtxId);

/* a list of objects hung off a library context; each node's next is at +168 */
typedef struct NpWebApiObjectNode {
	unsigned char reserved0[168];
	struct NpWebApiObjectNode *next;
} NpWebApiObjectNode;

typedef struct NpWebApiObjectList {
	const void *vtable;
	NpWebApiLibContext *libContext;
	NpWebApiObjectNode *head;
	int count;
} NpWebApiObjectList;

extern int npWebApiConvertHttpError(int id, int error);
extern int npWebApiIsUserProfileService(const char *service);
extern int npWebApiIsAccountsService(const char *service);
extern NpWebApiObjectList *npWebApiObjectListConstruct(NpWebApiObjectList *list, NpWebApiLibContext *libCtx);
extern NpWebApiObjectList *npWebApiObjectListDestruct(NpWebApiObjectList *list);
extern int npWebApiObjectListDeleteThis(NpWebApiObjectList *list);
extern const void *g_objectListVtable[];	/* 0x8100ef60 */

/* the extended push event filters' service registry (global 0x81010038): the services and the names they know */
#define NP_WEBAPI_ERROR_REGISTRY_NOT_FOUND	-2141871614	/* 0x8055a202 */
#define NP_WEBAPI_ERROR_REGISTRY_ENTRY_NOT_FOUND	-2141871562	/* 0x8055a236 */
#define NP_WEBAPI_ERROR_REGISTRY_NAME_EXISTS	-2141871563	/* 0x8055a235 */
#define NP_WEBAPI_ERROR_REGISTRY_SERVICE_TOO_LONG	-2141871578	/* 0x8055a226 */
#define NP_WEBAPI_ERROR_REGISTRY_NAME_TOO_LONG	-2141871571	/* 0x8055a22d */

/* what the filters keep of an entry: its object and the entry */
typedef struct NpWebApiExtdService {
	const void *const *vtable;
	NpWebApiExtdEntry *entry;
} NpWebApiExtdService;

typedef char NpWebApiExtdNameSizeCheck[sizeof(NpWebApiExtdName) == 48 ? 1 : -1];
typedef char NpWebApiExtdEntrySizeCheck[sizeof(NpWebApiExtdEntry) == 92 ? 1 : -1];

extern const void *g_extdNameVtable[];		/* 0x8100f0e0 */
extern const void *g_extdEntryVtable[];		/* 0x8100f0f0 */
extern const void *g_extdServiceVtable[];	/* 0x8100f088 */

extern void *npWebApiExtdEntryClearService(void *service) __attribute__((noinline));
extern int npWebApiExtdEntrySetService(char *dst, const char *src) __attribute__((noinline));
extern void *npWebApiExtdNameClear(void *name) __attribute__((noinline));
extern int npWebApiExtdNameSet(char *dst, const char *src) __attribute__((noinline));
extern int npWebApiExtdNameEquals(const char *a, const char *b) __attribute__((noinline));
extern NpWebApiExtdName *npWebApiExtdNameDestruct(NpWebApiExtdName *name) __attribute__((noinline));
extern int npWebApiExtdNameDelete(NpWebApiExtdName *name) __attribute__((noinline));
extern NpWebApiExtdEntry *npWebApiExtdEntryDestruct(NpWebApiExtdEntry *entry) __attribute__((noinline));
extern int npWebApiExtdEntryDelete(NpWebApiExtdEntry *entry) __attribute__((noinline));
extern NpWebApiExtdService *npWebApiExtdServiceDestruct(NpWebApiExtdService *service) __attribute__((noinline));
extern int npWebApiExtdServiceDelete(NpWebApiExtdService *service) __attribute__((noinline));
extern int npWebApiExtdEntryListAdd(NpWebApiExtdEntryList *list, void *pool, const char *service,
	NpWebApiExtdEntry **pEntry) __attribute__((noinline));
extern int npWebApiExtdEntryListRemove(NpWebApiExtdEntryList *list, NpWebApiExtdEntry *entry) __attribute__((noinline));
extern int npWebApiExtdEntryAddName(NpWebApiExtdEntry *entry, const char *name) __attribute__((noinline));
extern int npWebApiExtdRegistryAddEntry(NpPushLibContext *registry, void *pool, const char *service,
	NpWebApiExtdEntry **pEntry) __attribute__((noinline));
extern int npWebApiExtdRegistryRemoveEntry(NpPushLibContext *registry, NpWebApiExtdEntry *entry) __attribute__((noinline));
extern int npWebApiExtdRegistryAddName(NpPushLibContext *registry, NpWebApiExtdEntry *entry,
	const char *name) __attribute__((noinline));
extern int npWebApiExtdServiceCreate(void *pool, const char *service, NpWebApiExtdService **pService) __attribute__((noinline));
extern int npWebApiExtdServiceAddName(NpWebApiExtdService *service, const char *name) __attribute__((noinline));
extern int npWebApiExtdServiceDestroy(NpWebApiExtdService *service) __attribute__((noinline));

extern const void *g_extdFilterVtable[];	/* 0x8100ef70 */
extern NpWebApiExtdFilter *npWebApiExtdFilterConstruct(NpWebApiExtdFilter *filter, NpWebApiLibContext *libCtx)
	__attribute__((noinline));
extern NpWebApiExtdFilter *npWebApiExtdFilterDestruct(NpWebApiExtdFilter *filter) __attribute__((noinline));
extern int npWebApiExtdFilterDelete(NpWebApiExtdFilter *filter) __attribute__((noinline));
extern int npWebApiExtdFilterResolve(NpWebApiExtdFilter *filter, NpWebApiHandle *handle, const char *name,
	int arg) __attribute__((noinline));
extern int npWebApiExtdFilterInit(NpWebApiExtdFilter *filter, NpWebApiHandle *handle, const char *name, int arg,
	const char *records, unsigned int count) __attribute__((noinline));
extern int npWebApiLibContextCreateExtdPushEventFilter(NpWebApiLibContext *ctx, int handleId, const char *name,
	int arg, const char *records, unsigned int count) __attribute__((noinline));

/* the user context's callbacks: registered for a filter, found by their own ID */
extern int npWebApiLibContextHasPushEventFilter(NpWebApiLibContext *ctx, int filterId);
extern int npWebApiLibContextHasServicePushEventFilter(NpWebApiLibContext *ctx, int filterId);
extern int npWebApiUserContextRegisterPushEventCallback(NpWebApiUserContext *userCtx, int filterId,
	SceNpWebApiPushEventCallback cb, void *arg);
extern int npWebApiUserContextUnregisterPushEventCallbackOld(NpWebApiUserContext *userCtx, int callbackId);
extern int npWebApiUserContextUnregisterPushEventCallback(NpWebApiUserContext *userCtx, int callbackId);
extern int npWebApiUserContextRegisterServicePushEventCallback(NpWebApiUserContext *userCtx, int filterId,
	SceNpWebApiServicePushEventCallback cb, SceNpWebApiServicePushEventCallback cbEx, void *arg);
extern int npWebApiUserContextUnregisterServicePushEventCallbackOld(NpWebApiUserContext *userCtx, int callbackId);
extern int npWebApiUserContextUnregisterServicePushEventCallback(NpWebApiUserContext *userCtx, int callbackId);
extern int npWebApiUserContextRegisterExtdPushEventCallback(NpWebApiUserContext *userCtx, int filterId,
	SceNpWebApiExtdPushEventCallback cb, void *arg);
extern int npWebApiUserContextUnregisterExtdPushEventCallbackOld(NpWebApiUserContext *userCtx, int callbackId);
extern int npWebApiUserContextUnregisterExtdPushEventCallback(NpWebApiUserContext *userCtx, int callbackId);

extern void npWebApiCallbackListInsert(NpWebApiCallbackNode **pResult, NpWebApiCallbackList *list,
	NpWebApiCallbackRecord *record) __attribute__((noinline));
extern int npWebApiCallbackListErase(NpWebApiCallbackNode **pResult, NpWebApiCallbackList *list,
	NpWebApiCallbackNode **pAt) __attribute__((noinline));
extern void npWebApiServiceCallbackListInsert(NpWebApiServiceCallbackNode **pResult, NpWebApiServiceCallbackList *list,
	NpWebApiServiceCallbackRecord *record) __attribute__((noinline));
extern int npWebApiServiceCallbackListErase(NpWebApiServiceCallbackNode **pResult, NpWebApiServiceCallbackList *list,
	NpWebApiServiceCallbackNode **pAt) __attribute__((noinline));
extern void npWebApiExtdCallbackListInsert(NpWebApiCallbackNode **pResult, NpWebApiCallbackList *list,
	NpWebApiCallbackRecord *record) __attribute__((noinline));
extern int npWebApiExtdCallbackListErase(NpWebApiCallbackNode **pResult, NpWebApiCallbackList *list,
	NpWebApiCallbackNode **pAt) __attribute__((noinline));

#endif
