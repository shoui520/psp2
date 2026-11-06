/*
 * GestureDrag: where a drag started and where it is (see widget/gesture_drag.h).
 */
#include <stddef.h>
#include <stdint.h>
#include <widget/gesture_drag.h>

using namespace paf::ui;

GestureDrag::GestureDrag(int32_t axis, int32_t type)
	: m_axis(axis), m_type(type), m_active(false), m_start_x(0.0f), m_start_y(0.0f), m_x(0.0f),
	  m_y(0.0f), m_min_x(0.0f), m_min_y(0.0f), m_max_x(0.0f), m_max_y(0.0f)
{
}

GestureDrag::~GestureDrag()
{
}

bool GestureDrag::Begin(float start_x, float start_y, float x, float y)
{
	m_start_x = start_x;
	m_start_y = start_y;
	m_x = x;
	m_y = y;

	switch (m_axis) {
	case AXIS_X:
		m_y = start_y;
		break;
	case AXIS_Y:
		m_x = start_x;
		break;
	}

	if (m_type == TYPE_RANGE) {
		m_min_x = start_x < m_x ? start_x : m_x;
		m_max_x = start_x < m_x ? m_x : start_x;
		m_min_y = start_y < m_y ? start_y : m_y;
		m_max_y = start_y < m_y ? m_y : start_y;
	}

	m_active = true;
	return true;
}

bool GestureDrag::Move(float x, float y)
{
	if (!m_active)
		return false;

	switch (m_axis) {
	case AXIS_X:
		m_x = x;
		break;
	case AXIS_Y:
		m_y = y;
		break;
	case AXIS_XY:
		m_x = x;
		m_y = y;
		break;
	}

	if (m_type == TYPE_RANGE) {
		/* the start point is pulled a tenth of the way toward the far end of the range */
		float target_x, target_y;

		if (m_x > m_max_x) {
			m_max_x = m_x;
			target_x = m_min_x;
		} else if (m_x < m_min_x) {
			m_min_x = m_x;
			target_x = m_max_x;
		} else {
			target_x = (m_min_x + m_max_x) - m_x;
		}
		float start_x = m_start_x - (m_start_x - target_x) * 0.1f;

		if (m_y > m_max_y) {
			m_max_y = m_y;
			target_y = m_min_y;
		} else if (m_y < m_min_y) {
			m_min_y = m_y;
			target_y = m_max_y;
		} else {
			target_y = (m_min_y + m_max_y) - m_y;
		}
		float start_y = m_start_y - (m_start_y - target_y) * 0.1f;

		switch (m_axis) {
		case AXIS_X:
			m_start_x = start_x;
			break;
		case AXIS_Y:
			m_start_y = start_y;
			break;
		case AXIS_XY:
			m_start_x = start_x;
			m_start_y = start_y;
			break;
		}
	}

	return true;
}

bool GestureDrag::End(float x, float y)
{
	bool ret = Move(x, y);
	m_active = false;
	return ret;
}
