/*
 * paf::ui::RadioBox (TOOL 0x8110b81c..0x8110cb70): the box keeping one of its radio buttons
 * checked. A button checked unchecks the others under the box at its depth and brings the
 * slider over it; the arrow keys and the focus events move the focus among the buttons, back
 * to the last one left where the box keeps it. No record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <versatileparam/resourceparam.h>
#include <widget/core/environment.h>
#include <widget/core/event.h>
#include <widget/core/focus.h>
#include <widget/factory.h>
#include <widget/w_radio_box.h>
#include <widget/w_radio_button.h>

namespace paf {
namespace ui {

/* the widget shown, and hidden with its transition run over the timer's time (inlined in Sony's
   code, names ours) */
static inline void ShowWidget(Widget *widget, Timer *t)
{
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xff;
		widget->m_transition_complete = true;
		widget->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(widget, t, 2, 0.0f, NULL);
}

static inline void HideWidget(Widget *widget, Timer *t)
{
	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(t != NULL ? t->GetTotalTime() : 0.0f, true);
	ScePafWidget_4588699E(widget, t, 2, 0.0f, NULL);
}

/* where the widget's world position is in the space of the slider's world matrix */
static inline math::v4 PosOver(Widget *slider, Widget *widget)
{
	math::matrix inv = slider->GetWorldMatrix().transpose();

	return math::v4(inv.getImpl() * widget->GetWorldMatrix().getImpl().getCol3());
}

RadioBox::RadioBox(Widget *parent, CreateParam *param)
	: Widget(parent, param), m_slider(NULL), m_checked(NULL), m_last_focus(NULL),
	  m_keep_focus(false)
{
	m_hit_find_mode = 0;
	m_hit_detect_b_mode = 2;
	m_hit_detect_mode = 2;
	m_pre_dispatch = true;
	m_event_enable |= 0x10;
	EnableFocusEvent(true);
	SetAdjust(2, 2, 2);
}

RadioBoxParam::RadioBoxParam()
{
}

const char RadioBox::m_widget_type[] = "radio_box";
const char RadioBoxParam::m_param_name[] = "radio_box_base";

bool RadioBox::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *RadioBox::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *RadioBox::TypeName()
{
	return m_widget_type;
}

void RadioBox::SetSlider(Widget *widget)
{
	m_slider = widget;
	HideWidget(widget, NULL);
}

static void CBDestroyLastFocus(Widget *widget, void *data);

void RadioBox::DestroyWidget()
{
	if (m_checked != NULL)
		m_checked->RemoveDeleteCB(CBDestroyChecked, this);
	if (m_last_focus != NULL)
		m_last_focus->RemoveDeleteCB(CBDestroyLastFocus, this);
	Widget::DestroyWidget();
}

/* the slider kept over the checked button, as it moves and sizes */
void RadioBox::Update()
{
	if (m_checked != NULL && m_slider != NULL) {
		math::v4 size = *m_checked->GetSize(1);

		if (!(*m_slider->GetSize(1) == size)) {
			m_slider->KillAnim(ANIM_SIZE);
			m_slider->SetSize(size, new Timer(100.0f, Timer::FUNC_QUAD_OUT), ANIM_SIZE);
		}

		math::v4 pos = PosOver(m_slider, m_checked);

		if (!(m_slider->GetPos(1, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)) == pos)) {
			m_slider->KillAnim(ANIM_MOVE);
			m_slider->SetPos(pos, new Timer(100.0f, Timer::FUNC_QUAD_OUT), ANIM_MOVE);
		}
	}
	Widget::Update();
}

int32_t RadioBox::HandleFocusEvent(Event *e)
{
	if (e != NULL) {
		switch (e->GetType()) {
		case 0x401:
			if (OnFocusIn(e))
				e->Consume();
			break;
		case 0x402:
			if (OnFocusEnter(e))
				e->Consume();
			break;
		case 0x403:
			if (OnFocusLeave(e))
				e->Consume();
			break;
		case 0x404:
			if (OnFocusOut(e))
				e->Consume();
			break;
		}
	}
	return Handler::HandleFocusEvent(e);
}

int32_t RadioBox::HandleKeycodeEvent(Event *e)
{
	if (e != NULL) {
		switch (e->m_detail & 0xFF) {
		case 1:
		case 2:
		case 3:
		case 4: {
			uint32_t type = e->GetType();

			if ((type == 0x11 || type == 0x13) && OnDirKey(e))
				e->Consume();
			break;
		}
		default:
			if (m_pre_dispatch && (e->m_type & 0x40000000) != 0 &&
			    (e->m_type & Event::ROUTE_TERM) != 0)
				return 0;
		}
	}
	return Widget::HandleKeycodeEvent(e);
}

/* the function called on each widget depth levels below the widget */
static void ForEachChild(Widget *widget, int32_t depth, void (*func)(Widget *, void *),
			 void *data)
{
	list<Widget *>::iterator it = widget->m_children.begin();

	if (depth == 0) {
		for (list<Widget *>::iterator cur = it; cur != widget->m_children.end(); cur = ++it)
			func(*cur, data);
	} else {
		for (list<Widget *>::iterator cur = it; cur != widget->m_children.end(); cur = ++it)
			ForEachChild(*cur, depth - 1, func, data);
	}
}

static void UncheckOther(Widget *widget, void *data);

void RadioBox::OnValueChanged(RadioButton *radio_button)
{
	if (radio_button->m_is_checked) {
		if (m_checked != NULL) {
			m_checked->RemoveDeleteCB(CBDestroyChecked, this);
			m_checked = NULL;
		}
		if (m_last_focus != NULL) {
			m_last_focus->RemoveDeleteCB(CBDestroyLastFocus, this);
			m_last_focus = NULL;
		}

		int32_t depth = 0;

		for (Widget *w = radio_button->m_parent; w != NULL && w != this; w = w->m_parent)
			depth++;
		ForEachChild(this, depth, UncheckOther, radio_button);
		m_checked = radio_button;
		radio_button->AddDeleteCB(CBDestroyChecked, this);
		m_last_focus = m_checked;
		m_checked->AddDeleteCB(CBDestroyLastFocus, this);

		Widget *slider = m_slider;

		if (slider != NULL) {
			if (slider->m_show_alpha == 0.0f) {
				/* hidden: placed over it at once */
				slider->SetSize(*m_checked->GetSize(0), NULL, ANIM_SIZE);
				m_slider->SetPos(PosOver(m_slider, m_checked), NULL, ANIM_MOVE);
				slider = m_slider;
			}
			slider->KillAnim(ANIM_META_ALPHA);
			ShowWidget(m_slider, new Timer(100.0f));
		}
	} else {
		if (m_checked != NULL) {
			m_checked->RemoveDeleteCB(CBDestroyChecked, this);
			m_checked = NULL;
		}
		if (m_last_focus != NULL) {
			m_last_focus->RemoveDeleteCB(CBDestroyLastFocus, this);
			m_last_focus = NULL;
		}
		if (m_slider != NULL) {
			m_slider->KillAnim(ANIM_META_ALPHA);
			HideWidget(m_slider, new Timer(100.0f));
		}
	}
}

void RadioBox::SendEvent(int32_t type, int32_t v0, int32_t v1, int32_t v2, int32_t v3)
{
	Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, v0, v1, v2, v3);

	DoEvent(type, &ev);
}

void RadioBox::CBDestroyChecked(Widget *widget, void *data)
{
	((RadioBox *)data)->OnDestroyChecked(widget);
}

void RadioBox::OnDestroyChecked(Widget *widget)
{
	m_checked = NULL;
	if (m_slider != NULL)
		HideWidget(m_slider, new Timer(100.0f));
}

/* the last button left forgotten when it goes */
static void CBDestroyLastFocus(Widget *widget, void *data)
{
	((RadioBox *)data)->m_last_focus = NULL;
}

Widget *RadioBox::SearchFocus(const math::v2 &dir, Widget *from)
{
	if (m_env == NULL)
		return NULL;

	Widget *origin = from;

	if (origin == NULL) {
		origin = m_env->m_focus_cursor.m_target;
		if (origin == NULL || FindChild(origin, 0) == NULL)
			origin = GetRootFocus(this);
		if (!(dir.getImpl().getX() == sce::Vectormath::Simd::floatInVec(0.0f) &&
		      dir.getImpl().getY() == sce::Vectormath::Simd::floatInVec(0.0f)) &&
		    FindChild(origin, 0) == NULL)
			return NULL;
		if (origin == NULL)
			origin = this;
	}

	FocusSearchInfo info(255);

	info.m_dir = GetFocusDir(dir);
	ScePafWidget_EA117CF8(&info, origin);

	bool searching = false;

	if (m_focus_searching)
		searching = true;
	else
		m_focus_searching = true;

	Widget *found;

	if (from != NULL && FindChild(from, 0) == NULL)
		found = SetFocusOption(&info, 1);
	else
		found = origin->SetFocusOption(&info, 0);
	if (!searching)
		m_focus_searching = false;
	if (found == this)
		found = NULL;
	return found;
}

/* the focus kept where it is under the box, else given to the last button or the one found */
bool RadioBox::OnFocusIn(Event *e)
{
	Widget *focus = m_env->m_focus_cursor.m_target;

	if (focus != NULL && focus != this && FindChild(focus, 0) != NULL &&
	    ScePafWidget_DB0FFD33(focus))
		return true;
	if (m_last_focus != NULL && ScePafWidget_DB0FFD33(m_last_focus)) {
		m_last_focus->SetFocus(NULL);
		return true;
	}

	Widget *found = SearchFocus(math::v2(0.0f, 0.0f), (Widget *)e->m_data[0]);

	if (found != NULL && found != this && FindChild(found, 0) != NULL) {
		found->SetFocus(NULL);
		return true;
	}
	return false;
}

bool RadioBox::OnFocusEnter(Event *e)
{
	return false;
}

bool RadioBox::OnFocusLeave(Event *e)
{
	if (m_last_focus != NULL && ScePafWidget_DB0FFD33(m_last_focus)) {
		m_last_focus->SetFocus(NULL);
		return true;
	}
	return false;
}

/* the radio button the focus leaves for another under the box remembered, where it keeps it */
bool RadioBox::OnFocusOut(Event *e)
{
	Widget *widget = (Widget *)e->m_data[0];
	Widget *button = NULL;

	if (widget != NULL && widget->IsInherit(RadioButton::TypeName()))
		button = (Widget *)e->m_data[0];
	if (m_keep_focus && button != NULL && FindChild(button, 0) != NULL) {
		if (m_last_focus != NULL)
			m_last_focus->RemoveDeleteCB(CBDestroyLastFocus, this);
		m_last_focus = button;
		button->AddDeleteCB(CBDestroyLastFocus, this);
	}
	return false;
}

bool RadioBox::OnDirKey(Event *e)
{
	math::v2 dir(0.0f, 0.0f);

	if (e != NULL) {
		switch (e->m_detail & 0xFF) {
		case 1:
			dir = math::v2(0.0f, 1.0f);
			break;
		case 2:
			dir = math::v2(0.0f, -1.0f);
			break;
		case 3:
			dir = math::v2(-1.0f, 0.0f);
			break;
		case 4:
			dir = math::v2(1.0f, 0.0f);
			break;
		}
	}

	Widget *found = SearchFocus(dir, NULL);

	if (found != NULL && found->IsInherit(RadioButton::TypeName()) &&
	    FindChild(found, 0) != NULL) {
		found->SetFocus(NULL);
		return true;
	}
	return false;
}

const char *RadioBoxParam::GetParamName() const
{
	return m_param_name;
}

const char *RadioBoxParam::ParamName()
{
	return m_param_name;
}

int32_t RadioBoxParam::Create(int32_t mode)
{
	int32_t ret = WidgetParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL)
		m_serial->AddParam("w_slider", &m_slider, ResourceParam::SEMANTIC_WIDGETREF);
	return 0;
}

int32_t RadioBoxParam::Set()
{
	int32_t ret = WidgetParam::Set();

	if (ret < 0)
		return ret;

	RadioBox *box = (RadioBox *)m_widget;

	if (m_slider.GetIDHash() != 0) {
		Widget *slider = box->FindChild(m_slider, 0);

		if (slider != NULL)
			box->SetSlider(slider);
	}
	return 0;
}

int32_t RadioBoxParam::Get()
{
	int32_t ret = WidgetParam::Get();

	if (ret < 0)
		return ret;

	Widget *slider = ((RadioBox *)m_widget)->m_slider;

	if (slider != NULL)
		m_slider = slider->m_name;
	return 0;
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewRadioBox(Widget *parent)
{
	return new RadioBox(parent, NULL);
}

static WidgetParam *NewRadioBoxParam()
{
	return new RadioBoxParam;
}

static VersatileParam *NewRadioBoxLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewRadioBoxFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewRadioBoxStyleParam()
{
	return new WidgetStyleParam;
}

static VersatileParam *NewRadioBoxHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewRadioBoxSoundParam()
{
	return new WidgetSoundParam;
}

/* the other radio buttons beside the one checked unchecked */
void paf::ui::UncheckOther(Widget *widget, void *data)
{
	if (widget != NULL && widget->IsInherit(RadioButton::TypeName()) && widget != data)
		((RadioButton *)widget)->SetCheck(false, -0.1f, false);
}

FactoryFuncTable g_factoryRadioBox = {
	RadioBox::TypeName(),
	NewRadioBox,
	NewRadioBoxParam,
	NewRadioBoxLayoutParam,
	NewRadioBoxFocusParam,
	NewRadioBoxStyleParam,
	NewRadioBoxHandlerParam,
	NewRadioBoxSoundParam,
};
