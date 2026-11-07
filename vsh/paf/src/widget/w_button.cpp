/*
 * paf::ui::Button (DEBUGGER 0x810df010..0x810e109c): a ButtonBase with a background plane (0), a
 * text label (1), a highlight (67) and a glow (68). Where it adjusts it is sized to its label, its
 * children and the margins of its adjust parameter, its style animator laying the label out; it
 * plays the background's texture animation as it is pressed and released, and scrolls its label
 * and its texts (TextAutoScroll) on a long press. No record or string names the file; the names
 * of the static functions are ours.
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
#include <widget/text_auto_scroll.h>
#include <widget/w_button.h>
#include "../std/math.h"
#include "../paf_types.h"

namespace paf {
namespace ui {

using sce::Vectormath::Simd::Aos::maxPerElem;
using sce::Vectormath::Simd::Aos::minPerElem;

/* the label, where it is a text object (inlined in Sony's code, name ours) */
static inline __attribute__((always_inline)) graph::TextObj *GetLabel(Widget *widget)
{
	if (widget->GetDrawObj(Button::OBJ_LABEL) != NULL &&
	    widget->GetDrawObj(Button::OBJ_LABEL)->IsInherit(graph::TextObj::DrawObjType()))
		return (graph::TextObj *)widget->GetDrawObj(Button::OBJ_LABEL);
	return NULL;
}

/* the widget as a text, where it is one (inlined in Sony's code, name ours) */
static inline __attribute__((always_inline)) Text *AsText(Widget *const &widget)
{
	if (widget != NULL && widget->IsInherit(Text::TypeName()))
		return (Text *)widget;
	return NULL;
}

/* defined in reverse: SNC emits a file's static functions last defined first */
static graph::PlaneObj *CreateBg(Button *button)
{
	graph::DrawStyle style;

	style.blend = 1;
	style.depth = 1;
	style.lighting = 1;
	style.face_cull = 0;
	style.scissor = 0;
	style.deepen_color = 0;
	style.reduce_transparency = 0;
	style.surface_draw = 0;
	style.reserved1 = 0;

	graph::PlaneObj *obj = new graph::PlaneObj(style);
	common::SharedPtr<graph::DrawObj> ptr(obj);
	Widget::DrawObjAttr attr(Button::OBJ_BG);

	button->InsertDrawObj(button->LowerBoundDrawObj(Button::OBJ_BG),
			      Widget::DrawObjEntry(ptr, attr), false);
	return (graph::PlaneObj *)ptr.get();
}

static graph::TextObj *CreateLabel(Button *button)
{
	graph::DrawStyle style;

	style.blend = 1;
	style.depth = 1;
	style.lighting = 1;
	style.face_cull = 0;
	style.scissor = 0;
	style.deepen_color = 0;
	style.reduce_transparency = 0;
	style.surface_draw = 0;
	style.reserved1 = 0;

	graph::TextObj *obj = new graph::TextObj(style);
	common::SharedPtr<graph::DrawObj> ptr(obj);
	Widget::DrawObjAttr attr(Button::OBJ_LABEL);

	attr.m_flag1 = false;
	button->InsertDrawObj(button->LowerBoundDrawObj(Button::OBJ_LABEL),
			      Widget::DrawObjEntry(ptr, attr), false);
	return (graph::TextObj *)ptr.get();
}

static graph::PlaneObj *CreateHighlight(Button *button)
{
	graph::DrawStyle style;

	style.blend = 1;
	style.depth = 1;
	style.lighting = 1;
	style.face_cull = 0;
	style.scissor = 0;
	style.deepen_color = 0;
	style.reduce_transparency = 0;
	style.surface_draw = 0;
	style.reserved1 = 0;

	graph::PlaneObj *obj = new graph::PlaneObj(style);
	common::SharedPtr<graph::DrawObj> ptr(obj);
	Widget::DrawObjAttr attr(Button::OBJ_HIGHLIGHT);

	button->InsertDrawObj(button->LowerBoundDrawObj(Button::OBJ_HIGHLIGHT),
			      Widget::DrawObjEntry(ptr, attr), false);
	return (graph::PlaneObj *)ptr.get();
}

static graph::PlaneObj *CreateGlow(Button *button)
{
	graph::PlaneObj *obj = new graph::PlaneObj(graph::PlaneObjStyle());
	common::SharedPtr<graph::DrawObj> ptr(obj);
	Widget::DrawObjAttr attr(Button::OBJ_GLOW);

	button->InsertDrawObj(button->LowerBoundDrawObj(Button::OBJ_GLOW),
			      Widget::DrawObjEntry(ptr, attr), false);
	button->SetGlowObj(Button::OBJ_GLOW);
	return (graph::PlaneObj *)ptr.get();
}

/* the style type and its animator (inlined in Sony's code, name ours) */
static inline __attribute__((always_inline)) void SetStyleType(Button *button,
							       Button::StyleType type)
{
	button->m_style_type = type;
	button->SetStyleAnimator(type);
}

Button::Button(Widget *parent, CreateParam *param)
	: ButtonBase(parent, param), m_unused0(0), m_unused1(0), m_unused2(0),
	  m_style_type(STYLE_AUTO), m_frame_num(0), m_frame_time(0.0f), m_text_auto_scroll(NULL)
{
	/* through the vtable, as Sony's */
	Button *self = this;

	if (param == NULL || param->m_create_drawobj) {
		ReserveDrawObj(4);
		CreateBg(this);
		CreateLabel(this);
		CreateHighlight(this)->SetVisible(false);
		CreateGlow(this)->SetVisible(false);
		SetGlowObj(-1);
	}
	SetStyleType(self, self->m_style_type);
}

/* the strings a button's resource gives its draw objects: the label's */
static const struct {
	const char *name;
	int32_t index;
} s_button_strings[] = {
	{ "label", Button::OBJ_LABEL },
};

ButtonParam::ButtonParam()
	: m_enable_long_press_text_auto_scroll(false), m_long_press_text_auto_scroll_begin_msec(-1)
{
	for (uint32_t i = 0; i < sizeof(s_button_strings) / sizeof(s_button_strings[0]); i++)
		m_string_list.push_back(IndexedRef(s_button_strings[i].name, s_button_strings[i].index));
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

ButtonStyleParam::ButtonStyleParam()
	: m_style_type(Button::STYLE_AUTO), m_adjust(NULL), m_frame_num(0), m_frame_time(0.0f)
{
	AddObjParam(this, &m_bg_obj, "bg_obj", Button::OBJ_BG);
	AddObjParam(this, &m_label_obj, "label_obj", Button::OBJ_LABEL);
	AddObjParam(this, &m_highlight_obj, "highlight_obj", Button::OBJ_HIGHLIGHT);
	AddObjParam(this, &m_glow_obj, "glow_obj", Button::OBJ_GLOW);
	m_adjust = new AdjustParam;
	m_adjust->m_ref_count++;
}

const char Button::m_widget_type[] = "button";

const char *Button::TypeName()
{
	return m_widget_type;
}

int32_t StyleAnimator::OnUpdate_core()
{
	m_adjust_x = m_widget->m_adjust_x;
	m_adjust_y = m_widget->m_adjust_y;
	m_adjust_z = m_widget->m_adjust_z;
	if (m_last_adjust_x != m_adjust_x || m_last_adjust_y != m_adjust_y ||
	    m_last_adjust_z != m_adjust_z) {
		m_last_adjust_x = m_adjust_x;
		m_last_adjust_y = m_adjust_y;
		m_last_adjust_z = m_adjust_z;
		SetDirty(1);
	}
	if (m_dirty != 0 && Layout() != 0)
		m_dirty = 0;
	m_first = 0;
	return 0;
}

bool Button::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return ButtonBase::IsInherit(type);
}

char *Button::GetTypeName() const
{
	return (char *)m_widget_type;
}

Button::~Button()
{
	if (m_text_auto_scroll != NULL) {
		delete m_text_auto_scroll;
		m_text_auto_scroll = NULL;
	}
}

int32_t Button::SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj, uint32_t index)
{
	return ButtonBase::SetTexture(surf, obj, index);
}

int32_t Button::SetTexture(const intrusive_ptr<graph::Surface> &surf)
{
	return ButtonBase::SetTexture(surf, OBJ_BG, 0);
}

void Button::DoTexAnim(bool forward)
{
	int32_t num = m_frame_num;

	if (num <= 0)
		return;

	int32_t start = 0;
	int32_t end = num - 1;

	if (!forward) {
		start = num - 1;
		end = 0;
	}

	Timer *t = NULL;
	float time = m_frame_time * (float)num;

	if (time > 0.0f)
		t = new Timer(time, Timer::FUNC_LINEAR, Timer::REPEAT_NONE, -1, NULL);
	SetTexAnim(OBJ_BG, 1, m_frame_num, start, end, t, 1, 0.0f, NULL, NULL);
}

void Button::UpdateAdjustSize()
{
	UpdateButtonSize();
}

bool Button::UpdateObjsSize()
{
	graph::TextObj *label = GetLabel(this);

	if (label == NULL)
		return false;

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

	math::v4 size = m_size - math::v4(adjust->m_right_margin + adjust->m_left_margin,
					  adjust->m_top_margin + adjust->m_bottom_margin, 0.0f,
					  0.0f);
	math::v2 layout_size(0.0f, 0.0f);

	label->GetLayoutAttribute(graph::TextLayoutAttribute_Size, layout_size);
	if (m_adjust_x == 2)
		size.set_x(fit_x ? bounds.extract_x() : layout_size.extract_x());
	if (m_adjust_y == 2)
		size.set_y(fit_y ? bounds.extract_y() : layout_size.extract_y());
	if (m_adjust_z == 2)
		size.set_z(0.0f);

	math::v3 obj_size(maxPerElem(math::v4(0.0f, 0.0f, 0.0f, 0.0f).getImpl(), size.getImpl())
				  .getXYZ());

	if (!(label->m_size == obj_size))
		label->SetSize(obj_size);

	math::v3 offs(0.0f, 0.0f, 0.0f);

	if (obj_size.extract_x() != 0.0f && m_adjust_x <= 1)
		offs.set_x((adjust->m_left_margin - adjust->m_right_margin) * 0.5f);
	if (obj_size.extract_y() != 0.0f && m_adjust_y <= 1)
		offs.set_y((adjust->m_bottom_margin - adjust->m_top_margin) * 0.5f);

	graph::TextLayoutLineInfo info;

	if (label->GetLineInfo(0, info) == 0)
		offs.set_y((float)(int32_t)(offs.extract_y() +
					    ((info.descender + info.ascender) -
					     info.frame.extract_w()) * 0.5f));
	if (!(label->m_pos == offs))
		label->SetPos(offs);
	return true;
}

math::v4 Button::GetLayoutSize(const math::v4 &fit_size, const math::v4 &offs) const
{
	math::v4 size = m_size;

	if (m_adjust_x > 1 && m_adjust_x <= 3)
		size.set_x(fit_size.extract_x() + __builtin_fabsf(offs.extract_x() * 2.0f));
	if (m_adjust_y > 1 && m_adjust_y <= 3)
		size.set_y(fit_size.extract_y() + __builtin_fabsf(offs.extract_y() * 2.0f));
	if (m_adjust_z > 1 && m_adjust_z <= 3)
		size.set_z(fit_size.extract_z());
	return size;
}

void Button::GetChildrenFitGeometry(math::v4 *size, math::v4 *offs)
{
	math::v4 min(9.2233720e+18f, 9.2233720e+18f, 9.2233720e+18f, 9.2233720e+18f);
	math::v4 max(-9.2233720e+18f, -9.2233720e+18f, -9.2233720e+18f, -9.2233720e+18f);

	for (list<Widget *>::iterator it = m_children.begin(); it != m_children.end(); ++it) {
		if ((*it)->m_parent_protected)
			continue;

		math::v4 zero(0.0f, 0.0f, 0.0f, 0.0f);
		math::v4 half = math::v4(math::v3((*it)->GetSize(0)->getImpl().getXYZ()), 0.0f) *
				0.5f;
		math::v4 pos = math::v4(math::v3((*it)->GetPos(0, NULL, zero).getImpl().getXYZ()),
					0.0f);

		min = math::v4(minPerElem((pos - half).getImpl(), min.getImpl()));
		max = math::v4(maxPerElem((pos + half).getImpl(), max.getImpl()));
	}
	*offs = (max + min) / 2.0f;
	*size = max - min;
}

void Button::GetFitGeometry(math::v4 *size, math::v4 *offs) const
{
	math::v4 min(9.2233720e+18f, 9.2233720e+18f, 9.2233720e+18f, 9.2233720e+18f);
	math::v4 max(-9.2233720e+18f, -9.2233720e+18f, -9.2233720e+18f, -9.2233720e+18f);
	/* paf's vector has no const_iterator */
	vector<DrawObjEntry> &objs = const_cast<vector<DrawObjEntry> &>(m_drawobjs);

	for (vector<DrawObjEntry>::iterator it = objs.begin(); it != objs.end(); ++it) {
		if (it->m_attr.m_flag1)
			continue;

		graph::DrawObj *obj = it->m_obj.get();

		if (obj == NULL || !obj->m_flags.m_visible)
			continue;

		math::v4 half = math::v4(obj->m_size, 0.0f) * 0.5f;
		math::v4 pos = math::v4(obj->m_pos, 0.0f);

		min = math::v4(minPerElem((pos - half).getImpl(), min.getImpl()));
		max = math::v4(maxPerElem((pos + half).getImpl(), max.getImpl()));
	}
	if (m_adjust_param != NULL) {
		min = min - math::v4(m_adjust_param->m_left_margin, m_adjust_param->m_bottom_margin,
				     0.0f, 0.0f);
		max = max + math::v4(m_adjust_param->m_right_margin, m_adjust_param->m_top_margin,
				     0.0f, 0.0f);
	}
	*offs = (max + min) / 2.0f;
	*size = max - min;
}

void Button::UpdateButtonSize()
{
	if ((m_adjust_z | m_adjust_x | m_adjust_y) == 0)
		return;

	math::v4 fit_size;
	math::v4 fit_offs;
	math::v4 children_size;
	math::v4 children_offs;

	GetFitGeometry(&fit_size, &fit_offs);
	GetChildrenFitGeometry(&children_size, &children_offs);

	math::v4 fit_layout = GetLayoutSize(fit_size, fit_offs);
	math::v4 children_layout = GetLayoutSize(children_size, children_offs);
	math::v4 size(maxPerElem(children_layout.getImpl(), fit_layout.getImpl()));
	math::v4 lo(0.0f, 0.0f, 0.0f, 0.0f);
	math::v4 hi(3.4028235e+18f, 3.4028235e+18f, 3.4028235e+18f, 3.4028235e+18f);
	math::v4 min_size;
	math::v4 max_size;

	if (m_adjust_param != NULL) {
		min_size = m_adjust_param->m_min_size;
		max_size = m_adjust_param->m_max_size;
	} else {
		min_size = ScePafWidget_BE1CEA2E.m_min_size;
		max_size = ScePafWidget_BE1CEA2E.m_max_size;
	}

	if (m_adjust_x != 0) {
		lo.set_x(min_size.extract_x());
		hi.set_x(max_size.extract_x());
	}
	if (m_adjust_y != 0) {
		lo.set_y(min_size.extract_y());
		hi.set_y(max_size.extract_y());
	}
	if (m_adjust_z != 0) {
		lo.set_z(min_size.extract_z());
		hi.set_z(max_size.extract_z());
	}
	size = math::v4(minPerElem(hi.getImpl(), maxPerElem(lo.getImpl(), size.getImpl())));
	if (!(m_size == size))
		SetSize_core(size, 0xFF02, false);
}

uint32_t Button::ChangeButtonState(ButtonState state)
{
	uint32_t old = m_btn_state;
	uint32_t ret = ButtonBase::ChangeButtonState(state);

	if (ret != old) {
		if (ret == ST_BTN_PRESS)
			DoTexAnim(true);
		else if (old == ST_BTN_PRESS)
			DoTexAnim(false);
	}
	return ret;
}

graph::DrawObj *Button::CreateDrawObj(int32_t obj)
{
	graph::DrawObj *draw_obj = GetDrawObj(obj);

	if (draw_obj != NULL)
		return draw_obj;
	switch (obj) {
	case OBJ_BG:
		return CreateBg(this);
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

int32_t Button::IsTextAutoScroll()
{
	if (m_text_auto_scroll == NULL)
		return 0;

	vector<Widget *> texts;

	GetChildrenByType(Text::TypeName(), texts);

	bool scrolling = false;

	for (vector<Widget *>::iterator it = texts.begin(); it != texts.end(); ++it) {
		Text *text = AsText(*it);

		if (text != NULL)
			scrolling |= text->IsScrolling();
	}
	return (m_text_auto_scroll->IsRunning() | scrolling) ? 1 : 0;
}

int32_t Button::SetTextAutoScrollOrigin(const math::v2 &pos)
{
	if (m_text_auto_scroll == NULL)
		return SCE_PAF_ERROR_NO_OBJECT;

	m_text_auto_scroll->m_origin = pos;

	int32_t ret = 0;
	vector<Widget *> texts;

	GetChildrenByType(Text::TypeName(), texts);
	for (vector<Widget *>::iterator it = texts.begin(); it != texts.end(); ++it) {
		Text *text = AsText(*it);

		if (text != NULL)
			ret = ScePafWidget_8A5AE1CD(text, pos);
	}
	return ret;
}

int32_t Button::GetTextAutoScrollOrigin(math::v2 &pos)
{
	if (m_text_auto_scroll == NULL)
		return SCE_PAF_ERROR_NO_OBJECT;
	pos = m_text_auto_scroll->m_origin;
	return 0;
}

int32_t Button::GetTextAutoScrollSpeedByPos(const math::v2 &pos, float &speed, bool set_origin,
					    bool reverse)
{
	if (m_text_auto_scroll == NULL)
		return SCE_PAF_ERROR_NO_OBJECT;
	if (m_text_auto_scroll->IsRunning())
		return m_text_auto_scroll->GetSpeedByPos(pos, speed, set_origin, reverse);

	vector<Widget *> texts;

	GetChildrenByType(Text::TypeName(), texts);
	for (vector<Widget *>::iterator it = texts.begin(); it != texts.end(); ++it) {
		Text *text = AsText(*it);

		if (text != NULL && text->IsScrolling())
			return ScePafWidget_5B2F5119(text, pos, speed, set_origin, reverse);
	}
	return SCE_PAF_ERROR_NO_OBJECT;
}

int32_t Button::StartTextAutoScroll(Text::ScrollType type)
{
	if (IsTextAutoScroll())
		return SCE_PAF_ERROR_BUTTON_BUSY;
	if (m_text_auto_scroll == NULL) {
		graph::TextObj *label = GetLabel(this);

		m_text_auto_scroll = new TextAutoScroll(this, label);
		if (m_text_auto_scroll == NULL)
			return SCE_PAF_ERROR_NO_OBJECT;
	}
	if (type == Text::SCROLL_TYPE_HORIZONTAL_ONCE || type == Text::SCROLL_TYPE_VERTICAL_ONCE) {
		float speed = 0.0f;

		TextAutoScroll::GetDefaultSpeed(speed, Text::SCROLL_TYPE_HORIZONTAL_ONCE);
		SetTextAutoScrollSpeed(speed);
	}

	vector<Widget *> texts;

	GetChildrenByType(Text::TypeName(), texts);
	for (vector<Widget *>::iterator it = texts.begin(); it != texts.end(); ++it) {
		Text *text = AsText(*it);

		if (text != NULL)
			text->StartAutoScroll(Text::SCROLL_TYPE_HORIZONTAL_ONCE);
	}

	Text::ScrollType scroll = Text::SCROLL_TYPE_HORIZONTAL;

	switch (type) {
	case Text::SCROLL_TYPE_HORIZONTAL:
		break;
	case Text::SCROLL_TYPE_VERTICAL:
		scroll = Text::SCROLL_TYPE_VERTICAL;
		break;
	case Text::SCROLL_TYPE_HORIZONTAL_ONCE:
		scroll = Text::SCROLL_TYPE_HORIZONTAL_ONCE;
		break;
	case Text::SCROLL_TYPE_VERTICAL_ONCE:
		scroll = Text::SCROLL_TYPE_VERTICAL_ONCE;
		break;
	}
	return m_text_auto_scroll->Start(scroll);
}

int32_t Button::StopTextAutoScroll()
{
	if (!IsTextAutoScroll())
		return SCE_PAF_ERROR_BUTTON_BUSY;
	if (m_text_auto_scroll == NULL)
		return SCE_PAF_ERROR_NO_OBJECT;

	vector<Widget *> texts;

	GetChildrenByType(Text::TypeName(), texts);
	for (vector<Widget *>::iterator it = texts.begin(); it != texts.end(); ++it) {
		Text *text = AsText(*it);

		if (text != NULL)
			text->StopAutoScroll();
	}
	return m_text_auto_scroll->Stop();
}

int32_t Button::SetTextAutoScrollSpeed(float speed)
{
	if (m_text_auto_scroll == NULL)
		return SCE_PAF_ERROR_NO_OBJECT;

	vector<Widget *> texts;

	GetChildrenByType(Text::TypeName(), texts);
	for (vector<Widget *>::iterator it = texts.begin(); it != texts.end(); ++it) {
		Text *text = AsText(*it);

		if (text != NULL)
			ScePafWidget_7C976DF2(text, speed);
	}
	return m_text_auto_scroll->SetSpeed(speed, 2);
}

void Button::SetStyleAnimator(StyleType style_type)
{
	Animator *animator = NULL;

	if (style_type != STYLE_MANUAL)
		animator = new ButtonStyleAnimator;
	SetAnimator(animator);
}

const char ButtonParam::m_param_name[] = "button_base";

const char *ButtonParam::GetParamName() const
{
	return m_param_name;
}

const char *ButtonParam::ParamName()
{
	return m_param_name;
}

int32_t ButtonParam::Create(int32_t mode)
{
	int32_t ret = ButtonBaseParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL) {
		m_serial->AddParam("enable_long_press_text_auto_scroll",
				   &m_enable_long_press_text_auto_scroll, ResourceParam::SEMANTIC_BOOL);
		if (m_serial != NULL)
			m_serial->AddParam("long_press_text_auto_scroll_begin_msec",
					   &m_long_press_text_auto_scroll_begin_msec,
					   ResourceParam::SEMANTIC_INT);
	}
	return 0;
}

int32_t ButtonParam::Set()
{
	int32_t ret = ButtonBaseParam::Set();

	if (ret < 0)
		return ret;

	Button *button = (Button *)m_widget;

	if (m_enable_long_press_text_auto_scroll != button->IsLongPressTextAutoScrollEnabled() &&
	    m_enable_long_press_text_auto_scroll) {
		if (m_long_press_text_auto_scroll_begin_msec == -1)
			button->EnableLongPressTextAutoScroll(500);
		else
			button->EnableLongPressTextAutoScroll(m_long_press_text_auto_scroll_begin_msec);
	}
	return 0;
}

int32_t ButtonParam::Get()
{
	int32_t ret = ButtonBaseParam::Get();

	if (ret < 0)
		return ret;
	m_enable_long_press_text_auto_scroll =
		((Button *)m_widget)->IsLongPressTextAutoScrollEnabled();
	return 0;
}

const char ButtonStyleParam::m_param_name[] = "style_button";

const char *ButtonStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *ButtonStyleParam::ParamName()
{
	return m_param_name;
}

ButtonStyleParam::~ButtonStyleParam()
{
	if (m_adjust != NULL) {
		if (m_adjust->m_ref_count-- == 1)
			delete m_adjust;
		m_adjust = NULL;
	}
}

int32_t ButtonStyleParam::Init()
{
	int32_t ret = WidgetStyleParam::Init();

	if (ret < 0)
		return ret;
	return 0;
}

int32_t ButtonStyleParam::Create(int32_t mode)
{
	int32_t ret = ButtonBaseStyleParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_adjust == NULL)
		return -1;
	if (m_serial != NULL)
		m_serial->AddParam("style_type", &m_style_type, ResourceParam::SEMANTIC_INT);
	if (m_serial != NULL)
		m_serial->AddParam("adjust_min_size", &m_adjust->m_min_size,
				   ResourceParam::SEMANTIC_VEC4);
	if (m_serial != NULL)
		m_serial->AddParam("adjust_max_size", &m_adjust->m_max_size,
				   ResourceParam::SEMANTIC_VEC4);
	if (m_serial != NULL)
		m_serial->AddParam("top_margin", &m_adjust->m_top_margin,
				   ResourceParam::SEMANTIC_FLOAT);
	if (m_serial != NULL)
		m_serial->AddParam("bottom_margin", &m_adjust->m_bottom_margin,
				   ResourceParam::SEMANTIC_FLOAT);
	if (m_serial != NULL) {
		m_serial->AddParam("left_margin", &m_adjust->m_left_margin,
				   ResourceParam::SEMANTIC_FLOAT);
		if (m_serial != NULL) {
			m_serial->AddParam("right_margin", &m_adjust->m_right_margin,
					   ResourceParam::SEMANTIC_FLOAT);
			if (m_serial != NULL) {
				m_serial->AddParam("frame_num", &m_frame_num,
						   ResourceParam::SEMANTIC_INT);
				if (m_serial != NULL)
					m_serial->AddParam("frame_time", &m_frame_time,
							   ResourceParam::SEMANTIC_FLOAT);
			}
		}
	}
	return 0;
}

/* a draw object parameter without an ID draws nothing: its alpha 0 (inlined in Sony's code,
   name ours) */
static inline __attribute__((always_inline)) void HideUnset(graph::DrawObjParam *param)
{
	if (param->m_id.GetIDHash() == 0)
		param->m_color.set_w(0.0f);
}

int32_t ButtonStyleParam::SetEach(Widget *widget)
{
	HideUnset(&m_bg_obj);
	HideUnset(&m_label_obj);
	if (m_highlight_obj.m_id.GetIDHash() == 0) {
		graph::DrawObj *obj = widget->GetDrawObj(Button::OBJ_HIGHLIGHT);

		if (obj != NULL)
			obj->SetVisible(false);
		m_highlight_obj.m_color.set_w(0.0f);
	} else {
		graph::DrawObj *obj = widget->GetDrawObj(Button::OBJ_HIGHLIGHT);

		if (obj != NULL)
			obj->SetVisible(true);
	}

	int32_t ret = ButtonBaseStyleParam::SetEach(widget);

	if (ret < 0)
		return ret;
	if (widget != NULL) {
		Button *button = (Button *)widget;

		SetStyleType(button, m_style_type);
		ScePafWidget_55D83675(button, m_adjust);
		if (m_glow_obj.m_id.GetIDHash() == 0) {
			button->SetGlowObj(-1);
			m_glow_obj.m_color.set_w(0.0f);
		} else {
			button->SetGlowObj(Button::OBJ_GLOW);
		}
		button->m_frame_num = m_frame_num > 0 ? m_frame_num : 0;
		button->DoTexAnim(button->m_btn_state == ButtonBase::ST_BTN_PRESS);
		button->m_frame_time = 0.0f < m_frame_time ? m_frame_time : 0.0f;
	}
	return 0;
}

int32_t ButtonStyleParam::GetEach(Widget *widget)
{
	int32_t ret = ButtonBaseStyleParam::GetEach(widget);

	if (ret < 0)
		return ret;
	if (widget != NULL) {
		Button *button = (Button *)widget;

		m_style_type = button->m_style_type;
		m_frame_time = button->m_frame_time;
		m_frame_num = button->m_frame_num;
	}
	return 0;
}

const char ButtonHandlerParam::m_param_name[] = "handler_button";

const char *ButtonHandlerParam::GetParamName() const
{
	return m_param_name;
}

const char *ButtonHandlerParam::ParamName()
{
	return m_param_name;
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewButton(Widget *parent)
{
	return new Button(parent, NULL);
}

static WidgetParam *NewButtonParam()
{
	return new ButtonParam;
}

static VersatileParam *NewButtonLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewButtonFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewButtonStyleParam()
{
	return new ButtonStyleParam;
}

static VersatileParam *NewButtonHandlerParam()
{
	return new ButtonHandlerParam;
}

static VersatileParam *NewButtonSoundParam()
{
	return new ButtonBaseSoundParam;
}

FactoryFuncTable g_factoryButton = {
	Button::TypeName(),
	NewButton,
	NewButtonParam,
	NewButtonLayoutParam,
	NewButtonFocusParam,
	NewButtonStyleParam,
	NewButtonHandlerParam,
	NewButtonSoundParam,
};
