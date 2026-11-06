/*
 * ui::TextAutoScroll (text_auto_scroll.h): a text scrolled through its box, horizontally or
 * vertically, as far as the longest of its friends; the layout's ellipsis and word wrap are off
 * while it scrolls. The file's name is ours.
 */
#include <stddef.h>
#include <stdint.h>
#include <widget/text_auto_scroll.h>
#include <widget/core/animlistener.h>
#include <widget/core/handler.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <graphics/text/text_layout.h>
#include <timer/timer.h>
#include <versatileparam/idparam.h>
#include <std/vector>
#include "../std/math.h"
#include "../paf_types.h"

namespace paf {
namespace ui {

/* the listener queued for the widget's animation (inlined in Sony's code, name ours) */
static inline __attribute__((always_inline)) void AddAnim(Widget *widget, int32_t id, Timer *t,
							  AnimCB cb_anim, void *data)
{
	widget->SetAnimListener(id, new AnimListener(t, widget, cb_anim, NULL, data), 0);
}

/* the parent's child of the ID, where it is a text (name ours) */
static inline __attribute__((always_inline)) Text *FindText(Widget *parent, const IDParam &id)
{
	if (parent->FindChild(id, 0) != NULL &&
	    parent->FindChild(id, 0)->IsInherit(Text::TypeName()))
		return (Text *)parent->FindChild(id, 0);
	return NULL;
}

/* the text's draw object, where it is a text object (name ours) */
static inline __attribute__((always_inline)) graph::TextObj *GetTextObj(Text *text)
{
	if (text->GetDrawObj(0) != NULL &&
	    text->GetDrawObj(0)->IsInherit(graph::TextObj::DrawObjType()))
		return (graph::TextObj *)text->GetDrawObj(0);
	return NULL;
}

/* the friend's text object, where the friend is a text that scrolls (name ours) */
static inline __attribute__((always_inline)) graph::TextObj *GetFriendTextObj(Widget *parent,
									     const IDParam &id)
{
	Text *text = FindText(parent, id);

	if (text != NULL && text->m_hold_count <= 0 && text->IsScrolling())
		return GetTextObj(text);
	return NULL;
}

TextAutoScroll::TextAutoScroll(Text *text, graph::TextObj *text_obj)
	: m_text(text), m_text_obj(text_obj), m_ellipsis(false), m_word_wrap(false),
	  m_length_new(false), m_type(0), m_speed(60.0f), m_length(0.0f, 0.0f),
	  m_origin(-1000.0f, -1000.0f)
{
}

TextAutoScroll::~TextAutoScroll()
{
}

int32_t TextAutoScroll::SetAlpha(float alpha)
{
	math::v4 color = m_text_obj->GetColor();

	color.set_w(alpha);
	m_text_obj->SetColor(color);
	return 0;
}

int32_t TextAutoScroll::Restore()
{
	graph::TextLayoutAttribute condense;
	bool condensed;

	if (m_text == NULL || m_text_obj == NULL || m_type == 0)
		return 0;

	switch (m_type) {
	case Text::SCROLL_TYPE_HORIZONTAL:
	case Text::SCROLL_TYPE_HORIZONTAL_ONCE:
	default:
		condense = graph::TextLayoutAttribute_CondenseX;
		break;
	case Text::SCROLL_TYPE_VERTICAL:
	case Text::SCROLL_TYPE_VERTICAL_ONCE:
		condense = graph::TextLayoutAttribute_CondenseY;
		break;
	}
	condensed = false;
	m_text_obj->GetLayoutAttribute(condense, condensed);
	if (condensed)
		return 0;

	if (m_ellipsis) {
		m_ellipsis = false;
		m_text_obj->SetLayoutAttribute(graph::TextLayoutAttribute_Ellipsis, true);
		ResetPos();
	}
	if (m_word_wrap) {
		m_word_wrap = false;
		m_text_obj->SetLayoutAttribute(graph::TextLayoutAttribute_WordWrap, true);
		ResetPos();
	}

	if (IsAnimRunning(Text::ANIM_AUTO_SCROLL)) {
		m_text->KillAnim(Text::ANIM_AUTO_SCROLL);
		if (IsOnce()) {
			/* a scroll that ran once fades back in */
			ResetPos();
			AddAnim(m_text, Text::ANIM_AUTO_SCROLL_FADE, new Timer(700.0f),
				OnFadeBackTick, this);
		} else {
			SetAlpha(1.0f);
			ResetPos();
		}
	}
	return 0;
}

int32_t TextAutoScroll::Stop()
{
	if (m_text_obj == NULL)
		return SCE_PAF_ERROR_NO_OBJECT;

	Restore();
	m_type = 0;
	m_length = math::v2(0.0f, 0.0f);
	return 0;
}

void TextAutoScroll::GetFriendBounds(math::v2 &bounds)
{
	bounds = math::v2(0.0f, 0.0f);
	if (m_text == NULL || m_text_obj == NULL)
		return;

	m_text_obj->GetBounds(bounds);

	Widget *parent = m_text->m_parent;

	if (parent == NULL)
		return;

	for (vector<IDParam>::iterator it = m_friends.begin(); it != m_friends.end(); ++it) {
		graph::TextObj *text_obj = GetFriendTextObj(parent, *it);
		math::v2 friend_bounds;

		if (text_obj == NULL)
			continue;

		text_obj->GetBounds(friend_bounds);
		switch (m_type) {
		case Text::SCROLL_TYPE_HORIZONTAL:
		case Text::SCROLL_TYPE_HORIZONTAL_ONCE:
			if (friend_bounds.extract_x() > bounds.extract_x())
				bounds = friend_bounds;
			break;
		case Text::SCROLL_TYPE_VERTICAL:
		case Text::SCROLL_TYPE_VERTICAL_ONCE:
			if (friend_bounds.extract_y() > bounds.extract_y())
				bounds = friend_bounds;
			break;
		}
	}
}

bool TextAutoScroll::IsFriendReady()
{
	bool ready = true;

	if (m_text != NULL) {
		Widget *parent = m_text->m_parent;

		if (parent != NULL) {
			for (vector<IDParam>::iterator it = m_friends.begin();
			     it != m_friends.end(); ++it) {
				Text *text = FindText(parent, *it);

				if (text != NULL && text->m_hold_count <= 0 &&
				    text->IsScrolling()) {
					graph::TextObj *text_obj = GetTextObj(text);

					if (text_obj != NULL && text_obj->m_flags.m_prepare)
						ready = false;
				}
			}
		}
	}
	return ready;
}

bool TextAutoScroll::IsRunning()
{
	return m_type != 0;
}

int32_t TextAutoScroll::Update()
{
	graph::TextLayoutAttribute condense;
	bool condensed;
	bool ellipsis;
	bool word_wrap;
	math::v2 bounds;
	math::v2 pos;
	float length;
	float progress;

	if (IsAnimRunning(Text::ANIM_AUTO_SCROLL_FADE))
		m_text->KillAnim(Text::ANIM_AUTO_SCROLL_FADE);

	if (m_text == NULL || m_text_obj == NULL || m_type == 0)
		return 0;
	if (IsAnimRunning(Text::ANIM_AUTO_SCROLL))
		return 0;

	switch (m_type) {
	case Text::SCROLL_TYPE_HORIZONTAL:
	case Text::SCROLL_TYPE_HORIZONTAL_ONCE:
	default:
		condense = graph::TextLayoutAttribute_CondenseX;
		break;
	case Text::SCROLL_TYPE_VERTICAL:
	case Text::SCROLL_TYPE_VERTICAL_ONCE:
		condense = graph::TextLayoutAttribute_CondenseY;
		break;
	}
	condensed = false;
	m_text_obj->GetLayoutAttribute(condense, condensed);
	if (condensed) {
		Stop();
		return 0;
	}

	m_text_obj->GetLayoutAttribute(graph::TextLayoutAttribute_Ellipsis, ellipsis);
	if (ellipsis) {
		m_ellipsis = true;
		m_text_obj->SetLayoutAttribute(graph::TextLayoutAttribute_Ellipsis, false);
	}
	switch (m_type) {
	case Text::SCROLL_TYPE_VERTICAL:
	case Text::SCROLL_TYPE_VERTICAL_ONCE:
		break;
	default:
		m_text_obj->GetLayoutAttribute(graph::TextLayoutAttribute_WordWrap, word_wrap);
		if (word_wrap) {
			m_word_wrap = true;
			m_text_obj->SetLayoutAttribute(graph::TextLayoutAttribute_WordWrap, false);
		}
		break;
	}

	if (!m_text_obj->m_flags.m_prepare && IsFriendReady()) {
		length = 0.0f;
		progress = 0.0f;
		m_text_obj->GetBounds(bounds);
		m_text_obj->GetLayoutAttribute(graph::TextLayoutAttribute_VisiblePos, pos);

		math::v3 size = m_text_obj->m_size;

		switch (m_type) {
		case Text::SCROLL_TYPE_HORIZONTAL:
		case Text::SCROLL_TYPE_HORIZONTAL_ONCE:
			if (bounds.extract_x() > size.extract_x()) {
				GetFriendBounds(bounds);
				if (m_length == math::v2(0.0f, 0.0f)) {
					m_length_new = true;
					m_length = math::v2(bounds.extract_x(), 0.0f);
				} else {
					bounds = m_length;
					m_length_new = false;
				}
				length = bounds.extract_x();
				if (m_length.extract_x() > 0.0f)
					progress = pos.extract_x() / m_length.extract_x();
			}
			break;
		case Text::SCROLL_TYPE_VERTICAL:
		case Text::SCROLL_TYPE_VERTICAL_ONCE:
			if (bounds.extract_y() > size.extract_y()) {
				GetFriendBounds(bounds);
				if (m_length == math::v2(0.0f, 0.0f)) {
					m_length_new = true;
					m_length = math::v2(0.0f, bounds.extract_y());
				} else {
					bounds = m_length;
					m_length_new = false;
				}
				length = bounds.extract_y();
				if (m_length.extract_y() > 0.0f)
					progress = pos.extract_y() / m_length.extract_y();
			}
			break;
		}

		if (length > 0.0f && m_speed > 0.0f) {
			float time = length / m_speed * 1000.0f;
			Timer *scroll = new Timer(time);

			/* the scroll goes on from where the text is */
			if (progress > 0.0f) {
				scroll->m_curr = progress * time * 0.001f;
			} else {
				SetAlpha(1.0f);
				ResetPos();
			}

			if (m_type == Text::SCROLL_TYPE_HORIZONTAL ||
			    m_type == Text::SCROLL_TYPE_VERTICAL) {
				AddAnim(m_text, ANIM_AUTO_SCROLL_LOOP, new Timer(2000.0f), NULL,
					NULL);
				AddAnim(m_text, ANIM_AUTO_SCROLL_LOOP, scroll, OnScrollTick, this);
				AddAnim(m_text, ANIM_AUTO_SCROLL_LOOP, new Timer(1000.0f), NULL,
					NULL);
				AddAnim(m_text, ANIM_AUTO_SCROLL_LOOP, new Timer(1000.0f),
					OnFadeTick, this);
			} else {
				if (progress == 0.0f)
					AddAnim(m_text, Text::ANIM_AUTO_SCROLL, new Timer(500.0f),
						NULL, NULL);
				AddAnim(m_text, Text::ANIM_AUTO_SCROLL, scroll, OnScrollTick,
					this);
				AddAnim(m_text, Text::ANIM_AUTO_SCROLL, new Timer(1000.0f),
					OnFadeTick, this);
			}
		} else if (IsOnce()) {
			Stop();
		}
	} else {
		/* the texts still being laid out: the scroll starts once they are */
		Handler *handler = m_text;

		if (handler->m_map_t_listener.find(Handler::CB_INTERVAL | TIMER_WAIT_LAYOUT) ==
		    handler->m_map_t_listener.end())
			handler->SetIntervalEvent(TIMER_WAIT_LAYOUT, 0.0f, OnLayoutReady, this);
	}
	return 0;
}

int32_t TextAutoScroll::Start(Text::ScrollType type)
{
	if (m_text_obj == NULL)
		return SCE_PAF_ERROR_NO_OBJECT;

	Restore();
	m_type = type;
	Update();
	return 0;
}

int32_t TextAutoScroll::GetSpeedByPos(const math::v2 &pos, float &speed, bool set_origin,
				      bool reverse)
{
	int32_t ret = SCE_PAF_ERROR_FAILED;
	float range;
	float dist;
	float percent;

	if (set_origin)
		m_origin = math::v2(m_text->GetWorldMatrix().w().getImpl().getXY());

	if (m_text == NULL || m_type == 0)
		return ret;

	range = 30.0f;
	switch (m_type) {
	case Text::SCROLL_TYPE_HORIZONTAL:
	case Text::SCROLL_TYPE_HORIZONTAL_ONCE:
		if (set_origin)
			range = m_text->GetSize(0)->extract_x() * 100.0f / 300.0f;
		dist = pos.extract_x();
		break;
	case Text::SCROLL_TYPE_VERTICAL:
	case Text::SCROLL_TYPE_VERTICAL_ONCE:
		if (set_origin)
			range = m_text->GetSize(0)->extract_y() * 100.0f / 300.0f;
		dist = pos.extract_y();
		break;
	default:
		return ret;
	}
	/* Sony's takes the origin's x off, for a vertical scroll too */
	dist -= m_origin.extract_x();

	if (reverse)
		dist = -dist;
	percent = (1.0f - dist / range) * 100.0f;
	if (percent < 50.0f)
		percent = 50.0f;
	else if (percent > 300.0f)
		percent = 300.0f;
	ret = 0;
	speed = sce_paf_floor(percent / 20.0f) * 20.0;
	return ret;
}

int32_t TextAutoScroll::SetSpeed(float speed, int32_t mode)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL && m_text_obj != NULL) {
		if (m_speed == speed)
			return 0;

		if (mode == 1)
			Restore();
		else
			m_text->KillAnim(Text::ANIM_AUTO_SCROLL);
		m_speed = speed;
		Update();
		ret = 0;
	}
	return ret;
}

int32_t TextAutoScroll::GetSpeed(float &speed)
{
	speed = m_speed;
	return 0;
}

int32_t TextAutoScroll::GetDefaultSpeed(float &speed, Text::ScrollType type)
{
	speed = 60.0f;
	switch (type) {
	case Text::SCROLL_TYPE_HORIZONTAL:
	case Text::SCROLL_TYPE_VERTICAL:
		speed = 60.0f;
		break;
	case Text::SCROLL_TYPE_HORIZONTAL_ONCE:
	case Text::SCROLL_TYPE_VERTICAL_ONCE:
		speed = 100.0f;
		break;
	default:
		return SCE_PAF_ERROR_INVALID_ARGUMENT;
	}
	return 0;
}

/* a friend of the ID's hash (inlined in Sony's code, name ours) */
static inline __attribute__((always_inline)) bool HasFriend(vector<IDParam> &friends,
							    IDParam id)
{
	for (vector<IDParam>::iterator it = friends.begin(); it != friends.end(); ++it) {
		if (id.GetIDHash() == it->GetIDHash())
			return true;
	}
	return false;
}

int32_t TextAutoScroll::AddFriend(const IDParam &id)
{
	if (HasFriend(m_friends, id))
		return SCE_PAF_ERROR_INVALID_ARGUMENT;

	m_friends.push_back(id);
	return 0;
}

int32_t TextAutoScroll::RemoveFriend(const IDParam &id)
{
	for (vector<IDParam>::iterator it = m_friends.begin(); it != m_friends.end(); ++it) {
		if (id.GetIDHash() == it->GetIDHash()) {
			m_friends.erase(it);
			return 0;
		}
	}
	return SCE_PAF_ERROR_INVALID_ARGUMENT;
}

void TextAutoScroll::OnLayoutReady(int32_t type, Handler *self, Event *e, void *data)
{
	TextAutoScroll *scroll = (TextAutoScroll *)data;

	if (scroll->m_text_obj == NULL || scroll->m_text_obj->m_flags.m_prepare ||
	    !scroll->IsFriendReady())
		return;

	scroll->Update();
	self->KillIntervalEvent(TIMER_WAIT_LAYOUT);
}

void TextAutoScroll::OnFadeTick(Timer *t, AnimListener *listener, void *data1, void *data2,
				void *data3)
{
	TextAutoScroll *scroll = (TextAutoScroll *)data2;

	scroll->SetAlpha(t->m_mapped_x);
	if (scroll->IsOnce())
		scroll->Stop();
}

void TextAutoScroll::OnScrollTick(Timer *t, AnimListener *listener, void *data1, void *data2,
				  void *data3)
{
	TextAutoScroll *scroll = (TextAutoScroll *)data2;

	if (t->m_mapped_x < 1.0f) {
		scroll->m_text_obj->SetLayoutAttribute(graph::TextLayoutAttribute_VisiblePos,
						       scroll->m_length * t->m_mapped_x);
	} else {
		scroll->SetAlpha(0.0f);
		scroll->ResetPos();
	}
}

void TextAutoScroll::OnFadeBackTick(Timer *t, AnimListener *listener, void *data1, void *data2,
				    void *data3)
{
	TextAutoScroll *scroll = (TextAutoScroll *)data2;

	scroll->SetAlpha(t->m_mapped_x);
	if (scroll->IsRunning())
		scroll->Stop();
}

}

/* Sony's AddFriend calls the vector's push_back, which SNC inlines here: the copy Sony's build
   has out of line (0x81261e94 in TOOL) */
template int vector<IDParam>::push_back(const IDParam &val);

}
