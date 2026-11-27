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

#ifdef PAF_BUILD_2014
	__attribute__((always_inline)) v2(float x, float y)
		: _raw(sce::Vectormath::Simd::Aos::Vector2(x, y).get64())
	{
	}

	v2(sce::Vectormath::Simd::Aos::Vector2 value) : _raw(value.get64())
	{
	}
#else
	__attribute__((always_inline)) v2(float x, float y) : _impl(x, y)
	{
	}

	v2(sce::Vectormath::Simd::Aos::Vector2 value) : _impl(value)
	{
	}
#endif

	float extract_x() const
	{
		return vget_lane_f32(getImpl().get64(), 0);
	}

	float extract_y() const
	{
		return vget_lane_f32(getImpl().get64(), 1);
	}

	/* the element idx, 0 or 1 */
	float extract(int idx) const
	{
		return sce_vectormath_extract_f2(getImpl().get64(), idx);
	}

	void set_x(float value)
	{
		getImpl().setX(value);
	}

	void set_y(float value)
	{
		getImpl().setY(value);
	}

#ifdef PAF_BUILD_2014
#define PAF_V2_OP_INLINE __attribute__((always_inline))
#else
#define PAF_V2_OP_INLINE
#endif

	PAF_V2_OP_INLINE v2 operator+(v2 rhs) const
	{
		return v2(getImpl() + rhs.getImpl());
	}

	PAF_V2_OP_INLINE v2 operator-(v2 rhs) const
	{
		return v2(getImpl() - rhs.getImpl());
	}

	PAF_V2_OP_INLINE v2 operator*(v2 rhs) const
	{
		return v2(mulPerElem(getImpl(), rhs.getImpl()));
	}

	PAF_V2_OP_INLINE v2 operator*(float rhs) const
	{
		return v2(getImpl() * rhs);
	}

	PAF_V2_OP_INLINE v2 operator/(float rhs) const
	{
		return v2(getImpl() / rhs);
	}

	/* divided by the scalar, the result also returned by value */
	__attribute__((noinline)) v2 operator/=(sce::Vectormath::Simd::floatInVec rhs)
	{
		getImpl() /= rhs;
		return *this;
	}

	PAF_V2_OP_INLINE bool operator==(v2 rhs) const
	{
		return sce::Vectormath::Simd::Aos::allElemEqual(getImpl(), rhs.getImpl()).getAsBool();
	}
#undef PAF_V2_OP_INLINE

	const sce::Vectormath::Simd::Aos::Vector2 &getImpl() const
	{
#ifdef PAF_BUILD_2014
		return *reinterpret_cast<const sce::Vectormath::Simd::Aos::Vector2 *>(&_raw);
#else
		return _impl;
#endif
	}

	sce::Vectormath::Simd::Aos::Vector2 &getImpl()
	{
#ifdef PAF_BUILD_2014
		return *reinterpret_cast<sce::Vectormath::Simd::Aos::Vector2 *>(&_raw);
#else
		return _impl;
#endif
	}

private:
#ifdef PAF_BUILD_2014
	/* SDK 3.10 Vector2 was trivially copyable (passed/returned in NEON vector registers). */
	float32x2_t _raw;
#else
	sce::Vectormath::Simd::Aos::Vector2 _impl;
#endif
};

}
}

#endif
