/*
 * paf::ui::StateButtonBase (DEBUGGER 0x8112c77c..0x8112e22c): a ButtonBase that is checked or
 * not, with an image (0), a text label (1), a highlight (2) and a glow (3). Checking it sends
 * CB_STBTN_VALUE_CHANGED, plays the image's texture animation and its check or uncheck sound;
 * where it adjusts it is sized in x and y to its draw objects, its children and the margins of
 * its adjust parameter, its style animator laying the label out. No record or string names the
 * file; the names of the static functions are ours.
 */
#include <stddef.h>
#include <stdint.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <graphics/text/text_layout.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <std/vector>
#include <timer/timer.h>
#include <versatileparam/resourceparam.h>
#include <widget/factory.h>
#include <widget/w_state_button_base.h>

namespace paf {
namespace ui {

using sce::Vectormath::Simd::Aos::maxPerElem;
using sce::Vectormath::Simd::Aos::minPerElem;

/* the label, where it is a text object (inlined in Sony's code, name ours) */
static inline __attribute__((always_inline)) graph::TextObj *GetLabel(Widget *widget)
{
	if (widget->GetDrawObj(StateButtonBase::OBJ_LABEL) != NULL &&
	    widget->GetDrawObj(StateButtonBase::OBJ_LABEL)
		    ->IsInherit(graph::TextObj::DrawObjType()))
		return (graph::TextObj *)widget->GetDrawObj(StateButtonBase::OBJ_LABEL);
	return NULL;
}

/* defined in reverse: SNC emits a file's static functions last defined first */
static graph::PlaneObj *CreateImage(StateButtonBase *button)
{
	graph::PlaneObj *obj = new graph::PlaneObj(graph::PlaneObjStyle());
	common::SharedPtr<graph::DrawObj> ptr(obj);
	Widget::DrawObjAttr attr(StateButtonBase::OBJ_IMAGE);

	button->InsertDrawObj(button->LowerBoundDrawObj(StateButtonBase::OBJ_IMAGE),
			      Widget::DrawObjEntry(ptr, attr), false);
	return (graph::PlaneObj *)ptr.get();
}

static graph::TextObj *CreateLabel(StateButtonBase *button)
{
	graph::TextObj *obj = new graph::TextObj(graph::TextObjStyle());
	common::SharedPtr<graph::DrawObj> ptr(static_cast<graph::DrawObj *>(obj));
	Widget::DrawObjAttr attr(StateButtonBase::OBJ_LABEL);

	attr.m_flag1 = false;
	button->InsertDrawObj(button->LowerBoundDrawObj(StateButtonBase::OBJ_LABEL),
			      Widget::DrawObjEntry(ptr, attr), false);
	button->SetAnimator(new StateButtonBaseStyleAnimator);
	return (graph::TextObj *)ptr.get();
}

static graph::PlaneObj *CreateHighlight(StateButtonBase *button)
{
	graph::PlaneObj *obj = new graph::PlaneObj(graph::PlaneObjStyle());
	common::SharedPtr<graph::DrawObj> ptr(obj);
	Widget::DrawObjAttr attr(StateButtonBase::OBJ_HIGHLIGHT);

	button->InsertDrawObj(button->LowerBoundDrawObj(StateButtonBase::OBJ_HIGHLIGHT),
			      Widget::DrawObjEntry(ptr, attr), false);
	return (graph::PlaneObj *)ptr.get();
}

static graph::PlaneObj *CreateGlow(StateButtonBase *button)
{
	graph::PlaneObj *obj = new graph::PlaneObj(graph::PlaneObjStyle());
	common::SharedPtr<graph::DrawObj> ptr(obj);
	Widget::DrawObjAttr attr(StateButtonBase::OBJ_GLOW);

	button->InsertDrawObj(button->LowerBoundDrawObj(StateButtonBase::OBJ_GLOW),
			      Widget::DrawObjEntry(ptr, attr), false);
	button->SetGlowObj(StateButtonBase::OBJ_GLOW);
	return (graph::PlaneObj *)ptr.get();
}

graph::DrawObj *StateButtonBase::CreateDrawObj(int32_t obj)
{
	graph::DrawObj *draw_obj = GetDrawObj(obj);

	if (draw_obj != NULL)
		return draw_obj;
	switch (obj) {
	case OBJ_IMAGE:
		return CreateImage(this);
	case OBJ_LABEL:
		return CreateLabel(this);
	case OBJ_HIGHLIGHT:
		return CreateHighlight(this);
	case OBJ_GLOW:
		return CreateGlow(this);
	default:
		return ButtonBase::CreateDrawObj(obj);
	}
}

StateButtonBase::StateButtonBase(Widget *parent, CreateParam *param)
	: ButtonBase(parent, param), m_is_checked(false), m_frame_num(2),
	  m_frame_time(16.666666f), m_tuck(false), m_text_auto_scroll_origin(0.0f, 0.0f)
{
	if (param == NULL || param->m_create_drawobj)
		StateButtonBase::CreateDrawObj(OBJ_IMAGE);
	m_sound_mask |= 1 << SOUND_CHECK | 1 << SOUND_UNCHECK;
}

StateButtonBaseParam::StateButtonBaseParam() : m_tuck(false)
{
	m_string_list.push_back(IndexedRef("label", StateButtonBase::OBJ_LABEL));
}

/* the parameter named and indexed, among the style's draw object parameters (inlined in Sony's
   code, name ours) */
static inline __attribute__((always_inline)) void AddObjParam(WidgetStyleParam *style,
							      graph::DrawObjParam *param,
							      const char *name, int32_t index)
{
	param->m_name = name;
	param->m_index = index;
	style->m_drawobj_param_list.push_back(param);
}

StateButtonBaseStyleParam::StateButtonBaseStyleParam() : m_frame_num(2), m_frame_time(16.666666f)
{
	AddObjParam(this, &m_image_obj, "image_obj", StateButtonBase::OBJ_IMAGE);
	AddObjParam(this, &m_label_obj, "label_obj", StateButtonBase::OBJ_LABEL);
	AddObjParam(this, &m_highlight_obj, "highlight_obj", StateButtonBase::OBJ_HIGHLIGHT);
	AddObjParam(this, &m_glow_obj, "glow_obj", StateButtonBase::OBJ_GLOW);
	m_adjust = new AdjustParam;
	m_adjust->m_ref_count++;
}

/* the sounds a state button adds to a button's */
struct SoundDef {
	const char *name;
	uint32_t index;
};

extern const SoundDef s_state_button_sounds[] = {
	{ "check", StateButtonBase::SOUND_CHECK },
	{ "uncheck", StateButtonBase::SOUND_UNCHECK },
};

StateButtonBaseSoundParam::StateButtonBaseSoundParam()
{
	for (uint32_t i = 0; i < 2; i++)
		m_sound_list.push_back(Sound(s_state_button_sounds[i].name,
					     s_state_button_sounds[i].index));
}

const char StateButtonBase::m_widget_type[] = "state_button_base";

bool StateButtonBase::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return ButtonBase::IsInherit(type);
}

char *StateButtonBase::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *StateButtonBase::TypeName()
{
	return m_widget_type;
}

void StateButtonBase::DoTexAnim(bool onoff, float time)
{
	int32_t start = 0;
	int32_t end = m_frame_num - 1;

	if (!onoff) {
		start = m_frame_num - 1;
		end = 0;
	}

	Timer *t = NULL;

	if (time > 0.0f)
		t = new Timer(time, Timer::FUNC_LINEAR, Timer::REPEAT_NONE, -1, NULL);
	SetTexAnim(OBJ_IMAGE, 1, m_frame_num, start, end, t, 1, 0.0f, NULL, NULL);
}

void StateButtonBase::SendEvent(int32_t type, int32_t v0, int32_t v1, int32_t v2, int32_t v3)
{
	Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, v0, v1, v2, v3);

	DoEvent(type, &ev);
}

void StateButtonBase::SetCheck(bool onoff, float time, bool ch_anim)
{
	if (m_is_checked == onoff)
		return;

	{
		Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, onoff, m_is_checked,
			 0, 0);

		DoEvent(CB_STBTN_VALUE_CHANGED, &ev);
	}
	m_is_checked = onoff;
	/* a negative time: the whole animation's */
	if (time < 0.0f)
		time = m_frame_time * (float)m_frame_num;
	DoTexAnim(onoff, time);
	if (ch_anim)
		ScePafWidget_2B1A1493(this, m_is_checked ? SOUND_CHECK : SOUND_UNCHECK);
}

int32_t StateButtonBase::SetTextAutoScrollOrigin(const math::v2 &pos)
{
	m_text_auto_scroll_origin = pos;
	return 0;
}

int32_t StateButtonBase::GetTextAutoScrollOrigin(math::v2 &pos)
{
	pos = m_text_auto_scroll_origin;
	return 0;
}

void StateButtonBase::UpdateAdjustSize()
{
	UpdateButtonSize();
}

int32_t StateButtonBase::DoButtonEvent(int32_t type, Event *e)
{
	int32_t ret = ButtonBase::DoButtonEvent(type, e);

	if (m_tuck) {
		if (type == CB_BTN_PRESS)
			OnButtonDecide();
	} else if (type == CB_BTN_DECIDE) {
		OnButtonDecide();
	}
	return ret;
}

void StateButtonBase::OnButtonDecide()
{
}

math::v2 StateButtonBase::GetLayoutSize(const math::v2 &fit_size, const math::v2 &offs) const
{
	math::v2 size(m_size.getImpl().getXY());

	if (m_adjust_x > 1 && m_adjust_x <= 3) {
		if (fit_size.extract_x() == 0.0f)
			size.set_x(0.0f);
		else
			size.set_x(fit_size.extract_x() + __builtin_fabsf(offs.extract_x() * 2.0f));
	}
	if (m_adjust_y > 1 && m_adjust_y <= 3) {
		if (fit_size.extract_y() == 0.0f)
			size.set_y(0.0f);
		else
			size.set_y(fit_size.extract_y() + __builtin_fabsf(offs.extract_y() * 2.0f));
	}
	return size;
}

math::v2 StateButtonBase::GetChildrenFitSize()
{
	if (m_children.size() == 0) {
		math::v2 size(0.0f, 0.0f);
		math::v2 offs(0.0f, 0.0f);

		return GetLayoutSize(size, offs);
	}

	math::v2 min(3.4028235e+18f, 3.4028235e+18f);
	math::v2 max(-3.4028235e+18f, -3.4028235e+18f);

	for (list<Widget *>::iterator it = m_children.begin(); it != m_children.end(); ++it) {
		if ((*it)->m_parent_protected)
			continue;

		math::v2 half = math::v2((*it)->GetSize(0)->getImpl().getXY()) * 0.5f;
		math::v4 zero(0.0f, 0.0f, 0.0f, 0.0f);
		math::v2 pos((*it)->GetPosCenter(0, NULL, zero).getImpl().getXY());

		min = math::v2(minPerElem((pos - half).getImpl(), min.getImpl()));
		max = math::v2(maxPerElem((pos + half).getImpl(), max.getImpl()));
	}

	math::v2 size = max - min;
	math::v2 offs = (max + min) * 0.5f;

	return GetLayoutSize(size, offs);
}

math::v2 StateButtonBase::GetFitSize() const
{
	math::v3 min(0.0f, 0.0f, 0.0f);
	math::v3 max(0.0f, 0.0f, 0.0f);
	bool first = true;
	/* paf's vector has no const_iterator */
	vector<DrawObjEntry> &objs = const_cast<vector<DrawObjEntry> &>(m_drawobjs);

	for (vector<DrawObjEntry>::iterator it = objs.begin(); it != objs.end(); ++it) {
		if (it->m_attr.m_flag1)
			continue;

		graph::DrawObj *obj = it->m_obj.get();

		if (obj == NULL || !obj->m_flags.m_visible)
			continue;
		if (first) {
			graph::DrawObjBounds bounds = obj->GetBounds();

			min = bounds.m_min;
			max = bounds.m_max;
			first = false;
		} else {
			graph::DrawObjBounds bounds = obj->GetBounds();

			min = math::v3(minPerElem(bounds.m_max.getImpl(),
						  minPerElem(bounds.m_min.getImpl(), min.getImpl())));
			max = math::v3(maxPerElem(bounds.m_max.getImpl(),
						  maxPerElem(bounds.m_min.getImpl(), max.getImpl())));
		}
	}
	if (m_adjust_param != NULL) {
		min = min - math::v3(m_adjust_param->m_left_margin, m_adjust_param->m_bottom_margin,
				     0.0f);
		max = max + math::v3(m_adjust_param->m_right_margin, m_adjust_param->m_top_margin,
				     0.0f);
	}

	math::v2 size((max - min).getImpl().getXY());
	math::v2 offs(((max + min) * 0.5f).getImpl().getXY());

	return GetLayoutSize(size, offs);
}

void StateButtonBase::UpdateButtonSize()
{
	if ((m_adjust_x | m_adjust_y | m_adjust_z) == 0 || (m_adjust_x == 1 && m_adjust_y == 1))
		return;

	math::v2 fit = GetFitSize();
	math::v2 children = GetChildrenFitSize();
	math::v2 lo(0.0f, 0.0f);
	math::v2 hi(3.4028235e+18f, 3.4028235e+18f);

	if (m_adjust_param != NULL) {
		if (m_adjust_x != 0) {
			lo.set_x(m_adjust_param->m_min_size.extract_x());
			hi.set_x(m_adjust_param->m_max_size.extract_x());
		}
		if (m_adjust_y != 0) {
			lo.set_y(m_adjust_param->m_min_size.extract_y());
			hi.set_y(m_adjust_param->m_max_size.extract_y());
		}
	}

	math::v2 size(minPerElem(hi.getImpl(),
				 maxPerElem(lo.getImpl(),
					    maxPerElem(children.getImpl(), fit.getImpl()))));

	if (!(size == math::v2(m_size.getImpl().getXY())))
		SetSize_core(math::v4(size.extract_x(), size.extract_y(), m_size.extract_z(),
				      m_size.extract_w()),
			     0xFF02, false);
}

bool StateButtonBase::UpdateObjsSize()
{
	graph::TextObj *label = GetLabel(this);

	if (label == NULL)
		return true;

	bool fit_x = false;
	bool fit_y = false;

	label->GetLayoutAttribute(graph::TextLayoutAttribute_AdjustX, fit_x);
	label->GetLayoutAttribute(graph::TextLayoutAttribute_AdjustY, fit_y);

	math::v2 bounds(0.0f, 0.0f);

	if (((fit_x && m_adjust_x == 2) || (fit_y && m_adjust_y == 2)) &&
	    label->GetBounds(bounds) != 0)
		return false;

	const AdjustParam *adjust = &ScePafWidget_BE1CEA2E;

	if (m_adjust_param != NULL)
		adjust = m_adjust_param;

	math::v2 size = math::v2(m_size.getImpl().getXY()) -
			math::v2(adjust->m_right_margin + adjust->m_left_margin,
				 adjust->m_top_margin + adjust->m_bottom_margin);
	math::v2 layout_size(0.0f, 0.0f);

	label->GetLayoutAttribute(graph::TextLayoutAttribute_Size, layout_size);
	if (m_adjust_x == 2)
		size.set_x(fit_x ? bounds.extract_x() : layout_size.extract_x());
	if (m_adjust_y == 2)
		size.set_y(fit_y ? bounds.extract_y() : layout_size.extract_y());

	math::v2 obj_size(maxPerElem(math::v2(0.0f, 0.0f).getImpl(), size.getImpl()));

	if (!(obj_size == math::v2(label->m_size.getImpl().getXY())))
		label->SetSize(math::v3(obj_size.extract_x(), obj_size.extract_y(), 0.0f));

	math::v3 offs = label->m_pos;

	if (obj_size.extract_x() != 0.0f && m_adjust_x <= 1)
		offs.set_x((adjust->m_left_margin - adjust->m_right_margin) * 0.5f);
	if (obj_size.extract_y() != 0.0f && m_adjust_y <= 1)
		offs.set_y((adjust->m_bottom_margin - adjust->m_top_margin) * 0.5f);
	if (!(label->m_pos == offs))
		label->SetPos(offs);
	return true;
}

const char StateButtonBaseParam::m_param_name[] = "state_button_base";

const char *StateButtonBaseParam::GetParamName() const
{
	return m_param_name;
}

/* skips ButtonBaseParam's "enable", as Sony's */
int32_t StateButtonBaseParam::Create(int32_t mode)
{
	int32_t ret = WidgetParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL)
		m_serial->AddParam("tuck", &m_tuck, ResourceParam::SEMANTIC_BOOL);
	return 0;
}

int32_t StateButtonBaseParam::Set()
{
	int32_t ret = WidgetParam::Set();

	if (ret < 0)
		return ret;
	((StateButtonBase *)m_widget)->m_tuck = m_tuck;
	return 0;
}

int32_t StateButtonBaseParam::Get()
{
	int32_t ret = WidgetParam::Get();

	if (ret < 0)
		return ret;
	m_tuck = ((StateButtonBase *)m_widget)->m_tuck;
	return 0;
}

const char StateButtonBaseStyleParam::m_param_name[] = "style_state_button_base";

const char *StateButtonBaseStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *StateButtonBaseStyleParam::ParamName()
{
	return m_param_name;
}

StateButtonBaseStyleParam::~StateButtonBaseStyleParam()
{
	if (--m_adjust->m_ref_count == 0)
		delete m_adjust;
}

int32_t StateButtonBaseStyleParam::Create(int32_t mode)
{
	int32_t ret = ButtonBaseStyleParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("adjust_min_size", &m_adjust->m_min_size, ResourceParam::SEMANTIC_VEC4);
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("adjust_max_size", &m_adjust->m_max_size, ResourceParam::SEMANTIC_VEC4);
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("top_margin", &m_adjust->m_top_margin, ResourceParam::SEMANTIC_FLOAT);
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("bottom_margin", &m_adjust->m_bottom_margin,
			   ResourceParam::SEMANTIC_FLOAT);
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("left_margin", &m_adjust->m_left_margin, ResourceParam::SEMANTIC_FLOAT);
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("right_margin", &m_adjust->m_right_margin,
			   ResourceParam::SEMANTIC_FLOAT);
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("frame_num", &m_frame_num, ResourceParam::SEMANTIC_INT);
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("frame_time", &m_frame_time, ResourceParam::SEMANTIC_FLOAT);
	return 0;
}

int32_t StateButtonBaseStyleParam::SetEach(Widget *widget)
{
	StateButtonBase *button = (StateButtonBase *)widget;

	if (m_label_obj.m_id.GetIDHash() != 0)
		button->CreateDrawObj(StateButtonBase::OBJ_LABEL);
	if (m_highlight_obj.m_id.GetIDHash() != 0)
		button->CreateDrawObj(StateButtonBase::OBJ_HIGHLIGHT);
	if (m_glow_obj.m_id.GetIDHash() != 0)
		button->CreateDrawObj(StateButtonBase::OBJ_GLOW);

	int32_t ret = ButtonBaseStyleParam::SetEach(widget);

	if (ret < 0)
		return ret;
	button->m_frame_num = m_frame_num > 1 ? m_frame_num : 1;
	button->DoTexAnim(button->m_is_checked, 0.0f);
	button->m_frame_time = 0.0f < m_frame_time ? m_frame_time : 0.0f;
	ScePafWidget_55D83675(button, m_adjust);
	if (m_glow_obj.m_id.GetIDHash() != 0)
		button->SetGlowObj(StateButtonBase::OBJ_GLOW);
	return 0;
}

int32_t StateButtonBaseStyleParam::GetEach(Widget *widget)
{
	int32_t ret = ButtonBaseStyleParam::GetEach(widget);

	if (ret < 0)
		return ret;

	StateButtonBase *button = (StateButtonBase *)widget;

	m_frame_num = button->m_frame_num;
	m_frame_time = button->m_frame_time;
	return 0;
}

const char StateButtonBaseSoundParam::m_param_name[] = "sound_state_button_base";

const char *StateButtonBaseSoundParam::GetParamName() const
{
	return m_param_name;
}

const char *StateButtonBaseSoundParam::ParamName()
{
	return m_param_name;
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewStateButtonBase(Widget *parent)
{
	return new StateButtonBase(parent, NULL);
}

static WidgetParam *NewStateButtonBaseParam()
{
	return new StateButtonBaseParam;
}

static VersatileParam *NewStateButtonBaseLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewStateButtonBaseFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewStateButtonBaseStyleParam()
{
	return new StateButtonBaseStyleParam;
}

static VersatileParam *NewStateButtonBaseHandlerParam()
{
	return new ButtonBaseHandlerParam;
}

static VersatileParam *NewStateButtonBaseSoundParam()
{
	return new StateButtonBaseSoundParam;
}

FactoryFuncTable g_factoryStateButtonBase = {
	StateButtonBase::TypeName(),
	NewStateButtonBase,
	NewStateButtonBaseParam,
	NewStateButtonBaseLayoutParam,
	NewStateButtonBaseFocusParam,
	NewStateButtonBaseStyleParam,
	NewStateButtonBaseHandlerParam,
	NewStateButtonBaseSoundParam,
};
