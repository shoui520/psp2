/*
 * The XML parser component: reads the replies, 72 bytes. A Begin method makes the handler of
 * one reply and the parser of its schema, Parse feeds it the reply's pieces and EndParse
 * frees both; the handler reports the reply through the component's subject. No binary names
 * this file, the classes or their methods; the names are ours.
 */
#ifndef NP_ACTIVITY_COMPONENT_XML_PARSER_COMPONENT_XML_PARSER_H
#define NP_ACTIVITY_COMPONENT_XML_PARSER_COMPONENT_XML_PARSER_H

#include <np/np_common.h>
#include <np_common/np_common.h>
#include "../abstract_component.h"
#include "../../command/static_resource.h"
#include "../../np_activity.h"
#include "observer/abstract_xml_parser_subject.h"

class AbstractXmlParserHandler;

/* the parser's interface: a reply kind per Begin method, none read by the base */
class AbstractXmlParser : public Component, public AbstractXmlParserSubject {
public:
	AbstractXmlParser()
	{
	}
	virtual ~AbstractXmlParser()
	{
	}

	virtual int BeginStaticResource(StaticResource *resource)
	{
		return 0;
	}
	virtual int BeginResult()
	{
		return 0;
	}
	virtual int BeginAddStatusList(NpActivityPostResult *results, SceSize count)
	{
		return 0;
	}
	virtual int BeginAccountInformation(int *setting)
	{
		return 0;
	}
	virtual int BeginAccountStatusList(NpActivityStatusList *lists, SceSize count)
	{
		return 0;
	}
	virtual int BeginCheckStatus(NpActivityUpdates *updates)
	{
		return 0;
	}
	virtual int BeginStatusDetail(NpActivityStatusInfo *info, NpActivityComment *comments,
		SceSize maxComments, int *numComments, int *isVoted, SceNpId *voters, SceSize maxVoters,
		SceUInt32 *numVoters, int reserved, int *numStatuses)
	{
		return 0;
	}
	virtual int BeginStatusId(XmlParserStatusId *status)
	{
		return 0;
	}
	virtual int BeginStatusIdComment(XmlParserStatusIdComment *comment)
	{
		return 0;
	}
	virtual int BeginStatusInfo(int *count, NpActivityStatusDetail *details, SceSize max,
		int withCallback, NpActivityStatusDetail *detail)
	{
		return 0;
	}
	virtual int BeginStatusTimeline(NpActivityStatusList *lists, SceSize count)
	{
		return 0;
	}
	virtual int Parse(const char *data, int size, int *done)
	{
		return 0;
	}
	virtual int EndParse()
	{
		return 0;
	}
};

class ComponentXmlParser : public AbstractXmlParser {
public:
	/* the parser's report of an element, attribute or text, handed to the handler */
	int ReportEvent(NpXmlParser *parser, int event, SceUInt32 depth, SceUInt32 index,
		const char *name, const char *value);
	/* the parser's callback: arg is the component */
	static int Callback(NpXmlParser *parser, int event, SceUInt32 depth, SceUInt32 index,
		const char *name, const char *value, void *arg);

	ComponentXmlParser();
	virtual ~ComponentXmlParser();

	virtual int BeginStaticResource(StaticResource *resource);
	virtual int BeginResult();
	virtual int BeginAddStatusList(NpActivityPostResult *results, SceSize count);
	virtual int BeginAccountInformation(int *setting);
	virtual int BeginAccountStatusList(NpActivityStatusList *lists, SceSize count);
	virtual int BeginCheckStatus(NpActivityUpdates *updates);
	virtual int BeginStatusDetail(NpActivityStatusInfo *info, NpActivityComment *comments,
		SceSize maxComments, int *numComments, int *isVoted, SceNpId *voters, SceSize maxVoters,
		SceUInt32 *numVoters, int reserved, int *numStatuses);
	virtual int BeginStatusId(XmlParserStatusId *status);
	virtual int BeginStatusIdComment(XmlParserStatusIdComment *comment);
	virtual int BeginStatusInfo(int *count, NpActivityStatusDetail *details, SceSize max,
		int withCallback, NpActivityStatusDetail *detail);
	virtual int BeginStatusTimeline(NpActivityStatusList *lists, SceSize count);
	virtual int Parse(const char *data, int size, int *done);
	virtual int EndParse();
	virtual void SetAbort();
	virtual void ClearAbort();

private:
	NpXmlParser *m_parser;			/* while a reply is read */
	AbstractXmlParserHandler *m_handler;	/* while a reply is read */
};

#endif
