/* paf::math::quaternion: conversions to and from matrices, axes and Euler angles. */
#include <math/math.h>
#include "../std/math.h"

using sce::Vectormath::Simd::Aos::Matrix4;
using sce::Vectormath::Simd::Aos::Quat;
using sce::Vectormath::Simd::Aos::Vector3;
using sce::Vectormath::Simd::Aos::Vector4;
using sce::Vectormath::Simd::floatInVec;

namespace paf {
namespace math {

/* Shoemake's conversion: from the trace, or from the largest diagonal element. */
quaternion::quaternion(const matrix &mat)
{
	const Matrix4 &m = mat.getImpl();
	float trace = m.getElem(0, 0).getAsFloat() + m.getElem(1, 1).getAsFloat() +
		      m.getElem(2, 2).getAsFloat();

	if (trace > 0.0f) {
		float s = __builtin_sqrtf(trace + 1.0f);

		_impl.setW(s * 0.5f);
		s = 0.5f / s;
		_impl.setX((m.getElem(1, 2).getAsFloat() - m.getElem(2, 1).getAsFloat()) * s);
		_impl.setY((m.getElem(2, 0).getAsFloat() - m.getElem(0, 2).getAsFloat()) * s);
		_impl.setZ((m.getElem(0, 1).getAsFloat() - m.getElem(1, 0).getAsFloat()) * s);
	} else {
		int next[3] = { 1, 2, 0 };
		int i = 0;

		if (m.getElem(1, 1).getAsFloat() > m.getElem(0, 0).getAsFloat())
			i = 1;
		if (m.getElem(2, 2).getAsFloat() > m.getElem(i, i).getAsFloat())
			i = 2;

		int j = next[i];
		int k = next[j];
		float s = __builtin_sqrtf(m.getElem(i, i).getAsFloat() - m.getElem(j, j).getAsFloat() -
				m.getElem(k, k).getAsFloat() + 1.0f);

		_impl.setElem(i, s * 0.5f);
		s = 0.5f / s;
		_impl.setW((m.getElem(j, k).getAsFloat() - m.getElem(k, j).getAsFloat()) * s);
		_impl.setElem(j, (m.getElem(i, j).getAsFloat() + m.getElem(j, i).getAsFloat()) * s);
		_impl.setElem(k, (m.getElem(i, k).getAsFloat() + m.getElem(k, i).getAsFloat()) * s);
	}
}

/* The rotation that turns from into to; none when they are less than epsilon apart. */
quaternion quaternion::RotationBetween(v4 from, v4 to, float epsilon)
{
	from = from.unit();
	to = to.unit();

	float angle = sce_paf_acosf(fminf(fmaxf(from.dot(to), floatInVec(-1.0f)),
					  floatInVec(1.0f)).getAsFloat());

	if (__builtin_fabsf(angle) < epsilon)
		return quaternion(Quat::identity());

	v4 axis = from.cross(to).unit();
	float half = angle * 0.5f;
	Quat result(axis.getImpl() * sce_paf_sinf(half));

	result.setW(sce_paf_cosf(half));
	return quaternion(result);
}

void quaternion::toAxisAngle(v3 &axis, float &angle)
{
	Quat q = _impl;
	floatInVec len = length(q.getXYZ());

	if (len.getAsFloat() > 0.0f) {
		angle = sce_paf_acosf(ClampUnit(q.getW().getAsFloat())) * 2.0f;
		axis = v3(q.getXYZ() / len);
	} else {
		angle = 0.0f;
		axis = v3(Vector3::zAxis());
	}
}

matrix quaternion::toMatrix(float epsilon) const
{
	float n = norm(_impl).getAsFloat();

	if (n == 0.0f)
		return matrix(Matrix4::identity());

	float s = 2.0f / n;
	float x = _impl.getX().getAsFloat();
	float y = _impl.getY().getAsFloat();
	float z = _impl.getZ().getAsFloat();
	float w = _impl.getW().getAsFloat();
	float ys = y * s;
	float xs = x * s;
	float zs = z * s;
	float yy = y * ys;
	float xx = x * xs;
	float xy = x * ys;
	float wx = w * xs;
	float wy = w * ys;
	float wz = w * zs;
	float yz = y * zs;
	float xz = z * xs;
	float zz = z * zs;

	return matrix(Matrix4(Vector4(1.0f - yy - zz, wz + xy, xz - wy, 0.0f),
			      Vector4(xy - wz, 1.0f - xx - zz, wx + yz, 0.0f),
			      Vector4(wy + xz, yz - wx, 1.0f - xx - yy, 0.0f),
			      Vector4::wAxis()));
}

quaternion quaternion::inverse() const
{
	return quaternion(conj(_impl / norm(_impl)));
}

quaternion quaternion::log(float epsilon) const
{
	float w = _impl.getW().getAsFloat();

	if (__builtin_fabsf(w) < 1.0f) {
		float angle = sce_paf_acosf(ClampUnit(w));
		float s = sce_paf_sinf(angle);

		if (__builtin_fabsf(s) >= epsilon)
			return quaternion(Quat(_impl * (angle / s)).setW(0.0f));
	}
	return quaternion(Quat(_impl).setW(0.0f));
}

quaternion quaternion::exp(float epsilon) const
{
	float angle = length(_impl.getXYZ()).getAsFloat();
	float s = sce_paf_sinf(angle);
	quaternion result(_impl);

	if (__builtin_fabsf(s) > epsilon)
		result._impl = _impl * (s / angle);
	result._impl.setW(sce_paf_cosf(angle));
	return result;
}

/* Along the shorter arc; q0 when the arc is shorter than epsilon. */
quaternion quaternion::Slerp(quaternion q0, quaternion q1, float t, float epsilon)
{
	if (t == 0.0f)
		return q0;
	if (t == 1.0f)
		return q1;

	Quat start = q0._impl;
	Quat end = select(q1._impl, q1._impl * -1.0f, dot(q1._impl, start) < floatInVec(0.0f));
	float angle = sce_paf_acosf(ClampUnit(dot(end, start).getAsFloat()));

	if (angle < epsilon)
		return quaternion(start);

	float r = 1.0f / sce_paf_sinf(angle);
	float a = sce_paf_sinf((1.0f - t) * angle) * r;

	return quaternion(start * a + end * (sce_paf_sinf(angle * t) * r));
}


/*
 * The rotation by the Euler angles, applied in the order the name gives:
 * a sum of two products of the half angles' sines and cosines.
 */
quaternion quaternion::FromEulerAnglesZYX(v3 angles)
{
	vec_float4 s, c;

	angles = v3(angles.getImpl() * 0.5f);
	sce_vectormath_sincosf4(angles.getImpl().get128(), &s, &c);

	Vector4 sin(s), cos(c);
	Vector4 a = mulPerElem(mulPerElem(Vector4(cos.getZ(), cos.getX(), cos.getX(), cos.getZ()),
					  Vector4(cos.getY(), cos.getZ(), cos.getY(), cos.getY())),
			       Vector4(sin.getX(), sin.getY(), sin.getZ(), cos.getX()));
	Vector4 b = mulPerElem(mulPerElem(Vector4(cos.getX(), cos.getY(), cos.getZ(), sin.getZ()),
					  Vector4(sin.getZ(), sin.getZ(), sin.getY(), sin.getY())),
			       Vector4(sin.getY(), sin.getX(), sin.getX(), sin.getX()));

	return quaternion(Quat(a + mulPerElem(b, Vector4(-1.0f, 1.0f, -1.0f, 1.0f))));
}

quaternion quaternion::FromEulerAnglesYXZ(v3 angles)
{
	vec_float4 s, c;

	angles = v3(angles.getImpl() * 0.5f);
	sce_vectormath_sincosf4(angles.getImpl().get128(), &s, &c);

	Vector4 sin(s), cos(c);
	Vector4 a = mulPerElem(mulPerElem(Vector4(cos.getZ(), cos.getZ(), cos.getY(), cos.getY()),
					  Vector4(cos.getY(), cos.getX(), cos.getX(), cos.getX())),
			       Vector4(sin.getX(), sin.getY(), sin.getZ(), cos.getZ()));
	Vector4 b = mulPerElem(mulPerElem(Vector4(cos.getX(), cos.getY(), cos.getZ(), sin.getY()),
					  Vector4(sin.getY(), sin.getX(), sin.getY(), sin.getX())),
			       Vector4(sin.getZ(), sin.getZ(), sin.getX(), sin.getZ()));

	return quaternion(Quat(a + mulPerElem(b, Vector4(1.0f, -1.0f, -1.0f, 1.0f))));
}

quaternion quaternion::FromEulerAnglesXZY(v3 angles)
{
	vec_float4 s, c;

	angles = v3(angles.getImpl() * 0.5f);
	sce_vectormath_sincosf4(angles.getImpl().get128(), &s, &c);

	Vector4 sin(s), cos(c);
	Vector4 a = mulPerElem(mulPerElem(Vector4(cos.getY(), cos.getX(), cos.getY(), cos.getX()),
					  Vector4(cos.getZ(), cos.getZ(), cos.getX(), cos.getZ())),
			       Vector4(sin.getX(), sin.getY(), sin.getZ(), cos.getY()));
	Vector4 b = mulPerElem(mulPerElem(Vector4(cos.getX(), cos.getY(), cos.getZ(), sin.getX()),
					  Vector4(sin.getZ(), sin.getX(), sin.getX(), sin.getZ())),
			       Vector4(sin.getY(), sin.getZ(), sin.getY(), sin.getY()));

	return quaternion(Quat(a + mulPerElem(b, Vector4(-1.0f, -1.0f, 1.0f, 1.0f))));
}

quaternion quaternion::FromEulerAnglesXYZ(v3 angles)
{
	vec_float4 s, c;

	angles = v3(angles.getImpl() * 0.5f);
	sce_vectormath_sincosf4(angles.getImpl().get128(), &s, &c);

	Vector4 sin(s), cos(c);
	Vector4 a = mulPerElem(mulPerElem(Vector4(cos.getZ(), cos.getZ(), cos.getX(), cos.getX()),
					  Vector4(cos.getY(), cos.getX(), cos.getY(), cos.getY())),
			       Vector4(sin.getX(), sin.getY(), sin.getZ(), cos.getZ()));
	Vector4 b = mulPerElem(mulPerElem(Vector4(cos.getX(), cos.getY(), cos.getZ(), sin.getX()),
					  Vector4(sin.getY(), sin.getX(), sin.getX(), sin.getY())),
			       Vector4(sin.getZ(), sin.getZ(), sin.getY(), sin.getZ()));

	return quaternion(Quat(a + mulPerElem(b, Vector4(1.0f, -1.0f, 1.0f, -1.0f))));
}

quaternion quaternion::FromEulerAnglesYZX(v3 angles)
{
	vec_float4 s, c;

	angles = v3(angles.getImpl() * 0.5f);
	sce_vectormath_sincosf4(angles.getImpl().get128(), &s, &c);

	Vector4 sin(s), cos(c);
	Vector4 a = mulPerElem(mulPerElem(Vector4(cos.getY(), cos.getX(), cos.getX(), cos.getY()),
					  Vector4(cos.getZ(), cos.getZ(), cos.getY(), cos.getZ())),
			       Vector4(sin.getX(), sin.getY(), sin.getZ(), cos.getX()));
	Vector4 b = mulPerElem(mulPerElem(Vector4(cos.getX(), cos.getY(), cos.getZ(), sin.getY()),
					  Vector4(sin.getY(), sin.getZ(), sin.getY(), sin.getZ())),
			       Vector4(sin.getZ(), sin.getX(), sin.getX(), sin.getX()));

	return quaternion(Quat(a + mulPerElem(b, Vector4(1.0f, 1.0f, -1.0f, -1.0f))));
}

quaternion quaternion::FromEulerAnglesZXY(v3 angles)
{
	vec_float4 s, c;

	angles = v3(angles.getImpl() * 0.5f);
	sce_vectormath_sincosf4(angles.getImpl().get128(), &s, &c);

	Vector4 sin(s), cos(c);
	Vector4 a = mulPerElem(mulPerElem(Vector4(cos.getY(), cos.getZ(), cos.getY(), cos.getZ()),
					  Vector4(cos.getZ(), cos.getX(), cos.getX(), cos.getX())),
			       Vector4(sin.getX(), sin.getY(), sin.getZ(), cos.getY()));
	Vector4 b = mulPerElem(mulPerElem(Vector4(cos.getX(), cos.getY(), cos.getZ(), sin.getZ()),
					  Vector4(sin.getZ(), sin.getZ(), sin.getX(), sin.getX())),
			       Vector4(sin.getY(), sin.getX(), sin.getY(), sin.getY()));

	return quaternion(Quat(a + mulPerElem(b, Vector4(-1.0f, 1.0f, 1.0f, -1.0f))));
}
}
}
