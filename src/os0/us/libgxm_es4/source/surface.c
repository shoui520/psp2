/*
 * SceGxm: color and depth/stencil surfaces.
 *
 * A color surface is the PBE state for one render target plus the
 * texture that reads it back as the scene background.
 */

#include "gxm_internal.h"

/* --- color surfaces --- */

#define SCE_GXM_COLOR_SURFACE_LINEAR		0x00000000
#define SCE_GXM_COLOR_SURFACE_TILED		0x04000000
#define SCE_GXM_COLOR_SURFACE_SWIZZLED		0x08000000

#define SCE_GXM_COLOR_SURFACE_GAMMA_NONE	0x00000000
#define SCE_GXM_COLOR_SURFACE_GAMMA_R		0x00001000
#define SCE_GXM_COLOR_SURFACE_GAMMA_GR		0x00003000
#define SCE_GXM_COLOR_SURFACE_GAMMA_BGR		0x00001000

#define SCE_GXM_TEXTURE_GAMMA_R			0x08000000
#define SCE_GXM_TEXTURE_GAMMA_GR		0x18000000

#define SCE_GXM_COLOR_FORMAT_U8U8U8U8_ARGB	0x0c003000	/* as a texture format */

/* control word */
#define GXM_COLOR0_SCALE_MODE		0x00000003
#define GXM_COLOR0_ENABLED		0x00000004
#define GXM_COLOR0_DITHER		0x00000008
#define GXM_COLOR0_GAMMA		0x00003000

/* format word */
#define GXM_COLOR1_ENABLED		0x0001ff00
#define GXM_COLOR1_PBE_ENABLE		0x00010000
#define GXM_COLOR1_TYPE			0x0c000000
#define GXM_COLOR1_FORMAT		0xf1b00000

#define GXM_COLOR3_STRIDE		0x00003fff	/* stride in pixel pairs, minus one */

/* clip words: minimum in word 5, maximum in word 6 */
#define GXM_COLOR_CLIP_X		0x00000fff
#define GXM_COLOR_CLIP_Y_SHIFT		12
#define GXM_COLOR_CLIP_Y		0x00fff000
#define GXM_COLOR5_TILES_X_SHIFT	24
#define GXM_COLOR5_TILES_Y_SHIFT	28
#define GXM_COLOR5_TILES		0xff000000

typedef struct SceGxmColorSurface {
	SceUInt32 control;
	SceUInt32 format;
	SceUInt32 data;
	SceUInt32 stride;
	SceUInt32 reserved;		/* pbeEmitWords[3], always 0 */
	SceUInt32 clipMin;
	SceUInt32 clipMax;
	SceUInt32 outputRegisterSize;
	SceGxmTexture backgroundTex;
} SceGxmColorSurface;

static inline _Bool gxmColorSurfaceIsEnabled(const SceGxmColorSurface *surface)
{
	if (gxmGlobals.compatFlags & GXM_COMPAT_COLOR_ENABLE_DATA)
		return surface->data != 0;
	return (surface->format & GXM_COLOR1_ENABLED) != 0;
}

static inline SceUInt32 gxmLog2(SceUInt32 value)
{
	return 31 - __builtin_clz(value);
}

int sceGxmColorSurfaceInit(SceGxmColorSurface *surface, SceGxmColorFormat colorFormat,
			   SceUInt32 surfaceType, SceUInt32 scaleMode,
			   SceUInt32 outputRegisterSize, SceUInt32 width, SceUInt32 height,
			   SceUInt32 strideInPixels, void *data)
{
	SceGxmTextureFormat texFormat;
	SceUInt32 outputMode, bytesPerPixel, byteStride;
	int err;

	if (surface == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!gxmColorFormatIsSupported(colorFormat) ||
	    width - 1 >= GXM_TEXTURE_MAX_SIZE || height - 1 >= GXM_TEXTURE_MAX_SIZE)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (strideInPixels & 1)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (width > strideInPixels || ((SceUInt32)data & 3) != 0)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_SURFACE_ANY) &&
	    (scaleMode > 1 ||
	     ((surfaceType & ~SCE_GXM_COLOR_SURFACE_TILED) != 0 &&
	      surfaceType != SCE_GXM_COLOR_SURFACE_SWIZZLED)))
		return SCE_GXM_ERROR_INVALID_VALUE;

	if (scaleMode != 0 && !gxmColorFormatSupportsDownscale(colorFormat))
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (surfaceType == SCE_GXM_COLOR_SURFACE_SWIZZLED) {
		if (width < 16 || height < 16 ||
		    ((width - 1) & width) != 0 || ((height - 1) & height) != 0)
			return SCE_GXM_ERROR_INVALID_VALUE;
	} else if (surfaceType == SCE_GXM_COLOR_SURFACE_TILED) {
		if (width < GXM_TEXTURE_TILE_SIZE || height < GXM_TEXTURE_TILE_SIZE)
			return SCE_GXM_ERROR_INVALID_VALUE;
	}

	/* not written when the format has neither output mode */
	err = gxmColorFormatGetOutputMode(colorFormat, outputRegisterSize, &outputMode);
	if (err != 0)
		return err;

	surface->reserved = 0;
	surface->clipMax = (width - 1) | (height - 1) << GXM_COLOR_CLIP_Y_SHIFT;
	surface->control = (scaleMode & GXM_COLOR0_SCALE_MODE) | GXM_COLOR0_ENABLED;
	surface->clipMin = gxmLog2((width + 15) >> 4) << GXM_COLOR5_TILES_X_SHIFT |
			   gxmLog2((height + 15) >> 4) << GXM_COLOR5_TILES_Y_SHIFT;
	surface->format = outputMode | GXM_COLOR1_PBE_ENABLE |
			  (surfaceType & GXM_COLOR1_TYPE) | (colorFormat & GXM_COLOR1_FORMAT);
	surface->data = (SceUInt32)data & ~3;
	surface->stride = (((strideInPixels + 1) >> 1) - 1) & GXM_COLOR3_STRIDE;
	texFormat = gxmColorFormatGetTextureFormat(colorFormat);
	surface->outputRegisterSize = outputRegisterSize;

	if (surfaceType == SCE_GXM_COLOR_SURFACE_TILED) {
		err = sceGxmTextureInitTiled(&surface->backgroundTex, data, texFormat,
					     width, height, 1);
	} else if (surfaceType == SCE_GXM_COLOR_SURFACE_SWIZZLED) {
		err = sceGxmTextureInitSwizzled(&surface->backgroundTex, data, texFormat,
						width, height, 1);
	} else if (((width + 7) & ~7) == strideInPixels) {
		err = sceGxmTextureInitLinear(&surface->backgroundTex, data, texFormat,
					      width, height, 1);
	} else {
		/* a padded stride needs a linear strided texture */
		if (gxmTextureFormatIs24Bit(texFormat))
			return SCE_GXM_ERROR_INVALID_VALUE;
		if (gxmGlobals.compatFlags & GXM_COMPAT_COLOR_BYTES)
			bytesPerPixel = gxmColorFormatGetBytesPerPixel(colorFormat);
		else
			bytesPerPixel = gxmColorFormatGetBitsPerPixel(colorFormat) >> 3;
		byteStride = strideInPixels * bytesPerPixel;
		if (byteStride - 4 > GXM_TEXTURE_MAX_STRIDE - 4)
			return SCE_GXM_ERROR_INVALID_VALUE;
		err = sceGxmTextureInitLinearStrided(&surface->backgroundTex, data, texFormat,
						     width, height, byteStride);
	}

	if ((surface->control & GXM_COLOR0_GAMMA) == SCE_GXM_COLOR_SURFACE_GAMMA_R)
		sceGxmTextureSetGammaMode(&surface->backgroundTex, SCE_GXM_TEXTURE_GAMMA_R);
	else if ((surface->control & GXM_COLOR0_GAMMA) == SCE_GXM_COLOR_SURFACE_GAMMA_GR)
		sceGxmTextureSetGammaMode(&surface->backgroundTex, SCE_GXM_TEXTURE_GAMMA_GR);
	return err;
}

int sceGxmColorSurfaceInitDisabled(SceGxmColorSurface *surface)
{
	if (surface == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	surface->control = GXM_COLOR0_ENABLED;
	surface->format = 0;
	surface->data = 0;
	surface->stride = 0;
	surface->reserved = 0;
	surface->clipMin = 0;
	surface->clipMax = 0;
	surface->outputRegisterSize = 0;
	sceGxmTextureInitLinear(&surface->backgroundTex, NULL, SCE_GXM_COLOR_FORMAT_U8U8U8U8_ARGB,
				1, 1, 0);
	return 0;
}

SceBool sceGxmColorSurfaceIsEnabled(const SceGxmColorSurface *surface)
{
	return gxmColorSurfaceIsEnabled(surface);
}

void sceGxmColorSurfaceGetClip(const SceGxmColorSurface *surface, SceUInt32 *xMin,
			       SceUInt32 *yMin, SceUInt32 *xMax, SceUInt32 *yMax)
{
	if (xMin != NULL)
		*xMin = surface->clipMin & GXM_COLOR_CLIP_X;
	if (yMin != NULL)
		*yMin = (surface->clipMin & GXM_COLOR_CLIP_Y) >> GXM_COLOR_CLIP_Y_SHIFT;
	if (xMax != NULL)
		*xMax = surface->clipMax & GXM_COLOR_CLIP_X;
	if (yMax != NULL)
		*yMax = (surface->clipMax & GXM_COLOR_CLIP_Y) >> GXM_COLOR_CLIP_Y_SHIFT;
}

void sceGxmColorSurfaceSetClip(SceGxmColorSurface *surface, SceUInt32 xMin, SceUInt32 yMin,
			       SceUInt32 xMax, SceUInt32 yMax)
{
	surface->clipMin = ((yMin << GXM_COLOR_CLIP_Y_SHIFT) & GXM_COLOR_CLIP_Y) |
			   (surface->clipMin & GXM_COLOR5_TILES) | (xMin & GXM_COLOR_CLIP_X);
	surface->clipMax = ((yMax << GXM_COLOR_CLIP_Y_SHIFT) & GXM_COLOR_CLIP_Y) |
			   (surface->clipMax & GXM_COLOR5_TILES) | (xMax & GXM_COLOR_CLIP_X);
}

SceUInt32 sceGxmColorSurfaceGetScaleMode(const SceGxmColorSurface *surface)
{
	return surface->control & GXM_COLOR0_SCALE_MODE;
}

void sceGxmColorSurfaceSetScaleMode(SceGxmColorSurface *surface, SceUInt32 scaleMode)
{
	surface->control = (scaleMode & GXM_COLOR0_SCALE_MODE) |
			   (surface->control & ~GXM_COLOR0_SCALE_MODE);
}

void *sceGxmColorSurfaceGetData(const SceGxmColorSurface *surface)
{
	return (void *)(surface->data & ~3);
}

int sceGxmColorSurfaceSetData(SceGxmColorSurface *surface, void *data)
{
	if (surface == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!gxmColorSurfaceIsEnabled(surface))
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (((SceUInt32)data & 3) != 0)
		return SCE_GXM_ERROR_INVALID_VALUE;
	surface->data = ((SceUInt32)data & ~3) | (surface->data & 3);
	sceGxmTextureSetData(&surface->backgroundTex, data);
	return 0;
}

SceGxmColorFormat sceGxmColorSurfaceGetFormat(const SceGxmColorSurface *surface)
{
	return surface->format & GXM_COLOR1_FORMAT;
}

int sceGxmColorSurfaceSetFormat(SceGxmColorSurface *surface, SceGxmColorFormat format)
{
	SceGxmColorFormat oldFormat;

	if (surface == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!gxmColorSurfaceIsEnabled(surface))
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (!gxmColorFormatIsSupported(format))
		return SCE_GXM_ERROR_INVALID_VALUE;

	/* the pixel size cannot change */
	oldFormat = surface->format & GXM_COLOR1_FORMAT;
	if (gxmGlobals.compatFlags & GXM_COMPAT_COLOR_BYTES) {
		if (gxmColorFormatGetBytesPerPixel(oldFormat) !=
		    gxmColorFormatGetBytesPerPixel(format))
			return SCE_GXM_ERROR_UNSUPPORTED;
	} else {
		if (gxmColorFormatGetBitsPerPixel(oldFormat) !=
		    gxmColorFormatGetBitsPerPixel(format))
			return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if ((surface->control & GXM_COLOR0_SCALE_MODE) != 0 &&
	    !gxmColorFormatSupportsDownscale(format))
		return SCE_GXM_ERROR_UNSUPPORTED;

	surface->format = (format & GXM_COLOR1_FORMAT) | (surface->format & ~GXM_COLOR1_FORMAT);
	sceGxmTextureSetFormat(&surface->backgroundTex, gxmColorFormatGetTextureFormat(format));
	return 0;
}

SceUInt32 sceGxmColorSurfaceGetType(const SceGxmColorSurface *surface)
{
	return surface->format & GXM_COLOR1_TYPE;
}

SceUInt32 sceGxmColorSurfaceGetStrideInPixels(const SceGxmColorSurface *surface)
{
	return ((surface->stride & GXM_COLOR3_STRIDE) + 1) * 2;
}

SceUInt32 sceGxmColorSurfaceGetGammaMode(const SceGxmColorSurface *surface)
{
	return surface->control & GXM_COLOR0_GAMMA;
}

int sceGxmColorSurfaceSetGammaMode(SceGxmColorSurface *surface, SceUInt32 gammaMode)
{
	if (surface == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_SURFACE_ANY) &&
	    (gammaMode & ~SCE_GXM_COLOR_SURFACE_GAMMA_R) != 0 &&
	    gammaMode != SCE_GXM_COLOR_SURFACE_GAMMA_GR)
		return SCE_GXM_ERROR_INVALID_VALUE;

	if (gammaMode == SCE_GXM_COLOR_SURFACE_GAMMA_NONE) {
		sceGxmTextureSetGammaMode(&surface->backgroundTex, 0);
	} else {
		/* swizzled surfaces and formats without gamma at this register size cannot */
		if ((surface->format & GXM_COLOR1_TYPE) == SCE_GXM_COLOR_SURFACE_SWIZZLED)
			return SCE_GXM_ERROR_INVALID_VALUE;
		if (!gxmColorFormatIsGammaCapable(surface->format & GXM_COLOR1_FORMAT,
						  surface->outputRegisterSize))
			return SCE_GXM_ERROR_INVALID_VALUE;
		if (gammaMode == SCE_GXM_COLOR_SURFACE_GAMMA_R)
			sceGxmTextureSetGammaMode(&surface->backgroundTex, SCE_GXM_TEXTURE_GAMMA_R);
		else if (gammaMode == SCE_GXM_COLOR_SURFACE_GAMMA_GR)
			sceGxmTextureSetGammaMode(&surface->backgroundTex, SCE_GXM_TEXTURE_GAMMA_GR);
	}
	surface->control = (gammaMode & GXM_COLOR0_GAMMA) | (surface->control & ~GXM_COLOR0_GAMMA);
	return 0;
}

SceUInt32 sceGxmColorSurfaceGetDitherMode(const SceGxmColorSurface *surface)
{
	return surface->control & GXM_COLOR0_DITHER;
}

int sceGxmColorSurfaceSetDitherMode(SceGxmColorSurface *surface, SceUInt32 ditherMode)
{
	if (surface == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_SURFACE_ANY) &&
	    (ditherMode & ~GXM_COLOR0_DITHER) != 0)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (ditherMode != 0 &&
	    !gxmColorFormatSupportsDither(surface->format & GXM_COLOR1_FORMAT))
		return SCE_GXM_ERROR_INVALID_VALUE;
	surface->control = (ditherMode & GXM_COLOR0_DITHER) | (surface->control & ~GXM_COLOR0_DITHER);
	return 0;
}

/* --- depth/stencil surfaces --- */

#define SCE_GXM_DEPTH_STENCIL_FORMAT_DF32	0x00044000
#define SCE_GXM_DEPTH_STENCIL_FORMAT_S8		0x00022000
#define SCE_GXM_DEPTH_STENCIL_FORMAT_DF32_S8	0x00066000
#define SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24	0x01266000
#define SCE_GXM_DEPTH_STENCIL_FORMAT_D16	0x02444000
#define GXM_DEPTH_STENCIL_FORMAT_DF32M		0x000cc000
#define GXM_DEPTH_STENCIL_FORMAT_DF32M_S8	0x000ee000

#define SCE_GXM_DEPTH_STENCIL_SURFACE_LINEAR	0x00000000
#define SCE_GXM_DEPTH_STENCIL_SURFACE_TILED	0x00011000

/* zls control word */
#define GXM_ZLS_FORCE_LOAD		0x00000002
#define GXM_ZLS_FORCE_STORE		0x00000004
#define GXM_ZLS_STRIDE_SHIFT		3
#define GXM_ZLS_STRIDE			0x000007f8	/* stride in 32-sample tiles, minus one */
#define GXM_ZLS_TYPE			0x00011000
#define GXM_ZLS_FORMAT			0x07eee000
#define GXM_ZLS_ENABLE			0x00100000

/* background control word */
#define GXM_ZLS_BG_STENCIL		0x000000ff
#define GXM_ZLS_BG_MASK			0x00000100
#define GXM_ZLS_BG_DEFAULT		0x00000300

typedef struct SceGxmDepthStencilSurface {
	SceUInt32 zlsControl;
	void *depthData;
	void *stencilData;
	float backgroundDepth;
	SceUInt32 backgroundControl;
} SceGxmDepthStencilSurface;

int sceGxmDepthStencilSurfaceInit(SceGxmDepthStencilSurface *surface, SceUInt32 depthStencilFormat,
				  SceUInt32 surfaceType, SceUInt32 strideInSamples,
				  void *depthData, void *stencilData)
{
	SceUInt32 format = depthStencilFormat;

	if (surface == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!(format == SCE_GXM_DEPTH_STENCIL_FORMAT_DF32 ||
	      format == SCE_GXM_DEPTH_STENCIL_FORMAT_S8 ||
	      format == SCE_GXM_DEPTH_STENCIL_FORMAT_DF32_S8 ||
	      format == GXM_DEPTH_STENCIL_FORMAT_DF32M ||
	      format == GXM_DEPTH_STENCIL_FORMAT_DF32M_S8 ||
	      format == SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24 ||
	      format == SCE_GXM_DEPTH_STENCIL_FORMAT_D16) ||
	    strideInSamples == 0 || (strideInSamples & 0x1f) != 0)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if ((((SceUInt32)depthData | (SceUInt32)stencilData) & 0xf) != 0)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_SURFACE_ANY) &&
	    surfaceType != SCE_GXM_DEPTH_STENCIL_SURFACE_LINEAR &&
	    surfaceType != SCE_GXM_DEPTH_STENCIL_SURFACE_TILED)
		return SCE_GXM_ERROR_INVALID_VALUE;
	/* every format but S8 has depth; the separate-stencil ones need stencil memory */
	if (format != SCE_GXM_DEPTH_STENCIL_FORMAT_S8 && depthData == NULL)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if ((format == SCE_GXM_DEPTH_STENCIL_FORMAT_DF32_S8 ||
	     format == SCE_GXM_DEPTH_STENCIL_FORMAT_S8 ||
	     format == GXM_DEPTH_STENCIL_FORMAT_DF32M_S8) && stencilData == NULL)
		return SCE_GXM_ERROR_INVALID_VALUE;

	surface->depthData = depthData;
	surface->stencilData = stencilData;
	surface->zlsControl = (surfaceType & GXM_ZLS_TYPE) | GXM_ZLS_ENABLE |
			      (format & GXM_ZLS_FORMAT) |
			      ((((strideInSamples >> 5) - 1) << GXM_ZLS_STRIDE_SHIFT) & GXM_ZLS_STRIDE);
	surface->backgroundDepth = 1.0f;
	surface->backgroundControl = GXM_ZLS_BG_DEFAULT;
	return 0;
}

int sceGxmDepthStencilSurfaceInitDisabled(SceGxmDepthStencilSurface *surface)
{
	if (surface == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	surface->backgroundDepth = 1.0f;
	surface->zlsControl = 0;
	surface->depthData = NULL;
	surface->stencilData = NULL;
	surface->backgroundControl = GXM_ZLS_BG_DEFAULT;
	return 0;
}

SceBool sceGxmDepthStencilSurfaceGetBackgroundMask(const SceGxmDepthStencilSurface *surface)
{
	return (surface->backgroundControl & GXM_ZLS_BG_MASK) >> 8;
}

void sceGxmDepthStencilSurfaceSetBackgroundMask(SceGxmDepthStencilSurface *surface,
						SceBool backgroundMask)
{
	surface->backgroundControl = (backgroundMask ? GXM_ZLS_BG_MASK : 0) |
				     (surface->backgroundControl & ~GXM_ZLS_BG_MASK);
}

GXM_VFP float sceGxmDepthStencilSurfaceGetBackgroundDepth(const SceGxmDepthStencilSurface *surface)
{
	return surface->backgroundDepth;
}

GXM_VFP void sceGxmDepthStencilSurfaceSetBackgroundDepth(SceGxmDepthStencilSurface *surface,
							 float backgroundDepth)
{
	surface->backgroundDepth = backgroundDepth;
}

SceUInt8 sceGxmDepthStencilSurfaceGetBackgroundStencil(const SceGxmDepthStencilSurface *surface)
{
	return surface->backgroundControl & GXM_ZLS_BG_STENCIL;
}

void sceGxmDepthStencilSurfaceSetBackgroundStencil(SceGxmDepthStencilSurface *surface,
						   SceUInt8 backgroundStencil)
{
	surface->backgroundControl = backgroundStencil |
				     (surface->backgroundControl & ~GXM_ZLS_BG_STENCIL);
}

SceBool sceGxmDepthStencilSurfaceIsEnabled(const SceGxmDepthStencilSurface *surface)
{
	return (surface->zlsControl & GXM_ZLS_FORMAT) != 0;
}

void sceGxmDepthStencilSurfaceSetForceLoadMode(SceGxmDepthStencilSurface *surface,
					       SceUInt32 forceLoad)
{
	surface->zlsControl = (forceLoad & GXM_ZLS_FORCE_LOAD) |
			      (surface->zlsControl & ~GXM_ZLS_FORCE_LOAD);
}

SceUInt32 sceGxmDepthStencilSurfaceGetForceLoadMode(const SceGxmDepthStencilSurface *surface)
{
	return surface->zlsControl & GXM_ZLS_FORCE_LOAD;
}

void sceGxmDepthStencilSurfaceSetForceStoreMode(SceGxmDepthStencilSurface *surface,
						SceUInt32 forceStore)
{
	surface->zlsControl = (forceStore & GXM_ZLS_FORCE_STORE) |
			      (surface->zlsControl & ~GXM_ZLS_FORCE_STORE);
}

SceUInt32 sceGxmDepthStencilSurfaceGetForceStoreMode(const SceGxmDepthStencilSurface *surface)
{
	return surface->zlsControl & GXM_ZLS_FORCE_STORE;
}

SceUInt32 sceGxmDepthStencilSurfaceGetFormat(const SceGxmDepthStencilSurface *surface)
{
	return surface->zlsControl & GXM_ZLS_FORMAT;
}

SceUInt32 sceGxmDepthStencilSurfaceGetStrideInSamples(const SceGxmDepthStencilSurface *surface)
{
	return (((surface->zlsControl & GXM_ZLS_STRIDE) >> GXM_ZLS_STRIDE_SHIFT) + 1) * 32;
}
