/*
 * indicator_new_event_db.cpp: the indicator's view of the new-event database, the accessor's
 * calls, the title an event of kind 11 runs read from its link (0x8117F184-0x8117F6A4). The
 * file's and all names here are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <std/string.h>
#include <common/shared_ptr.h>
/* Sony built this file without the keys' header, so it has no static initializer */
#define LSDB_KEYS_H
#include "../../livespace_db/livespace_db_internal.h"
#include "indicator.h"

using namespace paf;

/* an event that opens a link, whose title is read from its execArg */
#define INDICATOR_NEW_EVENT_ACT_LINK	11

/* the longest title ID a psgm: link gives, its terminator included */
#define INDICATOR_NEW_EVENT_TITLE_ID_SIZE	10

/* (0x8117F184) */
IndicatorNewEventDb::IndicatorNewEventDb()
{
	SceLsdb_919C0934(&m_accessor);
}

/* (0x8117F190) */
IndicatorNewEventDb::~IndicatorNewEventDb()
{
	SceLsdb_937405B5(&m_accessor);
}

/* (0x8117F19C) */
int IndicatorNewEventDb::Open()
{
	return SceLsdb_4C137FCC(&m_accessor);
}

/* (0x8117F1A4) */
int IndicatorNewEventDb::Close()
{
	return SceLsdb_937405B5(&m_accessor);
}

/* (0x8117F1AE) */
int IndicatorNewEventDb::Delete(const LsdbNewEventKey *key)
{
	return SceLsdb_AE04DDEB(key, 1);
}

/* (0x8117F1B8) */
int IndicatorNewEventDb::DeleteAll(bool all)
{
	return SceLsdb_C02E38AD(all);
}

/* (0x8117F1C2) */
string indicatorNewEventExecTitleId(const string &execArg)
{
	if (sce_paf_strncmp(execArg.c_str(), "http:", sce_paf_strlen("http:")) == 0 ||
	    sce_paf_strncmp(execArg.c_str(), "https:", sce_paf_strlen("https:")) == 0)
		return "NPXS10003";
	if (sce_paf_strncmp(execArg.c_str(), "psns:", sce_paf_strlen("psns:")) == 0)
		return "NPXS10002";
	if (sce_paf_strncmp(execArg.c_str(), "psgm:", sce_paf_strlen("psgm:")) == 0) {
		const char *query = sce_paf_private_strtok(execArg.c_str(), "?");

		if (query != NULL) {
			const char *param = sce_paf_private_strtok(query, "titleid=");

			if (param != NULL) {
				const char *start = param + sce_paf_strlen("titleid=");
				const char *end = sce_paf_private_strtok(start, "&");

				if (end == NULL)
					end = start + sce_paf_strlen(start);

				size_t length = end - start;

				if (length + 1 <= INDICATOR_NEW_EVENT_TITLE_ID_SIZE) {
					char titleId[INDICATOR_NEW_EVENT_TITLE_ID_SIZE];

					sce_paf_strncpy(titleId, start, length);
					titleId[length] = '\0';
					return titleId;
				}
			}
		}
		return NULL;
	}
	return NULL;
}

/* (0x8117F36E) */
int IndicatorNewEventDb::GetEvents(vector<LsdbNewEventPtr> *events, int max, int *count,
				   int *newCount, bool clear)
{
	int result = SceLsdb_B285FC5E(&m_accessor, events, max, count, newCount, clear);

	for (vector<LsdbNewEventPtr>::iterator it = events->begin(); it != events->end(); ++it) {
		LsdbNewEvent *event = it->get();

		if (event->actType == INDICATOR_NEW_EVENT_ACT_LINK)
			event->execTitleId = indicatorNewEventExecTitleId(event->execArg);
	}
	return result;
}

/* (0x8117F40C) */
common::SharedPtr<DbBlob> IndicatorNewEventDb::GetIcon(const SceInt64 *rowid, int *result)
{
	return SceLsdb_03B27FDF(&m_accessor, rowid, result);
}

/* (0x8117F414) */
bool IndicatorNewEventDb::IsChanged()
{
	return SceLsdb_8DE37E2E(&m_accessor);
}

/* (0x8117F41C) */
int IndicatorNewEventDb::GetPopupCount()
{
	return SceLsdb_A2BD70BE(&m_accessor);
}

/* (0x8117F424) */
int IndicatorNewEventDb::GetPopup(LsdbNewEvent *event)
{
	int result = SceLsdb_D046218A(&m_accessor, event);

	if (event->actType == INDICATOR_NEW_EVENT_ACT_LINK)
		event->execTitleId = indicatorNewEventExecTitleId(event->execArg);
	return result;
}

/* (0x8117F47E) */
int IndicatorNewEventDb::ClearPopup(const SceInt64 *rowid)
{
	return SceLsdb_98672963(&m_accessor, rowid);
}

/* (0x8117F486) */
int IndicatorNewEventDb::ClearPopups()
{
	return SceLsdb_8A6B2B12(&m_accessor);
}

/* (0x8117F48E) */
int IndicatorNewEventDb::UpdateNewFlag(const LsdbNewEvent *event)
{
	LsdbNewEventPtr copy(new LsdbNewEvent(*event));

	return SceLsdb_464351D0(&m_accessor, &copy, 1);
}

/* (0x8117F692) */
int IndicatorNewEventDb::ClearNewFlags()
{
	return SceLsdb_FB3B1767(&m_accessor);
}

/* (0x8117F69A) */
unsigned int IndicatorNewEventDb::GetNotifyFlags(const LsdbNewEvent *event)
{
	return SceLsdb_121C66FC(&m_accessor, event);
}
