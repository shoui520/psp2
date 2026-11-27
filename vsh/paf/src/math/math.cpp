/* paf::math: angles turned part of the way toward others (names ours). */
#include <math/math.h>
#include <graphics/signed_angle.h>
#include "../std/math.h"

using sce::Vectormath::Simd::floatInVec;
using sce::Vectormath::Simd::Aos::Vector2;

namespace paf {
namespace math {

/* the turn from a to b, the shortest way round, taken between their directions */
float LerpAngle(float a, float b, float t)
{
	v2 from(sce_paf_cosf(a), sce_paf_sinf(a));
	v2 to(sce_paf_cosf(b), sce_paf_sinf(b));
	float angle = graph::GetSignedAngle(from, to) * t;
	float s = sce_paf_sinf(angle);
	float c = sce_paf_cosf(angle);
	Vector2 dir = from.getImpl();
	floatInVec x = floatInVec(c) * dir.getX() - floatInVec(s) * dir.getY();
	floatInVec y = floatInVec(s) * dir.getX() + floatInVec(c) * dir.getY();

	return sce_paf_atan2f(y.getAsFloat(), x.getAsFloat());
}

v2 LerpAngle(v2 a, v2 b, v2 t)
{
	float x = LerpAngle(a.extract_x(), b.extract_x(), t.extract_x());
	float y = LerpAngle(a.extract_y(), b.extract_y(), t.extract_y());

	return v2(x, y);
}

v3 LerpAngle(v3 a, v3 b, v3 t)
{
	float x = LerpAngle(a.extract_x(), b.extract_x(), t.extract_x());
	float y = LerpAngle(a.extract_y(), b.extract_y(), t.extract_y());
	float z = LerpAngle(a.extract_z(), b.extract_z(), t.extract_z());

	return v3(x, y, z);
}

v4 LerpAngle(v4 a, v4 b, v4 t)
{
	float x = LerpAngle(a.extract_x(), b.extract_x(), t.extract_x());
	float y = LerpAngle(a.extract_y(), b.extract_y(), t.extract_y());
	float z = LerpAngle(a.extract_z(), b.extract_z(), t.extract_z());
	float w = LerpAngle(a.extract_w(), b.extract_w(), t.extract_w());

	return v4(x, y, z, w);
}

}
}
