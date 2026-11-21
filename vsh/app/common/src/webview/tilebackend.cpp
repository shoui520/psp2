/*
 * The tile backend's renderer: the command buffer the page's process queues its drawing in, and
 * the GXM programs that draw it into the view's render target (the tiles as textured quads, and
 * filled or outlined rectangles), clipped to a rectangle. Sony's file name is not known (the
 * module has no debug records); the names are ours.
 */

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <kernel/sysmem/memblock.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <sysmem/memblock.h>
#include <sysmem/sysmem.h>
#include <gxm.h>
#include <vectormath.h>
#include "web_view.h"
#include "tilebackend.h"

using sce::Vectormath::Simd::Aos::Matrix4;
using sce::Vectormath::Simd::Aos::Vector4;

/* the GPU programs (cg/programs.S) */
extern SceGxmProgram g_tilebackend_color_f_gxp;
extern SceGxmProgram g_tilebackend_texture_f_gxp;
extern SceGxmProgram g_tilebackend_v_gxp;

/* a memory block, the function that frees it and where it is */
struct TilebackendMemBlock {
	TilebackendMemBlock(SceUID uid, int (*free)(SceUID uid)) : m_uid(uid), m_free(free),
		m_base(NULL)
	{
		sceKernelGetMemBlockBase(m_uid, &m_base);
	}

	~TilebackendMemBlock()
	{
		m_free(m_uid);
	}

	SceUID m_uid;
	int (*m_free)(SceUID uid);
	void *m_base;
};

/* a kernel object and the function that deletes it */
struct TilebackendKernelObject {
	TilebackendKernelObject(SceUID uid, SceInt32 (*destroy)(SceUID uid)) : m_uid(uid),
		m_delete(destroy)
	{
	}

	~TilebackendKernelObject()
	{
		m_delete(m_uid);
	}

	SceUID m_uid;
	SceInt32 (*m_delete)(SceUID uid);
};

/* the commands copied from the buffer the page's process writes ("<server name>B"), with its
   event ("...E": the length written, in the user data) and its mutex ("...M") */
struct TilebackendCommandBuffer {
	SceUInt8 m_commands[TILEBACKEND_COMMAND_BUFFER_SIZE];
	TilebackendMemBlock *m_shared;
	TilebackendKernelObject *m_event;
	TilebackendKernelObject *m_mutex;
};

/* a quad's corners, its texture coordinates and the triangle strip's indices */
struct TilebackendVertex {
	float m_x;
	float m_y;
	float m_u;
	float m_v;
};

struct TilebackendShaderData {
	TilebackendVertex m_vertices[4];
	SceUInt16 m_indices[4];
};

/* the rectangle drawn in */
struct TilebackendRect {
	TilebackendRect() : m_x(0.0f), m_y(0.0f), m_width(0.0f), m_height(0.0f)
	{
	}

	float m_x;
	float m_y;
	float m_width;
	float m_height;
};

namespace paf {
namespace ui {

/* the GXM render target over the memory the page is drawn into */
struct WebViewRenderTargetData {
	WebViewRenderTargetData() : m_memblock(-1), m_target(NULL)
	{
	}

	SceUID m_memblock;
	SceGxmRenderTarget *m_target;
};

struct WebViewRenderTarget {
	WebViewRenderTarget() : m_width(0), m_height(0)
	{
	}

	SceInt32 m_width;
	SceInt32 m_height;
	SceInt32 m_stride;
	SceUInt32 m_format;
	void *m_base;
	SceUInt32 m_reserved;
	WebViewRenderTargetData *m_data;
};

} /* namespace ui */
} /* namespace paf */

using paf::ui::WebViewRenderTarget;
using paf::ui::WebViewRenderTargetData;

/* the shader patcher's memory */
static SceUID s_patcherBufferUid = -1;
static SceUID s_patcherVertexUsseUid = -1;
static SceUID s_patcherFragmentUsseUid = -1;

static TilebackendCommandBuffer *s_commandBuffer;
static SceGxmContext *s_context;
static bool s_inScene;
static SceGxmShaderPatcher *s_patcher;
static SceGxmShaderPatcherId s_vertexProgramId;
static SceGxmShaderPatcherId s_textureProgramId;
static SceGxmShaderPatcherId s_colorProgramId;
static SceGxmVertexProgram *s_vertexProgram;
static SceGxmFragmentProgram *s_textureProgram;
static SceGxmFragmentProgram *s_colorProgram;
static const SceGxmProgramParameter *s_wvpParam;
static const SceGxmProgramParameter *s_colorParam;
static const SceGxmProgramParameter *s_opacityParam;
static const SceGxmProgramParameter *s_textureAabbParam;
static const SceGxmProgramParameter *s_colorAabbParam;
static TilebackendShaderData *s_shaderData;
static SceUID s_shaderDataUid;
/* the render target's pixels to the clip space */
static Matrix4 s_projection;
static TilebackendRect s_clip;

/* the drawing ended, when there is a scene */
static inline void EndScene()
{
	if (s_inScene)
		sceGxmEndScene(s_context, NULL, NULL);
	s_inScene = false;
}

static inline void ResetClip()
{
	s_clip.m_x = 0.0f;
	s_clip.m_y = 0.0f;
	s_clip.m_width = 4096.0f;
	s_clip.m_height = 4096.0f;
}

/* the clip rectangle as the programs take it */
#define CLIP_AABB	{ s_clip.m_x, s_clip.m_y, s_clip.m_x + s_clip.m_width,		\
			  s_clip.m_y + s_clip.m_height }

/* the rectangle's outline in the color */
int DrawTilebackendOutline(const Matrix4 *world, const float *color)
{
	void *vertexUniforms;

	if (!s_inScene)
		return -1;
	if (color != NULL) {
	void *fragmentUniforms = NULL;

	sceGxmSetFragmentProgram(s_context, s_colorProgram);
	sceGxmReserveFragmentDefaultUniformBuffer(s_context, &fragmentUniforms);
	sceGxmSetUniformDataF(fragmentUniforms, s_colorParam, 0, 4, color);

	float aabb[4] = CLIP_AABB;

	sceGxmSetUniformDataF(fragmentUniforms, s_colorAabbParam, 0, 4, aabb);

	Matrix4 wvp = transpose(*world * s_projection);
	float wvpData[16];

	vertexUniforms = NULL;

	vst1q_f32(&wvpData[0], wvp.getCol0().get128());
	vst1q_f32(&wvpData[4], wvp.getCol1().get128());
	vst1q_f32(&wvpData[8], wvp.getCol2().get128());
	vst1q_f32(&wvpData[12], wvp.getCol3().get128());
	sceGxmReserveVertexDefaultUniformBuffer(s_context, &vertexUniforms);
	sceGxmSetUniformDataF(vertexUniforms, s_wvpParam, 0, 16, wvpData);
	sceGxmSetFrontPolygonMode(s_context, SCE_GXM_POLYGON_MODE_TRIANGLE_LINE);
	sceGxmSetBackPolygonMode(s_context, SCE_GXM_POLYGON_MODE_TRIANGLE_LINE);

	int error = sceGxmDraw(s_context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16,
			       s_shaderData->m_indices, 4);

	sceGxmSetFrontPolygonMode(s_context, SCE_GXM_POLYGON_MODE_TRIANGLE_FILL);
	sceGxmSetBackPolygonMode(s_context, SCE_GXM_POLYGON_MODE_TRIANGLE_FILL);
	return error;
	}
	return -1;
}

/* the texture at the opacity, else the color, filling the quad */
int DrawTilebackendQuad(const TilebackendTexture *texture, const Matrix4 *world,
	const float *color, float opacity)
{
	void *vertexUniforms;

	if (!s_inScene)
		return -1;
	if (texture != NULL && texture->m_base != NULL) {
		sceGxmSetFragmentProgram(s_context, s_textureProgram);

		SceGxmTextureFormat format = SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB;
		SceGxmTexture gxmTexture;

		if (texture->m_format == 1)
			format = SCE_GXM_TEXTURE_FORMAT_U8U8U8_RGB;
		sceGxmTextureInitLinear(&gxmTexture, texture->m_base, format, texture->m_width,
			texture->m_height, 1);
		sceGxmSetFragmentTexture(s_context, 0, &gxmTexture);

		void *fragmentUniforms = NULL;

		sceGxmReserveFragmentDefaultUniformBuffer(s_context, &fragmentUniforms);
		sceGxmSetUniformDataF(fragmentUniforms, s_opacityParam, 0, 1, &opacity);

		float aabb[4] = CLIP_AABB;

		sceGxmSetUniformDataF(fragmentUniforms, s_textureAabbParam, 0, 4, aabb);
	} else if (color != NULL) {
		sceGxmSetFragmentProgram(s_context, s_colorProgram);

		void *fragmentUniforms = NULL;

		sceGxmReserveFragmentDefaultUniformBuffer(s_context, &fragmentUniforms);
		sceGxmSetUniformDataF(fragmentUniforms, s_colorParam, 0, 4, color);

		float aabb[4] = CLIP_AABB;

		sceGxmSetUniformDataF(fragmentUniforms, s_colorAabbParam, 0, 4, aabb);
	} else {
		return -1;
	}

	Matrix4 wvp = transpose(*world * s_projection);
	float wvpData[16];

	vertexUniforms = NULL;

	vst1q_f32(&wvpData[0], wvp.getCol0().get128());
	vst1q_f32(&wvpData[4], wvp.getCol1().get128());
	vst1q_f32(&wvpData[8], wvp.getCol2().get128());
	vst1q_f32(&wvpData[12], wvp.getCol3().get128());
	sceGxmReserveVertexDefaultUniformBuffer(s_context, &vertexUniforms);
	sceGxmSetUniformDataF(vertexUniforms, s_wvpParam, 0, 16, wvpData);
	return sceGxmDraw(s_context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16,
			  s_shaderData->m_indices, 4);
}

#undef CLIP_AABB

/* the scene begun on the render target, with its pixels as the coordinates */
int BeginTilebackendScene(const WebViewRenderTarget *target)
{
	SceGxmRenderTarget *gxmTarget = target->m_data->m_target;
	SceGxmColorSurface surface;
	int error;

	if (gxmTarget == NULL)
		return -1;
	sceGxmColorSurfaceInit(&surface, SCE_GXM_COLOR_FORMAT_U8U8U8U8_ARGB,
		SCE_GXM_COLOR_SURFACE_LINEAR, SCE_GXM_COLOR_SURFACE_SCALE_NONE,
		SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT, target->m_width, target->m_height,
		target->m_width, target->m_base);
	sceGxmSetFrontDepthFunc(s_context, SCE_GXM_DEPTH_FUNC_ALWAYS);
	sceGxmSetBackDepthFunc(s_context, SCE_GXM_DEPTH_FUNC_ALWAYS);
	if (s_inScene)
		return -1;
	error = sceGxmBeginScene(s_context, 0, gxmTarget, NULL, NULL, NULL, &surface, NULL);
	if (error != 0)
		return error;
	s_inScene = true;
	sceGxmSetVertexProgram(s_context, s_vertexProgram);
	sceGxmSetVertexStream(s_context, 0, s_shaderData);
	s_projection = Matrix4(Vector4(2.0f / (float)target->m_width, 0.0f, 0.0f, -1.0f),
			       Vector4(0.0f, -2.0f / (float)target->m_height, 0.0f, 1.0f),
			       Vector4(0.0f), Vector4(0.0f, 0.0f, 0.0f, 1.0f));
	ResetClip();
	return 0;
}

int ScePafWebMapView_273F200C(TilebackendParam *param)
{
	char name[32];
	TilebackendCommandBuffer *buffer = new TilebackendCommandBuffer;

	if (buffer != NULL) {
		sceClibStrlcpy(name, param->m_serverName, sizeof(name));
		sceClibStrlcat(name, "B", sizeof(name));

		MemBlockUserOpt opt;

		sceClibMemset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = ALLOC_ATTR_CLIENT_INFO | ALLOC_ATTR_HAS_CLIENT_INFO;
		opt.openFlags = MEMBLOCK_OPEN_RW;
		for (int i = 0; i < 8; i++)
			opt.capabilities[i] = 0;

		SceUID memblock = sceKernelAllocMemBlock(name, SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
			TILEBACKEND_COMMAND_BUFFER_SIZE, (SceKernelAllocMemBlockOpt *)&opt);
		TilebackendMemBlock *shared = NULL;

		if (memblock >= 0) {
			shared = new TilebackendMemBlock(memblock, sceKernelFreeMemBlock);
			if (shared == NULL)
				sceKernelFreeMemBlock(memblock);
		}
		buffer->m_shared = shared;

		sceClibStrlcpy(name, param->m_serverName, sizeof(name));
		sceClibStrlcat(name, "E", sizeof(name));

		SceUID event = sceKernelCreateSimpleEvent(name,
			SCE_KERNEL_EVENT_ATTR_AUTO_RESET | SCE_KERNEL_ATTR_OPENABLE, 0, NULL);
		TilebackendKernelObject *eventObject = NULL;

		if (event >= 0) {
			eventObject = new TilebackendKernelObject(event, sceKernelDeleteSimpleEvent);
			if (eventObject == NULL)
				sceKernelDeleteSimpleEvent(event);
		}
		buffer->m_event = eventObject;

		sceClibStrlcpy(name, param->m_serverName, sizeof(name));
		sceClibStrlcat(name, "M", sizeof(name));

		SceUID mutex = sceKernelCreateMutex(name, SCE_KERNEL_ATTR_OPENABLE, 0, NULL);
		TilebackendKernelObject *mutexObject = NULL;

		if (mutex >= 0) {
			mutexObject = new TilebackendKernelObject(mutex, sceKernelDeleteMutex);
			if (mutexObject == NULL)
				sceKernelDeleteMutex(mutex);
		}
		buffer->m_mutex = mutexObject;
	}
	s_commandBuffer = buffer;
	return 0;
}

int ScePafWebMapView_46215CAD(void)
{
	TilebackendCommandBuffer *buffer = s_commandBuffer;

	if (buffer != NULL) {
		delete buffer->m_shared;
		buffer->m_shared = NULL;
		delete buffer->m_event;
		buffer->m_event = NULL;
		delete buffer->m_mutex;
		buffer->m_mutex = NULL;
		delete buffer;
	}
	s_commandBuffer = NULL;
	return 0;
}

/* the quad's corners and the strip's indices */
__attribute__((noinline)) void InitTilebackendShaderData(TilebackendShaderData *data)
{
	data->m_vertices[0].m_x = 0.0f;
	data->m_vertices[0].m_y = 0.0f;
	data->m_vertices[0].m_u = 0.0f;
	data->m_vertices[0].m_v = 0.0f;
	data->m_vertices[1].m_x = 0.0f;
	data->m_vertices[1].m_y = 1.0f;
	data->m_vertices[1].m_u = 0.0f;
	data->m_vertices[1].m_v = 1.0f;
	data->m_vertices[2].m_x = 1.0f;
	data->m_vertices[2].m_y = 0.0f;
	data->m_vertices[2].m_u = 1.0f;
	data->m_vertices[2].m_v = 0.0f;
	data->m_vertices[3].m_x = 1.0f;
	data->m_vertices[3].m_y = 1.0f;
	data->m_vertices[3].m_u = 1.0f;
	data->m_vertices[3].m_v = 1.0f;
	data->m_indices[0] = 0;
	data->m_indices[1] = 1;
	data->m_indices[2] = 2;
	data->m_indices[3] = 3;
}

/* the vertex program with its two attributes, a position and texture coordinates, in one
   stream of TilebackendVertex */
__attribute__((noinline)) SceGxmVertexProgram *CreateTilebackendVertexProgram(SceGxmShaderPatcher *patcher,
	const SceGxmProgram *program, SceGxmShaderPatcherId id)
{
	const SceGxmProgramParameter *position;
	const SceGxmProgramParameter *texCoord;
	SceGxmVertexProgram *vertexProgram;
	SceGxmVertexStream stream;
	SceGxmVertexAttribute attributes[2];

	position = sceGxmProgramFindParameterByName(program, "aPosition");
	attributes[0].streamIndex = 0;
	attributes[0].offset = 0;
	attributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	texCoord = sceGxmProgramFindParameterByName(program, "aTexCoord");
	attributes[0].componentCount = 2;
	attributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(position);
	attributes[1].streamIndex = 0;
	attributes[1].offset = 8;
	attributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	attributes[1].componentCount = 2;
	attributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(texCoord);
	stream.stride = sizeof(TilebackendVertex);
	stream.indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
	sceGxmShaderPatcherCreateVertexProgram(patcher, id, attributes, 2, &stream, 1,
		&vertexProgram);
	return vertexProgram;
}

/* the programs registered and patched, their parameters found and the quad set up */
int CreateTilebackendPrograms()
{
	const SceGxmProgram *vertexProgram;
	void *base;
	int error;

	sceGxmShaderPatcherRegisterProgram(s_patcher, &g_tilebackend_v_gxp, &s_vertexProgramId);
	sceGxmShaderPatcherRegisterProgram(s_patcher, &g_tilebackend_texture_f_gxp,
		&s_textureProgramId);
	error = sceGxmShaderPatcherRegisterProgram(s_patcher, &g_tilebackend_color_f_gxp,
		&s_colorProgramId);
	vertexProgram = sceGxmShaderPatcherGetProgramFromId(s_vertexProgramId);
	s_vertexProgram = CreateTilebackendVertexProgram(s_patcher, vertexProgram,
		s_vertexProgramId);

	/* both blend premultiplied alpha */
	SceGxmBlendInfo colorBlend = { SCE_GXM_COLOR_MASK_ALL, SCE_GXM_BLEND_FUNC_ADD,
		SCE_GXM_BLEND_FUNC_ADD, SCE_GXM_BLEND_FACTOR_ONE,
		SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, SCE_GXM_BLEND_FACTOR_ONE,
		SCE_GXM_BLEND_FACTOR_ONE };
	SceGxmFragmentProgram *colorProgram;
	SceGxmBlendInfo textureBlend = { SCE_GXM_COLOR_MASK_ALL, SCE_GXM_BLEND_FUNC_ADD,
		SCE_GXM_BLEND_FUNC_ADD, SCE_GXM_BLEND_FACTOR_ONE,
		SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, SCE_GXM_BLEND_FACTOR_ONE,
		SCE_GXM_BLEND_FACTOR_ONE };
	SceGxmFragmentProgram *textureProgram;

	sceGxmShaderPatcherCreateFragmentProgram(s_patcher, s_textureProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4, SCE_GXM_MULTISAMPLE_NONE, &textureBlend,
		vertexProgram, &textureProgram);
	s_textureProgram = textureProgram;
	sceGxmShaderPatcherCreateFragmentProgram(s_patcher, s_colorProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4, SCE_GXM_MULTISAMPLE_NONE, &colorBlend,
		vertexProgram, &colorProgram);
	s_colorProgram = colorProgram;

	s_wvpParam = sceGxmProgramFindParameterByName(vertexProgram, "wvp");
	s_colorParam = sceGxmProgramFindParameterByName(
		sceGxmShaderPatcherGetProgramFromId(s_colorProgramId), "color");
	s_opacityParam = sceGxmProgramFindParameterByName(
		sceGxmShaderPatcherGetProgramFromId(s_textureProgramId), "opacity");
	s_textureAabbParam = sceGxmProgramFindParameterByName(
		sceGxmShaderPatcherGetProgramFromId(s_textureProgramId), "aabb");
	s_colorAabbParam = sceGxmProgramFindParameterByName(
		sceGxmShaderPatcherGetProgramFromId(s_colorProgramId), "aabb");

	s_shaderDataUid = sceKernelAllocMemBlock("SceTilebackendShaderData",
		SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW, 0x1000, NULL);
	base = NULL;
	sceKernelGetMemBlockBase(s_shaderDataUid, &base);
	sceGxmMapMemory(base, 0x1000, SCE_GXM_MEMORY_ATTRIB_READ);
	s_shaderData = (TilebackendShaderData *)base;
	InitTilebackendShaderData(s_shaderData);
	return error;
}

static void *HostAlloc(void *userData, SceSize size);
static void HostFree(void *userData, void *mem);

/* the shader patcher, in its three memory blocks */
int CreateTilebackendPatcher()
{
	SceUInt32 vertexUsseOffset;
	SceUInt32 fragmentUsseOffset;
	void *fragmentUsse;
	void *vertexUsse;
	void *buffer;
	SceGxmShaderPatcherParams params;
	/* where each is, once mapped */
	void *bufferMem;
	void *vertexUsseMem;
	void *fragmentUsseMem;

	s_patcherBufferUid = sceKernelAllocMemBlock("SceTilebackendPatcherBuffer",
		SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW, 0x10000, NULL);
	buffer = NULL;
	sceKernelGetMemBlockBase(s_patcherBufferUid, &buffer);
	sceGxmMapMemory(buffer, 0x10000, SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE);
	bufferMem = buffer;

	s_patcherVertexUsseUid = sceKernelAllocMemBlock("SceTilebackendPatcherVertUsse",
		SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW, 0x10000, NULL);
	vertexUsse = NULL;
	sceKernelGetMemBlockBase(s_patcherVertexUsseUid, &vertexUsse);
	sceGxmMapVertexUsseMemory(vertexUsse, 0x10000, &vertexUsseOffset);
	vertexUsseMem = vertexUsse;

	s_patcherFragmentUsseUid = sceKernelAllocMemBlock("SceTilebackendPatcherFragUsse",
		SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW, 0x10000, NULL);
	fragmentUsse = NULL;
	sceKernelGetMemBlockBase(s_patcherFragmentUsseUid, &fragmentUsse);
	sceGxmMapFragmentUsseMemory(fragmentUsse, 0x10000, &fragmentUsseOffset);
	fragmentUsseMem = fragmentUsse;

	sceClibMemset(&params, 0, sizeof(params));
	params.userData = NULL;
	params.hostAllocCallback = HostAlloc;
	params.hostFreeCallback = HostFree;
	params.bufferAllocCallback = NULL;
	params.bufferFreeCallback = NULL;
	params.bufferMem = bufferMem;
	params.bufferMemSize = 0x10000;
	params.vertexUsseAllocCallback = NULL;
	params.vertexUsseFreeCallback = NULL;
	params.vertexUsseMem = vertexUsseMem;
	params.vertexUsseMemSize = 0x10000;
	params.vertexUsseOffset = vertexUsseOffset;
	params.fragmentUsseAllocCallback = NULL;
	params.fragmentUsseFreeCallback = NULL;
	params.fragmentUsseMem = fragmentUsseMem;
	params.fragmentUsseMemSize = 0x10000;
	params.fragmentUsseOffset = fragmentUsseOffset;
	return sceGxmShaderPatcherCreate(&params, &s_patcher);
}

int ScePafWebMapView_EE99BD02(SceGxmContext **context)
{
	s_context = *context;
	CreateTilebackendPatcher();
	return CreateTilebackendPrograms();
}

int ScePafWebMapView_DF9BB931(void)
{
	void *shaderData;
	void *buffer;
	void *vertexUsse;
	void *fragmentUsse;
	SceUID uid;
	int error;

	uid = s_shaderDataUid;
	shaderData = NULL;
	sceKernelGetMemBlockBase(uid, &shaderData);
	sceGxmUnmapMemory(shaderData);
	sceKernelFreeMemBlock(uid);
	s_shaderDataUid = -1;
	s_shaderData = NULL;
	sceGxmShaderPatcherReleaseFragmentProgram(s_patcher, s_colorProgram);
	sceGxmShaderPatcherReleaseFragmentProgram(s_patcher, s_textureProgram);
	sceGxmShaderPatcherReleaseVertexProgram(s_patcher, s_vertexProgram);
	sceGxmShaderPatcherUnregisterProgram(s_patcher, s_colorProgramId);
	sceGxmShaderPatcherUnregisterProgram(s_patcher, s_textureProgramId);
	sceGxmShaderPatcherUnregisterProgram(s_patcher, s_vertexProgramId);
	error = sceGxmShaderPatcherDestroy(s_patcher);
	s_patcher = NULL;

	uid = s_patcherFragmentUsseUid;
	fragmentUsse = NULL;
	sceKernelGetMemBlockBase(uid, &fragmentUsse);
	sceGxmUnmapFragmentUsseMemory(fragmentUsse);
	sceKernelFreeMemBlock(uid);
	uid = s_patcherVertexUsseUid;
	vertexUsse = NULL;
	sceKernelGetMemBlockBase(uid, &vertexUsse);
	sceGxmUnmapVertexUsseMemory(vertexUsse);
	sceKernelFreeMemBlock(uid);
	uid = s_patcherBufferUid;
	buffer = NULL;
	sceKernelGetMemBlockBase(uid, &buffer);
	sceGxmUnmapMemory(buffer);
	sceKernelFreeMemBlock(uid);
	s_patcherFragmentUsseUid = -1;
	s_patcherVertexUsseUid = -1;
	s_patcherBufferUid = -1;
	s_context = NULL;
	return error;
}

namespace paf {
namespace ui {

int CreateRenderTarget(WebViewRenderTarget **target, int width, int height, int stride,
	void *base, SceUID memblock)
{
	WebViewRenderTargetData *data;
	WebViewRenderTarget *renderTarget;
	SceGxmRenderTargetParams params;
	int error;

	if (target == NULL || width <= 0 || height <= 0)
		return -1;
	data = NULL;
	renderTarget = NULL;
	error = -1;
	renderTarget = new WebViewRenderTarget;
	if (renderTarget == NULL)
		goto fail;
	data = new WebViewRenderTargetData;
	if (data == NULL)
		goto fail;
	sceClibMemset(&params, 0, sizeof(params));
	params.flags = 0;
	params.width = width;
	params.height = height;
	params.scenesPerFrame = 1;
	params.multisampleMode = SCE_GXM_MULTISAMPLE_NONE;
	params.multisampleLocations = 0;
	params.driverMemBlock = SCE_UID_INVALID_UID;
	error = sceGxmCreateRenderTarget(&params, &data->m_target);
	if (error != 0)
		goto fail;
	renderTarget->m_width = width;
	renderTarget->m_height = height;
	renderTarget->m_stride = stride;
	renderTarget->m_base = base;
	renderTarget->m_data = data;
	data->m_memblock = memblock;
	*target = renderTarget;
	return 0;
fail:
	delete data;
	delete renderTarget;
	return error;
}

int CreateRenderTarget(WebViewRenderTarget **target, int width, int height, int stride,
	void *base)
{
	return CreateRenderTarget(target, width, height, stride, base, SCE_UID_INVALID_UID);
}

int DestroyRenderTarget(WebViewRenderTarget *target)
{
	if (target != NULL) {
		WebViewRenderTargetData *data = target->m_data;

		if (data->m_target != NULL) {
			SceGxmRenderTarget *gxmTarget = data->m_target;

			data->m_target = NULL;
			sceGxmDestroyRenderTarget(gxmTarget);
		}
		if (data->m_memblock != SCE_UID_INVALID_UID) {
			SceUID uid = data->m_memblock;
			void *base;

			data->m_memblock = SCE_UID_INVALID_UID;
			base = NULL;
			sceKernelGetMemBlockBase(uid, &base);
			sceGxmUnmapMemory(base);
			sceKernelFreeMemBlock(uid);
		}
		delete data;
	}
	delete target;
	return 0;
}

/* the commands' sizes add up to the length, each 8 to 0x8000 bytes */
static inline bool IsValid(const SceUInt8 *commands, SceUInt32 length)
{
	SceUInt32 rest = length;
	const SceUInt8 *command = commands;

	while (rest >= sizeof(TilebackendCommandHeader)) {
		SceUInt32 size = ((const TilebackendCommandHeader *)command)->m_size;

		if (size < sizeof(TilebackendCommandHeader) || size > TILEBACKEND_COMMAND_BUFFER_SIZE)
			return false;
		if (rest < size)
			break;
		rest -= size;
		command += size;
	}
	return rest == 0 && length >= sizeof(TilebackendCommandHeader);
}

/* the commands the page's process queued, waited for (8 ms) and drawn; the render target is
   the one the commands name */
int ProcessRenderCommands(WebViewRenderTarget *target)
{
	TilebackendCommandBuffer *buffer = s_commandBuffer;

	if (buffer == NULL)
		return -1;

	TilebackendKernelObject *event = buffer->m_event;

	if (event == NULL || buffer->m_mutex == NULL || buffer->m_shared == NULL)
		return -1;

	SceUInt32 result = 0;
	SceUInt32 timeout = 8000;
	SceUInt64 userData;
	int error = sceKernelWaitEvent(event->m_uid, SCE_KERNEL_EVENT_USER_DEFINED_MASK,
				       &result, &userData, &timeout);

	if (error != 0) {
		EndScene();
		return error;
	}

	TilebackendKernelObject *mutex = buffer->m_mutex;
	TilebackendMemBlock *shared = buffer->m_shared;

	sceKernelLockMutex(mutex->m_uid, 1, NULL);
	sceClibMemcpy_safe(buffer->m_commands, shared->m_base, TILEBACKEND_COMMAND_BUFFER_SIZE);
	sceKernelUnlockMutex(mutex->m_uid, 1);

	SceUInt32 length = (SceUInt32)userData;

	if (!IsValid(buffer->m_commands, length))
		return 0;

	SceUInt8 *command = buffer->m_commands;

	do {
		TilebackendCommandHeader *header = (TilebackendCommandHeader *)command;
		SceUInt32 size = header->m_size;

		if (length < size)
			break;
		switch (header->m_type) {
		case TILEBACKEND_COMMAND_BEGIN_SCENE:
			BeginTilebackendScene((const WebViewRenderTarget *)(header + 1));
			break;
		case TILEBACKEND_COMMAND_END_SCENE:
			EndScene();
			break;
		case TILEBACKEND_COMMAND_DRAW_TEXTURE: {
			TilebackendDrawTexture *draw = (TilebackendDrawTexture *)command;

			if (draw->m_texture.m_base != NULL)
				draw->m_texture.m_base = (SceUInt8 *)GetTilebackendPoolBase(
					ScePafWebMapView_C2581D7F()) +
					(uintptr_t)draw->m_texture.m_base - 0x1000;
			DrawTilebackendQuad(&draw->m_texture, (const Matrix4 *)draw->m_world, NULL,
				draw->m_opacity);
			break;
		}
		case TILEBACKEND_COMMAND_FILL: {
			TilebackendDrawColor *draw = (TilebackendDrawColor *)command;

			DrawTilebackendQuad(NULL, (const Matrix4 *)draw->m_world, draw->m_color, 1.0f);
			break;
		}
		case TILEBACKEND_COMMAND_OUTLINE: {
			TilebackendDrawColor *draw = (TilebackendDrawColor *)command;

			DrawTilebackendOutline((const Matrix4 *)draw->m_world, draw->m_color);
			break;
		}
		case TILEBACKEND_COMMAND_CLIP: {
			TilebackendClip *clip = (TilebackendClip *)command;

			if (clip->m_enabled) {
				s_clip.m_x = clip->m_rect[0];
				s_clip.m_y = clip->m_rect[1];
				s_clip.m_width = clip->m_rect[2];
				s_clip.m_height = clip->m_rect[3];
			} else {
				ResetClip();
			}
			break;
		}
		}
		length -= size;
		command += size;
	} while (length >= sizeof(TilebackendCommandHeader));
	return 0;
}

} /* namespace ui */
} /* namespace paf */

static void *HostAlloc(void *userData, SceSize size)
{
	return new char[size];
}

static void HostFree(void *userData, void *mem)
{
	delete[] (char *)mem;
}
