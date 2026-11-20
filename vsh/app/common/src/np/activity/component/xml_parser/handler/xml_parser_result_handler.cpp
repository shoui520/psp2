/*
 * The handler of a reply with only its result. No binary names this file.
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include "xml_parser_handlers.h"

XmlParserResultHandler::XmlParserResultHandler(AbstractXmlParserSubject *subject)
{
	m_subject = subject;
}

XmlParserResultHandler::~XmlParserResultHandler()
{
}

int XmlParserResultHandler::StartElement(const char *name, SceSize length)
{
	return 0;
}

int XmlParserResultHandler::Attribute(const char *element, SceSize elementLength,
	const char *name, SceSize nameLength, const char *value, int valueLength)
{
	return parseResultAttribute(element, elementLength, name, nameLength, value, valueLength);
}

int XmlParserResultHandler::Text(const char *element, SceSize elementLength, const char *text,
	int textLength)
{
	char name[XML_PARSER_HANDLER_NAME_MAX];

	/* an element it does not read: its name, for a message the release build leaves out */
	sceClibSnprintf(name, sizeof(name), "%s", element);
	return 0;
}

int XmlParserResultHandler::EndElement(const char *name, SceSize length)
{
	if (isElement(name, length, "activity"))
		m_subject->NotifyResult(&m_result);
	return 0;
}
