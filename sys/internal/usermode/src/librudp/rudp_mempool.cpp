/*
 * rudp_mempool.cpp: per-connection pooled packet buffer allocator.
 */
#include "rudp_internal.h"

RudpPacketPool::RudpPacketPool()
	: m_freeList(0),
	  m_freeCount(0),
	  m_totalAllocated(0),
	  m_txInUse(0),
	  m_rxInUse(0),
	  m_txPeak(0),
	  m_rxPeak(0),
	  m_packetCapacity(RUDP_DEFAULT_MSS - RUDP_HEADER_OVERHEAD)
{
}

int RudpPacketPool::clear()
{
	RudpPacket *cur = m_freeList;
	while (cur != 0) {
		RudpPacket *next = cur->m_next;
		if (cur->m_storage != 0) {
			rudpFree(cur->m_storage);
		}
		delete cur;
		m_freeList = next;
		cur = next;
	}
	m_freeCount = 0;
	m_totalAllocated = 0;
	m_txInUse = 0;
	m_rxInUse = 0;
	return 0;
}

RudpPacketPool::~RudpPacketPool()
{
	clear();
}

RudpPacket *RudpPacketPool::acquire(uint8_t forTx)
{
	RudpPacket *pkt = m_freeList;
	if (pkt != 0) {
		m_freeList = pkt->m_next;
		m_freeCount--;
	} else {
		pkt = new RudpPacket();
		if (pkt != 0) {
			uint32_t cap = m_packetCapacity;
			pkt->m_timestamp = 0;
			pkt->m_retryInfo = 0;
			pkt->m_flags = 0;
			pkt->m_seq = 0;
			pkt->m_next = 0;
			pkt->m_storage = 0;
			pkt->m_allocError = 0;
			pkt->m_storage = (uint8_t *)rudpAlloc(cap);
			if (pkt->m_storage != 0) {
				pkt->m_buf.setBuffer(pkt->m_storage, cap);
			} else {
				pkt->m_allocError = SCE_RUDP_ERROR_MEMORY;
			}
		}
		if (pkt == 0 || pkt->m_allocError != 0) {
			return 0;
		}
		m_totalAllocated++;
	}

	if (pkt != 0) {
		pkt->m_seq = 0;
		pkt->m_timestamp = 0;
		pkt->m_retryInfo = 0;
		pkt->m_next = 0;
		pkt->m_flags = 0;
		pkt->m_buf.reset();
		if (forTx != 0) {
			pkt->m_flags |= RUDP_QPKT_TX_POOL;
			uint16_t inUse = m_txInUse + 1;
			m_txInUse = inUse;
			if (m_txPeak < inUse) {
				m_txPeak = inUse;
			}
		} else {
			uint16_t inUse = m_rxInUse + 1;
			m_rxInUse = inUse;
			if (m_rxPeak < inUse) {
				m_rxPeak = inUse;
			}
		}
	}
	return pkt;
}

void RudpPacketPool::release(RudpPacket *pkt)
{
	pkt->m_next = m_freeList;
	m_freeList = pkt;
	m_freeCount++;
	if ((pkt->m_flags & RUDP_QPKT_TX_POOL) != 0) {
		m_txInUse--;
	} else {
		m_rxInUse--;
	}
}
