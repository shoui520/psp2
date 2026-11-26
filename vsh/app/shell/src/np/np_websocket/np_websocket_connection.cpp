/*
 * np_websocket_connection.cpp: SceNpWebSocketConnection lifecycle, request headers,
 * epoll-driven send/receive loops with keepalive Ping and auto-Pong, and public API.
 * Sony path: vsh/app/common/src/np/np_websocket/
 * 3.65-CEX: 0x813FA7F8..0x813FC857, 0x813FE2A8..0x813FFE67, 0x8140211C..0x814026D7
 */
#include "np_websocket_internal.h"
#include <std/new>

#define NP_WS_RESOLVER_TIMEOUT_USEC		(30 * 1000 * 1000)
#define NP_WS_RESOLVER_RETRY_COUNT		3
#define NP_WS_EPOLL_ERROR_MASK			0x10018U
#define NP_WS_EPOLL_IN_EVENTS			(SCE_NET_EPOLLIN | SCE_NET_EPOLLERR | \
						 SCE_NET_EPOLLHUP | SCE_NET_EPOLLDESCID)
#define NP_WS_EPOLL_OUT_EVENTS			(SCE_NET_EPOLLOUT | SCE_NET_EPOLLERR | \
						 SCE_NET_EPOLLHUP | SCE_NET_EPOLLDESCID)

namespace sce {
namespace Np {
namespace WebSocket {

WsConnection::WsConnection()
	: m_refCount(0),
	  m_destroyRequested(0),
	  m_aborted(0),
	  m_forceClosed(0),
	  m_lock(NULL),
	  m_recvEpollId(0),
	  m_sendEpollId(0),
	  m_headerCount(0),
	  m_state(WS_STATE_INIT),
	  m_net(NULL),
	  m_handshake(NULL),
	  m_stream(NULL),
	  m_netRecvBuf(NULL),
	  m_netRecvBufSize(0),
	  m_lastPingClockUsec(0)
{
	sceClibMemset(&m_openParam, 0, sizeof(m_openParam));
	sceClibMemset(m_host, 0, sizeof(m_host));
	sceClibMemset(m_path, 0, sizeof(m_path));
	sceClibMemset(m_origin, 0, sizeof(m_origin));
	sceClibMemset(m_subProtocol, 0, sizeof(m_subProtocol));
	sceClibMemset(&m_allocator, 0, sizeof(m_allocator));
	sceClibMemset(m_headerNames, 0, sizeof(m_headerNames));
	sceClibMemset(m_headerValues, 0, sizeof(m_headerValues));
	SceNpCommonPs4_4266D870(&m_sendLock);
}

WsConnection::~WsConnection()
{
	if (SceNpCommonPs4_C78F5DF5(&m_sendLock) != 0)
		SceNpCommonPs4_63EED478(&m_sendLock);
	SceNpCommonPs4_294F0B1A(&m_sendLock);
}

/* 0x813FB214 */
int WsConnection::Acquire(void)
{
	if (m_lock == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;
	Push::ScopedLock lock(m_lock);
	++m_refCount;
	return 0;
}

/* 0x813FB292 */
void WsConnection::Release(void)
{
	bool shouldDestroy = false;
	{
		Push::ScopedLock lock(m_lock);
		if (m_refCount != 0)
			--m_refCount;
		if (m_destroyRequested != 0 && m_refCount == 0)
			shouldDestroy = true;
	}
	if (shouldDestroy)
		DestroyNow();
}

/* 0x813FB316 */
void WsConnection::ClearRequestHeaders(void)
{
	for (int idx = 0; idx < m_headerCount; ++idx) {
		if (m_headerNames[idx] != NULL) {
			SceNpCommonPs4_6B428829(&m_allocator, m_headerNames[idx]);
			m_headerNames[idx] = NULL;
		}
		if (m_headerValues[idx] != NULL) {
			SceNpCommonPs4_6B428829(&m_allocator, m_headerValues[idx]);
			m_headerValues[idx] = NULL;
		}
	}
	m_headerCount = 0;
}

/* 0x813FB3D6 */
int WsConnection::AddRequestHeader(const char *name, SceSize nameLen, const char *value,
				   SceSize valueLen)
{
	if (name == NULL || value == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;
	if (m_headerCount + 1 >= NP_WS_MAX_CUSTOM_HEADERS)
		return SCE_NP_WS_ERROR_TOO_MANY_HEADERS;

	SceSize actualNameLen = sceClibStrnlen(name, nameLen);
	SceSize actualValueLen = sceClibStrnlen(value, valueLen);
	if (actualNameLen == 0 || actualValueLen == 0)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;

	char *nameCopy = static_cast<char *>(
		SceNpCommonPs4_AA980113(&m_allocator, actualNameLen + 1U));
	if (nameCopy == NULL)
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	sceClibStrncpy(nameCopy, name, actualNameLen);
	nameCopy[actualNameLen] = '\0';

	char *valueCopy = static_cast<char *>(
		SceNpCommonPs4_AA980113(&m_allocator, actualValueLen + 1U));
	if (valueCopy == NULL) {
		SceNpCommonPs4_6B428829(&m_allocator, nameCopy);
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	}
	sceClibStrncpy(valueCopy, value, actualValueLen);
	valueCopy[actualValueLen] = '\0';

	m_headerNames[m_headerCount] = nameCopy;
	m_headerValues[m_headerCount] = valueCopy;
	++m_headerCount;
	return 0;
}

/* 0x813FB558 */
int WsConnection::RemoveRequestHeader(const char *name, SceSize nameLen)
{
	if (name == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;

	int foundIdx = -1;
	for (int idx = 0; idx < m_headerCount; ++idx) {
		if (m_headerNames[idx] != NULL &&
		    sceClibStrncmp(m_headerNames[idx], name, nameLen) == 0) {
			foundIdx = idx;
			break;
		}
	}
	if (foundIdx < 0)
		return SCE_NP_WS_ERROR_HEADER_NOT_FOUND;

	if (m_headerNames[foundIdx] != NULL) {
		SceNpCommonPs4_6B428829(&m_allocator, m_headerNames[foundIdx]);
		m_headerNames[foundIdx] = NULL;
	}
	if (m_headerValues[foundIdx] != NULL) {
		SceNpCommonPs4_6B428829(&m_allocator, m_headerValues[foundIdx]);
		m_headerValues[foundIdx] = NULL;
	}

	for (int idx = foundIdx + 1; idx < m_headerCount; ++idx) {
		m_headerNames[idx - 1] = m_headerNames[idx];
		m_headerValues[idx - 1] = m_headerValues[idx];
	}
	--m_headerCount;
	return 0;
}

void WsConnection::CloseEpolls(void)
{
	if (m_net == NULL)
		return;
	if (m_recvEpollId > 0) {
		m_net->DestroyEpoll(m_recvEpollId);
		m_recvEpollId = 0;
	}
	if (m_sendEpollId > 0) {
		m_net->DestroyEpoll(m_sendEpollId);
		m_sendEpollId = 0;
	}
}

/* 0x813FF2B6 */
int WsConnection::EnsureRecvEpoll(void)
{
	if (m_recvEpollId > 0)
		return 0;

	m_net->SetNonBlocking(1);
	SceNetId epollId = m_net->CreateEpoll("SceNpWsReceive");
	if (epollId < 0)
		return epollId;

	SceNetEpollEvent ev;
	sceClibMemset(&ev, 0, sizeof(ev));
	ev.events = NP_WS_EPOLL_IN_EVENTS;
	int ret = m_net->ControlSocketEpoll(epollId, &ev, 1);
	if (ret < 0) {
		m_net->DestroyEpoll(epollId);
		return ret;
	}
	m_recvEpollId = epollId;
	return 0;
}

/* 0x813FF9F6 */
int WsConnection::EnsureSendEpoll(void)
{
	if (m_sendEpollId > 0)
		return 0;

	m_net->SetNonBlocking(1);
	SceNetId epollId = m_net->CreateEpoll("SceNpWsSend");
	if (epollId < 0)
		return epollId;

	SceNetEpollEvent ev;
	sceClibMemset(&ev, 0, sizeof(ev));
	ev.events = NP_WS_EPOLL_OUT_EVENTS;
	int ret = m_net->ControlSocketEpoll(epollId, &ev, 1);
	if (ret < 0) {
		m_net->DestroyEpoll(epollId);
		return ret;
	}
	m_sendEpollId = epollId;
	return 0;
}

/* 0x813FB6EA: Final object teardown once refCount hits zero */
void WsConnection::DestroyNow(void)
{
	ClearRequestHeaders();

	if (m_netRecvBuf != NULL) {
		SceNpCommonPs4_6B428829(&m_allocator, m_netRecvBuf);
		m_netRecvBuf = NULL;
	}
	if (m_stream != NULL) {
		m_stream->Finalize();
		m_stream->~WsStream();
		SceNpCommonPs4_6B428829(&m_allocator, m_stream);
		m_stream = NULL;
	}
	if (m_handshake != NULL) {
		m_handshake->~WsHandshake();
		SceNpCommonPs4_6B428829(&m_allocator, m_handshake);
		m_handshake = NULL;
	}
	if (m_net != NULL) {
		CloseEpolls();
		m_net->Finalize();
		WsNetImpl *impl = m_net->GetImpl();
		if (impl != NULL) {
			impl->Finalize();
			impl->~WsNetImpl();
			SceNpCommonPs4_6B428829(&m_allocator, impl);
		}
		m_net->~WsNet();
		SceNpCommonPs4_6B428829(&m_allocator, m_net);
		m_net = NULL;
	}
	if (m_lock != NULL) {
		if (SceNpCommonPs4_C78F5DF5(m_lock) != 0)
			SceNpCommonPs4_63EED478(m_lock);
		SceNpCommonPs4_294F0B1A(m_lock);
		SceNpCommonPs4_6B428829(&m_allocator, m_lock);
		m_lock = NULL;
	}

	NpPushAllocator savedAlloc = m_allocator;
	this->~WsConnection();
	SceNpCommonPs4_6B428829(&savedAlloc, this);
}

/* 0x813FACF0: Allocate and initialize SceNpWebSocketConnection */
int WsConnection::Create(const NpWebSocketOpenParam *param, WsConnection **outConn)
{
	if (param == NULL || outConn == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;
	*outConn = NULL;

	if (param->allocator.allocate == NULL || param->allocator.deallocate == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;
	if (param->host == NULL || param->path == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;

	NpPushAllocator alloc = param->allocator;
	void *rawMem = SceNpCommonPs4_AA980113(&alloc, sizeof(WsConnection));
	if (rawMem == NULL)
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;

	WsConnection *conn = new (rawMem) WsConnection();
	conn->m_openParam = *param;
	conn->m_allocator = alloc;
	sceClibStrncpy(conn->m_host, param->host, NP_WS_HOST_MAX);
	sceClibStrncpy(conn->m_path, param->path, NP_WS_PATH_MAX);
	if (param->origin != NULL)
		sceClibStrncpy(conn->m_origin, param->origin, NP_WS_ORIGIN_MAX);
	if (param->subProtocol != NULL)
		sceClibStrncpy(conn->m_subProtocol, param->subProtocol, NP_WS_PROTOCOL_MAX);

	int ret = 0;
	void *lockMem = SceNpCommonPs4_0C922423(sizeof(NpPushMutex), &conn->m_allocator);
	if (lockMem == NULL) {
		ret = SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	} else {
		conn->m_lock = SceNpCommonPs4_4266D870(static_cast<NpPushMutex *>(lockMem));
		ret = SceNpCommonPs4_8AA7E995(conn->m_lock, "SceNpWebSocketConnection", 0);
	}

	if (ret >= 0)
		ret = SceNpCommonPs4_8AA7E995(&conn->m_sendLock, "SceNpWsSend", 0);

	WsNetImpl *netImpl = NULL;
	if (ret >= 0) {
		void *implMem = SceNpCommonPs4_0C922423(sizeof(WsNetImpl), &conn->m_allocator);
		if (implMem == NULL) {
			ret = SCE_NP_WS_ERROR_OUT_OF_MEMORY;
		} else {
			netImpl = new (implMem) WsNetImpl(&conn->m_allocator);
			ret = netImpl->Init();
		}
	}

	if (ret >= 0) {
		void *netMem = SceNpCommonPs4_0C922423(sizeof(WsNet), &conn->m_allocator);
		if (netMem == NULL) {
			ret = SCE_NP_WS_ERROR_OUT_OF_MEMORY;
		} else {
			conn->m_net = new (netMem) WsNet(netImpl);
			ret = conn->m_net->Init(0, param->libHttpCtxId);
		}
	}

	if (ret >= 0) {
		void *hsMem = SceNpCommonPs4_0C922423(sizeof(WsHandshake), &conn->m_allocator);
		if (hsMem == NULL)
			ret = SCE_NP_WS_ERROR_OUT_OF_MEMORY;
		else
			conn->m_handshake = new (hsMem) WsHandshake();
	}

	if (ret >= 0) {
		void *streamMem = SceNpCommonPs4_0C922423(sizeof(WsStream), &conn->m_allocator);
		if (streamMem == NULL) {
			ret = SCE_NP_WS_ERROR_OUT_OF_MEMORY;
		} else {
			conn->m_stream = new (streamMem)
				WsStream(&conn->m_allocator, NP_WS_DEFAULT_STREAM_BUF_SIZE,
					 conn->m_net);
			ret = conn->m_stream->Init();
		}
	}

	if (ret >= 0) {
		conn->m_netRecvBufSize = NP_WS_NET_RECV_CHUNK_SIZE;
		conn->m_netRecvBuf = static_cast<SceUInt8 *>(
			SceNpCommonPs4_AA980113(&conn->m_allocator, conn->m_netRecvBufSize));
		if (conn->m_netRecvBuf == NULL)
			ret = SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	}

	if (ret >= 0) {
		*outConn = conn;
		return 0;
	}

	if (netImpl != NULL && conn->m_net == NULL) {
		if (netImpl->IsInitialized())
			netImpl->Finalize();
		netImpl->~WsNetImpl();
		SceNpCommonPs4_6B428829(&conn->m_allocator, netImpl);
	}
	conn->DestroyNow();
	return ret;
}

/* 0x813FB190 */
int WsConnection::Destroy(WsConnection *conn)
{
	if (conn == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;

	int ret = conn->Acquire();
	if (ret >= 0) {
		conn->Abort(false);
		{
			Push::ScopedLock lock(conn->m_lock);
			conn->m_destroyRequested = 1;
		}
		conn->Release();
	}
	return ret;
}

/* 0x813FB8E8 */
int WsConnection::Abort(bool forceClose)
{
	int ret = Acquire();
	if (ret < 0)
		return ret;

	{
		Push::ScopedLock lock(m_lock);
		m_aborted = 1;
		if (forceClose)
			m_forceClosed = 1;
		if (m_net != NULL) {
			ret = m_net->Abort();
			if (m_recvEpollId > 0)
				ret = m_net->AbortEpoll(m_recvEpollId);
			if (m_sendEpollId > 0)
				ret = m_net->AbortEpoll(m_sendEpollId);
		}
	}

	Release();
	return ret;
}

/* 0x813FA99C / 0x813FBE86 / 0x813FF4C6 / 0x813FE7BE / 0x81402ADE:
 * DNS resolve -> TCP/TLS connect -> HTTP/1.1 Upgrade handshake */
int WsConnection::Connect(char *respHeaderBuf, SceSize respHeaderBufSize,
			  SceSize *outRespHeaderSize)
{
	if (outRespHeaderSize != NULL)
		*outRespHeaderSize = 0;

	int ret = Acquire();
	if (ret < 0)
		return ret;

	if (m_state != WS_STATE_INIT) {
		Release();
		return SCE_NP_WS_ERROR_INVALID_STATE;
	}

	m_state = WS_STATE_CONNECTING;

	/* Step 1: DNS resolution (0x813FF4C6) */
	ret = m_net->CreateResolver();
	if (ret >= 0) {
		ret = m_net->ResolveHost(m_host, NP_WS_RESOLVER_TIMEOUT_USEC,
					 NP_WS_RESOLVER_RETRY_COUNT);
		m_net->DestroyResolver();
	}

	/* Step 2: TCP + optional TLS connect (0x813FE7BE) */
	if (ret >= 0) {
		m_net->SetNonBlocking(0);
		ret = m_net->CreateSocket();
		if (ret >= 0) {
			ret = m_net->Connect(m_host, m_openParam.port, m_openParam.useTls, false);
			if (ret < 0)
				m_net->Close();
		}
	}

	/* Step 3: HTTP/1.1 WebSocket Upgrade handshake (0x81402ADE) */
	if (ret >= 0) {
		m_state = WS_STATE_HANDSHAKING;
		char reqBuf[NP_WS_HTTP_BUF_SIZE];
		sceClibMemset(reqBuf, 0, sizeof(reqBuf));

		m_handshake->GenerateKey();
		const char *origin = (m_origin[0] != '\0') ? m_origin : NULL;
		const char *subProto = (m_subProtocol[0] != '\0') ? m_subProtocol : NULL;

		ret = m_handshake->BuildUpgradeRequest(m_host, m_path, origin, subProto, reqBuf,
						       sizeof(reqBuf));
		for (int idx = 0; ret >= 0 && idx < m_headerCount; ++idx) {
			ret = m_handshake->AppendCustomHeader(m_headerNames[idx],
							      m_headerValues[idx], reqBuf,
							      sizeof(reqBuf));
		}
		if (ret >= 0)
			ret = m_handshake->TerminateRequest(reqBuf, sizeof(reqBuf));
		if (ret >= 0) {
			SceSize reqLen = sceClibStrnlen(reqBuf, sizeof(reqBuf));
			ret = m_net->Send(reqBuf, reqLen);
		}

		SceSize totalReceived = 0;
		SceSize headerEnd = 0;
		if (ret >= 0)
			ret = m_handshake->ReceiveResponse(m_net, &totalReceived, &headerEnd);

		if (ret >= 0 && respHeaderBuf != NULL && respHeaderBufSize > 0) {
			SceSize copyLen = (headerEnd < respHeaderBufSize - 1U)
						  ? headerEnd
						  : (respHeaderBufSize - 1U);
			sceClibMemcpy_safe(respHeaderBuf, m_handshake->GetResponseBuffer(),
					   copyLen);
			respHeaderBuf[copyLen] = '\0';
			if (outRespHeaderSize != NULL)
				*outRespHeaderSize = copyLen;
		}

		if (ret >= 0)
			ret = m_handshake->VerifyResponse();

		if (ret >= 0 && headerEnd < totalReceived) {
			m_stream->AppendData(m_handshake->GetResponseBuffer() + headerEnd,
					     totalReceived - headerEnd);
		}
	}

	if (ret >= 0) {
		m_state = WS_STATE_OPEN;
		EnsureRecvEpoll();
		EnsureSendEpoll();
	} else {
		m_net->Close();
		m_state = WS_STATE_INIT;
	}

	Release();
	return ret;
}

/* Disconnect transport and return to INIT state */
int WsConnection::Disconnect(void)
{
	int ret = Acquire();
	if (ret < 0)
		return ret;

	Abort(false);
	CloseEpolls();
	if (m_net != NULL) {
		m_net->Shutdown();
		m_net->Close();
	}
	m_state = WS_STATE_INIT;

	Release();
	return 0;
}

/* 0x813FFBDC: Wait for socket writability via epoll and send raw bytes */
int WsConnection::SendRawBuffer(const void *buf, SceSize len)
{
	if (buf == NULL || len == 0)
		return SCE_NP_WS_ERROR_INTERNAL;

	int timeoutUsec = (m_openParam.connectTimeout != 0)
				  ? static_cast<int>(m_openParam.connectTimeout)
				  : -1;

	Push::ScopedLock lock(&m_sendLock);
	int epollRet = EnsureSendEpoll();
	if (epollRet < 0)
		return epollRet;

	SceNetEpollEvent ev;
	sceClibMemset(&ev, 0, sizeof(ev));
	int waitRet = m_net->WaitEpoll(m_sendEpollId, &ev, 1, timeoutUsec);
	if (waitRet < 0)
		return waitRet;
	if (waitRet == 0 && timeoutUsec != -1)
		return SCE_NP_WS_ERROR_TIMEOUT;
	if (waitRet != 1)
		return SCE_NP_WS_ERROR_INTERNAL;

	if ((ev.events & NP_WS_EPOLL_ERROR_MASK) != 0) {
		int sockErr = m_net->GetSocketError();
		return (sockErr < 0) ? sockErr : SCE_NP_WS_ERROR_DISCONNECTED;
	}
	if ((ev.events & SCE_NET_EPOLLOUT) == 0)
		return SCE_NP_WS_ERROR_INTERNAL;

	return m_net->Send(buf, len);
}

/* 0x813FAA1C / 0x813FAB0A / 0x813FEBA0 / 0x813FFD6A: Frame & transmit an RFC 6455 message */
int WsConnection::SendFrame(int opcode, const void *payload, SceSize payloadLen)
{
	int ret = Acquire();
	if (ret < 0)
		return ret;

	if (m_state != WS_STATE_OPEN && m_state != WS_STATE_CLOSING) {
		Release();
		return SCE_NP_WS_ERROR_INVALID_STATE;
	}

	SceSize allocSize = WsFrameBuilder::CalcEncodedSize(payloadLen);
	SceUInt8 *frameBuf = static_cast<SceUInt8 *>(
		SceNpCommonPs4_AA980113(&m_allocator, allocSize));
	if (frameBuf == NULL) {
		Release();
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	}

	SceSize frameLen = 0;
	ret = WsFrameBuilder::BuildClientFrame(frameBuf, allocSize, opcode, payload, payloadLen,
					       &frameLen);
	if (ret >= 0) {
		SceSize sentTotal = 0;
		while (sentTotal < frameLen) {
			int sentNow = SendRawBuffer(frameBuf + sentTotal, frameLen - sentTotal);
			if (sentNow < 0) {
				ret = sentNow;
				break;
			}
			if (sentNow == 0) {
				ret = SCE_NP_WS_ERROR_DISCONNECTED;
				break;
			}
			sentTotal += static_cast<SceSize>(sentNow);
		}
		if (ret >= 0 && opcode == NP_WS_OPCODE_CLOSE)
			m_state = WS_STATE_CLOSING;
	}

	SceNpCommonPs4_6B428829(&m_allocator, frameBuf);
	Release();
	return (ret >= 0) ? 0 : ret;
}

/* 0x813FABF8 / 0x813FEF32 / 0x813FECF6: Epoll receive loop with keepalive Ping & auto-Pong */
int WsConnection::Receive(int *outMsgType, void *outBuf, SceSize bufSize, SceSize *outReceived)
{
	if (outBuf == NULL || bufSize == 0 || outReceived == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;

	int ret = Acquire();
	if (ret < 0)
		return ret;

	if (m_state != WS_STATE_OPEN && m_state != WS_STATE_CLOSING) {
		Release();
		return SCE_NP_WS_ERROR_INVALID_STATE;
	}

	*outReceived = 0;
	int localMsgType = 0;
	int *msgTypeDst = (outMsgType != NULL) ? outMsgType : &localMsgType;

	ret = EnsureRecvEpoll();
	while (ret >= 0) {
		if (!m_stream->HasUnreadData()) {
			int waitTimeout = -1;
			if (m_openParam.recvTimeout > 0) {
				if (m_lastPingClockUsec == 0) {
					int clkRet = SceNpCommonPs4_F56B0EAB(&m_lastPingClockUsec);
					if (clkRet < 0) {
						sceClibPrintf("[%s\t] %s() ", "NpWsHdrRecv",
							      "Visit");
						sceClibPrintf(
							"sceNpGetSystemClockUsec failed. ret = 0x%08x\n",
							clkRet);
					}
				}
				SceInt64 nowUsec = 0;
				int clkRet = SceNpCommonPs4_F56B0EAB(&nowUsec);
				if (clkRet < 0) {
					sceClibPrintf("[%s\t] %s() ", "NpWsHdrRecv", "Visit");
					sceClibPrintf(
						"sceNpGetSystemClockUsec failed. ret = 0x%08x\n",
						clkRet);
				}
				SceInt64 elapsed = nowUsec - m_lastPingClockUsec;
				SceInt64 limit = static_cast<SceInt64>(m_openParam.recvTimeout);
				waitTimeout = (elapsed > 0 && elapsed <= limit)
						      ? static_cast<int>(limit - elapsed)
						      : 1;
			}

			bool hasSslPending = false;
			ret = m_net->HasPendingData(&hasSslPending);
			if (ret < 0)
				break;

			if (!hasSslPending) {
				SceNetEpollEvent ev;
				sceClibMemset(&ev, 0, sizeof(ev));
				int waitRet = m_net->WaitEpoll(m_recvEpollId, &ev, 1, waitTimeout);
				if (waitRet < 0) {
					ret = waitRet;
					break;
				}
				if (waitRet == 0 && waitTimeout != -1) {
					const char pingByte = 'a';
					ret = SendFrame(NP_WS_OPCODE_PING, &pingByte, 1);
					if (ret < 0)
						break;
					int clkRet = SceNpCommonPs4_F56B0EAB(&m_lastPingClockUsec);
					if (clkRet < 0) {
						sceClibPrintf("[%s\t] %s() ", "NpWsHdrRecv",
							      "Visit");
						sceClibPrintf(
							"sceNpGetSystemClockUsec failed. ret = 0x%08x\n",
							clkRet);
					}
					continue;
				}
				if (waitRet != 1) {
					ret = SCE_NP_WS_ERROR_INTERNAL;
					break;
				}
				if ((ev.events & NP_WS_EPOLL_ERROR_MASK) != 0) {
					int sockErr = m_net->GetSocketError();
					ret = (sockErr < 0) ? sockErr
							    : SCE_NP_WS_ERROR_DISCONNECTED;
					break;
				}
				if ((ev.events & SCE_NET_EPOLLIN) == 0) {
					ret = SCE_NP_WS_ERROR_INTERNAL;
					break;
				}
			}

			int recvBytes = m_net->Recv(m_netRecvBuf, m_netRecvBufSize);
			if (recvBytes < 0) {
				ret = recvBytes;
				break;
			}
			if (recvBytes == 0) {
				int sockErr = m_net->GetSocketError();
				ret = (sockErr < 0) ? sockErr : SCE_NP_WS_ERROR_DISCONNECTED;
				break;
			}

			ret = m_stream->AppendData(m_netRecvBuf, static_cast<SceSize>(recvBytes));
			if (ret < 0)
				break;
		}

		int isControl = 0;
		WsControlFrame ctrlFrame;
		sceClibMemset(&ctrlFrame, 0, sizeof(ctrlFrame));
		ret = m_stream->GetMessage(bufSize, static_cast<char *>(outBuf), msgTypeDst,
					   outReceived, &isControl, &ctrlFrame);
		if (ret < 0)
			break;
		if (ret == 1) {
			ret = 0;
			continue;
		}

		if (isControl == 0) {
			ret = 0;
			break;
		}

		if (ctrlFrame.opcode == NP_WS_OPCODE_CLOSE) {
			if (bufSize < ctrlFrame.payloadLen) {
				ret = SCE_NP_WS_ERROR_OUT_OF_MEMORY;
				break;
			}
			if (ctrlFrame.payloadLen > 0)
				sceClibMemcpy_safe(outBuf, ctrlFrame.body, ctrlFrame.payloadLen);
			*msgTypeDst = NP_WS_MSG_TYPE_CLOSE;
			*outReceived = ctrlFrame.payloadLen;
			ret = 0;
			break;
		}
		if (ctrlFrame.opcode == NP_WS_OPCODE_PING) {
			ret = SendFrame(NP_WS_OPCODE_PONG, ctrlFrame.body, ctrlFrame.payloadLen);
			if (ret < 0)
				break;
			continue;
		}
		if (ctrlFrame.opcode == NP_WS_OPCODE_PONG)
			continue;

		ret = SCE_NP_WS_ERROR_PROTOCOL;
		break;
	}

	Release();
	return ret;
}

/* 0x814025C4 */
int WsConnection::GetResponseStatusCode(void) const
{
	if (m_handshake == NULL)
		return SCE_NP_WS_ERROR_INTERNAL;
	return m_handshake->ParseStatusCode();
}

/* 0x81402644 */
int WsConnection::GetResponseHeader(const char *name, const char **outValue,
				    SceSize *outValueLen) const
{
	if (m_handshake == NULL)
		return SCE_NP_WS_ERROR_INTERNAL;
	return m_handshake->FindResponseHeader(name, outValue, outValueLen);
}

} /* namespace WebSocket */
} /* namespace Np */
} /* namespace sce */

/* ============================================================================
 * Public C++ Entry Points (0x813FA7F8..0x813FABF8)
 * ============================================================================ */

/* 0x813FA7F8 */
int npWebSocketCreate(const NpWebSocketOpenParam *param, void **outHandle)
{
	if (outHandle == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;
	sce::Np::WebSocket::WsConnection *conn = NULL;
	int ret = sce::Np::WebSocket::WsConnection::Create(param, &conn);
	if (ret >= 0)
		*outHandle = conn;
	return ret;
}

/* 0x813FA820 */
int npWebSocketDestroy(void *wsHandle)
{
	return sce::Np::WebSocket::WsConnection::Destroy(
		static_cast<sce::Np::WebSocket::WsConnection *>(wsHandle));
}

/* 0x813FA844 */
int npWebSocketAddRequestHeader(void *wsHandle, const char *name, SceSize nameLen,
				const char *value, SceSize valueLen)
{
	if (wsHandle == NULL || name == NULL || value == NULL || nameLen == 0 || valueLen == 0)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;

	sce::Np::WebSocket::WsConnection *conn =
		static_cast<sce::Np::WebSocket::WsConnection *>(wsHandle);
	int ret = conn->Acquire();
	if (ret >= 0) {
		ret = conn->AddRequestHeader(name, nameLen, value, valueLen);
		conn->Release();
	}
	return ret;
}

/* 0x813FA8EA */
int npWebSocketAbort(void *wsHandle)
{
	if (wsHandle == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;
	return static_cast<sce::Np::WebSocket::WsConnection *>(wsHandle)->Abort(false);
}

/* 0x813FA99C */
int npWebSocketConnect(void *wsHandle, char *respHeaderBuf, SceSize respHeaderBufSize,
		       SceSize *outRespHeaderSize)
{
	if (wsHandle == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;
	return static_cast<sce::Np::WebSocket::WsConnection *>(wsHandle)->Connect(
		respHeaderBuf, respHeaderBufSize, outRespHeaderSize);
}

/* Disconnect wrapper */
int npWebSocketDisconnect(void *wsHandle)
{
	if (wsHandle == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;
	return static_cast<sce::Np::WebSocket::WsConnection *>(wsHandle)->Disconnect();
}

/* 0x813FAA1C */
int npWebSocketSendText(void *wsHandle, const char *text, SceSize length)
{
	if (wsHandle == NULL || text == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;
	return static_cast<sce::Np::WebSocket::WsConnection *>(wsHandle)->SendFrame(
		NP_WS_OPCODE_TEXT, text, length);
}

/* 0x813FAB0A */
int npWebSocketSendClose(void *wsHandle)
{
	if (wsHandle == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;
	return static_cast<sce::Np::WebSocket::WsConnection *>(wsHandle)->SendFrame(
		NP_WS_OPCODE_CLOSE, NULL, 0);
}

/* 0x813FABF8 */
int npWebSocketReceive(void *wsHandle, int *outOpcode, void *buffer,
		       SceSize bufferSize, SceSize *outReceived)
{
	if (wsHandle == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;
	return static_cast<sce::Np::WebSocket::WsConnection *>(wsHandle)->Receive(
		outOpcode, buffer, bufferSize, outReceived);
}

/* 0x813FA906 */
int npWebSocketGetResponseHeader(void *wsHandle, const char *headerName,
				 const char **outValue, SceSize *outValueLen)
{
	if (wsHandle == NULL || headerName == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;
	return static_cast<sce::Np::WebSocket::WsConnection *>(wsHandle)->GetResponseHeader(
		headerName, outValue, outValueLen);
}

/* 0x814025C4 */
int npWebSocketGetResponseStatusCode(void *wsHandle)
{
	if (wsHandle == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;
	return static_cast<sce::Np::WebSocket::WsConnection *>(wsHandle)->GetResponseStatusCode();
}
