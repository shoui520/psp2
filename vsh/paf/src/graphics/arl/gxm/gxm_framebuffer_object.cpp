/*
 * paf::graph::arl's FrameBufferObject: drawn into from Bind to Unbind (the file's name follows
 * Sony's gxm_framebuffer_object.h; the functions' names ours).
 */
#include <stddef.h>
#include <stdint.h>
#include <gxm.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_texture.h>
#include "gxm_framebuffer_object.h"

namespace paf {
namespace graph {
namespace arl {

void FrameBufferObject::Bind(const common::SharedPtr<TextureObject> &texture, uint32_t level,
			     uint32_t attachment)
{
	ArlInstance *gl = Gl();

	ScePafGraphics_E7B32741(gl);

	int32_t width = texture->m_desc.m_width >> level;
	int32_t height = texture->m_desc.m_height >> level;

	gl->m_viewport.Push();
	{
		Rect viewport;

		viewport.m_x0 = 0;
		viewport.m_y0 = 0;
		viewport.m_x1 = width;
		viewport.m_y1 = height;
		ScePafGraphics_93CBC733(gl, viewport);
	}
	{
		Rect scissor;

		scissor.m_x0 = 0;
		scissor.m_y0 = 0;
		scissor.m_x1 = width;
		scissor.m_y1 = height;
		ScePafGraphics_56C831A9(gl, scissor);
	}

	Attachment &target = m_attachments[attachment];

	target.m_level = level;

	const TextureDesc &desc = texture->m_desc;
	int32_t full_width = desc.m_width;
	int32_t full_height = desc.m_height;

	target.m_width = full_width;
	target.m_height = full_height;
	target.m_texture = texture;
	target.m_buffer.reset();
	m_size_source = &target;

	SceneDataStack *scenes = gl;

	scenes->Push();
	scenes = gl;
	scenes->Set(this);
}

void FrameBufferObject::Unbind()
{
	ArlInstance *gl = Gl();
	SceneDataStack *scenes = gl;

	scenes->Pop();
	gl->m_viewport.Pop();
	ScePafGraphics_93CBC733(gl, gl->m_viewport.Top().m_viewport);
	ScePafGraphics_56C831A9(gl, gl->m_viewport.Top().m_scissor);
}

}
}
}
