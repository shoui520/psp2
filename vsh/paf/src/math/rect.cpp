/* paf::math::rect: setters, edges and tests of a centre-and-size rectangle. */
#include <math/rect.h>

namespace paf {
namespace math {

void rect::set_height(float hvalue)
{
	m_h = hvalue >= 0.0f ? hvalue : 0.0f;
}

void rect::set_width(float wvalue)
{
	m_w = wvalue >= 0.0f ? wvalue : 0.0f;
}

void rect::set_size(float wvalue, float hvalue)
{
	set_width(wvalue);
	set_height(hvalue);
}

void rect::set_center_y(float yvalue)
{
	m_y_center = yvalue;
}

void rect::set_center_x(float xvalue)
{
	m_x_center = xvalue;
}

void rect::set_center(float xvalue, float yvalue)
{
	m_x_center = xvalue;
	m_y_center = yvalue;
}

void rect::set(float xvalue, float yvalue, float wvalue, float hvalue)
{
	set_center(xvalue, yvalue);
	set_size(wvalue, hvalue);
}

rect::rect()
{
	m_x_center = 0.0f;
	m_y_center = 0.0f;
	m_h = 0.0f;
	m_w = 0.0f;
}

void rect::set(const rect &obj)
{
	set(obj.m_x_center, obj.m_y_center, obj.m_w, obj.m_h);
}

rect::rect(const rect &obj)
{
	set(obj);
}

void rect::set(const v2 &center, const v2 &size)
{
	set(center.extract_x(), center.extract_y(), size.extract_x(), size.extract_y());
}

rect::rect(const v2 &center, const v2 &size)
{
	set(center, size);
}

rect::rect(float xvalue, float yvalue, float wvalue, float hvalue)
{
	set(xvalue, yvalue, wvalue, hvalue);
}

void rect::clear()
{
	m_x_center = 0.0f;
	m_y_center = 0.0f;
	m_w = 0.0f;
	m_h = 0.0f;
}

rect rect::get() const
{
	return rect(*this);
}

v2 rect::center() const
{
	return v2(m_x_center, m_y_center);
}

v2 rect::size() const
{
	return v2(m_w, m_h);
}

v4 rect::get_xywh() const
{
	return v4(m_x_center, m_y_center, m_w, m_h);
}

float rect::bottom() const
{
	return m_y_center - m_h * 0.5f;
}

float rect::right() const
{
	return m_x_center + m_w * 0.5f;
}

float rect::top() const
{
	return m_y_center + m_h * 0.5f;
}

float rect::left() const
{
	return m_x_center - m_w * 0.5f;
}

v4 rect::get_LTRB() const
{
	return v4(left(), top(), right(), bottom());
}

v4 rect::get_TLBR() const
{
	return v4(top(), left(), bottom(), right());
}

float rect::center_x() const
{
	return m_x_center;
}

float rect::center_y() const
{
	return m_y_center;
}

float rect::width() const
{
	return m_w;
}

float rect::height() const
{
	return m_h;
}

v2 rect::left_top() const
{
	return v2(left(), top());
}

v2 rect::left_bottom() const
{
	return v2(left(), bottom());
}

v2 rect::right_top() const
{
	return v2(right(), top());
}

v2 rect::right_bottom() const
{
	return v2(right(), bottom());
}

float rect::get_area() const
{
	return m_w * m_h;
}

}
}

using paf::math::rect;

/* The overlap, empty (and placed on the nearer edges) when there is none. */
rect ScePafMisc_B2795FE2(const rect &obj, const rect &other)
{
	float objTop = obj.top();
	float top = other.top();
	float objLeft = obj.left();
	float objBottom = obj.bottom();
	float objRight = obj.right();
	float left = other.left();
	float bottom = other.bottom();
	float right = other.right();

	if (top > objTop)
		top = objTop;
	if (left < objLeft)
		left = objLeft;
	if (bottom < objBottom)
		bottom = objBottom;
	if (right > objRight)
		right = objRight;
	if (bottom > top)
		bottom = top;
	if (left > right)
		left = right;
	return rect((left + right) * 0.5f, (bottom + top) * 0.5f, right - left, top - bottom);
}

/* The smallest rectangle holding both. */
rect ScePafMisc_8C14E735(const rect &obj, const rect &other)
{
	float objTop = obj.top();
	float top = other.top();
	float objLeft = obj.left();
	float objBottom = obj.bottom();
	float objRight = obj.right();
	float left = other.left();
	float bottom = other.bottom();
	float right = other.right();

	if (top < objTop)
		top = objTop;
	if (left > objLeft)
		left = objLeft;
	if (bottom > objBottom)
		bottom = objBottom;
	if (right < objRight)
		right = objRight;
	return rect((left + right) * 0.5f, (bottom + top) * 0.5f, right - left, top - bottom);
}

namespace paf {
namespace math {

/* Into a frame at origin, scaled, with each axis flipped where direction is negative. */
rect rect::convert(v2 origin, v2 direction, v2 scale) const
{
	direction.getImpl().setX(direction.extract_x() < 0.0f ? -1.0f : 1.0f);
	direction.getImpl().setY(direction.extract_y() < 0.0f ? -1.0f : 1.0f);
	return rect((center() - origin) * scale * direction, size() * scale);
}

void rect::set_center(const v2 &xyvalue)
{
	set_center(xyvalue.extract_x(), xyvalue.extract_y());
}

void rect::set_size(const v2 &whvalue)
{
	set_size(whvalue.extract_x(), whvalue.extract_y());
}

void rect::set_left(float xvalue)
{
	float r = right();

	if (r - xvalue < 0.0f)
		xvalue = r;

	float w = r - xvalue;

	m_x_center = (xvalue + r) * 0.5f;
	set_width(w);
}

void rect::set_right(float xvalue)
{
	float l = left();

	if (xvalue - l < 0.0f)
		xvalue = l;
	m_x_center = (l + xvalue) * 0.5f;
	set_width(xvalue - l);
}

void rect::set_top(float yvalue)
{
	float b = bottom();

	if (yvalue - b < 0.0f)
		yvalue = b;
	m_y_center = (b + yvalue) * 0.5f;
	set_height(yvalue - b);
}

void rect::set_bottom(float yvalue)
{
	float t = top();

	if (t - yvalue < 0.0f)
		yvalue = t;

	float h = t - yvalue;

	m_y_center = (yvalue + t) * 0.5f;
	set_height(h);
}

void rect::move(float xvalue, float yvalue)
{
	m_x_center += xvalue;
	m_y_center += yvalue;
}

void rect::move(const v2 &xyvalue)
{
	move(xyvalue.extract_x(), xyvalue.extract_y());
}

void rect::align_left(float xvalue)
{
	m_x_center += xvalue - left();
}

void rect::align_right(float xvalue)
{
	m_x_center += xvalue - right();
}

void rect::align_top(float yvalue)
{
	m_y_center += yvalue - top();
}

void rect::align_bottom(float yvalue)
{
	m_y_center += yvalue - bottom();
}

bool rect::hit(const rect &obj)
{
	if (left() >= obj.right())
		return false;
	if (right() <= obj.left())
		return false;
	if (top() <= obj.bottom())
		return false;
	if (bottom() >= obj.top())
		return false;
	return true;
}

bool rect::is_in_rect(const rect &obj)
{
	if (left() < obj.left())
		return false;
	if (right() > obj.right())
		return false;
	if (top() > obj.top())
		return false;
	if (bottom() < obj.bottom())
		return false;
	return true;
}

bool rect::is_point_in_rect(float xvalue, float yvalue)
{
	return left() <= xvalue && xvalue <= right() && top() >= yvalue && yvalue >= bottom();
}

bool rect::is_point_in_rect(const v2 &xyvalue)
{
	return is_point_in_rect(xyvalue.extract_x(), xyvalue.extract_y());
}

/* Sony's operator== also answers whether the rectangles differ. */
bool rect::operator!=(const rect &rhs) const
{
	return !(m_x_center == rhs.m_x_center && m_y_center == rhs.m_y_center &&
		 m_w == rhs.m_w && m_h == rhs.m_h);
}

bool rect::operator==(const rect &rhs) const
{
	return !(m_x_center == rhs.m_x_center && m_y_center == rhs.m_y_center &&
		 m_w == rhs.m_w && m_h == rhs.m_h);
}

rect &rect::operator=(const rect &rhs)
{
	set(rhs);
	return *this;
}

rect rect::_0000()
{
	return rect(0.0f, 0.0f, 0.0f, 0.0f);
}

rect rect::_0011()
{
	return rect(0.0f, 0.0f, 1.0f, 1.0f);
}

}
}
