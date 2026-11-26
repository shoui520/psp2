/*
 * rudp_scheduler.cpp: event queue, handler hash table, and timer RB-tree scheduler.
 */
#include "rudp_internal.h"

static int compareTimerKey(const void *lhs, const void *rhs)
{
	return (*(const uint64_t *)lhs < *(const uint64_t *)rhs) ? 1 : 0;
}

RudpScheduler::RudpScheduler()
	: m_delegate(0),
	  m_bucketCount(64),
	  m_buckets(0),
	  m_handlerCount(0),
	  m_lastHandlerId(0xFFFFFFFFU),
	  m_timerTree(compareTimerKey),
	  m_eventSignalled(false),
	  m_waiterCount(0),
	  m_cancelling(false)
{
	m_eventQueue.m_full = false;
	m_eventQueue.m_items = 0;
	m_eventQueue.m_capacity = 256;
	m_eventQueue.m_head = 0;
	m_eventQueue.m_tail = 0;
	m_eventQueue.m_items = (RudpEvent *)rudpAlloc(256 * sizeof(RudpEvent));

	m_buckets = (RudpHandlerBucket **)rudpAlloc(m_bucketCount * sizeof(RudpHandlerBucket *));
	if (m_buckets != 0) {
		rudpMemset(m_buckets, 0, m_bucketCount * sizeof(RudpHandlerBucket *));
	}
	rudpCondInit(&m_cond, &g_rudpMutex);
}

void RudpScheduler::cancelWaits()
{
	while (m_waiterCount > 0) {
		m_cancelling = true;
		rudpCondBroadcast(&m_cond);
		rudpCondWait(&m_cond, &g_rudpMutex);
	}
}

RudpScheduler::~RudpScheduler()
{
	cancelWaits();
	while (m_timerTree.getCount() != 0) {
		RudpRbNode *node = m_timerTree.first(0);
		m_timerTree.remove(node);
		delete node;
	}
	rudpCondTerm(&m_cond);
	m_timerTree.clear();
	if (m_buckets != 0) {
		for (uint32_t i = 0; i < m_bucketCount; i++) {
			while (m_buckets[i] != 0) {
				RudpHandlerBucket *entry = m_buckets[i];
				RudpHandlerBucket *next = entry->next;
				rudpFree(entry);
				m_buckets[i] = next;
			}
		}
		m_handlerCount = 0;
		rudpFree(m_buckets);
	}
	if (m_eventQueue.m_items != 0) {
		rudpFree(m_eventQueue.m_items);
	}
}

void RudpScheduler::setDelegate(IRudpSchedulerDelegate *delegate)
{
	m_delegate = delegate;
}

RudpResult RudpScheduler::registerHandler(uint32_t *outId, IRudpEventHandler *handler)
{
	if (m_buckets == 0) {
		return RudpResult(SCE_RUDP_ERROR_MEMORY);
	}
	uint32_t startId = m_lastHandlerId;
	uint32_t candidate = (startId == 0xFFFFFFFFU) ? 1U : (startId + 1U);

	while (candidate != startId) {
		uint32_t bucket = candidate % m_bucketCount;
		RudpHandlerBucket *cur = m_buckets[bucket];
		bool collision = false;
		while (cur != 0) {
			if (cur->handlerId == candidate) {
				collision = true;
				break;
			}
			cur = cur->next;
		}
		if (!collision) {
			break;
		}
		candidate++;
		if (candidate == 0) {
			candidate = 1;
		}
	}

	if (candidate == startId) {
		*outId = 0;
		return RudpResult(SCE_RUDP_ERROR_INTERNAL);
	}

	uint32_t bucket = candidate % m_bucketCount;
	RudpHandlerBucket *entry = (RudpHandlerBucket *)rudpAlloc(sizeof(RudpHandlerBucket));
	if (entry == 0) {
		*outId = 0;
		return RudpResult(SCE_RUDP_ERROR_MEMORY);
	}
	entry->handlerId = candidate;
	entry->handler = handler;
	entry->next = 0;

	if (m_buckets[bucket] != 0) {
		RudpHandlerBucket *tail = m_buckets[bucket];
		while (tail->next != 0) {
			tail = tail->next;
		}
		tail->next = entry;
	} else {
		m_buckets[bucket] = entry;
	}
	m_handlerCount++;
	m_lastHandlerId = candidate;
	*outId = candidate;
	return RudpResult(0);
}

RudpResult RudpScheduler::unregisterHandler(uint32_t handlerId)
{
	if (m_buckets == 0) {
		return RudpResult(SCE_RUDP_ERROR_MEMORY);
	}
	uint32_t bucket = handlerId % m_bucketCount;
	RudpHandlerBucket *cur = m_buckets[bucket];
	RudpHandlerBucket *prev = 0;
	while (cur != 0) {
		if (cur->handlerId == handlerId) {
			if (prev != 0) {
				prev->next = cur->next;
			} else {
				m_buckets[bucket] = cur->next;
			}
			rudpFree(cur);
			m_handlerCount--;
			return RudpResult(0);
		}
		prev = cur;
		cur = cur->next;
	}
	return RudpResult(-1);
}

RudpResult RudpScheduler::postEvent(const RudpEvent *ev)
{
	int err = 0;
	if (m_eventQueue.m_tail == m_eventQueue.m_head && m_eventQueue.m_full) {
		uint32_t oldCap = m_eventQueue.m_capacity;
		if (oldCap >= RUDP_MAX_RING_CAPACITY) {
			return RudpResult(SCE_RUDP_ERROR_INTERNAL);
		}
		uint32_t count = m_eventQueue.getSize();
		uint32_t newCap = oldCap * 2;
		if (count <= newCap) {
			RudpEvent *newBuf = (RudpEvent *)rudpAlloc(newCap * sizeof(RudpEvent));
			if (newBuf != 0) {
				for (uint32_t i = 0; i < count; i++) {
					uint32_t srcIdx = m_eventQueue.m_head + i;
					if (srcIdx >= m_eventQueue.m_capacity) {
						srcIdx -= m_eventQueue.m_capacity;
					}
					newBuf[i] = m_eventQueue.m_items[srcIdx];
				}
				rudpFree(m_eventQueue.m_items);
				m_eventQueue.m_items = newBuf;
				m_eventQueue.m_capacity = newCap;
				m_eventQueue.m_head = 0;
				m_eventQueue.m_tail = count;
			} else {
				err = SCE_RUDP_ERROR_MEMORY;
			}
		} else {
			err = SCE_RUDP_ERROR_INTERNAL;
		}
	}

	if (err == 0) {
		if (m_eventQueue.m_items == 0 || m_eventQueue.m_capacity == 0 ||
		    (m_eventQueue.m_tail == m_eventQueue.m_head && m_eventQueue.m_full)) {
			err = SCE_RUDP_ERROR_INTERNAL;
		} else {
			m_eventQueue.m_full = true;
			m_eventQueue.m_items[m_eventQueue.m_tail] = *ev;
			m_eventQueue.m_tail++;
			if (m_eventQueue.m_tail == m_eventQueue.m_capacity) {
				m_eventQueue.m_tail = 0;
			}
			m_eventSignalled = true;
			rudpCondSignal(&m_cond);
			if (m_delegate != 0) {
				m_delegate->onEventPosted(this);
			}
		}
	}
	return RudpResult(err);
}

RudpResult RudpScheduler::startTimer(uint32_t handlerId, uint64_t intervalUsec)
{
	int err;
	IRudpEventHandler *handler = 0;
	if (m_buckets != 0) {
		RudpHandlerBucket *cur = m_buckets[handlerId % m_bucketCount];
		err = -1;
		while (cur != 0) {
			if (cur->handlerId == handlerId) {
				handler = cur->handler;
				err = 0;
				break;
			}
			cur = cur->next;
		}
	} else {
		err = SCE_RUDP_ERROR_MEMORY;
	}

	if (err != 0) {
		if (err != SCE_RUDP_ERROR_MEMORY) {
			err = SCE_RUDP_ERROR_INTERNAL;
		}
	} else {
		handler->m_timerIntervalUsec = intervalUsec;
		RudpTimerNode *node = new RudpTimerNode();
		if (node != 0) {
			node->m_handlerId = handlerId;
			node->m_expireTime = intervalUsec + rudpGetTimeUsec();
			m_timerTree.insert(node);
		} else {
			err = SCE_RUDP_ERROR_MEMORY;
		}
	}
	return RudpResult(err);
}

RudpResult RudpScheduler::stopTimer(uint32_t handlerId)
{
	if (m_buckets == 0) {
		return RudpResult(SCE_RUDP_ERROR_MEMORY);
	}
	RudpHandlerBucket *cur = m_buckets[handlerId % m_bucketCount];
	while (cur != 0) {
		if (cur->handlerId == handlerId) {
			cur->handler->m_timerIntervalUsec = 0;
			return RudpResult(0);
		}
		cur = cur->next;
	}
	return RudpResult(-1);
}

RudpResult RudpScheduler::processEvents(uint64_t timeoutUsec)
{
	int err = 0;
	m_waiterCount++;
	if (!m_cancelling) {
		if (m_buckets == 0) {
			err = SCE_RUDP_ERROR_MEMORY;
		} else {
			uint64_t startTime = rudpGetTimeUsec();
			while (true) {
				uint64_t now = rudpGetTimeUsec();
				while (m_timerTree.getCount() != 0) {
					RudpTimerNode *timerNode = (RudpTimerNode *)m_timerTree.first(0);
					if (timerNode == 0 || timerNode->m_expireTime > now) {
						break;
					}
					m_timerTree.remove(timerNode);
					IRudpEventHandler *handler = 0;
					if (m_buckets != 0) {
						RudpHandlerBucket *cur = m_buckets[timerNode->m_handlerId % m_bucketCount];
						while (cur != 0) {
							if (cur->handlerId == timerNode->m_handlerId) {
								handler = cur->handler;
								break;
							}
							cur = cur->next;
						}
					}
					if (handler != 0 && handler->m_timerIntervalUsec != 0) {
						handler->onTimer(&handler->m_timerIntervalUsec);
						timerNode->m_expireTime = handler->m_timerIntervalUsec + now;
						m_timerTree.insert(timerNode);
					} else {
						delete timerNode;
					}
				}

				while (m_eventQueue.getSize() != 0) {
					RudpEvent ev;
					ev.handlerId = 0;
					ev.type = 0;
					ev.detail = 0;
					ev.arg = 0;
					if (m_eventQueue.m_items != 0 && m_eventQueue.m_capacity != 0 &&
					    (m_eventQueue.m_tail != m_eventQueue.m_head || m_eventQueue.m_full)) {
						m_eventQueue.m_full = false;
						ev = m_eventQueue.m_items[m_eventQueue.m_head];
						m_eventQueue.m_head++;
						if (m_eventQueue.m_head == m_eventQueue.m_capacity) {
							m_eventQueue.m_head = 0;
						}
						if (m_buckets != 0) {
							RudpHandlerBucket *cur = m_buckets[ev.handlerId % m_bucketCount];
							while (cur != 0) {
								if (cur->handlerId == ev.handlerId) {
									if (cur->handler != 0) {
										cur->handler->onEvent(&ev);
									}
									break;
								}
								cur = cur->next;
							}
						}
					}
				}

				if (timeoutUsec == 0) {
					break;
				}
				m_eventSignalled = false;
				bool doneWaiting = false;
				while (!m_eventSignalled) {
					uint64_t curTime = rudpGetTimeUsec();
					uint32_t elapsed = (uint32_t)(curTime - startTime);
					if ((uint64_t)elapsed + 125ULL >= timeoutUsec) {
						doneWaiting = true;
						break;
					}
					if (m_eventQueue.getSize() != 0) {
						break;
					}
					uint64_t waitDur = timeoutUsec - (uint64_t)elapsed;
					bool waitForUserTimeout = (elapsed != 0);
					if (m_timerTree.getCount() != 0) {
						RudpTimerNode *nextTimer = (RudpTimerNode *)m_timerTree.first(0);
						uint64_t untilTimer = 0;
						if (nextTimer->m_expireTime > curTime) {
							untilTimer = nextTimer->m_expireTime - curTime;
						}
						if (untilTimer == 0) {
							break;
						}
						if (untilTimer < waitDur) {
							waitDur = untilTimer;
							waitForUserTimeout = false;
						}
					}
					int wret = rudpCondTimedWait(&m_cond, &g_rudpMutex, waitDur);
					if ((wret == RUDP_OSAL_ETIMEDOUT && waitForUserTimeout) || m_cancelling) {
						doneWaiting = true;
						break;
					}
				}
				if (doneWaiting) {
					break;
				}
			}
		}
	}

	m_waiterCount--;
	if (m_cancelling) {
		err = SCE_RUDP_ERROR_CANCELLED;
		if (m_waiterCount == 0) {
			rudpCondBroadcast(&m_cond);
			m_cancelling = false;
		}
	}
	return RudpResult(err);
}

bool RudpScheduler::isValid() const
{
	return m_buckets != 0;
}
