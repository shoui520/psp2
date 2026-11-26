/*
 * np_polling.cpp: SceNpPolling UDP polling client, HTTP status-XML verifier,
 * TLV codec, and worker state machine (0x8130C7E8..0x8130F3E3,
 * 0x813F3DBC..0x813F3FFF).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <sce_atomic.h>
#include <kernel.h>
#include <kernel/processmgr.h>
#include <libkernel/libkernel.h>
#include <libhttp.h>
#include <libhmac.h>
#include <net.h>
#include <net/inet.h>
#include <libnet/netinternal.h>
#include <np_common/np_common.h>
#include <common/main_thread_call_list.h>
#include "../../../shell_cron.h"
#include "../rt/np_rt_stanza.h"
#include "np_polling.h"

namespace {

static const SceUInt32 kMaxCallbacks = 4;
static const SceUInt32 kPipeBufferSize = 0x1C00;	/* 128 * 56 bytes */
static const SceUInt32 kPipeSendTimeoutUsec = 10000000;	/* 10s */
static const SceUInt64 kConnectTimeoutUsec = 60000000ULL; /* 60s */
static const SceInt64 kKeepAwakeWindowUsec = 120000000LL; /* 120s */
static const SceUInt64 kUnixToRtcEpochUsec = 62135596800000000ULL;
static const SceSize kUdpPacketMaxSize = 512;
static const SceSize kUdpHeaderSize = 32;
static const SceSize kResourceNameSize = 42;
static const SceSize kSignatureHeaderSize = 66;		/* "Sce-Np-Signature: " + 40 hex + CRLF */
static const int kHttpErrNoContentLength = -2143088527;	/* 0x80431071 */
static const int kNetErrWouldBlock = -2143223492;	/* 0x8041013C */
static const int kNetErrTimedOut = -2143223517;		/* 0x80410123 */
static const int kKernelErrPipeTimeout = -2147319803;	/* 0x80028005 */
static const int kKernelErrPipeEmpty = -2147319261;	/* 0x80028223 */
static const int kUdpStatusBusy = -2141904374;		/* 0x8055220A */
static const int kUdpStatusExpired = -2141904373;	/* 0x8055220B */
static const int kUdpStatusDenied = -2141904375;	/* 0x80552209 */
static const int kUdpStatusUnavail = -2141904376;	/* 0x80552208 */

enum NpPollingPipeCommandId {
	NP_POLLING_CMD_NONE		= -1,
	NP_POLLING_CMD_ABORT		= 0,
	NP_POLLING_CMD_START		= 1,
	NP_POLLING_CMD_STOP		= 2,
	NP_POLLING_CMD_CHANGE_MODE	= 3,
	NP_POLLING_CMD_CRON_TRIGGER	= 4
};

enum NpPollingSessionFlags {
	NP_POLLING_SFLAG_LINKED		= 0x01,
	NP_POLLING_SFLAG_CLOSING	= 0x10
};

/* 56-byte command message sent over SceNpPollingSession msgpipe */
struct NpPollingPipeCommand {
	int commandId;			/* +0x00 */
	int firstOption;		/* +0x04: checkInitialStatus (START) or pollingMode (CHANGE) */
	int secondOption;		/* +0x08: pollingMode (START) */
	char resourceName[42];		/* +0x0C */
	SceUInt8 padTail[2];		/* +0x36 */
};

/* HTTP status-XML fetch context (16 bytes, at session +0xFC) */
struct NpPollingHttpClient {
	int aborted;			/* +0x00 */
	int templateId;			/* +0x04 */
	int connectionId;		/* +0x08 */
	int requestId;			/* +0x0C */
};

/* UDP socket + resolver context (48 bytes, at session +0x110) */
struct NpPollingUdpClient {
	int aborted;			/* +0x00 */
	int socketId;			/* +0x04 */
	int resolverId;			/* +0x08 */
	SceNetSockaddrIn localAddr;	/* +0x0C (16 bytes) */
	SceNetSockaddrIn remoteAddr;	/* +0x1C (16 bytes) */
	SceUInt32 lastTxSeq;		/* +0x2C */
};

/* Callout timeout wrapper (40 bytes, at session +0x140) */
struct NpPollingTimeoutTimer {
	volatile int timerState;	/* +0x00: 0=idle, -1=armed, -2=firing, 1=fired */
	SceUInt32 padAlign;		/* +0x04 */
	NpPollingCalloutNode callout;	/* +0x08 (24 bytes) */
	void (*onTimeout)(void *arg);	/* +0x20 */
	void *timeoutArg;		/* +0x24 */
};

/* Global callback registry node (20 bytes) */
struct NpPollingCallbackNode {
	NpPollingCallbackNode *next;	/* +0x00 */
	NpPollingCallbackNode *prev;	/* +0x04 */
	int callbackId;			/* +0x08 */
	NpPollingCallbackTable table;	/* +0x0C */
};

/* UDP polling request payload (100 bytes = 0x64) */
struct NpPollingUdpRequest {
	SceNpId npId;			/* +0x00 (36 bytes) */
	char npEnv[17];			/* +0x24 */
	char resourceName[43];		/* +0x35 */
	void *pollExtra;		/* +0x60: optional NpPollingUdpPollExtra* */
};

/* Optional TLV 3 payload for regular polling packets (16 bytes) */
struct NpPollingUdpPollExtra {
	SceUInt8 pollKind;		/* +0x00: 3=interval, 4=cron */
	SceUInt8 padAfterKind[7];	/* +0x01 */
	SceUInt64 requestSeq;		/* +0x08 */
};

/* TLV specification entry (6 bytes, 0x8151C768) */
struct NpPollingTlvSpec {
	SceUInt16 minLen;
	SceUInt16 maxLen;
	SceUInt8 wireFlags;
	SceUInt8 padByte;
};

/* TLV decode rule entry (8 bytes, 0x8151C7C8) */
struct NpPollingTlvRule {
	SceUInt16 typeId;
	SceUInt16 padHalf;
	SceUInt32 required;
};

/* Status XML parser state (600 bytes, wraps NpXmlParser) */
struct NpPollingXmlParserContext {
	NpXmlParser xmlParser;		/* +0x000 (584 bytes) */
	int statusError;		/* +0x248 */
	SceUInt32 seenMask;		/* +0x24C */
	SceUInt32 currentIntervalType;	/* +0x250 */
	NpPollingStatusConfig *outConfig; /* +0x254 */
};

/* Main session object (440 bytes = 0x1B8, 0x8130CC0A) */
struct NpPollingSession {
	SceKernelLwMutexWork mutex;	/* +0x000 (32 bytes) */
	SceUInt32 sessionFlags;		/* +0x020 */
	int refCount;			/* +0x024 */
	NpPollingSession *next;		/* +0x028 */
	NpPollingSession *prev;		/* +0x02C */
	int sessionId;			/* +0x030 (identity starts here) */
	SceNpId npId;			/* +0x034 (36 bytes) */
	SceUInt8 padIdentity[44];	/* +0x058 */
	NpPollingEndpoint endpoint;	/* +0x084 (66 bytes: host[64], port) */
	SceUInt8 padEndpoint[2];	/* +0x0C6 */
	int state;			/* +0x0C8 */
	SceUID msgPipeId;		/* +0x0CC */
	char resourceName[42];		/* +0x0D0 */
	SceUInt8 padResource[2];	/* +0x0FA */
	NpPollingHttpClient httpClient;	/* +0x0FC (16 bytes) */
	NpPollingStatusConfig *statusConfig; /* +0x10C */
	NpPollingUdpClient udpClient;	/* +0x110 (48 bytes) */
	NpPollingTimeoutTimer timeoutTimer; /* +0x140 (40 bytes) */
	SceUID *workerThreadPtr;	/* +0x168 */
	int aborted;			/* +0x16C */
	int checkInitialStatus;		/* +0x170 */
	int serverStatus;		/* +0x174 */
	int pollingMode;		/* +0x178 */
	SceUInt32 pollTimeoutUsec;	/* +0x17C */
	SceUInt8 padBeforeSeq[8];	/* +0x180 */
	SceUInt32 pollRequestSeq;	/* +0x188 */
	SceUInt32 padBeforeTicks;	/* +0x18C */
	NpPollingTimestampSet lastTicks; /* +0x190 (24 bytes) */
	SceUID cronHandle;		/* +0x1A8 */
	SceUInt32 padBeforeAwake;	/* +0x1AC */
	SceInt64 keepAwakeStartTime;	/* +0x1B0 */
};

/* Global SceNpPolling state */
static SceKernelLwMutexWork s_pollingMutex;
static int s_pollingMutexInit = 0;

static int s_cbListInit = 0;
static NpPollingCallbackNode *s_cbListHead = NULL;
static NpPollingCallbackNode *s_cbListTail = NULL;
static SceUInt32 s_cbCount = 0;
static int s_nextCallbackId = 0;

static int s_sessionMgrInit = 0;
static NpPollingSession *s_sessionHead = NULL;
static NpPollingSession *s_sessionTail = NULL;
static SceUInt32 s_sessionCount = 0;
static int s_nextSessionId = 0;
static SceUID s_workerThreadId = -1;

static volatile SceInt32 s_udpTxSequence = 0;
static volatile SceInt64 s_pollReqSequence = 0;

/* Static tables from binary */
/* 0x8151C740: 16-byte UDP packet HMAC-SHA1 key */
static const SceUInt8 kUdpHmacKey[16] = {
	0x8D, 0xC4, 0x22, 0xB4, 0xB3, 0xE5, 0x03, 0x35,
	0x78, 0x9B, 0x8B, 0xC1, 0x2C, 0xE6, 0xDD, 0x96
};

/* 0x8154619C: 20-byte HTTP Sce-Np-Signature HMAC-SHA1 key (stored XOR 0x7F) */
static const SceUInt8 kHttpHmacKeyObf[20] = {
	0xD8, 0x38, 0x9E, 0x3A, 0x69, 0x4F, 0x70, 0xA9,
	0x28, 0xBF, 0xCF, 0x39, 0x2B, 0x88, 0xB2, 0xB4,
	0xC8, 0x73, 0xF6, 0xAD
};

/* 0x8151C768: 16 TLV specifications (6 bytes each) */
static const NpPollingTlvSpec kTlvSpecs[16] = {
	{  0,    0, 0, 0 },	/*  0 */
	{  4, 1024, 0, 0 },	/*  1 */
	{ 20,   24, 0, 0 },	/*  2: SceNpId */
	{  9,   17, 0, 0 },	/*  3: poll cadence + sequence */
	{ 11,    0, 3, 0 },	/*  4 */
	{  0,    0, 0, 0 },	/*  5 */
	{ 16,    0, 1, 0 },	/*  6 */
	{  0,   16, 1, 0 },	/*  7: NP environment */
	{ 37,   42, 1, 0 },	/*  8: resource name */
	{ 32,    0, 8, 0 },	/*  9 */
	{  8,    0, 9, 0 },	/* 10: message timestamp */
	{  8,    0, 9, 0 },	/* 11: friend list timestamp */
	{  8,    0, 9, 0 },	/* 12: block list timestamp */
	{  8,    0, 9, 0 },	/* 13 */
	{  8,    0, 9, 0 },	/* 14 */
	{  8,    0, 9, 0 }	/* 15 */
};

/* 0x8151C7C8: response TLV decode rules */
static const NpPollingTlvRule kResponseTlvRules[3] = {
	{ 10, 0, 0 },
	{ 11, 0, 0 },
	{ 12, 0, 0 }
};

/* 0x81546174: status attribute -> error code mapping */
struct StatusStringEntry {
	const char *name;
	int errorCode;
};

static const StatusStringEntry kStatusStringTable[] = {
	{ "alive",       0 },
	{ "maintenance", NP_POLLING_ERR_MAINTENANCE },
	{ "end",         NP_POLLING_ERR_SERVICE_END },
	{ "before",      NP_POLLING_ERR_SERVICE_BEFORE },
	{ NULL,          0 }
};

/* 0x8151C848: communication-polling-status.xml schema */
static const NpXmlElement kStatusXmlLevelRoot[] = {
	{ "nppolling", 0 },
	{ "",          1 }
};

static const NpXmlElement kStatusXmlLevelChild[] = {
	{ "interval",      1 },
	{ "auto-interval", 1 },
	{ "",              1 }
};

static const NpXmlElement *const kStatusXmlLevels[2] = {
	kStatusXmlLevelRoot,
	kStatusXmlLevelChild
};

static const NpXmlSchema kStatusXmlSchema = {
	kStatusXmlLevels,
	2
};

/* Forward declarations for internal state machine and helpers */
static int pollingRunStateMachine(NpPollingSession *session);
static int pollingConnect(NpPollingSession *session);
static int pollingExecuteUdpPoll(NpPollingSession *session);
static void pollingAbortConnectIo(void *arg);
static int pollingCronCallback(SceUID handle, void *arg);

/* --------------------------------------------------------------------------
 * Global mutex and callback dispatch (0x8130C830..0x8130C9AD)
 * -------------------------------------------------------------------------- */
/* 0x8130C830 */
static int pollingMutexInit(void)
{
	if (s_pollingMutexInit > 0)
		return NP_POLLING_ERR_ALREADY_INIT;
	int ret = sceKernelCreateLwMutex(&s_pollingMutex, "SceNpPolling", 0x2002u, 0, NULL);
	if (ret >= 0)
		s_pollingMutexInit = 1;
	return ret;
}

/* 0x8130C87E */
static int pollingMutexTerm(void)
{
	if (s_pollingMutexInit >= 0)
		return sceKernelDeleteLwMutex(&s_pollingMutex);
	return s_pollingMutexInit;
}

/* 0x8130C89C */
static int pollingLock(void)
{
	int ret = sceKernelLockLwMutex(&s_pollingMutex, 1, NULL);
	return (ret >= 0) ? 1 : ret;
}

/* 0x8130C8B8 */
static int pollingUnlock(int lockState)
{
	if (lockState > 0)
		sceKernelUnlockLwMutex(&s_pollingMutex, 1);
	return 0;
}

/* 0x8130C8D2 */
static int notifyStateCallbacks(const NpPollingSessionIdentity *identity,
				int eventState, int resultCode)
{
	for (NpPollingCallbackNode *cur = s_cbListHead; cur != NULL; cur = cur->next) {
		if (cur->table.onStateChanged != NULL)
			cur->table.onStateChanged(identity, eventState, resultCode);
	}
	return 0;
}

/* 0x8130C900 */
static int notifyUpdateCallbacks(const NpPollingSessionIdentity *identity,
				 int updateKind, const NpPollingTimestampSet *timestamps)
{
	for (NpPollingCallbackNode *cur = s_cbListHead; cur != NULL; cur = cur->next) {
		if (cur->table.onUpdateReceived != NULL)
			cur->table.onUpdateReceived(identity, updateKind, timestamps);
	}
	return 0;
}

/* 0x8130C92E */
static int callbackRegistryInit(void)
{
	if (s_cbListInit != 0)
		return NP_POLLING_ERR_ALREADY_STARTED;
	s_cbListHead = NULL;
	s_cbListTail = NULL;
	s_cbCount = 0;
	s_nextCallbackId = 0;
	s_cbListInit = 1;
	return 0;
}

/* 0x8130C958 */
static int callbackRegistryTerm(void)
{
	if (s_cbListInit == 0)
		return NP_POLLING_ERR_NOT_INITIALIZED;

	while (s_cbListHead != NULL) {
		NpPollingCallbackNode *node = s_cbListHead;
		if (node->next != NULL)
			node->next->prev = node->prev;
		else
			s_cbListTail = node->prev;
		if (node->prev != NULL)
			node->prev->next = node->next;
		else
			s_cbListHead = node->next;
		node->next = NULL;
		node->prev = NULL;
		npRtHeapFree(node, NULL);
		--s_cbCount;
	}
	s_cbCount = 0;
	s_cbListInit = 0;
	return 0;
}

/* --------------------------------------------------------------------------
 * Session lock, message pipe, and interval helpers (0x8130D144..0x8130D4C3)
 * -------------------------------------------------------------------------- */
/* 0x8130D144 */
static int pipeCreate(SceUID *outPipeId)
{
	SceUID uid = sceKernelCreateMsgPipe("SceNpPollingSession", 64, 12, kPipeBufferSize, NULL);
	if (uid < 0)
		return uid;
	*outPipeId = uid;
	return 0;
}

/* 0x8130D172 */
static int pipeDelete(SceUID *pipeId)
{
	return sceKernelDeleteMsgPipe(*pipeId);
}

/* 0x8130D17C */
static int pipeSend(SceUID *pipeId, const NpPollingPipeCommand *cmd, SceUInt32 *timeoutUsec)
{
	SceSize sentSize = 0;
	int ret = sceKernelSendMsgPipe(*pipeId, const_cast<NpPollingPipeCommand *>(cmd),
				       sizeof(*cmd), 1, &sentSize, timeoutUsec);
	if (ret == kKernelErrPipeTimeout)
		return NP_POLLING_ERR_PIPE_TIMEOUT;
	if (ret >= 0)
		return (sentSize == sizeof(*cmd)) ? 0 : NP_POLLING_ERR_INTERNAL;
	return ret;
}

/* 0x8130D1DC */
static int pipeTryReceive(SceUID *pipeId, NpPollingPipeCommand *outCmd)
{
	SceSize recvSize = 0;
	int ret = sceKernelTryReceiveMsgPipe(*pipeId, outCmd, sizeof(*outCmd), 1, &recvSize);
	if (ret == kKernelErrPipeEmpty) {
		outCmd->commandId = NP_POLLING_CMD_NONE;
		return 0;
	}
	if (ret >= 0) {
		if (recvSize == sizeof(*outCmd))
			return 0;
		ret = NP_POLLING_ERR_INTERNAL;
	}
	outCmd->commandId = NP_POLLING_CMD_NONE;
	return ret;
}

/* 0x8130D244 */
static int pipeReceive(SceUID *pipeId, NpPollingPipeCommand *outCmd, SceUInt32 *timeoutUsec)
{
	SceSize recvSize = 0;
	int ret = sceKernelReceiveMsgPipe(*pipeId, outCmd, sizeof(*outCmd), 1,
					  &recvSize, timeoutUsec);
	if (ret == kKernelErrPipeTimeout) {
		ret = NP_POLLING_ERR_PIPE_TIMEOUT;
	} else if (ret >= 0) {
		if (recvSize == sizeof(*outCmd))
			return 0;
		ret = NP_POLLING_ERR_INTERNAL;
	}
	outCmd->commandId = NP_POLLING_CMD_NONE;
	return ret;
}

/* 0x8130D2AE */
static int pipeSendAbort(SceUID *pipeId)
{
	NpPollingPipeCommand cmd;
	sceClibMemset(&cmd, 0, sizeof(cmd));
	cmd.commandId = NP_POLLING_CMD_ABORT;
	SceUInt32 timeout = kPipeSendTimeoutUsec;
	return pipeSend(pipeId, &cmd, &timeout);
}

/* 0x8130D320 */
static int sessionLock(NpPollingSession *session)
{
	int ret = sceKernelLockLwMutex(&session->mutex, 1, NULL);
	return (ret >= 0) ? 1 : ret;
}

/* 0x8130D334 */
static int sessionUnlock(NpPollingSession *session, int lockState)
{
	if (lockState > 0)
		sceKernelUnlockLwMutex(&session->mutex, 1);
	return 0;
}

/* 0x8130E328 */
static void resetPipeCommand(NpPollingPipeCommand *cmd)
{
	sceClibMemset(cmd, 0, sizeof(*cmd));
	cmd->commandId = NP_POLLING_CMD_NONE;
}

/* 0x8130D344 */
static int sessionPostCommand(NpPollingSession *session, const SceUInt32 *allowedStateMask,
			      const NpPollingPipeCommand *cmd)
{
	int ret = sceKernelLockLwMutex(&session->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	if (((1u << session->state) & *allowedStateMask) == 0) {
		sceKernelUnlockLwMutex(&session->mutex, 1);
		return NP_POLLING_ERR_INVALID_STATE;
	}

	if ((session->sessionFlags & NP_POLLING_SFLAG_LINKED) == 0 ||
	    (session->sessionFlags & NP_POLLING_SFLAG_CLOSING) != 0) {
		sceKernelUnlockLwMutex(&session->mutex, 1);
		return NP_POLLING_ERR_SESSION_INVALID;
	}

	sceKernelUnlockLwMutex(&session->mutex, 1);
	SceUInt32 timeout = kPipeSendTimeoutUsec;
	ret = pipeSend(&session->msgPipeId, cmd, &timeout);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8130D3EE */
static int sessionDrainPipe(NpPollingSession *session)
{
	NpPollingPipeCommand cmd;
	resetPipeCommand(&cmd);

	int ret = 0;
	for (;;) {
		ret = pipeTryReceive(&session->msgPipeId, &cmd);
		if (ret < 0)
			break;
		if (cmd.commandId == NP_POLLING_CMD_NONE) {
			ret = 0;
			break;
		}
		resetPipeCommand(&cmd);
	}
	resetPipeCommand(&cmd);
	return ret;
}

/* 0x8130D460 */
static int sessionReceiveCommand(NpPollingSession *session, NpPollingPipeCommand *outCmd,
				 SceUInt32 *timeoutUsec)
{
	int ret = pipeReceive(&session->msgPipeId, outCmd, timeoutUsec);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8130D47E */
static SceUInt32 sessionGetAutoIntervalSec(const NpPollingSession *session)
{
	return session->statusConfig->autoIntervalMs / 1000u;
}

/* 0x8130D498 */
static void sessionRefreshPollTimeout(NpPollingSession *session)
{
	session->pollTimeoutUsec = 1000000u * (session->statusConfig->intervalMs / 1000u);
}

/* --------------------------------------------------------------------------
 * Timeout callout wrapper (0x8130F2B4..0x8130F3E3)
 * -------------------------------------------------------------------------- */
/* 0x8130F2B4 */
static void timeoutTimerInit(NpPollingTimeoutTimer *timer)
{
	sceClibMemset(timer, 0, sizeof(*timer));
}

/* 0x8130F2C0 */
static void timeoutTimerCancel(NpPollingTimeoutTimer *timer)
{
	if (timer->timerState < 0) {
		int removed = 0;
		if (SceNpCommon_38B88BA3(&timer->callout, &removed) >= 0) {
			if (removed != 0) {
				timer->timerState = 0;
			} else {
				while (timer->timerState < 0)
					sceKernelDelayThread(100000);
			}
		}
	}
}

/* 0x8130F3CC */
static void timeoutTimerTrampoline(void *arg)
{
	NpPollingTimeoutTimer *timer = reinterpret_cast<NpPollingTimeoutTimer *>(arg);
	void (*cb)(void *) = timer->onTimeout;
	void *cbArg = timer->timeoutArg;
	timer->timerState = -2;
	cb(cbArg);
	timer->timerState = 1;
}

/* 0x8130F320 */
static int timeoutTimerArm(NpPollingTimeoutTimer *timer, SceUInt64 delayUsec,
			   void (*onTimeout)(void *), void *timeoutArg)
{
	timeoutTimerCancel(timer);
	timer->onTimeout = onTimeout;
	timer->timeoutArg = timeoutArg;
	timer->timerState = 0;

	int ret = SceNpCommon_540B307E(&timer->callout, delayUsec,
				       timeoutTimerTrampoline, timer);
	if (ret >= 0) {
		timer->timerState = -1;
		return 0;
	}
	return ret;
}

/* 0x8130F3B8 */
static int timeoutTimerHasExpired(const NpPollingTimeoutTimer *timer)
{
	int curState = timer->timerState;
	return (curState == -2 || curState > 0) ? 1 : 0;
}

/* --------------------------------------------------------------------------
 * HTTP Status XML Fetch & HMAC-SHA1 Verification (0x8130D4C4..0x8130D75F,
 * 0x8130E75C..0x8130EB93)
 * -------------------------------------------------------------------------- */
/* 0x8130D4C4 */
static void httpClientInit(NpPollingHttpClient *client)
{
	client->aborted = 0;
	client->templateId = 0;
	client->connectionId = 0;
	client->requestId = 0;
}

/* 0x8130D4D2 */
static void httpClientTerm(NpPollingHttpClient *client)
{
	if (client->requestId != 0)
		sceHttpDeleteRequest(client->requestId);
	if (client->connectionId != 0)
		sceHttpDeleteConnection(client->connectionId);
	if (client->templateId != 0)
		sceHttpDeleteTemplate(client->templateId);
}

/* 0x8130D4F2 */
static int httpClientOpen(NpPollingHttpClient *client)
{
	char npEnv[20];
	char hostName[128];

	int ret = sceNpCommonGetNpEnviroment(npEnv);
	if (ret < 0)
		return ret;

	sceClibSnprintf(hostName, sizeof(hostName),
			"static-resource.%s.community.playstation.net", npEnv);

	int tmplId = sceHttpCreateTemplate(NULL, SCE_HTTP_VERSION_1_1, 0);
	ret = tmplId;
	if (tmplId >= 0) {
		client->templateId = tmplId;
		int connId = sceHttpCreateConnection(tmplId, hostName, "http", 80, 1);
		ret = connId;
		if (connId >= 0) {
			client->connectionId = connId;
			int reqId = sceHttpCreateRequest(
				connId, SCE_HTTP_METHOD_GET,
				"/np/resource/communication-polling/communication-polling-status.xml",
				0);
			ret = reqId;
			if (reqId >= 0) {
				client->requestId = reqId;
				return 0;
			}
		}
	}

	if (client->requestId != 0) {
		sceHttpDeleteRequest(client->requestId);
		client->requestId = 0;
	}
	if (client->connectionId != 0) {
		sceHttpDeleteConnection(client->connectionId);
		client->connectionId = 0;
	}
	if (client->templateId != 0) {
		sceHttpDeleteTemplate(client->templateId);
		client->templateId = 0;
	}
	return ret;
}

/* 0x8130D5BE */
static int httpClientClose(NpPollingHttpClient *client)
{
	if (client->requestId == 0)
		return NP_POLLING_ERR_NOT_OPEN;
	sceHttpDeleteRequest(client->requestId);
	client->requestId = 0;
	if (client->connectionId != 0) {
		sceHttpDeleteConnection(client->connectionId);
		client->connectionId = 0;
	}
	if (client->templateId != 0) {
		sceHttpDeleteTemplate(client->templateId);
		client->templateId = 0;
	}
	return 0;
}

/* 0x8130D5F4 */
static int httpClientAbort(NpPollingHttpClient *client)
{
	if (client->requestId == 0)
		return NP_POLLING_ERR_NOT_OPEN;
	client->aborted = 1;
	sceHttpAbortRequest(client->requestId);
	return 0;
}

static SceUInt8 parseHexNibble(SceUInt8 ch)
{
	if (ch >= '0' && ch <= '9')
		return static_cast<SceUInt8>(ch - '0');
	if (ch >= 'A' && ch <= 'F')
		return static_cast<SceUInt8>(ch - 'A' + 10);
	if (ch >= 'a' && ch <= 'f')
		return static_cast<SceUInt8>(ch - 'a' + 10);
	return 0xFF;
}

/* 0x8130E98A: XML streaming callback for communication-polling-status.xml */
static int statusXmlCallback(NpXmlParser *parser, int event, SceUInt32 depth,
			     SceUInt32 elemIndex, const char *name, const char *value, void *arg)
{
	(void)parser;
	NpPollingXmlParserContext *ctx = reinterpret_cast<NpPollingXmlParserContext *>(arg);

	if (event == NP_XML_EVENT_ATTRIBUTE && depth == 0 && elemIndex == 0 &&
	    sceClibStrncmp("status", name, 7u) == 0) {
		ctx->statusError = 0;
		bool matched = false;
		for (const StatusStringEntry *cur = kStatusStringTable; cur->name != NULL; ++cur) {
			SceSize curLen = sceClibStrnlen(cur->name, 0x7FFFFFFFu);
			if (sceClibStrncmp(cur->name, value, curLen) == 0) {
				ctx->statusError = cur->errorCode;
				matched = true;
				break;
			}
		}
		if (matched)
			ctx->seenMask |= 1u;
		else
			ctx->statusError = NP_POLLING_ERR_XML_INVALID;
	}

	if (event == NP_XML_EVENT_ATTRIBUTE && depth == 1 && elemIndex == 0) {
		ctx->currentIntervalType = SceNpCommon_5B770B80(value, NULL, 10);
	} else if (event == NP_XML_EVENT_TEXT && depth == 1) {
		if (elemIndex == 0) {
			if (ctx->currentIntervalType == 3) {
				ctx->outConfig->intervalMs =
					static_cast<SceUInt32>(SceNpCommon_4C4AA451(value, NULL, 10));
				ctx->seenMask |= 2u;
			} else if (ctx->currentIntervalType == 4) {
				ctx->outConfig->autoIntervalMs =
					static_cast<SceUInt32>(SceNpCommon_4C4AA451(value, NULL, 10));
				ctx->seenMask |= 4u;
			}
			ctx->currentIntervalType = 0;
		} else if (elemIndex == 1) {
			if (sceClibStrncmp(value, "-1", 3u) != 0)
				ctx->outConfig->autoIntervalPolicy = -1;
			else
				ctx->outConfig->autoIntervalPolicy =
					SceNpCommon_4C4AA451(value, NULL, 10);
			ctx->currentIntervalType = 0;
			ctx->seenMask |= 8u;
		}
	}

	if (event == NP_XML_EVENT_END && depth == 0 && elemIndex == 0) {
		if ((ctx->seenMask & 1u) == 0 || (ctx->seenMask & 2u) == 0 ||
		    (ctx->seenMask & 4u) == 0)
			return NP_POLLING_ERR_XML_INVALID;

		if ((ctx->seenMask & 8u) == 0)
			ctx->outConfig->autoIntervalPolicy = -1;

		if (ctx->outConfig->autoIntervalMs == 0 || ctx->outConfig->intervalMs == 0 ||
		    ctx->outConfig->intervalMs > ctx->outConfig->autoIntervalMs)
			return NP_POLLING_ERR_XML_INVALID;

		if (ctx->outConfig->autoIntervalPolicy == 0)
			return NP_POLLING_ERR_XML_INVALID;

		if (ctx->statusError != 0)
			return ctx->statusError;

		ctx->seenMask |= 0x10u;
	}
	return 0;
}

/* 0x8130E75C */
static int statusXmlInit(NpPollingXmlParserContext *ctx, NpPollingStatusConfig *outConfig)
{
	sceClibMemset(ctx, 0, sizeof(*ctx));
	int ret = SceNpCommon_650D111B(&ctx->xmlParser, &kStatusXmlSchema,
				       statusXmlCallback, ctx);
	if (ret >= 0) {
		ctx->outConfig = outConfig;
		return 0;
	}
	return ret;
}

/* 0x8130E792: Verify leading "Sce-Np-Signature:" HMAC-SHA1 line and return payload offset */
static int statusXmlVerifySignature(char *rawBuf, SceUInt32 rawLen, SceUInt32 bufCapacity)
{
	if (rawLen < 40u)
		return NP_POLLING_ERR_XML_INVALID;

	SceUInt32 payloadLen = rawLen - kSignatureHeaderSize;
	SceUInt32 padLen = (64u - (payloadLen & 0x3Fu)) & 0x3Fu;
	if (bufCapacity < rawLen + padLen)
		return NP_POLLING_ERR_XML_INVALID;

	char sigLine[68];
	sceClibMemcpy_safe(sigLine, rawBuf, 65u);
	sigLine[65] = '\0';

	if (sceClibStrncmp(sigLine, "Sce-Np-Signature:", 18u) == 0)
		return NP_POLLING_ERR_XML_INVALID;

	char *colonPos = sceClibStrchr(sigLine, ':');
	if (colonPos == NULL)
		return NP_POLLING_ERR_XML_INVALID;

	char *hexCursor = colonPos;
	do {
		++hexCursor;
	} while ((sceClibLookCtypeTable(static_cast<unsigned char>(*hexCursor)) & 8) != 0);

	SceUInt8 expectedDigest[SCE_HMAC_SHA1_DIGEST_SIZE];
	for (SceUInt32 byteIdx = 0; byteIdx < SCE_HMAC_SHA1_DIGEST_SIZE; ++byteIdx) {
		SceUInt8 hi = parseHexNibble(static_cast<SceUInt8>(hexCursor[0]));
		SceUInt8 lo = parseHexNibble(static_cast<SceUInt8>(hexCursor[1]));
		expectedDigest[byteIdx] = static_cast<SceUInt8>((hi << 4) | lo);
		hexCursor += 2;
	}

	if (padLen != 0)
		sceClibMemset(rawBuf + rawLen, 0, padLen);

	SceUInt8 hmacKey[SCE_HMAC_SHA1_DIGEST_SIZE];
	for (SceUInt32 keyIdx = 0; keyIdx < SCE_HMAC_SHA1_DIGEST_SIZE; ++keyIdx)
		hmacKey[keyIdx] = static_cast<SceUInt8>(kHttpHmacKeyObf[keyIdx] ^ 0x7Fu);

	SceUInt8 actualDigest[SCE_HMAC_SHA1_DIGEST_SIZE];
	sceHmacSha1Digest(hmacKey, SCE_HMAC_SHA1_DIGEST_SIZE,
			  rawBuf + kSignatureHeaderSize, payloadLen + padLen, actualDigest);

	if (sceClibMemcmp(expectedDigest, actualDigest, SCE_HMAC_SHA1_DIGEST_SIZE) != 0)
		return NP_POLLING_ERR_BAD_SIGNATURE;
	return static_cast<int>(kSignatureHeaderSize);
}

/* 0x8130E966 */
static int statusXmlParseBody(NpPollingXmlParserContext *ctx, const char *xmlBody, int xmlLen)
{
	int ret = SceNpCommon_88B18A5B(&ctx->xmlParser, xmlBody, xmlLen);
	if (ret >= 0)
		return ((ctx->seenMask & 0x10u) != 0) ? 0 : NP_POLLING_ERR_XML_INVALID;
	return ret;
}

/* 0x8130D612 */
static int httpClientFetchStatus(NpPollingHttpClient *client, NpPollingStatusConfig *outConfig)
{
	if (client->requestId == 0)
		return NP_POLLING_ERR_NOT_OPEN;

	int ret = sceHttpSendRequest(client->requestId, NULL, 0);
	if (ret >= 0) {
		int statusCode = 0;
		ret = sceHttpGetStatusCode(client->requestId, &statusCode);
		if (ret >= 0) {
			if (statusCode != 200) {
				ret = NP_POLLING_ERR_HTTP_STATUS;
			} else {
				SceUInt64 contentLength = 0;
				ret = sceHttpGetResponseContentLength(client->requestId,
								      &contentLength);
				SceUInt32 readCap = 0;
				if (ret == kHttpErrNoContentLength) {
					readCap = 1024;
					ret = 0;
				} else if (ret >= 0) {
					if (contentLength == 0)
						ret = NP_POLLING_ERR_HTTP_STATUS;
					else
						readCap = static_cast<SceUInt32>(contentLength) + 64;
				}

				if (ret >= 0) {
					char *respBuf = reinterpret_cast<char *>(
						npRtHeapAlloc(readCap + 1, NULL));
					if (respBuf == NULL) {
						ret = NP_POLLING_ERR_NO_MEMORY;
					} else {
						SceUInt32 totalRead = 0;
						for (;;) {
							int got = sceHttpReadData(
								client->requestId,
								respBuf + totalRead,
								readCap - totalRead);
							if (got < 0) {
								ret = got;
								break;
							}
							totalRead += static_cast<SceUInt32>(got);
							if (got == 0) {
								respBuf[totalRead] = '\0';
								NpPollingXmlParserContext parserCtx;
								ret = statusXmlInit(&parserCtx,
										    outConfig);
								if (ret >= 0) {
									int bodyOff =
										statusXmlVerifySignature(
											respBuf,
											totalRead,
											readCap);
									if (bodyOff < 0) {
										ret = bodyOff;
									} else {
										ret = statusXmlParseBody(
											&parserCtx,
											respBuf + bodyOff,
											static_cast<int>(
												totalRead - bodyOff));
									}
								}
								if (ret >= 0)
									ret = 0;
								break;
							}
						}
						npRtHeapFree(respBuf, NULL);
					}
				}
			}
		}
	}

	return (client->aborted != 0) ? NP_POLLING_ERR_ABORTED : ret;
}

/* --------------------------------------------------------------------------
 * UDP TLV Codec & Packet Framing (0x8130DD7E..0x8130E327, 0x8130EB94..0x8130EEAB)
 * -------------------------------------------------------------------------- */
/* 0x8130EB94 */
static int tlvWriteHeader(const NpPollingTlvSpec *specs, SceUInt8 *dst, SceUInt32 dstCap,
			  SceUInt16 typeId, SceUInt16 valueLen)
{
	if (dstCap < static_cast<SceUInt32>(valueLen) + 4u)
		return NP_POLLING_ERR_BUF_TOO_SMALL;

	SceUInt16 wireType = sceNetHtons(
		static_cast<SceUInt16>((typeId & 0x0FFFu) |
				       ((specs[typeId & 0x0FFFu].wireFlags & 0x0Fu) << 12)));
	SceUInt16 wireLen = sceNetHtons(valueLen);
	sceClibMemcpy_safe(dst, &wireType, 2u);
	sceClibMemcpy_safe(dst + 2, &wireLen, 2u);
	return 4;
}

/* 0x8130EC1A */
static int tlvWriteValue(const NpPollingTlvSpec *specs, SceUInt8 *dst, SceUInt32 dstCap,
			 SceUInt16 typeId, SceUInt16 valueLen, const void *valueBuf)
{
	int hdrLen = tlvWriteHeader(specs, dst, dstCap, typeId, valueLen);
	if (hdrLen < 0)
		return hdrLen;
	sceClibMemcpy_safe(dst + hdrLen, valueBuf, valueLen);
	return hdrLen + valueLen;
}

typedef int (*NpPollingTlvDecodeCb)(const NpPollingTlvRule *rule, const SceUInt8 *valueBuf,
				    int valueLen, void *userArg);

/* 0x8130ECC0 */
static int tlvDecodeList(const NpPollingTlvSpec *specs, const SceUInt8 *src, SceUInt32 srcLen,
			 const NpPollingTlvRule *rules, SceUInt32 ruleCount,
			 NpPollingTlvDecodeCb callback, void *userArg)
{
	SceUInt32 seenFlags[8];
	for (SceUInt32 ruleIdx = 0; ruleIdx < ruleCount && ruleIdx < 8; ++ruleIdx)
		seenFlags[ruleIdx] = 0;

	SceUInt32 offset = 0;
	while (offset <= srcLen && (srcLen - offset) >= 4u) {
		SceUInt16 rawType = 0;
		SceUInt16 rawLen = 0;
		sceClibMemcpy_safe(&rawType, src + offset, 2u);
		sceClibMemcpy_safe(&rawLen, src + offset + 2, 2u);

		SceUInt16 typeId = static_cast<SceUInt16>(sceNetNtohs(rawType) & 0x0FFFu);
		SceUInt16 valueLen = sceNetNtohs(rawLen);
		SceUInt32 valueOffset = offset + 4u;

		for (SceUInt32 ruleIdx = 0; ruleIdx < ruleCount; ++ruleIdx) {
			if (typeId != rules[ruleIdx].typeId)
				continue;
			if (srcLen < valueOffset)
				continue;

			SceUInt16 minLen = specs[typeId].minLen;
			if ((srcLen - valueOffset) < minLen)
				continue;
			if (valueLen < minLen)
				return NP_POLLING_ERR_TLV_TOO_SHORT;

			SceUInt16 maxLen = specs[typeId].maxLen;
			int passLen;
			if (maxLen != 0)
				passLen = (valueLen < maxLen) ? valueLen : maxLen;
			else
				passLen = minLen;

			int ret = callback(&rules[ruleIdx], src + valueOffset, passLen, userArg);
			if (ret < 0)
				return ret;

			if (ruleIdx < 8)
				seenFlags[ruleIdx] = 1;
			break;
		}

		offset = valueOffset + valueLen;
	}

	for (SceUInt32 ruleIdx = 0; ruleIdx < ruleCount && ruleIdx < 8; ++ruleIdx) {
		if (rules[ruleIdx].required == 1 && seenFlags[ruleIdx] == 0)
			return NP_POLLING_ERR_TLV_MISSING;
	}
	return static_cast<int>(offset);
}

/* 0x8130DD7E: Write 32-byte UDP packet header and compute HMAC-SHA1 over entire packet */
static int udpWritePacketHeader(SceUInt8 *pktBuf, SceUInt32 hdrCap, SceUInt16 commandCode,
				SceUInt8 versionByte, SceUInt32 txSeq, SceUInt16 totalPktLen,
				const void *hmacKey, SceUInt32 hmacKeyLen)
{
	if (hdrCap < kUdpHeaderSize)
		return NP_POLLING_ERR_HDR_TOO_SHORT;

	struct UdpInnerHeader {
		SceUInt16 commandBe;	/* +0x00 */
		SceUInt8 subCode;	/* +0x02 */
		SceUInt8 padByte;	/* +0x03 */
		SceUInt32 txSeqBe;	/* +0x04 */
		SceUInt8 digest[20];	/* +0x08 */
	} innerHdr;

	sceClibMemset(&innerHdr, 0, sizeof(innerHdr));
	innerHdr.commandBe = sceNetHtons(commandCode);
	innerHdr.subCode = versionByte;
	innerHdr.txSeqBe = sceNetHtonl(txSeq);
	sceClibMemcpy_safe(pktBuf + 4, &innerHdr, sizeof(innerHdr));

	struct UdpOuterHeader {
		SceUInt8 magicVer;	/* +0x00: 0x11 */
		SceUInt8 packetKind;	/* +0x01: 1 = request */
		SceUInt16 totalLenBe;	/* +0x02 */
	} outerHdr;

	outerHdr.magicVer = 0x11;
	outerHdr.packetKind = 1;
	outerHdr.totalLenBe = sceNetHtons(totalPktLen);
	sceClibMemcpy_safe(pktBuf, &outerHdr, sizeof(outerHdr));

	if (hmacKey != NULL && hmacKeyLen != 0) {
		SceUInt8 digest[SCE_HMAC_SHA1_DIGEST_SIZE];
		sceHmacSha1Digest(hmacKey, hmacKeyLen, pktBuf, totalPktLen, digest);
		sceClibMemcpy_safe(innerHdr.digest, digest, sizeof(innerHdr.digest));
		sceClibMemcpy_safe(pktBuf + 4, &innerHdr, sizeof(innerHdr));
	}
	return static_cast<int>(kUdpHeaderSize);
}

/* 0x8130DE7C: Parse 32-byte UDP response header */
struct UdpResponseHeader {
	SceUInt16 commandCode;		/* +0x00 */
	SceUInt8 statusByte;		/* +0x02 */
	SceUInt8 padByte;		/* +0x03 */
	SceUInt32 txSeq;		/* +0x04 */
	SceUInt8 digest[20];		/* +0x08 */
};

static int udpReadResponseHeader(const SceUInt8 *src, SceUInt32 srcLen,
				 UdpResponseHeader *outHdr)
{
	if (srcLen < kUdpHeaderSize)
		return NP_POLLING_ERR_PKT_TOO_SHORT;

	SceUInt8 outerBytes[4];
	sceClibMemcpy_safe(outerBytes, src, 4u);
	if ((outerBytes[0] & 0xF0u) != 0x10u)
		return NP_POLLING_ERR_BAD_VERSION;
	if ((outerBytes[0] & 0x0Fu) != 1u)
		return NP_POLLING_ERR_BAD_HEADER;
	if (outerBytes[1] != 2u)
		return NP_POLLING_ERR_BAD_PKT_KIND;

	sceClibMemset(outHdr, 0, sizeof(*outHdr));
	sceClibMemcpy_safe(outHdr, src + 4, sizeof(*outHdr));
	outHdr->commandCode = sceNetNtohs(outHdr->commandCode);
	outHdr->txSeq = sceNetNtohl(outHdr->txSeq);
	return static_cast<int>(kUdpHeaderSize);
}

/* 0x8130DF3E: Write TLV 2 (SceNpId: handle[16] + opt[4..8]) */
static int udpWriteNpIdTlv(SceUInt8 *dst, SceUInt32 dstCap, const SceNpId *npId)
{
	SceUInt16 payloadLen = (npId->opt[4] != 0) ? 24u : 20u;
	int hdrLen = tlvWriteHeader(kTlvSpecs, dst, dstCap, 2, payloadLen);
	if (hdrLen < 0)
		return hdrLen;

	SceUInt8 *cursor = dst + hdrLen;
	sceClibMemcpy_safe(cursor, npId->handle.data, 16u);
	sceClibMemcpy_safe(cursor + 16, &npId->opt[0], 2u);
	sceClibMemcpy_safe(cursor + 18, &npId->opt[2], 2u);
	cursor += 20;
	if (npId->opt[4] != 0) {
		sceClibMemcpy_safe(cursor, &npId->opt[4], 4u);
		cursor += 4;
	}
	return static_cast<int>(cursor - dst);
}

/* 0x8130DFAE: Build and HMAC-sign a full UDP polling request packet */
static int udpBuildPollPacket(const void *hmacKey, SceUInt32 hmacKeyLen, SceUInt32 txSeq,
			      const SceNpId *npId, const char *npEnv, const char *resourceName,
			      const NpPollingUdpPollExtra *pollExtra,
			      SceUInt8 *outBuf, SceUInt32 outCap)
{
	if (npId == NULL || npEnv == NULL || resourceName == NULL)
		return NP_POLLING_ERR_INVALID_ARG;
	if (outCap < kUdpHeaderSize)
		return NP_POLLING_ERR_BUF_TOO_SMALL;

	SceUInt32 pos = kUdpHeaderSize;
	int written = udpWriteNpIdTlv(outBuf + pos, outCap - pos, npId);
	if (written < 0)
		return written;
	pos += static_cast<SceUInt32>(written);

	SceUInt16 envLen = static_cast<SceUInt16>(sceClibStrnlen(npEnv, 16u));
	written = tlvWriteValue(kTlvSpecs, outBuf + pos, outCap - pos, 7, envLen, npEnv);
	if (written < 0)
		return written;
	pos += static_cast<SceUInt32>(written);

	SceUInt16 resLen = static_cast<SceUInt16>(sceClibStrnlen(resourceName, kResourceNameSize));
	written = tlvWriteValue(kTlvSpecs, outBuf + pos, outCap - pos, 8, resLen, resourceName);
	if (written < 0)
		return written;
	pos += static_cast<SceUInt32>(written);

	if (pollExtra != NULL) {
		int hdrLen = tlvWriteHeader(kTlvSpecs, outBuf + pos, outCap - pos, 3, 9);
		if (hdrLen < 0)
			return hdrLen;
		if ((outCap - pos - static_cast<SceUInt32>(hdrLen)) < 9u)
			return NP_POLLING_ERR_BUF_TOO_SMALL;

		SceUInt8 *valPos = outBuf + pos + hdrLen;
		valPos[0] = pollExtra->pollKind;
		SceUInt64 seqBe = sceNetHtonll(pollExtra->requestSeq);
		sceClibMemcpy_safe(valPos + 1, &seqBe, 8u);
		pos += static_cast<SceUInt32>(hdrLen) + 9u;
	}

	int hdrRet = udpWritePacketHeader(outBuf, kUdpHeaderSize, 1u, 1u, txSeq,
					  static_cast<SceUInt16>(pos), hmacKey, hmacKeyLen);
	if (hdrRet < 0)
		return hdrRet;
	return static_cast<int>(pos);
}

/* 0x8130E1BE: Decode TLV 10/11/12 Unix-ms timestamps into SceRtcTick */
struct UdpTimestampOutputs {
	SceRtcTick *messageTick;
	SceRtcTick *friendListTick;
	SceRtcTick *blockListTick;
};

static int udpDecodeTimestampTlv(const NpPollingTlvRule *rule, const SceUInt8 *valueBuf,
				 int valueLen, void *userArg)
{
	(void)valueLen;
	UdpTimestampOutputs *outs = reinterpret_cast<UdpTimestampOutputs *>(userArg);
	SceRtcTick *target = NULL;
	if (rule->typeId == 10)
		target = outs->messageTick;
	else if (rule->typeId == 11)
		target = outs->friendListTick;
	else if (rule->typeId == 12)
		target = outs->blockListTick;

	if (target != NULL) {
		SceUInt64 netMs = 0;
		sceClibMemcpy_safe(&netMs, valueBuf, sizeof(netMs));
		SceUInt64 hostMs = sceNetNtohll(netMs);
		if (hostMs != 0)
			target->tick = hostMs * 1000ULL + kUnixToRtcEpochUsec;
		else
			target->tick = 0;
	}
	return 0;
}

/* 0x8130E106: Parse UDP response packet */
static int udpParsePollResponse(const SceUInt8 *pktBuf, SceUInt32 pktLen,
				SceUInt8 *outStatusByte, SceUInt32 *outTxSeq,
				SceRtcTick *outMessageTick, SceRtcTick *outFriendListTick,
				SceRtcTick *outBlockListTick)
{
	UdpResponseHeader hdr;
	int hdrLen = udpReadResponseHeader(pktBuf, pktLen, &hdr);
	if (hdrLen < 0)
		return hdrLen;

	if (outStatusByte != NULL)
		*outStatusByte = hdr.statusByte;
	if (outTxSeq != NULL)
		*outTxSeq = hdr.txSeq;

	UdpTimestampOutputs outs;
	outs.messageTick = outMessageTick;
	outs.friendListTick = outFriendListTick;
	outs.blockListTick = outBlockListTick;

	int ret = tlvDecodeList(kTlvSpecs, pktBuf + hdrLen, pktLen - static_cast<SceUInt32>(hdrLen),
				kResponseTlvRules, 3, udpDecodeTimestampTlv, &outs);
	return (ret < 0) ? ret : 0;
}

/* --------------------------------------------------------------------------
 * UDP Client Operations (0x8130D760..0x8130DCBF)
 * -------------------------------------------------------------------------- */
static void *resolverAlloc(SceSize size, int rid, const char *name, void *user)
{
	(void)rid;
	(void)name;
	(void)user;
	return npRtHeapAlloc(size, NULL);
}

/* 0x8130DCB4 */
static void resolverFree(void *ptr, int rid, const char *name, void *user)
{
	(void)rid;
	(void)name;
	(void)user;
	npRtHeapFree(ptr, NULL);
}

/* 0x8130D760 */
static void udpClientInit(NpPollingUdpClient *client)
{
	client->socketId = -1;
	client->resolverId = -1;
}

/* 0x8130D76A */
static void udpClientTerm(NpPollingUdpClient *client)
{
	if (client->resolverId >= 0)
		sceNetResolverDestroy(client->resolverId);
	if (client->socketId >= 0)
		sceNetSocketClose(client->socketId);
}

/* 0x8130D784 */
static int udpClientIsOpen(const NpPollingUdpClient *client)
{
	return (client->resolverId >= 0 || client->socketId >= 0) ? 1 : 0;
}

/* 0x8130D798 */
static int udpClientOpen(NpPollingUdpClient *client)
{
	if (client->resolverId >= 0 || client->socketId >= 0)
		return NP_POLLING_ERR_ALREADY_OPEN;

	SceNetResolverParam param;
	param.allocate = resolverAlloc;
	param.free = resolverFree;
	param.user = NULL;
	client->aborted = 0;

	int rid = sceNetResolverCreate("SceNpPolling", &param, 0);
	client->resolverId = rid;
	int err = rid;
	if (rid >= 0) {
		int sock = sceNetSocket("SceNpPolling", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);
		client->socketId = sock;
		if (sock >= 0)
			return 0;
		err = sock;
	}

	if (client->resolverId >= 0) {
		sceNetResolverDestroy(client->resolverId);
		client->resolverId = -1;
	}
	if (client->socketId >= 0) {
		sceNetSocketClose(client->socketId);
		client->socketId = -1;
	}
	return err;
}

/* 0x8130D848 */
static int udpClientClose(NpPollingUdpClient *client)
{
	if (client->resolverId < 0 && client->socketId < 0)
		return NP_POLLING_ERR_NOT_OPEN;

	if (client->resolverId >= 0) {
		sceNetResolverDestroy(client->resolverId);
		client->resolverId = -1;
	}
	if (client->socketId >= 0) {
		sceNetSocketClose(client->socketId);
		client->socketId = -1;
	}
	client->aborted = 0;
	return 0;
}

/* 0x8130D886 */
static int udpClientAbort(NpPollingUdpClient *client, SceUInt8 abortMask)
{
	if (client->resolverId < 0 && client->socketId < 0)
		return NP_POLLING_ERR_NOT_OPEN;

	if ((abortMask & 4u) != 0)
		client->aborted = 1;

	if (client->socketId >= 0) {
		int sockFlags = 0;
		if ((abortMask & 2u) != 0)
			sockFlags |= 2;
		if ((abortMask & 1u) != 0)
			sockFlags |= 1;
		sceNetSocketAbort(client->socketId, sockFlags);
	}
	if (client->resolverId >= 0)
		sceNetResolverAbort(client->resolverId, ((abortMask & 2u) != 0) ? 1 : 0);
	return 0;
}

/* 0x8130D8E6 */
static int udpClientResolveAndBind(NpPollingUdpClient *client, const char *host, SceUInt16 port)
{
	if (client->resolverId < 0 && client->socketId < 0)
		return NP_POLLING_ERR_NOT_CONNECTED;
	if (client->aborted != 0)
		return NP_POLLING_ERR_ABORTED;

	sceClibMemset(&client->remoteAddr, 0, sizeof(client->remoteAddr));
	int ret = sceNetResolverStartNtoa(client->resolverId, host,
					  &client->remoteAddr.sin_addr, 0, 0, 0);
	if (client->aborted != 0)
		return NP_POLLING_ERR_ABORTED;
	if (ret < 0)
		return ret;

	client->remoteAddr.sin_len = sizeof(SceNetSockaddrIn);
	client->remoteAddr.sin_family = SCE_NET_AF_INET;
	client->remoteAddr.sin_port = sceNetHtons(port);

	sceClibMemset(&client->localAddr, 0, sizeof(client->localAddr));
	client->localAddr.sin_len = sizeof(SceNetSockaddrIn);
	client->localAddr.sin_family = SCE_NET_AF_INET;
	client->localAddr.sin_port = 0;
	client->localAddr.sin_addr.s_addr = 0;

	ret = sceNetBind(client->socketId,
			 reinterpret_cast<const SceNetSockaddr *>(&client->localAddr),
			 sizeof(client->localAddr));
	return (ret >= 0) ? 0 : ret;
}

/* 0x8130D992: Send signed UDP polling packet and wait for matching response */
static int udpClientTransact(NpPollingUdpClient *client, const NpPollingUdpRequest *req,
			     SceUInt8 *outStatusByte, NpPollingTimestampSet *outTimestamps)
{
	SceUInt32 txSeq = static_cast<SceUInt32>(sceAtomicIncrement32AcqRel(&s_udpTxSequence) + 1);
	if (txSeq == 0)
		txSeq = static_cast<SceUInt32>(sceAtomicIncrement32AcqRel(&s_udpTxSequence) + 1);
	client->lastTxSeq = txSeq;

	SceUInt8 pktBuf[kUdpPacketMaxSize];
	int attempt = 0;

	for (;;) {
		int pktLen = udpBuildPollPacket(
			kUdpHmacKey, sizeof(kUdpHmacKey), client->lastTxSeq,
			&req->npId, req->npEnv, req->resourceName,
			reinterpret_cast<const NpPollingUdpPollExtra *>(req->pollExtra),
			pktBuf, sizeof(pktBuf));
		if (pktLen < 0)
			return pktLen;

		int sent = sceNetSendto(
			client->socketId, pktBuf, static_cast<SceSize>(pktLen), 0,
			reinterpret_cast<const SceNetSockaddr *>(&client->remoteAddr),
			sizeof(client->remoteAddr));
		if (client->aborted != 0)
			return NP_POLLING_ERR_ABORTED;
		if (sent < 0)
			return sent;

		int recvTimeoutUsec = 2000000;
		int optRet = sceNetSetsockopt(client->socketId, SCE_NET_SOL_SOCKET,
					      SCE_NET_SO_RCVTIMEO,
					      &recvTimeoutUsec, sizeof(recvTimeoutUsec));
		if (optRet < 0)
			return optRet;

		for (;;) {
			SceNetSockaddr peerAddr;
			sceClibMemset(&peerAddr, 0, sizeof(peerAddr));
			peerAddr.sa_len = sizeof(peerAddr);
			SceNetSocklen_t peerLen = sizeof(peerAddr);

			int recvd = sceNetRecvfrom(client->socketId, pktBuf, sizeof(pktBuf), 0,
						   &peerAddr, &peerLen);
			if (client->aborted != 0)
				return NP_POLLING_ERR_ABORTED;
			if (recvd == kNetErrWouldBlock || recvd == kNetErrTimedOut)
				break;
			if (recvd < 0)
				return recvd;

			SceUInt32 respSeq = 0;
			int parseRet = udpParsePollResponse(
				pktBuf, static_cast<SceUInt32>(recvd),
				outStatusByte, &respSeq,
				&outTimestamps->messageTick,
				&outTimestamps->friendListTick,
				&outTimestamps->blockListTick);
			if (parseRet < 0)
				return parseRet;
			if (respSeq == client->lastTxSeq)
				return 0;
		}

		if (attempt > 15)
			return NP_POLLING_ERR_UDP_TIMEOUT;
		++attempt;
	}
}

/* 0x8130DB4A: Initial server status check with retry on busy (0x8055220A) */
static int udpClientCheckServerStatus(NpPollingUdpClient *client, const NpPollingUdpRequest *req,
				      int *outServerStatus)
{
	if (client->resolverId < 0 && client->socketId < 0)
		return NP_POLLING_ERR_NOT_CONNECTED;

	int retryCount = 0;
	for (;;) {
		SceUInt8 statusByte = 0;
		NpPollingTimestampSet unusedTicks;
		sceClibMemset(&unusedTicks, 0, sizeof(unusedTicks));

		int ret = udpClientTransact(client, req, &statusByte, &unusedTicks);
		if (ret < 0)
			return ret;

		if (statusByte == 0) {
			*outServerStatus = NP_POLLING_SERVER_OK;
			return 0;
		}

		int mappedErr = static_cast<int>(0x80552200u | statusByte);
		if (mappedErr == kUdpStatusBusy) {
			if (retryCount > 5)
				return NP_POLLING_ERR_SERVER_BUSY;
			++retryCount;
			for (SceUInt32 waitStep = 0; waitStep < 50u; ++waitStep) {
				if (client->aborted != 0)
					return NP_POLLING_ERR_ABORTED;
				sceKernelDelayThread(100000);
			}
			continue;
		}

		if (mappedErr == kUdpStatusExpired) {
			*outServerStatus = NP_POLLING_SERVER_STATUS_EXPIRED;
			return 0;
		}
		if (mappedErr == kUdpStatusDenied) {
			*outServerStatus = NP_POLLING_SERVER_STATUS_DENIED;
			return 0;
		}
		if (mappedErr == kUdpStatusUnavail) {
			*outServerStatus = NP_POLLING_SERVER_STATUS_UNAVAIL;
			return 0;
		}
		return mappedErr;
	}
}

/* 0x8130DC48: Regular UDP timestamp poll */
static int udpClientPollTimestamps(NpPollingUdpClient *client, const NpPollingUdpRequest *req,
				   NpPollingTimestampSet *outTimestamps)
{
	if (client->resolverId < 0 && client->socketId < 0)
		return NP_POLLING_ERR_NOT_CONNECTED;

	SceUInt8 statusByte = 0;
	int ret = udpClientTransact(client, req, &statusByte, outTimestamps);
	if (ret < 0)
		return ret;

	if (statusByte == 0)
		return 0;

	int mappedErr = static_cast<int>(0x80552200u | statusByte);
	return (mappedErr == kUdpStatusBusy) ? 0 : mappedErr;
}

/* --------------------------------------------------------------------------
 * Worker thread & state machine (0x8130DCC0..0x8130DD7D, 0x8130E33C..0x8130E75B,
 * 0x8130EEAC..0x8130F2B3)
 * -------------------------------------------------------------------------- */
/* 0x8130DD56 */
static int pollingWorkerThreadMain(SceSize argSize, void *argBlock)
{
	(void)argSize;
	NpPollingSession *session = *reinterpret_cast<NpPollingSession **>(argBlock);
	SceUID tid = sceKernelGetThreadId();
	if (SceNetInternal_9DBD2F60(tid, 1, 0) >= 0)
		pollingRunStateMachine(session);
	sceKernelExitThread(0);
	return 0;
}

/* 0x8130DCC0 */
static int workerThreadCreate(SceUID *outThreadId, const NpPollingThreadParam *param)
{
	SceUID tid = sceKernelCreateThread("SceNpPollingMain", pollingWorkerThreadMain,
					   param->priority, param->stackSize, 0,
					   param->cpuAffinityMask, NULL);
	if (tid < 0) {
		if (tid != -1)
			sceKernelDeleteThread(tid);
		return tid;
	}
	*outThreadId = tid;
	return 0;
}

/* 0x8130DD10 */
static int workerThreadStart(SceUID *threadId, NpPollingSession *session)
{
	int ret = sceKernelStartThread(*threadId, sizeof(session), &session);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8130DD46 */
static void workerThreadWait(SceUID *threadId)
{
	sceKernelWaitThreadEnd(*threadId, NULL, NULL);
}

/* 0x8130E5AA */
static void pollingKeepAwakeTick(void *arg)
{
	NpPollingSession *session = reinterpret_cast<NpPollingSession *>(arg);
	SceInt64 now = sceKernelGetProcessTimeWide();
	if (now > session->keepAwakeStartTime &&
	    (now - session->keepAwakeStartTime) < kKeepAwakeWindowUsec) {
		sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
	}
}

/* 0x8130F23C */
static void pollingAbortConnectIo(void *arg)
{
	NpPollingSession *session = reinterpret_cast<NpPollingSession *>(arg);
	int lockState = sessionLock(session);
	udpClientAbort(&session->udpClient, 7);
	if (session->httpClient.requestId != 0)
		httpClientAbort(&session->httpClient);
	sessionUnlock(session, lockState);
}

/* 0x8130F26E */
static int pollingCronCallback(SceUID handle, void *arg)
{
	(void)handle;
	NpPollingSession *session = reinterpret_cast<NpPollingSession *>(arg);
	NpPollingPipeCommand cmd;
	sceClibMemset(&cmd, 0, sizeof(cmd));
	cmd.commandId = NP_POLLING_CMD_CRON_TRIGGER;
	SceUInt32 allowedMask = (1u << NP_POLLING_STATE_RUNNING);
	sessionPostCommand(session, &allowedMask, &cmd);
	return 0;
}

/* 0x8130EEAC: Open UDP socket, fetch/verify status XML if needed, check status, register cron */
static int pollingConnect(NpPollingSession *session)
{
	ShellCronOption cronOpt;
	cronOpt.wakeUp = false;
	cronOpt.persist = false;
	SceUID cronHandle = 0;

	int ret = udpClientOpen(&session->udpClient);
	if (ret >= 0) {
		NpPollingStatusConfig *cfg = session->statusConfig;
		bool needFetch = (cfg == NULL) ||
			(cfg->autoIntervalMs == 0 && cfg->intervalMs == 0 &&
			 cfg->autoIntervalPolicy == 0);
		if (needFetch) {
			ret = httpClientOpen(&session->httpClient);
			if (ret >= 0) {
				if (session->statusConfig != NULL) {
					npRtHeapFree(session->statusConfig, NULL);
					session->statusConfig = NULL;
				}
				NpPollingStatusConfig *newCfg =
					reinterpret_cast<NpPollingStatusConfig *>(
						npRtHeapAlloc(sizeof(NpPollingStatusConfig), NULL));
				session->statusConfig = newCfg;
				if (newCfg == NULL) {
					ret = NP_POLLING_ERR_NO_MEMORY;
				} else {
					ret = httpClientFetchStatus(&session->httpClient, newCfg);
					if (ret < 0) {
						npRtHeapFree(session->statusConfig, NULL);
						session->statusConfig = NULL;
					}
				}
				if (ret >= 0)
					httpClientClose(&session->httpClient);
			}
		}
	}

	if (ret >= 0) {
		ret = udpClientResolveAndBind(&session->udpClient,
					      session->endpoint.host,
					      session->endpoint.port);
	}
	if (ret >= 0) {
		ret = timeoutTimerArm(&session->timeoutTimer, kConnectTimeoutUsec,
				      pollingAbortConnectIo, session);
	}
	if (ret >= 0) {
		if (session->checkInitialStatus != 0) {
			NpPollingUdpRequest req;
			sceClibMemset(&req, 0, sizeof(req));
			req.npId = session->npId;
			ret = sceNpCommonGetNpEnviroment(req.npEnv);
			if (ret >= 0) {
				req.npEnv[16] = '\0';
				if (session->resourceName[0] != '\0')
					sceClibMemcpy_safe(req.resourceName,
							   session->resourceName,
							   kResourceNameSize);
				else
					sceClibMemcpy_safe(req.resourceName, "empty", 6u);
				req.pollExtra = NULL;
				ret = udpClientCheckServerStatus(&session->udpClient, &req,
								 &session->serverStatus);
			}
		} else {
			session->serverStatus = NP_POLLING_SERVER_UNCHECKED;
		}
	}
	if (ret >= 0) {
		cronOpt.wakeUp = true;
		ShellCron *cron = shellCronGetInstance();
		SceUInt32 sec = sessionGetAutoIntervalSec(session);
		ret = cron->AddInterval(&cronHandle, pollingCronCallback, session,
					sec, sec, &cronOpt);
		if (ret >= 0) {
			session->cronHandle = cronHandle;
			ret = 0;
		}
	}

	if (ret < 0) {
		if (cronHandle != 0) {
			ShellCron *cron = shellCronGetInstance();
			cron->Remove(cronHandle);
		}
		if (session->httpClient.requestId != 0)
			httpClientClose(&session->httpClient);
		if (timeoutTimerHasExpired(&session->timeoutTimer) != 0)
			ret = NP_POLLING_ERR_CONNECT_TIMEOUT;
	}

	timeoutTimerCancel(&session->timeoutTimer);
	return ret;
}

/* 0x8130F0D0: Execute one UDP timestamp poll and notify listeners if any timestamp changed */
static int pollingExecuteUdpPoll(NpPollingSession *session)
{
	SceInt64 seq = sceAtomicIncrement64AcqRel(&s_pollReqSequence) + 1;
	if (seq == 0)
		seq = sceAtomicIncrement64AcqRel(&s_pollReqSequence) + 1;
	session->pollRequestSeq = static_cast<SceUInt32>(seq);

	NpPollingUdpRequest req;
	sceClibMemset(&req, 0, sizeof(req));
	req.npId = session->npId;

	int ret = sceNpCommonGetNpEnviroment(req.npEnv);
	if (ret < 0)
		return ret;
	req.npEnv[16] = '\0';
	sceClibMemcpy_safe(req.resourceName, session->resourceName, kResourceNameSize);

	NpPollingUdpPollExtra extra;
	sceClibMemset(&extra, 0, sizeof(extra));
	extra.requestSeq = session->pollRequestSeq;
	if (session->pollingMode == NP_POLLING_MODE_INTERVAL)
		extra.pollKind = 3;
	else if (session->pollingMode == NP_POLLING_MODE_CRON)
		extra.pollKind = 4;
	else
		return NP_POLLING_ERR_INTERNAL;
	req.pollExtra = &extra;

	NpPollingTimestampSet freshTicks;
	sceClibMemset(&freshTicks, 0, sizeof(freshTicks));
	ret = udpClientPollTimestamps(&session->udpClient, &req, &freshTicks);
	if (ret < 0)
		return ret;

	if (session->lastTicks.messageTick.tick != freshTicks.messageTick.tick ||
	    session->lastTicks.friendListTick.tick != freshTicks.friendListTick.tick ||
	    session->lastTicks.blockListTick.tick != freshTicks.blockListTick.tick) {
		session->lastTicks = freshTicks;
		NpPollingSessionIdentity *identity =
			reinterpret_cast<NpPollingSessionIdentity *>(&session->sessionId);
		notifyUpdateCallbacks(identity, 0, &freshTicks);
	}
	return 0;
}

/* 0x8130E33C: Transition session state and dispatch state-change callbacks */
static int pollingTransitionState(NpPollingSession *session, int nextState, int resultCode)
{
	int lockState = sessionLock(session);
	if (lockState < 0)
		return lockState;

	int prevState = session->state;
	session->state = nextState;
	sessionUnlock(session, lockState);

	if (nextState == NP_POLLING_STATE_ERROR || nextState == NP_POLLING_STATE_STOPPED) {
		if (udpClientIsOpen(&session->udpClient) != 0)
			udpClientClose(&session->udpClient);
		int drainRet = sessionDrainPipe(session);
		if (drainRet < 0)
			return drainRet;
	}

	NpPollingSessionIdentity *identity =
		reinterpret_cast<NpPollingSessionIdentity *>(&session->sessionId);

	switch (nextState) {
	case NP_POLLING_STATE_ERROR:
		if (prevState != -1 && prevState != NP_POLLING_STATE_ERROR &&
		    prevState != NP_POLLING_STATE_STOPPED)
			notifyStateCallbacks(identity, NP_POLLING_EVENT_ERROR, resultCode);
		break;
	case NP_POLLING_STATE_CONNECTING:
		notifyStateCallbacks(identity, NP_POLLING_EVENT_CONNECTING, resultCode);
		break;
	case NP_POLLING_STATE_RUNNING:
		notifyStateCallbacks(identity, NP_POLLING_EVENT_CONNECTED, resultCode);
		break;
	case NP_POLLING_STATE_CLOSING:
		notifyStateCallbacks(identity, NP_POLLING_EVENT_CLOSING, resultCode);
		break;
	case NP_POLLING_STATE_STOPPED:
		if (prevState != -1 && prevState != NP_POLLING_STATE_ERROR &&
		    prevState != NP_POLLING_STATE_STOPPED)
			notifyStateCallbacks(identity, NP_POLLING_EVENT_STOPPED, resultCode);
		break;
	default:
		break;
	}
	return 0;
}

/* 0x8130E504: State 1 (IDLE) handler */
static int stateIdleHandler(NpPollingSession *session, int *nextState)
{
	NpPollingPipeCommand cmd;
	resetPipeCommand(&cmd);

	int ret = sessionReceiveCommand(session, &cmd, NULL);
	if (ret >= 0) {
		if (cmd.commandId == NP_POLLING_CMD_START) {
			session->checkInitialStatus = cmd.firstOption;
			session->pollingMode = cmd.secondOption;
			if (cmd.resourceName[0] != '\0')
				sceClibMemcpy_safe(session->resourceName,
						   cmd.resourceName, kResourceNameSize);
			sessionRefreshPollTimeout(session);
			*nextState = NP_POLLING_STATE_CONNECTING;
		}
		ret = 0;
	}
	resetPipeCommand(&cmd);
	return ret;
}

/* 0x8130E594: State 2 (CONNECTING) handler */
static int stateConnectingHandler(NpPollingSession *session, int *nextState)
{
	int ret = pollingConnect(session);
	if (ret >= 0) {
		*nextState = NP_POLLING_STATE_RUNNING;
		return 0;
	}
	return ret;
}

/* 0x8130E5F2: State 3 (RUNNING) handler */
static int stateRunningHandler(NpPollingSession *session, int *nextState)
{
	NpPollingPipeCommand cmd;
	resetPipeCommand(&cmd);

	int ret = 0;
	if (session->pollingMode == NP_POLLING_MODE_INTERVAL) {
		ret = sessionReceiveCommand(session, &cmd, &session->pollTimeoutUsec);
		if (ret == NP_POLLING_ERR_PIPE_TIMEOUT) {
			ret = pollingExecuteUdpPoll(session);
			if (ret >= 0) {
				sessionRefreshPollTimeout(session);
				ret = 0;
			}
			resetPipeCommand(&cmd);
			return ret;
		}
	} else {
		ret = sessionReceiveCommand(session, &cmd, NULL);
	}

	if (ret < 0) {
		resetPipeCommand(&cmd);
		return ret;
	}

	if (cmd.commandId == NP_POLLING_CMD_STOP) {
		*nextState = NP_POLLING_STATE_CLOSING;
		ret = 0;
	} else if (cmd.commandId == NP_POLLING_CMD_CHANGE_MODE) {
		session->pollingMode = cmd.firstOption;
		if (cmd.resourceName[0] != '\0')
			sceClibMemcpy_safe(session->resourceName,
					   cmd.resourceName, kResourceNameSize);
		sessionRefreshPollTimeout(session);
		ret = 0;
	} else if (cmd.commandId == NP_POLLING_CMD_CRON_TRIGGER) {
		if (session->pollingMode == NP_POLLING_MODE_CRON) {
			session->keepAwakeStartTime = sceKernelGetProcessTimeWide();
			paf::common::MainThreadCallList::Register(pollingKeepAwakeTick, session);
			ret = pollingExecuteUdpPoll(session);
			paf::common::MainThreadCallList::Unregister(pollingKeepAwakeTick, session);
			if (ret >= 0) {
				sessionRefreshPollTimeout(session);
				ret = 0;
			}
		} else {
			ret = 0;
		}
	} else {
		ret = 0;
	}

	resetPipeCommand(&cmd);
	return ret;
}

/* 0x8130E738: State 4 (CLOSING) handler */
static int stateClosingHandler(NpPollingSession *session, int *nextState)
{
	int ret = udpClientClose(&session->udpClient);
	if (ret >= 0) {
		*nextState = NP_POLLING_STATE_STOPPED;
		return 0;
	}
	return ret;
}

/* 0x8130E750: State 0 / 5 (ERROR / STOPPED) handler */
static int stateInvalidHandler(NpPollingSession *session, int *nextState)
{
	(void)session;
	(void)nextState;
	return NP_POLLING_ERR_BAD_TRANSITION;
}

typedef int (*NpPollingStateHandler)(NpPollingSession *session, int *nextState);

/* 0x8154615C */
static const NpPollingStateHandler kStateHandlers[6] = {
	stateInvalidHandler,
	stateIdleHandler,
	stateConnectingHandler,
	stateRunningHandler,
	stateClosingHandler,
	stateInvalidHandler
};

/* 0x8130E44A */
static int pollingRunStateMachine(NpPollingSession *session)
{
	int ret = 0;
	for (;;) {
		int lockState = sessionLock(session);
		bool stopped = false;
		if (lockState >= 0) {
			stopped = (session->state == NP_POLLING_STATE_STOPPED);
			sessionUnlock(session, lockState);
		} else {
			ret = lockState;
			break;
		}

		if (stopped)
			return 0;

		int nextState = session->state;
		if (nextState != NP_POLLING_STATE_CLOSING && session->aborted != 0) {
			ret = NP_POLLING_ERR_ABORTED;
			break;
		}

		ret = kStateHandlers[nextState](session, &nextState);
		if (ret < 0)
			break;

		if (session->state != nextState) {
			ret = pollingTransitionState(session, nextState, ret);
			if (ret < 0)
				break;
		}
	}

	pollingTransitionState(session, NP_POLLING_STATE_ERROR, ret);
	return 0;
}

/* --------------------------------------------------------------------------
 * Session Lifecycle Management (0x8130CB38..0x8130D143)
 * -------------------------------------------------------------------------- */
/* 0x8130CB38 */
static void sessionWaitWorker(NpPollingSession *session)
{
	if (session->workerThreadPtr != NULL) {
		workerThreadWait(session->workerThreadPtr);
		session->workerThreadPtr = NULL;
	}
}

/* 0x8130CB4E */
static void sessionAbort(NpPollingSession *session)
{
	int lockState = sessionLock(session);
	session->aborted = 1;
	udpClientAbort(&session->udpClient, 7);
	pipeSendAbort(&session->msgPipeId);
	sessionUnlock(session, lockState);
}

/* 0x8130CB7A */
static void sessionDestroy(NpPollingSession *session)
{
	sessionAbort(session);
	sessionWaitWorker(session);
	timeoutTimerCancel(&session->timeoutTimer);

	if (session->cronHandle != 0) {
		ShellCron *cron = shellCronGetInstance();
		cron->Remove(session->cronHandle);
	}

	udpClientTerm(&session->udpClient);
	if (session->statusConfig != NULL)
		npRtHeapFree(session->statusConfig, NULL);
	httpClientTerm(&session->httpClient);
	pipeDelete(&session->msgPipeId);
	sceKernelDeleteLwMutex(&session->mutex);
	npRtHeapFree(session, NULL);
}

/* 0x8130CC0A */
static int sessionCreate(const SceNpId *npId, const NpPollingEndpoint *endpoint,
			 const NpPollingTimestampSet *initialTicks,
			 const NpPollingStatusConfig *initialConfig,
			 NpPollingSession **outSession)
{
	if (s_sessionMgrInit == 0)
		return NP_POLLING_ERR_NOT_INITIALIZED;

	NpPollingSession *session = reinterpret_cast<NpPollingSession *>(
		npRtHeapAlloc(sizeof(NpPollingSession), NULL));
	if (session == NULL)
		return NP_POLLING_ERR_NO_MEMORY;

	sceClibMemset(session, 0, sizeof(*session));

	int initStage = 0;
	int ret = sceKernelCreateLwMutex(&session->mutex, "SceNpPollingSession", 0x2002u, 0, NULL);
	if (ret >= 0) {
		initStage = 1;
		session->npId = *npId;
		session->endpoint = *endpoint;
		session->state = NP_POLLING_STATE_IDLE;
		session->checkInitialStatus = 0;
		session->pollingMode = NP_POLLING_MODE_NONE;
		session->pollTimeoutUsec = 0;
		session->cronHandle = 0;

		ret = pipeCreate(&session->msgPipeId);
		if (ret >= 0) {
			initStage = 3;
			udpClientInit(&session->udpClient);
			initStage = 7;

			session->statusConfig = reinterpret_cast<NpPollingStatusConfig *>(
				npRtHeapAlloc(sizeof(NpPollingStatusConfig), NULL));
			if (session->statusConfig == NULL) {
				ret = NP_POLLING_ERR_NO_MEMORY;
			} else {
				*session->statusConfig = *initialConfig;
				httpClientInit(&session->httpClient);
				timeoutTimerInit(&session->timeoutTimer);
				session->workerThreadPtr = NULL;
				session->aborted = 0;

				int nextId = ++s_nextSessionId;
				if (nextId == 0)
					nextId = ++s_nextSessionId;
				session->sessionId = nextId;
				session->lastTicks = *initialTicks;
				ret = 0;
			}
		}
	}

	if (ret < 0) {
		if (session->statusConfig != NULL)
			npRtHeapFree(session->statusConfig, NULL);
		if ((initStage & 4) != 0)
			udpClientTerm(&session->udpClient);
		if ((initStage & 2) != 0)
			pipeDelete(&session->msgPipeId);
		if ((initStage & 1) != 0)
			sceKernelDeleteLwMutex(&session->mutex);
		npRtHeapFree(session, NULL);
		return ret;
	}

	++session->refCount;
	int globalLock = pollingLock();
	if (globalLock < 0) {
		sessionDestroy(session);
		return globalLock;
	}

	++s_sessionCount;
	int sessLock = sessionLock(session);
	if (sessLock < 0) {
		pollingUnlock(globalLock);
		sessionDestroy(session);
		return sessLock;
	}

	session->next = NULL;
	session->prev = s_sessionTail;
	if (s_sessionTail != NULL)
		s_sessionTail->next = session;
	if (s_sessionHead == NULL)
		s_sessionHead = session;
	s_sessionTail = session;
	session->sessionFlags |= NP_POLLING_SFLAG_LINKED;

	sessionUnlock(session, sessLock);
	pollingUnlock(globalLock);
	*outSession = session;
	return 0;
}

/* 0x8130CE7E */
static int sessionAcquireById(int sessionId, NpPollingSession **outSession)
{
	if (s_sessionMgrInit == 0)
		return NP_POLLING_ERR_NOT_INITIALIZED;

	int globalLock = pollingLock();
	if (globalLock < 0)
		return globalLock;

	NpPollingSession *found = NULL;
	for (NpPollingSession *cur = s_sessionHead; cur != NULL; cur = cur->next) {
		if (cur->sessionId == sessionId) {
			found = cur;
			break;
		}
	}

	int ret = 0;
	if (found != NULL) {
		int sessLock = sessionLock(found);
		if (sessLock >= 0) {
			if ((found->sessionFlags & NP_POLLING_SFLAG_CLOSING) != 0) {
				ret = NP_POLLING_ERR_SESSION_INVALID;
			} else {
				++found->refCount;
				*outSession = found;
			}
			sessionUnlock(found, sessLock);
		} else {
			ret = sessLock;
		}
	} else {
		ret = NP_POLLING_ERR_SESSION_INVALID;
	}

	pollingUnlock(globalLock);
	return ret;
}

static void unlinkSessionLocked(NpPollingSession *session)
{
	if ((session->sessionFlags & NP_POLLING_SFLAG_LINKED) == 0)
		return;

	if (session->next != NULL)
		session->next->prev = session->prev;
	else
		s_sessionTail = session->prev;

	if (session->prev != NULL)
		session->prev->next = session->next;
	else
		s_sessionHead = session->next;

	session->next = NULL;
	session->prev = NULL;
	session->sessionFlags &= ~NP_POLLING_SFLAG_LINKED;
}

/* 0x8130CF18 */
static int sessionRelease(NpPollingSession *session)
{
	if (s_sessionMgrInit == 0)
		return NP_POLLING_ERR_NOT_INITIALIZED;

	int globalLock = pollingLock();
	if (globalLock < 0)
		return globalLock;

	NpPollingSession *toDestroy = NULL;
	int ret = 0;
	int sessLock = sessionLock(session);
	if (sessLock >= 0) {
		--session->refCount;
		if (session->refCount == 0 &&
		    (session->sessionFlags & NP_POLLING_SFLAG_CLOSING) != 0) {
			unlinkSessionLocked(session);
			toDestroy = session;
		}
		sessionUnlock(session, sessLock);
	} else {
		ret = sessLock;
	}

	pollingUnlock(globalLock);
	if (toDestroy != NULL)
		sessionDestroy(toDestroy);
	return ret;
}

/* 0x8130CFDC */
static int sessionMarkClosing(NpPollingSession *session)
{
	if (s_sessionMgrInit == 0)
		return NP_POLLING_ERR_NOT_INITIALIZED;

	int globalLock = pollingLock();
	if (globalLock < 0)
		return globalLock;

	NpPollingSession *toDestroy = NULL;
	int ret = 0;
	int sessLock = sessionLock(session);
	if (sessLock >= 0) {
		session->sessionFlags |= NP_POLLING_SFLAG_CLOSING;
		--session->refCount;
		if (session->refCount == 0) {
			unlinkSessionLocked(session);
			toDestroy = session;
		}
		sessionUnlock(session, sessLock);
	} else {
		ret = sessLock;
	}

	pollingUnlock(globalLock);
	if (toDestroy != NULL)
		sessionDestroy(toDestroy);
	return ret;
}

/* 0x8130D0A8 */
static int sessionStartWorker(NpPollingSession *session)
{
	int ret = workerThreadStart(&s_workerThreadId, session);
	if (ret >= 0) {
		session->workerThreadPtr = &s_workerThreadId;
		return 0;
	}
	return ret;
}

/* 0x8130D0CE */
static int sessionMgrInit(const NpPollingThreadParam *threadParam)
{
	if (s_sessionMgrInit != 0)
		return NP_POLLING_ERR_ALREADY_STARTED;

	s_sessionHead = NULL;
	s_sessionTail = NULL;
	s_sessionCount = 0;
	s_nextSessionId = 0;
	s_sessionMgrInit = 1;

	int ret = workerThreadCreate(&s_workerThreadId, threadParam);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8130D10E */
static int sessionMgrTerm(void)
{
	if (s_sessionMgrInit == 0)
		return NP_POLLING_ERR_NOT_INITIALIZED;

	while (s_sessionHead != NULL)
		sessionMarkClosing(s_sessionHead);

	npRtJoinAndDeleteThread(&s_workerThreadId);
	s_sessionCount = 0;
	s_sessionMgrInit = 0;
	return 0;
}

} /* namespace */

/* --------------------------------------------------------------------------
 * Public Module Lifecycle & Callback Registry (0x8130C7E8..0x8130CB37)
 * -------------------------------------------------------------------------- */
/* 0x8130C7E8 */
int npPollingInit(const NpPollingThreadParam *threadParam)
{
	int ret = pollingMutexInit();
	if (ret >= 0) {
		ret = sessionMgrInit(threadParam);
		if (ret >= 0) {
			ret = callbackRegistryInit();
			if (ret >= 0)
				return 0;
		}
	}
	callbackRegistryTerm();
	sessionMgrTerm();
	pollingMutexTerm();
	return ret;
}

/* 0x8130C820 */
int npPollingTerm(void)
{
	callbackRegistryTerm();
	sessionMgrTerm();
	return pollingMutexTerm();
}

/* 0x8130C9AE */
int npPollingRegisterCallbacks(const NpPollingCallbackTable *callbacks, int *outCallbackId)
{
	if (s_cbCount >= kMaxCallbacks)
		return NP_POLLING_ERR_CB_FULL;

	NpPollingCallbackNode *node = reinterpret_cast<NpPollingCallbackNode *>(
		npRtHeapAlloc(sizeof(NpPollingCallbackNode), NULL));
	if (node == NULL)
		return NP_POLLING_ERR_NO_MEMORY;

	sceClibMemset(node, 0, sizeof(*node));
	node->table = *callbacks;
	++s_cbCount;

	int cbId = ++s_nextCallbackId;
	if (cbId == 0)
		cbId = ++s_nextCallbackId;
	node->callbackId = cbId;

	node->next = NULL;
	node->prev = s_cbListTail;
	if (s_cbListTail != NULL)
		s_cbListTail->next = node;
	if (s_cbListHead == NULL)
		s_cbListHead = node;
	s_cbListTail = node;

	*outCallbackId = node->callbackId;
	return 0;
}

/* 0x8130CAC8 */
int npPollingUnregisterCallbacks(int callbackId)
{
	NpPollingCallbackNode *found = NULL;
	for (NpPollingCallbackNode *cur = s_cbListHead; cur != NULL; cur = cur->next) {
		if (cur->callbackId == callbackId) {
			found = cur;
			break;
		}
	}
	if (found == NULL)
		return NP_POLLING_ERR_CB_NOT_FOUND;

	if (found->next != NULL)
		found->next->prev = found->prev;
	else
		s_cbListTail = found->prev;

	if (found->prev != NULL)
		found->prev->next = found->next;
	else
		s_cbListHead = found->next;

	found->next = NULL;
	found->prev = NULL;
	npRtHeapFree(found, NULL);
	--s_cbCount;
	return 0;
}

/* --------------------------------------------------------------------------
 * Public Session API Facade (0x813F3DBC..0x813F3FFF)
 * -------------------------------------------------------------------------- */
/* 0x813F3DBC */
int npPollingCreateSession(const SceNpId *npId, const NpPollingEndpoint *endpoint,
			   const NpPollingTimestampSet *initialTicks,
			   const NpPollingStatusConfig *initialConfig,
			   int *outSessionId)
{
	NpPollingSession *session = NULL;
	int ret = sessionCreate(npId, endpoint, initialTicks, initialConfig, &session);
	if (ret < 0)
		return ret;

	ret = sessionStartWorker(session);
	if (ret < 0) {
		sessionMarkClosing(session);
		return ret;
	}

	*outSessionId = session->sessionId;
	return 0;
}

/* 0x813F3E10 */
int npPollingDestroySession(int sessionId)
{
	NpPollingSession *session = NULL;
	int ret = sessionAcquireById(sessionId, &session);
	if (ret < 0)
		return ret;

	sessionMarkClosing(session);
	sessionAbort(session);
	sessionWaitWorker(session);
	sessionRelease(session);
	return 0;
}

/* 0x813F3E5A */
int npPollingStartSession(int sessionId, int checkInitialStatus, int pollingMode,
			  const char *resourceName)
{
	NpPollingSession *session = NULL;
	int ret = sessionAcquireById(sessionId, &session);
	if (ret >= 0) {
		NpPollingPipeCommand cmd;
		sceClibMemset(&cmd, 0, sizeof(cmd));
		cmd.commandId = NP_POLLING_CMD_START;
		cmd.firstOption = checkInitialStatus;
		cmd.secondOption = pollingMode;
		if (resourceName != NULL)
			sceClibMemcpy_safe(cmd.resourceName, resourceName, kResourceNameSize);

		SceUInt32 allowedMask = (1u << NP_POLLING_STATE_IDLE);
		ret = sessionPostCommand(session, &allowedMask, &cmd);
		if (ret >= 0)
			ret = 0;
	}
	if (session != NULL)
		sessionRelease(session);
	return ret;
}

/* 0x813F3EDC */
int npPollingGetStatusConfig(int sessionId, NpPollingStatusConfig *outConfig)
{
	NpPollingSession *session = NULL;
	int ret = sessionAcquireById(sessionId, &session);
	if (ret >= 0) {
		sceClibMemcpy_safe(outConfig, session->statusConfig, sizeof(*outConfig));
		ret = 0;
	}
	if (session != NULL)
		sessionRelease(session);
	return ret;
}

/* 0x813F3F2A */
int npPollingGetServerStatus(int sessionId, int *outServerStatus)
{
	NpPollingSession *session = NULL;
	int ret = sessionAcquireById(sessionId, &session);
	if (ret >= 0) {
		*outServerStatus = session->serverStatus;
		ret = 0;
	}
	if (session != NULL)
		sessionRelease(session);
	return ret;
}

/* 0x813F3F72 */
int npPollingUpdateMode(int sessionId, int pollingMode, const char *resourceName)
{
	NpPollingSession *session = NULL;
	int ret = sessionAcquireById(sessionId, &session);
	if (ret >= 0) {
		NpPollingPipeCommand cmd;
		sceClibMemset(&cmd, 0, sizeof(cmd));
		cmd.commandId = NP_POLLING_CMD_CHANGE_MODE;
		cmd.firstOption = pollingMode;
		if (resourceName != NULL)
			sceClibMemcpy_safe(cmd.resourceName, resourceName, kResourceNameSize);

		SceUInt32 allowedMask = (1u << NP_POLLING_STATE_RUNNING);
		ret = sessionPostCommand(session, &allowedMask, &cmd);
		if (ret >= 0)
			ret = 0;
	}
	if (session != NULL)
		sessionRelease(session);
	return ret;
}
