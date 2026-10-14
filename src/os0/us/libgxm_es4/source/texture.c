/*
 * SceGxm: texture control words.
 *
 * A texture is four control words. The format is spread over three of
 * them, the type selects how width and height are stored (log2 for the
 * swizzled and cube types, minus one for the others), and a linear
 * strided texture reuses the filter and mip fields of word 0 for its
 * stride.
 */

#include "gxm_internal.h"

static inline SceGxmTextureFormat gxmTextureGetFormat(const SceGxmTexture *texture)
{
	return (texture->controlWords[1] & GXM_FORMAT_BASE) |
	       (texture->controlWords[0] & GXM_FORMAT_BASE_EXT) |
	       (((const SceUInt16 *)texture->controlWords)[7] & GXM_FORMAT_SWIZZLE);
}

static inline SceUInt32 gxmTextureGetType(const SceGxmTexture *texture)
{
	return texture->controlWords[1] & GXM_TEX1_TYPE;
}

static inline SceUInt32 gxmLog2(SceUInt32 value)
{
	return 31 - __builtin_clz(value);
}

/* word 0 of a new texture: clamped, neutral LOD bias */
static inline SceUInt32 gxmTextureWord0(SceGxmTextureFormat format, SceUInt32 mipCount)
{
	return ((mipCount - 1) & 0xf) << GXM_TEX0_MIP_COUNT_SHIFT |
	       0x1f << GXM_TEX0_LOD_BIAS_SHIFT |
	       SCE_GXM_TEXTURE_ADDR_CLAMP << GXM_TEX0_UADDR_MODE_SHIFT |
	       SCE_GXM_TEXTURE_ADDR_CLAMP << GXM_TEX0_VADDR_MODE_SHIFT |
	       (format & GXM_FORMAT_BASE_EXT);
}

/* word 3 of a new texture: normalized, no palette */
static inline SceUInt32 gxmTextureWord3(SceGxmTextureFormat format)
{
	return (format & GXM_FORMAT_SWIZZLE) << 16 | GXM_TEX3_NORMALIZE_MODE;
}

static inline int gxmIsPow2(SceUInt32 value)
{
	return (value & (value - 1)) == 0 && value != 0;
}

int sceGxmTextureInitSwizzled(SceGxmTexture *texture, const void *data,
			      SceGxmTextureFormat texFormat, SceUInt32 width,
			      SceUInt32 height, SceUInt32 mipCount)
{
	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!gxmTextureFormatIsSupported(texFormat))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (((SceUInt32)data & (gxmTextureFormatGetAlignment(texFormat) - 1)) != 0 ||
	    !gxmIsPow2(width) || !gxmIsPow2(height))
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (width - 1 >= GXM_TEXTURE_MAX_SIZE || height - 1 >= GXM_TEXTURE_MAX_SIZE ||
	    mipCount > GXM_TEXTURE_MAX_MIP_COUNT)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (gxmTextureFormatIsYuv(texFormat))
		return SCE_GXM_ERROR_UNSUPPORTED;

	texture->controlWords[0] = gxmTextureWord0(texFormat, mipCount);
	texture->controlWords[1] = gxmLog2(height) | (texFormat & GXM_FORMAT_BASE) |
				   gxmLog2(width) << GXM_TEX1_WIDTH_LOG2_SHIFT;
	texture->controlWords[2] = (SceUInt32)data & GXM_TEX2_DATA;
	texture->controlWords[3] = gxmTextureWord3(texFormat);
	return 0;
}

int sceGxmTextureInitSwizzledArbitrary(SceGxmTexture *texture, const void *data,
				       SceGxmTextureFormat texFormat, SceUInt32 width,
				       SceUInt32 height, SceUInt32 mipCount)
{
	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!gxmTextureFormatIsSupported(texFormat))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (((SceUInt32)data & (gxmTextureFormatGetAlignment(texFormat) - 1)) != 0)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (width - 1 >= GXM_TEXTURE_MAX_SIZE || height - 1 >= GXM_TEXTURE_MAX_SIZE ||
	    mipCount > GXM_TEXTURE_MAX_MIP_COUNT)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (gxmTextureFormatIsYuv(texFormat))
		return SCE_GXM_ERROR_UNSUPPORTED;

	texture->controlWords[0] = gxmTextureWord0(texFormat, mipCount);
	texture->controlWords[1] = (height - 1) | SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY |
				   (width - 1) << GXM_TEX1_WIDTH_SHIFT |
				   (texFormat & GXM_FORMAT_BASE);
	texture->controlWords[2] = (SceUInt32)data & GXM_TEX2_DATA;
	texture->controlWords[3] = gxmTextureWord3(texFormat);
	return 0;
}

int sceGxmTextureInitLinear(SceGxmTexture *texture, const void *data,
			    SceGxmTextureFormat texFormat, SceUInt32 width,
			    SceUInt32 height, SceUInt32 mipCount)
{
	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!gxmTextureFormatIsSupported(texFormat))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (gxmTextureFormatIsCompressed(texFormat))
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (((SceUInt32)data & (gxmTextureFormatGetAlignment(texFormat) - 1)) != 0)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (width - 1 >= GXM_TEXTURE_MAX_SIZE || height - 1 >= GXM_TEXTURE_MAX_SIZE ||
	    mipCount > GXM_TEXTURE_MAX_MIP_COUNT)
		return SCE_GXM_ERROR_INVALID_VALUE;
	/* a single-level three-plane YUV texture needs a width of whole 16s */
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_YUV_TILED) &&
	    (texFormat & 0x9f000000) == SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3 &&
	    mipCount == 0 && (width & 0xf) != 0)
		return SCE_GXM_ERROR_INVALID_VALUE;

	texture->controlWords[2] = (SceUInt32)data & GXM_TEX2_DATA;
	texture->controlWords[0] = gxmTextureWord0(texFormat, mipCount);
	texture->controlWords[1] = (height - 1) | SCE_GXM_TEXTURE_LINEAR |
				   (width - 1) << GXM_TEX1_WIDTH_SHIFT |
				   (texFormat & GXM_FORMAT_BASE);
	texture->controlWords[3] = gxmTextureWord3(texFormat);
	return 0;
}

int sceGxmTextureInitTiled(SceGxmTexture *texture, const void *data,
			   SceGxmTextureFormat texFormat, SceUInt32 width,
			   SceUInt32 height, SceUInt32 mipCount)
{
	SceUInt32 lastLevel;

	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!gxmTextureFormatIsSupported(texFormat))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (gxmTextureFormatIsCompressed(texFormat) ||
	    (!(gxmGlobals.compatFlags & GXM_COMPAT_YUV_TILED) &&
	     gxmTextureFormatIsYuvPlanar(texFormat)))
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (((SceUInt32)data & (gxmTextureFormatGetAlignment(texFormat) - 1)) != 0)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (width - GXM_TEXTURE_TILE_SIZE > GXM_TEXTURE_MAX_SIZE - GXM_TEXTURE_TILE_SIZE ||
	    height - GXM_TEXTURE_TILE_SIZE > GXM_TEXTURE_MAX_SIZE - GXM_TEXTURE_TILE_SIZE ||
	    mipCount > GXM_TEXTURE_MAX_MIP_COUNT)
		return SCE_GXM_ERROR_INVALID_VALUE;

	/* the smallest level must still cover a whole tile */
	if (mipCount == 0) {
		lastLevel = 0;
	} else {
		lastLevel = mipCount - 1;
		if (width >> lastLevel < GXM_TEXTURE_TILE_SIZE)
			return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (height >> lastLevel < GXM_TEXTURE_TILE_SIZE)
		return SCE_GXM_ERROR_INVALID_VALUE;

	texture->controlWords[2] = (SceUInt32)data & GXM_TEX2_DATA;
	texture->controlWords[0] = gxmTextureWord0(texFormat, mipCount);
	texture->controlWords[1] = (height - 1) | SCE_GXM_TEXTURE_TILED |
				   (width - 1) << GXM_TEX1_WIDTH_SHIFT |
				   (texFormat & GXM_FORMAT_BASE);
	texture->controlWords[3] = gxmTextureWord3(texFormat);
	return 0;
}

int sceGxmTextureInitCube(SceGxmTexture *texture, const void *data,
			  SceGxmTextureFormat texFormat, SceUInt32 width,
			  SceUInt32 height, SceUInt32 mipCount)
{
	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!gxmTextureFormatIsSupported(texFormat))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (((SceUInt32)data & (gxmTextureFormatGetAlignment(texFormat) - 1)) != 0 ||
	    !gxmIsPow2(width) || !gxmIsPow2(height))
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (width - 1 >= GXM_TEXTURE_MAX_SIZE || height - 1 >= GXM_TEXTURE_MAX_SIZE ||
	    mipCount > GXM_TEXTURE_MAX_MIP_COUNT)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (gxmTextureFormatIsYuv(texFormat))
		return SCE_GXM_ERROR_UNSUPPORTED;

	texture->controlWords[0] = gxmTextureWord0(texFormat, mipCount);
	texture->controlWords[1] = (texFormat & GXM_FORMAT_BASE) | SCE_GXM_TEXTURE_CUBE |
				   gxmLog2(height) |
				   gxmLog2(width) << GXM_TEX1_WIDTH_LOG2_SHIFT;
	texture->controlWords[2] = (SceUInt32)data & GXM_TEX2_DATA;
	texture->controlWords[3] = gxmTextureWord3(texFormat);
	return 0;
}

int sceGxmTextureInitCubeArbitrary(SceGxmTexture *texture, const void *data,
				   SceGxmTextureFormat texFormat, SceUInt32 width,
				   SceUInt32 height, SceUInt32 mipCount)
{
	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!gxmTextureFormatIsSupported(texFormat))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (((SceUInt32)data & (gxmTextureFormatGetAlignment(texFormat) - 1)) != 0)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (width - 1 >= GXM_TEXTURE_MAX_SIZE || height - 1 >= GXM_TEXTURE_MAX_SIZE ||
	    mipCount > GXM_TEXTURE_MAX_MIP_COUNT)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (gxmTextureFormatIsYuv(texFormat))
		return SCE_GXM_ERROR_UNSUPPORTED;

	texture->controlWords[0] = gxmTextureWord0(texFormat, mipCount);
	texture->controlWords[1] = (height - 1) | SCE_GXM_TEXTURE_CUBE_ARBITRARY |
				   (width - 1) << GXM_TEX1_WIDTH_SHIFT |
				   (texFormat & GXM_FORMAT_BASE);
	texture->controlWords[2] = (SceUInt32)data & GXM_TEX2_DATA;
	texture->controlWords[3] = gxmTextureWord3(texFormat);
	return 0;
}

/* a linear strided texture keeps (stride / 4 - 1) in three pieces of word 0 */
static inline SceUInt32 gxmTextureStrideBits(SceUInt32 byteStride)
{
	SceUInt32 stride = (byteStride >> 2) - 1;

	return ((stride << 9) & GXM_TEX0_STRIDE_LOW) |
	       ((stride << 14) & GXM_TEX0_STRIDE) |
	       ((stride >> 12) & GXM_TEX0_STRIDE_EXT);
}

int sceGxmTextureInitLinearStrided(SceGxmTexture *texture, const void *data,
				   SceGxmTextureFormat texFormat, SceUInt32 width,
				   SceUInt32 height, SceUInt32 byteStride)
{
	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!gxmTextureFormatIsSupported(texFormat))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (gxmTextureFormatIsCompressed(texFormat))
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (((SceUInt32)data & (gxmTextureFormatGetAlignment(texFormat) - 1)) != 0)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (width - 1 >= GXM_TEXTURE_MAX_SIZE || height - 1 >= GXM_TEXTURE_MAX_SIZE)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if ((byteStride & 3) != 0)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (byteStride - 4 > GXM_TEXTURE_MAX_STRIDE - 4)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (gxmTextureFormatIs24Bit(texFormat))
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (gxmTextureFormatIsYuvPlanar(texFormat))
		return SCE_GXM_ERROR_UNSUPPORTED;

	texture->controlWords[1] = (height - 1) | SCE_GXM_TEXTURE_LINEAR_STRIDED |
				   (width - 1) << GXM_TEX1_WIDTH_SHIFT |
				   (texFormat & GXM_FORMAT_BASE);
	texture->controlWords[2] = (SceUInt32)data & GXM_TEX2_DATA;
	texture->controlWords[3] = gxmTextureWord3(texFormat);
	texture->controlWords[0] = gxmTextureStrideBits(byteStride) |
				   SCE_GXM_TEXTURE_ADDR_CLAMP << GXM_TEX0_UADDR_MODE_SHIFT |
				   SCE_GXM_TEXTURE_ADDR_CLAMP << GXM_TEX0_VADDR_MODE_SHIFT |
				   (texFormat & GXM_FORMAT_BASE_EXT);
	return 0;
}

SceUInt32 sceGxmTextureGetType(const SceGxmTexture *texture)
{
	return gxmTextureGetType(texture);
}

int sceGxmTextureSetMinFilter(SceGxmTexture *texture, SceUInt32 minFilter)
{
	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (gxmTextureGetType(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return SCE_GXM_ERROR_UNSUPPORTED;
	texture->controlWords[0] = ((minFilter << GXM_TEX0_MIN_FILTER_SHIFT) & GXM_TEX0_MIN_FILTER) |
				   (texture->controlWords[0] & ~GXM_TEX0_MIN_FILTER);
	return 0;
}

/* a linear strided texture uses its mag filter for both */
SceUInt32 sceGxmTextureGetMinFilter(const SceGxmTexture *texture)
{
	if (gxmTextureGetType(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return (texture->controlWords[0] & GXM_TEX0_MAG_FILTER) >> GXM_TEX0_MAG_FILTER_SHIFT;
	return (texture->controlWords[0] & GXM_TEX0_MIN_FILTER) >> GXM_TEX0_MIN_FILTER_SHIFT;
}

int sceGxmTextureSetMagFilter(SceGxmTexture *texture, SceUInt32 magFilter)
{
	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	texture->controlWords[0] = ((magFilter << GXM_TEX0_MAG_FILTER_SHIFT) & GXM_TEX0_MAG_FILTER) |
				   (texture->controlWords[0] & ~GXM_TEX0_MAG_FILTER);
	return 0;
}

SceUInt32 sceGxmTextureGetMagFilter(const SceGxmTexture *texture)
{
	return (texture->controlWords[0] & GXM_TEX0_MAG_FILTER) >> GXM_TEX0_MAG_FILTER_SHIFT;
}

int sceGxmTextureSetMipFilter(SceGxmTexture *texture, SceUInt32 mipFilter)
{
	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (gxmTextureGetType(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return SCE_GXM_ERROR_UNSUPPORTED;
	texture->controlWords[0] = (mipFilter & GXM_TEX0_MIP_FILTER) |
				   (texture->controlWords[0] & ~GXM_TEX0_MIP_FILTER);
	return 0;
}

SceUInt32 sceGxmTextureGetMipFilter(const SceGxmTexture *texture)
{
	if (gxmTextureGetType(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return 0;
	return texture->controlWords[0] & GXM_TEX0_MIP_FILTER;
}

/*
 * The border modes need a plain format (not palettized, compressed or
 * YUV) on a swizzled texture; mirroring needs a swizzled texture too.
 * A linear strided texture always clamps.
 */
static inline int gxmTextureSetAddrModeSafe(SceGxmTexture *texture, SceUInt32 mode,
					    unsigned int shift, SceUInt32 mask)
{
	SceUInt32 type;
	SceGxmTextureFormat format;

	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	type = gxmTextureGetType(texture);
	if (type == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
		if (mode == SCE_GXM_TEXTURE_ADDR_CLAMP)
			return 0;
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (mode - SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER < 4) {
		format = gxmTextureGetFormat(texture);
		if (gxmTextureFormatIsPalettized(format) ||
		    gxmTextureFormatIsCompressed(format) ||
		    gxmTextureFormatIsYuv(format) ||
		    (type != SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY && type != SCE_GXM_TEXTURE_SWIZZLED))
			return SCE_GXM_ERROR_UNSUPPORTED;
	} else if (mode == SCE_GXM_TEXTURE_ADDR_MIRROR) {
		if (type != SCE_GXM_TEXTURE_SWIZZLED)
			return SCE_GXM_ERROR_UNSUPPORTED;
	}
	texture->controlWords[0] = ((mode << shift) & mask) | (texture->controlWords[0] & ~mask);
	return 0;
}

int sceGxmTextureSetUAddrModeSafe(SceGxmTexture *texture, SceUInt32 mode)
{
	return gxmTextureSetAddrModeSafe(texture, mode, GXM_TEX0_UADDR_MODE_SHIFT, GXM_TEX0_UADDR_MODE);
}

/* cube maps clamp, whatever the caller asks for */
static inline int gxmTextureIsCube(SceUInt32 word1)
{
	return (word1 & 0x60000000) == SCE_GXM_TEXTURE_CUBE ||
	       (word1 & GXM_TEX1_TYPE) == SCE_GXM_TEXTURE_CUBE_ARBITRARY;
}

static inline int gxmTextureSetAddrMode(SceGxmTexture *texture, SceUInt32 mode,
					unsigned int shift, SceUInt32 mask)
{
	SceUInt32 word1, type;
	SceGxmTextureFormat format;

	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	word1 = texture->controlWords[1];
	type = word1 & GXM_TEX1_TYPE;
	if (gxmTextureIsCube(word1)) {
		if (mode != SCE_GXM_TEXTURE_ADDR_CLAMP)
			return SCE_GXM_ERROR_UNSUPPORTED;
	} else {
		if (mode - SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER < 4) {
			format = (word1 & GXM_FORMAT_BASE) |
				 (texture->controlWords[0] & GXM_FORMAT_BASE_EXT) |
				 (((const SceUInt16 *)texture->controlWords)[7] & GXM_FORMAT_SWIZZLE);
			if (gxmTextureFormatIsPalettized(format))
				return SCE_GXM_ERROR_UNSUPPORTED;
			if (gxmTextureFormatIsCompressed(format))
				return SCE_GXM_ERROR_UNSUPPORTED;
			if (gxmTextureFormatIsYuv(format))
				return SCE_GXM_ERROR_UNSUPPORTED;
			if (type != SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY && type != SCE_GXM_TEXTURE_SWIZZLED)
				return SCE_GXM_ERROR_UNSUPPORTED;
		}
		if (mode == SCE_GXM_TEXTURE_ADDR_MIRROR) {
			if (type != SCE_GXM_TEXTURE_SWIZZLED)
				return SCE_GXM_ERROR_UNSUPPORTED;
		}
	}
	texture->controlWords[0] = ((mode << shift) & mask) | (texture->controlWords[0] & ~mask);
	return 0;
}

int sceGxmTextureSetUAddrMode(SceGxmTexture *texture, SceUInt32 mode)
{
	return gxmTextureSetAddrMode(texture, mode, GXM_TEX0_UADDR_MODE_SHIFT, GXM_TEX0_UADDR_MODE);
}

SceUInt32 sceGxmTextureGetUAddrModeSafe(const SceGxmTexture *texture)
{
	if (gxmTextureGetType(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return SCE_GXM_TEXTURE_ADDR_CLAMP;
	return (texture->controlWords[0] & GXM_TEX0_UADDR_MODE) >> GXM_TEX0_UADDR_MODE_SHIFT;
}

SceUInt32 sceGxmTextureGetUAddrMode(const SceGxmTexture *texture)
{
	return (texture->controlWords[0] & GXM_TEX0_UADDR_MODE) >> GXM_TEX0_UADDR_MODE_SHIFT;
}

int sceGxmTextureSetVAddrModeSafe(SceGxmTexture *texture, SceUInt32 mode)
{
	return gxmTextureSetAddrModeSafe(texture, mode, GXM_TEX0_VADDR_MODE_SHIFT, GXM_TEX0_VADDR_MODE);
}

int sceGxmTextureSetVAddrMode(SceGxmTexture *texture, SceUInt32 mode)
{
	return gxmTextureSetAddrMode(texture, mode, GXM_TEX0_VADDR_MODE_SHIFT, GXM_TEX0_VADDR_MODE);
}

SceUInt32 sceGxmTextureGetVAddrModeSafe(const SceGxmTexture *texture)
{
	if (gxmTextureGetType(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return SCE_GXM_TEXTURE_ADDR_CLAMP;
	return (texture->controlWords[0] & GXM_TEX0_VADDR_MODE) >> GXM_TEX0_VADDR_MODE_SHIFT;
}

SceUInt32 sceGxmTextureGetVAddrMode(const SceGxmTexture *texture)
{
	return (texture->controlWords[0] & GXM_TEX0_VADDR_MODE) >> GXM_TEX0_VADDR_MODE_SHIFT;
}

int sceGxmTextureSetFormat(SceGxmTexture *texture, SceGxmTextureFormat texFormat)
{
	SceGxmTextureFormat oldFormat;
	SceUInt32 word1, type;

	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!gxmTextureFormatIsSupported(texFormat))
		return SCE_GXM_ERROR_INVALID_VALUE;

	/* the texel size cannot change */
	oldFormat = gxmTextureGetFormat(texture);
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_FORMAT_BYTES)) {
		if (gxmTextureFormatGetBitsPerPixel(oldFormat) !=
		    gxmTextureFormatGetBitsPerPixel(texFormat))
			return SCE_GXM_ERROR_UNSUPPORTED;
	} else {
		if (gxmTextureFormatGetBytesPerPixel(oldFormat) !=
		    gxmTextureFormatGetBytesPerPixel(texFormat))
			return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if ((texture->controlWords[2] & GXM_TEX2_DATA &
	     (gxmTextureFormatGetAlignment(texFormat) - 1)) != 0)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;

	word1 = texture->controlWords[1];
	type = word1 & GXM_TEX1_TYPE;
	if (gxmTextureFormatIsCompressed(texFormat) &&
	    (type == SCE_GXM_TEXTURE_LINEAR || (word1 & 0xa0000000) == SCE_GXM_TEXTURE_TILED))
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (gxmGlobals.compatFlags & GXM_COMPAT_YUV_ANY_TYPE) {
		if (gxmTextureFormatIsYuv(texFormat) &&
		    (type == SCE_GXM_TEXTURE_CUBE || type == SCE_GXM_TEXTURE_CUBE_ARBITRARY))
			return SCE_GXM_ERROR_UNSUPPORTED;
	} else {
		/* planar YUV is linear only, or tiled too for older SDKs */
		if (gxmTextureFormatIsYuvPlanar(texFormat) &&
		    ((gxmGlobals.compatFlags & GXM_COMPAT_YUV_TILED) ?
		     type != SCE_GXM_TEXTURE_TILED && type != SCE_GXM_TEXTURE_LINEAR :
		     type != SCE_GXM_TEXTURE_LINEAR))
			return SCE_GXM_ERROR_UNSUPPORTED;
		if (gxmTextureFormatIsYuv422(texFormat) &&
		    type != SCE_GXM_TEXTURE_LINEAR && (word1 & 0xa0000000) != SCE_GXM_TEXTURE_TILED)
			return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_24BIT_STRIDED) &&
	    gxmTextureFormatIs24Bit(texFormat) && type == SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return SCE_GXM_ERROR_UNSUPPORTED;

	/* formats that cannot use the border address modes */
	if (gxmTextureFormatIsCompressed(texFormat) || gxmTextureFormatIsYuv(texFormat) ||
	    gxmTextureFormatIsPalettized(texFormat)) {
		if (((texture->controlWords[0] & GXM_TEX0_UADDR_MODE) >> GXM_TEX0_UADDR_MODE_SHIFT) -
		    SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER < 4)
			return SCE_GXM_ERROR_UNSUPPORTED;
		if (((texture->controlWords[0] & GXM_TEX0_VADDR_MODE) >> GXM_TEX0_VADDR_MODE_SHIFT) -
		    SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER < 4)
			return SCE_GXM_ERROR_UNSUPPORTED;
	}

	/* older SDKs tested the gamma capability the wrong way round */
	if (gxmGlobals.compatFlags & GXM_COMPAT_GAMMA_INVERTED) {
		if (gxmTextureFormatIsGammaCapable(texFormat) &&
		    (texture->controlWords[0] & GXM_TEX0_GAMMA_MODE) != 0)
			return SCE_GXM_ERROR_UNSUPPORTED;
	} else {
		if (!gxmTextureFormatIsGammaCapable(texFormat) &&
		    (texture->controlWords[0] & GXM_TEX0_GAMMA_MODE) != 0)
			return SCE_GXM_ERROR_UNSUPPORTED;
	}

	texture->controlWords[0] = (texture->controlWords[0] & ~GXM_FORMAT_BASE_EXT) |
				   (texFormat & GXM_FORMAT_BASE_EXT);
	texture->controlWords[3] = (texture->controlWords[3] & ~GXM_TEX3_SWIZZLE) |
				   (texFormat & GXM_FORMAT_SWIZZLE) << 16;
	texture->controlWords[1] = (texFormat & GXM_FORMAT_BASE) |
				   (texture->controlWords[1] & ~GXM_FORMAT_BASE);
	return 0;
}

SceGxmTextureFormat sceGxmTextureGetFormat(const SceGxmTexture *texture)
{
	return gxmTextureGetFormat(texture);
}

int sceGxmTextureSetLodBias(SceGxmTexture *texture, SceUInt32 bias)
{
	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (gxmTextureGetType(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (bias >= GXM_TEXTURE_MAX_LOD_BIAS)
		return SCE_GXM_ERROR_INVALID_VALUE;
	texture->controlWords[0] = (texture->controlWords[0] & ~GXM_TEX0_LOD_BIAS) |
				   bias << GXM_TEX0_LOD_BIAS_SHIFT;
	return 0;
}

SceUInt32 sceGxmTextureGetLodBias(const SceGxmTexture *texture)
{
	if (gxmTextureGetType(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return 0;
	return (texture->controlWords[0] & GXM_TEX0_LOD_BIAS) >> GXM_TEX0_LOD_BIAS_SHIFT;
}

/* the minimum LOD is split: bits 2..3 in word 2, bits 0..1 in word 3 */
int sceGxmTextureSetLodMin(SceGxmTexture *texture, SceUInt32 lodMin)
{
	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (gxmTextureGetType(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (lodMin > GXM_TEXTURE_MAX_MIP_COUNT - 1)
		return SCE_GXM_ERROR_INVALID_VALUE;
	texture->controlWords[2] = (texture->controlWords[2] & ~GXM_TEX2_LOD_MIN_HIGH) |
				   lodMin >> 2;
	texture->controlWords[3] = ((lodMin << GXM_TEX3_LOD_MIN_LOW_SHIFT) & GXM_TEX3_LOD_MIN_LOW) |
				   (texture->controlWords[3] & ~GXM_TEX3_LOD_MIN_LOW);
	return 0;
}

SceUInt32 sceGxmTextureGetLodMin(const SceGxmTexture *texture)
{
	if (gxmTextureGetType(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return 0;
	return (texture->controlWords[3] & GXM_TEX3_LOD_MIN_LOW) >> GXM_TEX3_LOD_MIN_LOW_SHIFT |
	       (texture->controlWords[2] & GXM_TEX2_LOD_MIN_HIGH) << 2;
}

int sceGxmTextureSetStride(SceGxmTexture *texture, SceUInt32 byteStride)
{
	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (gxmTextureGetType(texture) != SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return SCE_GXM_ERROR_UNSUPPORTED;
	if ((byteStride & 3) != 0)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (byteStride - 4 > GXM_TEXTURE_MAX_STRIDE - 4)
		return SCE_GXM_ERROR_INVALID_VALUE;
	texture->controlWords[0] = gxmTextureStrideBits(byteStride) |
				   (texture->controlWords[0] &
				    ~(GXM_TEX0_STRIDE_LOW | GXM_TEX0_STRIDE | GXM_TEX0_STRIDE_EXT));
	return 0;
}

SceUInt32 sceGxmTextureGetStride(const SceGxmTexture *texture)
{
	SceUInt32 word0;

	if (gxmTextureGetType(texture) != SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return 0;
	word0 = texture->controlWords[0];
	return (((word0 & GXM_TEX0_STRIDE_EXT) >> 1 << 13 |
		 (word0 & GXM_TEX0_STRIDE) >> 17 << 3 |
		 (word0 & GXM_TEX0_STRIDE_LOW) >> 9) + 1) * 4;
}

int _sceGxmTextureSetWidth(SceGxmTexture *texture, SceUInt32 width)
{
	SceUInt32 word1;

	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (width - 1 >= GXM_TEXTURE_MAX_SIZE)
		return SCE_GXM_ERROR_INVALID_VALUE;
	word1 = texture->controlWords[1];
	if ((word1 & GXM_TEXTURE_TYPE_ARBITRARY_SIZE) != 0) {
		texture->controlWords[1] = (word1 & ~GXM_TEX1_WIDTH) |
					   (width - 1) << GXM_TEX1_WIDTH_SHIFT;
		return 0;
	}
	if (((width - 1) & width) != 0)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	texture->controlWords[1] = (word1 & ~GXM_TEX1_WIDTH_LOG2) |
				   gxmLog2(width) << GXM_TEX1_WIDTH_LOG2_SHIFT;
	return 0;
}

static inline _Bool gxmTextureIsSingleLevel(const SceGxmTexture *texture)
{
	if (gxmTextureGetType(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return 1;
	return (texture->controlWords[0] & GXM_TEX0_MIP_COUNT) >> GXM_TEX0_MIP_COUNT_SHIFT == 0xf;
}

int sceGxmTextureSetWidth(SceGxmTexture *texture, SceUInt32 width)
{
	SceUInt32 word1, type, lastLevel;
	_Bool singleLevel;

	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (width - 1 >= GXM_TEXTURE_MAX_SIZE)
		return SCE_GXM_ERROR_INVALID_VALUE;
	word1 = texture->controlWords[1];
	type = word1 & GXM_TEX1_TYPE;

	if (!(gxmGlobals.compatFlags & GXM_COMPAT_YUV_TILED)) {
		/* a single-level three-plane YUV texture needs a width of whole 16s */
		singleLevel = gxmTextureIsSingleLevel(texture);
		if (((word1 & GXM_FORMAT_BASE) | (texture->controlWords[0] & GXM_FORMAT_BASE_EXT)) !=
		    SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3)
			singleLevel = 0;
		if (singleLevel && (width & 0xf) != 0)
			return SCE_GXM_ERROR_INVALID_VALUE;
	}

	if (type == SCE_GXM_TEXTURE_TILED) {
		lastLevel = (((texture->controlWords[0] & GXM_TEX0_MIP_COUNT) >>
			      GXM_TEX0_MIP_COUNT_SHIFT) + 1) & 0xf;
		if (lastLevel != 0)
			lastLevel--;
		if (width >> lastLevel < GXM_TEXTURE_TILE_SIZE)
			return SCE_GXM_ERROR_INVALID_VALUE;
	} else if ((word1 & GXM_TEXTURE_TYPE_ARBITRARY_SIZE) == 0) {
		if (((width - 1) & width) != 0)
			return SCE_GXM_ERROR_INVALID_ALIGNMENT;
		texture->controlWords[1] = (word1 & ~GXM_TEX1_WIDTH_LOG2) |
					   gxmLog2(width) << GXM_TEX1_WIDTH_LOG2_SHIFT;
		return 0;
	}
	texture->controlWords[1] = (word1 & ~GXM_TEX1_WIDTH) | (width - 1) << GXM_TEX1_WIDTH_SHIFT;
	return 0;
}

SceUInt32 sceGxmTextureGetWidth(const SceGxmTexture *texture)
{
	SceUInt32 word1 = texture->controlWords[1];

	if ((word1 & GXM_TEXTURE_TYPE_ARBITRARY_SIZE) == 0)
		return 1 << ((word1 & GXM_TEX1_WIDTH_LOG2) >> GXM_TEX1_WIDTH_LOG2_SHIFT);
	return ((word1 & GXM_TEX1_WIDTH) >> GXM_TEX1_WIDTH_SHIFT) + 1;
}

int _sceGxmTextureSetHeight(SceGxmTexture *texture, SceUInt32 height)
{
	SceUInt32 word1;

	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (height - 1 >= GXM_TEXTURE_MAX_SIZE)
		return SCE_GXM_ERROR_INVALID_VALUE;
	word1 = texture->controlWords[1];
	if ((word1 & GXM_TEXTURE_TYPE_ARBITRARY_SIZE) != 0) {
		texture->controlWords[1] = (height - 1) | (word1 & ~GXM_TEX1_HEIGHT);
		return 0;
	}
	if (((height - 1) & height) != 0)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	texture->controlWords[1] = gxmLog2(height) | (word1 & ~GXM_TEX1_HEIGHT_LOG2);
	return 0;
}

int sceGxmTextureSetHeight(SceGxmTexture *texture, SceUInt32 height)
{
	SceUInt32 word1, lastLevel;

	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (height - 1 >= GXM_TEXTURE_MAX_SIZE)
		return SCE_GXM_ERROR_INVALID_VALUE;
	word1 = texture->controlWords[1];
	if ((word1 & GXM_TEX1_TYPE) == SCE_GXM_TEXTURE_TILED) {
		lastLevel = (((texture->controlWords[0] & GXM_TEX0_MIP_COUNT) >>
			      GXM_TEX0_MIP_COUNT_SHIFT) + 1) & 0xf;
		if (lastLevel != 0)
			lastLevel--;
		if (height >> lastLevel < GXM_TEXTURE_TILE_SIZE)
			return SCE_GXM_ERROR_INVALID_VALUE;
	} else if ((word1 & GXM_TEXTURE_TYPE_ARBITRARY_SIZE) == 0) {
		if (((height - 1) & height) != 0)
			return SCE_GXM_ERROR_INVALID_ALIGNMENT;
		texture->controlWords[1] = gxmLog2(height) | (word1 & ~GXM_TEX1_HEIGHT_LOG2);
		return 0;
	}
	texture->controlWords[1] = (height - 1) | (word1 & ~GXM_TEX1_HEIGHT);
	return 0;
}

SceUInt32 sceGxmTextureGetHeight(const SceGxmTexture *texture)
{
	SceUInt32 word1 = texture->controlWords[1];

	if ((word1 & GXM_TEXTURE_TYPE_ARBITRARY_SIZE) == 0)
		return 1 << (word1 & GXM_TEX1_HEIGHT_LOG2);
	return (word1 & GXM_TEX1_HEIGHT) + 1;
}

int sceGxmTextureSetMipmapCount(SceGxmTexture *texture, SceUInt32 mipCount)
{
	SceUInt32 word1, lastLevel, width;

	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	word1 = texture->controlWords[1];
	if ((word1 & GXM_TEX1_TYPE) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (mipCount > GXM_TEXTURE_MAX_MIP_COUNT)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if ((word1 & GXM_TEX1_TYPE) == SCE_GXM_TEXTURE_TILED) {
		/* the smallest level must still cover a whole tile */
		width = ((word1 & GXM_TEX1_WIDTH) >> GXM_TEX1_WIDTH_SHIFT) + 1;
		if (mipCount == 0) {
			if (width < GXM_TEXTURE_TILE_SIZE)
				return SCE_GXM_ERROR_INVALID_VALUE;
			lastLevel = 0;
		} else {
			lastLevel = mipCount - 1;
			if (width >> lastLevel < GXM_TEXTURE_TILE_SIZE)
				return SCE_GXM_ERROR_INVALID_VALUE;
		}
		if (((word1 & GXM_TEX1_HEIGHT) + 1) >> lastLevel < GXM_TEXTURE_TILE_SIZE)
			return SCE_GXM_ERROR_INVALID_VALUE;
	}
	texture->controlWords[0] = (texture->controlWords[0] & ~GXM_TEX0_MIP_COUNT) |
				   ((mipCount - 1) & 0xf) << GXM_TEX0_MIP_COUNT_SHIFT;
	return 0;
}

SceUInt32 sceGxmTextureGetMipmapCountUnsafe(const SceGxmTexture *texture)
{
	return (((texture->controlWords[0] & GXM_TEX0_MIP_COUNT) >> GXM_TEX0_MIP_COUNT_SHIFT) + 1) & 0xf;
}

SceUInt32 sceGxmTextureGetMipmapCount(const SceGxmTexture *texture)
{
	if (gxmTextureGetType(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return 0;
	return (((texture->controlWords[0] & GXM_TEX0_MIP_COUNT) >> GXM_TEX0_MIP_COUNT_SHIFT) + 1) & 0xf;
}

int sceGxmTextureSetData(SceGxmTexture *texture, const void *data)
{
	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (((gxmTextureFormatGetAlignment(gxmTextureGetFormat(texture)) - 1) &
	     (SceUInt32)data) != 0)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	texture->controlWords[2] = ((SceUInt32)data & GXM_TEX2_DATA) |
				   (texture->controlWords[2] & GXM_TEX2_LOD_MIN_HIGH);
	return 0;
}

void *sceGxmTextureGetData(const SceGxmTexture *texture)
{
	return (void *)(texture->controlWords[2] & GXM_TEX2_DATA);
}

int sceGxmTextureSetPalette(SceGxmTexture *texture, const void *paletteData)
{
	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (((SceUInt32)paletteData & 0x3f) != 0)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	/* a palette goes with a palettized format, and only with one */
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_PALETTE_OPTIONAL)) {
		if (!gxmTextureFormatIsPalettized(gxmTextureGetFormat(texture))) {
			if (paletteData != NULL)
				return SCE_GXM_ERROR_INVALID_POINTER;
		} else if (paletteData == NULL) {
			return SCE_GXM_ERROR_INVALID_POINTER;
		}
	}
	texture->controlWords[3] = (texture->controlWords[3] & ~GXM_TEX3_PALETTE) |
				   (SceUInt32)paletteData >> 6;
	return 0;
}

void *sceGxmTextureGetPalette(const SceGxmTexture *texture)
{
	return (void *)(texture->controlWords[3] << 6);
}

int sceGxmTextureSetGammaMode(SceGxmTexture *texture, SceUInt32 gammaMode)
{
	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (gammaMode != 0 && !gxmTextureFormatIsGammaCapable(gxmTextureGetFormat(texture)))
		return SCE_GXM_ERROR_INVALID_VALUE;
	texture->controlWords[0] = (gammaMode & GXM_TEX0_GAMMA_MODE) |
				   (texture->controlWords[0] & ~GXM_TEX0_GAMMA_MODE);
	return 0;
}

SceUInt32 sceGxmTextureGetGammaMode(const SceGxmTexture *texture)
{
	return texture->controlWords[0] & GXM_TEX0_GAMMA_MODE;
}

int sceGxmTextureSetNormalizeMode(SceGxmTexture *texture, SceUInt32 normalizeMode)
{
	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	texture->controlWords[3] = (normalizeMode & GXM_TEX3_NORMALIZE_MODE) |
				   (texture->controlWords[3] & ~GXM_TEX3_NORMALIZE_MODE);
	return 0;
}

SceUInt32 sceGxmTextureGetNormalizeMode(const SceGxmTexture *texture)
{
	return texture->controlWords[3] & GXM_TEX3_NORMALIZE_MODE;
}

#define TYPE_INDEX(type)	((type) >> 29)

int sceGxmTextureValidate(const SceGxmTexture *texture)
{
	SceUInt32 word0, word1, word2, word3, type, uMode, vMode, gammaMode, mipField;
	SceUInt32 width, height, reserved0, reserved1;
	SceGxmTextureFormat format;
	_Bool compressed, palettized, yuv, yuvPlanar, yuv422;
	_Bool uBorder, vBorder, arbitrary;

	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;

	word0 = texture->controlWords[0];
	type = TYPE_INDEX(texture->controlWords[1]);
	format = gxmTextureGetFormat(texture);
	uMode = (word0 & GXM_TEX0_UADDR_MODE) >> GXM_TEX0_UADDR_MODE_SHIFT;
	vMode = (word0 & GXM_TEX0_VADDR_MODE) >> GXM_TEX0_VADDR_MODE_SHIFT;
	gammaMode = (word0 & GXM_TEX0_GAMMA_MODE) >> 27;
	mipField = (word0 & GXM_TEX0_MIP_COUNT) >> GXM_TEX0_MIP_COUNT_SHIFT;
	compressed = gxmTextureFormatIsCompressed(format);
	palettized = gxmTextureFormatIsPalettized(format);
	yuv = gxmTextureFormatIsYuv(format);
	yuvPlanar = gxmTextureFormatIsYuvPlanar(format);
	yuv422 = gxmTextureFormatIsYuv422(format);
	word3 = texture->controlWords[3];
	word2 = texture->controlWords[2];
	uBorder = uMode - SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER < 4;
	vBorder = vMode - SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER < 4;

	/* type 1 does not exist */
	if (type != TYPE_INDEX(SCE_GXM_TEXTURE_SWIZZLED) &&
	    type - TYPE_INDEX(SCE_GXM_TEXTURE_CUBE) > 5)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (!gxmTextureFormatIsSupported(format))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (!gxmTextureFormatGetSwizzleComponents(format))
		return SCE_GXM_ERROR_INVALID_VALUE;

	arbitrary = (type & 5) != 0;
	if (!arbitrary) {
		if ((texture->controlWords[1] & GXM_TEX1_WIDTH_LOG2) >> GXM_TEX1_WIDTH_LOG2_SHIFT > 12)
			return SCE_GXM_ERROR_INVALID_VALUE;
		if ((texture->controlWords[1] & GXM_TEX1_HEIGHT_LOG2) > 12)
			return SCE_GXM_ERROR_INVALID_VALUE;
	}
	/* a field of 15 is a count of 0 */
	if (type != TYPE_INDEX(SCE_GXM_TEXTURE_LINEAR_STRIDED) &&
	    mipField > GXM_TEXTURE_MAX_MIP_COUNT - 1 && mipField != 0xf)
		return SCE_GXM_ERROR_INVALID_VALUE;

	if (compressed &&
	    (type == TYPE_INDEX(SCE_GXM_TEXTURE_LINEAR) ||
	     type == TYPE_INDEX(SCE_GXM_TEXTURE_LINEAR_STRIDED) ||
	     type == TYPE_INDEX(SCE_GXM_TEXTURE_TILED)))
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (yuvPlanar) {
		if (gxmGlobals.compatFlags & GXM_COMPAT_YUV_TILED) {
			if (type - TYPE_INDEX(SCE_GXM_TEXTURE_LINEAR) > 1)
				return SCE_GXM_ERROR_UNSUPPORTED;
		} else if (type != TYPE_INDEX(SCE_GXM_TEXTURE_LINEAR)) {
			return SCE_GXM_ERROR_UNSUPPORTED;
		}
	}
	if (yuv422 &&
	    type != TYPE_INDEX(SCE_GXM_TEXTURE_LINEAR) &&
	    type != TYPE_INDEX(SCE_GXM_TEXTURE_LINEAR_STRIDED) &&
	    type != TYPE_INDEX(SCE_GXM_TEXTURE_TILED))
		return SCE_GXM_ERROR_UNSUPPORTED;

	/* border modes go on both axes or neither */
	if (uBorder != vBorder)
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (uBorder) {
		if (type != TYPE_INDEX(SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY) &&
		    type != TYPE_INDEX(SCE_GXM_TEXTURE_SWIZZLED))
			return SCE_GXM_ERROR_UNSUPPORTED;
		if (palettized || compressed)
			return SCE_GXM_ERROR_UNSUPPORTED;
		if (yuv)
			return SCE_GXM_ERROR_UNSUPPORTED;
	}

	/* modes: 0 off, 1 R, 3 GR */
	if (gammaMode > 1 && gammaMode != 3)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (!gxmTextureFormatIsGammaCapable(format) && gammaMode != 0)
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (gxmTextureFormatIs24Bit(format) && type == TYPE_INDEX(SCE_GXM_TEXTURE_LINEAR_STRIDED))
		return SCE_GXM_ERROR_UNSUPPORTED;

	/* cube maps and linear strided textures clamp; mirroring is for swizzled ones */
	if (type == TYPE_INDEX(SCE_GXM_TEXTURE_CUBE) ||
	    type == TYPE_INDEX(SCE_GXM_TEXTURE_LINEAR_STRIDED) ||
	    type == TYPE_INDEX(SCE_GXM_TEXTURE_CUBE_ARBITRARY)) {
		if (uMode != SCE_GXM_TEXTURE_ADDR_CLAMP || vMode != SCE_GXM_TEXTURE_ADDR_CLAMP)
			return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if ((uMode == SCE_GXM_TEXTURE_ADDR_MIRROR || vMode == SCE_GXM_TEXTURE_ADDR_MIRROR) &&
	    type != TYPE_INDEX(SCE_GXM_TEXTURE_SWIZZLED))
		return SCE_GXM_ERROR_UNSUPPORTED;

	if ((word2 & GXM_TEX2_DATA) == 0)
		return SCE_GXM_ERROR_INVALID_TEXTURE_DATA_POINTER;
	if (((gxmTextureFormatGetAlignment(format) - 1) & word2 & GXM_TEX2_DATA) != 0)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (!palettized) {
		if (!(gxmGlobals.compatFlags & GXM_COMPAT_PALETTE_OPTIONAL) && (word3 << 6) != 0)
			return SCE_GXM_ERROR_INVALID_TEXTURE_PALETTE_POINTER;
	} else if ((word3 << 6) == 0) {
		return SCE_GXM_ERROR_INVALID_TEXTURE_PALETTE_POINTER;
	}

	/* fields the type leaves unused must be zero */
	if (type == TYPE_INDEX(SCE_GXM_TEXTURE_TILED)) {
		word1 = texture->controlWords[1];
		if ((word1 & GXM_TEXTURE_TYPE_ARBITRARY_SIZE) == 0) {
			width = 1 << ((word1 & GXM_TEX1_WIDTH_LOG2) >> GXM_TEX1_WIDTH_LOG2_SHIFT);
			height = 1 << (word1 & GXM_TEX1_HEIGHT_LOG2);
		} else {
			width = ((word1 & GXM_TEX1_WIDTH) >> GXM_TEX1_WIDTH_SHIFT) + 1;
			height = (word1 & GXM_TEX1_HEIGHT) + 1;
		}
		if (mipField == 0xf)
			mipField = 0;
		if (width >> mipField < GXM_TEXTURE_TILE_SIZE || height >> mipField < GXM_TEXTURE_TILE_SIZE)
			return SCE_GXM_ERROR_UNSUPPORTED;
		reserved0 = 0x6001e807;
	} else if (type == TYPE_INDEX(SCE_GXM_TEXTURE_LINEAR_STRIDED)) {
		reserved0 = 0x6001e001;
	} else {
		reserved0 = 0x6001e807;
	}
	reserved1 = arbitrary ? 0 : 0x00f0fff0;
	if ((reserved0 & texture->controlWords[0]) != 0)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if ((reserved1 & texture->controlWords[1]) != 0)
		return SCE_GXM_ERROR_INVALID_VALUE;
	return 0;
}
