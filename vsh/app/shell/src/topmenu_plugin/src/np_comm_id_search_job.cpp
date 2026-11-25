/*
 * np_comm_id_search_job.cpp: searches livespace_db by NP_COMM_ID and selects
 * the most recently launched matching title (0x81299048-0x8129984B).
 */
#include "topmenu_internal.h"

namespace {

static const SceUInt32 TOPMENU_LSDB_KEY_NP_COMM_ID = 0x10C1B6E0u;

/* 0x81299048..0x812997D9 */
class NpCommIdSearchJob : public paf::job::JobItem {
public:
	NpCommIdSearchJob(const char *npCommId, int launchAppId)
		: paf::job::JobItem("NpCommIdSearchJob", NULL),
		  m_npCommId(npCommId != NULL ? npCommId : ""),
		  m_launchAppId(launchAppId)
	{
	}

	virtual ~NpCommIdSearchJob() {}

	/* 0x812991CC */
	virtual int32_t Run()
	{
		paf::vector<paf::string> matchingTitles;
		LsdbQueryCondition condition;
		condition.key = TOPMENU_LSDB_KEY_NP_COMM_ID;
		condition.op = LSDB_QUERY_OP_EQUAL;
		condition.mask = 0;
		condition.next = NULL;

		LsdbValue queryValue(m_npCommId);
		condition.values.push_back(queryValue);

		int queryResult = SceLsdb_DD7D68C0(g_shellLsdbClient->client, &condition,
						   &matchingTitles, 0);
		if (queryResult != 0)
			return queryResult;

		paf::string latestLaunchTime;
		for (paf::vector<paf::string>::iterator it = matchingTitles.begin();
		     it != matchingTitles.end(); ++it) {
			LsdbAppInfoPtr appInfo(new LsdbAppInfo());
			if (SceLsdb_87EE0526(g_shellLsdbClient->client, &(*it), &appInfo) != 0)
				continue;

			LsdbAppInfo::iterator llIt = appInfo->find(LSDB_KEY_LLTIME);
			if (llIt == appInfo->end()) {
				if (latestLaunchTime.size() == 0) {
					m_foundAppInfo = appInfo;
					m_foundTitleId = *it;
				}
			} else {
				paf::string candidateTime(LsdbValueText(llIt->second));
				if (latestLaunchTime < candidateTime) {
					latestLaunchTime = candidateTime;
					m_foundAppInfo = appInfo;
					m_foundTitleId = *it;
				}
			}
		}
		return 0;
	}

	/* 0x8129979E */
	virtual void Finish(int32_t result)
	{
		if (result == 0 || m_foundAppInfo.get() != NULL) {
			topmenuOnNpCommIdFound(&m_foundTitleId, &m_foundAppInfo, m_launchAppId);
			return;
		}
		topmenuShowAppErrorDialog(result, 0, 0, 0);
		topmenuCancelPendingAppLaunch(m_launchAppId);
	}

private:
	paf::string m_npCommId;
	int m_launchAppId;
	LsdbAppInfoPtr m_foundAppInfo;
	paf::string m_foundTitleId;
};

} /* namespace */

/* 0x81299048 */
int topmenuStartNpCommIdSearch(paf::common::SharedPtr<paf::job::JobItem> *outJob,
			       const char *npCommId, int launchAppId)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new NpCommIdSearchJob(npCommId, launchAppId));
	*outJob = job;
	return g_topWindow->m_jobQueue->Enqueue(*outJob);
}
