/*
 * np_polling.h: SceNpPolling UDP polling & status-check service
 * (0x8130C7E8..0x8130F3E3, 0x813F3DBC..0x813F3FFF).
 */
#ifndef SHELL_NP_POLLING_H
#define SHELL_NP_POLLING_H

#include <stddef.h>
#include <scetypes.h>
#include <np/np_common.h>
#include <rtc.h>

/* SceNpPolling error codes */
#define NP_POLLING_ERR_ALREADY_INIT		-2141913087	/* 0x80550001 */
#define NP_POLLING_ERR_NOT_INITIALIZED		-2141904895	/* 0x80552001 */
#define NP_POLLING_ERR_ALREADY_STARTED		-2141904894	/* 0x80552002 */
#define NP_POLLING_ERR_INVALID_ARG		-2141904893	/* 0x80552003 */
#define NP_POLLING_ERR_NO_MEMORY		-2141904892	/* 0x80552004 */
#define NP_POLLING_ERR_ABORTED			-2141904891	/* 0x80552005 */
#define NP_POLLING_ERR_UDP_TIMEOUT		-2141904887	/* 0x80552009 */
#define NP_POLLING_ERR_CONNECT_TIMEOUT		-2141904886	/* 0x8055200A */
#define NP_POLLING_ERR_MAINTENANCE		-2141904885	/* 0x8055200B */
#define NP_POLLING_ERR_SERVICE_END		-2141904884	/* 0x8055200C */
#define NP_POLLING_ERR_SERVICE_BEFORE		-2141904883	/* 0x8055200D */
#define NP_POLLING_ERR_HTTP_STATUS		-2141904882	/* 0x8055200E */
#define NP_POLLING_ERR_SERVER_BUSY		-2141904881	/* 0x8055200F */

#define NP_POLLING_ERR_INTERNAL			-2141876224	/* 0x80559000 */
#define NP_POLLING_ERR_PIPE_TIMEOUT		-2141876223	/* 0x80559001 */
#define NP_POLLING_ERR_HDR_TOO_SHORT		-2141876222	/* 0x80559002 */
#define NP_POLLING_ERR_PKT_TOO_SHORT		-2141876220	/* 0x80559004 */
#define NP_POLLING_ERR_BAD_VERSION		-2141876219	/* 0x80559005 */
#define NP_POLLING_ERR_BAD_HEADER		-2141876218	/* 0x80559006 */
#define NP_POLLING_ERR_BAD_PKT_KIND		-2141876217	/* 0x80559007 */
#define NP_POLLING_ERR_TLV_TOO_SHORT		-2141876216	/* 0x80559008 */
#define NP_POLLING_ERR_BUF_TOO_SMALL		-2141876215	/* 0x80559009 */
#define NP_POLLING_ERR_TLV_MISSING		-2141876214	/* 0x8055900A */
#define NP_POLLING_ERR_CB_NOT_FOUND		-2141876213	/* 0x8055900B */
#define NP_POLLING_ERR_CB_FULL			-2141876212	/* 0x8055900C */
#define NP_POLLING_ERR_SESSION_INVALID		-2141876211	/* 0x8055900D */
#define NP_POLLING_ERR_INVALID_STATE		-2141876210	/* 0x8055900E */
#define NP_POLLING_ERR_ALREADY_OPEN		-2141876209	/* 0x8055900F */
#define NP_POLLING_ERR_NOT_OPEN			-2141876208	/* 0x80559010 */
#define NP_POLLING_ERR_XML_INVALID		-2141876207	/* 0x80559011 */
#define NP_POLLING_ERR_BAD_SIGNATURE		-2141876206	/* 0x80559012 */

#define NP_POLLING_ERR_NOT_CONNECTED		-2141908437	/* 0x8055122B */
#define NP_POLLING_ERR_BAD_TRANSITION		-2141908447	/* 0x80551221 */

/* Server status values returned in UDP initial status check (0x8130DB4A) */
enum NpPollingServerStatus {
	NP_POLLING_SERVER_UNCHECKED	= -1,
	NP_POLLING_SERVER_OK		= 0,
	NP_POLLING_SERVER_STATUS_EXPIRED = 1,
	NP_POLLING_SERVER_STATUS_DENIED	= 2,
	NP_POLLING_SERVER_STATUS_UNAVAIL = 3
};

/* Polling cadence mode */
enum NpPollingMode {
	NP_POLLING_MODE_NONE		= 0,
	NP_POLLING_MODE_INTERVAL	= 1,	/* poll on msgpipe timeout (intervalMs) */
	NP_POLLING_MODE_CRON		= 2	/* poll on ShellCron wakeup (autoIntervalMs) */
};

/* Session worker states (0x8154615C) */
enum NpPollingState {
	NP_POLLING_STATE_ERROR		= 0,
	NP_POLLING_STATE_IDLE		= 1,
	NP_POLLING_STATE_CONNECTING	= 2,
	NP_POLLING_STATE_RUNNING	= 3,
	NP_POLLING_STATE_CLOSING	= 4,
	NP_POLLING_STATE_STOPPED	= 5
};

/* State event codes dispatched to registered state listeners (0x8130E33C) */
enum NpPollingEventState {
	NP_POLLING_EVENT_ERROR		= 0,
	NP_POLLING_EVENT_CONNECTING	= 1,
	NP_POLLING_EVENT_CONNECTED	= 2,
	NP_POLLING_EVENT_CLOSING	= 3,
	NP_POLLING_EVENT_STOPPED	= 4
};

/* SceNpPollingMain thread init parameters (12 bytes) */
struct NpPollingThreadParam {
	int priority;			/* +0x00 */
	SceSize stackSize;		/* +0x04 */
	int cpuAffinityMask;		/* +0x08 */
};

/* Server endpoint passed to session create (66 bytes = 0x42) */
struct NpPollingEndpoint {
	char host[64];			/* +0x00 */
	SceUInt16 port;			/* +0x40 */
};

/* Interval configuration parsed from communication-polling-status.xml (12 bytes) */
struct NpPollingStatusConfig {
	SceUInt32 intervalMs;		/* +0x00: <interval type="3"> in milliseconds */
	SceUInt32 autoIntervalMs;	/* +0x04: <interval type="4"> in milliseconds */
	SceInt32 autoIntervalPolicy;	/* +0x08: <auto-interval> (-1 when disabled) */
};

/* Server notification timestamps returned by UDP polling (24 bytes) */
struct NpPollingTimestampSet {
	SceRtcTick messageTick;		/* +0x00: TLV 10 */
	SceRtcTick friendListTick;	/* +0x08: TLV 11 */
	SceRtcTick blockListTick;	/* +0x10: TLV 12 */
};

/* Session identity passed to callbacks (84 bytes = 0x54, at session +0x30) */
struct NpPollingSessionIdentity {
	int sessionId;			/* +0x00 */
	SceNpId npId;			/* +0x04 (36 bytes) */
	SceUInt8 padTail[44];		/* +0x28 */
};

typedef void (*NpPollingStateCallback)(const NpPollingSessionIdentity *identity,
				       int eventState, int resultCode);
typedef void (*NpPollingUpdateCallback)(const NpPollingSessionIdentity *identity,
					int updateKind, const NpPollingTimestampSet *timestamps);

/* Registered global callback table (8 bytes) */
struct NpPollingCallbackTable {
	NpPollingStateCallback onStateChanged;	/* +0x00 */
	NpPollingUpdateCallback onUpdateReceived; /* +0x04 */
};

/* Module lifecycle & callback registry (0x8130C7E8..0x8130CB37) */
int npPollingInit(const NpPollingThreadParam *threadParam);
int npPollingTerm(void);
int npPollingRegisterCallbacks(const NpPollingCallbackTable *callbacks, int *outCallbackId);
int npPollingUnregisterCallbacks(int callbackId);

/* Public session API facade (0x813F3DBC..0x813F3FFF) */
int npPollingCreateSession(const SceNpId *npId, const NpPollingEndpoint *endpoint,
			   const NpPollingTimestampSet *initialTicks,
			   const NpPollingStatusConfig *initialConfig,
			   int *outSessionId);
int npPollingDestroySession(int sessionId);
int npPollingStartSession(int sessionId, int checkInitialStatus, int pollingMode,
			  const char *resourceName);
int npPollingGetStatusConfig(int sessionId, NpPollingStatusConfig *outConfig);
int npPollingGetServerStatus(int sessionId, int *outServerStatus);
int npPollingUpdateMode(int sessionId, int pollingMode, const char *resourceName);

/* SceNpCommon callout timer imports (from vsh/app/common/src/np/np_common/callout.c) */
struct NpPollingCalloutNode {
	NpPollingCalloutNode *next;	/* +0x00 */
	void (*func)(void *arg);	/* +0x04 */
	SceUInt64 time;			/* +0x08 */
	void *arg;			/* +0x10 */
	SceUInt32 padWord;		/* +0x14 */
};

/* Callout timeout wrapper (40 bytes, shared by SceNpPolling and SceNpCore) */
struct NpPollingTimeoutTimer {
	volatile int timerState;	/* +0x00: 0=idle, -1=armed, -2=firing, 1=fired */
	SceUInt32 padAlign;		/* +0x04 */
	NpPollingCalloutNode callout;	/* +0x08 (24 bytes) */
	void (*onTimeout)(void *arg);	/* +0x20 */
	void *timeoutArg;		/* +0x24 */
};

/* Base socket + resolver handle header shared by UDP polling and TCP/TLS XMPP (12 bytes) */
struct NpRtNetBase {
	int aborted;			/* +0x00 */
	int socketId;			/* +0x04 */
	int resolverId;			/* +0x08 */
};

/* Shared NP RT/Polling low-level synchronization, thread, net, and callout helpers */
int npRtDeleteMsgPipe(SceUID *pipeId);						/* 0x8130D172 */
int npRtLockLwMutex(SceKernelLwMutexWork *mutex);				/* 0x8130D320 */
int npRtUnlockLwMutex(SceKernelLwMutexWork *mutex, int lockState);		/* 0x8130D334 */
void npRtNetBaseInit(NpRtNetBase *net);						/* 0x8130D760 */
int npRtNetBaseIsOpen(const NpRtNetBase *net);					/* 0x8130D784 */
void npRtResolverFree(void *ptr, int rid, const char *name, void *user);	/* 0x8130DCB4 */
int npRtStartThread(SceUID *threadId, void *session);				/* 0x8130DD10 */
void npRtWaitThreadEnd(SceUID *threadId);					/* 0x8130DD46 */
int npRtStateInvalid(void *session, int *nextState);				/* 0x8130E750 */
void npRtCalloutInit(NpPollingTimeoutTimer *timer);				/* 0x8130F2B4 */
void npRtCalloutCancel(NpPollingTimeoutTimer *timer);				/* 0x8130F2C0 */
int npRtCalloutArm(NpPollingTimeoutTimer *timer, SceUInt64 delayUsec,
		   void (*onTimeout)(void *), void *timeoutArg);		/* 0x8130F320 */
int npRtCalloutIsFired(const NpPollingTimeoutTimer *timer);			/* 0x8130F3B8 */

extern "C" {
int SceNpCommon_540B307E(NpPollingCalloutNode *callout, SceUInt64 delay,
			 void (*func)(void *arg), void *arg);
int SceNpCommon_38B88BA3(NpPollingCalloutNode *callout, int *removed);
}

/* SceNpRt thread join helper (implemented in Batch F: 0x81313EA8) */
extern int npRtJoinAndDeleteThread(SceUID *threadId);

#endif /* SHELL_NP_POLLING_H */
