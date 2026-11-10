/*
 * paf::ui::SpinBase and its drum (DEBUGGER 0x8112a3f0..0x8112c77c). No record or string names
 * the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <std/algorithm>
#include <std/math.h>
#include <std/string>
#include <std/vector>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <timer/timer.h>
#include <versatileparam/resourceparam.h>
#include <widget/core/environment.h>
#include <widget/core/event.h>
#include <widget/w_spin_base.h>
#include "../paf_types.h"

namespace paf {
namespace ui {

/* the pixels the drum turns to a value (name ours) */
#define SPIN_ITEM_HEIGHT 60.0f
/* a quarter turn of the drum (name ours) */
#define SPIN_QUARTER_TURN 1.5707964f

static inline float Sign(float v)
{
	if (v == 0.0f)
		return 0.0f;
	return v < 0.0f ? -1.0f : 1.0f;
}

const char SpinBase::m_widget_type[] = "spin_base";

bool SpinBase::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *SpinBase::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *SpinBase::TypeName()
{
	return m_widget_type;
}

/* its plane (clear) and its text (white, sized to the text, 24.9 point) */
SpinBase::SpinBase(Widget *parent, CreateParam *param)
	: Widget(parent, param), m_min(0), m_max(0), m_value(0), m_active_on_click(1),
	  m_initialized(false), m_drum_open(0), m_value_changed(false), m_drum(NULL),
	  m_state(STATE_IDLE)
{
	m_event_enable |= 0x10C0;
	SetGestureMask(m_touch_gesture_mask | 1, false);
	SetDragMode(0x1000);
	m_hit_check = 1;
	m_hit_detect_mode = 0;
	m_hit_detect_b_mode = 0;
	m_pre_dispatch = 1;

	graph::DrawStyle base_style;

	base_style.blend = 1;
	base_style.depth = 1;
	base_style.lighting = 1;
	base_style.face_cull = 0;
	base_style.scissor = 0;
	base_style.deepen_color = 0;
	base_style.reduce_transparency = 0;
	base_style.surface_draw = 0;
	base_style.reserved1 = 0;

	graph::PlaneObj *base = new graph::PlaneObj(base_style);

	base->SetColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	AddDrawObj(DrawObjEntry(base, OBJ_BASE), false);

	graph::DrawStyle text_style;

	text_style.blend = 1;
	text_style.depth = 1;
	text_style.lighting = 1;
	text_style.face_cull = 0;
	text_style.scissor = 0;
	text_style.deepen_color = 0;
	text_style.reduce_transparency = 0;
	text_style.surface_draw = 0;
	text_style.reserved1 = 0;

	graph::TextObj *text = new graph::TextObj(text_style);

	text->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	text->SetLayoutAttribute(graph::TextLayoutAttribute_AdjustX, true);
	text->SetLayoutAttribute(graph::TextLayoutAttribute_AdjustY, true);
	text->SetStyleAttribute(graph::TextStyleAttribute_Point, 0, 0, 24.9f);

	common::SharedPtr<graph::DrawObj> ptr((graph::DrawObj *)text);
	DrawObjAttr attr(OBJ_TEXT);

	attr.m_flag1 = false;
	AddDrawObj(DrawObjEntry(ptr, attr), false);
}

__attribute__((noinline)) void SpinDrum::UpdatePos(float offset)
{
	float max = (float)m_max;
	float first = (float)m_first;
	float pos = m_value != -1 ? (float)m_value : first;

	m_pos = pos + offset / SPIN_ITEM_HEIGHT;
	pos = m_pos;
	if (m_loop) {
		float end = max + 1.0f;
		float range = end - first;

		if (first > pos)
			pos += range * (sce_paf_floorf((first - pos) / range) + 1.0f);
		if (pos >= end)
			pos -= range * (sce_paf_floorf((pos - end) / range) + 1.0f);
	} else {
		pos = paf::max(first, paf::min(m_pos, max));
	}
	m_pos = pos;
	if (m_blank.length() != 0 && (int32_t)pos == m_first)
		m_pos = -1.0f;
}

/* each digit's value about the offset, its place on the cylinder, its string and its alpha:
   clear where it is turned out of sight */
void SpinDrum::UpdateDigits(float offset)
{
	UpdatePos(offset);

	float height = 540.0f;
	const math::v4 *size = GetSize(0);

	if (size->extract_y() < height)
		height = size->extract_y();

	float half = height * 0.5f;
	int32_t turned = (int32_t)(offset / SPIN_ITEM_HEIGHT);
	float frac = offset - (float)turned * SPIN_ITEM_HEIGHT;
	int32_t i = -4;
	int32_t last = 5;

	if (offset >= 0.0f) {
		i = -5;
		last = 4;
	}
	for (; i <= last; i++) {
		int32_t base = m_value == -1 ? m_first : m_value;
		int32_t value = base + turned - i;
		int32_t digit = value % 10;

		if (digit < 0)
			digit += 10;

		graph::TextObj *text = (graph::TextObj *)GetDrawObj(digit);

		if (!m_loop && (value < m_first || value > m_max)) {
			text->SetColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
			continue;
		}
		if (m_loop) {
			int32_t span = m_max - m_first + 1;

			if (value < m_first)
				value += span * ((m_first - value - 1) / span) + span;
			else if (value > m_max)
				value -= span * ((value - m_max - 1) / span) + span;
		}

		float radius = m_size.extract_y() * 0.5f;
		float y = frac + (float)i * SPIN_ITEM_HEIGHT;

		if (y < -(radius * SPIN_QUARTER_TURN) || radius * SPIN_QUARTER_TURN < y ||
		    radius < 0.01f) {
			text->SetColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
			continue;
		}

		float angle = y / (m_size.extract_y() * 0.5f);
		float place = m_size.extract_y() * 0.5f * sce_paf_sinf(angle);

		m_digits[digit].m_y = place;
		m_digits[digit].m_scale_y = sce_paf_cosf(angle);
		if (m_blank.length() != 0 && value == m_first) {
			text->SetString(m_blank);
		} else {
			wstring str = ((SpinBase *)m_parent)->GetValueString(value);

			text->SetString(str);
		}
		if (!(place <= half && -half <= place))
			text->SetColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		else
			text->SetColor(math::v4(1.0f, 1.0f, 1.0f,
						1.0f - __builtin_fabsf(angle / SPIN_QUARTER_TURN)));
	}
}

/* its ten digits, each sized to its text, 32 point */
SpinDrum::SpinDrum(Widget *parent, CreateParam *param)
	: Widget(parent, param), m_min(0), m_first(0), m_max(0), m_value(0), m_touch_id(-1),
	  m_offset(0.0f), m_velocity(0.0f), m_snapping(false), m_key_dir(0.0f)
{
	if (!(m_event_enable & 0x400))
		EnableFocusEvent(true);
	m_event_enable |= 0x14D0;
	SetGestureMask(m_touch_gesture_mask | 1, false);
	SetDragMode(0x1000);
	m_hit_check = 1;
	m_hit_find_mode = 1;
	m_hit_detect_mode = 0;
	m_hit_detect_b_mode = 0;

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
	for (uint32_t i = 0; i < 10; i++) {
		graph::TextObj *text = new graph::TextObj(style);

		text->SetColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		text->SetLayoutAttribute(graph::TextLayoutAttribute_AdjustX, true);
		text->SetLayoutAttribute(graph::TextLayoutAttribute_AdjustY, true);
		text->SetStyleAttribute(graph::TextStyleAttribute_Point, 0, 0, 32.0f);

		common::SharedPtr<graph::DrawObj> ptr((graph::DrawObj *)text);
		DrawObjAttr attr(i);

		attr.m_flag1 = false;
		AddDrawObj(DrawObjEntry(ptr, attr), false);
	}
}

void SpinBase::Refresh()
{
	if (!m_initialized)
		return;

	graph::TextObj *text = (graph::TextObj *)GetDrawObj(OBJ_TEXT);

	if (m_drum_open == 0) {
		if (m_string.length() == 0) {
			wstring str = GetValueString(m_value);

			text->SetString(str);
		} else {
			text->SetString(m_string);
		}
		text->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	} else {
		text->SetString(wstring(L""));
		text->SetColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	}
	if (m_drum_open == 0 && m_drum != NULL) {
		m_drum->DestroyWidget();
		m_drum = NULL;
	}
	if (m_drum_open != 1 || m_drum != NULL)
		return;

	m_drum = new SpinDrum(this, NULL);
	m_drum->SetSize(*GetSize(0));
	if (m_active_on_click == 0)
		m_drum->SetString(m_string);

	SpinDrum *drum = m_drum;
	bool commit_on_release = m_active_on_click == 1;
	int32_t min = m_min;
	int32_t max = m_max;
	int32_t value = m_value;
	bool loop = m_loop;

	drum->m_min = min;
	drum->m_max = max;
	drum->m_first = min - (drum->m_blank.length() != 0);
	drum->m_value = value;
	drum->m_loop = loop;
	drum->m_commit_on_release = commit_on_release;
	drum->UpdateDigits(0.0f);
	for (int32_t i = 0; i <= 3; i++) {
		if (m_focuser != NULL) {
			Widget *widget = ScePafWidget_71349B04(m_focuser, i);

			if (widget != NULL)
				m_drum->SetFocusDirectionWidget(i, widget);
		}

		IDParam id;

		if (m_focuser != NULL) {
			m_focuser->GetHintID(i, &id);
			if (id.GetIDHash() != 0)
				m_drum->SetFocusDirection(i, id);
		}
	}
}

int32_t SpinBase::SetValue(int32_t value)
{
	if (value != -1 && (value < m_min || value > m_max))
		return SCE_PAF_ERROR_ALREADY_EXISTS;
	m_value = value;
	m_value_changed = false;
	if (m_drum != NULL) {
		m_drum->m_value = value;
		m_drum->m_offset = 0.0f;
		m_drum->UpdateDigits(0.0f);
	}
	Refresh();
	return 0;
}

/* the whole part the value, the rest the drum's offset; below m_min none */
int32_t SpinBase::SetFloatValue(float value)
{
	if (value < (float)(m_min - 1) || value >= (float)(m_max + 1))
		return SCE_PAF_ERROR_ALREADY_EXISTS;

	int32_t whole = -1;

	if (!(value < (float)m_min))
		whole = (int32_t)sce_paf_floorf(value);
	m_value = whole;
	m_value_changed = false;

	SpinDrum *drum = m_drum;

	if (drum != NULL && drum->m_touch_id < 0) {
		drum->m_value = (int32_t)sce_paf_floorf(value);

		float offset = (value - sce_paf_floorf(value)) * SPIN_ITEM_HEIGHT;

		drum->m_offset = offset;
		drum->UpdateDigits(offset);
	}
	Refresh();
	return 0;
}

float SpinBase::GetFloatValue() const
{
	if (m_drum != NULL)
		return m_drum->m_pos;
	return (float)m_value;
}

/* the value kept in the new range */
int32_t SpinBase::SetRange(int32_t min, int32_t max)
{
	int32_t value = m_value;
	SpinDrum *drum = m_drum;

	if (max < min) {
		int32_t tmp = min;

		min = max;
		max = tmp;
	}
	m_min = min;
	m_max = max;
	if (value != -1) {
		if (min > value)
			value = min;
		m_value = max > value ? value : max;
	}
	m_value_changed = false;
	if (drum != NULL) {
		drum->m_min = min;
		drum->m_max = max;
		drum->m_first = min - (drum->m_blank.length() != 0);
		drum->UpdateDigits(0.0f);
		drum = m_drum;
		drum->m_value = m_value;
		drum->m_offset = 0.0f;
		drum->UpdateDigits(0.0f);
	}
	Refresh();
	return 0;
}

}
}

using namespace paf;
using namespace paf::ui;

/* the drum opened at a click, or always open (the active_on_click parameter) */
void ScePafWidget_176F3B21(SpinBase *spin, int32_t active_on_click)
{
	if (spin->m_active_on_click == active_on_click)
		return;
	spin->m_active_on_click = active_on_click;
	spin->m_drum_open = active_on_click == 0;
	spin->Refresh();
}

namespace paf {
namespace ui {

void SpinBase::OnDrumCommit(int32_t value, int32_t reason)
{
	if (m_active_on_click == 1 && reason == 15)
		return;
	if ((m_active_on_click | reason) == 0)
		return;
	if (m_active_on_click == 1)
		m_drum_open = 0;
	m_value = value;
	m_value_changed = true;
	m_string.clear();
	Refresh();

	Event e(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, 0, 0, 0, 0);

	e.m_data[0] = m_value;
	DoEvent(CB_VALUE_CHANGED, &e);
}

void SpinBase::SetSpinState(int32_t state)
{
	int32_t prev = m_state;

	m_state = state;
	if (prev == STATE_IDLE && state == STATE_TURNING) {
		Event e(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, 0, 0, 0, 0);

		DoEvent(CB_TURN_START, &e);
	} else if (prev == STATE_TURNING && state == STATE_IDLE) {
		Event e(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, 0, 0, 0, 0);

		DoEvent(CB_TURN_END, &e);
	}
}

int32_t SpinBase::SetFocus(SetFocusInfo *info)
{
	if (m_drum == NULL)
		return Widget::SetFocus(info);

	int32_t ret = m_drum->SetFocus(info);

	if (ret < 0)
		return Widget::SetFocus(info);
	return ret;
}

void SpinBase::SetFocusDirection(int32_t dir, IDParam id)
{
	{
		IDParam focus_id = id;

		if (m_focuser != NULL)
			m_focuser->SetHintID(dir, focus_id);
	}
	if (m_drum != NULL)
		m_drum->SetFocusDirection(dir, id);
}

void SpinBase::SetFocusDirectionWidget(int32_t dir, Widget *widget)
{
	if (m_focuser != NULL) {
		if (ScePafWidget_71349B04(m_focuser, dir) != NULL) {
			FocusReferrers *referrers =
				ScePafWidget_71349B04(m_focuser, dir)->m_focus_referrers;

			if (referrers != NULL) {
				vector<Widget *> &list = referrers->m_referrers;
				vector<Widget *>::iterator it = list.begin();

				for (; it != list.end(); ++it)
					if (*it == this)
						break;
				if (it != list.end()) {
					for (it = list.begin(); it != list.end(); ++it)
						if (*it == this)
							break;
					if (it != list.end())
						list.erase(it);
				}
			}
		}

		FocusDirectionWidget *dirs = m_focuser->GetDirectionWidget();

		switch (dir) {
		case 0:
			dirs->m_widget[0] = widget;
			break;
		case 1:
			dirs->m_widget[1] = widget;
			break;
		case 2:
			dirs->m_widget[2] = widget;
			break;
		case 3:
			dirs->m_widget[3] = widget;
			break;
		}
		if (widget != NULL) {
			if (widget->m_focus_referrers == NULL)
				widget->m_focus_referrers = new FocusReferrers;

			vector<Widget *> &list = widget->m_focus_referrers->m_referrers;
			vector<Widget *>::iterator it = list.begin();

			for (; it != list.end(); ++it)
				if (*it == this)
					break;
			if (it == list.end()) {
				for (it = list.begin(); it != list.end(); ++it)
					if (*it == this)
						break;
				if (it == list.end())
					list.push_back(this);
			}
		}
	}
	if (m_drum != NULL)
		m_drum->SetFocusDirectionWidget(dir, widget);
}

graph::DrawObj *SpinBase::GetDrawObj(int32_t obj)
{
	if (obj < OBJ_DRUM || obj > OBJ_DRUM_END)
		return Widget::GetDrawObj(obj);
	if (m_drum == NULL)
		return NULL;
	return m_drum->GetDrawObj(obj - OBJ_DRUM);
}

int32_t SpinBase::SetString(const wstring &str)
{
	m_string = str;
	if (m_active_on_click == 0 && m_drum != NULL)
		m_drum->SetString(m_string);
	Refresh();
	return 0;
}

int32_t SpinBase::GetTexture(intrusive_ptr<graph::Surface> &surf, int32_t obj, uint32_t index)
{
	if (obj != 0)
		return -1;
	GetDrawObj(obj)->GetTexture(surf, index);
	return 0;
}

int32_t SpinBase::SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
			     uint32_t index)
{
	if (obj != 0)
		return -1;
	GetDrawObj(obj)->SetTexture(surf, index);
	return 0;
}

int32_t SpinBase::HandleGestureEvent(Event *e)
{
	if (e != NULL && ((e->m_detail >> 20) & 0xf) == 0 && ((e->m_detail >> 8) & 0x7f) == 0) {
		if (m_active_on_click == 1 && m_drum_open == 0) {
			m_drum_open = 1;
			Refresh();
		}
		e->Consume();
	}
	return Widget::HandleGestureEvent(e);
}

int32_t SpinBase::HandleKeycodeEvent(Event *e)
{
	return -1;
}

int32_t SpinBase::InitSpin(int32_t min, int32_t max, int32_t value, bool loop)
{
	if (value != -1 && (min > value || max < value))
		return SCE_PAF_ERROR_ALREADY_EXISTS;
	m_min = min;
	m_initialized = true;
	m_max = max;
	m_value = value;
	m_loop = loop;
	Refresh();
	return 0;
}

int32_t SpinBaseParam::Create(int32_t mode)
{
	int32_t ret = WidgetParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL)
		m_serial->AddParam("active_on_click", &m_active_on_click,
				   ResourceParam::SEMANTIC_BOOL);
	return 0;
}

int32_t SpinBaseParam::Set()
{
	int32_t ret = WidgetParam::Set();

	if (ret < 0)
		return ret;
	ScePafWidget_176F3B21((SpinBase *)m_widget, m_active_on_click);
	return 0;
}

const char SpinDrum::m_widget_type[] = "spin_base";

/* the fastest a key turns it (name ours) */
static const float s_key_speed_max = 2000.0f;

bool SpinDrum::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *SpinDrum::GetTypeName() const
{
	return (char *)m_widget_type;
}

int32_t SpinDrum::OnDraw(graph::DrawInfo &info)
{
	float alpha = m_color_alpha * m_meta_a_sum * m_show_a_sum;

	for (vector<DrawObjEntry>::iterator it = m_drawobjs.begin(); it != m_drawobjs.end();
	     ++it) {
		Digit &digit = m_digits[it->m_attr.m_id];

		digit.m_matrix = m_world_matrix *
				 math::matrix(math::v4(1.0f, 0.0f, 0.0f, 0.0f),
					      math::v4(0.0f, digit.m_scale_y, 0.0f, 0.0f),
					      math::v4(0.0f, 0.0f, 1.0f, 0.0f),
					      math::v4(0.0f, digit.m_y, 0.0f, 1.0f));
		it->m_obj->OnDraw(digit.m_matrix, m_color, alpha, info);
	}
	return 0;
}

int32_t SpinDrum::SetString(const wstring &str)
{
	m_blank = str;
	m_first = m_min - (m_blank.length() != 0);
	UpdateDigits(0.0f);
	return 0;
}

inline void SpinDrum::SnapTo(float pos, float accel)
{
	m_snapping = true;

	float target = sce_paf_roundf(pos / SPIN_ITEM_HEIGHT) * SPIN_ITEM_HEIGHT;
	float diff = target - m_offset;

	m_target = target;
	m_accel = Sign(diff) * accel;
}

/* the speed of the drag's last moves the flick goes on with, or the turn ended at the nearest
   value where it is too slow */
void SpinDrum::Flick()
{
	if (m_touch_id < 0)
		return;

	float offset = m_offset + m_drag;
	float velocity;
	int32_t num = m_samples.size();

	m_touch_id = -1;
	m_offset = offset;
	if (num == 0) {
		velocity = 0.0f;
	} else {
		float speed = m_samples[num - 1].m_distance * 1000000.0f / m_samples[num - 1].m_time;

		if (num >= 2)
			speed += m_samples[num - 2].m_distance * 1000000.0f / m_samples[num - 2].m_time;
		if (!(__builtin_fabsf(speed) < 89.4f)) {
			float sum = 0.0f;
			float dir = 0.0f;
			float weights = 0.0f;
			uint64_t time = 0;
			int32_t i;

			for (i = num - 1; i >= 0; i--) {
				dir = Sign(m_samples[i].m_distance);
				if (dir != 0.0f)
					break;
			}
			for (i = num - 1; i >= 0; i--) {
				const SpinDrumSample &sample = m_samples[i];
				float weight = 1.0f;

				if (sample.m_distance == 0.0f) {
					sum += 0.0f;
					if (dir == 0.0f)
						weight = 0.0f;
				} else {
					float sign = Sign(sample.m_distance);

					if (dir == 0.0f) {
						weight = 0.0f;
					} else if (dir != sign && sign != 0.0f) {
						weight = 0.0f;
						dir = 0.0f;
					}
					sum += sample.m_distance * 1000000.0f / sample.m_time * weight;
				}
				weights += weight;
				time += sample.m_time;
				if (time >= 134000)
					break;
			}
			if (weights < 1.0f)
				weights = 1.0f;
			velocity = sum / weights;
		} else {
			velocity = 0.0f;
		}
	}
	m_velocity = velocity;
	if (velocity == 0.0f)
		SnapTo(offset, 8.0f);
}

/* the drag followed, kept in the range where it does not loop, its moves kept (the last eight) */
void SpinDrum::Drag()
{
	if (m_touch_id < 0)
		return;

	map<uint32_t, TouchCapture *>::iterator it = m_touch_captures.find(m_touch_id);

	if (it == m_touch_captures.end() || it->second == NULL)
		return;

	float drag = it->second->m_touch->m_line_end.extract_y() - m_touch_y;
	float offset = m_offset;

	if (!m_loop) {
		float low = (float)(m_first - m_value) * SPIN_ITEM_HEIGHT;

		if (drag + offset < low) {
			drag = low - offset;
		} else {
			float high = (float)(m_max - m_value) * SPIN_ITEM_HEIGHT;

			if (drag + offset > high)
				drag = high - offset;
		}
	}
	UpdateDigits(drag + offset);

	uint64_t now = ScePafMisc_256F9060[0];

	if (m_last_time != now) {
		SpinDrumSample sample;

		sample.m_distance = drag - m_drag;
		sample.m_time = now - m_last_time;
		m_samples.push_back(sample);
		if (m_samples.size() >= 8)
			m_samples.erase(m_samples.begin());
		m_last_time = now;
	}
	m_drag = drag;
}

void SpinDrum::TouchDown(uint32_t touch_id)
{
	Environment *env = m_env;

	if (touch_id >= 8)
		return;

	HitInfo *hit = &env->m_touch_hit[0][touch_id];

	if (hit == NULL)
		return;
	((SpinBase *)m_parent)->SetSpinState(SpinBase::STATE_TURNING);
	m_snapping = false;
	m_velocity = 0.0f;
	m_key_dir = 0.0f;

	math::v4 pos = hit->m_pos;

	m_touch_id = touch_id;
	m_drag = 0.0f;
	m_touch_y = pos.extract_y();
	m_samples.clear();
	m_last_time = ScePafMisc_256F9060[0];
}

__attribute__((noinline)) void SpinDrum::Commit(int32_t reason)
{
	int32_t turned = (int32_t)sce_paf_roundf(m_offset / SPIN_ITEM_HEIGHT);
	int32_t base = m_value == -1 ? m_first : m_value;
	int32_t value = turned + base;
	int32_t result;

	if (m_loop) {
		int32_t span = m_max - m_first + 1;

		result = value;
		if (value < m_first)
			result = value + span * ((m_first - value - 1) / span) + span;
		else if (value > m_max)
			result = value - (span * ((value - m_max - 1) / span) + span);
	} else {
		result = paf::max(m_first, paf::min(value, m_max));
	}
	if (m_blank.length() != 0 && result == m_first)
		result = -1;
	((SpinBase *)m_parent)->OnDrumCommit(result, reason);
}

void SpinDrum::Release()
{
	m_velocity = 0.0f;
	m_key_dir = 0.0f;
	if (m_commit_on_release) {
		m_snapping = false;
		Commit(0);
		((SpinBase *)m_parent)->SetSpinState(SpinBase::STATE_IDLE);
	} else {
		SnapTo(m_offset, 8.0f);
	}
}

int32_t SpinDrum::HandleGestureEvent(Event *e)
{
	if (e != NULL && ((e->m_detail >> 20) & 0xf) == 0) {
		switch ((e->m_detail >> 8) & 0x7f) {
		case 0:
			Release();
			e->Consume();
			break;
		case 13:
			TouchDown((uint8_t)e->m_detail);
			e->Consume();
			break;
		case 14:
			Drag();
			e->Consume();
			break;
		case 15:
			Flick();
			e->Consume();
			break;
		}
	}
	return Widget::HandleGestureEvent(e);
}

int32_t SpinDrum::HandleKeycodeEvent(Event *e)
{
	uint32_t state = (uint8_t)e->m_detail;

	if (state != 0 && state <= 2) {
		float dir = 1.0f;

		if (state == 1)
			dir = -1.0f;
		else if (state == 2)
			dir = 1.0f;
		switch (e->GetType()) {
		case 0x11:
			m_key_dir = dir;
			m_velocity = dir * 120.0f;
			((SpinBase *)m_parent)->SetSpinState(SpinBase::STATE_TURNING);
			e->Consume();
			break;
		case 0x12:
		case 0x14:
			if (m_key_dir != 0.0f)
				SnapTo(m_offset + dir * SPIN_ITEM_HEIGHT, 96.0f);
			e->Consume();
			break;
		case 0x13:
			e->Consume();
			break;
		}
	}
	return Widget::HandleKeycodeEvent(e);
}

int32_t SpinDrum::HandleFocusEvent(Event *e)
{
	if (e != NULL && e->GetType() == 0x402) {
		if (m_key_dir != 0.0f)
			SnapTo(m_offset + m_key_dir * SPIN_ITEM_HEIGHT, 96.0f);
		e->Consume();
	}
	return Handler::HandleFocusEvent(e);
}

/* a frame of the snap, or of the flick slowing down (sped up while a key holds it, to 2000);
   ends where it reaches the end of the range that does not loop */
bool SpinDrum::Animate()
{
	bool done = false;
	float offset;

	if (m_touch_id >= 0)
		return false;
	if (m_snapping) {
		float t = Timer::s_tick_interval;

		offset = m_offset;
		if (t > 0.0f) {
			float accel = m_accel;
			float target = m_target;
			float velocity = m_velocity;

			for (;;) {
				float step = 1.0f;

				if (t < 1.0f)
					step = t;
				velocity += accel;
				t -= step;
				offset += Timer::s_tick_unit * step * velocity;
				m_velocity = velocity;
				m_offset = offset;
				if (!(Sign(accel) > 0.0f)) {
					if (offset <= target)
						done = true;
				} else if (offset >= target) {
					done = true;
				}
				if (done)
					break;
				if (!(t > 0.0f))
					goto clamp;
			}
			m_offset = target;
			offset = target;
		}
	} else {
		if (m_velocity == 0.0f)
			return false;

		float t = Timer::s_tick_interval;

		offset = m_offset;
		while (t > 0.0f) {
			float step = 1.0f;
			float velocity = m_velocity;

			if (t < 1.0f)
				step = t;

			float speed = __builtin_fabsf(velocity);

			t -= step;

			float dt = Timer::s_tick_unit * step;
			float change = (speed * 4.0f + 20.0f) * dt;
			float sign = Sign(velocity);
			float next = speed - change;

			if (next < 0.0f)
				next = 0.0f;
			if (m_key_dir != 0.0f) {
				sign = Sign(velocity);
				next = paf::min(speed + change, s_key_speed_max);
			}
			offset += dt * (next * sign);
			m_offset = offset;
			m_velocity = next * sign;
			if (__builtin_fabsf(next * sign) < 40.0f) {
				SnapTo(offset, 20.0f);
				offset = m_offset;
			}
		}
	}
clamp:
	if (!m_loop) {
		float low = (float)(m_first - m_value) * SPIN_ITEM_HEIGHT;

		if (offset < low) {
			done = true;
			m_offset = low;
		} else {
			float high = (float)(m_max - m_value) * SPIN_ITEM_HEIGHT;

			if (offset > high) {
				done = true;
				m_offset = high;
			}
		}
	}
	if (done) {
		m_velocity = 0.0f;
		m_snapping = false;
		m_key_dir = 0.0f;
	}
	UpdateDigits(m_offset);
	return done;
}

void SpinDrum::UpdateAfterChildren()
{
	if (Animate()) {
		Commit(15);
		((SpinBase *)m_parent)->SetSpinState(SpinBase::STATE_IDLE);
	}
}

}
}
