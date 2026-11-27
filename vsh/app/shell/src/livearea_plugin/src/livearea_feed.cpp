/*
 * livearea_feed.cpp: livearea_plugin activity feed & news icon bar
 * (0x8119C7A4-0x8119FB10, sti 0x8119FA72).
 */
#include <std/stdio.h>
#include <std/string.h>
#include <rtc/rtc.h>
#include <datetime/datetime.h>
#include <timer/timer.h>
#include <appmgr.h>
#include "../../../sysupchk.h"
#include "livearea_internal.h"

extern "C" {
int sceAppMgrLaunchAppByName(unsigned int flags, const char *name, const char *param);
int sce_paf_rand(void);
}

static paf::IDParam s_destroyId("destroy");			/* 0x81567100 */

static const float s_newsStartX[6] = {
	0.0f, -46.0f, -92.0f, -138.0f, -184.0f, -230.0f
};

class GetActivityNewsJob : public paf::common::SlotJobItem {
public:
	GetActivityNewsJob(const char *titleId, SceUInt64 lastRead, bool isFriendsFeed)
		: paf::common::SlotJobItem("GetActivityNewsJob", NULL),
		  m_titleId(titleId),
		  m_lastRead(lastRead),
		  m_isFriendsFeed(isFriendsFeed ? 1 : 0),
		  m_items(),
		  m_hasMore(false)
	{}

	virtual ~GetActivityNewsJob() {}			/* 0x8119F88E / 0x8119F8D8 */
	virtual int Run()					/* 0x8119F928 */
	{
		return SceActivityDb_6268A990(&m_items, &m_hasMore, &m_titleId, &m_lastRead,
					      5, m_isFriendsFeed);
	}

	paf::string m_titleId;					/* +0x20 */
	SceUInt64 m_lastRead;					/* +0x30 */
	int m_isFriendsFeed;					/* +0x38 */
	paf::vector<ActivityDbStatus> m_items;			/* +0x3C */
	bool m_hasMore;						/* +0x4C */
};

class SetLastReadTimeJob : public paf::common::SlotJobItem {
public:
	SetLastReadTimeJob(const char *titleId, SceUInt64 lastRead, bool isFriendsFeed)
		: paf::common::SlotJobItem("SetLastReadTimeJob", NULL),
		  m_titleId(titleId),
		  m_isFriendsFeed(isFriendsFeed ? 1 : 0),
		  m_lastRead(lastRead)
	{}

	virtual ~SetLastReadTimeJob() {}			/* 0x8119F94E / 0x8119F990 */
	virtual int Run()					/* 0x8119F9D8 */
	{
		SceInt64 baseTime = (SceInt64)m_lastRead;
		return SceActivityDb_C37F81FE(&m_titleId, &baseTime, m_isFriendsFeed);
	}

	paf::string m_titleId;					/* +0x20 */
	int m_isFriendsFeed;					/* +0x2C */
	SceUInt64 m_lastRead;					/* +0x30 */
};

static void liveareaFeedOnItemCountCb(int count, int pageKey);	/* 0x8119F4C0 */
static void liveareaFeedOnRefreshBtnCb(int eventId, paf::ui::Handler *handler,
				       paf::ui::Event *event, void *userdata); /* 0x8119F4DC */
static void liveareaFeedOnUpdateManualCb(int err, int subErr, int pageKey); /* 0x8119F4E8 */
static void liveareaFeedOnUpdateSilentCb(int err, int subErr, int pageKey); /* 0x8119F540 */
static void liveareaFeedOnUpdateAutoCb(int err, int subErr, int pageKey); /* 0x8119F590 */
static void liveareaFeedOnSysupchkDone(int eventId, int status, int eventArg, int userdata); /* 0x8119F5EA */
static void liveareaFeedOnMoreClickCb(void *userdata);		/* 0x8119F5FE */
static void liveareaFeedOnMoreResultCb(int err, int subErr, int pageKey); /* 0x8119F606 */
static void liveareaFeedOnJobRemovedCb(uint32_t slot, paf::common::SharedPtr<paf::job::JobItem> item,
				       void *userdata);		/* 0x8119F64E */
static void liveareaFeedOnArrowTimerCb(int eventId, paf::ui::Handler *handler); /* 0x8119F67C */
static void liveareaFeedOnNewsClickCb(int eventId, paf::ui::Handler *handler,
				      paf::ui::Event *event, void *userdata); /* 0x8119F806 */
static void liveareaFeedOnSignInBtnCb(void);			/* 0x8119E9D4 */

/* 0x8119C7A4 */
int LiveAreaFeed::StartSpinAnim()
{
	if (m_updatingCount != 0)
		return 0;
	m_updatingCount = 1;
	paf::ui::Widget *header = m_page->m_fileList->GetSegmentHeader(1);
	if (header != NULL) {
		paf::IDParam refreshBtnId(724248317U);
		paf::ui::Widget *btn = header->FindChild(refreshBtnId, 0);
		if (btn != NULL) {
			paf::math::v4 rot(0.0f, 0.0f, -6.2831855f, 0.0f);
			paf::Timer *spinTimer = new paf::Timer();
			spinTimer->InitTimer(22.0f, 0, -1, 0, NULL);
			btn->SetRot(rot, 0, spinTimer, 65542, 0, 0.0f, NULL, NULL);

			paf::Timer *loopTimer = new paf::Timer();
			loopTimer->InitTimer(0.0f, 0, -1, 0, NULL);
			btn->SetRot(rot, 0, loopTimer, 268500998, 0, 0.0f, NULL, NULL);
		}
	}
	return 1;
}

/* 0x8119C964 */
int LiveAreaFeed::RequestUpdate(int mode)
{
	LiveAreaUpdater *updater = &g_liveareaMgr->m_updater;
	const char *title = liveareaPageGetTitleIdRaw(m_page);
	int pageKey = m_page->m_pageKey;
	SceUInt64 taskId = 0;

	if (mode == 0) {
		if (liveareaUpdaterRequestFeedUpdate(updater, &taskId, title, 0,
						     (void *)liveareaFeedOnUpdateSilentCb,
						     pageKey) == 0)
			return 0;
	} else if (mode == 1) {
		if (liveareaUpdaterRequestFeedUpdate(updater, &taskId, title, 1,
						     (void *)liveareaFeedOnUpdateAutoCb,
						     pageKey) == 0)
			return 0;
	} else if (mode == 2) {
		if (m_updatingCount != 0)
			return -1;
		if (g_liveareaMgr->m_netState == 2) {
			sceAppMgrLaunchAppByName(0x20000, "NPXS10016", "signin");
			return -1;
		}
		netCtlIntRequestConnect();
		if (liveareaUpdaterRequestFeedUpdate(updater, &taskId, title, 1,
						     (void *)liveareaFeedOnUpdateManualCb,
						     pageKey) == 0) {
			StartSpinAnim();
			return 0;
		}
	}
	return -1;
}

/* 0x8119CA86 */
void LiveAreaFeed::RebuildFeed()
{
	paf::ui::FileList *fileList = m_page->m_fileList;
	paf::ui::filelist::SegmentSource *segSrc = fileList->m_segment_source.get();
	if (segSrc != NULL)
		segSrc->AddRef();

	const char *title = liveareaPageGetTitleIdRaw(m_page);
	bool isSpecial = (sce_paf_strcmp(title, "NPXS10999") == 0 ||
			  sce_paf_strcmp(title, "NPXS10998") == 0);
	int netState = g_liveareaMgr->m_netState;
	int signedIn = g_liveareaMgr->m_signedInState;

	if (netState == 0 && !isSpecial) {
		if (m_state != 1) {
			m_state = 1;
			m_moreCtrl.Disable();
			ActivityFeedCellSource *oldCell = NULL;
			activityUiSegmentGetCell(&oldCell, segSrc, 1);
			if (oldCell != NULL) {
				oldCell->Release();
				activityUiCellSourceSetCountCallback(oldCell, NULL, 0);
			}
			activityUiSegmentRemoveCell(segSrc, 1);
			paf::IDParam tmplId(748860498U);
			paf::Plugin::TemplateOpenParam openParam;
			g_liveareaPlugin->TemplateOpen(fileList, tmplId, openParam);
			fileList->BlockSync();
			fileList->UnblockSync();
		}
	} else if (signedIn != 0) {
		if (m_state != 3) {
			m_state = 3;
			m_moreCtrl.Disable();
			ActivityFeedCellSource *oldCell = NULL;
			activityUiSegmentGetCell(&oldCell, segSrc, 1);
			if (oldCell != NULL) {
				oldCell->Release();
				activityUiCellSourceSetCountCallback(oldCell, NULL, 0);
			}
			activityUiSegmentRemoveCell(segSrc, 1);
			paf::IDParam tmplId((uint32_t)-1274478996);
			paf::Plugin::TemplateOpenParam openParam;
			g_liveareaPlugin->TemplateOpen(fileList, tmplId, openParam);
			fileList->BlockSync();
			fileList->UnblockSync();
		}
	} else if (m_state != 2) {
		m_state = 2;
		m_moreCtrl.Disable();
		activityUiSegmentRemoveCell(segSrc, 1);
		paf::IDParam tmplId(596741914U);
		paf::Plugin::TemplateOpenParam openParam;
		g_liveareaPlugin->TemplateOpen(fileList, tmplId, openParam);
		m_moreCtrl.Enable();
		fileList->BlockSync();
		fileList->UnblockSync();
	}

	if (segSrc != NULL)
		segSrc->Release();
}

/* 0x8119D6B8 */
LiveAreaFeed::LiveAreaFeed(LiveAreaPage *page, paf::cache::CacheImageInterface **iconCacheRef)
	: m_headerFlags(0),
	  m_moreCtrl(page->m_fileList, 2, g_liveareaPlugin, liveareaFeedOnMoreClickCb, this),
	  m_page(page),
	  m_iconCacheRef(*iconCacheRef),
	  m_jobSlots(),
	  m_state(0),
	  m_updatingCount(0),
	  m_isVisible(false),
	  m_hasTimestamp(false),
	  m_lastReadTime(new SceUInt64(0)),
	  m_lastUpdateTime(0),
	  m_latestActivityTime(0),
	  m_dialogId(-1),
	  m_lockCount(0),
	  m_news()
{
	if (m_iconCacheRef != NULL)
		m_iconCacheRef->AddRef();
	m_jobSlots.SetRemovedFunction(liveareaFeedOnJobRemovedCb, (void *)m_page->m_pageKey);
	RebuildFeed();
	if (m_state == 2)
		RequestUpdate(0);
	activityUiInitFileList(page->m_fileList);
}

/* 0x8119D7A2 */
int LiveAreaFeed::HideDialog(bool checkFirst)
{
	if (m_dialogId >= 0) {
		if (checkFirst) {
			if (activityUiIsDialogActive() != 0)
				activityUiCloseDialogFast();
		} else {
			activityUiCloseDialog();
		}
		m_dialogId = -1;
		return -1;
	}
	return m_dialogId;
}

/* 0x8119D7CA */
LiveAreaFeed::~LiveAreaFeed()
{
	if (m_dialogId >= 0) {
		activityUiCloseDialog();
		m_dialogId = -1;
	}
	m_jobSlots.SetRemovedFunction(NULL, NULL);
	m_news.clear();
	if (m_iconCacheRef != NULL)
		m_iconCacheRef->Release();
}

/* 0x8119D860 */
void LiveAreaFeed::EnqueueSetLastReadTime()
{
	const char *title = liveareaPageGetTitleIdRaw(m_page);
	bool isFriendsFeed = (sce_paf_strcmp(title, "NPXS10006") == 0);
	paf::job::JobItem *job = new SetLastReadTimeJob(title, *m_lastReadTime.get(), isFriendsFeed);
	paf::common::SharedPtr<paf::job::JobItem> jobRef(job);
	m_jobSlots.Enqueue(1, paf::job::JobQueue::default_queue, jobRef);
}

/* 0x8119D9AC */
void LiveAreaFeed::AnimateOutNewsIcon(paf::ui::Widget *widget)
{
	paf::math::v4 pos = widget->GetPos(0, NULL, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	widget->KillAnim(0x10000);
	paf::Timer *slideTimer = new paf::Timer();
	slideTimer->InitTimer(14.0f, 0, -1, 0, NULL);
	paf::math::v4 dst(pos.extract_x(), -316.0f, 0.0f, 0.0f);
	widget->SetPos(dst, slideTimer, 83951616, 2, 0.0f, NULL, NULL);
	widget->m_name = s_destroyId;
	ScePafWidget_5A9793B0(widget, 0.0f);
}

/* 0x8119DABE */
void LiveAreaFeed::AnimateMoveNewsIcon(paf::ui::Widget *widget, float posX)
{
	paf::math::v4 pos = widget->GetPos(0, NULL, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	paf::math::v4 dst(posX, pos.extract_y(), 0.0f, 0.0f);
	paf::Timer *moveTimer = new paf::Timer();
	moveTimer->InitTimer(24.0f, 0, -1, 0, NULL);
	widget->SetPos(dst, moveTimer, 100728832, 2, 0.0f, NULL, NULL);
}

/* 0x8119DBBE */
void LiveAreaFeed::AnimateDropInNewsIcon(paf::ui::Widget *widget)
{
	sce_paf_rand();
	paf::math::v4 pos = widget->GetPos(0, NULL, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	paf::math::v4 dst(pos.extract_x(), -226.0f, 0.0f, 0.0f);
	paf::Timer *dropTimer = new paf::Timer();
	dropTimer->InitTimer(33.0f, 1, -1, 0, NULL);
	widget->SetPos(dst, dropTimer, 83951616, 2, 0.0f, NULL, NULL);
}

/* 0x8119DCD2 */
void LiveAreaFeed::LayoutNewsIcons(bool hasMore)
{
	paf::ui::Widget *container = liveareaGetNewsContainer(m_page);
	if (container == NULL)
		return;

	unsigned int count = (unsigned int)m_news.size();
	unsigned int total = count;
	if (hasMore) {
		++total;
		if (total > 5) {
			total = 5;
			--count;
		}
	}
	float posX = s_newsStartX[total];
	for (unsigned int i = 0; i < count; ++i) {
		paf::IDParam iconId(m_news[i].ownerId);
		paf::ui::Widget *iconWidget = container->FindChild(iconId, 0);
		if (iconWidget != NULL) {
			AnimateMoveNewsIcon(iconWidget, posX);
		} else {
			paf::IDParam tmplId((uint32_t)-627158250);
			paf::Plugin::TemplateOpenParam openParam;
			g_liveareaPlugin->TemplateOpen(container, tmplId, openParam);
			paf::IDParam childId(220984972U);
			iconWidget = container->FindChild(childId, 0);
			if (iconWidget != NULL) {
				iconWidget->m_name = iconId;
				paf::math::v4 pos(posX, -316.0f, 0.0f, 0.0f);
				iconWidget->SetPos(pos, NULL, 0x10000, 0, 0.0f, NULL, NULL);
				AnimateDropInNewsIcon(iconWidget);
			}
		}
		posX += 92.0f;
	}
}

/* 0x8119E48C */
void LiveAreaFeed::UpdateNewsList(const paf::vector<ActivityDbStatus> &items, bool hasMore)
{
	paf::ui::Widget *container = liveareaGetNewsContainer(m_page);
	for (paf::vector<LiveAreaNewsEntry>::iterator it = m_news.begin(); it != m_news.end(); ) {
		bool found = false;
		for (size_t j = 0; j < items.size(); ++j) {
			if (it->ownerId == items[j].ownerId) {
				found = true;
				break;
			}
		}
		if (!found) {
			if (container != NULL) {
				paf::IDParam iconId(it->ownerId);
				paf::ui::Widget *iconWidget = container->FindChild(iconId, 0);
				if (iconWidget != NULL)
					AnimateOutNewsIcon(iconWidget);
			}
			it = m_news.erase(it);
		} else {
			++it;
		}
	}
	m_news.clear();
	for (size_t j = 0; j < items.size(); ++j) {
		LiveAreaNewsEntry entry;
		entry.ownerId = items[j].ownerId;
		entry.iconUrl = items[j].iconUrl;
		m_news.push_back(entry);
	}
	LayoutNewsIcons(hasMore);
}

/* 0x8119E6B6 */
void LiveAreaFeed::SetVisible(bool visible)
{
	if (visible == m_isVisible)
		return;
	m_isVisible = visible;
	if (visible) {
		paf::vector<ActivityDbStatus> empty;
		UpdateNewsList(empty, false);
	} else {
		if (m_latestActivityTime > *m_lastReadTime.get()) {
			*m_lastReadTime.get() = m_latestActivityTime;
			EnqueueSetLastReadTime();
		}
		if (m_dialogId >= 0) {
			activityUiCloseDialog();
			m_dialogId = -1;
		}
		m_lockCount = 0;
		ScePafWidget_FF5BC957(m_page->m_fileList, NULL, 2, 0.0f, NULL);
	}
}

/* 0x8119E788 */
void LiveAreaFeed::OnNetChanged()
{
	RebuildFeed();
}

/* 0x8119E790 */
void LiveAreaFeed::SetDialogId(int id)
{
	if (m_dialogId >= 0)
		activityUiCloseDialog();
	m_dialogId = id;
}

/* 0x8119E7A4 */
void LiveAreaFeed::UnlockListView()
{
	if (m_lockCount > 0) {
		--m_lockCount;
		if (m_lockCount <= 0)
			ScePafWidget_FF5BC957(m_page->m_fileList, NULL, 2, 0.0f, NULL);
	} else {
		m_lockCount = 0;
	}
}

/* 0x8119E7FC */
void LiveAreaFeed::LockListView()
{
	++m_lockCount;
	if (m_lockCount > 0)
		ScePafWidget_4588699E(m_page->m_fileList, NULL, 2, 0.0f, NULL);
}

/* 0x8119E84E */
void LiveAreaFeed::OnRefresh()
{
	m_state = 0;
	RebuildFeed();
}

/* 0x8119E85A */
int LiveAreaFeed::HandleSysUpdateError(int err)
{
	if (err != (int)0x80550D12)
		return 0;
	ShellSysupchkParam param;
	ShellSysupchkInitParam checker;
	checker.SetParam(param);
	checker.Load(NULL, 0);
	return 1;
}

/* 0x8119E9D4 */
static void liveareaFeedOnSignInBtnCb(void)
{
	sceAppMgrLaunchAppByUri(0x20000, "psnreg:");
}

/* 0x8119E9E8 */
void LiveAreaFeed::EnqueueGetActivityNews()
{
	if (*m_lastReadTime.get() < m_lastUpdateTime) {
		const char *title = liveareaPageGetTitleIdRaw(m_page);
		bool isFriendsFeed = (sce_paf_strcmp(title, "NPXS10006") == 0);
		paf::job::JobItem *job = new GetActivityNewsJob(title, *m_lastReadTime.get(),
								isFriendsFeed);
		paf::common::SharedPtr<paf::job::JobItem> jobRef(job);
		m_jobSlots.Enqueue(0, paf::job::JobQueue::default_queue, jobRef);
	}
}

/* 0x8119EB54 */
int LiveAreaFeed::UpdateTimestampText(SceUInt64 utcTick)
{
	m_hasTimestamp = true;
	if (m_lastUpdateTime == utcTick)
		return 0;
	m_lastUpdateTime = utcTick;
	if (utcTick != 0) {
		paf::ui::Widget *header = m_page->m_fileList->GetSegmentHeader(1);
		if (header != NULL) {
			paf::IDParam timeTextId((uint32_t)-1333795569);
			paf::ui::Widget *timeWidget = header->FindChild(timeTextId, 0);
			if (timeWidget != NULL) {
				SceUInt64 localTick = 0;
				if (paf::rtc::ConvertUtcToLocalTime(&m_lastUpdateTime, &localTick) == 0) {
					paf::datetime::DateTime dt;
					dt.ConvertRtcTickToDateTime(&localTick);
					wchar_t dateStr[512];
					wchar_t fullStr[512];
					dt.Format(dateStr, 1024, L"%x %X", 0);
					paf::IDParam labelId((uint32_t)-1126876603);
					const wchar_t *label = g_liveareaPlugin->GetString(labelId);
					sce_paf_swprintf(fullStr, 1024, L"%s\n%s", label, dateStr);
				}
			}
		}
	}
	return 1;
}

/* 0x8119ED3C */
void LiveAreaFeed::UpdateEmptyNotice(bool hasItems)
{
	paf::ui::Widget *header = m_page->m_fileList->GetSegmentHeader(1);
	if (header == NULL)
		return;
	paf::IDParam noticeId(654898905U);
	paf::ui::Widget *child = header->FindChild(noticeId, 0);
	if (hasItems) {
		if (child != NULL) {
			paf::IDParam emptyId("");
			child->m_name = emptyId;
			paf::Timer *fadeTimer = new paf::Timer();
			fadeTimer->InitTimer(0.0f, 0, -1, 0, NULL);
			ScePafWidget_4588699E(child, fadeTimer, 2, 0.0f, NULL);
			ScePafWidget_5A9793B0(child, 0.0f);
		}
	} else if (child == NULL) {
		paf::IDParam tmplId((uint32_t)-1196761028);
		paf::Plugin::TemplateOpenParam openParam;
		g_liveareaPlugin->TemplateOpen(header, tmplId, openParam);
	}
}

/* 0x8119EF64 */
void LiveAreaFeed::OnItemCountChanged(int count)
{
	UpdateEmptyNotice(count > 0);
	paf::ui::filelist::SegmentSource *segSrc = m_page->m_fileList->m_segment_source.get();
	if (segSrc != NULL)
		segSrc->AddRef();

	ActivityFeedCellSource *cellSrc = NULL;
	activityUiSegmentGetCell(&cellSrc, segSrc, 1);
	if (cellSrc != NULL)
		cellSrc->Release();

	if (cellSrc != NULL) {
		int total = cellSrc->m_totalCount;
		SceUInt64 lastRead = cellSrc->m_lastReadTime;
		SceUInt64 updTime = cellSrc->m_lastUpdateTime;
		SceUInt64 latest = cellSrc->m_latestActivityTime;

		int changed = UpdateTimestampText(updTime);
		if (*m_lastReadTime.get() != lastRead) {
			*m_lastReadTime.get() = lastRead;
			changed = 1;
		}
		if (m_latestActivityTime != latest)
			m_latestActivityTime = latest;
		if (changed)
			EnqueueGetActivityNews();
		if (!m_isVisible && m_latestActivityTime == *m_lastReadTime.get()) {
			paf::vector<ActivityDbStatus> empty;
			UpdateNewsList(empty, false);
		}
		liveareaMoreCtrlSetCount(&m_moreCtrl, count);
		if (count >= total)
			m_moreCtrl.Hide();
		else
			m_moreCtrl.Show();
	}
	if (segSrc != NULL)
		segSrc->Release();
}

/* 0x8119F0A6 */
void LiveAreaFeed::OnUpdateResult(int err, int subErr, int mode, bool showDialog)
{
	(void)subErr;
	if (err == (int)0x8055860C && g_liveareaMgr->m_signedInState == 0)
		liveareaMgrForcePoll();
	if (mode != 1 || !showDialog) {
		if (err == 0)
			UpdateTimestampText(0);
		return;
	}
	if (HandleSysUpdateError(err) != 0)
		return;
	if (err == 0) {
		UpdateTimestampText(0);
		return;
	}
	if (g_liveareaMgr->m_topmenuIf->isTopmenuForeground() != 0) {
		CommonGuiMessageDialogParam dlg;
		dlg.priority = 33;
		dlg.type = err;
		SceCommonGuiDialog_20B141F1(&dlg);
	}
}

/* 0x8119F204 */
void LiveAreaFeed::StopSpinAnim()
{
	if (m_updatingCount > 0) {
		--m_updatingCount;
		if (m_updatingCount == 0) {
			paf::ui::Widget *header = m_page->m_fileList->GetSegmentHeader(1);
			if (header != NULL) {
				paf::IDParam refreshBtnId(724248317U);
				paf::ui::Widget *btn = header->FindChild(refreshBtnId, 0);
				if (btn != NULL) {
					btn->KillAnimType(65542);
					paf::math::v4 rot(0.0f, 0.0f, -6.2831855f, 0.0f);
					paf::Timer *stopTimer = new paf::Timer();
					stopTimer->InitTimer(0.0f, 0, -1, 0, NULL);
					btn->SetRot(rot, 0, stopTimer, 65542, 0, 0.0f, NULL, NULL);
				}
			}
		}
	}
}

/* 0x8119F39C */
void LiveAreaFeed::FetchMoreActivities()
{
	ActivityDbQueryCondition condition;
	condition.type = ACTIVITY_DB_QUERY_TITLE;
	condition.titleId = liveareaPageGetTitleIdRaw(m_page);
	condition.limit = 20;
	if (sce_paf_strcmp(condition.titleId.c_str(), "NPXS10006") == 0)
		condition.type = ACTIVITY_DB_QUERY_ALL;

	ActivityDbSyncHandle activityHandle;
	ActivityDbNetCallback callbackPair;
	callbackPair.func = reinterpret_cast<void (*)(int, bool, void *)>(liveareaFeedOnMoreResultCb);
	callbackPair.arg = reinterpret_cast<void *>(static_cast<intptr_t>(m_page->m_pageKey));
	activityHandle.GetAvatarInfo(&condition, &callbackPair);
}

/* 0x8119F4C0 */
static void liveareaFeedOnItemCountCb(int count, int pageKey)
{
	LiveAreaPage *page = liveareaPageFindByAppId(pageKey);
	if (page != NULL && page->m_feed != NULL)
		page->m_feed->OnItemCountChanged(count);
}

/* 0x8119F4DC */
static void liveareaFeedOnRefreshBtnCb(int eventId, paf::ui::Handler *handler,
				       paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)handler;
	(void)event;
	((LiveAreaFeed *)userdata)->RequestUpdate(2);
}

/* 0x8119F4E8 */
static void liveareaFeedOnUpdateManualCb(int err, int subErr, int pageKey)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	LiveAreaPage *page = liveareaPageFindByAppId(pageKey);
	if (page != NULL && page->m_feed != NULL) {
		page->m_feed->StopSpinAnim();
		page->m_feed->OnUpdateResult(err, subErr, 1, true);
	}
	paf::thread::RMutex::main_thread_mutex.Unlock();
}

/* 0x8119F540 */
static void liveareaFeedOnUpdateSilentCb(int err, int subErr, int pageKey)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	LiveAreaPage *page = liveareaPageFindByAppId(pageKey);
	if (page != NULL && page->m_feed != NULL)
		page->m_feed->OnUpdateResult(err, subErr, 0, false);
	paf::thread::RMutex::main_thread_mutex.Unlock();
}

/* 0x8119F590 */
static void liveareaFeedOnUpdateAutoCb(int err, int subErr, int pageKey)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	LiveAreaPage *page = liveareaPageFindByAppId(pageKey);
	if (page != NULL) {
		page->m_inner.StopReloadSpin(0);
		if (page->m_feed != NULL)
			page->m_feed->OnUpdateResult(err, subErr, 1, false);
	}
	paf::thread::RMutex::main_thread_mutex.Unlock();
}

/* 0x8119F5EA */
static void liveareaFeedOnSysupchkDone(int eventId, int status, int eventArg, int userdata)
{
	(void)eventId;
	(void)status;
	(void)eventArg;
	(void)userdata;
	paf::Plugin::UnloadAsync("sysupchk_plugin", NULL, false);
}

/* 0x8119F5FE */
static void liveareaFeedOnMoreClickCb(void *userdata)
{
	((LiveAreaFeed *)userdata)->FetchMoreActivities();
}

/* 0x8119F606 */
static void liveareaFeedOnMoreResultCb(int err, int subErr, int pageKey)
{
	(void)subErr;
	paf::thread::RMutex::main_thread_mutex.Lock();
	LiveAreaPage *page = liveareaPageFindByAppId(pageKey);
	if (page != NULL && page->m_feed != NULL)
		page->m_feed->m_moreCtrl.OnResult(err != 0);
	paf::thread::RMutex::main_thread_mutex.Unlock();
}

/* 0x8119F64E */
static void liveareaFeedOnJobRemovedCb(uint32_t slot, paf::common::SharedPtr<paf::job::JobItem> item,
				       void *userdata)
{
	LiveAreaPage *page = liveareaPageFindByAppId((int)userdata);
	if (page != NULL && page->m_feed != NULL && slot == 0 && !page->m_feed->m_isVisible) {
		GetActivityNewsJob *job = (GetActivityNewsJob *)item.get();
		page->m_feed->UpdateNewsList(job->m_items, job->m_hasMore);
	}
}

/* 0x8119F67C */
static void liveareaFeedOnArrowTimerCb(int eventId, paf::ui::Handler *handler)
{
	(void)eventId;
	paf::ui::Widget *widget = (paf::ui::Widget *)handler;
	paf::math::v4 pos = widget->GetPos(0, NULL, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	paf::Timer *upTimer = new paf::Timer();
	upTimer->InitTimer(28.0f, 0, -1, 0, NULL);
	paf::math::v4 upPos(pos.extract_x(), -186.0f, 0.0f, 0.0f);
	widget->SetPos(upPos, upTimer, 352387072, 0, 0.0f, NULL, NULL);

	paf::Timer *downTimer = new paf::Timer();
	downTimer->InitTimer(14.0f, 0, -1, 0, NULL);
	paf::math::v4 downPos(pos.extract_x(), -216.0f, 0.0f, 0.0f);
	widget->SetPos(downPos, downTimer, 352387072, 0, 0.0f, NULL, NULL);

	handler->KillTimer(16777507);
}

/* 0x8119F806 */
static void liveareaFeedOnNewsClickCb(int eventId, paf::ui::Handler *handler,
				      paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)handler;
	(void)event;
	LiveAreaFeed *feed = (LiveAreaFeed *)userdata;
	paf::Timer *scrollTimer = new paf::Timer();
	scrollTimer->InitTimer(23.0f, 0, -1, 0, NULL);
	feed->m_page->m_fileList->SetFocus(1, -1, paf::ui::ListView::FOCUS_ALIGN_TYPE_HEAD, scrollTimer);
}

/* 0x8119F9EA */
int liveareaIsGamePlayUri(ActivityRecord **recordPtr)
{
	ActivityRecord *record = *recordPtr;
	if (record == NULL)
		return 0;
	if (record->GetRecordKind() != 1)
		return 0;
	return (sce_paf_strncmp(record->m_actionUri.c_str(), "psgm:play?", 10) == 0) ? 1 : 0;
}

int liveareaFeedHandleSysUpdateErr(int err)
{
	return ((LiveAreaFeed *)NULL)->HandleSysUpdateError(err);
}
