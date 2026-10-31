/* paf::math::v4: four floats in one NEON register (vectormath's Vector4). */
#ifndef PAF_MATH_NGP_V4_H
#define PAF_MATH_NGP_V4_H

#include <vectormath.h>

namespace paf {
namespace math {

class v4 {
public:
	v4()
	{
	}

	v4(float x, float y, float z, float w) : _impl(x, y, z, w)
	{
	}

	v4(sce::Vectormath::Simd::Aos::Vector4 value) : _impl(value)
	{
	}

	float extract_x() const
	{
		return vgetq_lane_f32(_impl.get128(), 0);
	}

	float extract_y() const
	{
		return vgetq_lane_f32(_impl.get128(), 1);
	}

	float extract_z() const
	{
		return vgetq_lane_f32(_impl.get128(), 2);
	}

	float extract_w() const
	{
		return vgetq_lane_f32(_impl.get128(), 3);
	}

	v4 operator-(v4 rhs) const
	{
		return v4(_impl - rhs._impl);
	}

	bool operator==(v4 rhs) const
	{
		return sce::Vectormath::Simd::Aos::allElemEqual(_impl, rhs._impl).getAsBool();
	}

	sce::Vectormath::Simd::floatInVec dot(v4 other) const
	{
		return sce::Vectormath::Simd::Aos::dot(_impl, other._impl);
	}

	sce::Vectormath::Simd::floatInVec len() const
	{
		return sce::Vectormath::Simd::Aos::length(_impl);
	}

	v4 unit() const
	{
		return v4(_impl * sce::Vectormath::Simd::recipf(len()));
	}

	v4 cross(v4 other) const
	{
		return v4(sce::Vectormath::Simd::Aos::Vector4(
			sce::Vectormath::Simd::Aos::cross(_impl.getXYZ(), other._impl.getXYZ()), 0.0f));
	}

	v4 anyPerp() const;

	const sce::Vectormath::Simd::Aos::Vector4 &getImpl() const
	{
		return _impl;
	}

private:
	sce::Vectormath::Simd::Aos::Vector4 _impl;
};

/* The unit vector along the longest of the cross products with the three axes. */
inline v4 v4::anyPerp() const
{
	sce::Vectormath::Simd::Aos::Vector4 px(0.0f, extract_z(), -extract_y(), 0.0f);
	sce::Vectormath::Simd::Aos::Vector4 py(-extract_z(), 0.0f, extract_x(), 0.0f);
	sce::Vectormath::Simd::Aos::Vector4 pz(extract_y(), -extract_x(), 0.0f, 0.0f);
	sce::Vectormath::Simd::floatInVec lx = lengthSqr(px);
	sce::Vectormath::Simd::floatInVec ly = lengthSqr(py);
	sce::Vectormath::Simd::floatInVec lz = lengthSqr(pz);

	if ((lx > ly).getAsBool()) {
		if ((lx > lz).getAsBool())
			return v4(px).unit();
		return v4(pz).unit();
	}
	if ((ly > lz).getAsBool())
		return v4(py).unit();
	return v4(pz).unit();
}

}
}

#endif
