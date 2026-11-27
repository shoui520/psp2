/*
 * SceFriendSelect (vs0:sys/external/friend_select.suprx):
 * PopupListView engine ("page_popup_list_view", "popup_list_view_list_item",
 * "popup_list_view_sub_menu_mark") and SceRtc network-tick helper.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <rtc.h>
#include <arm_neon.h>
#include <std/algorithm>
#include <std/math.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/transition.h>
#include <framework.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <graphics/surface/surface.h>
#include <graphics/text/text_layout.h>
#include <graphics/text/text_style.h>
#include <inputdevice/inputdevice.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <math/rect.h>
#include <plugin.h>
#include <timer/timer.h>
#include <versatileparam/idparam.h>
#include <widget/core/event.h>
#include <widget/core/focus.h>
#include <widget/core/handler.h>
#include <widget/core/layouter.h>
#include <widget/core/widget.h>
#include <widget/w_button.h>
#include <widget/w_button_base.h>
#include <widget/w_camera.h>
#include <widget/w_list_item.h>
#include <widget/w_list_view.h>
#include <widget/w_plane.h>
#include <widget/w_scene.h>
#include <widget/w_text.h>
#include "friend_select_internal.h"

extern "C" {
int sceRtcGetCurrentRetainedNetworkTick(SceRtcTick *tick);
int sceRtcGetCurrentTick(SceRtcTick *tick);
}

using namespace paf;
using namespace paf::common;
using namespace paf::graph;
using namespace paf::math;
using namespace paf::ui;

#define POPUP_RCO_TMPL_STYLE0		0x2EFE5BEFU
#define POPUP_RCO_TMPL_STYLE1		0x84C1CAF8U
#define POPUP_RCO_TMPL_STYLE2		0xEC05706FU
#define POPUP_RCO_TMPL_STYLE34		0x81EC253AU
#define POPUP_RCO_TMPL_ITEM_01		0x5CCE0B24U
#define POPUP_RCO_TMPL_ITEM_234		0x4750F628U
#define POPUP_RCO_ITEM_ROOT		0x8ACBB965U
#define POPUP_RCO_ITEM_CONTENT		0x8CFAA337U
#define POPUP_RCO_DIM_BG		0xF0671286U
#define POPUP_RCO_PANEL			0xABF2353AU
#define POPUP_RCO_TITLE_TEXT		0x20FF585CU
#define POPUP_RCO_LIST_VIEW		0xD41F9959U
#define POPUP_RCO_CANCEL_BTN		0xC425BA34U
#define POPUP_RCO_TEX_CHECKMARK		0x3E7E52E9U
#define POPUP_RCO_TEX_SUBMENU		0x1845D10DU
#define POPUP_RCO_TEX_BTN_0_NORMAL	0x2822454FU
#define POPUP_RCO_TEX_BTN_0_GLOW	0xA7F5DBB2U
#define POPUP_RCO_TEX_BTN_1_GLOW	0x7C11E410U

#define POPUP_EVENT_TIMER_TICK		0x02000000
#define POPUP_EVENT_TIMER_SCROLL	0x01000064
#define POPUP_EVENT_BACK		0x00020019
#define POPUP_EVENT_CLOSE_FINISH	0x00100002
#define POPUP_EVENT_LAYOUT_DONE		0x00100004
#define POPUP_PI			3.14159274f
#define POPUP_HALF_PI			1.57079637f

/* ========================================================================= */
/* Helpers matching PAF inline patterns                                      */
/* ========================================================================= */

static bool IsWidgetShowHeld(Widget *w)
{
	if (w->m_show_hold != 0)
		return true;
	if ((Widget *)w->m_scene != w && w->m_parent != NULL)
		return IsWidgetShowHeld(w->m_parent);
	return false;
}

static void SetWidgetColorU32(Widget *w, uint32_t rgba, Timer *t, int32_t id, int32_t opt,
			      float delay, AnimCB cb, void *data)
{
	uint16x8_t u16 = vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(rgba)));
	float32x4_t f4 = vmulq_f32(vcvtq_f32_u32(vmovl_u16(vget_low_u16(u16))),
				   vdupq_n_f32(1.0f / 255.0f));
	v4 color((sce::Vectormath::Simd::Aos::Vector4)f4);

	w->SetColor(color, t, id, opt, delay, cb, data);
}

static inline void SetButtonDisableColor(ButtonBase *btn, const v4 &color)
{
	if (btn->m_disable_color != NULL)
		*btn->m_disable_color = color;
	else
		btn->m_disable_color = new v4(color);
}

static inline Widget *GetLastChildWidget(Widget *parent)
{
	list<Widget *>::iterator it = parent->m_children.end();

	--it;
	return *it;
}

static const float &PopupMaxFloat(const float &a, const float &b)
{
	return (a < b) ? b : a;
}

static const float &PopupMinFloat(const float &a, const float &b)
{
	return (b < a) ? b : a;
}

static v4 PopupScaleVec4(v4 &v, const float &scale)
{
	v = v * scale;
	return v;
}

static matrix PopupMulMatrix(const matrix &lhs, const matrix &rhs)
{
	return lhs * rhs;
}

/* ========================================================================= */
/* RTC helper                                                                */
/* ========================================================================= */

int32_t FriendLibGetCurrentNetworkTick(SceRtcTick *outTick)
{
	SceRtcTick tick;
	int32_t ret = sceRtcGetCurrentRetainedNetworkTick(&tick);

	if (ret < 0)
		sceRtcGetCurrentTick(&tick);
	outTick->tick = tick.tick;
	return ret;
}

/* ========================================================================= */
/* PopupListViewConfig & static placement tables                             */
/* ========================================================================= */

PopupListViewConfig::~PopupListViewConfig()
{
}

struct PopupAngleRule {
	float maxAngle;
	uint32_t alignX;
	uint32_t alignY;
	float offsetX;
	float offsetY;
	uint32_t textureHash;
	bool rotatePi;
};

static const PopupAngleRule s_angleRules[6] = {
	{ -2.35619450f, 1, 1,  22.0f, -21.0f, 0x4EB6E6DCU, true  },
	{ -0.78539819f, 0, 1,  12.0f,  21.0f, 0x581892BAU, false },
	{  0.0f,        2, 1, -22.0f, -21.0f, 0x4EB6E6DCU, false },
	{  0.78539819f, 2, 2, -22.0f,  21.0f, 0x97764502U, false },
	{  2.35619450f, 0, 2, -12.0f, -21.0f, 0xA4E24C7BU, true  },
	{  3.14159274f, 1, 2,  22.0f,  21.0f, 0x97764502U, true  }
};

static const PopupAngleRule s_sideRules[2] = {
	{ 0.0f, 1, 0,  23.0f, 0.0f, 0x8D37C3F6U, true  },
	{ 0.0f, 2, 0, -23.0f, 0.0f, 0x8D37C3F6U, false }
};

static const PopupListPlacementRule s_dirMaskRules[4] = {
	{ 0x010U,  0.78539819f,  2.35619450f, 0, 2,  12.0f, -21.0f, 0xA4E24C7BU, false },
	{ 0x020U,  0.78539819f,  2.35619450f, 0, 2, -12.0f, -21.0f, 0xA4E24C7BU, true  },
	{ 0x100U, -2.35619450f, -0.78539819f, 0, 1,  12.0f,  21.0f, 0x581892BAU, false },
	{ 0x200U, -2.35619450f, -0.78539819f, 0, 1, -12.0f,  21.0f, 0x581892BAU, true  }
};

static const uint32_t s_itemTexStyle23[8] = {
	0x8C2646D7U, 0x8CFD89BFU, 0x7717044EU, 0x5BC99FC5U,
	0x6D3F263EU, 0xC54CDDAAU, 0xA80B82F6U, 0xDAED2DE8U
};

static const uint32_t s_itemTexStyle4[8] = {
	0x00000000U, 0x00000000U, 0xE5F16C78U, 0x5BC99FC5U,
	0x59CB789CU, 0xC54CDDAAU, 0x0DF6ED07U, 0xDAED2DE8U
};

static const v4 s_disabledColor(1.0f, 1.0f, 1.0f, 1.0f);
static const v2 s_subMenuOffset(12.0f, 0.0f);

/* ========================================================================= */
/* PopupListView internal classes                                            */
/* ========================================================================= */

struct PopupColumnMetric {
	PopupColumnMetric() : align(0), posX(0.0f), width(0.0f) {}

	uint32_t align;
	float posX;
	float width;
};

struct PopupSubMenuAnchor {
	bool active;
	v2 pos;
};

struct PopupThemeInfo {
	PopupThemeInfo() : colorMask(0), bgColor(0), customRules()
	{
		sce_paf_memset(itemTexHashes, 0, sizeof(itemTexHashes));
	}

	void ApplyDefaultTextures(int32_t style);

	uint32_t colorMask;
	uint32_t bgColor;
	uint32_t itemTexHashes[8];
	vector<PopupListPlacementRule> customRules;
};

void PopupThemeInfo::ApplyDefaultTextures(int32_t style)
{
	switch (style) {
	case 0:
		itemTexHashes[0] = POPUP_RCO_TEX_BTN_0_NORMAL;
		itemTexHashes[1] = POPUP_RCO_TEX_BTN_0_GLOW;
		itemTexHashes[2] = POPUP_RCO_TEX_BTN_0_NORMAL;
		itemTexHashes[3] = POPUP_RCO_TEX_BTN_0_GLOW;
		itemTexHashes[4] = POPUP_RCO_TEX_BTN_0_NORMAL;
		itemTexHashes[5] = POPUP_RCO_TEX_BTN_0_GLOW;
		itemTexHashes[6] = POPUP_RCO_TEX_BTN_0_NORMAL;
		itemTexHashes[7] = POPUP_RCO_TEX_BTN_0_GLOW;
		break;
	case 1:
		itemTexHashes[0] = 0;
		itemTexHashes[1] = POPUP_RCO_TEX_BTN_1_GLOW;
		itemTexHashes[2] = 0;
		itemTexHashes[3] = POPUP_RCO_TEX_BTN_1_GLOW;
		itemTexHashes[4] = 0;
		itemTexHashes[5] = POPUP_RCO_TEX_BTN_1_GLOW;
		itemTexHashes[6] = 0;
		itemTexHashes[7] = POPUP_RCO_TEX_BTN_1_GLOW;
		break;
	case 2:
	case 3:
	case 4: {
		const uint32_t *src = (style == 4) ? s_itemTexStyle4 : s_itemTexStyle23;

		for (int32_t i = 0; i < 8; ++i)
			itemTexHashes[i] = src[i];
		break;
	}
	default:
		sce_paf_memset(itemTexHashes, 0, sizeof(itemTexHashes));
		break;
	}
}

namespace paf {
template<>
int vector<ui::FocusShape>::push_back(const ui::FocusShape &val)
{
	int res = 1;

	if (count + 1 > limit)
		res = reserve(limit + 10);
	if (res != 0) {
		new (&ptr[count]) ui::FocusShape(val);
		res = ++count;
	}
	return res;
}
}

class PopupItemFadeLayouter : public Layouter {
public:
	PopupItemFadeLayouter() : Layouter(NULL, NULL), m_readyFrames(0) {}
	virtual ~PopupItemFadeLayouter() {}

	virtual int32_t OnUpdate_core()
	{
		if (m_readyFrames < 2) {
			if (m_widget->m_prepare_count != 0) {
				m_readyFrames = 0;
			} else {
				++m_readyFrames;
				if (m_readyFrames == 2) {
					Timer *t = new Timer(50.0f, 0, 0, -1, NULL);

					m_widget->SetMetaAlpha(1.0f, t, 65539, 0, 0.0f, NULL,
							       NULL);
				}
			}
		}
		return 0;
	}

	int32_t m_readyFrames;
};

class PopupListViewInstance;

struct PopupColumnTextArg {
	PopupListViewInstance *owner;
	uint32_t columnIndex;
};

class PopupListItemFactory : public listview::ItemFactory {
public:
	PopupListItemFactory(PopupListViewInstance *owner, Plugin *plugin,
			     FuncCallback1<ListItem *, listview::ItemFactory::CreateParam &> *createCb,
			     Callback2<Widget *, int32_t> *bindCb, const IDParam &templateId)
		: m_owner(owner), m_plugin(plugin), m_customCreateCb(createCb),
		  m_customBindCb(bindCb), m_templateId(templateId)
	{
	}

	virtual ~PopupListItemFactory()
	{
		if (m_customCreateCb != NULL)
			delete m_customCreateCb;
		if (m_customBindCb != NULL)
			delete m_customBindCb;
	}

	virtual ListItem *Create(listview::ItemFactory::CreateParam &param);
	virtual void Start(listview::ItemFactory::StartParam &param) { (void)param; }
	virtual void Stop(listview::ItemFactory::StopParam &param) { (void)param; }
	virtual void Dispose(listview::ItemFactory::DisposeParam &param) { (void)param; }

	PopupListViewInstance *m_owner;
	Plugin *m_plugin;
	FuncCallback1<ListItem *, listview::ItemFactory::CreateParam &> *m_customCreateCb;
	Callback2<Widget *, int32_t> *m_customBindCb;
	IDParam m_templateId;
};

class PopupListViewInstance {
public:
	PopupListViewInstance(const IDParam &pageId, const PopupListViewConfig &cfg,
			      const PopupSubMenuAnchor &subAnchor, int32_t instanceId);
	inline __attribute__((always_inline)) ~PopupListViewInstance()
	{
		if (m_selectCb != NULL)
			delete m_selectCb;
		while (m_inputLockCount != 0) {
			inputdevice::Unlock(127);
			--m_inputLockCount;
		}
	}

	v2 GetSubMenuAnchorPos(int32_t itemIndex);
	void DisableAllItems();
	int32_t BeginClose(int32_t selectedIndex);
	void ApplyLayout(bool animate);
	void RecalculateWidth(float contentWidth);
	void UpdateColumnWidth(uint32_t colIndex, Widget *textWidget);
	void MeasureButtonText(Widget *buttonWidget);
	void SetupItem(int32_t index, ListItem *item);

	int32_t m_style;
	Plugin *m_plugin;
	int32_t m_itemCount;
	int32_t m_initialSelectIndex;
	vector<PopupListItemData> m_items;
	float m_minContentWidth;
	float m_maxContentWidth;
	Callback2<int32_t, const PopupListItemData *> *m_selectCb;
	PopupListCloseCallback *m_closeCb;
	int32_t m_checkMarkSide;
	float m_topPad;
	float m_bottomPad;
	float m_leftPad;
	float m_rightPad;
	float m_leftLimit;
	float m_rightLimit;
	bool m_autoClose;
	bool m_transparentBg;
	bool m_playSe;
	bool m_wrapText;
	float m_titleMinFontSize;
	float m_itemMinFontSize;
	PopupThemeInfo m_theme;
	int32_t m_instanceId;
	IDParam m_pageId;
	Scene *m_scene;
	Widget *m_dimBg;
	Widget *m_panel;
	ListView *m_listView;
	float m_panelPadLeft;
	float m_panelPadRight;
	vector<PopupColumnMetric> m_columns;
	float m_computedWidth;
	float m_iconPosX;
	float m_checkOffset;
	float m_checkPosX;
	float m_subMenuPosX;
	int32_t m_animState;
	int32_t m_usesBuiltinItem;
	bool m_singleTextItem;
	int32_t m_customBtnChildIdx;
	int32_t m_inputLockCount;
};

static vector<PopupListViewInstance *> s_popupStack;
static int32_t s_popupNextId = 0;

/* ========================================================================= */
/* Event callbacks                                                           */
/* ========================================================================= */

static int32_t ClosePopupInstance(PopupListViewInstance *inst, int32_t selectedIndex);

static void OnPopupCancelEvent(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)self;
	(void)e;
	(void)userData;
	if (s_popupStack.empty())
		return;
	PopupListViewInstance *top = s_popupStack[s_popupStack.size() - 1];
	Widget *panel = top->m_panel;

	if (panel->m_input_target && panel->m_show_alpha >= 0.95f &&
	    !panel->m_transition_in && !panel->m_transition_out &&
	    !panel->m_destroy_protected && !IsWidgetShowHeld(panel)) {
		if (top->m_playSe)
			Framework::Instance()->PlayCommonSE(2, 1.0f, false);
		ClosePopupInstance(top, -1);
	}
}

static void OnPopupPageDestroyed(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)self;
	(void)e;
	PopupListViewInstance *inst = (PopupListViewInstance *)userData;

	if (inst != NULL)
		delete inst;
	for (vector<PopupListViewInstance *>::iterator it = s_popupStack.begin();
	     it != s_popupStack.end(); ++it) {
		if (*it == inst) {
			s_popupStack.erase(it);
			break;
		}
	}
}

static void OnPopupTitleLayout(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)e;
	Text *titleText = (Text *)self;
	PopupListViewInstance *inst = (PopupListViewInstance *)userData;
	float neededW = titleText->GetSize(0)->extract_x() + 160.0f;

	if (neededW <= inst->m_computedWidth)
		return;
	float contentW = neededW - inst->m_leftPad - inst->m_rightPad;

	if (contentW > inst->m_maxContentWidth) {
		if (inst->m_titleMinFontSize != 0.0f) {
			float curFontSize = 0.0f;

			titleText->GetStyleAttribute(TextStyleAttribute_Point, 0, curFontSize);
			if (inst->m_titleMinFontSize < curFontSize) {
				scePafWidgetSetFontSize(titleText, inst->m_titleMinFontSize,
							TextStyleAttribute_Point, 0, 0);
				return;
			}
		}
		v4 curSz = *titleText->GetSize(0);

		curSz.set_x(inst->m_maxContentWidth + inst->m_leftPad + inst->m_rightPad - 160.0f);
		titleText->SetSize(curSz, NULL, 65540, 0, 0.0f, NULL, NULL);
		titleText->SetAdjust(0, titleText->m_adjust_y, titleText->m_adjust_z);
		titleText->SetLayoutAttribute(TextLayoutAttribute_Ellipsis, true);
		contentW = inst->m_maxContentWidth;
	}
	inst->RecalculateWidth(contentW);
}

static void OnPopupScrollTimer(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)e;
	(void)userData;
	Widget *listWidget = (Widget *)self;

	listWidget->m_clip_mode = 0;
}

static void OnPopupPanelTimer(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)e;
	Widget *panel = (Widget *)self;
	PopupListViewInstance *inst = (PopupListViewInstance *)userData;

	if (inst->m_animState == 0) {
		if (panel->m_prepare_count == 0)
			inst->m_animState = 1;
	} else if (inst->m_animState == 1) {
		if (panel->m_prepare_count != 0) {
			inst->m_animState = 0;
		} else {
			inst->m_animState = 2;
			inst->ApplyLayout(false);
			if (inst->m_style == 0) {
				ScePafWidget_44B22ACA(panel, transition::Type_Popup2, 0.0f, NULL,
						      NULL);
				inst->m_listView->SetTimerListener(
					POPUP_EVENT_TIMER_SCROLL, 0.0f,
					new EventCBListener(OnPopupScrollTimer, NULL));
				inst->m_listView->m_clip_mode = 1;
			} else if (inst->m_style == 2 || inst->m_style == 3 || inst->m_style == 4) {
				float dur = ScePafWidget_44B22ACA(panel, transition::Type_Popup5,
								  0.0f, NULL, NULL);

				inst->m_listView->Hide(0xFF, NULL, NULL, 0.0f);
				if (dur * 0.5f > 0.0f) {
					Timer *t = new Timer(dur * 0.5f, 0, 0, -1, NULL);

					inst->m_listView->Show(0xFF, t, NULL, 0.0f);
				} else {
					inst->m_listView->Show(0xFF, NULL, NULL, 0.0f);
				}
			}
		}
	} else if (inst->m_animState == 2) {
		panel->KillTimer(POPUP_EVENT_TIMER_TICK);
		inputdevice::Unlock(127);
		--inst->m_inputLockCount;
	}
}

static void OnPopupColumnTextLayout(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)e;
	PopupColumnTextArg *arg = (PopupColumnTextArg *)userData;
	PopupListViewInstance *inst = arg->owner;
	uint32_t colIndex = arg->columnIndex;

	delete arg;
	self->DeleteEventCallback(POPUP_EVENT_LAYOUT_DONE, OnPopupColumnTextLayout, arg);
	inst->UpdateColumnWidth(colIndex, (Widget *)self);
}

static void OnPopupButtonTextLayout(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)e;
	((PopupListViewInstance *)userData)->MeasureButtonText((Widget *)self);
}

static void OnPopupItemDecide(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)self;
	(void)e;
	ListItem *item = (ListItem *)userData;
	int32_t cellIndex = item->GetCellIndex();

	if (s_popupStack.empty())
		return;
	PopupListViewInstance *top = s_popupStack[s_popupStack.size() - 1];

	if (top == NULL)
		return;
	int32_t savedId = top->m_instanceId;

	if (top->m_playSe)
		Framework::Instance()->PlayCommonSE(1, 1.0f, false);
	if (top->m_selectCb != NULL) {
		const PopupListItemData *data =
			(cellIndex < (int32_t)top->m_items.size()) ? &top->m_items[cellIndex] :
								     NULL;

		top->m_selectCb->Invoke(cellIndex, data);
	}
	for (vector<PopupListViewInstance *>::iterator it = s_popupStack.begin();
	     it != s_popupStack.end(); ++it) {
		if (*it == top) {
			if ((*it)->m_instanceId == savedId && top->m_autoClose)
				ClosePopupInstance(top, cellIndex);
			break;
		}
	}
}

static void OnPopupScrollFocusTimer(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)e;
	PopupListViewInstance *inst = (PopupListViewInstance *)userData;

	if (inst == NULL) {
		self->KillTimer(POPUP_EVENT_TIMER_TICK);
		return;
	}
	ListItem *item = inst->m_listView->GetListItem(0, inst->m_initialSelectIndex);

	if (item == NULL) {
		self->KillTimer(POPUP_EVENT_TIMER_TICK);
		return;
	}
	if (ScePafWidget_DB0FFD33(item)) {
		inst->m_listView->SetFocus(0, inst->m_initialSelectIndex,
					   (ListView::FocusAlignType)3, NULL);
		self->KillTimer(POPUP_EVENT_TIMER_TICK);
	}
}

/* ========================================================================= */
/* PopupListViewInstance implementation                                      */
/* ========================================================================= */

PopupListViewInstance::PopupListViewInstance(const IDParam &pageId,
					     const PopupListViewConfig &cfg,
					     const PopupSubMenuAnchor &subAnchor,
					     int32_t instanceId)
	: m_style(cfg.style), m_plugin(cfg.plugin), m_itemCount(cfg.itemCount),
	  m_initialSelectIndex(cfg.initialSelectIndex), m_items(cfg.items),
	  m_minContentWidth(cfg.minSize.extract_x()), m_maxContentWidth(cfg.maxSize.extract_x()),
	  m_selectCb(cfg.selectCb), m_closeCb(cfg.closeCb), m_checkMarkSide(cfg.checkMarkSide),
	  m_topPad(cfg.topPad), m_bottomPad(cfg.bottomPad), m_leftPad(cfg.leftPad),
	  m_rightPad(cfg.rightPad), m_leftLimit(cfg.leftLimit), m_rightLimit(cfg.rightLimit),
	  m_autoClose(cfg.autoClose), m_transparentBg(cfg.transparentBg), m_playSe(cfg.playSe),
	  m_wrapText(cfg.wrapText), m_titleMinFontSize(cfg.titleMinFontSize),
	  m_itemMinFontSize(cfg.itemMinFontSize), m_theme(), m_instanceId(instanceId),
	  m_pageId(pageId), m_scene(NULL), m_dimBg(NULL), m_panel(NULL), m_listView(NULL),
	  m_panelPadLeft(0.0f), m_panelPadRight(0.0f), m_columns(), m_computedWidth(0.0f),
	  m_iconPosX(0.0f), m_checkOffset(32.0f), m_checkPosX(0.0f), m_subMenuPosX(0.0f),
	  m_animState(0), m_usesBuiltinItem(cfg.customCreateCb == NULL ? 1 : 0),
	  m_singleTextItem(false), m_customBtnChildIdx(-2), m_inputLockCount(0)
{
	const float minCheckPad = 64.0f;

	if (m_checkMarkSide == 1)
		m_leftPad = PopupMaxFloat(m_leftPad, minCheckPad);
	if (m_checkMarkSide == 2)
		m_rightPad = PopupMaxFloat(m_rightPad, minCheckPad);

	if (m_items.empty() && !cfg.legacyLabels.empty()) {
		PopupListItemData emptyItem;

		m_items.resize(cfg.legacyLabels.size(), emptyItem);
		for (size_t i = 0; i < m_items.size(); ++i)
			m_items[i].label = cfg.legacyLabels[i];
	}
	if (!m_items.empty()) {
		m_itemCount = (int32_t)m_items.size();
		m_usesBuiltinItem = 1;
	}
	if (m_initialSelectIndex >= m_itemCount)
		m_initialSelectIndex = -1;

	m_theme.colorMask = cfg.colorMask;
	m_theme.bgColor = cfg.bgColor;
	m_theme.customRules = cfg.customRules;
	if (cfg.itemTexHashes[0] != 0 || cfg.itemTexHashes[1] != 0) {
		for (int32_t i = 0; i < 8; ++i)
			m_theme.itemTexHashes[i] = cfg.itemTexHashes[i];
	} else if (m_style != 0 || m_transparentBg) {
		m_theme.ApplyDefaultTextures(m_style);
	} else {
		sce_paf_memset(m_theme.itemTexHashes, 0, sizeof(m_theme.itemTexHashes));
	}

	if (!m_items.empty()) {
		bool hasIcon = false;
		bool hasSubMenu = false;

		for (size_t i = 0; i < m_items.size(); ++i) {
			if (m_items[i].icon.get() != NULL)
				hasIcon = true;
			if (m_items[i].hasSubMenu)
				hasSubMenu = true;
			if (hasIcon && hasSubMenu)
				break;
		}
		if (hasIcon) {
			m_iconPosX = m_leftPad + 24.0f;
			m_leftPad += 56.0f;
			m_checkOffset += 56.0f;
		}
		if (hasSubMenu) {
			m_subMenuPosX = -m_rightPad - 23.0f;
			m_rightPad += 66.0f;
		}
	}

	Plugin::PageOpenParam pageParam;

	pageParam.overwrite_draw_priority = cfg.pagePriority;
	pageParam.option = Plugin::PageOption_Create;
	m_scene = m_plugin->PageOpen(pageId, pageParam);

	float hPad = m_leftPad + m_rightPad;

	m_minContentWidth -= hPad;
	if (m_minContentWidth <= 0.0f)
		m_minContentWidth = 0.0f;
	m_maxContentWidth -= hPad;
	if (m_maxContentWidth <= 0.0f)
		m_maxContentWidth = m_scene->GetSize(0)->extract_x() * 0.8f;

	IDParam tmplId;

	if (m_style == 0)
		tmplId.m_idhash = POPUP_RCO_TMPL_STYLE0;
	else if (m_style == 1)
		tmplId.m_idhash = POPUP_RCO_TMPL_STYLE1;
	else if (m_style == 2)
		tmplId.m_idhash = POPUP_RCO_TMPL_STYLE2;
	else
		tmplId.m_idhash = POPUP_RCO_TMPL_STYLE34;

	Plugin::TemplateOpenParam tmplParam;

	m_plugin->TemplateOpen(m_scene, tmplId, tmplParam);

	m_dimBg = m_scene->FindChild(IDParam(POPUP_RCO_DIM_BG), 0);
	if (m_dimBg != NULL) {
		m_dimBg->SetAdjust(1, 1, 1);
		ScePafWidget_42C5A4D4(m_dimBg, 0, 0);
		ScePafCommon_6CF3A3C8(0.0f, m_dimBg, transition::Type_Popup5, false, false);
	}

	float cellHeight = (m_style == 2 || m_style == 3 || m_style == 4) ? 60.0f : 70.0f;
	float maxHeight = cfg.maxSize.extract_y();

	if (maxHeight <= 0.0f)
		maxHeight = m_scene->GetSize(0)->extract_y() * 0.9f;
	float vPad = m_topPad + m_bottomPad;

	if (m_style == 0)
		vPad += 88.0f;
	float rawListH = cellHeight * (float)m_itemCount;
	float maxAvailH = maxHeight - vPad;
	float clampedH = PopupMinFloat(rawListH, maxAvailH);
	float minListH = cfg.minSize.extract_y() - vPad;

	clampedH = PopupMaxFloat(clampedH, minListH);
	bool alignedScroll = false;

	if (m_style == 0 && clampedH < rawListH) {
		alignedScroll = true;
		clampedH = (float)(((int32_t)clampedH + 31) & ~31);
	}
	float totalH = clampedH + vPad;

	m_panel = m_scene->FindChild(IDParam(POPUP_RCO_PANEL), 0);
	m_panel->Hide(0xFF, NULL, NULL, 0.0f);
	m_panel->SetIntervalEvent(0, 0.0f, OnPopupPanelTimer, this);
	inputdevice::Lock(127);
	++m_inputLockCount;
	if ((m_theme.colorMask & 1U) != 0)
		SetWidgetColorU32(m_panel, m_theme.bgColor, NULL, 65537, 0, 0.0f, NULL, NULL);

	if (m_style == 3 || m_style == 4) {
		v2 anchorPos(0.0f, 0.0f);
		float angle = 0.0f;

		if (cfg.anchorWidget != NULL) {
			if ((cfg.anchorWidget->m_update_flag & 7U) != 0)
				cfg.anchorWidget->UpdateMatrix();
			v4 worldPos = cfg.anchorWidget->m_world_matrix.w();

			if (worldPos.extract_z() != 0.0f) {
				Camera *cam = cfg.anchorWidget->m_scene->m_camera;
				matrix vp = PopupMulMatrix(cam->m_projection, cam->m_view);
				v4 clip(vp.Transform(worldPos.getImpl()));

				if (clip.extract_w() == 0.0f) {
					worldPos = v4(0.0f, 0.0f, 0.0f, 0.0f);
				} else {
					float invW = 1.0f / clip.extract_w();
					v4 ndc = PopupScaleVec4(clip, invW);
					rect vpRect(cam->m_scene->m_screen->vp);
					v2 screenSz = vpRect.size();

					worldPos = v4(ndc.extract_x() * screenSz.extract_x() * 0.5f,
						      ndc.extract_y() * screenSz.extract_y() * 0.5f,
						      0.0f, 0.0f);
				}
			}
			angle = sce_paf_atan2f(worldPos.extract_y(), worldPos.extract_x());
			anchorPos.set_x(worldPos.extract_x() -
					sce_paf_cosf(angle) * cfg.anchorRadius);
			anchorPos.set_y(worldPos.extract_y() -
					sce_paf_sinf(angle) * cfg.anchorRadius);
		} else {
			if (subAnchor.active)
				anchorPos = subAnchor.pos;
			else
				anchorPos = v2(cfg.position.extract_x(), cfg.position.extract_y());
			angle = sce_paf_atan2f(anchorPos.extract_y(), anchorPos.extract_x());
		}

		uint32_t alignX = 0;
		uint32_t alignY = 0;
		uint32_t texHash = 0;
		bool rotatePi = false;

		if (cfg.dirMask != 0 || !m_theme.customRules.empty()) {
			const PopupListPlacementRule *beginRule =
				!m_theme.customRules.empty() ? &m_theme.customRules[0] :
							       s_dirMaskRules;
			size_t ruleCount =
				!m_theme.customRules.empty() ? m_theme.customRules.size() : 4U;
			uint32_t mask = (cfg.dirMask != 0) ? cfg.dirMask : 0xFFFFFFFFU;
			float bestDist = 3.4028235e38f;
			const PopupListPlacementRule *bestRule = NULL;

			for (size_t i = 0; i < ruleCount; ++i) {
				const PopupListPlacementRule &r = beginRule[i];

				if ((r.dirMask & mask) != 0) {
					float dist = 0.0f;

					if (angle > r.maxAngle)
						dist = angle - r.maxAngle;
					else if (angle < r.minAngle)
						dist = r.minAngle - angle;
					if (dist < bestDist) {
						bestDist = dist;
						bestRule = &r;
					}
				}
			}
			alignX = bestRule->alignX;
			alignY = bestRule->alignY;
			anchorPos.set_x(anchorPos.extract_x() + bestRule->offsetX);
			anchorPos.set_y(anchorPos.extract_y() + bestRule->offsetY);
			texHash = bestRule->textureHash;
			rotatePi = bestRule->rotatePi;
		} else {
			const PopupAngleRule *chosen = NULL;

			if (anchorPos.extract_y() >= 100.0f || anchorPos.extract_y() <= -100.0f) {
				for (size_t i = 0; i < 6; ++i) {
					if (angle < s_angleRules[i].maxAngle) {
						chosen = &s_angleRules[i];
						break;
					}
				}
			} else {
				chosen = &s_sideRules[(anchorPos.extract_x() >= 0.0f) ? 1 : 0];
			}
			alignX = chosen->alignX;
			alignY = chosen->alignY;
			anchorPos.set_x(anchorPos.extract_x() + chosen->offsetX);
			anchorPos.set_y(anchorPos.extract_y() + chosen->offsetY);
			texHash = chosen->textureHash;
			rotatePi = chosen->rotatePi;
		}

		intrusive_ptr<Surface> tex = m_plugin->GetTexture(IDParam(texHash));

		m_panel->SetTexture(tex);
		if (rotatePi) {
			DrawObj *obj = m_panel->GetDrawObj(0);

			obj->SetRot(v3(0.0f, POPUP_PI, 0.0f));
		}

		float topEdge = 0.0f;
		float botEdge = 0.0f;

		if (alignY == 0) {
			topEdge = anchorPos.extract_y() + totalH * 0.5f;
			botEdge = anchorPos.extract_y() - totalH * 0.5f;
		} else if (alignY == 1) {
			topEdge = anchorPos.extract_y() + totalH;
			botEdge = anchorPos.extract_y();
		} else if (alignY == 2) {
			topEdge = anchorPos.extract_y();
			botEdge = anchorPos.extract_y() - totalH;
		}
		float adjustY = 0.0f;

		if (topEdge > cfg.topLimit)
			adjustY = cfg.topLimit - topEdge;
		else if (botEdge < cfg.bottomLimit)
			adjustY = cfg.bottomLimit - botEdge;

		v4 panelPos(sce_paf_roundf(anchorPos.extract_x()),
			    anchorPos.extract_y() + adjustY, cfg.position.extract_z(), 0.0f);

		m_panel->SetPos(panelPos, NULL, 0x10000, 0, 0.0f, NULL, NULL);
		m_panel->SetAlign(alignX, m_panel->m_align_y, m_panel->m_align_z, NULL);
		m_panel->SetAlign(m_panel->m_align_x, alignY, m_panel->m_align_z, NULL);
		m_panelPadLeft = 7.0f;
		m_panelPadRight = 7.0f;

		v4 panelSz = *m_panel->GetSize(0);

		panelSz.set_y(totalH + 14.0f);
		m_panel->SetSize(panelSz, NULL, 65540, 0, 0.0f, NULL, NULL);
	} else if (m_style != 1) {
		if (m_style == 2) {
			m_panelPadLeft = 7.0f;
			m_panelPadRight = 7.0f;
			totalH += 14.0f;
		}
		v4 panelPos = cfg.position;

		if (m_style == 0 && alignedScroll) {
			float refY = panelPos.extract_y();

			if (cfg.alignY == 0)
				refY += totalH * 0.5f;
			else if (cfg.alignY == 1)
				refY += totalH;
			float shifted = refY + 272.0f - 80.0f;
			float snapped = (float)(((int32_t)shifted + 31) & ~31);

			panelPos.set_y(panelPos.extract_y() + (snapped - shifted));
		}
		m_panel->SetPos(panelPos, NULL, 0x10000, 0, 0.0f, NULL, NULL);
		m_panel->SetAlign(cfg.alignX, m_panel->m_align_y, m_panel->m_align_z, NULL);
		m_panel->SetAlign(m_panel->m_align_x, cfg.alignY, m_panel->m_align_z, NULL);
		m_panel->SetOrigin(cfg.anchorX, m_panel->m_origin_y, m_panel->m_origin_z, NULL);
		m_panel->SetOrigin(m_panel->m_origin_x, cfg.anchorY, m_panel->m_origin_z, NULL);

		v4 panelSz = *m_panel->GetSize(0);

		panelSz.set_y(totalH);
		m_panel->SetSize(panelSz, NULL, 65540, 0, 0.0f, NULL, NULL);
	}

	if (m_style == 1) {
		m_panel->SetPos(v4(0.0f, 0.0f, 0.0f, 0.0f), NULL, 0x10000, 0, 0.0f, NULL, NULL);
		ScePafWidget_44B22ACA(m_panel, transition::Type_Popup4, 0.0f, NULL, NULL);
		ScePafWidget_42C5A4D4(m_panel, 0, 1);
		m_panel->AddEventCallback(POPUP_EVENT_BACK, OnPopupCancelEvent, NULL);
	} else if (m_style == 2 || m_style == 3 || m_style == 4) {
		ScePafWidget_42C5A4D4(m_panel, 0, 0);
		m_panel->AddEventCallback(POPUP_EVENT_BACK, OnPopupCancelEvent, NULL);
	} else if (m_style == 0) {
		Widget *titleWidget = m_panel->FindChild(IDParam(POPUP_RCO_TITLE_TEXT), 0);

		titleWidget->SetString(cfg.titleText);
		titleWidget->AddEventCallback(POPUP_EVENT_LAYOUT_DONE, OnPopupTitleLayout, this);
	}

	m_listView = (ListView *)m_scene->FindChild(IDParam(POPUP_RCO_LIST_VIEW), 0);
	v4 listPos = m_listView->GetPos(0, NULL, v4(0.0f, 0.0f, 0.0f, 0.0f));

	listPos.set_x(listPos.extract_x() + (m_panelPadLeft - m_panelPadRight) * 0.5f);
	m_listView->SetPos(listPos, NULL, 0x10000, 0, 0.0f, NULL, NULL);

	v4 listSz = *m_listView->GetSize(0);

	listSz.set_y(clampedH);
	m_listView->SetSize(listSz, NULL, 65540, 0, 0.0f, NULL, NULL);

	FuncCallback1<ListItem *, listview::ItemFactory::CreateParam &> *customCreate =
		(FuncCallback1<ListItem *, listview::ItemFactory::CreateParam &> *)
			cfg.customCreateCb;

	if (!m_items.empty()) {
		if (customCreate != NULL)
			delete customCreate;
		customCreate = NULL;
	}
	IDParam itemTmplId((m_style > 1) ? POPUP_RCO_TMPL_ITEM_234 : POPUP_RCO_TMPL_ITEM_01);
	PopupListItemFactory *factory = new PopupListItemFactory(
		this, cfg.plugin, customCreate, (Callback2<Widget *, int32_t> *)cfg.customBindCb,
		itemTmplId);

	m_listView->SetItemFactory(factory);
	m_listView->InsertSegment(0, 1);
	m_listView->SetCellSizeDefault(0, v4(0.0f, cellHeight, 0.0f, 0.0f));
	m_listView->InsertCell(0, 0, (uint32_t)m_itemCount);
	m_listView->SetFocus(0, m_initialSelectIndex, (ListView::FocusAlignType)1, NULL);
	m_listView->SetIntervalEvent(0, 0.0f, OnPopupScrollFocusTimer, this);

	Widget *cancelBtn = m_scene->FindChild(IDParam(POPUP_RCO_CANCEL_BTN), 0);

	if (cancelBtn != NULL) {
		ScePafWidget_21125870(cancelBtn, 32);
		cancelBtn->AddEventCallback(ButtonBase::CB_BTN_DECIDE, OnPopupCancelEvent, NULL);
	}
	if (m_panel != NULL && (m_style == 2 || m_style == 3)) {
		Button *hiddenCancel = new Button(m_panel, NULL);

		ScePafWidget_21125870(hiddenCancel, 64);
		hiddenCancel->m_keycode_capture = true;
		hiddenCancel->AddEventCallback(ButtonBase::CB_BTN_DECIDE, OnPopupCancelEvent, NULL);
		hiddenCancel->Hide(0xFF, NULL, NULL, 0.0f);
	}
}

v2 PopupListViewInstance::GetSubMenuAnchorPos(int32_t itemIndex)
{
	v2 result(0.0f, 0.0f);
	listview::ListPos pos = m_listView->GetGeneratedPos(itemIndex);
	ListItem *item = m_listView->GetListItem(pos.m_segment, pos.m_cell);

	if (item != NULL && item->GetChild(0) != NULL) {
		Widget *child = item->GetChild(0);

		if (child->IsInherit(Button::TypeName())) {
			Widget *mark =
				child->FindChild(IDParam("popup_list_view_sub_menu_mark"), 0);

			if (mark != NULL) {
				if ((mark->m_update_flag & 7U) != 0)
					mark->UpdateMatrix();
				v4 wp = mark->m_world_matrix.w();

				result = v2(wp.extract_x(), wp.extract_y()) + s_subMenuOffset;
			}
		}
	}
	return result;
}

void PopupListViewInstance::DisableAllItems()
{
	int32_t count = m_listView->GetGeneratedNum();

	for (int32_t i = 0; i < count; ++i) {
		listview::ListPos pos = m_listView->GetGeneratedPos(i);
		ListItem *item = m_listView->GetListItem(pos.m_segment, pos.m_cell);
		Widget *child = (item != NULL) ? item->GetChild(0) : NULL;

		if (child != NULL && child->IsInherit(Button::TypeName())) {
			ButtonBase *btn = (ButtonBase *)child;

			SetButtonDisableColor(btn, s_disabledColor);
			btn->Disable(false);
		}
	}
}

int32_t PopupListViewInstance::BeginClose(int32_t selectedIndex)
{
	int32_t retIdx = selectedIndex;

	if (selectedIndex >= 0 && m_initialSelectIndex == selectedIndex)
		retIdx = -1;

	m_scene->DeleteEventCallback(POPUP_EVENT_CLOSE_FINISH, OnPopupPageDestroyed, this);

	if (m_style == 0) {
		float dur = ScePafWidget_F1176ACC(m_panel, transition::Type_Popup2, 0.0f, NULL,
						  NULL);

		if (m_dimBg != NULL)
			ScePafCommon_B48C9BDC(0.0f, m_dimBg, transition::Type_Popup5, false,
					      false);
		ScePafWidget_5A9793B0(m_scene, dur);
		m_listView->m_clip_mode = 1;
	} else if (m_style == 1) {
		float dur = ScePafWidget_F1176ACC(m_panel, transition::Type_Popup4, 0.0f, NULL,
						  NULL);

		if (m_dimBg != NULL)
			ScePafCommon_B48C9BDC(0.0f, m_dimBg, transition::Type_Popup5, false,
					      false);
		ScePafWidget_5A9793B0(m_scene, dur);
	} else if (m_style == 2 || m_style == 3 || m_style == 4) {
		float dur = ScePafWidget_F1176ACC(m_panel, transition::Type_Popup5, 0.0f, NULL,
						  NULL);

		if (dur * 0.8f > 0.0f) {
			Timer *t = new Timer(dur * 0.8f, 0, 0, -1, NULL);

			m_listView->Hide(0xFF, t, NULL, 0.0f);
		} else {
			m_listView->Hide(0xFF, NULL, NULL, 0.0f);
		}
		if (m_dimBg != NULL)
			ScePafCommon_B48C9BDC(0.0f, m_dimBg, transition::Type_Popup5, false,
					      false);
		ScePafWidget_5A9793B0(m_scene, dur);
	}

	Plugin::PageCloseParam closeParam;

	m_plugin->PageClose(m_pageId, closeParam);
	return retIdx;
}

void PopupListViewInstance::ApplyLayout(bool animate)
{
	int32_t genNum = m_listView->GetGeneratedNum();

	if (m_singleTextItem) {
		for (int32_t i = 0; i < genNum; ++i) {
			listview::ListPos pos = m_listView->GetGeneratedPos(i);
			ListItem *item = m_listView->GetListItem(pos.m_segment, pos.m_cell);
			Widget *btn = item->GetChild(0);
			AdjustParam adj = (btn->m_adjust_param != NULL) ? *btn->m_adjust_param :
									  ScePafWidget_BE1CEA2E;

			adj.m_left_margin = m_columns[0].posX;
			ScePafWidget_B1C23134(btn, adj);

			if (m_checkMarkSide != 0 && pos.m_cell == m_initialSelectIndex &&
			    btn != NULL && btn->m_children.size() > 0) {
				Widget *check = GetLastChildWidget(btn);
				v4 p = check->GetPos(0, NULL, v4(0.0f, 0.0f, 0.0f, 0.0f));

				p.set_x(m_checkPosX);
				check->SetPos(p, NULL, 0x10000, 0, 0.0f, NULL, NULL);
			}
			if (m_style == 2 || m_style == 3 || m_style == 4) {
				v4 itemSz = *item->GetSize(1);

				for (size_t s = 0; s < btn->m_focus_parts.size(); ++s) {
					FocusShape &sh = btn->m_focus_parts[s];
					v3 rot = sh.m_rot;

					if (rot == v3(0.0f, 0.0f, POPUP_HALF_PI) ||
					    rot == v3(0.0f, 0.0f, -POPUP_HALF_PI)) {
						sh.m_size = v2(itemSz.extract_y(),
							       m_computedWidth);
					} else {
						sh.m_size = v2(m_computedWidth,
							       itemSz.extract_y());
					}
					sh.m_changed = true;
				}
				FocusShape defaultShape;
				FocusShape *shape =
					(btn->m_focus_shape != NULL) ? btn->m_focus_shape :
								       &defaultShape;

				shape->m_size = v2(m_computedWidth, itemSz.extract_y());
				ScePafWidget_13B246E0(btn, shape);
			}
		}
	} else {
		for (int32_t i = 0; i < genNum; ++i) {
			listview::ListPos pos = m_listView->GetGeneratedPos(i);
			ListItem *item = m_listView->GetListItem(pos.m_segment, pos.m_cell);
			Widget *container = item;

			if (m_usesBuiltinItem != 0) {
				container = item->GetChild(0);
			} else if (m_customBtnChildIdx >= 0 &&
				   m_customBtnChildIdx < (int32_t)item->m_children.size()) {
				container = item->GetChild(m_customBtnChildIdx);
			}

			size_t colIdx = 0;

			for (list<Widget *>::iterator it = container->m_children.begin();
			     it != container->m_children.end() && colIdx < m_columns.size();
			     ++it, ++colIdx) {
				Widget *w = *it;
				v4 p = w->GetPos(0, NULL, v4(0.0f, 0.0f, 0.0f, 0.0f));

				p.set_x(m_columns[colIdx].posX);
				w->SetPos(p, NULL, 0x10000, 0, 0.0f, NULL, NULL);
			}

			if (m_checkMarkSide != 0 && pos.m_cell == m_initialSelectIndex) {
				Widget *checkParent = container;

				if (m_usesBuiltinItem == 0 &&
				    (m_customBtnChildIdx < 0 ||
				     m_customBtnChildIdx >= (int32_t)item->m_children.size())) {
					if (checkParent == NULL ||
					    checkParent->m_children.size() == 0)
						continue;
					checkParent = GetLastChildWidget(checkParent);
				}
				if (checkParent != NULL && checkParent->m_children.size() > 0) {
					Widget *check = GetLastChildWidget(checkParent);
					v4 p = check->GetPos(0, NULL, v4(0.0f, 0.0f, 0.0f, 0.0f));

					p.set_x(m_checkPosX);
					check->SetPos(p, NULL, 0x10000, 0, 0.0f, NULL, NULL);
				}
			}
		}
	}

	v4 panelSz = *m_panel->GetSize(1);

	panelSz.set_x(m_computedWidth + m_panelPadLeft + m_panelPadRight);
	Timer *panelTimer = animate ? new Timer(100.0f, 0, 0, -1, NULL) : NULL;

	m_panel->SetSize(panelSz, panelTimer, 0x10000, 2, 0.0f, NULL, NULL);

	if (m_style == 3 || m_style == 4) {
		v4 curPos = m_panel->GetPos(1, NULL, v4(0.0f, 0.0f, 0.0f, 0.0f));
		uint32_t ax = m_panel->m_align_x;
		float leftEdge = 0.0f;
		float rightEdge = 0.0f;

		if (ax == 0) {
			leftEdge = curPos.extract_x() - panelSz.extract_x() * 0.5f;
			rightEdge = curPos.extract_x() + panelSz.extract_x() * 0.5f;
		} else if (ax == 1) {
			leftEdge = curPos.extract_x();
			rightEdge = curPos.extract_x() + panelSz.extract_x();
		} else if (ax == 2) {
			leftEdge = curPos.extract_x() - panelSz.extract_x();
			rightEdge = curPos.extract_x();
		}
		float shiftX = 0.0f;

		if (leftEdge < m_leftLimit)
			shiftX = m_leftLimit - leftEdge;
		else if (rightEdge > m_rightLimit)
			shiftX = m_rightLimit - rightEdge;

		if (shiftX != 0.0f) {
			curPos.set_x(curPos.extract_x() + shiftX);
			m_panel->SetPos(curPos, NULL, 0x10000, 0, 0.0f, NULL, NULL);
			PlaneObj *bgObj = (PlaneObj *)m_panel->GetDrawObj(0);
			uint32_t divMode = (bgObj->m_divide != NULL) ?
						   bgObj->m_divide->m_div_mode :
						   bgObj->m_tex_surface[0]->GetDivideMode();

			if (divMode == 5)
				bgObj->SetDivCenterPos(-shiftX);
		}
	}

	v4 listSz = *m_listView->GetSize(1);

	listSz.set_x(m_computedWidth);
	Timer *listTimer = animate ? new Timer(100.0f, 0, 0, -1, NULL) : NULL;

	m_listView->SetSize(listSz, listTimer, 0x10000, 2, 0.0f, NULL, NULL);

	v4 listPos = m_listView->GetPos(0, NULL, v4(0.0f, 0.0f, 0.0f, 0.0f));

	listPos.set_x((m_panelPadLeft - m_panelPadRight) * 0.5f);
	m_listView->SetPos(listPos, NULL, 0x10000, 0, 0.0f, NULL, NULL);
}

void PopupListViewInstance::RecalculateWidth(float contentWidth)
{
	float totalW = contentWidth + m_leftPad + m_rightPad;

	if (m_computedWidth < totalW)
		m_computedWidth = totalW;
	float innerW = m_computedWidth - m_leftPad - m_rightPad;

	if (m_singleTextItem) {
		m_columns[0].posX = m_leftPad;
	} else {
		float cursor = (m_leftPad - m_rightPad) * 0.5f - innerW * 0.5f;

		for (size_t i = 0; i < m_columns.size(); ++i) {
			float w = m_columns[i].width;

			m_columns[i].posX = cursor;
			if (m_columns[i].align == 0)
				m_columns[i].posX = cursor + w * 0.5f;
			else if (m_columns[i].align == 2)
				m_columns[i].posX = cursor + w;
			cursor += w;
		}
	}
	if (m_checkMarkSide != 0) {
		float base = (m_checkMarkSide == 1) ? (-innerW * 0.5f - m_checkOffset) :
						      (innerW * 0.5f + m_checkOffset);

		m_checkPosX = base + (m_leftPad - m_rightPad) * 0.5f;
	}
	if (m_animState == 2)
		ApplyLayout(true);
}

void PopupListViewInstance::UpdateColumnWidth(uint32_t colIndex, Widget *textWidget)
{
	float w = textWidget->GetSize(0)->extract_x();

	if (m_columns[colIndex].width < w) {
		m_columns[colIndex].width = w;
		float sum = 0.0f;

		for (size_t i = 0; i < m_columns.size(); ++i)
			sum += m_columns[i].width;
		if (sum >= m_maxContentWidth)
			m_columns[colIndex].width = m_maxContentWidth - (sum - w);
		RecalculateWidth(min(sum, m_maxContentWidth));
	}
}

void PopupListViewInstance::MeasureButtonText(Widget *buttonWidget)
{
	TextObj *textObj = (TextObj *)buttonWidget->GetDrawObj(1);
	float maxW = m_minContentWidth;
	uint32_t lineCount = 0;

	textObj->GetLineCount(lineCount);
	for (uint32_t i = 0; i < lineCount; ++i) {
		TextLayoutLineInfo info;

		textObj->GetLineInfo(i, info);
		if (info.frame.extract_z() > maxW)
			maxW = info.frame.extract_z();
	}

	float prevW = m_columns[0].width;

	if (prevW < maxW) {
		m_columns[0].width = maxW;
		if (maxW > m_maxContentWidth) {
			if (m_itemMinFontSize != 0.0f && lineCount == 1) {
				float curFontSize = 0.0f;

				textObj->GetStyleAttribute(TextStyleAttribute_Point, 0,
							   curFontSize);
				if (m_itemMinFontSize < curFontSize) {
					textObj->SetStyleAttribute(TextStyleAttribute_Point, 0, 0,
								   m_itemMinFontSize);
					m_columns[0].width = prevW;
					return;
				}
			}
			if (m_wrapText && lineCount == 1) {
				bool wordWrap = false;

				textObj->GetLayoutAttribute(TextLayoutAttribute_WordWrap,
							    wordWrap);
				if (!wordWrap) {
					textObj->SetSize(v3(m_maxContentWidth, 0.0f, 0.0f));
					textObj->SetLayoutAttribute(TextLayoutAttribute_AdjustX,
								    false);
					textObj->SetLayoutAttribute(TextLayoutAttribute_AdjustY,
								    true);
					textObj->SetLayoutAttribute(TextLayoutAttribute_WordWrap,
								    true);
					textObj->SetLayoutAttribute(TextLayoutAttribute_Ellipsis,
								    true);
					textObj->SetLayoutAttribute(
						TextLayoutAttribute_MaxLineCount, (int32_t)2);
					textObj->SetLayoutAttribute(TextLayoutAttribute_LinePitch,
								    4.0f);
					textObj->SetStyleAttribute(TextStyleAttribute_Point, 0, 0,
								   22.0f);
					m_columns[0].width = prevW;
					return;
				}
			}
			m_columns[0].width = m_maxContentWidth;
		}
		RecalculateWidth(m_columns[0].width);
	}
}

void PopupListViewInstance::SetupItem(int32_t index, ListItem *item)
{
	item->SetMetaAlpha(0.0f, NULL, 65539, 0, 0.0f, NULL, NULL);
	if (item->m_layouter != NULL)
		delete item->m_layouter;
	PopupItemFadeLayouter *layouter = new PopupItemFadeLayouter();

	item->m_layouter = layouter;
	if (layouter != NULL)
		layouter->BindWidget(item);

	Widget *container = item;
	Widget *btnWidget = NULL;

	if (m_usesBuiltinItem != 0) {
		container = item->GetChild(0);
	} else if (m_customBtnChildIdx >= 0 &&
		   m_customBtnChildIdx < (int32_t)item->m_children.size()) {
		btnWidget = item->GetChild(m_customBtnChildIdx);
		container = btnWidget;
	} else if (m_customBtnChildIdx == -2) {
		m_customBtnChildIdx = -1;
		int32_t idx = 0;

		for (list<Widget *>::iterator it = item->m_children.begin();
		     it != item->m_children.end(); ++it, ++idx) {
			Widget *w = *it;

			if (w != NULL && w->IsInherit(Button::TypeName()) &&
			    w->m_adjust_x == 1 && w->m_adjust_y == 1) {
				btnWidget = w;
				container = w;
				m_customBtnChildIdx = idx;
				break;
			}
		}
	}

	if (index >= 0 && index < (int32_t)m_items.size())
		container->SetString(m_items[index].label);

	if (m_columns.empty()) {
		int32_t colCount = (int32_t)container->m_children.size();

		if (colCount == 0 && m_usesBuiltinItem != 0) {
			wstring labelText;

			container->GetString(labelText);
			if (!labelText.empty()) {
				colCount = 1;
				m_singleTextItem = true;
			}
		}
		m_columns.resize((size_t)colCount);
	}

	if (m_singleTextItem) {
		if (m_columns[0].width != 0.0f) {
			AdjustParam adj = (container->m_adjust_param != NULL) ?
						  *container->m_adjust_param :
						  ScePafWidget_BE1CEA2E;

			adj.m_left_margin = m_columns[0].posX;
			ScePafWidget_B1C23134(container, adj);
		}
		container->AddEventCallback(POPUP_EVENT_LAYOUT_DONE, OnPopupButtonTextLayout, this);
	} else {
		uint32_t colIdx = 0;

		for (list<Widget *>::iterator it = container->m_children.begin();
		     it != container->m_children.end() && colIdx < m_columns.size();
		     ++it, ++colIdx) {
			Widget *w = *it;

			m_columns[colIdx].align = w->m_align_x;
			if (m_columns[colIdx].width != 0.0f) {
				v4 p = w->GetPos(0, NULL, v4(0.0f, 0.0f, 0.0f, 0.0f));

				p.set_x(m_columns[colIdx].posX);
				w->SetPos(p, NULL, 0x10000, 0, 0.0f, NULL, NULL);
			}
			if (w != NULL && w->IsInherit(Text::TypeName())) {
				PopupColumnTextArg *arg = new PopupColumnTextArg;

				arg->owner = this;
				arg->columnIndex = colIdx;
				w->AddEventCallback(POPUP_EVENT_LAYOUT_DONE, OnPopupColumnTextLayout,
						    arg);
			}
		}
	}

	if (btnWidget == NULL) {
		if (m_usesBuiltinItem != 0) {
			btnWidget = container;
		} else {
			Button *btn = new Button(item, NULL);

			btn->m_name.SetID(string("popup_list_view_list_item"));
			btn->SetAdjust(1, 1, 0);
			if (!m_transparentBg)
				btn->SetColor(v4(0.0f, 0.0f, 0.0f, 0.0f), NULL, 65537, 0, 0.0f,
					      NULL, NULL);
			btnWidget = btn;
		}
	}

	btnWidget->AddEventCallback(ButtonBase::CB_BTN_DECIDE, OnPopupItemDecide, item);

	if (index >= 0 && index < (int32_t)m_items.size() && m_items[index].disabled) {
		SetButtonDisableColor((ButtonBase *)btnWidget, s_disabledColor);
		((ButtonBase *)btnWidget)->Disable(false);
	}

	int32_t texSlot = 0;

	if (m_itemCount != 1) {
		if (index == 0)
			texSlot = 1;
		else if (index == m_itemCount - 1)
			texSlot = 3;
		else
			texSlot = 2;
	}

	uint32_t normalTexHash = m_theme.itemTexHashes[2 * texSlot];

	if (normalTexHash != 0) {
		intrusive_ptr<Surface> tex = m_plugin->GetTexture(IDParam(normalTexHash));

		btnWidget->SetTexture(tex);
		DrawObj *obj = btnWidget->GetDrawObj(0);

		obj->SetColor(v4(1.0f, 1.0f, 1.0f, 1.0f));
	}

	uint32_t glowTexHash = m_theme.itemTexHashes[2 * texSlot + 1];

	if (glowTexHash != 0) {
		((ButtonBase *)btnWidget)->SetGlowObj(68);
		intrusive_ptr<Surface> glowTex = m_plugin->GetTexture(IDParam(glowTexHash));

		btnWidget->SetTexture(glowTex, 68, 0);
	}

	if (m_style == 2 || m_style == 3 || m_style == 4) {
		FocusShape shape;

		if (m_itemCount != 1) {
			if (index == 0) {
				shape.m_shape = 6;
				shape.m_rot = v3(0.0f, 0.0f, -POPUP_HALF_PI);
			} else if (index == m_itemCount - 1) {
				shape.m_shape = 6;
				shape.m_rot = v3(0.0f, 0.0f, POPUP_HALF_PI);
			} else {
				shape.m_shape = 2;
			}
		}
		shape.m_changed = true;
		btnWidget->m_focus_parts.push_back(shape);
	}

	if (index >= 0 && index < (int32_t)m_items.size() && m_items[index].icon.get() != NULL) {
		Plane *iconPlane = new Plane(btnWidget, NULL);
		Surface *surf = m_items[index].icon.get();

		iconPlane->SetPos(v4(m_iconPosX, 0.0f, 0.0f, 0.0f), NULL, 0x10000, 0, 0.0f, NULL,
				  NULL);
		iconPlane->SetSize(
			v4((float)surf->GetWidth(), (float)surf->GetHeight(), 0.0f, 0.0f), NULL,
			65540, 0, 0.0f, NULL, NULL);
		iconPlane->SetOrigin(1, iconPlane->m_origin_y, iconPlane->m_origin_z, NULL);
		iconPlane->SetTexture(m_items[index].icon, 0, 0);
	}

	if (m_checkMarkSide != 0 && index == m_initialSelectIndex) {
		Plane *checkPlane = new Plane(btnWidget, NULL);
		intrusive_ptr<Surface> checkTex =
			m_plugin->GetTexture(IDParam(POPUP_RCO_TEX_CHECKMARK));

		checkPlane->SetPos(v4(m_checkPosX, 0.0f, 0.0f, 0.0f), NULL, 0x10000, 0, 0.0f,
				   NULL, NULL);
		checkPlane->SetSize(
			v4((float)checkTex->GetWidth(), (float)checkTex->GetHeight(), 0.0f, 0.0f),
			NULL, 65540, 0, 0.0f, NULL, NULL);
		checkPlane->SetTexture(checkTex, 0, 0);
	}

	if (index >= 0 && index < (int32_t)m_items.size() && m_items[index].hasSubMenu) {
		Plane *subPlane = new Plane(btnWidget, NULL);

		subPlane->m_name.SetID(string("popup_list_view_sub_menu_mark"));
		subPlane->SetPos(v4(m_subMenuPosX, 0.0f, 0.0f, 0.0f), NULL, 0x10000, 0, 0.0f, NULL,
				 NULL);
		subPlane->SetSize(v4(46.0f, 46.0f, 0.0f, 0.0f), NULL, 65540, 0, 0.0f, NULL, NULL);
		subPlane->SetOrigin(2, subPlane->m_origin_y, subPlane->m_origin_z, NULL);
		intrusive_ptr<Surface> subTex =
			m_plugin->GetTexture(IDParam(POPUP_RCO_TEX_SUBMENU));

		subPlane->SetTexture(subTex, 0, 0);
	}
}

ListItem *PopupListItemFactory::Create(listview::ItemFactory::CreateParam &param)
{
	if (param.m_list_view->m_destroy_protected)
		return new ListItem(param.m_parent, NULL);

	ListItem *item = NULL;

	if (m_customCreateCb != NULL) {
		item = m_customCreateCb->Invoke(param);
	} else {
		Plugin::TemplateOpenParam tmplParam;

		m_plugin->TemplateOpen(param.m_parent, m_templateId, tmplParam);
		item = (ListItem *)param.m_parent->FindChild(IDParam(POPUP_RCO_ITEM_ROOT), 0);
		item->m_name.m_idhash = 0;
		v4 cellSz = param.m_list_view->GetCellSize(param.m_segment, param.m_cell);

		item->SetSize(cellSz, NULL, 65540, 0, 0.0f, NULL, NULL);
		if (m_customBindCb != NULL) {
			Widget *content = item->FindChild(IDParam(POPUP_RCO_ITEM_CONTENT), 0);

			m_customBindCb->Invoke(content, param.m_cell);
		}
	}
	m_owner->SetupItem(param.m_cell, item);
	return item;
}

/* ========================================================================= */
/* Public PopupListViewPush / PopupListViewPopTo                             */
/* ========================================================================= */

static int32_t ClosePopupInstance(PopupListViewInstance *inst, int32_t selectedIndex)
{
	int32_t retIdx = inst->BeginClose(selectedIndex);
	PopupListCloseCallback *closeCb = inst->m_closeCb;

	delete inst;
	if (!s_popupStack.empty())
		s_popupStack.pop_back();
	if (closeCb != NULL) {
		closeCb->Invoke(retIdx);
		delete closeCb;
	}
	if (!s_popupStack.empty()) {
		PopupListViewInstance *top = s_popupStack[s_popupStack.size() - 1];
		int32_t count = top->m_listView->GetGeneratedNum();

		for (int32_t i = 0; i < count; ++i) {
			listview::ListPos pos = top->m_listView->GetGeneratedPos(i);
			ListItem *item = top->m_listView->GetListItem(pos.m_segment, pos.m_cell);
			Widget *child = (item != NULL) ? item->GetChild(0) : NULL;

			if (child != NULL && child->IsInherit(Button::TypeName())) {
				int32_t cellIdx = item->GetCellIndex();

				if (cellIdx < (int32_t)top->m_items.size() &&
				    !top->m_items[cellIdx].disabled)
					((ButtonBase *)child)->Enable(false);
			}
		}
	}
	return 0;
}

static inline __attribute__((always_inline)) void
DestroyConfigCallbacks(const PopupListViewConfig &cfg)
{
	if (cfg.customCreateCb != NULL)
		delete (Callback0 *)cfg.customCreateCb;
	if (cfg.customBindCb != NULL)
		delete (Callback0 *)cfg.customBindCb;
	if (cfg.selectCb != NULL)
		delete cfg.selectCb;
	if (cfg.closeCb != NULL)
		delete cfg.closeCb;
}

int32_t PopupListViewPush(const PopupListViewConfig &cfg, uint32_t depth, int32_t parentItemIdx)
{
	if (depth > s_popupStack.size()) {
		DestroyConfigCallbacks(cfg);
		return FRIEND_SELECT_ERROR_INVALID_ARG;
	}
	if (depth < s_popupStack.size()) {
		DestroyConfigCallbacks(cfg);
		return FRIEND_SELECT_ERROR_BUSY;
	}
	if (cfg.plugin == NULL ||
	    (cfg.itemCount <= 0 && cfg.legacyLabels.empty() && cfg.items.empty())) {
		DestroyConfigCallbacks(cfg);
		return FRIEND_SELECT_ERROR_INVALID_ARG;
	}

	PopupSubMenuAnchor subAnchor;

	subAnchor.active = false;
	if (!s_popupStack.empty()) {
		PopupListViewInstance *prev = s_popupStack[s_popupStack.size() - 1];

		prev->DisableAllItems();
		subAnchor.active = true;
		subAnchor.pos = prev->GetSubMenuAnchorPos(parentItemIdx);
	}

	char suffix[64];

	sce_paf_snprintf(suffix, sizeof(suffix), "%u", (unsigned int)s_popupStack.size());
	string pageName = string("page_popup_list_view") + string(suffix);
	IDParam pageId(pageName);
	PopupListViewInstance *inst =
		new PopupListViewInstance(pageId, cfg, subAnchor, s_popupNextId++);

	s_popupStack.push_back(inst);
	return 0;
}

void PopupListViewPopTo(int32_t depth)
{
	if ((uint32_t)(depth + 1) <= s_popupStack.size())
		ClosePopupInstance(s_popupStack[s_popupStack.size() - 1], -1);
}
