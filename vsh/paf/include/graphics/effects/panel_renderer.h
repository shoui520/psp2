/*
 * The panel renderer and what it draws with: its shaders' interfaces (the .vp
 * and .fp, their parameters Sony's names), the meshes its panels are drawn as and its parameter
 * sets. PanelRenderer, the shaders' file names, the parameters' and the sets' names
 * ("TouchWave", "Shadow") are Sony's strings; the other classes', the members' and this file's
 * names are ours.
 */
#ifndef PAF_GRAPHICS_EFFECTS_PANEL_RENDERER_H
#define PAF_GRAPHICS_EFFECTS_PANEL_RENDERER_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/camera.h>
#include <graphics/drawobj/bubble_obj.h>
#include <graphics/drawobj/panel_obj.h>
#include <graphics/drawobj/draw_env.h>
#include <graphics/effects/fft_wave_surface.h>
#include <graphics/effects/wave_renderer.h>
#include <math/ngp/matrix.h>
#include <math/ngp/quaternion.h>
#include <math/ngp/v4.h>

namespace paf {
namespace ui {
class Screen;
}

namespace graph {

/* the parameters the panel renderer sets on a panel's vertex shader, which keeps those it
   has; no shader has the hit point (where the touch meets the panel, which DrawPanel sets)
   or the Reserved ones, which nothing sets (names ours) */
class PanelVertShaderParams {
public:
	virtual ~PanelVertShaderParams()
	{
	}

	virtual void SetMVP(const math::matrix &value)
	{
	}

	virtual void SetSize(const math::v4 &value)
	{
	}

	virtual void SetTransform(const math::matrix &value)
	{
	}

	virtual void SetWorldScaleHack(const float &value)
	{
	}

	virtual void SetMaterialPack(const math::matrix &value)
	{
	}

	virtual void SetPointLightPack2(const math::matrix &value)
	{
	}

	virtual void SetPointLightPack1(const math::matrix &value)
	{
	}

	virtual void SetBackgroundSize(const math::v4 &value)
	{
	}

	virtual void SetBackgroundRatio(const math::v2 &value)
	{
	}

	virtual void SetView(const math::matrix &value)
	{
	}

	virtual void SetEyePos(const math::v4 &value)
	{
	}

	virtual void SetHitPoint(const math::v4 &value)
	{
	}

	virtual void SetReserved1(const math::v4 &value)
	{
	}

	virtual void SetReserved2(const math::v4 &value)
	{
	}

	virtual void SetSpecularFactor(const math::v4 &value)
	{
	}

	virtual void SetColor(const math::v4 &value)
	{
	}

	virtual void SetWaveCenterColor(const math::v4 &value)
	{
	}

	virtual void SetWaveEdgeColor(const math::v4 &value)
	{
	}

	virtual void SetWaveFogPack(const math::matrix &value)
	{
	}

	virtual void SetBorderSize(const math::v3 &value)
	{
	}

	virtual void SetRoundness(const math::v2 &value)
	{
	}

	virtual void SetBorderGlow(const math::v4 &value)
	{
	}

	virtual void SetBorderPow(const math::v2 &value)
	{
	}
};

/* the parameters the panel renderer sets on a panel's fragment shader, which keeps those it
   has; no shader has the hit point (where the touch meets the panel, which DrawPanel sets)
   or the Reserved ones, which nothing sets (names ours) */
class PanelFragShaderParams {
public:
	virtual ~PanelFragShaderParams()
	{
	}

	virtual void SetMaterialPack(const math::matrix &value)
	{
	}

	virtual void SetPointLightPack2(const math::matrix &value)
	{
	}

	virtual void SetPointLightPack1(const math::matrix &value)
	{
	}

	virtual void SetBackgroundSize(const math::v4 &value)
	{
	}

	virtual void SetBackgroundRatio(const math::v2 &value)
	{
	}

	virtual void SetView(const math::matrix &value)
	{
	}

	virtual void SetEyePos(const math::v4 &value)
	{
	}

	virtual void SetHitPoint(const math::v4 &value)
	{
	}

	virtual void SetReserved1(const math::v4 &value)
	{
	}

	virtual void SetReserved2(const math::v4 &value)
	{
	}

	virtual void SetSpecularFactor(const math::v4 &value)
	{
	}

	virtual void SetColor(const math::v4 &value)
	{
	}

	virtual void SetWaveCenterColor(const math::v4 &value)
	{
	}

	virtual void SetWaveEdgeColor(const math::v4 &value)
	{
	}

	virtual void SetWaveFogPack(const math::matrix &value)
	{
	}

	virtual void SetColorMap(const arl::TextureSource &value)
	{
	}

	virtual void SetBackground(const arl::TextureSource &value)
	{
	}

	virtual void SetEnvMapDetail(const arl::TextureSource &value)
	{
	}

	virtual void SetGlowColor(const math::v4 &value)
	{
	}
};

/* the parameters the panel renderer sets on a bubble's vertex shader (names ours) */
class BubbleVertShaderParams {
public:
	virtual ~BubbleVertShaderParams()
	{
	}

	virtual void SetMVP(const math::matrix &value)
	{
	}

	virtual void SetHeight(const math::v2 &value)
	{
	}

	virtual void SetTransformInverseTranspose(const math::matrix &value)
	{
	}

	virtual void SetTransform(const math::matrix &value)
	{
	}

	virtual void SetMaterialPack(const math::matrix &value)
	{
	}

	virtual void SetLightPos(const math::v3 &value)
	{
	}

	virtual void SetDiffuseColor(const math::v4 &value)
	{
	}

	virtual void SetView(const math::matrix &value)
	{
	}

	virtual void SetEyePos(const math::v4 &value)
	{
	}

	virtual void SetBackColorZRange(const math::v2 &value)
	{
	}
};

/* the parameters the panel renderer sets on a bubble's fragment shader (names ours) */
class BubbleFragShaderParams {
public:
	virtual ~BubbleFragShaderParams()
	{
	}

	virtual void SetBackColor(const math::v4 &value)
	{
	}

	virtual void SetSpecularFactor(const math::v4 &value)
	{
	}

	virtual void SetColorMap(const arl::TextureSource &value)
	{
	}

	virtual void SetEnvMapDetail(const arl::TextureSource &value)
	{
	}

	virtual void SetAlpha(const float &value)
	{
	}
};

/* paf/cg/drawobj/panel/panel_glass_border.vp */
class PanelGlassBorderVertShader : public arl::ShaderInterfaceBase, public PanelVertShaderParams {
public:
	PanelGlassBorderVertShader()
		: m_mvp(m_setup), m_size(m_setup), m_transform(m_setup), m_world_scale_hack(m_setup), m_material_pack(m_setup), m_point_light_pack2(m_setup), m_point_light_pack1(m_setup), m_background_ratio(m_setup), m_view(m_setup), m_eye_pos(m_setup), m_border_size(m_setup), m_roundness(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("MVP", arl::ShaderParameterBase::UNIFORM, 0);
		m_size.init("Size", arl::ShaderParameterBase::UNIFORM, 0);
		m_transform.init("Transform", arl::ShaderParameterBase::UNIFORM, 0);
		m_world_scale_hack.init("WorldScaleHack", arl::ShaderParameterBase::UNIFORM, 0);
		m_material_pack.init("cargs.MaterialPack", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack2.init("cargs.PointLightPack2", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack1.init("cargs.PointLightPack1", arl::ShaderParameterBase::UNIFORM, 0);
		m_background_ratio.init("cargs.BackgroundRatio", arl::ShaderParameterBase::UNIFORM, 0);
		m_view.init("cargs.View", arl::ShaderParameterBase::UNIFORM, 0);
		m_eye_pos.init("cargs.EyePos", arl::ShaderParameterBase::UNIFORM, 0);
		m_border_size.init("BorderSize", arl::ShaderParameterBase::UNIFORM, 0);
		m_roundness.init("Roundness", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_size.Apply();
		m_transform.Apply();
		m_world_scale_hack.Apply();
		m_material_pack.Apply();
		m_point_light_pack2.Apply();
		m_point_light_pack1.Apply();
		m_background_ratio.Apply();
		m_view.Apply();
		m_eye_pos.Apply();
		m_border_size.Apply();
		m_roundness.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/panel/panel_glass_border.vp", 0x784);

	virtual void SetMVP(const math::matrix &value)
	{
		m_mvp.Set(value);
	}

	virtual void SetSize(const math::v4 &value)
	{
		m_size.Set(value);
	}

	virtual void SetTransform(const math::matrix &value)
	{
		m_transform.Set(value);
	}

	virtual void SetWorldScaleHack(const float &value)
	{
		m_world_scale_hack.Set(value);
	}

	virtual void SetMaterialPack(const math::matrix &value)
	{
		m_material_pack.Set(value);
	}

	virtual void SetPointLightPack2(const math::matrix &value)
	{
		m_point_light_pack2.Set(value);
	}

	virtual void SetPointLightPack1(const math::matrix &value)
	{
		m_point_light_pack1.Set(value);
	}

	virtual void SetBackgroundSize(const math::v4 &value)
	{
	}

	virtual void SetBackgroundRatio(const math::v2 &value)
	{
		m_background_ratio.Set(value);
	}

	virtual void SetView(const math::matrix &value)
	{
		m_view.Set(value);
	}

	virtual void SetEyePos(const math::v4 &value)
	{
		m_eye_pos.Set(value);
	}

	virtual void SetHitPoint(const math::v4 &value)
	{
	}

	virtual void SetReserved1(const math::v4 &value)
	{
	}

	virtual void SetReserved2(const math::v4 &value)
	{
	}

	virtual void SetSpecularFactor(const math::v4 &value)
	{
	}

	virtual void SetColor(const math::v4 &value)
	{
	}

	virtual void SetWaveCenterColor(const math::v4 &value)
	{
	}

	virtual void SetWaveEdgeColor(const math::v4 &value)
	{
	}

	virtual void SetWaveFogPack(const math::matrix &value)
	{
	}

	virtual void SetBorderSize(const math::v3 &value)
	{
		m_border_size.Set(value);
	}

	virtual void SetRoundness(const math::v2 &value)
	{
		m_roundness.Set(value);
	}

	virtual void SetBorderGlow(const math::v4 &value)
	{
	}

	virtual void SetBorderPow(const math::v2 &value)
	{
	}

	arl::ShaderParameter<math::matrix> m_mvp;
	arl::ShaderParameter<math::v4> m_size;
	arl::ShaderParameter<math::matrix> m_transform;
	arl::ShaderParameter<float> m_world_scale_hack;
	arl::ShaderParameter<math::matrix> m_material_pack;
	arl::ShaderParameter<math::matrix> m_point_light_pack2;
	arl::ShaderParameter<math::matrix> m_point_light_pack1;
	arl::ShaderParameter<math::v2> m_background_ratio;
	arl::ShaderParameter<math::matrix> m_view;
	arl::ShaderParameter<math::v4> m_eye_pos;
	arl::ShaderParameter<math::v3> m_border_size;
	arl::ShaderParameter<math::v2> m_roundness;
};

/* paf/cg/drawobj/panel/panel_glass_top.vp */
class PanelGlassTopVertShader : public arl::ShaderInterfaceBase, public PanelVertShaderParams {
public:
	PanelGlassTopVertShader()
		: m_mvp(m_setup), m_size(m_setup), m_transform(m_setup), m_world_scale_hack(m_setup), m_material_pack(m_setup), m_point_light_pack2(m_setup), m_point_light_pack1(m_setup), m_background_ratio(m_setup), m_view(m_setup), m_eye_pos(m_setup), m_border_size(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("MVP", arl::ShaderParameterBase::UNIFORM, 0);
		m_size.init("Size", arl::ShaderParameterBase::UNIFORM, 0);
		m_transform.init("Transform", arl::ShaderParameterBase::UNIFORM, 0);
		m_world_scale_hack.init("WorldScaleHack", arl::ShaderParameterBase::UNIFORM, 0);
		m_material_pack.init("cargs.MaterialPack", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack2.init("cargs.PointLightPack2", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack1.init("cargs.PointLightPack1", arl::ShaderParameterBase::UNIFORM, 0);
		m_background_ratio.init("cargs.BackgroundRatio", arl::ShaderParameterBase::UNIFORM, 0);
		m_view.init("cargs.View", arl::ShaderParameterBase::UNIFORM, 0);
		m_eye_pos.init("cargs.EyePos", arl::ShaderParameterBase::UNIFORM, 0);
		m_border_size.init("BorderSize", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_size.Apply();
		m_transform.Apply();
		m_world_scale_hack.Apply();
		m_material_pack.Apply();
		m_point_light_pack2.Apply();
		m_point_light_pack1.Apply();
		m_background_ratio.Apply();
		m_view.Apply();
		m_eye_pos.Apply();
		m_border_size.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/panel/panel_glass_top.vp", 0x61c);

	virtual void SetMVP(const math::matrix &value)
	{
		m_mvp.Set(value);
	}

	virtual void SetSize(const math::v4 &value)
	{
		m_size.Set(value);
	}

	virtual void SetTransform(const math::matrix &value)
	{
		m_transform.Set(value);
	}

	virtual void SetWorldScaleHack(const float &value)
	{
		m_world_scale_hack.Set(value);
	}

	virtual void SetMaterialPack(const math::matrix &value)
	{
		m_material_pack.Set(value);
	}

	virtual void SetPointLightPack2(const math::matrix &value)
	{
		m_point_light_pack2.Set(value);
	}

	virtual void SetPointLightPack1(const math::matrix &value)
	{
		m_point_light_pack1.Set(value);
	}

	virtual void SetBackgroundSize(const math::v4 &value)
	{
	}

	virtual void SetBackgroundRatio(const math::v2 &value)
	{
		m_background_ratio.Set(value);
	}

	virtual void SetView(const math::matrix &value)
	{
		m_view.Set(value);
	}

	virtual void SetEyePos(const math::v4 &value)
	{
		m_eye_pos.Set(value);
	}

	virtual void SetHitPoint(const math::v4 &value)
	{
	}

	virtual void SetReserved1(const math::v4 &value)
	{
	}

	virtual void SetReserved2(const math::v4 &value)
	{
	}

	virtual void SetSpecularFactor(const math::v4 &value)
	{
	}

	virtual void SetColor(const math::v4 &value)
	{
	}

	virtual void SetWaveCenterColor(const math::v4 &value)
	{
	}

	virtual void SetWaveEdgeColor(const math::v4 &value)
	{
	}

	virtual void SetWaveFogPack(const math::matrix &value)
	{
	}

	virtual void SetBorderSize(const math::v3 &value)
	{
		m_border_size.Set(value);
	}

	virtual void SetRoundness(const math::v2 &value)
	{
	}

	virtual void SetBorderGlow(const math::v4 &value)
	{
	}

	virtual void SetBorderPow(const math::v2 &value)
	{
	}

	arl::ShaderParameter<math::matrix> m_mvp;
	arl::ShaderParameter<math::v4> m_size;
	arl::ShaderParameter<math::matrix> m_transform;
	arl::ShaderParameter<float> m_world_scale_hack;
	arl::ShaderParameter<math::matrix> m_material_pack;
	arl::ShaderParameter<math::matrix> m_point_light_pack2;
	arl::ShaderParameter<math::matrix> m_point_light_pack1;
	arl::ShaderParameter<math::v2> m_background_ratio;
	arl::ShaderParameter<math::matrix> m_view;
	arl::ShaderParameter<math::v4> m_eye_pos;
	arl::ShaderParameter<math::v3> m_border_size;
};

/* paf/cg/drawobj/panel/panel_glass.fp */
class PanelGlassFragShader : public arl::ShaderInterfaceBase, public PanelFragShaderParams {
public:
	PanelGlassFragShader()
		: m_specular_factor(m_setup), m_color(m_setup), m_background(m_setup), m_env_map_detail(m_setup)
	{
	}

	virtual void Init()
	{
		m_specular_factor.init("cargs.SpecularFactor", arl::ShaderParameterBase::UNIFORM, 0);
		m_color.init("cargs.Color", arl::ShaderParameterBase::UNIFORM, 0);
		m_background.init("cargs.Background", 0, 0);
		m_env_map_detail.init("cargs.EnvMapDetail", 1, 0);
	}

	virtual void Apply()
	{
		m_specular_factor.Apply();
		m_color.Apply();
		m_background.Apply();
		m_env_map_detail.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/panel/panel_glass.fp", 0x20c);

	virtual void SetMaterialPack(const math::matrix &value)
	{
	}

	virtual void SetPointLightPack2(const math::matrix &value)
	{
	}

	virtual void SetPointLightPack1(const math::matrix &value)
	{
	}

	virtual void SetBackgroundSize(const math::v4 &value)
	{
	}

	virtual void SetBackgroundRatio(const math::v2 &value)
	{
	}

	virtual void SetView(const math::matrix &value)
	{
	}

	virtual void SetEyePos(const math::v4 &value)
	{
	}

	virtual void SetHitPoint(const math::v4 &value)
	{
	}

	virtual void SetReserved1(const math::v4 &value)
	{
	}

	virtual void SetReserved2(const math::v4 &value)
	{
	}

	virtual void SetSpecularFactor(const math::v4 &value)
	{
		m_specular_factor.Set(value);
	}

	virtual void SetColor(const math::v4 &value)
	{
		m_color.Set(value);
	}

	virtual void SetWaveCenterColor(const math::v4 &value)
	{
	}

	virtual void SetWaveEdgeColor(const math::v4 &value)
	{
	}

	virtual void SetWaveFogPack(const math::matrix &value)
	{
	}

	virtual void SetColorMap(const arl::TextureSource &value)
	{
	}

	virtual void SetBackground(const arl::TextureSource &value)
	{
		m_background.Set(value);
	}

	virtual void SetEnvMapDetail(const arl::TextureSource &value)
	{
		m_env_map_detail.Set(value);
	}

	virtual void SetGlowColor(const math::v4 &value)
	{
	}

	arl::ShaderParameter<math::v4> m_specular_factor;
	arl::ShaderParameter<math::v4> m_color;
	arl::ShaderParameter<arl::TextureSource> m_background;
	arl::ShaderParameter<arl::TextureSource> m_env_map_detail;
};

/* paf/cg/drawobj/panel/panel_opaque_border.vp */
class PanelOpaqueBorderVertShader : public arl::ShaderInterfaceBase, public PanelVertShaderParams {
public:
	PanelOpaqueBorderVertShader()
		: m_mvp(m_setup), m_size(m_setup), m_transform(m_setup), m_world_scale_hack(m_setup), m_material_pack(m_setup), m_point_light_pack2(m_setup), m_point_light_pack1(m_setup), m_eye_pos(m_setup), m_border_size(m_setup), m_roundness(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("MVP", arl::ShaderParameterBase::UNIFORM, 0);
		m_size.init("Size", arl::ShaderParameterBase::UNIFORM, 0);
		m_transform.init("Transform", arl::ShaderParameterBase::UNIFORM, 0);
		m_world_scale_hack.init("WorldScaleHack", arl::ShaderParameterBase::UNIFORM, 0);
		m_material_pack.init("cargs.MaterialPack", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack2.init("cargs.PointLightPack2", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack1.init("cargs.PointLightPack1", arl::ShaderParameterBase::UNIFORM, 0);
		m_eye_pos.init("cargs.EyePos", arl::ShaderParameterBase::UNIFORM, 0);
		m_border_size.init("BorderSize", arl::ShaderParameterBase::UNIFORM, 0);
		m_roundness.init("Roundness", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_size.Apply();
		m_transform.Apply();
		m_world_scale_hack.Apply();
		m_material_pack.Apply();
		m_point_light_pack2.Apply();
		m_point_light_pack1.Apply();
		m_eye_pos.Apply();
		m_border_size.Apply();
		m_roundness.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/panel/panel_opaque_border.vp", 0x714);

	virtual void SetMVP(const math::matrix &value)
	{
		m_mvp.Set(value);
	}

	virtual void SetSize(const math::v4 &value)
	{
		m_size.Set(value);
	}

	virtual void SetTransform(const math::matrix &value)
	{
		m_transform.Set(value);
	}

	virtual void SetWorldScaleHack(const float &value)
	{
		m_world_scale_hack.Set(value);
	}

	virtual void SetMaterialPack(const math::matrix &value)
	{
		m_material_pack.Set(value);
	}

	virtual void SetPointLightPack2(const math::matrix &value)
	{
		m_point_light_pack2.Set(value);
	}

	virtual void SetPointLightPack1(const math::matrix &value)
	{
		m_point_light_pack1.Set(value);
	}

	virtual void SetBackgroundSize(const math::v4 &value)
	{
	}

	virtual void SetBackgroundRatio(const math::v2 &value)
	{
	}

	virtual void SetView(const math::matrix &value)
	{
	}

	virtual void SetEyePos(const math::v4 &value)
	{
		m_eye_pos.Set(value);
	}

	virtual void SetHitPoint(const math::v4 &value)
	{
	}

	virtual void SetReserved1(const math::v4 &value)
	{
	}

	virtual void SetReserved2(const math::v4 &value)
	{
	}

	virtual void SetSpecularFactor(const math::v4 &value)
	{
	}

	virtual void SetColor(const math::v4 &value)
	{
	}

	virtual void SetWaveCenterColor(const math::v4 &value)
	{
	}

	virtual void SetWaveEdgeColor(const math::v4 &value)
	{
	}

	virtual void SetWaveFogPack(const math::matrix &value)
	{
	}

	virtual void SetBorderSize(const math::v3 &value)
	{
		m_border_size.Set(value);
	}

	virtual void SetRoundness(const math::v2 &value)
	{
		m_roundness.Set(value);
	}

	virtual void SetBorderGlow(const math::v4 &value)
	{
	}

	virtual void SetBorderPow(const math::v2 &value)
	{
	}

	arl::ShaderParameter<math::matrix> m_mvp;
	arl::ShaderParameter<math::v4> m_size;
	arl::ShaderParameter<math::matrix> m_transform;
	arl::ShaderParameter<float> m_world_scale_hack;
	arl::ShaderParameter<math::matrix> m_material_pack;
	arl::ShaderParameter<math::matrix> m_point_light_pack2;
	arl::ShaderParameter<math::matrix> m_point_light_pack1;
	arl::ShaderParameter<math::v4> m_eye_pos;
	arl::ShaderParameter<math::v3> m_border_size;
	arl::ShaderParameter<math::v2> m_roundness;
};

/* paf/cg/drawobj/panel/panel_opaque_top.vp */
class PanelOpaqueTopVertShader : public arl::ShaderInterfaceBase, public PanelVertShaderParams {
public:
	PanelOpaqueTopVertShader()
		: m_mvp(m_setup), m_size(m_setup), m_transform(m_setup), m_world_scale_hack(m_setup), m_material_pack(m_setup), m_point_light_pack2(m_setup), m_point_light_pack1(m_setup), m_eye_pos(m_setup), m_border_size(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("MVP", arl::ShaderParameterBase::UNIFORM, 0);
		m_size.init("Size", arl::ShaderParameterBase::UNIFORM, 0);
		m_transform.init("Transform", arl::ShaderParameterBase::UNIFORM, 0);
		m_world_scale_hack.init("WorldScaleHack", arl::ShaderParameterBase::UNIFORM, 0);
		m_material_pack.init("cargs.MaterialPack", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack2.init("cargs.PointLightPack2", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack1.init("cargs.PointLightPack1", arl::ShaderParameterBase::UNIFORM, 0);
		m_eye_pos.init("cargs.EyePos", arl::ShaderParameterBase::UNIFORM, 0);
		m_border_size.init("BorderSize", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_size.Apply();
		m_transform.Apply();
		m_world_scale_hack.Apply();
		m_material_pack.Apply();
		m_point_light_pack2.Apply();
		m_point_light_pack1.Apply();
		m_eye_pos.Apply();
		m_border_size.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/panel/panel_opaque_top.vp", 0x5b4);

	virtual void SetMVP(const math::matrix &value)
	{
		m_mvp.Set(value);
	}

	virtual void SetSize(const math::v4 &value)
	{
		m_size.Set(value);
	}

	virtual void SetTransform(const math::matrix &value)
	{
		m_transform.Set(value);
	}

	virtual void SetWorldScaleHack(const float &value)
	{
		m_world_scale_hack.Set(value);
	}

	virtual void SetMaterialPack(const math::matrix &value)
	{
		m_material_pack.Set(value);
	}

	virtual void SetPointLightPack2(const math::matrix &value)
	{
		m_point_light_pack2.Set(value);
	}

	virtual void SetPointLightPack1(const math::matrix &value)
	{
		m_point_light_pack1.Set(value);
	}

	virtual void SetBackgroundSize(const math::v4 &value)
	{
	}

	virtual void SetBackgroundRatio(const math::v2 &value)
	{
	}

	virtual void SetView(const math::matrix &value)
	{
	}

	virtual void SetEyePos(const math::v4 &value)
	{
		m_eye_pos.Set(value);
	}

	virtual void SetHitPoint(const math::v4 &value)
	{
	}

	virtual void SetReserved1(const math::v4 &value)
	{
	}

	virtual void SetReserved2(const math::v4 &value)
	{
	}

	virtual void SetSpecularFactor(const math::v4 &value)
	{
	}

	virtual void SetColor(const math::v4 &value)
	{
	}

	virtual void SetWaveCenterColor(const math::v4 &value)
	{
	}

	virtual void SetWaveEdgeColor(const math::v4 &value)
	{
	}

	virtual void SetWaveFogPack(const math::matrix &value)
	{
	}

	virtual void SetBorderSize(const math::v3 &value)
	{
		m_border_size.Set(value);
	}

	virtual void SetRoundness(const math::v2 &value)
	{
	}

	virtual void SetBorderGlow(const math::v4 &value)
	{
	}

	virtual void SetBorderPow(const math::v2 &value)
	{
	}

	arl::ShaderParameter<math::matrix> m_mvp;
	arl::ShaderParameter<math::v4> m_size;
	arl::ShaderParameter<math::matrix> m_transform;
	arl::ShaderParameter<float> m_world_scale_hack;
	arl::ShaderParameter<math::matrix> m_material_pack;
	arl::ShaderParameter<math::matrix> m_point_light_pack2;
	arl::ShaderParameter<math::matrix> m_point_light_pack1;
	arl::ShaderParameter<math::v4> m_eye_pos;
	arl::ShaderParameter<math::v3> m_border_size;
};

/* paf/cg/drawobj/panel/panel_opaque.fp */
class PanelOpaqueFragShader : public arl::ShaderInterfaceBase, public PanelFragShaderParams {
public:
	PanelOpaqueFragShader()
		: m_specular_factor(m_setup), m_color(m_setup), m_color_map(m_setup), m_env_map_detail(m_setup)
	{
	}

	virtual void Init()
	{
		m_specular_factor.init("cargs.SpecularFactor", arl::ShaderParameterBase::UNIFORM, 0);
		m_color.init("cargs.Color", arl::ShaderParameterBase::UNIFORM, 0);
		m_color_map.init("cargs.ColorMap", 0, 0);
		m_env_map_detail.init("cargs.EnvMapDetail", 1, 0);
	}

	virtual void Apply()
	{
		m_specular_factor.Apply();
		m_color.Apply();
		m_color_map.Apply();
		m_env_map_detail.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/panel/panel_opaque.fp", 0x1b8);

	virtual void SetMaterialPack(const math::matrix &value)
	{
	}

	virtual void SetPointLightPack2(const math::matrix &value)
	{
	}

	virtual void SetPointLightPack1(const math::matrix &value)
	{
	}

	virtual void SetBackgroundSize(const math::v4 &value)
	{
	}

	virtual void SetBackgroundRatio(const math::v2 &value)
	{
	}

	virtual void SetView(const math::matrix &value)
	{
	}

	virtual void SetEyePos(const math::v4 &value)
	{
	}

	virtual void SetHitPoint(const math::v4 &value)
	{
	}

	virtual void SetReserved1(const math::v4 &value)
	{
	}

	virtual void SetReserved2(const math::v4 &value)
	{
	}

	virtual void SetSpecularFactor(const math::v4 &value)
	{
		m_specular_factor.Set(value);
	}

	virtual void SetColor(const math::v4 &value)
	{
		m_color.Set(value);
	}

	virtual void SetWaveCenterColor(const math::v4 &value)
	{
	}

	virtual void SetWaveEdgeColor(const math::v4 &value)
	{
	}

	virtual void SetWaveFogPack(const math::matrix &value)
	{
	}

	virtual void SetColorMap(const arl::TextureSource &value)
	{
		m_color_map.Set(value);
	}

	virtual void SetBackground(const arl::TextureSource &value)
	{
	}

	virtual void SetEnvMapDetail(const arl::TextureSource &value)
	{
		m_env_map_detail.Set(value);
	}

	virtual void SetGlowColor(const math::v4 &value)
	{
	}

	arl::ShaderParameter<math::v4> m_specular_factor;
	arl::ShaderParameter<math::v4> m_color;
	arl::ShaderParameter<arl::TextureSource> m_color_map;
	arl::ShaderParameter<arl::TextureSource> m_env_map_detail;
};

/* paf/cg/drawobj/panel/panel_glass_border_glowing.vp */
class PanelGlassBorderGlowingVertShader : public arl::ShaderInterfaceBase, public PanelVertShaderParams {
public:
	PanelGlassBorderGlowingVertShader()
		: m_mvp(m_setup), m_size(m_setup), m_transform(m_setup), m_world_scale_hack(m_setup), m_material_pack(m_setup), m_point_light_pack2(m_setup), m_point_light_pack1(m_setup), m_background_ratio(m_setup), m_view(m_setup), m_eye_pos(m_setup), m_border_size(m_setup), m_roundness(m_setup), m_border_glow(m_setup), m_border_pow(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("MVP", arl::ShaderParameterBase::UNIFORM, 0);
		m_size.init("Size", arl::ShaderParameterBase::UNIFORM, 0);
		m_transform.init("Transform", arl::ShaderParameterBase::UNIFORM, 0);
		m_world_scale_hack.init("WorldScaleHack", arl::ShaderParameterBase::UNIFORM, 0);
		m_material_pack.init("cargs.MaterialPack", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack2.init("cargs.PointLightPack2", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack1.init("cargs.PointLightPack1", arl::ShaderParameterBase::UNIFORM, 0);
		m_background_ratio.init("cargs.BackgroundRatio", arl::ShaderParameterBase::UNIFORM, 0);
		m_view.init("cargs.View", arl::ShaderParameterBase::UNIFORM, 0);
		m_eye_pos.init("cargs.EyePos", arl::ShaderParameterBase::UNIFORM, 0);
		m_border_size.init("BorderSize", arl::ShaderParameterBase::UNIFORM, 0);
		m_roundness.init("Roundness", arl::ShaderParameterBase::UNIFORM, 0);
		m_border_glow.init("BorderGlow", arl::ShaderParameterBase::UNIFORM, 0);
		m_border_pow.init("BorderPow", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_size.Apply();
		m_transform.Apply();
		m_world_scale_hack.Apply();
		m_material_pack.Apply();
		m_point_light_pack2.Apply();
		m_point_light_pack1.Apply();
		m_background_ratio.Apply();
		m_view.Apply();
		m_eye_pos.Apply();
		m_border_size.Apply();
		m_roundness.Apply();
		m_border_glow.Apply();
		m_border_pow.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/panel/panel_glass_border_glowing.vp", 0xa14);

	virtual void SetMVP(const math::matrix &value)
	{
		m_mvp.Set(value);
	}

	virtual void SetSize(const math::v4 &value)
	{
		m_size.Set(value);
	}

	virtual void SetTransform(const math::matrix &value)
	{
		m_transform.Set(value);
	}

	virtual void SetWorldScaleHack(const float &value)
	{
		m_world_scale_hack.Set(value);
	}

	virtual void SetMaterialPack(const math::matrix &value)
	{
		m_material_pack.Set(value);
	}

	virtual void SetPointLightPack2(const math::matrix &value)
	{
		m_point_light_pack2.Set(value);
	}

	virtual void SetPointLightPack1(const math::matrix &value)
	{
		m_point_light_pack1.Set(value);
	}

	virtual void SetBackgroundSize(const math::v4 &value)
	{
	}

	virtual void SetBackgroundRatio(const math::v2 &value)
	{
		m_background_ratio.Set(value);
	}

	virtual void SetView(const math::matrix &value)
	{
		m_view.Set(value);
	}

	virtual void SetEyePos(const math::v4 &value)
	{
		m_eye_pos.Set(value);
	}

	virtual void SetHitPoint(const math::v4 &value)
	{
	}

	virtual void SetReserved1(const math::v4 &value)
	{
	}

	virtual void SetReserved2(const math::v4 &value)
	{
	}

	virtual void SetSpecularFactor(const math::v4 &value)
	{
	}

	virtual void SetColor(const math::v4 &value)
	{
	}

	virtual void SetWaveCenterColor(const math::v4 &value)
	{
	}

	virtual void SetWaveEdgeColor(const math::v4 &value)
	{
	}

	virtual void SetWaveFogPack(const math::matrix &value)
	{
	}

	virtual void SetBorderSize(const math::v3 &value)
	{
		m_border_size.Set(value);
	}

	virtual void SetRoundness(const math::v2 &value)
	{
		m_roundness.Set(value);
	}

	virtual void SetBorderGlow(const math::v4 &value)
	{
		m_border_glow.Set(value);
	}

	virtual void SetBorderPow(const math::v2 &value)
	{
		m_border_pow.Set(value);
	}

	arl::ShaderParameter<math::matrix> m_mvp;
	arl::ShaderParameter<math::v4> m_size;
	arl::ShaderParameter<math::matrix> m_transform;
	arl::ShaderParameter<float> m_world_scale_hack;
	arl::ShaderParameter<math::matrix> m_material_pack;
	arl::ShaderParameter<math::matrix> m_point_light_pack2;
	arl::ShaderParameter<math::matrix> m_point_light_pack1;
	arl::ShaderParameter<math::v2> m_background_ratio;
	arl::ShaderParameter<math::matrix> m_view;
	arl::ShaderParameter<math::v4> m_eye_pos;
	arl::ShaderParameter<math::v3> m_border_size;
	arl::ShaderParameter<math::v2> m_roundness;
	arl::ShaderParameter<math::v4> m_border_glow;
	arl::ShaderParameter<math::v2> m_border_pow;
};

/* paf/cg/drawobj/panel/panel_glass_glowing.fp */
class PanelGlassGlowingFragShader : public arl::ShaderInterfaceBase, public PanelFragShaderParams {
public:
	PanelGlassGlowingFragShader()
		: m_specular_factor(m_setup), m_color(m_setup), m_background(m_setup), m_env_map_detail(m_setup), m_glow_color(m_setup)
	{
	}

	virtual void Init()
	{
		m_specular_factor.init("cargs.SpecularFactor", arl::ShaderParameterBase::UNIFORM, 0);
		m_color.init("cargs.Color", arl::ShaderParameterBase::UNIFORM, 0);
		m_background.init("cargs.Background", 0, 0);
		m_env_map_detail.init("cargs.EnvMapDetail", 1, 0);
		m_glow_color.init("GlowColor", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_specular_factor.Apply();
		m_color.Apply();
		m_background.Apply();
		m_env_map_detail.Apply();
		m_glow_color.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/panel/panel_glass_glowing.fp", 0x234);

	virtual void SetMaterialPack(const math::matrix &value)
	{
	}

	virtual void SetPointLightPack2(const math::matrix &value)
	{
	}

	virtual void SetPointLightPack1(const math::matrix &value)
	{
	}

	virtual void SetBackgroundSize(const math::v4 &value)
	{
	}

	virtual void SetBackgroundRatio(const math::v2 &value)
	{
	}

	virtual void SetView(const math::matrix &value)
	{
	}

	virtual void SetEyePos(const math::v4 &value)
	{
	}

	virtual void SetHitPoint(const math::v4 &value)
	{
	}

	virtual void SetReserved1(const math::v4 &value)
	{
	}

	virtual void SetReserved2(const math::v4 &value)
	{
	}

	virtual void SetSpecularFactor(const math::v4 &value)
	{
		m_specular_factor.Set(value);
	}

	virtual void SetColor(const math::v4 &value)
	{
		m_color.Set(value);
	}

	virtual void SetWaveCenterColor(const math::v4 &value)
	{
	}

	virtual void SetWaveEdgeColor(const math::v4 &value)
	{
	}

	virtual void SetWaveFogPack(const math::matrix &value)
	{
	}

	virtual void SetColorMap(const arl::TextureSource &value)
	{
	}

	virtual void SetBackground(const arl::TextureSource &value)
	{
		m_background.Set(value);
	}

	virtual void SetEnvMapDetail(const arl::TextureSource &value)
	{
		m_env_map_detail.Set(value);
	}

	virtual void SetGlowColor(const math::v4 &value)
	{
		m_glow_color.Set(value);
	}

	arl::ShaderParameter<math::v4> m_specular_factor;
	arl::ShaderParameter<math::v4> m_color;
	arl::ShaderParameter<arl::TextureSource> m_background;
	arl::ShaderParameter<arl::TextureSource> m_env_map_detail;
	arl::ShaderParameter<math::v4> m_glow_color;
};

/* paf/cg/drawobj/panel/panel_opaque_wave_border.vp */
class PanelOpaqueWaveBorderVertShader : public arl::ShaderInterfaceBase, public PanelVertShaderParams {
public:
	PanelOpaqueWaveBorderVertShader()
		: m_mvp(m_setup), m_size(m_setup), m_transform(m_setup), m_world_scale_hack(m_setup), m_material_pack(m_setup), m_point_light_pack2(m_setup), m_point_light_pack1(m_setup), m_eye_pos(m_setup), m_wave_center_color(m_setup), m_wave_edge_color(m_setup), m_wave_fog_pack(m_setup), m_border_size(m_setup), m_roundness(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("MVP", arl::ShaderParameterBase::UNIFORM, 0);
		m_size.init("Size", arl::ShaderParameterBase::UNIFORM, 0);
		m_transform.init("Transform", arl::ShaderParameterBase::UNIFORM, 0);
		m_world_scale_hack.init("WorldScaleHack", arl::ShaderParameterBase::UNIFORM, 0);
		m_material_pack.init("cargs.MaterialPack", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack2.init("cargs.PointLightPack2", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack1.init("cargs.PointLightPack1", arl::ShaderParameterBase::UNIFORM, 0);
		m_eye_pos.init("cargs.EyePos", arl::ShaderParameterBase::UNIFORM, 0);
		m_wave_center_color.init("cargs.WaveCenterColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_wave_edge_color.init("cargs.WaveEdgeColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_wave_fog_pack.init("cargs.WaveFogPack", arl::ShaderParameterBase::UNIFORM, 0);
		m_border_size.init("BorderSize", arl::ShaderParameterBase::UNIFORM, 0);
		m_roundness.init("Roundness", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_size.Apply();
		m_transform.Apply();
		m_world_scale_hack.Apply();
		m_material_pack.Apply();
		m_point_light_pack2.Apply();
		m_point_light_pack1.Apply();
		m_eye_pos.Apply();
		m_wave_center_color.Apply();
		m_wave_edge_color.Apply();
		m_wave_fog_pack.Apply();
		m_border_size.Apply();
		m_roundness.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/panel/panel_opaque_wave_border.vp", 0x5c8);

	virtual void SetMVP(const math::matrix &value)
	{
		m_mvp.Set(value);
	}

	virtual void SetSize(const math::v4 &value)
	{
		m_size.Set(value);
	}

	virtual void SetTransform(const math::matrix &value)
	{
		m_transform.Set(value);
	}

	virtual void SetWorldScaleHack(const float &value)
	{
		m_world_scale_hack.Set(value);
	}

	virtual void SetMaterialPack(const math::matrix &value)
	{
		m_material_pack.Set(value);
	}

	virtual void SetPointLightPack2(const math::matrix &value)
	{
		m_point_light_pack2.Set(value);
	}

	virtual void SetPointLightPack1(const math::matrix &value)
	{
		m_point_light_pack1.Set(value);
	}

	virtual void SetBackgroundSize(const math::v4 &value)
	{
	}

	virtual void SetBackgroundRatio(const math::v2 &value)
	{
	}

	virtual void SetView(const math::matrix &value)
	{
	}

	virtual void SetEyePos(const math::v4 &value)
	{
		m_eye_pos.Set(value);
	}

	virtual void SetHitPoint(const math::v4 &value)
	{
	}

	virtual void SetReserved1(const math::v4 &value)
	{
	}

	virtual void SetReserved2(const math::v4 &value)
	{
	}

	virtual void SetSpecularFactor(const math::v4 &value)
	{
	}

	virtual void SetColor(const math::v4 &value)
	{
	}

	virtual void SetWaveCenterColor(const math::v4 &value)
	{
		m_wave_center_color.Set(value);
	}

	virtual void SetWaveEdgeColor(const math::v4 &value)
	{
		m_wave_edge_color.Set(value);
	}

	virtual void SetWaveFogPack(const math::matrix &value)
	{
		m_wave_fog_pack.Set(value);
	}

	virtual void SetBorderSize(const math::v3 &value)
	{
		m_border_size.Set(value);
	}

	virtual void SetRoundness(const math::v2 &value)
	{
		m_roundness.Set(value);
	}

	virtual void SetBorderGlow(const math::v4 &value)
	{
	}

	virtual void SetBorderPow(const math::v2 &value)
	{
	}

	arl::ShaderParameter<math::matrix> m_mvp;
	arl::ShaderParameter<math::v4> m_size;
	arl::ShaderParameter<math::matrix> m_transform;
	arl::ShaderParameter<float> m_world_scale_hack;
	arl::ShaderParameter<math::matrix> m_material_pack;
	arl::ShaderParameter<math::matrix> m_point_light_pack2;
	arl::ShaderParameter<math::matrix> m_point_light_pack1;
	arl::ShaderParameter<math::v4> m_eye_pos;
	arl::ShaderParameter<math::v4> m_wave_center_color;
	arl::ShaderParameter<math::v4> m_wave_edge_color;
	arl::ShaderParameter<math::matrix> m_wave_fog_pack;
	arl::ShaderParameter<math::v3> m_border_size;
	arl::ShaderParameter<math::v2> m_roundness;
};

/* paf/cg/drawobj/panel/panel_opaque_wave_top.vp */
class PanelOpaqueWaveTopVertShader : public arl::ShaderInterfaceBase, public PanelVertShaderParams {
public:
	PanelOpaqueWaveTopVertShader()
		: m_mvp(m_setup), m_size(m_setup), m_transform(m_setup), m_world_scale_hack(m_setup), m_material_pack(m_setup), m_point_light_pack2(m_setup), m_point_light_pack1(m_setup), m_eye_pos(m_setup), m_wave_center_color(m_setup), m_wave_edge_color(m_setup), m_wave_fog_pack(m_setup), m_border_size(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("MVP", arl::ShaderParameterBase::UNIFORM, 0);
		m_size.init("Size", arl::ShaderParameterBase::UNIFORM, 0);
		m_transform.init("Transform", arl::ShaderParameterBase::UNIFORM, 0);
		m_world_scale_hack.init("WorldScaleHack", arl::ShaderParameterBase::UNIFORM, 0);
		m_material_pack.init("cargs.MaterialPack", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack2.init("cargs.PointLightPack2", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack1.init("cargs.PointLightPack1", arl::ShaderParameterBase::UNIFORM, 0);
		m_eye_pos.init("cargs.EyePos", arl::ShaderParameterBase::UNIFORM, 0);
		m_wave_center_color.init("cargs.WaveCenterColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_wave_edge_color.init("cargs.WaveEdgeColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_wave_fog_pack.init("cargs.WaveFogPack", arl::ShaderParameterBase::UNIFORM, 0);
		m_border_size.init("BorderSize", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_size.Apply();
		m_transform.Apply();
		m_world_scale_hack.Apply();
		m_material_pack.Apply();
		m_point_light_pack2.Apply();
		m_point_light_pack1.Apply();
		m_eye_pos.Apply();
		m_wave_center_color.Apply();
		m_wave_edge_color.Apply();
		m_wave_fog_pack.Apply();
		m_border_size.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/panel/panel_opaque_wave_top.vp", 0x438);

	virtual void SetMVP(const math::matrix &value)
	{
		m_mvp.Set(value);
	}

	virtual void SetSize(const math::v4 &value)
	{
		m_size.Set(value);
	}

	virtual void SetTransform(const math::matrix &value)
	{
		m_transform.Set(value);
	}

	virtual void SetWorldScaleHack(const float &value)
	{
		m_world_scale_hack.Set(value);
	}

	virtual void SetMaterialPack(const math::matrix &value)
	{
		m_material_pack.Set(value);
	}

	virtual void SetPointLightPack2(const math::matrix &value)
	{
		m_point_light_pack2.Set(value);
	}

	virtual void SetPointLightPack1(const math::matrix &value)
	{
		m_point_light_pack1.Set(value);
	}

	virtual void SetBackgroundSize(const math::v4 &value)
	{
	}

	virtual void SetBackgroundRatio(const math::v2 &value)
	{
	}

	virtual void SetView(const math::matrix &value)
	{
	}

	virtual void SetEyePos(const math::v4 &value)
	{
		m_eye_pos.Set(value);
	}

	virtual void SetHitPoint(const math::v4 &value)
	{
	}

	virtual void SetReserved1(const math::v4 &value)
	{
	}

	virtual void SetReserved2(const math::v4 &value)
	{
	}

	virtual void SetSpecularFactor(const math::v4 &value)
	{
	}

	virtual void SetColor(const math::v4 &value)
	{
	}

	virtual void SetWaveCenterColor(const math::v4 &value)
	{
		m_wave_center_color.Set(value);
	}

	virtual void SetWaveEdgeColor(const math::v4 &value)
	{
		m_wave_edge_color.Set(value);
	}

	virtual void SetWaveFogPack(const math::matrix &value)
	{
		m_wave_fog_pack.Set(value);
	}

	virtual void SetBorderSize(const math::v3 &value)
	{
		m_border_size.Set(value);
	}

	virtual void SetRoundness(const math::v2 &value)
	{
	}

	virtual void SetBorderGlow(const math::v4 &value)
	{
	}

	virtual void SetBorderPow(const math::v2 &value)
	{
	}

	arl::ShaderParameter<math::matrix> m_mvp;
	arl::ShaderParameter<math::v4> m_size;
	arl::ShaderParameter<math::matrix> m_transform;
	arl::ShaderParameter<float> m_world_scale_hack;
	arl::ShaderParameter<math::matrix> m_material_pack;
	arl::ShaderParameter<math::matrix> m_point_light_pack2;
	arl::ShaderParameter<math::matrix> m_point_light_pack1;
	arl::ShaderParameter<math::v4> m_eye_pos;
	arl::ShaderParameter<math::v4> m_wave_center_color;
	arl::ShaderParameter<math::v4> m_wave_edge_color;
	arl::ShaderParameter<math::matrix> m_wave_fog_pack;
	arl::ShaderParameter<math::v3> m_border_size;
};

/* paf/cg/drawobj/panel/panel_opaque_wave.fp */
class PanelOpaqueWaveFragShader : public arl::ShaderInterfaceBase, public PanelFragShaderParams {
public:
	PanelOpaqueWaveFragShader()
		: m_color(m_setup)
	{
	}

	virtual void Init()
	{
		m_color.init("cargs.Color", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_color.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/panel/panel_opaque_wave.fp", 0x100);

	virtual void SetMaterialPack(const math::matrix &value)
	{
	}

	virtual void SetPointLightPack2(const math::matrix &value)
	{
	}

	virtual void SetPointLightPack1(const math::matrix &value)
	{
	}

	virtual void SetBackgroundSize(const math::v4 &value)
	{
	}

	virtual void SetBackgroundRatio(const math::v2 &value)
	{
	}

	virtual void SetView(const math::matrix &value)
	{
	}

	virtual void SetEyePos(const math::v4 &value)
	{
	}

	virtual void SetHitPoint(const math::v4 &value)
	{
	}

	virtual void SetReserved1(const math::v4 &value)
	{
	}

	virtual void SetReserved2(const math::v4 &value)
	{
	}

	virtual void SetSpecularFactor(const math::v4 &value)
	{
	}

	virtual void SetColor(const math::v4 &value)
	{
		m_color.Set(value);
	}

	virtual void SetWaveCenterColor(const math::v4 &value)
	{
	}

	virtual void SetWaveEdgeColor(const math::v4 &value)
	{
	}

	virtual void SetWaveFogPack(const math::matrix &value)
	{
	}

	virtual void SetColorMap(const arl::TextureSource &value)
	{
	}

	virtual void SetBackground(const arl::TextureSource &value)
	{
	}

	virtual void SetEnvMapDetail(const arl::TextureSource &value)
	{
	}

	virtual void SetGlowColor(const math::v4 &value)
	{
	}

	arl::ShaderParameter<math::v4> m_color;
};

/* paf/cg/drawobj/panel/bubble_image.vp */
class BubbleImageVertShader : public arl::ShaderInterfaceBase, public BubbleVertShaderParams {
public:
	BubbleImageVertShader()
		: m_mvp(m_setup), m_height(m_setup), m_transform_inverse_transpose(m_setup), m_transform(m_setup), m_material_pack(m_setup), m_light_pos(m_setup), m_ambient_color(m_setup), m_diffuse_color(m_setup), m_eye_pos(m_setup), m_back_color_z_range(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("MVP", arl::ShaderParameterBase::UNIFORM, 0);
		m_height.init("Height", arl::ShaderParameterBase::UNIFORM, 0);
		m_transform_inverse_transpose.init("TransformInverseTranspose", arl::ShaderParameterBase::UNIFORM, 0);
		m_transform.init("Transform", arl::ShaderParameterBase::UNIFORM, 0);
		m_material_pack.init("cargs.MaterialPack", arl::ShaderParameterBase::UNIFORM, 0);
		m_light_pos.init("cargs.LightPos", arl::ShaderParameterBase::UNIFORM, 0);
		m_ambient_color.init("cargs.AmbientColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_diffuse_color.init("cargs.DiffuseColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_eye_pos.init("cargs.EyePos", arl::ShaderParameterBase::UNIFORM, 0);
		m_back_color_z_range.init("BackColorZRange", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_height.Apply();
		m_transform_inverse_transpose.Apply();
		m_transform.Apply();
		m_material_pack.Apply();
		m_light_pos.Apply();
		m_ambient_color.Apply();
		m_diffuse_color.Apply();
		m_eye_pos.Apply();
		m_back_color_z_range.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/panel/bubble_image.vp", 0x3b8);

	virtual void SetMVP(const math::matrix &value)
	{
		m_mvp.Set(value);
	}

	virtual void SetHeight(const math::v2 &value)
	{
		m_height.Set(value);
	}

	virtual void SetTransformInverseTranspose(const math::matrix &value)
	{
		m_transform_inverse_transpose.Set(value);
	}

	virtual void SetTransform(const math::matrix &value)
	{
		m_transform.Set(value);
	}

	virtual void SetMaterialPack(const math::matrix &value)
	{
		m_material_pack.Set(value);
	}

	virtual void SetLightPos(const math::v3 &value)
	{
		m_light_pos.Set(value);
	}

	virtual void SetDiffuseColor(const math::v4 &value)
	{
		m_diffuse_color.Set(value);
	}

	virtual void SetView(const math::matrix &value)
	{
	}

	virtual void SetEyePos(const math::v4 &value)
	{
		m_eye_pos.Set(value);
	}

	virtual void SetBackColorZRange(const math::v2 &value)
	{
		m_back_color_z_range.Set(value);
	}

	arl::ShaderParameter<math::matrix> m_mvp;
	arl::ShaderParameter<math::v2> m_height;
	arl::ShaderParameter<math::matrix> m_transform_inverse_transpose;
	arl::ShaderParameter<math::matrix> m_transform;
	arl::ShaderParameter<math::matrix> m_material_pack;
	arl::ShaderParameter<math::v3> m_light_pos;
	arl::ShaderParameter<math::v4> m_ambient_color;
	arl::ShaderParameter<math::v4> m_diffuse_color;
	arl::ShaderParameter<math::v4> m_eye_pos;
	arl::ShaderParameter<math::v2> m_back_color_z_range;
};

/* paf/cg/drawobj/panel/bubble_image.fp */
class BubbleImageFragShader : public arl::ShaderInterfaceBase, public BubbleFragShaderParams {
public:
	BubbleImageFragShader()
		: m_specular_factor(m_setup), m_color_map(m_setup)
	{
	}

	virtual void Init()
	{
		m_specular_factor.init("cargs.SpecularFactor", arl::ShaderParameterBase::UNIFORM, 0);
		m_color_map.init("cargs.ColorMap", 2, 0);
	}

	virtual void Apply()
	{
		m_specular_factor.Apply();
		m_color_map.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/panel/bubble_image.fp", 0x168);

	virtual void SetBackColor(const math::v4 &value)
	{
	}

	virtual void SetSpecularFactor(const math::v4 &value)
	{
		m_specular_factor.Set(value);
	}

	virtual void SetColorMap(const arl::TextureSource &value)
	{
		m_color_map.Set(value);
	}

	virtual void SetEnvMapDetail(const arl::TextureSource &value)
	{
	}

	virtual void SetAlpha(const float &value)
	{
	}

	arl::ShaderParameter<math::v4> m_specular_factor;
	arl::ShaderParameter<arl::TextureSource> m_color_map;
};

/* paf/cg/drawobj/panel/bubble_opaque.vp */
class BubbleOpaqueVertShader : public arl::ShaderInterfaceBase, public BubbleVertShaderParams {
public:
	BubbleOpaqueVertShader()
		: m_mvp(m_setup), m_height(m_setup), m_transform_inverse_transpose(m_setup), m_transform(m_setup), m_material_pack(m_setup), m_light_pos(m_setup), m_diffuse_color(m_setup), m_view(m_setup), m_eye_pos(m_setup), m_back_color_z_range(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("MVP", arl::ShaderParameterBase::UNIFORM, 0);
		m_height.init("Height", arl::ShaderParameterBase::UNIFORM, 0);
		m_transform_inverse_transpose.init("TransformInverseTranspose", arl::ShaderParameterBase::UNIFORM, 0);
		m_transform.init("Transform", arl::ShaderParameterBase::UNIFORM, 0);
		m_material_pack.init("cargs.MaterialPack", arl::ShaderParameterBase::UNIFORM, 0);
		m_light_pos.init("cargs.LightPos", arl::ShaderParameterBase::UNIFORM, 0);
		m_diffuse_color.init("cargs.DiffuseColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_view.init("cargs.View", arl::ShaderParameterBase::UNIFORM, 0);
		m_eye_pos.init("cargs.EyePos", arl::ShaderParameterBase::UNIFORM, 0);
		m_back_color_z_range.init("BackColorZRange", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_height.Apply();
		m_transform_inverse_transpose.Apply();
		m_transform.Apply();
		m_material_pack.Apply();
		m_light_pos.Apply();
		m_diffuse_color.Apply();
		m_view.Apply();
		m_eye_pos.Apply();
		m_back_color_z_range.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/panel/bubble_opaque.vp", 0x614);

	virtual void SetMVP(const math::matrix &value)
	{
		m_mvp.Set(value);
	}

	virtual void SetHeight(const math::v2 &value)
	{
		m_height.Set(value);
	}

	virtual void SetTransformInverseTranspose(const math::matrix &value)
	{
		m_transform_inverse_transpose.Set(value);
	}

	virtual void SetTransform(const math::matrix &value)
	{
		m_transform.Set(value);
	}

	virtual void SetMaterialPack(const math::matrix &value)
	{
		m_material_pack.Set(value);
	}

	virtual void SetLightPos(const math::v3 &value)
	{
		m_light_pos.Set(value);
	}

	virtual void SetDiffuseColor(const math::v4 &value)
	{
		m_diffuse_color.Set(value);
	}

	virtual void SetView(const math::matrix &value)
	{
		m_view.Set(value);
	}

	virtual void SetEyePos(const math::v4 &value)
	{
		m_eye_pos.Set(value);
	}

	virtual void SetBackColorZRange(const math::v2 &value)
	{
		m_back_color_z_range.Set(value);
	}

	arl::ShaderParameter<math::matrix> m_mvp;
	arl::ShaderParameter<math::v2> m_height;
	arl::ShaderParameter<math::matrix> m_transform_inverse_transpose;
	arl::ShaderParameter<math::matrix> m_transform;
	arl::ShaderParameter<math::matrix> m_material_pack;
	arl::ShaderParameter<math::v3> m_light_pos;
	arl::ShaderParameter<math::v4> m_diffuse_color;
	arl::ShaderParameter<math::matrix> m_view;
	arl::ShaderParameter<math::v4> m_eye_pos;
	arl::ShaderParameter<math::v2> m_back_color_z_range;
};

/* paf/cg/drawobj/panel/bubble_opaque.fp */
class BubbleOpaqueFragShader : public arl::ShaderInterfaceBase, public BubbleFragShaderParams {
public:
	BubbleOpaqueFragShader()
		: m_back_color(m_setup), m_specular_factor(m_setup), m_color_map(m_setup), m_env_map_detail(m_setup), m_alpha(m_setup)
	{
	}

	virtual void Init()
	{
		m_back_color.init("cargs.BackColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_specular_factor.init("cargs.SpecularFactor", arl::ShaderParameterBase::UNIFORM, 0);
		m_color_map.init("cargs.ColorMap", 0, 0);
		m_env_map_detail.init("cargs.EnvMapDetail", 1, 0);
		m_alpha.init("Alpha", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_back_color.Apply();
		m_specular_factor.Apply();
		m_color_map.Apply();
		m_env_map_detail.Apply();
		m_alpha.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/panel/bubble_opaque.fp", 0x1fc);

	virtual void SetBackColor(const math::v4 &value)
	{
		m_back_color.Set(value);
	}

	virtual void SetSpecularFactor(const math::v4 &value)
	{
		m_specular_factor.Set(value);
	}

	virtual void SetColorMap(const arl::TextureSource &value)
	{
		m_color_map.Set(value);
	}

	virtual void SetEnvMapDetail(const arl::TextureSource &value)
	{
		m_env_map_detail.Set(value);
	}

	virtual void SetAlpha(const float &value)
	{
		m_alpha.Set(value);
	}

	arl::ShaderParameter<math::v4> m_back_color;
	arl::ShaderParameter<math::v4> m_specular_factor;
	arl::ShaderParameter<arl::TextureSource> m_color_map;
	arl::ShaderParameter<arl::TextureSource> m_env_map_detail;
	arl::ShaderParameter<float> m_alpha;
};

/* paf/cg/drawobj/panel/bubble_opaque_folder.fp */
class BubbleOpaqueFolderFragShader : public arl::ShaderInterfaceBase, public BubbleFragShaderParams {
public:
	BubbleOpaqueFolderFragShader()
		: m_back_color(m_setup), m_specular_factor(m_setup), m_color_map(m_setup), m_env_map_detail(m_setup), m_alpha(m_setup)
	{
	}

	virtual void Init()
	{
		m_back_color.init("cargs.BackColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_specular_factor.init("cargs.SpecularFactor", arl::ShaderParameterBase::UNIFORM, 0);
		m_color_map.init("cargs.ColorMap", 0, 0);
		m_env_map_detail.init("cargs.EnvMapDetail", 1, 0);
		m_alpha.init("Alpha", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_back_color.Apply();
		m_specular_factor.Apply();
		m_color_map.Apply();
		m_env_map_detail.Apply();
		m_alpha.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/panel/bubble_opaque_folder.fp", 0x290);

	virtual void SetBackColor(const math::v4 &value)
	{
		m_back_color.Set(value);
	}

	virtual void SetSpecularFactor(const math::v4 &value)
	{
		m_specular_factor.Set(value);
	}

	virtual void SetColorMap(const arl::TextureSource &value)
	{
		m_color_map.Set(value);
	}

	virtual void SetEnvMapDetail(const arl::TextureSource &value)
	{
		m_env_map_detail.Set(value);
	}

	virtual void SetAlpha(const float &value)
	{
		m_alpha.Set(value);
	}

	arl::ShaderParameter<math::v4> m_back_color;
	arl::ShaderParameter<math::v4> m_specular_factor;
	arl::ShaderParameter<arl::TextureSource> m_color_map;
	arl::ShaderParameter<arl::TextureSource> m_env_map_detail;
	arl::ShaderParameter<float> m_alpha;
};

/* a shader's parameters, to be set, and its shader object, as the renderer picks them (names
   ours) */
template<class Params, class Object>
class ShaderPair {
public:
	ShaderPair(Params *params, Object *object) : m_params(params), m_object(object)
	{
	}

	Params *m_params;
	Object *m_object;
};

typedef ShaderPair<PanelVertShaderParams, arl::VertShaderObject> PanelVertShaderPair;
typedef ShaderPair<PanelFragShaderParams, arl::FragShaderObject> PanelFragShaderPair;
typedef ShaderPair<BubbleVertShaderParams, arl::VertShaderObject> BubbleVertShaderPair;
typedef ShaderPair<BubbleFragShaderParams, arl::FragShaderObject> BubbleFragShaderPair;

/* the kinds of panel the renderer draws, each with its shaders (names ours) */
enum PanelKind {
	PanelKind_Glass = 0,
	PanelKind_GlassGlowing = 1,
	PanelKind_Opaque = 2,
	PanelKind_OpaqueWave = 3,
	PanelKind_Bubble = 4,
	PanelKind_BubbleFolder = 5
};

/* the panel renderer's shaders (names ours); 0x24e8 bytes */
class PanelShaders {
public:
	/* 0x810ba570: the vertex shader of a panel's border, or of its top */
	PanelVertShaderPair GetPanelVert(int32_t kind, bool top);
	/* 0x810ba510 */
	PanelFragShaderPair GetPanelFrag(int32_t kind);
	/* 0x810bbfe2 */
	BubbleVertShaderPair GetBubbleVert(int32_t kind);
	/* 0x810bbdb8 */
	BubbleFragShaderPair GetBubbleFrag(int32_t kind);

	arl::ShaderInstance<PanelGlassBorderVertShader, arl::VertShaderObject> m_glass_border_vert;
	arl::ShaderInstance<PanelGlassTopVertShader, arl::VertShaderObject> m_glass_top_vert;
	arl::ShaderInstance<PanelGlassFragShader, arl::FragShaderObject> m_glass_frag;
	arl::ShaderInstance<PanelOpaqueBorderVertShader, arl::VertShaderObject> m_opaque_border_vert;
	arl::ShaderInstance<PanelOpaqueTopVertShader, arl::VertShaderObject> m_opaque_top_vert;
	arl::ShaderInstance<PanelOpaqueFragShader, arl::FragShaderObject> m_opaque_frag;
	arl::ShaderInstance<PanelGlassBorderGlowingVertShader, arl::VertShaderObject>
		m_glass_border_glowing_vert;
	arl::ShaderInstance<PanelGlassGlowingFragShader, arl::FragShaderObject> m_glass_glowing_frag;
	arl::ShaderInstance<PanelOpaqueWaveBorderVertShader, arl::VertShaderObject>
		m_opaque_wave_border_vert;
	arl::ShaderInstance<PanelOpaqueWaveTopVertShader, arl::VertShaderObject>
		m_opaque_wave_top_vert;
	arl::ShaderInstance<PanelOpaqueWaveFragShader, arl::FragShaderObject> m_opaque_wave_frag;
	arl::ShaderInstance<BubbleImageVertShader, arl::VertShaderObject> m_bubble_image_vert;
	arl::ShaderInstance<BubbleImageFragShader, arl::FragShaderObject> m_bubble_image_frag;
	arl::ShaderInstance<BubbleOpaqueVertShader, arl::VertShaderObject> m_bubble_opaque_vert;
	arl::ShaderInstance<BubbleOpaqueFragShader, arl::FragShaderObject> m_bubble_opaque_frag;
	arl::ShaderInstance<BubbleOpaqueFolderFragShader, arl::FragShaderObject>
		m_bubble_opaque_folder_frag;
};

/* the wave a touch sends across a panel */
class TouchWaveParamSet : public ParamSet, public ParamLink {
public:
	class Values {
	public:
		Values(ParamSet *set)
			: m_half_width(set, "half_width", 0.35f, 0.0f, 2.0f, PARAM_FLAG_FINE),
			  m_speed(set, "speed", 3.0f, 0.0f, 10.0f, 0),
			  m_fade(set, "fade", 2.5f, 0.0f, 10.0f, 0),
			  m_scale(set, "scale", 151.0f, 0.0f, 500.0f, 0)
		{
		}

		Param<float> m_half_width;
		Param<float> m_speed;
		Param<float> m_fade;
		Param<float> m_scale;
	};

	WAVE_PARAM_SET_KEYED(TouchWaveParamSet, "TouchWave", __attribute__((noinline)));
};

/* the shadow a bubble casts */
class ShadowParamSet : public ParamSet, public ParamLink {
public:
	class Values {
	public:
		Values(ParamSet *set)
			: m_scale_tweak(set, "scale_tweak", 0.86f, 0.0f, 1.2f, 0),
			  m_transparency(set, "transparency", 0.25f, 0.0f, 10.0f, PARAM_FLAG_FINE),
			  m_transparency_fade_start(set, "transparency_fade_start", 500.0f, 0.0f,
						    2000.0f, 0),
			  m_transparency_fade_len(set, "transparency_fade_len", 200.0f, 0.0f, 2000.0f,
						  0),
			  m_color(set, "color", math::v4(0.0f, 0.0f, 1.0f, 0.0f),
				  math::v4(0.0f, 0.0f, 0.0f, 0.0f), math::v4(1.0f, 1.0f, 1.0f, 1.0f),
				  PARAM_FLAG_COLOR_ALPHA),
			  m_plane_z(set, "plane_z", 0.04f, 0.0f, 0.5f, 0),
			  m_back_color_begin(set, "back_color_begin", 0.05f, 0.0f, 1.0f,
					     PARAM_FLAG_FINE),
			  m_back_color_end(set, "back_color_end", 0.37f, 0.0f, 1.0f, PARAM_FLAG_FINE),
			  m_wave_shadow_blend(set, "wave_shadow_blend", 1.0f, 0.0f, 1.0f, 0),
			  m_wave_shadow_filter(set, "wave_shadow_filter", 0.2f, 0.0f, 1.0f, 0)
		{
		}

		Param<float> m_scale_tweak;
		Param<float> m_transparency;
		Param<float> m_transparency_fade_start;
		Param<float> m_transparency_fade_len;
		Param<math::v4> m_color;
		Param<float> m_plane_z;
		Param<float> m_back_color_begin;
		Param<float> m_back_color_end;
		Param<float> m_wave_shadow_blend;
		Param<float> m_wave_shadow_filter;
	};

	WAVE_PARAM_SET_KEYED(ShadowParamSet, "Shadow", __attribute__((noinline)));
};

/* a vertex of a panel's grids: where it is in the grid and on the panel's outline, and the
   turns of its edge, as halves (names ours); 0x10 bytes */
class PanelVertex {
public:
	__attribute__((noinline)) PanelVertex(math::v4 position, math::v4 edge)
	{
		m_position[0] = Half(position.extract_x());
		m_position[1] = Half(position.extract_y());
		m_position[2] = Half(position.extract_z());
		m_position[3] = Half(position.extract_w());
		m_edge[0] = Half(edge.extract_x());
		m_edge[1] = Half(edge.extract_y());
		m_edge[2] = Half(edge.extract_z());
		m_edge[3] = Half(edge.extract_w());
	}

	Half m_position[4];
	Half m_edge[4];
};

/* a vertex's column and row in its grid (names ours) */
class GridPosition {
public:
	GridPosition(int32_t x, int32_t y) : m_x(x), m_y(y)
	{
	}

	int32_t m_x;
	int32_t m_y;
};

/* what makes each vertex of a grid from its column and row (names ours) */
template<class Vertex>
class GridVertexGenerator {
public:
	virtual Vertex operator()(GridPosition position, arl::SurfaceSize size) const = 0;

	virtual ~GridVertexGenerator()
	{
	}
};

/* every vertex of the mesh made by the generator, from its column and row (name ours) */
template<class Vertex>
inline void FillGrid(WaveMesh<Vertex> &mesh, const GridVertexGenerator<Vertex> &generator)
{
	if (mesh.m_index_count == 0)
		return;

	Vertex *vertex = (Vertex *)mesh.m_vertices.m_buffer->m_memory->m_addr;

	for (uint32_t y = 0; y < mesh.m_height; y++)
		for (uint32_t x = 0; x < mesh.m_width; x++)
			*vertex++ = generator(GridPosition(x, y),
					      arl::SurfaceSize(mesh.m_width, mesh.m_height));
}

/* a vertex of a profile swept around: the profile's point and normal, where it is on the
   grid, and the turn around (names ours); 0x20 bytes */
class ProfileVertex {
public:
	math::v4 m_position;
	math::v4 m_texcoord;
};

/* points held elsewhere, and how many (names ours) */
class PointArray {
public:
	PointArray(const math::v2 *points, uint32_t count, uint32_t limit)
		: m_points(points), m_count(count), m_limit(limit)
	{
	}

	const math::v2 *m_points;
	uint32_t m_count;
	uint32_t m_limit;
};

/*
 * A profile, a line of points, swept once around the vertical, its grid a column for each step
 * around and a row for each point (names ours); 0x54 bytes.
 */
class ProfileMesh {
public:
	/* how the profile's ends continue: mirrored across the vertical */
	enum Flags {
		MIRROR_START = 2,
		MIRROR_END = 4
	};

	/* 0x810b9cc2: the points kept, their lowest and highest, and the grid made */
	void Init(PointArray points, int32_t columns, uint32_t flags);

	/* 0x8120aaa4 */
	void Init(const vector<math::v2> &points, int32_t columns, uint32_t flags)
	{
		Init(PointArray(points.ptr, points.size(), points.size()), columns, flags);
	}

	WaveMesh<ProfileVertex> m_mesh;
	float m_bottom;
	float m_top;
	vector<math::v2> m_points;
};

/*
 * The grids a panel is drawn with: the rounded border at each of its four corners, the top
 * between each corner and the next, and the flat face (names ours); 0x248 bytes.
 */
class PanelMeshes {
public:
	/* 0x810b915c: the grids, the borders border_columns by rows, the tops columns by rows, the
	   face columns square */
	void Init(int32_t border_columns, int32_t rows, int32_t columns);

	common::SharedPtr<arl::VertexArraySpec> m_spec;
	WaveMesh<PanelVertex> m_borders[4];
	WaveMesh<PanelVertex> m_tops[4];
	WaveMesh<PanelVertex> m_face;
};


/* the meshes of the profiles swept around, as the renderer keeps its one (names ours) */
class ProfileMeshArray {
public:
	ProfileMeshArray(ProfileMesh *meshes, uint32_t count, uint32_t limit)
		: m_meshes(meshes), m_count(count), m_limit(limit)
	{
	}

	ProfileMesh *m_meshes;
	uint32_t m_count;
	uint32_t m_limit;
};

/*
 * Draws the panel and bubble objects (Sony's name: the string its constructor names it by):
 * their shaders and meshes, the parameter sets they are lit by, and the draws they queue in its
 * layer; 0x1478 bytes.
 */
class PanelRenderer {
public:
	/* what a solid panel's draw is asked for (names ours) */
	enum DrawFlags {
		DRAW_TOP = 1,
		DRAW_BLEND = 2
	};

	/* the key a draw is sorted by in its layer, its fields from the most significant: the
	   draws of one state follow its begin and come before its end (names ours) */
	union DrawKey {
		struct {
			uint32_t m_reserved : 2;
			uint32_t m_material : 8;
			uint32_t m_order3 : 8;
			uint32_t m_order2 : 8;
			uint32_t m_order1 : 2;
			uint32_t m_order0 : 2;
			uint32_t m_type : 2;
		};
		uint32_t m_word;
	};

	/* 0x810bc564 */
	PanelRenderer();
	/* 0x810bcd6e */
	~PanelRenderer();

	/* 0x810bdca8: the cameras of the 3D and 2D views, the background and the draw calls */
	void Init(common::SharedPtr<Camera> camera, common::SharedPtr<Camera> camera_2d,
		  common::SharedPtr<Background> background, common::SharedPtr<DrawEnv> draw_env);
	/* 0x810bdef0 */
	void Update();
	/* 0x810bdfa4: a panel queued in the solid layer: its draws, after the programs of its
	   kind and material are begun, without blending while it is opaque (names ours) */
	void AddSolid3x3Panel(const PanelParam &param, int32_t kind, uint32_t material);
	/* 0x810be4b6: a bubble queued in a layer, after the bubbles' vertex program and the
	   fragment program of its kind (names ours) */
	void AddBubble(const BubbleParam &param, int32_t kind, uint32_t material,
		       DrawLayer *const &layer);
	/* 0x810beba8: a bubble object queued as a bubble (0) or a folder (1), in the layer or
	   else the opaque bubbles' (names ours) */
	void AddBubbleObj(BubbleParam *param, int32_t type, WaveRenderer *wave_renderer,
			  DrawLayer *layer);
	/* 0x810bec18, 0x810becee: the bubbles' light, packed for the lighting programs'
	   PointLightPack1 and PointLightPack2 (names ours) */
	math::matrix GetPointLightPack1();
	math::matrix GetPointLightPack2();
	/* 0x810bed3e: the debug layer's draw, empty in this build (name ours) */
	void DrawDebug();
	/* 0x810bf36c: its resources loaded from the folder */
	void Load(const string &dir);

	/* 0x810ba384: the material of a kind of panel (names ours from here) */
	MaterialParamSet *GetMaterial(uint32_t material);
	/* 0x810ba3b8: the screen panels are drawn on */
	void CreateScreen();
	/* 0x810ba45a: the GPU's state for a kind of panel, blended or not */
	void SetRenderStates(const int32_t &kind, bool blend);
	/* 0x810ba50e: an empty member nothing calls */
	void Unused0();
	/* 0x810ba610: a panel's 3x3 grid drawn solid (Sony's marker "draw_solid_3x3_panel") */
	void DrawSolid3x3Panel(const int32_t &kind, const uint32_t &material, const int32_t &flags);

	/* 0x810bb0e0: a panel drawn with the programs DrawSolid3x3Panel began: its top, or its
	   borders and the tops of the sides the parameters leave open */
	void DrawPanel(const int32_t &kind, const PanelParam &param, const bool &top);

	/* 0x810bba40: the programs DrawSolid3x3Panel began ended */
	void EndSolid3x3Panel(const int32_t &kind, const bool &top);
	/* 0x810bbaf8: the bubbles' vertex program begun for a material (Sony's marker
	   "draw_revolution_panel") */
	void DrawRevolutionPanel(const uint32_t &material);

	/* 0x810bbdf4: the bubbles' fragment program begun for a kind and material */
	void BeginBubbleFrag(const int32_t &kind, const uint32_t &material, const bool &blend);
	/* 0x810bc010: a bubble drawn with the programs begun */
	void DrawBubble(const int32_t &kind, const BubbleParam &param);
	/* 0x810bc4b2: the fragment program BeginBubbleFrag began ended */
	void EndBubbleFrag(const int32_t &kind);
	/* 0x810bc4ea: the vertex program DrawRevolutionPanel began ended */
	void EndRevolutionPanel();
	/* 0x810bc562: an empty member nothing calls */
	void Unused1();

	/* a mesh drawn, as lines when the panels are */
	void DrawMesh(WaveMesh<PanelVertex> &mesh, arl::ArlInstance *gl)
	{
		if (m_wireframe)
			mesh.DrawLines(gl);
		else if (mesh.m_index_count != 0)
			mesh.Draw(gl, mesh.m_height - 1);
	}

	/* the light of a kind of panel */
	PointLightSphereParamSet::Values *GetLight(uint32_t material)
	{
		switch (material) {
		case 0:
			return &m_bubble_light.m_set.m_values;
		case 3:
			return &m_color_chip_light.m_set.m_values;
		default:
			return &m_panel_light.m_set.m_values;
		}
	}

	/* a texture's width and height, and their reciprocals */
	static math::v4 GetTextureSize(const arl::Texture2D &texture)
	{
		sce::Vectormath::Simd::Aos::Vector2 size((float)texture.m_desc.m_width,
							 (float)texture.m_desc.m_height);

		return math::v4(sce::Vectormath::Simd::Aos::Vector4(size, recipPerElem(size)));
	}

	/* its height over its width, and 1 */
	static math::v2 GetTextureRatio(const arl::Texture2D &texture)
	{
		sce::Vectormath::Simd::Aos::Vector2 size((float)texture.m_desc.m_width,
							 (float)texture.m_desc.m_height);
		sce::Vectormath::Simd::Aos::Vector2 ratio = size / size.getX();

		return math::v2(ratio.getY(), ratio.getX());
	}

	PanelShaders *m_shaders;
	PanelMeshes m_meshes;
	ProfileMeshArray m_profiles;
	ProfileMesh m_profile;
	/* the frame of the last update, -1 before it */
	int32_t m_frame;
	ui::Screen *m_screen;
	ParamSetKeysManager m_manager;
	ParamSetKeysManager m_color_chip_manager;
	ParamSetInstance<MaterialParamSet> m_bubble_material;
	ParamSetInstance<PointLightSphereParamSet> m_bubble_light;
	ParamSetInstance<MaterialParamSet> m_panel_material;
	ParamSetInstance<MaterialParamSet> m_panel_opaque_material;
	ParamSetInstance<PointLightSphereParamSet> m_panel_light;
	ParamSetInstance<TouchWaveParamSet> m_touch_wave;
	ParamSetInstance<ShadowParamSet> m_shadow;
	ParamSetInstance<MaterialParamSet> m_color_chip_material;
	ParamSetInstance<PointLightSphereParamSet> m_color_chip_light;
	common::SharedPtr<Background> m_background;
	common::SharedPtr<Camera> m_camera;
	common::SharedPtr<Camera> m_camera_2d;
	common::SharedPtr<DrawEnv> m_draw_env;
	/* the time since the screen was last touched */
	float m_touch_time;
	/* where it was touched, from -1 to 1 on both axes */
	math::v2 m_touch_pos;
	/* set by the constructor and read nowhere (name ours) */
	bool m_unread_flag;
	/* the panels drawn as lines */
	bool m_wireframe;
	/* a word nothing in libpaf reads or writes */
	uint32_t m_reserved;
	/* the wave renderer whose parameter sets light an opaque wave panel of the same name:
	   cleared by the constructor, and set by nothing in libpaf */
	common::SharedPtr<WaveRenderer> m_wave_renderer;
	/* set by the constructor, released by the destructor and read nowhere: what it holds is
	   not known (names ours) */
	common::SharedPtr<uint8_t> m_unread_ptr2;
	intrusive_ptr<Surface> m_unread_surface;
	/* the layers the draws are queued in (Sony's names, the layers') */
	DrawLayer *m_rs_solid_3x3_panel;
	DrawLayer *m_rs_bubble_glass;
	DrawLayer *m_rs_bubble_opaque;
	DrawLayer *m_rs_panel_debug;
};

/* Sony's build keeps the push of a panel's draw out of line */
template<>
inline __attribute__((noinline)) void
DrawLayer::Push(uint32_t kind, uint32_t word,
		void (PanelRenderer::*func)(const int32_t &, const PanelParam &, const bool &),
		PanelRenderer *const &object, const int32_t &arg1, const PanelParam &arg2,
		const bool &arg3)
{
	if (m_state != 1 && m_state != 0)
		return;
	if (m_used + sizeof(DrawLayerMemberCall3<PanelRenderer, int32_t, PanelParam, bool>) >
	    m_buffer.size())
		common_internal::on_error();

	DrawLayerMemberCall3<PanelRenderer, int32_t, PanelParam, bool> *call =
		new (&m_buffer[m_used]) DrawLayerMemberCall3<PanelRenderer, int32_t, PanelParam, bool>(
			func, object, arg1, arg2, arg3);

	call->m_queued = true;
	m_entries.push_back(Entry(kind, word, call));
	m_used += sizeof(DrawLayerMemberCall3<PanelRenderer, int32_t, PanelParam, bool>);
}
}
}

#endif
