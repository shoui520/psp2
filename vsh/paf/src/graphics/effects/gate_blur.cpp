/*
 * paf::graph::GateBlur: the gate an application opens through and closes into. The class and
 * the method names Initialize, Finalize, Draw, Draw_ and GetMode are Sony's (the export NIDs
 * hash as them); the file's name and every other name are ours.
 */
#include <stdint.h>
#include <gxm.h>
#include <vectormath.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/color.h>
#include <graphics/effects/gate_blur.h>
#include <timer/timer.h>

using sce::Vectormath::Simd::Aos::Matrix4;
using sce::Vectormath::Simd::Aos::Vector3;

/* the gate's size on the screen, and in its texture coordinates, before it grows */
#define GATE_WIDTH	0.29166666f
#define GATE_HEIGHT	0.2904412f
#define GATE_UV_WIDTH	0.14583333f
#define GATE_UV_HEIGHT	0.1452206f

namespace paf {
namespace graph {

/* the screen: a fan from the centre */
static const GateBlurVertex s_quad_vertices[6] = {
	{ { 0.0f, 0.0f, 0.0f }, { 0.5f, 0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
	{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
	{ { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
	{ { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
	{ { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
	{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
};

/* the same fan, nearly transparent; only its layout is used, by the vertex program */
static const GateBlurVertex s_faded_vertices[6] = {
	{ { 0.0f, 0.0f, 0.0f }, { 0.5f, 0.5f }, { 1.0f, 1.0f, 1.0f, 0.1f } },
	{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 0.2f } },
	{ { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 0.2f } },
	{ { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 0.2f } },
	{ { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 0.2f } },
	{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 0.2f } },
};

/* the gate: its centre, the gate's corners twice and the screen's corners, where it fades out;
   Draw_ puts the centre and the gate's corners where the gate is */
static const GateBlurVertex s_gate_vertices[13] = {
	{ { 0.0f, 0.0f, 0.0f }, { 0.5f, 0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
	{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
	{ { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
	{ { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
	{ { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
	{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 0.2f } },
	{ { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 0.2f } },
	{ { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 0.2f } },
	{ { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 0.2f } },
	{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 0.0f } },
	{ { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 0.0f } },
	{ { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 0.0f } },
	{ { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 0.0f } },
};

/* the gate's triangles: the fan of the centre, then the band from the gate to the screen */
static const uint16_t s_gate_indices[36] = {
	0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1,
	5, 6, 9, 6, 9, 10, 6, 7, 10, 7, 10, 11, 7, 8, 11, 8, 11, 12, 8, 5, 12, 5, 12, 9
};

GateBlurPrograms *s_gate_blur_programs;
GateBlur *GateBlur::s_instance;
arl::VertexArray *GateBlur::s_quad;
arl::VertexArray *GateBlur::s_faded;
arl::VertexArray *GateBlur::s_gate;
arl::IndexArray *GateBlur::s_gate_index;
GateBlurPrograms *GateBlur::s_programs;

GateBlur::GateBlur() : m_mode(Mode_None), m_time(0.0f), m_x(0.0f), m_y(0.0f), m_finish(NULL)
{
	s_instance = this;

	arl::ArlInstance *gl = arl::Gl();

	s_faded = new arl::VertexArray(
		gl->m_default_allocator,
		ScePafGraphics_4D1E289A(gl, arl::VertexArraySpec::ATTRIB0_F3_ATTRIB3_F2_ATTRIB2_F4,
					SCE_GXM_INDEX_SOURCE_INDEX_16BIT),
		s_faded_vertices, 6);
	s_gate = new arl::VertexArray(
		gl->m_default_allocator,
		ScePafGraphics_4D1E289A(gl, arl::VertexArraySpec::ATTRIB0_F3_ATTRIB3_F2_ATTRIB2_F4,
					SCE_GXM_INDEX_SOURCE_INDEX_16BIT),
		s_gate_vertices, 13);
	s_gate_index = new arl::IndexArray(gl->m_default_allocator, s_gate_indices, 36);
	s_quad = new arl::VertexArray(
		gl->m_default_allocator,
		ScePafGraphics_4D1E289A(gl, arl::VertexArraySpec::ATTRIB0_F3_ATTRIB3_F2_ATTRIB2_F4,
					SCE_GXM_INDEX_SOURCE_INDEX_16BIT),
		s_quad_vertices, 6);
	GateBlurPrograms *programs = new GateBlurPrograms;

	s_programs = programs;

	common::SharedPtr<arl::VertexArraySpec> spec = s_faded->m_spec;
	arl::ProgramState *state = gl;

	programs->m_vert.m_object.init(*gl, state->m_patcher, spec,
				       common::SharedPtr<arl::VertexArraySpec>(),
				       common::SharedPtr<arl::VertexArraySpec>(),
				       common::SharedPtr<arl::VertexArraySpec>());
	state = gl;
	programs->m_frag.m_object.init(*gl, state->m_patcher);
	s_gate_blur_programs = s_programs;
}

void GateBlur::Initialize()
{
	new GateBlur;
}

GateBlur::~GateBlur()
{
	delete s_programs;
	delete s_gate_index;
	delete s_faded;
	delete s_gate;
	delete s_quad;
	s_programs = NULL;
	s_gate_blur_programs = NULL;
	s_instance = NULL;
}

void GateBlur::Finalize()
{
	if (s_instance != NULL)
		delete s_instance;
}

/* a gate corner: where it is on the screen and in the screen's texture */
static inline __attribute__((always_inline)) void SetCorner(GateBlurVertex &vertex, float x,
							    float y, float u, float v)
{
	vertex.m_position[0] = x;
	vertex.m_position[1] = y;
	vertex.m_uv[0] = u;
	vertex.m_uv[1] = v;
}

/* the vertex array drawn with the programs begun */
static inline __attribute__((always_inline)) void DrawMesh(arl::ArlInstance *gl,
							   arl::VertexArray *mesh, uint32_t count)
{
	arl::ShaderState *state = gl;

	state->m_dirty_vertex |= mesh->m_spec->m_kinds;
	state = gl;
	state->m_dirty_fragment |= 1;
	sceGxmSetVertexStream(gl->m_context, 0, mesh->m_buffer->m_memory->m_addr);
	arl::VertexArray::DrawArrays(*gl, arl::TRIANGLE_FAN, 0, count);
	state = gl;
	state->m_dirty_vertex = 0;
	state = gl;
	state->m_dirty_fragment = 0;
}

/* vectormath's scale and translation matrices, which Sony's Draw_ calls out of line (SNC stopped
   inlining into it; names ours) */
static __attribute__((noinline)) Matrix4 ScaleMatrix(const Vector3 &scale)
{
	return Matrix4::scale(scale);
}

static __attribute__((noinline)) Matrix4 TranslationMatrix(const Vector3 &translation)
{
	return Matrix4::translation(translation);
}

void GateBlur::Draw_()
{
	if (m_mode == Mode_None)
		return;

	float duration = 0.4f;
	float level = 1.0f;

	if (m_mode != Mode_Close && m_slow)
		duration = 0.6f;
	if (m_mode != Mode_Hold)
		m_time += Timer::s_tick_unit * Timer::s_tick_interval;

	arl::ArlInstance *gl = arl::Gl();
	bool in_scene = gl->m_in_scene;

	if (!in_scene)
		ScePafGraphics_199BD5F2(gl, 0);
	gl->PushMarker("posteffects::draw_gate_blur");
	gl->CullMode(arl::CULL_NONE);
	gl->Disable(arl::BLEND);
	gl->Disable(arl::DEPTH_TEST);
	gl->Disable(arl::DEBUG_SURFACE_DRAW);
	gl->DepthMask(false, arl::FRONT_AND_BACK);
	gl->ColorMask(true, true, true, true);

	Surface *screen = intrusive_ptr<Surface>(gl->m_next_scene.m_surface).get();
	int32_t page;

	screen->GetPageIndex(FrameBufferType_Front, &page);

	void *pixels = screen->Lock(page);

	screen->Unlock();

	int32_t width = screen->GetWidth();
	int32_t height = screen->GetHeight();
	SceGxmTexture point;
	SceGxmTexture linear;

	sceGxmTextureInitLinear(&point, pixels, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR, width, height,
				0);
	sceGxmTextureSetMagFilter(&point, SCE_GXM_TEXTURE_FILTER_POINT);
	sceGxmTextureSetMinFilter(&point, SCE_GXM_TEXTURE_FILTER_POINT);
	sceGxmTextureInitLinear(&linear, pixels, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR, width, height,
				0);
	sceGxmTextureSetMagFilter(&linear, SCE_GXM_TEXTURE_FILTER_LINEAR);
	sceGxmTextureSetMinFilter(&linear, SCE_GXM_TEXTURE_FILTER_LINEAR);

	float t = m_time / duration;

	if (t > 1.0f)
		t = 1.0f;

	float rest = 1.0f - t;

	if (m_mode == Mode_Open || m_mode == Mode_Hold) {
		if (m_slow)
			level = Timer::DoFuncQuintOut(1.0f - t);

		math::v4 color(level, level, level, 1.0f);

		gl->Disable(arl::BLEND);
		sceGxmSetFragmentTexture(gl->m_context, 0, &point);
		s_programs->m_frag.m_interface.m_color.Set(color);

		math::matrix identity = math::matrix::Identity();

		s_programs->m_vert.m_interface.m_mat.Set(identity);
		s_programs->m_vert.m_object.begin();
		s_programs->m_frag.m_object.begin();
		DrawMesh(gl, s_quad, 6);
		s_programs->m_frag.m_object.end();
		s_programs->m_vert.m_object.end();
		if (m_mode != Mode_Open)
			goto done;

		gl->Enable(arl::BLEND);
		gl->BlendEquation(arl::FUNC_ADD, arl::FUNC_ADD);
		gl->BlendFunc(arl::SRC_ALPHA, arl::ONE_MINUS_SRC_ALPHA, arl::ONE,
			      arl::ONE_MINUS_SRC_ALPHA);
		if (m_surface.get() != NULL) {
			SceGxmTexture texture = *m_surface->GetRawTexture();

			sceGxmTextureSetMagFilter(&texture, SCE_GXM_TEXTURE_FILTER_LINEAR);
			sceGxmTextureSetMinFilter(&texture, SCE_GXM_TEXTURE_FILTER_LINEAR);
			sceGxmSetFragmentTexture(gl->m_context, 0, &texture);
		} else {
			sceGxmSetFragmentTexture(gl->m_context, 0, &linear);
		}

		GateBlurVertex *vertices = (GateBlurVertex *)s_gate->m_buffer->m_memory->m_addr;
		GateBlurVertex &center = vertices[0];
		float x = m_x;

		center.m_position[0] = x;

		float y = m_y;

		center.m_position[1] = y;

		float u = (m_x + 1.0f) * 0.5f;

		center.m_uv[0] = u;

		float v = 1.0f - (m_y + 1.0f) * 0.5f;

		/* the inner corners, where the copies start */
		vertices[1].m_position[0] = x - GATE_WIDTH;
		vertices[2].m_position[0] = x + GATE_WIDTH;
		vertices[3].m_position[0] = x + GATE_WIDTH;
		vertices[1].m_position[1] = y - GATE_HEIGHT;
		vertices[2].m_position[1] = y - GATE_HEIGHT;
		vertices[3].m_position[1] = y + GATE_HEIGHT;
		vertices[4].m_position[0] = x - GATE_WIDTH;
		vertices[4].m_position[1] = y + GATE_HEIGHT;
		center.m_uv[1] = v;

		float alpha = m_slow ? 0.2f : 1.0f;

		for (int32_t i = 0; i < 5; i++)
			vertices[i].m_color[3] = alpha;
		vertices[1].m_uv[0] = u - GATE_UV_WIDTH;
		vertices[1].m_uv[1] = v + GATE_UV_HEIGHT;
		vertices[2].m_uv[0] = u + GATE_UV_WIDTH;
		vertices[2].m_uv[1] = v + GATE_UV_HEIGHT;
		vertices[3].m_uv[0] = u + GATE_UV_WIDTH;
		vertices[3].m_uv[1] = v - GATE_UV_HEIGHT;
		vertices[4].m_uv[0] = u - GATE_UV_WIDTH;
		vertices[4].m_uv[1] = v - GATE_UV_HEIGHT;
		SetCorner(vertices[5], x - GATE_WIDTH, y - GATE_HEIGHT, u - GATE_UV_WIDTH,
			  v + GATE_UV_HEIGHT);
		SetCorner(vertices[6], x + GATE_WIDTH, y - GATE_HEIGHT, u + GATE_UV_WIDTH,
			  v + GATE_UV_HEIGHT);
		SetCorner(vertices[7], x + GATE_WIDTH, y + GATE_HEIGHT, u + GATE_UV_WIDTH,
			  v - GATE_UV_HEIGHT);
		SetCorner(vertices[8], x - GATE_WIDTH, y + GATE_HEIGHT, u - GATE_UV_WIDTH,
			  v - GATE_UV_HEIGHT);

		/* three copies of the gate, each a frame behind the one before, growing out of it */
		for (int32_t i = 2; i >= 0; i--) {
			float p = (m_time - Timer::s_tick_unit * Timer::s_tick_interval * (float)i) /
				  duration;

			if (p > 1.0f)
				p = 1.0f;

			/* the slow opening's copies grow further */
			float spread = m_slow ? 2.4f : 1.5f;
			float q = 1.0f - p;
			float scale = Timer::DoFuncCubicIn(1.0f - q) * spread + 1.0f;
			float move = Timer::DoFuncSineInOut(q);
			math::matrix grow =
				math::matrix(ScaleMatrix(Vector3(scale, scale, 1.0f))) *
				math::matrix(TranslationMatrix(Vector3(-m_x, -m_y, 0.0f)));

			s_programs->m_vert.m_interface.m_mat.Set(
				math::matrix(TranslationMatrix(Vector3(m_x * move, m_y * move, 0.0f))) *
				grow);
			s_programs->m_frag.m_interface.m_color.Set(color);
			s_programs->m_vert.m_object.begin();
			s_programs->m_frag.m_object.begin();

			arl::VertexArray *gate = s_gate;
			arl::ShaderState *state = gl;

			state->m_dirty_vertex |= gate->m_spec->m_kinds;
			state = gl;
			state->m_dirty_fragment |= 1;
			sceGxmSetVertexStream(gl->m_context, 0, gate->m_buffer->m_memory->m_addr);
			s_gate_index->draw(*gl, arl::TRIANGLES, 36);
			state = gl;
			state->m_dirty_vertex = 0;
			state = gl;
			state->m_dirty_fragment = 0;
			s_programs->m_frag.m_object.end();
			s_programs->m_vert.m_object.end();
		}

		if (m_cover == NULL || m_cover->get() == NULL)
			goto done;

		/* the cover grows from the gate to the screen */
		float size = Timer::DoFuncCubicIn(1.0f - rest);
		float move = Timer::DoFuncSineInOut(rest);
		math::matrix scale(ScaleMatrix(Vector3(size * (1.0f - GATE_WIDTH) + GATE_WIDTH,
							  size * (1.0f - GATE_HEIGHT) + GATE_HEIGHT,
							  1.0f)));

		sceGxmSetFragmentTexture(gl->m_context, 0, (*m_cover)->GetRawTexture());
		s_programs->m_vert.m_interface.m_mat.Set(
			math::matrix(identity.x(), identity.y(), identity.z(),
				     math::v4(m_x * move, m_y * move, 0.0f, 1.0f)) *
			scale);
		s_programs->m_frag.m_interface.m_color.Set(math::v4(1.0f, 1.0f, 1.0f, 1.0f - level));
		s_programs->m_vert.m_object.begin();
		s_programs->m_frag.m_object.begin();
		DrawMesh(gl, s_quad, 6);
		s_programs->m_frag.m_object.end();
		s_programs->m_vert.m_object.end();
	} else if (m_mode == Mode_Close) {
		gl->Disable(arl::BLEND);
		if (m_surface.get() == NULL)
			goto done;

		SceGxmTexture texture = *m_surface->GetRawTexture();

		sceGxmTextureSetMagFilter(&texture, SCE_GXM_TEXTURE_FILTER_LINEAR);
		sceGxmTextureSetMinFilter(&texture, SCE_GXM_TEXTURE_FILTER_LINEAR);
		sceGxmSetFragmentTexture(gl->m_context, 0, &texture);

		/* the surface shrinks to the gate's size and moves into the point */
		float size = Timer::DoFuncSineIn(1.0f - rest);
		float move = Timer::DoFuncBackIn(1.0f - rest);
		float move_x = move * m_x;
		float move_y = move * m_y;
		float fade = 1.0f;

		if (m_fade)
			fade = Timer::DoFuncCubicIn(rest);
		s_programs->m_frag.m_interface.m_color.Set(math::v4(fade, fade, fade, 1.0f));
		s_programs->m_vert.m_interface.m_mat.Set(
			math::matrix(math::v4(size * -(1.0f - GATE_WIDTH) + 1.0f, 0.0f, 0.0f, 0.0f),
				     math::v4(0.0f, size * -(1.0f - GATE_HEIGHT) + 1.0f, 0.0f, 0.0f),
				     math::v4(0.0f, 0.0f, 1.0f, 0.0f),
				     math::v4(move_x, move_y, 0.0f, 1.0f)));
		s_programs->m_vert.m_object.begin();
		s_programs->m_frag.m_object.begin();
		DrawMesh(gl, s_quad, 6);
		s_programs->m_frag.m_object.end();
		s_programs->m_vert.m_object.end();
	}

done:
	gl->PopMarker();
	if (!in_scene)
		ScePafGraphics_29F5AA8B(gl, NULL, false);
	if (duration < m_time) {
		FinishCallback finish = m_finish;

		m_mode = Mode_None;
		m_time = 0.0f;
		if (finish != NULL) {
			void *arg = m_finish_arg;

			m_finish = NULL;
			m_finish_arg = NULL;
			finish(arg);
		}
	}
}

void GateBlur::Draw()
{
	if (s_instance != NULL)
		s_instance->Draw_();
}

}
}

using namespace paf;
using namespace paf::graph;

void ScePafGraphics_16BDBC6E(bool slow, float x, float y, const intrusive_ptr<Surface> &surface,
			     intrusive_ptr<Surface> *cover, GateBlur::FinishCallback finish, void *arg)
{
	GateBlur::CallFinish();

	GateBlur *gate = GateBlur::s_instance;

	gate->m_mode = GateBlur::Mode_Open;
	gate->m_x = x;
	gate->m_y = y;
	gate->m_time = 0.0f;
	gate->m_surface = surface;
	GateBlur::s_instance->m_cover = cover;
	gate = GateBlur::s_instance;
	gate->m_finish = finish;
	gate->m_slow = slow;
	gate->m_fade = false;
	GateBlur::s_instance->m_finish_arg = arg;
}

void ScePafGraphics_6BBFE4F7(bool fade, float x, float y, const intrusive_ptr<Surface> &surface,
			     GateBlur::FinishCallback finish, void *arg)
{
	GateBlur::CallFinish();

	GateBlur *gate = GateBlur::s_instance;

	gate->m_mode = GateBlur::Mode_Close;
	gate->m_x = x;
	gate->m_y = y;
	gate->m_time = 0.0f;
	gate->m_surface = surface;
	GateBlur::s_instance->m_cover = NULL;
	gate = GateBlur::s_instance;
	gate->m_finish = finish;
	gate->m_slow = false;
	gate->m_fade = fade;
	GateBlur::s_instance->m_finish_arg = arg;
}

void ScePafGraphics_04F03872(void)
{
	if (GateBlur::s_instance->m_mode != GateBlur::Mode_None) {
		GateBlur::CallFinish();

		GateBlur *gate = GateBlur::s_instance;

		gate->m_finish = NULL;
		gate->m_mode = GateBlur::Mode_None;
		gate->m_time = 0.0f;
		GateBlur::s_instance->m_finish_arg = NULL;
	}
}

void ScePafGraphics_12BDEBFF(void)
{
	GateBlur::CallFinish();

	GateBlur *gate = GateBlur::s_instance;

	gate->m_finish = NULL;
	gate->m_mode = GateBlur::Mode_Hold;
	gate->m_time = 0.0f;
	GateBlur::s_instance->m_finish_arg = NULL;
}

int32_t GateBlur::GetMode()
{
	return s_instance->m_mode;
}
