/*
 * teleport_http_server.cpp: SceShellTeleportHttpCommThread, SceTeleportTcpServer,
 * SceTeleportHttpEpoll, and HTTP/1.1 header parser (0x813A0FB4..0x813A1E33).
 */
#include "teleport_internal.h"

#include <kernel/threadmgr.h>
#include <libnetctl.h>
#include <std/ctype.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>

extern "C" {
char *sce_paf_private_strtok(const char *haystack, const char *needle);
}

/* 0x813A0FB4 */
TeleportHttpServerThread::TeleportHttpServerThread(const TeleportHttpCallbacks &callbacks)
	: paf::thread::Thread(0x1000011F, 0x4000, "SceShellTeleportHttpCommThread", NULL),
	  m_listenSocket(0),
	  m_epollId(0),
	  m_epollEvents(NULL),
	  m_listenPort(0),
	  m_eventFlag(0),
	  m_stopState(0),
	  m_useEphemeralPort(true),
	  m_receivingBody(false),
	  m_receivedBytes(0),
	  m_expectedBytes(0)
{
	sce_paf_memset(&m_callbacks, 0, sizeof(m_callbacks));
	m_callbacks = callbacks;
	sce_paf_memset(m_localIpv4, 0, sizeof(m_localIpv4));
	m_eventFlag = sceKernelCreateEventFlag("SceTeleportHttpEvent", 0x1000, 0, NULL);
}

TeleportHttpServerThread::~TeleportHttpServerThread()
{
}

/* 0x813A1154 */
SceInt32 TeleportHttpServerThread::SendWithTimeout(TeleportHttpServerThread *thread,
						   SceNetId sock, const void *buffer,
						   SceSize length, SceInt32 timeoutUs)
{
	(void)thread;
	SceInt32 sendTimeout = TELEPORT_DEFAULT_SEND_TIMEOUT_US;
	if (timeoutUs > 0)
		sendTimeout = timeoutUs;

	SceInt32 ret = sceNetSetsockopt(sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_SNDTIMEO,
					&sendTimeout, sizeof(sendTimeout));
	if (ret >= 0)
		return sceNetSend(sock, buffer, length, 0);
	return ret;
}

/* 0x813A11B4 */
SceInt32 TeleportHttpServerThread::ParseHttpRequestHeader(const char *rawRequest, SceSize length,
							  SceSize *outExpectedTotal,
							  TeleportHttpRequestInfo *outInfo)
{
	const char *contentLenPrefix = "CONTENT-LENGTH:";
	const SceSize contentLenPrefixSize = sce_paf_strlen(contentLenPrefix);
	const char *authPrefix = "AUTHORIZATION:";
	const SceSize authPrefixSize = sce_paf_strlen(authPrefix);
	const char *authScheme = "Teleport";
	const SceSize authSchemeSize = sce_paf_strlen(authScheme);

	char headerBuf[512];
	sce_paf_memset(headerBuf, 0, sizeof(headerBuf));

	outInfo->contentLength = -1;
	outInfo->authToken.assign("");
	outInfo->uri.assign("");
	outInfo->totalLength = static_cast<SceSize>(-1);
	*outExpectedTotal = 0;

	SceSize copyLen = (length < sizeof(headerBuf)) ? length : (sizeof(headerBuf) - 1);
	char *headerEnd = sce_paf_private_strtok(rawRequest, "\r\n\r\n");
	SceSize headerSize = copyLen;
	if (headerEnd != NULL) {
		headerSize = static_cast<SceSize>(headerEnd - rawRequest) + sce_paf_strlen("\r\n\r\n");
		if (headerSize > sizeof(headerBuf))
			return TELEPORT_HTTP_UNKNOWN;
	}

	sce_paf_strncpy(headerBuf, rawRequest, headerSize);

	paf::string headerStr(headerBuf);
	paf::string crlfDelim("\r\n");
	paf::vector<paf::string> lines;
	teleportSplitString(&lines, headerStr, crlfDelim, 10);
	if (lines.empty())
		return TELEPORT_HTTP_UNKNOWN;

	paf::string spaceDelim(" ");
	paf::vector<paf::string> requestTokens;
	teleportSplitString(&requestTokens, lines[0], spaceDelim, 3);
	if (requestTokens.size() < 2)
		return TELEPORT_HTTP_UNKNOWN;

	outInfo->uri = requestTokens[1];

	SceInt32 method = TELEPORT_HTTP_POST;
	if (sce_paf_strncmp(requestTokens[0].c_str(), "POST", requestTokens[0].size()) != 0) {
		if (sce_paf_strncmp(requestTokens[0].c_str(), "GET", requestTokens[0].size()) == 0)
			method = TELEPORT_HTTP_GET;
		else if (sce_paf_strncmp(requestTokens[0].c_str(), "DELETE",
					 requestTokens[0].size()) == 0)
			method = TELEPORT_HTTP_DELETE;
		else if (sce_paf_strncmp(requestTokens[0].c_str(), "OPTIONS",
					 requestTokens[0].size()) == 0)
			method = TELEPORT_HTTP_OPTIONS;
		else
			method = TELEPORT_HTTP_UNKNOWN;
	}

	SceInt32 parsedContentLength = 0;
	for (SceSize lineIdx = 1; lineIdx < lines.size(); ++lineIdx) {
		if (sce_paf_strncasecmp(lines[lineIdx].c_str(), contentLenPrefix,
					contentLenPrefixSize) == 0) {
			paf::vector<paf::string> lenTokens;
			teleportSplitString(&lenTokens, lines[lineIdx], spaceDelim, 3);
			if (lenTokens.size() == 2) {
				paf::string digits;
				const paf::string &rawDigits = lenTokens[1];
				for (SceSize digitIdx = 0; digitIdx < 10 && digitIdx < rawDigits.size();
				     ++digitIdx) {
					char ch = rawDigits[digitIdx];
					if (ch < '0' || ch > '9')
						break;
					digits.append(1, ch);
				}
				parsedContentLength = sce_paf_atoi(digits.c_str());
				outInfo->contentLength = parsedContentLength;
			}
		} else if (sce_paf_strncasecmp(lines[lineIdx].c_str(), authPrefix,
					       authPrefixSize) == 0) {
			paf::vector<paf::string> authTokens;
			teleportSplitString(&authTokens, lines[lineIdx], spaceDelim, 4);
			if (authTokens.size() == 3 &&
			    sce_paf_strncmp(authTokens[1].c_str(), authScheme,
					    authSchemeSize) == 0 &&
			    authTokens[2].size() == 64) {
				paf::string upperToken = authTokens[2];
				for (SceSize charIdx = 0; charIdx < upperToken.size(); ++charIdx)
					upperToken[charIdx] =
						static_cast<char>(sce_paf_toupper(upperToken[charIdx]));
				outInfo->authToken = upperToken;
			}
		}
	}

	SceSize expectedTotal = headerSize + static_cast<SceSize>(parsedContentLength);
	if (static_cast<SceInt32>(expectedTotal) > static_cast<SceInt32>(TELEPORT_HTTP_BUFFER_SIZE))
		expectedTotal = 0;

	*outExpectedTotal = expectedTotal;
	outInfo->totalLength = expectedTotal;
	return method;
}

/* 0x813A18C0 */
SceInt32 TeleportHttpServerThread::BindAndListen(void)
{
	m_listenSocket = sceNetSocket("SceTeleportTcpServer", SCE_NET_AF_INET,
				      SCE_NET_SOCK_STREAM, 0);

	SceNetSockaddrIn bindAddr;
	sce_paf_memset(&bindAddr, 0, sizeof(bindAddr));
	bindAddr.sin_family = SCE_NET_AF_INET;
	bindAddr.sin_port = sceNetHtons(m_useEphemeralPort ? 0 : m_listenPort);
	bindAddr.sin_addr.s_addr = sceNetHtonl(SCE_NET_INADDR_ANY);

	SceInt32 ret = sceNetBind(m_listenSocket,
				  reinterpret_cast<const SceNetSockaddr *>(&bindAddr),
				  sizeof(bindAddr));
	if (ret >= 0) {
		SceNetSockInfo sockInfo;
		sceNetGetSockInfo(m_listenSocket, &sockInfo, 1, 0);
		m_listenPort = sceNetHtons(sockInfo.local_port);
		return sceNetListen(m_listenSocket, TELEPORT_MAX_EPOLL_EVENTS);
	}
	return ret;
}

/* 0x813A195E */
void TeleportHttpServerThread::EntryFunction()
{
	SceInt32 readyCount = 0;
	SceNetId acceptedSocket = 0;
	SceNetId registeredListenSocket = 0;

	for (;;) {
		SceUInt32 outBits = 0;
		sceKernelWaitEventFlag(m_eventFlag, TELEPORT_EVENT_READY_BIT, 5, &outBits, NULL);

		if (m_listenSocket == 0) {
			BindAndListen();
		} else {
			if (m_epollEvents == NULL) {
				m_epollEvents = static_cast<SceNetEpollEvent *>(
					ScePafStdc_C9532982(TELEPORT_MAX_EPOLL_EVENTS,
						       sizeof(SceNetEpollEvent)));
				m_epollId = sceNetEpollCreate("SceTeleportHttpEpoll", 0);
				if (m_epollId < 0)
					return;

				SceNetEpollEvent ev;
				sce_paf_memset(&ev, 0, sizeof(ev));
				ev.events = SCE_NET_EPOLLIN;
				ev.data.fd = m_listenSocket;
				sceNetEpollControl(m_epollId, SCE_NET_EPOLL_CTL_ADD,
						   m_listenSocket, &ev);
				registeredListenSocket = m_listenSocket;
			}

			readyCount = sceNetEpollWait(m_epollId, m_epollEvents,
						     TELEPORT_MAX_EPOLL_EVENTS, -1);
			if (readyCount > 0) {
				for (SceInt32 evIdx = 0; evIdx < readyCount; ++evIdx) {
					SceNetId activeFd = m_epollEvents[evIdx].data.fd;
					if (activeFd == m_listenSocket) {
						SceNetSockaddr peerAddr;
						SceNetSocklen_t addrLen = sizeof(peerAddr);
						acceptedSocket = sceNetAccept(m_listenSocket,
									      &peerAddr, &addrLen);
						if (acceptedSocket >= 0) {
							SceNetEpollEvent ev;
							sce_paf_memset(&ev, 0, sizeof(ev));
							ev.events = SCE_NET_EPOLLIN;
							ev.data.fd = acceptedSocket;
							sceNetEpollControl(m_epollId,
									   SCE_NET_EPOLL_CTL_ADD,
									   acceptedSocket, &ev);
						}
						break;
					}

					SceUInt32 evFlags = m_epollEvents[evIdx].events;
					if ((evFlags & SCE_NET_EPOLLHUP) != 0) {
						if (m_callbacks.onDisconnect != NULL)
							m_callbacks.onDisconnect(
								this, activeFd,
								m_callbacks.userData);
						SceNetEpollEvent delEv;
						sce_paf_memset(&delEv, 0, sizeof(delEv));
						sceNetEpollControl(m_epollId, SCE_NET_EPOLL_CTL_DEL,
								   activeFd, &delEv);
						sceNetSocketClose(activeFd);
						break;
					}

					if ((evFlags & SCE_NET_EPOLLIN) == 0)
						continue;

					if (!m_receivingBody)
						sce_paf_memset(m_callbacks.recvBuffer, 0,
							       m_callbacks.recvBufferSize);

					SceInt32 peekLen = sceNetRecv(
						activeFd, NULL, m_callbacks.recvBufferSize,
						SCE_NET_MSG_PEEK | SCE_NET_MSG_USECRYPTO);
					if (peekLen <= 0 ||
					    peekLen >= static_cast<SceInt32>(TELEPORT_HTTP_BUFFER_SIZE)) {
						if (m_callbacks.onDisconnect != NULL)
							m_callbacks.onDisconnect(
								this, activeFd,
								m_callbacks.userData);
						SceNetEpollEvent delEv;
						sce_paf_memset(&delEv, 0, sizeof(delEv));
						sceNetEpollControl(m_epollId, SCE_NET_EPOLL_CTL_DEL,
								   activeFd, &delEv);
						sceNetSocketClose(activeFd);
						m_receivedBytes = 0;
						m_expectedBytes = 0;
						m_receivingBody = false;
						continue;
					}

					char *chunk = new char[peekLen];
					if (chunk == NULL) {
						if (m_callbacks.onDisconnect != NULL)
							m_callbacks.onDisconnect(
								this, activeFd,
								m_callbacks.userData);
						SceNetEpollEvent delEv;
						sce_paf_memset(&delEv, 0, sizeof(delEv));
						sceNetEpollControl(m_epollId, SCE_NET_EPOLL_CTL_DEL,
								   activeFd, &delEv);
						sceNetSocketClose(activeFd);
						m_receivedBytes = 0;
						m_expectedBytes = 0;
						m_receivingBody = false;
						continue;
					}

					SceInt32 recvLen = sceNetRecv(activeFd, chunk,
								      static_cast<SceSize>(peekLen),
								      0);
					if (recvLen > 0) {
						SceSize chunkExpected = 0;
						TeleportHttpRequestInfo probeInfo;
						if (ParseHttpRequestHeader(
							    chunk, static_cast<SceSize>(peekLen),
							    &chunkExpected, &probeInfo) !=
						    TELEPORT_HTTP_UNKNOWN) {
							m_receivedBytes = 0;
							m_expectedBytes = 0;
							m_receivingBody = false;
							sce_paf_memset(m_callbacks.recvBuffer, 0,
								       m_callbacks.recvBufferSize);
						}

						if (m_receivedBytes + static_cast<SceSize>(peekLen) <
						    m_callbacks.recvBufferSize) {
							sce_paf_memcpy(
								m_callbacks.recvBuffer +
									m_receivedBytes,
								chunk,
								static_cast<SceSize>(recvLen));
							m_receivedBytes +=
								static_cast<SceSize>(recvLen);
							if (!m_receivingBody)
								m_expectedBytes = chunkExpected;
							if (m_receivedBytes < m_expectedBytes) {
								m_receivingBody = true;
							} else {
								m_receivedBytes = 0;
								m_expectedBytes = 0;
								m_receivingBody = false;
							}
						} else {
							recvLen = -1;
							m_receivedBytes = 0;
							m_expectedBytes = 0;
							m_receivingBody = false;
						}
					}

					delete[] chunk;

					if (recvLen <= 0) {
						if (m_callbacks.onDisconnect != NULL)
							m_callbacks.onDisconnect(
								this, activeFd,
								m_callbacks.userData);
						SceNetEpollEvent delEv;
						sce_paf_memset(&delEv, 0, sizeof(delEv));
						sceNetEpollControl(m_epollId, SCE_NET_EPOLL_CTL_DEL,
								   activeFd, &delEv);
						sceNetSocketClose(activeFd);
						m_receivedBytes = 0;
						m_expectedBytes = 0;
						m_receivingBody = false;
						continue;
					}

					if (m_receivingBody)
						break;

					SceSize parsedTotal = 0;
					TeleportHttpRequestInfo reqInfo;
					SceInt32 method = ParseHttpRequestHeader(
						m_callbacks.recvBuffer, m_callbacks.recvBufferSize,
						&parsedTotal, &reqInfo);

					SceInt32 handlerRet = 0;
					bool sendErrorReply = false;
					if (method == TELEPORT_HTTP_POST) {
						if (m_callbacks.onPost != NULL) {
							handlerRet = m_callbacks.onPost(
								this, activeFd,
								m_callbacks.recvBuffer,
								m_callbacks.recvBufferSize,
								SendWithTimeout, &reqInfo);
							sendErrorReply = (handlerRet != 0);
						}
					} else if (method == TELEPORT_HTTP_GET) {
						if (m_callbacks.onGet != NULL) {
							handlerRet = m_callbacks.onGet(
								this, activeFd,
								m_callbacks.recvBuffer,
								m_callbacks.recvBufferSize,
								SendWithTimeout, &reqInfo);
							sendErrorReply = (handlerRet != 0);
						}
					} else if (method == TELEPORT_HTTP_DELETE) {
						if (m_callbacks.onDelete != NULL) {
							handlerRet = m_callbacks.onDelete(
								this, activeFd,
								m_callbacks.recvBuffer,
								m_callbacks.recvBufferSize,
								SendWithTimeout, &reqInfo);
							sendErrorReply = (handlerRet != 0);
						}
					} else if (method == TELEPORT_HTTP_OPTIONS) {
						if (m_callbacks.onOptions != NULL) {
							handlerRet = m_callbacks.onOptions(
								this, activeFd,
								m_callbacks.recvBuffer,
								m_callbacks.recvBufferSize,
								SendWithTimeout,
								m_callbacks.userData);
							sendErrorReply = (handlerRet != 0);
						}
					} else {
						handlerRet = -1;
						sendErrorReply = true;
					}

					if (sendErrorReply) {
						char errResp[128];
						sce_paf_memset(errResp, 0, sizeof(errResp));
						if (handlerRet == -2) {
							sce_paf_snprintf(
								errResp, sizeof(errResp),
								"HTTP/1.1 411 LENGTH REQUIRED\r\n"
								"Content-Length: 0\r\n\r\n");
						} else if (handlerRet == -3) {
							sce_paf_snprintf(
								errResp, sizeof(errResp),
								"HTTP/1.1 404 NOT FOUND\r\n"
								"Content-Length: 0\r\n\r\n");
						} else {
							sce_paf_snprintf(
								errResp, sizeof(errResp),
								"HTTP/1.1 403 FORBIDDEN\r\n"
								"Content-Length: 0\r\n\r\n");
						}
						SendWithTimeout(this, activeFd, errResp,
								sce_paf_strlen(errResp) + 1, 0);
					}
					break;
				}
			}
		}

		if ((readyCount | acceptedSocket) >= 0) {
			if (m_stopState == 2) {
				m_stopState = 0;
				if (m_epollEvents != NULL) {
					sce_paf_free(m_epollEvents);
					m_epollEvents = NULL;
				}
			} else {
				sceKernelSetEventFlag(m_eventFlag, TELEPORT_EVENT_READY_BIT);
			}

			if (m_listenSocket != registeredListenSocket && m_epollEvents != NULL) {
				sce_paf_free(m_epollEvents);
				m_epollEvents = NULL;
			}
		} else {
			readyCount = 0;
			acceptedSocket = 0;
			if (m_epollEvents != NULL) {
				sce_paf_free(m_epollEvents);
				m_epollEvents = NULL;
			}
			if (m_epollId != 0) {
				sceNetEpollAbort(m_epollId, SCE_NET_EPOLL_ABORT_FLAG_PRESERVATION);
				sceNetEpollDestroy(m_epollId);
				m_epollId = 0;
			}
			if (m_listenSocket != 0) {
				sceNetSocketAbort(m_listenSocket,
						  SCE_NET_SOCKET_ABORT_FLAG_RCV_PRESERVATION);
				sceNetSocketClose(m_listenSocket);
				m_listenSocket = 0;
			}

			SceInt32 netState = 0;
			sceNetCtlInetGetState(&netState);
			if (netState == SCE_NET_CTL_STATE_IPOBTAINED)
				sceKernelSetEventFlag(m_eventFlag, TELEPORT_EVENT_READY_BIT);
		}
	}
}

TeleportHttpServer::~TeleportHttpServer()
{
	DestroyThread();
}

TeleportHttpServer *TeleportHttpServer::GetInstance(void)
{
	static TeleportHttpServer instance;
	return &instance;
}

/* 0x813A1070 */
SceInt32 TeleportHttpServer::Start(const TeleportHttpCallbacks &callbacks)
{
	m_thread = new TeleportHttpServerThread(callbacks);
	if (m_thread != NULL)
		m_thread->Start();
	return 0;
}

/* 0x813A10A0 */
SceInt32 TeleportHttpServer::Stop(void)
{
	if (m_thread == NULL)
		return 0;

	m_thread->m_stopState = 2;
	if (m_thread->m_epollId != 0) {
		sceNetEpollAbort(m_thread->m_epollId, SCE_NET_EPOLL_ABORT_FLAG_PRESERVATION);
		sceNetEpollDestroy(m_thread->m_epollId);
		m_thread->m_epollId = 0;
	}
	if (m_thread->m_listenSocket != 0) {
		sceNetSocketAbort(m_thread->m_listenSocket,
				  SCE_NET_SOCKET_ABORT_FLAG_RCV_PRESERVATION);
		sceNetSocketClose(m_thread->m_listenSocket);
		m_thread->m_listenSocket = 0;
	}
	return 0;
}

/* 0x813A10D4 */
SceInt32 TeleportHttpServer::Reconfigure(const SceUInt8 localIpv4[4], SceUInt16 port)
{
	if (m_thread == NULL)
		return 0;

	m_thread->m_stopState = 0;
	if (m_thread->m_epollId != 0) {
		sceNetEpollAbort(m_thread->m_epollId, SCE_NET_EPOLL_ABORT_FLAG_PRESERVATION);
		sceNetEpollDestroy(m_thread->m_epollId);
		m_thread->m_epollId = 0;
	}
	if (m_thread->m_listenSocket != 0) {
		sceNetSocketAbort(m_thread->m_listenSocket,
				  SCE_NET_SOCKET_ABORT_FLAG_RCV_PRESERVATION);
		sceNetSocketClose(m_thread->m_listenSocket);
		m_thread->m_listenSocket = 0;
	}

	sce_paf_memcpy(m_thread->m_localIpv4, localIpv4, 4);
	if (port != 0) {
		m_thread->m_listenPort = port;
		m_thread->m_useEphemeralPort = false;
	} else {
		m_thread->m_useEphemeralPort = true;
	}
	return sceKernelSetEventFlag(m_thread->m_eventFlag, TELEPORT_EVENT_READY_BIT);
}

/* 0x813A1132 */
SceInt32 TeleportHttpServer::DispatchIpmiResponse(const TeleportIpmiLaunchResponse *response,
						  SceInt32 timeoutUs)
{
	if (m_thread != NULL && m_thread->m_callbacks.onIpmiResponse != NULL) {
		m_thread->m_callbacks.onIpmiResponse(m_thread, response, timeoutUs,
						     TeleportHttpServerThread::SendWithTimeout,
						     m_thread->m_callbacks.userData);
	}
	return 0;
}

void TeleportHttpServer::DestroyThread(void)
{
	if (m_thread != NULL) {
		delete m_thread;
		m_thread = NULL;
	}
}
