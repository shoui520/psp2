/*
 * ParticleEmitter and GlareFalloffCache implementation (2014 PAF).
 */
#include <stdint.h>
#include <gxm.h>
#include <vectormath.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/color.h>
#include <graphics/effects/particles.h>
#include <src/common/frame_count.h>
#include "../../std/math.h"

using sce::Vectormath::Simd::floatInVec;
using sce::Vectormath::Simd::Aos::Matrix4;
using sce::Vectormath::Simd::Aos::Vector3;
using sce::Vectormath::Simd::Aos::Vector4;

namespace paf {
namespace graph {

namespace {

/* park-miller LCG producing uniform floats in [-1.0f, 1.0f) */
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

static inline math::v4 ReplaceW(const math::v4 &vec, float w)
{
	Vector4 impl = vec.getImpl();

	impl.setW(w);
	return math::v4(impl);
}

static inline math::matrix ComputeInverseModelView(arl::ArlInstance *gl)
{
	return gl->m_model.GetInverse() * gl->m_view.GetInverse();
}

} /* namespace */

common::SharedPtr<arl::Texture2D> GlareFalloffCache::Get(float power, float falloff)
{
	if (m_texture.get() == NULL) {
		arl::ArlInstance *gl = arl::Gl();

		m_texture = arl::CreateTexture<ImageMode_L8>(gl->m_default_allocator,
							     arl::SurfaceSize(64, 1), NULL,
							     arl::SurfaceFlags(), 0);
		m_texture->setWrapS(arl::TEXTURE_WRAP_CLAMP_FULL_BORDER);
		m_texture->setWrapT(arl::TEXTURE_WRAP_REPEAT);
	}
	if (power != m_power || falloff != m_falloff) {
		m_power = power;
		m_falloff = falloff;

		vector<uint8_t> pixels;
		int32_t tex_w = m_texture->m_desc.m_width;
		int32_t tex_h = m_texture->m_desc.m_height;

		for (int32_t y = 0; y != tex_h; ++y) {
			for (int32_t x = 0; x != tex_w; ++x) {
				float norm_x = (float)x / (float)(tex_w - 1);
				float curve = sce_paf_powf(norm_x, m_power);
				float value = sce_paf_expf(-(curve * m_falloff));

				if (value < 0.0f)
					value = 0.0f;
				if (value > 1.0f)
					value = 1.0f;
				pixels.push_back((uint8_t)(value * 255.0f));
			}
		}

		ScePafGraphics_309D2902(m_texture.get(), 0, m_texture->m_desc.m_width,
					m_texture->m_desc.m_height, m_texture->m_desc.m_format,
					m_texture->m_desc.m_levels, &pixels[0]);
	}
	return m_texture;
}

ParticleEmitter::ParticleEmitter(const string &name, uint32_t max_particles,
				 common::SharedPtr<ParticlesGraphics> graphics,
				 common::SharedPtr<Camera> camera)
	: m_name(name), m_camera(camera), m_max_particles(max_particles),
	  m_particles(max_particles), m_gravity(0.0f, -0.000005f, 0.0f, 0.0f),
	  m_friction(0.01f, 0.01f, 0.01f, 0.01f),
	  m_bounds_min(-10000.0f, -10000.0f, -10000.0f, -10000.0f),
	  m_bounds_max(10000.0f, 10000.0f, 10000.0f, 10000.0f), m_paused(false),
	  m_wind(0.0f, 0.0f, 0.0f, 0.0f), m_turbulence_unused(0.2f), m_brownian_scale(0.0f),
	  m_size_middle_copy(0.05f), m_spin_time_scale(2.74f),
	  m_delta_time_vec(1.0f / 60.0f, 1.0f / 60.0f, 1.0f / 60.0f, 1.0f),
	  m_cmd_data(NULL), m_cmd_count(0), m_cmd_capacity(0),
	  m_free_indices(max_particles), m_last_allocated(-1), m_active_count(0),
	  m_draw_enabled(true), m_debug_enabled(false), m_render_mode(2),
	  m_front_facing_forced(false), m_sort_enabled(false), m_write_ptr(NULL),
	  m_write_count(0), m_write_capacity(0), m_render(NULL), m_graphics(graphics),
	  m_hidden(false), m_last_frame(-1), m_clear_bounds_once(false), m_cmd_fill_ratio(0.0f)
{
	ParticleCommand empty_cmd;

	sce_paf_memset(&empty_cmd, 0, sizeof(empty_cmd));
	m_cmd_storage.resize(max_particles, empty_cmd);
	m_cmd_data = m_cmd_storage.empty() ? NULL : &m_cmd_storage[0];
	m_cmd_capacity = m_cmd_storage.size();
	m_cmd_count = 0;

	m_emit.m_vel_min = 0.001f;
	m_emit.m_vel_mul = 0.19f;
	m_emit.m_vel_var = 0.05f;
	m_emit.m_vel_zscale = 0.5f;
	m_emit.m_wait_time = 2.0f;
	m_emit.m_wait_time_var = 0.15f;
	m_emit.m_aging_speed = 0.001f;
	m_emit.m_aging_speed_var = 0.0f;
	m_emit.m_lifebounds_center = math::v3(0.0f, 0.0f, 0.0f);
	m_emit.m_lifebounds_size = math::v3(5.0f, 5.0f, 5.0f);

	m_mov.m_friction = 0.030551f;
	m_mov.m_spin_time_scale = 2.74f;
	m_mov.m_delta_time = 0.03f;
	m_mov.m_gravity = -0.000068f;
	m_mov.m_wind_dir = math::v3(0.152f, 0.0f, 0.35f);
	m_mov.m_wind_scale = 0.004f;
	m_mov.m_brownian_scale = 0.0025f;
	m_mov.m_brownian_scale_shake = 6.0f;

	m_lit.m_light_pos = math::v3(4.16f, 2.63f, -7.6f);
	m_lit.m_light_attn = math::v3(1.0f, 0.0f, 0.0f);
	m_lit.m_specular_power = 13.94f;
	m_lit.m_specular_coeff = 74.74f;
	m_lit.m_lambert_coeff = 8.0f;
	m_lit.m_exposure = 0.0537f;
	m_lit.m_fresnel = 1.24f;
	m_lit.m_color_control = 1.0f;
	m_lit.m_alpha_unused = 1.0f;
	m_lit.m_global_alpha = 0.93f;

	m_focus.m_size_middle = 0.0845f;
	m_focus.m_size_near = 0.17f;
	m_focus.m_size_far = 0.182f;
	m_focus.m_near_focus = 3.88f;
	m_focus.m_near_focus_dist = 3.4f;
	m_focus.m_near_focus_pow = 3.05f;
	m_focus.m_near_darkness = 1.0f;
	m_focus.m_near_fuzziness = 3.79f;
	m_focus.m_far_focus = 10.35f;
	m_focus.m_far_focus_dist = 2.4f;
	m_focus.m_far_focus_pow = 1.72f;
	m_focus.m_far_darkness = 13.9f;
	m_focus.m_near_align = 0.722f;
	m_focus.m_size_align = 13.19f;
	m_focus.m_global_align = 0.0f;

	m_glare.m_glare = 1.0f;
	m_glare.m_glare_scale = 4.16f;
	m_glare.m_glare_power = 0.99f;
	m_glare.m_glare_falloff = 4.44f;

	m_particles.m_count = m_particles.m_capacity;
	m_render = new ParticleEmitterRender();
	m_graphics->InitEmitterRender(m_render, m_max_particles);
	Reset();
}

ParticleEmitter::~ParticleEmitter()
{
	delete m_render;
	m_render = NULL;
}

void ParticleEmitter::Reset()
{
	m_free_indices.m_count = 0;
	for (uint32_t i = 0; i < m_particles.m_capacity; ++i) {
		m_free_indices.m_data[m_free_indices.m_count++] = i;
		m_particles.m_data[i].m_pos_age =
			ReplaceW(m_particles.m_data[i].m_pos_age, -666.0f);
	}
}

void ParticleEmitter::AllocParticle()
{
	m_last_allocated = -1;
	if (m_free_indices.m_count != 0) {
		uint32_t free_top = m_free_indices.m_count - 1;
		uint32_t index = m_free_indices.m_data[free_top];

		m_last_allocated = (int32_t)index;
		m_free_indices.m_count = free_top;
		m_particles.m_data[index].m_pos_age =
			ReplaceW(m_particles.m_data[index].m_pos_age, 1.0f);
		if (m_last_allocated >= 0) {
			ParticleState &state = m_particles.m_data[m_last_allocated];

			state.m_pos_age = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
			state.m_vel_aging = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
		}
	}
}

void ParticleEmitter::StepParticles()
{
	uint32_t count = m_particles.m_count;
	uint32_t seed_x = 0x98756161u;
	uint32_t seed_y = 556943497u;
	uint32_t seed_z = 0x82181158u;
	ParticleState *particle = m_particles.m_data;

	m_active_count = 0;
	for (uint32_t index = 0; index != count; ++index, ++particle) {
		if (particle->m_pos_age.extract_w() == -666.0f)
			continue;

		bool alive = true;

		if (!m_paused) {
			float rx = NextRandomSigned(seed_x);
			float ry = NextRandomSigned(seed_y);
			float rz = NextRandomSigned(seed_z);
			Vector4 brownian(rx, ry, rz, 0.0f);
			Vector4 gravity = m_gravity.getImpl();
			Vector4 friction = m_friction.getImpl();
			Vector4 force = m_wind.getImpl() + brownian * floatInVec(m_brownian_scale);

			gravity.setW(0.0f);
			friction.setW(0.0f);
			force.setW(0.0f);

			Vector4 dt = m_delta_time_vec.getImpl();
			Vector4 vel = particle->m_vel_aging.getImpl();
			Vector4 pos = particle->m_pos_age.getImpl();

			vel = vel + mulPerElem((gravity + force) - mulPerElem(vel, friction), dt);
			particle->m_vel_aging = math::v4(vel);
			pos = pos + mulPerElem(vel, dt);
			particle->m_pos_age = math::v4(pos);

			float age = pos.getW().getAsFloat();

			if (age >= 0.99999f) {
				alive = false;
			} else {
				float px = pos.getX().getAsFloat();
				float py = pos.getY().getAsFloat();
				float pz = pos.getZ().getAsFloat();

				if (px < m_bounds_min.extract_x() ||
				    px > m_bounds_max.extract_x() ||
				    py < m_bounds_min.extract_y() ||
				    py > m_bounds_max.extract_y() ||
				    pz < m_bounds_min.extract_z() ||
				    pz > m_bounds_max.extract_z())
					alive = false;
			}
		}

		if (alive) {
			++m_active_count;
			if (m_write_count < m_write_capacity)
				m_write_ptr[m_write_count++] = particle->m_pos_age;
		} else if (particle->m_pos_age.extract_w() != -666.0f) {
			m_free_indices.m_data[m_free_indices.m_count++] = index;
			particle->m_pos_age = ReplaceW(particle->m_pos_age, -666.0f);
		}
	}
}

void ParticleEmitter::Update()
{
	Vector4 half_size = Vector4(m_emit.m_lifebounds_size.getImpl()) * floatInVec(0.5f);
	Vector4 center(m_emit.m_lifebounds_center.getImpl());

	m_bounds_min = math::v4(center - half_size);
	m_bounds_max = math::v4(center + half_size);
	if (m_clear_bounds_once) {
		m_clear_bounds_once = false;
		m_bounds_min = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
		m_bounds_max = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	m_last_frame = common::FrameCount();
	m_cmd_fill_ratio = (float)m_cmd_count / (float)m_cmd_capacity;

	if (!m_paused) {
		for (uint32_t i = 0; i != m_cmd_count; ++i) {
			ParticleCommand &cmd = m_cmd_data[i];

			if (cmd.m_op == ParticleCommand::OP_ALLOC)
				AllocParticle();
			if (cmd.m_index < 0)
				cmd.m_index = m_last_allocated;
			if (cmd.m_index >= 0) {
				ParticleState &state = m_particles.m_data[cmd.m_index];

				switch (cmd.m_op) {
				case ParticleCommand::OP_SET_POS:
					state.m_pos_age = math::v4(cmd.m_x, cmd.m_y, cmd.m_z,
								   state.m_pos_age.extract_w());
					break;
				case ParticleCommand::OP_SET_VEL:
					state.m_vel_aging = math::v4(cmd.m_x, cmd.m_y, cmd.m_z,
								     state.m_vel_aging.extract_w());
					break;
				case ParticleCommand::OP_ADD_VEL:
					state.m_vel_aging = math::v4(
						state.m_vel_aging.getImpl() +
						Vector4(cmd.m_x, cmd.m_y, cmd.m_z, 0.0f));
					break;
				case ParticleCommand::OP_SET_AGING:
					state.m_vel_aging = ReplaceW(state.m_vel_aging, cmd.m_x);
					break;
				default:
					break;
				}
			}
		}
	}
	m_cmd_count = 0;

	uint32_t write_slot = common::FrameCount() & 1;

	m_render->m_slots[write_slot].m_count = 0;
	m_write_ptr = (math::v4 *)m_render->m_slots[write_slot].m_vertices.m_buffer->m_memory->m_addr;
	m_write_count = 0;
	m_write_capacity = m_render->m_max_particles;
	StepParticles();
	m_render->m_slots[common::FrameCount() & 1].m_count += m_write_count;
}

void ParticleEmitter::DrawPass(ParticlesVertParamsBase *vert, arl::VertShaderObject *vert_obj,
			       ParticlesFragParamsBase *frag, arl::FragShaderObject *frag_obj,
			       float unit, math::v4 color)
{
	arl::ArlInstance *gl = arl::Gl();
	uint32_t read_slot = 1 - (common::FrameCount() & 1);

	if (m_render->m_slots[read_slot].m_count == 0 || m_hidden)
		return;

	m_camera->PushMatrices();
	gl->CullMode(arl::CULL_NONE);
	gl->Disable(arl::STENCIL_TEST);
	gl->Disable(arl::DEPTH_TEST);
	gl->DepthMask(false, arl::FRONT_AND_BACK);

	vert->SetGlareRampTex(
		arl::TextureSource(common::SharedPtr<arl::TextureObject>(m_graphics->m_glare_tex)));
	vert->SetWorldScaleHack(unit);

	math::matrix mvp = gl->m_projection.Top().m_matrix * gl->m_view.Top().m_matrix *
			   gl->m_model.Top().m_matrix;

	vert->SetModelviewProjection(mvp);

	math::matrix mv = gl->m_view.Top().m_matrix * gl->m_model.Top().m_matrix;

	vert->SetModelview(mv);
	vert->SetParticleSize(math::v3(m_focus.m_size_middle,
				       m_focus.m_size_near - m_focus.m_size_middle,
				       m_focus.m_size_far - m_focus.m_size_middle));
	vert->SetDarkness(math::v2(m_focus.m_near_darkness, m_focus.m_far_darkness));

	Vector4 eye = m_camera->m_transform.m_world.w().getImpl() / floatInVec(unit);

	eye.setW(1.0f);

	Vector4 light_pos = Vector4(m_lit.m_light_pos.getImpl()) / floatInVec(unit);

	light_pos.setW(m_lit.m_specular_power);

	Vector4 light_attn(m_lit.m_light_attn.getImpl());

	light_attn.setW(0.0f);

	Vector4 light_terms(m_lit.m_lambert_coeff, m_lit.m_specular_coeff, m_lit.m_exposure,
			    m_lit.m_color_control);

	vert->SetLightPack(math::matrix(Matrix4(eye, light_pos, light_attn, light_terms)));
	vert->SetTransparency(math::v2(m_lit.m_fresnel, m_lit.m_global_alpha));

	float near_end = m_focus.m_near_focus + m_focus.m_near_focus_dist;
	float far_end = m_focus.m_far_focus + m_focus.m_far_focus_dist;

	vert->SetFocus(math::v4(m_focus.m_near_focus, near_end, m_focus.m_far_focus, far_end));
	vert->SetFocusRangesRcp(math::v2(1.0f / (m_focus.m_near_focus - near_end),
					 1.0f / (far_end - m_focus.m_far_focus)));

	math::matrix inv_mv_quat = ComputeInverseModelView(gl);

	vert->SetFrontFacingQuaternion(
		math::v4(Vector4(math::quaternion(inv_mv_quat).getImpl())));

	math::matrix inv_mv_mat = ComputeInverseModelView(gl);

	vert->SetFrontFacingMatrix(inv_mv_mat);
	vert->SetNearControl(math::v4(m_focus.m_size_align, m_focus.m_near_fuzziness,
				      m_focus.m_near_align, 0.0f));
	vert->SetLifeBoundsMin(math::v3(m_bounds_min.getImpl().getXYZ()));
	vert->SetLifeBoundsMax(math::v3(m_bounds_max.getImpl().getXYZ()));
	vert->SetFocusCurves(math::v3(m_focus.m_near_focus_pow, m_focus.m_far_focus_pow,
				      2.0f / m_camera->m_projection.GetFovY()));

	math::v4 glare_vec(m_glare.m_glare, m_glare.m_glare_scale, m_glare.m_glare_power,
			   m_glare.m_glare_falloff);

	vert->SetGlare(glare_vec);
	vert->SetGlobalAlign(m_focus.m_global_align);
	frag->SetColor(color);
	frag->SetGlare(glare_vec);
	frag->SetGlareTex(arl::TextureSource(common::SharedPtr<arl::TextureObject>(
		m_render->m_glare_cache.Get(m_glare.m_glare_power, m_glare.m_glare_falloff))));
	frag->SetRadiusTex(
		arl::TextureSource(common::SharedPtr<arl::TextureObject>(m_graphics->m_radius_tex)));
	vert->SetIridescentTex(arl::TextureSource(
		common::SharedPtr<arl::TextureObject>(m_graphics->m_iridescent_tex)));
	vert->SetColor(math::v3(color.getImpl().getXYZ()));
	vert->SetRotationsTex(m_graphics->m_rotations_tex);

	vert_obj->begin();
	frag_obj->begin();

	gl->m_dirty_vertex |= m_graphics->m_quad_vertices.m_spec->m_kinds;
	gl->m_dirty_fragment |= 2;
	sceGxmSetVertexStream(gl->m_context, 1,
			      m_graphics->m_quad_vertices.m_buffer->m_memory->m_addr);

	ParticleRenderSlot &slot = m_render->m_slots[1 - (common::FrameCount() & 1)];

	gl->m_dirty_vertex |= slot.m_vertices.m_spec->m_kinds;
	gl->m_dirty_fragment |= 1;
	sceGxmSetVertexStream(gl->m_context, 0, slot.m_vertices.m_buffer->m_memory->m_addr);

	uint32_t draw_count = m_render->m_slots[1 - (common::FrameCount() & 1)].m_count;

	ScePafGraphics_AE5BBA4B(&m_graphics->m_quad_indices, gl, arl::TRIANGLES, 6 * draw_count, 6);

	gl->m_dirty_vertex = 0;
	gl->m_dirty_fragment = 0;
	frag_obj->end();
	vert_obj->end();
	m_camera->PopMatrices();
}

void ParticleEmitter::DrawWithColor(float unit, math::v4 color)
{
	uint32_t read_slot = 1 - (common::FrameCount() & 1);

	if (m_render->m_slots[read_slot].m_count == 0 || m_hidden)
		return;

	arl::ArlInstance *gl = arl::Gl();

	if (color.extract_x() == 0.0f && color.extract_y() == 0.0f && color.extract_z() == 0.0f)
		return;

	ScePafGraphics_199BD5F2(gl, 0);
	gl->Enable(arl::BLEND);
	gl->BlendFunc(arl::ONE, arl::ONE, arl::ONE, arl::ONE);
	gl->BlendEquation(arl::FUNC_ADD, arl::FUNC_ADD);

	if (((m_render_mode + 1) & 1) != 0) {
		ParticlesShaders *shaders = m_graphics->m_shaders.get();
		ParticlesVertParamsBase *vert_params;
		arl::VertShaderObject *vert_obj;

		if (m_focus.m_global_align > 0.999f) {
			vert_params = &shaders->m_front_facing_vert.m_interface;
			vert_obj = &shaders->m_front_facing_vert.m_object;
		} else {
			vert_params = &shaders->m_vert.m_interface;
			vert_obj = &shaders->m_vert.m_object;
		}
		DrawPass(vert_params, vert_obj, &shaders->m_frag.m_interface,
			 &shaders->m_frag.m_object, unit, color);
	}
	if (((m_render_mode + 1) & 2) != 0) {
		ParticlesShaders *shaders = m_graphics->m_shaders.get();

		DrawPass(&shaders->m_glare_vert.m_interface, &shaders->m_glare_vert.m_object,
			 &shaders->m_glare_frag.m_interface, &shaders->m_glare_frag.m_object, unit,
			 color);
	}

	gl->BlendEquation(arl::FUNC_ADD, arl::FUNC_ADD);
	ScePafGraphics_29F5AA8B(gl, NULL, false);
}

void ParticleEmitter::Draw()
{
	if (m_last_frame != -1) {
		m_camera->PushMatrices();
		DrawWithColor(m_camera->GetUnit(), s_color_white);
		m_camera->PopMatrices();
	}
}

}
}
