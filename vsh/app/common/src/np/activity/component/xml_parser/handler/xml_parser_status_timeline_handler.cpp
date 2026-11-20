/*
 * The status timeline reply's handler: <timeline-status-list>, one per page asked for, each
 * with its <status> elements and their owners. No binary names this file.
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include "../../../np_activity.h"
#include "xml_parser_handlers.h"

/* <timeline-status-list type="...">'s text, read and not kept */
#define TIMELINE_TYPE_SIZE	10

XmlParserStatusTimelineHandler::XmlParserStatusTimelineHandler(
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

XmlParserStatusTimelineHandler::~XmlParserStatusTimelineHandler()
{
}

int XmlParserStatusTimelineHandler::StartElement(const char *name, SceSize length)
{
	SceSize i;

	if (isElement(name, length, "timeline-status-list")) {
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

int XmlParserStatusTimelineHandler::Attribute(const char *element, SceSize elementLength,
	const char *name, SceSize nameLength, const char *value, int valueLength)
{
	int ret;
	int copied;
	NpActivityStatus *status;
	char type[TIMELINE_TYPE_SIZE];

	ret = parseResultAttribute(element, elementLength, name, nameLength, value, valueLength);
	if (ret < 0)
		return ret;

	if (isElement(element, elementLength, "timeline-status-list")) {
		if (isElement(name, nameLength, "type")) {
			sceClibMemset(type, 0, sizeof(type));
			copied = 0;
			copyText(value, valueLength, type, sizeof(type), &copied);
		}
		if (isElement(name, nameLength, "start"))
			parseDecimal(value, valueLength);
		if (isElement(name, nameLength, "result"))
			parseDecimal(value, valueLength);
	}
	if (m_listIndex != (SceSize)-1 && m_statusIndex != (SceSize)-1) {
		status = &m_lists[m_listIndex].statuses[m_statusIndex];
		ret = parseStatusAttribute(element, elementLength, name, nameLength, value,
			valueLength, &status->statusId, NULL, &status->type, &status->creationTime,
			&status->updateTime, status->platform);
		if (ret < 0)
			return ret;
	}
	if (m_listIndex != (SceSize)-1 && m_statusIndex != (SceSize)-1)
		ret = parseOwnerIdAttribute(element, elementLength, name, nameLength, value,
			valueLength, &m_lists[m_listIndex].statuses[m_statusIndex].owner.type);
	return ret;
}

int XmlParserStatusTimelineHandler::Text(const char *element, SceSize elementLength,
	const char *text, int textLength)
{
	int ret;
	int found;
	char name[XML_PARSER_HANDLER_NAME_MAX];

	ret = 0;
	if (m_listIndex != (SceSize)-1 && m_statusIndex != (SceSize)-1) {
		found = 0;
		ret = parseOwnerIdText(element, elementLength, text, textLength,
			&m_lists[m_listIndex].statuses[m_statusIndex].owner, &found);
		if (ret < 0 || found)
			return ret;
	}
	/* an element it does not read: its name, for a message the release build leaves out */
	sceClibSnprintf(name, sizeof(name), "%s", element);
	return ret;
}

int XmlParserStatusTimelineHandler::EndElement(const char *name, SceSize length)
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
