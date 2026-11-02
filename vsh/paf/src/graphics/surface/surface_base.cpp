/*
 * paf::graph::SurfaceBase, what Surface and SurfaceCLUT share: the format and its pixel size;
 * and DisplaySurface's frame begin and end, under the surface list's lock (the file's name
 * ours: the code follows surface.cpp's in the binary, and surface.cpp's code calls it out of
 * line).
 */
#include <stddef.h>
#include <stdint.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_display.h>
#include <thread/thread.h>
#include <src/paf_types.h>

namespace paf {
namespace graph {

/* the display surface whose frames put the active mark at the list's head: nothing sets it,
   but the code reads it, so it is not static (name ours) */
DisplaySurface *s_marked_display = NULL;

int32_t SurfaceBase::GetPixelBits(ImageMode format)
{
	switch (format) {
	case ImageMode_DXT1:
	case ImageMode_index4:
		return 2;
	case ImageMode_A8:
	case ImageMode_L8:
	case ImageMode_DXT2:
	case ImageMode_DXT3:
	case ImageMode_DXT4:
	case ImageMode_DXT5:
	case ImageMode_index8:
	case ImageMode_YUV420P2:
	case ImageMode_YUV420P3:
	case ImageMode_PVRT2BPP:
	case ImageMode_PVRT4BPP:
	case ImageMode_PVRTII2BPP:
	case ImageMode_PVRTII4BPP:
		return 3;
	case ImageMode_XBGR0565:
	case ImageMode_ABGR1555:
	case ImageMode_ABGR4444:
	case ImageMode_index16:
	case ImageMode_depth16:
		return 4;
	case ImageMode_ABGR8888:
	case ImageMode_RGBA8888:
	case ImageMode_ARGB8888:
	case ImageMode_BGRA8888:
	case ImageMode_XCRYCB8888:
	case ImageMode_CRYCBX8888:
	case ImageMode_index32:
	case ImageMode_depth32:
	case ImageMode_A32FP:
		return 5;
	case ImageMode_WZYX16FP:
	case ImageMode_XCRYCB16FP:
		return 6;
	case ImageMode_WZYX32FP:
		return 7;
	default:
		return 0;
	}
}

SurfaceBase::SurfaceBase(ImageMode format, bool reserved)
{
	m_format = format;
	m_reserved = reserved;
	m_pixel_bits = GetPixelBits(format);
}

int32_t SurfaceBase::PixelsToBytes(int32_t pixels) const
{
	return ((pixels << m_pixel_bits) + 7) >> 3;
}

SurfaceBase::~SurfaceBase()
{
}

int32_t DisplaySurface::StartFrame()
{
	thread::RMutexLock lock(&m_list_cs);

	if (m_surf_type == TypeTexture)
		return SCE_PAF_ERROR_GRAPHICS_NOT_SUPPORTED;
	if (s_marked_display == this) {
		m_active_mark.m_prev = &m_surface_list;
		m_active_mark.m_next = m_surface_list.m_next;
		m_surface_list.m_next->m_prev = &m_active_mark;
		m_surface_list.m_next = &m_active_mark;
	}
	return OnStartFrame();
}

int32_t DisplaySurface::EndFrame()
{
	thread::RMutexLock lock(&m_list_cs);

	if (m_surf_type == TypeTexture)
		return SCE_PAF_ERROR_GRAPHICS_NOT_SUPPORTED;
	return OnEndFrame();
}

}
}
