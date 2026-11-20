/*
 * The account status list reply's handler: <account-status-list>, one per owner asked for, each
 * with the owner's <status> elements. No binary names this file.
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include "../../../np_activity.h"
#include "xml_parser_handlers.h"

XmlParserAccountStatusListHandler::XmlParserAccountStatusListHandler(
	AbstractXmlParserSubject *subject, NpActivityStatusList *lists, SceSize count)
{
	SceSize i;

	m_subject = subject;
	m_lists = lists;
	m_count = count;
	m_listIndex = -1;
	m_statusIndex = -1;
	m_full = 0;
	for (i = 0; i < count; i++)
		lists[i].count = 0;
}

XmlParserAccountStatusListHandler::~XmlParserAccountStatusListHandler()
{
}

int XmlParserAccountStatusListHandler::StartElement(const char *name, SceSize length)
{
	SceSize i;

	if (isElement(name, length, "account-status-list")) {
		m_listIndex++;
		m_statusIndex = 0;
		if (m_listIndex >= m_count || m_full) {
			/* more lists than asked for: the rest are not read */
			m_listIndex = -1;
			m_statusIndex = -1;
			m_full = 1;
		} else if (m_listIndex != (SceSize)-1) {
			for (i = 0; i < m_lists[m_listIndex].max; i++)
				sceClibMemset(&m_lists[m_listIndex].statuses[i], 0,
					sizeof(NpActivityStatus));
		}
	}
	return 0;
}

int XmlParserAccountStatusListHandler::Attribute(const char *element, SceSize elementLength,
	const char *name, SceSize nameLength, const char *value, int valueLength)
{
	int ret;
	int code;
	NpActivityStatus *status;

	ret = parseResultAttribute(element, elementLength, name, nameLength, value, valueLength);
	if (ret < 0)
		return ret;

	if (isElement(element, elementLength, "account-status-list")) {
		if (isElement(name, nameLength, "result"))
			parseDecimal(value, valueLength);
		if (isElement(name, nameLength, "permission-code")) {
			code = parseDecimal(value, valueLength);
			if (m_listIndex != (SceSize)-1 && m_statusIndex != (SceSize)-1) {
				switch (code) {
				case XML_PARSER_PERMISSION_GRANTED:
					m_lists[m_listIndex].result = 0;
					break;
				case XML_PARSER_PERMISSION_DENIED:
					m_lists[m_listIndex].result = NP_ACTIVITY_ERROR_PERMISSION_DENIED;
					break;
				case XML_PARSER_PERMISSION_NOT_AVAILABLE:
					m_lists[m_listIndex].result =
						NP_ACTIVITY_ERROR_PERMISSION_NOT_AVAILABLE;
					break;
				default:
					m_lists[m_listIndex].result = NP_ACTIVITY_ERROR_PERMISSION_DENIED;
					break;
				}
			}
		}
	}

	ret = parseOwnerIdAttribute(element, elementLength, name, nameLength, value, valueLength,
		&m_owner.type);
	if (ret >= 0 && m_listIndex != (SceSize)-1 && m_statusIndex != (SceSize)-1) {
		status = &m_lists[m_listIndex].statuses[m_statusIndex];
		return parseStatusAttribute(element, elementLength, name, nameLength, value,
			valueLength, &status->statusId, NULL, &status->type, &status->creationTime,
			&status->updateTime, status->platform);
	}
	return ret;
}

int XmlParserAccountStatusListHandler::Text(const char *element, SceSize elementLength,
	const char *text, int textLength)
{
	int ret;
	int found;
	SceSize i;
	char name[XML_PARSER_HANDLER_NAME_MAX];

	found = 0;
	ret = parseOwnerIdText(element, elementLength, text, textLength, &m_owner, &found);
	if (ret >= 0) {
		if (found) {
			/* the list's owner owns each of its statuses */
			if (m_listIndex != (SceSize)-1) {
				for (i = 0; i < m_lists[m_listIndex].max; i++)
					sceClibMemcpy_safe(&m_lists[m_listIndex].statuses[i].owner,
						&m_owner, sizeof(m_owner));
			}
		} else {
			/* an element it does not read: its name, for a message the release
			 * build leaves out */
			sceClibSnprintf(name, sizeof(name), "%s", element);
		}
	}
	return ret;
}

int XmlParserAccountStatusListHandler::EndElement(const char *name, SceSize length)
{
	if (isElement(name, length, "status") && m_statusIndex != (SceSize)-1) {
		m_statusIndex++;
		if (m_listIndex != (SceSize)-1)
			m_lists[m_listIndex].count = m_statusIndex;
		/* no room for more of the list's statuses */
		if (m_listIndex == (SceSize)-1
			|| m_statusIndex >= (SceSize)m_lists[m_listIndex].max)
			m_statusIndex = -1;
	} else if (isElement(name, length, "activity")) {
		m_subject->NotifyStatusTimeline(m_lists);
		m_subject->NotifyResult(&m_result);
	}
	return 0;
}
