/*
 * EarthRenderer: the 3D globe effect from 2014 PAF builds.
 */
#include <stdint.h>
#include <gxm.h>
#include <vectormath.h>
#include <graphics/drawobj/effect_path.h>
#include <graphics/color.h>
#include <graphics/effects/earth_renderer.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/gutil.h>
#include <math/math.h>
#include <src/common/frame_count.h>
#include "../../std/math.h"

using sce::Vectormath::Simd::floatInVec;
using sce::Vectormath::Simd::Aos::Matrix4;
using sce::Vectormath::Simd::Aos::Quat;
using sce::Vectormath::Simd::Aos::Vector2;
using sce::Vectormath::Simd::Aos::Vector3;
using sce::Vectormath::Simd::Aos::Vector4;

namespace paf {
namespace graph {

/* each vertex where it sits in the 64x64 grid */
class EarthVertexGenerator : public VertexGenerator<SkydomeVertex> {
public:
	virtual SkydomeVertex operator()(const math::v2 &uv) const;
};

SkydomeVertex EarthVertexGenerator::operator()(const math::v2 &uv) const
{
	SkydomeVertex vertex;

	vertex.m_uv = uv;
	return vertex;
}

struct SphereHit {
	math::v4 m_point;
	bool m_hit;
};

/* projects a ray onto the sphere: intersection point if it hits, closest limb point otherwise */
static SphereHit ProjectRayToSphere(const math::v4 &center, float radius,
				    const math::v4 &origin, const math::v4 &direction)
{
	SphereHit hit;
	Vector4 dir = direction.getImpl();
	Vector4 ctr = center.getImpl();
	Vector4 orig = origin.getImpl();
	Vector4 closest = orig + dir * (dot(dir, ctr - orig) / dot(dir, dir));
	Vector4 offset = closest - ctr;
	float dist = length(offset).getAsFloat();

	if (dist > radius) {
		hit.m_point = math::v4(ctr + offset * sce::Vectormath::Simd::recipf(floatInVec(dist)) *
						     floatInVec(radius));
		hit.m_hit = false;
	} else {
		float half_chord = __builtin_sqrtf(radius * radius - dist * dist);

		hit.m_point = math::v4(closest - dir * floatInVec(half_chord));
		hit.m_hit = true;
	}
	return hit;
}

/* records the nearer positive intersection of ray with sphere (center, radius) */
static inline void IntersectRaySphere(Ray &ray, const math::v4 &center, float radius)
{
	Vector4 dir = ray.m_direction.getImpl();
	Vector4 ctr = center.getImpl();
	Vector4 orig = ray.m_origin.getImpl();
	Vector4 closest = orig + dir * (dot(dir, ctr - orig) / dot(dir, dir));
	float dist = length(closest - ctr).getAsFloat();

	if (dist <= radius) {
		floatInVec half_chord(__builtin_sqrtf(radius * radius - dist * dist));
		floatInVec near_dist = length((closest - dir * half_chord) - orig);

		if (!(near_dist < floatInVec(0.0f)).getAsBool() &&
		    (!ray.m_hit || !(near_dist > ray.m_distance).getAsBool())) {
			ray.m_distance = near_dist;
			ray.m_hit = true;
			ray.m_hit_point = math::v4(orig + dir * near_dist);
		}

		floatInVec far_dist = length((closest + dir * half_chord) - orig);

		if (!(far_dist < floatInVec(0.0f)).getAsBool() &&
		    (!ray.m_hit || !(far_dist > ray.m_distance).getAsBool())) {
			ray.m_distance = far_dist;
			ray.m_hit = true;
			ray.m_hit_point = math::v4(orig + dir * far_dist);
		}
	}
}

static inline math::v2 ComputeStretchRange(float ground, float sea)
{
	const float span = 0.19f;
	const float sea_base = 0.39f;
	float delta = sea - ground;

	return math::v2(delta / span, ground - (delta * sea_base) / span);
}

void EarthRenderer::UpdateTrackball()
{
	m_prev_rotation = m_rotation;
	if (gutil::debug::Keyboard::GetInstance()->m_keys[0xA4].m_down) {
		m_dragging = false;
		return;
	}

	gutil::debug::Mouse *mouse = gutil::debug::Mouse::GetInstance();
	math::v2 ndc((float)mouse->m_pointers[0].m_x * 2.0f / (float)mouse->m_screen_width - 1.0f,
		     1.0f - (float)mouse->m_pointers[0].m_y * 2.0f / (float)mouse->m_screen_height);
	Ray ray = m_camera->GetRay(ndc);
	float unit = m_camera->GetUnit();
	Vector4 center = m_transform.m_world.w().getImpl() * floatInVec(unit);

	center.setW(1.0f);

	float radius = m_earth.m_set.m_values.m_radius.m_value * m_radius_scale * unit;

	IntersectRaySphere(ray, math::v4(center), radius);

	SphereHit hit = ProjectRayToSphere(math::v4(center), radius, ray.m_origin, ray.m_direction);

	if (hit.m_hit) {
		if (mouse->m_buttons[0].m_pressed) {
			m_drag_start_point = hit.m_point;
			m_dragging = true;
			m_inertia = math::quaternion::Identity();
			m_drag_start_rotation = m_rotation;
		}
		m_last_hit_point = hit.m_point;
	} else if (mouse->m_buttons[0].m_pressed) {
		m_dragging = false;
	}
	m_drag_point = hit.m_point;

	if (!mouse->m_buttons[0].m_down) {
		m_dragging = false;
	} else if (m_dragging) {
		math::matrix world_to_local =
			m_transform.m_world.inverse1() *
			math::matrix(Matrix4::scale(Vector3(1.0f / m_camera->GetUnit())));
		Vector4 start_vec = world_to_local.getImpl() * m_drag_start_point.getImpl();

		start_vec.setW(0.0f);
		start_vec = start_vec * sce::Vectormath::Simd::recipf(length(start_vec));

		math::matrix cur_world_to_local =
			m_transform.m_world.inverse1() *
			math::matrix(Matrix4::scale(Vector3(1.0f / m_camera->GetUnit())));
		Vector4 cur_vec = cur_world_to_local.getImpl() * m_drag_point.getImpl();

		cur_vec.setW(0.0f);
		cur_vec = cur_vec * sce::Vectormath::Simd::recipf(length(cur_vec));

		float cos_angle = sce::Vectormath::Simd::fminf(
					  sce::Vectormath::Simd::fmaxf(dot(cur_vec, start_vec),
								       floatInVec(-1.0f)),
					  floatInVec(1.0f))
					  .getAsFloat();
		float angle = sce_paf_acosf(cos_angle);

		if (__builtin_fabsf(angle) > 0.001f) {
			Vector3 axis = cross(start_vec.getXYZ(), cur_vec.getXYZ());

			if (m_lock_y_axis) {
				axis = Vector3(0.0f, axis.getY(), 0.0f);
				angle *= length(axis).getAsFloat();
			}

			floatInVec axis_len = length(axis);

			if ((axis_len > floatInVec(0.001f)).getAsBool() || !m_lock_y_axis) {
				Vector3 unit_axis = axis * sce::Vectormath::Simd::recipf(axis_len);
				float half_angle = angle * 0.5f;
				math::quaternion turn(Quat(unit_axis * sce_paf_sinf(half_angle),
							   sce_paf_cosf(half_angle)));
				math::quaternion rotated = turn * m_drag_start_rotation;

				m_rotation = Pose::Normalize(rotated);
			}
		}
	}

	m_delta_rotation = m_rotation * m_prev_rotation.inverse();

	math::v3 delta_axis;
	float delta_angle;

	m_delta_rotation.toAxisAngle(delta_axis, delta_angle);
	if (__builtin_fabsf(delta_angle) >
	    m_earth.m_set.m_values.m_inertia_thresh.m_value * PARAM_RADIANS_PER_DEGREE)
		m_inertia = m_delta_rotation;

	Vector4 omega = Vector4(m_inertia.getImpl()) * floatInVec(0.5f) *
			floatInVec(m_earth.m_set.m_values.m_inertial_w_mul.m_value);
	math::quaternion deriv = math::quaternion(Quat(omega)) * m_rotation;
	Vector4 next_quat = Vector4(m_rotation.getImpl()) + Vector4(deriv.getImpl());

	m_inertia = math::quaternion(
		Quat(Vector4(m_inertia.getImpl()) *
		     floatInVec(m_earth.m_set.m_values.m_inertia_damp.m_value)));
	m_rotation = Pose::Normalize(math::quaternion(Quat(next_quat)));
}

EarthRenderer::EarthRenderer()
	: m_shaders(new EarthShaders()), m_loaded(false), m_wireframe(false),
	  m_blend_mode(BLEND_ALPHA), m_mip_quality(1.0f), m_debug_sphere(false),
	  m_lock_y_axis(true), m_enable_input(false), m_transform_locked(false),
	  m_radius_scale(1.0f), m_manager("EarthRenderer"), m_material(m_manager, ""),
	  m_light(m_manager, ""), m_earth(m_manager, ""), m_frame(-1),
	  m_prev_rotation(math::quaternion::Identity()),
	  m_delta_rotation(math::quaternion::Identity()),
	  m_rotation(math::quaternion::Identity()), m_inertia(math::quaternion::Identity()),
	  m_dragging(false), m_drag_start_rotation(math::quaternion::Identity()),
	  m_alpha_scale(1.0f), m_rs_earth(NULL)
{
}

EarthRenderer::~EarthRenderer()
{
	delete m_shaders;
	m_shaders = NULL;
}

void EarthRenderer::Init(common::SharedPtr<Camera> camera,
			 common::SharedPtr<Background> background,
			 common::SharedPtr<DrawEnv> draw_env,
			 common::SharedPtr<TextureOps> texture_ops)
{
	m_camera = camera;
	m_background = background;
	m_draw_env = draw_env;
	m_texture_ops = texture_ops;
}

bool EarthRenderer::Load(const string &dir)
{
	arl::ArlInstance *gl = arl::Gl();
	EarthVertexGenerator generator;

	m_mesh = WaveMesh<SkydomeVertex>(arl::SurfaceSize(64, 64),
					 ScePafGraphics_4D1E289A(gl, arl::VertexArraySpec::ATTRIB0_F2,
								 SCE_GXM_INDEX_SOURCE_INDEX_16BIT),
					 gl->m_default_allocator);
	m_mesh.Fill(generator);

	arl::ProgramState *state = gl;

	m_shaders->m_vert.m_object.init(*gl, state->m_patcher, m_mesh.m_vertices.m_spec,
					common::SharedPtr<arl::VertexArraySpec>(),
					common::SharedPtr<arl::VertexArraySpec>(),
					common::SharedPtr<arl::VertexArraySpec>());
	state = gl;
	m_shaders->m_frag.m_object.init(*gl, state->m_patcher);

	m_rs_earth = m_draw_env->AddLayer("m_rs_earth", 201, DrawLayer::MODE_KEEP, 0x4000);
	m_rs_earth->Push(0, 0, &EarthRenderer::Draw, this);
	m_rs_earth->m_draw_mode = 2;

	if (dir.find("vs0") == string::npos) {
		m_specular_map =
			common::SharedPtr<arl::Texture2D>(new arl::Texture2D(gl->m_default_allocator));
		ScePafGraphics_7C53C58A(m_specular_map, s_earth_path + "/earth_spec_01.dds",
					arl::SurfaceFlags());
		m_edge_iris =
			common::SharedPtr<arl::Texture2D>(new arl::Texture2D(gl->m_default_allocator));
		ScePafGraphics_7C53C58A(m_edge_iris, s_earth_path + "/rainbow.dds",
					arl::SurfaceFlags());
	} else {
		m_specular_map =
			common::SharedPtr<arl::Texture2D>(new arl::Texture2D(gl->m_default_allocator));
		ScePafGraphics_7C53C58A(m_specular_map, dir + "/earth/textures/earth_spec_01.dds",
					arl::SurfaceFlags());
		m_edge_iris =
			common::SharedPtr<arl::Texture2D>(new arl::Texture2D(gl->m_default_allocator));
		ScePafGraphics_7C53C58A(m_edge_iris, dir + "/earth/textures/rainbow.dds",
					arl::SurfaceFlags());
	}

	m_manager.SetPaths(dir, "earth/psets");

	EarthParamSet &earth = m_earth.m_keys.m_default;

	earth.findParamAndSet("edge_iris_fresnel_front", 0.0f);
	earth.findParamAndSet("edge_iris_fresnel_control", 0.2f);
	earth.findParamAndSet("edge_iris_fresnel_coeff", 1.0f);
	earth.findParamAndSet("edge_iris_start", 0.364085f);
	earth.findParamAndSet("radius", 0.364085f);
	earth.findParamAndSet("translation", 0.724838f, 0.449357f, -1.026731f);
	earth.findParamAndSet("rotation", 0.092517f, 0.098645f, -0.02249f, 0.990557f);
	earth.findParamAndSet("inertial_w_mul", 0.8f);
	earth.findParamAndSet("inertia_damp", 0.97f);
	earth.findParamAndSet("inertia_thresh", 1.0f);
	earth.findParamAndSet("alpha", 0.0f);
	earth.findParamAndSet("env_fade_start", 0.8f);
	earth.findParamAndSet("env_fade_end", 1.0f);
	earth.findParamAndSet("diffuse_ground", 0.579999f);
	earth.findParamAndSet("diffuse_sea", 0.39f);
	earth.findParamAndSet("specular_ground", 0.579999f);
	earth.findParamAndSet("specular_sea", 0.39f);
	earth.findParamAndSet("alpha_ground", 0.579999f);
	earth.findParamAndSet("alpha_sea", 0.39f);
	earth.findParamAndSet("ambiant_ground", 0.579999f);
	earth.findParamAndSet("ambiant_sea", 0.39f);
	m_earth.m_keys.GetKey("default", true)->m_values = earth.m_values;

	earth.findParamAndSet("edge_iris_fresnel_front", 0.0f);
	earth.findParamAndSet("edge_iris_fresnel_control", 0.016252f);
	earth.findParamAndSet("edge_iris_fresnel_coeff", 0.54353f);
	earth.findParamAndSet("edge_iris_start", 0.416962f);
	earth.findParamAndSet("radius", 0.398738f);
	earth.findParamAndSet("translation", 0.487608f, 0.107226f, -0.45f);
	earth.findParamAndSet("rotation", 0.092517f, 0.098645f, -0.02249f, 0.990557f);
	earth.findParamAndSet("inertial_w_mul", 0.032537f);
	earth.findParamAndSet("inertia_damp", 0.901785f);
	earth.findParamAndSet("inertia_thresh", 1.428571f);
	earth.findParamAndSet("alpha", 0.946428f);
	earth.findParamAndSet("env_fade_start", 0.848214f);
	earth.findParamAndSet("env_fade_end", 1.107141f);
	earth.findParamAndSet("diffuse_ground", 0.901785f);
	earth.findParamAndSet("diffuse_sea", 0.553571f);
	earth.findParamAndSet("specular_ground", 0.169642f);
	earth.findParamAndSet("specular_sea", 0.857142f);
	earth.findParamAndSet("alpha_ground", 0.526783f);
	earth.findParamAndSet("alpha_sea", 0.107141f);
	earth.findParamAndSet("ambiant_ground", 0.678571f);
	earth.findParamAndSet("ambiant_sea", 0.535714f);
	m_earth.m_keys.GetKey("iboot", true)->m_values = earth.m_values;

	MaterialParamSet &material = m_material.m_keys.m_default;

	material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 1.0f);
	material.findParamAndSet("specular_coeff", 3.98738f);
	material.findParamAndSet("specular_power", 6.114284f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.003373f);
	material.findParamAndSet("fresnel_coeff", 0.158879f);
	material.findParamAndSet("refraction_coeff", 1.608655f);
	material.findParamAndSet("refraction_amount", -0.151785f);
	m_material.m_keys.GetKey("default", true)->m_values = material.m_values;

	material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 1.0f);
	material.findParamAndSet("specular_coeff", 0.501454f);
	material.findParamAndSet("specular_power", 11.428569f);
	material.findParamAndSet("fresnel_front", 0.0f);
	material.findParamAndSet("fresnel_control", 0.03098f);
	material.findParamAndSet("fresnel_coeff", 1.667853f);
	material.findParamAndSet("refraction_coeff", 8.0f);
	material.findParamAndSet("refraction_amount", 0.5f);
	m_material.m_keys.GetKey("iboot", true)->m_values = material.m_values;

	PointLightSphereParamSet &light = m_light.m_keys.m_default;

	light.findParamAndSet("theta_phi", 1.262246f, 0.504898f);
	light.findParamAndSet("distance", 11.962143f);
	light.findParamAndSet("attn", 0.517633f, 0.381115f, 0.0f);
	light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	light.findParamAndSet("fade", 1.0f);
	m_light.m_keys.GetKey("default", true)->m_values = light.m_values;

	light.findParamAndSet("theta_phi", 0.504898f, 1.178097f);
	light.findParamAndSet("distance", 14.885398f);
	light.findParamAndSet("attn", 0.517633f, 0.381115f, 0.0f);
	light.findParamAndSet("ambient_color", 0.205357f, 0.2f, 0.205357f);
	light.findParamAndSet("diffuse_color", 0.276785f, 0.330357f, 0.616071f);
	light.findParamAndSet("fade", 1.0f);
	m_light.m_keys.GetKey("iboot", true)->m_values = light.m_values;

	m_manager.AddVariant("default");
	m_manager.AddVariant("iboot");
	m_manager.SetVariant("default");
	m_loaded = true;
	return true;
}

void EarthRenderer::Update(float frame_time)
{
	m_frame = common::FrameCount();
	m_manager.Update(frame_time);
	if (!m_transform_locked) {
		math::matrix world(m_earth.m_set.m_values.m_rotation.m_value);

		world.set_w(math::v4(m_earth.m_set.m_values.m_translation.m_value, 1.0f));
		m_transform.SetWorld(world);
	}
	if (m_earth.m_set.m_values.m_alpha.m_value * m_alpha_scale >= (1.0f / 255.0f) &&
	    m_enable_input)
		UpdateTrackball();
}

void EarthRenderer::Draw()
{
	float alpha = m_earth.m_set.m_values.m_alpha.m_value * m_alpha_scale;

	if (alpha < (1.0f / 255.0f))
		return;

	arl::ArlInstance *gl = arl::Gl();

	switch (m_blend_mode) {
	case BLEND_OPAQUE:
		gl->CullMode(arl::CULL_CW);
		gl->Disable(arl::BLEND);
		gl->Enable(arl::DEPTH_TEST);
		gl->DepthFunc(arl::DEPTH_LEQUAL, arl::FRONT_AND_BACK);
		gl->DepthMask(true, arl::FRONT_AND_BACK);
		break;
	case BLEND_ADDITIVE:
		gl->CullMode(arl::CULL_NONE);
		gl->Disable(arl::DEPTH_TEST);
		gl->DepthFunc(arl::DEPTH_LEQUAL, arl::FRONT_AND_BACK);
		gl->DepthMask(false, arl::FRONT_AND_BACK);
		gl->Enable(arl::BLEND);
		gl->BlendFunc(arl::ONE, arl::ONE, arl::ONE, arl::ONE);
		gl->BlendEquation(arl::FUNC_ADD, arl::FUNC_ADD);
		break;
	case BLEND_ALPHA:
		gl->CullMode(arl::CULL_CW);
		gl->Enable(arl::DEPTH_TEST);
		gl->DepthFunc(arl::DEPTH_LEQUAL, arl::FRONT_AND_BACK);
		gl->DepthMask(true, arl::FRONT_AND_BACK);
		gl->Enable(arl::BLEND);
		gl->BlendFunc(arl::SRC_ALPHA, arl::ONE_MINUS_SRC_ALPHA, arl::ONE, arl::ZERO);
		gl->BlendEquation(arl::FUNC_ADD, arl::FUNC_ADD);
		break;
	}

	if (m_wireframe)
		gl->PolygonMode(arl::POLYGON_MODE_LINE, arl::FRONT_AND_BACK);
	ScePafGraphics_199BD5F2(gl, 0);
	ScePafGraphics_FF97623F(m_specular_map.get(), m_mip_quality);

	EarthVertShader &vert = m_shaders->m_vert.m_interface;
	EarthFragShader &frag = m_shaders->m_frag.m_interface;

	frag.m_env_map_detail.Set(m_background->GetReflectionTexture());
	vert.m_mvp.Set(m_camera->GetViewProjection());
	vert.m_model.Set(m_transform.m_world * m_rotation.toMatrix(0.0001f));
	vert.m_world_scale_hack.Set(m_camera->GetUnit());
	vert.m_radius.Set(m_earth.m_set.m_values.m_radius.m_value * m_radius_scale);
	vert.m_eye.Set(m_camera->GetEyePos());
	vert.m_material_pack.Set(m_material.m_set.m_values.GetPack());
	vert.m_point_light_pack2.Set(m_light.m_set.m_values.GetPack2());
	vert.m_point_light_pack1.Set(m_light.m_set.m_values.GetPack1());

	const EarthParamSet::Values &ev = m_earth.m_set.m_values;

	vert.m_edge_iris_control.Set(math::v4(ev.m_edge_iris_fresnel_front.m_value,
					      ev.m_edge_iris_fresnel_control.m_value,
					      ev.m_edge_iris_fresnel_coeff.m_value,
					      ev.m_edge_iris_start.m_value));

	math::v2 diffuse_range =
		ComputeStretchRange(ev.m_diffuse_ground.m_value, ev.m_diffuse_sea.m_value);
	math::v2 specular_range =
		ComputeStretchRange(ev.m_specular_ground.m_value, ev.m_specular_sea.m_value);
	math::v2 alpha_range =
		ComputeStretchRange(ev.m_alpha_ground.m_value, ev.m_alpha_sea.m_value);
	math::v2 ambiant_range =
		ComputeStretchRange(ev.m_ambiant_ground.m_value, ev.m_ambiant_sea.m_value);

	frag.m_stretch_range_scale.Set(math::v4(diffuse_range.extract_x(),
						specular_range.extract_x(),
						alpha_range.extract_x(),
						ambiant_range.extract_x()));
	frag.m_stretch_range_offset.Set(math::v4(diffuse_range.extract_y(),
						 specular_range.extract_y(),
						 alpha_range.extract_y(),
						 ambiant_range.extract_y()));

	/* computed and discarded in Sony's build */
	m_camera->GetEyePos();
	m_light.m_set.m_values.GetPack1();

	{
		common::SharedPtr<arl::Texture2D> spec = m_specular_map;

		frag.m_specular_map.Set(
			arl::TextureSource(common::SharedPtr<arl::TextureObject>(spec)));
	}
	{
		common::SharedPtr<arl::Texture2D> iris = m_edge_iris;

		frag.m_edge_iris.Set(
			arl::TextureSource(common::SharedPtr<arl::TextureObject>(iris)));
	}
	frag.m_transparency.Set(ev.m_alpha.m_value * m_alpha_scale);

	m_shaders->m_vert.m_object.begin();
	m_shaders->m_frag.m_object.begin();
	if (m_mesh.m_index_count != 0)
		m_mesh.Draw(gl, m_mesh.m_height - 1);
	m_shaders->m_frag.m_object.end();
	m_shaders->m_vert.m_object.end();

	if (m_wireframe)
		gl->PolygonMode(arl::POLYGON_MODE_FILL, arl::FRONT_AND_BACK);
	if (m_debug_sphere) {
		m_camera->PushMatrices();
		m_camera->GetUnit();
		gl->DepthMask(true, arl::FRONT_AND_BACK);
		m_camera->PopMatrices();
	}
	ScePafGraphics_29F5AA8B(gl, NULL, false);
}

}
}
