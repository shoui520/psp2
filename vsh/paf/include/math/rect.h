/*
 * paf::math::rect: a rectangle by its centre and size, y up. The setters keep
 * the size at 0 or more.
 */
#ifndef PAF_MATH_RECT_H
#define PAF_MATH_RECT_H

#include <math/ngp/v2.h>
#include <math/ngp/v4.h>

namespace paf {
namespace math {

class rect {
public:
	static rect _0000();
	static rect _0011();

	rect();
	rect(const rect &obj);
	rect(const v2 &center, const v2 &size);
	rect(float xvalue, float yvalue, float wvalue, float hvalue);

	void set(const v2 &center, const v2 &size);
	void set(float xvalue, float yvalue, float wvalue, float hvalue);
	void set(const rect &obj);
	void set_center(float xvalue, float yvalue);
	void set_center(const v2 &xyvalue);
	void set_size(float wvalue, float hvalue);
	void set_size(const v2 &whvalue);
	void set_center_x(float xvalue);
	void set_center_y(float yvalue);
	void set_width(float wvalue);
	void set_height(float hvalue);
	void set_left(float xvalue);
	void set_right(float xvalue);
	void set_top(float yvalue);
	void set_bottom(float yvalue);

	void move(float xvalue, float yvalue);
	void move(const v2 &xyvalue);
	void align_left(float xvalue);
	void align_right(float xvalue);
	void align_top(float yvalue);
	void align_bottom(float yvalue);

	bool hit(const rect &obj);
	bool is_in_rect(const rect &obj);
	bool is_point_in_rect(float xvalue, float yvalue);
	bool is_point_in_rect(const v2 &xyvalue);

	void clear();
	rect &operator=(const rect &rhs);

	rect get() const;
	v2 center() const;
	v2 size() const;
	v4 get_xywh() const;
	v4 get_LTRB() const;
	v4 get_TLBR() const;
	rect convert(v2 origin, v2 direction, v2 scale) const;

	float center_x() const;
	float center_y() const;
	float width() const;
	float height() const;
	float left() const;
	float right() const;
	float top() const;
	float bottom() const;

	v2 left_top() const;
	v2 left_bottom() const;
	v2 right_top() const;
	v2 right_bottom() const;

	float get_area() const;

	bool operator==(const rect &rhs) const;
	bool operator!=(const rect &rhs) const;

private:
	float m_x_center;
	float m_y_center;
	float m_w;
	float m_h;
};

}
}

/* No Sony name for these exports: the overlap and the bounding box of two rectangles. */
extern "C" paf::math::rect ScePafMisc_B2795FE2(const paf::math::rect &obj, const paf::math::rect &other);
extern "C" paf::math::rect ScePafMisc_8C14E735(const paf::math::rect &obj, const paf::math::rect &other);

#endif
