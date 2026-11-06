/*
 * The depth of field the renderer keeps (the programs "paf/cg/misc/dof/dob_init.fp",
 * "dob_spread.fp" and "dob_combine.fp"): its programs, its targets and the curve the blur
 * amount follows by depth. Only its constructor is in libpaf's code; the file's name is ours.
 */
#include <stdint.h>
#include <vectormath.h>
#include <common/shared_ptr.h>
#include <graphics/arl/gxm/gxm_shader.h>
#include <graphics/color.h>
#include <graphics/effects/post_effects.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>

using namespace sce::Vectormath::Simd::Aos;
using sce::Vectormath::Simd::floatInVec;

namespace paf {
namespace graph {

inline void HermiteCurve::Set(const math::v4 &points)
{
	Vector4 p = points.getImpl();

	m_points = points;
	m_coeffs = math::v4(Vector4(dot(p, Vector4(2.0f, -2.0f, 1.0f, 1.0f)),
				    dot(p, Vector4(-3.0f, 3.0f, -2.0f, -1.0f)), p.getZ(), p.getX()));

	/* the extrema cleared: their x and flag copied from fresh ones */
	Extremum none[2];

	for (int32_t i = 0; i < 2; i++) {
		m_extrema[i].m_x = none[i].m_x;
		m_extrema[i].m_valid = none[i].m_valid;
	}

	Vector4 c = m_coeffs.getImpl();
	/* the slope, 3a x^2 + 2b x + c */
	Vector4 slope = mulPerElem(c, Vector4(3.0f, 2.0f, 1.0f, 0.0f));
	floatInVec disc = slope.getY() * slope.getY() - floatInVec(4.0f) * slope.getX() * slope.getZ();

	if (!(disc < floatInVec(0.0f))) {
		floatInVec inv = recipf(floatInVec(2.0f) * slope.getX());
		floatInVec neg_b = -slope.getY();

		if (disc <= floatInVec(0.0f)) {
			m_extrema[0].m_x = math::v2(Vector2(neg_b * inv));
			m_extrema[0].m_valid = true;
		} else {
			floatInVec root = sce::Vectormath::Simd::sqrtf(disc);

			m_extrema[0].m_x = math::v2(Vector2((neg_b + root) * inv));
			m_extrema[0].m_valid = true;
			m_extrema[1].m_x = math::v2(Vector2((neg_b - root) * inv));
			m_extrema[1].m_valid = true;
		}
	}

	m_min = math::v2(minPerElem(Vector2(floatInVec(1.0f), p.getY()), Vector2(floatInVec(0.0f), p.getX())));
	m_max = math::v2(maxPerElem(Vector2(floatInVec(1.0f), p.getY()), Vector2(floatInVec(0.0f), p.getX())));
	for (int32_t i = 0; i < 2; i++) {
		Extremum &e = m_extrema[i];

		if (!e.m_valid)
			continue;

		Vector2 x = e.m_x.getImpl();
		Vector2 y = mulPerElem(mulPerElem(x * c.getX() + Vector2(c.getY()), x) + Vector2(c.getZ()), x) +
			    Vector2(c.getW());
		Vector2 point(x.getX(), y.getX());
		Vector2 at(x.getX());

		if (minPerElem(maxPerElem(at, Vector2(0.0f)), Vector2(1.0f)).getX() == at.getX()) {
			m_min = math::v2(minPerElem(point, m_min.getImpl()));
			m_max = math::v2(maxPerElem(point, m_max.getImpl()));
		}
	}
	m_inverse_range = math::v2(recipPerElem(Vector2(m_max.getImpl().getY()) - Vector2(m_min.getImpl().getY())));
}

DepthOfField::DepthOfField()
	: m_initialized(false), m_scale(1.0f, 1.0f), m_half_size(true), m_width(0), m_height(0),
	  m_curve(math::v4(0.308067f, 0.057868f, -1.217043f, 0.469729f))
{
	m_settings[0] = 16.4f;
	m_settings[1] = 1.0f;
	m_settings[2] = 3.0f;
	m_settings[3] = 5.0f;
	m_setting_flag = false;
	m_settings2[0] = 3.0f;
	m_settings2[1] = 5.0f;
	m_settings2[2] = 1.0f;
	m_settings2[3] = 1.0f;
}

}
}
