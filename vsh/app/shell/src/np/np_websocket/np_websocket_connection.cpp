/*
 * np_websocket_connection.cpp: SceNpWebSocketConnection lifecycle, request headers,
 * GoF State/Visitor/Event pipeline ("SceNpWsStateCtx", "NpWsHdrRecv"), and public API.
 * Sony path: vsh/app/common/src/np/np_websocket/
 * 3.65-CEX: 0x813FA7F8..0x813FC857, 0x813FE2A8..0x813FFE67, 0x8140211C..0x81402D33
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

/* ============================================================================
 * Event Hierarchy (0x813FE2A8..0x813FE763)
 * ============================================================================ */

WsEventBase::WsEventBase(WsConnection *conn)
	: m_connection(conn)
{
}

WsEventBase::~WsEventBase()
{
}

/* 0x813FE2A8 */
WsSendEventBase::WsSendEventBase(WsConnection *conn)
	: WsEventBase(conn),
	  m_next(NULL),
	  m_prev(NULL),
	  m_encodedLen(0),
	  m_encodedBuf(NULL),
	  m_encodedBufCap(0)
{
}

/* 0x813FE306 / 0x813FE350 */
WsSendEventBase::~WsSendEventBase()
{
}

/* 0x813FE376 */
int WsSendEventBase::AllocEncodedBuffer(NpPushAllocator *allocator, SceSize size)
{
	if (allocator == NULL || size == 0)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;
	SceUInt8 *buf = static_cast<SceUInt8 *>(SceNpCommonPs4_AA980113(allocator, size));
	if (buf == NULL)
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	m_encodedBuf = buf;
	m_encodedBufCap = size;
	return 0;
}

/* 0x813FE3C4 */
int WsSendEventBase::FreeEncodedBuffer(NpPushAllocator *allocator)
{
	if (m_encodedBuf != NULL) {
		SceNpCommonPs4_6B428829(allocator, m_encodedBuf);
		m_encodedBuf = NULL;
		m_encodedBufCap = 0;
	}
	m_encodedLen = 0;
	return 0;
}

int WsSendEventBase::Accept(WsHandlerBase *handler)
{
	(void)handler;
	return SCE_NP_WS_ERROR_INTERNAL;
}

void WsSendEventBase::GetEncodedBuffer(SceUInt8 **outBuf, SceSize *outCap) const
{
	if (outBuf != NULL)
		*outBuf = m_encodedBuf;
	if (outCap != NULL)
		*outCap = m_encodedBufCap;
}

int WsSendEventBase::GetPayload(const void **outData, SceSize *outLen) const
{
	if (outData != NULL)
		*outData = NULL;
	if (outLen != NULL)
		*outLen = 0;
	return SCE_NP_WS_ERROR_INTERNAL;
}

WsControlEventBase::WsControlEventBase(WsConnection *conn)
	: WsSendEventBase(conn), m_payloadLen(0)
{
	sceClibMemset(m_payload, 0, sizeof(m_payload));
}

WsControlEventBase::~WsControlEventBase()
{
}

/* 0x813FE468 */
int WsControlEventBase::SetPayload(const void *data, SceSize len)
{
	if (len > NP_WS_CTRL_PAYLOAD_MAX)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;
	m_payloadLen = len;
	if (data != NULL && len > 0)
		sceClibMemcpy_safe(m_payload, data, m_payloadLen);
	return 0;
}

int WsControlEventBase::GetPayload(const void **outData, SceSize *outLen) const
{
	if (outData != NULL)
		*outData = m_payload;
	if (outLen != NULL)
		*outLen = m_payloadLen;
	return 0;
}

/* 0x813FE400 / 0x813FE430 / 0x813FE452 / 0x813FE4A8 */
WsCloseEvent::WsCloseEvent(WsConnection *conn) : WsControlEventBase(conn) {}
WsCloseEvent::~WsCloseEvent() {}
int WsCloseEvent::Accept(WsHandlerBase *handler)
{
	return handler->VisitClose(this, 0);
}

/* 0x813FE4D8 / 0x813FE508 / 0x813FE52A / 0x813FE540 */
WsPingEvent::WsPingEvent(WsConnection *conn) : WsControlEventBase(conn) {}
WsPingEvent::~WsPingEvent() {}
int WsPingEvent::Accept(WsHandlerBase *handler)
{
	return handler->VisitPing(this, 0);
}

/* 0x813FE570 / 0x813FE5A0 / 0x813FE5C2 / 0x813FE5D8 */
WsPongEvent::WsPongEvent(WsConnection *conn) : WsControlEventBase(conn) {}
WsPongEvent::~WsPongEvent() {}
int WsPongEvent::Accept(WsHandlerBase *handler)
{
	return handler->VisitPong(this, 0);
}

/* 0x813FE608 / 0x813FE636 / 0x813FE658 / 0x813FE66E / 0x813FE6A4 */
WsSendTextEvent::WsSendTextEvent(WsConnection *conn)
	: WsSendEventBase(conn), m_text(NULL), m_textLen(0)
{
}

WsSendTextEvent::~WsSendTextEvent() {}

int WsSendTextEvent::SetText(const char *text, SceSize maxLen)
{
	if (text == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;
	m_text = text;
	m_textLen = sceClibStrnlen(text, maxLen);
	return 0;
}

int WsSendTextEvent::Accept(WsHandlerBase *handler)
{
	return handler->VisitText(this, 0);
}

int WsSendTextEvent::GetPayload(const void **outData, SceSize *outLen) const
{
	if (outData != NULL)
		*outData = m_text;
	if (outLen != NULL)
		*outLen = m_textLen;
	return 0;
}

/* 0x813FE6D4 / 0x813FE6FE / 0x813FE720 / 0x813FE736 */
WsRecvEvent::WsRecvEvent(WsConnection *conn) : WsEventBase(conn) {}
WsRecvEvent::~WsRecvEvent() {}
int WsRecvEvent::Accept(WsHandlerBase *handler)
{
	return handler->VisitRecv(this, 0);
}

/* ============================================================================
 * Visitor Handler Hierarchy (0x813FE764..0x813FFE67, 0x814029B8..0x81402D33)
 * ============================================================================ */

WsHandlerBase::WsHandlerBase()
	: m_next(NULL), m_connection(NULL)
{
}

WsHandlerBase::~WsHandlerBase()
{
}

int WsHandlerBase::InitChain(WsConnection *conn)
{
	WsHandlerBase *cur = this;
	while (cur != NULL) {
		cur->m_connection = conn;
		int ret = cur->Init();
		if (ret < 0)
			return ret;
		cur = cur->m_next;
	}
	return 0;
}

void WsHandlerBase::DestroyChain(NpPushAllocator *allocator)
{
	WsHandlerBase *cur = this;
	while (cur != NULL) {
		WsHandlerBase *next = cur->m_next;
		cur->~WsHandlerBase();
		SceNpCommonPs4_6B428829(allocator, cur);
		cur = next;
	}
}

void WsHandlerBase::EnterChain(void)
{
	WsHandlerBase *cur = this;
	while (cur != NULL) {
		cur->OnEnter();
		cur = cur->m_next;
	}
}

void WsHandlerBase::ExitChain(void)
{
	WsHandlerBase *cur = this;
	while (cur != NULL) {
		cur->OnExit();
		cur = cur->m_next;
	}
}

int WsHandlerBase::VisitPing(WsPingEvent *event, int prevResult)
{
	if (m_next != NULL)
		return m_next->VisitPing(event, prevResult);
	return prevResult;
}

int WsHandlerBase::VisitPong(WsPongEvent *event, int prevResult)
{
	if (m_next != NULL)
		return m_next->VisitPong(event, prevResult);
	return prevResult;
}

int WsHandlerBase::VisitClose(WsCloseEvent *event, int prevResult)
{
	if (m_next != NULL)
		return m_next->VisitClose(event, prevResult);
	return prevResult;
}

int WsHandlerBase::VisitBinary(WsSendEventBase *event, int prevResult)
{
	if (m_next != NULL)
		return m_next->VisitBinary(event, prevResult);
	return prevResult;
}

int WsHandlerBase::VisitText(WsSendTextEvent *event, int prevResult)
{
	if (m_next != NULL)
		return m_next->VisitText(event, prevResult);
	return prevResult;
}

int WsHandlerBase::VisitRecv(WsRecvEvent *event, int prevResult)
{
	if (m_next != NULL)
		return m_next->VisitRecv(event, prevResult);
	return prevResult;
}

int WsHandlerBase::Init(void)
{
	return 0;
}

void WsHandlerBase::OnEnter(void)
{
}

void WsHandlerBase::OnExit(void)
{
}

/* --- WsResolveHandler (0x813FF46C..0x813FF59B) --- */

WsResolveHandler::WsResolveHandler() {}
WsResolveHandler::~WsResolveHandler() {}

/* 0x813FF4C6 */
int WsResolveHandler::VisitRecv(WsRecvEvent *event, int prevResult)
{
	(void)prevResult;
	WsConnection *conn = event->GetConnection();
	int ret = conn->Acquire();
	if (ret < 0)
		return ret;

	ret = conn->GetNet()->CreateResolver();
	if (ret >= 0) {
		ret = conn->GetNet()->ResolveHost(conn->GetHost(), NP_WS_RESOLVER_TIMEOUT_USEC,
						  NP_WS_RESOLVER_RETRY_COUNT);
		if (ret >= 0)
			ret = 1;
	}
	conn->GetNet()->DestroyResolver();
	conn->Release();
	return ret;
}

/* --- WsConnectHandler (0x813FE764..0x813FE8DF) --- */

WsConnectHandler::WsConnectHandler() {}
WsConnectHandler::~WsConnectHandler() {}

/* 0x813FE7BE */
int WsConnectHandler::VisitRecv(WsRecvEvent *event, int prevResult)
{
	(void)prevResult;
	WsConnection *conn = event->GetConnection();
	int ret = conn->Acquire();
	if (ret < 0)
		return ret;

	conn->GetNet()->SetNonBlocking(0);
	ret = conn->GetNet()->CreateSocket();
	if (ret >= 0) {
		const NpWebSocketOpenParam &openParam = conn->GetOpenParam();
		ret = conn->GetNet()->Connect(conn->GetHost(), openParam.port, openParam.useTls,
					      false);
	}

	if (ret < 0) {
		conn->GetNet()->Close();
		conn->Release();
		return ret;
	}

	int nextRet = (m_next != NULL) ? m_next->VisitRecv(event, ret)
				       : SCE_NP_WS_ERROR_INVALID_STATE;
	conn->Release();
	return nextRet;
}

/* --- WsHandshakeHandler (0x814029B8..0x81402D33) --- */

WsHandshakeHandler::WsHandshakeHandler() {}
WsHandshakeHandler::~WsHandshakeHandler() {}

/* 0x81402ADE */
int WsHandshakeHandler::VisitRecv(WsRecvEvent *event, int prevResult)
{
	(void)prevResult;
	WsConnection *conn = event->GetConnection();
	int ret = conn->Acquire();
	if (ret < 0)
		return ret;

	char reqBuf[NP_WS_HTTP_BUF_SIZE];
	sceClibMemset(reqBuf, 0, sizeof(reqBuf));

	WsHandshake *hs = conn->GetHandshake();
	hs->GenerateKey();
	const char *origin = (conn->GetOrigin()[0] != '\0') ? conn->GetOrigin() : NULL;
	const char *subProto = (conn->GetSubProtocol()[0] != '\0') ? conn->GetSubProtocol() : NULL;

	ret = hs->BuildUpgradeRequest(conn->GetHost(), conn->GetPath(), origin, subProto, reqBuf,
				      sizeof(reqBuf));
	for (int idx = 0; ret >= 0 && idx < conn->GetHeaderCount(); ++idx) {
		ret = hs->AppendCustomHeader(conn->GetHeaderName(idx), conn->GetHeaderValue(idx),
					     reqBuf, sizeof(reqBuf));
	}
	if (ret >= 0)
		ret = hs->TerminateRequest(reqBuf, sizeof(reqBuf));
	if (ret >= 0) {
		SceSize reqLen = sceClibStrnlen(reqBuf, sizeof(reqBuf));
		ret = conn->GetNet()->Send(reqBuf, reqLen);
	}

	SceSize totalReceived = 0;
	SceSize headerEnd = 0;
	if (ret >= 0)
		ret = hs->ReceiveResponse(conn->GetNet(), &totalReceived, &headerEnd);

	if (ret >= 0 && conn->GetHandshakeRespBuf() != NULL && conn->GetHandshakeRespCap() > 0) {
		SceSize copyLen = (headerEnd < conn->GetHandshakeRespCap() - 1U)
					  ? headerEnd
					  : (conn->GetHandshakeRespCap() - 1U);
		sceClibMemcpy_safe(conn->GetHandshakeRespBuf(), hs->GetResponseBuffer(), copyLen);
		conn->GetHandshakeRespBuf()[copyLen] = '\0';
		if (conn->GetHandshakeRespOutLen() != NULL)
			*conn->GetHandshakeRespOutLen() = copyLen;
	}

	if (ret >= 0)
		ret = hs->VerifyResponse();

	if (ret >= 0 && headerEnd < totalReceived) {
		conn->GetStream()->AppendData(hs->GetResponseBuffer() + headerEnd,
					      totalReceived - headerEnd);
	}

	conn->Release();
	return (ret >= 0) ? 0 : ret;
}

/* --- WsRecvHandler ("NpWsHdrRecv", 0x813FEED8..0x813FF46B) --- */

WsRecvHandler::WsRecvHandler()
{
	sceClibMemset(&m_epollEvent, 0, sizeof(m_epollEvent));
}

WsRecvHandler::~WsRecvHandler() {}

/* 0x813FF2B6 */
void WsRecvHandler::OnEnter(void)
{
	if (m_connection == NULL)
		return;
	if (m_connection->Acquire() < 0)
		return;

	if (m_connection->RecvEpollId() <= 0) {
		m_connection->GetNet()->SetNonBlocking(1);
		SceNetId epollId = m_connection->GetNet()->CreateEpoll("SceNpWsReceive");
		if (epollId >= 0) {
			SceNetEpollEvent ev;
			sceClibMemset(&ev, 0, sizeof(ev));
			ev.events = NP_WS_EPOLL_IN_EVENTS;
			int ret = m_connection->GetNet()->ControlSocketEpoll(epollId, &ev, 1);
			if (ret < 0)
				m_connection->GetNet()->DestroyEpoll(epollId);
			else
				m_connection->RecvEpollId() = epollId;
		}
	}
	m_connection->Release();
}

/* 0x813FF3E0 */
void WsRecvHandler::OnExit(void)
{
	if (m_connection == NULL)
		return;
	if (m_connection->Acquire() < 0)
		return;

	if (m_connection->RecvEpollId() > 0) {
		m_connection->GetNet()->DestroyEpoll(m_connection->RecvEpollId());
		m_connection->RecvEpollId() = 0;
	}
	m_connection->Release();
}

/* 0x813FEF32 */
int WsRecvHandler::VisitRecv(WsRecvEvent *event, int prevResult)
{
	(void)prevResult;
	WsConnection *conn = event->GetConnection();
	int ret = conn->Acquire();
	if (ret < 0)
		return ret;

	conn->NetRecvBytes() = 0;
	SceUInt8 *netBuf = conn->GetNetRecvBuf();
	SceSize netBufSize = conn->GetNetRecvBufSize();
	if (netBuf == NULL || netBufSize == 0) {
		conn->Release();
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	}

	bool skipSocketRead = (conn->GetStream() != NULL && conn->GetStream()->HasUnreadData());
	if (!skipSocketRead) {
		int waitTimeout = -1;
		int recvTimeout = static_cast<int>(conn->GetOpenParam().recvTimeout);
		if (recvTimeout > 0) {
			if (conn->LastPingClockUsec() == 0) {
				int clkRet = SceNpCommonPs4_F56B0EAB(&conn->LastPingClockUsec());
				if (clkRet < 0) {
					sceClibPrintf("[%s\t] %s() ", "NpWsHdrRecv", "Visit");
					sceClibPrintf(
						"sceNpGetSystemClockUsec failed. ret = 0x%08x\n",
						clkRet);
				}
			}
			SceInt64 nowUsec = 0;
			int clkRet = SceNpCommonPs4_F56B0EAB(&nowUsec);
			if (clkRet < 0) {
				sceClibPrintf("[%s\t] %s() ", "NpWsHdrRecv", "Visit");
				sceClibPrintf("sceNpGetSystemClockUsec failed. ret = 0x%08x\n",
					      clkRet);
			}
			SceInt64 elapsed = nowUsec - conn->LastPingClockUsec();
			waitTimeout = (elapsed > 0 && elapsed <= recvTimeout)
					      ? static_cast<int>(recvTimeout - elapsed)
					      : 1;
		}

		bool hasSslPending = false;
		ret = conn->GetNet()->HasPendingData(&hasSslPending);
		if (ret < 0) {
			conn->Release();
			return ret;
		}

		bool doSocketRecv = true;
		if (!hasSslPending) {
			int waitRet = conn->GetNet()->WaitEpoll(conn->RecvEpollId(), &m_epollEvent,
								1, waitTimeout);
			if (waitRet < 0) {
				conn->Release();
				return waitRet;
			}
			if (waitRet != 1) {
				if (waitRet == 0 && waitTimeout != -1) {
					const char pingByte = 'a';
					ret = conn->GetStateContext()->EnqueuePing(&pingByte, 1);
					if (ret < 0) {
						conn->Release();
						return ret;
					}
					int clkRet =
						SceNpCommonPs4_F56B0EAB(&conn->LastPingClockUsec());
					if (clkRet < 0) {
						sceClibPrintf("[%s\t] %s() ", "NpWsHdrRecv",
							      "Visit");
						sceClibPrintf(
							"sceNpGetSystemClockUsec failed. ret = 0x%08x\n",
							clkRet);
					}
					doSocketRecv = false;
				} else {
					conn->Release();
					return SCE_NP_WS_ERROR_INTERNAL;
				}
			} else if ((m_epollEvent.events & NP_WS_EPOLL_ERROR_MASK) != 0) {
				int sockErr = conn->GetNet()->GetSocketError();
				conn->Release();
				return (sockErr < 0) ? sockErr : SCE_NP_WS_ERROR_DISCONNECTED;
			} else if ((m_epollEvent.events & SCE_NET_EPOLLIN) == 0) {
				conn->Release();
				return SCE_NP_WS_ERROR_INTERNAL;
			}
		}

		if (doSocketRecv) {
			int recvBytes = conn->GetNet()->Recv(netBuf, netBufSize);
			if (recvBytes < 0) {
				conn->Release();
				return recvBytes;
			}
			conn->NetRecvBytes() = static_cast<SceSize>(recvBytes);
			if (recvBytes == 0) {
				int sockErr = conn->GetNet()->GetSocketError();
				conn->Release();
				return (sockErr < 0) ? sockErr : SCE_NP_WS_ERROR_DISCONNECTED;
			}
		}
	}

	ret = (m_next != NULL) ? m_next->VisitRecv(event, ret) : SCE_NP_WS_ERROR_INVALID_STATE;
	conn->Release();
	return ret;
}

/* --- WsStreamHandler (0x813FEC9C..0x813FEED7) --- */

WsStreamHandler::WsStreamHandler() {}
WsStreamHandler::~WsStreamHandler() {}

/* 0x813FECF6 */
int WsStreamHandler::VisitRecv(WsRecvEvent *event, int prevResult)
{
	(void)prevResult;
	WsConnection *conn = event->GetConnection();
	int ret = conn->Acquire();
	if (ret < 0)
		return ret;

	bool hasUnread = conn->GetStream()->HasUnreadData();
	if (!hasUnread && conn->NetRecvBytes() == 0) {
		ret = 1;
	} else {
		if (!hasUnread) {
			ret = conn->GetStream()->AppendData(conn->GetNetRecvBuf(),
							    conn->NetRecvBytes());
		}
		if (ret >= 0) {
			int isControl = 0;
			WsControlFrame ctrlFrame;
			sceClibMemset(&ctrlFrame, 0, sizeof(ctrlFrame));
			ret = conn->GetStream()->GetMessage(
				conn->GetRecvOutCap(),
				static_cast<char *>(conn->GetRecvOutBuf()),
				conn->GetRecvOutMsgType(), conn->GetRecvOutLen(), &isControl,
				&ctrlFrame);
			if (ret >= 0 && ret != 1) {
				if (isControl != 0) {
					if (ctrlFrame.opcode == NP_WS_OPCODE_CLOSE) {
						if (conn->GetRecvOutCap() < ctrlFrame.payloadLen) {
							conn->Release();
							return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
						}
						if (ctrlFrame.payloadLen > 0) {
							sceClibMemcpy_safe(conn->GetRecvOutBuf(),
									   ctrlFrame.body,
									   ctrlFrame.payloadLen);
						}
						*conn->GetRecvOutMsgType() = NP_WS_MSG_TYPE_CLOSE;
						*conn->GetRecvOutLen() = ctrlFrame.payloadLen;
						conn->RecvDone() = 1;
						ret = 0;
					} else if (ctrlFrame.opcode == NP_WS_OPCODE_PING) {
						ret = conn->GetStateContext()->EnqueuePong(
							ctrlFrame.body, ctrlFrame.payloadLen);
						if (ret >= 0)
							ret = 1;
					} else if (ctrlFrame.opcode == NP_WS_OPCODE_PONG) {
						ret = 1;
					} else {
						conn->Release();
						return SCE_NP_WS_ERROR_PROTOCOL;
					}
				} else {
					conn->RecvDone() = 1;
					ret = 0;
				}
			}
		}
	}

	if (ret >= 0) {
		ret = (m_next != NULL) ? m_next->VisitRecv(event, ret)
				       : SCE_NP_WS_ERROR_INVALID_STATE;
		if (conn->RecvDone() != 0)
			ret = 0;
	}

	conn->Release();
	return ret;
}

/* --- WsCtrlFrameHandler (0x813FE8E0..0x813FEC9B) --- */

WsCtrlFrameHandler::WsCtrlFrameHandler() {}
WsCtrlFrameHandler::~WsCtrlFrameHandler() {}

/* 0x813FEBA0 */
int WsCtrlFrameHandler::BuildControlFrame(WsSendEventBase *event, int prevResult,
					  WsConnection *conn, int opcode)
{
	(void)prevResult;
	const void *payload = NULL;
	SceSize payloadLen = 0;
	int ret = event->GetPayload(&payload, &payloadLen);
	if (ret < 0)
		return ret;

	SceSize encodedSize = m_frameBuilder.CalcEncodedSize(payloadLen);
	ret = event->AllocEncodedBuffer(conn->GetAllocator(), encodedSize);
	if (ret < 0)
		return ret;

	SceUInt8 *encodedBuf = NULL;
	SceSize encodedCap = 0;
	event->GetEncodedBuffer(&encodedBuf, &encodedCap);
	if (encodedBuf == NULL)
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;

	SceSize frameLen = 0;
	ret = m_frameBuilder.BuildClientFrame(encodedBuf, encodedCap, opcode, payload, payloadLen,
					      &frameLen);
	if (ret >= 0)
		event->SetEncodedLength(frameLen);
	return ret;
}

/* 0x813FE94E */
int WsCtrlFrameHandler::VisitPing(WsPingEvent *event, int prevResult)
{
	WsConnection *conn = event->GetConnection();
	int ret = conn->Acquire();
	if (ret >= 0) {
		ret = BuildControlFrame(event, prevResult, conn, NP_WS_OPCODE_PING);
		if (ret >= 0) {
			if (m_next != NULL)
				ret = m_next->VisitPing(event, ret);
			if (ret >= 0)
				ret = 0;
		}
		event->FreeEncodedBuffer(conn->GetAllocator());
		conn->Release();
	}
	return ret;
}

/* 0x813FEA02 */
int WsCtrlFrameHandler::VisitPong(WsPongEvent *event, int prevResult)
{
	WsConnection *conn = event->GetConnection();
	int ret = conn->Acquire();
	if (ret >= 0) {
		ret = BuildControlFrame(event, prevResult, conn, NP_WS_OPCODE_PONG);
		if (ret >= 0) {
			if (m_next != NULL)
				ret = m_next->VisitPong(event, ret);
			if (ret >= 0)
				ret = 0;
		}
		event->FreeEncodedBuffer(conn->GetAllocator());
		conn->Release();
	}
	return ret;
}

/* 0x813FEAB6 */
int WsCtrlFrameHandler::VisitClose(WsCloseEvent *event, int prevResult)
{
	WsConnection *conn = event->GetConnection();
	int ret = conn->Acquire();
	if (ret >= 0) {
		ret = BuildControlFrame(event, prevResult, conn, NP_WS_OPCODE_CLOSE);
		if (ret >= 0) {
			if (m_next != NULL)
				ret = m_next->VisitClose(event, ret);
			if (ret >= 0)
				ret = 0;
		}
		event->FreeEncodedBuffer(conn->GetAllocator());
		conn->Release();
	}
	return ret;
}

/* 0x813FEB6A */
int WsCtrlFrameHandler::VisitRecv(WsRecvEvent *event, int prevResult)
{
	if (m_next != NULL)
		return m_next->VisitRecv(event, prevResult);
	return prevResult;
}

/* --- WsSendHandler (0x813FF59C..0x813FFE67) --- */

WsSendHandler::WsSendHandler()
{
	sceClibMemset(&m_epollEvent, 0, sizeof(m_epollEvent));
	SceNpCommonPs4_4266D870(&m_sendLock);
}

/* 0x813FF5D2 / 0x813FF620 */
WsSendHandler::~WsSendHandler()
{
	if (SceNpCommonPs4_C78F5DF5(&m_sendLock) != 0)
		SceNpCommonPs4_63EED478(&m_sendLock);
	SceNpCommonPs4_294F0B1A(&m_sendLock);
}

/* 0x813FF9A8 */
int WsSendHandler::Init(void)
{
	int ret = SceNpCommonPs4_8AA7E995(&m_sendLock, "SceNpWsSend", 0);
	if (ret < 0 && SceNpCommonPs4_C78F5DF5(&m_sendLock) != 0)
		SceNpCommonPs4_63EED478(&m_sendLock);
	return ret;
}

/* 0x813FF9F6 */
void WsSendHandler::OnEnter(void)
{
	if (m_connection == NULL)
		return;
	if (m_connection->Acquire() < 0)
		return;

	{
		Push::ScopedLock lock(&m_sendLock);
		if (m_connection->SendEpollId() <= 0) {
			m_connection->GetNet()->SetNonBlocking(1);
			SceNetId epollId = m_connection->GetNet()->CreateEpoll("SceNpWsSend");
			if (epollId >= 0) {
				SceNetEpollEvent ev;
				sceClibMemset(&ev, 0, sizeof(ev));
				ev.events = NP_WS_EPOLL_OUT_EVENTS;
				int ret = m_connection->GetNet()->ControlSocketEpoll(epollId, &ev,
										     1);
				if (ret < 0)
					m_connection->GetNet()->DestroyEpoll(epollId);
				else
					m_connection->SendEpollId() = epollId;
			}
		}
	}
	m_connection->Release();
}

/* 0x813FFB3E */
void WsSendHandler::OnExit(void)
{
	if (m_connection == NULL)
		return;
	if (m_connection->Acquire() < 0)
		return;

	{
		Push::ScopedLock lock(&m_sendLock);
		if (m_connection->SendEpollId() > 0) {
			m_connection->GetNet()->DestroyEpoll(m_connection->SendEpollId());
			m_connection->SendEpollId() = 0;
		}
	}
	m_connection->Release();
}

/* 0x813FFBDC */
int WsSendHandler::SendEncodedEvent(WsSendEventBase *event, int prevResult, WsConnection *conn)
{
	(void)prevResult;
	SceUInt8 *encodedBuf = NULL;
	SceSize encodedCap = 0;
	event->GetEncodedBuffer(&encodedBuf, &encodedCap);
	SceSize totalLen = event->GetEncodedLength();
	if (encodedBuf == NULL || totalLen == 0)
		return SCE_NP_WS_ERROR_INTERNAL;

	int timeoutUsec = (conn->GetOpenParam().connectTimeout != 0)
				  ? static_cast<int>(conn->GetOpenParam().connectTimeout)
				  : -1;

	Push::ScopedLock lock(&m_sendLock);
	SceSize sentTotal = 0;
	while (sentTotal < totalLen) {
		sceClibMemset(&m_epollEvent, 0, sizeof(m_epollEvent));
		int waitRet = conn->GetNet()->WaitEpoll(conn->SendEpollId(), &m_epollEvent, 1,
							timeoutUsec);
		if (waitRet < 0)
			return waitRet;
		if (waitRet == 0 && timeoutUsec != -1)
			return SCE_NP_WS_ERROR_TIMEOUT;
		if (waitRet != 1)
			return SCE_NP_WS_ERROR_INTERNAL;

		if ((m_epollEvent.events & NP_WS_EPOLL_ERROR_MASK) != 0) {
			int sockErr = conn->GetNet()->GetSocketError();
			return (sockErr < 0) ? sockErr : SCE_NP_WS_ERROR_DISCONNECTED;
		}
		if ((m_epollEvent.events & SCE_NET_EPOLLOUT) == 0)
			return SCE_NP_WS_ERROR_INTERNAL;

		int sentNow = conn->GetNet()->Send(encodedBuf + sentTotal, totalLen - sentTotal);
		if (sentNow < 0)
			return sentNow;
		if (sentNow == 0)
			return SCE_NP_WS_ERROR_DISCONNECTED;
		sentTotal += static_cast<SceSize>(sentNow);
	}
	return 0;
}

/* 0x813FFD6A */
int WsSendHandler::BuildDataFrame(WsSendEventBase *event, int prevResult, WsConnection *conn,
				  int opcode)
{
	(void)prevResult;
	const void *payload = NULL;
	SceSize payloadLen = 0;
	int ret = event->GetPayload(&payload, &payloadLen);
	if (ret < 0)
		return ret;

	SceSize encodedSize = m_frameBuilder.CalcEncodedSize(payloadLen);
	ret = event->AllocEncodedBuffer(conn->GetAllocator(), encodedSize);
	if (ret < 0)
		return ret;

	SceUInt8 *encodedBuf = NULL;
	SceSize encodedCap = 0;
	event->GetEncodedBuffer(&encodedBuf, &encodedCap);
	if (encodedBuf == NULL)
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;

	SceSize frameLen = 0;
	ret = m_frameBuilder.BuildClientFrame(encodedBuf, encodedCap, opcode, payload, payloadLen,
					      &frameLen);
	if (ret >= 0)
		event->SetEncodedLength(frameLen);
	return ret;
}

/* 0x813FF636 */
int WsSendHandler::VisitPing(WsPingEvent *event, int prevResult)
{
	WsConnection *conn = event->GetConnection();
	int ret = conn->Acquire();
	if (ret >= 0) {
		ret = SendEncodedEvent(event, prevResult, conn);
		if (ret >= 0 && m_next != NULL)
			ret = m_next->VisitPing(event, ret);
		conn->Release();
	}
	return ret;
}

/* 0x813FF6D2 */
int WsSendHandler::VisitPong(WsPongEvent *event, int prevResult)
{
	WsConnection *conn = event->GetConnection();
	int ret = conn->Acquire();
	if (ret >= 0) {
		ret = SendEncodedEvent(event, prevResult, conn);
		if (ret >= 0 && m_next != NULL)
			ret = m_next->VisitPong(event, ret);
		conn->Release();
	}
	return ret;
}

/* 0x813FF76E */
int WsSendHandler::VisitClose(WsCloseEvent *event, int prevResult)
{
	WsConnection *conn = event->GetConnection();
	int ret = conn->Acquire();
	if (ret >= 0) {
		ret = SendEncodedEvent(event, prevResult, conn);
		if (ret >= 0 && m_next != NULL)
			ret = m_next->VisitClose(event, ret);
		conn->Release();
	}
	return ret;
}

/* 0x813FF80A */
int WsSendHandler::VisitBinary(WsSendEventBase *event, int prevResult)
{
	WsConnection *conn = event->GetConnection();
	int ret = conn->Acquire();
	if (ret >= 0) {
		ret = BuildDataFrame(event, prevResult, conn, NP_WS_OPCODE_BINARY);
		if (ret >= 0) {
			ret = SendEncodedEvent(event, ret, conn);
			if (ret >= 0 && m_next != NULL)
				ret = m_next->VisitBinary(event, ret);
		}
		event->FreeEncodedBuffer(conn->GetAllocator());
		conn->Release();
	}
	return ret;
}

/* 0x813FF8D8 */
int WsSendHandler::VisitText(WsSendTextEvent *event, int prevResult)
{
	WsConnection *conn = event->GetConnection();
	int ret = conn->Acquire();
	if (ret >= 0) {
		ret = BuildDataFrame(event, prevResult, conn, NP_WS_OPCODE_TEXT);
		if (ret >= 0) {
			ret = SendEncodedEvent(event, ret, conn);
			if (ret >= 0 && m_next != NULL)
				ret = m_next->VisitText(event, ret);
		}
		event->FreeEncodedBuffer(conn->GetAllocator());
		conn->Release();
	}
	return ret;
}

int WsSendHandler::VisitRecv(WsRecvEvent *event, int prevResult)
{
	if (m_next != NULL)
		return m_next->VisitRecv(event, prevResult);
	return prevResult;
}

/* ============================================================================
 * State Hierarchy (0x813FC4C8..0x813FC857, 0x8140211C..0x814026D7)
 * ============================================================================ */

WsStateBase::WsStateBase()
	: m_stateId(0), m_handlerChain(NULL), m_allocator(NULL)
{
}

WsStateBase::~WsStateBase()
{
	CleanupChain();
}

/* 0x81402188 */
void WsStateBase::CleanupChain(void)
{
	if (m_handlerChain != NULL) {
		m_handlerChain->DestroyChain(m_allocator);
		m_handlerChain = NULL;
	}
}

/* 0x8140211C */
int WsStateBase::Finalize(void)
{
	CleanupChain();
	return 0;
}

/* 0x81402134 */
int WsStateBase::OnEnter(void)
{
	if (m_handlerChain == NULL)
		return SCE_NP_WS_ERROR_INTERNAL;
	m_handlerChain->EnterChain();
	return 0;
}

/* 0x8140215E */
int WsStateBase::OnExit(void)
{
	if (m_handlerChain == NULL)
		return SCE_NP_WS_ERROR_INTERNAL;
	m_handlerChain->ExitChain();
	return 0;
}

int WsStateBase::GetResponseStatusCode(WsConnection *conn)
{
	(void)conn;
	return SCE_NP_WS_ERROR_INVALID_STATE;
}

int WsStateBase::GetResponseHeader(WsConnection *conn, const char *name, const char **outValue,
				   SceSize *outValueLen)
{
	(void)conn;
	(void)name;
	(void)outValue;
	(void)outValueLen;
	return SCE_NP_WS_ERROR_INVALID_STATE;
}

int WsStateBase::PrepareConnect(WsStateContext *ctx)
{
	(void)ctx;
	return SCE_NP_WS_ERROR_INVALID_STATE;
}

int WsStateBase::StartConnect(WsStateContext *ctx)
{
	(void)ctx;
	return SCE_NP_WS_ERROR_INVALID_STATE;
}

int WsStateBase::CompleteHandshake(WsStateContext *ctx)
{
	(void)ctx;
	return SCE_NP_WS_ERROR_INVALID_STATE;
}

int WsStateBase::SendBinary(WsStateContext *ctx, WsSendEventBase *event)
{
	(void)ctx;
	(void)event;
	return SCE_NP_WS_ERROR_INVALID_STATE;
}

int WsStateBase::SendText(WsStateContext *ctx, WsSendEventBase *event)
{
	(void)ctx;
	(void)event;
	return SCE_NP_WS_ERROR_INVALID_STATE;
}

int WsStateBase::BeginClose(WsStateContext *ctx)
{
	(void)ctx;
	return SCE_NP_WS_ERROR_INVALID_STATE;
}

int WsStateBase::SendClose(WsStateContext *ctx, WsSendEventBase *event)
{
	(void)ctx;
	(void)event;
	return SCE_NP_WS_ERROR_INVALID_STATE;
}

int WsStateBase::Receive(WsStateContext *ctx)
{
	(void)ctx;
	return SCE_NP_WS_ERROR_INVALID_STATE;
}

/* --- State 0: WsInitState (0x81402218..0x8140229F) --- */

WsInitState::WsInitState() {}
WsInitState::~WsInitState() {}

int WsInitState::Init(WsConnection *conn)
{
	void *resMem = SceNpCommonPs4_0C922423(sizeof(WsResolveHandler), m_allocator);
	WsResolveHandler *resolve = (resMem != NULL) ? new (resMem) WsResolveHandler() : NULL;
	m_handlerChain = resolve;
	if (resolve == NULL) {
		Finalize();
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	}

	void *connMem = SceNpCommonPs4_0C922423(sizeof(WsConnectHandler), m_allocator);
	WsConnectHandler *connect = (connMem != NULL) ? new (connMem) WsConnectHandler() : NULL;
	if (connect == NULL) {
		Finalize();
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	}

	resolve->SetNext(connect);
	int ret = m_handlerChain->InitChain(conn);
	if (ret < 0)
		Finalize();
	return ret;
}

/* 0x81402276 */
int WsInitState::PrepareConnect(WsStateContext *ctx)
{
	return ctx->SetState(WS_STATE_CONNECTING);
}

/* --- State 1: WsConnectingState (0x813FC6F4..0x813FC857) --- */

WsConnectingState::WsConnectingState() {}
WsConnectingState::~WsConnectingState() {}

/* 0x813FC752 */
int WsConnectingState::Init(WsConnection *conn)
{
	void *resMem = SceNpCommonPs4_0C922423(sizeof(WsResolveHandler), m_allocator);
	WsResolveHandler *resolve = (resMem != NULL) ? new (resMem) WsResolveHandler() : NULL;
	m_handlerChain = resolve;
	if (resolve == NULL) {
		Finalize();
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	}

	void *connMem = SceNpCommonPs4_0C922423(sizeof(WsConnectHandler), m_allocator);
	WsConnectHandler *connect = (connMem != NULL) ? new (connMem) WsConnectHandler() : NULL;
	if (connect == NULL) {
		Finalize();
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	}

	resolve->SetNext(connect);
	int ret = m_handlerChain->InitChain(conn);
	if (ret < 0)
		Finalize();
	return ret;
}

/* 0x813FC812 */
int WsConnectingState::PrepareConnect(WsStateContext *ctx)
{
	(void)ctx;
	return 0;
}

/* 0x813FC824 */
int WsConnectingState::StartConnect(WsStateContext *ctx)
{
	int ret = ctx->DispatchRecv(m_handlerChain);
	if (ret >= 0)
		ctx->SetState(WS_STATE_CONNECTED);
	return ret;
}

/* --- State 2: WsConnectedState (0x81402474..0x814026D7) --- */

WsConnectedState::WsConnectedState() {}
WsConnectedState::~WsConnectedState() {}

/* 0x814024D2 */
int WsConnectedState::Init(WsConnection *conn)
{
	void *connMem = SceNpCommonPs4_0C922423(sizeof(WsConnectHandler), m_allocator);
	WsConnectHandler *connect = (connMem != NULL) ? new (connMem) WsConnectHandler() : NULL;
	m_handlerChain = connect;
	if (connect == NULL) {
		Finalize();
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	}

	void *hsMem = SceNpCommonPs4_0C922423(sizeof(WsHandshakeHandler), m_allocator);
	WsHandshakeHandler *hs = (hsMem != NULL) ? new (hsMem) WsHandshakeHandler() : NULL;
	if (hs == NULL) {
		Finalize();
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	}

	connect->SetNext(hs);
	int ret = m_handlerChain->InitChain(conn);
	if (ret < 0)
		Finalize();
	return ret;
}

/* 0x814025C4 */
int WsConnectedState::GetResponseStatusCode(WsConnection *conn)
{
	if (conn == NULL || conn->GetHandshake() == NULL)
		return SCE_NP_WS_ERROR_INTERNAL;
	return conn->GetHandshake()->ParseStatusCode();
}

/* 0x81402644 */
int WsConnectedState::GetResponseHeader(WsConnection *conn, const char *name,
					const char **outValue, SceSize *outValueLen)
{
	if (conn == NULL || conn->GetHandshake() == NULL)
		return SCE_NP_WS_ERROR_INTERNAL;
	return conn->GetHandshake()->FindResponseHeader(name, outValue, outValueLen);
}

/* 0x81402592 */
int WsConnectedState::CompleteHandshake(WsStateContext *ctx)
{
	int ret = ctx->DispatchRecv(m_handlerChain);
	if (ret >= 0)
		ctx->SetState(WS_STATE_OPEN);
	return ret;
}

/* --- State 3: WsOpenState (0x813FC4C8..0x813FC6F3) --- */

WsOpenState::WsOpenState() {}
WsOpenState::~WsOpenState() {}

/* 0x813FC526 */
int WsOpenState::Init(WsConnection *conn)
{
	void *recvMem = SceNpCommonPs4_0C922423(sizeof(WsRecvHandler), m_allocator);
	WsRecvHandler *recvHdr = (recvMem != NULL) ? new (recvMem) WsRecvHandler() : NULL;
	m_handlerChain = recvHdr;
	if (recvHdr == NULL) {
		Finalize();
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	}

	void *streamMem = SceNpCommonPs4_0C922423(sizeof(WsStreamHandler), m_allocator);
	WsStreamHandler *streamHdr = (streamMem != NULL) ? new (streamMem) WsStreamHandler() : NULL;
	if (streamHdr == NULL) {
		Finalize();
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	}
	recvHdr->SetNext(streamHdr);

	void *ctrlMem = SceNpCommonPs4_0C922423(sizeof(WsCtrlFrameHandler), m_allocator);
	WsCtrlFrameHandler *ctrlHdr =
		(ctrlMem != NULL) ? new (ctrlMem) WsCtrlFrameHandler() : NULL;
	if (ctrlHdr == NULL) {
		Finalize();
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	}
	streamHdr->SetNext(ctrlHdr);

	void *sendMem = SceNpCommonPs4_0C922423(sizeof(WsSendHandler), m_allocator);
	WsSendHandler *sendHdr = (sendMem != NULL) ? new (sendMem) WsSendHandler() : NULL;
	if (sendHdr == NULL) {
		Finalize();
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	}
	ctrlHdr->SetNext(sendHdr);

	int ret = m_handlerChain->InitChain(conn);
	if (ret < 0)
		Finalize();
	return ret;
}

int WsOpenState::GetResponseStatusCode(WsConnection *conn)
{
	if (conn == NULL || conn->GetHandshake() == NULL)
		return SCE_NP_WS_ERROR_INTERNAL;
	return conn->GetHandshake()->ParseStatusCode();
}

int WsOpenState::GetResponseHeader(WsConnection *conn, const char *name, const char **outValue,
				   SceSize *outValueLen)
{
	if (conn == NULL || conn->GetHandshake() == NULL)
		return SCE_NP_WS_ERROR_INTERNAL;
	return conn->GetHandshake()->FindResponseHeader(name, outValue, outValueLen);
}

/* 0x813FC668 */
int WsOpenState::SendBinary(WsStateContext *ctx, WsSendEventBase *event)
{
	return ctx->DispatchSend(m_handlerChain, event);
}

int WsOpenState::SendText(WsStateContext *ctx, WsSendEventBase *event)
{
	return ctx->DispatchSend(m_handlerChain, event);
}

/* 0x813FC68E */
int WsOpenState::BeginClose(WsStateContext *ctx)
{
	return ctx->SetState(WS_STATE_CLOSING);
}

/* 0x813FC6B8 */
int WsOpenState::Receive(WsStateContext *ctx)
{
	int ret;
	do {
		ret = ctx->DispatchRecv(m_handlerChain);
	} while (ret == 1);
	return ret;
}

/* --- State 4: WsClosingState (0x814022A0..0x81402473) --- */

WsClosingState::WsClosingState() {}
WsClosingState::~WsClosingState() {}

/* 0x814022FE */
int WsClosingState::Init(WsConnection *conn)
{
	void *ctrlMem = SceNpCommonPs4_0C922423(sizeof(WsCtrlFrameHandler), m_allocator);
	WsCtrlFrameHandler *ctrlHdr =
		(ctrlMem != NULL) ? new (ctrlMem) WsCtrlFrameHandler() : NULL;
	m_handlerChain = ctrlHdr;
	if (ctrlHdr == NULL) {
		Finalize();
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	}

	void *sendMem = SceNpCommonPs4_0C922423(sizeof(WsSendHandler), m_allocator);
	WsSendHandler *sendHdr = (sendMem != NULL) ? new (sendMem) WsSendHandler() : NULL;
	if (sendHdr == NULL) {
		Finalize();
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	}
	ctrlHdr->SetNext(sendHdr);

	void *recvMem = SceNpCommonPs4_0C922423(sizeof(WsRecvHandler), m_allocator);
	WsRecvHandler *recvHdr = (recvMem != NULL) ? new (recvMem) WsRecvHandler() : NULL;
	if (recvHdr == NULL) {
		Finalize();
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	}
	sendHdr->SetNext(recvHdr);

	void *streamMem = SceNpCommonPs4_0C922423(sizeof(WsStreamHandler), m_allocator);
	WsStreamHandler *streamHdr = (streamMem != NULL) ? new (streamMem) WsStreamHandler() : NULL;
	if (streamHdr == NULL) {
		Finalize();
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	}
	recvHdr->SetNext(streamHdr);

	int ret = m_handlerChain->InitChain(conn);
	if (ret < 0)
		Finalize();
	return ret;
}

/* 0x8140243E */
int WsClosingState::SendClose(WsStateContext *ctx, WsSendEventBase *event)
{
	int ret = ctx->DispatchSend(m_handlerChain, event);
	if (ret >= 0)
		ctx->SetState(WS_STATE_OPEN);
	return ret;
}

/* ============================================================================
 * WsStateContext ("SceNpWsStateCtx", 0x813FBA04..0x813FC4C7)
 * ============================================================================ */

/* 0x813FBA04 */
WsStateContext::WsStateContext(NpPushAllocator *allocator)
	: m_allocator(allocator),
	  m_currentState(WS_STATE_COUNT + 1),
	  m_connection(NULL),
	  m_recvEvent(NULL),
	  m_controlHead(NULL),
	  m_controlTail(NULL)
{
	SceNpCommonPs4_4266D870(&m_mutex);
	for (int idx = 0; idx < WS_STATE_COUNT; ++idx)
		m_states[idx] = NULL;
}

/* 0x813FBA90 / 0x813FBAD2 */
WsStateContext::~WsStateContext()
{
	Finalize();
	SceNpCommonPs4_294F0B1A(&m_mutex);
}

/* 0x813FBAE8 */
int WsStateContext::Init(WsConnection *conn)
{
	if (m_allocator == NULL)
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;

	m_connection = conn;
	int ret = SceNpCommonPs4_8AA7E995(&m_mutex, "SceNpWsStateCtx", 0);
	if (ret >= 0) {
		void *memInit = SceNpCommonPs4_0C922423(sizeof(WsInitState), m_allocator);
		m_states[WS_STATE_INIT] = (memInit != NULL) ? new (memInit) WsInitState() : NULL;

		void *memConn = SceNpCommonPs4_0C922423(sizeof(WsConnectingState), m_allocator);
		m_states[WS_STATE_CONNECTING] =
			(memConn != NULL) ? new (memConn) WsConnectingState() : NULL;

		void *memHs = SceNpCommonPs4_0C922423(sizeof(WsConnectedState), m_allocator);
		m_states[WS_STATE_CONNECTED] =
			(memHs != NULL) ? new (memHs) WsConnectedState() : NULL;

		void *memOpen = SceNpCommonPs4_0C922423(sizeof(WsOpenState), m_allocator);
		m_states[WS_STATE_OPEN] = (memOpen != NULL) ? new (memOpen) WsOpenState() : NULL;

		void *memClose = SceNpCommonPs4_0C922423(sizeof(WsClosingState), m_allocator);
		m_states[WS_STATE_CLOSING] =
			(memClose != NULL) ? new (memClose) WsClosingState() : NULL;

		for (int idx = 0; idx < WS_STATE_COUNT; ++idx) {
			if (m_states[idx] == NULL) {
				ret = SCE_NP_WS_ERROR_OUT_OF_MEMORY;
				break;
			}
			m_states[idx]->SetAllocator(m_allocator);
			ret = m_states[idx]->Init(conn);
			if (ret < 0)
				break;
		}
	}

	if (ret >= 0) {
		void *recvEvMem = SceNpCommonPs4_0C922423(sizeof(WsRecvEvent), m_allocator);
		m_recvEvent = (recvEvMem != NULL) ? new (recvEvMem) WsRecvEvent(conn) : NULL;
		if (m_recvEvent != NULL) {
			SetState(WS_STATE_INIT);
			return ret;
		}
		ret = SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	}

	Finalize();
	return ret;
}

/* 0x813FBD28 */
int WsStateContext::Finalize(void)
{
	while (m_controlHead != NULL)
		RemoveControlEvent(m_controlHead);

	if (m_recvEvent != NULL) {
		m_recvEvent->~WsRecvEvent();
		SceNpCommonPs4_6B428829(m_allocator, m_recvEvent);
		m_recvEvent = NULL;
	}
	for (int idx = 0; idx < WS_STATE_COUNT; ++idx) {
		if (m_states[idx] != NULL) {
			m_states[idx]->Finalize();
			m_states[idx]->~WsStateBase();
			SceNpCommonPs4_6B428829(m_allocator, m_states[idx]);
			m_states[idx] = NULL;
		}
	}
	if (SceNpCommonPs4_C78F5DF5(&m_mutex) != 0)
		SceNpCommonPs4_63EED478(&m_mutex);
	m_allocator = NULL;
	return 0;
}

/* 0x813FBE0C */
int WsStateContext::GetResponseHeader(WsConnection *conn, const char *name, const char **outValue,
				      SceSize *outValueLen)
{
	if (m_currentState >= WS_STATE_COUNT || m_states[m_currentState] == NULL)
		return SCE_NP_WS_ERROR_INTERNAL;
	return m_states[m_currentState]->GetResponseHeader(conn, name, outValue, outValueLen);
}

int WsStateContext::GetResponseStatusCode(WsConnection *conn)
{
	if (m_currentState >= WS_STATE_COUNT || m_states[m_currentState] == NULL)
		return SCE_NP_WS_ERROR_INTERNAL;
	return m_states[m_currentState]->GetResponseStatusCode(conn);
}

/* 0x813FBE86 */
int WsStateContext::Connect(void)
{
	if (m_currentState >= WS_STATE_COUNT || m_states[m_currentState] == NULL)
		return SCE_NP_WS_ERROR_INTERNAL;

	int ret = m_states[m_currentState]->PrepareConnect(this);
	if (ret < 0)
		return ret;
	ret = m_states[m_currentState]->StartConnect(this);
	if (ret < 0)
		return ret;
	return m_states[m_currentState]->CompleteHandshake(this);
}

/* 0x813FBF42 */
int WsStateContext::SendText(WsSendEventBase *event)
{
	if (m_currentState >= WS_STATE_COUNT || m_states[m_currentState] == NULL)
		return SCE_NP_WS_ERROR_INTERNAL;
	return m_states[m_currentState]->SendText(this, event);
}

/* 0x813FBFA2 */
int WsStateContext::SendClose(WsSendEventBase *event)
{
	if (m_currentState >= WS_STATE_COUNT || m_states[m_currentState] == NULL)
		return SCE_NP_WS_ERROR_INTERNAL;

	int ret = m_states[m_currentState]->BeginClose(this);
	if (ret < 0)
		return ret;
	return m_states[m_currentState]->SendClose(this, event);
}

/* 0x813FC032 */
int WsStateContext::Receive(void)
{
	if (m_currentState >= WS_STATE_COUNT || m_states[m_currentState] == NULL)
		return SCE_NP_WS_ERROR_INTERNAL;
	return m_states[m_currentState]->Receive(this);
}

/* 0x813FC0AC */
int WsStateContext::SetState(unsigned int stateIndex)
{
	if (stateIndex >= WS_STATE_COUNT)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;

	Push::ScopedLock lock(&m_mutex);
	if (m_currentState < WS_STATE_COUNT && m_states[m_currentState] != NULL)
		m_states[m_currentState]->OnExit();
	m_currentState = static_cast<int>(stateIndex);
	if (m_currentState < WS_STATE_COUNT && m_states[m_currentState] != NULL)
		m_states[m_currentState]->OnEnter();
	return 0;
}

/* 0x813FC184 */
int WsStateContext::DispatchRecv(WsHandlerBase *chain)
{
	{
		Push::ScopedLock lock(&m_mutex);
		WsSendEventBase *cur = m_controlHead;
		while (cur != NULL) {
			WsSendEventBase *next = cur->m_next;
			if (cur->Accept(chain) != 1)
				RemoveControlEvent(cur);
			cur = next;
		}
	}
	return m_recvEvent->Accept(chain);
}

/* 0x813FC224 */
int WsStateContext::DispatchSend(WsHandlerBase *chain, WsSendEventBase *event)
{
	if (event == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;
	return event->Accept(chain);
}

/* 0x813FC24A */
int WsStateContext::EnqueuePing(const void *payload, SceSize len)
{
	void *mem = SceNpCommonPs4_0C922423(sizeof(WsPingEvent), m_allocator);
	WsPingEvent *ping = (mem != NULL) ? new (mem) WsPingEvent(m_connection) : NULL;
	if (ping == NULL)
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;

	int ret = ping->SetPayload(payload, len);
	if (ret >= 0) {
		ret = EnqueueControlEvent(ping);
		if (ret >= 0)
			return ret;
	}
	ping->~WsPingEvent();
	SceNpCommonPs4_6B428829(m_allocator, ping);
	return ret;
}

/* 0x813FC2E0 */
int WsStateContext::EnqueuePong(const void *payload, SceSize len)
{
	void *mem = SceNpCommonPs4_0C922423(sizeof(WsPongEvent), m_allocator);
	WsPongEvent *pong = (mem != NULL) ? new (mem) WsPongEvent(m_connection) : NULL;
	if (pong == NULL)
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;

	int ret = pong->SetPayload(payload, len);
	if (ret >= 0) {
		ret = EnqueueControlEvent(pong);
		if (ret >= 0)
			return ret;
	}
	pong->~WsPongEvent();
	SceNpCommonPs4_6B428829(m_allocator, pong);
	return ret;
}

/* 0x813FC378 */
int WsStateContext::EnqueueControlEvent(WsSendEventBase *event)
{
	if (event == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;

	event->m_next = NULL;
	event->m_prev = m_controlTail;
	if (m_controlTail != NULL)
		m_controlTail->m_next = event;
	else
		m_controlHead = event;
	m_controlTail = event;
	return 0;
}

/* 0x813FC3F6 */
int WsStateContext::RemoveControlEvent(WsSendEventBase *event)
{
	if (event == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;

	WsSendEventBase *cur = m_controlHead;
	bool found = false;
	while (cur != NULL) {
		if (cur == event) {
			found = true;
			if (cur->m_prev != NULL)
				cur->m_prev->m_next = cur->m_next;
			else
				m_controlHead = cur->m_next;
			if (cur->m_next != NULL)
				cur->m_next->m_prev = cur->m_prev;
			else
				m_controlTail = cur->m_prev;
			break;
		}
		cur = cur->m_next;
	}
	if (!found)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;

	event->~WsSendEventBase();
	SceNpCommonPs4_6B428829(m_allocator, event);
	return 0;
}

/* ============================================================================
 * WsConnection Lifecycle & Operations (0x813FACF0..0x813FBA03)
 * ============================================================================ */

WsConnection::WsConnection()
	: m_refCount(0),
	  m_destroyRequested(0),
	  m_aborted(0),
	  m_forceClosed(0),
	  m_lock(NULL),
	  m_recvEpollId(0),
	  m_sendEpollId(0),
	  m_headerCount(0),
	  m_stateCtx(NULL),
	  m_net(NULL),
	  m_handshake(NULL),
	  m_stream(NULL),
	  m_netRecvBuf(NULL),
	  m_netRecvBufSize(0),
	  m_netRecvBytes(0),
	  m_lastPingClockUsec(0),
	  m_recvOutBuf(NULL),
	  m_recvOutCap(0),
	  m_recvOutLen(NULL),
	  m_recvOutMsgType(NULL),
	  m_recvDone(0),
	  m_respHeaderBuf(NULL),
	  m_respHeaderBufCap(0),
	  m_respHeaderOutLen(NULL)
{
	sceClibMemset(&m_openParam, 0, sizeof(m_openParam));
	sceClibMemset(m_host, 0, sizeof(m_host));
	sceClibMemset(m_path, 0, sizeof(m_path));
	sceClibMemset(m_origin, 0, sizeof(m_origin));
	sceClibMemset(m_subProtocol, 0, sizeof(m_subProtocol));
	sceClibMemset(&m_allocator, 0, sizeof(m_allocator));
	sceClibMemset(m_headerNames, 0, sizeof(m_headerNames));
	sceClibMemset(m_headerValues, 0, sizeof(m_headerValues));
}

WsConnection::~WsConnection()
{
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

/* 0x813FB6EA */
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
	if (m_stateCtx != NULL) {
		m_stateCtx->Finalize();
		m_stateCtx->~WsStateContext();
		SceNpCommonPs4_6B428829(&m_allocator, m_stateCtx);
		m_stateCtx = NULL;
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

/* 0x813FACF0 */
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

	if (ret >= 0) {
		void *ctxMem = SceNpCommonPs4_0C922423(sizeof(WsStateContext), &conn->m_allocator);
		if (ctxMem == NULL) {
			ret = SCE_NP_WS_ERROR_OUT_OF_MEMORY;
		} else {
			conn->m_stateCtx = new (ctxMem) WsStateContext(&conn->m_allocator);
			ret = conn->m_stateCtx->Init(conn);
		}
	}

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

/* 0x813FA99C */
int WsConnection::Connect(char *respHeaderBuf, SceSize respHeaderBufSize,
			  SceSize *outRespHeaderSize)
{
	if (outRespHeaderSize != NULL)
		*outRespHeaderSize = 0;

	int ret = Acquire();
	if (ret < 0)
		return ret;

	SetHandshakeRespBuffer(respHeaderBuf, respHeaderBufSize, outRespHeaderSize);
	if (m_stateCtx != NULL)
		ret = m_stateCtx->Connect();
	else
		ret = SCE_NP_WS_ERROR_INTERNAL;
	SetHandshakeRespBuffer(NULL, 0, NULL);

	Release();
	return ret;
}

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
	if (m_stateCtx != NULL)
		m_stateCtx->SetState(WS_STATE_INIT);

	Release();
	return 0;
}

/* 0x813FAA1C */
int WsConnection::SendText(const char *text, SceSize length)
{
	int ret = Acquire();
	if (ret < 0)
		return ret;

	if (m_stateCtx == NULL) {
		Release();
		return SCE_NP_WS_ERROR_INTERNAL;
	}

	void *evMem = SceNpCommonPs4_0C922423(sizeof(WsSendTextEvent), &m_allocator);
	WsSendTextEvent *ev = (evMem != NULL) ? new (evMem) WsSendTextEvent(this) : NULL;
	if (ev == NULL) {
		ret = SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	} else {
		ret = ev->SetText(text, length);
		if (ret >= 0)
			ret = m_stateCtx->SendText(ev);
		ev->~WsSendTextEvent();
		SceNpCommonPs4_6B428829(&m_allocator, ev);
	}

	Release();
	return ret;
}

/* 0x813FAB0A */
int WsConnection::SendClose(void)
{
	int ret = Acquire();
	if (ret < 0)
		return ret;

	if (m_stateCtx == NULL) {
		Release();
		return SCE_NP_WS_ERROR_INTERNAL;
	}

	void *evMem = SceNpCommonPs4_0C922423(sizeof(WsCloseEvent), &m_allocator);
	WsCloseEvent *ev = (evMem != NULL) ? new (evMem) WsCloseEvent(this) : NULL;
	if (ev == NULL) {
		ret = SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	} else {
		ret = ev->SetPayload(NULL, 0);
		if (ret >= 0)
			ret = m_stateCtx->SendClose(ev);
		ev->~WsCloseEvent();
		SceNpCommonPs4_6B428829(&m_allocator, ev);
	}

	Release();
	return ret;
}

/* 0x813FABF8 */
int WsConnection::Receive(int *outMsgType, void *outBuf, SceSize bufSize, SceSize *outReceived)
{
	if (outBuf == NULL || bufSize == 0 || outReceived == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;

	int ret = Acquire();
	if (ret < 0)
		return ret;

	if (m_stateCtx == NULL) {
		Release();
		return SCE_NP_WS_ERROR_INTERNAL;
	}

	*outReceived = 0;
	int localMsgType = 0;
	int *msgTypeDst = (outMsgType != NULL) ? outMsgType : &localMsgType;
	SetReceiveTargets(outBuf, bufSize, msgTypeDst, outReceived);
	ret = m_stateCtx->Receive();

	Release();
	return ret;
}

int WsConnection::GetResponseStatusCode(void)
{
	int ret = Acquire();
	if (ret < 0)
		return ret;

	if (m_stateCtx != NULL)
		ret = m_stateCtx->GetResponseStatusCode(this);
	else
		ret = SCE_NP_WS_ERROR_INTERNAL;

	Release();
	return ret;
}

int WsConnection::GetResponseHeader(const char *name, const char **outValue, SceSize *outValueLen)
{
	int ret = Acquire();
	if (ret < 0)
		return ret;

	if (m_stateCtx != NULL)
		ret = m_stateCtx->GetResponseHeader(this, name, outValue, outValueLen);
	else
		ret = SCE_NP_WS_ERROR_INTERNAL;

	Release();
	return ret;
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
	return static_cast<sce::Np::WebSocket::WsConnection *>(wsHandle)->SendText(text, length);
}

/* 0x813FAB0A */
int npWebSocketSendClose(void *wsHandle)
{
	if (wsHandle == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;
	return static_cast<sce::Np::WebSocket::WsConnection *>(wsHandle)->SendClose();
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
