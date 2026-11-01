/*
 * paf::graph::arl: the GXM layer under paf's graphics, its enums and the constants every arl
 * file sees. Names after ScePaf-RE's arl_defines.h and the mangled export names.
 */
#ifndef PAF_GRAPHICS_ARL_ARL_DEFINES_H
#define PAF_GRAPHICS_ARL_ARL_DEFINES_H

#include <stdint.h>
#include <gxm.h>
#include <common/enum_flag.h>

namespace paf {
namespace graph {
namespace arl {

/* what a surface is allocated for and how it is laid out, each the index of its bit */
enum SurfaceFlagsBits {
	READ = 0,
	WRITE = 1,
	PALETTE = 2,
	TEXTURE = 3,
	TEXTURE_LINEAR = 4,
	TEXTURE_LINEAR_STRIDED = 5,
	TEXTURE_TILED = 6,
	TEXTURE_SWIZZLED = 7,
	TEXTURE_ALLOC_MIPMAPS = 8,
	COLOR_SURFACE = 9,
	COLOR_SURFACE_LINEAR = 10,
	COLOR_SURFACE_TILED = 11,
	COLOR_SURFACE_SWIZZLED = 12,
	COLOR_SURFACE_SCALE_NONE = 13,
	COLOR_SURFACE_SCALE_MSAA_DOWNSCALE = 14,
	COLOR_SURFACE_OUTPUT_REGISTER_SIZE_32BIT = 15,
	COLOR_SURFACE_OUTPUT_REGISTER_SIZE_64BIT = 16,
	STENCILDEPTH_SURFACE = 17,
	STENCILDEPTH_SURFACE_LINEAR = 18,
	STENCILDEPTH_SURFACE_TILED = 19,
	STENCILDEPTH_SURFACE_FORCE_STORE = 20,
	STENCILDEPTH_SURFACE_FORCE_LOAD = 21
};

typedef common::EnumFlag<SurfaceFlagsBits> SurfaceFlags;

/* a texture's addressing: GXM's own values (enumerator names ours) */
enum TextureWrap {
	TEXTURE_WRAP_REPEAT = SCE_GXM_TEXTURE_ADDR_REPEAT,
	TEXTURE_WRAP_MIRROR = SCE_GXM_TEXTURE_ADDR_MIRROR,
	TEXTURE_WRAP_CLAMP = SCE_GXM_TEXTURE_ADDR_CLAMP,
	TEXTURE_WRAP_MIRROR_CLAMP = SCE_GXM_TEXTURE_ADDR_MIRROR_CLAMP,
	TEXTURE_WRAP_REPEAT_IGNORE_BORDER = SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER,
	TEXTURE_WRAP_CLAMP_FULL_BORDER = SCE_GXM_TEXTURE_ADDR_CLAMP_FULL_BORDER,
	TEXTURE_WRAP_CLAMP_IGNORE_BORDER = SCE_GXM_TEXTURE_ADDR_CLAMP_IGNORE_BORDER,
	TEXTURE_WRAP_CLAMP_HALF_BORDER = SCE_GXM_TEXTURE_ADDR_CLAMP_HALF_BORDER
};

/* the size of GXM's tiles, which every arl file has its copy of (names ours) */
class TileSize {
public:
	__attribute__((always_inline)) TileSize(int32_t width, int32_t height)
		: m_width(width), m_height(height)
	{
	}

	int32_t m_width;
	int32_t m_height;
};

static const TileSize s_tile_size(32, 32);

}
}
}

#endif
