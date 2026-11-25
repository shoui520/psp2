/*
 * livearea_page.cpp: livearea_plugin per-app LiveArea page controller
 * (0x8119B594-0x8119C7A4, sti 0x8119C6EE).
 */
#include <math.h>
#include <std/stdio.h>
#include <std/string.h>
#include <widget/w_scroll_view_base.h>
#include <rtc/rtc.h>
#include <timer/timer.h>
#include "livearea_internal.h"

extern "C" {
int SceShell_7FE4E732(int id, void *callback, void *arg);
int SceShell_D4C7D2B7(int id, void *callback, void *arg);
int SceShell_E3D3A162(int id, void *callback, void *arg);
void SceShell_83D16CA8(void);
}

static paf::list<LiveAreaPage *> s_pages;			/* 0x815670F4 */

static void liveareaPageOnDestroyCb(paf::ui::Widget *widget, void *page); /* 0x8119C5EC */
static void liveareaPageOnShowCb(int eventId, paf::ui::Handler *handler,
				 paf::ui::Event *event, void *page); /* 0x8119C5FC */
static void liveareaPageOnHideCb(int eventId, paf::ui::Handler *handler,
				 paf::ui::Event *event, void *page); /* 0x8119C654 */
static void liveareaPageOnHomeReturnCb(int eventId, paf::ui::Handler *handler,
				       paf::ui::Event *event, void *page); /* 0x8119C69C */
static void liveareaPageOnAppUpdatedCb(int eventId, ShellAppEventPayload *payload,
				       int flags, void *page); /* 0x8119C6C2 */
static void liveareaPageOnScrollEvent(int eventId, paf::ui::Handler *handler,
				      paf::ui::Event *event, void *page); /* 0x8119C294 */

/* 0x8119B594 */
LiveAreaPage::LiveAreaPage(int pageKey, const char *titleId, const char *contentDir,
			   const LiveAreaPageInitInfo *info)
	: m_pageKey(pageKey),
	  m_titleId(titleId),
	  m_rootWidget(NULL),
	  m_cacheManager(NULL),
	  m_surfacePool(NULL),
	  m_templateName(),
	  m_fileList(NULL),
	  m_feedVisible(false),
	  m_isActive(true),
	  m_pendingScroll(false),
	  statePadding(0),
	  m_inner(this, contentDir, info),
	  m_feed(NULL)
{
	LiveAreaTopmenuInterface *topmenuIf = g_liveareaMgr->m_topmenuIf;
	m_rootWidget = topmenuIf->getPageRootWidget(pageKey);
	m_cacheManager = topmenuIf->getPageCacheManager(pageKey);
	m_surfacePool = topmenuIf->getPageSurfacePool(pageKey);

	char nameBuf[256];
	sce_paf_snprintf(nameBuf, sizeof(nameBuf), "%s%s", "livearea_template_", m_titleId.c_str());
	m_templateName = nameBuf;

	paf::IDParam tmplId(0x4C202299U);
	paf::Plugin::TemplateOpenParam openParam;
	g_liveareaPlugin->TemplateOpen(m_rootWidget, tmplId, openParam);

	paf::IDParam listId(0x96091F06U);
	m_fileList = (paf::ui::FileList *)m_rootWidget->FindChild(listId, 0);
	if (m_fileList != NULL) {
		paf::math::v2 margin(0.0f, 192.0f);
		m_fileList->SetGenerateMargin(margin);
		m_fileList->SetBarFadeTime(0.0001f);
		paf::ui::ScrollViewScrollParam scrollParam;
		scrollParam.m_value[0] = 16.0f;
		scrollParam.m_value[1] = 16.0f;
		scrollParam.m_value[12] = 16.0f;
		m_fileList->SetScrollParam(&scrollParam);
	}

	s_pages.push_back(this);
	m_inner.Init();

	topmenuIf->addPageShowListener(pageKey, (void *)liveareaPageOnShowCb, this);
	topmenuIf->addPageHideListener(pageKey, (void *)liveareaPageOnHideCb, this);
	SceShell_7FE4E732(7, (void *)liveareaPageOnHomeReturnCb, this);
	SceShell_83D16CA8();

	if (liveareaMgrIsPendingLaunch(m_titleId.c_str()))
		m_pendingScroll = true;
	liveareaMgrClearPendingLaunch();
}

/* 0x8119B8EC */
LiveAreaPage::~LiveAreaPage()
{
	SceShell_D4C7D2B7(7, (void *)liveareaPageOnHomeReturnCb, this);
	SceShell_E3D3A162(6, (void *)liveareaPageOnAppUpdatedCb, this);
	if (m_feed != NULL) {
		delete m_feed;
		m_feed = NULL;
	}
	s_pages.remove(this);
}

/* 0x8119B990 */
const char *LiveAreaPage::GetTitleId()
{
	if (m_inner.m_kind == 1)
		return "NPXS10998";
	if (m_inner.m_kind == 2)
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
	if (m_feed != NULL)
		m_fileList->SetFocus(1, -1, paf::ui::ListView::FOCUS_ALIGN_TYPE_HEAD, NULL);
}

/* 0x8119BD32 */
void LiveAreaPage::ResetScroll(bool animate)
{
	if (m_feed != NULL)
		m_feed->HideDialog(false);

	paf::Timer *scrollTimer = NULL;
	if (animate) {
		scrollTimer = new paf::Timer();
		scrollTimer->InitTimer(23.0f, 0, -1, 0, NULL);
	}
	m_fileList->SetFocus(0, 0, paf::ui::ListView::FOCUS_ALIGN_TYPE_HEAD, scrollTimer);
}

/* 0x8119BE16 */
bool LiveAreaPage::IsAtTop()
{
	paf::ui::Widget *body = m_fileList->GetInternalWidget();
	paf::math::v4 pos = body->GetPos(0, NULL, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	return fabsf(pos.extract_y() - 256.0f) <= 0.01f;
}

/* 0x8119BE9A */
int LiveAreaPage::CreateImageCache(const paf::string &path,
				   paf::intrusive_ptr<paf::cache::CacheImageInterface> &outRef)
{
	if (m_cacheManager == NULL)
		return -1;
	paf::cache::CacheImage::Option option;
	sce_paf_memset(&option, 0, sizeof(option));
	option.auto_delete = true;
	option.retry = true;
	option.retry_count = 10;
	option.reserved = true;
	option.max_size_kb = 420;
	option.resize_type = (paf::image::ImageResizeType)1;
	option.image_order_param = -1;
	return m_cacheManager->CreateCache(path, outRef, &option, NULL, NULL);
}

/* 0x8119BF26 */
void LiveAreaPage::EnsureFeed()
{
	if (m_inner.m_hasFeed && m_feed == NULL)
		m_feed = new LiveAreaFeed(this, &m_inner.m_gateIconCache);
}

/* 0x8119BF9C */
void LiveAreaPage::ScrollIfPending()
{
	if (m_pendingScroll && m_feed != NULL) {
		paf::Timer *scrollTimer = new paf::Timer();
		scrollTimer->InitTimer(23.0f, 0, -1, 0, NULL);
		m_fileList->SetFocus(1, -1, paf::ui::ListView::FOCUS_ALIGN_TYPE_HEAD, scrollTimer);
		m_pendingScroll = false;
	}
}

/* 0x8119C042 */
void LiveAreaPage::OnFocusComplete()
{
	m_fileList->SetBarFadeTime(200.0f);
	m_fileList->ResetBarShown();
	ScrollIfPending();
}

/* 0x8119C064 */
void LiveAreaPage::UpdateInitInfo(const LiveAreaPageInitInfo *info)
{
	m_inner.UpdateInitInfo(info);
}

/* 0x8119C06E */
int LiveAreaPage::HasPatchIcon()
{
	return m_inner.HasPatchIcon();
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
LiveAreaPage *liveareaPageFindByAppId(int pageKey)
{
	for (paf::list<LiveAreaPage *>::iterator it = s_pages.begin();
	     it != s_pages.end(); ++it) {
		if ((*it)->m_pageKey == pageKey)
			return *it;
	}
	return NULL;
}

/* 0x8119C0FA */
LiveAreaPage *liveareaPageFindByListView(paf::ui::ListView *listView)
{
	for (paf::list<LiveAreaPage *>::iterator it = s_pages.begin();
	     it != s_pages.end(); ++it) {
		if ((*it)->m_fileList == listView)
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
			(*it)->m_feed->HideDialog(true);
	}
}

/* 0x8119C1BC */
void liveareaPageOnShellPause(void)
{
	for (paf::list<LiveAreaPage *>::iterator it = s_pages.begin();
	     it != s_pages.end(); ++it) {
		(*it)->ResetScroll(false);
		paf::math::v2 margin(-100.0f, -100.0f);
		(*it)->m_fileList->SetGenerateMargin(margin);
	}
}

/* 0x8119C234 */
void liveareaPageOnShellResume(int pageKey)
{
	for (paf::list<LiveAreaPage *>::iterator it = s_pages.begin();
	     it != s_pages.end(); ++it) {
		if ((*it)->m_pageKey == pageKey) {
			paf::math::v2 margin(0.0f, 192.0f);
			(*it)->m_fileList->SetGenerateMargin(margin);
			break;
		}
	}
}

/* 0x8119C294 */
static void liveareaPageOnScrollEvent(int eventId, paf::ui::Handler *handler,
				      paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)handler;
	(void)event;
	LiveAreaPage *page = (LiveAreaPage *)userdata;
	page->m_inner.OnScroll();
	if (page->m_feed != NULL) {
		paf::math::v2 target = page->m_fileList->GetViewFocusTarget();
		bool prevVisible = page->m_feedVisible;
		bool nowVisible = (target.extract_y() <= -768.0f);
		page->m_feedVisible = nowVisible;
		if (prevVisible != nowVisible)
			page->m_feed->SetVisible(nowVisible);
	}
}

/* 0x8119C312 */
void liveareaPageTickAll(bool changed)
{
	int isForeground = g_liveareaMgr->m_topmenuIf->isTopmenuForeground();
	for (paf::list<LiveAreaPage *>::iterator it = s_pages.begin();
	     it != s_pages.end(); ++it) {
		LiveAreaPage *page = *it;
		if (page != NULL) {
			if (isForeground != 0 && page->m_isActive)
				page->m_inner.Tick();
			if (page->m_feed != NULL && changed)
				page->m_feed->OnNetChanged();
		}
	}
}

/* 0x8119C37E */
int liveareaPageGetFrameTimestamp(const char *titleId, const char *frameId,
				  SceUInt64 *outTimestamp)
{
	g_shellMutex.Lock();
	for (paf::list<LiveAreaPage *>::iterator it = s_pages.begin();
	     it != s_pages.end(); ++it) {
		LiveAreaPage *page = *it;
		if (sce_paf_strncmp(titleId, page->m_titleId.c_str(), page->m_titleId.size()) == 0) {
			int ret = page->m_inner.GetFrameTimestamp(frameId, outTimestamp);
			g_shellMutex.Unlock();
			return ret;
		}
	}
	g_shellMutex.Unlock();
	return -1;
}

/* 0x8119C406 */
void liveareaPageReloadAll(void)
{
	LiveAreaTopmenuInterface *topmenuIf = g_liveareaMgr->m_topmenuIf;
	for (paf::list<LiveAreaPage *>::iterator it = s_pages.begin();
	     it != s_pages.end(); ++it) {
		LiveAreaPage *page = *it;
		if (page != NULL) {
			LsdbAppInfoPtr appInfo;
			if (topmenuIf->getAppInfo(&appInfo, page->m_titleId.c_str()) != 0)
				page->m_inner.QueueReload(-1, SceLsdb_C070E42E(&appInfo));
			if (page->m_feed != NULL)
				page->m_feed->OnRefresh();
		}
	}
}

/* 0x8119C4F2 */
void liveareaPageReloadPocketStation(void)
{
	LiveAreaTopmenuInterface *topmenuIf = g_liveareaMgr->m_topmenuIf;
	g_shellMutex.Lock();
	for (paf::list<LiveAreaPage *>::iterator it = s_pages.begin();
	     it != s_pages.end(); ++it) {
		LiveAreaPage *page = *it;
		if (page != NULL && page->m_inner.m_style == 3) {
			LsdbAppInfoPtr appInfo;
			if (topmenuIf->getAppInfo(&appInfo, page->m_titleId.c_str()) != 0)
				page->m_inner.QueueReload(-1, SceLsdb_C070E42E(&appInfo));
		}
	}
	g_shellMutex.Unlock();
}

/* 0x8119C5EC */
static void liveareaPageOnDestroyCb(paf::ui::Widget *widget, void *userdata)
{
	(void)widget;
	LiveAreaPage *page = (LiveAreaPage *)userdata;
	if (page != NULL)
		delete page;
}

/* 0x8119C5FC */
static void liveareaPageOnShowCb(int eventId, paf::ui::Handler *handler,
				 paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)handler;
	(void)event;
	LiveAreaPage *page = (LiveAreaPage *)userdata;
	paf::math::v2 margin(0.0f, 192.0f);
	page->m_fileList->SetGenerateMargin(margin);
	page->m_isActive = true;
	page->m_inner.ResetGateFocus();
	page->m_inner.ClearGateInitFlag();
	page->ScrollIfPending();
}

/* 0x8119C654 */
static void liveareaPageOnHideCb(int eventId, paf::ui::Handler *handler,
				 paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)handler;
	(void)event;
	LiveAreaPage *page = (LiveAreaPage *)userdata;
	page->ResetScroll(false);
	paf::math::v2 margin(-100.0f, -100.0f);
	page->m_fileList->SetGenerateMargin(margin);
	page->m_isActive = false;
}

/* 0x8119C69C */
static void liveareaPageOnHomeReturnCb(int eventId, paf::ui::Handler *handler,
				       paf::ui::Event *event, void *userdata)
{
	(void)eventId;
	(void)handler;
	(void)event;
	if (g_liveareaMgr->m_topmenuIf->isHomePageSelected() == 0 && userdata != NULL)
		((LiveAreaPage *)userdata)->ResetScroll(false);
}

/* 0x8119C6C2 */
static void liveareaPageOnAppUpdatedCb(int eventId, ShellAppEventPayload *payload,
				       int flags, void *userdata)
{
	(void)eventId;
	(void)flags;
	LiveAreaPage *page = (LiveAreaPage *)userdata;
	if (page != NULL) {
		const char *titleId = payload->titleId;
		if (sce_paf_strncmp(titleId, page->m_titleId.c_str(), sce_paf_strlen(titleId)) == 0) {
			if (page->m_feed != NULL)
				page->m_feed->RequestUpdate(0);
		}
	}
}
