/*
 * indicator_text_scroll.cpp: a notice's title scrolled while its item is focused: the text shown
 * a while, scrolled by its overflow, faded out and in again at its start, then scrolled again
 * or put back, ticked by the scene's draw function (0x81180848-0x81180FD8). The file's and all
 * names here are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <widget/core/widget.h>
#include <widget/w_scene.h>
#include <graphics/drawobj/text_obj.h>
#include <graphics/text/text_layout.h>
#include <graphics/text/text_style.h>
/* Sony built this file without the keys' header, so its static initializer has no keys */
#define LSDB_KEYS_H
#include "indicator.h"

using namespace paf;

/* the scroll's phases */
#define INDICATOR_TEXT_SCROLL_WAIT	0	/* shown before the first scroll */
#define INDICATOR_TEXT_SCROLL_REST	1	/* shown before the next scroll */
#define INDICATOR_TEXT_SCROLL_MOVE	2	/* scrolled by its overflow */
#define INDICATOR_TEXT_SCROLL_HOLD	3	/* gone at the scroll's end */
#define INDICATOR_TEXT_SCROLL_FADE	4	/* faded in at its start */

/* how long each phase lasts, in milliseconds, and how far it scrolls a second */
#define INDICATOR_TEXT_SCROLL_WAIT_TIME	500.0f
#define INDICATOR_TEXT_SCROLL_REST_TIME	1000.0f
#define INDICATOR_TEXT_SCROLL_HOLD_TIME	1000.0f
#define INDICATOR_TEXT_SCROLL_FADE_TIME	1000.0f
#define INDICATOR_TEXT_SCROLL_SPEED	100.0f

/* the scroll the scene's draw function ticks, as its draw argument */
class IndicatorTextScrollDraw {
public:
	__attribute__((noinline)) IndicatorTextScrollDraw() : m_scroll(NULL)
	{
	}

	IndicatorTextScroll *m_scroll;
	float m_alpha;
};

/* (0x81566B28) */
static IndicatorTextScrollDraw s_draw;

/* the scroll's tick taken off the scene of the widget */
static inline void indicatorTextScrollUnhook(ui::Widget *widget)
{
	ui::Scene *scene = widget->m_scene;
	ui::Scene::DrawArg arg;

	arg.m_data = NULL;
	scene->m_draw_func = NULL;
	scene->m_draw_arg = arg;
	s_draw.m_scroll = NULL;
}

/* the text's colour with the alpha given */
static inline void indicatorTextScrollSetAlpha(graph::TextObj *text, float alpha)
{
	math::v4 color = text->GetColor();

	color.set_w(alpha);
	text->SetColor(color);
}

/* (0x81180848) */
IndicatorTextScroll::IndicatorTextScroll(ui::Scene *scene)
	: m_direction(0), m_distance(0.0f, 0.0f), m_scrolling(false), m_phase(0),
	  m_elapsed(0.0f), m_duration(0.0f), m_text(NULL), m_ellipsis(false),
	  m_wordWrap(false), m_loop(false), m_scene(scene)
{
}

/* (0x8118087C) */
IndicatorTextScroll::~IndicatorTextScroll()
{
	if (s_draw.m_scroll == this)
		indicatorTextScrollUnhook(m_scene);
	m_scene = NULL;
}

/* (0x811808F0) */
int IndicatorTextScroll::GetTextBounds(math::v2 &bounds)
{
	graph::TextObj *text = m_text;
	common::SharedPtr<graph::TextLayout> layout(new graph::TextLayout(NULL, NULL));

	layout->SetLayoutAttribute(graph::TextLayoutAttribute_AdjustX, true);
	layout->SetLayoutAttribute(graph::TextLayoutAttribute_AdjustY, true);
	layout->SetLayoutAttribute(graph::TextLayoutAttribute_Ellipsis, false);
	layout->SetLayoutAttribute(graph::TextLayoutAttribute_WordWrap, false);

	graph::TextStyle style;

	text->GetStyle(0, style);
	layout->SetStyle(0, 0, style);

	wstring str;

	text->GetString(str);
	layout->SetString(str);
	layout->Update(NULL, NULL, NULL);
	return layout->GetBounds(bounds);
}

/* (0x81180A58) */
void IndicatorTextScroll::Reset(int direction, bool loop)
{
	m_direction = direction;
	m_scrolling = false;
	m_phase = INDICATOR_TEXT_SCROLL_WAIT;
	m_distance = math::v2(0.0f, 0.0f);
	m_elapsed = 0.0f;
	m_duration = 0.0f;
	m_loop = loop;

	graph::TextObj *text = m_text;

	if (direction == 0) {
		text->SetLayoutAttribute(graph::TextLayoutAttribute_VisiblePos, math::v2(0.0f, 0.0f));
		indicatorTextScrollSetAlpha(text, 1.0f);
	} else {
		math::v2 size(text->m_size.extract_x(), text->m_size.extract_y());
		float distance = 0.0f;
		math::v2 bounds;

		GetTextBounds(bounds);
		if (m_direction == 1) {
			if (size.extract_x() > 0.0f && bounds.extract_x() > size.extract_x()) {
				m_distance = math::v2(bounds.extract_x(), 0.0f);
				distance = bounds.extract_x();
			}
		} else if (m_direction == 2) {
			if (size.extract_y() > 0.0f && bounds.extract_y() > size.extract_y()) {
				m_distance = math::v2(0.0f, bounds.extract_y());
				distance = bounds.extract_y();
			}
		}
		if (distance > 0.0f) {
			m_scrolling = true;
			indicatorTextScrollSetAlpha(text, 1.0f);
			text->SetLayoutAttribute(graph::TextLayoutAttribute_VisiblePos,
						 math::v2(0.0f, 0.0f));
			m_duration = distance / INDICATOR_TEXT_SCROLL_SPEED * 1000.0f;
		}
	}
}

/* (0x81180C24) */
void IndicatorTextScroll::Stop(graph::TextObj *text)
{
	if (text != m_text)
		return;
	if (s_draw.m_scroll == this)
		indicatorTextScrollUnhook(m_scene);
	Reset(0, false);
	text->SetLayoutAttribute(graph::TextLayoutAttribute_Ellipsis, m_ellipsis);
	text->SetLayoutAttribute(graph::TextLayoutAttribute_WordWrap, m_wordWrap);
	m_ellipsis = false;
	m_wordWrap = false;
	m_text = NULL;
}

static void indicatorTextScrollOnDraw(const ui::Scene::DrawArg &arg);

/* (0x81180C84) */
void IndicatorTextScroll::Start(int unused, graph::TextObj *text)
{
	if (m_text != NULL)
		Stop(m_text);
	m_text = text;
	text->GetLayoutAttribute(graph::TextLayoutAttribute_Ellipsis, m_ellipsis);
	text->GetLayoutAttribute(graph::TextLayoutAttribute_WordWrap, m_wordWrap);
	Reset(1, false);

	ui::Scene::DrawArg arg;

	arg.m_data = this;
	arg.m_alpha = 0.0f;
	m_scene->m_draw_func = indicatorTextScrollOnDraw;
	m_scene->m_draw_arg = arg;
	s_draw.m_scroll = this;
	s_draw.m_alpha = 0.0f;
}

/* (0x81180D32) */
void IndicatorTextScroll::Update()
{
	if (!m_scrolling || m_direction == 0)
		return;

	graph::TextObj *text = m_text;

	m_elapsed += Timer::s_tick_unit * Timer::s_tick_interval * 1000.0f;
	switch (m_phase) {
	case INDICATOR_TEXT_SCROLL_WAIT:
		if (m_elapsed > INDICATOR_TEXT_SCROLL_WAIT_TIME) {
			m_phase = INDICATOR_TEXT_SCROLL_MOVE;
			m_elapsed = 0.0f;
			text->SetLayoutAttribute(graph::TextLayoutAttribute_Ellipsis, false);
			text->SetLayoutAttribute(graph::TextLayoutAttribute_WordWrap, false);
		}
		break;
	case INDICATOR_TEXT_SCROLL_REST:
		if (m_elapsed > INDICATOR_TEXT_SCROLL_REST_TIME) {
			m_phase = INDICATOR_TEXT_SCROLL_MOVE;
			m_elapsed = 0.0f;
		}
		break;
	case INDICATOR_TEXT_SCROLL_MOVE: {
		float rate = m_elapsed / m_duration;

		if (rate < 1.0f) {
			text->SetLayoutAttribute(graph::TextLayoutAttribute_VisiblePos,
						 m_distance * rate);
		} else {
			indicatorTextScrollSetAlpha(text, 0.0f);
			text->SetLayoutAttribute(graph::TextLayoutAttribute_VisiblePos,
						 math::v2(0.0f, 0.0f));
			m_phase = INDICATOR_TEXT_SCROLL_HOLD;
			m_elapsed = 0.0f;
		}
		break;
	}
	case INDICATOR_TEXT_SCROLL_HOLD:
		if (m_elapsed > INDICATOR_TEXT_SCROLL_HOLD_TIME) {
			m_phase = INDICATOR_TEXT_SCROLL_FADE;
			m_elapsed = 0.0f;
			if (!m_loop) {
				text->SetLayoutAttribute(graph::TextLayoutAttribute_Ellipsis,
							 m_ellipsis);
				text->SetLayoutAttribute(graph::TextLayoutAttribute_WordWrap,
							 m_wordWrap);
			}
		}
		break;
	case INDICATOR_TEXT_SCROLL_FADE: {
		float rate = m_elapsed / INDICATOR_TEXT_SCROLL_FADE_TIME;

		if (rate < 1.0f) {
			indicatorTextScrollSetAlpha(text, rate);
		} else {
			indicatorTextScrollSetAlpha(text, 1.0f);
			m_phase = INDICATOR_TEXT_SCROLL_REST;
			m_elapsed = 0.0f;
			if (!m_loop)
				Stop(m_text);
		}
		break;
	}
	}
}

/* the scene drawn: the scroll its argument names ticked (0x81180FBC) */
static void indicatorTextScrollOnDraw(const ui::Scene::DrawArg &arg)
{
	((IndicatorTextScroll *)arg.m_data)->Update();
}
