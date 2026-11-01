/*
 * paf::graph::arl's ClearBase: clears the colour of the scene drawn into by drawing a quad over
 * it in a colour, with the clear shaders. Laid out from its constructor and clear (0x160
 * bytes). ClearBase and ClearColor are Sony's (the marker clear pushes, the uniform's name);
 * the file's name, the shader interfaces' and the members' names ours.
 */
#ifndef PAF_GRAPHICS_ARL_GXM_GXM_CLEAR_H
#define PAF_GRAPHICS_ARL_GXM_GXM_CLEAR_H

#include <stdint.h>
#include <graphics/arl/gxm/gxm_shader.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <math/ngp/v4.h>

namespace paf {
namespace graph {
namespace arl {

class ArlInstance;

class ClearBase {
public:
	/* the vertex shader: the clear colour */
	class VertShader : public ShaderInterfaceBase {
	public:
		VertShader() : m_clear_color(m_setup)
		{
		}

		virtual void Init()
		{
			m_clear_color.init("ClearColor", ShaderParameterBase::UNIFORM, 0);
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
			return "paf/cg/arl/clear.vp";
		}

		virtual const uint8_t *GetBinary()
		{
			return s_binary;
		}

		virtual uint32_t GetBinarySize()
		{
			return sizeof(s_binary);
		}

		/* clear.vp built by psp2cgc */
		static const uint8_t s_binary[288];

		ShaderParameter<math::v4> m_clear_color;
	};

	/* the fragment shader, which takes no parameter */
	class FragShader : public ShaderInterfaceBase {
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

		virtual const char *GetName()
		{
			return "paf/cg/arl/clear.fp";
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
		static const uint8_t s_binary[204];
	};

	ClearBase();

	void SetColor(math::v4 color);
	void clear(ArlInstance &gl, uint32_t clear_mask);

	bool m_initialized;
	/* the quad drawn, over the whole viewport */
	VertexArray m_quad;
	ShaderInstance<VertShader, VertShaderObject> m_vert;
	ShaderInstance<FragShader, FragShaderObject> m_frag;
	math::v4 m_color;

private:
	void init(ArlInstance &gl);
};

}
}
}

#endif
