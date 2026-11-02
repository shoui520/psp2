/*
 * The wave renderer's parameter sets, as Sony's parameter code generator writes them: each set
 * names its parameters (the names, ranges and flags its constructor gives), keeps their values in
 * m_values and copies them between its keys. The sets' names ("Material", "PointLightSphere",
 * "Fog", "Sky", "FFTWave", "WaveInstance", "WaveRenderer", "WaveTransition") and the parameters'
 * are Sony's strings; the classes', the members' and the file's names are ours.
 */
#ifndef PAF_GRAPHICS_EFFECTS_WAVE_PARAM_H
#define PAF_GRAPHICS_EFFECTS_WAVE_PARAM_H

#include <stdint.h>
#include <graphics/color.h>
#include <graphics/effects/fft_wave.h>
#include <graphics/effects/param.h>
#include <math/ngp/matrix.h>
#include <math/ngp/quaternion.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>

namespace paf {
namespace graph {

/* pi, and the angles in the sets' ranges in radians */
#define WAVE_PARAM_PI		3.14159274f
#define WAVE_PARAM_HALF_PI	1.57079637f

/*
 * What every generated set has: its name, a constructor that gives its parameters keys when
 * add is set, the values copied into another set of its kind, and the values of a key set
 * through the saved key so the others keep theirs.
 */
#define WAVE_PARAM_SET_KEYED(Set, name, key_attr)						\
public:											\
	static const char *TypeName()							\
	{										\
		return name;								\
	}										\
											\
	__attribute__((noinline))							\
	Set(bool add) : ParamSet(TypeName()), m_values(add ? this : NULL)		\
	{										\
	}										\
											\
	virtual ~Set()									\
	{										\
	}										\
											\
	virtual void CopyValues(ParamSet *dst)						\
	{										\
		static_cast<Set *>(dst)->m_values = m_values;				\
	}										\
											\
	key_attr void SetKey(int32_t key, const Values &values)				\
	{										\
		if (key != PARAM_KEY_CURRENT) {						\
			CopyKey(PARAM_KEY_SAVED, PARAM_KEY_CURRENT);		\
			m_values = values;						\
			CopyKey(key, PARAM_KEY_CURRENT);				\
			CopyKey(PARAM_KEY_CURRENT, PARAM_KEY_SAVED);		\
		} else {								\
			m_values = values;						\
		}									\
	}										\
											\
	Values m_values

/* Sony keeps every set's SetKey out of line but the sky's */
#define WAVE_PARAM_SET(Set, name)	WAVE_PARAM_SET_KEYED(Set, name, __attribute__((noinline)))

/* a material's light response */
class MaterialParamSet : public ParamSet, public ParamLink {
public:
	class Values {
	public:
		Values(ParamSet *set)
			: m_color(set, "color", math::v4(1.0f, 1.0f, 1.0f, 1.0f), s_color_black,
				  s_color_white, PARAM_FLAG_COLOR),
			  m_diffuse_coeff(set, "diffuse_coeff", 0.4f, 0.0f, 1.0f, 0),
			  m_specular_coeff(set, "specular_coeff", 0.7f, 0.0f, 10.0f, PARAM_FLAG_FINE),
			  m_specular_power(set, "specular_power", 40.0f, 0.8f, 100.0f, 0),
			  m_fresnel_front(set, "fresnel_front", 0.0f, 0.0f, 1.0f, 0),
			  m_fresnel_control(set, "fresnel_control", 0.2f, 0.0f, 0.999f, PARAM_FLAG_FINE),
			  m_fresnel_coeff(set, "fresnel_coeff", 1.0f, 0.0f, 4.0f, PARAM_FLAG_FINE),
			  m_refraction_coeff(set, "refraction_coeff", 1.0f, 0.0f, 8.0f, PARAM_FLAG_FINE),
			  m_refraction_amount(set, "refraction_amount", 0.0f, -0.5f, 0.5f, 0),
			  m_second_reflection(set, "second_reflection", 0.0f, 0.0f, 1.3f,
					      PARAM_FLAG_FINE)
		{
		}

		Param<math::v4> m_color;
		Param<float> m_diffuse_coeff;
		Param<float> m_specular_coeff;
		Param<float> m_specular_power;
		Param<float> m_fresnel_front;
		Param<float> m_fresnel_control;
		Param<float> m_fresnel_coeff;
		Param<float> m_refraction_coeff;
		Param<float> m_refraction_amount;
		Param<float> m_second_reflection;

		/* the values packed for the lighting program's MaterialPack (name ours) */
		math::matrix GetPack() const
		{
			return math::matrix(m_color.m_value,
					    math::v4(m_diffuse_coeff.m_value, m_specular_coeff.m_value,
						     m_specular_power.m_value, 0.0f),
					    math::v4(m_fresnel_front.m_value, m_fresnel_control.m_value,
						     m_fresnel_coeff.m_value, 0.0f),
					    math::v4(m_refraction_coeff.m_value, 0.0f,
						     m_refraction_amount.m_value,
						     m_second_reflection.m_value));
		}
	};

	WAVE_PARAM_SET(MaterialParamSet, "Material");
};

/* a point light on a sphere round the waves */
class PointLightSphereParamSet : public ParamSet, public ParamLink {
public:
	class Values {
	public:
		Values(ParamSet *set)
			: m_theta_phi(set, "theta_phi", math::v2(2.3561945f, 0.0f),
				      math::v2(0.0f, -WAVE_PARAM_PI), math::v2(WAVE_PARAM_PI, WAVE_PARAM_PI),
				      PARAM_FLAG_DEGREES | PARAM_FLAG_ANGLE),
			  m_distance(set, "distance", 4.0f, 0.0f, 40.0f, PARAM_FLAG_FINE),
			  m_attn(set, "attn", math::v3(1.0f, 0.0f, 0.0f), math::v3(0.0f),
				 math::v3(1.0f), PARAM_FLAG_FINE),
			  m_ambient_color(set, "ambient_color", math::v3(0.2f),
					  math::v3(0.0f), math::v3(1.0f),
					  PARAM_FLAG_COLOR),
			  m_diffuse_color(set, "diffuse_color", math::v3(1.0f),
					  math::v3(0.0f), math::v3(1.0f),
					  PARAM_FLAG_COLOR),
			  m_fade(set, "fade", 1.0f, 0.0f, 1.0f, 0)
		{
		}

		Param<math::v2> m_theta_phi;
		Param<float> m_distance;
		Param<math::v3> m_attn;
		Param<math::v3> m_ambient_color;
		Param<math::v3> m_diffuse_color;
		Param<float> m_fade;

		/* the light's place, from its angles and distance, and its attenuation, for the
		   lighting program's PointLightPack1 (0x812040d0, name ours) */
		__attribute__((noinline)) math::matrix GetPack1() const;

		/* its colours and fade, for PointLightPack2 (name ours) */
		math::matrix GetPack2() const
		{
			return math::matrix(math::v4(m_ambient_color.m_value, 1.0f),
					    math::v4(0.0f, 0.0f, 0.0f, 0.0f),
					    math::v4(m_diffuse_color.m_value, 1.0f),
					    math::v4(sce::Vectormath::Simd::Aos::Vector4(m_fade.m_value)));
		}
	};

	WAVE_PARAM_SET(PointLightSphereParamSet, "PointLightSphere");
};

/* the fog over the waves */
class FogParamSet : public ParamSet, public ParamLink {
public:
	class Values {
	public:
		Values(ParamSet *set)
			: m_color(set, "color", math::v3(1.0f), math::v3(0.0f),
				  math::v3(1.0f), PARAM_FLAG_COLOR),
			  m_density(set, "density", math::v4(0.0f, 0.0f, 0.0f, 1.0f),
				    math::v4(0.0f, 0.0f, 0.0f, 0.0f), math::v4(1.0f, 1.0f, 1.0f, 10.0f),
				    PARAM_FLAG_COLOR | PARAM_FLAG_FINE),
			  m_light_color(set, "light_color", math::v4(1.0f, 1.0f, 1.0f, 1.0f),
					math::v4(0.0f, 0.0f, 0.0f, 0.0f),
					math::v4(1.0f, 1.0f, 1.0f, 1.0f), PARAM_FLAG_COLOR),
			  m_d_scale(set, "d_scale", 1.0f, 0.0f, 50.0f, PARAM_FLAG_FINE),
			  m_d_offset(set, "d_offset", 0.0f, -20.0f, 20.0f, PARAM_FLAG_FINE)
		{
		}

		Param<math::v3> m_color;
		Param<math::v4> m_density;
		Param<math::v4> m_light_color;
		Param<float> m_d_scale;
		Param<float> m_d_offset;

		/* the colours, density and distances packed for the lighting program's FogPack
		   (0x8120419c, name ours) */
		__attribute__((noinline)) math::matrix GetPack() const;
	};

	WAVE_PARAM_SET(FogParamSet, "Fog");
};

/* the sky dome and its sun */
class SkyParamSet : public ParamSet, public ParamLink {
public:
	class Values {
	public:
		Values(ParamSet *set)
			: m_theta_phi(set, "theta_phi", math::v2(2.3561945f, 0.0f),
				      math::v2(0.0f, -WAVE_PARAM_PI), math::v2(WAVE_PARAM_PI, WAVE_PARAM_PI),
				      PARAM_FLAG_DEGREES | PARAM_FLAG_ANGLE),
			  m_distance(set, "distance", 120.0f, 0.0f, 200.0f, PARAM_FLAG_FINE),
			  m_sun_color(set, "sun_color", math::v3(1.0f),
				      math::v3(0.0f), math::v3(1.0f),
				      PARAM_FLAG_COLOR),
			  m_sun_power(set, "sun_power", 50.0f, 1.0f, 80.0f, PARAM_FLAG_FINE),
			  m_sun_control(set, "sun_control", 1.0f, 0.0f, 4.0f, 0),
			  m_zenith_color(set, "zenith_color", math::v3(0.0f),
					 math::v3(0.0f), math::v3(1.0f),
					 PARAM_FLAG_COLOR),
			  m_horizon_color(set, "horizon_color", math::v3(0.0f),
					  math::v3(0.0f), math::v3(1.0f),
					  PARAM_FLAG_COLOR),
			  m_wave_fog_alpha(set, "wave_fog_alpha", 0.0f, 0.0f, 1.0f, 0),
			  m_radius(set, "radius", 8.36f, 0.2f, 10.0f, PARAM_FLAG_FINE),
			  m_xscale(set, "xscale", 0.6f, 0.0f, 4.0f, 0),
			  m_side_angle(set, "side_angle", 0.69813168f, 0.0174532924f, WAVE_PARAM_HALF_PI,
				       PARAM_FLAG_DEGREES),
			  m_horizon_angle(set, "horizon_angle", 0.122173049f, 0.0f, 0.52359879f,
					  PARAM_FLAG_DEGREES),
			  m_horizon_blend_range(set, "horizon_blend_range", 0.0f, 0.0f, 1.0f,
						PARAM_FLAG_FINE),
			  m_horizon_curvature(set, "horizon_curvature", 0.17453292f, 0.0f, 0.52359879f,
					      PARAM_FLAG_DEGREES),
			  m_sky_blend_start(set, "sky_blend_start", 0.488692194f, 0.0f,
					    WAVE_PARAM_HALF_PI, PARAM_FLAG_DEGREES),
			  m_sky_blend_range(set, "sky_blend_range", 0.139626339f, 0.0174532924f,
					    WAVE_PARAM_HALF_PI, PARAM_FLAG_DEGREES),
			  m_pos_distortion_scale(set, "pos_distortion_scale", 0.2f, 0.0f, 1.0f,
						 PARAM_FLAG_FINE),
			  m_grad_distortion_scale(set, "grad_distortion_scale", 0.2f, 0.0f, 1.0f,
						  PARAM_FLAG_FINE),
			  m_horizon_distortion_scale(set, "horizon_distortion_scale", 0.2f, 0.0f, 1.0f,
						     PARAM_FLAG_FINE)
		{
		}

		Param<math::v2> m_theta_phi;
		Param<float> m_distance;
		Param<math::v3> m_sun_color;
		Param<float> m_sun_power;
		Param<float> m_sun_control;
		Param<math::v3> m_zenith_color;
		Param<math::v3> m_horizon_color;
		Param<float> m_wave_fog_alpha;
		Param<float> m_radius;
		Param<float> m_xscale;
		Param<float> m_side_angle;
		Param<float> m_horizon_angle;
		Param<float> m_horizon_blend_range;
		Param<float> m_horizon_curvature;
		Param<float> m_sky_blend_start;
		Param<float> m_sky_blend_range;
		Param<float> m_pos_distortion_scale;
		Param<float> m_grad_distortion_scale;
		Param<float> m_horizon_distortion_scale;
	};

	WAVE_PARAM_SET_KEYED(SkyParamSet, "Sky", inline);
};

/* the FFT waves' spectrum, from the defaults of the file that makes the set */
class FFTWaveParamSet : public ParamSet, public ParamLink {
public:
	class Values {
	public:
		Values(ParamSet *set)
			: m_gravity(set, "gravity", s_fft_wave_param.m_gravity, 0.1f, 30.0f, 0),
			  m_a(set, "A", s_fft_wave_param.m_a, 0.0f, s_fft_wave_param.m_a * 10.0f,
			      PARAM_FLAG_RAW),
			  m_log_a(set, "logA", s_fft_wave_param.m_log_a, -20.0f, -1.0f, PARAM_FLAG_FINE),
			  m_patch_size(set, "patch_size", s_fft_wave_param.m_patch_size, 0.0f, 500.0f, 0),
			  m_wind_speed(set, "wind_speed", s_fft_wave_param.m_wind_speed, 6.0f, 20.0f, 0),
			  m_wind_dir(set, "wind_dir", s_fft_wave_param.m_wind_dir, -WAVE_PARAM_PI,
				     WAVE_PARAM_PI, 0),
			  m_time_step(set, "time_step", s_fft_wave_param.m_time_step, 0.0f, 0.1f,
				      PARAM_FLAG_FINE)
		{
		}

		/* the spectrum the values give, the height scale the default's (name ours) */
		FFTWaveParam GetParam() const
		{
			FFTWaveParam param;

			param.m_gravity = m_gravity.m_value;
			param.m_a = m_a.m_value;
			param.m_log_a = m_log_a.m_value;
			param.m_patch_size = m_patch_size.m_value;
			param.m_wind_speed = m_wind_speed.m_value;
			param.m_wind_dir = m_wind_dir.m_value;
			param.m_time_step = m_time_step.m_value;
			return param;
		}

		Param<float> m_gravity;
		Param<float> m_a;
		Param<float> m_log_a;
		Param<float> m_patch_size;
		Param<float> m_wind_speed;
		Param<float> m_wind_dir;
		Param<float> m_time_step;
	};

	WAVE_PARAM_SET(FFTWaveParamSet, "FFTWave");
};

/* one wave instance: its place, its distortion and its colours */
class WaveInstanceParamSet : public ParamSet, public ParamLink {
public:
	class Values {
	public:
		Values(ParamSet *set)
			: m_distortion_scale(set, "distortion_scale", 0.11f, 0.0f, 1.0f,
					     PARAM_FLAG_FINE),
			  m_translation(set, "translation", math::v3(0.0f), PARAM_FLAG_RAW),
			  m_rotation(set, "rotation", math::quaternion(), PARAM_FLAG_RAW),
			  m_scale(set, "scale", math::v3(0.5f), math::v3(0.0f),
				  math::v3(1.0f), PARAM_FLAG_FINE),
			  m_global_scale(set, "global_scale", 2.0f, 1.0f, 10.0f, PARAM_FLAG_FINE),
			  m_uv_scale(set, "uv_scale", 1.0f, 0.0f, 10.0f, PARAM_FLAG_FINE),
			  m_uv_rotate(set, "uv_rotate", 0.0f, 0.0f, 360.0f,
				      PARAM_FLAG_ANGLE_DEGREES | PARAM_FLAG_ANGLE |
					      PARAM_FLAG_UNCLAMPED),
			  m_center_color(set, "center_color", math::v3(0.8f),
					 math::v3(0.0f), math::v3(1.0f),
					 PARAM_FLAG_COLOR),
			  m_center_power(set, "center_power", 1.0f, 0.0f, 30.0f, PARAM_FLAG_FINE),
			  m_edge_color(set, "edge_color", math::v3(0.8f),
				       math::v3(0.0f), math::v3(1.0f),
				       PARAM_FLAG_COLOR),
			  m_edge_power(set, "edge_power", 1.0f, 0.0f, 30.0f, PARAM_FLAG_FINE),
			  m_cross_section(set, "cross_section",
					  math::v4(0.308067f, 0.057868f, -1.217043f, 0.469729f),
					  PARAM_FLAG_PROFILE),
			  m_cross_section_flat(set, "cross_section_flat",
					       math::v4(0.0f, 1.0f, 0.0f, 0.0f), PARAM_FLAG_PROFILE),
			  m_decay(set, "decay", 0.5f, 0.0f, 1.0f, 0),
			  m_shadow_nor_blend(set, "shadow_nor_blend", 0.4f, 0.0f, 1.0f, 0)
		{
		}

		/* every value set as the parameter's own set would; Sony inlines it */
		__attribute__((always_inline)) Values &operator=(const Values &other)
		{
			m_distortion_scale = other.m_distortion_scale;
			m_translation = other.m_translation;
			m_rotation = other.m_rotation;
			m_scale = other.m_scale;
			m_global_scale = other.m_global_scale;
			m_uv_scale = other.m_uv_scale;
			m_uv_rotate = other.m_uv_rotate;
			m_center_color = other.m_center_color;
			m_center_power = other.m_center_power;
			m_edge_color = other.m_edge_color;
			m_edge_power = other.m_edge_power;
			m_cross_section = other.m_cross_section;
			m_cross_section_flat = other.m_cross_section_flat;
			m_decay = other.m_decay;
			m_shadow_nor_blend = other.m_shadow_nor_blend;
			return *this;
		}

		Param<float> m_distortion_scale;
		Param<math::v3> m_translation;
		Param<math::quaternion> m_rotation;
		Param<math::v3> m_scale;
		Param<float> m_global_scale;
		Param<float> m_uv_scale;
		Param<float> m_uv_rotate;
		Param<math::v3> m_center_color;
		Param<float> m_center_power;
		Param<math::v3> m_edge_color;
		Param<float> m_edge_power;
		Param<math::v4> m_cross_section;
		Param<math::v4> m_cross_section_flat;
		Param<float> m_decay;
		Param<float> m_shadow_nor_blend;
	};

	WAVE_PARAM_SET(WaveInstanceParamSet, "WaveInstance");
};

/* the camera over the waves */
class WaveRendererParamSet : public ParamSet, public ParamLink {
public:
	class Values {
	public:
		Values(ParamSet *set)
			: m_edge_fog(set, "edge_fog", math::v2(0.94f, 0.9f), math::v2(0.0f, 0.0f),
				     math::v2(0.99f, 0.99f), 0),
			  m_pitch_max(set, "pitch_max", -1.16937053f, -WAVE_PARAM_HALF_PI, 0.0f,
				      PARAM_FLAG_DEGREES),
			  m_pitch_rest(set, "pitch_rest", -0.69813168f, -WAVE_PARAM_HALF_PI, 0.0f,
				       PARAM_FLAG_DEGREES),
			  m_pitch_min(set, "pitch_min", 0.0f, -WAVE_PARAM_HALF_PI, 0.0f,
				      PARAM_FLAG_DEGREES),
			  m_pitch_debug(set, "pitch_debug", 0.0f, -WAVE_PARAM_HALF_PI, 0.0f,
					PARAM_FLAG_DEGREES),
			  m_roll_max(set, "roll_max", 0.17453292f, 0.0f, 0.34906584f, PARAM_FLAG_DEGREES),
			  m_roll_max_zone(set, "roll_max_zone", 0.122173049f, 0.0f, 0.34906584f,
					  PARAM_FLAG_DEGREES),
			  m_zoom(set, "zoom", 0.5f, 0.0f, 5.0f, 0),
			  m_camera_chase(set, "camera_chase", 0.04f, 0.0f, 1.0f, PARAM_FLAG_FINE)
		{
		}

		Param<math::v2> m_edge_fog;
		Param<float> m_pitch_max;
		Param<float> m_pitch_rest;
		Param<float> m_pitch_min;
		Param<float> m_pitch_debug;
		Param<float> m_roll_max;
		Param<float> m_roll_max_zone;
		Param<float> m_zoom;
		Param<float> m_camera_chase;
	};

	WAVE_PARAM_SET(WaveRendererParamSet, "WaveRenderer");
};

/* how the waves' texture coordinates turn while they move from one look to another */
class WaveTransitionParamSet : public ParamSet, public ParamLink {
public:
	class Values {
	public:
		Values(ParamSet *set)
			: m_uv_rotate_distort_curve(set, "uv_rotate_distort_curve", 1.0f, 0.0f, 100.0f,
						    0),
			  m_uv_rotate_distort_control_chase(set, "uv_rotate_distort_control_chase",
							    0.1f, 0.0f, 1.0f, 0),
			  m_uv_rotate_distort_control(set, "uv_rotate_distort_control", 1.0f, 0.0f, 1.0f,
						      0)
		{
		}

		Param<float> m_uv_rotate_distort_curve;
		Param<float> m_uv_rotate_distort_control_chase;
		Param<float> m_uv_rotate_distort_control;
	};

	WAVE_PARAM_SET(WaveTransitionParamSet, "WaveTransition");
};

#undef WAVE_PARAM_SET

/* a set's keys, the set that follows them, and the manager they are edited with (name ours) */
template<class S>
class ParamSetInstance {
public:
	ParamSetInstance(ParamSetKeysManager &manager, const string &variant)
		: m_keys(variant), m_set(true)
	{
		m_keys.Add(&m_set);
		manager.Add(&m_keys);
	}

	~ParamSetInstance()
	{
	}

	ParamSetKeys<S> m_keys;
	S m_set;
};

/* Sony keeps these three out of line */
template<>
inline __attribute__((noinline)) ParamSetInstance<SkyParamSet>::~ParamSetInstance()
{
}

template<>
inline __attribute__((noinline)) ParamSetInstance<WaveInstanceParamSet>::~ParamSetInstance()
{
}

template<>
inline __attribute__((noinline)) ParamSetInstance<WaveRendererParamSet>::~ParamSetInstance()
{
}

}
}

#endif
