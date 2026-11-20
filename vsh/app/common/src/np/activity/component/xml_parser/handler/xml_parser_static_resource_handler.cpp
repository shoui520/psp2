/*
 * The static resource's handler (activity.xml): its signature comment, the service's status,
 * the activity host and the trophy icons' URLs. No binary names this file.
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include "../../../command/static_resource.h"
#include "../../../np_activity.h"
#include "xml_parser_handlers.h"

/* the end of the signature comment the parser reads as its attribute's name: "-->" less one */
#define SIGNATURE_COMMENT_END_LENGTH	2

XmlParserStaticResourceHandler::XmlParserStaticResourceHandler(AbstractXmlParserSubject *subject,
	StaticResource *resource)
{
	m_subject = subject;
	m_resource = resource;
	m_trophyType = XML_PARSER_TROPHY_NONE;
}

XmlParserStaticResourceHandler::~XmlParserStaticResourceHandler()
{
}

int XmlParserStaticResourceHandler::StartElement(const char *name, SceSize length)
{
	return 0;
}

int XmlParserStaticResourceHandler::Attribute(const char *element, SceSize elementLength,
	const char *name, SceSize nameLength, const char *value, int valueLength)
{
	int signatureCopied;
	int copied;

	if (m_resource == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;

	/* <!--Sce-Np-Signature:...-->: the parser gives the signature as the name */
	if (isElement(element, elementLength, "!--Sce-Np-Signature:")
		&& (int)nameLength > 3) {
		signatureCopied = 0;
		if (copyText(name, nameLength - SIGNATURE_COMMENT_END_LENGTH, m_resource->signature,
			sizeof(m_resource->signature) - 1, &signatureCopied))
			m_resource->signature[signatureCopied] = '\0';
	}
	if (isElement(element, elementLength, "activity") && isElement(name, nameLength, "status")) {
		if (isElement(value, valueLength, "alive"))
			m_resource->status = STATIC_RESOURCE_STATUS_ALIVE;
		else if (isElement(value, valueLength, "maintenance"))
			m_resource->status = STATIC_RESOURCE_STATUS_MAINTENANCE;
		else if (isElement(value, valueLength, "end"))
			m_resource->status = STATIC_RESOURCE_STATUS_END;
		else if (isElement(value, valueLength, "before"))
			m_resource->status = STATIC_RESOURCE_STATUS_BEFORE;
		else
			m_resource->status = STATIC_RESOURCE_STATUS_ALIVE;
	}
	if (isElement(element, elementLength, "trophy-icon") && isElement(name, nameLength, "uri")) {
		copied = 0;
		copyText(value, valueLength, m_resource->trophyIconBaseUrl,
			sizeof(m_resource->trophyIconBaseUrl), &copied);
	}
	if (isElement(element, elementLength, "trophy") && isElement(name, nameLength, "type")) {
		if (isElement(value, valueLength, "bronze"))
			m_trophyType = XML_PARSER_TROPHY_BRONZE;
		else if (isElement(value, valueLength, "silver"))
			m_trophyType = XML_PARSER_TROPHY_SILVER;
		else if (isElement(value, valueLength, "gold"))
			m_trophyType = XML_PARSER_TROPHY_GOLD;
		else if (isElement(value, valueLength, "platinum"))
			m_trophyType = XML_PARSER_TROPHY_PLATINUM;
		else if (isElement(value, valueLength, "hidden"))
			m_trophyType = XML_PARSER_TROPHY_HIDDEN;
		else
			m_trophyType = XML_PARSER_TROPHY_NONE;
	}
	return 0;
}

int XmlParserStaticResourceHandler::Text(const char *element, SceSize elementLength,
	const char *text, int textLength)
{
	int hostCopied;
	int copied;

	if (m_resource == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;

	if (isElement(element, elementLength, "activity-fqdn")) {
		hostCopied = 0;
		copyText(text, textLength, m_resource->activityFqdn,
			sizeof(m_resource->activityFqdn), &hostCopied);
	}
	if (isElement(element, elementLength, "trophy")) {
		copied = 0;
		switch (m_trophyType) {
		case XML_PARSER_TROPHY_BRONZE:
			copyText(text, textLength, m_resource->trophyIconBronze,
				sizeof(m_resource->trophyIconBronze), &copied);
			break;
		case XML_PARSER_TROPHY_SILVER:
			copyText(text, textLength, m_resource->trophyIconSilver,
				sizeof(m_resource->trophyIconSilver), &copied);
			break;
		case XML_PARSER_TROPHY_GOLD:
			copyText(text, textLength, m_resource->trophyIconGold,
				sizeof(m_resource->trophyIconGold), &copied);
			break;
		case XML_PARSER_TROPHY_PLATINUM:
			copyText(text, textLength, m_resource->trophyIconPlatinum,
				sizeof(m_resource->trophyIconPlatinum), &copied);
			break;
		case XML_PARSER_TROPHY_HIDDEN:
			copyText(text, textLength, m_resource->trophyIconHidden,
				sizeof(m_resource->trophyIconHidden), &copied);
			break;
		case XML_PARSER_TROPHY_NONE:
			break;
		}
	}
	return 0;
}

int XmlParserStaticResourceHandler::EndElement(const char *name, SceSize length)
{
	if (isElement(name, length, "activity")) {
		m_subject->NotifyStaticResource(m_resource);
		m_subject->NotifyResult(&m_result);
	}
	return 0;
}
