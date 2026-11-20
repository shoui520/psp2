/*
 * The add status list reply's handler: <status setindex="..." id="..." update="..."> for each
 * status posted; a status the reply does not name stays rejected. No binary names this file.
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include <rtc.h>
#include "../../../np_activity.h"
#include "../../utility/component_utility.h"
#include "xml_parser_handlers.h"

XmlParserAddStatusListHandler::XmlParserAddStatusListHandler(AbstractXmlParserSubject *subject,
	NpActivityPostResult *results, SceSize count)
{
	SceSize i;

	m_subject = subject;
	m_results = results;
	m_count = count;
	m_index = -1;
	for (i = 0; i < m_count; i++) {
		sceClibMemset(&m_results[i], 0, sizeof(NpActivityPostResult));
		m_results[i].result = NP_ACTIVITY_ERROR_POST_REJECTED;
	}
}

XmlParserAddStatusListHandler::~XmlParserAddStatusListHandler()
{
}

int XmlParserAddStatusListHandler::StartElement(const char *name, SceSize length)
{
	return 0;
}

int XmlParserAddStatusListHandler::Attribute(const char *element, SceSize elementLength,
	const char *name, SceSize nameLength, const char *value, int valueLength)
{
	int ret;
	int index;
	int id;
	int copied;
	char update[XML_PARSER_HANDLER_TIME_SIZE];

	ret = parseResultAttribute(element, elementLength, name, nameLength, value, valueLength);
	if (ret >= 0 && isElement(element, elementLength, "status")) {
		if (isElement(name, nameLength, "setindex")) {
			/* the status's place in the request, from 1 */
			index = parseDecimal(value, valueLength) - 1;
			if (index < 0 || index >= m_count)
				m_index = -1;
			else
				m_index = index;
		}
		if (isElement(name, nameLength, "id")) {
			id = parseDecimal(value, valueLength);
			if (m_index != -1) {
				m_results[m_index].statusId = id;
				m_results[m_index].result = 0;
			}
		}
		if (isElement(name, nameLength, "update")) {
			sceClibMemset(update, 0, sizeof(update));
			copied = 0;
			if (copyText(value, valueLength, update, sizeof(update), &copied)) {
				if (m_index >= 0 && m_index < m_count)
					ret = m_utility->ConvertTimeTextToSceRtcTick(update,
						(SceRtcTick *)&m_results[m_index].updateTime);
			}
		}
	}
	return ret;
}

int XmlParserAddStatusListHandler::Text(const char *element, SceSize elementLength,
	const char *text, int textLength)
{
	char name[XML_PARSER_HANDLER_NAME_MAX];

	/* an element it does not read: its name, for a message the release build leaves out */
	sceClibSnprintf(name, sizeof(name), "%s", element);
	return 0;
}

int XmlParserAddStatusListHandler::EndElement(const char *name, SceSize length)
{
	if (isElement(name, length, "activity")) {
		m_subject->NotifyStatusList(m_results);
		m_subject->NotifyResult(&m_result);
	}
	return 0;
}
