/*
 * paf::graph::GateBlur: the blur drawn over the screen while an application's gate opens or
 * closes. Opening, the screen is drawn faded, three blurred copies of the gate grow out of the
 * point it opens from, then the application's surface grows from the gate; closing, the
 * surface shrinks back into the point. The class, Initialize, Finalize, Draw, Draw_ and GetMode
 * are Sony's names (the export NIDs hash as them, suffix "PAF"), the marker
 * "posteffects::draw_gate_blur" and the programs' paths Sony's; every other name is ours.
 */
#ifndef PAF_GRAPHICS_EFFECTS_GATE_BLUR_H
#define PAF_GRAPHICS_EFFECTS_GATE_BLUR_H

#include <stdint.h>
#include <std/memory>
#include <graphics/arl/gxm/gxm_shader.h>
#include <graphics/arl/gxm/gxm_framebuffer_copy.h>
#include <graphics/arl/gxm/gxm_texture.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <graphics/surface/surface.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>

namespace paf {
namespace graph {

/* gate_blur.vp: the vertices placed by uMat (the parameter's name the program's, the class
   name ours) */
class GateBlurVertShader : public arl::ShaderInterfaceBase {
public:
	GateBlurVertShader() : m_mat(m_setup)
	{
	}

	virtual void Init()
	{
		m_mat.init("uMat", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mat.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/posteffects/gate_blur/gate_blur.vp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	/* gate_blur.vp built by psp2cgc */
	static const uint8_t s_binary[400];

	arl::ShaderParameter<math::matrix> m_mat;
};

/* gate_blur.fp: uColorTex tinted by the vertices' colour and uColor (the parameters' names the
   program's, the class name ours); Draw_ binds the texture itself */
class GateBlurFragShader : public arl::ShaderInterfaceBase {
public:
	GateBlurFragShader() : m_color_tex(m_setup), m_color(m_setup)
	{
	}

	virtual void Init()
	{
		m_color_tex.init("uColorTex", 0, 0);
		m_color.init("uColor", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_color_tex.Apply();
		m_color.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/posteffects/gate_blur/gate_blur.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	/* gate_blur.fp built by psp2cgc */
	static const uint8_t s_binary[320];

	arl::ShaderParameter<arl::TextureSource> m_color_tex;
	arl::ShaderParameter<math::v4> m_color;
};

/* the two programs, made once by GateBlur's constructor; 0x1b8 bytes (name ours) */
class GateBlurPrograms {
public:
	arl::ShaderInstance<GateBlurVertShader, arl::VertShaderObject> m_vert;
	arl::ShaderInstance<GateBlurFragShader, arl::FragShaderObject> m_frag;
};

/* a vertex of the gate's meshes: where, the texture coordinate, the colour (name ours) */
class GateBlurVertex {
public:
	float m_position[3];
	float m_uv[2];
	float m_color[4];
};

/* GateBlur's programs, which the launch effect also draws with (name ours) */
extern GateBlurPrograms *s_gate_blur_programs;

/* 0x24 bytes */
class GateBlur {
public:
	/* what is drawn (names ours) */
	enum Mode {
		Mode_None = 0,
		/* the gate opens: the screen fades, the gate and then the surface grow */
		Mode_Open = 1,
		/* the gate closes: the surface shrinks into the point */
		Mode_Close = 2,
		/* the screen stays faded, the time stopped */
		Mode_Hold = 3
	};

	typedef void (*FinishCallback)(void *arg);

	/* 0x810d30c8 */
	GateBlur();
	/* 0x810d3e72 */
	~GateBlur();

	/* 0x810d3e60: the instance made */
	static void Initialize();
	/* 0x810d41cc: the instance deleted */
	static void Finalize();
	/* 0x810d505c: the instance drawn, when there is one */
	static void Draw();
	/* 0x810d5214 */
	static int32_t GetMode();

	/* 0x810d41e4 */
	void Draw_();

	/* the instance's callback called with its argument once, and forgotten (name ours) */
	static __attribute__((always_inline)) void CallFinish()
	{
		FinishCallback callback = s_instance->m_finish;
		void *arg = s_instance->m_finish_arg;

		s_instance->m_finish = NULL;
		s_instance->m_finish_arg = NULL;
		if (callback != NULL)
			callback(arg);
	}

	/* the instance, and the meshes and programs it draws with (names ours) */
	static GateBlur *s_instance;
	static arl::VertexArray *s_quad;
	static arl::VertexArray *s_faded;
	static arl::VertexArray *s_gate;
	static arl::IndexArray *s_gate_index;
	static GateBlurPrograms *s_programs;

	int32_t m_mode;
	/* the seconds since the mode was set */
	float m_time;
	/* the point the gate opens from, -1 to 1 */
	float m_x;
	float m_y;
	/* what grows out of the gate, or shrinks into the point */
	intrusive_ptr<Surface> m_surface;
	/* drawn over the gate as it opens, when it is set */
	intrusive_ptr<Surface> *m_cover;
	/* the opening is slower and its screen fades out */
	bool m_slow;
	/* the closing fades the surface out */
	bool m_fade;
	/* called when the mode ends, with its argument */
	FinishCallback m_finish;
	void *m_finish_arg;
};

}
}

extern "C" {
/* the gate set opening from (x, y) (vitasdk: none) */
void ScePafGraphics_16BDBC6E(bool slow, float x, float y,
			     const paf::intrusive_ptr<paf::graph::Surface> &surface,
			     paf::intrusive_ptr<paf::graph::Surface> *cover,
			     paf::graph::GateBlur::FinishCallback finish, void *arg);
/* the gate set closing into (x, y) (vitasdk: none) */
void ScePafGraphics_6BBFE4F7(bool fade, float x, float y,
			     const paf::intrusive_ptr<paf::graph::Surface> &surface,
			     paf::graph::GateBlur::FinishCallback finish, void *arg);
/* the gate stopped, when it is drawn (vitasdk: none) */
void ScePafGraphics_04F03872(void);
/* the gate held (vitasdk: none) */
void ScePafGraphics_12BDEBFF(void);
}

#endif
