/*
 * npcommerce2_session_mgr.cpp: SceNpCommerce2SessionManager ShellSvc IPC server
 * (0x0E0000..0x0E001F, 0x81221D44 - 0x81224FD4).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string.h>
#include <libkernel/libkernel.h>
#include <np_common/np_common.h>
#include "../../../plugins.h"
#include "../../livespace_db/lsdb_keys.h"
#include "np_ipc_internal.h"

#define NP_COMM2_MAX_ACTIVE_REQS	16
#define NP_COMM2_MAX_SESSION_SLOTS	16
#define NP_COMM2_STATIC_TYPE_COUNT	13
#define NP_COMM2_QUEUE_MAX		8
#define NP_COMM2_RESP_BUF_SIZE		1024
#define NP_COMM2_URL_BUF_SIZE		256
#define NP_COMM2_SDK_SIGNIN_THRESHOLD	0x02000000u

/* topmenu_plugin store icon slots (offsets +220 and +224) */
#define TOPMENU_PLUGIN_FUNC_SHOW_STORE_ICON	55
#define TOPMENU_PLUGIN_FUNC_HIDE_STORE_ICON	56

/* Commerce2 ShellSvc IPC methods (0x0E0000..0x0E001F) */
#define NP_COMM2_IPC_INC_EPOCH			0x000E0000
#define NP_COMM2_IPC_GET_EPOCH			0x000E0001
#define NP_COMM2_IPC_INVALIDATE_TYPE		0x000E0002
#define NP_COMM2_IPC_GET_TYPE_GEN		0x000E0003
#define NP_COMM2_IPC_START_SESSION		0x000E0004
#define NP_COMM2_IPC_START_SESSION_SHARED	0x000E0005
#define NP_COMM2_IPC_START_SESSION_ALT		0x000E0006
#define NP_COMM2_IPC_GET_SESSION		0x000E0007
#define NP_COMM2_IPC_GET_SESSION_SHARED		0x000E0008
#define NP_COMM2_IPC_GET_SESSION_ALT		0x000E0009
#define NP_COMM2_IPC_START_VOUCHER		0x000E000A
#define NP_COMM2_IPC_GET_VOUCHER		0x000E000B
#define NP_COMM2_IPC_START_PRODUCT_INFO		0x000E000C
#define NP_COMM2_IPC_GET_PRODUCT_INFO		0x000E000D
#define NP_COMM2_IPC_START_PRODUCT_INFO_EX	0x000E000E
#define NP_COMM2_IPC_GET_PRODUCT_INFO_EX	0x000E000F
#define NP_COMM2_IPC_START_PRODUCT_INFO_ALT	0x000E0010
#define NP_COMM2_IPC_GET_PRODUCT_INFO_ALT	0x000E0011
#define NP_COMM2_IPC_START_PRODUCT_LIST		0x000E0012
#define NP_COMM2_IPC_GET_PRODUCT_LIST		0x000E0013
#define NP_COMM2_IPC_START_CATEGORY		0x000E0014
#define NP_COMM2_IPC_GET_CATEGORY		0x000E0015
#define NP_COMM2_IPC_START_BROWSE		0x000E0016
#define NP_COMM2_IPC_GET_BROWSE			0x000E0017
#define NP_COMM2_IPC_START_ENTITLEMENT		0x000E0018
#define NP_COMM2_IPC_GET_ENTITLEMENT		0x000E0019
#define NP_COMM2_IPC_START_EMPTY_STORE		0x000E001A
#define NP_COMM2_IPC_GET_EMPTY_STORE		0x000E001B
#define NP_COMM2_IPC_ABORT_REQUEST		0x000E001C
#define NP_COMM2_IPC_IS_REQUEST_ACTIVE		0x000E001D
#define NP_COMM2_IPC_SHOW_STORE_ICON		0x000E001E
#define NP_COMM2_IPC_HIDE_STORE_ICON		0x000E001F

/* Commerce2 request kinds (1..13) */
enum NpCommerce2ReqKind {
	NP_COMM2_KIND_SESSION		= 1,
	NP_COMM2_KIND_SESSION_SHARED	= 2,
	NP_COMM2_KIND_VOUCHER		= 3,
	NP_COMM2_KIND_PRODUCT_INFO	= 4,
	NP_COMM2_KIND_PRODUCT_LIST	= 5,
	NP_COMM2_KIND_CATEGORY		= 6,
	NP_COMM2_KIND_BROWSE		= 7,
	NP_COMM2_KIND_EMPTY_STORE	= 8,
	NP_COMM2_KIND_PRODUCT_INFO_EX	= 9,
	NP_COMM2_KIND_PRODUCT_INFO_ALT	= 10,
	NP_COMM2_KIND_SESSION_ALT	= 11,
	NP_COMM2_KIND_ENTITLEMENT	= 12,
	NP_COMM2_KIND_SHUTDOWN		= 13,
};

/* Request payload layouts (matching vsh/app/common/src/np/np_commerce2) */
struct NpCommerce2CodesPayload {
	SceUInt32 size;
	char countryCode[4];
	char langCode[4];
	int startPos;
	int maxCount;
	int sortMode;
	int filter;
};

struct NpCommerce2PairPayload {
	SceUInt32 size;
	int mode;
	char productId[259];
	char categoryId[261];
	int flags;
};

struct NpCommerce2NamesPayload {
	SceUInt32 size;
	char categoryId[259];
	char subCategory[261];
};

struct NpCommerce2BrowsePayload {
	SceUInt32 size;
	char targetId[260];
	int startPos;
	int maxCount;
	int flags;
};

struct NpCommerce2OnePayload {
	SceUInt32 size;
	char entitlementId[260];
};

struct NpCommerce2EmptyStorePayload {
	SceUInt32 size;
	int targetKind;
};

/* Queued work item (548 bytes) */
struct NpCommerce2QueueItem {
	IPMI::Session *session;		/* +0x00 */
	SceUID clientPid;		/* +0x04 */
	int kind;			/* +0x08 */
	union {
		SceUInt8 raw[532];
		NpCommerce2CodesPayload codes;
		NpCommerce2PairPayload pair;
		NpCommerce2NamesPayload names;
		NpCommerce2BrowsePayload browse;
		NpCommerce2OnePayload one;
		NpCommerce2EmptyStorePayload emptyStore;
	} payload;			/* +0x0C..+0x21F */
	SceUID netCtlPid;		/* +0x220 */
};

/* Queue linked-list node (16 bytes) */
struct NpCommerce2QueueNode {
	int requestId;
	NpCommerce2QueueNode *next;
	void *itemBuf;
	SceSize itemSize;
};

/* Result cache entry (32 bytes) */
struct NpCommerce2CacheSlot {
	IPMI::Session *ownerSession;	/* +0x00 (used by per-session slots) */
	SceUInt32 ctxType;		/* +0x04 */
	int coreCtxId;			/* +0x08 */
	SceUInt8 *respBuf;		/* +0x0C */
	SceSize respCapacity;		/* +0x10 */
	SceSize respSize;		/* +0x14 */
	int lastResult;			/* +0x18 */
	int generation;			/* +0x1C */
};

/* Active request tracker (16 bytes) */
struct NpCommerce2ActiveReq {
	int inUse;
	int requestId;
	int coreReqId;
	int aborted;
};

/* Session manager state (48 bytes at 0x815A33D8) */
struct NpCommerce2SessionManager {
	SceUID threadId;		/* +0x00 */
	SceUInt32 reserved;		/* +0x04 */
	SceKernelLwMutexWork mutex;	/* +0x08 (32 bytes) */
	int mutexCreated;		/* +0x28 */
	SceUInt32 pad;			/* +0x2C */
};

/* Static response buffers (0x815A3408..0x815AA008) */
static SceUInt8 s_sessionRespBufs[NP_COMM2_MAX_SESSION_SLOTS][NP_COMM2_RESP_BUF_SIZE];
static SceUInt8 s_sharedSessionBuf[NP_COMM2_RESP_BUF_SIZE];
static SceUInt8 s_altSessionBuf[NP_COMM2_RESP_BUF_SIZE];
static SceUInt8 s_voucherBuf[NP_COMM2_RESP_BUF_SIZE];
static SceUInt8 s_productInfoBuf[NP_COMM2_RESP_BUF_SIZE];
static SceUInt8 s_productListBuf[NP_COMM2_RESP_BUF_SIZE];
static SceUInt8 s_categoryBuf[NP_COMM2_RESP_BUF_SIZE];
static SceUInt8 s_browseBuf[NP_COMM2_RESP_BUF_SIZE];
static SceUInt8 s_entitlementBuf[NP_COMM2_RESP_BUF_SIZE];
static SceUInt8 s_emptyStoreBuf[NP_COMM2_RESP_BUF_SIZE];
static SceUInt8 s_productInfoExBuf[NP_COMM2_RESP_BUF_SIZE];
static SceUInt8 s_productInfoAltBuf[NP_COMM2_RESP_BUF_SIZE];

static int s_comm2Initialized = 0;				/* 0x815451B0 */
static NpCommerce2QueueNode *s_comm2QueueHead = NULL;		/* 0x815451B4 */
static SceUID s_comm2QueueSema = -1;				/* 0x815451B8 */
static SceUInt32 s_comm2QueueLimit = 0;				/* 0x815451BC */
static int s_comm2CurrentCoreReq = 0;				/* 0x815451C0 */
static int s_comm2Epoch = 0;					/* 0x815451C4 */

/* 13 static cache slots indexed by NpCommerce2ReqKind (0x815451C8) */
static NpCommerce2CacheSlot s_staticSlots[NP_COMM2_STATIC_TYPE_COUNT] = {
	{ NULL, 9, 0, NULL, 0, 0, 0, 0 },
	{ NULL, 3, 0, s_sessionRespBufs[0], NP_COMM2_RESP_BUF_SIZE, 0, 0, 0 },
	{ NULL, 1, 0, s_sharedSessionBuf, NP_COMM2_RESP_BUF_SIZE, 0, 0, 0 },
	{ NULL, 1, 0, s_voucherBuf, NP_COMM2_RESP_BUF_SIZE, 0, 0, 0 },
	{ NULL, 6, 0, s_productInfoBuf, NP_COMM2_RESP_BUF_SIZE, 0, 0, 0 },
	{ NULL, 5, 0, s_productListBuf, NP_COMM2_RESP_BUF_SIZE, 0, 0, 0 },
	{ NULL, 5, 0, s_categoryBuf, NP_COMM2_RESP_BUF_SIZE, 0, 0, 0 },
	{ NULL, 6, 0, s_browseBuf, NP_COMM2_RESP_BUF_SIZE, 0, 0, 0 },
	{ NULL, 1, 0, s_emptyStoreBuf, NP_COMM2_RESP_BUF_SIZE, 0, 0, 0 },
	{ NULL, 8, 0, s_productInfoExBuf, NP_COMM2_RESP_BUF_SIZE, 0, 0, 0 },
	{ NULL, 7, 0, s_productInfoAltBuf, NP_COMM2_RESP_BUF_SIZE, 0, 0, 0 },
	{ NULL, 1, 0, s_altSessionBuf, NP_COMM2_RESP_BUF_SIZE, 0, 0, 0 },
	{ NULL, 5, 0, s_entitlementBuf, NP_COMM2_RESP_BUF_SIZE, 0, 0, 0 },
};

static int s_comm2NextReqId = 0;				/* 0x81545368 */
static NpCommerce2SessionManager s_comm2Mgr;			/* 0x815A33D8 */
static NpCommerce2CacheSlot s_sessionSlots[NP_COMM2_MAX_SESSION_SLOTS];	/* 0x815AA008 */
static NpCommerce2ActiveReq s_activeReqs[NP_COMM2_MAX_ACTIVE_REQS];	/* 0x815AA208 */
static char s_lastCategoryErrUrl[NP_COMM2_URL_BUF_SIZE];	/* 0x815AA308 */
static IPMI::Session *s_lastCategoryErrSession = NULL;		/* 0x815AA408 */

/* Forward declarations */
static int npComm2QueueCreate(int maxVal);
static int npComm2QueueDestroy(void);
static int npComm2QueuePush(int requestId, const void *itemBuf, SceSize itemSize);
static int npComm2QueueRemoveById(int requestId);
static int npComm2QueuePop(int *outRequestId, void *outBuf, SceSize maxSize);
static int npComm2QueueWait(void);
static int npComm2InitTables(NpCommerce2SessionManager *mgr);
static int npComm2AllocReqId(NpCommerce2SessionManager *mgr);
static int npComm2FreeReqId(NpCommerce2SessionManager *mgr, int requestId);
static int npComm2AbortReq(NpCommerce2SessionManager *mgr, int requestId);
static int npComm2IsReqActive(NpCommerce2SessionManager *mgr, int requestId, int *outActive);
static int npComm2CleanupSession(NpCommerce2SessionManager *mgr, IPMI::Session *session);
static int npComm2WorkerThread(SceSize args, void *argp);
static int npComm2DispatchSync(IPMI::Session *session, SceUInt32 methodId,
			       const IPMI::DataInfo *inList, SceUInt32 inCount,
			       IPMI::BufferInfo *outList, SceUInt32 outCount);

/* 0x81221D44 */
static void npComm2NotifyCompletionEvents(const NpCommerce2QueueItem *item)
{
	IPMI::Session *session = item->session;

	switch (item->kind) {
	case NP_COMM2_KIND_SESSION:
		shellSvcNotifyEvent(session, 24);
		shellSvcNotifyEvent(session, 25);
		break;
	case NP_COMM2_KIND_SESSION_SHARED:
		shellSvcNotifyEvent(session, 26);
		shellSvcNotifyEvent(session, 27);
		break;
	case NP_COMM2_KIND_VOUCHER:
		shellSvcNotifyEvent(session, 30);
		shellSvcNotifyEvent(session, 31);
		break;
	case NP_COMM2_KIND_PRODUCT_INFO:
		shellSvcNotifyEvent(session, 32);
		shellSvcNotifyEvent(session, 33);
		break;
	case NP_COMM2_KIND_PRODUCT_LIST:
		shellSvcNotifyEvent(session, 38);
		shellSvcNotifyEvent(session, 39);
		break;
	case NP_COMM2_KIND_CATEGORY:
		shellSvcNotifyEvent(session, 40);
		shellSvcNotifyEvent(session, 41);
		break;
	case NP_COMM2_KIND_BROWSE:
		shellSvcNotifyEvent(session, 42);
		shellSvcNotifyEvent(session, 43);
		break;
	case NP_COMM2_KIND_EMPTY_STORE:
		shellSvcNotifyEvent(session, 46);
		shellSvcNotifyEvent(session, 47);
		break;
	case NP_COMM2_KIND_PRODUCT_INFO_EX:
		shellSvcNotifyEvent(session, 34);
		shellSvcNotifyEvent(session, 35);
		break;
	case NP_COMM2_KIND_PRODUCT_INFO_ALT:
		shellSvcNotifyEvent(session, 36);
		shellSvcNotifyEvent(session, 37);
		break;
	case NP_COMM2_KIND_SESSION_ALT:
		shellSvcNotifyEvent(session, 28);
		shellSvcNotifyEvent(session, 29);
		break;
	case NP_COMM2_KIND_ENTITLEMENT:
		shellSvcNotifyEvent(session, 44);
		shellSvcNotifyEvent(session, 45);
		break;
	default:
		break;
	}
}

/* 0x81221E2E */
int npCommerce2SessionManagerInit(SceSize stackSize, int priority)
{
	NpCommerce2SessionManager *mgrArg = &s_comm2Mgr;
	int ret;

	if (s_comm2Initialized != 0) {
		ret = NP_COMM2_ERR_ALREADY_INIT;
	} else {
		sce_paf_memset(&s_comm2Mgr, 0, sizeof(s_comm2Mgr));
		s_comm2Mgr.threadId = -1;
		s_comm2Mgr.mutexCreated = 0;

		ret = sceKernelCreateLwMutex(&s_comm2Mgr.mutex, "SceNpCommerce2SessionManager",
					     2u, 0, NULL);
		if (ret >= 0) {
			s_comm2Mgr.mutexCreated = 1;
			ret = npComm2InitTables(&s_comm2Mgr);
			if (ret >= 0) {
				SceUID tid = sceKernelCreateThread("SceNpCommerce2SessionManager",
								   npComm2WorkerThread, priority,
								   stackSize, 0, 0, NULL);
				ret = tid;
				if (tid >= 0) {
					s_comm2Mgr.threadId = tid;
					ret = sceKernelStartThread(tid, sizeof(mgrArg), &mgrArg);
					if (ret >= 0) {
						s_comm2Initialized = 1;
						return 0;
					}
				}
			}
		}
	}

	if (s_comm2Mgr.threadId >= 0) {
		sceKernelWaitThreadEnd(s_comm2Mgr.threadId, NULL, NULL);
		sceKernelDeleteThread(s_comm2Mgr.threadId);
		s_comm2Mgr.threadId = -1;
	}
	int lockRet = sceKernelLockLwMutex(&s_comm2Mgr.mutex, 1, NULL);
	npComm2QueueDestroy();
	if (lockRet >= 0)
		sceKernelUnlockLwMutex(&s_comm2Mgr.mutex, 1);
	if (s_comm2Mgr.mutexCreated == 1) {
		sceKernelDeleteLwMutex(&s_comm2Mgr.mutex);
		s_comm2Mgr.mutexCreated = 0;
	}
	return ret;
}

/* 0x81221F72 / 0x812236F8 */
static int npComm2IncEpoch(void)
{
	s_comm2Epoch++;
	return 0;
}

/* 0x81221F7A / 0x8122370A */
static int npComm2GetEpoch(int *outEpoch)
{
	*outEpoch = s_comm2Epoch;
	return 0;
}

/* 0x8122371A */
static int npComm2InvalidateSlot(NpCommerce2SessionManager *mgr, int kind)
{
	switch (kind) {
	case NP_COMM2_KIND_SESSION_SHARED:
	case NP_COMM2_KIND_VOUCHER:
	case NP_COMM2_KIND_PRODUCT_INFO:
	case NP_COMM2_KIND_PRODUCT_INFO_EX:
	case NP_COMM2_KIND_PRODUCT_INFO_ALT: {
		int ret = sceKernelLockLwMutex(&mgr->mutex, 1, NULL);
		if (ret >= 0) {
			s_staticSlots[kind].lastResult = NP_COMM2_ERR_NOT_FOUND;
			s_staticSlots[kind].generation++;
			sceKernelUnlockLwMutex(&mgr->mutex, 1);
		}
		return ret;
	}
	default:
		return NP_COMM2_ERR_INVALID_OP;
	}
}

/* 0x8122377A */
static int npComm2GetSlotGeneration(NpCommerce2SessionManager *mgr, int kind, int *outGen)
{
	switch (kind) {
	case NP_COMM2_KIND_SESSION_SHARED:
	case NP_COMM2_KIND_VOUCHER:
	case NP_COMM2_KIND_PRODUCT_INFO:
	case NP_COMM2_KIND_PRODUCT_INFO_EX:
	case NP_COMM2_KIND_PRODUCT_INFO_ALT: {
		int ret = sceKernelLockLwMutex(&mgr->mutex, 1, NULL);
		if (ret >= 0) {
			*outGen = s_staticSlots[kind].generation;
			sceKernelUnlockLwMutex(&mgr->mutex, 1);
		}
		return ret;
	}
	default:
		return NP_COMM2_ERR_INVALID_OP;
	}
}

/* Shared enqueuer for 0x81221FA4..0x81222A2C */
static int npComm2EnqueueRequest(IPMI::Session *session, int kind, const void *payload,
				 SceSize payloadSize)
{
	NpCommerce2QueueItem item;
	int reqId;
	int ret;

	sceClibMemset(&item, 0, sizeof(item));
	item.session = session;
	item.clientPid = session->getProcessId();
	item.kind = kind;
	item.netCtlPid = session->getProcessId();
	sceClibMemcpy_safe(item.payload.raw, payload, payloadSize);

	reqId = npComm2AllocReqId(&s_comm2Mgr);
	if (reqId < 0)
		return reqId;

	ret = sceKernelLockLwMutex(&s_comm2Mgr.mutex, 1, NULL);
	if (ret >= 0) {
		ret = npComm2QueuePush(reqId, &item, sizeof(item));
		sceKernelUnlockLwMutex(&s_comm2Mgr.mutex, 1);
	}
	if (ret >= 0)
		return reqId;

	npComm2FreeReqId(&s_comm2Mgr, reqId);
	return ret;
}

/* 0x812237D4 */
static int npComm2GetResultCommon(NpCommerce2SessionManager *mgr, IPMI::Session *session, int kind,
				  int *outResult, int *outGeneration, void *outRespBuf,
				  SceSize respBufSize, SceSize *outRespSize, char *outUrlBuf,
				  SceSize urlBufSize, SceSize *outUrlSize, int *outStamp)
{
	NpCommerce2CacheSlot *slot = NULL;
	const char *urlStr = NULL;
	int ret;

	*outRespSize = 0;
	*outUrlSize = 0;
	if (kind <= 0 || kind >= NP_COMM2_STATIC_TYPE_COUNT)
		return NP_COMM2_ERR_INVALID_OP;

	ret = sceKernelLockLwMutex(&mgr->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	if (kind == NP_COMM2_KIND_SESSION) {
		int innerRet = sceKernelLockLwMutex(&mgr->mutex, 1, NULL);
		if (innerRet >= 0) {
			SceUInt32 idx;
			for (idx = 0; idx < NP_COMM2_MAX_SESSION_SLOTS; idx++) {
				if (s_sessionSlots[idx].ownerSession == session)
					break;
			}
			if (idx == NP_COMM2_MAX_SESSION_SLOTS)
				innerRet = NP_COMM2_ERR_NOT_FOUND;
			else
				slot = &s_sessionSlots[idx];
			sceKernelUnlockLwMutex(&mgr->mutex, 1);
		}
		ret = innerRet;
	} else {
		slot = &s_staticSlots[kind];
	}

	if (ret >= 0) {
		*outResult = slot->lastResult;
		if (respBufSize != 0) {
			if (respBufSize < slot->respSize) {
				ret = NP_COMM2_ERR_BUF_TOO_SMALL;
			} else {
				*outRespSize = slot->respSize;
				sceClibMemcpy_safe(outRespBuf, slot->respBuf, slot->respSize);
			}
		} else {
			*outRespSize = 0;
		}
	}

	if (ret >= 0) {
		if (urlBufSize == 0) {
			*outUrlSize = 0;
		} else {
			ret = npCommerce2CoreGetContextUrl(slot->coreCtxId, &urlStr);
			if (ret >= 0) {
				if (kind == NP_COMM2_KIND_PRODUCT_INFO_EX &&
				    *urlStr == '\0' &&
				    s_lastCategoryErrSession == session) {
					urlStr = s_lastCategoryErrUrl;
				}
				SceSize urlLen = sceClibStrnlen(urlStr, NP_COMM2_URL_BUF_SIZE) + 1;
				*outUrlSize = urlLen;
				if (urlLen < NP_COMM2_URL_BUF_SIZE)
					sceClibStrncpy(outUrlBuf, urlStr, urlBufSize);
				else
					ret = NP_COMM2_ERR_BUF_TOO_SMALL;
			}
		}
	}

	if (ret >= 0 && outGeneration != NULL) {
		ret = npCommerce2CoreGetContextStamp(slot->coreCtxId, outStamp);
		if (ret >= 0)
			*outGeneration = slot->generation;
	}

	if (kind == NP_COMM2_KIND_SESSION && slot != NULL) {
		slot->ownerSession = NULL;
		slot->lastResult = NP_COMM2_ERR_NOT_FOUND;
	}
	sceKernelUnlockLwMutex(&mgr->mutex, 1);
	return ret;
}

/* 0x81222ADE */
static int npComm2GetEmptyStoreResult(IPMI::Session *session, int *outResult, void *outRespBuf,
				      SceSize respBufSize, SceSize *outRespSize)
{
	SceSize unusedUrlSize = 0;
	return npComm2GetResultCommon(&s_comm2Mgr, session, NP_COMM2_KIND_EMPTY_STORE,
				      outResult, NULL, outRespBuf, respBufSize, outRespSize,
				      NULL, 0, &unusedUrlSize, NULL);
}

/* 0x81222B4A */
static int npComm2ShowStoreIcon(IPMI::Session *session, int position)
{
	paf::Plugin *plugin = paf::Plugin::Find("topmenu_plugin");
	if (plugin == NULL)
		return NP_COMM2_ERR_NO_PLUGIN;

	ShellPluginInterface *iface = (ShellPluginInterface *)plugin->GetInterface(1);
	if (iface == NULL)
		return NP_COMM2_ERR_NO_PLUGIN;

	if (position < 0 || position > 2)
		return NP_COMM2_ERR_INVALID_ARG;

	typedef void (*ShowStoreIconFn)(SceUID pid, int layout);
	ShowStoreIconFn showFn = (ShowStoreIconFn)iface->func[TOPMENU_PLUGIN_FUNC_SHOW_STORE_ICON];
	showFn(session->getProcessId(), position);
	return 0;
}

/* 0x81222BC2 */
static int npComm2HideStoreIcon(IPMI::Session *session)
{
	paf::Plugin *plugin = paf::Plugin::Find("topmenu_plugin");
	if (plugin == NULL)
		return NP_COMM2_ERR_NO_PLUGIN;

	ShellPluginInterface *iface = (ShellPluginInterface *)plugin->GetInterface(1);
	if (iface == NULL)
		return NP_COMM2_ERR_NO_PLUGIN;

	typedef void (*HideStoreIconFn)(SceUID pid);
	HideStoreIconFn hideFn = (HideStoreIconFn)iface->func[TOPMENU_PLUGIN_FUNC_HIDE_STORE_ICON];
	hideFn(session->getProcessId());
	return 0;
}

/* 0x81222C00 */
int shellSvcDisp_0E_Sync(IPMI::Session *session, SceUInt32 methodId,
			 const IPMI::DataInfo *inList, SceUInt32 inCount,
			 IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	return npComm2DispatchSync(session, methodId, inList, inCount, outList, outCount);
}

/* 0x81222C14 */
int shellSvcDisp_0E_Async(IPMI::Session *session, SceUInt32 reqId, SceUInt32 methodId,
			  const IPMI::DataInfo *inList, SceUInt32 inCount)
{
	(void)session;
	(void)reqId;
	(void)methodId;
	(void)inList;
	(void)inCount;
	return ipmiAsyncNotSupported();
}

/* 0x81222C24 */
void shellSvcDisp_0E_Disconn(IPMI::Session *session)
{
	npComm2CleanupSession(&s_comm2Mgr, session);
}

/* Shared 7-output get-result responder (0x81222CA8..0x812230B4) */
static int npComm2RespondGetResult(IPMI::Session *session, int kind,
				   const IPMI::DataInfo *inList,
				   IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	const SceSize *sizes = (const SceSize *)inList[0].pData;
	SceSize *outRespSize = (SceSize *)outList[3].pBuffer;
	SceSize *outUrlSize = (SceSize *)outList[5].pBuffer;

	int res = npComm2GetResultCommon(&s_comm2Mgr, session, kind,
					 (int *)outList[0].pBuffer,
					 (int *)outList[1].pBuffer,
					 outList[2].pBuffer, sizes[0], outRespSize,
					 (char *)outList[4].pBuffer, sizes[1], outUrlSize,
					 (int *)outList[6].pBuffer);

	outList[0].outSize = sizeof(int);
	outList[1].outSize = sizeof(int);
	outList[2].outSize = *outRespSize;
	outList[3].outSize = sizeof(SceSize);
	outList[4].outSize = *outUrlSize;
	outList[5].outSize = sizeof(SceSize);
	outList[6].outSize = sizeof(int);
	return session->respondToSyncMethodRequest(res, outList, outCount);
}

/* 0x8122311A */
static int npComm2DispatchSync(IPMI::Session *session, SceUInt32 methodId,
			       const IPMI::DataInfo *inList, SceUInt32 inCount,
			       IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	(void)inCount;

	switch (methodId) {
	case NP_COMM2_IPC_INC_EPOCH: {
		int res = npComm2IncEpoch();
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_COMM2_IPC_GET_EPOCH: {
		int res = npComm2GetEpoch((int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_COMM2_IPC_INVALIDATE_TYPE: {
		int kind = *(const int *)inList[0].pData;
		int res = npComm2InvalidateSlot(&s_comm2Mgr, kind);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_COMM2_IPC_GET_TYPE_GEN: {
		int kind = *(const int *)inList[0].pData;
		int res = npComm2GetSlotGeneration(&s_comm2Mgr, kind, (int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_COMM2_IPC_START_SESSION: {
		int res = npComm2EnqueueRequest(session, NP_COMM2_KIND_SESSION, inList[0].pData, 4u);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_COMM2_IPC_START_SESSION_SHARED: {
		int res = npComm2EnqueueRequest(session, NP_COMM2_KIND_SESSION_SHARED, inList[0].pData, 4u);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_COMM2_IPC_START_SESSION_ALT: {
		int res = npComm2EnqueueRequest(session, NP_COMM2_KIND_SESSION_ALT, inList[0].pData, 4u);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_COMM2_IPC_GET_SESSION:
		return npComm2RespondGetResult(session, NP_COMM2_KIND_SESSION, inList, outList, outCount);
	case NP_COMM2_IPC_GET_SESSION_SHARED:
		return npComm2RespondGetResult(session, NP_COMM2_KIND_SESSION_SHARED, inList, outList, outCount);
	case NP_COMM2_IPC_GET_SESSION_ALT:
		return npComm2RespondGetResult(session, NP_COMM2_KIND_SESSION_ALT, inList, outList, outCount);
	case NP_COMM2_IPC_START_VOUCHER: {
		int res = npComm2EnqueueRequest(session, NP_COMM2_KIND_VOUCHER, inList[0].pData, 0x18u);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_COMM2_IPC_GET_VOUCHER:
		return npComm2RespondGetResult(session, NP_COMM2_KIND_VOUCHER, inList, outList, outCount);
	case NP_COMM2_IPC_START_PRODUCT_INFO: {
		int res = npComm2EnqueueRequest(session, NP_COMM2_KIND_PRODUCT_INFO, inList[0].pData, 0x214u);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_COMM2_IPC_GET_PRODUCT_INFO:
		return npComm2RespondGetResult(session, NP_COMM2_KIND_PRODUCT_INFO, inList, outList, outCount);
	case NP_COMM2_IPC_START_PRODUCT_INFO_EX: {
		int res = npComm2EnqueueRequest(session, NP_COMM2_KIND_PRODUCT_INFO_EX, inList[0].pData, 0x214u);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_COMM2_IPC_GET_PRODUCT_INFO_EX:
		return npComm2RespondGetResult(session, NP_COMM2_KIND_PRODUCT_INFO_EX, inList, outList, outCount);
	case NP_COMM2_IPC_START_PRODUCT_INFO_ALT: {
		int res = npComm2EnqueueRequest(session, NP_COMM2_KIND_PRODUCT_INFO_ALT, inList[0].pData, 0x214u);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_COMM2_IPC_GET_PRODUCT_INFO_ALT:
		return npComm2RespondGetResult(session, NP_COMM2_KIND_PRODUCT_INFO_ALT, inList, outList, outCount);
	case NP_COMM2_IPC_START_PRODUCT_LIST: {
		int res = npComm2EnqueueRequest(session, NP_COMM2_KIND_PRODUCT_LIST, inList[0].pData, 0x1Cu);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_COMM2_IPC_GET_PRODUCT_LIST:
		return npComm2RespondGetResult(session, NP_COMM2_KIND_PRODUCT_LIST, inList, outList, outCount);
	case NP_COMM2_IPC_START_CATEGORY: {
		int res = npComm2EnqueueRequest(session, NP_COMM2_KIND_CATEGORY, inList[0].pData, 0x20Cu);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_COMM2_IPC_GET_CATEGORY:
		return npComm2RespondGetResult(session, NP_COMM2_KIND_CATEGORY, inList, outList, outCount);
	case NP_COMM2_IPC_START_BROWSE: {
		int res = npComm2EnqueueRequest(session, NP_COMM2_KIND_BROWSE, inList[0].pData, 0x114u);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_COMM2_IPC_GET_BROWSE:
		return npComm2RespondGetResult(session, NP_COMM2_KIND_BROWSE, inList, outList, outCount);
	case NP_COMM2_IPC_START_ENTITLEMENT: {
		int res = npComm2EnqueueRequest(session, NP_COMM2_KIND_ENTITLEMENT, inList[0].pData, 0x114u);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_COMM2_IPC_GET_ENTITLEMENT:
		return npComm2RespondGetResult(session, NP_COMM2_KIND_ENTITLEMENT, inList, outList, outCount);
	case NP_COMM2_IPC_START_EMPTY_STORE: {
		int res = npComm2EnqueueRequest(session, NP_COMM2_KIND_EMPTY_STORE, inList[0].pData, 8u);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_COMM2_IPC_GET_EMPTY_STORE: {
		SceSize *outRespSize = (SceSize *)outList[2].pBuffer;
		SceSize maxRespSize = *(const SceSize *)inList[0].pData;
		int res = npComm2GetEmptyStoreResult(session, (int *)outList[0].pBuffer,
						     outList[1].pBuffer, maxRespSize, outRespSize);
		outList[0].outSize = sizeof(int);
		outList[1].outSize = *outRespSize;
		outList[2].outSize = sizeof(SceSize);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_COMM2_IPC_ABORT_REQUEST: {
		int reqId = *(const int *)inList[0].pData;
		int res = npComm2AbortReq(&s_comm2Mgr, reqId);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_COMM2_IPC_IS_REQUEST_ACTIVE: {
		int reqId = *(const int *)inList[0].pData;
		int res = npComm2IsReqActive(&s_comm2Mgr, reqId, (int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_COMM2_IPC_SHOW_STORE_ICON: {
		int pos = *(const int *)inList[0].pData;
		int res = npComm2ShowStoreIcon(session, pos);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_COMM2_IPC_HIDE_STORE_ICON: {
		int res = npComm2HideStoreIcon(session);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	default:
		return IPMI_ERR_UNSUPPORTED_METHOD;
	}
}

/* 0x81223438 */
static int npComm2QueueCreate(int maxVal)
{
	SceUID semaId = sceKernelCreateSema("SceNpCommerce2SM", 0, 0, maxVal, NULL);
	if (semaId >= 0) {
		s_comm2QueueSema = semaId;
		s_comm2QueueLimit = (SceUInt32)maxVal;
	}
	return semaId;
}

/* 0x81223470 */
static int npComm2QueueDestroy(void)
{
	if (s_comm2QueueSema >= 0) {
		sceKernelDeleteSema(s_comm2QueueSema);
		s_comm2QueueSema = -1;
	}
	while (s_comm2QueueHead != NULL) {
		NpCommerce2QueueNode *node = s_comm2QueueHead;
		s_comm2QueueHead = node->next;
		npCommerce2Free(node->itemBuf);
		npCommerce2Free(node);
	}
	return 0;
}

/* 0x812234B8 */
static int npComm2QueuePush(int requestId, const void *itemBuf, SceSize itemSize)
{
	NpCommerce2QueueNode *curr = s_comm2QueueHead;
	NpCommerce2QueueNode *tail = NULL;
	SceUInt32 count = 0;

	while (curr != NULL) {
		tail = curr;
		curr = curr->next;
		count++;
	}
	if (count >= s_comm2QueueLimit)
		return NP_COMM2_ERR_QUEUE_FULL;

	void *copyBuf = npCommerce2Alloc(itemSize);
	if (copyBuf == NULL)
		return NP_COMM2_ERR_NO_MEMORY;

	NpCommerce2QueueNode *node = (NpCommerce2QueueNode *)npCommerce2Alloc(sizeof(NpCommerce2QueueNode));
	if (node == NULL) {
		npCommerce2Free(copyBuf);
		return NP_COMM2_ERR_NO_MEMORY;
	}

	sceClibMemset(node, 0, sizeof(NpCommerce2QueueNode));
	node->requestId = requestId;
	node->itemSize = itemSize;
	sceClibMemcpy_safe(copyBuf, itemBuf, itemSize);
	node->itemBuf = copyBuf;

	if (tail != NULL)
		tail->next = node;
	else
		s_comm2QueueHead = node;

	sceKernelSignalSema(s_comm2QueueSema, 1);
	return 0;
}

/* 0x8122356E */
static int npComm2QueueRemoveById(int requestId)
{
	NpCommerce2QueueNode *curr = s_comm2QueueHead;
	NpCommerce2QueueNode *prev = NULL;

	while (curr != NULL && curr->requestId != requestId) {
		prev = curr;
		curr = curr->next;
	}
	if (curr == NULL)
		return NP_COMM2_ERR_QUEUE_EMPTY;

	if (prev != NULL)
		prev->next = curr->next;
	else
		s_comm2QueueHead = curr->next;

	npCommerce2Free(curr->itemBuf);
	npCommerce2Free(curr);
	sceKernelPollSema(s_comm2QueueSema, 1);
	return 0;
}

/* 0x812235C4 */
static int npComm2QueuePop(int *outRequestId, void *outBuf, SceSize maxSize)
{
	if (outBuf == NULL)
		return NP_COMM2_ERR_INVALID_ARG;
	if (s_comm2QueueHead == NULL)
		return NP_COMM2_ERR_QUEUE_EMPTY;

	NpCommerce2QueueNode *node = s_comm2QueueHead;
	if (outRequestId != NULL)
		*outRequestId = node->requestId;

	SceSize copyLen = (maxSize >= node->itemSize) ? node->itemSize : maxSize;
	sceClibMemcpy_safe(outBuf, node->itemBuf, copyLen);

	s_comm2QueueHead = node->next;
	npCommerce2Free(node->itemBuf);
	npCommerce2Free(node);
	sceKernelPollSema(s_comm2QueueSema, 1);
	return (int)copyLen;
}

/* 0x81223634 */
static int npComm2QueueWait(void)
{
	sceKernelWaitSema(s_comm2QueueSema, 1, NULL);
	return 0;
}

/* 0x8122364C */
static int npComm2InitTables(NpCommerce2SessionManager *mgr)
{
	int ret;
	int i;

	(void)mgr;
	ret = npComm2QueueCreate(NP_COMM2_QUEUE_MAX);
	if (ret < 0)
		return ret;

	for (i = 0; i < NP_COMM2_STATIC_TYPE_COUNT; i++)
		s_staticSlots[i].lastResult = NP_COMM2_ERR_NOT_FOUND;

	sceClibMemset(s_sessionSlots, 0, sizeof(s_sessionSlots));
	for (i = 0; i < NP_COMM2_MAX_SESSION_SLOTS; i++) {
		s_sessionSlots[i].ctxType = 3;
		s_sessionSlots[i].respBuf = s_sessionRespBufs[i];
		s_sessionSlots[i].respCapacity = NP_COMM2_RESP_BUF_SIZE;
		s_sessionSlots[i].lastResult = NP_COMM2_ERR_NOT_FOUND;
	}

	sceClibMemset(s_activeReqs, 0, sizeof(s_activeReqs));
	s_comm2NextReqId = 0;
	s_lastCategoryErrUrl[0] = '\0';
	s_lastCategoryErrSession = NULL;
	return 0;
}

/* 0x812239CA */
static int npComm2AllocReqId(NpCommerce2SessionManager *mgr)
{
	int ret = sceKernelLockLwMutex(&mgr->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	SceUInt32 freeIdx;
	for (freeIdx = 0; freeIdx < NP_COMM2_MAX_ACTIVE_REQS; freeIdx++) {
		if (s_activeReqs[freeIdx].inUse == 0)
			break;
	}

	int assignedId = NP_COMM2_ERR_TABLE_FULL;
	if (freeIdx < NP_COMM2_MAX_ACTIVE_REQS) {
		for (SceUInt32 tries = 0; tries < NP_COMM2_MAX_ACTIVE_REQS; tries++) {
			int nextVal = s_comm2NextReqId;
			if ((SceUInt32)nextVal > 0x7FFFFFFEu)
				nextVal = 0;
			int candidate = nextVal + 1;
			s_comm2NextReqId = candidate;

			SceUInt32 scan;
			for (scan = 0; scan < NP_COMM2_MAX_ACTIVE_REQS; scan++) {
				if (s_activeReqs[scan].inUse == 1 &&
				    s_activeReqs[scan].requestId == candidate) {
					break;
				}
			}
			if (scan == NP_COMM2_MAX_ACTIVE_REQS) {
				if (candidate >= 0) {
					sceClibMemset(&s_activeReqs[freeIdx], 0,
						      sizeof(NpCommerce2ActiveReq));
					s_activeReqs[freeIdx].inUse = 1;
					s_activeReqs[freeIdx].requestId = candidate;
					assignedId = candidate;
				}
				break;
			}
		}
	}

	sceKernelUnlockLwMutex(&mgr->mutex, 1);
	return assignedId;
}

/* 0x81223A9E */
static int npComm2FreeReqId(NpCommerce2SessionManager *mgr, int requestId)
{
	int ret = sceKernelLockLwMutex(&mgr->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	SceUInt32 idx;
	for (idx = 0; idx < NP_COMM2_MAX_ACTIVE_REQS; idx++) {
		if (s_activeReqs[idx].requestId == requestId)
			break;
	}
	if (idx == NP_COMM2_MAX_ACTIVE_REQS)
		ret = NP_COMM2_ERR_NOT_FOUND;
	else
		sceClibMemset(&s_activeReqs[idx], 0, sizeof(NpCommerce2ActiveReq));

	sceKernelUnlockLwMutex(&mgr->mutex, 1);
	return ret;
}

/* 0x81223B02 */
static int npComm2AbortReq(NpCommerce2SessionManager *mgr, int requestId)
{
	int ret = sceKernelLockLwMutex(&mgr->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	if (npComm2QueueRemoveById(requestId) < 0) {
		SceUInt32 idx;
		for (idx = 0; idx < NP_COMM2_MAX_ACTIVE_REQS; idx++) {
			if (s_activeReqs[idx].requestId == requestId)
				break;
		}
		if (idx == NP_COMM2_MAX_ACTIVE_REQS) {
			sceKernelUnlockLwMutex(&mgr->mutex, 1);
			return NP_COMM2_ERR_NOT_FOUND;
		}
		if (s_activeReqs[idx].coreReqId != 0)
			npCommerce2CoreAbortRequest(s_activeReqs[idx].coreReqId);
		s_activeReqs[idx].aborted = 1;
	} else {
		npComm2FreeReqId(mgr, requestId);
	}

	sceKernelUnlockLwMutex(&mgr->mutex, 1);
	return 0;
}

/* 0x81223BB0 */
static int npComm2IsReqActive(NpCommerce2SessionManager *mgr, int requestId, int *outActive)
{
	int ret = sceKernelLockLwMutex(&mgr->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	*outActive = 0;
	for (SceUInt32 idx = 0; idx < NP_COMM2_MAX_ACTIVE_REQS; idx++) {
		if (s_activeReqs[idx].requestId == requestId) {
			*outActive = 1;
			break;
		}
	}

	sceKernelUnlockLwMutex(&mgr->mutex, 1);
	return ret;
}

/* 0x81223C06 */
static int npComm2CleanupSession(NpCommerce2SessionManager *mgr, IPMI::Session *session)
{
	int ret = sceKernelLockLwMutex(&mgr->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	for (SceUInt32 idx = 0; idx < NP_COMM2_MAX_SESSION_SLOTS; idx++) {
		if (s_sessionSlots[idx].ownerSession == session) {
			s_sessionSlots[idx].ownerSession = NULL;
			s_sessionSlots[idx].lastResult = NP_COMM2_ERR_NOT_FOUND;
		}
	}
	if (s_lastCategoryErrSession == session) {
		s_lastCategoryErrSession = NULL;
		s_lastCategoryErrUrl[0] = '\0';
	}

	return sceKernelUnlockLwMutex(&mgr->mutex, 1);
}

/* Helper: bind active core request handle if request was not aborted */
static int npComm2BindCoreReq(NpCommerce2SessionManager *mgr, int requestId, int coreReqId)
{
	int ret = sceKernelLockLwMutex(&mgr->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	SceUInt32 idx;
	for (idx = 0; idx < NP_COMM2_MAX_ACTIVE_REQS; idx++) {
		if (s_activeReqs[idx].requestId == requestId)
			break;
	}
	if (idx == NP_COMM2_MAX_ACTIVE_REQS) {
		ret = NP_COMM2_ERR_NOT_FOUND;
	} else if (s_activeReqs[idx].aborted == 1) {
		ret = NP_COMM2_ERR_ABORTED;
	} else {
		s_activeReqs[idx].coreReqId = coreReqId;
		ret = 0;
	}

	sceKernelUnlockLwMutex(&mgr->mutex, 1);
	return ret;
}

static void npComm2UnbindCoreReq(NpCommerce2SessionManager *mgr, int requestId)
{
	if (sceKernelLockLwMutex(&mgr->mutex, 1, NULL) >= 0) {
		for (SceUInt32 idx = 0; idx < NP_COMM2_MAX_ACTIVE_REQS; idx++) {
			if (s_activeReqs[idx].requestId == requestId) {
				s_activeReqs[idx].coreReqId = 0;
				break;
			}
		}
		sceKernelUnlockLwMutex(&mgr->mutex, 1);
	}
}

/* Helper: check SDK 2.00+ sign-in precondition for session / voucher creation */
static int npComm2CheckSignInRequirement(SceUID clientPid)
{
	int isSignedIn = 0;
	if (npCommonServiceIsSignedIn(&isSignedIn) < 0 || isSignedIn == 0)
		return 0;

	SceUInt32 sdkVersion = 0;
	int ret = vshKernelGetCompiledSdkVersionByPid(clientPid, &sdkVersion);
	if (ret < 0)
		return ret;

	if (sdkVersion < NP_COMM2_SDK_SIGNIN_THRESHOLD)
		return 0;

	ret = npAccountIsSignedIn();
	if (ret < 0)
		return ret;
	return (ret != 0) ? 0 : NP_COMM2_ERR_SIGNED_OUT;
}

/* 0x81223C70 */
static int npComm2ExecSessionRequest(NpCommerce2SessionManager *mgr, int requestId,
				     const NpCommerce2QueueItem *item, int coreCtxId,
				     void *respBuf, SceSize respCap, SceSize *outRespSize)
{
	int ret = npComm2CheckSignInRequirement(item->clientPid);
	if (ret >= 0) {
		ret = npCommerce2CoreCreateSessionReq(coreCtxId, &s_comm2CurrentCoreReq);
		if (ret < 0)
			return ret;

		ret = npComm2BindCoreReq(mgr, requestId, s_comm2CurrentCoreReq);
		if (ret >= 0) {
			ret = npCommerce2CoreStartSessionReq(s_comm2CurrentCoreReq);
			if (ret >= 0)
				ret = npCommerce2CoreGetResponse(s_comm2CurrentCoreReq, respBuf,
								 respCap, outRespSize);
		}
	}

	npComm2UnbindCoreReq(mgr, requestId);
	npCommerce2CoreDestroyRequest(s_comm2CurrentCoreReq);
	s_comm2CurrentCoreReq = 0;
	return ret;
}

/* 0x81223E02 */
static int npComm2StoreSlotResult(NpCommerce2SessionManager *mgr, int kind, int sessionIdx,
				  int reqResult)
{
	switch (kind) {
	case NP_COMM2_KIND_SESSION:
	case NP_COMM2_KIND_SESSION_SHARED:
	case NP_COMM2_KIND_VOUCHER:
	case NP_COMM2_KIND_PRODUCT_INFO:
	case NP_COMM2_KIND_PRODUCT_LIST:
	case NP_COMM2_KIND_CATEGORY:
	case NP_COMM2_KIND_BROWSE:
	case NP_COMM2_KIND_PRODUCT_INFO_EX:
	case NP_COMM2_KIND_PRODUCT_INFO_ALT:
	case NP_COMM2_KIND_SESSION_ALT:
	case NP_COMM2_KIND_ENTITLEMENT: {
		int ret = sceKernelLockLwMutex(&mgr->mutex, 1, NULL);
		if (ret >= 0) {
			NpCommerce2CacheSlot *slot = (kind == NP_COMM2_KIND_SESSION)
				? &s_sessionSlots[sessionIdx]
				: &s_staticSlots[kind];
			slot->lastResult = reqResult;
			slot->generation++;
			sceKernelUnlockLwMutex(&mgr->mutex, 1);
		}
		return ret;
	}
	default:
		return NP_COMM2_ERR_INVALID_OP;
	}
}

/* 0x81223E82 */
static int npComm2WorkerThread(SceSize args, void *argp)
{
	NpCommerce2SessionManager *mgr = *(NpCommerce2SessionManager **)argp;
	bool initOk = true;
	int i;

	(void)args;

	for (i = 1; i < NP_COMM2_STATIC_TYPE_COUNT; i++) {
		if (i == NP_COMM2_KIND_SESSION)
			continue;
		int ret = npCommerce2CoreCreateContext(2u, &s_staticSlots[i].coreCtxId);
		if (ret >= 0 && s_staticSlots[i].ctxType != 9u)
			ret = npCommerce2CoreSetContextType(s_staticSlots[i].coreCtxId,
							    s_staticSlots[i].ctxType);
		if (ret < 0) {
			initOk = false;
			break;
		}
	}

	if (initOk) {
		for (i = 0; i < NP_COMM2_MAX_SESSION_SLOTS; i++) {
			int ret = npCommerce2CoreCreateContext(2u, &s_sessionSlots[i].coreCtxId);
			if (ret >= 0 && s_sessionSlots[i].ctxType != 9u)
				ret = npCommerce2CoreSetContextType(s_sessionSlots[i].coreCtxId,
								    s_sessionSlots[i].ctxType);
			if (ret < 0) {
				initOk = false;
				break;
			}
		}
	}

	while (initOk && sceKernelLockLwMutex(&mgr->mutex, 1, NULL) >= 0) {
		int requestId = 0;
		NpCommerce2QueueItem item;
		int popRet = npComm2QueuePop(&requestId, &item, sizeof(item));
		sceKernelUnlockLwMutex(&mgr->mutex, 1);

		if (popRet < 0) {
			npComm2QueueWait();
			continue;
		}

		unsigned int netCtlToken = 0;
		if (item.netCtlPid >= 0)
			netCtlToken = (unsigned int)SceShell_6B8772A2(item.netCtlPid);

		if (item.kind == NP_COMM2_KIND_SHUTDOWN)
			break;

		bool shouldNotify = true;

		switch (item.kind) {
		case NP_COMM2_KIND_SESSION: {
			int slotIdx = -1;
			int lockRet = sceKernelLockLwMutex(&mgr->mutex, 1, NULL);
			if (lockRet >= 0) {
				int idx;
				for (idx = 0; idx < NP_COMM2_MAX_SESSION_SLOTS; idx++) {
					if (s_sessionSlots[idx].ownerSession == item.session)
						break;
				}
				if (idx == NP_COMM2_MAX_SESSION_SLOTS) {
					for (idx = 0; idx < NP_COMM2_MAX_SESSION_SLOTS; idx++) {
						if (s_sessionSlots[idx].ownerSession == NULL)
							break;
					}
				}
				if (idx == NP_COMM2_MAX_SESSION_SLOTS) {
					slotIdx = NP_COMM2_ERR_TABLE_FULL;
				} else {
					s_sessionSlots[idx].ownerSession = item.session;
					s_sessionSlots[idx].lastResult = NP_COMM2_ERR_NOT_FOUND;
					slotIdx = idx;
				}
				sceKernelUnlockLwMutex(&mgr->mutex, 1);
			} else {
				slotIdx = lockRet;
			}

			if (slotIdx >= 0) {
				NpCommerce2CacheSlot *slot = &s_sessionSlots[slotIdx];
				int res = npComm2ExecSessionRequest(mgr, requestId, &item,
								    slot->coreCtxId, slot->respBuf,
								    slot->respCapacity,
								    &slot->respSize);
				npComm2StoreSlotResult(mgr, NP_COMM2_KIND_SESSION, slotIdx, res);
			}
			npComm2FreeReqId(mgr, requestId);
			break;
		}

		case NP_COMM2_KIND_SESSION_SHARED:
		case NP_COMM2_KIND_SESSION_ALT: {
			int kind = item.kind;
			NpCommerce2CacheSlot *slot = &s_staticSlots[kind];
			npComm2InvalidateSlot(mgr, kind);
			int res = npComm2ExecSessionRequest(mgr, requestId, &item, slot->coreCtxId,
							    slot->respBuf, slot->respCapacity,
							    &slot->respSize);
			npComm2StoreSlotResult(mgr, kind, 0, res);
			npComm2FreeReqId(mgr, requestId);
			break;
		}

		case NP_COMM2_KIND_VOUCHER: {
			NpCommerce2CacheSlot *slot = &s_staticSlots[NP_COMM2_KIND_VOUCHER];
			npComm2InvalidateSlot(mgr, NP_COMM2_KIND_VOUCHER);
			int res = npComm2CheckSignInRequirement(item.clientPid);
			if (res >= 0) {
				res = npCommerce2CoreCreateVoucherReq(slot->coreCtxId,
								      &s_comm2CurrentCoreReq);
				if (res >= 0) {
					res = npComm2BindCoreReq(mgr, requestId,
								 s_comm2CurrentCoreReq);
					if (res >= 0) {
						res = npCommerce2CoreStartVoucherReq(
							s_comm2CurrentCoreReq,
							item.payload.codes.countryCode,
							item.payload.codes.langCode,
							item.payload.codes.startPos,
							item.payload.codes.maxCount,
							item.payload.codes.sortMode);
						if (res >= 0) {
							res = npCommerce2CoreGetResponse(
								s_comm2CurrentCoreReq,
								slot->respBuf, slot->respCapacity,
								&slot->respSize);
						}
					}
					npComm2UnbindCoreReq(mgr, requestId);
					npCommerce2CoreDestroyRequest(s_comm2CurrentCoreReq);
					s_comm2CurrentCoreReq = 0;
				}
			} else {
				npComm2UnbindCoreReq(mgr, requestId);
				npCommerce2CoreDestroyRequest(s_comm2CurrentCoreReq);
				s_comm2CurrentCoreReq = 0;
			}
			npComm2StoreSlotResult(mgr, NP_COMM2_KIND_VOUCHER, 0, res);
			npComm2FreeReqId(mgr, requestId);
			break;
		}

		case NP_COMM2_KIND_PRODUCT_INFO:
		case NP_COMM2_KIND_PRODUCT_INFO_EX:
		case NP_COMM2_KIND_PRODUCT_INFO_ALT: {
			int kind = item.kind;
			NpCommerce2CacheSlot *slot = &s_staticSlots[kind];
			npComm2InvalidateSlot(mgr, kind);
			int res = npCommerce2CoreCreateProductInfoReq(slot->coreCtxId,
								      &s_comm2CurrentCoreReq);
			if (res >= 0) {
				res = npComm2BindCoreReq(mgr, requestId, s_comm2CurrentCoreReq);
				if (res >= 0) {
					bool boundUrl = false;
					if (kind == NP_COMM2_KIND_PRODUCT_INFO_EX &&
					    s_lastCategoryErrSession == item.session) {
						boundUrl = true;
						npCommerce2CoreBindThreadUrl(sceKernelGetThreadId(),
									     s_lastCategoryErrUrl);
					}
					res = npCommerce2CoreStartProductInfoReq(
						s_comm2CurrentCoreReq,
						item.payload.pair.mode,
						item.payload.pair.productId,
						item.payload.pair.categoryId,
						item.payload.pair.flags);
					if (res >= 0) {
						res = npCommerce2CoreGetResponse(
							s_comm2CurrentCoreReq,
							slot->respBuf, slot->respCapacity,
							&slot->respSize);
					}
					if (boundUrl)
						npCommerce2CoreUnbindThreadUrl();
				}
			}
			npComm2UnbindCoreReq(mgr, requestId);
			npCommerce2CoreDestroyRequest(s_comm2CurrentCoreReq);
			s_comm2CurrentCoreReq = 0;
			npComm2StoreSlotResult(mgr, kind, 0, res);
			npComm2FreeReqId(mgr, requestId);
			break;
		}

		case NP_COMM2_KIND_PRODUCT_LIST: {
			NpCommerce2CacheSlot *slot = &s_staticSlots[NP_COMM2_KIND_PRODUCT_LIST];
			npComm2InvalidateSlot(mgr, NP_COMM2_KIND_PRODUCT_LIST);
			int res = npCommerce2CoreCreateProductListReq(slot->coreCtxId,
								      &s_comm2CurrentCoreReq);
			if (res >= 0) {
				res = npComm2BindCoreReq(mgr, requestId, s_comm2CurrentCoreReq);
				if (res >= 0) {
					res = npCommerce2CoreStartProductListReq(
						s_comm2CurrentCoreReq,
						item.payload.codes.countryCode,
						item.payload.codes.langCode,
						item.payload.codes.startPos,
						item.payload.codes.maxCount,
						item.payload.codes.sortMode,
						item.payload.codes.filter);
					if (res >= 0) {
						res = npCommerce2CoreGetResponse(
							s_comm2CurrentCoreReq,
							slot->respBuf, slot->respCapacity,
							&slot->respSize);
					}
				}
				npComm2UnbindCoreReq(mgr, requestId);
				npCommerce2CoreDestroyRequest(s_comm2CurrentCoreReq);
				s_comm2CurrentCoreReq = 0;
			}
			npComm2StoreSlotResult(mgr, NP_COMM2_KIND_PRODUCT_LIST, 0, res);
			npComm2FreeReqId(mgr, requestId);
			break;
		}

		case NP_COMM2_KIND_CATEGORY: {
			NpCommerce2CacheSlot *slot = &s_staticSlots[NP_COMM2_KIND_CATEGORY];
			npComm2InvalidateSlot(mgr, NP_COMM2_KIND_CATEGORY);
			int res = npCommerce2CoreCreateCategoryReq(slot->coreCtxId,
								   &s_comm2CurrentCoreReq);
			if (res >= 0) {
				res = npComm2BindCoreReq(mgr, requestId, s_comm2CurrentCoreReq);
				if (res >= 0) {
					res = npCommerce2CoreStartCategoryReq(
						s_comm2CurrentCoreReq,
						item.payload.names.categoryId,
						item.payload.names.subCategory);
					if (res >= 0) {
						res = npCommerce2CoreGetResponse(
							s_comm2CurrentCoreReq,
							slot->respBuf, slot->respCapacity,
							&slot->respSize);
						if (res >= 0) {
							s_lastCategoryErrUrl[0] = '\0';
							s_lastCategoryErrSession = NULL;
						} else {
							const char *errUrl = NULL;
							if (npCommerce2CoreGetContextUrl(slot->coreCtxId,
											 &errUrl) >= 0) {
								s_lastCategoryErrSession = item.session;
								sceClibStrlcpy(s_lastCategoryErrUrl,
									       errUrl,
									       sizeof(s_lastCategoryErrUrl));
							}
						}
					}
				}
				npComm2UnbindCoreReq(mgr, requestId);
				npCommerce2CoreDestroyRequest(s_comm2CurrentCoreReq);
				s_comm2CurrentCoreReq = 0;
			}
			npComm2StoreSlotResult(mgr, NP_COMM2_KIND_CATEGORY, 0, res);
			npComm2FreeReqId(mgr, requestId);
			break;
		}

		case NP_COMM2_KIND_BROWSE: {
			NpCommerce2CacheSlot *slot = &s_staticSlots[NP_COMM2_KIND_BROWSE];
			npComm2InvalidateSlot(mgr, NP_COMM2_KIND_BROWSE);
			int res = npCommerce2CoreCreateBrowseReq(slot->coreCtxId,
								 &s_comm2CurrentCoreReq);
			if (res >= 0) {
				res = npComm2BindCoreReq(mgr, requestId, s_comm2CurrentCoreReq);
				if (res >= 0) {
					res = npCommerce2CoreStartBrowseReq(
						s_comm2CurrentCoreReq,
						item.payload.browse.targetId,
						item.payload.browse.startPos,
						item.payload.browse.maxCount,
						item.payload.browse.flags);
					if (res >= 0) {
						res = npCommerce2CoreGetResponse(
							s_comm2CurrentCoreReq,
							slot->respBuf, slot->respCapacity,
							&slot->respSize);
					}
				}
				npComm2UnbindCoreReq(mgr, requestId);
				npCommerce2CoreDestroyRequest(s_comm2CurrentCoreReq);
				s_comm2CurrentCoreReq = 0;
			}
			npComm2StoreSlotResult(mgr, NP_COMM2_KIND_BROWSE, 0, res);
			npComm2FreeReqId(mgr, requestId);
			break;
		}

		case NP_COMM2_KIND_EMPTY_STORE: {
			int targetKind = item.payload.emptyStore.targetKind;
			npComm2InvalidateSlot(mgr, targetKind);
			if (targetKind == 0)
				targetKind = NP_COMM2_KIND_EMPTY_STORE;
			NpCommerce2CacheSlot *dstSlot = &s_staticSlots[NP_COMM2_KIND_EMPTY_STORE];
			int res = npCommerce2CoreCreateEmptyStoreReq(s_staticSlots[targetKind].coreCtxId,
								     &s_comm2CurrentCoreReq);
			if (res >= 0) {
				res = npComm2BindCoreReq(mgr, requestId, s_comm2CurrentCoreReq);
				if (res >= 0) {
					res = npCommerce2CoreStartEmptyStoreReq(s_comm2CurrentCoreReq);
					if (res >= 0) {
						res = npCommerce2CoreGetResponse(
							s_comm2CurrentCoreReq,
							dstSlot->respBuf, dstSlot->respCapacity,
							&dstSlot->respSize);
					}
				}
			}
			npComm2UnbindCoreReq(mgr, requestId);
			npCommerce2CoreDestroyRequest(s_comm2CurrentCoreReq);
			s_comm2CurrentCoreReq = 0;
			dstSlot->lastResult = res;
			npComm2FreeReqId(mgr, requestId);
			break;
		}

		case NP_COMM2_KIND_ENTITLEMENT: {
			NpCommerce2CacheSlot *slot = &s_staticSlots[NP_COMM2_KIND_ENTITLEMENT];
			npComm2InvalidateSlot(mgr, NP_COMM2_KIND_ENTITLEMENT);
			int res = npCommerce2CoreCreateEntitlementReq(slot->coreCtxId,
								      &s_comm2CurrentCoreReq);
			if (res >= 0) {
				res = npComm2BindCoreReq(mgr, requestId, s_comm2CurrentCoreReq);
				if (res >= 0) {
					res = npCommerce2CoreStartEntitlementReq(
						s_comm2CurrentCoreReq,
						item.payload.one.entitlementId);
					if (res >= 0) {
						res = npCommerce2CoreGetResponse(
							s_comm2CurrentCoreReq,
							slot->respBuf, slot->respCapacity,
							&slot->respSize);
					}
				}
				npComm2UnbindCoreReq(mgr, requestId);
				npCommerce2CoreDestroyRequest(s_comm2CurrentCoreReq);
				s_comm2CurrentCoreReq = 0;
			}
			npComm2StoreSlotResult(mgr, NP_COMM2_KIND_ENTITLEMENT, 0, res);
			npComm2FreeReqId(mgr, requestId);
			break;
		}

		default:
			npComm2FreeReqId(mgr, requestId);
			shouldNotify = false;
			break;
		}

		if (shouldNotify)
			npComm2NotifyCompletionEvents(&item);
		if (item.netCtlPid >= 0)
			SceShell_794BF5BE(netCtlToken);
	}

	for (i = 0; i < NP_COMM2_MAX_SESSION_SLOTS; i++)
		npCommerce2CoreDestroyContext(s_sessionSlots[i].coreCtxId);
	for (i = 1; i < NP_COMM2_STATIC_TYPE_COUNT; i++) {
		if (i != NP_COMM2_KIND_SESSION)
			npCommerce2CoreDestroyContext(s_staticSlots[i].coreCtxId);
	}

	sceKernelExitThread(0);
	return 0;
}
