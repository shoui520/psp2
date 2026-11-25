/*
 * app_info_search_job.cpp: AppInfoSearchJob (0x81286BF4-0x81286FBF).
 * Searches livespace_db for the first installed title among a target title ID and
 * its comma-separated alternate title IDs, then reports the match to TopWindow.
 */
#include "topmenu_internal.h"

namespace {

const SceInt32 TOPMENU_ERROR_APP_NOT_FOUND = -2146420468; /* 0x8010390C */

class AppInfoSearchJob : public paf::job::JobItem {
public:
	AppInfoSearchJob(const char *titleId, const char *altTitleIds, SceInt32 launchMode);
	virtual ~AppInfoSearchJob();
	virtual int32_t Run();
	virtual void Finish(int32_t result);

	SceInt32 m_launchMode;				/* +0x1C */
	LsdbAppInfoPtr m_appInfo;			/* +0x20 */
	paf::list<paf::string> m_candidateTitles;	/* +0x28 */
	SceInt32 m_matchedIndex;			/* +0x34 */
	paf::string m_matchedTitleId;			/* +0x38 */
};

typedef char AppInfoSearchJobSizeCheck[sizeof(AppInfoSearchJob) == 68 ? 1 : -1];

/* (0x81286BF4) */
AppInfoSearchJob::AppInfoSearchJob(const char *titleId, const char *altTitleIds,
				   SceInt32 launchMode)
	: paf::job::JobItem("AppInfoSearchJob", NULL),
	  m_launchMode(launchMode),
	  m_appInfo(new LsdbAppInfo()),
	  m_candidateTitles(),
	  m_matchedIndex(-1),
	  m_matchedTitleId()
{
	paf::string mainTitle(titleId != NULL ? titleId : "");
	m_candidateTitles.push_back(mainTitle);

	paf::string altList(altTitleIds != NULL ? altTitleIds : "");
	paf::common::string_util::tokenize(m_candidateTitles, altList, 0, ',');
}

/* (0x81286E02 / 0x81286EC4) */
AppInfoSearchJob::~AppInfoSearchJob()
{
}

/* (0x81286ED4) */
int32_t AppInfoSearchJob::Run()
{
	SceInt32 index = 0;

	for (paf::list<paf::string>::iterator it = m_candidateTitles.begin();
	     it != m_candidateTitles.end(); ++it) {
		if (SceLsdb_87EE0526(g_shellLsdbClient->client, &(*it), &m_appInfo) == 0) {
			m_matchedIndex = index;
			m_matchedTitleId = *it;
			return 0;
		}
		++index;
	}
	return TOPMENU_ERROR_APP_NOT_FOUND;
}

/* (0x81286F3C) */
void AppInfoSearchJob::Finish(int32_t result)
{
	(void)result;
	topmenuOnAppInfoSearchDone(&m_matchedTitleId, &m_appInfo, m_launchMode);
}

} /* namespace */

/* (0x81286D74) */
int topmenuStartAppInfoSearch(paf::common::SharedPtr<paf::job::JobItem> *outJob,
			      const char *titleId, const char *altTitleIds,
			      SceInt32 launchMode)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new AppInfoSearchJob(titleId, altTitleIds, launchMode));
	*outJob = job;
	return g_topWindow->m_jobQueue->Enqueue(job);
}
