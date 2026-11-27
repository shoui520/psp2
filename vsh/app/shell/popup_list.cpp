/*
 * popup_list.cpp: shell popup list view manager ("page_popup_list_view"),
 * supporting stackable menus, callout anchoring, dynamic item sizing,
 * selection/close callbacks, and input locking (0x813450BC..0x81348E6F).
 */
#include "popup_list.h"

#include <stddef.h>
#include <scetypes.h>
#include <framework.h>
#include <plugin.h>
#include <std/string>
#include <std/vector>
#include <std/string.h>
#include <std/stdio.h>
#include <src/std/math.h>
#include <math/math.h>
#include <versatileparam/idparam.h>
#include <timer/timer.h>
#include <inputdevice/inputdevice.h>
#include <common/transition.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <graphics/text/text_layout.h>
#include <graphics/text/text_style.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/core/layouter.h>
#include <widget/w_scene.h>
#include <widget/w_camera.h>
#include <widget/w_list_view.h>
#include <widget/w_list_item.h>
#include <widget/w_button.h>
#include <widget/w_button_base.h>
#include <widget/w_plane.h>
#include <widget/w_text.h>

using namespace paf;

typedef __typeof__(static_cast<ui::ListView *>(NULL)->m_min_size) PafVec2;
typedef __typeof__(static_cast<ui::FocusShape *>(NULL)->m_rot) PafVec3;
typedef __typeof__(static_cast<ui::Widget *>(NULL)->m_pos) PafVec4;

/* Popup list type kinds */
#define POPUP_LIST_TYPE_DIALOG		0
#define POPUP_LIST_TYPE_SHEET		1
#define POPUP_LIST_TYPE_FLOATING	2
#define POPUP_LIST_TYPE_CALLOUT		3
#define POPUP_LIST_TYPE_CALLOUT_ALT	4

/* Checkmark placement sides */
#define POPUP_LIST_CHECK_NONE		0
#define POPUP_LIST_CHECK_LEFT		1
#define POPUP_LIST_CHECK_RIGHT		2

/* Resource template and widget ID hashes */
#define HASH_TMPL_DIALOG		0x2EFE3A6Fu	/* 788421615 */
#define HASH_TMPL_SHEET			0x84C19EF8u	/* -2067674376 */
#define HASH_TMPL_FLOATING		0xEC0579BFu	/* -335187857 */
#define HASH_TMPL_CALLOUT		0x81EC05FAu	/* -2115230406 */
#define HASH_TMPL_ITEM_DIALOG		0x5CCE5AE4u	/* 1557007140 */
#define HASH_TMPL_ITEM_FLOATING		0x47506FE8u	/* 1196488232 */

#define HASH_WIDGET_BG_DIM		0xF0671EC6u	/* -261680506 */
#define HASH_WIDGET_FRAME		0xABF21A3Au	/* -1410190022 */
#define HASH_WIDGET_TITLE		0x20FF24DCu	/* 553605212 */
#define HASH_WIDGET_LIST_VIEW		0xD41FB459u	/* -736126631 */
#define HASH_WIDGET_CANCEL_BTN		0xC4262E54u	/* -1004160460 */
#define HASH_WIDGET_LIST_ITEM		0x8ACB68E5u	/* -1966360219 */
#define HASH_WIDGET_ITEM_BUTTON		0x8CFA4DF7u	/* -1929731273 */

#define HASH_TEX_CHECKMARK		0x3E7DB4E9u	/* 1048466153 */
#define HASH_TEX_SUBMENU_ARROW		0x1845AD4Du	/* 407228685 */
#define HASH_TEX_DIALOG_BG		0x28224A4Fu	/* 673334607 */
#define HASH_TEX_DIALOG_GLOW		0xA7F53902u	/* -1477059662 */
#define HASH_TEX_SHEET_GLOW		0x7C121D10u	/* 2081547280 */

/* Error codes */
#define POPUP_LIST_ERR_INVALID_PARAM	(-2136014705)	/* 0x80AF008F */
#define POPUP_LIST_ERR_BUSY		(-2136014815)	/* 0x80AF0021 */

/* Device mask for locking all input while opening */
#define POPUP_LIST_INPUT_LOCK_MASK	0x7Fu

/* Default angle-based callout placement entry (28 bytes, 0x8151ECBC) */
struct PopupListAngleCallout {
	float maxAngle;
	SceUInt32 alignX;
	SceUInt32 alignY;
	float offsetX;
	float offsetY;
	SceUInt32 textureHash;
	bool flipHorizontal;
};

static const PopupListAngleCallout s_defaultAngleCallouts[6] = {
	{ -2.3561945f, ui::Widget::ALIGN_LEFT,   ui::Widget::ALIGN_BOTTOM,  22.0f, -21.0f, 0x4EB6E6DCu, true  },
	{ -0.7853982f, ui::Widget::ALIGN_CENTER, ui::Widget::ALIGN_BOTTOM,  12.0f,  21.0f, 0x581892BAu, false },
	{  0.0000000f, ui::Widget::ALIGN_RIGHT,  ui::Widget::ALIGN_BOTTOM, -22.0f, -21.0f, 0x4EB6E6DCu, false },
	{  0.7853982f, ui::Widget::ALIGN_RIGHT,  ui::Widget::ALIGN_TOP,    -22.0f,  21.0f, 0x97764502u, false },
	{  2.3561945f, ui::Widget::ALIGN_CENTER, ui::Widget::ALIGN_TOP,    -12.0f, -21.0f, 0xA4E24C7Bu, true  },
	{  3.1415927f, ui::Widget::ALIGN_LEFT,   ui::Widget::ALIGN_TOP,     22.0f,  21.0f, 0x97764502u, true  },
};

static const PopupListAngleCallout s_sideCallouts[2] = {
	{ 0.0f, ui::Widget::ALIGN_LEFT,  ui::Widget::ALIGN_CENTER,  23.0f, 0.0f, 0x8D37C3F6u, true  },
	{ 0.0f, ui::Widget::ALIGN_RIGHT, ui::Widget::ALIGN_CENTER, -23.0f, 0.0f, 0x8D37C3F6u, false },
};

/* Masked direction callout table (4 entries, 0x8151ED9C) */
static const ShellPopupListCalloutRule s_maskedCallouts[4] = {
	{ 0x010u,  0.7853982f,  2.3561945f, ui::Widget::ALIGN_CENTER, ui::Widget::ALIGN_TOP,     12.0f, -21.0f, 0xA4E24C7Bu, false },
	{ 0x020u,  0.7853982f,  2.3561945f, ui::Widget::ALIGN_CENTER, ui::Widget::ALIGN_TOP,    -12.0f, -21.0f, 0xA4E24C7Bu, true  },
	{ 0x100u, -2.3561945f, -0.7853982f, ui::Widget::ALIGN_CENTER, ui::Widget::ALIGN_BOTTOM,  12.0f,  21.0f, 0x581892BAu, false },
	{ 0x200u, -2.3561945f, -0.7853982f, ui::Widget::ALIGN_CENTER, ui::Widget::ALIGN_BOTTOM, -12.0f,  21.0f, 0x581892BAu, true  },
};

/* Floating / callout item button textures: [single, top, middle, bottom] x [normal, glow] */
static const SceUInt32 s_floatingItemTextures[4][2] = {
	{ 0x8C2646D7u, 0x8CFD89BFu },
	{ 0x7717044Eu, 0x5BC99FC5u },
	{ 0x6D3F263Eu, 0xC54CDDAAu },
	{ 0xA80B82F6u, 0xDAED2DE8u },
};

static const SceUInt32 s_calloutAltItemTextures[4][2] = {
	{ 0x00000000u, 0x00000000u },
	{ 0xE5F16C78u, 0x5BC99FC5u },
	{ 0x59CB789Cu, 0xC54CDDAAu },
	{ 0x0DF6ED07u, 0xDAED2DE8u },
};

/* Callback wrappers for C function pointers (vtables 0x8153D06C, 0x8153D080) */
class PopupListSelectFuncCallback : public ShellPopupListSelectCallback {
public:
	PopupListSelectFuncCallback(ShellPopupListSelectFunc func, void *userContext)
		: m_func(func), m_userContext(userContext)
	{
	}

	virtual ~PopupListSelectFuncCallback()
	{
	}

	virtual void OnSelect(int index, const ShellPopupListItem *item)
	{
		if (m_func != NULL)
			m_func(index, item != NULL ? item->value : 0, m_userContext);
	}

	ShellPopupListSelectFunc m_func;
	void *m_userContext;
};

class PopupListCloseFuncCallback : public ShellPopupListCloseCallback {
public:
	PopupListCloseFuncCallback(ShellPopupListCloseFunc func, void *userContext)
		: m_func(func), m_userContext(userContext)
	{
	}

	virtual ~PopupListCloseFuncCallback()
	{
	}

	virtual void OnClose(int result)
	{
		if (m_func != NULL)
			m_func(result, m_userContext);
	}

	ShellPopupListCloseFunc m_func;
	void *m_userContext;
};

/* 0x813450BC */
void ShellPopupListParam::SetSelectCallback(ShellPopupListSelectFunc func, void *userContext)
{
	delete selectCallback;
	selectCallback = new PopupListSelectFuncCallback(func, userContext);
}

/* 0x813450F4 */
void ShellPopupListParam::SetCloseCallback(ShellPopupListCloseFunc func, void *userContext)
{
	delete closeCallback;
	closeCallback = new PopupListCloseFuncCallback(func, userContext);
}

/* Column layout tracking entry for child widgets inside each popup item (12 bytes) */
struct PopupListColumnMetric {
	PopupListColumnMetric() : alignX(0), posX(0.0f), maxWidth(0.0f)
	{
	}

	SceUInt32 alignX;
	float posX;
	float maxWidth;
};

/* Anchor handoff from parent popup submenu mark to child popup */
struct PopupListSubAnchor {
	bool valid;
	PafVec2 position;
};

/* Layouter that fades each newly constructed list item in after 2 ready frames (0x81348B60) */
class PopupListItemFadeLayouter : public ui::Layouter {
public:
	PopupListItemFadeLayouter() : ui::Layouter(NULL, NULL), m_readyFrames(0)
	{
	}

	virtual ~PopupListItemFadeLayouter()
	{
	}

	/* 0x81348B60 */
	virtual int32_t OnUpdate_core()
	{
		if (m_readyFrames < 2) {
			if (m_widget->m_prepare_count != 0) {
				m_readyFrames = 0;
			} else {
				m_readyFrames++;
				if (m_readyFrames == 2) {
					Timer *fadeTimer = new Timer(0.0f);
					m_widget->SetMetaAlpha(1.0f, fadeTimer,
							       ui::Widget::ANIM_META_ALPHA,
							       0, 0.0f, NULL, NULL);
				}
			}
		}
		return 0;
	}

	int m_readyFrames;
};

class PopupListInstance;

/* Global stack of currently open popup lists (0x815C6360) */
static vector<PopupListInstance *> s_popupListStack;
/* Monotonic instance identifier (0x815C6370) */
static SceUInt32 s_nextInstanceId = 0;
/* Color applied to disabled popup list buttons (0x815C6378) */
static PafVec4 s_disabledItemColor(1.0f, 1.0f, 1.0f, 1.0f);
/* Submenu arrow anchor offset (0x815C6388) */
static PafVec2 s_subMenuAnchorOffset(12.0f, 0.0f);

/* Active popup list instance (244 bytes) */
class PopupListInstance {
public:
	PopupListInstance(const IDParam &pageId, ShellPopupListParam *param,
			  const PopupListSubAnchor *subAnchor, SceUInt32 instanceId);
	~PopupListInstance();

	int ClosePage(int resultIndex);
	void ApplyLayout(bool animate);
	void UpdateColumnMetrics(float requiredWidth);
	void ConfigureListItem(int cellIndex, ui::ListItem *listItem);
	PafVec2 GetSubMenuAnchorPos();
	void DisableVisibleButtons();
	void RestoreVisibleButtons();

	int m_type;
	Plugin *m_plugin;
	int m_itemCount;
	int m_selectedIndex;
	vector<ShellPopupListItem> m_items;
	float m_minContentWidth;
	float m_maxContentWidth;
	ShellPopupListSelectCallback *m_selectCallback;
	ShellPopupListCloseCallback *m_closeCallback;
	int m_checkMarkSide;
	float m_topPadding;
	float m_bottomPadding;
	float m_leftPadding;
	float m_rightPadding;
	float m_boundsLeft;
	float m_boundsRight;
	bool m_autoCloseOnSelect;
	bool m_showItemBg;
	bool m_playSe;
	bool m_wrapLongText;
	float m_titleMinFontSize;
	float m_itemMinFontSize;
	ShellPopupListStyle m_style;
	SceUInt32 m_instanceId;
	IDParam m_pageId;
	ui::Widget *m_pageWidget;
	ui::Widget *m_bgDimWidget;
	ui::Widget *m_frameWidget;
	ui::ListView *m_listView;
	float m_listMarginLeft;
	float m_listMarginRight;
	vector<PopupListColumnMetric> m_columnMetrics;
	float m_currentListWidth;
	float m_iconPosX;
	float m_checkMarkOffset;
	float m_checkMarkPosX;
	float m_subMenuMarkPosX;
	int m_layoutPhase;
	int m_useDefaultItemLayout;
	bool m_singleLabelMode;
	int m_customButtonChildIndex;
	int m_inputLockCount;
};

/* Context for deferred text column width measurement */
struct PopupListTextMeasureContext {
	PopupListInstance *instance;
	int columnIndex;
};

/* ListView item factory (36 bytes, vtable 0x8153D04C) */
class PopupListItemFactory : public ui::listview::ItemFactory {
public:
	PopupListItemFactory(PopupListInstance *owner, Plugin *plugin,
			     ui::listview::ItemFactory *customFactory,
			     ShellPopupListItemCallback *itemCallback,
			     const IDParam &templateId)
		: m_owner(owner), m_plugin(plugin), m_customFactory(customFactory),
		  m_itemCallback(itemCallback), m_templateId(templateId)
	{
	}

	virtual ~PopupListItemFactory()
	{
		delete m_customFactory;
		delete m_itemCallback;
	}

	/* 0x81348C00 */
	virtual ui::ListItem *Create(CreateParam &param)
	{
		if (param.m_list_view->m_destroy_protected)
			return new ui::ListItem(param.m_parent, NULL);

		ui::ListItem *itemWidget = NULL;
		if (m_customFactory != NULL) {
			itemWidget = m_customFactory->Create(param);
		} else {
			Plugin::TemplateOpenParam tmplParam;
			m_plugin->TemplateOpen(param.m_parent, m_templateId, tmplParam);

			IDParam itemId("", HASH_WIDGET_LIST_ITEM);
			itemWidget = static_cast<ui::ListItem *>(
				param.m_parent->FindChild(itemId, 0));
			itemWidget->m_name.m_idhash = 0;

			PafVec4 cellSize = param.m_list_view->GetCellSize(
				param.m_segment, param.m_cell);
			itemWidget->SetSize(cellSize, NULL, ui::Widget::ANIM_SIZE,
					    0, 0.0f, NULL, NULL);

			if (m_itemCallback != NULL) {
				IDParam buttonId("", HASH_WIDGET_ITEM_BUTTON);
				ui::Widget *btn = itemWidget->FindChild(buttonId, 0);
				(*m_itemCallback)(btn, param.m_cell);
			}
		}

		m_owner->ConfigureListItem(param.m_cell, itemWidget);
		return itemWidget;
	}

	virtual void Start(StartParam &param)
	{
		if (m_customFactory != NULL)
			m_customFactory->Start(param);
		else
			ui::listview::ItemFactory::Start(param);
	}

	virtual void Stop(StopParam &param)
	{
		if (m_customFactory != NULL)
			m_customFactory->Stop(param);
		else
			ui::listview::ItemFactory::Stop(param);
	}

	virtual void Dispose(DisposeParam &param)
	{
		if (m_customFactory != NULL)
			m_customFactory->Dispose(param);
		else
			ui::listview::ItemFactory::Dispose(param);
	}

	PopupListInstance *m_owner;
	Plugin *m_plugin;
	ui::listview::ItemFactory *m_customFactory;
	ShellPopupListItemCallback *m_itemCallback;
	IDParam m_templateId;
};

/* Forward declarations for static helpers and callbacks */
static int popupListDismiss(PopupListInstance *instance, int resultIndex);
static void popupListOnOutsideOrCancel(int32_t type, ui::Handler *self, ui::Event *event,
				       void *userContext);
static void popupListOnPageTerm(int32_t type, ui::Handler *self, ui::Event *event,
				void *userContext);
static void popupListOnTitleReady(int32_t type, ui::Handler *self, ui::Event *event,
				  void *userContext);
static void popupListOnFrameInterval(int32_t type, ui::Handler *self, ui::Event *event,
				     void *userContext);
static void popupListOnChildTextReady(int32_t type, ui::Handler *self, ui::Event *event,
				      void *userContext);
static void popupListOnClipTimerEnd(int32_t type, ui::Handler *self, ui::Event *event,
				    void *userContext);
static void popupListOnLabelTextReady(int32_t type, ui::Handler *self, ui::Event *event,
				      void *userContext);
static void popupListOnItemDecide(int32_t type, ui::Handler *self, ui::Event *event,
				  void *userContext);
static void popupListOnFocusPoll(int32_t type, ui::Handler *self, ui::Event *event,
				 void *userContext);

/* Check whether any widget up to root currently has a show hold active (0x81440B04) */
static bool isWidgetShowHeld(ui::Widget *widget)
{
	if (widget->m_show_hold != 0)
		return true;
	if (widget->m_parent != NULL && widget->m_parent != widget)
		return isWidgetShowHeld(widget->m_parent);
	return false;
}

/* Convert packed RGBA byte color to float RGBA vector and apply to widget (0x81440A30) */
static void setWidgetColorRgba(ui::Widget *widget, SceUInt32 rgba)
{
	float r = static_cast<float>(rgba & 0xFFu) / 255.0f;
	float g = static_cast<float>((rgba >> 8) & 0xFFu) / 255.0f;
	float b = static_cast<float>((rgba >> 16) & 0xFFu) / 255.0f;
	float a = static_cast<float>((rgba >> 24) & 0xFFu) / 255.0f;
	PafVec4 color(r, g, b, a);
	widget->SetColor(color, NULL, ui::Widget::ANIM_COLOR, 0, 0.0f, NULL, NULL);
}

/* Populate default item button textures based on popup type (0x8134512E) */
static void initDefaultItemTextures(ShellPopupListStyle *style, int listType)
{
	switch (listType) {
	case POPUP_LIST_TYPE_DIALOG:
		for (int i = 0; i < 4; ++i) {
			style->itemTextureHash[i][0] = HASH_TEX_DIALOG_BG;
			style->itemTextureHash[i][1] = HASH_TEX_DIALOG_GLOW;
		}
		break;
	case POPUP_LIST_TYPE_SHEET:
		for (int i = 0; i < 4; ++i) {
			style->itemTextureHash[i][0] = 0;
			style->itemTextureHash[i][1] = HASH_TEX_SHEET_GLOW;
		}
		break;
	case POPUP_LIST_TYPE_FLOATING:
	case POPUP_LIST_TYPE_CALLOUT:
	case POPUP_LIST_TYPE_CALLOUT_ALT: {
		const SceUInt32 (*src)[2] = (listType == POPUP_LIST_TYPE_CALLOUT_ALT)
						    ? s_calloutAltItemTextures
						    : s_floatingItemTextures;
		for (int i = 0; i < 4; ++i) {
			style->itemTextureHash[i][0] = src[i][0];
			style->itemTextureHash[i][1] = src[i][1];
		}
		break;
	}
	default:
		sce_paf_memset(style->itemTextureHash, 0, sizeof(style->itemTextureHash));
		break;
	}
}

/* Destroy popup list instance (inlined in 0x81346CE0 and 0x81346F04) */
PopupListInstance::~PopupListInstance()
{
	delete m_selectCallback;
	while (m_inputLockCount != 0) {
		inputdevice::Unlock(POPUP_LIST_INPUT_LOCK_MASK);
		m_inputLockCount--;
	}
}

/* Construct and open a popup list page and widgets (0x813451CA) */
PopupListInstance::PopupListInstance(const IDParam &pageId, ShellPopupListParam *param,
				     const PopupListSubAnchor *subAnchor, SceUInt32 instanceId)
	: m_type(param->type),
	  m_plugin(param->plugin),
	  m_itemCount(param->selected),
	  m_selectedIndex(param->focus),
	  m_items(param->items),
	  m_minContentWidth(param->minSize[0]),
	  m_maxContentWidth(param->maxSize[0]),
	  m_selectCallback(param->selectCallback),
	  m_closeCallback(param->closeCallback),
	  m_checkMarkSide(param->checkMarkSide),
	  m_topPadding(param->offset[0]),
	  m_bottomPadding(param->offset[1]),
	  m_leftPadding(param->padding[0]),
	  m_rightPadding(param->padding[1]),
	  m_boundsLeft(param->bounds[2]),
	  m_boundsRight(param->bounds[3]),
	  m_autoCloseOnSelect(param->show[0]),
	  m_showItemBg(param->show[1]),
	  m_playSe(param->show[2]),
	  m_wrapLongText(param->show[3]),
	  m_titleMinFontSize(param->extra[0]),
	  m_itemMinFontSize(param->extra[1]),
	  m_instanceId(instanceId),
	  m_pageId(pageId),
	  m_pageWidget(NULL),
	  m_bgDimWidget(NULL),
	  m_frameWidget(NULL),
	  m_listView(NULL),
	  m_listMarginLeft(0.0f),
	  m_listMarginRight(0.0f),
	  m_currentListWidth(0.0f),
	  m_iconPosX(0.0f),
	  m_checkMarkOffset(32.0f),
	  m_checkMarkPosX(0.0f),
	  m_subMenuMarkPosX(0.0f),
	  m_layoutPhase(0),
	  m_useDefaultItemLayout(param->customFactory == NULL ? 1 : 0),
	  m_singleLabelMode(false),
	  m_customButtonChildIndex(-2),
	  m_inputLockCount(0)
{
	if (m_checkMarkSide == POPUP_LIST_CHECK_LEFT && m_leftPadding < 64.0f)
		m_leftPadding = 64.0f;
	if (m_checkMarkSide == POPUP_LIST_CHECK_RIGHT && m_rightPadding < 64.0f)
		m_rightPadding = 64.0f;

	if (m_items.size() == 0 && param->texts.size() != 0) {
		ShellPopupListItem emptyItem;
		m_items.resize(param->texts.size(), emptyItem);
		for (size_t i = 0; i < m_items.size(); ++i)
			m_items[i].text = param->texts[i];
	}

	if (m_items.size() != 0) {
		m_itemCount = static_cast<int>(m_items.size());
		m_useDefaultItemLayout = 1;
	}
	if (m_selectedIndex >= m_itemCount)
		m_selectedIndex = -1;

	if (param->styleOverride != NULL) {
		m_style = *param->styleOverride;
	} else if (m_type != POPUP_LIST_TYPE_DIALOG || m_showItemBg) {
		initDefaultItemTextures(&m_style, m_type);
	} else {
		sce_paf_memset(m_style.itemTextureHash, 0, sizeof(m_style.itemTextureHash));
	}

	if (m_items.size() != 0) {
		bool hasIcon = false;
		bool hasSubMenu = false;
		for (size_t i = 0; i < m_items.size(); ++i) {
			if (m_items[i].icon != NULL)
				hasIcon = true;
			if (m_items[i].hasSubMenu)
				hasSubMenu = true;
			if (hasIcon && hasSubMenu)
				break;
		}
		if (hasIcon) {
			m_iconPosX = m_leftPadding + 24.0f;
			m_leftPadding += 56.0f;
			m_checkMarkOffset += 56.0f;
		}
		if (hasSubMenu) {
			m_subMenuMarkPosX = -m_rightPadding - 23.0f;
			m_rightPadding += 66.0f;
		}
	}

	Plugin::PageOpenParam pageParam;
	pageParam.overwrite_draw_priority = param->flags;
	pageParam.option = Plugin::PageOption_Create;
	m_pageWidget = m_plugin->PageOpen(pageId, pageParam);
	m_pageWidget->AddEventCallback(ui::Handler::CB_STATE_TERM, popupListOnPageTerm, this);

	float horizontalPadding = m_leftPadding + m_rightPadding;
	m_minContentWidth -= horizontalPadding;
	if (m_minContentWidth <= 0.0f)
		m_minContentWidth = 0.0f;

	m_maxContentWidth -= horizontalPadding;
	if (m_maxContentWidth <= 0.0f)
		m_maxContentWidth = m_pageWidget->GetSize(ui::Widget::GET_CURRENT)->extract_x() * 0.8f;

	SceUInt32 templateHash;
	if (m_type == POPUP_LIST_TYPE_DIALOG)
		templateHash = HASH_TMPL_DIALOG;
	else if (m_type == POPUP_LIST_TYPE_SHEET)
		templateHash = HASH_TMPL_SHEET;
	else if (m_type == POPUP_LIST_TYPE_FLOATING)
		templateHash = HASH_TMPL_FLOATING;
	else
		templateHash = HASH_TMPL_CALLOUT;

	IDParam tmplId("", templateHash);
	Plugin::TemplateOpenParam tmplParam;
	m_plugin->TemplateOpen(m_pageWidget, tmplId, tmplParam);

	IDParam bgDimId("", HASH_WIDGET_BG_DIM);
	m_bgDimWidget = m_pageWidget->FindChild(bgDimId, 0);
	if (m_bgDimWidget != NULL) {
		m_bgDimWidget->SetAdjust(1u, 1u, 1u);
		ScePafWidget_42C5A4D4(m_bgDimWidget, 0, 0);
		ScePafCommon_6CF3A3C8(0.0f, m_bgDimWidget, common::transition::Type_FadeinSlow,
				      false, false);
	}

	float rowHeight = (m_type == POPUP_LIST_TYPE_FLOATING ||
			   m_type == POPUP_LIST_TYPE_CALLOUT ||
			   m_type == POPUP_LIST_TYPE_CALLOUT_ALT)
				  ? 60.0f
				  : 70.0f;

	float maxHeight = param->maxSize[1];
	if (maxHeight <= 0.0f)
		maxHeight = m_pageWidget->GetSize(ui::Widget::GET_CURRENT)->extract_y() * 0.9f;

	float verticalChrome = m_topPadding + m_bottomPadding;
	if (m_type == POPUP_LIST_TYPE_DIALOG)
		verticalChrome += 88.0f;

	float fullListHeight = rowHeight * static_cast<float>(m_itemCount);
	float maxListHeight = maxHeight - verticalChrome;
	float listHeight = fullListHeight < maxListHeight ? fullListHeight : maxListHeight;
	float minListHeight = param->minSize[1] - verticalChrome;
	if (listHeight < minListHeight)
		listHeight = minListHeight;

	bool dialogClamped = false;
	if (m_type == POPUP_LIST_TYPE_DIALOG && listHeight < fullListHeight) {
		dialogClamped = true;
		listHeight = static_cast<float>((static_cast<int>(listHeight) + 31) & ~31);
	}

	float frameHeight = listHeight + verticalChrome;

	IDParam frameId("", HASH_WIDGET_FRAME);
	m_frameWidget = m_pageWidget->FindChild(frameId, 0);
	m_frameWidget->Hide(0xFFu, NULL, NULL, 0.0f);
	m_frameWidget->SetIntervalEvent(0, 0.0f, popupListOnFrameInterval, this);
	inputdevice::Lock(POPUP_LIST_INPUT_LOCK_MASK);
	m_inputLockCount++;

	if ((m_style.bgColorFlags & 1u) != 0)
		setWidgetColorRgba(m_frameWidget, m_style.bgColorRgba);

	if (m_type == POPUP_LIST_TYPE_CALLOUT || m_type == POPUP_LIST_TYPE_CALLOUT_ALT) {
		PafVec2 anchorPos(0.0f, 0.0f);
		float angle = 0.0f;

		if (param->anchor != NULL) {
			PafVec4 worldPos = param->anchor->GetWorldMatrix().w();
			if (worldPos.extract_z() != 0.0f) {
				ui::Camera *cam = param->anchor->m_scene->m_camera;
				math::matrix mvp = cam->m_projection * cam->m_view;
				PafVec4 clip(mvp.Transform(worldPos.getImpl()));
				if (clip.extract_w() == 0.0f) {
					worldPos = PafVec4(0.0f, 0.0f, 0.0f, 0.0f);
				} else {
					float invW = 1.0f / clip.extract_w();
					PafVec4 ndc(clip.extract_x() * invW,
						    clip.extract_y() * invW,
						    clip.extract_z() * invW,
						    clip.extract_w() * invW);
					math::rect vp(param->anchor->m_scene->m_screen->vp);
					PafVec2 vpSize = vp.size();
					worldPos = PafVec4(ndc.extract_x() * vpSize.extract_x() * 0.5f,
							   ndc.extract_y() * vpSize.extract_y() * 0.5f,
							   0.0f, 0.0f);
				}
			}
			float px = worldPos.extract_x();
			float py = worldPos.extract_y();
			angle = sce_paf_atan2f(py, px);
			anchorPos = PafVec2(px - sce_paf_cosf(angle) * param->anchorOffset[0],
					    py - sce_paf_sinf(angle) * param->anchorOffset[0]);
		} else {
			if (subAnchor->valid)
				anchorPos = subAnchor->position;
			else
				anchorPos = PafVec2(param->origin[0], param->origin[1]);
			angle = sce_paf_atan2f(anchorPos.extract_y(), anchorPos.extract_x());
		}

		SceUInt32 dirMask = static_cast<SceUInt32>(param->anchorOffset[1]);
		SceUInt32 alignX = 0;
		SceUInt32 alignY = 0;
		SceUInt32 texHash = 0;
		bool flipHoriz = false;

		if (dirMask != 0 || m_style.customCallouts.size() != 0) {
			const ShellPopupListCalloutRule *begin = s_maskedCallouts;
			const ShellPopupListCalloutRule *end = s_maskedCallouts + 4;
			if (m_style.customCallouts.size() != 0) {
				begin = &m_style.customCallouts[0];
				end = begin + m_style.customCallouts.size();
				if (dirMask == 0)
					dirMask = 0xFFFFFFFFu;
			}

			float bestDist = 3.4028235e38f;
			const ShellPopupListCalloutRule *bestRule = NULL;
			for (const ShellPopupListCalloutRule *it = begin; it != end; ++it) {
				if ((it->directionMask & dirMask) == 0)
					continue;
				float dist;
				if (angle > it->maxAngle)
					dist = angle - it->maxAngle;
				else if (angle >= it->minAngle)
					dist = 0.0f;
				else
					dist = it->minAngle - angle;
				if (dist < bestDist) {
					bestDist = dist;
					bestRule = it;
				}
			}

			alignX = bestRule->alignX;
			alignY = bestRule->alignY;
			anchorPos = PafVec2(anchorPos.extract_x() + bestRule->offsetX,
					    anchorPos.extract_y() + bestRule->offsetY);
			texHash = bestRule->textureHash;
			flipHoriz = bestRule->flipHorizontal;
		} else {
			const PopupListAngleCallout *chosen = NULL;
			if (anchorPos.extract_y() >= 100.0f || anchorPos.extract_y() <= -100.0f) {
				for (int i = 0; i < 6; ++i) {
					if (angle < s_defaultAngleCallouts[i].maxAngle) {
						chosen = &s_defaultAngleCallouts[i];
						break;
					}
				}
			} else {
				chosen = &s_sideCallouts[anchorPos.extract_x() >= 0.0f ? 1 : 0];
			}

			alignX = chosen->alignX;
			alignY = chosen->alignY;
			anchorPos = PafVec2(anchorPos.extract_x() + chosen->offsetX,
					    anchorPos.extract_y() + chosen->offsetY);
			texHash = chosen->textureHash;
			flipHoriz = chosen->flipHorizontal;
		}

		IDParam texId("", texHash);
		intrusive_ptr<graph::Surface> tex = m_plugin->GetTexture(texId);
		m_frameWidget->SetTexture(tex);
		if (flipHoriz) {
			graph::DrawObj *bgObj = m_frameWidget->GetDrawObj(0);
			bgObj->SetRot(PafVec3(0.0f, 3.1415927f, 0.0f));
		}

		float topEdge = 0.0f;
		float bottomEdge = 0.0f;
		if (alignY == ui::Widget::ALIGN_TOP) {
			topEdge = anchorPos.extract_y();
			bottomEdge = anchorPos.extract_y() - frameHeight;
		} else if (alignY == ui::Widget::ALIGN_BOTTOM) {
			topEdge = anchorPos.extract_y() + frameHeight;
			bottomEdge = anchorPos.extract_y();
		} else {
			topEdge = anchorPos.extract_y() + frameHeight * 0.5f;
			bottomEdge = anchorPos.extract_y() - frameHeight * 0.5f;
		}

		float clampShiftY = 0.0f;
		if (topEdge > param->bounds[0])
			clampShiftY = param->bounds[0] - topEdge;
		else if (bottomEdge < param->bounds[1])
			clampShiftY = param->bounds[1] - bottomEdge;

		PafVec4 framePos(sce_paf_roundf(anchorPos.extract_x()),
				 anchorPos.extract_y() + clampShiftY,
				 param->origin[2], 0.0f);
		m_frameWidget->SetPos(framePos, NULL, ui::Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
		m_frameWidget->SetAlign(alignX, m_frameWidget->m_align_y,
					m_frameWidget->m_align_z, NULL);
		m_frameWidget->SetAlign(m_frameWidget->m_align_x, alignY,
					m_frameWidget->m_align_z, NULL);

		m_listMarginLeft = 7.0f;
		m_listMarginRight = 7.0f;
		PafVec4 frameSize = *m_frameWidget->GetSize(ui::Widget::GET_CURRENT);
		frameSize.set_y(frameHeight + 14.0f);
		m_frameWidget->SetSize(frameSize, NULL, ui::Widget::ANIM_SIZE,
				       0, 0.0f, NULL, NULL);
	} else if (m_type != POPUP_LIST_TYPE_SHEET) {
		if (m_type == POPUP_LIST_TYPE_FLOATING) {
			m_listMarginLeft = 7.0f;
			m_listMarginRight = 7.0f;
			frameHeight += 14.0f;
		}

		PafVec4 framePos(param->origin[0], param->origin[1],
				 param->origin[2], param->origin[3]);
		if (m_type == POPUP_LIST_TYPE_DIALOG && dialogClamped) {
			float topY = framePos.extract_y();
			if (param->margin[1] == 0)
				topY += frameHeight * 0.5f;
			else if (param->margin[1] == 1)
				topY += frameHeight;
			float relTop = topY + 272.0f - 80.0f;
			float snapped = static_cast<float>((static_cast<int>(relTop) + 31) & ~31);
			framePos.set_y(framePos.extract_y() + (snapped - relTop));
		}

		m_frameWidget->SetPos(framePos, NULL, ui::Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
		m_frameWidget->SetAlign(param->margin[0], m_frameWidget->m_align_y,
					m_frameWidget->m_align_z, NULL);
		m_frameWidget->SetAlign(m_frameWidget->m_align_x, param->margin[1],
					m_frameWidget->m_align_z, NULL);
		m_frameWidget->SetAnchor(param->columns, m_frameWidget->m_anchor_y,
					 m_frameWidget->m_anchor_z, NULL);
		m_frameWidget->SetAnchor(m_frameWidget->m_anchor_x, param->rows,
					 m_frameWidget->m_anchor_z, NULL);

		PafVec4 frameSize = *m_frameWidget->GetSize(ui::Widget::GET_CURRENT);
		frameSize.set_y(frameHeight);
		m_frameWidget->SetSize(frameSize, NULL, ui::Widget::ANIM_SIZE,
				       0, 0.0f, NULL, NULL);
	}

	if (m_type == POPUP_LIST_TYPE_SHEET) {
		m_frameWidget->SetPos(PafVec4(0.0f, 0.0f, 0.0f, 0.0f), NULL,
				      ui::Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
		ScePafWidget_44B22ACA(m_frameWidget, common::transition::Type_SlideFromRight1,
				      0.0f, NULL, NULL);
		ScePafWidget_42C5A4D4(m_frameWidget, 0, 1);
		m_frameWidget->AddEventCallback(ui::Handler::CB_MODAL_TOUCH_OUTSIDE,
						popupListOnOutsideOrCancel, NULL);
	}

	if (m_type == POPUP_LIST_TYPE_FLOATING ||
	    m_type == POPUP_LIST_TYPE_CALLOUT ||
	    m_type == POPUP_LIST_TYPE_CALLOUT_ALT) {
		ScePafWidget_42C5A4D4(m_frameWidget, 0, 0);
		m_frameWidget->AddEventCallback(ui::Handler::CB_MODAL_TOUCH_OUTSIDE,
						popupListOnOutsideOrCancel, NULL);
	}

	if (m_type == POPUP_LIST_TYPE_DIALOG) {
		IDParam titleId("", HASH_WIDGET_TITLE);
		ui::Widget *titleWidget = m_frameWidget->FindChild(titleId, 0);
		titleWidget->SetString(param->title);
		titleWidget->AddEventCallback(ui::Handler::CB_STATE_READY,
					      popupListOnTitleReady, this);
	}

	IDParam listViewId("", HASH_WIDGET_LIST_VIEW);
	m_listView = static_cast<ui::ListView *>(m_pageWidget->FindChild(listViewId, 0));

	PafVec4 listPos = m_listView->GetPos(ui::Widget::GET_CURRENT, NULL,
					     PafVec4(0.0f, 0.0f, 0.0f, 0.0f));
	listPos.set_x(listPos.extract_x() + (m_listMarginLeft - m_listMarginRight) * 0.5f);
	m_listView->SetPos(listPos, NULL, ui::Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);

	PafVec4 listSize = *m_listView->GetSize(ui::Widget::GET_CURRENT);
	listSize.set_y(listHeight);
	m_listView->SetSize(listSize, NULL, ui::Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);

	ui::listview::ItemFactory *customFactory = param->customFactory;
	if (m_items.size() != 0) {
		delete customFactory;
		customFactory = NULL;
	}

	IDParam itemTemplateId("", m_type > POPUP_LIST_TYPE_SHEET
					   ? HASH_TMPL_ITEM_FLOATING
					   : HASH_TMPL_ITEM_DIALOG);
	PopupListItemFactory *factory = new PopupListItemFactory(
		this, param->plugin, customFactory, param->itemCallback, itemTemplateId);
	m_listView->SetItemFactory(factory);
	m_listView->InsertSegment(0, 1u);
	m_listView->SetCellSizeDefault(0, PafVec4(0.0f, rowHeight, 0.0f, 0.0f));
	m_listView->InsertCell(0, 0, static_cast<uint32_t>(m_itemCount));
	m_listView->SetFocus(0, m_selectedIndex, ui::ListView::FOCUS_ALIGN_TYPE_CENTER, NULL);
	m_listView->SetIntervalEvent(0, 0.0f, popupListOnFocusPoll, this);

	IDParam cancelBtnId("", HASH_WIDGET_CANCEL_BTN);
	ui::Widget *cancelBtn = m_pageWidget->FindChild(cancelBtnId, 0);
	if (cancelBtn != NULL) {
		ScePafWidget_21125870(cancelBtn, 32u);
		cancelBtn->AddEventCallback(ui::ButtonBase::CB_BTN_DECIDE,
					    popupListOnOutsideOrCancel, NULL);
	}

	if (m_frameWidget != NULL &&
	    (m_type == POPUP_LIST_TYPE_FLOATING || m_type == POPUP_LIST_TYPE_CALLOUT)) {
		ui::Button *backKeyBtn = new ui::Button(m_frameWidget, NULL);
		ScePafWidget_21125870(backKeyBtn, 64u);
		backKeyBtn->m_keycode_capture = true;
		backKeyBtn->AddEventCallback(ui::ButtonBase::CB_BTN_DECIDE,
					     popupListOnOutsideOrCancel, NULL);
		backKeyBtn->Hide(0xFFu, NULL, NULL, 0.0f);
	}
}

/* Compute anchor position of the first visible item's submenu arrow (0x81346550) */
PafVec2 PopupListInstance::GetSubMenuAnchorPos()
{
	PafVec2 anchorPos(0.0f, 0.0f);
	ui::listview::ListPos genPos = m_listView->GetGeneratedPos(0);
	ui::ListItem *item = m_listView->GetListItem(genPos.m_segment, genPos.m_cell);
	if (item == NULL)
		return anchorPos;

	ui::Widget *child = item->GetChild(0);
	if (child == NULL || !child->IsInherit(ui::Button::TypeName()))
		return anchorPos;

	IDParam markId("popup_list_view_sub_menu_mark");
	ui::Widget *markWidget = child->FindChild(markId, 0);
	if (markWidget != NULL) {
		PafVec4 markWorld = markWidget->GetWorldMatrix().w();
		anchorPos = PafVec2(markWorld.extract_x() + s_subMenuAnchorOffset.extract_x(),
				    markWorld.extract_y() + s_subMenuAnchorOffset.extract_y());
	}
	return anchorPos;
}

/* Disable all currently generated item buttons when a child submenu opens (0x8134667C) */
void PopupListInstance::DisableVisibleButtons()
{
	int generatedNum = m_listView->GetGeneratedNum();
	for (int i = 0; i < generatedNum; ++i) {
		ui::listview::ListPos genPos = m_listView->GetGeneratedPos(i);
		ui::ListItem *item = m_listView->GetListItem(genPos.m_segment, genPos.m_cell);
		ui::Widget *child = item->GetChild(0);
		if (child != NULL && child->IsInherit(ui::Button::TypeName())) {
			ui::ButtonBase *btn = static_cast<ui::ButtonBase *>(child);
			if (btn->m_disable_color != NULL)
				*btn->m_disable_color = s_disabledItemColor;
			else
				btn->m_disable_color = new PafVec4(s_disabledItemColor);
			btn->Disable(false);
		}
	}
}

/* Re-enable non-disabled item buttons after a child submenu closes (inlined in 0x81346CE0) */
void PopupListInstance::RestoreVisibleButtons()
{
	int generatedNum = m_listView->GetGeneratedNum();
	for (int i = 0; i < generatedNum; ++i) {
		ui::listview::ListPos genPos = m_listView->GetGeneratedPos(i);
		ui::ListItem *item = m_listView->GetListItem(genPos.m_segment, genPos.m_cell);
		ui::Widget *child = item->GetChild(0);
		if (child != NULL && child->IsInherit(ui::Button::TypeName())) {
			ui::ButtonBase *btn = static_cast<ui::ButtonBase *>(child);
			int cellIdx = item->GetCellIndex();
			if (cellIdx < static_cast<int>(m_items.size()) &&
			    !m_items[cellIdx].disabled)
				btn->Enable(false);
		}
	}
}

/* Release all owned callback objects on open failure */
static void releaseParamCallbacks(ShellPopupListParam *param)
{
	delete param->customFactory;
	delete param->itemCallback;
	delete param->selectCallback;
	delete param->closeCallback;
}

/* Internal open entry point (0x81346748) */
static int popupListOpenInternal(ShellPopupListParam *param, unsigned int depth, int option)
{
	(void)option;
	if (depth > s_popupListStack.size()) {
		releaseParamCallbacks(param);
		return POPUP_LIST_ERR_INVALID_PARAM;
	}
	if (depth < s_popupListStack.size()) {
		releaseParamCallbacks(param);
		return POPUP_LIST_ERR_BUSY;
	}
	if (param->plugin == NULL ||
	    (param->selected <= 0 && param->texts.size() == 0 && param->items.size() == 0)) {
		releaseParamCallbacks(param);
		return POPUP_LIST_ERR_INVALID_PARAM;
	}

	PopupListSubAnchor subAnchor;
	subAnchor.valid = false;
	subAnchor.position = PafVec2(0.0f, 0.0f);
	if (s_popupListStack.size() != 0) {
		PopupListInstance *parent = s_popupListStack.back();
		parent->DisableVisibleButtons();
		subAnchor.valid = true;
		subAnchor.position = parent->GetSubMenuAnchorPos();
	}

	char depthBuf[64];
	sce_paf_snprintf(depthBuf, sizeof(depthBuf), "%u",
			 static_cast<unsigned int>(s_popupListStack.size()));
	string pageName = string("page_popup_list_view") + string(depthBuf);
	IDParam pageId(pageName);

	SceUInt32 instanceId = s_nextInstanceId++;
	PopupListInstance *inst = new PopupListInstance(pageId, param, &subAnchor, instanceId);
	s_popupListStack.push_back(inst);
	return 0;
}

/* Play closing transitions and close the PAF page (0x81346A54) */
int PopupListInstance::ClosePage(int resultIndex)
{
	int effectiveResult = resultIndex;
	if (resultIndex >= 0 && m_selectedIndex == resultIndex)
		effectiveResult = -1;

	m_pageWidget->DeleteEventCallback(ui::Handler::CB_STATE_TERM, popupListOnPageTerm, this);

	if (m_type == POPUP_LIST_TYPE_DIALOG) {
		float time = ScePafWidget_F1176ACC(m_frameWidget, common::transition::Type_Popup2,
						   0.0f, NULL, NULL);
		if (m_bgDimWidget != NULL)
			ScePafCommon_B48C9BDC(time, m_bgDimWidget,
					      common::transition::Type_FadeinSlow,
					      false, false);
		ScePafWidget_5A9793B0(m_pageWidget, time);
		m_listView->m_clip_mode = 1;
	} else if (m_type == POPUP_LIST_TYPE_SHEET) {
		float time = ScePafWidget_F1176ACC(m_frameWidget,
						   common::transition::Type_SlideFromRight1,
						   0.0f, NULL, NULL);
		if (m_bgDimWidget != NULL)
			ScePafCommon_B48C9BDC(time, m_bgDimWidget,
					      common::transition::Type_FadeinSlow,
					      false, false);
		ScePafWidget_5A9793B0(m_pageWidget, time);
	} else if (m_type == POPUP_LIST_TYPE_FLOATING ||
		   m_type == POPUP_LIST_TYPE_CALLOUT ||
		   m_type == POPUP_LIST_TYPE_CALLOUT_ALT) {
		float time = ScePafWidget_F1176ACC(m_frameWidget, common::transition::Type_Popup3,
						   0.0f, NULL, NULL);
		float hideTime = time * 0.8f;
		if (hideTime > 0.0f) {
			Timer *timer = new Timer(hideTime);
			m_listView->Hide(0xFFu, timer, NULL, 0.0f);
		} else {
			m_listView->Hide(0xFFu, NULL, NULL, 0.0f);
		}
		if (m_bgDimWidget != NULL)
			ScePafCommon_B48C9BDC(time, m_bgDimWidget,
					      common::transition::Type_FadeinSlow,
					      false, false);
		ScePafWidget_5A9793B0(m_pageWidget, time);
	}

	Plugin::PageCloseParam closeParam;
	m_plugin->PageClose(m_pageId, closeParam);
	return effectiveResult;
}

/* Dismiss a popup list, notify close callback, and restore parent popup (0x81346CE0) */
static int popupListDismiss(PopupListInstance *instance, int resultIndex)
{
	int closeResult = instance->ClosePage(resultIndex);
	ShellPopupListCloseCallback *closeCb = instance->m_closeCallback;

	delete instance;
	s_popupListStack.pop_back();

	if (closeCb != NULL) {
		closeCb->OnClose(closeResult);
		delete closeCb;
	}

	if (s_popupListStack.size() != 0)
		s_popupListStack.back()->RestoreVisibleButtons();

	return 0;
}

/* Outside touch or cancel button handler (0x81346E82) */
static void popupListOnOutsideOrCancel(int32_t type, ui::Handler *self, ui::Event *event,
				       void *userContext)
{
	(void)type;
	(void)self;
	(void)event;
	(void)userContext;

	if (s_popupListStack.size() == 0)
		return;

	PopupListInstance *top = s_popupListStack.back();
	ui::Widget *frame = top->m_frameWidget;
	if (frame->IsInputTarget() && !isWidgetShowHeld(frame)) {
		if (top->m_playSe)
			Framework::Instance()->PlayCommonSE(2, 0.0f, false);
		popupListDismiss(top, -1);
	}
}

/* Emergency page termination cleanup handler (0x81346F04) */
static void popupListOnPageTerm(int32_t type, ui::Handler *self, ui::Event *event,
				void *userContext)
{
	(void)type;
	(void)self;
	(void)event;

	PopupListInstance *instance = static_cast<PopupListInstance *>(userContext);
	delete instance;

	for (vector<PopupListInstance *>::iterator it = s_popupListStack.begin();
	     it != s_popupListStack.end(); ++it) {
		if (*it == instance) {
			s_popupListStack.erase(it);
			break;
		}
	}
}

/* Apply computed width and column positions to all visible items and frame (0x81346FF6) */
void PopupListInstance::ApplyLayout(bool animate)
{
	int generatedNum = m_listView->GetGeneratedNum();

	if (m_singleLabelMode) {
		for (int i = 0; i < generatedNum; ++i) {
			ui::listview::ListPos genPos = m_listView->GetGeneratedPos(i);
			ui::ListItem *listItem = m_listView->GetListItem(genPos.m_segment, genPos.m_cell);
			ui::Widget *btn = listItem->GetChild(0);

			ui::AdjustParam adjustParam;
			if (btn->m_adjust_param != NULL)
				adjustParam = *btn->m_adjust_param;
			else
				adjustParam = ScePafWidget_BE1CEA2E;
			adjustParam.m_left_margin = m_columnMetrics[0].posX;
			ScePafWidget_B1C23134(btn, adjustParam);

			if (m_checkMarkSide != POPUP_LIST_CHECK_NONE &&
			    genPos.m_cell == m_selectedIndex &&
			    btn != NULL && btn->m_children.size() != 0) {
				ui::Widget *checkWidget = btn->m_children.back();
				PafVec4 checkPos = checkWidget->GetPos(
					ui::Widget::GET_CURRENT, NULL,
					PafVec4(0.0f, 0.0f, 0.0f, 0.0f));
				checkPos.set_x(m_checkMarkPosX);
				checkWidget->SetPos(checkPos, NULL, ui::Widget::ANIM_MOVE,
						    0, 0.0f, NULL, NULL);
			}

			if (m_type == POPUP_LIST_TYPE_FLOATING ||
			    m_type == POPUP_LIST_TYPE_CALLOUT ||
			    m_type == POPUP_LIST_TYPE_CALLOUT_ALT) {
				PafVec4 itemTargetSize = *listItem->GetSize(ui::Widget::GET_ANIMTARGET);
				for (size_t partIdx = 0; partIdx < btn->m_focus_parts.size(); ++partIdx) {
					ui::FocusShape &part = btn->m_focus_parts[partIdx];
					float rotZ = part.m_rot.extract_z();
					bool rotatedQuarter =
						(part.m_rot.extract_x() == 0.0f &&
						 part.m_rot.extract_y() == 0.0f &&
						 (rotZ == 1.5707964f || rotZ == -1.5707964f));
					if (rotatedQuarter)
						part.m_size = PafVec2(itemTargetSize.extract_y(),
								      m_currentListWidth);
					else
						part.m_size = PafVec2(m_currentListWidth,
								      itemTargetSize.extract_y());
					part.m_changed = true;
				}

				ui::FocusShape defaultShape;
				ui::FocusShape *shapePtr = btn->m_focus_shape != NULL
								   ? btn->m_focus_shape
								   : &defaultShape;
				shapePtr->m_size = PafVec2(m_currentListWidth,
							   itemTargetSize.extract_y());
				ScePafWidget_13B246E0(btn, shapePtr);
			}
		}
	} else {
		for (int i = 0; i < generatedNum; ++i) {
			ui::listview::ListPos genPos = m_listView->GetGeneratedPos(i);
			ui::ListItem *listItem = m_listView->GetListItem(genPos.m_segment, genPos.m_cell);

			ui::Widget *container = NULL;
			if (m_useDefaultItemLayout != 0) {
				container = listItem->GetChild(0);
			} else if (m_customButtonChildIndex >= 0 &&
				   m_customButtonChildIndex < static_cast<int>(listItem->m_children.size())) {
				container = listItem->GetChild(m_customButtonChildIndex);
			} else {
				container = listItem;
			}

			size_t colIdx = 0;
			for (list<ui::Widget *>::iterator it = container->m_children.begin();
			     it != container->m_children.end() && colIdx < m_columnMetrics.size();
			     ++it, ++colIdx) {
				ui::Widget *colWidget = *it;
				PafVec4 colPos = colWidget->GetPos(
					ui::Widget::GET_CURRENT, NULL,
					PafVec4(0.0f, 0.0f, 0.0f, 0.0f));
				colPos.set_x(m_columnMetrics[colIdx].posX);
				colWidget->SetPos(colPos, NULL, ui::Widget::ANIM_MOVE,
						  0, 0.0f, NULL, NULL);
			}

			if (m_checkMarkSide != POPUP_LIST_CHECK_NONE &&
			    genPos.m_cell == m_selectedIndex) {
				ui::Widget *checkParent = container;
				if (m_useDefaultItemLayout == 0 &&
				    (m_customButtonChildIndex < 0 ||
				     m_customButtonChildIndex >=
					     static_cast<int>(listItem->m_children.size()))) {
					if (checkParent == NULL || checkParent->m_children.size() == 0)
						continue;
					checkParent = checkParent->m_children.back();
				}
				if (checkParent != NULL && checkParent->m_children.size() != 0) {
					ui::Widget *checkWidget = checkParent->m_children.back();
					PafVec4 checkPos = checkWidget->GetPos(
						ui::Widget::GET_CURRENT, NULL,
						PafVec4(0.0f, 0.0f, 0.0f, 0.0f));
					checkPos.set_x(m_checkMarkPosX);
					checkWidget->SetPos(checkPos, NULL, ui::Widget::ANIM_MOVE,
							    0, 0.0f, NULL, NULL);
				}
			}
		}
	}

	PafVec4 frameSize = *m_frameWidget->GetSize(ui::Widget::GET_ANIMTARGET);
	float newFrameWidth = m_currentListWidth + m_listMarginLeft + m_listMarginRight;
	frameSize.set_x(newFrameWidth);
	Timer *frameResizeTimer = animate ? new Timer(0.0f) : NULL;
	m_frameWidget->SetSize(frameSize, frameResizeTimer, ui::Widget::ANIM_MOVE,
			       2, 0.0f, NULL, NULL);

	if (m_type == POPUP_LIST_TYPE_CALLOUT || m_type == POPUP_LIST_TYPE_CALLOUT_ALT) {
		PafVec4 framePos = m_frameWidget->GetPos(ui::Widget::GET_ANIMTARGET, NULL,
							 PafVec4(0.0f, 0.0f, 0.0f, 0.0f));
		SceUInt32 alignX = m_frameWidget->m_align_x;
		float leftEdge = 0.0f;
		float rightEdge = 0.0f;
		if (alignX == ui::Widget::ALIGN_RIGHT) {
			leftEdge = framePos.extract_x() - newFrameWidth;
			rightEdge = framePos.extract_x();
		} else if (alignX == ui::Widget::ALIGN_LEFT) {
			leftEdge = framePos.extract_x();
			rightEdge = framePos.extract_x() + newFrameWidth;
		} else {
			leftEdge = framePos.extract_x() - newFrameWidth * 0.5f;
			rightEdge = framePos.extract_x() + newFrameWidth * 0.5f;
		}

		float clampShiftX = 0.0f;
		if (leftEdge < m_boundsLeft)
			clampShiftX = m_boundsLeft - leftEdge;
		else if (rightEdge > m_boundsRight)
			clampShiftX = m_boundsRight - rightEdge;

		if (clampShiftX != 0.0f) {
			framePos.set_x(framePos.extract_x() + clampShiftX);
			m_frameWidget->SetPos(framePos, NULL, ui::Widget::ANIM_MOVE,
					      0, 0.0f, NULL, NULL);

			graph::PlaneObj *bgPlane = static_cast<graph::PlaneObj *>(
				m_frameWidget->GetDrawObj(0));
			SceUInt32 divMode = bgPlane->m_divide != NULL
						    ? bgPlane->m_divide->m_div_mode
						    : bgPlane->m_tex_surface[0]->GetDivideMode();
			if (divMode == 5u)
				bgPlane->SetDivCenterPos(bgPlane->m_div_center_pos - clampShiftX);
		}
	}

	PafVec4 listSize = *m_listView->GetSize(ui::Widget::GET_ANIMTARGET);
	listSize.set_x(m_currentListWidth);
	Timer *listResizeTimer = animate ? new Timer(0.0f) : NULL;
	m_listView->SetSize(listSize, listResizeTimer, ui::Widget::ANIM_MOVE,
			    2, 0.0f, NULL, NULL);

	PafVec4 listPos = m_listView->GetPos(ui::Widget::GET_CURRENT, NULL,
					     PafVec4(0.0f, 0.0f, 0.0f, 0.0f));
	listPos.set_x((m_listMarginLeft - m_listMarginRight) * 0.5f);
	m_listView->SetPos(listPos, NULL, ui::Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
}

/* Recompute list width, column positions, and checkmark X offset (0x813477FE) */
void PopupListInstance::UpdateColumnMetrics(float requiredWidth)
{
	float totalWidth = requiredWidth + m_leftPadding + m_rightPadding;
	if (m_currentListWidth < totalWidth)
		m_currentListWidth = totalWidth;

	float innerWidth = m_currentListWidth - m_leftPadding - m_rightPadding;
	if (m_singleLabelMode) {
		m_columnMetrics[0].posX = m_leftPadding;
	} else {
		float cursorX = (m_leftPadding - m_rightPadding) * 0.5f - innerWidth * 0.5f;
		for (size_t i = 0; i < m_columnMetrics.size(); ++i) {
			float colStart = cursorX;
			float colWidth = m_columnMetrics[i].maxWidth;
			cursorX += colWidth;

			if (m_columnMetrics[i].alignX == ui::Widget::ALIGN_RIGHT)
				m_columnMetrics[i].posX = colStart + colWidth;
			else if (m_columnMetrics[i].alignX == ui::Widget::ALIGN_CENTER)
				m_columnMetrics[i].posX = colStart + colWidth * 0.5f;
			else
				m_columnMetrics[i].posX = colStart;
		}
	}

	if (m_checkMarkSide != POPUP_LIST_CHECK_NONE) {
		float basePosX = (m_checkMarkSide == POPUP_LIST_CHECK_LEFT)
					 ? -(innerWidth * 0.5f) - m_checkMarkOffset
					 : (innerWidth * 0.5f) + m_checkMarkOffset;
		m_checkMarkPosX = basePosX + (m_leftPadding - m_rightPadding) * 0.5f;
	}

	if (m_layoutPhase == 2)
		ApplyLayout(true);
}

/* Title text ready callback: shrink font or ellipsize if wider than max width (0x813478EC) */
static void popupListOnTitleReady(int32_t type, ui::Handler *self, ui::Event *event,
				  void *userContext)
{
	(void)type;
	(void)event;

	ui::Text *titleText = static_cast<ui::Text *>(self);
	PopupListInstance *inst = static_cast<PopupListInstance *>(userContext);

	float titleWithMargin = titleText->GetSize(ui::Widget::GET_CURRENT)->extract_x() + 160.0f;
	if (titleWithMargin <= inst->m_currentListWidth)
		return;

	float neededContent = titleWithMargin - inst->m_leftPadding - inst->m_rightPadding;
	if (neededContent > inst->m_maxContentWidth) {
		if (inst->m_titleMinFontSize != 0.0f) {
			float currentFontSize = 0.0f;
			titleText->GetStyleAttribute(graph::TextStyleAttribute_Point, 0,
						     currentFontSize);
			if (currentFontSize > inst->m_titleMinFontSize) {
				scePafWidgetSetFontSize(titleText, currentFontSize - 1.0f,
							graph::TextStyleAttribute_Point, 0, 0);
				return;
			}
		}

		float clampedTitleWidth = inst->m_maxContentWidth + inst->m_leftPadding +
					  inst->m_rightPadding - 160.0f;
		PafVec4 size = *titleText->GetSize(ui::Widget::GET_CURRENT);
		size.set_x(clampedTitleWidth);
		titleText->SetSize(size, NULL, ui::Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);
		titleText->SetAdjust(0, titleText->m_adjust_y, titleText->m_adjust_z);
		titleText->SetLayoutAttribute(graph::TextLayoutAttribute_Ellipsis, true);
		neededContent = inst->m_maxContentWidth;
	}

	inst->UpdateColumnMetrics(neededContent);
}

/* Frame readiness polling interval: shows popup once children finish preparing (0x81347A40) */
static void popupListOnFrameInterval(int32_t type, ui::Handler *self, ui::Event *event,
				     void *userContext)
{
	(void)type;
	(void)event;

	ui::Widget *frame = static_cast<ui::Widget *>(self);
	PopupListInstance *inst = static_cast<PopupListInstance *>(userContext);

	if (inst->m_layoutPhase == 0) {
		if (frame->m_prepare_count == 0)
			inst->m_layoutPhase = 1;
		return;
	}

	if (inst->m_layoutPhase == 1) {
		if (frame->m_prepare_count != 0) {
			inst->m_layoutPhase = 0;
			return;
		}
		inst->m_layoutPhase = 2;
		inst->ApplyLayout(false);

		if (inst->m_type == POPUP_LIST_TYPE_DIALOG) {
			float time = ScePafWidget_44B22ACA(frame, common::transition::Type_Popup2,
							   0.0f, NULL, NULL);
			inst->m_listView->SetTimeoutEvent(100, time, popupListOnClipTimerEnd, NULL);
			inst->m_listView->m_clip_mode = 1;
		} else if (inst->m_type == POPUP_LIST_TYPE_FLOATING ||
			   inst->m_type == POPUP_LIST_TYPE_CALLOUT ||
			   inst->m_type == POPUP_LIST_TYPE_CALLOUT_ALT) {
			float time = ScePafWidget_44B22ACA(frame, common::transition::Type_Popup3,
							   0.0f, NULL, NULL);
			inst->m_listView->Hide(0xFFu, NULL, NULL, 0.0f);
			float fadeTime = time * 0.5f;
			if (fadeTime <= 0.0f) {
				inst->m_listView->Show(0xFFu, NULL, NULL, 0.0f);
			} else {
				Timer *timer = new Timer(fadeTime);
				inst->m_listView->Show(0xFFu, timer, NULL, 0.0f);
			}
		}
		return;
	}

	if (inst->m_layoutPhase == 2) {
		frame->KillIntervalEvent(0);
		inputdevice::Unlock(POPUP_LIST_INPUT_LOCK_MASK);
		inst->m_inputLockCount--;
	}
}

/* Measure a child column widget and update total column width (0x81347C88) */
static void measureColumnChildWidth(PopupListInstance *inst, int columnIndex, ui::Widget *child)
{
	float childWidth = child->GetSize(ui::Widget::GET_CURRENT)->extract_x();
	PopupListColumnMetric &metric = inst->m_columnMetrics[columnIndex];
	if (metric.maxWidth >= childWidth)
		return;

	metric.maxWidth = childWidth;
	float sumWidth = 0.0f;
	for (size_t i = 0; i < inst->m_columnMetrics.size(); ++i)
		sumWidth += inst->m_columnMetrics[i].maxWidth;

	if (sumWidth >= inst->m_maxContentWidth) {
		metric.maxWidth = inst->m_maxContentWidth - (sumWidth - childWidth);
		sumWidth = inst->m_maxContentWidth;
	}

	inst->UpdateColumnMetrics(sumWidth);
}

/* Child text ready callback for multi-child custom items (0x81347D06) */
static void popupListOnChildTextReady(int32_t type, ui::Handler *self, ui::Event *event,
				      void *userContext)
{
	(void)type;
	(void)event;

	PopupListTextMeasureContext *ctx = static_cast<PopupListTextMeasureContext *>(userContext);
	PopupListInstance *inst = ctx->instance;
	int colIndex = ctx->columnIndex;
	delete ctx;

	self->DeleteEventCallback(ui::Handler::CB_STATE_READY, popupListOnChildTextReady,
				  userContext);
	measureColumnChildWidth(inst, colIndex, static_cast<ui::Widget *>(self));
}

/* Clear dialog clip mode after opening transition finishes (0x81347D3C) */
static void popupListOnClipTimerEnd(int32_t type, ui::Handler *self, ui::Event *event,
				    void *userContext)
{
	(void)type;
	(void)event;
	(void)userContext;

	ui::Widget *listView = static_cast<ui::Widget *>(self);
	listView->m_clip_mode = 0;
}

/* Measure single-label button text width, shrinking or wrapping if over max width (0x81347D4A) */
static void measureSingleLabelWidth(PopupListInstance *inst, ui::Widget *buttonWidget)
{
	graph::TextObj *textObj = static_cast<graph::TextObj *>(
		buttonWidget->GetDrawObj(ui::Button::OBJ_LABEL));

	float maxLineWidth = inst->m_minContentWidth;
	uint32_t lineCount = 0;
	textObj->GetLineCount(lineCount);
	for (uint32_t i = 0; i < lineCount; ++i) {
		graph::TextLayoutLineInfo lineInfo;
		textObj->GetLineInfo(i, lineInfo);
		float lineWidth = lineInfo.frame.extract_z();
		if (lineWidth > maxLineWidth)
			maxLineWidth = lineWidth;
	}

	float prevWidth = inst->m_columnMetrics[0].maxWidth;
	if (prevWidth >= maxLineWidth)
		return;

	inst->m_columnMetrics[0].maxWidth = maxLineWidth;
	if (maxLineWidth > inst->m_maxContentWidth) {
		if (inst->m_itemMinFontSize != 0.0f && lineCount == 1u) {
			float fontSize = 0.0f;
			textObj->GetStyleAttribute(graph::TextStyleAttribute_Point, 0, fontSize);
			if (fontSize > inst->m_itemMinFontSize) {
				textObj->SetStyleAttribute(graph::TextStyleAttribute_Point,
							   0, 0, fontSize - 1.0f);
				inst->m_columnMetrics[0].maxWidth = prevWidth;
				return;
			}
		}

		if (inst->m_wrapLongText && lineCount == 1u) {
			bool wordWrap = false;
			textObj->GetLayoutAttribute(graph::TextLayoutAttribute_WordWrap, wordWrap);
			if (!wordWrap) {
				textObj->SetSize(PafVec3(inst->m_maxContentWidth, 0.0f, 0.0f));
				textObj->SetLayoutAttribute(graph::TextLayoutAttribute_AdjustX, false);
				textObj->SetLayoutAttribute(graph::TextLayoutAttribute_AdjustY, true);
				textObj->SetLayoutAttribute(graph::TextLayoutAttribute_WordWrap, true);
				textObj->SetLayoutAttribute(graph::TextLayoutAttribute_Ellipsis, true);
				textObj->SetLayoutAttribute(graph::TextLayoutAttribute_MaxLineCount, 2);
				textObj->SetLayoutAttribute(graph::TextLayoutAttribute_LinePitch, 20.0f);
				textObj->SetStyleAttribute(graph::TextStyleAttribute_Point,
							   0, 0, 20.0f);
				inst->m_columnMetrics[0].maxWidth = prevWidth;
				return;
			}
		}

		inst->m_columnMetrics[0].maxWidth = inst->m_maxContentWidth;
		maxLineWidth = inst->m_maxContentWidth;
	}

	inst->UpdateColumnMetrics(maxLineWidth);
}

/* Single-label text ready callback (0x81347F0A) */
static void popupListOnLabelTextReady(int32_t type, ui::Handler *self, ui::Event *event,
				      void *userContext)
{
	(void)type;
	(void)event;

	measureSingleLabelWidth(static_cast<PopupListInstance *>(userContext),
				static_cast<ui::Widget *>(self));
}

/* Item button decide handler: play SE, notify select callback, auto-close if enabled (0x81347F14) */
static void popupListOnItemDecide(int32_t type, ui::Handler *self, ui::Event *event,
				  void *userContext)
{
	(void)type;
	(void)self;
	(void)event;

	ui::ListItem *listItem = static_cast<ui::ListItem *>(userContext);
	int cellIndex = listItem->GetCellIndex();
	if (s_popupListStack.size() == 0)
		return;

	PopupListInstance *top = s_popupListStack.back();
	if (top == NULL)
		return;

	SceUInt32 savedInstanceId = top->m_instanceId;
	if (top->m_playSe)
		Framework::Instance()->PlayCommonSE(1, 0.0f, false);

	if (top->m_selectCallback != NULL) {
		const ShellPopupListItem *item = NULL;
		if (cellIndex < static_cast<int>(top->m_items.size()))
			item = &top->m_items[cellIndex];
		top->m_selectCallback->OnSelect(cellIndex, item);
	}

	for (vector<PopupListInstance *>::iterator it = s_popupListStack.begin();
	     it != s_popupListStack.end(); ++it) {
		if (*it == top) {
			if ((*it)->m_instanceId == savedInstanceId && top->m_autoCloseOnSelect)
				popupListDismiss(top, cellIndex);
			break;
		}
	}
}

/* Focus polling timer: sets focus on the initially selected item once visible (0x81347FBE) */
static void popupListOnFocusPoll(int32_t type, ui::Handler *self, ui::Event *event,
				 void *userContext)
{
	(void)type;
	(void)event;

	PopupListInstance *inst = static_cast<PopupListInstance *>(userContext);
	if (inst == NULL) {
		self->KillIntervalEvent(0);
		return;
	}

	ui::ListItem *item = inst->m_listView->GetListItem(0, inst->m_selectedIndex);
	if (item == NULL) {
		self->KillIntervalEvent(0);
		return;
	}

	if (ScePafWidget_DB0FFD33(item)) {
		inst->m_listView->SetFocus(0, inst->m_selectedIndex,
					   ui::ListView::FOCUS_ALIGN_TYPE_NEAR, NULL);
		self->KillIntervalEvent(0);
	}
}

/* Configure a newly created ListItem widget (0x8134801A) */
void PopupListInstance::ConfigureListItem(int cellIndex, ui::ListItem *listItem)
{
	listItem->SetMetaAlpha(0.0f, NULL, ui::Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);
	listItem->SetLayouter(new PopupListItemFadeLayouter());

	ui::Widget *contentWidget = listItem;
	ui::Widget *buttonWidget = NULL;

	if (m_useDefaultItemLayout != 0) {
		contentWidget = listItem->GetChild(0);
	} else if (m_customButtonChildIndex >= 0 &&
		   m_customButtonChildIndex < static_cast<int>(listItem->m_children.size())) {
		buttonWidget = listItem->GetChild(m_customButtonChildIndex);
		contentWidget = buttonWidget;
	} else if (m_customButtonChildIndex == -2) {
		m_customButtonChildIndex = -1;
		int idx = 0;
		for (list<ui::Widget *>::iterator it = listItem->m_children.begin();
		     it != listItem->m_children.end(); ++it, ++idx) {
			ui::Widget *candidate = *it;
			if (candidate != NULL &&
			    candidate->IsInherit(ui::Button::TypeName()) &&
			    candidate->m_adjust_x == 1u &&
			    candidate->m_adjust_y == 1u) {
				contentWidget = candidate;
				buttonWidget = candidate;
				m_customButtonChildIndex = idx;
				break;
			}
		}
	}

	if (cellIndex >= 0 && cellIndex < static_cast<int>(m_items.size()))
		contentWidget->SetString(m_items[cellIndex].text);

	if (m_columnMetrics.size() == 0) {
		size_t columnCount = contentWidget->m_children.size();
		if (columnCount == 0 && m_useDefaultItemLayout != 0) {
			wstring labelText;
			contentWidget->GetString(labelText);
			if (labelText.length() != 0) {
				columnCount = 1;
				m_singleLabelMode = true;
			}
		}
		PopupListColumnMetric emptyMetric;
		m_columnMetrics.resize(columnCount, emptyMetric);
	}

	if (m_singleLabelMode) {
		if (m_columnMetrics[0].maxWidth != 0.0f) {
			ui::AdjustParam adjustParam;
			if (contentWidget->m_adjust_param != NULL)
				adjustParam = *contentWidget->m_adjust_param;
			else
				adjustParam = ScePafWidget_BE1CEA2E;
			adjustParam.m_left_margin = m_columnMetrics[0].posX;
			ScePafWidget_B1C23134(contentWidget, adjustParam);
		}
		contentWidget->AddEventCallback(ui::Handler::CB_STATE_READY,
						popupListOnLabelTextReady, this);
	} else {
		size_t colIdx = 0;
		for (list<ui::Widget *>::iterator it = contentWidget->m_children.begin();
		     it != contentWidget->m_children.end() && colIdx < m_columnMetrics.size();
		     ++it, ++colIdx) {
			ui::Widget *colWidget = *it;
			m_columnMetrics[colIdx].alignX = colWidget->m_align_x;
			if (m_columnMetrics[colIdx].maxWidth != 0.0f) {
				PafVec4 colPos = colWidget->GetPos(
					ui::Widget::GET_CURRENT, NULL,
					PafVec4(0.0f, 0.0f, 0.0f, 0.0f));
				colPos.set_x(m_columnMetrics[colIdx].posX);
				colWidget->SetPos(colPos, NULL, ui::Widget::ANIM_MOVE,
						  0, 0.0f, NULL, NULL);
			}
			if (colWidget != NULL && colWidget->IsInherit(ui::Text::TypeName())) {
				PopupListTextMeasureContext *ctx = new PopupListTextMeasureContext();
				ctx->instance = this;
				ctx->columnIndex = static_cast<int>(colIdx);
				colWidget->AddEventCallback(ui::Handler::CB_STATE_READY,
							    popupListOnChildTextReady, ctx);
			}
		}
	}

	if (buttonWidget == NULL) {
		if (m_useDefaultItemLayout != 0) {
			buttonWidget = contentWidget;
		} else {
			buttonWidget = new ui::Button(listItem, NULL);
			buttonWidget->m_name = IDParam("popup_list_view_list_item");
			buttonWidget->SetAdjust(1u, 1u, 0u);
			if (!m_showItemBg) {
				buttonWidget->SetColor(PafVec4(0.0f, 0.0f, 0.0f, 0.0f), NULL,
						       ui::Widget::ANIM_COLOR, 0, 0.0f, NULL, NULL);
			}
		}
	}

	buttonWidget->AddEventCallback(ui::ButtonBase::CB_BTN_DECIDE,
				       popupListOnItemDecide, listItem);

	ui::ButtonBase *buttonBase = static_cast<ui::ButtonBase *>(buttonWidget);
	if (cellIndex < static_cast<int>(m_items.size()) && m_items[cellIndex].disabled) {
		if (buttonBase->m_disable_color != NULL)
			*buttonBase->m_disable_color = s_disabledItemColor;
		else
			buttonBase->m_disable_color = new PafVec4(s_disabledItemColor);
		buttonBase->Disable(false);
	}

	int posKind = 0;
	if (m_itemCount != 1) {
		if (cellIndex == 0)
			posKind = 1;
		else if (cellIndex == m_itemCount - 1)
			posKind = 3;
		else
			posKind = 2;
	}

	SceUInt32 bgTexHash = m_style.itemTextureHash[posKind][0];
	if (bgTexHash != 0) {
		IDParam texId("", bgTexHash);
		intrusive_ptr<graph::Surface> tex = m_plugin->GetTexture(texId);
		buttonBase->SetTexture(tex);
		graph::DrawObj *bgObj = buttonBase->GetDrawObj(0);
		bgObj->SetColor(PafVec4(1.0f, 1.0f, 1.0f, 1.0f));
	}

	SceUInt32 glowTexHash = m_style.itemTextureHash[posKind][1];
	if (glowTexHash != 0) {
		buttonBase->SetGlowObj(ui::Button::OBJ_GLOW);
		IDParam glowId("", glowTexHash);
		intrusive_ptr<graph::Surface> glowTex = m_plugin->GetTexture(glowId);
		buttonBase->SetTexture(glowTex, ui::Button::OBJ_GLOW, 0);
	}

	if (m_type == POPUP_LIST_TYPE_FLOATING ||
	    m_type == POPUP_LIST_TYPE_CALLOUT ||
	    m_type == POPUP_LIST_TYPE_CALLOUT_ALT) {
		ui::FocusShape partShape;
		if (m_itemCount != 1) {
			if (cellIndex == 0) {
				partShape.m_shape = 6;
				partShape.m_rot = PafVec3(0.0f, 0.0f, -1.5707964f);
			} else if (cellIndex == m_itemCount - 1) {
				partShape.m_shape = 6;
				partShape.m_rot = PafVec3(0.0f, 0.0f, 1.5707964f);
			} else {
				partShape.m_shape = 2;
			}
		}
		partShape.m_changed = true;
		buttonBase->m_focus_parts.insert(buttonBase->m_focus_parts.end(), partShape);
	}

	if (cellIndex >= 0 && cellIndex < static_cast<int>(m_items.size()) &&
	    m_items[cellIndex].icon != NULL) {
		ui::Plane *iconPlane = new ui::Plane(buttonBase, NULL);
		iconPlane->SetPos(PafVec4(m_iconPosX, 0.0f, 0.0f, 0.0f), NULL,
				  ui::Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
		graph::Surface *iconSurf = m_items[cellIndex].icon;
		iconPlane->SetSize(PafVec4(static_cast<float>(iconSurf->GetWidth()),
					   static_cast<float>(iconSurf->GetHeight()),
					   0.0f, 0.0f),
				   NULL, ui::Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);
		iconPlane->SetAnchor(ui::Widget::ANCHOR_LEFT, iconPlane->m_anchor_y,
				     iconPlane->m_anchor_z, NULL);
		intrusive_ptr<graph::Surface> iconRef(iconSurf);
		iconPlane->SetTexture(iconRef, 0, 0);
	}

	if (m_checkMarkSide != POPUP_LIST_CHECK_NONE && cellIndex == m_selectedIndex) {
		ui::Plane *checkPlane = new ui::Plane(buttonBase, NULL);
		checkPlane->SetPos(PafVec4(m_checkMarkPosX, 0.0f, 0.0f, 0.0f), NULL,
				   ui::Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
		IDParam checkTexId("", HASH_TEX_CHECKMARK);
		intrusive_ptr<graph::Surface> checkTex = m_plugin->GetTexture(checkTexId);
		checkPlane->SetSize(PafVec4(static_cast<float>(checkTex->GetWidth()),
					    static_cast<float>(checkTex->GetHeight()),
					    0.0f, 0.0f),
				    NULL, ui::Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);
		checkPlane->SetTexture(checkTex, 0, 0);
	}

	if (cellIndex >= 0 && cellIndex < static_cast<int>(m_items.size()) &&
	    m_items[cellIndex].hasSubMenu) {
		ui::Plane *subMenuPlane = new ui::Plane(buttonBase, NULL);
		subMenuPlane->m_name = IDParam("popup_list_view_sub_menu_mark");
		subMenuPlane->SetPos(PafVec4(m_subMenuMarkPosX, 0.0f, 0.0f, 0.0f), NULL,
				     ui::Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
		subMenuPlane->SetSize(PafVec4(46.0f, 46.0f, 0.0f, 0.0f), NULL,
				      ui::Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);
		subMenuPlane->SetAnchor(ui::Widget::ANCHOR_RIGHT, subMenuPlane->m_anchor_y,
					subMenuPlane->m_anchor_z, NULL);
		IDParam arrowTexId("", HASH_TEX_SUBMENU_ARROW);
		intrusive_ptr<graph::Surface> arrowTex = m_plugin->GetTexture(arrowTexId);
		subMenuPlane->SetTexture(arrowTex, 0, 0);
	}
}

/* 0x81348DA4 */
int shellPopupListOpen(ShellPopupListParam *param, unsigned int depth, int option)
{
	return popupListOpenInternal(param, depth, option);
}

/* 0x81348DAC */
int shellPopupListClose(unsigned int depth)
{
	int count = static_cast<int>(s_popupListStack.size());
	if (depth + 1u <= static_cast<unsigned int>(count))
		return popupListDismiss(s_popupListStack.back(), -1);
	return count;
}

/* 0x81348DDA */
ui::Widget *shellPopupListGetWidget(unsigned int depth)
{
	if (s_popupListStack.size() != 0) {
		PopupListInstance *top = s_popupListStack.back();
		if (top != NULL && depth == 0)
			return top->m_listView;
	}
	return NULL;
}
