/*
 * The wave renderer (the file's name ours): wave instances, each an FFT surface drawn over a
 * grid bent along its cross section, the textures their positions, normals and lit colours are
 * drawn into, and the renderer that keeps them, the sky and the parameters they are drawn with.
 */
#include <stdint.h>
#include <arm_neon.h>
#include <vectormath.h>
#include <graphics/drawobj/effect_path.h>
#include <graphics/color.h>
#include <graphics/effects/wave_renderer.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/camera.h>
#include <graphics/drawobj/draw_env.h>
#include <math/math.h>
#include <src/common/frame_count.h>
#include <src/paf_types.h>
#include <graphics/effects/post_effects.h>
#include <graphics/effects/fft_wave.h>
#include "../../std/math.h"

using sce::Vectormath::Simd::floatInVec;
using sce::Vectormath::Simd::Aos::Vector2;
using sce::Vectormath::Simd::Aos::Vector3;
using sce::Vectormath::Simd::Aos::Vector4;

namespace paf {
namespace graph {

/* a flat grid: every vertex where it is in the grid, its normal the same (name ours) */
class FlatWaveVertex : public VertexGenerator<WaveVertex> {
public:
	virtual WaveVertex operator()(const math::v2 &uv) const;
};

/* the layout of the waves' vertices, made the first time it is asked for */
static common::SharedPtr<arl::VertexArraySpec> s_wave_vertex_spec;

math::v2 WaveInstance::GetProfilePoint(float t)
{
	Vector3 scale = m_values.m_scale.m_value.getImpl() * m_values.m_global_scale.m_value;
	Vector4 curve = GetProfile().m_coefficients.getImpl();
	floatInVec at(t);
	floatInVec y = ((curve.getX() * at + curve.getY()) * at + curve.getZ()) * at + curve.getW();

	return math::v2(mulPerElem(Vector2(t * 2.0f + -1.0f, (float)y * 2.0f),
				   Vector2(scale.getY(), scale.getZ())));
}

void WaveInstance::GetProfilePoint(float t, math::v2 &point, math::v2 &normal)
{
	point = GetProfilePoint(t);

	Vector2 slope = (GetProfilePoint(t + 0.01f) - point).getImpl();
	Vector2 perpendicular(-slope.getY(), slope.getX());

	normal = math::v2(perpendicular / length(perpendicular));
}

static common::SharedPtr<arl::VertexArraySpec> GetWaveVertexSpec()
{
	if (s_wave_vertex_spec.get() == NULL)
		s_wave_vertex_spec = ScePafGraphics_4D1E289A(arl::Gl(),
							     arl::VertexArraySpec::ATTRIB0_F4_ATTRIB3_F4,
							     SCE_GXM_INDEX_SOURCE_INDEX_16BIT);
	return s_wave_vertex_spec;
}

WaveInstance::WaveInstance(int32_t index, int32_t size, int32_t resolution)
	: m_surface("WaveInstance"), m_index(index), m_values(NULL), m_last_uv_rotate(0.0f),
	  m_uv_rotate_damp(1.0f), m_flatness(0.0f), m_look_down(0.0f)
{
	arl::ArlInstance *gl = arl::Gl();

	m_surface.Init(arl::SurfaceSize(size, size));
	m_mesh = WaveMesh<WaveVertex>(arl::SurfaceSize(resolution, resolution), GetWaveVertexSpec(),
				      gl->m_default_allocator);
	m_mesh.Fill(FlatWaveVertex());

	m_textures[0] = common::SharedPtr<arl::Texture2D>(new arl::Texture2D(gl->m_default_allocator));
	ScePafGraphics_C4BD3558(m_textures[0].get(), m_mesh.m_width, m_mesh.m_height,
				ImageMode_WZYX16FP, 0,
				arl::SurfaceFlags()
					.Set(arl::COLOR_SURFACE)
					.Set(arl::COLOR_SURFACE_TILED)
					.Set(arl::COLOR_SURFACE_SCALE_NONE)
					.Set(arl::COLOR_SURFACE_OUTPUT_REGISTER_SIZE_64BIT));

	m_textures[1] = common::SharedPtr<arl::Texture2D>(new arl::Texture2D(gl->m_default_allocator));
	ScePafGraphics_C4BD3558(m_textures[1].get(), m_mesh.m_width, m_mesh.m_height,
				ImageMode_RGBA8888, 0,
				arl::SurfaceFlags()
					.Set(arl::COLOR_SURFACE)
					.Set(arl::COLOR_SURFACE_TILED)
					.Set(arl::COLOR_SURFACE_SCALE_NONE)
					.Set(arl::COLOR_SURFACE_OUTPUT_REGISTER_SIZE_32BIT));

	m_textures[2] = common::SharedPtr<arl::Texture2D>(new arl::Texture2D(gl->m_default_allocator));
	ScePafGraphics_C4BD3558(m_textures[2].get(), m_mesh.m_width, m_mesh.m_height,
				ImageMode_RGBA8888, 0,
				arl::SurfaceFlags()
					.Set(arl::COLOR_SURFACE)
					.Set(arl::COLOR_SURFACE_TILED)
					.Set(arl::COLOR_SURFACE_SCALE_NONE));

	m_textures[3] = common::SharedPtr<arl::Texture2D>(new arl::Texture2D(gl->m_default_allocator));
	m_textures[3]->init(common::SharedPtr<arl::TextureObject>(m_textures[2]));
}

WaveInstance::~WaveInstance()
{
	s_wave_vertex_spec.reset();
}

void WaveInstance::Set(const FFTWaveParam &param, const WaveInstanceParamSet::Values &values)
{
	FFTWaveParam old = m_surface.m_param;

	m_surface.m_param = param;
	m_values = values;
	if (old.m_a != m_surface.m_param.m_a || old.m_log_a != m_surface.m_param.m_log_a ||
	    old.m_gravity != m_surface.m_param.m_gravity ||
	    old.m_patch_size != m_surface.m_param.m_patch_size ||
	    old.m_wind_speed != m_surface.m_param.m_wind_speed ||
	    old.m_wind_dir != m_surface.m_param.m_wind_dir)
		m_surface.Reset();
}

WaveVertex WaveInstance::MakeVertex(const math::v2 &uv)
{
	Vector3 scale = m_values.m_scale.m_value.getImpl() * m_values.m_global_scale.m_value;
	math::v2 point;
	math::v2 normal;
	WaveVertex vertex;

	GetProfilePoint(uv.extract_y(), point, normal);
	vertex.m_position = math::v4((uv.extract_x() * 2.0f + -1.0f) * scale.getX(),
				     point.extract_x(), point.extract_y(), 1.0f);
	vertex.m_normal = math::v4(0.0f, normal.extract_x(), normal.extract_y(), 0.0f);
	return vertex;
}

void WaveRenderer::MakeLitTexture()
{
	if (m_lit_made)
		return;

	arl::ArlInstance *gl = arl::Gl();
	arl::Rect &viewport = gl->m_viewport.Top().m_viewport;
	Vector2 size(sce_vectormath_floorf2((Vector2((float)(viewport.m_x1 - viewport.m_x0),
						     (float)(viewport.m_y1 - viewport.m_y0)) *
					     m_lit_scale)
						    .get64()));
	int32_t width = (int32_t)(float)size.getX() / arl::s_tile_size.m_width *
			arl::s_tile_size.m_width;
	int32_t height = (int32_t)(float)size.getY() / arl::s_tile_size.m_height *
			 arl::s_tile_size.m_height;

	m_lit_texture = common::SharedPtr<arl::Texture2D>(new arl::Texture2D(gl->m_default_allocator));
	ScePafGraphics_C4BD3558(m_lit_texture.get(), width, height, ScePafGraphics_96173730(gl), 0,
				ScePafGraphics_BB119805(gl));
	m_lit_made = true;
}

FogParamSet::Values WaveRenderer::GetFog()
{
	FogParamSet fog(true);

	fog.SetKey(PARAM_KEY_LERP_FROM, m_fog.m_set.m_values);
	fog.SetKey(PARAM_KEY_LERP_TO, m_fog_lookdown.m_set.m_values);
	fog.Lerp(m_wave_x);
	return fog.m_values;
}

void WaveRenderer::draw_pos(WaveInstance *instance)
{
	arl::ArlInstance *gl = arl::Gl();

	m_pos_fbo.Bind(common::SharedPtr<arl::TextureObject>(instance->m_textures[0]), 0, 0);
	ScePafGraphics_199BD5F2(gl, 0);
	gl->PushMarker("WaveRenderer::draw_pos");
	gl->m_model.PushTop();
	gl->m_model.LoadIdentity();
	gl->m_model.MultMatrix(instance->GetModelMatrix());

	math::v2 size;
	{
		common::SharedPtr<arl::Texture2D> height_map = instance->m_surface.m_texture;

		size = math::v2((float)height_map->m_desc.m_width, (float)height_map->m_desc.m_height);
	}
	{
		common::SharedPtr<arl::Texture2D> height_map = instance->m_surface.m_texture;

		m_pos_frag.m_interface.m_height_map.Set(common::SharedPtr<arl::TextureObject>(height_map));
	}
	math::v2 texel = math::v2(recipPerElem(size.getImpl()));

	m_pos_frag.m_interface.m_distortion_scale.Set(instance->m_values.m_distortion_scale.m_value *
						      instance->m_values.m_global_scale.m_value);
	m_pos_frag.m_interface.m_distortion_uv_scale_rotate.Set(
		math::v2(instance->m_values.m_uv_scale.m_value,
			 instance->m_values.m_uv_rotate.m_value * 0.017453292f));
	m_pos_frag.m_interface.m_cross_section.Set(instance->GetProfile().m_coefficients);
	m_pos_frag.m_interface.m_cross_section_scale.Set(
		math::v3(instance->m_values.m_scale.m_value.getImpl() *
			 instance->m_values.m_global_scale.m_value));

	math::matrix world = m_camera->GetResetWorld();
	Vector4 position = world.w().getImpl() / floatInVec(m_camera->GetUnit());

	position.setW(1.0f);
	world.set_w(math::v4(position));

	math::matrix view_model = world.inverse1() * instance->GetModelMatrix();
	math::matrix model_view = instance->GetModelMatrix().inverse1();

	m_pos_frag.m_interface.m_vm.Set(view_model);

	WaveVertex front = instance->MakeVertex(math::v2(0.5f, 0.0f));
	Vector4 front_view =
		view_model.getImpl() * Vector4(front.m_position.getImpl().getXYZ(), 1.0f);
	WaveVertex back = instance->MakeVertex(math::v2(0.5f, 1.0f));

	m_pos_frag.m_interface.m_persp_hack.Set(
		math::v4(m_wave_speed, 0.0f, -1.0f, (float)front_view.getZ()));

	QuadArea area;

	area.m_min = math::v2(0.0f, 0.0f);
	area.m_max = math::v2(1.0f, 1.0f);
	m_pos_vert.m_interface.m_mvp.Set(GetQuadMatrix(area));
	m_pos_vert.m_object.begin();
	m_pos_frag.m_object.begin();

	m_texture_ops->DrawQuad();
	m_pos_frag.m_object.end();
	m_pos_vert.m_object.end();
	gl->m_model.Pop();
	gl->PopMarker();
	ScePafGraphics_29F5AA8B(gl, NULL, true);
	arl::FrameBufferObject::Unbind();
}

inline __attribute__((noinline)) math::matrix PointLightSphereParamSet::Values::GetPack1() const
{
	return MakePack1();
}

inline __attribute__((noinline)) math::matrix FogParamSet::Values::GetPack() const
{
	return MakePack();
}

void WaveRenderer::DrawPositions()
{
	arl::ArlInstance *gl = arl::Gl();

	gl->Disable(arl::DEPTH_TEST);
	gl->DepthFunc(arl::DEPTH_LEQUAL, arl::FRONT_AND_BACK);
	gl->Disable(arl::BLEND);
	gl->DepthMask(false, arl::FRONT_AND_BACK);
	gl->CullMode(arl::CULL_NONE);

	for (ParamLink *link = m_instances.m_next; link != &m_instances; link = link->m_next)
		draw_pos(static_cast<WaveInstance *>(link));

	for (ParamLink *link = m_instances.m_next; link != &m_instances; link = link->m_next) {
		WaveInstance *instance = static_cast<WaveInstance *>(link);

		instance->m_textures[0]->setMinFilter(SCE_GXM_TEXTURE_FILTER_POINT, false);
		instance->m_textures[0]->setMagFilter(SCE_GXM_TEXTURE_FILTER_POINT);
		instance->m_textures[0]->setWrapS(arl::TEXTURE_WRAP_CLAMP_FULL_BORDER);
		instance->m_textures[0]->setWrapT(arl::TEXTURE_WRAP_CLAMP_FULL_BORDER);
		instance->m_textures[1]->setMinFilter(SCE_GXM_TEXTURE_FILTER_POINT, false);
		instance->m_textures[1]->setMagFilter(SCE_GXM_TEXTURE_FILTER_POINT);
		instance->m_textures[1]->setWrapS(arl::TEXTURE_WRAP_REPEAT);
		instance->m_textures[1]->setWrapT(arl::TEXTURE_WRAP_REPEAT);
		m_texture_ops->gridNormal8(instance->m_textures[1], instance->m_textures[0], 0, 0);
	}

	for (ParamLink *link = m_instances.m_next; link != &m_instances; link = link->m_next) {
		WaveInstance *instance = static_cast<WaveInstance *>(link);

		instance->m_textures[0]->setMinFilter(SCE_GXM_TEXTURE_FILTER_LINEAR, false);
		instance->m_textures[0]->setMagFilter(SCE_GXM_TEXTURE_FILTER_LINEAR);
		instance->m_textures[1]->setMinFilter(SCE_GXM_TEXTURE_FILTER_LINEAR, false);
		instance->m_textures[1]->setMagFilter(SCE_GXM_TEXTURE_FILTER_LINEAR);
	}
}

WaveVertex FlatWaveVertex::operator()(const math::v2 &uv) const
{
	WaveVertex vertex;

	vertex.m_position = math::v4(uv.extract_x(), uv.extract_y(), 0.0f, 1.0f);
	vertex.m_normal = vertex.m_position;
	return vertex;
}

void WaveRenderer::render_lighting_texture()
{
	arl::ArlInstance *gl = arl::Gl();

	gl->Disable(arl::DEPTH_TEST);
	gl->DepthFunc(arl::DEPTH_LEQUAL, arl::FRONT_AND_BACK);
	gl->Disable(arl::BLEND);
	gl->DepthMask(false, arl::FRONT_AND_BACK);
	gl->CullMode(arl::CULL_NONE);

	for (ParamLink *link = m_instances.m_next; link != &m_instances; link = link->m_next) {
		WaveInstance *instance = static_cast<WaveInstance *>(link);

		m_lit_fbo.Bind(common::SharedPtr<arl::TextureObject>(instance->m_textures[2]), 0, 0);
		ScePafGraphics_199BD5F2(gl, 1);
		gl->PushMarker("WaveRenderer::render_lighting_texture");
		gl->m_model.PushTop();
		gl->m_model.LoadIdentity();
		gl->m_model.MultMatrix(instance->GetModelMatrix());
		m_lit_frag.m_interface.m_model_matrix.Set(gl->m_model.Top().m_matrix);
		m_lit_frag.m_interface.m_pos_tex.Set(arl::TextureSource(
			common::SharedPtr<arl::TextureObject>(instance->m_textures[0])));
		m_lit_frag.m_interface.m_nor_tex.Set(arl::TextureSource(
			common::SharedPtr<arl::TextureObject>(instance->m_textures[1])));

		Vector2 size((float)instance->m_textures[1]->m_desc.m_width,
			     (float)instance->m_textures[1]->m_desc.m_height);

		m_lit_frag.m_interface.m_half_pixel.Set(
			math::v2(mulPerElem(recipPerElem(size), Vector2(0.5f))));

		Vector4 eye = gl->m_view.GetInverse().w().getImpl() / floatInVec(m_camera->GetUnit());

		m_lit_frag.m_interface.m_eye_pos.Set(math::v3(eye.getXYZ()));
		m_lit_frag.m_interface.m_material_pack.Set(m_material.m_set.m_values.GetPack());
		m_lit_frag.m_interface.m_point_light_pack2.Set(m_point_light.m_set.m_values.GetPack2());
		m_lit_frag.m_interface.m_point_light_pack1.Set(m_point_light.m_set.m_values.GetPack1());
		m_lit_frag.m_interface.m_fog_pack.Set(GetFog().GetPack());
		m_lit_frag.m_interface.m_edge_color.Set(
			math::v4(instance->m_values.m_edge_color.m_value,
				 instance->m_values.m_edge_power.m_value));
		m_lit_frag.m_interface.m_center_color.Set(
			math::v4(instance->m_values.m_center_color.m_value,
				 instance->m_values.m_center_power.m_value));
		m_lit_frag.m_interface.m_edge_fade.Set(m_params.m_set.m_values.m_edge_fog.m_value);

		QuadArea area;
		QuadArea uv_area;

		area.m_min = math::v2(0.0f, 0.0f);
		area.m_max = math::v2(1.0f, 1.0f);
		uv_area.m_min = math::v2(0.0f, 0.0f);
		uv_area.m_max = math::v2(1.0f, 1.0f);
		m_texture_ops->m_vert.m_interface.m_mvp.Set(GetQuadMatrix(area));
		m_texture_ops->m_vert.m_interface.m_uv_transform.Set(
			math::v4(uv_area.m_min.extract_x(), uv_area.m_min.extract_y(),
				 (uv_area.m_max - uv_area.m_min).extract_x(),
				 (uv_area.m_max - uv_area.m_min).extract_y()));
		m_texture_ops->m_vert.m_object.begin();
		m_lit_frag.m_object.begin();
		m_texture_ops->DrawQuad();
		m_lit_frag.m_object.end();
		m_texture_ops->m_vert.m_object.end();
		gl->m_model.Pop();
		gl->PopMarker();
		ScePafGraphics_29F5AA8B(gl, NULL, true);
		arl::FrameBufferObject::Unbind();

		if (m_lit_feedback > 0.001f) {
			m_texture_ops->fliptri(instance->m_textures[3], instance->m_textures[2], 0,
					       m_lit_feedback);

			common::SharedPtr<arl::Texture2D> flipped = instance->m_textures[3];

			instance->m_textures[3] = instance->m_textures[2];
			instance->m_textures[2] = flipped;
		}
	}
}


void WaveRenderer::draw_color(WaveInstance *instance)
{
	arl::ArlInstance *gl = arl::Gl();

	gl->Disable(arl::BLEND);
	gl->m_model.PushTop();
	gl->m_model.LoadIdentity();
	gl->m_model.MultMatrix(instance->GetModelMatrix());
	gl->m_projection_changed = true;
	gl->m_view_changed = true;
	m_wave_vert.m_interface.m_vp.Set(gl->m_projection.Top().m_matrix *
					 gl->m_view.Top().m_matrix);
	m_wave_vert.m_interface.m_model_matrix.Set(gl->m_model.Top().m_matrix);
	{
		common::SharedPtr<arl::Texture2D> positions = instance->m_textures[0];

		m_wave_vert.m_interface.m_pos_tex.Set(common::SharedPtr<arl::TextureObject>(positions));
	}

	Vector2 size((float)instance->m_textures[1]->m_desc.m_width,
		     (float)instance->m_textures[1]->m_desc.m_height);

	m_wave_vert.m_interface.m_half_pixel.Set(
		math::v2(mulPerElem(recipPerElem(size), Vector2(0.5f))));
	m_wave_vert.m_interface.m_world_scale_hack.Set(m_camera->GetUnit());

	Vector4 eye = gl->m_view.GetInverse().w().getImpl() / floatInVec(m_camera->GetUnit());
	math::matrix light = m_point_light.m_set.m_values.GetPack1();

	/* the fog is worked out and not used: Sony's build destroys it at once */
	GetFog();

	instance->m_textures[2]->setMinFilter(SCE_GXM_TEXTURE_FILTER_LINEAR, false);
	instance->m_textures[2]->setMagFilter(SCE_GXM_TEXTURE_FILTER_LINEAR);
	instance->m_textures[3]->setMinFilter(SCE_GXM_TEXTURE_FILTER_LINEAR, false);
	instance->m_textures[3]->setMagFilter(SCE_GXM_TEXTURE_FILTER_LINEAR);
	{
		common::SharedPtr<arl::Texture2D> lit = instance->m_textures[2];

		m_wave_frag.m_interface.m_lit_tex.Set(common::SharedPtr<arl::TextureObject>(lit));
	}
	if (m_wireframe)
		gl->PolygonMode(arl::POLYGON_MODE_LINE, arl::FRONT_AND_BACK);
	m_wave_vert.m_object.begin();
	m_wave_frag.m_object.begin();
	gl->CullMode(arl::CULL_CW);
	if (m_wireframe)
		instance->m_mesh.DrawLines(gl);
	else if (instance->m_mesh.m_index_count != 0)
		instance->m_mesh.Draw(gl, instance->m_mesh.m_height - 1);
	m_wave_frag.m_object.end();
	m_wave_vert.m_object.end();
	if (m_wireframe)
		gl->PolygonMode(arl::POLYGON_MODE_FILL, arl::FRONT_AND_BACK);
	gl->m_model.Pop();
	gl->CullMode(arl::CULL_CW);
}

void WaveRenderer::render_color()
{
	arl::ArlInstance *gl = arl::Gl();

	gl->Enable(arl::DEPTH_TEST);
	gl->DepthFunc(arl::DEPTH_LEQUAL, arl::FRONT_AND_BACK);
	gl->Disable(arl::BLEND);
	gl->DepthMask(true, arl::FRONT_AND_BACK);
	gl->CullMode(arl::CULL_CW);
	gl->PushMarker("WaveRenderer::render_color");
	gl->Enable(arl::DEPTH_TEST);
	gl->DepthFunc(arl::DEPTH_LEQUAL, arl::FRONT_AND_BACK);

	for (ParamLink *link = m_instances.m_next; link != &m_instances; link = link->m_next)
		draw_color(static_cast<WaveInstance *>(link));

	gl->PopMarker();
}

/* the z axis turned by the angles' y about the y axis, then by their x about the x axis
   (0x81204220; name ours) */
inline __attribute__((noinline)) math::v4 GetTiltDirection(const math::v2 &angles)
{
	Vector4 dir(0.0f, 0.0f, 1.0f, 0.0f);
	float yaw = angles.extract_y();
	Vector2 cs(sce_paf_cosf(yaw), sce_paf_sinf(yaw));
	floatInVec x = dot(Vector2(dir.getX(), dir.getZ()), cs);
	floatInVec z = dir.getZ() * cs.getX() - dir.getX() * cs.getY();

	dir.setX(x);
	dir.setZ(z);

	float pitch = angles.extract_x();

	cs = Vector2(sce_paf_cosf(pitch), sce_paf_sinf(pitch));

	floatInVec y = dir.getY() * cs.getX() - dir.getZ() * cs.getY();

	z = dot(Vector2(dir.getY(), dir.getZ()), Vector2(cs.getY(), cs.getX()));
	dir.setY(y);
	dir.setZ(z);
	return math::v4(dir);
}

void WaveRenderer::ResetRotation()
{
	float pitch = -m_params.m_set.m_values.m_pitch_rest.m_value;
	MotionListener *listener = m_listener.get();
	math::v4 front = GetTiltDirection(math::v2(pitch, 0.0f));
	math::v4 down = GetTiltDirection(math::v2(pitch - 1.5707964f, 0.0f));
	math::v4 x = down.cross(front).unit();
	math::v4 y = front.cross(x);

	listener->m_rotation =
		math::matrix(x, y, front, math::v4(0.0f, 0.0f, 0.0f, 1.0f)).inverse1();
}

/* the angle from from to to, negative the other way round (0x8120431c; name ours) */
inline __attribute__((noinline)) float GetSignedAngle(const math::v2 &from, const math::v2 &to)
{
	Vector2 a = from.getImpl() * sce::Vectormath::Simd::recipf(length(from.getImpl()));
	Vector2 b = to.getImpl() * sce::Vectormath::Simd::recipf(length(to.getImpl()));
	float angle = sce_paf_acosf(math::ClampUnit(dot(b, a).getAsFloat()));
	floatInVec cross = floatInVec(to.extract_y() * from.extract_x() -
				      to.extract_x() * from.extract_y());

	return angle * sce::Vectormath::Simd::select(floatInVec(-1.0f), floatInVec(1.0f),
						     cross > floatInVec(0.0f))
			       .getAsFloat();
}

void WaveRenderer::UpdateInstances()
{
	m_param_keys.Update(1.0f / 60.0f);
	m_transition_keys.Update(1.0f / 60.0f);

	for (ParamLink *link = m_instances.m_next; link != &m_instances; link = link->m_next) {
		WaveInstance *instance = static_cast<WaveInstance *>(link);

		if (instance->m_index == 0)
			instance->Set(m_fft_wave.m_set.m_values.GetParam(),
				      m_wave_instance.m_set.m_values);

		float rotate = instance->m_values.m_uv_rotate.m_value * 0.017453292f;
		float c = sce_paf_cosf(rotate);
		float s = sce_paf_sinf(rotate);
		math::v2 dir(c, s);

		rotate = instance->m_last_uv_rotate * 0.017453292f;
		c = sce_paf_cosf(rotate);
		s = sce_paf_sinf(rotate);

		math::v2 last(c, s);
		float turn = -(__builtin_fabsf(GetSignedAngle(last, dir)) *
			       m_transition.m_set.m_values.m_uv_rotate_distort_curve.m_value);
		float damp = 0.0f;

		if (turn >= -100.0f)
			damp = sce_paf_expf(turn);

		float chase = instance->m_uv_rotate_damp;

		instance->m_last_uv_rotate = instance->m_values.m_uv_rotate.m_value;
		instance->m_uv_rotate_damp =
			chase + (damp - chase) *
				m_transition.m_set.m_values.m_uv_rotate_distort_control_chase.m_value;
		instance->m_values.m_distortion_scale.SetValue(
			instance->m_values.m_distortion_scale.m_value *
			((instance->m_uv_rotate_damp - 1.0f) *
				 m_transition.m_set.m_values.m_uv_rotate_distort_control.m_value +
			 1.0f));
	}
}

WaveRenderer::WaveRenderer()
	: m_wave_scale(1.0f), m_wave_speed(0.19f), m_wireframe(false), m_paused(false), m_unread_flag1(false),
	  m_update_flag(false), m_unread_flag2(false), m_unread_flag3(true), m_unread_value1(0), m_clear(true),
	  m_frame(-1), m_lit_scale(1.0f), m_lit_made(false), m_param_keys("WaveRenderer"),
	  m_material(m_param_keys, ""), m_point_light(m_param_keys, ""),
	  m_fog(m_param_keys, "sunlit_smoothstep"),
	  m_fog_lookdown(m_param_keys, "sunlit_smoothstep_lookdown"), m_sky(m_param_keys, ""),
	  m_fft_wave(m_param_keys, ""), m_wave_instance(m_param_keys, ""),
	  m_params(m_param_keys, ""), m_transition_keys("WaveTransition"),
	  m_transition(m_transition_keys, ""), m_update_count(0), m_lit_feedback(0.5f), m_rs_wave(NULL),
	  m_rs_wave_debug(NULL), m_tilt(false), m_unread_value2(0), m_draw_sky(true), m_wave_x(0.0f),
	  m_wave_y(0.0f)
{
}

void WaveRenderer::Init(common::SharedPtr<Camera> camera, common::SharedPtr<Background> background,
			common::SharedPtr<DrawEnv> draw_env,
			common::SharedPtr<TextureOps> texture_ops,
			common::SharedPtr<MotionListener> listener)
{
	m_background = background;
	m_camera = camera;
	m_draw_env = draw_env;
	m_texture_ops = texture_ops;
	m_listener = listener;
}

void WaveRenderer::Update()
{
	int32_t frame = m_frame;

	m_update_count++;
	if (frame == common::FrameCount())
		return;

	m_frame = common::FrameCount();
	UpdateInstances();
	for (ParamLink *link = m_instances.m_next; link != &m_instances; link = link->m_next)
		static_cast<WaveInstance *>(link)->m_surface.Update(m_paused, m_update_flag);
}

void WaveRenderer::DrawDebug()
{
}

void WaveRenderer::Load(const string &dir, uint32_t count)
{
	arl::ArlInstance *gl = arl::Gl();
	arl::ProgramState *state = gl;

	m_wave_vert.m_object.init(*gl, state->m_patcher, GetWaveVertexSpec(),
				  common::SharedPtr<arl::VertexArraySpec>(),
				  common::SharedPtr<arl::VertexArraySpec>(),
				  common::SharedPtr<arl::VertexArraySpec>());
	state = gl;
	m_wave_frag.m_object.init(*gl, state->m_patcher);
	state = gl;
	m_pos_vert.m_object.init(*gl, state->m_patcher, m_texture_ops->m_quad.m_vertices.m_spec,
				 common::SharedPtr<arl::VertexArraySpec>(),
				 common::SharedPtr<arl::VertexArraySpec>(),
				 common::SharedPtr<arl::VertexArraySpec>());
	m_pos_frag.m_object.setOutputRegisterFormat(SCE_GXM_OUTPUT_REGISTER_FORMAT_HALF4);
	state = gl;
	m_pos_frag.m_object.init(*gl, state->m_patcher);
	state = gl;
	m_lit_frag.m_object.init(*gl, state->m_patcher);
	LoadPresets(dir);

	m_rs_wave = m_draw_env->AddLayer("m_rs_wave", 100, 2, 0x4000);
	m_rs_wave->Push(0x40000000, 0, &WaveRenderer::Draw, this);
	m_rs_wave->m_reserved_value = 2;

	m_rs_wave_debug = m_draw_env->AddLayer("m_rs_wave_debug", 900, 2, 0x4000);
	m_rs_wave_debug->Push(0x40000000, 0, &WaveRenderer::DrawDebug, this);
	m_rs_wave_debug->m_reserved_value = 2;

	m_gradient_power_min = 2.0f;
	m_gradient_power_max = 18.0f;
	m_gradient = arl::CreateTexture<ImageMode_L8>(arl::Gl()->m_default_allocator,
						      arl::SurfaceSize(128, 64), NULL,
						      arl::SurfaceFlags(), 0);
	{
		vector<uint8_t> levels;
		int32_t height = m_gradient->m_desc.m_height;
		int32_t width = m_gradient->m_desc.m_width;

		for (int32_t y = 0; y != height; y++) {
			for (int32_t x = 0; x != width; x++) {
				float level = sce_paf_powf(
					(float)x / (float)(width - 1),
					(float)y / (float)(height - 1) * 16.0f + 2.0f);

				if (level < 0.0f)
					level = 0.0f;
				levels.push_back((uint8_t)((level < 1.0f ? level : 1.0f) * 255.0f));
			}
		}

		arl::Texture2D *texture = m_gradient.get();

		ScePafGraphics_309D2902(texture, 0, texture->m_desc.m_width,
					texture->m_desc.m_height, texture->m_desc.m_format,
					texture->m_desc.m_levels, levels.ptr);
		m_gradient->setWrapS(arl::TEXTURE_WRAP_CLAMP_FULL_BORDER);
		m_gradient->setWrapT(arl::TEXTURE_WRAP_CLAMP_FULL_BORDER);
	}

	for (uint32_t i = 0; i < count; i++) {
		uint32_t index = 0;

		for (ParamLink *link = m_instances.m_next; link != &m_instances; link = link->m_next)
			index++;
		m_instances.PushBack(new WaveInstance(index, 32, 64));
	}

	Update();
	m_skydome.Init(arl::SurfaceSize(32, 64));
	m_skydome.SetCamera(m_camera);
	ResetRotation();
}

math::v4 WaveRenderer::GetFogColor()
{
	return math::v4(Vector4(GetFog().m_color.m_value.getImpl(), 1.0f));
}

void WaveRenderer::Draw()
{
	if (m_update_count == 0)
		return;

	arl::ArlInstance *gl = arl::Gl();

	m_camera->PushMatrices();

	bool scaled = m_lit_scale != 1.0f;

	DrawPositions();
	render_lighting_texture();
	if (scaled)
		m_wave_fbo.Bind(common::SharedPtr<arl::TextureObject>(GetLitTexture()), 0, 0);
	ScePafGraphics_199BD5F2(gl, 2);
	if (m_clear) {
		gl->PushMarker("WaveRenderer::draw - clear");
		gl->Disable(arl::DEPTH_TEST);
		gl->DepthMask(true, arl::FRONT_AND_BACK);
		gl->ClearColor(GetFogColor());
		gl->Clear(arl::DEPTH_BUFFER_BIT | arl::COLOR_BUFFER_BIT);
		gl->PopMarker();
	}
	render_color();

	Skydome::DrawParam param;

	param.m_world_scale_hack = m_camera->GetUnit();
	{
		common::SharedPtr<arl::Texture2D> normals =
			m_instances.m_next == &m_instances ?
				common::SharedPtr<arl::Texture2D>() :
				static_cast<WaveInstance *>(m_instances.m_next)->m_textures[1];

		param.m_wave_nor_tex = common::SharedPtr<arl::TextureObject>(normals);
	}
	param.m_wave_model_matrix = static_cast<WaveInstance *>(m_instances.m_next)->GetModelMatrix();
	param.m_wave_fog_color = GetFogColor();
	param.m_distortion_scale =
		m_instances.m_next == &m_instances ?
			1.0f :
			static_cast<WaveInstance *>(m_instances.m_next)->m_values.m_distortion_scale.m_value *
				static_cast<WaveInstance *>(m_instances.m_next)->m_values.m_global_scale.m_value;
	if (m_draw_sky)
		m_skydome.Draw(param, m_sky.m_set.m_values);
	ScePafGraphics_29F5AA8B(gl, NULL, true);
	m_camera->PopMatrices();
	if (scaled) {
		arl::FrameBufferObject::Unbind();
		gl->Disable(arl::BLEND);

		TextureOps::CopyArea area = {
			QuadArea(math::v2(0.0f, 0.0f), math::v2(1.0f, 1.0f)),
			QuadArea(math::v2(0.0f, 0.0f), math::v2(1.0f, 1.0f)),
		};

		m_texture_ops->pureCopy(common::SharedPtr<arl::Texture2D>(), GetLitTexture(), 0, area);
	}
}

static inline float Max(float a, float b)
{
	return a > b ? a : b;
}

static inline float Min(float a, float b)
{
	return a > b ? b : a;
}

static inline float Clamp(float x, float lo, float hi)
{
	return Min(hi, Max(lo, x));
}

/* 1 with the sign of x (name ours) */
static inline float Sign(float x)
{
	union {
		float f;
		uint32_t u;
	} bits;

	bits.f = x;
	bits.u = (bits.u & 0x80000000) | 0x3f800000;
	return bits.f;
}

/* where x is on the way from a to b, 0 to 1 (name ours) */
static inline float LinearStep(float a, float b, float x)
{
	return (Clamp(x, Min(b, a), Max(b, a)) - a) / (b - a);
}

/* x up to limit - zone, limit from limit + zone, and a Hermite curve between them (name
   ours) */
static inline float SoftLimit(float x, float limit, float zone)
{
	float start = limit - zone;

	if (x < start)
		return x;
	if (x > limit + zone)
		return limit;

	float t = (x - start) / (zone * 2.0f);
	sce::Vectormath::Simd::Aos::Matrix4 hermite(Vector4(2.0f, -2.0f, 1.0f, 1.0f),
						    Vector4(-3.0f, 3.0f, -2.0f, -1.0f),
						    Vector4(0.0f, 0.0f, 1.0f, 0.0f),
						    Vector4(1.0f, 0.0f, 0.0f, 0.0f));
	float t2 = t * t;
	float h = dot(Vector4(0.0f, 0.5f, 1.0f, 0.0f), hermite * Vector4(t * t2, t2, t, 1.0f));

	return start + h * 2.0f * zone;
}

math::matrix WaveRenderer::GetTiltedWorld(const math::matrix &reset_world)
{
	Vector4 dir = m_listener->m_rotation.z().getImpl();
	float pitch = -sce_paf_atan2f(dir.getY(), dir.getZ());
	Vector2 cs(sce_paf_cosf(-pitch), sce_paf_sinf(-pitch));
	floatInVec y = dir.getY() * cs.getX() - dir.getZ() * cs.getY();
	floatInVec z = dot(Vector2(dir.getZ(), dir.getY()), cs);

	dir.setY(y);
	dir.setZ(z);

	math::v2 angles(pitch, sce_paf_atan2f(dir.getX(), dir.getZ()));
	static math::v2 s_angles(0.0f, 0.0f);

	math::v2 smooth = s_angles + (angles - s_angles) * math::v2(0.08f, 0.1f);

	s_angles = smooth;

	WaveRendererParamSet::Values &params = m_params.m_set.m_values;
	float lo = params.m_pitch_max.m_value;
	float hi = Max(lo, params.m_pitch_min.m_value);
	float rest = Clamp(params.m_pitch_rest.m_value, lo, hi);
	float tilt = Clamp(smooth.extract_x(), lo, hi);

	if (m_param_keys.m_edit_variant != "")
		m_params.m_keys.GetKey(m_param_keys.m_edit_variant, false)
			->m_values.m_pitch_debug.SetValue(tilt);

	float yaw = smooth.extract_y();
	float roll = Sign(yaw) *
		     SoftLimit(__builtin_fabsf(yaw), params.m_roll_max.m_value,
			       params.m_roll_max_zone.m_value);
	math::v4 front = GetTiltDirection(math::v2(tilt - rest, roll));

	m_wave_x = rest + 0.017453292f < hi ? LinearStep(rest, hi, tilt) : 0.0f;

	float down = lo < rest ? LinearStep(rest, lo, tilt) : 0.0f;

	m_wave_y = down * down * down;
	if (m_instances.m_next != &m_instances) {
		static_cast<WaveInstance *>(m_instances.m_next)->m_flatness = m_wave_x;
		static_cast<WaveInstance *>(m_instances.m_next)->m_look_down = m_wave_y;
	}

	Vector4 offset(0.0f);

	if (m_instances.m_next != &m_instances)
		offset = static_cast<WaveInstance *>(m_instances.m_next)->GetModelMatrix().w().getImpl();

	Vector4 center = offset * m_camera->GetUnit();

	center.setY(0.0f);
	center = Vector4(0.0f) + (center - Vector4(0.0f)) * m_wave_x;

	float lift = reset_world.w().getImpl().getZ() * m_wave_x * params.m_zoom.m_value;
	math::v4 up = front.cross(math::v4(1.0f, 0.0f, 0.0f, 0.0f)).unit();
	math::matrix look = math::matrix::LookAt(math::v4(0.0f, 0.0f, 0.0f, 1.0f),
						 math::v4(Vector4(-front.getImpl().getXYZ(), 1.0f)),
						 up, 0.9999f);
	math::matrix move(sce::Vectormath::Simd::Aos::Matrix4::translation(center.getXYZ()));
	math::matrix world = move * look.inverse1() * move.inverse1() *
			     math::matrix(sce::Vectormath::Simd::Aos::Matrix4::translation(
				     Vector3(0.0f, 0.0f, lift))) *
			     reset_world;
	static Pose s_pose(world);
	Pose target(world);
	float chase = params.m_camera_chase.m_value;
	math::v4 position = s_pose.m_position + (target.m_position - s_pose.m_position) * chase;
	math::quaternion rotation =
		math::quaternion::Slerp(s_pose.m_rotation, target.m_rotation, chase, 0.0001f);

	s_pose.m_position = position;
	s_pose.m_rotation = Pose::Normalize(rotation);

	math::matrix result(s_pose.m_rotation);

	result.set_w(s_pose.m_position);
	return result;
}

int32_t WaveRenderer::SetPreset(const char *variant, const WavePreset &preset)
{
	if (preset.m_magic != WAVE_PRESET_MAGIC || preset.m_version != 1 || preset.m_reserved != 0)
		return SCE_PAF_ERROR_INVALID_ARGUMENT;

	bool fog_set = false;

	for (ParamLink *link = m_param_keys.m_list.m_next; link != &m_param_keys.m_list;
	     link = link->m_next) {
		ParamSetKeysBase &keys = static_cast<ParamSetKeysBase &>(*link);
		const char *name = keys.m_name.c_str();

		if (sce_paf_strcmp(name, "Material") == 0) {
			MaterialParamSet::Values &values =
				static_cast<ParamSetKeys<MaterialParamSet> &>(keys)
					.GetKey(string(variant), false)
					->m_values;

			values.m_color.SetValue(preset.m_material.m_color);
			values.m_diffuse_coeff.SetValue(preset.m_material.m_diffuse_coeff);
			values.m_specular_coeff.SetValue(preset.m_material.m_specular_coeff);
			values.m_specular_power.SetValue(preset.m_material.m_specular_power);
			values.m_fresnel_front.SetValue(preset.m_material.m_fresnel_front);
			values.m_fresnel_control.SetValue(preset.m_material.m_fresnel_control);
			values.m_fresnel_coeff.SetValue(preset.m_material.m_fresnel_coeff);
			values.m_refraction_coeff.SetValue(preset.m_material.m_refraction_coeff);
			values.m_refraction_amount.SetValue(preset.m_material.m_refraction_amount);
			values.m_second_reflection.SetValue(preset.m_material.m_second_reflection);
		} else if (sce_paf_strcmp(name, "PointLightSphere") == 0) {
			PointLightSphereParamSet::Values &values =
				static_cast<ParamSetKeys<PointLightSphereParamSet> &>(keys)
					.GetKey(string(variant), false)
					->m_values;

			values.m_theta_phi.SetValue(preset.m_point_light.m_theta_phi);
			values.m_distance.SetValue(preset.m_point_light.m_distance);
			values.m_attn.SetValue(preset.m_point_light.m_attn);
			values.m_ambient_color.SetValue(preset.m_point_light.m_ambient_color);
			values.m_diffuse_color.SetValue(preset.m_point_light.m_diffuse_color);
			values.m_fade.SetValue(preset.m_point_light.m_fade);
		} else if (sce_paf_strcmp(name, "Fog") == 0) {
			const WavePreset::Fog *fog = &preset.m_fog_lookdown;

			if (!fog_set) {
				fog = &preset.m_fog;
				fog_set = true;
			}

			FogParamSet::Values &values =
				static_cast<ParamSetKeys<FogParamSet> &>(keys)
					.GetKey(string(variant), false)
					->m_values;

			values.m_color.SetValue(fog->m_color);
			values.m_density.SetValue(fog->m_density);
			values.m_light_color.SetValue(fog->m_light_color);
			values.m_d_scale.SetValue(fog->m_d_scale);
			values.m_d_offset.SetValue(fog->m_d_offset);
		} else if (sce_paf_strcmp(name, "Sky") == 0) {
			SkyParamSet::Values &values =
				static_cast<ParamSetKeys<SkyParamSet> &>(keys)
					.GetKey(string(variant), false)
					->m_values;

			values.m_theta_phi.SetValue(preset.m_sky.m_theta_phi);
			values.m_distance.SetValue(preset.m_sky.m_distance);
			values.m_sun_color.SetValue(preset.m_sky.m_sun_color);
			values.m_sun_power.SetValue(preset.m_sky.m_sun_power);
			values.m_sun_control.SetValue(preset.m_sky.m_sun_control);
			values.m_zenith_color.SetValue(preset.m_sky.m_zenith_color);
			values.m_horizon_color.SetValue(preset.m_sky.m_horizon_color);
			values.m_wave_fog_alpha.SetValue(preset.m_sky.m_wave_fog_alpha);
			values.m_radius.SetValue(preset.m_sky.m_radius);
			values.m_xscale.SetValue(preset.m_sky.m_xscale);
			values.m_side_angle.SetValue(preset.m_sky.m_side_angle);
			values.m_horizon_angle.SetValue(preset.m_sky.m_horizon_angle);
			values.m_horizon_blend_range.SetValue(preset.m_sky.m_horizon_blend_range);
			values.m_horizon_curvature.SetValue(preset.m_sky.m_horizon_curvature);
			values.m_sky_blend_start.SetValue(preset.m_sky.m_sky_blend_start);
			values.m_sky_blend_range.SetValue(preset.m_sky.m_sky_blend_range);
			values.m_pos_distortion_scale.SetValue(preset.m_sky.m_pos_distortion_scale);
			values.m_grad_distortion_scale.SetValue(preset.m_sky.m_grad_distortion_scale);
			values.m_horizon_distortion_scale.SetValue(preset.m_sky.m_horizon_distortion_scale);
		} else if (sce_paf_strcmp(name, "FFTWave") == 0) {
			FFTWaveParamSet::Values &values =
				static_cast<ParamSetKeys<FFTWaveParamSet> &>(keys)
					.GetKey(string(variant), false)
					->m_values;

			values.m_gravity.SetValue(preset.m_fft_wave.m_gravity);
			values.m_a.SetValue(preset.m_fft_wave.m_a);
			values.m_log_a.SetValue(preset.m_fft_wave.m_log_a);
			values.m_patch_size.SetValue(preset.m_fft_wave.m_patch_size);
			values.m_wind_speed.SetValue(preset.m_fft_wave.m_wind_speed);
			values.m_wind_dir.SetValue(preset.m_fft_wave.m_wind_dir);
			values.m_time_step.SetValue(preset.m_fft_wave.m_time_step);
		} else if (sce_paf_strcmp(name, "WaveInstance") == 0) {
			WaveInstanceParamSet::Values &values =
				static_cast<ParamSetKeys<WaveInstanceParamSet> &>(keys)
					.GetKey(string(variant), false)
					->m_values;

			values.m_distortion_scale.SetValue(preset.m_wave_instance.m_distortion_scale);
			values.m_translation.SetValue(preset.m_wave_instance.m_translation);
			values.m_rotation.SetValue(preset.m_wave_instance.m_rotation);
			values.m_scale.SetValue(preset.m_wave_instance.m_scale);
			values.m_global_scale.SetValue(preset.m_wave_instance.m_global_scale);
			values.m_uv_scale.SetValue(preset.m_wave_instance.m_uv_scale);
			values.m_uv_rotate.SetValue(preset.m_wave_instance.m_uv_rotate);
			values.m_center_color.SetValue(preset.m_wave_instance.m_center_color);
			values.m_center_power.SetValue(preset.m_wave_instance.m_center_power);
			values.m_edge_color.SetValue(preset.m_wave_instance.m_edge_color);
			values.m_edge_power.SetValue(preset.m_wave_instance.m_edge_power);
			values.m_cross_section.SetValue(preset.m_wave_instance.m_cross_section);
			values.m_cross_section_flat.SetValue(preset.m_wave_instance.m_cross_section_flat);
			values.m_decay.SetValue(preset.m_wave_instance.m_decay);
			values.m_shadow_nor_blend.SetValue(preset.m_wave_instance.m_shadow_nor_blend);
		} else if (sce_paf_strcmp(name, "WaveRenderer") == 0) {
			WaveRendererParamSet::Values &values =
				static_cast<ParamSetKeys<WaveRendererParamSet> &>(keys)
					.GetKey(string(variant), false)
					->m_values;

			values.m_edge_fog.SetValue(preset.m_params.m_edge_fog);
			values.m_pitch_max.SetValue(preset.m_params.m_pitch_max);
			values.m_pitch_rest.SetValue(preset.m_params.m_pitch_rest);
			values.m_pitch_min.SetValue(preset.m_params.m_pitch_min);
			values.m_pitch_debug.SetValue(preset.m_params.m_pitch_debug);
			values.m_roll_max.SetValue(preset.m_params.m_roll_max);
			values.m_roll_max_zone.SetValue(preset.m_params.m_roll_max_zone);
			values.m_zoom.SetValue(preset.m_params.m_zoom);
			values.m_camera_chase.SetValue(preset.m_params.m_camera_chase);
		}
	}
	return 0;
}

void WaveRenderer::LoadPresets(const string &dir)
{
	m_param_keys.SetPaths(dir, "wave/psets");
	m_transition_keys.SetPaths(dir, "wave/psets");

	FFTWaveParamSet &fft_wave = m_fft_wave.m_keys.m_default;

	fft_wave.findParamAndSet("gravity", 9.710714f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -20.0f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.005403f);
	m_fft_wave.m_keys.GetKey("effects_0406_ruby", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 9.710714f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -20.0f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.005815f);
	m_fft_wave.m_keys.GetKey("effects_0407_purple_01", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 11.846428f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -20.0f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.033174f);
	m_fft_wave.m_keys.GetKey("effects_0408_01_orange", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 12.113389f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -18.536848f);
	fft_wave.findParamAndSet("patch_size", 93.75f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.003373f);
	m_fft_wave.m_keys.GetKey("effects_0411_purple_02", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 9.710714f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -20.0f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.005403f);
	m_fft_wave.m_keys.GetKey("effects_0415_A5", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 8.108922f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -20.0f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.005815f);
	m_fft_wave.m_keys.GetKey("effects_0518_dalia", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 9.710714f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -20.0f);
	fft_wave.findParamAndSet("patch_size", 35.714287f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.024753f);
	m_fft_wave.m_keys.GetKey("effects_0518_gray_FLAT", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 9.710714f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -20.0f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.005403f);
	m_fft_wave.m_keys.GetKey("effects_0518_ruby", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 15.583928f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -19.047237f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 14.75f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.008799f);
	m_fft_wave.m_keys.GetKey("effects_A1", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 9.710714f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -20.0f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.005815f);
	m_fft_wave.m_keys.GetKey("effects_black", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 15.583928f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -19.047237f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 14.75f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.003663f);
	m_fft_wave.m_keys.GetKey("effects_blueTB_0513_01", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 15.583928f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -19.047237f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 14.75f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.003663f);
	m_fft_wave.m_keys.GetKey("effects_blue_FLAT", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 9.710714f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -20.0f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.005403f);
	m_fft_wave.m_keys.GetKey("effects_bluewhite_05", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 9.710714f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -20.0f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.005403f);
	m_fft_wave.m_keys.GetKey("effects_casis", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 9.710714f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -20.0f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.003663f);
	m_fft_wave.m_keys.GetKey("effects_default_0512_01", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 15.583928f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -19.047237f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 14.75f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.003663f);
	m_fft_wave.m_keys.GetKey("effects_green_0512_01", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 15.583928f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -19.047237f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 14.75f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.003663f);
	m_fft_wave.m_keys.GetKey("effects_koiao_0512_01", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 13.715168f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -19.79719f);
	fft_wave.findParamAndSet("patch_size", 107.14286f);
	fft_wave.findParamAndSet("wind_speed", 15.375f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.002381f);
	m_fft_wave.m_keys.GetKey("effects_limegreen_0513_02", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 12.113389f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -18.536848f);
	fft_wave.findParamAndSet("patch_size", 93.75f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.003373f);
	m_fft_wave.m_keys.GetKey("effects_maintosh", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 9.710714f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -20.0f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.003101f);
	m_fft_wave.m_keys.GetKey("effects_marrygold", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 9.710714f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -20.0f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.010011f);
	m_fft_wave.m_keys.GetKey("effects_peach_0512_001", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 9.710714f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -20.0f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.003101f);
	m_fft_wave.m_keys.GetKey("effects_purple_01", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 15.583928f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -19.047237f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 14.75f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.003663f);
	m_fft_wave.m_keys.GetKey("effects_purplered", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 9.710714f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -20.0f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.003101f);
	m_fft_wave.m_keys.GetKey("effects_redTB_0513_01", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 9.710714f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -20.0f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.005403f);
	m_fft_wave.m_keys.GetKey("effects_turquoise_0513_01", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 8.108922f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -20.0f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.005815f);
	m_fft_wave.m_keys.GetKey("effects_yellow_0512_01", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 9.710714f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -20.0f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.005403f);
	m_fft_wave.m_keys.GetKey("effects_color_vari_2g_01", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 9.710714f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -20.0f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.005403f);
	m_fft_wave.m_keys.GetKey("effects_color_vari_2g_02", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 9.710714f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -20.0f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.005403f);
	m_fft_wave.m_keys.GetKey("effects_color_vari_2g_03", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 9.710714f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -20.0f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.003663f);
	m_fft_wave.m_keys.GetKey("effects_color_vari_2g_04", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 9.710714f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -20.0f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.003101f);
	m_fft_wave.m_keys.GetKey("iboot", true)->m_values = fft_wave.m_values;

	fft_wave.findParamAndSet("gravity", 9.710714f);
	fft_wave.findParamAndSet("A", 0.0f);
	fft_wave.findParamAndSet("logA", -20.0f);
	fft_wave.findParamAndSet("patch_size", 98.21429f);
	fft_wave.findParamAndSet("wind_speed", 10.25f);
	fft_wave.findParamAndSet("wind_dir", -3.141592f);
	fft_wave.findParamAndSet("time_step", 0.003663f);
	m_fft_wave.m_keys.GetKey("effects_original_01", true)->m_values = fft_wave.m_values;

	FogParamSet &fog = m_fog.m_keys.m_default;

	fog.findParamAndSet("color", 0.9375f, 0.267857f, 0.133928f);
	fog.findParamAndSet("density", 1.0f, 0.247532f, 0.050144f, 10.0f);
	fog.findParamAndSet("light_color", 1.0f, 0.169642f, 0.107141f, 1.0f);
	fog.findParamAndSet("d_scale", 1.086354f);
	fog.findParamAndSet("d_offset", 20.0f);
	m_fog.m_keys.GetKey("effects_0406_ruby", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.767857f, 0.0f, 0.232142f);
	fog.findParamAndSet("density", 0.223403f, 0.212022f, 0.180475f, 10.0f);
	fog.findParamAndSet("light_color", 0.642857f, 0.169642f, 0.125f, 1.0f);
	fog.findParamAndSet("d_scale", 1.686795f);
	fog.findParamAndSet("d_offset", 2.132445f);
	m_fog.m_keys.GetKey("effects_0407_purple_01", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.0f, 0.589283f, 0.758928f);
	fog.findParamAndSet("density", 0.19057f, 0.316406f, 0.539773f, 10.0f);
	fog.findParamAndSet("light_color", 0.0f, 1.0f, 1.0f, 1.0f);
	fog.findParamAndSet("d_scale", 1.686795f);
	fog.findParamAndSet("d_offset", 3.051757f);
	m_fog.m_keys.GetKey("effects_0408_01_orange", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.330357f, 0.080356f, 0.178571f);
	fog.findParamAndSet("density", 0.128063f, 0.033735f, 0.113443f, 10.0f);
	fog.findParamAndSet("light_color", 0.357142f, 0.0625f, 0.151785f, 1.0f);
	fog.findParamAndSet("d_scale", 1.686795f);
	fog.findParamAndSet("d_offset", 3.051757f);
	m_fog.m_keys.GetKey("effects_0411_purple_02", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.0f, 0.455357f, 0.839285f);
	fog.findParamAndSet("density", 0.120587f, 0.135881f, 0.260307f, 10.0f);
	fog.findParamAndSet("light_color", 0.116071f, 0.0f, 0.883928f, 1.0f);
	fog.findParamAndSet("d_scale", 2.507272f);
	fog.findParamAndSet("d_offset", 2.411756f);
	m_fog.m_keys.GetKey("effects_0415_A5", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.723214f, 0.785714f, 0.857142f);
	fog.findParamAndSet("density", 0.661322f, 0.687905f, 1.0f, 10.0f);
	fog.findParamAndSet("light_color", 0.526783f, 0.526783f, 0.526783f, 1.0f);
	fog.findParamAndSet("d_scale", 2.907692f);
	fog.findParamAndSet("d_offset", 4.704748f);
	m_fog.m_keys.GetKey("effects_0518_dalia", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.0f, 0.0f, 0.0f);
	fog.findParamAndSet("density", 0.082361f, 0.082361f, 0.082361f, 10.0f);
	fog.findParamAndSet("light_color", 0.0f, 0.0f, 0.0f, 0.392857f);
	fog.findParamAndSet("d_scale", 2.907692f);
	fog.findParamAndSet("d_offset", 3.051757f);
	m_fog.m_keys.GetKey("effects_0518_gray_FLAT", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.9375f, 0.375f, 0.526783f);
	fog.findParamAndSet("density", 0.082361f, 0.082361f, 0.082361f, 10.0f);
	fog.findParamAndSet("light_color", 0.9375f, 0.839285f, 0.839285f, 1.0f);
	fog.findParamAndSet("d_scale", 2.507272f);
	fog.findParamAndSet("d_offset", 3.051757f);
	m_fog.m_keys.GetKey("effects_0518_ruby", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.5f, 0.491071f, 0.017857f);
	fog.findParamAndSet("density", 0.635518f, 0.381115f, 0.042994f, 10.0f);
	fog.findParamAndSet("light_color", 0.428571f, 0.357142f, 0.0f, 0.5625f);
	fog.findParamAndSet("d_scale", 50.0f);
	fog.findParamAndSet("d_offset", 0.325384f);
	m_fog.m_keys.GetKey("effects_A1", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.0f, 0.0f, 0.0f);
	fog.findParamAndSet("density", 0.201081f, 0.212022f, 0.223403f, 10.0f);
	fog.findParamAndSet("light_color", 0.0f, 0.0f, 0.0f, 0.892857f);
	fog.findParamAndSet("d_scale", 1.985993f);
	fog.findParamAndSet("d_offset", 3.051757f);
	m_fog.m_keys.GetKey("effects_black", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.0f, 0.0f, 0.0f);
	fog.findParamAndSet("density", 1.0f, 0.287331f, 0.45528f, 10.0f);
	fog.findParamAndSet("light_color", 0.0f, 0.0f, 0.0f, 0.5625f);
	fog.findParamAndSet("d_scale", 50.0f);
	fog.findParamAndSet("d_offset", 0.042169f);
	m_fog.m_keys.GetKey("effects_blueTB_0513_01", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.1875f, 0.241071f, 0.464284f);
	fog.findParamAndSet("density", 0.635518f, 0.661322f, 1.0f, 10.0f);
	fog.findParamAndSet("light_color", 0.0f, 0.0f, 0.0f, 0.5625f);
	fog.findParamAndSet("d_scale", 50.0f);
	fog.findParamAndSet("d_offset", 0.325384f);
	m_fog.m_keys.GetKey("effects_blue_FLAT", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.633928f, 0.026785f, 0.142857f);
	fog.findParamAndSet("density", 0.49618f, 0.635518f, 0.743466f, 10.0f);
	fog.findParamAndSet("light_color", 1.0f, 0.348214f, 0.080356f, 1.0f);
	fog.findParamAndSet("d_scale", 0.333194f);
	fog.findParamAndSet("d_offset", 3.41576f);
	m_fog.m_keys.GetKey("effects_bluewhite_05", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.9375f, 0.651784f, 0.553571f);
	fog.findParamAndSet("density", 0.364085f, 0.287331f, 0.381115f, 10.0f);
	fog.findParamAndSet("light_color", 0.294642f, 0.1875f, 0.580357f, 1.0f);
	fog.findParamAndSet("d_scale", 1.686795f);
	fog.findParamAndSet("d_offset", 17.292307f);
	m_fog.m_keys.GetKey("effects_casis", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.142857f, 0.607142f, 0.875f);
	fog.findParamAndSet("density", 0.120587f, 0.135881f, 0.260307f, 10.0f);
	fog.findParamAndSet("light_color", 0.053567f, 0.25f, 0.625f, 1.0f);
	fog.findParamAndSet("d_scale", 2.507272f);
	fog.findParamAndSet("d_offset", 2.411756f);
	m_fog.m_keys.GetKey("effects_default_0512_01", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.071428f, 0.544642f, 0.303571f);
	fog.findParamAndSet("density", 0.635518f, 0.661322f, 1.0f, 10.0f);
	fog.findParamAndSet("light_color", 0.0f, 0.0f, 0.0f, 0.5625f);
	fog.findParamAndSet("d_scale", 50.0f);
	fog.findParamAndSet("d_offset", 0.325384f);
	m_fog.m_keys.GetKey("effects_green_0512_01", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.0f, 0.428571f, 0.517857f);
	fog.findParamAndSet("density", 0.331743f, 0.331743f, 0.331743f, 10.0f);
	fog.findParamAndSet("light_color", 0.0f, 0.0f, 0.0f, 0.5625f);
	fog.findParamAndSet("d_scale", 50.0f);
	fog.findParamAndSet("d_offset", 0.325384f);
	m_fog.m_keys.GetKey("effects_koiao_0512_01", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.321427f, 0.660714f, 0.071428f);
	fog.findParamAndSet("density", 0.45528f, 0.381115f, 0.364085f, 10.0f);
	fog.findParamAndSet("light_color", 0.339285f, 0.357142f, 0.160712f, 0.785714f);
	fog.findParamAndSet("d_scale", 0.376837f);
	fog.findParamAndSet("d_offset", 4.240448f);
	m_fog.m_keys.GetKey("effects_limegreen_0513_02", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.169642f, 0.0f, 0.294642f);
	fog.findParamAndSet("density", 0.128063f, 0.033735f, 0.113443f, 10.0f);
	fog.findParamAndSet("light_color", 0.357142f, 0.0625f, 0.151785f, 1.0f);
	fog.findParamAndSet("d_scale", 1.686795f);
	fog.findParamAndSet("d_offset", 3.051757f);
	m_fog.m_keys.GetKey("effects_maintosh", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.732142f, 0.696428f, 0.651784f);
	fog.findParamAndSet("density", 0.058153f, 0.0625f, 0.071918f, 10.0f);
	fog.findParamAndSet("light_color", 0.669642f, 0.526783f, 0.508926f, 1.0f);
	fog.findParamAndSet("d_scale", 2.907692f);
	fog.findParamAndSet("d_offset", 4.704748f);
	m_fog.m_keys.GetKey("effects_marrygold", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.214285f, 0.196427f, 0.169642f);
	fog.findParamAndSet("density", 0.087992f, 0.087992f, 0.087992f, 10.0f);
	fog.findParamAndSet("light_color", 0.0f, 0.0f, 0.0f, 0.0f);
	fog.findParamAndSet("d_scale", 2.507272f);
	fog.findParamAndSet("d_offset", 3.051757f);
	m_fog.m_keys.GetKey("effects_peach_0512_001", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.803571f, 0.223213f, 0.946428f);
	fog.findParamAndSet("density", 0.170787f, 0.120587f, 0.082361f, 10.0f);
	fog.findParamAndSet("light_color", 0.294642f, 0.0625f, 0.696428f, 1.0f);
	fog.findParamAndSet("d_scale", 1.686795f);
	fog.findParamAndSet("d_offset", 3.051757f);
	m_fog.m_keys.GetKey("effects_purple_01", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.830357f, 0.0f, 0.0f);
	fog.findParamAndSet("density", 0.635518f, 0.661322f, 1.0f, 10.0f);
	fog.findParamAndSet("light_color", 0.0f, 0.0f, 0.0f, 0.5625f);
	fog.findParamAndSet("d_scale", 50.0f);
	fog.findParamAndSet("d_offset", 0.325384f);
	m_fog.m_keys.GetKey("effects_purplered", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.607142f, 0.491071f, 0.875f);
	fog.findParamAndSet("density", 0.212022f, 0.170787f, 0.247532f, 10.0f);
	fog.findParamAndSet("light_color", 0.660714f, 0.5f, 0.776785f, 1.0f);
	fog.findParamAndSet("d_scale", 2.507272f);
	fog.findParamAndSet("d_offset", 5.746631f);
	m_fog.m_keys.GetKey("effects_redTB_0513_01", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.0f, 0.696428f, 0.866069f);
	fog.findParamAndSet("density", 0.287331f, 0.381115f, 0.539773f, 10.0f);
	fog.findParamAndSet("light_color", 0.0f, 1.0f, 1.0f, 1.0f);
	fog.findParamAndSet("d_scale", 1.686795f);
	fog.findParamAndSet("d_offset", 3.051757f);
	m_fog.m_keys.GetKey("effects_turquoise_0513_01", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.6875f, 0.6875f, 0.6875f);
	fog.findParamAndSet("density", 0.212022f, 0.212022f, 0.212022f, 10.0f);
	fog.findParamAndSet("light_color", 0.526783f, 0.526783f, 0.526783f, 1.0f);
	fog.findParamAndSet("d_scale", 2.907692f);
	fog.findParamAndSet("d_offset", 4.704748f);
	m_fog.m_keys.GetKey("effects_yellow_0512_01", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.776785f, 0.830357f, 0.196427f);
	fog.findParamAndSet("density", 0.082361f, 0.082361f, 0.082361f, 10.0f);
	fog.findParamAndSet("light_color", 0.946428f, 0.946428f, 0.116071f, 1.0f);
	fog.findParamAndSet("d_scale", 2.507272f);
	fog.findParamAndSet("d_offset", 3.051757f);
	m_fog.m_keys.GetKey("effects_color_vari_2g_01", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.276785f, 0.401784f, 0.285714f);
	fog.findParamAndSet("density", 0.082361f, 0.082361f, 0.082361f, 10.0f);
	fog.findParamAndSet("light_color", 0.205357f, 0.25f, 0.0f, 0.169642f);
	fog.findParamAndSet("d_scale", 2.507272f);
	fog.findParamAndSet("d_offset", 3.051757f);
	m_fog.m_keys.GetKey("effects_color_vari_2g_02", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.348214f, 0.714285f, 0.6875f);
	fog.findParamAndSet("density", 0.082361f, 0.082361f, 0.082361f, 10.0f);
	fog.findParamAndSet("light_color", 0.526783f, 0.526783f, 0.526783f, 1.0f);
	fog.findParamAndSet("d_scale", 2.507272f);
	fog.findParamAndSet("d_offset", 3.051757f);
	m_fog.m_keys.GetKey("effects_color_vari_2g_03", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.446427f, 0.508926f, 0.5625f);
	fog.findParamAndSet("density", 0.260307f, 0.260307f, 0.260307f, 10.0f);
	fog.findParamAndSet("light_color", 0.553571f, 0.553571f, 0.580357f, 0.607142f);
	fog.findParamAndSet("d_scale", 2.507272f);
	fog.findParamAndSet("d_offset", -0.674718f);
	m_fog.m_keys.GetKey("effects_color_vari_2g_04", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.732142f, 0.785714f, 0.8125f);
	fog.findParamAndSet("density", 0.058153f, 0.0625f, 0.071918f, 10.0f);
	fog.findParamAndSet("light_color", 0.553571f, 0.616071f, 0.883928f, 1.0f);
	fog.findParamAndSet("d_scale", 2.507272f);
	fog.findParamAndSet("d_offset", 3.41576f);
	m_fog.m_keys.GetKey("iboot", true)->m_values = fog.m_values;

	fog.findParamAndSet("color", 0.446427f, 0.508926f, 0.5625f);
	fog.findParamAndSet("density", 0.260307f, 0.260307f, 0.260307f, 10.0f);
	fog.findParamAndSet("light_color", 0.553571f, 0.553571f, 0.580357f, 0.607142f);
	fog.findParamAndSet("d_scale", 2.507272f);
	fog.findParamAndSet("d_offset", -0.674718f);
	m_fog.m_keys.GetKey("effects_original_01", true)->m_values = fog.m_values;

	FogParamSet &fog_lookdown = m_fog_lookdown.m_keys.m_default;

	fog_lookdown.findParamAndSet("color", 0.0f, 0.473214f, 0.946428f);
	fog_lookdown.findParamAndSet("density", 0.170787f, 0.201081f, 0.49618f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.0f, 0.0f, 0.0f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 1.686795f);
	fog_lookdown.findParamAndSet("d_offset", 3.051757f);
	m_fog_lookdown.m_keys.GetKey("effects_0406_ruby", true)->m_values = fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.0f, 0.473214f, 0.946428f);
	fog_lookdown.findParamAndSet("density", 0.170787f, 0.201081f, 0.49618f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.0f, 0.0f, 0.0f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 1.686795f);
	fog_lookdown.findParamAndSet("d_offset", 3.051757f);
	m_fog_lookdown.m_keys.GetKey("effects_0407_purple_01", true)->m_values =
		fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.0f, 0.473214f, 0.946428f);
	fog_lookdown.findParamAndSet("density", 0.170787f, 0.201081f, 0.49618f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.0f, 0.0f, 0.0f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 1.686795f);
	fog_lookdown.findParamAndSet("d_offset", 3.051757f);
	m_fog_lookdown.m_keys.GetKey("effects_0408_01_orange", true)->m_values =
		fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.0f, 0.473214f, 0.946428f);
	fog_lookdown.findParamAndSet("density", 0.170787f, 0.201081f, 0.49618f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.0f, 0.0f, 0.0f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 1.686795f);
	fog_lookdown.findParamAndSet("d_offset", 3.051757f);
	m_fog_lookdown.m_keys.GetKey("effects_0411_purple_02", true)->m_values =
		fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.732142f, 0.785714f, 0.8125f);
	fog_lookdown.findParamAndSet("density", 0.058153f, 0.0625f, 0.071918f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.553571f, 0.616071f, 0.883928f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 2.507272f);
	fog_lookdown.findParamAndSet("d_offset", 3.41576f);
	m_fog_lookdown.m_keys.GetKey("effects_0415_A5", true)->m_values = fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.0f, 0.473214f, 0.946428f);
	fog_lookdown.findParamAndSet("density", 0.170787f, 0.201081f, 0.49618f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.0f, 0.0f, 0.0f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 1.686795f);
	fog_lookdown.findParamAndSet("d_offset", 3.051757f);
	m_fog_lookdown.m_keys.GetKey("effects_0518_dalia", true)->m_values = fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.0f, 0.473214f, 0.946428f);
	fog_lookdown.findParamAndSet("density", 0.170787f, 0.201081f, 0.49618f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.0f, 0.0f, 0.0f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 1.686795f);
	fog_lookdown.findParamAndSet("d_offset", 3.051757f);
	m_fog_lookdown.m_keys.GetKey("effects_0518_gray_FLAT", true)->m_values =
		fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.732142f, 0.785714f, 0.8125f);
	fog_lookdown.findParamAndSet("density", 0.058153f, 0.0625f, 0.071918f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.553571f, 0.616071f, 0.883928f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 2.507272f);
	fog_lookdown.findParamAndSet("d_offset", 3.41576f);
	m_fog_lookdown.m_keys.GetKey("effects_0518_ruby", true)->m_values = fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.767857f, 0.660714f, 0.598214f);
	fog_lookdown.findParamAndSet("density", 0.058153f, 0.0625f, 0.071918f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.732142f, 0.616071f, 0.5625f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 2.507272f);
	fog_lookdown.findParamAndSet("d_offset", 3.41576f);
	m_fog_lookdown.m_keys.GetKey("effects_A1", true)->m_values = fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.0f, 0.473214f, 0.946428f);
	fog_lookdown.findParamAndSet("density", 0.170787f, 0.201081f, 0.49618f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.0f, 0.0f, 0.0f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 1.686795f);
	fog_lookdown.findParamAndSet("d_offset", 3.051757f);
	m_fog_lookdown.m_keys.GetKey("effects_black", true)->m_values = fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.0f, 0.0f, 0.0f);
	fog_lookdown.findParamAndSet("density", 0.058153f, 0.0625f, 0.071918f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.553571f, 0.714285f, 0.883928f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 2.507272f);
	fog_lookdown.findParamAndSet("d_offset", 3.41576f);
	m_fog_lookdown.m_keys.GetKey("effects_blueTB_0513_01", true)->m_values =
		fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.767857f, 0.660714f, 0.598214f);
	fog_lookdown.findParamAndSet("density", 0.058153f, 0.0625f, 0.071918f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.732142f, 0.616071f, 0.5625f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 2.507272f);
	fog_lookdown.findParamAndSet("d_offset", 3.41576f);
	m_fog_lookdown.m_keys.GetKey("effects_blue_FLAT", true)->m_values = fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.0f, 0.473214f, 0.946428f);
	fog_lookdown.findParamAndSet("density", 0.170787f, 0.201081f, 0.49618f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.0f, 0.0f, 0.0f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 1.686795f);
	fog_lookdown.findParamAndSet("d_offset", 3.051757f);
	m_fog_lookdown.m_keys.GetKey("effects_bluewhite_05", true)->m_values =
		fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.0f, 0.473214f, 0.946428f);
	fog_lookdown.findParamAndSet("density", 0.170787f, 0.201081f, 0.49618f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.0f, 0.0f, 0.0f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 1.686795f);
	fog_lookdown.findParamAndSet("d_offset", 3.051757f);
	m_fog_lookdown.m_keys.GetKey("effects_casis", true)->m_values = fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.732142f, 0.785714f, 0.8125f);
	fog_lookdown.findParamAndSet("density", 0.058153f, 0.0625f, 0.071918f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.553571f, 0.616071f, 0.883928f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 2.507272f);
	fog_lookdown.findParamAndSet("d_offset", 3.41576f);
	m_fog_lookdown.m_keys.GetKey("effects_default_0512_01", true)->m_values =
		fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.767857f, 0.660714f, 0.598214f);
	fog_lookdown.findParamAndSet("density", 0.058153f, 0.0625f, 0.071918f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.732142f, 0.616071f, 0.5625f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 2.507272f);
	fog_lookdown.findParamAndSet("d_offset", 3.41576f);
	m_fog_lookdown.m_keys.GetKey("effects_green_0512_01", true)->m_values =
		fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.767857f, 0.660714f, 0.598214f);
	fog_lookdown.findParamAndSet("density", 0.058153f, 0.0625f, 0.071918f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.732142f, 0.616071f, 0.5625f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 2.507272f);
	fog_lookdown.findParamAndSet("d_offset", 3.41576f);
	m_fog_lookdown.m_keys.GetKey("effects_koiao_0512_01", true)->m_values =
		fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.0f, 0.473214f, 0.946428f);
	fog_lookdown.findParamAndSet("density", 0.170787f, 0.201081f, 0.49618f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.0f, 0.0f, 0.0f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 1.686795f);
	fog_lookdown.findParamAndSet("d_offset", 3.051757f);
	m_fog_lookdown.m_keys.GetKey("effects_limegreen_0513_02", true)->m_values =
		fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.0f, 0.473214f, 0.946428f);
	fog_lookdown.findParamAndSet("density", 0.170787f, 0.201081f, 0.49618f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.0f, 0.0f, 0.0f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 1.686795f);
	fog_lookdown.findParamAndSet("d_offset", 3.051757f);
	m_fog_lookdown.m_keys.GetKey("effects_maintosh", true)->m_values = fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.732142f, 0.785714f, 0.8125f);
	fog_lookdown.findParamAndSet("density", 0.058153f, 0.0625f, 0.071918f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.553571f, 0.616071f, 0.883928f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 2.507272f);
	fog_lookdown.findParamAndSet("d_offset", 3.41576f);
	m_fog_lookdown.m_keys.GetKey("effects_marrygold", true)->m_values = fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.732142f, 0.785714f, 0.8125f);
	fog_lookdown.findParamAndSet("density", 0.058153f, 0.0625f, 0.071918f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.553571f, 0.616071f, 0.883928f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 2.507272f);
	fog_lookdown.findParamAndSet("d_offset", 3.41576f);
	m_fog_lookdown.m_keys.GetKey("effects_peach_0512_001", true)->m_values =
		fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.0f, 0.473214f, 0.946428f);
	fog_lookdown.findParamAndSet("density", 0.170787f, 0.201081f, 0.49618f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.0f, 0.0f, 0.0f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 1.686795f);
	fog_lookdown.findParamAndSet("d_offset", 3.051757f);
	m_fog_lookdown.m_keys.GetKey("effects_purple_01", true)->m_values = fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.767857f, 0.660714f, 0.598214f);
	fog_lookdown.findParamAndSet("density", 0.058153f, 0.0625f, 0.071918f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.732142f, 0.616071f, 0.5625f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 2.507272f);
	fog_lookdown.findParamAndSet("d_offset", 3.41576f);
	m_fog_lookdown.m_keys.GetKey("effects_purplered", true)->m_values = fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.732142f, 0.785714f, 0.8125f);
	fog_lookdown.findParamAndSet("density", 0.058153f, 0.0625f, 0.071918f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.553571f, 0.616071f, 0.883928f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 2.507272f);
	fog_lookdown.findParamAndSet("d_offset", 3.41576f);
	m_fog_lookdown.m_keys.GetKey("effects_redTB_0513_01", true)->m_values =
		fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.0f, 0.473214f, 0.946428f);
	fog_lookdown.findParamAndSet("density", 0.170787f, 0.201081f, 0.49618f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.0f, 0.0f, 0.0f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 1.686795f);
	fog_lookdown.findParamAndSet("d_offset", 3.051757f);
	m_fog_lookdown.m_keys.GetKey("effects_turquoise_0513_01", true)->m_values =
		fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.0f, 0.419642f, 0.946428f);
	fog_lookdown.findParamAndSet("density", 0.170787f, 0.201081f, 0.49618f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.464284f, 0.464284f, 0.0f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 1.686795f);
	fog_lookdown.findParamAndSet("d_offset", 3.051757f);
	m_fog_lookdown.m_keys.GetKey("effects_yellow_0512_01", true)->m_values =
		fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.732142f, 0.785714f, 0.8125f);
	fog_lookdown.findParamAndSet("density", 0.058153f, 0.0625f, 0.071918f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.553571f, 0.616071f, 0.883928f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 2.507272f);
	fog_lookdown.findParamAndSet("d_offset", 3.41576f);
	m_fog_lookdown.m_keys.GetKey("effects_color_vari_2g_01", true)->m_values =
		fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.732142f, 0.785714f, 0.8125f);
	fog_lookdown.findParamAndSet("density", 0.058153f, 0.0625f, 0.071918f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.553571f, 0.616071f, 0.883928f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 2.507272f);
	fog_lookdown.findParamAndSet("d_offset", 3.41576f);
	m_fog_lookdown.m_keys.GetKey("effects_color_vari_2g_02", true)->m_values =
		fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.732142f, 0.785714f, 0.8125f);
	fog_lookdown.findParamAndSet("density", 0.058153f, 0.0625f, 0.071918f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.553571f, 0.616071f, 0.883928f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 2.507272f);
	fog_lookdown.findParamAndSet("d_offset", 3.41576f);
	m_fog_lookdown.m_keys.GetKey("effects_color_vari_2g_03", true)->m_values =
		fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.732142f, 0.785714f, 0.8125f);
	fog_lookdown.findParamAndSet("density", 0.058153f, 0.0625f, 0.071918f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.553571f, 0.616071f, 0.883928f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 2.507272f);
	fog_lookdown.findParamAndSet("d_offset", 3.41576f);
	m_fog_lookdown.m_keys.GetKey("effects_color_vari_2g_04", true)->m_values =
		fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.732142f, 0.785714f, 0.8125f);
	fog_lookdown.findParamAndSet("density", 0.058153f, 0.0625f, 0.071918f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.553571f, 0.616071f, 0.883928f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 2.507272f);
	fog_lookdown.findParamAndSet("d_offset", 3.41576f);
	m_fog_lookdown.m_keys.GetKey("iboot", true)->m_values = fog_lookdown.m_values;

	fog_lookdown.findParamAndSet("color", 0.732142f, 0.785714f, 0.8125f);
	fog_lookdown.findParamAndSet("density", 0.058153f, 0.0625f, 0.071918f, 10.0f);
	fog_lookdown.findParamAndSet("light_color", 0.553571f, 0.616071f, 0.883928f, 1.0f);
	fog_lookdown.findParamAndSet("d_scale", 2.507272f);
	fog_lookdown.findParamAndSet("d_offset", 3.41576f);
	m_fog_lookdown.m_keys.GetKey("effects_original_01", true)->m_values = fog_lookdown.m_values;

	MaterialParamSet &material = m_material.m_keys.m_default;

	material.findParamAndSet("color", 0.75f, 0.276785f, 0.455357f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.598214f);
	material.findParamAndSet("specular_coeff", 1.525877f);
	material.findParamAndSet("specular_power", 68.75f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.120467f);
	material.findParamAndSet("fresnel_coeff", 0.940949f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.151785f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_0406_ruby", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.535714f, 0.535714f, 0.526783f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.214285f);
	material.findParamAndSet("specular_coeff", 0.42994f);
	material.findParamAndSet("specular_power", 4.342855f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.22318f);
	material.findParamAndSet("fresnel_coeff", 0.512249f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.196427f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_0407_purple_01", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.535714f, 0.535714f, 0.526783f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.214285f);
	material.findParamAndSet("specular_coeff", 0.42994f);
	material.findParamAndSet("specular_power", 4.342855f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.22318f);
	material.findParamAndSet("fresnel_coeff", 0.512249f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.196427f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_0408_01_orange", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.535714f, 0.169642f, 0.526783f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.214285f);
	material.findParamAndSet("specular_coeff", 0.42994f);
	material.findParamAndSet("specular_power", 4.342855f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.22318f);
	material.findParamAndSet("fresnel_coeff", 0.512249f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.196427f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_0411_purple_02", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.151785f, 0.142857f, 0.455357f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.598214f);
	material.findParamAndSet("specular_coeff", 1.525877f);
	material.findParamAndSet("specular_power", 68.75f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.120467f);
	material.findParamAndSet("fresnel_coeff", 0.940949f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.151785f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_0415_A5", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.303571f, 0.303571f, 0.303571f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.491071f);
	material.findParamAndSet("specular_coeff", 1.525877f);
	material.findParamAndSet("specular_power", 68.75f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.120467f);
	material.findParamAndSet("fresnel_coeff", 0.940949f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.151785f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_0518_dalia", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.0f, 0.0f, 0.0f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.125f);
	material.findParamAndSet("specular_coeff", 0.42994f);
	material.findParamAndSet("specular_power", 4.342855f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.22318f);
	material.findParamAndSet("fresnel_coeff", 0.512249f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.196427f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_0518_gray_FLAT", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.151785f, 0.142857f, 0.455357f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.598214f);
	material.findParamAndSet("specular_coeff", 1.525877f);
	material.findParamAndSet("specular_power", 68.75f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.120467f);
	material.findParamAndSet("fresnel_coeff", 0.940949f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.151785f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_0518_ruby", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.660714f, 0.544642f, 0.0f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.196427f);
	material.findParamAndSet("specular_coeff", 0.879928f);
	material.findParamAndSet("specular_power", 63.685715f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.058094f);
	material.findParamAndSet("fresnel_coeff", 0.940949f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.151785f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_A1", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.0f, 0.0f, 0.0f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.214285f);
	material.findParamAndSet("specular_coeff", 0.42994f);
	material.findParamAndSet("specular_power", 4.342855f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.22318f);
	material.findParamAndSet("fresnel_coeff", 0.512249f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.196427f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_black", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.0f, 0.053567f, 0.107141f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.0f);
	material.findParamAndSet("specular_coeff", 0.77008f);
	material.findParamAndSet("specular_power", 63.685715f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.076931f);
	material.findParamAndSet("fresnel_coeff", 0.804327f);
	material.findParamAndSet("refraction_coeff", 1.78723f);
	material.findParamAndSet("refraction_amount", 0.125f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_blueTB_0513_01", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.321427f, 0.053567f, 0.0f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.196427f);
	material.findParamAndSet("specular_coeff", 0.879928f);
	material.findParamAndSet("specular_power", 63.685715f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.058094f);
	material.findParamAndSet("fresnel_coeff", 0.940949f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.151785f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_blue_FLAT", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.348214f, 0.142857f, 0.089285f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.598214f);
	material.findParamAndSet("specular_coeff", 1.525877f);
	material.findParamAndSet("specular_power", 68.75f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.120467f);
	material.findParamAndSet("fresnel_coeff", 0.940949f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.151785f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_bluewhite_05", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.75f, 0.276785f, 0.455357f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.598214f);
	material.findParamAndSet("specular_coeff", 1.525877f);
	material.findParamAndSet("specular_power", 68.75f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.120467f);
	material.findParamAndSet("fresnel_coeff", 0.940949f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.151785f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_casis", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.151785f, 0.160712f, 0.455357f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.598214f);
	material.findParamAndSet("specular_coeff", 1.525877f);
	material.findParamAndSet("specular_power", 68.75f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.120467f);
	material.findParamAndSet("fresnel_coeff", 0.940949f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.151785f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_default_0512_01", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.321427f, 0.053567f, 0.0f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.196427f);
	material.findParamAndSet("specular_coeff", 0.879928f);
	material.findParamAndSet("specular_power", 63.685715f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.058094f);
	material.findParamAndSet("fresnel_coeff", 0.940949f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.151785f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_green_0512_01", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.0f, 0.169642f, 0.25f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.196427f);
	material.findParamAndSet("specular_coeff", 0.879928f);
	material.findParamAndSet("specular_power", 63.685715f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.058094f);
	material.findParamAndSet("fresnel_coeff", 0.940949f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.151785f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_koiao_0512_01", true)->m_values = material.m_values;

	material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.214285f);
	material.findParamAndSet("specular_coeff", 0.42994f);
	material.findParamAndSet("specular_power", 4.342855f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.22318f);
	material.findParamAndSet("fresnel_coeff", 0.512249f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.196427f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_limegreen_0513_02", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.535714f, 0.169642f, 0.526783f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.214285f);
	material.findParamAndSet("specular_coeff", 0.42994f);
	material.findParamAndSet("specular_power", 4.342855f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.22318f);
	material.findParamAndSet("fresnel_coeff", 0.512249f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.196427f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_maintosh", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.321427f, 0.383927f, 0.446427f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.491071f);
	material.findParamAndSet("specular_coeff", 1.525877f);
	material.findParamAndSet("specular_power", 68.75f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.120467f);
	material.findParamAndSet("fresnel_coeff", 0.940949f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.151785f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_marrygold", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.151785f, 0.142857f, 0.455357f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.598214f);
	material.findParamAndSet("specular_coeff", 1.525877f);
	material.findParamAndSet("specular_power", 68.75f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.120467f);
	material.findParamAndSet("fresnel_coeff", 0.940949f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.151785f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_peach_0512_001", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.535714f, 0.169642f, 0.526783f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.214285f);
	material.findParamAndSet("specular_coeff", 0.42994f);
	material.findParamAndSet("specular_power", 4.342855f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.22318f);
	material.findParamAndSet("fresnel_coeff", 0.512249f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.196427f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_purple_01", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.607142f, 0.053567f, 0.0f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.196427f);
	material.findParamAndSet("specular_coeff", 0.879928f);
	material.findParamAndSet("specular_power", 63.685715f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.058094f);
	material.findParamAndSet("fresnel_coeff", 0.940949f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.151785f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_purplered", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.321427f, 0.383927f, 0.446427f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.491071f);
	material.findParamAndSet("specular_coeff", 1.525877f);
	material.findParamAndSet("specular_power", 68.75f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.120467f);
	material.findParamAndSet("fresnel_coeff", 0.940949f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.151785f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_redTB_0513_01", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.535714f, 0.535714f, 0.526783f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.214285f);
	material.findParamAndSet("specular_coeff", 0.42994f);
	material.findParamAndSet("specular_power", 4.342855f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.22318f);
	material.findParamAndSet("fresnel_coeff", 0.512249f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.196427f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_turquoise_0513_01", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.303571f, 0.303571f, 0.303571f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.491071f);
	material.findParamAndSet("specular_coeff", 1.525877f);
	material.findParamAndSet("specular_power", 68.75f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.120467f);
	material.findParamAndSet("fresnel_coeff", 0.940949f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.151785f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_yellow_0512_01", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.383927f, 0.419642f, 0.321427f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.598214f);
	material.findParamAndSet("specular_coeff", 1.525877f);
	material.findParamAndSet("specular_power", 68.75f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.120467f);
	material.findParamAndSet("fresnel_coeff", 0.940949f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.151785f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_color_vari_2g_01", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.383927f, 0.419642f, 0.321427f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.598214f);
	material.findParamAndSet("specular_coeff", 1.525877f);
	material.findParamAndSet("specular_power", 68.75f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.120467f);
	material.findParamAndSet("fresnel_coeff", 0.940949f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.151785f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_color_vari_2g_02", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.383927f, 0.419642f, 0.321427f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.598214f);
	material.findParamAndSet("specular_coeff", 1.525877f);
	material.findParamAndSet("specular_power", 68.75f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.120467f);
	material.findParamAndSet("fresnel_coeff", 0.940949f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.151785f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_color_vari_2g_03", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.428571f, 0.4375f, 0.455357f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.598214f);
	material.findParamAndSet("specular_coeff", 1.525877f);
	material.findParamAndSet("specular_power", 68.75f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.120467f);
	material.findParamAndSet("fresnel_coeff", 0.940949f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.151785f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_color_vari_2g_04", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.321427f, 0.383927f, 0.446427f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.491071f);
	material.findParamAndSet("specular_coeff", 1.525877f);
	material.findParamAndSet("specular_power", 68.75f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.120467f);
	material.findParamAndSet("fresnel_coeff", 0.940949f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.151785f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("iboot", true)->m_values = material.m_values;

	material.findParamAndSet("color", 0.321427f, 0.383927f, 0.446427f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.491071f);
	material.findParamAndSet("specular_coeff", 1.525877f);
	material.findParamAndSet("specular_power", 68.75f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.120467f);
	material.findParamAndSet("fresnel_coeff", 0.940949f);
	material.findParamAndSet("refraction_coeff", 2.082463f);
	material.findParamAndSet("refraction_amount", 0.151785f);
	material.findParamAndSet("second_reflection", 0.0f);
	m_material.m_keys.GetKey("effects_original_01", true)->m_values = material.m_values;

	PointLightSphereParamSet &point_light = m_point_light.m_keys.m_default;

	point_light.findParamAndSet("theta_phi", 1.430546f, 2.468394f);
	point_light.findParamAndSet("distance", 1.504363f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 1.0f, 0.392857f, 0.214285f);
	point_light.findParamAndSet("diffuse_color", 0.875f, 0.285714f, 0.107141f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_0406_ruby", true)->m_values = point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, 1.178097f);
	point_light.findParamAndSet("distance", 3.003386f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.919642f, 0.026785f, 0.223213f);
	point_light.findParamAndSet("diffuse_color", 0.571428f, 0.125f, 0.6875f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_0407_purple_01", true)->m_values =
		point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, 2.468394f);
	point_light.findParamAndSet("distance", 1.504363f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.0f, 0.714285f, 1.0f);
	point_light.findParamAndSet("diffuse_color", 0.571428f, 0.125f, 0.6875f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_0408_01_orange", true)->m_values =
		point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, 1.178097f);
	point_light.findParamAndSet("distance", 3.003386f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.473214f, 0.026785f, 0.214285f);
	point_light.findParamAndSet("diffuse_color", 0.571428f, 0.125f, 0.633928f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_0411_purple_02", true)->m_values =
		point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, -1.065896f);
	point_light.findParamAndSet("distance", 1.099089f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 1.0f, 1.0f, 0.0f);
	point_light.findParamAndSet("diffuse_color", 0.571428f, 0.125f, 0.6875f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_0415_A5", true)->m_values = point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, -0.336599f);
	point_light.findParamAndSet("distance", 1.099089f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.6875f, 0.6875f, 0.723214f);
	point_light.findParamAndSet("diffuse_color", 0.571428f, 0.5625f, 0.580357f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_0518_dalia", true)->m_values = point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.402495f, 2.468394f);
	point_light.findParamAndSet("distance", 1.504363f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("diffuse_color", 0.098213f, 0.125f, 0.6875f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_0518_gray_FLAT", true)->m_values =
		point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, -1.065896f);
	point_light.findParamAndSet("distance", 1.099089f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.723214f, 0.080356f, 0.196427f);
	point_light.findParamAndSet("diffuse_color", 0.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_0518_ruby", true)->m_values = point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, -1.065896f);
	point_light.findParamAndSet("distance", 0.476425f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.901785f, 0.758928f, 0.035714f);
	point_light.findParamAndSet("diffuse_color", 0.232142f, 0.375f, 0.75f);
	point_light.findParamAndSet("fade", 0.660714f);
	m_point_light.m_keys.GetKey("effects_A1", true)->m_values = point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, 2.468394f);
	point_light.findParamAndSet("distance", 1.504363f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.0f, 0.0f, 0.3125f);
	point_light.findParamAndSet("diffuse_color", 0.571428f, 0.125f, 0.6875f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_black", true)->m_values = point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, -1.065896f);
	point_light.findParamAndSet("distance", 0.476425f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.0f, 0.196427f, 0.392857f);
	point_light.findParamAndSet("diffuse_color", 0.107141f, 0.5625f, 0.821428f);
	point_light.findParamAndSet("fade", 0.651784f);
	m_point_light.m_keys.GetKey("effects_blueTB_0513_01", true)->m_values =
		point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, -1.065896f);
	point_light.findParamAndSet("distance", 0.476425f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.330357f, 0.455357f, 0.767857f);
	point_light.findParamAndSet("diffuse_color", 0.232142f, 0.375f, 0.75f);
	point_light.findParamAndSet("fade", 0.660714f);
	m_point_light.m_keys.GetKey("effects_blue_FLAT", true)->m_values = point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.065896f, -2.300094f);
	point_light.findParamAndSet("distance", 0.205907f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.741071f, 0.455357f, 0.285714f);
	point_light.findParamAndSet("diffuse_color", 0.571428f, 0.125f, 0.6875f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_bluewhite_05", true)->m_values = point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, 2.468394f);
	point_light.findParamAndSet("distance", 1.504363f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 1.0f, 0.678571f, 0.419642f);
	point_light.findParamAndSet("diffuse_color", 0.571428f, 0.125f, 0.6875f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_casis", true)->m_values = point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, -1.065896f);
	point_light.findParamAndSet("distance", 1.099089f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.776785f, 0.785714f, 0.0f);
	point_light.findParamAndSet("diffuse_color", 0.571428f, 0.125f, 0.6875f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_default_0512_01", true)->m_values =
		point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, -1.065896f);
	point_light.findParamAndSet("distance", 0.476425f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.330357f, 0.910714f, 0.651784f);
	point_light.findParamAndSet("diffuse_color", 0.232142f, 0.375f, 0.75f);
	point_light.findParamAndSet("fade", 0.660714f);
	m_point_light.m_keys.GetKey("effects_green_0512_01", true)->m_values = point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, -1.065896f);
	point_light.findParamAndSet("distance", 0.0f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.0f, 0.6875f, 0.785714f);
	point_light.findParamAndSet("diffuse_color", 0.04464f, 0.732142f, 0.866069f);
	point_light.findParamAndSet("fade", 0.660714f);
	m_point_light.m_keys.GetKey("effects_koiao_0512_01", true)->m_values = point_light.m_values;

	point_light.findParamAndSet("theta_phi", 2.608643f, 2.075695f);
	point_light.findParamAndSet("distance", 3.617636f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.473214f, 0.25f, 0.0625f);
	point_light.findParamAndSet("diffuse_color", 0.089285f, 0.535714f, 0.6875f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_limegreen_0513_02", true)->m_values =
		point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, 1.178097f);
	point_light.findParamAndSet("distance", 3.003386f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.473214f, 0.026785f, 0.214285f);
	point_light.findParamAndSet("diffuse_color", 0.571428f, 0.125f, 0.6875f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_maintosh", true)->m_values = point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, -0.336599f);
	point_light.findParamAndSet("distance", 1.099089f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.651784f, 0.6875f, 0.732142f);
	point_light.findParamAndSet("diffuse_color", 0.589283f, 0.125f, 0.6875f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_marrygold", true)->m_values = point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, -1.065896f);
	point_light.findParamAndSet("distance", 1.099089f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.285714f, 0.116071f, 0.071428f);
	point_light.findParamAndSet("diffuse_color", 0.223213f, 0.080356f, 0.0f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_peach_0512_001", true)->m_values =
		point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, 1.178097f);
	point_light.findParamAndSet("distance", 3.003386f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.473214f, 0.026785f, 0.214285f);
	point_light.findParamAndSet("diffuse_color", 0.571428f, 0.125f, 0.6875f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_purple_01", true)->m_values = point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, -1.065896f);
	point_light.findParamAndSet("distance", 0.476425f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.848214f, 0.071428f, 0.3125f);
	point_light.findParamAndSet("diffuse_color", 0.232142f, 0.375f, 0.75f);
	point_light.findParamAndSet("fade", 0.660714f);
	m_point_light.m_keys.GetKey("effects_purplered", true)->m_values = point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, -0.336599f);
	point_light.findParamAndSet("distance", 1.099089f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.651784f, 0.6875f, 0.732142f);
	point_light.findParamAndSet("diffuse_color", 0.589283f, 0.125f, 0.6875f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_redTB_0513_01", true)->m_values = point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, 2.468394f);
	point_light.findParamAndSet("distance", 1.504363f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.053567f, 0.589283f, 0.758928f);
	point_light.findParamAndSet("diffuse_color", 0.571428f, 0.125f, 0.6875f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_turquoise_0513_01", true)->m_values =
		point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, -0.336599f);
	point_light.findParamAndSet("distance", 1.099089f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.6875f, 0.6875f, 0.723214f);
	point_light.findParamAndSet("diffuse_color", 0.571428f, 0.5625f, 0.580357f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_yellow_0512_01", true)->m_values =
		point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, -1.065896f);
	point_light.findParamAndSet("distance", 1.099089f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.776785f, 0.741071f, 0.116071f);
	point_light.findParamAndSet("diffuse_color", 0.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_color_vari_2g_01", true)->m_values =
		point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, -1.065896f);
	point_light.findParamAndSet("distance", 1.099089f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("diffuse_color", 0.125f, 0.125f, 0.125f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_color_vari_2g_02", true)->m_values =
		point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, -1.065896f);
	point_light.findParamAndSet("distance", 1.099089f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("diffuse_color", 0.0f, 0.330357f, 0.357142f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_color_vari_2g_03", true)->m_values =
		point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, -1.065896f);
	point_light.findParamAndSet("distance", 1.099089f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.633928f, 0.633928f, 0.625f);
	point_light.findParamAndSet("diffuse_color", 0.517857f, 0.517857f, 0.517857f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_color_vari_2g_04", true)->m_values =
		point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, -0.336599f);
	point_light.findParamAndSet("distance", 1.099089f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.651784f, 0.6875f, 0.732142f);
	point_light.findParamAndSet("diffuse_color", 0.589283f, 0.125f, 0.6875f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("iboot", true)->m_values = point_light.m_values;

	point_light.findParamAndSet("theta_phi", 1.430546f, -0.336599f);
	point_light.findParamAndSet("distance", 1.099089f);
	point_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	point_light.findParamAndSet("ambient_color", 0.651784f, 0.6875f, 0.732142f);
	point_light.findParamAndSet("diffuse_color", 0.589283f, 0.125f, 0.6875f);
	point_light.findParamAndSet("fade", 0.919642f);
	m_point_light.m_keys.GetKey("effects_original_01", true)->m_values = point_light.m_values;

	SkyParamSet &sky = m_sky.m_keys.m_default;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 1.0f, 0.0f, 0.0f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 1.0f, 0.482142f, 0.375f);
	sky.findParamAndSet("horizon_color", 1.0f, 0.196427f, 0.133928f);
	sky.findParamAndSet("wave_fog_alpha", 0.026785f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 0.821428f);
	sky.findParamAndSet("side_angle", 0.932816f);
	sky.findParamAndSet("horizon_angle", 0.126223f);
	sky.findParamAndSet("horizon_blend_range", 0.058153f);
	sky.findParamAndSet("horizon_curvature", 0.500222f);
	sky.findParamAndSet("sky_blend_start", 0.056096f);
	sky.findParamAndSet("sky_blend_range", 0.22549f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_0406_ruby", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 0.053567f, 0.04464f, 0.839285f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 0.794642f, 0.142857f, 0.125f);
	sky.findParamAndSet("horizon_color", 0.535714f, 0.017857f, 0.196427f);
	sky.findParamAndSet("wave_fog_alpha", 0.151785f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 0.821428f);
	sky.findParamAndSet("side_angle", 0.766386f);
	sky.findParamAndSet("horizon_angle", 0.144922f);
	sky.findParamAndSet("horizon_blend_range", 0.152587f);
	sky.findParamAndSet("horizon_curvature", 0.490873f);
	sky.findParamAndSet("sky_blend_start", 0.0f);
	sky.findParamAndSet("sky_blend_range", 0.211621f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_0407_purple_01", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 0.053567f, 0.04464f, 0.839285f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 0.0f, 0.169642f, 0.3125f);
	sky.findParamAndSet("horizon_color", 0.0f, 0.633928f, 0.776785f);
	sky.findParamAndSet("wave_fog_alpha", 0.026785f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 0.821428f);
	sky.findParamAndSet("side_angle", 0.170013f);
	sky.findParamAndSet("horizon_angle", 0.116874f);
	sky.findParamAndSet("horizon_blend_range", 0.152587f);
	sky.findParamAndSet("horizon_curvature", 0.472173f);
	sky.findParamAndSet("sky_blend_start", 0.0f);
	sky.findParamAndSet("sky_blend_range", 0.211621f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_0408_01_orange", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 0.053567f, 0.04464f, 0.839285f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 0.267857f, 0.026785f, 0.142857f);
	sky.findParamAndSet("horizon_color", 0.223213f, 0.04464f, 0.107141f);
	sky.findParamAndSet("wave_fog_alpha", 0.026785f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 0.821428f);
	sky.findParamAndSet("side_angle", 0.350312f);
	sky.findParamAndSet("horizon_angle", 0.261796f);
	sky.findParamAndSet("horizon_blend_range", 0.113443f);
	sky.findParamAndSet("horizon_curvature", 0.490873f);
	sky.findParamAndSet("sky_blend_start", 0.0f);
	sky.findParamAndSet("sky_blend_range", 0.211621f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_0411_purple_02", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 1.0f, 1.0f, 0.0f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 0.0625f, 0.616071f, 1.0f);
	sky.findParamAndSet("horizon_color", 0.348214f, 0.0f, 0.883928f);
	sky.findParamAndSet("wave_fog_alpha", 0.0f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 1.107141f);
	sky.findParamAndSet("side_angle", 0.83573f);
	sky.findParamAndSet("horizon_angle", 0.107524f);
	sky.findParamAndSet("horizon_blend_range", 0.071918f);
	sky.findParamAndSet("horizon_curvature", 0.322574f);
	sky.findParamAndSet("sky_blend_start", 0.0f);
	sky.findParamAndSet("sky_blend_range", 0.211621f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_0415_A5", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 0.0f, 0.0f, 0.0f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 0.794642f, 0.848214f, 0.901785f);
	sky.findParamAndSet("horizon_color", 0.026785f, 0.348214f, 0.696428f);
	sky.findParamAndSet("wave_fog_alpha", 0.026785f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 0.642857f);
	sky.findParamAndSet("side_angle", 0.170013f);
	sky.findParamAndSet("horizon_angle", 0.186999f);
	sky.findParamAndSet("horizon_blend_range", 0.082361f);
	sky.findParamAndSet("horizon_curvature", 0.523598f);
	sky.findParamAndSet("sky_blend_start", 0.0f);
	sky.findParamAndSet("sky_blend_range", 0.170013f);
	sky.findParamAndSet("pos_distortion_scale", 0.0f);
	sky.findParamAndSet("grad_distortion_scale", 0.0f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_0518_dalia", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 0.0f, 0.0f, 0.0f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 0.0f, 0.0f, 0.0f);
	sky.findParamAndSet("horizon_color", 0.169642f, 0.169642f, 0.160712f);
	sky.findParamAndSet("wave_fog_alpha", 0.223213f);
	sky.findParamAndSet("radius", 6.428077f);
	sky.findParamAndSet("xscale", 0.892857f);
	sky.findParamAndSet("side_angle", 1.099244f);
	sky.findParamAndSet("horizon_angle", 0.0f);
	sky.findParamAndSet("horizon_blend_range", 0.416962f);
	sky.findParamAndSet("horizon_curvature", 0.472173f);
	sky.findParamAndSet("sky_blend_start", 0.0f);
	sky.findParamAndSet("sky_blend_range", 0.239357f);
	sky.findParamAndSet("pos_distortion_scale", 1.0f);
	sky.findParamAndSet("grad_distortion_scale", 1.0f);
	sky.findParamAndSet("horizon_distortion_scale", 1.0f);
	m_sky.m_keys.GetKey("effects_0518_gray_FLAT", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 0.053567f, 0.04464f, 0.839285f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 0.848214f, 0.151785f, 0.267857f);
	sky.findParamAndSet("horizon_color", 1.0f, 0.098213f, 0.232142f);
	sky.findParamAndSet("wave_fog_alpha", 0.0f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 0.642857f);
	sky.findParamAndSet("side_angle", 0.54448f);
	sky.findParamAndSet("horizon_angle", 0.102849f);
	sky.findParamAndSet("horizon_blend_range", 0.180475f);
	sky.findParamAndSet("horizon_curvature", 0.458149f);
	sky.findParamAndSet("sky_blend_start", 0.0f);
	sky.findParamAndSet("sky_blend_range", 0.253227f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_0518_ruby", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.168299f);
	sky.findParamAndSet("distance", 49.506958f);
	sky.findParamAndSet("sun_color", 0.767857f, 0.517857f, 0.04464f);
	sky.findParamAndSet("sun_power", 27.20781f);
	sky.findParamAndSet("sun_control", 0.0f);
	sky.findParamAndSet("zenith_color", 0.901785f, 0.732142f, 0.0f);
	sky.findParamAndSet("horizon_color", 0.8125f, 0.669642f, 0.0f);
	sky.findParamAndSet("wave_fog_alpha", 0.0f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 1.107141f);
	sky.findParamAndSet("side_angle", 0.738648f);
	sky.findParamAndSet("horizon_angle", 0.130899f);
	sky.findParamAndSet("horizon_blend_range", 0.161495f);
	sky.findParamAndSet("horizon_curvature", 0.486198f);
	sky.findParamAndSet("sky_blend_start", 0.042074f);
	sky.findParamAndSet("sky_blend_range", 0.211621f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_A1", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 0.053567f, 0.04464f, 0.839285f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 0.0f, 0.0f, 0.0f);
	sky.findParamAndSet("horizon_color", 0.0f, 0.0f, 0.0f);
	sky.findParamAndSet("wave_fog_alpha", 0.026785f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 0.821428f);
	sky.findParamAndSet("side_angle", 0.766386f);
	sky.findParamAndSet("horizon_angle", 0.205699f);
	sky.findParamAndSet("horizon_blend_range", 0.152587f);
	sky.findParamAndSet("horizon_curvature", 0.490873f);
	sky.findParamAndSet("sky_blend_start", 0.0f);
	sky.findParamAndSet("sky_blend_range", 0.211621f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_black", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.168299f);
	sky.findParamAndSet("distance", 49.506958f);
	sky.findParamAndSet("sun_color", 0.330357f, 0.696428f, 1.0f);
	sky.findParamAndSet("sun_power", 27.20781f);
	sky.findParamAndSet("sun_control", 0.0f);
	sky.findParamAndSet("zenith_color", 0.0f, 0.0f, 0.0f);
	sky.findParamAndSet("horizon_color", 0.0f, 0.0f, 0.0f);
	sky.findParamAndSet("wave_fog_alpha", 0.0f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 1.107141f);
	sky.findParamAndSet("side_angle", 0.738648f);
	sky.findParamAndSet("horizon_angle", 0.261796f);
	sky.findParamAndSet("horizon_blend_range", 0.161495f);
	sky.findParamAndSet("horizon_curvature", 0.486198f);
	sky.findParamAndSet("sky_blend_start", 0.042074f);
	sky.findParamAndSet("sky_blend_range", 0.128406f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_blueTB_0513_01", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.168299f);
	sky.findParamAndSet("distance", 49.506958f);
	sky.findParamAndSet("sun_color", 0.330357f, 0.294642f, 0.598214f);
	sky.findParamAndSet("sun_power", 27.20781f);
	sky.findParamAndSet("sun_control", 0.0f);
	sky.findParamAndSet("zenith_color", 0.0f, 0.0f, 0.0f);
	sky.findParamAndSet("horizon_color", 0.223213f, 0.258925f, 0.544642f);
	sky.findParamAndSet("wave_fog_alpha", 0.0f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 1.107141f);
	sky.findParamAndSet("side_angle", 0.738648f);
	sky.findParamAndSet("horizon_angle", 0.261796f);
	sky.findParamAndSet("horizon_blend_range", 0.161495f);
	sky.findParamAndSet("horizon_curvature", 0.486198f);
	sky.findParamAndSet("sky_blend_start", 0.042074f);
	sky.findParamAndSet("sky_blend_range", 0.128406f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_blue_FLAT", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 0.053567f, 0.04464f, 0.839285f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 1.0f, 1.0f, 1.0f);
	sky.findParamAndSet("horizon_color", 0.892857f, 0.830357f, 0.276785f);
	sky.findParamAndSet("wave_fog_alpha", 0.026785f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 0.464284f);
	sky.findParamAndSet("side_angle", 0.461265f);
	sky.findParamAndSet("horizon_angle", 0.056096f);
	sky.findParamAndSet("horizon_blend_range", 0.113443f);
	sky.findParamAndSet("horizon_curvature", 0.392699f);
	sky.findParamAndSet("sky_blend_start", 0.098174f);
	sky.findParamAndSet("sky_blend_range", 0.128406f);
	sky.findParamAndSet("pos_distortion_scale", 0.381115f);
	sky.findParamAndSet("grad_distortion_scale", 0.001488f);
	sky.findParamAndSet("horizon_distortion_scale", 0.001488f);
	m_sky.m_keys.GetKey("effects_bluewhite_05", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 0.053567f, 0.04464f, 0.839285f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 1.0f, 0.883928f, 0.8125f);
	sky.findParamAndSet("horizon_color", 0.919642f, 0.410714f, 0.357142f);
	sky.findParamAndSet("wave_fog_alpha", 0.026785f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 0.821428f);
	sky.findParamAndSet("side_angle", 0.558349f);
	sky.findParamAndSet("horizon_angle", 0.112199f);
	sky.findParamAndSet("horizon_blend_range", 0.082361f);
	sky.findParamAndSet("horizon_curvature", 0.490873f);
	sky.findParamAndSet("sky_blend_start", 0.028049f);
	sky.findParamAndSet("sky_blend_range", 0.239357f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_casis", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 1.0f, 1.0f, 0.0f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 0.017857f, 0.0625f, 0.464284f);
	sky.findParamAndSet("horizon_color", 0.1875f, 0.383927f, 0.910714f);
	sky.findParamAndSet("wave_fog_alpha", 0.0f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 1.107141f);
	sky.findParamAndSet("side_angle", 0.83573f);
	sky.findParamAndSet("horizon_angle", 0.107524f);
	sky.findParamAndSet("horizon_blend_range", 0.071918f);
	sky.findParamAndSet("horizon_curvature", 0.322574f);
	sky.findParamAndSet("sky_blend_start", 0.0f);
	sky.findParamAndSet("sky_blend_range", 0.211621f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_default_0512_01", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.168299f);
	sky.findParamAndSet("distance", 49.506958f);
	sky.findParamAndSet("sun_color", 0.330357f, 0.294642f, 0.598214f);
	sky.findParamAndSet("sun_power", 27.20781f);
	sky.findParamAndSet("sun_control", 0.0f);
	sky.findParamAndSet("zenith_color", 0.0f, 0.410714f, 0.401784f);
	sky.findParamAndSet("horizon_color", 0.267857f, 0.660714f, 0.705355f);
	sky.findParamAndSet("wave_fog_alpha", 0.0f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 1.107141f);
	sky.findParamAndSet("side_angle", 0.738648f);
	sky.findParamAndSet("horizon_angle", 0.261796f);
	sky.findParamAndSet("horizon_blend_range", 0.161495f);
	sky.findParamAndSet("horizon_curvature", 0.486198f);
	sky.findParamAndSet("sky_blend_start", 0.042074f);
	sky.findParamAndSet("sky_blend_range", 0.128406f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_green_0512_01", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.168299f);
	sky.findParamAndSet("distance", 49.506958f);
	sky.findParamAndSet("sun_color", 0.330357f, 0.294642f, 0.598214f);
	sky.findParamAndSet("sun_power", 27.20781f);
	sky.findParamAndSet("sun_control", 0.0f);
	sky.findParamAndSet("zenith_color", 0.0f, 0.517857f, 0.758928f);
	sky.findParamAndSet("horizon_color", 0.0f, 0.785714f, 0.901785f);
	sky.findParamAndSet("wave_fog_alpha", 0.0f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 1.107141f);
	sky.findParamAndSet("side_angle", 0.738648f);
	sky.findParamAndSet("horizon_angle", 0.238424f);
	sky.findParamAndSet("horizon_blend_range", 0.161495f);
	sky.findParamAndSet("horizon_curvature", 0.486198f);
	sky.findParamAndSet("sky_blend_start", 0.042074f);
	sky.findParamAndSet("sky_blend_range", 0.128406f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_koiao_0512_01", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 0.053567f, 0.04464f, 0.678571f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 0.866069f, 1.0f, 0.089285f);
	sky.findParamAndSet("horizon_color", 0.366071f, 0.526783f, 0.0f);
	sky.findParamAndSet("wave_fog_alpha", 0.080356f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 0.821428f);
	sky.findParamAndSet("side_angle", 0.766386f);
	sky.findParamAndSet("horizon_angle", 0.219723f);
	sky.findParamAndSet("horizon_blend_range", 0.19057f);
	sky.findParamAndSet("horizon_curvature", 0.490873f);
	sky.findParamAndSet("sky_blend_start", 0.0f);
	sky.findParamAndSet("sky_blend_range", 0.211621f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_limegreen_0513_02", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 0.053567f, 0.04464f, 0.839285f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 0.053567f, 0.035714f, 0.1875f);
	sky.findParamAndSet("horizon_color", 0.116071f, 0.04464f, 0.267857f);
	sky.findParamAndSet("wave_fog_alpha", 0.026785f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 0.821428f);
	sky.findParamAndSet("side_angle", 0.350312f);
	sky.findParamAndSet("horizon_angle", 0.261796f);
	sky.findParamAndSet("horizon_blend_range", 0.113443f);
	sky.findParamAndSet("horizon_curvature", 0.490873f);
	sky.findParamAndSet("sky_blend_start", 0.0f);
	sky.findParamAndSet("sky_blend_range", 0.211621f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_maintosh", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 0.053567f, 0.04464f, 0.839285f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 0.839285f, 0.758928f, 0.732142f);
	sky.findParamAndSet("horizon_color", 0.660714f, 0.607142f, 0.580357f);
	sky.findParamAndSet("wave_fog_alpha", 0.026785f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 0.642857f);
	sky.findParamAndSet("side_angle", 0.530611f);
	sky.findParamAndSet("horizon_angle", 0.210374f);
	sky.findParamAndSet("horizon_blend_range", 0.144052f);
	sky.findParamAndSet("horizon_curvature", 0.523598f);
	sky.findParamAndSet("sky_blend_start", 0.0f);
	sky.findParamAndSet("sky_blend_range", 0.183882f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_marrygold", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 1.0f, 1.0f, 1.0f);
	sky.findParamAndSet("sun_power", 1.0f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 0.142857f, 0.107141f, 0.098213f);
	sky.findParamAndSet("horizon_color", 0.339285f, 0.214285f, 0.098213f);
	sky.findParamAndSet("wave_fog_alpha", 0.0f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 0.642857f);
	sky.findParamAndSet("side_angle", 0.128406f);
	sky.findParamAndSet("horizon_angle", 0.172974f);
	sky.findParamAndSet("horizon_blend_range", 0.152587f);
	sky.findParamAndSet("horizon_curvature", 0.341274f);
	sky.findParamAndSet("sky_blend_start", 0.0f);
	sky.findParamAndSet("sky_blend_range", 0.211621f);
	sky.findParamAndSet("pos_distortion_scale", 0.0f);
	sky.findParamAndSet("grad_distortion_scale", 0.000415f);
	sky.findParamAndSet("horizon_distortion_scale", 0.000415f);
	m_sky.m_keys.GetKey("effects_peach_0512_001", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 0.053567f, 0.04464f, 0.839285f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 0.1875f, 0.0f, 0.232142f);
	sky.findParamAndSet("horizon_color", 0.473214f, 0.017857f, 0.660714f);
	sky.findParamAndSet("wave_fog_alpha", 0.026785f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 0.821428f);
	sky.findParamAndSet("side_angle", 0.655433f);
	sky.findParamAndSet("horizon_angle", 0.205699f);
	sky.findParamAndSet("horizon_blend_range", 0.152587f);
	sky.findParamAndSet("horizon_curvature", 0.490873f);
	sky.findParamAndSet("sky_blend_start", 0.0f);
	sky.findParamAndSet("sky_blend_range", 0.211621f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_purple_01", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.168299f);
	sky.findParamAndSet("distance", 49.506958f);
	sky.findParamAndSet("sun_color", 0.330357f, 0.294642f, 0.598214f);
	sky.findParamAndSet("sun_power", 27.20781f);
	sky.findParamAndSet("sun_control", 0.0f);
	sky.findParamAndSet("zenith_color", 0.848214f, 0.321427f, 0.428571f);
	sky.findParamAndSet("horizon_color", 0.830357f, 0.089285f, 0.276785f);
	sky.findParamAndSet("wave_fog_alpha", 0.0f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 1.107141f);
	sky.findParamAndSet("side_angle", 0.738648f);
	sky.findParamAndSet("horizon_angle", 0.229074f);
	sky.findParamAndSet("horizon_blend_range", 0.067085f);
	sky.findParamAndSet("horizon_curvature", 0.486198f);
	sky.findParamAndSet("sky_blend_start", 0.042074f);
	sky.findParamAndSet("sky_blend_range", 0.128406f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_purplered", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 0.053567f, 0.04464f, 0.839285f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 0.901785f, 0.616071f, 0.803571f);
	sky.findParamAndSet("horizon_color", 0.723214f, 0.642857f, 0.866069f);
	sky.findParamAndSet("wave_fog_alpha", 0.026785f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 0.642857f);
	sky.findParamAndSet("side_angle", 0.83573f);
	sky.findParamAndSet("horizon_angle", 0.196349f);
	sky.findParamAndSet("horizon_blend_range", 0.144052f);
	sky.findParamAndSet("horizon_curvature", 0.523598f);
	sky.findParamAndSet("sky_blend_start", 0.0f);
	sky.findParamAndSet("sky_blend_range", 0.267095f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_redTB_0513_01", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 0.053567f, 0.04464f, 0.839285f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 0.0f, 0.758928f, 0.910714f);
	sky.findParamAndSet("horizon_color", 0.0f, 0.25f, 0.446427f);
	sky.findParamAndSet("wave_fog_alpha", 0.026785f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 0.821428f);
	sky.findParamAndSet("side_angle", 0.766386f);
	sky.findParamAndSet("horizon_angle", 0.205699f);
	sky.findParamAndSet("horizon_blend_range", 0.152587f);
	sky.findParamAndSet("horizon_curvature", 0.490873f);
	sky.findParamAndSet("sky_blend_start", 0.0f);
	sky.findParamAndSet("sky_blend_range", 0.211621f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_turquoise_0513_01", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 0.0f, 0.0f, 0.0f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 0.866069f, 0.892857f, 0.919642f);
	sky.findParamAndSet("horizon_color", 0.821428f, 0.839285f, 0.866069f);
	sky.findParamAndSet("wave_fog_alpha", 0.026785f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 0.642857f);
	sky.findParamAndSet("side_angle", 0.530611f);
	sky.findParamAndSet("horizon_angle", 0.261796f);
	sky.findParamAndSet("horizon_blend_range", 0.082361f);
	sky.findParamAndSet("horizon_curvature", 0.523598f);
	sky.findParamAndSet("sky_blend_start", 0.0f);
	sky.findParamAndSet("sky_blend_range", 0.170013f);
	sky.findParamAndSet("pos_distortion_scale", 0.0f);
	sky.findParamAndSet("grad_distortion_scale", 0.0f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_yellow_0512_01", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 0.053567f, 0.04464f, 0.839285f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 0.875f, 0.866071f, 0.0f);
	sky.findParamAndSet("horizon_color", 0.723214f, 0.767857f, 0.098214f);
	sky.findParamAndSet("wave_fog_alpha", 0.0f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 0.642857f);
	sky.findParamAndSet("side_angle", 0.54448f);
	sky.findParamAndSet("horizon_angle", 0.126223f);
	sky.findParamAndSet("horizon_blend_range", 0.113443f);
	sky.findParamAndSet("horizon_curvature", 0.458149f);
	sky.findParamAndSet("sky_blend_start", 0.0f);
	sky.findParamAndSet("sky_blend_range", 0.239359f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_color_vari_2g_01", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 0.053567f, 0.04464f, 0.839285f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 0.1875f, 0.232142f, 0.089285f);
	sky.findParamAndSet("horizon_color", 0.4375f, 0.464284f, 0.392857f);
	sky.findParamAndSet("wave_fog_alpha", 0.0f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 0.642857f);
	sky.findParamAndSet("side_angle", 0.54448f);
	sky.findParamAndSet("horizon_angle", 0.126223f);
	sky.findParamAndSet("horizon_blend_range", 0.180475f);
	sky.findParamAndSet("horizon_curvature", 0.458149f);
	sky.findParamAndSet("sky_blend_start", 0.0f);
	sky.findParamAndSet("sky_blend_range", 0.253227f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_color_vari_2g_02", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 0.053567f, 0.04464f, 0.839285f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 0.357142f, 0.508926f, 0.5f);
	sky.findParamAndSet("horizon_color", 0.508926f, 0.758928f, 0.803571f);
	sky.findParamAndSet("wave_fog_alpha", 0.0f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 0.642857f);
	sky.findParamAndSet("side_angle", 0.54448f);
	sky.findParamAndSet("horizon_angle", 0.126223f);
	sky.findParamAndSet("horizon_blend_range", 0.180475f);
	sky.findParamAndSet("horizon_curvature", 0.458149f);
	sky.findParamAndSet("sky_blend_start", 0.0f);
	sky.findParamAndSet("sky_blend_range", 0.253227f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_color_vari_2g_03", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 0.0f, 0.0f, 0.0f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 0.330357f, 0.330357f, 0.375f);
	sky.findParamAndSet("horizon_color", 0.241071f, 0.294642f, 0.392857f);
	sky.findParamAndSet("wave_fog_alpha", 0.0f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 1.107141f);
	sky.findParamAndSet("side_angle", 0.83573f);
	sky.findParamAndSet("horizon_angle", 0.116874f);
	sky.findParamAndSet("horizon_blend_range", 0.087992f);
	sky.findParamAndSet("horizon_curvature", 0.322574f);
	sky.findParamAndSet("sky_blend_start", 0.0f);
	sky.findParamAndSet("sky_blend_range", 0.211621f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_color_vari_2g_04", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 0.053567f, 0.04464f, 0.839285f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 0.482142f, 0.767857f, 0.901785f);
	sky.findParamAndSet("horizon_color", 0.723214f, 0.785714f, 0.883928f);
	sky.findParamAndSet("wave_fog_alpha", 0.026785f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 0.642857f);
	sky.findParamAndSet("side_angle", 0.83573f);
	sky.findParamAndSet("horizon_angle", 0.196349f);
	sky.findParamAndSet("horizon_blend_range", 0.144052f);
	sky.findParamAndSet("horizon_curvature", 0.523598f);
	sky.findParamAndSet("sky_blend_start", 0.392699f);
	sky.findParamAndSet("sky_blend_range", 0.586082f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("iboot", true)->m_values = sky.m_values;

	sky.findParamAndSet("theta_phi", 2.440341f, -0.336599f);
	sky.findParamAndSet("distance", 40.21638f);
	sky.findParamAndSet("sun_color", 0.0f, 0.0f, 0.0f);
	sky.findParamAndSet("sun_power", 21.564348f);
	sky.findParamAndSet("sun_control", 0.035714f);
	sky.findParamAndSet("zenith_color", 0.330357f, 0.330357f, 0.375f);
	sky.findParamAndSet("horizon_color", 0.241071f, 0.294642f, 0.392857f);
	sky.findParamAndSet("wave_fog_alpha", 0.0f);
	sky.findParamAndSet("radius", 8.363738f);
	sky.findParamAndSet("xscale", 1.107141f);
	sky.findParamAndSet("side_angle", 0.83573f);
	sky.findParamAndSet("horizon_angle", 0.116874f);
	sky.findParamAndSet("horizon_blend_range", 0.087992f);
	sky.findParamAndSet("horizon_curvature", 0.322574f);
	sky.findParamAndSet("sky_blend_start", 0.0f);
	sky.findParamAndSet("sky_blend_range", 0.211621f);
	sky.findParamAndSet("pos_distortion_scale", 0.005146f);
	sky.findParamAndSet("grad_distortion_scale", 0.005146f);
	sky.findParamAndSet("horizon_distortion_scale", 0.002108f);
	m_sky.m_keys.GetKey("effects_original_01", true)->m_values = sky.m_values;

	WaveInstanceParamSet &wave_instance = m_wave_instance.m_keys.m_default;

	wave_instance.findParamAndSet("distortion_scale", 0.113443f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.439483f, -0.025184f, 0.012327f, 0.897813f);
	wave_instance.findParamAndSet("scale", 0.49853f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 4.922255f);
	wave_instance.findParamAndSet("uv_scale", 0.42994f);
	wave_instance.findParamAndSet("uv_rotate", 221.78572f);
	wave_instance.findParamAndSet("center_color", 0.839285f, 0.303571f, 0.330357f);
	wave_instance.findParamAndSet("center_power", 0.063254f);
	wave_instance.findParamAndSet("edge_color", 1.0f, 0.696428f, 0.732142f);
	wave_instance.findParamAndSet("edge_power", 1.504363f);
	wave_instance.findParamAndSet("cross_section", 0.199972f, 0.253692f, -0.241464f, 2.056981f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.109336f, -0.307033f, 1.053961f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_0406_ruby", true)->m_values = wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.120587f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.439656f, 0.0f, 0.0f, 0.898166f);
	wave_instance.findParamAndSet("scale", 0.539773f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 4.922255f);
	wave_instance.findParamAndSet("uv_scale", 0.625f);
	wave_instance.findParamAndSet("uv_rotate", 221.78572f);
	wave_instance.findParamAndSet("center_color", 0.232142f, 0.0f, 0.0f);
	wave_instance.findParamAndSet("center_power", 0.063254f);
	wave_instance.findParamAndSet("edge_color", 0.839285f, 0.651784f, 0.446427f);
	wave_instance.findParamAndSet("edge_power", 1.289822f);
	wave_instance.findParamAndSet("cross_section", 0.199972f, 0.253692f, -0.241464f, 2.056981f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.109336f, -0.307033f, 1.053961f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_0407_purple_01", true)->m_values =
		wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.120587f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.439656f, 0.0f, 0.0f, 0.898166f);
	wave_instance.findParamAndSet("scale", 0.586176f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 4.922255f);
	wave_instance.findParamAndSet("uv_scale", 0.501454f);
	wave_instance.findParamAndSet("uv_rotate", 221.78572f);
	wave_instance.findParamAndSet("center_color", 0.214285f, 0.446427f, 0.571428f);
	wave_instance.findParamAndSet("center_power", 0.117187f);
	wave_instance.findParamAndSet("edge_color", 0.366071f, 0.607142f, 0.8125f);
	wave_instance.findParamAndSet("edge_power", 4.57763f);
	wave_instance.findParamAndSet("cross_section", 0.199972f, 0.253692f, -0.241464f, 2.056981f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.109336f, -0.307033f, 1.053961f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_0408_01_orange", true)->m_values =
		wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.054037f);
	wave_instance.findParamAndSet("translation", 0.02569f, 0.22212f, -2.850654f);
	wave_instance.findParamAndSet("rotation", -0.403026f, 0.039285f, 0.043514f, 0.913308f);
	wave_instance.findParamAndSet("scale", 0.49853f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 4.922255f);
	wave_instance.findParamAndSet("uv_scale", 0.464665f);
	wave_instance.findParamAndSet("uv_rotate", 231.42857f);
	wave_instance.findParamAndSet("center_color", 0.294642f, 0.035714f, 0.098213f);
	wave_instance.findParamAndSet("center_power", 0.063254f);
	wave_instance.findParamAndSet("edge_color", 1.0f, 0.839285f, 0.928571f);
	wave_instance.findParamAndSet("edge_power", 7.057122f);
	wave_instance.findParamAndSet("cross_section", 0.210266f, 0.25369f, -0.022578f, 2.056981f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.109336f, -0.307033f, 1.053961f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_0411_purple_02", true)->m_values =
		wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.120587f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.439656f, 0.0f, 0.0f, 0.898166f);
	wave_instance.findParamAndSet("scale", 0.49853f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 4.922255f);
	wave_instance.findParamAndSet("uv_scale", 0.879928f);
	wave_instance.findParamAndSet("uv_rotate", 221.78572f);
	wave_instance.findParamAndSet("center_color", 0.0f, 0.0f, 0.982142f);
	wave_instance.findParamAndSet("center_power", 0.087123f);
	wave_instance.findParamAndSet("edge_color", 1.0f, 1.0f, 1.0f);
	wave_instance.findParamAndSet("edge_power", 0.007324f);
	wave_instance.findParamAndSet("cross_section", 0.194825f, 0.253699f, -0.293658f, 1.538535f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.109336f, -0.307033f, 1.053961f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_0415_A5", true)->m_values = wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.077008f);
	wave_instance.findParamAndSet("translation", 0.067883f, -0.453926f, -2.998693f);
	wave_instance.findParamAndSet("rotation", -0.483834f, -0.071704f, 0.000642f, 0.872216f);
	wave_instance.findParamAndSet("scale", 0.49853f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 5.857975f);
	wave_instance.findParamAndSet("uv_scale", 0.625f);
	wave_instance.findParamAndSet("uv_rotate", 295.7143f);
	wave_instance.findParamAndSet("center_color", 0.241071f, 0.4375f, 0.526783f);
	wave_instance.findParamAndSet("center_power", 0.007324f);
	wave_instance.findParamAndSet("edge_color", 0.508926f, 0.428571f, 0.455357f);
	wave_instance.findParamAndSet("edge_power", 3.003386f);
	wave_instance.findParamAndSet("cross_section", 0.210267f, 0.22794f, -0.087715f, 1.542543f);
	wave_instance.findParamAndSet("cross_section_flat", 0.26689f, 0.68066f, 0.862735f, 5.562808f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_0518_dalia", true)->m_values =
		wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 1.0f);
	wave_instance.findParamAndSet("translation", -0.120936f, -0.349922f, -2.364098f);
	wave_instance.findParamAndSet("rotation", -0.205612f, -0.00041f, -0.000833f, 0.978633f);
	wave_instance.findParamAndSet("scale", 0.45528f, 1.0f, 0.260307f);
	wave_instance.findParamAndSet("global_scale", 3.585984f);
	wave_instance.findParamAndSet("uv_scale", 0.879928f);
	wave_instance.findParamAndSet("uv_rotate", 48.214287f);
	wave_instance.findParamAndSet("center_color", 0.071428f, 0.3125f, 0.794642f);
	wave_instance.findParamAndSet("center_power", 0.441069f);
	wave_instance.findParamAndSet("edge_color", 0.0f, 0.348214f, 0.767857f);
	wave_instance.findParamAndSet("edge_power", 7.057122f);
	wave_instance.findParamAndSet("cross_section", 0.133058f, 0.804424f, 0.102874f, 1.505366f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.109336f, -0.307033f, 1.053961f);
	wave_instance.findParamAndSet("decay", 0.553571f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.875f);
	m_wave_instance.m_keys.GetKey("effects_0518_gray_FLAT", true)->m_values =
		wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.100112f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.439656f, 0.0f, 0.0f, 0.898166f);
	wave_instance.findParamAndSet("scale", 0.562616f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 4.922255f);
	wave_instance.findParamAndSet("uv_scale", 0.879928f);
	wave_instance.findParamAndSet("uv_rotate", 221.78572f);
	wave_instance.findParamAndSet("center_color", 0.8125f, 0.8125f, 0.803571f);
	wave_instance.findParamAndSet("center_power", 0.134827f);
	wave_instance.findParamAndSet("edge_color", 0.883928f, 0.883928f, 0.714285f);
	wave_instance.findParamAndSet("edge_power", 2.470894f);
	wave_instance.findParamAndSet("cross_section", 0.210267f, 0.253699f, -0.137129f, 1.211933f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.109336f, -0.307033f, 1.053961f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_0518_ruby", true)->m_values = wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.087992f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.439656f, 0.0f, 0.0f, 0.898166f);
	wave_instance.findParamAndSet("scale", 0.49853f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 5.857975f);
	wave_instance.findParamAndSet("uv_scale", 0.310112f);
	wave_instance.findParamAndSet("uv_rotate", 253.92857f);
	wave_instance.findParamAndSet("center_color", 0.669642f, 0.267857f, 0.116071f);
	wave_instance.findParamAndSet("center_power", 0.087123f);
	wave_instance.findParamAndSet("edge_color", 0.9375f, 0.919642f, 0.839285f);
	wave_instance.findParamAndSet("edge_power", 4.076475f);
	wave_instance.findParamAndSet("cross_section", 0.077203f, -0.043735f, 0.759908f, 1.058961f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.238011f, -0.307033f, 1.96111f);
	wave_instance.findParamAndSet("decay", 0.4375f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.5625f);
	m_wave_instance.m_keys.GetKey("effects_A1", true)->m_values = wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.120587f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.439656f, 0.0f, 0.0f, 0.898166f);
	wave_instance.findParamAndSet("scale", 0.49853f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 4.922255f);
	wave_instance.findParamAndSet("uv_scale", 0.879928f);
	wave_instance.findParamAndSet("uv_rotate", 221.78572f);
	wave_instance.findParamAndSet("center_color", 0.0f, 0.0f, 0.0f);
	wave_instance.findParamAndSet("center_power", 0.002791f);
	wave_instance.findParamAndSet("edge_color", 0.0f, 0.0f, 0.0f);
	wave_instance.findParamAndSet("edge_power", 1.504363f);
	wave_instance.findParamAndSet("cross_section", 0.199972f, 0.253692f, -0.241464f, 2.056981f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.109336f, -0.307033f, 1.053961f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_black", true)->m_values = wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.087992f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.439656f, 0.0f, 0.0f, 0.898166f);
	wave_instance.findParamAndSet("scale", 0.586176f, 0.381115f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 5.857975f);
	wave_instance.findParamAndSet("uv_scale", 0.397198f);
	wave_instance.findParamAndSet("uv_rotate", 250.71428f);
	wave_instance.findParamAndSet("center_color", 0.053567f, 0.0625f, 0.3125f);
	wave_instance.findParamAndSet("center_power", 0.063254f);
	wave_instance.findParamAndSet("edge_color", 0.419642f, 0.723214f, 0.839285f);
	wave_instance.findParamAndSet("edge_power", 4.076475f);
	wave_instance.findParamAndSet("cross_section", 0.06176f, -0.038589f, 0.557301f, 0.787998f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.238011f, -0.307033f, 1.96111f);
	wave_instance.findParamAndSet("decay", 0.428571f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.5625f);
	m_wave_instance.m_keys.GetKey("effects_blueTB_0513_01", true)->m_values =
		wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.077008f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.439656f, 0.0f, 0.0f, 0.898166f);
	wave_instance.findParamAndSet("scale", 0.49853f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 5.857975f);
	wave_instance.findParamAndSet("uv_scale", 0.310112f);
	wave_instance.findParamAndSet("uv_rotate", 253.92857f);
	wave_instance.findParamAndSet("center_color", 0.0f, 0.196427f, 0.607142f);
	wave_instance.findParamAndSet("center_power", 0.087123f);
	wave_instance.findParamAndSet("edge_color", 0.9375f, 0.955357f, 1.0f);
	wave_instance.findParamAndSet("edge_power", 2.012557f);
	wave_instance.findParamAndSet("cross_section", 0.072056f, -0.051458f, 0.679165f, 0.961613f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.238011f, -0.307033f, 1.96111f);
	wave_instance.findParamAndSet("decay", 0.4375f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.5625f);
	m_wave_instance.m_keys.GetKey("effects_blue_FLAT", true)->m_values = wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.071918f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.439656f, 0.0f, 0.0f, 0.898166f);
	wave_instance.findParamAndSet("scale", 0.49853f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 4.922255f);
	wave_instance.findParamAndSet("uv_scale", 0.879928f);
	wave_instance.findParamAndSet("uv_rotate", 221.78572f);
	wave_instance.findParamAndSet("center_color", 0.419642f, 0.080356f, 0.178571f);
	wave_instance.findParamAndSet("center_power", 0.063254f);
	wave_instance.findParamAndSet("edge_color", 0.767857f, 0.705355f, 0.160712f);
	wave_instance.findParamAndSet("edge_power", 2.31024f);
	wave_instance.findParamAndSet("cross_section", 0.199972f, 0.253692f, -0.241464f, 2.056981f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.109336f, -0.307033f, 1.053961f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_bluewhite_05", true)->m_values =
		wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.113443f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.439483f, -0.025184f, 0.012327f, 0.897813f);
	wave_instance.findParamAndSet("scale", 0.49853f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 4.922255f);
	wave_instance.findParamAndSet("uv_scale", 0.42994f);
	wave_instance.findParamAndSet("uv_rotate", 221.78572f);
	wave_instance.findParamAndSet("center_color", 0.839285f, 0.321427f, 0.276785f);
	wave_instance.findParamAndSet("center_power", 0.063254f);
	wave_instance.findParamAndSet("edge_color", 0.928571f, 0.741071f, 0.696428f);
	wave_instance.findParamAndSet("edge_power", 1.504363f);
	wave_instance.findParamAndSet("cross_section", 0.199972f, 0.253692f, -0.241464f, 2.056981f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.109336f, -0.307033f, 1.053961f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_casis", true)->m_values = wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.120587f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.439656f, 0.0f, 0.0f, 0.898166f);
	wave_instance.findParamAndSet("scale", 0.539773f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 4.922255f);
	wave_instance.findParamAndSet("uv_scale", 0.74f);
	wave_instance.findParamAndSet("uv_rotate", 221.78572f);
	wave_instance.findParamAndSet("center_color", 0.107141f, 0.116071f, 1.0f);
	wave_instance.findParamAndSet("center_power", 0.117187f);
	wave_instance.findParamAndSet("edge_color", 0.839285f, 0.919642f, 0.919642f);
	wave_instance.findParamAndSet("edge_power", 0.063254f);
	wave_instance.findParamAndSet("cross_section", 0.199972f, 0.253692f, -0.241464f, 2.056981f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.109336f, -0.307033f, 1.053961f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_default_0512_01", true)->m_values =
		wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.093906f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.439656f, 0.0f, 0.0f, 0.898166f);
	wave_instance.findParamAndSet("scale", 0.49853f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 5.857975f);
	wave_instance.findParamAndSet("uv_scale", 0.310112f);
	wave_instance.findParamAndSet("uv_rotate", 253.92857f);
	wave_instance.findParamAndSet("center_color", 0.017857f, 0.428571f, 0.366071f);
	wave_instance.findParamAndSet("center_power", 0.087123f);
	wave_instance.findParamAndSet("edge_color", 0.821428f, 0.839285f, 0.803571f);
	wave_instance.findParamAndSet("edge_power", 2.012557f);
	wave_instance.findParamAndSet("cross_section", 0.072056f, -0.051458f, 0.679165f, 0.961613f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.238011f, -0.307033f, 1.96111f);
	wave_instance.findParamAndSet("decay", 0.4375f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.5625f);
	m_wave_instance.m_keys.GetKey("effects_green_0512_01", true)->m_values =
		wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.120587f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.439656f, 0.0f, 0.0f, 0.898166f);
	wave_instance.findParamAndSet("scale", 0.49853f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 5.857975f);
	wave_instance.findParamAndSet("uv_scale", 0.310112f);
	wave_instance.findParamAndSet("uv_rotate", 253.92857f);
	wave_instance.findParamAndSet("center_color", 0.017857f, 0.669642f, 0.982142f);
	wave_instance.findParamAndSet("center_power", 0.087123f);
	wave_instance.findParamAndSet("edge_color", 0.598214f, 0.848214f, 0.883928f);
	wave_instance.findParamAndSet("edge_power", 3.403316f);
	wave_instance.findParamAndSet("cross_section", 0.033452f, -0.051456f, 0.699727f, 0.961613f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.238011f, -0.307033f, 1.96111f);
	wave_instance.findParamAndSet("decay", 0.4375f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.5625f);
	m_wave_instance.m_keys.GetKey("effects_koiao_0512_01", true)->m_values =
		wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.100112f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.439656f, 0.0f, 0.0f, 0.898166f);
	wave_instance.findParamAndSet("scale", 0.772475f, 0.381115f, 0.301605f);
	wave_instance.findParamAndSet("global_scale", 3.714455f);
	wave_instance.findParamAndSet("uv_scale", 0.464665f);
	wave_instance.findParamAndSet("uv_rotate", 128.57143f);
	wave_instance.findParamAndSet("center_color", 0.357142f, 0.821428f, 0.125f);
	wave_instance.findParamAndSet("center_power", 0.15443f);
	wave_instance.findParamAndSet("edge_color", 0.642857f, 0.955357f, 0.125f);
	wave_instance.findParamAndSet("edge_power", 5.123641f);
	wave_instance.findParamAndSet("cross_section", 0.199972f, 0.253692f, -0.241464f, 2.056981f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.109336f, -0.307033f, 1.053961f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_limegreen_0513_02", true)->m_values =
		wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.087992f);
	wave_instance.findParamAndSet("translation", 0.05352f, 0.143787f, -2.626997f);
	wave_instance.findParamAndSet("rotation", -0.403026f, 0.039285f, 0.043514f, 0.913308f);
	wave_instance.findParamAndSet("scale", 0.539773f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 4.922255f);
	wave_instance.findParamAndSet("uv_scale", 0.464665f);
	wave_instance.findParamAndSet("uv_rotate", 231.42857f);
	wave_instance.findParamAndSet("center_color", 0.035714f, 0.04464f, 0.3125f);
	wave_instance.findParamAndSet("center_power", 0.063254f);
	wave_instance.findParamAndSet("edge_color", 0.741071f, 0.267857f, 0.839285f);
	wave_instance.findParamAndSet("edge_power", 2.012557f);
	wave_instance.findParamAndSet("cross_section", 0.194825f, 0.284574f, -0.292349f, 1.421681f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.109336f, -0.307033f, 1.053961f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_maintosh", true)->m_values = wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.077008f);
	wave_instance.findParamAndSet("translation", 0.067883f, -0.453926f, -2.998693f);
	wave_instance.findParamAndSet("rotation", -0.483834f, -0.071704f, 0.000642f, 0.872216f);
	wave_instance.findParamAndSet("scale", 0.49853f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 5.857975f);
	wave_instance.findParamAndSet("uv_scale", 0.625f);
	wave_instance.findParamAndSet("uv_rotate", 295.7143f);
	wave_instance.findParamAndSet("center_color", 0.553571f, 0.419642f, 0.303571f);
	wave_instance.findParamAndSet("center_power", 0.007324f);
	wave_instance.findParamAndSet("edge_color", 0.410714f, 0.339285f, 0.25f);
	wave_instance.findParamAndSet("edge_power", 3.198667f);
	wave_instance.findParamAndSet("cross_section", 0.210267f, 0.22794f, -0.087715f, 1.542543f);
	wave_instance.findParamAndSet("cross_section_flat", 0.26689f, 0.68066f, 0.862735f, 5.562808f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_marrygold", true)->m_values = wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.144052f);
	wave_instance.findParamAndSet("translation", -0.002604f, -0.211211f, -2.679461f);
	wave_instance.findParamAndSet("rotation", -0.480731f, 0.0f, 0.0f, 0.876867f);
	wave_instance.findParamAndSet("scale", 0.49853f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 4.922255f);
	wave_instance.findParamAndSet("uv_scale", 0.42994f);
	wave_instance.findParamAndSet("uv_rotate", 234.64285f);
	wave_instance.findParamAndSet("center_color", 0.267857f, 0.205357f, 0.151785f);
	wave_instance.findParamAndSet("center_power", 0.020014f);
	wave_instance.findParamAndSet("edge_color", 0.848214f, 0.589283f, 0.5f);
	wave_instance.findParamAndSet("edge_power", 3.841918f);
	wave_instance.findParamAndSet("cross_section", 0.210267f, 0.253699f, -0.137129f, 1.211933f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.109336f, -0.307033f, 1.053961f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_peach_0512_001", true)->m_values =
		wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.082361f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.439656f, 0.0f, 0.0f, 0.898166f);
	wave_instance.findParamAndSet("scale", 0.49853f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 4.922255f);
	wave_instance.findParamAndSet("uv_scale", 0.719183f);
	wave_instance.findParamAndSet("uv_rotate", 231.42857f);
	wave_instance.findParamAndSet("center_color", 0.410714f, 0.035714f, 0.642857f);
	wave_instance.findParamAndSet("center_power", 0.063254f);
	wave_instance.findParamAndSet("edge_color", 0.866069f, 0.303571f, 0.776785f);
	wave_instance.findParamAndSet("edge_power", 1.875f);
	wave_instance.findParamAndSet("cross_section", 0.199972f, 0.253692f, -0.241464f, 2.056981f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.109336f, -0.307033f, 1.053961f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_purple_01", true)->m_values = wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.113443f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.439656f, 0.0f, 0.0f, 0.898166f);
	wave_instance.findParamAndSet("scale", 0.49853f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 5.857975f);
	wave_instance.findParamAndSet("uv_scale", 0.21727f);
	wave_instance.findParamAndSet("uv_rotate", 231.42857f);
	wave_instance.findParamAndSet("center_color", 0.875f, 0.116071f, 0.455357f);
	wave_instance.findParamAndSet("center_power", 0.087123f);
	wave_instance.findParamAndSet("edge_color", 0.928571f, 0.741071f, 0.455357f);
	wave_instance.findParamAndSet("edge_power", 1.393999f);
	wave_instance.findParamAndSet("cross_section", 0.072056f, -0.059174f, 0.616946f, 1.069332f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.238011f, -0.307033f, 1.96111f);
	wave_instance.findParamAndSet("decay", 0.4375f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.5625f);
	m_wave_instance.m_keys.GetKey("effects_purplered", true)->m_values = wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.058153f);
	wave_instance.findParamAndSet("translation", 0.072841f, -0.489928f, -3.017712f);
	wave_instance.findParamAndSet("rotation", -0.518141f, 0.017704f, 0.0342f, 0.854427f);
	wave_instance.findParamAndSet("scale", 0.49853f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 5.658699f);
	wave_instance.findParamAndSet("uv_scale", 0.625f);
	wave_instance.findParamAndSet("uv_rotate", 250.71428f);
	wave_instance.findParamAndSet("center_color", 0.526783f, 0.383927f, 0.625f);
	wave_instance.findParamAndSet("center_power", 0.007324f);
	wave_instance.findParamAndSet("edge_color", 0.589283f, 0.446427f, 0.705355f);
	wave_instance.findParamAndSet("edge_power", 2.470894f);
	wave_instance.findParamAndSet("cross_section", 0.21284f, 0.233095f, 0.008506f, 1.17487f);
	wave_instance.findParamAndSet("cross_section_flat", 0.26689f, 0.68066f, 0.862735f, 5.562808f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_redTB_0513_01", true)->m_values =
		wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.128063f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.420472f, 0.016009f, 0.029842f, 0.906673f);
	wave_instance.findParamAndSet("scale", 0.897085f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 4.922255f);
	wave_instance.findParamAndSet("uv_scale", 0.397198f);
	wave_instance.findParamAndSet("uv_rotate", 54.642857f);
	wave_instance.findParamAndSet("center_color", 0.383927f, 0.419642f, 0.455357f);
	wave_instance.findParamAndSet("center_power", 0.117187f);
	wave_instance.findParamAndSet("edge_color", 0.678571f, 0.758928f, 0.830357f);
	wave_instance.findParamAndSet("edge_power", 3.403316f);
	wave_instance.findParamAndSet("cross_section", 0.199972f, 0.253692f, -0.241464f, 2.056981f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.109336f, -0.307033f, 1.053961f);
	wave_instance.findParamAndSet("decay", 0.285714f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_turquoise_0513_01", true)->m_values =
		wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.077008f);
	wave_instance.findParamAndSet("translation", 0.067883f, -0.453926f, -2.998693f);
	wave_instance.findParamAndSet("rotation", -0.483834f, -0.071704f, 0.000642f, 0.872216f);
	wave_instance.findParamAndSet("scale", 0.49853f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 5.857975f);
	wave_instance.findParamAndSet("uv_scale", 0.625f);
	wave_instance.findParamAndSet("uv_rotate", 295.7143f);
	wave_instance.findParamAndSet("center_color", 0.491071f, 0.5f, 0.526783f);
	wave_instance.findParamAndSet("center_power", 0.007324f);
	wave_instance.findParamAndSet("edge_color", 0.875f, 0.892857f, 0.910714f);
	wave_instance.findParamAndSet("edge_power", 6.702117f);
	wave_instance.findParamAndSet("cross_section", 0.210267f, 0.22794f, -0.087715f, 1.542543f);
	wave_instance.findParamAndSet("cross_section_flat", 0.26689f, 0.68066f, 0.862735f, 5.562808f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_yellow_0512_01", true)->m_values =
		wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.100112f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.439656f, 0.0f, 0.0f, 0.898166f);
	wave_instance.findParamAndSet("scale", 0.562616f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 4.922255f);
	wave_instance.findParamAndSet("uv_scale", 0.879928f);
	wave_instance.findParamAndSet("uv_rotate", 221.78572f);
	wave_instance.findParamAndSet("center_color", 0.625f, 0.625f, 0.625f);
	wave_instance.findParamAndSet("center_power", 0.538746f);
	wave_instance.findParamAndSet("edge_color", 0.901785f, 0.901785f, 0.901785f);
	wave_instance.findParamAndSet("edge_power", 2.012557f);
	wave_instance.findParamAndSet("cross_section", 0.210267f, 0.253699f, -0.137129f, 1.211933f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.109336f, -0.307033f, 1.053961f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_color_vari_2g_01", true)->m_values =
		wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.100112f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.439656f, 0.0f, 0.0f, 0.898166f);
	wave_instance.findParamAndSet("scale", 0.562616f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 4.922255f);
	wave_instance.findParamAndSet("uv_scale", 0.879928f);
	wave_instance.findParamAndSet("uv_rotate", 221.78572f);
	wave_instance.findParamAndSet("center_color", 0.6875f, 0.6875f, 0.598214f);
	wave_instance.findParamAndSet("center_power", 0.651812f);
	wave_instance.findParamAndSet("edge_color", 1.0f, 1.0f, 1.0f);
	wave_instance.findParamAndSet("edge_power", 1.744616f);
	wave_instance.findParamAndSet("cross_section", 0.210267f, 0.253699f, -0.137129f, 1.211933f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.109336f, -0.307033f, 1.053961f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_color_vari_2g_02", true)->m_values =
		wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.100112f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.439656f, 0.0f, 0.0f, 0.898166f);
	wave_instance.findParamAndSet("scale", 0.562616f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 4.922255f);
	wave_instance.findParamAndSet("uv_scale", 0.879928f);
	wave_instance.findParamAndSet("uv_rotate", 221.78572f);
	wave_instance.findParamAndSet("center_color", 0.705355f, 0.732142f, 0.758928f);
	wave_instance.findParamAndSet("center_power", 0.538746f);
	wave_instance.findParamAndSet("edge_color", 0.892857f, 1.0f, 0.946428f);
	wave_instance.findParamAndSet("edge_power", 1.393999f);
	wave_instance.findParamAndSet("cross_section", 0.210267f, 0.253699f, -0.137129f, 1.211933f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.109336f, -0.307033f, 1.053961f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_color_vari_2g_03", true)->m_values =
		wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.120587f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.439656f, 0.0f, 0.0f, 0.898166f);
	wave_instance.findParamAndSet("scale", 0.539773f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 4.922255f);
	wave_instance.findParamAndSet("uv_scale", 0.74f);
	wave_instance.findParamAndSet("uv_rotate", 221.78572f);
	wave_instance.findParamAndSet("center_color", 0.767857f, 0.767857f, 0.767857f);
	wave_instance.findParamAndSet("center_power", 0.117187f);
	wave_instance.findParamAndSet("edge_color", 1.0f, 1.0f, 0.803571f);
	wave_instance.findParamAndSet("edge_power", 0.853651f);
	wave_instance.findParamAndSet("cross_section", 0.199972f, 0.253692f, -0.241464f, 2.056981f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.109336f, -0.307033f, 1.053961f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_color_vari_2g_04", true)->m_values =
		wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.120587f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.439656f, 0.0f, 0.0f, 0.898166f);
	wave_instance.findParamAndSet("scale", 0.539773f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 4.922255f);
	wave_instance.findParamAndSet("uv_scale", 0.625f);
	wave_instance.findParamAndSet("uv_rotate", 221.78572f);
	wave_instance.findParamAndSet("center_color", 0.526783f, 0.553571f, 0.625f);
	wave_instance.findParamAndSet("center_power", 0.007324f);
	wave_instance.findParamAndSet("edge_color", 0.607142f, 0.651784f, 0.714285f);
	wave_instance.findParamAndSet("edge_power", 4.844873f);
	wave_instance.findParamAndSet("cross_section", 0.199972f, 0.253692f, -0.241464f, 2.056981f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.109336f, -0.307033f, 1.053961f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("iboot", true)->m_values = wave_instance.m_values;

	wave_instance.findParamAndSet("distortion_scale", 0.120587f);
	wave_instance.findParamAndSet("translation", 0.000371f, -0.009445f, -2.550752f);
	wave_instance.findParamAndSet("rotation", -0.439656f, 0.0f, 0.0f, 0.898166f);
	wave_instance.findParamAndSet("scale", 0.539773f, 0.450897f, 0.435805f);
	wave_instance.findParamAndSet("global_scale", 4.922255f);
	wave_instance.findParamAndSet("uv_scale", 0.74f);
	wave_instance.findParamAndSet("uv_rotate", 221.78572f);
	wave_instance.findParamAndSet("center_color", 0.767857f, 0.767857f, 0.767857f);
	wave_instance.findParamAndSet("center_power", 0.117187f);
	wave_instance.findParamAndSet("edge_color", 1.0f, 1.0f, 0.803571f);
	wave_instance.findParamAndSet("edge_power", 0.853651f);
	wave_instance.findParamAndSet("cross_section", 0.199972f, 0.253692f, -0.241464f, 2.056981f);
	wave_instance.findParamAndSet("cross_section_flat", 0.169096f, 0.109336f, -0.307033f, 1.053961f);
	wave_instance.findParamAndSet("decay", 0.464284f);
	wave_instance.findParamAndSet("shadow_nor_blend", 0.4f);
	m_wave_instance.m_keys.GetKey("effects_original_01", true)->m_values =
		wave_instance.m_values;

	WaveRendererParamSet &params = m_params.m_keys.m_default;

	params.findParamAndSet("edge_fog", 0.044196f, 0.777857f);
	params.findParamAndSet("pitch_max", -1.458596f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.196349f);
	params.findParamAndSet("pitch_debug", -0.698131f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_0406_ruby", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.455357f, 0.723214f);
	params.findParamAndSet("pitch_max", -1.458596f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.196349f);
	params.findParamAndSet("pitch_debug", -0.698131f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_0407_purple_01", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.455357f, 0.723214f);
	params.findParamAndSet("pitch_max", -1.458596f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.196349f);
	params.findParamAndSet("pitch_debug", -0.698131f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_0408_01_orange", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.455357f, 0.723214f);
	params.findParamAndSet("pitch_max", -1.458596f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.196349f);
	params.findParamAndSet("pitch_debug", -0.698131f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_0411_purple_02", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.37125f, 0.689464f);
	params.findParamAndSet("pitch_max", -1.458596f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.196349f);
	params.findParamAndSet("pitch_debug", -0.698131f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_0415_A5", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.99f, 0.300535f);
	params.findParamAndSet("pitch_max", -0.813448f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.743323f);
	params.findParamAndSet("pitch_debug", -0.743323f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_0518_dalia", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.035357f, 0.406607f);
	params.findParamAndSet("pitch_max", -1.458596f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.196349f);
	params.findParamAndSet("pitch_debug", -0.698131f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_0518_gray_FLAT", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.37125f, 0.822053f);
	params.findParamAndSet("pitch_max", -1.458596f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.196349f);
	params.findParamAndSet("pitch_debug", -0.698131f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_0518_ruby", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.37125f, 0.495f);
	params.findParamAndSet("pitch_max", -1.262246f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.687223f);
	params.findParamAndSet("pitch_debug", -0.673198f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_A1", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.455357f, 0.723214f);
	params.findParamAndSet("pitch_max", -1.458596f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.701248f);
	params.findParamAndSet("pitch_debug", -0.701248f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_black", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.37125f, 0.495f);
	params.findParamAndSet("pitch_max", -1.262246f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.687223f);
	params.findParamAndSet("pitch_debug", -0.673198f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_blueTB_0513_01", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.37125f, 0.495f);
	params.findParamAndSet("pitch_max", -1.262246f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.687223f);
	params.findParamAndSet("pitch_debug", -0.673198f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_blue_FLAT", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.294642f, 0.654107f);
	params.findParamAndSet("pitch_max", -1.458596f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.196349f);
	params.findParamAndSet("pitch_debug", -0.698131f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_bluewhite_05", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.044196f, 0.777857f);
	params.findParamAndSet("pitch_max", -1.458596f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.196349f);
	params.findParamAndSet("pitch_debug", -0.698131f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_casis", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.37125f, 0.689464f);
	params.findParamAndSet("pitch_max", -1.458596f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.196349f);
	params.findParamAndSet("pitch_debug", -0.698131f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_default_0512_01", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.37125f, 0.495f);
	params.findParamAndSet("pitch_max", -1.262246f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.687223f);
	params.findParamAndSet("pitch_debug", -0.673198f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_green_0512_01", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.37125f, 0.495f);
	params.findParamAndSet("pitch_max", -1.262246f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.687223f);
	params.findParamAndSet("pitch_debug", -0.673198f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_koiao_0512_01", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.5625f, 0.660714f);
	params.findParamAndSet("pitch_max", -1.458596f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.196349f);
	params.findParamAndSet("pitch_debug", -0.698131f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_limegreen_0513_02", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.455357f, 0.723214f);
	params.findParamAndSet("pitch_max", -1.458596f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.196349f);
	params.findParamAndSet("pitch_debug", -0.698131f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_maintosh", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.203302f, 0.857142f);
	params.findParamAndSet("pitch_max", -0.813448f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.743323f);
	params.findParamAndSet("pitch_debug", -0.743323f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_marrygold", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.327053f, 0.822053f);
	params.findParamAndSet("pitch_max", -1.458596f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.196349f);
	params.findParamAndSet("pitch_debug", -0.698131f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_peach_0512_001", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.455357f, 0.723214f);
	params.findParamAndSet("pitch_max", -1.458596f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.196349f);
	params.findParamAndSet("pitch_debug", -0.698131f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_purple_01", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.37125f, 0.495f);
	params.findParamAndSet("pitch_max", -1.262246f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.687223f);
	params.findParamAndSet("pitch_debug", -0.673198f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_purplered", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.401784f, 0.857142f);
	params.findParamAndSet("pitch_max", -0.813448f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.743323f);
	params.findParamAndSet("pitch_debug", -0.743323f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.333482f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_redTB_0513_01", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.455357f, 0.723214f);
	params.findParamAndSet("pitch_max", -1.458596f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.196349f);
	params.findParamAndSet("pitch_debug", -0.698131f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_turquoise_0513_01", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.203302f, 0.857142f);
	params.findParamAndSet("pitch_max", -0.813448f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.743323f);
	params.findParamAndSet("pitch_debug", -0.743323f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_yellow_0512_01", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.37125f, 0.822053f);
	params.findParamAndSet("pitch_max", -1.458596f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.196349f);
	params.findParamAndSet("pitch_debug", -0.698131f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_color_vari_2g_01", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.37125f, 0.822053f);
	params.findParamAndSet("pitch_max", -1.458596f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.196349f);
	params.findParamAndSet("pitch_debug", -0.698131f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_color_vari_2g_02", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.37125f, 0.822053f);
	params.findParamAndSet("pitch_max", -1.458596f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.196349f);
	params.findParamAndSet("pitch_debug", -0.698131f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_color_vari_2g_03", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.37125f, 0.689464f);
	params.findParamAndSet("pitch_max", -1.458596f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.196349f);
	params.findParamAndSet("pitch_debug", -0.698131f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_color_vari_2g_04", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.401784f, 0.857142f);
	params.findParamAndSet("pitch_max", -0.813448f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.743323f);
	params.findParamAndSet("pitch_debug", -0.743323f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("iboot", true)->m_values = params.m_values;

	params.findParamAndSet("edge_fog", 0.37125f, 0.689464f);
	params.findParamAndSet("pitch_max", -1.458596f);
	params.findParamAndSet("pitch_rest", -0.698131f);
	params.findParamAndSet("pitch_min", -0.196349f);
	params.findParamAndSet("pitch_debug", -0.698131f);
	params.findParamAndSet("roll_max", 0.180766f);
	params.findParamAndSet("roll_max_zone", 0.122173f);
	params.findParamAndSet("zoom", 0.535714f);
	params.findParamAndSet("camera_chase", 0.106621f);
	m_params.m_keys.GetKey("effects_original_01", true)->m_values = params.m_values;

	WaveTransitionParamSet &transition = m_transition.m_keys.m_default;

	transition.findParamAndSet("uv_rotate_distort_curve", 69.64286f);
	transition.findParamAndSet("uv_rotate_distort_control_chase", 0.1f);
	transition.findParamAndSet("uv_rotate_distort_control", 1.0f);
	m_transition.m_keys.GetKey("default", true)->m_values = transition.m_values;

	m_param_keys.AddVariant("effects_default_0512_01");
	m_param_keys.AddVariant("iboot");
	m_param_keys.AddVariant("effects_0406_ruby");
	m_param_keys.AddVariant("effects_0407_purple_01");
	m_param_keys.AddVariant("effects_0408_01_orange");
	m_param_keys.AddVariant("effects_0411_purple_02");
	m_param_keys.AddVariant("effects_0415_A5");
	m_param_keys.AddVariant("effects_0518_dalia");
	m_param_keys.AddVariant("effects_0518_gray_FLAT");
	m_param_keys.AddVariant("effects_0518_ruby");
	m_param_keys.AddVariant("effects_A1");
	m_param_keys.AddVariant("effects_black");
	m_param_keys.AddVariant("effects_blue_FLAT");
	m_param_keys.AddVariant("effects_blueTB_0513_01");
	m_param_keys.AddVariant("effects_bluewhite_05");
	m_param_keys.AddVariant("effects_casis");
	m_param_keys.AddVariant("effects_green_0512_01");
	m_param_keys.AddVariant("effects_koiao_0512_01");
	m_param_keys.AddVariant("effects_maintosh");
	m_param_keys.AddVariant("effects_marrygold");
	m_param_keys.AddVariant("effects_peach_0512_001");
	m_param_keys.AddVariant("effects_purple_01");
	m_param_keys.AddVariant("effects_purplered");
	m_param_keys.AddVariant("effects_yellow_0512_01");
	m_param_keys.AddVariant("effects_redTB_0513_01");
	m_param_keys.AddVariant("effects_limegreen_0513_02");
	m_param_keys.AddVariant("effects_turquoise_0513_01");
	m_param_keys.AddVariant("effects_color_vari_2g_01");
	m_param_keys.AddVariant("effects_color_vari_2g_02");
	m_param_keys.AddVariant("effects_color_vari_2g_03");
	m_param_keys.AddVariant("effects_color_vari_2g_04");
	m_param_keys.AddVariant("effects_original_01");
	m_param_keys.SetVariant("effects_default_0512_01");
	m_transition_keys.AddVariant("default");
	m_transition_keys.SetVariant("default");
}
}
}
