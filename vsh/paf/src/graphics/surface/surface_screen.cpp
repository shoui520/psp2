/*
 * paf::graph::ScreenSurface: a surface GXM draws into, a colour surface per page over the
 * surface's pages, a render target the surface's size and a depth surface after the pages (the
 * file's name ours: the code follows surface_texture.cpp's in the binary). Its scenes go to the
 * context SetRawContext sets, else to the front end's.
 */
#include <stddef.h>
#include <stdint.h>
#include <gxm.h>
#include <kernel/sysmem/memblock.h>
#include <sysmem/memblock.h>
#include <vectormath.h>
#include <common/common_internal.h>
#include <math/ngp/v2.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_screen.h>
#include <std/memory>
#include <std/string.h>
#include <src/module.h>
#include <src/paf_types.h>
#include "../../std/math.h"

extern "C" {
/* SceGxmInternal: a render target in the memory block its parameters name */
int sceGxmGetRenderTargetMemSizeInternal(const SceGxmRenderTargetParams *params,
					 SceSize *driverMemSize);
int sceGxmCreateRenderTargetInternal(const SceGxmRenderTargetParams *params,
				     SceGxmRenderTarget **renderTarget);
}

using namespace sce::Vectormath::Simd::Aos;

namespace paf {
namespace graph {

/* an image mode's or order's GXM counterpart, -1 for none (names ours) */
static const struct {
	ImageMode mode;
	SceGxmColorFormat format;
} s_color_formats[] = {
	{ ImageMode_RGBA8888, SCE_GXM_COLOR_FORMAT_A8B8G8R8 },
	{ ImageMode_NONE, (SceGxmColorFormat)-1 },
};

static const struct {
	ImageOrder order;
	SceGxmColorSurfaceType type;
} s_color_surface_types[] = {
	{ ImageOrder_Linear, SCE_GXM_COLOR_SURFACE_LINEAR },
	{ ImageOrder_Swizzle, SCE_GXM_COLOR_SURFACE_SWIZZLED },
	{ ImageOrder_Tiled, SCE_GXM_COLOR_SURFACE_TILED },
	{ (ImageOrder)0, (SceGxmColorSurfaceType)-1 },
};

static const struct {
	ImageMode mode;
	int32_t format;
} s_depth_formats[] = {
	{ ImageMode_depth16, SCE_GXM_DEPTH_STENCIL_FORMAT_D16 },
	{ ImageMode_depth32, SCE_GXM_DEPTH_STENCIL_FORMAT_DF32 },
	{ ImageMode_NONE, -1 },
};

/* SetRawContext's (name ours) */
static SceGxmContext *s_raw_context;

/* a coordinate kept in a size (name ours) */
static inline int32_t ClampToSize(int32_t value, int32_t size)
{
	int32_t max = size - 1;

	if (value < 0)
		value = 0;
	if (max > value)
		max = value;
	return max;
}

int32_t ScreenSurface::ConvertDepthFormat(ImageMode format)
{
	for (int32_t i = 0; i < 3; i++)
		if (s_depth_formats[i].mode == format)
			return s_depth_formats[i].format;
	return -1;
}

int32_t ScreenSurface::GetDepthFormat()
{
	return ConvertDepthFormat(m_depth->m_format);
}

int32_t ScreenSurface::InitDepthSurface()
{
	int32_t ret;

	if (m_depth_stencil_surface == NULL)
		return 0;
	if (m_depth.get() != NULL && m_depth_offset != 0) {
		int32_t format = ConvertDepthFormat(m_depth->m_format);

		if (format == -1)
			return SCE_PAF_ERROR_GRAPHICS_NOT_SUPPORTED;
		ret = sceGxmDepthStencilSurfaceInit(m_depth_stencil_surface,
						    (SceGxmDepthStencilFormat)format,
						    SCE_GXM_DEPTH_STENCIL_SURFACE_LINEAR,
						    m_depth->GetWidth(), (void *)m_depth_offset, NULL);
	} else {
		ret = sceGxmDepthStencilSurfaceInitDisabled(m_depth_stencil_surface);
	}
	if (ret < 0)
		return ret;
	return 0;
}

int32_t ScreenSurface::DestroyRenderTarget(SceGxmRenderTarget *render_target)
{
	SceUID block;
	int32_t ret;

	if (render_target == NULL)
		return SCE_PAF_ERROR_GRAPHICS_INVALID_POINTER;
	if (!m_own_render_target)
		return 0;
	sceGxmRenderTargetGetDriverMemBlock(render_target, &block);
	ret = sceGxmDestroyRenderTarget(render_target);
	sceKernelFreeMemBlock(block);
	return ret;
}

int32_t ScreenSurface::CreateRenderTarget(SceGxmRenderTarget **render_target,
					  SceKernelMemBlockType mem_type)
{
	SceSize size;
	SceGxmRenderTargetParams params;

	if (m_own_render_target) {
		SceGxmRenderTarget *old = *render_target;

		if (old != NULL) {
			SceUID block;

			sceGxmRenderTargetGetDriverMemBlock(old, &block);

			int32_t ret = sceGxmDestroyRenderTarget(old);

			sceKernelFreeMemBlock(block);
			if (ret != 0)
				common_internal::on_error();
			*render_target = NULL;
		}
	}

	math::v2 scene_size;

	if (m_scale_mode == 2 && !m_full_size)
		scene_size = math::v2((float)m_width, (float)m_height) / 2.0f;
	else
		scene_size = math::v2((float)m_width, (float)m_height);
	sce_paf_memset(&params, 0, sizeof(params));
	params.flags = 0;
	params.width = (int32_t)scene_size.extract_x();
	params.height = (int32_t)scene_size.extract_y();
	params.scenesPerFrame = 1;

	SceGxmMultisampleMode multisample_mode;

	if (m_scale_mode == 0)
		multisample_mode = SCE_GXM_MULTISAMPLE_NONE;
	else if (m_scale_mode == 1)
		multisample_mode = SCE_GXM_MULTISAMPLE_2X;
	else if (m_scale_mode == 2)
		multisample_mode = SCE_GXM_MULTISAMPLE_4X;
	else
		multisample_mode = SCE_GXM_MULTISAMPLE_NONE;
	params.multisampleMode = multisample_mode;
	params.multisampleLocations = 0;
	params.driverMemBlock = -1;
	if (sce_paf_is_common_dialog_mode)
		params.flags = 4;
	if (sceGxmGetRenderTargetMemSizeInternal(&params, &size) != 0)
		common_internal::on_error();
	if (mem_type == 0) {
		mem_type = SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW;
		if (sce_paf_is_common_dialog_mode)
			mem_type = SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_NC_RW;
	}
	params.driverMemBlock = sceKernelAllocMemBlock("ScePafSurfaceRenderTarget", mem_type, size,
						       NULL);
	if (params.driverMemBlock < 0)
		common_internal::on_error();

	int32_t ret = sceGxmCreateRenderTargetInternal(&params, render_target);

	if (ret != 0)
		common_internal::on_error();
	m_own_render_target = true;
	return ret;
}

int32_t ScreenSurface::SetColorSurface(int32_t page, SceGxmColorFormat format,
				       SceGxmColorSurfaceType type, uint32_t stride, void *data)
{
	if (m_color_surfaces == NULL)
		return SCE_PAF_ERROR_GRAPHICS_INVALID_POINTER;
	return sceGxmColorSurfaceInit(&m_color_surfaces[page], format, type,
				      (SceGxmColorSurfaceScaleMode)(m_full_size != 0),
				      m_output_register_size, m_width, m_height, stride, data);
}

SceGxmColorSurfaceType ScreenSurface::ConvertColorSurfaceOrder(ImageOrder order)
{
	for (int32_t i = 0; i < 4; i++)
		if (s_color_surface_types[i].order == order)
			return s_color_surface_types[i].type;
	return (SceGxmColorSurfaceType)-1;
}

SceGxmColorSurfaceType ScreenSurface::GetColorSurfaceOrder()
{
	return ConvertColorSurfaceOrder(m_order);
}

SceGxmColorFormat ScreenSurface::ConvertColorSurfaceFormat(ImageMode format)
{
	for (int32_t i = 0; i < 2; i++)
		if (s_color_formats[i].mode == format)
			return s_color_formats[i].format;
	return (SceGxmColorFormat)-1;
}

SceGxmColorFormat ScreenSurface::GetColorSurfaceFormat()
{
	return ConvertColorSurfaceFormat(m_format);
}

int32_t ScreenSurface::InitColorSurface()
{
	if (m_color_surfaces == NULL)
		return SCE_PAF_ERROR_GRAPHICS_INVALID_POINTER;

	SceGxmColorFormat format = GetColorSurfaceFormat();

	if (format == (SceGxmColorFormat)-1)
		return SCE_PAF_ERROR_GRAPHICS_NOT_SUPPORTED;

	SceGxmColorSurfaceType type = GetColorSurfaceOrder();

	if (type == (SceGxmColorSurfaceType)-1)
		return SCE_PAF_ERROR_GRAPHICS_NOT_SUPPORTED;
	for (int32_t page = 0; page < m_num_page; page++)
		SetColorSurface(page, format, type, (m_stride * 8) >> m_pixel_bits,
				GetPagePtr(page));
	return 0;
}

void ScreenSurface::SetScissor(float x0, float y0, float x1, float y1, bool update)
{
	int32_t scissor_x0 = ClampToSize((int32_t)sce_paf_roundf(x0), m_width);
	int32_t scissor_y0 = ClampToSize((int32_t)sce_paf_roundf(y0), m_height);
	int32_t scissor_x1 = ClampToSize((int32_t)sce_paf_roundf(x1) - 1, m_width);
	int32_t scissor_y1 = ClampToSize((int32_t)sce_paf_roundf(y1) - 1, m_height);

	if (m_scissor_x0 == scissor_x0 && m_scissor_y0 == scissor_y0 &&
	    m_scissor_x1 == scissor_x1 && m_scissor_y1 == scissor_y1)
		return;
	m_scissor_x0 = scissor_x0;
	m_scissor_y0 = scissor_y0;
	m_scissor_x1 = scissor_x1;
	m_scissor_y1 = scissor_y1;
	if (update) {
		bool drawn = false;

		if (arl::Gl()->m_in_scene) {
			intrusive_ptr<Surface> surface(arl::Gl()->m_next_scene.m_surface);

			if (surface.get() == this)
				drawn = true;
		}
		if (drawn) {
			EndScene();
			BeginScene();
		}
	}
}

int32_t ScreenSurface::Initialize(int32_t width, int32_t height, ImageMode format,
				  ImageOrder order, int32_t num_page, ImageMode depth_format,
				  ImageOrder depth_order, void *bp)
{
	m_memory = bp;
	if (num_page != 0) {
		m_color_surfaces = new SceGxmColorSurface[m_num_page];
		sce_paf_memset(m_color_surfaces, 0, sizeof(SceGxmColorSurface) * m_num_page);
		if (GetTextureFormat(depth_format, false) != (SceGxmTextureFormat)-1) {
			uint32_t depth_offset = 0;

			if (bp != NULL)
				depth_offset = ((m_page_size * num_page + 511) & ~511) + (uint32_t)bp;
			m_depth_offset = depth_offset;

			intrusive_ptr<Surface> depth(new Surface(width, height, depth_format,
								 depth_order, 1, NULL, 0, 1, false));

			m_depth = depth;
			m_depth_stencil_surface = new SceGxmDepthStencilSurface;
			sce_paf_memset(m_depth_stencil_surface, 0, sizeof(SceGxmDepthStencilSurface));
		}
	}
	if (m_num_page > 1)
		ActivatePage(1);
	SetScissor(0.0f, 0.0f, (float)width, (float)height, false);
	return 0;
}

ScreenSurface::ScreenSurface(const Param *param, void *bp, SurfaceType surf_type)
	: Surface(param->width, param->height, param->format, param->order, surf_type,
		  param->num_page, bp, 0, 1, false),
	  m_color_surfaces(NULL), m_render_target(NULL), m_depth_stencil_surface(NULL),
	  m_own_render_target(false), m_yuv_csc_index(0),
	  m_yuv_profile(SCE_GXM_YUV_PROFILE_BT601_STANDARD),
	  m_output_register_size(SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT), m_max_width(0),
	  m_max_height(0), m_scale_mode(0), m_full_size(false), m_memory(NULL), m_depth_offset(0)
{
	Initialize(param->width, param->height, param->format, param->order, param->num_page,
		   param->depth_format, param->depth_order, bp);
	m_full_size = param->full_size;
	m_scale_mode = param->scale_mode;
	if (bp != NULL) {
		m_max_width = param->width;
		m_max_height = param->height;
	}
	if (surf_type == TypeRenderTarget) {
		if (InitColorSurface() >= 0)
			CreateRenderTarget(&m_render_target, 0);
		InitDepthSurface();
	}
}

ScreenSurface::ScreenSurface(int32_t width, int32_t height, ImageMode format, ImageOrder order,
			     int32_t num_page, ImageMode depth_format, ImageOrder depth_order,
			     void *bp, SurfaceType surf_type)
	: Surface(width, height, format, order, surf_type, num_page, bp, 0, 1, false),
	  m_color_surfaces(NULL), m_render_target(NULL), m_depth_stencil_surface(NULL),
	  m_own_render_target(false), m_yuv_csc_index(0),
	  m_yuv_profile(SCE_GXM_YUV_PROFILE_BT601_STANDARD),
	  m_output_register_size(SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT), m_max_width(0),
	  m_max_height(0), m_scale_mode(0), m_full_size(false), m_memory(NULL), m_depth_offset(0)
{
	Initialize(width, height, format, order, num_page, depth_format, depth_order, bp);
	if (bp != NULL) {
		m_max_width = width;
		m_max_height = height;
	}
	if (surf_type == TypeRenderTarget) {
		if (InitColorSurface() >= 0)
			CreateRenderTarget(&m_render_target, 0);
		InitDepthSurface();
	}
}

ScreenSurface::ScreenSurface(SurfacePool *pool, int32_t width, int32_t height, ImageMode format,
			     ImageOrder order, int32_t num_page, ImageMode depth_format,
			     ImageOrder depth_order, SurfaceType surf_type)
	: Surface(pool, width, height, format, order, surf_type, num_page, 1, false),
	  m_color_surfaces(NULL), m_render_target(NULL), m_depth_stencil_surface(NULL),
	  m_own_render_target(false), m_yuv_csc_index(0),
	  m_yuv_profile(SCE_GXM_YUV_PROFILE_BT601_STANDARD),
	  m_output_register_size(SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT), m_max_width(0),
	  m_max_height(0), m_scale_mode(0), m_full_size(false), m_memory(NULL), m_depth_offset(0)
{
	Initialize(width, height, format, order, num_page, depth_format, depth_order, NULL);
	if (surf_type == TypeRenderTarget) {
		if (InitColorSurface() >= 0)
			CreateRenderTarget(&m_render_target, 0);
		InitDepthSurface();
	}
}

ScreenSurface::ScreenSurface(SceGxmRenderTarget *render_target, SurfacePool *pool,
			     int32_t width, int32_t height, ImageMode format, ImageOrder order,
			     int32_t num_page, ImageMode depth_format, ImageOrder depth_order)
	: Surface(pool, width, height, format, order, TypeRenderTarget, num_page, 1, false),
	  m_color_surfaces(NULL), m_render_target(NULL), m_depth_stencil_surface(NULL),
	  m_own_render_target(false), m_yuv_csc_index(0),
	  m_yuv_profile(SCE_GXM_YUV_PROFILE_BT601_STANDARD),
	  m_output_register_size(SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT), m_max_width(0),
	  m_max_height(0), m_scale_mode(0), m_full_size(false), m_memory(NULL), m_depth_offset(0)
{
	Initialize(width, height, format, order, num_page, depth_format, depth_order, NULL);
	if (InitColorSurface() >= 0) {
		m_render_target = render_target;
		InitDepthSurface();
	}
}

SceGxmContext *ScreenSurface::GetRawContext()
{
	if (s_raw_context == NULL)
		return arl::Gl()->m_context;
	return s_raw_context;
}

int32_t ScreenSurface::Begin_(SceGxmRenderTarget *render_target, SceGxmSyncObject *sync_object,
			      SceGxmColorSurface *color_surface,
			      SceGxmDepthStencilSurface *depth_stencil, uint32_t flags)
{
	SceGxmContext *context = GetRawContext();

	if (context == NULL)
		return SCE_PAF_ERROR_GRAPHICS_INVALID_POINTER;

	math::v2 scene_size;

	if (m_scale_mode == 2 && !m_full_size)
		scene_size = math::v2((float)m_width, (float)m_height) / 2.0f;
	else
		scene_size = math::v2((float)m_width, (float)m_height);

	arl::ArlInstance *gl = arl::Gl();
	arl::Rect viewport;

	viewport.m_x1 = (int32_t)scene_size.extract_x();
	viewport.m_y1 = (int32_t)scene_size.extract_y();
	ScePafGraphics_93CBC733(gl, viewport);

	int32_t x0 = m_scissor_x0;
	int32_t y0 = m_scissor_y0;
	int32_t x1 = m_scissor_x1;
	int32_t y1 = m_scissor_y1;

	if (x0 < 0)
		x0 = 0;
	if (y0 < 0)
		y0 = 0;
	if (x1 > m_width - 1)
		x1 = m_width - 1;
	if (y1 > m_height - 1)
		y1 = m_height - 1;
	sceGxmColorSurfaceSetClip(color_surface, x0, y0, x1, y1);
	sceGxmSetYuvProfile(arl::Gl()->m_context, m_yuv_csc_index, m_yuv_profile);

	int32_t ret = sceGxmBeginScene(context, flags, render_target, NULL, NULL, sync_object,
				       color_surface, depth_stencil);

	if (ret >= 0)
		ret = 0;
	return ret;
}

int32_t ScreenSurface::Begin(uint32_t flags)
{
	return Begin_(m_render_target, NULL, &m_color_surfaces[m_page_counter],
		      m_depth_stencil_surface, flags);
}

int32_t ScreenSurface::End(const SceGxmNotification *vertex_notification,
			   const SceGxmNotification *fragment_notification)
{
	SceGxmContext *context = GetRawContext();

	if (context == NULL)
		return SCE_PAF_ERROR_GRAPHICS_INVALID_POINTER;

	int32_t ret = sceGxmEndScene(context, vertex_notification, fragment_notification);

	if (ret >= 0)
		return 0;
	return ret;
}

void ScreenSurface::Swap()
{
	Surface::Swap();
}

int32_t ScreenSurface::GenTexture()
{
	if (m_tex_pages == NULL)
		return 0;

	int32_t ret = Surface::GenTexture(m_tex_pages, m_mem.bp);

	if (ret >= 0)
		return 0;
	return ret;
}

void ScreenSurface::SetClip(float x0, float y0, float x1, float y1, bool update)
{
	m_clip_x0 = ClampToSize((int32_t)sce_paf_roundf(x0), m_width);
	m_clip_y0 = ClampToSize((int32_t)sce_paf_roundf(y0), m_height);
	m_clip_x1 = ClampToSize((int32_t)sce_paf_roundf(x1) - 1, m_width);
	m_clip_y1 = ClampToSize((int32_t)sce_paf_roundf(y1) - 1, m_height);
	if (update && arl::Gl()->m_context != NULL)
		sceGxmSetRegionClip(arl::Gl()->m_context, SCE_GXM_REGION_CLIP_OUTSIDE, m_clip_x0,
				    m_clip_y0, m_clip_x1, m_clip_y1);
}

void *ScreenSurface::Lock(int32_t page)
{
	return Surface::Lock(page);
}

void ScreenSurface::Unlock()
{
	Surface::Unlock();
}

void ScreenSurface::SetRawContext(SceGxmContext *context)
{
	s_raw_context = context;
}

SceGxmColorSurface *ScreenSurface::GetRawColorSurface(int32_t page)
{
	return &m_color_surfaces[page];
}

int32_t ScreenSurface::Resize(int32_t width, int32_t height)
{
	int32_t ret = 0;

	if (!m_own_render_target)
		return SCE_PAF_ERROR_NOT_PERMITTED;
	if (m_width == width && m_height == height)
		return ret;
	if (m_max_width != 0 && width > m_max_width)
		return SCE_PAF_ERROR_INVALID_ARGUMENT;
	if (m_max_height != 0 && height > m_max_height)
		return SCE_PAF_ERROR_INVALID_ARGUMENT;
	ret = Surface::Resize(width, height);
	if (ret == 0) {
		ret = InitColorSurface();
		if (ret == 0) {
			if (m_depth.get() != NULL) {
				m_depth->Resize(m_width, m_height);
				ret = InitDepthSurface();
				if (ret != 0)
					common_internal::on_error();
			}
			if (m_render_target != NULL) {
				ret = CreateRenderTarget(&m_render_target, 0);
				if (ret != 0)
					common_internal::on_error();
			}
		}
	}
	return ret;
}

ScreenSurface::~ScreenSurface()
{
	m_depth.clear();
	if (m_own_render_target && m_render_target != NULL) {
		SceGxmRenderTarget *render_target = m_render_target;
		SceUID block;

		sceGxmRenderTargetGetDriverMemBlock(render_target, &block);
		sceGxmDestroyRenderTarget(render_target);
		sceKernelFreeMemBlock(block);
		m_render_target = NULL;
	}
	delete m_depth_stencil_surface;
	delete[] m_color_surfaces;
}

int32_t ScreenSurface::GetDepthSize(int32_t width, int32_t height, ImageMode format,
				    ImageOrder order)
{
	return ScePafGraphics_48FBF58D(TypeRenderTarget, width, height, format, order, 1, 0, 1, 1);
}

}
}
