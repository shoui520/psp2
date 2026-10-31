/* paf::math::quaternion: a rotation (vectormath's Quat). */
#ifndef PAF_MATH_NGP_QUATERNION_H
#define PAF_MATH_NGP_QUATERNION_H

#include <vectormath.h>
#include <math/ngp/v4.h>

namespace paf {
namespace math {

class matrix;

class quaternion {
public:
	quaternion(const matrix &mat);

	quaternion(sce::Vectormath::Simd::Aos::Quat value) : _impl(value)
	{
	}

	matrix toMatrix(float epsilon) const;

private:
	sce::Vectormath::Simd::Aos::Quat _impl;
};

}
}

#endif
