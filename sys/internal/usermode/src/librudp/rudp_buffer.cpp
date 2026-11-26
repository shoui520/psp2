/*
 * rudp_buffer.cpp: byte stream read/write buffer helper.
 */
#include "rudp_internal.h"

RudpBuffer::RudpBuffer()
	: m_data(0), m_length(0), m_capacity(0), m_offset(0)
{
}

RudpBuffer::RudpBuffer(void *data, uint32_t capacity)
	: m_data((uint8_t *)data), m_length(0), m_capacity(capacity), m_offset(0)
{
}

RudpBuffer::~RudpBuffer()
{
}

int RudpBuffer::writeU8(uint8_t value)
{
	if (m_offset + m_length + 1 > m_capacity) {
		return -1;
	}
	uint8_t byteVal = value;
	rudpMemcpy(m_data + m_offset + m_length, &byteVal, 1);
	m_length += 1;
	return 0;
}

int RudpBuffer::writeU16(uint16_t value)
{
	uint16_t netVal = sceNetNtohs(value);
	if (m_offset + m_length + 2 > m_capacity) {
		return -1;
	}
	rudpMemcpy(m_data + m_offset + m_length, &netVal, 2);
	m_length += 2;
	return 0;
}

int RudpBuffer::writeU32(uint32_t value)
{
	uint32_t netVal = sceNetNtohl(value);
	if (m_offset + m_length + 4 > m_capacity) {
		return -1;
	}
	rudpMemcpy(m_data + m_offset + m_length, &netVal, 4);
	m_length += 4;
	return 0;
}

int RudpBuffer::putBytes(const void *src, uint32_t len)
{
	if (m_offset + m_length + len > m_capacity) {
		return -1;
	}
	rudpMemcpy(m_data + m_offset + m_length, src, len);
	m_length += len;
	return 0;
}

int RudpBuffer::readU8(uint8_t *out)
{
	if (m_length == 0) {
		return -1;
	}
	if (out != 0) {
		rudpMemcpy(out, m_data + m_offset, 1);
	}
	m_offset += 1;
	m_length -= 1;
	return 0;
}

int RudpBuffer::readU16(uint16_t *out)
{
	if (m_length < 2) {
		return -1;
	}
	if (out != 0) {
		rudpMemcpy(out, m_data + m_offset, 2);
	}
	uint16_t raw = *out;
	m_offset += 2;
	m_length -= 2;
	*out = sceNetNtohs(raw);
	return 0;
}

int RudpBuffer::readU32(uint32_t *out)
{
	if (m_length < 4) {
		return -1;
	}
	if (out != 0) {
		rudpMemcpy(out, m_data + m_offset, 4);
	}
	m_offset += 4;
	m_length -= 4;
	*out = sceNetNtohl(*out);
	return 0;
}

int RudpBuffer::getBytes(void *dst, uint32_t len)
{
	if (m_length < len) {
		return -1;
	}
	if (dst != 0) {
		rudpMemcpy(dst, m_data + m_offset, len);
	}
	m_offset += len;
	m_length -= len;
	return 0;
}

int RudpBuffer::writeAt(uint32_t offset, const void *src, uint32_t len)
{
	if (m_length < offset) {
		return -1;
	}
	if (m_length - offset < len) {
		return -1;
	}
	if (src == 0 && len != 0) {
		return -1;
	}
	rudpMemcpy(m_data + m_offset + offset, src, len);
	return 0;
}

void RudpBuffer::setLength(uint32_t len)
{
	m_length = len;
}

void RudpBuffer::reset()
{
	m_offset = 0;
	m_length = 0;
}
