/*
 * np_rt_core.h: SceNpCore / SceNpSession / SceNetXmpp / SceNpBasic shared
 * real-time XMPP transport, session manager, and basic roster/privacy service
 * (0x81310320..0x813176AB).
 */
#ifndef SHELL_NP_RT_CORE_H
#define SHELL_NP_RT_CORE_H

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <net/in.h>
#include <net/nettypes.h>
#include <np/np_common.h>
#include <np_common/np_common.h>
#include "../polling/np_polling.h"
#include "np_rt_stanza.h"

struct ssl_st;

/* SceNpCore / SceNpSession / SceNpBasic error codes (0x80551200..0x80551230) */
#define NP_RT_CORE_ERR_ALREADY_INIT		-2141908479	/* 0x80551201 */
#define NP_RT_CORE_ERR_NOT_INITIALIZED		-2141908478	/* 0x80551202 */
#define NP_RT_CORE_ERR_CONNECT_TIMEOUT		-2141908472	/* 0x80551208 */
#define NP_RT_CORE_ERR_PIPE_TIMEOUT		-2141908463	/* 0x80551211 */
#define NP_RT_CORE_ERR_ABORTED			-2141908462	/* 0x80551212 */
#define NP_RT_CORE_ERR_WOULD_BLOCK		-2141908461	/* 0x80551213 */
#define NP_RT_CORE_ERR_TIMEOUT			-2141908460	/* 0x80551214 */
#define NP_RT_CORE_ERR_SHORT_SEND		-2141908459	/* 0x80551215 */
#define NP_RT_CORE_ERR_INVALID_STREAM_STATE	-2141908457	/* 0x80551217 */
#define NP_RT_CORE_ERR_HANDLER_FULL		-2141908456	/* 0x80551218 */
#define NP_RT_CORE_ERR_DEFAULT_HANDLER_SET	-2141908455	/* 0x80551219 */
#define NP_RT_CORE_ERR_HANDLER_NOT_FOUND	-2141908454	/* 0x8055121A */
#define NP_RT_CORE_ERR_SESSION_NOT_FOUND	-2141908452	/* 0x8055121C */
#define NP_RT_CORE_ERR_NO_SASL_MECH		-2141908451	/* 0x8055121D */
#define NP_RT_CORE_ERR_SASL_BUILD		-2141908450	/* 0x8055121E */
#define NP_RT_CORE_ERR_SASL_BAD_SID		-2141908449	/* 0x8055121F */
#define NP_RT_CORE_ERR_SASL_BAD_XMLNS		-2141908448	/* 0x80551220 */
#define NP_RT_CORE_ERR_BAD_STATE		-2141908447	/* 0x80551221 */
#define NP_RT_CORE_ERR_STREAM_ATTR_MISSING	-2141908446	/* 0x80551222 */
#define NP_RT_CORE_ERR_STREAM_BAD_XMLNS		-2141908445	/* 0x80551223 */
#define NP_RT_CORE_ERR_STREAM_BAD_VERSION	-2141908444	/* 0x80551224 */
#define NP_RT_CORE_ERR_STREAM_ERROR		-2141908443	/* 0x80551225 */
#define NP_RT_CORE_ERR_DISCONNECTED		-2141908442	/* 0x80551226 */
#define NP_RT_CORE_ERR_CLOSE_TIMEOUT		-2141908441	/* 0x80551227 */
#define NP_RT_CORE_ERR_BUF_TOO_SMALL		-2141908440	/* 0x80551228 */
#define NP_RT_CORE_ERR_NET_NOT_OPEN		-2141908437	/* 0x8055122B */
#define NP_RT_CORE_ERR_NET_ALREADY_OPEN		-2141908436	/* 0x8055122C */
#define NP_RT_CORE_ERR_XMPP_NOT_OPEN		-2141908435	/* 0x8055122D */
#define NP_RT_CORE_ERR_XMPP_ALREADY_OPEN	-2141908434	/* 0x8055122E */
#define NP_RT_CORE_ERR_NS_CONFLICT		-2141908432	/* 0x80551230 */

/* SceNetXmpp / XML / SSL layer error codes (0x80021280..0x800212A4) */
#define NP_XMPP_ERR_XML				-2147347840	/* 0x80021280 */
#define NP_XMPP_ERR_NO_MEMORY			-2147347839	/* 0x80021281 */
#define NP_XMPP_ERR_INVALID_ARG			-2147347837	/* 0x80021283 */
#define NP_XMPP_ERR_SSL_HANDSHAKE		-2147347806	/* 0x800212A2 */
#define NP_XMPP_ERR_SSL_WRITE			-2147347805	/* 0x800212A3 */
#define NP_XMPP_ERR_SSL_READ			-2147347804	/* 0x800212A4 */

/* SceNpCore session worker states (0x8154646C) */
enum NpRtSessionState {
	NP_RT_STATE_ERROR		= 0,
	NP_RT_STATE_IDLE		= 1,
	NP_RT_STATE_CONNECTING		= 2,
	NP_RT_STATE_RUNNING		= 3,
	NP_RT_STATE_CLOSING		= 4,
	NP_RT_STATE_STOPPED		= 5
};

/* Session state event codes reported to registered handlers (0x8131403C) */
enum NpRtEventState {
	NP_RT_EVENT_ERROR		= 0,
	NP_RT_EVENT_CONNECTING		= 1,
	NP_RT_EVENT_CONNECTED		= 2,
	NP_RT_EVENT_CLOSING		= 3,
	NP_RT_EVENT_STOPPED		= 4
};

/* XMPP stream lifecycle states (0x81313A68) */
enum NpRtXmppStreamState {
	NP_RT_STREAM_NONE		= 0,
	NP_RT_STREAM_OPENING		= 1,
	NP_RT_STREAM_OPEN		= 2,
	NP_RT_STREAM_CLOSING		= 3,
	NP_RT_STREAM_ERROR		= 5
};

/* Optional custom allocator passed to npRtHeapAlloc/Realloc/Free (16 bytes) */
struct NpRtCustomAllocator {
	void *(*alloc)(SceSize size, void *userArg);
	void *(*realloc)(void *ptr, SceSize size, void *userArg);
	void (*free)(void *ptr, void *userArg);
	void *userArg;
};

/* SceNpCore initialization parameters: thread params for Main and Recv (24 bytes) */
struct NpRtCoreInitParam {
	NpPollingThreadParam mainThread;	/* +0x00 (12 bytes) */
	NpPollingThreadParam recvThread;	/* +0x0C (12 bytes) */
};

/* XMPP server connection target passed to npRtCreateSession (130 bytes = 0x82) */
struct NpRtServerConfig {
	char host[64];				/* +0x00 */
	SceUInt16 port;				/* +0x40 */
	char domain[64];			/* +0x42 */
};

/* Public session identity block embedded at NpRtSession +0x30 (200 bytes = 0xC8) */
struct NpRtSessionIdentity {
	int sessionId;				/* +0x00 */
	SceNpOnlineId onlineId;			/* +0x04 (20 bytes) */
	char sessionSid[42];			/* +0x18 */
	SceUInt8 padAfterSid[2];		/* +0x42 */
	NpRtServerConfig serverConfig;		/* +0x44 (130 bytes) */
	SceUInt8 padAfterServer[2];		/* +0xC6 */
};

/* Session callbacks */
typedef void (*NpRtStateCallback)(const NpRtSessionIdentity *session, int eventState,
				  int resultCode);
typedef void (*NpRtIqCallback)(const NpRtSessionIdentity *session, NpRtStanza *stanza);
typedef void (*NpRtMessageCallback)(const NpRtSessionIdentity *session, NpRtMessageStanza *stanza);
typedef void (*NpRtPresenceCallback)(const NpRtSessionIdentity *session,
				     NpRtPresenceStanza *stanza);
typedef void (*NpRtIqResponseCallback)(const NpRtSessionIdentity *session, int transactionId,
				       int result, NpRtStanza *response, void *userArg);
typedef void (*NpRtSendDoneCallback)(const NpRtSessionIdentity *session, int transactionId,
				     int result, void *userArg);

/* Namespace-routed handler descriptor (20 bytes, 0x8131081A) */
struct NpRtHandlerConfig {
	const char *const *namespaces;		/* +0x00 */
	NpRtStateCallback onStateChanged;	/* +0x04 */
	NpRtIqCallback onIqRecv;		/* +0x08 */
	NpRtMessageCallback onMessageRecv;	/* +0x0C */
	NpRtPresenceCallback onPresenceRecv;	/* +0x10 */
};

/* XMPP keepalive tracker (16 bytes, at NpRtXmppClient +0x70) */
struct NpRtKeepAlive {
	int intervalUsec;			/* +0x00 */
	SceUInt32 padAlign;			/* +0x04 */
	SceUInt64 lastSentUsec;			/* +0x08 */
};

class XmppSaxParser;

/* XMPP TLS/TCP client embedded at NpRtSession +0x120 (128 bytes = 0x80) */
struct NpRtXmppClient {
	NpRtNetBase netBase;			/* +0x00 (12 bytes) */
	SceNetSockaddrIn remoteAddr;		/* +0x0C (16 bytes) */
	SceUInt32 padBeforeMutex;		/* +0x1C */
	SceKernelLwMutexWork sslMutex;		/* +0x20 (32 bytes) */
	SceNetId epollId;			/* +0x40 */
	void *sslCtx;				/* +0x44 */
	struct ssl_st *ssl;			/* +0x48 */
	void *sslVerifyCtx;			/* +0x4C */
	int recvTimeoutUsec;			/* +0x50 */
	int sendTimeoutUsec;			/* +0x54 */
	int sslError;				/* +0x58 */
	SceUInt32 sslErrorDetail;		/* +0x5C */
	char *recvBuf;				/* +0x60 */
	SceSize recvBufSize;			/* +0x64 */
	XmppSaxParser *parser;			/* +0x68 */
	int streamState;			/* +0x6C */
	NpRtKeepAlive keepAlive;		/* +0x70 (16 bytes) */
};

/* SceNpBasic ref-counted roster & privacy structures (0x81316F54..0x813176AB) */
enum NpBasicRosterSubscription {
	NP_BASIC_SUB_NONE		= 0,
	NP_BASIC_SUB_ASKED_IN		= 1,
	NP_BASIC_SUB_OUTGOING		= 2,
	NP_BASIC_SUB_FRIEND		= 3,
	NP_BASIC_SUB_REMOVE		= 4
};

struct NpBasicRosterItem {
	SceNpId npId;				/* +0x00 (36 bytes) */
	int subscription;			/* +0x24: NpBasicRosterSubscription */
};

struct NpBasicRosterList {
	volatile SceInt32 refCount;		/* +0x00 */
	NpBasicRosterItem **items;		/* +0x04 */
	SceUInt32 count;			/* +0x08 */
};

struct NpBasicPrivacyList {
	volatile SceInt32 refCount;		/* +0x00 */
	char *listName;				/* +0x04 */
};

typedef void (*NpBasicRosterPushCallback)(const NpRtSessionIdentity *session,
					  NpBasicRosterList *rosterList);
typedef void (*NpBasicPrivacyPushCallback)(const NpRtSessionIdentity *session,
					   NpBasicPrivacyList *privacyList);
typedef void (*NpBasicRosterGetCallback)(const NpRtSessionIdentity *session, int transactionId,
					 int result, NpBasicRosterList *rosterList);

/* SceNpBasic callback table (16 bytes, 0x81316E64) */
struct NpBasicCallbacks {
	NpBasicRosterPushCallback onRosterPush;		/* +0x00 */
	NpBasicPrivacyPushCallback onPrivacyPush;	/* +0x04 */
	NpRtMessageCallback onMessageRecv;		/* +0x08 */
	NpRtPresenceCallback onPresenceRecv;		/* +0x0C */
};

/* SceNpCore lifecycle & handler registration (0x81310320..0x81310A93) */
int npRtCoreInit(const NpRtCoreInitParam *param);
int npRtCoreTerm(void);
int npRtRegisterDefaultHandler(const NpRtHandlerConfig *config, int *outHandlerId);
int npRtRegisterHandler(const NpRtHandlerConfig *config, int *outHandlerId);
int npRtUnregisterHandler(int handlerId);

/* SceNpCore session & stanza send API (0x81310A94..0x81310F73) */
int npRtCreateSession(const SceNpOnlineId *onlineId, const NpRtServerConfig *serverConfig,
		      int *outSessionHandle);
int npRtDestroySession(int sessionHandle);
int npRtConnectSession(int sessionHandle);
int npRtReconnectSession(int sessionHandle, const char *sessionSid);
int npRtDisconnectSession(int sessionHandle);
int npRtGetSessionByNpId(const SceNpId *npId, int *outSessionHandle);
int npRtAllocTransactionId(int sessionHandle, int *outTransactionId);
int npRtSendIqRequest(int sessionHandle, int handlerId, int transactionId, NpRtStanza *stanza,
		      NpRtIqResponseCallback callback, void *userArg);
int npRtSendIqRequestEx(int sessionHandle, int handlerId, int transactionId, NpRtStanza *stanza,
			SceUInt64 timeoutUsec, NpRtIqResponseCallback callback, void *userArg);
int npRtSendMessage(int sessionHandle, int handlerId, int transactionId,
		    NpRtMessageStanza *stanza, NpRtSendDoneCallback callback, void *userArg);
int npRtSendPresence(int sessionHandle, int handlerId, int transactionId,
		     NpRtPresenceStanza *stanza, NpRtSendDoneCallback callback, void *userArg);
int npRtSendIqResult(int sessionHandle, int handlerId, NpRtStanza *stanza);
int npRtSendIqError(int sessionHandle, int handlerId, const NpXmppErrorStanza *errStanza);
int npRtJoinAndDeleteThread(SceUID *threadId);

/* SceNetXmpp heap, formatting, JID, and XML serializer helpers (np_rt_util.cpp) */
int npRtHeapInit(void *memblock, SceSize size);
void npRtHeapTerm(void);
void *npRtHeapAlloc(SceSize size, void *customAlloc);
void *npRtHeapRealloc(void *ptr, SceSize size, void *customAlloc);
void npRtHeapFree(void *ptr, void *customAlloc);
int npRtFormatUInt64(SceUInt64 value, char *outBuf, SceSize outBufSize,
		     int base, SceSize *outRequired);
int npRtDupString(char **outStr, const char *src, void *customAlloc, SceUInt8 flags);
void npRtFreeString(void *ptr, void *customAlloc, SceUInt8 flags);
int npRtXmlUnescapeToBuf(const char *src, char *dst, SceSize dstSize);
int npRtCopyJid(NpJid *dst, const NpJid *src);
void npRtFreeJid(NpJid *jid);
int npRtParseJidToNpId(const char *jidStr, SceNpId *outNpId);
void npRtFreeLangTextList(NpRtLangTextNode *head);
int npRtAllocLangTextNode(NpRtLangTextNode **outNode, const char *text, int lang);
int npXmppSerializePresence(const NpRtPresenceStanza *stanza, char **outXml);
int npXmppSerializeIq(const NpRtStanza *stanza, char **outXml);
int npXmppSerializeMessage(const NpRtMessageStanza *stanza, char **outXml);
int npXmppSerializeIqError(const NpXmppErrorStanza *errStanza, char **outXml);

/* SceNetXmpp transport & stream client helpers (np_rt_xmpp.cpp) */
int npRtXmppClientInit(NpRtXmppClient *client);
void npRtXmppClientTerm(NpRtXmppClient *client);
int npRtXmppClientIsOpen(const NpRtXmppClient *client);
int npRtXmppClientOpen(NpRtXmppClient *client);
int npRtXmppClientClose(NpRtXmppClient *client);
int npRtXmppClientAbort(NpRtXmppClient *client, int abortMask);
int npRtXmppClientSend(NpRtXmppClient *client, const void *buf, SceSize len);
int npRtXmppClientConnect(NpRtXmppClient *client, const char *host, SceUInt16 port,
			  const char *domain);
int npRtXmppClientSendStreamClose(NpRtXmppClient *client);
int npRtXmppClientCheckKeepAlive(NpRtXmppClient *client, SceUInt8 *outEnabled,
				 SceUInt32 *outRemainingUsec);
int npRtXmppClientSendKeepAlive(NpRtXmppClient *client);
int npRtXmppClientRecvStanza(NpRtXmppClient *client, NpRtAnyStanza *outStanza);

/* SceNpBasic roster & privacy service (np_rt_basic.cpp, 0x81316C1C..0x813176AB) */
int npBasicInit(void);
int npBasicTerm(void);
void npBasicSetCallbacks(const NpBasicCallbacks *callbacks);
int npBasicRequestRoster(int sessionHandle, int transactionId, NpBasicRosterGetCallback callback);
void npBasicReleaseRosterList(NpBasicRosterList *rosterList);
void npBasicReleasePrivacyList(NpBasicPrivacyList *privacyList);

#endif /* SHELL_NP_RT_CORE_H */
