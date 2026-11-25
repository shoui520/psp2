/*
 * power_manage_keylock.cpp: peel-away keylock screen with clock, notifications, and theme support.
 * 3.65-CEX: 0x8123B524 - 0x8123FF58, 0x8142961C - 0x8142A0B8
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <std/string.h>
#include <wchar.h>
#include <std/string>
#include <std/vector>
#include <av_config/av_config.h>
#include <kernel/processmgr.h>
#include <regmgr/regmgr.h>
#include <vshbridge/vshbridge.h>
#include <framework.h>
#include <plugin.h>
#include <common/job_slots.h>
#include <common/main_thread_call_list.h>
#include <common/property_list.h>
#include <common/transition.h>
#include <datetime/datetime.h>
#include <graphics/drawobj/page_turn_obj.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/surface/surface.h>
#include <inputdevice/inputdevice.h>
#include <job/job.h>
#include <math/math.h>
#include <rtc/rtc.h>
#include <system/system.h>
#include <timer/timer.h>
#include <widget/w_list_item.h>
#include <widget/w_list_view.h>
#include <widget/w_pageturn.h>
#include <widget/w_plane.h>
#include <widget/w_supplier.h>
#include <widget/w_text.h>

#include "shell.h"
#include "plugins.h"
#include "theme.h"
#include "src/indicator_plugin/src/indicator.h"
#include "src/indicator_plugin/src/indicator_interface.h"
#include "src/livespace_db/livespace_db_internal.h"
#include "src/livespace_db/lsdb_keys.h"
#include "power_manage_internal.h"

extern "C" {
extern int sceAppMgrRestoreBgmSettingForShell(void);
extern int vshCtrlSetIdleCancelKey(SceUInt32 keyA, SceUInt32 keyB,
				   SceUInt32 keyC, SceUInt32 keyD);
extern int vshCtrlSetIdleCancelThreshold(int thresholdX, int thresholdY);
}

namespace power_manage {

KeyLockScreen *g_keyLockScreen = NULL;

static paf::common::JobSlots s_eventJobSlots;
static bool s_newEventAccessorOpened = false;
static int s_clockFrameCounter = 0;

static const SceUInt32 kKeyLockPageHash = 0xA3C0D1D7U;		/* -1547656425 */
static const SceUInt32 kPageTurnBackTexHash = 0xCA711AF6U;	/* -898599114 */
static const SceUInt32 kKeyLockTemplateHash = 0xF5227577U;	/* -182286473 */
static const SceUInt32 kKeyLockGuideHash = 0xCDB836A9U;		/* -843580823 */
static const SceUInt32 kKeyLockClockRootHash = 0xDE1F4EC8U;	/* -568366104 */
static const SceUInt32 kClockColonHash = 0xFD7D06C8U;		/* -42137208 */
static const SceUInt32 kClockDateHash = 0xC869BE10U;		/* -932631184 */
static const SceUInt32 kClockHourHash = 0xC88F9462U;		/* -930151454 */
static const SceUInt32 kClockMinuteHash = 0x162AEC9FU;		/* 371903903 */
static const SceUInt32 kClockHourFlipHash = 0x30FEA978U;	/* 822011208 */
static const SceUInt32 kClockMinuteFlipHash = 0x20994494U;	/* 546901332 */
static const SceUInt32 kClockAmPmHash = 0xAD9966EEU;		/* -1382409234 */
static const SceUInt32 kEventPanelHash = 0xF8423FCDU;		/* -129877107 */
static const SceUInt32 kEventBoxHash = 0x162D494EU;		/* 372058702 */
static const SceUInt32 kEventListViewHash = 0xAA71C149U;	/* -1435420023 */
static const SceUInt32 kEventItemTemplateHash = 0x1124A9E6U;	/* 287619494 */
static const SceUInt32 kEventItemRootHash = 0x63676C6EU;	/* 1667731438 */
static const SceUInt32 kEventItemBgHash = 0xD55598B7U;		/* -715815113 */
static const SceUInt32 kEventItemBorderHash = 0xA9FD32DCU;	/* -1443058724 */
static const SceUInt32 kEventItemTexSingleHash = 0x2765B8FDU;	/* 660991229 */
static const SceUInt32 kEventItemTexTopHash = 0x3983249EU;	/* 964883326 */
static const SceUInt32 kEventItemTexBottomHash = 0x75AEAD9EU;	/* 1974375838 */
static const SceUInt32 kCommonBlankTexHash = 0x65FEA4F3U;	/* 1711163699 */

static const SceInt32 kClockTimerId = 0x02000100;		/* 33554688 */
static const SceInt32 kUnlockCheckTimerId = 0x02000101;		/* 33554689 */
static const SceInt32 kUnlockCompleteTimerId = 0x02000102;	/* 33554690 */

static const SceUInt32 kEventDragStart = 0x10000002U;
static const SceUInt32 kEventPageTurnDone = 0x10000003U;
static const SceUInt32 kEventDragCancel = 0x10000005U;

static const SceUInt32 kMaxVisibleEvents = 3;
static const SceUInt32 kDefaultNoticeFontColor = 0xFFFAFAF5U;	/* -328966 */
static const SceUInt32 kInputOwnershipMask = 0x6FU;
static const int kTopmenuSlotUnlockComplete = 75;		/* +0x12C (300) */
static const int kKeyLockPagePriority = 27;

static paf::math::v4 rgbaFromPacked(SceUInt32 packed)
{
	const float scale = 1.0f / 255.0f;
	float red = (float)(packed & 0xFFU) * scale;
	float green = (float)((packed >> 8) & 0xFFU) * scale;
	float blue = (float)((packed >> 16) & 0xFFU) * scale;
	float alpha = (float)((packed >> 24) & 0xFFU) * scale;
	return paf::math::v4(red, green, blue, alpha);
}

template <typename WidgetType>
static WidgetType *findTypedWidget(paf::ui::Widget *parent, SceUInt32 hash)
{
	paf::ui::Widget *child = parent->FindChild(paf::IDParam(hash), 0);
	if (child != NULL && child->IsInherit(WidgetType::TypeName()))
		return (WidgetType *)child;
	return NULL;
}

static void keyLockKeyNoop(void *userData)
{
	(void)userData;
}

static void setPageTurnMargin(paf::ui::PageTurn *pageTurn, const paf::math::v2 &margin)
{
	if (pageTurn->m_turn_obj == NULL)
		return;
	pageTurn->m_turn_obj->m_margin = margin;
	if (pageTurn->m_plane != NULL) {
		bool visible = (margin.extract_x() != 0.0f && margin.extract_y() != 0.0f);
		pageTurn->m_plane->SetVisible(visible);
	}
}

static void setPageTurnRendererParam(paf::ui::PageTurn *pageTurn, const paf::math::v2 &param)
{
	if (pageTurn->m_turn_obj != NULL && pageTurn->m_turn_obj->m_param != NULL)
		pageTurn->m_turn_obj->m_param->m_renderer_param = param;
}

static int findMatchingEvent(const LsdbNewEvent *candidate,
			     const paf::vector<LsdbNewEventPtr> &list)
{
	int count = (int)list.size();
	for (int index = 0; index < count; ++index) {
		const LsdbNewEvent *existing = list[index].get();
		bool sameKey = false;
		if (candidate->rowid != 0 && existing->rowid != 0 &&
		    candidate->rowid == existing->rowid) {
			sameKey = true;
		} else if (!candidate->titleId.empty() && !candidate->itemId.empty() &&
			   candidate->titleId == existing->titleId &&
			   candidate->itemId == existing->itemId) {
			sameKey = true;
		}
		if (sameKey) {
			if (candidate->timeUpdated == existing->timeUpdated &&
			    candidate->msgType == existing->msgType) {
				return -1;
			}
			return index;
		}
	}
	return count;
}

/* --- EventListJob --- */

typedef void (*EventListJobCallback)(int result, const paf::vector<LsdbNewEventPtr> *events);

class EventListJob : public paf::common::SlotJobItem {
public:
	EventListJob(int slotIndex, EventListJobCallback callback, void *userData)
		: paf::common::SlotJobItem("EventListJob", NULL),
		  m_slotIndex(slotIndex),
		  m_callback(callback),
		  m_userData(userData)
	{
	}

	virtual ~EventListJob()
	{
	}

	virtual int Run()
	{
		IndicatorNewEventDb *db = indicatorNewEventDb();
		if (db == NULL)
			return -1;
		int totalCount = 0;
		int newCount = 0;
		return db->GetEvents(&m_events, 32, &totalCount, &newCount, false);
	}

	virtual void Finish(int result)
	{
		if (m_callback != NULL && !IsCanceled())
			m_callback(result, &m_events);
		paf::common::SlotJobItem::Finish(result);
	}

	int m_slotIndex;
	EventListJobCallback m_callback;
	void *m_userData;
	paf::vector<LsdbNewEventPtr> m_events;
};

static paf::common::SharedPtr<paf::job::JobItem>
enqueueEventListJob(int slotIndex, EventListJobCallback callback, void *userData)
{
	paf::common::SharedPtr<paf::job::JobItem> item(
		new EventListJob(slotIndex, callback, userData));
	s_eventJobSlots.Enqueue((uint32_t)slotIndex, paf::job::JobQueue::default_queue, item);
	return item;
}

/* --- KeyLockEventLayouter & ItemFactory --- */

static int32_t onLayoutEventList(paf::ui::Layouter *layouter, paf::ui::Widget *widget,
				 void *unusedA, void *unusedB, void *unusedC)
{
	(void)unusedA;
	(void)unusedB;
	(void)unusedC;
	if (widget == NULL || layouter == NULL || !layouter->IsDirty(0xFF) ||
	    !widget->IsInherit(paf::ui::ListView::TypeName())) {
		return 0;
	}

	paf::ui::ListView *listView = (paf::ui::ListView *)widget;
	paf::math::v4 cellSize = listView->GetCellSizeDefault(0);
	uint32_t cellCount = listView->GetCellNum(0);
	if (cellCount != 0) {
		uint32_t lastIndex = cellCount - 1;
		for (uint32_t idx = 0; idx < cellCount; ++idx) {
			paf::ui::Widget *itemWidget =
				listView->GetListItem(0, (int32_t)idx);
			if (itemWidget == NULL)
				continue;
			paf::ui::Widget *bgWidget =
				itemWidget->FindChild(paf::IDParam(kEventItemBgHash), 0);
			paf::ui::Widget *borderWidget = itemWidget->FindChild(
				paf::IDParam(kEventItemBorderHash), 0);
			if (bgWidget == NULL || borderWidget == NULL)
				continue;

			SceUInt32 texHash = 0;
			if (idx == 0) {
				if (cellCount == 1) {
					texHash = kEventItemTexSingleHash;
					borderWidget->Hide(0xFF, NULL, NULL, 0.0f);
				} else {
					texHash = kEventItemTexTopHash;
					borderWidget->Show(0xFF, NULL, NULL, 0.0f);
				}
			} else if (idx == lastIndex) {
				texHash = kEventItemTexBottomHash;
				borderWidget->Hide(0xFF, NULL, NULL, 0.0f);
			} else {
				texHash = 0;
				borderWidget->Show(0xFF, NULL, NULL, 0.0f);
			}

			paf::intrusive_ptr<paf::graph::Surface> tex =
				g_keyLockScreen->m_plugin->GetTexture(
					paf::IDParam(texHash));
			bgWidget->SetTexture(tex, 0, 0);
		}

		paf::math::v4 panelSize(cellSize.extract_x() + 4.0f,
					cellSize.extract_y() * (float)cellCount + 2.0f,
					0.0f, 0.0f);
		paf::ui::Widget *parent = listView->m_parent;
		if (parent != NULL && parent->IsInherit(paf::ui::Plane::TypeName())) {
			parent->SetSize(panelSize);
			if (parent->FindAnim(paf::ui::Widget::ANIM_SCALE) == NULL) {
				paf::math::v4 fullScale(1.0f, 1.0f, 1.0f, 0.0f);
				parent->SetScale(fullScale, new paf::Timer(150.0f));
			}
		}
	}
	return 0;
}

class KeyLockEventItemFactory : public paf::ui::listview::ItemFactory {
public:
	KeyLockEventItemFactory(paf::Plugin *plugin, KeyLockScreen *screen)
		: m_plugin(plugin), m_screen(screen)
	{
	}

	virtual ~KeyLockEventItemFactory()
	{
	}

	virtual paf::ui::ListItem *Create(CreateParam &param)
	{
		paf::Plugin::TemplateOpenParam tplParam;
		if (m_plugin->TemplateOpen(param.m_parent, paf::IDParam(kEventItemTemplateHash),
					   tplParam) != 0) {
			return NULL;
		}

		paf::ui::Widget *itemWidget =
			param.m_parent->FindChild(paf::IDParam(kEventItemRootHash), 0);
		if (itemWidget == NULL)
			return NULL;

		paf::common::SharedPtr<LsdbThemeInfo> theme = m_screen->m_themeInfo;
		if (theme.get() != NULL) {
			paf::ui::Widget *bgWidget =
				itemWidget->FindChild(paf::IDParam(kEventItemBgHash), 0);
			if (bgWidget != NULL) {
				bgWidget->SetColor(
					rgbaFromPacked((SceUInt32)theme->m_startScreenProperty
							       .m_notifyBgColor));
			}
			paf::ui::Widget *borderWidget =
				itemWidget->FindChild(paf::IDParam(kEventItemBorderHash), 0);
			if (borderWidget != NULL) {
				borderWidget->SetColor(
					rgbaFromPacked((SceUInt32)theme->m_startScreenProperty
							       .m_notifyBorderColor));
			}
		}

		itemWidget->m_name.SetID(paf::string(""));
		paf::math::v4 cellSize = param.m_list_view->GetCellSizeDefault(param.m_segment);
		itemWidget->SetSize(cellSize);

		paf::Plugin *indicatorPlugin = paf::Plugin::Find("indicator_plugin");
		if (indicatorPlugin != NULL && param.m_segment == 0) {
			IndicatorPluginInterface *indicatorIface =
				(IndicatorPluginInterface *)indicatorPlugin->GetInterface(1);
			if (indicatorIface != NULL) {
				LsdbNewEvent eventCopy;
				if (m_screen->GetEventAt(param.m_cell, &eventCopy) == 0) {
					IndicatorPushNewEventParam pushParam;
					pushParam.event = &eventCopy;
					pushParam.widget = itemWidget;
					pushParam.item = NULL;
					pushParam.color = (theme.get() != NULL)
						? (SceUInt32)theme->m_startScreenProperty
							  .m_notifyFontColor
						: kDefaultNoticeFontColor;
					indicatorIface->PushNewEvent(&pushParam);

					paf::ui::ListItem *listItem =
						(paf::ui::ListItem *)itemWidget;
					paf::common::property_list::PropertyValue propVal;
					propVal.m_pointer = (void *)pushParam.item;
					listItem->m_property_list.SetValue(
						paf::IDParam("event_item_ptr"), propVal);
				}
			}
		}
		return (paf::ui::ListItem *)itemWidget;
	}

	virtual void Stop(StopParam &param)
	{
		paf::Plugin *indicatorPlugin = paf::Plugin::Find("indicator_plugin");
		if (indicatorPlugin == NULL)
			return;
		IndicatorPluginInterface *indicatorIface =
			(IndicatorPluginInterface *)indicatorPlugin->GetInterface(1);
		if (indicatorIface == NULL)
			return;

		paf::common::property_list::PropertyValue propVal =
			param.m_item->m_property_list.GetValue(paf::IDParam("event_item_ptr"));
		IndicatorEventItem *eventItem = (IndicatorEventItem *)propVal.m_pointer;
		indicatorIface->DeleteObject(&eventItem);
	}

	paf::Plugin *m_plugin;
	KeyLockScreen *m_screen;
};

/* --- KeyLockScreen --- */

KeyLockScreen::KeyLockScreen(paf::Plugin *plugin)
	: m_plugin(plugin),
	  m_page(NULL),
	  m_clockRoot(NULL),
	  m_dateText(NULL),
	  m_hourText(NULL),
	  m_minuteText(NULL),
	  m_eventPanel(NULL),
	  m_guideWidget(NULL),
	  m_pageTurn(NULL),
	  m_touchSupplier(NULL),
	  m_wallpaperReady(false),
	  m_isDragging(false),
	  m_pendingDisplayOn(false),
	  m_skipPasscode(false),
	  m_themeTestVisible(false),
	  m_themeInfo(*shellThemeGetInfo()),
	  m_timeFormat(0),
	  m_lastDateTime(0, 0, 0, 0, 0, 0, 0),
	  m_lockStartTick(0)
{
	g_keyLockScreen = this;

	int wallpaperMode = 0;
	if (!shellThemeIconsEnabled() &&
	    sceRegMgrGetKeyInt("/CONFIG/THEME", "wallpaper", &wallpaperMode) == 0 &&
	    wallpaperMode != 2) {
		m_themeInfo.reset();
	}

	vshCtrlSetIdleCancelKey(3211264, 3211264, 0, 0);
	vshCtrlSetIdleCancelThreshold(128, 128);
	KeyLockBlankPage::Open();

	paf::Plugin::PageOpenParam openParam;
	openParam.overwrite_draw_priority = kKeyLockPagePriority;
	m_page = plugin->PageOpen(paf::IDParam(kKeyLockPageHash), openParam);

	paf::intrusive_ptr<paf::graph::Surface> backTex =
		m_plugin->GetTexture(paf::IDParam(kPageTurnBackTexHash));
	paf::graph::PageTurnObj::SetBgTexture(backTex);

	m_pageTurn = new paf::ui::PageTurn(m_page, NULL);
	ScePafWidget_0253DC9C(m_pageTurn, true);
	ScePafWidget_A1193AAB(m_pageTurn, true, false);
	m_pageTurn->SetPos(paf::math::v4(0.0f, -16.0f, 0.0f, 0.0f));
	m_pageTurn->SetSize(paf::math::v4(960.0f, 512.0f, 0.0f, 0.0f));
	m_pageTurn->ScePafWidget_F378C87D(0.23f, 0.85f);
	setPageTurnMargin(m_pageTurn, paf::math::v2(0.0f, 0.0f));
	setPageTurnRendererParam(m_pageTurn, paf::math::v2(0.08f, 0.08f));
	m_pageTurn->SetColor(paf::math::v4(0.0f, 0.0f, 0.0f, 1.0f));

	paf::Plugin::TemplateOpenParam tplParam;
	plugin->TemplateOpen(m_page, paf::IDParam(kKeyLockTemplateHash), tplParam);
	m_guideWidget = m_page->FindChild(paf::IDParam(kKeyLockGuideHash), 0);

	paf::common::MainThreadCallList::Register(OnMainThreadTick, NULL);

	m_clockRoot = m_page->FindChild(paf::IDParam(kKeyLockClockRootHash), 0);
	if (m_clockRoot != NULL) {
		paf::ui::Text *colonText = findTypedWidget<paf::ui::Text>(m_clockRoot,
									  kClockColonHash);
		if (colonText != NULL)
			colonText->SetString(paf::wstring(L":"));

		m_clockRoot->SetParent(m_clockRoot->m_parent, NULL, 6);
		m_clockRoot->SetTimerListener(kClockTimerId, 1000.0f,
					      new paf::ui::EventCBListener(OnClockTimer, NULL));
		SceShell_7FE4E732(13, OnShellEvent, NULL);
		SceShell_7FE4E732(14, OnShellEvent, NULL);
		m_clockRoot->Hide(0xFF, NULL, NULL, 0.0f);

		m_dateText = findTypedWidget<paf::ui::Text>(m_clockRoot, kClockDateHash);
		m_hourText = findTypedWidget<paf::ui::Text>(m_clockRoot, kClockHourHash);
		m_minuteText = findTypedWidget<paf::ui::Text>(m_clockRoot, kClockMinuteHash);
		sceRegMgrGetKeyInt("/CONFIG/DATE", "time_format", &m_timeFormat);
	}

	m_eventPanel = m_page->FindChild(paf::IDParam(kEventPanelHash), 0);
	if (m_eventPanel != NULL) {
		paf::ui::Widget *eventBox =
			m_eventPanel->FindChild(paf::IDParam(kEventBoxHash), 0);
		if (eventBox != NULL) {
			paf::ui::ListView *listView =
				findTypedWidget<paf::ui::ListView>(eventBox, kEventListViewHash);
			if (listView != NULL) {
				listView->SetPos(paf::math::v4(0.0f, -1.0f, 0.0f, 0.0f));
				listView->EnableTouchFrontEvent(false);
				listView->Clear();
				listView->SetScrollType(paf::ui::ListView::SCROLL_TYPE_VERTICAL);
				listView->SetGenerateMargin(paf::math::v2(80.0f, 80.0f));
				listView->SetItemFactory(
					new KeyLockEventItemFactory(m_plugin, this));
				listView->InsertSegment(0, 1);
				listView->SetCellSizeDefault(
					0, paf::math::v4(694.0f, 76.0f, 0.0f, 0.0f));
				listView->DeleteCell(0, 0, listView->GetCellNum(0));
				if (listView->m_parent != NULL) {
					listView->m_parent->SetSize(
						paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
				}
				paf::rtc::GetCurrentTick(&m_lockStartTick);
				m_events.clear();
				listView->SetLayouter(
					new paf::ui::Layouter(onLayoutEventList, NULL));
			}
			eventBox->SetScale(paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		}
		m_eventPanel->SetParent(m_eventPanel->m_parent, NULL, 6);
		enqueueEventListJob(0, OnEventListLoaded, NULL);
	}

	m_touchSupplier = new paf::ui::Supplier(m_page, NULL);
	paf::math::v4 pageTurnSize = *m_pageTurn->GetSize(0);
	paf::math::v4 supplierSize(192.0f, 180.0f, 0.0f, 0.0f);
	m_touchSupplier->ScePafWidget_2EBD646E(m_pageTurn, true);
	m_touchSupplier->m_follow_pos = true;
	m_touchSupplier->m_pos_offset = (pageTurnSize * 0.5f) - (supplierSize * 0.5f);
	m_touchSupplier->SetSize(supplierSize);
	m_touchSupplier->SetMetaAlpha(0.0f);
	m_touchSupplier->SetParent(m_touchSupplier->m_parent, NULL, 6);

	m_pageTurn->AddEventListener(kEventPageTurnDone,
				     new paf::ui::EventCBListener(OnPageTurnCompleted, NULL),
				     false);
	m_pageTurn->AddEventListener(kEventDragStart,
				     new paf::ui::EventCBListener(OnDragStart, NULL), false);
	m_pageTurn->AddEventListener(kEventDragCancel,
				     new paf::ui::EventCBListener(OnDragCancel, NULL), false);

	if (m_themeInfo.get() != NULL) {
		paf::math::v4 dateColor =
			rgbaFromPacked((SceUInt32)m_themeInfo->m_startScreenProperty.m_dateColor);
		if (m_dateText != NULL)
			m_dateText->SetStyleAttribute(paf::graph::TextStyleAttribute_Color,
						      0, 0, dateColor);
		if (m_hourText != NULL)
			m_hourText->SetStyleAttribute(paf::graph::TextStyleAttribute_Color,
						      0, 0, dateColor);
		if (m_minuteText != NULL)
			m_minuteText->SetStyleAttribute(paf::graph::TextStyleAttribute_Color,
							0, 0, dateColor);

		paf::ui::Text *hourFlip =
			findTypedWidget<paf::ui::Text>(m_clockRoot, kClockHourFlipHash);
		if (hourFlip != NULL)
			hourFlip->SetStyleAttribute(paf::graph::TextStyleAttribute_Color,
						    0, 0, dateColor);
		paf::ui::Text *minuteFlip =
			findTypedWidget<paf::ui::Text>(m_clockRoot, kClockMinuteFlipHash);
		if (minuteFlip != NULL)
			minuteFlip->SetStyleAttribute(paf::graph::TextStyleAttribute_Color,
						      0, 0, dateColor);
		paf::ui::Text *colonText =
			findTypedWidget<paf::ui::Text>(m_clockRoot, kClockColonHash);
		if (colonText != NULL)
			colonText->SetStyleAttribute(paf::graph::TextStyleAttribute_Color,
						     0, 0, dateColor);
		paf::ui::Text *amPmText =
			findTypedWidget<paf::ui::Text>(m_clockRoot, kClockAmPmHash);
		if (amPmText != NULL)
			amPmText->SetStyleAttribute(paf::graph::TextStyleAttribute_Color,
						    0, 0, dateColor);

		paf::ui::Widget *eventBox =
			m_eventPanel->FindChild(paf::IDParam(kEventBoxHash), 0);
		if (eventBox != NULL) {
			eventBox->SetColor(rgbaFromPacked(
				(SceUInt32)m_themeInfo->m_startScreenProperty.m_notifyBorderColor));
			int dateLayout = m_themeInfo->m_startScreenProperty.m_dateLayout;
			if (dateLayout == 1) {
				eventBox->SetPos(paf::math::v4(-68.0f, 14.0f, 0.0f, 0.0f));
				m_clockRoot->SetPos(paf::math::v4(-430.0f, 59.0f, 0.0f, 0.0f));
			} else if (dateLayout == 2) {
				if (m_timeFormat != 0) {
					m_clockRoot->SetPos(
						paf::math::v4(54.0f, -222.0f, 0.0f, 0.0f));
					m_dateText->SetPos(
						paf::math::v4(380.0f, 108.0f, 0.0f, 0.0f));
				} else {
					m_clockRoot->SetPos(
						paf::math::v4(-46.0f, -222.0f, 0.0f, 0.0f));
					m_dateText->SetPos(
						paf::math::v4(480.0f, 108.0f, 0.0f, 0.0f));
				}
				m_dateText->SetAlign(2, 5, 0, NULL);
			}
		}

		if (shellThemeIconsEnabled()) {
			m_lockStartTick = 0;
			static const char *const kTestTitles[3] = {
				"NPXS10014", "NPXS10072", "NPXS10091"
			};
			static const char *const kTestLabels[3] = {
				"Custom Theme Test #1",
				"Custom Theme Test #2",
				"Custom Theme Test #3"
			};
			for (int idx = 0; idx < 3; ++idx) {
				LsdbNewEvent testEvent;
				testEvent.titleId = kTestTitles[idx];
				testEvent.itemId = "";
				testEvent.newFlag = true;
				testEvent.desc = kTestLabels[idx];
				testEvent.execTitleId = kTestTitles[idx];
				SceLsdb_315B9FD6(&testEvent, true);
			}
		}
	}

	powerManagePushKeyHandler(keyLockKeyNoop, NULL);
	powerManageLockPsButton();
	paf::inputdevice::GetOwnership(kInputOwnershipMask);

	if (!g_keyLockWallpaperMgr->IsLoading())
		ApplyWallpaper();
}

KeyLockScreen::~KeyLockScreen()
{
	s_eventJobSlots.CancelAll();
	paf::inputdevice::ReleaseOwnership(kInputOwnershipMask);
	SceShell_D4C7D2B7(13, OnShellEvent, NULL);
	SceShell_D4C7D2B7(14, OnShellEvent, NULL);
	paf::common::MainThreadCallList::Unregister(OnMainThreadTick, NULL);
	powerManagePopKeyHandler(keyLockKeyNoop, NULL);
	powerManageUnlockPsButton();

	paf::Plugin::PageCloseParam closeParam;
	m_plugin->PageClose(paf::IDParam(kKeyLockPageHash), closeParam);

	if (shellStartupGetIduMode() != 1 && !shellStartupIsTestMode())
		sceAVConfigClearAutoSuspend2();

	vshCtrlSetIdleCancelKey(809500671, 3276799, 1879965696, 0);
	vshCtrlSetIdleCancelThreshold(32, 32);
	g_keyLockScreen = NULL;
}

void KeyLockScreen::ApplyWallpaper()
{
	int texWidth = -1;
	int texHeight = -1;
	paf::intrusive_ptr<paf::graph::Surface> surface =
		g_keyLockWallpaperMgr->GetSurface(&texWidth, &texHeight);

	if (surface.get() != NULL) {
		m_pageTurn->SetTexture(surface, paf::ui::PageTurn::OBJ_TURN, 0);
		if (texWidth > 0 && texHeight > 0) {
			float srcW = (float)texWidth;
			float srcH = (float)texHeight;
			int surfW = surface->GetWidth();
			int surfH = surface->GetHeight();
			float regionX = 0.0f;
			float regionY = 0.0f;
			float regionW = 1.0f;
			float regionH = 1.0f;

			if (srcW != 960.0f || srcH != 512.0f || surfW != 960 || surfH != 512) {
				float cropW = srcW;
				float cropH = srcH;
				float ratioW = srcW / 30.0f;
				float ratioH = srcH / 17.0f;
				if (ratioW != ratioH) {
					if (ratioW < ratioH)
						cropH = (float)(int)(ratioW * 17.0f);
					else
						cropW = (float)(int)(ratioH * 30.0f);
				}
				if (surfW > 0) {
					float offsetX = (srcW - cropW) * 0.5f;
					regionX = offsetX / (float)surfW;
					regionW = ((cropW + offsetX) / (float)surfW) - regionX;
				} else {
					regionX = 0.0f;
					regionW = 0.0f;
				}
				if (surfH > 0) {
					float offsetY = (srcH - cropH) * 0.5f;
					float topV = offsetY / (float)surfH;
					float bottomV = (cropH + offsetY) / (float)surfH;
					regionY = topV + (bottomV - topV) * 0.05882353f;
					regionH = bottomV - regionY;
				} else {
					regionY = 0.0f;
					regionH = 0.0f;
				}
			}

			paf::math::v4 region(regionX, regionY, regionW, regionH);
			if (m_pageTurn->m_turn_obj != NULL &&
			    m_pageTurn->m_turn_obj->m_param != NULL) {
				m_pageTurn->m_turn_obj->m_param->m_region = region;
			}
			if (m_pageTurn->m_plane != NULL)
				m_pageTurn->m_plane->SetTexRegion(region);
		}
	}

	m_wallpaperReady = true;
	OnResume();
	m_pageTurn->SetColor(paf::math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	m_clockRoot->Show(0xFF, NULL, NULL, 0.0f);
}

void KeyLockScreen::ClearWallpaper()
{
	if (m_pageTurn == NULL)
		return;
	paf::intrusive_ptr<paf::graph::Surface> blankTex =
		paf::Framework::Instance()->GetCommonTexture(paf::IDParam(kCommonBlankTexHash));
	m_pageTurn->SetTexture(blankTex, paf::ui::PageTurn::OBJ_TURN, 0);
}

void KeyLockScreen::OnResume()
{
	if (!m_wallpaperReady)
		return;

	if (shellStartupGetIduMode() != 1 && !shellStartupIsTestMode()) {
		if (vshSblUtMgrHasComTestFlag() != 0 && !g_powerManage->m_isSuspended) {
			sceAVConfigClearAutoSuspend2();
		} else {
			sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
			sceAVConfigSetAutoSuspend2(15);
		}
	}

	if (!g_powerManage->m_isSuspended) {
		authServiceReloadRegistry(true);
		m_pendingDisplayOn = true;
	}
}

void KeyLockScreen::Unlock(bool skipPasscode)
{
	m_skipPasscode = skipPasscode;
	if (m_pageTurn->m_map_e_listener.find(kUnlockCheckTimerId) ==
	    m_pageTurn->m_map_e_listener.end()) {
		OnCheckPasscodeBeforeUnlock(0, m_pageTurn, NULL, NULL);
		ScePafWidget_F1176ACC(m_pageTurn, paf::common::transition::Type_Reset,
				      0.0f, OnUnlockTransitionEnd, NULL);
		m_guideWidget->Hide(0xFF, NULL, NULL, 0.0f);
		m_eventPanel->Hide(0xFF, NULL, NULL, 0.0f);
		m_touchSupplier->Hide(0xFF, NULL, NULL, 0.0f);
	}
}

bool KeyLockScreen::IsDragging()
{
	return m_isDragging;
}

int KeyLockScreen::GetEventAt(int index, LsdbNewEvent *outEvent)
{
	if (index < 0 || (size_t)index >= m_events.size())
		return -1;
	*outEvent = *m_events[(size_t)index];
	return 0;
}

void KeyLockScreen::UpdateClock(bool animateFlip)
{
	if (m_clockRoot == NULL)
		return;

	paf::datetime::DateTime now = paf::datetime::DateTime::Now();
	paf::datetime::DateTime zeroTime(0, 0, 0, 0, 0, 0, 0);

	bool isUnset = (m_lastDateTime.m_year == zeroTime.m_year &&
			m_lastDateTime.m_month == zeroTime.m_month &&
			m_lastDateTime.m_day == zeroTime.m_day &&
			m_lastDateTime.m_hour == zeroTime.m_hour &&
			m_lastDateTime.m_minute == zeroTime.m_minute &&
			m_lastDateTime.m_second == zeroTime.m_second &&
			m_lastDateTime.m_microsecond == zeroTime.m_microsecond);

	if (isUnset || now.m_month != m_lastDateTime.m_month ||
	    now.m_day != m_lastDateTime.m_day) {
		paf::ui::Text *dateWidget =
			findTypedWidget<paf::ui::Text>(m_clockRoot, kClockDateHash);
		if (dateWidget != NULL) {
			wchar_t dateBuf[128];
			now.FormatMsg(dateBuf, 128, L"%c");
			dateWidget->SetString(paf::wstring(dateBuf));
		}
	}

	if (isUnset || now.m_hour != m_lastDateTime.m_hour) {
		paf::ui::Text *hourWidget =
			findTypedWidget<paf::ui::Text>(m_clockRoot, kClockHourHash);
		if (hourWidget != NULL) {
			paf::ui::Text *amPmWidget =
				findTypedWidget<paf::ui::Text>(m_clockRoot, kClockAmPmHash);
			wchar_t hourBuf[32];
			if (m_timeFormat != 0) {
				now.Format(hourBuf, 32, L"%H", false);
				if (amPmWidget != NULL)
					amPmWidget->SetMetaAlpha(0.0f);
			} else {
				now.Format(hourBuf, 32, L"%I", false);
				if (amPmWidget != NULL) {
					amPmWidget->SetMetaAlpha(1.0f);
					wchar_t amPmBuf[32];
					now.Format(amPmBuf, 32, L"%p", false);
					amPmWidget->SetString(paf::wstring(amPmBuf));
				}
			}
			hourWidget->SetString(paf::wstring(hourBuf));
		}
	}

	if (isUnset || now.m_minute != m_lastDateTime.m_minute) {
		paf::ui::Text *minuteWidget =
			findTypedWidget<paf::ui::Text>(m_clockRoot, kClockMinuteHash);
		if (minuteWidget != NULL) {
			wchar_t minuteBuf[32];
			now.Format(minuteBuf, 32, L"%M", false);
			minuteWidget->SetString(paf::wstring(minuteBuf));
		}
	}

	if (animateFlip && now.m_second > 58) {
		paf::ui::Text *minuteFlip =
			findTypedWidget<paf::ui::Text>(m_clockRoot, kClockMinuteFlipHash);
		if (minuteFlip != NULL &&
		    minuteFlip->FindAnim(paf::ui::Widget::ANIM_ROT) == NULL) {
			int curMinute = now.m_minute;
			int digitVal = curMinute;
			paf::ui::AnimCB flipCb = OnDigitFlipEnd;
			wchar_t nextBuf[16];
			if (curMinute % 10 == 9) {
				flipCb = OnMinuteFlipEnd;
				sce_paf_swprintf(nextBuf, 16, L"%02d", (curMinute + 1) % 60);
			} else {
				digitVal = curMinute % 10;
				sce_paf_swprintf(nextBuf, 16, L"%d", digitVal + 1);
			}
			minuteFlip->SetString(paf::wstring(nextBuf));
			minuteFlip->SetRot(paf::math::v4(0.75f, 0.0f, 0.0f, 0.0f), 0, NULL,
					   paf::ui::Widget::ANIM_ROT, 0, 0.0f, NULL, NULL);
			minuteFlip->SetRot(paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f), 0,
					   new paf::Timer(500.0f, paf::Timer::FUNC_SINE_IN),
					   paf::ui::Widget::ANIM_ROT, 2, 0.0f, flipCb,
					   (void *)(intptr_t)digitVal);
			minuteFlip->SetRot(paf::math::v4(-0.9f, 0.0f, 0.0f, 0.0f), 0,
					   new paf::Timer(500.0f, paf::Timer::FUNC_QUAD_OUT),
					   paf::ui::Widget::ANIM_ROT, 0, 0.0f, NULL, NULL);
			minuteFlip->SetMetaAlpha(0.0f);
			minuteFlip->SetMetaAlpha(1.0f,
						 new paf::Timer(500.0f, paf::Timer::FUNC_QUAD_OUT),
						 paf::ui::Widget::ANIM_COLOR, 2);
			minuteFlip->SetMetaAlpha(0.0f, new paf::Timer(500.0f));

			paf::math::v4 pos =
				minuteFlip->GetPos(0, NULL, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
			pos.set_z(0.0f);
			minuteFlip->SetPos(pos);
			pos.set_z(-100.0f);
			minuteFlip->SetPos(pos, new paf::Timer(500.0f, paf::Timer::FUNC_QUAD_OUT),
					   paf::ui::Widget::ANIM_MOVE, 2);
		}

		if (now.m_minute >= 59) {
			paf::ui::Text *hourFlip =
				findTypedWidget<paf::ui::Text>(m_clockRoot, kClockHourFlipHash);
			if (hourFlip != NULL &&
			    hourFlip->FindAnim(paf::ui::Widget::ANIM_ROT) == NULL) {
				int curHour = now.m_hour;
				int nextHour = 0;
				if (m_timeFormat != 0) {
					if (curHour % 10 == 9 || curHour == 23) {
						nextHour = (curHour + 1) % 24;
					} else {
						curHour = curHour % 10;
						nextHour = curHour + 1;
					}
				} else if (curHour != 0 && curHour != 12) {
					curHour = curHour % 12;
					if (curHour == 9 || now.m_hour == 12) {
						nextHour = (now.m_hour + 1) % 12;
					} else {
						curHour = curHour % 10;
						nextHour = curHour + 1;
					}
				} else {
					curHour = 12;
					nextHour = 1;
				}
				wchar_t nextHourBuf[16];
				sce_paf_swprintf(nextHourBuf, 16, L"%d", nextHour);
				hourFlip->SetString(paf::wstring(nextHourBuf));
				hourFlip->SetRot(paf::math::v4(0.875f, 0.0f, 0.0f, 0.0f), 0, NULL,
						 paf::ui::Widget::ANIM_ROT, 0, 0.0f, NULL, NULL);
				hourFlip->SetRot(paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f), 0,
						 new paf::Timer(500.0f, paf::Timer::FUNC_SINE_IN),
						 paf::ui::Widget::ANIM_ROT, 2, 0.0f, OnDigitFlipEnd,
						 (void *)(intptr_t)curHour);
				hourFlip->SetRot(paf::math::v4(-1.05f, 0.0f, 0.0f, 0.0f), 0,
						 new paf::Timer(500.0f, paf::Timer::FUNC_QUAD_OUT),
						 paf::ui::Widget::ANIM_ROT, 0, 0.0f, NULL, NULL);
				hourFlip->SetMetaAlpha(0.0f);
				hourFlip->SetMetaAlpha(
					1.0f, new paf::Timer(500.0f, paf::Timer::FUNC_QUAD_OUT),
					paf::ui::Widget::ANIM_COLOR, 2);
				hourFlip->SetMetaAlpha(0.0f, new paf::Timer(500.0f));

				paf::math::v4 pos = hourFlip->GetPos(
					0, NULL, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f));
				pos.set_z(0.0f);
				hourFlip->SetPos(pos);
				pos.set_z(-100.0f);
				hourFlip->SetPos(pos,
						 new paf::Timer(500.0f, paf::Timer::FUNC_QUAD_OUT),
						 paf::ui::Widget::ANIM_MOVE, 2);
			}
		}
	}

	m_lastDateTime = now;
}

void KeyLockScreen::UpdateEventList(const paf::vector<LsdbNewEventPtr> &events)
{
	paf::vector<LsdbNewEventPtr> filtered;
	for (size_t idx = 0; idx < events.size(); ++idx) {
		paf::datetime::DateTime dt =
			paf::datetime::DateTime::ParseSQLiteDateTime(
				events[idx]->timeUpdated.c_str());
		paf::rtc::Tick tick = paf::datetime::DateTime::ConvertDateTimeToRtcTick(dt);
		if (tick >= m_lockStartTick && events[idx]->newFlag &&
		    filtered.size() < kMaxVisibleEvents) {
			filtered.push_back(events[idx]);
		}
		if (filtered.size() >= kMaxVisibleEvents)
			break;
	}

	paf::ui::ListView *listView =
		findTypedWidget<paf::ui::ListView>(m_eventPanel, kEventListViewHash);
	if (listView != NULL) {
		for (int idx = 0; idx < (int)m_events.size(); ++idx) {
			int matchPos = findMatchingEvent(m_events[idx].get(), filtered);
			if (matchPos >= 0) {
				if (matchPos >= (int)kMaxVisibleEvents)
					listView->DeleteCell(0, 2, 1);
				else
					listView->DeleteCell(0, idx, 1);
			}
		}

		int insertCount = 0;
		if (listView->GetCellNum(0) < kMaxVisibleEvents)
			insertCount = (int)filtered.size() - (int)listView->GetCellNum(0);

		m_events = filtered;
		if (insertCount > 0)
			listView->InsertCell(0, 0, (uint32_t)insertCount);
	}
}

void KeyLockScreen::OnDragStart(int32_t eventType, paf::ui::Handler *handler,
				paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)handler;
	(void)event;
	(void)userData;
	g_keyLockScreen->m_guideWidget->SetMetaAlpha(0.0f);
	g_keyLockScreen->m_eventPanel->SetMetaAlpha(0.0f);
	g_keyLockScreen->m_isDragging = true;
}

void KeyLockScreen::OnDragCancel(int32_t eventType, paf::ui::Handler *handler,
				 paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)handler;
	(void)event;
	(void)userData;
	g_keyLockScreen->m_guideWidget->SetMetaAlpha(1.0f, new paf::Timer(200.0f));
	g_keyLockScreen->m_eventPanel->SetMetaAlpha(1.0f, new paf::Timer(200.0f));
	g_keyLockScreen->m_isDragging = false;
}

void KeyLockScreen::OnPageTurnCompleted(int32_t eventType, paf::ui::Handler *handler,
					paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)handler;
	(void)event;
	(void)userData;
	g_keyLockScreen->Unlock(false);
	paf::Framework::Instance()->PlayCommonSE(28, 1.0f, false);
}

void KeyLockScreen::OnCheckPasscodeBeforeUnlock(int32_t eventType, paf::ui::Handler *handler,
						paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)event;
	(void)userData;

	if (authServiceIsRegistryLoaded()) {
		bool passcodeEnabled = true;
		authServiceIsPasscodeEnabled(AUTH_STORE_SCREEN_LOCK_DIALOG, &passcodeEnabled);
		if (!passcodeEnabled || g_keyLockScreen->m_skipPasscode) {
			KeyLockBlankPage::Close();
			PowerManage::SetPowerSaveMode(false, false);
		}
		handler->KillTimer(kUnlockCheckTimerId);
	} else if (handler->m_map_e_listener.find(kUnlockCheckTimerId) ==
		   handler->m_map_e_listener.end()) {
		handler->SetTimerListener(
			kUnlockCheckTimerId, 16.0f,
			new paf::ui::EventCBListener(OnCheckPasscodeBeforeUnlock, NULL));
	}
}

void KeyLockScreen::OnUnlockTransitionEnd(int32_t eventType, paf::ui::Handler *handler,
					  paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)event;
	(void)userData;

	if (authServiceIsRegistryLoaded()) {
		bool passcodeEnabled = true;
		authServiceIsPasscodeEnabled(AUTH_STORE_SCREEN_LOCK_DIALOG, &passcodeEnabled);
		if (!passcodeEnabled || g_keyLockScreen->m_skipPasscode) {
			sceAppMgrRestoreBgmSettingForShell();
			bool hasPendingApp = !g_powerManage->m_pendingLaunchApp.empty();
			paf::string launchApp = g_powerManage->m_pendingLaunchApp;
			PowerManage::SetScreenState(SCREEN_STATE_UNLOCKED);

			paf::Plugin *topmenuPlugin = paf::Plugin::Find("topmenu_plugin");
			if (topmenuPlugin != NULL) {
				ShellPluginInterface *topmenuIface =
					(ShellPluginInterface *)topmenuPlugin->GetInterface(1);
				if (topmenuIface != NULL) {
					void (*onUnlock)(bool, const char *) =
						(void (*)(bool, const char *))
							topmenuIface->func[kTopmenuSlotUnlockComplete];
					onUnlock(hasPendingApp, launchApp.c_str());
				}
			}
		} else {
			PowerManage::SetScreenState(SCREEN_STATE_PASSCODE);
		}

		handler->KillTimer(kUnlockCompleteTimerId);

		if (shellThemeIconsEnabled()) {
			LsdbNewEvent testEvent;
			testEvent.titleId = "NPXS10001";
			testEvent.itemId = "";
			testEvent.newFlag = true;
			testEvent.desc = "Custom Theme Test #4";
			testEvent.execTitleId = "NPXS10001";
			SceLsdb_315B9FD6(&testEvent, true);
		}
	} else if (handler->m_map_e_listener.find(kUnlockCompleteTimerId) ==
		   handler->m_map_e_listener.end()) {
		handler->SetTimerListener(
			kUnlockCompleteTimerId, 16.0f,
			new paf::ui::EventCBListener(OnUnlockTransitionEnd, NULL));
	}
}

void KeyLockScreen::OnClockTimer(int32_t eventType, paf::ui::Handler *handler,
				 paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)handler;
	(void)event;
	(void)userData;
	g_keyLockScreen->UpdateClock(true);
}

void KeyLockScreen::OnMinuteFlipEnd(paf::Timer *timer, paf::ui::AnimListener *listener,
				    void *userData, void *digitValue, void *unused)
{
	(void)timer;
	(void)userData;
	(void)unused;
	paf::ui::Widget *target = (paf::ui::Widget *)listener->m_widget;
	if (target != NULL && target->IsInherit(paf::ui::Text::TypeName())) {
		wchar_t textBuf[8];
		sce_paf_swprintf(textBuf, 8, L"%02d", (int)(intptr_t)digitValue);
		((paf::ui::Text *)target)->SetString(paf::wstring(textBuf));
	}
}

void KeyLockScreen::OnDigitFlipEnd(paf::Timer *timer, paf::ui::AnimListener *listener,
				   void *userData, void *digitValue, void *unused)
{
	(void)timer;
	(void)userData;
	(void)unused;
	paf::ui::Widget *target = (paf::ui::Widget *)listener->m_widget;
	if (target != NULL && target->IsInherit(paf::ui::Text::TypeName())) {
		wchar_t textBuf[8];
		sce_paf_swprintf(textBuf, 8, L"%d", (int)(intptr_t)digitValue);
		((paf::ui::Text *)target)->SetString(paf::wstring(textBuf));
	}
}

void KeyLockScreen::OnMainThreadTick(void *userData)
{
	(void)userData;
	if (!g_keyLockScreen->m_wallpaperReady)
		return;

	if (!g_powerManage->m_isSuspended && !s_eventJobSlots.Contains(0)) {
		static NewEventAccessor s_newEventAccessor;
		if (!s_newEventAccessorOpened) {
			SceLsdb_4C137FCC(&s_newEventAccessor);
			s_newEventAccessorOpened = true;
		}
		if (SceLsdb_8DE37E2E(&s_newEventAccessor) != 0)
			enqueueEventListJob(0, OnEventListLoaded, NULL);
	}

	if (s_clockFrameCounter % 12 == 0)
		g_keyLockScreen->UpdateClock(true);
	++s_clockFrameCounter;
	if (s_clockFrameCounter > 3600)
		s_clockFrameCounter = 0;

	if (g_keyLockScreen->m_pendingDisplayOn) {
		paf::Plugin *indicatorPlugin = paf::Plugin::Find("indicator_plugin");
		IndicatorPluginInterface *indicatorIface = (indicatorPlugin != NULL)
			? (IndicatorPluginInterface *)indicatorPlugin->GetInterface(1)
			: NULL;
		bool clockReady = (g_keyLockScreen->m_dateText->m_state == 0 &&
				   g_keyLockScreen->m_hourText->m_state == 0 &&
				   g_keyLockScreen->m_minuteText->m_state == 0);
		if (!g_powerManage->m_pendingResumeDisplayOn ||
		    (clockReady && (indicatorIface == NULL || indicatorIface->UpdateNotices()))) {
			PowerManage::TurnDisplayOnAndNotify();
			g_keyLockScreen->m_pendingDisplayOn = false;
		}
	}
}

void KeyLockScreen::OnEventListLoaded(int result, const paf::vector<LsdbNewEventPtr> *events)
{
	if (result >= 0 && g_keyLockScreen != NULL && events != NULL)
		g_keyLockScreen->UpdateEventList(*events);
}

void KeyLockScreen::OnShellEvent(int eventCode, const void *eventBuf, SceSize eventSize,
				 void *userData)
{
	(void)eventBuf;
	(void)eventSize;
	(void)userData;
	if (eventCode == 13) {
		g_keyLockScreen->UpdateClock(false);
		return;
	}
	g_keyLockScreen->UpdateClock(true);
	if (shellThemeIconsEnabled()) {
		if (g_keyLockScreen->m_themeTestVisible)
			g_keyLockScreen->m_eventPanel->Hide(0xFF, NULL, NULL, 0.0f);
		else
			g_keyLockScreen->m_eventPanel->Show(0xFF, NULL, NULL, 0.0f);
		g_keyLockScreen->m_themeTestVisible = !g_keyLockScreen->m_themeTestVisible;
	}
}

} /* namespace power_manage */
