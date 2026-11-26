/*
 * Particles: the 3D particle background and trail effect present in 2014 PAF builds
 * (pup:psp2swu.self), consisting of ParticlesGraphics, ParticleEmitter,
 * ParticlesPattern1 (ambient field), and ParticlesPattern2 (Tinkerbell trail).
 */
#ifndef PAF_GRAPHICS_EFFECTS_PARTICLES_H
#define PAF_GRAPHICS_EFFECTS_PARTICLES_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/arl/gxm/gxm_shader.h>
#include <graphics/arl/gxm/gxm_texture.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <graphics/camera.h>
#include <graphics/drawobj/draw_env.h>
#include <graphics/effects/param.h>
#include <graphics/effects/particles_param.h>
#include <graphics/effects/wave_renderer.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <std/stdlib.h>
#include <std/string>
#include <std/vector>

namespace paf {
namespace graph {

/* uniform setter interface shared by all four particle vertex shaders */
class ParticlesVertParamsBase {
public:
	virtual ~ParticlesVertParamsBase()
	{
	}

	virtual void SetModelviewProjection(const math::matrix &value)
	{
	}

	virtual void SetModelview(const math::matrix &value)
	{
	}

	virtual void SetParticleSize(const math::v3 &value)
	{
	}

	virtual void SetLightPack(const math::matrix &value)
	{
	}

	virtual void SetTransparency(const math::v2 &value)
	{
	}

	virtual void SetFocus(const math::v4 &value)
	{
	}

	virtual void SetFocusRangesRcp(const math::v2 &value)
	{
	}

	virtual void SetFocusCurves(const math::v3 &value)
	{
	}

	virtual void SetFrontFacingQuaternion(const math::v4 &value)
	{
	}

	virtual void SetFrontFacingMatrix(const math::matrix &value)
	{
	}

	virtual void SetNearControl(const math::v4 &value)
	{
	}

	virtual void SetLifeBoundsMin(const math::v3 &value)
	{
	}

	virtual void SetLifeBoundsMax(const math::v3 &value)
	{
	}

	virtual void SetDarkness(const math::v2 &value)
	{
	}

	virtual void SetGlobalAlign(const float &value)
	{
	}

	virtual void SetGlare(const math::v4 &value)
	{
	}

	virtual void SetWorldScaleHack(const float &value)
	{
	}

	virtual void SetGlareRampTex(const arl::TextureSource &value)
	{
	}

	virtual void SetIridescentTex(const arl::TextureSource &value)
	{
	}

	virtual void SetRotationsTex(const arl::TextureSource &value)
	{
	}

	virtual void SetColor(const math::v3 &value)
	{
	}
};

/* uniform setter interface shared by all three particle fragment shaders */
class ParticlesFragParamsBase {
public:
	virtual ~ParticlesFragParamsBase()
	{
	}

	virtual void SetGlare(const math::v4 &value)
	{
	}

	virtual void SetRadiusTex(const arl::TextureSource &value)
	{
	}

	virtual void SetGlareTex(const arl::TextureSource &value)
	{
	}

	virtual void SetColor(const math::v4 &value)
	{
	}
};

/* paf/cg/drawobj/particles/particles.vp */
class ParticlesVertShader : public arl::ShaderInterfaceBase, public ParticlesVertParamsBase {
public:
	ParticlesVertShader()
		: m_mvp(m_setup), m_mv(m_setup), m_particle_size(m_setup), m_light_pack(m_setup),
		  m_transparency(m_setup), m_focus(m_setup), m_focus_ranges_rcp(m_setup),
		  m_focus_curves(m_setup), m_front_facing_quat(m_setup), m_near_control(m_setup),
		  m_life_bounds_min(m_setup), m_life_bounds_max(m_setup), m_darkness(m_setup),
		  m_global_align(m_setup), m_glare(m_setup), m_world_scale_hack(m_setup),
		  m_iridescent_tex(m_setup), m_rotations_tex(m_setup), m_color(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("ModelviewProjection", arl::ShaderParameterBase::UNIFORM, 0);
		m_mv.init("Modelview", arl::ShaderParameterBase::UNIFORM, 0);
		m_particle_size.init("ParticleSize", arl::ShaderParameterBase::UNIFORM, 0);
		m_light_pack.init("LightPack", arl::ShaderParameterBase::UNIFORM, 0);
		m_transparency.init("Transparency", arl::ShaderParameterBase::UNIFORM, 0);
		m_focus.init("Focus", arl::ShaderParameterBase::UNIFORM, 0);
		m_focus_ranges_rcp.init("FocusRangesRcp", arl::ShaderParameterBase::UNIFORM, 0);
		m_focus_curves.init("FocusCurves", arl::ShaderParameterBase::UNIFORM, 0);
		m_front_facing_quat.init("FrontFacingQuaternion",
					 arl::ShaderParameterBase::UNIFORM, 0);
		m_near_control.init("NearControl", arl::ShaderParameterBase::UNIFORM, 0);
		m_life_bounds_min.init("LifeBoundsMin", arl::ShaderParameterBase::UNIFORM, 0);
		m_life_bounds_max.init("LifeBoundsMax", arl::ShaderParameterBase::UNIFORM, 0);
		m_darkness.init("Darkness", arl::ShaderParameterBase::UNIFORM, 0);
		m_global_align.init("GlobalAlign", arl::ShaderParameterBase::UNIFORM, 0);
		m_glare.init("Glare", arl::ShaderParameterBase::UNIFORM, 0);
		m_world_scale_hack.init("WorldScaleHack", arl::ShaderParameterBase::UNIFORM, 0);
		m_iridescent_tex.init("IridescentTex", 20, 0);
		m_rotations_tex.init("RotationsTex", 21, 0);
		m_color.init("Color", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_mv.Apply();
		m_particle_size.Apply();
		m_light_pack.Apply();
		m_transparency.Apply();
		m_focus.Apply();
		m_focus_ranges_rcp.Apply();
		m_focus_curves.Apply();
		m_front_facing_quat.Apply();
		m_near_control.Apply();
		m_life_bounds_min.Apply();
		m_life_bounds_max.Apply();
		m_darkness.Apply();
		m_global_align.Apply();
		m_glare.Apply();
		m_world_scale_hack.Apply();
		m_iridescent_tex.Apply();
		m_rotations_tex.Apply();
		m_color.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/particles/particles.vp", 0x9F4);

	virtual void SetModelviewProjection(const math::matrix &value) { m_mvp.Set(value); }
	virtual void SetModelview(const math::matrix &value) { m_mv.Set(value); }
	virtual void SetParticleSize(const math::v3 &value) { m_particle_size.Set(value); }
	virtual void SetLightPack(const math::matrix &value) { m_light_pack.Set(value); }
	virtual void SetTransparency(const math::v2 &value) { m_transparency.Set(value); }
	virtual void SetFocus(const math::v4 &value) { m_focus.Set(value); }
	virtual void SetFocusRangesRcp(const math::v2 &value) { m_focus_ranges_rcp.Set(value); }
	virtual void SetFocusCurves(const math::v3 &value) { m_focus_curves.Set(value); }
	virtual void SetFrontFacingQuaternion(const math::v4 &value)
	{
		m_front_facing_quat.Set(value);
	}
	virtual void SetNearControl(const math::v4 &value) { m_near_control.Set(value); }
	virtual void SetLifeBoundsMin(const math::v3 &value) { m_life_bounds_min.Set(value); }
	virtual void SetLifeBoundsMax(const math::v3 &value) { m_life_bounds_max.Set(value); }
	virtual void SetDarkness(const math::v2 &value) { m_darkness.Set(value); }
	virtual void SetGlobalAlign(const float &value) { m_global_align.Set(value); }
	virtual void SetGlare(const math::v4 &value) { m_glare.Set(value); }
	virtual void SetWorldScaleHack(const float &value) { m_world_scale_hack.Set(value); }
	virtual void SetIridescentTex(const arl::TextureSource &value)
	{
		m_iridescent_tex.Set(value);
	}
	virtual void SetRotationsTex(const arl::TextureSource &value) { m_rotations_tex.Set(value); }
	virtual void SetColor(const math::v3 &value) { m_color.Set(value); }

	arl::ShaderParameter<math::matrix> m_mvp;
	arl::ShaderParameter<math::matrix> m_mv;
	arl::ShaderParameter<math::v3> m_particle_size;
	arl::ShaderParameter<math::matrix> m_light_pack;
	arl::ShaderParameter<math::v2> m_transparency;
	arl::ShaderParameter<math::v4> m_focus;
	arl::ShaderParameter<math::v2> m_focus_ranges_rcp;
	arl::ShaderParameter<math::v3> m_focus_curves;
	arl::ShaderParameter<math::v4> m_front_facing_quat;
	arl::ShaderParameter<math::v4> m_near_control;
	arl::ShaderParameter<math::v3> m_life_bounds_min;
	arl::ShaderParameter<math::v3> m_life_bounds_max;
	arl::ShaderParameter<math::v2> m_darkness;
	arl::ShaderParameter<float> m_global_align;
	arl::ShaderParameter<math::v4> m_glare;
	arl::ShaderParameter<float> m_world_scale_hack;
	arl::ShaderParameter<arl::TextureSource> m_iridescent_tex;
	arl::ShaderParameter<arl::TextureSource> m_rotations_tex;
	arl::ShaderParameter<math::v3> m_color;
};

/* paf/cg/drawobj/particles/particles.fp */
class ParticlesFragShader : public arl::ShaderInterfaceBase, public ParticlesFragParamsBase {
public:
	ParticlesFragShader() : m_radius_tex(m_setup)
	{
	}

	virtual void Init()
	{
		m_radius_tex.init("RadiusTex", 3, 0);
	}

	virtual void Apply()
	{
		m_radius_tex.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/particles/particles.fp", 0x27C);

	virtual void SetRadiusTex(const arl::TextureSource &value) { m_radius_tex.Set(value); }

	arl::ShaderParameter<arl::TextureSource> m_radius_tex;
};

/* paf/cg/drawobj/particles/particles_glare.vp */
class ParticlesGlareVertShader : public arl::ShaderInterfaceBase, public ParticlesVertParamsBase {
public:
	ParticlesGlareVertShader()
		: m_mvp(m_setup), m_mv(m_setup), m_particle_size(m_setup), m_light_pack(m_setup),
		  m_transparency(m_setup), m_focus(m_setup), m_focus_ranges_rcp(m_setup),
		  m_focus_curves(m_setup), m_front_facing_matrix(m_setup),
		  m_near_control(m_setup), m_life_bounds_min(m_setup), m_life_bounds_max(m_setup),
		  m_darkness(m_setup), m_glare(m_setup), m_world_scale_hack(m_setup),
		  m_iridescent_tex(m_setup), m_rotations_tex(m_setup), m_color(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("ModelviewProjection", arl::ShaderParameterBase::UNIFORM, 0);
		m_mv.init("Modelview", arl::ShaderParameterBase::UNIFORM, 0);
		m_particle_size.init("ParticleSize", arl::ShaderParameterBase::UNIFORM, 0);
		m_light_pack.init("LightPack", arl::ShaderParameterBase::UNIFORM, 0);
		m_transparency.init("Transparency", arl::ShaderParameterBase::UNIFORM, 0);
		m_focus.init("Focus", arl::ShaderParameterBase::UNIFORM, 0);
		m_focus_ranges_rcp.init("FocusRangesRcp", arl::ShaderParameterBase::UNIFORM, 0);
		m_focus_curves.init("FocusCurves", arl::ShaderParameterBase::UNIFORM, 0);
		m_front_facing_matrix.init("FrontFacingMatrix",
					   arl::ShaderParameterBase::UNIFORM, 0);
		m_near_control.init("NearControl", arl::ShaderParameterBase::UNIFORM, 0);
		m_life_bounds_min.init("LifeBoundsMin", arl::ShaderParameterBase::UNIFORM, 0);
		m_life_bounds_max.init("LifeBoundsMax", arl::ShaderParameterBase::UNIFORM, 0);
		m_darkness.init("Darkness", arl::ShaderParameterBase::UNIFORM, 0);
		m_glare.init("Glare", arl::ShaderParameterBase::UNIFORM, 0);
		m_world_scale_hack.init("WorldScaleHack", arl::ShaderParameterBase::UNIFORM, 0);
		m_iridescent_tex.init("IridescentTex", 20, 0);
		m_rotations_tex.init("RotationsTex", 21, 0);
		m_color.init("Color", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_mv.Apply();
		m_particle_size.Apply();
		m_light_pack.Apply();
		m_transparency.Apply();
		m_focus.Apply();
		m_focus_ranges_rcp.Apply();
		m_focus_curves.Apply();
		m_front_facing_matrix.Apply();
		m_near_control.Apply();
		m_life_bounds_min.Apply();
		m_life_bounds_max.Apply();
		m_darkness.Apply();
		m_glare.Apply();
		m_world_scale_hack.Apply();
		m_iridescent_tex.Apply();
		m_rotations_tex.Apply();
		m_color.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/particles/particles_glare.vp", 0x8E4);

	virtual void SetModelviewProjection(const math::matrix &value) { m_mvp.Set(value); }
	virtual void SetModelview(const math::matrix &value) { m_mv.Set(value); }
	virtual void SetParticleSize(const math::v3 &value) { m_particle_size.Set(value); }
	virtual void SetLightPack(const math::matrix &value) { m_light_pack.Set(value); }
	virtual void SetTransparency(const math::v2 &value) { m_transparency.Set(value); }
	virtual void SetFocus(const math::v4 &value) { m_focus.Set(value); }
	virtual void SetFocusRangesRcp(const math::v2 &value) { m_focus_ranges_rcp.Set(value); }
	virtual void SetFocusCurves(const math::v3 &value) { m_focus_curves.Set(value); }
	virtual void SetFrontFacingMatrix(const math::matrix &value)
	{
		m_front_facing_matrix.Set(value);
	}
	virtual void SetNearControl(const math::v4 &value) { m_near_control.Set(value); }
	virtual void SetLifeBoundsMin(const math::v3 &value) { m_life_bounds_min.Set(value); }
	virtual void SetLifeBoundsMax(const math::v3 &value) { m_life_bounds_max.Set(value); }
	virtual void SetDarkness(const math::v2 &value) { m_darkness.Set(value); }
	virtual void SetGlare(const math::v4 &value) { m_glare.Set(value); }
	virtual void SetWorldScaleHack(const float &value) { m_world_scale_hack.Set(value); }
	virtual void SetIridescentTex(const arl::TextureSource &value)
	{
		m_iridescent_tex.Set(value);
	}
	virtual void SetRotationsTex(const arl::TextureSource &value) { m_rotations_tex.Set(value); }
	virtual void SetColor(const math::v3 &value) { m_color.Set(value); }

	arl::ShaderParameter<math::matrix> m_mvp;
	arl::ShaderParameter<math::matrix> m_mv;
	arl::ShaderParameter<math::v3> m_particle_size;
	arl::ShaderParameter<math::matrix> m_light_pack;
	arl::ShaderParameter<math::v2> m_transparency;
	arl::ShaderParameter<math::v4> m_focus;
	arl::ShaderParameter<math::v2> m_focus_ranges_rcp;
	arl::ShaderParameter<math::v3> m_focus_curves;
	arl::ShaderParameter<math::matrix> m_front_facing_matrix;
	arl::ShaderParameter<math::v4> m_near_control;
	arl::ShaderParameter<math::v3> m_life_bounds_min;
	arl::ShaderParameter<math::v3> m_life_bounds_max;
	arl::ShaderParameter<math::v2> m_darkness;
	arl::ShaderParameter<math::v4> m_glare;
	arl::ShaderParameter<float> m_world_scale_hack;
	arl::ShaderParameter<arl::TextureSource> m_iridescent_tex;
	arl::ShaderParameter<arl::TextureSource> m_rotations_tex;
	arl::ShaderParameter<math::v3> m_color;
};

/* paf/cg/drawobj/particles/particles_glare.fp */
class ParticlesGlareFragShader : public arl::ShaderInterfaceBase, public ParticlesFragParamsBase {
public:
	ParticlesGlareFragShader() : m_radius_tex(m_setup), m_glare_tex(m_setup)
	{
	}

	virtual void Init()
	{
		m_radius_tex.init("RadiusTex", 3, 0);
		m_glare_tex.init("GlareTex", 4, 0);
	}

	virtual void Apply()
	{
		m_radius_tex.Apply();
		m_glare_tex.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/particles/particles_glare.fp", 0x134);

	virtual void SetRadiusTex(const arl::TextureSource &value) { m_radius_tex.Set(value); }
	virtual void SetGlareTex(const arl::TextureSource &value) { m_glare_tex.Set(value); }

	arl::ShaderParameter<arl::TextureSource> m_radius_tex;
	arl::ShaderParameter<arl::TextureSource> m_glare_tex;
};

/* paf/cg/drawobj/particles/particles_front_facing.vp */
class ParticlesFrontFacingVertShader : public arl::ShaderInterfaceBase,
				       public ParticlesVertParamsBase {
public:
	ParticlesFrontFacingVertShader()
		: m_mvp(m_setup), m_mv(m_setup), m_particle_size(m_setup), m_light_pack(m_setup),
		  m_transparency(m_setup), m_focus(m_setup), m_focus_ranges_rcp(m_setup),
		  m_focus_curves(m_setup), m_front_facing_matrix(m_setup),
		  m_near_control(m_setup), m_life_bounds_min(m_setup), m_life_bounds_max(m_setup),
		  m_darkness(m_setup), m_glare(m_setup), m_world_scale_hack(m_setup),
		  m_iridescent_tex(m_setup), m_rotations_tex(m_setup), m_color(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("ModelviewProjection", arl::ShaderParameterBase::UNIFORM, 0);
		m_mv.init("Modelview", arl::ShaderParameterBase::UNIFORM, 0);
		m_particle_size.init("ParticleSize", arl::ShaderParameterBase::UNIFORM, 0);
		m_light_pack.init("LightPack", arl::ShaderParameterBase::UNIFORM, 0);
		m_transparency.init("Transparency", arl::ShaderParameterBase::UNIFORM, 0);
		m_focus.init("Focus", arl::ShaderParameterBase::UNIFORM, 0);
		m_focus_ranges_rcp.init("FocusRangesRcp", arl::ShaderParameterBase::UNIFORM, 0);
		m_focus_curves.init("FocusCurves", arl::ShaderParameterBase::UNIFORM, 0);
		m_front_facing_matrix.init("FrontFacingMatrix",
					   arl::ShaderParameterBase::UNIFORM, 0);
		m_near_control.init("NearControl", arl::ShaderParameterBase::UNIFORM, 0);
		m_life_bounds_min.init("LifeBoundsMin", arl::ShaderParameterBase::UNIFORM, 0);
		m_life_bounds_max.init("LifeBoundsMax", arl::ShaderParameterBase::UNIFORM, 0);
		m_darkness.init("Darkness", arl::ShaderParameterBase::UNIFORM, 0);
		m_glare.init("Glare", arl::ShaderParameterBase::UNIFORM, 0);
		m_world_scale_hack.init("WorldScaleHack", arl::ShaderParameterBase::UNIFORM, 0);
		m_iridescent_tex.init("IridescentTex", 20, 0);
		m_rotations_tex.init("RotationsTex", 21, 0);
		m_color.init("Color", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_mv.Apply();
		m_particle_size.Apply();
		m_light_pack.Apply();
		m_transparency.Apply();
		m_focus.Apply();
		m_focus_ranges_rcp.Apply();
		m_focus_curves.Apply();
		m_front_facing_matrix.Apply();
		m_near_control.Apply();
		m_life_bounds_min.Apply();
		m_life_bounds_max.Apply();
		m_darkness.Apply();
		m_glare.Apply();
		m_world_scale_hack.Apply();
		m_iridescent_tex.Apply();
		m_rotations_tex.Apply();
		m_color.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/particles/particles_front_facing.vp", 0x90C);

	virtual void SetModelviewProjection(const math::matrix &value) { m_mvp.Set(value); }
	virtual void SetModelview(const math::matrix &value) { m_mv.Set(value); }
	virtual void SetParticleSize(const math::v3 &value) { m_particle_size.Set(value); }
	virtual void SetLightPack(const math::matrix &value) { m_light_pack.Set(value); }
	virtual void SetTransparency(const math::v2 &value) { m_transparency.Set(value); }
	virtual void SetFocus(const math::v4 &value) { m_focus.Set(value); }
	virtual void SetFocusRangesRcp(const math::v2 &value) { m_focus_ranges_rcp.Set(value); }
	virtual void SetFocusCurves(const math::v3 &value) { m_focus_curves.Set(value); }
	virtual void SetFrontFacingMatrix(const math::matrix &value)
	{
		m_front_facing_matrix.Set(value);
	}
	virtual void SetNearControl(const math::v4 &value) { m_near_control.Set(value); }
	virtual void SetLifeBoundsMin(const math::v3 &value) { m_life_bounds_min.Set(value); }
	virtual void SetLifeBoundsMax(const math::v3 &value) { m_life_bounds_max.Set(value); }
	virtual void SetDarkness(const math::v2 &value) { m_darkness.Set(value); }
	virtual void SetGlare(const math::v4 &value) { m_glare.Set(value); }
	virtual void SetWorldScaleHack(const float &value) { m_world_scale_hack.Set(value); }
	virtual void SetIridescentTex(const arl::TextureSource &value)
	{
		m_iridescent_tex.Set(value);
	}
	virtual void SetRotationsTex(const arl::TextureSource &value) { m_rotations_tex.Set(value); }
	virtual void SetColor(const math::v3 &value) { m_color.Set(value); }

	arl::ShaderParameter<math::matrix> m_mvp;
	arl::ShaderParameter<math::matrix> m_mv;
	arl::ShaderParameter<math::v3> m_particle_size;
	arl::ShaderParameter<math::matrix> m_light_pack;
	arl::ShaderParameter<math::v2> m_transparency;
	arl::ShaderParameter<math::v4> m_focus;
	arl::ShaderParameter<math::v2> m_focus_ranges_rcp;
	arl::ShaderParameter<math::v3> m_focus_curves;
	arl::ShaderParameter<math::matrix> m_front_facing_matrix;
	arl::ShaderParameter<math::v4> m_near_control;
	arl::ShaderParameter<math::v3> m_life_bounds_min;
	arl::ShaderParameter<math::v3> m_life_bounds_max;
	arl::ShaderParameter<math::v2> m_darkness;
	arl::ShaderParameter<math::v4> m_glare;
	arl::ShaderParameter<float> m_world_scale_hack;
	arl::ShaderParameter<arl::TextureSource> m_iridescent_tex;
	arl::ShaderParameter<arl::TextureSource> m_rotations_tex;
	arl::ShaderParameter<math::v3> m_color;
};

/* paf/cg/drawobj/particles/particles_debug.vp */
class ParticlesDebugVertShader : public arl::ShaderInterfaceBase, public ParticlesVertParamsBase {
public:
	ParticlesDebugVertShader()
		: m_mvp(m_setup), m_mv(m_setup), m_particle_size(m_setup), m_light_pack(m_setup),
		  m_focus(m_setup), m_focus_ranges_rcp(m_setup), m_focus_curves(m_setup),
		  m_front_facing_quat(m_setup), m_near_control(m_setup), m_global_align(m_setup),
		  m_glare(m_setup), m_world_scale_hack(m_setup), m_rotations_tex(m_setup),
		  m_color(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("ModelviewProjection", arl::ShaderParameterBase::UNIFORM, 0);
		m_mv.init("Modelview", arl::ShaderParameterBase::UNIFORM, 0);
		m_particle_size.init("ParticleSize", arl::ShaderParameterBase::UNIFORM, 0);
		m_light_pack.init("LightPack", arl::ShaderParameterBase::UNIFORM, 0);
		m_focus.init("Focus", arl::ShaderParameterBase::UNIFORM, 0);
		m_focus_ranges_rcp.init("FocusRangesRcp", arl::ShaderParameterBase::UNIFORM, 0);
		m_focus_curves.init("FocusCurves", arl::ShaderParameterBase::UNIFORM, 0);
		m_front_facing_quat.init("FrontFacingQuaternion",
					 arl::ShaderParameterBase::UNIFORM, 0);
		m_near_control.init("NearControl", arl::ShaderParameterBase::UNIFORM, 0);
		m_global_align.init("GlobalAlign", arl::ShaderParameterBase::UNIFORM, 0);
		m_glare.init("Glare", arl::ShaderParameterBase::UNIFORM, 0);
		m_world_scale_hack.init("WorldScaleHack", arl::ShaderParameterBase::UNIFORM, 0);
		m_rotations_tex.init("RotationsTex", 21, 0);
		m_color.init("Color", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_mv.Apply();
		m_particle_size.Apply();
		m_light_pack.Apply();
		m_focus.Apply();
		m_focus_ranges_rcp.Apply();
		m_focus_curves.Apply();
		m_front_facing_quat.Apply();
		m_near_control.Apply();
		m_global_align.Apply();
		m_glare.Apply();
		m_world_scale_hack.Apply();
		m_rotations_tex.Apply();
		m_color.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/particles/particles_debug.vp", 0x634);

	virtual void SetModelviewProjection(const math::matrix &value) { m_mvp.Set(value); }
	virtual void SetModelview(const math::matrix &value) { m_mv.Set(value); }
	virtual void SetParticleSize(const math::v3 &value) { m_particle_size.Set(value); }
	virtual void SetLightPack(const math::matrix &value) { m_light_pack.Set(value); }
	virtual void SetFocus(const math::v4 &value) { m_focus.Set(value); }
	virtual void SetFocusRangesRcp(const math::v2 &value) { m_focus_ranges_rcp.Set(value); }
	virtual void SetFocusCurves(const math::v3 &value) { m_focus_curves.Set(value); }
	virtual void SetFrontFacingQuaternion(const math::v4 &value)
	{
		m_front_facing_quat.Set(value);
	}
	virtual void SetNearControl(const math::v4 &value) { m_near_control.Set(value); }
	virtual void SetGlobalAlign(const float &value) { m_global_align.Set(value); }
	virtual void SetGlare(const math::v4 &value) { m_glare.Set(value); }
	virtual void SetWorldScaleHack(const float &value) { m_world_scale_hack.Set(value); }
	virtual void SetRotationsTex(const arl::TextureSource &value) { m_rotations_tex.Set(value); }
	virtual void SetColor(const math::v3 &value) { m_color.Set(value); }

	arl::ShaderParameter<math::matrix> m_mvp;
	arl::ShaderParameter<math::matrix> m_mv;
	arl::ShaderParameter<math::v3> m_particle_size;
	arl::ShaderParameter<math::matrix> m_light_pack;
	arl::ShaderParameter<math::v4> m_focus;
	arl::ShaderParameter<math::v2> m_focus_ranges_rcp;
	arl::ShaderParameter<math::v3> m_focus_curves;
	arl::ShaderParameter<math::v4> m_front_facing_quat;
	arl::ShaderParameter<math::v4> m_near_control;
	arl::ShaderParameter<float> m_global_align;
	arl::ShaderParameter<math::v4> m_glare;
	arl::ShaderParameter<float> m_world_scale_hack;
	arl::ShaderParameter<arl::TextureSource> m_rotations_tex;
	arl::ShaderParameter<math::v3> m_color;
};

/* paf/cg/drawobj/particles/particles_debug.fp */
class ParticlesDebugFragShader : public arl::ShaderInterfaceBase, public ParticlesFragParamsBase {
public:
	ParticlesDebugFragShader() : m_radius_tex(m_setup)
	{
	}

	virtual void Init()
	{
		m_radius_tex.init("RadiusTex", 3, 0);
	}

	virtual void Apply()
	{
		m_radius_tex.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/particles/particles_debug.fp", 0x27C);

	virtual void SetRadiusTex(const arl::TextureSource &value) { m_radius_tex.Set(value); }

	arl::ShaderParameter<arl::TextureSource> m_radius_tex;
};

/* all seven particle shaders held together (4360 bytes) */
class ParticlesShaders {
public:
	arl::ShaderInstance<ParticlesVertShader, arl::VertShaderObject> m_vert;
	arl::ShaderInstance<ParticlesFragShader, arl::FragShaderObject> m_frag;
	arl::ShaderInstance<ParticlesGlareVertShader, arl::VertShaderObject> m_glare_vert;
	arl::ShaderInstance<ParticlesGlareFragShader, arl::FragShaderObject> m_glare_frag;
	arl::ShaderInstance<ParticlesFrontFacingVertShader, arl::VertShaderObject>
		m_front_facing_vert;
	arl::ShaderInstance<ParticlesDebugVertShader, arl::VertShaderObject> m_debug_vert;
	arl::ShaderInstance<ParticlesDebugFragShader, arl::FragShaderObject> m_debug_frag;
};

/* cached 1D glare falloff lookup texture, rebuilt when power or falloff changes (16 bytes) */
class GlareFalloffCache {
public:
	GlareFalloffCache() : m_power(-1.0f), m_falloff(-1.0f)
	{
	}

	common::SharedPtr<arl::Texture2D> Get(float power, float falloff);

	common::SharedPtr<arl::Texture2D> m_texture;
	float m_power;
	float m_falloff;
};

/* one double-buffered instance vertex buffer for an emitter (20 bytes) */
struct ParticleRenderSlot {
	arl::VertexArray m_vertices;
	uint32_t m_count;
};

/* per-emitter GPU resources (60 bytes) */
struct ParticleEmitterRender {
	ParticleEmitterRender() : m_max_particles(0)
	{
		m_slots[0].m_count = 0;
		m_slots[1].m_count = 0;
	}

	uint32_t m_max_particles;
	ParticleRenderSlot m_slots[2];
	GlareFalloffCache m_glare_cache;
};

/* shared particle geometry, textures, and shaders (92 bytes) */
class ParticlesGraphics {
public:
	ParticlesGraphics(const string &dir, uint32_t max_particles,
			  common::SharedPtr<arl::VramAllocator> allocator);
	~ParticlesGraphics();

	void InitEmitterRender(ParticleEmitterRender *render, uint32_t max_particles);

	uint32_t m_max_particles;
	common::SharedPtr<arl::VertexArraySpec> m_instance_spec;
	common::SharedPtr<arl::VramAllocator> m_allocator;
	arl::VertexArray m_quad_vertices;
	arl::IndexArray m_quad_indices;
	common::SharedPtr<arl::Texture2D> m_iridescent_tex;
	common::SharedPtr<arl::Texture2D> m_radius_tex;
	common::SharedPtr<ParticlesShaders> m_shaders;
	arl::TextureSource m_rotations_tex;
	common::SharedPtr<arl::Texture2D> m_glare_tex;
};

/* one live or dead particle in the simulation array (32 bytes) */
struct ParticleState {
	/* xyz = position, w = age (-666.0f when dead) */
	math::v4 m_pos_age;
	/* xyz = velocity, w = aging speed */
	math::v4 m_vel_aging;
};

/* queued emitter command (24 bytes) */
struct ParticleCommand {
	enum Op {
		OP_ALLOC = 0,
		OP_SET_POS = 1,
		OP_SET_VEL = 2,
		OP_ADD_VEL = 3,
		OP_SET_AGING = 4
	};

	int32_t m_op;
	int32_t m_index;
	float m_x;
	float m_y;
	float m_z;
	float m_w;
};

/* 128-byte aligned array used for particle state and free-index stacks (20 bytes) */
template<class T>
class ParticleAlignedArray {
public:
	ParticleAlignedArray(uint32_t capacity)
		: m_data(NULL), m_count(0), m_capacity(0), m_alloc(NULL), m_alloc_cap(0)
	{
		if (capacity != 0) {
			m_alloc = (T *)sce_paf_memalign(128, sizeof(T) * capacity);
			m_alloc_cap = capacity;
			m_data = m_alloc;
			m_capacity = capacity;
		}
	}

	~ParticleAlignedArray()
	{
		if (m_alloc != NULL) {
			sce_paf_free(m_alloc);
			m_alloc = NULL;
			m_alloc_cap = 0;
		}
	}

	T *m_data;
	uint32_t m_count;
	uint32_t m_capacity;
	T *m_alloc;
	uint32_t m_alloc_cap;
};

/* simulated and rendered set of particles (544 bytes) */
class ParticleEmitter {
public:
	ParticleEmitter(const string &name, uint32_t max_particles,
			common::SharedPtr<ParticlesGraphics> graphics,
			common::SharedPtr<Camera> camera);
	~ParticleEmitter();

	void Reset();
	void AllocParticle();
	void StepParticles();
	void Update();
	void DrawPass(ParticlesVertParamsBase *vert, arl::VertShaderObject *vert_obj,
		      ParticlesFragParamsBase *frag, arl::FragShaderObject *frag_obj, float unit,
		      math::v4 color);
	void DrawWithColor(float unit, math::v4 color);
	void Draw();

	void PushAlloc()
	{
		if (m_cmd_count != m_cmd_capacity) {
			m_cmd_data[m_cmd_count].m_op = ParticleCommand::OP_ALLOC;
			m_cmd_count++;
		}
	}

	void PushSetPos(const math::v4 &pos)
	{
		if (m_cmd_count != m_cmd_capacity) {
			ParticleCommand &entry = m_cmd_data[m_cmd_count++];

			entry.m_op = ParticleCommand::OP_SET_POS;
			entry.m_index = -1;
			entry.m_x = pos.extract_x();
			entry.m_y = pos.extract_y();
			entry.m_z = pos.extract_z();
		}
	}

	void PushSetVel(const math::v4 &vel)
	{
		if (m_cmd_count != m_cmd_capacity) {
			ParticleCommand &entry = m_cmd_data[m_cmd_count++];

			entry.m_op = ParticleCommand::OP_SET_VEL;
			entry.m_index = -1;
			entry.m_x = vel.extract_x();
			entry.m_y = vel.extract_y();
			entry.m_z = vel.extract_z();
		}
	}

	void PushSetAging(float aging)
	{
		if (m_cmd_count != m_cmd_capacity) {
			ParticleCommand &entry = m_cmd_data[m_cmd_count++];

			entry.m_op = ParticleCommand::OP_SET_AGING;
			entry.m_index = -1;
			entry.m_x = aging;
		}
	}

	string m_name;
	common::SharedPtr<Camera> m_camera;
	uint32_t m_max_particles;
	ParticleAlignedArray<ParticleState> m_particles;
	math::v4 m_gravity;
	math::v4 m_friction;
	math::v4 m_bounds_min;
	math::v4 m_bounds_max;
	bool m_paused;
	math::v4 m_wind;
	float m_turbulence_unused;
	float m_brownian_scale;
	float m_size_middle_copy;
	float m_spin_time_scale;
	math::v4 m_delta_time_vec;
	uint32_t m_command_padding_low;
	uint32_t m_command_padding_high;
	ParticleCommand *m_cmd_data;
	uint32_t m_cmd_count;
	uint32_t m_cmd_capacity;
	vector<ParticleCommand> m_cmd_storage;
	uint32_t m_free_padding;
	ParticleAlignedArray<uint32_t> m_free_indices;
	int32_t m_last_allocated;
	uint32_t m_active_count;
	EmitParamData m_emit;
	MovParamData m_mov;
	LitParamData m_lit;
	FocusParamData m_focus;
	GlareParamData m_glare;
	uint32_t m_render_padding;
	bool m_draw_enabled;
	bool m_debug_enabled;
	int32_t m_render_mode;
	bool m_front_facing_forced;
	bool m_sort_enabled;
	math::v4 *m_write_ptr;
	uint32_t m_write_count;
	uint32_t m_write_capacity;
	ParticleEmitterRender *m_render;
	common::SharedPtr<ParticlesGraphics> m_graphics;
	bool m_hidden;
	int32_t m_last_frame;
	bool m_clear_bounds_once;
	float m_cmd_fill_ratio;
};

/* damped spring modulating Brownian motion during device shake (28 bytes) */
struct ParticleShakeSpring {
	ParticleShakeSpring()
		: m_damping(0.98f), m_scale(1.0f), m_value(0.0f), m_pos(0.0f), m_vel(0.0f),
		  m_force(0.0f)
	{
	}

	float Update()
	{
		m_vel = (m_force + m_vel * -0.4f + m_pos * -0.004f) + m_vel;
		m_force = 0.0f;
		m_pos += m_vel;

		float clamped = m_pos;

		if (clamped < 0.0f)
			clamped = 0.0f;
		if (clamped > 1.0f)
			clamped = 1.0f;
		m_value = clamped;
		return clamped;
	}

	float m_reserved;
	float m_damping;
	float m_scale;
	float m_value;
	float m_pos;
	float m_vel;
	float m_force;
};

/* ambient floating particles with wave-height interpolation ("ParticlesPattern1", 5920 bytes) */
class ParticlesPattern1 {
public:
	ParticlesPattern1();
	~ParticlesPattern1();

	void Init(common::SharedPtr<Camera> camera, common::SharedPtr<DrawEnv> draw_env);
	void Load(const string &dir, common::SharedPtr<ParticlesGraphics> graphics,
		  uint32_t max_particles);
	void UpdateParams(float frame_time);
	void UpdateMotion();
	void Emit();
	void Update(float frame_time);
	void Draw();

	int32_t m_reserved;
	common::SharedPtr<ParticleEmitter> m_emitter;
	common::SharedPtr<Camera> m_camera;
	common::SharedPtr<DrawEnv> m_draw_env;
	ParticleShakeSpring m_shake;
	float m_wave_x;
	float m_wave_y;
	bool m_enabled;
	ParamSetKeysManager m_manager;
	ParamSetInstance<EmitParamSet> m_emit;
	ParamSetInstance<MovParamSet> m_mov;
	ParamSetInstance<LitParamSet> m_lit;
	ParamSetInstance<FocusParamSet> m_focus;
	ParamSetInstance<GlareParamSet> m_glare;
	ParamSetInstance<EmitParamSet> m_emit_up;
	ParamSetInstance<MovParamSet> m_mov_up;
	ParamSetInstance<LitParamSet> m_lit_up;
	ParamSetInstance<FocusParamSet> m_focus_up;
	ParamSetInstance<GlareParamSet> m_glare_up;
	uint32_t m_rng_seed;
	uint64_t m_last_emit_time;
	float m_wait_jitter;
	DrawLayer *m_rs_particles_pattern1;
};

/* touch/trail Tinkerbell particles ("ParticlesPattern2", 5336 bytes) */
class ParticlesPattern2 {
public:
	enum {
		TRAIL_CAPACITY = 64
	};

	ParticlesPattern2();
	~ParticlesPattern2();

	void Init(common::SharedPtr<Camera> camera, common::SharedPtr<DrawEnv> draw_env);
	void Load(const string &dir, common::SharedPtr<ParticlesGraphics> graphics,
		  uint32_t max_particles);
	void UpdateMotion();
	void SampleTouch();
	void Emit();
	void Update(float frame_time);
	void Draw();
	void SetEnabled(bool enabled);

	common::SharedPtr<ParticleEmitter> m_emitter;
	common::SharedPtr<Camera> m_camera;
	common::SharedPtr<DrawEnv> m_draw_env;
	ParticleShakeSpring m_shake;
	bool m_enabled;
	bool m_unused_flag;
	ParamSetKeysManager m_manager;
	ParamSetInstance<EmitParamSet> m_emit;
	ParamSetInstance<MovParamSet> m_mov;
	ParamSetInstance<LitParamSet> m_lit;
	ParamSetInstance<FocusParamSet> m_focus;
	ParamSetInstance<GlareParamSet> m_glare;
	ParamSetInstance<TinkerbellParamSet> m_tinkerbell;
	uint32_t m_rng_seed;
	uint64_t m_last_emit_time;
	float m_wait_jitter;
	int32_t m_last_frame;
	math::v4 m_trail_storage[TRAIL_CAPACITY];
	CustomTrailPoints m_trail;
	math::v4 m_trail_ordered[TRAIL_CAPACITY];
	bool m_sample_touch;
	DrawLayer *m_rs_particles_pattern2;
};

/* top-level particle effect owning both patterns (11280 bytes) */
class Particles {
public:
	Particles()
	{
	}

	~Particles()
	{
	}

	void Init(common::SharedPtr<Camera> camera, common::SharedPtr<DrawEnv> draw_env);
	void Load(const string &dir_ambient, uint32_t max_ambient, const string &dir_trail,
		  uint32_t max_trail);
	void Update(float frame_time);

	common::SharedPtr<Camera> m_camera;
	common::SharedPtr<DrawEnv> m_draw_env;
	common::SharedPtr<ParticlesGraphics> m_graphics;
	ParticlesPattern1 m_pattern1;
	ParticlesPattern2 m_pattern2;
};

}
}

#endif
