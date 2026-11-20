/*
 * The status detail reply's handler: one <status>'s info, its <comment-list> and its
 * <vote-list> (the users who gave it a good job). No binary names this file.
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include "../../../np_activity.h"
#include "../../utility/component_utility.h"
#include "xml_parser_handlers.h"

XmlParserStatusDetailHandler::XmlParserStatusDetailHandler(AbstractXmlParserSubject *subject,
	NpActivityStatusInfo *info, NpActivityComment *comments, SceSize maxComments,
	int *numComments, int *isVoted, SceNpId *voters, SceSize maxVoters, SceUInt32 *numVoters,
	int reserved, int *numStatuses)
{
	m_subject = subject;
	m_info = info;
	m_comments = comments;
	m_maxComments = maxComments;
	m_numComments = numComments;
	m_isVoted = isVoted;
	m_voters = voters;
	m_maxVoters = maxVoters;
	m_numVoters = numVoters;
	m_reserved = reserved;
	m_numStatuses = numStatuses;
	m_hasStatusId = 0;
	m_inComment = 0;
	m_inVoter = 0;
	m_commentIndex = -1;
	m_voterIndex = -1;
	sceClibMemset(info, 0, sizeof(NpActivityStatusInfo));
	sceClibMemset(&m_key, 0, sizeof(m_key));
	if (m_numStatuses != NULL)
		*m_numStatuses = 0;
}

XmlParserStatusDetailHandler::~XmlParserStatusDetailHandler()
{
}

int XmlParserStatusDetailHandler::StartElement(const char *name, SceSize length)
{
	if (isElement(name, length, "status-list")) {
		if (m_numStatuses != NULL)
			*m_numStatuses = 0;
	}
	if (isElement(name, length, "status")) {
		m_commentIndex = 0;
		m_voterIndex = 0;
		*m_numComments = 0;
		sceClibMemset(m_info, 0, sizeof(NpActivityStatusInfo));
		sceClibMemset(&m_key, 0, sizeof(m_key));
	}
	if (isElement(name, length, "comment")) {
		if (m_commentIndex != -1) {
			m_inComment = 1;
			sceClibMemset(&m_comments[m_commentIndex], 0, sizeof(NpActivityComment));
		}
	}
	if (isElement(name, length, "votedid")) {
		if (m_voterIndex != -1) {
			m_inVoter = 1;
			sceClibMemset(&m_voters[m_voterIndex], 0, sizeof(SceNpId));
		}
	}
	return 0;
}

int XmlParserStatusDetailHandler::Attribute(const char *element, SceSize elementLength,
	const char *name, SceSize nameLength, const char *value, int valueLength)
{
	int ret;
	int hasId;
	int found;
	NpActivityComment *comment;

	ret = parseResultAttribute(element, elementLength, name, nameLength, value, valueLength);
	if (ret < 0)
		return ret;

	hasId = 0;
	ret = parseStatusAttribute(element, elementLength, name, nameLength, value, valueLength,
		&m_key.statusId, &hasId, &m_info->type, &m_info->creationTime, &m_info->updateTime,
		m_info->platform);
	if (ret < 0)
		return ret;
	if (hasId)
		m_hasStatusId = 1;

	ret = parseOwnerIdAttribute(element, elementLength, name, nameLength, value, valueLength,
		&m_key.owner.type);
	if (ret < 0)
		return ret;

	if (isElement(element, elementLength, "comment-list")) {
		if (isElement(name, nameLength, "start"))
			parseDecimal(value, valueLength);
		if (isElement(name, nameLength, "result"))
			parseDecimal(value, valueLength);
		if (isElement(name, nameLength, "total"))
			parseDecimal(value, valueLength);
	}
	if (m_commentIndex != -1) {
		comment = &m_comments[m_commentIndex];
		ret = parseCommentAttribute(element, elementLength, name, nameLength, value,
			valueLength, &comment->commentId, &comment->body.creationTime,
			comment->body.platform);
		if (ret < 0)
			return ret;
	}
	if (m_commentIndex != -1) {
		ret = parseFromIdAttribute(element, elementLength, name, nameLength, value,
			valueLength, &m_comments[m_commentIndex].body.owner.type);
		if (ret < 0)
			return ret;
	}
	if (isElement(element, elementLength, "vote-list")) {
		if (isElement(name, nameLength, "start"))
			parseDecimal(value, valueLength);
		if (isElement(name, nameLength, "result"))
			*m_numVoters = parseDecimal(value, valueLength);
		if (isElement(name, nameLength, "total"))
			parseDecimal(value, valueLength);
	}
	if (isElement(element, elementLength, "votedid") && isElement(name, nameLength, "type"))
		convertOwnerType(parseDecimal(value, valueLength));

	found = 0;
	return parsePanelUrlAttribute(element, elementLength, name, nameLength, value, valueLength,
		&m_info->panelBgColor, &found);
}

int XmlParserStatusDetailHandler::Text(const char *element, SceSize elementLength,
	const char *text, int textLength)
{
	int ret;
	int infoFound;
	int commentFound;
	int ownerFound;
	int voted;
	SceSize length;
	char jid[XML_PARSER_HANDLER_JID_SIZE];
	char name[XML_PARSER_HANDLER_NAME_MAX];

	ret = 0;
	if (m_inComment) {
		if (m_commentIndex != -1) {
			commentFound = 0;
			ret = parseFromIdText(element, elementLength, text, textLength,
				&m_comments[m_commentIndex].body.owner, &commentFound);
			if (ret >= 0 && !commentFound) {
				commentFound = 0;
				ret = parseCommentText(element, elementLength, text, textLength,
					&m_comments[m_commentIndex].body, &commentFound);
			}
		}
		return ret;
	}
	if (m_inVoter) {
		if (m_voterIndex != -1 && isElement(element, elementLength, "votedid")) {
			sceClibMemset(jid, 0, sizeof(jid));
			length = (SceSize)textLength > sizeof(jid) - 1 ? sizeof(jid) - 1 : textLength;
			sceClibMemcpy_safe(jid, text, length);
			jid[length] = '\0';
			m_utility->ConvertJidToNpId(jid, &m_voters[m_voterIndex]);
		}
		return ret;
	}
	if (isElement(element, elementLength, "ownerid")) {
		ownerFound = 0;
		ret = parseOwnerIdText(element, elementLength, text, textLength, &m_key.owner,
			&ownerFound);
		if (ret < 0)
			return ret;
		if (ownerFound) {
			/* a title's status: the title is the owner */
			if (m_key.owner.type == NP_ACTIVITY_OWNER_TITLE) {
				sceClibMemset(m_info->titleId, 0, NP_ACTIVITY_TITLE_ID_SIZE);
				sceClibMemcpy_safe(m_info->titleId, m_key.owner.titleId,
					NP_ACTIVITY_TITLE_ID_SIZE);
				m_info->titleId[NP_ACTIVITY_TITLE_ID_SIZE - 1] = '\0';
			}
			return ret;
		}
	}
	infoFound = 0;
	ret = parseStatusInfoText(element, elementLength, text, textLength, m_info, &infoFound);
	if (ret < 0 || infoFound)
		return ret;
	if (isElement(element, elementLength, "voted-flag")) {
		voted = parseInt(text, textLength, 10);
		if (m_isVoted != NULL)
			*m_isVoted = voted != 0;
		return ret;
	}
	/* an element it does not read: its name, for a message the release build leaves out */
	sceClibSnprintf(name, sizeof(name), "%s", element);
	return ret;
}

int XmlParserStatusDetailHandler::EndElement(const char *name, SceSize length)
{
	if (isElement(name, length, "comment")) {
		if (m_commentIndex != -1) {
			m_inComment = 0;
			*m_numComments = m_commentIndex + 1;
			m_commentIndex++;
			if (m_commentIndex >= m_maxComments)
				m_commentIndex = -1;
		}
	}
	if (isElement(name, length, "votedid")) {
		if (m_voterIndex != -1) {
			m_inVoter = 0;
			*m_numVoters = m_voterIndex + 1;
			m_voterIndex++;
			if (m_voterIndex >= m_maxVoters)
				m_voterIndex = -1;
		}
	}
	if (isElement(name, length, "status")) {
		if (m_numStatuses != NULL)
			(*m_numStatuses)++;
		m_subject->NotifyStatusDetail(&m_key, m_info, m_comments, m_numComments, m_isVoted,
			m_voters, m_numVoters);
	}
	if (isElement(name, length, "activity")) {
		/* a reply without the status: it was not found */
		if (m_result.result == 0 && !m_hasStatusId)
			m_result.result = XML_PARSER_RESULT_STATUS_NOT_FOUND;
		m_subject->NotifyResult(&m_result);
	}
	return 0;
}
