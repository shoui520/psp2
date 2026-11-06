/*
 * The effects drawn over the scene once it is drawn: the blur of the screen behind the panels,
 * and the texture operations they share. Blur and TextureOps are Sony's names (the markers
 * "Blur::draw_uniform_blur" and "TextureOps::copy"); the depth of field's and every member name
 * are ours. Blur's code is in blur.cpp, the depth of field's in depth_of_field.cpp; the
 * texture operations' are not converted yet (0x811a16f2..).
 */
#ifndef PAF_GRAPHICS_EFFECTS_POST_EFFECTS_H
#define PAF_GRAPHICS_EFFECTS_POST_EFFECTS_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <std/string>
#include <std/vector>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_framebuffer_copy.h>
#include <graphics/arl/gxm/gxm_shader.h>
#include <graphics/arl/gxm/gxm_texture.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>

namespace paf {
namespace graph {

/* the rectangle of the screen a quad covers, 0 to 1 (names ours) */
class QuadArea {
public:
	QuadArea()
	{
	}

	QuadArea(math::v2 min, math::v2 max) : m_min(min), m_max(max)
	{
	}

	math::v2 m_min;
	math::v2 m_max;
};

/* the unit quad drawn over the area: the area's scale and offset, then 0 to 1 across the
   screen (0x81203f2c, name ours) */
__attribute__((noinline)) math::matrix GetQuadMatrix(const QuadArea &area);

inline math::matrix GetQuadMatrix(const QuadArea &area)
{
	math::v2 size = area.m_max - area.m_min;

	return math::matrix::Ortho(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f) *
	       math::matrix(math::v4(size.extract_x(), 0.0f, 0.0f, 0.0f),
			    math::v4(0.0f, size.extract_y(), 0.0f, 0.0f),
			    math::v4(0.0f, 0.0f, 1.0f, 0.0f),
			    math::v4(area.m_min.extract_x(), area.m_min.extract_y(), 0.0f, 1.0f));
}

/* the quad the texture operations draw, placed by MVP, its UVs by UVtransform (the parameters'
   names the program's, the class name ours) */
class QuadVertShader : public arl::ShaderInterfaceBase {
public:
	QuadVertShader() : m_mvp(m_setup), m_uv_transform(m_setup)
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

	/* "paf/cg/misc/texture_ops/quad.vp" */
	virtual const char *GetName();
	virtual const uint8_t *GetBinary();
	virtual uint32_t GetBinarySize();

	arl::ShaderParameter<math::matrix> m_mvp;
	/* the UVs' offset, then their scale */
	arl::ShaderParameter<math::v4> m_uv_transform;
};

/* the operations drawn from one texture into another (copy, half, bicubic, gridNormal8, ...),
   their programs and textures; 0x188 bytes */
class TextureOps {
public:
	/* 0x811a16f2 */
	TextureOps();
	/* 0x811fe8e4: implicit, emitted with the programs */
	~TextureOps();

	/* 0x811a5422: the normals of the grid of positions in source drawn into the level of
	   normals, in a scene begun with the flags (arguments' names ours) */
	void gridNormal8(common::SharedPtr<arl::Texture2D> &normals,
			 common::SharedPtr<arl::Texture2D> &source, uint32_t level,
			 uint32_t scene_flags);
	/* the area of the destination drawn into and the area of the source read (names
	   ours) */
	class CopyArea {
	public:
		QuadArea m_dest;
		QuadArea m_source;
	};

	/* 0x811a2bb6: the area of source drawn into the level of dest, faded by alpha (the
	   program's "Alpha"; arguments' names ours) */
	void copy(const common::SharedPtr<arl::Texture2D> &dest,
		  const common::SharedPtr<arl::Texture2D> &source, uint32_t level,
		  const CopyArea &area, float alpha);
	/* 0x811a31f4: the area of source drawn into the level of dest unchanged (arguments'
	   names ours) */
	void pureCopy(const common::SharedPtr<arl::Texture2D> &dest,
		      const common::SharedPtr<arl::Texture2D> &source, uint32_t level,
		      const CopyArea &area);
	/* 0x811a37e4: the area of source drawn into the level of dest at half its size; the
	   flags are not read (arguments' names ours) */
	void half(const common::SharedPtr<arl::Texture2D> &dest,
		  const common::SharedPtr<arl::Texture2D> &source, uint32_t level,
		  const CopyArea &area, bool flag0, bool flag1);
	/* 0x811a3de8 and 0x811a4732: source scaled into the level of dest, bicubic (arguments'
	   names ours) */
	void bicubic(const common::SharedPtr<arl::Texture2D> &dest,
		     const common::SharedPtr<arl::Texture2D> &source, uint32_t level);
	void bicubicSlow(const common::SharedPtr<arl::Texture2D> &dest,
			 const common::SharedPtr<arl::Texture2D> &source, uint32_t level);
	/* 0x811a4dd6: the texture box blurred, through a temporary of its size (arguments'
	   names ours) */
	void boxBlur(const common::SharedPtr<arl::Texture2D> &texture,
		     const common::SharedPtr<arl::Texture2D> &temporary);
	/* 0x811a5afe: source drawn into the level of dest flipped (arguments' names ours) */
	void fliptri(common::SharedPtr<arl::Texture2D> &dest, common::SharedPtr<arl::Texture2D> &source,
		     uint32_t level);

	/* the unit quad drawn with the shaders begun (name ours) */
	void DrawQuad()
	{
		arl::ArlInstance &gl = *arl::Gl();
		arl::ShaderState *state = &gl;

		state->m_dirty_vertex |= m_quad.m_spec->m_kinds;
		state = &gl;
		state->m_dirty_fragment |= 1;
		sceGxmSetVertexStream(gl.m_context, 0, m_quad.m_buffer->m_memory->m_addr);
		arl::VertexArray::DrawArrays(gl, arl::TRIANGLE_STRIP, 0, 4);
		state = &gl;
		state->m_dirty_vertex = 0;
		state = &gl;
		state->m_dirty_fragment = 0;
	}

	uint32_t reserved[8];
	/* the unit quad, a triangle strip, the operations are drawn with (name ours) */
	arl::VertexArray m_quad;
	uint32_t reserved1[4];
	arl::ShaderInstance<QuadVertShader, arl::VertShaderObject> m_vert;
	uint32_t reserved2[(0x188 - 0x180) / 4];
};

/* the uniform blur's program: the copy's levels, faded by Alpha (the parameters' names the
   program's, the class name ours) */
class UniformBlurShader : public arl::ShaderInterfaceBase {
public:
	UniformBlurShader() : m_blur_levels(m_setup), m_alpha(m_setup)
	{
	}

	virtual void Init()
	{
		m_blur_levels.init("BlurLevels", 2, 0);
		m_alpha.init("Alpha", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_blur_levels.Apply();
		m_alpha.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/blur/uniform_blur.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	/* uniform_blur.fp built by psp2cgc */
	static const uint8_t s_binary[312];

	arl::ShaderParameter<arl::TextureSource> m_blur_levels;
	arl::ShaderParameter<float> m_alpha;
};

/* the variable blur's program: the copy's levels by a mask (the parameters' names the
   program's, the class name ours); nothing draws with it */
class VariableBlurShader : public arl::ShaderInterfaceBase {
public:
	VariableBlurShader() : m_blur_levels(m_setup), m_mask(m_setup), m_nmips(m_setup)
	{
	}

	virtual void Init()
	{
		m_blur_levels.init("BlurLevels", 2, 0);
		m_mask.init("Mask", 3, 0);
		m_nmips.init("Nmips", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_blur_levels.Apply();
		m_mask.Apply();
		m_nmips.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/blur/variable_blur.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	/* variable_blur.fp built by psp2cgc */
	static const uint8_t s_binary[452];

	arl::ShaderParameter<arl::TextureSource> m_blur_levels;
	arl::ShaderParameter<arl::TextureSource> m_mask;
	arl::ShaderParameter<float> m_nmips;
};

/* blurs a copy of the screen in steps and draws it back; 0x1f0 bytes */
class Blur {
public:
	enum {
		/* the smaller copies the capture is blurred from */
		NUM_LEVELS = 3
	};

	/* 0x8119dd28 */
	Blur();
	/* the destructor is implicit (0x811f0efc), emitted with the renderer's */

	/* 0x8119de6c: the texture of the screen captured; the size is taken the first time
	   only, the amount of blur is in parts of the capture's levels */
	void Capture(common::SharedPtr<arl::Texture2D> texture, arl::SurfaceSize size,
		     const math::v2 &amount);
	/* 0x8119eba2: the blurred copy drawn over the screen by the amount, 0 to 1 */
	void Draw(float amount);

	void SetTextureOps(common::SharedPtr<TextureOps> texture_ops)
	{
		m_texture_ops = texture_ops;
	}

	/* 0x8119d51c: a texture the size and format of another to blur through, kept for
	   the next time (name ours) */
	common::SharedPtr<arl::Texture2D> GetTemporary(const common::SharedPtr<arl::Texture2D> &like);
	/* 0x8119d772 */
	void draw_uniform_blur(float amount);

	/* the screen at half its size, its mip levels drawn from the smaller copies */
	common::SharedPtr<arl::Texture2D> m_capture;
	common::SharedPtr<TextureOps> m_texture_ops;
	/* the amount of blur in levels, from Capture's amount */
	math::v2 m_amount;
	/* a digit for each of the capture's levels, '?' from the tenth */
	string m_level_names;
	/* the copies at a quarter, an eighth and a sixteenth of the size, each blurred, through
	   m_levels */
	common::SharedPtr<arl::Texture2D> m_level_textures[NUM_LEVELS];
	common::SharedPtr<arl::Texture2D> *m_levels;
	uint32_t m_num_levels;
	uint32_t m_max_levels;
	/* the textures GetTemporary keeps */
	vector<common::SharedPtr<arl::Texture2D> > m_temporaries;
	/* the copy a level of the capture is drawn from: its index less one, and level 0 a plain
	   copy of the first copy, or drawn like the others */
	int32_t m_level_offset;
	bool m_copy_first_level;
	/* written by the constructor only */
	float m_strength;
	/* the levels drawn bicubic, slow or fast, else copied */
	bool m_bicubic_slow;
	bool m_bicubic;
	/* the box blur passes over the copies, the smallest first */
	uint32_t m_box_blur_passes;
	/* written by the constructor only, a value for each copy */
	float m_quarter_scale;
	float m_eighth_scale;
	float m_sixteenth_scale;
	float m_alpha_scale;
	/* passed to TextureOps::half, which does not read it */
	bool m_half_flag;
	bool m_initialized;
	/* the size of the texture captured the first time */
	int32_t m_texture_width;
	int32_t m_texture_height;
	/* the size of the screen in it */
	int32_t m_width;
	int32_t m_height;
	/* the powers of two nearest the texture's size, which the copies take when m_pow2 */
	int32_t m_pow2_width;
	int32_t m_pow2_height;
	/* the part of the capture the screen fills, and of the texture it was taken from */
	QuadArea m_capture_area;
	QuadArea m_source_area;
	bool m_pow2;
	arl::ShaderInstance<UniformBlurShader, arl::FragShaderObject> m_uniform_blur;
	arl::ShaderInstance<VariableBlurShader, arl::FragShaderObject> m_variable_blur;
};

/* the depth of field's first program: the blur amount from Depth, by the distance between
   NearFar and the two curves (the parameters' names the program's, the class name ours) */
class DobInitShader : public arl::ShaderInterfaceBase {
public:
	DobInitShader() : m_depth(m_setup), m_near_far(m_setup), m_curve1(m_setup), m_curve2(m_setup)
	{
	}

	virtual void Init()
	{
		m_depth.init("Depth", 1, 0);
		m_near_far.init("NearFar", arl::ShaderParameterBase::UNIFORM, 0);
		m_curve1.init("Curve1", arl::ShaderParameterBase::UNIFORM, 0);
		m_curve2.init("Curve2", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_depth.Apply();
		m_near_far.Apply();
		m_curve1.Apply();
		m_curve2.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/dof/dob_init.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	/* dob_init.fp built by psp2cgc */
	static const uint8_t s_binary[552];

	arl::ShaderParameter<arl::TextureSource> m_depth;
	arl::ShaderParameter<math::v2> m_near_far;
	arl::ShaderParameter<math::v4> m_curve1;
	arl::ShaderParameter<math::v4> m_curve2;
};

/* the depth of field's second program: the amount spread from Source by Step (the parameters'
   names the program's, the class name ours) */
class DobSpreadShader : public arl::ShaderInterfaceBase {
public:
	DobSpreadShader() : m_source(m_setup), m_step(m_setup)
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
		return "paf/cg/misc/dof/dob_spread.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	/* dob_spread.fp built by psp2cgc */
	static const uint8_t s_binary[592];

	arl::ShaderParameter<arl::TextureSource> m_source;
	arl::ShaderParameter<math::v2> m_step;
};

/* the depth of field's last program: Dob and DobBlurred mixed (the parameters' names the
   program's, the class name ours) */
class DobCombineShader : public arl::ShaderInterfaceBase {
public:
	DobCombineShader() : m_dob(m_setup), m_dob_blurred(m_setup)
	{
	}

	virtual void Init()
	{
		m_dob.init("Dob", 1, 0);
		m_dob_blurred.init("DobBlurred", 2, 0);
	}

	virtual void Apply()
	{
		m_dob.Apply();
		m_dob_blurred.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/dof/dob_combine.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	/* dob_combine.fp built by psp2cgc */
	static const uint8_t s_binary[332];

	arl::ShaderParameter<arl::TextureSource> m_dob;
	arl::ShaderParameter<arl::TextureSource> m_dob_blurred;
};

/* a cubic Hermite curve y(x) over x in [0, 1]: its end values and slopes, its polynomial, and
   the lowest and highest point it reaches there (names ours) */
class HermiteCurve {
public:
	/* a point where the slope is zero: x in both lanes */
	class Extremum {
	public:
		Extremum() : m_x(0.0f, 0.0f), m_valid(false)
		{
		}

		math::v2 m_x;
		/* not set */
		math::v2 m_point;
		bool m_valid;
	};

	/* the curve from y(0), y(1), y'(0) and y'(1) */
	HermiteCurve(const math::v4 &points) : m_inverse_range(0.0f, 0.0f)
	{
		Set(points);
	}

	/* the points taken, the polynomial and its bounds worked out */
	void Set(const math::v4 &points);

	/* y(0), y(1), y'(0), y'(1) */
	math::v4 m_points;
	/* a, b, c, d of a x^3 + b x^2 + c x + d */
	math::v4 m_coeffs;
	/* the lowest and highest (x, y) */
	math::v2 m_min;
	math::v2 m_max;
	/* 1 / (highest y - lowest y), in both lanes */
	math::v2 m_inverse_range;
	/* nothing sets or reads it */
	math::v2 m_reserved;
	Extremum m_extrema[2];
};

/* the depth of field the renderer keeps: its three programs, the textures they draw, and the
   curve the blur amount follows; nothing in libpaf draws it. 0x2e8 bytes (names ours) */
class DepthOfField {
public:
	/* 0x811a0f64 */
	DepthOfField();
	/* the destructor is implicit (0x811f1198), emitted with the renderer's */

	bool m_initialized;
	arl::ShaderInstance<DobInitShader, arl::FragShaderObject> m_init;
	arl::ShaderInstance<DobSpreadShader, arl::FragShaderObject> m_spread;
	arl::ShaderInstance<DobCombineShader, arl::FragShaderObject> m_combine;
	common::SharedPtr<arl::Texture2D> m_targets[3];
	math::v2 m_scale;
	bool m_half_size;
	uint32_t m_width;
	uint32_t m_height;
	HermiteCurve m_curve;
	/* the values the constructor sets (16.4, 1, 3, 5, false, 3, 5, 1, 1); nothing in libpaf
	   reads them */
	float m_settings[4];
	bool m_setting_flag;
	float m_settings2[4];
	/* nothing sets or reads it */
	uint32_t m_reserved;
	common::SharedPtr<arl::Texture2D> m_temporary;
};

}
}

#endif
