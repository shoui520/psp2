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

#ifdef PAF_BUILD_2014
	matrix(v4 xcol, v4 ycol, v4 zcol, v4 wcol)
	{
		_raw[0] = xcol.getImpl().get128();
		_raw[1] = ycol.getImpl().get128();
		_raw[2] = zcol.getImpl().get128();
		_raw[3] = wcol.getImpl().get128();
	}

	matrix(sce::Vectormath::Simd::Aos::Matrix4 value)
	{
		_raw[0] = value.getCol0().get128();
		_raw[1] = value.getCol1().get128();
		_raw[2] = value.getCol2().get128();
		_raw[3] = value.getCol3().get128();
	}
#else
	matrix(v4 xcol, v4 ycol, v4 zcol, v4 wcol)
		: _impl(xcol.getImpl(), ycol.getImpl(), zcol.getImpl(), wcol.getImpl())
	{
	}

	matrix(sce::Vectormath::Simd::Aos::Matrix4 value) : _impl(value)
	{
	}
#endif

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
		return matrix(v4(Transform(rhs.getImpl().getCol0())),
			      v4(Transform(rhs.getImpl().getCol1())),
			      v4(Transform(rhs.getImpl().getCol2())),
			      v4(Transform(rhs.getImpl().getCol3())));
	}

	__attribute__((always_inline)) sce::Vectormath::Simd::Aos::Vector4
	Transform(sce::Vectormath::Simd::Aos::Vector4 v) const
	{
		float32x4_t lanes = v.get128();
		float32x4_t r = vmulq_f32(getImpl().getCol0().get128(),
					  vdupq_lane_f32(vget_low_f32(lanes), 0));

		r = vmlaq_f32(r, getImpl().getCol1().get128(), vdupq_lane_f32(vget_low_f32(lanes), 1));
		r = vmlaq_f32(r, getImpl().getCol2().get128(), vdupq_lane_f32(vget_high_f32(lanes), 0));
		r = vmlaq_f32(r, getImpl().getCol3().get128(), vdupq_lane_f32(vget_high_f32(lanes), 1));
		return sce::Vectormath::Simd::Aos::Vector4(r);
	}

	v4 x() const
	{
		return v4(getImpl().getCol0());
	}

	v4 y() const
	{
		return v4(getImpl().getCol1());
	}

	v4 z() const
	{
		return v4(getImpl().getCol2());
	}

	v4 w() const
	{
		return v4(getImpl().getCol3());
	}

	/* the columns copied one by one (name ours) */
	void assign(const matrix &rhs)
	{
		getImpl().setCol0(rhs.getImpl().getCol0());
		getImpl().setCol1(rhs.getImpl().getCol1());
		getImpl().setCol2(rhs.getImpl().getCol2());
		getImpl().setCol3(rhs.getImpl().getCol3());
	}

	void set_w(v4 col)
	{
		getImpl().setCol3(col.getImpl());
	}

	const sce::Vectormath::Simd::Aos::Matrix4 &getImpl() const
	{
#ifdef PAF_BUILD_2014
		return *reinterpret_cast<const sce::Vectormath::Simd::Aos::Matrix4 *>(_raw);
#else
		return _impl;
#endif
	}

	sce::Vectormath::Simd::Aos::Matrix4 &getImpl()
	{
#ifdef PAF_BUILD_2014
		return *reinterpret_cast<sce::Vectormath::Simd::Aos::Matrix4 *>(_raw);
#else
		return _impl;
#endif
	}

private:
#ifdef PAF_BUILD_2014
	/* SDK 3.10 Matrix4 was trivially copyable (returned in NEON vector registers). */
	float32x4_t _raw[4];
#else
	sce::Vectormath::Simd::Aos::Matrix4 _impl;
#endif
};

namespace math_matrix {

/* The names of the checks the last isCoordSys failed. */
string GetCoordSysError();

}

}
}

#endif
