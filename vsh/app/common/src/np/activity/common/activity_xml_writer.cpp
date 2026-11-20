/*
 * The XML writer (no check names Sony's file; the path is ours, after the class)
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include "../np_activity.h"
#include "activity_xml_writer.h"

ActivityXmlWriter::ActivityXmlWriter() :
	m_opened(0), m_buffer(NULL), m_size(0), m_cursor(NULL), m_remaining(0),
	m_startTagOpen(0), m_elementEnded(0)
{
}

ActivityXmlWriter::~ActivityXmlWriter()
{
}

int ActivityXmlWriter::SetBuffer(char *buffer, SceSize size)
{
	m_size = size;
	m_cursor = buffer;
	m_remaining = size;
	m_startTagOpen = 0;
	m_elementEnded = 0;
	m_buffer = buffer;
	m_opened = 1;
	return 0;
}

inline int ActivityXmlWriter::closeStartTag()
{
	int length;
	int ret;

	ret = NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	if (m_remaining >= 2) {
		length = sceClibSnprintf(m_cursor, m_remaining, ">");
		m_cursor += length;
		ret = 0;
		m_remaining -= length;
		m_startTagOpen = 0;
	}
	return ret;
}

int ActivityXmlWriter::Finish(SceSize *size)
{
	if (m_opened == 0)
		return 0;
	if (m_startTagOpen != 0)
		closeStartTag();
	if (m_remaining != 0) {
		*m_cursor = '\0';
		m_cursor++;
		m_remaining--;
	}
	m_buffer = NULL;
	*size = m_size - m_remaining;
	m_opened = 0;
	m_size = 0;
	m_cursor = NULL;
	m_remaining = 0;
	m_startTagOpen = 0;
	m_elementEnded = 0;
	return 0;
}

int ActivityXmlWriter::WriteDeclaration(const char *declaration)
{
	int length;
	int ret;

	if (m_opened == 0)
		return NP_ACTIVITY_ERROR_INVALID_STATE;
	if (m_startTagOpen != 0) {
		ret = closeStartTag();
		if (ACTIVITY_FAILED(ret))
			return ret;
	}
	m_elementEnded = 0;
	if (sceClibStrnlen(declaration, ACTIVITY_XML_TEXT_MAX) + 2 > m_remaining)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	if (m_buffer == m_cursor)
		length = sceClibSnprintf(m_cursor, m_remaining, "%s", declaration);
	else
		length = sceClibSnprintf(m_cursor, m_remaining, "\n%s", declaration);
	m_cursor += length;
	m_remaining -= length;
	return 0;
}

int ActivityXmlWriter::StartElement(const char *name)
{
	int length;
	int ret;

	if (m_opened == 0)
		return NP_ACTIVITY_ERROR_INVALID_STATE;
	if (m_startTagOpen != 0) {
		ret = closeStartTag();
		if (ACTIVITY_FAILED(ret))
			return ret;
	}
	m_elementEnded = 0;
	if (sceClibStrnlen(name, ACTIVITY_XML_NAME_MAX) + 3 > m_remaining)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	if (m_buffer == m_cursor)
		length = sceClibSnprintf(m_cursor, m_remaining, "<%s", name);
	else
		length = sceClibSnprintf(m_cursor, m_remaining, "\n<%s", name);
	m_cursor += length;
	m_remaining -= length;
	m_startTagOpen = 1;
	return 0;
}

int ActivityXmlWriter::WriteText(const char *text)
{
	SceSize length;
	int ret;

	if (m_opened == 0)
		return NP_ACTIVITY_ERROR_INVALID_STATE;
	if (m_startTagOpen != 0) {
		ret = closeStartTag();
		if (ACTIVITY_FAILED(ret))
			return ret;
	}
	m_elementEnded = 0;
	length = sceClibStrnlen(text, ACTIVITY_XML_TEXT_MAX);
	if (m_remaining < length + 1)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	sceClibMemcpy_safe(m_cursor, text, length);
	m_cursor += length;
	m_remaining -= length;
	return 0;
}

int ActivityXmlWriter::WriteInt(int value)
{
	int length;
	int ret;

	if (m_opened == 0)
		return NP_ACTIVITY_ERROR_INVALID_STATE;
	if (m_startTagOpen != 0) {
		ret = closeStartTag();
		if (ACTIVITY_FAILED(ret))
			return ret;
	}
	m_elementEnded = 0;
	if (m_remaining < ACTIVITY_XML_INT_SIZE)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	m_elementEnded = 0;
	length = sceClibSnprintf(m_cursor, m_remaining, "%d", value);
	m_cursor += length;
	m_remaining -= length;
	return 0;
}

int ActivityXmlWriter::WriteTextString(const char *text)
{
	return WriteText(text);
}

int ActivityXmlWriter::EndElement(const char *name)
{
	int length;
	int ret;

	if (m_opened == 0)
		return NP_ACTIVITY_ERROR_INVALID_STATE;
	if (m_startTagOpen != 0) {
		/* an empty element: its start tag ends it */
		ret = NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
		if (m_remaining >= 3) {
			length = sceClibSnprintf(m_cursor, m_remaining, "/>");
			m_cursor += length;
			ret = 0;
			m_remaining -= length;
			m_startTagOpen = 0;
			m_elementEnded = 1;
		}
		return ret;
	}
	if (sceClibStrnlen(name, ACTIVITY_XML_NAME_MAX) + 5 > m_remaining)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	if (m_elementEnded != 0)
		length = sceClibSnprintf(m_cursor, m_remaining, "\n</%s>", name);
	else
		length = sceClibSnprintf(m_cursor, m_remaining, "</%s>", name);
	m_cursor += length;
	m_remaining -= length;
	m_elementEnded = 1;
	return 0;
}

int ActivityXmlWriter::WriteAttribute(const char *name, const char *value)
{
	SceSize nameLength;
	int length;

	if (m_opened == 0)
		return NP_ACTIVITY_ERROR_INVALID_STATE;
	nameLength = sceClibStrnlen(name, ACTIVITY_XML_NAME_MAX);
	if (nameLength + sceClibStrnlen(value, ACTIVITY_XML_VALUE_MAX) + 5 > m_remaining)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	m_elementEnded = 0;
	length = sceClibSnprintf(m_cursor, m_remaining, " %s=\"%s\"", name, value);
	m_cursor += length;
	m_remaining -= length;
	return 0;
}

int ActivityXmlWriter::WriteAttributeInt(const char *name, int value)
{
	int length;

	if (m_opened == 0)
		return NP_ACTIVITY_ERROR_INVALID_STATE;
	if (sceClibStrnlen(name, ACTIVITY_XML_NAME_MAX) + 16 > m_remaining)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	m_elementEnded = 0;
	length = sceClibSnprintf(m_cursor, m_remaining, " %s=\"%d\"", name, value);
	m_cursor += length;
	m_remaining -= length;
	return 0;
}
