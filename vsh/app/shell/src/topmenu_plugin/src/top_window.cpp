/*
 * top_window.cpp: TopWindow carousel, app lifecycle coordination, launch
 * sequencing, pre-launch confirmation dialogs, shell event dispatch, and the
 * TopmenuPluginInterface table (0x8126F818-0x81281F17).
 */
#include "topmenu_internal.h"
#if !defined(FW_TYPE_CEX)
extern "C" void ScePafAutoTestTty_9E10E873(int32_t type, const char *format, ...);
#endif
#include <graphics/surface/surface_pool.h>
#include <graphics/graphmem/graphmem.h>
#include <widget/w_pageturn.h>
#include <av_config/av_config.h>
#include <display/display.h>
#include <gxm.h>
#include <kernel/processmgr.h>
#include <kernel/sysmem.h>
#include <std/math.h>
#include <graphics/effects/gate_blur.h>
#include <graphics/effects/wave_switch_api.h>
#include <inputdevice/inputdevice.h>
#include <inputdevice/touchscreen/touchscreen.h>
#include "../../../shell_util_server.h"
#include "../../../theme.h"

extern "C" {
int sceAppMgrReceiveEventNum(SceInt32 *num);
void ScePafMisc_F8D63B8E(float value);
void ScePafGraphics_00B9D8CC(void);
void ScePafGraphics_906BE45C(void);
}

/* TopWindow globals (0x81545714..0x815AC4F0) */
TopWindow *g_topWindow;
TopmenuSurfacePoolSlot g_topmenuSurfacePools[6];
bool g_topmenuTestCrashEnabled;
SceGxmRenderTarget *g_topmenuSmallRenderTarget;
SceGxmRenderTarget *g_topmenuLargeRenderTarget;

static void *g_topmenuSurfaceMem;
static bool g_topmenuParentalBypassOnce;
static SceInt32 g_topmenuBusyDialogId = -1;
static SceInt32 g_topmenuPsCloseDialogId = -1;
static SceInt32 g_topmenuDebuggerErrorDialogId = -1;
static bool g_topmenuBootFreeSpaceChecked;

static SceUInt32 g_hashLiveAreaPath;
static SceUInt32 g_hashCtime;
static SceUInt32 g_hashLltime;
static SceUInt32 g_hashPromoteAttr;
static SceUInt32 g_hashUpStatus;
static SceUInt32 g_hashLibLocation;
static SceUInt32 g_hashAppConf;

#if FW_SINCE(0x03650000)
static const char *const g_topmenuBlockedTitles[4] = {
	"PCSA00044",
	"PCSC80009",
	"PCSD00025",
	"PCSF00124",
};
#endif

/* Resource / template / timer IDs used across TopWindow */
static const SceUInt32 TOP_WINDOW_PAGE_ID		= 0x5E95B7F3u;	/* 1586836915 */
static const SceUInt32 TOP_WINDOW_SCROLLVIEW_ID		= 0x9CC47823u;	/* -1664875101 */
static const SceUInt32 TOP_WINDOW_FOOTER_ID		= 0xB774ED3Au;	/* -1217057430 */
static const SceUInt32 TOP_WINDOW_ARROW_LEFT_ID		= 0xEB5C8B22u;	/* -346263710 */
static const SceUInt32 TOP_WINDOW_ARROW_RIGHT_ID	= 0x1526C956u;	/* 354871318 */
static const SceUInt32 TOP_WINDOW_COVER_PAGE_ID		= 0x9F7C54CDu;	/* -1619262707 */
static const SceUInt32 TOP_WINDOW_COVER_PLANE_ID	= 0x1613EB69u;	/* 370399817 */
static const SceUInt32 TOP_WINDOW_DEFAULT_TEX_ID	= 0xD33ECAC2u;	/* -750861822 */
static const SceUInt32 TOP_WINDOW_BLANK_TAB_TPL_ID	= 0x5E0E02BDu;	/* 1577953405 */
static const SceUInt32 TOP_WINDOW_BLANK_TAB_PLANE_ID	= 0x36BF9C0Fu;	/* 918532879 */
static const SceUInt32 TOP_WINDOW_BLANK_TAB_TEX_ID	= 0x8881EB29u;	/* -2004669911 */
static const SceUInt32 TOP_WINDOW_DIALOG_APP_TPL_ID	= 0x0DF26682u;	/* 233988610 */
static const SceUInt32 TOP_WINDOW_DIALOG_INVITE_TPL_ID	= 0xC5172754u;	/* -988329132 */
static const SceUInt32 TOP_WINDOW_DIALOG_BOX_ID		= 0x3F528975u;	/* 1062353013 */
static const SceUInt32 TOP_WINDOW_DIALOG_ICON_ID	= 0x08CBBD0Bu;	/* 147575179 */
static const SceUInt32 TOP_WINDOW_DIALOG_TITLE_ID	= 0x6D543D6Eu;	/* 1834253806 */
static const SceUInt32 TOP_WINDOW_DIALOG_FALLBACK_TEX_ID = 0xAFDF1D10u;	/* -1344341552 */
static const SceUInt32 TOP_WINDOW_HEADER_ICON_ID	= 0x1A256DF4u;	/* 438663636 */

static const SceUInt32 TOP_MSG_CLOSE_CONFLICT_APP	= 0xDBD99A90u;	/* -606497072 */
static const SceUInt32 TOP_MSG_BGDL_INSTALL_WAIT	= 0x382B4B2Eu;	/* 942373966 */
static const SceUInt32 TOP_MSG_BGDL_PATCH_WAIT		= 0xF7429AC3u;	/* -146634333 */
static const SceUInt32 TOP_MSG_POCKETSTATION_STORE	= 0xDC7DFC0Fu;	/* -595723505 */
static const SceUInt32 TOP_MSG_POCKETSTATION_BTN_STORE	= 0x66A8B84Cu;	/* 1722351884 */
static const SceUInt32 TOP_MSG_POCKETSTATION_BTN_CANCEL	= 0x7C73CE65u;	/* 2087987685 */
static const SceUInt32 TOP_MSG_HDMI_RESOLUTION_CHANGE	= 0x8F6DFEC0u;	/* -1888609472 */
static const SceUInt32 TOP_MSG_CLOSE_ALL_APPS_CONFIRM	= 0xD9C8977Du;	/* -641172515 */
static const SceUInt32 TOP_MSG_KEYPAD_ENABLE_CONFIRM	= 0x922D749Eu;	/* -1842507426 */
static const SceUInt32 TOP_MSG_BLOCKED_APP		= 0xAD0B7706u;	/* -1391789450 */
static const SceUInt32 TOP_MSG_BUSY_WAIT		= 0xA8CB8496u;	/* -1463023626 */
static const SceUInt32 TOP_MSG_STORE_SEARCH_NAMED	= 0xEEA15933u;	/* -291424269 */
static const SceUInt32 TOP_MSG_STORE_SEARCH_GENERIC	= 0xE2E5BA78u;	/* -488258472 */
static const SceUInt32 TOP_MSG_STORE_BTN_YES		= 0x2BD3D686u;	/* 735295558 */
static const SceUInt32 TOP_MSG_STORE_BTN_NO		= 0xADCE053Fu;	/* -1379034401 */
static const SceUInt32 TOP_MSG_APP_NOT_FOUND_GENERIC	= 0x351AC187u;	/* 890947911 */
static const SceUInt32 TOP_MSG_APP_NOT_FOUND_NAMED	= 0xE000A492u;	/* -536841070 */
static const SceUInt32 TOP_MSG_PSCLOUD_STORE_PROMPT	= 0x0F776B02u;	/* 259486978 */
static const SceUInt32 TOP_MSG_HOME_TITLE		= 0x145F5FE1u;	/* 341789665 */
static const SceUInt32 TOP_MSG_PS_CLOSE_PROTECTED	= 0x3C7A974Bu;	/* 1014637003 */
static const SceUInt32 TOP_MSG_VOICE_APP_CHANGED	= 0xD610B5ADu;	/* -703559987 */
static const SceUInt32 TOP_MSG_INVITATION_LAUNCH	= 0xFC441D6Fu;	/* -62645009 */
static const SceUInt32 TOP_SOUND_DIALOG_OPEN		= 0xD74A60E5u;	/* -682994459 */
static const SceUInt32 TOP_NOTICE_VOICE_ICON_ID		= 0x261784EDu;	/* 639083309 */

static const SceInt32 TIMER_CAROUSEL_CLOSE_ID		= 0x01010000;	/* 16842496 */
static const SceInt32 TIMER_CAROUSEL_OPEN_ID		= 0x01010001;	/* 16842497 */
static const SceInt32 TIMER_EDIT_MODE_ANIM_ID		= 0x01010002;	/* 16842498 */
static const SceInt32 TIMER_PARENTAL_BROWSER_ID		= 0x0200230F;	/* 33564431 */

/* Forward declarations for internal functions used by handler classes & Chunk 1 */
static void onBgAppCloseDialogButton(SceInt32 dialogId, SceInt32 button,
				     bool *handled, bool *confirmed, bool wasLocked,
				     SceInt32 pageKey, TopmenuAppProcess *pendingProc,
				     bool lockPromote);
static void onConflictAppDialogButton(SceInt32 dialogId, SceInt32 button,
				      bool *handled, bool *confirmed,
				      TopmenuAppProcess *conflictProc, bool wasLocked,
				      SceInt32 pageKey);
static void onBgdlWaitDialogButton(SceInt32 dialogId, SceInt32 button,
				   bool *handled, bool *confirmed, bool wasLocked,
				   SceInt32 pageKey);
static void onHdmiPromptDialogButton(SceInt32 dialogId, SceInt32 button,
				     bool *handled, bool *confirmed,
				     SceInt32 pageKey, SceInt32 targetHeight);
static void onHdmiCloseAllDialogButton(SceInt32 dialogId, SceInt32 button,
				       bool *handled, bool *confirmed,
				       SceInt32 pageKey, SceInt32 targetHeight);
static void onPocketStationDialogButton(SceInt32 dialogId, SceInt32 button,
					bool *handled, bool *confirmed);
static void onKeypadPromptDialogButton(SceInt32 dialogId, SceInt32 button,
				       bool *handled, bool *confirmed, SceInt32 pageKey);
static void onKeypadCloseAllDialogButton(SceInt32 dialogId, SceInt32 button,
					 bool *handled, bool *confirmed, SceInt32 pageKey);
static void onBlockedTitleDialogButton(SceInt32 dialogId, SceInt32 button,
				       bool wasLocked, SceInt32 pageKey);
static void onInvitationLaunchDialogButton(SceInt32 dialogId, SceInt32 button,
					   bool *handled, bool *confirmed,
					   const char *titleId, const LsdbAppInfoPtr *appInfo,
					   SceInt32 launchAppId);
static void onParentalBrowserTimer(SceInt32 eventType, paf::ui::Handler *self,
				   paf::ui::Event *event, void *userData);
static void onParentalBrowserFinish(SceInt32 result, TopmenuAppProcess *proc);
static void onParentalBrowserRetry(TopmenuAppProcess *proc);
static void onCarouselCloseTimer(SceInt32 eventType, paf::ui::Handler *self,
				 paf::ui::Event *event, void *userData);
static void onCarouselOpenTimer(SceInt32 eventType, paf::ui::Handler *self,
				paf::ui::Event *event, void *userData);
static void onEditModeAnimTimer(SceInt32 eventType, paf::ui::Handler *self,
				paf::ui::Event *event, void *userData);
static void onGateOpenFinish(void *userData);
static void onGateCloseFinish(void *userData);
static void onGateEffectDraw(const paf::ui::Scene::DrawArg &arg);
static void onTopWindowPsButton(TopWindow *window);
static void onTopWindowScrollKey(SceInt32 eventType, paf::ui::Handler *self,
				 paf::ui::Event *event, void *userData);
static void onTopWindowFocusChange(SceInt32 eventType, paf::ui::Handler *self,
				   paf::ui::Event *event, void *userData);
static void onTopWindowSnapChange(SceInt32 eventType, paf::ui::Handler *self,
				  paf::ui::Event *event, void *userData);
static void onTopWindowScrollCancelDrag(SceInt32 eventType, paf::ui::Handler *self,
					paf::ui::Event *event, void *userData);
static void onTopWindowArrowLeft(SceInt32 eventType, paf::ui::Handler *self,
				 paf::ui::Event *event, void *userData);
static void onTopWindowArrowRight(SceInt32 eventType, paf::ui::Handler *self,
				  paf::ui::Event *event, void *userData);
static void onTopWindowTabAdded(SceInt32 eventType, void *sender, SceInt32 pageKey, void *userData);
static void onTopWindowTabRemoved(SceInt32 eventType, void *sender, SceInt32 pageKey, void *userData);
static void onTopWindowTabsCleared(SceInt32 eventType, void *sender, SceInt32 unused, void *userData);
static void onTopWindowMainTick(void *userData);
static void onTopWindowPowerKey(void *userData);
static void onTabIconReady(paf::intrusive_ptr<paf::cache::CacheImageInterface> *iconCache,
			   SceInt32 pageKey);
static void onInviteDialogIconReady(paf::intrusive_ptr<paf::cache::CacheImageInterface> *iconCache,
				    SceInt32 dialogId);
static void onLsdbAppInfoForUri(void *job, void *userData);
static void onLsdbAppInfoForPsgm(void *job, void *userData);
static void onLsdbAppInfoForSyncUpdate(void *job, void *userData);
static void topmenuHandleAppLaunchByStatus(const char *titleId, const LsdbAppInfoPtr *appInfo,
					   const SceAppMgrAppStatus *status, SceInt32 launchAppId);
static void topmenuDispatchAppLaunch(const paf::string *titleId, const LsdbAppInfoPtr *appInfo,
				     SceInt32 launchAppId, SceInt32 searchError);
static void topmenuStartLaunchSequence(TopmenuLaunchSeq *seq, const TopmenuLaunchSeqParam *param);
static void topmenuFinishLaunchSequence(TopmenuLaunchSeq *seq);
static void topmenuCancelLaunchSequence(TopmenuLaunchSeq *seq);
static void topmenuAdvanceLaunchScroll(TopmenuLaunchSeq *seq);
static void topmenuSetLaunchSeqState(TopmenuLaunchSeq *seq, SceInt32 nextState);

namespace {

static void showWidgetNow(paf::ui::Widget *widget)
{
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xFF;
		widget->m_transition_complete = true;
		widget->DoTransition_core(1.0f, true);
	}
	ScePafWidget_FF5BC957(widget, NULL, 2, 0.0f, NULL);
}

static void showWidgetWithTimer(paf::ui::Widget *widget, paf::Timer *timer)
{
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xFF;
		widget->m_transition_complete = true;
		widget->DoTransition_core(1.0f, true);
	}
	ScePafWidget_FF5BC957(widget, timer, 2, 0.0f, NULL);
}

static void hideWidgetNow(paf::ui::Widget *widget)
{
	widget->m_transition_complete = false;
	widget->m_transition = 0xFF;
	widget->DoTransitionReverse_core(1.0f, true);
	ScePafWidget_4588699E(widget, NULL, 2, 0.0f, NULL);
}

static void hideWidgetWithTimer(paf::ui::Widget *widget, paf::Timer *timer)
{
	widget->m_transition_complete = false;
	widget->m_transition = 0xFF;
	widget->DoTransitionReverse_core(1.0f, true);
	ScePafWidget_4588699E(widget, timer, 2, 0.0f, NULL);
}

static void setTouchDefaultRegions(bool enable)
{
	paf::inputdevice::touchscreen::ActivateDefaultRegion(0, enable);
	paf::inputdevice::touchscreen::ActivateDefaultRegion(1, enable);
}

static void syncTouchDefaultRegions(TopWindow *window)
{
	bool active = window->m_touchActive ||
		      window->m_touchSuspendRequested ||
		      window->m_touchForceResume;
	setTouchDefaultRegions(active);
}

static TopmenuProcessBase *findProcessInListByAppId(TopWindow *window, SceInt32 appId)
{
	for (paf::list<TopmenuProcessBase *>::iterator it = window->m_pendingLaunchQueue.begin();
	     it != window->m_pendingLaunchQueue.end(); ++it) {
		TopmenuProcessBase *proc = *it;
		if (proc->m_appId == appId)
			return proc;
	}
	return NULL;
}

static void populateCloseAppDialogContent(SceInt32 dialogId, TopmenuProcessBase *proc)
{
	paf::ui::Widget *bodyBox = SceCommonGuiDialog_8947FC23(dialogId, 10);
	if (bodyBox == NULL || !bodyBox->IsInherit(paf::ui::Box::TypeName()))
		return;

	paf::ui::Widget *msgWidget = SceCommonGuiDialog_8947FC23(dialogId, 1);
	if (msgWidget != NULL && msgWidget->IsInherit(paf::ui::Text::TypeName())) {
		paf::ui::Text *msgText = (paf::ui::Text *)msgWidget;
		msgText->SetAdjust(0, msgText->m_adjust_y, msgText->m_adjust_z);
		msgText->SetLayoutAttribute(paf::graph::TextLayoutAttribute_TextAlign, (int32_t)0);
		msgText->SetLayoutAttribute(paf::graph::TextLayoutAttribute_AlignX, (int32_t)0);
	}

	g_topmenuPlugin->TemplateOpen(bodyBox, paf::IDParam(TOP_WINDOW_DIALOG_APP_TPL_ID),
				      paf::Plugin::TemplateOpenParam());
	paf::ui::Widget *appCard =
		bodyBox->FindChild(paf::IDParam(TOP_WINDOW_DIALOG_BOX_ID), 0);
	if (appCard == NULL)
		return;

	paf::ui::Widget *iconWidget =
		appCard->FindChild(paf::IDParam(TOP_WINDOW_DIALOG_ICON_ID), 0);
	if (iconWidget != NULL) {
		TopTabAppData *appData = topmenuProcessGetTabAppData(proc);
		if (appData != NULL)
			iconWidget->SetDrawObjMaskBit(&appData->m_ownerTab->m_iconCache, 0);
	}

	paf::ui::Widget *titleWidget =
		appCard->FindChild(paf::IDParam(TOP_WINDOW_DIALOG_TITLE_ID), 0);
	if (titleWidget != NULL) {
		const char *titleUtf8 = sceLsdbGetTitle(topmenuProcessGetAppInfo(proc));
		if (titleUtf8 != NULL) {
			paf::string utf8Str(titleUtf8);
			paf::wstring titleWide;
			paf::common::Utf8ToUtf16(utf8Str, &titleWide, 0);
			((paf::ui::Text *)titleWidget)->SetString(titleWide);
		}
	}
}

/* 0x81539A18: Store browse Yes/No dialog handler */
class TopmenuStoreBrowseHandler : public CommonGuiDialogHandler {
public:
	TopmenuStoreBrowseHandler(const char *queryValue, bool isContentQuery)
		: m_queryValue(queryValue != NULL ? queryValue : ""),
		  m_isContentQuery(isContentQuery)
	{
		paf::inputdevice::GetOwnership(0x6Fu);
	}

	/* 0x81281976 / 0x812819B8 */
	virtual ~TopmenuStoreBrowseHandler()
	{
		paf::inputdevice::ReleaseOwnership(0x6Fu);
	}

	/* 0x81281A00 */
	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		if (button != 0) {
			if (button == 4) {
				char uriBuf[1024];
				if (m_isContentQuery) {
					sce_paf_snprintf(uriBuf, sizeof(uriBuf),
							 "%sbrowse?content=%s",
							 "psns:", m_queryValue.c_str());
				} else {
					sce_paf_snprintf(uriBuf, sizeof(uriBuf),
							 "%sbrowse?product&title=%s",
							 "psns:", m_queryValue.c_str());
				}
				sceAppMgrLaunchAppByUri(0x20000u, uriBuf);
			}
			SceCommonGuiDialog_BC311DC0(dialogId);
		} else {
			topmenuDialogOnPowerRelease(dialogId);
		}
	}

private:
	paf::string m_queryValue;
	bool m_isContentQuery;
};

/* 0x81539A2C: Debugger attach failure OK handler */
class TopmenuDebuggerErrorHandler : public CommonGuiDialogHandler {
public:
	explicit TopmenuDebuggerErrorHandler(SceInt32 pageKey)
		: m_pageKey(pageKey)
	{
	}

	virtual ~TopmenuDebuggerErrorHandler() {}

	/* 0x814308B8 */
	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		(void)button;
		if (dialogId != -1)
			SceCommonGuiDialog_BC311DC0(dialogId);
		TopTab *tab = (TopTab *)g_topWindow->FindTabByPageKey(m_pageKey);
		if (tab != NULL)
			topmenuCloseTab(tab, true);
	}

private:
	SceInt32 m_pageKey;
};

/* 0x81539A40: Background app close confirm handler */
class TopmenuBgAppCloseHandler : public CommonGuiDialogHandler {
public:
	TopmenuBgAppCloseHandler(bool wasLocked, SceInt32 pageKey,
				 TopmenuAppProcess *pendingProc, bool lockPromote)
		: m_handled(false),
		  m_wasLocked(wasLocked),
		  m_confirmed(false),
		  m_lockPromote(lockPromote),
		  m_pageKey(pageKey),
		  m_pendingProc(pendingProc)
	{
		paf::inputdevice::GetOwnership(0x6Fu);
	}

	/* 0x81281A8C / 0x81281AB4 */
	virtual ~TopmenuBgAppCloseHandler()
	{
		paf::inputdevice::ReleaseOwnership(0x6Fu);
	}

	/* 0x81281AE0 */
	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		onBgAppCloseDialogButton(dialogId, button, &m_handled, &m_confirmed,
					 m_wasLocked, m_pageKey, m_pendingProc, m_lockPromote);
	}

private:
	bool m_handled;
	bool m_wasLocked;
	bool m_confirmed;
	bool m_lockPromote;
	SceInt32 m_pageKey;
	TopmenuAppProcess *m_pendingProc;
};

/* 0x81539A54: Blocked title OK dialog handler */
class TopmenuBlockedTitleHandler : public CommonGuiDialogHandler {
public:
	TopmenuBlockedTitleHandler(bool wasLocked, SceInt32 pageKey)
		: m_wasLocked(wasLocked),
		  m_pageKey(pageKey)
	{
		paf::inputdevice::GetOwnership(0x6Fu);
	}

	/* 0x81281B08 / 0x81281B30 */
	virtual ~TopmenuBlockedTitleHandler()
	{
		paf::inputdevice::ReleaseOwnership(0x6Fu);
	}

	/* 0x81281B5C */
	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		onBlockedTitleDialogButton(dialogId, button, m_wasLocked, m_pageKey);
	}

private:
	bool m_wasLocked;
	SceInt32 m_pageKey;
};

/* 0x81539A68: Conflicting attribute app close confirm handler */
class TopmenuConflictAppCloseHandler : public CommonGuiDialogHandler {
public:
	TopmenuConflictAppCloseHandler(TopmenuAppProcess *conflictProc, bool wasLocked,
				       SceInt32 pageKey)
		: m_conflictProc(conflictProc),
		  m_handled(false),
		  m_wasLocked(wasLocked),
		  m_confirmed(false),
		  m_pageKey(pageKey)
	{
	}

	virtual ~TopmenuConflictAppCloseHandler() {}

	/* 0x81281B70 */
	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		onConflictAppDialogButton(dialogId, button, &m_handled, &m_confirmed,
					  m_conflictProc, m_wasLocked, m_pageKey);
	}

private:
	TopmenuAppProcess *m_conflictProc;
	bool m_handled;
	bool m_wasLocked;
	bool m_confirmed;
	SceInt32 m_pageKey;
};

/* 0x81539A7C: Pending BGDL promote/install confirm handler */
class TopmenuBgdlWaitHandler : public CommonGuiDialogHandler {
public:
	TopmenuBgdlWaitHandler(bool wasLocked, SceInt32 pageKey)
		: m_handled(false),
		  m_wasLocked(wasLocked),
		  m_confirmed(false),
		  m_pageKey(pageKey)
	{
	}

	virtual ~TopmenuBgdlWaitHandler() {}

	/* 0x81281B9A */
	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		onBgdlWaitDialogButton(dialogId, button, &m_handled, &m_confirmed,
				       m_wasLocked, m_pageKey);
	}

private:
	bool m_handled;
	bool m_wasLocked;
	bool m_confirmed;
	SceInt32 m_pageKey;
};

/* 0x81539A90: HDMI resolution change confirm handler */
class TopmenuHdmiPromptHandler : public CommonGuiDialogHandler {
public:
	TopmenuHdmiPromptHandler(SceInt32 pageKey, SceInt32 targetHeight)
		: m_pageKey(pageKey),
		  m_targetHeight(targetHeight),
		  m_handled(false),
		  m_confirmed(false)
	{
	}

	virtual ~TopmenuHdmiPromptHandler() {}

	/* 0x81281BBA */
	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		onHdmiPromptDialogButton(dialogId, button, &m_handled, &m_confirmed,
					 m_pageKey, m_targetHeight);
	}

private:
	SceInt32 m_pageKey;
	SceInt32 m_targetHeight;
	bool m_handled;
	bool m_confirmed;
};

/* 0x81539AA4: HDMI close-all-apps confirm handler */
class TopmenuHdmiCloseAllHandler : public CommonGuiDialogHandler {
public:
	TopmenuHdmiCloseAllHandler(SceInt32 pageKey, SceInt32 targetHeight)
		: m_pageKey(pageKey),
		  m_targetHeight(targetHeight),
		  m_handled(false),
		  m_confirmed(false)
	{
	}

	virtual ~TopmenuHdmiCloseAllHandler() {}

	/* 0x81281BDE */
	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		onHdmiCloseAllDialogButton(dialogId, button, &m_handled, &m_confirmed,
					   m_pageKey, m_targetHeight);
	}

private:
	SceInt32 m_pageKey;
	SceInt32 m_targetHeight;
	bool m_handled;
	bool m_confirmed;
};

/* 0x81539AB8: PocketStation Store prompt handler */
class TopmenuPocketStationHandler : public CommonGuiDialogHandler {
public:
	explicit TopmenuPocketStationHandler(SceInt32 pageKey)
		: m_pageKey(pageKey),
		  m_handled(false),
		  m_confirmed(false)
	{
	}

	virtual ~TopmenuPocketStationHandler() {}

	/* 0x81281C00 */
	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		onPocketStationDialogButton(dialogId, button, &m_handled, &m_confirmed);
	}

private:
	SceInt32 m_pageKey;
	bool m_handled;
	bool m_confirmed;
};

/* 0x81539ACC: Busy dialog dismiss handler */
class TopmenuBusyDialogHandler : public CommonGuiDialogHandler {
public:
	virtual ~TopmenuBusyDialogHandler() {}

	/* 0x8143090C */
	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		if (button != 0) {
			SceCommonGuiDialog_BC311DC0(dialogId);
			return;
		}
		topmenuDialogOnPowerRelease(dialogId);
		g_topmenuBusyDialogId = -1;
	}
};

/* 0x81539AE0: PS-button close protected app confirm handler */
class TopmenuPsCloseAppHandler : public CommonGuiDialogHandler {
public:
	virtual ~TopmenuPsCloseAppHandler() {}

	/* 0x81430930 */
	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		(void)dialogId;
		if (button == 4)
			onTopWindowPsButton(g_topWindow);
		shellStateWatcherCloseDialog(g_topmenuPsCloseDialogId);
		g_topmenuPsCloseDialogId = -1;
	}
};

/* 0x81539AF4: Parental control blocked launch error handler */
class TopmenuParentalBlockedHandler : public CommonGuiDialogHandler {
public:
	TopmenuParentalBlockedHandler()
	{
		paf::inputdevice::GetOwnership(0x6Fu);
	}

	/* 0x8143095C / 0x81430984 */
	virtual ~TopmenuParentalBlockedHandler()
	{
		paf::inputdevice::ReleaseOwnership(0x6Fu);
	}

	/* 0x814309B0 */
	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		(void)button;
		SceCommonGuiDialog_BC311DC0(dialogId);
	}
};

/* 0x81539B08: Simple modal OK handler with input ownership */
class TopmenuOwnedOkDialogHandler : public CommonGuiDialogHandler {
public:
	TopmenuOwnedOkDialogHandler()
	{
		paf::inputdevice::GetOwnership(0x6Fu);
	}

	/* 0x814309BC / 0x814309E4 */
	virtual ~TopmenuOwnedOkDialogHandler()
	{
		paf::inputdevice::ReleaseOwnership(0x6Fu);
	}

	/* 0x814308A4 */
	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		if (button != 0)
			SceCommonGuiDialog_BC311DC0(dialogId);
		else
			topmenuDialogOnPowerRelease(dialogId);
	}
};

/* 0x81539B1C: Invitation app launch confirm handler */
class TopmenuInvitationLaunchHandler : public CommonGuiDialogHandler {
public:
	/* 0x8126F996 */
	TopmenuInvitationLaunchHandler(const char *titleId, const LsdbAppInfoPtr *appInfo,
				       SceInt32 launchAppId)
		: m_titleId(titleId != NULL ? titleId : ""),
		  m_appInfo(*appInfo),
		  m_launchAppId(launchAppId),
		  m_handled(false),
		  m_confirmed(false)
	{
		paf::inputdevice::GetOwnership(0x6Fu);
	}

	/* 0x81281C1E / 0x81281CB0 */
	virtual ~TopmenuInvitationLaunchHandler()
	{
		paf::inputdevice::ReleaseOwnership(0x6Fu);
	}

	/* 0x81281CC0 */
	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		onInvitationLaunchDialogButton(dialogId, button, &m_handled, &m_confirmed,
					       m_titleId.c_str(), &m_appInfo, m_launchAppId);
	}

private:
	paf::string m_titleId;
	LsdbAppInfoPtr m_appInfo;
	SceInt32 m_launchAppId;
	bool m_handled;
	bool m_confirmed;
};

/* 0x81539B30: key_pad enable confirm handler */
class TopmenuKeypadPromptHandler : public CommonGuiDialogHandler {
public:
	explicit TopmenuKeypadPromptHandler(SceInt32 pageKey)
		: m_pageKey(pageKey),
		  m_handled(false),
		  m_confirmed(false)
	{
	}

	virtual ~TopmenuKeypadPromptHandler() {}

	/* 0x81281CE8 */
	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		onKeypadPromptDialogButton(dialogId, button, &m_handled, &m_confirmed, m_pageKey);
	}

private:
	SceInt32 m_pageKey;
	bool m_handled;
	bool m_confirmed;
};

/* 0x81539B44: key_pad close-all-apps confirm handler */
class TopmenuKeypadCloseAllHandler : public CommonGuiDialogHandler {
public:
	explicit TopmenuKeypadCloseAllHandler(SceInt32 pageKey)
		: m_pageKey(pageKey),
		  m_handled(false),
		  m_confirmed(false)
	{
	}

	virtual ~TopmenuKeypadCloseAllHandler() {}

	/* 0x81281D06 */
	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		onKeypadCloseAllDialogButton(dialogId, button, &m_handled, &m_confirmed, m_pageKey);
	}

private:
	SceInt32 m_pageKey;
	bool m_handled;
	bool m_confirmed;
};

static inline paf::Plugin *getTopWindowPlugin(TopWindow *window)
{
	return reinterpret_cast<paf::Plugin *>(window->m_rootWidget);
}

static void setGameDialogHold(GamePluginInterface *gameIf, bool hold)
{
	if (gameIf != NULL && gameIf->func[GAME_PLUGIN_FUNC_SET_DIALOG_HOLD] != NULL) {
		typedef void (*SetHoldFn)(bool);
		reinterpret_cast<SetHoldFn>(gameIf->func[GAME_PLUGIN_FUNC_SET_DIALOG_HOLD])(hold);
	}
}

/* 0x8126F818 */
static void releaseInputBlocker(TopWindow *window)
{
	window->m_overlayPlaneRefCount--;
	if (window->m_overlayPlaneRefCount <= 0) {
		g_topWindow->m_scrollView->EnableFocusEvent(true);
		hideWidgetNow(window->m_backPlaneWidget);
	}
}

/* 0x8126FA0A */
static void acquireInputBlocker(TopWindow *window)
{
	window->m_overlayPlaneRefCount++;
	if (window->m_overlayPlaneRefCount > 0) {
		g_topWindow->m_scrollView->EnableFocusEvent(false);
		showWidgetNow(window->m_backPlaneWidget);
	}
}

/* 0x8126F8BE */
static void unregisterSystemAppId(TopWindow *window, SceInt32 appId)
{
	for (paf::list<SceInt32>::iterator it = window->m_appTerminateWaiters.begin();
	     it != window->m_appTerminateWaiters.end(); ++it) {
		if (*it == appId) {
			window->m_appTerminateWaiters.erase(it);
			topmenuEndBusy();
			GamePluginInterface *gameIf =
				(GamePluginInterface *)topmenuFindPluginInterface("game_plugin", 1);
			setGameDialogHold(gameIf, false);
			return;
		}
	}
}

/* 0x8126FADC */
static void shiftCarouselLeftAfterClose(TopWindow *window)
{
	if (window->m_tabs.size() != 1) {
		paf::math::v4 footerPos =
			window->m_homeTabWidget->GetPos(1, 0, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		window->m_homeTabWidget->SetPos(footerPos, NULL, 0x10000, 0.0f, 0, NULL, NULL);
	}

	for (paf::vector<TopTabBase *>::iterator it = window->m_tabs.begin();
	     it != window->m_tabs.end(); ++it) {
		paf::ui::Widget *tabWidget = (*it)->m_widget;
		paf::math::v4 tabPos =
			tabWidget->GetPos(1, 0, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		tabWidget->SetPos(tabPos, NULL, 0x10000, 0.0f, 0, NULL, NULL);
	}

	paf::math::v2 focusTarget = g_topWindow->m_scrollView->GetViewFocusTarget();
	focusTarget.set_x(focusTarget.extract_x() - 960.0f);
	g_topWindow->m_scrollView->SetViewFocus(focusTarget, NULL, true, true);
}

/* 0x8126FBFE */
static void setFocusedTab(TopWindow *window, TopTab *tab, bool skipIfSystemApp)
{
	window->m_currentTab = tab;
	if (skipIfSystemApp &&
	    window->m_closingApp != NULL &&
	    window->m_closingApp == window->m_foregroundApp) {
		return;
	}

	if (tab == NULL || tab->m_appData == NULL) {
		topmenuSwitchToProcess(window, NULL, true);
#if !defined(FW_TYPE_CEX)
		ApplauncherInterface *appIf = topmenuGetApplauncherInterface();
		if (appIf != NULL && appIf->func[18] != NULL) {
			SceInt32 page = ((SceInt32 (*)(void))appIf->func[18])();
			ScePafAutoTestTty_9E10E873(1, "/livespace/home_page%d", page);
		}
#endif
		return;
	}

	if (tab->m_appData->m_process.m_launchMode != 1 || tab->m_appData->m_viewKind == 1) {
		if (tab->m_appData->m_viewKind != 1) {
			topmenuSwitchToProcess(window, &tab->m_appData->m_process, true);
		} else {
			topmenuSwitchToProcess(window, NULL, true);
#if !defined(FW_TYPE_CEX)
			if (window->m_activeSnapId ==
			    tab->m_scrollView->GetSnapID(tab->m_widget)) {
				ScePafAutoTestTty_9E10E873(1, "/livespace/livearea_%s",
							   tab->m_appData->m_titleId.c_str());
			}
#endif
		}
	}
}

/* 0x8126FC4C */
static void updatePageArrows(TopWindow *window)
{
	if (!window->m_navArrowsEnabled || window->m_scrollLockCount > 0) {
		if (window->m_arrowUpVisible) {
			hideWidgetWithTimer(window->m_arrowUpWidget, new paf::Timer(0.0f));
			window->m_arrowUpVisible = false;
		}
		if (window->m_arrowDownVisible) {
			if (window->m_arrowDownWidget->m_parent != window->m_scrollView)
				window->m_arrowDownWidget->SetParent(window->m_scrollView, NULL, 0);
			hideWidgetWithTimer(window->m_arrowDownWidget, new paf::Timer(0.0f));
			window->m_arrowDownVisible = false;
		}
		return;
	}

	TopTab *currentTab = (TopTab *)g_topWindow->FindFocusedTab();
	if (currentTab == NULL)
		return;

	if (currentTab->m_index != 0) {
		if (!window->m_arrowUpVisible) {
			showWidgetWithTimer(window->m_arrowUpWidget, new paf::Timer(0.0f));
			window->m_arrowUpVisible = true;
		}
	} else if (window->m_arrowUpVisible) {
		hideWidgetWithTimer(window->m_arrowUpWidget, new paf::Timer(0.0f));
		window->m_arrowUpVisible = false;
	}

	TopTabBase *lastTab = (window->m_tabs.size() != 0) ? window->m_tabs.back() : NULL;
	if (lastTab == currentTab) {
		if (window->m_arrowDownVisible) {
			if (window->m_arrowDownWidget->m_parent != window->m_scrollView)
				window->m_arrowDownWidget->SetParent(window->m_scrollView, NULL, 0);
			hideWidgetWithTimer(window->m_arrowDownWidget, new paf::Timer(0.0f));
			window->m_arrowDownVisible = false;
		}
	} else if (!window->m_arrowDownVisible) {
		if (currentTab->m_index != 0) {
			if (window->m_arrowDownWidget->m_parent != window->m_scrollView)
				window->m_arrowDownWidget->SetParent(window->m_scrollView, NULL, 0);
		} else if (window->m_focusedNavWidget != NULL) {
			paf::math::v4 homePos =
				window->m_focusedNavWidget->GetPos(0, 0, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
			if (homePos.extract_z() != 0.0f)
				return;
			paf::ui::Widget *homeParent = window->m_focusedNavWidget->m_parent;
			if (window->m_arrowDownWidget->m_parent != homeParent)
				window->m_arrowDownWidget->SetParent(homeParent, NULL, 0);
		}
		showWidgetWithTimer(window->m_arrowDownWidget, new paf::Timer(0.0f));
		window->m_arrowDownVisible = true;
	}
}

/* 0x81270420 */
static void playGateCloseTransition(void)
{
	if (g_topWindow->m_gateEffectSurface.get() != NULL && !g_topWindow->m_gateEffectActive) {
		paf::Framework::Instance()->PlayCommonSE(38, 0.0f, false);
		g_topWindow->m_busyRefCount++;
		acquireInputBlocker(g_topWindow);
		if (g_topWindow->m_indicatorIf != NULL)
			g_topWindow->m_indicatorIf->LockButton();
		Shell_81244BB0();
		ScePafGraphics_6BBFE4F7(g_topWindow->m_gateEffectReverse,
					g_topWindow->m_gateEffectOriginX,
					g_topWindow->m_gateEffectOriginY,
					g_topWindow->m_gateEffectSurface,
					onGateCloseFinish, NULL);
		showWidgetNow(g_topWindow->m_gateEffectScene);
		g_topWindow->m_gateEffectSurface.clear();
	} else {
		if (g_topWindow->m_launchSeq.state == 1)
			topmenuAdvanceLaunchScroll(&g_topWindow->m_launchSeq);
		topmenuHelpShowLiveAreaPeel();
		shellEventServerSend(6, NULL, 0);
	}
}

/* 0x81270514 */
static void playGateOpenTransition(void)
{
	if (g_topWindow->m_gateEffectActive && g_topWindow->m_foregroundApp != NULL) {
		g_topWindow->m_foregroundApp->SendProcessCommand(1);
		return;
	}

	paf::Framework::Instance()->PlayCommonSE(37, 0.0f, false);
	g_topWindow->m_busyRefCount++;
	acquireInputBlocker(g_topWindow);
	if (g_topWindow->m_indicatorIf != NULL)
		g_topWindow->m_indicatorIf->LockButton();
	Shell_81244BB0();

	bool isColdBoot = false;
	if (g_topWindow->m_foregroundApp != NULL)
		isColdBoot = (g_topWindow->m_foregroundApp->m_appId == -1);

	paf::intrusive_ptr<paf::graph::Surface> capturedSurface;
	shellScreenshotCaptureSurface(&capturedSurface, -5);

	paf::intrusive_ptr<paf::graph::Surface> *splashSurface = NULL;
	if (isColdBoot) {
		topmenuBootScreenClearImage();
		splashSurface = topmenuBootScreenGetSurfacePtr();
	}

	g_topWindow->m_gateEffectApp = g_topWindow->m_foregroundApp;
	ScePafGraphics_16BDBC6E(isColdBoot,
				g_topWindow->m_gateEffectOriginX,
				g_topWindow->m_gateEffectOriginY,
				capturedSurface,
				splashSurface,
				onGateOpenFinish,
				(void *)(intptr_t)isColdBoot);
	showWidgetNow(g_topWindow->m_gateEffectScene);
}

/* 0x81270664 */
static void applyIndicatorBgStyle(int styleKind, const TopmenuBgLayerOption *opt, bool animate)
{
	TopmenuBgLayerOption defaultOpt;
	defaultOpt.useCustomColor = false;
	defaultOpt.style = 0;
	defaultOpt.color = 0;
	if (opt == NULL)
		opt = &defaultOpt;

	if (g_topWindow->m_bgStyleKind == styleKind &&
	    g_topWindow->m_bgColorParam == opt->style &&
	    static_cast<SceUInt32>(g_topWindow->m_bgWaveParam) == opt->color) {
		return;
	}

	g_topWindow->m_bgStyleKind = styleKind;
	g_topWindow->m_bgColorParam = opt->style;
	g_topWindow->m_bgWaveParam = static_cast<SceInt32>(opt->color);

	IndicatorPluginInterface *indIf = g_topWindow->m_indicatorIf;
	if (styleKind == 0) {
		indIf->Hide(animate);
	} else if (styleKind == 1) {
		IndicatorMode mode;
		mode.type = 0;
		mode.style = 2;
		mode.dimmed = 0;
		indIf->Show(mode, animate);
	} else if (styleKind == 2) {
		IndicatorMode mode;
		mode.type = 1;
		mode.style = opt->style;
		mode.dimmed = static_cast<int>(opt->color);
		indIf->Show(mode, animate);
	}
}

/* 0x81270818 */
static void refreshDefaultIndicatorBg(TopWindow *window)
{
	if (window->m_bgRequests.size() == 0)
		return;
	if (window->m_bgRequests.back().owner != &window->m_bgRequests)
		return;

	if (shellBootIsLaunchRestricted()) {
		topmenuSetDefaultBgStyle(0, NULL, true);
		return;
	}

	if (window->m_shellViewMode == 2 &&
	    window->m_foregroundApp != NULL &&
	    !window->m_foregroundApp->m_hasAppId) {
		return;
	}

	switch (window->m_shellViewMode) {
	case 1:
	case 3:
	case 5:
		topmenuSetDefaultBgStyle(1, NULL, true);
		break;
	case 2:
	case 4:
		if (window->m_foregroundApp == NULL ||
		    !window->m_foregroundApp->m_bgLayerOpt.useCustomColor) {
			topmenuSetDefaultBgStyle(0, NULL, true);
		} else {
			topmenuSetDefaultBgStyle(2, &window->m_foregroundApp->m_bgLayerOpt, true);
		}
		break;
	default:
		break;
	}
}

/* 0x812708B8 */
static void hideShellScenesForApp(TopWindow *window)
{
	paf::ui::Environment *env = paf::Framework::Instance()->GetEnvironmentInstance();
	env->HideSceneEntry(50);
	env->HideSceneEntry(45);
	ScePafGraphics_00B9D8CC();
	sceTouchSetProcessPrivilege(0);

	if (window->m_touchSuspendRequested) {
		window->m_touchSuspendRequested = false;
		syncTouchDefaultRegions(window);
	}
}

/* 0x8127091E */
static void showShellScenes(TopWindow *window)
{
	paf::ui::Environment *env = paf::Framework::Instance()->GetEnvironmentInstance();
	env->ShowSceneEntry(50);
	env->ShowSceneEntry(45);
	ScePafGraphics_906BE45C();
	sceTouchSetProcessPrivilege(0);

	if (!window->m_touchSuspendRequested) {
		window->m_touchSuspendRequested = true;
		setTouchDefaultRegions(true);
	}
}

/* 0x81270B56 */
static void removeAppFlagsByName(TopWindow *window, const char *appName)
{
	for (paf::list<TopmenuAppFlagEntry>::iterator it = window->m_appFlagList.begin();
	     it != window->m_appFlagList.end(); ++it) {
		if (sce_paf_strncmp(it->appName, appName, 32) == 0) {
			if ((it->flags & 0x80000ULL) != 0)
				g_topWindow->m_powerManageIf->releasePowerSwitch();
			window->m_appFlagList.erase(it);
			return;
		}
	}
}

/* 0x81270C1E */
static int destroyProcessAndBgPartner(TopWindow *window, TopmenuAppProcess *proc)
{
	(void)window;
	if (proc != NULL)
		topmenuProcessCommand(proc, 5);

	if (proc->m_appInfo.get() != NULL) {
		const char *bgTitleId = SceLsdb_457F369E(&proc->m_appInfo);
		if (bgTitleId != NULL) {
			SceUInt64 flags = topmenuGetAppFlags(g_topWindow, bgTitleId);
			if ((flags & 0x4000000ULL) == 0) {
				sceAppMgrDestroyAppByName(bgTitleId);
				removeAppFlagsByName(g_topWindow, bgTitleId);
				if (proc->m_tabAppData != NULL) {
					LiveAreaPluginInterface *laIf = topmenuGetLiveAreaInterface();
					if (laIf != NULL) {
						SceInt32 pageKey = proc->m_tabAppData->m_ownerTab->m_pageKey;
						laIf->clearGateSurface(pageKey);
						laIf->refreshPage(pageKey);
					}
				}
			}
		}
	}
	return 1;
}

/* 0x81270CF2 */
static bool checkConflictingAttributeApp(TopWindow *window, SceUInt64 attrMask,
					 bool showPrompt, bool requireSelfMatch)
{
	if (shellStartupGetIduMode() == 1)
		return false;

	TopmenuAppProcess *activeProc = window->m_foregroundApp;
	if (activeProc == NULL || activeProc->m_tabAppData == NULL)
		return false;
	if (activeProc->m_launchMode == 1 || activeProc->m_launchMode == 2 ||
	    activeProc->m_launchMode == 3) {
		return false;
	}
	if (activeProc->m_tabAppData->m_viewKind != 1)
		return false;

	(void)SceLsdb_0B16523B(&activeProc->m_appInfo);
	if (requireSelfMatch && (static_cast<SceUInt64>(SceLsdb_0B16523B(&activeProc->m_appInfo)) & attrMask) == 0)
		return false;

	TopTabAppData *conflictData = NULL;
	for (paf::vector<TopTabBase *>::iterator it = g_topWindow->m_tabs.begin();
	     it != g_topWindow->m_tabs.end(); ++it) {
		TopTab *tab = (TopTab *)(*it);
		if (tab == NULL || tab->m_appData == NULL)
			continue;
		TopmenuAppProcess *candidate = &tab->m_appData->m_process;
		if (candidate == activeProc)
			continue;
		if (topmenuProcessIsSpawnedOrRunning(candidate) &&
		    (static_cast<SceUInt64>(SceLsdb_0B16523B(&candidate->m_appInfo)) & attrMask) != 0) {
			conflictData = tab->m_appData;
			break;
		}
	}

	if (conflictData == NULL)
		return false;

	if (!showPrompt) {
		destroyProcessAndBgPartner(window, &conflictData->m_process);
		return true;
	}

	CommonGuiLayoutDialogParam dlgParam;
	dlgParam.layout = SceCommonGuiDialog_92752DE2;
	dlgParam.priority = 33;
	dlgParam.resourcePlugin = g_topmenuPlugin;
	dlgParam.text = getTopWindowPlugin(window)->GetString(paf::IDParam(TOP_MSG_CLOSE_CONFLICT_APP));

	bool wasLocked = g_topWindow->m_powerManageIf->isWakeApp(
		activeProc->m_titleId.c_str()) != 0;
	SceInt32 targetPageKey = activeProc->m_tabAppData->m_ownerTab->m_pageKey;
	dlgParam.handler = new TopmenuConflictAppCloseHandler(&conflictData->m_process,
							      wasLocked, targetPageKey);
	dlgParam.soundId = TOP_SOUND_DIALOG_OPEN;
	dlgParam.notifyClose = true;

	SceInt32 dialogId = SceCommonGuiDialog_CCFCD9A6(&dlgParam);
	if (dialogId >= 0) {
		topmenuDialogRegister(dialogId);
		topmenuDialogOnPowerHold(dialogId);
		populateCloseAppDialogContent(dialogId, &conflictData->m_process);
	}
	return true;
}

/* 0x812712BA */
static bool showBgdlPendingConfirmDialog(TopWindow *window, bool isPatchTask)
{
	CommonGuiLayoutDialogParam dlgParam;
	dlgParam.layout = SceCommonGuiDialog_6CD802FC;
	dlgParam.priority = 33;
	dlgParam.resourcePlugin = g_topmenuPlugin;
	dlgParam.text = getTopWindowPlugin(window)->GetString(
		paf::IDParam(isPatchTask ? TOP_MSG_BGDL_PATCH_WAIT : TOP_MSG_BGDL_INSTALL_WAIT));

	if (window->m_foregroundApp != NULL) {
		bool wasLocked = g_topWindow->m_powerManageIf->isWakeApp(
			window->m_foregroundApp->m_titleId.c_str()) != 0;
		SceInt32 pageKey = window->m_foregroundApp->m_tabAppData->m_ownerTab->m_pageKey;
		dlgParam.handler = new TopmenuBgdlWaitHandler(wasLocked, pageKey);
	} else {
		dlgParam.handler = new TopmenuBgdlWaitHandler(false, -1);
	}
	dlgParam.soundId = TOP_SOUND_DIALOG_OPEN;
	dlgParam.notifyClose = true;

	SceInt32 dialogId = SceCommonGuiDialog_CCFCD9A6(&dlgParam);
	if (dialogId < 0)
		return false;

	topmenuDialogRegister(dialogId);
	topmenuDialogOnPowerHold(dialogId);
	return true;
}

/* 0x8127156E */
static bool checkParentalWebBrowserLaunch(TopWindow *window)
{
	TopmenuAppProcess *activeProc = window->m_foregroundApp;
	if (activeProc == NULL || activeProc->m_tabAppData == NULL)
		return false;
	if (activeProc->m_tabAppData->m_viewKind != 1)
		return false;
	if (sce_paf_strncmp(activeProc->m_titleId.c_str(), "NPXS10094",
			    sce_paf_strlen("NPXS10094")) != 0 ||
	    window->m_homeInputBlocked) {
		return false;
	}
	if (g_topmenuParentalBypassOnce) {
		g_topmenuParentalBypassOnce = false;
		return false;
	}

	bool hasPasscode = false;
	SceUInt16 promptMode = 2;
	SceUInt16 checkKind = 1;
	if (parentalGetRemainingSec() != 0) {
		paf::ui::Handler *viewParent = g_topWindow->m_scrollView->GetViewParent();
		viewParent->SetTimerListener(TIMER_PARENTAL_BROWSER_ID, 1000.0f,
					     new paf::ui::EventCBListener(onParentalBrowserTimer, window));
		checkKind = 0;
	}

	power_manage::AuthInterface *parentalIf = window->m_powerManageIf->getAuthInterface();
	if (parentalIf == NULL)
		return false;

	parentalIf->isPasscodeEnabled(checkKind, &hasPasscode);
	if (!hasPasscode)
		promptMode = 0;
	parentalIf->reloadRegistry(true);
	parentalIf->startAuth(checkKind, promptMode,
			      reinterpret_cast<power_manage::AuthResultCallback>(
				      onParentalBrowserFinish),
			      &activeProc->m_tabAppData->m_process,
			      reinterpret_cast<power_manage::AuthDialogOpenCallback>(
				      onParentalBrowserRetry));
	return true;
}

/* 0x812716A8 */
static bool checkPocketStationPrompt(TopWindow *window)
{
	TopmenuAppProcess *activeProc = window->m_foregroundApp;
	if (activeProc == NULL || activeProc->m_appInfo.get() == NULL)
		return false;

	const char *category = sceLsdbGetCategory(&activeProc->m_appInfo);
	if (sce_paf_strcmp("ME", category) != 0)
		return false;

	SceInt32 launchAppId = topmenuProcessGetPendingLaunchId(activeProc);
	char launchParam[1024];
	if (launchAppId == -1 ||
	    sceAppMgrPeekLaunchParamForShell(launchAppId, launchParam, sizeof(launchParam)) != 0 ||
	    sce_paf_private_strtok(launchParam, "pocketstation=-1") == NULL) {
		return false;
	}

	topmenuProcessClearSpawnJob(activeProc);

	CommonGuiLayoutDialogParam dlgParam;
	dlgParam.layout = SceCommonGuiDialog_92752DE2;
	dlgParam.priority = 33;
	dlgParam.resourcePlugin = g_topmenuPlugin;
	dlgParam.text = g_topmenuPlugin->GetString(paf::IDParam(TOP_MSG_POCKETSTATION_STORE));
	dlgParam.handler = new TopmenuPocketStationHandler(
		activeProc->m_tabAppData->m_ownerTab->m_pageKey);
	dlgParam.soundId = TOP_SOUND_DIALOG_OPEN;
	dlgParam.notifyClose = true;

	SceInt32 dialogId = SceCommonGuiDialog_CCFCD9A6(&dlgParam);
	if (dialogId < 0)
		return false;

	topmenuDialogRegister(dialogId);
	topmenuDialogOnPowerHold(dialogId);

	paf::ui::Widget *okBtn = SceCommonGuiDialog_8947FC23(dialogId, 12);
	paf::ui::Widget *cancelBtn = SceCommonGuiDialog_8947FC23(dialogId, 13);
	if (okBtn != NULL) {
		paf::wstring label(g_topmenuPlugin->GetString(
			paf::IDParam(TOP_MSG_POCKETSTATION_BTN_STORE)));
		((paf::ui::Text *)okBtn)->SetString(label);
	}
	if (cancelBtn != NULL) {
		paf::wstring label(g_topmenuPlugin->GetString(
			paf::IDParam(TOP_MSG_POCKETSTATION_BTN_CANCEL)));
		((paf::ui::Text *)cancelBtn)->SetString(label);
	}
	return true;
}

/* 0x81271A92 */
static bool checkHdmiResolutionPrompt(TopWindow *window)
{
	if (paf::system::Model::GetType() != 1)
		return false;

	TopmenuAppProcess *activeProc = window->m_foregroundApp;
	if (activeProc == NULL || activeProc->m_appInfo.get() == NULL)
		return false;

	const char *category = sceLsdbGetCategory(&activeProc->m_appInfo);
	if (sce_paf_strcmp("ME", category) != 0)
		return false;

	if (sceDisplayGetPrimaryHead() != 1)
		return false;

	SceDisplayResolutionInfo resInfo;
	resInfo.size = sizeof(resInfo);
	if (sceDisplayGetResolutionInfoInternal(1, &resInfo) != 0)
		return false;
	if (resInfo.height != 1080)
		return false;

	AvConfigMonitorInfo monitorInfo;
	monitorInfo.size = sizeof(monitorInfo);
	if (sceAVConfigHdmiGetMonitorInfo(&monitorInfo) != 0)
		return false;

	paf::wstring msgText;
	SceInt32 targetHeight = 480;
	const wchar_t *fmt = g_topmenuPlugin->GetString(
		paf::IDParam(TOP_MSG_HDMI_RESOLUTION_CHANGE));
	if ((monitorInfo.videoFormats[0].supported & 4u) != 0) {
		msgText = shellFormatText(fmt, L"720p");
		targetHeight = 720;
	} else {
		msgText = shellFormatText(fmt, L"480p");
	}

	CommonGuiLayoutDialogParam dlgParam;
	dlgParam.layout = SceCommonGuiDialog_6CD802FC;
	dlgParam.priority = 33;
	dlgParam.resourcePlugin = g_topmenuPlugin;
	dlgParam.text = msgText;
	dlgParam.handler = new TopmenuHdmiPromptHandler(
		activeProc->m_tabAppData->m_ownerTab->m_pageKey, targetHeight);
	dlgParam.soundId = TOP_SOUND_DIALOG_OPEN;
	dlgParam.notifyClose = true;

	SceInt32 dialogId = SceCommonGuiDialog_CCFCD9A6(&dlgParam);
	if (dialogId < 0)
		return false;

	topmenuDialogRegister(dialogId);
	topmenuDialogOnPowerHold(dialogId);
	return true;
}

/* 0x81271E78 */
static bool checkBrowserBgConflictPrompt(TopWindow *window)
{
	if (shellStartupGetIduMode() == 1)
		return false;

	TopmenuAppProcess *activeProc = window->m_foregroundApp;
	if (activeProc == NULL || activeProc->m_tabAppData == NULL)
		return false;
	if (activeProc->m_launchMode == 1 || activeProc->m_launchMode == 2 ||
	    activeProc->m_launchMode == 3) {
		return false;
	}
	if (activeProc->m_tabAppData->m_viewKind != 1)
		return false;
	if (sce_paf_strcmp(activeProc->m_titleId.c_str(), "NPXS10003") != 0)
		return false;

	SceInt32 bgAppId = sceAppMgrGetAppIdByAppId(-3);
	if (bgAppId == -1)
		return false;

	TopmenuProcessBase *bgProc = findProcessInListByAppId(g_topWindow, bgAppId);
	if (bgProc == NULL || bgProc == activeProc)
		return false;

	if (SceLsdb_9D27A87D(topmenuProcessGetAppInfo(bgProc)) == 1) {
		if (!topmenuIsLaunchSeqActive(&g_topWindow->m_launchSeq) ||
		    !window->m_launchSeq.isWebApp) {
			return checkConflictingAttributeApp(window, 0x40000ULL, true, false);
		}
	} else {
		topmenuProcessCommand(bgProc, 5);
	}
	return false;
}

/* 0x81271F72 */
static bool promptCloseBgApp(TopWindow *window, bool forceDestroyNonGame,
			     TopmenuAppProcess *pendingProc, bool lockPromote)
{
	if (pendingProc == NULL && window->m_foregroundApp == NULL)
		return false;

	SceInt32 bgAppId = sceAppMgrGetAppIdByAppId(-3);
	if (bgAppId == -1)
		return false;

	TopmenuProcessBase *bgProc = findProcessInListByAppId(g_topWindow, bgAppId);
	if (bgProc == NULL || (pendingProc == NULL && bgProc == window->m_foregroundApp))
		return false;

	if (SceLsdb_9D27A87D(topmenuProcessGetAppInfo(bgProc)) != 1) {
		if (forceDestroyNonGame) {
			topmenuSetPromoteWaitLock(true, topmenuProcessGetTitleId(bgProc));
			topmenuProcessCommand(bgProc, 5);
		}
		return false;
	}

	CommonGuiLayoutDialogParam dlgParam;
	dlgParam.layout = SceCommonGuiDialog_92752DE2;
	dlgParam.priority = 33;
	dlgParam.resourcePlugin = g_topmenuPlugin;
	dlgParam.text = getTopWindowPlugin(window)->GetString(paf::IDParam(TOP_MSG_CLOSE_CONFLICT_APP));

	SceInt32 pageKey = -1;
	bool wasLocked = false;
	if (window->m_foregroundApp != NULL) {
		pageKey = window->m_foregroundApp->m_tabAppData->m_ownerTab->m_pageKey;
		wasLocked = g_topWindow->m_powerManageIf->isWakeApp(
			window->m_foregroundApp->m_titleId.c_str()) != 0;
	}

	dlgParam.handler = new TopmenuBgAppCloseHandler(wasLocked, pageKey,
							pendingProc, lockPromote);
	dlgParam.soundId = TOP_SOUND_DIALOG_OPEN;
	dlgParam.notifyClose = true;

	SceInt32 dialogId = SceCommonGuiDialog_CCFCD9A6(&dlgParam);
	if (dialogId >= 0) {
		topmenuDialogRegister(dialogId);
		topmenuDialogOnPowerHold(dialogId);
		populateCloseAppDialogContent(dialogId, bgProc);
	}
	return true;
}

/* 0x81272526 */
static bool hasConflictingExclusiveBgApp(TopWindow *window, bool ignoreActiveProc)
{
	if (shellStartupGetIduMode() == 1)
		return false;

	SceInt32 bgAppId = sceAppMgrGetAppIdByAppId(-3);
	if (bgAppId == -1)
		return false;

	TopmenuProcessBase *bgProc = findProcessInListByAppId(g_topWindow, bgAppId);
	if (bgProc == NULL || (ignoreActiveProc && bgProc == window->m_foregroundApp))
		return false;

	SceInt64 exclusiveKind = SceLsdb_43153457(topmenuProcessGetAppInfo(bgProc));
	if (exclusiveKind == 0)
		return false;

	const char *activeTitle = window->m_foregroundApp->m_titleId.c_str();
	if (sce_paf_strcmp(activeTitle, "NPXS10015") != 0) {
		if (sce_paf_strcmp(activeTitle, "NPXS10104") != 0)
			return true;
	} else if (exclusiveKind != 1 && exclusiveKind != 2) {
		return true;
	}
	return false;
}

/* 0x812725E8 */
static bool checkBgAppConflictPrompt(TopWindow *window)
{
	if (shellStartupGetIduMode() == 1)
		return false;

	TopmenuAppProcess *activeProc = window->m_foregroundApp;
	if (activeProc == NULL || activeProc->m_tabAppData == NULL)
		return false;
	if (activeProc->m_launchMode == 1 || activeProc->m_launchMode == 2 ||
	    activeProc->m_launchMode == 3) {
		return false;
	}
	if (activeProc->m_tabAppData->m_viewKind != 1)
		return false;

	if (SceLsdb_849E8DC6(&activeProc->m_appInfo) == 0)
		return promptCloseBgApp(g_topWindow, false, NULL, false);
	if (hasConflictingExclusiveBgApp(window, true))
		return promptCloseBgApp(g_topWindow, false, NULL, true);
	return false;
}

/* 0x81272652 */
static bool checkKeypadEnablePrompt(TopWindow *window)
{
	TopmenuAppProcess *activeProc = window->m_foregroundApp;
	if (activeProc == NULL || activeProc->m_titleId.c_str() == NULL ||
	    activeProc->m_tabAppData == NULL || activeProc->m_tabAppData->m_viewKind != 1) {
		return false;
	}

	const char *titleId = activeProc->m_titleId.c_str();
	if (sce_paf_strcmp("PCSI00013", titleId) != 0 &&
	    sce_paf_strcmp("PCSI00012", titleId) != 0) {
		return false;
	}

	paf::ui::Environment *env = paf::Framework::Instance()->GetEnvironmentInstance();
	if (env->m_focus_enabled)
		return false;

	CommonGuiLayoutDialogParam dlgParam;
	dlgParam.layout = SceCommonGuiDialog_6CD802FC;
	dlgParam.priority = 33;
	dlgParam.resourcePlugin = g_topmenuPlugin;
	dlgParam.text = g_topmenuPlugin->GetString(paf::IDParam(TOP_MSG_KEYPAD_ENABLE_CONFIRM));
	dlgParam.handler = new TopmenuKeypadPromptHandler(
		activeProc->m_tabAppData->m_ownerTab->m_pageKey);
	dlgParam.soundId = TOP_SOUND_DIALOG_OPEN;
	dlgParam.notifyClose = true;

	SceInt32 dialogId = SceCommonGuiDialog_CCFCD9A6(&dlgParam);
	if (dialogId < 0)
		return false;

	topmenuDialogRegister(dialogId);
	topmenuDialogOnPowerHold(dialogId);
	return true;
}

#if FW_SINCE(0x03650000)
/* 0x812728CE */
static bool checkBlockedTitlePrompt(TopWindow *window)
{
	TopmenuAppProcess *activeProc = window->m_foregroundApp;
	if (activeProc == NULL || activeProc->m_tabAppData == NULL ||
	    activeProc->m_tabAppData->m_viewKind == 0) {
		return false;
	}

	const char *titleId = activeProc->m_titleId.c_str();
	if (titleId == NULL || !topmenuIsBlockedLaunchTitle(titleId))
		return false;

	CommonGuiLayoutDialogParam dlgParam;
	dlgParam.layout = SceCommonGuiDialog_AF75CA6D;
	dlgParam.priority = 33;
	dlgParam.resourcePlugin = g_topmenuPlugin;
	dlgParam.text = getTopWindowPlugin(window)->GetString(paf::IDParam(TOP_MSG_BLOCKED_APP));

	SceInt32 pageKey = activeProc->m_tabAppData->m_ownerTab->m_pageKey;
	bool wasLocked = g_topWindow->m_powerManageIf->isWakeApp(titleId) != 0;
	dlgParam.handler = new TopmenuBlockedTitleHandler(wasLocked, pageKey);
	dlgParam.soundId = TOP_SOUND_DIALOG_OPEN;
	dlgParam.notifyClose = true;

	SceInt32 dialogId = SceCommonGuiDialog_CCFCD9A6(&dlgParam);
	if (dialogId >= 0) {
		topmenuDialogRegister(dialogId);
		topmenuDialogOnPowerHold(dialogId);
	}
	return true;
}
#endif

/* 0x81272AEE */
static void detachSystemAppProcess(TopWindow *window)
{
	TopmenuAppProcess *sysProc = window->m_closingApp;
	window->m_closingAppPending = false;
	window->m_closingApp = NULL;
	if (sysProc == NULL)
		return;

	topmenuBootScreenHide();
	if (g_topWindow->m_foregroundApp == sysProc)
		topmenuBootScreenCaptureClose(sysProc);

	if (sysProc->m_suspended) {
		topmenuProcessResetAndCancel(sysProc);
		sysProc->DestroyOrDefer();
		return;
	}

	if (!window->m_skipDestroyCheck) {
		SceAppMgrAppStatus status;
		if (sceAppMgrGetStatusByAppId(sysProc->m_appId, &status) == 0 &&
		    (status.launchMode & 0x40000u) != 0) {
			if (window->m_bgLaunchingApp != NULL &&
			    window->m_bgLaunchingApp->m_appId == status.launchSourceAppId) {
				window->m_bgLaunchingApp->SendProcessCommand(5);
				window->m_bgLaunchingApp->DestroyOrDefer();
				window->m_bgLaunchingApp = NULL;
			}
		}
		sysProc->SendProcessCommand(5);
		sysProc->DestroyOrDefer();
		return;
	}

	sysProc->SendProcessCommand(2);
	if (window->m_bgLaunchingApp != NULL)
		window->m_bgLaunchingApp->DestroyOrDefer();
	window->m_bgLaunchingApp = sysProc;
}

/* 0x81272EFC */
static void resetLaunchSeq(TopmenuLaunchSeq *seq)
{
	seq->notifyApplauncher = false;
	seq->immediateStart = false;
	seq->holdBusy = false;
	seq->openPatchChk = false;
	seq->isWebApp = false;
	seq->skipScroll = false;
	seq->autoStart = false;
	seq->state = 0;
	seq->closePageKey = -1;
	seq->targetPageKey = -1;
}

/* 0x81272F1A */
static void computeTabScrollTransform(TopWindow *window, paf::math::v4 *outPos,
				      paf::math::v4 *outRot, const paf::math::v4 *tabWorldPos,
				      SceInt32 tabIndex)
{
	(void)window;
	float posX = tabWorldPos->extract_x();
	if (posX < 0.0f)
		posX = 0.0f;

	float ratio = -posX / 960.0f;
	if (ratio > 1.0f)
		ratio = 1.0f;
	else if (ratio < -1.0f)
		ratio = -1.0f;

	/* 0x3F860A92 == pi / 3 (~1.0471976 rad) */
	*outRot = paf::math::v4(0.0f, 1.0471976f * ratio, 0.0f, 0.0f);

	if (tabIndex == 1 && ratio < 0.0f)
		ratio = 0.0f;

	float posZ = ((ratio < 0.0f) ? -ratio : ratio) * -200.0f;
	*outPos = paf::math::v4(0.0f, 0.0f, posZ, 1.0f);
}

/* 0x81273D1A */
static void ensureModeWatermarkText(TopWindow *window)
{
	if ((shellStartupGetIduMode() != 1 && !shellStartupIsTestMode()) ||
	    window->m_iduOverlayWidget != NULL) {
		return;
	}

	paf::ui::Text *label = new paf::ui::Text(g_topWindow->m_scrollView, NULL);
	if (shellStartupGetIduMode() == 1)
		label->SetString(paf::wstring(L"DEMO MODE"));
	else
		label->SetString(paf::wstring(L"SHOW MODE"));

	paf::math::v4 textColor(0.9f, 0.9f, 0.9f, 1.0f);
	label->SetStyleAttribute(paf::graph::TextStyleAttribute_Color, 0, 0, textColor);
	label->SetStyleAttribute(paf::graph::TextStyleAttribute_Shadow, 0, 0, true);
	label->SetStyleAttribute(paf::graph::TextStyleAttribute_Bold, 0, 0, true);
	scePafWidgetSetFontSize(label, 28.0f, paf::graph::TextStyleAttribute_Point, 0, 0);
	label->SetAlign(2, 1, 0, NULL);
	label->SetAdjust(2, 2, 0);
	label->SetPos(paf::math::v4(480.0f, -258.0f, 0.0f, 0.0f),
		      NULL, 0x10000, 0.0f, 0, NULL, NULL);
	window->m_iduOverlayWidget = label;
}

/* 0x81273ED6 */
static void loadHomeBgmFromTheme(void)
{
	paf::string themeBgmPath;
	const char *bgmUri = "pd0:data/systembgm/home.at9";

	paf::common::SharedPtr<LsdbThemeInfo> *themeInfo = shellThemeGetInfo();
	if (themeInfo != NULL && themeInfo->get() != NULL &&
	    (*themeInfo)->m_homeProperty.m_bgmFilePath.size() != 0) {
		shellThemeInfoGetFilePath(themeInfo->get(), &themeBgmPath,
					  (*themeInfo)->m_homeProperty.m_bgmFilePath);
		bgmUri = themeBgmPath.c_str();
	}

	char uriBuf[1025];
	SceUInt8 loadOpt[16];
	sce_paf_memset(uriBuf, 0, sizeof(uriBuf));
	sce_paf_memset(loadOpt, 0, sizeof(loadOpt));
	sce_paf_strncpy(uriBuf, bgmUri, 1024);
	*(SceInt32 *)&loadOpt[0] = -1;
	loadOpt[4] = 0;

	if (musicCoreBgmLoadUri(uriBuf, loadOpt) == 0) {
		if (g_topWindow->m_powerSuspendMode == 2)
			musicCoreBgmControl(0, 0);
		g_topWindow->m_powerSuspendMode = 0;
	}
}

/* 0x8127407A */
static void createTopmenuRenderTarget(SceGxmRenderTarget **outTarget,
				      SceUInt16 width, SceUInt16 height)
{
	SceGxmRenderTargetParams rtParams;
	sce_paf_memset(&rtParams, 0, sizeof(rtParams));
	rtParams.width = width;
	rtParams.height = height;
	rtParams.flags = 0;
	rtParams.scenesPerFrame = 1;
	rtParams.multisampleMode = SCE_GXM_MULTISAMPLE_NONE;
	rtParams.multisampleLocations = 0;
	rtParams.driverMemBlock = -1;

	SceUInt32 driverMemSize = 0;
	sceGxmGetRenderTargetMemSize(&rtParams, &driverMemSize);
	rtParams.driverMemBlock = sceKernelAllocMemBlock(
		"SceShellRenderTarget", 0x0C208060u, driverMemSize, NULL);
	sceGxmCreateRenderTarget(&rtParams, outTarget);
}

} /* namespace */

/* 0x8126F882 */
void topmenuEndBusy(void)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	g_topWindow->m_busyRefCount--;
	releaseInputBlocker(g_topWindow);
	if (g_topWindow->m_indicatorIf != NULL)
		g_topWindow->m_indicatorIf->UnlockButton();
	paf::thread::RMutex::main_thread_mutex.Unlock();
}

/* 0x8126F94A */
void topmenuCancelPendingAppLaunch(int launchAppId)
{
	if (launchAppId != -1) {
		shellEventServerSend(12, NULL, launchAppId);
		sceAppMgrLaunchAppCancel(launchAppId);
		unregisterSystemAppId(g_topWindow, launchAppId);
	}
}

/* 0x8126F976 */
ApplauncherInterface *topmenuGetApplauncherInterface(void)
{
	if (g_topWindow->m_applauncherIf != NULL)
		return g_topWindow->m_applauncherIf;
	return (ApplauncherInterface *)topmenuFindPluginInterface("applauncher_plugin", 1);
}

/* 0x8126FA70 */
static void topmenuAdvanceLaunchScroll(TopmenuLaunchSeq *seq)
{
	if (seq->state != 1 && seq->state != 0 && seq->state != 4)
		return;

	seq->state = 2;
	TopTab *focused = g_topWindow->m_currentTab;
	TopTab *target = (TopTab *)g_topWindow->FindTabByPageKey(seq->targetPageKey);
	if (target != NULL) {
		if ((focused == NULL || focused->m_pageKey != seq->targetPageKey) &&
		    !seq->skipScroll) {
			g_topWindow->ScrollToTab(target, 500.0f);
		}
	} else {
		topmenuSetLaunchSeqState(seq, 0);
	}
}

/* 0x812701BA */
void topmenuCloseTabWithNext(TopTab *tab, bool animate, TopTabBase ***outNextIter)
{
	SceInt32 removedIdx = tab->m_index;
	SceInt32 removedSnapId = tab->m_scrollView->GetSnapID(tab->m_widget);

	TopTabBase **nextIter = NULL;
	g_topWindow->RemoveTab(&nextIter, tab);
	if (outNextIter != NULL)
		*outNextIter = nextIter;

	updatePageArrows(g_topWindow);

	if (tab->m_pageKey == g_topWindow->m_launchSeq.targetPageKey) {
		topmenuHelpCancelLiveAreaIntro();
		topmenuSetLaunchSeqState(&g_topWindow->m_launchSeq, 0);
	} else if (tab->m_pageKey == g_topWindow->m_launchSeq.closePageKey) {
		g_topWindow->m_launchSeq.closePageKey = -1;
	}

	SceInt32 prevIdx = removedIdx - 1;
	if (g_topWindow->m_editModeActive)
		animate = false;

	if (tab == g_topWindow->m_currentTab && prevIdx >= 0)
		setFocusedTab(g_topWindow, (TopTab *)g_topWindow->m_tabs[prevIdx], true);

	if (animate) {
		if (prevIdx >= 0)
			g_topWindow->ScrollToTabAnimated(g_topWindow->m_tabs[prevIdx]);
	} else if (g_topWindow->m_activeSnapId != removedSnapId) {
		TopTab *curTab = (TopTab *)g_topWindow->FindTabBySnapId(g_topWindow->m_activeSnapId);
		if (curTab != NULL && prevIdx < curTab->m_index)
			shiftCarouselLeftAfterClose(g_topWindow);
	}

	delete tab;
}

void topmenuCloseTab(TopTab *tab, bool animate)
{
	topmenuCloseTabWithNext(tab, animate, NULL);
}

/* 0x812702BA */
static void topmenuCancelLaunchSequence(TopmenuLaunchSeq *seq)
{
	if (seq->closePageKey != -1) {
		TopTab *closingTab = (TopTab *)g_topWindow->FindTabByPageKey(seq->closePageKey);
		if (closingTab != NULL) {
			seq->closePageKey = -1;
			topmenuCloseTab(closingTab, g_topWindow->m_currentTab == closingTab);
		}
		if (shellStartupGetIduMode() == 1 &&
		    g_topWindow->m_tabs.size() != 0 &&
		    seq->targetPageKey == g_topWindow->m_tabs[0]->m_pageKey) {
			while (g_topWindow->m_tabs.size() > 1) {
				TopTab *tailTab = (TopTab *)g_topWindow->m_tabs.back();
				if (tailTab != NULL)
					topmenuCloseTab(tailTab, false);
			}
		}
	}

	if (seq->holdBusy)
		topmenuEndBusy();

	bool notifyLauncher = seq->notifyApplauncher;
	bool wasLaunching = seq->autoStart;
	seq->state = 0;
	seq->targetPageKey = -1;
	seq->autoStart = false;
	seq->closePageKey = -1;
	seq->holdBusy = false;
	seq->isWebApp = false;
	seq->openPatchChk = false;
	seq->immediateStart = false;
	seq->notifyApplauncher = false;

	if (notifyLauncher) {
		ApplauncherInterface *launcherIf = topmenuGetApplauncherInterface();
		if (launcherIf != NULL)
			launcherIf->func[8]();
	}
	if (notifyLauncher || wasLaunching)
		setGameDialogHold(g_topWindow->m_gameIf, false);
}

/* 0x812703EE */
static void topmenuSetLaunchSeqState(TopmenuLaunchSeq *seq, SceInt32 nextState)
{
	switch (nextState) {
	case 0:
		topmenuCancelLaunchSequence(seq);
		break;
	case 1:
		seq->state = 1;
		break;
	case 2:
		topmenuAdvanceLaunchScroll(seq);
		break;
	case 3:
		seq->state = 3;
		break;
	case 4:
		seq->state = 4;
		break;
	default:
		break;
	}
}

/* 0x81270644 */
LiveAreaPluginInterface *topmenuGetLiveAreaInterface(void)
{
	if (g_topWindow->m_liveAreaIf != NULL)
		return g_topWindow->m_liveAreaIf;
	return (LiveAreaPluginInterface *)topmenuFindPluginInterface("livearea_plugin", 1);
}

/* 0x81270738 */
void topmenuSetDefaultBgStyle(int styleKind, const TopmenuBgLayerOption *opt, bool animate)
{
	TopmenuBgLayerOption defaultOpt;
	defaultOpt.useCustomColor = false;
	defaultOpt.style = 0;
	defaultOpt.color = 0;

	for (paf::vector<TopmenuBgRequest>::iterator it = g_topWindow->m_bgRequests.begin();
	     it != g_topWindow->m_bgRequests.end(); ++it) {
		if (it->owner == g_topWindow) {
			const TopmenuBgLayerOption *src = (opt != NULL) ? opt : &defaultOpt;
			it->styleKind = styleKind;
			it->style = src->style;
			it->color = src->color;
			return;
		}
	}

	for (paf::vector<TopmenuBgRequest>::iterator it = g_topWindow->m_bgRequests.begin();
	     it != g_topWindow->m_bgRequests.end(); ++it) {
		if (it->owner == &g_topWindow->m_bgRequests) {
			const TopmenuBgLayerOption *src = (opt != NULL) ? opt : &defaultOpt;
			it->styleKind = styleKind;
			it->style = src->style;
			it->color = src->color;
			break;
		}
	}

	if (g_topWindow->m_bgRequests.size() != 0 &&
	    g_topWindow->m_bgRequests.back().owner == &g_topWindow->m_bgRequests) {
		applyIndicatorBgStyle(styleKind, opt, animate);
	}
}

/* 0x81270968 */
void topmenuSetShellViewMode(TopWindow *window, SceInt32 viewMode)
{
	SceInt32 prevMode = window->m_shellViewMode;
	if (prevMode == viewMode && g_topWindow->m_foregroundApp != NULL)
		g_topWindow->m_foregroundApp->SendProcessCommand(1);

	window->m_shellViewMode = viewMode;
	bool wasAppVisible = (prevMode == 2 || prevMode == 4);
	bool isAppVisible = (viewMode == 2 || viewMode == 4);

	switch (viewMode) {
	case 1:
		if (viewMode != prevMode)
			showShellScenes(window);
		break;
	case 2:
	case 4:
		if (viewMode != prevMode)
			hideShellScenesForApp(window);
		if (window->m_shellViewMode == 4 && window->m_powerSuspendMode != 2)
			musicCoreBgmControl(1, 2);
		break;
	case 3:
	case 5:
		if (viewMode != prevMode) {
			paf::ui::Environment *env = paf::Framework::Instance()->GetEnvironmentInstance();
			env->ShowSceneEntry(50);
			env->ShowSceneEntry(45);
			ScePafGraphics_906BE45C();
			sceTouchSetProcessPrivilege(1);
			if (window->m_touchSuspendRequested) {
				window->m_touchSuspendRequested = false;
				syncTouchDefaultRegions(window);
			}
		}
		if (wasAppVisible && window->m_shellViewMode == 5 && window->m_powerSuspendMode != 2)
			musicCoreBgmControl(0, 2);
		break;
	default:
		break;
	}

	refreshDefaultIndicatorBg(window);

	if (wasAppVisible == isAppVisible)
		return;

	if (isAppVisible) {
		shellEventServerSend(7, NULL, 0);
		TopmenuAppProcess *proc = window->m_foregroundApp;
		if (proc != NULL) {
			if (proc->m_tabAppData != NULL) {
				float gateX = 0.0f;
				float gateY = 0.0f;
				LiveAreaPluginInterface *laIf = topmenuGetLiveAreaInterface();
				if (laIf != NULL) {
					laIf->getGateNormPos(proc->m_tabAppData->m_ownerTab->m_pageKey,
							     &gateX, &gateY);
				}
				g_topWindow->m_gateEffectOriginX = gateX;
				g_topWindow->m_gateEffectOriginY = gateY;
				playGateOpenTransition();
			}
			if (window->m_shellViewMode == 2 && !proc->m_hasAppId) {
				const char *splashPath = NULL;
				if (proc->m_appInfo.get() != NULL)
					splashPath = SceLsdb_1B9A8B45(&proc->m_appInfo);
				topmenuBootScreenShow(splashPath, proc);
			}
		}
	} else {
		shellEventServerSend(9, NULL, 0);
		if (window->m_shellViewMode == 5 && prevMode == 4) {
			TopmenuAppProcess *proc = g_topWindow->m_foregroundApp;
			if (proc != NULL && proc->m_tabAppData->m_pageTurn != NULL)
				proc->m_tabAppData->SetAppViewState(0);
		}
		playGateCloseTransition();
#if !defined(FW_TYPE_CEX)
		if (window->m_currentTab != NULL && window->m_currentTab->m_appData != NULL) {
			ScePafAutoTestTty_9E10E873(
				1, "/livespace/livearea_%s",
				window->m_currentTab->m_appData->m_titleId.c_str());
		}
#endif
	}
}

/* 0x81270BD6 */
SceUInt64 topmenuGetAppFlags(TopWindow *window, const char *titleId)
{
	for (paf::list<TopmenuAppFlagEntry>::iterator it = window->m_appFlagList.begin();
	     it != window->m_appFlagList.end(); ++it) {
		if (sce_paf_strncmp(it->appName, titleId, 32) == 0)
			return it->flags;
	}
	return 0;
}

/* 0x81271E3C */
bool topmenuIsLaunchSeqActive(const TopmenuLaunchSeq *seq)
{
	if (seq->state == 0)
		return false;
	if (!seq->autoStart && seq->closePageKey != -1) {
		TopTab *closingTab = (TopTab *)g_topWindow->FindTabByPageKey(seq->closePageKey);
		if (closingTab != NULL && closingTab->m_appData != NULL &&
		    closingTab->m_appData->m_process.m_launchMode == 1) {
			return false;
		}
	}
	return true;
}

#if FW_SINCE(0x03650000)
/* 0x8127289E */
bool topmenuIsBlockedLaunchTitle(const char *titleId)
{
	for (SceUInt32 idx = 0; idx < 4; ++idx) {
		if (sce_paf_strcmp(titleId, g_topmenuBlockedTitles[idx]) == 0)
			return true;
	}
	return false;
}
#endif

/* 0x81272BAE */
void topmenuSwitchToProcess(TopWindow *window, TopmenuAppProcess *proc, bool checkBgdl)
{
	if (window->m_foregroundApp == proc)
		return;

	if (g_topmenuPsCloseDialogId >= 0) {
		shellStateWatcherCloseDialog(g_topmenuPsCloseDialogId);
		g_topmenuPsCloseDialogId = -1;
	}

	bool detachedSystemApp = false;
	if (window->m_foregroundApp != NULL) {
		if (window->m_foregroundApp->m_tabAppData != NULL) {
			if (topmenuAppProcessIsRunning(window->m_foregroundApp)) {
				window->m_foregroundApp->SendProcessCommand(2);
			} else if (window->m_shellViewMode != 1) {
				if (window->m_foregroundApp->m_wantActive) {
					TopTabAppData *oldData = window->m_foregroundApp->m_tabAppData;
					if (oldData != NULL && oldData->m_viewKind == 1 &&
					    oldData->m_pageTurn != NULL) {
						oldData->SetAppViewState(0);
					}
				}
			}
		} else {
			detachedSystemApp = true;
			detachSystemAppProcess(window);
			window->m_foregroundApp = NULL;
		}
		ScePafMisc_F8D63B8E(0.8f);
		topmenuBootScreenHide();
	}

	window->m_foregroundApp = proc;

	if (
#if FW_SINCE(0x03650000)
	    checkBlockedTitlePrompt(window) ||
#endif
	    checkKeypadEnablePrompt(window) ||
	    checkBgAppConflictPrompt(window) ||
	    checkConflictingAttributeApp(window, 0x40000ULL, true, true) ||
	    checkBrowserBgConflictPrompt(window) ||
	    checkHdmiResolutionPrompt(window) ||
	    checkPocketStationPrompt(window) ||
	    checkParentalWebBrowserLaunch(window)) {
		window->m_foregroundApp = NULL;
	} else if (window->m_foregroundApp != NULL && window->m_foregroundApp->m_appId != -1) {
		if (checkBgdl && !detachedSystemApp) {
			SceInt64 appType = sceLsdbGetType(&window->m_foregroundApp->m_appInfo);
			if (appType == 4) {
				if (shellBgdlTitleHasTasks(window->m_foregroundApp->m_titleId.c_str(), 0)) {
					if (showBgdlPendingConfirmDialog(window, true))
						window->m_foregroundApp = NULL;
				} else if (sceLsdbGetGameDataId(&window->m_foregroundApp->m_appInfo) != NULL &&
					   shellBgdlTitleHasTasksOfKind(
						   sceLsdbGetGameDataId(&window->m_foregroundApp->m_appInfo), 1, 0) &&
					   showBgdlPendingConfirmDialog(window, false)) {
					window->m_foregroundApp = NULL;
				}
			} else if (appType == 5) {
				if (sceLsdbGetGameDataId(&window->m_foregroundApp->m_appInfo) != NULL &&
				    shellBgdlTitleHasTasksOfKind(
					    sceLsdbGetGameDataId(&window->m_foregroundApp->m_appInfo), 1, 0) &&
				    showBgdlPendingConfirmDialog(window, false)) {
					window->m_foregroundApp = NULL;
				}
			} else if (shellBgdlRecordHasTasks(&window->m_foregroundApp->m_appInfo, 0) &&
				   showBgdlPendingConfirmDialog(window, false)) {
				window->m_foregroundApp = NULL;
			}
		}

		if (g_topWindow->m_gameIf != NULL) {
			typedef int (*CheckCardFn)(LsdbAppInfoPtr *);
			typedef void (*ShowCardErrFn)(LsdbAppInfoPtr *);
			if (reinterpret_cast<CheckCardFn>(g_topWindow->m_gameIf->func[56])(&proc->m_appInfo) < 0)
				reinterpret_cast<ShowCardErrFn>(g_topWindow->m_gameIf->func[57])(&proc->m_appInfo);
		}
	}

	if (window->m_foregroundApp != NULL) {
		checkConflictingAttributeApp(window, 0x20000ULL, false, true);
		if (window->m_foregroundApp->m_appInfo.get() != NULL &&
		    (static_cast<SceUInt64>(SceLsdb_0B16523B(&window->m_foregroundApp->m_appInfo)) & 0x20000ULL) != 0) {
			if (sce_paf_strncmp(window->m_foregroundApp->m_titleId.c_str(),
					    "NPXS10009", sce_paf_strlen("NPXS10009")) != 0) {
				musicCorePlayerControl(1, 0);
			}
		}

		if (g_topWindow->m_foregroundApp == window->m_foregroundApp &&
		    !window->m_foregroundApp->m_hasAppId) {
			ScePafMisc_F8D63B8E(0.1f);
		}

		TopTabAppData *appData = window->m_foregroundApp->m_tabAppData;
		if (appData != NULL) {
			if (appData->m_viewKind == 1 || appData->m_viewKind == 2 ||
			    appData->m_viewKind == 3) {
				proc->ResumeDefault();
				if (appData->m_viewKind == 1)
					topmenuSetShellViewMode(window, 2);
				else
					window->m_foregroundApp->SendProcessCommand(1);
				shellAppEventServerSend(0, NULL, 0);
			} else if (appData->m_viewKind == 0) {
				window->m_foregroundApp->SendProcessCommand(1);
			}
		} else {
			window->m_foregroundApp->SendProcessCommand(1);
			shellAppEventServerSend(0, NULL, 0);
		}
	} else {
		if (window->m_shellViewMode != 1)
			shellAppEventServerSend(0, NULL, 0);
		topmenuSetShellViewMode(window, 1);
		topmenuMotionUpdateFrontApp();
	}
}

/* 0x81272ECC */
static void onParentalBrowserTimer(SceInt32 eventType, paf::ui::Handler *self,
				   paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)self;
	(void)event;
	(void)userData;
	if (parentalGetRemainingSec() == 0) {
		g_topmenuParentalBypassOnce = false;
		power_manage::AuthInterface *parentalIf = g_topWindow->m_powerManageIf->getAuthInterface();
		if (parentalIf != NULL)
			parentalIf->cancelAuth();
	}
}

/* 0x81272FD0 */
static void onCarouselOpenTimer(SceInt32 eventType, paf::ui::Handler *self,
				paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)self;
	(void)event;
	(void)userData;

	g_topWindow->m_carouselState = 2;
	g_topWindow->m_scrollView->KillTimer(TIMER_CAROUSEL_OPEN_ID);

	SceInt32 savedIdx = g_topWindow->m_pendingScrollPageKey;
	if (savedIdx >= 0 && g_topWindow->m_carouselDragging &&
	    (SceUInt32)savedIdx < g_topWindow->m_tabs.size()) {
		TopTab *tab = (TopTab *)g_topWindow->m_tabs[savedIdx];
		paf::ui::Widget *iconWidget =
			tab->m_headerWidget->FindChild(paf::IDParam(TOP_WINDOW_HEADER_ICON_ID), 0);
		if (iconWidget != NULL) {
			iconWidget->SetFocus(0);
			paf::Framework::Instance()->GetEnvironmentInstance()->ShowFocus(-1);
		}
	}
}

/* 0x812730B8 */
void topmenuSelectHeaderTab(TopTab *tab)
{
	if (g_topWindow->m_carouselState == 0 || g_topWindow->m_carouselState == 3)
		return;

	topmenuWhatsNewHide();

	if (g_topWindow->m_scrollView->m_map_t_listener.find(TIMER_CAROUSEL_OPEN_ID) !=
	    g_topWindow->m_scrollView->m_map_t_listener.end()) {
		onCarouselOpenTimer(0, NULL, NULL, NULL);
	}

	paf::Framework::Instance()->PlayCommonSE(40, 0.0f, false);
	paf::inputdevice::Lock(0x7Fu);
	paf::inputdevice::Unlock(0x7Fu);

	if (tab == NULL) {
		SceUInt32 savedIdx = (SceUInt32)g_topWindow->m_pendingScrollPageKey;
		if (savedIdx < g_topWindow->m_tabs.size())
			tab = (TopTab *)g_topWindow->m_tabs[savedIdx];
	}

	SceUInt32 targetIdx = 0;
	if (tab == NULL) {
		topmenuHelpShowHomeHelp();
	} else {
		targetIdx = (SceUInt32)tab->m_index;
		g_topWindow->m_activeSnapId = tab->m_scrollView->GetSnapID(tab->m_widget);
		if (targetIdx == 0)
			topmenuHelpShowHomeHelp();
	}

	g_topWindow->m_scrollView->m_is_auto_snap = false;
	paf::math::v4 targetPos((float)targetIdx * -960.0f, 0.0f, 0.0f, 0.0f);
	paf::ui::Widget *viewParent = g_topWindow->m_scrollView->GetViewParent();
	viewParent->SetPos(targetPos, new paf::Timer(350.0f, paf::Timer::FUNC_CUBIC_OUT),
			   0x10000, 0.0f, 0, NULL, NULL);

	g_topWindow->m_hoveredTab = NULL;
	g_topWindow->m_draggedTab = NULL;
	g_topWindow->m_carouselAnimating = false;
	g_topWindow->m_pendingScrollPageKey = -1;
	g_topWindow->m_carouselDragging = false;
	g_topWindow->m_carouselState = 3;

	g_topWindow->m_scrollView->SetTimerListener(
		TIMER_CAROUSEL_CLOSE_ID, 350.0f,
		new paf::ui::EventCBListener(onCarouselCloseTimer, NULL));
	g_topWindow->UpdateTabLayout(0);

	if ((g_topWindow->m_scrollView->m_event_enable & 0x400u) == 0)
		g_topWindow->m_scrollView->EnableFocusEvent(true);
	g_topWindow->m_scrollView->m_event_enable |= 0xFFF0u;
	g_topWindow->m_scrollView->SetMovableDirOn(0);

	for (paf::vector<TopTabBase *>::iterator it = g_topWindow->m_tabs.begin();
	     it != g_topWindow->m_tabs.end(); ++it) {
		TopTab *curTab = (TopTab *)(*it);
		hideWidgetWithTimer(curTab->m_headerWidget, new paf::Timer(0.0f));
		curTab->SetActiveTab(false);

		float animTime = 150.0f;
		float animDelay = 150.0f;
		if (targetIdx != (SceUInt32)curTab->m_index) {
			animTime = 1.0f;
			animDelay = 299.0f;
		}

		if (curTab->m_tabMode != 3) {
			paf::ui::Widget *contentWidget = (curTab->m_appData != NULL) ?
				curTab->m_appData->m_rootWidget : curTab->m_contentRoot;
			if (animTime <= 0.0f && animDelay <= 0.0f) {
				showWidgetNow(contentWidget);
			} else {
				showWidgetWithTimer(contentWidget, new paf::Timer(animTime));
			}
		}
	}

	if (tab != NULL)
		setFocusedTab(g_topWindow, tab, true);
	else
		setFocusedTab(g_topWindow, (TopTab *)g_topWindow->m_tabs[0], true);

	float tabSpacing = (float)g_topWindow->m_tabSpacing;
	float curX = 0.0f;
	if (tab != NULL && tab->m_index > 0)
		curX = -((float)tab->m_index * 960.0f);

	SceInt32 idx = 0;
	for (paf::vector<TopTabBase *>::iterator it = g_topWindow->m_tabs.begin();
	     it != g_topWindow->m_tabs.end(); ++it, ++idx) {
		TopTab *curTab = (TopTab *)(*it);
		curTab->m_isClosing = false;
		curTab->m_hoverProgress = 0.0f;

		if (idx != 0) {
			paf::math::v4 worldPos(curX, 0.0f, 0.0f, 0.0f);
			paf::math::v4 tiltPos;
			paf::math::v4 tiltRot;
			computeTabScrollTransform(g_topWindow, &tiltPos, &tiltRot, &worldPos, idx);
			curTab->m_contentRoot->SetPos(
				tiltPos, new paf::Timer(350.0f, paf::Timer::FUNC_CUBIC_OUT),
				0x10000, 0.0f, 2, NULL, NULL);
			curTab->m_contentRoot->SetRot(
				tiltRot, 0, new paf::Timer(210.0f, paf::Timer::FUNC_CUBIC_OUT),
				0x10006, 0, 2, NULL, NULL);
		}
		curX += tabSpacing;
	}

	SceInt32 focusedKey = (g_topWindow->m_currentTab != NULL) ?
		g_topWindow->m_currentTab->m_pageKey : 0;
	shellEventServerSend(5, NULL, focusedKey);
}

/* 0x812737FE */
void topmenuScrollToTabPageKey(SceInt32 pageKey)
{
	if (g_topWindow->m_carouselState == 2 || g_topWindow->m_carouselState == 1) {
		TopTab *tab = (TopTab *)g_topWindow->FindTabByPageKey(pageKey);
		topmenuSelectHeaderTab(tab);
		return;
	}

	TopmenuAppProcess *activeProc = g_topWindow->m_foregroundApp;
	if (activeProc == NULL)
		return;

	if (activeProc->m_tabAppData != NULL) {
		if (activeProc->m_tabAppData->m_viewKind == 1) {
			topmenuSwitchToProcess(g_topWindow, NULL, true);
		} else if (activeProc->m_tabAppData->m_appViewState == 1) {
			activeProc->m_tabAppData->OnLeaveViewport();
			g_topWindow->m_foregroundApp->SendProcessCommand(4);
		}
		return;
	}

	for (paf::vector<TopTabBase *>::iterator it = g_topWindow->m_tabs.begin();
	     it != g_topWindow->m_tabs.end(); ++it) {
		TopTab *tab = (TopTab *)(*it);
		if (tab->m_appData != NULL && tab->m_appData->m_appViewState == 1) {
			tab->m_appData->OnLeaveViewport();
			tab->m_appData->m_process.SendProcessCommand(4);
		}
	}

	TopTab *curTab = (TopTab *)g_topWindow->FindFocusedTab();
	setFocusedTab(g_topWindow, curTab, false);
}

/* 0x812738B4 */
void topmenuBeginBusy(void)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	g_topWindow->m_busyRefCount++;
	acquireInputBlocker(g_topWindow);
	if (g_topWindow->m_indicatorIf != NULL)
		g_topWindow->m_indicatorIf->LockButton();
	paf::thread::RMutex::main_thread_mutex.Unlock();
}

/* 0x812738F2 */
void topmenuTriggerLiveAreaUpdate(SceInt32 pageKey)
{
	TopTab *tab = (TopTab *)g_topWindow->FindTabByPageKey(pageKey);
	if (tab == NULL)
		return;

	TopTab *curTab = (TopTab *)g_topWindow->FindTabBySnapId(g_topWindow->m_activeSnapId);
	if (tab == curTab && g_topWindow->m_navArrowsEnabled && tab->m_appData != NULL)
		topmenuLaunchPatchChk(&tab->m_appData->m_process.m_appInfo);
}

/* 0x81273946 */
static void topmenuFinishLaunchSequence(TopmenuLaunchSeq *seq)
{
	TopTab *target = (TopTab *)g_topWindow->FindTabByPageKey(seq->targetPageKey);
	if (seq->autoStart) {
		if (target != NULL) {
			target->RefreshOrderStamp();
			topmenuSwitchToProcess(g_topWindow, &target->m_appData->m_process, true);
		}
	} else if (target != NULL) {
		target->RefreshOrderStamp();
		if (seq->openPatchChk)
			topmenuTriggerLiveAreaUpdate(target->m_pageKey);
	}

	topmenuHelpShowLiveAreaIntro();
	topmenuCancelLaunchSequence(seq);
}

/* 0x812739EE */
static void topmenuStartLaunchSequence(TopmenuLaunchSeq *seq,
				       const TopmenuLaunchSeqParam *param)
{
	topmenuDialogCloseAll();
	if (seq->state != 0) {
		topmenuHelpCancelLiveAreaIntro();
		topmenuCancelLaunchSequence(seq);
	}

	shellEventServerSend(10, NULL, 0);

	TopmenuAppProcess *activeProc = g_topWindow->m_foregroundApp;
	TopTab *target = (TopTab *)g_topWindow->FindTabByPageKey(param->targetPageKey);

	if (g_topWindow->m_carouselState == 0 && param->autoStart &&
	    target != NULL && activeProc == &target->m_appData->m_process) {
		activeProc->ResumeWithArg(-1, false);
		activeProc->SendProcessCommand(1);
		return;
	}

	seq->targetPageKey = param->targetPageKey;
	seq->autoStart = param->autoStart;
	seq->skipScroll = param->skipScroll;
	seq->holdBusy = param->holdBusy;
	seq->isWebApp = param->isWebApp;
	seq->openPatchChk = param->openPatchChk;
	seq->closePageKey = param->closePageKey;
	seq->immediateStart = param->immediateStart;
	seq->notifyApplauncher = param->notifyApplauncher;

	if (param->autoStart || param->notifyApplauncher)
		setGameDialogHold(g_topWindow->m_gameIf, true);

	SceInt32 initialState = 2;
	if (activeProc != NULL) {
		if (activeProc->m_tabAppData != NULL) {
			if (activeProc->m_tabAppData->m_viewKind == 1 ||
			    activeProc->m_tabAppData->m_appViewState == 1) {
				initialState = 1;
			}
		} else {
			if (param->immediateStart) {
				topmenuBeginBusy();
				topmenuFinishLaunchSequence(seq);
				return;
			}
			initialState = 1;
		}
	} else if (g_topWindow->m_carouselState == 2 ||
		   g_topWindow->m_carouselState == 3 ||
		   g_topWindow->m_carouselState == 1) {
		initialState = 4;
	}

	if (seq->holdBusy)
		topmenuBeginBusy();

	if (initialState == 1)
		seq->state = 1;
	else if (initialState == 2)
		topmenuAdvanceLaunchScroll(seq);
	else if (initialState == 4)
		seq->state = 4;

	topmenuScrollToTabPageKey(target != NULL ? target->m_pageKey : -1);
}

/* 0x81273BCA */
static void topmenuCompleteLaunchIfReady(TopmenuLaunchSeq *seq, SceInt32 pageKey)
{
	if (seq->state != 3 || seq->targetPageKey != pageKey)
		return;

	TopTab *target = (TopTab *)g_topWindow->FindTabByPageKey(pageKey);
	if (target == NULL) {
		topmenuCancelLaunchSequence(seq);
		return;
	}

	if (target->m_index == 0) {
		topmenuFinishLaunchSequence(seq);
		return;
	}

	LiveAreaPluginInterface *laIf = topmenuGetLiveAreaInterface();
	if (laIf->isIntroDone(pageKey) != 0 &&
	    target->m_appData->m_bgLoadPendingCount == 3) {
		topmenuFinishLaunchSequence(seq);
	}
}

/* 0x81273C34 */
static void topmenuOnScrollSettled(TopmenuLaunchSeq *seq, TopTab *settledTab)
{
	if (seq->state != 2)
		return;

	TopTab *target = (TopTab *)g_topWindow->FindTabByPageKey(seq->targetPageKey);
	if (target == NULL) {
		topmenuCancelLaunchSequence(seq);
		return;
	}

	TopTab *curTab = (TopTab *)g_topWindow->FindTabBySnapId(g_topWindow->m_activeSnapId);
	if (curTab != target) {
		if (!seq->skipScroll) {
			if (!seq->holdBusy) {
				topmenuHelpCancelLiveAreaIntro();
				topmenuCancelLaunchSequence(seq);
				return;
			}
			g_topWindow->ScrollToTab(target, 500.0f);
		}
	}

	if (target == settledTab) {
		LiveAreaPluginInterface *laIf = topmenuGetLiveAreaInterface();
		if (!seq->autoStart ||
		    settledTab->m_appData == NULL ||
		    laIf == NULL ||
		    laIf->isPageAtTop(settledTab->m_pageKey) != 0) {
			g_topWindow->ScrollToTab(target, 500.0f);
			seq->state = 3;
			topmenuCompleteLaunchIfReady(seq, seq->targetPageKey);
		}
	}
}

/* ------------------------------------------------------------------ */
/* Helper: FocusRoot bidirectional link                               */
/* ------------------------------------------------------------------ */

static void linkFocusRootMember(paf::ui::Widget *rootWidget, paf::ui::Widget *memberWidget)
{
	if (rootWidget == NULL || memberWidget == NULL || rootWidget->m_focus_root == NULL)
		return;
	if (rootWidget->FindChild(memberWidget, 0) != NULL ||
	    memberWidget->FindChild(rootWidget, 0) != NULL) {
		return;
	}

	rootWidget->m_focus_root->Members().push_back(memberWidget);

	if (memberWidget->m_focus_referrers == NULL)
		memberWidget->m_focus_referrers = new paf::ui::FocusReferrers();

	paf::vector<paf::ui::Widget *> &roots = memberWidget->m_focus_referrers->m_roots;
	bool found = false;
	for (paf::vector<paf::ui::Widget *>::iterator it = roots.begin();
	     it != roots.end(); ++it) {
		if (*it == rootWidget) {
			found = true;
			break;
		}
	}
	if (!found)
		roots.push_back(rootWidget);
}

/* Pending app tab creation descriptor passed to createOrActivateAppTab (32 bytes) */
struct TopmenuOpenAppTabParam {
	TopmenuOpenAppTabParam()
		: titleId(),
		  afterPageKey(-1),
		  startSequence(true),
		  autoStart(false),
		  openPatchChk(false),
		  appInfo(),
		  launchAppId(-1)
	{
	}

	paf::string titleId;		/* +0x00 */
	SceInt32 afterPageKey;		/* +0x0C */
	bool startSequence;		/* +0x10 */
	bool autoStart;			/* +0x11 */
	bool openPatchChk;		/* +0x12 */
	LsdbAppInfoPtr appInfo;		/* +0x14 */
	SceInt32 launchAppId;		/* +0x1C */
};

/* Shell event packet received from sceAppMgrReceiveShellEvent */
struct TopmenuAppMgrShellEvent {
	SceInt32 eventType;		/* +0x00 */
	SceUID processId;		/* +0x04 */
	SceInt32 appId;			/* +0x08 */
	SceInt32 detailCode;		/* +0x0C */
	SceInt32 extraFlag;		/* +0x10 */
	SceInt32 targetAppId;		/* +0x14 */
	SceInt32 callerAppId;		/* +0x18 */
	SceInt32 isSubProcess;		/* +0x1C */
	char titleId[32];		/* +0x20 */
};

/* Forward declarations for interface thunks populated in TopWindow::TopWindow */
static int ifCreateCustomPage(int afterPageKey, paf::intrusive_ptr<paf::graph::Surface> *bgSurface);
static int ifOpenAppPage(const paf::string *titleId, int *inOutPageKey,
			 paf::intrusive_ptr<paf::graph::Surface> *iconSurface, int launchAppId);
static void ifSetPageSurfaceByTitleId(const char *const *titleId,
				      paf::intrusive_ptr<paf::graph::Surface> *surface);
static paf::ui::Widget *ifGetPageWidget(int pageKey);
static paf::ui::Widget *ifGetPageRootWidget(int pageKey);
static paf::cache::CacheManager *ifGetPageCacheManager(int pageKey);
static paf::graph::SurfacePool *ifGetPageSurfacePool(int pageKey);
static paf::intrusive_ptr<paf::graph::Surface> *ifGetDefaultBgTexture(void);
static int ifGetCurrentPageKey(void);
static int ifGetPageIndex(int pageKey);
static int ifGetPageKeyAt(int index);
static int ifFindPageKeyByTitleId(const char *titleId);
static paf::job::JobQueue *ifGetJobQueue(void);
static void ifOnManualIconClick(int pageKey);
static void ifOnImportSavedataIconClick(int pageKey);
static void ifOnOnlineStorageIconClick(int pageKey);
static void ifAddPageCloseListener(int pageKey, TopmenuPageEventCallback callback, void *userData);
static void ifRemovePageCloseListener(int pageKey, TopmenuPageEventCallback callback,
				      void *userData);
static void ifScrollToPage(int pageKey);
static void ifShowPageContent(int pageKey, float duration);
static void ifCancelPendingPageTransition(void);
static void ifCompletePageTransition(int pageKey);
static void ifHidePageContent(int pageKey, float duration);
static void ifAddPageShowListener(int pageKey, TopmenuPageEventCallback callback, void *userData);
static void ifAddPageHideListener(int pageKey, TopmenuPageEventCallback callback, void *userData);
static int ifRequestReturnToHome(void);
static int ifRequestReturnToHomeImmediate(void);
static void ifEnterEditMode(void);
static void ifLeaveEditMode(void);
static void ifBeginHold(void);
static void ifEndHold(void);
static int ifIsBusy(void);
static void ifSetPendingLaunchParam(int param);
static void ifOnGateClick(int pageKey);
static int ifIsPageInteractive(int pageKey);
static int ifIsPageSelected(int pageKey);
static paf::ui::Widget *ifGetPageTurnWidget(int pageKey);
static void ifSetPageScrolledDown(int pageKey, bool scrolledDown);
static int ifNotifyContentUpdated(const char *const *updateArgs);
static const char *ifGetCurrentPageTitleId(void);
static void ifSetPageMusicPlaying(int pageKey, bool playing);
static const wchar_t *ifGetPageTitle(int pageKey);
static void ifOnInitialBootReady(void);
static void ifPushBackgroundLayer(void *owner, int priority, bool useCustomColor, int style,
				  SceUInt32 color);
static void ifPopBackgroundLayer(void *owner, int priority);
static void ifSetScrollEnabled(bool enabled);
static int ifOpenPsStoreLogo(int mode, int arg);
static int ifClosePsStoreLogo(void *arg);
static int ifIsAppInForeground(void);
static int ifIsHomePageSelected(void);
static int ifGetRunningApplications(paf::vector<TopmenuRunningAppInfo> *outApps);
static void ifSetApplauncherInterface(ApplauncherInterface *iface);
static void ifSetLiveAreaInterface(LiveAreaPluginInterface *iface);
static void ifClosePageByTitleId(const char *titleId);
static void ifCloseCurrentAppIfMatches(const char *titleId);
static void ifReloadWhatsNew(void);
static int ifStartAppDataPromote(const char *titleId, TopmenuPromoteCallback callback,
				 void *userData);
static void ifNotifyUpStatusChanged(const char *titleId, int upStatus);
static int ifGetAppInfoByTitleId(LsdbAppInfoPtr *outInfo, const char *titleId);
static const char *ifGetAppExecInfoByBgTitleId(const char *bgTitleId, SceUInt64 *outTimestamp);
static void ifGetOpenPageTitleIds(paf::vector<paf::string> *outTitles);
static void ifEnterAppForeground(void);
static void ifLeaveAppForeground(bool animate, const char *titleId);
static int ifBeginAppInstallLock(const char *titleId);
static int ifEndAppInstallLock(const char *titleId, int relaunchMode);
static void ifAddPageAddedListener(TopmenuWindowEventCallback callback, void *userData);
static void ifRemovePageAddedListener(TopmenuWindowEventCallback callback, void *userData);
static void ifAddPageRemovedListener(TopmenuWindowEventCallback callback, void *userData);
static void ifRemovePageRemovedListener(TopmenuWindowEventCallback callback, void *userData);
static void ifAddPageFocusListener(TopmenuWindowEventCallback callback, void *userData);
static void ifRemovePageFocusListener(TopmenuWindowEventCallback callback, void *userData);
static void ifRefreshAppInfoSync(const char *titleId);
static int ifGetOpenAppPageCount(void);
static void ifReloadSystemBgm(void);
static void ifInitLiveboardSurfacePools(void);

/* ------------------------------------------------------------------ */
/* Chunk 2: TopWindow lifecycle, carousel, dialogs, app launch        */
/* (0x812740FE..0x81279F06)                                           */
/* ------------------------------------------------------------------ */

/* 0x812740FE */
TopWindow::TopWindow(paf::Plugin *plugin)
	: TopWindowBase(reinterpret_cast<paf::ui::Widget *>(plugin), 2, 7, 960),
	  m_indicatorIf(reinterpret_cast<IndicatorPluginInterface *>(
		  topmenuFindPluginInterface("indicator_plugin", 1))),
	  m_imposeIf(reinterpret_cast<ImposePluginInterface *>(
		  topmenuFindPluginInterface("impose_plugin", 1))),
	  m_powerManageIf(reinterpret_cast<power_manage::PowerManageInterface *>(
		  topmenuFindPluginInterface("power_manage_plugin", 1))),
	  m_gameIf(reinterpret_cast<GamePluginInterface *>(
		  topmenuFindPluginInterface("game_plugin", 1))),
	  m_applauncherIf(NULL),
	  m_liveAreaIf(NULL),
	  m_defaultBgTex(plugin->GetTexture(paf::IDParam(TOP_WINDOW_DEFAULT_TEX_ID))),
	  m_homeTabWidget(NULL),
	  m_overlayBgWidget(NULL),
	  m_backPlaneWidget(NULL),
	  m_gateEffectScene(NULL),
	  m_currentTab(NULL),
	  m_foregroundApp(NULL),
	  m_activeSnapId(-1),
	  m_freePools(),
	  m_carouselState(0),
	  m_hoveredTab(NULL),
	  m_draggedTab(NULL),
	  m_pendingScrollPageKey(-1),
	  m_carouselAnimating(false),
	  m_carouselDragging(false),
	  m_focusedNavWidget(NULL),
	  m_navFocusMode(0),
	  m_homeInputBlocked(false),
	  m_closingAppPending(false),
	  m_switchToHomeOnClose(false),
	  m_closingApp(NULL),
	  m_shellViewMode(1),
	  m_pendingLaunchQueue(),
	  m_pendingClosePageKey(-1),
	  m_touchActive(false),
	  m_touchSuspendRequested(true),
	  m_touchForceResume(false),
	  m_busyRefCount(0),
	  m_overlayPlaneRefCount(0),
	  m_suspendLockCount(0),
	  m_gameHoldCount(0),
	  m_editModeActive(false),
	  m_jobQueue(NULL),
	  m_bgStyleKind(1),
	  m_bgColorParam(0),
	  m_bgWaveParam(0),
	  m_gateEffectActive(false),
	  m_gateEffectReverse(false),
	  m_gateEffectSurface(),
	  m_gateEffectOriginX(0.0f),
	  m_gateEffectOriginY(0.0f),
	  m_gateEffectApp(NULL),
	  m_appCloseSnapshot(),
	  m_launchSeq(),
	  m_pendingKillAppId(-1),
	  m_closingAllTabs(false),
	  m_deactivateDelayFrames(0),
	  m_arrowUpWidget(NULL),
	  m_arrowDownWidget(NULL),
	  m_arrowUpVisible(true),
	  m_arrowDownVisible(true),
	  m_bgRequests(),
	  m_openingLiveArea(false),
	  m_openingPageKey(-1),
	  m_liveAreaReady(false),
	  m_skipDestroyCheck(false),
	  m_bgLaunchingApp(NULL),
	  m_carouselInputListener(),
	  m_iduOverlayWidget(NULL),
	  m_navArrowsEnabled(true),
	  m_scrollLockCount(0),
	  m_whatsNewNotifyEnabled(false),
	  m_psnSignInState(0),
	  m_powerSuspendMode(0),
	  m_appFlagList(),
	  m_appTerminateWaiters()
{
	g_topWindow = this;
	topmenuExecCheckInit();
	topmenuCoredumpInit();

	{
		paf::Plugin::PageOpenParam pageParam;
		paf::ui::Scene *mainPage = reinterpret_cast<paf::ui::Scene *>(
			plugin->PageOpen(paf::IDParam(TOP_WINDOW_PAGE_ID), pageParam));
		paf::ui::ScrollView *scrollView = reinterpret_cast<paf::ui::ScrollView *>(
			mainPage->FindChild(paf::IDParam(TOP_WINDOW_SCROLLVIEW_ID), 0));

		scrollView->m_draw_before_children = true;
		mainPage->m_hide_transparent = true;
		scrollView->SetVerticalScrollBar(NULL);
		scrollView->SetHorizontalScrollBar(NULL);
		ScePafWidget_21125870(scrollView, 3072);
		scrollView->AddEventListener(0x00010001,
					     new paf::ui::EventCBListener(onTopWindowScrollKey, NULL),
					     false);
		scrollView->AddEventListener(0x00010003,
					     new paf::ui::EventCBListener(onTopWindowScrollKey, NULL),
					     false);

		paf::ui::ScrollViewScrollParam scrollParam;
		scrollParam.m_value[0] = 16.0f;
		scrollParam.m_value[1] = 16.0f;
		scrollParam.m_value[12] = 16.0f;
		scrollParam.m_bounce = false;
		scrollView->SetScrollParam(&scrollParam);
		AttachScrollView(scrollView);

		m_homeTabWidget = scrollView->FindChild(paf::IDParam(TOP_WINDOW_FOOTER_ID), 0);
		SceInt32 footerSnapId = scrollView->GetSnapID(m_homeTabWidget);
		scrollView->DelSnapPoint(footerSnapId);
		hideWidgetNow(m_homeTabWidget);
		m_homeTabWidget->m_hit_find_mode = 2;
		m_homeTabWidget->m_hit_detect_mode = 2;

		scrollView->AddEventListener(0x10000002,
					     new paf::ui::EventCBListener(onTopWindowSnapChange, this),
					     false);
		scrollView->AddEventListener(0x10000005,
					     new paf::ui::EventCBListener(onTopWindowScrollCancelDrag,
									  this),
					     false);
		scrollView->AddEventListener(0x10000002,
					     new paf::ui::EventCBListener(onTopWindowScrollCancelDrag,
									  this),
					     false);
		scrollView->AddEventListener(0x10000001,
					     new paf::ui::EventCBListener(onTopWindowScrollCancelDrag,
									  this),
					     false);

		paf::ui::Widget *scrollScene = scrollView->m_scene;
		scrollScene->AddEventListener(0x00800008,
					      new paf::ui::EventCBListener(onTopWindowFocusChange,
									   this),
					      false);
		scrollScene->m_event_enable |= 0x2000u;
		scrollScene->AddEventListener(0x0080000A,
					      new paf::ui::EventCBListener(onTopWindowFocusChange,
									   this),
					      false);

		m_arrowUpWidget = mainPage->FindChild(paf::IDParam(TOP_WINDOW_ARROW_LEFT_ID), 0);
		m_arrowDownWidget = mainPage->FindChild(paf::IDParam(TOP_WINDOW_ARROW_RIGHT_ID), 0);
		m_arrowUpWidget->EnableFocusEvent(false);
		m_arrowDownWidget->EnableFocusEvent(false);
		m_arrowUpWidget->SetParent(scrollView, NULL, 0);
		m_arrowDownWidget->SetParent(scrollView, NULL, 0);
		m_arrowUpWidget->AddEventListener(0x10000008,
						  new paf::ui::EventCBListener(onTopWindowArrowLeft,
									       NULL),
						  false);
		m_arrowDownWidget->AddEventListener(0x10000008,
						    new paf::ui::EventCBListener(onTopWindowArrowRight,
										 NULL),
						    false);
		if (m_navArrowsEnabled) {
			m_navArrowsEnabled = false;
			updatePageArrows(this);
		}
		hideWidgetNow(m_arrowUpWidget);
		hideWidgetNow(m_arrowDownWidget);
		m_navFocusMode = 0;
	}

	{
		paf::Plugin::PageOpenParam coverParam;
		coverParam.overwrite_draw_priority = 40;
		paf::ui::Scene *coverPage = reinterpret_cast<paf::ui::Scene *>(
			plugin->PageOpen(paf::IDParam(TOP_WINDOW_COVER_PAGE_ID), coverParam));
		coverPage->m_hide_transparent = true;
		m_overlayBgWidget = coverPage->FindChild(paf::IDParam(TOP_WINDOW_COVER_PLANE_ID), 0);
		ScePafWidget_42C5A4D4(m_overlayBgWidget, 0, 0);
		hideWidgetNow(m_overlayBgWidget);
	}

	m_backPlaneWidget = topmenuCreatePlane(NULL);
	ScePafWidget_42C5A4D4(m_backPlaneWidget, 0, 0);
	hideWidgetNow(m_backPlaneWidget);

	paf::ui::Environment *env = paf::Framework::Instance()->GetEnvironmentInstance();
	m_gateEffectScene = new paf::ui::Scene(env, 31, 0, 0, 128);
	m_gateEffectScene->m_name = paf::IDParam("GateEffectScene");
	new paf::ui::Camera(m_gateEffectScene, 0);
	m_gateEffectScene->m_draw_func = onGateEffectDraw;
	m_gateEffectScene->m_draw_arg.m_data = NULL;
	m_gateEffectScene->m_draw_arg.m_alpha = 0.0f;
	m_gateEffectScene->m_hide_transparent = true;
	hideWidgetNow(m_gateEffectScene);

	m_freePools.reserve(6);
	for (int i = 0; i < 6; ++i)
		m_freePools.push_back(&g_topmenuSurfacePools[i]);

	createTopmenuRenderTarget(&g_topmenuSmallRenderTarget, 280, 79);
	createTopmenuRenderTarget(&g_topmenuLargeRenderTarget, 480, 272);

	g_topmenuInterface->createCustomPage = ifCreateCustomPage;
	g_topmenuInterface->openAppPage = ifOpenAppPage;
	g_topmenuInterface->setPageSurfaceByTitleId = ifSetPageSurfaceByTitleId;
	g_topmenuInterface->setApplauncherInterface = ifSetApplauncherInterface;
	g_topmenuInterface->setLiveAreaInterface = ifSetLiveAreaInterface;
	g_topmenuInterface->closePageByTitleId = ifClosePageByTitleId;
	g_topmenuInterface->closeCurrentAppIfMatches = ifCloseCurrentAppIfMatches;
	g_topmenuInterface->reloadWhatsNew = ifReloadWhatsNew;
	g_topmenuInterface->startAppDataPromote = ifStartAppDataPromote;
	g_topmenuInterface->notifyUpStatusChanged = ifNotifyUpStatusChanged;
	g_topmenuInterface->getAppInfoByTitleId = ifGetAppInfoByTitleId;
	g_topmenuInterface->getAppExecInfoByBgTitleId = ifGetAppExecInfoByBgTitleId;
	g_topmenuInterface->getOpenPageTitleIds = ifGetOpenPageTitleIds;
	g_topmenuInterface->openErrorDialog = topmenuOpenErrorDialog;
	g_topmenuInterface->getCurrentPageKey = ifGetCurrentPageKey;
	g_topmenuInterface->getPageIndex = ifGetPageIndex;
	g_topmenuInterface->findPageKeyByTitleId = ifFindPageKeyByTitleId;
	g_topmenuInterface->getPageKeyAt = ifGetPageKeyAt;
	g_topmenuInterface->getPageWidget = ifGetPageWidget;
	g_topmenuInterface->getPageRootWidget = ifGetPageRootWidget;
	g_topmenuInterface->closePage = topmenuDeleteTabByPageKey;
	g_topmenuInterface->getJobQueue = ifGetJobQueue;
	g_topmenuInterface->onManualIconClick = ifOnManualIconClick;
	g_topmenuInterface->onPatchIconClick = topmenuTriggerLiveAreaUpdate;
	g_topmenuInterface->onImportSavedataIconClick = ifOnImportSavedataIconClick;
	g_topmenuInterface->onOnlineStorageIconClick = ifOnOnlineStorageIconClick;
	g_topmenuInterface->setPageMusicPlaying = ifSetPageMusicPlaying;
	g_topmenuInterface->getPageTitle = ifGetPageTitle;
	g_topmenuInterface->addPageCloseListener = ifAddPageCloseListener;
	g_topmenuInterface->removePageCloseListener = ifRemovePageCloseListener;
	g_topmenuInterface->scrollToPage = ifScrollToPage;
	g_topmenuInterface->showPageContent = ifShowPageContent;
	g_topmenuInterface->cancelPendingPageTransition = ifCancelPendingPageTransition;
	g_topmenuInterface->completePageTransition = ifCompletePageTransition;
	g_topmenuInterface->hidePageContent = ifHidePageContent;
	g_topmenuInterface->requestReturnToHome = ifRequestReturnToHome;
	g_topmenuInterface->requestReturnToHomeImmediate = ifRequestReturnToHomeImmediate;
	g_topmenuInterface->addPageShowListener = ifAddPageShowListener;
	g_topmenuInterface->addPageHideListener = ifAddPageHideListener;
	g_topmenuInterface->getPageTurnWidget = ifGetPageTurnWidget;
	g_topmenuInterface->notifyIntroComplete = topmenuNotifyLaunchSeqReady;
	g_topmenuInterface->setPageScrolledDown = ifSetPageScrolledDown;
	g_topmenuInterface->notifyContentUpdated = ifNotifyContentUpdated;
	g_topmenuInterface->getDefaultBgTexture = ifGetDefaultBgTexture;
	g_topmenuInterface->getCurrentPageTitleId = ifGetCurrentPageTitleId;
	g_topmenuInterface->isTopmenuForeground =
		reinterpret_cast<int (*)(void)>(topmenuIsPowerSuspendNormal);
	g_topmenuInterface->onInitialBootReady = ifOnInitialBootReady;
	g_topmenuInterface->beginGameHold = topmenuBeginGameHold;
	g_topmenuInterface->endGameHold = topmenuEndGameHold;
	g_topmenuInterface->beginHold = ifBeginHold;
	g_topmenuInterface->endHold = ifEndHold;
	g_topmenuInterface->beginBusy = topmenuBeginBusy;
	g_topmenuInterface->endBusy = topmenuEndBusy;
	g_topmenuInterface->isBusy = ifIsBusy;
	g_topmenuInterface->enterEditMode = ifEnterEditMode;
	g_topmenuInterface->leaveEditMode = ifLeaveEditMode;
	g_topmenuInterface->setPendingLaunchParam = ifSetPendingLaunchParam;
	g_topmenuInterface->refreshAppInfoSync = ifRefreshAppInfoSync;
	g_topmenuInterface->getPageCacheManager = ifGetPageCacheManager;
	g_topmenuInterface->getPageSurfacePool = ifGetPageSurfacePool;
	g_topmenuInterface->onGateClick = ifOnGateClick;
	g_topmenuInterface->isPageInteractive = ifIsPageInteractive;
	g_topmenuInterface->isPageSelected = ifIsPageSelected;
	g_topmenuInterface->enterAppForeground = ifEnterAppForeground;
	g_topmenuInterface->leaveAppForeground = ifLeaveAppForeground;
	g_topmenuInterface->isAppInForeground = ifIsAppInForeground;
	g_topmenuInterface->isHomePageSelected = ifIsHomePageSelected;
	g_topmenuInterface->getRunningApplications = ifGetRunningApplications;
	g_topmenuInterface->beginAppInstallLock = ifBeginAppInstallLock;
	g_topmenuInterface->endAppInstallLock = ifEndAppInstallLock;
	g_topmenuInterface->pushBackgroundLayer = ifPushBackgroundLayer;
	g_topmenuInterface->popBackgroundLayer = ifPopBackgroundLayer;
	g_topmenuInterface->setScrollEnabled = ifSetScrollEnabled;
	g_topmenuInterface->getNetworkState = topmenuGetPsnSignInState;
	g_topmenuInterface->openPsStoreLogo = ifOpenPsStoreLogo;
	g_topmenuInterface->closePsStoreLogo = ifClosePsStoreLogo;
	g_topmenuInterface->getOpenAppPageCount = ifGetOpenAppPageCount;
	g_topmenuInterface->reloadSystemBgm = ifReloadSystemBgm;
	g_topmenuInterface->initLiveboardSurfacePools = ifInitLiveboardSurfacePools;
	g_topmenuInterface->createPlane = topmenuCreatePlane;
	g_topmenuInterface->addPageAddedListener = ifAddPageAddedListener;
	g_topmenuInterface->removePageAddedListener = ifRemovePageAddedListener;
	g_topmenuInterface->addPageRemovedListener = ifAddPageRemovedListener;
	g_topmenuInterface->removePageRemovedListener = ifRemovePageRemovedListener;
	g_topmenuInterface->addPageFocusListener = ifAddPageFocusListener;
	g_topmenuInterface->removePageFocusListener = ifRemovePageFocusListener;

	paf::common::MainThreadCallList::Register(onTopWindowMainTick, NULL);
	if (m_powerManageIf != NULL)
		m_powerManageIf->pushKeyHandler(onTopWindowPowerKey, this);

	paf::ui::Widget *indicatorBtn = m_indicatorIf->GetButton();
	indicatorBtn->EnableFocusEvent(true);
	paf::ui::Widget *indicatorParent = indicatorBtn->m_parent;
	if (!indicatorBtn->m_focus_direction_off)
		indicatorBtn->m_cursor_hidden = true;
	indicatorBtn->m_pre_dispatch = false;
	ScePafWidget_00AA6519(indicatorParent);
	linkFocusRootMember(m_scrollView, indicatorParent);
	linkFocusRootMember(indicatorParent, m_scrollView);

	ifReloadSystemBgm();

	paf::job::JobQueue::Option queueOpt;
	queueOpt.priority = 192;
	queueOpt.stackSize = 12288;
	queueOpt.numThreads = 1;
	m_jobQueue = new paf::job::JobQueue("SceShellJobQueueTopWindow", &queueOpt);

	AddEventListener(0, onTopWindowTabAdded, NULL);
	AddEventListener(1, onTopWindowTabRemoved, NULL);
	AddEventListener(2, onTopWindowTabsCleared, NULL);

	paf::common::SharedPtr<paf::inputdevice::InputListener> inputListener(
		topmenuCreateInputListener());
	paf::inputdevice::AddInputListener(inputListener);

	m_busyRefCount++;
	acquireInputBlocker(this);
	if (m_indicatorIf != NULL)
		m_indicatorIf->LockButton();

	shellAppSetForeground(0, "NPXS19999", 0);
	ensureModeWatermarkText(this);
}

/* 0x81275710 */
TopWindow::~TopWindow()
{
	topmenuWhatsNewHide();

	if (m_carouselInputListener.get() != NULL) {
		paf::inputdevice::DelInputListener(m_carouselInputListener);
		m_carouselInputListener.reset();
	}

	RemoveEventListener(0, onTopWindowTabAdded, NULL);
	RemoveEventListener(1, onTopWindowTabRemoved, NULL);
	RemoveEventListener(2, onTopWindowTabsCleared, NULL);

	SceUID memBlockUid = -1;
	sceGxmRenderTargetGetDriverMemBlock(g_topmenuSmallRenderTarget, &memBlockUid);
	sceGxmDestroyRenderTarget(g_topmenuSmallRenderTarget);
	sceKernelFreeMemBlock(memBlockUid);

	sceGxmRenderTargetGetDriverMemBlock(g_topmenuLargeRenderTarget, &memBlockUid);
	sceGxmDestroyRenderTarget(g_topmenuLargeRenderTarget);
	sceKernelFreeMemBlock(memBlockUid);

	m_gateEffectScene->DestroyWidget();

	if (m_jobQueue != NULL) {
		delete m_jobQueue;
		m_jobQueue = NULL;
	}

	if (m_powerManageIf != NULL)
		m_powerManageIf->popKeyHandler(onTopWindowPowerKey, this);

	paf::common::MainThreadCallList::Unregister(onTopWindowMainTick, NULL);
	m_backPlaneWidget->DestroyWidget();

	while (!m_tabs.empty()) {
		TopTabBase *tab = m_tabs.back();
		m_tabs.pop_back();
		delete tab;
	}

	topmenuExecCheckTerm();
	g_topWindow = NULL;
}

/* 0x81275C3A */
void topmenuTopWindowStart(paf::Plugin *plugin)
{
	new TopWindow(plugin);
}

/* 0x81275C52 */
void topmenuTopWindowStop(void)
{
	if (g_topWindow != NULL)
		delete g_topWindow;
}

/* 0x81275C68 */
void topmenuTopWindowExit(void)
{
	for (unsigned int i = 0; i < 6; ++i) {
		if (g_topmenuSurfacePools[i].cacheManager != NULL) {
			delete g_topmenuSurfacePools[i].cacheManager;
			g_topmenuSurfacePools[i].cacheManager = NULL;
		}
		if (g_topmenuSurfacePools[i].surfacePool != NULL) {
			delete g_topmenuSurfacePools[i].surfacePool;
			g_topmenuSurfacePools[i].surfacePool = NULL;
		}
	}
	paf::GraphMem::FreeOSMemory(paf::GraphMem::DeviceType_SystemMemory, g_topmenuSurfaceMem);
}

/* 0x81275CA4 */
void topmenuTopWindowInitConfig(void)
{
	SceInt32 crashFlag = 0;
	if (sceRegMgrGetKeyInt("/CONFIG/SHELL", "test_crash", &crashFlag) == 0 &&
	    crashFlag != 0) {
		g_topmenuTestCrashEnabled = true;
	}
}

/* 0x81275CF4 */
TopmenuSurfacePoolSlot *topmenuAcquireSurfacePool(bool highPriority)
{
	(void)highPriority;
	if (g_topWindow->m_freePools.empty())
		return NULL;
	TopmenuSurfacePoolSlot *slot = g_topWindow->m_freePools.back();
	g_topWindow->m_freePools.pop_back();
	return slot;
}

/* 0x81275D32 */
void topmenuReleaseSurfacePool(TopmenuSurfacePoolSlot *slot)
{
	if (slot != NULL)
		g_topWindow->m_freePools.push_back(slot);
}

/* 0x81275D78 */
static void onCarouselCloseTimer(SceInt32 eventType, paf::ui::Handler *self,
				 paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)self;
	(void)event;
	(void)userData;

	paf::ui::ScrollView *scrollView = g_topWindow->m_scrollView;
	scrollView->m_is_auto_snap = true;
	scrollView->ScePafWidget_A0538225(-1);

	g_topWindow->m_emitEvents = false;
	while (!g_topWindow->m_tabs.empty()) {
		TopTab *lastTab = reinterpret_cast<TopTab *>(g_topWindow->m_tabs.back());
		if (lastTab == NULL || lastTab->m_tabMode != 3)
			break;
		topmenuCloseTab(lastTab, false);
	}
	g_topWindow->m_emitEvents = true;

	g_topWindow->m_carouselState = 0;
	if (g_topWindow->m_launchSeq.state == 4)
		topmenuAdvanceLaunchScroll(&g_topWindow->m_launchSeq);

	g_topWindow->m_scrollView->KillTimer(TIMER_CAROUSEL_CLOSE_ID);
}

/* 0x81275E2C */
static bool ensureTabSlotAvailable(TopWindow *window, TopTabBase *protectedTab)
{
	if (window->m_tabs.size() < window->m_initialCapacity)
		return true;
	if (window->m_tabs.size() == 1)
		return false;

	SceUInt32 oldestOrder = 0xFFFFFFFFu;
	TopTab *victim = NULL;
	for (size_t i = 0; i < window->m_tabs.size() - 1; ++i) {
		TopTab *candidate =
			reinterpret_cast<TopTab *>(window->m_tabs[window->m_tabs.size() - i - 1]);
		if (candidate != protectedTab && candidate->CanClose()) {
			if (candidate->m_tabMode == 3) {
				victim = candidate;
				break;
			}
			if (static_cast<SceUInt32>(candidate->m_orderStamp) < oldestOrder) {
				oldestOrder = static_cast<SceUInt32>(candidate->m_orderStamp);
				victim = candidate;
			}
		}
	}

	if (victim != NULL) {
		topmenuCloseTab(victim, false);
		return true;
	}
	return false;
}

/* 0x81275ED2 */
static void fillCarouselBlankTabs(TopWindow *window)
{
	window->m_emitEvents = false;
	for (size_t count = window->m_tabs.size(); count < window->m_initialCapacity; ++count) {
		TopTabBase *lastTab = window->m_tabs.empty() ? NULL : window->m_tabs.back();
		if (ensureTabSlotAvailable(g_topWindow, lastTab)) {
			TopTab *blankTab = new TopTab(getTopWindowPlugin(g_topWindow), false);
			if (blankTab != NULL) {
				window->AddTab(blankTab, true, NULL);
				blankTab->m_tabMode = 3;
				blankTab->HideIcon();

				g_topmenuPlugin->TemplateOpen(
					blankTab->m_contentRoot,
					paf::IDParam(TOP_WINDOW_BLANK_TAB_TPL_ID),
					paf::Plugin::TemplateOpenParam());
				paf::ui::Widget *plane = blankTab->m_contentRoot->FindChild(
					paf::IDParam(TOP_WINDOW_BLANK_TAB_PLANE_ID), 0);
				paf::intrusive_ptr<paf::graph::Surface> tex =
					getTopWindowPlugin(window)->GetTexture(
						paf::IDParam(TOP_WINDOW_BLANK_TAB_TEX_ID));
				plane->SetTexture(tex, 0, 0);
				plane->SetColor(paf::math::v4(1.0f, 1.0f, 1.0f, 1.0f), 0, 0x10001,
						0.0f, 0, NULL, NULL);
			}
		}
	}
	window->m_emitEvents = true;
}

/* 0x81276122 */
static void openCarouselView(void)
{
	if (shellStartupGetIduMode() == 1)
		return;

	shellEventServerSend(4, NULL, 0);

	if (g_topWindow->m_openingLiveArea) {
		ApplauncherInterface *appIf = topmenuGetApplauncherInterface();
		if (appIf != NULL)
			appIf->func[8]();
	}

	ApplauncherInterface *appIf = topmenuGetApplauncherInterface();
	if (appIf != NULL) {
		appIf->func[6]();
		appIf->func[19]();
	}

	topmenuWhatsNewShow();
	paf::inputdevice::Lock(0x7Fu);
	paf::inputdevice::Unlock(0x7Fu);

	if (g_topWindow->m_scrollView->m_map_t_listener.find(TIMER_CAROUSEL_CLOSE_ID) !=
	    g_topWindow->m_scrollView->m_map_t_listener.end()) {
		onCarouselCloseTimer(0, NULL, NULL, NULL);
	}

	g_topWindow->m_scrollView->SetTimerListener(
		TIMER_CAROUSEL_OPEN_ID, 300.0f,
		new paf::ui::EventCBListener(onCarouselOpenTimer, NULL));
	paf::Framework::Instance()->PlayCommonSE(39, 0.0f, false);

	fillCarouselBlankTabs(g_topWindow);
	g_topWindow->m_carouselState = 1;

	TopTab *focusedTab = reinterpret_cast<TopTab *>(g_topWindow->FindFocusedTab());
	g_topWindow->m_hoveredTab = NULL;
	g_topWindow->m_draggedTab = NULL;
	g_topWindow->m_carouselDragging = false;

	if (focusedTab != NULL) {
		g_topWindow->m_pendingScrollPageKey = focusedTab->m_index;
		paf::ui::Environment *env = paf::Framework::Instance()->GetEnvironmentInstance();
		bool focusShown = ScePafWidget_529FB877(env, -1);
		g_topWindow->m_carouselAnimating = focusShown;
		if (focusShown)
			env->HideFocus(-1);
	}

	g_topWindow->m_scrollView->m_event_enable &= ~0xE0u;
	g_topWindow->m_scrollView->SetMovableDirOn(3u);

	if (g_topWindow->m_navArrowsEnabled) {
		g_topWindow->m_navArrowsEnabled = false;
		updatePageArrows(g_topWindow);
	}

	int tabIdx = 0;
	for (paf::vector<TopTabBase *>::iterator it = g_topWindow->m_tabs.begin();
	     it != g_topWindow->m_tabs.end(); ++it, ++tabIdx) {
		TopTab *tab = reinterpret_cast<TopTab *>(*it);
		float hideDelay = (tab == focusedTab) ? 150.0f : 0.0f;

		showWidgetWithTimer(tab->m_headerWidget, new paf::Timer(300.0f));
		tab->SetActiveTab(true);

		if (tab->m_tabMode != 3) {
			paf::ui::Widget *body = (tab->m_appData != NULL)
							? tab->m_appData->m_contentWidget
							: tab->m_contentRoot;
			if (hideDelay > 0.0f) {
				hideWidgetWithTimer(body, new paf::Timer(hideDelay));
			} else {
				hideWidgetNow(body);
			}
		}

		tab->m_hoverProgress = 0.0f;
		tab->m_isClosing = false;
		if (tabIdx != 0) {
			paf::math::v4 tiltRot(0.0f, 0.43633235f, 0.0f, 0.0f);
			tab->m_contentRoot->SetRot(
				tiltRot, 0, new paf::Timer(300.0f, paf::Timer::FUNC_QUAD_INOUT),
				0x10006, 0, 2, NULL, NULL);
		}
	}

	g_topWindow->UpdateTabLayout(3);
	topmenuSwitchToProcess(g_topWindow, NULL, true);

	SceInt32 homeSnapId =
		g_topWindow->m_scrollView->GetSnapID(g_topWindow->m_tabs[0]->m_widget);
	g_topWindow->m_scrollView->DoSnap(
		homeSnapId, new paf::Timer(300.0f, paf::Timer::FUNC_QUAD_INOUT));
#if !defined(FW_TYPE_CEX)
	ScePafAutoTestTty_9E10E873(1, "/livespace/indexview");
#endif
}

/* 0x812766F6 */
void topmenuCancelLiveAreaOpen(SceInt32 pageKey)
{
	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	if (g_topWindow->m_openingLiveArea && tab != NULL && tab->m_appData != NULL) {
		ApplauncherInterface *appIf = topmenuGetApplauncherInterface();
		if (appIf != NULL)
			appIf->func[8]();
	}

	if (g_topWindow->m_launchSeq.targetPageKey == pageKey) {
		topmenuHelpCancelLiveAreaIntro();
		topmenuCancelLaunchSequence(&g_topWindow->m_launchSeq);
	}
}

/* 0x81276754 */
static void activateSystemAppProcess(TopWindow *window, TopmenuAppProcess *sysProc)
{
	window->m_closingApp = sysProc;
	window->m_closingAppPending = true;
	topmenuAppProcessCancel(sysProc);

	window->m_appCloseSnapshot.clear();
	SceInt32 captureAppId = -5;
	if (window->m_foregroundApp != NULL && window->m_foregroundApp->m_appInfo.get() != NULL) {
		if (SceLsdb_849E8DC6(&window->m_foregroundApp->m_appInfo) == 0)
			captureAppId = window->m_foregroundApp->m_appId;
	}

	paf::intrusive_ptr<paf::graph::Surface> capturedSurface;
	if (shellScreenshotCaptureSurface(&capturedSurface, captureAppId) == 0)
		window->m_appCloseSnapshot = capturedSurface;

	topmenuSwitchToProcess(g_topWindow, window->m_closingApp, true);
	topmenuSetShellViewMode(window, 2);

	const char *splashPath = SceLsdb_1B9A8B45(&window->m_closingApp->m_appInfo);
	topmenuBootScreenShow(splashPath, window->m_closingApp);
}

/* 0x81276826 */
static void onBgAppCloseDialogButton(SceInt32 dialogId, SceInt32 button,
				     bool *handled, bool *confirmed, bool wasLocked,
				     SceInt32 pageKey, TopmenuAppProcess *pendingProc,
				     bool lockPromote)
{
	if (button != 0) {
		if (button == 2) {
			*confirmed = true;
			*handled = true;
			topmenuBeginBusy();
		} else if (button == 3) {
			if (pendingProc == NULL) {
				TopTab *tab = reinterpret_cast<TopTab *>(
					g_topWindow->FindTabByPageKey(pageKey));
				if (tab != NULL && tab->m_appData != NULL) {
					tab->m_appData->m_process.SendProcessCommand(5);
					if (wasLocked && g_topWindow->m_powerManageIf != NULL &&
					    g_topWindow->m_powerManageIf->lockIfWakeApp(
						    tab->m_appData->m_titleId.c_str()) != 0) {
						showShellScenes(g_topWindow);
					}
				}
			}
			*handled = true;
		}
		SceCommonGuiDialog_BC311DC0(dialogId);
		return;
	}

	if (*confirmed) {
		SceInt32 bgAppId = sceAppMgrGetAppIdByAppId(-3);
		if (bgAppId != -1) {
			TopmenuProcessBase *bgProc = findProcessInListByAppId(g_topWindow, bgAppId);
			if (bgProc != NULL) {
				if (lockPromote) {
					const char *titleId = topmenuProcessGetTitleId(bgProc);
					topmenuSetPromoteWaitLock(true, titleId);
				}
				topmenuProcessCommand(bgProc, 5);
			}
		}

		if (pendingProc != NULL) {
			activateSystemAppProcess(g_topWindow, pendingProc);
		} else {
			TopTab *tab = reinterpret_cast<TopTab *>(
				g_topWindow->FindTabByPageKey(pageKey));
			if (tab != NULL && tab->m_appData != NULL)
				topmenuSwitchToProcess(g_topWindow, &tab->m_appData->m_process, true);
		}
		topmenuEndBusy();
	} else if (pendingProc != NULL) {
		delete pendingProc;
	}

	if (!(*handled) && pendingProc == NULL) {
		TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
		if (tab != NULL && tab->m_appData != NULL) {
			tab->m_appData->m_process.SendProcessCommand(5);
			if (wasLocked && g_topWindow->m_powerManageIf != NULL &&
			    g_topWindow->m_powerManageIf->lockIfWakeApp(
				    tab->m_appData->m_titleId.c_str()) != 0) {
				showShellScenes(g_topWindow);
			}
		}
	}

	topmenuDialogOnPowerRelease(dialogId);
}

/* 0x81276A5A */
static void onConflictAppDialogButton(SceInt32 dialogId, SceInt32 button,
				      bool *handled, bool *confirmed,
				      TopmenuAppProcess *conflictProc, bool wasLocked,
				      SceInt32 pageKey)
{
	switch (button) {
	case 0:
		if (*confirmed) {
			TopTab *tab = reinterpret_cast<TopTab *>(
				g_topWindow->FindTabByPageKey(pageKey));
			if (tab != NULL && tab->m_appData != NULL) {
				bool conflictStillAlive = (conflictProc == g_topWindow->m_closingApp);
				if (!conflictStillAlive) {
					for (paf::vector<TopTabBase *>::iterator it =
						     g_topWindow->m_tabs.begin();
					     it != g_topWindow->m_tabs.end(); ++it) {
						TopTab *iterTab = reinterpret_cast<TopTab *>(*it);
						if (iterTab->m_appData != NULL &&
						    &iterTab->m_appData->m_process == conflictProc) {
							conflictStillAlive = true;
							break;
						}
					}
				}
				if (conflictStillAlive)
					destroyProcessAndBgPartner(g_topWindow, conflictProc);
				topmenuSwitchToProcess(g_topWindow, &tab->m_appData->m_process, true);
			}
			topmenuEndBusy();
		}
		if (!(*handled)) {
			TopTab *tab = reinterpret_cast<TopTab *>(
				g_topWindow->FindTabByPageKey(pageKey));
			if (tab != NULL && tab->m_appData != NULL) {
				tab->m_appData->m_process.SendProcessCommand(5);
				if (wasLocked && g_topWindow->m_powerManageIf != NULL &&
				    g_topWindow->m_powerManageIf->lockIfWakeApp(
					    tab->m_appData->m_titleId.c_str()) != 0) {
					showShellScenes(g_topWindow);
				}
			}
		}
		topmenuDialogOnPowerRelease(dialogId);
		return;

	case 2:
		*handled = true;
		*confirmed = true;
		topmenuBeginBusy();
		break;

	case 3: {
		TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
		if (tab != NULL && tab->m_appData != NULL) {
			tab->m_appData->m_process.SendProcessCommand(5);
			if (wasLocked && g_topWindow->m_powerManageIf != NULL &&
			    g_topWindow->m_powerManageIf->lockIfWakeApp(
				    tab->m_appData->m_titleId.c_str()) != 0) {
				showShellScenes(g_topWindow);
			}
		}
		*handled = true;
		break;
	}

	default:
		break;
	}

	SceCommonGuiDialog_BC311DC0(dialogId);
}

/* 0x81276C28 */
static void onBgdlWaitDialogButton(SceInt32 dialogId, SceInt32 button,
				   bool *handled, bool *confirmed, bool wasLocked,
				   SceInt32 pageKey)
{
	if (button != 0) {
		if (button == 4) {
			TopTab *tab = reinterpret_cast<TopTab *>(
				g_topWindow->FindTabByPageKey(pageKey));
			if (tab != NULL && tab->m_appData != NULL) {
				topmenuProcessCommand(&tab->m_appData->m_process, 5);
				topmenuStartAppDataPromote(&tab->m_appData->m_process.m_appInfo,
							   NULL, NULL, true);
			}
			*handled = true;
		} else if (button == 5) {
			*handled = true;
			*confirmed = true;
			topmenuBeginBusy();
		}
		SceCommonGuiDialog_BC311DC0(dialogId);
		return;
	}

	if (*confirmed) {
		TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
		if (tab != NULL && tab->m_appData != NULL)
			topmenuSwitchToProcess(g_topWindow, &tab->m_appData->m_process, false);
		topmenuEndBusy();
	}

	if (!(*handled)) {
		TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
		if (tab != NULL && tab->m_appData != NULL) {
			tab->m_appData->m_process.SendProcessCommand(5);
			if (wasLocked && g_topWindow->m_powerManageIf != NULL &&
			    g_topWindow->m_powerManageIf->lockIfWakeApp(
				    tab->m_appData->m_titleId.c_str()) != 0) {
				showShellScenes(g_topWindow);
			}
		}
	}

	topmenuDialogOnPowerRelease(dialogId);
}

/* 0x81276D8E */
static void onHdmiPromptDialogButton(SceInt32 dialogId, SceInt32 button,
				     bool *handled, bool *confirmed,
				     SceInt32 pageKey, SceInt32 targetHeight)
{
	if (button != 0) {
		if (button == 4) {
			*confirmed = true;
			*handled = true;
			topmenuBeginBusy();
		} else if (button == 5) {
			TopTab *tab = reinterpret_cast<TopTab *>(
				g_topWindow->FindTabByPageKey(pageKey));
			if (tab != NULL && tab->m_appData != NULL)
				tab->m_appData->m_process.SendProcessCommand(5);
			*handled = true;
		}
		SceCommonGuiDialog_BC311DC0(dialogId);
		return;
	}

	if (*confirmed) {
		bool hasRunningApps = false;
		for (paf::vector<TopTabBase *>::iterator it = g_topWindow->m_tabs.begin();
		     it != g_topWindow->m_tabs.end(); ++it) {
			TopTab *tab = reinterpret_cast<TopTab *>(*it);
			if (tab->m_appData != NULL && tab->m_appData->m_process.m_procState >= 2) {
				hasRunningApps = true;
				break;
			}
		}

		if (hasRunningApps) {
			CommonGuiLayoutDialogParam dlgParam;
			dlgParam.priority = 33;
			dlgParam.resourcePlugin = g_topmenuPlugin;
			dlgParam.layout = SceCommonGuiDialog_6CD802FC;
			dlgParam.text = g_topmenuPlugin->GetString(
				paf::IDParam(TOP_MSG_CLOSE_ALL_APPS_CONFIRM));
			dlgParam.handler = new TopmenuHdmiCloseAllHandler(pageKey, targetHeight);
			dlgParam.soundId = TOP_SOUND_DIALOG_OPEN;
			dlgParam.notifyClose = true;

			SceInt32 nextDlgId = SceCommonGuiDialog_CCFCD9A6(&dlgParam);
			if (nextDlgId >= 0) {
				topmenuDialogRegister(nextDlgId);
				topmenuDialogOnPowerHold(nextDlgId);
			}
		} else {
			TopTab *tab = reinterpret_cast<TopTab *>(
				g_topWindow->FindTabByPageKey(pageKey));
			if (tab != NULL && tab->m_appData != NULL) {
				SceInt32 hdmiRes = (targetHeight == 720) ? 0x8600 : 0x8300;
				SceInt32 res = sceAVConfigHdmiSetResolution(hdmiRes, 1);
				if (res != 0) {
					topmenuShowAppErrorMessageDialog(res, NULL, NULL, NULL,
									 NULL, NULL, NULL, NULL);
				} else {
					SceInt32 regMode = (hdmiRes == 0x8600) ? 2 : 3;
					sceRegMgrSetKeyInt("/CONFIG/DISPLAY",
								  "hdmi_resolution_mode", regMode);
					topmenuSwitchToProcess(g_topWindow,
							       &tab->m_appData->m_process, true);
				}
			}
		}
		topmenuEndBusy();
	}

	if (!(*handled)) {
		TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
		if (tab != NULL && tab->m_appData != NULL)
			tab->m_appData->m_process.SendProcessCommand(5);
	}

	topmenuDialogOnPowerRelease(dialogId);
}

/* 0x812770E0 */
static void onHdmiCloseAllDialogButton(SceInt32 dialogId, SceInt32 button,
				       bool *handled, bool *confirmed,
				       SceInt32 pageKey, SceInt32 targetHeight)
{
	if (button != 0) {
		if (button == 4) {
			*confirmed = true;
			*handled = true;
			topmenuBeginBusy();
		} else if (button == 5) {
			TopTab *tab = reinterpret_cast<TopTab *>(
				g_topWindow->FindTabByPageKey(pageKey));
			if (tab != NULL && tab->m_appData != NULL)
				tab->m_appData->m_process.SendProcessCommand(5);
			*handled = true;
		}
		SceCommonGuiDialog_BC311DC0(dialogId);
		return;
	}

	if (*confirmed) {
		for (paf::vector<TopTabBase *>::iterator it = g_topWindow->m_tabs.begin();
		     it != g_topWindow->m_tabs.end(); ++it) {
			TopTab *tab = reinterpret_cast<TopTab *>(*it);
			if (tab->m_appData != NULL && tab->m_appData->m_process.m_procState >= 2)
				tab->m_appData->m_process.SendProcessCommand(5);
		}

		TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
		if (tab != NULL && tab->m_appData != NULL) {
			SceInt32 hdmiRes = (targetHeight == 720) ? 0x8600 : 0x8300;
			SceInt32 res = sceAVConfigHdmiSetResolution(hdmiRes, 1);
			if (res != 0) {
				topmenuShowAppErrorMessageDialog(res, NULL, NULL, NULL,
								 NULL, NULL, NULL, NULL);
			} else {
				SceInt32 regMode = (hdmiRes == 0x8600) ? 2 : 3;
				sceRegMgrSetKeyInt("/CONFIG/DISPLAY",
							  "hdmi_resolution_mode", regMode);
				topmenuSwitchToProcess(g_topWindow,
						       &tab->m_appData->m_process, true);
			}
		}
		topmenuEndBusy();
	}

	if (!(*handled)) {
		TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
		if (tab != NULL && tab->m_appData != NULL)
			tab->m_appData->m_process.SendProcessCommand(5);
	}

	topmenuDialogOnPowerRelease(dialogId);
}

/* 0x812772AE */
void topmenuRegisterSystemAppId(TopWindow *window, SceInt32 appId)
{
	for (paf::list<SceInt32>::iterator it = window->m_appTerminateWaiters.begin();
	     it != window->m_appTerminateWaiters.end(); ++it) {
		if (*it == appId)
			return;
	}

	window->m_appTerminateWaiters.push_back(appId);
	topmenuBeginBusy();

	GamePluginInterface *gameIf = reinterpret_cast<GamePluginInterface *>(
		topmenuFindPluginInterface("game_plugin", 1));
	if (gameIf != NULL)
		reinterpret_cast<void (*)(int)>(gameIf->func[GAME_PLUGIN_FUNC_SET_DIALOG_HOLD])(1);
}

/* 0x81277348 */
static void onPocketStationDialogButton(SceInt32 dialogId, SceInt32 button,
					bool *handled, bool *confirmed)
{
	if (button != 0) {
		if (button == 2) {
			*confirmed = true;
			*handled = true;
			topmenuBeginBusy();
		} else if (button == 3) {
			*handled = true;
		}
		SceCommonGuiDialog_BC311DC0(dialogId);
		return;
	}

	if (*confirmed) {
		topmenuEndBusy();
		SceInt32 launchId = sceAppMgrLaunchAppByNameForShell(
			0x20000, "NPXS10002",
			"psns:browse?product=JA0003-PCSC80018_00-POCKETSTATION001");
		if (launchId >= 0)
			topmenuRegisterSystemAppId(g_topWindow, launchId);
	}

	topmenuDialogOnPowerRelease(dialogId);
}

/* 0x8127741A */
static void onKeypadPromptDialogButton(SceInt32 dialogId, SceInt32 button,
				       bool *handled, bool *confirmed, SceInt32 pageKey)
{
	if (button != 0) {
		if (button == 4) {
			*confirmed = true;
			*handled = true;
			topmenuBeginBusy();
		} else if (button == 5) {
			TopTab *tab = reinterpret_cast<TopTab *>(
				g_topWindow->FindTabByPageKey(pageKey));
			if (tab != NULL && tab->m_appData != NULL)
				tab->m_appData->m_process.SendProcessCommand(5);
			*handled = true;
		}
		SceCommonGuiDialog_BC311DC0(dialogId);
		return;
	}

	if (*confirmed) {
		bool hasRunningApps = false;
		for (paf::vector<TopTabBase *>::iterator it = g_topWindow->m_tabs.begin();
		     it != g_topWindow->m_tabs.end(); ++it) {
			TopTab *tab = reinterpret_cast<TopTab *>(*it);
			if (tab->m_appData != NULL && tab->m_appData->m_process.m_procState >= 2) {
				hasRunningApps = true;
				break;
			}
		}

		if (hasRunningApps) {
			CommonGuiLayoutDialogParam dlgParam;
			dlgParam.priority = 33;
			dlgParam.resourcePlugin = g_topmenuPlugin;
			dlgParam.layout = SceCommonGuiDialog_6CD802FC;
			dlgParam.text = g_topmenuPlugin->GetString(
				paf::IDParam(TOP_MSG_CLOSE_ALL_APPS_CONFIRM));
			dlgParam.handler = new TopmenuKeypadCloseAllHandler(pageKey);
			dlgParam.soundId = TOP_SOUND_DIALOG_OPEN;
			dlgParam.notifyClose = true;

			SceInt32 nextDlgId = SceCommonGuiDialog_CCFCD9A6(&dlgParam);
			if (nextDlgId >= 0) {
				topmenuDialogRegister(nextDlgId);
				topmenuDialogOnPowerHold(nextDlgId);
			}
		} else if (sceRegMgrSetKeyInt("/CONFIG/SYSTEM", "key_pad", 1) == 0) {
			paf::ui::Environment *env =
				paf::Framework::Instance()->GetEnvironmentInstance();
			ScePafWidget_75FB31F1(env, true);
			TopTab *tab = reinterpret_cast<TopTab *>(
				g_topWindow->FindTabByPageKey(pageKey));
			if (tab != NULL)
				topmenuSwitchToProcess(g_topWindow,
						       &tab->m_appData->m_process, true);
		}
		topmenuEndBusy();
	}

	if (!(*handled)) {
		TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
		if (tab != NULL && tab->m_appData != NULL)
			tab->m_appData->m_process.SendProcessCommand(5);
	}

	topmenuDialogOnPowerRelease(dialogId);
}

/* 0x81277746 */
static void onKeypadCloseAllDialogButton(SceInt32 dialogId, SceInt32 button,
					 bool *handled, bool *confirmed, SceInt32 pageKey)
{
	if (button != 0) {
		if (button == 4) {
			*confirmed = true;
			*handled = true;
			topmenuBeginBusy();
		} else if (button == 5) {
			TopTab *tab = reinterpret_cast<TopTab *>(
				g_topWindow->FindTabByPageKey(pageKey));
			if (tab != NULL && tab->m_appData != NULL)
				tab->m_appData->m_process.SendProcessCommand(5);
			*handled = true;
		}
		SceCommonGuiDialog_BC311DC0(dialogId);
		return;
	}

	if (*confirmed) {
		for (paf::vector<TopTabBase *>::iterator it = g_topWindow->m_tabs.begin();
		     it != g_topWindow->m_tabs.end(); ++it) {
			TopTab *tab = reinterpret_cast<TopTab *>(*it);
			if (tab->m_appData != NULL && tab->m_appData->m_process.m_procState >= 2)
				tab->m_appData->m_process.SendProcessCommand(5);
		}

		if (sceRegMgrSetKeyInt("/CONFIG/SYSTEM", "key_pad", 1) == 0) {
			paf::ui::Environment *env =
				paf::Framework::Instance()->GetEnvironmentInstance();
			ScePafWidget_75FB31F1(env, true);
			TopTab *tab = reinterpret_cast<TopTab *>(
				g_topWindow->FindTabByPageKey(pageKey));
			if (tab != NULL)
				topmenuSwitchToProcess(g_topWindow,
						       &tab->m_appData->m_process, true);
		}
		topmenuEndBusy();
	}

	if (!(*handled)) {
		TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
		if (tab != NULL && tab->m_appData != NULL)
			tab->m_appData->m_process.SendProcessCommand(5);
	}

	topmenuDialogOnPowerRelease(dialogId);
}

/* 0x812778D6 */
static void onBlockedTitleDialogButton(SceInt32 dialogId, SceInt32 button,
				       bool wasLocked, SceInt32 pageKey)
{
	if (button != 0) {
		if (button == 2) {
			TopTab *tab = reinterpret_cast<TopTab *>(
				g_topWindow->FindTabByPageKey(pageKey));
			if (tab != NULL && tab->m_appData != NULL) {
				tab->m_appData->m_process.SendProcessCommand(5);
				if (wasLocked && g_topWindow->m_powerManageIf != NULL &&
				    g_topWindow->m_powerManageIf->lockIfWakeApp(
					    tab->m_appData->m_titleId.c_str()) != 0) {
					showShellScenes(g_topWindow);
				}
			}
		}
		SceCommonGuiDialog_BC311DC0(dialogId);
		return;
	}

	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	if (tab != NULL && tab->m_appData != NULL) {
		tab->m_appData->m_process.SendProcessCommand(5);
		if (wasLocked && g_topWindow->m_powerManageIf != NULL &&
		    g_topWindow->m_powerManageIf->lockIfWakeApp(
			    tab->m_appData->m_titleId.c_str()) != 0) {
			showShellScenes(g_topWindow);
		}
	}
	topmenuDialogOnPowerRelease(dialogId);
}

/* 0x81277988 */
static SceInt32 createAppTab(SceInt32 afterPageKey, const paf::string &titleId,
			     const LsdbAppInfoPtr &appInfo,
			     paf::intrusive_ptr<paf::cache::CacheImageInterface> *iconCache,
			     bool startSequence, SceInt32 launchMode, bool autoStart,
			     bool holdBusy, bool openPatchChk, SceInt32 launchAppId)
{
	TopTabBase *afterTab = NULL;
	if (afterPageKey >= 0)
		afterTab = g_topWindow->FindTabByPageKey(afterPageKey);

	if (!ensureTabSlotAvailable(g_topWindow, afterTab))
		return -1;

	TopTab *newTab = new TopTab(getTopWindowPlugin(g_topWindow), false);
	if (newTab == NULL)
		return -1;

	SceInt32 newPageKey = newTab->m_pageKey;
	newTab->m_tabMode = 2;

	typedef void (*GetIconFn)(paf::intrusive_ptr<paf::cache::CacheImageInterface> *,
				  const paf::string &,
				  void (*)(paf::intrusive_ptr<paf::cache::CacheImageInterface> *,
					   SceInt32),
				  SceInt32);
	ApplauncherInterface *appIf = topmenuGetApplauncherInterface();
	reinterpret_cast<GetIconFn>(appIf->func[3])(iconCache, titleId, onTabIconReady, newPageKey);

	if (iconCache->get() != NULL)
		newTab->SetIconCache(*iconCache);

	g_topWindow->AddTab(newTab, false, afterTab);

	bool isWebApp = false;
	char launchParam[1024];
	if (launchAppId != -1 &&
	    sceAppMgrPeekLaunchParamForShell(launchAppId, launchParam, sizeof(launchParam)) == 0) {
		if (sce_paf_strncmp(launchParam, "wbapp0:", sce_paf_strlen("wbapp0:")) == 0 ||
		    sce_paf_strncmp(launchParam, "wbapp1:", sce_paf_strlen("wbapp1:")) == 0) {
			isWebApp = true;
		}
	}

	TopTabAppData *appData = new TopTabAppData(newTab, titleId, appInfo, launchMode,
						   autoStart, launchAppId);
	newTab->m_appData = appData;
	if (appData == NULL) {
		topmenuCloseTab(newTab, true);
		return -1;
	}

	if (launchMode != 1 && launchMode != 2 && launchMode != 3)
		appData->m_process.StartAppCheck(NULL);

	if (startSequence) {
		TopmenuLaunchSeqParam seqParam;
		seqParam.targetPageKey = newTab->m_pageKey;
		seqParam.autoStart = autoStart;
		seqParam.holdBusy = holdBusy;
		seqParam.skipScroll = false;
		seqParam.closePageKey = -1;
		seqParam.immediateStart = false;
		seqParam.isWebApp = isWebApp;
		seqParam.openPatchChk = openPatchChk;
		seqParam.notifyApplauncher = false;
		topmenuStartLaunchSequence(&g_topWindow->m_launchSeq, &seqParam);
	}

	return newTab->m_pageKey;
}

/* 0x81277BB0 */
static void createAppTabWithoutIcon(TopWindow *window, const TopmenuOpenAppTabParam *param)
{
	(void)window;
	paf::intrusive_ptr<paf::cache::CacheImageInterface> iconCache;
	createAppTab(param->afterPageKey, param->titleId, param->appInfo, &iconCache,
		     param->startSequence, 0, param->autoStart, true, param->openPatchChk,
		     param->launchAppId);
}

/* 0x81277C90 */
static void createAppTabWithIcon(TopWindow *window, const TopmenuOpenAppTabParam *param,
				 paf::intrusive_ptr<paf::cache::CacheImageInterface> *iconCache)
{
	(void)window;
	topmenuGetApplauncherInterface();
	createAppTab(param->afterPageKey, param->titleId, param->appInfo, iconCache,
		     param->startSequence, 0, param->autoStart, true, param->openPatchChk,
		     param->launchAppId);
}

/* 0x81277D76 */
static void showBusyWaitDialog(TopWindow *window)
{
	if (g_topmenuBusyDialogId >= 0)
		return;

	CommonGuiLayoutDialogParam dlgParam;
	dlgParam.priority = 33;
	dlgParam.resourcePlugin = g_topmenuPlugin;
	dlgParam.text = getTopWindowPlugin(window)->GetString(paf::IDParam(TOP_MSG_BUSY_WAIT));
	dlgParam.handler = new TopmenuBusyDialogHandler();
	dlgParam.soundId = TOP_SOUND_DIALOG_OPEN;
	dlgParam.notifyClose = true;

	SceInt32 dlgId = SceCommonGuiDialog_CCFCD9A6(&dlgParam);
	g_topmenuBusyDialogId = dlgId;
	if (dlgId >= 0) {
		topmenuDialogRegister(dlgId);
		topmenuDialogOnPowerHold(dlgId);
	}
}

/* 0x81277F24 */
static bool activateExistingAppTab(TopWindow *window, const TopmenuOpenAppTabParam *param,
				   SceInt32 targetPageKey, bool holdBusy)
{
	if (targetPageKey == -1)
		return false;

	if (window->m_gameHoldCount > 0) {
		showBusyWaitDialog(window);
		return false;
	}

	if (g_topWindow->m_launchSeq.state != 0) {
		bool isDebuggerClosing = false;
		if (!g_topWindow->m_launchSeq.autoStart &&
		    g_topWindow->m_launchSeq.closePageKey != -1) {
			TopTab *closingTab = reinterpret_cast<TopTab *>(
				g_topWindow->FindTabByPageKey(g_topWindow->m_launchSeq.closePageKey));
			if (closingTab != NULL && closingTab->m_appData != NULL &&
			    closingTab->m_appData->m_process.m_launchMode == 1) {
				isDebuggerClosing = true;
			}
		}
		if (!isDebuggerClosing)
			return false;
	}

	if (g_topWindow->m_carouselState == 0) {
		TopTab *focusedTab = reinterpret_cast<TopTab *>(g_topWindow->FindFocusedTab());
		if (focusedTab != NULL && focusedTab->m_pageKey == targetPageKey) {
			if (focusedTab->m_appData != NULL && param->autoStart) {
				LiveAreaPluginInterface *laIf = topmenuGetLiveAreaInterface();
				if (laIf != NULL && laIf->resetPageScroll != NULL)
					laIf->resetPageScroll(focusedTab->m_pageKey);
			}
		} else {
			TopTabBase *moveTab = g_topWindow->FindTabByPageKey(targetPageKey);
			TopTabBase *afterTab = g_topWindow->FindTabByPageKey(param->afterPageKey);
			if (moveTab != NULL)
				g_topWindow->MoveTabAfter(moveTab, afterTab);
			if (g_topWindow->FindFocusedTab() != focusedTab)
				window->ScrollToTab(focusedTab, 0.0f);
		}
	}

	TopTab *targetTab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(targetPageKey));
	if (targetTab == NULL)
		return false;

	bool isWebApp = false;
	char launchParam[1024];
	if (param->launchAppId != -1 &&
	    sceAppMgrPeekLaunchParamForShell(param->launchAppId, launchParam,
					    sizeof(launchParam)) == 0) {
		if (sce_paf_strncmp(launchParam, "wbapp0:", sce_paf_strlen("wbapp0:")) == 0 ||
		    sce_paf_strncmp(launchParam, "wbapp1:", sce_paf_strlen("wbapp1:")) == 0) {
			isWebApp = true;
		}
	}

	if (param->autoStart) {
		if (!topmenuProcessLaunchAsync(&targetTab->m_appData->m_process,
					       param->launchAppId)) {
			return false;
		}
	}

	TopmenuLaunchSeqParam seqParam;
	seqParam.targetPageKey = targetPageKey;
	seqParam.autoStart = param->autoStart;
	seqParam.holdBusy = holdBusy;
	seqParam.skipScroll = false;
	seqParam.closePageKey = -1;
	seqParam.immediateStart = false;
	seqParam.isWebApp = isWebApp;
	seqParam.openPatchChk = param->openPatchChk;
	seqParam.notifyApplauncher = false;
	topmenuStartLaunchSequence(&g_topWindow->m_launchSeq, &seqParam);
	return true;
}

/* 0x8127813C */
TopTab *topmenuFindTabByTitleId(TopWindow *window, const char *titleId)
{
	for (paf::vector<TopTabBase *>::iterator it = window->m_tabs.begin();
	     it != window->m_tabs.end(); ++it) {
		TopTab *tab = reinterpret_cast<TopTab *>(*it);
		if (tab->m_appData != NULL) {
			const char *tabTitle = tab->m_appData->m_titleId.c_str();
			if (sce_paf_strncmp(titleId, tabTitle, sce_paf_strlen(tabTitle)) == 0)
				return tab;
		}
	}
	return NULL;
}

/* 0x8127819C */
static int parseNumericVersion(const char *verStr)
{
	size_t len = sce_paf_strlen(verStr);
	if (len > 5)
		return 0;
	int value = 0;
	for (size_t i = 0; i < len; ++i) {
		char ch = verStr[i];
		if (ch >= '0' && ch <= '9')
			value = value * 10 + (ch - '0');
	}
	return value;
}

static bool isPsgmUpdateRequired(const LsdbAppInfoPtr *appInfo, const char *uri)
{
	if (topmenuParsePsgmUriScheme(uri) != 1)
		return false;

	char reqVer[16];
	if (topmenuParsePsgmReqVer(uri, reqVer, sizeof(reqVer)) == 0)
		return false;

	const char *appVer = sceLsdbGetAppVer(appInfo);
	if (appVer == NULL)
		return false;

	return parseNumericVersion(appVer) < parseNumericVersion(reqVer);
}

/* 0x81278264 */
static TopTab *findRunningTabByAppId(TopWindow *window, SceInt32 appId)
{
	for (paf::vector<TopTabBase *>::iterator it = window->m_tabs.begin();
	     it != window->m_tabs.end(); ++it) {
		TopTab *tab = reinterpret_cast<TopTab *>(*it);
		if (tab->m_appData != NULL &&
		    topmenuAppProcessIsRunning(&tab->m_appData->m_process)) {
			SceInt32 runningId = tab->m_appData->m_process.m_appId;
			if (runningId == appId)
				return tab;
		}
	}
	return NULL;
}

/* 0x812782CC */
static void topmenuHandleAppLaunchByStatus(const char *titleId, const LsdbAppInfoPtr *appInfo,
					   const SceAppMgrAppStatus *status, SceInt32 launchAppId)
{
	ApplauncherInterface *appIf = topmenuGetApplauncherInterface();
	bool autoStart = (status->launchMode & 0x20000u) != 0;
	TopTab *callerTab = findRunningTabByAppId(g_topWindow, status->launchSourceAppId);

	TopTabBase *baseTab = (g_topWindow->m_switchToHomeOnClose &&
			       g_topWindow->m_tabs.size() >= 2)
				      ? g_topWindow->m_tabs[1]
				      : g_topWindow->m_tabs[0];
	SceInt32 afterPageKey = baseTab->m_pageKey;

	TopTabBase *focusedTab = g_topWindow->FindFocusedTab();
	if (focusedTab != NULL)
		afterPageKey = focusedTab->m_pageKey;
	if (callerTab != NULL)
		afterPageKey = callerTab->m_pageKey;

	if (g_topWindow->m_switchToHomeOnClose && !g_topWindow->m_tabs.empty()) {
		if (afterPageKey == g_topWindow->m_tabs[0]->m_pageKey) {
			TopTabBase *pinnedTab = (g_topWindow->m_tabs.size() >= 2)
							? g_topWindow->m_tabs[1]
							: g_topWindow->m_tabs[0];
			afterPageKey = pinnedTab->m_pageKey;
		}
	}

	bool openPatchChk = false;
	if ((status->launchMode & 0x100000u) != 0) {
		char uriBuf[1024];
		if (sceAppMgrPeekLaunchParamForShell(launchAppId, uriBuf, sizeof(uriBuf)) == 0) {
			int scheme = topmenuParsePsgmUriScheme(uriBuf);
			if (scheme == 2) {
				autoStart = false;
				if (sceLsdbGetType(appInfo) == 1 && SceLsdb_9D27A87D(appInfo) == 0) {
					showBusyWaitDialog(g_topWindow);
					if (launchAppId != -1)
						topmenuCancelPendingAppLaunch(launchAppId);
					return;
				}
			} else if (scheme == 1 && appInfo->get() != NULL) {
				char typeBuf[128];
				if (topmenuParsePsgmType(uriBuf, typeBuf, sizeof(typeBuf)) != 0 &&
				    sce_paf_strcmp("INVITATION_MESSAGE", typeBuf) == 0 &&
				    SceLsdb_E26FC9E8(appInfo) == NULL) {
					openPatchChk = true;
					autoStart = false;
				} else if (isPsgmUpdateRequired(appInfo, uriBuf)) {
					openPatchChk = true;
					autoStart = false;
				}
			}
		}
	}

	if (g_topWindow->m_gameHoldCount > 0) {
		showBusyWaitDialog(g_topWindow);
		if (launchAppId != -1)
			topmenuCancelPendingAppLaunch(launchAppId);
		return;
	}

	if (g_topWindow->m_launchSeq.state != 0) {
		bool allowDebugger = false;
		if (!g_topWindow->m_launchSeq.autoStart &&
		    g_topWindow->m_launchSeq.closePageKey != -1) {
			TopTab *closingTab = reinterpret_cast<TopTab *>(
				g_topWindow->FindTabByPageKey(g_topWindow->m_launchSeq.closePageKey));
			if (closingTab != NULL && closingTab->m_appData != NULL &&
			    closingTab->m_appData->m_process.m_launchMode == 1) {
				allowDebugger = true;
			}
		}
		if (!allowDebugger) {
			if (launchAppId != -1)
				topmenuCancelPendingAppLaunch(launchAppId);
			return;
		}
	}

	unregisterSystemAppId(g_topWindow, launchAppId);

	TopmenuOpenAppTabParam openParam;
	openParam.titleId = titleId;
	openParam.afterPageKey = afterPageKey;
	openParam.autoStart = autoStart;
	openParam.openPatchChk = openPatchChk;
	openParam.launchAppId = launchAppId;

	TopTab *existingTab = topmenuFindTabByTitleId(g_topWindow, titleId);
	if (existingTab != NULL) {
		if (!activateExistingAppTab(g_topWindow, &openParam, existingTab->m_pageKey, true)) {
			if (launchAppId != -1)
				topmenuCancelPendingAppLaunch(launchAppId);
			return;
		}
		if (!autoStart && launchAppId != -1)
			topmenuCancelPendingAppLaunch(launchAppId);
		return;
	}

	if (g_topWindow->m_closingApp != NULL &&
	    sce_paf_strncmp(g_topWindow->m_closingApp->m_titleId.c_str(), titleId,
			    sce_paf_strlen(titleId)) == 0) {
		topmenuProcessLaunchAsync(g_topWindow->m_closingApp, launchAppId);
		g_topWindow->m_closingApp->StartOrResume(-1);
		return;
	}

	typedef int (*HasAppFn)(const paf::string *);
	paf::string titleStr(titleId);
	int hasIconInLauncher = reinterpret_cast<HasAppFn>(appIf->func[21])(&titleStr);

	if (appInfo->get() != NULL)
		openParam.appInfo = *appInfo;

	paf::intrusive_ptr<paf::cache::CacheImageInterface> iconCache;
	if (hasIconInLauncher != 0)
		createAppTabWithIcon(g_topWindow, &openParam, &iconCache);
	else
		createAppTabWithoutIcon(g_topWindow, &openParam);

	if (!autoStart && launchAppId != -1)
		topmenuCancelPendingAppLaunch(launchAppId);
}

/* 0x8127895E */
static bool shouldConfirmCloseBgApp(TopWindow *window, bool ignoreForeground, const char *titleId)
{
	if (titleId == NULL)
		return true;
	if (shellStartupGetIduMode() == 1)
		return false;

	SceInt32 bgAppId = sceAppMgrGetAppIdByAppId(-3);
	if (bgAppId == -1)
		return false;

	TopmenuProcessBase *bgProc = findProcessInListByAppId(window, bgAppId);
	if (bgProc == NULL)
		return false;
	if (ignoreForeground && bgProc == window->m_foregroundApp)
		return false;

	LsdbAppInfoPtr *bgInfo = topmenuProcessGetAppInfo(bgProc);
	SceInt64 bgPolicy = SceLsdb_43153457(bgInfo);
	if (bgPolicy == 3)
		return true;
	return bgPolicy == 2 && sce_paf_strcmp("NPXS10090", titleId) == 0;
}

/* 0x81278A00 */
static void launchSystemAppOverlay(TopWindow *window, const char *titleId,
				   const LsdbAppInfoPtr *appInfo, SceInt32 launchAppId)
{
	if (window->m_closingAppPending) {
		SceAppMgrAppStatus status;
		if (sceAppMgrGetStatusByAppId(launchAppId, &status) == 0 &&
		    (status.launchMode & 0x40000u) != 0) {
			window->m_skipDestroyCheck = true;
		}
		topmenuSwitchToProcess(window, NULL, true);
		window->m_skipDestroyCheck = false;
	}

	for (paf::vector<TopTabBase *>::iterator it = window->m_tabs.begin();
	     it != window->m_tabs.end(); ++it) {
		TopTab *tab = reinterpret_cast<TopTab *>(*it);
		if (tab->m_appData != NULL && tab->m_appData->m_dialogId == launchAppId)
			tab->m_appData->m_dialogId = -1;
	}

	bool needsConfirm = true;
	if (!shouldConfirmCloseBgApp(window, false, titleId)) {
		shellEventServerSend(10, NULL, 0);
		window->m_closingAppPending = true;
		needsConfirm = false;
	}

	TopmenuAppProcess *sysProc = new TopmenuAppProcess(paf::string(titleId), *appInfo, NULL,
							   0, true, launchAppId);
	if (needsConfirm)
		promptCloseBgApp(g_topWindow, NULL, sysProc, true);
	else
		activateSystemAppProcess(window, sysProc);
}

/* 0x81278B4A */
static void showAppNotFoundOrStoreDialog(SceInt32 errorCode, SceInt32 launchAppId)
{
	char uriBuf[1024];
	if (sceAppMgrPeekLaunchParamForShell(launchAppId, uriBuf, sizeof(uriBuf)) != 0) {
		topmenuShowAppErrorMessageDialog(errorCode, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
		return;
	}

	CommonGuiLayoutDialogParam dlgParam;
	dlgParam.priority = 33;
	dlgParam.resourcePlugin = g_topmenuPlugin;
	dlgParam.soundId = TOP_SOUND_DIALOG_OPEN;

	char titleNameUtf8[132];
	bool hasFallbackTitle = false;
	if (topmenuParsePsgmTitleName(uriBuf, titleNameUtf8, 129) == 0) {
		sce_paf_snprintf(titleNameUtf8, 129, "???");
		hasFallbackTitle = true;
	}

	paf::string titleUtf8(titleNameUtf8);
	paf::wstring titleWide;
	paf::common::Utf8ToUtf16(titleUtf8.c_str(), &titleWide);

	bool openedStorePrompt = false;
	char parsedTitleId[136];
	bool isWelcomePark = (topmenuParsePsgmTitleId(uriBuf, parsedTitleId, 129) != 0 &&
			      sce_paf_strcmp(parsedTitleId, "NPXS10007") == 0);

	if (!isWelcomePark) {
		char *queryBuf = new char[1024];
		bool isContentId = false;
		if (topmenuParsePsgmContent(uriBuf, queryBuf, 1024) != 0) {
			isContentId = true;
			openedStorePrompt = true;
		} else if (topmenuParsePsgmCombinedTitleIds(uriBuf, queryBuf, 1024) != 0) {
			isContentId = false;
			openedStorePrompt = true;
		}

		if (openedStorePrompt) {
			if (hasFallbackTitle) {
				dlgParam.text = g_topmenuPlugin->GetString(
					paf::IDParam(TOP_MSG_STORE_SEARCH_GENERIC));
			} else {
				const wchar_t *fmt = g_topmenuPlugin->GetString(
					paf::IDParam(TOP_MSG_STORE_SEARCH_NAMED));
				dlgParam.text = shellFormatText(fmt, titleWide.c_str());
			}
			dlgParam.layout = SceCommonGuiDialog_22B17F2B;
			dlgParam.handler = new TopmenuStoreBrowseHandler(queryBuf, isContentId);
			dlgParam.notifyClose = true;

			SceInt32 dlgId = SceCommonGuiDialog_CCFCD9A6(&dlgParam);
			if (dlgId >= 0) {
				topmenuDialogRegister(dlgId);
				topmenuDialogOnPowerHold(dlgId);

				paf::ui::Widget *yesBtn = SceCommonGuiDialog_8947FC23(dlgId, 12);
				paf::ui::Widget *noBtn = SceCommonGuiDialog_8947FC23(dlgId, 13);
				if (yesBtn != NULL) {
					paf::wstring yesLabel(g_topmenuPlugin->GetString(
						paf::IDParam(TOP_MSG_STORE_BTN_YES)));
					yesBtn->SetString(yesLabel);
				}
				if (noBtn != NULL) {
					paf::wstring noLabel(g_topmenuPlugin->GetString(
						paf::IDParam(TOP_MSG_STORE_BTN_NO)));
					noBtn->SetString(noLabel);
				}
			}
		}
		delete[] queryBuf;
	}

	if (!openedStorePrompt) {
		paf::wstring msgText;
		if (hasFallbackTitle) {
			msgText = g_topmenuPlugin->GetString(
				paf::IDParam(TOP_MSG_APP_NOT_FOUND_GENERIC));
		} else {
			msgText = g_topmenuPlugin->GetString(
				paf::IDParam(TOP_MSG_APP_NOT_FOUND_NAMED));
			msgText.append(L"\n\n");
			msgText.append(titleWide);
		}

		dlgParam.text = msgText;
		dlgParam.layout = SceCommonGuiDialog_AF75CA6D;
		dlgParam.handler = new TopmenuOwnedOkDialogHandler();
		dlgParam.notifyClose = true;

		SceInt32 dlgId = SceCommonGuiDialog_CCFCD9A6(&dlgParam);
		if (dlgId >= 0) {
			topmenuDialogRegister(dlgId);
			topmenuDialogOnPowerHold(dlgId);
		}
	}
}

/* 0x81279276 */
static void topmenuDispatchAppLaunch(const paf::string *titleId, const LsdbAppInfoPtr *appInfo,
				     SceInt32 launchAppId, SceInt32 searchError)
{
	SceAppMgrAppStatus status;
	int statusRes = sceAppMgrGetStatusByAppId(launchAppId, &status);

	if (searchError != 0) {
		if (SceLsdb_9D27A87D(appInfo) == 3)
			showAppNotFoundOrStoreDialog(-2146420479, launchAppId);
		else
			showAppNotFoundOrStoreDialog(-2146420473, launchAppId);
		if (launchAppId != -1)
			topmenuCancelPendingAppLaunch(launchAppId);
		return;
	}

	if (statusRes != 0) {
		if (launchAppId != -1)
			topmenuCancelPendingAppLaunch(launchAppId);
		return;
	}

	unregisterSystemAppId(g_topWindow, launchAppId);
	SceUInt32 appAttr = SceLsdb_45B38A03(appInfo);
	topmenuGetApplauncherInterface();

	if ((appAttr & 1u) != 0) {
		paf::common::SharedPtr<paf::job::JobItem> bgJob;
		topmenuStartSpawnBgJob(&bgJob, appInfo, launchAppId);
		return;
	}

	if (sceLsdbGetMetaContentsPath(appInfo) != 3) {
		LsdbAppInfoPtr infoCopy(*appInfo);
		topmenuHandleAppLaunchByStatus(titleId->c_str(), &infoCopy, &status, launchAppId);
		return;
	}

	if (sce_paf_strncmp(titleId->c_str(), "NPXS10090", sce_paf_strlen("NPXS10090")) == 0) {
		SceInt32 browserRestrict = 0;
		SceInt32 regRes = sceRegMgrGetKeyInt("/CONFIG/SECURITY/PARENTAL",
							    "browser_start_control",
							    &browserRestrict);
		if (regRes != 0) {
			if (launchAppId != -1)
				topmenuCancelPendingAppLaunch(launchAppId);
			topmenuShowAppError(regRes, appInfo, new TopmenuParentalBlockedHandler(),
					    NULL, NULL, NULL, NULL, NULL);
			return;
		}

		SceAppMgrAppStatus callerStatus;
		sceAppMgrGetStatusByAppId(status.launchSourceAppId, &callerStatus);
		if (sce_paf_strncmp(callerStatus.appName, "NPXS10094", 0x20u) == 0) {
			browserRestrict = 0;
		} else if (browserRestrict != 0) {
			if (launchAppId != -1)
				topmenuCancelPendingAppLaunch(launchAppId);
			topmenuShowAppError(-2146430685, appInfo, new TopmenuParentalBlockedHandler(),
					    NULL, NULL, NULL, NULL, NULL);
			return;
		}
	}

	if (sce_paf_strncmp(titleId->c_str(), "NPXS10035", sce_paf_strlen("NPXS10035")) == 0) {
		SceInt32 storeRestrict = 0;
		if (sceRegMgrGetKeyInt("/CONFIG/SECURITY/PARENTAL",
					      "store_start_control", &storeRestrict) == 0 &&
		    storeRestrict != 0) {
			if (launchAppId != -1)
				topmenuCancelPendingAppLaunch(launchAppId);
			topmenuShowAppError(-2146430685, appInfo, new TopmenuParentalBlockedHandler(),
					    NULL, NULL, NULL, NULL, NULL);
			return;
		}
	}

	launchSystemAppOverlay(g_topWindow, titleId->c_str(), appInfo, launchAppId);
}

/* 0x81279646 */
static void onInvitationLaunchDialogButton(SceInt32 dialogId, SceInt32 button,
					   bool *handled, bool *confirmed,
					   const char *titleId, const LsdbAppInfoPtr *appInfo,
					   SceInt32 launchAppId)
{
	if (button != 0) {
		if (button == 2) {
			*confirmed = true;
			*handled = true;
			topmenuBeginBusy();
		} else if (button == 3) {
			*handled = true;
		}
		SceCommonGuiDialog_BC311DC0(dialogId);
		return;
	}

	if (*confirmed) {
		topmenuEndBusy();
		paf::string titleStr(titleId);
		topmenuDispatchAppLaunch(&titleStr, appInfo, launchAppId, 0);
	} else if (launchAppId != -1) {
		topmenuCancelPendingAppLaunch(launchAppId);
	}

	topmenuDialogOnPowerRelease(dialogId);
}

/* 0x8127978E */
static void snapScrollViewToTab(TopWindow *window, TopTabBase *tab)
{
	paf::ui::Widget *tabWidget = tab->m_widget;
	SceInt32 snapId = window->m_scrollView->GetSnapID(tabWidget);
	paf::math::v4 centerPos = tabWidget->GetPosCenter(0, 0, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	paf::math::v2 snapPos(centerPos.extract_x(), centerPos.extract_y());
	paf::ui::ScrollViewSnapPoint snapPoint(snapPos);
	window->m_scrollView->UpdateSnapPoint(snapId, snapPoint);

	paf::Timer *timer = new paf::Timer();
	timer->InitTimer(300.0f, 23, 0, -1, NULL);
	window->m_scrollView->DoSnap(snapId, timer);
}

/* 0x81279874 */
static void finishOpeningLiveArea(TopWindow *window)
{
	if (!window->m_openingLiveArea)
		return;

	SceInt32 openPageKey = window->m_openingPageKey;
	window->m_liveAreaReady = false;
	window->m_openingLiveArea = false;

	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(openPageKey));
	if (tab != NULL && tab->m_appData != NULL)
		tab->m_appData->OnBgLoadStepDone();

	if (window->m_launchSeq.targetPageKey == window->m_openingPageKey) {
		topmenuHelpCancelLiveAreaIntro();
		topmenuCancelLaunchSequence(&window->m_launchSeq);
	}

	window->m_openingPageKey = -1;
}

/* 0x812798CA */
static void enterEditMode(TopWindow *window)
{
	if (window->m_editModeActive)
		return;

	topmenuHelpShowEditHelp();
	window->m_editModeActive = true;

	paf::math::v4 scrollSize = *window->m_scrollView->GetSize(0);
	paf::math::v4 scrollPos =
		window->m_scrollView->GetPos(0, 0, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	scrollPos = paf::math::v4(scrollPos.extract_x(), 0.0f,
				  scrollPos.extract_z(), scrollPos.extract_w());
	scrollSize = paf::math::v4(scrollSize.extract_x(), 544.0f,
				   scrollSize.extract_z(), scrollSize.extract_w());
	g_topWindow->m_scrollView->SetMovableDirOn(3u);

	if (window->m_scrollView->m_map_t_listener.find(TIMER_EDIT_MODE_ANIM_ID) != window->m_scrollView->m_map_t_listener.end()) {
		window->m_scrollView->KillTimer(TIMER_EDIT_MODE_ANIM_ID);
		releaseInputBlocker(g_topWindow);
	}

	acquireInputBlocker(window);
	window->m_scrollView->SetTimerListener(
		TIMER_EDIT_MODE_ANIM_ID, 300.0f,
		new paf::ui::EventCBListener(onEditModeAnimTimer, NULL));

	paf::Timer *sizeTimer = new paf::Timer();
	sizeTimer->InitTimer(300.0f, 35, 0, -1, NULL);
	window->m_scrollView->SetSize(scrollSize, sizeTimer, 0x10004, 2, NULL, NULL);

	paf::Timer *posTimer = new paf::Timer();
	posTimer->InitTimer(300.0f, 35, 0, -1, NULL);
	window->m_scrollView->SetPos(scrollPos, posTimer, 0x10000, 2, NULL, NULL);

	if (window->m_tabs.size() > 1) {
		paf::ui::Widget *secondTabWidget = window->m_tabs[1]->m_widget;
		paf::math::v4 tabPos =
			secondTabWidget->GetPos(0, 0, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		tabPos = paf::math::v4(960.0f, tabPos.extract_y(),
				       tabPos.extract_z(), tabPos.extract_w());
		secondTabWidget->KillAnim(0x10000);

		paf::Timer *slideTimer = new paf::Timer();
		slideTimer->InitTimer(300.0f, 35, 0, -1, NULL);
		secondTabWidget->SetPos(tabPos, slideTimer, 0x10000, 0, NULL, NULL);
	}
}

/* 0x81279BEA */
static void leaveEditMode(TopWindow *window)
{
	if (!window->m_editModeActive)
		return;

	window->m_editModeActive = false;

	paf::math::v4 scrollSize = *window->m_scrollView->GetSize(0);
	paf::math::v4 scrollPos =
		window->m_scrollView->GetPos(0, 0, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	scrollSize = paf::math::v4(scrollSize.extract_x(), 512.0f,
				   scrollSize.extract_z(), scrollSize.extract_w());
	scrollPos = paf::math::v4(scrollPos.extract_x(), -16.0f,
				  scrollPos.extract_z(), scrollPos.extract_w());
	g_topWindow->m_scrollView->SetMovableDirOn(0u);

	if (window->m_scrollView->m_map_t_listener.find(TIMER_EDIT_MODE_ANIM_ID) != window->m_scrollView->m_map_t_listener.end()) {
		window->m_scrollView->KillTimer(TIMER_EDIT_MODE_ANIM_ID);
		releaseInputBlocker(g_topWindow);
	}

	acquireInputBlocker(window);
	window->m_scrollView->SetTimerListener(
		TIMER_EDIT_MODE_ANIM_ID, 300.0f,
		new paf::ui::EventCBListener(onEditModeAnimTimer, NULL));

	paf::Timer *sizeTimer = new paf::Timer();
	sizeTimer->InitTimer(300.0f, 35, 0, -1, NULL);
	window->m_scrollView->SetSize(scrollSize, sizeTimer, 0x10004, 2, NULL, NULL);

	paf::Timer *posTimer = new paf::Timer();
	posTimer->InitTimer(300.0f, 35, 0, -1, NULL);
	window->m_scrollView->SetPos(scrollPos, posTimer, 0x10000, 2, NULL, NULL);

	if (window->m_tabs.size() > 1) {
		paf::ui::Widget *secondTabWidget = window->m_tabs[1]->m_widget;
		paf::math::v4 tabPos =
			secondTabWidget->GetPos(0, 0, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		tabPos = paf::math::v4(static_cast<float>(window->m_tabSpacing),
				       tabPos.extract_y(),
				       tabPos.extract_z(), tabPos.extract_w());
		secondTabWidget->KillAnim(0x10000);

		paf::Timer *slideTimer = new paf::Timer();
		slideTimer->InitTimer(300.0f, 35, 0, -1, NULL);
		secondTabWidget->SetPos(tabPos, slideTimer, 0x10000, 0, NULL, NULL);
	}
}

/* 0x81279F06 */
void topmenuOnTabDestroyed(TopWindow *window, TopTab *tab)
{
	(void)window;
	if (tab->m_appData != NULL && topmenuAppProcessIsRunning(&tab->m_appData->m_process))
		tab->m_appData->m_process.SendProcessCommand(5);
}

/* ------------------------------------------------------------------ */
/* Chunk 3: Carousel 3D tilt, appmgr event dispatch, main tick        */
/* (0x81279F26..0x8127ECA4)                                           */
/* ------------------------------------------------------------------ */

/* 0x81279F26 */
static void updateCarouselLayoutAndTilt(TopWindow *window)
{
	if (window->m_carouselDragging) {
		SceInt32 hoveredIdx = (window->m_hoveredTab != NULL)
					      ? window->m_hoveredTab->m_index
					      : -1;
		paf::math::v4 pos(0.0f, 0.0f, 0.0f, 0.0f);
		SceInt32 idx = 0;

		for (paf::vector<TopTabBase *>::iterator it = window->m_tabs.begin();
		     it != window->m_tabs.end(); ++it, ++idx) {
			TopTab *tab = reinterpret_cast<TopTab *>(*it);
			if (idx == 0)
				continue;

			float stepX;
			if (idx == 1) {
				if (hoveredIdx < 0)
					stepX = 84.0f;
				else if (hoveredIdx == 0)
					stepX = 192.0f;
				else
					stepX = 58.0f;
			} else {
				if (hoveredIdx < 0)
					stepX = 136.0f;
				else if (hoveredIdx == idx - 1)
					stepX = 252.0f;
				else
					stepX = 118.0f;
			}

			pos = paf::math::v4(pos.extract_x() + stepX, 0.0f, 0.0f, 0.0f);
			paf::Timer *timer = new paf::Timer();
			timer->InitTimer(300.0f, 35, 0, -1, NULL);
			tab->m_widget->SetPos(pos, timer, 0x10000, 2, NULL, NULL);
			tab->m_isClosing = (hoveredIdx == idx);
		}
		window->m_carouselDragging = false;
	}

	paf::ui::Camera *camera = g_topWindow->m_scrollView->m_scene->m_camera;
	ScePafWidget_72F479CA(camera);
	float cameraZ = camera->m_pos.extract_z();

	int idx = 0;
	for (paf::vector<TopTabBase *>::iterator it = window->m_tabs.begin();
	     it != window->m_tabs.end(); ++it, ++idx) {
		TopTab *tab = reinterpret_cast<TopTab *>(*it);
		if (idx == 0)
			continue;

		paf::ui::Widget *tabWidget = tab->m_widget;
		paf::math::v4 rot = tab->m_contentRoot->GetRot(
			0, 0, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f), 0);
		if ((tabWidget->m_update_flag & 7) != 0)
			tabWidget->UpdateMatrix();

		float worldX = tabWidget->m_world_matrix.w().extract_x();
		float halfWidth = tab->m_contentRoot->GetSize(0)->extract_x() * 0.5f;
		float rotY = rot.extract_y();
		float sinY = sce_paf_sinf(rotY);
		float cosY = sce_paf_cosf(rotY);

		float deltaMs = (paf::Timer::s_tick_unit * paf::Timer::s_tick_interval) * 1000.0f;
		float progress = tab->m_hoverProgress;
		if (tab->m_isClosing) {
			progress += deltaMs;
			if (progress > 100.0f)
				progress = 100.0f;
		} else {
			progress -= deltaMs;
			if (progress < 0.0f)
				progress = 0.0f;
		}
		tab->m_hoverProgress = progress;

		float eased = paf::Timer::DoFuncQuadInOut(progress / 100.0f);
		float depthOffset = (eased * -75.0f) + 25.0f;
		float tiltRatio = rotY / 0.43633235f;
		float shiftZ = -(halfWidth * sinY) - (depthOffset * tiltRatio);
		float depthFromCam = cameraZ - (depthOffset * tiltRatio);
		float shiftX = ((((worldX - 420.0f) * (cameraZ / depthFromCam)) -
				 (worldX - 420.0f)) *
				(depthFromCam / cameraZ)) -
			       ((1.0f - cosY) * halfWidth);

		paf::math::v4 contentPos(shiftX, 0.0f, shiftZ, 1.0f);
		tab->m_contentRoot->SetPos(contentPos, NULL, 0x10000, 0, NULL, NULL);
	}
}

/* 0x8127A396 */
void topmenuSetPageTurnDragging(TopWindow *window, bool dragging)
{
	if (window->m_touchForceResume == dragging)
		return;
	window->m_touchForceResume = dragging;
	syncTouchDefaultRegions(window);
}

/* 0x8127A3DA */
TopTab *topmenuFindDebuggerTab(TopWindow *window)
{
	for (paf::vector<TopTabBase *>::iterator it = window->m_tabs.begin();
	     it != window->m_tabs.end(); ++it) {
		TopTab *tab = reinterpret_cast<TopTab *>(*it);
		if (tab->m_appData != NULL) {
			SceInt32 mode = tab->m_appData->m_process.m_launchMode;
			if (mode == 1 || mode == 2 || mode == 3)
				return tab;
		}
	}
	return NULL;
}

/* 0x8127A418 */
static int reloadLiveAreaForTitle(TopWindow *window, const LiveAreaReloadRequest *req)
{
	(void)window;
	paf::thread::RMutex::main_thread_mutex.Lock();

	const char *titleId = req->titleId;
	TopTab *tab = topmenuFindTabByTitleId(g_topWindow, titleId);
	if (tab == NULL) {
		if (sce_paf_strncmp(titleId, "NPXS10999", sce_paf_strlen("NPXS10999")) == 0) {
			tab = topmenuFindDebuggerTab(g_topWindow);
		} else if (sce_paf_strncmp(titleId, "NPXS10998",
					   sce_paf_strlen("NPXS10998")) == 0) {
			paf::thread::RMutex::main_thread_mutex.Unlock();
			return 0;
		}
	}

	int result = 0;
	if (tab != NULL)
		result = tab->m_appData->ReloadLiveArea(req);

	paf::thread::RMutex::main_thread_mutex.Unlock();
	return result;
}

/* 0x8127A53A */
void topmenuDismissIduOverlay(TopWindow *window)
{
	if (window->m_iduOverlayWidget != NULL) {
		window->m_iduOverlayWidget->DestroyWidget();
		window->m_iduOverlayWidget = NULL;
	}
}

/* 0x8127A552 */
void topmenuReturnFromIduDemoApp(TopWindow *window)
{
	if (g_topWindow->m_busyRefCount > 0 || shellStartupGetIduMode() != 1)
		return;
	if (window->m_foregroundApp == NULL || window->m_tabs.empty())
		return;
	if (window->m_foregroundApp->m_tabAppData == NULL)
		return;

	TopTabBase *homeTab = (window->m_switchToHomeOnClose && window->m_tabs.size() >= 2)
				      ? window->m_tabs[1]
				      : window->m_tabs[0];
	TopmenuLaunchSeqParam seqParam;
	seqParam.targetPageKey = homeTab->m_pageKey;
	seqParam.autoStart = false;
	seqParam.holdBusy = true;
	seqParam.skipScroll = false;
	seqParam.closePageKey = window->m_foregroundApp->m_tabAppData->m_ownerTab->m_pageKey;
	seqParam.immediateStart = false;
	seqParam.isWebApp = false;
	seqParam.openPatchChk = false;
	seqParam.notifyApplauncher = false;
	topmenuStartLaunchSequence(&window->m_launchSeq, &seqParam);
}

/* 0x8127A5EA */
void topmenuUpdateIduAfterAppFailure(TopWindow *window, TopTab *tab)
{
	if (tab == NULL || shellStartupGetIduMode() != 1 || window->m_tabs.empty())
		return;

	TopTabBase *homeTab = (window->m_switchToHomeOnClose && window->m_tabs.size() >= 2)
				      ? window->m_tabs[1]
				      : window->m_tabs[0];
	TopmenuLaunchSeqParam seqParam;
	seqParam.targetPageKey = homeTab->m_pageKey;
	seqParam.autoStart = false;
	seqParam.holdBusy = true;
	seqParam.skipScroll = false;
	seqParam.closePageKey = tab->m_pageKey;
	seqParam.immediateStart = false;
	seqParam.isWebApp = false;
	seqParam.openPatchChk = false;
	seqParam.notifyApplauncher = false;
	topmenuStartLaunchSequence(&window->m_launchSeq, &seqParam);
}

/* 0x8127A664 */
static int ifGetPageKeyAt(int index)
{
	if (index >= 0 && static_cast<size_t>(index) < g_topWindow->m_tabs.size())
		return g_topWindow->m_tabs[index]->m_pageKey;
	return -1;
}

/* 0x8127A688 */
bool topmenuCanPromoteApp(TopWindow *window, int checkMode, bool ignoreActive, bool force)
{
	paf::thread::RMutex::main_thread_mutex.Lock();

	if (checkMode != 0) {
		if ((window->m_gateEffectActive && window->m_foregroundApp != NULL) ||
		    window->m_suspendLockCount > 0 || window->m_busyRefCount > 0) {
			paf::thread::RMutex::main_thread_mutex.Unlock();
			return false;
		}
	}

	ApplauncherInterface *appIf = topmenuGetApplauncherInterface();
	if (appIf != NULL)
		appIf->func[15]();
	if (window->m_openingLiveArea && appIf != NULL)
		appIf->func[8]();

	if (ignoreActive) {
		SceInt32 homePageKey = -1;
		if (!g_topWindow->m_tabs.empty())
			homePageKey = g_topWindow->m_tabs[0]->m_pageKey;

		TopmenuLaunchSeqParam seqParam;
		seqParam.targetPageKey = homePageKey;
		seqParam.autoStart = false;
		seqParam.holdBusy = true;
		seqParam.skipScroll = false;
		seqParam.closePageKey = -1;
		seqParam.immediateStart = false;
		seqParam.isWebApp = false;
		seqParam.openPatchChk = false;
		seqParam.notifyApplauncher = force;
		topmenuStartLaunchSequence(&window->m_launchSeq, &seqParam);
	}

	paf::thread::RMutex::main_thread_mutex.Unlock();
	return true;
}

/* 0x8127A784 */
TopmenuAppProcess *topmenuFindProcessByAppId(TopWindow *window, SceInt32 pid)
{
	for (paf::vector<TopTabBase *>::iterator it = window->m_tabs.begin();
	     it != window->m_tabs.end(); ++it) {
		TopTab *tab = reinterpret_cast<TopTab *>(*it);
		if (tab->m_appData != NULL &&
		    topmenuAppProcessIsRunning(&tab->m_appData->m_process) &&
		    topmenuProcessGetPid(&tab->m_appData->m_process) == pid) {
			return &tab->m_appData->m_process;
		}
	}

	if (window->m_closingApp != NULL && topmenuProcessGetPid(window->m_closingApp) == pid)
		return window->m_closingApp;
	return NULL;
}

/* 0x8127A7FC */
TopmenuAppProcess *topmenuFindProcessByRunningAppId(TopWindow *window, SceInt32 appId)
{
	for (paf::vector<TopTabBase *>::iterator it = window->m_tabs.begin();
	     it != window->m_tabs.end(); ++it) {
		TopTab *tab = reinterpret_cast<TopTab *>(*it);
		if (tab->m_appData != NULL &&
		    topmenuAppProcessIsRunning(&tab->m_appData->m_process) &&
		    tab->m_appData->m_process.m_appId == appId) {
			return &tab->m_appData->m_process;
		}
	}

	if (window->m_closingApp != NULL && window->m_closingApp->m_appId == appId)
		return window->m_closingApp;
	return NULL;
}

/* 0x8127A874 */
void topmenuRecordLaunchedAppFlags(TopWindow *window, SceInt32 appId,
				   const char *appName, SceInt64 attribute)
{
	for (paf::list<TopmenuAppFlagEntry>::iterator it = window->m_appFlagList.begin();
	     it != window->m_appFlagList.end(); ++it) {
		if (it->appId == appId) {
			if (!it->registered) {
				it->flags = static_cast<SceUInt64>(attribute);
				it->registered = true;
				if ((attribute & 0x80000LL) != 0 &&
				    g_topWindow->m_powerManageIf != NULL) {
					g_topWindow->m_powerManageIf->releasePowerSwitch();
				}
			}
			return;
		}
	}

	TopmenuAppFlagEntry entry;
	entry.flags = static_cast<SceUInt64>(attribute);
	entry.appId = appId;
	sce_paf_strncpy(entry.appName, appName, 0x20u);
	entry.registered = true;
	if ((attribute & 0x80000LL) != 0 && g_topWindow->m_powerManageIf != NULL)
		g_topWindow->m_powerManageIf->releasePowerSwitch();
	window->m_appFlagList.push_back(entry);
}

/* 0x8127A944 */
static void removeLaunchedAppFlags(TopWindow *window, SceInt32 appId)
{
	for (paf::list<TopmenuAppFlagEntry>::iterator it = window->m_appFlagList.begin();
	     it != window->m_appFlagList.end(); ++it) {
		if (it->appId == appId) {
			if ((it->flags & 0x80000ULL) != 0 &&
			    g_topWindow->m_powerManageIf != NULL) {
				g_topWindow->m_powerManageIf->holdPowerSwitch();
			}
			window->m_appFlagList.erase(it);
			return;
		}
	}
}

/* 0x8127A9AC */
bool topmenuHasRunningAppWithTitleId(TopWindow *window, const char *titleId)
{
	for (paf::list<TopmenuAppFlagEntry>::iterator it = window->m_appFlagList.begin();
	     it != window->m_appFlagList.end(); ++it) {
		if (sce_paf_strncmp(it->appName, titleId, 0x20u) == 0)
			return true;
	}
	return false;
}

/* 0x8127A9F2 */
void topmenuOnAppInfoSearchDone(const paf::string *titleId, const LsdbAppInfoPtr *appInfo,
				SceInt32 launchAppId)
{
	SceAppMgrAppStatus status;
	int statusRes = sceAppMgrGetStatusByAppId(launchAppId, &status);

	if (titleId->c_str() == NULL || titleId->size() == 0) {
		showAppNotFoundOrStoreDialog(-2146420473, launchAppId);
		if (launchAppId != -1)
			topmenuCancelPendingAppLaunch(launchAppId);
		return;
	}

	if (statusRes != 0) {
		if (launchAppId != -1)
			topmenuCancelPendingAppLaunch(launchAppId);
		return;
	}

	TopTab *existingTab = topmenuFindTabByTitleId(g_topWindow, titleId->c_str());
	if (existingTab != NULL) {
		LsdbAppInfoPtr emptyInfo;
		topmenuHandleAppLaunchByStatus(titleId->c_str(), &emptyInfo, &status, launchAppId);
	} else {
		paf::string titleCopy(titleId->c_str());
		topmenuDispatchAppLaunch(&titleCopy, appInfo, launchAppId, 0);
	}
}

/* 0x8127AB9A */
TopmenuAppProcess *topmenuFindProcessByTitleId(TopWindow *window, const char *titleId)
{
	for (paf::vector<TopTabBase *>::iterator it = window->m_tabs.begin();
	     it != window->m_tabs.end(); ++it) {
		TopTab *tab = reinterpret_cast<TopTab *>(*it);
		if (tab->m_appData != NULL) {
			const char *tabTitle = tab->m_appData->m_titleId.c_str();
			if (sce_paf_strncmp(titleId, tabTitle, sce_paf_strlen(tabTitle)) == 0)
				return &tab->m_appData->m_process;
		}
	}

	if (window->m_closingApp != NULL &&
	    sce_paf_strncmp(window->m_closingApp->m_titleId.c_str(), titleId,
			    sce_paf_strlen(titleId)) == 0) {
		return window->m_closingApp;
	}
	return NULL;
}

/* 0x8127AC1E */
void topmenuOnUriSearchDone(SceInt32 result, const char *titleId, SceInt32 launchAppId)
{
	SceAppMgrAppStatus status;
	if (sceAppMgrGetStatusByAppId(launchAppId, &status) != 0) {
		unregisterSystemAppId(g_topWindow, launchAppId);
		return;
	}

	if (titleId != NULL && result == 0) {
		if (shellBootIsLaunchRestricted()) {
			if (sce_paf_strncmp(titleId, "NPXS10037",
					    sce_paf_strlen("NPXS10037")) == 0) {
				sceAppMgrLaunchAppByName2ndStage(
					launchAppId, "vs0:app/NPXS10037/eboot.bin",
					0x4000000, 1, NULL);
			} else if (sce_paf_strncmp(titleId, "NPXS10084",
						   sce_paf_strlen("NPXS10084")) == 0) {
				sceAppMgrLaunchAppByName2ndStage(
					launchAppId, "vs0:app/NPXS10084/eboot.bin",
					0x4000000, 1, NULL);
			} else if (sce_paf_strncmp(titleId, "NPXS10016",
						   sce_paf_strlen("NPXS10016")) == 0) {
				AppmgrLaunchAppOption launchOpt;
				sce_paf_memset(&launchOpt, 0, sizeof(launchOpt));
				if (status.launchSourceAppId == -1) {
					sce_paf_strncpy(launchOpt.proxyAppName, "NPXS19999", 0x20u);
				} else {
					SceAppMgrAppStatus callerStatus;
					if (sceAppMgrGetStatusByAppId(status.launchSourceAppId,
								      &callerStatus) == 0) {
						sce_paf_strncpy(launchOpt.proxyAppName,
								callerStatus.appName, 0x20u);
					}
				}
				sceAppMgrLaunchAppByName2ndStage(
					launchAppId, "vs0:app/NPXS10015/eboot.bin",
					0x2000000, 0, &launchOpt);
			} else if (launchAppId != -1) {
				topmenuCancelPendingAppLaunch(launchAppId);
			}
			return;
		}

		if (topmenuFindProcessByTitleId(g_topWindow, titleId) != NULL) {
			LsdbAppInfoPtr emptyInfo;
			topmenuHandleAppLaunchByStatus(titleId, &emptyInfo, &status, launchAppId);
			return;
		}

		paf::common::SharedPtr<paf::job::JobItem> job;
		paf::string titleStr(titleId);
		lsdbEnqueueGetAppInfoJob(&job, g_topWindow->m_jobQueue, titleStr,
					 onLsdbAppInfoForUri,
					 reinterpret_cast<void *>(static_cast<intptr_t>(launchAppId)));
		return;
	}

	bool showedPsCloudPrompt = false;
	if ((status.launchMode & 0x100000u) != 0) {
		char *uriBuf = new char[1024];
		if (sceAppMgrPeekLaunchParamForShell(launchAppId, uriBuf, 1024) == 0 &&
		    sce_paf_strncmp(uriBuf, "pscloud:", 8u) == 0) {
			showedPsCloudPrompt = true;

			CommonGuiLayoutDialogParam dlgParam;
			dlgParam.priority = 33;
			dlgParam.resourcePlugin = g_topmenuPlugin;
			dlgParam.soundId = TOP_SOUND_DIALOG_OPEN;
			dlgParam.text = g_topmenuPlugin->GetString(
				paf::IDParam(TOP_MSG_PSCLOUD_STORE_PROMPT));
			dlgParam.layout = SceCommonGuiDialog_22B17F2B;
			dlgParam.handler = new TopmenuStoreBrowseHandler("PCSI00013", false);
			dlgParam.notifyClose = true;

			SceInt32 dlgId = SceCommonGuiDialog_CCFCD9A6(&dlgParam);
			if (dlgId >= 0) {
				topmenuDialogRegister(dlgId);
				topmenuDialogOnPowerHold(dlgId);

				paf::ui::Widget *yesBtn = SceCommonGuiDialog_8947FC23(dlgId, 12);
				paf::ui::Widget *noBtn = SceCommonGuiDialog_8947FC23(dlgId, 13);
				if (yesBtn != NULL) {
					paf::wstring yesLabel(g_topmenuPlugin->GetString(
						paf::IDParam(TOP_MSG_STORE_BTN_YES)));
					yesBtn->SetString(yesLabel);
				}
				if (noBtn != NULL) {
					paf::wstring noLabel(g_topmenuPlugin->GetString(
						paf::IDParam(TOP_MSG_STORE_BTN_NO)));
					noBtn->SetString(noLabel);
				}
			}
		}
		delete[] uriBuf;
	}

	if (!showedPsCloudPrompt && result != 0) {
		if (result == -2146430971) {
			CommonGuiLayoutDialogParam dlgParam;
			dlgParam.priority = 33;
			dlgParam.resourcePlugin = g_topmenuPlugin;
			dlgParam.soundId = TOP_SOUND_DIALOG_OPEN;
			dlgParam.text = g_topmenuPlugin->GetString(
				paf::IDParam(TOP_MSG_APP_NOT_FOUND_GENERIC));
			dlgParam.layout = SceCommonGuiDialog_AF75CA6D;
			dlgParam.handler = new TopmenuOwnedOkDialogHandler();
			dlgParam.notifyClose = true;

			SceInt32 dlgId = SceCommonGuiDialog_CCFCD9A6(&dlgParam);
			if (dlgId >= 0) {
				topmenuDialogRegister(dlgId);
				topmenuDialogOnPowerHold(dlgId);
			}
		} else {
			topmenuShowAppErrorMessageDialog(result, NULL, NULL, NULL,
							 NULL, NULL, NULL, NULL);
		}
	}

	if (launchAppId != -1)
		topmenuCancelPendingAppLaunch(launchAppId);
}

/* 0x8127B420 */
void topmenuSuspendBgPartners(void)
{
	SceAppMgrAppStatus status;
	if (sceAppMgrGetStatusByName("NPXS10083", &status) == 0)
		sceAppMgrSuspendBgAppByShell(status.appId);
	if (sceAppMgrGetStatusByName("NPXS10084", &status) == 0)
		sceAppMgrSuspendBgAppByShell(status.appId);
}

/* 0x8127B472 */
void topmenuResumeOrDestroyBgPartners(SceInt32 parentAppId)
{
	SceAppMgrAppStatus status;
	if (sceAppMgrGetStatusByName("NPXS10083", &status) == 0) {
		if (parentAppId == status.launchSourceAppId)
			sceAppMgrResumeBgAppByShell(status.appId);
		else
			sceAppMgrDestroyAppByName("NPXS10083");
	}
	if (sceAppMgrGetStatusByName("NPXS10084", &status) == 0) {
		if (parentAppId == status.launchSourceAppId)
			sceAppMgrResumeBgAppByShell(status.appId);
		else
			sceAppMgrDestroyAppByName("NPXS10084");
	}
}

/* 0x8127B4EE */
static void returnToHomeOrIdu(TopWindow *window)
{
	if (shellStartupGetIduMode() == 1 && !window->m_tabs.empty()) {
		TopTabAppData *activeAppData = (window->m_foregroundApp != NULL)
						       ? window->m_foregroundApp->m_tabAppData
						       : NULL;
		TopTabBase *homeTab = (window->m_switchToHomeOnClose && window->m_tabs.size() >= 2)
					      ? window->m_tabs[1]
					      : window->m_tabs[0];

		TopmenuLaunchSeqParam seqParam;
		seqParam.targetPageKey = homeTab->m_pageKey;
		seqParam.autoStart = false;
		seqParam.holdBusy = true;
		seqParam.skipScroll = false;
		seqParam.closePageKey = (activeAppData != NULL)
						? activeAppData->m_ownerTab->m_pageKey
						: -1;
		seqParam.immediateStart = false;
		seqParam.isWebApp = false;
		seqParam.openPatchChk = false;
		seqParam.notifyApplauncher = false;
		topmenuStartLaunchSequence(&window->m_launchSeq, &seqParam);

		ApplauncherInterface *appIf = topmenuGetApplauncherInterface();
		if (appIf != NULL)
			appIf->func[10]();
	} else {
		topmenuSwitchToProcess(window, NULL, true);
	}
}

/* 0x8127B5AE */
static void onTopWindowPsButton(TopWindow *window)
{
	if (window->m_busyRefCount > 0) {
		if (window->m_imposeIf != NULL)
			window->m_imposeIf->zoomRefuse();
		return;
	}

	if (window->m_carouselState == 2 || window->m_carouselState == 1) {
		topmenuSelectHeaderTab(NULL);
		return;
	}

	TopmenuAppProcess *activeProc = window->m_foregroundApp;
	if (activeProc == NULL) {
		g_topmenuParentalBypassOnce = false;
		power_manage::AuthInterface *authIf =
			window->m_powerManageIf->getAuthInterface();
		if (authIf != NULL)
			authIf->cancelAuth();
		openCarouselView();
		return;
	}

	SceAppMgrAppStatus status;
	if (g_topmenuPsCloseDialogId < 0 &&
	    sceAppMgrGetStatusByAppId(activeProc->m_appId, &status) == 0 &&
	    status.protectionMode == 1) {
		CommonGuiLayoutDialogParam dlgParam;
		dlgParam.dimmer = 1;
		dlgParam.layout = SceCommonGuiDialog_6CD802FC;
		dlgParam.resourcePlugin = g_topmenuPlugin;
		dlgParam.text = getTopWindowPlugin(window)->GetString(
			paf::IDParam(TOP_MSG_PS_CLOSE_PROTECTED));
		dlgParam.handler = new TopmenuPsCloseAppHandler();

		g_topmenuPsCloseDialogId = shellStateWatcherOpenDialog(&dlgParam, true, 1);
		return;
	}

	TopTabAppData *appData = activeProc->m_tabAppData;
	if (appData != NULL) {
		if (appData->m_viewKind == 1) {
			shellAppSetForeground(0, "NPXS19999", 0);
			topmenuHelpPrepareLiveAreaPeel();
			returnToHomeOrIdu(window);
		} else if (appData->m_appViewState == 1) {
			shellAppSetForeground(0, "NPXS19999", 0);
			appData->OnLeaveViewport();
			activeProc->SendProcessCommand(4);
		} else {
			openCarouselView();
		}
		return;
	}

	shellAppSetForeground(0, "NPXS19999", 0);
	for (paf::vector<TopTabBase *>::iterator it = window->m_tabs.begin();
	     it != window->m_tabs.end(); ++it) {
		TopTab *tab = reinterpret_cast<TopTab *>(*it);
		if (tab->m_appData != NULL && tab->m_appData->m_appViewState == 1) {
			tab->m_appData->OnLeaveViewport();
			tab->m_appData->m_process.SendProcessCommand(4);
		}
	}

	TopTab *focusedTab = reinterpret_cast<TopTab *>(g_topWindow->FindFocusedTab());
	setFocusedTab(window, focusedTab, false);
}

/* 0x8127B864 */
void topmenuSetHoveredTab(TopTab *tab)
{
	if (g_topWindow->m_carouselState != 0 && g_topWindow->m_carouselState != 3 &&
	    g_topWindow->m_hoveredTab != tab) {
		g_topWindow->m_hoveredTab = tab;
		g_topWindow->m_carouselDragging = true;
		updateCarouselLayoutAndTilt(g_topWindow);
	}
}

/* 0x8127B89C */
void topmenuFocusHoveredTab(TopTab *tab)
{
	if (g_topWindow->m_carouselState != 0 && g_topWindow->m_carouselState != 3)
		g_topWindow->m_draggedTab = tab;
}

/* 0x8127B8B6 */
void topmenuReturnToHomeIfActive(TopmenuAppProcess *proc)
{
	if (g_topWindow->m_foregroundApp != NULL && g_topWindow->m_foregroundApp == proc) {
		TopTab *focusedTab = reinterpret_cast<TopTab *>(g_topWindow->FindFocusedTab());
		setFocusedTab(g_topWindow, focusedTab, true);
	}
}

/* 0x8127B8DC */
void topmenuSetGateEffectSurface(bool reverseEffect,
				 const paf::intrusive_ptr<paf::graph::Surface> *surface,
				 float originX, float originY)
{
	if (g_topWindow->m_gateEffectActive)
		return;

	g_topWindow->m_gateEffectReverse = reverseEffect;
	g_topWindow->m_gateEffectSurface = *surface;
	g_topWindow->m_gateEffectOriginX = originX;
	g_topWindow->m_gateEffectOriginY = originY;
}

/* 0x8127B934 */
void TopWindow::LayoutCustom(int layoutMode)
{
	float spacing = static_cast<float>(m_tabSpacing);
	paf::math::v4 cursorPos(0.0f, 0.0f, 0.0f, 0.0f);

	if (layoutMode == 3) {
		m_homeTabWidget->Hide(0xFF, NULL, NULL, 0.0f);
	} else if (layoutMode == 2) {
		if (m_tabs.size() == 1) {
			paf::math::v4 zeroPad(0.0f, 0.0f, 0.0f, 0.0f);
			m_scrollView->SetViewPadding(zeroPad);
			paf::math::v4 startPos(spacing * static_cast<float>(m_tabs.size()) + 480.0f,
					       0.0f, 0.0f, 0.0f);
			m_homeTabWidget->SetPos(startPos, NULL, 0x10000, 0, NULL, NULL);

			paf::Timer *timer = new paf::Timer();
			timer->InitTimer(300.0f, 23, 0, -1, NULL);
			m_homeTabWidget->SetPos(zeroPad, timer, 0x10000, 2, NULL, NULL);
			m_homeTabWidget->Hide(0xFF, NULL, NULL, 0.0f);
		} else {
			paf::math::v4 pad(0.0f, 0.0f, 60.0f, 0.0f);
			m_scrollView->SetViewPadding(pad);

			float footerX = (spacing * static_cast<float>(m_tabs.size() - 1) + 480.0f) - 60.0f;
			paf::math::v4 footerPos(footerX, 0.0f, 0.0f, 0.0f);
			paf::Timer *timer = new paf::Timer();
			timer->InitTimer(300.0f, 23, 0, -1, NULL);
			m_homeTabWidget->SetPos(footerPos, timer, 0x10000, 2, NULL, NULL);
			m_homeTabWidget->Show(0xFF, NULL, NULL, 0.0f);
		}
	} else if (m_tabs.size() == 1) {
		paf::math::v4 zeroPad(0.0f, 0.0f, 0.0f, 0.0f);
		m_scrollView->SetViewPadding(zeroPad);
		m_homeTabWidget->SetPos(zeroPad, NULL, 0x10000, 0, NULL, NULL);
		m_homeTabWidget->Hide(0xFF, NULL, NULL, 0.0f);
	} else {
		paf::math::v4 pad(0.0f, 0.0f, 60.0f, 0.0f);
		m_scrollView->SetViewPadding(pad);

		float footerX = (spacing * static_cast<float>(m_tabs.size() - 1) + 480.0f) - 60.0f;
		paf::math::v4 footerPos(footerX, 0.0f, 0.0f, 0.0f);
		m_homeTabWidget->SetPos(footerPos, NULL, 0x10000, 0, NULL, NULL);
		m_homeTabWidget->Show(0xFF, NULL, NULL, 0.0f);
	}

	int tabIdx = 0;
	for (paf::vector<TopTabBase *>::iterator it = m_tabs.begin();
	     it != m_tabs.end(); ++it, ++tabIdx) {
		paf::ui::Widget *tabWidget = (*it)->m_widget;
		tabWidget->SetParent(tabWidget->m_parent, NULL, 6);

		switch (layoutMode) {
		case 0: {
			tabWidget->KillAnim(0x10000);
			paf::Timer *timer = new paf::Timer();
			timer->InitTimer(300.0f, 35, 0, -1, NULL);
			tabWidget->SetPos(cursorPos, timer, 0x10000, 2, NULL, NULL);
			cursorPos = paf::math::v4(cursorPos.extract_x() + spacing, 0.0f, 0.0f, 0.0f);
			break;
		}
		case 1:
		case 4:
			tabWidget->KillAnim(0x10000);
			tabWidget->SetPos(cursorPos, NULL, 0x10000, 0, NULL, NULL);
			cursorPos = paf::math::v4(cursorPos.extract_x() + spacing, 0.0f, 0.0f, 0.0f);
			break;
		case 2: {
			paf::Timer *timer = new paf::Timer();
			timer->InitTimer(300.0f, 23, 0, -1, NULL);
			tabWidget->SetPos(cursorPos, timer, 0x10000, 2, NULL, NULL);
			cursorPos = paf::math::v4(cursorPos.extract_x() + spacing, 0.0f, 0.0f, 0.0f);
			break;
		}
		case 3: {
			tabWidget->KillAnim(0x10000);
			paf::Timer *timer = new paf::Timer();
			timer->InitTimer(300.0f, 35, 0, -1, NULL);
			tabWidget->SetPos(cursorPos, timer, 0x10000, 2, NULL, NULL);
			float step = (tabIdx != 0) ? 136.0f : 84.0f;
			cursorPos = paf::math::v4(cursorPos.extract_x() + step, 0.0f, 0.0f, 0.0f);
			break;
		}
		default:
			break;
		}
	}
}

/* 0x8127BFC4 */
static void onEditModeAnimTimer(SceInt32 eventType, paf::ui::Handler *self,
				paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)event;
	(void)userData;

	releaseInputBlocker(g_topWindow);
	self->KillTimer(TIMER_EDIT_MODE_ANIM_ID);
}

/* 0x8127BFE6 */
static void refreshAllTabTitles(TopWindow *window)
{
	for (size_t i = 0; i < window->m_tabs.size(); ++i) {
		TopTab *tab = reinterpret_cast<TopTab *>(window->m_tabs[i]);
		if (tab->m_appData != NULL) {
			const char *stitle = sceLsdbGetStitle(&tab->m_appData->m_process.m_appInfo);
			if (stitle != NULL) {
				paf::string stitleUtf8(stitle);
				paf::wstring stitleWide;
				paf::common::Utf8ToUtf16(stitleUtf8.c_str(), &stitleWide);
				tab->SetTitleText(stitleWide);
			}
		} else if (i == 0) {
			paf::wstring homeTitle(
				getTopWindowPlugin(window)->GetString(paf::IDParam(TOP_MSG_HOME_TITLE)));
			tab->SetTitleText(homeTitle);
		}
	}
}

/* 0x8127C184 */
static void reloadAllLanguagesAndLiveAreas(void)
{
	LiveAreaPluginInterface *laIf = topmenuGetLiveAreaInterface();
	refreshAllTabTitles(g_topWindow);
	topmenuWhatsNewForceRefresh();
	if (laIf != NULL && laIf->reloadAllPages != NULL)
		laIf->reloadAllPages();
}

/* 0x8127C1C0 */
static void onTopWindowScrollKey(SceInt32 eventType, paf::ui::Handler *self,
				 paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)self;
	(void)userData;

	if ((event->m_detail & 0x1000000u) == 0)
		return;
	if (g_topWindow->m_carouselState != 0)
		return;
	if ((g_topWindow->m_scrollView->GetMovableDirection(false) & 1u) == 0)
		return;

	TopTabBase *curTab = g_topWindow->FindFocusedTab();
	if (curTab == NULL)
		return;

	SceUInt32 keyCode = event->m_detail & 0xFFu;
	if (keyCode == 31) {
		TopTabBase *prevTab = g_topWindow->GetPrevTab(curTab);
		if (prevTab != NULL && prevTab != curTab)
			g_topWindow->ScrollToTab(prevTab, 500.0f);
	} else if (keyCode == 32) {
		TopTabBase *nextTab = g_topWindow->GetNextTab(curTab);
		if (nextTab != NULL && nextTab != curTab)
			g_topWindow->ScrollToTab(nextTab, 500.0f);
	}
}

/* 0x8127C278 */
static void onTopWindowFocusChange(SceInt32 eventType, paf::ui::Handler *self,
				   paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)self;
	(void)event;
	(void)userData;

	if (g_topWindow->m_carouselState == 0 || g_topWindow->m_carouselState == 3)
		return;

	paf::ui::Environment *env = paf::Framework::Instance()->GetEnvironmentInstance();
	if (ScePafWidget_529FB877(env, -1)) {
		topmenuSetHoveredTab(g_topWindow->m_draggedTab);
	} else if (g_topmenuHeaderFocusByPad) {
		g_topWindow->m_draggedTab = g_topWindow->m_hoveredTab;
		topmenuSetHoveredTab(NULL);
	}
}

/* 0x8127C32C */
static void onTopWindowSnapChange(SceInt32 eventType, paf::ui::Handler *self,
				  paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)self;
	(void)userData;

	SceInt32 newSnapId = static_cast<SceInt32>(event->m_data[0]);
	if (g_topWindow->m_activeSnapId == newSnapId)
		return;

	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabBySnapId(newSnapId));
	if (tab == NULL)
		return;

	SceInt32 prevSnapId = g_topWindow->m_activeSnapId;
	g_topWindow->m_activeSnapId = newSnapId;
	setFocusedTab(g_topWindow, tab, true);

	if (prevSnapId != -1 && g_topWindow->m_carouselState == 0) {
		if (!g_topWindow->m_openingLiveArea)
			paf::Framework::Instance()->PlayCommonSE(32, 0.0f, false);
		if (tab->m_index == 0)
			topmenuHelpShowHomeHelp();
	}

	if (tab->m_index != 0) {
		ApplauncherInterface *appIf = topmenuGetApplauncherInterface();
		if (appIf != NULL)
			appIf->func[19]();
	}

	LiveAreaPluginInterface *laIf = topmenuGetLiveAreaInterface();
	if (laIf != NULL && laIf->onActivePageChanged != NULL)
		laIf->onActivePageChanged(tab->m_pageKey);
}

/* 0x8127C40C */
static void onTopWindowScrollCancelDrag(SceInt32 eventType, paf::ui::Handler *self,
					paf::ui::Event *event, void *userData)
{
	(void)self;
	(void)userData;

	if (!g_topWindow->m_openingLiveArea)
		return;

	if (g_topWindow->m_scrollView->m_touch_captures.size() == 0) {
		if (eventType != 0x10000002)
			return;
		TopTabBase *snapTab =
			g_topWindow->FindTabBySnapId(static_cast<SceInt32>(event->m_data[0]));
		TopTabBase *openingTab =
			g_topWindow->FindTabByPageKey(g_topWindow->m_openingPageKey);
		if (snapTab == NULL || snapTab == openingTab)
			return;
	}

	ApplauncherInterface *appIf = topmenuGetApplauncherInterface();
	if (appIf != NULL)
		appIf->func[8]();
}

/* 0x8127C47C */
static void handleShellEventLiveAreaOp(TopWindow *window, const TopmenuAppMgrShellEvent *ev)
{
	(void)window;
	ApplauncherInterface *appIf = topmenuGetApplauncherInterface();
	if (appIf == NULL) {
		sceAppMgrReturnLiveAreaOperationResultForShell(0, -2146420460);
		return;
	}

	const char *titleId = reinterpret_cast<const char *>(&ev->appId);
	paf::string titleStr(titleId);
	typedef int (*HasAppFn)(const paf::string *);
	if (reinterpret_cast<HasAppFn>(appIf->func[21])(&titleStr) == 0) {
		sceAppMgrReturnLiveAreaOperationResultForShell(0, -2146420468);
		return;
	}

	switch (ev->processId) {
	case 1:
		sceAppMgrLaunchAppByNameForShell(0, titleId, NULL);
		break;
	case 2:
		sceAppMgrLaunchAppByNameForShell(0x20000, titleId, NULL);
		break;
	case 3: {
		TopTab *tab = topmenuFindTabByTitleId(g_topWindow, titleId);
		if (tab != NULL)
			topmenuCloseTab(tab, true);
		break;
	}
	case 4: {
		TopTab *tab = topmenuFindTabByTitleId(g_topWindow, titleId);
		if (tab != NULL)
			topmenuCloseTab(tab, true);
		sceAppMgrLaunchAppByNameForShell(0, titleId, NULL);
		break;
	}
	default:
		break;
	}

	sceAppMgrReturnLiveAreaOperationResultForShell(0, 0);
}

/* 0x8127C646 */
static void syncRunningProcessFocusFlags(TopWindow *window)
{
	SceInt32 appIds[20];
	int count = sceAppMgrGetRunningAppIdListForShell(appIds, 20);
	if (count < 0)
		return;

	for (paf::list<TopmenuProcessBase *>::iterator it = window->m_pendingLaunchQueue.begin();
	     it != window->m_pendingLaunchQueue.end(); ++it) {
		TopmenuProcessBase *proc = *it;
		bool found = false;
		for (int i = 0; i < count; ++i) {
			if (appIds[i] == proc->m_appId) {
				found = true;
				break;
			}
		}
		if (!found)
			topmenuProcessSetLiveAreaFocus(proc, false);
	}

	for (int i = 0; i < count; ++i) {
		TopmenuProcessBase *proc = findProcessInListByAppId(g_topWindow, appIds[i]);
		if (proc != NULL)
			topmenuProcessSetLiveAreaFocus(proc, true);
	}
}

/* 0x8127C71E */
static void handleShellEventDeactivateApp(TopWindow *window, const TopmenuAppMgrShellEvent *ev)
{
	TopmenuProcessBase *proc = findProcessInListByAppId(g_topWindow, ev->processId);
	if (proc != NULL && proc->m_owner != NULL && proc->m_owner == window->m_foregroundApp)
		topmenuSwitchToProcess(g_topWindow, NULL, true);
}

/* 0x8127C774 */
static void handleShellEventActivateApp(TopWindow *window, const TopmenuAppMgrShellEvent *ev)
{
	(void)window;
	TopmenuProcessBase *proc = findProcessInListByAppId(g_topWindow, ev->processId);
	if (proc != NULL && proc->m_owner != NULL)
		topmenuSwitchToProcess(g_topWindow, proc->m_owner, true);
}

/* 0x8127C7B6 */
void topmenuNotifyLaunchSeqReady(SceInt32 pageKey)
{
	topmenuCompleteLaunchIfReady(&g_topWindow->m_launchSeq, pageKey);
}

/* 0x8127C7CE */
void topmenuNotifyForegroundReady(TopmenuAppProcess *proc)
{
	if (g_topWindow->m_foregroundApp == proc)
		topmenuSwitchToProcess(g_topWindow, NULL, true);
}

/* 0x8127C7EC */
static void handleShellEventDebuggerReady(TopWindow *window, const TopmenuAppMgrShellEvent *ev)
{
	(void)window;
	SceInt32 debugAppId = ev->processId;
	TopTab *debugTab = reinterpret_cast<TopTab *>(
		g_topWindow->FindTabByPageKey(g_topWindow->m_pendingClosePageKey));

	if (debugTab != NULL && debugTab->m_appData != NULL) {
		TopTabAppData *appData = debugTab->m_appData;
		SceAppMgrAppStatus status;
		if (sceAppMgrGetStatusByAppId(debugAppId, &status) == 0) {
			TopTab *dupTab = topmenuFindTabByTitleId(g_topWindow, status.appName);
			if (dupTab != NULL && dupTab != debugTab)
				topmenuCloseTab(dupTab, false);
			appData->m_titleId = status.appName;
			appData->m_process.m_titleId = status.appName;
		}

		char rawPath[296];
		char debugPath[1024];
		if (sceAppMgrGetRawPathOfApp0ByAppIdForShell(debugAppId, rawPath) != 0)
			debugPath[0] = '\0';
		else
			sce_paf_snprintf(debugPath, sizeof(debugPath), "%s", rawPath);

		appData->m_process.StartAppCheck(debugPath);

		if (g_topWindow->m_foregroundApp == &appData->m_process)
			topmenuSwitchToProcess(g_topWindow, NULL, true);

		if (debugAppId == -1) {
			topmenuProcessResetAndCancel(&appData->m_process);
			topmenuCancelLiveAreaOpen(g_topWindow->m_pendingClosePageKey);
			if (g_topmenuDebuggerErrorDialogId >= 0) {
				SceCommonGuiDialog_BC311DC0(g_topmenuDebuggerErrorDialogId);
				g_topmenuDebuggerErrorDialogId = -1;
			}
			g_topmenuDebuggerErrorDialogId = topmenuShowAppErrorMessageDialog(
				-2146420470, NULL,
				new TopmenuDebuggerErrorHandler(g_topWindow->m_pendingClosePageKey),
				NULL, NULL, NULL, NULL, NULL);
		} else {
			topmenuProcessAttachAppId(&appData->m_process, debugAppId);
			topmenuCompleteLaunchIfReady(&g_topWindow->m_launchSeq,
						     g_topWindow->m_pendingClosePageKey);
		}
	}

	g_topWindow->m_pendingClosePageKey = -1;
}

/* 0x8127CA08 */
static void ifShowPageContent(int pageKey, float duration)
{
	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	if (tab != NULL && tab->m_appData != NULL)
		tab->m_appData->ShowAnimated(duration);
}

/* 0x8127CA3A */
static void handleShellEventDebuggerLaunch(TopWindow *window, const TopmenuAppMgrShellEvent *ev)
{
	if (g_topWindow->m_powerManageIf != NULL) {
		g_topWindow->m_powerManageIf->powerTickAndSleep();
		g_topWindow->m_powerManageIf->unlockAndLaunchApp(1, NULL, NULL);
	}

	TopTab *oldDebugTab = topmenuFindTabByTitleId(window, "NPXS10999");
	if (oldDebugTab != NULL)
		topmenuCloseTab(oldDebugTab, true);

	ApplauncherInterface *appIf = topmenuGetApplauncherInterface();
	typedef void (*LoadIconFn)(const char *,
				   paf::intrusive_ptr<paf::cache::CacheImageInterface> *);
	paf::intrusive_ptr<paf::cache::CacheImageInterface> iconCache;
	reinterpret_cast<LoadIconFn>(appIf->func[11])(
		"vs0:data/internal/livearea/default/sce_sys/icon0.png", &iconCache);

	SceInt32 launchMode = 1;
	if (ev->appId == 1)
		launchMode = 2;
	else if (ev->appId == 2)
		launchMode = 3;

	SceInt32 homePageKey = g_topWindow->m_tabs[0]->m_pageKey;
	paf::string debugTitle("NPXS10999");
	LsdbAppInfoPtr emptyInfo;
	g_topWindow->m_pendingClosePageKey =
		createAppTab(homePageKey, debugTitle, emptyInfo, &iconCache,
			     true, launchMode, true, true, false, -1);

	if (g_topWindow->m_pendingClosePageKey != -1) {
		TopTab *newTab = reinterpret_cast<TopTab *>(
			g_topWindow->FindTabByPageKey(g_topWindow->m_pendingClosePageKey));
		if (newTab != NULL && newTab->m_appData != NULL) {
			topmenuProcessSetState(&newTab->m_appData->m_process, 2);
			newTab->m_appData->ShowAnimated(0.0f);
		}
	}
}

/* 0x8127CC66 */
static void handleShellEventVoiceAppChanged(TopWindow *window, const TopmenuAppMgrShellEvent *ev)
{
	TopmenuProcessBase *proc = findProcessInListByAppId(g_topWindow, ev->appId);
	if (proc == NULL || proc->m_owner == NULL || window->m_imposeIf == NULL)
		return;

	const char *title = sceLsdbGetTitle(&proc->m_owner->m_appInfo);
	if (title == NULL)
		return;

	ImposeNotice notice;
	notice.name = "VOICE_APP_CHANGED";
	notice.iconHash = TOP_NOTICE_VOICE_ICON_ID;

	paf::string titleUtf8(title);
	paf::wstring titleWide;
	paf::common::Utf8ToUtf16(titleUtf8.c_str(), &titleWide);

	const wchar_t *fmt = g_topmenuPlugin->GetString(paf::IDParam(TOP_MSG_VOICE_APP_CHANGED));
	paf::wstring formatted = shellFormatText(fmt, titleWide.c_str());
	notice.message = formatted.c_str();

	window->m_imposeIf->showNotice(&notice);
}

/* 0x8127CE76 */
static void showInvitationLaunchDialog(const char *titleId, const LsdbAppInfoPtr *appInfo,
				       SceInt32 launchAppId)
{
	CommonGuiLayoutDialogParam dlgParam;
	dlgParam.priority = 33;
	dlgParam.layout = SceCommonGuiDialog_92752DE2;
	dlgParam.resourcePlugin = g_topmenuPlugin;
	dlgParam.text = g_topmenuPlugin->GetString(paf::IDParam(TOP_MSG_INVITATION_LAUNCH));
	dlgParam.handler = new TopmenuInvitationLaunchHandler(titleId, appInfo, launchAppId);
	dlgParam.soundId = TOP_SOUND_DIALOG_OPEN;
	dlgParam.notifyClose = true;

	SceInt32 dlgId = SceCommonGuiDialog_CCFCD9A6(&dlgParam);
	if (dlgId < 0) {
		if (launchAppId != -1)
			topmenuCancelPendingAppLaunch(launchAppId);
		return;
	}

	topmenuDialogRegister(dlgId);
	topmenuDialogOnPowerHold(dlgId);

	paf::ui::Widget *bodyBox = SceCommonGuiDialog_8947FC23(dlgId, 10);
	if (bodyBox == NULL || !bodyBox->IsInherit(paf::ui::Box::TypeName()))
		return;

	paf::ui::Widget *msgWidget = SceCommonGuiDialog_8947FC23(dlgId, 1);
	if (msgWidget != NULL && msgWidget->IsInherit(paf::ui::Text::TypeName())) {
		paf::ui::Text *msgText = reinterpret_cast<paf::ui::Text *>(msgWidget);
		msgText->SetAdjust(0, msgText->m_adjust_y, msgText->m_adjust_z);
		msgText->SetLayoutAttribute(paf::graph::TextLayoutAttribute_TextAlign, (int32_t)0);
		msgText->SetLayoutAttribute(paf::graph::TextLayoutAttribute_AlignX, (int32_t)0);
	}

	g_topmenuPlugin->TemplateOpen(bodyBox, paf::IDParam(TOP_WINDOW_DIALOG_INVITE_TPL_ID),
				      paf::Plugin::TemplateOpenParam());
	paf::ui::Widget *appCard =
		bodyBox->FindChild(paf::IDParam(TOP_WINDOW_DIALOG_BOX_ID), 0);
	if (appCard == NULL)
		return;

	paf::ui::Widget *iconWidget =
		appCard->FindChild(paf::IDParam(TOP_WINDOW_DIALOG_ICON_ID), 0);
	if (iconWidget != NULL) {
		paf::intrusive_ptr<paf::cache::CacheImageInterface> iconCache;
		paf::string titleStr(titleId);
		typedef void (*GetIconFn)(paf::intrusive_ptr<paf::cache::CacheImageInterface> *,
					  const paf::string &,
					  void (*)(paf::intrusive_ptr<paf::cache::CacheImageInterface> *,
						   SceInt32),
					  SceInt32);
		ApplauncherInterface *appIf = topmenuGetApplauncherInterface();
		reinterpret_cast<GetIconFn>(appIf->func[3])(&iconCache, titleStr,
							    onInviteDialogIconReady, dlgId);
		if (iconCache.get() != NULL) {
			iconWidget->SetDrawObjMaskBit(&iconCache, 0);
		} else {
			paf::intrusive_ptr<paf::graph::Surface> fallbackTex =
				g_topmenuPlugin->GetTexture(
					paf::IDParam(TOP_WINDOW_DIALOG_FALLBACK_TEX_ID));
			iconWidget->SetTexture(fallbackTex);
		}
	}

	paf::ui::Widget *titleWidget =
		appCard->FindChild(paf::IDParam(TOP_WINDOW_DIALOG_TITLE_ID), 0);
	if (titleWidget != NULL) {
		const char *title = sceLsdbGetTitle(appInfo);
		if (title != NULL) {
			paf::string titleUtf8(title);
			paf::wstring titleWide;
			paf::common::Utf8ToUtf16(titleUtf8.c_str(), &titleWide);
			titleWidget->SetString(titleWide);
		}
	}
}

/* 0x8127D402 */
static void handleShellEventAppLaunchRequest(TopWindow *window,
					     const TopmenuAppMgrShellEvent *ev)
{
	SceInt32 launchAppId = ev->processId;
	if (window->m_pendingKillAppId == launchAppId) {
		unregisterSystemAppId(g_topWindow, launchAppId);
		window->m_pendingKillAppId = -1;
		return;
	}

	SceAppMgrAppStatus status;
	if (sceAppMgrGetStatusByAppId(launchAppId, &status) != 0) {
		unregisterSystemAppId(g_topWindow, launchAppId);
		return;
	}

	if ((status.launchMode & 0x100000u) == 0) {
		topmenuOnUriSearchDone(0, status.appName, launchAppId);
		return;
	}

	char uriBuf[1024];
	if (sceAppMgrPeekLaunchParamForShell(launchAppId, uriBuf, sizeof(uriBuf)) != 0) {
		topmenuOnUriSearchDone(0, NULL, launchAppId);
		return;
	}

	if (topmenuParsePsgmUriScheme(uriBuf) != 0) {
		char parsedTitleId[136];
		char altTitleIds[1024];
		char npCommId[16];

		if (topmenuParsePsgmTitleId(uriBuf, parsedTitleId, 129) != 0) {
			TopmenuAppProcess *proc =
				topmenuFindProcessByTitleId(g_topWindow, parsedTitleId);
			if (proc != NULL) {
				char typeBuf[128];
				if (topmenuParsePsgmType(uriBuf, typeBuf, sizeof(typeBuf)) != 0 &&
				    sce_paf_strcmp("INVITATION_MESSAGE", typeBuf) == 0 &&
				    SceLsdb_E26FC9E8(&proc->m_appInfo) == NULL) {
					showInvitationLaunchDialog(parsedTitleId, &proc->m_appInfo,
								   launchAppId);
				} else {
					LsdbAppInfoPtr infoCopy(proc->m_appInfo);
					topmenuHandleAppLaunchByStatus(parsedTitleId, &infoCopy,
								       &status, launchAppId);
				}
			} else if (topmenuParsePsgmAltTitleIds(uriBuf, altTitleIds,
							       sizeof(altTitleIds)) != 0) {
				paf::common::SharedPtr<paf::job::JobItem> searchJob;
				topmenuStartAppInfoSearch(&searchJob, parsedTitleId,
							  altTitleIds, launchAppId);
			} else {
				paf::common::SharedPtr<paf::job::JobItem> infoJob;
				paf::string titleStr(parsedTitleId);
				lsdbEnqueueGetAppInfoJob(
					&infoJob, g_topWindow->m_jobQueue, titleStr,
					onLsdbAppInfoForPsgm,
					reinterpret_cast<void *>(static_cast<intptr_t>(launchAppId)));
			}
		} else if (topmenuParsePsgmNpCommId(uriBuf, npCommId, 14) != 0) {
			paf::common::SharedPtr<paf::job::JobItem> npJob;
			topmenuStartNpCommIdSearch(&npJob, npCommId, launchAppId);
		} else {
			showAppNotFoundOrStoreDialog(-2146420473, launchAppId);
			if (launchAppId != -1)
				topmenuCancelPendingAppLaunch(launchAppId);
		}
		return;
	}

	if (paf::system::Model::GetType() == 1 &&
	    sce_paf_strncmp("near:", uriBuf, sce_paf_strlen("near:")) == 0) {
		CommonGuiLayoutDialogParam dlgParam;
		dlgParam.priority = 33;
		dlgParam.resourcePlugin = g_topmenuPlugin;
		dlgParam.text = getTopWindowPlugin(window)->GetString(
			paf::IDParam(TOP_MSG_BLOCKED_APP));
		dlgParam.handler = new TopmenuOwnedOkDialogHandler();
		dlgParam.soundId = TOP_SOUND_DIALOG_OPEN;
		dlgParam.notifyClose = true;

		SceInt32 dlgId = SceCommonGuiDialog_CCFCD9A6(&dlgParam);
		if (dlgId >= 0) {
			topmenuDialogRegister(dlgId);
			topmenuDialogOnPowerHold(dlgId);
		}
		if (launchAppId != -1)
			topmenuCancelPendingAppLaunch(launchAppId);
		return;
	}

	paf::common::SharedPtr<paf::job::JobItem> uriJob;
	topmenuStartUriSearchOrFastPath(&uriJob, uriBuf, launchAppId);
}

/* 0x8127D974 */
static void handleShellEventCloseOtherTabs(TopWindow *window, const TopmenuAppMgrShellEvent *ev)
{
	SceInt32 keepAppId = ev->appId;
	paf::vector<TopTabBase *>::iterator it = window->m_tabs.begin();
	while (it != window->m_tabs.end()) {
		TopTab *tab = reinterpret_cast<TopTab *>(*it);
		if (tab->m_appData == NULL) {
			++it;
			continue;
		}
		if (window->m_switchToHomeOnClose &&
		    sce_paf_strcmp(SceActivityDb_E6C23731(),
				   tab->m_appData->m_titleId.c_str()) == 0) {
			++it;
			continue;
		}
		if (tab->m_appData->m_process.m_appId == keepAppId) {
			++it;
			continue;
		}

		TopTabBase **nextPtr = NULL;
		topmenuCloseTabWithNext(tab, false, &nextPtr);
		it = window->m_tabs.begin();
		if (nextPtr != NULL) {
			while (it != window->m_tabs.end() && &(*it) != nextPtr)
				++it;
		}
	}
}

/* 0x8127DA06 */
static void clearBgPartnerLiveAreaOnExit(TopWindow *window, SceInt32 exitedAppId)
{
	const char *exitedAppName = NULL;
	for (paf::list<TopmenuAppFlagEntry>::iterator it = window->m_appFlagList.begin();
	     it != window->m_appFlagList.end(); ++it) {
		if (it->appId == exitedAppId) {
			exitedAppName = it->appName;
			break;
		}
	}
	if (exitedAppName == NULL)
		return;

	for (paf::vector<TopTabBase *>::iterator it = window->m_tabs.begin();
	     it != window->m_tabs.end(); ++it) {
		TopTab *tab = reinterpret_cast<TopTab *>(*it);
		if (tab->m_appData == NULL ||
		    topmenuAppProcessIsRunning(&tab->m_appData->m_process)) {
			continue;
		}

		const char *bgTitleId = topmenuAppProcessGetTitleId(&tab->m_appData->m_process);
		if (bgTitleId == NULL)
			continue;

		SceUInt64 flags = topmenuGetAppFlags(window, bgTitleId);
		if ((flags & 0x4000000ULL) == 0 &&
		    sce_paf_strncmp(bgTitleId, exitedAppName, 0x20u) == 0) {
			LiveAreaPluginInterface *laIf = topmenuGetLiveAreaInterface();
			if (laIf != NULL) {
				if (laIf->resetPageFrames != NULL)
					laIf->resetPageFrames(tab->m_pageKey);
				if (laIf->clearGateSurface != NULL)
					laIf->clearGateSurface(tab->m_pageKey);
			}
			return;
		}
	}
}

/* 0x8127DB18 */
static SceInt32 resolveCallerReturnPageKey(TopWindow *window, SceInt32 exitedAppId,
					   SceInt32 reason, SceInt32 callerAppId,
					   SceInt32 returnFlag)
{
	(void)reason;
	if (callerAppId == -1)
		return -1;

	TopmenuProcessBase *proc = findProcessInListByAppId(g_topWindow, exitedAppId);
	if (proc == NULL)
		return -1;

	SceAppMgrAppStatus status;
	if (sceAppMgrGetStatusByAppId(exitedAppId, &status) != 0)
		return -1;

	if ((status.launchMode & 0x40000u) != 0) {
		SceUID selfPid = sceKernelGetProcessId();
		if (selfPid != sceAppMgrGetProcessIdByAppIdForShell(callerAppId)) {
			TopTab *callerTab = findRunningTabByAppId(window, callerAppId);
			if (callerTab != NULL && callerTab->m_appData != NULL && returnFlag != 0)
				return callerTab->m_pageKey;
		}
	}
	return -1;
}

/* 0x8127DBD2 */
static void handleShellEventAppExited(TopWindow *window, const TopmenuAppMgrShellEvent *ev,
				      bool isFinalExit)
{
	(void)isFinalExit;
	if (ev->isSubProcess != 0)
		return;

	SceInt32 exitedAppId = ev->targetAppId;
	SceInt32 callerAppId = ev->callerAppId;
	SceInt32 returnFlag = ev->extraFlag;
	SceInt32 reason = ev->appId;

	findRunningTabByAppId(window, exitedAppId);
	TopmenuProcessBase *proc = findProcessInListByAppId(g_topWindow, exitedAppId);
	if (proc != NULL)
		proc->m_suspended = true;

	if (window->m_bgLaunchingApp != NULL && window->m_bgLaunchingApp->m_appId == exitedAppId) {
		window->m_bgLaunchingApp->DestroyOrDefer();
		window->m_bgLaunchingApp = NULL;
		return;
	}

	SceInt32 returnPageKey =
		resolveCallerReturnPageKey(window, exitedAppId, reason, callerAppId, returnFlag);
	if (proc == NULL)
		return;

	TopTabAppData *appData = topmenuProcessGetTabAppData(proc);
	if (appData != NULL) {
		bool animateClose = true;
		SceInt32 mode = appData->m_process.m_launchMode;
		bool shouldCloseTab = (mode == 1 || mode == 2 || mode == 3);
		if (reason == 2) {
			animateClose = (returnFlag != 0);
			shouldCloseTab = true;
		}

		if (window->m_foregroundApp != NULL && window->m_foregroundApp == proc) {
			if (appData->m_appViewState == 1) {
				appData->OnLeaveViewport();
				window->m_foregroundApp->SendProcessCommand(4);
			} else if (appData->m_appViewState == 2) {
				topmenuLaunchPendingCrashReport();
			} else {
				window->m_foregroundApp->SendProcessCommand(2);
			}

			topmenuProcessResetAndCancel(proc);

			SceInt32 closeKey = shouldCloseTab ? appData->m_ownerTab->m_pageKey : -1;
			TopmenuLaunchSeqParam seqParam;
			seqParam.targetPageKey = returnPageKey;
			seqParam.autoStart = (returnPageKey != -1);
			seqParam.holdBusy = true;
			seqParam.skipScroll = false;
			seqParam.closePageKey = closeKey;
			seqParam.immediateStart = false;
			seqParam.isWebApp = false;
			seqParam.openPatchChk = false;
			seqParam.notifyApplauncher = false;
			topmenuStartLaunchSequence(&g_topWindow->m_launchSeq, &seqParam);
		} else {
			topmenuProcessResetAndCancel(proc);
			if (shouldCloseTab)
				topmenuCloseTab(appData->m_ownerTab, animateClose);
		}
		return;
	}

	if (returnPageKey != -1) {
		TopmenuLaunchSeqParam seqParam;
		seqParam.targetPageKey = returnPageKey;
		seqParam.autoStart = true;
		seqParam.holdBusy = true;
		seqParam.skipScroll = false;
		seqParam.closePageKey = -1;
		seqParam.immediateStart = true;
		seqParam.isWebApp = false;
		seqParam.openPatchChk = false;
		seqParam.notifyApplauncher = false;
		topmenuStartLaunchSequence(&g_topWindow->m_launchSeq, &seqParam);
		return;
	}

	SceAppMgrAppStatus callerStatus;
	if (returnFlag == 0 || sceAppMgrGetStatusByAppId(callerAppId, &callerStatus) != 0) {
		for (paf::vector<TopTabBase *>::iterator it = window->m_tabs.begin();
		     it != window->m_tabs.end(); ++it) {
			TopTab *tab = reinterpret_cast<TopTab *>(*it);
			if (tab->m_appData != NULL && tab->m_appData->m_appViewState == 1) {
				tab->m_appData->OnLeaveViewport();
				tab->m_appData->m_process.SendProcessCommand(4);
			}
		}
		TopTab *focusedTab = reinterpret_cast<TopTab *>(g_topWindow->FindFocusedTab());
		setFocusedTab(window, focusedTab, false);
		return;
	}

	if (window->m_bgLaunchingApp != NULL && window->m_bgLaunchingApp->m_appId == callerAppId) {
		topmenuSwitchToProcess(window, NULL, true);
		TopmenuAppProcess *resumedSys = window->m_bgLaunchingApp;
		window->m_closingAppPending = true;
		window->m_bgLaunchingApp = NULL;
		window->m_closingApp = resumedSys;
		topmenuSwitchToProcess(window, resumedSys, true);
	}
}

/* 0x8127DEF2 */
static void updateTopWindowFrame(TopWindow *window)
{
	if (window->m_deactivateDelayFrames > 0) {
		window->m_deactivateDelayFrames--;
		if (window->m_deactivateDelayFrames == 0) {
			TopmenuAppProcess *fg = window->m_foregroundApp;
			if (fg == NULL || fg->m_launchState != 3 ||
			    SceLsdb_849E8DC6(&fg->m_appInfo) != 0) {
				sceAppMgrSetBackRenderPortOwner(-1);
			}
		}
	}

	if (window->m_foregroundApp != NULL) {
		if (window->m_foregroundApp->m_procState == 3) {
			SceAppMgrAppStatus status;
			if (sceAppMgrGetStatusByAppId(window->m_foregroundApp->m_appId,
						      &status) == 0) {
				TopmenuAppAttrFlags attrFlags;
				attrFlags.hasFlagBitSeven = (status.infobar.visibility != 0);
				attrFlags.flagBitEight = status.infobar.color;
				attrFlags.flagBitNine = status.infobar.transparency;
				window->m_foregroundApp->SetAttrFlags(&attrFlags);
				refreshDefaultIndicatorBg(window);
			}
		}

		if (window->m_foregroundApp != NULL) {
			topmenuProcessPollSharedFb(window->m_foregroundApp);
			if (window->m_foregroundApp->m_hasAppId) {
				ScePafMisc_F8D63B8E(0.8f);
				topmenuBootScreenHide();
			}
			if (paf::graph::GateBlur::GetMode() == paf::graph::GateBlur::Mode_Hold) {
				SceUID fgPid = topmenuProcessGetPid(g_topWindow->m_foregroundApp);
				if (sceSharedFbGetRenderingInfo(fgPid) == 0) {
					ScePafGraphics_04F03872();
					window->m_gateEffectScene->Hide(0xFF, NULL, NULL, 0.0f);
				}
			}
		}
	}

	if (!shellBootHasAppStack()) {
		if (window->m_foregroundApp != NULL && window->m_foregroundApp->m_hasAppId)
			window->m_imposeIf->openOverlay(window->m_foregroundApp->m_parentAppId);
		else
			window->m_imposeIf->closeOverlay();
	}

	int shellEventCount = sceAppMgrReceiveShellEventNum();
	for (int i = 0; i < shellEventCount; ++i) {
		TopmenuAppMgrShellEvent ev;
		sceAppMgrReceiveShellEvent(&ev, sizeof(ev));

		switch (ev.eventType) {
		case 2: {
			SceAppMgrAppStatus status;
			sceAppMgrGetStatusByAppId(ev.appId, &status);

			ShellAppActivatedEvent actEv;
			actEv.processId = ev.processId;
			actEv.appId = ev.appId;
			actEv.appName = status.appName;
			actEv.fromBoot = (ev.detailCode != 0);
			shellAppEventSend(SHELL_APP_EVENT_STARTED, &actEv, sizeof(actEv));
#if !defined(FW_TYPE_CEX)
			ScePafAutoTestTty_9E10E873(3, "loaded_%s", actEv.appName);
#endif

			bool alreadyTracked = false;
			for (paf::list<TopmenuAppFlagEntry>::iterator it =
				     window->m_appFlagList.begin();
			     it != window->m_appFlagList.end(); ++it) {
				if (it->appId == ev.appId) {
					alreadyTracked = true;
					break;
				}
			}
			if (!alreadyTracked) {
				TopmenuAppFlagEntry entry;
				entry.flags = 0;
				entry.appId = ev.appId;
				sce_paf_strncpy(entry.appName, status.appName, 0x20u);
				entry.registered = false;
				window->m_appFlagList.push_back(entry);
			}
			break;
		}
		case 3: {
			SceAppMgrAppStatus status;
			sceAppMgrGetStatusByAppId(ev.targetAppId, &status);
			handleShellEventAppExited(window, &ev, false);
			break;
		}
		case 4: {
			SceAppMgrAppStatus status;
			sceAppMgrGetStatusByAppId(ev.targetAppId, &status);
			handleShellEventAppExited(window, &ev, true);

			ShellAppActivatedEvent endEv;
			endEv.processId = ev.processId;
			endEv.appId = ev.targetAppId;
			endEv.appName = ev.titleId;
			endEv.fromBoot = (ev.isSubProcess != 0);
			shellAppEventSend(SHELL_APP_EVENT_ENDED, &endEv, sizeof(endEv));
#if !defined(FW_TYPE_CEX)
			ScePafAutoTestTty_9E10E873(3, "unloaded_%s", endEv.appName);
#endif

			clearBgPartnerLiveAreaOnExit(window, ev.targetAppId);
			topmenuSetPromoteWaitLock(false, ev.titleId);
			removeLaunchedAppFlags(window, ev.targetAppId);
			break;
		}
		case 6:
			handleShellEventAppLaunchRequest(window, &ev);
			break;
		case 8: {
			SceAppMgrAppStatus status;
			sceAppMgrGetStatusByAppId(ev.appId, &status);
			handleShellEventCloseOtherTabs(window, &ev);
			break;
		}
		case 9:
			handleShellEventDebuggerLaunch(window, &ev);
			break;
		case 10:
			handleShellEventDebuggerReady(window, &ev);
			break;
		case 11: {
			TopmenuCrashEvent crashEv;
			crashEv.processId = ev.processId;
			crashEv.reservedWord = ev.appId;
			crashEv.appId = ev.detailCode;
			crashEv.hasCoredump = ev.extraFlag;
			topmenuHandleAppCrash(&crashEv);
			break;
		}
		case 12:
			handleShellEventVoiceAppChanged(window, &ev);
			break;
		case 13: {
			TopTab *tab = findRunningTabByAppId(window, ev.appId);
			if (tab != NULL) {
				if (ev.detailCode == 0) {
					tab->OnEnterViewport(false);
					tab->RefreshOrderStamp();
				} else if (ev.detailCode == 1) {
					tab->OnLeaveViewport(false);
					tab->RefreshOrderStamp();
				}
			}
			break;
		}
		case 14:
			topmenuShowPowerHoldErrorDialog(ev.appId);
			break;
		case 15:
			handleShellEventActivateApp(window, &ev);
			break;
		case 16:
			handleShellEventDeactivateApp(window, &ev);
			break;
		case 17:
			topmenuMotionUpdateFrontApp();
			break;
		case 18:
			syncRunningProcessFocusFlags(window);
			topmenuMotionUpdateFrontApp();
			break;
		case 19:
			handleShellEventLiveAreaOp(window, &ev);
			break;
		default:
			break;
		}
	}

	SceInt32 appEventCount = 0;
	sceAppMgrReceiveEventNum(&appEventCount);
	for (int i = 0; i < appEventCount; ++i) {
		SceAppMgrAppEvent appEvent;
		sceAppMgrReceiveEvent(&appEvent);
	}

	for (paf::vector<TopTabBase *>::iterator it = window->m_tabs.begin();
	     it != window->m_tabs.end(); ++it) {
		TopTab *tab = reinterpret_cast<TopTab *>(*it);
		if (tab != NULL && tab->m_appData != NULL)
			tab->m_appData->Update();
	}

	if (window->m_carouselState == 2 || window->m_carouselState == 1) {
		updateCarouselLayoutAndTilt(window);
	} else if (window->m_carouselState == 0) {
		int idx = 0;
		for (paf::vector<TopTabBase *>::iterator it = window->m_tabs.begin();
		     it != window->m_tabs.end(); ++it, ++idx) {
			if (idx == 0)
				continue;
			TopTab *tab = reinterpret_cast<TopTab *>(*it);
			paf::ui::Widget *tabWidget = tab->m_widget;
			if ((tabWidget->m_update_flag & 7) != 0)
				tabWidget->UpdateMatrix();

			paf::math::v4 worldPos = tabWidget->m_world_matrix.w();
			paf::math::v4 contentPos;
			paf::math::v4 contentRot;
			computeTabScrollTransform(window, &contentPos, &contentRot, &worldPos, idx);
			tab->m_contentRoot->SetPos(contentPos, NULL, 0x10000, 0, NULL, NULL);
			tab->m_contentRoot->SetRot(contentRot, 0, NULL, 0x10006, 0, 0, NULL, NULL);
		}
	}

	if (window->m_focusedNavWidget != NULL) {
		paf::ui::Widget *viewParent = g_topWindow->m_scrollView->GetViewParent();
		paf::math::v4 scrollViewPos =
			viewParent->GetPos(0, 0, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		paf::ui::Widget *wallpaperWidget =
			reinterpret_cast<paf::ui::Widget *>(window->m_navFocusMode);
		if (wallpaperWidget != NULL) {
			paf::math::v4 wpSize = *wallpaperWidget->GetSize(0);
			paf::math::v4 wpPos =
				wallpaperWidget->GetPos(0, 0, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));

			paf::ui::ScrollView *homeScroll =
				reinterpret_cast<paf::ui::ScrollView *>(window->m_focusedNavWidget);
			paf::ui::Widget *homeViewParent = homeScroll->GetViewParent();
			paf::math::v4 homeSize = *homeScroll->GetSize(0);
			paf::math::v4 homePos =
				homeViewParent->GetPos(0, 0, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));

			float availH = (wpSize.extract_y() - 512.0f) - 16.0f;
			float posY = (((wpSize.extract_y() * -0.5f) + 16.0f) +
				      (homeSize.extract_y() * 0.5f)) +
				     (availH * (homePos.extract_y() / 2560.0f));
			float mainW = g_topWindow->m_scrollView->GetSize(0)->extract_x();
			float posX = ((wpSize.extract_x() * 0.5f) - (mainW * 0.5f)) +
				     (((wpSize.extract_x() - 960.0f) * scrollViewPos.extract_x()) /
				      2880.0f);

			paf::math::v4 newPos(posX, posY, wpPos.extract_z(), wpPos.extract_w());
			wallpaperWidget->SetPos(newPos, NULL, 0x10000, 0, NULL, NULL);
		}
	}

	paf::ui::Widget *mainViewParent = g_topWindow->m_scrollView->GetViewParent();
	paf::math::v4 mainScrollPos =
		mainViewParent->GetPos(0, 0, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	float pageCoord = mainScrollPos.extract_x() / 960.0f;

	if (pageCoord != sce_paf_floorf(pageCoord) || window->m_editModeActive ||
	    window->m_carouselState != 0) {
		if (window->m_navArrowsEnabled) {
			window->m_navArrowsEnabled = false;
			updatePageArrows(window);
		}
		if (!window->m_touchActive) {
			window->m_touchActive = true;
			syncTouchDefaultRegions(window);
		}
	} else {
		if (!window->m_navArrowsEnabled) {
			window->m_navArrowsEnabled = true;
			updatePageArrows(window);
		}
		if (window->m_touchActive) {
			window->m_touchActive = false;
			syncTouchDefaultRegions(window);
		}
		TopTab *focusedTab = reinterpret_cast<TopTab *>(window->FindFocusedTab());
		topmenuOnScrollSettled(&window->m_launchSeq, focusedTab);
	}

	for (size_t i = 0; i < window->m_tabs.size(); ++i) {
		TopTab *tab = reinterpret_cast<TopTab *>(window->m_tabs[i]);
		if (tab == NULL)
			continue;

		paf::math::v4 cullBounds = (i == 0) ? paf::math::v4(958.0f, 512.0f, 0.0f, 0.0f)
						    : paf::math::v4(840.0f, 512.0f, 0.0f, 0.0f);
		bool visible = tab->IsVisibleInViewport(&cullBounds, tab->m_contentRoot);
		if (visible) {
			if (tab->m_widget->m_show_alpha < 1.0f)
				tab->m_widget->Show(0xFF, NULL, NULL, 0.0f);
		} else if (tab->m_widget->m_show_alpha > 0.0f) {
			tab->m_widget->Hide(0xFF, NULL, NULL, 0.0f);
		}
	}

	SceInt32 prevPsnState = window->m_psnSignInState;
	int signedIn = 0;
	npCommonServiceIsSignedIn(&signedIn);
	SceInt32 newPsnState = 0;
	if (signedIn != 0)
		newPsnState = (shellActivityIsEnabled() == 1) ? 1 : 2;
	window->m_psnSignInState = newPsnState;

	if (prevPsnState != newPsnState && window->m_whatsNewNotifyEnabled && newPsnState == 1)
		topmenuWhatsNewOnAgeChanged();

	if (window->m_powerSuspendMode == 0) {
		TopmenuMusicStatus bgmStatus;
		if (musicCoreBgmGetStatus(reinterpret_cast<MusicCorePublicStatus *>(&bgmStatus)) == 0 &&
		    bgmStatus.playMode == 0) {
			if (bgmStatus.posLo == 6 && bgmStatus.posHi == 144 &&
			    bgmStatus.sampleRate == 48000 && bgmStatus.bitrate == 2 &&
			    bgmStatus.channels == -1) {
				window->m_powerSuspendMode = 1;
			} else {
				musicCoreBgmControl(2, 0);
				window->m_powerSuspendMode = 2;
			}
		}
	}
}

/* Mirrors paf::Framework layout from vsh/paf/include/framework.h for inline m_shutdown_requested read */
struct TopmenuFrameworkLayout {
	uint32_t runLoopCount;
	paf::vector<paf::Plugin *> pluginList;
	paf::Plugin *activePlugin;
	int32_t loadingPluginCount;
	paf::Plugin *commonResourcePlugin;
	paf::Framework::LoadCRFinishCallback loadFinishCallback;
	paf::Framework::UnloadCRFinishCallback unloadFinishCallback;
	paf::Framework::ShutdownCallback shutdownCallback;
	void *shutdownCallbackData;
	bool jobReady;
	paf::memory::HeapAllocator *decodeHeap;
	bool decodeHeapCreated;
	paf::memory::HeapAllocator *pluginHeap;
	bool pluginHeapCreated;
	void *surfacePoolAddr;
	paf::graph::SurfacePool *surfacePool;
	paf::ui::Environment *env;
	paf::ui::Screen *scr;
	int32_t mode;
	uint32_t option;
	int32_t locale;
	paf::thread::Mutex *updateMutex;
	paf::thread::Cond *updateCond;
	paf::thread::RWLock *listLock;
	paf::sound::Output *soundOutput;
	bool shutdownRequested;
};

/* 0x8127E7BA */
static void onTopWindowMainTick(void *userData)
{
	(void)userData;
	updateTopWindowFrame(g_topWindow);
	topmenuWhatsNewCheckRefresh();

	const TopmenuFrameworkLayout *fw =
		reinterpret_cast<const TopmenuFrameworkLayout *>(paf::Framework::Instance());
	if (fw->shutdownRequested) {
		while (g_topWindow->m_tabs.size() > 1) {
			TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->m_tabs.back());
			if (tab != NULL)
				topmenuCloseTab(tab, false);
		}
	}
}

/* 0x8127E820 */
static void onLsdbAppInfoForUri(void *jobPtr, void *userData)
{
	LsdbGetAppInfoJobResult *job = reinterpret_cast<LsdbGetAppInfoJobResult *>(jobPtr);
	SceInt32 launchAppId = static_cast<SceInt32>(reinterpret_cast<intptr_t>(userData));

	if (sceLsdbGetMetaContentsPath(&job->m_appInfo) != 0 ||
	    (SceLsdb_45B38A03(&job->m_appInfo) & 1u) != 0 ||
	    SceLsdb_41019D89(&job->m_appInfo) != 0) {
		topmenuDispatchAppLaunch(&job->m_titleId, &job->m_appInfo, launchAppId,
					 job->m_result);
		return;
	}

	SceLsdb_C070E42E(&job->m_appInfo);
	ApplauncherInterface *appIf = topmenuGetApplauncherInterface();
	typedef int (*HasAppFn)(const paf::string *);
	if (reinterpret_cast<HasAppFn>(appIf->func[21])(&job->m_titleId) != 0) {
		topmenuDispatchAppLaunch(&job->m_titleId, &job->m_appInfo, launchAppId,
					 job->m_result);
		return;
	}

	showAppNotFoundOrStoreDialog(-2146420473, launchAppId);
	if (launchAppId != -1)
		topmenuCancelPendingAppLaunch(launchAppId);
}

/* 0x8127E8DA */
static void onLsdbAppInfoForPsgm(void *jobPtr, void *userData)
{
	LsdbGetAppInfoJobResult *job = reinterpret_cast<LsdbGetAppInfoJobResult *>(jobPtr);
	SceInt32 launchAppId = static_cast<SceInt32>(reinterpret_cast<intptr_t>(userData));

	if (job->m_result != 0) {
		char uriBuf[1024];
		char npCommId[132];
		if (sceAppMgrPeekLaunchParamForShell(launchAppId, uriBuf, sizeof(uriBuf)) == 0 &&
		    topmenuParsePsgmNpCommId(uriBuf, npCommId, 128) != 0) {
			paf::common::SharedPtr<paf::job::JobItem> npJob;
			topmenuStartNpCommIdSearch(&npJob, npCommId, launchAppId);
		} else {
			showAppNotFoundOrStoreDialog(job->m_result, launchAppId);
		}
		return;
	}

	char uriBuf[1024];
	char typeBuf[128];
	if (sceAppMgrPeekLaunchParamForShell(launchAppId, uriBuf, sizeof(uriBuf)) == 0 &&
	    topmenuParsePsgmType(uriBuf, typeBuf, sizeof(typeBuf)) != 0 &&
	    sce_paf_strcmp("INVITATION_MESSAGE", typeBuf) == 0 &&
	    SceLsdb_E26FC9E8(&job->m_appInfo) == NULL) {
		showInvitationLaunchDialog(job->m_titleId.c_str(), &job->m_appInfo, launchAppId);
	} else {
		topmenuDispatchAppLaunch(&job->m_titleId, &job->m_appInfo, launchAppId,
					 job->m_result);
	}
}

/* 0x8127E9F8 */
static void onLsdbAppInfoForSyncUpdate(void *jobPtr, void *userData)
{
	LsdbGetAppInfoJobResult *job = reinterpret_cast<LsdbGetAppInfoJobResult *>(jobPtr);
	paf::thread::Semaphore *sem = reinterpret_cast<paf::thread::Semaphore *>(userData);

	if (job->m_result == 0) {
		TopTab *tab = topmenuFindTabByTitleId(g_topWindow, job->m_titleId.c_str());
		if (tab != NULL && tab->m_appData != NULL) {
			LsdbAppInfoPtr infoCopy(job->m_appInfo);
			tab->m_appData->m_process.UpdateAppInfo(infoCopy);
		}
	}

	sem->Release();
}

/* 0x8127EB08 */
static void onGateOpenFinish(void *userData)
{
	(void)userData;
	if (g_topWindow->m_foregroundApp != NULL)
		g_topWindow->m_foregroundApp->SendProcessCommand(1);

	shellAppStateMarkForeground();
	g_topWindow->m_busyRefCount--;
	releaseInputBlocker(g_topWindow);
	if (g_topWindow->m_indicatorIf != NULL)
		g_topWindow->m_indicatorIf->UnlockButton();

	TopmenuAppProcess *fg = g_topWindow->m_foregroundApp;
	if (fg != NULL && g_topWindow->m_gateEffectApp == fg && fg->m_appInfo.get() != NULL &&
	    SceLsdb_849E8DC6(&fg->m_appInfo) != 0 && fg->m_hasAppId &&
	    sceSharedFbGetRenderingInfo(topmenuProcessGetPid(fg)) != 0) {
		ScePafGraphics_12BDEBFF();
	} else {
		g_topWindow->m_gateEffectScene->Hide(0xFF, NULL, NULL, 0.0f);
	}
	g_topWindow->m_gateEffectApp = NULL;
}

/* 0x8127EBF4 */
static void onGateCloseFinish(void *userData)
{
	(void)userData;
	shellAppStateMarkForeground();
	topmenuLaunchPendingCrashReport();

	g_topWindow->m_busyRefCount--;
	releaseInputBlocker(g_topWindow);
	if (g_topWindow->m_indicatorIf != NULL)
		g_topWindow->m_indicatorIf->UnlockButton();

	if (g_topWindow->m_launchSeq.state == 1)
		topmenuAdvanceLaunchScroll(&g_topWindow->m_launchSeq);

	g_topWindow->m_gateEffectScene->Hide(0xFF, NULL, NULL, 0.0f);
	topmenuHelpShowLiveAreaPeel();
	topmenuStartCheckUx0FreeSpace();
	shellEventServerSend(6, NULL, 0);
}

/* 0x8127EC9C */
static void onGateEffectDraw(const paf::ui::Scene::DrawArg &arg)
{
	(void)arg;
	paf::graph::GateBlur::Draw();
}

/* 0x8127ECA4 */
static TopmenuAppProcess *findProcessByBgTitleId(TopWindow *window, const char *bgTitleId)
{
	for (paf::vector<TopTabBase *>::iterator it = window->m_tabs.begin();
	     it != window->m_tabs.end(); ++it) {
		TopTab *tab = reinterpret_cast<TopTab *>(*it);
		if (tab->m_appData != NULL) {
			const char *tabBgTitle = SceLsdb_457F369E(&tab->m_appData->m_process.m_appInfo);
			if (tabBgTitle != NULL &&
			    sce_paf_strncmp(bgTitleId, tabBgTitle, sce_paf_strlen(tabBgTitle)) == 0) {
				return &tab->m_appData->m_process;
			}
		}
	}

	if (window->m_closingApp != NULL) {
		const char *sysBgTitle = SceLsdb_457F369E(&window->m_closingApp->m_appInfo);
		if (sysBgTitle != NULL &&
		    sce_paf_strncmp(bgTitleId, sysBgTitle, sce_paf_strlen(sysBgTitle)) == 0) {
			return window->m_closingApp;
		}
	}
	return NULL;
}

/* ------------------------------------------------------------------ */
/* Chunk 4: TopmenuPluginInterface thunks, background layer stack,    */
/* surface pools, and callbacks (0x8127ED36..0x81281EA8)              */
/* ------------------------------------------------------------------ */

/* 0x8127ED36 */
static int ifCreateCustomPage(int afterPageKey,
			      paf::intrusive_ptr<paf::graph::Surface> *bgSurface)
{
	TopTabBase *afterTab = NULL;
	if (afterPageKey >= 0)
		afterTab = g_topWindow->FindTabByPageKey(afterPageKey);

	if (!ensureTabSlotAvailable(g_topWindow, afterTab))
		return -1;

	TopTab *tab = new TopTab(getTopWindowPlugin(g_topWindow), true);
	if (tab == NULL)
		return -1;

	if (bgSurface != NULL)
		tab->SetIconSurface(*bgSurface);

	g_topWindow->AddTab(tab, true, afterTab);
	tab->m_tabMode = 1;

	const wchar_t *homeTitle = g_topmenuPlugin->GetString(paf::IDParam(TOP_MSG_HOME_TITLE));
	paf::wstring titleStr(homeTitle != NULL ? homeTitle : L"");
	tab->m_titleText->SetString(titleStr);
	tab->m_lockedByApp = true;
	setFocusedTab(g_topWindow, tab, true);
	return tab->m_pageKey;
}

/* 0x8127EEAE */
static int ifGetCurrentPageKey(void)
{
	TopTabBase *tab = g_topWindow->FindFocusedTab();
	return (tab != NULL) ? tab->m_pageKey : -1;
}

/* 0x8127EECC */
static int ifGetPageIndex(int pageKey)
{
	TopTabBase *tab = g_topWindow->FindTabByPageKey(pageKey);
	return (tab != NULL) ? tab->m_index : -1;
}

/* 0x8127EEEE */
static int ifFindPageKeyByTitleId(const char *titleId)
{
	TopTab *tab = topmenuFindTabByTitleId(g_topWindow, titleId);
	return (tab != NULL) ? tab->m_pageKey : -1;
}

/* 0x81430840 */
static paf::job::JobQueue *ifGetJobQueue(void)
{
	return g_topWindow->m_jobQueue;
}

/* 0x8127EF52 */
static paf::ui::Widget *ifGetPageWidget(int pageKey)
{
	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	return (tab != NULL) ? tab->m_contentRoot : NULL;
}

/* 0x8127EF72 */
static paf::ui::Widget *ifGetPageRootWidget(int pageKey)
{
	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	if (tab != NULL && tab->m_appData != NULL)
		return tab->m_appData->m_contentWidget;
	return NULL;
}

/* 0x8127EF96 */
void topmenuDeleteTabByPageKey(SceInt32 pageKey)
{
	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	if (tab != NULL)
		topmenuCloseTab(tab, true);
}

/* 0x8127EFD8 */
static void ifAddPageCloseListener(int pageKey, TopmenuPageEventCallback callback, void *userData)
{
	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	if (tab != NULL)
		tab->AddEventListener(0, reinterpret_cast<TopmenuEventCallback>(callback), userData);
}

/* 0x8127F13C */
static void ifRemovePageCloseListener(int pageKey, TopmenuPageEventCallback callback,
				      void *userData)
{
	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	if (tab != NULL) {
		tab->RemoveEventListener(0, reinterpret_cast<TopmenuEventCallback>(callback),
					 userData);
	}
}

/* 0x8127F216 */
static int ifRequestReturnToHome(void)
{
	return topmenuCanPromoteApp(g_topWindow, 1, true, false) ? 1 : 0;
}

/* 0x8127F22E */
static int ifRequestReturnToHomeImmediate(void)
{
	return topmenuCanPromoteApp(g_topWindow, 1, true, true) ? 1 : 0;
}

/* 0x8127F246 */
static void ifScrollToPage(int pageKey)
{
	TopTabBase *tab = g_topWindow->FindTabByPageKey(pageKey);
	if (tab != NULL)
		snapScrollViewToTab(g_topWindow, tab);
}

/* 0x8127F282 */
static void ifCancelPendingPageTransition(void)
{
	finishOpeningLiveArea(g_topWindow);
}

/* 0x8127F294 */
static void ifCompletePageTransition(int pageKey)
{
	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	if (tab == NULL || tab->m_appData == NULL)
		return;

	if (g_topWindow->m_openingLiveArea && g_topWindow->m_openingPageKey == pageKey)
		g_topWindow->m_liveAreaReady = true;
	tab->m_appData->OnBgLoadStepDone();
}

/* 0x8127F2D4 */
static void ifHidePageContent(int pageKey, float duration)
{
	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	if (tab != NULL && tab->m_appData != NULL)
		tab->m_appData->HideAnimated(duration);
}

/* 0x8127F306 */
static paf::cache::CacheManager *ifGetPageCacheManager(int pageKey)
{
	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	if (tab != NULL && tab->m_poolSlot != NULL && tab->m_poolSlot->cacheManager != NULL)
		return tab->m_poolSlot->cacheManager->GetCacheManager();
	return NULL;
}

/* 0x8127F32A */
static paf::graph::SurfacePool *ifGetPageSurfacePool(int pageKey)
{
	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	if (tab != NULL && tab->m_poolSlot != NULL)
		return tab->m_poolSlot->surfacePool;
	return NULL;
}

/* 0x8127F34C */
static void ifOnGateClick(int pageKey)
{
	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	if (tab == NULL)
		return;
	if (tab != g_topWindow->FindTabBySnapId(g_topWindow->m_activeSnapId))
		return;

	tab->RefreshOrderStamp();
	topmenuSwitchToProcess(g_topWindow, &tab->m_appData->m_process, true);
}

/* 0x8127F3A2 */
static int ifIsPageInteractive(int pageKey)
{
	TopTabBase *tab = g_topWindow->FindTabByPageKey(pageKey);
	return (tab != NULL &&
		tab == g_topWindow->FindTabBySnapId(g_topWindow->m_activeSnapId) &&
		g_topWindow->m_navArrowsEnabled) ? 1 : 0;
}

/* 0x8127F3DC */
static int ifIsPageSelected(int pageKey)
{
	TopTabBase *tab = g_topWindow->FindTabByPageKey(pageKey);
	return (tab != NULL &&
		tab == g_topWindow->FindTabBySnapId(g_topWindow->m_activeSnapId)) ? 1 : 0;
}

/* 0x8127F410 */
static void ifOnManualIconClick(int pageKey)
{
	if (!ifIsPageInteractive(pageKey))
		return;

	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	if (tab == NULL || tab->m_appData == NULL)
		return;

	TopTabAppData *appData = tab->m_appData;
	char paramBuf[1024];
	if (appData->m_dialogId == -1 ||
	    sceAppMgrPeekLaunchParamForShell(appData->m_dialogId, paramBuf, sizeof(paramBuf)) != 0) {
		const char *origPath = sceLsdbGetOriginalPath(&appData->m_process.m_appInfo);
		SceInt32 launchedId =
			sceAppMgrLaunchAppByNameForShell(0x10000u, "NPXS10027", origPath);
		appData->m_dialogId = launchedId;
		if (launchedId >= 0)
			topmenuRegisterSystemAppId(g_topWindow, launchedId);
	}
}

/* 0x8127F4CA */
static void ifOnImportSavedataIconClick(int pageKey)
{
	if (!ifIsPageInteractive(pageKey))
		return;

	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	if (tab != NULL && tab->m_appData != NULL)
		topmenuLaunchImportSavedata(&tab->m_appData->m_process.m_appInfo);
}

/* 0x8127F51E */
static void ifOnOnlineStorageIconClick(int pageKey)
{
	if (!ifIsPageInteractive(pageKey))
		return;

	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	if (tab != NULL && tab->m_appData != NULL)
		topmenuLaunchOnlineStorage(&tab->m_appData->m_process.m_appInfo);
}

/* 0x8127F572 */
static void ifSetPageMusicPlaying(int pageKey, bool playing)
{
	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	if (tab != NULL)
		tab->m_lockedByApp = playing;
}

/* 0x8127F592 */
static const wchar_t *ifGetPageTitle(int pageKey)
{
	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	return (tab != NULL) ? tab->m_title.c_str() : NULL;
}

/* 0x8127F5B2 */
static const char *ifGetCurrentPageTitleId(void)
{
	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindFocusedTab());
	if (tab != NULL && tab->m_appData != NULL)
		return tab->m_appData->m_titleId.c_str();
	return NULL;
}

/* 0x8127F5D6 */
static void pushBgRequestSorted(TopWindow *window, void *owner, SceInt32 priority,
				const TopmenuBgLayerOption *opt)
{
	TopmenuBgRequest req;
	req.owner = owner;
	req.priority = priority;
	if (opt != NULL) {
		req.styleKind = opt->useCustomColor ? 2 : 0;
		req.style = opt->style;
		req.color = opt->color;
	} else {
		req.styleKind = window->m_bgStyleKind;
		req.style = window->m_bgColorParam;
		req.color = static_cast<SceUInt32>(window->m_bgWaveParam);
	}

	for (paf::vector<TopmenuBgRequest>::iterator it = window->m_bgRequests.begin();
	     it != window->m_bgRequests.end(); ++it) {
		if (it->priority < priority) {
			window->m_bgRequests.insert(it, req);
			return;
		}
	}
	window->m_bgRequests.push_back(req);
}

/* 0x8127F6E4 */
static void ifPushBackgroundLayer(void *owner, int priority, bool useCustomColor, int style,
				  SceUInt32 color)
{
	TopmenuBgLayerOption opt;
	opt.useCustomColor = useCustomColor;
	opt.style = style;
	opt.color = color;
	pushBgRequestSorted(g_topWindow, owner, priority, &opt);

	if (!g_topWindow->m_bgRequests.empty()) {
		const TopmenuBgRequest &topReq = g_topWindow->m_bgRequests.back();
		TopmenuBgLayerOption applyOpt;
		applyOpt.useCustomColor = (topReq.styleKind != 0);
		applyOpt.style = topReq.style;
		applyOpt.color = topReq.color;
		applyIndicatorBgStyle(topReq.styleKind, &applyOpt, true);
	} else {
		pushBgRequestSorted(g_topWindow, &g_topWindow->m_bgRequests,
				    getTopWindowPlugin(g_topWindow)->m_draw_priority, NULL);
		refreshDefaultIndicatorBg(g_topWindow);
	}
}

/* 0x8127F95A */
static void removeBgRequestByOwner(TopWindow *window, void *owner)
{
	for (paf::vector<TopmenuBgRequest>::iterator it = window->m_bgRequests.begin();
	     it != window->m_bgRequests.end(); ++it) {
		if (it->owner == owner) {
			window->m_bgRequests.erase(it);
			return;
		}
	}
}

/* 0x8127F9CC */
static void ifPopBackgroundLayer(void *owner, int priority)
{
	(void)priority;
	removeBgRequestByOwner(g_topWindow, owner);

	if (!g_topWindow->m_bgRequests.empty()) {
		const TopmenuBgRequest &topReq = g_topWindow->m_bgRequests.back();
		if (topReq.owner == &g_topWindow->m_bgRequests) {
			refreshDefaultIndicatorBg(g_topWindow);
		} else {
			TopmenuBgLayerOption applyOpt;
			applyOpt.useCustomColor = (topReq.styleKind != 0);
			applyOpt.style = topReq.style;
			applyOpt.color = topReq.color;
			applyIndicatorBgStyle(topReq.styleKind, &applyOpt, true);
		}
	} else {
		pushBgRequestSorted(g_topWindow, &g_topWindow->m_bgRequests,
				    getTopWindowPlugin(g_topWindow)->m_draw_priority, NULL);
		refreshDefaultIndicatorBg(g_topWindow);
	}
}

/* 0x8127F784 */
static void ifEnterAppForeground(void)
{
	if (!g_topWindow->m_gateEffectActive) {
		if (g_topmenuPsCloseDialogId >= 0) {
			shellStateWatcherCloseDialog(g_topmenuPsCloseDialogId);
			g_topmenuPsCloseDialogId = -1;
		}

		if (paf::system::Model::GetType() != 1) {
			pushBgRequestSorted(g_topWindow, g_topWindow,
					    getTopWindowPlugin(g_topWindow)->m_draw_priority, NULL);
		}

		ApplauncherInterface *appIf = topmenuGetApplauncherInterface();
		if (appIf != NULL)
			appIf->func[16]();

		shellEventServerSend(2, NULL, 0);

		if (paf::system::Model::GetType() != 1) {
			ifPushBackgroundLayer(&g_topWindow->m_gateEffectActive, 27, true, 2, 0);
		}
	}

	paf::ui::Widget *scrollScene = g_topWindow->m_scrollView->GetViewParent()->m_scene;
	hideWidgetNow(scrollScene);
	showWidgetNow(g_topWindow->m_overlayBgWidget);
	ScePafGraphics_04F03872();
	g_topWindow->m_gateEffectActive = true;
}

/* 0x8127FA7E */
static void ifLeaveAppForeground(bool animate, const char *titleId)
{
	if (!g_topmenuBootFreeSpaceChecked) {
		g_topmenuBootFreeSpaceChecked = true;
		topmenuStartCheckUx0FreeSpace();
	}

	if (g_topWindow->m_gateEffectActive) {
		if (paf::system::Model::GetType() != 1) {
			ifPopBackgroundLayer(&g_topWindow->m_gateEffectActive, 230);
			ifPopBackgroundLayer(g_topWindow, 230);
		}

		ApplauncherInterface *appIf = topmenuGetApplauncherInterface();
		if (appIf != NULL) {
			typedef void (*LeaveFgFn)(int);
			reinterpret_cast<LeaveFgFn>(appIf->func[17])(0);
		}

		const char *fgTitle = "NPXS19999";
		if (g_topWindow->m_foregroundApp != NULL)
			fgTitle = g_topWindow->m_foregroundApp->m_titleId.c_str();
		shellEventServerSend(3, NULL, reinterpret_cast<intptr_t>(fgTitle));

		if (g_topWindow->m_powerSuspendMode == 0)
			musicCoreBgmControl(0, 0);
	}

	paf::ui::Widget *scrollScene = g_topWindow->m_scrollView->GetViewParent()->m_scene;
	showWidgetNow(scrollScene);
	ScePafGraphics_04F03872();

	if (animate) {
		hideWidgetWithTimer(g_topWindow->m_overlayBgWidget, new paf::Timer(0.0f));
		topmenuDialogCloseAll();

		if (titleId != NULL && g_topWindow->m_foregroundApp != NULL) {
			const char *fgTitle = g_topWindow->m_foregroundApp->m_titleId.c_str();
			if (sce_paf_strncmp(titleId, fgTitle, sce_paf_strlen(fgTitle)) != 0)
				topmenuSwitchToProcess(g_topWindow, NULL, true);
		}
		hideShellScenesForApp(g_topWindow);
	} else {
		hideWidgetNow(g_topWindow->m_overlayBgWidget);
	}

	g_topWindow->m_gateEffectActive = false;
}

/* 0x8127FCE4 */
static int ifIsAppInForeground(void)
{
	return g_topWindow->m_gateEffectActive ? 1 : 0;
}

/* 0x8127FCF4 */
static int ifIsHomePageSelected(void)
{
	TopmenuAppProcess *fg = g_topWindow->m_foregroundApp;
	return (fg != NULL && fg->m_tabAppData == NULL) ? 1 : 0;
}

/* 0x8127FD0E */
static int ifGetOpenAppPageCount(void)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	int count = static_cast<int>(g_topWindow->m_tabs.size()) - 1;
	paf::thread::RMutex::main_thread_mutex.Unlock();
	return count;
}

/* 0x8127FD36 */
static int ifGetRunningApplications(paf::vector<TopmenuRunningAppInfo> *outApps)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	if (g_topWindow == NULL) {
		paf::thread::RMutex::main_thread_mutex.Unlock();
		return 0;
	}

	if (outApps->capacity() < g_topWindow->m_tabs.size())
		outApps->reserve(g_topWindow->m_tabs.size());

	for (paf::vector<TopTabBase *>::iterator it = g_topWindow->m_tabs.begin();
	     it != g_topWindow->m_tabs.end(); ++it) {
		TopTab *tab = reinterpret_cast<TopTab *>(*it);
		if (tab == NULL || tab->m_appData == NULL)
			continue;

		TopTabAppData *appData = tab->m_appData;
		TopmenuRunningAppInfo info;
		sce_paf_memset(&info, 0, sizeof(info));
		sce_paf_strncpy(info.titleId, appData->m_titleId.c_str(), 10);
		if (appData->m_process.m_appId == -1) {
			info.launchState = 0;
		} else {
			info.launchState = appData->m_process.m_isActive ? 1 : 2;
		}
		info.processId = appData->m_mediaAppType;
		info.appId = appData->m_mediaStorageKind;
		outApps->push_back(info);
	}

	paf::thread::RMutex::main_thread_mutex.Unlock();
	return 1;
}

/* 0x8127FE64 */
static int ifBeginAppInstallLock(const char *titleId)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	ApplauncherInterface *appIf = topmenuGetApplauncherInterface();
	if (appIf == NULL || g_topWindow->m_closingAllTabs) {
		paf::thread::RMutex::main_thread_mutex.Unlock();
		return 0;
	}

	g_topWindow->m_closingAllTabs = true;
	TopTab *tab = topmenuFindTabByTitleId(g_topWindow, titleId);
	if (tab != NULL && tab->m_appData != NULL &&
	    topmenuAppProcessIsRunning(&tab->m_appData->m_process)) {
		tab->m_appData->m_process.SendProcessCommand(5);
	}

	typedef void (*LockTitleFn)(const char *);
	reinterpret_cast<LockTitleFn>(appIf->func[12])(titleId);
	paf::thread::RMutex::main_thread_mutex.Unlock();
	return 1;
}

/* 0x8127FF48 */
static int ifEndAppInstallLock(const char *titleId, int relaunchMode)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	ApplauncherInterface *appIf = topmenuGetApplauncherInterface();
	if (appIf == NULL || !g_topWindow->m_closingAllTabs) {
		paf::thread::RMutex::main_thread_mutex.Unlock();
		return 0;
	}

	g_topWindow->m_closingAllTabs = false;
	typedef void (*UnlockTitleFn)(const char *);
	reinterpret_cast<UnlockTitleFn>(appIf->func[13])(titleId);

	TopTab *tab = topmenuFindTabByTitleId(g_topWindow, titleId);
	if (tab != NULL) {
		LiveAreaPluginInterface *laIf = topmenuGetLiveAreaInterface();
		if (laIf != NULL)
			laIf->refreshPage(tab->m_pageKey);
	}

	if (relaunchMode == 1)
		sceAppMgrLaunchAppByNameForShell(0, titleId, NULL);
	else if (relaunchMode == 2)
		sceAppMgrLaunchAppByNameForShell(0x20000u, titleId, NULL);

	paf::thread::RMutex::main_thread_mutex.Unlock();
	return 1;
}

/* 0x8128006C */
static int ifOpenAppPage(const paf::string *titleId, int *inOutPageKey,
			 paf::intrusive_ptr<paf::graph::Surface> *iconSurface, int launchAppId)
{
	if (launchAppId != -1)
		g_topWindow->m_pendingKillAppId = launchAppId;

	if (*inOutPageKey != -1) {
		TopTabBase *existingTab = g_topWindow->FindTabByPageKey(*inOutPageKey);
		if (existingTab != NULL) {
			TopmenuOpenAppTabParam param;
			param.titleId = *titleId;
			TopTabBase *anchorTab =
				(g_topWindow->m_switchToHomeOnClose &&
				 g_topWindow->m_tabs.size() >= 2) ?
				g_topWindow->m_tabs[1] : g_topWindow->m_tabs[0];
			param.afterPageKey = anchorTab->m_pageKey;
			param.autoStart = (launchAppId != -1) || (shellStartupGetIduMode() == 1);
			param.launchAppId = launchAppId;

			if (!activateExistingAppTab(g_topWindow, &param, existingTab->m_pageKey,
						    false) &&
			    launchAppId != -1) {
				topmenuCancelPendingAppLaunch(launchAppId);
			}
		}
		return 0;
	}

	bool startOnLiveArea = (launchAppId == -1);
	bool autoStart = (launchAppId != -1);
	bool isIdu = (shellStartupGetIduMode() == 1);
	if (isIdu)
		autoStart = true;

	TopTabBase *anchorTab =
		(g_topWindow->m_switchToHomeOnClose && g_topWindow->m_tabs.size() >= 2) ?
		g_topWindow->m_tabs[1] : g_topWindow->m_tabs[0];

	LsdbAppInfoPtr emptyInfo;
	paf::intrusive_ptr<paf::cache::CacheImageInterface> *iconCache =
		reinterpret_cast<paf::intrusive_ptr<paf::cache::CacheImageInterface> *>(iconSurface);
	SceInt32 newPageKey = createAppTab(anchorTab->m_pageKey, *titleId, emptyInfo, iconCache,
					   startOnLiveArea, false, autoStart, isIdu, false,
					   launchAppId);
	*inOutPageKey = newPageKey;
	if (newPageKey == -1)
		return 0;

	if (!startOnLiveArea) {
		TopmenuLaunchSeqParam seqParam;
		seqParam.targetPageKey = newPageKey;
		seqParam.autoStart = true;
		seqParam.holdBusy = false;
		seqParam.skipScroll = true;
		seqParam.closePageKey = -1;
		seqParam.immediateStart = false;
		seqParam.isWebApp = false;
		seqParam.openPatchChk = false;
		seqParam.notifyApplauncher = false;
		topmenuStartLaunchSequence(&g_topWindow->m_launchSeq, &seqParam);
	}

	topmenuGetApplauncherInterface();
	if (!g_topWindow->m_openingLiveArea) {
		g_topWindow->m_openingPageKey = *inOutPageKey;
		g_topWindow->m_openingLiveArea = true;
		g_topWindow->m_liveAreaReady = false;
	}

	topmenuHelpPrepareLiveAreaIntro();
	paf::Framework::Instance()->PlayCommonSE(27, 0.0f, false);
	return 1;
}

/* 0x81280356 */
static void ifSetPageSurfaceByTitleId(const char *const *titleId,
				      paf::intrusive_ptr<paf::graph::Surface> *surface)
{
	TopTab *tab = topmenuFindTabByTitleId(g_topWindow, *titleId);
	if (tab == NULL)
		return;

	paf::intrusive_ptr<paf::cache::CacheImageInterface> *cacheImg =
		reinterpret_cast<paf::intrusive_ptr<paf::cache::CacheImageInterface> *>(surface);
	tab->SetIconCache(*cacheImg);
	if (surface->get() == NULL) {
		paf::intrusive_ptr<paf::graph::Surface> emptySurface;
		tab->SetIconSurface(emptySurface);
	}
	if (g_topWindow->m_indicatorIf != NULL) {
		g_topWindow->m_indicatorIf->SetPageDotImage(
			tab->m_pageKey,
			reinterpret_cast<const paf::intrusive_ptr<paf::ui::TextureSource> *>(surface));
	}
}

/* 0x81280416 */
static void ifSetApplauncherInterface(ApplauncherInterface *iface)
{
	if (g_topWindow != NULL)
		g_topWindow->m_applauncherIf = iface;
}

/* 0x81280426 */
static void ifSetLiveAreaInterface(LiveAreaPluginInterface *iface)
{
	if (g_topWindow == NULL)
		return;

	g_topWindow->m_liveAreaIf = iface;
	if (paf::system::Model::GetType() == 1 && SceActivityDb_6D8000C1() != 0 &&
	    shellStartupGetIduMode() == 0) {
		paf::intrusive_ptr<paf::cache::CacheImageInterface> emptyIcon;
		SceInt32 firstPageKey =
			!g_topWindow->m_tabs.empty() ? g_topWindow->m_tabs[0]->m_pageKey : -1;
		paf::string activityTitle(SceActivityDb_E6C23731());
		LsdbAppInfoPtr emptyInfo;
		createAppTab(firstPageKey, activityTitle, emptyInfo, &emptyIcon,
			     false, false, false, false, false, -1);

		TopTab *tab = topmenuFindTabByTitleId(g_topWindow, SceActivityDb_E6C23731());
		if (tab != NULL) {
			tab->m_appData->SetPageTurnDisabled(true);
			tab->m_lockedByApp = true;
			g_topWindow->m_switchToHomeOnClose = true;
		}
	}
}

/* 0x812805C6 */
static void ifClosePageByTitleId(const char *titleId)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	TopTab *tab = topmenuFindTabByTitleId(g_topWindow, titleId);
	if (tab != NULL)
		topmenuCloseTab(tab, false);
	paf::thread::RMutex::main_thread_mutex.Unlock();
}

/* 0x81280662 */
static void ifCloseCurrentAppIfMatches(const char *titleId)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	TopmenuAppProcess *fg = g_topWindow->m_foregroundApp;
	if (fg != NULL) {
		const char *fgTitle = fg->m_titleId.c_str();
		if (sce_paf_strncmp(titleId, fgTitle, sce_paf_strlen(fgTitle)) == 0)
			returnToHomeOrIdu(g_topWindow);
	}
	paf::thread::RMutex::main_thread_mutex.Unlock();
}

/* 0x812806AE */
static void ifReloadWhatsNew(void)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	topmenuWhatsNewForceRefresh();
	paf::thread::RMutex::main_thread_mutex.Unlock();
}

/* 0x812806CA */
static int ifStartAppDataPromote(const char *titleId, TopmenuPromoteCallback callback,
				 void *userData)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	TopTab *tab = topmenuFindTabByTitleId(g_topWindow, titleId);
	int result = 0;
	if (tab != NULL && tab->m_appData != NULL) {
		result = topmenuStartAppDataPromote(&tab->m_appData->m_process.m_appInfo,
						    callback, userData, false) ? 1 : 0;
	}
	paf::thread::RMutex::main_thread_mutex.Unlock();
	return result;
}

/* 0x81280772 */
static void ifNotifyUpStatusChanged(const char *titleId, int upStatus)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	TopTab *tab = topmenuFindTabByTitleId(g_topWindow, titleId);
	if (tab != NULL && tab->m_appData != NULL)
		tab->m_appData->UpdateUpStatus(upStatus);
	paf::thread::RMutex::main_thread_mutex.Unlock();
}

/* 0x812807F4 */
static int ifGetAppInfoByTitleId(LsdbAppInfoPtr *outInfo, const char *titleId)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	TopmenuAppProcess *proc = topmenuFindProcessByTitleId(g_topWindow, titleId);
	int found = 0;
	if (proc != NULL) {
		*outInfo = proc->m_appInfo;
		found = 1;
	}
	paf::thread::RMutex::main_thread_mutex.Unlock();
	return found;
}

/* 0x812808AC */
static const char *ifGetAppExecInfoByBgTitleId(const char *bgTitleId, SceUInt64 *outTimestamp)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	TopmenuAppProcess *proc = findProcessByBgTitleId(g_topWindow, bgTitleId);
	const char *titleId = NULL;
	if (proc != NULL) {
		*outTimestamp = topmenuGetAppFlags(g_topWindow, bgTitleId);
		titleId = topmenuProcessGetTitleId(proc);
	}
	paf::thread::RMutex::main_thread_mutex.Unlock();
	return titleId;
}

/* 0x81280944 */
static void ifGetOpenPageTitleIds(paf::vector<paf::string> *outTitles)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	for (size_t i = 0; i < g_topWindow->m_tabs.size(); ++i) {
		TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->m_tabs[i]);
		if (tab != NULL && tab->m_appData != NULL)
			outTitles->push_back(tab->m_appData->m_titleId);
	}
	paf::thread::RMutex::main_thread_mutex.Unlock();
}

/* 0x81280A58 */
static paf::ui::Widget *ifGetPageTurnWidget(int pageKey)
{
	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	if (tab != NULL && tab->m_appData != NULL)
		return tab->m_appData->m_pageTurn;
	return NULL;
}

/* 0x81280A7C */
static void ifSetPageScrolledDown(int pageKey, bool scrolledDown)
{
	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	if (tab != NULL && tab->m_appData != NULL)
		tab->m_appData->SetPageTurnDisabled(scrolledDown);
}

/* 0x81280AA2 */
static void ifAddPageShowListener(int pageKey, TopmenuPageEventCallback callback, void *userData)
{
	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	if (tab != NULL)
		tab->AddEventListener(1, reinterpret_cast<TopmenuEventCallback>(callback), userData);
}

/* 0x81292F24 */
static void ifAddPageHideListener(int pageKey, TopmenuPageEventCallback callback, void *userData)
{
	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	if (tab != NULL)
		tab->AddEventListener(2, reinterpret_cast<TopmenuEventCallback>(callback), userData);
}

/* 0x81280BFE */
static int ifNotifyContentUpdated(const char *const *updateArgs)
{
	return reloadLiveAreaForTitle(g_topWindow,
				      reinterpret_cast<const LiveAreaReloadRequest *>(updateArgs));
}

/* 0x81280C14 */
static paf::intrusive_ptr<paf::graph::Surface> *ifGetDefaultBgTexture(void)
{
	return &g_topWindow->m_defaultBgTex;
}

/* 0x81280C22 */
static void ifAddPageAddedListener(TopmenuWindowEventCallback callback, void *userData)
{
	g_topWindow->AddEventListener(0, reinterpret_cast<TopmenuEventCallback>(callback),
				      userData);
}

/* 0x81280D78 */
static void ifRemovePageAddedListener(TopmenuWindowEventCallback callback, void *userData)
{
	g_topWindow->RemoveEventListener(0, reinterpret_cast<TopmenuEventCallback>(callback),
					 userData);
}

/* 0x81280E42 */
static void ifAddPageRemovedListener(TopmenuWindowEventCallback callback, void *userData)
{
	g_topWindow->AddEventListener(1, reinterpret_cast<TopmenuEventCallback>(callback),
				      userData);
}

/* 0x81280F90 */
static void ifRemovePageRemovedListener(TopmenuWindowEventCallback callback, void *userData)
{
	g_topWindow->RemoveEventListener(1, reinterpret_cast<TopmenuEventCallback>(callback),
					 userData);
}

/* 0x8128105A */
static void ifAddPageFocusListener(TopmenuWindowEventCallback callback, void *userData)
{
	g_topWindow->AddEventListener(2, reinterpret_cast<TopmenuEventCallback>(callback),
				      userData);
}

/* 0x812811A8 */
static void ifRemovePageFocusListener(TopmenuWindowEventCallback callback, void *userData)
{
	g_topWindow->RemoveEventListener(2, reinterpret_cast<TopmenuEventCallback>(callback),
					 userData);
}

/* 0x81281272 */
static void ifBeginHold(void)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	g_topWindow->m_suspendLockCount++;
	paf::thread::RMutex::main_thread_mutex.Unlock();
}

/* 0x8128129E */
static void ifEndHold(void)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	g_topWindow->m_suspendLockCount--;
	paf::thread::RMutex::main_thread_mutex.Unlock();
}

/* 0x812812CA */
void topmenuBeginGameHold(void)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	shellParentalControlPause();
	g_topWindow->m_gameHoldCount++;
	paf::thread::RMutex::main_thread_mutex.Unlock();
}

/* 0x812812FC */
void topmenuEndGameHold(void)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	g_topWindow->m_gameHoldCount--;
	shellParentalControlResume();
	paf::thread::RMutex::main_thread_mutex.Unlock();
}

/* 0x8128132C */
static int ifIsBusy(void)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	int busy = (g_topWindow->m_gameHoldCount > 0) ? 1 : 0;
	paf::thread::RMutex::main_thread_mutex.Unlock();
	return busy;
}

/* 0x8128135A */
static void ifEnterEditMode(void)
{
	enterEditMode(g_topWindow);
}

/* 0x8128136C */
static void ifLeaveEditMode(void)
{
	leaveEditMode(g_topWindow);
}

/* 0x8128137E */
static void ifSetPendingLaunchParam(int param)
{
	g_topWindow->m_focusedNavWidget = reinterpret_cast<paf::ui::Widget *>(
		static_cast<intptr_t>(param));
}

/* 0x8128138E */
bool topmenuIsPowerSuspendNormal(void)
{
	SceInt32 mode = g_topWindow->m_shellViewMode;
	return (mode == 1 || mode == 3 || mode == 5);
}

/* 0x812813B0 */
static void ifOnInitialBootReady(void)
{
	if (shellStartupNeedsInitialSetup())
		topmenuHelpShowHomeHelp();
	topmenuProcessCrashReportOnBoot();
	g_topWindow->m_whatsNewNotifyEnabled = true;
	if (g_topWindow->m_psnSignInState == 1)
		topmenuWhatsNewShow();
}

/* 0x812813E0 */
static void ifSetScrollEnabled(bool enabled)
{
	if (enabled)
		g_topWindow->m_scrollLockCount--;
	else
		g_topWindow->m_scrollLockCount++;

	if (g_topWindow->m_scrollLockCount <= 0)
		g_topWindow->m_scrollView->SetMovableDirOn(0);
	else
		g_topWindow->m_scrollView->SetMovableDirOn(3);

	updatePageArrows(g_topWindow);
}

/* 0x81281424 */
SceInt32 topmenuGetPsnSignInState(void)
{
	return g_topWindow->m_psnSignInState;
}

struct TopmenuPsStoreLogoParam {
	SceInt32 appId;
	SceInt32 parentAppId;
};

/* 0x812817C4 */
static void onPsStoreLogoOpen(void *arg)
{
	TopmenuPsStoreLogoParam *param = reinterpret_cast<TopmenuPsStoreLogoParam *>(arg);
	TopmenuAppProcess *proc = topmenuFindProcessByAppId(g_topWindow, param->appId);
	if (proc != NULL)
		proc->SetParentAppId(param->parentAppId);
	delete param;
}

/* 0x812817EC */
static void onPsStoreLogoClose(void *arg)
{
	SceInt32 appId = static_cast<SceInt32>(reinterpret_cast<intptr_t>(arg));
	TopmenuAppProcess *proc = topmenuFindProcessByAppId(g_topWindow, appId);
	if (proc != NULL)
		proc->SetParentAppId(-1);
}

/* 0x81281434 */
static int ifOpenPsStoreLogo(int appId, int parentAppId)
{
	TopmenuPsStoreLogoParam *param = new TopmenuPsStoreLogoParam();
	param->appId = appId;
	param->parentAppId = parentAppId;
	paf::thread::CallQueue::main_thread_callqueue.Enqueue(
		"psStoreLogoOpen", onPsStoreLogoOpen, param);
	return 0;
}

/* 0x8128147C */
static int ifClosePsStoreLogo(void *arg)
{
	paf::thread::CallQueue::main_thread_callqueue.Enqueue(
		"psStoreLogoClose", onPsStoreLogoClose, arg);
	return 0;
}

/* 0x8128149E */
static void ifRefreshAppInfoSync(const char *titleId)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	TopTab *tab = topmenuFindTabByTitleId(g_topWindow, titleId);
	paf::thread::RMutex::main_thread_mutex.Unlock();

	if (tab == NULL)
		return;

	static paf::thread::Semaphore s_appInfoUpdateSem("SceShellAppInfoUpdate", 0, 1, NULL);
	paf::string titleStr(titleId != NULL ? titleId : "");
	paf::common::SharedPtr<paf::job::JobItem> job;
	lsdbEnqueueGetAppInfoJob(&job, g_topWindow->m_jobQueue, titleStr,
				 onLsdbAppInfoForSyncUpdate, &s_appInfoUpdateSem);
	s_appInfoUpdateSem.Acquire();
}

/* 0x81273ED6 wrapper */
static void ifReloadSystemBgm(void)
{
	loadHomeBgmFromTheme();
}

/* 0x81281660 */
static void ifInitLiveboardSurfacePools(void)
{
	if (g_topmenuSurfaceMem != NULL)
		return;

	static const SceSize kLiveboardPoolTotalSize = 6920598;
	static const SceSize kLiveboardPoolSliceSize = 1153433;
	g_topmenuSurfaceMem = paf::GraphMem::AllocOSMemory(
		paf::GraphMem::DeviceType_SystemMemory, kLiveboardPoolTotalSize,
		"SceShellLiveboardSurface", NULL);

	SceUInt8 *sliceBase = reinterpret_cast<SceUInt8 *>(g_topmenuSurfaceMem);
	for (unsigned int i = 0; i < 6; ++i) {
		paf::memory::HeapAllocator::Option poolOpt;
		poolOpt.entry_max = 0;
		poolOpt.warning_size = 0;
		poolOpt.disable_dbgprint = false;
		poolOpt.disable_assert_on_alloc_failure = true;
		poolOpt.memblk_type = paf::memory::AllocationBlockType_User;
		poolOpt.alignment = 307200;

		char poolName[64];
		sce_paf_snprintf(poolName, sizeof(poolName), "SceShellLiveboardSurface%d", i);
		g_topmenuSurfacePools[i].surfacePool = new paf::graph::SurfacePool(
			sliceBase, kLiveboardPoolSliceSize, poolName, &poolOpt,
			paf::graph::SurfacePool::MemoryType_Normal);
		g_topmenuSurfacePools[i].cacheManager =
			new TopTabCacheManager(g_topmenuSurfacePools[i].surfacePool);
		sliceBase += kLiveboardPoolSliceSize;
	}
}

/* 0x81281758 */
void topmenuProcessListAdd(TopmenuProcessBase *proc)
{
	g_topWindow->m_pendingLaunchQueue.push_back(proc);
}

/* 0x8128178E */
void topmenuProcessListRemove(TopmenuProcessBase *proc)
{
	g_topWindow->m_pendingLaunchQueue.remove(proc);
}

/* 0x8128180E */
static void onTopWindowPowerKey(void *userData)
{
	onTopWindowPsButton(reinterpret_cast<TopWindow *>(userData));
}

/* 0x81281816 */
static void onTopWindowTabAdded(SceInt32 eventType, void *sender, SceInt32 pageKey, void *userData)
{
	(void)eventType;
	(void)sender;
	(void)userData;
	if (g_topWindow->m_indicatorIf == NULL)
		return;

	if (g_topWindow->m_indicatorIf->AddPageDot(pageKey) == NULL)
		return;

	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	if (tab == NULL)
		return;

	if (tab->m_iconSurface.get() != NULL) {
		g_topWindow->m_indicatorIf->SetPageDotTexture(pageKey, tab->m_iconSurface);
	} else if (tab->m_iconCache.get() != NULL) {
		g_topWindow->m_indicatorIf->SetPageDotImage(
			pageKey,
			reinterpret_cast<const paf::intrusive_ptr<paf::ui::TextureSource> *>(
				&tab->m_iconCache));
	}
}

/* 0x8128187C */
static void onTopWindowTabRemoved(SceInt32 eventType, void *sender, SceInt32 pageKey,
				  void *userData)
{
	(void)eventType;
	(void)sender;
	(void)userData;
	if (g_topWindow->m_indicatorIf != NULL)
		g_topWindow->m_indicatorIf->RemovePageDot(pageKey);
}

/* 0x81281892 */
static void onTopWindowTabsCleared(SceInt32 eventType, void *sender, SceInt32 unused,
				   void *userData)
{
	(void)eventType;
	(void)sender;
	(void)unused;
	(void)userData;
	if (g_topWindow->m_indicatorIf != NULL)
		g_topWindow->m_indicatorIf->RefreshPageDots();
}

/* 0x812818A6 */
static void onTopWindowArrowLeft(SceInt32 eventType, paf::ui::Handler *self,
				 paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)self;
	(void)event;
	(void)userData;
	TopTabBase *cur = g_topWindow->FindFocusedTab();
	if (cur == NULL)
		return;

	TopTabBase *prev = g_topWindow->GetPrevTab(cur);
	if (prev == NULL)
		return;

	g_topWindow->ScrollToTabAnimated(prev);
	if (g_topWindow->m_openingLiveArea) {
		ApplauncherInterface *appIf = topmenuGetApplauncherInterface();
		if (appIf != NULL)
			appIf->func[8]();
	}
}

/* 0x8128190E */
static void onTopWindowArrowRight(SceInt32 eventType, paf::ui::Handler *self,
				  paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)self;
	(void)event;
	(void)userData;
	TopTabBase *cur = g_topWindow->FindFocusedTab();
	if (cur == NULL)
		return;

	TopTabBase *next = g_topWindow->GetNextTab(cur);
	if (next == NULL)
		return;

	g_topWindow->ScrollToTabAnimated(next);
	if (g_topWindow->m_openingLiveArea) {
		ApplauncherInterface *appIf = topmenuGetApplauncherInterface();
		if (appIf != NULL)
			appIf->func[8]();
	}
}

/* 0x81281D24 */
static void onInviteDialogIconReady(paf::intrusive_ptr<paf::cache::CacheImageInterface> *iconCache,
				    SceInt32 dialogId)
{
	paf::ui::Widget *bodyBox = SceCommonGuiDialog_8947FC23(dialogId, 10);
	if (bodyBox == NULL)
		return;

	paf::ui::Widget *iconWidget =
		bodyBox->FindChild(paf::IDParam(TOP_WINDOW_DIALOG_ICON_ID), 0);
	if (iconWidget != NULL)
		iconWidget->SetDrawObjMaskBit(iconCache, 0);
}

/* 0x81281DCA */
static void onTabIconReady(paf::intrusive_ptr<paf::cache::CacheImageInterface> *iconCache,
			   SceInt32 pageKey)
{
	TopTab *tab = reinterpret_cast<TopTab *>(g_topWindow->FindTabByPageKey(pageKey));
	if (tab == NULL)
		return;

	tab->SetIconCache(*iconCache);
	if (g_topWindow->m_indicatorIf != NULL) {
		g_topWindow->m_indicatorIf->SetPageDotImage(
			pageKey,
			reinterpret_cast<const paf::intrusive_ptr<paf::ui::TextureSource> *>(
				iconCache));
	}
}

/* 0x81281DFE */
static void onParentalBrowserFinish(SceInt32 result, TopmenuAppProcess *proc)
{
	if (result != 0) {
		if (proc != NULL)
			proc->m_spawnJob.reset();
	} else {
		g_topmenuParentalBypassOnce = true;
	}

	paf::ui::Handler *viewParent = g_topWindow->m_scrollView->GetViewParent();
	viewParent->KillTimer(TIMER_PARENTAL_BROWSER_ID);
}

/* 0x81281E86 */
static void onParentalBrowserRetry(TopmenuAppProcess *proc)
{
	if (g_topmenuParentalBypassOnce)
		topmenuSwitchToProcess(g_topWindow, proc, true);
}

/* 0x81281EA8 */
struct TopmenuStaticKeyHashInit {
	TopmenuStaticKeyHashInit()
	{
		g_hashLiveAreaPath = paf::IDParam("#_livearea_path").GetIDHash();
		g_hashCtime = paf::IDParam("#_ctime").GetIDHash();
		g_hashLltime = paf::IDParam("#_lltime").GetIDHash();
		g_hashPromoteAttr = paf::IDParam("#_promote_attr").GetIDHash();
		g_hashUpStatus = paf::IDParam("#_up_status").GetIDHash();
		g_hashLibLocation = paf::IDParam("#LIBLOCATION").GetIDHash();
		g_hashAppConf = paf::IDParam("#appconf").GetIDHash();
	}
};
static TopmenuStaticKeyHashInit s_topmenuKeyHashInit;
