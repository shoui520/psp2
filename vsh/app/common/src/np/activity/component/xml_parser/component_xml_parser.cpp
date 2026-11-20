/*
 * The XML parser component: the schemas of the replies, the handler and parser of the reply
 * being read, and the parser's reports handed to the handler. No binary names this file.
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include <np_common/np_common.h>
#include "../../common/activity_memory.h"
#include "../../common/activity_simple_lock.h"
#include "../../np_activity.h"
#include "handler/xml_parser_handlers.h"
#include "component_xml_parser.h"

/* the longest element or attribute name, and value, the parser reports */
#define XML_PARSER_NAME_MAX	0x100
#define XML_PARSER_VALUE_MAX	NP_XML_VALUE_SIZE

/*
 * The schemas: the elements each reply's handler is told about, level by level. A level ends
 * with an empty name; an element with text 1 has its text reported.
 */
/* the static resource (activity.xml) */
static const NpXmlElement staticResourceLevel0[] = {
	{ "!--Sce-Np-Signature:", 0 },
	{ "activity", 0 },
	{ "", 1 },
};
static const NpXmlElement staticResourceLevel1[] = {
	{ "activity-fqdn", 1 },
	{ "trophy-icon", 0 },
	{ "", 1 },
};
static const NpXmlElement staticResourceLevel2[] = {
	{ "trophy", 1 },
	{ "", 1 },
};
static const NpXmlElement *staticResourceLevels[] = {
	staticResourceLevel0, staticResourceLevel1, staticResourceLevel2
};
static const NpXmlSchema staticResourceSchema = {
	staticResourceLevels, sizeof(staticResourceLevels) / sizeof(staticResourceLevels[0])
};

/* a reply with only its result */
static const NpXmlElement resultLevel0[] = {
	{ "activity", 0 },
	{ "", 1 },
};
static const NpXmlElement *resultLevels[] = {
	resultLevel0
};
static const NpXmlSchema resultSchema = {
	resultLevels, sizeof(resultLevels) / sizeof(resultLevels[0])
};

/* the account setting */
static const NpXmlElement accountInformationLevel0[] = {
	{ "activity", 0 },
	{ "", 1 },
};
static const NpXmlElement accountInformationLevel1[] = {
	{ "user-range", 0 },
	{ "", 1 },
};
static const NpXmlElement accountInformationLevel2[] = {
	{ "public", 1 },
	{ "editable", 1 },
	{ "", 1 },
};
static const NpXmlElement *accountInformationLevels[] = {
	accountInformationLevel0, accountInformationLevel1, accountInformationLevel2
};
static const NpXmlSchema accountInformationSchema = {
	accountInformationLevels, sizeof(accountInformationLevels) / sizeof(accountInformationLevels[0])
};

/* the posted statuses' results */
static const NpXmlElement addStatusListLevel0[] = {
	{ "activity", 0 },
	{ "", 1 },
};
static const NpXmlElement addStatusListLevel1[] = {
	{ "status-list", 0 },
	{ "", 1 },
};
static const NpXmlElement addStatusListLevel2[] = {
	{ "status", 0 },
	{ "", 1 },
};
static const NpXmlElement *addStatusListLevels[] = {
	addStatusListLevel0, addStatusListLevel1, addStatusListLevel2
};
static const NpXmlSchema addStatusListSchema = {
	addStatusListLevels, sizeof(addStatusListLevels) / sizeof(addStatusListLevels[0])
};

/* the statuses of several owners */
static const NpXmlElement accountStatusListLevel0[] = {
	{ "activity", 0 },
	{ "", 1 },
};
static const NpXmlElement accountStatusListLevel1[] = {
	{ "account-status-list", 0 },
	{ "", 1 },
};
static const NpXmlElement accountStatusListLevel2[] = {
	{ "ownerid", 1 },
	{ "status", 0 },
	{ "", 1 },
};
static const NpXmlElement *accountStatusListLevels[] = {
	accountStatusListLevel0, accountStatusListLevel1, accountStatusListLevel2
};
static const NpXmlSchema accountStatusListSchema = {
	accountStatusListLevels, sizeof(accountStatusListLevels) / sizeof(accountStatusListLevels[0])
};

/* what has new statuses */
static const NpXmlElement checkStatusLevel0[] = {
	{ "activity", 0 },
	{ "", 1 },
};
static const NpXmlElement checkStatusLevel1[] = {
	{ "own", 1 },
	{ "friends", 1 },
	{ "virtual-users", 1 },
	{ "", 1 },
};
static const NpXmlElement *checkStatusLevels[] = {
	checkStatusLevel0, checkStatusLevel1
};
static const NpXmlSchema checkStatusSchema = {
	checkStatusLevels, sizeof(checkStatusLevels) / sizeof(checkStatusLevels[0])
};

/* a status's detail, comments and good jobs */
static const NpXmlElement statusDetailLevel0[] = {
	{ "activity", 0 },
	{ "", 1 },
};
static const NpXmlElement statusDetailLevel1[] = {
	{ "status-list", 0 },
	{ "", 1 },
};
static const NpXmlElement statusDetailLevel2[] = {
	{ "status", 0 },
	{ "", 1 },
};
static const NpXmlElement statusDetailLevel3[] = {
	{ "ownerid", 1 },
	{ "titleid", 1 },
	{ "title-name", 1 },
	{ "text", 1 },
	{ "contentid", 1 },
	{ "icon-url", 1 },
	{ "action-url", 1 },
	{ "info", 1 },
	{ "comment-list", 0 },
	{ "total-comments", 1 },
	{ "vote-list", 0 },
	{ "total-votes", 1 },
	{ "virtual-user-profile", 0 },
	{ "", 1 },
};
static const NpXmlElement statusDetailLevel4[] = {
	{ "comment", 0 },
	{ "voted-flag", 1 },
	{ "votedid", 1 },
	{ "from-name", 1 },
	{ "avatar-url", 1 },
	{ "panel-url", 1 },
	{ "", 1 },
};
static const NpXmlElement statusDetailLevel5[] = {
	{ "fromid", 1 },
	{ "text", 1 },
	{ "info", 1 },
	{ "", 1 },
};
static const NpXmlElement *statusDetailLevels[] = {
	statusDetailLevel0, statusDetailLevel1, statusDetailLevel2, statusDetailLevel3,
	statusDetailLevel4, statusDetailLevel5
};
static const NpXmlSchema statusDetailSchema = {
	statusDetailLevels, sizeof(statusDetailLevels) / sizeof(statusDetailLevels[0])
};

/* a posted good job's status */
static const NpXmlElement statusIdLevel0[] = {
	{ "activity", 0 },
	{ "", 1 },
};
static const NpXmlElement statusIdLevel1[] = {
	{ "status", 0 },
	{ "", 1 },
};
static const NpXmlElement statusIdLevel2[] = {
	{ "ownerid", 1 },
	{ "", 1 },
};
static const NpXmlElement *statusIdLevels[] = {
	statusIdLevel0, statusIdLevel1, statusIdLevel2
};
static const NpXmlSchema statusIdSchema = {
	statusIdLevels, sizeof(statusIdLevels) / sizeof(statusIdLevels[0])
};

/* a posted comment's status and comment */
static const NpXmlElement statusIdCommentLevel0[] = {
	{ "activity", 0 },
	{ "", 1 },
};
static const NpXmlElement statusIdCommentLevel1[] = {
	{ "status", 0 },
	{ "", 1 },
};
static const NpXmlElement statusIdCommentLevel2[] = {
	{ "ownerid", 1 },
	{ "comment", 0 },
	{ "", 1 },
};
static const NpXmlElement *statusIdCommentLevels[] = {
	statusIdCommentLevel0, statusIdCommentLevel1, statusIdCommentLevel2
};
static const NpXmlSchema statusIdCommentSchema = {
	statusIdCommentLevels, sizeof(statusIdCommentLevels) / sizeof(statusIdCommentLevels[0])
};

/* several statuses' details */
static const NpXmlElement statusInfoLevel0[] = {
	{ "activity", 0 },
	{ "", 1 },
};
static const NpXmlElement statusInfoLevel1[] = {
	{ "status-list", 0 },
	{ "", 1 },
};
static const NpXmlElement statusInfoLevel2[] = {
	{ "status", 0 },
	{ "", 1 },
};
static const NpXmlElement statusInfoLevel3[] = {
	{ "ownerid", 1 },
	{ "titleid", 1 },
	{ "title-name", 1 },
	{ "text", 1 },
	{ "contentid", 1 },
	{ "icon-url", 1 },
	{ "action-url", 1 },
	{ "info", 1 },
	{ "total-comments", 1 },
	{ "total-votes", 1 },
	{ "virtual-user-profile", 0 },
	{ "", 1 },
};
static const NpXmlElement statusInfoLevel4[] = {
	{ "from-name", 1 },
	{ "avatar-url", 1 },
	{ "panel-url", 1 },
	{ "", 1 },
};
static const NpXmlElement *statusInfoLevels[] = {
	statusInfoLevel0, statusInfoLevel1, statusInfoLevel2, statusInfoLevel3, statusInfoLevel4
};
static const NpXmlSchema statusInfoSchema = {
	statusInfoLevels, sizeof(statusInfoLevels) / sizeof(statusInfoLevels[0])
};

/* a timeline's statuses */
static const NpXmlElement statusTimelineLevel0[] = {
	{ "activity", 0 },
	{ "", 1 },
};
static const NpXmlElement statusTimelineLevel1[] = {
	{ "timeline-status-list", 0 },
	{ "", 1 },
};
static const NpXmlElement statusTimelineLevel2[] = {
	{ "status", 0 },
	{ "", 1 },
};
static const NpXmlElement statusTimelineLevel3[] = {
	{ "ownerid", 1 },
	{ "", 1 },
};
static const NpXmlElement *statusTimelineLevels[] = {
	statusTimelineLevel0, statusTimelineLevel1, statusTimelineLevel2, statusTimelineLevel3
};
static const NpXmlSchema statusTimelineSchema = {
	statusTimelineLevels, sizeof(statusTimelineLevels) / sizeof(statusTimelineLevels[0])
};

int ComponentXmlParser::ReportEvent(NpXmlParser *parser, int event, SceUInt32 depth,
	SceUInt32 index, const char *name, const char *value)
{
	int ret;
	const NpXmlElement *level;
	const char *element;
	SceSize length;

	ret = CheckAbort(0);
	if (ret == NP_ACTIVITY_ERROR_ABORTED)
		return ret;

	if (depth > parser->schema->levelCount)
		return NP_ACTIVITY_ERROR_INVALID_TIME;
	level = parser->schema->levels[depth];
	if (level == NULL)
		return NP_ACTIVITY_ERROR_INVALID_TIME;
	element = level[index].name;
	if (element == NULL)
		return NP_ACTIVITY_ERROR_INVALID_TIME;
	length = sceClibStrnlen(element, XML_PARSER_NAME_MAX);

	switch (event) {
	case NP_XML_EVENT_START:
		return m_handler->StartElement(element, length);
	case NP_XML_EVENT_END:
		return m_handler->EndElement(element, length);
	case NP_XML_EVENT_TEXT:
		if (value == NULL)
			return NP_ACTIVITY_ERROR_INVALID_TIME;
		ret = m_handler->Text(element, length, value,
			sceClibStrnlen(value, XML_PARSER_VALUE_MAX));
		if (ret < 0)
			return ret;
		return m_handler->EndElement(element, length);
	case NP_XML_EVENT_ATTRIBUTE:
		if (name == NULL || value == NULL)
			return NP_ACTIVITY_ERROR_INVALID_TIME;
		return m_handler->Attribute(element, length, name,
			sceClibStrnlen(name, XML_PARSER_NAME_MAX), value,
			sceClibStrnlen(value, XML_PARSER_VALUE_MAX));
	default:
		return NP_ACTIVITY_ERROR_INVALID_TIME;
	}
}

int ComponentXmlParser::Callback(NpXmlParser *parser, int event, SceUInt32 depth, SceUInt32 index,
	const char *name, const char *value, void *arg)
{
	if (arg == NULL)
		return NP_ACTIVITY_ERROR_INVALID_TIME;
	return ((ComponentXmlParser *)arg)->ReportEvent(parser, event, depth, index, name, value);
}

ComponentXmlParser::ComponentXmlParser()
{
	m_parser = NULL;
	m_handler = NULL;
}

ComponentXmlParser::~ComponentXmlParser()
{
}

int ComponentXmlParser::BeginStaticResource(StaticResource *resource)
{
	AbstractXmlParserHandler *handler;

	handler = new XmlParserStaticResourceHandler(
		static_cast<AbstractXmlParserSubject *>(this), resource);
	m_handler = handler;
	if (handler == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	handler->m_utility = m_utility;

	m_parser = (NpXmlParser *)activityMalloc(sizeof(NpXmlParser));
	if (m_parser == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	return SceNpCommon_650D111B(m_parser, &staticResourceSchema, Callback, this);
}

int ComponentXmlParser::BeginResult()
{
	AbstractXmlParserHandler *handler;

	handler = new XmlParserResultHandler(
		static_cast<AbstractXmlParserSubject *>(this));
	m_handler = handler;
	if (handler == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	handler->m_utility = m_utility;

	m_parser = (NpXmlParser *)activityMalloc(sizeof(NpXmlParser));
	if (m_parser == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	return SceNpCommon_650D111B(m_parser, &resultSchema, Callback, this);
}

int ComponentXmlParser::BeginAddStatusList(NpActivityPostResult *results, SceSize count)
{
	AbstractXmlParserHandler *handler;

	handler = new XmlParserAddStatusListHandler(
		static_cast<AbstractXmlParserSubject *>(this), results, count);
	m_handler = handler;
	if (handler == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	handler->m_utility = m_utility;

	m_parser = (NpXmlParser *)activityMalloc(sizeof(NpXmlParser));
	if (m_parser == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	return SceNpCommon_650D111B(m_parser, &addStatusListSchema, Callback, this);
}

int ComponentXmlParser::BeginAccountInformation(int *setting)
{
	AbstractXmlParserHandler *handler;

	handler = new XmlParserAccountInformationHandler(
		static_cast<AbstractXmlParserSubject *>(this), setting);
	m_handler = handler;
	if (handler == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	handler->m_utility = m_utility;

	m_parser = (NpXmlParser *)activityMalloc(sizeof(NpXmlParser));
	if (m_parser == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	return SceNpCommon_650D111B(m_parser, &accountInformationSchema, Callback, this);
}

int ComponentXmlParser::BeginAccountStatusList(NpActivityStatusList *lists, SceSize count)
{
	AbstractXmlParserHandler *handler;

	handler = new XmlParserAccountStatusListHandler(
		static_cast<AbstractXmlParserSubject *>(this), lists, count);
	m_handler = handler;
	if (handler == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	handler->m_utility = m_utility;

	m_parser = (NpXmlParser *)activityMalloc(sizeof(NpXmlParser));
	if (m_parser == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	return SceNpCommon_650D111B(m_parser, &accountStatusListSchema, Callback, this);
}

int ComponentXmlParser::BeginCheckStatus(NpActivityUpdates *updates)
{
	AbstractXmlParserHandler *handler;

	handler = new XmlParserCheckStatusHandler(
		static_cast<AbstractXmlParserSubject *>(this), updates);
	m_handler = handler;
	if (handler == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	handler->m_utility = m_utility;

	m_parser = (NpXmlParser *)activityMalloc(sizeof(NpXmlParser));
	if (m_parser == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	return SceNpCommon_650D111B(m_parser, &checkStatusSchema, Callback, this);
}

int ComponentXmlParser::BeginStatusDetail(NpActivityStatusInfo *info, NpActivityComment *comments,
	SceSize maxComments, int *numComments, int *isVoted, SceNpId *voters, SceSize maxVoters,
	SceUInt32 *numVoters, int reserved, int *numStatuses)
{
	AbstractXmlParserHandler *handler;

	handler = new XmlParserStatusDetailHandler(
		static_cast<AbstractXmlParserSubject *>(this), info, comments, maxComments,
		numComments, isVoted, voters, maxVoters, numVoters, reserved, numStatuses);
	m_handler = handler;
	if (handler == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	handler->m_utility = m_utility;

	m_parser = (NpXmlParser *)activityMalloc(sizeof(NpXmlParser));
	if (m_parser == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	return SceNpCommon_650D111B(m_parser, &statusDetailSchema, Callback, this);
}

int ComponentXmlParser::BeginStatusId(XmlParserStatusId *status)
{
	AbstractXmlParserHandler *handler;

	handler = new XmlParserStatusIdHandler(
		static_cast<AbstractXmlParserSubject *>(this), status);
	m_handler = handler;
	if (handler == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	handler->m_utility = m_utility;

	m_parser = (NpXmlParser *)activityMalloc(sizeof(NpXmlParser));
	if (m_parser == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	return SceNpCommon_650D111B(m_parser, &statusIdSchema, Callback, this);
}

int ComponentXmlParser::BeginStatusIdComment(XmlParserStatusIdComment *comment)
{
	AbstractXmlParserHandler *handler;

	handler = new XmlParserStatusIdCommentHandler(
		static_cast<AbstractXmlParserSubject *>(this), comment);
	m_handler = handler;
	if (handler == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	handler->m_utility = m_utility;

	m_parser = (NpXmlParser *)activityMalloc(sizeof(NpXmlParser));
	if (m_parser == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	return SceNpCommon_650D111B(m_parser, &statusIdCommentSchema, Callback, this);
}

int ComponentXmlParser::BeginStatusInfo(int *count, NpActivityStatusDetail *details, SceSize max,
	int withCallback, NpActivityStatusDetail *detail)
{
	AbstractXmlParserHandler *handler;

	handler = new XmlParserStatusInfoHandler(
		static_cast<AbstractXmlParserSubject *>(this), count, details, max, withCallback, detail);
	m_handler = handler;
	if (handler == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	handler->m_utility = m_utility;

	m_parser = (NpXmlParser *)activityMalloc(sizeof(NpXmlParser));
	if (m_parser == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	return SceNpCommon_650D111B(m_parser, &statusInfoSchema, Callback, this);
}

int ComponentXmlParser::BeginStatusTimeline(NpActivityStatusList *lists, SceSize count)
{
	AbstractXmlParserHandler *handler;

	handler = new XmlParserStatusTimelineHandler(
		static_cast<AbstractXmlParserSubject *>(this), lists, count);
	m_handler = handler;
	if (handler == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	handler->m_utility = m_utility;

	m_parser = (NpXmlParser *)activityMalloc(sizeof(NpXmlParser));
	if (m_parser == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	return SceNpCommon_650D111B(m_parser, &statusTimelineSchema, Callback, this);
}

int ComponentXmlParser::Parse(const char *data, int size, int *done)
{
	int ret;

	ret = CheckAbort(0);
	if (ret == NP_ACTIVITY_ERROR_ABORTED)
		return ret;
	return SceNpCommon_88B18A5B(m_parser, data, size);
}

int ComponentXmlParser::EndParse()
{
	if (m_parser != NULL) {
		activityFree(m_parser);
		m_parser = NULL;
	}
	if (m_handler != NULL) {
		delete m_handler;
		m_handler = NULL;
	}
	return 0;
}

void ComponentXmlParser::SetAbort()
{
	ActivitySimpleLock lock(GetMutex());

	m_aborted = 1;
}

void ComponentXmlParser::ClearAbort()
{
	ActivitySimpleLock lock(GetMutex());

	m_aborted = 0;
}
