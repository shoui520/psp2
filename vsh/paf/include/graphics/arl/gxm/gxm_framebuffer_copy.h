/*
 * paf::graph::arl's FrameBufferCopy: the screen's page copied into a texture by drawing it over
 * a frame buffer object, and the two textures (one a page of the screen) it copies with. Laid
 * out from its constructor and init (0x154 bytes). FrameBufferCopy and CopyTexImage2D are
 * Sony's (the marker CopyTexImage2D pushes), Source the sampler's name; the file's name, the
 * helper classes' and the members' names ours.
 */
#ifndef PAF_GRAPHICS_ARL_GXM_GXM_FRAMEBUFFER_COPY_H
#define PAF_GRAPHICS_ARL_GXM_GXM_FRAMEBUFFER_COPY_H

#include <stddef.h>
#include <stdint.h>
#include <gxm.h>
#include <common/shared_ptr.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_shader.h>
#include <graphics/arl/gxm/gxm_texture.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <graphics/surface/surface.h>
#include <std/memory>

namespace paf {
namespace graph {
namespace arl {

class FrameBufferObject;

/* a sampler's texture: an arl texture, else a surface made a texture (names ours) */
class TextureSource {
public:
	TextureSource()
	{
	}

	TextureSource(const common::SharedPtr<TextureObject> &texture)
	{
		*this = texture;
	}

	TextureSource &operator=(const common::SharedPtr<TextureObject> &texture)
	{
		m_texture = texture;
		m_surface.clear();
		return *this;
	}

	common::SharedPtr<TextureObject> m_texture;
	intrusive_ptr<Surface> m_surface;
};

/* binds the texture to the unit for the program's kind, vertex or fragment */
template<>
inline void ShaderParameter<TextureSource>::Apply()
{
	if (m_param != NULL) {
		if (m_value.m_texture.get() != NULL) {
			if (m_setup->m_program->m_type == ShaderProgram::VERTEX)
				sceGxmSetVertexTexture(m_setup->m_context, m_index,
						       m_value.m_texture->m_texture.get());
			if (m_setup->m_program->m_type == ShaderProgram::FRAGMENT)
				sceGxmSetFragmentTexture(m_setup->m_context, m_index,
							 m_value.m_texture->m_texture.get());
		} else if (m_value.m_surface.get() != NULL) {
			m_value.m_surface->Texturize();
			if (m_setup->m_program->m_type == ShaderProgram::VERTEX)
				sceGxmSetVertexTexture(m_setup->m_context, m_index,
						       m_value.m_surface->GetRawTexture());
			if (m_setup->m_program->m_type == ShaderProgram::FRAGMENT)
				sceGxmSetFragmentTexture(m_setup->m_context, m_index,
							 m_value.m_surface->GetRawTexture());
		}
	} else {
		m_missing++;
	}
}

/* a VramAllocator that gives out memory it was given, a page of the screen (names ours) */
class FixedVramAllocator : public HeapVramAllocator {
public:
	FixedVramAllocator(void *memory)
		: HeapVramAllocator("", common::WeakPtr<GraphMem::GraphHeapAllocator>()),
		  m_memory(memory), m_count(0)
	{
	}

	virtual ~FixedVramAllocator()
	{
	}

	virtual void Alloc(SurfaceMemory *memory, uint32_t size)
	{
		memory->m_addr = m_memory;
		memory->m_alloc_size = size;
		++m_count;
	}

	virtual void Free(SurfaceMemory *memory)
	{
		memory->m_addr = NULL;
		memory->m_alloc_size = 0;
		--m_count;
	}

	void *m_memory;
	/* the surfaces given it */
	int32_t m_count;
};

class FrameBufferCopy {
public:
	enum {
		NUM_TEXTURES = 2
	};

	/* the vertex shader, which takes no parameter */
	class VertShader : public ShaderInterfaceBase {
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
			return "paf/cg/arl/copy.vp";
		}

		virtual const uint8_t *GetBinary()
		{
			return s_binary;
		}

		virtual uint32_t GetBinarySize()
		{
			return sizeof(s_binary);
		}

		/* copy.vp built by psp2cgc */
		static const uint8_t s_binary[288];
	};

	/* the fragment shader: the texture copied */
	class FragShader : public ShaderInterfaceBase {
	public:
		FragShader() : m_source(m_setup)
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
			return "paf/cg/arl/copy.fp";
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
		static const uint8_t s_binary[260];

		ShaderParameter<TextureSource> m_source;
	};

	FrameBufferCopy();

	/* the texture over the page shown, the other one with the flag 1 */
	common::SharedPtr<Texture2D> *GetTexture(ArlInstance &gl, uint32_t flags);
	void CopyTexImage2D(ArlInstance &gl, common::SharedPtr<TextureObject> texture,
			    int32_t type, int32_t level, int32_t x, int32_t y, size_t width,
			    size_t height);

	/* the screen's pages as textures */
	common::SharedPtr<Texture2D> m_textures[NUM_TEXTURES];
	/* the frame buffer object drawn into */
	common::SharedPtr<FrameBufferObject> m_fbo;
	/* the quad drawn, over the whole viewport */
	VertexArray m_quad;
	ShaderInstance<VertShader, VertShaderObject> m_vert;
	ShaderInstance<FragShader, FragShaderObject> m_frag;
	bool m_initialized;

private:
	void init(ArlInstance &gl);
};

}
}
}

#endif
