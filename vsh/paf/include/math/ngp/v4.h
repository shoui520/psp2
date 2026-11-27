/* paf::math::v4: four floats in one NEON register (vectormath's Vector4). */
#ifndef PAF_MATH_NGP_V4_H
#define PAF_MATH_NGP_V4_H

#include <vectormath.h>
#include <math/ngp/v3.h>

namespace paf {
namespace math {

class v4 {
public:
	v4()
	{
	}

#ifdef PAF_BUILD_2014
	__attribute__((always_inline)) v4(float x, float y, float z, float w)
		: _raw(sce::Vectormath::Simd::Aos::Vector4(x, y, z, w).get128())
	{
	}

	v4(sce::Vectormath::Simd::Aos::Vector4 value) : _raw(value.get128())
	{
	}

	v4(v3 xyz, float w) : _raw(sce::Vectormath::Simd::Aos::Vector4(xyz.getImpl(), w).get128())
	{
	}
#else
	__attribute__((always_inline)) v4(float x, float y, float z, float w) : _impl(x, y, z, w)
	{
	}

	v4(sce::Vectormath::Simd::Aos::Vector4 value) : _impl(value)
	{
	}

	v4(v3 xyz, float w) : _impl(xyz.getImpl(), w)
	{
	}
#endif

	float extract_x() const
	{
		return vgetq_lane_f32(getImpl().get128(), 0);
	}

	float extract_y() const
	{
		return vgetq_lane_f32(getImpl().get128(), 1);
	}

	float extract_z() const
	{
		return vgetq_lane_f32(getImpl().get128(), 2);
	}

	float extract_w() const
	{
		return vgetq_lane_f32(getImpl().get128(), 3);
	}

	/* the element idx, 0 to 3 */
	float extract(int idx) const
	{
		return sce_vectormath_extract_f4(getImpl().get128(), idx);
	}

	void set_x(float value)
	{
		getImpl().setX(value);
	}

	void set_y(float value)
	{
		getImpl().setY(value);
	}

	void set_z(float value)
	{
		getImpl().setZ(value);
	}

	void set_w(float value)
	{
		getImpl().setW(value);
	}

#ifdef PAF_BUILD_2014
#define PAF_V4_OP_INLINE __attribute__((always_inline))
#else
#define PAF_V4_OP_INLINE
#endif

	PAF_V4_OP_INLINE v4 operator+(v4 rhs) const
	{
		return v4(getImpl() + rhs.getImpl());
	}

	PAF_V4_OP_INLINE v4 operator-(v4 rhs) const
	{
		return v4(getImpl() - rhs.getImpl());
	}

	__attribute__((always_inline)) v4 operator-() const
	{
		return v4(-getImpl());
	}

	PAF_V4_OP_INLINE v4 operator*(float rhs) const
	{
		return v4(getImpl() * rhs);
	}

	/* inlined wherever Sony's modules divide (no module keeps a copy) */
	__attribute__((always_inline)) v4 operator/(float rhs) const
	{
		return v4(getImpl() / rhs);
	}

	PAF_V4_OP_INLINE bool operator==(v4 rhs) const
	{
		return sce::Vectormath::Simd::Aos::allElemEqual(getImpl(), rhs.getImpl()).getAsBool();
	}
#undef PAF_V4_OP_INLINE

	sce::Vectormath::Simd::floatInVec dot(v4 other) const
	{
		return sce::Vectormath::Simd::Aos::dot(getImpl(), other.getImpl());
	}

	sce::Vectormath::Simd::floatInVec len() const
	{
		return sce::Vectormath::Simd::Aos::length(getImpl());
	}

	v4 unit() const
	{
		return v4(getImpl() * sce::Vectormath::Simd::recipf(len()));
	}

	v4 cross(v4 other) const
	{
		return v4(sce::Vectormath::Simd::Aos::Vector4(
			sce::Vectormath::Simd::Aos::cross(getImpl().getXYZ(), other.getImpl().getXYZ()), 0.0f));
	}

	v4 anyPerp() const;

	const sce::Vectormath::Simd::Aos::Vector4 &getImpl() const
	{
#ifdef PAF_BUILD_2014
		return *reinterpret_cast<const sce::Vectormath::Simd::Aos::Vector4 *>(&_raw);
#else
		return _impl;
#endif
	}

	sce::Vectormath::Simd::Aos::Vector4 &getImpl()
	{
#ifdef PAF_BUILD_2014
		return *reinterpret_cast<sce::Vectormath::Simd::Aos::Vector4 *>(&_raw);
#else
		return _impl;
#endif
	}

private:
#ifdef PAF_BUILD_2014
	/* SDK 3.10 Vector4 was trivially copyable (passed/returned in NEON vector registers). */
	float32x4_t _raw;
#else
	sce::Vectormath::Simd::Aos::Vector4 _impl;
#endif
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
