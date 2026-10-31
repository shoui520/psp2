/* paf::math: vectors, matrices and quaternions over the SDK's vectormath. */
#ifndef PAF_MATH_MATH_H
#define PAF_MATH_MATH_H

#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <math/ngp/matrix.h>
#include <math/ngp/quaternion.h>

namespace paf {
namespace math {

/* An argument of asinf or acosf, kept in [-1, 1]. */
static inline float ClampUnit(float value)
{
	value = value < -1.0f ? -1.0f : value;
	return value < 1.0f ? value : 1.0f;
}

}
}

#endif
