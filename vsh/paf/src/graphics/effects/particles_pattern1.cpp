/*
 * ParticlesPattern1: ambient floating particles whose parameters interpolate with the wave
 * height between the base and "up" parameter sets (2014 PAF).
 */
#include <stdint.h>
#include <vectormath.h>
#include <graphics/effects/particles.h>
#include <timer/timer.h>
#include "../../std/math.h"

using sce::Vectormath::Simd::floatInVec;
using sce::Vectormath::Simd::recipf;
using sce::Vectormath::Simd::Aos::Vector3;
using sce::Vectormath::Simd::Aos::Vector4;

namespace paf {
namespace graph {

namespace {

static inline float NextRandomSigned(uint32_t &seed)
{
	union {
		uint32_t bits;
		float value;
	} sample;

	seed *= 16807u;
	sample.bits = (seed >> 9) | 0x40000000u;
	return sample.value - 3.0f;
}

/* scales the component of vec parallel to axis by scale while leaving perpendicular components */
static math::v4 ScaleAlongAxis(const math::v4 &vec, const math::v4 &axis, float scale)
{
	Vector4 dir = axis.getImpl();

	dir.setW(0.0f);

	Vector4 input = vec.getImpl();

	input.setW(0.0f);

	floatInVec axis_dot = dot(dir, dir);
	Vector4 parallel = dir * (dot(dir, input) * recipf(axis_dot));

	return math::v4((input - parallel) + parallel * floatInVec(scale));
}

} /* namespace */

ParticlesPattern1::ParticlesPattern1()
	: m_wave_x(0.0f), m_wave_y(0.0f), m_enabled(true), m_manager("ParticlesPattern1"),
	  m_emit(m_manager, ""), m_mov(m_manager, ""), m_lit(m_manager, ""),
	  m_focus(m_manager, ""), m_glare(m_manager, ""), m_emit_up(m_manager, "up"),
	  m_mov_up(m_manager, "up"), m_lit_up(m_manager, "up"), m_focus_up(m_manager, "up"),
	  m_glare_up(m_manager, "up"), m_rng_seed(8675309u),
	  m_last_emit_time(get_time_internal()), m_wait_jitter(0.0f),
	  m_rs_particles_pattern1(NULL)
{
}

ParticlesPattern1::~ParticlesPattern1()
{
}

void ParticlesPattern1::Init(common::SharedPtr<Camera> camera,
			     common::SharedPtr<DrawEnv> draw_env)
{
	m_camera = camera;
	m_draw_env = draw_env;
}

void ParticlesPattern1::Load(const string &dir, common::SharedPtr<ParticlesGraphics> graphics,
			     uint32_t max_particles)
{
	m_emitter = common::SharedPtr<ParticleEmitter>(
		new ParticleEmitter("neutrinos", max_particles, graphics, m_camera));
	m_manager.SetPaths(dir, "particles/pattern1/psets");

	EmitParamSet &emit = m_emit.m_keys.m_default;

	emit.findParamAndSet("vel_min", 0.001f);
	emit.findParamAndSet("vel_mul", 0.062018f);
	emit.findParamAndSet("vel_var", 0.049999f);
	emit.findParamAndSet("vel_zscale", 0.5f);
	emit.findParamAndSet("wait_time", 0.108798f);
	emit.findParamAndSet("wait_time_var", 0.15f);
	emit.findParamAndSet("aging_speed", 0.002602f);
	emit.findParamAndSet("aging_speed_var", 0.0f);
	emit.findParamAndSet("lifebounds_center", 0.0f, 1.25f, -4.821424f);
	emit.findParamAndSet("lifebounds_size", 9.375f, 5.892857f, 7.946428f);
	m_emit.m_keys.GetKey("default", true)->m_values = emit.m_values;

	emit.findParamAndSet("vel_min", 0.435806f);
	emit.findParamAndSet("vel_mul", 1.37581f);
	emit.findParamAndSet("vel_var", 0.049999f);
	emit.findParamAndSet("vel_zscale", 0.705357f);
	emit.findParamAndSet("wait_time", 0.232492f);
	emit.findParamAndSet("wait_time_var", 0.15f);
	emit.findParamAndSet("aging_speed", 0.002602f);
	emit.findParamAndSet("aging_speed_var", 0.0f);
	emit.findParamAndSet("lifebounds_center", 0.0f, 1.25f, -4.821424f);
	emit.findParamAndSet("lifebounds_size", 9.375f, 5.892857f, 7.946428f);
	m_emit.m_keys.GetKey("iboot", true)->m_values = emit.m_values;

	MovParamSet &mov = m_mov.m_keys.m_default;

	mov.findParamAndSet("friction", 0.022093f);
	mov.findParamAndSet("spin_time_scale", 2.74f);
	mov.findParamAndSet("delta_time", 0.003333f);
	mov.findParamAndSet("gravity", -0.000068f);
	mov.findParamAndSet("wind_dir", 0.0f, 0.0f, 0.464284f);
	mov.findParamAndSet("wind_scale", 0.014702f);
	mov.findParamAndSet("wind_scale_10", 0.435805f);
	mov.findParamAndSet("brownian_scale", 3.473259f);
	mov.findParamAndSet("brownian_scale_shake", 6.0f);
	m_mov.m_keys.GetKey("default", true)->m_values = mov.m_values;

	mov.findParamAndSet("friction", 0.127626f);
	mov.findParamAndSet("spin_time_scale", 3.542072f);
	mov.findParamAndSet("delta_time", 0.00318f);
	mov.findParamAndSet("gravity", -0.000037f);
	mov.findParamAndSet("wind_dir", 0.0f, 0.0f, 0.464284f);
	mov.findParamAndSet("wind_scale", 0.014702f);
	mov.findParamAndSet("wind_scale_10", 0.435805f);
	mov.findParamAndSet("brownian_scale", 5.616289f);
	mov.findParamAndSet("brownian_scale_shake", 6.0f);
	m_mov.m_keys.GetKey("iboot", true)->m_values = mov.m_values;

	LitParamSet &lit = m_lit.m_keys.m_default;

	lit.findParamAndSet("light_pos", 4.159996f, 2.63f, -7.599998f);
	lit.findParamAndSet("light_attn", 1.0f, 0.0f, 0.0f);
	lit.findParamAndSet("specular_power", 39.873829f);
	lit.findParamAndSet("specular_coeff", 74.74f);
	lit.findParamAndSet("lambert_coeff", 15.245608f);
	lit.findParamAndSet("exposure", 0.337359f);
	lit.findParamAndSet("fresnel", 1.24f);
	lit.findParamAndSet("color_control", 1.0f);
	lit.findParamAndSet("global_alpha", 0.93f);
	m_lit.m_keys.GetKey("default", true)->m_values = lit.m_values;

	lit.findParamAndSet("light_pos", 4.159996f, 2.63f, -7.599998f);
	lit.findParamAndSet("light_attn", 1.0f, 0.0f, 0.0f);
	lit.findParamAndSet("specular_power", 39.873829f);
	lit.findParamAndSet("specular_coeff", 74.74f);
	lit.findParamAndSet("lambert_coeff", 15.245608f);
	lit.findParamAndSet("exposure", 0.77008f);
	lit.findParamAndSet("fresnel", 1.24f);
	lit.findParamAndSet("color_control", 1.0f);
	lit.findParamAndSet("global_alpha", 0.875f);
	m_lit.m_keys.GetKey("iboot", true)->m_values = lit.m_values;

	FocusParamSet &focus = m_focus.m_keys.m_default;

	focus.findParamAndSet("size_middle", 0.06844f);
	focus.findParamAndSet("size_near", 0.500655f);
	focus.findParamAndSet("size_far", 0.056989f);
	focus.findParamAndSet("near_focus", 1.5f);
	focus.findParamAndSet("near_focus_dist", 6.785714f);
	focus.findParamAndSet("near_focus_pow", 3.049999f);
	focus.findParamAndSet("near_darkness", 1.0f);
	focus.findParamAndSet("near_fuzziness", 4.961801f);
	focus.findParamAndSet("far_focus", 6.696424f);
	focus.findParamAndSet("far_focus_dist", 2.678571f);
	focus.findParamAndSet("far_focus_pow", 1.72f);
	focus.findParamAndSet("far_darkness", 13.899993f);
	focus.findParamAndSet("near_align", 0.722f);
	focus.findParamAndSet("size_align", 13.189991f);
	focus.findParamAndSet("global_align", 0.0f);
	m_focus.m_keys.GetKey("default", true)->m_values = focus.m_values;

	focus.findParamAndSet("size_middle", 0.06844f);
	focus.findParamAndSet("size_near", 0.326166f);
	focus.findParamAndSet("size_far", 0.028761f);
	focus.findParamAndSet("near_focus", 1.5f);
	focus.findParamAndSet("near_focus_dist", 7.410714f);
	focus.findParamAndSet("near_focus_pow", 3.049999f);
	focus.findParamAndSet("near_darkness", 1.0f);
	focus.findParamAndSet("near_fuzziness", 4.358062f);
	focus.findParamAndSet("far_focus", 6.696424f);
	focus.findParamAndSet("far_focus_dist", 2.678571f);
	focus.findParamAndSet("far_focus_pow", 1.72f);
	focus.findParamAndSet("far_darkness", 13.899993f);
	focus.findParamAndSet("near_align", 1.0f);
	focus.findParamAndSet("size_align", 15.357142f);
	focus.findParamAndSet("global_align", 0.321428f);
	m_focus.m_keys.GetKey("iboot", true)->m_values = focus.m_values;

	GlareParamSet &glare = m_glare.m_keys.m_default;

	glare.findParamAndSet("glare", 0.830357f);
	glare.findParamAndSet("glare_scale", 6.214284f);
	glare.findParamAndSet("glare_p1", 0.569101f);
	glare.findParamAndSet("glare_p2", 4.44f);
	m_glare.m_keys.GetKey("default", true)->m_values = glare.m_values;

	glare.findParamAndSet("glare", 0.830357f);
	glare.findParamAndSet("glare_scale", 5.428571f);
	glare.findParamAndSet("glare_p1", 0.569101f);
	glare.findParamAndSet("glare_p2", 4.44f);
	m_glare.m_keys.GetKey("iboot", true)->m_values = glare.m_values;

	EmitParamSet &emit_up = m_emit_up.m_keys.m_default;

	emit_up.findParamAndSet("vel_min", 0.0f);
	emit_up.findParamAndSet("vel_mul", 0.000643f);
	emit_up.findParamAndSet("vel_var", 0.049999f);
	emit_up.findParamAndSet("vel_zscale", 1.0f);
	emit_up.findParamAndSet("wait_time", 0.005307f);
	emit_up.findParamAndSet("wait_time_var", 0.15f);
	emit_up.findParamAndSet("aging_speed", 0.00576f);
	emit_up.findParamAndSet("aging_speed_var", 0.0f);
	emit_up.findParamAndSet("lifebounds_center", 0.0f, 3.75f, -3.035712f);
	emit_up.findParamAndSet("lifebounds_size", 8.75f, 7.232141f, 6.428571f);
	m_emit_up.m_keys.GetKey("default", true)->m_values = emit_up.m_values;

	emit_up.findParamAndSet("vel_min", 0.0f);
	emit_up.findParamAndSet("vel_mul", 0.000643f);
	emit_up.findParamAndSet("vel_var", 0.049999f);
	emit_up.findParamAndSet("vel_zscale", 1.0f);
	emit_up.findParamAndSet("wait_time", 0.005307f);
	emit_up.findParamAndSet("wait_time_var", 0.15f);
	emit_up.findParamAndSet("aging_speed", 0.00576f);
	emit_up.findParamAndSet("aging_speed_var", 0.0f);
	emit_up.findParamAndSet("lifebounds_center", 0.0f, 3.75f, -3.035712f);
	emit_up.findParamAndSet("lifebounds_size", 8.75f, 7.232141f, 6.428571f);
	m_emit_up.m_keys.GetKey("iboot", true)->m_values = emit_up.m_values;

	MovParamSet &mov_up = m_mov_up.m_keys.m_default;

	mov_up.findParamAndSet("friction", 0.010207f);
	mov_up.findParamAndSet("spin_time_scale", 2.74f);
	mov_up.findParamAndSet("delta_time", 0.003333f);
	mov_up.findParamAndSet("gravity", -0.000068f);
	mov_up.findParamAndSet("wind_dir", 0.008928f, -0.419642f, 0.0f);
	mov_up.findParamAndSet("wind_scale", 0.016268f);
	mov_up.findParamAndSet("wind_scale_10", 0.331743f);
	mov_up.findParamAndSet("brownian_scale", 0.351018f);
	mov_up.findParamAndSet("brownian_scale_shake", 6.0f);
	m_mov_up.m_keys.GetKey("default", true)->m_values = mov_up.m_values;

	mov_up.findParamAndSet("friction", 0.010207f);
	mov_up.findParamAndSet("spin_time_scale", 2.74f);
	mov_up.findParamAndSet("delta_time", 0.003333f);
	mov_up.findParamAndSet("gravity", -0.000068f);
	mov_up.findParamAndSet("wind_dir", 0.008928f, -0.419642f, 0.0f);
	mov_up.findParamAndSet("wind_scale", 0.016268f);
	mov_up.findParamAndSet("wind_scale_10", 0.331743f);
	mov_up.findParamAndSet("brownian_scale", 0.351018f);
	mov_up.findParamAndSet("brownian_scale_shake", 6.0f);
	m_mov_up.m_keys.GetKey("iboot", true)->m_values = mov_up.m_values;

	LitParamSet &lit_up = m_lit_up.m_keys.m_default;

	lit_up.findParamAndSet("light_pos", 4.159996f, 2.63f, -7.599998f);
	lit_up.findParamAndSet("light_attn", 1.0f, 0.0f, 0.0f);
	lit_up.findParamAndSet("specular_power", 13.939993f);
	lit_up.findParamAndSet("specular_coeff", 74.74f);
	lit_up.findParamAndSet("lambert_coeff", 8.0f);
	lit_up.findParamAndSet("exposure", 0.053694f);
	lit_up.findParamAndSet("fresnel", 1.24f);
	lit_up.findParamAndSet("color_control", 1.0f);
	lit_up.findParamAndSet("global_alpha", 0.93f);
	m_lit_up.m_keys.GetKey("default", true)->m_values = lit_up.m_values;

	lit_up.findParamAndSet("light_pos", 4.159996f, 2.63f, -7.599998f);
	lit_up.findParamAndSet("light_attn", 1.0f, 0.0f, 0.0f);
	lit_up.findParamAndSet("specular_power", 13.939993f);
	lit_up.findParamAndSet("specular_coeff", 74.74f);
	lit_up.findParamAndSet("lambert_coeff", 8.0f);
	lit_up.findParamAndSet("exposure", 0.053694f);
	lit_up.findParamAndSet("fresnel", 1.24f);
	lit_up.findParamAndSet("color_control", 1.0f);
	lit_up.findParamAndSet("global_alpha", 0.93f);
	m_lit_up.m_keys.GetKey("iboot", true)->m_values = lit_up.m_values;

	FocusParamSet &focus_up = m_focus_up.m_keys.m_default;

	focus_up.findParamAndSet("size_middle", 0.01917f);
	focus_up.findParamAndSet("size_near", 0.047791f);
	focus_up.findParamAndSet("size_far", 1.127424f);
	focus_up.findParamAndSet("near_focus", 1.0625f);
	focus_up.findParamAndSet("near_focus_dist", 4.464283f);
	focus_up.findParamAndSet("near_focus_pow", 2.285712f);
	focus_up.findParamAndSet("near_darkness", 1.0f);
	focus_up.findParamAndSet("near_fuzziness", 3.79f);
	focus_up.findParamAndSet("far_focus", 6.696424f);
	focus_up.findParamAndSet("far_focus_dist", 2.678571f);
	focus_up.findParamAndSet("far_focus_pow", 1.72f);
	focus_up.findParamAndSet("far_darkness", 13.899993f);
	focus_up.findParamAndSet("near_align", 0.722f);
	focus_up.findParamAndSet("size_align", 13.189991f);
	focus_up.findParamAndSet("global_align", 0.0f);
	m_focus_up.m_keys.GetKey("default", true)->m_values = focus_up.m_values;

	focus_up.findParamAndSet("size_middle", 0.01917f);
	focus_up.findParamAndSet("size_near", 0.047791f);
	focus_up.findParamAndSet("size_far", 1.127424f);
	focus_up.findParamAndSet("near_focus", 1.0625f);
	focus_up.findParamAndSet("near_focus_dist", 4.464283f);
	focus_up.findParamAndSet("near_focus_pow", 2.285712f);
	focus_up.findParamAndSet("near_darkness", 1.0f);
	focus_up.findParamAndSet("near_fuzziness", 3.79f);
	focus_up.findParamAndSet("far_focus", 6.696424f);
	focus_up.findParamAndSet("far_focus_dist", 2.678571f);
	focus_up.findParamAndSet("far_focus_pow", 1.72f);
	focus_up.findParamAndSet("far_darkness", 13.899993f);
	focus_up.findParamAndSet("near_align", 0.722f);
	focus_up.findParamAndSet("size_align", 13.189991f);
	focus_up.findParamAndSet("global_align", 0.0f);
	m_focus_up.m_keys.GetKey("iboot", true)->m_values = focus_up.m_values;

	GlareParamSet &glare_up = m_glare_up.m_keys.m_default;

	glare_up.findParamAndSet("glare", 1.0f);
	glare_up.findParamAndSet("glare_scale", 4.159996f);
	glare_up.findParamAndSet("glare_p1", 0.99f);
	glare_up.findParamAndSet("glare_p2", 4.44f);
	m_glare_up.m_keys.GetKey("default", true)->m_values = glare_up.m_values;

	glare_up.findParamAndSet("glare", 1.0f);
	glare_up.findParamAndSet("glare_scale", 4.159996f);
	glare_up.findParamAndSet("glare_p1", 0.99f);
	glare_up.findParamAndSet("glare_p2", 4.44f);
	m_glare_up.m_keys.GetKey("iboot", true)->m_values = glare_up.m_values;

	m_manager.AddVariant("default");
	m_manager.AddVariant("iboot");
	m_manager.SetVariant("default");

	m_rs_particles_pattern1 =
		m_draw_env->AddLayer("m_rs_particles_pattern1", 650, DrawLayer::MODE_KEEP, 0x4000);
	m_rs_particles_pattern1->Push(0, 0, &ParticlesPattern1::Draw, this);
	m_rs_particles_pattern1->m_draw_mode = 2;
}

void ParticlesPattern1::UpdateParams(float frame_time)
{
	m_manager.Update(frame_time);

	EmitParamSet blended_emit(true);
	MovParamSet blended_mov(true);
	LitParamSet blended_lit(true);
	FocusParamSet blended_focus(true);
	GlareParamSet blended_glare(true);

	blended_emit.m_values = m_emit.m_set.m_values;
	blended_mov.m_values = m_mov.m_set.m_values;
	blended_lit.m_values = m_lit.m_set.m_values;
	blended_focus.m_values = m_focus.m_set.m_values;
	blended_glare.m_values = m_glare.m_set.m_values;

	blended_emit.SetKey(PARAM_KEY_LERP_FROM, m_emit.m_set.m_values);
	blended_mov.SetKey(PARAM_KEY_LERP_FROM, m_mov.m_set.m_values);
	blended_lit.SetKey(PARAM_KEY_LERP_FROM, m_lit.m_set.m_values);
	blended_focus.SetKey(PARAM_KEY_LERP_FROM, m_focus.m_set.m_values);
	blended_glare.SetKey(PARAM_KEY_LERP_FROM, m_glare.m_set.m_values);

	blended_emit.SetKey(PARAM_KEY_LERP_TO, m_emit_up.m_set.m_values);
	blended_mov.SetKey(PARAM_KEY_LERP_TO, m_mov_up.m_set.m_values);
	blended_lit.SetKey(PARAM_KEY_LERP_TO, m_lit_up.m_set.m_values);
	blended_focus.SetKey(PARAM_KEY_LERP_TO, m_focus_up.m_set.m_values);
	blended_glare.SetKey(PARAM_KEY_LERP_TO, m_glare_up.m_set.m_values);

	blended_emit.Lerp(m_wave_y);
	blended_mov.Lerp(m_wave_y);
	blended_lit.Lerp(m_wave_y);
	blended_focus.Lerp(m_wave_y);
	blended_glare.Lerp(m_wave_y);

	blended_emit.m_values.CopyToRaw(m_emitter->m_emit);
	blended_mov.m_values.CopyToRaw(m_emitter->m_mov);
	blended_lit.m_values.CopyToRaw(m_emitter->m_lit);
	blended_focus.m_values.CopyToRaw(m_emitter->m_focus);
	blended_glare.m_values.CopyToRaw(m_emitter->m_glare);
}

void ParticlesPattern1::UpdateMotion()
{
	ParticleEmitter *emitter = m_emitter.get();
	float shake = m_shake.Update();

	emitter->m_spin_time_scale = emitter->m_mov.m_spin_time_scale;
	emitter->m_size_middle_copy = emitter->m_focus.m_size_middle;
	emitter->m_friction =
		math::v4(emitter->m_mov.m_friction, emitter->m_mov.m_friction,
			 emitter->m_mov.m_friction, emitter->m_mov.m_friction);
	emitter->m_gravity = math::v4(0.0f, emitter->m_mov.m_gravity, 0.0f, 0.0f);
	emitter->m_delta_time_vec =
		math::v4(emitter->m_mov.m_delta_time, emitter->m_mov.m_delta_time,
			 emitter->m_mov.m_delta_time, 1.0f);

	Vector3 wind_dir = emitter->m_mov.m_wind_dir.getImpl();
	float wind_len = length(wind_dir).getAsFloat();

	if (wind_len > 0.0001f) {
		Vector3 wind_unit = wind_dir * recipf(floatInVec(wind_len));

		emitter->m_wind =
			math::v4(wind_unit * floatInVec(emitter->m_mov.m_wind_scale), 0.0f);
	} else {
		emitter->m_wind = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	}
	emitter->m_brownian_scale =
		emitter->m_mov.m_brownian_scale + shake * emitter->m_mov.m_brownian_scale_shake;
}

void ParticlesPattern1::Emit()
{
	ParticleEmitter *emitter = m_emitter.get();
	Vector4 bounds_min = emitter->m_bounds_min.getImpl();
	Vector4 bounds_span = emitter->m_bounds_max.getImpl() - bounds_min;
	float wait_time = emitter->m_emit.m_wait_time *
			  (m_wait_jitter * emitter->m_emit.m_wait_time_var + 1.0f);
	uint64_t elapsed_us = get_time_internal() - m_last_emit_time;

	if ((float)elapsed_us / 1000000.0f <= wait_time)
		return;

	uint64_t now_elapsed_us = get_time_internal() - m_last_emit_time;
	uint32_t spawn_count = (uint32_t)(((float)now_elapsed_us / 1000000.0f) / wait_time);

	if (spawn_count > 20)
		spawn_count = 20;
	m_last_emit_time = get_time_internal();
	m_wait_jitter = NextRandomSigned(m_rng_seed);

	for (uint32_t i = 0; i < spawn_count; ++i) {
		float rand_px = NextRandomSigned(m_rng_seed);
		float rand_py = NextRandomSigned(m_rng_seed);
		float rand_pz = NextRandomSigned(m_rng_seed);
		Vector4 unit_pos = (Vector4(rand_px, rand_py, rand_pz, 0.0f) +
				    Vector4(1.0f, 1.0f, 1.0f, 1.0f)) *
				   floatInVec(0.5f);
		Vector4 spawn_pos = bounds_min + mulPerElem(bounds_span, unit_pos);

		float rand_vx = NextRandomSigned(m_rng_seed);
		float rand_vy = NextRandomSigned(m_rng_seed);
		float rand_vz = NextRandomSigned(m_rng_seed);
		Vector4 offset_pos = spawn_pos +
				     Vector4(rand_vx, rand_vy, rand_vz, 0.0f) * floatInVec(0.1f);
		Vector4 vel_raw = (spawn_pos - offset_pos) / floatInVec(emitter->m_mov.m_delta_time);
		float speed = length(vel_raw.getXYZ()).getAsFloat();

		if (speed < 0.001f)
			break;

		Vector4 vel_dir = vel_raw * recipf(floatInVec(speed));
		math::v4 scaled_dir =
			ScaleAlongAxis(math::v4(vel_dir), m_camera->m_transform.m_world.z(),
				       emitter->m_emit.m_vel_zscale);
		float speed_jitter = NextRandomSigned(m_rng_seed);
		float final_speed = sce_paf_powf(
			emitter->m_emit.m_vel_mul *
				(speed_jitter * emitter->m_emit.m_vel_var + 1.0f) * speed,
			0.5f);

		if (final_speed < emitter->m_emit.m_vel_min)
			final_speed = emitter->m_emit.m_vel_min;

		float aging_jitter = NextRandomSigned(m_rng_seed);
		float aging = emitter->m_emit.m_aging_speed *
			      (aging_jitter * emitter->m_emit.m_aging_speed_var + 1.0f);

		if (aging <= 0.001f)
			aging = 0.001f;

		math::v4 velocity(scaled_dir.getImpl() * floatInVec(final_speed));

		m_emitter->PushAlloc();
		m_emitter->PushSetPos(math::v4(spawn_pos));
		m_emitter->PushSetVel(velocity);
		m_emitter->PushSetAging(aging);
	}
}

void ParticlesPattern1::Update(float frame_time)
{
	if (m_enabled) {
		UpdateParams(frame_time);
		UpdateMotion();
		Emit();
		m_emitter->Update();
	}
}

void ParticlesPattern1::Draw()
{
	if (m_enabled)
		m_emitter->Draw();
}

}
}
