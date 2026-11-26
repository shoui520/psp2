/*
 * rt_netinfo_udpp2p.cpp: SceShell NP real-time netinfo & udpp2p signaling bridge
 * over XMPP IQ XML stanzas (0x81224FD4 - 0x81226C10).
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <np_common/np_common.h>
#include "np_ipc_internal.h"

#define NP_RT_SIGNALING_CHANNEL		1
#define NP_RT_SIGNALING_HEADER_SIZE	92
#define NP_RT_SIGNALING_ERROR_SIZE	84
#define NP_RT_SIGNALING_MSG_MAX		512
#define NP_RT_AF_INET			2

#define NP_RT_IPC_TYPE_GET		1
#define NP_RT_IPC_TYPE_SET		2
#define NP_RT_IPC_TYPE_RESULT		3
#define NP_RT_IPC_TYPE_ERROR		4

#define NP_RT_STANZA_ERROR		1
#define NP_RT_STANZA_IQ_GET		14
#define NP_RT_STANZA_IQ_RESULT		15
#define NP_RT_STANZA_IQ_SET		16

/* Signaling operation codes */
enum NpRtSignalingOp {
	NP_RT_OP_OFFER		= 1,
	NP_RT_OP_ACCEPT		= 2,
	NP_RT_OP_TERMINATE	= 3,
	NP_RT_OP_ACK		= 4,
	NP_RT_OP_NETINFO_REQ	= 5,
	NP_RT_OP_NETINFO_RES	= 6,
	NP_RT_OP_ACTIVATED	= 7,
	NP_RT_OP_DEACTIVATED	= 8,
	NP_RT_OP_FTERMINATE	= 9,
};

/* UDPP2P candidate types */
enum NpUdpP2pCandidateType {
	NP_UDPP2P_CAND_LOCAL	= 1,
	NP_UDPP2P_CAND_DERIVED	= 2,
	NP_UDPP2P_CAND_STUN	= 3,
	NP_UDPP2P_CAND_STATIC	= 4,
};

/* 12-byte netinfo payload (local IPv4, mapped IPv4, NAT type) */
struct NpRtNetInfoPayload {
	SceUInt32 localAddr;		/* +0x00 */
	SceUInt32 mappedAddr;		/* +0x04 */
	int natType;			/* +0x08 (0=unset, 1..3=NAT open/moderate/strict) */
};

/* 16-byte UDPP2P candidate entry */
struct NpUdpP2pCandidate {
	int candidateType;		/* +0x00 */
	SceUInt32 addr;			/* +0x04 */
	SceUInt32 mappedAddr;		/* +0x08 */
	SceUInt16 port;			/* +0x0C (network byte order) */
	SceUInt16 mappedPort;		/* +0x0E (network byte order) */
};

/* 152-byte UDPP2P signaling payload */
struct NpUdpP2pPayload {
	SceUInt16 sid;			/* +0x00 */
	SceUInt16 psid;			/* +0x02 */
	SceUInt8 skey[16];		/* +0x04 */
	SceUInt16 natType;		/* +0x14 */
	SceUInt16 reserved;		/* +0x16 */
	NpUdpP2pCandidate candidates[8]; /* +0x18 (8 * 16 = 128 bytes) */
};

/* 40-byte pending signaling stanza node */
struct NpRtPendingNode {
	NpRtPendingNode *next;		/* +0x00 */
	int requestId;			/* +0x04 */
	char stanzaId[32];		/* +0x08 */
};

/* Raw IPC messages for channel 1 */
struct NpRtServerToClientMsg {
	int msgType;			/* +0x00 */
	int requestId;			/* +0x04 */
	SceNpId fromNpId;		/* +0x08 (36 bytes) */
	SceNpId toNpId;			/* +0x2C (36 bytes) */
	SceSize totalPayloadSize;	/* +0x50 (payloadSize + 8) */
	int opCode;			/* +0x54 */
	SceSize payloadSize;		/* +0x58 */
	SceUInt8 payload[NP_RT_SIGNALING_MSG_MAX - NP_RT_SIGNALING_HEADER_SIZE];
};

struct NpRtErrorNotifyMsg {
	int msgType;			/* +0x00 */
	int transactionId;		/* +0x04 */
	int errorCode;			/* +0x08 */
	SceUInt8 reserved[72];		/* +0x0C */
};

struct NpRtClientRequestMsg {
	int msgType;			/* +0x00 (1 or 2) */
	int reserved;			/* +0x04 */
	int paramFirst;			/* +0x08 */
	int paramSecond;		/* +0x0C */
	SceNpId fromNpId;		/* +0x10 (36 bytes) */
	SceNpId toNpId;			/* +0x34 (36 bytes) */
	SceSize totalPayloadSize;	/* +0x58 */
	int opCode;			/* +0x5C */
	SceSize payloadSize;		/* +0x60 */
	SceUInt8 payload[NP_RT_SIGNALING_MSG_MAX - 100];
};

struct NpRtClientResultMsg {
	int msgType;			/* +0x00 (3) */
	int reserved;			/* +0x04 */
	int requestId;			/* +0x08 */
	SceNpId fromNpId;		/* +0x0C (36 bytes) */
	SceNpId toNpId;			/* +0x30 (36 bytes) */
	SceSize totalPayloadSize;	/* +0x54 */
	int opCode;			/* +0x58 */
	SceSize payloadSize;		/* +0x5C */
	SceUInt8 payload[NP_RT_SIGNALING_MSG_MAX - 96];
};

struct NpRtClientErrorMsg {
	int msgType;			/* +0x00 (4) */
	int reserved;			/* +0x04 */
	int requestId;			/* +0x08 */
	int errorCode;			/* +0x0C */
	SceNpId fromNpId;		/* +0x10 (36 bytes) */
	SceNpId toNpId;			/* +0x34 (36 bytes) */
};

/* Namespace dispatch entry (12 bytes, 0x81545500) */
struct NpRtNamespaceEntry {
	const char *namespaceUri;
	int (*onRecvRequest)(const NpRtStanza *stanza);
	int (*onRecvResponse)(const NpRtStanza *stanza, int transactionId);
};

typedef int (*NpRtRecvRequestCb)(int stanzaType, const char *stanzaId, const NpRtJidPair *npIds,
				 int opCode, const void *payload, SceSize payloadSize);
typedef int (*NpRtRecvResultCb)(int transactionId, const NpRtJidPair *npIds,
				int opCode, const void *payload, SceSize payloadSize);
typedef void (*NpRtRecvErrorCb)(int transactionId, int errorCode);

static const char s_nsNetInfo[] = "http://playstation.net/np/rt/netinfo";
static const char s_nsUdpP2p[]  = "http://playstation.net/np/rt/udpp2p";

static const char *const s_rtNamespaces[] = {		/* 0x81545380 */
	s_nsNetInfo,
	s_nsUdpP2p,
	NULL,
};

static IPMI::Session *s_rtClientSession = NULL;		/* 0x8154536C */
static int s_rtNextRequestId = 1;			/* 0x81545370 */
static NpRtPendingNode *s_rtPendingHead = NULL;		/* 0x81545374 */
static int s_rtHandlerId = 0;				/* 0x81545378 */
static NpRtRecvErrorCb s_rtOnError = NULL;		/* 0x8154537C */
static NpRtRecvRequestCb s_rtOnRequest = NULL;		/* 0x815454F8 */
static NpRtRecvResultCb s_rtOnResult = NULL;		/* 0x815454FC */

/* Forward declarations */
static int npRtSignalingOnResult(int transactionId, const NpRtJidPair *npIds,
				 int opCode, const void *payload, SceSize payloadSize);
static int npRtSignalingOnRequest(int stanzaType, const char *stanzaId, const NpRtJidPair *npIds,
				  int opCode, const void *payload, SceSize payloadSize);
static void npRtSignalingOnError(int transactionId, int errorCode);
static void npRtSignalingOnClientConnected(IPMI::Session *session);
static void npRtSignalingOnClientDisconnected(IPMI::Session *session);
static int npRtSignalingOnClientMessage(IPMI::Session *session, const void *msgBuf);
static int npRtSignalingRegister(NpRtRecvRequestCb onReq, NpRtRecvResultCb onRes,
				 NpRtRecvErrorCb onErr);
static int npRtSignalingSendError(const char *stanzaId, const NpRtJidPair *npIds, int errorCode);
static int npRtSignalingSendAck(const char *stanzaId, const NpRtJidPair *npIds);
static int npRtSignalingSendUdpP2p(const NpRtJidPair *npIds, int opCode,
				   const NpUdpP2pPayload *payload);
static int npRtSignalingSendNetInfo(const char *stanzaId, const NpRtJidPair *npIds, int opCode,
				    const NpRtNetInfoPayload *payload);
static void npRtSignalingRtOnStateNoop(const NpRtSessionIdentity *session, int eventState,
				       int resultCode);
static void npRtSignalingRtOnRecv(const NpRtSessionIdentity *session, NpRtStanza *stanza);
static void npRtSignalingRtOnIqResponse(const NpRtSessionIdentity *session, int transactionId,
					int result, NpRtStanza *response, void *userArg);
static int npRtNetInfoOnRequest(const NpRtStanza *stanza);
static int npRtNetInfoOnResponse(const NpRtStanza *stanza, int transactionId);
static int npRtUdpP2pOnRequest(const NpRtStanza *stanza);
static int npRtUdpP2pOnResponse(const NpRtStanza *stanza, int transactionId);

static const NpRtNamespaceEntry s_rtNsTable[] = {	/* 0x81545500 */
	{ s_nsNetInfo, npRtNetInfoOnRequest, npRtNetInfoOnResponse },
	{ s_nsUdpP2p,  npRtUdpP2pOnRequest,  npRtUdpP2pOnResponse  },
	{ NULL,        NULL,                 NULL                  },
};

static const NpRtNamespaceEntry *npRtFindNamespace(const char *uri)
{
	const NpRtNamespaceEntry *entry = s_rtNsTable;
	while (entry->namespaceUri != NULL) {
		if (sceClibStrcmp(uri, entry->namespaceUri) == 0)
			return entry;
		entry++;
	}
	return NULL;
}

static NpRtPendingNode *npRtFindPendingById(int requestId)
{
	NpRtPendingNode *curr = s_rtPendingHead;
	while (curr != NULL) {
		if (curr->requestId == requestId)
			return curr;
		curr = curr->next;
	}
	return NULL;
}

static void npRtRemovePendingById(int requestId)
{
	NpRtPendingNode *curr = s_rtPendingHead;
	NpRtPendingNode *prev = NULL;

	while (curr != NULL) {
		if (curr->requestId == requestId)
			break;
		prev = curr;
		curr = curr->next;
	}
	if (curr != NULL) {
		if (prev != NULL)
			prev->next = curr->next;
		else
			s_rtPendingHead = curr->next;
		npRtHeapFree(curr, NULL);
	}
}

/* 0x81224FD4 */
int npRtSignalingServerInit(void)
{
	NpImpsRawMsgServerCallbacks callbacks;
	int ret;

	callbacks.onClientConnected = npRtSignalingOnClientConnected;
	callbacks.onClientDisconnected = npRtSignalingOnClientDisconnected;
	callbacks.onClientMessage = npRtSignalingOnClientMessage;

	s_rtClientSession = NULL;
	ret = npImpsRawMsgRegisterChannel(NP_RT_SIGNALING_CHANNEL, &callbacks);
	if (ret < 0)
		return ret;

	ret = npRtSignalingRegister(npRtSignalingOnRequest, npRtSignalingOnResult,
				    npRtSignalingOnError);
	if (ret < 0) {
		npImpsRawMsgUnregisterChannel(NP_RT_SIGNALING_CHANNEL);
		return ret;
	}
	return 0;
}

/* 0x8122505E */
static int npRtSignalingHandleClientMsg(const void *msgBuf)
{
	const int *header = (const int *)msgBuf;
	int msgType = header[0];
	NpRtJidPair npIds;

	sceClibMemset(&npIds, 0, sizeof(npIds));

	if (msgType == NP_RT_IPC_TYPE_ERROR) {
		const NpRtClientErrorMsg *errMsg = (const NpRtClientErrorMsg *)msgBuf;
		NpRtPendingNode *node = npRtFindPendingById(errMsg->requestId);
		if (node == NULL)
			return NP_JGA_ERR_INVALID_ID;

		npIds.selfNpId = &errMsg->fromNpId;
		npIds.peerNpId = &errMsg->toNpId;
		int ret = npRtSignalingSendError(node->stanzaId, &npIds, errMsg->errorCode);
		npRtRemovePendingById(errMsg->requestId);
		return ret;
	}

	int opCode;
	const void *payloadPtr;
	int requestId = 0;

	if (msgType == NP_RT_IPC_TYPE_GET || msgType == NP_RT_IPC_TYPE_SET) {
		const NpRtClientRequestMsg *reqMsg = (const NpRtClientRequestMsg *)msgBuf;
		npIds.selfNpId = &reqMsg->fromNpId;
		npIds.peerNpId = &reqMsg->toNpId;
		opCode = reqMsg->opCode;
		payloadPtr = reqMsg->payload;
	} else if (msgType == NP_RT_IPC_TYPE_RESULT) {
		const NpRtClientResultMsg *resMsg = (const NpRtClientResultMsg *)msgBuf;
		npIds.selfNpId = &resMsg->fromNpId;
		npIds.peerNpId = &resMsg->toNpId;
		requestId = resMsg->requestId;
		opCode = resMsg->opCode;
		payloadPtr = resMsg->payload;
	} else {
		return NP_JGA_ERR_INVALID_TYPE;
	}

	switch (opCode) {
	case NP_RT_OP_OFFER:
	case NP_RT_OP_ACCEPT:
	case NP_RT_OP_TERMINATE:
	case NP_RT_OP_ACTIVATED:
	case NP_RT_OP_DEACTIVATED:
		return npRtSignalingSendUdpP2p(&npIds, opCode, (const NpUdpP2pPayload *)payloadPtr);

	case NP_RT_OP_ACK: {
		NpRtPendingNode *node = npRtFindPendingById(requestId);
		if (node == NULL)
			return NP_JGA_ERR_INVALID_ID;
		int ret = npRtSignalingSendAck(node->stanzaId, &npIds);
		npRtRemovePendingById(requestId);
		return ret;
	}

	case NP_RT_OP_NETINFO_REQ:
		return npRtSignalingSendNetInfo(NULL, &npIds, opCode,
						(const NpRtNetInfoPayload *)payloadPtr);

	case NP_RT_OP_NETINFO_RES: {
		NpRtPendingNode *node = npRtFindPendingById(requestId);
		if (node == NULL)
			return NP_JGA_ERR_INVALID_ID;
		int ret = npRtSignalingSendNetInfo(node->stanzaId, &npIds, opCode,
						   (const NpRtNetInfoPayload *)payloadPtr);
		npRtRemovePendingById(requestId);
		return ret;
	}

	default:
		return NP_RT_ERR_INVALID_OP;
	}
}

/* 0x81225274 */
static int npRtSignalingOnResult(int transactionId, const NpRtJidPair *npIds,
				 int opCode, const void *payload, SceSize payloadSize)
{
	NpRtServerToClientMsg msg;
	int ret;

	if (payloadSize + NP_RT_SIGNALING_HEADER_SIZE > NP_RT_SIGNALING_MSG_MAX)
		return NP_RT_ERR_TOO_LARGE;

	sceClibMemset(&msg, 0, sizeof(msg));
	msg.msgType = NP_RT_IPC_TYPE_RESULT;
	msg.requestId = transactionId;
	sceClibMemcpy_safe(&msg.toNpId, npIds->peerNpId, sizeof(SceNpId));
	sceClibMemcpy_safe(&msg.fromNpId, npIds->selfNpId, sizeof(SceNpId));
	msg.totalPayloadSize = payloadSize + 8u;
	msg.opCode = opCode;
	msg.payloadSize = payloadSize;
	if (payload != NULL)
		sceClibMemcpy_safe(msg.payload, payload, payloadSize);

	if (s_rtClientSession == NULL)
		return NP_RT_ERR_NO_CLIENT;

	ret = npImpsRawMsgSend(s_rtClientSession, &msg,
			       payloadSize + NP_RT_SIGNALING_HEADER_SIZE, 2);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8122532C */
static int npRtSignalingOnRequest(int stanzaType, const char *stanzaId, const NpRtJidPair *npIds,
				  int opCode, const void *payload, SceSize payloadSize)
{
	NpRtServerToClientMsg msg;
	NpRtPendingNode *node;
	int ipcType;
	int ret;

	if (payloadSize + NP_RT_SIGNALING_HEADER_SIZE > NP_RT_SIGNALING_MSG_MAX)
		return NP_RT_ERR_TOO_LARGE;

	sceClibMemset(&msg, 0, sizeof(msg));
	if (stanzaType == NP_RT_STANZA_IQ_GET)
		ipcType = NP_RT_IPC_TYPE_GET;
	else if (stanzaType == NP_RT_STANZA_IQ_SET)
		ipcType = NP_RT_IPC_TYPE_SET;
	else
		return NP_JGA_ERR_INVALID_TYPE;
	msg.msgType = ipcType;

	node = (NpRtPendingNode *)npRtHeapAlloc(sizeof(NpRtPendingNode), NULL);
	if (node == NULL)
		return NP_JGA_ERR_NO_MEMORY;

	sceClibMemset(node, 0, sizeof(NpRtPendingNode));
	sceClibStrncpy(node->stanzaId, stanzaId, sizeof(node->stanzaId));
	node->requestId = s_rtNextRequestId;
	if (s_rtPendingHead != NULL)
		node->next = s_rtPendingHead;
	msg.requestId = s_rtNextRequestId;
	s_rtPendingHead = node;
	s_rtNextRequestId = msg.requestId + 1;
	if (msg.requestId == -1)
		s_rtNextRequestId = 1;

	sceClibMemcpy_safe(&msg.toNpId, npIds->peerNpId, sizeof(SceNpId));
	sceClibMemcpy_safe(&msg.fromNpId, npIds->selfNpId, sizeof(SceNpId));
	msg.totalPayloadSize = payloadSize + 8u;
	msg.opCode = opCode;
	msg.payloadSize = payloadSize;
	if (payload != NULL)
		sceClibMemcpy_safe(msg.payload, payload, payloadSize);

	if (s_rtClientSession != NULL) {
		ret = npImpsRawMsgSend(s_rtClientSession, &msg,
				       payloadSize + NP_RT_SIGNALING_HEADER_SIZE, 2);
		if (ret >= 0)
			return 0;
	} else {
		ret = NP_RT_ERR_NO_CLIENT;
	}

	npRtRemovePendingById(msg.requestId);
	return ret;
}

/* 0x812254C4 */
static void npRtSignalingOnError(int transactionId, int errorCode)
{
	NpRtErrorNotifyMsg msg;

	msg.msgType = NP_RT_IPC_TYPE_ERROR;
	msg.transactionId = transactionId;
	msg.errorCode = errorCode;
	if (s_rtClientSession != NULL)
		npImpsRawMsgSend(s_rtClientSession, &msg, NP_RT_SIGNALING_ERROR_SIZE, 2);
}

/* 0x81225504 */
static void npRtSignalingOnClientConnected(IPMI::Session *session)
{
	if (s_rtClientSession == NULL)
		s_rtClientSession = session;
}

/* 0x81225514 */
static void npRtSignalingOnClientDisconnected(IPMI::Session *session)
{
	NpRtPendingNode *curr = s_rtPendingHead;
	(void)session;

	s_rtClientSession = NULL;
	while (curr != NULL) {
		NpRtPendingNode *next = curr->next;
		npRtHeapFree(curr, NULL);
		curr = next;
	}
	s_rtPendingHead = NULL;
}

/* 0x81225548 */
static int npRtSignalingOnClientMessage(IPMI::Session *session, const void *msgBuf)
{
	(void)session;
	return npRtSignalingHandleClientMsg(msgBuf);
}

/* 0x81225554 */
static int npRtSignalingSendError(const char *stanzaId, const NpRtJidPair *npIds, int errorCode)
{
	NpRtStanza *stanza = NULL;
	NpRtJidPair reversed;
	int ret;

	reversed.selfNpId = npIds->peerNpId;
	reversed.peerNpId = npIds->selfNpId;

	ret = npRtCreateStanza(&reversed, stanzaId, NP_RT_STANZA_ERROR, &stanza);
	if (ret >= 0) {
		NpXmppErrorStanza errStanza;
		sceClibMemset(&errStanza, 0, sizeof(errStanza));
		errStanza.errorKind = 2;
		errStanza.errorCode = (SceUInt8)(errorCode & 0xFF);
		errStanza.origStanza = stanza;
		ret = npRtSendError(npIds->peerNpId, s_rtHandlerId, &errStanza);
	}
	if (stanza != NULL)
		npRtReleaseStanza(stanza);
	return ret;
}

static void npRtSignalingRtOnStateNoop(const NpRtSessionIdentity *session, int eventState,
				       int resultCode)
{
	(void)session;
	(void)eventState;
	(void)resultCode;
}

/* 0x812259FC */
static int npRtSetCallbacks(NpRtRecvRequestCb onReq, NpRtRecvResultCb onRes)
{
	s_rtOnRequest = onReq;
	s_rtOnResult = onRes;
	return 0;
}

/* 0x812255D6 */
static int npRtSignalingRegister(NpRtRecvRequestCb onReq, NpRtRecvResultCb onRes,
				 NpRtRecvErrorCb onErr)
{
	NpRtHandlerConfig config;
	int ret;

	config.namespaces = s_rtNamespaces;
	config.onStateChanged = npRtSignalingRtOnStateNoop;
	config.onIqRecv = npRtSignalingRtOnRecv;
	config.onMessageRecv = NULL;
	config.onPresenceRecv = NULL;

	ret = npRtRegisterHandler(&config, &s_rtHandlerId);
	if (ret < 0)
		return ret;

	ret = npRtSetCallbacks(onReq, onRes);
	if (ret >= 0) {
		s_rtOnError = onErr;
		return 0;
	}

	npRtUnregisterHandler(s_rtHandlerId);
	s_rtHandlerId = 0;
	return ret;
}

/* 0x81225670 */
static int npRtSignalingSendAck(const char *stanzaId, const NpRtJidPair *npIds)
{
	NpRtStanza *stanza = NULL;
	int ret = npRtCreateStanza(npIds, stanzaId, NP_RT_STANZA_IQ_RESULT, &stanza);
	if (ret >= 0)
		ret = npRtSendResult(npIds->peerNpId, stanza, s_rtHandlerId);
	if (stanza != NULL)
		npRtReleaseStanza(stanza);
	return ret;
}

/* XML helpers for netinfo / udpp2p */
static int npRtBuildNetInfoResultXml(const NpRtNetInfoPayload *info, BxceDocument **outDoc);
static int npRtBuildNetInfoQueryXml(BxceDocument **outDoc);
static int npRtBuildUdpP2pXml(BxceDocument **outDoc, int opCode, const NpUdpP2pPayload *payload);
static int npRtLookupQueryNamespace(const BxceDocument *doc, const NpRtNamespaceEntry **outEntry);

/* 0x812256CC */
static int npRtSignalingSendUdpP2p(const NpRtJidPair *npIds, int opCode,
				   const NpUdpP2pPayload *payload)
{
	NpRtStanza *stanza = NULL;
	int txId = 0;
	int ret;

	switch (opCode) {
	case NP_RT_OP_OFFER:
	case NP_RT_OP_ACCEPT:
	case NP_RT_OP_TERMINATE:
	case NP_RT_OP_ACTIVATED:
	case NP_RT_OP_DEACTIVATED:
		ret = npRtCreateStanza(npIds, NULL, NP_RT_STANZA_IQ_SET, &stanza);
		if (ret >= 0) {
			ret = npRtBuildUdpP2pXml(&stanza->xmlDoc, opCode, payload);
			if (ret >= 0) {
				const NpRtNamespaceEntry *nsEntry = npRtFindNamespace(s_nsUdpP2p);
				if (nsEntry != NULL) {
					ret = npRtSendIq(npIds->peerNpId, stanza, s_rtHandlerId,
							 npRtSignalingRtOnIqResponse,
							 (void *)nsEntry, &txId);
					if (ret >= 0)
						ret = txId;
				} else {
					ret = NP_RT_ERR_UNKNOWN_NS;
				}
			}
		}
		if (stanza != NULL)
			npRtReleaseStanza(stanza);
		return ret;
	default:
		return NP_RT_ERR_INVALID_OP;
	}
}

/* 0x812257A6 */
static int npRtSignalingSendNetInfo(const char *stanzaId, const NpRtJidPair *npIds, int opCode,
				    const NpRtNetInfoPayload *payload)
{
	NpRtStanza *stanza = NULL;
	int txId = 0;
	int stanzaType;
	int ret;

	if (opCode == NP_RT_OP_NETINFO_REQ)
		stanzaType = NP_RT_STANZA_IQ_GET;
	else if (opCode == NP_RT_OP_NETINFO_RES)
		stanzaType = NP_RT_STANZA_IQ_RESULT;
	else
		return NP_RT_ERR_INVALID_OP;

	ret = npRtCreateStanza(npIds, stanzaId, stanzaType, &stanza);
	if (ret >= 0) {
		if (opCode == NP_RT_OP_NETINFO_REQ)
			ret = npRtBuildNetInfoQueryXml(&stanza->xmlDoc);
		else
			ret = npRtBuildNetInfoResultXml(payload, &stanza->xmlDoc);

		if (ret >= 0) {
			const NpRtNamespaceEntry *nsEntry = npRtFindNamespace(s_nsNetInfo);
			if (nsEntry != NULL) {
				if (stanzaType == NP_RT_STANZA_IQ_RESULT) {
					ret = npRtSendResult(npIds->peerNpId, stanza, s_rtHandlerId);
					if (ret >= 0)
						ret = txId;
				} else {
					ret = npRtSendIq(npIds->peerNpId, stanza, s_rtHandlerId,
							 npRtSignalingRtOnIqResponse,
							 (void *)nsEntry, &txId);
					if (ret >= 0)
						ret = txId;
				}
			} else {
				ret = NP_RT_ERR_UNKNOWN_NS;
			}
		}
	}
	if (stanza != NULL)
		npRtReleaseStanza(stanza);
	return ret;
}

/* 0x812258A6 */
static void npRtSignalingRtOnRecv(const NpRtSessionIdentity *session, NpRtStanza *stanza)
{
	const NpRtNamespaceEntry *nsEntry = NULL;
	int ret;

	(void)session;
	if (stanza->stanzaType != NP_RT_STANZA_IQ_GET &&
	    stanza->stanzaType != NP_RT_STANZA_IQ_SET) {
		return;
	}

	ret = npRtLookupQueryNamespace(stanza->xmlDoc, &nsEntry);
	if (ret >= 0)
		ret = nsEntry->onRecvRequest(stanza);

	if (ret < 0) {
		SceNpId localNpId;
		int sessionHandle = 0;
		if (SceNpCommon_9B7CAA67(&localNpId, &stanza->toJid) >= 0 &&
		    npRtGetSessionByNpId(&localNpId, &sessionHandle) >= 0) {
			NpXmppErrorStanza errStanza;
			sceClibMemset(&errStanza, 0, sizeof(errStanza));
			errStanza.errorKind = 2;
			errStanza.errorCode = (SceUInt8)(ret & 0xFF);
			errStanza.origStanza = stanza;
			npRtSendIqError(sessionHandle, s_rtHandlerId, &errStanza);
		}
	}
}

/* 0x8122593E */
static void npRtSignalingRtOnIqResponse(const NpRtSessionIdentity *session, int transactionId,
					int result, NpRtStanza *response, void *userArg)
{
	const NpRtNamespaceEntry *nsEntry = (const NpRtNamespaceEntry *)userArg;
	(void)session;

	if (result < 0) {
		s_rtOnError(transactionId, result);
		return;
	}

	if (response->stanzaType == NP_RT_STANZA_IQ_RESULT) {
		int ret = nsEntry->onRecvResponse(response, transactionId);
		if (ret < 0)
			s_rtOnError(transactionId, ret);
		return;
	}

	if (response->stanzaType == NP_RT_STANZA_ERROR) {
		NpXmppErrorStanza xmppErr;
		int ret = npRtParseXmppError(&xmppErr, response);
		if (ret < 0) {
			s_rtOnError(transactionId, ret);
		} else if (xmppErr.errorKind == 2) {
			s_rtOnError(transactionId, (int)(0x80552700u | xmppErr.errorCode));
		} else if (xmppErr.errorKind == 1) {
			s_rtOnError(transactionId, (int)(0x80551100u | xmppErr.errorCode));
		}
	}
}

static BxceAttributeList *npRtAllocAttrList(int *outErr)
{
	BxceAttributeList *attrs =
		(BxceAttributeList *)sceNpCommonMallocNpServerName(BXCE_ATTRIBUTE_LIST_SIZE);
	if (attrs == NULL) {
		*outErr = NP_RT_ERR_NO_MEMORY;
		return NULL;
	}
	if (ScebXCe_E7C8B20D(attrs) != 0) {
		ScebXCe_9C2F9897(attrs);
		sceNpCommonFreeNpServerName(attrs);
		*outErr = NP_RT_ERR_XML_FAIL;
		return NULL;
	}
	return attrs;
}

static void npRtFreeAttrList(BxceAttributeList *attrs)
{
	if (attrs != NULL) {
		ScebXCe_9C2F9897(attrs);
		sceNpCommonFreeNpServerName(attrs);
	}
}

/* 0x81225A14 */
static int npRtBuildNetInfoResultXml(const NpRtNetInfoPayload *info, BxceDocument **outDoc)
{
	BxceDocument *doc = (BxceDocument *)sceNpCommonMallocNpServerName(BXCE_DOCUMENT_SIZE);
	if (doc == NULL)
		return NP_RT_ERR_NO_MEMORY;

	int ret = 0;
	BxceAttributeList *attrs = NULL;
	BxceNodeId queryNode = 0;

	if (ScebXCe_FAB50608(doc, 1) != 0) {
		ret = NP_RT_ERR_XML_FAIL;
	} else {
		attrs = npRtAllocAttrList(&ret);
		if (attrs != NULL) {
			if (ScebXCe_3DD761CF(attrs, "xmlns", s_nsNetInfo) != 0) {
				ret = NP_RT_ERR_XML_FAIL;
			} else {
				BxceNodeId root = ScebXCe_75E1B095(doc);
				queryNode = ScebXCe_AC0E4F67(doc, root, "query", attrs, NULL, 0);
				if (queryNode == 0)
					ret = NP_RT_ERR_XML_FAIL;
			}
			npRtFreeAttrList(attrs);
			attrs = NULL;
		}
	}

	/* <address local="..." mapped="..." /> */
	if (ret == 0) {
		char ipStr[16];
		attrs = npRtAllocAttrList(&ret);
		if (attrs != NULL) {
			sceClibMemset(ipStr, 0, sizeof(ipStr));
			sceNetInetNtop(NP_RT_AF_INET, &info->localAddr, ipStr, sizeof(ipStr));
			if (ScebXCe_3DD761CF(attrs, "local", ipStr) != 0) {
				ret = NP_RT_ERR_XML_FAIL;
			} else {
				sceClibMemset(ipStr, 0, sizeof(ipStr));
				sceNetInetNtop(NP_RT_AF_INET, &info->mappedAddr, ipStr, sizeof(ipStr));
				if (ScebXCe_3DD761CF(attrs, "mapped", ipStr) != 0)
					ret = NP_RT_ERR_XML_FAIL;
				else if (ScebXCe_AC0E4F67(doc, queryNode, "address", attrs, NULL, 0) == 0)
					ret = NP_RT_ERR_NO_MEMORY;
			}
			npRtFreeAttrList(attrs);
			attrs = NULL;
		}
	}

	/* <upnp status="..." /> */
	if (ret == 0) {
		attrs = npRtAllocAttrList(&ret);
		if (attrs != NULL) {
			if (ScebXCe_3DD761CF(attrs, "status", "un" "known") != 0)
				ret = NP_RT_ERR_XML_FAIL;
			else if (ScebXCe_AC0E4F67(doc, queryNode, "upnp", attrs, NULL, 0) == 0)
				ret = NP_RT_ERR_NO_MEMORY;
			npRtFreeAttrList(attrs);
			attrs = NULL;
		}
	}

	/* <npport status="..." port="0" /> */
	if (ret == 0) {
		attrs = npRtAllocAttrList(&ret);
		if (attrs != NULL) {
			if (ScebXCe_3DD761CF(attrs, "status", "un" "known") != 0 ||
			    ScebXCe_3DD761CF(attrs, "port", "0") != 0)
				ret = NP_RT_ERR_XML_FAIL;
			else if (ScebXCe_AC0E4F67(doc, queryNode, "npport", attrs, NULL, 0) == 0)
				ret = NP_RT_ERR_NO_MEMORY;
			npRtFreeAttrList(attrs);
			attrs = NULL;
		}
	}

	/* <nat status="..." /> */
	if (ret == 0) {
		attrs = npRtAllocAttrList(&ret);
		if (attrs != NULL) {
			const char *natStr = NULL;
			switch (info->natType) {
			case 0: natStr = "un" "known"; break;
			case 1: natStr = "type" "1"; break;
			case 2: natStr = "type" "2"; break;
			case 3: natStr = "type" "3"; break;
			default: break;
			}
			if (natStr != NULL) {
				if (ScebXCe_3DD761CF(attrs, "status", natStr) != 0) {
					ret = NP_RT_ERR_XML_FAIL;
				} else if (ScebXCe_AC0E4F67(doc, queryNode, "nat", attrs, NULL, 0) != 0) {
					npRtFreeAttrList(attrs);
					*outDoc = doc;
					return 0;
				} else {
					ret = NP_RT_ERR_NO_MEMORY;
				}
			}
			npRtFreeAttrList(attrs);
		}
	}

	ScebXCe_B0891B13(doc);
	sceNpCommonFreeNpServerName(doc);
	return ret;
}

/* 0x81225DC2 */
static int npRtBuildNetInfoQueryXml(BxceDocument **outDoc)
{
	BxceDocument *doc = (BxceDocument *)sceNpCommonMallocNpServerName(BXCE_DOCUMENT_SIZE);
	if (doc == NULL)
		return NP_RT_ERR_NO_MEMORY;

	int ret = NP_RT_ERR_XML_FAIL;
	if (ScebXCe_FAB50608(doc, 1) == 0) {
		BxceAttributeList *attrs = npRtAllocAttrList(&ret);
		if (attrs != NULL) {
			if (ScebXCe_3DD761CF(attrs, "xmlns", s_nsNetInfo) == 0) {
				BxceNodeId root = ScebXCe_75E1B095(doc);
				if (ScebXCe_AC0E4F67(doc, root, "query", attrs, NULL, 0) != 0) {
					npRtFreeAttrList(attrs);
					*outDoc = doc;
					return 0;
				}
			}
			ret = NP_RT_ERR_XML_FAIL;
			npRtFreeAttrList(attrs);
		}
	}

	ScebXCe_B0891B13(doc);
	sceNpCommonFreeNpServerName(doc);
	return ret;
}

/* 0x81225EA0 */
static int npRtBuildUdpP2pXml(BxceDocument **outDoc, int opCode, const NpUdpP2pPayload *payload)
{
	BxceDocument *doc = (BxceDocument *)sceNpCommonMallocNpServerName(BXCE_DOCUMENT_SIZE);
	if (doc == NULL)
		return NP_RT_ERR_NO_MEMORY;

	int ret = 0;
	BxceNodeId queryNode = 0;
	char numStr[8];
	char ipStr[16];
	char skeyHex[36];

	if (ScebXCe_FAB50608(doc, 1) != 0) {
		ret = NP_RT_ERR_XML_FAIL;
	} else {
		BxceAttributeList *attrs = npRtAllocAttrList(&ret);
		if (attrs != NULL) {
			const char *actionStr = NULL;
			switch (opCode) {
			case NP_RT_OP_OFFER:       actionStr = "offer"; break;
			case NP_RT_OP_ACCEPT:      actionStr = "accept"; break;
			case NP_RT_OP_TERMINATE:   actionStr = "terminate"; break;
			case NP_RT_OP_ACTIVATED:   actionStr = "activated"; break;
			case NP_RT_OP_DEACTIVATED: actionStr = "deactivated"; break;
			case NP_RT_OP_FTERMINATE:  actionStr = "fterminate"; break;
			default:                   break;
			}

			if (actionStr == NULL ||
			    ScebXCe_3DD761CF(attrs, "xmlns", s_nsUdpP2p) != 0 ||
			    ScebXCe_3DD761CF(attrs, "action", actionStr) != 0) {
				ret = NP_RT_ERR_XML_FAIL;
			}

			if (ret == 0) {
				sceClibMemset(numStr, 0, 6u);
				sceClibSnprintf(numStr, 6u, "%u", (unsigned int)payload->sid);
				if (ScebXCe_3DD761CF(attrs, "sid", numStr) != 0)
					ret = NP_RT_ERR_XML_FAIL;
			}

			if (ret == 0) {
				sceClibMemset(numStr, 0, 6u);
				sceClibSnprintf(numStr, 6u, "%u", (unsigned int)payload->psid);
				if (ScebXCe_3DD761CF(attrs, "psid", numStr) != 0)
					ret = NP_RT_ERR_XML_FAIL;
			}

			if (ret == 0 && (opCode == NP_RT_OP_OFFER || opCode == NP_RT_OP_ACCEPT)) {
				for (int i = 0; i < 16; i++)
					sceClibSnprintf(&skeyHex[i * 2], 3u, "%02x", payload->skey[i]);
				if (ScebXCe_3DD761CF(attrs, "skey", skeyHex) != 0)
					ret = NP_RT_ERR_XML_FAIL;
			}

			if (ret == 0 && opCode == NP_RT_OP_OFFER) {
				sceClibMemset(numStr, 0, 6u);
				sceClibSnprintf(numStr, 6u, "%u", (unsigned int)payload->natType);
				if (ScebXCe_3DD761CF(attrs, "nattype", numStr) != 0)
					ret = NP_RT_ERR_XML_FAIL;
			}

			if (ret == 0) {
				BxceNodeId root = ScebXCe_75E1B095(doc);
				queryNode = ScebXCe_AC0E4F67(doc, root, "query", attrs, NULL, 0);
				if (queryNode == 0)
					ret = NP_RT_ERR_XML_FAIL;
			}
			npRtFreeAttrList(attrs);
		}
	}

	if (ret == 0 && (opCode == NP_RT_OP_OFFER || opCode == NP_RT_OP_ACCEPT)) {
		for (int i = 0; i < 8 && ret == 0; i++) {
			const NpUdpP2pCandidate *cand = &payload->candidates[i];
			if (cand->candidateType == 0)
				continue;

			BxceAttributeList *candAttrs = npRtAllocAttrList(&ret);
			if (candAttrs == NULL)
				break;

			const char *typeStr = NULL;
			switch (cand->candidateType) {
			case NP_UDPP2P_CAND_LOCAL:   typeStr = "local"; break;
			case NP_UDPP2P_CAND_DERIVED: typeStr = "derived"; break;
			case NP_UDPP2P_CAND_STUN:    typeStr = "stun"; break;
			case NP_UDPP2P_CAND_STATIC:  typeStr = "static"; break;
			default:                     break;
			}

			if (typeStr == NULL || ScebXCe_3DD761CF(candAttrs, "type", typeStr) != 0) {
				ret = NP_RT_ERR_XML_FAIL;
			}

			if (ret == 0) {
				sceClibMemset(ipStr, 0, sizeof(ipStr));
				sceNetInetNtop(NP_RT_AF_INET, &cand->addr, ipStr, sizeof(ipStr));
				if (ScebXCe_3DD761CF(candAttrs, "address", ipStr) != 0)
					ret = NP_RT_ERR_XML_FAIL;
			}

			if (ret == 0) {
				sceClibMemset(numStr, 0, 6u);
				sceClibSnprintf(numStr, 6u, "%u", (unsigned int)sceNetNtohs(cand->port));
				if (ScebXCe_3DD761CF(candAttrs, "port", numStr) != 0)
					ret = NP_RT_ERR_XML_FAIL;
			}

			if (ret == 0 && opCode == NP_RT_OP_ACCEPT) {
				sceClibMemset(ipStr, 0, sizeof(ipStr));
				sceNetInetNtop(NP_RT_AF_INET, &cand->mappedAddr, ipStr, sizeof(ipStr));
				if (ScebXCe_3DD761CF(candAttrs, "maddress", ipStr) != 0) {
					ret = NP_RT_ERR_XML_FAIL;
				} else {
					sceClibMemset(numStr, 0, 6u);
					sceClibSnprintf(numStr, 6u, "%u",
							(unsigned int)sceNetNtohs(cand->mappedPort));
					if (ScebXCe_3DD761CF(candAttrs, "mport", numStr) != 0)
						ret = NP_RT_ERR_XML_FAIL;
				}
			}

			if (ret == 0 &&
			    ScebXCe_AC0E4F67(doc, queryNode, "candidate", candAttrs, NULL, 0) == 0) {
				ret = NP_RT_ERR_XML_FAIL;
			}

			npRtFreeAttrList(candAttrs);
		}
	}

	if (ret == 0) {
		*outDoc = doc;
		return 0;
	}

	ScebXCe_B0891B13(doc);
	sceNpCommonFreeNpServerName(doc);
	return ret;
}

/* 0x8122632E */
static int npRtLookupQueryNamespace(const BxceDocument *doc, const NpRtNamespaceEntry **outEntry)
{
	BxceNodeId root = ScebXCe_75E1B095(doc);
	BxceNodeList *queryList = ScebXCe_1FA24DC0(doc, root, "query");
	if (queryList == NULL)
		return NP_RT_ERR_XML_FAIL;

	int ret = NP_RT_ERR_UNKNOWN_NS;
	for (SceUInt32 idx = 0; idx < queryList->count && ret < 0; idx++) {
		BxceNodeId node = ScebXCe_6C46D786(queryList, idx);
		const char *xmlns = ScebXCe_753D9382(doc, node, "xmlns");
		if (xmlns == NULL)
			continue;

		const NpRtNamespaceEntry *entry = npRtFindNamespace(xmlns);
		if (entry != NULL) {
			*outEntry = entry;
			ret = 0;
			break;
		}
	}

	ScebXCe_1F59F0C4(queryList);
	ScebXCe_EC560647(queryList);
	return ret;
}

/* 0x812263C8 */
static int npRtNetInfoOnRequest(const NpRtStanza *stanza)
{
	SceNpId fromNpId;
	SceNpId toNpId;
	NpRtJidPair npIds;
	int ret;

	npIds.selfNpId = &fromNpId;
	npIds.peerNpId = &toNpId;

	ret = SceNpCommon_9B7CAA67(&fromNpId, &stanza->toJid);
	if (ret >= 0)
		ret = SceNpCommon_9B7CAA67(&toNpId, &stanza->fromJid);
	if (ret >= 0) {
		ret = s_rtOnRequest(stanza->stanzaType, stanza->stanzaId, &npIds,
				    NP_RT_OP_NETINFO_REQ, NULL, 0);
		if (ret >= 0)
			return 0;
	}
	return ret;
}

/* Helper: find first <query xmlns="..."> node */
static int npRtFindQueryNode(const BxceDocument *doc, const char *targetNs, BxceNodeId *outNode)
{
	BxceNodeId root = ScebXCe_75E1B095(doc);
	BxceNodeList *queryList = ScebXCe_1FA24DC0(doc, root, "query");
	if (queryList == NULL)
		return NP_RT_ERR_XML_FAIL;

	int ret = NP_RT_ERR_UNKNOWN_NS;
	for (SceUInt32 idx = 0; idx < queryList->count; idx++) {
		BxceNodeId node = ScebXCe_6C46D786(queryList, idx);
		const char *xmlns = ScebXCe_753D9382(doc, node, "xmlns");
		if (xmlns != NULL && sceClibStrcmp(xmlns, targetNs) == 0) {
			*outNode = node;
			ret = 0;
			break;
		}
	}

	ScebXCe_1F59F0C4(queryList);
	ScebXCe_EC560647(queryList);
	return ret;
}

/* 0x81226438 */
static int npRtNetInfoOnResponse(const NpRtStanza *stanza, int transactionId)
{
	NpRtNetInfoPayload info;
	SceNpId fromNpId;
	SceNpId toNpId;
	NpRtJidPair npIds;
	int ret;

	sceClibMemset(&info, 0, sizeof(info));
	npIds.selfNpId = &fromNpId;
	npIds.peerNpId = &toNpId;

	ret = SceNpCommon_9B7CAA67(&fromNpId, &stanza->toJid);
	if (ret < 0)
		return ret;
	ret = SceNpCommon_9B7CAA67(&toNpId, &stanza->fromJid);
	if (ret < 0)
		return ret;

	if (stanza->stanzaType == NP_RT_STANZA_IQ_RESULT) {
		const BxceDocument *doc = stanza->xmlDoc;
		BxceNodeId queryNode = 0;

		ret = npRtFindQueryNode(doc, s_nsNetInfo, &queryNode);
		if (ret < 0)
			return ret;

		SceUInt32 seenMask = 0;
		BxceNodeId child = ScebXCe_FEA6E4A1(doc, queryNode);
		while (child != 0) {
			int nodeType = ScebXCe_FA18A039(doc, child);
			if (nodeType == BXCE_NODE_TYPE_ELEMENT ||
			    nodeType == BXCE_NODE_TYPE_EMPTY_ELEMENT) {
				const char *tagName = ScebXCe_1189A2B1(doc, child);
				if ((seenMask & 1u) == 0 && sceClibStrcmp(tagName, "address") == 0) {
					const char *localStr = ScebXCe_753D9382(doc, child, "local");
					if (localStr == NULL ||
					    sceNetInetPton(NP_RT_AF_INET, localStr, &info.localAddr) < 0) {
						return NP_RT_ERR_XML_FAIL;
					}
					const char *mappedStr = ScebXCe_753D9382(doc, child, "mapped");
					if (mappedStr != NULL &&
					    sceNetInetPton(NP_RT_AF_INET, mappedStr, &info.mappedAddr) < 0) {
						return NP_RT_ERR_XML_FAIL;
					}
					seenMask |= 1u;
				} else if ((seenMask & 4u) == 0 && sceClibStrcmp(tagName, "nat") == 0) {
					const char *statusStr = ScebXCe_753D9382(doc, child, "status");
					if (statusStr == NULL)
						return NP_RT_ERR_XML_FAIL;
					if (sceClibStrcmp(statusStr, "type" "1") == 0)
						info.natType = 1;
					else if (sceClibStrcmp(statusStr, "type" "2") == 0)
						info.natType = 2;
					else if (sceClibStrcmp(statusStr, "type" "3") == 0)
						info.natType = 3;
					else
						info.natType = 0;
					seenMask |= 4u;
				}
			}
			child = ScebXCe_6FC9DC3F(doc, child);
		}
		if ((seenMask & 5u) != 5u)
			return NP_RT_ERR_XML_FAIL;
	}

	ret = s_rtOnResult(transactionId, &npIds, NP_RT_OP_NETINFO_RES, &info, sizeof(info));
	return (ret >= 0) ? 0 : ret;
}

/* 0x812266C4 */
static int npRtParseUdpP2pXml(const BxceDocument *doc, int *outOpCode, NpUdpP2pPayload *outPayload)
{
	BxceNodeId queryNode = 0;
	int ret = npRtFindQueryNode(doc, s_nsUdpP2p, &queryNode);
	if (ret < 0)
		return ret;

	const char *actionStr = ScebXCe_753D9382(doc, queryNode, "action");
	if (actionStr == NULL)
		return NP_RT_ERR_XML_FAIL;

	if (sceClibStrcmp(actionStr, "offer") == 0)
		*outOpCode = NP_RT_OP_OFFER;
	else if (sceClibStrcmp(actionStr, "accept") == 0)
		*outOpCode = NP_RT_OP_ACCEPT;
	else if (sceClibStrcmp(actionStr, "terminate") == 0)
		*outOpCode = NP_RT_OP_TERMINATE;
	else if (sceClibStrcmp(actionStr, "activated") == 0)
		*outOpCode = NP_RT_OP_ACTIVATED;
	else if (sceClibStrcmp(actionStr, "deactivated") == 0)
		*outOpCode = NP_RT_OP_DEACTIVATED;
	else if (sceClibStrcmp(actionStr, "fterminate") == 0)
		*outOpCode = NP_RT_OP_FTERMINATE;
	else
		return NP_RT_ERR_XML_FAIL;

	const char *sidStr = ScebXCe_753D9382(doc, queryNode, "sid");
	if (sidStr == NULL)
		return NP_RT_ERR_XML_FAIL;
	outPayload->sid = (SceUInt16)sceClibStrtoll(sidStr, NULL, 10);

	const char *psidStr = ScebXCe_753D9382(doc, queryNode, "psid");
	outPayload->psid = (psidStr != NULL) ? (SceUInt16)sceClibStrtoll(psidStr, NULL, 10) : 0;

	if (*outOpCode == NP_RT_OP_OFFER || *outOpCode == NP_RT_OP_ACCEPT) {
		const char *skeyStr = ScebXCe_753D9382(doc, queryNode, "skey");
		if (skeyStr == NULL || sceClibStrnlen(skeyStr, 0xFFFFFFFFu) != 32u)
			return NP_RT_ERR_XML_FAIL;

		for (int i = 0; i < 16; i++) {
			char byteHex[4];
			byteHex[0] = skeyStr[i * 2];
			byteHex[1] = skeyStr[i * 2 + 1];
			byteHex[2] = '\0';
			outPayload->skey[i] = (SceUInt8)sceClibStrtoll(byteHex, NULL, 16);
		}
	}

	if (*outOpCode == NP_RT_OP_OFFER) {
		const char *natStr = ScebXCe_753D9382(doc, queryNode, "nattype");
		outPayload->natType = (natStr != NULL) ? (SceUInt16)sceClibStrtoll(natStr, NULL, 10) : 0;
	}

	if (*outOpCode == NP_RT_OP_OFFER || *outOpCode == NP_RT_OP_ACCEPT) {
		int candCount = 0;
		BxceNodeId child = ScebXCe_FEA6E4A1(doc, queryNode);
		while (child != 0) {
			int nodeType = ScebXCe_FA18A039(doc, child);
			if (nodeType == BXCE_NODE_TYPE_ELEMENT ||
			    nodeType == BXCE_NODE_TYPE_EMPTY_ELEMENT) {
				const char *tagName = ScebXCe_1189A2B1(doc, child);
				if (sceClibStrcmp(tagName, "candidate") == 0) {
					if (candCount >= 8)
						return 0;

					NpUdpP2pCandidate *cand = &outPayload->candidates[candCount++];
					sceClibMemset(cand, 0, sizeof(NpUdpP2pCandidate));

					const char *typeStr = ScebXCe_753D9382(doc, child, "type");
					if (typeStr == NULL)
						return NP_RT_ERR_XML_FAIL;
					if (sceClibStrcmp(typeStr, "local") == 0)
						cand->candidateType = NP_UDPP2P_CAND_LOCAL;
					else if (sceClibStrcmp(typeStr, "stun") == 0)
						cand->candidateType = NP_UDPP2P_CAND_STUN;
					else if (sceClibStrcmp(typeStr, "derived") == 0)
						cand->candidateType = NP_UDPP2P_CAND_DERIVED;
					else if (sceClibStrcmp(typeStr, "static") == 0)
						cand->candidateType = NP_UDPP2P_CAND_STATIC;
					else
						return NP_RT_ERR_XML_FAIL;

					const char *addrStr = ScebXCe_753D9382(doc, child, "address");
					if (addrStr == NULL ||
					    sceNetInetPton(NP_RT_AF_INET, addrStr, &cand->addr) < 0) {
						return NP_RT_ERR_XML_FAIL;
					}

					const char *portStr = ScebXCe_753D9382(doc, child, "port");
					if (portStr == NULL)
						return NP_RT_ERR_XML_FAIL;
					cand->port = sceNetHtons((SceUInt16)sceClibStrtoll(portStr, NULL, 10));

					if (*outOpCode == NP_RT_OP_ACCEPT) {
						const char *maddrStr = ScebXCe_753D9382(doc, child, "maddress");
						if (maddrStr == NULL ||
						    sceNetInetPton(NP_RT_AF_INET, maddrStr,
								   &cand->mappedAddr) < 0) {
							return NP_RT_ERR_XML_FAIL;
						}
						const char *mportStr = ScebXCe_753D9382(doc, child, "mport");
						if (mportStr == NULL)
							return NP_RT_ERR_XML_FAIL;
						cand->mappedPort = sceNetHtons(
							(SceUInt16)sceClibStrtoll(mportStr, NULL, 10));
					}
				}
			}
			child = ScebXCe_6FC9DC3F(doc, child);
		}
	}

	return 0;
}

/* 0x81226B14 */
static int npRtUdpP2pOnRequest(const NpRtStanza *stanza)
{
	NpUdpP2pPayload payload;
	SceNpId fromNpId;
	SceNpId toNpId;
	NpRtJidPair npIds;
	int opCode = 0;
	int ret;

	sceClibMemset(&payload, 0, sizeof(payload));
	npIds.selfNpId = &fromNpId;
	npIds.peerNpId = &toNpId;

	ret = SceNpCommon_9B7CAA67(&fromNpId, &stanza->toJid);
	if (ret >= 0)
		ret = SceNpCommon_9B7CAA67(&toNpId, &stanza->fromJid);
	if (ret >= 0)
		ret = npRtParseUdpP2pXml(stanza->xmlDoc, &opCode, &payload);
	if (ret >= 0) {
		ret = s_rtOnRequest(stanza->stanzaType, stanza->stanzaId, &npIds,
				    opCode, &payload, sizeof(payload));
		if (ret >= 0)
			return 0;
	}
	return ret;
}

/* 0x81226BA2 */
static int npRtUdpP2pOnResponse(const NpRtStanza *stanza, int transactionId)
{
	SceNpId fromNpId;
	SceNpId toNpId;
	NpRtJidPair npIds;
	int ret;

	npIds.selfNpId = &fromNpId;
	npIds.peerNpId = &toNpId;

	ret = SceNpCommon_9B7CAA67(&fromNpId, &stanza->toJid);
	if (ret >= 0)
		ret = SceNpCommon_9B7CAA67(&toNpId, &stanza->fromJid);
	if (ret >= 0) {
		ret = s_rtOnResult(transactionId, &npIds, NP_RT_OP_ACK, NULL, 0);
		if (ret >= 0)
			return 0;
	}
	return ret;
}
