/*
 * SceNpCommerce2: the NP commerce client, a library of the PlayStation Store's web API.
 * Sony built it with SNC 3.57. No debug record names its files or functions: the exports'
 * names are the SDK's (np/np_commerce2.h) where it has one, the rest are the NID's; the
 * internal names are ours, taken from what the code does.
 */
#ifndef NP_COMMERCE2_INTERNAL_H
#define NP_COMMERCE2_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <kernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <kernel/sysmem.h>
#include <libkernel/libkernel.h>
#include <libhttp.h>
#include <np/np_common.h>
#include <np/np_error.h>
#include <np/np_commerce2.h>
#include <np_common/np_common.h>

/* the libkernel mspace calls the tree has no header of yet */
typedef void *SceClibMspace;
extern void *sceClibMspaceMalloc(SceClibMspace mspace, SceSize size);
extern void sceClibMspaceFree(SceClibMspace mspace, void *ptr);
extern SceClibMspace sceClibMspaceCreate(void *base, SceSize size);
extern int sceClibMspaceDestroy(SceClibMspace mspace);

extern int sceHttpsEnableOptionPrivate(int id, SceUInt32 sslFlags);
extern int sceHttpsDisableOptionPrivate(int id, SceUInt32 sslFlags);

/* the imports SceNpCommerce2 takes from libraries the tree has no header of yet */
extern void SceNpManager_9554802B(const void *data1, const void *data2, const void *data3);

/* the request kinds the Start and GetResult exports of one operation come in */
#define NP_COMMERCE2_VERSION_1		0
#define NP_COMMERCE2_VERSION_2		1
#define NP_COMMERCE2_VERSION_3		2

/* a request kind the operation does not have */
#define NP_COMMERCE2_ERROR_INVALID_VERSION	-2141909100	/* 0x80550f94 */

/* the request does not name what it is for */
#define NP_COMMERCE2_ERROR_NO_TARGET	-2141908950	/* 0x805510aa */

/* a store check was asked for a target type it does not have */
#define NP_COMMERCE2_ERROR_INVALID_TARGET_TYPE	-2141909098	/* 0x80550f96 */

/* a name did not fit the buffer it was copied to */
#define NP_COMMERCE2_ERROR_BUF_TOO_SMALL	-2141909116	/* 0x80550f84 */

extern int SceNpCommon_F585C86C(void);
extern int SceNpCommon_187C0A8D(int *value);

/* the library's memory: a 8 KiB memory block with an mspace over it */
#define NP_COMMERCE2_HEAP_SIZE		0x2000
#define NP_COMMERCE2_HEAP_MEMTYPE	0x0C20D060

/* what the session result's accessor (export 64E27331) reads out of the response */
typedef struct NpCommerce2SessionData {
	SceNpCommerce2CommonData commonData;
	const char *currencyCode;
	const char *currencySymbol;
	SceUInt16 decimals;
	SceUInt16 padding1;
	SceUInt32 reserved1[3];
	SceUInt32 symbolPosition;
	SceUInt32 symbolWithSpace;
	SceUInt32 reserved2;
	const char *thousandSeparator;
	const char *decimalLetter;
} NpCommerce2SessionData;

extern int g_libInitialized;
extern SceUID g_heapMemBlock;
extern SceClibMspace g_heap;
extern int npCommerce2Initialize(const char *environment, int npCommonValue, int environmentType);
extern int npCommerce2Finalize(void);
extern int npCommerce2InitTable(const char *environment, int npCommonValue, int environmentType);
extern int npCommerce2TermTable(void);
extern SceKernelLwMutexWork g_tableLock;
extern int npCommerce2_8101D9AC(SceKernelLwMutexWork *lock);

/* exports the library's own exports call (commerce2.c) */
extern int SceNpCommerce2_1BFF9C73(int a1, void *a2, int a3);
extern int SceNpCommerce2_64E27331(int a1, int a2, int a3);
extern int SceNpCommerce2_F6A4FC50(int a1, int a2);
extern int SceNpCommerce2_EC09FF89(int a1, int a2);
extern int SceNpCommerce2_9E082DB2(int a1, int a2);
extern int SceNpCommerce2_F988A255(const SceNpCommerce2GetProductInfoResult *result,
				   SceNpCommerce2GameProductInfo *gameProductInfo);
extern int SceNpCommerce2_C4CC8E22(int a1, int a2, int a3);

extern int g_shellReady;
extern int g_shellLockReady;
extern SceKernelLwMutexWork g_shellLock;
extern int SceShellSvc_D943CE15(int event, int (*callback)(int), int arg);
extern int SceShellSvc_7CBF442B(int event);
extern int npCommerce2_81018A7E(int a1);
extern int npCommerce2_81018A88(int a1);
extern int npCommerce2_81017DCC(void);
extern void npCommerce2_81017EF8(int a1);
extern SceUInt32 g_checkCtxId;
extern SceUInt32 g_checkReqId;
extern int g_checkAborted;
extern int SceShellSvc_B31E7F1C(void);
extern int npCommerce2_8101EB8E(int a1, int a2);
extern int npCommerce2_8101EBDE(int a1);
extern int npCommerce2_81017F7A(void);
extern int npCommerce2_81017F9C(void);
extern int npCommerce2_81017FC2(int a1);

/* Init's settings and the heap (settings.c) */
#define NP_COMMERCE2_ERROR_NO_ENVIRONMENT	-2141909109	/* 0x80550f8b */
#define NP_COMMERCE2_ERROR_ENVIRONMENT_TOO_LONG	-2141909108	/* 0x80550f8c */
extern int g_npCommonValue;
extern int g_environmentType;
extern char g_npEnvironment[17];
extern int npCommerce2SetNpEnvironment(const char *environment);
extern int npCommerce2SetNpCommonValue(int value);
extern int npCommerce2SetEnvironmentType(int type);
extern void *npCommerce2Alloc(SceSize size);
extern void npCommerce2Free(void *ptr);

/* a request of a context: the HTTP request it runs and the shell slot it reports to */
typedef struct NpCommerce2Request {
	struct NpCommerce2Request *next;
	int refCount;
	SceUInt32 id;			/* the context's ID in the high half */
	SceUInt32 flags;		/* NP_COMMERCE2_REQUEST_FLAG_* */
	int httpRequest;
	SceUInt16 type;
	SceUInt16 reserved1;
	int shellSlot;
} NpCommerce2Request;

#define NP_COMMERCE2_REQUEST_FLAG_REMOVED	0x1
#define NP_COMMERCE2_REQUEST_FLAG_ABORTED	0x2

/* a context: the HTTP template and connection its requests run on, and their list */
typedef struct NpCommerce2Context {
	struct NpCommerce2Context *next;
	int refCount;
	SceUInt32 version;
	SceUInt32 id;
	SceUInt32 flags;		/* NP_COMMERCE2_CONTEXT_FLAG_* */
	SceUInt32 reserved1;
	int httpTemplate;
	int httpConnection;
	NpCommerce2Request *requests;
	char text[0x100];
	char response[0x400];
	SceUInt32 responseSize;
	SceUInt32 mode;
	SceUInt32 reserved2[3];
	SceUInt32 kind;			/* 0: the context of the library, 1: the shell's */
	SceUInt32 reserved3;
	char currencyCode[4];
	char countryCode[4];
	SceUInt32 sslCreated;
	SceInt32 sslShortfall;
	SceInt32 httpShortfall;
	SceUInt32 responseKind;
	SceUInt32 reserved4;
	SceUInt32 reserved5;
} NpCommerce2Context;

#define NP_COMMERCE2_CONTEXT_FLAG_REMOVED	0x1

/* the object table (object_table.c, request_table.c) */
extern int npCommerce2AddContext(SceKernelLwMutexWork *lock, NpCommerce2Context *ctx, SceUInt32 *ctxId, int kind);
extern int npCommerce2GetContext(SceKernelLwMutexWork *lock, SceUInt32 ctxId, NpCommerce2Context **ctx, int notRemoved);
extern int npCommerce2ReleaseContext(SceKernelLwMutexWork *lock, NpCommerce2Context *ctx);
extern int npCommerce2UnlinkContext(SceKernelLwMutexWork *lock, SceUInt32 ctxId);
extern int npCommerce2DestroyAllContexts(SceKernelLwMutexWork *lock);
extern int npCommerce2CreateHttpTemplate(int *httpTemplate, int *httpConnection);
extern int npCommerce2DeleteHttpTemplate(int httpTemplate, int httpConnection);
extern int npCommerce2GetRequest(SceKernelLwMutexWork *lock, SceUInt32 reqId, NpCommerce2Request **req);
extern void npCommerce2ReleaseRequest(SceKernelLwMutexWork *lock, NpCommerce2Request *req);
extern int npCommerce2RemoveRequest(SceKernelLwMutexWork *lock, SceUInt32 reqId);
extern int npCommerce2RemoveAllRequests(SceKernelLwMutexWork *lock, SceUInt32 ctxId);
extern int npCommerce2DeleteHttpRequest(int httpRequest);
extern int npCommerce2AbortHttpRequest(int httpRequest);
extern int npCommerce2DeleteShellSlot(int slot);
extern int npCommerce2AbortShellSlot(int slot);
extern NpCommerce2Context *g_contextList;
extern SceUInt32 g_lastContextId;

#define NP_COMMERCE2_ERROR_CTX_MAX_REACHED	SCE_NP_COMMERCE2_ERROR_CTX_MAX

/* set by Init, cleared by Term */
extern int g_initialized;

/* the context and request tables (commerce2.c) */
extern int npCommerce2CreateContext(SceUInt32 version, SceUInt32 *ctxId, int shell);
extern int npCommerce2DestroyContext(SceUInt32 ctxId);
extern int npCommerce2DestroyRequest(SceUInt32 reqId);
extern int npCommerce2AbortRequest(SceUInt32 reqId);
extern int npCommerce2DestroyAllContextsLocked(void);

/* request builders and result readers not understood well enough to name yet; named by address */
extern int npCommerce2_8100D4EC(int a1, void *a2, int a3);
extern int npCommerce2_8100D61A(int a1, int a2, int a3);
extern int npCommerce2_81005E40(int type, SceUInt32 reqId, void *buf, SceSize bufLen,
				SceSize *fillSize);
extern int npCommerce2_810062FA(SceUInt32 reqId, const char *id, int a3, int a4, int a5, int a6);
extern int npCommerce2GetResultData(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize);
extern int npCommerce2_8100DAB0(SceNpCommerce2GetCategoryContentsResult *result, void *buf,
				SceSize bufLen);
extern int npCommerce2_8100DD78(const SceNpCommerce2GetCategoryContentsResult *result,
				SceNpCommerce2CategoryInfo *categoryInfo);
extern int npCommerce2GetEnvironment(void);
extern int npCommerce2_8100DDCA(const SceNpCommerce2GetCategoryContentsResult *result,
				SceUInt32 index, SceNpCommerce2ContentInfo *contentInfo);
extern int npCommerce2_8100DE5C(const SceNpCommerce2ContentInfo *contentInfo,
				SceNpCommerce2CategoryInfo *categoryInfo);
extern int npCommerce2_8100DEB4(int a1, int a2);
extern int npCommerce2_8100E272(int a1, void *a2, int a3);
extern int npCommerce2_8100E5FA(int a1, int a2);
extern int npCommerce2_8100E6F4(const void *info, SceNpCommerce2ContentRatingInfo *contentRatingInfo);
extern int npCommerce2_8100E746(int a1, int a2, int a3);
extern int npCommerce2_8100E868(int a1, int a2, int a3);
extern int npCommerce2_8100ED8C(int a1, void *a2, int a3);
extern int npCommerce2_8101059C(int a1, int a2, int a3);
extern int npCommerce2_810102FC(int a1, int a2, int a3);
extern int npCommerce2_81010620(int a1, int a2, int a3);
extern int npCommerce2_810100C2(int a1, int a2, int a3);
extern int npCommerce2_8100F1D6(int a1, int a2);
extern char g_shortText[0x39];
extern char g_longText[0xFA1];
extern int npCommerce2_8100646C(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100647A(SceUInt32 reqId, const char *categoryId, const char *productId,
				int a4, int a5);
extern int npCommerce2_8100F316(SceNpCommerce2GetProductInfoResult *result, void *buf,
				SceSize bufLen);
extern int npCommerce2_8100F38C(int a1, int a2);
extern int npCommerce2_8100F416(int a1, void *a2, int a3);
extern int npCommerce2_8100F740(const SceNpCommerce2GetProductInfoResult *result,
				SceNpCommerce2GameProductInfo *gameProductInfo);
extern int npCommerce2_8100F798(int a1, int a2, int a3);
extern int npCommerce2_8100F8D8(int a1, int a2, void *a3, int a4);
extern int npCommerce2_8101144E(int a1, int a2, int a3, int a4);
extern int npCommerce2_810114C6(int a1, int a2, int a3);
extern int npCommerce2_8100FE34(int a1, int a2, int a3);
extern int npCommerce2_8100FE7C(int a1, int a2, int a3);
extern int npCommerce2_8100FF6E(int a1, int a2, int a3);
extern int npCommerce2_81010B14(int a1, int a2);
extern int npCommerce2_81010BEC(int a1, int a2);
extern int npCommerce2_81010DD8(int a1, int a2, int a3, int a4);
extern int npCommerce2_8100FDBC(const SceNpCommerce2GameProductInfo *gameProductInfo, int dataType,
				SceUInt32 idx, SceNpCommerce2GameSkuInfo *gameSkuInfo);
extern int npCommerce2_810065CC(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810065DA(int a1, int a2, int a3, int a4);
extern int npCommerce2_81010938(int a1, void *a2, int a3);
extern int npCommerce2_81010C98(int a1, int a2, int a3);
extern int npCommerce2_81010FFA(int a1, int a2);
extern int npCommerce2_8101117E(int a1, void *a2, int a3);
extern int npCommerce2_81010710(int a1, int a2, void *a3, int a4);
extern int npCommerce2_8100672C(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100673A(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8101150E(int a1, void *a2, int a3);
extern int npCommerce2_810115FC(int a1, int a2);
extern int npCommerce2_81011604(int a1, int a2);
extern int npCommerce2_81006872(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81006880(int a1, int a2);
extern int npCommerce2_810069B2(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810069C0(SceUInt32 reqId);
extern int npCommerce2_8101160C(int a1, void *a2, int a3);
extern int npCommerce2_810116FA(int a1, int a2);
extern int npCommerce2_81011702(int a1, int a2);
extern int npCommerce2_81006AF2(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81006B0E(SceUInt32 a1, void *a2, SceSize a3, SceSize *a4);
extern int npCommerce2_8101170A(int a1, void *a2, int a3);
extern int npCommerce2_810117FC(int a1, int a2);
extern int npCommerce2_810118B0(int a1, int a2);
extern int npCommerce2_810118A2(int a1, int a2);
extern int npCommerce2_81006B00(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810118B8(int a1, void *a2, int a3);
extern int npCommerce2_810119A0(int a1, int a2);
extern int npCommerce2_810119A8(int a1, int a2);
extern int npCommerce2_810119B0(int a1, int a2);
extern int npCommerce2_81006C56(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81006C64(SceUInt32 reqId);
extern int npCommerce2_810119BE(int a1, void *a2, int a3);
extern int npCommerce2_81011C24(int a1, int a2);
extern int npCommerce2_81006D96(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81006DA4(SceUInt32 reqId);
extern int npCommerce2_81011C2C(int a1, void *a2, int a3);
extern int npCommerce2_81011D84(int a1, int a2, int a3);
extern int npCommerce2_81011D8C(int a1, int a2, int a3);
extern int npCommerce2_81006ED6(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81006F00(int a1, int a2, int a3);
extern int npCommerce2_81011E42(int a1, void *a2, int a3);
extern int npCommerce2_81011FC8(int a1, int a2, int a3);
extern int npCommerce2_81007038(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007046(int a1, int a2, int a3, int a4, int a5);
extern int npCommerce2_81011FD0(int a1, void *a2, int a3);
extern int npCommerce2_8101209C(int a1, int a2, int a3);
extern int npCommerce2_8101284E(int a1, int a2, int a3);
extern int npCommerce2_810071C0(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810071EA(SceUInt32 reqId);
extern int npCommerce2_810128C8(int a1, void *a2, int a3);
extern int npCommerce2_8100731C(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007462(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007470(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81012A5C(int a1, void *a2, int a3);
extern int npCommerce2_810075A8(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810075B6(SceUInt32 reqId);
extern int npCommerce2_81012B60(int a1, void *a2, int a3);
extern int npCommerce2_81012BD6(int a1, int a2);
extern int npCommerce2_8100732A(int a1, const int *a2);
extern int npCommerce2_810076E8(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007704(int a1, int a2);
extern int npCommerce2_81012BDE(int a1, void *a2, int a3);
extern int npCommerce2_81012CE8(int a1, int a2);
extern int npCommerce2_810076F6(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81012CF0(int a1, void *a2, int a3);
extern int npCommerce2_81012D66(int a1, int a2);
extern int npCommerce2_8100783C(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007988(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100784A(int a1, int a2, int a3);
extern int npCommerce2_810079A4(int a1, int a2, int a3, int a4);
extern int npCommerce2_81012E3C(int a1, void *a2, int a3);
extern int npCommerce2_81012F80(int a1, int a2);
extern int npCommerce2_81013012(int a1, int a2);
extern int npCommerce2_8101301A(int a1, int a2, int a3);
extern int npCommerce2_81013362(int a1, void *a2, int a3);
extern int npCommerce2_81013432(int a1, int a2, int a3);
extern int npCommerce2_81007996(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81013952(int a1, void *a2, int a3);
extern int npCommerce2_81013A22(int a1, int a2);
extern int npCommerce2_81013A2A(int a1, int a2);
extern int npCommerce2_810071CE(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81013A32(int a1, void *a2, int a3);
extern int npCommerce2_81007AF6(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007B04(SceUInt32 a1, void *a2, SceSize a3, SceSize *a4);
extern int npCommerce2_81007C4C(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007C5A(int a1, int a2, int a3, int a4);
extern int npCommerce2_81013BC6(int a1, void *a2, int a3);
extern int npCommerce2_81013C92(int a1, int a2, int a3);
extern int npCommerce2_81007DB6(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007DC4(SceUInt32 reqId);
extern int npCommerce2_81013D24(int a1, void *a2, int a3);
extern int npCommerce2_81013DAE(int a1, int a2, int a3);
extern int npCommerce2_81013E86(int a1, int a2, int a3);
extern int npCommerce2_81013ECE(int a1, int a2, int a3);
extern int npCommerce2_8101413A(int a1, int a2);
extern int npCommerce2_81007ECC(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007EDA(int a1, int a2, int a3);
extern int npCommerce2_81013F58(int a1, void *a2, int a3);
extern int npCommerce2_810141C2(int a1, int a2, int a3);
extern int npCommerce2_81007FB0(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81014250(int a1, void *a2, int a3);
extern int npCommerce2_810080A0(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810080AE(SceUInt32 reqId);
extern int npCommerce2_810144A0(int a1, void *a2, int a3);
extern int npCommerce2_810145CE(int a1, int a2, int a3);
extern int npCommerce2_810081E0(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100834A(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81007FBE(int a1, int a2, int a3, int a4, int a5, int a6);
extern int npCommerce2_810081EE(int a1, int a2, int a3, int a4, int a5, int a6);
extern int npCommerce2_81008358(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81008490(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810085DC(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810085EA(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81008606(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810085F8(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100873E(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100849E(int a1, int a2, int a3, int a4);
extern int npCommerce2_8100874C(SceUInt32 reqId);
extern int npCommerce2_8101461E(int a1, void *a2, int a3);
extern int npCommerce2_81014694(int a1, int a2);
extern int npCommerce2_8100887E(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810089FC(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81008B42(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81008B50(int a1, int a2, int a3);
extern int npCommerce2_81008C8E(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81008C9C(SceUInt32 reqId);
extern int npCommerce2_8100888C(int a1, int a2, int a3, int a4);
extern int npCommerce2_81008A0A(int a1, const int *a2);
extern int npCommerce2_8101478E(int a1, void *a2, int a3);
extern int npCommerce2_81014804(int a1, int a2);
extern int npCommerce2_81008DCE(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81008DDC(int a1, int a2, int a3);
extern int npCommerce2_81008F1A(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81008F28(int a1, int a2, int a3);
extern int npCommerce2_81006EE4(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810148A2(int a1, void *a2, int a3);
extern int npCommerce2_81014A92(int a1, int a2, int a3);
extern int npCommerce2_81014A9A(int a1, int a2, int a3);
extern int npCommerce2_81014AA2(int a1, int a2);
extern int npCommerce2_81009066(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81009074(int a1, int a2, int a3, int a4);
extern int npCommerce2_810091D0(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810091DE(int a1, int a2, int a3);
extern int npCommerce2_810092E0(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810092EE(SceUInt32 reqId);
extern int npCommerce2_81014AAA(int a1, void *a2, int a3);
extern int npCommerce2_81014B20(int a1, int a2);
extern int npCommerce2_81009420(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100942E(int a1, int a2, int a3);
extern int npCommerce2_8100956C(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100957A(SceUInt32 reqId);
extern int npCommerce2_81014BCE(int a1, void *a2, int a3);
extern int npCommerce2_810096AC(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81014C44(int a1, void *a2, int a3);
extern int npCommerce2_81014D62(int a1, int a2, int a3);
extern int npCommerce2_81014D6A(int a1, int a2, int a3);
extern int npCommerce2_81014EB4(int a1, int a2, int a3);
extern int npCommerce2_810096BA(int a1, int a2, int a3, int a4);
extern int npCommerce2_810097C4(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810097D2(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81014F08(int a1, void *a2, int a3);
extern int npCommerce2_81014F92(int a1, int a2, int a3);
extern int npCommerce2_8100998C(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100999A(SceUInt32 reqId);
extern int npCommerce2_810151B8(int a1, void *a2, int a3);
extern int npCommerce2_8101522E(int a1, int a2);
extern int npCommerce2_8101545C(int a1, int a2, int a3);
extern int npCommerce2_81015606(int a1, int a2, int a3);
extern int npCommerce2_81015818(int a1, int a2, int a3);
extern int npCommerce2_810097E0(int a1, int a2, int a3, int a4, int a5, int a6);
extern int npCommerce2_8100F2F6(int a1, int a2);
extern int npCommerce2_81009AD4(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81009AE2(int a1, int a2, int a3);
extern int npCommerce2_8101591C(int a1, void *a2, int a3);
extern int npCommerce2_810159E8(int a1, int a2, int a3);
extern int npCommerce2_81009C20(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81009C2E(int a1, int a2, int a3, int a4);
extern int npCommerce2_81015BCE(int a1, void *a2, int a3);
extern int npCommerce2_81015C9A(int a1, int a2, int a3);
extern int npCommerce2_81009D76(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81009D84(int a1, int a2, int a3, int a4);
extern int npCommerce2_81015EB4(int a1, void *a2, int a3);
extern int npCommerce2_81015F80(int a1, int a2, int a3);
extern int npCommerce2_810162BA(int a1, int a2, int a3);
extern int npCommerce2_81009ECC(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81009EDA(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A012(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A020(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A158(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A166(SceUInt32 reqId);
extern int npCommerce2_810163AC(int a1, void *a2, int a3);
extern int npCommerce2_81016436(int a1, int a2, int a3);
extern int npCommerce2_8100A298(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A2A6(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8101652E(int a1, void *a2, int a3);
extern int npCommerce2_810165A4(int a1, int a2);
extern int npCommerce2_8100A380(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810165F0(int a1, void *a2, int a3);
extern int npCommerce2_8100AECA(int a1, int a2);
extern int npCommerce2_8100A2B4(int a1, int a2, int a3, int a4, int a5);
extern int npCommerce2_8100A38E(int a1, int a2, int a3, int a4);
extern int npCommerce2_8100A49A(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A83A(SceUInt32 reqId);
extern int npCommerce2_8100A4A8(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A5F4(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A602(SceUInt32 reqId);
extern int npCommerce2_81016666(int a1, void *a2, int a3);
extern int npCommerce2_8100B174(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100B182(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A734(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A4B6(int a1, const int *a2, int a3);
extern int npCommerce2_8100A81E(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81006EF2(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8101670C(int a1, void *a2, int a3);
extern int npCommerce2_810168FC(int a1, int a2, int a3);
extern int npCommerce2_81016904(int a1, int a2, int a3);
extern int npCommerce2_8101690C(int a1, int a2);
extern int npCommerce2_8100A82C(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A742(int a1, int a2, int a3, int a4, int a5, int a6);
extern int npCommerce2_810169A8(int a1, int a2, void *a3);
extern int npCommerce2_8100A982(int a1, const char **a2, int a3);
extern int npCommerce2_81016914(SceNpCommerce2GetProductInfoListResult *result, void *buf,
				SceSize bufLen);
extern int npCommerce2_810071DC(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100A974(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100DF44(int a1, int a2, int a3);
extern int npCommerce2_8100AAC0(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100AACE(int a1, int a2, int a3, int a4, SceUInt32 a5, SceUInt32 a6);
extern int npCommerce2_81016A3A(int a1, void *a2, int a3);
extern int npCommerce2_8100AC2A(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100AC38(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81016B50(int a1, void *a2, int a3);
extern int npCommerce2_8100AD70(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_81016BC6(int a1, void *a2, int a3);
extern int npCommerce2_81016C50(int a1, int a2, int a3);
extern int npCommerce2_8100AEBC(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100AD7E(int a1, int a2, int a3);
extern int npCommerce2_8100B002(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100B010(SceUInt32 reqId);
extern int npCommerce2_8100B166(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100B238(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_8100B3C6(int a1, int a2);
extern int npCommerce2_8100B292(int a1, int a2, int a3, int a4);
extern int npCommerce2_8100B372(int a1, int a2);
extern int npCommerce2_8100B41A(int a1, const char *a2);
extern int npCommerce2_8100B7A0(SceUInt32 ctxId, SceInt32 *shortfall);
extern int npCommerce2_8100B7F4(SceUInt32 ctxId, SceInt32 *shortfall);
extern int npCommerce2_8100D6BC(int a1, void *a2, int a3);
extern int npCommerce2_8100D8E4(int a1, int a2, int a3);
extern int npCommerce2_8100D8EC(int a1, int a2);
extern int npCommerce2_81013FEE(int a1, int a2);
extern int npCommerce2_810142C6(int a1, int a2);
extern int npCommerce2_8100D980(int a1, int a2, int a3);
extern int npCommerce2_810060C0(SceUInt32 ctxId, SceUInt32 *reqId);
extern int npCommerce2_810060CE(SceUInt32 reqId);
extern int npCommerce2_810062EC(SceUInt32 ctxId, SceUInt32 *reqId);

/* the session requests (CreateSession) */
extern int npCommerce2CreateSessionRequest(SceUInt32 ctxId, SceUInt32 *reqId, int version);
extern int npCommerce2StartSessionRequest(SceUInt32 reqId);
extern int npCommerce2GetResult(int type, SceUInt32 reqId, void *buf, SceSize bufLen,
				SceSize *fillSize);

#endif
