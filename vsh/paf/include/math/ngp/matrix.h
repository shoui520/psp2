/* paf::math::matrix: a 4x4 column matrix (vectormath's Matrix4). */
#ifndef PAF_MATH_NGP_MATRIX_H
#define PAF_MATH_NGP_MATRIX_H

#include <vectormath.h>
#include <std/string>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>

namespace paf {
namespace math {

class quaternion;

class matrix {
public:
	static matrix Frustum(float l, float r, float b, float t, float n, float f);
	static matrix Ortho(float l, float r, float b, float t, float n, float f);
	static matrix Ortho(v3 min, v3 max);
	static matrix Shadow(v3 point, v3 normal, v4 light);
	static matrix LookAt(v4 eye, v4 center, v4 up, float epsilon);

	matrix()
	{
	}

	matrix(v4 xcol, v4 ycol, v4 zcol, v4 wcol)
		: _impl(xcol.getImpl(), ycol.getImpl(), zcol.getImpl(), wcol.getImpl())
	{
	}

	matrix(sce::Vectormath::Simd::Aos::Matrix4 value) : _impl(value)
	{
	}

	static matrix Zero()
	{
		return matrix(sce::Vectormath::Simd::Aos::Matrix4::zero());
	}

	static matrix Identity()
	{
		return matrix(sce::Vectormath::Simd::Aos::Matrix4::identity());
	}

	matrix(quaternion quat);

	matrix inverse3() const;
	matrix inverse1() const;
	matrix inverse() const;
	matrix transpose() const;
	bool isCoordSys(float epsilon) const;
	matrix fixedCoordSys() const;
	string getCoordSysError();
	v3 extractEulerAnglesXYZ() const;
	v3 extractEulerAnglesXZY() const;
	v3 extractEulerAnglesYXZ() const;
	v3 extractEulerAnglesYZX() const;
	v3 extractEulerAnglesZXY() const;
	v3 extractEulerAnglesZYX() const;

	/* the product: each column the left's columns weighted by the right column's lanes */
	__attribute__((always_inline)) matrix operator*(const matrix &rhs) const
	{
		return matrix(v4(Transform(rhs._impl.getCol0())), v4(Transform(rhs._impl.getCol1())),
			      v4(Transform(rhs._impl.getCol2())), v4(Transform(rhs._impl.getCol3())));
	}

	__attribute__((always_inline)) sce::Vectormath::Simd::Aos::Vector4
	Transform(sce::Vectormath::Simd::Aos::Vector4 v) const
	{
		float32x4_t lanes = v.get128();
		float32x4_t r = vmulq_f32(_impl.getCol0().get128(),
					  vdupq_lane_f32(vget_low_f32(lanes), 0));

		r = vmlaq_f32(r, _impl.getCol1().get128(), vdupq_lane_f32(vget_low_f32(lanes), 1));
		r = vmlaq_f32(r, _impl.getCol2().get128(), vdupq_lane_f32(vget_high_f32(lanes), 0));
		r = vmlaq_f32(r, _impl.getCol3().get128(), vdupq_lane_f32(vget_high_f32(lanes), 1));
		return sce::Vectormath::Simd::Aos::Vector4(r);
	}

	v4 x() const
	{
		return v4(_impl.getCol0());
	}

	v4 y() const
	{
		return v4(_impl.getCol1());
	}

	v4 z() const
	{
		return v4(_impl.getCol2());
	}

	v4 w() const
	{
		return v4(_impl.getCol3());
	}

	const sce::Vectormath::Simd::Aos::Matrix4 &getImpl() const
	{
		return _impl;
	}

private:
	sce::Vectormath::Simd::Aos::Matrix4 _impl;
};

namespace math_matrix {

/* The names of the checks the last isCoordSys failed. */
string GetCoordSysError();

}

}
}

#endif
