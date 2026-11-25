/*
 * npmanager_ipc_server.cpp: SceNpManager BlockingIpc server, SceShellSvc
 * 0x0B0000 (NpManager) and 0x0A0000 (NpCommon) IPC dispatchers, device token
 * and account hash generators, and NP debug impose notices
 * (0x8121E728..0x81220064, 0x812214B4..0x81221D40).
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <rtc.h>
#include <plugin.h>
#include <blocking_ipc/blocking_ipc_server.h>
#include <ipmi/session.h>
#include <np/np_common.h>
#include <np/np_auth_oauth.h>
#include "plugins.h"
#include "src/np/ipc/np_ipc_internal.h"

extern "C" {
extern void *sceNpCommonMallocNpServerName(SceSize size);
extern void sceNpCommonFreeNpServerName(void *ptr);
extern int SceNpCommon_BBC13621(void);
extern int sceRegMgrGetKeyStrForUser(const char *category, const char *name,
				     char *buf, SceSize size);
}

#define NP_MGR_MAX_REQUESTS		8
#define NP_MGR_MAX_CALLBACKS		4
#define NP_MGR_JOB_THREAD_ATTR		0x10000100U
#define NP_MGR_JOB_STACK_SIZE		0x4000U
#define NP_MGR_EVENT_ACCOUNT_STATE	51

#define UNIX_EPOCH_RTC_TICKS		62135596800000000ULL
#define TICKS_PER_MILLISECOND		1000ULL

enum NpManagerRequestType {
	NP_MGR_REQ_ACCOUNT	= 1,
	NP_MGR_REQ_PROFILE	= 2,
	NP_MGR_REQ_SYSUPDATE	= 3,
	NP_MGR_REQ_OAUTH	= 4
};

enum NpManagerIpcMethod {
	NP_MGR_CMD_GET_NP_ID				= 0x0B0000,
	NP_MGR_CMD_GET_NP_ID_RES			= 0x0B0001,
	NP_MGR_CMD_GET_NP_ID_FORCE			= 0x0B0002,
	NP_MGR_CMD_GET_NP_ID_FORCE_RES			= 0x0B0003,
	NP_MGR_CMD_GET_TICKET				= 0x0B0004,
	NP_MGR_CMD_GET_TICKET_RES			= 0x0B0005,
	NP_MGR_CMD_GET_TICKET_FORCE			= 0x0B0006,
	NP_MGR_CMD_GET_TICKET_FORCE_RES			= 0x0B0007,
	NP_MGR_CMD_GET_TICKET_CACHED			= 0x0B0008,
	NP_MGR_CMD_GET_TICKET_CACHED_RES		= 0x0B0009,
	NP_MGR_CMD_GET_ACCOUNT_INFO			= 0x0B000A,
	NP_MGR_CMD_GET_ACCOUNT_INFO_RES			= 0x0B000B,
	NP_MGR_CMD_UPDATE_ACCOUNT			= 0x0B000C,
	NP_MGR_CMD_UPDATE_ACCOUNT_RES			= 0x0B000D,
	NP_MGR_CMD_CHECK_SYS_UPDATE			= 0x0B000E,
	NP_MGR_CMD_CHECK_SYS_UPDATE_RES			= 0x0B000F,

	NP_MGR_CMD_GET_CACHED_NP_ID			= 0x0B0010,
	NP_MGR_CMD_CLEAR_SESSION			= 0x0B0011,
	NP_MGR_CMD_IS_SIGNED_IN				= 0x0B0012,
	NP_MGR_CMD_SET_CREDENTIALS			= 0x0B0013,
	NP_MGR_CMD_CLEAR_TICKET_CACHE			= 0x0B0014,
	NP_MGR_CMD_GET_CACHED_ACCOUNT_INFO		= 0x0B0015,
	NP_MGR_CMD_HAS_CACHED_TICKET			= 0x0B0016,
	NP_MGR_CMD_COPY_CACHED_TICKET			= 0x0B0017,
	NP_MGR_CMD_CHECK_TICKET_VALID_SIZE		= 0x0B0018,
	NP_MGR_CMD_CHECK_TICKET_VALID			= 0x0B0019,
	NP_MGR_CMD_WRITE_REG_CREDENTIALS		= 0x0B001A,
	NP_MGR_CMD_WRITE_REG_CREDENTIALS_EX		= 0x0B001B,
	NP_MGR_CMD_READ_REG_CREDENTIALS			= 0x0B001C,
	NP_MGR_CMD_GET_TICKET_VERSION			= 0x0B001D,
	NP_MGR_CMD_GET_REGION				= 0x0B001E,
	NP_MGR_CMD_GET_REGION_AND_LOCALE		= 0x0B001F,
	NP_MGR_CMD_GET_CONTENT_RATING			= 0x0B0020,
	NP_MGR_CMD_GET_CHAT_RESTRICTION			= 0x0B0021,
	NP_MGR_CMD_RESET_ACCOUNT_STATE			= 0x0B0022,
	NP_MGR_CMD_REGISTER_CALLBACK			= 0x0B0023,
	NP_MGR_CMD_UNREGISTER_CALLBACK			= 0x0B0024,
	NP_MGR_CMD_GET_LOGIN_ID				= 0x0B0025,
	NP_MGR_CMD_SIGNIN_DIALOG_START_A		= 0x0B0026,
	NP_MGR_CMD_SIGNIN_DIALOG_START_B		= 0x0B0027,
	NP_MGR_CMD_SIGNIN_DIALOG_START_C		= 0x0B0028,
	NP_MGR_CMD_SIGNIN_DIALOG_GET_STATUS		= 0x0B0029,
	NP_MGR_CMD_SIGNIN_DIALOG_ABORT			= 0x0B002A,
	NP_MGR_CMD_SIGNIN_DIALOG_GET_RESULT		= 0x0B002B,

	NP_MGR_CMD_GET_PROFILE				= 0x0B002C,
	NP_MGR_CMD_GET_PROFILE_RES			= 0x0B002D,
	NP_MGR_CMD_UPDATE_PROFILE			= 0x0B002E,
	NP_MGR_CMD_UPDATE_PROFILE_RES			= 0x0B002F,
	NP_MGR_CMD_GET_AVATAR_LIST			= 0x0B0030,
	NP_MGR_CMD_GET_AVATAR_LIST_RES			= 0x0B0031,
	NP_MGR_CMD_GET_AVATAR_URLS			= 0x0B0032,
	NP_MGR_CMD_GET_AVATAR_URLS_RES			= 0x0B0033,
	NP_MGR_CMD_GET_PANEL_CATEGORIES			= 0x0B0034,
	NP_MGR_CMD_GET_PANEL_CATEGORIES_RES		= 0x0B0035,
	NP_MGR_CMD_GET_PANEL_URLS			= 0x0B0036,
	NP_MGR_CMD_GET_PANEL_URLS_RES			= 0x0B0037,
	NP_MGR_CMD_GET_CATALOG_LIST			= 0x0B0038,
	NP_MGR_CMD_GET_CATALOG_LIST_RES			= 0x0B0039,
	NP_MGR_CMD_GET_AVATAR_BATCH			= 0x0B003A,
	NP_MGR_CMD_GET_AVATAR_BATCH_RES			= 0x0B003B,
	NP_MGR_CMD_GET_AVATAR_URL			= 0x0B003C,
	NP_MGR_CMD_GET_AVATAR_URL_RES			= 0x0B003D,
	NP_MGR_CMD_GET_TICKET_AVATAR_BATCH		= 0x0B003E,
	NP_MGR_CMD_GET_TICKET_AVATAR_BATCH_RES		= 0x0B003F,
	NP_MGR_CMD_GET_TICKET_AVATAR_URL		= 0x0B0040,
	NP_MGR_CMD_GET_TICKET_AVATAR_URL_RES		= 0x0B0041,
	NP_MGR_CMD_OAUTH_GET_ACCESS_TOKEN		= 0x0B0042,
	NP_MGR_CMD_OAUTH_GET_ACCESS_TOKEN_RES		= 0x0B0043,
	NP_MGR_CMD_OAUTH_GET_AUTH_CODE			= 0x0B0044,
	NP_MGR_CMD_OAUTH_GET_AUTH_CODE_RES		= 0x0B0045,
	NP_MGR_CMD_OAUTH_GET_AUTH_CODE_REDIRECT		= 0x0B0046,
	NP_MGR_CMD_OAUTH_GET_AUTH_CODE_REDIRECT_RES	= 0x0B0047,
	NP_MGR_CMD_OAUTH_CLEAR_TOKEN_CACHE		= 0x0B0048,
	NP_MGR_CMD_OAUTH_GET_CACHED_TOKEN		= 0x0B0049
};

enum NpCommonIpcMethod {
	NP_COMMON_CMD_IS_SIGNED_IN		= 0x0A0000,
	NP_COMMON_CMD_COMPARE_ACCOUNT_ID	= 0x0A0001,
	NP_COMMON_CMD_GET_SETTINGS		= 0x0A0002,
	NP_COMMON_CMD_GET_PRIVACY_CONFIRM	= 0x0A0003,
	NP_COMMON_CMD_SET_PRIVACY_CONFIRM	= 0x0A0004,
	NP_COMMON_CMD_GET_DEVICE_TOKEN		= 0x0A0005,
	NP_COMMON_CMD_GET_ACCOUNT_HASH		= 0x0A0006,
	NP_COMMON_CMD_SHOW_DEBUG_NOTICE		= 0x0A0007,
	NP_COMMON_CMD_SHOW_RAW_NOTICE		= 0x0A0008
};

struct NpManagerRequestSlot {
	int type;
	int coreHandle;
	int clientReqId;
	SceUID ownerPid;
};

struct NpManagerCallbackSlot {
	int callbackId;
	SceUID ownerPid;
};

struct NpManagerSetCredsIn {
	SceUInt64 accountId;
	int loginIdAbsent;
	int passwordAbsent;
};

struct NpManagerRegCredsMaskIn {
	int hasLoginId;
	int hasPassword;
	int hasAccountId;
	int hasOnlineId;
};

struct NpManagerUpdateProfileHeaderIn {
	int clientReqId;
	int aboutMeAbsent;
	int avatarUrlAbsent;
	int languagesAbsent;
	int panelColorAbsent;
	int panelIdAbsent;
	int countryCode;
	int privacyFlagAbsent;
	int commIdAbsent;
	int avatarId;
	SceSize customSize;
};

struct NpManagerTicketReqIn {
	int clientReqId;
	SceSize bufferSize;
};

struct NpManagerAccountInfoReqIn {
	int clientReqId;
	int wantProfile;
	int wantMeta;
};

struct NpManagerCheckSysUpdateReqIn {
	int clientReqId;
	int checkType;
	int checkMode;
	int checkFlag;
};

struct NpManagerGetProfileReqIn {
	int clientReqId;
	int langCode;
};

struct NpManagerListReqIn {
	int clientReqId;
	int categoryOrOffset;
	int offsetOrSort;
	SceUInt32 maxCount;
};

struct NpManagerAvatarBatchReqIn {
	int clientReqId;
	SceSize ticketSize;
	int startOffset;
	int sortMode;
	SceUInt32 maxCount;
	int skipExtraIds;
};

struct NpManagerTicketAvatarBatchReqIn {
	int clientReqId;
	int authReqId;
	SceSize ticketSize;
	int startOffset;
	int sortMode;
	SceUInt32 maxCount;
	int skipExtraIds;
};

struct NpManagerAvatarUrlReqIn {
	int clientReqId;
	SceSize ticketSize;
	int avatarId;
	int sizeType;
};

struct NpManagerTicketAvatarUrlReqIn {
	int clientReqId;
	int authReqId;
	SceSize ticketSize;
	int avatarId;
	int sizeType;
};

/* Global server state (0x815451A8..0x815451AC, 0x815A32F0..0x815A33B8) */
static BlockingIpc::Server *s_npManagerServer = NULL;
static NpJobQueue *s_npManagerJobQueue = NULL;
static NpManagerRequestSlot s_npManagerRequests[NP_MGR_MAX_REQUESTS];
static NpManagerCallbackSlot s_npManagerCallbacks[NP_MGR_MAX_CALLBACKS];
static NpIpcLwMutex s_npManagerIpcMutex;

/* Triple-DES-EDE-CBC keys and initial vector for device token (0x81514140..0x8151415F) */
static const SceUInt32 s_deviceTokenKeyFirst[2] = { 0xD34D9E4DU, 0xF8B3D917U };
static const SceUInt32 s_deviceTokenKeySecond[2] = { 0x3F5DFEA1U, 0x7F363AD0U };
static const SceUInt32 s_deviceTokenKeyThird[2] = { 0xEA591781U, 0xC4B76389U };
static const SceUInt8 s_deviceTokenInitialVector[8] = {
	0xDB, 0xC4, 0xFB, 0x4D, 0xFB, 0xEC, 0x69, 0x10
};

/* Forward declarations for BlockingIpc callbacks */
static int npManagerOnCreateRequest(IPMI::Session *session, const void *arg, SceSize argSize);
static int npManagerOnDestroyRequest(IPMI::Session *session, int clientReqId);
static int npManagerOnAbortRequest(IPMI::Session *session, int clientReqId);
static void npManagerNoopCallback(IPMI::Session *session);
static int npManagerSyncDispatch(IPMI::Session *session, SceUInt32 methodId,
				 const IPMI::DataInfo *inBufs, SceUInt32 inCount,
				 IPMI::BufferInfo *outBufs, SceUInt32 outCount);
static int npManagerAsyncDispatch(IPMI::Session *session, SceUInt32 ipcReqId,
				  SceUInt32 methodId, const IPMI::DataInfo *inBufs,
				  SceUInt32 inCount);
static int npCommonSyncDispatch(IPMI::Session *session, SceUInt32 methodId,
				const IPMI::DataInfo *inBufs, SceUInt32 inCount,
				IPMI::BufferInfo *outBufs, SceUInt32 outCount);

static void npManagerNoopCallback(IPMI::Session *session)
{
	(void)session;
}

/* 0x8121E728: initialize SceNpManager BlockingIpc server and job queue */
int npManagerIpcServerInit(void)
{
	BlockingIpc::Server::Config cfg;
	void *serverBuf = NULL;
	NpJobQueue *jobQueue = NULL;
	int ret;

	sceClibMemset(&cfg, 0, sizeof(cfg));
	cfg.threadPriority = 80;
	cfg.threadStackSize = 0x10000;
	cfg.maxSessions = 16;
	cfg.syncCallCount = 1;
	cfg.maxSyncCallInSize = 3840;
	cfg.maxSyncCallOutSize = 3840;
	cfg.asyncCallCount = 0;
	cfg.maxAsyncCallInSize = 3840;
	cfg.maxAsyncCallOutSize = 3840;
	cfg.blockingIpcCount = 1;
	cfg.maxBlockingInSize = 3840;
	cfg.maxBlockingOutSize = 3840;
	cfg.eventFlagCount = 0;
	cfg.msgQueueCount = 1;
	for (int i = 0; i < 32; ++i) {
		cfg.msgQueue[i].msgSize = 256;
		cfg.msgQueue[i].msgCount = 8;
	}
	cfg.slotCount = 0;

	if (s_npManagerServer != NULL)
		return 0;

	void *queueMem = ::operator new(88);
	if (queueMem == NULL)
		return NP_MGR_ERR_NO_MEMORY;

	jobQueue = npJobQueueConstruct(queueMem);
	if (jobQueue == NULL)
		return NP_MGR_ERR_NO_MEMORY;

	ret = npJobQueueInit(jobQueue, "SceNpManagerServerJob",
			     NP_MGR_JOB_THREAD_ATTR, NP_MGR_JOB_STACK_SIZE);
	if (ret >= 0) {
		ret = npJobQueueStart(jobQueue);
		if (ret >= 0) {
			sceClibStrncpy(cfg.name, "SceNpManager", sizeof(cfg.name));
			cfg.threadStackSize = 0x2000;
			cfg.maxSessions = 8;
			cfg.asyncCallCount = 1;
			cfg.maxAsyncCallInSize = 1024;
			cfg.maxAsyncCallOutSize = 1024;
			cfg.blockingIpcCount = 4;
			cfg.maxBlockingInSize = 1024;
			cfg.maxBlockingOutSize = 12288;
			cfg.callbacks.createRequest = npManagerOnCreateRequest;
			cfg.callbacks.destroyRequest = npManagerOnDestroyRequest;
			cfg.callbacks.abortRequest = npManagerOnAbortRequest;
			cfg.callbacks.onSessionCreated = npManagerNoopCallback;
			cfg.callbacks.onSessionClosedByClient = npManagerNoopCallback;
			cfg.callbacks.onSyncMethod = npManagerSyncDispatch;
			cfg.callbacks.onAsyncMethod = npManagerAsyncDispatch;
			cfg.budget = 2;

			SceSize bufSize = static_cast<SceSize>(
				BlockingIpc::Server::getBufferSize(&cfg));
			serverBuf = sceNpCommonMallocNpServerName(bufSize);
			if (serverBuf != NULL) {
				ret = BlockingIpc::Server::create(&s_npManagerServer, &cfg,
								  serverBuf);
				if (ret >= 0) {
					ret = s_npManagerServer->start();
					if (ret >= 0) {
						ret = npLwMutexCreate(&s_npManagerIpcMutex,
								      "SceNpManagerIpc", 1);
						if (ret >= 0) {
							sceClibMemset(s_npManagerRequests, 0,
								      sizeof(s_npManagerRequests));
							sceClibMemset(s_npManagerCallbacks, 0,
								      sizeof(s_npManagerCallbacks));
							s_npManagerJobQueue = jobQueue;
							return 0;
						}
					}
				} else {
					sceNpCommonFreeNpServerName(serverBuf);
					s_npManagerServer = NULL;
				}
			} else {
				ret = NP_MGR_ERR_NO_MEMORY;
			}
		}
	}

	delete jobQueue;
	s_npManagerJobQueue = NULL;
	if (s_npManagerServer != NULL) {
		s_npManagerServer->destroy();
		sceNpCommonFreeNpServerName(serverBuf);
		s_npManagerServer = NULL;
	}
	npLwMutexDestroy(&s_npManagerIpcMutex);
	return ret;
}

/* 0x8121E996: resolve client request ID + PID to core service handle */
int npManagerResolveRequestHandle(int clientReqId, SceUID clientPid)
{
	npLwMutexLock(&s_npManagerIpcMutex);
	for (int i = 0; i < NP_MGR_MAX_REQUESTS; ++i) {
		if (s_npManagerRequests[i].ownerPid == clientPid &&
		    s_npManagerRequests[i].clientReqId == clientReqId) {
			int coreHandle = s_npManagerRequests[i].coreHandle;
			npLwMutexUnlock(&s_npManagerIpcMutex);
			return coreHandle;
		}
	}
	npLwMutexUnlock(&s_npManagerIpcMutex);
	return NP_MGR_ERR_NO_SLOT;
}

/* 0x8121EA02: BlockingIpc create-request callback */
static int npManagerOnCreateRequest(IPMI::Session *session, const void *arg, SceSize argSize)
{
	(void)argSize;
	npLwMutexLock(&s_npManagerIpcMutex);
	if (arg == NULL) {
		npLwMutexUnlock(&s_npManagerIpcMutex);
		return NP_MGR_ERR_INVALID_TYPE;
	}

	int slotIdx = -1;
	for (int i = 0; i < NP_MGR_MAX_REQUESTS; ++i) {
		if (s_npManagerRequests[i].coreHandle == 0) {
			slotIdx = i;
			break;
		}
	}
	if (slotIdx < 0) {
		npLwMutexUnlock(&s_npManagerIpcMutex);
		return NP_MGR_ERR_NO_SLOT;
	}

	int reqType = *static_cast<const int *>(arg);
	int coreHandle = 0;
	int ret = 0;

	if (reqType == NP_MGR_REQ_ACCOUNT) {
		ret = npAccountCreateRequest();
		if (ret < 0) {
			npLwMutexUnlock(&s_npManagerIpcMutex);
			return ret;
		}
		coreHandle = ret;
	} else if (reqType == NP_MGR_REQ_PROFILE) {
		SceUInt8 onlineId[20];
		ret = npAccountGetOnlineId(onlineId);
		if (ret < 0) {
			npLwMutexUnlock(&s_npManagerIpcMutex);
			return ret;
		}
		ret = npProfileCreateRequest(onlineId, 0, NULL);
		if (ret < 0) {
			npLwMutexUnlock(&s_npManagerIpcMutex);
			return ret;
		}
		coreHandle = ret;
	} else if (reqType == NP_MGR_REQ_SYSUPDATE) {
		ret = npSysUpdateCreateRequest(&coreHandle);
		if (ret < 0) {
			npLwMutexUnlock(&s_npManagerIpcMutex);
			return ret;
		}
	} else if (reqType == NP_MGR_REQ_OAUTH) {
		ret = npOAuthCreateRequest(&coreHandle);
		if (ret < 0) {
			npLwMutexUnlock(&s_npManagerIpcMutex);
			return ret;
		}
	} else {
		npLwMutexUnlock(&s_npManagerIpcMutex);
		return NP_MGR_ERR_INVALID_TYPE;
	}

	s_npManagerRequests[slotIdx].type = reqType;
	s_npManagerRequests[slotIdx].coreHandle = coreHandle;
	s_npManagerRequests[slotIdx].clientReqId = slotIdx + 1;
	s_npManagerRequests[slotIdx].ownerPid = session->getProcessId();
	int clientReqId = s_npManagerRequests[slotIdx].clientReqId;
	npLwMutexUnlock(&s_npManagerIpcMutex);
	return clientReqId;
}

/* 0x8121EBD4 */
NpJobQueue *npManagerGetJobQueue(void)
{
	return s_npManagerJobQueue;
}

/* 0x8121EBE0: BlockingIpc destroy-request callback */
static int npManagerOnDestroyRequest(IPMI::Session *session, int clientReqId)
{
	npLwMutexLock(&s_npManagerIpcMutex);
	if (clientReqId == 0) {
		npLwMutexUnlock(&s_npManagerIpcMutex);
		return NP_MGR_ERR_NOT_FOUND;
	}

	int slotIdx = -1;
	for (int i = 0; i < NP_MGR_MAX_REQUESTS; ++i) {
		if (s_npManagerRequests[i].clientReqId == clientReqId &&
		    s_npManagerRequests[i].ownerPid == session->getProcessId()) {
			slotIdx = i;
			break;
		}
	}
	if (slotIdx < 0) {
		npLwMutexUnlock(&s_npManagerIpcMutex);
		return NP_MGR_ERR_NOT_FOUND;
	}

	NpManagerRequestSlot &slot = s_npManagerRequests[slotIdx];
	int cancelHandle = slot.coreHandle;
	if (slot.type == NP_MGR_REQ_ACCOUNT) {
		npAccountAbortRequest(slot.coreHandle);
		npAccountDestroyRequest(slot.coreHandle);
		cancelHandle = slot.coreHandle;
	} else if (slot.type == NP_MGR_REQ_PROFILE) {
		npProfileDestroyRequest(slot.coreHandle);
		cancelHandle = slot.coreHandle;
	} else if (slot.type == NP_MGR_REQ_SYSUPDATE) {
		npSysUpdateDestroyRequest(slot.coreHandle);
		slot.type = 0;
		slot.coreHandle = 0;
		slot.clientReqId = 0;
		cancelHandle = 0;
	} else if (slot.type == NP_MGR_REQ_OAUTH) {
		npOAuthDestroyRequest(slot.coreHandle);
		cancelHandle = slot.coreHandle;
	}

	npJobQueueCancel(s_npManagerJobQueue, cancelHandle, NP_MGR_ERR_DESTROYED);
	slot.type = 0;
	slot.coreHandle = 0;
	slot.clientReqId = 0;
	slot.ownerPid = 0;
	npLwMutexUnlock(&s_npManagerIpcMutex);
	return 0;
}

/* 0x8121ECE4: BlockingIpc abort-request callback */
static int npManagerOnAbortRequest(IPMI::Session *session, int clientReqId)
{
	npLwMutexLock(&s_npManagerIpcMutex);
	if (clientReqId == 0) {
		npLwMutexUnlock(&s_npManagerIpcMutex);
		return NP_MGR_ERR_NOT_FOUND;
	}

	int slotIdx = -1;
	for (int i = 0; i < NP_MGR_MAX_REQUESTS; ++i) {
		if (s_npManagerRequests[i].clientReqId == clientReqId &&
		    s_npManagerRequests[i].ownerPid == session->getProcessId()) {
			slotIdx = i;
			break;
		}
	}
	if (slotIdx < 0) {
		npLwMutexUnlock(&s_npManagerIpcMutex);
		return NP_MGR_ERR_NOT_FOUND;
	}

	NpManagerRequestSlot &slot = s_npManagerRequests[slotIdx];
	if (slot.type == NP_MGR_REQ_ACCOUNT)
		npAccountAbortRequest(slot.coreHandle);
	else if (slot.type == NP_MGR_REQ_PROFILE)
		npProfileAbortRequest(slot.coreHandle);
	else if (slot.type == NP_MGR_REQ_SYSUPDATE)
		npSysUpdateAbortRequest(slot.coreHandle);
	else if (slot.type == NP_MGR_REQ_OAUTH)
		npOAuthAbortRequest(slot.coreHandle);

	int ret = npJobQueueCancel(s_npManagerJobQueue, slot.coreHandle, NP_MGR_ERR_ABORTED);
	npLwMutexUnlock(&s_npManagerIpcMutex);
	return ret;
}

/* 0x8121EDCA: ShellSvc 0x0B sync dispatcher */
int shellSvcDisp_0B_Sync(IPMI::Session *session, SceUInt32 methodId,
			 const IPMI::DataInfo *inBufs, SceUInt32 inCount,
			 IPMI::BufferInfo *outBufs, SceUInt32 outCount)
{
	return npManagerSyncDispatch(session, methodId, inBufs, inCount, outBufs, outCount);
}

/* 0x8121EDDE: ShellSvc 0x0B async dispatcher */
int shellSvcDisp_0B_Async(IPMI::Session *session, SceUInt32 ipcReqId,
			  SceUInt32 methodId, const IPMI::DataInfo *inBufs,
			  SceUInt32 inCount)
{
	return npManagerAsyncDispatch(session, ipcReqId, methodId, inBufs, inCount);
}

/* 0x8121EDEE: ShellSvc 0x0B disconnect cleanup */
int shellSvcDisp_0B_Disconn(IPMI::Session *session)
{
	npLwMutexLock(&s_npManagerIpcMutex);
	for (int i = 0; i < NP_MGR_MAX_REQUESTS; ++i) {
		if (s_npManagerRequests[i].ownerPid == session->getProcessId())
			npManagerOnDestroyRequest(session, s_npManagerRequests[i].clientReqId);
	}
	for (int i = 0; i < NP_MGR_MAX_CALLBACKS; ++i) {
		if (s_npManagerCallbacks[i].ownerPid == session->getProcessId()) {
			npAccountUnregisterCallback(s_npManagerCallbacks[i].callbackId);
			s_npManagerCallbacks[i].callbackId = 0;
			s_npManagerCallbacks[i].ownerPid = 0;
		}
	}
	if (session->getProcessId() == 0) {
		npSignInDialogAbort(0);
		npSignInDialogGetResult(0, NULL);
		npSignInDialogCleanup();
	}
	npLwMutexUnlock(&s_npManagerIpcMutex);
	return 0;
}

/* 0x8121EE88 */
NpIpcLwMutex *npManagerGetIpcMutex(void)
{
	return &s_npManagerIpcMutex;
}

/* 0x8121F1C0: account state change callback forwarding event 51 to ShellSvc client */
static int npManagerAccountStateCallback(int eventKind, IPMI::Session *session)
{
	if (eventKind == 0)
		return shellSvcNotifyEvent(session, NP_MGR_EVENT_ACCOUNT_STATE);
	return 0;
}

/* Sync IPC handler helpers (0x8121EE92..0x8121F1B4) */
static int npManagerHandleGetCachedNpId(IPMI::Session *session, SceNpId *outNpId)
{
	(void)session;
	return npAccountGetCachedNpId(outNpId);
}

static int npManagerHandleClearSession(void)
{
	return npAccountClearSession();
}

static int npManagerHandleIsSignedIn(void)
{
	return npAccountIsSignedIn();
}

static int npManagerHandleSetCredentials(IPMI::Session *session, SceUInt64 accountId,
					 const void *loginId, int loginIdAbsent,
					 const void *password, int passwordAbsent)
{
	(void)session;
	return npAccountSetCredentials(accountId,
				       loginIdAbsent == 1 ? NULL : loginId,
				       passwordAbsent == 1 ? NULL : password);
}

static int npManagerHandleClearTicketCache(void)
{
	return npAccountClearTicketCache();
}

static int npManagerHandleGetCachedAccountInfo(IPMI::Session *session, void *outInfo)
{
	(void)session;
	return npAccountGetCachedAccountInfo(outInfo);
}

static int npManagerHandleHasCachedTicket(void)
{
	return npAccountHasCachedTicket();
}

static int npManagerHandleCopyCachedTicket(IPMI::Session *session, void *outTicket,
					   SceSize maxSize, SceSize *ioSize)
{
	(void)session;
	*ioSize = maxSize;
	return npAccountCopyCachedTicket(outTicket, ioSize);
}

static int npManagerHandleCheckTicketValidSize(IPMI::Session *session, SceSize *outSize)
{
	(void)session;
	return npAccountCheckTicketValid(NULL, outSize);
}

static int npManagerHandleCheckTicketValid(IPMI::Session *session, int isNullTicket,
					   void *outTicket, SceSize *outSize)
{
	(void)session;
	return npAccountCheckTicketValid(isNullTicket == 1 ? NULL : outTicket, outSize);
}

static int npManagerHandleWriteRegCredentials(IPMI::Session *session,
					      int hasLoginId, const void *loginId,
					      int hasPassword, const void *password,
					      int hasAccountId, const SceUInt64 *accountId)
{
	(void)session;
	return npAccountWriteRegistryCredentials(hasLoginId == 1 ? loginId : NULL,
						 hasPassword == 1 ? password : NULL,
						 hasAccountId == 1 ? accountId : NULL);
}

static int npManagerHandleWriteRegCredentialsEx(IPMI::Session *session,
						int hasLoginId, const void *loginId,
						int hasPassword, const void *password,
						int hasAccountId, const SceUInt64 *accountId,
						int hasOnlineId, const char *onlineId)
{
	(void)session;
	return npAccountWriteRegistryCredentialsEx(hasLoginId == 1 ? loginId : NULL,
						   hasPassword == 1 ? password : NULL,
						   hasAccountId == 1 ? accountId : NULL,
						   hasOnlineId == 1 ? onlineId : NULL);
}

static int npManagerHandleReadRegCredentials(IPMI::Session *session,
					     int hasLoginId, void *loginId,
					     int hasPassword, void *password,
					     int hasAccountId, SceUInt64 *accountId)
{
	(void)session;
	return npAccountReadRegistryCredentials(hasLoginId == 1 ? loginId : NULL,
						hasPassword == 1 ? password : NULL,
						hasAccountId == 1 ? accountId : NULL);
}

static int npManagerHandleGetTicketVersion(void)
{
	return npAccountGetTicketVersion();
}

static int npManagerHandleGetRegion(IPMI::Session *session, SceNpCountryCode *countryCode,
				    int *languageCode)
{
	(void)session;
	return npAccountGetRegion(countryCode, languageCode);
}

static int npManagerHandleGetRegionAndLocale(IPMI::Session *session, SceNpCountryCode *countryCode,
					     char *localeStr)
{
	(void)session;
	return npAccountGetRegionAndLocale(countryCode, localeStr);
}

static int npManagerHandleGetContentRating(IPMI::Session *session, int *isRestricted, int *age)
{
	(void)session;
	return npAccountGetContentRating(isRestricted, age);
}

static int npManagerHandleGetChatRestriction(IPMI::Session *session, int *isRestricted)
{
	(void)session;
	return npAccountGetChatRestriction(isRestricted);
}

static int npManagerHandleResetAccountState(void)
{
	npAccountResetState();
	return 0;
}

static int npManagerHandleRegisterCallback(IPMI::Session *session)
{
	npLwMutexLock(&s_npManagerIpcMutex);
	int slotIdx = -1;
	for (int i = 0; i < NP_MGR_MAX_CALLBACKS; ++i) {
		if (s_npManagerCallbacks[i].ownerPid == 0) {
			slotIdx = i;
			break;
		}
	}
	if (slotIdx < 0) {
		npLwMutexUnlock(&s_npManagerIpcMutex);
		return NP_MGR_ERR_NO_SLOT;
	}

	int cbId = npAccountRegisterCallback(npManagerAccountStateCallback, session);
	if (cbId >= 0) {
		s_npManagerCallbacks[slotIdx].callbackId = cbId;
		s_npManagerCallbacks[slotIdx].ownerPid = session->getProcessId();
	}
	npLwMutexUnlock(&s_npManagerIpcMutex);
	return cbId;
}

static int npManagerHandleUnregisterCallback(IPMI::Session *session, int callbackId)
{
	(void)session;
	npLwMutexLock(&s_npManagerIpcMutex);
	int slotIdx = -1;
	for (int i = 0; i < NP_MGR_MAX_CALLBACKS; ++i) {
		if (s_npManagerCallbacks[i].callbackId == callbackId) {
			slotIdx = i;
			break;
		}
	}
	if (slotIdx < 0) {
		npLwMutexUnlock(&s_npManagerIpcMutex);
		return NP_MGR_ERR_INVALID_ARG;
	}

	int ret = npAccountUnregisterCallback(callbackId);
	if (ret >= 0) {
		s_npManagerCallbacks[slotIdx].callbackId = 0;
		s_npManagerCallbacks[slotIdx].ownerPid = 0;
	}
	npLwMutexUnlock(&s_npManagerIpcMutex);
	return ret;
}

static int npManagerHandleGetLoginId(IPMI::Session *session, char *outLoginId)
{
	(void)session;
	char loginBuf[68];
	int ret = npAccountGetMemoryCredentials(loginBuf, NULL, NULL);
	if (ret < 0)
		return 0;
	sceClibStrncpy(outLoginId, loginBuf, 63);
	outLoginId[63] = '\0';
	return ret;
}

static int npManagerHandleSignInDialogStartA(IPMI::Session *session, int *outReqId,
					     int dialogParam)
{
	return npSignInDialogStartModeA(outReqId, dialogParam, session->getProcessId());
}

static int npManagerHandleSignInDialogStartB(IPMI::Session *session, int *outReqId,
					     int dialogParam)
{
	return npSignInDialogStartModeB(outReqId, dialogParam, session->getProcessId());
}

static int npManagerHandleSignInDialogStartC(IPMI::Session *session, int *outReqId,
					     int dialogParam)
{
	return npSignInDialogStartModeC(outReqId, dialogParam, session->getProcessId());
}

static int npManagerHandleSignInDialogGetStatus(IPMI::Session *session, int dialogReqId)
{
	(void)session;
	return npSignInDialogGetStatus(dialogReqId);
}

static int npManagerHandleSignInDialogAbort(IPMI::Session *session, int dialogReqId)
{
	(void)session;
	return npSignInDialogAbort(dialogReqId);
}

static int npManagerHandleSignInDialogGetResult(IPMI::Session *session, int dialogReqId,
						int *outResult)
{
	(void)session;
	return npSignInDialogGetResult(dialogReqId, outResult);
}

static int npManagerHandleOAuthClearTokenCache(void)
{
	npOAuthClearAccessTokenCache();
	return 0;
}

static int npManagerHandleOAuthGetCachedToken(IPMI::Session *session, void *outToken,
					      bool *outValid)
{
	(void)session;
	return npOAuthGetCachedAccessToken(outToken, outValid);
}

/* Async response senders (0x8121F1F8..0x8121F932) */
int npManagerSendGetNpIdResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
				 const SceNpId *npId)
{
	IPMI::DataInfo out;
	out.pData = npId;
	out.dataSize = 36;
	return session->respondToAsyncMethodRequest(NP_MGR_CMD_GET_NP_ID_RES,
						    ipcReqId, result, &out, 1);
}

int npManagerSendGetNpIdForceResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
				      const SceNpId *npId)
{
	IPMI::DataInfo out;
	out.pData = npId;
	out.dataSize = 36;
	return session->respondToAsyncMethodRequest(NP_MGR_CMD_GET_NP_ID_FORCE_RES,
						    ipcReqId, result, &out, 1);
}

int npManagerSendGetTicketResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
				   const SceSize *totalSize, const SceSize *copySize,
				   const void *ticketBuf)
{
	IPMI::DataInfo out[3];
	out[0].pData = totalSize;
	out[0].dataSize = sizeof(SceSize);
	out[1].pData = copySize;
	out[1].dataSize = sizeof(SceSize);
	out[2].pData = ticketBuf;
	out[2].dataSize = *copySize;
	return session->respondToAsyncMethodRequest(NP_MGR_CMD_GET_TICKET_RES,
						    ipcReqId, result, out, 3);
}

int npManagerSendGetTicketForceResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
					const SceSize *totalSize, const SceSize *copySize,
					const void *ticketBuf)
{
	IPMI::DataInfo out[3];
	out[0].pData = totalSize;
	out[0].dataSize = sizeof(SceSize);
	out[1].pData = copySize;
	out[1].dataSize = sizeof(SceSize);
	out[2].pData = ticketBuf;
	out[2].dataSize = *copySize;
	return session->respondToAsyncMethodRequest(NP_MGR_CMD_GET_TICKET_FORCE_RES,
						    ipcReqId, result, out, 3);
}

int npManagerSendGetTicketCachedResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
					 const SceSize *totalSize, const SceSize *copySize,
					 const void *ticketBuf)
{
	IPMI::DataInfo out[3];
	out[0].pData = totalSize;
	out[0].dataSize = sizeof(SceSize);
	out[1].pData = copySize;
	out[1].dataSize = sizeof(SceSize);
	out[2].pData = ticketBuf;
	out[2].dataSize = *copySize;
	return session->respondToAsyncMethodRequest(NP_MGR_CMD_GET_TICKET_CACHED_RES,
						    ipcReqId, result, out, 3);
}

int npManagerSendGetAccountInfoResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
					const void *profileBuf, const void *accountMetaBuf)
{
	IPMI::DataInfo out[2];
	out[0].pData = profileBuf;
	out[0].dataSize = 464;
	out[1].pData = accountMetaBuf;
	out[1].dataSize = 16;
	return session->respondToAsyncMethodRequest(NP_MGR_CMD_GET_ACCOUNT_INFO_RES,
						    ipcReqId, result, out, 2);
}

int npManagerSendUpdateAccountResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result)
{
	return session->respondToAsyncMethodRequest(NP_MGR_CMD_UPDATE_ACCOUNT_RES,
						    ipcReqId, result, NULL, 0);
}

int npManagerSendCheckSysUpdateResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
					const int *outStatus)
{
	IPMI::DataInfo out;
	out.pData = outStatus;
	out.dataSize = sizeof(int);
	return session->respondToAsyncMethodRequest(NP_MGR_CMD_CHECK_SYS_UPDATE_RES,
						    ipcReqId, result, &out, 1);
}

int npManagerSendGetProfileResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
				    const void *profileBuf)
{
	IPMI::DataInfo out;
	out.pData = profileBuf;
	out.dataSize = 456;
	return session->respondToAsyncMethodRequest(NP_MGR_CMD_GET_PROFILE_RES,
						    ipcReqId, result, &out, 1);
}

/* 0x8121F468: unpack UpdateProfile async input buffers and enqueue job */
static int npManagerUnpackUpdateProfile(IPMI::Session *session, SceUInt32 ipcReqId,
					const IPMI::DataInfo *inBufs)
{
	const NpManagerUpdateProfileHeaderIn *hdr =
		static_cast<const NpManagerUpdateProfileHeaderIn *>(inBufs[0].pData);
	return npManagerEnqueueUpdateProfileJob(session, ipcReqId, hdr->clientReqId,
						inBufs[1].pData, hdr->aboutMeAbsent,
						inBufs[2].pData, hdr->avatarUrlAbsent,
						inBufs[3].pData, hdr->languagesAbsent,
						static_cast<const int *>(inBufs[4].pData),
						hdr->panelColorAbsent,
						inBufs[5].pData, hdr->panelIdAbsent,
						hdr->countryCode,
						static_cast<const int *>(inBufs[6].pData),
						hdr->privacyFlagAbsent,
						inBufs[7].pData, hdr->commIdAbsent,
						hdr->avatarId,
						inBufs[8].pData, hdr->customSize);
}

int npManagerSendUpdateProfileResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
				       const void *profileBuf, const int *outStatus,
				       const int *outFlags, const void *outStamp)
{
	IPMI::DataInfo out[4];
	out[0].pData = profileBuf;
	out[0].dataSize = 456;
	out[1].pData = outStatus;
	out[1].dataSize = sizeof(int);
	out[2].pData = outFlags;
	out[2].dataSize = sizeof(int);
	out[3].pData = outStamp;
	out[3].dataSize = 8;
	return session->respondToAsyncMethodRequest(NP_MGR_CMD_UPDATE_PROFILE_RES,
						    ipcReqId, result, out, 4);
}

int npManagerSendGetAvatarListResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
				       const SceUInt32 *outCount, const void *entries,
				       const int *outTotal)
{
	IPMI::DataInfo out[3];
	out[0].pData = outCount;
	out[0].dataSize = sizeof(SceUInt32);
	out[1].pData = entries;
	out[1].dataSize = 412 * (*outCount);
	out[2].pData = outTotal;
	out[2].dataSize = sizeof(int);
	return session->respondToAsyncMethodRequest(NP_MGR_CMD_GET_AVATAR_LIST_RES,
						    ipcReqId, result, out, 3);
}

int npManagerSendGetAvatarUrlsResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
				       const SceUInt32 *outCount, const int *avatarIds,
				       const void *urlEntries, const int *outTotal)
{
	IPMI::DataInfo out[4];
	SceUInt32 count = *outCount;
	out[0].pData = outCount;
	out[0].dataSize = sizeof(SceUInt32);
	out[1].pData = avatarIds;
	out[1].dataSize = sizeof(int) * count;
	out[2].pData = urlEntries;
	out[2].dataSize = 128 * count;
	out[3].pData = outTotal;
	out[3].dataSize = sizeof(int);
	return session->respondToAsyncMethodRequest(NP_MGR_CMD_GET_AVATAR_URLS_RES,
						    ipcReqId, result, out, 4);
}

int npManagerSendGetPanelCategoriesResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
					    const SceUInt32 *outCount, const void *entries,
					    const int *outTotal)
{
	IPMI::DataInfo out[3];
	out[0].pData = outCount;
	out[0].dataSize = sizeof(SceUInt32);
	out[1].pData = entries;
	out[1].dataSize = 412 * (*outCount);
	out[2].pData = outTotal;
	out[2].dataSize = sizeof(int);
	return session->respondToAsyncMethodRequest(NP_MGR_CMD_GET_PANEL_CATEGORIES_RES,
						    ipcReqId, result, out, 3);
}

int npManagerSendGetPanelUrlsResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
				      const SceUInt32 *outCount, const int *panelIds,
				      const int *bgColors, const void *urlEntries,
				      const int *outTotal)
{
	IPMI::DataInfo out[5];
	SceUInt32 count = *outCount;
	out[0].pData = outCount;
	out[0].dataSize = sizeof(SceUInt32);
	out[1].pData = panelIds;
	out[1].dataSize = sizeof(int) * count;
	out[2].pData = bgColors;
	out[2].dataSize = sizeof(int) * count;
	out[3].pData = urlEntries;
	out[3].dataSize = 128 * count;
	out[4].pData = outTotal;
	out[4].dataSize = sizeof(int);
	return session->respondToAsyncMethodRequest(NP_MGR_CMD_GET_PANEL_URLS_RES,
						    ipcReqId, result, out, 5);
}

int npManagerSendGetCatalogListResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
					const SceUInt32 *outCount, const void *entries,
					const int *outTotal)
{
	IPMI::DataInfo out[3];
	out[0].pData = outCount;
	out[0].dataSize = sizeof(SceUInt32);
	out[1].pData = entries;
	out[1].dataSize = 412 * (*outCount);
	out[2].pData = outTotal;
	out[2].dataSize = sizeof(int);
	return session->respondToAsyncMethodRequest(NP_MGR_CMD_GET_CATALOG_LIST_RES,
						    ipcReqId, result, out, 3);
}

int npManagerSendGetAvatarBatchResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
					const SceUInt32 *outCount, const int *avatarIds,
					const void *urlEntries, const SceUInt32 *outExtraCount,
					const int *extraIds, const int *outTotal,
					const int *outNextOffset)
{
	IPMI::DataInfo out[7];
	SceUInt32 count = *outCount;
	out[0].pData = outCount;
	out[0].dataSize = sizeof(SceUInt32);
	out[1].pData = avatarIds;
	out[1].dataSize = sizeof(int) * count;
	out[2].pData = urlEntries;
	out[2].dataSize = 128 * count;
	out[3].pData = outExtraCount;
	out[3].dataSize = sizeof(SceUInt32);
	out[4].pData = extraIds;
	out[4].dataSize = sizeof(int) * (*outExtraCount);
	out[5].pData = outTotal;
	out[5].dataSize = sizeof(int);
	out[6].pData = outNextOffset;
	out[6].dataSize = sizeof(int);
	return session->respondToAsyncMethodRequest(NP_MGR_CMD_GET_AVATAR_BATCH_RES,
						    ipcReqId, result, out, 7);
}

int npManagerSendGetAvatarUrlResponse(IPMI::Session *session, SceUInt32 ipcReqId, int result,
				      const void *urlBuf)
{
	IPMI::DataInfo out;
	out.pData = urlBuf;
	out.dataSize = 128;
	return session->respondToAsyncMethodRequest(NP_MGR_CMD_GET_AVATAR_URL_RES,
						    ipcReqId, result, &out, 1);
}

int npManagerSendGetTicketAvatarBatchResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					      int result, const SceUInt32 *outCount,
					      const int *avatarIds, const void *urlEntries,
					      const SceUInt32 *outExtraCount,
					      const int *extraIds, const int *outTotal,
					      const int *outNextOffset)
{
	IPMI::DataInfo out[7];
	SceUInt32 count = *outCount;
	out[0].pData = outCount;
	out[0].dataSize = sizeof(SceUInt32);
	out[1].pData = avatarIds;
	out[1].dataSize = sizeof(int) * count;
	out[2].pData = urlEntries;
	out[2].dataSize = 128 * count;
	out[3].pData = outExtraCount;
	out[3].dataSize = sizeof(SceUInt32);
	out[4].pData = extraIds;
	out[4].dataSize = sizeof(int) * (*outExtraCount);
	out[5].pData = outTotal;
	out[5].dataSize = sizeof(int);
	out[6].pData = outNextOffset;
	out[6].dataSize = sizeof(int);
	return session->respondToAsyncMethodRequest(NP_MGR_CMD_GET_TICKET_AVATAR_BATCH_RES,
						    ipcReqId, result, out, 7);
}

int npManagerSendGetTicketAvatarUrlResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					    int result, const void *urlBuf)
{
	IPMI::DataInfo out;
	out.pData = urlBuf;
	out.dataSize = 128;
	return session->respondToAsyncMethodRequest(NP_MGR_CMD_GET_TICKET_AVATAR_URL_RES,
						    ipcReqId, result, &out, 1);
}

int npManagerSendOAuthGetAccessTokenResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					     int result, const void *tokenBuf)
{
	IPMI::DataInfo out;
	out.pData = tokenBuf;
	out.dataSize = 264;
	return session->respondToAsyncMethodRequest(NP_MGR_CMD_OAUTH_GET_ACCESS_TOKEN_RES,
						    ipcReqId, result, &out, 1);
}

int npManagerSendOAuthGetAuthCodeResponse(IPMI::Session *session, SceUInt32 ipcReqId,
					  int result, const SceNpAuthorizationCode *authCode,
					  const int *issuerId)
{
	IPMI::DataInfo out[2];
	out[0].pData = authCode;
	out[0].dataSize = sizeof(SceNpAuthorizationCode);
	out[1].pData = issuerId;
	out[1].dataSize = sizeof(int);
	return session->respondToAsyncMethodRequest(NP_MGR_CMD_OAUTH_GET_AUTH_CODE_RES,
						    ipcReqId, result, out, 2);
}

int npManagerSendOAuthGetAuthCodeWithRedirectResponse(IPMI::Session *session, SceUInt32 ipcReqId,
						      int result,
						      const SceNpAuthorizationCode *authCode,
						      const int *issuerId)
{
	IPMI::DataInfo out[2];
	out[0].pData = authCode;
	out[0].dataSize = sizeof(SceNpAuthorizationCode);
	out[1].pData = issuerId;
	out[1].dataSize = sizeof(int);
	return session->respondToAsyncMethodRequest(NP_MGR_CMD_OAUTH_GET_AUTH_CODE_REDIRECT_RES,
						    ipcReqId, result, out, 2);
}

/* 0x8121F97E: SceNpManager synchronous method dispatcher */
static int npManagerSyncDispatch(IPMI::Session *session, SceUInt32 methodId,
				 const IPMI::DataInfo *inBufs, SceUInt32 inCount,
				 IPMI::BufferInfo *outBufs, SceUInt32 outCount)
{
	(void)inCount;
	int ret;

	switch (methodId) {
	case NP_MGR_CMD_GET_CACHED_NP_ID:
		ret = npManagerHandleGetCachedNpId(session,
						   static_cast<SceNpId *>(outBufs[0].pBuffer));
		outBufs[0].outSize = 36;
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_MGR_CMD_CLEAR_SESSION:
		ret = npManagerHandleClearSession();
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_MGR_CMD_IS_SIGNED_IN:
		ret = npManagerHandleIsSignedIn();
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_MGR_CMD_SET_CREDENTIALS: {
		const NpManagerSetCredsIn *in =
			static_cast<const NpManagerSetCredsIn *>(inBufs[0].pData);
		ret = npManagerHandleSetCredentials(session, in->accountId,
						    inBufs[1].pData, in->loginIdAbsent,
						    inBufs[2].pData, in->passwordAbsent);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	}

	case NP_MGR_CMD_CLEAR_TICKET_CACHE:
		ret = npManagerHandleClearTicketCache();
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_MGR_CMD_GET_CACHED_ACCOUNT_INFO:
		ret = npManagerHandleGetCachedAccountInfo(session, outBufs[0].pBuffer);
		outBufs[0].outSize = 328;
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_MGR_CMD_HAS_CACHED_TICKET:
		ret = npManagerHandleHasCachedTicket();
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_MGR_CMD_COPY_CACHED_TICKET: {
		SceSize *outSize = static_cast<SceSize *>(outBufs[1].pBuffer);
		SceSize maxSize = *static_cast<const SceSize *>(inBufs[0].pData);
		ret = npManagerHandleCopyCachedTicket(session, outBufs[0].pBuffer,
						      maxSize, outSize);
		outBufs[0].outSize = *outSize;
		outBufs[1].outSize = sizeof(SceSize);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	}

	case NP_MGR_CMD_CHECK_TICKET_VALID_SIZE:
		ret = npManagerHandleCheckTicketValidSize(session,
							  static_cast<SceSize *>(outBufs[0].pBuffer));
		outBufs[0].outSize = sizeof(SceSize);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_MGR_CMD_CHECK_TICKET_VALID: {
		SceSize *outSize = static_cast<SceSize *>(outBufs[1].pBuffer);
		int isNullTicket = *static_cast<const int *>(inBufs[0].pData);
		ret = npManagerHandleCheckTicketValid(session, isNullTicket,
						      outBufs[0].pBuffer, outSize);
		outBufs[0].outSize = *outSize;
		outBufs[1].outSize = sizeof(SceSize);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	}

	case NP_MGR_CMD_WRITE_REG_CREDENTIALS: {
		const NpManagerRegCredsMaskIn *mask =
			static_cast<const NpManagerRegCredsMaskIn *>(inBufs[0].pData);
		ret = npManagerHandleWriteRegCredentials(
			session,
			mask->hasLoginId, inBufs[1].pData,
			mask->hasPassword, inBufs[2].pData,
			mask->hasAccountId,
			static_cast<const SceUInt64 *>(inBufs[3].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	}

	case NP_MGR_CMD_WRITE_REG_CREDENTIALS_EX: {
		const NpManagerRegCredsMaskIn *mask =
			static_cast<const NpManagerRegCredsMaskIn *>(inBufs[0].pData);
		ret = npManagerHandleWriteRegCredentialsEx(
			session,
			mask->hasLoginId, inBufs[1].pData,
			mask->hasPassword, inBufs[2].pData,
			mask->hasAccountId,
			static_cast<const SceUInt64 *>(inBufs[3].pData),
			mask->hasOnlineId,
			static_cast<const char *>(inBufs[4].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	}

	case NP_MGR_CMD_READ_REG_CREDENTIALS: {
		const NpManagerRegCredsMaskIn *mask =
			static_cast<const NpManagerRegCredsMaskIn *>(inBufs[0].pData);
		ret = npManagerHandleReadRegCredentials(
			session,
			mask->hasLoginId, outBufs[0].pBuffer,
			mask->hasPassword, outBufs[1].pBuffer,
			mask->hasAccountId,
			static_cast<SceUInt64 *>(outBufs[2].pBuffer));
		outBufs[0].outSize = 68;
		outBufs[1].outSize = 36;
		outBufs[2].outSize = sizeof(SceUInt64);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	}

	case NP_MGR_CMD_GET_TICKET_VERSION:
		ret = npManagerHandleGetTicketVersion();
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_MGR_CMD_GET_REGION:
		ret = npManagerHandleGetRegion(session,
					       static_cast<SceNpCountryCode *>(outBufs[0].pBuffer),
					       static_cast<int *>(outBufs[1].pBuffer));
		outBufs[0].outSize = 4;
		outBufs[1].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_MGR_CMD_GET_REGION_AND_LOCALE:
		ret = npManagerHandleGetRegionAndLocale(
			session,
			static_cast<SceNpCountryCode *>(outBufs[0].pBuffer),
			static_cast<char *>(outBufs[1].pBuffer));
		outBufs[0].outSize = 4;
		outBufs[1].outSize = 6;
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_MGR_CMD_GET_CONTENT_RATING:
		ret = npManagerHandleGetContentRating(session,
						      static_cast<int *>(outBufs[0].pBuffer),
						      static_cast<int *>(outBufs[1].pBuffer));
		outBufs[0].outSize = sizeof(int);
		outBufs[1].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_MGR_CMD_GET_CHAT_RESTRICTION:
		ret = npManagerHandleGetChatRestriction(session,
							static_cast<int *>(outBufs[0].pBuffer));
		outBufs[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_MGR_CMD_RESET_ACCOUNT_STATE:
		ret = npManagerHandleResetAccountState();
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_MGR_CMD_REGISTER_CALLBACK:
		ret = npManagerHandleRegisterCallback(session);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_MGR_CMD_UNREGISTER_CALLBACK:
		ret = npManagerHandleUnregisterCallback(
			session, *static_cast<const int *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_MGR_CMD_GET_LOGIN_ID:
		ret = npManagerHandleGetLoginId(session, static_cast<char *>(outBufs[0].pBuffer));
		outBufs[0].outSize = 64;
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_MGR_CMD_SIGNIN_DIALOG_START_A:
		ret = npManagerHandleSignInDialogStartA(
			session, static_cast<int *>(outBufs[0].pBuffer),
			*static_cast<const int *>(inBufs[0].pData));
		outBufs[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_MGR_CMD_SIGNIN_DIALOG_START_B:
		ret = npManagerHandleSignInDialogStartB(
			session, static_cast<int *>(outBufs[0].pBuffer),
			*static_cast<const int *>(inBufs[0].pData));
		outBufs[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_MGR_CMD_SIGNIN_DIALOG_START_C:
		ret = npManagerHandleSignInDialogStartC(
			session, static_cast<int *>(outBufs[0].pBuffer),
			*static_cast<const int *>(inBufs[0].pData));
		outBufs[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_MGR_CMD_SIGNIN_DIALOG_GET_STATUS:
		ret = npManagerHandleSignInDialogGetStatus(
			session, *static_cast<const int *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_MGR_CMD_SIGNIN_DIALOG_ABORT:
		ret = npManagerHandleSignInDialogAbort(
			session, *static_cast<const int *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_MGR_CMD_SIGNIN_DIALOG_GET_RESULT:
		ret = npManagerHandleSignInDialogGetResult(
			session, *static_cast<const int *>(inBufs[0].pData),
			static_cast<int *>(outBufs[0].pBuffer));
		outBufs[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_MGR_CMD_OAUTH_CLEAR_TOKEN_CACHE:
		ret = npManagerHandleOAuthClearTokenCache();
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_MGR_CMD_OAUTH_GET_CACHED_TOKEN:
		ret = npManagerHandleOAuthGetCachedToken(session, outBufs[0].pBuffer,
							 static_cast<bool *>(outBufs[1].pBuffer));
		outBufs[0].outSize = 264;
		outBufs[1].outSize = 1;
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	default:
		return IPMI_ERR_UNSUPPORTED_METHOD;
	}
}

/* 0x8121FE00: SceNpManager asynchronous method dispatcher */
static int npManagerAsyncDispatch(IPMI::Session *session, SceUInt32 ipcReqId,
				  SceUInt32 methodId, const IPMI::DataInfo *inBufs,
				  SceUInt32 inCount)
{
	(void)inCount;

	switch (methodId) {
	case NP_MGR_CMD_GET_NP_ID:
		return npManagerEnqueueGetNpIdJob(session, ipcReqId,
						  *static_cast<const int *>(inBufs[0].pData));

	case NP_MGR_CMD_GET_NP_ID_FORCE:
		return npManagerEnqueueGetNpIdForceJob(session, ipcReqId,
						       *static_cast<const int *>(inBufs[0].pData));

	case NP_MGR_CMD_GET_TICKET: {
		const NpManagerTicketReqIn *in =
			static_cast<const NpManagerTicketReqIn *>(inBufs[0].pData);
		return npManagerEnqueueGetTicketJob(session, ipcReqId,
						    in->clientReqId, in->bufferSize);
	}

	case NP_MGR_CMD_GET_TICKET_FORCE: {
		const NpManagerTicketReqIn *in =
			static_cast<const NpManagerTicketReqIn *>(inBufs[0].pData);
		return npManagerEnqueueGetTicketForceJob(session, ipcReqId,
							 in->clientReqId, in->bufferSize);
	}

	case NP_MGR_CMD_GET_TICKET_CACHED: {
		const NpManagerTicketReqIn *in =
			static_cast<const NpManagerTicketReqIn *>(inBufs[0].pData);
		return npManagerEnqueueGetTicketCachedJob(session, ipcReqId,
							  in->clientReqId, in->bufferSize);
	}

	case NP_MGR_CMD_GET_ACCOUNT_INFO: {
		const NpManagerAccountInfoReqIn *in =
			static_cast<const NpManagerAccountInfoReqIn *>(inBufs[0].pData);
		return npManagerEnqueueGetAccountInfoJob(session, ipcReqId, in->clientReqId,
							 in->wantProfile, in->wantMeta);
	}

	case NP_MGR_CMD_UPDATE_ACCOUNT:
		return npManagerEnqueueUpdateAccountJob(session, ipcReqId,
							*static_cast<const int *>(inBufs[0].pData),
							inBufs[1].pData, inBufs[2].pData);

	case NP_MGR_CMD_CHECK_SYS_UPDATE: {
		const NpManagerCheckSysUpdateReqIn *in =
			static_cast<const NpManagerCheckSysUpdateReqIn *>(inBufs[0].pData);
		return npManagerEnqueueCheckSysUpdateJob(session, ipcReqId, in->clientReqId,
							 in->checkType, in->checkMode,
							 in->checkFlag);
	}

	case NP_MGR_CMD_GET_PROFILE: {
		const NpManagerGetProfileReqIn *in =
			static_cast<const NpManagerGetProfileReqIn *>(inBufs[0].pData);
		return npManagerEnqueueGetProfileJob(session, ipcReqId, in->clientReqId,
						     inBufs[1].pData, in->langCode);
	}

	case NP_MGR_CMD_UPDATE_PROFILE:
		return npManagerUnpackUpdateProfile(session, ipcReqId, inBufs);

	case NP_MGR_CMD_GET_AVATAR_LIST: {
		const NpManagerListReqIn *in =
			static_cast<const NpManagerListReqIn *>(inBufs[0].pData);
		return npManagerEnqueueGetAvatarListJob(session, ipcReqId, in->clientReqId,
							in->categoryOrOffset, in->offsetOrSort,
							in->maxCount);
	}

	case NP_MGR_CMD_GET_AVATAR_URLS: {
		const NpManagerListReqIn *in =
			static_cast<const NpManagerListReqIn *>(inBufs[0].pData);
		return npManagerEnqueueGetAvatarUrlsJob(session, ipcReqId, in->clientReqId,
							static_cast<const int *>(inBufs[1].pData),
							in->categoryOrOffset, in->offsetOrSort,
							in->maxCount);
	}

	case NP_MGR_CMD_GET_PANEL_CATEGORIES: {
		const NpManagerListReqIn *in =
			static_cast<const NpManagerListReqIn *>(inBufs[0].pData);
		return npManagerEnqueueGetPanelCategoriesJob(session, ipcReqId, in->clientReqId,
							     in->categoryOrOffset,
							     in->offsetOrSort, in->maxCount);
	}

	case NP_MGR_CMD_GET_PANEL_URLS: {
		const NpManagerListReqIn *in =
			static_cast<const NpManagerListReqIn *>(inBufs[0].pData);
		return npManagerEnqueueGetPanelUrlsJob(session, ipcReqId, in->clientReqId,
						       static_cast<const int *>(inBufs[1].pData),
						       in->categoryOrOffset, in->offsetOrSort,
						       in->maxCount);
	}

	case NP_MGR_CMD_GET_CATALOG_LIST: {
		const NpManagerListReqIn *in =
			static_cast<const NpManagerListReqIn *>(inBufs[0].pData);
		return npManagerEnqueueGetCatalogListJob(session, ipcReqId, in->clientReqId,
							 in->categoryOrOffset, in->offsetOrSort,
							 in->maxCount);
	}

	case NP_MGR_CMD_GET_AVATAR_BATCH: {
		const NpManagerAvatarBatchReqIn *in =
			static_cast<const NpManagerAvatarBatchReqIn *>(inBufs[0].pData);
		return npManagerEnqueueGetAvatarBatchJob(session, ipcReqId, in->clientReqId,
							 inBufs[1].pData, in->ticketSize,
							 static_cast<const int *>(inBufs[2].pData),
							 in->startOffset, in->sortMode,
							 in->maxCount, in->skipExtraIds);
	}

	case NP_MGR_CMD_GET_AVATAR_URL: {
		const NpManagerAvatarUrlReqIn *in =
			static_cast<const NpManagerAvatarUrlReqIn *>(inBufs[0].pData);
		return npManagerEnqueueGetAvatarUrlJob(session, ipcReqId, in->clientReqId,
						       inBufs[1].pData, in->ticketSize,
						       in->avatarId, in->sizeType);
	}

	case NP_MGR_CMD_GET_TICKET_AVATAR_BATCH: {
		const NpManagerTicketAvatarBatchReqIn *in =
			static_cast<const NpManagerTicketAvatarBatchReqIn *>(inBufs[0].pData);
		return npManagerEnqueueGetTicketAvatarBatchJob(
			session, ipcReqId, in->clientReqId, in->authReqId, in->ticketSize,
			static_cast<const int *>(inBufs[1].pData), in->startOffset,
			in->sortMode, in->maxCount, in->skipExtraIds);
	}

	case NP_MGR_CMD_GET_TICKET_AVATAR_URL: {
		const NpManagerTicketAvatarUrlReqIn *in =
			static_cast<const NpManagerTicketAvatarUrlReqIn *>(inBufs[0].pData);
		return npManagerEnqueueGetTicketAvatarUrlJob(session, ipcReqId, in->clientReqId,
							     in->authReqId, in->ticketSize,
							     in->avatarId, in->sizeType);
	}

	case NP_MGR_CMD_OAUTH_GET_ACCESS_TOKEN:
		return npManagerEnqueueOAuthGetAccessTokenJob(
			session, ipcReqId, *static_cast<const int *>(inBufs[0].pData));

	case NP_MGR_CMD_OAUTH_GET_AUTH_CODE:
		return npManagerEnqueueOAuthGetAuthCodeJob(
			session, ipcReqId, *static_cast<const int *>(inBufs[0].pData),
			static_cast<const SceNpClientId *>(inBufs[1].pData),
			static_cast<const char *>(inBufs[2].pData));

	case NP_MGR_CMD_OAUTH_GET_AUTH_CODE_REDIRECT:
		return npManagerEnqueueOAuthGetAuthCodeWithRedirectJob(
			session, ipcReqId, *static_cast<const int *>(inBufs[0].pData),
			static_cast<const SceNpClientId *>(inBufs[1].pData),
			static_cast<const char *>(inBufs[2].pData),
			static_cast<const char *>(inBufs[3].pData));

	default:
		return IPMI_ERR_UNSUPPORTED_METHOD;
	}
}

static SceUInt8 parseHexNibble(char ch)
{
	if (ch >= 'a' && ch <= 'f')
		return static_cast<SceUInt8>(ch - 'a' + 10);
	if (ch >= 'A' && ch <= 'F')
		return static_cast<SceUInt8>(ch - 'A' + 10);
	return static_cast<SceUInt8>(ch - '0');
}

static SceUInt64 loadBigEndianBlock(const SceUInt8 *bytes)
{
	SceUInt64 word = 0;
	for (int i = 0; i < 8; ++i)
		word = (word << 8) | bytes[i];
	return word;
}

static void storeBigEndianBlock(SceUInt8 *bytes, SceUInt64 word)
{
	for (int i = 0; i < 8; ++i)
		bytes[i] = static_cast<SceUInt8>((word >> (56 - i * 8)) & 0xFFU);
}

static int desCbcEncryptSingleBlock(const SceUInt32 keyWords[2], const SceUInt8 iv[8],
				    const SceUInt8 inBlock[8], SceUInt8 outBlock[8])
{
	SceUInt32 subkeys[32];
	SceUInt8 workIv[8];
	SceUInt8 xoredBlock[8];

	int ret = shellDesSetKey(subkeys, 0, keyWords[0], keyWords[1]);
	if (ret < 0)
		return ret;

	sceClibMemcpy(workIv, iv, 8);
	for (int i = 0; i < 8; ++i)
		xoredBlock[i] = inBlock[i] ^ workIv[i];

	SceUInt64 packed = loadBigEndianBlock(xoredBlock);
	SceUInt64 cipher = shellDesEncryptBlock(subkeys, 0,
						static_cast<SceUInt32>(packed),
						static_cast<SceUInt32>(packed >> 32));
	storeBigEndianBlock(outBlock, cipher);
	return 0;
}

static int desCbcDecryptSingleBlock(const SceUInt32 keyWords[2], const SceUInt8 iv[8],
				    const SceUInt8 inBlock[8], SceUInt8 outBlock[8])
{
	SceUInt32 subkeys[32];
	SceUInt8 workIv[8];
	SceUInt8 decrypted[8];

	int ret = shellDesSetKey(subkeys, 0, keyWords[0], keyWords[1]);
	if (ret < 0)
		return ret;

	sceClibMemcpy(workIv, iv, 8);
	SceUInt64 packed = loadBigEndianBlock(inBlock);
	SceUInt64 plain = shellDesDecryptBlock(subkeys, 0,
					       static_cast<SceUInt32>(packed),
					       static_cast<SceUInt32>(packed >> 32));
	storeBigEndianBlock(decrypted, plain);
	for (int i = 0; i < 8; ++i)
		outBlock[i] = decrypted[i] ^ workIv[i];
	return 0;
}

/* 0x812214B4: build 32-byte Triple-DES-EDE-CBC encrypted device token */
static int npGenerateDeviceToken(void *outTokenBuf)
{
	SceUInt8 plaintext[32];
	SceRtcTick tick;
	SceUInt64 accountId = 0;
	SceUInt8 openPsId[16];
	char openPsIdHex[33];
	char zeroHex[33];
	SceUInt8 cbcIv[8];
	SceUInt8 *outBytes = static_cast<SceUInt8 *>(outTokenBuf);

	sceClibMemset(plaintext, 0, sizeof(plaintext));
	int ret = sceRtcGetCurrentTick(&tick);
	if (ret < 0)
		return ret;

	SceUInt64 unixTimeMs = (tick.tick - UNIX_EPOCH_RTC_TICKS) / TICKS_PER_MILLISECOND;
	SceUInt64 unixTimeBe = sceNetHtonll(unixTimeMs);
	sceClibMemcpy(&plaintext[0], &unixTimeBe, 8);

	if (npAccountReadRegistryCredentials(NULL, NULL, &accountId) < 0)
		accountId = 0;
	SceUInt64 accountIdBe = sceNetHtonll(accountId);
	sceClibMemcpy(&plaintext[8], &accountIdBe, 8);

	ret = sceKernelGetOpenPsId(openPsId);
	if (ret >= 0) {
		for (int i = 0; i < 16; ++i)
			sceClibSnprintf(&openPsIdHex[i * 2], 3, "%02x", openPsId[i]);
		sceClibSnprintf(zeroHex, sizeof(zeroHex), "%016llx%016llx", 0ULL, 0ULL);
	}
	if (ret < 0)
		return ret;

	for (int i = 0; i < 16; ++i) {
		SceUInt8 hi = parseHexNibble(openPsIdHex[i * 2]);
		SceUInt8 lo = parseHexNibble(openPsIdHex[i * 2 + 1]);
		plaintext[16 + i] = static_cast<SceUInt8>((hi << 4) | lo);
	}

	sceClibMemcpy(cbcIv, s_deviceTokenInitialVector, 8);
	for (SceSize offset = 0; offset < sizeof(plaintext); offset += 8) {
		SceUInt8 firstCipher[8];
		SceUInt8 middlePlain[8];

		if (desCbcEncryptSingleBlock(s_deviceTokenKeyFirst, cbcIv,
					     &plaintext[offset], firstCipher) >= 0) {
			if (desCbcDecryptSingleBlock(s_deviceTokenKeySecond, cbcIv,
						     firstCipher, middlePlain) >= 0) {
				desCbcEncryptSingleBlock(s_deviceTokenKeyThird, cbcIv,
							 middlePlain, &outBytes[offset]);
			}
		}
		sceClibMemcpy(cbcIv, &outBytes[offset], 8);
	}

	return ret;
}

/* 0x812218A2: compute SHA-256 digest of big-endian 64-bit NP account ID */
static int npGenerateAccountIdHash(void *outDigest)
{
	SceUInt64 accountId = 0;
	if (npAccountReadRegistryCredentials(NULL, NULL, &accountId) < 0)
		accountId = 0;
	SceUInt64 accountIdBe = sceNetHtonll(accountId);
	return shellSha256Digest(&accountIdBe, sizeof(accountIdBe), outDigest);
}

/* 0x812218F8: show formatted NP debug notice via impose_plugin when debug enabled */
void npDebugShowNotice(const char *tag, const char *detail, const char *noticeName)
{
	ImposeNotice notice;
	if (SceNpCommon_BBC13621() == 0)
		return;

	paf::Plugin *plugin = paf::Plugin::Find("impose_plugin");
	if (plugin == NULL)
		return;
	ImposePluginInterface *iface =
		static_cast<ImposePluginInterface *>(plugin->GetInterface(1));
	if (iface == NULL)
		return;

	wchar_t wideMsg[128];
	char narrowMsg[128];
	sceClibMemset(wideMsg, 0, sizeof(wideMsg));
	sceClibMemset(narrowMsg, 0, sizeof(narrowMsg));
	sceClibSnprintf(narrowMsg, sizeof(narrowMsg), "%s: %s", tag, detail);
	for (int i = 0; i < 127; ++i)
		wideMsg[i] = static_cast<wchar_t>(narrowMsg[i]);

	if (noticeName != NULL)
		notice.name = noticeName;
	notice.message = wideMsg;
	iface->showNotice(&notice);
}

/* 0x81221A2C: show raw NP debug notice via impose_plugin when debug enabled */
void npDebugShowRawNotice(const char *message, const char *noticeName)
{
	ImposeNotice notice;
	if (SceNpCommon_BBC13621() == 0)
		return;

	paf::Plugin *plugin = paf::Plugin::Find("impose_plugin");
	if (plugin == NULL)
		return;
	ImposePluginInterface *iface =
		static_cast<ImposePluginInterface *>(plugin->GetInterface(1));
	if (iface == NULL)
		return;

	wchar_t wideMsg[1024];
	sceClibMemset(wideMsg, 0, sizeof(wideMsg));
	for (int i = 0; i < 1023; ++i)
		wideMsg[i] = static_cast<wchar_t>(message[i]);

	if (noticeName != NULL)
		notice.name = noticeName;
	notice.message = wideMsg;
	iface->showNotice(&notice);
}

/* SceNpCommon IPC handlers (0x81221B34..0x81221BC4) */
static int npCommonHandleIsSignedIn(IPMI::Session *session, int *outSignedIn)
{
	(void)session;
	return npCommonServiceIsSignedIn(outSignedIn);
}

static int npCommonHandleCompareAccountId(IPMI::Session *session, SceUInt64 accountId)
{
	(void)session;
	return npCommonServiceCompareAccountId(accountId);
}

static int npCommonHandleGetSettings(IPMI::Session *session, void *outSettings,
				     char *outEnv, SceSize envBufSize, SceSize *outEnvSize)
{
	(void)session;
	*outEnvSize = 0;
	int ret = npCommonServiceGetSettings(outSettings);
	if (ret >= 0 && envBufSize >= 17) {
		ret = sceRegMgrGetKeyStrForUser("/CONFIG/NP", "env", outEnv, envBufSize);
		if (ret >= 0)
			*outEnvSize = 17;
	}
	return ret;
}

static int npCommonHandleGetPrivacyConfirm(IPMI::Session *session, int *outConfirmed)
{
	(void)session;
	return npCommonServiceGetPrivacyConfirm(outConfirmed);
}

static int npCommonHandleSetPrivacyConfirm(IPMI::Session *session, int confirmed)
{
	(void)session;
	return npCommonServiceSetPrivacyConfirm(confirmed);
}

static int npCommonHandleShowNotice(IPMI::Session *session, const char *tag,
				    const char *detail, const char *noticeName)
{
	(void)session;
	npDebugShowNotice(tag, detail, noticeName);
	return 0;
}

static int npCommonHandleShowRawNotice(IPMI::Session *session, const char *message,
				       const char *noticeName)
{
	(void)session;
	npDebugShowRawNotice(message, noticeName);
	return 0;
}

static int npCommonHandleGetDeviceToken(IPMI::Session *session, void *outToken)
{
	(void)session;
	npGenerateDeviceToken(outToken);
	return 0;
}

static int npCommonHandleGetAccountHash(IPMI::Session *session, void *outDigest)
{
	(void)session;
	npGenerateAccountIdHash(outDigest);
	return 0;
}

/* 0x81221BD0: ShellSvc 0x0A sync dispatcher */
int shellSvcDisp_0A_Sync(IPMI::Session *session, SceUInt32 methodId,
			 const IPMI::DataInfo *inBufs, SceUInt32 inCount,
			 IPMI::BufferInfo *outBufs, SceUInt32 outCount)
{
	return npCommonSyncDispatch(session, methodId, inBufs, inCount, outBufs, outCount);
}

/* 0x81221BE4: ShellSvc 0x0A async dispatcher */
int shellSvcDisp_0A_Async(void)
{
	return ipmiAsyncNotSupported();
}

/* 0x81221BF6: SceNpCommon synchronous method dispatcher */
static int npCommonSyncDispatch(IPMI::Session *session, SceUInt32 methodId,
				const IPMI::DataInfo *inBufs, SceUInt32 inCount,
				IPMI::BufferInfo *outBufs, SceUInt32 outCount)
{
	(void)inCount;
	int ret;

	switch (methodId) {
	case NP_COMMON_CMD_IS_SIGNED_IN:
		ret = npCommonHandleIsSignedIn(session, static_cast<int *>(outBufs[0].pBuffer));
		outBufs[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_COMMON_CMD_COMPARE_ACCOUNT_ID:
		ret = npCommonHandleCompareAccountId(
			session, *static_cast<const SceUInt64 *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_COMMON_CMD_GET_SETTINGS: {
		SceSize *outEnvSize = static_cast<SceSize *>(outBufs[2].pBuffer);
		SceSize envBufSize = *static_cast<const SceSize *>(inBufs[0].pData);
		ret = npCommonHandleGetSettings(session, outBufs[0].pBuffer,
						static_cast<char *>(outBufs[1].pBuffer),
						envBufSize, outEnvSize);
		outBufs[0].outSize = 60;
		outBufs[1].outSize = *outEnvSize;
		outBufs[2].outSize = sizeof(SceSize);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);
	}

	case NP_COMMON_CMD_GET_PRIVACY_CONFIRM:
		ret = npCommonHandleGetPrivacyConfirm(session,
						      static_cast<int *>(outBufs[0].pBuffer));
		outBufs[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_COMMON_CMD_SET_PRIVACY_CONFIRM:
		ret = npCommonHandleSetPrivacyConfirm(
			session, *static_cast<const int *>(inBufs[0].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_COMMON_CMD_GET_DEVICE_TOKEN:
		ret = npCommonHandleGetDeviceToken(session, outBufs[0].pBuffer);
		outBufs[0].outSize = 32;
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_COMMON_CMD_GET_ACCOUNT_HASH:
		ret = npCommonHandleGetAccountHash(session, outBufs[0].pBuffer);
		outBufs[0].outSize = 32;
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_COMMON_CMD_SHOW_DEBUG_NOTICE:
		ret = npCommonHandleShowNotice(session,
					       static_cast<const char *>(inBufs[0].pData),
					       static_cast<const char *>(inBufs[1].pData),
					       static_cast<const char *>(inBufs[2].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	case NP_COMMON_CMD_SHOW_RAW_NOTICE:
		ret = npCommonHandleShowRawNotice(session,
						  static_cast<const char *>(inBufs[0].pData),
						  static_cast<const char *>(inBufs[1].pData));
		return session->respondToSyncMethodRequest(ret, outBufs, outCount);

	default:
		return IPMI_ERR_UNSUPPORTED_METHOD;
	}
}
