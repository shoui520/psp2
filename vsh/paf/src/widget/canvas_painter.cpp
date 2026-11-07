/*
 * ui::Canvas's painter (canvas_painter.h): points added to a stroke are filled out into round
 * points no further apart than their size, drawn over a copy of the picture into the display
 * surface, and drawn into the picture itself every 20 points and when the stroke ends. The file's
 * name is ours.
 */
#include <stddef.h>
#include <stdint.h>
#include <gxm.h>
#include <kernel/sysmem/memblock.h>
#include <widget/canvas_painter.h>
#include <std/stdlib.h>
#include <std/string.h>
#include "../std/math.h"

namespace paf {
namespace ui {

/* the GXP programs, built from Cg with psp2cgc (names ours) */
extern const uint8_t g_canvas_clear_f_gxp[226];
extern const uint8_t g_canvas_clear_v_gxp[250];
extern const uint8_t g_canvas_copy_f_gxp[260];
extern const uint8_t g_canvas_copy_v_gxp[307];
extern const uint8_t g_canvas_brush_f_gxp[323];
extern const uint8_t g_canvas_brush_v_gxp[352];
extern const uint8_t g_canvas_point_f_gxp[226];
extern const uint8_t g_canvas_point_v_gxp[375];

bool CanvasPainter::Impl::s_initialized;
static void *s_host_mem;
static SceGxmContext *s_context;
static SceGxmShaderPatcher *s_patcher;
static SceUID s_vdm_ring_uid;
static SceUID s_vertex_ring_uid;
static SceUID s_fragment_ring_uid;
static SceUID s_fragment_usse_ring_uid;
static SceUID s_patcher_buffer_uid;
static SceUID s_patcher_usse_uid;

/* what the GPU may do with a buffer it renders into */
#define GPU_READ_WRITE ((SceGxmMemoryAttribFlags)(SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE))

/* GPU memory and render targets (names ours) */
void *AllocGpuMemory(SceKernelMemBlockType type, uint32_t size, uint32_t alignment,
		     SceGxmMemoryAttribFlags attribs, SceUID *uid);
__attribute__((noinline)) SceGxmRenderTarget *CreateRenderTarget(uint16_t width, uint16_t height,
								 SceGxmMultisampleMode multisample);
/* the shader patcher's host memory */
void *HostAlloc(void *user_data, uint32_t size);
void HostFree(void *user_data, void *mem);

void CanvasPainter::Impl::InitDisplayTarget()
{
	m_display_data = AllocGpuMemory(SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, WIDTH * HEIGHT * 4, 16,
					GPU_READ_WRITE, &m_display_uid);
	sceGxmColorSurfaceInit(&m_display_surface, SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR,
			       SCE_GXM_COLOR_SURFACE_LINEAR, SCE_GXM_COLOR_SURFACE_SCALE_NONE,
			       SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT, WIDTH, HEIGHT, WIDTH, m_display_data);
	sceGxmTextureInitLinear(&m_display_texture, m_display_data,
				SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR, WIDTH, HEIGHT, 0);
	sceGxmTextureSetMagFilter(&m_display_texture, SCE_GXM_TEXTURE_FILTER_LINEAR);
	sceGxmTextureSetMinFilter(&m_display_texture, SCE_GXM_TEXTURE_FILTER_LINEAR);
	m_display_depth_data = AllocGpuMemory(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
					      WIDTH * HEIGHT * 4, 16, GPU_READ_WRITE,
					      &m_display_depth_uid);
	sceGxmDepthStencilSurfaceInit(&m_display_depth, SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24,
				      SCE_GXM_DEPTH_STENCIL_SURFACE_TILED, WIDTH, m_display_depth_data,
				      NULL);
	m_display_target = CreateRenderTarget(WIDTH, HEIGHT, SCE_GXM_MULTISAMPLE_NONE);
}

void CanvasPainter::Impl::InitPictureTarget()
{
	m_picture_data = AllocGpuMemory(SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, WIDTH * HEIGHT * 4, 16,
					GPU_READ_WRITE, &m_picture_uid);
	sceGxmColorSurfaceInit(&m_picture_surface, SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR,
			       SCE_GXM_COLOR_SURFACE_LINEAR, SCE_GXM_COLOR_SURFACE_SCALE_NONE,
			       SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT, WIDTH, HEIGHT, WIDTH, m_picture_data);
	sceGxmTextureInitLinear(&m_picture_texture, m_picture_data,
				SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR, WIDTH, HEIGHT, 0);
	sceGxmTextureSetMagFilter(&m_picture_texture, SCE_GXM_TEXTURE_FILTER_LINEAR);
	sceGxmTextureSetMinFilter(&m_picture_texture, SCE_GXM_TEXTURE_FILTER_LINEAR);
	m_picture_depth_data = AllocGpuMemory(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
					      WIDTH * HEIGHT * 4, 16, GPU_READ_WRITE,
					      &m_picture_depth_uid);
	sceGxmDepthStencilSurfaceInit(&m_picture_depth, SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24,
				      SCE_GXM_DEPTH_STENCIL_SURFACE_TILED, WIDTH, m_picture_depth_data,
				      NULL);
	m_picture_target = CreateRenderTarget(WIDTH, HEIGHT, SCE_GXM_MULTISAMPLE_NONE);
}

void CanvasPainter::Impl::InitBuffers()
{
	m_stroke_vertices = (CanvasPoint *)AllocGpuMemory(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, MAX_STROKE_VERTICES * sizeof(CanvasPoint), 4,
		SCE_GXM_MEMORY_ATTRIB_READ, &m_stroke_vertices_uid);
	m_stroke_indices = (uint16_t *)AllocGpuMemory(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, MAX_STROKE_VERTICES * sizeof(uint16_t), 2,
		SCE_GXM_MEMORY_ATTRIB_READ, &m_stroke_indices_uid);
	m_positions = (CanvasClearVertex *)AllocGpuMemory(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, MAX_POINTS * sizeof(CanvasClearVertex), 4,
		SCE_GXM_MEMORY_ATTRIB_READ, &m_positions_uid);
	m_position_indices = (uint16_t *)AllocGpuMemory(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, MAX_POINTS * sizeof(uint16_t), 2,
		SCE_GXM_MEMORY_ATTRIB_READ, &m_position_indices_uid);

	uint16_t *index = m_position_indices;

	for (int32_t i = 0; i < MAX_POINTS; i++)
		*index++ = i;
}

void CanvasPainter::Impl::InitPointProgram()
{
	sceGxmShaderPatcherRegisterProgram(s_patcher, (const SceGxmProgram *)g_canvas_point_v_gxp,
					   &m_point_vertex_id);
	sceGxmShaderPatcherRegisterProgram(s_patcher, (const SceGxmProgram *)g_canvas_point_f_gxp,
					   &m_point_fragment_id);

	const SceGxmProgram *vertex = sceGxmShaderPatcherGetProgramFromId(m_point_vertex_id);
	const SceGxmProgramParameter *position =
		sceGxmProgramFindParameterByName(vertex, "aPosition");

	m_point_size = sceGxmProgramFindParameterByName(vertex, "size");
	m_point_color = sceGxmProgramFindParameterByName(
		sceGxmShaderPatcherGetProgramFromId(m_point_fragment_id), "color");

	SceGxmVertexAttribute attribute;
	SceGxmVertexStream stream;

	attribute.streamIndex = 0;
	attribute.offset = 0;
	attribute.format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	attribute.componentCount = 2;
	attribute.regIndex = sceGxmProgramParameterGetResourceIndex(position);
	stream.stride = sizeof(CanvasClearVertex);
	stream.indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
	sceGxmShaderPatcherCreateVertexProgram(s_patcher, m_point_vertex_id, &attribute, 1, &stream,
					       1, &m_point_vertex);

	SceGxmBlendInfo blend;

	blend.colorFunc = SCE_GXM_BLEND_FUNC_ADD;
	blend.alphaFunc = SCE_GXM_BLEND_FUNC_ADD;
	blend.colorSrc = SCE_GXM_BLEND_FACTOR_SRC_ALPHA;
	blend.colorDst = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend.alphaSrc = SCE_GXM_BLEND_FACTOR_SRC_ALPHA;
	blend.alphaDst = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend.colorMask = SCE_GXM_COLOR_MASK_ALL;
	sceGxmShaderPatcherCreateFragmentProgram(
		s_patcher, m_point_fragment_id, SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		SCE_GXM_MULTISAMPLE_NONE, &blend,
		sceGxmShaderPatcherGetProgramFromId(m_point_vertex_id), &m_point_fragment);
}

void CanvasPainter::Impl::InitBrushProgram()
{
	sceGxmShaderPatcherRegisterProgram(s_patcher, (const SceGxmProgram *)g_canvas_brush_v_gxp,
					   &m_brush_vertex_id);
	sceGxmShaderPatcherRegisterProgram(s_patcher, (const SceGxmProgram *)g_canvas_brush_f_gxp,
					   &m_brush_fragment_id);

	const SceGxmProgram *vertex = sceGxmShaderPatcherGetProgramFromId(m_brush_vertex_id);
	const SceGxmProgramParameter *position =
		sceGxmProgramFindParameterByName(vertex, "aPosition");
	const SceGxmProgramParameter *size = sceGxmProgramFindParameterByName(vertex, "aSize");

	m_brush_color = sceGxmProgramFindParameterByName(
		sceGxmShaderPatcherGetProgramFromId(m_brush_fragment_id), "fColor");

	SceGxmVertexAttribute attributes[2];
	SceGxmVertexStream stream;

	attributes[0].streamIndex = 0;
	attributes[0].offset = 0;
	attributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	attributes[0].componentCount = 2;
	attributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(position);
	attributes[1].streamIndex = 0;
	attributes[1].offset = offsetof(CanvasPoint, size);
	attributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	attributes[1].componentCount = 1;
	attributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(size);
	stream.stride = sizeof(CanvasPoint);
	stream.indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
	sceGxmShaderPatcherCreateVertexProgram(s_patcher, m_brush_vertex_id, attributes, 2, &stream,
					       1, &m_brush_vertex);

	SceGxmBlendInfo blend;

	blend.colorFunc = SCE_GXM_BLEND_FUNC_ADD;
	blend.alphaFunc = SCE_GXM_BLEND_FUNC_ADD;
	blend.colorSrc = SCE_GXM_BLEND_FACTOR_SRC_ALPHA;
	blend.colorDst = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend.alphaSrc = SCE_GXM_BLEND_FACTOR_ONE;
	blend.alphaDst = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend.colorMask = SCE_GXM_COLOR_MASK_ALL;
	sceGxmShaderPatcherCreateFragmentProgram(
		s_patcher, m_brush_fragment_id, SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		SCE_GXM_MULTISAMPLE_NONE, &blend,
		sceGxmShaderPatcherGetProgramFromId(m_brush_vertex_id), &m_brush_fragment);
}

void CanvasPainter::Impl::InitQuad()
{
	m_quad_vertices = (CanvasCopyVertex *)AllocGpuMemory(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, 4 * sizeof(CanvasCopyVertex), 4,
		SCE_GXM_MEMORY_ATTRIB_READ, &m_quad_vertices_uid);

	uint16_t *indices = (uint16_t *)AllocGpuMemory(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
						       6 * sizeof(uint16_t), 2,
						       SCE_GXM_MEMORY_ATTRIB_READ, &m_quad_indices_uid);
	/* the whole surface, the texture upright (half floats 0 and 1) */
	CanvasCopyVertex *vertex = m_quad_vertices;

	m_quad_indices = indices;

	vertex[0].x = -1.0f;
	vertex[0].y = -1.0f;
	vertex[0].z = 0.0f;
	vertex[0].color = 0xffffffff;
	vertex[0].u = 0x0000;
	vertex[0].v = 0x3c00;
	vertex[1].x = -1.0f;
	vertex[1].y = 1.0f;
	vertex[1].z = 0.0f;
	vertex[1].color = 0xffffffff;
	vertex[1].u = 0x0000;
	vertex[1].v = 0x0000;
	vertex[2].x = 1.0f;
	vertex[2].y = 1.0f;
	vertex[2].z = 0.0f;
	vertex[2].color = 0xffffffff;
	vertex[2].u = 0x3c00;
	vertex[2].v = 0x0000;
	vertex[3].x = 1.0f;
	vertex[3].y = -1.0f;
	vertex[3].z = 0.0f;
	vertex[3].color = 0xffffffff;
	vertex[3].u = 0x3c00;
	vertex[3].v = 0x3c00;
	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	indices[3] = 2;
	indices[4] = 3;
	indices[5] = 0;
}

void CanvasPainter::Impl::InitCopyProgram()
{
	sceGxmShaderPatcherRegisterProgram(s_patcher, (const SceGxmProgram *)g_canvas_copy_v_gxp,
					   &m_copy_vertex_id);
	sceGxmShaderPatcherRegisterProgram(s_patcher, (const SceGxmProgram *)g_canvas_copy_f_gxp,
					   &m_copy_fragment_id);

	const SceGxmProgram *vertex = sceGxmShaderPatcherGetProgramFromId(m_copy_vertex_id);
	const SceGxmProgramParameter *position =
		sceGxmProgramFindParameterByName(vertex, "aPosition");
	const SceGxmProgramParameter *color = sceGxmProgramFindParameterByName(vertex, "aColor");
	const SceGxmProgramParameter *tex_coord =
		sceGxmProgramFindParameterByName(vertex, "aTexCoord");
	SceGxmVertexAttribute attributes[3];
	SceGxmVertexStream stream;

	attributes[0].streamIndex = 0;
	attributes[0].offset = 0;
	attributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	attributes[0].componentCount = 3;
	attributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(position);
	attributes[1].streamIndex = 0;
	attributes[1].offset = offsetof(CanvasCopyVertex, color);
	attributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_U8N;
	attributes[1].componentCount = 4;
	attributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(color);
	attributes[2].streamIndex = 0;
	attributes[2].offset = offsetof(CanvasCopyVertex, u);
	attributes[2].format = SCE_GXM_ATTRIBUTE_FORMAT_F16;
	attributes[2].componentCount = 2;
	attributes[2].regIndex = sceGxmProgramParameterGetResourceIndex(tex_coord);
	stream.stride = sizeof(CanvasCopyVertex);
	stream.indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
	sceGxmShaderPatcherCreateVertexProgram(s_patcher, m_copy_vertex_id, attributes, 3, &stream, 1,
					       &m_copy_vertex);
	sceGxmShaderPatcherCreateFragmentProgram(
		s_patcher, m_copy_fragment_id, SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		SCE_GXM_MULTISAMPLE_NONE, NULL, sceGxmShaderPatcherGetProgramFromId(m_copy_vertex_id),
		&m_copy_fragment);
}

void CanvasPainter::Impl::InitClearProgram()
{
	sceGxmShaderPatcherRegisterProgram(s_patcher, (const SceGxmProgram *)g_canvas_clear_v_gxp,
					   &m_clear_vertex_id);
	sceGxmShaderPatcherRegisterProgram(s_patcher, (const SceGxmProgram *)g_canvas_clear_f_gxp,
					   &m_clear_fragment_id);

	const SceGxmProgramParameter *position = sceGxmProgramFindParameterByName(
		sceGxmShaderPatcherGetProgramFromId(m_clear_vertex_id), "aPosition");

	m_clear_color = sceGxmProgramFindParameterByName(
		sceGxmShaderPatcherGetProgramFromId(m_clear_fragment_id), "color");

	SceGxmVertexAttribute attribute;
	SceGxmVertexStream stream;

	attribute.streamIndex = 0;
	attribute.offset = 0;
	attribute.format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	attribute.componentCount = 2;
	attribute.regIndex = sceGxmProgramParameterGetResourceIndex(position);
	stream.stride = sizeof(CanvasClearVertex);
	stream.indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
	sceGxmShaderPatcherCreateVertexProgram(s_patcher, m_clear_vertex_id, &attribute, 1, &stream,
					       1, &m_clear_vertex);
	sceGxmShaderPatcherCreateFragmentProgram(
		s_patcher, m_clear_fragment_id, SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		SCE_GXM_MULTISAMPLE_NONE, NULL, sceGxmShaderPatcherGetProgramFromId(m_clear_vertex_id),
		&m_clear_fragment);
	m_clear_vertices = (CanvasClearVertex *)AllocGpuMemory(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, 3 * sizeof(CanvasClearVertex), 4,
		SCE_GXM_MEMORY_ATTRIB_READ, &m_clear_vertices_uid);
	m_clear_indices = (uint16_t *)AllocGpuMemory(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
						     3 * sizeof(uint16_t), 2,
						     SCE_GXM_MEMORY_ATTRIB_READ, &m_clear_indices_uid);

	/* one triangle over the whole surface */
	m_clear_vertices[0].x = -1.0f;
	m_clear_vertices[0].y = -1.0f;
	m_clear_vertices[1].x = 3.0f;
	m_clear_vertices[1].y = -1.0f;
	m_clear_vertices[2].x = -1.0f;
	m_clear_vertices[2].y = 3.0f;
	m_clear_indices[0] = 0;
	m_clear_indices[1] = 1;
	m_clear_indices[2] = 2;
}

CanvasPainter::CanvasPainter()
{
	m_impl = new Impl;
}

CanvasPainter::~CanvasPainter()
{
	delete m_impl;
}

void CanvasPainter::Impl::BuildStroke(CanvasRect *rect)
{
	m_stroke_count = 0;
	if (m_point_count != 0) {
		m_stroke_vertices[m_stroke_count].x = m_points[0].x;
		m_stroke_vertices[m_stroke_count].y = m_points[0].y;
		m_stroke_vertices[m_stroke_count].size = m_points[0].size;

		float left = m_points[0].x - m_points[0].size;
		float right = m_points[0].x + m_points[0].size;
		float top = m_points[0].y - m_points[0].size;
		float bottom = m_points[0].y + m_points[0].size;

		m_stroke_count++;

		const CanvasPoint *from = &m_points[0];
		const CanvasPoint *to = &m_points[1];

		for (uint32_t i = 1; i < m_point_count; i++, from++, to++) {
			float to_x = to->x;
			float to_y = to->y;
			float to_size = to->size;
			float from_x = from->x;
			float from_y = from->y;
			float from_size = from->size;

			if (to_x - to_size < left)
				left = to_x - to_size;
			if (to_x + to_size > right)
				right = to_x + to_size;
			if (to_y - to_size < top)
				top = to_y - to_size;
			if (to_y + to_size > bottom)
				bottom = to_y + to_size;

			float dy = to_y - from_y;
			float dx = to_x - from_x;
			float dsize = to_size - from_size;
			float distance = __builtin_sqrtf(dy * dy + dx * dx);

			if (distance > from_size) {
				int32_t steps = (int32_t)(distance / from_size) + 1;

				for (int32_t step = 0; step < steps; step++) {
					float x = dx * step / steps + from_x;
					float y = dy * step / steps + from_y;
					float size = dsize * step / steps + from_size;

					m_stroke_vertices[m_stroke_count].x = x;
					m_stroke_vertices[m_stroke_count].y = y;
					m_stroke_vertices[m_stroke_count].size = size;
					m_stroke_indices[m_stroke_count] = m_stroke_count;
					m_stroke_count++;
				}
				m_segments += 1.0f;
			} else {
				m_stroke_vertices[m_stroke_count].x = m_points[i].x;
				m_stroke_vertices[m_stroke_count].y = m_points[i].y;
				m_stroke_vertices[m_stroke_count].size = from_size;
				m_stroke_indices[m_stroke_count] = m_stroke_count;
				m_stroke_count++;
				m_segments += 1.0f;
			}
		}

		rect->left = (int32_t)left / 2;
		rect->top = (int32_t)top / 2;
		rect->right = (int32_t)right / 2;
		rect->bottom = (int32_t)bottom / 2;
		if (rect->right != rect->left || rect->bottom != rect->top) {
			rect->left--;
			rect->right++;
			rect->top--;
			rect->bottom++;
			if (rect->left < 0)
				rect->left = 0;
			if (rect->right >= WIDTH)
				rect->right = WIDTH - 1;
			if (rect->top < 0)
				rect->top = 0;
			if (rect->bottom >= HEIGHT)
				rect->bottom = HEIGHT - 1;
		}
	} else {
		rect->top = 0;
		rect->bottom = 0;
		rect->left = 0;
		rect->right = 0;
	}
}

void CanvasPainter::Impl::Draw()
{
	if (sceGxmBeginScene(s_context, 0, m_display_target, NULL, NULL, NULL, &m_display_surface,
			     &m_display_depth) != 0) {
		sce_paf_printf_dummy();
		return;
	}

	/* cleared to transparent black */
	sceGxmSetFrontDepthFunc(s_context, SCE_GXM_DEPTH_FUNC_ALWAYS);
	sceGxmSetFrontPolygonMode(s_context, SCE_GXM_POLYGON_MODE_TRIANGLE_FILL);
	sceGxmSetVertexProgram(s_context, m_clear_vertex);
	sceGxmSetFragmentProgram(s_context, m_clear_fragment);

	void *uniforms;

	sceGxmReserveFragmentDefaultUniformBuffer(s_context, &uniforms);

	float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	sceGxmSetUniformDataF(uniforms, m_clear_color, 0, 4, clear_color);
	sceGxmSetVertexStream(s_context, 0, m_clear_vertices);
	sceGxmDraw(s_context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, m_clear_indices,
		   3);

	/* the picture */
	sceGxmSetVertexProgram(s_context, m_copy_vertex);
	sceGxmSetFragmentProgram(s_context, m_copy_fragment);
	sceGxmSetVertexStream(s_context, 0, m_quad_vertices);
	sceGxmSetFragmentTexture(s_context, 0, &m_picture_texture);

	void *vertex_uniforms = NULL;

	sceGxmReserveVertexDefaultUniformBuffer(s_context, &vertex_uniforms);
	sceGxmSetFrontPolygonMode(s_context, SCE_GXM_POLYGON_MODE_TRIANGLE_FILL);
	sceGxmDraw(s_context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, m_quad_indices,
		   6);

	/* the stroke not yet in it */
	CanvasRect rect = { 0, 0, 0, 0 };

	BuildStroke(&rect);
	if (m_stroke_count != 0) {
		void *brush_vertex_uniforms;
		void *brush_fragment_uniforms;

		sceGxmSetVertexProgram(s_context, m_brush_vertex);
		sceGxmSetFragmentProgram(s_context, m_brush_fragment);
		sceGxmSetVertexStream(s_context, 0, m_stroke_vertices);
		sceGxmSetFragmentTexture(s_context, 0, &m_brush_texture);
		sceGxmReserveVertexDefaultUniformBuffer(s_context, &brush_vertex_uniforms);
		sceGxmReserveFragmentDefaultUniformBuffer(s_context, &brush_fragment_uniforms);
		sceGxmSetUniformDataF(brush_fragment_uniforms, m_brush_color, 0, 4, m_color);
		sceGxmSetFrontPolygonMode(s_context, SCE_GXM_POLYGON_MODE_POINT_01UV);
		sceGxmDraw(s_context, SCE_GXM_PRIMITIVE_POINTS, SCE_GXM_INDEX_FORMAT_U16,
			   m_stroke_indices, m_stroke_count);
	}
	sceGxmEndScene(s_context, NULL, NULL);
}

void CanvasPainter::GetColor(float *r, float *g, float *b, float *a)
{
	*r = m_impl->m_color[0];
	*g = m_impl->m_color[1];
	*b = m_impl->m_color[2];
	*a = m_impl->m_color[3];
}

void CanvasPainter::SetColor(float r, float g, float b, float a)
{
	m_impl->m_color[0] = r;
	m_impl->m_color[1] = g;
	m_impl->m_color[2] = b;
	m_impl->m_color[3] = a;
}

float CanvasPainter::GetSize()
{
	return m_impl->m_size;
}

void CanvasPainter::SetSize(float size)
{
	m_impl->m_size = size;
}

void CanvasPainter::SetPressureSensitive(bool sensitive)
{
	m_impl->m_pressure_sensitive = sensitive;
}

void CanvasPainter::Empty1()
{
}

void CanvasPainter::Empty2()
{
}

void CanvasPainter::Impl::Flush()
{
	if (sceGxmBeginScene(s_context, 0, m_picture_target, NULL, NULL, NULL, &m_picture_surface,
			     &m_picture_depth) != 0)
		return;

	CanvasRect rect = { 0, 0, 0, 0 };

	BuildStroke(&rect);
	if (m_stroke_count != 0) {
		void *vertex_uniforms;
		void *fragment_uniforms;

		sceGxmSetVertexProgram(s_context, m_brush_vertex);
		sceGxmSetFragmentProgram(s_context, m_brush_fragment);
		sceGxmSetVertexStream(s_context, 0, m_stroke_vertices);
		sceGxmSetFragmentTexture(s_context, 0, &m_brush_texture);
		sceGxmReserveVertexDefaultUniformBuffer(s_context, &vertex_uniforms);
		sceGxmReserveFragmentDefaultUniformBuffer(s_context, &fragment_uniforms);
		sceGxmSetUniformDataF(fragment_uniforms, m_brush_color, 0, 4, m_color);
		sceGxmSetFrontPolygonMode(s_context, SCE_GXM_POLYGON_MODE_POINT_01UV);
		sceGxmDraw(s_context, SCE_GXM_PRIMITIVE_POINTS, SCE_GXM_INDEX_FORMAT_U16,
			   m_stroke_indices, m_stroke_count);
	}
	sceGxmEndScene(s_context, NULL, NULL);
}

void CanvasPainter::Impl::AddPoint(float x, float y, float pressure)
{
	uint32_t count = m_point_count;

	if (count >= MAX_POINTS) {
		Flush();
		count = 0;
		m_point_count = 0;
	}

	float size = m_size;
	bool sensitive = m_pressure_sensitive;
	CanvasPoint &point = m_points[count];

	point.x = x;
	point.y = y;
	if (sensitive)
		size = pressure * size * 2.0f;
	point.size = size;
	m_positions[count].x = x;
	m_positions[count].y = y;
	m_point_count = count + 1;
}

void CanvasPainter::BeginStroke(float x, float y, float pressure)
{
	Impl *impl = m_impl;

	impl->m_point_count = 0;
	impl->AddPoint(x, y, pressure);
	impl->Draw();
}

void CanvasPainter::ContinueStroke(float x, float y, float pressure)
{
	Impl *impl = m_impl;

	impl->AddPoint(x, y, pressure);
	impl->Draw();
}

void CanvasPainter::EndStroke(float x, float y)
{
	Impl *impl = m_impl;

	impl->Flush();
	impl->m_point_count = 0;
	impl->Draw();
}

void CanvasPainter::GetTexture(SceGxmTexture **texture, int32_t *width, int32_t *height,
			       int32_t *pitch)
{
	*texture = &m_impl->m_display_texture;
	*width = Impl::WIDTH;
	*height = Impl::HEIGHT;
	*pitch = 4096;
}

void *AllocGpuMemory(SceKernelMemBlockType type, uint32_t size, uint32_t alignment,
			    SceGxmMemoryAttribFlags attribs, SceUID *uid)
{
	if (type == SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW)
		size = (size + 0x3ffff) & ~0x3ffff;
	else
		size = (size + 0xfff) & ~0xfff;

	SceUID block = sceKernelAllocMemBlock("common", type, size, NULL);
	void *base;

	*uid = block;
	base = NULL;
	sceKernelGetMemBlockBase(block, &base);
	sceGxmMapMemory(base, size, attribs);
	return base;
}

void CanvasPainter::Impl::InitGxm()
{
	void *vdm_ring;
	void *vertex_ring;
	void *fragment_ring;
	void *fragment_usse_ring;
	uint32_t fragment_usse_ring_offset;

	s_host_mem = sce_paf_malloc(SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE);
	s_vdm_ring_uid = sceKernelAllocMemBlock("common", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
						SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE, NULL);
	vdm_ring = NULL;
	sceKernelGetMemBlockBase(s_vdm_ring_uid, &vdm_ring);
	sceGxmMapMemory(vdm_ring, SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE, SCE_GXM_MEMORY_ATTRIB_READ);
	s_vertex_ring_uid = sceKernelAllocMemBlock("common", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
						   SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE, NULL);
	vertex_ring = NULL;
	sceKernelGetMemBlockBase(s_vertex_ring_uid, &vertex_ring);
	sceGxmMapMemory(vertex_ring, SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE,
			SCE_GXM_MEMORY_ATTRIB_READ);
	s_fragment_ring_uid = sceKernelAllocMemBlock("common",
						     SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
						     SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE, NULL);
	fragment_ring = NULL;
	sceKernelGetMemBlockBase(s_fragment_ring_uid, &fragment_ring);
	sceGxmMapMemory(fragment_ring, SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE,
			SCE_GXM_MEMORY_ATTRIB_READ);
	fragment_usse_ring = NULL;
	s_fragment_usse_ring_uid = sceKernelAllocMemBlock(
		"basic", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE, NULL);
	sceKernelGetMemBlockBase(s_fragment_usse_ring_uid, &fragment_usse_ring);
	sceGxmMapFragmentUsseMemory(fragment_usse_ring, SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE,
				    &fragment_usse_ring_offset);

	SceGxmContextParams context;

	sce_paf_memset(&context, 0, sizeof(context));
	context.hostMem = s_host_mem;
	context.vertexRingBufferMem = vertex_ring;
	context.fragmentRingBufferMem = fragment_ring;
	context.fragmentUsseRingBufferMem = fragment_usse_ring;
	context.hostMemSize = SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE;
	context.vdmRingBufferMem = vdm_ring;
	context.vdmRingBufferMemSize = SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE;
	context.vertexRingBufferMemSize = SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE;
	context.fragmentRingBufferMemSize = SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE;
	context.fragmentUsseRingBufferMemSize = SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE;
	context.fragmentUsseRingBufferOffset = fragment_usse_ring_offset;
	sceGxmCreateContext(&context, &s_context);

	void *patcher_buffer;
	void *patcher_usse;
	uint32_t vertex_usse_offset;
	uint32_t fragment_usse_offset;

	s_patcher_buffer_uid = sceKernelAllocMemBlock("common",
						      SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
						      0x10000, NULL);
	patcher_buffer = NULL;
	sceKernelGetMemBlockBase(s_patcher_buffer_uid, &patcher_buffer);
	sceGxmMapMemory(patcher_buffer, 0x10000, GPU_READ_WRITE);
	patcher_usse = NULL;
	s_patcher_usse_uid = sceKernelAllocMemBlock("basic", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
						    0x10000, NULL);
	sceKernelGetMemBlockBase(s_patcher_usse_uid, &patcher_usse);
	sceGxmMapVertexUsseMemory(patcher_usse, 0x10000, &vertex_usse_offset);
	sceGxmMapFragmentUsseMemory(patcher_usse, 0x10000, &fragment_usse_offset);

	SceGxmShaderPatcherParams patcher;

	sce_paf_memset(&patcher, 0, sizeof(patcher));
	patcher.bufferAllocCallback = NULL;
	patcher.bufferFreeCallback = NULL;
	patcher.bufferMem = patcher_buffer;
	patcher.vertexUsseAllocCallback = NULL;
	patcher.vertexUsseFreeCallback = NULL;
	patcher.vertexUsseMem = patcher_usse;
	patcher.fragmentUsseAllocCallback = NULL;
	patcher.fragmentUsseFreeCallback = NULL;
	patcher.fragmentUsseMem = patcher_usse;
	patcher.userData = NULL;
	patcher.hostAllocCallback = HostAlloc;
	patcher.hostFreeCallback = HostFree;
	patcher.bufferMemSize = 0x10000;
	patcher.vertexUsseMemSize = 0x10000;
	patcher.fragmentUsseMemSize = 0x10000;
	patcher.vertexUsseOffset = vertex_usse_offset;
	patcher.fragmentUsseOffset = fragment_usse_offset;
	sceGxmShaderPatcherCreate(&patcher, &s_patcher);
}

SceGxmRenderTarget *CreateRenderTarget(uint16_t width, uint16_t height,
					      SceGxmMultisampleMode multisample)
{
	SceGxmRenderTargetParams params;
	SceGxmRenderTarget *target;

	sce_paf_memset(&params, 0, sizeof(params));
	params.width = width;
	params.height = height;
	params.flags = 0;
	params.scenesPerFrame = 1;
	params.multisampleMode = multisample;
	params.multisampleLocations = 0;
	params.driverMemBlock = -1;
	sceGxmCreateRenderTarget(&params, &target);
	return target;
}

void *HostAlloc(void *user_data, uint32_t size)
{
	return sce_paf_malloc(size);
}

void HostFree(void *user_data, void *mem)
{
	sce_paf_free(mem);
}

}
}
