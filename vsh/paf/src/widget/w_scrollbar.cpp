/*
 * paf::ui::ScrollBar (DEBUGGER 0x81122cec..0x811243f4): a slider over its base, as long against
 * the base as the visible amount is against the maximum and placed as the value is, with a jet
 * tail stretched behind it and a glow over it. No record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <versatileparam/resourceparam.h>
#include <widget/factory.h>
#include <widget/w_plane.h>
#include <widget/w_scroll_bar.h>

namespace paf {
namespace ui {

/* the slider: a plane whose draw objects, the jet tail and the glow, take its plane's size (name
   ours) */
class ScrollBarSlider : public Plane {
public:
	static const char m_widget_type[];

	ScrollBarSlider(Widget *parent, CreateParam *param) : Plane(parent, param)
	{
	}

	virtual ~ScrollBarSlider()
	{
	}

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void UpdateSize();
};

/* the two vectors multiplied lane by lane (name ours) */
static inline math::v4 MulPerElem(math::v4 a, math::v4 b)
{
	return math::v4(sce::Vectormath::Simd::Aos::mulPerElem(a.getImpl(), b.getImpl()));
}

/* the draw object shown or hidden, redrawn when it is shown (inlined in Sony's code, name ours) */
static inline void ShowDrawObj(graph::DrawObj *obj, bool show)
{
	if (obj->m_flags.m_visible != show) {
		obj->m_flags.m_visible = show;
		if (show && !graph::DrawObj::s_safety)
			obj->m_flags.m_dirty = graph::DrawObj::s_dirty_count;
	}
}

/* the base on the bar itself, the slider a widget of its own */
ScrollBar::ScrollBar(Widget *parent, CreateParam *param)
	: Widget(parent, param), m_scroll_type(SCROLL_TYPE_VERTICAL),
	  m_slider_size(0.0f, 0.0f, 0.0f, 0.0f), m_slider_min_size(0.0f), m_max_value(100.0f),
	  m_visible_amount(100.0f), m_value(0.0f), m_slider_anim_time(150.0f),
	  m_slider_anim_func(0), m_jet_tail_min_length(0.0f), m_range(100.0f), m_visible_rate(1.0f), m_value_rate(0.0f),
	  m_offset_rate(0.0f)
{
	m_hit_find_mode = 0;
	m_hit_detect_mode = 0;
	m_hit_detect_b_mode = 0;
	m_event_enable |= 0x1000;
	AddDrawObj(DrawObjEntry(new graph::PlaneObj(graph::PlaneObjStyle()), OBJ_BASE), false);

	CreateParam create;

	create.m_parent_protected = true;

	ScrollBarSlider *slider = new ScrollBarSlider(this, &create);

	m_parts.resize(1);
	m_parts[0] = slider;
}

const ScrollBarParam::TextureRef ScrollBarParam::c_textureList[] = {
	{ "texture_slider", ScrollBar::OBJ_SLIDER },
};

ScrollBarParam::ScrollBarParam()
{
	for (uint32_t i = 0; i < sizeof(c_textureList) / sizeof(c_textureList[0]); i++)
		m_texture_list.push_back(IndexedRef(c_textureList[i].name, c_textureList[i].index));
	m_scroll_type = ScrollBar::SCROLL_TYPE_VERTICAL;
	m_slider_size = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
}

ScrollBarStyleParam::ScrollBarStyleParam()
{
	m_min_size = 0.0f;
	m_anim_time = 150.0f;
	m_anim_func = 0;
	m_obj[ScrollBar::OBJ_BASE].m_name = "base_obj";
	m_obj[ScrollBar::OBJ_BASE].m_index = ScrollBar::OBJ_BASE;
	m_drawobj_param_list.push_back(&m_obj[ScrollBar::OBJ_BASE]);
	m_obj[ScrollBar::OBJ_SLIDER].m_name = "slider_obj";
	m_obj[ScrollBar::OBJ_SLIDER].m_index = ScrollBar::OBJ_SLIDER;
	m_drawobj_param_list.push_back(&m_obj[ScrollBar::OBJ_SLIDER]);
	m_obj[ScrollBar::OBJ_JET_TAIL].m_name = "jet_tail_obj";
	m_obj[ScrollBar::OBJ_JET_TAIL].m_index = ScrollBar::OBJ_JET_TAIL;
	m_drawobj_param_list.push_back(&m_obj[ScrollBar::OBJ_JET_TAIL]);
	m_obj[ScrollBar::OBJ_SLIDER_GLOW].m_name = "slider_glow_obj";
	m_obj[ScrollBar::OBJ_SLIDER_GLOW].m_index = ScrollBar::OBJ_SLIDER_GLOW;
	m_drawobj_param_list.push_back(&m_obj[ScrollBar::OBJ_SLIDER_GLOW]);
}

const char ScrollBar::m_widget_type[] = "scrollbar";

bool ScrollBar::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *ScrollBar::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *ScrollBar::TypeName()
{
	return m_widget_type;
}

ScrollBar::~ScrollBar()
{
}

graph::DrawObj *ScrollBar::GetDrawObj(int32_t obj)
{
	int32_t child;
	Widget *widget = GetPartsWidget(obj, &child);

	if (widget == NULL)
		return NULL;
	if (widget == this)
		return Widget::GetDrawObj(obj);
	return widget->GetDrawObj(child);
}

common::SharedPtr<graph::DrawObj> ScrollBar::SwapDrawObj(common::SharedPtr<graph::DrawObj> obj,
							 int32_t id)
{
	int32_t child;
	Widget *widget = GetPartsWidget(id, &child);

	if (widget == NULL)
		return common::SharedPtr<graph::DrawObj>();
	return widget->SwapDrawObj(obj, child);
}

int32_t ScrollBar::SetObjColor(const math::v4 &color, int32_t obj)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw == NULL)
		return -1;
	draw->SetColor(color);
	return 0;
}

int32_t ScrollBar::GetObjColor(math::v4 &color, int32_t obj)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw == NULL)
		return -1;
	color = draw->GetColor();
	return 0;
}

void ScrollBar::CreateJetTail()
{
	if (GetDrawObj(OBJ_JET_TAIL) != NULL)
		return;

	Widget *slider = m_parts[0];

	if (slider == NULL)
		return;

	graph::DrawObj *tail = new graph::PlaneObj(graph::PlaneObjStyle());
	common::SharedPtr<graph::DrawObj> ptr(tail);
	DrawObjAttr attr(OBJ_JET_TAIL);

	attr.m_flag1 = false;
	slider->AddDrawObj(DrawObjEntry(ptr, attr), false);
	tail->SetVisible(false);
}

void ScrollBar::ShowJetTail(bool show)
{
	graph::DrawObj *tail = GetDrawObj(OBJ_JET_TAIL);

	if (tail != NULL)
		ShowDrawObj(tail, show);
}

void ScrollBar::SetJetTailMinLength(float length)
{
	m_jet_tail_min_length = length;
	UpdateJetTail();
}

void ScrollBar::CreateSliderGlow()
{
	if (GetDrawObj(OBJ_SLIDER_GLOW) != NULL)
		return;

	Widget *slider = m_parts[0];

	if (slider == NULL)
		return;

	graph::PlaneObj *glow = new graph::PlaneObj(graph::PlaneObjStyle());

	{
		common::SharedPtr<graph::PlaneObj> ptr(glow);

		slider->AddDrawObj(DrawObjEntry(ptr, DrawObjAttr(OBJ_SLIDER_GLOW)), false);
	}
	glow->SetVisible(false);
}

void ScrollBar::ShowGlow(int32_t obj, bool show)
{
	graph::DrawObj *glow = GetDrawObj(obj);

	if (glow != NULL)
		ShowDrawObj(glow, show);
}

int32_t ScrollBar::SetScrollType(ScrollType type)
{
	if (type < SCROLL_TYPE_VERTICAL || type > SCROLL_TYPE_HORIZONTAL)
		return 0x80AF008E;
	m_scroll_type = type;
	UpdateSlider(false);
	return 0;
}

int32_t ScrollBar::SetSliderSize(const math::v4 &size)
{
	m_slider_size = size;
	UpdateSlider(false);
	return 0;
}

int32_t ScrollBar::SetSliderMinSize(float size)
{
	m_slider_min_size = size;
	UpdateSlider(false);
	return 0;
}

/* called whole by the setters */
__attribute__((noinline)) void ScrollBar::AdjustParameter(bool anim)
{
	float max = m_max_value;
	float visible = m_visible_amount < max ? m_visible_amount : max;

	m_visible_amount = visible;

	float value = m_value < 0.0f ? 0.0f : m_value;
	float range = max - visible;

	value = range > value ? value : range;
	m_value = value;
	m_range = max;

	float visible_rate = visible / max;

	m_visible_rate = visible_rate;
	m_value_rate = value / max;
	m_offset_rate = (1.0f - visible_rate) * 0.5f;
	UpdateSlider(anim);
}

int32_t ScrollBar::SetMaxValue(float value)
{
	if (value <= 0.0f)
		return 0x80AF008E;
	m_max_value = value;
	AdjustParameter(false);
	return 0;
}

int32_t ScrollBar::SetVisibleAmount(float value)
{
	if (value <= 0.0f)
		return 0x80AF008E;
	m_visible_amount = value;
	AdjustParameter(false);
	return 0;
}

int32_t ScrollBar::SetValue(float value, bool anim)
{
	m_value = value;
	AdjustParameter(anim);
	return 0;
}

int32_t ScrollBar::SetSliderAnim(float time, int32_t func)
{
	if (time <= 0.0f)
		return 0x80AF008E;
	m_slider_anim_time = time;
	m_slider_anim_func = func;
	return 0;
}

int32_t ScrollBar::SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
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

uint32_t ScrollBar::CheckState()
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

void ScrollBar::UpdateSize()
{
	Widget::UpdateSize();
	UpdateSlider(false);
}

Widget *ScrollBar::GetPartsWidget(int32_t obj, int32_t *child_obj)
{
	int32_t index = -1;
	int32_t child = 0;

	switch (obj) {
	case OBJ_BASE:
		child = obj;
		break;
	case OBJ_SLIDER:
		index = 0;
		break;
	case OBJ_JET_TAIL:
	case OBJ_SLIDER_GLOW:
		index = 0;
		child = obj;
		break;
	default:
		return NULL;
	}
	if (child_obj != NULL)
		*child_obj = child;
	if (index >= 0)
		return m_parts[index];
	return this;
}

int32_t ScrollBar::SetColor_core(const math::v4 &v)
{
	int32_t ret = Widget::SetColor_core(v);

	for (vector<Widget *>::iterator it = m_parts.begin(); it != m_parts.end(); ++it) {
		Widget *part = *it;

		if (part != NULL && part != this)
			part->SetColor(v);
	}
	return ret;
}

void ScrollBar::AdjustSliderSize()
{
	float rate = 0.0f;
	float min_size = m_slider_min_size;

	if (min_size <= 0.0f)
		return;

	float max = m_max_value;
	float visible = m_visible_amount;
	float length = m_size.extract_x();

	if (m_scroll_type == SCROLL_TYPE_VERTICAL)
		length = m_size.extract_y();
	if (!(length < 1.0f)) {
		rate = min_size / length;
		if (rate > 0.5f)
			rate = 0.5f;
	}
	if (rate * max > visible) {
		float range = (max - visible) / (1.0f - rate);
		float value_rate = m_value / range;

		m_range = range;
		m_visible_rate = rate * range / range;
		m_value_rate = value_rate;
		m_offset_rate = (1.0f - m_visible_rate) * 0.5f;
	}
}

void ScrollBar::UpdateSlider(bool anim)
{
	Widget *slider = m_parts[0];

	if (slider == NULL)
		return;
	slider->KillAnim(ANIM_MOVE);
	AdjustSliderSize();

	math::v4 slider_size = m_slider_size;
	float offset = m_offset_rate - m_value_rate;
	math::v4 origin = m_origin_offs;
	math::v4 size;
	math::v4 pos;

	if (m_scroll_type == SCROLL_TYPE_VERTICAL) {
		size = m_size;
		slider_size.set_y(m_visible_rate * size.extract_y());
		pos = origin + MulPerElem(size, math::v4(0.0f, offset, 0.0f, 0.0f));
	} else {
		size = m_size;
		slider_size.set_x(m_visible_rate * size.extract_x());
		pos = origin + MulPerElem(size, math::v4(-offset, 0.0f, 0.0f, 0.0f));
	}
	if (slider_size.extract_x() <= 0.0f)
		slider_size.set_x(size.extract_x());
	if (slider_size.extract_y() <= 0.0f)
		slider_size.set_y(size.extract_y());
	if (slider_size.extract_z() <= 0.0f)
		slider_size.set_z(size.extract_z());
	if (anim)
		slider->SetPos(pos, new Timer(m_slider_anim_time, m_slider_anim_func, 0, -1, NULL),
			       ANIM_MOVE, 2);
	else
		slider->SetPos(pos);
	slider->SetSize(slider_size);
	UpdateJetTail();
}

void ScrollBar::UpdateJetTail()
{
	graph::DrawObj *tail = GetDrawObj(OBJ_JET_TAIL);

	if (tail == NULL)
		return;

	Widget *slider = m_parts[0];

	if (slider == NULL)
		return;

	math::v4 zero(0.0f, 0.0f, 0.0f, 0.0f);
	float back, front, half;

	if (m_scroll_type != SCROLL_TYPE_VERTICAL) {
		float x = slider->GetPos(0, this, zero).extract_x();
		float slider_half = slider->GetSize(0)->extract_x() * 0.5f;

		front = slider_half - x;
		half = m_size.extract_x() * 0.5f;
		back = half - (slider_half + x);
	} else {
		float y = slider->GetPos(0, this, zero).extract_y();
		float slider_half = slider->GetSize(0)->extract_y() * 0.5f;

		half = m_size.extract_y() * 0.5f;
		front = slider_half + y;
		back = (y - slider_half) + half;
	}

	float limit = m_jet_tail_min_length;

	if (front - half > limit)
		limit = front - half;
	if (back > limit)
		back = limit;

	math::v4 size = *slider->GetSize(0);
	float length = __builtin_fabsf(back);
	math::v4 pos;

	if (m_scroll_type != SCROLL_TYPE_VERTICAL) {
		size.set_x(size.extract_x() + length);
		pos = math::v4(back * 0.5f, 0.0f, 0.0f, 0.0f);
	} else {
		size.set_y(size.extract_y() + length);
		pos = math::v4(0.0f, back * -0.5f, 0.0f, 0.0f);
	}
	tail->SetPos(math::v3(pos.getImpl().getXYZ()));
	tail->SetSize(math::v3(size.getImpl().getXYZ()));
}

const char ScrollBarParam::m_param_name[] = "scrollbar_base";

const char *ScrollBarParam::GetParamName() const
{
	return m_param_name;
}

const char *ScrollBarParam::ParamName()
{
	return m_param_name;
}

int32_t ScrollBarParam::Create(int32_t mode)
{
	int32_t ret = WidgetParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL) {
		m_serial->AddParam("scroll_type", &m_scroll_type, ResourceParam::SEMANTIC_INT);
		if (m_serial != NULL)
			m_serial->AddParam("slider_size", &m_slider_size,
					   ResourceParam::SEMANTIC_VEC3);
	}
	return 0;
}

int32_t ScrollBarParam::Set()
{
	int32_t ret = WidgetParam::Set();

	if (ret < 0)
		return ret;

	ScrollBar *bar = (ScrollBar *)m_widget;

	bar->SetScrollType((ScrollBar::ScrollType)m_scroll_type);
	bar->SetSliderSize(math::v4(math::v3(m_slider_size.getImpl().getXYZ()), 0.0f));
	return 0;
}

int32_t ScrollBarParam::Get()
{
	int32_t ret = WidgetParam::Get();

	if (ret < 0)
		return ret;

	ScrollBar *bar = (ScrollBar *)m_widget;

	m_scroll_type = bar->m_scroll_type;
	m_slider_size = bar->m_slider_size;
	return 0;
}

const char ScrollBarStyleParam::m_param_name[] = "style_scrollbar";

const char *ScrollBarStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *ScrollBarStyleParam::ParamName()
{
	return m_param_name;
}

int32_t ScrollBarStyleParam::Create(int32_t mode)
{
	int32_t ret = WidgetStyleParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL) {
		m_serial->AddParam("min_size", &m_min_size, ResourceParam::SEMANTIC_FLOAT);
		if (m_serial != NULL) {
			m_serial->AddParam("anim_time", &m_anim_time,
					   ResourceParam::SEMANTIC_FLOAT);
			if (m_serial != NULL)
				m_serial->AddParam("anim_func", &m_anim_func,
						   ResourceParam::SEMANTIC_INT);
		}
	}
	return 0;
}

/* the jet tail and the glow made where the style names them */
int32_t ScrollBarStyleParam::SetEach(Widget *widget)
{
	ScrollBar *bar = (ScrollBar *)widget;
	graph::DrawObjParam *tail = GetDrawObjParam(ScrollBar::OBJ_JET_TAIL);

	if (tail != NULL && tail->m_id.GetIDHash() != 0)
		bar->CreateJetTail();

	graph::DrawObjParam *glow = GetDrawObjParam(ScrollBar::OBJ_SLIDER_GLOW);

	if (glow != NULL && glow->m_id.GetIDHash() != 0)
		bar->CreateSliderGlow();

	int32_t ret = WidgetStyleParam::SetEach(widget);

	if (ret < 0)
		return ret;
	bar->SetSliderMinSize(m_min_size);
	bar->SetSliderAnim(m_anim_time, m_anim_func);
	return 0;
}

int32_t ScrollBarStyleParam::GetEach(Widget *widget)
{
	int32_t ret = WidgetStyleParam::GetEach(widget);

	if (ret < 0)
		return ret;

	ScrollBar *bar = (ScrollBar *)widget;

	m_min_size = bar->m_slider_min_size;
	bar->GetSliderAnim(&m_anim_time, &m_anim_func);
	return 0;
}

const char ScrollBarSlider::m_widget_type[] = "slider";

bool ScrollBarSlider::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Plane::IsInherit(type);
}

char *ScrollBarSlider::GetTypeName() const
{
	return (char *)m_widget_type;
}

/* its jet tail and glow sized to its plane */
void ScrollBarSlider::UpdateSize()
{
	Plane::UpdateSize();

	math::v3 size = m_plane->m_size;

	for (vector<DrawObjEntry>::iterator it = m_drawobjs.begin(); it != m_drawobjs.end(); ++it) {
		if (it->m_attr.m_id != 0 && it->m_attr.m_flag1)
			it->m_obj->SetSize(size);
	}
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewScrollBar(Widget *parent)
{
	return new ScrollBar(parent, NULL);
}

static WidgetParam *NewScrollBarParam()
{
	return new ScrollBarParam;
}

static VersatileParam *NewScrollBarLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewScrollBarFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewScrollBarStyleParam()
{
	return new ScrollBarStyleParam;
}

static VersatileParam *NewScrollBarHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewScrollBarSoundParam()
{
	return new WidgetSoundParam;
}

FactoryFuncTable g_factoryScrollBar = {
	ScrollBar::TypeName(),
	NewScrollBar,
	NewScrollBarParam,
	NewScrollBarLayoutParam,
	NewScrollBarFocusParam,
	NewScrollBarStyleParam,
	NewScrollBarHandlerParam,
	NewScrollBarSoundParam,
};
