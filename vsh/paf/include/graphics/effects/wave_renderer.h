/*
 * What the wave renderer draws with: its shaders' interfaces (paf/cg/drawobj/wave/*.vp and .fp,
 * their parameters Sony's names), the grid its waves and sky are drawn as, the sky dome, and a
 * wave instance: one FFT surface drawn over a grid with its own parameters. The shaders' file
 * names and parameters are Sony's; the classes', the members' and this file's names are ours.
 */
#ifndef PAF_GRAPHICS_EFFECTS_WAVE_RENDERER_H
#define PAF_GRAPHICS_EFFECTS_WAVE_RENDERER_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_framebuffer_copy.h>
#include <graphics/arl/gxm/gxm_shader.h>
#include <graphics/arl/gxm/gxm_texture.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <graphics/effects/fft_wave_surface.h>
#include <graphics/effects/param.h>
#include <graphics/effects/wave_param.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>

namespace paf {
namespace graph {

/* a shader interface's name, binary and its size, as each generated interface has them */
#define WAVE_SHADER_BINARY(path, size)						\
	virtual const char *GetName()							\
	{										\
		return path;								\
	}										\
											\
	virtual const uint8_t *GetBinary()						\
	{										\
		return s_binary;							\
	}										\
											\
	virtual uint32_t GetBinarySize()						\
	{										\
		return sizeof(s_binary);						\
	}										\
											\
	/* built by psp2cgc */								\
	static const uint8_t s_binary[size]

/* the waves' vertices, moved by the heights in the position texture */
class WaveVertShader : public arl::ShaderInterfaceBase {
public:
	WaveVertShader()
		: m_vp(m_setup), m_model_matrix(m_setup), m_pos_tex(m_setup), m_half_pixel(m_setup),
		  m_world_scale_hack(m_setup)
	{
	}

	virtual void Init()
	{
		m_vp.init("VP", arl::ShaderParameterBase::UNIFORM, 0);
		m_model_matrix.init("ModelMatrix", arl::ShaderParameterBase::UNIFORM, 0);
		m_pos_tex.init("PosTex", 6, 0);
		m_half_pixel.init("HalfPixel", arl::ShaderParameterBase::UNIFORM, 0);
		m_world_scale_hack.init("WorldScaleHack", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_vp.Apply();
		m_model_matrix.Apply();
		m_pos_tex.Apply();
		m_half_pixel.Apply();
		m_world_scale_hack.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/wave/wave.vp", 0x238);

	arl::ShaderParameter<math::matrix> m_vp;
	arl::ShaderParameter<math::matrix> m_model_matrix;
	arl::ShaderParameter<arl::TextureSource> m_pos_tex;
	arl::ShaderParameter<math::v2> m_half_pixel;
	arl::ShaderParameter<float> m_world_scale_hack;
};

/* the waves' colour, from the lit texture */
class WaveFragShader : public arl::ShaderInterfaceBase {
public:
	WaveFragShader() : m_lit_tex(m_setup)
	{
	}

	virtual void Init()
	{
		m_lit_tex.init("LitTex", 1, 0);
	}

	virtual void Apply()
	{
		m_lit_tex.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/wave/wave.fp", 0x104);

	arl::ShaderParameter<arl::TextureSource> m_lit_tex;
};

/* the waves' positions drawn into a texture */
class WavePosVertShader : public arl::ShaderInterfaceBase {
public:
	WavePosVertShader() : m_mvp(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("MVP", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/wave/wave_pos.vp", 0x134);

	arl::ShaderParameter<math::matrix> m_mvp;
};

/* the positions from the height map, extruded along the cross section */
class WavePosFragShader : public arl::ShaderInterfaceBase {
public:
	WavePosFragShader()
		: m_height_map(m_setup), m_distortion_scale(m_setup),
		  m_distortion_uv_scale_rotate(m_setup), m_cross_section(m_setup),
		  m_cross_section_scale(m_setup), m_vm(m_setup), m_persp_hack(m_setup)
	{
	}

	virtual void Init()
	{
		m_height_map.init("HeightMap", 7, 0);
		m_distortion_scale.init("DistortionScale", arl::ShaderParameterBase::UNIFORM, 0);
		m_distortion_uv_scale_rotate.init("DistortionUVScaleRotate",
						  arl::ShaderParameterBase::UNIFORM, 0);
		m_cross_section.init("extrude_args.CrossSection", arl::ShaderParameterBase::UNIFORM,
				     0);
		m_cross_section_scale.init("extrude_args.CrossSectionScale",
					   arl::ShaderParameterBase::UNIFORM, 0);
		m_vm.init("extrude_args.VM", arl::ShaderParameterBase::UNIFORM, 0);
		m_persp_hack.init("extrude_args.PerspHack", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_height_map.Apply();
		m_distortion_scale.Apply();
		m_distortion_uv_scale_rotate.Apply();
		m_cross_section.Apply();
		m_cross_section_scale.Apply();
		m_vm.Apply();
		m_persp_hack.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/wave/wave_pos.fp", 0x4b0);

	arl::ShaderParameter<arl::TextureSource> m_height_map;
	arl::ShaderParameter<float> m_distortion_scale;
	arl::ShaderParameter<math::v2> m_distortion_uv_scale_rotate;
	arl::ShaderParameter<math::v4> m_cross_section;
	arl::ShaderParameter<math::v3> m_cross_section_scale;
	arl::ShaderParameter<math::matrix> m_vm;
	arl::ShaderParameter<math::v4> m_persp_hack;
};

/* the waves lit, from their position and normal textures */
class WaveLitTexFragShader : public arl::ShaderInterfaceBase {
public:
	WaveLitTexFragShader()
		: m_model_matrix(m_setup), m_pos_tex(m_setup), m_nor_tex(m_setup),
		  m_half_pixel(m_setup), m_eye_pos(m_setup), m_material_pack(m_setup),
		  m_point_light_pack2(m_setup), m_point_light_pack1(m_setup), m_edge_color(m_setup),
		  m_center_color(m_setup), m_fog_pack(m_setup), m_edge_fade(m_setup)
	{
	}

	virtual void Init()
	{
		m_model_matrix.init("ModelMatrix", arl::ShaderParameterBase::UNIFORM, 0);
		m_pos_tex.init("PosTex", 3, 0);
		m_nor_tex.init("NorTex", 4, 0);
		m_half_pixel.init("HalfPixel", arl::ShaderParameterBase::UNIFORM, 0);
		m_eye_pos.init("EyePos", arl::ShaderParameterBase::UNIFORM, 0);
		m_material_pack.init("MaterialPack", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack2.init("PointLightPack2", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack1.init("PointLightPack1", arl::ShaderParameterBase::UNIFORM, 0);
		m_edge_color.init("EdgeColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_center_color.init("CenterColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_fog_pack.init("FogPack", arl::ShaderParameterBase::UNIFORM, 0);
		m_edge_fade.init("EdgeFade", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_model_matrix.Apply();
		m_pos_tex.Apply();
		m_nor_tex.Apply();
		m_half_pixel.Apply();
		m_eye_pos.Apply();
		m_material_pack.Apply();
		m_point_light_pack2.Apply();
		m_point_light_pack1.Apply();
		m_edge_color.Apply();
		m_center_color.Apply();
		m_fog_pack.Apply();
		m_edge_fade.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/wave/wave_lit_tex.fp", 0x58c);

	arl::ShaderParameter<math::matrix> m_model_matrix;
	arl::ShaderParameter<arl::TextureSource> m_pos_tex;
	arl::ShaderParameter<arl::TextureSource> m_nor_tex;
	arl::ShaderParameter<math::v2> m_half_pixel;
	arl::ShaderParameter<math::v3> m_eye_pos;
	arl::ShaderParameter<math::matrix> m_material_pack;
	arl::ShaderParameter<math::matrix> m_point_light_pack2;
	arl::ShaderParameter<math::matrix> m_point_light_pack1;
	arl::ShaderParameter<math::v4> m_edge_color;
	arl::ShaderParameter<math::v4> m_center_color;
	arl::ShaderParameter<math::matrix> m_fog_pack;
	arl::ShaderParameter<math::v2> m_edge_fade;
};

/* the sky dome's vertices and its colours, from the sun and the waves' normals */
class SkydomeVertShader : public arl::ShaderInterfaceBase {
public:
	SkydomeVertShader()
		: m_vp(m_setup), m_m(m_setup), m_world_scale_hack(m_setup), m_horizon_angle(m_setup),
		  m_eye_pos(m_setup), m_sun_pos(m_setup), m_wave_fog_color(m_setup),
		  m_sun_color(m_setup), m_sun_power(m_setup), m_sun_control(m_setup),
		  m_zenith_color(m_setup), m_horizon_color(m_setup), m_color_blend_range(m_setup),
		  m_wave_nor_tex(m_setup), m_distortion_scale(m_setup), m_wave_model_matrix(m_setup)
	{
	}

	virtual void Init()
	{
		m_vp.init("VP", arl::ShaderParameterBase::UNIFORM, 0);
		m_m.init("M", arl::ShaderParameterBase::UNIFORM, 0);
		m_world_scale_hack.init("WorldScaleHack", arl::ShaderParameterBase::UNIFORM, 0);
		m_horizon_angle.init("HorizonAngle", arl::ShaderParameterBase::UNIFORM, 0);
		m_eye_pos.init("EyePos", arl::ShaderParameterBase::UNIFORM, 0);
		m_sun_pos.init("SunPos", arl::ShaderParameterBase::UNIFORM, 0);
		m_wave_fog_color.init("WaveFogColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_sun_color.init("SunColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_sun_power.init("SunPower", arl::ShaderParameterBase::UNIFORM, 0);
		m_sun_control.init("SunControl", arl::ShaderParameterBase::UNIFORM, 0);
		m_zenith_color.init("ZenithColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_horizon_color.init("HorizonColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_color_blend_range.init("ColorBlendRange", arl::ShaderParameterBase::UNIFORM, 0);
		m_wave_nor_tex.init("WaveNorTex", 15, 0);
		m_distortion_scale.init("DistortionScale", arl::ShaderParameterBase::UNIFORM, 0);
		m_wave_model_matrix.init("WaveModelMatrix", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_vp.Apply();
		m_m.Apply();
		m_world_scale_hack.Apply();
		m_horizon_angle.Apply();
		m_eye_pos.Apply();
		m_sun_pos.Apply();
		m_wave_fog_color.Apply();
		m_sun_color.Apply();
		m_sun_power.Apply();
		m_sun_control.Apply();
		m_zenith_color.Apply();
		m_horizon_color.Apply();
		m_color_blend_range.Apply();
		m_wave_nor_tex.Apply();
		m_distortion_scale.Apply();
		m_wave_model_matrix.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/wave/skydome.vp", 0x70c);

	arl::ShaderParameter<math::matrix> m_vp;
	arl::ShaderParameter<math::matrix> m_m;
	arl::ShaderParameter<float> m_world_scale_hack;
	arl::ShaderParameter<math::v4> m_horizon_angle;
	arl::ShaderParameter<math::v3> m_eye_pos;
	arl::ShaderParameter<math::v3> m_sun_pos;
	arl::ShaderParameter<math::v4> m_wave_fog_color;
	arl::ShaderParameter<math::v3> m_sun_color;
	arl::ShaderParameter<float> m_sun_power;
	arl::ShaderParameter<float> m_sun_control;
	arl::ShaderParameter<math::v3> m_zenith_color;
	arl::ShaderParameter<math::v3> m_horizon_color;
	arl::ShaderParameter<math::v2> m_color_blend_range;
	arl::ShaderParameter<arl::TextureSource> m_wave_nor_tex;
	arl::ShaderParameter<math::v3> m_distortion_scale;
	arl::ShaderParameter<math::matrix> m_wave_model_matrix;
};

/* the sky dome's fragments: the colour its vertices give */
class SkydomeFragShader : public arl::ShaderInterfaceBase {
public:
	virtual void Init()
	{
	}

	virtual void Apply()
	{
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/wave/skydome.fp", 0xcc);
};

#undef WAVE_SHADER_BINARY

/*
 * A grid of width by height vertices, drawn as one triangle strip with an index array of 16-bit
 * indices, or 32-bit ones past 65536 vertices (names ours); 0x40 bytes.
 */
class WaveMesh {
public:
	WaveMesh() : m_index_count(0), m_width(0), m_height(0), m_strip_count(0),
		     m_primitive(arl::TRIANGLE_STRIP), m_first(0)
	{
	}

	/* 0x81202718: the grid's vertices in a buffer of the allocator laid out by the spec */
	WaveMesh(int32_t width, int32_t height, common::SharedPtr<arl::VertexArraySpec> spec,
		 common::SharedPtr<arl::VramAllocator> allocator);

	uint32_t m_index_count;
	int32_t m_width;
	int32_t m_height;
	uint32_t m_strip_count;
	common::SharedPtr<arl::Texture2D> m_texture;
	uint32_t m_unused;
	uint32_t m_first;
	arl::Primitive m_primitive;
	arl::VertexArray m_vertices;
	arl::IndexArray m_indices;
};

/* the sky over the waves (names ours); 0x438 bytes */
class Skydome {
public:
	/* 0x811a6924 */
	Skydome();

	WaveMesh m_mesh;
	arl::ShaderInstance<SkydomeVertShader, arl::VertShaderObject> m_vert;
	arl::ShaderInstance<SkydomeFragShader, arl::FragShaderObject> m_frag;
	bool m_dirty;
	common::SharedPtr<arl::Texture2D> m_texture;
	common::SharedPtr<arl::Texture2D> m_texture2;
	bool m_loaded;
};

/* a target the waves draw into; three of each kind (names ours) */
class WaveTarget {
public:
	WaveTarget() : m_width(0), m_height(0), m_format(0)
	{
	}

	int32_t m_width;
	int32_t m_height;
	uint32_t m_format;
	common::SharedPtr<arl::Texture2D> m_texture;
	common::SharedPtr<arl::Texture2D> m_depth;
};

class WaveRenderer;

/*
 * One surface of waves: its FFT heights, the grid they move, the textures it draws its
 * positions and normals into, and its own parameters (names ours); 0x2a8 bytes.
 */
class WaveInstance : public ParamLink {
public:
	/* 0x810a0ba6: an FFT surface of size, over a grid of resolution by resolution */
	WaveInstance(WaveRenderer *renderer, int32_t size, int32_t resolution);
	/* 0x810a18c8 */
	~WaveInstance();

	WaveMesh m_mesh;
	FFTWaveSurface m_surface;
	WaveRenderer *m_renderer;
	common::SharedPtr<arl::Texture2D> m_textures[4];
	WaveInstanceParamSet::Values m_values;
	float m_time;
	float m_scale;
	float m_reserved[2];
};

class Background;
class Camera;
class DrawEnv;
class DrawLayer;
class PostEffects;
class MotionListener;

/* the renderer of the waves (the name its constructor 0x810a54c2 gives it); 0x2090 bytes */
class WaveRenderer {
public:
	/* 0x810a54c2 */
	WaveRenderer();

	/* emitted with the renderer's code (0x811fe8e8) */
	~WaveRenderer()
	{
		while (m_instances.m_next != &m_instances)
			delete static_cast<WaveInstance *>(m_instances.m_next);
	}

	/* 0x810a5ee2 */
	void Init(common::SharedPtr<Camera> camera, common::SharedPtr<Background> background,
		  common::SharedPtr<DrawEnv> draw_env, common::SharedPtr<PostEffects> post_effects,
		  common::SharedPtr<MotionListener> listener);
	/* 0x810a61c6 */
	void Load(const string &dir, int32_t mode);
	/* 0x810a76a0: the world of the 2D camera tilted with the waves, from the one it was reset
	   to */
	math::matrix GetTiltedWorld(const math::matrix &reset_world);

	float m_wave_scale;
	float m_wave_speed;
	bool m_flag8;
	/* the instances' surfaces stand still */
	bool m_paused;
	bool m_flag10;
	/* handed to the surfaces' updates, which do not read it */
	bool m_update_flag;
	bool m_flag12;
	bool m_flag13;
	int32_t m_value16;
	bool m_flag20;
	/* the frame of the last update */
	int32_t m_frame;
	common::SharedPtr<MotionListener> m_listener;
	common::SharedPtr<PostEffects> m_post_effects;
	/* the lit texture's size, times the viewport's */
	float m_lit_scale;
	bool m_lit_made;
	common::SharedPtr<arl::Texture2D> m_lit_texture;
	WaveTarget m_wave_targets[3];
	uint32_t m_wave_target_index;
	arl::ShaderInstance<WaveVertShader, arl::VertShaderObject> m_wave_vert;
	arl::ShaderInstance<WaveFragShader, arl::FragShaderObject> m_wave_frag;
	WaveTarget m_pos_targets[3];
	uint32_t m_pos_target_index;
	arl::ShaderInstance<WavePosVertShader, arl::VertShaderObject> m_pos_vert;
	arl::ShaderInstance<WavePosFragShader, arl::FragShaderObject> m_pos_frag;
	WaveTarget m_lit_targets[3];
	uint32_t m_lit_target_index;
	arl::ShaderInstance<WaveLitTexFragShader, arl::FragShaderObject> m_lit_frag;
	ParamList m_instances;
	Skydome m_skydome;
	ParamSetKeysManager m_param_keys;
	ParamSetInstance<MaterialParamSet> m_material;
	ParamSetInstance<PointLightSphereParamSet> m_point_light;
	ParamSetInstance<FogParamSet> m_fog;
	ParamSetInstance<FogParamSet> m_fog_lookdown;
	ParamSetInstance<SkyParamSet> m_sky;
	ParamSetInstance<FFTWaveParamSet> m_fft_wave;
	ParamSetInstance<WaveInstanceParamSet> m_wave_instance;
	ParamSetInstance<WaveRendererParamSet> m_params;
	ParamSetKeysManager m_transition_keys;
	ParamSetInstance<WaveTransitionParamSet> m_transition;
	common::SharedPtr<Background> m_background;
	common::SharedPtr<Camera> m_camera;
	common::SharedPtr<DrawEnv> m_draw_env;
	common::SharedPtr<Camera> m_camera2;
	uint32_t m_spare[2];
	/* counted up by every update */
	int32_t m_update_count;
	float m_half;
	DrawLayer *m_rs_wave;
	DrawLayer *m_rs_wave_debug;
	/* the 2D camera follows the waves */
	bool m_tilt;
	int32_t m_value8316;
	bool m_flag8320;
	/* what the custom renderer is given each frame */
	float m_wave_x;
	float m_wave_y;
	uint32_t m_spare2;
};

}
}

#endif
