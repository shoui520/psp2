/*
 * The page-turn renderer and what it draws with: its shaders' interfaces
 * (paf/cg/drawobj/page_turn/*.vp and .fp, their parameters Sony's names), the quads its copy
 * shaders draw, the fold the page is turned about and its parameter set ("TurnPage").
 * PageTurnRenderer, the shaders' file names, the parameters' and the set's names are Sony's
 * strings; the other classes', the members' and this file's names are ours.
 */
#ifndef PAF_GRAPHICS_EFFECTS_PAGE_TURN_RENDERER_H
#define PAF_GRAPHICS_EFFECTS_PAGE_TURN_RENDERER_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_framebuffer_copy.h>
#include <graphics/arl/gxm/gxm_shader.h>
#include <graphics/arl/gxm/gxm_texture.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <graphics/color.h>
#include <graphics/drawobj/page_turn_obj.h>
#include <graphics/effects/page_turn_quad.h>
#include <graphics/effects/param.h>
#include <graphics/effects/wave_param.h>
#include <graphics/effects/wave_renderer.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <std/string>
#include <std/vector>

namespace paf {
namespace ui {
class Screen;
}

namespace graph {

/* the parameters the page-turn renderer sets on a page's vertex shader, which keeps those it
   has (names ours) */
class PageTurnVertShaderParams {
public:
	virtual ~PageTurnVertShaderParams()
	{
	}

	virtual void SetMVP(const math::matrix &value) = 0;
	virtual void SetModelMatrix(const math::matrix &value) = 0;
	virtual void SetGrep(const math::v2 &value) = 0;
	virtual void SetFoldPlane(const math::v4 &value) = 0;
	virtual void SetParams(const math::v4 &value) = 0;
	virtual void SetDeepest(const math::v4 &value) = 0;
	virtual void SetAspect(const math::v4 &value) = 0;
	virtual void SetNormalScaleZ(const float &value) = 0;
	virtual void SetEye(const math::v4 &value) = 0;
	virtual void SetMaterialPack(const math::matrix &value) = 0;
	virtual void SetPointLightPack2(const math::matrix &value) = 0;
	virtual void SetPointLightPack1(const math::matrix &value) = 0;
	virtual void SetShadow(const math::v4 &value) = 0;
	virtual void SetUVTransform(const math::v4 &value) = 0;
	virtual void SetSqueezeZ(const math::v4 &value) = 0;
	virtual void SetExpMinusTexture(const arl::TextureSource &value) = 0;
	virtual void SetExpMinusRange(const math::v2 &value) = 0;
};

/* the parameters the page-turn renderer sets on a page's fragment shader, which keeps those it
   has (names ours) */
class PageTurnFragShaderParams {
public:
	virtual ~PageTurnFragShaderParams()
	{
	}

	virtual void SetGrep(const math::v2 &value) = 0;
	virtual void SetFoldPlane(const math::v4 &value) = 0;
	virtual void SetParams(const math::v4 &value) = 0;
	virtual void SetDeepest(const math::v4 &value) = 0;
	virtual void SetAspect(const math::v4 &value) = 0;
	virtual void SetNormalScaleZ(const float &value) = 0;
	virtual void SetEye(const math::v4 &value) = 0;
	virtual void SetMaterialPack(const math::matrix &value) = 0;
	virtual void SetPointLightPack2(const math::matrix &value) = 0;
	virtual void SetPointLightPack1(const math::matrix &value) = 0;
	virtual void SetTex(const arl::TextureSource &value) = 0;
	virtual void SetCornerSize(const math::v4 &value) = 0;
	virtual void SetCornerTex(const arl::TextureSource &value) = 0;
	virtual void SetCornerRefract(const math::v2 &value) = 0;
	virtual void SetEmptyShadow(const float &value) = 0;
	virtual void SetEmptyColor(const math::v4 &value) = 0;
	virtual void SetEmptyShadowColor(const math::v4 &value) = 0;
	virtual void SetEmptyBackground(const arl::TextureSource &value) = 0;
	virtual void SetEmptyBackgroundUVFactor(const math::v2 &value) = 0;
	virtual void SetBackSideOpacity(const float &value) = 0;
};

/* paf/cg/drawobj/page_turn/page_turn_bottom_round.vp */
class PageTurnBottomRoundVertShader : public arl::ShaderInterfaceBase, public PageTurnVertShaderParams {
public:
	PageTurnBottomRoundVertShader()
		: m_mvp(m_setup), m_grep(m_setup), m_fold_plane(m_setup), m_params(m_setup), m_deepest(m_setup), m_aspect(m_setup), m_normal_scale_z(m_setup), m_shadow(m_setup), m_uv_transform(m_setup), m_squeeze_z(m_setup), m_exp_minus_texture(m_setup), m_exp_minus_range(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("MVP", arl::ShaderParameterBase::UNIFORM, 0);
		m_grep.init("shape.Grep", arl::ShaderParameterBase::UNIFORM, 0);
		m_fold_plane.init("shape.FoldPlane", arl::ShaderParameterBase::UNIFORM, 0);
		m_params.init("shape.Params", arl::ShaderParameterBase::UNIFORM, 0);
		m_deepest.init("shape.Deepest", arl::ShaderParameterBase::UNIFORM, 0);
		m_aspect.init("shape.Aspect", arl::ShaderParameterBase::UNIFORM, 0);
		m_normal_scale_z.init("shape.NormalScaleZ", arl::ShaderParameterBase::UNIFORM, 0);
		m_shadow.init("Shadow", arl::ShaderParameterBase::UNIFORM, 0);
		m_uv_transform.init("UVTransform", arl::ShaderParameterBase::UNIFORM, 0);
		m_squeeze_z.init("SqueezeZ", arl::ShaderParameterBase::UNIFORM, 0);
		m_exp_minus_texture.init("ExpMinusTexture", 8, 0);
		m_exp_minus_range.init("ExpMinusRange", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_grep.Apply();
		m_fold_plane.Apply();
		m_params.Apply();
		m_deepest.Apply();
		m_aspect.Apply();
		m_normal_scale_z.Apply();
		m_shadow.Apply();
		m_uv_transform.Apply();
		m_squeeze_z.Apply();
		m_exp_minus_texture.Apply();
		m_exp_minus_range.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/page_turn/page_turn_bottom_round.vp", 0x3d4);

	virtual void SetMVP(const math::matrix &value)
	{
		m_mvp.Set(value);
	}

	virtual void SetModelMatrix(const math::matrix &value)
	{
	}

	virtual void SetGrep(const math::v2 &value)
	{
		m_grep.Set(value);
	}

	virtual void SetFoldPlane(const math::v4 &value)
	{
		m_fold_plane.Set(value);
	}

	virtual void SetParams(const math::v4 &value)
	{
		m_params.Set(value);
	}

	virtual void SetDeepest(const math::v4 &value)
	{
		m_deepest.Set(value);
	}

	virtual void SetAspect(const math::v4 &value)
	{
		m_aspect.Set(value);
	}

	virtual void SetNormalScaleZ(const float &value)
	{
		m_normal_scale_z.Set(value);
	}

	virtual void SetEye(const math::v4 &value)
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

	virtual void SetShadow(const math::v4 &value)
	{
		m_shadow.Set(value);
	}

	virtual void SetUVTransform(const math::v4 &value)
	{
		m_uv_transform.Set(value);
	}

	virtual void SetSqueezeZ(const math::v4 &value)
	{
		m_squeeze_z.Set(value);
	}

	virtual void SetExpMinusTexture(const arl::TextureSource &value)
	{
		m_exp_minus_texture.Set(value);
	}

	virtual void SetExpMinusRange(const math::v2 &value)
	{
		m_exp_minus_range.Set(value);
	}

	arl::ShaderParameter<math::matrix> m_mvp;
	arl::ShaderParameter<math::v2> m_grep;
	arl::ShaderParameter<math::v4> m_fold_plane;
	arl::ShaderParameter<math::v4> m_params;
	arl::ShaderParameter<math::v4> m_deepest;
	arl::ShaderParameter<math::v4> m_aspect;
	arl::ShaderParameter<float> m_normal_scale_z;
	arl::ShaderParameter<math::v4> m_shadow;
	arl::ShaderParameter<math::v4> m_uv_transform;
	arl::ShaderParameter<math::v4> m_squeeze_z;
	arl::ShaderParameter<arl::TextureSource> m_exp_minus_texture;
	arl::ShaderParameter<math::v2> m_exp_minus_range;
};

/* paf/cg/drawobj/page_turn/page_turn_bottom_round.fp */
class PageTurnBottomRoundFragShader : public arl::ShaderInterfaceBase, public PageTurnFragShaderParams {
public:
	PageTurnBottomRoundFragShader()
		: m_tex(m_setup), m_corner_size(m_setup), m_corner_tex(m_setup), m_corner_refract(m_setup)
	{
	}

	virtual void Init()
	{
		m_tex.init("Tex", 4, 0);
		m_corner_size.init("CornerSize", arl::ShaderParameterBase::UNIFORM, 0);
		m_corner_tex.init("CornerTex", 6, 0);
		m_corner_refract.init("CornerRefract", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_tex.Apply();
		m_corner_size.Apply();
		m_corner_tex.Apply();
		m_corner_refract.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/page_turn/page_turn_bottom_round.fp", 0x20c);

	virtual void SetGrep(const math::v2 &value)
	{
	}

	virtual void SetFoldPlane(const math::v4 &value)
	{
	}

	virtual void SetParams(const math::v4 &value)
	{
	}

	virtual void SetDeepest(const math::v4 &value)
	{
	}

	virtual void SetAspect(const math::v4 &value)
	{
	}

	virtual void SetNormalScaleZ(const float &value)
	{
	}

	virtual void SetEye(const math::v4 &value)
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

	virtual void SetTex(const arl::TextureSource &value)
	{
		m_tex.Set(value);
	}

	virtual void SetCornerSize(const math::v4 &value)
	{
		m_corner_size.Set(value);
	}

	virtual void SetCornerTex(const arl::TextureSource &value)
	{
		m_corner_tex.Set(value);
	}

	virtual void SetCornerRefract(const math::v2 &value)
	{
		m_corner_refract.Set(value);
	}

	virtual void SetEmptyShadow(const float &value)
	{
	}

	virtual void SetEmptyColor(const math::v4 &value)
	{
	}

	virtual void SetEmptyShadowColor(const math::v4 &value)
	{
	}

	virtual void SetEmptyBackground(const arl::TextureSource &value)
	{
	}

	virtual void SetEmptyBackgroundUVFactor(const math::v2 &value)
	{
	}

	virtual void SetBackSideOpacity(const float &value)
	{
	}

	arl::ShaderParameter<arl::TextureSource> m_tex;
	arl::ShaderParameter<math::v4> m_corner_size;
	arl::ShaderParameter<arl::TextureSource> m_corner_tex;
	arl::ShaderParameter<math::v2> m_corner_refract;
};

/* paf/cg/drawobj/page_turn/page_turn_bottom_round_empty.vp */
class PageTurnBottomRoundEmptyVertShader : public arl::ShaderInterfaceBase, public PageTurnVertShaderParams {
public:
	PageTurnBottomRoundEmptyVertShader()
		: m_mvp(m_setup), m_grep(m_setup), m_fold_plane(m_setup), m_params(m_setup), m_deepest(m_setup), m_aspect(m_setup), m_normal_scale_z(m_setup), m_shadow(m_setup), m_uv_transform(m_setup), m_squeeze_z(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("MVP", arl::ShaderParameterBase::UNIFORM, 0);
		m_grep.init("shape.Grep", arl::ShaderParameterBase::UNIFORM, 0);
		m_fold_plane.init("shape.FoldPlane", arl::ShaderParameterBase::UNIFORM, 0);
		m_params.init("shape.Params", arl::ShaderParameterBase::UNIFORM, 0);
		m_deepest.init("shape.Deepest", arl::ShaderParameterBase::UNIFORM, 0);
		m_aspect.init("shape.Aspect", arl::ShaderParameterBase::UNIFORM, 0);
		m_normal_scale_z.init("shape.NormalScaleZ", arl::ShaderParameterBase::UNIFORM, 0);
		m_shadow.init("Shadow", arl::ShaderParameterBase::UNIFORM, 0);
		m_uv_transform.init("UVTransform", arl::ShaderParameterBase::UNIFORM, 0);
		m_squeeze_z.init("SqueezeZ", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_grep.Apply();
		m_fold_plane.Apply();
		m_params.Apply();
		m_deepest.Apply();
		m_aspect.Apply();
		m_normal_scale_z.Apply();
		m_shadow.Apply();
		m_uv_transform.Apply();
		m_squeeze_z.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/page_turn/page_turn_bottom_round_empty.vp", 0x258);

	virtual void SetMVP(const math::matrix &value)
	{
		m_mvp.Set(value);
	}

	virtual void SetModelMatrix(const math::matrix &value)
	{
	}

	virtual void SetGrep(const math::v2 &value)
	{
		m_grep.Set(value);
	}

	virtual void SetFoldPlane(const math::v4 &value)
	{
		m_fold_plane.Set(value);
	}

	virtual void SetParams(const math::v4 &value)
	{
		m_params.Set(value);
	}

	virtual void SetDeepest(const math::v4 &value)
	{
		m_deepest.Set(value);
	}

	virtual void SetAspect(const math::v4 &value)
	{
		m_aspect.Set(value);
	}

	virtual void SetNormalScaleZ(const float &value)
	{
		m_normal_scale_z.Set(value);
	}

	virtual void SetEye(const math::v4 &value)
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

	virtual void SetShadow(const math::v4 &value)
	{
		m_shadow.Set(value);
	}

	virtual void SetUVTransform(const math::v4 &value)
	{
		m_uv_transform.Set(value);
	}

	virtual void SetSqueezeZ(const math::v4 &value)
	{
		m_squeeze_z.Set(value);
	}

	virtual void SetExpMinusTexture(const arl::TextureSource &value)
	{
	}

	virtual void SetExpMinusRange(const math::v2 &value)
	{
	}

	arl::ShaderParameter<math::matrix> m_mvp;
	arl::ShaderParameter<math::v2> m_grep;
	arl::ShaderParameter<math::v4> m_fold_plane;
	arl::ShaderParameter<math::v4> m_params;
	arl::ShaderParameter<math::v4> m_deepest;
	arl::ShaderParameter<math::v4> m_aspect;
	arl::ShaderParameter<float> m_normal_scale_z;
	arl::ShaderParameter<math::v4> m_shadow;
	arl::ShaderParameter<math::v4> m_uv_transform;
	arl::ShaderParameter<math::v4> m_squeeze_z;
};

/* paf/cg/drawobj/page_turn/page_turn_bottom_round_empty.fp */
class PageTurnBottomRoundEmptyFragShader : public arl::ShaderInterfaceBase, public PageTurnFragShaderParams {
public:
	PageTurnBottomRoundEmptyFragShader()
		: m_corner_size(m_setup), m_corner_tex(m_setup), m_empty_shadow(m_setup), m_empty_color(m_setup), m_empty_shadow_color(m_setup), m_empty_background(m_setup), m_empty_background_uv_factor(m_setup)
	{
	}

	virtual void Init()
	{
		m_corner_size.init("CornerSize", arl::ShaderParameterBase::UNIFORM, 0);
		m_corner_tex.init("CornerTex", 6, 0);
		m_empty_shadow.init("EmptyShadow", arl::ShaderParameterBase::UNIFORM, 0);
		m_empty_color.init("EmptyColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_empty_shadow_color.init("EmptyShadowColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_empty_background.init("EmptyBackground", 11, 0);
		m_empty_background_uv_factor.init("EmptyBackgroundUVFactor", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_corner_size.Apply();
		m_corner_tex.Apply();
		m_empty_shadow.Apply();
		m_empty_color.Apply();
		m_empty_shadow_color.Apply();
		m_empty_background.Apply();
		m_empty_background_uv_factor.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/page_turn/page_turn_bottom_round_empty.fp", 0x2bc);

	virtual void SetGrep(const math::v2 &value)
	{
	}

	virtual void SetFoldPlane(const math::v4 &value)
	{
	}

	virtual void SetParams(const math::v4 &value)
	{
	}

	virtual void SetDeepest(const math::v4 &value)
	{
	}

	virtual void SetAspect(const math::v4 &value)
	{
	}

	virtual void SetNormalScaleZ(const float &value)
	{
	}

	virtual void SetEye(const math::v4 &value)
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

	virtual void SetTex(const arl::TextureSource &value)
	{
	}

	virtual void SetCornerSize(const math::v4 &value)
	{
		m_corner_size.Set(value);
	}

	virtual void SetCornerTex(const arl::TextureSource &value)
	{
		m_corner_tex.Set(value);
	}

	virtual void SetCornerRefract(const math::v2 &value)
	{
	}

	virtual void SetEmptyShadow(const float &value)
	{
		m_empty_shadow.Set(value);
	}

	virtual void SetEmptyColor(const math::v4 &value)
	{
		m_empty_color.Set(value);
	}

	virtual void SetEmptyShadowColor(const math::v4 &value)
	{
		m_empty_shadow_color.Set(value);
	}

	virtual void SetEmptyBackground(const arl::TextureSource &value)
	{
		m_empty_background.Set(value);
	}

	virtual void SetEmptyBackgroundUVFactor(const math::v2 &value)
	{
		m_empty_background_uv_factor.Set(value);
	}

	virtual void SetBackSideOpacity(const float &value)
	{
	}

	arl::ShaderParameter<math::v4> m_corner_size;
	arl::ShaderParameter<arl::TextureSource> m_corner_tex;
	arl::ShaderParameter<float> m_empty_shadow;
	arl::ShaderParameter<math::v4> m_empty_color;
	arl::ShaderParameter<math::v4> m_empty_shadow_color;
	arl::ShaderParameter<arl::TextureSource> m_empty_background;
	arl::ShaderParameter<math::v2> m_empty_background_uv_factor;
};

/* paf/cg/drawobj/page_turn/page_turn_top_round.vp */
class PageTurnTopRoundVertShader : public arl::ShaderInterfaceBase, public PageTurnVertShaderParams {
public:
	PageTurnTopRoundVertShader()
		: m_mvp(m_setup), m_model_matrix(m_setup), m_grep(m_setup), m_fold_plane(m_setup), m_params(m_setup), m_deepest(m_setup), m_aspect(m_setup), m_normal_scale_z(m_setup), m_eye(m_setup), m_material_pack(m_setup), m_point_light_pack2(m_setup), m_point_light_pack1(m_setup), m_uv_transform(m_setup), m_squeeze_z(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("MVP", arl::ShaderParameterBase::UNIFORM, 0);
		m_model_matrix.init("ModelMatrix", arl::ShaderParameterBase::UNIFORM, 0);
		m_grep.init("shape.Grep", arl::ShaderParameterBase::UNIFORM, 0);
		m_fold_plane.init("shape.FoldPlane", arl::ShaderParameterBase::UNIFORM, 0);
		m_params.init("shape.Params", arl::ShaderParameterBase::UNIFORM, 0);
		m_deepest.init("shape.Deepest", arl::ShaderParameterBase::UNIFORM, 0);
		m_aspect.init("shape.Aspect", arl::ShaderParameterBase::UNIFORM, 0);
		m_normal_scale_z.init("shape.NormalScaleZ", arl::ShaderParameterBase::UNIFORM, 0);
		m_eye.init("lit_params.Eye", arl::ShaderParameterBase::UNIFORM, 0);
		m_material_pack.init("lit_params.MaterialPack", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack2.init("lit_params.PointLightPack2", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack1.init("lit_params.PointLightPack1", arl::ShaderParameterBase::UNIFORM, 0);
		m_uv_transform.init("UVTransform", arl::ShaderParameterBase::UNIFORM, 0);
		m_squeeze_z.init("SqueezeZ", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_model_matrix.Apply();
		m_grep.Apply();
		m_fold_plane.Apply();
		m_params.Apply();
		m_deepest.Apply();
		m_aspect.Apply();
		m_normal_scale_z.Apply();
		m_eye.Apply();
		m_material_pack.Apply();
		m_point_light_pack2.Apply();
		m_point_light_pack1.Apply();
		m_uv_transform.Apply();
		m_squeeze_z.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/page_turn/page_turn_top_round.vp", 0x678);

	virtual void SetMVP(const math::matrix &value)
	{
		m_mvp.Set(value);
	}

	virtual void SetModelMatrix(const math::matrix &value)
	{
		m_model_matrix.Set(value);
	}

	virtual void SetGrep(const math::v2 &value)
	{
		m_grep.Set(value);
	}

	virtual void SetFoldPlane(const math::v4 &value)
	{
		m_fold_plane.Set(value);
	}

	virtual void SetParams(const math::v4 &value)
	{
		m_params.Set(value);
	}

	virtual void SetDeepest(const math::v4 &value)
	{
		m_deepest.Set(value);
	}

	virtual void SetAspect(const math::v4 &value)
	{
		m_aspect.Set(value);
	}

	virtual void SetNormalScaleZ(const float &value)
	{
		m_normal_scale_z.Set(value);
	}

	virtual void SetEye(const math::v4 &value)
	{
		m_eye.Set(value);
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

	virtual void SetShadow(const math::v4 &value)
	{
	}

	virtual void SetUVTransform(const math::v4 &value)
	{
		m_uv_transform.Set(value);
	}

	virtual void SetSqueezeZ(const math::v4 &value)
	{
		m_squeeze_z.Set(value);
	}

	virtual void SetExpMinusTexture(const arl::TextureSource &value)
	{
	}

	virtual void SetExpMinusRange(const math::v2 &value)
	{
	}

	arl::ShaderParameter<math::matrix> m_mvp;
	arl::ShaderParameter<math::matrix> m_model_matrix;
	arl::ShaderParameter<math::v2> m_grep;
	arl::ShaderParameter<math::v4> m_fold_plane;
	arl::ShaderParameter<math::v4> m_params;
	arl::ShaderParameter<math::v4> m_deepest;
	arl::ShaderParameter<math::v4> m_aspect;
	arl::ShaderParameter<float> m_normal_scale_z;
	arl::ShaderParameter<math::v4> m_eye;
	arl::ShaderParameter<math::matrix> m_material_pack;
	arl::ShaderParameter<math::matrix> m_point_light_pack2;
	arl::ShaderParameter<math::matrix> m_point_light_pack1;
	arl::ShaderParameter<math::v4> m_uv_transform;
	arl::ShaderParameter<math::v4> m_squeeze_z;
};

/* paf/cg/drawobj/page_turn/page_turn_top_round.fp */
class PageTurnTopRoundFragShader : public arl::ShaderInterfaceBase, public PageTurnFragShaderParams {
public:
	PageTurnTopRoundFragShader()
		: m_material_pack(m_setup), m_point_light_pack2(m_setup), m_tex(m_setup), m_corner_size(m_setup), m_corner_tex(m_setup), m_back_side_opacity(m_setup)
	{
	}

	virtual void Init()
	{
		m_material_pack.init("lit_params.MaterialPack", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack2.init("lit_params.PointLightPack2", arl::ShaderParameterBase::UNIFORM, 0);
		m_tex.init("Tex", 4, 0);
		m_corner_size.init("CornerSize", arl::ShaderParameterBase::UNIFORM, 0);
		m_corner_tex.init("CornerTex", 6, 0);
		m_back_side_opacity.init("BackSideOpacity", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_material_pack.Apply();
		m_point_light_pack2.Apply();
		m_tex.Apply();
		m_corner_size.Apply();
		m_corner_tex.Apply();
		m_back_side_opacity.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/page_turn/page_turn_top_round.fp", 0x258);

	virtual void SetGrep(const math::v2 &value)
	{
	}

	virtual void SetFoldPlane(const math::v4 &value)
	{
	}

	virtual void SetParams(const math::v4 &value)
	{
	}

	virtual void SetDeepest(const math::v4 &value)
	{
	}

	virtual void SetAspect(const math::v4 &value)
	{
	}

	virtual void SetNormalScaleZ(const float &value)
	{
	}

	virtual void SetEye(const math::v4 &value)
	{
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
	}

	virtual void SetTex(const arl::TextureSource &value)
	{
		m_tex.Set(value);
	}

	virtual void SetCornerSize(const math::v4 &value)
	{
		m_corner_size.Set(value);
	}

	virtual void SetCornerTex(const arl::TextureSource &value)
	{
		m_corner_tex.Set(value);
	}

	virtual void SetCornerRefract(const math::v2 &value)
	{
	}

	virtual void SetEmptyShadow(const float &value)
	{
	}

	virtual void SetEmptyColor(const math::v4 &value)
	{
	}

	virtual void SetEmptyShadowColor(const math::v4 &value)
	{
	}

	virtual void SetEmptyBackground(const arl::TextureSource &value)
	{
	}

	virtual void SetEmptyBackgroundUVFactor(const math::v2 &value)
	{
	}

	virtual void SetBackSideOpacity(const float &value)
	{
		m_back_side_opacity.Set(value);
	}

	arl::ShaderParameter<math::matrix> m_material_pack;
	arl::ShaderParameter<math::matrix> m_point_light_pack2;
	arl::ShaderParameter<arl::TextureSource> m_tex;
	arl::ShaderParameter<math::v4> m_corner_size;
	arl::ShaderParameter<arl::TextureSource> m_corner_tex;
	arl::ShaderParameter<float> m_back_side_opacity;
};

/* paf/cg/drawobj/page_turn/page_turn_bottom.vp */
class PageTurnBottomVertShader : public arl::ShaderInterfaceBase, public PageTurnVertShaderParams {
public:
	PageTurnBottomVertShader()
		: m_mvp(m_setup), m_grep(m_setup), m_fold_plane(m_setup), m_params(m_setup), m_deepest(m_setup), m_aspect(m_setup), m_normal_scale_z(m_setup), m_shadow(m_setup), m_uv_transform(m_setup), m_squeeze_z(m_setup), m_exp_minus_texture(m_setup), m_exp_minus_range(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("MVP", arl::ShaderParameterBase::UNIFORM, 0);
		m_grep.init("shape.Grep", arl::ShaderParameterBase::UNIFORM, 0);
		m_fold_plane.init("shape.FoldPlane", arl::ShaderParameterBase::UNIFORM, 0);
		m_params.init("shape.Params", arl::ShaderParameterBase::UNIFORM, 0);
		m_deepest.init("shape.Deepest", arl::ShaderParameterBase::UNIFORM, 0);
		m_aspect.init("shape.Aspect", arl::ShaderParameterBase::UNIFORM, 0);
		m_normal_scale_z.init("shape.NormalScaleZ", arl::ShaderParameterBase::UNIFORM, 0);
		m_shadow.init("Shadow", arl::ShaderParameterBase::UNIFORM, 0);
		m_uv_transform.init("UVTransform", arl::ShaderParameterBase::UNIFORM, 0);
		m_squeeze_z.init("SqueezeZ", arl::ShaderParameterBase::UNIFORM, 0);
		m_exp_minus_texture.init("ExpMinusTexture", 8, 0);
		m_exp_minus_range.init("ExpMinusRange", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_grep.Apply();
		m_fold_plane.Apply();
		m_params.Apply();
		m_deepest.Apply();
		m_aspect.Apply();
		m_normal_scale_z.Apply();
		m_shadow.Apply();
		m_uv_transform.Apply();
		m_squeeze_z.Apply();
		m_exp_minus_texture.Apply();
		m_exp_minus_range.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/page_turn/page_turn_bottom.vp", 0x3b4);

	virtual void SetMVP(const math::matrix &value)
	{
		m_mvp.Set(value);
	}

	virtual void SetModelMatrix(const math::matrix &value)
	{
	}

	virtual void SetGrep(const math::v2 &value)
	{
		m_grep.Set(value);
	}

	virtual void SetFoldPlane(const math::v4 &value)
	{
		m_fold_plane.Set(value);
	}

	virtual void SetParams(const math::v4 &value)
	{
		m_params.Set(value);
	}

	virtual void SetDeepest(const math::v4 &value)
	{
		m_deepest.Set(value);
	}

	virtual void SetAspect(const math::v4 &value)
	{
		m_aspect.Set(value);
	}

	virtual void SetNormalScaleZ(const float &value)
	{
		m_normal_scale_z.Set(value);
	}

	virtual void SetEye(const math::v4 &value)
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

	virtual void SetShadow(const math::v4 &value)
	{
		m_shadow.Set(value);
	}

	virtual void SetUVTransform(const math::v4 &value)
	{
		m_uv_transform.Set(value);
	}

	virtual void SetSqueezeZ(const math::v4 &value)
	{
		m_squeeze_z.Set(value);
	}

	virtual void SetExpMinusTexture(const arl::TextureSource &value)
	{
		m_exp_minus_texture.Set(value);
	}

	virtual void SetExpMinusRange(const math::v2 &value)
	{
		m_exp_minus_range.Set(value);
	}

	arl::ShaderParameter<math::matrix> m_mvp;
	arl::ShaderParameter<math::v2> m_grep;
	arl::ShaderParameter<math::v4> m_fold_plane;
	arl::ShaderParameter<math::v4> m_params;
	arl::ShaderParameter<math::v4> m_deepest;
	arl::ShaderParameter<math::v4> m_aspect;
	arl::ShaderParameter<float> m_normal_scale_z;
	arl::ShaderParameter<math::v4> m_shadow;
	arl::ShaderParameter<math::v4> m_uv_transform;
	arl::ShaderParameter<math::v4> m_squeeze_z;
	arl::ShaderParameter<arl::TextureSource> m_exp_minus_texture;
	arl::ShaderParameter<math::v2> m_exp_minus_range;
};

/* paf/cg/drawobj/page_turn/page_turn_bottom.fp */
class PageTurnBottomFragShader : public arl::ShaderInterfaceBase, public PageTurnFragShaderParams {
public:
	PageTurnBottomFragShader()
		: m_tex(m_setup)
	{
	}

	virtual void Init()
	{
		m_tex.init("Tex", 4, 0);
	}

	virtual void Apply()
	{
		m_tex.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/page_turn/page_turn_bottom.fp", 0x108);

	virtual void SetGrep(const math::v2 &value)
	{
	}

	virtual void SetFoldPlane(const math::v4 &value)
	{
	}

	virtual void SetParams(const math::v4 &value)
	{
	}

	virtual void SetDeepest(const math::v4 &value)
	{
	}

	virtual void SetAspect(const math::v4 &value)
	{
	}

	virtual void SetNormalScaleZ(const float &value)
	{
	}

	virtual void SetEye(const math::v4 &value)
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

	virtual void SetTex(const arl::TextureSource &value)
	{
		m_tex.Set(value);
	}

	virtual void SetCornerSize(const math::v4 &value)
	{
	}

	virtual void SetCornerTex(const arl::TextureSource &value)
	{
	}

	virtual void SetCornerRefract(const math::v2 &value)
	{
	}

	virtual void SetEmptyShadow(const float &value)
	{
	}

	virtual void SetEmptyColor(const math::v4 &value)
	{
	}

	virtual void SetEmptyShadowColor(const math::v4 &value)
	{
	}

	virtual void SetEmptyBackground(const arl::TextureSource &value)
	{
	}

	virtual void SetEmptyBackgroundUVFactor(const math::v2 &value)
	{
	}

	virtual void SetBackSideOpacity(const float &value)
	{
	}

	arl::ShaderParameter<arl::TextureSource> m_tex;
};

/* paf/cg/drawobj/page_turn/page_turn_top.vp */
class PageTurnTopVertShader : public arl::ShaderInterfaceBase, public PageTurnVertShaderParams {
public:
	PageTurnTopVertShader()
		: m_mvp(m_setup), m_model_matrix(m_setup), m_grep(m_setup), m_fold_plane(m_setup), m_params(m_setup), m_deepest(m_setup), m_aspect(m_setup), m_normal_scale_z(m_setup), m_eye(m_setup), m_material_pack(m_setup), m_point_light_pack2(m_setup), m_point_light_pack1(m_setup), m_uv_transform(m_setup), m_squeeze_z(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("MVP", arl::ShaderParameterBase::UNIFORM, 0);
		m_model_matrix.init("ModelMatrix", arl::ShaderParameterBase::UNIFORM, 0);
		m_grep.init("shape.Grep", arl::ShaderParameterBase::UNIFORM, 0);
		m_fold_plane.init("shape.FoldPlane", arl::ShaderParameterBase::UNIFORM, 0);
		m_params.init("shape.Params", arl::ShaderParameterBase::UNIFORM, 0);
		m_deepest.init("shape.Deepest", arl::ShaderParameterBase::UNIFORM, 0);
		m_aspect.init("shape.Aspect", arl::ShaderParameterBase::UNIFORM, 0);
		m_normal_scale_z.init("shape.NormalScaleZ", arl::ShaderParameterBase::UNIFORM, 0);
		m_eye.init("lit_params.Eye", arl::ShaderParameterBase::UNIFORM, 0);
		m_material_pack.init("lit_params.MaterialPack", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack2.init("lit_params.PointLightPack2", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack1.init("lit_params.PointLightPack1", arl::ShaderParameterBase::UNIFORM, 0);
		m_uv_transform.init("UVTransform", arl::ShaderParameterBase::UNIFORM, 0);
		m_squeeze_z.init("SqueezeZ", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_model_matrix.Apply();
		m_grep.Apply();
		m_fold_plane.Apply();
		m_params.Apply();
		m_deepest.Apply();
		m_aspect.Apply();
		m_normal_scale_z.Apply();
		m_eye.Apply();
		m_material_pack.Apply();
		m_point_light_pack2.Apply();
		m_point_light_pack1.Apply();
		m_uv_transform.Apply();
		m_squeeze_z.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/page_turn/page_turn_top.vp", 0x610);

	virtual void SetMVP(const math::matrix &value)
	{
		m_mvp.Set(value);
	}

	virtual void SetModelMatrix(const math::matrix &value)
	{
		m_model_matrix.Set(value);
	}

	virtual void SetGrep(const math::v2 &value)
	{
		m_grep.Set(value);
	}

	virtual void SetFoldPlane(const math::v4 &value)
	{
		m_fold_plane.Set(value);
	}

	virtual void SetParams(const math::v4 &value)
	{
		m_params.Set(value);
	}

	virtual void SetDeepest(const math::v4 &value)
	{
		m_deepest.Set(value);
	}

	virtual void SetAspect(const math::v4 &value)
	{
		m_aspect.Set(value);
	}

	virtual void SetNormalScaleZ(const float &value)
	{
		m_normal_scale_z.Set(value);
	}

	virtual void SetEye(const math::v4 &value)
	{
		m_eye.Set(value);
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

	virtual void SetShadow(const math::v4 &value)
	{
	}

	virtual void SetUVTransform(const math::v4 &value)
	{
		m_uv_transform.Set(value);
	}

	virtual void SetSqueezeZ(const math::v4 &value)
	{
		m_squeeze_z.Set(value);
	}

	virtual void SetExpMinusTexture(const arl::TextureSource &value)
	{
	}

	virtual void SetExpMinusRange(const math::v2 &value)
	{
	}

	arl::ShaderParameter<math::matrix> m_mvp;
	arl::ShaderParameter<math::matrix> m_model_matrix;
	arl::ShaderParameter<math::v2> m_grep;
	arl::ShaderParameter<math::v4> m_fold_plane;
	arl::ShaderParameter<math::v4> m_params;
	arl::ShaderParameter<math::v4> m_deepest;
	arl::ShaderParameter<math::v4> m_aspect;
	arl::ShaderParameter<float> m_normal_scale_z;
	arl::ShaderParameter<math::v4> m_eye;
	arl::ShaderParameter<math::matrix> m_material_pack;
	arl::ShaderParameter<math::matrix> m_point_light_pack2;
	arl::ShaderParameter<math::matrix> m_point_light_pack1;
	arl::ShaderParameter<math::v4> m_uv_transform;
	arl::ShaderParameter<math::v4> m_squeeze_z;
};

/* paf/cg/drawobj/page_turn/page_turn_top.fp */
class PageTurnTopFragShader : public arl::ShaderInterfaceBase, public PageTurnFragShaderParams {
public:
	PageTurnTopFragShader()
		: m_material_pack(m_setup), m_point_light_pack2(m_setup), m_tex(m_setup), m_back_side_opacity(m_setup)
	{
	}

	virtual void Init()
	{
		m_material_pack.init("lit_params.MaterialPack", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack2.init("lit_params.PointLightPack2", arl::ShaderParameterBase::UNIFORM, 0);
		m_tex.init("Tex", 4, 0);
		m_back_side_opacity.init("BackSideOpacity", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_material_pack.Apply();
		m_point_light_pack2.Apply();
		m_tex.Apply();
		m_back_side_opacity.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/page_turn/page_turn_top.fp", 0x1bc);

	virtual void SetGrep(const math::v2 &value)
	{
	}

	virtual void SetFoldPlane(const math::v4 &value)
	{
	}

	virtual void SetParams(const math::v4 &value)
	{
	}

	virtual void SetDeepest(const math::v4 &value)
	{
	}

	virtual void SetAspect(const math::v4 &value)
	{
	}

	virtual void SetNormalScaleZ(const float &value)
	{
	}

	virtual void SetEye(const math::v4 &value)
	{
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
	}

	virtual void SetTex(const arl::TextureSource &value)
	{
		m_tex.Set(value);
	}

	virtual void SetCornerSize(const math::v4 &value)
	{
	}

	virtual void SetCornerTex(const arl::TextureSource &value)
	{
	}

	virtual void SetCornerRefract(const math::v2 &value)
	{
	}

	virtual void SetEmptyShadow(const float &value)
	{
	}

	virtual void SetEmptyColor(const math::v4 &value)
	{
	}

	virtual void SetEmptyShadowColor(const math::v4 &value)
	{
	}

	virtual void SetEmptyBackground(const arl::TextureSource &value)
	{
	}

	virtual void SetEmptyBackgroundUVFactor(const math::v2 &value)
	{
	}

	virtual void SetBackSideOpacity(const float &value)
	{
		m_back_side_opacity.Set(value);
	}

	arl::ShaderParameter<math::matrix> m_material_pack;
	arl::ShaderParameter<math::matrix> m_point_light_pack2;
	arl::ShaderParameter<arl::TextureSource> m_tex;
	arl::ShaderParameter<float> m_back_side_opacity;
};

/* paf/cg/drawobj/page_turn/quad.vp */
class PageTurnQuadVertShader : public arl::ShaderInterfaceBase {
public:
	PageTurnQuadVertShader()
		: m_mvp(m_setup), m_uv_transform(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("MVP", arl::ShaderParameterBase::UNIFORM, 0);
		m_uv_transform.init("UVtransform", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_uv_transform.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/page_turn/quad.vp", 0x158);

	arl::ShaderParameter<math::matrix> m_mvp;
	arl::ShaderParameter<math::v4> m_uv_transform;
};

/* paf/cg/drawobj/page_turn/pure_copy.fp */
class PageTurnPureCopyFragShader : public arl::ShaderInterfaceBase {
public:
	PageTurnPureCopyFragShader()
		: m_source(m_setup)
	{
	}

	virtual void Init()
	{
		m_source.init("Source", 1, 0);
	}

	virtual void Apply()
	{
		m_source.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/page_turn/pure_copy.fp", 0x104);

	arl::ShaderParameter<arl::TextureSource> m_source;
};

/* paf/cg/drawobj/page_turn/page_turn_shadow.fp */
class PageTurnShadowFragShader : public arl::ShaderInterfaceBase {
public:
	PageTurnShadowFragShader()
		: m_tex(m_setup), m_border(m_setup), m_level(m_setup), m_color(m_setup)
	{
	}

	virtual void Init()
	{
		m_tex.init("Tex", 1, 0);
		m_border.init("Border", arl::ShaderParameterBase::UNIFORM, 0);
		m_level.init("Level", arl::ShaderParameterBase::UNIFORM, 0);
		m_color.init("Color", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_tex.Apply();
		m_border.Apply();
		m_level.Apply();
		m_color.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/page_turn/page_turn_shadow.fp", 0x1c4);

	arl::ShaderParameter<arl::TextureSource> m_tex;
	arl::ShaderParameter<math::v4> m_border;
	arl::ShaderParameter<float> m_level;
	arl::ShaderParameter<math::v4> m_color;
};

/* a shader's parameters, to be set, and its shader object, as the renderer picks them for a
   pass (names ours) */
template<class Params, class Object>
class PageTurnShaderPair {
public:
	Params *m_params;
	Object *m_object;
};

typedef PageTurnShaderPair<PageTurnVertShaderParams, arl::VertShaderObject> PageTurnVertShaderPair;
typedef PageTurnShaderPair<PageTurnFragShaderParams, arl::FragShaderObject> PageTurnFragShaderPair;

/*
 * Vertices for a strip of quads, made once with room for twice the count asked for, under a
 * name (names ours); 0x50 bytes. The renderer makes one with forty and draws nothing with it;
 * the members it leaves alone are read and written by nothing in libpaf.
 */
class PageTurnStripBuffer {
public:
	PageTurnStripBuffer()
		: m_count(0), m_unused0(0), m_unused1(0), m_unused2(0), m_unused3(0), m_unused_flag(false),
		  m_unused4(0), m_initialized(false)
	{
	}

	/* 0x81215c38: false when it was made before */
	bool Init(common::SharedPtr<arl::VramAllocator> allocator, uint32_t count,
		  common::SharedPtr<arl::VertexArraySpec> spec, const char *name)
	{
		if (m_initialized)
			return m_initialized;
		m_name = name;
		m_count = count;
		m_vertices = arl::VertexArray(allocator, spec, m_count * 2);
		m_initialized = true;
		return true;
	}

	uint32_t m_count;
	uint32_t m_unused0;
	uint32_t m_unused1;
	arl::VertexArray m_vertices;
	uint32_t m_unused2;
	float m_unused_value;
	uint32_t m_unused3;
	bool m_unused_flag;
	uint32_t m_unused4;
	string m_name;
	bool m_initialized;
	common::SharedPtr<arl::VertexArray> m_unused_ptr;
	math::v2 m_unused_point;
};

/* the page-turn renderer's shaders and the quads its copy shaders draw (names ours); 0x17d0
   bytes, made by 0x81214608 and freed by 0x81216424 */
class PageTurnShaders {
public:
	arl::ShaderInstance<PageTurnBottomRoundVertShader, arl::VertShaderObject> m_bottom_round_vert;
	arl::ShaderInstance<PageTurnBottomRoundFragShader, arl::FragShaderObject> m_bottom_round_frag;
	arl::ShaderInstance<PageTurnBottomRoundEmptyVertShader, arl::VertShaderObject>
		m_bottom_round_empty_vert;
	arl::ShaderInstance<PageTurnBottomRoundEmptyFragShader, arl::FragShaderObject>
		m_bottom_round_empty_frag;
	arl::ShaderInstance<PageTurnTopRoundVertShader, arl::VertShaderObject> m_top_round_vert;
	arl::ShaderInstance<PageTurnTopRoundFragShader, arl::FragShaderObject> m_top_round_frag;
	arl::ShaderInstance<PageTurnBottomVertShader, arl::VertShaderObject> m_bottom_vert;
	arl::ShaderInstance<PageTurnBottomFragShader, arl::FragShaderObject> m_bottom_frag;
	arl::ShaderInstance<PageTurnTopVertShader, arl::VertShaderObject> m_top_vert;
	arl::ShaderInstance<PageTurnTopFragShader, arl::FragShaderObject> m_top_frag;
	PageTurnQuad m_quad;
	PageTurnQuad m_quad_ndc;
	PageTurnStripBuffer m_strips;
	arl::ShaderInstance<PageTurnQuadVertShader, arl::VertShaderObject> m_quad_vert;
	arl::ShaderInstance<PageTurnPureCopyFragShader, arl::FragShaderObject> m_pure_copy_frag;
	arl::ShaderInstance<PageTurnShadowFragShader, arl::FragShaderObject> m_shadow_frag;
};

/* how a page is turned and lit */
class TurnPageParamSet : public ParamSet, public ParamLink {
public:
	class Values {
	public:
		Values(ParamSet *set)
			: m_top_scale(set, "top_scale", 0.1f, 0.0f, 5.0f, PARAM_FLAG_FINE),
			  m_top_curl(set, "top_curl", 1.0f, 0.0f, 1.0f, 0),
			  m_back_opacity(set, "back_opacity", 0.2f, 0.0f, 1.0f, 0),
			  m_bottom_scale(set, "bottom_scale", 0.032f, 0.0f, 0.25f, 0),
			  m_shadow(set, "shadow", 200.0f, 0.0f, 500.0f, 0),
			  m_shadow_level(set, "shadow_level", 0.2f, 0.0f, 1.0f, 0),
			  m_normal_scale_z(set, "normal_scale_z", 1.0f, 0.0f, 5.0f, 0),
			  m_squeeze_z(set, "squeeze_z", 1.0f, 0.0f, 1.0f, 0),
			  m_empty_shadow(set, "empty_shadow", 17.4f, 0.0f, 300.0f, PARAM_FLAG_FINE),
			  m_empty_shadow_level(set, "empty_shadow_level", 0.61f, 0.0f, 1.0f, 0),
			  m_empty_corner_shadow(set, "empty_corner_shadow", 0.14f, 0.0f, 1.0f,
						PARAM_FLAG_FINE),
			  m_empty_color(set, "empty_color",
					math::v3(sce::Vectormath::Simd::Aos::Vector3(s_color_white.getImpl().get128())),
					math::v3(sce::Vectormath::Simd::Aos::Vector3(s_color_black.getImpl().get128())),
					math::v3(sce::Vectormath::Simd::Aos::Vector3(s_color_white.getImpl().get128())),
					PARAM_FLAG_COLOR),
			  m_empty_shadow_color(set, "empty_shadow_color",
					       math::v3(sce::Vectormath::Simd::Aos::Vector3(s_color_white.getImpl().get128())),
					       math::v3(sce::Vectormath::Simd::Aos::Vector3(s_color_black.getImpl().get128())),
					       math::v3(sce::Vectormath::Simd::Aos::Vector3(s_color_white.getImpl().get128())),
					       PARAM_FLAG_COLOR),
			  m_occluder_shadow_sharpness(set, "occluder_shadow_sharpness", 0.45f, 0.0f,
						      1.0f, PARAM_FLAG_FINE),
			  m_occluder_shadow_level(set, "occluder_shadow_level", 0.5f, 0.0f, 1.0f, 0),
			  m_occluder_shadow_color(set, "occluder_shadow_color", s_color_white,
						  s_color_black, s_color_white, 0)
		{
		}

		/* every value set as the parameter's own set would */
		Values &operator=(const Values &other)
		{
			m_top_scale = other.m_top_scale;
			m_top_curl = other.m_top_curl;
			m_back_opacity = other.m_back_opacity;
			m_bottom_scale = other.m_bottom_scale;
			m_shadow = other.m_shadow;
			m_shadow_level = other.m_shadow_level;
			m_normal_scale_z = other.m_normal_scale_z;
			m_squeeze_z = other.m_squeeze_z;
			m_empty_shadow = other.m_empty_shadow;
			m_empty_shadow_level = other.m_empty_shadow_level;
			m_empty_corner_shadow = other.m_empty_corner_shadow;
			m_empty_color = other.m_empty_color;
			m_empty_shadow_color = other.m_empty_shadow_color;
			m_occluder_shadow_sharpness = other.m_occluder_shadow_sharpness;
			m_occluder_shadow_level = other.m_occluder_shadow_level;
			m_occluder_shadow_color = other.m_occluder_shadow_color;
			return *this;
		}

		Param<float> m_top_scale;
		Param<float> m_top_curl;
		Param<float> m_back_opacity;
		Param<float> m_bottom_scale;
		Param<float> m_shadow;
		Param<float> m_shadow_level;
		Param<float> m_normal_scale_z;
		Param<float> m_squeeze_z;
		Param<float> m_empty_shadow;
		Param<float> m_empty_shadow_level;
		Param<float> m_empty_corner_shadow;
		Param<math::v3> m_empty_color;
		Param<math::v3> m_empty_shadow_color;
		Param<float> m_occluder_shadow_sharpness;
		Param<float> m_occluder_shadow_level;
		Param<math::v4> m_occluder_shadow_color;
	};

	WAVE_PARAM_SET_KEYED(TurnPageParamSet, "TurnPage", __attribute__((noinline)));
};

/* a texture of exp(-x * range) across its width, eight rows high (names ours); 0xc bytes */
class ExpMinusTexture {
public:
	/* 0x81214ee8 */
	void Init(int32_t width, float range)
	{
		m_range = range;
		m_texture = arl::CreateTexture<ImageMode_L8>(arl::Gl()->m_default_allocator,
							     arl::SurfaceSize(width, 8), NULL,
							     arl::SurfaceFlags(), 0);

		vector<uint8_t> levels;
		int32_t height = m_texture->m_desc.m_height;
		int32_t columns = m_texture->m_desc.m_width;

		for (int32_t y = 0; y != height; y++) {
			for (int32_t x = 0; x != columns; x++) {
				float level = sce_paf_expf(-((float)x / (float)(columns - 1) * m_range));

				if (level < 0.0f)
					level = 0.0f;
				levels.push_back((uint8_t)((level < 1.0f ? level : 1.0f) * 255.0f));
			}
		}

		arl::Texture2D *texture = m_texture.get();

		ScePafGraphics_309D2902(texture, 0, texture->m_desc.m_width, texture->m_desc.m_height,
					texture->m_desc.m_format, texture->m_desc.m_levels, levels.ptr);
		m_texture->setWrapS(arl::TEXTURE_WRAP_CLAMP_FULL_BORDER);
		m_texture->setWrapT(arl::TEXTURE_WRAP_CLAMP_FULL_BORDER);
	}

	common::SharedPtr<arl::Texture2D> m_texture;
	float m_range;
};

/* a point of the page projected on the fold: its foot on the fold line, where it lands once
   the page is turned, and how far in front of the fold it was (names ours); 0x18 bytes */
class PageTurnProjection {
public:
	math::v2 m_foot;
	math::v2 m_point;
	float m_distance;
};

/* the line a page is folded along, from where its corner is pulled to, and the page's corners
   projected on it (names ours); 0xe8 bytes */
class PageTurnFold {
public:
	/* 0x810caa24: worked out only when the curl changed */
	PageTurnFold(PageTurnParam param, math::v2 aspect);

	/* 0x810ca98c */
	PageTurnProjection Project(math::v2 point) const;

	math::v2 m_origin;
	math::v2 m_normal;
	/* the page's corners, scaled to its aspect */
	math::v2 m_quad[4];
	PageTurnProjection m_projections[4];
	math::v2 m_corner;
	/* the corner furthest in front of the fold, and the one furthest behind */
	PageTurnProjection m_max;
	PageTurnProjection m_min;
	/* the fold line clipped to the page; outside when it misses the page */
	math::v2 m_edge[2];
	bool m_outside;
	math::v2 m_aspect;
};

/*
 * The segment from a to b clipped to the half plane in front of the line through point with
 * the normal (names ours, 0x81213fd4): 0 when it is all in front, 3 when all behind, 1 when
 * a was moved onto the line, 2 when b was.
 */
inline __attribute__((noinline)) int32_t ClipSegment(sce::Vectormath::Simd::Aos::Vector2 &a,
			   sce::Vectormath::Simd::Aos::Vector2 &b,
			   sce::Vectormath::Simd::Aos::Vector2 point,
			   sce::Vectormath::Simd::Aos::Vector2 normal)
{
	sce::Vectormath::Simd::floatInVec a_distance = dot(normal, a - point);
	sce::Vectormath::Simd::floatInVec b_distance = dot(normal, b - point);
	bool a_in = (a_distance >= sce::Vectormath::Simd::floatInVec(0.0f)).getAsBool();
	bool b_in = (b_distance >= sce::Vectormath::Simd::floatInVec(0.0f)).getAsBool();

	if (a_in && b_in)
		return 0;
	if (!a_in && !b_in)
		return 3;

	sce::Vectormath::Simd::Aos::Vector2 cut =
		a + (b - a) * (dot(normal, (a - normal * a_distance) - a) / (b_distance - a_distance));

	if (!a_in && b_in) {
		a = cut;
		return 1;
	}
	if (a_in && !b_in) {
		b = cut;
		return 2;
	}
	return 0;
}

/* turns a page with a texture on each side (Sony's name, the identifier PageTurnRenderer);
   0xa58 bytes */
class PageTurnRenderer {
public:
	/* 0x810c86d0: a 64 by 64 texture of each texel's distance from the corner, capped at 1
	   (name ours) */
	void MakeDistanceTexture(common::SharedPtr<arl::TextureObject> &texture);
	/* 0x810c89fc: its resources loaded from the folder */
	PageTurnRenderer(const string &dir);
	/* 0x810ca076 */
	~PageTurnRenderer();
	/* 0x810ca98a: an empty member nothing calls (name ours) */
	void Unused0();
	/* 0x810cade8: draws the page from its parameters and mesh */
	void Draw(PageTurnParam *param, PageTurnMesh *mesh);
	/* 0x810cc0f6: lets go of the textures its shaders hold */
	void ReleaseTextures();
	/* 0x810cc42a */
	void Update();
	/* 0x810cc44c: an empty member nothing calls (name ours) */
	void Unused1();

	/* the page's world transform: its rotation and position, the page moved to centre it
	   (name ours, 0x81216eb0) */
	static __attribute__((noinline)) math::matrix GetWorld(const PageTurnParam &param)
	{
		math::matrix world(math::quaternion(
			sce::Vectormath::Simd::Aos::Quat(param.m_rot.getImpl())));

		world.set_w(param.m_pos);

		sce::Vectormath::Simd::Aos::Vector2 half =
			mulPerElem(mulPerElem(param.m_page_size.getImpl(),
					      param.m_scale.getImpl().getXY()),
				   sce::Vectormath::Simd::Aos::Vector2(0.5f));

		return world * math::matrix(sce::Vectormath::Simd::Aos::Matrix4::translation(
				       -sce::Vectormath::Simd::Aos::Vector3(half, 0.0f)));
	}

	/* where the curl puts the page's corner in the world (name ours, 0x81216f84) */
	static __attribute__((noinline)) math::v4 GetCurlPoint(const PageTurnParam &param, math::v2 curl)
	{
		math::matrix world = GetWorld(param);
		sce::Vectormath::Simd::Aos::Matrix4 scale = sce::Vectormath::Simd::Aos::Matrix4::scale(
			sce::Vectormath::Simd::Aos::Vector3(param.m_page_size.getImpl().getY()));

		return math::v4(world.Transform(scale * sce::Vectormath::Simd::Aos::Vector4(
			curl.getImpl(), sce::Vectormath::Simd::Aos::Vector2(0.0f, 1.0f))));
	}

	PageTurnShaders *m_shaders;
	/* set by the constructor and read by Draw, which works out a debug transform with the
	   first and draws the page as lines with the third; nothing in libpaf sets them (names
	   ours) */
	bool m_debug_transform;
	bool m_unused_flag0;
	bool m_unused_flag1;
	bool m_unused_flag2;
	bool m_wireframe;
	bool m_unused_flag3;
	ExpMinusTexture m_exp_minus;
	ParamSetKeysManager m_manager;
	/* the frame of the last update, -1 before it */
	int32_t m_frame;
	ParamSetInstance<MaterialParamSet> m_material;
	ParamSetInstance<PointLightSphereParamSet> m_light;
	ParamSetInstance<PointLightSphereParamSet> m_shadow_light;
	ParamSetInstance<TurnPageParamSet> m_turn_page;
	/* the material a draw lights the page with, the fresnel turned down with the curl */
	MaterialParamSet m_draw_material;
	/* the page's corner, drawn where it is turned */
	arl::TextureSource m_corner;
	/* made by the constructor and used by nothing (name ours) */
	WaveMesh<WaveVertex> m_unused_mesh;
	arl::TextureSource m_distance;
	/* the back of the page (PageTurnObj::SetBgTexture) */
	arl::TextureSource m_background;
	ui::Screen *m_screen;
};

/* Sony's build keeps the page set's instance destructor out of line */
template<>
inline __attribute__((noinline)) ParamSetInstance<TurnPageParamSet>::~ParamSetInstance()
{
}

}
}

#endif
