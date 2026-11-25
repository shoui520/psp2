/*
 * top_tab_app_data.cpp: TopTabScreenCopy and TopTabAppData managing each open
 * LiveArea / LiveBoard tab's PageTurn card, snapshot, and process binding
 * (0x8129666C-0x81298D23).
 */
#include "topmenu_internal.h"
#include <inputdevice/inputdevice.h>
#include <inputdevice/pad/pad.h>
#include <common/string_util.h>

namespace {

static const SceUInt32 TAB_APP_CARD_TEMPLATE_ID = 0xEC6A516Cu;	/* -328574900 */
static const SceUInt32 TAB_APP_ROOT_WIDGET_ID = 0xB349460Bu;	/* -1286985381 */
static const SceUInt32 TAB_APP_CONTENT_WIDGET_ID = 0xCF970E4Eu;	/* -812175378 */
static const SceUInt32 TAB_APP_SNAP_TEMPLATE_ID = 0x94932809u;	/* -1802327895 */
static const SceUInt32 TAB_APP_SNAP_PLANE_ID = 0xCE594C7Au;	/* -833015238 */
static const SceUInt32 TAB_APP_DEFAULT_TEX_ID = 0xAFDF05D0u;	/* -1344341552 */
static const SceUInt32 TAB_APP_BUSY_TEMPLATE_ID = 0xB6E0C320u;	/* -1226795232 */
static const SceUInt32 TAB_APP_BUSY_WIDGET_ID = 0xAE8F3C5Bu;	/* -1366318501 */
static const SceUInt32 TAB_APP_SCROLL_TEMPLATE_ID = 0x07F318C7u;/* 133375143 */
static const SceUInt32 TAB_APP_SCROLL_WIDGET_ID = 0x6C22D94Du;	/* 1814211981 */
static const SceUInt32 LSDB_KEY_ONLINE_MANUAL = 0x6DBCFF31u;

static paf::Timer *createTabAppTimer(float durationMs, paf::Timer::Func funcType)
{
	paf::Timer *timer = new paf::Timer();
	if (timer != NULL)
		timer->InitTimer(durationMs, funcType, paf::Timer::REPEAT_NONE, -1, NULL);
	return timer;
}

static void showWidgetImmediate(paf::ui::Widget *widget)
{
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xFF;
		widget->m_transition_complete = true;
		widget->DoTransition_core(1.0f, true);
	}
	ScePafWidget_FF5BC957(widget, NULL, 2, 0.0f, NULL);
}

static void hideWidgetWithTimer(paf::ui::Widget *widget, paf::Timer *timer)
{
	widget->m_transition_complete = false;
	widget->m_transition = 0xFF;
	widget->DoTransitionReverse_core(1.0f, true);
	ScePafWidget_4588699E(widget, timer, 2, 0.0f, NULL);
}

static void onPageTurnMove(int32_t eventType, paf::ui::Handler *self,
			   paf::ui::Event *event, void *userData);
static void onPageTurnCancel(int32_t eventType, paf::ui::Handler *self,
			     paf::ui::Event *event, void *userData);
static void onPageTurnTurned(int32_t eventType, paf::ui::Handler *self,
			     paf::ui::Event *event, void *userData);
static void onPageTurnFoldTimer(int32_t eventType, paf::ui::Handler *self,
				paf::ui::Event *event, void *userData);
static void onPageTurnCloseTimer(int32_t eventType, paf::ui::Handler *self,
				 paf::ui::Event *event, void *userData);
static void onLiveAreaBannerUriClick(int32_t eventType, paf::ui::Handler *self,
				     paf::ui::Event *event, void *userData);
static void onBackgroundImageLoaded(int32_t eventType, paf::ui::Handler *self,
				    paf::ui::Event *event, void *userData);
static void onContentAlphaRestored(paf::Timer *timer, paf::ui::AnimListener *listener,
				   void *targetWidget, void *userData, void *extraData);

/* 0x81298658 */
static void onPageTurnMove(int32_t eventType, paf::ui::Handler *self,
			   paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)self;
	(void)event;
	TopTabAppData *appData = (TopTabAppData *)userData;
	appData->m_contentWidget->SetMetaAlpha(0.0f, NULL, 0x10003, 0.0f, 0, NULL, NULL);
	topmenuSetPageTurnDragging(g_topWindow, true);

	if (appData->m_snapshotPlane != NULL && appData->m_hasSnapshot) {
		paf::graph::PlaneObj *planeObj =
			(paf::graph::PlaneObj *)appData->m_snapshotPlane->GetDrawObj(0);
		if (planeObj != NULL)
			planeObj->m_draw_style.blend = 1u;
	}
}

/* 0x81298700 */
static void onPageTurnCancel(int32_t eventType, paf::ui::Handler *self,
			     paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)self;
	(void)event;
	TopTabAppData *appData = (TopTabAppData *)userData;
	paf::Timer *timer = createTabAppTimer(0.0f, paf::Timer::FUNC_LINEAR);
	appData->m_contentWidget->SetMetaAlpha(
		1.0f, timer, 0x10003, 0.0f, 0, onContentAlphaRestored, appData);
	topmenuSetPageTurnDragging(g_topWindow, false);
}

/* 0x8129879C */
static void onPageTurnTurned(int32_t eventType, paf::ui::Handler *self,
			     paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)event;
	TopTabAppData *appData = (TopTabAppData *)userData;
	paf::ui::PageTurn *pageTurn = (paf::ui::PageTurn *)self;

	pageTurn->KillAnim(paf::ui::PageTurn::ANIM_TURN);
	paf::math::v4 peelTarget(150.0f, -900.0f, 0.0f, 0.0f);
	paf::Timer *peelTimer = createTabAppTimer(0.0f, paf::Timer::FUNC_LINEAR);
	pageTurn->ScePafWidget_B0E5E134(
		peelTarget, peelTimer, paf::ui::PageTurn::ANIM_TURN, 0.0f, 2, NULL, NULL);

	pageTurn->SetTimeoutEvent(1, 0.0f, onPageTurnFoldTimer, appData);

	hideWidgetWithTimer(pageTurn, createTabAppTimer(0.0f, paf::Timer::FUNC_LINEAR));
	hideWidgetWithTimer(appData->m_contentWidget,
			    createTabAppTimer(0.0f, paf::Timer::FUNC_LINEAR));
	if (appData->m_cornerSupplier != NULL)
		hideWidgetWithTimer(appData->m_cornerSupplier, NULL);

	paf::Framework::Instance()->PlayCommonSE(28, 0.0f, false);
	topmenuSetPageTurnDragging(g_topWindow, false);
	shellAppEventServerSend(0, NULL, 0);
}

/* 0x81298A8E */
static void onPageTurnFoldTimer(int32_t eventType, paf::ui::Handler *self,
				paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)event;
	paf::ui::Widget *widget = (paf::ui::Widget *)self;
	hideWidgetWithTimer(widget, NULL);
	widget->SetTimeoutEvent(1, 0.0f, onPageTurnCloseTimer, userData);
}

/* 0x81298B2C */
static void onPageTurnCloseTimer(int32_t eventType, paf::ui::Handler *self,
				 paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)self;
	(void)event;
	TopTabAppData *appData = (TopTabAppData *)userData;
	TopTab *ownerTab = appData->m_ownerTab;
	bool animate = false;
	if (!topmenuIsLaunchSeqIdle(&g_topWindow->m_launchSeq)) {
		animate = false;
	} else {
		SceInt32 snapId = ownerTab->m_scrollView->GetSnapID(ownerTab->m_widget);
		animate = (g_topWindow->m_activeSnapId == snapId);
	}
	topmenuCloseTab(ownerTab, animate);
}

/* 0x81298B92 */
static void onLiveAreaBannerUriClick(int32_t eventType, paf::ui::Handler *self,
				     paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)self;
	(void)event;
	TopTabAppData *appData = (TopTabAppData *)userData;
	const char *uri = SceLsdb_B7F9E89F(&appData->m_process.m_appInfo);
	if (uri != NULL)
		sceAppMgrLaunchAppByUri(0x20000u, uri);
}

/* 0x81298BAC */
static void onBackgroundImageLoaded(int32_t eventType, paf::ui::Handler *self,
				    paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)event;
	TopTabAppData *appData = (TopTabAppData *)userData;
	--appData->m_bgLoadPendingCount;

	if (!g_topWindow->m_openingLiveArea && appData->m_bgLoadPendingCount > 0) {
		appData->m_bgLoadPendingCount = 0;
		appData->ShowAnimated(0.0f);
	} else if (appData->m_bgLoadPendingCount == 0) {
		appData->ShowAnimated(0.0f);
	}

	self->DeleteEventCallback(0x100005, onBackgroundImageLoaded, userData);
	self->DeleteEventCallback(0x100007, onBackgroundImageLoaded, userData);
}

/* 0x81298C0E */
static void onContentAlphaRestored(paf::Timer *timer, paf::ui::AnimListener *listener,
				   void *targetWidget, void *userData, void *extraData)
{
	(void)timer;
	(void)listener;
	(void)extraData;
	paf::ui::Widget *widget = (paf::ui::Widget *)targetWidget;
	TopTabAppData *appData = (TopTabAppData *)userData;
	paf::ui::Widget *snapPlane = widget->FindChild(paf::IDParam(TAB_APP_SNAP_PLANE_ID), 0);
	if (snapPlane != NULL && appData->m_hasSnapshot) {
		paf::graph::PlaneObj *planeObj =
			(paf::graph::PlaneObj *)snapPlane->GetDrawObj(0);
		if (planeObj != NULL)
			planeObj->m_draw_style.blend = 0xFu;
	}
}

} /* namespace */

SceGxmRenderTarget *g_topmenuSmallRenderTarget;
SceGxmRenderTarget *g_topmenuLargeRenderTarget;
SceInt32 g_topmenuDisabledPageTurnCount;

/* ------------------------------------------------------------------ */
/* TopTabScreenCopy (0x8129666C..0x81296806)                          */
/* ------------------------------------------------------------------ */

/* 0x8129666C */
TopTabScreenCopy::TopTabScreenCopy(TopTabAppData *owner, paf::graph::SurfacePool *pool,
				   bool fullSize)
	: m_owner(owner),
	  m_surface(),
	  m_copyObj(NULL),
	  m_captured(false)
{
	if (owner->m_noScreenCopy)
		return;

	SceGxmRenderTarget *renderTarget = fullSize
						   ? g_topmenuLargeRenderTarget
						   : g_topmenuSmallRenderTarget;
	SceInt32 width = fullSize ? 480 : 280;
	SceInt32 height = fullSize ? 272 : 79;
	SceInt32 copySource = fullSize ? paf::graph::SCREEN_COPY_OFFSCREEN
				       : paf::graph::SCREEN_COPY_SURFACE;

	paf::intrusive_ptr<paf::graph::Surface> surf(new paf::graph::ScreenSurface(
		renderTarget, pool, width, height,
		ImageMode_RGBA8888, ImageOrder_Linear, 1,
		ImageMode_NONE, (ImageOrder)0));
	m_surface = surf;
	m_copyObj = paf::graph::ScreenCopy::CreateObj(m_surface, copySource);
	m_copyObj->m_state = paf::graph::SCREEN_COPY_OFF;
}

TopTabScreenCopy::~TopTabScreenCopy()
{
	if (m_copyObj != NULL)
		paf::graph::ScreenCopy::DeleteObj(m_copyObj);
}

/* 0x8129673A */
void TopTabScreenCopy::ClearState()
{
	m_copyObj->m_state = paf::graph::SCREEN_COPY_OFF;
}

/* 0x81296742 */
void TopTabScreenCopy::CaptureAndPublish()
{
	int captureFlags = -5;
	if (SceLsdb_849E8DC6(&m_owner->m_process.m_appInfo) == 0) {
		captureFlags = -2;
		if (topmenuAppProcessIsRunning(&m_owner->m_process) &&
		    m_owner->m_process.m_hasAppId) {
			captureFlags = m_owner->m_process.m_appId;
		}
	}

	paf::intrusive_ptr<paf::graph::Surface> capturedSurf;
	if (shellScreenshotCaptureSurface(&capturedSurf, captureFlags) == 0) {
		LiveAreaPluginInterface *liveAreaIf = topmenuGetLiveAreaInterface();
		float gateX = 0.0f;
		float gateY = 0.0f;
		liveAreaIf->getGateNormPos(m_owner->m_ownerTab->m_pageKey, &gateX, &gateY);

		bool reverseEffect = true;
		if (m_copyObj != NULL) {
			m_copyObj->m_state = paf::graph::SCREEN_COPY_ONCE;
			m_copyObj->m_source = capturedSurf;
			reverseEffect = false;
		}
		topmenuSetGateEffectSurface(reverseEffect, &capturedSurf);
	}
}

/* 0x81296806 */
bool TopTabScreenCopy::IsReady() const
{
	if (m_copyObj == NULL)
		return false;
	if (m_captured)
		return true;
	return m_copyObj->m_state != paf::graph::SCREEN_COPY_OFF;
}

/* ------------------------------------------------------------------ */
/* TopTabAppData (0x81296820..0x81298C0E)                             */
/* ------------------------------------------------------------------ */

/* 0x81296820 */
void TopTabAppData::HideAnimated(float durationSec)
{
	if (m_pageTurn != NULL)
		ScePafMisc_C4E8CBE6(0x20u);

	paf::math::v4 zeroScale(0.0f, 0.0f, 0.0f, 0.0f);
	if (durationSec > 0.0f) {
		m_rootWidget->SetMetaAlpha(
			0.0f, createTabAppTimer(durationSec * 1000.0f, paf::Timer::FUNC_QUAD_OUT),
			0x10003, 0.0f, 2, NULL, NULL);
		m_rootWidget->SetScale(
			zeroScale,
			createTabAppTimer(durationSec * 1000.0f, paf::Timer::FUNC_BACK_IN),
			0x10005, 0.0f, 2, NULL, NULL);
	} else {
		m_rootWidget->SetMetaAlpha(0.0f, NULL, 0x10003, 0.0f, 0, NULL, NULL);
		m_rootWidget->SetScale(zeroScale, NULL, 0x10005, 0.0f, 0, NULL, NULL);
	}
}

/* 0x8129699E */
TopTabAppData::TopTabAppData(TopTab *ownerTab, const paf::string &titleId,
			     const LsdbAppInfoPtr &appInfo, SceInt32 launchMode,
			     bool autoStart, SceInt32 startArg)
	: m_titleId(titleId),
	  m_ownerTab(ownerTab),
	  m_noScreenCopy(false),
	  m_hasSnapshot(false),
	  m_shownOnce(false),
	  m_upStatus(0),
	  m_rootWidget(NULL),
	  m_pageTurn(NULL),
	  m_contentWidget(NULL),
	  m_snapshotPlane(NULL),
	  m_busyIndicator(NULL),
	  m_cornerSupplier(NULL),
	  m_screenCopy(NULL),
	  m_viewKind(0),
	  m_appViewState(0),
	  m_captureCountdown(0),
	  m_bgLoadPendingCount(0),
	  m_dialogId(-1),
	  m_mediaAppType(0),
	  m_mediaStorageKind(0),
	  m_process(titleId, appInfo, this, launchMode, autoStart, startArg)
{
	paf::ui::Widget *hostWidget = ownerTab->m_contentRoot;
	paf::Plugin *plugin = ownerTab->m_plugin;

	plugin->TemplateOpen(hostWidget, paf::IDParam(TAB_APP_CARD_TEMPLATE_ID),
			     paf::Plugin::TemplateOpenParam());
	m_rootWidget = hostWidget->FindChild(paf::IDParam(TAB_APP_ROOT_WIDGET_ID), 0);

	m_pageTurn = new paf::ui::PageTurn(m_rootWidget, NULL);
	ScePafWidget_CB95F8F2(m_pageTurn, 1);
	if (!m_titleId.empty())
		m_pageTurn->m_name = paf::IDParam(m_titleId.c_str());
	else
		m_pageTurn->m_name.m_idhash = 0;

	m_pageTurn->m_hit_box_mode = 0;
	m_pageTurn->m_draw_always = true;
	ScePafWidget_21125870(m_pageTurn, 0x20u);

	paf::math::v4 rootSize = *m_rootWidget->GetSize(0);
	m_pageTurn->SetSize(rootSize, NULL, 0x10004, 0.0f, 0, NULL, NULL);

	m_contentWidget = m_rootWidget->FindChild(paf::IDParam(TAB_APP_CONTENT_WIDGET_ID), 0);
	m_pageTurn->SetBehind(m_contentWidget);

	ownerTab->m_poolSlot->cacheManager->GetCacheManager()->MakeSpaceSurfacePool(0, 0);
	HideAnimated(0.0f);

	if (m_rootWidget->m_scene != NULL) {
		paf::ui::Supplier *supplier = new paf::ui::Supplier(m_rootWidget, NULL);
		paf::math::v4 pageSize = *m_pageTurn->GetSize(0);
		paf::math::v4 cornerSize(194.0f, 132.0f, 0.0f, 0.0f);
		supplier->ScePafWidget_2EBD646E(m_pageTurn, true);
		supplier->m_follow_pos = true;
		supplier->m_pos_offset = paf::math::v4(
			pageSize.extract_x() * 0.5f - cornerSize.extract_x() * 0.5f + 81.0f,
			pageSize.extract_y() * 0.5f - cornerSize.extract_y() * 0.5f + 33.0f,
			0.0f, 0.0f);
		supplier->SetSize(cornerSize, NULL, 0x10004, 0.0f, 0, NULL, NULL);
		supplier->SetMetaAlpha(0.0f, NULL, 0x10003, 0.0f, 0, NULL, NULL);

		m_pageTurn->AddEventCallback(
			paf::ui::PageTurnBase::CB_PAGETURN_TURNED, onPageTurnTurned, this);
		m_pageTurn->AddEventCallback(
			paf::ui::PageTurnBase::CB_PAGETURN_MOVE, onPageTurnMove, this);
		m_pageTurn->AddEventCallback(
			paf::ui::PageTurnBase::CB_PAGETURN_CANCEL, onPageTurnCancel, this);
		m_cornerSupplier = supplier;
	}
}

/* 0x81296EEE */
TopTabAppData::~TopTabAppData()
{
	paf::inputdevice::Lock(0x7Fu);
	paf::inputdevice::Unlock(0x7Fu);

	if (m_process.m_appInfo.get() != NULL) {
		const char *appName = SceLsdb_457F369E(&m_process.m_appInfo);
		if (appName != NULL && (topmenuGetAppFlags(g_topWindow, appName) & 0x4000000u) == 0)
			sceAppMgrDestroyAppByName(appName);
	}

	if (m_process.m_launchMode == 0) {
		ShellMediaInfoRequest req;
		sce_paf_memset(&req, 0, sizeof(req));
		req.type = 8;
		sce_paf_strncpy(req.titleId, m_titleId.c_str(), 10);
		req.attr[2] = topmenuIsLaunchSeqIdle(&g_topWindow->m_launchSeq) ? 1 : 2;
		shellMediaInfoWrite(&req, false);
	}

	if (m_pageTurn->m_turn_state == paf::ui::PageTurnBase::ST_PAGETURN_DISABLE)
		--g_topmenuDisabledPageTurnCount;

	m_contentWidget->KillAnimType(0x10003);
	m_rootWidget->DestroyWidget();

	if (m_screenCopy != NULL)
		delete m_screenCopy;
}

/* 0x8129700E */
void TopTabAppData::ShowAnimated(float durationSec)
{
	ApplauncherInterface *launcherIf = topmenuGetApplauncherInterface();
	if (launcherIf != NULL) {
		paf::string titleCopy(m_titleId);
		typedef void (*EndIconEffectFn)(const paf::string &titleId);
		((EndIconEffectFn)launcherIf->func[7])(titleCopy);
	}

	if (!m_shownOnce)
		m_shownOnce = true;

	paf::math::v4 oneScale(1.0f, 1.0f, 1.0f, 1.0f);
	if (durationSec > 0.0f) {
		m_rootWidget->SetMetaAlpha(
			1.0f, createTabAppTimer(durationSec * 1000.0f, paf::Timer::FUNC_QUAD_OUT),
			0x10003, 0.0f, 2, NULL, NULL);
		m_rootWidget->SetScale(
			oneScale,
			createTabAppTimer(durationSec * 1000.0f, paf::Timer::FUNC_BACK_OUT),
			0x10005, 0.0f, 2, NULL, NULL);
	} else {
		m_rootWidget->SetMetaAlpha(1.0f, NULL, 0x10003, 0.0f, 0, NULL, NULL);
		m_rootWidget->SetScale(oneScale, NULL, 0x10005, 0.0f, 0, NULL, NULL);
	}
}

/* 0x812971EE */
void TopTabAppData::SetAppViewState(SceInt32 state)
{
	if (state != m_appViewState)
		m_appViewState = state;
}

/* 0x812971F8 */
void TopTabAppData::SetSnapshotSurface(const paf::intrusive_ptr<paf::graph::Surface> &surface)
{
	if (m_snapshotPlane == NULL)
		return;

	paf::math::v4 white(1.0f, 1.0f, 1.0f, 1.0f);
	m_hasSnapshot = (surface.get() != NULL);
	m_snapshotPlane->SetTexture(surface);

	if (m_hasSnapshot) {
		m_snapshotPlane->SetColor(white, NULL, 0x10001, 0.0f, 0, NULL, NULL);
		paf::graph::PlaneObj *planeObj =
			(paf::graph::PlaneObj *)m_snapshotPlane->GetDrawObj(0);
		paf::math::v4 region(0.0625f, 0.0588235f, 0.875f, 0.9411765f);
		planeObj->SetTexRegion(region);
		planeObj->m_draw_style.blend = 0xFu;
	} else {
		m_snapshotPlane->SetColor(white, NULL, 0x10001, 0.0f, 0, NULL, NULL);
		paf::intrusive_ptr<paf::graph::Surface> fallbackTex =
			g_topmenuPlugin->GetTexture(paf::IDParam(TAB_APP_DEFAULT_TEX_ID));
		m_snapshotPlane->SetTexture(fallbackTex);
		paf::graph::PlaneObj *planeObj =
			(paf::graph::PlaneObj *)m_snapshotPlane->GetDrawObj(0);
		paf::math::v4 fullRegion(0.0f, 0.0f, 1.0f, 1.0f);
		planeObj->SetTexRegion(fullRegion);
		planeObj->m_draw_style.blend = 1u;
	}
}

/* 0x81297388 */
void TopTabAppData::Update()
{
	bool isCurrentAndRunning =
		(g_topWindow->m_currentTab == m_ownerTab) &&
		topmenuAppProcessIsRunning(&m_process) &&
		(m_process.m_appId != -1);

	if (m_captureCountdown > 0 && isCurrentAndRunning) {
		SceUID pid = topmenuProcessGetPid(&m_process);
		if (sceSharedFbGetRenderingInfo(pid) == 0) {
			--m_captureCountdown;
			if (m_captureCountdown == 0) {
				if (m_screenCopy != NULL && m_screenCopy->m_copyObj != NULL) {
					m_screenCopy->m_captured = true;
					SceInt32 srcKind = m_screenCopy->m_copyObj->m_source_kind;
					if (srcKind != paf::graph::SCREEN_COPY_FRONT &&
					    srcKind != paf::graph::SCREEN_COPY_SURFACE) {
						m_screenCopy->m_copyObj->m_state =
							paf::graph::SCREEN_COPY_ON;
					}
				}
				paf::intrusive_ptr<paf::graph::Surface> defaultSnap(
					&g_topmenuDefaultSnapshotSurface);
				SetSnapshotSurface(defaultSnap);
			}
		}
	}

	if (m_busyIndicator != NULL) {
		if (!isCurrentAndRunning ||
		    sceSharedFbGetRenderingInfo(topmenuProcessGetPid(&m_process)) == 0) {
			m_busyIndicator->Stop();
		} else {
			m_busyIndicator->Start();
		}
	}

	if (sceKernelCheckDipswForUser(185) != 0 &&
	    m_process.m_bootTimeStartUs != 0 &&
	    topmenuAppProcessIsRunning(&m_process) &&
	    m_process.m_appId != -1) {
		SceUID pid = topmenuProcessGetPid(&m_process);
		if (sceSharedFbGetRenderingInfo(pid) == 0) {
			SceUInt32 startUs = m_process.m_bootTimeStartUs;
			m_process.m_bootTimeStartUs = 0;
			SceUInt32 elapsedUs = sceKernelGetSystemTimeLow() - startUs;
			if (g_topWindow->m_imposeIf != NULL) {
				ImposeNotice notice;
				wchar_t textBuf[2048];
				sce_paf_swprintf(textBuf, 2048, L"BootTime\n%.2f[sec]",
						 (double)elapsedUs / 1000000.0);
				notice.name = "BootTime";
				notice.message = textBuf;
				g_topWindow->m_imposeIf->showNotice(&notice);
			}
		}
	}
}

/* 0x81297578 */
void TopTabAppData::SetPageTurnDisabled(bool disabled)
{
	if (m_cornerSupplier == NULL)
		return;

	if (disabled) {
		m_pageTurn->ScePafWidget_1FE12722();
		hideWidgetWithTimer(m_cornerSupplier, NULL);
	} else {
		const char *activityTitle = SceActivityDb_E6C23731();
		if (sce_paf_strcmp(activityTitle, m_titleId.c_str()) != 0) {
			m_pageTurn->ScePafWidget_D321F831();
			showWidgetImmediate(m_cornerSupplier);
		}
	}
}

/* 0x81297624 */
int TopTabAppData::ReloadLiveArea(const LiveAreaReloadRequest *req)
{
	LiveAreaPluginInterface *liveAreaIf = topmenuGetLiveAreaInterface();
	return liveAreaIf->requestReload(m_ownerTab->m_pageKey, req);
}

/* 0x8129763C */
void TopTabAppData::OnActivateTab()
{
	if (m_cornerSupplier == NULL)
		return;

	hideWidgetWithTimer(m_cornerSupplier, NULL);
	if (m_pageTurn != NULL) {
		m_pageTurn->m_hit_detect_b_mode = 2;
		m_pageTurn->m_hit_detect_mode = 2;
		ScePafWidget_164BFFA5(m_pageTurn, 0x20u);
	}
}

/* 0x8129769E */
void TopTabAppData::OnDeactivateTab()
{
	if (m_cornerSupplier == NULL || m_pageTurn == NULL)
		return;

	if (m_pageTurn->m_turn_state != paf::ui::PageTurnBase::ST_PAGETURN_DISABLE)
		showWidgetImmediate(m_cornerSupplier);

	m_pageTurn->m_hit_detect_mode = 0;
	m_pageTurn->m_hit_detect_b_mode = 0;
	ScePafWidget_21125870(m_pageTurn, 0x20u);
}

/* 0x8129770C */
void TopTabAppData::OnBgLoadStepDone()
{
	if (m_bgLoadPendingCount > 0) {
		--m_bgLoadPendingCount;
		if (m_bgLoadPendingCount == 0)
			ShowAnimated(0.0f);
	}
}

/* 0x8129772C */
paf::intrusive_ptr<paf::graph::Surface> TopTabAppData::GetScreenCopySurface() const
{
	if (m_screenCopy != NULL)
		return m_screenCopy->m_surface;
	return paf::intrusive_ptr<paf::graph::Surface>();
}

/* 0x81297746 */
void TopTabAppData::UpdateUpStatus(SceInt32 newStatus)
{
	LsdbAppInfo *record = m_process.m_appInfo.get();
	LsdbAppInfo::iterator it = record->find(LSDB_KEY_UP_STATUS);
	if (it == record->end())
		return;

	if ((newStatus == 3 && m_upStatus == 1) || (newStatus == 1 && m_upStatus == 3)) {
		if (it->second.type == LSDB_VALUE_TEXT && it->second.text != NULL) {
			delete it->second.text;
			it->second.text = NULL;
		}
		it->second.type = LSDB_VALUE_INTEGER;
		it->second.integer = newStatus;
		m_upStatus = newStatus;

		LiveAreaReloadRequest req;
		req.titleId = m_titleId.c_str();
		req.contentDir = NULL;
		req.reloadMask = -1;
		LiveAreaPluginInterface *liveAreaIf = topmenuGetLiveAreaInterface();
		liveAreaIf->requestReload(m_ownerTab->m_pageKey, &req);
	}
}

/* 0x8129787A */
void TopTabAppData::FillLiveAreaInitInfo(LiveAreaPageInitInfo *outInfo)
{
	outInfo->gateIconCache = m_ownerTab->m_iconCache.get();

	if (sceLsdbGetCategory(&m_process.m_appInfo) != NULL) {
		int appType = sceLsdbGetType(&m_process.m_appInfo);
		outInfo->upgradeMode = appType;
		switch (appType) {
		case 1:
			outInfo->style = (SceLsdb_9D27A87D(&m_process.m_appInfo) != 0) ? 1 : 0;
			break;
		case 2:
			outInfo->style = 2;
			break;
		case 3:
			outInfo->style = 3;
			break;
		case 4:
		case 5:
			outInfo->style = 4;
			break;
		default:
			break;
		}
	}

	if (outInfo->style >= 1 && outInfo->style <= 4) {
		if ((SceLsdb_0B16523B(&m_process.m_appInfo) & 2) != 0)
			outInfo->manualMode = 1;
	} else if (outInfo->style == 0) {
		outInfo->title = m_ownerTab->m_title;
	}

	const char *title = sceLsdbGetTitle(&m_process.m_appInfo);
	outInfo->searchKey = (title != NULL) ? title : "";

	outInfo->appAttr = (int)sceLsdbGetSystemVersion(&m_process.m_appInfo);
	outInfo->hasInfo = (SceLsdb_DA0D7B0A(&m_process.m_appInfo) != 0);
	outInfo->hasExt = (SceLsdb_44CD0D07(&m_process.m_appInfo) != 0);
	outInfo->hasFeed = SceLsdb_F4325AD0(&m_process.m_appInfo);

	const char *appVer = sceLsdbGetAppVer(&m_process.m_appInfo);
	outInfo->url = (appVer != NULL) ? appVer : "";

	LsdbAppInfo *record = m_process.m_appInfo.get();
	outInfo->hasOnlineManual = (record != NULL && record->find(LSDB_KEY_ONLINE_MANUAL) != record->end());

	SceInt64 kindVal = SceLsdb_9D27A87D(&m_process.m_appInfo);
	if (kindVal == 2) {
		outInfo->kind = 1;
		if (sce_paf_strcmp("NPXS10998", m_titleId.c_str()) == 0)
			outInfo->hasExt = false;
	} else if (kindVal == 3) {
		outInfo->kind = 2;
	}
}

/* 0x81297A36 */
void TopTabAppData::RefreshLiveAreaInfo()
{
	LiveAreaPageInitInfo info;
	info.style = 1;
	info.manualMode = 0;
	info.upgradeMode = 0;
	info.gateIconCache = NULL;
	info.appAttr = 0;
	info.kind = 0;
	info.hasInfo = false;
	info.hasExt = false;
	info.hasFeed = false;
	info.hasOnlineManual = false;

	LiveAreaPluginInterface *liveAreaIf = topmenuGetLiveAreaInterface();
	FillLiveAreaInitInfo(&info);
	liveAreaIf->updatePageInfo(m_ownerTab->m_pageKey, &info);

	char contentDir[2048];
	const char *liveAreaPath = SceLsdb_C070E42E(&m_process.m_appInfo);
	if (liveAreaPath != NULL)
		sce_paf_snprintf(contentDir, sizeof(contentDir), "%s", liveAreaPath);
	else
		contentDir[0] = '\0';

	LiveAreaReloadRequest req;
	req.titleId = m_titleId.c_str();
	req.contentDir = contentDir;
	req.reloadMask = (SceInt32)0xFFEAEFFF;
	liveAreaIf->requestReload(m_ownerTab->m_pageKey, &req);
}

/* 0x81297B58 */
void TopTabAppData::OnEnterViewport()
{
	topmenuAppProcessOnEnterViewport(&m_process);
}

/* 0x81297B62 */
void TopTabAppData::OnLeaveViewport()
{
	topmenuAppProcessOnLeaveViewport(&m_process);
}

/* 0x81297B6C */
void TopTabAppData::ReportLiveAreaOpen()
{
	if (m_process.m_launchMode != 0)
		return;

	ShellMediaInfoRequest req;
	sce_paf_memset(&req, 0, sizeof(req));
	req.type = 6;
	sce_paf_strncpy(req.titleId, m_titleId.c_str(), 10);
	req.attr[0] = m_mediaAppType;
	req.attr[1] = m_mediaStorageKind;
	req.attr[2] = topmenuIsLaunchSeqIdle(&g_topWindow->m_launchSeq) ? 1 : 2;
	req.attr[3] = g_topWindow->m_gateEffectActive ? 1 : 2;
	shellMediaInfoWrite(&req, false);
}

/* 0x81297BF6 */
void TopTabAppData::ReportLiveAreaClose()
{
	if (m_process.m_launchMode != 0)
		return;

	ShellMediaInfoRequest req;
	sce_paf_memset(&req, 0, sizeof(req));
	req.type = 7;
	sce_paf_strncpy(req.titleId, m_titleId.c_str(), 10);
	req.attr[0] = m_mediaAppType;
	req.attr[1] = m_mediaStorageKind;
	req.attr[2] = topmenuIsLaunchSeqIdle(&g_topWindow->m_launchSeq) ? 1 : 2;
	shellMediaInfoWrite(&req, false);
}

/* 0x81297C6A */
void TopTabAppData::LoadBackgroundImage()
{
	char bgPath[2048];
	const char *liveAreaPath = SceLsdb_C070E42E(&m_process.m_appInfo);
	if (liveAreaPath != NULL)
		sce_paf_snprintf(bgPath, sizeof(bgPath), "%s", liveAreaPath);
	else
		bgPath[0] = '\0';
	sce_paf_strncat(bgPath, "/bg0.png", sizeof(bgPath));

	paf::intrusive_ptr<paf::cache::CacheImageInterface> bgCache;
	m_ownerTab->m_poolSlot->cacheManager->CreateImageCache(
		paf::string(bgPath), bgCache, NULL, NULL);

	m_pageTurn->AddEventCallback(0x100005, onBackgroundImageLoaded, this);
	m_pageTurn->AddEventCallback(0x100007, onBackgroundImageLoaded, this);

	m_bgLoadPendingCount =
		(!g_topWindow->m_openingLiveArea || g_topWindow->m_liveAreaReady) ? 1 : 2;
	m_pageTurn->SetDrawObjMaskBit(&bgCache, 0);
}

/* 0x81297DF8 */
void TopTabAppData::SetupContent()
{
	LsdbAppInfoPtr *appInfo = &m_process.m_appInfo;
	LsdbAppInfo *record = appInfo->get();
	if (record != NULL) {
		LsdbAppInfo::iterator it = record->find(LSDB_KEY_UP_STATUS);
		if (it != record->end() && it->second.type == LSDB_VALUE_INTEGER)
			m_upStatus = (SceInt32)it->second.integer;
	}

	if (m_ownerTab != NULL) {
		const char *stitle = sceLsdbGetStitle(appInfo);
		if (stitle != NULL) {
			paf::string utf8Title(stitle);
			paf::wstring utf16Title;
			paf::common::Utf8ToUtf16(utf8Title, &utf16Title);
			m_ownerTab->SetTitleText(utf16Title);
		}
		if (m_ownerTab->m_iconCache.get() == NULL) {
			ApplauncherInterface *launcherIf = topmenuGetApplauncherInterface();
			paf::intrusive_ptr<paf::cache::CacheImageInterface> iconCache;
			const char *origPath = sceLsdbGetOriginalPath(appInfo);
			if (origPath != NULL) {
				char iconDir[2048];
				sce_paf_snprintf(iconDir, sizeof(iconDir), "%s/", origPath);
				typedef void (*LoadIconImageFn)(
					const char *dir,
					paf::intrusive_ptr<paf::cache::CacheImageInterface> *out);
				((LoadIconImageFn)launcherIf->func[11])(iconDir, &iconCache);
				m_ownerTab->SetIconCache(iconCache);
				if (g_topWindow->m_indicatorIf != NULL) {
					g_topWindow->m_indicatorIf->SetPageDotImage(
						m_ownerTab->m_pageKey,
						(const paf::intrusive_ptr<paf::ui::TextureSource> *)&iconCache);
				}
			}
		}
	}

	int metaContentsKind = sceLsdbGetMetaContentsPath(appInfo);
	if (metaContentsKind == 0) {
		m_viewKind = 1;
	} else if (metaContentsKind == 1) {
		m_viewKind = 2;
	} else if (metaContentsKind == 2) {
		m_viewKind = 3;
		if (m_appViewState != 2)
			m_appViewState = 2;
	}

	if (m_viewKind == 1) {
		m_noScreenCopy = (SceLsdb_C80B07C3(appInfo) != 0);
		if (m_screenCopy == NULL) {
			m_ownerTab->m_poolSlot->cacheManager->GetCacheManager()
				->MakeSpaceSurfacePool(0x199A0u, 0x40u);
			m_screenCopy = new TopTabScreenCopy(
				this, m_ownerTab->m_poolSlot->surfacePool, false);
		}

		char contentDir[2048];
		const char *liveAreaPath = SceLsdb_C070E42E(appInfo);
		if (liveAreaPath != NULL)
			sce_paf_snprintf(contentDir, sizeof(contentDir), "%s", liveAreaPath);
		else
			contentDir[0] = '\0';

		LiveAreaPluginInterface *liveAreaIf = topmenuGetLiveAreaInterface();
		if (liveAreaIf != NULL) {
			LiveAreaPageInitInfo info;
			info.style = 1;
			info.manualMode = 0;
			info.upgradeMode = 0;
			info.gateIconCache = NULL;
			info.appAttr = 0;
			info.kind = 0;
			info.hasInfo = false;
			info.hasExt = false;
			info.hasFeed = false;
			info.hasOnlineManual = false;

			FillLiveAreaInitInfo(&info);
			liveAreaIf->createPage(m_ownerTab->m_pageKey, sceLsdbGetName(appInfo),
					       contentDir, &info);
		}
	} else if (m_viewKind == 2 || m_viewKind == 3) {
		LoadBackgroundImage();
		if (g_topWindow->m_foregroundApp == &m_process)
			shellAppEventServerSend(0, NULL, 0);

		if (m_screenCopy == NULL) {
			m_ownerTab->m_poolSlot->cacheManager->GetCacheManager()
				->MakeSpaceSurfacePool(0x83800u, 0x40u);
			m_screenCopy = new TopTabScreenCopy(
				this, m_ownerTab->m_poolSlot->surfacePool, true);
		}

		paf::Plugin *plugin = m_ownerTab->m_plugin;
		plugin->TemplateOpen(m_contentWidget, paf::IDParam(TAB_APP_SNAP_TEMPLATE_ID),
				     paf::Plugin::TemplateOpenParam());
		m_snapshotPlane =
			m_contentWidget->FindChild(paf::IDParam(TAB_APP_SNAP_PLANE_ID), 0);
		paf::intrusive_ptr<paf::graph::Surface> defaultTex =
			g_topmenuPlugin->GetTexture(paf::IDParam(TAB_APP_DEFAULT_TEX_ID));
		m_snapshotPlane->SetTexture(defaultTex);

		plugin->TemplateOpen(m_contentWidget, paf::IDParam(TAB_APP_BUSY_TEMPLATE_ID),
				     paf::Plugin::TemplateOpenParam());
		m_busyIndicator = (paf::ui::BusyIndicator *)m_contentWidget->FindChild(
			paf::IDParam(TAB_APP_BUSY_WIDGET_ID), 0);
		if (m_busyIndicator != NULL)
			m_busyIndicator->Start();

		if (m_viewKind == 2) {
			plugin->TemplateOpen(m_contentWidget,
					     paf::IDParam(TAB_APP_SCROLL_TEMPLATE_ID),
					     paf::Plugin::TemplateOpenParam());
			paf::ui::ScrollView *scroll =
				(paf::ui::ScrollView *)m_contentWidget->FindChild(
					paf::IDParam(TAB_APP_SCROLL_WIDGET_ID), 0);
			if (scroll != NULL) {
				paf::math::v2 minSize(960.0f, 544.0f);
				scroll->SetMinViewSize(minSize);
				scroll->SetVerticalScrollBar(NULL);
			}
		}

		if (SceLsdb_B7F9E89F(appInfo) != NULL) {
			LiveAreaPluginInterface *liveAreaIf = topmenuGetLiveAreaInterface();
			if (liveAreaIf != NULL) {
				paf::ui::Widget *overlay =
					liveAreaIf->openPageOverlay(m_contentWidget);
				overlay->AddEventCallback(
					0x10000008, onLiveAreaBannerUriClick, this);
			}
		}
	}

	shellAppEventServerSend(0, NULL, 0);

	if (SceLsdb_9D27A87D(appInfo) == 0) {
		m_mediaAppType = 1;
	} else {
		typedef int (*GetCategoryKindFn)(const LsdbAppInfoPtr *info, int *outKind);
		int categoryKind = 0;
		if (((GetCategoryKindFn)g_topWindow->m_gameIf->func[GAME_PLUGIN_FUNC_GET_CATEGORY_KIND])(
			    appInfo, &categoryKind) != 0) {
			m_mediaAppType = 0;
		} else {
			switch (categoryKind) {
			case 1:
				m_mediaAppType = 2;
				break;
			case 2:
				m_mediaAppType = 3;
				break;
			case 3:
				m_mediaAppType = 6;
				break;
			default:
				m_mediaAppType = 0;
				break;
			}
		}
	}

	const char *category = sceLsdbGetCategory(appInfo);
	if (category != NULL) {
		if (sce_paf_strcmp("gda", category) == 0 || sce_paf_strcmp("gdb", category) == 0) {
			m_mediaStorageKind = 3;
		} else {
			typedef int (*GetStorageFn)(const LsdbAppInfoPtr *info, int *outStorage);
			int storageKind = 0;
			if (((GetStorageFn)g_topWindow->m_gameIf->func[GAME_PLUGIN_FUNC_GET_STORAGE])(
				    appInfo, &storageKind) == 0) {
				if (storageKind == 1 || storageKind == 2)
					m_mediaStorageKind = 1;
				else if (storageKind == 3)
					m_mediaStorageKind = 2;
			}
		}
	}

	if (m_process.m_launchMode == 0) {
		ShellMediaInfoRequest req;
		sce_paf_memset(&req, 0, sizeof(req));
		req.type = 4;
		sce_paf_strncpy(req.titleId, m_titleId.c_str(), 10);
		shellMediaInfoWrite(&req, false);
	}
}

/* 0x8129864E */
void TopTabAppData::RequestCancelLaunch()
{
	topmenuAppProcessCancel(&m_process);
}
