/*
 * rudp_protocol.cpp: RUDP reliable/ordered/stream/datagram transport protocol state machine.
 */
#include "rudp_internal.h"

RudpProtocol::RudpProtocol()
	: m_delegate(0),
	  m_localConnId(0),
	  m_remoteConnId(0),
	  m_mss(RUDP_DEFAULT_MSS),
	  m_sendBufSize(RUDP_DEFAULT_BUFFER_SIZE),
	  m_recvBufSize(RUDP_DEFAULT_BUFFER_SIZE),
	  m_packetPool(),
	  m_sendUnackedSeq(0),
	  m_sendNextSeq(0),
	  m_peerMaxWindow(0),
	  m_peerWindow(0),
	  m_cwnd(RUDP_DEFAULT_MSS),
	  m_ssthresh(RUDP_DEFAULT_BUFFER_SIZE),
	  m_fastRecoveryEndSeq(-1),
	  m_fastRecoveryHighSeq(0),
	  m_packetsToRead(0),
	  m_recvExpectedSeq(0),
	  m_recvHighestSeq(0),
	  m_recvReadSeq(0),
	  m_recvLastSeq(0),
	  m_sendHead(0),
	  m_sendTail(0),
	  m_sendTotalBytes(0),
	  m_sendQueuedBytes(0),
	  m_recvHead(0),
	  m_recvTail(0),
	  m_recvContiguousTail(0),
	  m_recvTotalBytes(0),
	  m_recvContiguousBytes(0),
	  m_recvUrgentCount(0),
	  m_readablePacketSize(0),
	  m_nextUrgentPacket(0),
	  m_dupAckCount(0),
	  m_retransmitCount(0),
	  m_unackedRecvCount(0),
	  m_connTimeoutMs(RUDP_DEFAULT_CONN_TIMEOUT_MS),
	  m_closeWaitTimeoutMs(0),
	  m_aggrTimeoutMs(RUDP_DEFAULT_AGGR_TIMEOUT_MS),
	  m_rttVarUsec(0),
	  m_srttUsec(0),
	  m_rtoUsec(RUDP_INITIAL_RTO_USEC),
	  m_rtoDeadlineUsec(0),
	  m_stateDeadlineUsec(0),
	  m_clockGranularityUsec(RUDP_MIN_CLOCK_GRANULARITY_USEC),
	  m_lastRecvSeqTimeUsec(0),
	  m_delayedAckStartUsec(0),
	  m_zeroWinProbeStartUsec(0),
	  m_zeroWinProbeDelayMs(0),
	  m_keepAliveIntervalMs(0),
	  m_keepAliveTimeoutMs(RUDP_DEFAULT_KEEPALIVE_TIMEOUT_MS),
	  m_keepAliveStartUsec(0),
	  m_keepAliveLastSentUsec(0),
	  m_keepAliveLastAckUsec(0),
	  m_keepAliveRetryCount(0),
	  m_keepAliveNextDelayUsec(0),
	  m_lastRxTimeUsec(0),
	  m_lastTxTimeUsec(0),
	  m_cookieData(0),
	  m_cookieLen(0),
	  m_state(SCE_RUDP_STATE_IDLE),
	  m_flags(RUDP_PROTO_DEFAULT_FLAGS)
{
	rudpMemset(&m_stats, 0, sizeof(m_stats));
}

int RudpProtocol::clearSendQueue()
{
	if (m_sendHead != 0 && m_delegate != 0) {
		m_delegate->onProtocolCancelAggregator(this);
	}
	RudpPacket *cur = m_sendHead;
	while (cur != 0) {
		RudpPacket *next = cur->m_next;
		m_packetPool.release(cur);
		m_sendHead = next;
		cur = next;
	}
	m_sendUnackedSeq = 0;
	m_peerMaxWindow = 0;
	m_peerWindow = 0;
	m_cwnd = RUDP_DEFAULT_MSS;
	m_ssthresh = RUDP_DEFAULT_BUFFER_SIZE;
	m_fastRecoveryEndSeq = -1;
	m_fastRecoveryHighSeq = 0;
	m_sendHead = 0;
	m_sendTail = 0;
	m_sendTotalBytes = 0;
	m_sendQueuedBytes = 0;
	m_dupAckCount = 0;
	return 0;
}

RudpProtocol::~RudpProtocol()
{
	clearSendQueue();
	RudpPacket *cur = m_recvHead;
	while (cur != 0) {
		RudpPacket *next = cur->m_next;
		m_packetPool.release(cur);
		m_recvHead = next;
		cur = next;
	}
	m_recvExpectedSeq = 0;
	m_recvHighestSeq = 0;
	m_recvLastSeq = 0;
	m_recvHead = 0;
	m_recvTail = 0;
	m_recvContiguousTail = 0;
	m_recvTotalBytes = 0;
	m_recvContiguousBytes = 0;
	m_recvUrgentCount = 0;
	m_readablePacketSize = 0;
	m_packetPool.clear();
	if (m_cookieData != 0) {
		rudpFree(m_cookieData);
		m_cookieData = 0;
	}
}

void RudpProtocol::setDelegate(IRudpProtocolDelegate *delegate)
{
	m_delegate = delegate;
}

RudpResult RudpProtocol::setMaxSegmentSize(uint32_t mss)
{
	if (m_state != SCE_RUDP_STATE_IDLE) {
		return RudpResult(SCE_RUDP_ERROR_NOT_ACCEPTABLE);
	}
	if (mss < RUDP_HEADER_OVERHEAD) {
		return RudpResult(SCE_RUDP_ERROR_INVALID_ARGUMENT);
	}
	uint32_t minBuf = (m_sendBufSize < m_recvBufSize) ? m_sendBufSize : m_recvBufSize;
	if (minBuf < mss) {
		return RudpResult(SCE_RUDP_ERROR_INVALID_ARGUMENT);
	}
	m_mss = mss;
	m_packetPool.m_packetCapacity = (uint16_t)(mss - RUDP_HEADER_OVERHEAD);
	return RudpResult(0);
}

uint32_t RudpProtocol::getMaxSegmentSize() const
{
	return (uint32_t)m_packetPool.m_packetCapacity + RUDP_HEADER_OVERHEAD;
}

RudpResult RudpProtocol::setSendBufferSize(uint32_t size)
{
	if (m_state != SCE_RUDP_STATE_IDLE) {
		return RudpResult(SCE_RUDP_ERROR_NOT_ACCEPTABLE);
	}
	if (size < (uint32_t)m_packetPool.m_packetCapacity + RUDP_HEADER_OVERHEAD) {
		return RudpResult(SCE_RUDP_ERROR_INVALID_ARGUMENT);
	}
	m_sendBufSize = size;
	return RudpResult(0);
}

uint32_t RudpProtocol::getSendBufferSize() const
{
	return m_sendBufSize;
}

RudpResult RudpProtocol::setRecvBufferSize(uint32_t size)
{
	if (m_state != SCE_RUDP_STATE_IDLE) {
		return RudpResult(SCE_RUDP_ERROR_NOT_ACCEPTABLE);
	}
	if (size < (uint32_t)m_packetPool.m_packetCapacity + RUDP_HEADER_OVERHEAD) {
		return RudpResult(SCE_RUDP_ERROR_INVALID_ARGUMENT);
	}
	m_recvBufSize = size;
	return RudpResult(0);
}

uint32_t RudpProtocol::getRecvBufferSize() const
{
	return m_recvBufSize;
}

RudpResult RudpProtocol::setConnectionTimeout(uint32_t ms)
{
	m_connTimeoutMs = ms;
	return RudpResult(0);
}

uint32_t RudpProtocol::getConnectionTimeout() const
{
	return m_connTimeoutMs;
}

RudpResult RudpProtocol::setCloseWaitTimeout(uint32_t ms)
{
	m_closeWaitTimeoutMs = ms;
	return RudpResult(0);
}

uint32_t RudpProtocol::getCloseWaitTimeout() const
{
	return m_closeWaitTimeoutMs;
}

RudpResult RudpProtocol::setAggregationTimeout(uint32_t ms)
{
	m_aggrTimeoutMs = ms;
	return RudpResult(0);
}

uint32_t RudpProtocol::getAggregationTimeout() const
{
	return m_aggrTimeoutMs;
}

RudpResult RudpProtocol::setStreamMode(int enable)
{
	RudpResult res(0);
	switch (enable) {
	case 0:
		m_flags &= ~RUDP_PROTO_FLAG_STREAM;
		break;
	case 1:
		m_flags |= RUDP_PROTO_FLAG_STREAM;
		break;
	default:
		res = SCE_RUDP_ERROR_INVALID_ARGUMENT;
		break;
	}
	return res;
}

bool RudpProtocol::getStreamMode() const
{
	return (m_flags & RUDP_PROTO_FLAG_STREAM) != 0;
}

RudpResult RudpProtocol::setDeliveryCritical(uint8_t enable)
{
	if (enable != 0) {
		m_flags |= RUDP_PROTO_FLAG_RELIABLE;
	} else {
		m_flags &= ~RUDP_PROTO_FLAG_RELIABLE;
	}
	return RudpResult(0);
}

bool RudpProtocol::getDeliveryCritical() const
{
	return (m_flags & RUDP_PROTO_FLAG_RELIABLE) != 0;
}

RudpResult RudpProtocol::setOrderCritical(uint8_t enable)
{
	if (enable != 0) {
		m_flags |= RUDP_PROTO_FLAG_ORDERED;
	} else {
		m_flags &= ~RUDP_PROTO_FLAG_ORDERED;
	}
	return RudpResult(0);
}

bool RudpProtocol::getOrderCritical() const
{
	return (m_flags & RUDP_PROTO_FLAG_ORDERED) != 0;
}

RudpResult RudpProtocol::setNoDelay(uint8_t enable)
{
	if (enable != 0) {
		m_flags |= RUDP_PROTO_FLAG_NODELAY;
	} else {
		m_flags &= ~RUDP_PROTO_FLAG_NODELAY;
	}
	return RudpResult(0);
}

bool RudpProtocol::getNoDelay() const
{
	return (m_flags & RUDP_PROTO_FLAG_NODELAY) != 0;
}

RudpResult RudpProtocol::setSackEnabled(uint8_t enable)
{
	if (enable != 0) {
		m_flags |= RUDP_PROTO_FLAG_SACK;
	} else {
		m_flags &= ~RUDP_PROTO_FLAG_SACK;
	}
	return RudpResult(0);
}

bool RudpProtocol::getSackEnabled() const
{
	return (m_flags & RUDP_PROTO_FLAG_SACK) != 0;
}

RudpResult RudpProtocol::setKeepAliveInterval(uint32_t ms)
{
	if (m_keepAliveIntervalMs != ms) {
		m_keepAliveIntervalMs = ms;
		m_keepAliveStartUsec = 0;
		m_keepAliveLastSentUsec = 0;
		m_keepAliveLastAckUsec = 0;
		m_keepAliveRetryCount = 0;
		m_keepAliveNextDelayUsec = 0;
	}
	return RudpResult(0);
}

uint32_t RudpProtocol::getKeepAliveInterval() const
{
	return m_keepAliveIntervalMs;
}

RudpResult RudpProtocol::setKeepAliveTimeout(uint32_t ms)
{
	m_keepAliveTimeoutMs = ms;
	return RudpResult(0);
}

uint32_t RudpProtocol::getKeepAliveTimeout() const
{
	return m_keepAliveTimeoutMs;
}

void RudpProtocol::setDebugFlag(uint8_t enable)
{
	if (enable != 0) {
		m_flags |= RUDP_PROTO_FLAG_DEBUG;
	} else {
		m_flags &= ~RUDP_PROTO_FLAG_DEBUG;
	}
}

bool RudpProtocol::getDebugFlag() const
{
	return (m_flags & RUDP_PROTO_FLAG_DEBUG) != 0;
}

int RudpProtocol::resetState(uint32_t flagMask)
{
	clearSendQueue();
	RudpPacket *cur = m_recvHead;
	while (cur != 0) {
		RudpPacket *next = cur->m_next;
		m_packetPool.release(cur);
		m_recvHead = next;
		cur = next;
	}
	m_recvExpectedSeq = 0;
	m_recvHighestSeq = 0;
	m_recvLastSeq = 0;
	m_recvHead = 0;
	m_recvTail = 0;
	m_recvContiguousTail = 0;
	m_recvTotalBytes = 0;
	m_recvContiguousBytes = 0;
	m_recvUrgentCount = 0;
	m_readablePacketSize = 0;
	m_packetPool.clear();
	if (m_cookieData != 0) {
		rudpFree(m_cookieData);
		m_cookieData = 0;
	}
	m_cookieLen = 0;
	m_remoteConnId = 0;
	m_retransmitCount = 0;
	m_rttVarUsec = 0;
	m_srttUsec = 0;
	m_rtoUsec = RUDP_INITIAL_RTO_USEC;
	m_rtoDeadlineUsec = 0;
	m_stateDeadlineUsec = 0;
	m_clockGranularityUsec = RUDP_MIN_CLOCK_GRANULARITY_USEC;
	m_delayedAckStartUsec = 0;
	m_zeroWinProbeStartUsec = 0;
	m_zeroWinProbeDelayMs = 0;
	m_keepAliveStartUsec = 0;
	m_keepAliveLastSentUsec = 0;
	m_keepAliveLastAckUsec = 0;
	m_keepAliveRetryCount = 0;
	m_keepAliveNextDelayUsec = 0;
	m_lastRxTimeUsec = 0;
	m_lastTxTimeUsec = 0;
	m_flags &= flagMask;
	return 0;
}

void RudpProtocol::resetConnectionIds(uint32_t flagMask)
{
	uint16_t randWords[16];
	resetState(flagMask);
	if (m_delegate != 0) {
		m_delegate->onProtocolGenerateRandom(this, (uint8_t *)randWords);
	}
	uint16_t lowId = randWords[1];
	uint16_t highId;
	if ((m_flags & RUDP_PROTO_FLAG_NEED_INITIAL_SEQ) != 0) {
		highId = randWords[0];
		m_flags &= ~RUDP_PROTO_FLAG_NEED_INITIAL_SEQ;
		m_sendNextSeq = randWords[2];
	} else {
		highId = (uint16_t)((m_localConnId >> 16) + 1);
	}
	m_localConnId = (uint32_t)lowId | ((uint32_t)highId << 16);
	m_sendUnackedSeq = m_sendNextSeq;
}

RudpResult RudpProtocol::activate()
{
	if (m_delegate == 0) {
		return RudpResult(SCE_RUDP_ERROR_NOT_ACCEPTABLE);
	}
	if (m_state == SCE_RUDP_STATE_ESTABLISHED || m_state == SCE_RUDP_STATE_CLOSE_WAIT) {
		return RudpResult(SCE_RUDP_ERROR_NOT_ACCEPTABLE);
	}
	if ((m_flags & RUDP_PROTO_FLAG_INITIATING) != 0) {
		return RudpResult(SCE_RUDP_ERROR_NOT_ACCEPTABLE);
	}
	if ((m_flags & RUDP_PROTO_FLAG_ACTIVATING) != 0) {
		return RudpResult(SCE_RUDP_ERROR_IN_PROGRESS);
	}
	resetConnectionIds(RUDP_PROTO_CONFIG_FLAGS_MASK);
	m_flags |= RUDP_PROTO_FLAG_ACTIVATING;
	m_state = SCE_RUDP_STATE_CLOSED;
	uint64_t now = rudpGetTimeUsec();
	m_stateDeadlineUsec = now + (uint64_t)m_connTimeoutMs * 1000ULL;
	m_retransmitCount = 0;
	uint64_t rto = (m_rtoUsec < RUDP_MAX_RTO_USEC) ? m_rtoUsec : RUDP_MAX_RTO_USEC;
	m_rtoDeadlineUsec = rto + rudpGetTimeUsec();
	return RudpResult(0);
}

RudpResult RudpProtocol::initiate()
{
	if (m_delegate == 0) {
		return RudpResult(SCE_RUDP_ERROR_NOT_ACCEPTABLE);
	}
	if (m_state == SCE_RUDP_STATE_ESTABLISHED || m_state == SCE_RUDP_STATE_CLOSE_WAIT) {
		return RudpResult(SCE_RUDP_ERROR_NOT_ACCEPTABLE);
	}
	if ((m_flags & RUDP_PROTO_FLAG_ACTIVATING) != 0) {
		return RudpResult(SCE_RUDP_ERROR_NOT_ACCEPTABLE);
	}
	if ((m_flags & RUDP_PROTO_FLAG_INITIATING) != 0) {
		return RudpResult(SCE_RUDP_ERROR_IN_PROGRESS);
	}
	resetConnectionIds(RUDP_PROTO_CONFIG_FLAGS_MASK);
	m_flags |= RUDP_PROTO_FLAG_INITIATING;
	m_state = SCE_RUDP_STATE_SYN_SENT;
	if (m_delegate != 0) {
		m_delegate->onProtocolScheduleSyn(this);
	}
	uint64_t now = rudpGetTimeUsec();
	m_stateDeadlineUsec = now + (uint64_t)m_connTimeoutMs * 1000ULL;
	m_retransmitCount = 0;
	uint64_t rto = (m_rtoUsec < RUDP_MAX_RTO_USEC) ? m_rtoUsec : RUDP_MAX_RTO_USEC;
	m_rtoDeadlineUsec = rto + rudpGetTimeUsec();
	return RudpResult(0);
}

int RudpProtocol::getSizeWritable() const
{
	if (m_state != SCE_RUDP_STATE_ESTABLISHED && m_state != SCE_RUDP_STATE_CLOSE_WAIT) {
		return SCE_RUDP_ERROR_NOT_ACCEPTABLE;
	}
	if (m_state == SCE_RUDP_STATE_CLOSE_WAIT) {
		return 0;
	}
	uint32_t maxPackets = m_sendBufSize;
	if (m_mss > RUDP_HEADER_OVERHEAD) {
		maxPackets = m_sendBufSize / (m_mss - RUDP_HEADER_OVERHEAD);
	}
	uint32_t usedPackets = m_packetPool.m_txInUse;
	if (usedPackets >= maxPackets) {
		return 0;
	}
	uint32_t freePackets = maxPackets - usedPackets;
	if (freePackets == 0) {
		return 0;
	}
	uint32_t maxPayload = m_mss - RUDP_HEADER_OVERHEAD;
	if ((m_flags & RUDP_PROTO_FLAG_STREAM) != 0 ||
	    ((m_flags & RUDP_PROTO_FLAG_RELIABLE) != 0 && (m_flags & RUDP_PROTO_FLAG_ORDERED) != 0)) {
		return (int)(maxPayload * freePackets);
	}
	return (int)maxPayload;
}

RudpResult RudpProtocol::enqueueWrite(RudpBuffer *src, uint8_t flags)
{
	uint32_t remaining = src->m_length;
	int withTimestamp = flags & SCE_RUDP_MSG_WITH_TX_TIMESTAMP;
	uint32_t freePackets = 0;
	if (m_state == SCE_RUDP_STATE_ESTABLISHED) {
		uint32_t maxPackets = m_sendBufSize;
		if (m_mss > RUDP_HEADER_OVERHEAD) {
			maxPackets = m_sendBufSize / (m_mss - RUDP_HEADER_OVERHEAD);
		}
		if (m_packetPool.m_txInUse < maxPackets) {
			freePackets = maxPackets - m_packetPool.m_txInUse;
		}
	}

	if ((m_flags & RUDP_PROTO_FLAG_STREAM) != 0) {
		RudpPacket *tail = m_sendTail;
		if (tail != 0 && (tail->m_flags & RUDP_QPKT_SENT) == 0) {
			uint32_t tailUsed = tail->m_buf.m_length;
			if (m_mss > tailUsed + RUDP_HEADER_OVERHEAD) {
				uint32_t space = m_mss - tailUsed - RUDP_HEADER_OVERHEAD;
				uint32_t take = (remaining < space) ? remaining : space;
				tail->m_buf.putBytes(src->m_data + src->m_offset, take);
				src->getBytes(0, take);
				m_sendTotalBytes += take;
				m_sendQueuedBytes += take;
				remaining = src->m_length;
				if (withTimestamp != 0) {
					tail->m_flags |= RUDP_QPKT_HAS_TIMESTAMP;
				}
			}
		}
	}

	RudpPacket *pkt = 0;
	while (remaining != 0 && freePackets != 0) {
		if (pkt != 0 && (m_flags & RUDP_PROTO_FLAG_STREAM) == 0) {
			pkt->m_flags |= RUDP_QPKT_MORE_FRAGMENTS;
		}
		pkt = m_packetPool.acquire(1);
		if (pkt == 0) {
			return RudpResult(SCE_RUDP_ERROR_MEMORY);
		}
		freePackets--;
		uint32_t maxPayload = m_mss - RUDP_HEADER_OVERHEAD;
		if ((m_flags & RUDP_PROTO_FLAG_STREAM) != 0) {
			pkt->m_flags |= RUDP_QPKT_MORE_FRAGMENTS;
		}
		uint32_t take = (src->m_length < maxPayload) ? src->m_length : maxPayload;
		pkt->m_next = 0;
		pkt->m_buf.putBytes(src->m_data + src->m_offset, take);
		src->getBytes(0, take);

		if ((flags & SCE_RUDP_MSG_LATENCY_CRITICAL) != 0) {
			pkt->m_flags |= RUDP_QPKT_LATENCY_CRITICAL;
		}
		if ((flags & SCE_RUDP_MSG_ALIGN_32) != 0) {
			pkt->m_flags |= RUDP_QPKT_ALIGN_32;
		}
		if ((flags & SCE_RUDP_MSG_ALIGN_64) != 0) {
			pkt->m_flags |= RUDP_QPKT_ALIGN_64;
		}
		if (withTimestamp != 0) {
			pkt->m_flags |= RUDP_QPKT_HAS_TIMESTAMP;
		}

		if ((pkt->m_flags & RUDP_QPKT_LATENCY_CRITICAL) == 0 ||
		    (m_flags & RUDP_PROTO_FLAG_STREAM) != 0 ||
		    (m_flags & RUDP_PROTO_FLAG_ORDERED) != 0) {
			if (m_sendTail != 0) {
				m_sendTail->m_next = pkt;
			} else {
				m_sendHead = pkt;
			}
			m_sendTail = pkt;
		} else {
			RudpPacket *cur = m_sendHead;
			RudpPacket *prev = 0;
			while (cur != 0 && ((cur->m_flags & RUDP_QPKT_SENT) != 0 ||
			                    (cur->m_flags & RUDP_QPKT_LATENCY_CRITICAL) != 0)) {
				prev = cur;
				cur = cur->m_next;
			}
			if (prev != 0) {
				prev->m_next = pkt;
				if (cur != 0) {
					pkt->m_next = cur;
				} else {
					m_sendTail = pkt;
				}
			} else if (m_sendTail != 0) {
				pkt->m_next = m_sendHead;
				m_sendHead = pkt;
			} else {
				m_sendHead = pkt;
				m_sendTail = pkt;
			}
		}

		m_sendTotalBytes += take;
		m_sendQueuedBytes += take;
		remaining = src->m_length;
	}
	return RudpResult(0);
}

RudpResult RudpProtocol::write(RudpBuffer *src, int flags)
{
	if (m_state != SCE_RUDP_STATE_ESTABLISHED) {
		return RudpResult(SCE_RUDP_ERROR_NOT_ACCEPTABLE);
	}
	uint32_t totalLen = src->m_length;
	if (totalLen == 0) {
		return RudpResult(SCE_RUDP_ERROR_INVALID_ARGUMENT);
	}
	int writable = getSizeWritable();
	if (writable == 0) {
		return RudpResult(SCE_RUDP_ERROR_WOULDBLOCK);
	}
	if (writable < 0) {
		return RudpResult(writable);
	}
	if ((m_flags & RUDP_PROTO_FLAG_STREAM) == 0) {
		if ((m_flags & RUDP_PROTO_FLAG_RELIABLE) != 0 && (m_flags & RUDP_PROTO_FLAG_ORDERED) != 0) {
			uint32_t peerCap = (m_mss - RUDP_HEADER_OVERHEAD) * (uint32_t)m_peerMaxWindow;
			if (totalLen > peerCap) {
				return RudpResult(SCE_RUDP_ERROR_MSG_TOO_LARGE);
			}
		} else if (totalLen > m_mss - RUDP_HEADER_OVERHEAD) {
			return RudpResult(SCE_RUDP_ERROR_MSG_TOO_LARGE);
		}
		if (totalLen > (uint32_t)writable) {
			m_flags |= RUDP_PROTO_FLAG_WRITE_BLOCKED;
			return RudpResult(SCE_RUDP_ERROR_WOULDBLOCK);
		}
	}

	RudpResult enqRes = enqueueWrite(src, (uint8_t)flags);
	if (enqRes.getCode() < 0) {
		return enqRes;
	}

	if (m_peerWindow == 0 && m_sendQueuedBytes != 0 && m_zeroWinProbeDelayMs == 0) {
		m_zeroWinProbeStartUsec = rudpGetTimeUsec();
		m_zeroWinProbeDelayMs = RUDP_ZERO_WINDOW_PROBE_INIT_MS;
	}
	if ((m_flags & RUDP_PROTO_FLAG_SEND_SCHEDULED) == 0 && m_delegate != 0) {
		m_flags |= RUDP_PROTO_FLAG_SEND_SCHEDULED;
		m_delegate->onProtocolScheduleData(this);
	}
	if (getSizeWritable() == 0) {
		m_flags |= RUDP_PROTO_FLAG_WRITE_BLOCKED;
	}

	uint32_t written = totalLen - src->m_length;
	int qualityIdx;
	if ((m_flags & RUDP_PROTO_FLAG_RELIABLE) != 0 && (m_flags & RUDP_PROTO_FLAG_ORDERED) != 0) {
		qualityIdx = 0;
	} else if ((m_flags & RUDP_PROTO_FLAG_RELIABLE) != 0) {
		qualityIdx = 1;
	} else if ((m_flags & RUDP_PROTO_FLAG_ORDERED) != 0) {
		qualityIdx = 2;
	} else {
		qualityIdx = 3;
	}

	g_rudpStatus->m_data.sentUserPackets++;
	g_rudpStatus->m_data.sentUserBytes += written;
	if ((flags & SCE_RUDP_MSG_LATENCY_CRITICAL) != 0 || (m_flags & RUDP_PROTO_FLAG_NODELAY) != 0) {
		g_rudpStatus->m_data.sentLatencyCriticalPackets++;
	}
	switch (qualityIdx) {
	case 0:
		g_rudpStatus->m_data.sentQualityLevel1Bytes += written;
		g_rudpStatus->m_data.sentQualityLevel1Packets++;
		break;
	case 1:
		g_rudpStatus->m_data.sentQualityLevel2Bytes += written;
		g_rudpStatus->m_data.sentQualityLevel2Packets++;
		break;
	case 2:
		g_rudpStatus->m_data.sentQualityLevel3Bytes += written;
		g_rudpStatus->m_data.sentQualityLevel3Packets++;
		break;
	default:
		g_rudpStatus->m_data.sentQualityLevel4Bytes += written;
		g_rudpStatus->m_data.sentQualityLevel4Packets++;
		break;
	}
	return RudpResult((int)written);
}

int RudpProtocol::getSizeReadable() const
{
	if (m_state != SCE_RUDP_STATE_ESTABLISHED && m_state != SCE_RUDP_STATE_CLOSE_WAIT) {
		return SCE_RUDP_ERROR_NOT_ACCEPTABLE;
	}
	uint32_t size = ((m_flags & RUDP_PROTO_FLAG_STREAM) != 0) ? m_recvContiguousBytes : m_readablePacketSize;
	if (size == 0) {
		return (m_state == SCE_RUDP_STATE_ESTABLISHED) ? 0 : SCE_RUDP_ERROR_END_OF_DATA;
	}
	return (int)size;
}

void RudpProtocol::updateReadableSize()
{
	if (m_state != SCE_RUDP_STATE_ESTABLISHED && m_state != SCE_RUDP_STATE_CLOSE_WAIT) {
		m_readablePacketSize = 0;
		m_flags &= ~RUDP_PROTO_FLAG_READABLE_NOTIFIED;
		return;
	}
	RudpPacket *cur = m_recvHead;
	if (cur == 0) {
		m_readablePacketSize = 0;
		return;
	}
	if (m_readablePacketSize != 0) {
		return;
	}

	uint32_t readySize = 0;
	if ((m_flags & RUDP_PROTO_FLAG_STREAM) != 0) {
		if (m_recvContiguousBytes != 0) {
			m_readablePacketSize = m_recvContiguousBytes;
			readySize = m_recvContiguousBytes;
		}
	} else if ((m_flags & RUDP_PROTO_FLAG_RELIABLE) != 0) {
		if ((m_flags & RUDP_PROTO_FLAG_ORDERED) != 0) {
			if (m_recvContiguousBytes != 0) {
				uint32_t msgLen = 0;
				while (cur != 0) {
					msgLen += cur->m_buf.m_length;
					if ((cur->m_flags & RUDP_QPKT_MORE_FRAGMENTS) == 0) {
						m_readablePacketSize = msgLen;
						readySize = msgLen;
						break;
					}
					if (cur == m_recvContiguousTail) {
						break;
					}
					cur = cur->m_next;
				}
			}
		} else if (m_recvUrgentCount != 0) {
			while (cur != 0) {
				if ((cur->m_flags & RUDP_QPKT_LATENCY_CRITICAL) != 0 &&
				    (cur->m_flags & RUDP_QPKT_SENT) == 0) {
					readySize = cur->m_buf.m_length;
					m_readablePacketSize = readySize;
					m_nextUrgentPacket = cur;
					cur->m_flags |= RUDP_QPKT_SACKED;
					break;
				}
				cur = cur->m_next;
			}
		} else if (m_recvContiguousBytes != 0) {
			while (cur != 0) {
				if ((cur->m_flags & RUDP_QPKT_SENT) == 0) {
					readySize = cur->m_buf.m_length;
					m_readablePacketSize = readySize;
					break;
				}
				if (cur == m_recvContiguousTail) {
					break;
				}
				cur = cur->m_next;
			}
		}
	} else if ((m_flags & RUDP_PROTO_FLAG_ORDERED) != 0) {
		readySize = cur->m_buf.m_length;
		m_readablePacketSize = readySize;
	} else if (m_recvUrgentCount != 0) {
		while (cur != 0) {
			if ((cur->m_flags & RUDP_QPKT_LATENCY_CRITICAL) != 0 &&
			    (cur->m_flags & RUDP_QPKT_SENT) == 0) {
				readySize = cur->m_buf.m_length;
				m_readablePacketSize = readySize;
				m_nextUrgentPacket = cur;
				cur->m_flags |= RUDP_QPKT_SACKED;
				break;
			}
			cur = cur->m_next;
		}
	} else {
		readySize = cur->m_buf.m_length;
		m_readablePacketSize = readySize;
	}

	if ((m_flags & RUDP_PROTO_FLAG_STREAM) != 0) {
		readySize = m_recvContiguousBytes;
	}
	if ((int)readySize > 0 && (m_flags & RUDP_PROTO_FLAG_READABLE_NOTIFIED) == 0 && m_delegate != 0) {
		m_delegate->onProtocolReadable(this);
	}
}

RudpResult RudpProtocol::dequeueRead(RudpBuffer *dst, SceRudpReadInfo *info, bool *outLatencyCritical)
{
	dst->reset();
	uint32_t freeSpace = dst->getRemainingSpace();
	uint32_t toRead;
	if ((m_flags & RUDP_PROTO_FLAG_STREAM) != 0) {
		toRead = (m_recvContiguousBytes < freeSpace) ? m_recvContiguousBytes : freeSpace;
	} else {
		if (freeSpace < m_readablePacketSize) {
			return RudpResult(SCE_RUDP_ERROR_BUFFER_TOO_SMALL);
		}
		toRead = m_readablePacketSize;
	}

	while (toRead != 0 && m_recvHead != 0) {
		RudpPacket *pkt = m_recvHead;
		if (m_nextUrgentPacket != 0) {
			if ((m_flags & RUDP_PROTO_FLAG_STREAM) != 0 || (m_flags & RUDP_PROTO_FLAG_ORDERED) != 0) {
				m_nextUrgentPacket = 0;
				return RudpResult(0);
			}
			pkt = m_nextUrgentPacket;
		}
		uint32_t take = (pkt->m_buf.m_length < toRead) ? pkt->m_buf.m_length : toRead;
		if ((int16_t)(m_recvReadSeq - pkt->m_seq) <= 0) {
			m_recvReadSeq = (uint16_t)(pkt->m_seq + 1);
		}
		dst->putBytes(pkt->m_buf.m_data + pkt->m_buf.m_offset, take);
		pkt->m_buf.getBytes(0, take);
		toRead -= take;
		m_recvTotalBytes -= take;
		if ((m_flags & RUDP_PROTO_FLAG_STREAM) != 0 || (m_flags & RUDP_PROTO_FLAG_RELIABLE) != 0) {
			m_recvContiguousBytes -= take;
		}
		if (m_nextUrgentPacket != 0) {
			m_nextUrgentPacket = 0;
			pkt->m_flags = (uint16_t)((pkt->m_flags & ~(RUDP_QPKT_SENT | RUDP_QPKT_SACKED)) | RUDP_QPKT_SENT);
			if (m_recvUrgentCount != 0) {
				m_recvUrgentCount--;
			}
		}
		if (info != 0) {
			uint32_t retryInfo = pkt->m_retryInfo;
			info->retransmissionDelay = (uint16_t)(retryInfo & 0xFFFFU);
			info->retransmissionCount = (uint8_t)(retryInfo >> 24);
			info->retransmissionDelay2 = (uint8_t)((retryInfo >> 16) & 0xFFU);
			if ((pkt->m_flags & RUDP_QPKT_LATENCY_CRITICAL) != 0) {
				info->flags |= SCE_RUDP_MSG_LATENCY_CRITICAL;
			}
			info->sequenceNumber = pkt->m_seq;
			if ((pkt->m_flags & RUDP_QPKT_HAS_TIMESTAMP) != 0) {
				info->flags |= SCE_RUDP_MSG_WITH_TX_TIMESTAMP;
				info->timestamp = pkt->m_timestamp;
			}
		}
		if ((pkt->m_flags & RUDP_QPKT_LATENCY_CRITICAL) != 0) {
			*outLatencyCritical = true;
		}

		RudpPacket *head = m_recvHead;
		if (head->m_buf.m_length == 0) {
			RudpPacket *next = head->m_next;
			m_recvHead = next;
			if (next == 0) {
				m_recvTotalBytes = 0;
				m_recvTail = 0;
				if ((m_flags & RUDP_PROTO_FLAG_STREAM) != 0 || (m_flags & RUDP_PROTO_FLAG_RELIABLE) != 0) {
					m_recvContiguousBytes = 0;
					m_recvContiguousTail = 0;
				}
			}
			if (((m_flags & RUDP_PROTO_FLAG_STREAM) != 0 || (m_flags & RUDP_PROTO_FLAG_RELIABLE) != 0) &&
			    head == m_recvContiguousTail) {
				m_recvContiguousTail = 0;
				m_recvContiguousBytes = 0;
			}
			m_packetPool.release(head);
		} else if (pkt->m_buf.m_length == 0) {
			if (pkt->m_next != 0) {
				RudpPacket *next = pkt->m_next;
				rudpMemcpy(pkt, next, sizeof(RudpPacket));
				m_packetPool.release(next);
			} else {
				RudpPacket *prev = head;
				for (RudpPacket *scan = head->m_next; scan != pkt; scan = scan->m_next) {
					prev = scan;
				}
				prev->m_next = 0;
				m_recvTail = prev;
				m_packetPool.release(pkt);
			}
		}
	}
	m_packetsToRead--;
	return RudpResult(0);
}

RudpResult RudpProtocol::read(RudpBuffer *dst, SceRudpReadInfo *info)
{
	if (info != 0) {
		rudpMemset(info, 0, sizeof(SceRudpReadInfo));
	}
	if (m_state != SCE_RUDP_STATE_ESTABLISHED && m_state != SCE_RUDP_STATE_CLOSE_WAIT) {
		return RudpResult(SCE_RUDP_ERROR_NOT_ACCEPTABLE);
	}
	int avail = ((m_flags & RUDP_PROTO_FLAG_STREAM) != 0) ? (int)m_recvContiguousBytes : (int)m_readablePacketSize;
	if (avail == 0) {
		return RudpResult((m_state == SCE_RUDP_STATE_ESTABLISHED) ? SCE_RUDP_ERROR_WOULDBLOCK : SCE_RUDP_ERROR_END_OF_DATA);
	}
	if (avail < 0) {
		return RudpResult(avail);
	}
	if (dst->m_capacity - dst->m_offset == dst->m_length) {
		return RudpResult(SCE_RUDP_ERROR_BUFFER_TOO_SMALL);
	}

	bool latencyCritical = false;
	RudpResult deqRes = dequeueRead(dst, info, &latencyCritical);
	if (deqRes.getCode() < 0) {
		return deqRes;
	}

	uint32_t readBytes = dst->m_length;
	int qualityIdx;
	if ((m_flags & RUDP_PROTO_FLAG_RELIABLE) != 0 && (m_flags & RUDP_PROTO_FLAG_ORDERED) != 0) {
		qualityIdx = 0;
	} else if ((m_flags & RUDP_PROTO_FLAG_RELIABLE) != 0) {
		qualityIdx = 1;
	} else if ((m_flags & RUDP_PROTO_FLAG_ORDERED) != 0) {
		qualityIdx = 2;
	} else {
		qualityIdx = 3;
	}

	g_rudpStatus->m_data.rcvdUserPackets++;
	g_rudpStatus->m_data.rcvdUserBytes += readBytes;
	if (latencyCritical) {
		g_rudpStatus->m_data.rcvdLatencyCriticalPackets++;
	}
	switch (qualityIdx) {
	case 0:
		g_rudpStatus->m_data.rcvdQualityLevel1Bytes += readBytes;
		g_rudpStatus->m_data.rcvdQualityLevel1Packets++;
		break;
	case 1:
		g_rudpStatus->m_data.rcvdQualityLevel2Bytes += readBytes;
		g_rudpStatus->m_data.rcvdQualityLevel2Packets++;
		break;
	case 2:
		g_rudpStatus->m_data.rcvdQualityLevel3Bytes += readBytes;
		g_rudpStatus->m_data.rcvdQualityLevel3Packets++;
		break;
	default:
		g_rudpStatus->m_data.rcvdQualityLevel4Bytes += readBytes;
		g_rudpStatus->m_data.rcvdQualityLevel4Packets++;
		break;
	}

	m_readablePacketSize = 0;
	m_flags &= ~RUDP_PROTO_FLAG_READABLE_NOTIFIED;
	updateReadableSize();
	return RudpResult((int)readBytes);
}

void RudpProtocol::recordFailedConnection(int reason)
{
	if (reason == RUDP_REASON_KEEP_ALIVE_FAILURE) {
		return;
	}
	g_rudpStatus->m_data.failedConnections++;
	switch (reason) {
	case RUDP_REASON_RESET:
		g_rudpStatus->m_data.failedConnectionsReset++;
		break;
	case RUDP_REASON_REFUSED:
		g_rudpStatus->m_data.failedConnectionsRefused++;
		break;
	case RUDP_REASON_TIMEOUT:
		g_rudpStatus->m_data.failedConnectionsTimeout++;
		break;
	case RUDP_REASON_VERSION_MISMATCH:
		g_rudpStatus->m_data.failedConnectionsVersionMismatch++;
		break;
	case RUDP_REASON_TRANSPORT_TYPE_MISMATCH:
		g_rudpStatus->m_data.failedConnectionsTransportTypeMismatch++;
		break;
	case RUDP_REASON_QUALITY_LEVEL_MISMATCH:
		g_rudpStatus->m_data.failedConnectionsQualityLevelMismatch++;
		break;
	default:
		break;
	}
}

void RudpProtocol::transmitPacket(RudpBuffer *buf, RudpHeader *header, uint8_t immediate, uint32_t *txTimestampOut)
{
	m_lastTxTimeUsec = rudpGetTimeUsec();
	m_stats.sentPackets++;
	m_stats.sentBytes += buf->m_length;

	switch (header->m_type) {
	case RUDP_PKT_DATA:
		g_rudpStatus->m_data.sentUsrPackets++;
		break;
	case RUDP_PKT_PING:
		g_rudpStatus->m_data.sentPrbPackets++;
		break;
	case RUDP_PKT_SYN:
		g_rudpStatus->m_data.sentSynPackets++;
		break;
	case RUDP_PKT_RST:
		g_rudpStatus->m_data.sentRstPackets++;
		break;
	}

	if (m_delegate != 0) {
		RudpTransmitRequest req;
		req.protocol = this;
		req.header = header;
		req.payload = buf;
		req.txTimestampOut = txTimestampOut;
		if (immediate != 0 || (header->m_flags & RUDP_HDR_FLAG_LATENCY_CRITICAL) != 0 ||
		    (m_flags & RUDP_PROTO_FLAG_NODELAY) != 0) {
			req.aggrDelayMs = 0;
		} else {
			req.aggrDelayMs = m_aggrTimeoutMs;
		}
		m_delegate->onProtocolTransmit(&req);
	}
}

void RudpProtocol::sendReset(uint32_t connId, uint8_t reason)
{
	uint8_t rawBuf[64];
	m_sendNextSeq++;
	RudpBuffer buf(rawBuf, sizeof(rawBuf));
	RudpHeader hdr;
	hdr.m_type = RUDP_PKT_RST;
	hdr.m_seq = 0;
	hdr.encodeWindow(0);
	hdr.m_connectionId = connId;
	hdr.m_rstReason = reason;
	transmitPacket(&buf, &hdr, 1, 0);
}

int RudpProtocol::mapReasonToWire(int reason)
{
	switch (reason) {
	case 0: return 0;
	case 1:
	case 8: return 1;
	case 2: return 2;
	case 3: return 3;
	case 4: return 4;
	case 5: return 5;
	case 6: return 6;
	case 7: return 7;
	default: return 255;
	}
}

void RudpProtocol::enterCloseOrReset(int reason, int sendRstMode)
{
	uint32_t prevState = m_state;
	if (prevState == SCE_RUDP_STATE_IDLE || prevState == SCE_RUDP_STATE_CLOSE_WAIT) {
		return;
	}
	clearSendQueue();
	if (sendRstMode != 0) {
		uint8_t wireReason = (uint8_t)mapReasonToWire(reason);
		if (sendRstMode == 1) {
			if (m_delegate != 0) {
				m_delegate->onProtocolScheduleRst(this, m_localConnId, wireReason);
			}
		} else {
			sendReset(m_localConnId, wireReason);
		}
	}

	if (prevState == SCE_RUDP_STATE_ESTABLISHED) {
		m_state = SCE_RUDP_STATE_CLOSE_WAIT;
		if (reason != 0 && m_delegate != 0) {
			m_delegate->onProtocolDisconnected(this, (uint8_t)reason);
		}
	} else {
		recordFailedConnection(reason);
		if (prevState == SCE_RUDP_STATE_SYN_SENT || prevState == SCE_RUDP_STATE_SYN_RCVD) {
			if ((m_flags & RUDP_PROTO_FLAG_INITIATING) != 0) {
				m_state = SCE_RUDP_STATE_CLOSE_WAIT;
				if (reason != 0 && m_delegate != 0) {
					m_delegate->onProtocolConnectFailed(this, (uint8_t)reason);
				}
			} else {
				m_state = SCE_RUDP_STATE_CLOSED;
				resetState(RUDP_PROTO_CONFIG_FLAGS_MASK);
			}
		} else {
			if ((m_flags & RUDP_PROTO_FLAG_ACTIVATING) != 0) {
				if (m_delegate != 0) {
					m_delegate->onProtocolConnectFailed(this, (uint8_t)reason);
				}
			} else if (m_delegate != 0) {
				m_delegate->onProtocolClosed(this);
			}
			resetState(RUDP_PROTO_CONFIG_FLAGS_MASK);
		}
	}

	if (m_state == SCE_RUDP_STATE_CLOSE_WAIT) {
		uint64_t now = rudpGetTimeUsec();
		m_stateDeadlineUsec = now + (uint64_t)m_closeWaitTimeoutMs * 1000ULL;
		uint64_t rto = (m_rtoUsec < RUDP_MAX_RTO_USEC) ? m_rtoUsec : RUDP_MAX_RTO_USEC;
		m_rtoDeadlineUsec = rto + rudpGetTimeUsec();
	}
	m_flags &= ~(RUDP_PROTO_FLAG_INITIATING | RUDP_PROTO_FLAG_ACTIVATING);
}

RudpResult RudpProtocol::terminate()
{
	if (m_state <= SCE_RUDP_STATE_CLOSED) {
		return RudpResult(SCE_RUDP_ERROR_NOT_ACCEPTABLE);
	}
	if (m_state != SCE_RUDP_STATE_CLOSE_WAIT) {
		enterCloseOrReset(RUDP_REASON_NORMAL, 1);
	}
	return RudpResult(0);
}

RudpResult RudpProtocol::flush()
{
	if (m_state != SCE_RUDP_STATE_ESTABLISHED) {
		return RudpResult(SCE_RUDP_ERROR_NOT_ACCEPTABLE);
	}
	if ((m_flags & RUDP_PROTO_FLAG_FLUSH_PENDING) != 0) {
		return RudpResult(SCE_RUDP_ERROR_IN_PROGRESS);
	}
	if (m_sendTail != 0) {
		m_flags |= RUDP_PROTO_FLAG_FLUSH_PENDING;
		m_sendTail->m_flags |= RUDP_QPKT_FLUSH_MARKER;
	} else if (m_delegate != 0) {
		m_delegate->onProtocolFlushed(this);
	}
	return RudpResult(0);
}

void RudpProtocol::sendPing()
{
	uint8_t rawBuf[64];
	RudpBuffer buf(rawBuf, sizeof(rawBuf));
	RudpHeader hdr;
	hdr.m_seq = m_sendNextSeq;
	hdr.m_type = RUDP_PKT_PING;

	uint16_t maxRxPkts = (uint16_t)m_recvBufSize;
	if (m_mss > RUDP_HEADER_OVERHEAD) {
		maxRxPkts = (uint16_t)(m_recvBufSize / (m_mss - RUDP_HEADER_OVERHEAD));
	}
	uint16_t win = (maxRxPkts > m_packetPool.m_rxInUse) ? (uint16_t)(maxRxPkts - m_packetPool.m_rxInUse) : 0;
	hdr.encodeWindow(win);
	transmitPacket(&buf, &hdr, 0, 0);
}

void RudpProtocol::checkKeepAlive()
{
	uint64_t now = rudpGetTimeUsec();
	if (m_keepAliveIntervalMs == 0) {
		return;
	}
	if (m_keepAliveStartUsec != 0) {
		if (m_keepAliveLastAckUsec > m_keepAliveStartUsec) {
			m_keepAliveStartUsec = 0;
			m_keepAliveLastSentUsec = 0;
			m_keepAliveLastAckUsec = 0;
			m_keepAliveRetryCount = 0;
			m_keepAliveNextDelayUsec = 0;
			return;
		}
		if (m_keepAliveStartUsec + (uint64_t)m_keepAliveTimeoutMs * 1000ULL < now) {
			enterCloseOrReset(RUDP_REASON_KEEP_ALIVE_FAILURE, 1);
			return;
		}
		if (m_keepAliveLastSentUsec + m_keepAliveNextDelayUsec < now) {
			uint64_t sentAt = rudpGetTimeUsec();
			sendPing();
			uint32_t retries = m_keepAliveRetryCount;
			uint64_t delay = m_rtoUsec << retries;
			if (m_keepAliveStartUsec == 0) {
				m_keepAliveStartUsec = sentAt;
			}
			m_keepAliveLastSentUsec = sentAt;
			m_keepAliveNextDelayUsec = (delay < RUDP_MAX_RTO_USEC) ? delay : RUDP_MAX_RTO_USEC;
			m_keepAliveRetryCount = retries + 1;
		}
	} else {
		uint64_t intervalUsec = (uint64_t)m_keepAliveIntervalMs * 1000ULL;
		if (m_lastRxTimeUsec + intervalUsec < now || m_lastTxTimeUsec + intervalUsec < now) {
			uint64_t sentAt = rudpGetTimeUsec();
			sendPing();
			uint32_t retries = m_keepAliveRetryCount;
			uint64_t delay = m_rtoUsec << retries;
			if (m_keepAliveStartUsec == 0) {
				m_keepAliveStartUsec = sentAt;
			}
			m_keepAliveLastSentUsec = sentAt;
			m_keepAliveNextDelayUsec = (delay < RUDP_MAX_RTO_USEC) ? delay : RUDP_MAX_RTO_USEC;
			m_keepAliveRetryCount = retries + 1;
		}
	}
}

void RudpProtocol::fillSackBlocks(RudpHeader *header)
{
	if (m_recvHead == 0) {
		return;
	}
	if (m_recvContiguousTail != 0 && m_recvContiguousTail == m_recvTail) {
		return;
	}
	RudpPacket *cur = (m_recvContiguousTail != 0) ? m_recvContiguousTail->m_next : m_recvHead;
	if (cur == 0) {
		return;
	}
	RudpSackBlock block;
	block.start = cur->m_seq;
	while (cur != 0 && header->getSackCount() < RUDP_MAX_SACK_BLOCKS) {
		RudpPacket *next = cur->m_next;
		if (next != 0) {
			if (next->m_seq != (uint16_t)(cur->m_seq + 1)) {
				block.end = (uint16_t)(cur->m_seq + 1);
				header->addSackBlock(&block);
				block.start = next->m_seq;
			}
		} else {
			block.end = (uint16_t)(cur->m_seq + 1);
			header->addSackBlock(&block);
		}
		cur = next;
	}
}

void RudpProtocol::sendAck()
{
	uint8_t rawBuf[64];
	m_unackedRecvCount = 0;
	m_delayedAckStartUsec = 0;
	m_flags &= ~RUDP_PROTO_FLAG_ACK_SCHEDULED;

	RudpBuffer buf(rawBuf, sizeof(rawBuf));
	RudpHeader hdr;

	if (m_state == SCE_RUDP_STATE_SYN_SENT || m_state == SCE_RUDP_STATE_SYN_RCVD) {
		hdr.m_type = RUDP_PKT_SYN;
		if ((m_flags & RUDP_PROTO_FLAG_STREAM) != 0) {
			hdr.m_synQuality = 1;
			hdr.m_synCaps |= (RUDP_SYN_CAP_STREAM | RUDP_SYN_CAP_RELIABLE | RUDP_SYN_CAP_ORDERED);
		} else {
			if ((m_flags & RUDP_PROTO_FLAG_RELIABLE) != 0) {
				hdr.m_synQuality = 1;
				hdr.m_synCaps |= RUDP_SYN_CAP_RELIABLE;
			} else {
				hdr.m_synQuality = 0;
			}
			if ((m_flags & RUDP_PROTO_FLAG_ORDERED) != 0) {
				hdr.m_synCaps |= RUDP_SYN_CAP_ORDERED;
			}
		}
		hdr.m_connectionId = m_localConnId;
		hdr.m_mss = (uint16_t)(m_packetPool.m_packetCapacity + RUDP_HEADER_OVERHEAD);
		hdr.m_extFlags |= RUDP_EXT_MSS;
	} else {
		hdr.m_type = RUDP_PKT_DATA;
	}

	uint16_t maxRxPkts = (uint16_t)m_recvBufSize;
	if (m_mss > RUDP_HEADER_OVERHEAD) {
		maxRxPkts = (uint16_t)(m_recvBufSize / (m_mss - RUDP_HEADER_OVERHEAD));
	}
	uint16_t win = (maxRxPkts > m_packetPool.m_rxInUse) ? (uint16_t)(maxRxPkts - m_packetPool.m_rxInUse) : 0;
	hdr.encodeWindow(win);

	hdr.m_seq = m_sendNextSeq;
	hdr.m_flags |= RUDP_HDR_FLAG_ACK;
	hdr.m_ack = m_recvExpectedSeq;
	if ((m_flags & RUDP_PROTO_FLAG_SACK) != 0) {
		fillSackBlocks(&hdr);
	}
	if (hdr.m_type == RUDP_PKT_DATA) {
		if (m_recvLastSeq == m_recvHighestSeq && (uint16_t)(m_recvHighestSeq + 1) == m_recvExpectedSeq) {
			uint64_t now = rudpGetTimeUsec();
			hdr.m_ackDelayMs = (uint16_t)((now - m_lastRecvSeqTimeUsec) / 1000ULL);
			hdr.m_flags |= RUDP_HDR_FLAG_ACK_DELAY;
		}
	}
	transmitPacket(&buf, &hdr, 0, 0);
}

void RudpProtocol::sendData()
{
	RudpHeader hdr;
	m_flags &= ~RUDP_PROTO_FLAG_SEND_SCHEDULED;
	if (m_sendTotalBytes == 0) {
		return;
	}

	uint16_t winBase = ((m_sendHead->m_flags & RUDP_QPKT_SENT) != 0) ? m_sendUnackedSeq : m_sendNextSeq;
	uint16_t winLimit = (uint16_t)(winBase + m_peerWindow);
	uint64_t nowUsec = rudpGetTimeUsec();
	uint32_t nowMs = (uint32_t)(nowUsec / 1000ULL);
	RudpPacket *cur = m_sendHead;
	uint32_t inFlightBytes = 0;

	while (cur != 0) {
		if (m_sendQueuedBytes == 0) {
			break;
		}
		while (cur != 0) {
			if ((cur->m_flags & RUDP_QPKT_SENT) == 0 || (cur->m_flags & RUDP_QPKT_RETRANSMIT) != 0) {
				break;
			}
			if ((cur->m_flags & RUDP_QPKT_SACKED) == 0) {
				inFlightBytes += cur->m_buf.m_length;
			}
			cur = cur->m_next;
		}
		if (cur == 0) {
			break;
		}
		if ((cur->m_flags & RUDP_QPKT_RETRANSMIT) != 0 && (cur->m_flags & RUDP_QPKT_SACKED) != 0) {
			cur->m_flags &= ~RUDP_QPKT_RETRANSMIT;
			cur = cur->m_next;
			continue;
		}
		if (m_fastRecoveryEndSeq >= 0 && (cur->m_flags & RUDP_QPKT_SENT) == 0) {
			break;
		}
		uint16_t pktSeq = ((cur->m_flags & RUDP_QPKT_SENT) != 0) ? cur->m_seq : m_sendNextSeq;
		if ((int16_t)(pktSeq - winLimit) >= 0) {
			break;
		}
		inFlightBytes += cur->m_buf.m_length;
		if (((m_flags & RUDP_PROTO_FLAG_STREAM) != 0 || (m_flags & RUDP_PROTO_FLAG_RELIABLE) != 0) &&
		    inFlightBytes >= m_cwnd) {
			break;
		}

		hdr.reset();
		hdr.m_type = RUDP_PKT_DATA;
		uint32_t *txTimestampOut = 0;
		if ((cur->m_flags & RUDP_QPKT_SENT) != 0) {
			uint8_t retries = (uint8_t)(cur->m_retryInfo >> 24);
			if (retries != 255) {
				retries++;
			}
			hdr.m_seq = cur->m_seq;
			uint32_t delayField = ((cur->m_flags & RUDP_QPKT_RETRANSMITTED) != 0)
				? 0xFFFFFFU
				: ((nowMs - cur->m_timestamp) & 0xFFFFFFU);
			uint32_t retryInfo = ((uint32_t)retries << 24) | delayField;
			cur->m_retryInfo = retryInfo;
			hdr.m_optFlags |= RUDP_OPT_RETRY_INFO;
			hdr.m_retryInfo = retryInfo;
		} else {
			uint16_t seq = m_sendNextSeq++;
			cur->m_seq = seq;
			if ((m_flags & RUDP_PROTO_FLAG_STREAM) != 0 || (m_flags & RUDP_PROTO_FLAG_RELIABLE) != 0) {
				cur->m_timestamp = nowMs;
				txTimestampOut = &cur->m_timestamp;
			}
			hdr.m_seq = seq;
		}
		if ((cur->m_flags & RUDP_QPKT_HAS_TIMESTAMP) != 0) {
			hdr.m_txTimestamp = nowMs;
			hdr.m_optFlags |= RUDP_OPT_TX_TIMESTAMP;
		}

		uint16_t maxRxPkts = (uint16_t)m_recvBufSize;
		if (m_mss > RUDP_HEADER_OVERHEAD) {
			maxRxPkts = (uint16_t)(m_recvBufSize / (m_mss - RUDP_HEADER_OVERHEAD));
		}
		uint16_t win = (maxRxPkts > m_packetPool.m_rxInUse) ? (uint16_t)(maxRxPkts - m_packetPool.m_rxInUse) : 0;
		hdr.encodeWindow(win);

		if (m_unackedRecvCount != 0 && (m_flags & RUDP_PROTO_FLAG_ACK_SCHEDULED) == 0) {
			hdr.m_ack = m_recvExpectedSeq;
			hdr.m_flags |= RUDP_HDR_FLAG_ACK;
			if ((m_flags & RUDP_PROTO_FLAG_SACK) != 0) {
				fillSackBlocks(&hdr);
			}
			if (hdr.m_type == RUDP_PKT_DATA) {
				if (m_recvLastSeq == m_recvHighestSeq &&
				    m_recvExpectedSeq == (uint16_t)(m_recvHighestSeq + 1)) {
					uint64_t t = rudpGetTimeUsec();
					hdr.m_ackDelayMs = (uint16_t)((t - m_lastRecvSeqTimeUsec) / 1000ULL);
					hdr.m_flags |= RUDP_HDR_FLAG_ACK_DELAY;
				}
			}
			m_unackedRecvCount = 0;
			m_delayedAckStartUsec = 0;
		}

		if ((cur->m_flags & RUDP_QPKT_LATENCY_CRITICAL) != 0) {
			hdr.m_flags |= RUDP_HDR_FLAG_LATENCY_CRITICAL;
		}
		if ((cur->m_flags & RUDP_QPKT_MORE_FRAGMENTS) != 0) {
			hdr.m_flags |= RUDP_HDR_FLAG_MORE_FRAGMENTS;
		}
		int alignBits = ((cur->m_flags & RUDP_QPKT_ALIGN_32) != 0 ? 1 : 0) +
		                ((cur->m_flags & RUDP_QPKT_ALIGN_64) != 0 ? 2 : 0);
		if (alignBits != 0) {
			hdr.setPaddingAlignment((uint8_t)(1U << (alignBits + 1)));
		}
		if (m_zeroWinProbeDelayMs != 0) {
			m_zeroWinProbeStartUsec = nowUsec;
			m_zeroWinProbeDelayMs = RUDP_ZERO_WINDOW_PROBE_INIT_MS;
		}

		transmitPacket(&cur->m_buf, &hdr, 0, txTimestampOut);
		cur->m_flags = (uint16_t)((cur->m_flags & ~(RUDP_QPKT_SENT | RUDP_QPKT_RETRANSMIT)) | RUDP_QPKT_SENT);

		RudpPacket *next = cur->m_next;
		if ((m_flags & RUDP_PROTO_FLAG_STREAM) != 0 || (m_flags & RUDP_PROTO_FLAG_RELIABLE) != 0) {
			m_sendQueuedBytes -= cur->m_buf.m_length;
			cur = next;
		} else {
			RudpPacket *sent = cur;
			m_sendTotalBytes -= sent->m_buf.m_length;
			m_sendQueuedBytes -= sent->m_buf.m_length;
			m_sendUnackedSeq = sent->m_seq;
			if (sent == m_sendHead) {
				m_sendHead = next;
				if (next == 0) {
					m_sendTail = 0;
					m_sendQueuedBytes = 0;
				}
			}
			if ((m_flags & RUDP_PROTO_FLAG_FLUSH_PENDING) != 0 &&
			    (sent->m_flags & RUDP_QPKT_FLUSH_MARKER) != 0) {
				m_flags &= ~RUDP_PROTO_FLAG_FLUSH_PENDING;
				if (m_delegate != 0) {
					m_delegate->onProtocolFlushed(this);
				}
			}
			m_packetPool.release(sent);
			if ((m_flags & RUDP_PROTO_FLAG_WRITE_BLOCKED) != 0 && getSizeWritable() > 0) {
				m_flags &= ~RUDP_PROTO_FLAG_WRITE_BLOCKED;
				if (m_delegate != 0) {
					m_delegate->onProtocolWritable(this);
				}
			}
			cur = next;
		}
	}

	if (m_rtoDeadlineUsec == 0) {
		m_retransmitCount = 0;
		if (m_sendTotalBytes != 0) {
			uint64_t rto = (m_rtoUsec < RUDP_MAX_RTO_USEC) ? m_rtoUsec : RUDP_MAX_RTO_USEC;
			m_rtoDeadlineUsec = rto + rudpGetTimeUsec();
		} else {
			m_rtoDeadlineUsec = 0;
		}
	}
}

void RudpProtocol::sendSyn()
{
	uint8_t rawBuf[64];
	RudpBuffer buf(rawBuf, sizeof(rawBuf));
	RudpHeader hdr;
	hdr.m_seq = m_sendNextSeq;
	hdr.m_type = RUDP_PKT_SYN;

	uint16_t maxRxPkts = (uint16_t)m_recvBufSize;
	if (m_mss > RUDP_HEADER_OVERHEAD) {
		maxRxPkts = (uint16_t)(m_recvBufSize / (m_mss - RUDP_HEADER_OVERHEAD));
	}
	uint16_t win = (maxRxPkts > m_packetPool.m_rxInUse) ? (uint16_t)(maxRxPkts - m_packetPool.m_rxInUse) : 0;
	hdr.encodeWindow(win);

	if ((m_flags & RUDP_PROTO_FLAG_STREAM) != 0) {
		hdr.m_synQuality = 1;
		hdr.m_synCaps |= (RUDP_SYN_CAP_STREAM | RUDP_SYN_CAP_RELIABLE | RUDP_SYN_CAP_ORDERED);
	} else {
		if ((m_flags & RUDP_PROTO_FLAG_RELIABLE) != 0) {
			hdr.m_synQuality = 1;
			hdr.m_synCaps |= RUDP_SYN_CAP_RELIABLE;
		} else {
			hdr.m_synQuality = 0;
		}
		if ((m_flags & RUDP_PROTO_FLAG_ORDERED) != 0) {
			hdr.m_synCaps |= RUDP_SYN_CAP_ORDERED;
		}
	}
	hdr.m_connectionId = m_localConnId;
	hdr.m_mss = (uint16_t)(m_packetPool.m_packetCapacity + RUDP_HEADER_OVERHEAD);
	hdr.m_extFlags |= RUDP_EXT_MSS;
	if (m_cookieData != 0 && m_cookieLen != 0) {
		hdr.setCookie(m_cookieData, m_cookieLen);
	}
	transmitPacket(&buf, &hdr, 0, 0);
}

void RudpProtocol::handleRetransmitTimeout()
{
	switch (m_state) {
	case SCE_RUDP_STATE_CLOSED:
		if ((m_flags & RUDP_PROTO_FLAG_ACTIVATING) != 0) {
			if (m_stateDeadlineUsec != 0 && rudpGetTimeUsec() >= m_stateDeadlineUsec) {
				enterCloseOrReset(RUDP_REASON_TIMEOUT, 0);
			} else {
				m_retransmitCount++;
				uint64_t rto = m_rtoUsec << m_retransmitCount;
				uint64_t clamped = (rto < RUDP_MAX_RTO_USEC) ? rto : RUDP_MAX_RTO_USEC;
				m_rtoDeadlineUsec = clamped + rudpGetTimeUsec();
			}
		}
		break;
	case SCE_RUDP_STATE_SYN_SENT:
		if (m_stateDeadlineUsec != 0 && rudpGetTimeUsec() >= m_stateDeadlineUsec) {
			enterCloseOrReset(RUDP_REASON_TIMEOUT, 2);
		} else {
			m_retransmitCount++;
			sendSyn();
			uint64_t rto = m_rtoUsec << m_retransmitCount;
			uint64_t clamped = (rto < RUDP_MAX_RTO_USEC) ? rto : RUDP_MAX_RTO_USEC;
			m_rtoDeadlineUsec = clamped + rudpGetTimeUsec();
		}
		break;
	case SCE_RUDP_STATE_SYN_RCVD:
		if (m_stateDeadlineUsec != 0 && rudpGetTimeUsec() >= m_stateDeadlineUsec) {
			enterCloseOrReset(RUDP_REASON_TIMEOUT, 2);
		} else {
			m_retransmitCount++;
			sendAck();
			uint64_t rto = m_rtoUsec << m_retransmitCount;
			uint64_t clamped = (rto < RUDP_MAX_RTO_USEC) ? rto : RUDP_MAX_RTO_USEC;
			m_rtoDeadlineUsec = clamped + rudpGetTimeUsec();
		}
		break;
	case SCE_RUDP_STATE_ESTABLISHED: {
		uint32_t inFlight = (m_sendTotalBytes - m_sendQueuedBytes) >> 1;
		uint32_t minThresh = m_mss * 2;
		m_cwnd = m_mss;
		m_ssthresh = (inFlight > minThresh) ? inFlight : minThresh;
		if ((m_flags & RUDP_PROTO_FLAG_STREAM) != 0 || (m_flags & RUDP_PROTO_FLAG_RELIABLE) != 0) {
			for (RudpPacket *cur = m_sendHead; cur != 0; cur = cur->m_next) {
				if ((cur->m_flags & RUDP_QPKT_SENT) == 0) {
					break;
				}
				if ((cur->m_flags & RUDP_QPKT_SACKED) == 0 && (cur->m_flags & RUDP_QPKT_RETRANSMIT) == 0) {
					m_stats.retransmissions++;
					g_rudpStatus->m_data.retransmittedPackets++;
					m_sendQueuedBytes += cur->m_buf.m_length;
					cur->m_flags |= RUDP_QPKT_RETRANSMIT;
				}
			}
		}
		m_retransmitCount++;
		m_dupAckCount = 0;
		m_fastRecoveryEndSeq = -1;
		uint64_t rto = m_rtoUsec << m_retransmitCount;
		uint64_t clamped = (rto < RUDP_MAX_RTO_USEC) ? rto : RUDP_MAX_RTO_USEC;
		m_rtoDeadlineUsec = clamped + rudpGetTimeUsec();
		if (((m_flags & RUDP_PROTO_FLAG_STREAM) != 0 || (m_flags & RUDP_PROTO_FLAG_RELIABLE) != 0) &&
		    (m_flags & RUDP_PROTO_FLAG_SEND_SCHEDULED) == 0) {
			sendData();
		}
		break;
	}
	case SCE_RUDP_STATE_CLOSE_WAIT:
		if (m_stateDeadlineUsec >= rudpGetTimeUsec()) {
			uint64_t clamped = (m_rtoUsec < RUDP_MAX_RTO_USEC) ? m_rtoUsec : RUDP_MAX_RTO_USEC;
			m_rtoDeadlineUsec = clamped + rudpGetTimeUsec();
		} else {
			m_rtoDeadlineUsec = 0;
			m_retransmitCount = 0;
			m_state = SCE_RUDP_STATE_CLOSED;
			if (m_delegate != 0) {
				m_delegate->onProtocolClosed(this);
			}
		}
		break;
	default:
		break;
	}
}

void RudpProtocol::onTimerTick(uint64_t *nextIntervalUsec)
{
	uint64_t now = rudpGetTimeUsec();
	bool closeWaitExpired = (m_state == SCE_RUDP_STATE_CLOSE_WAIT &&
	                         m_stateDeadlineUsec != 0 &&
	                         m_stateDeadlineUsec < now);
	if ((m_rtoDeadlineUsec != 0 && m_rtoDeadlineUsec < now) || closeWaitExpired) {
		m_rtoDeadlineUsec = 0;
		handleRetransmitTimeout();
	}

	uint64_t gran;
	if (m_srttUsec != 0) {
		gran = m_srttUsec >> 1;
		if (gran < RUDP_MIN_CLOCK_GRANULARITY_USEC) {
			gran = RUDP_MIN_CLOCK_GRANULARITY_USEC;
		}
	} else {
		gran = RUDP_DEFAULT_TIMER_INTERVAL_USEC;
	}
	m_clockGranularityUsec = gran;
	*nextIntervalUsec = gran;

	if (m_state == SCE_RUDP_STATE_ESTABLISHED) {
		if ((m_flags & RUDP_PROTO_FLAG_SEND_SCHEDULED) == 0) {
			sendData();
		}
		if (m_unackedRecvCount != 0 && (m_flags & RUDP_PROTO_FLAG_ACK_SCHEDULED) == 0 &&
		    m_delayedAckStartUsec + RUDP_DELAYED_ACK_USEC < now) {
			sendAck();
		}
		if (m_zeroWinProbeDelayMs != 0 &&
		    m_zeroWinProbeStartUsec + (uint64_t)m_zeroWinProbeDelayMs * 1000ULL < now) {
			sendPing();
			m_zeroWinProbeStartUsec = now;
			uint32_t nextDelay = m_zeroWinProbeDelayMs * 2;
			m_zeroWinProbeDelayMs = (nextDelay < RUDP_ZERO_WINDOW_PROBE_MAX_MS)
				? nextDelay : RUDP_ZERO_WINDOW_PROBE_MAX_MS;
		}
		checkKeepAlive();
	}
}

void RudpProtocol::sendPingAck()
{
	uint8_t rawBuf[64];
	RudpBuffer buf(rawBuf, sizeof(rawBuf));
	RudpHeader hdr;
	hdr.m_seq = m_sendNextSeq;
	hdr.m_type = RUDP_PKT_PING;

	uint16_t maxRxPkts = (uint16_t)m_recvBufSize;
	if (m_mss > RUDP_HEADER_OVERHEAD) {
		maxRxPkts = (uint16_t)(m_recvBufSize / (m_mss - RUDP_HEADER_OVERHEAD));
	}
	uint16_t win = (maxRxPkts > m_packetPool.m_rxInUse) ? (uint16_t)(maxRxPkts - m_packetPool.m_rxInUse) : 0;
	hdr.encodeWindow(win);

	hdr.m_flags |= RUDP_HDR_FLAG_ACK;
	hdr.m_ack = m_recvExpectedSeq;
	if ((m_flags & RUDP_PROTO_FLAG_SACK) != 0) {
		fillSackBlocks(&hdr);
	}
	if (hdr.m_type == RUDP_PKT_DATA) {
		if (m_recvLastSeq == m_recvHighestSeq &&
		    (uint16_t)(m_recvHighestSeq + 1) == m_recvExpectedSeq) {
			uint64_t now = rudpGetTimeUsec();
			hdr.m_ackDelayMs = (uint16_t)((now - m_lastRecvSeqTimeUsec) / 1000ULL);
			hdr.m_flags |= RUDP_HDR_FLAG_ACK_DELAY;
		}
	}
	transmitPacket(&buf, &hdr, 0, 0);
}

void RudpProtocol::handlePingPacket(RudpHeader *header)
{
	switch (m_state) {
	case SCE_RUDP_STATE_ESTABLISHED:
		if ((header->m_flags & RUDP_HDR_FLAG_ACK) == 0 && m_delegate != 0) {
			m_delegate->onProtocolSchedulePing(this);
		}
		break;
	case SCE_RUDP_STATE_CLOSE_WAIT:
		if (m_delegate != 0) {
			m_delegate->onProtocolScheduleRst(this, m_localConnId, 1);
		}
		break;
	default:
		break;
	}
}

void RudpProtocol::handleCookieChallenge(RudpHeader *header)
{
	const uint8_t *cookiePtr = 0;
	uint32_t cookieLen = 0;
	if ((header->m_extFlags & RUDP_EXT_COOKIE) != 0) {
		header->getCookie(&cookiePtr, &cookieLen);
		if (cookiePtr != 0 && cookieLen != 0 && cookieLen <= RUDP_MAX_COOKIE_SIZE) {
			if (m_cookieData == 0) {
				m_cookieData = (uint8_t *)rudpAlloc(cookieLen);
			}
			if (m_cookieData != 0) {
				m_cookieLen = cookieLen;
				rudpMemcpy(m_cookieData, cookiePtr, cookieLen);
				if (m_delegate != 0) {
					m_delegate->onProtocolScheduleSyn(this);
				}
				return;
			}
		}
	}
	enterCloseOrReset(RUDP_REASON_RESET, 0);
}

void RudpProtocol::handleResetPacket(RudpHeader *header)
{
	switch (m_state) {
	case SCE_RUDP_STATE_SYN_SENT:
		handleCookieChallenge(header);
		break;
	case SCE_RUDP_STATE_SYN_RCVD:
		if (header->m_connectionId == m_remoteConnId) {
			m_state = SCE_RUDP_STATE_CLOSED;
			recordFailedConnection(RUDP_REASON_RESET);
			if (m_delegate != 0) {
				m_delegate->onProtocolConnectFailed(this, RUDP_REASON_RESET);
			}
		}
		break;
	case SCE_RUDP_STATE_ESTABLISHED:
	case SCE_RUDP_STATE_CLOSE_WAIT:
		if (header->m_connectionId == m_remoteConnId) {
			enterCloseOrReset(RUDP_REASON_RESET, 0);
		}
		break;
	default:
		break;
	}
}

void RudpProtocol::completeHandshakeFromSynRcvd(RudpHeader *header)
{
	uint16_t initSeq = (uint16_t)(header->m_seq + 1);
	m_recvExpectedSeq = initSeq;
	m_recvHighestSeq = initSeq;
	m_recvLastSeq = initSeq;
	m_recvReadSeq = initSeq;
	m_peerMaxWindow = header->decodeWindow();
	uint32_t peerMss = header->m_mss;
	if (peerMss < RUDP_HEADER_OVERHEAD) {
		peerMss = RUDP_HEADER_OVERHEAD;
	}
	m_mss = (peerMss < RUDP_DEFAULT_MSS) ? peerMss : RUDP_DEFAULT_MSS;
	m_sendUnackedSeq = m_sendNextSeq;
	m_rtoDeadlineUsec = 0;
	m_retransmitCount = 0;
	g_rudpStatus->m_data.establishedConnections++;
	if (m_delegate != 0) {
		m_delegate->onProtocolConnected(this);
	}
}

void RudpProtocol::completeHandshakeFromSynSent(RudpHeader *header)
{
	uint16_t initSeq = (uint16_t)(header->m_seq + 1);
	m_recvExpectedSeq = initSeq;
	m_recvHighestSeq = initSeq;
	m_recvLastSeq = initSeq;
	m_recvReadSeq = initSeq;
	m_peerMaxWindow = header->decodeWindow();
	uint32_t peerMss = header->m_mss;
	if (peerMss < RUDP_HEADER_OVERHEAD) {
		peerMss = RUDP_HEADER_OVERHEAD;
	}
	if (peerMss < m_mss) {
		m_mss = peerMss;
	}
	if ((m_flags & RUDP_PROTO_FLAG_ACK_SCHEDULED) == 0 && m_delegate != 0) {
		m_flags |= RUDP_PROTO_FLAG_ACK_SCHEDULED;
		m_delegate->onProtocolScheduleAck(this);
	}
	m_rtoDeadlineUsec = 0;
	m_retransmitCount = 0;
	m_sendUnackedSeq = m_sendNextSeq;
	g_rudpStatus->m_data.establishedConnections++;
	if (m_delegate != 0) {
		m_delegate->onProtocolConnected(this);
	}
}

void RudpProtocol::handleSimultaneousSyn(RudpHeader *header)
{
	uint16_t initSeq = (uint16_t)(header->m_seq + 1);
	m_recvExpectedSeq = initSeq;
	m_recvHighestSeq = initSeq;
	m_recvLastSeq = initSeq;
	m_recvReadSeq = initSeq;
	m_peerMaxWindow = header->decodeWindow();
	uint32_t peerMss = header->m_mss;
	if (peerMss < RUDP_HEADER_OVERHEAD) {
		peerMss = RUDP_HEADER_OVERHEAD;
	}
	if (peerMss < m_mss) {
		m_mss = peerMss;
	}
	if ((m_flags & RUDP_PROTO_FLAG_ACK_SCHEDULED) == 0 && m_delegate != 0) {
		m_flags |= RUDP_PROTO_FLAG_ACK_SCHEDULED;
		m_delegate->onProtocolScheduleAck(this);
	}
	m_retransmitCount = 0;
	uint64_t rto = (m_rtoUsec < RUDP_MAX_RTO_USEC) ? m_rtoUsec : RUDP_MAX_RTO_USEC;
	m_rtoDeadlineUsec = rto + rudpGetTimeUsec();
}

void RudpProtocol::handleIncomingSyn(RudpHeader *header)
{
	uint16_t initSeq = (uint16_t)(header->m_seq + 1);
	m_recvExpectedSeq = initSeq;
	m_recvHighestSeq = initSeq;
	m_recvLastSeq = initSeq;
	m_recvReadSeq = initSeq;
	m_peerMaxWindow = header->decodeWindow();
	uint32_t peerMss = header->m_mss;
	if (peerMss < RUDP_HEADER_OVERHEAD) {
		peerMss = RUDP_HEADER_OVERHEAD;
	}
	uint32_t localMax = (uint32_t)m_packetPool.m_packetCapacity + RUDP_HEADER_OVERHEAD;
	m_mss = (peerMss < localMax) ? peerMss : localMax;
	if ((m_flags & RUDP_PROTO_FLAG_ACK_SCHEDULED) == 0 && m_delegate != 0) {
		m_flags |= RUDP_PROTO_FLAG_ACK_SCHEDULED;
		m_delegate->onProtocolScheduleAck(this);
	}
	uint64_t now = rudpGetTimeUsec();
	m_retransmitCount = 0;
	m_stateDeadlineUsec = now + (uint64_t)m_connTimeoutMs * 1000ULL;
	uint64_t rto = (m_rtoUsec < RUDP_MAX_RTO_USEC) ? m_rtoUsec : RUDP_MAX_RTO_USEC;
	m_rtoDeadlineUsec = rto + rudpGetTimeUsec();
}

void RudpProtocol::handleSynPacket(RudpHeader *header)
{
	if (header->m_minVersion > 0x0100U || header->m_maxVersion < 0x0100U) {
		enterCloseOrReset(RUDP_REASON_VERSION_MISMATCH, 1);
		return;
	}
	if ((m_flags & RUDP_PROTO_FLAG_STREAM) != 0) {
		if ((header->m_synCaps & RUDP_SYN_CAP_STREAM) == 0) {
			enterCloseOrReset(RUDP_REASON_TRANSPORT_TYPE_MISMATCH, 1);
			return;
		}
	} else {
		if ((header->m_synCaps & RUDP_SYN_CAP_STREAM) != 0) {
			enterCloseOrReset(RUDP_REASON_TRANSPORT_TYPE_MISMATCH, 1);
			return;
		}
		bool localReliable = (m_flags & RUDP_PROTO_FLAG_RELIABLE) != 0;
		bool peerReliable = (header->m_synCaps & RUDP_SYN_CAP_RELIABLE) != 0;
		bool localOrdered = (m_flags & RUDP_PROTO_FLAG_ORDERED) != 0;
		bool peerOrdered = (header->m_synCaps & RUDP_SYN_CAP_ORDERED) != 0;
		if (localReliable != peerReliable || localOrdered != peerOrdered) {
			enterCloseOrReset(RUDP_REASON_QUALITY_LEVEL_MISMATCH, 1);
			return;
		}
	}

	switch (m_state) {
	case SCE_RUDP_STATE_CLOSED:
		if ((header->m_flags & RUDP_HDR_FLAG_ACK) != 0) {
			if (m_delegate != 0) {
				m_delegate->onProtocolScheduleRst(this, m_localConnId, 1);
			}
		} else {
			resetConnectionIds(0xFFFFFF80U);
			m_state = SCE_RUDP_STATE_SYN_RCVD;
			m_remoteConnId = header->m_connectionId;
			handleIncomingSyn(header);
		}
		break;
	case SCE_RUDP_STATE_SYN_SENT:
		if ((header->m_flags & RUDP_HDR_FLAG_ACK) != 0) {
			m_state = SCE_RUDP_STATE_ESTABLISHED;
			m_remoteConnId = header->m_connectionId;
			completeHandshakeFromSynSent(header);
		} else {
			m_state = SCE_RUDP_STATE_SYN_RCVD;
			m_remoteConnId = header->m_connectionId;
			handleSimultaneousSyn(header);
		}
		break;
	case SCE_RUDP_STATE_SYN_RCVD:
		if (header->m_connectionId != m_remoteConnId) {
			enterCloseOrReset(RUDP_REASON_RESET, 0);
		} else if ((header->m_flags & RUDP_HDR_FLAG_ACK) != 0) {
			m_remoteConnId = header->m_connectionId;
			m_state = SCE_RUDP_STATE_ESTABLISHED;
			completeHandshakeFromSynRcvd(header);
		} else {
			if ((m_flags & RUDP_PROTO_FLAG_ACK_SCHEDULED) == 0 && m_delegate != 0) {
				m_flags |= RUDP_PROTO_FLAG_ACK_SCHEDULED;
				m_delegate->onProtocolScheduleAck(this);
			}
			uint64_t rto = (m_rtoUsec < RUDP_MAX_RTO_USEC) ? m_rtoUsec : RUDP_MAX_RTO_USEC;
			m_rtoDeadlineUsec = rto + rudpGetTimeUsec();
		}
		break;
	case SCE_RUDP_STATE_ESTABLISHED:
		if (header->m_connectionId != m_remoteConnId) {
			enterCloseOrReset(RUDP_REASON_RESET, 0);
		} else if ((header->m_flags & RUDP_HDR_FLAG_ACK) != 0) {
			if ((m_flags & RUDP_PROTO_FLAG_ACK_SCHEDULED) == 0 && m_delegate != 0) {
				m_flags |= RUDP_PROTO_FLAG_ACK_SCHEDULED;
				m_delegate->onProtocolScheduleAck(this);
			}
		}
		break;
	case SCE_RUDP_STATE_CLOSE_WAIT:
		if (m_delegate != 0) {
			m_delegate->onProtocolScheduleRst(this, m_localConnId, 1);
		}
		break;
	default:
		break;
	}
}

void RudpProtocol::rebuildContiguousTail()
{
	RudpPacket *head = m_recvHead;
	if (head == 0) {
		m_recvTail = 0;
		m_recvContiguousTail = 0;
		m_recvContiguousBytes = 0;
		return;
	}
	if ((int16_t)(m_recvExpectedSeq - head->m_seq) >= 0) {
		m_recvContiguousTail = head;
		uint32_t bytes = head->m_buf.m_length;
		m_recvContiguousBytes = bytes;
		RudpPacket *cur = head;
		for (RudpPacket *next = head->m_next; next != 0; next = next->m_next) {
			if ((int16_t)(cur->m_seq - next->m_seq + 1) < 0) {
				break;
			}
			cur = next;
			m_recvContiguousTail = next;
			bytes += next->m_buf.m_length;
			m_recvContiguousBytes = bytes;
		}
	} else {
		m_recvContiguousTail = 0;
		m_recvContiguousBytes = 0;
	}
}

void RudpProtocol::advanceContiguousTail(uint8_t hasRetryInfo)
{
	RudpPacket *cur = m_recvContiguousTail;
	if (cur == 0) {
		return;
	}
	RudpPacket *next = cur->m_next;
	bool advanced = false;
	while (next != 0) {
		if ((int16_t)(cur->m_seq - next->m_seq + 1) < 0) {
			break;
		}
		m_recvContiguousTail = next;
		cur = next;
		cur->m_flags |= RUDP_QPKT_RETRANSMIT;
		m_recvContiguousBytes += cur->m_buf.m_length;
		advanced = true;
		next = cur->m_next;
	}
	if (advanced && m_stats.lostPackets != 0 && hasRetryInfo == 0) {
		m_stats.lostPackets--;
		m_stats.reorderedPackets++;
		g_rudpStatus->m_data.lostPackets--;
		g_rudpStatus->m_data.reorderedPackets++;
	}
}

void RudpProtocol::insertRecvPacket(RudpBuffer *payload, RudpHeader *header)
{
	uint32_t dataLen = payload->m_length;
	uint64_t now = rudpGetTimeUsec();
	uint16_t seq = header->m_seq;
	int16_t seqDiff = (int16_t)(seq - m_recvHighestSeq);
	m_recvLastSeq = seq;
	m_lastRecvSeqTimeUsec = now;
	if (seqDiff > 0) {
		if ((uint16_t)seqDiff > 1U) {
			uint32_t gap = (uint32_t)((uint16_t)seqDiff - 1U);
			m_stats.lostPackets += gap;
			g_rudpStatus->m_data.lostPackets += gap;
		}
		m_recvHighestSeq = seq;
	}

	if ((m_flags & RUDP_PROTO_FLAG_STREAM) == 0 && (m_flags & RUDP_PROTO_FLAG_RELIABLE) == 0) {
		m_recvExpectedSeq = (uint16_t)(m_recvHighestSeq + 1);
		if ((m_flags & RUDP_PROTO_FLAG_ORDERED) != 0) {
			if ((int16_t)(seq - m_recvReadSeq) < 0) {
				return;
			}
			RudpPacket *prev = 0;
			RudpPacket *cur = m_recvHead;
			while (cur != 0) {
				if (cur->m_seq == seq) {
					m_stats.duplicatePackets++;
					return;
				}
				if ((int16_t)(seq - cur->m_seq) < 0) {
					break;
				}
				prev = cur;
				cur = cur->m_next;
			}
			RudpPacket *pkt = m_packetPool.acquire(0);
			if (pkt == 0) {
				enterCloseOrReset(RUDP_REASON_OUT_OF_MEMORY, 1);
				return;
			}
			pkt->m_buf.putBytes(payload->m_data + payload->m_offset, dataLen);
			payload->getBytes(0, dataLen);
			pkt->m_seq = seq;
			if ((header->m_flags & RUDP_HDR_FLAG_LATENCY_CRITICAL) != 0) {
				pkt->m_flags |= RUDP_QPKT_LATENCY_CRITICAL;
			}
			if ((header->m_flags & RUDP_HDR_FLAG_MORE_FRAGMENTS) != 0) {
				pkt->m_flags |= RUDP_QPKT_MORE_FRAGMENTS;
			} else {
				m_packetsToRead++;
			}
			if ((header->m_optFlags & RUDP_OPT_RETRY_INFO) != 0) {
				pkt->m_retryInfo = header->m_retryInfo;
			}
			if ((header->m_optFlags & RUDP_OPT_TX_TIMESTAMP) != 0) {
				pkt->m_timestamp = header->m_txTimestamp;
				pkt->m_flags |= RUDP_QPKT_HAS_TIMESTAMP;
			}
			m_recvTotalBytes += dataLen;
			if (cur != 0) {
				pkt->m_next = cur;
				if (cur == m_recvHead) {
					m_recvHead = pkt;
				} else {
					prev->m_next = pkt;
				}
			} else {
				pkt->m_next = 0;
				if (prev != 0) {
					m_recvTail->m_next = pkt;
				} else {
					m_recvHead = pkt;
				}
				m_recvTail = pkt;
			}
		} else {
			RudpPacket *pkt = m_packetPool.acquire(0);
			if (pkt == 0) {
				enterCloseOrReset(RUDP_REASON_OUT_OF_MEMORY, 1);
				return;
			}
			pkt->m_buf.putBytes(payload->m_data + payload->m_offset, dataLen);
			payload->getBytes(0, dataLen);
			pkt->m_seq = seq;
			if (m_recvTail != 0) {
				m_recvTail->m_next = pkt;
			} else {
				m_recvHead = pkt;
			}
			m_recvTail = pkt;
			if ((header->m_flags & RUDP_HDR_FLAG_LATENCY_CRITICAL) != 0) {
				pkt->m_flags |= RUDP_QPKT_LATENCY_CRITICAL;
				if ((m_flags & RUDP_PROTO_FLAG_ORDERED) == 0) {
					m_recvUrgentCount++;
				}
			}
			if ((header->m_flags & RUDP_HDR_FLAG_MORE_FRAGMENTS) != 0) {
				pkt->m_flags |= RUDP_QPKT_MORE_FRAGMENTS;
			} else {
				m_packetsToRead++;
			}
			if ((header->m_optFlags & RUDP_OPT_RETRY_INFO) != 0) {
				pkt->m_retryInfo = header->m_retryInfo;
			}
			if ((header->m_optFlags & RUDP_OPT_TX_TIMESTAMP) != 0) {
				pkt->m_timestamp = header->m_txTimestamp;
				pkt->m_flags |= RUDP_QPKT_HAS_TIMESTAMP;
			}
			m_recvTotalBytes += dataLen;
		}
		return;
	}

	if ((int16_t)(seq - m_recvExpectedSeq) < 0) {
		m_stats.duplicatePackets++;
		return;
	}

	if (seq == m_recvExpectedSeq) {
		RudpPacket *pkt = m_packetPool.acquire(0);
		if (pkt == 0) {
			enterCloseOrReset(RUDP_REASON_OUT_OF_MEMORY, 1);
			return;
		}
		pkt->m_buf.putBytes(payload->m_data + payload->m_offset, dataLen);
		payload->getBytes(0, dataLen);
		pkt->m_seq = seq;
		pkt->m_flags |= RUDP_QPKT_RETRANSMIT;
		if ((header->m_flags & RUDP_HDR_FLAG_MORE_FRAGMENTS) != 0) {
			pkt->m_flags |= RUDP_QPKT_MORE_FRAGMENTS;
		} else {
			m_packetsToRead++;
		}
		if ((header->m_flags & RUDP_HDR_FLAG_LATENCY_CRITICAL) != 0) {
			pkt->m_flags |= RUDP_QPKT_LATENCY_CRITICAL;
			if ((m_flags & RUDP_PROTO_FLAG_ORDERED) == 0) {
				m_recvUrgentCount++;
			}
		}
		if ((header->m_optFlags & RUDP_OPT_RETRY_INFO) != 0) {
			pkt->m_retryInfo = header->m_retryInfo;
		}
		if ((header->m_optFlags & RUDP_OPT_TX_TIMESTAMP) != 0) {
			pkt->m_timestamp = header->m_txTimestamp;
			pkt->m_flags |= RUDP_QPKT_HAS_TIMESTAMP;
		}
		m_recvExpectedSeq++;
		m_recvTotalBytes += dataLen;
		m_recvContiguousBytes += dataLen;
		if (m_recvHead != 0) {
			if (m_recvContiguousTail != 0) {
				pkt->m_next = m_recvContiguousTail->m_next;
				m_recvContiguousTail->m_next = pkt;
				if (m_recvContiguousTail == m_recvTail) {
					m_recvTail = pkt;
				}
			} else {
				pkt->m_next = m_recvHead;
				m_recvHead = pkt;
			}
			m_recvContiguousTail = pkt;
			advanceContiguousTail(header->m_optFlags & RUDP_OPT_RETRY_INFO);
			m_recvExpectedSeq = (uint16_t)(m_recvContiguousTail->m_seq + 1);
		} else {
			pkt->m_next = 0;
			m_recvHead = pkt;
			m_recvTail = pkt;
			m_recvContiguousTail = pkt;
		}
	} else {
		RudpPacket *prev = 0;
		RudpPacket *cur = 0;
		if (m_recvContiguousTail != 0) {
			prev = m_recvContiguousTail;
			cur = m_recvContiguousTail->m_next;
		} else {
			cur = m_recvHead;
		}
		while (cur != 0) {
			if (cur->m_seq == seq) {
				m_stats.duplicatePackets++;
				return;
			}
			if ((int16_t)(seq - cur->m_seq) < 0) {
				break;
			}
			prev = cur;
			cur = cur->m_next;
		}
		RudpPacket *pkt = m_packetPool.acquire(0);
		if (pkt == 0) {
			enterCloseOrReset(RUDP_REASON_OUT_OF_MEMORY, 1);
			return;
		}
		pkt->m_buf.putBytes(payload->m_data + payload->m_offset, dataLen);
		payload->getBytes(0, dataLen);
		pkt->m_seq = seq;
		if ((header->m_flags & RUDP_HDR_FLAG_LATENCY_CRITICAL) != 0) {
			pkt->m_flags |= RUDP_QPKT_LATENCY_CRITICAL;
			if ((m_flags & RUDP_PROTO_FLAG_ORDERED) == 0) {
				m_recvUrgentCount++;
			}
		}
		if ((header->m_flags & RUDP_HDR_FLAG_MORE_FRAGMENTS) != 0) {
			pkt->m_flags |= RUDP_QPKT_MORE_FRAGMENTS;
		} else {
			m_packetsToRead++;
		}
		if ((header->m_optFlags & RUDP_OPT_RETRY_INFO) != 0) {
			pkt->m_retryInfo = header->m_retryInfo;
		}
		if ((header->m_optFlags & RUDP_OPT_TX_TIMESTAMP) != 0) {
			pkt->m_timestamp = header->m_txTimestamp;
			pkt->m_flags |= RUDP_QPKT_HAS_TIMESTAMP;
		}
		m_recvTotalBytes += dataLen;
		if (cur != 0) {
			if (cur == m_recvHead) {
				pkt->m_next = cur;
				m_recvHead = pkt;
				rebuildContiguousTail();
			} else {
				pkt->m_next = cur;
				if (prev != 0) {
					prev->m_next = pkt;
				}
			}
		} else {
			pkt->m_next = 0;
			if (prev != 0) {
				m_recvTail->m_next = pkt;
			} else {
				m_recvHead = pkt;
			}
			m_recvTail = pkt;
		}
	}
}

void RudpProtocol::processIncomingData(RudpHeader *header, RudpBuffer *payload)
{
	uint16_t maxRxPkts = (uint16_t)m_recvBufSize;
	if (m_mss > RUDP_HEADER_OVERHEAD) {
		maxRxPkts = (uint16_t)(m_recvBufSize / (m_mss - RUDP_HEADER_OVERHEAD));
	}
	if ((maxRxPkts > m_packetPool.m_rxInUse) || (int16_t)(header->m_seq - m_recvHighestSeq) <= 0) {
		insertRecvPacket(payload, header);
		if ((m_flags & RUDP_PROTO_FLAG_STREAM) != 0 || (m_flags & RUDP_PROTO_FLAG_RELIABLE) != 0) {
			if (m_unackedRecvCount != 0) {
				if ((m_flags & RUDP_PROTO_FLAG_ACK_SCHEDULED) == 0 && m_delegate != 0) {
					m_flags |= RUDP_PROTO_FLAG_ACK_SCHEDULED;
					m_delegate->onProtocolScheduleAck(this);
				}
			} else {
				m_delayedAckStartUsec = rudpGetTimeUsec();
				m_unackedRecvCount++;
			}
		}
		updateReadableSize();
	}
}

void RudpProtocol::handleDataPacket(RudpHeader *header, RudpBuffer *payload)
{
	switch (m_state) {
	case SCE_RUDP_STATE_SYN_RCVD:
		if ((header->m_flags & RUDP_HDR_FLAG_ACK) != 0) {
			m_rtoDeadlineUsec = 0;
			m_retransmitCount = 0;
			m_state = SCE_RUDP_STATE_ESTABLISHED;
			m_sendUnackedSeq = m_sendNextSeq;
			g_rudpStatus->m_data.establishedConnections++;
			if (m_delegate != 0) {
				m_delegate->onProtocolConnected(this);
			}
			if (m_recvTotalBytes != 0) {
				updateReadableSize();
			}
		} else if ((m_flags & RUDP_PROTO_FLAG_ACK_SCHEDULED) == 0 && m_delegate != 0) {
			m_flags |= RUDP_PROTO_FLAG_ACK_SCHEDULED;
			m_delegate->onProtocolScheduleAck(this);
		}
		if (payload->m_length != 0) {
			processIncomingData(header, payload);
		}
		break;
	case SCE_RUDP_STATE_ESTABLISHED:
		if (payload->m_length != 0) {
			processIncomingData(header, payload);
		}
		break;
	case SCE_RUDP_STATE_CLOSE_WAIT:
		if (m_delegate != 0) {
			m_delegate->onProtocolScheduleRst(this, m_localConnId, 1);
		}
		break;
	default:
		break;
	}
}

void RudpProtocol::markPacketForRetransmit(RudpPacket *pkt)
{
	if (pkt == 0) {
		return;
	}
	if ((m_flags & RUDP_PROTO_FLAG_STREAM) == 0 && (m_flags & RUDP_PROTO_FLAG_RELIABLE) == 0) {
		return;
	}
	if ((pkt->m_flags & RUDP_QPKT_SENT) != 0 &&
	    (pkt->m_flags & RUDP_QPKT_SACKED) == 0 &&
	    (pkt->m_flags & RUDP_QPKT_RETRANSMIT) == 0) {
		pkt->m_flags |= RUDP_QPKT_RETRANSMIT;
		m_sendQueuedBytes += pkt->m_buf.m_length;
		m_stats.retransmissions++;
		g_rudpStatus->m_data.retransmittedPackets++;
		if ((m_flags & RUDP_PROTO_FLAG_SEND_SCHEDULED) == 0 && m_delegate != 0) {
			m_flags |= RUDP_PROTO_FLAG_SEND_SCHEDULED;
			m_delegate->onProtocolScheduleData(this);
		}
	}
}

void RudpProtocol::updateRtt(uint32_t sampleMs)
{
	uint64_t sampleUsec = (uint64_t)sampleMs * 1000ULL;
	int64_t delta = 0;
	uint64_t rttVar;
	uint64_t varTerm;
	uint64_t newRto;

	if (m_srttUsec == 0) {
		m_srttUsec = sampleUsec;
		rttVar = sampleUsec >> 1;
		m_rttVarUsec = rttVar;
		varTerm = rttVar * 4;
		uint64_t effVar = (m_clockGranularityUsec > varTerm) ? m_clockGranularityUsec : varTerm;
		newRto = sampleUsec + effVar;
		m_rtoUsec = newRto;
	} else {
		delta = (int64_t)(sampleUsec - m_srttUsec);
		uint64_t absDelta = (delta < 0) ? (uint64_t)(-delta) : (uint64_t)delta;
		rttVar = (75ULL * m_rttVarUsec) / 100ULL + (25ULL * absDelta) / 100ULL;
		m_rttVarUsec = rttVar;
		uint64_t srtt = (88ULL * m_srttUsec) / 100ULL + (12ULL * sampleUsec) / 100ULL;
		m_srttUsec = srtt;
		varTerm = rttVar * 4;
		uint64_t effVar = (m_clockGranularityUsec > varTerm) ? m_clockGranularityUsec : varTerm;
		newRto = srtt + effVar;
		m_rtoUsec = newRto;
	}

	if (newRto < RUDP_INITIAL_RTO_USEC) {
		m_rtoUsec = RUDP_INITIAL_RTO_USEC;
	} else if (newRto > RUDP_MAX_RTO_USEC) {
		m_rtoUsec = RUDP_MAX_RTO_USEC;
	}

	if (rttVar > 1000ULL && delta > (int64_t)varTerm) {
		uint32_t reduced = m_cwnd - (m_cwnd >> 3);
		if (reduced < m_mss) {
			reduced = m_mss;
		}
		m_cwnd = reduced;
		if (m_ssthresh > reduced) {
			m_ssthresh = reduced;
		}
	}
}

RudpResult RudpProtocol::processAck(RudpHeader *header)
{
	if (m_state == SCE_RUDP_STATE_SYN_SENT || m_state == SCE_RUDP_STATE_SYN_RCVD) {
		if ((uint16_t)(m_sendNextSeq + 1) == header->m_ack) {
			m_sendNextSeq++;
			return RudpResult(0);
		}
		enterCloseOrReset(RUDP_REASON_REFUSED, 1);
		return RudpResult(-1);
	}
	if (header->m_type == RUDP_PKT_SYN) {
		return RudpResult(0);
	}
	if (header->m_type == RUDP_PKT_PING) {
		m_keepAliveLastAckUsec = rudpGetTimeUsec();
		return RudpResult(0);
	}
	if (m_state == SCE_RUDP_STATE_CLOSE_WAIT) {
		return RudpResult(0);
	}
	if ((m_flags & RUDP_PROTO_FLAG_STREAM) == 0 && (m_flags & RUDP_PROTO_FLAG_RELIABLE) == 0) {
		return RudpResult(0);
	}

	uint32_t nowMs = (uint32_t)(rudpGetTimeUsec() / 1000ULL);
	bool measureRtt = (header->m_flags & RUDP_HDR_FLAG_ACK_DELAY) != 0;
	uint16_t ackSeq = header->m_ack;

	while (m_sendHead != 0 && (m_sendHead->m_flags & RUDP_QPKT_SENT) != 0) {
		RudpPacket *pkt = m_sendHead;
		if ((int16_t)(pkt->m_seq - ackSeq) >= 0) {
			break;
		}
		if (measureRtt && (uint16_t)(pkt->m_seq + 1) == ackSeq) {
			if ((pkt->m_flags & RUDP_QPKT_RETRANSMITTED) == 0) {
				int32_t rttRaw = (int32_t)(nowMs - pkt->m_timestamp);
				if (rttRaw >= 0) {
					uint32_t sample = ((uint32_t)rttRaw > header->m_ackDelayMs)
						? ((uint32_t)rttRaw - header->m_ackDelayMs) : 0;
					updateRtt(sample);
				}
			}
			measureRtt = false;
		}
		if ((m_flags & RUDP_PROTO_FLAG_FLUSH_PENDING) != 0 && (pkt->m_flags & RUDP_QPKT_FLUSH_MARKER) != 0) {
			m_flags &= ~RUDP_PROTO_FLAG_FLUSH_PENDING;
			if (m_delegate != 0) {
				m_delegate->onProtocolFlushed(this);
			}
		}
		m_sendTotalBytes -= pkt->m_buf.m_length;
		if ((pkt->m_flags & RUDP_QPKT_RETRANSMIT) != 0) {
			m_sendQueuedBytes -= pkt->m_buf.m_length;
		}
		m_sendHead = pkt->m_next;
		if (m_sendHead == 0) {
			m_sendTail = 0;
		}
		m_packetPool.release(pkt);
	}

	uint16_t prevUnacked = m_sendUnackedSeq;
	m_sendUnackedSeq = ackSeq;
	if (m_sendHead != 0 && (int16_t)(ackSeq - prevUnacked) <= 0) {
		m_dupAckCount++;
	}

	bool sackAdvanced = false;
	if ((m_flags & RUDP_PROTO_FLAG_SACK) != 0 && header->getSackCount() != 0) {
		uint32_t lastIdx = header->getSackCount() - 1;
		uint16_t sackMaxEnd = header->getSackBlock(lastIdx)->end;
		for (RudpPacket *cur = m_sendHead; cur != 0; cur = cur->m_next) {
			if ((cur->m_flags & RUDP_QPKT_SENT) == 0 || (int16_t)(cur->m_seq - sackMaxEnd) >= 0) {
				break;
			}
			for (uint32_t i = 0; i < header->getSackCount(); i++) {
				const RudpSackBlock *blk = header->getSackBlock(i);
				if ((int16_t)(cur->m_seq - blk->start) >= 0 && (int16_t)(cur->m_seq - blk->end) < 0) {
					sackAdvanced = true;
					cur->m_flags |= RUDP_QPKT_SACKED;
				}
			}
		}
	}

	bool inFastRecovery = (m_dupAckCount >= 3);
	bool enteredFastRecovery = false;
	if (!inFastRecovery) {
		if ((int16_t)(ackSeq - prevUnacked) > 0) {
			uint32_t inc;
			if (m_cwnd >= m_ssthresh) {
				inc = (m_mss * m_mss) / m_cwnd;
				if (inc < 1) {
					inc = 1;
				}
			} else {
				inc = m_mss;
			}
			m_cwnd += inc;
			if (m_cwnd > m_sendBufSize) {
				m_cwnd = m_sendBufSize;
			}
			if (m_sendTotalBytes == 0) {
				if (m_cwnd > m_ssthresh) {
					m_ssthresh = m_cwnd;
				}
				m_cwnd = m_mss;
			}
		}
	} else {
		if (m_fastRecoveryEndSeq < 0) {
			uint32_t halfFlight = (m_sendTotalBytes - m_sendQueuedBytes) >> 1;
			uint32_t minCwnd = m_mss * 2;
			uint32_t target = (halfFlight > minCwnd) ? halfFlight : minCwnd;
			enteredFastRecovery = true;
			m_cwnd = target;
			m_ssthresh = target;
			m_fastRecoveryEndSeq = m_sendNextSeq;
			m_fastRecoveryHighSeq = ackSeq;
		} else if ((int16_t)(ackSeq - m_fastRecoveryEndSeq) >= 0) {
			m_cwnd = m_ssthresh;
			m_dupAckCount = 0;
			m_fastRecoveryEndSeq = -1;
		}

		if (enteredFastRecovery && m_sendHead != 0) {
			markPacketForRetransmit(m_sendHead);
			m_fastRecoveryHighSeq++;
		}
		int thirdSack = -1;
		int secondSack = -1;
		int firstSack = -1;
		for (RudpPacket *cur = m_sendHead; cur != 0; cur = cur->m_next) {
			if ((cur->m_flags & RUDP_QPKT_SENT) == 0) {
				break;
			}
			if ((cur->m_flags & RUDP_QPKT_SACKED) != 0) {
				thirdSack = secondSack;
				secondSack = firstSack;
				firstSack = cur->m_seq;
			}
		}
		if (thirdSack >= 0) {
			for (RudpPacket *cur = m_sendHead; cur != 0; cur = cur->m_next) {
				if ((cur->m_flags & RUDP_QPKT_SENT) == 0) {
					break;
				}
				if ((int16_t)(cur->m_seq - (uint16_t)thirdSack) >= 0) {
					break;
				}
				if ((cur->m_flags & RUDP_QPKT_SACKED) == 0 &&
				    (int16_t)(cur->m_seq - m_fastRecoveryHighSeq) >= 0) {
					markPacketForRetransmit(cur);
				}
			}
			m_fastRecoveryHighSeq = (uint16_t)thirdSack;
		}
	}

	if (sackAdvanced || (int16_t)(ackSeq - prevUnacked) > 0) {
		m_retransmitCount = 0;
		if (m_sendTotalBytes != 0) {
			uint64_t rto = (m_rtoUsec < RUDP_MAX_RTO_USEC) ? m_rtoUsec : RUDP_MAX_RTO_USEC;
			m_rtoDeadlineUsec = rto + rudpGetTimeUsec();
		} else {
			m_rtoDeadlineUsec = 0;
		}
	}

	if ((m_flags & RUDP_PROTO_FLAG_WRITE_BLOCKED) != 0 && getSizeWritable() > 0) {
		m_flags &= ~RUDP_PROTO_FLAG_WRITE_BLOCKED;
		if (m_delegate != 0) {
			m_delegate->onProtocolWritable(this);
		}
	}
	if ((m_flags & RUDP_PROTO_FLAG_SEND_SCHEDULED) == 0 && m_delegate != 0) {
		m_flags |= RUDP_PROTO_FLAG_SEND_SCHEDULED;
		m_delegate->onProtocolScheduleData(this);
	}
	return RudpResult(0);
}

RudpResult RudpProtocol::onPacketReceived(RudpBuffer *buf)
{
	if (m_state == SCE_RUDP_STATE_IDLE) {
		return RudpResult(0);
	}
	m_stats.rcvdPackets++;
	m_stats.rcvdBytes += buf->m_length;

	RudpHeader hdr;
	RudpResult parseRes = hdr.parse(buf);
	if (parseRes.getCode() != 0) {
		return RudpResult(SCE_RUDP_ERROR_MSG_MALFORMED);
	}

	m_lastRxTimeUsec = rudpGetTimeUsec();
	switch (hdr.m_type) {
	case RUDP_PKT_DATA:
		g_rudpStatus->m_data.rcvdUsrPackets++;
		break;
	case RUDP_PKT_PING:
		g_rudpStatus->m_data.rcvdPrbPackets++;
		break;
	case RUDP_PKT_SYN:
		g_rudpStatus->m_data.rcvdSynPackets++;
		break;
	case RUDP_PKT_RST:
		g_rudpStatus->m_data.rcvdRstPackets++;
		break;
	}

	if ((hdr.m_flags & RUDP_HDR_FLAG_ACK) != 0) {
		if (hdr.m_type == RUDP_PKT_RST) {
			return RudpResult(SCE_RUDP_ERROR_MSG_MALFORMED);
		}
		RudpResult ackRes = processAck(&hdr);
		if (ackRes.getCode() < 0) {
			return RudpResult(0);
		}
	}

	if (hdr.m_type != RUDP_PKT_RST) {
		uint16_t win = hdr.decodeWindow();
		m_peerWindow = win;
		if (win == 0 && m_sendTotalBytes != 0) {
			if (m_zeroWinProbeDelayMs == 0) {
				m_zeroWinProbeStartUsec = rudpGetTimeUsec();
				m_zeroWinProbeDelayMs = RUDP_ZERO_WINDOW_PROBE_INIT_MS;
			}
		} else if (m_zeroWinProbeDelayMs != 0) {
			if (m_sendTotalBytes != 0 && (m_flags & RUDP_PROTO_FLAG_SEND_SCHEDULED) == 0 && m_delegate != 0) {
				m_flags |= RUDP_PROTO_FLAG_SEND_SCHEDULED;
				m_delegate->onProtocolScheduleData(this);
			}
			m_zeroWinProbeDelayMs = 0;
		}
	}

	switch (hdr.m_type) {
	case RUDP_PKT_DATA:
		handleDataPacket(&hdr, buf);
		break;
	case RUDP_PKT_PING:
		handlePingPacket(&hdr);
		break;
	case RUDP_PKT_SYN:
		handleSynPacket(&hdr);
		break;
	case RUDP_PKT_RST:
		handleResetPacket(&hdr);
		break;
	}
	return RudpResult(0);
}

void RudpProtocol::getStats(RudpProtocolStats *outStats) const
{
	*outStats = m_stats;
	outStats->smoothedRttUsec = m_srttUsec;
}

uint32_t RudpProtocol::getPacketsToRead() const
{
	return m_packetsToRead;
}
