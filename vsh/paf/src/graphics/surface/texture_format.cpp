/*
 * The GXM texture format of an ImageMode: the colour formats with their alpha, or without it for
 * a surface that keeps none (SurfaceBase's reserved flag), and the depth formats (the file's and
 * the functions' names ours; Sony's file is not named by any evidence).
 */
#include <stddef.h>
#include <stdint.h>
#include <gxm.h>
#include <image/image_types.h>
#include <graphics/surface/surface.h>

namespace paf {
namespace graph {

#define NO_FORMAT ((SceGxmTextureFormat)-1)

/* ImageMode_XBGR0565..ImageMode_PVRTII4BPP with their alpha (name ours) */
static const SceGxmTextureFormat s_texture_formats[] = {
	SCE_GXM_TEXTURE_FORMAT_U5U6U5_BGR,
	SCE_GXM_TEXTURE_FORMAT_U1U5U5U5_ABGR,
	SCE_GXM_TEXTURE_FORMAT_U4U4U4U4_ABGR,
	NO_FORMAT,
	SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR,
	SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_BGRA,
	SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB,
	NO_FORMAT,
	NO_FORMAT,
	NO_FORMAT,
	NO_FORMAT,
	NO_FORMAT,
	NO_FORMAT,
	NO_FORMAT,
	SCE_GXM_TEXTURE_FORMAT_U8_R111,
	SCE_GXM_TEXTURE_FORMAT_U8_1RRR,
	SCE_GXM_TEXTURE_FORMAT_UBC1_ABGR,
	NO_FORMAT,
	SCE_GXM_TEXTURE_FORMAT_UBC2_ABGR,
	NO_FORMAT,
	SCE_GXM_TEXTURE_FORMAT_UBC3_ABGR,
	SCE_GXM_TEXTURE_FORMAT_P4_ABGR,
	SCE_GXM_TEXTURE_FORMAT_P8_ABGR,
	NO_FORMAT,
	NO_FORMAT,
	NO_FORMAT,
	NO_FORMAT,
	SCE_GXM_TEXTURE_FORMAT_YVU420P2_CSC0,
	SCE_GXM_TEXTURE_FORMAT_YUV420P3_CSC0,
	SCE_GXM_TEXTURE_FORMAT_PVRT2BPP_ABGR,
	SCE_GXM_TEXTURE_FORMAT_PVRT4BPP_ABGR,
	SCE_GXM_TEXTURE_FORMAT_PVRTII2BPP_ABGR,
	SCE_GXM_TEXTURE_FORMAT_PVRTII4BPP_ABGR,
};

/* the same with the alpha read as one (name ours) */
static const SceGxmTextureFormat s_opaque_texture_formats[] = {
	SCE_GXM_TEXTURE_FORMAT_U5U6U5_BGR,
	SCE_GXM_TEXTURE_FORMAT_X1U5U5U5_1BGR,
	SCE_GXM_TEXTURE_FORMAT_X4U4U4U4_1BGR,
	NO_FORMAT,
	SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1BGR,
	SCE_GXM_TEXTURE_FORMAT_U8U8U8X8_BGR1,
	SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1RGB,
	NO_FORMAT,
	NO_FORMAT,
	NO_FORMAT,
	NO_FORMAT,
	NO_FORMAT,
	NO_FORMAT,
	NO_FORMAT,
	SCE_GXM_TEXTURE_FORMAT_U8_R111,
	SCE_GXM_TEXTURE_FORMAT_U8_1RRR,
	SCE_GXM_TEXTURE_FORMAT_UBC1_1BGR,
	NO_FORMAT,
	SCE_GXM_TEXTURE_FORMAT_UBC2_1BGR,
	NO_FORMAT,
	SCE_GXM_TEXTURE_FORMAT_UBC3_1BGR,
	SCE_GXM_TEXTURE_FORMAT_P4_1BGR,
	SCE_GXM_TEXTURE_FORMAT_P8_1BGR,
	NO_FORMAT,
	NO_FORMAT,
	NO_FORMAT,
	NO_FORMAT,
	SCE_GXM_TEXTURE_FORMAT_YVU420P2_CSC0,
	SCE_GXM_TEXTURE_FORMAT_YUV420P3_CSC0,
	SCE_GXM_TEXTURE_FORMAT_PVRT2BPP_1BGR,
	SCE_GXM_TEXTURE_FORMAT_PVRT4BPP_1BGR,
	SCE_GXM_TEXTURE_FORMAT_PVRTII2BPP_1BGR,
	SCE_GXM_TEXTURE_FORMAT_PVRTII4BPP_1BGR,
};

/* ImageMode_depth16 and ImageMode_depth32 (name ours) */
static const SceGxmTextureFormat s_depth_texture_formats[] = {
	SCE_GXM_TEXTURE_FORMAT_U16_000R,
	SCE_GXM_TEXTURE_FORMAT_F32M_000R,
};

static inline SceGxmTextureFormat TextureFormat(ImageMode format, bool reserved)
{
	if (format >= ImageMode_XBGR0565 && format <= ImageMode_PVRTII4BPP) {
		if (!reserved)
			return s_texture_formats[format];
		return s_opaque_texture_formats[format];
	}
	if (format >= ImageMode_depth16 && format <= ImageMode_depth32)
		return s_depth_texture_formats[format - ImageMode_depth16];
	return NO_FORMAT;
}

SceGxmTextureFormat GetTextureFormat(ImageMode format, bool reserved)
{
	return TextureFormat(format, reserved);
}

SceGxmTextureFormat GetTextureFormat(const SurfaceBase *surface)
{
	return TextureFormat(surface->m_format, surface->m_reserved);
}

}
}
