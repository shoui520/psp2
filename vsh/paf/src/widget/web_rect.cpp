/*
 * paf::ui::WebRect implementation for 2014 PAF web/map widgets.
 */
#include <widget/web_rect.h>

namespace paf {
namespace ui {

WebRect::WebRect()
	: m_left(0.0f), m_right(0.0f), m_top(0.0f), m_bottom(0.0f)
{
}

WebRect::~WebRect()
{
}

void WebRect::Set(float left, float bottom, float right, float top)
{
	m_left = left;
	m_bottom = bottom;
	m_right = right;
	m_top = top;
}

void WebRect::SetCenterSize(float center_x, float center_y, float width, float height)
{
	float half_width = width * 0.5f;
	float half_height = height * 0.5f;

	m_left = center_x - half_width;
	m_bottom = center_y - half_height;
	m_right = center_x + half_width;
	m_top = center_y + half_height;
}

void WebRect::Intersect(const WebRect &other)
{
	float left = other.m_left;
	float bottom = other.m_bottom;
	float right = other.m_right;
	float top = other.m_top;

	if (m_left > left)
		left = m_left;
	if (m_bottom > bottom)
		bottom = m_bottom;
	if (m_right < right)
		right = m_right;
	if (m_top < top)
		top = m_top;

	if (left >= right || top <= bottom) {
		left = 0.0f;
		bottom = 0.0f;
		right = 0.0f;
		top = 0.0f;
	}

	m_left = left;
	m_bottom = bottom;
	m_right = right;
	m_top = top;
}

void WebRect::ClampMin(const WebRect &bounds)
{
	float left = m_left;
	float bottom = m_bottom;
	float width = m_right - m_left;
	float height = m_top - m_bottom;

	if (left < bounds.m_left)
		left = bounds.m_left;
	if (bottom < bounds.m_bottom)
		bottom = bounds.m_bottom;

	m_left = left;
	m_bottom = bottom;
	m_right = left + width;
	m_top = bottom + height;
}

void WebRect::ClampInside(const WebRect &bounds)
{
	float left = m_left;
	float bottom = m_bottom;
	float width = m_right - m_left;
	float height = m_top - m_bottom;

	if (left < bounds.m_left) {
		left = bounds.m_left;
	} else if (bounds.m_right < m_right) {
		left = bounds.m_right - width;
	}

	if (bottom > bounds.m_bottom) {
		bottom = bounds.m_bottom;
	} else if (bounds.m_top > m_top) {
		bottom = bounds.m_top - height;
	}

	m_left = left;
	m_bottom = bottom;
	m_right = left + width;
	m_top = bottom + height;
}

bool WebRect::ApproxEquals(const WebRect &other) const
{
	int32_t left_scaled = (int32_t)(m_left * 100.0f);
	int32_t bottom_scaled = (int32_t)(m_bottom * 100.0f);
	int32_t right_scaled = (int32_t)(m_right * 100.0f);
	int32_t top_scaled = (int32_t)(m_top * 100.0f);

	if (left_scaled != (int32_t)(other.m_left * 100.0f))
		return false;
	if (bottom_scaled != (int32_t)(other.m_bottom * 100.0f))
		return false;
	if (right_scaled != (int32_t)(other.m_right * 100.0f))
		return false;
	return top_scaled == (int32_t)(other.m_top * 100.0f);
}

}
}
