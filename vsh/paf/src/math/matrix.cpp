/* paf::math::matrix: projections, inverses and coordinate-system checks. */
#include <math/math.h>
#include "../std/math.h"

using sce::Vectormath::Simd::Aos::Matrix3;
using sce::Vectormath::Simd::Aos::Matrix4;
using sce::Vectormath::Simd::Aos::Vector3;
using sce::Vectormath::Simd::Aos::Vector4;
using sce::Vectormath::Simd::floatInVec;

namespace paf {
namespace math {

/* Why the last isCoordSys failed: one COORD_SYS_* bit. */
enum {
	COORD_SYS_X_NAN = 0x1,
	COORD_SYS_Y_NAN = 0x2,
	COORD_SYS_Z_NAN = 0x4,
	COORD_SYS_W_NAN = 0x8,
	COORD_SYS_NOT_RIGHT_HANDED = 0x10,
	COORD_SYS_X_NOT_UNIT_LENGTH = 0x20,
	COORD_SYS_Y_NOT_UNIT_LENGTH = 0x40,
	COORD_SYS_Z_NOT_UNIT_LENGTH = 0x80,
	COORD_SYS_XY_NOT_PERPENDICULAR = 0x100,
	COORD_SYS_YZ_NOT_PERPENDICULAR = 0x200,
	COORD_SYS_ZX_NOT_PERPENDICULAR = 0x400,
	COORD_SYS_WW_NOT_1 = 0x800,
	COORD_SYS_LAST_ROW_NOT_0001 = 0x1000,
	COORD_SYS_MAT_NOT_UNIFORM_SCALE = 0x2000,
	COORD_SYS_X_ZERO_LENGTH = 0x4000,
	COORD_SYS_Y_ZERO_LENGTH = 0x8000,
	COORD_SYS_Z_ZERO_LENGTH = 0x10000
};

static int s_coord_sys_error;

matrix::matrix(quaternion quat) : _impl(quat.toMatrix(0.0001f)._impl)
{
}

matrix matrix::Frustum(float l, float r, float b, float t, float n, float f)
{
	return matrix(v4(n * 2.0f / (r - l), 0.0f, 0.0f, 0.0f),
		      v4(0.0f, n * 2.0f / (t - b), 0.0f, 0.0f),
		      v4((l + r) / (r - l), (b + t) / (t - b), -(f + n) / (f - n), -1.0f),
		      v4(0.0f, 0.0f, f * -2.0f * n / (f - n), 0.0f));
}

matrix matrix::Ortho(float l, float r, float b, float t, float n, float f)
{
	float rl = 1.0f / (r - l);
	float tb = 1.0f / (t - b);
	float nf = 1.0f / (n - f);

	return matrix(v4(rl * 2.0f, 0.0f, 0.0f, 0.0f),
		      v4(0.0f, tb * 2.0f, 0.0f, 0.0f),
		      v4(0.0f, 0.0f, nf * 2.0f, 0.0f),
		      v4(-((l + r) * rl), -((b + t) * tb), (f + n) * nf, 1.0f));
}

matrix matrix::Ortho(v3 min, v3 max)
{
	return Ortho(min.extract_x(), max.extract_x(), min.extract_y(), max.extract_y(),
		     min.extract_z(), max.extract_z());
}

/* Projects onto the plane through point with the given normal, away from light. */
matrix matrix::Shadow(v3 point, v3 normal, v4 light)
{
	Vector4 plane(normal.getImpl(), -dot(point.getImpl(), normal.getImpl()));
	float d = dot(light.getImpl(), plane);

	return matrix(Matrix4(Vector4(d, 0.0f, 0.0f, 0.0f) - light.getImpl() * plane.getX(),
			      Vector4(0.0f, d, 0.0f, 0.0f) - light.getImpl() * plane.getY(),
			      Vector4(0.0f, 0.0f, d, 0.0f) - light.getImpl() * plane.getZ(),
			      Vector4(0.0f, 0.0f, 0.0f, d) - light.getImpl() * plane.getW()));
}

matrix matrix::LookAt(v4 eye, v4 center, v4 up, float epsilon)
{
	v4 x, y, z;

	if (center == eye) {
		y = up;
		x = y.anyPerp();
		z = x.cross(y);
	} else {
		z = (eye - center).unit();
		if ((sce::Vectormath::Simd::fabsf(up.dot(z)) > sce::Vectormath::Simd::floatInVec(epsilon)).getAsBool()) {
			y = z.anyPerp();
			x = y.cross(z);
		} else {
			x = up.cross(z).unit();
			y = z.cross(x);
		}
	}
	return matrix(x, y, z, eye);
}

/* The inverse of the upper 3x3 part (adjugate over determinant), without translation. */
matrix matrix::inverse3() const
{
	Vector4 a = _impl.getCol0();
	Vector4 b = _impl.getCol1();
	Vector4 c = _impl.getCol2();
	floatInVec det = b.getY() * a.getX() * c.getZ() + c.getY() * b.getX() * a.getZ() +
			 a.getY() * c.getX() * b.getZ() - b.getY() * c.getX() * a.getZ() -
			 a.getY() * b.getX() * c.getZ() - c.getY() * a.getX() * b.getZ();

	return matrix(Matrix4(Matrix3(Vector3(c.getZ() * b.getY() - b.getZ() * c.getY(),
					      -(c.getZ() * a.getY() - a.getZ() * c.getY()),
					      b.getZ() * a.getY() - a.getZ() * b.getY()),
				      Vector3(-(c.getZ() * b.getX() - b.getZ() * c.getX()),
					      c.getZ() * a.getX() - a.getZ() * c.getX(),
					      -(b.getZ() * a.getX() - a.getZ() * b.getX())),
				      Vector3(c.getY() * b.getX() - b.getY() * c.getX(),
					      -(c.getY() * a.getX() - a.getY() * c.getX()),
					      b.getY() * a.getX() - a.getY() * b.getX())) * recipf(det),
			      Vector3(0.0f)));
}

/* The inverse of a rotation and translation: the transposed rotation, translated back. */
matrix matrix::inverse1() const
{
	Matrix4 inv(sce::Vectormath::Simd::Aos::transpose(_impl.getUpper3x3()), Vector3(0.0f));
	Vector4 t = -_impl.getCol3();

	inv.setCol3(inv * Vector4(t.getXYZ(), 1.0f));
	return matrix(inv);
}

matrix matrix::inverse() const
{
	return matrix(sce::Vectormath::Simd::Aos::inverse(_impl));
}

matrix matrix::transpose() const
{
	return matrix(sce::Vectormath::Simd::Aos::transpose(_impl));
}

/* A NaN compares unequal to itself. */
bool matrix::isCoordSys(float epsilon) const
{
	floatInVec eps(epsilon);
	floatInVec one(1.0f);

	s_coord_sys_error = 0;
	if (!(x() == x())) {
		s_coord_sys_error = COORD_SYS_X_NAN;
		return false;
	}
	if (!(y() == y())) {
		s_coord_sys_error = COORD_SYS_Y_NAN;
		return false;
	}
	if (!(z() == z())) {
		s_coord_sys_error = COORD_SYS_Z_NAN;
		return false;
	}
	if (!(w() == w())) {
		s_coord_sys_error = COORD_SYS_W_NAN;
		return false;
	}
	if (!(z().dot(x().cross(y())) > floatInVec(0.0f)).getAsBool()) {
		s_coord_sys_error = COORD_SYS_NOT_RIGHT_HANDED;
		return false;
	}
	if (!((float)fabsf(x().len() - one) <= epsilon)) {
		s_coord_sys_error = COORD_SYS_X_NOT_UNIT_LENGTH;
		return false;
	}
	if (!((float)fabsf(y().len() - one) <= epsilon)) {
		s_coord_sys_error = COORD_SYS_Y_NOT_UNIT_LENGTH;
		return false;
	}
	if (!((float)fabsf(z().len() - one) <= epsilon)) {
		s_coord_sys_error = COORD_SYS_Z_NOT_UNIT_LENGTH;
		return false;
	}
	if (!(eps > fabsf(y().dot(x()))).getAsBool()) {
		s_coord_sys_error = COORD_SYS_XY_NOT_PERPENDICULAR;
		return false;
	}
	if (!(eps > fabsf(z().dot(y()))).getAsBool()) {
		s_coord_sys_error = COORD_SYS_YZ_NOT_PERPENDICULAR;
		return false;
	}
	if (!(eps > fabsf(x().dot(z()))).getAsBool()) {
		s_coord_sys_error = COORD_SYS_ZX_NOT_PERPENDICULAR;
		return false;
	}
	if (!(_impl.getCol3().getW() == one).getAsBool()) {
		s_coord_sys_error = COORD_SYS_WW_NOT_1;
		return false;
	}
	if (!(_impl.getCol0().getW() == floatInVec(0.0f)).getAsBool() ||
	    !(_impl.getCol1().getW() == floatInVec(0.0f)).getAsBool() ||
	    !(_impl.getCol2().getW() == floatInVec(0.0f)).getAsBool()) {
		s_coord_sys_error = COORD_SYS_LAST_ROW_NOT_0001;
		return false;
	}
	return true;
}

/* Orthonormalizes x, then z = x cross y, then y = z cross x; w is kept. */
matrix matrix::fixedCoordSys() const
{
	v4 nx = x().unit();
	v4 nz = nx.cross(y()).unit();

	return matrix(nx, nz.cross(nx), nz, w());
}

namespace math_matrix {

string GetCoordSysError()
{
	string error;

	if (s_coord_sys_error & COORD_SYS_X_NAN)
		error += "MAT_X_NAN";
	if (s_coord_sys_error & COORD_SYS_Y_NAN)
		error += "MAT_Y_NAN";
	if (s_coord_sys_error & COORD_SYS_Z_NAN)
		error += "MAT_Z_NAN";
	if (s_coord_sys_error & COORD_SYS_W_NAN)
		error += "MAT_W_NAN";
	if (s_coord_sys_error & COORD_SYS_NOT_RIGHT_HANDED)
		error += "NOT_RIGHT_HANDED";
	if (s_coord_sys_error & COORD_SYS_X_NOT_UNIT_LENGTH)
		error += "X_NOT_UNIT_LENGTH";
	if (s_coord_sys_error & COORD_SYS_Y_NOT_UNIT_LENGTH)
		error += "Y_NOT_UNIT_LENGTH";
	if (s_coord_sys_error & COORD_SYS_Z_NOT_UNIT_LENGTH)
		error += "Z_NOT_UNIT_LENGTH";
	if (s_coord_sys_error & COORD_SYS_X_ZERO_LENGTH)
		error += "X_ZERO_LENGTH";
	if (s_coord_sys_error & COORD_SYS_Y_ZERO_LENGTH)
		error += "Y_ZERO_LENGTH";
	if (s_coord_sys_error & COORD_SYS_Z_ZERO_LENGTH)
		error += "Z_ZERO_LENGTH";
	if (s_coord_sys_error & COORD_SYS_XY_NOT_PERPENDICULAR)
		error += "XY_NOT_PERPENDICULAR";
	if (s_coord_sys_error & COORD_SYS_YZ_NOT_PERPENDICULAR)
		error += "YZ_NOT_PERPENDICULAR";
	if (s_coord_sys_error & COORD_SYS_ZX_NOT_PERPENDICULAR)
		error += "ZX_NOT_PERPENDICULAR";
	if (s_coord_sys_error & COORD_SYS_WW_NOT_1)
		error += "WW_NOT_1";
	if (s_coord_sys_error & COORD_SYS_LAST_ROW_NOT_0001)
		error += "LAST_ROW_NOT_0001";
	if (s_coord_sys_error & COORD_SYS_MAT_NOT_UNIFORM_SCALE)
		error += "MAT_NOT_UNIFORM_SCALE";
	return error;
}

}

string matrix::getCoordSysError()
{
	return math_matrix::GetCoordSysError();
}

/* asinf's argument, kept in [-1, 1]. */
static inline float ClampUnit(float value)
{
	value = value < -1.0f ? -1.0f : value;
	return value < 1.0f ? value : 1.0f;
}

/*
 * The angles of the rotation this matrix makes, for the rotations applied in
 * the order the name gives. At gimbal lock the last angle is 0.
 */
v3 matrix::extractEulerAnglesXYZ() const
{
	if (z().extract_x() < 1.0f) {
		if (z().extract_x() > -1.0f) {
			float ax = sce_paf_atan2f(-z().extract_y(), z().extract_z());
			float ay = sce_paf_asinf(ClampUnit(z().extract_x()));
			float az = sce_paf_atan2f(-y().extract_x(), x().extract_x());

			return v3(ax, ay, az);
		}
		return v3(-sce_paf_atan2f(x().extract_y(), y().extract_y()), -SCE_VECTORMATH_PI_OVER_2_F, 0.0f);
	}
	return v3(sce_paf_atan2f(x().extract_y(), y().extract_y()), SCE_VECTORMATH_PI_OVER_2_F, 0.0f);
}

v3 matrix::extractEulerAnglesXZY() const
{
	if (y().extract_x() < 1.0f) {
		if (y().extract_x() > -1.0f) {
			float ax = sce_paf_atan2f(y().extract_z(), y().extract_y());
			float ay = sce_paf_atan2f(z().extract_x(), x().extract_x());
			float az = sce_paf_asinf(ClampUnit(-y().extract_x()));

			return v3(ax, ay, az);
		}
		return v3(sce_paf_atan2f(x().extract_z(), z().extract_z()), 0.0f, SCE_VECTORMATH_PI_OVER_2_F);
	}
	return v3(sce_paf_atan2f(-x().extract_z(), z().extract_z()), 0.0f, -SCE_VECTORMATH_PI_OVER_2_F);
}

v3 matrix::extractEulerAnglesYXZ() const
{
	if (z().extract_y() < 1.0f) {
		if (z().extract_y() > -1.0f) {
			float ax = sce_paf_asinf(ClampUnit(-z().extract_y()));
			float ay = sce_paf_atan2f(z().extract_x(), z().extract_z());
			float az = sce_paf_atan2f(x().extract_y(), y().extract_y());

			return v3(ax, ay, az);
		}
		return v3(SCE_VECTORMATH_PI_OVER_2_F, sce_paf_atan2f(y().extract_x(), x().extract_x()), 0.0f);
	}
	return v3(-SCE_VECTORMATH_PI_OVER_2_F, sce_paf_atan2f(-y().extract_x(), x().extract_x()), 0.0f);
}

v3 matrix::extractEulerAnglesYZX() const
{
	if (x().extract_y() < 1.0f) {
		if (x().extract_y() > -1.0f) {
			float ax = sce_paf_atan2f(-z().extract_y(), y().extract_y());
			float ay = sce_paf_atan2f(-x().extract_z(), x().extract_x());
			float az = sce_paf_asinf(ClampUnit(x().extract_y()));

			return v3(ax, ay, az);
		}
		return v3(0.0f, -sce_paf_atan2f(y().extract_z(), z().extract_z()), -SCE_VECTORMATH_PI_OVER_2_F);
	}
	return v3(0.0f, sce_paf_atan2f(y().extract_z(), z().extract_z()), SCE_VECTORMATH_PI_OVER_2_F);
}

v3 matrix::extractEulerAnglesZXY() const
{
	if (y().extract_z() < 1.0f) {
		if (y().extract_z() > -1.0f) {
			float ax = sce_paf_asinf(ClampUnit(y().extract_z()));
			float ay = sce_paf_atan2f(-x().extract_z(), z().extract_z());
			float az = sce_paf_atan2f(-y().extract_x(), y().extract_y());

			return v3(ax, ay, az);
		}
		return v3(-SCE_VECTORMATH_PI_OVER_2_F, 0.0f, -sce_paf_atan2f(z().extract_x(), x().extract_x()));
	}
	return v3(SCE_VECTORMATH_PI_OVER_2_F, 0.0f, sce_paf_atan2f(z().extract_x(), x().extract_x()));
}

v3 matrix::extractEulerAnglesZYX() const
{
	if (x().extract_z() < 1.0f) {
		if (x().extract_z() > -1.0f) {
			float ax = sce_paf_atan2f(y().extract_z(), z().extract_z());
			float ay = sce_paf_asinf(ClampUnit(-x().extract_z()));
			float az = sce_paf_atan2f(x().extract_y(), x().extract_x());

			return v3(ax, ay, az);
		}
		return v3(0.0f, SCE_VECTORMATH_PI_OVER_2_F, -sce_paf_atan2f(y().extract_x(), z().extract_x()));
	}
	return v3(0.0f, -SCE_VECTORMATH_PI_OVER_2_F, sce_paf_atan2f(-y().extract_x(), -z().extract_x()));
}

}
}
