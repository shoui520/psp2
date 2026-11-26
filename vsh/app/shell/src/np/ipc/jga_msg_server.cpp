/*
 * jga_msg_server.cpp: SceShell NP real-time Join Game Ack (JGA) message bridge
 * and shared XMPP RT stanza send helpers (0x8121D18C - 0x8121DF68).
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <np_common/np_common.h>
#include "np_ipc_internal.h"

#define NP_JGA_RAW_MSG_CHANNEL		2
#define NP_JGA_HEADER_SIZE		84
#define NP_JGA_MSG_MAX_SIZE		256

#define NP_JGA_IPC_TYPE_GET		1
#define NP_JGA_IPC_TYPE_SET		2
#define NP_JGA_IPC_TYPE_RESULT		3
#define NP_JGA_IPC_TYPE_ERROR		4

#define NP_RT_STANZA_TYPE_ERROR		1
#define NP_RT_STANZA_TYPE_IQ_GET	14
#define NP_RT_STANZA_TYPE_IQ_RESULT	15
#define NP_RT_STANZA_TYPE_IQ_SET	16

/* 40-byte singly-linked pending JGA request node */
struct NpJgaPendingNode {
	NpJgaPendingNode *next;		/* +0x00 */
	int requestId;			/* +0x04 */
	char stanzaId[32];		/* +0x08 */
};

/* Server-to-client JGA raw message (max 256 bytes) */
struct NpJgaServerToClientMsg {
	int msgType;			/* +0x00 */
	int requestId;			/* +0x04 */
	SceNpId fromNpId;		/* +0x08 (36 bytes) */
	SceNpId toNpId;			/* +0x2C (36 bytes) */
	SceSize payloadSize;		/* +0x50 */
	SceUInt8 payload[NP_JGA_MSG_MAX_SIZE - NP_JGA_HEADER_SIZE];	/* +0x54 */
};

/* Server-to-client JGA error notification (84 bytes) */
struct NpJgaErrorNotifyMsg {
	int msgType;			/* +0x00 */
	int transactionId;		/* +0x04 */
	int errorCode;			/* +0x08 */
	SceUInt8 reserved[72];		/* +0x0C */
};

/* Client-to-server JGA request message (type 1/2: IQ get/set) */
struct NpJgaClientSendMsg {
	int msgType;			/* +0x00 */
	int reserved;			/* +0x04 */
	int timeoutMs;			/* +0x08 */
	int retryCount;			/* +0x0C */
	SceNpId fromNpId;		/* +0x10 (36 bytes) */
	SceNpId toNpId;			/* +0x34 (36 bytes) */
	SceSize payloadSize;		/* +0x58 */
	SceUInt8 payload[NP_JGA_MSG_MAX_SIZE - 92];	/* +0x5C */
};

/* Client-to-server JGA result message (type 3: IQ result) */
struct NpJgaClientResultMsg {
	int msgType;			/* +0x00 */
	int reserved;			/* +0x04 */
	int requestId;			/* +0x08 */
	SceNpId fromNpId;		/* +0x0C (36 bytes) */
	SceNpId toNpId;			/* +0x30 (36 bytes) */
	SceSize payloadSize;		/* +0x54 */
	SceUInt8 payload[NP_JGA_MSG_MAX_SIZE - 88];	/* +0x58 */
};

/* Active JGA client state (0x8154514C..0x81545164, 0x815A2DE0..0x815A2DE4) */
typedef int (*NpJgaRecvRequestCb)(int stanzaType, const char *stanzaId, const NpRtJidPair *npIds,
				  const void *payload, SceSize payloadSize);
typedef int (*NpJgaRecvResultCb)(int transactionId, const NpRtJidPair *npIds,
				 const void *payload, SceSize payloadSize);
typedef void (*NpJgaRecvErrorCb)(int transactionId, int errorCode);

static IPMI::Session *s_jgaListenerSession = NULL;	/* 0x8154514C */
static int s_jgaNextRequestId = 1;			/* 0x81545150 */
static NpJgaPendingNode *s_jgaPendingHead = NULL;	/* 0x81545154 */
static int s_jgaRtHandlerId = 0;			/* 0x81545158 */
static NpJgaRecvRequestCb s_jgaOnRequest = NULL;	/* 0x8154515C */
static NpJgaRecvResultCb s_jgaOnResult = NULL;		/* 0x81545160 */
static NpJgaRecvErrorCb s_jgaOnError = NULL;		/* 0x81545164 */

static int s_jgaActiveTxId = 0;				/* 0x815A2DE0 */
static IPMI::Session *s_jgaSenderSession = NULL;	/* 0x815A2DE4 */

static const char s_jgaNamespaceUri[] = "http://playstation.net/np/rt/jga-msg";
static const char *const s_jgaNamespaces[] = {		/* 0x8154519C */
	s_jgaNamespaceUri,
	NULL,
};

/* Forward declarations */
static int npJgaOnRtRequest(int stanzaType, const char *stanzaId, const NpRtJidPair *npIds,
			    const void *payload, SceSize payloadSize);
static int npJgaOnRtResult(int transactionId, const NpRtJidPair *npIds,
			   const void *payload, SceSize payloadSize);
static void npJgaOnRtError(int transactionId, int errorCode);
static void npJgaOnClientConnected(IPMI::Session *session);
static void npJgaOnClientDisconnected(IPMI::Session *session);
static int npJgaOnClientMessage(IPMI::Session *session, const void *msgBuf);
static int npJgaRegisterRt(NpJgaRecvRequestCb onReq, NpJgaRecvResultCb onRes,
			   NpJgaRecvErrorCb onErr);
static int npJgaSendRtResult(const char *stanzaId, const NpRtJidPair *npIds,
			     const void *payload, SceSize payloadSize);
static int npJgaSendRtRequest(const NpRtJidPair *npIds, int timeoutMs, int retryCount,
			      const void *payload, SceSize payloadSize);
static void npJgaRtOnStateNoop(const NpRtSessionIdentity *session, int eventState, int resultCode);
static void npJgaRtOnRecv(const NpRtSessionIdentity *session, NpRtStanza *stanza);
static void npJgaRtOnIqResponse(const NpRtSessionIdentity *session, int transactionId, int result,
				NpRtStanza *response, void *userArg);

static void npJgaRemovePendingById(int requestId)
{
	NpJgaPendingNode *curr = s_jgaPendingHead;
	NpJgaPendingNode *prev = NULL;

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
			s_jgaPendingHead = curr->next;
		npRtHeapFree(curr, NULL);
	}
}

/* 0x8121D18C */
int npJgaMsgServerInit(void)
{
	NpImpsRawMsgServerCallbacks callbacks;
	int ret;

	callbacks.onClientConnected = npJgaOnClientConnected;
	callbacks.onClientDisconnected = npJgaOnClientDisconnected;
	callbacks.onClientMessage = npJgaOnClientMessage;

	s_jgaListenerSession = NULL;
	s_jgaActiveTxId = 0;
	s_jgaSenderSession = NULL;

	ret = npImpsRawMsgRegisterChannel(NP_JGA_RAW_MSG_CHANNEL, &callbacks);
	if (ret < 0)
		return ret;

	ret = npJgaRegisterRt(npJgaOnRtRequest, npJgaOnRtResult, npJgaOnRtError);
	if (ret < 0) {
		npImpsRawMsgUnregisterChannel(NP_JGA_RAW_MSG_CHANNEL);
		return ret;
	}
	return 0;
}

/* 0x8121D226 */
static int npJgaOnRtResult(int transactionId, const NpRtJidPair *npIds,
			   const void *payload, SceSize payloadSize)
{
	NpJgaServerToClientMsg msg;
	SceSize totalSize = payloadSize + NP_JGA_HEADER_SIZE;
	int ret;

	if (totalSize > NP_JGA_MSG_MAX_SIZE)
		return NP_JGA_ERR_TOO_LARGE;

	sceClibMemset(&msg, 0, sizeof(msg));
	msg.msgType = NP_JGA_IPC_TYPE_RESULT;
	msg.requestId = transactionId;
	sceClibMemcpy_safe(&msg.toNpId, npIds->peerNpId, sizeof(SceNpId));
	sceClibMemcpy_safe(&msg.fromNpId, npIds->selfNpId, sizeof(SceNpId));
	msg.payloadSize = payloadSize;
	if (payload != NULL)
		sceClibMemcpy_safe(msg.payload, payload, payloadSize);

	if (s_jgaActiveTxId != transactionId)
		return NP_JGA_ERR_INVALID_ID;
	if (s_jgaSenderSession == NULL)
		return NP_JGA_ERR_NO_CLIENT;

	ret = npImpsRawMsgSend(s_jgaSenderSession, &msg, totalSize, 2);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8121D2F0 */
static int npJgaOnRtRequest(int stanzaType, const char *stanzaId, const NpRtJidPair *npIds,
			    const void *payload, SceSize payloadSize)
{
	NpJgaServerToClientMsg msg;
	NpJgaPendingNode *node;
	int ipcType;
	int ret;

	if (payloadSize + NP_JGA_HEADER_SIZE > NP_JGA_MSG_MAX_SIZE)
		return NP_JGA_ERR_TOO_LARGE;

	sceClibMemset(&msg, 0, sizeof(msg));
	if (stanzaType == NP_RT_STANZA_TYPE_IQ_GET) {
		ipcType = NP_JGA_IPC_TYPE_GET;
	} else if (stanzaType == NP_RT_STANZA_TYPE_IQ_SET) {
		ipcType = NP_JGA_IPC_TYPE_SET;
	} else {
		return NP_JGA_ERR_INVALID_TYPE;
	}
	msg.msgType = ipcType;

	node = (NpJgaPendingNode *)npRtHeapAlloc(sizeof(NpJgaPendingNode), NULL);
	if (node == NULL)
		return NP_JGA_ERR_NO_MEMORY;

	sceClibMemset(node, 0, sizeof(NpJgaPendingNode));
	sceClibStrncpy(node->stanzaId, stanzaId, sizeof(node->stanzaId));
	node->requestId = s_jgaNextRequestId;
	if (s_jgaPendingHead != NULL)
		node->next = s_jgaPendingHead;
	msg.requestId = s_jgaNextRequestId;
	s_jgaPendingHead = node;
	s_jgaNextRequestId = msg.requestId + 1;
	if (msg.requestId == -1)
		s_jgaNextRequestId = 1;

	sceClibMemcpy_safe(&msg.toNpId, npIds->peerNpId, sizeof(SceNpId));
	sceClibMemcpy_safe(&msg.fromNpId, npIds->selfNpId, sizeof(SceNpId));
	msg.payloadSize = payloadSize;
	if (payload != NULL)
		sceClibMemcpy_safe(msg.payload, payload, payloadSize);

	if (s_jgaListenerSession != NULL) {
		ret = npImpsRawMsgSend(s_jgaListenerSession, &msg,
				       payloadSize + NP_JGA_HEADER_SIZE, 2);
		if (ret >= 0)
			return 0;
	} else {
		ret = NP_JGA_ERR_NO_CLIENT;
	}

	npJgaRemovePendingById(msg.requestId);
	return ret;
}

/* 0x8121D472 */
static void npJgaOnRtError(int transactionId, int errorCode)
{
	NpJgaErrorNotifyMsg msg;

	if (s_jgaActiveTxId == transactionId && s_jgaSenderSession != NULL) {
		msg.msgType = NP_JGA_IPC_TYPE_ERROR;
		msg.transactionId = transactionId;
		msg.errorCode = errorCode;
		npImpsRawMsgSend(s_jgaSenderSession, &msg, NP_JGA_HEADER_SIZE, 2);
	}
}

/* 0x8121D4BA */
static void npJgaOnClientConnected(IPMI::Session *session)
{
	SceUID pid = session->getProcessId();
	char category[4];
	SceUInt8 appStatus[128];

	if (sceAppMgrAppParamGetString(pid, 8, category, sizeof(category)) >= 0 &&
	    sceClibStrcmp("gd", category) == 0 &&
	    sceAppMgrGetStatusById(pid, appStatus) >= 0 &&
	    appStatus[113] == 1 &&
	    s_jgaListenerSession == NULL) {
		s_jgaListenerSession = session;
	}
}

/* 0x8121D528 */
static void npJgaOnClientDisconnected(IPMI::Session *session)
{
	SceUID pid = session->getProcessId();
	char category[4];
	SceUInt8 appStatus[128];

	if (sceAppMgrAppParamGetString(pid, 8, category, sizeof(category)) >= 0 &&
	    sceClibStrcmp("gd", category) == 0 &&
	    sceAppMgrGetStatusById(pid, appStatus) >= 0 &&
	    appStatus[113] == 1) {
		NpJgaPendingNode *curr = s_jgaPendingHead;
		s_jgaListenerSession = NULL;
		while (curr != NULL) {
			NpJgaPendingNode *next = curr->next;
			npRtHeapFree(curr, NULL);
			curr = next;
		}
		s_jgaPendingHead = NULL;
	}
}

/* 0x8121D5B4 */
static int npJgaOnClientMessage(IPMI::Session *session, const void *msgBuf)
{
	const int *header = (const int *)msgBuf;
	int msgType = header[0];
	NpRtJidPair npIds;

	sceClibMemset(&npIds, 0, sizeof(npIds));
	if (msgType == NP_JGA_IPC_TYPE_GET || msgType == NP_JGA_IPC_TYPE_SET) {
		const NpJgaClientSendMsg *sendMsg = (const NpJgaClientSendMsg *)msgBuf;
		int txId;

		npIds.selfNpId = &sendMsg->fromNpId;
		npIds.peerNpId = &sendMsg->toNpId;
		txId = npJgaSendRtRequest(&npIds, sendMsg->timeoutMs, sendMsg->retryCount,
					  sendMsg->payload, sendMsg->payloadSize);
		if (txId < 0)
			return txId;
		s_jgaActiveTxId = txId;
		s_jgaSenderSession = session;
		return txId;
	}

	if (msgType == NP_JGA_IPC_TYPE_RESULT) {
		const NpJgaClientResultMsg *resMsg = (const NpJgaClientResultMsg *)msgBuf;
		NpJgaPendingNode *node = s_jgaPendingHead;
		int ret;

		npIds.selfNpId = &resMsg->fromNpId;
		npIds.peerNpId = &resMsg->toNpId;
		while (node != NULL && node->requestId != resMsg->requestId)
			node = node->next;
		if (node == NULL)
			return NP_JGA_ERR_INVALID_ID;

		ret = npJgaSendRtResult(node->stanzaId, &npIds, resMsg->payload,
					resMsg->payloadSize);
		npJgaRemovePendingById(resMsg->requestId);
		return ret;
	}

	return NP_JGA_ERR_INVALID_TYPE;
}

static void npJgaRtOnStateNoop(const NpRtSessionIdentity *session, int eventState, int resultCode)
{
	(void)session;
	(void)eventState;
	(void)resultCode;
}

/* 0x8121D6C8 */
static int npJgaRegisterRt(NpJgaRecvRequestCb onReq, NpJgaRecvResultCb onRes,
			   NpJgaRecvErrorCb onErr)
{
	NpRtHandlerConfig config;
	int ret;

	config.namespaces = s_jgaNamespaces;
	config.onStateChanged = npJgaRtOnStateNoop;
	config.onIqRecv = npJgaRtOnRecv;
	config.onMessageRecv = NULL;
	config.onPresenceRecv = NULL;

	ret = npRtRegisterHandler(&config, &s_jgaRtHandlerId);
	if (ret >= 0) {
		s_jgaOnRequest = onReq;
		s_jgaOnResult = onRes;
		s_jgaOnError = onErr;
		return 0;
	}
	return ret;
}

/* 0x8121D748 */
static int npJgaBuildXmlDoc(BxceDocument **outDoc, const void *payload, SceSize payloadSize)
{
	BxceDocument *doc;
	BxceAttributeList *attrs = NULL;
	char *encoded = NULL;
	SceSize encodedLen;
	BxceNodeId docRoot;
	int ret;

	if (payload == NULL || payloadSize == 0)
		return 0;

	doc = (BxceDocument *)sceNpCommonMallocNpServerName(BXCE_DOCUMENT_SIZE);
	if (doc == NULL)
		return NP_JGA_ERR_NO_MEMORY;

	if (ScebXCe_FAB50608(doc, 1) != 0) {
		ret = NP_JGA_ERR_XML_FAIL;
	} else {
		attrs = (BxceAttributeList *)sceNpCommonMallocNpServerName(BXCE_ATTRIBUTE_LIST_SIZE);
		if (attrs == NULL) {
			ret = NP_JGA_ERR_NO_MEMORY;
		} else if (ScebXCe_E7C8B20D(attrs) != 0 ||
			   ScebXCe_3DD761CF(attrs, "xmlns", s_jgaNamespaceUri) != 0) {
			ret = NP_JGA_ERR_XML_FAIL;
		} else {
			encodedLen = 4 * ((payloadSize + 2) / 3u);
			encoded = (char *)npRtHeapAlloc(encodedLen, NULL);
			if (encoded == NULL) {
				ret = NP_JGA_ERR_NO_MEMORY;
			} else {
				ret = (int)sceNpCommonBase64Encode((const SceUInt8 *)payload,
								   encoded, payloadSize);
				if (ret >= 0) {
					docRoot = ScebXCe_75E1B095(doc);
					if (ScebXCe_AC0E4F67(doc, docRoot, "msg", attrs,
							     encoded, encodedLen) != 0) {
						ScebXCe_9C2F9897(attrs);
						sceNpCommonFreeNpServerName(attrs);
						npRtHeapFree(encoded, NULL);
						*outDoc = doc;
						return 0;
					}
					ret = NP_JGA_ERR_XML_FAIL;
				}
				npRtHeapFree(encoded, NULL);
			}
		}
		if (attrs != NULL) {
			ScebXCe_9C2F9897(attrs);
			sceNpCommonFreeNpServerName(attrs);
		}
	}

	ScebXCe_B0891B13(doc);
	sceNpCommonFreeNpServerName(doc);
	return ret;
}

/* 0x8121D88A */
static int npJgaSendRtResult(const char *stanzaId, const NpRtJidPair *npIds,
			     const void *payload, SceSize payloadSize)
{
	NpRtStanza *stanza = NULL;
	int ret;

	ret = npRtCreateStanza(npIds, stanzaId, NP_RT_STANZA_TYPE_IQ_RESULT, &stanza);
	if (ret >= 0) {
		ret = npJgaBuildXmlDoc(&stanza->xmlDoc, payload, payloadSize);
		if (ret >= 0)
			ret = npRtSendResult(npIds->peerNpId, stanza, s_jgaRtHandlerId);
	}
	if (stanza != NULL)
		npRtReleaseStanza(stanza);
	return ret;
}

/* 0x8121D904 */
static int npJgaSendRtRequest(const NpRtJidPair *npIds, int timeoutMs, int retryCount,
			      const void *payload, SceSize payloadSize)
{
	NpRtStanza *stanza = NULL;
	int transactionId = 0;
	int ret;

	ret = npRtCreateStanza(npIds, NULL, NP_RT_STANZA_TYPE_IQ_GET, &stanza);
	if (ret >= 0) {
		ret = npJgaBuildXmlDoc(&stanza->xmlDoc, payload, payloadSize);
		if (ret >= 0) {
			SceUInt64 timeoutUsec =
				static_cast<SceUInt64>(static_cast<SceUInt32>(timeoutMs)) |
				(static_cast<SceUInt64>(static_cast<SceUInt32>(retryCount)) << 32);
			ret = npRtSendIqWithTimeout(npIds->peerNpId, stanza, s_jgaRtHandlerId,
						    timeoutUsec, npJgaRtOnIqResponse, NULL,
						    &transactionId);
			if (ret >= 0)
				ret = transactionId;
		}
	}
	if (stanza != NULL)
		npRtReleaseStanza(stanza);
	return ret;
}

/* 0x8121D99E */
static int npJgaParseXmlDoc(const BxceDocument *doc, SceUInt8 *outBuf, SceSize outBufSize,
			    SceSize *outDecodedSize)
{
	BxceNodeId docRoot;
	BxceNodeList *msgList;
	BxceNodeId matchedNode = 0;
	SceUInt32 idx;
	int ret;

	if (doc == NULL)
		return 0;

	docRoot = ScebXCe_75E1B095(doc);
	msgList = ScebXCe_1FA24DC0(doc, docRoot, "msg");
	if (msgList == NULL)
		return NP_RT_ERR_XML_FAIL;

	ret = NP_JGA_ERR_NOT_FOUND;
	for (idx = 0; idx < msgList->count; idx++) {
		BxceNodeId candidate = ScebXCe_6C46D786(msgList, idx);
		const char *xmlns = ScebXCe_753D9382(doc, candidate, "xmlns");
		if (xmlns != NULL && sceClibStrcmp(xmlns, s_jgaNamespaceUri) == 0) {
			matchedNode = candidate;
			ret = 0;
			break;
		}
	}
	ScebXCe_1F59F0C4(msgList);
	ScebXCe_EC560647(msgList);

	if (ret < 0)
		return ret;

	BxceNodeId textNode = ScebXCe_FEA6E4A1(doc, matchedNode);
	const char *base64Text = ScebXCe_33825EFF(doc, textNode);
	if (base64Text == NULL)
		return NP_JGA_ERR_XML_FAIL;

	SceSize textLen = sceClibStrnlen(base64Text, 0xFFFFFFFFu);
	ret = SceNpCommon_FB1F70F3(base64Text, textLen, outBuf, outBufSize);
	if (ret < 0)
		return ret;

	ret = SceNpCommon_A0C5C10E(base64Text, textLen);
	if (ret < 0)
		return ret;

	*outDecodedSize = (SceSize)ret;
	return 0;
}

/* 0x8121DAAE */
static void npJgaRtOnRecv(const NpRtSessionIdentity *session, NpRtStanza *stanza)
{
	SceUInt8 payloadBuf[NP_JGA_MSG_MAX_SIZE];
	SceSize payloadSize = 0;
	NpRtJidPair npIds;
	SceNpId toNpId;
	SceNpId fromNpId;
	int ret;

	(void)session;
	if (stanza->stanzaType != NP_RT_STANZA_TYPE_IQ_GET &&
	    stanza->stanzaType != NP_RT_STANZA_TYPE_IQ_SET) {
		return;
	}

	sceClibMemset(payloadBuf, 0, sizeof(payloadBuf));
	npIds.selfNpId = &toNpId;
	npIds.peerNpId = &fromNpId;

	ret = SceNpCommon_9B7CAA67(&toNpId, &stanza->toJid);
	if (ret >= 0)
		ret = SceNpCommon_9B7CAA67(&fromNpId, &stanza->fromJid);
	if (ret >= 0)
		ret = npJgaParseXmlDoc(stanza->xmlDoc, payloadBuf, sizeof(payloadBuf), &payloadSize);
	if (ret >= 0)
		ret = s_jgaOnRequest(stanza->stanzaType, stanza->stanzaId, &npIds,
				     payloadBuf, payloadSize);

	if (ret < 0) {
		SceNpId errTarget;
		int sessionHandle = 0;
		if (SceNpCommon_9B7CAA67(&errTarget, &stanza->toJid) >= 0 &&
		    npRtGetSessionByNpId(&errTarget, &sessionHandle) >= 0) {
			NpXmppErrorStanza errStanza;
			sceClibMemset(&errStanza, 0, sizeof(errStanza));
			errStanza.errorKind = 2;
			errStanza.errorCode = (SceUInt8)(ret & 0xFF);
			errStanza.origStanza = stanza;
			npRtSendIqError(sessionHandle, s_jgaRtHandlerId, &errStanza);
		}
	}
}

/* 0x8121DBA8 */
static void npJgaRtOnIqResponse(const NpRtSessionIdentity *session, int transactionId, int result,
				NpRtStanza *response, void *userArg)
{
	(void)session;
	(void)userArg;

	if (result < 0) {
		s_jgaOnError(transactionId, result);
		return;
	}

	if (response->stanzaType == NP_RT_STANZA_TYPE_IQ_RESULT) {
		SceUInt8 payloadBuf[NP_JGA_MSG_MAX_SIZE];
		SceSize payloadSize = 0;
		const void *payloadPtr = NULL;
		NpRtJidPair npIds;
		SceNpId toNpId;
		SceNpId fromNpId;
		int ret;

		sceClibMemset(payloadBuf, 0, sizeof(payloadBuf));
		npIds.selfNpId = &toNpId;
		npIds.peerNpId = &fromNpId;

		ret = SceNpCommon_9B7CAA67(&toNpId, &response->toJid);
		if (ret >= 0)
			ret = SceNpCommon_9B7CAA67(&fromNpId, &response->fromJid);
		if (ret >= 0) {
			if (ScebXCe_CED6BECF(response->xmlDoc) != 0) {
				ret = npJgaParseXmlDoc(response->xmlDoc, payloadBuf,
						       sizeof(payloadBuf), &payloadSize);
				if (ret >= 0)
					payloadPtr = payloadBuf;
			}
			if (ret >= 0)
				ret = s_jgaOnResult(transactionId, &npIds, payloadPtr, payloadSize);
		}
		if (ret < 0)
			s_jgaOnError(transactionId, ret);
		return;
	}

	if (response->stanzaType == NP_RT_STANZA_TYPE_ERROR) {
		NpXmppErrorStanza xmppErr;
		int ret = npRtParseXmppError(&xmppErr, response);
		if (ret < 0) {
			s_jgaOnError(transactionId, ret);
		} else if (xmppErr.errorKind == 2) {
			s_jgaOnError(transactionId, (int)(0x80559A00u | xmppErr.errorCode));
		} else if (xmppErr.errorKind == 1) {
			s_jgaOnError(transactionId, (int)(0x80551100u | xmppErr.errorCode));
		}
	}
}

/* 0x8121DCDC */
int npRtCreateStanza(const NpRtJidPair *npIds, const char *stanzaId, int stanzaType,
		     NpRtStanza **outStanza)
{
	NpRtStanzaInit init;
	NpRtStanza *stanza = NULL;
	char *fromBuf = NULL;
	char *toBuf = NULL;
	SceSize required = 0;
	int ret;

	sceClibMemset(&init, 0, sizeof(init));

	ret = SceNpCommon_40CC4D15(&init.toJid, npIds->selfNpId, NULL, 0, &required);
	if (ret < 0)
		return ret;

	fromBuf = (char *)npRtHeapAlloc(required, NULL);
	if (fromBuf == NULL)
		return NP_JGA_ERR_NO_MEMORY;

	ret = SceNpCommon_40CC4D15(&init.toJid, npIds->selfNpId, fromBuf, required, NULL);
	if (ret >= 0) {
		ret = SceNpCommon_40CC4D15(&init.fromJid, npIds->peerNpId, NULL, 0, &required);
		if (ret >= 0) {
			toBuf = (char *)npRtHeapAlloc(required, NULL);
			if (toBuf == NULL) {
				ret = NP_JGA_ERR_NO_MEMORY;
			} else {
				ret = SceNpCommon_40CC4D15(&init.fromJid, npIds->peerNpId,
							   toBuf, required, NULL);
				if (ret >= 0) {
					init.stanzaId = stanzaId;
					init.stanzaType = stanzaType;
					init.lang = 0;
					ret = npRtAllocStanza(&stanza, &init);
					if (ret >= 0)
						*outStanza = stanza;
				}
			}
		}
	}

	if (fromBuf != NULL)
		npRtHeapFree(fromBuf, NULL);
	if (toBuf != NULL)
		npRtHeapFree(toBuf, NULL);
	return ret;
}

/* 0x8121DDF4 */
int npRtSendIq(const SceNpId *peerNpId, NpRtStanza *stanza, int handlerId,
	       NpRtIqResponseCallback callback, void *userArg, int *outTransactionId)
{
	int sessionHandle = 0;
	int transactionId = 0;
	int ret;

	ret = npRtGetSessionByNpId(peerNpId, &sessionHandle);
	if (ret >= 0) {
		ret = npRtAllocTransactionId(sessionHandle, &transactionId);
		if (ret >= 0) {
			ret = npRtSendIqRequest(sessionHandle, handlerId, transactionId,
						stanza, callback, userArg);
			if (ret >= 0) {
				*outTransactionId = transactionId;
				return 0;
			}
		}
	}
	return ret;
}

/* 0x8121DE68 */
int npRtSendIqWithTimeout(const SceNpId *peerNpId, NpRtStanza *stanza, int handlerId,
			  SceUInt64 timeoutUsec, NpRtIqResponseCallback callback,
			  void *userArg, int *outTransactionId)
{
	int sessionHandle = 0;
	int transactionId = 0;
	int ret;

	ret = npRtGetSessionByNpId(peerNpId, &sessionHandle);
	if (ret >= 0) {
		ret = npRtAllocTransactionId(sessionHandle, &transactionId);
		if (ret >= 0) {
			ret = npRtSendIqRequestEx(sessionHandle, handlerId, transactionId,
						  stanza, timeoutUsec, callback, userArg);
			if (ret >= 0) {
				*outTransactionId = transactionId;
				return 0;
			}
		}
	}
	return ret;
}

/* 0x8121DEE0 */
int npRtSendResult(const SceNpId *peerNpId, NpRtStanza *stanza, int handlerId)
{
	int sessionHandle = 0;
	int ret;

	ret = npRtGetSessionByNpId(peerNpId, &sessionHandle);
	if (ret >= 0) {
		ret = npRtSendIqResult(sessionHandle, handlerId, stanza);
		if (ret >= 0)
			return 0;
	}
	return ret;
}

/* 0x8121DF24 */
int npRtSendError(const SceNpId *peerNpId, int handlerId, const NpXmppErrorStanza *errStanza)
{
	int sessionHandle = 0;
	int ret;

	ret = npRtGetSessionByNpId(peerNpId, &sessionHandle);
	if (ret >= 0) {
		ret = npRtSendIqError(sessionHandle, handlerId, errStanza);
		if (ret >= 0)
			return 0;
	}
	return ret;
}
