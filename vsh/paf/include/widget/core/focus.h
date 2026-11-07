/*
 * paf::ui::Widget's focus requests: how SetFocus moves the focus to a widget, and the search
 * SetFocusOption makes for the widget the focus goes to. Names ours, except SetFocusInfo (the
 * NID hash of Widget::SetFocus).
 */
#ifndef PAF_WIDGET_CORE_FOCUS_H
#define PAF_WIDGET_CORE_FOCUS_H

#include <stdint.h>
#include <math/ngp/v2.h>
#include <math/rect.h>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

/* how SetFocus moves the focus to the widget (Sony's name from the export's NID hash;
   members' names ours): +4 and +16 are what SetFocus reads */
class Widget::SetFocusInfo {
public:
	int32_t m_mode;
	/* the focus moved with its notifications */
	bool m_notify;
	/* the direction the focus came from, as a unit vector */
	math::v2 m_direction;
	/* kept by the focus root (its +36) */
	bool m_keep;
};

/* a search for the widget the focus goes to, from a widget's place in a direction
   (SetFocusOption and ScePafWidget_EA117CF8; names ours) */
class Widget::FocusSearchInfo {
public:
	explicit FocusSearchInfo(int32_t dir)
		: m_dir(dir), m_distance(0), m_option(0), m_origin(NULL), m_result(NULL),
		  m_pos(0.0f, 0.0f), m_pos_zw(0.0f, 0.0f), m_size(0.0f, 0.0f),
		  m_size_zw(0.0f, 0.0f), m_enabled(true), m_search_outside(true), m_ratio(0.4f)
	{
	}

	/* the direction, 255 for none */
	int32_t m_dir;
	/* the best distance found, 0x7FFFFFFF where the focus root takes it */
	int32_t m_distance;
	int32_t m_option;
	/* the widget searched from, and the widget found */
	Widget *m_origin;
	Widget *m_result;
	/* the origin's position and size (x, y and z, w) */
	math::v2 m_pos;
	math::v2 m_pos_zw;
	math::v2 m_size;
	math::v2 m_size_zw;
	/* both set by default; a composite button's search among its own parts clears the
	   second */
	bool m_enabled;
	bool m_search_outside;
	/* 0.4 by default */
	float m_ratio;
	/* the origin's focus search rectangle */
	math::rect m_rect;
};

}
}

extern "C" {
/* the search given the widget as its origin: its position, size and focus search rectangle
   (names ours) */
void ScePafWidget_EA117CF8(paf::ui::Widget::FocusSearchInfo *info, paf::ui::Widget *widget);
}

#endif
