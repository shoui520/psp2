/*
 * ParticlesGraphics and Particles top-level implementation (2014 PAF).
 */
#include <stdint.h>
#include <gxm.h>
#include <vectormath.h>
#include <graphics/drawobj/effect_path.h>
#include <graphics/color.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/effects/particles.h>
#include "../../std/math.h"

using sce::Vectormath::Simd::recipf;
using sce::Vectormath::Simd::Aos::Quat;
using sce::Vectormath::Simd::Aos::Vector3;
using sce::Vectormath::Simd::Aos::Vector4;

namespace paf {
namespace graph {

namespace {

struct HalfQuad {
	uint16_t m_x;
	uint16_t m_y;
	uint16_t m_z;
	uint16_t m_w;
};

static inline uint16_t FloatToHalf(float value)
{
	union {
		float f;
		uint32_t u;
	} bits;

	bits.f = value;
	if (bits.u == 0)
		return 0;

	int32_t exponent = (int32_t)((bits.u >> 23) & 0xFF) - 112;

	if (exponent < 0)
		return 0;
	if (exponent > 31)
		exponent = 31;

	uint32_t sign = (bits.u >> 31) & 1;
	uint32_t mantissa = (bits.u >> 13) & 0x3FF;

	return (uint16_t)((sign << 15) | ((uint32_t)(exponent & 0x1F) << 10) | mantissa);
}

static HalfQuad PackQuaternionHalf(const math::quaternion &q)
{
	float32x4_t lanes = q.getImpl().get128();
	HalfQuad packed;

	packed.m_x = FloatToHalf(vgetq_lane_f32(lanes, 0));
	packed.m_y = FloatToHalf(vgetq_lane_f32(lanes, 1));
	packed.m_z = FloatToHalf(vgetq_lane_f32(lanes, 2));
	packed.m_w = FloatToHalf(vgetq_lane_f32(lanes, 3));
	return packed;
}

static math::quaternion RotationFromAxisAngle(const Vector3 &axis, float angle)
{
	float half = angle * 0.5f;

	return math::quaternion(Quat(axis * sce_paf_sinf(half), sce_paf_cosf(half)));
}

static void CreateGlareRampTexture(common::SharedPtr<arl::Texture2D> &out_tex, uint32_t width)
{
	arl::ArlInstance *gl = arl::Gl();

	out_tex = arl::CreateTexture<ImageMode_L8>(gl->m_default_allocator,
						   arl::SurfaceSize(width, 1), NULL,
						   arl::SurfaceFlags(), 0);

	vector<uint8_t> pixels;
	int32_t tex_w = out_tex->m_desc.m_width;
	int32_t tex_h = out_tex->m_desc.m_height;

	for (int32_t y = 0; y != tex_h; ++y) {
		for (int32_t x = 0; x != tex_w; ++x) {
			float sample = sce_paf_atanf(((float)x / (float)(tex_w - 1)) * 10.0f) /
				       1.5707964f;

			if (sample < 0.0f)
				sample = 0.0f;
			if (sample > 1.0f)
				sample = 1.0f;
			pixels.push_back((uint8_t)(sample * 255.0f));
		}
	}

	ScePafGraphics_309D2902(out_tex.get(), 0, out_tex->m_desc.m_width,
				out_tex->m_desc.m_height, out_tex->m_desc.m_format,
				out_tex->m_desc.m_levels, &pixels[0]);
	out_tex->setWrapS(arl::TEXTURE_WRAP_CLAMP_FULL_BORDER);
	out_tex->setWrapT(arl::TEXTURE_WRAP_CLAMP_FULL_BORDER);
}

} /* namespace */

ParticlesGraphics::ParticlesGraphics(const string &dir, uint32_t max_particles,
				     common::SharedPtr<arl::VramAllocator> allocator)
	: m_max_particles(max_particles), m_allocator(allocator)
{
	arl::ArlInstance *gl = arl::Gl();

	m_instance_spec = common::SharedPtr<arl::VertexArraySpec>(
		new arl::VertexArraySpec(SCE_GXM_INDEX_SOURCE_INSTANCE_16BIT));
	m_instance_spec->AddAttribute(0, 4, SCE_GXM_ATTRIBUTE_FORMAT_F32, 16, 0);

	common::SharedPtr<arl::VertexArraySpec> quad_spec =
		ScePafGraphics_4D1E289A(gl, arl::VertexArraySpec::ATTRIB3_F2,
					SCE_GXM_INDEX_SOURCE_INDEX_16BIT);
	common::SharedPtr<arl::VertexBuffer> quad_buffer(new arl::VertexBuffer(allocator));

	m_quad_vertices = arl::VertexArray(quad_spec, quad_buffer);

	arl::SurfaceMemory *quad_mem = m_quad_vertices.m_buffer->m_memory.get();
	uint32_t quad_bytes = 6 * quad_spec->m_stride;

	if (quad_mem->m_addr == NULL) {
		quad_mem->m_allocator->Alloc(quad_mem, quad_bytes);
		quad_mem->m_size = quad_bytes;
	}

	math::v2 *quad_uv = (math::v2 *)m_quad_vertices.m_buffer->m_memory->m_addr;

	quad_uv[0] = math::v2(0.0f, 0.0f);
	quad_uv[1] = math::v2(1.0f, 0.0f);
	quad_uv[2] = math::v2(1.0f, 1.0f);
	quad_uv[3] = math::v2(0.0f, 0.0f);
	quad_uv[4] = math::v2(1.0f, 1.0f);
	quad_uv[5] = math::v2(0.0f, 1.0f);

	static const uint16_t s_quad_indices[6] = { 0, 1, 2, 3, 4, 5 };

	m_quad_indices = arl::IndexArray(gl->m_default_allocator, s_quad_indices, 6,
					 SCE_GXM_INDEX_FORMAT_U16);

	m_iridescent_tex = common::SharedPtr<arl::Texture2D>(new arl::Texture2D(allocator));
	if (dir.find("vs0") == string::npos)
		ScePafGraphics_7C53C58A(m_iridescent_tex, s_particles_path + "/proc_iridescent.dds",
					arl::SurfaceFlags());
	else
		ScePafGraphics_7C53C58A(m_iridescent_tex,
					dir + "/particles/textures/proc_iridescent.dds",
					arl::SurfaceFlags());

	m_shaders = common::SharedPtr<ParticlesShaders>(new ParticlesShaders());

	common::SharedPtr<arl::VertexArraySpec> no_spec;

	m_shaders->m_vert.m_object.init(*gl, m_instance_spec, m_quad_vertices.m_spec, no_spec,
					no_spec);
	m_shaders->m_frag.m_object.init(*gl);
	m_shaders->m_glare_vert.m_object.init(*gl, m_instance_spec, m_quad_vertices.m_spec, no_spec,
					      no_spec);
	m_shaders->m_glare_frag.m_object.init(*gl);
	m_shaders->m_front_facing_vert.m_object.init(*gl, m_instance_spec, m_quad_vertices.m_spec,
						     no_spec, no_spec);
	m_shaders->m_debug_vert.m_object.init(*gl, m_instance_spec, m_quad_vertices.m_spec, no_spec,
					      no_spec);
	m_shaders->m_debug_frag.m_object.init(*gl);

	vector<HalfQuad> rot_pixels;
	math::quaternion cur_quat = math::quaternion::Identity();
	const float two_pi = 6.2831855f;
	const float phase_third = 2.0943952f;
	const float phase_two_thirds = 4.1887903f;
	const float step_angle = 0.87266463f;

	for (int32_t step = 0; step != 128; ++step) {
		float theta = ((float)step / 127.0f) * two_pi;
		Vector3 axis(sce_paf_cosf(theta), sce_paf_cosf(theta + phase_third),
			     sce_paf_cosf(theta + phase_two_thirds));
		Vector3 axis_unit = axis * recipf(length(axis));
		math::quaternion delta_quat = RotationFromAxisAngle(axis_unit, step_angle);
		math::quaternion prod_quat = cur_quat * delta_quat;
		Vector4 blended = Vector4(cur_quat.getImpl()) +
				  (Vector4(prod_quat.getImpl()) * 0.5f) * (1.0f / 3.0f);

		cur_quat = Pose::Normalize(math::quaternion(Quat(blended)));
		rot_pixels.push_back(PackQuaternionHalf(cur_quat));
	}

	common::SharedPtr<arl::Texture2D> rot_tex = arl::CreateTexture<ImageMode_WZYX16FP>(
		m_allocator, arl::SurfaceSize(128, 1), &rot_pixels[0], arl::SurfaceFlags(), 0);

	rot_tex->setWrapS(arl::TEXTURE_WRAP_CLAMP_FULL_BORDER);
	rot_tex->setWrapT(arl::TEXTURE_WRAP_REPEAT);
	m_rotations_tex = arl::TextureSource(common::SharedPtr<arl::TextureObject>(rot_tex));

	m_radius_tex = ScePafGraphics_27C5476A(m_allocator, 64, arl::SurfaceFlags(), false, false);
	CreateGlareRampTexture(m_glare_tex, 128);
}

ParticlesGraphics::~ParticlesGraphics()
{
}

void ParticlesGraphics::InitEmitterRender(ParticleEmitterRender *render, uint32_t max_particles)
{
	render->m_max_particles = max_particles;
	for (uint32_t slot = 0; slot < 2; ++slot) {
		common::SharedPtr<arl::VertexBuffer> buffer(new arl::VertexBuffer(m_allocator));
		arl::SurfaceMemory *memory = buffer->m_memory.get();
		uint32_t buffer_size = 16 * m_max_particles;

		if (memory->m_addr == NULL) {
			memory->m_allocator->Alloc(memory, buffer_size);
			memory->m_size = buffer_size;
		}
		render->m_slots[slot].m_vertices = arl::VertexArray(m_instance_spec, buffer);
	}
}

void Particles::Init(common::SharedPtr<Camera> camera, common::SharedPtr<DrawEnv> draw_env)
{
	m_pattern1.Init(camera, draw_env);
	m_pattern2.Init(camera, draw_env);
	m_camera = camera;
	m_draw_env = draw_env;
}

void Particles::Load(const string &dir_ambient, uint32_t max_ambient, const string &dir_trail,
		     uint32_t max_trail)
{
	uint32_t shared_max = max_ambient < max_trail ? max_trail : max_ambient;

	m_graphics = common::SharedPtr<ParticlesGraphics>(
		new ParticlesGraphics(dir_ambient, shared_max, arl::Gl()->m_default_allocator));
	m_pattern1.Load(dir_ambient, m_graphics, max_ambient);
	m_pattern2.Load(dir_trail, m_graphics, max_trail);
	m_pattern1.m_enabled = true;
	m_pattern2.SetEnabled(false);
}

void Particles::Update(float frame_time)
{
	m_pattern1.Update(frame_time);
	m_pattern2.Update(frame_time);
}

}
}
