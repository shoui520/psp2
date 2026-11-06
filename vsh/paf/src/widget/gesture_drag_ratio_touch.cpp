/*
 * GestureDragRatioTouch: a GestureDragRatio with its own parameter (see widget/gesture_drag.h).
 */
#include <stddef.h>
#include <stdint.h>
#include <widget/gesture_drag.h>

using namespace paf::ui;

GestureDragRatioTouch::GestureDragRatioTouch(int32_t axis, int32_t type, float width,
					     float height, float scale, float far_x, float far_y)
	: GestureDragRatio(axis, type, width, height, scale, far_x, far_y)
{
	static GestureDragRatioTouchParam s_param;
	m_param = &s_param;
}

GestureDragRatioTouch::~GestureDragRatioTouch()
{
}
