/*
 * livearea_status_job.cpp: livearea_plugin GetStatusInfoJob (activity status viewer launch;
 * 0x8119FB10-0x811A078C, sti 0x811A071C).
 */
#include <std/stdlib.h>
#include <std/string.h>
#include "livearea_internal.h"

extern "C" {
int sceAppMgrLaunchAppByName(unsigned int flags, const char *name, const char *param);
long long sce_paf_strtoll(const char *nptr, char **endptr, int base);
void SceActivityDb_6C98B745(void *ctx);
int SceActivityDb_F65C0C21(void *ctx, const ActivityDbQueryCondition *condition);
int SceActivityDb_74F96707(void *ctx, const paf::vector<SceInt64> *uids,
			   paf::vector<ActivityDbStatus> *outItems);
int SceActivityDb_7ED9033C(void *ctx, SceUInt64 *outStartTime, SceUInt64 *outLastReadTime,
			   SceUInt64 *outLastUpdateTime, SceUInt64 *outLatestTime);
void SceActivityDb_6E198FFA(void *ctx);
}

static bool s_statusJobActive;					/* 0x81567130 */
static int s_statusDlgId = -1;					/* 0x81544D90 */

class GetStatusInfoJob : public paf::job::JobItem {
public:
	GetStatusInfoJob(const char *titleId, const char *statusParam,
			 paf::ui::Widget *waitWidget);		/* 0x8119FB10 */
	virtual ~GetStatusInfoJob();				/* 0x8119FC54 / 0x8119FCB8 */
	virtual int Run();					/* 0x8119FD20 */
	virtual void Finish(int result);			/* 0x8119FF8E */

	static paf::common::SharedPtr<paf::job::JobItem> Enqueue(const char *titleId,
								 const char *statusParam,
								 paf::ui::Widget *waitWidget); /* 0x8119FBB8 */

	paf::string m_titleId;					/* +0x1C */
	paf::string m_statusParam;				/* +0x28 */
	paf::ui::Widget *m_waitWidget;				/* +0x34 */
	paf::vector<ActivityDbStatus> m_items;			/* +0x38 */
	SceUInt64 m_lastReadTime;				/* +0x48 */
	SceInt64 m_statusUid;					/* +0x50 */
};

/* 0x8119FB10 */
GetStatusInfoJob::GetStatusInfoJob(const char *titleId, const char *statusParam,
				   paf::ui::Widget *waitWidget)
	: paf::job::JobItem("GetStatusInfoJob", NULL),
	  m_titleId(titleId),
	  m_statusParam(statusParam),
	  m_waitWidget(waitWidget),
	  m_items(),
	  m_lastReadTime(0),
	  m_statusUid(-1)
{
	s_statusJobActive = true;
}

/* 0x8119FBB8 */
paf::common::SharedPtr<paf::job::JobItem> GetStatusInfoJob::Enqueue(const char *titleId,
								    const char *statusParam,
								    paf::ui::Widget *waitWidget)
{
	paf::job::JobItem *job = new GetStatusInfoJob(titleId, statusParam, waitWidget);
	paf::common::SharedPtr<paf::job::JobItem> jobRef(job);
	paf::job::JobQueue *queue = g_liveareaMgr->m_topmenuIf->getJobQueue();
	queue->Enqueue(jobRef);
	return jobRef;
}

/* 0x8119FC54 / 0x8119FCB8 */
GetStatusInfoJob::~GetStatusInfoJob()
{
	s_statusJobActive = false;
}

/* 0x8119FD20 */
int GetStatusInfoJob::Run()
{
	unsigned char activityHandle[4];
	SceActivityDb_6C98B745(activityHandle);

	ActivityDbQueryCondition condition;
	condition.type = ACTIVITY_DB_QUERY_TITLE;
	condition.titleId = m_titleId;
	condition.limit = 0;

	int ret = SceActivityDb_F65C0C21(activityHandle, &condition);
	if (ret != 0) {
		SceActivityDb_6E198FFA(activityHandle);
		return ret;
	}

	paf::vector<SceInt64> uids;
	paf::string paramStr(m_statusParam);
	size_t prefixLen = sce_paf_strlen("status_uid=");
	if (paramStr.find("status_uid=") != paf::string::npos)
		paramStr.erase(0, prefixLen);

	char *endPtr = NULL;
	SceInt64 uid = sce_paf_strtoll(paramStr.c_str(), &endPtr, 10);
	if (endPtr != NULL && *endPtr == '\0') {
		m_statusUid = uid;
		uids.push_back(uid);
	}

	ret = SceActivityDb_74F96707(activityHandle, &uids, &m_items);
	if (ret != 0) {
		SceActivityDb_6E198FFA(activityHandle);
		return ret;
	}

	SceUInt64 startTime = 0;
	SceUInt64 lastUpdateTime = 0;
	SceUInt64 latestTime = 0;
	ret = SceActivityDb_7ED9033C(activityHandle, &startTime, &m_lastReadTime,
				     &lastUpdateTime, &latestTime);
	SceActivityDb_6E198FFA(activityHandle);
	return ret;
}

/* 0x8119FF8E */
void GetStatusInfoJob::Finish(int result)
{
	paf::ui::Widget *waitWidget = m_waitWidget;
	paf::ui::Widget *parent = waitWidget->m_parent;
	waitWidget->DestroyWidget();

	if (result != 0) {
		CommonGuiMessageDialogParam dlg;
		dlg.type = result;
		if (result == (int)0x80100F05)
			dlg.messageId = 482989490U;
		g_liveareaMgr->m_topmenuIf->openErrorDialog(&dlg);
		return;
	}

	if (m_items.size() == 1) {
		sceAppMgrLaunchAppByName(0, m_titleId.c_str(), NULL);
		paf::common::SharedPtr<SceUInt64> lastReadRef(new SceUInt64(m_lastReadTime));
		paf::ui::Widget *overlay = new paf::ui::Widget(parent, NULL);
		paf::math::v4 pos(0.0f, -22.0f, 0.0f, 0.0f);
		paf::math::v4 size(960.0f, 512.0f, 0.0f, 0.0f);
		overlay->SetPos(pos, NULL, 0x10000, 0, 0.0f, NULL, NULL);
		overlay->SetSize(size, NULL, 65540, 0, 0.0f, NULL, NULL);

		ActivityDbQueryCondition condition;
		condition.type = ACTIVITY_DB_QUERY_TITLE_ONLY;
		condition.titleId = m_titleId;
		condition.limit = 0;

		paf::common::SharedPtr<int> ownerContext;
		paf::common::SharedPtr<int> feedContext;
		activityUiCreateStatusContexts(&feedContext, &ownerContext, &condition);

		ActivityRecord *record = NULL;
		paf::cache::CacheImageInterface *nullIcon = NULL;
		LiveAreaStatusDlg *dlg = new LiveAreaStatusDlg(NULL, overlay, &record, 3,
							       &feedContext, &lastReadRef, &nullIcon);
		if (dlg != NULL) {
			activityUiStatusDlgOpen(dlg, NULL);
			dlg->SetupIcon();
			s_statusDlgId = dlg->m_dialogId;
			dlg->BindContext(&ownerContext, m_titleId.c_str());
			return;
		}
		overlay->DestroyWidget();
	}
}

/* 0x811A064C */
void liveareaLaunchWithArg(const char *titleId, const char *statusParam)
{
	if (s_statusJobActive)
		return;
	if (s_statusDlgId >= 0 && activityUiIsDialogActive() != 0) {
		activityUiCloseDialogFast();
		s_statusDlgId = -1;
	}
	paf::ui::Widget *waitWidget = g_liveareaMgr->m_topmenuIf->createPlane(0);
	if (waitWidget != NULL) {
		paf::common::SharedPtr<paf::job::JobItem> jobRef =
			GetStatusInfoJob::Enqueue(titleId, statusParam, waitWidget);
	}
}
