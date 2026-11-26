/*
 * ParticlesPattern2: interactive touch / Tinkerbell trail particles sampled along a 64-point
 * Catmull-Rom spline with inverse-CDF weighting by trail intensity (2014 PAF).
 */
#include <stdint.h>
#include <vectormath.h>
#include <graphics/effects/particles.h>
#include <graphics/gutil.h>
#include <src/common/frame_count.h>
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

static inline int32_t WrapIndex(int32_t index, int32_t count)
{
	int32_t rem = index % count;

	if (rem < 0)
		rem += count;
	return rem;
}

static inline int32_t ClampIndex(int32_t index, int32_t max_index)
{
	if (index < 0)
		return 0;
	if (index > max_index)
		return max_index;
	return index;
}

/* inverse-CDF lookup over (t, cumulative_weight) pairs */
static float SampleTrailCdf(const math::v2 *cdf, uint32_t count, float target)
{
	uint32_t index = 0;

	for (; index < count; ++index) {
		if (target <= cdf[index].extract_y())
			break;
	}
	if (index == count)
		return cdf[count - 1].extract_x();
	if (index == 0)
		return cdf[0].extract_x();

	const math::v2 &lo = cdf[index - 1];
	const math::v2 &hi = cdf[index];
	float frac = (target - lo.extract_y()) / (hi.extract_y() - lo.extract_y());

	return lo.extract_x() + (hi.extract_x() - lo.extract_x()) * frac;
}

/* uniform Catmull-Rom spline evaluation over ordered trail control points */
static math::v4 EvaluateTrailSpline(const math::v4 *points, uint32_t count, float param)
{
	if (count == 0)
		return math::v4(0.0f, 0.0f, 0.0f, 0.0f);

	int32_t max_index = (int32_t)(count - 1);
	float scaled = param * (float)(count - 1);
	int32_t seg = (int32_t)scaled;
	float frac = scaled - (float)seg;

	Vector4 ctrl_prev = points[ClampIndex(seg - 1, max_index)].getImpl();
	Vector4 ctrl_curr = points[ClampIndex(seg, max_index)].getImpl();
	Vector4 ctrl_next = points[ClampIndex(seg + 1, max_index)].getImpl();
	Vector4 ctrl_after = points[ClampIndex(seg + 2, max_index)].getImpl();

	Vector4 coeff_const = ctrl_curr * floatInVec(2.0f);
	Vector4 coeff_lin = ctrl_next - ctrl_prev;
	Vector4 coeff_quad = ctrl_prev * floatInVec(2.0f) - ctrl_curr * floatInVec(5.0f) +
			     ctrl_next * floatInVec(4.0f) - ctrl_after;
	Vector4 coeff_cube = -ctrl_prev + ctrl_curr * floatInVec(3.0f) -
			     ctrl_next * floatInVec(3.0f) + ctrl_after;

	float frac_sq = frac * frac;
	float frac_cu = frac * frac_sq;
	Vector4 sum = coeff_const + coeff_lin * floatInVec(frac) + coeff_quad * floatInVec(frac_sq) +
		      coeff_cube * floatInVec(frac_cu);

	return math::v4(sum * floatInVec(0.5f));
}

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

ParticlesPattern2::ParticlesPattern2()
	: m_enabled(true), m_unused_flag(false), m_manager("ParticlesPattern2"),
	  m_emit(m_manager, ""), m_mov(m_manager, ""), m_lit(m_manager, ""),
	  m_focus(m_manager, ""), m_glare(m_manager, ""), m_tinkerbell(m_manager, ""),
	  m_rng_seed(8675309u), m_last_emit_time(get_time_internal()), m_wait_jitter(0.0f),
	  m_last_frame(-1), m_sample_touch(true), m_rs_particles_pattern2(NULL)
{
	m_trail.m_points = m_trail_storage;
	m_trail.m_reserved = TRAIL_CAPACITY;
	m_trail.m_count = TRAIL_CAPACITY;
	m_trail.m_last = 0;
	m_trail.m_next = 1;
	for (uint32_t i = 0; i < TRAIL_CAPACITY; ++i)
		m_trail_storage[i] = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
}

ParticlesPattern2::~ParticlesPattern2()
{
}

void ParticlesPattern2::Init(common::SharedPtr<Camera> camera,
			     common::SharedPtr<DrawEnv> draw_env)
{
	m_camera = camera;
	m_draw_env = draw_env;
}

void ParticlesPattern2::Load(const string &dir, common::SharedPtr<ParticlesGraphics> graphics,
			     uint32_t max_particles)
{
	m_emitter = common::SharedPtr<ParticleEmitter>(
		new ParticleEmitter("neutrinos", max_particles, graphics, m_camera));
	m_manager.SetPaths(dir, "particles/pattern2/psets");

	EmitParamSet &emit = m_emit.m_keys.m_default;

	emit.findParamAndSet("vel_min", 0.000026f);
	emit.findParamAndSet("vel_mul", 0.003556f);
	emit.findParamAndSet("vel_var", 0.301605f);
	emit.findParamAndSet("vel_zscale", 0.5f);
	emit.findParamAndSet("wait_time", 2.0f);
	emit.findParamAndSet("wait_time_var", 0.15f);
	emit.findParamAndSet("aging_speed", 0.018263f);
	emit.findParamAndSet("aging_speed_var", 0.0f);
	emit.findParamAndSet("lifebounds_center", 0.0f, 0.0f, 0.0f);
	emit.findParamAndSet("lifebounds_size", 5.0f, 5.0f, 5.0f);
	m_emit.m_keys.GetKey("default", true)->m_values = emit.m_values;

	emit.findParamAndSet("vel_min", 0.0f);
	emit.findParamAndSet("vel_mul", 0.0f);
	emit.findParamAndSet("vel_var", 0.033735f);
	emit.findParamAndSet("vel_zscale", 0.5f);
	emit.findParamAndSet("wait_time", 0.00525f);
	emit.findParamAndSet("wait_time_var", 0.15f);
	emit.findParamAndSet("aging_speed", 0.004209f);
	emit.findParamAndSet("aging_speed_var", 0.223213f);
	emit.findParamAndSet("lifebounds_center", 0.0f, 0.0f, 0.0f);
	emit.findParamAndSet("lifebounds_size", 4.107141f, 3.660714f, 3.660714f);
	m_emit.m_keys.GetKey("iboot", true)->m_values = emit.m_values;

	MovParamSet &mov = m_mov.m_keys.m_default;

	mov.findParamAndSet("friction", 0.03055f);
	mov.findParamAndSet("spin_time_scale", 2.74f);
	mov.findParamAndSet("delta_time", 0.029999f);
	mov.findParamAndSet("gravity", -0.000068f);
	mov.findParamAndSet("wind_dir", 0.152f, 0.0f, 0.35f);
	mov.findParamAndSet("wind_scale", 0.004f);
	mov.findParamAndSet("wind_scale_10", 0.0f);
	mov.findParamAndSet("brownian_scale", 0.002499f);
	mov.findParamAndSet("brownian_scale_shake", 6.0f);
	m_mov.m_keys.GetKey("default", true)->m_values = mov.m_values;

	mov.findParamAndSet("friction", 0.03055f);
	mov.findParamAndSet("spin_time_scale", 2.74f);
	mov.findParamAndSet("delta_time", 0.035232f);
	mov.findParamAndSet("gravity", -0.000068f);
	mov.findParamAndSet("wind_dir", 0.0f, 0.0f, 0.35f);
	mov.findParamAndSet("wind_scale", 0.013251f);
	mov.findParamAndSet("wind_scale_10", 0.0f);
	mov.findParamAndSet("brownian_scale", 0.012449f);
	mov.findParamAndSet("brownian_scale_shake", 6.0f);
	m_mov.m_keys.GetKey("iboot", true)->m_values = mov.m_values;

	LitParamSet &lit = m_lit.m_keys.m_default;

	lit.findParamAndSet("light_pos", 4.159996f, 2.63f, -7.599998f);
	lit.findParamAndSet("light_attn", 1.0f, 0.0f, 0.0f);
	lit.findParamAndSet("specular_power", 13.939993f);
	lit.findParamAndSet("specular_coeff", 74.74f);
	lit.findParamAndSet("lambert_coeff", 8.0f);
	lit.findParamAndSet("exposure", 0.053694f);
	lit.findParamAndSet("fresnel", 1.24f);
	lit.findParamAndSet("color_control", 1.0f);
	lit.findParamAndSet("global_alpha", 0.93f);
	m_lit.m_keys.GetKey("default", true)->m_values = lit.m_values;

	lit.findParamAndSet("light_pos", 4.159996f, 2.63f, -7.599998f);
	lit.findParamAndSet("light_attn", 1.0f, 0.0f, 0.0f);
	lit.findParamAndSet("specular_power", 13.939993f);
	lit.findParamAndSet("specular_coeff", 10.601129f);
	lit.findParamAndSet("lambert_coeff", 8.0f);
	lit.findParamAndSet("exposure", 0.053694f);
	lit.findParamAndSet("fresnel", 1.24f);
	lit.findParamAndSet("color_control", 1.0f);
	lit.findParamAndSet("global_alpha", 0.93f);
	m_lit.m_keys.GetKey("iboot", true)->m_values = lit.m_values;

	FocusParamSet &focus = m_focus.m_keys.m_default;

	focus.findParamAndSet("size_middle", 0.006476f);
	focus.findParamAndSet("size_near", 0.008062f);
	focus.findParamAndSet("size_far", 0.016708f);
	focus.findParamAndSet("near_focus", 3.88f);
	focus.findParamAndSet("near_focus_dist", 3.4f);
	focus.findParamAndSet("near_focus_pow", 3.049999f);
	focus.findParamAndSet("near_darkness", 1.0f);
	focus.findParamAndSet("near_fuzziness", 3.79f);
	focus.findParamAndSet("far_focus", 10.0f);
	focus.findParamAndSet("far_focus_dist", 2.4f);
	focus.findParamAndSet("far_focus_pow", 1.72f);
	focus.findParamAndSet("far_darkness", 13.899993f);
	focus.findParamAndSet("near_align", 0.722f);
	focus.findParamAndSet("size_align", 13.189991f);
	focus.findParamAndSet("global_align", 0.0f);
	m_focus.m_keys.GetKey("default", true)->m_values = focus.m_values;

	focus.findParamAndSet("size_middle", 0.012882f);
	focus.findParamAndSet("size_near", 0.10109f);
	focus.findParamAndSet("size_far", 0.158631f);
	focus.findParamAndSet("near_focus", 0.9375f);
	focus.findParamAndSet("near_focus_dist", 1.071428f);
	focus.findParamAndSet("near_focus_pow", 3.049999f);
	focus.findParamAndSet("near_darkness", 1.0f);
	focus.findParamAndSet("near_fuzziness", 3.79f);
	focus.findParamAndSet("far_focus", 2.5f);
	focus.findParamAndSet("far_focus_dist", 1.785713f);
	focus.findParamAndSet("far_focus_pow", 1.72f);
	focus.findParamAndSet("far_darkness", 13.899993f);
	focus.findParamAndSet("near_align", 0.722f);
	focus.findParamAndSet("size_align", 13.189991f);
	focus.findParamAndSet("global_align", 1.0f);
	m_focus.m_keys.GetKey("iboot", true)->m_values = focus.m_values;

	GlareParamSet &glare = m_glare.m_keys.m_default;

	glare.findParamAndSet("glare", 1.0f);
	glare.findParamAndSet("glare_scale", 4.159996f);
	glare.findParamAndSet("glare_p1", 0.99f);
	glare.findParamAndSet("glare_p2", 4.44f);
	m_glare.m_keys.GetKey("default", true)->m_values = glare.m_values;

	glare.findParamAndSet("glare", 0.258925f);
	glare.findParamAndSet("glare_scale", 4.357141f);
	glare.findParamAndSet("glare_p1", 0.99f);
	glare.findParamAndSet("glare_p2", 4.44f);
	m_glare.m_keys.GetKey("iboot", true)->m_values = glare.m_values;

	m_manager.AddVariant("default");
	m_manager.AddVariant("iboot");
	m_manager.SetVariant("default");

	m_rs_particles_pattern2 =
		m_draw_env->AddLayer("m_rs_particles_pattern2", 650, DrawLayer::MODE_KEEP, 0x4000);
	m_rs_particles_pattern2->Push(0, 0, &ParticlesPattern2::Draw, this);
	m_rs_particles_pattern2->m_draw_mode = 2;
}

void ParticlesPattern2::UpdateMotion()
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

void ParticlesPattern2::SampleTouch()
{
	gutil::debug::Mouse *mouse = gutil::debug::Mouse::GetInstance();
	math::v2 ndc((float)mouse->m_pointers[0].m_x * 2.0f / (float)mouse->m_screen_width - 1.0f,
		     1.0f - (float)mouse->m_pointers[0].m_y * 2.0f / (float)mouse->m_screen_height);
	Ray ray = m_camera->GetRay(ndc);
	math::v4 sample = m_trail.m_points[m_trail.m_last];

	if (mouse->m_buttons[0].m_down &&
	    !gutil::debug::Keyboard::GetInstance()->m_keys[0xA4].m_down) {
		Vector4 plane_normal(0.0f, 0.0f, 1.0f, 0.0f);
		Vector4 plane_origin(0.0f, 0.0f, 0.0f, 1.0f);
		floatInVec denom = dot(plane_normal, ray.m_direction.getImpl());

		if (!(denom == floatInVec(0.0f)).getAsBool()) {
			floatInVec dist =
				dot(plane_normal, plane_origin - ray.m_origin.getImpl()) *
				recipf(denom);

			if (!(dist < floatInVec(0.0f)).getAsBool() &&
			    (!ray.m_hit || !(dist > ray.m_distance).getAsBool())) {
				ray.m_hit = true;
				ray.m_distance = dist;
				ray.m_hit_point = math::v4(ray.m_origin.getImpl() +
							   ray.m_direction.getImpl() * dist);
			}
		}
		if (ray.m_hit) {
			Vector4 hit_world =
				ray.m_hit_point.getImpl() / floatInVec(m_camera->GetUnit());

			hit_world.setW(1.0f);
			sample = math::v4(hit_world);
		}
	}

	uint32_t slot = m_trail.m_next;

	m_trail.m_last = slot;
	m_trail.m_next = (slot + 1) % m_trail.m_count;
	m_trail.m_points[slot] = sample;
}

void ParticlesPattern2::Emit()
{
	ParticleEmitter *emitter = m_emitter.get();

	for (uint32_t i = 0; i < m_trail.m_count; ++i) {
		int32_t src_idx = WrapIndex((int32_t)m_trail.m_last - (int32_t)i,
					    (int32_t)m_trail.m_reserved);

		m_trail_ordered[i] = m_trail.m_points[src_idx];
	}

	Vector4 fade_mul(1.0f, 1.0f, 1.0f, 1.0f - m_tinkerbell.m_set.m_values.m_fade.m_value);

	for (uint32_t i = 0; i < m_trail.m_count; ++i) {
		int32_t dst_idx = WrapIndex((int32_t)m_trail.m_last - (int32_t)i,
					    (int32_t)m_trail.m_reserved);

		m_trail.m_points[dst_idx] =
			math::v4(mulPerElem(m_trail.m_points[dst_idx].getImpl(), fade_mul));
	}

	float weight_sum = 0.0f;

	for (uint32_t i = 0; i < TRAIL_CAPACITY; ++i) {
		int32_t idx = WrapIndex((int32_t)m_trail.m_last - (int32_t)i,
					(int32_t)m_trail.m_reserved);

		weight_sum += m_trail.m_points[idx].extract_w();
	}
	if (weight_sum < 0.001f)
		return;

	math::v2 pdf[TRAIL_CAPACITY];

	for (uint32_t i = 0; i < TRAIL_CAPACITY; ++i) {
		int32_t idx = WrapIndex((int32_t)m_trail.m_last - (int32_t)i,
					(int32_t)m_trail.m_reserved);

		pdf[i] = math::v2((float)i / 63.0f, m_trail.m_points[idx].extract_w() / weight_sum);
	}

	math::v2 cdf[TRAIL_CAPACITY];

	cdf[0] = pdf[0];
	for (uint32_t i = 1; i < TRAIL_CAPACITY; ++i)
		cdf[i] = math::v2(pdf[i].extract_x(), pdf[i].extract_y() + cdf[i - 1].extract_y());

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
		float uniform_rand = (NextRandomSigned(m_rng_seed) + 1.0f) * 0.5f;
		float curve_param = SampleTrailCdf(cdf, TRAIL_CAPACITY, uniform_rand);
		math::v4 spawn_pos =
			EvaluateTrailSpline(m_trail_ordered, m_trail.m_count, curve_param);
		math::v4 next_pos =
			EvaluateTrailSpline(m_trail_ordered, m_trail.m_count, curve_param + 0.01f);
		Vector4 vel_raw = (spawn_pos.getImpl() - next_pos.getImpl()) /
				  floatInVec(emitter->m_mov.m_delta_time);
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
		m_emitter->PushSetPos(spawn_pos);
		m_emitter->PushSetVel(velocity);
		m_emitter->PushSetAging(aging);
	}
}

void ParticlesPattern2::Update(float frame_time)
{
	m_last_frame = common::FrameCount();
	if (m_enabled) {
		m_manager.Update(frame_time);
		m_emit.m_set.m_values.CopyToRaw(m_emitter->m_emit);
		m_mov.m_set.m_values.CopyToRaw(m_emitter->m_mov);
		m_lit.m_set.m_values.CopyToRaw(m_emitter->m_lit);
		m_focus.m_set.m_values.CopyToRaw(m_emitter->m_focus);
		m_glare.m_set.m_values.CopyToRaw(m_emitter->m_glare);
		UpdateMotion();
		if (m_sample_touch)
			SampleTouch();
		Emit();
		m_emitter->Update();
	}
}

void ParticlesPattern2::Draw()
{
	if (m_enabled)
		m_emitter->Draw();
}

void ParticlesPattern2::SetEnabled(bool enabled)
{
	m_enabled = enabled;
}

}
}
