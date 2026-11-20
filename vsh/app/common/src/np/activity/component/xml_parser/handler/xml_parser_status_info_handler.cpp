/*
 * The status info reply's handler: <status-list>'s <status> elements, each a status's detail,
 * into the caller's array or, for a callback request, one at a time into one detail reported as
 * its <status> ends. No binary names this file.
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include "../../../np_activity.h"
#include "xml_parser_handlers.h"

XmlParserStatusInfoHandler::XmlParserStatusInfoHandler(AbstractXmlParserSubject *subject,
	int *count, NpActivityStatusDetail *details, SceSize max, int withCallback,
	NpActivityStatusDetail *detail)
{
	m_subject = subject;
	m_count = count;
	m_details = details;
	m_max = max;
	m_withCallback = withCallback;
	m_detail = detail;
	m_index = -1;
	m_current = NULL;
	*count = 0;
}

XmlParserStatusInfoHandler::~XmlParserStatusInfoHandler()
{
}

int XmlParserStatusInfoHandler::StartElement(const char *name, SceSize length)
{
	if (isElement(name, length, "status-list")) {
		m_index = 0;
		*m_count = 0;
		if (m_withCallback) {
			m_current = m_detail;
		} else if (m_index < m_max) {
			m_current = &m_details[m_index];
		} else {
			m_index = -1;
			m_current = NULL;
		}
		if (m_current != NULL)
			sceClibMemset(m_current, 0, sizeof(NpActivityStatusDetail));
	}
	if (isElement(name, length, "status")) {
		if (m_current != NULL)
			sceClibMemset(m_current, 0, sizeof(NpActivityStatusDetail));
	}
	return 0;
}

int XmlParserStatusInfoHandler::Attribute(const char *element, SceSize elementLength,
	const char *name, SceSize nameLength, const char *value, int valueLength)
{
	int ret;
	int found;

	ret = parseResultAttribute(element, elementLength, name, nameLength, value, valueLength);
	if (ret < 0)
		return ret;
	if (m_current != NULL) {
		ret = parseStatusAttribute(element, elementLength, name, nameLength, value,
			valueLength, &m_current->key.statusId, NULL, &m_current->info.type,
			&m_current->info.creationTime, &m_current->info.updateTime,
			m_current->info.platform);
		if (ret < 0)
			return ret;
	}
	if (m_current != NULL) {
		ret = parseOwnerIdAttribute(element, elementLength, name, nameLength, value,
			valueLength, &m_current->key.owner.type);
		if (ret < 0)
			return ret;
	}
	if (m_current != NULL) {
		found = 0;
		ret = parsePanelUrlAttribute(element, elementLength, name, nameLength, value,
			valueLength, &m_current->info.panelBgColor, &found);
	}
	return ret;
}

int XmlParserStatusInfoHandler::Text(const char *element, SceSize elementLength,
	const char *text, int textLength)
{
	int ret;
	int found;
	char name[XML_PARSER_HANDLER_NAME_MAX];

	if (m_current == NULL)
		return 0;

	found = 0;
	ret = parseOwnerIdText(element, elementLength, text, textLength, &m_current->key.owner,
		&found);
	if (ret < 0)
		return ret;
	if (found) {
		/* a title's status: the title is the owner */
		if (m_current->key.owner.type == NP_ACTIVITY_OWNER_TITLE) {
			sceClibMemset(m_current->info.titleId, 0, NP_ACTIVITY_TITLE_ID_SIZE);
			sceClibMemcpy_safe(m_current->info.titleId, m_current->key.owner.titleId,
				NP_ACTIVITY_TITLE_ID_SIZE);
			m_current->info.titleId[NP_ACTIVITY_TITLE_ID_SIZE - 1] = '\0';
		}
		return ret;
	}
	if (m_current != NULL) {
		found = 0;
		ret = parseStatusInfoText(element, elementLength, text, textLength,
			&m_current->info, &found);
		if (ret >= 0 && !found) {
			/* an element it does not read: its name, for a message the release
			 * build leaves out */
			sceClibSnprintf(name, sizeof(name), "%s", element);
		}
	}
	return ret;
}

int XmlParserStatusInfoHandler::EndElement(const char *name, SceSize length)
{
	if (isElement(name, length, "status") && m_current != NULL) {
		m_index++;
		*m_count = m_index;
		if (m_withCallback) {
			m_subject->NotifyStatusInfo(m_current);
		} else if (m_index < m_max) {
			m_current = &m_details[m_index];
		} else {
			/* no room for more */
			m_index = -1;
			m_current = NULL;
		}
	} else if (isElement(name, length, "activity") && !m_withCallback) {
		m_subject->NotifyStatusInfoList(m_details, m_count);
		m_subject->NotifyResult(&m_result);
	}
	return 0;
}
