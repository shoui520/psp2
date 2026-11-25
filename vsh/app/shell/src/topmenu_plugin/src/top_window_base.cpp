/*
 * top_window_base.cpp: TopTabBase, TopWindowBase scroll/layout container, and
 * AppDataPromoteJob (0x812845A8-0x81286BF3).
 */
#include "topmenu_internal.h"
#include <appmgr.h>
#include <math.h>

namespace {

static const SceUInt32 PROMOTE_MSG_PREPARING_ID = 0x632C3F93u;	/* 1663788851 */
static const SceUInt32 PROMOTE_MSG_INSTALLING_ID = 0x2A85FB97u;	/* 713433959 */

static paf::Timer *createLayoutTimer(float durationMs, paf::Timer::Func funcType)
{
	paf::Timer *timer = new paf::Timer();
	if (timer != NULL)
		timer->InitTimer(durationMs, funcType, paf::Timer::REPEAT_NONE, -1, NULL);
	return timer;
}

static void showTabWidgetImmediate(paf::ui::Widget *widget)
{
	widget->m_transition_complete = false;
	widget->m_transition = 0xFF;
	widget->DoTransitionReverse_core(1.0f, true);
	ScePafWidget_4588699E(widget, NULL, 2, 0.0f, NULL);

	paf::Timer *timer = createLayoutTimer(0.0f, paf::Timer::FUNC_LINEAR);
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xFF;
		widget->m_transition_complete = true;
		widget->DoTransition_core(1.0f, true);
	}
	ScePafWidget_FF5BC957(widget, timer, 2, 0.0f, NULL);
}

/* 0x81286044 */
static void onTopWindowScrollBegin(int32_t eventType, paf::ui::Handler *self,
				   paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)self;
	(void)event;
	TopWindowBase *window = (TopWindowBase *)userData;
	window->DispatchEvent(3, window, 0);
}

/* 0x8128611A */
static void onTopWindowScrollEnd(int32_t eventType, paf::ui::Handler *self,
				 paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)self;
	(void)event;
	TopWindowBase *window = (TopWindowBase *)userData;
	for (SceUInt32 i = 0; i < window->m_tabs.size(); ++i)
		window->m_tabs[i]->UpdateViewportVisibility();
	window->DispatchEvent(2, window, 0);
}

/* AppDataPromoteJob (140 bytes, 0x8128622C..0x81286A4C) */
class AppDataPromoteJob : public paf::job::JobItem {
public:
	/* 0x8128622C */
	AppDataPromoteJob(const LsdbAppInfoPtr &appInfo,
			  TopmenuPromoteDoneCallback doneCallback,
			  void *userData, bool isLaunchPromote)
		: paf::job::JobItem("AppDataPromoteJob", NULL),
		  m_appInfo(appInfo),
		  m_isLaunchPromote(isLaunchPromote),
		  m_waitDialogId(-1),
		  m_doneCallback(doneCallback),
		  m_userData(userData),
		  m_didBeginPromote(false),
		  m_promoteTitleId(),
		  m_installError(),
		  m_hadPatchInstalled(false)
	{
		paf::thread::RMutex::main_thread_mutex.Lock();
		CommonGuiLayoutDialogParam dialogParam;
		dialogParam.resourcePlugin = g_topmenuPlugin;
		dialogParam.priority = 34;
		dialogParam.layout = SceCommonGuiDialog_2A68B4A4;
		dialogParam.text = paf::wstring(
			g_topmenuPlugin->GetString(paf::IDParam(PROMOTE_MSG_PREPARING_ID)));
		m_waitDialogId = SceCommonGuiDialog_CCFCD9A6(&dialogParam);
		paf::thread::RMutex::main_thread_mutex.Unlock();
	}

	/* 0x8128643A / 0x81286552 */
	virtual ~AppDataPromoteJob()
	{
		if (m_waitDialogId >= 0) {
			SceCommonGuiDialog_BC311DC0(m_waitDialogId);
			m_waitDialogId = -1;
		}
	}

	/* 0x81286562 */
	virtual int32_t Run()
	{
		bool isPspOrPs1 = false;
		bool isPsm = false;
		bool isPsmRuntime = false;

		int appType = sceLsdbGetType(&m_appInfo);
		if (appType == 2 || appType == 3)
			isPspOrPs1 = true;
		else if (appType == 4)
			isPsm = true;
		else if (appType == 5)
			isPsmRuntime = true;

		int bgdlTaskCount = 0;
		int gamePendingCount = 0;

		if (isPspOrPs1) {
			if (!Bgdl_81113900(&m_appInfo, &m_promoteTitleId))
				return (SceInt32)0x80103900u;
			bgdlTaskCount = shellBgdlCountAppTasks(m_promoteTitleId.c_str());
			if (bgdlTaskCount == 0)
				return 0;
		} else if (isPsm) {
			bgdlTaskCount = shellBgdlCountTitleTasks("PCSI00011");
			const char *titleId = sceLsdbGetName(&m_appInfo);
			if (titleId != NULL) {
				m_promoteTitleId = titleId;
				if (shellBgdlTitleHasTasksOfKind(m_promoteTitleId.c_str(), 2, 0))
					++bgdlTaskCount;
			}
			if (bgdlTaskCount == 0)
				return 0;
		} else if (isPsmRuntime) {
			const char *titleId = sceLsdbGetName(&m_appInfo);
			if (titleId != NULL) {
				m_promoteTitleId = titleId;
				bgdlTaskCount =
					shellBgdlTitleHasTasksOfKind(m_promoteTitleId.c_str(), 2, 0) ? 1 : 0;
			}
			if (bgdlTaskCount == 0)
				return 0;
		} else {
			if (!topmenuResolvePromoteTitleId(&m_appInfo, &m_promoteTitleId))
				return (SceInt32)0x80103900u;
			bgdlTaskCount = shellBgdlCountTitleTasks(m_promoteTitleId.c_str());
			if (g_topWindow->m_gameIf != NULL) {
				typedef int (*HasPendingInstallFn)(const char *titleId);
				gamePendingCount = ((HasPendingInstallFn)g_topWindow->m_gameIf->func[32])(
					m_promoteTitleId.c_str());
			}
			if ((bgdlTaskCount | gamePendingCount) == 0)
				return 0;
		}

		if (!topmenuCanPromoteApp(g_topWindow, 0, false, false))
			return (SceInt32)0x80103910u;

		if (g_topmenuInterface->beginAppInstallLock(m_promoteTitleId.c_str()) == 0)
			return (SceInt32)0x80103910u;

		/* Switch waiting dialog text to "Installing..." */
		paf::thread::RMutex::main_thread_mutex.Lock();
		if (m_waitDialogId >= 0) {
			SceCommonGuiDialog_BC311DC0(m_waitDialogId);
			m_waitDialogId = -1;
		}
		paf::Plugin *applauncherPlugin = paf::Plugin::Find("applauncher_plugin");
		CommonGuiLayoutDialogParam dialogParam;
		dialogParam.resourcePlugin = applauncherPlugin;
		dialogParam.priority = 34;
		dialogParam.layout = SceCommonGuiDialog_2A68B4A4;
		if (applauncherPlugin != NULL) {
			dialogParam.text = paf::wstring(
				applauncherPlugin->GetString(paf::IDParam(PROMOTE_MSG_INSTALLING_ID)));
		}
		m_waitDialogId = SceCommonGuiDialog_CCFCD9A6(&dialogParam);
		paf::thread::RMutex::main_thread_mutex.Unlock();

		m_didBeginPromote = true;

		/* Wait up to 15 seconds for the running title to exit before installing */
		for (int attempt = 0; attempt <= 30; ++attempt) {
			SceAppMgrAppStatus status;
			sce_paf_memset(&status, 0, sizeof(status));
			if (sceAppMgrGetStatusByName(m_promoteTitleId.c_str(), &status) != 0)
				break;
			paf::thread::Sleep(500);
		}

		BgdlTaskInfo failedTask;
		SceInt32 result = 0;

		if (isPspOrPs1) {
			if (bgdlTaskCount != 0) {
				const char *titlePtr = m_promoteTitleId.empty()
							       ? NULL
							       : m_promoteTitleId.c_str();
				result = shellBgdlInstallTitleTasksType1(titlePtr, &failedTask);
				if (result != 0)
					topmenuInstallErrorFill(result, &failedTask, &m_installError);
			}
			return result;
		}

		const char *targetTitleId = isPsm ? "PCSI00011" : m_promoteTitleId.c_str();
		if (bgdlTaskCount != 0) {
			result = shellBgdlInstallTitleTasks(
				targetTitleId, &failedTask, &m_hadPatchInstalled);
			if (result != 0) {
				topmenuInstallErrorFill(result, &failedTask, &m_installError);
				return result;
			}
			if (isPsm) {
				const char *psmTitleId = sceLsdbGetName(&m_appInfo);
				if (psmTitleId != NULL) {
					result = shellBgdlInstallTitleTasksType2(
						psmTitleId, &failedTask);
					if (result != 0) {
						topmenuInstallErrorFill(
							result, &failedTask, &m_installError);
						return result;
					}
				}
			}
		}

		if (gamePendingCount != 0) {
			if (g_topWindow->m_gameIf != NULL) {
				typedef int (*PromotePendingFn)(const char *titleId);
				result = ((PromotePendingFn)g_topWindow->m_gameIf->func[31])(
					targetTitleId);
			} else {
				result = (SceInt32)0x80103906u;
			}
		}
		return result;
	}

	/* 0x812869D8 */
	virtual void Finish(int32_t result)
	{
		SceCommonGuiDialog_BC311DC0(m_waitDialogId);
		m_waitDialogId = -1;

		topmenuInstallErrorShowDialog(&m_installError);
		if (m_doneCallback != NULL)
			m_doneCallback(result, m_userData);

		int finishMode = 0;
		if (result == 0 && m_didBeginPromote)
			finishMode = m_isLaunchPromote ? 2 : 1;
		g_topmenuInterface->endAppInstallLock(m_promoteTitleId.c_str(), finishMode);

		if (result == 0 && m_hadPatchInstalled)
			g_topmenuInterface->notifyUpStatusChanged(m_promoteTitleId.c_str(), 3);
	}

private:
	LsdbAppInfoPtr m_appInfo;				/* +0x1C */
	bool m_isLaunchPromote;					/* +0x24 */
	SceInt32 m_waitDialogId;				/* +0x28 */
	TopmenuPromoteDoneCallback m_doneCallback;		/* +0x2C */
	void *m_userData;					/* +0x30 */
	bool m_didBeginPromote;					/* +0x34 */
	paf::string m_promoteTitleId;				/* +0x38 */
	TopmenuInstallError m_installError;			/* +0x44 */
	bool m_hadPatchInstalled;				/* +0x88 */
};

typedef char AppDataPromoteJobSizeCheck[sizeof(AppDataPromoteJob) == 140 ? 1 : -1];

} /* namespace */

/* ------------------------------------------------------------------ */
/* TopTabBase (0x812845A8..0x81284BF0)                                */
/* ------------------------------------------------------------------ */

/* 0x812845A8 / 0x8128471E */
TopTabBase::TopTabBase(paf::Plugin *plugin, TopWindowBase *window,
		       const paf::IDParam &templateId,
		       const paf::IDParam &rootWidgetId)
	: m_widget(NULL),
	  m_plugin(plugin),
	  m_scrollView(window->m_scrollView),
	  m_inViewport(0),
	  m_prevInViewport(0),
	  m_viewportAlignPad(0),
	  m_lastViewPosX(0.0f),
	  m_lastViewPosY(0.0f),
	  m_index(-1),
	  m_tabKind(0),
	  m_hitShrinkX(0.0f),
	  m_hitShrinkY(0.0f),
	  m_pageKey(window->m_nextPageKey++)
{
	paf::ui::Widget *viewParent = m_scrollView->GetViewParent();
	paf::math::v4 viewPos = viewParent->GetPos(0, 0, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	m_lastViewPosX = -viewPos.extract_x();
	m_lastViewPosY = -viewPos.extract_y();

	plugin->TemplateOpen(viewParent, templateId, paf::Plugin::TemplateOpenParam());
	m_widget = viewParent->FindChild(rootWidgetId, 0);

	char nameBuf[64];
	sce_paf_snprintf(nameBuf, sizeof(nameBuf), "%p", (void *)this);
	m_widget->m_name = paf::IDParam(nameBuf);
}

/* 0x8128472E */
bool TopTabBase::IsVisibleInViewport(const paf::math::v4 *boundsSize,
				     paf::ui::Widget *overrideWidget)
{
	paf::ui::Widget *target = (overrideWidget != NULL) ? overrideWidget : m_widget;
	if ((target->m_update_flag & 7) != 0)
		target->UpdateMatrix();

	paf::ui::Scene *scene = m_scrollView->m_scene;
	if (scene == NULL || scene->m_camera == NULL)
		return false;

	paf::math::matrix viewProj = scene->m_camera->m_projection *
				     scene->m_camera->m_view;
	paf::math::matrix mvp = viewProj * target->m_world_matrix;

	float halfW = boundsSize->extract_x() * 0.5f;
	float halfH = boundsSize->extract_y() * 0.5f;

	paf::math::v4 corners[4];
	corners[0] = paf::math::v4(mvp.Transform(paf::math::v4(halfW, halfH, 0.0f, 1.0f).getImpl()));
	corners[1] = paf::math::v4(mvp.Transform(paf::math::v4(halfW, -halfH, 0.0f, 1.0f).getImpl()));
	corners[2] = paf::math::v4(mvp.Transform(paf::math::v4(-halfW, halfH, 0.0f, 1.0f).getImpl()));
	corners[3] = paf::math::v4(mvp.Transform(paf::math::v4(-halfW, -halfH, 0.0f, 1.0f).getImpl()));

	if (corners[0].extract_w() == 0.0f)
		return false;

	float minX = 1e30f;
	float maxX = -1e30f;
	float minY = 1e30f;
	float maxY = -1e30f;
	float minZ = 1e30f;
	float maxZ = -1e30f;

	for (int i = 0; i < 4; ++i) {
		float invW = 1.0f / corners[i].extract_w();
		float nx = corners[i].extract_x() * invW;
		float ny = corners[i].extract_y() * invW;
		float nz = corners[i].extract_z() * invW;
		if (nx < minX) minX = nx;
		if (nx > maxX) maxX = nx;
		if (ny < minY) minY = ny;
		if (ny > maxY) maxY = ny;
		if (nz < minZ) minZ = nz;
		if (nz > maxZ) maxZ = nz;
	}

	float absMinX = (fabsf(minX) < fabsf(maxX)) ? fabsf(minX) : fabsf(maxX);
	float absMinY = (fabsf(minY) < fabsf(maxY)) ? fabsf(minY) : fabsf(maxY);

	if ((minX * maxX < 0.0f || absMinX <= 1.00001f) &&
	    (minY * maxY < 0.0f || absMinY <= 1.00001f) &&
	    maxZ >= -1e-6f && minZ <= 1.000001f) {
		return true;
	}
	return false;
}

/* 0x81284A4C / 0x81284BD8 / 0x81284BE8 */
TopTabBase::~TopTabBase()
{
	DispatchEvent(0, (void *)(intptr_t)m_pageKey, 0);
	if (m_widget != NULL)
		m_widget->DestroyWidget();
}

/* 0x81284BF0 */
void TopTabBase::UpdateViewportVisibility()
{
	paf::ui::Widget *viewParent = m_scrollView->GetViewParent();
	paf::math::v4 viewSize = *viewParent->GetSize(0);
	paf::math::v4 tabSize = *m_widget->GetSize(0);
	paf::math::v4 viewPos = viewParent->GetPos(0, 0, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	paf::math::v2 viewportOrigin(-viewPos.extract_x(), -viewPos.extract_y());
	paf::math::v2 viewportExtent(viewSize.extract_x(), viewSize.extract_y());
	paf::math::rect viewportRect(viewportOrigin, viewportExtent);

	paf::math::v4 tabPos = m_widget->GetPos(0, 0, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	paf::math::v2 tabOrigin(tabPos.extract_x(), tabPos.extract_y());
	paf::math::v2 tabExtent(tabSize.extract_x() - m_hitShrinkX,
				tabSize.extract_y() - m_hitShrinkY);
	paf::math::rect tabRect(tabOrigin, tabExtent);

	bool fromAbove = viewportOrigin.extract_y() < m_lastViewPosY;
	m_lastViewPosX = viewportOrigin.extract_x();
	m_lastViewPosY = viewportOrigin.extract_y();
	m_prevInViewport = m_inViewport;

	bool nowVisible = viewportRect.hit(tabRect);
	m_inViewport = nowVisible ? 1 : 0;

	if (m_inViewport == m_prevInViewport)
		return;

	if (nowVisible) {
		OnEnterViewport(fromAbove);
		DispatchEvent(1, (void *)(intptr_t)m_pageKey, 0);
	} else {
		OnLeaveViewport(fromAbove);
		DispatchEvent(2, (void *)(intptr_t)m_pageKey, 0);
	}
}

/* ------------------------------------------------------------------ */
/* TopWindowBase (0x81284EB4..0x81286222)                             */
/* ------------------------------------------------------------------ */

/* 0x81284EB4 / 0x81284F5A */
TopWindowBase::TopWindowBase(paf::ui::Widget *rootWidget, SceInt32 layoutOrientation,
			     SceUInt32 initialCapacity, SceInt32 tabSpacing)
	: m_rootWidget(rootWidget),
	  m_scrollView(NULL),
	  m_layoutOrientation(layoutOrientation),
	  m_tabs(),
	  m_nextPageKey(0),
	  m_initialCapacity(initialCapacity),
	  m_tabSpacing(tabSpacing),
	  m_emitEvents(true)
{
	if (initialCapacity != 0)
		m_tabs.reserve(initialCapacity);
}

/* 0x81284F6A / 0x81285042 / 0x81285052 */
TopWindowBase::~TopWindowBase()
{
	while (!m_tabs.empty()) {
		TopTabBase *tab = m_tabs.back();
		m_tabs.pop_back();
		delete tab;
	}
}

/* 0x8128505C */
void TopWindowBase::LayoutVertical(int layoutMode)
{
	paf::math::v4 pos(0.0f, 0.0f, 0.0f, 0.0f);
	for (SceUInt32 i = 0; i < m_tabs.size(); ++i) {
		paf::ui::Widget *widget = m_tabs[i]->m_widget;
		if (layoutMode == 1) {
			widget->KillAnim(0x10002);
			widget->KillAnim(0x10000);
			showTabWidgetImmediate(widget);
			widget->SetPos(pos, NULL, 0x10000, 0.0f, 0, NULL, NULL);
		} else {
			widget->KillAnim(0x10000);
			paf::Timer *timer = createLayoutTimer(300.0f, paf::Timer::FUNC_EXPO_OUT);
			widget->SetPos(pos, timer, 0x10000, 0.0f, 0, NULL, NULL);
		}
		pos.set_y(pos.extract_y() - (float)m_tabSpacing);
	}
}

/* 0x812852BA */
void TopWindowBase::LayoutHorizontal(int layoutMode)
{
	paf::math::v4 pos(0.0f, 0.0f, 0.0f, 0.0f);
	for (SceUInt32 i = 0; i < m_tabs.size(); ++i) {
		paf::ui::Widget *widget = m_tabs[i]->m_widget;
		if (layoutMode == 1) {
			widget->KillAnim(0x10002);
			widget->KillAnim(0x10000);
			showTabWidgetImmediate(widget);
			widget->SetPos(pos, NULL, 0x10000, 0.0f, 0, NULL, NULL);
		} else {
			widget->KillAnim(0x10000);
			paf::Timer *timer = createLayoutTimer(300.0f, paf::Timer::FUNC_EXPO_OUT);
			widget->SetPos(pos, timer, 0x10000, 0.0f, 0, NULL, NULL);
		}
		pos.set_x(pos.extract_x() + (float)m_tabSpacing);
	}
}

/* 0x81285520 */
void TopWindowBase::UpdateTabLayout(int layoutMode)
{
	for (SceUInt32 i = 0; i < m_tabs.size(); ++i)
		m_tabs[i]->m_index = (SceInt32)i;

	if (m_layoutOrientation == 0)
		LayoutHorizontal(layoutMode);
	else if (m_layoutOrientation == 1)
		LayoutVertical(layoutMode);
	else if (m_layoutOrientation == 2)
		LayoutCustom(layoutMode);

	for (SceUInt32 i = 0; i < m_tabs.size(); ++i)
		m_tabs[i]->UpdateViewportVisibility();
}

/* 0x812855C0 */
bool TopWindowBase::InsertTabInternal(TopTabBase *tab, bool appendEnd, TopTabBase *afterTab)
{
	if (afterTab != NULL) {
		for (paf::vector<TopTabBase *>::iterator it = m_tabs.begin();
		     it != m_tabs.end(); ++it) {
			if (*it == afterTab) {
				++it;
				if (it == m_tabs.end())
					m_tabs.push_back(tab);
				else
					m_tabs.insert(it, tab);
				tab->m_widget->SetForward(afterTab->m_widget);
				return true;
			}
		}
	}

	TopTabBase *prevTab = NULL;
	if (m_tabs.empty() || appendEnd) {
		if (!m_tabs.empty())
			prevTab = m_tabs.back();
		m_tabs.push_back(tab);
	} else {
		prevTab = m_tabs[0];
		paf::vector<TopTabBase *>::iterator insertPos = m_tabs.begin();
		++insertPos;
		m_tabs.insert(insertPos, tab);
	}

	if (prevTab != NULL)
		tab->m_widget->SetForward(prevTab->m_widget);
	return true;
}

/* 0x812856FA */
void TopWindowBase::AddTab(TopTabBase *tab, bool appendEnd, TopTabBase *afterTab)
{
	if (!InsertTabInternal(tab, appendEnd, afterTab))
		return;
	UpdateTabLayout(1);
	if (m_emitEvents)
		DispatchEvent(0, this, tab->m_pageKey);
}

/* 0x812857E8 */
bool TopWindowBase::RemoveTabInternal(TopTabBase *tab, TopTabBase ***outNextIter)
{
	for (paf::vector<TopTabBase *>::iterator it = m_tabs.begin();
	     it != m_tabs.end(); ++it) {
		if (*it == tab) {
			paf::vector<TopTabBase *>::iterator nextIt = m_tabs.erase(it);
			if (outNextIter != NULL)
				*outNextIter = nextIt.m_ptr;
			return true;
		}
	}
	if (outNextIter != NULL)
		*outNextIter = m_tabs.end().m_ptr;
	return false;
}

/* 0x81285854 */
void TopWindowBase::RemoveTab(TopTabBase ***outNextIter, TopTabBase *tab)
{
	if (outNextIter != NULL)
		*outNextIter = NULL;
	if (!RemoveTabInternal(tab, outNextIter))
		return;
	UpdateTabLayout(2);
	if (m_emitEvents)
		DispatchEvent(1, this, tab->m_pageKey);
}

/* 0x8128594E */
void TopWindowBase::MoveTabAfter(TopTabBase *tab, TopTabBase *afterTab)
{
	TopTabBase **nextIter = NULL;
	if (RemoveTabInternal(tab, &nextIter) && InsertTabInternal(tab, false, afterTab))
		UpdateTabLayout(4);
}

/* 0x8128599E */
void TopWindowBase::AttachScrollView(paf::ui::ScrollView *scrollView)
{
	m_scrollView = scrollView;
	scrollView->AddEventCallback(0x10000002, onTopWindowScrollBegin, this);
	scrollView->AddEventCallback(0x10000001, onTopWindowScrollEnd, this);
}

/* 0x81285A3C */
TopTabBase *TopWindowBase::FindFocusedTab()
{
	if (m_tabs.empty())
		return NULL;

	paf::math::v2 focusPos = m_scrollView->GetViewFocus();
	float bestDist = 100000.0f;
	TopTabBase *bestTab = NULL;

	for (SceUInt32 i = 0; i < m_tabs.size(); ++i) {
		paf::math::v4 center = m_tabs[i]->m_widget->GetPosCenter(
			0, 0, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		float dx = center.extract_x() - focusPos.extract_x();
		float dy = center.extract_y() - focusPos.extract_y();
		float dist = sqrtf(dx * dx + dy * dy);
		if (dist <= bestDist) {
			bestDist = dist;
			bestTab = m_tabs[i];
		}
	}
	return bestTab;
}

/* 0x81285B60 */
TopTabBase *TopWindowBase::FindNearestTab()
{
	paf::ui::Widget *viewParent = m_scrollView->GetViewParent();
	paf::math::v4 viewPos = viewParent->GetPos(0, 0, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	float originX = -viewPos.extract_x();
	float originY = -viewPos.extract_y();

	float bestDist = 10000000.0f;
	TopTabBase *bestTab = NULL;

	for (SceUInt32 i = 0; i < m_tabs.size(); ++i) {
		paf::math::v4 tabPos = m_tabs[i]->m_widget->GetPos(
			0, 0, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		float dx = originX - tabPos.extract_x();
		float dy = originY - tabPos.extract_y();
		float dist = sqrtf(dx * dx + dy * dy);
		if (dist < bestDist) {
			bestDist = dist;
			bestTab = m_tabs[i];
		}
	}
	return bestTab;
}

/* 0x81285CCE */
TopTabBase *TopWindowBase::FindTabByPageKey(SceInt32 pageKey)
{
	if (pageKey == -1)
		return NULL;
	for (SceUInt32 i = 0; i < m_tabs.size(); ++i) {
		if (m_tabs[i]->m_pageKey == pageKey)
			return m_tabs[i];
	}
	return NULL;
}

/* 0x81285D04 */
TopTabBase *TopWindowBase::FindTabBySnapId(SceInt32 snapId)
{
	for (SceUInt32 i = 0; i < m_tabs.size(); ++i) {
		if (m_tabs[i]->m_scrollView->GetSnapID(m_tabs[i]->m_widget) == snapId)
			return m_tabs[i];
	}
	return NULL;
}

/* 0x81285D54 */
TopTabBase *TopWindowBase::GetNextTab(TopTabBase *tab)
{
	for (SceUInt32 i = 0; i < m_tabs.size(); ++i) {
		if (m_tabs[i] == tab) {
			if (i + 1 < m_tabs.size())
				return m_tabs[i + 1];
			break;
		}
	}
	return tab;
}

/* 0x81285D84 */
TopTabBase *TopWindowBase::GetPrevTab(TopTabBase *tab)
{
	for (SceUInt32 i = 0; i < m_tabs.size(); ++i) {
		if (m_tabs[i] == tab) {
			if (i > 0)
				return m_tabs[i - 1];
			break;
		}
	}
	return tab;
}

/* 0x81285DB6 */
SceInt32 TopWindowBase::GetTabSnapId(TopTabBase *tab)
{
	return m_scrollView->GetSnapID(tab->m_widget);
}

/* 0x81285DC2 */
void TopWindowBase::ScrollToTab(TopTabBase *tab, float durationSec)
{
	paf::ui::Widget *widget = tab->m_widget;
	SceInt32 snapId = m_scrollView->GetSnapID(widget);
	paf::math::v4 center = widget->GetPosCenter(0, 0, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	paf::math::v2 snapPos(center.extract_x(), center.extract_y());
	paf::ui::ScrollViewSnapPoint snapPoint(snapPos);
	m_scrollView->UpdateSnapPoint(snapId, snapPoint);

	if (durationSec > 0.0f) {
		paf::Timer *timer = createLayoutTimer(
			durationSec * 1000.0f, paf::Timer::FUNC_EXPO_OUT);
		m_scrollView->DoSnap(snapId, timer);
	} else {
		m_scrollView->DoSnap(snapId, NULL);
	}
}

/* 0x81285EC4 */
void TopWindowBase::ScrollToTabAnimated(TopTabBase *tab)
{
	paf::ui::Widget *widget = tab->m_widget;
	SceInt32 snapId = m_scrollView->GetSnapID(widget);
	paf::math::v4 center = widget->GetPosCenter(0, 0, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	paf::math::v2 snapPos(center.extract_x(), center.extract_y());
	paf::ui::ScrollViewSnapPoint snapPoint(snapPos);
	m_scrollView->UpdateSnapPoint(snapId, snapPoint);

	(void)m_scrollView->GetViewFocus();
	paf::Timer *timer = createLayoutTimer(300.0f, paf::Timer::FUNC_EXPO_OUT);
	m_scrollView->DoSnap(snapId, timer);
}

/* 0x81286218 */
void TopWindowBase::RefreshLayoutImmediate()
{
	UpdateTabLayout(3);
}

/* 0x81286222 */
void TopWindowBase::RefreshLayoutDefault()
{
	UpdateTabLayout(0);
}

/* 0x81286A4C */
bool topmenuStartAppDataPromote(const LsdbAppInfoPtr *appInfo,
				TopmenuPromoteDoneCallback callback,
				void *userData, bool isLaunchPromote)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new AppDataPromoteJob(*appInfo, callback, userData, isLaunchPromote));
	if (job.get() == NULL)
		return false;

	topmenuEnqueueInstallJob(&job);
	return true;
}
