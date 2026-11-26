/*
 * rudp_context.cpp: RUDP context lifecycle, blocking waits, cookies, and RudpContextManager.
 */
#include "rudp_internal.h"

int rudpCompareEndpoint(const void *lhs, const void *rhs)
{
	const RudpEndpoint *a = (const RudpEndpoint *)lhs;
	const RudpEndpoint *b = (const RudpEndpoint *)rhs;
	if (rudpSockaddrLess(&a->addr, &b->addr) != 0) {
		return 1;
	}
	if (rudpSockaddrEqual(&a->addr, &b->addr) != 0 && a->vport < b->vport) {
		return 1;
	}
	return 0;
}

int rudpCompareU64(const void *lhs, const void *rhs)
{
	return (*(const uint64_t *)lhs < *(const uint64_t *)rhs) ? 1 : 0;
}

static void destroyPendingChildTree(RudpRbTree *tree)
{
	RudpPendingChildNode *chain = 0;
	while (true) {
		RudpPendingChildNode *node = (RudpPendingChildNode *)tree->next(chain, 0);
		if (node == 0) {
			break;
		}
		node->m_cleanupNext = chain;
		chain = node;
	}
	tree->clear();
	while (chain != 0) {
		RudpPendingChildNode *cur = chain;
		chain = cur->m_cleanupNext;
		delete cur;
	}
}

static void destroyEndpointContextTree(RudpRbTree *tree)
{
	RudpEndpointContextNode *chain = 0;
	while (true) {
		RudpEndpointContextNode *node = (RudpEndpointContextNode *)tree->next(chain, 0);
		if (node == 0) {
			break;
		}
		node->m_cleanupNext = chain;
		chain = node;
	}
	tree->clear();
	while (chain != 0) {
		RudpEndpointContextNode *cur = chain;
		chain = cur->m_cleanupNext;
		delete cur;
	}
}

static void destroyContextMapTree(RudpRbTree *tree)
{
	RudpContextMapNode *chain = 0;
	while (true) {
		RudpContextMapNode *node = (RudpContextMapNode *)tree->next(chain, 0);
		if (node == 0) {
			break;
		}
		node->m_cleanupNext = chain;
		chain = node;
	}
	tree->clear();
	while (chain != 0) {
		RudpContextMapNode *cur = chain;
		chain = cur->m_cleanupNext;
		delete cur;
	}
}

int RudpContext::mapReasonToErrorCode(int reason)
{
	switch (reason) {
	case 0: return 0;
	case 1: return SCE_RUDP_ERROR_CONN_RESET;
	case 2: return SCE_RUDP_ERROR_CONN_REFUSED;
	case 3: return SCE_RUDP_ERROR_CONN_TIMEOUT;
	case 4: return SCE_RUDP_ERROR_CONN_VERSION_MISMATCH;
	case 5: return SCE_RUDP_ERROR_CONN_TRANSPORT_TYPE_MISMATCH;
	case 6: return SCE_RUDP_ERROR_CONN_QUALITY_LEVEL_MISMATCH;
	case 8: return SCE_RUDP_ERROR_KEEP_ALIVE_FAILURE;
	default: return SCE_RUDP_ERROR_INTERNAL;
	}
}

RudpContext::RudpContext(RudpScheduler *scheduler, int ctxId)
	: m_scheduler(scheduler),
	  m_delegate(0),
	  m_ctxId(ctxId),
	  m_handlerId(0),
	  m_eventHandler(0),
	  m_eventHandlerArg(0),
	  m_protocol(),
	  m_blocking(false),
	  m_waiterCount(0),
	  m_sock(-1),
	  m_parent(0),
	  m_isChild(false),
	  m_childrenByEndpoint(rudpCompareEndpoint),
	  m_pendingChildren(rudpCompareU64),
	  m_backlog(0),
	  m_cookieTimeoutSec(RUDP_DEFAULT_COOKIE_LIFETIME_SEC),
	  m_cookieChallengeQueue(0),
	  m_cookieSecret(0),
	  m_muxMode(0),
	  m_pollId(-1),
	  m_lastError(0),
	  m_readTimeoutMs(0),
	  m_writeTimeoutMs(0),
	  m_flushTimeoutMs(0),
	  m_contextFlags(0),
	  m_waitFlags(0),
	  m_magic(RUDP_CONTEXT_MAGIC)
{
	m_localEndpoint.vport = 0;
	rudpMemset(&m_localEndpoint.addr, 0, sizeof(m_localEndpoint.addr));
	m_remoteEndpoint.vport = 0;
	rudpMemset(&m_remoteEndpoint.addr, 0, sizeof(m_remoteEndpoint.addr));

	rudpCondInit(&m_waitCond, &g_rudpMutex);
	m_protocol.setDelegate(this);

	RudpResult regRes = m_scheduler->registerHandler(&m_handlerId, this);
	int regErr = regRes.getCode();
	if (regErr >= 0) {
		RudpResult timerRes = m_scheduler->startTimer(m_handlerId, RUDP_DEFAULT_TIMER_INTERVAL_USEC);
		int timerErr = timerRes.getCode();
		if (timerErr < 0) {
			if (timerErr == SCE_RUDP_ERROR_MEMORY) {
				RudpEvent ev;
				ev.handlerId = m_handlerId;
				ev.type = RUDP_EV_OUT_OF_MEMORY;
				ev.detail = 0;
				ev.arg = 0;
				RudpResult postRes = m_scheduler->postEvent(&ev);
				postRes.getCode();
			} else {
				timerErr = SCE_RUDP_ERROR_INTERNAL;
			}
			m_lastError = timerErr;
		}
	} else {
		m_handlerId = 0;
		if (regErr != SCE_RUDP_ERROR_MEMORY) {
			regErr = SCE_RUDP_ERROR_INTERNAL;
		}
		m_lastError = regErr;
	}
}

void RudpContext::detachChild(const RudpEndpoint *ep)
{
	if (m_magic != RUDP_CONTEXT_MAGIC) {
		return;
	}
	for (RudpPendingChildNode *node = (RudpPendingChildNode *)m_pendingChildren.first(0);
	     node != 0;
	     node = (RudpPendingChildNode *)m_pendingChildren.next(node, 0)) {
		RudpContext *child = node->m_context;
		if (rudpSockaddrEqual(&child->m_remoteEndpoint.addr, &ep->addr) != 0 &&
		    child->m_remoteEndpoint.vport == ep->vport) {
			m_pendingChildren.remove(node);
			delete node;
			break;
		}
	}
	RudpEndpointContextNode *epNode = (RudpEndpointContextNode *)m_childrenByEndpoint.find(ep);
	if (epNode != 0) {
		m_childrenByEndpoint.remove(epNode);
		delete epNode;
	}
	if ((m_contextFlags & RUDP_CTX_FLAG_TERMINATING) != 0 &&
	    m_pendingChildren.getCount() == 0 &&
	    m_childrenByEndpoint.getCount() == 0) {
		RudpEvent ev;
		ev.handlerId = m_handlerId;
		ev.type = RUDP_EV_CLOSED;
		ev.detail = 0;
		ev.arg = 0;
		RudpResult postRes = m_scheduler->postEvent(&ev);
		postRes.getCode();
	}
}

RudpContext::~RudpContext()
{
	if (m_waiterCount > 0) {
		do {
			m_waitFlags |= RUDP_WAIT_CANCELLED;
			rudpCondBroadcast(&m_waitCond);
			rudpCondWait(&m_waitCond, &g_rudpMutex);
		} while (m_waiterCount > 0);
		m_waitFlags &= ~RUDP_WAIT_CANCELLED;
	}
	if (m_magic == RUDP_CONTEXT_MAGIC && m_parent != 0) {
		m_parent->detachChild(&m_remoteEndpoint);
		m_parent = 0;
	}
	if (m_handlerId != 0) {
		RudpResult stopRes = m_scheduler->stopTimer(m_handlerId);
		stopRes.getCode();
		RudpResult unregRes = m_scheduler->unregisterHandler(m_handlerId);
		unregRes.getCode();
	}
	m_protocol.setDelegate(0);
	if ((m_contextFlags & RUDP_CTX_FLAG_REMOTE_BOUND) != 0) {
		m_contextFlags &= ~RUDP_CTX_FLAG_REMOTE_BOUND;
		if (m_delegate != 0) {
			m_delegate->onContextUnregisterAggregator(this, m_sock, &m_remoteEndpoint.addr, m_ctxId);
		}
	}
	if (m_cookieChallengeQueue != 0) {
		if (m_cookieChallengeQueue->m_items != 0) {
			rudpFree(m_cookieChallengeQueue->m_items);
		}
		rudpFree(m_cookieChallengeQueue);
		m_cookieChallengeQueue = 0;
	}
	rudpCondTerm(&m_waitCond);
	m_magic = 0;
	destroyPendingChildTree(&m_pendingChildren);
	destroyEndpointContextTree(&m_childrenByEndpoint);
}

void RudpContext::setEventHandler(SceRudpContextEventHandler handler, void *arg)
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		m_eventHandler = handler;
		m_eventHandlerArg = arg;
	}
}

void RudpContext::noopHelper()
{
}

int RudpContext::getLastError()
{
	if (m_magic != RUDP_CONTEXT_MAGIC) {
		return 0;
	}
	int err = m_lastError;
	m_lastError = 0;
	return err;
}

RudpResult RudpContext::setMaxSegmentSize(uint32_t mss)
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.setMaxSegmentSize(mss);
	}
	return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
}

uint32_t RudpContext::getMaxSegmentSize() const
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.getMaxSegmentSize();
	}
	return 0;
}

RudpResult RudpContext::setSendBufferSize(uint32_t size)
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.setSendBufferSize(size);
	}
	return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
}

uint32_t RudpContext::getSendBufferSize() const
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.getSendBufferSize();
	}
	return 0;
}

RudpResult RudpContext::setRecvBufferSize(uint32_t size)
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.setRecvBufferSize(size);
	}
	return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
}

uint32_t RudpContext::getRecvBufferSize() const
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.getRecvBufferSize();
	}
	return 0;
}

RudpResult RudpContext::setConnectionTimeout(uint32_t ms)
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.setConnectionTimeout(ms);
	}
	return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
}

uint32_t RudpContext::getConnectionTimeout() const
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.getConnectionTimeout();
	}
	return 0;
}

RudpResult RudpContext::setCloseWaitTimeout(uint32_t ms)
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.setCloseWaitTimeout(ms);
	}
	return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
}

uint32_t RudpContext::getCloseWaitTimeout() const
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.getCloseWaitTimeout();
	}
	return 0;
}

RudpResult RudpContext::setAggregationTimeout(uint32_t ms)
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.setAggregationTimeout(ms);
	}
	return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
}

uint32_t RudpContext::getAggregationTimeout() const
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.getAggregationTimeout();
	}
	return 0;
}

RudpResult RudpContext::setNonBlock(bool nonBlock)
{
	if (m_magic != RUDP_CONTEXT_MAGIC) {
		return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
	}
	if (m_blocking) {
		if (nonBlock) {
			m_blocking = false;
			m_waitFlags |= RUDP_WAIT_CANCELLED;
			rudpCondBroadcast(&m_waitCond);
		}
	} else if (!nonBlock) {
		m_blocking = true;
	}
	return RudpResult(0);
}

bool RudpContext::getNonBlock() const
{
	return (m_magic == RUDP_CONTEXT_MAGIC) && !m_blocking;
}

RudpResult RudpContext::setStreamMode(int enable)
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.setStreamMode(enable);
	}
	return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
}

bool RudpContext::getStreamMode() const
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.getStreamMode();
	}
	return false;
}

RudpResult RudpContext::setDeliveryCritical(uint8_t enable)
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.setDeliveryCritical(enable);
	}
	return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
}

bool RudpContext::getDeliveryCritical() const
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.getDeliveryCritical();
	}
	return true;
}

RudpResult RudpContext::setOrderCritical(uint8_t enable)
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.setOrderCritical(enable);
	}
	return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
}

bool RudpContext::getOrderCritical() const
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.getOrderCritical();
	}
	return true;
}

RudpResult RudpContext::setNoDelay(uint8_t enable)
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.setNoDelay(enable);
	}
	return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
}

bool RudpContext::getNoDelay() const
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.getNoDelay();
	}
	return false;
}

RudpResult RudpContext::setSackEnabled(uint8_t enable)
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.setSackEnabled(enable);
	}
	return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
}

bool RudpContext::getSackEnabled() const
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.getSackEnabled();
	}
	return true;
}

RudpResult RudpContext::setCookieTimeout(uint32_t seconds)
{
	if (m_magic != RUDP_CONTEXT_MAGIC) {
		return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
	}
	m_cookieTimeoutSec = seconds;
	return RudpResult(0);
}

uint32_t RudpContext::getCookieTimeout() const
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_cookieTimeoutSec;
	}
	return 0;
}

RudpResult RudpContext::setKeepAliveInterval(uint32_t ms)
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.setKeepAliveInterval(ms);
	}
	return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
}

uint32_t RudpContext::getKeepAliveInterval() const
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.getKeepAliveInterval();
	}
	return 0;
}

RudpResult RudpContext::setKeepAliveTimeout(uint32_t ms)
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.setKeepAliveTimeout(ms);
	}
	return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
}

uint32_t RudpContext::getKeepAliveTimeout() const
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.getKeepAliveTimeout();
	}
	return 0;
}

void RudpContext::setDebugFlag(uint8_t enable)
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		m_protocol.setDebugFlag(enable);
	}
}

RudpResult RudpContext::bind(int sock, uint16_t vport, uint8_t muxMode)
{
	if (m_magic != RUDP_CONTEXT_MAGIC) {
		return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
	}
	RudpResult res(0);
	if ((m_contextFlags & RUDP_CTX_FLAG_LISTENING) != 0 || m_protocol.m_state != SCE_RUDP_STATE_IDLE) {
		res = SCE_RUDP_ERROR_NOT_ACCEPTABLE;
	} else if (m_sock == -1) {
		m_sock = sock;
		m_localEndpoint.vport = vport;
		m_muxMode = muxMode;
	} else {
		res = SCE_RUDP_ERROR_ALREADY_BOUND;
	}
	return res;
}

int RudpContext::waitBlocking(uint8_t *ioMask, uint32_t timeoutMs)
{
	uint64_t timeoutUsec = (uint64_t)timeoutMs * 1000ULL;
	m_waiterCount++;
	uint8_t mask = *ioMask;
	int err = 0;

	while ((m_waitFlags & RUDP_WAIT_CANCELLED) == 0 && (m_waitFlags & mask) == 0) {
		if (timeoutMs != 0) {
			int wret = rudpCondTimedWait(&m_waitCond, &g_rudpMutex, timeoutUsec);
			if (wret < 0) {
				err = (wret == RUDP_OSAL_ETIMEDOUT) ? SCE_RUDP_ERROR_WOULDBLOCK : SCE_RUDP_ERROR_INTERNAL;
				break;
			}
		} else {
			rudpCondWait(&m_waitCond, &g_rudpMutex);
		}
	}

	*ioMask = (uint8_t)(m_waitFlags & (mask | RUDP_WAIT_CANCELLED));
	m_waitFlags &= (uint8_t)(~mask);
	m_waiterCount--;
	if (m_waiterCount == 0 && (m_waitFlags & RUDP_WAIT_CANCELLED) != 0) {
		rudpCondBroadcast(&m_waitCond);
	}
	return err;
}

RudpResult RudpContext::activate()
{
	if (m_magic != RUDP_CONTEXT_MAGIC) {
		return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
	}
	RudpResult res(0);
	m_lastError = 0;
	RudpResult actRes = m_protocol.activate();
	res = actRes;
	if ((res.getCode() == 0 || res.getCode() == SCE_RUDP_ERROR_IN_PROGRESS) && m_blocking) {
		uint8_t waitMask = RUDP_WAIT_ESTABLISHED | RUDP_WAIT_CLOSED;
		res = waitBlocking(&waitMask, 0);
		if (res.getCode() >= 0) {
			if ((waitMask & RUDP_WAIT_CLOSED) != 0) {
				res = m_lastError;
				m_lastError = 0;
			} else if ((waitMask & RUDP_WAIT_CANCELLED) != 0) {
				res = SCE_RUDP_ERROR_CANCELLED;
			}
		}
	}
	return res;
}

RudpResult RudpContext::initiate()
{
	if (m_magic != RUDP_CONTEXT_MAGIC) {
		return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
	}
	m_lastError = 0;
	RudpResult initRes = m_protocol.initiate();
	int err = initRes.getCode();
	if ((err == 0 || err == SCE_RUDP_ERROR_IN_PROGRESS) && m_blocking) {
		uint8_t waitMask = RUDP_WAIT_ESTABLISHED | RUDP_WAIT_CLOSED;
		waitBlocking(&waitMask, 0);
		if ((waitMask & RUDP_WAIT_CLOSED) != 0) {
			err = m_lastError;
			m_lastError = 0;
		} else if ((waitMask & RUDP_WAIT_CANCELLED) != 0) {
			err = SCE_RUDP_ERROR_CANCELLED;
		}
	}
	return RudpResult(err);
}

RudpResult RudpContext::write(RudpBuffer *buf, uint8_t flags)
{
	if (m_magic != RUDP_CONTEXT_MAGIC) {
		return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
	}
	RudpResult res;
	RudpResult wres = m_protocol.write(buf, flags);
	res = wres;
	if (res.getCode() == SCE_RUDP_ERROR_WOULDBLOCK && m_blocking &&
	    (flags & SCE_RUDP_MSG_DONTWAIT) == 0 && m_protocol.m_state == SCE_RUDP_STATE_ESTABLISHED) {
		do {
			uint8_t waitMask = RUDP_WAIT_WRITABLE | RUDP_WAIT_CLOSED;
			res = waitBlocking(&waitMask, m_writeTimeoutMs);
			if (res.getCode() < 0) {
				break;
			}
			if ((waitMask & RUDP_WAIT_CANCELLED) != 0) {
				res = SCE_RUDP_ERROR_CANCELLED;
				break;
			}
			RudpResult retryRes = m_protocol.write(buf, flags);
			res = retryRes;
		} while (res.getCode() < 0 && res.getCode() == SCE_RUDP_ERROR_WOULDBLOCK &&
		         m_protocol.m_state == SCE_RUDP_STATE_ESTABLISHED);
	}
	return res;
}

RudpResult RudpContext::read(RudpBuffer *buf, uint8_t flags, SceRudpReadInfo *info)
{
	if (m_magic != RUDP_CONTEXT_MAGIC) {
		return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
	}
	RudpResult res;
	RudpResult rres = m_protocol.read(buf, info);
	res = rres;
	if (res.getCode() == SCE_RUDP_ERROR_WOULDBLOCK && m_blocking &&
	    (flags & SCE_RUDP_MSG_DONTWAIT) == 0) {
		do {
			if (m_protocol.m_state != SCE_RUDP_STATE_ESTABLISHED &&
			    m_protocol.m_state != SCE_RUDP_STATE_CLOSE_WAIT) {
				break;
			}
			uint8_t waitMask = RUDP_WAIT_READABLE | RUDP_WAIT_CLOSED;
			res = waitBlocking(&waitMask, m_readTimeoutMs);
			if (res.getCode() < 0) {
				break;
			}
			if ((waitMask & RUDP_WAIT_CANCELLED) != 0) {
				res = SCE_RUDP_ERROR_CANCELLED;
				break;
			}
			RudpResult retryRes = m_protocol.read(buf, info);
			res = retryRes;
		} while (res.getCode() < 0 && res.getCode() == SCE_RUDP_ERROR_WOULDBLOCK);
	}
	return res;
}

RudpResult RudpContext::terminate()
{
	if (m_magic != RUDP_CONTEXT_MAGIC) {
		return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
	}
	RudpResult res(0);
	if ((m_contextFlags & RUDP_CTX_FLAG_TERMINATING) != 0) {
		res = SCE_RUDP_ERROR_INVALID_CONTEXT_ID;
	} else {
		m_contextFlags |= RUDP_CTX_FLAG_TERMINATING;
		if ((m_contextFlags & RUDP_CTX_FLAG_LISTENING) != 0) {
			for (RudpEndpointContextNode *node = (RudpEndpointContextNode *)m_childrenByEndpoint.first(0);
			     node != 0;
			     node = (RudpEndpointContextNode *)m_childrenByEndpoint.next(node, 0)) {
				RudpResult childRes = node->m_context->terminate();
				childRes.getCode();
			}
			for (RudpPendingChildNode *node = (RudpPendingChildNode *)m_pendingChildren.first(0);
			     node != 0;
			     node = (RudpPendingChildNode *)m_pendingChildren.next(node, 0)) {
				RudpResult childRes = node->m_context->terminate();
				childRes.getCode();
				node->m_context->onProtocolClosed(&m_protocol);
			}
			if (m_pendingChildren.getCount() == 0 && m_childrenByEndpoint.getCount() == 0) {
				RudpEvent ev;
				ev.handlerId = m_handlerId;
				ev.type = RUDP_EV_CLOSED;
				ev.detail = 0;
				ev.arg = 0;
				RudpResult postRes = m_scheduler->postEvent(&ev);
				postRes.getCode();
			}
		} else {
			RudpResult termRes = m_protocol.terminate();
			res = termRes;
		}
		if (m_blocking) {
			m_waitFlags |= RUDP_WAIT_CANCELLED;
			rudpCondBroadcast(&m_waitCond);
		}
	}
	return res;
}

RudpResult RudpContext::flush()
{
	if (m_magic != RUDP_CONTEXT_MAGIC) {
		return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
	}
	RudpResult res;
	RudpResult fres = m_protocol.flush();
	res = fres;
	if ((res.getCode() == 0 || res.getCode() == SCE_RUDP_ERROR_IN_PROGRESS) && m_blocking) {
		uint8_t waitMask = RUDP_WAIT_FLUSHED;
		res = waitBlocking(&waitMask, m_flushTimeoutMs);
		if (res.getCode() >= 0 && (waitMask & RUDP_WAIT_CANCELLED) != 0) {
			res = SCE_RUDP_ERROR_CANCELLED;
		}
	}
	return res;
}

int RudpContext::getSizeWritable() const
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.getSizeWritable();
	}
	return 0;
}

int RudpContext::getSizeReadable() const
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.getSizeReadable();
	}
	return 0;
}

uint32_t RudpContext::getPacketsToRead() const
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_protocol.getPacketsToRead();
	}
	return 0;
}

RudpResult RudpContext::getLocalInfo(int *sock, void *addr, uint32_t *addrLen, uint16_t *vport, uint8_t *muxMode)
{
	if (m_magic != RUDP_CONTEXT_MAGIC) {
		return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
	}
	RudpResult res(0);
	if (m_sock == -1) {
		res = SCE_RUDP_ERROR_NOT_BOUND;
	}
	if (res.getCode() == 0) {
		if (sock != 0) {
			*sock = m_sock;
		}
		if (addr != 0 && addrLen != 0) {
			uint8_t localAddrBuf[40];
			rudpMemset(localAddrBuf, 0, sizeof(localAddrBuf));
			if (*addrLen > sizeof(SceNetSockaddr)) {
				*addrLen = sizeof(SceNetSockaddr);
			}
			rudpNetGetsockname(m_sock, (SceNetSockaddr *)localAddrBuf);
			rudpMemcpy(addr, localAddrBuf, *addrLen);
		}
		if (vport != 0) {
			*vport = m_localEndpoint.vport;
		}
		if (muxMode != 0) {
			*muxMode = m_muxMode;
		}
	}
	return res;
}

RudpResult RudpContext::getRemoteInfo(void *addr, int *addrLen, uint16_t *vport)
{
	if (m_magic != RUDP_CONTEXT_MAGIC) {
		return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
	}
	RudpResult res(0);
	if ((m_protocol.m_state != SCE_RUDP_STATE_ESTABLISHED &&
	     m_protocol.m_state != SCE_RUDP_STATE_CLOSE_WAIT) ||
	    (m_contextFlags & RUDP_CTX_FLAG_LISTENING) != 0) {
		res = SCE_RUDP_ERROR_NOT_ACCEPTABLE;
	}
	if (res.getCode() == 0) {
		if (vport != 0) {
			*vport = (m_muxMode == SCE_RUDP_MUXMODE_P2P) ? m_localEndpoint.vport : m_remoteEndpoint.vport;
		}
		if (addr != 0 && addrLen != 0) {
			if ((uint32_t)*addrLen > sizeof(SceNetSockaddr)) {
				*addrLen = sizeof(SceNetSockaddr);
			}
			rudpMemcpy(addr, &m_remoteEndpoint.addr, (SceSize)*addrLen);
		}
	}
	return res;
}

RudpResult RudpContext::setReadTimeout(uint32_t ms)
{
	if (m_magic != RUDP_CONTEXT_MAGIC) {
		return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
	}
	m_readTimeoutMs = ms;
	return RudpResult(0);
}

RudpResult RudpContext::getReadTimeout(uint32_t *outMs)
{
	*outMs = m_readTimeoutMs;
	return RudpResult(0);
}

RudpResult RudpContext::setWriteTimeout(uint32_t ms)
{
	if (m_magic != RUDP_CONTEXT_MAGIC) {
		return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
	}
	m_writeTimeoutMs = ms;
	return RudpResult(0);
}

RudpResult RudpContext::getWriteTimeout(uint32_t *outMs)
{
	*outMs = m_writeTimeoutMs;
	return RudpResult(0);
}

RudpResult RudpContext::setFlushTimeout(uint32_t ms)
{
	if (m_magic != RUDP_CONTEXT_MAGIC) {
		return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
	}
	m_flushTimeoutMs = ms;
	return RudpResult(0);
}

RudpResult RudpContext::getFlushTimeout(uint32_t *outMs)
{
	*outMs = m_flushTimeoutMs;
	return RudpResult(0);
}

RudpResult RudpContext::setRemoteEndpointInternal(const RudpSockaddr *addr, uint16_t vport)
{
	RudpResult res(0);
	if (m_muxMode == SCE_RUDP_MUXMODE_CLIENT_SERVER && (m_contextFlags & RUDP_CTX_FLAG_LISTENING) != 0) {
		res = SCE_RUDP_ERROR_NOT_ACCEPTABLE;
		return res;
	}
	if (m_muxMode != SCE_RUDP_MUXMODE_CLIENT_SERVER) {
		vport = m_localEndpoint.vport;
	}
	if ((m_contextFlags & RUDP_CTX_FLAG_REMOTE_BOUND) == 0) {
		m_contextFlags |= RUDP_CTX_FLAG_REMOTE_BOUND;
		rudpMemset(&m_remoteEndpoint.addr, 0, sizeof(m_remoteEndpoint.addr));
		rudpMemcpy(&m_remoteEndpoint.addr, addr, addr->sa_len);
		m_remoteEndpoint.vport = vport;
		if (m_delegate != 0) {
			RudpResult regRes = m_delegate->onContextRegisterAggregator(
				this, m_sock, &m_remoteEndpoint.addr, m_ctxId, m_protocol.getAggregationTimeout());
			res = regRes;
		}
		return res;
	}
	if (rudpSockaddrEqual(addr, &m_remoteEndpoint.addr) != 0 && m_remoteEndpoint.vport == vport) {
		return RudpResult(0);
	}
	return RudpResult(SCE_RUDP_ERROR_IN_PROGRESS);
}

RudpResult RudpContext::setRemoteEndpoint(const RudpSockaddr *addr, uint16_t vport)
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return setRemoteEndpointInternal(addr, vport);
	}
	return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
}

uint32_t RudpContext::getState(bool *outTerminating) const
{
	if (m_magic != RUDP_CONTEXT_MAGIC) {
		return SCE_RUDP_STATE_IDLE;
	}
	uint32_t state;
	if ((m_contextFlags & RUDP_CTX_FLAG_LISTENING) != 0) {
		state = SCE_RUDP_STATE_LISTEN;
	} else {
		switch (m_protocol.m_state) {
		case SCE_RUDP_STATE_IDLE:        state = SCE_RUDP_STATE_IDLE; break;
		case SCE_RUDP_STATE_CLOSED:      state = SCE_RUDP_STATE_CLOSED; break;
		case SCE_RUDP_STATE_SYN_SENT:    state = SCE_RUDP_STATE_SYN_SENT; break;
		case SCE_RUDP_STATE_SYN_RCVD:    state = SCE_RUDP_STATE_SYN_RCVD; break;
		case SCE_RUDP_STATE_ESTABLISHED: state = SCE_RUDP_STATE_ESTABLISHED; break;
		case SCE_RUDP_STATE_CLOSE_WAIT:  state = SCE_RUDP_STATE_CLOSE_WAIT; break;
		default:                         state = SCE_RUDP_STATE_IDLE; break;
		}
	}
	if (outTerminating != 0) {
		*outTerminating = (m_contextFlags & RUDP_CTX_FLAG_TERMINATING) != 0;
	}
	return state;
}

int RudpContext::verifyCookie(const uint8_t *cookie)
{
	uint32_t secretNet = sceNetNtohl(m_cookieSecret);
	uint32_t nowMs = (uint32_t)(rudpGetTimeUsec() / 1000ULL);
	uint32_t cookieTimeNet;
	uint8_t nonce[4];
	rudpMemcpy(&cookieTimeNet, cookie, 4);
	rudpMemcpy(nonce, cookie + 4, 4);
	uint32_t cookieTimeMs = sceNetNtohl(cookieTimeNet);
	if ((int32_t)(nowMs - (cookieTimeMs + m_cookieTimeoutSec * 1000U)) >= 0) {
		return 2;
	}
	SceSha256Context sha;
	uint8_t digest[32];
	rudpSha256Init(&sha);
	rudpSha256Update(&sha, &cookieTimeNet, 4);
	rudpSha256Update(&sha, nonce, 4);
	rudpSha256Update(&sha, &secretNet, 4);
	rudpSha256Final(&sha, digest);
	return (rudpMemcmp(cookie + 8, digest + 8, 24) != 0) ? 1 : 0;
}

static inline void enqueueCookieChallenge(RudpContext *ctx, const RudpSockaddr *from, uint16_t srcVport)
{
	RudpEndpoint ep;
	ep.vport = 0;
	rudpMemset(&ep.addr, 0, sizeof(ep.addr));
	ep.addr = *from;
	ep.vport = srcVport;

	RudpRingBuffer<RudpEndpoint> *q = ctx->m_cookieChallengeQueue;
	if (q->m_items != 0 && q->m_capacity != 0 && (q->m_tail != q->m_head || !q->m_full)) {
		q->m_full = true;
		q->m_items[q->m_tail] = ep;
		q->m_tail++;
		if (q->m_tail == q->m_capacity) {
			q->m_tail = 0;
		}
		RudpEvent ev;
		ev.handlerId = ctx->m_handlerId;
		ev.type = RUDP_EV_SEND_COOKIE;
		ev.detail = 0;
		ev.arg = 0;
		RudpResult postRes = ctx->m_scheduler->postEvent(&ev);
		postRes.getCode();
	}
}

RudpResult RudpContext::onPacketReceived(
	RudpBuffer *buf,
	uint8_t muxMode,
	uint16_t dstVport,
	uint16_t srcVport,
	const RudpSockaddr *from)
{
	if (m_magic != RUDP_CONTEXT_MAGIC) {
		return RudpResult(SCE_RUDP_ERROR_INVALID_CONTEXT_ID);
	}
	RudpResult res(0);
	bool activeListen = ((m_contextFlags & RUDP_CTX_FLAG_LISTENING) != 0 &&
	                     (m_contextFlags & RUDP_CTX_FLAG_TERMINATING) == 0);
	bool activeConn = ((m_contextFlags & RUDP_CTX_FLAG_LISTENING) == 0 &&
	                   m_protocol.m_state != SCE_RUDP_STATE_IDLE);
	if (!activeListen && !activeConn) {
		return res;
	}

	if (muxMode <= SCE_RUDP_MUXMODE_P2P) {
		RudpResult rxRes = m_protocol.onPacketReceived(buf);
		res = rxRes;
		return res;
	}

	if (muxMode != SCE_RUDP_MUXMODE_CLIENT_SERVER || srcVport == 0 || from == 0 || from->sa_len == 0) {
		return res;
	}

	if ((m_contextFlags & RUDP_CTX_FLAG_LISTENING) == 0) {
		if ((m_contextFlags & RUDP_CTX_FLAG_REMOTE_BOUND) != 0 &&
		    (m_protocol.m_state == SCE_RUDP_STATE_CLOSED ||
		     rudpSockaddrEqual(&m_remoteEndpoint.addr, from) != 0)) {
			RudpResult rxRes = m_protocol.onPacketReceived(buf);
			res = rxRes;
			if (res.getCode() == 0 && (m_contextFlags & RUDP_CTX_FLAG_REMOTE_BOUND) == 0) {
				RudpResult epRes = setRemoteEndpoint(from, srcVport);
				res = epRes;
				res.getCode();
			}
		}
		return res;
	}

	RudpEndpoint keyEp;
	keyEp.vport = 0;
	rudpMemset(&keyEp.addr, 0, sizeof(keyEp.addr));
	keyEp.addr = *from;
	keyEp.vport = srcVport;

	RudpEndpointContextNode *existing = (RudpEndpointContextNode *)m_childrenByEndpoint.find(&keyEp);
	if (existing != 0) {
		RudpResult childRes = existing->m_context->onPacketReceived(buf, muxMode, dstVport, srcVport, from);
		childRes.getCode();
		return res;
	}

	for (RudpPendingChildNode *node = (RudpPendingChildNode *)m_pendingChildren.first(0);
	     node != 0;
	     node = (RudpPendingChildNode *)m_pendingChildren.next(node, 0)) {
		RudpContext *child = node->m_context;
		if (rudpSockaddrEqual(&child->m_remoteEndpoint.addr, &keyEp.addr) != 0 &&
		    child->m_remoteEndpoint.vport == keyEp.vport) {
			if (child != 0) {
				RudpResult childRes = child->onPacketReceived(buf, muxMode, dstVport, srcVport, from);
				childRes.getCode();
			}
			return res;
		}
	}

	RudpHeader hdr;
	RudpBuffer peekBuf(buf->m_data + buf->m_offset, buf->m_length);
	peekBuf.setLength(buf->m_length);
	bool isSyn = false;
	RudpResult parseRes = hdr.parse(&peekBuf);
	if (parseRes.getCode() == 0 && hdr.m_type == RUDP_PKT_SYN) {
		isSyn = true;
	}

	if (!isSyn || m_pendingChildren.getCount() >= m_backlog) {
		return res;
	}

	if (m_cookieTimeoutSec != 0) {
		if (m_cookieChallengeQueue == 0) {
			RudpRingBuffer<RudpEndpoint> *q =
				(RudpRingBuffer<RudpEndpoint> *)rudpAlloc(sizeof(RudpRingBuffer<RudpEndpoint>));
			if (q != 0) {
				q->m_items = 0;
				q->m_capacity = 64;
				q->m_head = 0;
				q->m_tail = 0;
				q->m_full = false;
				q->m_items = (RudpEndpoint *)rudpAlloc(64 * sizeof(RudpEndpoint));
			}
			m_cookieChallengeQueue = q;
			if (q == 0) {
				res = SCE_RUDP_ERROR_MEMORY;
				return res;
			}
			if (q->m_items == 0 || q->m_capacity == 0) {
				res = SCE_RUDP_ERROR_MEMORY;
				if (q->m_items != 0) {
					rudpFree(q->m_items);
				}
				rudpFree(q);
				m_cookieChallengeQueue = 0;
				return res;
			}
		}

		if ((hdr.m_extFlags & RUDP_EXT_COOKIE) == 0) {
			enqueueCookieChallenge(this, from, srcVport);
			return res;
		}

		const uint8_t *cookiePtr = 0;
		uint32_t cookieLen = 0;
		hdr.getCookie(&cookiePtr, &cookieLen);
		if (cookieLen != RUDP_MAX_COOKIE_SIZE) {
			return res;
		}
		int verifyRet = verifyCookie(cookiePtr);
		if (verifyRet != 0) {
			if (verifyRet == 2 && m_pendingChildren.getCount() < m_backlog) {
				enqueueCookieChallenge(this, from, srcVport);
			}
			return res;
		}
	}

	RudpContext *child = 0;
	RudpResult createRes = g_rudpContextManager->createContext(&child, m_delegate);
	res = createRes;
	if (res.getCode() == 0) {
		child->m_parent = this;
		child->m_isChild = true;
		child->setMaxSegmentSize(m_protocol.getMaxSegmentSize());
		child->setSendBufferSize(m_protocol.getSendBufferSize());
		child->setRecvBufferSize(m_protocol.getRecvBufferSize());
		child->setConnectionTimeout(m_protocol.getConnectionTimeout());
		child->setCloseWaitTimeout(m_protocol.getCloseWaitTimeout());
		child->setAggregationTimeout(m_protocol.getAggregationTimeout());
		child->setNonBlock(true);
		child->setStreamMode(m_protocol.getStreamMode() ? 1 : 0);
		child->setDeliveryCritical(m_protocol.getDeliveryCritical() ? 1 : 0);
		child->setOrderCritical(m_protocol.getOrderCritical() ? 1 : 0);
		child->setNoDelay(m_protocol.getNoDelay() ? 1 : 0);
		child->setSackEnabled(m_protocol.getSackEnabled() ? 1 : 0);
		child->setKeepAliveInterval(m_protocol.getKeepAliveInterval());
		child->setKeepAliveTimeout(m_protocol.getKeepAliveTimeout());
		child->setDebugFlag(m_protocol.getDebugFlag() ? 1 : 0);
		child->bind(m_sock, m_localEndpoint.vport, m_muxMode);
		child->setRemoteEndpoint(from, srcVport);
		child->activate();
		if (m_blocking) {
			child->setNonBlock(false);
		}
		RudpResult rxRes = child->onPacketReceived(buf, muxMode, dstVport, srcVport, from);
		if (rxRes.getCode() == 0) {
			RudpPendingChildNode *pnode = new RudpPendingChildNode();
			if (pnode != 0) {
				pnode->m_cleanupNext = 0;
				pnode->m_timestamp = rudpGetTimeUsec();
				pnode->m_context = child;
				m_pendingChildren.insert(pnode);
			}
		} else {
			g_rudpContextManager->destroyContext(child->m_ctxId);
		}
	}
	return res;
}

void RudpContext::postSocketError(int errCode)
{
	RudpEvent ev;
	ev.handlerId = m_handlerId;
	ev.type = RUDP_EV_SOCKET_ERROR;
	ev.detail = 0;
	ev.arg = errCode;
	RudpResult res = m_scheduler->postEvent(&ev);
	res.getCode();
}

void RudpContext::getProtocolStats(RudpProtocolStats *outStats) const
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		m_protocol.getStats(outStats);
	}
}

RudpResult RudpContext::attachPoll(int pollId)
{
	RudpResult res(0);
	if (m_pollId > 0) {
		res = SCE_RUDP_ERROR_ALREADY_EXISTS;
	} else {
		m_pollId = pollId;
	}
	return res;
}

int RudpContext::getPollId() const
{
	return m_pollId;
}

void RudpContext::detachPoll()
{
	m_pollId = -1;
}

uint16_t RudpContext::computePollEvents() const
{
	uint16_t events = 0;
	if ((m_contextFlags & RUDP_CTX_FLAG_LISTENING) != 0) {
		for (RudpPendingChildNode *node = (RudpPendingChildNode *)m_pendingChildren.first(0);
		     node != 0;
		     node = (RudpPendingChildNode *)m_pendingChildren.next(node, 0)) {
			if (node->m_context->m_protocol.m_state == SCE_RUDP_STATE_ESTABLISHED) {
				return SCE_RUDP_POLL_EV_READ;
			}
		}
	} else if (m_protocol.m_state == SCE_RUDP_STATE_ESTABLISHED) {
		if (m_protocol.getSizeReadable() > 0) {
			events |= SCE_RUDP_POLL_EV_READ;
		}
		if (m_protocol.getSizeWritable() > 0) {
			events |= SCE_RUDP_POLL_EV_WRITE;
		}
	} else if (m_protocol.m_state == SCE_RUDP_STATE_CLOSE_WAIT) {
		int avail = m_protocol.getSizeReadable();
		if (avail > 0 || avail == SCE_RUDP_ERROR_END_OF_DATA) {
			return SCE_RUDP_POLL_EV_READ;
		}
	}
	return events;
}

uint16_t RudpContext::queryPollEvents() const
{
	return computePollEvents();
}

int RudpContext::getParentId() const
{
	if (m_magic != RUDP_CONTEXT_MAGIC || m_parent == 0) {
		return -1;
	}
	return m_parent->m_ctxId;
}

uint32_t RudpContext::getChildCount() const
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		return m_childrenByEndpoint.getCount();
	}
	return 0;
}

void RudpContext::processCookieChallenges()
{
	if (m_cookieChallengeQueue == 0) {
		return;
	}
	RudpHeader hdr;
	RudpEndpoint targetEp;
	targetEp.vport = 0;
	rudpMemset(&targetEp.addr, 0, sizeof(targetEp.addr));

	while (true) {
		RudpRingBuffer<RudpEndpoint> *q = m_cookieChallengeQueue;
		if (q->m_items == 0 || q->m_capacity == 0 || (q->m_tail == q->m_head && !q->m_full)) {
			break;
		}
		q->m_full = false;
		targetEp = q->m_items[q->m_head];
		q->m_head++;
		if (q->m_head == q->m_capacity) {
			q->m_head = 0;
		}

		uint32_t nowMsNet = sceNetNtohl((uint32_t)(rudpGetTimeUsec() / 1000ULL));
		uint32_t nonceNet = sceNetNtohl(rudpRandU32());
		uint32_t secretNet = sceNetNtohl(m_cookieSecret);

		SceSha256Context sha;
		uint8_t cookie[32];
		rudpSha256Init(&sha);
		rudpSha256Update(&sha, &nowMsNet, 4);
		rudpSha256Update(&sha, &nonceNet, 4);
		rudpSha256Update(&sha, &secretNet, 4);
		rudpSha256Final(&sha, cookie);
		rudpMemcpy(cookie, &nowMsNet, 4);
		rudpMemcpy(cookie + 4, &nonceNet, 4);

		hdr.reset();
		hdr.m_seq = 0;
		hdr.m_type = RUDP_PKT_RST;
		hdr.encodeWindow(0);
		hdr.setCookie(cookie, sizeof(cookie));
		hdr.m_connectionId = 0;
		hdr.m_rstReason = 2;

		if (m_delegate != 0) {
			m_delegate->onContextSendDirect(this, m_muxMode, &hdr, m_sock, &m_localEndpoint, &targetEp);
		}
	}

	if (m_cookieTimeoutSec == 0) {
		if (m_cookieChallengeQueue->m_items != 0) {
			rudpFree(m_cookieChallengeQueue->m_items);
		}
		rudpFree(m_cookieChallengeQueue);
		m_cookieChallengeQueue = 0;
	}
}

void RudpContext::onProtocolConnected(RudpProtocol *proto)
{
	(void)proto;
	RudpEvent ev;
	if (m_parent != 0) {
		ev.handlerId = m_parent->m_handlerId;
		ev.type = RUDP_EV_CHILD_ACCEPTED;
		ev.detail = 0;
		ev.arg = m_parent->m_ctxId;
	} else {
		ev.handlerId = m_handlerId;
		ev.type = RUDP_EV_CONNECTED;
		ev.detail = 0;
		ev.arg = 0;
	}
	RudpResult res = m_scheduler->postEvent(&ev);
	res.getCode();
}

void RudpContext::onProtocolConnectFailed(RudpProtocol *proto, uint8_t reason)
{
	(void)proto;
	RudpEvent ev;
	if (m_parent != 0) {
		ev.handlerId = m_parent->m_handlerId;
		ev.type = RUDP_EV_CHILD_FAILED;
		ev.detail = 0;
		ev.arg = m_ctxId;
	} else {
		ev.handlerId = m_handlerId;
		ev.type = RUDP_EV_CONNECT_FAILED;
		ev.detail = reason;
		ev.arg = 0;
	}
	RudpResult res = m_scheduler->postEvent(&ev);
	res.getCode();
}

void RudpContext::onProtocolDisconnected(RudpProtocol *proto, uint8_t reason)
{
	(void)proto;
	RudpEvent ev;
	ev.handlerId = m_handlerId;
	ev.type = RUDP_EV_DISCONNECTED;
	ev.detail = reason;
	ev.arg = 0;
	RudpResult res = m_scheduler->postEvent(&ev);
	res.getCode();
}

void RudpContext::onProtocolReadable(RudpProtocol *proto)
{
	(void)proto;
	RudpEvent ev;
	ev.handlerId = m_handlerId;
	ev.type = RUDP_EV_READABLE;
	ev.detail = 0;
	ev.arg = 0;
	RudpResult res = m_scheduler->postEvent(&ev);
	res.getCode();
}

void RudpContext::onProtocolWritable(RudpProtocol *proto)
{
	(void)proto;
	RudpEvent ev;
	ev.handlerId = m_handlerId;
	ev.type = RUDP_EV_WRITABLE;
	ev.detail = 0;
	ev.arg = 0;
	RudpResult res = m_scheduler->postEvent(&ev);
	res.getCode();
}

void RudpContext::onProtocolFlushed(RudpProtocol *proto)
{
	(void)proto;
	RudpEvent ev;
	ev.handlerId = m_handlerId;
	ev.type = RUDP_EV_FLUSHED;
	ev.detail = 0;
	ev.arg = 0;
	RudpResult res = m_scheduler->postEvent(&ev);
	res.getCode();
}

void RudpContext::onProtocolScheduleSyn(RudpProtocol *proto)
{
	(void)proto;
	RudpEvent ev;
	ev.handlerId = m_handlerId;
	ev.type = RUDP_EV_SEND_SYN;
	ev.detail = 0;
	ev.arg = 0;
	RudpResult res = m_scheduler->postEvent(&ev);
	res.getCode();
}

void RudpContext::onProtocolScheduleAck(RudpProtocol *proto)
{
	(void)proto;
	RudpEvent ev;
	ev.handlerId = m_handlerId;
	ev.type = RUDP_EV_SEND_ACK;
	ev.detail = 0;
	ev.arg = 0;
	RudpResult res = m_scheduler->postEvent(&ev);
	res.getCode();
}

void RudpContext::onProtocolScheduleRst(RudpProtocol *proto, uint32_t connId, uint8_t reason)
{
	(void)proto;
	RudpEvent ev;
	ev.handlerId = m_handlerId;
	ev.type = RUDP_EV_SEND_RST;
	ev.detail = reason;
	ev.arg = (int)connId;
	RudpResult res = m_scheduler->postEvent(&ev);
	res.getCode();
}

void RudpContext::onProtocolScheduleData(RudpProtocol *proto)
{
	(void)proto;
	RudpEvent ev;
	ev.handlerId = m_handlerId;
	ev.type = RUDP_EV_SEND_DATA;
	ev.detail = 0;
	ev.arg = 0;
	RudpResult res = m_scheduler->postEvent(&ev);
	res.getCode();
}

void RudpContext::onProtocolSchedulePing(RudpProtocol *proto)
{
	(void)proto;
	RudpEvent ev;
	ev.handlerId = m_handlerId;
	ev.type = RUDP_EV_SEND_PING;
	ev.detail = 0;
	ev.arg = 0;
	RudpResult res = m_scheduler->postEvent(&ev);
	res.getCode();
}

void RudpContext::onProtocolSetTimer(RudpProtocol *proto, uint64_t intervalUsec)
{
	(void)proto;
	if (intervalUsec != 0) {
		RudpResult res = m_scheduler->startTimer(m_handlerId, intervalUsec);
		if (res.getCode() == SCE_RUDP_ERROR_MEMORY) {
			RudpEvent ev;
			ev.handlerId = m_handlerId;
			ev.type = RUDP_EV_OUT_OF_MEMORY;
			ev.detail = 0;
			ev.arg = 0;
			RudpResult postRes = m_scheduler->postEvent(&ev);
			postRes.getCode();
		}
	} else {
		m_scheduler->stopTimer(m_handlerId);
	}
}

void RudpContext::onProtocolTransmit(const RudpTransmitRequest *req)
{
	if (m_delegate != 0) {
		m_delegate->onContextSendPacket(this, req->header, req->payload, req->aggrDelayMs, req->txTimestampOut);
	}
}

void RudpContext::onProtocolClosed(RudpProtocol *proto)
{
	(void)proto;
	RudpEvent ev;
	ev.handlerId = m_handlerId;
	ev.type = RUDP_EV_CLOSED;
	ev.detail = 0;
	ev.arg = 0;
	RudpResult res = m_scheduler->postEvent(&ev);
	res.getCode();
}

void RudpContext::onProtocolCancelAggregator(RudpProtocol *proto)
{
	(void)proto;
	if (m_delegate != 0) {
		m_delegate->onContextCancelAggregator(this);
	}
}

void RudpContext::onProtocolGenerateRandom(RudpProtocol *proto, uint8_t *outDigest32)
{
	(void)proto;
	uint64_t now = rudpGetTimeUsec();
	uint32_t rnd = rudpRandU32();
	SceSha256Context sha;
	rudpSha256Init(&sha);
	rudpSha256Update(&sha, &m_sock, sizeof(m_sock));
	rudpSha256Update(&sha, &m_localEndpoint.addr, sizeof(m_localEndpoint.addr));
	rudpSha256Update(&sha, &m_localEndpoint.vport, sizeof(m_localEndpoint.vport));
	rudpSha256Update(&sha, &now, sizeof(now));
	rudpSha256Update(&sha, &rnd, sizeof(rnd));
	rudpSha256Final(&sha, outDigest32);
}

void RudpContext::onEvent(const RudpEvent *ev)
{
	switch (ev->type) {
	case RUDP_EV_CONNECTED:
		if (m_blocking || m_pollId >= 0) {
			if (m_blocking) {
				m_waitFlags |= RUDP_WAIT_ESTABLISHED;
				rudpCondBroadcast(&m_waitCond);
			}
			if (m_pollId >= 0 && m_delegate != 0) {
				m_delegate->onContextNotifyPoll(this, m_ctxId, m_pollId, (uint16_t)(computePollEvents() | SCE_RUDP_POLL_EV_WRITE));
			}
		} else if (m_eventHandler != 0 && (m_contextFlags & RUDP_CTX_FLAG_TERMINATING) == 0) {
			m_eventHandler(m_ctxId, SCE_RUDP_CONTEXT_EVENT_ESTABLISHED, 0, m_eventHandlerArg);
		}
		break;

	case RUDP_EV_CONNECT_FAILED: {
		if ((m_contextFlags & RUDP_CTX_FLAG_REMOTE_BOUND) != 0) {
			m_contextFlags &= ~RUDP_CTX_FLAG_REMOTE_BOUND;
			if (m_delegate != 0) {
				m_delegate->onContextUnregisterAggregator(this, m_sock, &m_remoteEndpoint.addr, m_ctxId);
			}
		}
		int errCode = mapReasonToErrorCode(ev->detail);
		m_lastError = errCode;
		if (m_blocking || m_pollId >= 0) {
			if (m_blocking) {
				m_waitFlags |= RUDP_WAIT_CLOSED;
				rudpCondBroadcast(&m_waitCond);
			}
			if (m_pollId >= 0 && (m_contextFlags & RUDP_CTX_FLAG_TERMINATING) == 0 && m_delegate != 0) {
				m_delegate->onContextNotifyPoll(this, m_ctxId, m_pollId, (uint16_t)(computePollEvents() | SCE_RUDP_POLL_EV_WRITE));
			}
		} else if (m_eventHandler != 0 && (m_contextFlags & RUDP_CTX_FLAG_TERMINATING) == 0) {
			m_eventHandler(m_ctxId, SCE_RUDP_CONTEXT_EVENT_CLOSED, errCode, m_eventHandlerArg);
		}
		break;
	}

	case RUDP_EV_DISCONNECTED: {
		int errCode = mapReasonToErrorCode(ev->detail);
		m_lastError = errCode;
		if (m_blocking || m_pollId >= 0) {
			if (m_blocking) {
				m_waitFlags |= RUDP_WAIT_CLOSED;
				rudpCondBroadcast(&m_waitCond);
			}
			if (m_pollId >= 0 && (m_contextFlags & RUDP_CTX_FLAG_TERMINATING) == 0 && m_delegate != 0) {
				m_delegate->onContextNotifyPoll(this, m_ctxId, m_pollId, (uint16_t)(computePollEvents() | SCE_RUDP_POLL_EV_READ));
			}
		} else if (m_eventHandler != 0 && (m_contextFlags & RUDP_CTX_FLAG_TERMINATING) == 0) {
			m_eventHandler(m_ctxId, SCE_RUDP_CONTEXT_EVENT_CLOSED, errCode, m_eventHandlerArg);
		}
		break;
	}

	case RUDP_EV_READABLE:
		if (m_protocol.getSizeReadable() != 0) {
			if (m_blocking || m_pollId >= 0) {
				if (m_blocking) {
					m_waitFlags |= RUDP_WAIT_READABLE;
					rudpCondBroadcast(&m_waitCond);
				}
				if (m_pollId >= 0 && m_delegate != 0) {
					m_delegate->onContextNotifyPoll(this, m_ctxId, m_pollId, (uint16_t)(computePollEvents() | SCE_RUDP_POLL_EV_READ));
				}
			} else if (m_eventHandler != 0 && (m_contextFlags & RUDP_CTX_FLAG_TERMINATING) == 0) {
				m_eventHandler(m_ctxId, SCE_RUDP_CONTEXT_EVENT_READABLE, 0, m_eventHandlerArg);
			}
		}
		break;

	case RUDP_EV_WRITABLE:
		if (m_protocol.getSizeWritable() != 0) {
			if (m_blocking || m_pollId >= 0) {
				if (m_blocking) {
					m_waitFlags |= RUDP_WAIT_WRITABLE;
					rudpCondBroadcast(&m_waitCond);
				}
				if (m_pollId >= 0 && m_delegate != 0) {
					m_delegate->onContextNotifyPoll(this, m_ctxId, m_pollId, (uint16_t)(computePollEvents() | SCE_RUDP_POLL_EV_WRITE));
				}
			} else if (m_eventHandler != 0 && (m_contextFlags & RUDP_CTX_FLAG_TERMINATING) == 0) {
				m_eventHandler(m_ctxId, SCE_RUDP_CONTEXT_EVENT_WRITABLE, 0, m_eventHandlerArg);
			}
		}
		break;

	case RUDP_EV_FLUSHED:
		if (m_blocking || m_pollId >= 0) {
			if (m_blocking) {
				m_waitFlags |= RUDP_WAIT_FLUSHED;
				rudpCondBroadcast(&m_waitCond);
			}
			if (m_pollId >= 0 && m_delegate != 0) {
				m_delegate->onContextNotifyPoll(this, m_ctxId, m_pollId, (uint16_t)(computePollEvents() | SCE_RUDP_POLL_EV_FLUSH));
			}
		} else if (m_eventHandler != 0 && (m_contextFlags & RUDP_CTX_FLAG_TERMINATING) == 0) {
			m_eventHandler(m_ctxId, SCE_RUDP_CONTEXT_EVENT_FLUSHED, 0, m_eventHandlerArg);
		}
		break;

	case RUDP_EV_SEND_SYN:
		m_protocol.sendSyn();
		break;

	case RUDP_EV_SEND_ACK:
		m_protocol.sendAck();
		break;

	case RUDP_EV_SEND_RST:
		m_protocol.sendReset((uint32_t)ev->arg, ev->detail);
		break;

	case RUDP_EV_SEND_DATA:
		m_protocol.sendData();
		break;

	case RUDP_EV_SEND_PING:
		m_protocol.sendPingAck();
		break;

	case RUDP_EV_CHILD_ACCEPTED:
		if (m_blocking || m_pollId >= 0) {
			if (m_blocking) {
				m_waitFlags |= RUDP_WAIT_ACCEPTED;
				rudpCondBroadcast(&m_waitCond);
			}
			if (m_pollId >= 0 && m_delegate != 0) {
				m_delegate->onContextNotifyPoll(this, m_ctxId, m_pollId, computePollEvents());
			}
		} else if (m_eventHandler != 0 && (m_contextFlags & RUDP_CTX_FLAG_TERMINATING) == 0) {
			m_eventHandler(m_ctxId, SCE_RUDP_CONTEXT_EVENT_ACCEPTED, 0, m_eventHandlerArg);
		}
		break;

	case RUDP_EV_CHILD_FAILED: {
		RudpContext *child = g_rudpContextManager->findContext(ev->arg, 1, 0);
		if (child != 0) {
			for (RudpPendingChildNode *node = (RudpPendingChildNode *)m_pendingChildren.first(0);
			     node != 0;
			     node = (RudpPendingChildNode *)m_pendingChildren.next(node, 0)) {
				if (rudpSockaddrEqual(&node->m_context->m_remoteEndpoint.addr, &child->m_remoteEndpoint.addr) != 0 &&
				    node->m_context->m_remoteEndpoint.vport == child->m_remoteEndpoint.vport &&
				    node->m_context->m_ctxId == ev->arg) {
					m_pendingChildren.remove(node);
					delete node;
					break;
				}
			}
		}
		if (ev->arg != -1) {
			g_rudpContextManager->destroyContext(ev->arg);
		}
		break;
	}

	case RUDP_EV_CLOSED:
		if ((m_contextFlags & RUDP_CTX_FLAG_REMOTE_BOUND) != 0) {
			m_contextFlags &= ~RUDP_CTX_FLAG_REMOTE_BOUND;
			if (m_delegate != 0) {
				m_delegate->onContextUnregisterAggregator(this, m_sock, &m_remoteEndpoint.addr, m_ctxId);
			}
		}
		if ((m_contextFlags & RUDP_CTX_FLAG_TERMINATING) != 0) {
			if (m_ctxId != -1) {
				g_rudpContextManager->destroyContext(m_ctxId);
			}
		} else if (m_parent != 0 && m_magic == RUDP_CONTEXT_MAGIC) {
			m_parent->detachChild(&m_remoteEndpoint);
			m_parent = 0;
		}
		break;

	case RUDP_EV_SEND_COOKIE:
		processCookieChallenges();
		break;

	case RUDP_EV_OUT_OF_MEMORY:
		m_lastError = SCE_RUDP_ERROR_MEMORY;
		if (!m_blocking && m_pollId < 0) {
			if (m_eventHandler != 0 && (m_contextFlags & RUDP_CTX_FLAG_TERMINATING) == 0) {
				m_eventHandler(m_ctxId, SCE_RUDP_CONTEXT_EVENT_ERROR, SCE_RUDP_ERROR_MEMORY, m_eventHandlerArg);
			}
		} else {
			if (m_blocking) {
				m_waitFlags |= RUDP_WAIT_CANCELLED;
				rudpCondBroadcast(&m_waitCond);
			}
			if (m_pollId >= 0 && m_delegate != 0) {
				m_delegate->onContextNotifyPoll(this, m_ctxId, m_pollId, (uint16_t)(computePollEvents() | SCE_RUDP_POLL_EV_ERROR));
			}
		}
		break;

	case RUDP_EV_SOCKET_ERROR:
		if (m_blocking || m_pollId >= 0) {
			if (m_blocking) {
				m_waitFlags |= RUDP_WAIT_CANCELLED;
				rudpCondBroadcast(&m_waitCond);
			}
			if (m_pollId >= 0 && m_delegate != 0) {
				m_delegate->onContextNotifyPoll(this, m_ctxId, m_pollId, (uint16_t)(computePollEvents() | SCE_RUDP_POLL_EV_ERROR));
			}
		} else {
			m_lastError = ev->arg;
			if (m_eventHandler != 0 && (m_contextFlags & RUDP_CTX_FLAG_TERMINATING) == 0) {
				m_eventHandler(m_ctxId, SCE_RUDP_CONTEXT_EVENT_ERROR, ev->arg, m_eventHandlerArg);
			}
		}
		break;

	default:
		break;
	}
}

void RudpContext::onTimer(uint64_t *nextIntervalUsec)
{
	if (m_magic == RUDP_CONTEXT_MAGIC) {
		m_protocol.onTimerTick(nextIntervalUsec);
	}
}

RudpContextManager::RudpContextManager(RudpScheduler *scheduler)
	: m_nextCtxId(0),
	  m_contexts(rudpCompareInt),
	  m_scheduler(scheduler),
	  m_delegate(0)
{
}

void RudpContextManager::clear()
{
	for (RudpContextMapNode *node = (RudpContextMapNode *)m_contexts.first(0);
	     node != 0;
	     node = (RudpContextMapNode *)m_contexts.next(node, 0)) {
		RudpContext *ctx = node->m_context;
		if (ctx->m_magic == RUDP_CONTEXT_MAGIC && ctx->m_parent != 0) {
			ctx->m_parent->detachChild(&ctx->m_remoteEndpoint);
			ctx->m_parent = 0;
		}
	}
	while (m_contexts.getCount() != 0) {
		RudpContextMapNode *node = (RudpContextMapNode *)m_contexts.first(0);
		bool terminating = false;
		node->m_context->getState(&terminating);
		destroyContext(node->m_ctxId);
	}
	m_contexts.getCount();
}

RudpContextManager::~RudpContextManager()
{
	clear();
	destroyContextMapTree(&m_contexts);
}

void RudpContextManager::setDelegate(IRudpContextManagerDelegate *delegate)
{
	m_delegate = delegate;
}

RudpContext *RudpContextManager::findContext(int ctxId, uint8_t includeTerminating, RudpResult *outRes)
{
	RudpContext *ctx = 0;
	RudpContextMapNode *node = (RudpContextMapNode *)m_contexts.find(&ctxId);
	if (node != 0) {
		ctx = node->m_context;
	}
	if (ctx != 0 && includeTerminating == 0) {
		bool terminating = true;
		ctx->getState(&terminating);
		if (terminating) {
			ctx = 0;
		}
	}
	if (outRes != 0) {
		*outRes = (ctx != 0) ? 0 : SCE_RUDP_ERROR_INVALID_CONTEXT_ID;
	}
	return ctx;
}

RudpResult RudpContextManager::destroyContext(int ctxId)
{
	RudpResult res;
	RudpContext *ctx = findContext(ctxId, 1, &res);
	if (ctx != 0) {
		if (m_delegate != 0) {
			m_delegate->onContextDestroyed(ctx);
		}
		delete ctx;
		RudpContextMapNode *node = (RudpContextMapNode *)m_contexts.find(&ctxId);
		if (node != 0) {
			m_contexts.remove(node);
			delete node;
		}
	}
	return res;
}

RudpResult RudpContextManager::createContext(RudpContext **outCtx, IRudpContextDelegate *delegate)
{
	RudpResult res;
	int ctxId = m_nextCtxId;
	m_nextCtxId = (m_nextCtxId + 1) & 0x7FFFFFFF;
	*outCtx = 0;

	RudpContext *ctx = new RudpContext(m_scheduler, ctxId);
	*outCtx = ctx;
	if (ctx == 0) {
		res = SCE_RUDP_ERROR_MEMORY;
		return res;
	}
	int initErr = (ctx->m_magic == RUDP_CONTEXT_MAGIC) ? ctx->m_lastError : 0;
	res = initErr;
	if (res.getCode() != 0) {
		delete *outCtx;
		*outCtx = 0;
		return res;
	}
	ctx->m_delegate = delegate;
	RudpContextMapNode *node = new RudpContextMapNode();
	if (node != 0) {
		node->m_cleanupNext = 0;
		node->m_ctxId = ctxId;
		node->m_context = ctx;
		m_contexts.insert(node);
		res = 0;
	} else {
		res = SCE_RUDP_ERROR_MEMORY;
	}
	if (res.getCode() != 0 && *outCtx != 0) {
		delete *outCtx;
		*outCtx = 0;
	}
	return res;
}

RudpResult RudpContextManager::terminateContext(int ctxId)
{
	RudpResult res;
	RudpContext *ctx = findContext(ctxId, 1, &res);
	if (ctx != 0) {
		if (ctx->getState(0) > SCE_RUDP_STATE_CLOSED) {
			RudpResult termRes = ctx->terminate();
			res = termRes;
		} else {
			destroyContext(ctxId);
			res = 0;
		}
	}
	return res;
}

uint32_t RudpContextManager::getContextCount() const
{
	return m_contexts.getCount();
}

void RudpContextManager::cancelAllWaits()
{
	for (RudpContextMapNode *node = (RudpContextMapNode *)m_contexts.first(0);
	     node != 0;
	     node = (RudpContextMapNode *)m_contexts.next(node, 0)) {
		RudpContext *ctx = node->m_context;
		if (ctx->m_waiterCount > 0) {
			do {
				ctx->m_waitFlags |= RUDP_WAIT_CANCELLED;
				rudpCondBroadcast(&ctx->m_waitCond);
				rudpCondWait(&ctx->m_waitCond, &g_rudpMutex);
			} while (ctx->m_waiterCount > 0);
			ctx->m_waitFlags &= ~RUDP_WAIT_CANCELLED;
		}
	}
}
