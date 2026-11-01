/* paf::math::v2: a 2D vector (vectormath's Vector2). */
#ifndef PAF_MATH_NGP_V2_H
#define PAF_MATH_NGP_V2_H

#include <vectormath.h>

namespace paf {
namespace math {

class v2 {
public:
	v2()
	{
	}

	v2(float x, float y) : _impl(x, y)
	{
	}

	v2(sce::Vectormath::Simd::Aos::Vector2 value) : _impl(value)
	{
	}

	float extract_x() const
	{
		return vget_lane_f32(_impl.get64(), 0);
	}

	float extract_y() const
	{
		return vget_lane_f32(_impl.get64(), 1);
	}

	void set_x(float value)
	{
		_impl.setX(value);
	}

	void set_y(float value)
	{
		_impl.setY(value);
	}

	v2 operator-(v2 rhs) const
	{
		return v2(_impl - rhs._impl);
	}

	v2 operator*(v2 rhs) const
	{
		return v2(mulPerElem(_impl, rhs._impl));
	}

	bool operator==(v2 rhs) const
	{
		return sce::Vectormath::Simd::Aos::allElemEqual(_impl, rhs._impl).getAsBool();
	}

	const sce::Vectormath::Simd::Aos::Vector2 &getImpl() const
	{
		return _impl;
	}

	sce::Vectormath::Simd::Aos::Vector2 &getImpl()
	{
		return _impl;
	}

private:
	sce::Vectormath::Simd::Aos::Vector2 _impl;
};

}
}

#endif
