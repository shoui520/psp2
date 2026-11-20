/*
 * The handler of a reply that names a status and one of its comments (a posted comment's). No binary names this file.
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include <rtc.h>
#include "../../utility/component_utility.h"
#include "xml_parser_handlers.h"

XmlParserStatusIdCommentHandler::XmlParserStatusIdCommentHandler(AbstractXmlParserSubject *subject,
	XmlParserStatusIdComment *comment)
{
	m_subject = subject;
	m_comment = comment;
	m_index = -1;
}

XmlParserStatusIdCommentHandler::~XmlParserStatusIdCommentHandler()
{
}

int XmlParserStatusIdCommentHandler::StartElement(const char *name, SceSize length)
{
	return 0;
}

int XmlParserStatusIdCommentHandler::Attribute(const char *element, SceSize elementLength,
	const char *name, SceSize nameLength, const char *value, int valueLength)
{
	int ret;
	int copied;
	char update[XML_PARSER_HANDLER_TIME_SIZE];

	ret = parseResultAttribute(element, elementLength, name, nameLength, value, valueLength);
	if (ret >= 0) {
		if (isElement(element, elementLength, "status")) {
			if (isElement(name, nameLength, "id"))
				m_comment->statusId = parseDecimal(value, valueLength);
			if (isElement(name, nameLength, "update")) {
				sceClibMemset(update, 0, sizeof(update));
				copied = 0;
				/* a time it cannot convert is left out */
				if (copyText(value, valueLength, update, sizeof(update), &copied))
					m_utility->ConvertTimeTextToSceRtcTick(update,
						(SceRtcTick *)&m_comment->updateTime);
			}
		}
		ret = parseOwnerIdAttribute(element, elementLength, name, nameLength, value,
			valueLength, &m_comment->owner.type);
		if (ret >= 0)
			ret = parseCommentAttribute(element, elementLength, name, nameLength, value,
				valueLength, &m_comment->commentId, &m_comment->creationTime, NULL);
	}
	return ret;
}

int XmlParserStatusIdCommentHandler::Text(const char *element, SceSize elementLength,
	const char *text, int textLength)
{
	int ret;
	int found;
	char name[XML_PARSER_HANDLER_NAME_MAX];

	found = 0;
	ret = parseOwnerIdText(element, elementLength, text, textLength, &m_comment->owner,
		&found);
	if (ret >= 0 && !found) {
		/* an element it does not read: its name, for a message the release build
		 * leaves out */
		sceClibSnprintf(name, sizeof(name), "%s", element);
	}
	return ret;
}

int XmlParserStatusIdCommentHandler::EndElement(const char *name, SceSize length)
{
	if (isElement(name, length, "activity")) {
		m_subject->NotifyStatusIdComment(m_comment);
		m_subject->NotifyResult(&m_result);
	}
	return 0;
}
