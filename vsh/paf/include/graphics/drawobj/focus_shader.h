/*
 * The shader the key focus's planes are drawn with ("paf/cg/drawobj/focus/focus.vp" and
 * ".fp"): the focus frame's texture lit in three shades of one colour. Its programs, the shader
 * and a blank texture are made once for every Init and given back by the last Term. The program
 * paths and parameter names are Sony's (their strings); every class, method and member name is
 * ours.
 */
#ifndef PAF_GRAPHICS_DRAWOBJ_FOCUS_SHADER_H
#define PAF_GRAPHICS_DRAWOBJ_FOCUS_SHADER_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/v4.h>

namespace paf {
namespace graph {

class FocusVertShader;
class FocusFragShader;

/* 0x48 bytes */
class FocusShader : public Shader {
public:
	typedef arl::ShaderInstance<FocusVertShader, arl::VertShaderObject> VertInstance;
	typedef arl::ShaderInstance<FocusFragShader, arl::FragShaderObject> FragInstance;

	/* 0x811a83cc: the programs Init made, the colours white */
	FocusShader();
	/* 0x811a844a */
	virtual ~FocusShader();

	/* 0x811a851a: the plane's first texture, else the blank one, in the three colours */
	virtual int32_t Draw(PlaneObjDrawParams &params);

	/* 0x811a8ba2: the programs, the shader and the blank texture made by the first */
	static void Init();
	/* 0x811a9394: all of it given back by the last */
	static void Term();
	/* 0x811a9544: the shader the focus's planes are given */
	static common::SharedPtr<Shader> &GetShader();
	/* 0x811a954e: the three shades of a colour: its value and alpha at 0.6, its saturation
	   at 0.7, and itself */
	static void SetColor(const math::v4 &color);

	common::SharedPtr<VertInstance> m_vert;
	common::SharedPtr<FragInstance> m_frag;
	/* "BaseColor" */
	math::v4 m_base_color;
	/* "GlowColor" */
	math::v4 m_glow_color;
	/* "Color" */
	math::v4 m_color;
};

}
}

#endif
