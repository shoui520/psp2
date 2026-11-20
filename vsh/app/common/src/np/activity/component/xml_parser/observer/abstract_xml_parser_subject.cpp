/*
 * The XML parser's subject: the observers the replies are reported to (Sony's file: its checks
 * name component/xml_parser/observer/abstract_xml_parser_subject.cpp)
 */
#include <stddef.h>
#include "../../../common/activity_debug.h"
#include "../../../np_activity.h"
#include "abstract_xml_parser_subject.h"

#define ACTIVITY_FILE	ACTIVITY_SRC_DIR "component\\xml_parser\\observer\\abstract_xml_parser_subject.cpp"

AbstractXmlParserSubject::AbstractXmlParserSubject()
{
	m_staticResourceObservers[0] = NULL;
	m_resultObservers[0] = NULL;
	m_statusListObservers[0] = NULL;
	m_accountInformationObservers[0] = NULL;
	m_checkStatusObservers[0] = NULL;
	m_statusDetailObservers[0] = NULL;
	m_statusIdObservers[0] = NULL;
	m_statusIdCommentObservers[0] = NULL;
	m_statusInfoObservers[0] = NULL;
	m_statusInfoListObservers[0] = NULL;
	m_statusTimelineObservers[0] = NULL;
}

AbstractXmlParserSubject::~AbstractXmlParserSubject()
{
}

void AbstractXmlParserSubject::NotifyStaticResource(StaticResource *resource)
{
	int i;

	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_staticResourceObservers[i] != NULL)
			m_staticResourceObservers[i]->UpdateStaticResource(resource);
	}
}

void AbstractXmlParserSubject::NotifyResult(XmlParserResult *result)
{
	int i;

	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_resultObservers[i] != NULL)
			m_resultObservers[i]->UpdateResult(result);
	}
}

void AbstractXmlParserSubject::NotifyStatusList(NpActivityPostResult *results)
{
	int i;

	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_statusListObservers[i] != NULL)
			m_statusListObservers[i]->UpdateStatusList(results);
	}
}

void AbstractXmlParserSubject::NotifyAccountInformation(int *setting)
{
	int i;

	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_accountInformationObservers[i] != NULL)
			m_accountInformationObservers[i]->UpdateAccountInformation(setting);
	}
}

void AbstractXmlParserSubject::NotifyStatusTimeline(NpActivityStatusList *lists)
{
	int i;

	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_statusTimelineObservers[i] != NULL)
			m_statusTimelineObservers[i]->UpdateStatusTimeline(lists);
	}
}

void AbstractXmlParserSubject::NotifyCheckStatus(NpActivityUpdates *updates)
{
	int i;

	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_checkStatusObservers[i] != NULL)
			m_checkStatusObservers[i]->UpdateCheckStatus(updates);
	}
}

void AbstractXmlParserSubject::NotifyStatusDetail(NpActivityStatusKey *key, NpActivityStatusInfo *info,
	NpActivityComment *comments, int *numComments, int *isVoted, SceNpId *voters,
	SceUInt32 *numVoters)
{
	int i;

	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_statusDetailObservers[i] != NULL)
			m_statusDetailObservers[i]->UpdateStatusDetail(key, info, comments,
				numComments, isVoted, voters, numVoters);
	}
}

void AbstractXmlParserSubject::NotifyStatusId(XmlParserStatusId *status)
{
	int i;

	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_statusIdObservers[i] != NULL)
			m_statusIdObservers[i]->UpdateStatusId(status);
	}
}

void AbstractXmlParserSubject::NotifyStatusIdComment(XmlParserStatusIdComment *comment)
{
	int i;

	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_statusIdCommentObservers[i] != NULL)
			m_statusIdCommentObservers[i]->UpdateStatusIdComment(comment);
	}
}

void AbstractXmlParserSubject::NotifyStatusInfoList(NpActivityStatusDetail *details, int *count)
{
	int i;

	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_statusInfoListObservers[i] != NULL)
			m_statusInfoListObservers[i]->UpdateStatusInfoList(details, count);
	}
}

void AbstractXmlParserSubject::NotifyStatusInfo(NpActivityStatusDetail *detail)
{
	int i;

	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_statusInfoObservers[i] != NULL)
			m_statusInfoObservers[i]->UpdateStatusInfo(detail);
	}
}

int AbstractXmlParserSubject::AddStaticResourceObserver(StaticResourceObserver *observer)
{
	int i;

	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_staticResourceObservers[i] == NULL) {
			m_staticResourceObservers[i] = observer;
			return 0;
		}
	}
#line 288
	ACTIVITY_CHECK("AddStaticResourceObserver");
	return NP_ACTIVITY_ERROR_INVALID_STATE;
}

int AbstractXmlParserSubject::RemoveStaticResourceObserver(StaticResourceObserver *observer)
{
	int found;
	int i;

	found = 0;
	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_staticResourceObservers[i] == observer) {
			found = 1;
			m_staticResourceObservers[i] = NULL;
		}
	}
	if (found)
		return 0;
#line 313
	ACTIVITY_CHECK("RemoveStaticResourceObserver");
	return NP_ACTIVITY_ERROR_INVALID_STATE;
}

int AbstractXmlParserSubject::AddResultObserver(ResultObserver *observer)
{
	int i;

	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_resultObservers[i] == NULL) {
			m_resultObservers[i] = observer;
			return 0;
		}
	}
#line 333
	ACTIVITY_CHECK("AddResultObserver");
	return NP_ACTIVITY_ERROR_INVALID_STATE;
}

int AbstractXmlParserSubject::RemoveResultObserver(ResultObserver *observer)
{
	int found;
	int i;

	found = 0;
	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_resultObservers[i] == observer) {
			found = 1;
			m_resultObservers[i] = NULL;
		}
	}
	if (found)
		return 0;
#line 358
	ACTIVITY_CHECK("RemoveResultObserver");
	return NP_ACTIVITY_ERROR_INVALID_STATE;
}

int AbstractXmlParserSubject::AddStatusListObserver(StatusListObserver *observer)
{
	int i;

	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_statusListObservers[i] == NULL) {
			m_statusListObservers[i] = observer;
			return 0;
		}
	}
#line 378
	ACTIVITY_CHECK("AddStatusListObserver");
	return NP_ACTIVITY_ERROR_INVALID_STATE;
}

int AbstractXmlParserSubject::RemoveStatusListObserver(StatusListObserver *observer)
{
	int found;
	int i;

	found = 0;
	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_statusListObservers[i] == observer) {
			found = 1;
			m_statusListObservers[i] = NULL;
		}
	}
	if (found)
		return 0;
#line 403
	ACTIVITY_CHECK("RemoveStatusListObserver");
	return NP_ACTIVITY_ERROR_INVALID_STATE;
}

int AbstractXmlParserSubject::AddAccountInformationObserver(AccountInformationObserver *observer)
{
	int i;

	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_accountInformationObservers[i] == NULL) {
			m_accountInformationObservers[i] = observer;
			return 0;
		}
	}
#line 423
	ACTIVITY_CHECK("AddAccountInformationObserver");
	return NP_ACTIVITY_ERROR_INVALID_STATE;
}

int AbstractXmlParserSubject::RemoveAccountInformationObserver(AccountInformationObserver *observer)
{
	int found;
	int i;

	found = 0;
	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_accountInformationObservers[i] == observer) {
			found = 1;
			m_accountInformationObservers[i] = NULL;
		}
	}
	if (found)
		return 0;
#line 448
	ACTIVITY_CHECK("RemoveAccountInformationObserver");
	return NP_ACTIVITY_ERROR_INVALID_STATE;
}

int AbstractXmlParserSubject::AddCheckStatusObserver(CheckStatusObserver *observer)
{
	int i;

	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_checkStatusObservers[i] == NULL) {
			m_checkStatusObservers[i] = observer;
			return 0;
		}
	}
#line 468
	ACTIVITY_CHECK("AddCheckStatusObserver");
	return NP_ACTIVITY_ERROR_INVALID_STATE;
}

int AbstractXmlParserSubject::RemoveCheckStatusObserver(CheckStatusObserver *observer)
{
	int found;
	int i;

	found = 0;
	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_checkStatusObservers[i] == observer) {
			found = 1;
			m_checkStatusObservers[i] = NULL;
		}
	}
	if (found)
		return 0;
#line 493
	ACTIVITY_CHECK("RemoveCheckStatusObserver");
	return NP_ACTIVITY_ERROR_INVALID_STATE;
}

int AbstractXmlParserSubject::AddStatusDetailObserver(StatusDetailObserver *observer)
{
	int i;

	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_statusDetailObservers[i] == NULL) {
			m_statusDetailObservers[i] = observer;
			return 0;
		}
	}
#line 513
	ACTIVITY_CHECK("AddStatusDetailObserver");
	return NP_ACTIVITY_ERROR_INVALID_STATE;
}

int AbstractXmlParserSubject::RemoveStatusDetailObserver(StatusDetailObserver *observer)
{
	int found;
	int i;

	found = 0;
	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_statusDetailObservers[i] == observer) {
			found = 1;
			m_statusDetailObservers[i] = NULL;
		}
	}
	if (found)
		return 0;
#line 538
	ACTIVITY_CHECK("RemoveStatusDetailObserver");
	return NP_ACTIVITY_ERROR_INVALID_STATE;
}

int AbstractXmlParserSubject::AddStatusIdObserver(StatusIdObserver *observer)
{
	int i;

	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_statusIdObservers[i] == NULL) {
			m_statusIdObservers[i] = observer;
			return 0;
		}
	}
#line 558
	ACTIVITY_CHECK("AddStatusIdObserver");
	return NP_ACTIVITY_ERROR_INVALID_STATE;
}

int AbstractXmlParserSubject::RemoveStatusIdObserver(StatusIdObserver *observer)
{
	int found;
	int i;

	found = 0;
	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_statusIdObservers[i] == observer) {
			found = 1;
			m_statusIdObservers[i] = NULL;
		}
	}
	if (found)
		return 0;
#line 583
	ACTIVITY_CHECK("RemoveStatusIdObserver");
	return NP_ACTIVITY_ERROR_INVALID_STATE;
}

int AbstractXmlParserSubject::AddStatusIdCommentObserver(StatusIdCommentObserver *observer)
{
	int i;

	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_statusIdCommentObservers[i] == NULL) {
			m_statusIdCommentObservers[i] = observer;
			return 0;
		}
	}
#line 603
	ACTIVITY_CHECK("AddStatusIdCommentObserver");
	return NP_ACTIVITY_ERROR_INVALID_STATE;
}

int AbstractXmlParserSubject::RemoveStatusIdCommentObserver(StatusIdCommentObserver *observer)
{
	int found;
	int i;

	found = 0;
	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_statusIdCommentObservers[i] == observer) {
			found = 1;
			m_statusIdCommentObservers[i] = NULL;
		}
	}
	if (found)
		return 0;
#line 628
	ACTIVITY_CHECK("RemoveStatusIdCommentObserver");
	return NP_ACTIVITY_ERROR_INVALID_STATE;
}

int AbstractXmlParserSubject::AddStatusInfoObserver(StatusInfoObserver *observer)
{
	int i;

	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_statusInfoObservers[i] == NULL) {
			m_statusInfoObservers[i] = observer;
			return 0;
		}
	}
#line 648
	ACTIVITY_CHECK("AddStatusInfoObserver");
	return NP_ACTIVITY_ERROR_INVALID_STATE;
}

int AbstractXmlParserSubject::RemoveStatusInfoObserver(StatusInfoObserver *observer)
{
	int found;
	int i;

	found = 0;
	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_statusInfoObservers[i] == observer) {
			found = 1;
			m_statusInfoObservers[i] = NULL;
		}
	}
	if (found)
		return 0;
#line 673
	ACTIVITY_CHECK("RemoveStatusInfoObserver");
	return NP_ACTIVITY_ERROR_INVALID_STATE;
}

int AbstractXmlParserSubject::AddStatusInfoListObserver(StatusInfoListObserver *observer)
{
	int i;

	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_statusInfoListObservers[i] == NULL) {
			m_statusInfoListObservers[i] = observer;
			return 0;
		}
	}
#line 693
	ACTIVITY_CHECK("AddStatusInfoListObserver");
	return NP_ACTIVITY_ERROR_INVALID_STATE;
}

int AbstractXmlParserSubject::RemoveStatusInfoListObserver(StatusInfoListObserver *observer)
{
	int found;
	int i;

	found = 0;
	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_statusInfoListObservers[i] == observer) {
			found = 1;
			m_statusInfoListObservers[i] = NULL;
		}
	}
	if (found)
		return 0;
#line 718
	ACTIVITY_CHECK("RemoveStatusInfoListObserver");
	return NP_ACTIVITY_ERROR_INVALID_STATE;
}

int AbstractXmlParserSubject::AddStatusTimelineObserver(StatusTimelineObserver *observer)
{
	int i;

	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_statusTimelineObservers[i] == NULL) {
			m_statusTimelineObservers[i] = observer;
			return 0;
		}
	}
#line 738
	ACTIVITY_CHECK("AddStatusTimelineObserver");
	return NP_ACTIVITY_ERROR_INVALID_STATE;
}

int AbstractXmlParserSubject::RemoveStatusTimelineObserver(StatusTimelineObserver *observer)
{
	int found;
	int i;

	found = 0;
	for (i = 0; i < XML_PARSER_SUBJECT_OBSERVER_MAX; i++) {
		if (m_statusTimelineObservers[i] == observer) {
			found = 1;
			m_statusTimelineObservers[i] = NULL;
		}
	}
	if (found)
		return 0;
#line 763
	ACTIVITY_CHECK("RemoveStatusTimelineObserver");
	return NP_ACTIVITY_ERROR_INVALID_STATE;
}
