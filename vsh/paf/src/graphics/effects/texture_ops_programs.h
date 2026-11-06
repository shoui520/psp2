/*
 * The texture operations' fragment programs ("paf/cg/misc/texture_ops/<name>.fp"), each with
 * the interface its parameters are set through, and the set TextureOps makes of them. The
 * programs' paths and their parameters' names are Sony's; the class and member names ours.
 */
#ifndef PAF_SRC_GRAPHICS_EFFECTS_TEXTURE_OPS_PROGRAMS_H
#define PAF_SRC_GRAPHICS_EFFECTS_TEXTURE_OPS_PROGRAMS_H

#include <stdint.h>
#include <graphics/arl/gxm/gxm_framebuffer_copy.h>
#include <graphics/arl/gxm/gxm_shader.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>

namespace paf {
namespace graph {

/* the box blurs' parameters, set the same way whatever the program's radius (names ours) */
class BoxBlurParams {
public:
	virtual ~BoxBlurParams()
	{
	}

	virtual void SetSource(const arl::TextureSource &source) = 0;
	virtual void SetStep(const math::v2 &step) = 0;
	virtual void SetSpread(const float &spread) = 0;
};

/* the modulated box blurs' parameters, an interface of their own: the box blurs' and two more,
   set by nothing and empty in every program (names ours) */
class BoxBlurModulateParams {
public:
	virtual ~BoxBlurModulateParams()
	{
	}

	virtual void SetSource(const arl::TextureSource &source) = 0;
	virtual void SetStep(const math::v2 &step) = 0;
	virtual void SetSpread(const float &spread) = 0;
	virtual void Reserved0() = 0;
	virtual void Reserved1() = 0;
};

/* clear.fp: one colour, ClearColor */
class ClearShader : public arl::ShaderInterfaceBase {
public:
	ClearShader() : m_clear_color(m_setup)
	{
	}

	virtual void Init()
	{
		m_clear_color.init("ClearColor", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_clear_color.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/texture_ops/clear.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	/* clear.fp built by psp2cgc */
	static const uint8_t s_binary[232];

	arl::ShaderParameter<math::v4> m_clear_color;
};

/* bicubic.fp: Source scaled bicubic through the weights in Table; SourceSize its size and the
   size's reciprocal */
class BicubicShader : public arl::ShaderInterfaceBase {
public:
	BicubicShader() : m_source(m_setup), m_table(m_setup), m_source_size(m_setup)
	{
	}

	virtual void Init()
	{
		m_source.init("Source", 1, 0);
		m_table.init("Table", 2, 0);
		m_source_size.init("SourceSize", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_source.Apply();
		m_table.Apply();
		m_source_size.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/texture_ops/bicubic.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	/* bicubic.fp built by psp2cgc */
	static const uint8_t s_binary[604];

	arl::ShaderParameter<arl::TextureSource> m_source;
	arl::ShaderParameter<arl::TextureSource> m_table;
	arl::ShaderParameter<math::v4> m_source_size;
};

/* bicubic_slow.fp: Source scaled bicubic, the weights computed per texel */
class BicubicSlowShader : public arl::ShaderInterfaceBase {
public:
	BicubicSlowShader() : m_source(m_setup), m_source_size(m_setup)
	{
	}

	virtual void Init()
	{
		m_source.init("Source", 1, 0);
		m_source_size.init("SourceSize", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_source.Apply();
		m_source_size.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/texture_ops/bicubic_slow.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	/* bicubic_slow.fp built by psp2cgc */
	static const uint8_t s_binary[1896];

	arl::ShaderParameter<arl::TextureSource> m_source;
	arl::ShaderParameter<math::v4> m_source_size;
};

/* half.fp: Source at half its size */
class HalfShader : public arl::ShaderInterfaceBase {
public:
	HalfShader() : m_source(m_setup)
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

	virtual const char *GetName()
	{
		return "paf/cg/misc/texture_ops/half.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	/* half.fp built by psp2cgc */
	static const uint8_t s_binary[268];

	arl::ShaderParameter<arl::TextureSource> m_source;
};

/* copy.fp: Source faded by Alpha */
class CopyShader : public arl::ShaderInterfaceBase {
public:
	CopyShader() : m_source(m_setup), m_alpha(m_setup)
	{
	}

	virtual void Init()
	{
		m_source.init("Source", 1, 0);
		m_alpha.init("Alpha", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_source.Apply();
		m_alpha.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/texture_ops/copy.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	/* copy.fp built by psp2cgc */
	static const uint8_t s_binary[308];

	arl::ShaderParameter<arl::TextureSource> m_source;
	arl::ShaderParameter<float> m_alpha;
};

/* pure_copy.fp: Source unchanged */
class PureCopyShader : public arl::ShaderInterfaceBase {
public:
	PureCopyShader() : m_source(m_setup)
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

	virtual const char *GetName()
	{
		return "paf/cg/misc/texture_ops/pure_copy.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	/* pure_copy.fp built by psp2cgc */
	static const uint8_t s_binary[260];

	arl::ShaderParameter<arl::TextureSource> m_source;
};

/* grid_normal.fp: the normals of the grid of positions in Source, SourceDuDv the step to the
   neighbours */
class GridNormalShader : public arl::ShaderInterfaceBase {
public:
	GridNormalShader() : m_source(m_setup), m_source_du_dv(m_setup)
	{
	}

	virtual void Init()
	{
		m_source.init("Source", 1, 0);
		m_source_du_dv.init("SourceDuDv", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_source.Apply();
		m_source_du_dv.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/texture_ops/grid_normal.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	/* grid_normal.fp built by psp2cgc */
	static const uint8_t s_binary[488];

	arl::ShaderParameter<arl::TextureSource> m_source;
	arl::ShaderParameter<math::v4> m_source_du_dv;
};

/* grid_normal_8.fp: the normals of the grid of positions in Source from eight neighbours */
class GridNormal8Shader : public arl::ShaderInterfaceBase {
public:
	GridNormal8Shader() : m_source(m_setup), m_source_du_dv(m_setup)
	{
	}

	virtual void Init()
	{
		m_source.init("Source", 1, 0);
		m_source_du_dv.init("SourceDuDv", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_source.Apply();
		m_source_du_dv.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/texture_ops/grid_normal_8.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	/* grid_normal_8.fp built by psp2cgc */
	static const uint8_t s_binary[496];

	arl::ShaderParameter<arl::TextureSource> m_source;
	arl::ShaderParameter<math::v4> m_source_du_dv;
};

/* flipquad.fp: Source read at four offsets chosen by the pattern in FlipTex; nothing draws with
   it */
class FlipQuadShader : public arl::ShaderInterfaceBase {
public:
	FlipQuadShader()
		: m_source(m_setup), m_offset0(m_setup), m_offset1(m_setup), m_offset2(m_setup),
		  m_offset3(m_setup), m_flip_tex(m_setup), m_flip_tex_size(m_setup)
	{
	}

	virtual void Init()
	{
		m_source.init("Source", 1, 0);
		m_offset0.init("Offset0", arl::ShaderParameterBase::UNIFORM, 0);
		m_offset1.init("Offset1", arl::ShaderParameterBase::UNIFORM, 0);
		m_offset2.init("Offset2", arl::ShaderParameterBase::UNIFORM, 0);
		m_offset3.init("Offset3", arl::ShaderParameterBase::UNIFORM, 0);
		m_flip_tex.init("FlipTex", 7, 0);
		m_flip_tex_size.init("FlipTexSize", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_source.Apply();
		m_offset0.Apply();
		m_offset1.Apply();
		m_offset2.Apply();
		m_offset3.Apply();
		m_flip_tex.Apply();
		m_flip_tex_size.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/texture_ops/flipquad.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	/* flipquad.fp built by psp2cgc */
	static const uint8_t s_binary[648];

	arl::ShaderParameter<arl::TextureSource> m_source;
	arl::ShaderParameter<math::v2> m_offset0;
	arl::ShaderParameter<math::v2> m_offset1;
	arl::ShaderParameter<math::v2> m_offset2;
	arl::ShaderParameter<math::v2> m_offset3;
	arl::ShaderParameter<arl::TextureSource> m_flip_tex;
	arl::ShaderParameter<math::v2> m_flip_tex_size;
};

/* fliptri.fp: Source read at three offsets chosen by the pattern in FlipTex */
class FlipTriShader : public arl::ShaderInterfaceBase {
public:
	FlipTriShader()
		: m_source(m_setup), m_offset0(m_setup), m_offset1(m_setup), m_offset2(m_setup),
		  m_flip_tex(m_setup), m_flip_tex_size(m_setup)
	{
	}

	virtual void Init()
	{
		m_source.init("Source", 1, 0);
		m_offset0.init("Offset0", arl::ShaderParameterBase::UNIFORM, 0);
		m_offset1.init("Offset1", arl::ShaderParameterBase::UNIFORM, 0);
		m_offset2.init("Offset2", arl::ShaderParameterBase::UNIFORM, 0);
		m_flip_tex.init("FlipTex", 6, 0);
		m_flip_tex_size.init("FlipTexSize", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_source.Apply();
		m_offset0.Apply();
		m_offset1.Apply();
		m_offset2.Apply();
		m_flip_tex.Apply();
		m_flip_tex_size.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/texture_ops/fliptri.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	/* fliptri.fp built by psp2cgc */
	static const uint8_t s_binary[592];

	arl::ShaderParameter<arl::TextureSource> m_source;
	arl::ShaderParameter<math::v2> m_offset0;
	arl::ShaderParameter<math::v2> m_offset1;
	arl::ShaderParameter<math::v2> m_offset2;
	arl::ShaderParameter<arl::TextureSource> m_flip_tex;
	arl::ShaderParameter<math::v2> m_flip_tex_size;
};

/* box_blur_n.fp: Source box blurred along Step, Spread texels each way */
class BoxBlurNShader : public arl::ShaderInterfaceBase, public BoxBlurParams {
public:
	BoxBlurNShader() : m_source(m_setup), m_step(m_setup), m_spread(m_setup)
	{
	}

	virtual void Init()
	{
		m_source.init("Source", 1, 0);
		m_step.init("Step", arl::ShaderParameterBase::UNIFORM, 0);
		m_spread.init("Spread", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_source.Apply();
		m_step.Apply();
		m_spread.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/texture_ops/box_blur_n.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	virtual void SetSource(const arl::TextureSource &source)
	{
		m_source.Set(source);
	}

	virtual void SetStep(const math::v2 &step)
	{
		m_step.Set(step);
	}

	virtual void SetSpread(const float &spread)
	{
		m_spread.Set(spread);
	}

	/* box_blur_n.fp built by psp2cgc */
	static const uint8_t s_binary[536];

	arl::ShaderParameter<arl::TextureSource> m_source;
	arl::ShaderParameter<math::v2> m_step;
	arl::ShaderParameter<float> m_spread;
};

/* box_blur_1.fp: Source box blurred along Step, 1 texel each way */
class BoxBlur1Shader : public arl::ShaderInterfaceBase, public BoxBlurParams {
public:
	BoxBlur1Shader() : m_source(m_setup), m_step(m_setup)
	{
	}

	virtual void Init()
	{
		m_source.init("Source", 1, 0);
		m_step.init("Step", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_source.Apply();
		m_step.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/texture_ops/box_blur_1.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	virtual void SetSource(const arl::TextureSource &source)
	{
		m_source.Set(source);
	}

	virtual void SetStep(const math::v2 &step)
	{
		m_step.Set(step);
	}

	/* no Spread: the radius is the program's */
	virtual void SetSpread(const float &spread)
	{
	}

	/* box_blur_1.fp built by psp2cgc */
	static const uint8_t s_binary[440];

	arl::ShaderParameter<arl::TextureSource> m_source;
	arl::ShaderParameter<math::v2> m_step;
};

/* box_blur_2.fp: Source box blurred along Step, 2 texels each way */
class BoxBlur2Shader : public arl::ShaderInterfaceBase, public BoxBlurParams {
public:
	BoxBlur2Shader() : m_source(m_setup), m_step(m_setup)
	{
	}

	virtual void Init()
	{
		m_source.init("Source", 1, 0);
		m_step.init("Step", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_source.Apply();
		m_step.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/texture_ops/box_blur_2.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	virtual void SetSource(const arl::TextureSource &source)
	{
		m_source.Set(source);
	}

	virtual void SetStep(const math::v2 &step)
	{
		m_step.Set(step);
	}

	/* no Spread: the radius is the program's */
	virtual void SetSpread(const float &spread)
	{
	}

	/* box_blur_2.fp built by psp2cgc */
	static const uint8_t s_binary[496];

	arl::ShaderParameter<arl::TextureSource> m_source;
	arl::ShaderParameter<math::v2> m_step;
};

/* box_blur_3.fp: Source box blurred along Step, 3 texels each way */
class BoxBlur3Shader : public arl::ShaderInterfaceBase, public BoxBlurParams {
public:
	BoxBlur3Shader() : m_source(m_setup), m_step(m_setup)
	{
	}

	virtual void Init()
	{
		m_source.init("Source", 1, 0);
		m_step.init("Step", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_source.Apply();
		m_step.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/texture_ops/box_blur_3.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	virtual void SetSource(const arl::TextureSource &source)
	{
		m_source.Set(source);
	}

	virtual void SetStep(const math::v2 &step)
	{
		m_step.Set(step);
	}

	/* no Spread: the radius is the program's */
	virtual void SetSpread(const float &spread)
	{
	}

	/* box_blur_3.fp built by psp2cgc */
	static const uint8_t s_binary[568];

	arl::ShaderParameter<arl::TextureSource> m_source;
	arl::ShaderParameter<math::v2> m_step;
};

/* box_blur_4.fp: Source box blurred along Step, 4 texels each way */
class BoxBlur4Shader : public arl::ShaderInterfaceBase, public BoxBlurParams {
public:
	BoxBlur4Shader() : m_source(m_setup), m_step(m_setup)
	{
	}

	virtual void Init()
	{
		m_source.init("Source", 1, 0);
		m_step.init("Step", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_source.Apply();
		m_step.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/texture_ops/box_blur_4.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	virtual void SetSource(const arl::TextureSource &source)
	{
		m_source.Set(source);
	}

	virtual void SetStep(const math::v2 &step)
	{
		m_step.Set(step);
	}

	/* no Spread: the radius is the program's */
	virtual void SetSpread(const float &spread)
	{
	}

	/* box_blur_4.fp built by psp2cgc */
	static const uint8_t s_binary[616];

	arl::ShaderParameter<arl::TextureSource> m_source;
	arl::ShaderParameter<math::v2> m_step;
};

/* box_blur_5.fp: Source box blurred along Step, 5 texels each way */
class BoxBlur5Shader : public arl::ShaderInterfaceBase, public BoxBlurParams {
public:
	BoxBlur5Shader() : m_source(m_setup), m_step(m_setup)
	{
	}

	virtual void Init()
	{
		m_source.init("Source", 1, 0);
		m_step.init("Step", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_source.Apply();
		m_step.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/texture_ops/box_blur_5.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	virtual void SetSource(const arl::TextureSource &source)
	{
		m_source.Set(source);
	}

	virtual void SetStep(const math::v2 &step)
	{
		m_step.Set(step);
	}

	/* no Spread: the radius is the program's */
	virtual void SetSpread(const float &spread)
	{
	}

	/* box_blur_5.fp built by psp2cgc */
	static const uint8_t s_binary[680];

	arl::ShaderParameter<arl::TextureSource> m_source;
	arl::ShaderParameter<math::v2> m_step;
};

/* box_blur_n_modulate.fp: Source box blurred along Step, Spread texels each way, modulated */
class BoxBlurNModulateShader : public arl::ShaderInterfaceBase, public BoxBlurModulateParams {
public:
	BoxBlurNModulateShader() : m_source(m_setup), m_step(m_setup), m_spread(m_setup)
	{
	}

	virtual void Init()
	{
		m_source.init("Source", 1, 0);
		m_step.init("Step", arl::ShaderParameterBase::UNIFORM, 0);
		m_spread.init("Spread", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_source.Apply();
		m_step.Apply();
		m_spread.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/texture_ops/box_blur_n_modulate.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	virtual void SetSource(const arl::TextureSource &source)
	{
		m_source.Set(source);
	}

	virtual void SetStep(const math::v2 &step)
	{
		m_step.Set(step);
	}

	virtual void SetSpread(const float &spread)
	{
		m_spread.Set(spread);
	}

	virtual void Reserved0()
	{
	}

	virtual void Reserved1()
	{
	}

	/* box_blur_n_modulate.fp built by psp2cgc */
	static const uint8_t s_binary[536];

	arl::ShaderParameter<arl::TextureSource> m_source;
	arl::ShaderParameter<math::v2> m_step;
	arl::ShaderParameter<float> m_spread;
};

/* box_blur_1_modulate.fp: Source box blurred along Step, 1 texel each way, modulated */
class BoxBlur1ModulateShader : public arl::ShaderInterfaceBase, public BoxBlurModulateParams {
public:
	BoxBlur1ModulateShader() : m_source(m_setup), m_step(m_setup)
	{
	}

	virtual void Init()
	{
		m_source.init("Source", 1, 0);
		m_step.init("Step", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_source.Apply();
		m_step.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/texture_ops/box_blur_1_modulate.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	virtual void SetSource(const arl::TextureSource &source)
	{
		m_source.Set(source);
	}

	virtual void SetStep(const math::v2 &step)
	{
		m_step.Set(step);
	}

	/* no Spread: the radius is the program's */
	virtual void SetSpread(const float &spread)
	{
	}

	virtual void Reserved0()
	{
	}

	virtual void Reserved1()
	{
	}

	/* box_blur_1_modulate.fp built by psp2cgc */
	static const uint8_t s_binary[440];

	arl::ShaderParameter<arl::TextureSource> m_source;
	arl::ShaderParameter<math::v2> m_step;
};

/* box_blur_2_modulate.fp: Source box blurred along Step, 2 texels each way, modulated */
class BoxBlur2ModulateShader : public arl::ShaderInterfaceBase, public BoxBlurModulateParams {
public:
	BoxBlur2ModulateShader() : m_source(m_setup), m_step(m_setup)
	{
	}

	virtual void Init()
	{
		m_source.init("Source", 1, 0);
		m_step.init("Step", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_source.Apply();
		m_step.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/texture_ops/box_blur_2_modulate.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	virtual void SetSource(const arl::TextureSource &source)
	{
		m_source.Set(source);
	}

	virtual void SetStep(const math::v2 &step)
	{
		m_step.Set(step);
	}

	/* no Spread: the radius is the program's */
	virtual void SetSpread(const float &spread)
	{
	}

	virtual void Reserved0()
	{
	}

	virtual void Reserved1()
	{
	}

	/* box_blur_2_modulate.fp built by psp2cgc */
	static const uint8_t s_binary[496];

	arl::ShaderParameter<arl::TextureSource> m_source;
	arl::ShaderParameter<math::v2> m_step;
};

/* box_blur_3_modulate.fp: Source box blurred along Step, 3 texels each way, modulated */
class BoxBlur3ModulateShader : public arl::ShaderInterfaceBase, public BoxBlurModulateParams {
public:
	BoxBlur3ModulateShader() : m_source(m_setup), m_step(m_setup)
	{
	}

	virtual void Init()
	{
		m_source.init("Source", 1, 0);
		m_step.init("Step", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_source.Apply();
		m_step.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/texture_ops/box_blur_3_modulate.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	virtual void SetSource(const arl::TextureSource &source)
	{
		m_source.Set(source);
	}

	virtual void SetStep(const math::v2 &step)
	{
		m_step.Set(step);
	}

	/* no Spread: the radius is the program's */
	virtual void SetSpread(const float &spread)
	{
	}

	virtual void Reserved0()
	{
	}

	virtual void Reserved1()
	{
	}

	/* box_blur_3_modulate.fp built by psp2cgc */
	static const uint8_t s_binary[568];

	arl::ShaderParameter<arl::TextureSource> m_source;
	arl::ShaderParameter<math::v2> m_step;
};

/* box_blur_4_modulate.fp: Source box blurred along Step, 4 texels each way, modulated */
class BoxBlur4ModulateShader : public arl::ShaderInterfaceBase, public BoxBlurModulateParams {
public:
	BoxBlur4ModulateShader() : m_source(m_setup), m_step(m_setup)
	{
	}

	virtual void Init()
	{
		m_source.init("Source", 1, 0);
		m_step.init("Step", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_source.Apply();
		m_step.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/texture_ops/box_blur_4_modulate.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	virtual void SetSource(const arl::TextureSource &source)
	{
		m_source.Set(source);
	}

	virtual void SetStep(const math::v2 &step)
	{
		m_step.Set(step);
	}

	/* no Spread: the radius is the program's */
	virtual void SetSpread(const float &spread)
	{
	}

	virtual void Reserved0()
	{
	}

	virtual void Reserved1()
	{
	}

	/* box_blur_4_modulate.fp built by psp2cgc */
	static const uint8_t s_binary[616];

	arl::ShaderParameter<arl::TextureSource> m_source;
	arl::ShaderParameter<math::v2> m_step;
};

/* box_blur_5_modulate.fp: Source box blurred along Step, 5 texels each way, modulated */
class BoxBlur5ModulateShader : public arl::ShaderInterfaceBase, public BoxBlurModulateParams {
public:
	BoxBlur5ModulateShader() : m_source(m_setup), m_step(m_setup)
	{
	}

	virtual void Init()
	{
		m_source.init("Source", 1, 0);
		m_step.init("Step", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_source.Apply();
		m_step.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/texture_ops/box_blur_5_modulate.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	virtual void SetSource(const arl::TextureSource &source)
	{
		m_source.Set(source);
	}

	virtual void SetStep(const math::v2 &step)
	{
		m_step.Set(step);
	}

	/* no Spread: the radius is the program's */
	virtual void SetSpread(const float &spread)
	{
	}

	virtual void Reserved0()
	{
	}

	virtual void Reserved1()
	{
	}

	/* box_blur_5_modulate.fp built by psp2cgc */
	static const uint8_t s_binary[680];

	arl::ShaderParameter<arl::TextureSource> m_source;
	arl::ShaderParameter<math::v2> m_step;
};

/* the programs TextureOps draws with; 0xe28 bytes (names ours) */
class TextureOpsPrograms {
public:
	arl::ShaderInstance<ClearShader, arl::FragShaderObject> m_clear;
	arl::ShaderInstance<BicubicShader, arl::FragShaderObject> m_bicubic;
	arl::ShaderInstance<BicubicSlowShader, arl::FragShaderObject> m_bicubic_slow;
	arl::ShaderInstance<HalfShader, arl::FragShaderObject> m_half;
	arl::ShaderInstance<CopyShader, arl::FragShaderObject> m_copy;
	arl::ShaderInstance<PureCopyShader, arl::FragShaderObject> m_pure_copy;
	arl::ShaderInstance<GridNormalShader, arl::FragShaderObject> m_grid_normal;
	arl::ShaderInstance<GridNormal8Shader, arl::FragShaderObject> m_grid_normal_8;
	arl::ShaderInstance<FlipQuadShader, arl::FragShaderObject> m_flip_quad;
	arl::ShaderInstance<FlipTriShader, arl::FragShaderObject> m_flip_tri;
	arl::ShaderInstance<BoxBlurNShader, arl::FragShaderObject> m_box_blur_n;
	arl::ShaderInstance<BoxBlur1Shader, arl::FragShaderObject> m_box_blur_1;
	arl::ShaderInstance<BoxBlur2Shader, arl::FragShaderObject> m_box_blur_2;
	arl::ShaderInstance<BoxBlur3Shader, arl::FragShaderObject> m_box_blur_3;
	arl::ShaderInstance<BoxBlur4Shader, arl::FragShaderObject> m_box_blur_4;
	arl::ShaderInstance<BoxBlur5Shader, arl::FragShaderObject> m_box_blur_5;
	arl::ShaderInstance<BoxBlurNModulateShader, arl::FragShaderObject> m_box_blur_n_modulate;
	arl::ShaderInstance<BoxBlur1ModulateShader, arl::FragShaderObject> m_box_blur_1_modulate;
	arl::ShaderInstance<BoxBlur2ModulateShader, arl::FragShaderObject> m_box_blur_2_modulate;
	arl::ShaderInstance<BoxBlur3ModulateShader, arl::FragShaderObject> m_box_blur_3_modulate;
	arl::ShaderInstance<BoxBlur4ModulateShader, arl::FragShaderObject> m_box_blur_4_modulate;
	arl::ShaderInstance<BoxBlur5ModulateShader, arl::FragShaderObject> m_box_blur_5_modulate;
};

}
}

#endif
