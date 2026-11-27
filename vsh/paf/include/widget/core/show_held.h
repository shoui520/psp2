/*
 * paf::ui::IsShowHeld: whether a widget or a widget above it up to its scene holds its show.
 * Sony's binary has one shared copy (with the header-inline functions), called from
 * Widget::EventExecDispatch and Environment::UpdateWidgetGroups; the name is ours.
 */
#ifndef PAF_WIDGET_CORE_SHOW_HELD_H
#define PAF_WIDGET_CORE_SHOW_HELD_H

#include <stdint.h>
#include <widget/core/widget.h>
#include <widget/w_scene.h>

namespace paf {
namespace ui {

#ifdef PAF_BUILD_2014
inline __attribute__((always_inline)) int32_t IsShowHeld(Widget *widget)
{
	while (widget != NULL) {
		if (widget->m_show_hold > 0)
			return 1;
		if (static_cast<Widget *>(widget->m_scene) == widget)
			break;
		widget = widget->m_parent;
	}
	return 0;
}
#else
inline int32_t IsShowHeld(Widget *widget)
{
	if (widget->m_show_hold > 0)
		return 1;
	if (static_cast<Widget *>(widget->m_scene) != widget) {
		Widget *parent = widget->m_parent;

		if (parent != NULL)
			return IsShowHeld(parent);
	}
	return 0;
}
#endif

}
}

#endif
