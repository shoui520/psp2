/*
 * paf::graph::arl's frame buffer objects: a colour, a depth and a stencil attachment, each a
 * level of a texture or a render buffer, and the attachment that sets the render target's
 * size. The file's name and the class name Sony's; the members' names ours.
 */
#ifndef PAF_SRC_GRAPHICS_ARL_GXM_GXM_FRAMEBUFFER_OBJECT_H
#define PAF_SRC_GRAPHICS_ARL_GXM_GXM_FRAMEBUFFER_OBJECT_H

#include <stddef.h>
#include <stdint.h>
#include <gxm.h>
#include <common/shared_ptr.h>
#include <paf_build.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_render_target_pool.h>
#include <graphics/arl/gxm/gxm_texture.h>

/* this file's path in Sony's build, which the render target pool is given */
#define GXM_FRAMEBUFFER_OBJECT_FILE PAF_BUILD_ROOT "/src/graphics/arl/gxm/gxm_framebuffer_object.h"

namespace paf {
namespace graph {
namespace arl {

/* GXM's surfaces made by whoever attaches it; no code in paf makes one */
class RenderBuffer {
public:
	uint8_t m_reserved0[16];
	SceGxmColorSurface *m_color_surface;
	uint8_t m_reserved1[92];
	SceGxmDepthStencilSurface *m_depth_stencil_surface;
};

class FrameBufferObject {
public:
	/* the attachments' indices (names ours) */
	enum {
		COLOR,
		DEPTH,
		STENCIL,
		NUM_ATTACHMENTS
	};

	class Attachment {
	public:
		Attachment() : m_level(0), m_width(0), m_height(0)
		{
		}

		/* the level drawn into, and the texture's size at level 0 */
		uint32_t m_level;
		int32_t m_width;
		int32_t m_height;
		common::SharedPtr<TextureObject> m_texture;
		common::SharedPtr<RenderBuffer> m_buffer;
	};

	FrameBufferObject() : m_size_source(NULL)
	{
	}

	/* the same, out of line: Sony's TextureOps constructor calls it, where the frame buffer
	   copy has it inline (the tag and its name ours) */
	struct OutOfLine {
	};

	explicit __attribute__((noinline)) FrameBufferObject(OutOfLine) : m_size_source(NULL)
	{
	}

	/* 0x81194264: drawn into from here on: the texture's level as the attachment, the
	   viewport and scissor its size, the scene pushed (names ours) */
	void Bind(const common::SharedPtr<TextureObject> &texture, uint32_t level,
		  uint32_t attachment);
	/* 0x8119446c: the scene, viewport and scissor before the last Bind */
	static void Unbind();

	/* a texture's level drawn into as the colour, the render target sized by it (name
	   ours) */
	void AttachColor(const common::SharedPtr<TextureObject> &texture, uint32_t level)
	{
		m_attachments[COLOR].m_level = level;

		int32_t width = texture->m_desc.m_width;
		int32_t height = texture->m_desc.m_height;

		m_attachments[COLOR].m_width = width;
		m_attachments[COLOR].m_height = height;
		m_attachments[COLOR].m_texture = texture;
		m_attachments[COLOR].m_buffer.reset();
		m_size_source = &m_attachments[COLOR];
	}

	/* the size of the level drawn into, nothing without an attachment to take it from */
	SurfaceSize GetSize()
	{
		if (m_size_source == NULL)
			return SurfaceSize(0, 0);
		return SurfaceSize(m_size_source->m_width >> m_size_source->m_level,
				   m_size_source->m_height >> m_size_source->m_level);
	}

	/* a render target of the size from the pool, none without a size */
	SceGxmRenderTarget *GetRenderTarget()
	{
		if (m_size_source == NULL)
			return NULL;

		RenderTargetPool::Key key;

		key.m_width = m_size_source->m_width >> m_size_source->m_level;
		key.m_height = m_size_source->m_height >> m_size_source->m_level;
		key.m_multisample_mode = SCE_GXM_MULTISAMPLE_NONE;
		return Gl()->m_render_target_pool->GetRenderTarget(key, GXM_FRAMEBUFFER_OBJECT_FILE);
	}

	SceGxmColorSurface *GetColorSurface()
	{
		if (m_attachments[COLOR].m_buffer.get() != NULL)
			return m_attachments[COLOR].m_buffer->m_color_surface;
		if (m_attachments[COLOR].m_texture.get() == NULL)
			return NULL;
		m_attachments[COLOR].m_texture->PrepareColorSurface(m_attachments[COLOR].m_level);
		return m_attachments[COLOR]
			.m_texture->m_color_surfaces[m_attachments[COLOR].m_level]
			.get();
	}

	/* Sony's build keeps this one out of line */
	__attribute__((noinline)) SceGxmDepthStencilSurface *GetDepthStencilSurface()
	{
		if (m_attachments[DEPTH].m_buffer.get() != NULL)
			return m_attachments[DEPTH].m_buffer->m_depth_stencil_surface;
		if (m_attachments[DEPTH].m_texture.get() != NULL) {
			m_attachments[DEPTH].m_texture->PrepareDepthStencilSurface();
			return m_attachments[DEPTH].m_texture->m_depth_stencil_surface.get();
		}
		if (m_attachments[STENCIL].m_buffer.get() != NULL)
			return m_attachments[STENCIL].m_buffer->m_depth_stencil_surface;
		if (m_attachments[STENCIL].m_texture.get() != NULL) {
			m_attachments[STENCIL].m_texture->PrepareDepthStencilSurface();
			return m_attachments[STENCIL].m_texture->m_depth_stencil_surface.get();
		}
		return NULL;
	}

	/* Sony's deleter destroys them as one array */
	Attachment m_attachments[NUM_ATTACHMENTS];
	/* the attachment the render target takes its size from */
	Attachment *m_size_source;
};

}
}
}

#endif
