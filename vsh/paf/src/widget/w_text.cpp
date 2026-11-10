/*
 * paf::ui::Text (TOOL 0x8112e80c..0x81133ddc): a widget drawn as a text object, with its links
 * (set or found in its string by URL scheme), its auto scroll and the friends that scroll with
 * it, its touch selection and caret; its resource, style and sound parameters and its factories.
 * No record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <inputdevice/inputlistener.h>
#include <versatileparam/resourceparam.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <math/rect.h>
#include <std/algorithm>
#include <widget/core/environment.h>
#include <widget/core/event.h>
#include <widget/core/focus.h>
#include <widget/core/widget.h>
#include <widget/factory.h>
#include <widget/text_auto_scroll.h>
#include <widget/w_scroll_view_base.h>
#include <widget/w_scene.h>
#include <widget/w_text.h>
#include <widget/w_text_box.h>
#include "../paf_types.h"

using sce::Vectormath::Simd::Aos::Vector4;
using sce::Vectormath::Simd::Aos::mulPerElem;

extern "C" {
/* the edit menu listener wutil::Init made (state.c) */
paf::ui::TextBoxEditMenuListener *ScePafWidget_1EE9D37E(void);
}

namespace paf {

/* the focus shapes' clear and push_back, which Sony keeps out of line (widget.h) */
template<>
__attribute__((noinline)) void vector<ui::FocusShape>::clear()
{
	if (ptr != NULL) {
		for (size_type i = 0; i < count; ++i)
			ptr[i].~FocusShape();
		alloc.deallocate(ptr, limit);
		ptr = NULL;
		count = 0;
		limit = 0;
	}
}

template<>
__attribute__((noinline)) int vector<ui::FocusShape>::push_back(const ui::FocusShape &val)
{
	int result;

	if (count + 1 <= limit || (result = reserve(limit + 10)) != 0) {
		new (&ptr[count]) ui::FocusShape(val);
		result = ++count;
	}
	return result;
}

namespace ui {

/* the text whose auto scroll a touch started last (name ours) */
static Text *s_touch_scroll_text;

/* the colours of its found links, of a pressed one, and two of a focused one, the second
   one's alpha 0.3; the last two unused (names ours) */
static inline __attribute__((always_inline)) math::v4 LinkColor(float r, float g, float b, float a)
{
	return math::v4(r, g, b, a);
}

static const math::v4 s_link_color = LinkColor(0.0f, 0.263f, 0.867f, 1.0f);
static const math::v4 s_link_pressed_color = LinkColor(0.886f, 0.043f, 0.478f, 1.0f);
static const math::v4 s_link_focus_color = LinkColor(0.043f, 0.867f, 0.867f, 1.0f);
static const math::v4 s_link_focus_back_color = LinkColor(0.043f, 0.867f, 0.867f, 0.3f);

static int32_t TextFocusCB(Focuser *focuser, Widget *widget, void *data, void *move);
static void LinkStyleCB(int32_t type, Handler *self, Event *e, void *data);

Text::Text(Widget *parent, CreateParam *param)
	: Widget(parent, param), m_plane(NULL), m_text(new graph::TextObj(graph::TextObjStyle())),
	  m_scroll_view(NULL), m_touch_modes(0), m_touch_states(2), m_touch_pos(0.0f, 0.0f),
	  m_auto_link_detect(0), m_url_scheme(3), m_link_styles(), m_focus_link(NULL), m_auto_scroll_enable(true),
	  m_link_event(true), m_link_mode(1), m_auto_scroll(NULL),
	  m_edit_menu_listener(ScePafWidget_1EE9D37E()), m_scene_listener(NULL)
{
	ReserveDrawObj(1);
	if (m_text != NULL)
		AddDrawObj(DrawObjEntry(m_text, OBJ_TEXT), false);

	m_event_enable |= 0xFBF0;
	m_hit_find_mode = 0;
	m_hit_detect_mode = 2;
	m_hit_detect_b_mode = 2;
	m_hit_check = 0;
	m_sound_mask |= 1;
	UpdateTouchMode();

	Focuser *focuser = new Focuser(TextFocusCB);

	delete m_focuser;
	m_focuser = focuser;
	if (focuser != NULL)
		focuser->BindWidget(this);
}

TextParam::TextParam()
	: m_selection_mode(false), m_auto_link_detect(0), m_url_scheme(3), m_auto_scroll(0),
	  m_auto_scroll_speed(60.0f), m_long_press_text_auto_scroll_mode(false),
	  m_disable_auto_scroll(false)
{
	m_string_list.push_back(IndexedRef("label", Text::OBJ_TEXT));
	m_alias_string_list.push_back(IndexedRef("label_alias", Text::OBJ_TEXT));
}

TextStyleParam::TextStyleParam() : m_adjust(NULL)
{
	m_plane_param.m_name = "planeobj";
	m_plane_param.m_index = Text::OBJ_PLANE;
	m_drawobj_param_list.push_back(&m_plane_param);
	m_text_param.m_name = "textobj";
	m_text_param.m_index = Text::OBJ_TEXT;
	m_drawobj_param_list.push_back(&m_text_param);
	m_adjust = new AdjustParam;
	m_adjust->m_ref_count++;
}

TextSoundParam::TextSoundParam()
{
	m_sound_list.push_back(Sound("link_tapped", 0));
}

TextBoxEditMenuListener::TextBoxEditMenuListener()
{
}

TextBoxEditMenuListener::~TextBoxEditMenuListener()
{
}

const char Text::m_widget_type[] = "text";

const float Text::c_floatInfinity = 10000.0f;
const float Text::c_caretScrollPosMsec = 100.0f;
const float Text::c_caretBlinkIdleMsec = 500.0f;
const float Text::c_caretBlinkFadeMsec = 100.0f;

bool Text::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *Text::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *Text::TypeName()
{
	return m_widget_type;
}

int32_t Text::DeleteLinkStyles()
{
	if (m_link_styles[LINK_STYLE_NORMAL] != NULL) {
		delete m_link_styles[LINK_STYLE_NORMAL];
		m_link_styles[LINK_STYLE_NORMAL] = NULL;
	}
	if (m_link_styles[LINK_STYLE_AUTO] != NULL) {
		delete m_link_styles[LINK_STYLE_AUTO];
		m_link_styles[LINK_STYLE_AUTO] = NULL;
	}
	if (m_link_styles[LINK_STYLE_PRESSED] != NULL) {
		delete m_link_styles[LINK_STYLE_PRESSED];
		m_link_styles[LINK_STYLE_PRESSED] = NULL;
	}
	if (m_link_styles[LINK_STYLE_FOCUSED] != NULL) {
		delete m_link_styles[LINK_STYLE_FOCUSED];
		m_link_styles[LINK_STYLE_FOCUSED] = NULL;
	}
	if (m_scene_listener != NULL) {
		m_scene->DeleteEventListener(0x80000A, m_scene_listener);
		m_scene_listener = NULL;
	}
	return 0;
}

Text::~Text()
{
	DeleteLinkStyles();
	if (m_auto_scroll != NULL) {
		delete m_auto_scroll;
		m_auto_scroll = NULL;
	}
	if (s_touch_scroll_text == this)
		s_touch_scroll_text = NULL;
	m_plane = (graph::PlaneObj *)-1;
	m_text = (graph::TextObj *)-1;
	m_scroll_view = (ScrollViewBase *)-1;
}

void Text::DestroyWidget()
{
	if (m_edit_menu_listener != NULL)
		m_edit_menu_listener->EndEdit((TextBox *)this);
	Widget::DestroyWidget();
}

inline bool Text::IsAutoScrollRunning()
{
	return m_auto_scroll != NULL && m_auto_scroll->IsRunning();
}

int32_t Text::RunAutoScroll(ScrollType type)
{
	if (m_auto_scroll == NULL && m_auto_scroll_enable)
		m_auto_scroll = new TextAutoScroll(this, m_text);
	if (m_auto_scroll == NULL)
		return SCE_PAF_ERROR_NO_OBJECT;

	if (type == SCROLL_TYPE_HORIZONTAL_ONCE || type == SCROLL_TYPE_VERTICAL_ONCE) {
		float speed = 0.0f;

		TextAutoScroll::GetDefaultSpeed(speed, SCROLL_TYPE_HORIZONTAL_ONCE);
		if (m_auto_scroll != NULL)
			m_auto_scroll->SetSpeed(speed, 2);
	}

	ScrollType start = SCROLL_TYPE_HORIZONTAL;

	switch (type) {
	case SCROLL_TYPE_VERTICAL:
		start = SCROLL_TYPE_VERTICAL;
		break;
	case SCROLL_TYPE_HORIZONTAL_ONCE:
		start = SCROLL_TYPE_HORIZONTAL_ONCE;
		break;
	case SCROLL_TYPE_VERTICAL_ONCE:
		start = SCROLL_TYPE_VERTICAL_ONCE;
		break;
	default:
		break;
	}
	return m_auto_scroll->Start(start);
}

}
}

using namespace paf;
using namespace paf::ui;

int32_t ScePafWidget_ADBA776F(Text *text, vector<IDParam> &friends)
{
	if (text->m_auto_scroll == NULL)
		return SCE_PAF_ERROR_NO_OBJECT;
	friends = text->m_auto_scroll->m_friends;
	return 0;
}

namespace paf {
namespace ui {

int32_t Text::FriendAutoScroll(int32_t op, void *arg)
{
	Widget *parent = m_parent;
	bool running = false;
	int32_t ret = SCE_PAF_ERROR_INVALID_ARGUMENT;

	if (parent == NULL)
		goto out;

	{
		vector<IDParam> friends;

		ScePafWidget_ADBA776F(this, friends);
		for (vector<IDParam>::iterator it = friends.begin(); it != friends.end(); ++it) {
			Text *text = NULL;

			if (parent->FindChild(*it, 0) != NULL &&
			    parent->FindChild(*it, 0)->IsInherit(Text::m_widget_type))
				text = (Text *)parent->FindChild(*it, 0);

			switch (op) {
			case FRIEND_START:
				if (text != NULL && text != this && !text->IsAutoScrollRunning())
					ret = text->RunAutoScroll(*(ScrollType *)arg);
				break;
			case FRIEND_STOP:
				if (text != NULL && text != this && text->IsAutoScrollRunning()) {
					int32_t stop = SCE_PAF_ERROR_NO_OBJECT;

					if (text->m_auto_scroll != NULL)
						stop = text->m_auto_scroll->Stop();
					ret = stop;
				}
				break;
			case FRIEND_IS_RUNNING:
				if (text != NULL && text != this) {
					bool on = running;

					if (text->m_auto_scroll != NULL)
						on = running | text->m_auto_scroll->IsRunning();
					running = on ? true : false;
				}
				break;
			case FRIEND_SET_ORIGIN:
				if (text != NULL && text != this) {
					ret = SCE_PAF_ERROR_NO_OBJECT;
					if (text->m_auto_scroll != NULL) {
						ret = 0;
						text->m_auto_scroll->m_origin = *(math::v2 *)arg;
					}
				}
				break;
			case FRIEND_GET_SPEED_BY_POS: {
				FriendSpeedArg speed = *(FriendSpeedArg *)arg;

				if (text != NULL && text != this && text->IsAutoScrollRunning()) {
					int32_t get = SCE_PAF_ERROR_NO_OBJECT;

					if (text->m_auto_scroll != NULL)
						get = text->m_auto_scroll->GetSpeedByPos(
							speed.pos, speed.speed, speed.set_origin,
							speed.reverse);
					((FriendSpeedArg *)arg)->speed = speed.speed;
					return get;
				}
				break;
			}
			case FRIEND_SET_SPEED:
				if (text != NULL && text != this && text->IsAutoScrollRunning()) {
					int32_t set = SCE_PAF_ERROR_NO_OBJECT;

					if (text->m_auto_scroll != NULL)
						set = text->m_auto_scroll->SetSpeed(*(float *)arg, 2);
					ret = set;
				}
				break;
			default:
				break;
			}
		}
	}
out:
	if (op == FRIEND_IS_RUNNING)
		return running;
	return ret;
}

}
}

int32_t ScePafWidget_5B2F5119(Text *text, math::v2 pos, float &speed, bool set_origin,
			      bool reverse)
{
	if (text->IsAutoScrollRunning()) {
		int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

		if (text->m_auto_scroll != NULL)
			ret = text->m_auto_scroll->GetSpeedByPos(pos, speed, set_origin, reverse);
		return ret;
	}

	Text::FriendSpeedArg arg;

	arg.pos = pos;
	arg.set_origin = set_origin;
	arg.reverse = reverse;

	int32_t ret = text->FriendAutoScroll(Text::FRIEND_GET_SPEED_BY_POS, &arg);

	speed = arg.speed;
	return ret;
}

namespace paf {
namespace ui {

int32_t Text::GetTouchTextPos(uint32_t port, uint32_t id, math::v2 &pos)
{
	Environment *env = m_env;
	int32_t ret = SCE_PAF_ERROR_BUSY;

	if (id < 8) {
		HitInfo *hit = &env->m_touch_hit[0][id];

		if (hit != NULL) {
			if (hit->m_hit_valid == 0 && id < 8 && port < 2) {
				TouchInfo *touch = &env->m_touch_state[port][id];

				touch->m_port = port;
				touch->m_id = id;
				if (touch != NULL)
					CalcHitBoxToLine(math::v4(10000.0f, 10000.0f, 0.0f, 0.0f),
							 touch->m_line_start, touch->m_line_end, hit);
			}

			math::v4 box(0.0f, 0.0f, 0.0f, 0.0f);

			if (m_hit_box != NULL)
				box = *m_hit_box;

			math::v3 text_pos = m_text->m_pos;
			math::v3 text_size = m_text->m_size;
			math::v2 visible;

			m_text->GetLayoutAttribute(graph::TextLayoutAttribute_VisiblePos, visible);
			ret = 0;
			pos = (hit->m_touch_pos - math::v2(box.getImpl().getXY()) -
			       math::v2(text_pos.getImpl().getXY())) * math::v2(1.0f, -1.0f) +
			      math::v2(text_size.getImpl().getXY()) * 0.5f + visible;
		}
	}
	return ret;
}

int32_t Text::HandleTouchMove(Event *e)
{
	int32_t ret = SCE_PAF_ERROR_BUSY;

	if (m_touch_held) {
		math::v2 pos;

		ret = GetTouchTextPos((e->m_detail >> 20) & 0xf, (uint8_t)e->m_detail, pos);
		if (ret == 0) {
			ret = UpdateTouch(pos);
			if (ret == 0 && !m_touch_auto_scroll)
				e->m_type = (e->m_type & 0xf0ffffff) | 0x1000000;
		}
		if (m_touch_auto_scroll) {
			float speed = 0.0f;
			math::v2 touch((float)e->m_data[0], (float)e->m_data[1]);

			if (ScePafWidget_5B2F5119(this, touch, speed, false, true) == 0) {
				if (m_auto_scroll != NULL)
					m_auto_scroll->SetSpeed(speed, 2);
				FriendAutoScroll(FRIEND_SET_SPEED, &speed);
			}
		}
	}
	return ret;
}

int32_t Text::HandleTouchEvent(Event *e)
{
	uint32_t port = (e->m_detail >> 20) & 0xf;
	uint32_t id = (uint8_t)e->m_detail;

	if (m_text != NULL && port == 0 && id == 0) {
		switch (e->m_type & 0xffff) {
		case 0x41:
			HandleTouchPress(e);
			break;
		case 0x43:
			HandleTouchMove(e);
			break;
		}
	}
	return Widget::HandleTouchEvent(e);
}

}
}

int32_t ScePafWidget_8A5AE1CD(Text *text, const math::v2 &pos)
{
	text->FriendAutoScroll(Text::FRIEND_SET_ORIGIN, (void *)&pos);
	if (text->m_auto_scroll == NULL)
		return SCE_PAF_ERROR_NO_OBJECT;
	text->m_auto_scroll->m_origin = pos;
	return 0;
}

int32_t ScePafWidget_222D55C7(Text *text, bool *on)
{
	*on = text->m_touch_auto_scroll;
	return 0;
}

namespace paf {
namespace ui {

__attribute__((noinline)) int32_t Text::StopAutoScroll()
{
	if (!m_touch_auto_scroll || !m_touch_held)
		FriendAutoScroll(FRIEND_STOP, NULL);

	int32_t ret = SCE_PAF_ERROR_BUSY;

	if ((!m_touch_auto_scroll || !m_touch_held) && IsAutoScrollRunning()) {
		ret = SCE_PAF_ERROR_NO_OBJECT;
		if (m_auto_scroll != NULL)
			ret = m_auto_scroll->Stop();
	}
	return ret;
}

int32_t Text::BeginTouch(const math::v2 &pos)
{
	int32_t ret = SCE_PAF_ERROR_BUSY;

	if (m_touch_modes != 0) {
		m_touch_pos = pos;
		if (m_edit_menu_listener != NULL)
			m_edit_menu_listener->BeginEdit((TextBox *)this);
		if (m_selection_mode) {
			Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

			DoEvent(CB_TEXT_SELECTION_BEGIN, &ev);
		}
		if (m_touch_auto_scroll) {
			StopAutoScroll();
			if (!IsAutoScrollRunning()) {
				if (s_touch_scroll_text != NULL)
					s_touch_scroll_text->StopAutoScroll();
				s_touch_scroll_text = this;
				RunAutoScroll(SCROLL_TYPE_HORIZONTAL_ONCE);
			}

			ScrollType type = SCROLL_TYPE_HORIZONTAL_ONCE;

			FriendAutoScroll(FRIEND_START, &type);
		}
		ret = 0;
	}
	return ret;
}

int32_t Text::HandleLongPress(Event *e)
{
	math::v2 pos;
	int32_t ret = GetTouchTextPos((e->m_detail >> 20) & 0xf, (uint8_t)e->m_detail, pos);

	if (ret != 0)
		return ret;

	if (m_auto_scroll != NULL)
		m_auto_scroll->Restore();
	ret = BeginTouch(pos);
	if (m_touch_auto_scroll)
		ScePafWidget_8A5AE1CD(this, math::v2((float)e->m_data[0], (float)e->m_data[1]));
	if (ret == 0) {
		m_touch_held = true;
		ret = UpdateTouch(pos);
	}
	return ret;
}

int32_t Text::HandleGestureEvent(Event *e)
{
	if (m_text != NULL) {
		switch ((e->m_detail >> 8) & 0x7f) {
		case 4:
			HandleLongPress(e);
			break;
		case 0:
			ScePafWidget_695200BA(e);
			break;
		}
	}
	return Widget::HandleGestureEvent(e);
}

uint32_t Text::CheckState()
{
	uint32_t state = m_state;

	if (m_text != NULL && !m_text->m_flags.m_prepare)
		return 0;
	return state;
}

uint32_t Text::ChangeState(uint32_t state, int32_t ev_type)
{
	if (m_text != NULL && state != m_state && state == 0)
		AdjustSize();
	return Widget::ChangeState(state, ev_type);
}

int32_t Text::SetLayoutAttribute(graph::TextLayoutAttribute attr, const math::v2 &val)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL) {
		ret = m_text->SetLayoutAttribute(attr, val);
		if (ret > 0) {
			if (ret & 2)
				ChangeState(3, -1);
			return 0;
		}
	}
	return ret;
}

math::v4 Text::GetLinkRect(const Link *link)
{
	math::v4 rect(0.0f, 0.0f, 0.0f, 0.0f);

	rect.set_z(GetSize(1)->extract_x());
	rect.set_w(GetSize(1)->extract_y());
	if (link != NULL) {
		graph::TextLayoutCharInfo info;
		graph::TextLayoutLineInfo line;

		m_text->GetCharInfo(link->m_index, info);

		uint32_t first = info.line;

		m_text->GetLineInfo(first, line);

		float top = line.frame.extract_y();
		float height = line.frame.extract_w();
		float left = info.x;

		m_text->GetCharInfo(link->m_index + link->m_len, info);

		uint32_t last = info.line;
		float right = info.x;
		float bottom_top = top;

		if (first != last) {
			for (uint32_t i = first; i < last; i++) {
				m_text->GetLineInfo(i, line);
				if (line.frame.extract_x() + line.frame.extract_z() > right)
					right = line.frame.extract_x() + line.frame.extract_z();
			}
			m_text->GetLineInfo(last, line);
			height = line.frame.extract_w();
			left = line.frame.extract_x();
			bottom_top = line.frame.extract_y();
		}

		float bottom = bottom_top + height;

		rect.set_x(math::v4(-(GetSize(1)->getImpl() * 0.5f)).extract_x() + (left + right) * 0.5f);
		rect.set_y((*GetSize(1) * 0.5f).extract_y() - (bottom + top) * 0.5f);
		rect.set_z(right - left);
		rect.set_w(bottom - top);
	}
	return rect;
}

void Text::UpdateSize()
{
	if (m_focus_link != NULL && m_cursor_on) {
		FocusShape own;
		FocusShape *shape = m_focus_shape;

		if (shape == NULL)
			shape = &own;
		shape->m_use_parts = true;

		math::v4 rect = GetLinkRect(m_focus_link);

		shape->m_offset = math::v3(rect.extract_x(), rect.extract_y(), 0.0f);
		shape->m_size = math::v2(rect.getImpl().getZ(), rect.getImpl().getW()) +
				math::v2(6.0f, 4.0f);

		uint32_t ax, ay, az;

		GetOrigin(ax, ay, az);

		math::v4 half = (*GetSize(1) - math::v4(shape->m_size.extract_x(),
							shape->m_size.extract_y(), 0.0f, 0.0f)) * 0.5f;

		if (ax == 2)
			shape->m_offset.set_x(shape->m_offset.extract_x() + half.extract_x());
		else if (ax == 1)
			shape->m_offset.set_x(shape->m_offset.extract_x() - half.extract_x());
		if (ay == 2)
			shape->m_offset.set_y(shape->m_offset.extract_y() - half.extract_y());
		else if (ay == 1)
			shape->m_offset.set_y(shape->m_offset.extract_y() + half.extract_y());
		ScePafWidget_13B246E0(this, shape);
		m_focus_parts.clear();

		graph::TextLayoutCharInfo info;
		graph::TextLayoutLineInfo line;

		m_text->GetCharInfo(m_focus_link->m_index, info);

		uint32_t line_index = info.line;

		m_text->GetCharInfo(m_focus_link->m_index + m_focus_link->m_len, info);

		int32_t last = info.line - line_index;
		int32_t count = last + 1;
		uint32_t index = m_focus_link->m_index;

		for (int32_t i = 0; i < count; i++) {
			m_text->GetCharInfo(index, info);
			m_text->GetLineInfo(line_index, line);

			float left = info.x;
			float top = line.frame.extract_y();
			float right = line.frame.extract_x() + line.frame.extract_z();
			float bottom = line.frame.extract_y() + line.frame.extract_w();

			if (i == last) {
				m_text->GetCharInfo(m_focus_link->m_index + m_focus_link->m_len, info);
				right = info.x;
			}

			FocusShape part;

			if (count == 1) {
				part.m_shape = 7;
			} else if (i != 0) {
				if (i == last) {
					part.m_shape = 9;
					part.m_rot = math::v3(0.0f, 3.14159274f, 0.0f);
				} else {
					part.m_shape = 8;
				}
			} else {
				part.m_shape = 9;
			}

			math::v4 offset(0.0f, 0.0f, 0.0f, 0.0f);

			offset.set_x(math::v4(-(GetSize(1)->getImpl() * 0.5f)).extract_x() +
				     (left + right) * 0.5f);
			offset.set_y((*GetSize(1) * 0.5f).extract_y() - (bottom + top) * 0.5f);
			part.m_size = math::v2(right - left, bottom - top) + math::v2(6.0f, 4.0f);

			AdjustParam *adjust = m_adjust_param;

			if (adjust == NULL)
				adjust = &ScePafWidget_BE1CEA2E;
			offset.set_x(offset.extract_x() + adjust->m_left_margin);
			offset.set_y(offset.extract_y() - adjust->m_top_margin);
			if (ax == 2)
				offset.set_x(offset.extract_x() + half.extract_x());
			else if (ax == 1)
				offset.set_x(offset.extract_x() - half.extract_x());
			if (ay == 2)
				offset.set_y(offset.extract_y() - half.extract_y());
			else if (ay == 1)
				offset.set_y(offset.extract_y() + half.extract_y());
			part.m_offset = math::v3(offset.getImpl().getXYZ()) - shape->m_offset;
			m_focus_parts.push_back(part);
			m_text->GetLineInfo(++line_index, line);
			index = line.index;
		}
	}

	if (m_plane != NULL) {
		if (m_text == NULL)
			return;
		if (!m_text->m_flags.m_prepare)
			m_plane->SetSize(math::v3(m_size.getImpl().getXYZ()));
	}
	if (m_text != NULL) {
		AdjustParam *adjust = m_adjust_param;

		if (adjust == NULL)
			adjust = &ScePafWidget_BE1CEA2E;

		math::v2 size(m_size.extract_x() - adjust->m_left_margin - adjust->m_right_margin,
			      m_size.extract_y() - adjust->m_bottom_margin - adjust->m_top_margin);
		int32_t ret = m_text->SetLayoutAttribute(graph::TextLayoutAttribute_Size, size);

		if (ret > 0 && (ret & 2))
			ChangeState(3, -1);
	}
}

int32_t Text::SetLayoutAttribute(graph::TextLayoutAttribute attr, bool val)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL) {
		ret = m_text->SetLayoutAttribute(attr, val);
		if (ret > 0) {
			if (ret & 2)
				ChangeState(3, -1);
			return 0;
		}
	}
	return ret;
}

void Text::UpdateLayoutAttribute()
{
	bool adjust_x = m_adjust_x == 2;

	if (m_text != NULL) {
		int32_t ret = m_text->SetLayoutAttribute(graph::TextLayoutAttribute_AdjustX,
							 adjust_x);

		if (ret > 0 && (ret & 2))
			ChangeState(3, -1);
	}
	if (m_text != NULL) {
		int32_t ret = m_text->SetLayoutAttribute(graph::TextLayoutAttribute_AdjustY,
							 m_adjust_y == 2);

		if (ret > 0 && (ret & 2))
			ChangeState(3, -1);
	}
}

int32_t Text::SetStyle(uint32_t index, uint32_t len, const graph::TextStyle &style)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL) {
		ret = m_text->SetStyle(index, len, style);
		if (ret > 0) {
			if (ret & 2)
				ChangeState(3, -1);
			return 0;
		}
	}
	return ret;
}

math::v4 Text::GetLinkWorldRect(const Link *link)
{
	math::v4 rect(0.0f, 0.0f, 0.0f, 0.0f);

	rect = GetLinkRect(link);
	rect.set_x((rect + GetWorldMatrix().w()).extract_x());
	rect.set_y((rect + GetWorldMatrix().w()).extract_y());
	return rect;
}

/* half the size of the view a text's links are in: its scene's, else the screen's */
static inline math::v2 GetLinkViewHalf(Widget *scene)
{
	math::v4 size;

	if (scene != NULL) {
		size = *scene->GetSize(0);
	} else {
		math::rect screen(Screen::s_root_screen->sp);

		size = math::v4(screen.width(), screen.height(), 0.0f, 0.0f);
	}
	return math::v2(size.getImpl().getXY()) * 0.5f;
}

/* the rectangle (a centre and a size) is not all inside the view of half size half */
static inline bool IsOutOfLinkView(const math::v4 &rect, const math::v2 &half)
{
	float half_w = rect.extract_z() * 0.5f;
	float half_h = rect.extract_w() * 0.5f;

	return rect.extract_x() - half_w < -half.extract_x() ||
	       rect.extract_y() - half_h < -half.extract_y() ||
	       rect.extract_x() + half_w > half.extract_x() ||
	       rect.extract_y() + half_h > half.extract_y();
}

Text::Link *Text::SearchLinkFocus(int32_t dir, const math::v4 &rect, int32_t reserved)
{
	if (m_touch_captures.size() != 0)
		return NULL;

	sort(m_links.begin(), m_links.end());

	Link *found = NULL;

	if (m_focus_link == NULL && m_links.size() != 0 && m_link_mode != 1) {
		found = m_links.begin().m_ptr;
		m_focus_link = found;
	}

	math::v2 half = GetLinkViewHalf(m_scene);
	FocusSearchInfo info(255);

	info.m_dir = dir;
	info.m_origin = this;
	if (dir == 1 || dir == 2)
		info.m_search_outside = false;
	info.m_ratio = 2.0f;

	int32_t best = -0x7fffffff;

	for (vector<Link>::iterator it = m_links.begin(); it != m_links.end(); ++it) {
		if (m_link_mode == 1) {
			math::v4 from = rect;
			math::v4 from_pos(from.extract_x(), from.extract_y(), 0.0f, 0.0f);
			math::v4 from_size(from.extract_z(), from.extract_w(), 0.0f, 0.0f);
			math::v4 to = GetLinkWorldRect(it.m_ptr);
			math::v4 to_pos(to.extract_x(), to.extract_y(), 0.0f, 0.0f);
			math::v4 to_size(to.extract_z(), to.extract_w(), 0.0f, 0.0f);
			float from_hw = from.extract_z() * 0.5f;
			float from_hh = from.extract_w() * 0.5f;
			float to_hw = to.extract_z() * 0.5f;
			float to_hh = to.extract_w() * 0.5f;
			float from_l = from.extract_x() - from_hw, from_r = from.extract_x() + from_hw;
			float from_b = from.extract_y() - from_hh, from_t = from.extract_y() + from_hh;
			float to_l = to.extract_x() - to_hw, to_r = to.extract_x() + to_hw;
			float to_b = to.extract_y() - to_hh, to_t = to.extract_y() + to_hh;

			if (to_l < -half.extract_x() || to_b < -half.extract_y() ||
			    to_r > half.extract_x() || to_t > half.extract_y())
				continue;

			/* a rectangle holding the other counts as a point */
			if (from_l <= to_l && from_b <= to_b && from_r >= to_r && from_t >= to_t)
				from_size = math::v4(1.0f, 1.0f, 0.0f, 0.0f);
			if (from_l >= to_l && from_b >= to_b && from_r <= to_r && from_t <= to_t)
				to_size = math::v4(1.0f, 1.0f, 0.0f, 0.0f);

			FocusCandidate candidate;

			candidate.m_from_pos = from_pos;
			candidate.m_from_size = from_size;
			candidate.m_to_pos = to_pos;
			candidate.m_to_size = to_size;
			candidate.m_half_view = half;

			int32_t score = ScePafWidget_5D4941E8(&candidate, &info);

			if (best < score) {
				best = score;
				found = it.m_ptr;
			}
		} else {
			/* the links step in the text's order: down and up, or right and left */
			int32_t next = 2;
			int32_t prev = 1;

			if (m_link_mode == 3) {
				next = 4;
				prev = 3;
			}
			if (m_focus_link != it.m_ptr)
				continue;

			vector<Link>::iterator to = it;

			if (dir == next) {
				++to;
				if (to != m_links.end())
					found = to.m_ptr;
			} else if (dir == prev && to != m_links.begin()) {
				--to;
				found = to.m_ptr;
			}
			if (IsOutOfLinkView(GetLinkWorldRect(to.m_ptr), half) || found == NULL)
				return NULL;
			best = 0x7fffffff;
			break;
		}
	}
	if (found == NULL || best < 0)
		return NULL;

	Link *old = m_focus_link;

	m_focus_link = found;
	UpdateSize();
	if (old != NULL && m_link_styles[LINK_STYLE_AUTO] != NULL)
		SetStyle(old->m_index, old->m_len, *m_link_styles[LINK_STYLE_AUTO]);
	if (m_focus_link != NULL && m_link_styles[LINK_STYLE_FOCUSED] != NULL && m_env != NULL &&
	    ScePafWidget_529FB877(m_env, -1))
		SetStyle(m_focus_link->m_index, m_focus_link->m_len,
			 *m_link_styles[LINK_STYLE_FOCUSED]);
	return found;
}

/* the focus link let go where it is not all in the view (name ours) */
void Text::CheckLinkFocusInView()
{
	math::v4 rect = GetLinkWorldRect(m_focus_link);
	math::v2 half = GetLinkViewHalf(m_scene);

	if (IsOutOfLinkView(rect, half))
		m_focus_link = NULL;
}

/* the text's focuser: the focus moves among its links while one is in the way it goes, else
   on to the nearest widget (name ours) */
__attribute__((noinline)) static int32_t TextFocusCB(Focuser *focuser, Widget *widget, void *data,
						       void *move)
{
	Widget::FocusSearchInfo *info = (Widget::FocusSearchInfo *)move;

	if (info == NULL)
		return 0;
	if (widget == NULL || !widget->IsInherit(Text::m_widget_type))
		return 0;

	Text *text = (Text *)widget;

	if (text->m_links.size() != 0) {
		text->CheckLinkFocusInView();

		Widget *origin = info->m_origin;
		int32_t dir = info->m_dir;

		if (origin == NULL)
			origin = text;

		math::v4 rect(origin->GetWorldMatrix().w().extract_x(),
			      origin->GetWorldMatrix().w().extract_y(),
			      origin->GetSize(0)->extract_x(), origin->GetSize(0)->extract_y());

		if (text->m_focus_link == NULL && text->SearchLinkFocus(dir, rect, 0) == NULL)
			return 0;
	}
	return DefaultFocusCB(focuser, widget, data, move);
}

void Text::UpdatePost()
{
	if (m_scroll_view != NULL && m_text != NULL) {
		/* the text shows what the scroll view shows of it */
		math::v2 view_size(m_scroll_view->GetSize(0)->getImpl().getXY());

		if (m_text != NULL) {
			int32_t ret = m_text->SetLayoutAttribute(graph::TextLayoutAttribute_VisibleSize,
								 view_size);

			if (ret > 0 && (ret & 2))
				ChangeState(3, -1);
		}

		math::v2 focus = m_scroll_view->GetViewFocus();
		math::v4 box = m_scroll_view->m_view_box +
			       math::v4(mulPerElem(Vector4(view_size.getImpl(), view_size.getImpl()),
						   Vector4(0.5f, -0.5f, -0.5f, 0.5f)));

		/* the view's focus kept where the view stays in its box */
		if (focus.extract_x() < box.extract_x())
			focus.set_x(box.extract_x());
		else if (focus.extract_x() > box.extract_z())
			focus.set_x(box.extract_z());
		if (focus.extract_y() > box.extract_y())
			focus.set_y(box.extract_y());
		else if (focus.extract_y() < box.extract_w())
			focus.set_y(box.extract_w());

		float top, bottom, left, right;

		if (m_adjust_param != NULL) {
			top = m_adjust_param->m_top_margin;
			bottom = m_adjust_param->m_bottom_margin;
			left = m_adjust_param->m_left_margin;
			right = m_adjust_param->m_right_margin;
		} else {
			top = ScePafWidget_BE1CEA2E.m_top_margin;
			bottom = ScePafWidget_BE1CEA2E.m_bottom_margin;
			left = ScePafWidget_BE1CEA2E.m_left_margin;
			right = ScePafWidget_BE1CEA2E.m_right_margin;
		}
		math::v4 center = GetPosCenter(0, m_scroll_view->GetInternalWidget(),
					       math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		math::v2 visible = (focus - math::v2(center.getImpl().getXY())) * math::v2(1.0f, -1.0f) +
				   (math::v2(m_size.getImpl().getXY()) - view_size) * 0.5f -
				   math::v2(left, top);

		if (m_text != NULL) {
			int32_t ret = m_text->SetLayoutAttribute(graph::TextLayoutAttribute_VisiblePos,
								 visible);

			if (ret > 0 && (ret & 2))
				ChangeState(3, -1);
		}
		m_text->SetPos(math::v3(visible.extract_x(), -visible.extract_y(), 0.0f) +
			       math::v3(left - right, bottom - top, 0.0f) * 0.5f);
	}

	if (Text::IsTextEventEnabled() && m_cursor_on && m_focus_link == NULL) {
		FocusSearchInfo info(255);

		info.m_dir = 255;
		info.m_origin = this;
		TextFocusCB(NULL, this, NULL, &info);
		if (m_focus_link != NULL) {
			UpdateSize();
			if (m_focus_shape != NULL)
				m_focus_shape->m_use_parts = false;
		}
	}
}

int32_t Text::OnDraw(graph::DrawInfo &info)
{
	float alpha = m_color_alpha * m_meta_a_sum * m_show_a_sum;

	if (m_plane != NULL)
		m_plane->OnDraw(m_world_matrix, m_color, alpha, info);
	if (m_text != NULL)
		m_text->OnDraw(m_world_matrix, m_color, alpha, info);
	return 0;
}

int32_t Text::CreatePlaneObj()
{
	if (m_plane != NULL)
		return SCE_PAF_ERROR_ALREADY_CREATED;

	m_plane = new graph::PlaneObj(graph::PlaneObjStyle());
	if (m_plane == NULL)
		return SCE_PAF_ERROR_NO_OBJECT;

	AddDrawObj(DrawObjEntry(m_plane, OBJ_PLANE), true);
	return 0;
}

int32_t Text::SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj, uint32_t index)
{
	int32_t ret = 0;

	if (surf.get() != NULL)
		ret = CreatePlaneObj();
	if (m_plane != NULL) {
		m_plane->SetTexture(surf, index);
		return 0;
	}
	return ret;
}

}
}

using namespace paf;
using namespace paf::ui;

/* the URL schemes "url_scheme" picks from, a bit each (name ours) */
static const wchar_t *const s_url_schemes[] = {
	L"http://", L"https://", L"psns:", L"psgm:", L"psvp:", L"psim:", L"psnp:", L"pstc:",
	L"pspr:",
};

/* a character of a URL after its scheme; a ')' only inside a '(' */
static inline bool IsUrlChar(uint32_t c, bool paren)
{
	return c == '!' || (c >= '#' && c <= '(') || (paren && c == ')') || (c >= '*' && c <= ';') ||
	       c == '=' || (c >= '?' && c <= 'Z') || c == '_' || (c >= 'a' && c <= 'z') || c == '~';
}

int32_t ScePafWidget_374B1ECF(Text *text, const wstring &str)
{
	vector<wstring> schemes;
	vector<uint16_t> bits;

	for (uint32_t i = 0; i < 9; i++) {
		if (text->m_url_scheme & (1 << i)) {
			schemes.push_back(wstring(s_url_schemes[i]));
			bits.push_back(1 << i);
		}
	}

	uint32_t size = str.length();

	for (uint32_t pos = 0; pos < size;) {
		uint32_t len = 1;

		if (schemes.size() != 0) {
			uint32_t k;

			for (k = 0; k < schemes.size(); k++) {
				if (str.substr(pos, schemes[k].length()) == schemes[k])
					break;
			}
			if (k < schemes.size()) {
				len = schemes[k].length();

				uint32_t count = str.length();
				bool paren = false;

				for (uint32_t i = pos + len; i < count; i++) {
					uint32_t c = str.c_str()[i];

					if (!IsUrlChar(c, paren))
						break;
					if (c == '(')
						paren = true;
					else if (c == ')')
						paren = false;
					len++;
				}
				text->m_links.push_back(Text::Link(pos, len, Text::LINK_FOUND_URL, bits[k]));
				if (text->m_text != NULL) {
					int32_t set = text->m_text->SetStyle(
						pos, len, *text->m_link_styles[Text::LINK_STYLE_AUTO]);

					if (set > 0 && (set & 2))
						text->ChangeState(3, -1);
				}
			}
			size = str.length();
		}
		pos += len;
	}
	return 0;
}

/* a character of an e-mail address's name or domain */
static inline bool IsMailChar(uint32_t c)
{
	return c == '-' || c == '.' || (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
	       c == '_' || (c >= 'a' && c <= 'z');
}

int32_t ScePafWidget_F7F1BE9A(Text *text, const wstring &str)
{
	int32_t ret = 0;
	uint32_t size = str.length();

	for (uint32_t pos = 0; pos < size;) {
		const wchar_t *buf = str.c_str();
		uint32_t len = 1;

		if (buf[pos] == L'@' && pos != 0) {
			/* the name before the '@' */
			do {
				if (!IsMailChar(buf[pos - 1]))
					break;
				pos--;
				len++;
			} while (pos != 0);

			/* the domain after it, a '.' only after another character */
			if (len > 1 && pos + len < size) {
				bool valid = false;

				for (uint32_t i = pos + len; i < size; i++) {
					uint32_t c = buf[i];

					if (c != '-' && (!valid || c != '.') && !(c >= '0' && c <= '9') &&
					    !(c >= 'A' && c <= 'Z') && c != '_' &&
					    !(c >= 'a' && c <= 'z'))
						break;
					valid = c != '.';
					len++;
				}
				if (valid) {
					text->m_links.push_back(
						Text::Link(pos, len, Text::LINK_FOUND_MAIL, 0));
					if (text->m_text != NULL) {
						int32_t set = text->m_text->SetStyle(
							pos, len,
							*text->m_link_styles[Text::LINK_STYLE_AUTO]);

						if (set > 0 && (set & 2))
							text->ChangeState(3, -1);
					}
					size = str.length();
				}
			}
		}
		pos += len;
	}
	return ret;
}

int32_t ScePafWidget_439FFB75(Text *text)
{
	if (text->m_auto_link_detect != 0) {
		wstring str;

		text->m_text->GetString(str);
		if (text->m_auto_link_detect & 1)
			ScePafWidget_374B1ECF(text, str);
		if (text->m_auto_link_detect & 2)
			ScePafWidget_F7F1BE9A(text, str);
	}
	return 0;
}

int32_t ScePafWidget_D56954F6(Text *text)
{
	if (text->m_auto_link_detect != 0) {
		vector<Text::Link>::iterator it = text->m_links.begin();

		while (it != text->m_links.end()) {
			if (it->m_found != Text::LINK_SET) {
				if (text->m_text != NULL) {
					int32_t ret = text->m_text->SetStyle(
						it->m_index, it->m_len,
						*text->m_link_styles[Text::LINK_STYLE_NORMAL]);

					if (ret > 0 && (ret & 2))
						text->ChangeState(3, -1);
				}
				if (text->m_focus_link == it.m_ptr)
					text->m_focus_link = NULL;
				it = text->m_links.erase(it, it + 1);
			} else {
				++it;
			}
		}
	}
	text->UpdateTouchMode();
	return 0;
}

namespace paf {
namespace ui {

int32_t Text::SetString(const wstring &str, int32_t obj)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text == NULL)
		return ret;

	/* a running auto scroll back at its start, shown in full */
	if (m_auto_scroll != NULL) {
		m_auto_scroll->Restore();

		math::v4 color = m_text->GetColor();

		color.set_w(1.0f);
		m_text->SetColor(color);
		if (m_auto_scroll->m_length_new)
			m_auto_scroll->m_length = math::v2(0.0f, 0.0f);
	}
	ScePafWidget_D56954F6(this);
	if (m_edit_menu_listener != NULL)
		m_edit_menu_listener->EndEdit(static_cast<TextBox *>(this));
	ret = m_text->SetString(str);
	if (ret > 0) {
		if (ret & 2)
			ChangeState(3, -1);
		ret = 0;
	}
	ScePafWidget_439FFB75(this);
	if (m_auto_scroll != NULL)
		m_auto_scroll->Update();
	return ret;
}

int32_t Text::GetString(wstring &str, int32_t obj)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		ret = m_text->GetString(str);
	return ret;
}

int32_t Text::SetLayoutAttribute(graph::TextLayoutAttribute attr, int32_t val)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL) {
		ret = m_text->SetLayoutAttribute(attr, val);
		if (ret > 0) {
			if (ret & 2)
				ChangeState(3, -1);
			return 0;
		}
	}
	return ret;
}

int32_t Text::GetLayoutAttribute(graph::TextLayoutAttribute attr, int32_t &val)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetLayoutAttribute(attr, val);
	return ret;
}

int32_t Text::SetLayoutAttribute(graph::TextLayoutAttribute attr, float val)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL) {
		ret = m_text->SetLayoutAttribute(attr, val);
		if (ret > 0) {
			if (ret & 2)
				ChangeState(3, -1);
			return 0;
		}
	}
	return ret;
}

int32_t Text::GetLayoutAttribute(graph::TextLayoutAttribute attr, float &val)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetLayoutAttribute(attr, val);
	return ret;
}

int32_t Text::GetLayoutAttribute(graph::TextLayoutAttribute attr, math::v2 &val)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetLayoutAttribute(attr, val);
	return ret;
}

int32_t Text::GetLayoutAttribute(graph::TextLayoutAttribute attr, bool &val)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetLayoutAttribute(attr, val);
	return ret;
}

int32_t Text::SetStyleAttribute(graph::TextStyleAttribute attr, uint32_t index, uint32_t len, int32_t val)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL) {
		ret = m_text->SetStyleAttribute(attr, index, len, val);
		if (ret > 0) {
			if (ret & 2)
				ChangeState(3, -1);
			return 0;
		}
	}
	return ret;
}

int32_t Text::GetStyleAttribute(graph::TextStyleAttribute attr, uint32_t index, int32_t &val)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetStyleAttribute(attr, index, val);
	return ret;
}

}
}

using namespace paf;
using namespace paf::ui;

int32_t scePafWidgetSetFontSize(Widget *widget, float size, graph::TextStyleAttribute attr,
				uint32_t pos, uint32_t len)
{
	Text *text = (Text *)widget;
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (text->m_text != NULL) {
		ret = text->m_text->SetStyleAttribute(attr, pos, len, size);
		if (ret > 0) {
			if (ret & 2)
				text->ChangeState(3, -1);
			return 0;
		}
	}
	return ret;
}

namespace paf {
namespace ui {

int32_t Text::GetStyleAttribute(graph::TextStyleAttribute attr, uint32_t index, float &val)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetStyleAttribute(attr, index, val);
	return ret;
}

int32_t Text::SetStyleAttribute(graph::TextStyleAttribute attr, uint32_t index, uint32_t len, const math::v2 &val)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL) {
		ret = m_text->SetStyleAttribute(attr, index, len, val);
		if (ret > 0) {
			if (ret & 2)
				ChangeState(3, -1);
			return 0;
		}
	}
	return ret;
}

int32_t Text::GetStyleAttribute(graph::TextStyleAttribute attr, uint32_t index, math::v2 &val)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetStyleAttribute(attr, index, val);
	return ret;
}

int32_t Text::SetStyleAttribute(graph::TextStyleAttribute attr, uint32_t index, uint32_t len, const math::v4 &val)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL) {
		ret = m_text->SetStyleAttribute(attr, index, len, val);
		if (ret > 0) {
			if (ret & 2)
				ChangeState(3, -1);
			return 0;
		}
	}
	return ret;
}

int32_t Text::GetStyleAttribute(graph::TextStyleAttribute attr, uint32_t index, math::v4 &val)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetStyleAttribute(attr, index, val);
	return ret;
}

int32_t Text::SetStyleAttribute(graph::TextStyleAttribute attr, uint32_t index, uint32_t len, bool val)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL) {
		ret = m_text->SetStyleAttribute(attr, index, len, val);
		if (ret > 0) {
			if (ret & 2)
				ChangeState(3, -1);
			return 0;
		}
	}
	return ret;
}

int32_t Text::GetStyleAttribute(graph::TextStyleAttribute attr, uint32_t index, bool &val)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetStyleAttribute(attr, index, val);
	return ret;
}

int32_t Text::GetStyle(uint32_t index, graph::TextStyle &style)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetStyle(index, style);
	return ret;
}

int32_t Text::ReplaceString(uint32_t index, uint32_t len, const wstring &str)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL) {
		if (m_edit_menu_listener != NULL)
			m_edit_menu_listener->EndEdit(static_cast<TextBox *>(this));
		ret = m_text->ReplaceString(index, len, str);
		if (ret > 0) {
			if (ret & 2)
				ChangeState(3, -1);
			return 0;
		}
	}
	return ret;
}

int32_t Text::GetLength(uint32_t &len)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetLength(len);
	return ret;
}

}
}

using namespace paf;
using namespace paf::ui;

int32_t ScePafWidget_62604965(Text *text, const wchar_t **buf, uint32_t *len)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (text->m_text != NULL)
		return text->m_text->GetStringBuffer(*buf, *len);
	return ret;
}

namespace paf {
namespace ui {

int32_t Text::SetAttributedString(const graph::AttributedString &str)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL) {
		ret = m_text->SetAttributedString(str);
		if (ret > 0) {
			if (ret & 2)
				ChangeState(3, -1);
			return 0;
		}
	}
	return ret;
}

int32_t Text::GetAttributedString(graph::AttributedString &str)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetAttributedString(str);
	return ret;
}

}
}

using namespace paf;
using namespace paf::ui;

int32_t ScePafWidget_BC74324A(Text *text, const graph::TextStyle &style)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (text->m_text != NULL) {
		ret = text->m_text->SetPlaceholderStyle(style);
		if (ret > 0) {
			if (ret & 2)
				text->ChangeState(3, -1);
			return 0;
		}
	}
	return ret;
}

int32_t ScePafWidget_3CA096AE(Text *text, const wstring &str)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (text->m_text != NULL) {
		ret = text->m_text->SetPlaceholderString(str);
		if (ret > 0) {
			if (ret & 2)
				text->ChangeState(3, -1);
			return 0;
		}
	}
	return ret;
}

int32_t ScePafWidget_758E46AA(Text *text, wstring &str)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (text->m_text != NULL)
		return text->m_text->GetPlaceholderString(str);
	return ret;
}

namespace paf {
namespace ui {

int32_t Text::GetBounds(math::v2 &bounds)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetBounds(bounds);
	return ret;
}

int32_t Text::GetLineCount(uint32_t &count)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetLineCount(count);
	return ret;
}

int32_t Text::SetCharPos(uint32_t index, const math::v2 &pos)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->SetCharPos(index, pos);
	return ret;
}

int32_t Text::GetCharInfo(uint32_t index, graph::TextLayoutCharInfo &info)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetCharInfo(index, info);
	return ret;
}

int32_t Text::GetLineInfo(uint32_t line, graph::TextLayoutLineInfo &info)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetLineInfo(line, info);
	return ret;
}

int32_t Text::GetNextUpHit(uint32_t index, uint32_t &next)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetNextUpHit(index, next);
	return ret;
}

int32_t Text::GetNextDownHit(uint32_t index, uint32_t &next)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetNextDownHit(index, next);
	return ret;
}

int32_t Text::GetNextLeftHit(uint32_t index, uint32_t &next)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetNextLeftHit(index, next);
	return ret;
}

int32_t Text::GetNextRightHit(uint32_t index, uint32_t &next)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetNextRightHit(index, next);
	return ret;
}

}
}

using namespace paf;
using namespace paf::ui;

int32_t ScePafWidget_773A87E8(Text *text, const math::v2 &pos, bool inside, uint32_t &index)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (text->m_text != NULL)
		return text->m_text->HitTestChar(pos, inside, index);
	return ret;
}

namespace paf {
namespace ui {

int32_t Text::MoveCaretUp()
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->MoveCaretUp();
	return ret;
}

int32_t Text::MoveCaretDown()
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->MoveCaretDown();
	return ret;
}

int32_t Text::MoveCaretLeft()
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->MoveCaretLeft();
	return ret;
}

int32_t Text::MoveCaretRight()
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->MoveCaretRight();
	return ret;
}

int32_t Text::SetCaretPos(const math::v2 &pos)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->SetCaretPos(pos);
	return ret;
}

int32_t Text::SetSelectionPos(const math::v2 &start, const math::v2 &end)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL) {
		ret = m_text->SetSelectionPos(start, end);
		if (ret == 0) {
			Event e(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, 0, 0, 0, 0);

			DoEvent(CB_TEXT_SELECTION_POS_CHANGED, &e);
		}
	}
	return ret;
}

int32_t Text::SetCaretIndex(uint32_t index)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->SetCaretIndex(index);
	return ret;
}

int32_t Text::GetCaretIndex(uint32_t &index)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetCaretIndex(index);
	return ret;
}

int32_t Text::SetSelectionRange(uint32_t index, uint32_t len)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->SetSelectionRange(index, len);
	return ret;
}

int32_t Text::GetSelectionRange(uint32_t &index, uint32_t &len)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetSelectionRange(index, len);
	return ret;
}

int32_t Text::SetCaretVisible(bool visible)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->SetCaretVisible(visible);
	return ret;
}

int32_t Text::GetCaretVisible(bool &visible)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetCaretVisible(visible);
	return ret;
}

int32_t Text::SetCaretColor(const math::v4 &color)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->SetCaretColor(color);
	return ret;
}

int32_t Text::GetCaretColor(math::v4 &color)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetCaretColor(color);
	return ret;
}

int32_t Text::SetSelectionColor(const math::v4 &color)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->SetSelectionColor(color);
	return ret;
}

int32_t Text::GetSelectionColor(math::v4 &color)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL)
		return m_text->GetSelectionColor(color);
	return ret;
}

int32_t Text::Invalidate()
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL) {
		ret = m_text->Invalidate();
		if (ret > 0) {
			if (ret & 2)
				ChangeState(3, -1);
			return 0;
		}
	}
	return ret;
}

}
}

using namespace paf;
using namespace paf::ui;

int32_t ScePafWidget_DA5F91F8(TextBox *text_box, ScrollView *view)
{
	if (text_box->m_text == NULL)
		return SCE_PAF_ERROR_NO_OBJECT;
	if (view == NULL)
		return SCE_PAF_ERROR_INVALID_ARGUMENT;
	if (text_box->m_auto_scroll != NULL)
		text_box->m_auto_scroll->Restore();
	text_box->m_scroll_view = view;
	return 0;
}

int32_t ScePafWidget_B80D9362(Text *text)
{
	if (text->m_text == NULL)
		return SCE_PAF_ERROR_NO_OBJECT;
	if (text->m_scroll_view == NULL)
		return SCE_PAF_ERROR_INVALID_ARGUMENT;
	text->m_scroll_view = NULL;
	if (text->m_auto_scroll != NULL)
		text->m_auto_scroll->Update();
	return 0;
}

namespace paf {
namespace ui {

int32_t Text::SetSelectionMode(bool mode)
{
	return SCE_PAF_ERROR_NO_ENTRY;
}

int32_t Text::GetSelectionMode(bool &mode)
{
	mode = m_selection_mode;
	return 0;
}

}
}

using namespace paf;
using namespace paf::ui;

int32_t ScePafWidget_3E4AABD3(Text *text, TextBoxEditMenuListener *listener)
{
	if (listener != text->m_edit_menu_listener) {
		if (text->m_edit_menu_listener != NULL)
			text->m_edit_menu_listener->EndEdit(static_cast<TextBox *>(text));
		text->m_edit_menu_listener = listener;
	}
	return 0;
}

namespace paf {
namespace ui {

void Text::DoNothing()
{
}

}
}

using namespace paf;
using namespace paf::ui;

int32_t ScePafWidget_B93118F3(Text *text, bool on)
{
	bool cur;

	ScePafWidget_222D55C7(text, &cur);
	if (on != cur) {
		if (on)
			text->m_touch_auto_scroll = true;
		else
			text->m_touch_auto_scroll = false;
		text->UpdateTouchMode();
		if (on) {
			text->m_edit_menu_listener = NULL;
			text->m_hit_detect_mode = 0;
			text->m_hit_detect_b_mode = 0;
			text->m_hit_check = 1;
		} else {
			text->m_edit_menu_listener = ScePafWidget_1EE9D37E();
			text->m_hit_check = 0;
			text->m_hit_detect_b_mode = 2;
			text->m_hit_detect_mode = 2;
		}
	}
	return 0;
}

namespace paf {
namespace ui {

int32_t Text::CreateLinkStyles()
{
	int32_t ret = 0;

	if (m_link_styles[LINK_STYLE_NORMAL] != NULL)
		return ret;

	ret = SCE_PAF_ERROR_NO_OBJECT;
	m_link_styles[LINK_STYLE_NORMAL] = new graph::TextStyle;
	if (m_link_styles[LINK_STYLE_NORMAL] != NULL) {
		m_text->GetStyle(0, *m_link_styles[LINK_STYLE_NORMAL]);
		m_link_styles[LINK_STYLE_AUTO] = new graph::TextStyle(*m_link_styles[LINK_STYLE_NORMAL]);
		m_link_styles[LINK_STYLE_PRESSED] =
			new graph::TextStyle(*m_link_styles[LINK_STYLE_NORMAL]);
		if (m_link_styles[LINK_STYLE_AUTO] != NULL && m_link_styles[LINK_STYLE_PRESSED] != NULL) {
			m_link_styles[LINK_STYLE_AUTO]->SetStyleAttribute(graph::TextStyleAttribute_Color,
									  s_link_color);
			m_link_styles[LINK_STYLE_PRESSED]->SetStyleAttribute(
				graph::TextStyleAttribute_Color, s_link_pressed_color);
			ret = 0;
		}
		if (m_link_styles[LINK_STYLE_FOCUSED] == NULL) {
			m_link_styles[LINK_STYLE_FOCUSED] =
				new graph::TextStyle(*m_link_styles[LINK_STYLE_NORMAL]);
			if (m_link_styles[LINK_STYLE_FOCUSED] != NULL) {
				m_link_styles[LINK_STYLE_FOCUSED]->SetStyleAttribute(
					graph::TextStyleAttribute_Color, s_link_color);
				m_scene_listener = new EventCBListener(LinkStyleCB, this);
				m_scene->AddEventListener(0x80000A, m_scene_listener, false);
				return 0;
			}
		}
		if (ret >= 0)
			return ret;
	}
	DeleteLinkStyles();
	return ret;
}

}
}

int32_t ScePafWidget_00253246(Text *text, int32_t mode)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (text->m_text != NULL) {
		ret = 0;
		if (mode != text->m_auto_link_detect) {
			ret = text->CreateLinkStyles();
			if (ret == 0) {
				ScePafWidget_D56954F6(text);
				text->m_auto_link_detect = mode;
				if (text->m_auto_link_detect != 0) {
					wstring str;

					text->m_text->GetString(str);
					if (text->m_auto_link_detect & 1)
						ScePafWidget_374B1ECF(text, str);
					if (text->m_auto_link_detect & 2)
						ScePafWidget_F7F1BE9A(text, str);
				}
				text->m_hit_detect_mode = 0;
				text->m_hit_detect_b_mode = 0;
				text->m_hit_check = 1;
				if (text->m_link_event)
					text->EnableFocusEvent(true);
			}
		}
	}
	return ret;
}

int32_t ScePafWidget_5E14AD2A(Text *text, int32_t *mode)
{
	*mode = text->m_auto_link_detect;
	return 0;
}

namespace paf {
namespace ui {

int32_t Text::SetLinksStyleAuto()
{
	for (vector<Link>::iterator it = m_links.begin(); it != m_links.end(); ++it)
		SetStyle(it->m_index, it->m_len, *m_link_styles[LINK_STYLE_AUTO]);
	return 0;
}

int32_t Text::SetLinksStyleNormal()
{
	for (vector<Link>::iterator it = m_links.begin(); it != m_links.end(); ++it)
		SetStyle(it->m_index, it->m_len, *m_link_styles[LINK_STYLE_NORMAL]);
	return 0;
}

}
}

int32_t ScePafWidget_B2A7DF66(Text *text, int32_t which, const graph::TextStyle &style)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (text->m_text != NULL) {
		ret = SCE_PAF_ERROR_INVALID_ARGUMENT;
		if (which != 0) {
			ret = text->CreateLinkStyles();
			if (ret == 0) {
				text->SetLinksStyleNormal();
				if (which & 1) {
					*text->m_link_styles[Text::LINK_STYLE_AUTO] = style;
					if (text->m_link_styles[Text::LINK_STYLE_FOCUSED] != NULL)
						*text->m_link_styles[Text::LINK_STYLE_FOCUSED] =
							*text->m_link_styles[Text::LINK_STYLE_AUTO];
				}
				if (which & 2)
					*text->m_link_styles[Text::LINK_STYLE_PRESSED] = style;
				if (which & 4)
					*text->m_link_styles[Text::LINK_STYLE_FOCUSED] = style;
				text->SetLinksStyleAuto();
			}
		}
	}
	return ret;
}

int32_t ScePafWidget_DDB0437F(Text *text, int32_t scheme)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (text->m_text != NULL) {
		ret = 0;
		if (scheme != text->m_url_scheme) {
			ScePafWidget_D56954F6(text);
			text->m_url_scheme = scheme;
			if (text->m_auto_link_detect != 0) {
				wstring str;

				text->m_text->GetString(str);
				if (text->m_auto_link_detect & 1)
					ScePafWidget_374B1ECF(text, str);
				if (text->m_auto_link_detect & 2)
					ScePafWidget_F7F1BE9A(text, str);
			}
		}
	}
	return ret;
}

int32_t ScePafWidget_7524D3BB(Text *text, int32_t *scheme)
{
	*scheme = text->m_url_scheme;
	return 0;
}

int32_t ScePafWidget_4D5F35D5(Text *text, uint32_t index, uint32_t len)
{
	int32_t ret = SCE_PAF_ERROR_INVALID_ARGUMENT;

	if (len != 0) {
		for (vector<Text::Link>::iterator it = text->m_links.begin(); it != text->m_links.end();
		     ++it) {
			if (it->m_index == index && it->m_len == len && it->m_found == Text::LINK_SET)
				return SCE_PAF_ERROR_ALREADY_CREATED;
		}
		ret = text->CreateLinkStyles();
		if (ret == 0) {
			text->m_links.push_back(Text::Link(index, len, Text::LINK_SET, 0));
			text->SetStyle(index, len, *text->m_link_styles[Text::LINK_STYLE_AUTO]);
			text->m_hit_detect_mode = 0;
			text->m_hit_detect_b_mode = 0;
			text->m_hit_check = 1;
			if (text->m_link_event)
				text->EnableFocusEvent(true);
		}
	}
	return ret;
}

int32_t ScePafWidget_04FF32F3(Text *text, uint32_t index, uint32_t len)
{
	int32_t ret = SCE_PAF_ERROR_INVALID_ARGUMENT;

	if (len != 0) {
		for (vector<Text::Link>::iterator it = text->m_links.begin(); it != text->m_links.end();
		     ++it) {
			if (it->m_index == index && it->m_len == len && it->m_found == Text::LINK_SET) {
				text->SetStyle(it->m_index, it->m_len,
					       *text->m_link_styles[Text::LINK_STYLE_NORMAL]);
				if (text->m_focus_link == it.m_ptr)
					text->m_focus_link = NULL;
				text->m_links.erase(it, it + 1);
				if (text->m_links.size() == 0 && text->m_link_event)
					text->EnableFocusEvent(false);
				ret = 0;
				break;
			}
		}
	}
	text->UpdateTouchMode();
	return ret;
}

namespace paf {
namespace ui {

int32_t Text::StartAutoScroll(ScrollType type)
{
	if (!m_touch_auto_scroll || !m_touch_held)
		FriendAutoScroll(FRIEND_START, &type);

	int32_t ret = SCE_PAF_ERROR_BUSY;

	if ((!m_touch_auto_scroll || !m_touch_held) && !IsAutoScrollRunning())
		ret = RunAutoScroll(type);
	return ret;
}

bool Text::IsScrolling()
{
	bool running = false;
	bool ret = false;

	if (m_auto_scroll != NULL && m_auto_scroll->IsRunning())
		running = true;
	if (FriendAutoScroll(FRIEND_IS_RUNNING, NULL) | running)
		ret = true;
	return ret;
}

}
}

int32_t ScePafWidget_7C976DF2(Text *text, float speed)
{
	if (!text->m_touch_auto_scroll || !text->m_touch_held)
		text->FriendAutoScroll(Text::FRIEND_SET_SPEED, &speed);

	int32_t ret = SCE_PAF_ERROR_BUSY;

	if (!text->m_touch_auto_scroll || !text->m_touch_held) {
		ret = SCE_PAF_ERROR_NO_OBJECT;
		if (text->m_auto_scroll != NULL)
			ret = text->m_auto_scroll->SetSpeed(speed, 2);
	}
	return ret;
}

int32_t ScePafWidget_9EA15F9C(Text *text, float &speed)
{
	if (text->m_auto_scroll != NULL)
		return text->m_auto_scroll->GetSpeed(speed);
	return SCE_PAF_ERROR_NO_OBJECT;
}

int32_t ScePafWidget_B9F2249C(Text *text, const math::v2 &length)
{
	if (text->m_auto_scroll == NULL)
		return SCE_PAF_ERROR_NO_OBJECT;
	text->m_auto_scroll->m_length = length;
	return 0;
}

int32_t ScePafWidget_B9F3F203(Text *text, const IDParam &id)
{
	TextAutoScroll *scroll = text->m_auto_scroll;

	if (scroll == NULL && text->m_auto_scroll_enable)
		scroll = text->m_auto_scroll = new TextAutoScroll(text, text->m_text);
	if (scroll == NULL)
		return SCE_PAF_ERROR_NO_OBJECT;

	IDParam copy(id);

	return scroll->AddFriend(copy);
}

int32_t ScePafWidget_3612C18B(Text *text, const IDParam &id)
{
	TextAutoScroll *scroll = text->m_auto_scroll;

	if (scroll == NULL)
		return SCE_PAF_ERROR_NO_OBJECT;

	IDParam copy(id);

	return scroll->RemoveFriend(copy);
}

namespace paf {
namespace ui {

int32_t Text::SendRectEvent(int32_t type, const math::v4 &rect)
{
	Event e(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, 0, 0, 0, 0);

	e.m_data[0] = (int32_t)rect.extract_x();
	e.m_data[1] = (int32_t)rect.extract_y();
	e.m_data[2] = (int32_t)rect.extract_z();
	e.m_data[3] = (int32_t)rect.extract_w();
	DoEvent(type, &e);
	return 0;
}

}
}

int32_t ScePafWidget_4608F7EF(Text *text, math::v2 *origin)
{
	if (text->m_auto_scroll == NULL)
		return SCE_PAF_ERROR_NO_OBJECT;
	*origin = text->m_auto_scroll->m_origin;
	return 0;
}

void Text::OnTouchCaptureChanged(uint32_t port, uint32_t id, int32_t arg)
{
	if (m_touch_captures.size() == 0)
		HandleTouchRelease();
}

namespace paf {
namespace ui {

int32_t Text::AdjustOffsetY(int32_t type, const math::v4 &size, math::v4 &offs)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;
	uint32_t count;

	if (m_text == NULL)
		return ret;
	ret = m_text->GetLineCount(count);
	if (ret == 0 && count != 0) {
		switch (type) {
		case 0: {
			graph::TextLayoutLineInfo first;
			graph::TextLayoutLineInfo last;

			m_text->GetLineInfo(0, first);
			m_text->GetLineInfo(count - 1, last);
			offs.set_y((float)(int)((float)(((first.frame.extract_y() + first.descender) +
							 last.frame.extract_y() + last.descender) -
						       size.extract_y()) * 0.5f));
			break;
		}
		case 1: {
			graph::TextLayoutLineInfo last;

			m_text->GetLineInfo(count - 1, last);
			offs.set_y((float)(int)((last.frame.extract_y() + last.descender) -
						size.extract_y() * 0.5f));
			break;
		}
		case 2:
		case 3: {
			graph::TextLayoutLineInfo first;

			m_text->GetLineInfo(0, first);
			offs.set_y((float)(int)((first.frame.extract_y() + first.descender) -
						size.extract_y() * 0.5f));
			break;
		}
		case 4: {
			graph::TextLayoutLineInfo last;

			m_text->GetLineInfo(count - 1, last);
			offs.set_y((float)(int)(last.frame.extract_y() +
						((last.descender + last.ascender) - size.extract_y()) *
							0.5f));
			break;
		}
		case 5: {
			graph::TextLayoutLineInfo first;

			m_text->GetLineInfo(0, first);
			offs.set_y((float)(int)(first.frame.extract_y() +
						((first.descender + first.ascender) - size.extract_y()) *
							0.5f));
			break;
		}
		default:
			break;
		}
	}
	return ret;
}

math::v4 Text::calc_align_offset(const math::v4 &size, const math::v4 &aoffs, uint32_t ax,
				 uint32_t ay, uint32_t az)
{
	math::v4 result = Widget::calc_align_offset(size, aoffs, ax, ay, az);

	AdjustOffsetY(ay - 4, size, result);
	return result;
}

math::v4 Text::calc_origin_offset(const math::v4 &size, const math::v4 &offs, uint32_t ax,
				  uint32_t ay, uint32_t az)
{
	math::v4 result = Widget::calc_origin_offset(size, offs, ax, ay, az);

	AdjustOffsetY(ay - 4, size, result);
	return result;
}

math::v4 Text::calc_anchor_offset(const math::v4 &size, const math::v4 &offs, uint32_t ax,
				  uint32_t ay, uint32_t az)
{
	math::v4 result = Widget::calc_anchor_offset(size, offs, ax, ay, az);

	AdjustOffsetY(ay - 5, size, result);
	return result;
}

}
}

int32_t ScePafWidget_55EC0090(Text *text, const math::v2 &pos)
{
	uint32_t index;

	text->m_focus_link = NULL;

	int32_t ret = text->m_text->HitTestChar(pos, true, index);

	if (ret == 0) {
		for (vector<Text::Link>::iterator it = text->m_links.begin();
		     it != text->m_links.end(); ++it) {
			if (it->m_index <= index && index <= it->m_index + it->m_len) {
				ret = 0;
				text->m_focus_link = it.m_ptr;
				break;
			}
		}
	}
	text->UpdateTouchMode();
	return ret;
}

namespace paf {
namespace ui {

int32_t Text::HandleTouchPress(Event *e)
{
	uint32_t port = (e->m_detail >> 20) & 0xf;
	uint32_t id = (uint8_t)e->m_detail;
	int32_t ret = SCE_PAF_ERROR_BUSY;

	if (id < 8) {
		HitInfo *hit = &m_env->m_touch_hit[0][id];

		if (hit != NULL && hit->m_widget == this) {
			math::v2 pos;

			ret = GetTouchTextPos(port, id, pos);
			if (ret == 0) {
				if (m_focus_link != NULL && m_link_styles[LINK_STYLE_AUTO] != NULL) {
					if (m_text != NULL) {
						int32_t set = m_text->SetStyle(
							m_focus_link->m_index, m_focus_link->m_len,
							*m_link_styles[LINK_STYLE_AUTO]);

						if (set > 0 && (set & 2))
							ChangeState(3, -1);
					}
				}
				ScePafWidget_55EC0090(this, pos);
				if (m_focus_link != NULL && m_link_styles[LINK_STYLE_PRESSED] != NULL) {
					if (m_text != NULL) {
						int32_t set = m_text->SetStyle(
							m_focus_link->m_index, m_focus_link->m_len,
							*m_link_styles[LINK_STYLE_PRESSED]);

						if (set > 0 && (set & 2))
							ChangeState(3, -1);
					}
				}
				ApplyTouchGesture(hit->m_port, hit->m_touch_id, m_touch_gesture_mask, NULL);
			}
		}
	}
	return ret;
}

int32_t Text::EndTouch()
{
	int32_t ret = SCE_PAF_ERROR_BUSY;

	if (m_touch_modes != 0) {
		if (m_edit_menu_listener != NULL)
			m_edit_menu_listener->NotifyCaretMove(static_cast<TextBox *>(this));
		if (m_selection_mode) {
			Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, 0, 0, 0, 0);

			DoEvent(CB_TEXT_SELECTION_END, &ev);
		}
		if (m_touch_auto_scroll && m_auto_scroll != NULL)
			m_auto_scroll->IsRunning();
		ret = 0;
	}
	return ret;
}

int32_t Text::HandleTouchRelease()
{
	if (m_focus_link != NULL && m_text != NULL) {
		int32_t set = m_text->SetStyle(m_focus_link->m_index, m_focus_link->m_len,
					       *m_link_styles[LINK_STYLE_AUTO]);

		if (set > 0 && (set & 2))
			ChangeState(3, -1);
	}

	int32_t ret = EndTouch();

	if (ret == 0) {
		m_touch_held = false;
		if (!m_touch_auto_scroll && m_auto_scroll != NULL)
			m_auto_scroll->Update();
	}
	return ret;
}

int32_t Text::ScePafWidget_695200BA(Event *e)
{
	int32_t ret = SCE_PAF_ERROR_BUSY;

	if (m_focus_link != NULL) {
		ScePafWidget_2B1A1493(this, 0);

		Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, 0, 0, 0, 0);

		ev.m_data[0] = m_focus_link->m_index;
		ev.m_data[1] = m_focus_link->m_len;
		ev.m_data[2] = m_focus_link->m_found;
		ev.m_data[3] = (int16_t)m_focus_link->m_scheme;
		DoEvent(CB_TEXT_LINK, &ev);
		if (m_link_styles[LINK_STYLE_AUTO] != NULL && m_text != NULL) {
			int32_t set = m_text->SetStyle(m_focus_link->m_index, m_focus_link->m_len,
						       *m_link_styles[LINK_STYLE_AUTO]);

			if (set > 0 && (set & 2))
				ChangeState(3, -1);
		}
		ret = 0;
	}
	return ret;
}

int32_t Text::UpdateTouch(const math::v2 &pos)
{
	int32_t ret = SCE_PAF_ERROR_BUSY;

	if (m_touch_modes != 0) {
		if (m_edit_menu_listener != NULL)
			m_edit_menu_listener->Update(static_cast<TextBox *>(this), pos);
		if (m_selection_mode && m_touch_state1)
			SetSelectionPos(m_touch_pos, pos);
		ret = 0;
	}
	return ret;
}

int32_t Text::SetVisiblePos(const math::v2 &pos)
{
	m_text->SetLayoutAttribute(graph::TextLayoutAttribute_VisiblePos, pos);
	return 0;
}

int32_t Text::AdjustSize()
{
	float top, bottom, left, right;

	if (m_adjust_param != NULL) {
		top = m_adjust_param->m_top_margin;
		bottom = m_adjust_param->m_bottom_margin;
		left = m_adjust_param->m_left_margin;
		right = m_adjust_param->m_right_margin;
	} else {
		top = ScePafWidget_BE1CEA2E.m_top_margin;
		bottom = ScePafWidget_BE1CEA2E.m_bottom_margin;
		left = ScePafWidget_BE1CEA2E.m_left_margin;
		right = ScePafWidget_BE1CEA2E.m_right_margin;
	}

	math::v2 margin(left - right, bottom - top);
	math::v4 size = m_size;

	m_size = math::v4(-1.0f, -1.0f, size.extract_z(), size.extract_w());
	SetSize_core_force(math::v4(m_text->m_size.extract_x() + left + right,
				    m_text->m_size.extract_y() + bottom + top, size.extract_z(),
				    size.extract_w()),
			   0xFF02, false);
	m_text->SetPos(math::v3(margin.extract_x() * 0.5f, margin.extract_y() * 0.5f, 0.0f));
	return 0;
}

}
}

int32_t ScePafWidget_56384DA3(Widget *widget, int32_t id)
{
	AnimQueue *anim = widget->FindAnim(id);

	if (anim != NULL) {
		for (int32_t i = 0; i < 3; i++) {
			if (anim->m_queue[i] != NULL && !anim->m_queue[i]->IsFinished())
				return 1;
		}
	}
	return 0;
}

namespace paf {
namespace ui {

static void LinkStyleCB(int32_t type, Handler *self, Event *e, void *data)
{
	Text *text = (Text *)data;

	if (text->m_touch_captures.size() == 0) {
		Environment *env = text->m_env;

		if (env != NULL && env->m_focus_cursor.m_target == text) {
			Text::Link *link = text->m_focus_link;

			if (link == NULL) {
				math::v4 rect = text->GetLinkWorldRect(NULL);

				link = text->SearchLinkFocus(255, math::v4(rect), 0);
				env = text->m_env;
			}

			graph::TextStyle *focused = text->m_link_styles[Text::LINK_STYLE_FOCUSED];
			graph::TextStyle *normal = text->m_link_styles[Text::LINK_STYLE_AUTO];

			if (link != NULL) {
				if (focused != NULL && ScePafWidget_529FB877(env, -1)) {
					if (text->m_text != NULL) {
						int32_t set = text->m_text->SetStyle(
							link->m_index, link->m_len, *focused);

						if (set > 0 && (set & 2))
							text->ChangeState(3, -1);
					}
				} else if (normal != NULL) {
					if (text->m_text != NULL) {
						int32_t set = text->m_text->SetStyle(
							link->m_index, link->m_len, *normal);

						if (set > 0 && (set & 2))
							text->ChangeState(3, -1);
					}
				}
			}
		}
	}
}

}
}

namespace paf {
namespace ui {

Widget *Text::SetFocusOption(const void *param, int32_t option)
{
	if (param != NULL) {
		int32_t dir = *(const int32_t *)param;
		math::v4 rect = GetLinkWorldRect(m_focus_link);

		if (m_focus_link == NULL)
			dir = 255;

		math::v4 search = rect;

		if (SearchLinkFocus(dir, search, 0) != NULL) {
			if (m_env->m_focus_history_on)
				m_env->m_focus_cursor.RemoveFocusHistory(this);
			return this;
		}
	}
	return Widget::SetFocusOption(param, option);
}

int32_t Text::HandleKeycodeEvent(Event *e)
{
	if (e != NULL) {
		uint32_t dir = (uint8_t)e->m_detail;
		uint32_t type = (uint16_t)e->m_type;

		if (dir > 2) {
			if (dir > 4)
				goto out;
		} else if (dir == 0) {
			goto out;
		}
		if (type == 17 || type == 19) {
			math::v4 rect = GetLinkWorldRect(m_focus_link);

			if (m_focus_link == NULL)
				dir = 255;

			math::v4 search = rect;

			if (SearchLinkFocus(dir, search, 0) != NULL) {
				if (m_env->m_focus_history_on)
					m_env->m_focus_cursor.RemoveFocusHistory(this);
				e->m_type = (e->m_type & 0xf0ffffff) | 0x1000000;
			} else {
				CheckLinkFocusInView();
			}
		}
	}
out:
	return Widget::HandleKeycodeEvent(e);
}

int32_t Text::HandleFocusEvent(Event *e)
{
	if (e != NULL) {
		uint32_t type = (uint16_t)e->m_type;

		if (type > 0x401) {
			if (type <= 0x402) {
				if (m_focus_link != NULL && m_link_styles[LINK_STYLE_AUTO] != NULL &&
				    m_text != NULL) {
					int32_t set = m_text->SetStyle(m_focus_link->m_index,
								       m_focus_link->m_len,
								       *m_link_styles[LINK_STYLE_AUTO]);

					if (set > 0 && (set & 2))
						ChangeState(3, -1);
				}

				FocusShape *shape = m_focus_shape;

				if (shape != NULL && m_focus_link != NULL) {
					shape->m_offset = math::v3(0.0f, 0.0f, 0.0f);
					shape->m_size = math::v2(GetSize(0)->getImpl().getXY());
					ScePafWidget_13B246E0(this, shape);
				}
			}
		} else if (type > 0x400) {
			FocusSearchInfo info(255);

			info.m_ratio = 0.3f;
			info.m_origin = (Widget *)e->m_data[0];
			TextFocusCB(NULL, this, NULL, &info);
			if (m_focus_link != NULL) {
				UpdateSize();
				if (m_focus_shape != NULL)
					m_focus_shape->m_use_parts = false;
				e->m_type = (e->m_type & 0xf0ffffff) | 0x1000000;
			}
		}
	}
	return Handler::HandleFocusEvent(e);
}

void Text::SetFitToScene(bool on_off)
{
	if (!(m_size.extract_x() < m_scene->GetSize(0)->extract_x()) ||
	    !(m_size.extract_y() < m_scene->GetSize(0)->extract_y()))
		m_fit_to_scene = false;
	else
		m_fit_to_scene = on_off;
}

}
}

namespace paf {
namespace ui {

const char TextParam::m_param_name[] = "text_base";

const char *TextParam::GetParamName() const
{
	return m_param_name;
}

const char *TextParam::ParamName()
{
	return m_param_name;
}

TextParam::~TextParam()
{
}

int32_t TextParam::Create(int32_t mode)
{
	int32_t ret = WidgetParam::Create(mode);

	if (ret == 0 && m_serial != NULL) {
		m_serial->AddParam("selection_mode", &m_selection_mode, ResourceParam::SEMANTIC_BOOL);
		if (m_serial != NULL) {
			m_serial->AddParam("auto_link_detect", &m_auto_link_detect,
					   ResourceParam::SEMANTIC_INT);
			if (m_serial != NULL) {
				m_serial->AddParam("url_scheme", &m_url_scheme, ResourceParam::SEMANTIC_INT);
				if (m_serial != NULL) {
					m_serial->AddParam("auto_scroll", &m_auto_scroll,
							   ResourceParam::SEMANTIC_INT);
					if (m_serial != NULL) {
						m_serial->AddParam("auto_scroll_speed", &m_auto_scroll_speed,
								   ResourceParam::SEMANTIC_FLOAT);
						if (m_serial != NULL) {
							m_serial->AddParam("placeholder", &m_placeholder,
									   ResourceParam::SEMANTIC_LABELREF);
							if (m_serial != NULL) {
								m_serial->AddParam("scroll_view", &m_scroll_view,
										   ResourceParam::SEMANTIC_WIDGETREF);
								if (m_serial != NULL) {
									m_serial->AddParam(
										"long_press_text_auto_scroll_mode",
										&m_long_press_text_auto_scroll_mode,
										ResourceParam::SEMANTIC_BOOL);
									if (m_serial != NULL) {
										m_serial->AddParam(
											"auto_scroll_target_friend",
											&m_auto_scroll_target_friend,
											ResourceParam::SEMANTIC_WIDGETREF);
										if (m_serial != NULL)
											m_serial->AddParam(
												"disable_auto_scroll",
												&m_disable_auto_scroll,
												ResourceParam::SEMANTIC_BOOL);
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return ret;
}

int32_t TextParam::Set()
{
	int32_t ret = WidgetParam::Set();

	if (ret == 0) {
		Text *text = (Text *)m_widget;

		ScePafWidget_00253246(text, m_auto_link_detect);
		ScePafWidget_DDB0437F(text, m_url_scheme);
		if (m_auto_scroll != 0)
			text->StartAutoScroll((Text::ScrollType)m_auto_scroll);
		ScePafWidget_7C976DF2(text, m_auto_scroll_speed);

		wstring placeholder(GetString(m_placeholder));

		if (text->m_text != NULL) {
			int32_t set = text->m_text->SetPlaceholderString(placeholder);

			if (set > 0 && (set & 2))
				text->ChangeState(3, -1);
		}
		if (text->m_scene->FindChild(m_scroll_view, 0) != NULL) {
			if (text->m_scene->FindChild(m_scroll_view, 0)->IsInherit(
				    ScrollViewBase::TypeName())) {
				Widget *view = text->m_scene->FindChild(m_scroll_view, 0);

				if (view != NULL && text->m_text != NULL) {
					if (text->m_auto_scroll != NULL)
						text->m_auto_scroll->Restore();
					text->m_scroll_view = (ScrollViewBase *)view;
				}
			}
		}
		ScePafWidget_B93118F3(text, m_long_press_text_auto_scroll_mode);
		if (IDParam().GetIDHash() != m_auto_scroll_target_friend.GetIDHash())
			ScePafWidget_B9F3F203(text, m_auto_scroll_target_friend);
		if (m_disable_auto_scroll)
			text->m_auto_scroll_enable = false;
	}
	return ret;
}

int32_t TextParam::Get()
{
	int32_t ret = WidgetParam::Get();

	if (ret == 0) {
		Text *text = (Text *)m_widget;

		m_selection_mode = text->m_selection_mode;
		m_auto_link_detect = text->m_auto_link_detect;
		m_url_scheme = text->m_url_scheme;
		if (text->m_auto_scroll != NULL)
			text->m_auto_scroll->GetSpeed(m_auto_scroll_speed);
		m_long_press_text_auto_scroll_mode = text->m_touch_auto_scroll;
		m_disable_auto_scroll = text->m_auto_scroll_enable;
	}
	return ret;
}

TextStyleParam::~TextStyleParam()
{
	if (m_adjust != NULL) {
		if (m_adjust->m_ref_count-- == 1)
			delete m_adjust;
		m_adjust = NULL;
	}
}

const char *TextStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *TextStyleParam::ParamName()
{
	return m_param_name;
}

int32_t TextStyleParam::Create(int32_t mode)
{
	if (m_adjust == NULL)
		return -1;

	int32_t ret = WidgetStyleParam::Create(mode);

	if (ret == 0 && m_serial != NULL) {
		m_serial->AddParam("top_margin", &m_adjust->m_top_margin, ResourceParam::SEMANTIC_FLOAT);
		if (m_serial != NULL) {
			m_serial->AddParam("bottom_margin", &m_adjust->m_bottom_margin,
					   ResourceParam::SEMANTIC_FLOAT);
			if (m_serial != NULL) {
				m_serial->AddParam("left_margin", &m_adjust->m_left_margin,
						   ResourceParam::SEMANTIC_FLOAT);
				if (m_serial != NULL)
					m_serial->AddParam("right_margin", &m_adjust->m_right_margin,
							   ResourceParam::SEMANTIC_FLOAT);
			}
		}
	}
	return ret;
}

int32_t TextStyleParam::SetEach(Widget *widget)
{
	if (m_plane_param.m_id.GetIDHash() != 0)
		static_cast<Text *>(widget)->CreatePlaneObj();

	int32_t ret = WidgetStyleParam::SetEach(widget);

	if (ret == 0)
		ScePafWidget_55D83675(widget, m_adjust);
	return ret;
}

int32_t TextStyleParam::GetEach(Widget *widget)
{
	int32_t ret = WidgetStyleParam::GetEach(widget);

	if (ret == 0)
		widget->GetHitRegion();
	return ret;
}

const char TextStyleParam::m_param_name[] = "style_text";

const char TextSoundParam::m_param_name[] = "sound_text";

const char *TextSoundParam::GetParamName() const
{
	return m_param_name;
}

const char *TextSoundParam::ParamName()
{
	return m_param_name;
}

TextSoundParam::~TextSoundParam()
{
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewText(Widget *parent)
{
	return new Text(parent, NULL);
}

static WidgetParam *NewTextParam()
{
	return new TextParam;
}

static VersatileParam *NewTextLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewTextFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewTextStyleParam()
{
	return new TextStyleParam;
}

static VersatileParam *NewTextHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewTextSoundParam()
{
	return new TextSoundParam;
}

FactoryFuncTable g_factoryText = {
	Text::TypeName(),
	NewText,
	NewTextParam,
	NewTextLayoutParam,
	NewTextFocusParam,
	NewTextStyleParam,
	NewTextHandlerParam,
	NewTextSoundParam,
};
