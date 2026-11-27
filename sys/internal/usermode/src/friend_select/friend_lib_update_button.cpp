/*
 * SceFriendSelect (vs0:sys/external/friend_select.suprx):
 * UpdateButtonWidget custom PAF widget, UpdateButtonStyleParam,
 * SceFriendLib PopupListView wrapper, and integer widget text helper.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <plugin.h>
#include <widget/core/animlistener.h>
#include <widget/core/event.h>
#include <widget/core/widget.h>
#include <widget/w_button_base.h>
#include <widget/w_plane_button.h>
#include "friend_select_internal.h"

using namespace paf;
using namespace paf::common;
using namespace paf::graph;
using namespace paf::math;
using namespace paf::ui;

#define UPDATE_BUTTON_ANIM_SPIN		0x1000E
#define UPDATE_BUTTON_TWO_PI		6.2831855f

static const char s_updateButtonTypeName[] = "update_button";
static const char s_updateButtonStyleName[] = "style_update_button";
static const DrawStyle s_defaultDrawStyle;

/* ========================================================================= */
/* UpdateButtonWidget & UpdateButtonStyleParam                               */
/* ========================================================================= */

static void OnUpdateButtonSpinAnim(Timer *timer, AnimListener *listener, void *self,
				   void *userData, void *arg)
{
	(void)listener;
	(void)self;
	(void)arg;
	DrawObj *arrowObj = (DrawObj *)userData;
	v3 rot(0.0f, 0.0f, timer->m_mapped_x * -UPDATE_BUTTON_TWO_PI);

	arrowObj->SetRot(rot);
}

const char *UpdateButtonWidget::TypeName(void)
{
	return s_updateButtonTypeName;
}

UpdateButtonWidget::UpdateButtonWidget(Widget *parent, Widget::CreateParam *param)
	: PlaneButton(parent, param), m_spinning(false)
{
	PlaneObj *arrowObj = new PlaneObj(s_defaultDrawStyle);
	DrawObjEntry entry(SharedPtr<DrawObj>(arrowObj), 0x17U);

	m_drawobjs.insert(FindDrawObj(1), entry);
}

bool UpdateButtonWidget::IsInherit(const char *type) const
{
	if (type == s_updateButtonTypeName)
		return true;
	return PlaneButton::IsInherit(type);
}

char *UpdateButtonWidget::GetTypeName() const
{
	return (char *)s_updateButtonTypeName;
}

void UpdateButtonWidget::EnsureHighlightObj(void)
{
	if (GetDrawObj(3) != NULL)
		return;
	PlaneObj *highlightObj = new PlaneObj(s_defaultDrawStyle);

	if (highlightObj != NULL) {
		DrawObjEntry entry(SharedPtr<DrawObj>(highlightObj), 0x1FU);

		m_drawobjs.insert(FindDrawObj(1), entry);
	}
}

bool UpdateButtonWidget::StartSpin()
{
	if (m_spinning)
		return true;
	DrawObj *arrowObj = GetDrawObj(2);

	if (arrowObj == NULL)
		return false;

	Timer *timer = new Timer(1000.0f, 0, 1, -1, NULL);
	AnimListener *listener =
		new AnimListener(timer, this, OnUpdateButtonSpinAnim, NULL, arrowObj);

	SetAnimListener(UPDATE_BUTTON_ANIM_SPIN, listener, 2);

	DrawObj *obj0 = GetDrawObj(0);

	if (obj0 != NULL)
		obj0->m_flags.m_visible = false;
	DrawObj *obj1 = GetDrawObj(1);

	if (obj1 != NULL)
		obj1->m_flags.m_visible = false;
	DrawObj *obj3 = GetDrawObj(3);

	if (obj3 != NULL)
		obj3->m_flags.m_visible = false;

	if ((m_event_enable & 0x400U) != 0)
		EnableFocusEvent(false);
	m_event_enable = (m_event_enable & 0xFFFFF00FU) & 0xFFFF5FFFU;
	m_focus_direction_off = true;
	ReleaseFocus();
	m_spinning = true;
	return true;
}

void UpdateButtonWidget::StopSpin()
{
	if (!m_spinning)
		return;
	DrawObj *arrowObj = GetDrawObj(2);

	if (arrowObj == NULL)
		return;

	KillAnim(UPDATE_BUTTON_ANIM_SPIN);
	v3 zeroRot(0.0f, 0.0f, 0.0f);

	arrowObj->SetRot(zeroRot);

	DrawObj *obj0 = GetDrawObj(0);

	if (obj0 != NULL)
		obj0->m_flags.m_visible = true;
	DrawObj *obj1 = GetDrawObj(1);

	if (obj1 != NULL)
		obj1->m_flags.m_visible = true;
	DrawObj *obj3 = GetDrawObj(3);

	if (obj3 != NULL)
		obj3->m_flags.m_visible = true;

	if ((m_event_enable & 0x400U) == 0)
		EnableFocusEvent(true);
	m_event_enable |= 0x0000FFF0U;
	m_focus_direction_off = false;
	m_spinning = false;
}

class UpdateButtonStyleParam : public PlaneButtonStyleParam {
public:
	UpdateButtonStyleParam();
	virtual ~UpdateButtonStyleParam() {}
	virtual const char *GetParamName() const;
	virtual int32_t SetEach(Widget *widget);

	PlaneObjParam m_arrow_obj;
	PlaneObjParam m_highlight_obj;
};

UpdateButtonStyleParam::UpdateButtonStyleParam()
{
	m_arrow_obj.m_name = "arrow_obj";
	m_arrow_obj.m_index = 2;
	m_drawobj_param_list.push_back(&m_arrow_obj);

	m_highlight_obj.m_name = "highlight_obj";
	m_highlight_obj.m_index = 3;
	m_drawobj_param_list.push_back(&m_highlight_obj);
}

const char *UpdateButtonStyleParam::GetParamName() const
{
	return s_updateButtonStyleName;
}

int32_t UpdateButtonStyleParam::SetEach(Widget *widget)
{
	DrawObjParam *hlParam = GetDrawObjParam(3);

	if (hlParam != NULL && hlParam->m_node != NULL)
		((UpdateButtonWidget *)widget)->EnsureHighlightObj();
	return PlaneButtonStyleParam::SetEach(widget);
}

static Widget *CreateUpdateButtonWidget(Widget *parent)
{
	return new UpdateButtonWidget(parent, NULL);
}

static WidgetParam *CreateUpdateButtonParam(void)
{
	return new PlaneButtonParam();
}

static VersatileParam *CreateUpdateButtonLayoutParam(void)
{
	return new WidgetLayoutParam();
}

static VersatileParam *CreateUpdateButtonFocusParam(void)
{
	return new WidgetFocusParam();
}

static VersatileParam *CreateUpdateButtonStyleParam(void)
{
	return new UpdateButtonStyleParam();
}

static VersatileParam *CreateUpdateButtonHandlerParam(void)
{
	return new PlaneButtonHandlerParam();
}

static VersatileParam *CreateUpdateButtonSoundParam(void)
{
	return new ButtonBaseSoundParam();
}

FactoryFuncTable g_updateButtonFactoryTable = {
	UpdateButtonWidget::TypeName(),
	CreateUpdateButtonWidget,
	CreateUpdateButtonParam,
	CreateUpdateButtonLayoutParam,
	CreateUpdateButtonFocusParam,
	CreateUpdateButtonStyleParam,
	CreateUpdateButtonHandlerParam,
	CreateUpdateButtonSoundParam
};

/* ========================================================================= */
/* PopupListView wrapper (FriendLibPopup*)                                   */
/* ========================================================================= */

class FriendLibPopupSession {
public:
	FriendLibPopupSession() : m_depth(-1) {}
	virtual ~FriendLibPopupSession() {}

	void Open(const PopupListOpenArg &arg);
	void OnClosed(int32_t selectedIndex, void *userData);

	int32_t m_depth;
};

static FriendLibPopupSession *s_popupSession = NULL;
static Callback1<int32_t> *s_popupCallback = NULL;

void FriendLibPopupMgrInit(void)
{
	s_popupSession = NULL;
	s_popupCallback = NULL;
}

void FriendLibPopupClose(void)
{
	if (s_popupSession != NULL) {
		if (s_popupSession->m_depth >= 0)
			PopupListViewPopTo(0);
		if (s_popupSession != NULL) {
			delete s_popupSession;
			s_popupSession = NULL;
		}
		if (s_popupCallback != NULL) {
			delete s_popupCallback;
			s_popupCallback = NULL;
		}
	}
}

void FriendLibPopupMgrTerm(void)
{
	if (s_popupSession != NULL) {
		if (s_popupSession->m_depth >= 0)
			PopupListViewPopTo(0);
		if (s_popupSession != NULL) {
			delete s_popupSession;
			s_popupSession = NULL;
		}
		if (s_popupCallback != NULL) {
			delete s_popupCallback;
			s_popupCallback = NULL;
		}
	}
}

void FriendLibPopupInit(void)
{
	FriendLibPopupMgrInit();
}

void FriendLibPopupTerm(void)
{
	FriendLibPopupMgrTerm();
}

void FriendLibPopupSession::Open(const PopupListOpenArg &arg)
{
	PopupListViewConfig cfg;

	cfg.plugin = arg.plugin;
	cfg.pagePriority = arg.plugin->m_draw_priority - 4;
	cfg.anchorWidget = arg.anchorWidget;
	cfg.anchorRadius = 42.0f;
	cfg.closeCb = new PopupListCloseMemberCallback<FriendLibPopupSession>(
		this, &FriendLibPopupSession::OnClosed, NULL);

	if (arg.selectedIndex >= 0) {
		cfg.style = 4;
		cfg.checkMarkSide = 1;
		cfg.minSize.set_x(186.0f);
		cfg.initialSelectIndex = arg.selectedIndex;
		cfg.topLimit = 254.0f;
		cfg.bottomLimit = -254.0f;
		cfg.leftLimit = -454.0f;
		cfg.rightLimit = 454.0f;
		cfg.leftPad = 68.0f;
	} else {
		cfg.style = 3;
		cfg.checkMarkSide = 0;
		cfg.minSize.set_x(200.0f);
		cfg.maxSize.set_x(760.0f);
		cfg.topLimit = 240.0f;
		cfg.bottomLimit = -240.0f;
		cfg.leftLimit = -480.0f;
		cfg.rightLimit = 480.0f;
		cfg.leftPad = 20.0f;
	}
	cfg.rightPad = 20.0f;
	if (arg.mode == 1)
		cfg.dirMask = 32U;

	cfg.items = arg.items;
	cfg.itemCount = (int32_t)arg.items.size();
	PopupListViewPush(cfg, 0, 0);
	m_depth = 0;
}

void FriendLibPopupSession::OnClosed(int32_t selectedIndex, void *userData)
{
	(void)userData;
	m_depth = -1;
	if (s_popupSession != NULL) {
		if (s_popupSession->m_depth >= 0)
			PopupListViewPopTo(0);
		if (s_popupSession != NULL) {
			delete s_popupSession;
			s_popupSession = NULL;
		}
		Callback1<int32_t> *cb = s_popupCallback;

		s_popupCallback = NULL;
		if (cb != NULL) {
			if (selectedIndex >= 0)
				cb->Invoke(selectedIndex);
			delete cb;
		}
	}
}

int32_t FriendLibPopupOpen(const PopupListOpenArg &arg)
{
	if (arg.plugin == NULL) {
		if (arg.callback != NULL)
			delete arg.callback;
		return FRIEND_SELECT_ERROR_INVALID_ARG;
	}

	if (s_popupSession != NULL) {
		if (s_popupSession->m_depth >= 0)
			PopupListViewPopTo(0);
		if (s_popupSession != NULL) {
			delete s_popupSession;
			s_popupSession = NULL;
		}
		if (s_popupCallback != NULL) {
			Callback1<int32_t> *oldCb = s_popupCallback;

			s_popupCallback = NULL;
			delete oldCb;
		}
	}

	s_popupSession = new FriendLibPopupSession();
	if (s_popupSession == NULL) {
		if (arg.callback != NULL)
			delete arg.callback;
		return FRIEND_SELECT_ERROR_NOMEM;
	}

	s_popupSession->Open(arg);
	s_popupCallback = arg.callback;
	return 0;
}

void FriendLibSetNumberText(Widget *widget, int32_t value, int32_t maxDigits)
{
	if (widget == NULL)
		return;
	wchar_t *buf = new wchar_t[maxDigits + 1];

	if (buf != NULL) {
		sce_paf_swprintf(buf, (size_t)(maxDigits + 1), L"%d", value);
		wstring str(buf);

		widget->SetString(str);
		delete[] buf;
	}
}
