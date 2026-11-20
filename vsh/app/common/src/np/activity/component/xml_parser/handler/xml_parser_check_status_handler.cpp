/*
 * The check status reply's handler: <own>, <friends> and <virtual-users>, whether each has
 * statuses the user has not seen. No binary names this file.
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include "../../../np_activity.h"
#include "xml_parser_handlers.h"

XmlParserCheckStatusHandler::XmlParserCheckStatusHandler(AbstractXmlParserSubject *subject,
	NpActivityUpdates *updates)
{
	m_subject = subject;
	m_updates = updates;
}

XmlParserCheckStatusHandler::~XmlParserCheckStatusHandler()
{
}

int XmlParserCheckStatusHandler::StartElement(const char *name, SceSize length)
{
	return 0;
}

int XmlParserCheckStatusHandler::Attribute(const char *element, SceSize elementLength,
	const char *name, SceSize nameLength, const char *value, int valueLength)
{
	return parseResultAttribute(element, elementLength, name, nameLength, value, valueLength);
}

int XmlParserCheckStatusHandler::Text(const char *element, SceSize elementLength,
	const char *text, int textLength)
{
	int value;
	char name[XML_PARSER_HANDLER_NAME_MAX];

	if (isElement(element, elementLength, "own")) {
		value = parseHex(text, textLength);
		switch (value) {
		case XML_PARSER_UPDATE_NONE:
			m_updates->counts[0] = XML_PARSER_UPDATE_NONE;
			break;
		case XML_PARSER_UPDATE_NEW:
			m_updates->counts[0] = XML_PARSER_UPDATE_NEW;
			break;
		case XML_PARSER_UPDATE_UNKNOWN_TEXT:
			m_updates->counts[0] = XML_PARSER_UPDATE_UNKNOWN;
			break;
		default:
			m_updates->counts[0] = XML_PARSER_UPDATE_UNKNOWN;
			break;
		}
		return 0;
	}
	if (isElement(element, elementLength, "friends")) {
		value = parseHex(text, textLength);
		switch (value) {
		case XML_PARSER_UPDATE_NONE:
			m_updates->counts[1] = XML_PARSER_UPDATE_NONE;
			break;
		case XML_PARSER_UPDATE_NEW:
			m_updates->counts[1] = XML_PARSER_UPDATE_NEW;
			break;
		case XML_PARSER_UPDATE_UNKNOWN_TEXT:
			m_updates->counts[1] = XML_PARSER_UPDATE_UNKNOWN;
			break;
		default:
			m_updates->counts[1] = XML_PARSER_UPDATE_UNKNOWN;
			break;
		}
		return 0;
	}
	if (isElement(element, elementLength, "virtual-users")) {
		value = parseHex(text, textLength);
		switch (value) {
		case XML_PARSER_UPDATE_NONE:
			m_updates->counts[2] = XML_PARSER_UPDATE_NONE;
			break;
		case XML_PARSER_UPDATE_NEW:
			m_updates->counts[2] = XML_PARSER_UPDATE_NEW;
			break;
		case XML_PARSER_UPDATE_UNKNOWN_TEXT:
			m_updates->counts[2] = XML_PARSER_UPDATE_UNKNOWN;
			break;
		default:
			m_updates->counts[2] = XML_PARSER_UPDATE_UNKNOWN;
			break;
		}
		return 0;
	}
	/* an element it does not read: its name, for a message the release build leaves out */
	sceClibSnprintf(name, sizeof(name), "%s", element);
	return 0;
}

int XmlParserCheckStatusHandler::EndElement(const char *name, SceSize length)
{
	if (isElement(name, length, "activity")) {
		m_subject->NotifyCheckStatus(m_updates);
		m_subject->NotifyResult(&m_result);
	}
	return 0;
}
