/*
 * rudp_poll.cpp: RUDP poll set (RudpPoll) and poll registry (RudpPollManager).
 */
#include "rudp_internal.h"

static int rudpComparePollContextId(const void *lhs, const void *rhs)
{
	return (*(const int *)lhs < *(const int *)rhs) ? 1 : 0;
}

static int rudpComparePollId(const void *lhs, const void *rhs)
{
	return (*(const int *)lhs < *(const int *)rhs) ? 1 : 0;
}

int rudpCompareInt(const void *lhs, const void *rhs)
{
	return (*(const int *)lhs < *(const int *)rhs) ? 1 : 0;
}

int RudpPoll::getId() const
{
	return m_pollId;
}

static void destroyPollContextTree(RudpRbTree *tree)
{
	RudpPollContextNode *chain = 0;
	while (true) {
		RudpPollContextNode *node = (RudpPollContextNode *)tree->next(chain, 0);
		if (node == 0) {
			break;
		}
		node->m_cleanupNext = chain;
		chain = node;
	}
	tree->clear();
	while (chain != 0) {
		RudpPollContextNode *cur = chain;
		chain = cur->m_cleanupNext;
		delete cur;
	}
}

static void destroyPollManagerTree(RudpRbTree *tree)
{
	RudpPollEntryNode *chain = 0;
	while (true) {
		RudpPollEntryNode *node = (RudpPollEntryNode *)tree->next(chain, 0);
		if (node == 0) {
			break;
		}
		node->m_cleanupNext = chain;
		chain = node;
	}
	tree->clear();
	while (chain != 0) {
		RudpPollEntryNode *cur = chain;
		chain = cur->m_cleanupNext;
		delete cur;
	}
}

RudpPoll::RudpPoll(int pollId, uint32_t initialCapacity, IRudpPollDelegate *delegate)
	: m_pollId(pollId),
	  m_delegate(0),
	  m_contexts(rudpComparePollContextId),
	  m_lastReportedCount(0),
	  m_waiterCount(0),
	  m_cancellerCount(0),
	  m_cancelPending(false),
	  m_destroying(false)
{
	m_readyQueue.m_items = 0;
	m_readyQueue.m_capacity = initialCapacity;
	m_readyQueue.m_head = 0;
	m_readyQueue.m_tail = 0;
	m_readyQueue.m_full = false;
	if (initialCapacity != 0) {
		m_readyQueue.m_items = (int *)rudpAlloc(initialCapacity * sizeof(int));
	}
	rudpCondInit(&m_cond, &g_rudpMutex);
	m_delegate = delegate;
}

void RudpPoll::cancel(bool setPendingFlag, bool waitAll)
{
	if (m_waiterCount != 0 || !setPendingFlag) {
		while (m_waiterCount > 0 && (!m_destroying || waitAll)) {
			m_cancellerCount++;
			rudpCondBroadcast(&m_cond);
			rudpCondWait(&m_cond, &g_rudpMutex);
			m_cancellerCount--;
		}
	} else {
		m_cancelPending = true;
	}
	if (m_destroying) {
		if (waitAll) {
			while (m_cancellerCount > 0) {
				rudpCondBroadcast(&m_cond);
				rudpCondWait(&m_cond, &g_rudpMutex);
			}
		} else {
			rudpCondBroadcast(&m_cond);
		}
	}
}

RudpPoll::~RudpPoll()
{
	m_destroying = true;
	cancel(false, true);
	if (m_delegate != 0) {
		for (RudpPollContextNode *node = (RudpPollContextNode *)m_contexts.first(0);
		     node != 0;
		     node = (RudpPollContextNode *)m_contexts.next(node, 0)) {
			m_delegate->onPollDetachContext(node->m_ctxId);
		}
	}
	rudpCondTerm(&m_cond);
	if (m_readyQueue.m_items != 0) {
		rudpFree(m_readyQueue.m_items);
	}
	destroyPollContextTree(&m_contexts);
}

RudpResult RudpPoll::addContext(int ctxId, uint16_t reqEvents, uint16_t currentEvents)
{
	int err = 0;
	if (m_contexts.find(&ctxId) != 0) {
		return RudpResult(SCE_RUDP_ERROR_ALREADY_EXISTS);
	}

	uint32_t cap = m_readyQueue.m_capacity;
	if (m_contexts.getCount() == cap) {
		if (cap >= RUDP_MAX_RING_CAPACITY) {
			return RudpResult(SCE_RUDP_ERROR_TOO_MANY_CONTEXTS);
		}
		uint32_t count = m_readyQueue.getSize();
		uint32_t newCap = cap * 2;
		int growErr = 0;
		if (count <= newCap) {
			int *newBuf = (int *)rudpAlloc(newCap * sizeof(int));
			if (newBuf != 0) {
				for (uint32_t i = 0; i < count; i++) {
					uint32_t srcIdx = m_readyQueue.m_head + i;
					if (srcIdx >= m_readyQueue.m_capacity) {
						srcIdx -= m_readyQueue.m_capacity;
					}
					newBuf[i] = m_readyQueue.m_items[srcIdx];
				}
				rudpFree(m_readyQueue.m_items);
				m_readyQueue.m_items = newBuf;
				m_readyQueue.m_capacity = newCap;
				m_readyQueue.m_head = 0;
				m_readyQueue.m_tail = count;
			} else {
				growErr = -4;
			}
		} else {
			growErr = -1;
		}
		if (growErr < 0) {
			return RudpResult(SCE_RUDP_ERROR_TOO_MANY_CONTEXTS);
		}
	}

	uint16_t matched = (uint16_t)(reqEvents & currentEvents);
	RudpPollContextNode *node = new RudpPollContextNode();
	if (node != 0) {
		node->m_cleanupNext = 0;
		node->m_ctxId = ctxId;
		node->m_reqEvents = reqEvents;
		node->m_rtnEvents = matched;
		m_contexts.insert(node);
	}

	if (matched != 0) {
		if (m_readyQueue.m_items != 0 && m_readyQueue.m_capacity != 0 &&
		    (m_readyQueue.m_tail != m_readyQueue.m_head || !m_readyQueue.m_full)) {
			m_readyQueue.m_full = true;
			m_readyQueue.m_items[m_readyQueue.m_tail] = ctxId;
			m_readyQueue.m_tail++;
			if (m_readyQueue.m_tail == m_readyQueue.m_capacity) {
				m_readyQueue.m_tail = 0;
			}
			RudpPollContextNode *found = (RudpPollContextNode *)m_contexts.find(&ctxId);
			if (found == 0) {
				RudpPollContextNode *extra = new RudpPollContextNode();
				if (extra != 0) {
					extra->m_cleanupNext = 0;
					extra->m_ctxId = ctxId;
					extra->m_reqEvents = 0;
					extra->m_rtnEvents = 0;
					m_contexts.insert(extra);
				}
				found = (RudpPollContextNode *)m_contexts.find(&ctxId);
			}
			found->m_rtnEvents |= 0x8000U;
			rudpCondSignal(&m_cond);
		}
	}
	return RudpResult(err);
}

RudpResult RudpPoll::modifyContext(int ctxId, uint16_t reqEvents, uint16_t currentEvents)
{
	RudpResult res(0);
	RudpPollContextNode *node = (RudpPollContextNode *)m_contexts.find(&ctxId);
	if (node == 0) {
		res = SCE_RUDP_ERROR_INVALID_CONTEXT_ID;
		return res;
	}
	uint16_t kept = (uint16_t)(node->m_rtnEvents & (reqEvents | 0x8000U));
	node->m_reqEvents = reqEvents;
	uint16_t combined = (uint16_t)(kept | (reqEvents & currentEvents));
	node->m_rtnEvents = combined;
	if ((combined & 0x8000U) == 0 && (combined & 0x7FFFU) != 0) {
		if (m_readyQueue.m_items != 0 && m_readyQueue.m_capacity != 0 &&
		    (m_readyQueue.m_tail != m_readyQueue.m_head || !m_readyQueue.m_full)) {
			m_readyQueue.m_full = true;
			m_readyQueue.m_items[m_readyQueue.m_tail] = ctxId;
			m_readyQueue.m_tail++;
			if (m_readyQueue.m_tail == m_readyQueue.m_capacity) {
				m_readyQueue.m_tail = 0;
			}
			node->m_rtnEvents |= 0x8000U;
			rudpCondSignal(&m_cond);
		}
	}
	return res;
}

RudpResult RudpPoll::removeContext(int ctxId)
{
	RudpResult res(0);
	RudpPollContextNode *node = (RudpPollContextNode *)m_contexts.find(&ctxId);
	if (node == 0) {
		res = SCE_RUDP_ERROR_INVALID_CONTEXT_ID;
		return res;
	}
	if ((node->m_rtnEvents & 0x8000U) == 0) {
		if (m_readyQueue.m_items != 0 && m_readyQueue.m_capacity != 0 &&
		    (m_readyQueue.m_tail != m_readyQueue.m_head || !m_readyQueue.m_full)) {
			m_readyQueue.m_full = true;
			m_readyQueue.m_items[m_readyQueue.m_tail] = ctxId;
			m_readyQueue.m_tail++;
			if (m_readyQueue.m_tail == m_readyQueue.m_capacity) {
				m_readyQueue.m_tail = 0;
			}
			rudpCondBroadcast(&m_cond);
		}
	}
	m_contexts.remove(node);
	delete node;
	return res;
}

int RudpPoll::wait(SceRudpPollEvent *events, uint32_t maxEvents, uint64_t timeoutUsec)
{
	uint32_t reported = 0;
	uint64_t startTime = rudpGetTimeUsec();
	bool timedOut = false;

	for (uint32_t i = 0; i < m_lastReportedCount; i++) {
		int ctxId = -1;
		if (m_readyQueue.m_items != 0 && m_readyQueue.m_capacity != 0 &&
		    (m_readyQueue.m_tail != m_readyQueue.m_head || m_readyQueue.m_full)) {
			m_readyQueue.m_full = false;
			ctxId = m_readyQueue.m_items[m_readyQueue.m_head];
			m_readyQueue.m_head++;
			if (m_readyQueue.m_head == m_readyQueue.m_capacity) {
				m_readyQueue.m_head = 0;
			}
		}
		if (ctxId < 0) {
			continue;
		}
		RudpPollContextNode *node = (RudpPollContextNode *)m_contexts.find(&ctxId);
		if (node == 0) {
			continue;
		}
		if ((node->m_rtnEvents & SCE_RUDP_POLL_EV_ERROR) != 0) {
			m_contexts.remove(node);
			delete node;
			if (m_delegate != 0) {
				m_delegate->onPollDetachContext(ctxId);
			}
		} else {
			uint16_t curEvents = 0;
			if (m_delegate != 0) {
				m_delegate->onPollQueryEvents(ctxId, &curEvents);
			}
			uint16_t merged = (uint16_t)(node->m_rtnEvents | (node->m_reqEvents & curEvents));
			node->m_rtnEvents = merged;
			uint16_t active = (uint16_t)(merged & 0x7FFFU);
			if (active != 0) {
				if (m_readyQueue.m_items != 0 && m_readyQueue.m_capacity != 0 &&
				    (m_readyQueue.m_tail != m_readyQueue.m_head || !m_readyQueue.m_full)) {
					m_readyQueue.m_full = true;
					m_readyQueue.m_items[m_readyQueue.m_tail] = ctxId;
					m_readyQueue.m_tail++;
					if (m_readyQueue.m_tail == m_readyQueue.m_capacity) {
						m_readyQueue.m_tail = 0;
					}
				} else {
					node->m_rtnEvents = active;
				}
			} else {
				node->m_rtnEvents = 0;
			}
		}
	}

	uint64_t deadline = startTime + timeoutUsec;
	uint32_t readyCount = 0;
	while (true) {
		readyCount = m_readyQueue.getSize();
		if (readyCount != 0 || timedOut || m_cancellerCount != 0 || m_cancelPending) {
			break;
		}
		uint64_t now = rudpGetTimeUsec();
		uint64_t waitDur;
		if (timeoutUsec == SCE_RUDP_USEC_INDEFINITE) {
			waitDur = SCE_RUDP_USEC_INDEFINITE;
		} else {
			waitDur = (deadline > now) ? (deadline - now) : 0;
		}
		m_waiterCount++;
		int wret = rudpCondTimedWait(&m_cond, &g_rudpMutex, waitDur);
		m_waiterCount--;
		if (m_waiterCount == 0 && m_cancellerCount > 0) {
			rudpCondBroadcast(&m_cond);
		}
		if (wret == RUDP_OSAL_ETIMEDOUT) {
			timedOut = true;
		}
	}

	uint32_t scanned = 0;
	if (readyCount != 0) {
		for (uint32_t i = 0; i < readyCount && reported < maxEvents; i++) {
			uint32_t idx = m_readyQueue.m_head + i;
			if (idx >= m_readyQueue.m_capacity) {
				idx -= m_readyQueue.m_capacity;
			}
			int ctxId = m_readyQueue.m_items[idx];
			RudpPollContextNode *node = (RudpPollContextNode *)m_contexts.find(&ctxId);
			if (node == 0) {
				events[i].ctxId = ctxId;
				events[i].reqEvents = 0;
				events[i].rtnEvents = SCE_RUDP_POLL_EV_ERROR;
				reported++;
			} else if ((node->m_rtnEvents & 0x8000U) != 0) {
				uint16_t raw = node->m_rtnEvents;
				events[i].ctxId = ctxId;
				events[i].reqEvents = node->m_reqEvents;
				events[i].rtnEvents = (uint16_t)(raw & 0x7FFFU);
				node->m_rtnEvents = (uint16_t)(raw & (0x8000U | SCE_RUDP_POLL_EV_ERROR));
				reported++;
			}
			scanned = i + 1;
		}
	}

	m_lastReportedCount = scanned;
	int result = (int)reported;
	if (reported == 0 && (m_cancellerCount != 0 || m_cancelPending)) {
		result = SCE_RUDP_ERROR_CANCELLED;
	}
	m_cancelPending = false;
	return result;
}

void RudpPoll::notifyContext(int ctxId, uint16_t events)
{
	RudpPollContextNode *node = (RudpPollContextNode *)m_contexts.find(&ctxId);
	if (node == 0) {
		return;
	}
	uint16_t combined = (uint16_t)((node->m_reqEvents & events) | node->m_rtnEvents);
	node->m_rtnEvents = combined;
	if ((combined & 0x8000U) == 0 && (combined & 0x7FFFU) != 0) {
		if (m_readyQueue.m_items != 0 && m_readyQueue.m_capacity != 0 &&
		    (m_readyQueue.m_tail != m_readyQueue.m_head || !m_readyQueue.m_full)) {
			m_readyQueue.m_full = true;
			m_readyQueue.m_items[m_readyQueue.m_tail] = ctxId;
			m_readyQueue.m_tail++;
			if (m_readyQueue.m_tail == m_readyQueue.m_capacity) {
				m_readyQueue.m_tail = 0;
			}
			node->m_rtnEvents |= 0x8000U;
			rudpCondSignal(&m_cond);
		}
	}
}

RudpPollManager::RudpPollManager()
	: m_polls(rudpComparePollId), m_nextPollId(0)
{
}

void RudpPollManager::clear()
{
	for (RudpPollEntryNode *node = (RudpPollEntryNode *)m_polls.first(0);
	     node != 0;
	     node = (RudpPollEntryNode *)m_polls.next(node, 0)) {
		if (node->m_poll != 0) {
			delete node->m_poll;
		}
	}
	destroyPollManagerTree(&m_polls);
}

RudpPollManager::~RudpPollManager()
{
	clear();
	destroyPollManagerTree(&m_polls);
}

RudpResult RudpPollManager::createPoll(uint32_t size, IRudpPollDelegate *delegate)
{
	RudpResult res;
	RudpPoll *poll = new RudpPoll(m_nextPollId, size, delegate);
	if (poll == 0) {
		res = SCE_RUDP_ERROR_MEMORY;
		return res;
	}
	m_nextPollId = (m_nextPollId + 1) & 0x7FFFFFFF;
	RudpPollEntryNode *node = new RudpPollEntryNode();
	if (node != 0) {
		node->m_cleanupNext = 0;
		node->m_pollId = poll->getId();
		node->m_poll = poll;
		m_polls.insert(node);
	}
	res = poll->getId();
	return res;
}

RudpResult RudpPollManager::destroyPoll(int pollId)
{
	RudpResult res(0);
	RudpPollEntryNode *node = (RudpPollEntryNode *)m_polls.find(&pollId);
	if (node == 0) {
		res = SCE_RUDP_ERROR_INVALID_POLL_ID;
		return res;
	}
	RudpPoll *poll = node->m_poll;
	m_polls.remove(node);
	delete node;
	if (poll != 0) {
		delete poll;
	}
	return res;
}

RudpPoll *RudpPollManager::findPoll(int pollId)
{
	RudpPollEntryNode *node = (RudpPollEntryNode *)m_polls.find(&pollId);
	if (node != 0) {
		return node->m_poll;
	}
	return 0;
}

void RudpPollManager::cancelAll()
{
	for (RudpPollEntryNode *node = (RudpPollEntryNode *)m_polls.first(0);
	     node != 0;
	     node = (RudpPollEntryNode *)m_polls.next(node, 0)) {
		node->m_poll->cancel(false, false);
	}
}
