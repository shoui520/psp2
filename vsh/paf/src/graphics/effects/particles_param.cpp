/*
 * Particles parameter set constructors and raw-data copies (2014 PAF).
 */
#include <stdint.h>
#include <graphics/color.h>
#include <graphics/effects/particles_param.h>

namespace paf {
namespace graph {

#ifdef PAF_BUILD_2014
static inline __attribute__((always_inline)) EmitParamData MakeEmitDefaults()
#else
static EmitParamData MakeEmitDefaults()
#endif
{
	EmitParamData defaults;

	defaults.m_vel_min = 0.001f;
	defaults.m_vel_mul = 0.19f;
	defaults.m_vel_var = 0.05f;
	defaults.m_vel_zscale = 0.5f;
	defaults.m_wait_time = 2.0f;
	defaults.m_wait_time_var = 0.15f;
	defaults.m_aging_speed = 0.001f;
	defaults.m_aging_speed_var = 0.0f;
	defaults.m_lifebounds_center = math::v3(0.0f, 0.0f, 0.0f);
	defaults.m_lifebounds_size = math::v3(5.0f, 5.0f, 5.0f);
	return defaults;
}

EmitParamSet::Values::Values(ParamSet *set)
	: m_defaults(MakeEmitDefaults()),
	  m_vel_min(set, "vel_min", m_defaults.m_vel_min, 0.0f, 1.0f, PARAM_FLAG_FINE),
	  m_vel_mul(set, "vel_mul", m_defaults.m_vel_mul, 0.0f, 2.0f, PARAM_FLAG_FINE),
	  m_vel_var(set, "vel_var", m_defaults.m_vel_var, 0.0f, 1.0f, PARAM_FLAG_FINE),
	  m_vel_zscale(set, "vel_zscale", m_defaults.m_vel_zscale, 0.0f, 1.0f, 0),
	  m_wait_time(set, "wait_time", m_defaults.m_wait_time, 1.0f / 6000.0f, 5.0f,
		      PARAM_FLAG_FINE),
	  m_wait_time_var(set, "wait_time_var", m_defaults.m_wait_time_var, 0.0f, 0.5f, 0),
	  m_aging_speed(set, "aging_speed", m_defaults.m_aging_speed, 0.0001f, 0.05f,
			PARAM_FLAG_FINE),
	  m_aging_speed_var(set, "aging_speed_var", m_defaults.m_aging_speed_var, 0.0f, 0.5f, 0),
	  m_lifebounds_center(set, "lifebounds_center", m_defaults.m_lifebounds_center,
			      math::v3(0.0f, -10.0f, -10.0f), math::v3(0.0f, 10.0f, 10.0f), 0),
	  m_lifebounds_size(set, "lifebounds_size", m_defaults.m_lifebounds_size,
			    math::v3(0.0f), math::v3(10.0f), 0)
{
}

void EmitParamSet::Values::CopyToRaw(EmitParamData &dst) const
{
	dst.m_vel_min = m_vel_min.m_value;
	dst.m_vel_mul = m_vel_mul.m_value;
	dst.m_vel_var = m_vel_var.m_value;
	dst.m_vel_zscale = m_vel_zscale.m_value;
	dst.m_wait_time = m_wait_time.m_value;
	dst.m_wait_time_var = m_wait_time_var.m_value;
	dst.m_aging_speed = m_aging_speed.m_value;
	dst.m_aging_speed_var = m_aging_speed_var.m_value;
	dst.m_lifebounds_center = m_lifebounds_center.m_value;
	dst.m_lifebounds_size = m_lifebounds_size.m_value;
}

static MovParamData MakeMovDefaults()
{
	MovParamData defaults;

	defaults.m_friction = 0.030551f;
	defaults.m_spin_time_scale = 2.74f;
	defaults.m_delta_time = 0.03f;
	defaults.m_gravity = -0.000068f;
	defaults.m_wind_dir = math::v3(0.152f, 0.0f, 0.35f);
	defaults.m_wind_scale = 0.004f;
	defaults.m_brownian_scale = 0.0025f;
	defaults.m_brownian_scale_shake = 6.0f;
	defaults.m_padding = 0.0f;
	return defaults;
}

MovParamSet::Values::Values(ParamSet *set)
	: m_defaults(MakeMovDefaults()),
	  m_friction(set, "friction", m_defaults.m_friction, 0.01f, 0.4f, PARAM_FLAG_FINE),
	  m_spin_time_scale(set, "spin_time_scale", m_defaults.m_spin_time_scale, 1.0f, 5.0f,
			    PARAM_FLAG_FINE),
	  m_delta_time(set, "delta_time", m_defaults.m_delta_time, 1.0f / 1200.0f, 1.0f / 12.0f,
		       PARAM_FLAG_FINE),
	  m_gravity(set, "gravity", m_defaults.m_gravity, -0.0002f, 0.0f, 0),
	  m_wind_dir(set, "wind_dir", m_defaults.m_wind_dir, math::v3(-0.5f), math::v3(0.5f), 0),
	  m_wind_scale(set, "wind_scale", m_defaults.m_wind_scale, 0.0f, 1.0f, PARAM_FLAG_FINE),
	  m_wind_scale_ten(set, "wind_scale_10", 0.0f, 0.0f, 1.0f, PARAM_FLAG_FINE),
	  m_brownian_scale(set, "brownian_scale", m_defaults.m_brownian_scale, 0.0f, 7.0f,
			   PARAM_FLAG_FINE),
	  m_brownian_scale_shake(set, "brownian_scale_shake", m_defaults.m_brownian_scale_shake,
				 0.0f, m_defaults.m_brownian_scale_shake * 2.0f, 0)
{
}

void MovParamSet::Values::CopyToRaw(MovParamData &dst) const
{
	dst.m_friction = m_friction.m_value;
	dst.m_spin_time_scale = m_spin_time_scale.m_value;
	dst.m_delta_time = m_delta_time.m_value;
	dst.m_gravity = m_gravity.m_value;
	dst.m_wind_dir = m_wind_dir.m_value;
	dst.m_wind_scale = m_wind_scale.m_value + m_wind_scale_ten.m_value * 10.0f;
	dst.m_brownian_scale = m_brownian_scale.m_value;
	dst.m_brownian_scale_shake = m_brownian_scale_shake.m_value;
}

#ifdef PAF_BUILD_2014
static inline __attribute__((always_inline)) LitParamData MakeLitDefaults()
#else
static LitParamData MakeLitDefaults()
#endif
{
	LitParamData defaults;

	defaults.m_light_pos = math::v3(4.16f, 2.63f, -7.6f);
	defaults.m_light_attn = math::v3(1.0f, 0.0f, 0.0f);
	defaults.m_specular_power = 13.94f;
	defaults.m_specular_coeff = 74.74f;
	defaults.m_lambert_coeff = 8.0f;
	defaults.m_exposure = 0.0537f;
	defaults.m_fresnel = 1.24f;
	defaults.m_color_control = 1.0f;
	defaults.m_alpha_unused = 1.0f;
	defaults.m_global_alpha = 0.93f;
	return defaults;
}

LitParamSet::Values::Values(ParamSet *set)
	: m_defaults(MakeLitDefaults()),
	  m_light_pos(set, "light_pos", m_defaults.m_light_pos, math::v3(-10.0f), math::v3(10.0f),
		      0),
	  m_light_attn(set, "light_attn", m_defaults.m_light_attn, math::v3(0.0f), math::v3(1.0f),
		       PARAM_FLAG_FINE),
	  m_specular_power(set, "specular_power", m_defaults.m_specular_power, 0.0f, 100.0f,
			   PARAM_FLAG_FINE),
	  m_specular_coeff(set, "specular_coeff", m_defaults.m_specular_coeff, 0.0f, 800.0f,
			   PARAM_FLAG_FINE),
	  m_lambert_coeff(set, "lambert_coeff", m_defaults.m_lambert_coeff, 0.0f, 80.0f,
			  PARAM_FLAG_FINE),
	  m_exposure(set, "exposure", m_defaults.m_exposure, 0.0f, 10.0f, PARAM_FLAG_FINE),
	  m_fresnel(set, "fresnel", m_defaults.m_fresnel, 0.0f, 4.0f, 0),
	  m_color_control(set, "color_control", m_defaults.m_color_control, 0.0f, 1.0f, 0),
	  m_global_alpha(set, "global_alpha", m_defaults.m_global_alpha, 0.0f, 1.0f, 0)
{
}

void LitParamSet::Values::CopyToRaw(LitParamData &dst) const
{
	dst.m_light_pos = m_light_pos.m_value;
	dst.m_light_attn = m_light_attn.m_value;
	dst.m_specular_power = m_specular_power.m_value;
	dst.m_specular_coeff = m_specular_coeff.m_value;
	dst.m_lambert_coeff = m_lambert_coeff.m_value;
	dst.m_exposure = m_exposure.m_value;
	dst.m_fresnel = m_fresnel.m_value;
	dst.m_color_control = m_color_control.m_value;
	dst.m_global_alpha = m_global_alpha.m_value;
}

#ifdef PAF_BUILD_2014
static inline __attribute__((always_inline)) FocusParamData MakeFocusDefaults()
#else
static FocusParamData MakeFocusDefaults()
#endif
{
	FocusParamData defaults;

	defaults.m_size_middle = 0.0845f;
	defaults.m_size_near = 0.17f;
	defaults.m_size_far = 0.182f;
	defaults.m_near_focus = 3.88f;
	defaults.m_near_focus_dist = 3.4f;
	defaults.m_near_focus_pow = 3.05f;
	defaults.m_near_darkness = 1.0f;
	defaults.m_near_fuzziness = 3.79f;
	defaults.m_far_focus = 10.35f;
	defaults.m_far_focus_dist = 2.4f;
	defaults.m_far_focus_pow = 1.72f;
	defaults.m_far_darkness = 13.9f;
	defaults.m_near_align = 0.722f;
	defaults.m_size_align = 13.19f;
	defaults.m_global_align = 0.0f;
	return defaults;
}

FocusParamSet::Values::Values(ParamSet *set)
	: m_defaults(MakeFocusDefaults()),
	  m_size_middle(set, "size_middle", m_defaults.m_size_middle, 0.005f, 0.6f,
			PARAM_FLAG_FINE),
	  m_size_near(set, "size_near", m_defaults.m_size_near, 0.005f, 0.6f, PARAM_FLAG_FINE),
	  m_size_far(set, "size_far", m_defaults.m_size_far, 0.005f, 2.0f, PARAM_FLAG_FINE),
	  m_near_focus(set, "near_focus", m_defaults.m_near_focus, 0.0f, 7.0f, 0),
	  m_near_focus_dist(set, "near_focus_dist", m_defaults.m_near_focus_dist, 0.0f, 10.0f, 0),
	  m_near_focus_pow(set, "near_focus_pow", m_defaults.m_near_focus_pow, 1.0f, 5.0f, 0),
	  m_near_darkness(set, "near_darkness", m_defaults.m_near_darkness, 0.0f, 10.0f,
			  PARAM_FLAG_FINE),
	  m_near_fuzziness(set, "near_fuzziness", m_defaults.m_near_fuzziness, 0.0f, 10.0f,
			   PARAM_FLAG_FINE),
	  m_far_focus(set, "far_focus", m_defaults.m_far_focus, 0.0f, 10.0f, 0),
	  m_far_focus_dist(set, "far_focus_dist", m_defaults.m_far_focus_dist, 0.0f, 10.0f, 0),
	  m_far_focus_pow(set, "far_focus_pow", m_defaults.m_far_focus_pow, 1.0f, 5.0f, 0),
	  m_far_darkness(set, "far_darkness", m_defaults.m_far_darkness, 0.0f, 80.0f,
			 PARAM_FLAG_FINE),
	  m_near_align(set, "near_align", m_defaults.m_near_align, 0.0f, 1.0f, 0),
	  m_size_align(set, "size_align", m_defaults.m_size_align, 0.0f, 20.0f, 0),
	  m_global_align(set, "global_align", m_defaults.m_global_align, 0.0f, 1.0f, 0)
{
}

void FocusParamSet::Values::CopyToRaw(FocusParamData &dst) const
{
	dst.m_size_middle = m_size_middle.m_value;
	dst.m_size_near = m_size_near.m_value;
	dst.m_size_far = m_size_far.m_value;
	dst.m_near_focus = m_near_focus.m_value;
	dst.m_near_focus_dist = m_near_focus_dist.m_value;
	dst.m_near_focus_pow = m_near_focus_pow.m_value;
	dst.m_near_darkness = m_near_darkness.m_value;
	dst.m_near_fuzziness = m_near_fuzziness.m_value;
	dst.m_far_focus = m_far_focus.m_value;
	dst.m_far_focus_dist = m_far_focus_dist.m_value;
	dst.m_far_focus_pow = m_far_focus_pow.m_value;
	dst.m_far_darkness = m_far_darkness.m_value;
	dst.m_near_align = m_near_align.m_value;
	dst.m_size_align = m_size_align.m_value;
	dst.m_global_align = m_global_align.m_value;
}

static GlareParamData MakeGlareDefaults()
{
	GlareParamData defaults;

	defaults.m_glare = 1.0f;
	defaults.m_glare_scale = 4.16f;
	defaults.m_glare_power = 0.99f;
	defaults.m_glare_falloff = 4.44f;
	return defaults;
}

GlareParamSet::Values::Values(ParamSet *set)
	: m_defaults(MakeGlareDefaults()),
	  m_glare(set, "glare", m_defaults.m_glare, 0.0f, 1.0f, 0),
	  m_glare_scale(set, "glare_scale", m_defaults.m_glare_scale, 0.0f, 8.0f, 0),
	  m_glare_power(set, "glare_p1", m_defaults.m_glare_power, 0.0f, 20.0f, 0),
	  m_glare_falloff(set, "glare_p2", m_defaults.m_glare_falloff, 0.0f, 20.0f, 0)
{
}

void GlareParamSet::Values::CopyToRaw(GlareParamData &dst) const
{
	dst.m_glare = m_glare.m_value;
	dst.m_glare_scale = m_glare_scale.m_value;
	dst.m_glare_power = m_glare_power.m_value;
	dst.m_glare_falloff = m_glare_falloff.m_value;
}

TinkerbellParamSet::Values::Values(ParamSet *set)
	: m_fade(set, "fade", 0.106f, 0.0f, 1.0f, PARAM_FLAG_FINE)
{
}

}
}
