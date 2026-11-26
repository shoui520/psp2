/*
 * np_imps_core.h: SceNpImpsCore, SceNpImpsSession, SceNpImpsContext,
 * SceNpImpsCtl, and SceNpImpsNotification shared interfaces
 * (0x813176AC..0x8131F65B).
 */
#ifndef SHELL_NP_IMPS_CORE_H
#define SHELL_NP_IMPS_CORE_H

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <np/np_common.h>
#include <rtc.h>
#include "../polling/np_polling.h"
#include "../rt/np_rt_core.h"
#include "np_imps_svc.h"

/* SceNpImpsCore error codes (0x80551400..0x80551410) */
#define NP_IMPS_CORE_ERR_INTERNAL		-2141907968	/* 0x80551400 */
#define NP_IMPS_CORE_ERR_ALREADY_INIT		-2141907967	/* 0x80551401 */
#define NP_IMPS_CORE_ERR_NOT_INITIALIZED	-2141907966	/* 0x80551402 */
#define NP_IMPS_CORE_ERR_INVALID_ARG		-2141907965	/* 0x80551403 */
#define NP_IMPS_CORE_ERR_NO_MEMORY		-2141907964	/* 0x80551404 */
#define NP_IMPS_CORE_ERR_PIPE_TIMEOUT		-2141907963	/* 0x80551405 */
#define NP_IMPS_CORE_ERR_ABORTED		-2141907962	/* 0x80551406 */
#define NP_IMPS_CORE_ERR_CB_NOT_FOUND		-2141907961	/* 0x80551407 */
#define NP_IMPS_CORE_ERR_CB_FULL		-2141907960	/* 0x80551408 */
#define NP_IMPS_CORE_ERR_SESSION_EXISTS		-2141907959	/* 0x80551409 */
#define NP_IMPS_CORE_ERR_SESSION_NOT_FOUND	-2141907958	/* 0x8055140A */
#define NP_IMPS_CORE_ERR_BAD_STATE		-2141907957	/* 0x8055140B */
#define NP_IMPS_CORE_ERR_STOPPED		-2141907956	/* 0x8055140C */
#define NP_IMPS_CORE_ERR_INVALID_MODE		-2141907955	/* 0x8055140D */
#define NP_IMPS_CORE_ERR_PROTOCOL		-2141907954	/* 0x8055140E */
#define NP_IMPS_CORE_ERR_NO_SESSION_SID		-2141907953	/* 0x8055140F */
#define NP_IMPS_CORE_ERR_CONFIG_TIMEOUT		-2141907952	/* 0x80551410 */

/* SceNpImpsCtl error codes (0x80551500..0x80551517) */
#define NP_IMPS_CTL_ERR_INTERNAL		-2141907712	/* 0x80551500 */
#define NP_IMPS_CTL_ERR_ALREADY_INIT		-2141907711	/* 0x80551501 */
#define NP_IMPS_CTL_ERR_INVALID_ARG		-2141907709	/* 0x80551503 */
#define NP_IMPS_CTL_ERR_PIPE_TIMEOUT		-2141907707	/* 0x80551505 */
#define NP_IMPS_CTL_ERR_ABORTED			-2141907706	/* 0x80551506 */
#define NP_IMPS_CTL_ERR_CTX_NOT_FOUND		-2141907702	/* 0x8055150A */
#define NP_IMPS_CTL_ERR_BAD_STATE		-2141907701	/* 0x8055150B */
#define NP_IMPS_CTL_ERR_ALREADY_STARTED		-2141907700	/* 0x8055150C */
#define NP_IMPS_CTL_ERR_NOT_STARTED		-2141907699	/* 0x8055150D */
#define NP_IMPS_CTL_ERR_SESSION_FAILED		-2141907697	/* 0x8055150F */
#define NP_IMPS_CTL_ERR_CLIENT_LIMIT		-2141907695	/* 0x80551511 */
#define NP_IMPS_CTL_ERR_DISCONNECTED		-2141907694	/* 0x80551512 */
#define NP_IMPS_CTL_ERR_CANCELED		-2141907693	/* 0x80551513 */
#define NP_IMPS_CTL_ERR_NOT_READY		-2141907692	/* 0x80551514 */
#define NP_IMPS_CTL_ERR_NO_SLOT			-2141907691	/* 0x80551515 */
#define NP_IMPS_CTL_ERR_CLOSED			-2141907690	/* 0x80551516 */
#define NP_IMPS_CTL_ERR_SIGNED_OUT		-2141907689	/* 0x80551517 */

/* SceNpImpsNotification error codes (0x80552101..0x80552104) */
#define NP_IMPS_NOTIF_ERR_NOT_INITIALIZED	-2141904639	/* 0x80552101 */
#define NP_IMPS_NOTIF_ERR_ALREADY_INIT		-2141904638	/* 0x80552102 */
#define NP_IMPS_NOTIF_ERR_NO_CALLBACK		-2141904637	/* 0x80552103 */
#define NP_IMPS_NOTIF_ERR_CALLBACK_SET		-2141904636	/* 0x80552104 */

/* Account / auth errors that trigger clearing the cached NP account session */
#define NP_ACCOUNT_ERR_AUTH_EXPIRED		-2141908733	/* 0x80551103 */
#define NP_ACCOUNT_ERR_TOKEN_REJECTED		-2141908706	/* 0x8055111E */
#define NP_ACCOUNT_ERR_TICKET_INVALID		-2141904373	/* 0x8055220B */

/* SceNpImpsCore desired connection modes */
enum NpImpsSessionMode {
	NP_IMPS_MODE_NONE		= 0,
	NP_IMPS_MODE_DISCONNECT		= 1,
	NP_IMPS_MODE_CRON_POLL		= 2,
	NP_IMPS_MODE_INTERVAL_POLL	= 3,
	NP_IMPS_MODE_ONLINE		= 4
};

/* SceNpImpsCore session state events reported to registered listeners */
enum NpImpsSessionEventState {
	NP_IMPS_EVT_CONNECTING		= 0,
	NP_IMPS_EVT_CONNECTED		= 1,
	NP_IMPS_EVT_ENTERING_ONLINE	= 2,
	NP_IMPS_EVT_ENTERING_INTERVAL	= 3,
	NP_IMPS_EVT_ENTERING_CRON	= 4,
	NP_IMPS_EVT_DISCONNECTING	= 5,
	NP_IMPS_EVT_ONLINE		= 6,
	NP_IMPS_EVT_POLL_INTERVAL	= 7,
	NP_IMPS_EVT_POLL_CRON		= 8,
	NP_IMPS_EVT_SUSPENDED		= 9,
	NP_IMPS_EVT_STOPPING		= 10,
	NP_IMPS_EVT_STOPPED		= 11
};

/* Public session identity block (40 bytes) */
struct NpImpsSessionIdentity {
	int sessionId;				/* +0x00 */
	SceNpId npId;				/* +0x04 (36 bytes) */
};

/* Notification extension payload ("http://playstation.net/np/rt/notification") */
struct NpImpsCoreNotificationExt {
	int kind;				/* +0x00 (0 = text notification) */
	const char *text;			/* +0x04 */
};

/* Parsed / outgoing IMPS message structure (44 bytes = 0x2C, 0x813F4782 / 0x813F49AC) */
struct NpImpsCoreMessage {
	int type;				/* +0x00: 0=normal,1=chat,2=groupchat,3=headline,4=error */
	char *subject;				/* +0x04 */
	char *body;				/* +0x08 */
	char *thread;				/* +0x0C */
	NpImpsCoreDataNode *extData;		/* +0x10: http://playstation.net/np/rt/ext */
	void *messageExt;			/* +0x14: http://playstation.net/np/rt/message */
	void *storedExt;			/* +0x18: http://playstation.net/np/rt/stored */
	void *contentExt;			/* +0x1C: http://playstation.net/np/rt/content */
	NpImpsCoreNotificationExt *notificationExt; /* +0x20: http://playstation.net/np/rt/notification */
	SceUInt8 storeOffline;			/* +0x24 */
};

/* Internal 3-list transaction queue shared by SceNpImpsSession and SceNpImpsCtl (32 bytes) */
struct NpImpsTxNodeHeader {
	struct NpImpsTxQueue *owner;		/* +0x00 */
	int listState;				/* +0x04: 0=none, 1=pending, 2=inFlight, 3=completed */
	NpImpsTxNodeHeader *next;		/* +0x08 */
	NpImpsTxNodeHeader *prev;		/* +0x0C */
	int requestId;				/* +0x10 */
};

struct NpImpsTxQueue {
	SceKernelLwMutexWork *mutex;		/* +0x00 */
	int totalCount;				/* +0x04 */
	NpImpsTxNodeHeader *pendingHead;	/* +0x08 */
	NpImpsTxNodeHeader *pendingTail;	/* +0x0C */
	NpImpsTxNodeHeader *inFlightHead;	/* +0x10 */
	NpImpsTxNodeHeader *inFlightTail;	/* +0x14 */
	NpImpsTxNodeHeader *completedHead;	/* +0x18 */
	NpImpsTxNodeHeader *completedTail;	/* +0x1C */
};

/* Callback signatures */
typedef void (*NpImpsSessionSendDoneCallback)(const NpImpsSessionIdentity *session,
					      int transactionId, int result, void *userArg);

typedef void (*NpImpsCoreConnectCallback)(SceUID processId, int errorCode);
typedef void (*NpImpsCoreSendMsgCallback)(const NpImpsSessionIdentity *session,
					  int transactionId, int result, void *userArg);

typedef void (*NpImpsNotifOnlineCallback)(int stateCode);
typedef void (*NpImpsNotifMessageCallback)(const SceNpId *senderNpId, const char *text);

/* Internal SceNpImpsCore session listener callbacks (24 bytes, 0x8131B246) */
struct NpImpsCoreSessionCallbacks {
	void (*onSessionState)(const NpImpsSessionIdentity *session, int eventState,
			       int errorCode);
	void (*onRosterUpdate)(const NpImpsSessionIdentity *session, int isPush,
			       NpBasicRosterList *rosterList);
	void (*onPrivacyUpdate)(const NpImpsSessionIdentity *session, int action,
				NpBasicPrivacyList *privacyList);
	void (*onMessageStanza)(const NpImpsSessionIdentity *session, NpRtMessageStanza *stanza);
	void (*onPresenceStanza)(const NpImpsSessionIdentity *session,
				 NpRtPresenceStanza *stanza);
	void (*onPollingUpdate)(const NpImpsSessionIdentity *session, int updateKind,
				const SceRtcTick *timestamp);
};

/* Public SceNpImpsCtl listener callbacks (28 bytes = 7 slots, 0x8131E5DA) */
struct NpImpsCoreCallbacks {
	void (*onCtlEvent)(int eventType);
	void (*onSessionState)(const NpImpsSessionIdentity *session, int sessionState,
			       int errorCode);
	void (*onRosterUpdate)(const NpImpsSessionIdentity *session, int action,
			       const NpBasicRosterList *rosterList);
	void (*onPrivacyUpdate)(const NpImpsSessionIdentity *session, int action,
				const NpBasicPrivacyList *privacyList);
	void (*onPresenceUpdate)(const NpImpsSessionIdentity *session, const SceNpId *npId,
				 const NpImpsCorePresenceUpdate *update);
	void (*onMessageRecv)(const NpImpsSessionIdentity *session, const SceNpId *senderNpId,
			      const NpImpsCoreMessage *msg);
	void (*onPollingNotify)(const NpImpsSessionIdentity *session, int notifyType,
				const SceRtcTick *timestamp);
};

/* SceNpImpsCore lifecycle & listener registry (np_imps_core_session.cpp) */
int npImpsCoreInit(const NpPollingThreadParam *threadParam);
int npImpsCoreTerm(void);
int npImpsCoreRegisterSessionCallbacks(const NpImpsCoreSessionCallbacks *callbacks,
				       int *outCallbackId);
int npImpsCoreUnregisterSessionCallbacks(int callbackId);

/* Shared 3-list transaction queue helpers (np_imps_core_session.cpp) */
void npImpsTxQueueInit(NpImpsTxQueue *queue, SceKernelLwMutexWork *mutex);
void npImpsTxQueueUnlink(NpImpsTxQueue *queue, NpImpsTxNodeHeader *node);
int npImpsTxQueueFindPending(NpImpsTxQueue *queue, int requestId, NpImpsTxNodeHeader **outNode);
int npImpsTxQueuePopCompleted(NpImpsTxQueue *queue, NpImpsTxNodeHeader **outNode);

/* SceNpImpsCtl lifecycle & public API (np_imps_core_ctl.cpp) */
int npImpsCtlInit(const NpPollingThreadParam *threadParam);
int npImpsCtlTerm(void);
int npImpsCtlStartDefault(void);
int npImpsCtlIsStarted(void);
int npImpsCoreConnectProcess(SceUID processId, NpImpsCoreConnectCallback callback);
int npImpsCoreDisconnectProcess(SceUID processId);
int npImpsCoreResyncSession(void);
int npImpsCoreUpdatePresence(const NpImpsCorePresenceUpdate *update);
int npImpsCoreSendInGameMessage(const SceNpId *targetNpId, const NpImpsCoreMessage *msg,
				NpImpsCoreSendMsgCallback callback, int *outTransactionId);
int npImpsCoreRegisterCallbacks(const NpImpsCoreCallbacks *callbacks, int *outCallbackId);
int npImpsCoreUnregisterCallbacks(int callbackId);

/* SceNpImpsNotification API (np_imps_core_notif.cpp) */
int npImpsNotifInit(void);
int npImpsNotifTerm(void);
int npImpsNotifRegisterOnlineCallback(NpImpsNotifOnlineCallback callback);
int npImpsNotifUnregisterOnlineCallback(void);

/* External SceNpRt IMPS stanza & session facade (0x813F3FE8..0x813F5193) */
extern int npRtImpsSendPresence(int rtSessionHandle, int transactionId,
				NpRtPresenceStanza *stanza,
				NpRtSendDoneCallback callback);				/* 0x813F3FE8 */
extern int npRtImpsSendOfflinePresence(int rtSessionHandle);				/* 0x813F4012 */
extern int npRtImpsSendStandbyIq(int rtSessionHandle, int transactionId,
				 int standbyMode, NpRtSendDoneCallback callback);	/* 0x813F409A */
extern int npRtImpsBuildPresenceStanza(const NpImpsCorePresenceUpdate *update,
				       NpRtPresenceStanza **outStanza);			/* 0x813F4270 */
extern int npRtImpsParsePresenceStanza(NpImpsCorePresenceUpdate **outUpdate,
				       const NpRtPresenceStanza *stanza,
				       void *customAlloc);				/* 0x813F4324 */
extern void npRtImpsFreePresenceUpdate(NpImpsCorePresenceUpdate *update,
				       void *customAlloc);				/* 0x813F44C4 */
extern int npRtImpsClonePresenceUpdate(NpImpsCorePresenceUpdate **outCopy,
				       const NpImpsCorePresenceUpdate *src);		/* 0x813F451E */
extern int npRtImpsBuildMessageStanza(const SceNpId *targetNpId,
				      const NpImpsCoreMessage *msg,
				      NpRtMessageStanza **outStanza);			/* 0x813F4618 */
extern int npRtImpsParseMessageStanza(NpImpsCoreMessage **outMsg,
				      const NpRtMessageStanza *stanza,
				      void *customAlloc);				/* 0x813F4782 */
extern void npRtImpsFreeMessage(NpImpsCoreMessage *msg, void *customAlloc);		/* 0x813F492C */
extern int npRtImpsCloneMessage(NpImpsCoreMessage **outCopy,
				const NpImpsCoreMessage *src);				/* 0x813F49AC */
extern int npRtImpsSendMessage(int rtSessionHandle, int transactionId,
			       NpRtMessageStanza *stanza,
			       NpRtSendDoneCallback callback);				/* 0x813F4B0C */
extern int npRtImpsFetchStored(int rtSessionHandle, int transactionId,
			       NpRtSendDoneCallback callback);				/* 0x813F4B38 */
extern int npRtImpsDeleteStored(int rtSessionHandle, int transactionId,
				SceUInt64 storedId, NpRtSendDoneCallback callback);	/* 0x813F4C5A */

extern int npImpsCoreCreateSession(int *outSessionId);					/* 0x813F4D74 */
extern int npImpsCoreDestroySession(int sessionId, int reasonError);			/* 0x813F4DC6 */
extern int npImpsCoreStartSession(int sessionId, int mode,
				  NpRtPresenceStanza *initialPresence);			/* 0x813F4E28 */
extern int npImpsCoreSetSessionMode(int sessionId, int mode,
				    NpRtPresenceStanza *presence);			/* 0x813F4ED6 */
extern int npImpsCoreGetSessionAutoInterval(int sessionId, int *outIntervalSec);	/* 0x813F4F88 */
extern int npImpsCoreAllocTransactionId(int *outTransactionId);				/* 0x813F4FD4 */
extern int npImpsCoreSendPresence(int sessionId, int transactionId,
				  NpRtPresenceStanza *stanza,
				  NpImpsSessionSendDoneCallback callback,
				  void *userArg);					/* 0x813F5018 */
extern int npImpsCoreSendMessage(int sessionId, int transactionId,
				 NpRtMessageStanza *stanza,
				 NpImpsSessionSendDoneCallback callback,
				 void *userArg);					/* 0x813F50D6 */

/* External NP account & shell callback bridges */
extern int npAccountCreateRequest(void);						/* 0x813268BC */
extern int npAccountDestroyRequest(int reqHandle);					/* 0x813268DC */
extern int npAccountAbortRequest(int reqHandle);					/* 0x813269E2 */
extern int npAccountGetNpIdForce(int reqHandle, SceNpId *outNpId);			/* 0x81326A92 */
extern int npAccountClearSession(void);							/* 0x81326CDC */
extern int npAccountRegisterNetCallback(void (*callback)(int eventType, void *userArg),
					void *userArg);					/* 0x8132773E */
extern int npAccountUnregisterNetCallback(int callbackId);				/* 0x813277B8 */

extern "C" {
int sceNpCommonGetNpEnviroment(char *outEnv);
int SceNpCommon_9B7CAA67(SceNpId *outNpId, const NpJid *jid);
int SceNetInternal_9DBD2F60(SceUID threadId, int enable, int reserved);
}

#endif /* SHELL_NP_IMPS_CORE_H */
