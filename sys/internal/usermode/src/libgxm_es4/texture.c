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

/* the Init functions' messages; a few of Sony's name another parameter or function */
#define GXM_TEXTURE_INIT_ERROR_FORMAT(line, function)						\
	GXM_DBG_ERROR(line, "Parameter texFormat to function " #function " has value %08x, "	\
		      "which is not a valid texture format.", texFormat)
#define GXM_TEXTURE_INIT_ERROR_DATA(line, function)						\
	GXM_DBG_ERROR(line, "Parameter data to function " #function " has value %p, "		\
		      "which is not aligned to %d.", data, gxmTextureFormatGetAlignment(texFormat))
#define GXM_TEXTURE_INIT_ERROR_POW2(line, function, parameter)					\
	GXM_DBG_ERROR(line, "Parameter " #parameter " to function " #function " has value %u, "	\
		      "which is not a power of 2.", parameter)
#define GXM_TEXTURE_INIT_ERROR_SIZE(line, function, name, value)				\
	GXM_DBG_ERROR(line, "Parameter " #name " to function " #function " has value %u, "	\
		      "which is not in the range [1, %u].", value, GXM_TEXTURE_MAX_SIZE)
#define GXM_TEXTURE_INIT_ERROR_MIPS(line, function)						\
	GXM_DBG_ERROR(line, "Parameter mipCount to function " #function " has value %u, "	\
		      "which is not in the range [0, %u].", mipCount, GXM_TEXTURE_MAX_MIP_COUNT)
#define GXM_TEXTURE_INIT_ERROR_YUV(line, function)						\
	GXM_DBG_ERROR(line, "Parameter texFormat to function " #function " cannot be based on a "	\
		      "YUV texture format.")
#if SCE_DBG_ASSERTS_ENABLED
#define GXM_TEXTURE_INIT_ERROR_NAMED(line, function, text)					\
	do {											\
		const char *name = gxmTextureFormatGetName(texFormat);				\
		const char *swizzle = gxmTextureFormatGetSwizzleName(texFormat);		\
		GXM_DBG_ERROR(line, "Parameter texFormat [%s|%s] to function " #function " " text,	\
			      name, swizzle);							\
	} while (0)
#else
#define GXM_TEXTURE_INIT_ERROR_NAMED(line, function, text)	((void)0)
#endif

int sceGxmTextureInitSwizzled(SceGxmTexture *texture, const void *data,
			      SceGxmTextureFormat texFormat, SceUInt32 width,
			      SceUInt32 height, SceUInt32 mipCount)
{
	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(44, sceGxmTextureInitSwizzled, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (!gxmTextureFormatIsSupported(texFormat)) {
		GXM_TEXTURE_INIT_ERROR_FORMAT(49, sceGxmTextureInitSwizzled);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (((SceUInt32)data & (gxmTextureFormatGetAlignment(texFormat) - 1)) != 0) {
		GXM_TEXTURE_INIT_ERROR_DATA(55, sceGxmTextureInitSwizzled);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	if (!gxmIsPow2(width)) {
		GXM_TEXTURE_INIT_ERROR_POW2(60, sceGxmTextureInitSwizzled, width);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	if (!gxmIsPow2(height)) {
		GXM_TEXTURE_INIT_ERROR_POW2(65, sceGxmTextureInitSwizzled, height);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	if (width - 1 >= GXM_TEXTURE_MAX_SIZE) {
		GXM_TEXTURE_INIT_ERROR_SIZE(71, sceGxmTextureInitSwizzled, width, width);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (height - 1 >= GXM_TEXTURE_MAX_SIZE) {
		GXM_TEXTURE_INIT_ERROR_SIZE(77, sceGxmTextureInitSwizzled, height, height);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (mipCount > GXM_TEXTURE_MAX_MIP_COUNT) {
		GXM_TEXTURE_INIT_ERROR_MIPS(83, sceGxmTextureInitSwizzled);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (gxmTextureFormatIsYuv(texFormat)) {
		GXM_TEXTURE_INIT_ERROR_YUV(87, sceGxmTextureInitSwizzled);
		return SCE_GXM_ERROR_UNSUPPORTED;
	}

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
	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(153, sceGxmTextureInitSwizzledArbitrary, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (!gxmTextureFormatIsSupported(texFormat)) {
		GXM_TEXTURE_INIT_ERROR_FORMAT(158, sceGxmTextureInitSwizzledArbitrary);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (((SceUInt32)data & (gxmTextureFormatGetAlignment(texFormat) - 1)) != 0) {
		GXM_TEXTURE_INIT_ERROR_DATA(164, sceGxmTextureInitSwizzledArbitrary);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	if (width - 1 >= GXM_TEXTURE_MAX_SIZE) {
		GXM_TEXTURE_INIT_ERROR_SIZE(170, sceGxmTextureInitSwizzledArbitrary, width, width);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (height - 1 >= GXM_TEXTURE_MAX_SIZE) {
		GXM_TEXTURE_INIT_ERROR_SIZE(176, sceGxmTextureInitSwizzledArbitrary, height, height);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (mipCount > GXM_TEXTURE_MAX_MIP_COUNT) {
		GXM_TEXTURE_INIT_ERROR_MIPS(182, sceGxmTextureInitSwizzledArbitrary);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (gxmTextureFormatIsYuv(texFormat)) {
		GXM_TEXTURE_INIT_ERROR_YUV(186, sceGxmTextureInitSwizzledArbitrary);
		return SCE_GXM_ERROR_UNSUPPORTED;
	}

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
	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(248, sceGxmTextureInitLinear, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (!gxmTextureFormatIsSupported(texFormat)) {
		GXM_TEXTURE_INIT_ERROR_FORMAT(253, sceGxmTextureInitLinear);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (gxmTextureFormatIsCompressed(texFormat)) {
		GXM_TEXTURE_INIT_ERROR_NAMED(258, sceGxmTextureInitLinear,
					     "cannot be based on a block compressed texture format.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (((SceUInt32)data & (gxmTextureFormatGetAlignment(texFormat) - 1)) != 0) {
		GXM_TEXTURE_INIT_ERROR_DATA(264, sceGxmTextureInitLinear);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	if (width - 1 >= GXM_TEXTURE_MAX_SIZE) {
		GXM_TEXTURE_INIT_ERROR_SIZE(270, sceGxmTextureInitLinear, width, width);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (height - 1 >= GXM_TEXTURE_MAX_SIZE) {
		GXM_TEXTURE_INIT_ERROR_SIZE(276, sceGxmTextureInitLinear, height, height);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (mipCount > GXM_TEXTURE_MAX_MIP_COUNT) {
		GXM_TEXTURE_INIT_ERROR_MIPS(282, sceGxmTextureInitLinear);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	/* a single-level three-plane YUV texture needs a width of whole 16s */
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_YUV_TILED) &&
	    (texFormat & 0x9f000000) == SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3 &&
	    mipCount == 0 && (width & 0xf) != 0) {
		GXM_DBG_ERROR(292, "Parameter width to function sceGxmTextureInitLinear has value %u, which "
			      "must be aligned to %d when parameter texFormat is based on "
			      "SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3 and parameter mipCount is 0.", width, 16);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}

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

	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(356, sceGxmTextureInitTiled, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (!gxmTextureFormatIsSupported(texFormat)) {
		GXM_TEXTURE_INIT_ERROR_FORMAT(361, sceGxmTextureInitTiled);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (gxmTextureFormatIsCompressed(texFormat)) {
		GXM_TEXTURE_INIT_ERROR_NAMED(366, sceGxmTextureInitTiled,
					     "cannot be based on a block compressed texture format.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_YUV_TILED) &&
	    gxmTextureFormatIsYuvPlanar(texFormat)) {
		GXM_TEXTURE_INIT_ERROR_NAMED(373, sceGxmTextureInitTiled,
					     "cannot be based on a planar YUV texture format.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (((SceUInt32)data & (gxmTextureFormatGetAlignment(texFormat) - 1)) != 0) {
		GXM_TEXTURE_INIT_ERROR_DATA(380, sceGxmTextureInitTiled);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	if (width - GXM_TEXTURE_TILE_SIZE > GXM_TEXTURE_MAX_SIZE - GXM_TEXTURE_TILE_SIZE) {
		GXM_DBG_ERROR(387, "Parameter width to function sceGxmTextureInitTiled has value %u, which "
			      "is not in the range [%u, %u].", width, GXM_TEXTURE_TILE_SIZE, GXM_TEXTURE_MAX_SIZE);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (height - GXM_TEXTURE_TILE_SIZE > GXM_TEXTURE_MAX_SIZE - GXM_TEXTURE_TILE_SIZE) {
		GXM_DBG_ERROR(394, "Parameter height to function sceGxmTextureInitTiled has value %u, which "
			      "is not in the range [%u, %u].", height, GXM_TEXTURE_TILE_SIZE, GXM_TEXTURE_MAX_SIZE);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (mipCount > GXM_TEXTURE_MAX_MIP_COUNT) {
		GXM_TEXTURE_INIT_ERROR_MIPS(400, sceGxmTextureInitTiled);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}

	/* the smallest level must still cover a whole tile */
	if (mipCount == 0) {
		lastLevel = 0;
	} else {
		lastLevel = mipCount - 1;
		if (width >> lastLevel < GXM_TEXTURE_TILE_SIZE)
			goto tooSmall;
	}
	if (height >> lastLevel < GXM_TEXTURE_TILE_SIZE)
		goto tooSmall;

	texture->controlWords[2] = (SceUInt32)data & GXM_TEX2_DATA;
	texture->controlWords[0] = gxmTextureWord0(texFormat, mipCount);
	texture->controlWords[1] = (height - 1) | SCE_GXM_TEXTURE_TILED |
				   (width - 1) << GXM_TEX1_WIDTH_SHIFT |
				   (texFormat & GXM_FORMAT_BASE);
	texture->controlWords[3] = gxmTextureWord3(texFormat);
	return 0;

tooSmall:
	GXM_DBG_ERROR(407, "Parameter mipCount to function sceGxmTextureInitTiled results in a mip level "
		      "that has dimension(s) smaller than %ux%u", GXM_TEXTURE_TILE_SIZE, GXM_TEXTURE_TILE_SIZE);
	return SCE_GXM_ERROR_INVALID_VALUE;
}

int sceGxmTextureInitCube(SceGxmTexture *texture, const void *data,
			  SceGxmTextureFormat texFormat, SceUInt32 width,
			  SceUInt32 height, SceUInt32 mipCount)
{
	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(469, sceGxmTextureInitCube, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (!gxmTextureFormatIsSupported(texFormat)) {
		GXM_TEXTURE_INIT_ERROR_FORMAT(474, sceGxmTextureInitCube);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (((SceUInt32)data & (gxmTextureFormatGetAlignment(texFormat) - 1)) != 0) {
		GXM_TEXTURE_INIT_ERROR_DATA(480, sceGxmTextureInitCube);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	if (!gxmIsPow2(width)) {
		GXM_TEXTURE_INIT_ERROR_POW2(485, sceGxmTextureInitCube, width);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	if (!gxmIsPow2(height)) {
		GXM_TEXTURE_INIT_ERROR_POW2(490, sceGxmTextureInitCube, height);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	if (width - 1 >= GXM_TEXTURE_MAX_SIZE) {
		GXM_TEXTURE_INIT_ERROR_SIZE(496, sceGxmTextureInitCube, width, width);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (height - 1 >= GXM_TEXTURE_MAX_SIZE) {
		GXM_TEXTURE_INIT_ERROR_SIZE(502, sceGxmTextureInitCube, width, height);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (mipCount > GXM_TEXTURE_MAX_MIP_COUNT) {
		GXM_TEXTURE_INIT_ERROR_MIPS(508, sceGxmTextureInitCube);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (gxmTextureFormatIsYuv(texFormat)) {
		GXM_TEXTURE_INIT_ERROR_YUV(512, sceGxmTextureInitCube);
		return SCE_GXM_ERROR_UNSUPPORTED;
	}

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
	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(577, sceGxmTextureInitCubeArbitrary, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (!gxmTextureFormatIsSupported(texFormat)) {
		GXM_TEXTURE_INIT_ERROR_FORMAT(582, sceGxmTextureInitCubeArbitrary);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (((SceUInt32)data & (gxmTextureFormatGetAlignment(texFormat) - 1)) != 0) {
		GXM_TEXTURE_INIT_ERROR_DATA(588, sceGxmTextureInitCubeArbitrary);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	if (width - 1 >= GXM_TEXTURE_MAX_SIZE) {
		GXM_TEXTURE_INIT_ERROR_SIZE(594, sceGxmTextureInitCube, width, width);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (height - 1 >= GXM_TEXTURE_MAX_SIZE) {
		GXM_TEXTURE_INIT_ERROR_SIZE(600, sceGxmTextureInitCubeArbitrary, width, height);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (mipCount > GXM_TEXTURE_MAX_MIP_COUNT) {
		GXM_TEXTURE_INIT_ERROR_MIPS(606, sceGxmTextureInitCubeArbitrary);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (gxmTextureFormatIsYuv(texFormat)) {
		GXM_TEXTURE_INIT_ERROR_YUV(610, sceGxmTextureInitCubeArbitrary);
		return SCE_GXM_ERROR_UNSUPPORTED;
	}

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
	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(671, sceGxmTextureInitLinearStrided, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (!gxmTextureFormatIsSupported(texFormat)) {
		GXM_TEXTURE_INIT_ERROR_FORMAT(676, sceGxmTextureInitLinearStrided);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (gxmTextureFormatIsCompressed(texFormat)) {
		GXM_TEXTURE_INIT_ERROR_NAMED(681, sceGxmTextureInitLinearStrided,
					     "cannot be based on a block compressed texture format.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (((SceUInt32)data & (gxmTextureFormatGetAlignment(texFormat) - 1)) != 0) {
		GXM_DBG_ERROR(687, "Parameter data to function sceGxmTextureInitLinearStrided has value %p, "
			      "is not aligned to %d.", data, gxmTextureFormatGetAlignment(texFormat));
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	if (width - 1 >= GXM_TEXTURE_MAX_SIZE) {
		GXM_TEXTURE_INIT_ERROR_SIZE(693, sceGxmTextureInitLinearStrided, width, width);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (height - 1 >= GXM_TEXTURE_MAX_SIZE) {
		GXM_TEXTURE_INIT_ERROR_SIZE(699, sceGxmTextureInitLinearStrided, height, height);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if ((byteStride & 3) != 0) {
		GXM_DBG_ERROR(705, "Parameter byteStride to function sceGxmTextureInitLinearStrided has value "
			      "%u, is not aligned to %u.", byteStride, 4);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	if (byteStride - 4 > GXM_TEXTURE_MAX_STRIDE - 4) {
		GXM_DBG_ERROR(712, "Parameter byteStride to function sceGxmTextureInitLinearStrided has value "
			      "%u, which is not in the range [%d, %d].", byteStride, 4, GXM_TEXTURE_MAX_STRIDE);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (gxmTextureFormatIs24Bit(texFormat)) {
		GXM_TEXTURE_INIT_ERROR_NAMED(717, sceGxmTextureInitLinearStrided,
					     "cannot be based on a 24-bit texture format.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (gxmTextureFormatIsYuvPlanar(texFormat)) {
		GXM_TEXTURE_INIT_ERROR_NAMED(722, sceGxmTextureInitLinearStrided,
					     "cannot be based on a planar YUV texture format.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}

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
	GXM_DBG_ASSERT(774, texture);
	return gxmTextureGetType(texture);
}

int sceGxmTextureSetMinFilter(SceGxmTexture *texture, SceUInt32 minFilter)
{
	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(787, sceGxmTextureSetMinFilter, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (gxmTextureGetType(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
		GXM_DBG_ERROR(791, "The function sceGxmTextureSetMinFilter cannot be called on a texture of type "
			      "SCE_GXM_TEXTURE_LINEAR_STRIDED.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	texture->controlWords[0] = ((minFilter << GXM_TEX0_MIN_FILTER_SHIFT) & GXM_TEX0_MIN_FILTER) |
				   (texture->controlWords[0] & ~GXM_TEX0_MIN_FILTER);
	return 0;
}

/* a linear strided texture uses its mag filter for both */
SceUInt32 sceGxmTextureGetMinFilter(const SceGxmTexture *texture)
{
	GXM_DBG_ASSERT(805, texture);
	if (sceGxmTextureGetType(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return (texture->controlWords[0] & GXM_TEX0_MAG_FILTER) >> GXM_TEX0_MAG_FILTER_SHIFT;
	return (texture->controlWords[0] & GXM_TEX0_MIN_FILTER) >> GXM_TEX0_MIN_FILTER_SHIFT;
}

int sceGxmTextureSetMagFilter(SceGxmTexture *texture, SceUInt32 magFilter)
{
	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(824, sceGxmTextureSetMagFilter, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	texture->controlWords[0] = ((magFilter << GXM_TEX0_MAG_FILTER_SHIFT) & GXM_TEX0_MAG_FILTER) |
				   (texture->controlWords[0] & ~GXM_TEX0_MAG_FILTER);
	return 0;
}

SceUInt32 sceGxmTextureGetMagFilter(const SceGxmTexture *texture)
{
	GXM_DBG_ASSERT(838, texture);
	return (texture->controlWords[0] & GXM_TEX0_MAG_FILTER) >> GXM_TEX0_MAG_FILTER_SHIFT;
}

int sceGxmTextureSetMipFilter(SceGxmTexture *texture, SceUInt32 mipFilter)
{
	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(851, sceGxmTextureSetMipFilter, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (gxmTextureGetType(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
		GXM_DBG_ERROR(855, "The function sceGxmTextureSetMipFilter cannot be called on a texture of type "
			      "SCE_GXM_TEXTURE_LINEAR_STRIDED.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	texture->controlWords[0] = (mipFilter & GXM_TEX0_MIP_FILTER) |
				   (texture->controlWords[0] & ~GXM_TEX0_MIP_FILTER);
	return 0;
}

SceUInt32 sceGxmTextureGetMipFilter(const SceGxmTexture *texture)
{
	GXM_DBG_ASSERT(869, texture);
	if (sceGxmTextureGetType(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return 0;
	return texture->controlWords[0] & GXM_TEX0_MIP_FILTER;
}

/*
 * The border modes need a plain format (not palettized, compressed or
 * YUV) on a swizzled texture; mirroring needs a swizzled texture too.
 * A linear strided texture always clamps. The U and V setters print
 * their own messages, under the name without "Safe".
 */
#define GXM_TEXTURE_ADDR_ERROR(vaddr, uLine, uText, vLine, vText)				\
	((vaddr) ? GXM_DBG_ERROR(vLine, "Parameter addrMode to sceGxmTextureSetVAddrMode " vText)	\
		 : GXM_DBG_ERROR(uLine, "Parameter addrMode to sceGxmTextureSetUAddrMode " uText))
#define GXM_TEXTURE_ADDR_BORDER_TYPE							\
	"can only refer to a border mode for textures with a type of "			\
	"SCE_GXM_TEXTURE_SWIZZLED or SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY"
#define GXM_TEXTURE_ADDR_BORDER_FORMAT							\
	"cannot refer to a border mode when the texture is based on a palettized, "	\
	"block compressed, or YUV format."
#define GXM_TEXTURE_ADDR_MIRROR_TYPE							\
	"is SCE_GXM_TEXTURE_ADDR_MIRROR but the type is not SCE_GXM_TEXTURE_SWIZZLED."

static inline int gxmTextureSetAddrModeSafe(SceGxmTexture *texture, SceUInt32 mode,
					    unsigned int shift, SceUInt32 mask, SceBool vaddr)
{
	SceUInt32 type;
	SceGxmTextureFormat format;

	if (texture == NULL) {
		if (vaddr)
			GXM_DBG_ERROR_NULL(1012, sceGxmTextureSetVAddrMode, texture);
		else
			GXM_DBG_ERROR_NULL(886, sceGxmTextureSetUAddrMode, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	type = gxmTextureGetType(texture);
	if (type == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
		if (mode == SCE_GXM_TEXTURE_ADDR_CLAMP)
			return 0;
		GXM_TEXTURE_ADDR_ERROR(vaddr,
			891, "must be SCE_GXM_TEXTURE_ADDR_CLAMP for textures of type SCE_GXM_TEXTURE_LINEAR_STRIDED.",
			1017, "must be SCE_GXM_TEXTURE_ADDR_CLAMP for textures of type SCE_GXM_TEXTURE_LINEAR_STRIDED.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (mode - SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER < 4) {
		format = gxmTextureGetFormat(texture);
		if (gxmTextureFormatIsPalettized(format) ||
		    gxmTextureFormatIsCompressed(format) ||
		    gxmTextureFormatIsYuv(format)) {
			GXM_TEXTURE_ADDR_ERROR(vaddr, 905, GXM_TEXTURE_ADDR_BORDER_FORMAT,
					       1031, GXM_TEXTURE_ADDR_BORDER_FORMAT);
			return SCE_GXM_ERROR_UNSUPPORTED;
		}
		if (type != SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY && type != SCE_GXM_TEXTURE_SWIZZLED) {
			GXM_TEXTURE_ADDR_ERROR(vaddr, 910, GXM_TEXTURE_ADDR_BORDER_TYPE ".",
					       1036, GXM_TEXTURE_ADDR_BORDER_TYPE);
			return SCE_GXM_ERROR_UNSUPPORTED;
		}
	} else if (mode == SCE_GXM_TEXTURE_ADDR_MIRROR) {
		if (type != SCE_GXM_TEXTURE_SWIZZLED) {
			GXM_TEXTURE_ADDR_ERROR(vaddr, 918, GXM_TEXTURE_ADDR_MIRROR_TYPE,
					       1044, GXM_TEXTURE_ADDR_MIRROR_TYPE);
			return SCE_GXM_ERROR_UNSUPPORTED;
		}
	}
	texture->controlWords[0] = ((mode << shift) & mask) | (texture->controlWords[0] & ~mask);
	return 0;
}

int sceGxmTextureSetUAddrModeSafe(SceGxmTexture *texture, SceUInt32 mode)
{
	return gxmTextureSetAddrModeSafe(texture, mode, GXM_TEX0_UADDR_MODE_SHIFT, GXM_TEX0_UADDR_MODE, SCE_FALSE);
}

/* cube maps clamp, whatever the caller asks for */
static inline int gxmTextureIsCube(SceUInt32 word1)
{
	return (word1 & 0x60000000) == SCE_GXM_TEXTURE_CUBE ||
	       (word1 & GXM_TEX1_TYPE) == SCE_GXM_TEXTURE_CUBE_ARBITRARY;
}

static inline int gxmTextureSetAddrMode(SceGxmTexture *texture, SceUInt32 mode,
					unsigned int shift, SceUInt32 mask, SceBool vaddr)
{
	SceUInt32 word1, type;
	SceGxmTextureFormat format;

	if (texture == NULL) {
		if (vaddr)
			GXM_DBG_ERROR_NULL(1064, sceGxmTextureSetVAddrMode, texture);
		else
			GXM_DBG_ERROR_NULL(938, sceGxmTextureSetUAddrMode, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	word1 = texture->controlWords[1];
	type = word1 & GXM_TEX1_TYPE;
	if (gxmTextureIsCube(word1)) {
		if (mode != SCE_GXM_TEXTURE_ADDR_CLAMP) {
			GXM_TEXTURE_ADDR_ERROR(vaddr,
				947, "must be SCE_GXM_TEXTURE_ADDR_CLAMP for textures of type SCE_GXM_TEXTURE_LINEAR_STRIDED, "
				     "SCE_GXM_TEXTURE_CUBE and SCE_GXM_TEXTURE_CUBE_ARBITRARY.",
				1073, "must be SCE_GXM_TEXTURE_ADDR_CLAMP for textures of type SCE_GXM_TEXTURE_LINEAR_STRIDED, "
				      "SCE_GXM_TEXTURE_CUBE and SCE_GXM_TEXTURE_CUBE_ARBITRARY.");
			return SCE_GXM_ERROR_UNSUPPORTED;
		}
	} else {
		if (mode - SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER < 4) {
			format = (word1 & GXM_FORMAT_BASE) |
				 (texture->controlWords[0] & GXM_FORMAT_BASE_EXT) |
				 (((const SceUInt16 *)texture->controlWords)[7] & GXM_FORMAT_SWIZZLE);
#if SCE_DBG_ASSERTS_ENABLED
			if (gxmTextureFormatIsPalettized(format) ||
			    gxmTextureFormatIsCompressed(format) ||
			    gxmTextureFormatIsYuv(format)) {
				GXM_TEXTURE_ADDR_ERROR(vaddr, 956, GXM_TEXTURE_ADDR_BORDER_FORMAT,
						       1082, GXM_TEXTURE_ADDR_BORDER_FORMAT);
				return SCE_GXM_ERROR_UNSUPPORTED;
			}
#else
			if (gxmTextureFormatIsPalettized(format))
				return SCE_GXM_ERROR_UNSUPPORTED;
			if (gxmTextureFormatIsCompressed(format))
				return SCE_GXM_ERROR_UNSUPPORTED;
			if (gxmTextureFormatIsYuv(format))
				return SCE_GXM_ERROR_UNSUPPORTED;
#endif
			if (type != SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY && type != SCE_GXM_TEXTURE_SWIZZLED) {
				GXM_TEXTURE_ADDR_ERROR(vaddr, 961, GXM_TEXTURE_ADDR_BORDER_TYPE ".",
						       1087, GXM_TEXTURE_ADDR_BORDER_TYPE);
				return SCE_GXM_ERROR_UNSUPPORTED;
			}
		}
		if (mode == SCE_GXM_TEXTURE_ADDR_MIRROR) {
			if (type != SCE_GXM_TEXTURE_SWIZZLED) {
				GXM_TEXTURE_ADDR_ERROR(vaddr, 969, GXM_TEXTURE_ADDR_MIRROR_TYPE,
						       1095, GXM_TEXTURE_ADDR_MIRROR_TYPE);
				return SCE_GXM_ERROR_UNSUPPORTED;
			}
		}
	}
	texture->controlWords[0] = ((mode << shift) & mask) | (texture->controlWords[0] & ~mask);
	return 0;
}

int sceGxmTextureSetUAddrMode(SceGxmTexture *texture, SceUInt32 mode)
{
	return gxmTextureSetAddrMode(texture, mode, GXM_TEX0_UADDR_MODE_SHIFT, GXM_TEX0_UADDR_MODE, SCE_FALSE);
}

SceUInt32 sceGxmTextureGetUAddrModeSafe(const SceGxmTexture *texture)
{
	GXM_DBG_ASSERT(985, texture);
	if (sceGxmTextureGetType(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return SCE_GXM_TEXTURE_ADDR_CLAMP;
	return (texture->controlWords[0] & GXM_TEX0_UADDR_MODE) >> GXM_TEX0_UADDR_MODE_SHIFT;
}

SceUInt32 sceGxmTextureGetUAddrMode(const SceGxmTexture *texture)
{
	GXM_DBG_ASSERT(998, texture);
	return (texture->controlWords[0] & GXM_TEX0_UADDR_MODE) >> GXM_TEX0_UADDR_MODE_SHIFT;
}

int sceGxmTextureSetVAddrModeSafe(SceGxmTexture *texture, SceUInt32 mode)
{
	return gxmTextureSetAddrModeSafe(texture, mode, GXM_TEX0_VADDR_MODE_SHIFT, GXM_TEX0_VADDR_MODE, SCE_TRUE);
}

int sceGxmTextureSetVAddrMode(SceGxmTexture *texture, SceUInt32 mode)
{
	return gxmTextureSetAddrMode(texture, mode, GXM_TEX0_VADDR_MODE_SHIFT, GXM_TEX0_VADDR_MODE, SCE_TRUE);
}

SceUInt32 sceGxmTextureGetVAddrModeSafe(const SceGxmTexture *texture)
{
	GXM_DBG_ASSERT(1111, texture);
	if (sceGxmTextureGetType(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return SCE_GXM_TEXTURE_ADDR_CLAMP;
	return (texture->controlWords[0] & GXM_TEX0_VADDR_MODE) >> GXM_TEX0_VADDR_MODE_SHIFT;
}

SceUInt32 sceGxmTextureGetVAddrMode(const SceGxmTexture *texture)
{
	GXM_DBG_ASSERT(1124, texture);
	return (texture->controlWords[0] & GXM_TEX0_VADDR_MODE) >> GXM_TEX0_VADDR_MODE_SHIFT;
}

#if SCE_DBG_ASSERTS_ENABLED
#define GXM_TEXTURE_SET_FORMAT_ERROR(line, text, ...)						\
	do {											\
		name = gxmTextureFormatGetName(texFormat);					\
		swizzle = gxmTextureFormatGetSwizzleName(texFormat);				\
		GXM_DBG_ERROR(line, "Parameter texFormat [%s|%s] to sceGxmTextureSetFormat " text,	\
			      name, swizzle, ##__VA_ARGS__);					\
	} while (0)
#else
#define GXM_TEXTURE_SET_FORMAT_ERROR(line, text, ...)	((void)0)
#endif

int sceGxmTextureSetFormat(SceGxmTexture *texture, SceGxmTextureFormat texFormat)
{
	SceGxmTextureFormat oldFormat;
	SceUInt32 word1, type;
#if SCE_DBG_ASSERTS_ENABLED
	const char *name, *swizzle, *oldName, *oldSwizzle;
	SceUInt32 oldSize, newSize;
#endif

	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(1135, sceGxmTextureSetFormat, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (!gxmTextureFormatIsSupported(texFormat)) {
		GXM_DBG_ERROR(1140, "Parameter texFormat to function sceGxmTextureSetFormat has value %08x, "
			      "which is not a valid texture format.", texFormat);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}

	/* the texel size cannot change */
	oldFormat = gxmTextureGetFormat(texture);
#if SCE_DBG_ASSERTS_ENABLED
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_FORMAT_BYTES)) {
		oldSize = gxmTextureFormatGetBitsPerPixel(oldFormat);
		newSize = gxmTextureFormatGetBitsPerPixel(texFormat);
		if (oldSize != newSize) {
			name = gxmTextureFormatGetName(texFormat);
			swizzle = gxmTextureFormatGetSwizzleName(texFormat);
			oldName = gxmTextureFormatGetName(oldFormat);
			oldSwizzle = gxmTextureFormatGetSwizzleName(oldFormat);
			GXM_DBG_ERROR(1164, "Parameter texFormat [%s|%s] to sceGxmTextureSetFormat has %u bits "
				      "per pixel, but existing format [%s|%s] has %u bits per pixel.",
				      name, swizzle, newSize, oldName, oldSwizzle, oldSize);
			return SCE_GXM_ERROR_UNSUPPORTED;
		}
	} else {
		oldSize = gxmTextureFormatGetBytesPerPixel(oldFormat);
		newSize = gxmTextureFormatGetBytesPerPixel(texFormat);
		if (oldSize != newSize) {
			name = gxmTextureFormatGetName(texFormat);
			swizzle = gxmTextureFormatGetSwizzleName(texFormat);
			oldName = gxmTextureFormatGetName(oldFormat);
			oldSwizzle = gxmTextureFormatGetSwizzleName(oldFormat);
			GXM_DBG_ERROR(1154, "Parameter texFormat [%s|%s] to sceGxmTextureSetFormat has %u bytes "
				      "per pixel, but existing format [%s|%s] has %u bytes per pixel.",
				      name, swizzle, newSize, oldName, oldSwizzle, oldSize);
			return SCE_GXM_ERROR_UNSUPPORTED;
		}
	}
#else
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_FORMAT_BYTES)) {
		if (gxmTextureFormatGetBitsPerPixel(oldFormat) !=
		    gxmTextureFormatGetBitsPerPixel(texFormat))
			return SCE_GXM_ERROR_UNSUPPORTED;
	} else {
		if (gxmTextureFormatGetBytesPerPixel(oldFormat) !=
		    gxmTextureFormatGetBytesPerPixel(texFormat))
			return SCE_GXM_ERROR_UNSUPPORTED;
	}
#endif
	if ((texture->controlWords[2] & GXM_TEX2_DATA &
	     (gxmTextureFormatGetAlignment(texFormat) - 1)) != 0) {
		GXM_TEXTURE_SET_FORMAT_ERROR(1172, "requires that the current data address is aligned to %d.",
					     gxmTextureFormatGetAlignment(texFormat));
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}

	word1 = texture->controlWords[1];
	type = word1 & GXM_TEX1_TYPE;
	if (gxmTextureFormatIsCompressed(texFormat) &&
	    (type == SCE_GXM_TEXTURE_LINEAR || (word1 & 0xa0000000) == SCE_GXM_TEXTURE_TILED)) {
		GXM_TEXTURE_SET_FORMAT_ERROR(1183, "is not supported for textures with a type of "
					     "SCE_GXM_TEXTURE_TILED, SCE_GXM_TEXTURE_LINEAR, or "
					     "SCE_GXM_TEXTURE_LINEAR_STRIDED.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (gxmGlobals.compatFlags & GXM_COMPAT_YUV_ANY_TYPE) {
		if (gxmTextureFormatIsYuv(texFormat) &&
		    (type == SCE_GXM_TEXTURE_CUBE || type == SCE_GXM_TEXTURE_CUBE_ARBITRARY)) {
			GXM_TEXTURE_SET_FORMAT_ERROR(1195, "is not supported for textures with a type of "
						     "SCE_GXM_TEXTURE_CUBE or SCE_GXM_TEXTURE_CUBE_ARBITRARY.");
			return SCE_GXM_ERROR_UNSUPPORTED;
		}
	} else {
		/* planar YUV is linear only, or tiled too for older SDKs */
#if SCE_DBG_ASSERTS_ENABLED
		if (gxmTextureFormatIsYuvPlanar(texFormat)) {
			if (gxmGlobals.compatFlags & GXM_COMPAT_YUV_TILED) {
				if (type != SCE_GXM_TEXTURE_TILED && type != SCE_GXM_TEXTURE_LINEAR) {
					GXM_TEXTURE_SET_FORMAT_ERROR(1207, "is only supported for textures with a type "
								     "of SCE_GXM_TEXTURE_LINEAR or SCE_GXM_TEXTURE_TILED.");
					return SCE_GXM_ERROR_UNSUPPORTED;
				}
			} else if (type != SCE_GXM_TEXTURE_LINEAR) {
				GXM_TEXTURE_SET_FORMAT_ERROR(1214, "is only supported for textures with a type of "
							     "SCE_GXM_TEXTURE_LINEAR.");
				return SCE_GXM_ERROR_UNSUPPORTED;
			}
		}
#else
		if (gxmTextureFormatIsYuvPlanar(texFormat) &&
		    ((gxmGlobals.compatFlags & GXM_COMPAT_YUV_TILED) ?
		     type != SCE_GXM_TEXTURE_TILED && type != SCE_GXM_TEXTURE_LINEAR :
		     type != SCE_GXM_TEXTURE_LINEAR))
			return SCE_GXM_ERROR_UNSUPPORTED;
#endif
		if (gxmTextureFormatIsYuv422(texFormat) &&
		    type != SCE_GXM_TEXTURE_LINEAR && (word1 & 0xa0000000) != SCE_GXM_TEXTURE_TILED) {
			GXM_TEXTURE_SET_FORMAT_ERROR(1224, "is only supported for textures with a type of "
						     "SCE_GXM_TEXTURE_LINEAR, SCE_GXM_TEXTURE_TILED, or "
						     "SCE_GXM_TEXTURE_LINEAR_STRIDED.");
			return SCE_GXM_ERROR_UNSUPPORTED;
		}
	}
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_24BIT_STRIDED) &&
	    gxmTextureFormatIs24Bit(texFormat) && type == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
		GXM_TEXTURE_SET_FORMAT_ERROR(1236, "is not supported for textures with a type of "
					     "SCE_GXM_TEXTURE_LINEAR_STRIDED.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}

	/* formats that cannot use the border address modes */
	if (gxmTextureFormatIsCompressed(texFormat) || gxmTextureFormatIsYuv(texFormat) ||
	    gxmTextureFormatIsPalettized(texFormat)) {
#if SCE_DBG_ASSERTS_ENABLED
		if (((texture->controlWords[0] & GXM_TEX0_UADDR_MODE) >> GXM_TEX0_UADDR_MODE_SHIFT) -
		    SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER < 4 ||
		    ((texture->controlWords[0] & GXM_TEX0_VADDR_MODE) >> GXM_TEX0_VADDR_MODE_SHIFT) -
		    SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER < 4) {
			GXM_TEXTURE_SET_FORMAT_ERROR(1246, "requires that the current U and V addressing modes "
						     "do not refer to a border mode.");
			return SCE_GXM_ERROR_UNSUPPORTED;
		}
#else
		if (((texture->controlWords[0] & GXM_TEX0_UADDR_MODE) >> GXM_TEX0_UADDR_MODE_SHIFT) -
		    SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER < 4)
			return SCE_GXM_ERROR_UNSUPPORTED;
		if (((texture->controlWords[0] & GXM_TEX0_VADDR_MODE) >> GXM_TEX0_VADDR_MODE_SHIFT) -
		    SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER < 4)
			return SCE_GXM_ERROR_UNSUPPORTED;
#endif
	}

	/* older SDKs tested the gamma capability the wrong way round */
	if (gxmGlobals.compatFlags & GXM_COMPAT_GAMMA_INVERTED) {
		if (gxmTextureFormatIsGammaCapable(texFormat) &&
		    (texture->controlWords[0] & GXM_TEX0_GAMMA_MODE) != 0) {
			GXM_TEXTURE_SET_FORMAT_ERROR(1257, "requires the current gamma mode to be "
						     "SCE_GXM_TEXTURE_GAMMA_NONE.");
			return SCE_GXM_ERROR_UNSUPPORTED;
		}
	} else {
		if (!gxmTextureFormatIsGammaCapable(texFormat) &&
		    (texture->controlWords[0] & GXM_TEX0_GAMMA_MODE) != 0) {
			GXM_TEXTURE_SET_FORMAT_ERROR(1266, "requires the current gamma mode to be "
						     "SCE_GXM_TEXTURE_GAMMA_NONE.");
			return SCE_GXM_ERROR_UNSUPPORTED;
		}
	}

	texture->controlWords[0] = (texture->controlWords[0] & ~GXM_FORMAT_BASE_EXT) |
				   (texFormat & GXM_FORMAT_BASE_EXT);
	texture->controlWords[3] = (texture->controlWords[3] & ~GXM_TEX3_SWIZZLE) |
				   (texFormat & GXM_FORMAT_SWIZZLE) << 16;
	texture->controlWords[1] = (texFormat & GXM_FORMAT_BASE) |
				   (texture->controlWords[1] & ~GXM_FORMAT_BASE);
	return 0;
}

GXM_DBG_NOINLINE SceGxmTextureFormat sceGxmTextureGetFormat(const SceGxmTexture *texture)
{
	GXM_DBG_ASSERT(1285, texture);
	return gxmTextureGetFormat(texture);
}

int sceGxmTextureSetLodBias(SceGxmTexture *texture, SceUInt32 bias)
{
	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(1299, sceGxmTextureSetLodBias, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (gxmTextureGetType(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
		GXM_DBG_ERROR(1303, "The function sceGxmTextureSetLodBias cannot be called on a texture of type "
			      "SCE_GXM_TEXTURE_LINEAR_STRIDED.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (bias >= GXM_TEXTURE_MAX_LOD_BIAS) {
		GXM_DBG_ERROR(1309, "The parameter bias to function sceGxmTextureSetLodBias has value "
			      "%u, which is not in the range [0, %d].", bias, GXM_TEXTURE_MAX_LOD_BIAS - 1);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	texture->controlWords[0] = (texture->controlWords[0] & ~GXM_TEX0_LOD_BIAS) |
				   bias << GXM_TEX0_LOD_BIAS_SHIFT;
	return 0;
}

SceUInt32 sceGxmTextureGetLodBias(const SceGxmTexture *texture)
{
	GXM_DBG_ASSERT(1321, texture);
	if (sceGxmTextureGetType(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return 0;
	return (texture->controlWords[0] & GXM_TEX0_LOD_BIAS) >> GXM_TEX0_LOD_BIAS_SHIFT;
}

/* the minimum LOD is split: bits 2..3 in word 2, bits 0..1 in word 3 */
int sceGxmTextureSetLodMin(SceGxmTexture *texture, SceUInt32 lodMin)
{
	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(1339, sceGxmTextureSetLodMin, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (gxmTextureGetType(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
		GXM_DBG_ERROR(1343, "The function sceGxmTextureSetLodMin cannot be called on a texture of type "
			      "SCE_GXM_TEXTURE_LINEAR_STRIDED.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (lodMin > GXM_TEXTURE_MAX_MIP_COUNT - 1) {
		GXM_DBG_ERROR(1349, "The parameter minLod to function sceGxmTextureSetLodMin has value "
			      "%u, which is not in the range [0, %d].", lodMin,
			      GXM_TEXTURE_MAX_MIP_COUNT - 1);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	texture->controlWords[2] = (texture->controlWords[2] & ~GXM_TEX2_LOD_MIN_HIGH) |
				   lodMin >> 2;
	texture->controlWords[3] = ((lodMin << GXM_TEX3_LOD_MIN_LOW_SHIFT) & GXM_TEX3_LOD_MIN_LOW) |
				   (texture->controlWords[3] & ~GXM_TEX3_LOD_MIN_LOW);
	return 0;
}

SceUInt32 sceGxmTextureGetLodMin(const SceGxmTexture *texture)
{
	GXM_DBG_ASSERT(1363, texture);
	if (sceGxmTextureGetType(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return 0;
	return (texture->controlWords[3] & GXM_TEX3_LOD_MIN_LOW) >> GXM_TEX3_LOD_MIN_LOW_SHIFT |
	       (texture->controlWords[2] & GXM_TEX2_LOD_MIN_HIGH) << 2;
}

int sceGxmTextureSetStride(SceGxmTexture *texture, SceUInt32 byteStride)
{
	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(1383, sceGxmTextureSetStride, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (gxmTextureGetType(texture) != SCE_GXM_TEXTURE_LINEAR_STRIDED) {
		GXM_DBG_ERROR(1387, "The function sceGxmTextureSetStride cannot be called on a texture "
			      "that does not have type SCE_GXM_TEXTURE_LINEAR_STRIDED.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if ((byteStride & 3) != 0) {
		GXM_DBG_ERROR(1393, "Parameter byteStride to function sceGxmTextureSetStride has value "
			      "%u, which is not aligned to %u.", byteStride, 4);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	if (byteStride - 4 > GXM_TEXTURE_MAX_STRIDE - 4) {
		GXM_DBG_ERROR(1400, "Parameter byteStride to function sceGxmTextureSetStride has value "
			      "%u, which is not in the range [%d, %d].", byteStride, 4,
			      GXM_TEXTURE_MAX_STRIDE);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	texture->controlWords[0] = gxmTextureStrideBits(byteStride) |
				   (texture->controlWords[0] &
				    ~(GXM_TEX0_STRIDE_LOW | GXM_TEX0_STRIDE | GXM_TEX0_STRIDE_EXT));
	return 0;
}

SceUInt32 sceGxmTextureGetStride(const SceGxmTexture *texture)
{
	SceUInt32 word0;

	GXM_DBG_ASSERT(1420, texture);
	if (sceGxmTextureGetType(texture) != SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return 0;
	word0 = texture->controlWords[0];
	return (((word0 & GXM_TEX0_STRIDE_EXT) >> 1 << 13 |
		 (word0 & GXM_TEX0_STRIDE) >> 17 << 3 |
		 (word0 & GXM_TEX0_STRIDE_LOW) >> 9) + 1) * 4;
}

int _sceGxmTextureSetWidth(SceGxmTexture *texture, SceUInt32 width)
{
	SceUInt32 word1;

	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(1443, sceGxmTextureSetWidth, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (width - 1 >= GXM_TEXTURE_MAX_SIZE) {
		GXM_DBG_ERROR(1449, "Parameter width to function sceGxmTextureSetWidth has value %u, "
			      "which is not in the range [1, %u].", width, GXM_TEXTURE_MAX_SIZE);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	word1 = texture->controlWords[1];
	if ((word1 & GXM_TEXTURE_TYPE_ARBITRARY_SIZE) != 0) {
		texture->controlWords[1] = (word1 & ~GXM_TEX1_WIDTH) |
					   (width - 1) << GXM_TEX1_WIDTH_SHIFT;
		return 0;
	}
	if (((width - 1) & width) != 0) {
		GXM_DBG_ERROR(1459, "Parameter width to function sceGxmTextureSetWidth has value %u, "
			      "which is not a power of 2, but the texture is swizzled.", width);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
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

	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(1485, sceGxmTextureSetWidth, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (width - 1 >= GXM_TEXTURE_MAX_SIZE) {
		GXM_DBG_ERROR(1491, "Parameter width to function sceGxmTextureSetWidth has value %u, "
			      "which is not in the range [1, %u].", width, GXM_TEXTURE_MAX_SIZE);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	word1 = texture->controlWords[1];
	type = word1 & GXM_TEX1_TYPE;

	if (!(gxmGlobals.compatFlags & GXM_COMPAT_YUV_TILED)) {
		/* a single-level three-plane YUV texture needs a width of whole 16s */
		singleLevel = gxmTextureIsSingleLevel(texture);
		if (((word1 & GXM_FORMAT_BASE) | (texture->controlWords[0] & GXM_FORMAT_BASE_EXT)) !=
		    SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3)
			singleLevel = 0;
		if (singleLevel && (width & 0xf) != 0) {
			GXM_DBG_ERROR(1505, "Parameter width to function sceGxmTextureSetWidth has value %u, "
				      "which must be aligned to %d when the texture is based on "
				      "SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3 and not using mip maps.", width, 16);
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
	}

	if (type == SCE_GXM_TEXTURE_TILED) {
		lastLevel = (((texture->controlWords[0] & GXM_TEX0_MIP_COUNT) >>
			      GXM_TEX0_MIP_COUNT_SHIFT) + 1) & 0xf;
		if (lastLevel != 0)
			lastLevel--;
		if (width >> lastLevel < GXM_TEXTURE_TILE_SIZE) {
			GXM_DBG_ERROR(1517, "Parameter width to function sceGxmTextureSetWidth results in a mip "
				      "level with width less than %u, but the texture is tiled",
				      GXM_TEXTURE_TILE_SIZE);
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
	} else if ((word1 & GXM_TEXTURE_TYPE_ARBITRARY_SIZE) == 0) {
		if (((width - 1) & width) != 0) {
			GXM_DBG_ERROR(1527, "Parameter width to function sceGxmTextureSetWidth has value %u, "
				      "which is not a power of 2, but the texture is swizzled.", width);
			return SCE_GXM_ERROR_INVALID_ALIGNMENT;
		}
		texture->controlWords[1] = (word1 & ~GXM_TEX1_WIDTH_LOG2) |
					   gxmLog2(width) << GXM_TEX1_WIDTH_LOG2_SHIFT;
		return 0;
	}
	texture->controlWords[1] = (word1 & ~GXM_TEX1_WIDTH) | (width - 1) << GXM_TEX1_WIDTH_SHIFT;
	return 0;
}

SceUInt32 sceGxmTextureGetWidth(const SceGxmTexture *texture)
{
	SceUInt32 word1;

	GXM_DBG_ASSERT(1550, texture);
	if ((sceGxmTextureGetType(texture) & GXM_TEXTURE_TYPE_ARBITRARY_SIZE) == 0) {
		word1 = texture->controlWords[1];
		return 1 << ((word1 & GXM_TEX1_WIDTH_LOG2) >> GXM_TEX1_WIDTH_LOG2_SHIFT);
	}
	word1 = texture->controlWords[1];
	return ((word1 & GXM_TEX1_WIDTH) >> GXM_TEX1_WIDTH_SHIFT) + 1;
}

int _sceGxmTextureSetHeight(SceGxmTexture *texture, SceUInt32 height)
{
	SceUInt32 word1;

	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(1572, sceGxmTextureSetHeight, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (height - 1 >= GXM_TEXTURE_MAX_SIZE) {
		GXM_DBG_ERROR(1578, "Parameter height to function sceGxmTextureSetHeight has value %u, "
			      "which is not in the range [1, %u].", height, GXM_TEXTURE_MAX_SIZE);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	word1 = texture->controlWords[1];
	if ((word1 & GXM_TEXTURE_TYPE_ARBITRARY_SIZE) != 0) {
		texture->controlWords[1] = (height - 1) | (word1 & ~GXM_TEX1_HEIGHT);
		return 0;
	}
	if (((height - 1) & height) != 0) {
		GXM_DBG_ERROR(1588, "Parameter height to function sceGxmTextureSetHeight has value %u, "
			      "which is not a power of 2, but the texture is swizzled.", height);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	texture->controlWords[1] = gxmLog2(height) | (word1 & ~GXM_TEX1_HEIGHT_LOG2);
	return 0;
}

int sceGxmTextureSetHeight(SceGxmTexture *texture, SceUInt32 height)
{
	SceUInt32 word1, lastLevel;

	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(1614, sceGxmTextureSetHeight, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (height - 1 >= GXM_TEXTURE_MAX_SIZE) {
		GXM_DBG_ERROR(1620, "Parameter height to function sceGxmTextureSetHeight has value %u, "
			      "which is not in the range [1, %u].", height, GXM_TEXTURE_MAX_SIZE);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	word1 = texture->controlWords[1];
	if ((word1 & GXM_TEX1_TYPE) == SCE_GXM_TEXTURE_TILED) {
		lastLevel = (((texture->controlWords[0] & GXM_TEX0_MIP_COUNT) >>
			      GXM_TEX0_MIP_COUNT_SHIFT) + 1) & 0xf;
		if (lastLevel != 0)
			lastLevel--;
		if (height >> lastLevel < GXM_TEXTURE_TILE_SIZE) {
			GXM_DBG_ERROR(1632, "Parameter height to function sceGxmTextureSetHeight results in a mip "
				      "level with height less than %u, but the texture is tiled",
				      GXM_TEXTURE_TILE_SIZE);
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
	} else if ((word1 & GXM_TEXTURE_TYPE_ARBITRARY_SIZE) == 0) {
		if (((height - 1) & height) != 0) {
			GXM_DBG_ERROR(1642, "Parameter height to function sceGxmTextureSetHeight has value %u, "
				      "which is not a power of 2, but the texture is swizzled.", height);
			return SCE_GXM_ERROR_INVALID_ALIGNMENT;
		}
		texture->controlWords[1] = gxmLog2(height) | (word1 & ~GXM_TEX1_HEIGHT_LOG2);
		return 0;
	}
	texture->controlWords[1] = (height - 1) | (word1 & ~GXM_TEX1_HEIGHT);
	return 0;
}

SceUInt32 sceGxmTextureGetHeight(const SceGxmTexture *texture)
{
	GXM_DBG_ASSERT(1665, texture);
	if ((sceGxmTextureGetType(texture) & GXM_TEXTURE_TYPE_ARBITRARY_SIZE) == 0)
		return 1 << (texture->controlWords[1] & GXM_TEX1_HEIGHT_LOG2);
	return (texture->controlWords[1] & GXM_TEX1_HEIGHT) + 1;
}

int sceGxmTextureSetMipmapCount(SceGxmTexture *texture, SceUInt32 mipCount)
{
	SceUInt32 word1, lastLevel, width;

	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(1686, sceGxmTextureSetMipmapCount, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	word1 = texture->controlWords[1];
	if ((word1 & GXM_TEX1_TYPE) == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
		GXM_DBG_ERROR(1690, "The function sceGxmTextureSetMipmapCount cannot be called on a texture of "
			      "type SCE_GXM_TEXTURE_LINEAR_STRIDED.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (mipCount > GXM_TEXTURE_MAX_MIP_COUNT) {
		GXM_DBG_ERROR(1696, "Parameter mipCount to function sceGxmTextureSetMipmapCount has value %u, "
			      "which is not in the range [0, %u].", mipCount, GXM_TEXTURE_MAX_MIP_COUNT);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if ((word1 & GXM_TEX1_TYPE) == SCE_GXM_TEXTURE_TILED) {
		/* the smallest level must still cover a whole tile */
		width = ((word1 & GXM_TEX1_WIDTH) >> GXM_TEX1_WIDTH_SHIFT) + 1;
		if (mipCount == 0) {
			if (width < GXM_TEXTURE_TILE_SIZE)
				goto tooSmall;
			lastLevel = 0;
		} else {
			lastLevel = mipCount - 1;
			if (width >> lastLevel < GXM_TEXTURE_TILE_SIZE)
				goto tooSmall;
		}
		if (((word1 & GXM_TEX1_HEIGHT) + 1) >> lastLevel < GXM_TEXTURE_TILE_SIZE)
			goto tooSmall;
	}
	texture->controlWords[0] = (texture->controlWords[0] & ~GXM_TEX0_MIP_COUNT) |
				   ((mipCount - 1) & 0xf) << GXM_TEX0_MIP_COUNT_SHIFT;
	return 0;

tooSmall:
	GXM_DBG_ERROR(1707, "Parameter mipCount to function sceGxmTextureSetMipmapCount results in a mip "
		      "level that has dimension(s) smaller than %ux%u, but the texture is tiled",
		      GXM_TEXTURE_TILE_SIZE, GXM_TEXTURE_TILE_SIZE);
	return SCE_GXM_ERROR_INVALID_VALUE;
}

SceUInt32 sceGxmTextureGetMipmapCountUnsafe(const SceGxmTexture *texture)
{
	GXM_DBG_ASSERT(1725, texture);
	return (((texture->controlWords[0] & GXM_TEX0_MIP_COUNT) >> GXM_TEX0_MIP_COUNT_SHIFT) + 1) & 0xf;
}

SceUInt32 sceGxmTextureGetMipmapCount(const SceGxmTexture *texture)
{
	GXM_DBG_ASSERT(1738, texture);
	if (sceGxmTextureGetType(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
		return 0;
	return (((texture->controlWords[0] & GXM_TEX0_MIP_COUNT) >> GXM_TEX0_MIP_COUNT_SHIFT) + 1) & 0xf;
}

int sceGxmTextureSetData(SceGxmTexture *texture, const void *data)
{
	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(1759, sceGxmTextureSetData, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (((gxmTextureFormatGetAlignment(gxmTextureGetFormat(texture)) - 1) &
	     (SceUInt32)data) != 0) {
		GXM_DBG_ERROR(1765, "Parameter data to function sceGxmTextureSetData has value %p, "
			      "which is not aligned to %d.", data,
			      gxmTextureFormatGetAlignment(sceGxmTextureGetFormat(texture)));
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	texture->controlWords[2] = ((SceUInt32)data & GXM_TEX2_DATA) |
				   (texture->controlWords[2] & GXM_TEX2_LOD_MIN_HIGH);
	return 0;
}

void *sceGxmTextureGetData(const SceGxmTexture *texture)
{
	GXM_DBG_ASSERT(1783, texture);
	return (void *)(texture->controlWords[2] & GXM_TEX2_DATA);
}

int sceGxmTextureSetPalette(SceGxmTexture *texture, const void *paletteData)
{
	SceGxmTextureFormat format;
#if SCE_DBG_ASSERTS_ENABLED
	const char *name, *swizzle;
#endif

	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(1801, sceGxmTextureSetPalette, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (((SceUInt32)paletteData & 0x3f) != 0) {
		GXM_DBG_ERROR(1807, "Parameter paletteData to function sceGxmTextureSetPalette has value %p, "
			      "which is not aligned to %d.", paletteData, 64);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	/* a palette goes with a palettized format, and only with one */
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_PALETTE_OPTIONAL)) {
		format = gxmTextureGetFormat(texture);
		if (!gxmTextureFormatIsPalettized(format)) {
			if (paletteData != NULL) {
#if SCE_DBG_ASSERTS_ENABLED
				name = gxmTextureFormatGetName(format);
				swizzle = gxmTextureFormatGetSwizzleName(format);
				GXM_DBG_ERROR(1825, "Parameter paletteData to function sceGxmTextureSetPalette must be "
					      "NULL when the format [%s|%s] is not palettized.", name, swizzle);
#endif
				return SCE_GXM_ERROR_INVALID_POINTER;
			}
		} else if (paletteData == NULL) {
#if SCE_DBG_ASSERTS_ENABLED
			name = gxmTextureFormatGetName(format);
			swizzle = gxmTextureFormatGetSwizzleName(format);
			GXM_DBG_ERROR(1818, "Parameter paletteData to function sceGxmTextureSetPalette must be "
				      "non-NULL when the format [%s|%s] is palettized.", name, swizzle);
#endif
			return SCE_GXM_ERROR_INVALID_POINTER;
		}
	}
	texture->controlWords[3] = (texture->controlWords[3] & ~GXM_TEX3_PALETTE) |
				   (SceUInt32)paletteData >> 6;
	return 0;
}

void *sceGxmTextureGetPalette(const SceGxmTexture *texture)
{
	GXM_DBG_ASSERT(1839, texture);
	return (void *)(texture->controlWords[3] << 6);
}

int sceGxmTextureSetGammaMode(SceGxmTexture *texture, SceUInt32 gammaMode)
{
#if SCE_DBG_ASSERTS_ENABLED
	const char *name, *swizzle;
#endif

	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(1858, sceGxmTextureSetGammaMode, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (gammaMode != 0 && !gxmTextureFormatIsGammaCapable(gxmTextureGetFormat(texture))) {
#if SCE_DBG_ASSERTS_ENABLED
		name = gxmTextureFormatGetName(sceGxmTextureGetFormat(texture));
		swizzle = gxmTextureFormatGetSwizzleName(sceGxmTextureGetFormat(texture));
		GXM_DBG_ERROR(1866, "Parameter gammaMode specifies gamma correction, but texture format [%s|%s] "
			      "does not support gamma correction.", name, swizzle);
#endif
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	texture->controlWords[0] = (gammaMode & GXM_TEX0_GAMMA_MODE) |
				   (texture->controlWords[0] & ~GXM_TEX0_GAMMA_MODE);
	return 0;
}

SceUInt32 sceGxmTextureGetGammaMode(const SceGxmTexture *texture)
{
	GXM_DBG_ASSERT(1877, texture);
	return texture->controlWords[0] & GXM_TEX0_GAMMA_MODE;
}

int sceGxmTextureSetNormalizeMode(SceGxmTexture *texture, SceUInt32 normalizeMode)
{
	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(1890, sceGxmTextureSetNormalizeMode, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	texture->controlWords[3] = (normalizeMode & GXM_TEX3_NORMALIZE_MODE) |
				   (texture->controlWords[3] & ~GXM_TEX3_NORMALIZE_MODE);
	return 0;
}

SceUInt32 sceGxmTextureGetNormalizeMode(const SceGxmTexture *texture)
{
	GXM_DBG_ASSERT(1900, texture);
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

	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(1910, sceGxmValidateTexture, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}

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
	    type - TYPE_INDEX(SCE_GXM_TEXTURE_CUBE) > 5) {
		GXM_DBG_ERROR(1946, "T1_TEXTYPE has value %d, which is not a valid texture type.", type);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (!gxmTextureFormatIsSupported(format)) {
		GXM_DBG_ERROR(1953, "T0_TEXFEXT has value %d and T1_TEXFORMAT has value %d, which is not a "
			      "valid texture format.", format >> 31, (format & GXM_FORMAT_BASE) >> 24);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (!gxmTextureFormatGetSwizzleComponents(format)) {
		GXM_DBG_ERROR(1960, "T3_SWIZ has value %d, which is not a valid swizzle mode for texture "
			      "format %s.", (format & GXM_FORMAT_SWIZZLE) >> 12, gxmTextureFormatGetName(format));
		return SCE_GXM_ERROR_INVALID_VALUE;
	}

	arbitrary = (type & 5) != 0;
	if (!arbitrary) {
		if ((texture->controlWords[1] & GXM_TEX1_WIDTH_LOG2) >> GXM_TEX1_WIDTH_LOG2_SHIFT > 12) {
			GXM_DBG_ERROR(1972, "T1_USIZE has value %u, which is not in the range [%u, %u].",
				      (texture->controlWords[1] & GXM_TEX1_WIDTH_LOG2) >> GXM_TEX1_WIDTH_LOG2_SHIFT,
				      0, 12);
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
		if ((texture->controlWords[1] & GXM_TEX1_HEIGHT_LOG2) > 12) {
			GXM_DBG_ERROR(1980, "T1_VSIZE has value %u, which is not in the range [%u, %u].",
				      texture->controlWords[1] & GXM_TEX1_HEIGHT_LOG2, 0, 12);
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
	}
	/* a field of 15 is a count of 0 */
	if (type != TYPE_INDEX(SCE_GXM_TEXTURE_LINEAR_STRIDED) &&
	    mipField > GXM_TEXTURE_MAX_MIP_COUNT - 1 && mipField != 0xf) {
		GXM_DBG_ERROR(1989, "T0_MIPMAPCLAMP has value %u, which is not a valid mip clamp value.",
			      mipField);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}

	if (compressed &&
	    (type == TYPE_INDEX(SCE_GXM_TEXTURE_LINEAR) ||
	     type == TYPE_INDEX(SCE_GXM_TEXTURE_LINEAR_STRIDED) ||
	     type == TYPE_INDEX(SCE_GXM_TEXTURE_TILED))) {
		GXM_DBG_ERROR(1998, "Texture format %s is not supported for textures of type TILED, LINEAR, "
			      "or LINEAR_STRIDED.", gxmTextureFormatGetName(format));
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (yuvPlanar) {
		if (gxmGlobals.compatFlags & GXM_COMPAT_YUV_TILED) {
			if (type - TYPE_INDEX(SCE_GXM_TEXTURE_LINEAR) > 1) {
				GXM_DBG_ERROR(2009, "Texture format %s is only supported for textures of type "
					      "LINEAR or TILED.", gxmTextureFormatGetName(format));
				return SCE_GXM_ERROR_UNSUPPORTED;
			}
		} else if (type != TYPE_INDEX(SCE_GXM_TEXTURE_LINEAR)) {
			GXM_DBG_ERROR(2016, "Texture format %s is only supported for textures of type LINEAR.",
				      gxmTextureFormatGetName(format));
			return SCE_GXM_ERROR_UNSUPPORTED;
		}
	}
	if (yuv422 &&
	    type != TYPE_INDEX(SCE_GXM_TEXTURE_LINEAR) &&
	    type != TYPE_INDEX(SCE_GXM_TEXTURE_LINEAR_STRIDED) &&
	    type != TYPE_INDEX(SCE_GXM_TEXTURE_TILED)) {
		GXM_DBG_ERROR(2026, "Texture format %s is only supported for textures of type LINEAR, TILED, "
			      "or LINEAR_STRIDED", gxmTextureFormatGetName(format));
		return SCE_GXM_ERROR_UNSUPPORTED;
	}

	/* border modes go on both axes or neither */
	if (uBorder != vBorder) {
		GXM_DBG_ERROR(2033, "Address modes in U and V contain both a border and a non-border mode, "
			      "which is not supported.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (uBorder) {
		if (type != TYPE_INDEX(SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY) &&
		    type != TYPE_INDEX(SCE_GXM_TEXTURE_SWIZZLED)) {
			GXM_DBG_ERROR(2040, "Border address modes are only supported for textures of type of "
				      "SWIZZLED or SWIZZLED_ARBITRARY.");
			return SCE_GXM_ERROR_UNSUPPORTED;
		}
		if (palettized || compressed || yuv) {
			GXM_DBG_ERROR(2046, "Border address modes are not supported for palettized, block "
				      "compressed, or YUV formats.");
			return SCE_GXM_ERROR_UNSUPPORTED;
		}
	}

	/* modes: 0 off, 1 R, 3 GR; Sony's message prints 2 whatever the mode */
	if (gammaMode > 1 && gammaMode != 3) {
		GXM_DBG_ERROR(2057, "T0_GAMMA has value %d, which is not a valid gamma mode.", 2);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (!gxmTextureFormatIsGammaCapable(format) && gammaMode != 0) {
		GXM_DBG_ERROR(2065, "Texture format %s requires the gamma mode to be NONE.",
			      gxmTextureFormatGetName(format));
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (gxmTextureFormatIs24Bit(format) && type == TYPE_INDEX(SCE_GXM_TEXTURE_LINEAR_STRIDED)) {
		GXM_DBG_ERROR(2073, "24-bit texture formats are not supported for textures with a type of "
			      "LINEAR_STRIDED.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}

	/* cube maps and linear strided textures clamp; mirroring is for swizzled ones */
	if (type == TYPE_INDEX(SCE_GXM_TEXTURE_CUBE) ||
	    type == TYPE_INDEX(SCE_GXM_TEXTURE_LINEAR_STRIDED) ||
	    type == TYPE_INDEX(SCE_GXM_TEXTURE_CUBE_ARBITRARY)) {
		if (uMode != SCE_GXM_TEXTURE_ADDR_CLAMP || vMode != SCE_GXM_TEXTURE_ADDR_CLAMP) {
			GXM_DBG_ERROR(2081, "Address mode must be CLAMP for textures of type LINEAR_STRIDED, "
				      "CUBE or CUBE_ARBITRARY.");
			return SCE_GXM_ERROR_UNSUPPORTED;
		}
	}
	if ((uMode == SCE_GXM_TEXTURE_ADDR_MIRROR || vMode == SCE_GXM_TEXTURE_ADDR_MIRROR) &&
	    type != TYPE_INDEX(SCE_GXM_TEXTURE_SWIZZLED)) {
		GXM_DBG_ERROR(2089, "Address mode MIRROR is only supported for textures of type SWIZZLED.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}

	if ((word2 & GXM_TEX2_DATA) == 0) {
		GXM_DBG_ERROR(2096, "Texture data pointer is NULL.");
		return SCE_GXM_ERROR_INVALID_TEXTURE_DATA_POINTER;
	}
	if (((gxmTextureFormatGetAlignment(format) - 1) & word2 & GXM_TEX2_DATA) != 0) {
		GXM_DBG_ERROR(2103, "Texture format %s requires that the data address is aligned to %d.",
			      gxmTextureFormatGetName(format), gxmTextureFormatGetAlignment(format));
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	if (!palettized) {
		if (!(gxmGlobals.compatFlags & GXM_COMPAT_PALETTE_OPTIONAL) && (word3 << 6) != 0) {
			GXM_DBG_ERROR(2118, "Palette data pointer is not NULL.");
			return SCE_GXM_ERROR_INVALID_TEXTURE_PALETTE_POINTER;
		}
	} else if ((word3 << 6) == 0) {
		GXM_DBG_ERROR(2110, "Palette data pointer is NULL.");
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
		if (width >> mipField < GXM_TEXTURE_TILE_SIZE || height >> mipField < GXM_TEXTURE_TILE_SIZE) {
			GXM_DBG_ERROR(2133, "The smallest mipmap must be at least %ux%u pixels in size for "
				      "textures of type TILED", GXM_TEXTURE_TILE_SIZE, GXM_TEXTURE_TILE_SIZE);
			return SCE_GXM_ERROR_UNSUPPORTED;
		}
		reserved0 = 0x6001e807;
	} else if (type == TYPE_INDEX(SCE_GXM_TEXTURE_LINEAR_STRIDED)) {
		reserved0 = 0x6001e001;
	} else {
		reserved0 = 0x6001e807;
	}
	reserved1 = arbitrary ? 0 : 0x00f0fff0;
	if ((reserved0 & texture->controlWords[0]) != 0) {
		GXM_DBG_ERROR(2202, "Texture control word 0 has some unused bits set.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if ((reserved1 & texture->controlWords[1]) != 0) {
		GXM_DBG_ERROR(2207, "Texture control word 1 has some unused bits set.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	return 0;
}
