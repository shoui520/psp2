/*
 * rudpHeader.cpp: RUDP wire protocol header serializer and parser.
 */
#include "rudp_internal.h"

RudpHeader::RudpHeader()
{
	m_optFlags = 0;
	m_padAlign = 0;
	m_reservedByte = 0;
	m_rstReason = 0;
	m_synQuality = 0;
	m_windowEncoded = 0;
	m_extFlags = 0;
	m_synCaps = 0;
	m_flags = 0;
	m_type = 0;
	m_maxVersion = 0x0100;
	m_minVersion = 0x0100;
	m_seq = 0;
	m_ack = 0;
	m_connectionId = 0;
	m_ackDelayMs = 0;
	m_mss = 0;
	m_cookieLen = 0;
	m_sackCount = 0;
	m_reservedHalf = 0;
	m_txTimestamp = 0;
	m_retryInfo = 0;
}

void RudpHeader::encodeWindow(uint16_t windowPackets)
{
	if ((windowPackets & 0xF000U) != 0) {
		windowPackets = 4095;
	}
	if ((windowPackets & 0xFC00U) != 0) {
		m_windowEncoded = (uint8_t)((windowPackets >> 6) | 0xC0U);
	} else if ((windowPackets & 0xFF00U) != 0) {
		m_windowEncoded = (uint8_t)((windowPackets >> 4) | 0x80U);
	} else if ((windowPackets & 0xFFC0U) != 0) {
		m_windowEncoded = (uint8_t)((windowPackets >> 2) | 0x40U);
	} else {
		m_windowEncoded = (uint8_t)windowPackets;
	}
}

uint16_t RudpHeader::decodeWindow() const
{
	return (uint16_t)((m_windowEncoded & 0x3FU) << (2 * (m_windowEncoded >> 6)));
}

void RudpHeader::setCookie(const void *cookie, uint32_t len)
{
	if (cookie != 0 && len != 0) {
		m_extFlags |= RUDP_EXT_COOKIE;
		uint32_t copyLen = (len < RUDP_MAX_COOKIE_SIZE) ? len : RUDP_MAX_COOKIE_SIZE;
		m_cookieLen = copyLen;
		rudpMemcpy(m_cookie, cookie, copyLen);
	}
}

void RudpHeader::getCookie(const uint8_t **outData, uint32_t *outLen) const
{
	*outData = m_cookie;
	*outLen = m_cookieLen;
}

void RudpHeader::addSackBlock(const RudpSackBlock *block)
{
	if (m_sackCount < RUDP_MAX_SACK_BLOCKS) {
		m_sackBlocks[m_sackCount].start = block->start;
		m_sackBlocks[m_sackCount].end = block->end;
		m_sackCount++;
	}
}

uint32_t RudpHeader::getSackCount() const
{
	return m_sackCount;
}

const RudpSackBlock *RudpHeader::getSackBlock(uint32_t index) const
{
	if (index >= RUDP_MAX_SACK_BLOCKS) {
		return 0;
	}
	return &m_sackBlocks[index];
}

void RudpHeader::reset()
{
	m_optFlags = 0;
	m_padAlign = 0;
	m_reservedByte = 0;
	m_rstReason = 0;
	m_synQuality = 0;
	m_windowEncoded = 0;
	m_extFlags = 0;
	m_synCaps = 0;
	m_flags = 0;
	m_type = 0;
	m_maxVersion = 0x0100;
	m_seq = 0;
	m_ack = 0;
	m_connectionId = 0;
	m_ackDelayMs = 0;
	m_mss = 0;
	m_cookieLen = 0;
	m_sackCount = 0;
	m_reservedHalf = 0;
	m_retryInfo = 0;
	m_txTimestamp = 0;
}

void RudpHeader::setPaddingAlignment(uint8_t alignment)
{
	for (int shift = 2; shift < 8; shift++) {
		if (alignment == (1 << shift)) {
			m_padAlign = alignment;
			break;
		}
	}
}

RudpResult RudpHeader::serialize(RudpBuffer *buf)
{
	uint8_t *lastOptionByte = 0;
	uint32_t startLen = buf->m_length;
	uint8_t *basePtr = buf->m_data + buf->m_offset;

	if (buf->writeU8((uint8_t)(m_flags | (m_type << 6))) < 0) {
		return RudpResult(rudpRecordError("rudpHeader.cpp", 171));
	}
	if (buf->writeU8(m_windowEncoded) < 0) {
		return RudpResult(rudpRecordError("rudpHeader.cpp", 174));
	}
	if (buf->writeU16(m_seq) < 0) {
		return RudpResult(rudpRecordError("rudpHeader.cpp", 177));
	}
	if ((m_flags & RUDP_HDR_FLAG_ACK) != 0) {
		if (buf->writeU16(m_ack) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 182));
		}
		if ((m_flags & RUDP_HDR_FLAG_ACK_DELAY) != 0) {
			if (buf->writeU16(m_ackDelayMs) < 0) {
				return RudpResult(rudpRecordError("rudpHeader.cpp", 187));
			}
		}
	}

	if (m_type == RUDP_PKT_SYN) {
		uint16_t synWord = (uint16_t)(m_synQuality & 0x0FU);
		if ((m_synCaps & RUDP_SYN_CAP_STREAM) != 0) {
			synWord |= 0x0800U;
		}
		if ((m_synCaps & RUDP_SYN_CAP_RELIABLE) != 0) {
			synWord |= 0x0200U;
		}
		if ((m_synCaps & RUDP_SYN_CAP_ORDERED) != 0) {
			synWord |= 0x0100U;
		}
		if (buf->writeU16(synWord) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 207));
		}
		if (buf->writeU16(m_maxVersion) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 210));
		}
		if (buf->writeU16(m_minVersion) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 212));
		}
		if (buf->writeU32(m_connectionId) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 214));
		}
		if (buf->writeU16(m_mss) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 216));
		}
	} else if (m_type == RUDP_PKT_RST) {
		if (buf->writeU16(m_rstReason) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 223));
		}
		if (buf->writeU32(m_connectionId) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 226));
		}
	}

	if ((m_extFlags & RUDP_EXT_COOKIE) != 0) {
		lastOptionByte = buf->m_data + buf->m_offset + buf->m_length;
		if (buf->writeU8(1) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 239));
		}
		if (buf->writeU8((uint8_t)m_cookieLen) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 242));
		}
		if (buf->putBytes(m_cookie, m_cookieLen) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 245));
		}
	}

	if (m_type != RUDP_PKT_SYN && (m_extFlags & RUDP_EXT_MSS) != 0) {
		lastOptionByte = buf->m_data + buf->m_offset + buf->m_length;
		if (buf->writeU8(2) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 255));
		}
		if (buf->writeU8(2) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 257));
		}
		if (buf->writeU16(m_mss) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 259));
		}
	}

	if (m_sackCount != 0) {
		lastOptionByte = buf->m_data + buf->m_offset + buf->m_length;
		if (buf->writeU8(5) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 271));
		}
		if (buf->writeU8((uint8_t)(4 * (m_sackCount & 0x3FU))) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 273));
		}
		for (uint32_t i = 0; i < m_sackCount; i++) {
			if (buf->writeU16(m_sackBlocks[i].start) < 0) {
				return RudpResult(rudpRecordError("rudpHeader.cpp", 279));
			}
			if (buf->writeU16(m_sackBlocks[i].end) < 0) {
				return RudpResult(rudpRecordError("rudpHeader.cpp", 281));
			}
		}
	}

	if ((m_optFlags & RUDP_OPT_RETRY_INFO) != 0) {
		lastOptionByte = buf->m_data + buf->m_offset + buf->m_length;
		if (buf->writeU8(10) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 290));
		}
		if (buf->writeU8(4) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 292));
		}
		if (buf->writeU32(m_retryInfo) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 294));
		}
	}

	if ((m_optFlags & RUDP_OPT_TX_TIMESTAMP) != 0) {
		lastOptionByte = buf->m_data + buf->m_offset + buf->m_length;
		if (buf->writeU8(11) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 303));
		}
		if (buf->writeU8(4) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 305));
		}
		if (buf->writeU32(m_txTimestamp) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 307));
		}
	}

	if (m_padAlign != 0) {
		uintptr_t curAddr = (uintptr_t)(buf->m_data + buf->m_offset + buf->m_length);
		uint8_t mask = (uint8_t)(m_padAlign - 1);
		uint8_t padCount = (uint8_t)((m_padAlign - (curAddr & mask)) & mask);
		for (uint8_t i = 0; i < padCount; i++) {
			lastOptionByte = buf->m_data + buf->m_offset + buf->m_length;
			if (buf->writeU8(0) < 0) {
				return RudpResult(rudpRecordError("rudpHeader.cpp", 327));
			}
		}
	}

	if (lastOptionByte != 0) {
		m_flags |= RUDP_HDR_FLAG_HAS_OPTIONS;
		basePtr[startLen] |= RUDP_HDR_FLAG_HAS_OPTIONS;
		*lastOptionByte |= 0x80U;
	}
	return RudpResult(0);
}

RudpResult RudpHeader::parse(RudpBuffer *buf)
{
	reset();

	if (buf->readU8(&m_flags) < 0) {
		return RudpResult(rudpRecordError("rudpHeader.cpp", 351));
	}
	m_type = m_flags >> 6;
	m_flags &= 0x3FU;

	if (buf->readU8(&m_windowEncoded) < 0) {
		return RudpResult(rudpRecordError("rudpHeader.cpp", 357));
	}
	if (buf->readU16(&m_seq) < 0) {
		return RudpResult(rudpRecordError("rudpHeader.cpp", 361));
	}
	if ((m_flags & RUDP_HDR_FLAG_ACK) != 0) {
		if (buf->readU16(&m_ack) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 367));
		}
	}
	if ((m_flags & RUDP_HDR_FLAG_ACK_DELAY) != 0) {
		if ((m_flags & RUDP_HDR_FLAG_ACK) == 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 373));
		}
		if (buf->readU16(&m_ackDelayMs) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 375));
		}
	}

	if (m_type == RUDP_PKT_SYN) {
		uint16_t synWord = 0;
		if (buf->readU16(&synWord) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 381));
		}
		if ((synWord & 0x0800U) != 0) {
			m_synCaps |= RUDP_SYN_CAP_STREAM;
		}
		if ((synWord & 0x0200U) != 0) {
			m_synCaps |= RUDP_SYN_CAP_RELIABLE;
		}
		if ((synWord & 0x0100U) != 0) {
			m_synCaps |= RUDP_SYN_CAP_ORDERED;
		}
		m_synQuality = (uint8_t)(synWord & 0x0FU);
		if (buf->readU16(&m_maxVersion) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 392));
		}
		if (buf->readU16(&m_minVersion) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 394));
		}
		if (buf->readU32(&m_connectionId) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 396));
		}
		if (buf->readU16(&m_mss) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 398));
		}
		m_extFlags |= RUDP_EXT_MSS;
	} else if (m_type == RUDP_PKT_RST) {
		uint16_t rstWord = 0;
		if (buf->readU16(&rstWord) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 404));
		}
		m_rstReason = (uint8_t)rstWord;
		if (buf->readU32(&m_connectionId) < 0) {
			return RudpResult(rudpRecordError("rudpHeader.cpp", 409));
		}
	}

	if ((m_flags & RUDP_HDR_FLAG_HAS_OPTIONS) != 0) {
		int done = 0;
		while (!done && buf->m_length != 0) {
			uint8_t optType = 0;
			if (buf->readU8(&optType) < 0) {
				return RudpResult(rudpRecordError("rudpHeader.cpp", 422));
			}
			if ((optType & 0x80U) != 0) {
				optType &= 0x7FU;
				done = 1;
			}
			if (optType == 0) {
				continue;
			}
			uint8_t optLen = 0;
			if (buf->readU8(&optLen) < 0) {
				return RudpResult(rudpRecordError("rudpHeader.cpp", 434));
			}
			if (buf->m_length < optLen) {
				return RudpResult(rudpRecordError("rudpHeader.cpp", 439));
			}
			switch (optType) {
			case 1:
				if (optLen == 0 || optLen > RUDP_MAX_COOKIE_SIZE) {
					return RudpResult(rudpRecordError("rudpHeader.cpp", 457));
				}
				m_cookieLen = optLen;
				if (buf->getBytes(m_cookie, optLen) < 0) {
					return RudpResult(rudpRecordError("rudpHeader.cpp", 450));
				}
				m_extFlags |= RUDP_EXT_COOKIE;
				break;
			case 2:
				if (optLen != 2) {
					return RudpResult(rudpRecordError("rudpHeader.cpp", 473));
				}
				if (buf->readU16(&m_mss) < 0) {
					return RudpResult(rudpRecordError("rudpHeader.cpp", 466));
				}
				m_extFlags |= RUDP_EXT_MSS;
				break;
			case 5:
				if ((optLen & 3) != 0) {
					return RudpResult(rudpRecordError("rudpHeader.cpp", 497));
				}
				m_sackCount = optLen >> 2;
				for (uint32_t i = 0; i < m_sackCount; i++) {
					if (buf->readU16(&m_sackBlocks[i].start) < 0) {
						return RudpResult(rudpRecordError("rudpHeader.cpp", 485));
					}
					if (buf->readU16(&m_sackBlocks[i].end) < 0) {
						return RudpResult(rudpRecordError("rudpHeader.cpp", 490));
					}
				}
				break;
			case 10:
				if (optLen != 4) {
					return RudpResult(rudpRecordError("rudpHeader.cpp", 515));
				}
				if (buf->readU32(&m_retryInfo) < 0) {
					return RudpResult(rudpRecordError("rudpHeader.cpp", 506));
				}
				m_optFlags |= RUDP_OPT_RETRY_INFO;
				break;
			case 11:
				if (optLen != 4) {
					return RudpResult(rudpRecordError("rudpHeader.cpp", 533));
				}
				if (buf->readU32(&m_txTimestamp) < 0) {
					return RudpResult(rudpRecordError("rudpHeader.cpp", 524));
				}
				m_optFlags |= RUDP_OPT_TX_TIMESTAMP;
				break;
			default:
				if (buf->getBytes(0, optLen) < 0) {
					return RudpResult(rudpRecordError("rudpHeader.cpp", 541));
				}
				break;
			}
		}
	}

	return RudpResult(0);
}

RudpResult RudpHeader::readAckDelay(const uint8_t *rawHeader, uint16_t *outAckSeq)
{
	if ((rawHeader[0] & RUDP_HDR_FLAG_ACK) != 0 && (rawHeader[0] & RUDP_HDR_FLAG_ACK_DELAY) != 0) {
		const uint16_t *words = (const uint16_t *)(const void *)rawHeader;
		if (outAckSeq != 0) {
			*outAckSeq = sceNetNtohs(words[2]);
		}
		return RudpResult(sceNetNtohs(words[3]));
	}
	return RudpResult(rudpRecordError("rudpHeader.cpp", 556));
}

RudpResult RudpHeader::writeAckDelay(uint8_t *rawHeader, uint16_t delayMs)
{
	if ((rawHeader[0] & RUDP_HDR_FLAG_ACK) != 0 && (rawHeader[0] & RUDP_HDR_FLAG_ACK_DELAY) != 0) {
		uint16_t *words = (uint16_t *)(void *)rawHeader;
		words[3] = sceNetNtohs(delayMs);
		return RudpResult(0);
	}
	return RudpResult(rudpRecordError("rudpHeader.cpp", 569));
}
