/*
 * The account setting reply's handler: <activity public="..." editable="...">'s values. No
 * binary names this file.
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include "../../../np_activity.h"
#include "xml_parser_handlers.h"

XmlParserAccountInformationHandler::XmlParserAccountInformationHandler(
	AbstractXmlParserSubject *subject, int *setting)
{
	m_subject = subject;
	m_setting = setting;
}

XmlParserAccountInformationHandler::~XmlParserAccountInformationHandler()
{
}

int XmlParserAccountInformationHandler::StartElement(const char *name, SceSize length)
{
	return 0;
}

int XmlParserAccountInformationHandler::Attribute(const char *element, SceSize elementLength,
	const char *name, SceSize nameLength, const char *value, int valueLength)
{
	return parseResultAttribute(element, elementLength, name, nameLength, value, valueLength);
}

/* the values a setting may take */
static inline int isAccountSettingValue(int value)
{
	return value == 1 || value == 10 || value == 20 || value == 99;
}

int XmlParserAccountInformationHandler::Text(const char *element, SceSize elementLength,
	const char *text, int textLength)
{
	int ret;
	int value;
	char name[XML_PARSER_HANDLER_NAME_MAX];

	ret = 0;
	if (m_setting == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;

	if (isElement(element, elementLength, "public")) {
		value = parseDecimal(text, textLength);
		if (!isAccountSettingValue(value))
			ret = NP_ACTIVITY_ERROR_INVALID_TIME;
		m_setting[0] = value;
		return ret;
	}
	if (isElement(element, elementLength, "editable")) {
		value = parseDecimal(text, textLength);
		if (!isAccountSettingValue(value))
			ret = NP_ACTIVITY_ERROR_INVALID_TIME;
		m_setting[1] = value;
		return ret;
	}
	/* an element it does not read: its name, for a message the release build leaves out */
	sceClibSnprintf(name, sizeof(name), "%s", element);
	return 0;
}

int XmlParserAccountInformationHandler::EndElement(const char *name, SceSize length)
{
	if (isElement(name, length, "activity")) {
		m_subject->NotifyAccountInformation(m_setting);
		m_subject->NotifyResult(&m_result);
	}
	return 0;
}
