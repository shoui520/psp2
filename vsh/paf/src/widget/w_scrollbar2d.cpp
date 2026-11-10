/*
 * paf::ui::ScrollBar2d (DEBUGGER 0x811243f4..0x811253c8): a slider over its base on both axes,
 * as large against the base as the visible amount is against the maximum and placed as the
 * value is; dragged, it sets the value. No record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <versatileparam/resourceparam.h>
#include <widget/core/event.h>
#include <widget/core/handler.h>
#include <widget/factory.h>
#include <widget/w_plane.h>
#include <widget/w_scroll_bar2d.h>

namespace paf {
namespace ui {

/* the vector's two lanes, the others zero (name ours) */
static inline math::v4 ToV4(math::v2 v)
{
	return math::v4(v.extract_x(), v.extract_y(), 0.0f, 0.0f);
}

/* the two vectors multiplied lane by lane (name ours) */
static inline math::v4 MulPerElem(math::v4 a, math::v4 b)
{
	return math::v4(sce::Vectormath::Simd::Aos::mulPerElem(a.getImpl(), b.getImpl()));
}

/* the base on the bar itself, the slider a plane dragged on both axes */
ScrollBar2d::ScrollBar2d(Widget *parent, CreateParam *param)
	: Widget(parent, param), m_max_value(100.0f, 100.0f), m_visible_amount(100.0f, 100.0f),
	  m_value(0.0f, 0.0f), m_slider_anim_time(150.0f), m_slider_anim_func(0),
	  m_visible_rate(1.0f, 1.0f), m_value_rate(0.0f, 0.0f), m_offset_rate(0.0f, 0.0f),
	  m_touch_id(-1)
{
	m_hit_check = 0;
	m_hit_detect_b_mode = 0;
	m_hit_detect_mode = 0;
	m_hit_find_mode = 0;
	m_event_enable |= 0xFBF0;
	AddDrawObj(DrawObjEntry(new graph::PlaneObj(graph::PlaneObjStyle()), OBJ_BASE), false);

	CreateParam create;

	create.m_parent_protected = true;

	Plane *slider = new Plane(this, &create);

	if (slider != NULL) {
		slider->m_hit_check = 1;
		slider->m_event_enable |= 0x10C0;
		slider->SetDragMode(0x110);
		slider->AddEventListener(0x2000D, new EventCBListener(OnDragSliderEvent, this), false);
		slider->AddEventListener(0x2000E, new EventCBListener(OnDragSliderEvent, this), false);
		slider->AddEventListener(0x2000F, new EventCBListener(OnDragSliderEvent, this), false);
	}
	m_parts.resize(2);
	m_parts[0] = this;
	m_parts[1] = slider;
}

const ScrollBar2dParam::TextureRef ScrollBar2dParam::c_textureList[] = {
	{ "texture_slider", ScrollBar2d::OBJ_SLIDER },
};

ScrollBar2dParam::ScrollBar2dParam()
{
	for (uint32_t i = 0; i < sizeof(c_textureList) / sizeof(c_textureList[0]); i++)
		m_texture_list.push_back(IndexedRef(c_textureList[i].name, c_textureList[i].index));
}

ScrollBar2dStyleParam::ScrollBar2dStyleParam()
{
	m_anim_time = 150.0f;
	m_anim_func = 0;
	m_base_obj.m_name = "base_obj";
	m_base_obj.m_index = ScrollBar2d::OBJ_BASE;
	m_drawobj_param_list.push_back(&m_base_obj);
	m_slider_obj.m_name = "slider_obj";
	m_slider_obj.m_index = ScrollBar2d::OBJ_SLIDER;
	m_drawobj_param_list.push_back(&m_slider_obj);
}

const char ScrollBar2d::m_widget_type[] = "scrollbar2d";

bool ScrollBar2d::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *ScrollBar2d::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *ScrollBar2d::TypeName()
{
	return m_widget_type;
}

ScrollBar2d::~ScrollBar2d()
{
}

graph::DrawObj *ScrollBar2d::GetDrawObj(int32_t obj)
{
	int32_t child;
	Widget *widget = GetPartsWidget(obj, &child);

	if (widget == NULL)
		return NULL;
	if (widget == this)
		return Widget::GetDrawObj(obj);
	return widget->GetDrawObj(child);
}

int32_t ScrollBar2d::SetObjColor(const math::v4 &color, int32_t obj)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw == NULL)
		return -1;
	draw->SetColor(color);
	return 0;
}

int32_t ScrollBar2d::GetObjColor(math::v4 &color, int32_t obj)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw == NULL)
		return -1;
	color = draw->GetColor();
	return 0;
}

/* called whole by the setters */
__attribute__((noinline)) void ScrollBar2d::AdjustParameter(bool anim)
{
	using namespace sce::Vectormath::Simd::Aos;

	Vector2 max = m_max_value.getImpl();
	Vector2 visible = minPerElem(max, m_visible_amount.getImpl());
	Vector2 value = m_value.getImpl();

	m_visible_amount = math::v2(visible);
	value = maxPerElem(Vector2(0.0f), minPerElem(max - visible, value));
	m_value = math::v2(value);

	Vector2 visible_rate = divPerElem(visible, max);

	m_visible_rate = math::v2(visible_rate);
	m_value_rate = math::v2(divPerElem(value, max));
	m_offset_rate = math::v2(mulPerElem(Vector2(1.0f) - visible_rate, Vector2(0.5f)));
	UpdateSlider(anim);
}

int32_t ScrollBar2d::SetMaxValue(const math::v2 &value)
{
	if (value.extract_x() <= 0.0f || value.extract_y() <= 0.0f)
		return 0x80AF008E;
	m_max_value = value;
	AdjustParameter(false);
	return 0;
}

int32_t ScrollBar2d::SetVisibleAmount(const math::v2 &value)
{
	if (value.extract_x() <= 0.0f || value.extract_y() <= 0.0f)
		return 0x80AF008E;
	m_visible_amount = value;
	AdjustParameter(false);
	return 0;
}

int32_t ScrollBar2d::SetValue(const math::v2 &value, bool anim)
{
	m_value = value;
	AdjustParameter(anim);
	return 0;
}

int32_t ScrollBar2d::SetSliderAnim(float time, int32_t func)
{
	if (time <= 0.0f)
		return 0x80AF008E;
	m_slider_anim_time = time;
	m_slider_anim_func = func;
	return 0;
}

int32_t ScrollBar2d::SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				uint32_t index)
{
	int32_t child;
	Widget *widget = GetPartsWidget(obj, &child);

	if (widget == NULL)
		return -1;
	if (widget != this)
		return widget->SetTexture(surf, child, index);

	graph::DrawObj *draw = Widget::GetDrawObj(obj);

	if (draw == NULL)
		return 0x80AF4101;
	draw->SetTexture(surf, index);
	return 0;
}

uint32_t ScrollBar2d::CheckState()
{
	if (m_hold_count > 0)
		return m_state;
	if (IsPrepareObjs())
		return 3;
	for (vector<Widget *>::iterator it = m_parts.begin(); it != m_parts.end(); ++it) {
		Widget *part = *it;

		if (part != NULL && part != this && part->m_state == 3)
			return 3;
	}
	return 0;
}

void ScrollBar2d::UpdateSize()
{
	Widget::UpdateSize();
	UpdateSlider(false);
}

int32_t ScrollBar2d::HandlePointEvent(Event *e)
{
	OnTouch(e);
	return Handler::HandlePointEvent(e);
}

int32_t ScrollBar2d::HandleTouchEvent(Event *e)
{
	OnTouch(e);
	return Widget::HandleTouchEvent(e);
}

Widget *ScrollBar2d::GetPartsWidget(int32_t obj, int32_t *child_obj)
{
	Widget *widget;

	switch (obj) {
	case OBJ_BASE:
		widget = m_parts[0];
		break;
	case OBJ_SLIDER:
		widget = m_parts[1];
		break;
	default:
		return NULL;
	}
	if (child_obj != NULL)
		*child_obj = 0;
	return widget;
}

int32_t ScrollBar2d::SetColor_core(const math::v4 &v)
{
	int32_t ret = Widget::SetColor_core(v);

	for (vector<Widget *>::iterator it = m_parts.begin(); it != m_parts.end(); ++it) {
		Widget *part = *it;

		if (part != NULL && part != this)
			part->SetColor(v);
	}
	return ret;
}

/* the slider sized and placed to the value, the bounds of its drag set to the base's */
void ScrollBar2d::UpdateSlider(bool anim)
{
	Widget *slider = m_parts[1];

	if (slider == NULL)
		return;
	slider->KillAnim(ANIM_MOVE);

	math::v2 offset = m_offset_rate;
	math::v4 size = m_size;
	math::v4 origin = m_origin_offs;
	math::v4 range = MulPerElem(ToV4(offset), size);
	math::v4 pos = origin + MulPerElem(ToV4((offset - m_value_rate) * math::v2(-1.0f, 1.0f)),
					   size);
	math::v4 slider_size = MulPerElem(size, ToV4(m_visible_rate));

	slider->SetDragLimitMin(origin - range);
	slider->SetDragLimitMax(m_origin_offs + range);
	if (anim)
		slider->SetPos(pos, new Timer(m_slider_anim_time, m_slider_anim_func, 0, -1, NULL),
			       ANIM_MOVE, 2);
	else
		slider->SetPos(pos);
	slider->SetSize(slider_size);
}

void ScrollBar2d::OnTouch(Event *e)
{
	if (m_touch_id >= 0)
		return;
	switch (e->GetType()) {
	case 0x21:
	case 0x41:
		break;
	default:
		return;
	}

	Widget *slider = m_parts[1];

	if (slider != NULL)
		ScePafWidget_D3D0A7C3(slider, (e->m_detail >> 20) & 0xF, (uint8_t)e->m_detail, true,
				      true);
	e->Consume();
}

void ScrollBar2d::OnDragSliderEvent(int32_t type, Handler *self, Event *e, void *data)
{
	ScrollBar2d *bar = (ScrollBar2d *)data;

	switch (type) {
	case 0x2000D:
		bar->OnDragBeginSlider(e);
		break;
	case 0x2000E:
		bar->OnDragMoveSlider(e);
		break;
	case 0x2000F:
		bar->OnDragEndSlider(e, (e->m_detail & 0x8000) != 0);
		break;
	}
}

void ScrollBar2d::OnDragBeginSlider(Event *e)
{
	m_touch_id = (uint8_t)e->m_detail;

	Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

	DoEvent(CB_SCROLLBAR2D_DRAG_BEGIN, &ev);
	OnDragSlider();
}

void ScrollBar2d::OnDragMoveSlider(Event *e)
{
	OnDragSlider();
}

void ScrollBar2d::OnDragEndSlider(Event *e, bool cancel)
{
	m_touch_id = -1;

	Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

	ev.m_data[0] = cancel;
	DoEvent(CB_SCROLLBAR2D_DRAG_END, &ev);
}

void ScrollBar2d::OnDragSlider()
{
	using namespace sce::Vectormath::Simd::Aos;

	Widget *slider = m_parts[1];

	if (slider == NULL)
		return;

	math::v4 pos = slider->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)) - m_origin_offs;
	Vector2 size = Vector2(m_size.extract_x(), m_size.extract_y());
	math::v2 old = m_value;
	Vector2 rate(0.0f);

	if (!(size.getX() < 1.0f))
		rate.setX(sce::Vectormath::Simd::floatInVec(pos.extract_x()) / size.getX());
	if (!(size.getY() < 1.0f))
		rate.setY(sce::Vectormath::Simd::floatInVec(pos.extract_y()) / size.getY());

	math::v2 value = (m_offset_rate - math::v2(rate) * math::v2(-1.0f, 1.0f)) * m_max_value;

	SetValue(value, false);
	if (!(m_value == old)) {
		Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

		DoEvent(CB_SCROLLBAR2D_CHANGED, &ev);
	}
}

const char ScrollBar2dParam::m_param_name[] = "scrollbar2d_base";

const char *ScrollBar2dParam::GetParamName() const
{
	return m_param_name;
}

const char *ScrollBar2dParam::ParamName()
{
	return m_param_name;
}

const char ScrollBar2dStyleParam::m_param_name[] = "style_scrollbar2d";

const char *ScrollBar2dStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *ScrollBar2dStyleParam::ParamName()
{
	return m_param_name;
}

int32_t ScrollBar2dStyleParam::Create(int32_t mode)
{
	int32_t ret = WidgetStyleParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL) {
		m_serial->AddParam("anim_time", &m_anim_time, ResourceParam::SEMANTIC_FLOAT);
		if (m_serial != NULL)
			m_serial->AddParam("anim_func", &m_anim_func, ResourceParam::SEMANTIC_INT);
	}
	return 0;
}

int32_t ScrollBar2dStyleParam::SetEach(Widget *widget)
{
	int32_t ret = WidgetStyleParam::SetEach(widget);

	if (ret < 0)
		return ret;
	((ScrollBar2d *)widget)->SetSliderAnim(m_anim_time, m_anim_func);
	return 0;
}

int32_t ScrollBar2dStyleParam::GetEach(Widget *widget)
{
	int32_t ret = WidgetStyleParam::GetEach(widget);

	if (ret < 0)
		return ret;
	((ScrollBar2d *)widget)->GetSliderAnim(&m_anim_time, &m_anim_func);
	return 0;
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewScrollBar2d(Widget *parent)
{
	return new ScrollBar2d(parent, NULL);
}

static WidgetParam *NewScrollBar2dParam()
{
	return new ScrollBar2dParam;
}

static VersatileParam *NewScrollBar2dLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewScrollBar2dFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewScrollBar2dStyleParam()
{
	return new ScrollBar2dStyleParam;
}

static VersatileParam *NewScrollBar2dHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewScrollBar2dSoundParam()
{
	return new WidgetSoundParam;
}

FactoryFuncTable g_factoryScrollBar2d = {
	ScrollBar2d::TypeName(),
	NewScrollBar2d,
	NewScrollBar2dParam,
	NewScrollBar2dLayoutParam,
	NewScrollBar2dFocusParam,
	NewScrollBar2dStyleParam,
	NewScrollBar2dHandlerParam,
	NewScrollBar2dSoundParam,
};
