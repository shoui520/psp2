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

	/* the identity */
#ifdef PAF_BUILD_2014
	quaternion() : _raw(sce::Vectormath::Simd::Aos::Quat::identity().get128())
	{
	}

	static quaternion Identity()
	{
		return quaternion(sce::Vectormath::Simd::Aos::Quat::identity());
	}

	quaternion(float xvalue, float yvalue, float zvalue, float wvalue)
		: _raw(sce::Vectormath::Simd::Aos::Quat(xvalue, yvalue, zvalue, wvalue).get128())
	{
	}

	quaternion(sce::Vectormath::Simd::Aos::Quat value) : _raw(value.get128())
	{
	}
#else
	quaternion() : _impl(sce::Vectormath::Simd::Aos::Quat::identity())
	{
	}

	static quaternion Identity()
	{
		return quaternion(sce::Vectormath::Simd::Aos::Quat::identity());
	}

	quaternion(float xvalue, float yvalue, float zvalue, float wvalue)
		: _impl(xvalue, yvalue, zvalue, wvalue)
	{
	}

	quaternion(sce::Vectormath::Simd::Aos::Quat value) : _impl(value)
	{
	}
#endif

	bool operator==(quaternion rhs) const
	{
		return sce::Vectormath::Simd::Aos::allElemEqual(
			       sce::Vectormath::Simd::Aos::Vector4(getImpl()),
			       sce::Vectormath::Simd::Aos::Vector4(rhs.getImpl()))
			.getAsBool();
	}

	/* the product, this turn after rhs: worked lane by lane with each axis's signs (inline in
	   Sony's header, its copies out of line in the callers' modules; name ours) */
	__attribute__((noinline)) quaternion operator*(quaternion rhs) const
	{
		const float32x4_t signX = { 1.0f, -1.0f, 1.0f, -1.0f };
		const float32x4_t signZ = { -1.0f, 1.0f, 1.0f, -1.0f };
		float32x4_t a = getImpl().get128();
		float32x4_t b = rhs.getImpl().get128();
		float32x4_t swap = vcombine_f32(vget_high_f32(b), vget_low_f32(b));
		float32x4_t w = vmulq_lane_f32(b, vget_high_f32(a), 1);
		float32x4_t x = vcombine_f32(vrev64_f32(vget_low_f32(swap)),
					     vrev64_f32(vget_high_f32(swap)));
		float32x4_t y = swap;
		float32x4_t z = vrev64q_f32(b);

		x = vmulq_lane_f32(x, vget_low_f32(a), 0);
		y = vmulq_lane_f32(y, vget_low_f32(a), 1);
		z = vmulq_lane_f32(z, vget_high_f32(a), 0);
		x = vmulq_f32(x, signX);
		y = vmulq_f32(y, vcombine_f32(vdup_n_f32(1.0f), vdup_n_f32(-1.0f)));
		z = vmulq_f32(z, signZ);
		return quaternion(sce::Vectormath::Simd::Aos::Quat(
			vaddq_f32(vaddq_f32(vaddq_f32(w, x), y), z)));
	}

	void toAxisAngle(v3 &axis, float &angle);
	matrix toMatrix(float epsilon) const;
	quaternion inverse() const;
	quaternion log(float epsilon) const;
	quaternion exp(float epsilon) const;

	const sce::Vectormath::Simd::Aos::Quat &getImpl() const
	{
#ifdef PAF_BUILD_2014
		return *reinterpret_cast<const sce::Vectormath::Simd::Aos::Quat *>(&_raw);
#else
		return _impl;
#endif
	}

	sce::Vectormath::Simd::Aos::Quat &getImpl()
	{
#ifdef PAF_BUILD_2014
		return *reinterpret_cast<sce::Vectormath::Simd::Aos::Quat *>(&_raw);
#else
		return _impl;
#endif
	}

private:
#ifdef PAF_BUILD_2014
	/* SDK 3.10 Quat was trivially copyable (passed/returned in NEON vector registers). */
	float32x4_t _raw;
#else
	sce::Vectormath::Simd::Aos::Quat _impl;
#endif
};

}
}

#endif
