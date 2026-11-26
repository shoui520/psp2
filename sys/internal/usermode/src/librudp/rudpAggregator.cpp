/*
 * rudpAggregator.cpp: packet aggregation buffer manager per (socket, peer address).
 */
#include "rudp_internal.h"

int rudpCompareAggregatorKey(const void *lhs, const void *rhs)
{
	const RudpAggregatorKey *a = (const RudpAggregatorKey *)lhs;
	const RudpAggregatorKey *b = (const RudpAggregatorKey *)rhs;
	if (a->sock < b->sock) {
		return 1;
	}
	if (a->sock == b->sock && rudpSockaddrLess(&a->addr, &b->addr) != 0) {
		return 1;
	}
	return 0;
}

static void destroyAggregatorTree(RudpRbTree *tree)
{
	RudpAggregatorNode *chain = 0;
	while (true) {
		RudpAggregatorNode *node = (RudpAggregatorNode *)tree->next(chain, 0);
		if (node == 0) {
			break;
		}
		node->m_cleanupNext = chain;
		chain = node;
	}
	tree->clear();
	while (chain != 0) {
		RudpAggregatorNode *cur = chain;
		chain = cur->m_cleanupNext;
		delete cur;
	}
}

RudpAggregator::RudpAggregator(RudpScheduler *scheduler)
	: m_scheduler(scheduler),
	  m_delegate(0),
	  m_handlerId(0),
	  m_maxSegmentSize(RUDP_DEFAULT_MSS),
	  m_timerIntervalMs(15),
	  m_entries(rudpCompareAggregatorKey),
	  m_flags(0)
{
	RudpResult res = m_scheduler->registerHandler(&m_handlerId, this);
	res.getCode();
}

RudpAggregator::~RudpAggregator()
{
	if (m_handlerId != 0) {
		RudpResult res = m_scheduler->unregisterHandler(m_handlerId);
		res.getCode();
	}
	destroyAggregatorTree(&m_entries);
}

void RudpAggregator::setDelegate(IRudpAggregatorDelegate *delegate)
{
	m_delegate = delegate;
}

void RudpAggregator::setMaxSegmentSize(uint16_t mss)
{
	m_maxSegmentSize = mss;
}

uint16_t RudpAggregator::getMaxSegmentSize() const
{
	return m_maxSegmentSize;
}

RudpResult RudpAggregator::registerContext(const RudpAggregatorKey *key, int ctxId, uint32_t aggrTimeoutMs)
{
	if (key->sock < 0) {
		return RudpResult(SCE_RUDP_ERROR_INVALID_SOCKET);
	}
	RudpResult res(0);
	RudpAggregatorBuffer *aggrBuf = 0;
	RudpAggregatorNode *found = (RudpAggregatorNode *)m_entries.find(key);
	if (found != 0) {
		aggrBuf = found->m_buffer;
	} else {
		aggrBuf = new RudpAggregatorBuffer();
		if (aggrBuf != 0) {
			aggrBuf->m_storage = 0;
			aggrBuf->m_contextList = 0;
			aggrBuf->m_startTimeUsec = 0;
			aggrBuf->m_contextCount = 0;
			aggrBuf->m_maxTimeoutMs = 0;
			aggrBuf->m_deadlineMs = 0;
			aggrBuf->m_packetCount = 0;
			aggrBuf->m_storage = (uint8_t *)rudpAlloc(m_maxSegmentSize);
			if (aggrBuf->m_storage != 0) {
				aggrBuf->m_buf.setBuffer(aggrBuf->m_storage, m_maxSegmentSize);
			}
		}
		if (aggrBuf != 0) {
			if (aggrBuf->m_storage != 0) {
				RudpAggregatorNode *node = new RudpAggregatorNode();
				if (node != 0) {
					node->m_cleanupNext = 0;
					node->m_key = *key;
					node->m_buffer = aggrBuf;
					m_entries.insert(node);
					res = 0;
				} else {
					res = SCE_RUDP_ERROR_MEMORY;
				}
			} else {
				delete aggrBuf;
				aggrBuf = 0;
				res = SCE_RUDP_ERROR_MEMORY;
			}
		} else {
			res = SCE_RUDP_ERROR_MEMORY;
		}
	}

	if (res.getCode() == 0) {
		RudpAggregatorBuffer::ContextEntry *entry =
			(RudpAggregatorBuffer::ContextEntry *)rudpAlloc(sizeof(RudpAggregatorBuffer::ContextEntry));
		if (entry != 0) {
			entry->ctxId = ctxId;
			entry->aggrTimeoutMs = aggrTimeoutMs;
			entry->next = aggrBuf->m_contextList;
			aggrBuf->m_contextList = entry;
			aggrBuf->m_contextCount++;

			uint32_t minTimeout = 1000;
			uint32_t maxTimeout = 30;
			for (RudpAggregatorBuffer::ContextEntry *cur = aggrBuf->m_contextList; cur != 0; cur = cur->next) {
				uint32_t val = cur->aggrTimeoutMs;
				if (minTimeout > val) {
					minTimeout = (val > 30) ? val : 30;
				}
				if (maxTimeout < val) {
					maxTimeout = (val < 1000) ? val : 1000;
				}
			}
			aggrBuf->m_maxTimeoutMs = maxTimeout;
			m_timerIntervalMs = minTimeout >> 1;
			if ((m_flags & 1) == 0) {
				m_flags |= 1;
				aggrBuf->m_deadlineMs = aggrBuf->m_maxTimeoutMs;
				RudpResult timerRes = m_scheduler->startTimer(m_handlerId, (uint64_t)m_timerIntervalMs * 1000ULL);
				res = timerRes;
			}
		} else {
			res = SCE_RUDP_ERROR_MEMORY;
		}
	}

	if (res.getCode() == SCE_RUDP_ERROR_MEMORY && aggrBuf != 0 && aggrBuf->m_contextCount == 0) {
		RudpAggregatorNode *cleanupNode = (RudpAggregatorNode *)m_entries.find(key);
		if (cleanupNode != 0) {
			m_entries.remove(cleanupNode);
			delete cleanupNode;
		}
	}
	return res;
}

void RudpAggregator::unregisterContext(const RudpAggregatorKey *key, int ctxId)
{
	if (key->sock < 0) {
		return;
	}
	RudpAggregatorNode *node = (RudpAggregatorNode *)m_entries.find(key);
	if (node == 0) {
		return;
	}
	RudpAggregatorBuffer *aggrBuf = node->m_buffer;
	if (aggrBuf->m_contextCount != 0) {
		RudpAggregatorBuffer::ContextEntry *cur = aggrBuf->m_contextList;
		RudpAggregatorBuffer::ContextEntry *prev = 0;
		while (cur != 0) {
			if (cur->ctxId == ctxId) {
				if (prev != 0) {
					prev->next = cur->next;
				} else {
					aggrBuf->m_contextList = cur->next;
				}
				rudpFree(cur);
				break;
			}
			prev = cur;
			cur = cur->next;
		}
		aggrBuf->m_contextCount--;
	}

	if (aggrBuf->m_contextCount != 0) {
		uint32_t minTimeout = 1000;
		uint32_t maxTimeout = 30;
		for (RudpAggregatorBuffer::ContextEntry *cur = aggrBuf->m_contextList; cur != 0; cur = cur->next) {
			uint32_t val = cur->aggrTimeoutMs;
			if (minTimeout > val) {
				minTimeout = (val > 30) ? val : 30;
			}
			if (maxTimeout < val) {
				maxTimeout = (val < 1000) ? val : 1000;
			}
		}
		aggrBuf->m_maxTimeoutMs = maxTimeout;
		m_timerIntervalMs = minTimeout >> 1;
	} else {
		if (aggrBuf != 0) {
			if (aggrBuf->m_storage != 0) {
				rudpFree(aggrBuf->m_storage);
			}
			delete aggrBuf;
		}
		m_entries.remove(node);
		delete node;
		if (m_entries.getCount() == 0 && (m_flags & 1) != 0) {
			m_flags &= ~1U;
			m_scheduler->stopTimer(m_handlerId);
		}
	}
}

RudpResult RudpAggregator::flushBuffer(const RudpAggregatorKey *key, RudpAggregatorBuffer *aggrBuf, uint64_t nowUsec)
{
	RudpResult res(0);
	uint32_t nowMs = (uint32_t)(nowUsec / 1000ULL);
	RudpBuffer view(aggrBuf->m_buf.m_data + aggrBuf->m_buf.m_offset, aggrBuf->m_buf.m_length);
	view.setLength(aggrBuf->m_buf.m_length);

	for (uint16_t i = 0; i < aggrBuf->m_packetCount; i++) {
		uint8_t firstByte = view.m_data[view.m_offset];
		uint16_t rawLenWord;
		rudpMemcpy(&rawLenWord, view.m_data + view.m_offset, 2);
		uint16_t totalPktLen = (uint16_t)(sceNetNtohs(rawLenWord) & 0x7FFU);
		uint8_t *hdrPtr = view.m_data + view.m_offset + 2 * (firstByte >> 6);

		uint16_t ackSeq = 0;
		RudpResult delayRes = RudpHeader::readAckDelay(hdrPtr, &ackSeq);
		res = delayRes;
		if (res.getCode() >= 0) {
			uint16_t updatedDelay = (uint16_t)(nowMs - (uint16_t)aggrBuf->m_packetEnqueuedMs[i] + res.getCode());
			RudpResult writeRes = RudpHeader::writeAckDelay(hdrPtr, updatedDelay);
			res = writeRes;
		}
		if (aggrBuf->m_packetTxTimestampOut[i] != 0) {
			*aggrBuf->m_packetTxTimestampOut[i] = nowMs;
		}
		view.getBytes(0, totalPktLen);
	}

	res = 0;
	if (m_delegate != 0) {
		res = m_delegate->onAggregatorSend(this, key->sock, &key->addr, &aggrBuf->m_buf);
	}
	aggrBuf->m_buf.reset();
	aggrBuf->m_startTimeUsec = nowUsec;
	aggrBuf->m_deadlineMs = aggrBuf->m_maxTimeoutMs;
	aggrBuf->m_packetCount = 0;
	return res;
}

RudpResult RudpAggregator::appendPacket(
	RudpAggregatorBuffer *aggrBuf,
	uint8_t muxMode,
	uint16_t srcVport,
	uint16_t dstVport,
	RudpHeader *header,
	RudpBuffer *payload,
	uint32_t nowMs,
	uint32_t *txTimestampOut)
{
	uint32_t startLen = aggrBuf->m_buf.m_length;
	uint16_t frameTag = 0;

	if (aggrBuf->m_buf.writeU16(0) < 0) {
		return RudpResult(rudpRecordError("rudpAggregator.cpp", 415));
	}
	switch (muxMode) {
	case SCE_RUDP_MUXMODE_NONE:
		frameTag = 0x4000U;
		break;
	case SCE_RUDP_MUXMODE_P2P:
		frameTag = 0x8000U;
		if (aggrBuf->m_buf.writeU16(srcVport) < 0) {
			return RudpResult(rudpRecordError("rudpAggregator.cpp", 425));
		}
		break;
	case SCE_RUDP_MUXMODE_CLIENT_SERVER:
		frameTag = 0xC000U;
		if (aggrBuf->m_buf.writeU16(srcVport) < 0) {
			return RudpResult(rudpRecordError("rudpAggregator.cpp", 431));
		}
		if (aggrBuf->m_buf.writeU16(dstVport) < 0) {
			return RudpResult(rudpRecordError("rudpAggregator.cpp", 433));
		}
		break;
	default:
		return RudpResult(rudpRecordError("rudpAggregator.cpp", 438));
	}

	RudpResult serRes = header->serialize(&aggrBuf->m_buf);
	if (serRes.getCode() < 0) {
		return RudpResult(rudpRecordError("rudpAggregator.cpp", 442));
	}
	if (aggrBuf->m_buf.putBytes(payload->m_data + payload->m_offset, payload->m_length) < 0) {
		return RudpResult(rudpRecordError("rudpAggregator.cpp", 444));
	}
	uint16_t frameWord = sceNetNtohs((uint16_t)((aggrBuf->m_buf.m_length - startLen) | frameTag));
	aggrBuf->m_buf.writeAt(startLen, &frameWord, sizeof(frameWord));
	uint16_t idx = aggrBuf->m_packetCount;
	aggrBuf->m_packetEnqueuedMs[idx] = nowMs;
	aggrBuf->m_packetTxTimestampOut[idx] = txTimestampOut;
	aggrBuf->m_packetCount = (uint16_t)(idx + 1);
	return RudpResult(0);
}

void RudpAggregator::enqueuePacket(
	const RudpAggregatorKey *key,
	int ctxId,
	uint8_t muxMode,
	uint16_t srcVport,
	uint16_t dstVport,
	RudpHeader *header,
	RudpBuffer *payload,
	uint32_t aggrDelayMs,
	uint32_t *txTimestampOut)
{
	(void)ctxId;
	if (key->sock < 0 || header == 0 || payload == 0) {
		return;
	}
	RudpAggregatorNode *node = (RudpAggregatorNode *)m_entries.find(key);
	if (node == 0) {
		return;
	}
	uint64_t nowUsec = rudpGetTimeUsec();
	RudpAggregatorBuffer *aggrBuf = node->m_buffer;
	if (aggrBuf->m_buf.m_length != 0) {
		uint32_t freeSpace = aggrBuf->m_buf.getRemainingSpace();
		if (freeSpace < payload->m_length + 2U * muxMode + 66U) {
			RudpResult flushRes = flushBuffer(key, aggrBuf, nowUsec);
			if (flushRes.getCode() < 0) {
				return;
			}
		}
	} else {
		aggrBuf->m_startTimeUsec = nowUsec;
		aggrBuf->m_deadlineMs = aggrBuf->m_maxTimeoutMs;
	}

	uint32_t elapsedMs = (uint32_t)((nowUsec - aggrBuf->m_startTimeUsec) / 1000ULL);
	uint32_t nowMs = (uint32_t)(nowUsec / 1000ULL);
	bool flushNow;
	if (aggrBuf->m_deadlineMs > elapsedMs) {
		if (aggrBuf->m_deadlineMs > elapsedMs + aggrDelayMs) {
			aggrBuf->m_deadlineMs = elapsedMs + aggrDelayMs;
		}
		flushNow = (aggrDelayMs == 0);
	} else {
		flushNow = true;
	}

	RudpResult addRes = appendPacket(aggrBuf, muxMode, srcVport, dstVport, header, payload, nowMs, txTimestampOut);
	if (addRes.getCode() >= 0 && (aggrBuf->m_packetCount >= RUDP_MAX_AGGREGATED_PACKETS || flushNow)) {
		RudpResult flushRes = flushBuffer(key, aggrBuf, nowUsec);
		flushRes.getCode();
	}
}

void RudpAggregator::invalidateContextTimestamps(const RudpAggregatorKey *key, uint16_t srcVport)
{
	if (key->sock < 0) {
		return;
	}
	RudpAggregatorNode *node = (RudpAggregatorNode *)m_entries.find(key);
	if (node == 0) {
		return;
	}
	RudpAggregatorBuffer *aggrBuf = node->m_buffer;
	if (aggrBuf->m_packetCount == 0) {
		return;
	}
	RudpBuffer view(aggrBuf->m_buf.m_data + aggrBuf->m_buf.m_offset, aggrBuf->m_buf.m_length);
	view.setLength(aggrBuf->m_buf.m_length);
	for (uint16_t i = 0; i < aggrBuf->m_packetCount; i++) {
		const uint8_t *ptr = view.m_data + view.m_offset;
		const uint16_t *words = (const uint16_t *)(const void *)ptr;
		uint8_t muxWordCount = ptr[0] >> 6;
		uint16_t totalPktLen = (uint16_t)(sceNetNtohs(words[0]) & 0x7FFU);
		if (muxWordCount != 0) {
			if (muxWordCount == 1) {
				aggrBuf->m_packetTxTimestampOut[i] = 0;
			} else if (sceNetNtohs(words[1]) == srcVport) {
				aggrBuf->m_packetTxTimestampOut[i] = 0;
			}
		}
		view.getBytes(0, totalPktLen);
	}
}

bool RudpAggregator::isValid() const
{
	return m_handlerId != 0;
}

void RudpAggregator::onEvent(const RudpEvent *ev)
{
	(void)ev;
}

void RudpAggregator::onTimer(uint64_t *nextIntervalUsec)
{
	uint64_t nowUsec = rudpGetTimeUsec();
	for (RudpAggregatorNode *node = (RudpAggregatorNode *)m_entries.first(0);
	     node != 0;
	     node = (RudpAggregatorNode *)m_entries.next(node, 0)) {
		RudpAggregatorBuffer *aggrBuf = node->m_buffer;
		if (aggrBuf->m_buf.m_length != 0) {
			if (aggrBuf->m_startTimeUsec + (uint64_t)aggrBuf->m_deadlineMs * 1000ULL < nowUsec) {
				RudpResult res = flushBuffer(&node->m_key, aggrBuf, nowUsec);
				res.getCode();
			}
		} else {
			aggrBuf->m_packetCount = 0;
		}
	}
	*nextIntervalUsec = (uint64_t)m_timerIntervalMs * 1000ULL;
}
