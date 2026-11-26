/*
 * EarthRenderer: the 3D textured globe background effect present in 2014 PAF builds
 * (such as pup:psp2swu.self), drawn over a 64x64 grid bent into a sphere by
 * paf/cg/drawobj/earth/earth.vp and earth.fp with specular/sea-land stretch ranges,
 * Fresnel edge iridescence, reflection map, and inertial trackball rotation.
 */
#ifndef PAF_GRAPHICS_EFFECTS_EARTH_RENDERER_H
#define PAF_GRAPHICS_EFFECTS_EARTH_RENDERER_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/arl/gxm/gxm_shader.h>
#include <graphics/arl/gxm/gxm_texture.h>
#include <graphics/camera.h>
#include <graphics/drawobj/draw_env.h>
#include <graphics/effects/background.h>
#include <graphics/effects/param.h>
#include <graphics/effects/post_effects.h>
#include <graphics/effects/wave_param.h>
#include <graphics/effects/wave_renderer.h>
#include <math/ngp/matrix.h>
#include <math/ngp/quaternion.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <std/string>

namespace paf {
namespace graph {

/* paf/cg/drawobj/earth/earth.vp */
class EarthVertShader : public arl::ShaderInterfaceBase {
public:
	EarthVertShader()
		: m_mvp(m_setup), m_model(m_setup), m_radius(m_setup), m_world_scale_hack(m_setup),
		  m_material_pack(m_setup), m_point_light_pack2(m_setup),
		  m_point_light_pack1(m_setup), m_eye(m_setup), m_edge_iris_control(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("MVP", arl::ShaderParameterBase::UNIFORM, 0);
		m_model.init("Model", arl::ShaderParameterBase::UNIFORM, 0);
		m_radius.init("Radius", arl::ShaderParameterBase::UNIFORM, 0);
		m_world_scale_hack.init("WorldScaleHack", arl::ShaderParameterBase::UNIFORM, 0);
		m_material_pack.init("lit_params.MaterialPack", arl::ShaderParameterBase::UNIFORM,
				     0);
		m_point_light_pack2.init("lit_params.PointLightPack2",
					 arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack1.init("lit_params.PointLightPack1",
					 arl::ShaderParameterBase::UNIFORM, 0);
		m_eye.init("lit_params.Eye", arl::ShaderParameterBase::UNIFORM, 0);
		m_edge_iris_control.init("EdgeIrisControl", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_model.Apply();
		m_radius.Apply();
		m_world_scale_hack.Apply();
		m_material_pack.Apply();
		m_point_light_pack2.Apply();
		m_point_light_pack1.Apply();
		m_eye.Apply();
		m_edge_iris_control.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/earth/earth.vp", 2340);

	arl::ShaderParameter<math::matrix> m_mvp;
	arl::ShaderParameter<math::matrix> m_model;
	arl::ShaderParameter<float> m_radius;
	arl::ShaderParameter<float> m_world_scale_hack;
	arl::ShaderParameter<math::matrix> m_material_pack;
	arl::ShaderParameter<math::matrix> m_point_light_pack2;
	arl::ShaderParameter<math::matrix> m_point_light_pack1;
	arl::ShaderParameter<math::v4> m_eye;
	arl::ShaderParameter<math::v4> m_edge_iris_control;
};

/* paf/cg/drawobj/earth/earth.fp */
class EarthFragShader : public arl::ShaderInterfaceBase {
public:
	EarthFragShader()
		: m_specular_map(m_setup), m_edge_iris(m_setup), m_env_map_detail(m_setup),
		  m_stretch_range_scale(m_setup), m_stretch_range_offset(m_setup),
		  m_transparency(m_setup)
	{
	}

	virtual void Init()
	{
		m_specular_map.init("SpecularMap", 3, 0);
		m_edge_iris.init("EdgeIris", 4, 0);
		m_env_map_detail.init("EnvMapDetail", 5, 0);
		m_stretch_range_scale.init("StretchRangeScale", arl::ShaderParameterBase::UNIFORM,
					   0);
		m_stretch_range_offset.init("StretchRangeOffset", arl::ShaderParameterBase::UNIFORM,
					    0);
		m_transparency.init("Transparency", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_specular_map.Apply();
		m_edge_iris.Apply();
		m_env_map_detail.Apply();
		m_stretch_range_scale.Apply();
		m_stretch_range_offset.Apply();
		m_transparency.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/earth/earth.fp", 692);

	arl::ShaderParameter<arl::TextureSource> m_specular_map;
	arl::ShaderParameter<arl::TextureSource> m_edge_iris;
	arl::ShaderParameter<arl::TextureSource> m_env_map_detail;
	arl::ShaderParameter<math::v4> m_stretch_range_scale;
	arl::ShaderParameter<math::v4> m_stretch_range_offset;
	arl::ShaderParameter<float> m_transparency;
};

/* the globe's shader pair (0x418 bytes) */
class EarthShaders {
public:
	arl::ShaderInstance<EarthVertShader, arl::VertShaderObject> m_vert;
	arl::ShaderInstance<EarthFragShader, arl::FragShaderObject> m_frag;
};

/* "Earth" parameter set (0x168 bytes) */
class EarthParamSet : public ParamSet, public ParamLink {
public:
	class Values {
	public:
		Values(ParamSet *set)
			: m_edge_iris_fresnel_front(set, "edge_iris_fresnel_front", 0.0f, 0.0f,
						    1.0f, 0),
			  m_edge_iris_fresnel_control(set, "edge_iris_fresnel_control", 0.2f, 0.0f,
						      0.999f, PARAM_FLAG_FINE),
			  m_edge_iris_fresnel_coeff(set, "edge_iris_fresnel_coeff", 1.0f, 0.0f,
						    4.0f, PARAM_FLAG_FINE),
			  m_edge_iris_start(set, "edge_iris_start", 0.3f, 0.0f, 1.0f,
					    PARAM_FLAG_FINE),
			  m_radius(set, "radius", 0.35f, 0.0f, 1.0f, PARAM_FLAG_FINE),
			  m_translation(set, "translation", math::v3(0.0f), PARAM_FLAG_RAW),
			  m_rotation(set, "rotation", math::quaternion::Identity(), PARAM_FLAG_RAW),
			  m_inertial_w_mul(set, "inertial_w_mul", 0.8f, 0.0f, 2.0f,
					   PARAM_FLAG_FINE),
			  m_inertia_damp(set, "inertia_damp", 0.97f, 0.0f, 1.0f, 0),
			  m_inertia_thresh(set, "inertia_thresh", 1.0f, 0.0f, 10.0f, 0),
			  m_alpha(set, "alpha", 1.0f, 0.0f, 1.0f, 0),
			  m_env_fade_start(set, "env_fade_start", 0.8f, 0.0f, 1.0f, 0),
			  m_env_fade_end(set, "env_fade_end", 1.0f, 0.0f, 2.0f, 0),
			  m_diffuse_ground(set, "diffuse_ground", 0.58f, 0.0f, 1.0f, 0),
			  m_diffuse_sea(set, "diffuse_sea", 0.39f, 0.0f, 1.0f, 0),
			  m_specular_ground(set, "specular_ground", 0.58f, 0.0f, 1.0f, 0),
			  m_specular_sea(set, "specular_sea", 0.39f, 0.0f, 1.0f, 0),
			  m_alpha_ground(set, "alpha_ground", 0.58f, 0.0f, 1.0f, 0),
			  m_alpha_sea(set, "alpha_sea", 0.39f, 0.0f, 1.0f, 0),
			  m_ambiant_ground(set, "ambiant_ground", 0.58f, 0.0f, 1.0f, 0),
			  m_ambiant_sea(set, "ambiant_sea", 0.39f, 0.0f, 1.0f, 0)
		{
		}

		Param<float> m_edge_iris_fresnel_front;
		Param<float> m_edge_iris_fresnel_control;
		Param<float> m_edge_iris_fresnel_coeff;
		Param<float> m_edge_iris_start;
		Param<float> m_radius;
		Param<math::v3> m_translation;
		Param<math::quaternion> m_rotation;
		Param<float> m_inertial_w_mul;
		Param<float> m_inertia_damp;
		Param<float> m_inertia_thresh;
		Param<float> m_alpha;
		Param<float> m_env_fade_start;
		Param<float> m_env_fade_end;
		Param<float> m_diffuse_ground;
		Param<float> m_diffuse_sea;
		Param<float> m_specular_ground;
		Param<float> m_specular_sea;
		Param<float> m_alpha_ground;
		Param<float> m_alpha_sea;
		Param<float> m_ambiant_ground;
		Param<float> m_ambiant_sea;
	};

	WAVE_PARAM_SET_KEYED(EarthParamSet, "Earth", __attribute__((noinline)));
};

/* EarthRenderer (0x918 bytes) */
class EarthRenderer {
public:
	enum BlendMode {
		BLEND_OPAQUE = 0,
		BLEND_ADDITIVE = 1,
		BLEND_ALPHA = 2
	};

	EarthRenderer();
	~EarthRenderer();

	void Init(common::SharedPtr<Camera> camera, common::SharedPtr<Background> background,
		  common::SharedPtr<DrawEnv> draw_env, common::SharedPtr<TextureOps> texture_ops);
	bool Load(const string &dir);
	void Update(float frame_time);
	void Draw();
	void UpdateTrackball();

	EarthShaders *m_shaders;
	common::SharedPtr<Camera> m_camera;
	common::SharedPtr<Background> m_background;
	common::SharedPtr<DrawEnv> m_draw_env;
	common::SharedPtr<TextureOps> m_texture_ops;
	common::SharedPtr<arl::Texture2D> m_specular_map;
	common::SharedPtr<arl::Texture2D> m_edge_iris;
	WaveMesh<SkydomeVertex> m_mesh;
	bool m_loaded;
	bool m_wireframe;
	int32_t m_blend_mode;
	float m_mip_quality;
	bool m_debug_sphere;
	bool m_lock_y_axis;
	bool m_enable_input;
	bool m_transform_locked;
	float m_radius_scale;
	ParamSetKeysManager m_manager;
	ParamSetInstance<MaterialParamSet> m_material;
	ParamSetInstance<PointLightSphereParamSet> m_light;
	ParamSetInstance<EarthParamSet> m_earth;
	int32_t m_frame;
	math::quaternion m_prev_rotation;
	math::quaternion m_delta_rotation;
	math::quaternion m_rotation;
	math::quaternion m_inertia;
	math::v4 m_drag_start_point;
	math::v4 m_last_hit_point;
	math::v4 m_drag_point;
	bool m_dragging;
	math::quaternion m_drag_start_rotation;
	float m_alpha_scale;
	Transform m_transform;
	DrawLayer *m_rs_earth;
};

}
}

#endif
