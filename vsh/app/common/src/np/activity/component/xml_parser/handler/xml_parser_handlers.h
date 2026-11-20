/*
 * The XML parser's reply handlers, one per reply the parser reads (component_xml_parser.cpp's
 * Begin* methods make them). Each fills its request's results from the reply and, when the
 * reply's <activity> ends, notifies the subject of the results, then of the reply's result.
 * No binary names these classes or their files; the names are ours, after the subject's
 * observers.
 */
#ifndef NP_ACTIVITY_COMPONENT_XML_PARSER_HANDLER_XML_PARSER_HANDLERS_H
#define NP_ACTIVITY_COMPONENT_XML_PARSER_HANDLER_XML_PARSER_HANDLERS_H

#include <np/np_common.h>
#include "../../../command/static_resource.h"
#include "../../../np_activity.h"
#include "../observer/abstract_xml_parser_subject.h"
#include "abstract_xml_parser_handler.h"

/* <activity public="..." editable="...">: the account setting's values */
class XmlParserAccountInformationHandler : public AbstractXmlParserHandler {
public:
	XmlParserAccountInformationHandler(AbstractXmlParserSubject *subject, int *setting);
	virtual ~XmlParserAccountInformationHandler();

	virtual int StartElement(const char *name, SceSize length);
	virtual int Attribute(const char *element, SceSize elementLength, const char *name,
		SceSize nameLength, const char *value, int valueLength);
	virtual int Text(const char *element, SceSize elementLength, const char *text,
		int textLength);
	virtual int EndElement(const char *name, SceSize length);

private:
	AbstractXmlParserSubject *m_subject;
	int *m_setting;			/* public, editable */
};

/* <account-status-list permission-code="...">: may the user see the owner's statuses */
#define XML_PARSER_PERMISSION_GRANTED		0
#define XML_PARSER_PERMISSION_DENIED		1
#define XML_PARSER_PERMISSION_NOT_AVAILABLE	99

/* <account-status-list><status><ownerid>: the statuses of several owners, a list each */
class XmlParserAccountStatusListHandler : public AbstractXmlParserHandler {
public:
	XmlParserAccountStatusListHandler(AbstractXmlParserSubject *subject,
		NpActivityStatusList *lists, SceSize count);
	virtual ~XmlParserAccountStatusListHandler();

	virtual int StartElement(const char *name, SceSize length);
	virtual int Attribute(const char *element, SceSize elementLength, const char *name,
		SceSize nameLength, const char *value, int valueLength);
	virtual int Text(const char *element, SceSize elementLength, const char *text,
		int textLength);
	virtual int EndElement(const char *name, SceSize length);

private:
	AbstractXmlParserSubject *m_subject;
	NpActivityStatusList *m_lists;
	SceSize m_count;
	SceSize m_listIndex;		/* the list being read, -1: none */
	SceSize m_statusIndex;		/* its status being read, -1: none */
	NpActivityOwner m_owner;	/* the list's <ownerid> */
	int m_full;			/* 1 once the reply has more lists than m_count */
};

/* <status-list><status setindex id update>: the posted statuses' results */
class XmlParserAddStatusListHandler : public AbstractXmlParserHandler {
public:
	XmlParserAddStatusListHandler(AbstractXmlParserSubject *subject,
		NpActivityPostResult *results, SceSize count);
	virtual ~XmlParserAddStatusListHandler();

	virtual int StartElement(const char *name, SceSize length);
	virtual int Attribute(const char *element, SceSize elementLength, const char *name,
		SceSize nameLength, const char *value, int valueLength);
	virtual int Text(const char *element, SceSize elementLength, const char *text,
		int textLength);
	virtual int EndElement(const char *name, SceSize length);

private:
	AbstractXmlParserSubject *m_subject;
	NpActivityPostResult *m_results;
	SceSize m_count;
	int m_index;			/* the <status setindex> being read, -1: none */
};

/* <own>, <friends>, <virtual-users>: the reply's value (hexadecimal) and what it gives */
#define XML_PARSER_UPDATE_NONE		0	/* no new statuses */
#define XML_PARSER_UPDATE_NEW		1	/* new statuses */
#define XML_PARSER_UPDATE_UNKNOWN	2	/* the network cannot say */
#define XML_PARSER_UPDATE_UNKNOWN_TEXT	0xFF	/* the reply's value for it */

/* <own>, <friends>, <virtual-users>: whether each has new statuses */
class XmlParserCheckStatusHandler : public AbstractXmlParserHandler {
public:
	XmlParserCheckStatusHandler(AbstractXmlParserSubject *subject, NpActivityUpdates *updates);
	virtual ~XmlParserCheckStatusHandler();

	virtual int StartElement(const char *name, SceSize length);
	virtual int Attribute(const char *element, SceSize elementLength, const char *name,
		SceSize nameLength, const char *value, int valueLength);
	virtual int Text(const char *element, SceSize elementLength, const char *text,
		int textLength);
	virtual int EndElement(const char *name, SceSize length);

private:
	AbstractXmlParserSubject *m_subject;
	NpActivityUpdates *m_updates;
	int m_reserved;			/* never set or read */
};

/* a reply with only its result */
class XmlParserResultHandler : public AbstractXmlParserHandler {
public:
	XmlParserResultHandler(AbstractXmlParserSubject *subject);
	virtual ~XmlParserResultHandler();

	virtual int StartElement(const char *name, SceSize length);
	virtual int Attribute(const char *element, SceSize elementLength, const char *name,
		SceSize nameLength, const char *value, int valueLength);
	virtual int Text(const char *element, SceSize elementLength, const char *text,
		int textLength);
	virtual int EndElement(const char *name, SceSize length);

private:
	AbstractXmlParserSubject *m_subject;
};

/* the trophy icon types of <trophy type="...">, in StaticResource's order of their URLs */
#define XML_PARSER_TROPHY_BRONZE	0
#define XML_PARSER_TROPHY_SILVER	1
#define XML_PARSER_TROPHY_GOLD		2
#define XML_PARSER_TROPHY_PLATINUM	3
#define XML_PARSER_TROPHY_HIDDEN	4
#define XML_PARSER_TROPHY_NONE		5

/* the static resource (activity.xml): its signature, status, host and trophy icons */
class XmlParserStaticResourceHandler : public AbstractXmlParserHandler {
public:
	XmlParserStaticResourceHandler(AbstractXmlParserSubject *subject, StaticResource *resource);
	virtual ~XmlParserStaticResourceHandler();

	virtual int StartElement(const char *name, SceSize length);
	virtual int Attribute(const char *element, SceSize elementLength, const char *name,
		SceSize nameLength, const char *value, int valueLength);
	virtual int Text(const char *element, SceSize elementLength, const char *text,
		int textLength);
	virtual int EndElement(const char *name, SceSize length);

private:
	AbstractXmlParserSubject *m_subject;
	StaticResource *m_resource;
	int m_trophyType;		/* XML_PARSER_TROPHY_*: the <trophy> being read */
};

/* <activity result="...">'s code for a status the network does not have
 * (NP_ACTIVITY_ERROR_STATUS_NOT_FOUND's low byte); the reply gives none, so the handler sets it */
#define XML_PARSER_RESULT_STATUS_NOT_FOUND	0x65

/* one status's detail: its info, comments and the users who gave it a good job */
class XmlParserStatusDetailHandler : public AbstractXmlParserHandler {
public:
	XmlParserStatusDetailHandler(AbstractXmlParserSubject *subject,
		NpActivityStatusInfo *info, NpActivityComment *comments, SceSize maxComments,
		int *numComments, int *isVoted, SceNpId *voters, SceSize maxVoters,
		SceUInt32 *numVoters, int reserved, int *numStatuses);
	virtual ~XmlParserStatusDetailHandler();

	virtual int StartElement(const char *name, SceSize length);
	virtual int Attribute(const char *element, SceSize elementLength, const char *name,
		SceSize nameLength, const char *value, int valueLength);
	virtual int Text(const char *element, SceSize elementLength, const char *text,
		int textLength);
	virtual int EndElement(const char *name, SceSize length);

private:
	AbstractXmlParserSubject *m_subject;
	NpActivityStatusInfo *m_info;
	NpActivityComment *m_comments;
	SceSize m_maxComments;
	int *m_numComments;
	int *m_isVoted;
	SceNpId *m_voters;
	SceSize m_maxVoters;
	SceUInt32 *m_numVoters;
	int m_reserved;			/* kept, never read */
	int *m_numStatuses;
	int m_hasStatusId;		/* 1 once the <status> named its id */
	int m_inComment;		/* in a <comment> */
	int m_inVoter;			/* in a <votedid> */
	int m_commentIndex;		/* -1: no room for more */
	int m_voterIndex;		/* -1: no room for more */
	NpActivityStatusKey m_key;	/* the status's owner and id */
};

/* a reply that names a status (a posted good job's) */
class XmlParserStatusIdHandler : public AbstractXmlParserHandler {
public:
	XmlParserStatusIdHandler(AbstractXmlParserSubject *subject, XmlParserStatusId *status);
	virtual ~XmlParserStatusIdHandler();

	virtual int StartElement(const char *name, SceSize length);
	virtual int Attribute(const char *element, SceSize elementLength, const char *name,
		SceSize nameLength, const char *value, int valueLength);
	virtual int Text(const char *element, SceSize elementLength, const char *text,
		int textLength);
	virtual int EndElement(const char *name, SceSize length);

private:
	AbstractXmlParserSubject *m_subject;
	XmlParserStatusId *m_status;
	int m_index;			/* -1, never read */
};

/* a reply that names a status and a comment (a posted comment's) */
class XmlParserStatusIdCommentHandler : public AbstractXmlParserHandler {
public:
	XmlParserStatusIdCommentHandler(AbstractXmlParserSubject *subject,
		XmlParserStatusIdComment *comment);
	virtual ~XmlParserStatusIdCommentHandler();

	virtual int StartElement(const char *name, SceSize length);
	virtual int Attribute(const char *element, SceSize elementLength, const char *name,
		SceSize nameLength, const char *value, int valueLength);
	virtual int Text(const char *element, SceSize elementLength, const char *text,
		int textLength);
	virtual int EndElement(const char *name, SceSize length);

private:
	AbstractXmlParserSubject *m_subject;
	XmlParserStatusIdComment *m_comment;
	int m_index;			/* -1, never read */
};

/* <status-list><status>: several statuses' details, into an array or one at a time */
class XmlParserStatusInfoHandler : public AbstractXmlParserHandler {
public:
	XmlParserStatusInfoHandler(AbstractXmlParserSubject *subject, int *count,
		NpActivityStatusDetail *details, SceSize max, int withCallback,
		NpActivityStatusDetail *detail);
	virtual ~XmlParserStatusInfoHandler();

	virtual int StartElement(const char *name, SceSize length);
	virtual int Attribute(const char *element, SceSize elementLength, const char *name,
		SceSize nameLength, const char *value, int valueLength);
	virtual int Text(const char *element, SceSize elementLength, const char *text,
		int textLength);
	virtual int EndElement(const char *name, SceSize length);

private:
	AbstractXmlParserSubject *m_subject;
	int *m_count;
	NpActivityStatusDetail *m_details;
	SceSize m_max;
	int m_withCallback;		/* 1: each status into m_detail, reported as it ends */
	NpActivityStatusDetail *m_detail;
	SceSize m_index;		/* the status being read, -1: no room */
	NpActivityStatusDetail *m_current;
};

/* <timeline-status-list><status><ownerid>: a timeline's statuses, a list per page */
class XmlParserStatusTimelineHandler : public AbstractXmlParserHandler {
public:
	XmlParserStatusTimelineHandler(AbstractXmlParserSubject *subject,
		NpActivityStatusList *lists, SceSize count);
	virtual ~XmlParserStatusTimelineHandler();

	virtual int StartElement(const char *name, SceSize length);
	virtual int Attribute(const char *element, SceSize elementLength, const char *name,
		SceSize nameLength, const char *value, int valueLength);
	virtual int Text(const char *element, SceSize elementLength, const char *text,
		int textLength);
	virtual int EndElement(const char *name, SceSize length);

private:
	AbstractXmlParserSubject *m_subject;
	NpActivityStatusList *m_lists;
	SceSize m_count;
	SceSize m_listIndex;		/* the list being read, -1: none */
	SceSize m_statusIndex;		/* its status being read, -1: none */
	NpActivityOwner m_owner;	/* <ownerid> */
	int m_full;			/* 1 once the reply has more lists than m_count */
};

#endif
