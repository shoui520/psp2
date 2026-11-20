/*
 * The base of the XML parser's reply handlers, 12 bytes: the parser hands a handler the elements,
 * attributes and text of the schema it parses, and the handler fills its request's results and
 * notifies the subject when the reply ends. With the readers the handlers share: numbers, text
 * copies, and the status, owner and comment elements several replies carry. No binary names
 * these classes, methods or files; the names are ours.
 */
#ifndef NP_ACTIVITY_COMPONENT_XML_PARSER_HANDLER_ABSTRACT_XML_PARSER_HANDLER_H
#define NP_ACTIVITY_COMPONENT_XML_PARSER_HANDLER_ABSTRACT_XML_PARSER_HANDLER_H

#include <scetypes.h>
#include "../../../common/activity_object.h"
#include "../../../np_activity.h"
#include "../observer/abstract_xml_parser_subject.h"

/* the longest text a number or a copy is read from */
#define XML_PARSER_HANDLER_TEXT_MAX	2048
/* an element name's longest length */
#define XML_PARSER_HANDLER_NAME_MAX	0x80
/* a time's text (RFC 3339) */
#define XML_PARSER_HANDLER_TIME_SIZE	128
/* a platform's text (pf="psp2") */
#define XML_PARSER_HANDLER_PLATFORM_SIZE	5
/* a JID's text */
#define XML_PARSER_HANDLER_JID_SIZE	65
/* a URL's text (the avatar URL's field has 2 bytes more) */
#define XML_PARSER_HANDLER_URL_SIZE	257
/* an info's bytes, base64-decoded */
#define XML_PARSER_HANDLER_INFO_SIZE	256

/* the owner types a reply gives (type="..."): NP_ACTIVITY_OWNER_*, else this */
#define XML_PARSER_HANDLER_OWNER_UNKNOWN	2

class ComponentUtility;

class AbstractXmlParserHandler : public ActivityObject {
public:
	AbstractXmlParserHandler();
	virtual ~AbstractXmlParserHandler();

	virtual int StartElement(const char *name, SceSize length)
	{
		return -1;
	}
	virtual int Attribute(const char *element, SceSize elementLength, const char *name,
		SceSize nameLength, const char *value, int valueLength)
	{
		return -1;
	}
	virtual int Text(const char *element, SceSize elementLength, const char *text,
		int textLength)
	{
		return -1;
	}
	virtual int EndElement(const char *name, SceSize length)
	{
		return -1;
	}

	ComponentUtility *m_utility;	/* the parser's utility */

protected:
	/* 1 when name (length bytes) is expected, ignoring case */
	int isElement(const char *name, SceSize length, const char *expected);
	/* the number text (length bytes) gives, in base; 0 without one */
	int parseInt(const char *text, int length, int base);
	int parseDecimal(const char *text, int length);
	int parseHex(const char *text, int length);
	/* copies text (length bytes) into dst, at most dstSize - 1 bytes; 1 when it copied */
	int copyText(const char *text, int length, char *dst, int dstSize, int *copied);
	int convertOwnerType(int type);
	int convertStatusType(int type);

	/* <activity result="..."> */
	int parseResultAttribute(const char *element, SceSize elementLength, const char *name,
		SceSize nameLength, const char *value, int valueLength);
	/* <status id pf type date update> */
	int parseStatusAttribute(const char *element, SceSize elementLength, const char *name,
		SceSize nameLength, const char *value, int valueLength, int *statusId, int *hasId,
		int *type, SceUInt64 *creationTime, SceUInt64 *updateTime, char *platform);
	/* <ownerid type="..."> and its text */
	int parseOwnerIdAttribute(const char *element, SceSize elementLength, const char *name,
		SceSize nameLength, const char *value, int valueLength, int *type);
	int parseOwnerIdText(const char *element, SceSize elementLength, const char *text,
		int textLength, NpActivityOwner *owner, int *found);
	/* a status's elements: titleid, title-name, contentid, text, icon-url, ... */
	int parseStatusInfoText(const char *element, SceSize elementLength, const char *text,
		int textLength, NpActivityStatusInfo *info, int *found);
	/* <panel-url bgc="..."> */
	int parsePanelUrlAttribute(const char *element, SceSize elementLength, const char *name,
		SceSize nameLength, const char *value, int valueLength, SceUInt32 *bgColor,
		int *found);
	/* <comment id pf date> and its elements */
	int parseCommentAttribute(const char *element, SceSize elementLength, const char *name,
		SceSize nameLength, const char *value, int valueLength, int *commentId,
		SceUInt64 *creationTime, char *platform);
	int parseCommentText(const char *element, SceSize elementLength, const char *text,
		int textLength, NpActivityCommentBody *comment, int *found);
	/* <fromid type="..."> and its text */
	int parseFromIdAttribute(const char *element, SceSize elementLength, const char *name,
		SceSize nameLength, const char *value, int valueLength, int *type);
	int parseFromIdText(const char *element, SceSize elementLength, const char *text,
		int textLength, NpActivityOwner *owner, int *found);

	XmlParserResult m_result;	/* the reply's result */
};

#endif
