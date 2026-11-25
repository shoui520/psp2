/*
 * livearea_feed.cpp: livearea_plugin activity feed & news icon bar
 * (0x8119C7A4-0x8119FB10, sti 0x8119FA72). The file and class names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <plugin.h>
#include <framework.h>
#include <thread/thread.h>
#include <common/ref_obj.h>
#include <common/shared_ptr.h>
#include <common/job_slots.h>
#include <widget/core/widget.h>
#include <widget/w_list_view.h>
#include <graphics/surface/surface.h>
#include <rtc/rtc.h>
#include <datetime/datetime.h>
#include <timer/timer.h>
#include <appmgr.h>
#include "../../livespace_db/lsdb_keys.h"

extern paf::Plugin *g_liveareaPlugin;				/* 0x81566F94 */
extern paf::thread::RMutex g_shellMutex;			/* 0x81467AAC */
extern paf::job::JobQueue *g_activityJobQueue;			/* 0x81488474 + 146*4 */

struct LiveAreaMgr;
extern LiveAreaMgr *g_liveareaMgr;				/* 0x815670A8 */

extern "C" {
int sceAppMgrLaunchAppByName(unsigned int flags, const char *name, const char *param);
int SceActivityDb_6268A990(void *outVec, bool *outMore, paf::string *title, SceUInt64 *ts, int maxCnt, int isSpecial);
int SceActivityDb_C37F81FE(paf::string *title, SceUInt64 *ts, int isSpecial);
void SceActivityDb_B83F0A77(void *ctx);
void SceActivityDb_A16326B1(void *ctx, void *req, void *cbPair);
void SceActivityDb_937D49BA(void *ctx);
void SceCommonGuiDialog_696DB9C9(void *param);
void SceCommonGuiDialog_20B141F1(void *param);
int sce_paf_rand(void);
}

extern void *shellPluginGetInterface(const char *name, int id); /* 0x810C9D14 */
extern const char *liveareaPageGetTitleIdRaw(void *page);	/* 0x8119982C */
extern void *liveareaPageFindByAppId(int appId);		/* 0x8119C0C6 */
extern void liveareaMgrForcePoll(void);				/* 0x8119AC7A */
extern void liveareaCb20EF44(void);				/* 0x8120EF44 */
extern void liveareaPageInnerSubAB5DA(void *inner, int status);	/* 0x811AB5DA */
extern int liveareaUpdaterSub9DA(void *u, void *out, const char *title, int mode, void *cb, int appId); /* 0x811A29DA */
extern int liveareaCacheCreateImg(const paf::string &path, void *outRef, void *opt, void *q); /* 0x8119A51E */

extern void liveareaMoreCtrlCtor(void *c, void *lv, int mode, paf::Plugin *p, void (*cb)(void *), void *arg); /* 0x811A1348 */
extern void liveareaMoreCtrlDtor(void *c);			/* 0x811A13C4 */
extern void liveareaMoreCtrlEnable(void *c);			/* 0x811A1AF2 */
extern void liveareaMoreCtrlDisable(void *c);			/* 0x811A1B76 */
extern void liveareaMoreCtrlShow(void *c);			/* 0x811A1BA2 */
extern void liveareaMoreCtrlHide(void *c);			/* 0x811A1C7C */
extern void liveareaMoreCtrlSetCount(void *c, int count);	/* 0x811A1CA2 */
extern void liveareaMoreCtrlOnResult(void *c, bool ok);		/* 0x811A1D10 */
extern paf::ui::Widget *liveareaGetNewsContainer(void *page);	/* 0x811D4E58 */

extern void liveareaListSrcGetSeg(paf::common::RefObj **out, paf::common::RefObj *src, int idx); /* 0x8134AB4C */
extern void liveareaListSrcRemoveSeg(paf::common::RefObj *src, int idx); /* 0x8134AAA2 */
extern void liveareaListSrcSetSeg(paf::common::RefObj *src, int idx, paf::common::RefObj **cell, void *param); /* 0x8134A8D2 */
extern void liveareaCellSrcSetCb(paf::common::RefObj *cell, void (*cb)(int, int), int appId); /* 0x8134BF04 */
extern void liveareaCellSrcSetMode(paf::common::RefObj *cell, int mode); /* 0x81208F50 */
extern void *liveareaCellSrcCtor(void *mem, void *vis, void *filter); /* 0x8134BBD0 */
extern void *liveareaFilterCtor(void *mem, int cap);		/* 0x8134CAE4 */
extern void *liveareaVisCtor(void *mem, void *lastReadSp, void *ref); /* 0x811A240E */
extern void liveareaVisSetFlag1(void *vis, int v);		/* 0x811A25B0 */
extern void liveareaVisSetFlag2(void *vis, int v);		/* 0x811A25B8 */
extern void liveareaInitListView(void *lv);			/* 0x81350B40 */
extern int liveareaCheckDialogActive(void);			/* 0x8135B002 */
extern void liveareaCloseDialog(void);				/* 0x81353DF2 */
extern void liveareaCloseDialogFast(void);			/* 0x81353E16 */

extern void shellSysupchkParamCtor(void *p);			/* 0x813623D8 */
extern void shellSysupchkCtor(void *s);				/* 0x81362410 */
extern void shellSysupchkInit(void *s, void *p);		/* 0x81362476 */
extern void shellSysupchkRun(void *s, int a, int b);		/* 0x81362AA0 */
extern void shellSysupchkDtor(void *s);				/* 0x814091D0 */

static paf::IDParam s_destroyId("destroy");			/* 0x81567100 */

static const float s_newsStartX[6] = {
	0.0f, -46.0f, -92.0f, -138.0f, -184.0f, -230.0f
};

struct LiveAreaNewsEntry {
	paf::string id;
	paf::string iconUrl;
};

struct LiveAreaActivityNewsItem {
	paf::string id;
	unsigned char pad[124];
	paf::string iconUrl;
	unsigned char pad2[20];
};

class GetActivityNewsJob : public paf::common::SlotJobItem {
public:
	GetActivityNewsJob(const char *title, SceUInt64 lastRead, bool isSpecial)
		: paf::common::SlotJobItem("GetActivityNewsJob", NULL),
		  m_title(title),
		  m_lastRead(lastRead),
		  m_isSpecial(isSpecial ? 1 : 0),
		  m_items(),
		  m_hasMore(false)
	{}

	virtual ~GetActivityNewsJob() {}			/* 0x8119F88E / 0x8119F8D8 */
	virtual int Run()					/* 0x8119F928 */
	{
		return SceActivityDb_6268A990(&m_items, &m_hasMore, &m_title, &m_lastRead, 5, m_isSpecial);
	}

	paf::string m_title;					/* +0x20 */
	SceUInt64 m_lastRead;					/* +0x30 */
	int m_isSpecial;					/* +0x38 */
	paf::vector<LiveAreaActivityNewsItem> m_items;		/* +0x3C */
	bool m_hasMore;						/* +0x4C */
};

class SetLastReadTimeJob : public paf::common::SlotJobItem {
public:
	SetLastReadTimeJob(const char *title, SceUInt64 lastRead, bool isSpecial)
		: paf::common::SlotJobItem("SetLastReadTimeJob", NULL),
		  m_title(title),
		  m_isSpecial(isSpecial ? 1 : 0),
		  m_lastRead(lastRead)
	{}

	virtual ~SetLastReadTimeJob() {}			/* 0x8119F94E / 0x8119F990 */
	virtual int Run()					/* 0x8119F9D8 */
	{
		return SceActivityDb_C37F81FE(&m_title, &m_lastRead, m_isSpecial);
	}

	paf::string m_title;					/* +0x20 */
	int m_isSpecial;					/* +0x2C */
	SceUInt64 m_lastRead;					/* +0x30 */
};

class LiveAreaFeed {
public:
	LiveAreaFeed(void *page, void **ref);			/* 0x8119D6B8 */
	~LiveAreaFeed();					/* 0x8119D7CA */

	int StartSpinAnim();					/* 0x8119C7A4 */
	int RequestUpdate(int mode);				/* 0x8119C964 */
	void RebuildFeed();					/* 0x8119CA86 */
	int HideDialog(bool checkFirst);			/* 0x8119D7A2 */
	void EnqueueSetLastReadTime();				/* 0x8119D860 */
	void AnimateOutNewsIcon(paf::ui::Widget *w);		/* 0x8119D9AC */
	void AnimateMoveNewsIcon(paf::ui::Widget *w, float x);	/* 0x8119DABE */
	void AnimateDropInNewsIcon(paf::ui::Widget *w);		/* 0x8119DBBE */
	void LayoutNewsIcons(bool hasMore);			/* 0x8119DCD2 */
	void UpdateNewsList(const paf::vector<LiveAreaActivityNewsItem> &items, bool hasMore); /* 0x8119E48C */
	void SetVisible(bool visible);				/* 0x8119E6B6 */
	void OnNetChanged();					/* 0x8119E788 */
	void SetDialogId(int id);				/* 0x8119E790 */
	void UnlockListView();					/* 0x8119E7A4 */
	void LockListView();					/* 0x8119E7FC */
	void OnRefresh();					/* 0x8119E84E */
	int HandleSysUpdateError(int err);			/* 0x8119E85A */
	void EnqueueGetActivityNews();				/* 0x8119E9E8 */
	int UpdateTimestampText(SceUInt64 utcTick);		/* 0x8119EB54 */
	void UpdateEmptyNotice(bool hasItems);			/* 0x8119ED3C */
	void OnItemCountChanged(int count);			/* 0x8119EF64 */
	void OnUpdateResult(int err, int subErr, int mode, bool showDialog); /* 0x8119F0A6 */
	void StopSpinAnim();					/* 0x8119F204 */
	void FetchMoreActivities();				/* 0x8119F39C */

	int m_reserved0;					/* +0x00 */
	unsigned char m_moreCtrl[32];				/* +0x04 */
	void *m_page;						/* +0x24 */
	void *m_refObj;						/* +0x28 */
	paf::common::JobSlots m_jobSlots;			/* +0x2C */
	int m_state;						/* +0x44 */
	int m_updatingCount;					/* +0x48 */
	bool m_isVisible;					/* +0x4C */
	bool m_hasTimestamp;					/* +0x4D */
	unsigned char m_pad4E[2];
	paf::common::SharedPtr<SceUInt64> m_lastReadTime;	/* +0x50 */
	SceUInt64 m_lastUpdateTime;				/* +0x58 */
	SceUInt64 m_latestActivityTime;				/* +0x60 */
	int m_dialogId;						/* +0x68 */
	int m_lockCount;					/* +0x6C */
	paf::vector<LiveAreaNewsEntry> m_news;			/* +0x70 */
};

static void liveareaFeedOnItemCountCb(int count, int appId);	/* 0x8119F4C0 */
static void liveareaFeedOnRefreshBtnCb(int eventId, int handler, int ev, void *arg); /* 0x8119F4DC */
static void liveareaFeedOnUpdateManualCb(int err, int subErr, int appId); /* 0x8119F4E8 */
static void liveareaFeedOnUpdateSilentCb(int err, int subErr, int appId); /* 0x8119F540 */
static void liveareaFeedOnUpdateAutoCb(int err, int subErr, int appId); /* 0x8119F590 */
static void liveareaFeedOnSysupchkDone(int eventId, int status, int param1, int param2); /* 0x8119F5EA */
static void liveareaFeedOnMoreClickCb(void *arg);		/* 0x8119F5FE */
static void liveareaFeedOnMoreResultCb(int err, int subErr, int appId); /* 0x8119F606 */
static void liveareaFeedOnJobRemovedCb(uint32_t slot, paf::common::SharedPtr<paf::job::JobItem> item, void *userdata); /* 0x8119F64E */
static void liveareaFeedOnArrowTimerCb(int eventId, paf::ui::Handler *h); /* 0x8119F67C */
static void liveareaFeedOnNewsClickCb(int eventId, int handler, int ev, void *arg); /* 0x8119F806 */
static void liveareaFeedOnSignInBtnCb(void);			/* 0x8119E9D4 */

static paf::ui::ListView *feedGetListView(void *page)
{
	return *(paf::ui::ListView **)((char *)page + 40);
}

static int feedGetAppId(void *page)
{
	return *(int *)page;
}

/* 0x8119C7A4 */
int LiveAreaFeed::StartSpinAnim()
{
	if (m_updatingCount != 0)
		return 0;
	m_updatingCount = 1;
	paf::ui::Widget *hdr = feedGetListView(m_page)->GetSegmentHeader(1);
	if (hdr != NULL) {
		paf::IDParam id(724248317U);
		paf::ui::Widget *btn = hdr->FindChild(id, 0);
		if (btn != NULL) {
			paf::math::v4 rot(0.0f, 0.0f, -6.2831855f, 0.0f);
			paf::Timer *t1 = new paf::Timer();
			t1->InitTimer(22.0f, 0, -1, 0, NULL);
			btn->SetRot(rot, 0, t1, 65542, 0, 0.0f, NULL, NULL);

			paf::Timer *t2 = new paf::Timer();
			t2->InitTimer(0.0f, 0, -1, 0, NULL);
			btn->SetRot(rot, 0, t2, 268500998, 0, 0.0f, NULL, NULL);
		}
	}
	return 1;
}

/* 0x8119C964 */
int LiveAreaFeed::RequestUpdate(int mode)
{
	void *updater = (char *)g_liveareaMgr + 48;
	const char *title = liveareaPageGetTitleIdRaw(m_page);
	int appId = feedGetAppId(m_page);
	char outBuf[8];

	if (mode == 0) {
		if (liveareaUpdaterSub9DA(updater, outBuf, title, 0, (void *)liveareaFeedOnUpdateSilentCb, appId) == 0)
			return 0;
	} else if (mode == 1) {
		if (liveareaUpdaterSub9DA(updater, outBuf, title, 1, (void *)liveareaFeedOnUpdateAutoCb, appId) == 0)
			return 0;
	} else if (mode == 2) {
		if (m_updatingCount != 0)
			return -1;
		if (*((int *)g_liveareaMgr + 4) == 2) {
			sceAppMgrLaunchAppByName(0x20000, "NPXS10016", "signin");
			return -1;
		}
		liveareaCb20EF44();
		if (liveareaUpdaterSub9DA(updater, outBuf, title, 1, (void *)liveareaFeedOnUpdateManualCb, appId) == 0) {
			StartSpinAnim();
			return 0;
		}
	}
	return -1;
}

/* 0x8119CA86 */
void LiveAreaFeed::RebuildFeed()
{
	paf::ui::ListView *lv = feedGetListView(m_page);
	paf::common::RefObj *segSrc = *(paf::common::RefObj **)((char *)lv + 1252);
	if (segSrc != NULL)
		segSrc->AddRef();

	const char *title = liveareaPageGetTitleIdRaw(m_page);
	bool isSpecial = (sce_paf_strcmp(title, "NPXS10999") == 0 ||
			  sce_paf_strcmp(title, "NPXS10998") == 0);
	int netState = *((int *)g_liveareaMgr + 4);
	int signedIn = *((int *)g_liveareaMgr + 5);

	if (netState == 0 && !isSpecial) {
		if (m_state != 1) {
			m_state = 1;
			liveareaMoreCtrlDisable(m_moreCtrl);
			paf::common::RefObj *oldCell = NULL;
			liveareaListSrcGetSeg(&oldCell, segSrc, 1);
			if (oldCell != NULL) {
				oldCell->Release();
				liveareaCellSrcSetCb(oldCell, NULL, 0);
			}
			liveareaListSrcRemoveSeg(segSrc, 1);
			paf::IDParam tmplId(748860498U);
			paf::Plugin::TemplateOpenParam op;
			g_liveareaPlugin->TemplateOpen(lv, tmplId, op);
			lv->BlockSync();
			lv->UnblockSync();
		}
	} else if (signedIn != 0) {
		if (m_state != 3) {
			m_state = 3;
			liveareaMoreCtrlDisable(m_moreCtrl);
			paf::common::RefObj *oldCell = NULL;
			liveareaListSrcGetSeg(&oldCell, segSrc, 1);
			if (oldCell != NULL) {
				oldCell->Release();
				liveareaCellSrcSetCb(oldCell, NULL, 0);
			}
			liveareaListSrcRemoveSeg(segSrc, 1);
			paf::IDParam tmplId((uint32_t)-1274478996);
			paf::Plugin::TemplateOpenParam op;
			g_liveareaPlugin->TemplateOpen(lv, tmplId, op);
			lv->BlockSync();
			lv->UnblockSync();
		}
	} else if (m_state != 2) {
		m_state = 2;
		liveareaMoreCtrlDisable(m_moreCtrl);
		liveareaListSrcRemoveSeg(segSrc, 1);
		paf::IDParam tmplId(596741914U);
		paf::Plugin::TemplateOpenParam op;
		g_liveareaPlugin->TemplateOpen(lv, tmplId, op);
		liveareaMoreCtrlEnable(m_moreCtrl);
		lv->BlockSync();
		lv->UnblockSync();
	}

	if (segSrc != NULL)
		segSrc->Release();
}

/* 0x8119D6B8 */
LiveAreaFeed::LiveAreaFeed(void *page, void **ref)
	: m_reserved0(0),
	  m_page(page),
	  m_refObj(*ref),
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
	liveareaMoreCtrlCtor(m_moreCtrl, feedGetListView(page), 2, g_liveareaPlugin,
			     liveareaFeedOnMoreClickCb, this);
	if (m_refObj != NULL) {
		typedef void (*AddRefFn)(void *);
		(*(AddRefFn **)(*(void **)m_refObj))[6](m_refObj);
	}
	m_jobSlots.SetRemovedFunction(liveareaFeedOnJobRemovedCb, (void *)feedGetAppId(m_page));
	RebuildFeed();
	if (m_state == 2)
		RequestUpdate(0);
	liveareaInitListView(feedGetListView(page));
}

/* 0x8119D7A2 */
int LiveAreaFeed::HideDialog(bool checkFirst)
{
	if (m_dialogId >= 0) {
		if (checkFirst) {
			if (liveareaCheckDialogActive() != 0)
				liveareaCloseDialogFast();
		} else {
			liveareaCloseDialog();
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
		liveareaCloseDialog();
		m_dialogId = -1;
	}
	m_jobSlots.SetRemovedFunction(NULL, NULL);
	m_news.clear();
	if (m_refObj != NULL) {
		typedef void (*ReleaseFn)(void *);
		(*(ReleaseFn **)(*(void **)m_refObj))[7](m_refObj);
	}
	liveareaMoreCtrlDtor(m_moreCtrl);
}

/* 0x8119D860 */
void LiveAreaFeed::EnqueueSetLastReadTime()
{
	const char *title = liveareaPageGetTitleIdRaw(m_page);
	bool isSpecial = (sce_paf_strcmp(title, "NPXS10006") == 0);
	paf::job::JobItem *job = new SetLastReadTimeJob(title, *m_lastReadTime.get(), isSpecial);
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	m_jobSlots.Enqueue(1, g_activityJobQueue, sp);
}

/* 0x8119D9AC */
void LiveAreaFeed::AnimateOutNewsIcon(paf::ui::Widget *w)
{
	paf::math::v4 pos = w->GetPos(0, NULL, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	w->KillAnim(0x10000);
	paf::Timer *t = new paf::Timer();
	t->InitTimer(14.0f, 0, -1, 0, NULL);
	paf::math::v4 dst(((const float *)&pos)[0], -316.0f, 0.0f, 0.0f);
	w->SetPos(dst, t, 83951616, 2, 0.0f, NULL, NULL);
	*(paf::IDParam *)((char *)w + 320) = s_destroyId;
	ScePafWidget_5A9793B0(w, 0.0f);
}

/* 0x8119DABE */
void LiveAreaFeed::AnimateMoveNewsIcon(paf::ui::Widget *w, float x)
{
	paf::math::v4 pos = w->GetPos(0, NULL, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	paf::math::v4 dst(x, ((const float *)&pos)[1], 0.0f, 0.0f);
	paf::Timer *t = new paf::Timer();
	t->InitTimer(24.0f, 0, -1, 0, NULL);
	w->SetPos(dst, t, 100728832, 2, 0.0f, NULL, NULL);
}

/* 0x8119DBBE */
void LiveAreaFeed::AnimateDropInNewsIcon(paf::ui::Widget *w)
{
	sce_paf_rand();
	paf::math::v4 pos = w->GetPos(0, NULL, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	paf::math::v4 dst(((const float *)&pos)[0], -226.0f, 0.0f, 0.0f);
	paf::Timer *t = new paf::Timer();
	t->InitTimer(33.0f, 1, -1, 0, NULL);
	w->SetPos(dst, t, 83951616, 2, 0.0f, NULL, NULL);
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
	float x = s_newsStartX[total];
	for (unsigned int i = 0; i < count; ++i) {
		paf::IDParam id(m_news[i].id);
		paf::ui::Widget *w = container->FindChild(id, 0);
		if (w != NULL) {
			AnimateMoveNewsIcon(w, x);
		} else {
			paf::IDParam tmplId((uint32_t)-627158250);
			paf::Plugin::TemplateOpenParam op;
			g_liveareaPlugin->TemplateOpen(container, tmplId, op);
			paf::IDParam childId(220984972U);
			w = container->FindChild(childId, 0);
			if (w != NULL) {
				*(paf::IDParam *)((char *)w + 320) = id;
				paf::math::v4 pos(x, -316.0f, 0.0f, 0.0f);
				w->SetPos(pos, NULL, 0x10000, 0, 0.0f, NULL, NULL);
				AnimateDropInNewsIcon(w);
			}
		}
		x += 92.0f;
	}
}

/* 0x8119E48C */
void LiveAreaFeed::UpdateNewsList(const paf::vector<LiveAreaActivityNewsItem> &items, bool hasMore)
{
	paf::ui::Widget *container = liveareaGetNewsContainer(m_page);
	for (paf::vector<LiveAreaNewsEntry>::iterator it = m_news.begin(); it != m_news.end(); ) {
		bool found = false;
		for (size_t j = 0; j < items.size(); ++j) {
			if (it->id == items[j].id) {
				found = true;
				break;
			}
		}
		if (!found) {
			if (container != NULL) {
				paf::IDParam id(it->id);
				paf::ui::Widget *w = container->FindChild(id, 0);
				if (w != NULL)
					AnimateOutNewsIcon(w);
			}
			it = m_news.erase(it);
		} else {
			++it;
		}
	}
	m_news.clear();
	for (size_t j = 0; j < items.size(); ++j) {
		LiveAreaNewsEntry e;
		e.id = items[j].id;
		e.iconUrl = items[j].iconUrl;
		m_news.push_back(e);
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
		paf::vector<LiveAreaActivityNewsItem> empty;
		UpdateNewsList(empty, false);
	} else {
		if (m_latestActivityTime > *m_lastReadTime.get()) {
			*m_lastReadTime.get() = m_latestActivityTime;
			EnqueueSetLastReadTime();
		}
		if (m_dialogId >= 0) {
			liveareaCloseDialog();
			m_dialogId = -1;
		}
		m_lockCount = 0;
		paf::ui::ListView *lv = feedGetListView(m_page);
		ScePafWidget_FF5BC957(lv, NULL, 2, 0.0f, NULL);
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
		liveareaCloseDialog();
	m_dialogId = id;
}

/* 0x8119E7A4 */
void LiveAreaFeed::UnlockListView()
{
	if (m_lockCount > 0) {
		--m_lockCount;
		if (m_lockCount <= 0) {
			paf::ui::ListView *lv = feedGetListView(m_page);
			ScePafWidget_FF5BC957(lv, NULL, 2, 0.0f, NULL);
		}
	} else {
		m_lockCount = 0;
	}
}

/* 0x8119E7FC */
void LiveAreaFeed::LockListView()
{
	++m_lockCount;
	if (m_lockCount > 0) {
		paf::ui::ListView *lv = feedGetListView(m_page);
		ScePafWidget_4588699E(lv, NULL, 2, 0.0f, NULL);
	}
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
	unsigned char param[80];
	unsigned char checker[160];
	shellSysupchkParamCtor(param);
	shellSysupchkCtor(checker);
	shellSysupchkInit(checker, param);
	shellSysupchkRun(checker, 0, 0);
	shellSysupchkDtor(checker);
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
		bool isSpecial = (sce_paf_strcmp(title, "NPXS10006") == 0);
		paf::job::JobItem *job = new GetActivityNewsJob(title, *m_lastReadTime.get(), isSpecial);
		paf::common::SharedPtr<paf::job::JobItem> sp(job);
		m_jobSlots.Enqueue(0, g_activityJobQueue, sp);
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
		paf::ui::Widget *hdr = feedGetListView(m_page)->GetSegmentHeader(1);
		if (hdr != NULL) {
			paf::IDParam id((uint32_t)-1333795569);
			paf::ui::Widget *w = hdr->FindChild(id, 0);
			if (w != NULL) {
				SceUInt64 localTick = 0;
				if (paf::rtc::ConvertUtcToLocalTime(&m_lastUpdateTime, &localTick) == 0) {
					paf::datetime::DateTime dt;
					dt.ConvertRtcTickToDateTime(&localTick);
					wchar_t dateStr[512];
					wchar_t fullStr[512];
					dt.Format(dateStr, 1024, L"%x %X", 0);
					paf::IDParam strId((uint32_t)-1126876603);
					const wchar_t *label = g_liveareaPlugin->GetString(strId);
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
	paf::ui::Widget *hdr = feedGetListView(m_page)->GetSegmentHeader(1);
	if (hdr == NULL)
		return;
	paf::IDParam id(654898905U);
	paf::ui::Widget *child = hdr->FindChild(id, 0);
	if (hasItems) {
		if (child != NULL) {
			paf::IDParam emptyId("");
			*(paf::IDParam *)((char *)child + 320) = emptyId;
			paf::Timer *t = new paf::Timer();
			t->InitTimer(0.0f, 0, -1, 0, NULL);
			ScePafWidget_4588699E(child, t, 2, 0.0f, NULL);
			ScePafWidget_5A9793B0(child, 0.0f);
		}
	} else if (child == NULL) {
		paf::IDParam tmplId((uint32_t)-1196761028);
		paf::Plugin::TemplateOpenParam op;
		g_liveareaPlugin->TemplateOpen(hdr, tmplId, op);
	}
}

/* 0x8119EF64 */
void LiveAreaFeed::OnItemCountChanged(int count)
{
	UpdateEmptyNotice(count > 0);
	paf::ui::ListView *lv = feedGetListView(m_page);
	paf::common::RefObj *segSrc = *(paf::common::RefObj **)((char *)lv + 1252);
	if (segSrc != NULL)
		segSrc->AddRef();

	paf::common::RefObj *cellSrc = NULL;
	liveareaListSrcGetSeg(&cellSrc, segSrc, 1);
	if (cellSrc != NULL)
		cellSrc->Release();

	if (cellSrc != NULL) {
		int total = *((int *)cellSrc + 19);
		SceUInt64 lastRead = *((SceUInt64 *)cellSrc + 10);
		SceUInt64 updTime = *((SceUInt64 *)cellSrc + 11);
		SceUInt64 latest = *((SceUInt64 *)cellSrc + 12);

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
			paf::vector<LiveAreaActivityNewsItem> empty;
			UpdateNewsList(empty, false);
		}
		liveareaMoreCtrlSetCount(m_moreCtrl, count);
		if (count >= total)
			liveareaMoreCtrlHide(m_moreCtrl);
		else
			liveareaMoreCtrlShow(m_moreCtrl);
	}
	if (segSrc != NULL)
		segSrc->Release();
}

/* 0x8119F0A6 */
void LiveAreaFeed::OnUpdateResult(int err, int subErr, int mode, bool showDialog)
{
	(void)subErr;
	if (err == (int)0x8055860C && *((int *)g_liveareaMgr + 5) == 0)
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
	typedef int (*IsActiveFn)(void);
	void *topmenu = *((void **)g_liveareaMgr + 1);
	if ((*(IsActiveFn *)((char *)topmenu + 196))() != 0) {
		unsigned char dlg[200];
		SceCommonGuiDialog_696DB9C9(dlg);
		*(int *)(dlg + 48) = 33;
		*(int *)(dlg + 68) = err;
		SceCommonGuiDialog_20B141F1(dlg);
	}
}

/* 0x8119F204 */
void LiveAreaFeed::StopSpinAnim()
{
	if (m_updatingCount > 0) {
		--m_updatingCount;
		if (m_updatingCount == 0) {
			paf::ui::Widget *hdr = feedGetListView(m_page)->GetSegmentHeader(1);
			if (hdr != NULL) {
				paf::IDParam id(724248317U);
				paf::ui::Widget *btn = hdr->FindChild(id, 0);
				if (btn != NULL) {
					btn->KillAnimType(65542);
					paf::math::v4 rot(0.0f, 0.0f, -6.2831855f, 0.0f);
					paf::Timer *t = new paf::Timer();
					t->InitTimer(0.0f, 0, -1, 0, NULL);
					btn->SetRot(rot, 0, t, 65542, 0, 0.0f, NULL, NULL);
				}
			}
		}
	}
}

/* 0x8119F39C */
void LiveAreaFeed::FetchMoreActivities()
{
	struct {
		int type;
		paf::string filter;
		paf::string title;
		int limit;
	} req;
	req.type = 0;
	req.title = liveareaPageGetTitleIdRaw(m_page);
	req.limit = 20;
	if (sce_paf_strcmp(req.title.c_str(), "NPXS10006") == 0)
		req.type = 1;

	unsigned char ctx[4];
	SceActivityDb_B83F0A77(ctx);
	void *cbPair[2];
	cbPair[0] = (void *)liveareaFeedOnMoreResultCb;
	cbPair[1] = (void *)feedGetAppId(m_page);
	SceActivityDb_A16326B1(ctx, &req, cbPair);
	SceActivityDb_937D49BA(ctx);
}

/* 0x8119F4C0 */
static void liveareaFeedOnItemCountCb(int count, int appId)
{
	void *page = liveareaPageFindByAppId(appId);
	if (page != NULL) {
		LiveAreaFeed *feed = *(LiveAreaFeed **)((char *)page + 248);
		if (feed != NULL)
			feed->OnItemCountChanged(count);
	}
}

/* 0x8119F4DC */
static void liveareaFeedOnRefreshBtnCb(int eventId, int handler, int ev, void *arg)
{
	(void)eventId;
	(void)handler;
	(void)ev;
	((LiveAreaFeed *)arg)->RequestUpdate(2);
}

/* 0x8119F4E8 */
static void liveareaFeedOnUpdateManualCb(int err, int subErr, int appId)
{
	g_shellMutex.Lock();
	void *page = liveareaPageFindByAppId(appId);
	if (page != NULL) {
		LiveAreaFeed *feed = *(LiveAreaFeed **)((char *)page + 248);
		if (feed != NULL) {
			feed->StopSpinAnim();
			feed->OnUpdateResult(err, subErr, 1, true);
		}
	}
	g_shellMutex.Unlock();
}

/* 0x8119F540 */
static void liveareaFeedOnUpdateSilentCb(int err, int subErr, int appId)
{
	g_shellMutex.Lock();
	void *page = liveareaPageFindByAppId(appId);
	if (page != NULL) {
		LiveAreaFeed *feed = *(LiveAreaFeed **)((char *)page + 248);
		if (feed != NULL)
			feed->OnUpdateResult(err, subErr, 0, false);
	}
	g_shellMutex.Unlock();
}

/* 0x8119F590 */
static void liveareaFeedOnUpdateAutoCb(int err, int subErr, int appId)
{
	g_shellMutex.Lock();
	void *page = liveareaPageFindByAppId(appId);
	if (page != NULL) {
		liveareaPageInnerSubAB5DA((char *)page + 48, 0);
		LiveAreaFeed *feed = *(LiveAreaFeed **)((char *)page + 248);
		if (feed != NULL)
			feed->OnUpdateResult(err, subErr, 1, false);
	}
	g_shellMutex.Unlock();
}

/* 0x8119F5EA */
static void liveareaFeedOnSysupchkDone(int eventId, int status, int param1, int param2)
{
	(void)eventId;
	(void)status;
	(void)param1;
	(void)param2;
	paf::Plugin::UnloadAsync("sysupchk_plugin", NULL, NULL);
}

/* 0x8119F5FE */
static void liveareaFeedOnMoreClickCb(void *arg)
{
	((LiveAreaFeed *)arg)->FetchMoreActivities();
}

/* 0x8119F606 */
static void liveareaFeedOnMoreResultCb(int err, int subErr, int appId)
{
	(void)subErr;
	g_shellMutex.Lock();
	void *page = liveareaPageFindByAppId(appId);
	if (page != NULL) {
		LiveAreaFeed *feed = *(LiveAreaFeed **)((char *)page + 248);
		if (feed != NULL)
			liveareaMoreCtrlOnResult(feed->m_moreCtrl, err != 0);
	}
	g_shellMutex.Unlock();
}

/* 0x8119F64E */
static void liveareaFeedOnJobRemovedCb(uint32_t slot, paf::common::SharedPtr<paf::job::JobItem> item, void *userdata)
{
	void *page = liveareaPageFindByAppId((int)userdata);
	if (page != NULL) {
		LiveAreaFeed *feed = *(LiveAreaFeed **)((char *)page + 248);
		if (feed != NULL && slot == 0 && !feed->m_isVisible) {
			GetActivityNewsJob *job = (GetActivityNewsJob *)item.get();
			feed->UpdateNewsList(job->m_items, job->m_hasMore);
		}
	}
}

/* 0x8119F67C */
static void liveareaFeedOnArrowTimerCb(int eventId, paf::ui::Handler *h)
{
	(void)eventId;
	paf::ui::Widget *w = (paf::ui::Widget *)h;
	paf::math::v4 pos = w->GetPos(0, NULL, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	paf::Timer *t1 = new paf::Timer();
	t1->InitTimer(28.0f, 0, -1, 0, NULL);
	paf::math::v4 p1(((const float *)&pos)[0], -186.0f, 0.0f, 0.0f);
	w->SetPos(p1, t1, 352387072, 0, 0.0f, NULL, NULL);

	paf::Timer *t2 = new paf::Timer();
	t2->InitTimer(14.0f, 0, -1, 0, NULL);
	paf::math::v4 p2(((const float *)&pos)[0], -216.0f, 0.0f, 0.0f);
	w->SetPos(p2, t2, 352387072, 0, 0.0f, NULL, NULL);

	h->KillTimer(16777507);
}

/* 0x8119F806 */
static void liveareaFeedOnNewsClickCb(int eventId, int handler, int ev, void *arg)
{
	(void)eventId;
	(void)handler;
	(void)ev;
	LiveAreaFeed *feed = (LiveAreaFeed *)arg;
	paf::ui::ListView *lv = feedGetListView(feed->m_page);
	paf::Timer *t = new paf::Timer();
	t->InitTimer(23.0f, 0, -1, 0, NULL);
	typedef void (*FocusCellFn)(paf::ui::ListView *, int, int, int, paf::Timer *);
	FocusCellFn fn = (*(FocusCellFn **)(*(void **)lv))[628 / 4];
	fn(lv, 1, -1, 0, t);
}

/* 0x8119F9EA */
int liveareaIsGamePlayUri(void **itemPtr)
{
	void *item = *itemPtr;
	if (item == NULL)
		return 0;
	typedef int (*GetTypeFn)(void *);
	if ((*(GetTypeFn **)(*(void **)item))[2](item) != 1)
		return 0;
	paf::string uri = *(paf::string *)((char *)item + 128);
	return (sce_paf_strncmp(uri.c_str(), "psgm:play?", 10) == 0) ? 1 : 0;
}
