/*
 * Base ISO 14496-12 Box class (IsoBox) and endian byte-swap helper.
 */

#include "be_isobmf_internal.h"

BeField *IsoBox::findField(const char *name)
{
	m_fields.moveFirst();
	do {
		BeField *field = static_cast<BeField *>(m_fields.getCurrent());
		if (field == NULL)
			break;
		if (beStrncmp(field->getName(), name, 64) == 0)
			return field;
	} while (m_fields.moveNext() == 1);
	return NULL;
}

void IsoBox::onFieldParsed()
{
	if (findField("largesize") == NULL) {
		BeField *sizeField = findField("size");
		if (*static_cast<const uint32_t *>(sizeField->getData()) == 1) {
			m_hasExtendedHeader = 1;
			BeField *largeSize = new BeField();
			largeSize->setName("largesize");
			largeSize->setType(BE_FIELD_UINT64);
			m_fields.append(largeSize);
		}
	}

	findField("size")->getData();

	if (findField("usertype") == NULL) {
		if (findField("type")->getSkipAlloc() == 0) {
			if (beMemcmp(findField("type")->getData(), "uuid", 4) == 0) {
				m_hasExtendedHeader = 1;
				BeField *userType = new BeField();
				userType->setName("usertype");
				userType->setType(BE_FIELD_BYTES);
				m_fields.append(userType);
				findField("usertype")->allocate(16);
			}
		}
	}
}

void IsoBox::onBoxComplete()
{
}

IsoBox::IsoBox()
{
	uint32_t zeroInit = 0;
	m_parseState = ISO_BOX_STATE_INIT;

	BeField *sizeField = new BeField();
	sizeField->setName("size");
	sizeField->setType(BE_FIELD_UINT32);
	m_fields.append(sizeField);

	BeField *typeField = new BeField();
	typeField->setName("type");
	typeField->setType(BE_FIELD_BYTES);
	m_fields.append(typeField);

	findField("type")->allocate(4);
	BeField *sizeEntry = findField("size");
	if (sizeEntry != NULL)
		sizeEntry->setData(&zeroInit);
	BeField *typeEntry = findField("type");
	if (typeEntry != NULL)
		typeEntry->setData(&zeroInit);

	m_currentField = NULL;
	m_fieldIndex = 0;
	m_fallbackSize = 0;
	m_fieldBytesRead = 0;
	m_bytesRead = 0;
	m_diskOffset = 0;
	m_boxKindId = ISO_BOX_GENERIC;
	m_parser = NULL;
	m_hasExtendedHeader = 0;
}

IsoBox::~IsoBox()
{
}

void beSwapByteOrder(void *buffer, uint32_t totalBytes, uint32_t elementSize)
{
	if (totalBytes == 1 || elementSize < 2)
		return;

	uint8_t *bytes = static_cast<uint8_t *>(buffer);
	if (totalBytes == 3) {
		uint8_t swapped = bytes[0];
		bytes[0] = bytes[2];
		bytes[2] = swapped;
		return;
	}

	if (totalBytes / elementSize != 0) {
		uint32_t count = totalBytes / elementSize;
		uint32_t i = 0;
		do {
			if ((elementSize >> 1) != 0) {
				uint8_t *lo = bytes;
				uint8_t *hi = &bytes[elementSize - 1];
				uint32_t half = elementSize >> 1;
				do {
					uint8_t swapped = *lo;
					*lo++ = *hi;
					*hi-- = swapped;
					half--;
				} while (half != 0);
			}
			i++;
			bytes += elementSize;
		} while (i < count);
	}
}

uint32_t IsoBox::getParseState() const
{
	return m_parseState;
}

uint64_t IsoBox::getBytesRead() const
{
	return m_bytesRead;
}

uint32_t IsoBox::getBoxKindId() const
{
	return m_boxKindId;
}

void IsoBox::setDiskOffset(uint64_t offset)
{
	m_diskOffset = offset;
}

uint64_t IsoBox::getDiskOffset() const
{
	return m_diskOffset;
}

uint64_t IsoBox::getBoxSize()
{
	BeField *largeField = findField("largesize");
	BeField *sizeField = findField("size");

	if (largeField != NULL) {
		if (largeField->isIncomplete() == 1)
			return 16;
		return *static_cast<const uint64_t *>(largeField->getData());
	}

	if (sizeField != NULL) {
		uint32_t smallSize = *static_cast<const uint32_t *>(sizeField->getData());
		if (smallSize == 0)
			return m_fallbackSize;
		return smallSize;
	}

	return 0;
}

const char *IsoBox::getTypeStr()
{
	const char *rawType = static_cast<const char *>(findField("type")->getData());
	m_typeStr[0] = rawType[0];
	m_typeStr[1] = rawType[1];
	m_typeStr[2] = rawType[2];
	m_typeStr[3] = rawType[3];
	m_typeStr[4] = '\0';
	return m_typeStr;
}

uint64_t IsoBox::getHeaderSize()
{
	uint64_t headerBytes = 8;
	if (*static_cast<const uint32_t *>(findField("size")->getData()) == 1)
		headerBytes = 16;
	if (beStrncmp(getTypeStr(), "uuid", 4) == 0)
		return headerBytes + 16;
	return headerBytes;
}

uint64_t IsoBox::getDeclaredFieldsSize()
{
	uint64_t total = 0;
	m_fields.moveFirst();
	BeField *field = static_cast<BeField *>(m_fields.getCurrent());
	do {
		total += field->getSize();
		m_fields.moveNext();
		field = static_cast<BeField *>(m_fields.getCurrent());
	} while (field != NULL);
	return total;
}

uint32_t IsoBox::hasExtendedHeader() const
{
	return m_hasExtendedHeader;
}

int IsoBox::isContainer()
{
	return 1;
}

int IsoBox::feedData(const uint8_t *data, int size)
{
	if (size < 0)
		return 0;

	int remaining = size;
	uint8_t nulByte = 0;

	if (remaining == 0 || m_parseState == ISO_BOX_STATE_COMPLETE)
		return remaining;

	if (m_parseState == ISO_BOX_STATE_INIT)
		m_parseState = ISO_BOX_STATE_PARSING;

	if (m_bytesRead == 0) {
		m_fields.moveFirst();
		m_currentField = static_cast<BeField *>(m_fields.getCurrent());
	}

	if (m_currentField == NULL) {
		if (m_bytesRead < getBoxSize())
			m_parseState = ISO_BOX_STATE_HEADER_DONE;
		else
			m_parseState = ISO_BOX_STATE_COMPLETE;
		return remaining;
	}

	for (;;) {
		if (m_currentField->getType() == BE_FIELD_CSTRING) {
			if (*data != '\0') {
				if (beStrlen(static_cast<const char *>(m_currentField->getData())) >= 510)
					m_currentField->resize(m_currentField->getSize() + 512);
				m_currentField->writeAt(m_fieldBytesRead, data, 1);
				m_currentField->writeAt(m_fieldBytesRead + 1, &nulByte, 1);
				m_fieldBytesRead++;
				data++;
				remaining--;
			} else {
				size_t strLen = beStrlen(static_cast<const char *>(m_currentField->getData()));
				m_currentField->resize(static_cast<uint32_t>(strLen) + 1);
				uint32_t consumed = m_fieldBytesRead + 1;
				m_fieldIndex++;
				m_fieldBytesRead = 0;
				m_bytesRead += consumed;
				data++;
				remaining--;
				onFieldParsed();
				m_currentField = static_cast<BeField *>(m_currentField->getNext());
			}
		} else if (m_fieldBytesRead != 0) {
			if (static_cast<int>(m_currentField->getSize()) > static_cast<int>(remaining + m_fieldBytesRead)) {
				m_currentField->writeAt(m_fieldBytesRead, data, static_cast<uint32_t>(remaining));
				m_fieldBytesRead += static_cast<uint32_t>(remaining);
				remaining = 0;
				return remaining;
			}
			m_currentField->writeAt(m_fieldBytesRead, data, m_currentField->getSize() - m_fieldBytesRead);
			m_currentField->swapEndian();
			m_bytesRead += m_currentField->getSize();
			data += m_currentField->getSize() - m_fieldBytesRead;
			remaining -= static_cast<int>(m_currentField->getSize() - m_fieldBytesRead);
			m_fieldIndex++;
			m_fieldBytesRead = 0;
			onFieldParsed();
			m_currentField = static_cast<BeField *>(m_currentField->getNext());
		} else {
			if (static_cast<int>(m_currentField->getSize()) > remaining) {
				m_currentField->writeAt(m_fieldBytesRead, data, static_cast<uint32_t>(remaining));
				m_fieldBytesRead = static_cast<uint32_t>(remaining);
				remaining = 0;
				return remaining;
			}
			m_currentField->setData(data);
			m_currentField->swapEndian();
			m_bytesRead += m_currentField->getSize();
			data += m_currentField->getSize();
			remaining -= static_cast<int>(m_currentField->getSize());
			m_fieldIndex++;
			onFieldParsed();
			m_currentField = static_cast<BeField *>(m_currentField->getNext());
		}

		if (m_bytesRead >= getBoxSize()) {
			m_parseState = ISO_BOX_STATE_COMPLETE;
			onBoxComplete();
			return remaining;
		}
		if (remaining <= 0 || m_currentField == NULL)
			return remaining;
	}
}

void IsoBox::addField(const char *name, uint32_t type, uint32_t size)
{
	if (findField(name) != NULL)
		return;

	if ((size == 0 && type != BE_FIELD_CSTRING) ||
	    (type == BE_FIELD_INT8 && size != 1) ||
	    (type == BE_FIELD_UINT8 && size != 1) ||
	    (type == BE_FIELD_INT16 && size != 2) ||
	    (type == BE_FIELD_UINT16 && size != 2) ||
	    (type == BE_FIELD_INT24 && size != 3) ||
	    (type == BE_FIELD_UINT24 && size != 3) ||
	    (type == BE_FIELD_INT32 && size != 4) ||
	    (type == BE_FIELD_UINT32 && size != 4) ||
	    (type == BE_FIELD_INT64 && size != 8) ||
	    (type == BE_FIELD_UINT64 && size != 8))
		return;

	BeField *field = new BeField();
	field->setName(name);
	field->setType(type);
	m_fields.append(field);

	if (findField(name)->isDynamicType() == 1) {
		if (type == BE_FIELD_CSTRING)
			findField(name)->allocate(512);
		else
			findField(name)->allocate(size);
	}
}

void IsoBox::setParser(IsoParser *parser)
{
	m_parser = parser;
}

IsoParser *IsoBox::getParser() const
{
	return m_parser;
}
