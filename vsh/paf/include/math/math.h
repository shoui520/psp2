/* paf::math: vectors, matrices and quaternions over the SDK's vectormath. */
#ifndef PAF_MATH_MATH_H
#define PAF_MATH_MATH_H

#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <math/ngp/matrix.h>
#include <math/ngp/quaternion.h>
#include <math/rect.h>

namespace paf {
namespace math {

/* An argument of asinf or acosf, kept in [-1, 1]. */
static inline float ClampUnit(float value)
{
	value = value < -1.0f ? -1.0f : value;
	return value < 1.0f ? value : 1.0f;
}

/* the angle a turned toward b by t of the way between them, the shortest way round, in each
   component (names ours; 0x81185298, 0x8118535e, 0x811853b2, 0x8118543c) */
float LerpAngle(float a, float b, float t);
v2 LerpAngle(v2 a, v2 b, v2 t);
v3 LerpAngle(v3 a, v3 b, v3 t);
v4 LerpAngle(v4 a, v4 b, v4 t);

}
}

#endif
