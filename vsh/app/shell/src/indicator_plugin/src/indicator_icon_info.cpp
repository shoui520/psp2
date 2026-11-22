/*
 * indicator_icon_info.cpp: the icons' information of titles read from the LiveSpace database as
 * a job on paf's default queue, the caller's callback told with them (0x811825A0-0x81182A28).
 * The file's and the names here are ours (PLACEHOLDER), but for the job's class, named by the
 * name it gives its job.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <std/map>
#include <common/shared_ptr.h>
#include <job/job.h>
#include "../../livespace_db/livespace_db_internal.h"
#include "../../livespace_db/lsdb_keys.h"
#include "indicator.h"

using namespace paf;

/* (0x811825A0) */
__attribute__((noinline))
JobGetIconInfo::JobGetIconInfo(vector<string> *titleIds, IndicatorIconInfoCB callback, void *arg)
	: job::JobItem("JobGetIconInfo", NULL), m_titleIds(*titleIds), m_callback(callback),
	  m_arg(arg)
{
}

/* (0x81182604) */
common::SharedPtr<job::JobItem> indicatorIconInfoJobStart(vector<string> *titleIds,
							  IndicatorIconInfoCB callback, void *arg)
{
	job::JobQueue *queue = job::JobQueue::default_queue;
	common::SharedPtr<job::JobItem> job(new JobGetIconInfo(titleIds, callback, arg));

	queue->Enqueue(job);
	return job;
}

/* (0x81182678) */
JobGetIconInfo::~JobGetIconInfo()
{
}

/* each title's icon read into the map, by its title; the last read's result (0x81182706) */
int JobGetIconInfo::Run()
{
	AppInfoQClient client;
	int result = 0;

	SceLsdb_A6D8772C(&client);
	SceLsdb_E90B81CD(&client);
	for (unsigned int i = 0; i < m_titleIds.size(); i++) {
		LsdbIconPtr icon(new LsdbIcon());

		result = SceLsdb_7AF9008C(&client, &m_titleIds[i], &icon, true);
		m_icons[m_titleIds[i]] = icon;
	}
	SceLsdb_DEF5C3F9(&client);
	SceLsdb_E2101B03(&client);
	return result;
}

/* the callback told once with the icons, which are then let go (0x8118292E) */
void JobGetIconInfo::Finish(int result)
{
	if (m_callback != NULL) {
		m_callback(&m_icons, result, m_arg);
		m_callback = NULL;
	}

	IndicatorIconInfoMap::iterator it = m_icons.begin();

	while (it != m_icons.end()) {
		IndicatorIconInfoMap::iterator where = it++;

		if (where.node != NULL)
			m_icons.erase(where->first);
	}
}
