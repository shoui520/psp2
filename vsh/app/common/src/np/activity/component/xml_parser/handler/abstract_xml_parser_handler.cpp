/*
 * The base of the XML parser's reply handlers and the readers they share: numbers, text copies,
 * and the status, owner and comment elements several replies carry. No binary names this file.
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include <np_common/np_common.h>
#include "../../../common/activity_memory.h"
#include "../../../np_activity.h"
#include "../../utility/component_utility.h"
#include "abstract_xml_parser_handler.h"

AbstractXmlParserHandler::AbstractXmlParserHandler()
{
	m_utility = NULL;
	sceClibMemset(&m_result, 0, sizeof(m_result));
}

AbstractXmlParserHandler::~AbstractXmlParserHandler()
{
}

int AbstractXmlParserHandler::isElement(const char *name, SceSize length, const char *expected)
{
	SceSize expectedLength;

	expectedLength = sceClibStrnlen(expected, XML_PARSER_HANDLER_NAME_MAX);
	if (length == 0 || expectedLength == 0)
		return 0;
	if (length == expectedLength)
		return sceClibStrncasecmp(name, expected, length) == 0;
	return 0;
}

int AbstractXmlParserHandler::parseInt(const char *text, int length, int base)
{
	int value;
	char *buffer;

	value = 0;
	if (length > 0 && length <= XML_PARSER_HANDLER_TEXT_MAX) {
		buffer = (char *)activityMalloc(length + 1);
		if (buffer != NULL) {
			sceClibMemcpy_safe(buffer, text, length);
			buffer[length] = '\0';
			value = SceNpCommon_4C4AA451(buffer, NULL, base);
			activityFree(buffer);
		}
	}
	return value;
}

int AbstractXmlParserHandler::parseDecimal(const char *text, int length)
{
	return parseInt(text, length, 10);
}

int AbstractXmlParserHandler::parseHex(const char *text, int length)
{
	return parseInt(text, length, 16);
}

int AbstractXmlParserHandler::copyText(const char *text, int length, char *dst, int dstSize,
	int *copied)
{
	if (length <= 0 || length > XML_PARSER_HANDLER_TEXT_MAX || dst == NULL)
		return 0;
	if (dstSize < length + 1)
		length = dstSize - 1;
	*copied = length;
	sceClibMemcpy_safe(dst, text, length);
	/* one past the copy: the buffers are cleared before */
	dst[*copied + 1] = '\0';
	return 1;
}

int AbstractXmlParserHandler::convertOwnerType(int type)
{
	switch (type) {
	case NP_ACTIVITY_OWNER_USER:
		return NP_ACTIVITY_OWNER_USER;
	case NP_ACTIVITY_OWNER_TITLE:
		return NP_ACTIVITY_OWNER_TITLE;
	default:
		return XML_PARSER_HANDLER_OWNER_UNKNOWN;
	}
}

__attribute__((noinline)) int AbstractXmlParserHandler::convertStatusType(int type)
{
	switch (type) {
	case 0:
		return 0;
	case 1:
		return 1;
	case 2:
		return 2;
	case 3:
		return 3;
	case 4:
		return 4;
	case 5:
		return 5;
	case 99:
		return 99;
	default:
		return type;
	}
}

int AbstractXmlParserHandler::parseResultAttribute(const char *element, SceSize elementLength,
	const char *name, SceSize nameLength, const char *value, int valueLength)
{
	if (isElement(element, elementLength, "activity")) {
		if (isElement(name, nameLength, "result"))
			m_result.result = parseHex(value, valueLength);
	}
	return 0;
}

int AbstractXmlParserHandler::parseStatusAttribute(const char *element, SceSize elementLength,
	const char *name, SceSize nameLength, const char *value, int valueLength, int *statusId,
	int *hasId, int *type, SceUInt64 *creationTime, SceUInt64 *updateTime, char *platform)
{
	int ret;
	int copied;
	int id;
	char pf[XML_PARSER_HANDLER_PLATFORM_SIZE];
	char date[XML_PARSER_HANDLER_TIME_SIZE];
	char update[XML_PARSER_HANDLER_TIME_SIZE];

	ret = 0;
	copied = 0;
	if (isElement(element, elementLength, "status")) {
		if (isElement(name, nameLength, "id")) {
			id = parseDecimal(value, valueLength);
			if (statusId != NULL)
				*statusId = id;
			if (hasId != NULL)
				*hasId = 1;
		}
		if (isElement(name, nameLength, "pf")) {
			sceClibMemset(pf, 0, sizeof(pf));
			if (copyText(value, valueLength, pf, sizeof(pf), &copied))
				sceClibStrncpy(platform, pf, sizeof(pf));
		}
		if (isElement(name, nameLength, "type")) {
			id = parseDecimal(value, valueLength);
			if (type != NULL)
				*type = convertStatusType(id);
		}
		if (isElement(name, nameLength, "date")) {
			sceClibMemset(date, 0, sizeof(date));
			if (copyText(value, valueLength, date, sizeof(date), &copied)
				&& creationTime != NULL)
				ret = m_utility->ConvertTimeTextToSceRtcTick(date,
					(SceRtcTick *)creationTime);
		}
		if (isElement(name, nameLength, "update")) {
			sceClibMemset(update, 0, sizeof(update));
			if (copyText(value, valueLength, update, sizeof(update), &copied)
				&& updateTime != NULL)
				ret = m_utility->ConvertTimeTextToSceRtcTick(update,
					(SceRtcTick *)updateTime);
		}
	}
	return ret;
}

int AbstractXmlParserHandler::parseOwnerIdAttribute(const char *element, SceSize elementLength,
	const char *name, SceSize nameLength, const char *value, int valueLength, int *type)
{
	int ownerType;

	if (isElement(element, elementLength, "ownerid")) {
		if (isElement(name, nameLength, "type")) {
			ownerType = parseDecimal(value, valueLength);
			if (type != NULL)
				*type = convertOwnerType(ownerType);
		}
	}
	return 0;
}

int AbstractXmlParserHandler::parseOwnerIdText(const char *element, SceSize elementLength,
	const char *text, int textLength, NpActivityOwner *owner, int *found)
{
	char jid[XML_PARSER_HANDLER_JID_SIZE];
	SceSize length;

	if (owner != NULL && isElement(element, elementLength, "ownerid")) {
		if (found != NULL)
			*found = 1;
		if (owner->type == NP_ACTIVITY_OWNER_USER) {
			sceClibMemset(jid, 0, sizeof(jid));
			length = (SceSize)textLength > sizeof(jid) - 1 ? sizeof(jid) - 1 : textLength;
			sceClibMemcpy_safe(jid, text, length);
			jid[length] = '\0';
			m_utility->ConvertJidToNpId(jid, &owner->npId);
		} else if (owner->type == NP_ACTIVITY_OWNER_TITLE) {
			sceClibMemset(owner->titleId, 0, sizeof(owner->titleId));
			sceClibMemcpy_safe(owner->titleId, text,
				textLength > (int)sizeof(owner->titleId) - 1
				? (int)sizeof(owner->titleId) - 1 : textLength);
		}
	}
	return 0;
}

int AbstractXmlParserHandler::parseStatusInfoText(const char *element, SceSize elementLength,
	const char *text, int textLength, NpActivityStatusInfo *info, int *found)
{
	*found = 0;
	if (info == NULL)
		return 0;

	if (isElement(element, elementLength, "titleid")) {
		sceClibMemset(info->titleId, 0, NP_ACTIVITY_TITLE_ID_SIZE);
		sceClibMemcpy_safe(info->titleId, text, textLength > NP_ACTIVITY_TITLE_ID_SIZE
			? NP_ACTIVITY_TITLE_ID_SIZE : textLength);
		info->titleId[NP_ACTIVITY_TITLE_ID_SIZE - 1] = '\0';
		*found = 1;
	} else if (isElement(element, elementLength, "title-name")) {
		sceClibMemset(info->titleName, 0, sizeof(info->titleName));
		sceClibMemcpy_safe(info->titleName, text, textLength >= (int)sizeof(info->titleName)
			? (int)sizeof(info->titleName) - 1 : textLength);
		*found = 1;
	} else if (isElement(element, elementLength, "contentid")) {
		sceClibMemset(info->contentId, 0, sizeof(info->contentId));
		sceClibMemcpy_safe(info->contentId, text, textLength > (int)sizeof(info->contentId)
			? (int)sizeof(info->contentId) : textLength);
		info->contentId[sizeof(info->contentId) - 1] = '\0';
		*found = 1;
	} else if (isElement(element, elementLength, "text")) {
		sceClibMemset(info->message, 0, sizeof(info->message));
		sceClibMemcpy_safe(info->message, text, textLength >= (int)sizeof(info->message)
			? (int)sizeof(info->message) - 1 : textLength);
		*found = 1;
	} else if (isElement(element, elementLength, "icon-url")) {
		sceClibMemset(info->iconUrl, 0, sizeof(info->iconUrl));
		sceClibMemcpy_safe(info->iconUrl, text, textLength >= (int)sizeof(info->iconUrl)
			? (int)sizeof(info->iconUrl) - 1 : textLength);
		*found = 1;
	} else if (isElement(element, elementLength, "action-url")) {
		sceClibMemset(info->actionUrl, 0, sizeof(info->actionUrl));
		sceClibMemcpy_safe(info->actionUrl, text, textLength >= (int)sizeof(info->actionUrl)
			? (int)sizeof(info->actionUrl) - 1 : textLength);
		*found = 1;
	} else if (isElement(element, elementLength, "info")) {
		sceClibMemset(info->info, 0, sizeof(info->info));
		info->infoSize = 0;
		if ((int)m_utility->GetBase64DecodedSize(textLength) <= (int)sizeof(info->info))
			info->infoSize = m_utility->Base64Decode(text, textLength, info->info,
				sizeof(info->info));
		*found = 1;
	} else if (isElement(element, elementLength, "total-comments")) {
		info->numOfComments = parseDecimal(text, textLength);
		*found = 1;
	} else if (isElement(element, elementLength, "total-votes")) {
		info->numOfVotes = parseDecimal(text, textLength);
		*found = 1;
	} else if (isElement(element, elementLength, "from-name")) {
		sceClibMemset(info->fromName, 0, sizeof(info->fromName));
		sceClibMemcpy_safe(info->fromName, text, textLength >= (int)sizeof(info->fromName)
			? (int)sizeof(info->fromName) - 1 : textLength);
		*found = 1;
	} else if (isElement(element, elementLength, "panel-url")) {
		sceClibMemset(info->panelUrl, 0, sizeof(info->panelUrl));
		sceClibMemcpy_safe(info->panelUrl, text, textLength >= (int)sizeof(info->panelUrl)
			? (int)sizeof(info->panelUrl) - 1 : textLength);
		*found = 1;
	} else if (isElement(element, elementLength, "avatar-url")) {
		sceClibMemset(info->avatarUrl, 0, XML_PARSER_HANDLER_URL_SIZE);
		sceClibMemcpy_safe(info->avatarUrl, text, textLength >= XML_PARSER_HANDLER_URL_SIZE
			? XML_PARSER_HANDLER_URL_SIZE - 1 : textLength);
		*found = 1;
	}
	return 0;
}

int AbstractXmlParserHandler::parsePanelUrlAttribute(const char *element, SceSize elementLength,
	const char *name, SceSize nameLength, const char *value, int valueLength,
	SceUInt32 *bgColor, int *found)
{
	SceUInt32 color;
	char *buffer;

	*found = 0;
	if (isElement(element, elementLength, "panel-url")) {
		if (isElement(name, nameLength, "bgc")) {
			color = 0;
			if (valueLength > 0 && valueLength <= XML_PARSER_HANDLER_TEXT_MAX) {
				buffer = (char *)activityMalloc(valueLength + 1);
				if (buffer != NULL) {
					sceClibMemcpy_safe(buffer, value, valueLength);
					buffer[valueLength] = '\0';
					color = SceNpCommon_CF843378(buffer, NULL, 16);
					activityFree(buffer);
				}
			}
			if (bgColor != NULL)
				*bgColor = color;
			*found = 1;
		}
	}
	return 0;
}

int AbstractXmlParserHandler::parseCommentAttribute(const char *element, SceSize elementLength,
	const char *name, SceSize nameLength, const char *value, int valueLength, int *commentId,
	SceUInt64 *creationTime, char *platform)
{
	int ret;
	int copied;
	int id;
	char pf[XML_PARSER_HANDLER_PLATFORM_SIZE];
	char date[XML_PARSER_HANDLER_TIME_SIZE];

	ret = 0;
	copied = 0;
	if (isElement(element, elementLength, "comment")) {
		if (isElement(name, nameLength, "id")) {
			id = parseDecimal(value, valueLength);
			if (commentId != NULL)
				*commentId = id;
		}
		if (platform != NULL && isElement(name, nameLength, "pf")) {
			sceClibMemset(pf, 0, sizeof(pf));
			if (copyText(value, valueLength, pf, sizeof(pf), &copied))
				sceClibStrncpy(platform, pf, sizeof(pf));
		}
		if (isElement(name, nameLength, "date")) {
			sceClibMemset(date, 0, sizeof(date));
			if (copyText(value, valueLength, date, sizeof(date), &copied)
				&& creationTime != NULL)
				ret = m_utility->ConvertTimeTextToSceRtcTick(date,
					(SceRtcTick *)creationTime);
		}
	}
	return ret;
}

int AbstractXmlParserHandler::parseCommentText(const char *element, SceSize elementLength,
	const char *text, int textLength, NpActivityCommentBody *comment, int *found)
{
	if (comment == NULL)
		return 0;

	if (isElement(element, elementLength, "text")) {
		sceClibMemset(comment->message, 0, sizeof(comment->message));
		sceClibMemcpy_safe(comment->message, text, textLength >= (int)sizeof(comment->message)
			? (int)sizeof(comment->message) - 1 : textLength);
		*found = 1;
	} else if (isElement(element, elementLength, "info")) {
		sceClibMemset(comment->info, 0, sizeof(comment->info));
		comment->infoSize = 0;
		if ((int)m_utility->GetBase64DecodedSize(textLength) > (int)sizeof(comment->info)) {
			*found = 1;
			return 0;
		}
		comment->infoSize = m_utility->Base64Decode(text, textLength, comment->info,
			sizeof(comment->info));
		*found = 1;
	}
	return 0;
}

int AbstractXmlParserHandler::parseFromIdAttribute(const char *element, SceSize elementLength,
	const char *name, SceSize nameLength, const char *value, int valueLength, int *type)
{
	int ownerType;

	if (isElement(element, elementLength, "fromid")) {
		if (isElement(name, nameLength, "type")) {
			ownerType = parseDecimal(value, valueLength);
			if (type != NULL)
				*type = convertOwnerType(ownerType);
		}
	}
	return 0;
}

int AbstractXmlParserHandler::parseFromIdText(const char *element, SceSize elementLength,
	const char *text, int textLength, NpActivityOwner *owner, int *found)
{
	char jid[XML_PARSER_HANDLER_JID_SIZE];
	SceSize length;

	if (owner != NULL && isElement(element, elementLength, "fromid")) {
		if (found != NULL)
			*found = 1;
		if (owner->type == NP_ACTIVITY_OWNER_USER) {
			sceClibMemset(jid, 0, sizeof(jid));
			length = (SceSize)textLength > sizeof(jid) - 1 ? sizeof(jid) - 1 : textLength;
			sceClibMemcpy_safe(jid, text, length);
			jid[length] = '\0';
			m_utility->ConvertJidToNpId(jid, &owner->npId);
		} else if (owner->type == NP_ACTIVITY_OWNER_TITLE) {
			sceClibMemset(owner->titleId, 0, sizeof(owner->titleId));
			sceClibMemcpy_safe(owner->titleId, text,
				textLength > (int)sizeof(owner->titleId) - 1
				? (int)sizeof(owner->titleId) - 1 : textLength);
		}
	}
	return 0;
}
