/*
 * The subject the XML parser reports the replies through, 48 bytes (Sony's file and the
 * Add*Observer and Remove*Observer names: abstract_xml_parser_subject.cpp's checks). One observer
 * of each kind at a time; a reply's handler notifies the observer of its kind when the reply
 * ends, then the result observer with the reply's result. The observer classes, the Notify
 * methods and the reply structs here are ours, named after the Add*Observer methods.
 */
#ifndef NP_ACTIVITY_COMPONENT_XML_PARSER_OBSERVER_ABSTRACT_XML_PARSER_SUBJECT_H
#define NP_ACTIVITY_COMPONENT_XML_PARSER_OBSERVER_ABSTRACT_XML_PARSER_SUBJECT_H

#include <np/np_common.h>
#include "../../../common/activity_object.h"
#include "../../../command/static_resource.h"
#include "../../../np_activity.h"

/* the observers of each kind it keeps */
#define XML_PARSER_SUBJECT_OBSERVER_MAX	1

/* <activity result="...">: the reply's result code (hexadecimal), 0 when it has none */
typedef struct XmlParserResult {
	int result;
} XmlParserResult;

/* a reply that names a status: a posted comment's or good job's */
typedef struct XmlParserStatusId {
	int statusId;
	NpActivityOwner owner;
	SceUInt64 updateTime;
} XmlParserStatusId;

/* a reply that names a status and one of its comments: a posted comment's */
typedef struct XmlParserStatusIdComment {
	int statusId;
	NpActivityOwner owner;
	SceUInt64 updateTime;
	int commentId;
	SceUInt64 creationTime;
} XmlParserStatusIdComment;

class StaticResourceObserver : public ActivityObject {
public:
	virtual void UpdateStaticResource(StaticResource *resource)
	{
	}
};

class ResultObserver : public ActivityObject {
public:
	/* no result returned: the commands override it */
	virtual int UpdateResult(XmlParserResult *result)
	{
	}
};

class StatusListObserver : public ActivityObject {
public:
	virtual void UpdateStatusList(NpActivityPostResult *results)
	{
	}
};

class AccountInformationObserver : public ActivityObject {
public:
	/* setting: the public and editable values */
	virtual void UpdateAccountInformation(int *setting)
	{
	}
};

class CheckStatusObserver : public ActivityObject {
public:
	virtual void UpdateCheckStatus(NpActivityUpdates *updates)
	{
	}
};

class StatusDetailObserver : public ActivityObject {
public:
	virtual void UpdateStatusDetail(NpActivityStatusKey *key, NpActivityStatusInfo *info,
		NpActivityComment *comments, int *numComments, int *isVoted, SceNpId *voters,
		SceUInt32 *numVoters)
	{
	}
};

class StatusIdObserver : public ActivityObject {
public:
	virtual void UpdateStatusId(XmlParserStatusId *status)
	{
	}
};

class StatusIdCommentObserver : public ActivityObject {
public:
	virtual void UpdateStatusIdComment(XmlParserStatusIdComment *comment)
	{
	}
};

class StatusInfoObserver : public ActivityObject {
public:
	/* one status's detail, as the reply lists it (the callback request) */
	virtual void UpdateStatusInfo(NpActivityStatusDetail *detail)
	{
	}
};

class StatusInfoListObserver : public ActivityObject {
public:
	virtual void UpdateStatusInfoList(NpActivityStatusDetail *details, int *count)
	{
	}
};

class StatusTimelineObserver : public ActivityObject {
public:
	virtual void UpdateStatusTimeline(NpActivityStatusList *lists)
	{
	}
};

class AbstractXmlParserSubject : public ActivityObject {
public:
	AbstractXmlParserSubject();
	virtual ~AbstractXmlParserSubject();

	virtual void NotifyStaticResource(StaticResource *resource);
	virtual void NotifyResult(XmlParserResult *result);
	virtual void NotifyStatusList(NpActivityPostResult *results);
	virtual void NotifyAccountInformation(int *setting);
	virtual void NotifyStatusTimeline(NpActivityStatusList *lists);
	virtual void NotifyCheckStatus(NpActivityUpdates *updates);
	virtual void NotifyStatusDetail(NpActivityStatusKey *key, NpActivityStatusInfo *info,
		NpActivityComment *comments, int *numComments, int *isVoted, SceNpId *voters,
		SceUInt32 *numVoters);
	virtual void NotifyStatusId(XmlParserStatusId *status);
	virtual void NotifyStatusIdComment(XmlParserStatusIdComment *comment);
	virtual void NotifyStatusInfoList(NpActivityStatusDetail *details, int *count);
	virtual void NotifyStatusInfo(NpActivityStatusDetail *detail);

	virtual int AddStaticResourceObserver(StaticResourceObserver *observer);
	virtual int RemoveStaticResourceObserver(StaticResourceObserver *observer);
	virtual int AddResultObserver(ResultObserver *observer);
	virtual int RemoveResultObserver(ResultObserver *observer);
	virtual int AddStatusListObserver(StatusListObserver *observer);
	virtual int RemoveStatusListObserver(StatusListObserver *observer);
	virtual int AddAccountInformationObserver(AccountInformationObserver *observer);
	virtual int RemoveAccountInformationObserver(AccountInformationObserver *observer);
	virtual int AddCheckStatusObserver(CheckStatusObserver *observer);
	virtual int RemoveCheckStatusObserver(CheckStatusObserver *observer);
	virtual int AddStatusDetailObserver(StatusDetailObserver *observer);
	virtual int RemoveStatusDetailObserver(StatusDetailObserver *observer);
	virtual int AddStatusIdObserver(StatusIdObserver *observer);
	virtual int RemoveStatusIdObserver(StatusIdObserver *observer);
	virtual int AddStatusIdCommentObserver(StatusIdCommentObserver *observer);
	virtual int RemoveStatusIdCommentObserver(StatusIdCommentObserver *observer);
	virtual int AddStatusInfoObserver(StatusInfoObserver *observer);
	virtual int RemoveStatusInfoObserver(StatusInfoObserver *observer);
	virtual int AddStatusInfoListObserver(StatusInfoListObserver *observer);
	virtual int RemoveStatusInfoListObserver(StatusInfoListObserver *observer);
	virtual int AddStatusTimelineObserver(StatusTimelineObserver *observer);
	virtual int RemoveStatusTimelineObserver(StatusTimelineObserver *observer);

private:
	StaticResourceObserver *m_staticResourceObservers[XML_PARSER_SUBJECT_OBSERVER_MAX];
	ResultObserver *m_resultObservers[XML_PARSER_SUBJECT_OBSERVER_MAX];
	StatusListObserver *m_statusListObservers[XML_PARSER_SUBJECT_OBSERVER_MAX];
	AccountInformationObserver *m_accountInformationObservers[XML_PARSER_SUBJECT_OBSERVER_MAX];
	CheckStatusObserver *m_checkStatusObservers[XML_PARSER_SUBJECT_OBSERVER_MAX];
	StatusDetailObserver *m_statusDetailObservers[XML_PARSER_SUBJECT_OBSERVER_MAX];
	StatusIdObserver *m_statusIdObservers[XML_PARSER_SUBJECT_OBSERVER_MAX];
	StatusIdCommentObserver *m_statusIdCommentObservers[XML_PARSER_SUBJECT_OBSERVER_MAX];
	StatusInfoObserver *m_statusInfoObservers[XML_PARSER_SUBJECT_OBSERVER_MAX];
	StatusInfoListObserver *m_statusInfoListObservers[XML_PARSER_SUBJECT_OBSERVER_MAX];
	StatusTimelineObserver *m_statusTimelineObservers[XML_PARSER_SUBJECT_OBSERVER_MAX];
};

#endif
