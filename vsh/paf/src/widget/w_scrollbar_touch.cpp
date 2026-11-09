/*
 * paf::ui::ScrollBarTouch (DEBUGGER 0x811253c8..0x81126504): a ScrollBar whose slider is dragged
 * by touch, and which a touch on the bar beside the slider scrolls a page at a time toward the
 * touch, under a glow on the base. No record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <versatileparam/resourceparam.h>
#include <widget/core/environment.h>
#include <widget/factory.h>
#include <widget/w_scroll_bar.h>

namespace paf {
namespace ui {

/* the two vectors multiplied lane by lane (name ours) */
static inline math::v4 MulPerElem(math::v4 a, math::v4 b)
{
	return math::v4(sce::Vectormath::Simd::Aos::mulPerElem(a.getImpl(), b.getImpl()));
}

/* the slider dragged by touch; the base takes the touches beside it */
ScrollBarTouch::ScrollBarTouch(Widget *parent, CreateParam *param)
	: ScrollBar(parent, param), m_page_scroll_mode(PAGE_SCROLL_MODE_VALUE),
	  m_page_touch_id(-1), m_drag_touch_id(-1), m_page_scroll_time(0.0f)
{
	m_hit_find_mode = 0;
	m_hit_detect_mode = 0;
	m_hit_detect_b_mode = 0;
	m_event_enable |= 0xFBF0;
	m_hit_check = 1;
	/* it plays its four sounds */
	m_sound_mask = 0xF;

	Widget *slider = m_parts[0];

	if (slider != NULL) {
		slider->m_hit_check = 3;
		slider->m_event_enable |= 0x10C0;
		slider->AddEventListener(0x2000D, new EventCBListener(OnDragSliderEvent, this), false);
		slider->AddEventListener(0x2000E, new EventCBListener(OnDragSliderEvent, this), false);
		slider->AddEventListener(0x2000F, new EventCBListener(OnDragSliderEvent, this), false);
	}
}

ScrollBarTouchParam::ScrollBarTouchParam()
{
	m_page_scroll = ScrollBarTouch::PAGE_SCROLL_MODE_VALUE;
}

ScrollBarTouchStyleParam::ScrollBarTouchStyleParam()
{
	m_base_glow_obj.m_name = "base_glow_obj";
	m_base_glow_obj.m_index = ScrollBarTouch::OBJ_BASE_GLOW;
	m_drawobj_param_list.push_back(&m_base_glow_obj);
}

/* the sounds its sound parameter names, by index */
struct SoundDef {
	const char *name;
	uint32_t index;
};

extern const SoundDef s_scrollbar_touch_sounds[] = {
	{ "drag_begin", ScrollBarTouch::SOUND_DRAG_BEGIN },
	{ "drag_end", ScrollBarTouch::SOUND_DRAG_END },
	{ "page_scroll", ScrollBarTouch::SOUND_PAGE_SCROLL },
	{ "page_scroll_repeat", ScrollBarTouch::SOUND_PAGE_SCROLL_REPEAT },
};

ScrollBarTouchSoundParam::ScrollBarTouchSoundParam()
{
	for (uint32_t i = 0; i < 4; i++)
		m_sound_list.push_back(
			Sound(s_scrollbar_touch_sounds[i].name, s_scrollbar_touch_sounds[i].index));
}

const char ScrollBarTouch::m_widget_type[] = "scrollbar_touch";

bool ScrollBarTouch::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return ScrollBar::IsInherit(type);
}

char *ScrollBarTouch::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *ScrollBarTouch::TypeName()
{
	return m_widget_type;
}

ScrollBarTouch::~ScrollBarTouch()
{
}

int32_t ScrollBarTouch::SetPageScrollMode(PageScrollMode mode)
{
	if (mode < PAGE_SCROLL_MODE_NONE || mode > PAGE_SCROLL_MODE_EVENT)
		return 0x80AF008E;
	m_page_scroll_mode = mode;
	return 0;
}

int32_t ScrollBarTouch::HandlePointEvent(Event *e)
{
	OnTouch(e);
	return Handler::HandlePointEvent(e);
}

int32_t ScrollBarTouch::HandleTouchEvent(Event *e)
{
	OnTouch(e);
	return Widget::HandleTouchEvent(e);
}

void ScrollBarTouch::AddSound(uint32_t index, WidgetSoundParam::Sound &sound)
{
	if (index < SOUND_SCROLLBAR_TOUCH_END) {
		/* the sound it had of the index replaced */
		for (list<WidgetSoundParam::Sound>::iterator it = m_sound_list.begin();
		     it != m_sound_list.end(); ++it) {
			if (it->m_index == index) {
				m_sound_list.erase(it);
				break;
			}
		}
		sound.m_index = index;
		m_sound_list.push_back(sound);
	}
}

WidgetSoundParam::Sound *ScrollBarTouch::GetSound(int32_t id)
{
	for (list<WidgetSoundParam::Sound>::iterator it = m_sound_list.begin();
	     it != m_sound_list.end(); ++it) {
		if (it->m_index == (uint32_t)id)
			return &*it;
	}
	return FindSound(id);
}

void ScrollBarTouch::CreateBaseGlow()
{
	if (GetDrawObj(OBJ_BASE_GLOW) != NULL)
		return;

	graph::DrawObj *glow = new graph::PlaneObj(graph::PlaneObjStyle());
	common::SharedPtr<graph::DrawObj> ptr(glow);

	DrawObjAttr attr(OBJ_BASE_GLOW);

	attr.m_flag1 = false;
	AddDrawObj(DrawObjEntry(ptr, attr), false);
	ShowGlow(OBJ_BASE_GLOW, m_page_touch_id >= 0);
}

void ScrollBarTouch::CreateSliderGlow()
{
	if (GetDrawObj(OBJ_SLIDER_GLOW) != NULL)
		return;
	ScrollBar::CreateSliderGlow();
	ShowGlow(OBJ_SLIDER_GLOW, m_drag_touch_id >= 0);
}

void ScrollBarTouch::SetBaseGlowSize(const math::v3 &size)
{
	CreateBaseGlow();

	graph::DrawObj *glow = GetDrawObj(OBJ_BASE_GLOW);

	if (glow != NULL)
		glow->SetSize(size);
}

math::v3 ScrollBarTouch::GetBaseGlowSize()
{
	graph::DrawObj *glow = GetDrawObj(OBJ_BASE_GLOW);

	if (glow != NULL)
		return glow->m_size;
	return math::v3(0.0f, 0.0f, 0.0f);
}

Widget *ScrollBarTouch::GetPartsWidget(int32_t obj, int32_t *child_obj)
{
	if (obj != OBJ_BASE_GLOW)
		return ScrollBar::GetPartsWidget(obj, child_obj);
	if (child_obj != NULL)
		*child_obj = OBJ_BASE_GLOW;
	return this;
}

/* the slider dragged along the bar only, as far as either end; the base glow at the touch */
void ScrollBarTouch::UpdateSlider(bool anim)
{
	ScrollBar::UpdateSlider(anim);

	Widget *slider = m_parts[0];

	if (slider == NULL)
		return;

	uint32_t mode;
	math::v4 range;

	if (m_scroll_type == SCROLL_TYPE_VERTICAL) {
		mode = 0x121;
		range = MulPerElem(m_size, math::v4(0.0f, m_offset_rate, 0.0f, 0.0f));
	} else {
		mode = 0x120;
		range = MulPerElem(m_size, math::v4(m_offset_rate, 0.0f, 0.0f, 0.0f));
	}
	slider->SetDragMode(mode);
	slider->SetDragLimitMin(m_origin_offs - range);
	slider->SetDragLimitMax(m_origin_offs + range);

	graph::DrawObj *glow = GetDrawObj(OBJ_BASE_GLOW);

	if (glow == NULL || !glow->m_flags.m_visible || m_page_touch_id < 0)
		return;

	HitInfo *hit = NULL;

	if ((uint32_t)m_page_touch_id < 8)
		hit = &m_env->m_touch_hit[0][m_page_touch_id];

	math::v2 touch = hit->m_touch_pos;
	math::v3 pos(0.0f, touch.extract_y(), 0.0f);

	if (m_scroll_type == SCROLL_TYPE_HORIZONTAL)
		pos = math::v3(touch.extract_x(), 0.0f, 0.0f);
	glow->SetPos(pos);
}

void ScrollBarTouch::PageScroll()
{
	HitInfo *hit = NULL;

	if ((uint32_t)m_page_touch_id < 8)
		hit = &m_env->m_touch_hit[0][m_page_touch_id];
	if (hit == NULL || hit->m_widget != this)
		return;

	float offset = m_offset_rate - m_value_rate;
	float length;
	float distance;

	if (m_scroll_type == SCROLL_TYPE_VERTICAL) {
		length = (m_size * m_visible_rate).extract_y();
		distance = (m_size * offset).extract_y() - hit->m_touch_pos.extract_y();
	} else {
		length = (m_size * m_visible_rate).extract_x();
		distance = (m_size * offset).extract_x() + hit->m_touch_pos.extract_x();
	}
	if (__builtin_fabsf(distance) <= length * 0.5f) {
		ShowGlow(OBJ_BASE_GLOW, false);
		return;
	}
	ShowGlow(OBJ_BASE_GLOW, true);
	if (m_page_scroll_time >= 300.0f)
		ScePafWidget_2B1A1493(this, SOUND_PAGE_SCROLL_REPEAT);
	else
		ScePafWidget_2B1A1493(this, SOUND_PAGE_SCROLL);

	if (m_page_scroll_mode == PAGE_SCROLL_MODE_VALUE) {
		float value = GetValue();
		float page = m_visible_amount * 0.8f;
		float target = value - page;

		if (distance > 0.0f)
			target = value + page;
		SetValue(target, false);
		if (GetValue() != value) {
			Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

			DoEvent(CB_SCROLLBAR_TOUCH_CHANGED, &ev);
		}
	} else if (m_page_scroll_mode == PAGE_SCROLL_MODE_EVENT) {
		Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

		ev.m_data[0] = distance > 0.0f ? 1 : -1;
		DoEvent(CB_SCROLLBAR_TOUCH_PAGE_SCROLL, &ev);
	}
}

void ScrollBarTouch::OnTouch(Event *e)
{
	int32_t id = (uint8_t)e->m_detail;

	if (m_drag_touch_id >= 0)
		return;
	if (m_page_touch_id >= 0 && m_page_touch_id != id)
		return;
	if (m_page_scroll_mode == PAGE_SCROLL_MODE_NONE) {
		if (m_page_touch_id >= 0) {
			m_page_touch_id = -1;
			ShowGlow(OBJ_BASE_GLOW, false);
		}
		e->Consume();
		return;
	}
	switch (e->GetType()) {
	case 0x21:
	case 0x41:
		m_page_touch_id = id;
		ShowGlow(OBJ_BASE_GLOW, true);
		m_page_scroll_time = Timer::s_tick_unit * 1000.0f;
		PageScroll();
		e->Consume();
		break;
	case 0x23:
	case 0x25:
	case 0x43:
	case 0x44:
		if (m_page_touch_id < 0)
			return;
		/* held, it scrolls again after 300 ms, then every 5 */
		if (m_page_scroll_time >= 300.0f) {
			PageScroll();
			m_page_scroll_time = 295.0f;
		} else {
			m_page_scroll_time += Timer::s_tick_unit * 1000.0f;
		}
		e->Consume();
		break;
	case 0x26:
	case 0x45:
		if (m_page_touch_id < 0)
			return;
		ShowGlow(OBJ_BASE_GLOW, false);
		e->Consume();
		break;
	case 0x22:
	case 0x28:
	case 0x42:
	case 0x47:
		if (m_page_touch_id < 0)
			return;
		m_page_touch_id = -1;
		ShowGlow(OBJ_BASE_GLOW, false);
		e->Consume();
		break;
	}
}

void ScrollBarTouch::OnDragSliderEvent(int32_t type, Handler *self, Event *e, void *data)
{
	ScrollBarTouch *bar = (ScrollBarTouch *)data;

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

void ScrollBarTouch::OnDragBeginSlider(Event *e)
{
	m_drag_touch_id = (uint8_t)e->m_detail;
	ShowGlow(OBJ_SLIDER_GLOW, true);
	m_page_touch_id = -1;
	ShowGlow(OBJ_BASE_GLOW, false);
	ScePafWidget_2B1A1493(this, SOUND_DRAG_BEGIN);

	Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

	DoEvent(CB_SCROLLBAR_TOUCH_DRAG_BEGIN, &ev);
	OnDragSlider();
}

void ScrollBarTouch::OnDragMoveSlider(Event *e)
{
	OnDragSlider();
}

void ScrollBarTouch::OnDragEndSlider(Event *e, bool cancel)
{
	m_drag_touch_id = -1;
	ShowGlow(OBJ_SLIDER_GLOW, false);
	ScePafWidget_2B1A1493(this, SOUND_DRAG_END);

	Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

	ev.m_data[0] = cancel;
	DoEvent(CB_SCROLLBAR_TOUCH_DRAG_END, &ev);
}

void ScrollBarTouch::OnDragSlider()
{
	Widget *slider = m_parts[0];

	if (slider == NULL)
		return;

	math::v4 pos = slider->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)) - m_origin_offs;
	math::v4 size = m_size;
	float distance = pos.extract_y();
	float length = size.extract_y();

	if (m_scroll_type != SCROLL_TYPE_VERTICAL) {
		distance = -pos.extract_x();
		length = size.extract_x();
	}

	float rate = 0.0f;

	if (!(length < 1.0f))
		rate = distance / length;

	float value = (m_offset_rate - rate) * m_range;
	float old = GetValue();

	SetValue(value, false);
	if (GetValue() != old) {
		Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

		DoEvent(CB_SCROLLBAR_TOUCH_CHANGED, &ev);
	}
}

const char ScrollBarTouchParam::m_param_name[] = "scrollbar_touch_base";

const char *ScrollBarTouchParam::GetParamName() const
{
	return m_param_name;
}

const char *ScrollBarTouchParam::ParamName()
{
	return m_param_name;
}

int32_t ScrollBarTouchParam::Create(int32_t mode)
{
	int32_t ret = ScrollBarParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL)
		m_serial->AddParam("page_scroll", &m_page_scroll, ResourceParam::SEMANTIC_INT);
	return 0;
}

int32_t ScrollBarTouchParam::Set()
{
	int32_t ret = ScrollBarParam::Set();

	if (ret < 0)
		return ret;
	((ScrollBarTouch *)m_widget)
		->SetPageScrollMode((ScrollBarTouch::PageScrollMode)m_page_scroll);
	return 0;
}

int32_t ScrollBarTouchParam::Get()
{
	int32_t ret = ScrollBarParam::Get();

	if (ret < 0)
		return ret;
	m_page_scroll = ((ScrollBarTouch *)m_widget)->m_page_scroll_mode;
	return 0;
}

const char ScrollBarTouchStyleParam::m_param_name[] = "style_scrollbar_touch";

const char *ScrollBarTouchStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *ScrollBarTouchStyleParam::ParamName()
{
	return m_param_name;
}

int32_t ScrollBarTouchStyleParam::SetEach(Widget *widget)
{
	graph::DrawObjParam *glow = GetDrawObjParam(ScrollBarTouch::OBJ_BASE_GLOW);

	if (glow != NULL && glow->m_id.GetIDHash() != 0)
		((ScrollBarTouch *)widget)->CreateBaseGlow();
	return ScrollBarStyleParam::SetEach(widget);
}

const char ScrollBarTouchSoundParam::m_param_name[] = "sound_scrollbar_touch";

const char *ScrollBarTouchSoundParam::GetParamName() const
{
	return m_param_name;
}

const char *ScrollBarTouchSoundParam::ParamName()
{
	return m_param_name;
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewScrollBarTouch(Widget *parent)
{
	return new ScrollBarTouch(parent, NULL);
}

static WidgetParam *NewScrollBarTouchParam()
{
	return new ScrollBarTouchParam;
}

static VersatileParam *NewScrollBarTouchLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewScrollBarTouchFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewScrollBarTouchStyleParam()
{
	return new ScrollBarTouchStyleParam;
}

static VersatileParam *NewScrollBarTouchHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewScrollBarTouchSoundParam()
{
	return new ScrollBarTouchSoundParam;
}

FactoryFuncTable g_factoryScrollBarTouch = {
	ScrollBarTouch::TypeName(),
	NewScrollBarTouch,
	NewScrollBarTouchParam,
	NewScrollBarTouchLayoutParam,
	NewScrollBarTouchFocusParam,
	NewScrollBarTouchStyleParam,
	NewScrollBarTouchHandlerParam,
	NewScrollBarTouchSoundParam,
};
