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

#ifdef PAF_BUILD_2014
	__attribute__((always_inline)) v3(float x, float y, float z)
		: _raw(sce::Vectormath::Simd::Aos::Vector3(x, y, z).get128())
	{
	}

	/* every element value */
	explicit v3(float value) : _raw(sce::Vectormath::Simd::Aos::Vector3(value).get128())
	{
	}

	v3(sce::Vectormath::Simd::Aos::Vector3 value) : _raw(value.get128())
	{
	}
#else
	v3(float x, float y, float z) : _impl(x, y, z)
	{
	}

	/* every element value */
	explicit v3(float value) : _impl(value)
	{
	}

	v3(sce::Vectormath::Simd::Aos::Vector3 value) : _impl(value)
	{
	}
#endif

	static v3 _000()
	{
		return v3(0.0f, 0.0f, 0.0f);
	}

	static v3 _010()
	{
		return v3(0.0f, 1.0f, 0.0f);
	}

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

	/* the element idx, 0 to 2 */
	float extract(int idx) const
	{
		return sce_vectormath_extract_f4(getImpl().get128(), idx);
	}

	void set(int idx, float value)
	{
		getImpl().setElem(idx, value);
	}

#ifdef PAF_BUILD_2014
#define PAF_V3_OP_INLINE __attribute__((always_inline))
#else
#define PAF_V3_OP_INLINE
#endif

	PAF_V3_OP_INLINE v3 operator+(v3 rhs) const
	{
		return v3(getImpl() + rhs.getImpl());
	}

	PAF_V3_OP_INLINE v3 operator-(v3 rhs) const
	{
		return v3(getImpl() - rhs.getImpl());
	}

	PAF_V3_OP_INLINE v3 operator*(float rhs) const
	{
		return v3(getImpl() * rhs);
	}

	PAF_V3_OP_INLINE bool operator==(v3 rhs) const
	{
		return sce::Vectormath::Simd::Aos::allElemEqual(getImpl(), rhs.getImpl()).getAsBool();
	}
#undef PAF_V3_OP_INLINE

	const sce::Vectormath::Simd::Aos::Vector3 &getImpl() const
	{
#ifdef PAF_BUILD_2014
		return *reinterpret_cast<const sce::Vectormath::Simd::Aos::Vector3 *>(&_raw);
#else
		return _impl;
#endif
	}

	sce::Vectormath::Simd::Aos::Vector3 &getImpl()
	{
#ifdef PAF_BUILD_2014
		return *reinterpret_cast<sce::Vectormath::Simd::Aos::Vector3 *>(&_raw);
#else
		return _impl;
#endif
	}

private:
#ifdef PAF_BUILD_2014
	/* SDK 3.10 Vector3 was trivially copyable (passed/returned in NEON vector registers). */
	float32x4_t _raw;
#else
	sce::Vectormath::Simd::Aos::Vector3 _impl;
#endif
};

}
}

#endif
