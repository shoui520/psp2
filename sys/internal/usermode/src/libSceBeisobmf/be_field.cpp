/*
 * Typed ISO box field (BeField).
 */

#include "be_isobmf_internal.h"

BeField::BeField()
	: m_heapData(NULL)
	, m_dataSize(0)
	, m_dataType(BE_FIELD_NONE)
	, m_reservedValue(0)
	, m_skipAlloc(0)
	, m_incomplete(1)
{
	m_name[0] = '\0';
	beMemset(m_inlineData, 0, sizeof(m_inlineData));
}

BeField::~BeField()
{
	if (m_heapData != NULL) {
		BeMemObject::operator delete[](m_heapData);
		m_heapData = NULL;
	}
}

int BeField::isDynamicType() const
{
	return m_dataType == BE_FIELD_BYTES ||
		m_dataType == BE_FIELD_UINT16_ARRAY ||
		m_dataType == BE_FIELD_UINT32_ARRAY ||
		m_dataType == BE_FIELD_UINT64_ARRAY ||
		m_dataType == BE_FIELD_CSTRING;
}

void *BeField::getData()
{
	if (isDynamicType())
		return m_heapData;
	return m_inlineData;
}

const char *BeField::getName() const
{
	return m_name;
}

uint32_t BeField::getType() const
{
	return m_dataType;
}

uint32_t BeField::getSize() const
{
	return m_dataSize;
}

void BeField::setName(const char *name)
{
	beStrncpy(m_name, name, sizeof(m_name));
}

int BeField::setType(uint32_t type)
{
	if (m_dataType == type)
		return 1;

	switch (type) {
	case BE_FIELD_NONE:
		return 0;
	case BE_FIELD_INT8:
	case BE_FIELD_UINT8:
		m_dataSize = 1;
		break;
	case BE_FIELD_INT16:
	case BE_FIELD_UINT16:
		m_dataSize = 2;
		break;
	case BE_FIELD_INT24:
	case BE_FIELD_UINT24:
		m_dataSize = 3;
		break;
	case BE_FIELD_INT32:
	case BE_FIELD_UINT32:
		m_dataSize = 4;
		break;
	case BE_FIELD_INT64:
	case BE_FIELD_UINT64:
		m_dataSize = 8;
		break;
	case BE_FIELD_BYTES:
	case BE_FIELD_UINT16_ARRAY:
	case BE_FIELD_UINT32_ARRAY:
	case BE_FIELD_UINT64_ARRAY:
	case BE_FIELD_CSTRING:
		m_dataSize = 0;
		if (m_heapData != NULL) {
			BeMemObject::operator delete[](m_heapData);
			m_heapData = NULL;
		}
		m_incomplete = 0;
		break;
	default:
		return 0;
	}

	m_dataType = type;
	return 1;
}

int BeField::setData(const void *src)
{
	if (m_dataSize == 0)
		return 0;

	if (isDynamicType()) {
		if (m_heapData == NULL)
			return 0;
		beMemcpy(m_heapData, src, m_dataSize);
	} else {
		beMemcpy(m_inlineData, src, m_dataSize);
	}

	m_incomplete = 0;
	return 1;
}

int BeField::allocate(uint32_t size)
{
	if (size == 0 || !isDynamicType())
		return 0;

	if (m_heapData != NULL) {
		BeMemObject::operator delete[](m_heapData);
		m_heapData = NULL;
	}

	if (m_skipAlloc == 0) {
		uint8_t *buf = static_cast<uint8_t *>(BeMemObject::operator new[](size));
		m_heapData = buf;
		if (m_dataType == BE_FIELD_CSTRING)
			buf[0] = '\0';
		m_dataSize = size;
	}

	m_incomplete = 1;
	return 1;
}

int BeField::resize(uint32_t newSize)
{
	if (newSize == 0 || !isDynamicType())
		return 0;

	if (m_skipAlloc == 0) {
		void *buf = BeMemObject::operator new[](newSize);
		uint32_t copySize = (newSize < m_dataSize) ? newSize : m_dataSize;
		beMemcpy(buf, m_heapData, copySize);
		if (m_heapData != NULL)
			BeMemObject::operator delete[](m_heapData);
		m_heapData = buf;
		m_dataSize = newSize;
		m_incomplete = 0;
	}

	return 1;
}

int BeField::writeAt(uint32_t offset, const void *src, uint32_t size)
{
	if (size == 0)
		return 0;

	uint32_t endOffset = offset + size;
	if (m_dataSize < endOffset)
		return 0;

	if (isDynamicType()) {
		if (m_heapData == NULL)
			return 0;
		uint8_t *dst = static_cast<uint8_t *>(m_heapData);
		beMemcpy(&dst[offset], src, size);
	} else {
		beMemcpy(&m_inlineData[offset], src, size);
	}

	if (endOffset >= m_dataSize)
		m_incomplete = 0;

	return 1;
}

void BeField::swapEndian()
{
	if (m_dataType == BE_FIELD_BYTES || m_dataType == BE_FIELD_NONE)
		return;
	if (m_dataSize <= 1)
		return;

	uint32_t elemSize;
	switch (m_dataType) {
	case BE_FIELD_INT16:
	case BE_FIELD_UINT16:
	case BE_FIELD_UINT16_ARRAY:
		elemSize = 2;
		break;
	case BE_FIELD_INT24:
	case BE_FIELD_UINT24: {
		uint8_t swapped = m_inlineData[0];
		m_inlineData[0] = m_inlineData[2];
		m_inlineData[2] = swapped;
		return;
	}
	case BE_FIELD_INT32:
	case BE_FIELD_UINT32:
	case BE_FIELD_UINT32_ARRAY:
		elemSize = 4;
		break;
	case BE_FIELD_INT64:
	case BE_FIELD_UINT64:
	case BE_FIELD_UINT64_ARRAY:
		elemSize = 8;
		break;
	default:
		return;
	}

	uint8_t *cursor = static_cast<uint8_t *>(getData());
	for (uint32_t i = 0; i < m_dataSize / elemSize; i++) {
		if ((elemSize >> 1) != 0) {
			uint8_t *lo = cursor;
			uint8_t *hi = &cursor[elemSize - 1];
			uint32_t half = elemSize >> 1;
			do {
				uint8_t swapped = *lo;
				*lo++ = *hi;
				*hi-- = swapped;
				half--;
			} while (half != 0);
		}
		cursor += elemSize;
	}
}

uint32_t BeField::getSkipAlloc() const
{
	return m_skipAlloc;
}

uint32_t BeField::isIncomplete() const
{
	return m_incomplete;
}
