/*
 * DTCPIP-SW AKE transport, timer, and session list implementation.
 */

#include "sw_ake_transport.h"

#include <string.h>

static int g_swAkeListenBacklog = 5;

/* ---- SwTimer ---- */

SwTimer::SwTimer()
	: m_running(0), m_startMs(0), m_durationMs(0)
{
	m_reserved[0] = 0;
	m_reserved[1] = 0;
	m_reserved[2] = 0;
}

SwTimer::~SwTimer() {}

void SwTimer::start(uint32_t durationMs)
{
	m_startMs = swGetTickMs();
	m_durationMs = durationMs;
	m_running = 1;
}

void SwTimer::stop()
{
	m_running = 0;
}

bool SwTimer::isExpired() const
{
	if (!m_running)
		return false;
	uint32_t now = swGetTickMs();
	uint32_t elapsed = (now >= m_startMs) ? (now - m_startMs) : (0xFFFFFFFFu - m_startMs + now);
	return elapsed >= m_durationMs;
}

/* ---- SwAkeCmdTimerThread ---- */

SwAkeCmdTimerThread::SwAkeCmdTimerThread(SwAkeCmdTimerCallback *cb)
	: SwThread(TYPE_AKE_CMD_TIMER), m_active(1), m_cb(cb), m_timer()
{
	m_reserved[0] = 0;
	m_reserved[1] = 0;
	m_reserved[2] = 0;
}

SwAkeCmdTimerThread::~SwAkeCmdTimerThread()
{
	if (m_active)
		stopTimer();
	join();
}

void SwAkeCmdTimerThread::startTimer(uint32_t timeoutMs)
{
	m_timer.start(timeoutMs);
}

void SwAkeCmdTimerThread::stopTimer()
{
	m_condMutex.lock();
	m_active = 0;
	if (m_cond)
		m_cond->signal();
	m_condMutex.unlock();
}

void SwAkeCmdTimerThread::run()
{
	m_result = 0;
	int err = 0;
	while (m_active) {
		if (m_timer.isExpired()) {
			err = 1033;
			m_timer.stop();
			m_active = 0;
			if (m_cb)
				m_cb->onAkeTimeout(1033);
		} else {
			if (m_condMutex.lock()) {
				if (m_cond) {
					if (m_active) {
						err = m_cond->wait(50);
						if (err == 9)
							err = 0;
					}
				} else {
					err = 1;
				}
				if (!m_condMutex.unlock())
					err = 1;
			} else {
				err = 1;
			}
			if (err != 0)
				m_active = 0;
		}
	}
	m_result = err;
}

/* ---- SwAkeTcpChannel ---- */

SwAkeTcpChannel::SwAkeTcpChannel()
	: m_sock(NULL), m_bufUsed(0), m_ttlEnabled(1),
	  m_ttlProbe(), m_lastErr(0), m_cancelFlag(0)
{
	memset(m_rxBuf, 0, sizeof(m_rxBuf));
	m_reserved[0] = 0;
	m_reserved[1] = 0;
	m_reserved[2] = 0;
}

SwAkeTcpChannel::~SwAkeTcpChannel()
{
	close();
}

int SwAkeTcpChannel::configureSocket(SwTcpSocket *sock)
{
	sock->setNoDelay(1);
	if (!sock->setIpTtl(3))
		return 212;
	return 0;
}

int SwAkeTcpChannel::connectTo(const SwNetAddress &addr, int timeoutMs)
{
	close();
	SwTcpClientSocket *cli = new (std::nothrow) SwTcpClientSocket();
	if (!cli)
		return 2;
	int err = cli->open();
	if (err == 0)
		err = configureSocket(cli);
	if (err == 0) {
		m_ttlProbe.openSink(cli->fd());
		err = cli->connect(addr, &m_cancelFlag, timeoutMs);
	}
	if (err != 0) {
		delete cli;
		return err;
	}
	m_sock = cli;
	return 0;
}

int SwAkeTcpChannel::attachAccepted(SwTcpSocket *clientSock, const SwSocket &serverSock)
{
	close();
	if (!clientSock)
		return 5;
	int err = configureSocket(clientSock);
	if (err != 0) {
		delete clientSock;
		return err;
	}
	if (!m_ttlProbe.openSourceFromSocket(clientSock->fd(), serverSock)) {
		delete clientSock;
		return 215;
	}
	m_sock = clientSock;
	return 0;
}

int SwAkeTcpChannel::listenOn(const SwNetAddress &addr)
{
	close();
	SwTcpServerSocket *srv = new (std::nothrow) SwTcpServerSocket();
	if (!srv)
		return 2;
	int err = srv->listen(addr, 1, g_swAkeListenBacklog);
	if (err != 0) {
		delete srv;
		return err;
	}
	m_ttlProbe.openSource(srv->fd());
	m_sock = srv;
	return 0;
}

int SwAkeTcpChannel::acceptChannel(SwAkeTcpChannel **outChannel)
{
	if (!outChannel)
		return 5;
	*outChannel = NULL;
	if (!m_sock)
		return 4;
	SwTcpServerSocket *srv = static_cast<SwTcpServerSocket *>(m_sock);
	SwTcpSocket *clientSock = NULL;
	int err = srv->accept(&clientSock);
	if (err != 0)
		return err;
	if (!clientSock)
		return 204;

	SwAkeTcpChannel *ch = new (std::nothrow) SwAkeTcpChannel();
	if (!ch) {
		delete clientSock;
		return 2;
	}
	err = ch->attachAccepted(clientSock, *srv);
	if (err != 0) {
		delete ch;
		return err;
	}
	*outChannel = ch;
	return 0;
}

int SwAkeTcpChannel::waitReadable(int timeoutMs)
{
	if (!m_sock)
		return 4;
	return m_sock->waitReadable(timeoutMs, &m_cancelFlag) ? 0 : 9;
}

int SwAkeTcpChannel::sendPacket(SwAkePacket *pkt)
{
	if (!pkt)
		return 5;
	if (!m_sock)
		return 4;

	uint8_t *wire = NULL;
	uint32_t len = 0;
	int err = pkt->serialize(&wire, &len);
	if (err != 0)
		return err;

	uint32_t sentTotal = 0;
	while (sentTotal < len) {
		if (m_cancelFlag)
			return 1024;
		if (!m_sock->waitWritable(5000, &m_cancelFlag))
			return 207;
		int n = m_sock->send(wire + sentTotal, len - sentTotal);
		if (n <= 0)
			return 207;
		sentTotal += (uint32_t)n;
	}
	return 0;
}

int SwAkeTcpChannel::fillBuffer(int timeoutMs)
{
	if (!m_sock)
		return 4;
	if (m_bufUsed >= sizeof(m_rxBuf))
		return 0;
	if (!m_sock->waitReadable(timeoutMs, &m_cancelFlag))
		return m_cancelFlag ? 1024 : 9;
	int n = m_sock->recv(m_rxBuf + m_bufUsed, sizeof(m_rxBuf) - m_bufUsed);
	if (n <= 0)
		return 208;
	m_bufUsed += (uint32_t)n;
	return 0;
}

int SwAkeTcpChannel::recvPacket(SwAkePacket **outPkt, int timeoutMs)
{
	if (!outPkt)
		return 5;
	*outPkt = NULL;
	if (!m_sock)
		return 4;

	while (!hasHeader()) {
		int err = fillBuffer(timeoutMs);
		if (err != 0)
			return err;
	}

	uint16_t payloadLenBe = 0;
	memcpy(&payloadLenBe, m_rxBuf + 1, 2);
	uint16_t payloadLen = swSwap16(payloadLenBe);
	uint32_t frameLen = (uint32_t)payloadLen + 3u;
	if (frameLen > sizeof(m_rxBuf))
		return 1006;

	while (!hasFullFrame(payloadLen)) {
		int err = fillBuffer(timeoutMs);
		if (err != 0)
			return err;
	}

	int err = SwAkePacket::parseFrame(outPkt, m_rxBuf, frameLen);
	uint32_t remain = m_bufUsed - frameLen;
	if (remain > 0)
		memmove(m_rxBuf, m_rxBuf + frameLen, remain);
	m_bufUsed = remain;
	return err;
}

int SwAkeTcpChannel::getPeerTtl()
{
	if (!m_ttlEnabled)
		return 1;
	return m_ttlProbe.getPeerTtl();
}

int SwAkeTcpChannel::getPeerAddress(SwNetAddress &out)
{
	if (!m_sock)
		return 4;
	return m_sock->getPeerAddress(out);
}

void SwAkeTcpChannel::cancel()
{
	m_cancelFlag = 1;
}

void SwAkeTcpChannel::close()
{
	m_ttlProbe.close();
	if (m_sock) {
		m_sock->close();
		delete m_sock;
		m_sock = NULL;
	}
	m_bufUsed = 0;
	m_lastErr = 0;
	m_cancelFlag = 0;
}

/* ---- SwAkeSessionList ---- */

SwAkeSessionList::SwAkeSessionList()
	: m_entries(), m_mutex()
{
}

SwAkeSessionList::~SwAkeSessionList()
{
	clearAll();
}

int SwAkeSessionList::add(SwAkeSessionEntry *entry)
{
	if (!entry)
		return 5;
	SwScopedLock lock(m_mutex);
	return m_entries.pushBackPtr(entry);
}

int SwAkeSessionList::removeById(uint32_t sessionId)
{
	SwScopedLock lock(m_mutex);
	for (SwListIterator it = m_entries.begin(); it != m_entries.end(); ++it) {
		SwAkeSessionEntry *e = SwPtrList<SwAkeSessionEntry>::deref(it);
		if (e && e->sessionId == sessionId) {
			delete e;
			m_entries.erase(it);
			return 0;
		}
	}
	return 100;
}

SwAkeSessionEntry *SwAkeSessionList::findByAddr(const SwNetAddress &addr)
{
	SwScopedLock lock(m_mutex);
	SwString target;
	addr.toString(target);
	for (SwListIterator it = m_entries.begin(); it != m_entries.end(); ++it) {
		SwAkeSessionEntry *e = SwPtrList<SwAkeSessionEntry>::deref(it);
		if (e) {
			SwString cur;
			e->peerAddr.toString(cur);
			if (cur.compare(target) == 0)
				return e;
		}
	}
	return NULL;
}

SwAkeSessionEntry *SwAkeSessionList::findById(uint32_t sessionId)
{
	SwScopedLock lock(m_mutex);
	for (SwListIterator it = m_entries.begin(); it != m_entries.end(); ++it) {
		SwAkeSessionEntry *e = SwPtrList<SwAkeSessionEntry>::deref(it);
		if (e && e->sessionId == sessionId)
			return e;
	}
	return NULL;
}

SwAkeSessionEntry *SwAkeSessionList::findByLabel(uint8_t label)
{
	SwScopedLock lock(m_mutex);
	for (SwListIterator it = m_entries.begin(); it != m_entries.end(); ++it) {
		SwAkeSessionEntry *e = SwPtrList<SwAkeSessionEntry>::deref(it);
		if (e && e->akeLabel == label)
			return e;
	}
	return NULL;
}

void SwAkeSessionList::clearAll()
{
	SwScopedLock lock(m_mutex);
	for (SwListIterator it = m_entries.begin(); it != m_entries.end(); ++it)
		delete SwPtrList<SwAkeSessionEntry>::deref(it);
	m_entries.clear();
}
