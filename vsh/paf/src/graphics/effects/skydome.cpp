/*
 * The sky over the waves: a grid of 32 by 64 vertices the vertex program bends into a dome
 * around the eye ("paf/cg/drawobj/wave/skydome.vp"), its colours from the sky's parameters
 * (the file's name ours).
 */
#include <stdint.h>
#include <vectormath.h>
#include <graphics/color.h>
#include <graphics/effects/wave_renderer.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/camera.h>
#include "../../std/math.h"
#include <graphics/drawobj/effect_path.h>

using namespace sce::Vectormath::Simd::Aos;
using sce::Vectormath::Simd::floatInVec;

namespace paf {
namespace graph {

/* each vertex where it is in the grid (name ours) */
class SkydomeVertexGenerator : public VertexGenerator<SkydomeVertex> {
public:
	virtual SkydomeVertex operator()(const math::v2 &uv) const;
};

Skydome::Skydome() : m_blend(true), m_wireframe(false)
{
}

void Skydome::Init(arl::SurfaceSize size)
{
	SkydomeVertexGenerator generator;
	arl::ArlInstance *gl = arl::Gl();

	m_mesh = WaveMesh<SkydomeVertex>(size,
					 ScePafGraphics_4D1E289A(gl, arl::VertexArraySpec::ATTRIB0_F2,
								 SCE_GXM_INDEX_SOURCE_INDEX_16BIT),
					 gl->m_default_allocator);
	m_mesh.Fill(generator);

	arl::ProgramState *state = gl;

	m_vert.m_object.init(*gl, state->m_patcher, m_mesh.m_vertices.m_spec,
			     common::SharedPtr<arl::VertexArraySpec>(),
			     common::SharedPtr<arl::VertexArraySpec>(),
			     common::SharedPtr<arl::VertexArraySpec>());
	state = gl;
	m_frag.m_object.init(*gl, state->m_patcher);
}

void Skydome::SetCamera(common::SharedPtr<Camera> camera)
{
	m_camera = camera;
}

void Skydome::Draw(const DrawParam &param, const SkyParamSet::Values &values)
{
	arl::ArlInstance *gl = arl::Gl();

	if (m_wireframe)
		gl->PolygonMode(arl::POLYGON_MODE_LINE, arl::FRONT_AND_BACK);
	if (m_blend) {
		gl->BlendFunc(arl::SRC_ALPHA, arl::ONE_MINUS_SRC_ALPHA, arl::ONE,
			      arl::ONE_MINUS_SRC_ALPHA);
		gl->BlendEquation(arl::FUNC_ADD, arl::FUNC_ADD);
		gl->Enable(arl::BLEND);
	} else {
		gl->Disable(arl::BLEND);
	}
	gl->Disable(arl::DEPTH_TEST);
	gl->CullMode(arl::CULL_CW);
	gl->m_projection_changed = true;
	gl->m_view_changed = true;
	m_vert.m_interface.m_vp.Set(gl->m_projection.Top().m_matrix * gl->m_view.Top().m_matrix);
	m_vert.m_object.begin();
	m_frag.m_object.begin();
	gl->m_model.Push();

	/* the dome moved to the eye the camera was reset to (MatrixStack::MultMatrix's body,
	   inlined in Sony's build) */
	Vector4 eye = m_camera->GetResetWorld().w().getImpl() /
		      floatInVec(param.m_world_scale_hack);
	arl::MatrixStack::Entry &moved = gl->m_model.Top();

	moved.m_matrix = moved.m_matrix * math::matrix(Matrix4::translation(eye.getXYZ()));
	moved.m_inverse_valid = false;
	moved.m_inverse_entry = gl->m_model.m_depth - 1;
	moved.m_rigid &= true;

	/* and scaled to its radius, wider by xscale (MatrixStack::Scale's body) */
	float radius = values.m_radius.m_value;
	arl::MatrixStack::Entry &scaled = gl->m_model.Top();

	scaled.m_matrix = scaled.m_matrix *
			  math::matrix(Matrix4::scale(Vector3(radius * values.m_xscale.m_value,
							      radius, radius)));
	scaled.m_rigid = false;
	scaled.m_inverse_valid = false;
	scaled.m_inverse_entry = gl->m_model.m_depth - 1;
	{
		common::SharedPtr<arl::Texture2D> texture = m_texture;
		common::SharedPtr<arl::TextureObject> object(texture);
	}
	m_vert.m_interface.m_m.Set(gl->m_model.Top().m_matrix);
	m_vert.m_interface.m_world_scale_hack.Set(param.m_world_scale_hack);
	m_vert.m_interface.m_horizon_angle.Set(math::v4(values.m_horizon_angle.m_value,
							values.m_side_angle.m_value,
							values.m_horizon_blend_range.m_value,
							values.m_horizon_curvature.m_value));

	Vector4 eye_pos = gl->m_view.GetInverse().w().getImpl() / floatInVec(param.m_world_scale_hack);

	m_vert.m_interface.m_eye_pos.Set(math::v3(eye_pos.getXYZ()));

	/* the sun at theta from the zenith, phi around it */
	Vector2 theta_phi = values.m_theta_phi.m_value.getImpl();
	float theta = theta_phi.getX();
	float sin_theta = sce_paf_sinf(theta);
	float x = sin_theta * sce_paf_cosf(theta_phi.getY());
	float z = sin_theta * sce_paf_sinf(theta_phi.getY());
	Vector4 sun = Vector4(z, sce_paf_cosf(theta), x, 0.0f) * values.m_distance.m_value;

	m_vert.m_interface.m_sun_pos.Set(math::v3(sun.getXYZ()));
	m_vert.m_interface.m_wave_fog_color.Set(
		math::v4(Vector4(param.m_wave_fog_color.getImpl().getXYZ(),
				 values.m_wave_fog_alpha.m_value)));
	m_vert.m_interface.m_sun_color.Set(values.m_sun_color.m_value);
	m_vert.m_interface.m_sun_power.Set(values.m_sun_power.m_value);
	m_vert.m_interface.m_sun_control.Set(values.m_sun_control.m_value);
	m_vert.m_interface.m_zenith_color.Set(values.m_zenith_color.m_value);
	m_vert.m_interface.m_horizon_color.Set(values.m_horizon_color.m_value);
	m_vert.m_interface.m_wave_nor_tex.Set(param.m_wave_nor_tex);
	m_vert.m_interface.m_wave_model_matrix.Set(param.m_wave_model_matrix);
	m_vert.m_interface.m_distortion_scale.Set(
		math::v3(values.m_pos_distortion_scale.m_value * param.m_distortion_scale,
			 values.m_grad_distortion_scale.m_value * 10.0f,
			 values.m_horizon_distortion_scale.m_value * 10.0f));

	float blend_start = values.m_sky_blend_start.m_value;

	m_vert.m_interface.m_color_blend_range.Set(
		math::v2(blend_start, blend_start + values.m_sky_blend_range.m_value));
	if (m_wireframe)
		m_mesh.DrawLines(gl);
	else if (m_mesh.m_index_count != 0)
		m_mesh.Draw(gl, m_mesh.m_height - 1);
	gl->m_model.Pop();
	if (m_wireframe)
		gl->PolygonMode(arl::POLYGON_MODE_FILL, arl::FRONT_AND_BACK);
	m_frag.m_object.end();
	m_vert.m_object.end();
}

SkydomeVertex SkydomeVertexGenerator::operator()(const math::v2 &uv) const
{
	SkydomeVertex vertex;

	vertex.m_uv = uv;
	return vertex;
}

}
}
