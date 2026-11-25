/*
 * top_tab.cpp: TopTab implementation (individual LiveArea / Home tab widget
 * and header strip interaction, 0x81281F18-0x81282C3B).
 */
#include "topmenu_internal.h"
#include <appmgr.h>

namespace {

static const SceUInt32 TOP_TAB_TEMPLATE_ID = 0xDC3E1144u;	/* -599895164 */
static const SceUInt32 TOP_TAB_ROOT_WIDGET_ID = 0x2CE52BFFu;	/* 753273983 */
static const SceUInt32 TOP_TAB_CONTENT_ROOT_ID = 0x18411D34u;	/* 406920180 */
static const SceUInt32 TOP_TAB_HEADER_TEMPLATE_ID = 0x3E2B9185u;/* 1043097797 */
static const SceUInt32 TOP_TAB_HEADER_WIDGET_ID = 0xD991A73Bu;	/* -644788229 */
static const SceUInt32 TOP_TAB_TITLE_TEXT_ID = 0x6D543D6Eu;	/* 1834253806 */
static const SceUInt32 TOP_TAB_ICON_WIDGET_ID = 0x1A256DF4u;	/* 438663636 */

static void showWidgetImmediate(paf::ui::Widget *widget)
{
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xFF;
		widget->m_transition_complete = true;
		widget->DoTransition_core(1.0f, true);
	}
	ScePafWidget_FF5BC957(widget, NULL, 2, 0.0f, NULL);
}

static void hideWidgetImmediate(paf::ui::Widget *widget)
{
	widget->m_transition_complete = false;
	widget->m_transition = 0xFF;
	widget->DoTransitionReverse_core(1.0f, true);
	ScePafWidget_4588699E(widget, NULL, 2, 0.0f, NULL);
}

/* 0x81282AE8 */
static void onTabHeaderFocusHover(int32_t eventType, paf::ui::Handler *self,
				  paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)self;
	(void)event;
	topmenuSetHoveredTab((TopTab *)userData);
}

/* 0x81282AF2 */
static void onTabHeaderFocusIn(int32_t eventType, paf::ui::Handler *self,
			       paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)self;
	TopTab *tab = (TopTab *)userData;
	if ((event->m_type & 0xFFF0) == 0x400) {
		paf::ui::Environment *env = paf::Framework::Instance()->GetEnvironmentInstance();
		if (ScePafWidget_529FB877(env, -1) == 0) {
			topmenuFocusHoveredTab(tab);
			return;
		}
		g_topmenuHeaderFocusByPad = true;
	} else {
		g_topmenuHeaderFocusByPad = false;
	}
	topmenuSetHoveredTab(tab);
}

/* 0x81282B42 */
static void onTabHeaderSelect(int32_t eventType, paf::ui::Handler *self,
			      paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)self;
	TopTab *tab = (TopTab *)userData;
	if ((SceUInt16)event->m_type != 17 || (SceUInt8)event->m_detail == 27) {
		if (tab->m_tabMode == 3)
			topmenuSetHoveredTab(NULL);
		else
			topmenuSelectHeaderTab(tab);
	}
}

/* 0x81282B6A */
static void onTabHeaderFocusOut(int32_t eventType, paf::ui::Handler *self,
				paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)self;
	TopTab *tab = (TopTab *)userData;
	if ((event->m_type & 0xFFF0) == 0x400) {
		paf::ui::Environment *env = paf::Framework::Instance()->GetEnvironmentInstance();
		if (ScePafWidget_529FB877(env, -1) == 0) {
			topmenuFocusHoveredTab(tab);
			return;
		}
	}
	if (g_topWindow != NULL && tab == g_topWindow->m_hoveredTab)
		topmenuSetHoveredTab(NULL);
}

} /* namespace */

bool g_topmenuHeaderFocusByPad;
SceInt32 g_topmenuNextTabOrderStamp;

/* 0x81281F18 */
bool TopTab::CanClose() const
{
	if (m_lockedByApp)
		return false;
	if (m_appData == NULL)
		return true;

	SceInt32 appId = m_appData->m_process.m_appId;
	if (appId != -1) {
		SceAppMgrAppStatus status;
		if (sceAppMgrGetStatusByAppId(appId, &status) == 0 && status.protectionMode != 0)
			return false;
	}

	const char *titleId = topmenuAppProcessGetTitleId(&m_appData->m_process);
	return titleId == NULL || !topmenuHasRunningAppWithTitleId(g_topWindow, titleId);
}

/* 0x81281F8C */
void TopTab::SetIconSurface(const paf::intrusive_ptr<paf::graph::Surface> &surface)
{
	paf::ui::Widget *iconWidget =
		m_headerWidget->FindChild(paf::IDParam(TOP_TAB_ICON_WIDGET_ID), 0);
	if (iconWidget != NULL) {
		showWidgetImmediate(iconWidget);
		iconWidget->SetTexture(surface);
	}
	m_iconSurface = surface;
}

/* 0x81282086 */
void TopTab::SetIconCache(const paf::intrusive_ptr<paf::cache::CacheImageInterface> &imageCache)
{
	paf::ui::Widget *iconWidget =
		m_headerWidget->FindChild(paf::IDParam(TOP_TAB_ICON_WIDGET_ID), 0);
	if (iconWidget != NULL) {
		showWidgetImmediate(iconWidget);
		iconWidget->SetDrawObjMaskBit(&imageCache, 0);
	}
	m_iconCache = imageCache;
}

/* 0x81282186 */
void TopTab::SetTitleText(const paf::wstring &title)
{
	m_title = title;
	m_titleText->SetString(title);
	m_titleText->Invalidate();
}

/* 0x812821A8 */
void TopTab::HideIcon()
{
	paf::ui::Widget *iconWidget =
		m_headerWidget->FindChild(paf::IDParam(TOP_TAB_ICON_WIDGET_ID), 0);
	if (iconWidget != NULL)
		hideWidgetImmediate(iconWidget);
}

/* 0x81282274 */
void TopTab::RefreshOrderStamp()
{
	m_orderStamp = g_topmenuNextTabOrderStamp++;
}

/* 0x81282286 */
void TopTab::InitHeaderWidgets(paf::Plugin *plugin)
{
	plugin->TemplateOpen(m_widget, paf::IDParam(TOP_TAB_HEADER_TEMPLATE_ID),
			     paf::Plugin::TemplateOpenParam());
	m_headerWidget = m_widget->FindChild(paf::IDParam(TOP_TAB_HEADER_WIDGET_ID), 0);
	m_headerWidget->SetAdjust(1, 1, 1);
	m_headerWidget->m_hit_find_mode = 0;
	m_headerWidget->m_hit_detect_mode = 0;
	m_headerWidget->m_hit_detect_b_mode = 0;
	m_headerWidget->m_event_enable |= 0x40u;

	m_titleText = (paf::ui::Text *)m_headerWidget->FindChild(
		paf::IDParam(TOP_TAB_TITLE_TEXT_ID), 0);

	if (g_topWindow->m_tabs.size() == 0) {
		paf::ui::Widget *iconWidget =
			m_headerWidget->FindChild(paf::IDParam(TOP_TAB_ICON_WIDGET_ID), 0);
		paf::math::v4 iconPos = iconWidget->GetPos(0, 0, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		float shiftX = (960.0f - 840.0f) * 0.5f;
		iconPos.set_x(iconPos.extract_x() - shiftX);
		iconWidget->SetPos(iconPos, NULL, 0x10000, 0.0f, 0, NULL, NULL);

		paf::math::v4 titlePos = m_titleText->GetPos(0, 0, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		titlePos.set_x(titlePos.extract_x() - shiftX);
		m_titleText->SetPos(titlePos, NULL, 0x10000, 0.0f, 0, NULL, NULL);
	}

	paf::ui::Widget *iconWidget =
		m_headerWidget->FindChild(paf::IDParam(TOP_TAB_ICON_WIDGET_ID), 0);
	iconWidget->EnableFocusEvent(true);
	iconWidget->m_event_enable |= 0x10u;

	iconWidget->AddEventCallback(0x40001, onTabHeaderFocusIn, this);
	iconWidget->AddEventCallback(0x40002, onTabHeaderFocusOut, this);
	iconWidget->AddEventCallback(0x10001, onTabHeaderSelect, this);

	hideWidgetImmediate(m_headerWidget);

	m_headerWidget->AddEventCallback(0x1000E, onTabHeaderFocusHover, this);
	m_headerWidget->AddEventCallback(0x1000B, onTabHeaderFocusIn, this);
	m_headerWidget->AddEventCallback(0x1000C, onTabHeaderSelect, this);
	m_headerWidget->AddEventCallback(0x1000F, onTabHeaderFocusOut, this);
}

/* 0x812827DE */
void TopTab::SetActiveTab(bool active)
{
	if (active == m_isActiveTab)
		return;

	m_isActiveTab = active;
	if (!active) {
		m_contentRoot->m_hit_find_mode = 0;
		m_contentRoot->m_hit_detect_b_mode = 0;
		m_contentRoot->m_hit_detect_mode = 0;
		if (m_appData != NULL)
			m_appData->OnDeactivateTab();
	} else {
		m_contentRoot->m_hit_detect_b_mode = 3;
		m_contentRoot->m_hit_detect_mode = 3;
		m_contentRoot->m_hit_find_mode = 1;
		if (m_appData != NULL)
			m_appData->OnActivateTab();
	}
}

/* 0x81282826 */
TopTab::TopTab(paf::Plugin *plugin, bool isHomeTab)
	: TopTabBase(plugin, g_topWindow, paf::IDParam(TOP_TAB_TEMPLATE_ID),
		     paf::IDParam(TOP_TAB_ROOT_WIDGET_ID)),
	  m_lockedByApp(false),
	  m_contentRoot(NULL),
	  m_headerWidget(NULL),
	  m_titleText(NULL),
	  m_iconSurface(),
	  m_iconCache(),
	  m_tabMode(0),
	  m_appData(NULL),
	  m_poolSlot(NULL),
	  m_title(),
	  m_isActiveTab(true),
	  m_orderStamp(0),
	  m_isClosing(false),
	  m_hoverProgress(0.0f)
{
	m_contentRoot = m_widget->FindChild(paf::IDParam(TOP_TAB_CONTENT_ROOT_ID), 0);
	if (!isHomeTab)
		m_poolSlot = topmenuAcquireSurfacePool(false);
	m_orderStamp = g_topmenuNextTabOrderStamp++;

	if (isHomeTab) {
		paf::math::v4 tabSize = *m_widget->GetSize(0);
		tabSize.set_x(960.0f);
		m_widget->SetSize(tabSize, NULL, 0x10004, 0.0f, 0, NULL, NULL);
	}

	SetActiveTab(false);
	InitHeaderWidgets(plugin);
}

/* 0x81282A66 / 0x81282AD8 */
TopTab::~TopTab()
{
	topmenuOnTabDestroyed(g_topWindow, this);
	if (m_appData != NULL)
		delete m_appData;
	(void)paf::Framework::Instance();
	topmenuReleaseSurfacePool(m_poolSlot);
}

/* 0x81282BB2 */
void TopTab::OnEnterViewport(bool fromAbove)
{
	(void)fromAbove;
	if (m_appData != NULL)
		m_appData->OnEnterViewport();
}

/* 0x81282BBE */
void TopTab::OnLeaveViewport(bool fromAbove)
{
	(void)fromAbove;
	if (m_appData != NULL)
		m_appData->OnLeaveViewport();
}
