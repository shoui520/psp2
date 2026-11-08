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
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <widget/core/event.h>
#include <widget/core/widget.h>
#include <widget/factory.h>
#include <widget/text_auto_scroll.h>
#include <widget/w_scene.h>
#include <widget/w_text.h>
#include <widget/w_text_box.h>
#include "../paf_types.h"

extern "C" {
/* the edit menu listener wutil::Init made (state.c) */
paf::ui::TextBoxEditMenuListener *ScePafWidget_1EE9D37E(void);
}

namespace paf {
namespace ui {

/* the text whose auto scroll a touch started last (name ours) */
static Text *s_touch_scroll_text;

static int32_t TextFocusCB(Focuser *focuser, Widget *widget, void *data, void *move);

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
		HitInfo *hit = &env->m_touch_hit[id];

		if (hit != NULL) {
			if (hit->m_hit_valid == 0 && id < 8 && port < 2) {
				TouchState *touch = &env->m_touch_state[port][id];

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

}
}
