/*
 * The effects drawn over the scene once it is drawn: the blur of the screen behind the panels,
 * and the texture operations they share. Blur and TextureOps are Sony's names (the markers
 * "Blur::draw_uniform_blur" and "TextureOps::copy"); the filter's and every member name are
 * ours. Their code is not converted yet: Blur's is at
 * 0x8119d772.., the filter's and the programs' at 0x811a0f64.. and 0x811a16f2.
 */
#ifndef PAF_GRAPHICS_EFFECTS_POST_EFFECTS_H
#define PAF_GRAPHICS_EFFECTS_POST_EFFECTS_H

#include <stdint.h>
#include <common/shared_ptr.h>
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

	/* 0x811a31f4: the area of source drawn into the level of dest unchanged (arguments'
	   names ours) */
	void pureCopy(const common::SharedPtr<arl::Texture2D> &dest,
		      const common::SharedPtr<arl::Texture2D> &source, uint32_t level,
		      const CopyArea &area);
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

/* blurs a copy of the screen in steps and draws it back; 0x1f0 bytes */
class Blur {
public:
	/* 0x8119dd28 */
	Blur();
	/* 0x811f0efc: implicit, emitted with the renderer's destructor */
	~Blur();

	/* 0x8119de6c: the texture of the screen captured at a size, blurred from the first to
	   the second amount */
	void Capture(common::SharedPtr<arl::Texture2D> texture, arl::SurfaceSize size,
		     const math::v2 &amount);
	/* 0x8119eba2: the blurred copy drawn by the amount */
	void Draw(float amount);

	void SetTextureOps(common::SharedPtr<TextureOps> texture_ops)
	{
		m_texture_ops = texture_ops;
	}

	uint8_t reserved[8];
	common::SharedPtr<TextureOps> m_texture_ops;
	uint8_t reserved2[0x1e0];
};

/* a filter drawn over the screen; 0x2ec bytes */
class ScreenFilter {
public:
	/* 0x811a0f64 */
	ScreenFilter();
	/* 0x811f1198: implicit, emitted with the renderer's destructor */
	~ScreenFilter();

	uint32_t reserved[0x2e8 / 4];
	/* the flag ScePafGraphics_505CD74E sets; nothing in libpaf reads it by this offset */
	bool m_flag_505CD74E;
	uint8_t reserved2[3];
};

}
}

#endif
