/*
 * paf::graph::ScreenCopy (DEBUGGER 0x810d7db0..0x810d8a7c): surfaces drawn over with the screen,
 * the offscreen display surface or a surface of their own while they are on. The class and method
 * names are Sony's (the export NIDs hash as them); the file's and the other names are ours. The
 * headers' globals (the tile size, the colours) are constructed in the order gate_blur.cpp's are.
 */
#include <stdint.h>
#include <gxm.h>
#include <vectormath.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/color.h>
#include <graphics/effects/gate_blur.h>
#include <graphics/effects/screen_copy.h>
#include <graphics/surface/surface_display.h>

namespace paf {

/* Sony's DeleteObj has the unlinking inline. */
template<>
inline __attribute__((always_inline)) list<graph::ScreenCopyObj *>::iterator
list<graph::ScreenCopyObj *>::erase(iterator where)
{
	_Node *node = where.node;
	_Node *next = node->next;

	node->next->prev = node->prev;
	node->prev->next = node->next;
	destroy(&node->value);
	operator delete(node);
	--count;
	return iterator(next);
}

namespace graph {

ScreenCopy *ScreenCopy::s_instance;
arl::VertexArray *ScreenCopy::s_quad;

/* a strip over the whole surface, white */
static const GateBlurVertex s_copy_vertices[4] = {
	{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
	{ { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
	{ { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
	{ { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } }
};

ScreenCopy::ScreenCopy()
{
	s_instance = this;

	arl::ArlInstance *gl = arl::Gl();

	s_quad = new arl::VertexArray(
		gl->m_default_allocator,
		ScePafGraphics_4D1E289A(gl, arl::VertexArraySpec::ATTRIB0_F3_ATTRIB3_F2_ATTRIB2_F4,
					SCE_GXM_INDEX_SOURCE_INDEX_16BIT),
		s_copy_vertices, 4);
}

ScreenCopy::~ScreenCopy()
{
	delete s_quad;
	s_instance = NULL;
}

/* the screen's page as a texture, linear filtered */
static inline __attribute__((always_inline)) bool InitScreenTexture(SceGxmTexture *texture,
								    const void *pixels)
{
	if (sceGxmTextureInitLinear(texture, pixels, SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1BGR, 960, 544,
				    0) != SCE_OK)
		return false;
	sceGxmTextureSetMagFilter(texture, SCE_GXM_TEXTURE_FILTER_LINEAR);
	sceGxmTextureSetMinFilter(texture, SCE_GXM_TEXTURE_FILTER_LINEAR);
	return true;
}

/* the quad drawn with the programs begun */
static inline __attribute__((always_inline)) void DrawQuad(arl::ArlInstance *gl)
{
	arl::VertexArray *quad = ScreenCopy::s_quad;
	arl::ShaderState *state = gl;

	state->m_dirty_vertex |= quad->m_spec->m_kinds;
	state = gl;
	state->m_dirty_fragment |= 1;
	sceGxmSetVertexStream(gl->m_context, 0, quad->m_buffer->m_memory->m_addr);
	arl::VertexArray::DrawArrays(*gl, arl::TRIANGLE_STRIP, 0, 4);
	state = gl;
	state->m_dirty_vertex = 0;
	state = gl;
	state->m_dirty_fragment = 0;
}

void ScreenCopy::Draw_()
{
	if (m_objs.size() == 0)
		return;

	arl::ArlInstance *gl = arl::Gl();
	bool page0_ready = false;
	GateBlurPrograms *programs = s_gate_blur_programs;
	bool offscreen_ready = false;
	SceGxmTexture page0;
	SceGxmTexture front;
	SceGxmTexture offscreen;
	SceGxmTexture source;

	bool has_offscreen = intrusive_ptr<Surface>(DisplaySurface::s_offscreen).get() != NULL;

	if (has_offscreen) {
		intrusive_ptr<Surface> surface(DisplaySurface::s_offscreen);

		offscreen = *surface->GetRawTexture();
		offscreen_ready = true;
	}

	Surface *screen = intrusive_ptr<Surface>(gl->m_next_scene.m_surface).get();
	void *pixels = screen->Lock(0);

	screen->Unlock();
	if (pixels != NULL && InitScreenTexture(&page0, pixels))
		page0_ready = true;

	screen = intrusive_ptr<Surface>(gl->m_next_scene.m_surface).get();

	bool front_ready = false;
	int32_t page = 0;

	if (screen->GetPageIndex(FrameBufferType_Front, &page) == SCE_OK) {
		pixels = screen->Lock(page);
		screen->Unlock();
		if (pixels != NULL && InitScreenTexture(&front, pixels))
			front_ready = true;
	}

	for (list<ScreenCopyObj *>::iterator it = m_objs.begin(); it != m_objs.end();
	     ++it) {
		ScreenCopyObj *obj = *it;

		if (obj->m_state == SCREEN_COPY_OFF)
			continue;
		if (obj->m_state == SCREEN_COPY_NEXT) {
			obj->m_state = SCREEN_COPY_ONCE;
			continue;
		}

		Surface *surface = obj->m_target.get();

		surface->BeginScene();
		gl->CullMode(arl::CULL_NONE);
		gl->Disable(arl::DEPTH_TEST);
		gl->DepthMask(false, arl::FRONT_AND_BACK);
		gl->ColorMask(true, true, true, true);
		gl->Disable(arl::BLEND);

		if (obj->m_source_kind == SCREEN_COPY_PAGE0) {
			if (!page0_ready ||
			    sceGxmSetFragmentTexture(gl->m_context, 0, &page0) != SCE_OK)
				goto done;
		} else if (obj->m_source_kind == SCREEN_COPY_FRONT) {
			if (!front_ready ||
			    sceGxmSetFragmentTexture(gl->m_context, 0, &front) != SCE_OK)
				goto done;
		} else if (obj->m_source_kind == SCREEN_COPY_OFFSCREEN) {
			if (!offscreen_ready ||
			    sceGxmSetFragmentTexture(gl->m_context, 0, &offscreen) != SCE_OK)
				goto done;
		} else if (obj->m_source_kind == SCREEN_COPY_SURFACE) {
			if (obj->m_source.get() == NULL)
				goto done;

			void *data = obj->m_source->Lock(0);

			obj->m_source->Unlock();
			if (sceGxmTextureInitLinear(&source, data, SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1BGR,
						    obj->m_source->GetWidth(),
						    obj->m_source->GetHeight(), 0) != SCE_OK)
				goto done;
			sceGxmTextureSetMagFilter(&source, SCE_GXM_TEXTURE_FILTER_LINEAR);
			sceGxmTextureSetMinFilter(&source, SCE_GXM_TEXTURE_FILTER_LINEAR);
			sceGxmSetFragmentTexture(gl->m_context, 0, &source);
		} else {
			goto done;
		}
		programs->m_vert.m_interface.m_mat.Set(math::matrix::Identity());
		programs->m_frag.m_interface.m_color.Set(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
		programs->m_vert.m_object.begin();
		programs->m_frag.m_object.begin();
		DrawQuad(gl);
		programs->m_frag.m_object.end();
		programs->m_vert.m_object.end();
done:
		surface->EndScene();
		if (obj->m_state == SCREEN_COPY_ONCE)
			obj->m_state = SCREEN_COPY_OFF;
	}
}

void ScreenCopy::Initialize()
{
	new ScreenCopy;
}

void ScreenCopy::Finalize()
{
	if (s_instance != NULL)
		delete s_instance;
}

void ScreenCopy::Draw()
{
	if (s_instance != NULL)
		s_instance->Draw_();
}

ScreenCopyObj *ScreenCopy::CreateObj(intrusive_ptr<Surface> &target, int32_t source)
{
	if (s_instance == NULL)
		return NULL;

	ScreenCopyObj *obj = new ScreenCopyObj(target, source);

	s_instance->m_objs.push_back(obj);
	return obj;
}

void ScreenCopy::DeleteObj(ScreenCopyObj *obj)
{
	ScreenCopy *instance = s_instance;

	if (instance == NULL)
		return;

	for (list<ScreenCopyObj *>::iterator it = instance->m_objs.begin();
	     it != instance->m_objs.end(); ++it) {
		if (*it == obj) {
			instance->m_objs.erase(it);
			delete obj;
			return;
		}
	}
}

}
}
