/* paf::math::quaternion: a rotation (vectormath's Quat). */
#ifndef PAF_MATH_NGP_QUATERNION_H
#define PAF_MATH_NGP_QUATERNION_H

#include <vectormath.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>

namespace paf {
namespace math {

class matrix;

class quaternion {
public:
	static quaternion RotationBetween(v4 from, v4 to, float epsilon);
	static quaternion Slerp(quaternion q0, quaternion q1, float t, float epsilon);
	static quaternion FromEulerAnglesZYX(v3 angles);
	static quaternion FromEulerAnglesYXZ(v3 angles);
	static quaternion FromEulerAnglesXZY(v3 angles);
	static quaternion FromEulerAnglesXYZ(v3 angles);
	static quaternion FromEulerAnglesYZX(v3 angles);
	static quaternion FromEulerAnglesZXY(v3 angles);

	quaternion(const matrix &mat);

	quaternion(sce::Vectormath::Simd::Aos::Quat value) : _impl(value)
	{
	}

	void toAxisAngle(v3 &axis, float &angle);
	matrix toMatrix(float epsilon) const;
	quaternion inverse() const;
	quaternion log(float epsilon) const;
	quaternion exp(float epsilon) const;

	const sce::Vectormath::Simd::Aos::Quat &getImpl() const
	{
		return _impl;
	}

private:
	sce::Vectormath::Simd::Aos::Quat _impl;
};

}
}

#endif
