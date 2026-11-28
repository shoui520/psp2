/*
 * np_websocket_internal.h: Internal types, error codes, SceSslInternal imports,
 * and class declarations for SceNpWebSocket (vsh/app/common/src/np/np_websocket).
 * 3.65-CEX: 0x813FA7F8..0x81402D33
 */
#ifndef NP_WEBSOCKET_INTERNAL_H
#define NP_WEBSOCKET_INTERNAL_H

#include <stddef.h>
#include <fw.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <net/socket.h>
#include <net/in.h>
#include <net/inet.h>
#include <net/epoll.h>
#include <net/resolver.h>
#include <net/errno.h>
#include "../np_push_vsh/np_push_internal.h"

/* ============================================================================
 * SceNpWebSocket Error Codes (0x8055A1xx)
 * ============================================================================ */

#define SCE_NP_WS_ERROR_INTERNAL		(-2141871872)	/* 0x8055A100 */
#define SCE_NP_WS_ERROR_INVALID_ARGUMENT	(-2141871869)	/* 0x8055A103 */
#define SCE_NP_WS_ERROR_OUT_OF_MEMORY		(-2141871868)	/* 0x8055A104 */
#define SCE_NP_WS_ERROR_INVALID_STATE		(-2141871867)	/* 0x8055A105 */
#define SCE_NP_WS_ERROR_ABORTED			(-2141871866)	/* 0x8055A106 */
#define SCE_NP_WS_ERROR_HTTP_STATUS		(-2141871865)	/* 0x8055A107 */
#define SCE_NP_WS_ERROR_HANDSHAKE		(-2141871864)	/* 0x8055A108 */
#define SCE_NP_WS_ERROR_PROTOCOL		(-2141871863)	/* 0x8055A109 */
#define SCE_NP_WS_ERROR_DISCONNECTED		(-2141871862)	/* 0x8055A10A */
#define SCE_NP_WS_ERROR_TIMEOUT			(-2141871861)	/* 0x8055A10B */
#define SCE_NP_WS_ERROR_TOO_MANY_HEADERS	(-2141871860)	/* 0x8055A10C */
#define SCE_NP_WS_ERROR_HEADER_NOT_FOUND	(-2141871859)	/* 0x8055A10D */
#define SCE_NP_WS_ERROR_NO_RESPONSE		(-2141871858)	/* 0x8055A10E */

#define SCE_NP_WS_ERROR_SSL_HANDSHAKE		(-2141871646)	/* 0x8055A1E2 */
#define SCE_NP_WS_ERROR_SSL_WRITE		(-2141871645)	/* 0x8055A1E3 */
#define SCE_NP_WS_ERROR_SSL_READ		(-2141871644)	/* 0x8055A1E4 */
#define SCE_NP_WS_ERROR_SSL_ALREADY_OPEN	(-2141871643)	/* 0x8055A1E5 */

#define SCE_NET_ERROR_EABORT_MASKED		(-2143223537)	/* 0x8041010F */

#define NP_WS_MAX_CUSTOM_HEADERS		32
#define NP_WS_HOST_MAX				128
#define NP_WS_PATH_MAX				256
#define NP_WS_ORIGIN_MAX			256
#define NP_WS_PROTOCOL_MAX			64
#define NP_WS_KEY_BUF_SIZE			64
#define NP_WS_HTTP_BUF_SIZE			2048
#define NP_WS_DEFAULT_STREAM_BUF_SIZE		0x2000U
#define NP_WS_NET_RECV_CHUNK_SIZE		1024U
#define NP_WS_CTRL_PAYLOAD_MAX			125U

/* RFC 6455 opcodes */
#define NP_WS_OPCODE_CONTINUATION		0
#define NP_WS_OPCODE_TEXT			1
#define NP_WS_OPCODE_BINARY			2
#define NP_WS_OPCODE_CLOSE			8
#define NP_WS_OPCODE_PING			9
#define NP_WS_OPCODE_PONG			10

/* Public receive message types returned by npWebSocketReceive */
#define NP_WS_MSG_TYPE_TEXT			1
#define NP_WS_MSG_TYPE_BINARY			2
#define NP_WS_MSG_TYPE_CLOSE			3

#define NP_WS_SOURCE_PATH(rel) \
	"W:\\Build\\" NP_PUSH_BUILD_ID "\\vsh\\app\\common\\src\\np\\np_websocket\\" rel

/* ============================================================================
 * External Imports (SceNpCommonPs4, SceHttp, SceSslInternal, SceKernel)
 * ============================================================================ */

extern "C" {

extern void *SceNpCommonPs4_67BBE5F9(NpPushAllocator *allocator, void *pointer, SceSize size);
extern int SceNpCommonPs4_EC456A83(const void *srcBytes, char *outBase64, SceSize srcLen);
extern int SceNpCommonPs4_F56B0EAB(SceInt64 *outClockUsec);

extern int sceHttpParseStatusLine(const char *statusLine, SceSize lineLen,
				  int *outMajorVer, int *outMinorVer, int *outStatusCode,
				  const char **outReasonPhrase, SceSize *outPhraseLen);
extern int SceHttp_A9A39BA1(void *sslCtx, void *verifyConfig);

extern int SceSslInternal_2185BB05(void **outMethod);
extern void *SceSslInternal_A374CD7B(void);
extern void *SceSslInternal_E746465E(void *method, void *clientMethod);
extern int SceSslInternal_67FB1CD3(void *sslCtx, const void *seedBuf, SceSize seedLen);
extern int SceSslInternal_461C5990(int mode, int fallbackError);
extern void SceSslInternal_FBEB1FD7(void *sslCtx, SceUInt32 options);
extern void SceSslInternal_CC7F4211(void *sslCtx, int cmd, int arg, void *ptr);
extern void *SceSslInternal_D28834E6(void *sslCtx);
extern void SceSslInternal_0DB6C206(void *sslObj, SceUInt32 flags);
extern void *SceSslInternal_FFE2ED51(SceNetId sock, int closeFlag);
extern void SceSslInternal_4A1557A7(void *sslObj, void *bio);
extern void SceSslInternal_F2C9B744(void *sslObj, void (*infoCb)(void *, SceInt16, SceUInt8));
extern void SceSslInternal_F71DB06F(void *sslObj, void (*msgCb)(void), void *arg);
extern void SceSslInternal_CEE46B33(void *sslObj);
extern int SceSslInternal_E5EDD91B(void *sslObj);
extern int SceSslInternal_2F0FC3CD(void *sslObj, int ioResult);
extern int SceSslInternal_4F781BB6(void *sslObj, void *buf, int len);
extern int sceSslInternalWrite(void *sslObj, const void *buf, int len);
extern int SceSslInternal_EC9FAF42(void *sslObj);
extern void SceSslInternal_19B2BB58(void *sslObj);
extern void SceSslInternal_EF9EBAC7(void *sslCtx);
extern int SceSslInternal_D776E2F8(void *sslObj);

extern int sceKernelGetRandomNumber(void *output, SceSize size);
extern SceUInt32 sceKernelGetProcessTimeLow(void);

} /* extern "C" */

namespace sce {
namespace Np {
namespace WebSocket {

/* ============================================================================
 * Network & TLS Transport (np_websocket_net.cpp)
 * ============================================================================ */

class WsNetImpl {
public:
	explicit WsNetImpl(NpPushAllocator *allocator);
	~WsNetImpl();

	int Init(void);
	void Finalize(void);
	bool IsInitialized(void) const { return m_initialized; }

	SceNetId CreateResolver(const char *name);
	int ResolveName(SceNetId resolverId, const char *hostname, SceNetInAddr *outAddr,
			int timeoutUsec, int retryCount, int flags);
	int DestroyResolver(SceNetId resolverId);
	int AbortResolver(SceNetId resolverId, int flags);

	const char *InetNtop(int af, const void *src, char *dst, SceSize size);
	int InetPton(int af, const char *src, void *dst);

	SceNetId CreateTcpSocket(const char *name);
	int ShutdownSocket(SceNetId sock, int how);
	int CloseSocket(SceNetId sock);
	int AbortSocket(SceNetId sock, int flags);
	int SetSockOpt(SceNetId sock, int level, int optName, const void *optVal, SceSize optLen);
	int GetSockOpt(SceNetId sock, int level, int optName, void *optVal, SceSize *optLen);
	int ConnectSocket(SceNetId sock, const SceNetSockaddr *addr, SceSize addrLen);
	int RecvSocket(SceNetId sock, void *buf, SceSize len, int flags);
	int SendSocket(SceNetId sock, const void *buf, SceSize len, int flags);

	int CreateSslSession(int unusedCtx, SceNetId sock, const char *hostName);
	int DestroySslSession(int sslHandle);
	int PerformSslHandshake(int sslHandle);
	int RecvSsl(int sslHandle, void *buf, SceSize len);
	int CheckSslPending(int sslHandle, bool *outHasPending);
	int SendSsl(int sslHandle, const void *buf, SceSize len);
	int ShutdownSsl(int sslHandle);
	int ConfigureSslVerify(int sslHandle, bool disableVerify);

	SceNetId CreateEpoll(const char *name);
	int DestroyEpoll(SceNetId epollId);
	int AbortEpoll(SceNetId epollId, int flags);
	int ControlEpoll(SceNetId epollId, int op, SceNetId fd, SceNetEpollEvent *event);
	int WaitEpoll(SceNetId epollId, SceNetEpollEvent *events, int maxEvents, int timeoutUsec);

private:
	NpPushMutex m_sslLock;
	bool m_initialized;
};

class WsNet {
public:
	explicit WsNet(WsNetImpl *impl);
	~WsNet();

	int Init(int resolverParam, int sslContextParam);
	void Finalize(void);
	WsNetImpl *GetImpl(void) const { return m_impl; }

	void SetNonBlocking(int nonBlocking);
	int GetSocketError(void);
	int Abort(void);
	void ClearAbort(void);
	int CheckAborted(void);

	int CreateResolver(void);
	int ResolveHost(const char *hostName, int timeoutUsec, int retryCount);
	int DestroyResolver(void);

	int CreateSocket(void);
	int Connect(const char *hostName, SceUShort16 port, int tlsMode, bool disableVerify);
	int Shutdown(void);
	int Close(void);

	int Recv(void *buf, SceSize len);
	int HasPendingData(bool *outHasPending);
	int Send(const void *buf, SceSize len);

	SceNetId CreateEpoll(const char *name);
	int DestroyEpoll(SceNetId epollId);
	int ControlResolverEpoll(SceNetId epollId, SceNetEpollEvent *event, int opMode);
	int ControlSocketEpoll(SceNetId epollId, SceNetEpollEvent *event, int opMode);
	int WaitEpoll(SceNetId epollId, SceNetEpollEvent *events, int maxEvents, int timeoutUsec);
	int AbortEpoll(SceNetId epollId);

private:
	WsNetImpl *m_impl;
	int m_aborted;
	int m_nonBlocking;
	SceNetId m_resolverId;
	SceNetId m_socketId;
	int m_sslHandle;
	NpPushMutex m_mutex;
	int m_resolverParam;
	int m_sslContextParam;
	int m_tlsMode;
	int m_useTls;
	bool m_disableVerify;
	SceNetSockaddrIn m_remoteAddr;
};

/* ============================================================================
 * HTTP/1.1 WebSocket Opening Handshake (np_websocket_handshake.cpp)
 * ============================================================================ */

class WsHandshake {
public:
	WsHandshake();
	~WsHandshake();

	int GenerateKey(void);
	int BuildUpgradeRequest(const char *host, const char *path, const char *origin,
				const char *subProtocol, char *outBuf, SceSize bufSize);
	int AppendCustomHeader(const char *name, const char *value, char *outBuf, SceSize bufSize);
	int TerminateRequest(char *outBuf, SceSize bufSize);

	int ReceiveResponse(WsNet *net, SceSize *outTotalReceived, SceSize *outHeaderEnd);
	int ParseStatusCode(void) const;
	int FindResponseHeader(const char *headerName, const char **outValue,
			       SceSize *outValueLen) const;
	int VerifyResponse(void) const;

	const char *GetResponseBuffer(void) const { return m_responseBuf; }
	SceSize GetResponseBufferSize(void) const { return sizeof(m_responseBuf); }
	SceSize GetResponseHeaderLength(void) const { return m_responseHeaderLen; }
	void SetResponseHeaderLength(SceSize len) { m_responseHeaderLen = len; }
	void ResetResponse(void);

private:
	int FindHeaderEndOffset(void) const;
	int VerifyAcceptKey(const char *acceptValue) const;

	char m_clientKey[NP_WS_KEY_BUF_SIZE];
	int m_hasResponse;
	char m_responseBuf[NP_WS_HTTP_BUF_SIZE];
	SceSize m_responseHeaderLen;
	int m_expectSubProtocol;
	char m_subProtocol[NP_WS_PROTOCOL_MAX + 1];
};

/* ============================================================================
 * RFC 6455 Stream Buffer & Frame Builder (util/np_websocket_util_stream.cpp)
 * ============================================================================ */

struct WsControlFrame {
	int opcode;
	SceSize payloadLen;
	SceUInt8 body[NP_WS_CTRL_PAYLOAD_MAX + 3];
};

struct WsParsedFrame {
	int opcode;
	int fin;
	int masked;
	SceUInt32 maskKey;
	SceSize payloadLen;
	SceUInt8 *start;
	SceUInt8 *payload;
	SceUInt8 *end;
};

class WsStream {
public:
	WsStream(NpPushAllocator *allocator, SceSize bufferCapacity, WsNet *net);
	~WsStream();

	int Init(void);
	void Finalize(void);

	int AppendData(const void *srcData, SceSize dataLen);
	bool HasUnreadData(void) const { return m_hasData != 0; }
	int GetMessage(SceSize outCapacity, char *outBuf, int *outMsgType, SceSize *outMsgLen,
		       int *outIsControl, WsControlFrame *outCtrlFrame);

	static void ApplyMaskKey(SceUInt8 *dst, const SceUInt8 *src, SceSize len,
				 SceUInt32 maskKey);

private:
	SceSize CompactBuffer(void);
	SceSize removeFromBuffer(const WsParsedFrame &frame);
	int ParseFrameAtIndex(int frameIndex, const SceUInt8 *buf, SceSize bufLen,
			      WsParsedFrame *outFrame) const;
	int FindFrameOffset(int frameIndex, const SceUInt8 *buf, SceSize bufLen,
			    SceSize *outOffset) const;
	int AdvanceFrameOffset(SceSize currentOffset, SceSize *outNextOffset,
			       const SceUInt8 *buf, SceSize bufLen) const;
	static bool IsControlOpcode(int opcode);
	int ParseRsvBits(const SceUInt8 *buf, SceSize bufLen, SceSize offset,
			 bool *outRsvFirst, bool *outRsvSecond, bool *outRsvThird) const;
	static int ParsePayloadLength(const SceUInt8 *buf, SceSize bufLen, SceSize offset,
				      SceUInt64 *outLength, int *outRawLenCode);
	int ParseMaskKey(const SceUInt8 *buf, SceSize bufLen, SceSize offset,
			 SceUInt64 payloadLen, int lenCode, SceUInt32 *outMaskKey) const;

	WsNet *m_net;
	NpPushAllocator *m_allocator;
	SceSize m_capacity;
	SceUInt8 *m_pBuffer;
	SceSize m_writeIndex;
	SceSize m_readIndex;
	int m_hasData;
};

class WsFrameBuilder {
public:
	WsFrameBuilder() {}
	~WsFrameBuilder() {}

	static SceSize CalcEncodedSize(SceSize payloadLen) { return payloadLen + 64U; }
	static int BuildClientFrame(SceUInt8 *outBuf, SceSize outBufSize, int opcode,
				    const void *payload, SceSize payloadLen,
				    SceSize *outFrameLen);
};

/* ============================================================================
 * WebSocket Connection, State Machine, Visitor Handlers, & Events
 * (np_websocket_connection.cpp)
 * ============================================================================ */

class WsConnection;
class WsHandlerBase;
class WsStateContext;

enum WsConnectionState {
	WS_STATE_INIT		= 0,
	WS_STATE_CONNECTING	= 1,
	WS_STATE_CONNECTED	= 2,
	WS_STATE_OPEN		= 3,
	WS_STATE_CLOSING	= 4,
	WS_STATE_COUNT		= 5
};

/* --- Event Hierarchy (Visitor Acceptors) --- */

class WsEventBase {
public:
	explicit WsEventBase(WsConnection *conn);
	virtual ~WsEventBase();

	WsConnection *GetConnection(void) const { return m_connection; }
	virtual int Accept(WsHandlerBase *handler) = 0;

protected:
	WsConnection *m_connection;
};

class WsSendEventBase : public WsEventBase {
public:
	explicit WsSendEventBase(WsConnection *conn);
	virtual ~WsSendEventBase();

	int AllocEncodedBuffer(NpPushAllocator *allocator, SceSize size);
	int FreeEncodedBuffer(NpPushAllocator *allocator);
	void SetEncodedLength(SceSize len) { m_encodedLen = len; }
	SceSize GetEncodedLength(void) const { return m_encodedLen; }

	virtual int Accept(WsHandlerBase *handler);
	virtual void GetEncodedBuffer(SceUInt8 **outBuf, SceSize *outCap) const;
	virtual int GetPayload(const void **outData, SceSize *outLen) const;

	WsSendEventBase *m_next;
	WsSendEventBase *m_prev;

protected:
	SceSize m_encodedLen;
	SceUInt8 *m_encodedBuf;
	SceSize m_encodedBufCap;
};

class WsControlEventBase : public WsSendEventBase {
public:
	explicit WsControlEventBase(WsConnection *conn);
	virtual ~WsControlEventBase();

	int SetPayload(const void *data, SceSize len);
	virtual int GetPayload(const void **outData, SceSize *outLen) const;

protected:
	SceUInt8 m_payload[NP_WS_CTRL_PAYLOAD_MAX + 3];
	SceSize m_payloadLen;
};

class WsCloseEvent : public WsControlEventBase {
public:
	explicit WsCloseEvent(WsConnection *conn);
	virtual ~WsCloseEvent();
	virtual int Accept(WsHandlerBase *handler);
};

class WsPingEvent : public WsControlEventBase {
public:
	explicit WsPingEvent(WsConnection *conn);
	virtual ~WsPingEvent();
	virtual int Accept(WsHandlerBase *handler);
};

class WsPongEvent : public WsControlEventBase {
public:
	explicit WsPongEvent(WsConnection *conn);
	virtual ~WsPongEvent();
	virtual int Accept(WsHandlerBase *handler);
};

class WsSendTextEvent : public WsSendEventBase {
public:
	explicit WsSendTextEvent(WsConnection *conn);
	virtual ~WsSendTextEvent();

	int SetText(const char *text, SceSize maxLen);
	virtual int Accept(WsHandlerBase *handler);
	virtual int GetPayload(const void **outData, SceSize *outLen) const;

private:
	const char *m_text;
	SceSize m_textLen;
};

class WsRecvEvent : public WsEventBase {
public:
	explicit WsRecvEvent(WsConnection *conn);
	virtual ~WsRecvEvent();
	virtual int Accept(WsHandlerBase *handler);
};

/* --- Handler Hierarchy (Chain-of-Responsibility Visitors) --- */

class WsHandlerBase {
public:
	WsHandlerBase();
	virtual ~WsHandlerBase();

	void SetNext(WsHandlerBase *next) { m_next = next; }
	WsHandlerBase *GetNext(void) const { return m_next; }
	int InitChain(WsConnection *conn);
	void DestroyChain(NpPushAllocator *allocator);
	void EnterChain(void);
	void ExitChain(void);

	virtual int VisitPing(WsPingEvent *event, int prevResult);
	virtual int VisitPong(WsPongEvent *event, int prevResult);
	virtual int VisitClose(WsCloseEvent *event, int prevResult);
	virtual int VisitBinary(WsSendEventBase *event, int prevResult);
	virtual int VisitText(WsSendTextEvent *event, int prevResult);
	virtual int VisitRecv(WsRecvEvent *event, int prevResult);
	virtual int Init(void);
	virtual void OnEnter(void);
	virtual void OnExit(void);

protected:
	WsHandlerBase *m_next;
	WsConnection *m_connection;
};

class WsResolveHandler : public WsHandlerBase {
public:
	WsResolveHandler();
	virtual ~WsResolveHandler();
	virtual int VisitRecv(WsRecvEvent *event, int prevResult);
};

class WsConnectHandler : public WsHandlerBase {
public:
	WsConnectHandler();
	virtual ~WsConnectHandler();
	virtual int VisitRecv(WsRecvEvent *event, int prevResult);
};

class WsHandshakeHandler : public WsHandlerBase {
public:
	WsHandshakeHandler();
	virtual ~WsHandshakeHandler();
	virtual int VisitRecv(WsRecvEvent *event, int prevResult);
};

class WsRecvHandler : public WsHandlerBase {
public:
	WsRecvHandler();
	virtual ~WsRecvHandler();
	virtual int VisitRecv(WsRecvEvent *event, int prevResult);
	virtual void OnEnter(void);
	virtual void OnExit(void);

private:
	SceNetEpollEvent m_epollEvent;
};

class WsStreamHandler : public WsHandlerBase {
public:
	WsStreamHandler();
	virtual ~WsStreamHandler();
	virtual int VisitRecv(WsRecvEvent *event, int prevResult);
};

class WsCtrlFrameHandler : public WsHandlerBase {
public:
	WsCtrlFrameHandler();
	virtual ~WsCtrlFrameHandler();
	virtual int VisitPing(WsPingEvent *event, int prevResult);
	virtual int VisitPong(WsPongEvent *event, int prevResult);
	virtual int VisitClose(WsCloseEvent *event, int prevResult);
	virtual int VisitRecv(WsRecvEvent *event, int prevResult);

private:
	int BuildControlFrame(WsSendEventBase *event, int prevResult, WsConnection *conn,
			      int opcode);
	WsFrameBuilder m_frameBuilder;
};

class WsSendHandler : public WsHandlerBase {
public:
	WsSendHandler();
	virtual ~WsSendHandler();
	virtual int VisitPing(WsPingEvent *event, int prevResult);
	virtual int VisitPong(WsPongEvent *event, int prevResult);
	virtual int VisitClose(WsCloseEvent *event, int prevResult);
	virtual int VisitBinary(WsSendEventBase *event, int prevResult);
	virtual int VisitText(WsSendTextEvent *event, int prevResult);
	virtual int VisitRecv(WsRecvEvent *event, int prevResult);
	virtual int Init(void);
	virtual void OnEnter(void);
	virtual void OnExit(void);
	virtual int SendEncodedEvent(WsSendEventBase *event, int prevResult, WsConnection *conn);
	virtual int BuildDataFrame(WsSendEventBase *event, int prevResult, WsConnection *conn,
				   int opcode);

private:
	SceNetEpollEvent m_epollEvent;
	WsFrameBuilder m_frameBuilder;
	NpPushMutex m_sendLock;
};

/* --- State Hierarchy (GoF State Pattern) --- */

class WsStateBase {
public:
	WsStateBase();
	virtual ~WsStateBase();

	void SetAllocator(NpPushAllocator *allocator) { m_allocator = allocator; }
	WsHandlerBase *GetHandlerChain(void) const { return m_handlerChain; }

	virtual int Init(WsConnection *conn) = 0;
	virtual int Finalize(void);
	virtual int OnEnter(void);
	virtual int OnExit(void);
	virtual int GetResponseStatusCode(WsConnection *conn);
	virtual int GetResponseHeader(WsConnection *conn, const char *name, const char **outValue,
				      SceSize *outValueLen);
	virtual int PrepareConnect(WsStateContext *ctx);
	virtual int StartConnect(WsStateContext *ctx);
	virtual int CompleteHandshake(WsStateContext *ctx);
	virtual int SendBinary(WsStateContext *ctx, WsSendEventBase *event);
	virtual int SendText(WsStateContext *ctx, WsSendEventBase *event);
	virtual int BeginClose(WsStateContext *ctx);
	virtual int SendClose(WsStateContext *ctx, WsSendEventBase *event);
	virtual int Receive(WsStateContext *ctx);

protected:
	void CleanupChain(void);

	int m_stateId;
	WsHandlerBase *m_handlerChain;
	NpPushAllocator *m_allocator;
};

class WsInitState : public WsStateBase {
public:
	WsInitState();
	virtual ~WsInitState();
	virtual int Init(WsConnection *conn);
	virtual int PrepareConnect(WsStateContext *ctx);
};

class WsConnectingState : public WsStateBase {
public:
	WsConnectingState();
	virtual ~WsConnectingState();
	virtual int Init(WsConnection *conn);
	virtual int PrepareConnect(WsStateContext *ctx);
	virtual int StartConnect(WsStateContext *ctx);
};

class WsConnectedState : public WsStateBase {
public:
	WsConnectedState();
	virtual ~WsConnectedState();
	virtual int Init(WsConnection *conn);
	virtual int GetResponseStatusCode(WsConnection *conn);
	virtual int GetResponseHeader(WsConnection *conn, const char *name, const char **outValue,
				      SceSize *outValueLen);
	virtual int CompleteHandshake(WsStateContext *ctx);
};

class WsOpenState : public WsStateBase {
public:
	WsOpenState();
	virtual ~WsOpenState();
	virtual int Init(WsConnection *conn);
	virtual int GetResponseStatusCode(WsConnection *conn);
	virtual int GetResponseHeader(WsConnection *conn, const char *name, const char **outValue,
				      SceSize *outValueLen);
	virtual int SendBinary(WsStateContext *ctx, WsSendEventBase *event);
	virtual int SendText(WsStateContext *ctx, WsSendEventBase *event);
	virtual int BeginClose(WsStateContext *ctx);
	virtual int Receive(WsStateContext *ctx);
};

class WsClosingState : public WsStateBase {
public:
	WsClosingState();
	virtual ~WsClosingState();
	virtual int Init(WsConnection *conn);
	virtual int SendClose(WsStateContext *ctx, WsSendEventBase *event);
};

/* --- State Context ("SceNpWsStateCtx") --- */

class WsStateContext {
public:
	explicit WsStateContext(NpPushAllocator *allocator);
	virtual ~WsStateContext();

	int Init(WsConnection *conn);
	int Finalize(void);

	int GetResponseStatusCode(WsConnection *conn);
	int GetResponseHeader(WsConnection *conn, const char *name, const char **outValue,
			      SceSize *outValueLen);
	int Connect(void);
	int SendText(WsSendEventBase *event);
	int SendClose(WsSendEventBase *event);
	int Receive(void);

	int SetState(unsigned int stateIndex);
	int DispatchRecv(WsHandlerBase *chain);
	int DispatchSend(WsHandlerBase *chain, WsSendEventBase *event);
	int EnqueuePing(const void *payload, SceSize len);
	int EnqueuePong(const void *payload, SceSize len);
	int EnqueueControlEvent(WsSendEventBase *event);
	int RemoveControlEvent(WsSendEventBase *event);

private:
	NpPushAllocator *m_allocator;
	NpPushMutex m_mutex;
	int m_currentState;
	WsStateBase *m_states[WS_STATE_COUNT];
	WsConnection *m_connection;
	WsRecvEvent *m_recvEvent;
	WsSendEventBase *m_controlHead;
	WsSendEventBase *m_controlTail;
};

class WsConnection {
public:
	static int Create(const NpWebSocketOpenParam *param, WsConnection **outConn);
	static int Destroy(WsConnection *conn);

	int Acquire(void);
	void Release(void);

	int AddRequestHeader(const char *name, SceSize nameLen, const char *value,
			     SceSize valueLen);
	int RemoveRequestHeader(const char *name, SceSize nameLen);
	void ClearRequestHeaders(void);

	int Connect(char *respHeaderBuf, SceSize respHeaderBufSize, SceSize *outRespHeaderSize);
	int Disconnect(void);
	int Abort(bool forceClose);
	int SendText(const char *text, SceSize length);
	int SendClose(void);
	int Receive(int *outMsgType, void *outBuf, SceSize bufSize, SceSize *outReceived);
	int GetResponseStatusCode(void);
	int GetResponseHeader(const char *name, const char **outValue, SceSize *outValueLen);

	NpPushAllocator *GetAllocator(void) { return &m_allocator; }
	WsStateContext *GetStateContext(void) const { return m_stateCtx; }
	WsNet *GetNet(void) const { return m_net; }
	WsHandshake *GetHandshake(void) const { return m_handshake; }
	WsStream *GetStream(void) const { return m_stream; }
	const NpWebSocketOpenParam &GetOpenParam(void) const { return m_openParam; }
	const char *GetHost(void) const { return m_host; }
	const char *GetPath(void) const { return m_path; }
	const char *GetOrigin(void) const { return m_origin; }
	const char *GetSubProtocol(void) const { return m_subProtocol; }
	int GetHeaderCount(void) const { return m_headerCount; }
	const char *GetHeaderName(int index) const { return m_headerNames[index]; }
	const char *GetHeaderValue(int index) const { return m_headerValues[index]; }

	SceNetId &RecvEpollId(void) { return m_recvEpollId; }
	SceNetId &SendEpollId(void) { return m_sendEpollId; }
	SceUInt8 *GetNetRecvBuf(void) const { return m_netRecvBuf; }
	SceSize GetNetRecvBufSize(void) const { return m_netRecvBufSize; }
	SceSize &NetRecvBytes(void) { return m_netRecvBytes; }
	SceInt64 &LastPingClockUsec(void) { return m_lastPingClockUsec; }

	void SetReceiveTargets(void *outBuf, SceSize bufSize, int *outMsgType,
			       SceSize *outReceived)
	{
		m_recvOutBuf = outBuf;
		m_recvOutCap = bufSize;
		m_recvOutMsgType = outMsgType;
		m_recvOutLen = outReceived;
		m_recvDone = 0;
	}
	void *GetRecvOutBuf(void) const { return m_recvOutBuf; }
	SceSize GetRecvOutCap(void) const { return m_recvOutCap; }
	int *GetRecvOutMsgType(void) const { return m_recvOutMsgType; }
	SceSize *GetRecvOutLen(void) const { return m_recvOutLen; }
	int &RecvDone(void) { return m_recvDone; }

	void SetHandshakeRespBuffer(char *buf, SceSize cap, SceSize *outLen)
	{
		m_respHeaderBuf = buf;
		m_respHeaderBufCap = cap;
		m_respHeaderOutLen = outLen;
	}
	char *GetHandshakeRespBuf(void) const { return m_respHeaderBuf; }
	SceSize GetHandshakeRespCap(void) const { return m_respHeaderBufCap; }
	SceSize *GetHandshakeRespOutLen(void) const { return m_respHeaderOutLen; }

private:
	WsConnection();
	~WsConnection();
	void DestroyNow(void);
	void CloseEpolls(void);

	volatile SceUInt32 m_refCount;
	int m_destroyRequested;
	int m_aborted;
	int m_forceClosed;
	NpWebSocketOpenParam m_openParam;
	char m_host[NP_WS_HOST_MAX + 1];
	char m_path[NP_WS_PATH_MAX + 1];
	char m_origin[NP_WS_ORIGIN_MAX + 1];
	char m_subProtocol[NP_WS_PROTOCOL_MAX + 1];
	NpPushMutex *m_lock;
	SceNetId m_recvEpollId;
	SceNetId m_sendEpollId;
	NpPushAllocator m_allocator;
	int m_headerCount;
	char *m_headerNames[NP_WS_MAX_CUSTOM_HEADERS];
	char *m_headerValues[NP_WS_MAX_CUSTOM_HEADERS];
	WsStateContext *m_stateCtx;
	WsNet *m_net;
	WsHandshake *m_handshake;
	WsStream *m_stream;
	SceUInt8 *m_netRecvBuf;
	SceSize m_netRecvBufSize;
	SceSize m_netRecvBytes;
	SceInt64 m_lastPingClockUsec;
	void *m_recvOutBuf;
	SceSize m_recvOutCap;
	SceSize *m_recvOutLen;
	int *m_recvOutMsgType;
	int m_recvDone;
	char *m_respHeaderBuf;
	SceSize m_respHeaderBufCap;
	SceSize *m_respHeaderOutLen;
};

} /* namespace WebSocket */
} /* namespace Np */
} /* namespace sce */

#endif /* NP_WEBSOCKET_INTERNAL_H */
