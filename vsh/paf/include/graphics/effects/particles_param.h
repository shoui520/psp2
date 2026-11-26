/*
 * Particles parameter sets ("Emit", "Mov", "Lit", "Focus", "Glare", "Tinkerbell")
 * as built in 2014 PAF (pup:psp2swu.self). Each set keeps a raw data prefix for
 * defaults and fast copying into ParticleEmitter alongside its Param<T> members.
 */
#ifndef PAF_GRAPHICS_EFFECTS_PARTICLES_PARAM_H
#define PAF_GRAPHICS_EFFECTS_PARTICLES_PARAM_H

#include <stdint.h>
#include <graphics/effects/param.h>
#include <graphics/effects/wave_param.h>
#include <math/ngp/v3.h>

namespace paf {
namespace graph {

/* raw emission parameters (64 bytes) */
struct EmitParamData {
	float m_vel_min;
	float m_vel_mul;
	float m_vel_var;
	float m_vel_zscale;
	float m_wait_time;
	float m_wait_time_var;
	float m_aging_speed;
	float m_aging_speed_var;
	math::v3 m_lifebounds_center;
	math::v3 m_lifebounds_size;
};

/* raw movement parameters (48 bytes) */
struct MovParamData {
	float m_friction;
	float m_spin_time_scale;
	float m_delta_time;
	float m_gravity;
	math::v3 m_wind_dir;
	float m_wind_scale;
	float m_brownian_scale;
	float m_brownian_scale_shake;
	float m_padding;
};

/* raw lighting parameters (64 bytes) */
struct LitParamData {
	math::v3 m_light_pos;
	math::v3 m_light_attn;
	float m_specular_power;
	float m_specular_coeff;
	float m_lambert_coeff;
	float m_exposure;
	float m_fresnel;
	float m_color_control;
	float m_alpha_unused;
	float m_global_alpha;
};

/* raw depth-of-field focus and sizing parameters (60 bytes) */
struct FocusParamData {
	float m_size_middle;
	float m_size_near;
	float m_size_far;
	float m_near_focus;
	float m_near_focus_dist;
	float m_near_focus_pow;
	float m_near_darkness;
	float m_near_fuzziness;
	float m_far_focus;
	float m_far_focus_dist;
	float m_far_focus_pow;
	float m_far_darkness;
	float m_near_align;
	float m_size_align;
	float m_global_align;
};

/* raw glare parameters (16 bytes) */
struct GlareParamData {
	float m_glare;
	float m_glare_scale;
	float m_glare_power;
	float m_glare_falloff;
};

/* "Emit" parameter set (288 bytes) */
class EmitParamSet : public ParamSet, public ParamLink {
public:
	class Values {
	public:
		Values(ParamSet *set);
		void CopyToRaw(EmitParamData &dst) const;

		EmitParamData m_defaults;
		Param<float> m_vel_min;
		Param<float> m_vel_mul;
		Param<float> m_vel_var;
		Param<float> m_vel_zscale;
		Param<float> m_wait_time;
		Param<float> m_wait_time_var;
		Param<float> m_aging_speed;
		Param<float> m_aging_speed_var;
		Param<math::v3> m_lifebounds_center;
		Param<math::v3> m_lifebounds_size;
	};

	WAVE_PARAM_SET_KEYED(EmitParamSet, "Emit", __attribute__((noinline)));
};

/* "Mov" parameter set (240 bytes) */
class MovParamSet : public ParamSet, public ParamLink {
public:
	class Values {
	public:
		Values(ParamSet *set);
		void CopyToRaw(MovParamData &dst) const;

		MovParamData m_defaults;
		Param<float> m_friction;
		Param<float> m_spin_time_scale;
		Param<float> m_delta_time;
		Param<float> m_gravity;
		Param<math::v3> m_wind_dir;
		Param<float> m_wind_scale;
		Param<float> m_wind_scale_ten;
		Param<float> m_brownian_scale;
		Param<float> m_brownian_scale_shake;
	};

	WAVE_PARAM_SET_KEYED(MovParamSet, "Mov", __attribute__((noinline)));
};

/* "Lit" parameter set (280 bytes) */
class LitParamSet : public ParamSet, public ParamLink {
public:
	class Values {
	public:
		Values(ParamSet *set);
		void CopyToRaw(LitParamData &dst) const;

		LitParamData m_defaults;
		Param<math::v3> m_light_pos;
		Param<math::v3> m_light_attn;
		Param<float> m_specular_power;
		Param<float> m_specular_coeff;
		Param<float> m_lambert_coeff;
		Param<float> m_exposure;
		Param<float> m_fresnel;
		Param<float> m_color_control;
		Param<float> m_global_alpha;
	};

	WAVE_PARAM_SET_KEYED(LitParamSet, "Lit", __attribute__((noinline)));
};

/* "Focus" parameter set (304 bytes) */
class FocusParamSet : public ParamSet, public ParamLink {
public:
	class Values {
	public:
		Values(ParamSet *set);
		void CopyToRaw(FocusParamData &dst) const;

		FocusParamData m_defaults;
		Param<float> m_size_middle;
		Param<float> m_size_near;
		Param<float> m_size_far;
		Param<float> m_near_focus;
		Param<float> m_near_focus_dist;
		Param<float> m_near_focus_pow;
		Param<float> m_near_darkness;
		Param<float> m_near_fuzziness;
		Param<float> m_far_focus;
		Param<float> m_far_focus_dist;
		Param<float> m_far_focus_pow;
		Param<float> m_far_darkness;
		Param<float> m_near_align;
		Param<float> m_size_align;
		Param<float> m_global_align;
	};

	WAVE_PARAM_SET_KEYED(FocusParamSet, "Focus", __attribute__((noinline)));
};

/* "Glare" parameter set (128 bytes) */
class GlareParamSet : public ParamSet, public ParamLink {
public:
	class Values {
	public:
		Values(ParamSet *set);
		void CopyToRaw(GlareParamData &dst) const;

		GlareParamData m_defaults;
		Param<float> m_glare;
		Param<float> m_glare_scale;
		Param<float> m_glare_power;
		Param<float> m_glare_falloff;
	};

	WAVE_PARAM_SET_KEYED(GlareParamSet, "Glare", __attribute__((noinline)));
};

/* "Tinkerbell" parameter set (76 bytes) */
class TinkerbellParamSet : public ParamSet, public ParamLink {
public:
	class Values {
	public:
		Values(ParamSet *set);

		Param<float> m_fade;
	};

	WAVE_PARAM_SET_KEYED(TinkerbellParamSet, "Tinkerbell", __attribute__((noinline)));
};

}
}

#endif
