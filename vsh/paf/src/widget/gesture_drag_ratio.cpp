/*
 * GestureDragRatio: how far a drag has gone between a near and a far distance (see
 * widget/gesture_drag.h).
 */
#include <stddef.h>
#include <stdint.h>
#include <float.h>
#include <widget/gesture_drag.h>

using namespace paf::ui;

GestureDragRatio::GestureDragRatio(int32_t axis, int32_t type, float width, float height,
				   float scale, float far_x, float far_y)
	: GestureDrag(axis, type), m_enabled(false), m_width(0.0f), m_height(0.0f), m_param(NULL),
	  m_scale(0.0f), m_far_x(far_x), m_far_y(far_y)
{
	if (width > FLT_EPSILON && height > FLT_EPSILON && scale > FLT_EPSILON) {
		m_width = width;
		m_height = height;
		m_scale = scale;
		m_enabled = true;
	}

	static GestureDragRatioParam s_param;
	m_param = &s_param;
}

GestureDragRatio::~GestureDragRatio()
{
}

int32_t GestureDragRatio::GetRatio(float *ratio_x, float *ratio_y)
{
	if (!m_enabled || !IsActive()) {
		*ratio_y = 0.0f;
		*ratio_x = 0.0f;
		return 0;
	}

	float near_x = m_param->m_near_x * m_scale;
	float near_y = m_param->m_near_y * m_scale;
	float far_x = m_far_x * m_scale;
	float far_y = m_far_y * m_scale;

	if (near_x > m_width)
		near_x = m_width;
	if (near_y > m_height)
		near_y = m_height;
	if (near_x > far_x)
		far_x = near_x;
	if (near_y > far_y)
		far_y = near_y;
	if (far_x > m_width)
		far_x = m_width;
	if (far_y > m_height)
		far_y = m_height;

	float dx = __builtin_fabsf(GetX() - GetStartX());
	float dy = __builtin_fabsf(GetY() - GetStartY());

	if (dx < near_x) {
		*ratio_x = 0.0f;
	} else if (dx < far_x) {
		float t = (dx - near_x) / (far_x - near_x);
		*ratio_x = t * t * t;
	} else {
		*ratio_x = 1.0f;
	}

	if (dy < near_y) {
		*ratio_y = 0.0f;
	} else if (dy < far_y) {
		float t = (dy - near_y) / (far_y - near_y);
		*ratio_y = t * t * t;
	} else {
		*ratio_y = 1.0f;
	}

	if (GetX() - GetStartX() < 0.0f)
		*ratio_x = -*ratio_x;
	if (GetY() - GetStartY() < 0.0f)
		*ratio_y = -*ratio_y;

	return 0;
}

bool GestureDragRatio::SetFarX(float far_x)
{
	if (far_x >= 0.0f) {
		m_far_x = far_x;
		return true;
	}

	return false;
}
