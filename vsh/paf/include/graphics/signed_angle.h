/* paf::graph::GetSignedAngle: the angle between two directions (an inline function Sony's link
   keeps once, at 0x8120431c in 3.650.011-TOOL; name ours). */
#ifndef PAF_GRAPHICS_SIGNED_ANGLE_H
#define PAF_GRAPHICS_SIGNED_ANGLE_H

#include <vectormath.h>
#include <math/math.h>
#include <src/std/math.h>

namespace paf {
namespace graph {

/* the angle from from to to, negative the other way round */
inline __attribute__((noinline)) float GetSignedAngle(const math::v2 &from, const math::v2 &to)
{
	sce::Vectormath::Simd::Aos::Vector2 a =
		from.getImpl() * sce::Vectormath::Simd::recipf(length(from.getImpl()));
	sce::Vectormath::Simd::Aos::Vector2 b =
		to.getImpl() * sce::Vectormath::Simd::recipf(length(to.getImpl()));
	float angle = sce_paf_acosf(math::ClampUnit(dot(b, a).getAsFloat()));
	sce::Vectormath::Simd::floatInVec cross = sce::Vectormath::Simd::floatInVec(
		to.extract_y() * from.extract_x() - to.extract_x() * from.extract_y());

	return angle * sce::Vectormath::Simd::select(sce::Vectormath::Simd::floatInVec(-1.0f),
						     sce::Vectormath::Simd::floatInVec(1.0f),
						     cross > sce::Vectormath::Simd::floatInVec(0.0f))
			       .getAsFloat();
}

}
}

#endif
