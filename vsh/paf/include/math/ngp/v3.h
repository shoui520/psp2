/* paf::math::v3: three floats in one NEON register (vectormath's Vector3). */
#ifndef PAF_MATH_NGP_V3_H
#define PAF_MATH_NGP_V3_H

#include <vectormath.h>

namespace paf {
namespace math {

class v3 {
public:
	v3()
	{
	}

	v3(float x, float y, float z) : _impl(x, y, z)
	{
	}

	v3(sce::Vectormath::Simd::Aos::Vector3 value) : _impl(value)
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

	const sce::Vectormath::Simd::Aos::Vector3 &getImpl() const
	{
		return _impl;
	}

private:
	sce::Vectormath::Simd::Aos::Vector3 _impl;
};

}
}

#endif
