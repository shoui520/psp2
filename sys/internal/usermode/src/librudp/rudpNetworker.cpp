/*
 * rudpNetworker.cpp: background epoll I/O thread for internal IO thread mode.
 */
#include "rudp_internal.h"

RudpNetworker::RudpNetworker(RudpScheduler *scheduler)
	: m_scheduler(scheduler),
	  m_delegate(0),
	  m_poll(0),
	  m_wakeSock(-1),
	  m_wakePending(0),
	  m_recvBufSize(2048),
	  m_recvBuf(0)
{
}

RudpNetworker::~RudpNetworker()
{
	if (m_recvBuf != 0) {
		rudpFree(m_recvBuf);
	}
}

void RudpNetworker::setDelegate(IRudpNetworkerDelegate *delegate)
{
	m_delegate = delegate;
}

void RudpNetworker::setBufferSize(uint16_t size)
{
	m_recvBufSize = size;
}

RudpResult RudpNetworker::start()
{
	m_recvBuf = (uint8_t *)rudpAlloc(m_recvBufSize);
	if (m_recvBuf == 0) {
		return RudpResult(SCE_RUDP_ERROR_MEMORY);
	}
	int ret = rudpNetInetPton(SCE_NET_AF_INET, "127.0.0.1", &m_wakeAddr);
	if (ret < 0) {
		return RudpResult(ret);
	}
	m_wakeAddr.sa_data[0] = 0;
	m_wakeAddr.sa_data[1] = 0;
	m_wakeSock = rudpNetSocket(SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM_P2P);
	if (m_wakeSock < 0) {
		return RudpResult(m_wakeSock);
	}
	ret = rudpNetBind(m_wakeSock, (const SceNetSockaddr *)(const void *)&m_wakeAddr);
	if (ret < 0 && m_wakeSock < 0) {
		return RudpResult(ret);
	}
	ret = rudpNetGetsockname(m_wakeSock, (SceNetSockaddr *)(void *)&m_wakeAddr);
	if (ret < 0) {
		rudpNetClose(m_wakeSock);
		return RudpResult(ret);
	}
	ret = rudpNetSetNonBlocking(m_wakeSock, 1);
	if (ret < 0) {
		rudpNetClose(m_wakeSock);
		return RudpResult(ret);
	}
	RudpAllocator alloc;
	alloc.allocFn = ::operator new;
	alloc.freeFn = rudpFree;
	ret = rudpNetPollCreate(&m_poll, 8, &alloc);
	if (ret < 0) {
		rudpNetClose(m_wakeSock);
		return RudpResult(ret);
	}
	ret = rudpNetPollControl(m_poll, 1, m_wakeSock, 1);
	if (ret < 0) {
		rudpNetPollDestroy(m_poll);
		m_poll = 0;
		rudpNetClose(m_wakeSock);
		m_wakeSock = -1;
		return RudpResult(ret);
	}
	RudpResult startRes = RudpThread::start();
	if (startRes.getCode() >= 0) {
		return RudpResult(0);
	}
	rudpNetPollDestroy(m_poll);
	m_poll = 0;
	rudpNetClose(m_wakeSock);
	return startRes;
}

void RudpNetworker::wakeUp()
{
	if (m_running && rudpAtomicAdd(&m_wakePending, 1) == 0) {
		int dummy = 0;
		rudpNetSendto(m_wakeSock, &dummy, sizeof(dummy), 0, (const SceNetSockaddr *)(const void *)&m_wakeAddr);
	}
}

RudpResult RudpNetworker::stop()
{
	RudpResult res;
	wakeUp();
	RudpResult stopRes = RudpThread::stop();
	res = stopRes;
	if (m_poll != 0) {
		rudpNetPollDestroy(m_poll);
		m_poll = 0;
	}
	if (m_wakeSock >= 0) {
		rudpNetClose(m_wakeSock);
		m_wakeSock = -1;
	}
	if (m_recvBuf != 0) {
		rudpFree(m_recvBuf);
		m_recvBuf = 0;
	}
	return res;
}

RudpResult RudpNetworker::addSocket(int sock)
{
	if (!m_running) {
		return RudpResult(rudpRecordError("rudpNetworker.cpp", 183));
	}
	int ret = rudpNetPollControl(m_poll, 1, sock, 1);
	if (ret < 0) {
		rudpRecordError("rudpNetworker.cpp", 193);
		return RudpResult(ret);
	}
	wakeUp();
	return RudpResult(0);
}

RudpResult RudpNetworker::removeSocket(int sock)
{
	if (!m_running) {
		return RudpResult(rudpRecordError("rudpNetworker.cpp", 204));
	}
	int ret = rudpNetPollControl(m_poll, 3, sock, 0);
	if (ret < 0) {
		rudpRecordError("rudpNetworker.cpp", 213);
		return RudpResult(ret);
	}
	wakeUp();
	return RudpResult(0);
}

int RudpNetworker::run()
{
	RudpOsalPollEvent events[8];
	SceNetSockaddr fromAddr;

	rudpAtomicAnd(&m_wakePending, 0);
	while (m_running) {
		int n = rudpNetPollWait(m_poll, events, 8, 0, 27);
		if (n > 0) {
			for (int i = 0; i < n; i++) {
				if (events[i].sock == m_wakeSock) {
					if ((events[i].revents & 1) != 0) {
						if (rudpNetRecvfrom(m_wakeSock, m_recvBuf, m_recvBufSize, 0, &fromAddr) < 0) {
							rudpNetGetLastError();
						}
					}
					continue;
				}
				if ((events[i].revents & 1) != 0) {
					int recvd = rudpNetRecvfrom(events[i].sock, m_recvBuf, m_recvBufSize, 0, &fromAddr);
					if (recvd >= 0) {
						if (m_delegate != 0) {
							rudpMutexLock(&g_rudpMutex);
							if (!m_running) {
								rudpMutexUnlock(&g_rudpMutex);
								return 0;
							}
							if (m_delegate->onNetworkerReceived(this, events[i].sock, m_recvBuf, (uint32_t)recvd, &fromAddr, sizeof(fromAddr)) < 0) {
								rudpNetPollControl(m_poll, 3, events[i].sock, 0);
								rudpMutexUnlock(&g_rudpMutex);
								continue;
							}
							rudpMutexUnlock(&g_rudpMutex);
						}
					} else {
						int netErr = rudpNetGetLastError();
						if (netErr != RUDP_NET_ERRNO_EAGAIN && netErr != RUDP_NET_ERRNO_ECONNRESET) {
							if (m_delegate != 0) {
								rudpMutexLock(&g_rudpMutex);
								if (!m_running) {
									rudpMutexUnlock(&g_rudpMutex);
									return 0;
								}
								m_delegate->onNetworkerSocketError(this, events[i].sock);
								rudpMutexUnlock(&g_rudpMutex);
							}
							rudpNetPollControl(m_poll, 3, events[i].sock, 0);
							continue;
						}
					}
				}
				if ((events[i].revents & 4) != 0) {
					if (m_delegate != 0) {
						rudpMutexLock(&g_rudpMutex);
						if (!m_running) {
							rudpMutexUnlock(&g_rudpMutex);
							return 0;
						}
						m_delegate->onNetworkerSocketError(this, events[i].sock);
						rudpMutexUnlock(&g_rudpMutex);
					}
					rudpNetPollControl(m_poll, 3, events[i].sock, 0);
				}
			}
		} else {
			if (n < 0) {
				if (rudpNetGetLastError() == RUDP_NET_ERRNO_EINTR) {
					rudpDelay1Ms();
				}
			}
			rudpDelay1Ms();
		}

		rudpAtomicAnd(&m_wakePending, 0);
		rudpMutexLock(&g_rudpMutex);
		if (!m_running) {
			rudpMutexUnlock(&g_rudpMutex);
			return 0;
		}
		RudpResult procRes = m_scheduler->processEvents(0);
		if (procRes.getCode() < 0) {
			procRes.getCode();
		}
		rudpMutexUnlock(&g_rudpMutex);
	}
	return 0;
}
