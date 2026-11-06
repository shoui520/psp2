/*
 * A cube of six textures drawn around the camera, each face tinted by the colour that names
 * it: the environment the probe backgrounds draw. The programs' paths
 * ("paf/cg/misc/cube_map.vp" and ".fp") and their parameters' names are Sony's; the class,
 * member and function names are ours.
 */
#ifndef PAF_GRAPHICS_EFFECTS_CUBE_MAP_H
#define PAF_GRAPHICS_EFFECTS_CUBE_MAP_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <std/string>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_framebuffer_copy.h>
#include <graphics/arl/gxm/gxm_shader.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v4.h>

namespace paf {
namespace graph {

/* the cube's vertex program: the faces placed by MVP, tinted by Color */
class CubeMapVertShader : public arl::ShaderInterfaceBase {
public:
	CubeMapVertShader() : m_color(m_setup), m_mvp(m_setup)
	{
	}

	virtual void Init()
	{
		m_color.init("Color", arl::ShaderParameterBase::UNIFORM, 0);
		m_mvp.init("MVP", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_color.Apply();
		m_mvp.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/cube_map.vp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	/* cube_map.vp built by psp2cgc */
	static const uint8_t s_binary[372];

	arl::ShaderParameter<math::v4> m_color;
	arl::ShaderParameter<math::matrix> m_mvp;
};

/* the cube's fragment program: the face's texture, Tex */
class CubeMapFragShader : public arl::ShaderInterfaceBase {
public:
	CubeMapFragShader() : m_tex(m_setup)
	{
	}

	virtual void Init()
	{
		m_tex.init("Tex", 0, 0);
	}

	virtual void Apply()
	{
		m_tex.Apply();
	}

	virtual void Unused()
	{
	}

	virtual const char *GetName()
	{
		return "paf/cg/misc/cube_map.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_binary;
	}

	virtual uint32_t GetBinarySize()
	{
		return sizeof(s_binary);
	}

	/* cube_map.fp built by psp2cgc */
	static const uint8_t s_binary[264];

	arl::ShaderParameter<arl::TextureSource> m_tex;
};

/* the cube: six faces, the images "<dir>x+.dds" to "<dir>z-.dds"; 0x2b0 bytes */
class CubeMap {
public:
	enum {
		NUM_FACES = 6
	};

	/* a face: its image, its quad, and the UVs that keep its border texels out */
	class Face {
	public:
		Face() : m_uv_min(0.0f), m_uv_max(1.0f)
		{
		}

		arl::TextureSource m_texture;
		/* four vertices, a triangle strip: positions, then UVs */
		arl::VertexArray m_quad;
		/* the face's number, 0 to 5 */
		uint32_t m_index;
		float m_uv_min;
		float m_uv_max;
	};

	/* 0x8119e96a (CEX) */
	CubeMap();

	/* Sony's build keeps the destructor out of line */
	__attribute__((noinline)) ~CubeMap()
	{
	}

	/* each face's image loaded from the directory; the allocator is not used */
	void LoadFaces(common::SharedPtr<arl::VramAllocator> allocator);
	/* each face's quad made, turned to its side of the cube, and the programs set up */
	void Init();
	/* the cube drawn scaled, in a scene of its own when none is begun; nothing at 0 */
	void Draw(float scale);
	/* the directory the faces' images are read from */
	void SetDirectory(const string &dir);

	/* the colour the face is tinted by: x+ red, x- cyan, y+ green, y- magenta, z+ blue,
	   z- yellow */
	static math::v4 GetFaceColor(int32_t face);
	/* "x+", "x-", "y+", "y-", "z+", "z-", "error" past them */
	static const string &GetFaceName(int32_t face);
	/* the rotation that turns the x+ face to the face; the identity past them */
	static math::matrix GetFaceMatrix(int32_t face);

	void DrawFace(int32_t face, arl::ArlInstance &gl);
	void LoadFace(uint32_t face, common::SharedPtr<arl::VramAllocator> allocator);

	arl::ShaderInstance<CubeMapVertShader, arl::VertShaderObject> m_vert;
	arl::ShaderInstance<CubeMapFragShader, arl::FragShaderObject> m_frag;
	Face m_faces[NUM_FACES];
	string m_dir;
};

}
}

#endif
