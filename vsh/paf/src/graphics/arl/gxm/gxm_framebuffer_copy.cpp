/*
 * paf::graph::arl's FrameBufferCopy: the screen's page copied into a texture (the file's name
 * ours).
 */
#include <stddef.h>
#include <stdint.h>
#include <gxm.h>
#include <src/common/frame_count.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_framebuffer_copy.h>
#include <graphics/arl/gxm/gxm_shader.h>
#include <graphics/arl/gxm/gxm_texture.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <graphics/color.h>
#include <graphics/surface/surface.h>
#include <math/ngp/v2.h>
#include <std/memory>
#include "gxm_framebuffer_object.h"

namespace paf {
namespace graph {
namespace arl {

void FrameBufferCopy::init(ArlInstance &gl)
{
	if (m_initialized)
		return;
	m_initialized = true;

	for (int32_t i = 0; i != NUM_TEXTURES; i++) {
		void *memory = intrusive_ptr<Surface>(gl.m_next_scene.m_surface)->Lock(i);

		intrusive_ptr<Surface>(gl.m_next_scene.m_surface)->Unlock();

		common::SharedPtr<FixedVramAllocator> allocator(new FixedVramAllocator(memory));

		m_textures[i] = common::SharedPtr<Texture2D>(new Texture2D(allocator));

		SurfaceSize size = ScePafGraphics_AC339F46(&gl);

		ScePafGraphics_C4BD3558(m_textures[i].get(), size.m_width, size.m_height,
					ScePafGraphics_96173730(&gl), 0, ScePafGraphics_BB119805(&gl));
	}

	m_fbo = common::SharedPtr<FrameBufferObject>(new FrameBufferObject());

	float vertices[] = {
		-1.0f, 1.0f,
		-1.0f, -1.0f,
		1.0f, 1.0f,
		1.0f, -1.0f,
	};

	m_quad = VertexArray(gl.m_default_allocator,
			     ScePafGraphics_4D1E289A(&gl, VertexArraySpec::ATTRIB0_F2,
						     SCE_GXM_INDEX_SOURCE_INDEX_16BIT),
			     vertices, 4);

	common::SharedPtr<VertexArraySpec> spec = m_quad.m_spec;
	ProgramState *state = &gl;

	m_vert.m_object.init(gl, state->m_patcher, spec, common::SharedPtr<VertexArraySpec>(),
			   common::SharedPtr<VertexArraySpec>(),
			   common::SharedPtr<VertexArraySpec>());
	state = &gl;
	m_frag.m_object.init(gl, state->m_patcher);
}

FrameBufferCopy::FrameBufferCopy() : m_initialized(false)
{
}

/* Sony's build calls it from CopyTexImage2D */
__attribute__((noinline)) common::SharedPtr<Texture2D> *FrameBufferCopy::GetTexture(ArlInstance &gl, uint32_t flags)
{
	uint32_t index;

	init(gl);
	if (intrusive_ptr<Surface>(gl.m_next_scene.m_surface).get() != NULL)
		index = intrusive_ptr<Surface>(gl.m_next_scene.m_surface)->GetPageCounter();
	else
		index = 1 - (common::FrameCount() & 1);
	if (flags & 1)
		index = 1 - index;
	return &m_textures[index];
}

void FrameBufferCopy::CopyTexImage2D(ArlInstance &gl, common::SharedPtr<TextureObject> texture,
				     int32_t type, int32_t level, int32_t x, int32_t y,
				     size_t width, size_t height)
{
	if (texture.get() == NULL)
		return;

	ScePafGraphics_E7B32741(&gl);
	init(gl);
	gl.m_viewport.Push();

	{
		int32_t width = texture->m_desc.m_width;
		int32_t height = texture->m_desc.m_height;
		Rect viewport;

		viewport.m_x0 = 0;
		viewport.m_y0 = 0;
		viewport.m_x1 = width;
		viewport.m_y1 = height;
		ScePafGraphics_93CBC733(&gl, viewport);
	}

	{
		int32_t width = texture->m_desc.m_width;
		int32_t height = texture->m_desc.m_height;
		Rect scissor;

		scissor.m_x0 = 0;
		scissor.m_y0 = 0;
		scissor.m_x1 = width;
		scissor.m_y1 = height;
		ScePafGraphics_56C831A9(&gl, scissor);
	}

	SceneDataStack *scenes = &gl;

	scenes->Push();
	scenes = &gl;
	scenes->Set(m_fbo.get());
	m_fbo->AttachColor(texture, 0);
	ScePafGraphics_199BD5F2(&gl, 0);
	gl.PushMarker("FrameBufferCopy::CopyTexImage2D");
	gl.CullMode(CULL_NONE);
	gl.Disable(BLEND);
	gl.Disable(DEPTH_TEST);
	gl.DepthMask(false, FRONT_AND_BACK);
	gl.ColorMask(true, true, true, true);
	{
		common::SharedPtr<Texture2D> source = *GetTexture(gl, 0);

		m_frag.m_interface.m_source.Set(common::SharedPtr<TextureObject>(source));
	}
	m_vert.m_object.begin();
	m_frag.m_object.begin();

	ShaderState *state = &gl;

	state->m_dirty_vertex |= m_quad.m_spec->m_kinds;
	state = &gl;
	state->m_dirty_fragment |= 1;
	sceGxmSetVertexStream(gl.m_context, 0, m_quad.m_buffer->m_memory->m_addr);
	VertexArray::DrawArrays(gl, TRIANGLE_STRIP, 0, 4);
	state = &gl;
	state->m_dirty_vertex = 0;
	state = &gl;
	state->m_dirty_fragment = 0;
	m_frag.m_object.end();
	m_vert.m_object.end();
	gl.PopMarker();
	ScePafGraphics_29F5AA8B(&gl, NULL, true);
	scenes = &gl;
	scenes->Pop();
	gl.m_viewport.Pop();
	ScePafGraphics_93CBC733(&gl, gl.m_viewport.Top().m_viewport);
	ScePafGraphics_56C831A9(&gl, gl.m_viewport.Top().m_scissor);
}

}
}
}
