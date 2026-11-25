/*
 * livearea_page.cpp: livearea_plugin per-app LiveArea page controller
 * (0x8119B594-0x8119C7A4, sti 0x8119C6EE). The file and class names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <math.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <std/list>
#include <plugin.h>
#include <thread/thread.h>
#include <common/shared_ptr.h>
#include <widget/core/widget.h>
#include <widget/w_list_view.h>
#include <widget/w_scroll_view_base.h>
#include "include/rtc/rtc.h"
#include <timer/timer.h>
#include "../../livespace_db/lsdb_keys.h"

extern paf::Plugin *g_liveareaPlugin;				/* 0x81566F94 */
extern paf::thread::RMutex g_shellMutex;			/* 0x81467AAC */

struct LiveAreaMgr;
extern LiveAreaMgr *g_liveareaMgr;				/* 0x815670A8 */

extern "C" {
int SceShell_7FE4E732(int id, void *cb, void *arg);
int SceShell_D4C7D2B7(int id, void *cb, void *arg);
int SceShell_E3D3A162(int id, void *cb, void *arg);
void SceShell_83D16CA8(void);
const char *SceLsdb_C070E42E(void *sp);
}

extern bool liveareaMgrIsPendingLaunch(const char *titleId);	/* 0x8119ACFC */
extern void liveareaMgrClearPendingLaunch(void);		/* 0x8119ACE4 */
extern void liveareaPageInnerCtor(void *inner, void *page, int flags, int mode); /* 0x811A3C72 */
extern void liveareaPageInnerDtor(void *inner);			/* 0x811A3F4E */
extern void liveareaPageInnerInit(void *inner);			/* 0x811A417E */
extern int liveareaPageInnerSubAACC2(void *inner, int state, int flags); /* 0x811AACC2 */
extern void liveareaPageInnerSubAAD60(void *inner, int mode, const char *path); /* 0x811AAD60 */
extern void liveareaPageInnerSubAAFB8(void *inner);		/* 0x811AAFB8 */
extern void liveareaPageInnerSubAB056(void *inner);		/* 0x811AB056 */
extern void liveareaPageInnerSubAB2DE(void *inner);		/* 0x811AB2DE */
extern void liveareaPageInnerSubAB944(void *inner);		/* 0x811AB944 */
extern void liveareaPageInnerSubAB954(void *inner);		/* 0x811AB954 */
extern int liveareaPageInnerSubABA06(void *inner);		/* 0x811ABA06 */
extern void *liveareaFeedCtor(void *mem, void *page, void *ref); /* 0x8119D6B8 */
extern void liveareaFeedDtor(void *feed);			/* 0x8119D7CA */
extern void liveareaFeedHide(void *feed, int mode);		/* 0x8119D7A2 */
extern int liveareaFeedRequestUpdate(void *feed, int mode);	/* 0x8119C964 */
extern void liveareaFeedOnScrollVisibility(void *feed);		/* 0x8119E6B6 */
extern void liveareaFeedOnNetChanged(void *feed);		/* 0x8119E788 */
extern void liveareaFeedOnRefresh(void *feed);			/* 0x8119E84E */
extern void liveareaLsdbUpdateTime(paf::common::SharedPtr<int> *out, void *ctx,
				   void *mapSp, void *vec, void *optSp,
				   int flags, int completeCb, int userData); /* 0x811C6568 */

class LiveAreaPage {
public:
	LiveAreaPage(int appId, const char *titleId, int flags, int mode); /* 0x8119B594 */
	~LiveAreaPage();					/* 0x8119B8EC */

	const char *GetTitleId();				/* 0x8119B990 */
	void UpdateLastLaunchTime();				/* 0x8119B9B2 */
	void ScrollToContent();					/* 0x8119BD0E */
	void ResetScroll(bool anim);				/* 0x8119BD32 */
	bool IsAtTop();						/* 0x8119BE16 */
	int CreateImageCache(int width, int height);		/* 0x8119BE9A */
	void EnsureFeed();					/* 0x8119BF26 */
	void ScrollIfPending();					/* 0x8119BF9C */
	void OnFocusComplete();					/* 0x8119C042 */
	void SubC064();						/* 0x8119C064 */
	int SubC06E();						/* 0x8119C06E */

	int m_appId;						/* +0x00 */
	paf::string m_titleId;					/* +0x04 */
	paf::ui::Widget *m_rootWidget;				/* +0x10 */
	void *m_cacheRef;					/* +0x14 */
	void *m_pageCtx;					/* +0x18 */
	paf::string m_templateName;				/* +0x1C */
	paf::ui::ListView *m_listView;				/* +0x28 */
	bool m_feedVisible;					/* +0x2C */
	bool m_isActive;					/* +0x2D */
	bool m_pendingScroll;					/* +0x2E */
	unsigned char m_pad2F;
	unsigned char m_inner[200];				/* +0x30 */
	void *m_feed;						/* +0xF8 */
};

static paf::list<LiveAreaPage *> s_pages;			/* 0x815670F4 */

static void liveareaPageOnDestroyCb(void *w, void *page);	/* 0x8119C5EC */
static void liveareaPageOnShowCb(int eventId, int handler, int ev, void *page); /* 0x8119C5FC */
static void liveareaPageOnHideCb(int eventId, int handler, int ev, void *page); /* 0x8119C654 */
static void liveareaPageOnEvent7Cb(int eventId, int handler, int ev, void *page); /* 0x8119C69C */
static void liveareaPageOnEvent6Cb(int eventId, void *ev, int flags, void *page); /* 0x8119C6C2 */
static void liveareaPageOnScrollEvent(int eventId, int handler, int ev, void *page); /* 0x8119C294 */

static void *getTopmenuIf(void)
{
	return *((void **)g_liveareaMgr + 1);
}

/* 0x8119B594 */
LiveAreaPage::LiveAreaPage(int appId, const char *titleId, int flags, int mode)
	: m_appId(appId),
	  m_titleId(titleId),
	  m_rootWidget(NULL),
	  m_cacheRef(NULL),
	  m_pageCtx(NULL),
	  m_templateName(),
	  m_listView(NULL),
	  m_feedVisible(false),
	  m_isActive(true),
	  m_pendingScroll(false),
	  m_pad2F(0),
	  m_feed(NULL)
{
	liveareaPageInnerCtor(m_inner, this, flags, mode);

	void *tm = getTopmenuIf();
	typedef paf::ui::Widget *(*GetWidgetFn)(int);
	typedef void *(*GetPtrFn)(int);
	m_rootWidget = (*(GetWidgetFn *)((char *)tm + 20))(appId);
	m_cacheRef = (*(GetPtrFn *)((char *)tm + 24))(appId);
	m_pageCtx = (*(GetPtrFn *)((char *)tm + 28))(appId);

	char buf[256];
	sce_paf_snprintf(buf, sizeof(buf), "%s%s", "livearea_template_", m_titleId.c_str());
	m_templateName = buf;

	paf::IDParam tmplId(0x4C202299U);
	paf::Plugin::TemplateOpenParam openParam;
	g_liveareaPlugin->TemplateOpen(m_rootWidget, tmplId, openParam);

	paf::IDParam childId(0x96091F06U);
	m_listView = (paf::ui::ListView *)m_rootWidget->FindChild(childId, 0);
	if (m_listView != NULL) {
		paf::math::v2 margin(0.0f, 192.0f);
		m_listView->SetGenerateMargin(margin);
		m_listView->SetBarFadeTime(0.0001f);
		paf::ui::ScrollViewScrollParam sp;
		((float *)&sp)[0] = 16.0f;
		((float *)&sp)[1] = 16.0f;
		((float *)&sp)[12] = 16.0f;
		m_listView->SetScrollParam(&sp);
	}

	s_pages.push_back(this);
	liveareaPageInnerInit(m_inner);

	typedef void (*RegCbFn)(int, void *, void *);
	(*(RegCbFn *)((char *)tm + 100))(appId, (void *)liveareaPageOnShowCb, this);
	(*(RegCbFn *)((char *)tm + 104))(appId, (void *)liveareaPageOnHideCb, this);
	SceShell_7FE4E732(7, (void *)liveareaPageOnEvent7Cb, this);
	SceShell_83D16CA8();

	if (liveareaMgrIsPendingLaunch(m_titleId.c_str()))
		m_pendingScroll = true;
	liveareaMgrClearPendingLaunch();
}

/* 0x8119B8EC */
LiveAreaPage::~LiveAreaPage()
{
	SceShell_D4C7D2B7(7, (void *)liveareaPageOnEvent7Cb, this);
	SceShell_E3D3A162(6, (void *)liveareaPageOnEvent6Cb, this);
	if (m_feed != NULL) {
		liveareaFeedDtor(m_feed);
		operator delete(m_feed);
	}
	s_pages.remove(this);
	liveareaPageInnerDtor(m_inner);
}

/* 0x8119B990 */
const char *LiveAreaPage::GetTitleId()
{
	int kind = *(int *)(m_inner + 60);
	if (kind == 1)
		return "NPXS10998";
	if (kind == 2)
		return "NPXS10999";
	return m_titleId.c_str();
}

/* 0x8119B9B2 */
void LiveAreaPage::UpdateLastLaunchTime()
{
	if (sce_paf_strncmp(m_titleId.c_str(), "NPXS10999", m_titleId.size()) == 0 ||
	    sce_paf_strncmp(m_titleId.c_str(), "NPXS10998", m_titleId.size()) == 0)
		return;

	paf::rtc::Tick tick = 0;
	char dateBuf[64];
	if (paf::rtc::GetCurrentTick(&tick) != 0)
		return;
	if (paf::rtc::FormatSQLiteDateTime(dateBuf, sizeof(dateBuf), &tick, 0x20) != 0)
		return;
}

/* 0x8119BD0E */
void LiveAreaPage::ScrollToContent()
{
	if (m_feed != NULL) {
		typedef void (*FocusCellFn)(paf::ui::ListView *, int, int, int, void *);
		FocusCellFn fn = (*(FocusCellFn **)(*(void **)m_listView))[628 / 4];
		fn(m_listView, 1, -1, 0, NULL);
	}
}

/* 0x8119BD32 */
void LiveAreaPage::ResetScroll(bool anim)
{
	if (m_feed != NULL)
		liveareaFeedHide(m_feed, 0);

	paf::Timer *timer = NULL;
	if (anim) {
		timer = new paf::Timer();
		timer->InitTimer(23.0f, 0, -1, 0, NULL);
	}
	typedef void (*FocusCellFn)(paf::ui::ListView *, int, int, int, paf::Timer *);
	FocusCellFn fn = (*(FocusCellFn **)(*(void **)m_listView))[628 / 4];
	fn(m_listView, 0, 0, 0, timer);
}

/* 0x8119BE16 */
bool LiveAreaPage::IsAtTop()
{
	typedef paf::ui::Widget *(*GetBodyFn)(paf::ui::ListView *);
	GetBodyFn fn = (*(GetBodyFn **)(*(void **)m_listView))[140 / 4];
	paf::ui::Widget *body = fn(m_listView);
	paf::math::v4 pos = body->GetPos(0, NULL, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	return fabsf(((const float *)&pos)[1] - 256.0f) <= 0.01f;
}

/* 0x8119BE9A */
int LiveAreaPage::CreateImageCache(int width, int height)
{
	if (m_cacheRef == NULL)
		return -1;
	unsigned char opt[84];
	sce_paf_memset(opt, 0, sizeof(opt));
	opt[0] = 1;
	opt[1] = 1;
	*(int *)(opt + 4) = 10;
	opt[8] = 1;
	*(short *)(opt + 20) = 420;
	*(int *)(opt + 56) = 1;
	*(int *)(opt + 64) = -1;
	typedef int (*CreateFn)(void *, int, int, void *, int, int);
	CreateFn fn = (*(CreateFn **)(*(void **)m_cacheRef))[2];
	return fn(m_cacheRef, width, height, opt, 0, 0);
}

/* 0x8119BF26 */
void LiveAreaPage::EnsureFeed()
{
	if (m_inner[66] != 0 && m_feed == NULL) {
		void *mem = operator new(128);
		if (mem != NULL) {
			void *ref = **(void ***)((char *)this + 64);
			m_feed = liveareaFeedCtor(mem, this, &ref);
		}
	}
}

/* 0x8119BF9C */
void LiveAreaPage::ScrollIfPending()
{
	if (m_pendingScroll && m_feed != NULL) {
		paf::Timer *timer = new paf::Timer();
		timer->InitTimer(23.0f, 0, -1, 0, NULL);
		typedef void (*FocusCellFn)(paf::ui::ListView *, int, int, int, paf::Timer *);
		FocusCellFn fn = (*(FocusCellFn **)(*(void **)m_listView))[628 / 4];
		fn(m_listView, 1, -1, 0, timer);
		m_pendingScroll = false;
	}
}

/* 0x8119C042 */
void LiveAreaPage::OnFocusComplete()
{
	m_listView->SetBarFadeTime(200.0f);
	m_listView->ResetBarShown();
	ScrollIfPending();
}

/* 0x8119C064 */
void LiveAreaPage::SubC064()
{
	liveareaPageInnerSubAB954(m_inner);
}

/* 0x8119C06E */
int LiveAreaPage::SubC06E()
{
	return liveareaPageInnerSubABA06(m_inner);
}

/* 0x8119C078 */
LiveAreaPage *liveareaPageFindByTemplate(const char *name)
{
	for (paf::list<LiveAreaPage *>::iterator it = s_pages.begin();
	     it != s_pages.end(); ++it) {
		if (sce_paf_strncmp(name, (*it)->m_templateName.c_str(), (*it)->m_templateName.size()) == 0)
			return *it;
	}
	return NULL;
}

/* 0x8119C0C6 */
LiveAreaPage *liveareaPageFindByAppId(int appId)
{
	for (paf::list<LiveAreaPage *>::iterator it = s_pages.begin();
	     it != s_pages.end(); ++it) {
		if ((*it)->m_appId == appId)
			return *it;
	}
	return NULL;
}

/* 0x8119C0FA */
LiveAreaPage *liveareaPageFindByListView(void *lv)
{
	for (paf::list<LiveAreaPage *>::iterator it = s_pages.begin();
	     it != s_pages.end(); ++it) {
		if ((*it)->m_listView == lv)
			return *it;
	}
	return NULL;
}

/* 0x8119C12E */
LiveAreaPage *liveareaPageFindByTitleId(const char *titleId)
{
	for (paf::list<LiveAreaPage *>::iterator it = s_pages.begin();
	     it != s_pages.end(); ++it) {
		if (sce_paf_strncmp(titleId, (*it)->m_titleId.c_str(), (*it)->m_titleId.size()) == 0)
			return *it;
	}
	return NULL;
}

/* 0x8119C17C */
void liveareaPageHideAllFeeds(void)
{
	for (paf::list<LiveAreaPage *>::iterator it = s_pages.begin();
	     it != s_pages.end(); ++it) {
		if (*it != NULL && (*it)->m_feed != NULL)
			liveareaFeedHide((*it)->m_feed, 1);
	}
}

/* 0x8119C1BC */
void liveareaPageOnEvent4(void)
{
	for (paf::list<LiveAreaPage *>::iterator it = s_pages.begin();
	     it != s_pages.end(); ++it) {
		(*it)->ResetScroll(false);
		paf::math::v2 margin(-100.0f, -100.0f);
		(*it)->m_listView->SetGenerateMargin(margin);
	}
}

/* 0x8119C234 */
void liveareaPageOnEvent5(int appId)
{
	for (paf::list<LiveAreaPage *>::iterator it = s_pages.begin();
	     it != s_pages.end(); ++it) {
		if ((*it)->m_appId == appId) {
			paf::math::v2 margin(0.0f, 192.0f);
			(*it)->m_listView->SetGenerateMargin(margin);
			break;
		}
	}
}

/* 0x8119C294 */
static void liveareaPageOnScrollEvent(int eventId, int handler, int ev, void *userdata)
{
	(void)eventId;
	(void)handler;
	(void)ev;
	LiveAreaPage *page = (LiveAreaPage *)userdata;
	liveareaPageInnerSubAB2DE(page->m_inner);
	if (page->m_feed != NULL) {
		paf::math::v2 target = page->m_listView->GetViewFocusTarget();
		bool prev = page->m_feedVisible;
		bool now = (((const float *)&target)[1] <= -768.0f);
		page->m_feedVisible = now;
		if (prev != now)
			liveareaFeedOnScrollVisibility(page->m_feed);
	}
}

/* 0x8119C312 */
void liveareaPageTickAll(bool changed)
{
	typedef int (*IsActiveFn)(void);
	int active = (*(IsActiveFn *)((char *)getTopmenuIf() + 196))();
	for (paf::list<LiveAreaPage *>::iterator it = s_pages.begin();
	     it != s_pages.end(); ++it) {
		LiveAreaPage *page = *it;
		if (page != NULL) {
			if (active != 0 && page->m_isActive)
				liveareaPageInnerSubAB056(page->m_inner);
			if (page->m_feed != NULL && changed)
				liveareaFeedOnNetChanged(page->m_feed);
		}
	}
}

/* 0x8119C37E */
int liveareaPageIfEntry5(const char *titleId, int state, int flags)
{
	g_shellMutex.Lock();
	for (paf::list<LiveAreaPage *>::iterator it = s_pages.begin();
	     it != s_pages.end(); ++it) {
		LiveAreaPage *page = *it;
		if (sce_paf_strncmp(titleId, page->m_titleId.c_str(), page->m_titleId.size()) == 0) {
			int r = liveareaPageInnerSubAACC2(page->m_inner, state, flags);
			g_shellMutex.Unlock();
			return r;
		}
	}
	g_shellMutex.Unlock();
	return -1;
}

/* 0x8119C406 */
void liveareaPageIfEntry7(void)
{
	typedef int (*GetAppInfoFn)(paf::common::SharedPtr<int> *, const char *);
	GetAppInfoFn getInfo = *(GetAppInfoFn *)((char *)getTopmenuIf() + 276);
	for (paf::list<LiveAreaPage *>::iterator it = s_pages.begin();
	     it != s_pages.end(); ++it) {
		LiveAreaPage *page = *it;
		if (page != NULL) {
			paf::common::SharedPtr<int> sp;
			if (getInfo(&sp, page->m_titleId.c_str()) != 0)
				liveareaPageInnerSubAAD60(page->m_inner, -1, SceLsdb_C070E42E(&sp));
			if (page->m_feed != NULL)
				liveareaFeedOnRefresh(page->m_feed);
		}
	}
}

/* 0x8119C4F2 */
void liveareaPageIfEntry8(void)
{
	typedef int (*GetAppInfoFn)(paf::common::SharedPtr<int> *, const char *);
	GetAppInfoFn getInfo = *(GetAppInfoFn *)((char *)getTopmenuIf() + 276);
	g_shellMutex.Lock();
	for (paf::list<LiveAreaPage *>::iterator it = s_pages.begin();
	     it != s_pages.end(); ++it) {
		LiveAreaPage *page = *it;
		if (page != NULL && *(int *)(page->m_inner + 4) == 3) {
			paf::common::SharedPtr<int> sp;
			if (getInfo(&sp, page->m_titleId.c_str()) != 0)
				liveareaPageInnerSubAAD60(page->m_inner, -1, SceLsdb_C070E42E(&sp));
		}
	}
	g_shellMutex.Unlock();
}

/* 0x8119C5EC */
static void liveareaPageOnDestroyCb(void *w, void *userdata)
{
	(void)w;
	LiveAreaPage *page = (LiveAreaPage *)userdata;
	if (page != NULL)
		delete page;
}

/* 0x8119C5FC */
static void liveareaPageOnShowCb(int eventId, int handler, int ev, void *userdata)
{
	(void)eventId;
	(void)handler;
	(void)ev;
	LiveAreaPage *page = (LiveAreaPage *)userdata;
	paf::math::v2 margin(0.0f, 192.0f);
	page->m_listView->SetGenerateMargin(margin);
	page->m_isActive = true;
	liveareaPageInnerSubAAFB8(page->m_inner);
	liveareaPageInnerSubAB944(page->m_inner);
	page->ScrollIfPending();
}

/* 0x8119C654 */
static void liveareaPageOnHideCb(int eventId, int handler, int ev, void *userdata)
{
	(void)eventId;
	(void)handler;
	(void)ev;
	LiveAreaPage *page = (LiveAreaPage *)userdata;
	page->ResetScroll(false);
	paf::math::v2 margin(-100.0f, -100.0f);
	page->m_listView->SetGenerateMargin(margin);
	page->m_isActive = false;
}

/* 0x8119C69C */
static void liveareaPageOnEvent7Cb(int eventId, int handler, int ev, void *userdata)
{
	(void)eventId;
	(void)handler;
	(void)ev;
	typedef int (*CheckFn)(void);
	if ((*(CheckFn *)((char *)getTopmenuIf() + 232))() == 0 && userdata != NULL)
		((LiveAreaPage *)userdata)->ResetScroll(false);
}

/* 0x8119C6C2 */
static void liveareaPageOnEvent6Cb(int eventId, void *ev, int flags, void *userdata)
{
	(void)eventId;
	(void)flags;
	LiveAreaPage *page = (LiveAreaPage *)userdata;
	if (page != NULL) {
		const char *tid = *(const char **)((char *)ev + 8);
		if (sce_paf_strncmp(tid, page->m_titleId.c_str(), sce_paf_strlen(tid)) == 0) {
			if (page->m_feed != NULL)
				liveareaFeedRequestUpdate(page->m_feed, 0);
		}
	}
}
