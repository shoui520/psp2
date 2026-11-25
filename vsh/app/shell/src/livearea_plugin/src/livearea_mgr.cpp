/*
 * livearea_mgr.cpp: livearea_plugin main manager & interface callbacks
 * (0x8119A698-0x8119B594, sti 0x8119B522).
 */
#include <std/string.h>
#include <common/main_thread_call_list.h>
#include <system/system.h>
#include <appmgr.h>
#include "livearea_internal.h"

extern "C" {
int sceAppMgrLaunchAppByName(unsigned int flags, const char *name, const char *param);
int SceShell_7FE4E732(int id, void *callback, void *arg);
int SceShell_D4C7D2B7(int id, void *callback, void *arg);
int SceLsdb_B51740A6(int *cookie, paf::common::SharedPtr<LiTemplate> *outTemplate,
		     paf::string *titleId, SceUInt8 isDefault);
int SceLsdb_DA5DC77C(int *cookie, paf::vector<paf::string> *openTitles, paf::string *outEvictTitle);
int SceLsdb_C87397C0(paf::string *titleId);
int SceLsdb_C875C57A(paf::string *titleId, const char *contentPath);
}

/* Interface table callbacks */
static int liveareaIfCreatePage(int pageKey, const char *titleId, const char *contentDir,
				const LiveAreaPageInitInfo *info);		/* 0x8119AD92 */
static void liveareaIfSetGateSurface(int pageKey, paf::graph::Surface **surface); /* 0x8119ADBA */
static void liveareaIfClearGateSurface(int pageKey);				/* 0x8119ADD0 */
static int liveareaIfRequestReload(int pageKey, const LiveAreaReloadRequest *req); /* 0x8119ADE2 */
static void liveareaIfResetPageFrames(int pageKey);				/* 0x8119ADFC */
static void liveareaIfResetPageScroll(int pageKey);				/* 0x8119AE90 */
static int liveareaIfIsPageAtTop(int pageKey);					/* 0x8119AEA2 */
static int liveareaIfIsIntroDone(int pageKey);					/* 0x8119AEB6 */
static int liveareaIfGetGateNormPos(int pageKey, float *outX, float *outY);	/* 0x8119AECA */
static paf::ui::Widget *liveareaIfOpenPageOverlay(paf::ui::Widget *parent);	/* 0x8119AEF4 */
static int liveareaIfEnqueueActivityRevert(int unusedAppId, int flags, int mode, int option); /* 0x8119B0B0 */
static int liveareaIfOpenOrLaunchApp(const char *titleId, const char *statusParam); /* 0x8119B110 */
static void liveareaIfOnActivePageChanged(int pageKey);				/* 0x8119B18E */
static void liveareaIfUpdatePageInfo(int pageKey, const LiveAreaPageInitInfo *info); /* 0x8119B1E0 */
static void liveareaIfRefreshPage(int pageKey);					/* 0x8119B1F4 */
static int liveareaIfHasPatchIcon(int pageKey);					/* 0x8119B20E */
static int liveareaIfOnAppDeleted(paf::string *titleId);			/* 0x8119B4C8 */
static void liveareaMgrOnUpdaterDone(int status, int pageKey);			/* 0x8119B4EC */
static void liveareaMgrOnNetEvent(int event);					/* 0x8119B506 */
static void liveareaMgrOnShellPause(void);					/* 0x8119B510 */
static void liveareaMgrOnShellResume(int eventId, int eventArg, int pageKey);	/* 0x8119B518 */
static void liveareaMgrMainThreadTick(void *userdata);				/* 0x8119A8EA */

LiveAreaMgr *g_liveareaMgr;							/* 0x815670A8 */

/* 0x8119A698 */
LiveAreaMgr::LiveAreaMgr(paf::Plugin *plugin)
	: m_plugin(plugin),
	  m_topmenuIf((LiveAreaTopmenuInterface *)shellPluginGetInterface("topmenu_plugin", 1)),
	  m_gameIf((LiveAreaGameInterface *)shellPluginGetInterface("game_plugin", 1)),
	  m_applauncherIf((LiveAreaApplauncherInterface *)shellPluginGetInterface("applauncher_plugin", 1)),
	  m_netState(0),
	  m_signedInState(0),
	  m_pollTimer(0.0f),
	  m_pendingLaunchTitle(),
	  m_currentAppId(-1),
	  m_lsdbCookie(0),
	  m_netCbId(-1)
{
	m_updater.m_impl = NULL;
	liveareaUpdaterCtor(&m_updater);
	g_liveareaMgr = this;

	LiveAreaPluginInterface *iface = g_liveareaInterface;
	iface->createPage = liveareaIfCreatePage;
	iface->setGateSurface = liveareaIfSetGateSurface;
	iface->clearGateSurface = liveareaIfClearGateSurface;
	iface->requestReload = liveareaIfRequestReload;
	iface->resetPageScroll = liveareaIfResetPageScroll;
	iface->installLiveAreaContent = liveareaIfInstallContent;
	iface->onAppDeleted = liveareaIfOnAppDeleted;
	iface->isPageAtTop = liveareaIfIsPageAtTop;
	iface->hasPatchIcon = liveareaIfHasPatchIcon;
	iface->resetPageFrames = liveareaIfResetPageFrames;
	iface->isIntroDone = liveareaIfIsIntroDone;
	iface->getGateNormPos = liveareaIfGetGateNormPos;
	iface->getFrameTimestamp = liveareaPageGetFrameTimestamp;
	iface->reloadAllPages = liveareaPageReloadAll;
	iface->reloadPocketStationPages = liveareaPageReloadPocketStation;
	iface->openPageOverlay = liveareaIfOpenPageOverlay;
	iface->enqueueActivityRevert = liveareaIfEnqueueActivityRevert;
	iface->openOrLaunchApp = liveareaIfOpenOrLaunchApp;
	iface->onActivePageChanged = liveareaIfOnActivePageChanged;
	iface->updatePageInfo = liveareaIfUpdatePageInfo;
	iface->refreshPage = liveareaIfRefreshPage;

	liveareaUpdaterStart(&m_updater);
	m_netCbId = npManagerRegisterStateCallback(liveareaMgrOnNetEvent, NULL);
	SceShell_7FE4E732(4, (void *)liveareaMgrOnShellPause, NULL);
	SceShell_7FE4E732(5, (void *)liveareaMgrOnShellResume, NULL);
}

/* 0x8119A83C */
LiveAreaMgr *liveareaMgrCreate(paf::Plugin *plugin)
{
	return new LiveAreaMgr(plugin);
}

/* 0x8119A852 */
void liveareaMgrDestroy(void)
{
	LiveAreaMgr *mgr = g_liveareaMgr;
	if (mgr != NULL) {
		SceShell_D4C7D2B7(4, (void *)liveareaMgrOnShellResume, NULL);
		SceShell_D4C7D2B7(4, (void *)liveareaMgrOnShellPause, NULL);
		npManagerUnregisterStateCallback(mgr->m_netCbId);
		liveareaUpdaterStop(&mgr->m_updater);
		g_liveareaMgr = NULL;
		liveareaUpdaterDtor(&mgr->m_updater);
		delete mgr;
	}
}

/* 0x8119A8C6 */
void liveareaMgrStart(void)
{
	paf::common::MainThreadCallList::Register(liveareaMgrMainThreadTick, NULL);
}

/* 0x8119A8D8 */
void liveareaMgrStop(void)
{
	paf::common::MainThreadCallList::Unregister(liveareaMgrMainThreadTick, NULL);
}

/* 0x8119A8EA */
static void liveareaMgrMainThreadTick(void *userdata)
{
	(void)userdata;
	LiveAreaMgr *mgr = g_liveareaMgr;
	int prevNet = mgr->m_netState;
	int curNet = mgr->m_topmenuIf->getNetworkState();
	mgr->m_netState = curNet;

	bool changed = (prevNet != curNet);
	bool poll = changed;
	mgr->m_pollTimer += (1.0f / 60.0f);
	if (mgr->m_pollTimer > 60.0f)
		poll = true;

	if (poll) {
		mgr->m_pollTimer = 0.0f;
		int signedIn = 0;
		if (npManagerGetSignInState(&signedIn) == 0) {
			if (mgr->m_signedInState != signedIn) {
				changed = true;
				mgr->m_signedInState = signedIn;
			}
		}
	}
	liveareaPageTickAll(changed);
}

/* 0x8119A99C */
int liveareaMgrGetCachedTemplate(paf::common::SharedPtr<LiTemplate> *outTemplate,
				 paf::string *titleId, SceUInt8 isDefault)
{
	return SceLsdb_B51740A6(&g_liveareaMgr->m_lsdbCookie, outTemplate, titleId, isDefault);
}

/* 0x8119A9BA */
void liveareaMgrUpdatePage(LiveAreaPageInner *pageInner, int mode)
{
	if (pageInner->m_xmlDoc.get() == NULL || pageInner->GetReloadCount() > 0)
		return;

	const char *title = pageInner->m_page->GetTitleId();
	SceUInt64 modifiedDate = pageInner->m_xmlDoc->m_modifiedDate;
	void *callback = NULL;
	int pageKey = 0;
	bool checkDolceClean = false;
	bool connectRequested = false;
	SceUInt64 taskId = 0;

	if (mode == 1) {
		callback = (void *)liveareaMgrOnUpdaterDone;
		pageKey = pageInner->m_page->m_pageKey;
	}

	if (pageInner->m_hasInfo) {
		bool shouldUpdateContent = true;
		if (mode != 0) {
			if (paf::system::Model::GetType() == 1)
				checkDolceClean = true;
		} else if (pageInner->m_kind != 0) {
			shouldUpdateContent = false;
		}

		if (shouldUpdateContent) {
			if (mode == 1) {
				connectRequested = true;
				netCtlIntRequestConnect();
			}
			int ret = liveareaUpdaterRequestContentUpdate(&g_liveareaMgr->m_updater,
								      &taskId, title, &modifiedDate,
								      mode, callback, pageKey);
			if (ret == 0 && mode == 1)
				pageInner->StartReloadSpin();
			if (checkDolceClean) {
				if (!connectRequested) {
					connectRequested = true;
					netCtlIntRequestConnect();
				}
				if (liveareaUpdaterRequestClean(&g_liveareaMgr->m_updater,
								&taskId, title, 1,
								callback, pageKey) == 0)
					pageInner->StartReloadSpin();
			}
		}
	}

	if (pageInner->m_hasExt) {
		if (!connectRequested && mode == 1) {
			connectRequested = true;
			netCtlIntRequestConnect();
		}
		if (liveareaUpdaterRequestPatchCheck(&g_liveareaMgr->m_updater, &taskId, title,
						     mode, pageInner->m_upgradeMode,
						     callback, pageKey) == 0 && mode == 1)
			pageInner->StartReloadSpin();
	}

	if (pageInner->m_hasFeed && mode == 1) {
		if (!connectRequested)
			netCtlIntRequestConnect();
		LiveAreaFeed *feed = pageInner->m_page->m_feed;
		if (feed != NULL && feed->RequestUpdate(1) == 0)
			pageInner->StartReloadSpin();
	}
}

/* 0x8119AB34 */
void liveareaReplaceOnlineId(paf::string &out, const paf::string &in)
{
	static paf::string s_tag("<SCE_PSN_ONLINEID>");
	out = in;
	size_t pos = out.find(s_tag);
	if (pos != paf::string::npos) {
		char onlineId[20];
		if (npManagerGetOnlineId(onlineId) < 0)
			out.erase(pos, s_tag.size());
		else
			out.replace(pos, s_tag.size(), onlineId);
	}
}

/* 0x8119AC7A */
void liveareaMgrForcePoll(void)
{
	g_liveareaMgr->m_pollTimer += 60.0f;
}

/* 0x8119AC9C */
void liveareaMgrNotifyPage(const char *titleId, int reloadMask)
{
	LiveAreaPage *page = liveareaPageFindByTitleId(titleId);
	if (page != NULL) {
		LiveAreaReloadRequest req;
		req.titleId = titleId;
		req.contentDir = NULL;
		req.reloadMask = reloadMask;
		page->m_inner.SetDirtyFlag();
		page->m_inner.QueueReloadFromReq(&req);
	}
}

/* 0x8119ACE4 */
void liveareaMgrClearPendingLaunch(void)
{
	g_liveareaMgr->m_pendingLaunchTitle.clear();
}

/* 0x8119ACFC */
bool liveareaMgrIsPendingLaunch(const char *titleId)
{
	if (g_liveareaMgr->m_pendingLaunchTitle.empty())
		return false;
	paf::string candidate(titleId);
	return candidate == g_liveareaMgr->m_pendingLaunchTitle;
}

/* 0x8119AD92 */
static int liveareaIfCreatePage(int pageKey, const char *titleId, const char *contentDir,
				const LiveAreaPageInitInfo *info)
{
	new LiveAreaPage(pageKey, titleId, contentDir, info);
	return 1;
}

/* 0x8119ADBA */
static void liveareaIfSetGateSurface(int pageKey, paf::graph::Surface **surface)
{
	LiveAreaPage *page = liveareaPageFindByAppId(pageKey);
	if (page != NULL)
		page->m_inner.SetGateSurface(surface);
}

/* 0x8119ADD0 */
static void liveareaIfClearGateSurface(int pageKey)
{
	LiveAreaPage *page = liveareaPageFindByAppId(pageKey);
	if (page != NULL)
		page->m_inner.ClearGateSurface();
}

/* 0x8119ADE2 */
static int liveareaIfRequestReload(int pageKey, const LiveAreaReloadRequest *req)
{
	LiveAreaPage *page = liveareaPageFindByAppId(pageKey);
	if (page != NULL)
		return page->m_inner.QueueReloadFromReq(req);
	return 0;
}

/* 0x8119ADFC */
static void liveareaIfResetPageFrames(int pageKey)
{
	LiveAreaPage *page = liveareaPageFindByAppId(pageKey);
	if (page != NULL) {
		int resetMask = page->m_inner.GetResetMask();
		if (resetMask != 0)
			liveareaEnqueueContentResetJob(&page->m_inner, resetMask);
	}
}

/* 0x8119AE90 */
static void liveareaIfResetPageScroll(int pageKey)
{
	LiveAreaPage *page = liveareaPageFindByAppId(pageKey);
	if (page != NULL)
		page->ResetScroll(true);
}

/* 0x8119AEA2 */
static int liveareaIfIsPageAtTop(int pageKey)
{
	LiveAreaPage *page = liveareaPageFindByAppId(pageKey);
	if (page != NULL)
		return page->IsAtTop() ? 1 : 0;
	return 1;
}

/* 0x8119AEB6 */
static int liveareaIfIsIntroDone(int pageKey)
{
	LiveAreaPage *page = liveareaPageFindByAppId(pageKey);
	if (page != NULL)
		return page->m_inner.m_introDone ? 1 : 0;
	return 1;
}

/* 0x8119AECA */
static int liveareaIfGetGateNormPos(int pageKey, float *outX, float *outY)
{
	LiveAreaPage *page = liveareaPageFindByAppId(pageKey);
	if (page != NULL)
		return page->m_inner.GetGateNormPos(outX, outY);
	*outX = 0.0f;
	*outY = 0.105f;
	return 0;
}

/* 0x8119AEF4 */
static paf::ui::Widget *liveareaIfOpenPageOverlay(paf::ui::Widget *parent)
{
	paf::IDParam tmplId(0xF88DEE04U);
	paf::Plugin::TemplateOpenParam openParam;
	g_liveareaPlugin->TemplateOpen(parent, tmplId, openParam);

	paf::IDParam childId(0x5721D77BU);
	paf::ui::Widget *child = parent->FindChild(childId, 0);
	if (child != NULL) {
		paf::IDParam texId(0xE766F27FU);
		paf::intrusive_ptr<paf::graph::Surface> surf = g_liveareaPlugin->GetTexture(texId);
		child->SetTexture(surf);

		paf::math::v4 pos(0.0f, 384.0f, 0.0f, 0.0f);
		child->SetPos(pos, NULL, 0, 0x10000, 0.0f, NULL, NULL);
	}
	return child;
}

/* 0x8119B0B0 */
static int liveareaIfEnqueueActivityRevert(int titleIdArg, int flags, int mode, int option)
{
	if (g_liveareaMgr == NULL)
		return -1;
	SceUInt64 taskId = 0;
	const char *activityTitle = SceActivityDb_F138D4E5((const char *)titleIdArg);
	return liveareaUpdaterRequestActivityRevert(&g_liveareaMgr->m_updater, &taskId,
						    (int)activityTitle, flags, mode, option);
}

/* 0x8119B110 */
static int liveareaIfOpenOrLaunchApp(const char *titleId, const char *statusParam)
{
	if (g_liveareaMgr == NULL)
		return (int)titleId;
	liveareaPageHideAllFeeds();
	if (statusParam != NULL) {
		liveareaLaunchWithArg(titleId, statusParam);
		return 0;
	}

	LiveAreaPage *page = liveareaPageFindByTitleId(titleId);
	if (page != NULL) {
		if (page->m_isActive && page->m_feed != NULL) {
			page->ScrollToContent();
			return 0;
		}
		page->m_pendingScroll = true;
	} else if (g_liveareaMgr != NULL) {
		g_liveareaMgr->m_pendingLaunchTitle = titleId;
	}
	return sceAppMgrLaunchAppByName(0, titleId, NULL);
}

/* 0x8119B18E */
static void liveareaIfOnActivePageChanged(int pageKey)
{
	LiveAreaMgr *mgr = g_liveareaMgr;
	if (mgr != NULL && mgr->m_currentAppId != pageKey) {
		LiveAreaPage *page = liveareaPageFindByAppId(pageKey);
		if (page != NULL) {
			page->m_inner.ClearGateInitFlag();
			if (liveareaMgrIsPendingLaunch(page->m_titleId.c_str()))
				page->m_pendingScroll = true;
			mgr->m_pendingLaunchTitle.clear();
		}
		mgr->m_currentAppId = pageKey;
	}
}

/* 0x8119B1E0 */
static void liveareaIfUpdatePageInfo(int pageKey, const LiveAreaPageInitInfo *info)
{
	LiveAreaPage *page = liveareaPageFindByAppId(pageKey);
	if (page != NULL)
		page->UpdateInitInfo(info);
}

/* 0x8119B1F4 */
static void liveareaIfRefreshPage(int pageKey)
{
	LiveAreaPage *page = liveareaPageFindByAppId(pageKey);
	if (page != NULL && page->m_inner.m_xmlDoc.get() != NULL)
		liveareaMgrUpdatePage(&page->m_inner, 0);
}

/* 0x8119B20E */
static int liveareaIfHasPatchIcon(int pageKey)
{
	LiveAreaPage *page = liveareaPageFindByAppId(pageKey);
	if (page != NULL)
		return page->HasPatchIcon();
	return 0;
}

/* 0x8119B222 */
int liveareaIfInstallContent(LsdbAppInfoPtr *appInfo, bool force, bool isUpdate)
{
	LiveAreaMgr *mgr = g_liveareaMgr;
	if (!force) {
		paf::vector<paf::string> openTitles;
		paf::string evictTitle;
		mgr->m_topmenuIf->getOpenPageTitleIds(&openTitles);
		SceLsdb_DA5DC77C(&mgr->m_lsdbCookie, &openTitles, &evictTitle);
		if (!evictTitle.empty()) {
			int ret = mgr->m_gameIf->removeLiveAreaContent(&evictTitle, 1);
			if (ret != 0)
				return ret;
			ret = SceLsdb_C87397C0(&evictTitle);
			if (ret != 0)
				return ret;
		}
	}

	int installMode = 0;
	if (force)
		installMode = isUpdate ? 2 : 1;
	int ret = mgr->m_gameIf->prepareLiveAreaInstall(appInfo, installMode);
	if (ret == 0 && !force) {
		const char *name = sceLsdbGetName(appInfo);
		paf::string titleStr(name);
		const char *contentPath = SceLsdb_C070E42E(appInfo);
		ret = SceLsdb_C875C57A(&titleStr, contentPath);
	}
	return ret;
}

/* 0x8119B4C8 */
static int liveareaIfOnAppDeleted(paf::string *titleId)
{
	g_liveareaMgr->m_gameIf->removeLiveAreaContent(titleId, 1);
	SceLsdb_C87397C0(titleId);
	return 0;
}

/* 0x8119B4EC */
static void liveareaMgrOnUpdaterDone(int status, int pageKey)
{
	LiveAreaPage *page = liveareaPageFindByAppId(pageKey);
	if (page != NULL)
		page->m_inner.StopReloadSpin(status);
}

/* 0x8119B506 */
static void liveareaMgrOnNetEvent(int event)
{
	if (event == 0)
		activityUiEnqueueCacheInfoJob();
}

/* 0x8119B510 */
static void liveareaMgrOnShellPause(void)
{
	liveareaPageOnShellPause();
}

/* 0x8119B518 */
static void liveareaMgrOnShellResume(int eventId, int eventArg, int pageKey)
{
	(void)eventId;
	(void)eventArg;
	liveareaPageOnShellResume(pageKey);
}
