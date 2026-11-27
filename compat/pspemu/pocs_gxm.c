/*
 * pocs_gxm.c: ScePspemu PocketStation GXM 2D renderer, encrypted background
 * texture loader, and custom photo premultiplier (0x8102a37c..0x8102bf57).
 */
#include "pocs_internal.h"

#include <libaes.h>
#include <libsha256.h>
#include <stdlib.h>
#include <string.h>

#define POCS_GXM_DISPLAY_WIDTH          960U
#define POCS_GXM_DISPLAY_HEIGHT         544U
#define POCS_GXM_DISPLAY_STRIDE         1024U
#define POCS_GXM_FB_BYTES               (POCS_GXM_DISPLAY_STRIDE * POCS_GXM_DISPLAY_HEIGHT * 4U)
#define POCS_GXM_CDRAM_POOL_SIZE        0x880000U
#define POCS_GXM_DDR_RW_POOL_SIZE       0x20E000U
#define POCS_GXM_DDR_RO_POOL_SIZE       0x306000U
#define POCS_GXM_DEPTH_BYTES            2088960U
#define POCS_GXM_VERTEX_BUF_SIZE        0x5A258U
#define POCS_GXM_INDEX_BUF_SIZE         0x603CU
#define POCS_GXM_BG_UNPACK_SIZE         2616320U
#define POCS_GXM_BG_TEX_STRIDE          522240U
#define POCS_GXM_LED_LARGE_W            208U
#define POCS_GXM_LED_LARGE_H            38U
#define POCS_GXM_LED_SMALL_W            48U
#define POCS_GXM_LED_SMALL_H            16U
#define POCS_GXM_PIXEL_QUAD_VERTS       6U
#define POCS_GXM_GRID_VERTS             (32U * 32U * POCS_GXM_PIXEL_QUAD_VERTS)

#define POCS_GXM_ERR_IO                 ((int)0x80010005)
#define POCS_GXM_ERR_NO_MEMORY          ((int)0x8001000C)
#define POCS_GXM_ERR_BAD_SIZE           ((int)0x8001002F)

#define POCS_GXM_TEX_MAGIC              0x54415850U /* "PXAT" */

#define POCS_GXM_MEM_ATTRIB_READ        1U
#define POCS_GXM_MEM_ATTRIB_RW          3U

#define POCS_GXM_PRIMITIVE_TRIANGLES    0U
#define POCS_GXM_INDEX_FORMAT_U16       0U
#define POCS_GXM_COLOR_SCALE_NONE       0U
#define POCS_GXM_OUT_REG_SIZE_32BIT     0U
#define POCS_GXM_OUT_REG_FMT_UCHAR4     0U
#define POCS_GXM_MULTISAMPLE_NONE       0U
#define POCS_GXM_DEPTH_FMT_S8D24        0x00400000U
#define POCS_GXM_DEPTH_SURFACE_TILED    0x00011000U
#define POCS_GXM_TEX_FMT_U8U8U8U8_ABGR  0x0C001000U
#define POCS_GXM_TEX_FMT_P4_ABGR        0x94000000U
#define POCS_GXM_TEX_FMT_P8_ABGR        0x95000000U
#define POCS_GXM_ATTR_FMT_U8N           4U
#define POCS_GXM_ATTR_FMT_F16           8U
#define POCS_GXM_ATTR_FMT_F32           9U
#define POCS_GXM_PARAM_CAT_ATTRIBUTE    0U
#define POCS_GXM_PARAM_CAT_UNIFORM      1U

#define POCS_GXM_HALF_ZERO              0x0000U
#define POCS_GXM_HALF_HALF              0x3800U /* 0.5f */
#define POCS_GXM_HALF_ONE               0x3C00U /* 1.0f */

typedef struct SceGxmContext SceGxmContext;
typedef struct SceGxmRenderTarget SceGxmRenderTarget;
typedef struct SceGxmShaderPatcher SceGxmShaderPatcher;
typedef struct SceGxmProgram SceGxmProgram;
typedef struct SceGxmProgramParameter SceGxmProgramParameter;
typedef struct SceGxmVertexProgram SceGxmVertexProgram;
typedef struct SceGxmFragmentProgram SceGxmFragmentProgram;
typedef void *SceGxmShaderPatcherId;

typedef struct SceGxmTexture {
	uint32_t words[4];
} SceGxmTexture;

_Static_assert(sizeof(SceGxmTexture) == 0x10U, "SceGxmTexture size");

typedef struct SceGxmColorSurface {
	uint32_t      pbeSidebandWord;
	uint32_t      pbeEmitWords[6];
	uint32_t      outputRegisterSize;
	SceGxmTexture backgroundTex;
} SceGxmColorSurface;

_Static_assert(sizeof(SceGxmColorSurface) == 0x30U, "SceGxmColorSurface size");

typedef struct SceGxmDepthStencilSurface {
	uint32_t zlsControl;
	void    *depthData;
	void    *stencilData;
	float    backgroundDepth;
	uint32_t backgroundControl;
} SceGxmDepthStencilSurface;

_Static_assert(sizeof(SceGxmDepthStencilSurface) == 0x14U, "SceGxmDepthStencilSurface size");

typedef struct SceGxmContextParams {
	void    *hostMem;
	SceSize  hostMemSize;
	void    *vdmRingBufferMem;
	SceSize  vdmRingBufferMemSize;
	void    *vertexRingBufferMem;
	SceSize  vertexRingBufferMemSize;
	void    *fragmentRingBufferMem;
	SceSize  fragmentRingBufferMemSize;
	void    *fragmentUsseRingBufferMem;
	SceSize  fragmentUsseRingBufferMemSize;
	uint32_t fragmentUsseRingBufferOffset;
} SceGxmContextParams;

_Static_assert(sizeof(SceGxmContextParams) == 0x2CU, "SceGxmContextParams size");

typedef struct SceGxmRenderTargetParams {
	uint32_t flags;
	uint16_t width;
	uint16_t height;
	uint16_t scenesPerFrame;
	uint16_t multisampleMode;
	uint32_t multisampleLocations;
	SceUID   driverMemBlock;
} SceGxmRenderTargetParams;

_Static_assert(sizeof(SceGxmRenderTargetParams) == 0x14U, "SceGxmRenderTargetParams size");

typedef struct SceGxmShaderPatcherParams {
	void    *userData;
	void   *(*hostAllocCallback)(void *userData, SceSize size);
	void    (*hostFreeCallback)(void *userData, void *mem);
	void   *(*bufferAllocCallback)(void *userData, SceSize size);
	void    (*bufferFreeCallback)(void *userData, void *mem);
	void    *bufferMem;
	SceSize  bufferMemSize;
	void   *(*vertexUsseAllocCallback)(void *userData, SceSize size, uint32_t *usseOffset);
	void    (*vertexUsseFreeCallback)(void *userData, void *mem);
	void    *vertexUsseMem;
	SceSize  vertexUsseMemSize;
	uint32_t vertexUsseOffset;
	void   *(*fragmentUsseAllocCallback)(void *userData, SceSize size, uint32_t *usseOffset);
	void    (*fragmentUsseFreeCallback)(void *userData, void *mem);
	void    *fragmentUsseMem;
	SceSize  fragmentUsseMemSize;
	uint32_t fragmentUsseOffset;
} SceGxmShaderPatcherParams;

_Static_assert(sizeof(SceGxmShaderPatcherParams) == 0x44U, "SceGxmShaderPatcherParams size");

typedef struct SceGxmVertexAttribute {
	uint16_t streamIndex;
	uint16_t offset;
	uint8_t  format;
	uint8_t  componentCount;
	uint16_t regIndex;
} SceGxmVertexAttribute;

_Static_assert(sizeof(SceGxmVertexAttribute) == 8U, "SceGxmVertexAttribute size");

typedef struct SceGxmVertexStream {
	uint16_t stride;
	uint16_t indexSource;
} SceGxmVertexStream;

_Static_assert(sizeof(SceGxmVertexStream) == 4U, "SceGxmVertexStream size");

typedef struct SceGxmBlendInfo {
	uint8_t colorMask;
	uint8_t colorFunc : 4;
	uint8_t alphaFunc : 4;
	uint8_t colorSrc : 4;
	uint8_t colorDst : 4;
	uint8_t alphaSrc : 4;
	uint8_t alphaDst : 4;
} SceGxmBlendInfo;

_Static_assert(sizeof(SceGxmBlendInfo) == 4U, "SceGxmBlendInfo size");

int sceGxmMapVertexUsseMemory(void *base, SceSize size, uint32_t *offset);
int sceGxmMapFragmentUsseMemory(void *base, SceSize size, uint32_t *offset);
int sceGxmCreateContext(const SceGxmContextParams *params, SceGxmContext **context);
int sceGxmCreateRenderTarget(const SceGxmRenderTargetParams *params, SceGxmRenderTarget **renderTarget);
int sceGxmColorSurfaceInit(SceGxmColorSurface *surface, uint32_t colorFormat, uint32_t surfaceType,
                           uint32_t scaleMode, uint32_t outputRegisterSize,
                           uint32_t width, uint32_t height, uint32_t strideInPixels, void *data);
int sceGxmDepthStencilSurfaceInit(SceGxmDepthStencilSurface *surface, uint32_t depthStencilFormat,
                                  uint32_t surfaceType, uint32_t strideInSamples,
                                  void *depthData, void *stencilData);
int sceGxmShaderPatcherCreate(const SceGxmShaderPatcherParams *params, SceGxmShaderPatcher **patcher);
int sceGxmShaderPatcherRegisterProgram(SceGxmShaderPatcher *patcher, const SceGxmProgram *program,
                                       SceGxmShaderPatcherId *programId);
const SceGxmProgramParameter *sceGxmProgramFindParameterByName(const SceGxmProgram *program, const char *name);
uint32_t sceGxmProgramParameterGetCategory(const SceGxmProgramParameter *param);
uint32_t sceGxmProgramParameterGetResourceIndex(const SceGxmProgramParameter *param);
int sceGxmShaderPatcherCreateVertexProgram(SceGxmShaderPatcher *patcher, SceGxmShaderPatcherId programId,
                                           const SceGxmVertexAttribute *attributes, uint32_t attrCount,
                                           const SceGxmVertexStream *streams, uint32_t streamCount,
                                           SceGxmVertexProgram **vertexProgram);
int sceGxmShaderPatcherCreateFragmentProgram(SceGxmShaderPatcher *patcher, SceGxmShaderPatcherId programId,
                                             uint32_t outputFormat, uint32_t multisampleMode,
                                             const SceGxmBlendInfo *blendInfo, const SceGxmProgram *vertexProgram,
                                             SceGxmFragmentProgram **fragmentProgram);
void sceGxmSetVertexProgram(SceGxmContext *context, const SceGxmVertexProgram *vertexProgram);
void sceGxmSetFragmentProgram(SceGxmContext *context, const SceGxmFragmentProgram *fragmentProgram);
int sceGxmSetVertexStream(SceGxmContext *context, uint32_t streamIndex, const void *streamData);
int sceGxmSetFragmentTexture(SceGxmContext *context, uint32_t textureIndex, const SceGxmTexture *texture);
int sceGxmReserveVertexDefaultUniformBuffer(SceGxmContext *context, void **uniformBuffer);
int sceGxmSetUniformDataF(void *uniformBuffer, const SceGxmProgramParameter *param,
                          uint32_t componentOffset, uint32_t componentCount, const float *sourceData);
int sceGxmBeginScene(SceGxmContext *context, uint32_t flags, const SceGxmRenderTarget *renderTarget,
                     const void *validRegion, SceGxmSyncObject *vertexSync, SceGxmSyncObject *fragmentSync,
                     const SceGxmColorSurface *colorSurface, const SceGxmDepthStencilSurface *depthStencil);
int sceGxmEndScene(SceGxmContext *context, const void *vertexNotif, const void *fragmentNotif);
int sceGxmDraw(SceGxmContext *context, uint32_t primType, uint32_t indexType,
               const void *indexData, uint32_t indexCount);
int sceGxmPadHeartbeat(const SceGxmColorSurface *displaySurface, SceGxmSyncObject *displaySync);
void sceGxmFinish(SceGxmContext *context);
int sceGxmDisplayQueueFinish(void);
int sceGxmTextureInitLinear(SceGxmTexture *texture, const void *data, uint32_t texFormat,
                            uint32_t width, uint32_t height, uint32_t mipCount);
int sceGxmTextureSetPalette(SceGxmTexture *texture, const void *paletteData);

typedef struct PocsGxmVertex {
	float    x;
	float    y;
	float    z;
	uint32_t color;
	uint16_t uHalf;
	uint16_t vHalf;
} PocsGxmVertex;

_Static_assert(sizeof(PocsGxmVertex) == 20U, "PocsGxmVertex size");

typedef struct PocsGxmFrameState {
	uint32_t lcdBitmap[32];
	uint32_t ledLit;
} PocsGxmFrameState;

typedef struct PocsGxmTexHeader {
	uint32_t magic;
	uint32_t version;
	uint32_t normalCount;
	uint32_t fullCount;
	char     lzrcMagic[4];
} PocsGxmTexHeader;

typedef struct PocsGxmAesWork {
	SceAesContext aesCtx;
	uint8_t       iv[32];
} PocsGxmAesWork;

static const uint8_t g_pocsTexAesKey[16] = {
	0x5BU, 0x27U, 0xB1U, 0x2FU, 0xCBU, 0x51U, 0x3FU, 0x79U,
	0xA3U, 0xB6U, 0x10U, 0xD9U, 0x12U, 0xB0U, 0x9CU, 0xF5U
};

static const uint8_t g_pocsTexAesIv[16] = {
	0x72U, 0xD8U, 0x59U, 0xC6U, 0x64U, 0x2BU, 0xE6U, 0x5CU,
	0xF0U, 0xBEU, 0x14U, 0x1BU, 0x84U, 0xB9U, 0x3AU, 0xA8U
};

static const uint32_t g_pocsShadowPalette[16] = {
	0x00000000U, 0x01000000U, 0x02000000U, 0x0D000009U,
	0x1B020206U, 0x28020107U, 0x0600000AU, 0x31020207U,
	0x0500000CU, 0x25030208U, 0x36030208U, 0x3A030208U,
	0x11030309U, 0x13040309U, 0x03000011U, 0x0D050509U
};

static const uint32_t g_pocsPixelPalette[16] = {
	0x00000000U, 0x9F040408U, 0xFF000000U, 0x4F040408U,
	0x7F000000U, 0x00000000U, 0x00000000U, 0x00000000U,
	0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U,
	0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U
};

static const uint8_t g_pocsPixelTexDim[8] = {
	0x33U, 0x33U, 0x33U, 0x33U, 0x44U, 0x44U, 0x44U, 0x44U
};

static const uint8_t g_pocsPixelTexLit[8] = {
	0x11U, 0x11U, 0x11U, 0x11U, 0x22U, 0x22U, 0x22U, 0x22U
};

static const float g_pocsIdentityWvp[16] = {
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
};

static const float g_pocsOverlayWvp[16] = {
	0.6f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.6f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.8f, -0.6f, 0.0f, 1.0f
};

static PocsGxmFrameState *g_gxmFrameState;
static SceGxmSyncObject *g_gxmSyncObjects[3];
static uint8_t *g_gxmDdrRoBase;
static PocsGxmVertex *g_gxmVertices;
static uint8_t *g_gxmLedMaskPixels;
static uint32_t g_gxmTexMagic;
static uint32_t g_gxmTexVersion;
static int g_gxmNormalBgCount;
static int g_gxmFullBgCount;
static SceUID g_gxmDrawEventFlag;
static SceGxmTexture g_gxmTexLedLarge;
static SceGxmTexture g_gxmTexLedSmall;
static SceGxmTexture g_gxmTexLedCutout;
static uint32_t g_gxmReqChannel;
static SceGxmDepthStencilSurface g_gxmDepthSurface;
static SceGxmShaderPatcherId g_gxmVertexProgramId;
static uint32_t g_gxmDdrRwOffset;
static SceGxmTexture g_gxmTexPixelLit;
static uint16_t *g_gxmIndices;
static SceGxmTexture g_gxmTexCustomBg;
static uint32_t g_gxmCdramOffset;
static SceGxmShaderPatcher *g_gxmShaderPatcher;
static SceGxmShaderPatcherId g_gxmFragmentProgramId;
static void *g_gxmDisplayBufs[3];
static uint32_t *g_gxmCustomBgBuf;
static uint32_t g_gxmDdrRoOffset;
static SceGxmTexture g_gxmTexPixelDim;
static SceGxmVertexProgram *g_gxmVertexProgram;
static SceGxmContext *g_gxmContext;
static uint32_t g_gxmFrontBufIdx;
static int g_gxmReqBgMode;
static SceGxmFragmentProgram *g_gxmFragmentProgram;
static const SceGxmProgramParameter *g_gxmWvpParam;
static uint32_t g_gxmBackBufIdx;
static uint8_t *g_gxmCdramBase;
static SceGxmTexture g_gxmTexShadow;
static uint8_t *g_gxmBgPackBase;
static int g_gxmReqPresentMode;
static uint8_t *g_gxmDdrRwBase;
static SceGxmTexture g_gxmTexBgActive;
static SceGxmColorSurface g_gxmColorSurfaces[3];
static SceGxmRenderTarget *g_gxmRenderTarget;
static int g_gxmActiveBgIndex = -1;

/* 0x8102a37c */
static int pocsGxmAppendLcdIndices(int indexCursor, const uint32_t *lcdBitmap, uint16_t baseVertex)
{
	uint16_t *indices = g_gxmIndices;
	uint16_t rowBase = baseVertex;
	uint16_t endVertex = (uint16_t)(baseVertex + POCS_GXM_GRID_VERTS);
	int row = 0;

	do {
		uint32_t rowBits = lcdBitmap[row++];
		uint16_t vert = rowBase;
		int col;

		for (col = 0; col < 32; col++) {
			if (((rowBits >> col) & 1U) != 0U) {
				int k;
				for (k = 0; k < 6; k++)
					indices[indexCursor + k] = (uint16_t)(vert + k);
				indexCursor += 6;
			}
			vert = (uint16_t)(vert + 6U);
		}
		rowBase = (uint16_t)(rowBase + 192U);
	} while (rowBase != endVertex);

	return indexCursor;
}

/* 0x8102a3ec */
static int pocsGxmPremultiplyBgPixel(int x, int y)
{
	int pixelIndex = x + (int)POCS_GXM_DISPLAY_WIDTH * y;
	uint32_t px = g_gxmCustomBgBuf[pixelIndex];
	uint32_t alpha = px >> 24;

	if (alpha != 255U) {
		uint32_t r = (alpha * (px & 0x000000FFU)) >> 8;
		uint32_t g = ((alpha * (px & 0x0000FF00U)) >> 8) & 0x0000FF00U;
		uint32_t b = ((alpha * (px & 0x00FF0000U)) >> 8) & 0x00FF0000U;
		g_gxmCustomBgBuf[pixelIndex] = 0xFF000000U | b | g | r;
	}
	return pixelIndex;
}

/* 0x8102a440 */
static void *pocsGxmPatcherHostAlloc(void *userData, SceSize size)
{
	(void)userData;
	return malloc(size);
}

/* 0x8102a44c */
static void pocsGxmPatcherHostFree(void *userData, void *mem)
{
	(void)userData;
	free(mem);
}

/* 0x8102a458 */
static void *pocsGxmAllocMappedMem(const char *name, SceKernelMemBlockType type,
                                   SceSize size, uint32_t attrib)
{
	SceUID uid = sceKernelAllocMemBlock(name, type, size, NULL);
	void *base = NULL;

	if (uid < 0 || sceKernelGetMemBlockBase(uid, &base) != 0 ||
	    sceGxmMapMemory(base, size, attrib) != 0)
		return NULL;
	return base;
}

/* 0x8102a4a8 */
static int pocsGxmDrawTexturedTriangles(const SceGxmTexture *tex, const void *indexData,
                                        uint32_t indexCount)
{
	sceGxmSetFragmentTexture(g_gxmContext, 0U, tex);
	return sceGxmDraw(g_gxmContext, POCS_GXM_PRIMITIVE_TRIANGLES,
	                  POCS_GXM_INDEX_FORMAT_U16, indexData, indexCount);
}

/* 0x8102a4d8 */
static void __attribute__((noreturn)) pocsGxmDrawThread(SceSize args, void *argp)
{
	uint32_t flickerCopy[32];

	(void)args;
	(void)argp;

	for (;;) {
		int bgMode;
		uint32_t channel;
		int presentMode;
		int valid = 1;

		sceKernelWaitEventFlag(g_gxmDrawEventFlag, 2U, SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
		                       NULL, NULL);
		bgMode = g_gxmReqBgMode;
		channel = g_gxmReqChannel;
		presentMode = g_gxmReqPresentMode;

		if (bgMode == 3) {
			popsHostCaptureScreen(g_gxmDisplayBufs[g_gxmBackBufIdx], 0);
			channel = 0U;
		} else if (bgMode == 2) {
			if (bgImageEnsureLoaded() < 0) {
				popsHostSetPocsBgType(0U);
				if (channel >= (uint32_t)g_gxmNormalBgCount)
					valid = 0;
				else
					bgMode = 0;
			} else if (sceGxmTextureInitLinear(&g_gxmTexCustomBg, g_gxmCustomBgBuf,
			                                   POCS_GXM_TEX_FMT_U8U8U8U8_ABGR,
			                                   POCS_GXM_DISPLAY_WIDTH, POCS_GXM_DISPLAY_HEIGHT,
			                                   1U) < 0) {
				valid = 0;
			}
			if (valid && (bgMode == 2 || bgMode == 3))
				channel = 0U;
		} else if (bgMode == 0) {
			if (channel >= (uint32_t)g_gxmNormalBgCount)
				valid = 0;
		} else if (bgMode == 1) {
			if (channel >= (uint32_t)g_gxmFullBgCount) {
				valid = 0;
			} else {
				channel += (uint32_t)g_gxmNormalBgCount;
			}
		} else if ((uint32_t)bgMode > 4U) {
			valid = 0;
		}

		if (valid) {
			if (g_gxmActiveBgIndex != (int)channel) {
				uint32_t totalBgs = (uint32_t)(g_gxmNormalBgCount + g_gxmFullBgCount);
				const uint8_t *texPixels = g_gxmBgPackBase + POCS_GXM_BG_TEX_STRIDE * channel;
				const uint8_t *texPal = g_gxmBgPackBase +
				                        POCS_GXM_BG_TEX_STRIDE * totalBgs + (channel << 10);

				if (sceGxmTextureInitLinear(&g_gxmTexBgActive, texPixels,
				                            POCS_GXM_TEX_FMT_P8_ABGR,
				                            POCS_GXM_DISPLAY_WIDTH, POCS_GXM_DISPLAY_HEIGHT,
				                            1U) < 0 ||
				    sceGxmTextureSetPalette(&g_gxmTexBgActive, texPal) < 0) {
					valid = 0;
				} else {
					g_gxmActiveBgIndex = (int)channel;
				}
			}
		}

		if (valid) {
			uint16_t lcdBaseVert;
			int idxCursor;
			int endLitIdx;
			uint32_t flickerCount = 0U;
			const void *flickerPtr;
			void *uniformBuf = NULL;

			if (bgMode == 1) {
				lcdBaseVert = POCS_GXM_GRID_VERTS;
				idxCursor = 30;
			} else {
				lcdBaseVert = 0U;
				idxCursor = pocsGxmAppendLcdIndices(30, g_gxmFrameState->lcdBitmap, 12306U);
			}

			endLitIdx = pocsGxmAppendLcdIndices(idxCursor, g_gxmFrameState->lcdBitmap, lcdBaseVert);
			flickerPtr = pocsLcdGetFlickerOverlay();
			if (flickerPtr != NULL) {
				memcpy(flickerCopy, flickerPtr, sizeof(flickerCopy));
				flickerCount = (uint32_t)(pocsGxmAppendLcdIndices(endLitIdx, flickerCopy, lcdBaseVert) -
				                          endLitIdx);
			}

			sceGxmBeginScene(g_gxmContext, 0U, g_gxmRenderTarget, NULL, NULL,
			                 g_gxmSyncObjects[g_gxmBackBufIdx],
			                 &g_gxmColorSurfaces[g_gxmBackBufIdx], &g_gxmDepthSurface);
			sceGxmReserveVertexDefaultUniformBuffer(g_gxmContext, &uniformBuf);

			if (bgMode == 3) {
				int quadIdxOff = (remotePocsHasMountedMedia() == 1) ? 18 : 24;
				sceGxmSetUniformDataF(uniformBuf, g_gxmWvpParam, 0U, 16U, g_pocsOverlayWvp);
				pocsGxmDrawTexturedTriangles(&g_gxmTexBgActive, &g_gxmIndices[quadIdxOff], 6U);
			} else {
				sceGxmSetUniformDataF(uniformBuf, g_gxmWvpParam, 0U, 16U, g_pocsIdentityWvp);
				if (bgMode == 2) {
					pocsGxmDrawTexturedTriangles(&g_gxmTexLedCutout, &g_gxmIndices[6], 6U);
					if (g_gxmFrameState->ledLit != 0U)
						pocsGxmDrawTexturedTriangles(&g_gxmTexLedLarge, &g_gxmIndices[6], 6U);
					pocsGxmDrawTexturedTriangles(&g_gxmTexCustomBg, &g_gxmIndices[0], 6U);
					pocsGxmDrawTexturedTriangles(&g_gxmTexBgActive, &g_gxmIndices[24], 6U);
				} else {
					pocsGxmDrawTexturedTriangles(&g_gxmTexBgActive, &g_gxmIndices[0], 6U);
					if (g_gxmFrameState->ledLit != 0U) {
						if (bgMode == 1)
							pocsGxmDrawTexturedTriangles(&g_gxmTexLedSmall, &g_gxmIndices[12], 6U);
						else
							pocsGxmDrawTexturedTriangles(&g_gxmTexLedLarge, &g_gxmIndices[6], 6U);
					}
				}
			}

			if (endLitIdx > 30) {
				uint32_t litCount;
				uint32_t litStart;
				if (bgMode == 1) {
					litCount = (uint32_t)(endLitIdx - 30);
					litStart = 30U;
				} else {
					uint32_t halfCount = (uint32_t)(endLitIdx - 30) >> 1;
					pocsGxmDrawTexturedTriangles(&g_gxmTexShadow, &g_gxmIndices[30], halfCount);
					litCount = halfCount;
					litStart = 30U + halfCount;
				}
				pocsGxmDrawTexturedTriangles(&g_gxmTexPixelLit, &g_gxmIndices[litStart], litCount);
				if (flickerCount != 0U)
					pocsGxmDrawTexturedTriangles(&g_gxmTexPixelDim, &g_gxmIndices[endLitIdx], flickerCount);
			}

			sceGxmEndScene(g_gxmContext, NULL, NULL);
			sceGxmPadHeartbeat(&g_gxmColorSurfaces[g_gxmBackBufIdx], g_gxmSyncObjects[g_gxmBackBufIdx]);

			if (presentMode != 0) {
				if (presentMode > 0) {
					void *cbData = g_gxmDisplayBufs[g_gxmBackBufIdx];
					sceGxmDisplayQueueAddEntry(g_gxmSyncObjects[g_gxmFrontBufIdx],
					                           g_gxmSyncObjects[g_gxmBackBufIdx],
					                           &cbData);
				}
				g_gxmFrontBufIdx = g_gxmBackBufIdx;
				g_gxmBackBufIdx = (g_gxmBackBufIdx + 1U) % 3U;
			}
		}

		sceKernelSetEventFlag(g_gxmDrawEventFlag, 1U);
	}
}

/* 0x8102a9a4 */
static void *pocsGxmAllocPhycont(const char *name, uint32_t size)
{
	SceSize aligned = (size + 4095U) & ~4095U;
	SceUID uid = sceKernelAllocMemBlock(name, PSPEMU_MEMBLOCK_PHYCONT_NC_RW, aligned, NULL);
	void *base = NULL;

	if (uid < 0)
		return NULL;
	if (sceKernelGetMemBlockBase(uid, &base) != 0) {
		sceKernelFreeMemBlock(uid);
		return NULL;
	}
	return base;
}

/* 0x8102aa00 */
static void *pocsGxmAllocFragmentUsse(SceSize size, uint32_t *outOffset)
{
	void *mem = pocsGxmAllocPhycont("ScePocketStationFragment", size);
	if (mem == NULL || sceGxmMapFragmentUsseMemory(mem, size, outOffset) != 0)
		return NULL;
	return mem;
}

/* 0x8102aa2c */
static void *pocsGxmAllocDdrRo(uint32_t size, uint32_t align)
{
	uint32_t alignedOff = (g_gxmDdrRoOffset + align - 1U) & ~(align - 1U);
	g_gxmDdrRoOffset = alignedOff;

	if (size > POCS_GXM_DDR_RO_POOL_SIZE || alignedOff > POCS_GXM_DDR_RO_POOL_SIZE ||
	    alignedOff + size > POCS_GXM_DDR_RO_POOL_SIZE)
		return NULL;

	g_gxmDdrRoOffset = alignedOff + size;
	return g_gxmDdrRoBase + alignedOff;
}

/* 0x8102aa74 */
static __attribute__((noinline, noclone)) void *pocsGxmAllocCdramFrameBuf(void)
{
	uint32_t alignedOff = (g_gxmCdramOffset + 3U) & ~3U;

	if (alignedOff > POCS_GXM_CDRAM_POOL_SIZE ||
	    alignedOff + POCS_GXM_FB_BYTES > POCS_GXM_CDRAM_POOL_SIZE) {
		g_gxmCdramOffset = alignedOff;
		return NULL;
	}

	g_gxmCdramOffset = alignedOff + POCS_GXM_FB_BYTES;
	return g_gxmCdramBase + alignedOff;
}

/* 0x8102aaac */
static int pocsGxmLoadEncryptedAsset(SceUID fd, uint8_t *buf, SceSize fileSize)
{
	PocsGxmAesWork aesWork;
	uint8_t digestOrBlock[32];
	SceSSize bytesRead = sceIoRead(fd, buf, fileSize);
	SceSize payloadSize;
	uint32_t blockBytes;
	SceSize offset;
	int ret;

	sceIoClose(fd);
	if (bytesRead != (SceSSize)fileSize)
		return POCS_GXM_ERR_IO;

	ret = sceAesScheduleKey(&aesWork.aesCtx, 128U, 128U, g_pocsTexAesKey);
	if (ret < 0)
		return ret;

	blockBytes = aesWork.aesCtx.blockWords * 4U;
	memcpy(aesWork.iv, g_pocsTexAesIv, blockBytes);

	payloadSize = fileSize - SCE_SHA256_DIGEST_SIZE;
	for (offset = 0U; offset < payloadSize; offset += 16U) {
		uint8_t *blk = &buf[offset];
		uint32_t i;
		sceAesDecrypt(&aesWork.aesCtx, blk, digestOrBlock);
		blockBytes = aesWork.aesCtx.blockWords * 4U;
		for (i = 0U; i < blockBytes; i++) {
			uint8_t cipherByte = blk[i];
			blk[i] = aesWork.iv[i] ^ digestOrBlock[i];
			aesWork.iv[i] = cipherByte;
		}
	}

	sceSha256Digest(buf, payloadSize, digestOrBlock);
	if (sceClibMemcmpConstTime(digestOrBlock, &buf[payloadSize], SCE_SHA256_DIGEST_SIZE) != 0)
		return POCS_GXM_ERR_IO;
	return 0;
}

/* 0x8102ab9c */
static int pocsGxmInitPalettedTexture(SceGxmTexture *tex, uint32_t width, uint32_t height,
                                      const void *pixelData, const void *paletteData,
                                      uint32_t texFormat)
{
	uint32_t pixelBytes = width * height;
	uint32_t palBytes = 1024U;
	void *texDst;
	void *palDst;
	int ret;

	if (texFormat == POCS_GXM_TEX_FMT_P4_ABGR) {
		pixelBytes >>= 1;
		palBytes = 64U;
	}

	texDst = pocsGxmAllocDdrRo(pixelBytes, 16U);
	if (texDst == NULL)
		return POCS_GXM_ERR_NO_MEMORY;
	memcpy(texDst, pixelData, pixelBytes);

	ret = sceGxmTextureInitLinear(tex, texDst, texFormat, width, height, 1U);
	if (ret != 0)
		return ret;

	palDst = pocsGxmAllocDdrRo(palBytes, 64U);
	if (palDst == NULL)
		return POCS_GXM_ERR_NO_MEMORY;
	memcpy(palDst, paletteData, palBytes);

	return sceGxmTextureSetPalette(tex, palDst);
}

/* 0x8102ac20 */
void *remotePocsGetDefaultFrameBuffer(void)
{
	return g_gxmDisplayBufs[g_gxmFrontBufIdx];
}

/* 0x8102ac38 */
void *remotePocsGetRenderBuffer(void)
{
	return g_gxmDisplayBufs[g_gxmBackBufIdx];
}

/* 0x8102ac50 */
void *remotePocsGetBgTextureBuffer(void)
{
	return g_gxmCustomBgBuf;
}

/* 0x8102ac5c */
void remotePocsClearBgPreviewCache(void)
{
	int bufIdx;
	for (bufIdx = 0; bufIdx < 3; bufIdx++) {
		uint8_t *rowPtr = (uint8_t *)g_gxmDisplayBufs[bufIdx];
		int y;
		for (y = 0; y < (int)POCS_GXM_DISPLAY_HEIGHT; y++) {
			uint32_t *px = (uint32_t *)rowPtr;
			int x;
			for (x = 0; x < (int)POCS_GXM_DISPLAY_WIDTH; x++)
				px[x] = 0xFF000000U;
			rowPtr += POCS_GXM_DISPLAY_STRIDE * 4U;
		}
	}
}

/* 0x8102ac94 */
int remotePocsInstallAppSupport(void)
{
	SceIoStat stat;
	PocsGxmTexHeader *hdr;
	SceUID fd;
	const void *lzrcEnd = NULL;
	int ret;

	ret = sceIoGetstat("ux0:ps1emu/PCSC80018/texture.enc", &stat);
	if (ret < 0)
		return ret;
	if (stat.st_size < 33 || stat.st_size > 0x27EC0F)
		return POCS_GXM_ERR_BAD_SIZE;

	hdr = (PocsGxmTexHeader *)malloc((size_t)stat.st_size);
	if (hdr == NULL)
		return POCS_GXM_ERR_NO_MEMORY;

	fd = sceIoOpen("ux0:ps1emu/PCSC80018/texture.enc", SCE_O_RDONLY, 0);
	if (fd < 0) {
		free(hdr);
		return fd;
	}

	ret = pocsGxmLoadEncryptedAsset(fd, (uint8_t *)hdr, (SceSize)stat.st_size);
	if (ret < 0) {
		free(hdr);
		return ret;
	}

	g_gxmTexMagic = hdr->magic;
	g_gxmTexVersion = hdr->version;
	g_gxmNormalBgCount = (int)hdr->normalCount;
	g_gxmFullBgCount = (int)hdr->fullCount;

	if (hdr->magic != POCS_GXM_TEX_MAGIC || hdr->version != 1U ||
	    hdr->normalCount != 4U || hdr->fullCount != 1U) {
		free(hdr);
		return POCS_GXM_ERR_IO;
	}

	g_gxmBgPackBase = (uint8_t *)pocsGxmAllocMappedMem("ScePocketStationBgTexture",
	                                                   PSPEMU_MEMBLOCK_PHYCONT_NC_RW,
	                                                   0x27F000U, POCS_GXM_MEM_ATTRIB_READ);
	if (g_gxmBgPackBase == NULL) {
		free(hdr);
		return POCS_GXM_ERR_NO_MEMORY;
	}

	if (memcmp("ARZL", hdr->lzrcMagic, 4U) != 0) {
		free(hdr);
		return POCS_GXM_ERR_IO;
	}

	ret = sceLzrcDecompress(g_gxmBgPackBase, POCS_GXM_BG_UNPACK_SIZE,
	                        (const uint8_t *)(hdr + 1), &lzrcEnd);
	free(hdr);
	if (ret < 0)
		return ret;

	ret = pocsGxmInitPalettedTexture(&g_gxmTexPixelLit, 16U, 1U,
	                                 g_pocsPixelTexLit, g_pocsPixelPalette,
	                                 POCS_GXM_TEX_FMT_P4_ABGR);
	if (ret != 0)
		return ret;

	ret = pocsGxmInitPalettedTexture(&g_gxmTexPixelDim, 16U, 1U,
	                                 g_pocsPixelTexDim, g_pocsPixelPalette,
	                                 POCS_GXM_TEX_FMT_P4_ABGR);
	if (ret != 0)
		return ret;

	ret = pocsGxmInitPalettedTexture(&g_gxmTexShadow, 16U, 15U,
	                                 g_pocsLedShadowData, g_pocsShadowPalette,
	                                 POCS_GXM_TEX_FMT_P4_ABGR);
	if (ret != 0)
		return ret;

	ret = pocsGxmInitPalettedTexture(&g_gxmTexLedLarge, POCS_GXM_LED_LARGE_W, POCS_GXM_LED_LARGE_H,
	                                 g_pocsLedLargeData, &g_pocsLedLargeData[0x1EE0],
	                                 POCS_GXM_TEX_FMT_P8_ABGR);
	if (ret != 0)
		return ret;

	ret = pocsGxmInitPalettedTexture(&g_gxmTexLedSmall, POCS_GXM_LED_SMALL_W, POCS_GXM_LED_SMALL_H,
	                                 g_pocsLedSmallData, &g_pocsLedSmallData[0x300],
	                                 POCS_GXM_TEX_FMT_P8_ABGR);
	if (ret != 0)
		return ret;

	g_gxmLedMaskPixels = (uint8_t *)pocsGxmAllocDdrRo(0x1EE0U, 16U);
	if (g_gxmLedMaskPixels == NULL)
		return POCS_GXM_ERR_NO_MEMORY;

	/* Build top-border cutout mask matching opaque silhouette of large LED */
	{
		const uint32_t *ledPal = (const uint32_t *)(const void *)&g_pocsLedLargeData[0x1EE0];
		int col;

		for (col = 0; col < (int)POCS_GXM_LED_LARGE_W; col++) {
			int row;
			int opaqueSeen = 0;
			for (row = (int)POCS_GXM_LED_LARGE_H - 1; row >= 0; row--) {
				int idx = row * (int)POCS_GXM_LED_LARGE_W + col;
				if (!opaqueSeen && (ledPal[g_pocsLedLargeData[idx]] >> 24) <= 0x18U) {
					g_gxmLedMaskPixels[idx] = 0U;
				} else {
					opaqueSeen = 1;
					g_gxmLedMaskPixels[idx] = g_gxmBgPackBase[row * (int)POCS_GXM_DISPLAY_WIDTH + col + 376];
				}
			}
		}
	}

	ret = sceGxmTextureInitLinear(&g_gxmTexLedCutout, g_gxmLedMaskPixels,
	                              POCS_GXM_TEX_FMT_P8_ABGR,
	                              POCS_GXM_LED_LARGE_W, POCS_GXM_LED_LARGE_H, 1U);
	if (ret == 0) {
		uint32_t palOff = POCS_GXM_BG_TEX_STRIDE * (uint32_t)(g_gxmNormalBgCount + g_gxmFullBgCount);
		ret = sceGxmTextureSetPalette(&g_gxmTexLedCutout, g_gxmBgPackBase + palOff);
	}
	return ret;
}

static inline __attribute__((always_inline)) void pocsGxmSetQuad(
	PocsGxmVertex *verts, uint16_t *indices, uint16_t baseVertex,
	float xLeft, float xRight, float yTop, float yBottom,
	uint16_t uLeft, uint16_t uRight, uint16_t vTop, uint16_t vBottom)
{
	int k;

	verts[0].x = xLeft;  verts[0].y = yTop;    verts[0].uHalf = uLeft;  verts[0].vHalf = vTop;
	verts[1].x = xRight; verts[1].y = yTop;    verts[1].uHalf = uRight; verts[1].vHalf = vTop;
	verts[2].x = xLeft;  verts[2].y = yBottom; verts[2].uHalf = uLeft;  verts[2].vHalf = vBottom;
	verts[3].x = xRight; verts[3].y = yBottom; verts[3].uHalf = uRight; verts[3].vHalf = vBottom;
	verts[4].x = xLeft;  verts[4].y = yBottom; verts[4].uHalf = uLeft;  verts[4].vHalf = vBottom;
	verts[5].x = xRight; verts[5].y = yTop;    verts[5].uHalf = uRight; verts[5].vHalf = vTop;

	for (k = 0; k < 6; k++) {
		verts[k].z = 0.0f;
		verts[k].color = 0xFFFFFFFFU;
		if (indices != NULL)
			indices[k] = (uint16_t)(baseVertex + k);
	}
}

/* 0x8102afb4 */
int pocsGxmInit(void *gxmFramebufferState)
{
	void *vdmRing;
	void *vtxRing;
	void *fragRing;
	void *fragUsseRing;
	uint32_t fragUsseOff = 0U;
	uint32_t vtxUsseOff = 0U;
	SceGxmContextParams ctxParams;
	SceGxmRenderTargetParams rtParams;
	SceGxmShaderPatcherParams patcherParams;
	SceGxmVertexAttribute attrs[3];
	SceGxmVertexStream stream;
	SceGxmBlendInfo blend;
	void *depthMem;
	void *patcherBuf;
	void *vtxUsseMem;
	void *fragUsseMem;
	const SceGxmProgram *vtxProg = (const SceGxmProgram *)(const void *)g_pocsVertexShaderGxp;
	const SceGxmProgram *fragProg = (const SceGxmProgram *)(const void *)g_pocsFragmentShaderGxp;
	const SceGxmProgramParameter *paramPos;
	const SceGxmProgramParameter *paramColor;
	const SceGxmProgramParameter *paramTex;
	int i;
	int ret;

	if (g_gxmFrameState != NULL)
		return 0;

	g_gxmCdramBase = (uint8_t *)pocsGxmAllocMappedMem("ScePocketStationCommonCDRAM",
	                                                  PSPEMU_MEMBLOCK_CDRAM_RW,
	                                                  POCS_GXM_CDRAM_POOL_SIZE, POCS_GXM_MEM_ATTRIB_RW);
	g_gxmDdrRwBase = (uint8_t *)pocsGxmAllocMappedMem("ScePocketStationCommonDdrRW",
	                                                  PSPEMU_MEMBLOCK_PHYCONT_NC_RW,
	                                                  POCS_GXM_DDR_RW_POOL_SIZE, POCS_GXM_MEM_ATTRIB_RW);
	g_gxmDdrRoBase = (uint8_t *)pocsGxmAllocMappedMem("ScePocketStationCommonDdrRO",
	                                                  PSPEMU_MEMBLOCK_PHYCONT_NC_RW,
	                                                  POCS_GXM_DDR_RO_POOL_SIZE, POCS_GXM_MEM_ATTRIB_READ);
	if (g_gxmCdramBase == NULL || g_gxmDdrRwBase == NULL || g_gxmDdrRoBase == NULL)
		return POCS_GXM_ERR_NO_MEMORY;

	vdmRing = pocsGxmAllocDdrRo(0x20000U, 4U);
	vtxRing = pocsGxmAllocDdrRo(0x200000U, 4U);
	fragRing = pocsGxmAllocDdrRo(0x80000U, 4U);
	fragUsseRing = pocsGxmAllocFragmentUsse(0x4000U, &fragUsseOff);
	if (vdmRing == NULL || vtxRing == NULL || fragRing == NULL || fragUsseRing == NULL)
		return POCS_GXM_ERR_NO_MEMORY;

	memset(&ctxParams, 0, sizeof(ctxParams));
	ctxParams.hostMem = malloc(0x800U);
	ctxParams.hostMemSize = 0x800U;
	ctxParams.vdmRingBufferMem = vdmRing;
	ctxParams.vdmRingBufferMemSize = 0x20000U;
	ctxParams.vertexRingBufferMem = vtxRing;
	ctxParams.vertexRingBufferMemSize = 0x200000U;
	ctxParams.fragmentRingBufferMem = fragRing;
	ctxParams.fragmentRingBufferMemSize = 0x80000U;
	ctxParams.fragmentUsseRingBufferMem = fragUsseRing;
	ctxParams.fragmentUsseRingBufferMemSize = 0x4000U;
	ctxParams.fragmentUsseRingBufferOffset = fragUsseOff;

	ret = sceGxmCreateContext(&ctxParams, &g_gxmContext);
	if (ret < 0)
		return ret;

	memset(&rtParams, 0, sizeof(rtParams));
	rtParams.width = POCS_GXM_DISPLAY_WIDTH;
	rtParams.height = POCS_GXM_DISPLAY_HEIGHT;
	rtParams.scenesPerFrame = 1U;
	rtParams.multisampleMode = POCS_GXM_MULTISAMPLE_NONE;
	rtParams.driverMemBlock = -1;

	ret = sceGxmCreateRenderTarget(&rtParams, &g_gxmRenderTarget);
	if (ret < 0)
		return ret;

	for (i = 0; i < 3; i++) {
		g_gxmDisplayBufs[i] = pocsGxmAllocCdramFrameBuf();
		if (g_gxmDisplayBufs[i] == NULL)
			return POCS_GXM_ERR_NO_MEMORY;
		ret = sceGxmColorSurfaceInit(&g_gxmColorSurfaces[i],
		                             PSPEMU_GXM_COLOR_FORMAT_U8U8U8U8_ABGR,
		                             PSPEMU_GXM_COLOR_SURFACE_LINEAR,
		                             POCS_GXM_COLOR_SCALE_NONE,
		                             POCS_GXM_OUT_REG_SIZE_32BIT,
		                             POCS_GXM_DISPLAY_WIDTH, POCS_GXM_DISPLAY_HEIGHT,
		                             POCS_GXM_DISPLAY_STRIDE, g_gxmDisplayBufs[i]);
		if (ret != 0)
			return ret;
		ret = sceGxmSyncObjectCreate(&g_gxmSyncObjects[i]);
		if (ret != 0)
			return ret;
	}
	remotePocsClearBgPreviewCache();

	g_gxmDdrRwOffset = (g_gxmDdrRwOffset + 15U) & ~15U;
	if (g_gxmDdrRwOffset > POCS_GXM_DDR_RW_POOL_SIZE ||
	    g_gxmDdrRwOffset + POCS_GXM_DEPTH_BYTES > POCS_GXM_DDR_RW_POOL_SIZE)
		return POCS_GXM_ERR_NO_MEMORY;
	depthMem = g_gxmDdrRwBase + g_gxmDdrRwOffset;
	g_gxmDdrRwOffset += POCS_GXM_DEPTH_BYTES;

	ret = sceGxmDepthStencilSurfaceInit(&g_gxmDepthSurface, POCS_GXM_DEPTH_FMT_S8D24,
	                                    POCS_GXM_DEPTH_SURFACE_TILED, POCS_GXM_DISPLAY_WIDTH,
	                                    depthMem, NULL);
	if (ret < 0)
		return ret;

	g_gxmDdrRwOffset = (g_gxmDdrRwOffset + 3U) & ~3U;
	if (g_gxmDdrRwOffset > POCS_GXM_DDR_RW_POOL_SIZE ||
	    g_gxmDdrRwOffset + 0x10000U > POCS_GXM_DDR_RW_POOL_SIZE) {
		patcherBuf = NULL;
	} else {
		patcherBuf = g_gxmDdrRwBase + g_gxmDdrRwOffset;
		g_gxmDdrRwOffset += 0x10000U;
	}

	vtxUsseMem = pocsGxmAllocPhycont("ScePocketStationVertex", 0x10000U);
	if (vtxUsseMem == NULL || sceGxmMapVertexUsseMemory(vtxUsseMem, 0x10000U, &vtxUsseOff) != 0) {
		pocsGxmAllocFragmentUsse(0x10000U, &fragUsseOff);
		return POCS_GXM_ERR_NO_MEMORY;
	}
	fragUsseMem = pocsGxmAllocFragmentUsse(0x10000U, &fragUsseOff);
	if (patcherBuf == NULL || fragUsseMem == NULL)
		return POCS_GXM_ERR_NO_MEMORY;

	memset(&patcherParams, 0, sizeof(patcherParams));
	patcherParams.hostAllocCallback = pocsGxmPatcherHostAlloc;
	patcherParams.hostFreeCallback = pocsGxmPatcherHostFree;
	patcherParams.bufferMem = patcherBuf;
	patcherParams.bufferMemSize = 0x10000U;
	patcherParams.vertexUsseMem = vtxUsseMem;
	patcherParams.vertexUsseMemSize = 0x10000U;
	patcherParams.vertexUsseOffset = vtxUsseOff;
	patcherParams.fragmentUsseMem = fragUsseMem;
	patcherParams.fragmentUsseMemSize = 0x10000U;
	patcherParams.fragmentUsseOffset = fragUsseOff;

	ret = sceGxmShaderPatcherCreate(&patcherParams, &g_gxmShaderPatcher);
	if (ret == 0)
		ret = sceGxmShaderPatcherRegisterProgram(g_gxmShaderPatcher, vtxProg, &g_gxmVertexProgramId);
	if (ret == 0)
		ret = sceGxmShaderPatcherRegisterProgram(g_gxmShaderPatcher, fragProg, &g_gxmFragmentProgramId);
	if (ret < 0)
		return ret;

	paramPos = sceGxmProgramFindParameterByName(vtxProg, "aPosition");
	paramColor = sceGxmProgramFindParameterByName(vtxProg, "aColor");
	paramTex = sceGxmProgramFindParameterByName(vtxProg, "aTexCoord");
	if (paramPos == NULL || sceGxmProgramParameterGetCategory(paramPos) != POCS_GXM_PARAM_CAT_ATTRIBUTE ||
	    paramColor == NULL || sceGxmProgramParameterGetCategory(paramColor) != POCS_GXM_PARAM_CAT_ATTRIBUTE ||
	    paramTex == NULL || sceGxmProgramParameterGetCategory(paramTex) != POCS_GXM_PARAM_CAT_ATTRIBUTE)
		return -1;

	attrs[0].streamIndex = 0U;
	attrs[0].offset = 0U;
	attrs[0].format = POCS_GXM_ATTR_FMT_F32;
	attrs[0].componentCount = 3U;
	attrs[0].regIndex = (uint16_t)sceGxmProgramParameterGetResourceIndex(paramPos);

	attrs[1].streamIndex = 0U;
	attrs[1].offset = 12U;
	attrs[1].format = POCS_GXM_ATTR_FMT_U8N;
	attrs[1].componentCount = 4U;
	attrs[1].regIndex = (uint16_t)sceGxmProgramParameterGetResourceIndex(paramColor);

	attrs[2].streamIndex = 0U;
	attrs[2].offset = 16U;
	attrs[2].format = POCS_GXM_ATTR_FMT_F16;
	attrs[2].componentCount = 2U;
	attrs[2].regIndex = (uint16_t)sceGxmProgramParameterGetResourceIndex(paramTex);

	stream.stride = sizeof(PocsGxmVertex);
	stream.indexSource = 0U;

	ret = sceGxmShaderPatcherCreateVertexProgram(g_gxmShaderPatcher, g_gxmVertexProgramId,
	                                             attrs, 3U, &stream, 1U, &g_gxmVertexProgram);
	if (ret < 0)
		return ret;

	blend.colorMask = 0x0FU;
	blend.colorFunc = 1U;
	blend.alphaFunc = 1U;
	blend.colorSrc = 4U;
	blend.colorDst = 5U;
	blend.alphaSrc = 4U;
	blend.alphaDst = 5U;

	ret = sceGxmShaderPatcherCreateFragmentProgram(g_gxmShaderPatcher, g_gxmFragmentProgramId,
	                                               POCS_GXM_OUT_REG_FMT_UCHAR4,
	                                               POCS_GXM_MULTISAMPLE_NONE, &blend,
	                                               vtxProg, &g_gxmFragmentProgram);
	if (ret < 0)
		return ret;

	g_gxmWvpParam = sceGxmProgramFindParameterByName(vtxProg, "wvp");
	if (g_gxmWvpParam == NULL ||
	    sceGxmProgramParameterGetCategory(g_gxmWvpParam) != POCS_GXM_PARAM_CAT_UNIFORM)
		return -1;

	g_gxmVertices = (PocsGxmVertex *)pocsGxmAllocDdrRo(POCS_GXM_VERTEX_BUF_SIZE, 4U);
	g_gxmIndices = (uint16_t *)pocsGxmAllocDdrRo(POCS_GXM_INDEX_BUF_SIZE, 2U);
	if (g_gxmVertices == NULL || g_gxmIndices == NULL)
		return POCS_GXM_ERR_NO_MEMORY;

	/* Build 32x32 pixel grids for normal mode [0..6143] and fullscreen mode [6144..12287] */
	{
		int mode;
		for (mode = 0; mode < 2; mode++) {
			uint16_t uTop = (mode != 0) ? POCS_GXM_HALF_HALF : POCS_GXM_HALF_ZERO;
			uint16_t uBot = (mode != 0) ? POCS_GXM_HALF_ONE : POCS_GXM_HALF_HALF;
			int row;

			for (row = 0; row < 32; row++) {
				float rowHi = (float)(32 - row);
				float rowLo = (float)(31 - row);
				int col;

				for (col = 0; col < 32; col++) {
					float xL, xR, yT, yB;
					int vertBase = mode * (int)POCS_GXM_GRID_VERTS + (row * 32 + col) * 6;

					if (mode != 0) {
						xL = (float)col * 0.035416666f - 0.56666666f;
						xR = (float)(col + 1) * 0.035416666f - 0.56666666f;
						yT = rowHi * 0.0625f - 1.0f;
						yB = rowLo * 0.0625f - 1.0f;
					} else {
						xL = (float)col * 0.020833334f - 0.33333334f;
						xR = ((float)(col + 1) * 0.020833334f - 0.33333334f) - 0.0020833334f;
						yT = (rowHi * 0.036764704f - 0.6691176f) - 0.0036764704f;
						yB = rowLo * 0.036764704f - 0.6691176f;
					}

					pocsGxmSetQuad(&g_gxmVertices[vertBase], NULL, 0U,
					               xL, xR, yT, yB, uTop, uTop, POCS_GXM_HALF_ZERO, POCS_GXM_HALF_ZERO);
					g_gxmVertices[vertBase + 2].uHalf = uBot;
					g_gxmVertices[vertBase + 3].uHalf = uBot;
					g_gxmVertices[vertBase + 4].uHalf = uBot;
				}
			}
		}
	}

	/* Fullscreen BG quad [12288..12293], indices [0..5] */
	pocsGxmSetQuad(&g_gxmVertices[12288], &g_gxmIndices[0], 12288U,
	               -1.0f, 1.0f, 1.0f, -1.0f,
	               POCS_GXM_HALF_ZERO, POCS_GXM_HALF_ONE, POCS_GXM_HALF_ZERO, POCS_GXM_HALF_ONE);

	/* Large top LED quad [12294..12299], indices [6..11] */
	pocsGxmSetQuad(&g_gxmVertices[12294], &g_gxmIndices[6], 12294U,
	               -0.21145834f, 0.21145834f, 1.0f, 0.860294104f,
	               POCS_GXM_HALF_ZERO, POCS_GXM_HALF_ONE, POCS_GXM_HALF_ZERO, POCS_GXM_HALF_ONE);

	/* Small side LED quad [12300..12305], indices [12..17] */
	pocsGxmSetQuad(&g_gxmVertices[12300], &g_gxmIndices[12], 12300U,
	               0.735416651f, 0.835416675f, -0.0257352944f, -0.0845588222f,
	               POCS_GXM_HALF_ZERO, POCS_GXM_HALF_ONE, POCS_GXM_HALF_ZERO, POCS_GXM_HALF_ONE);

	/* 32x32 LCD drop-shadow quads [12306..18449] */
	{
		int row;
		for (row = 0; row < 32; row++) {
			float yBase = ((float)(31 - row) * 0.036764704f - 0.6691176f) - 0.0036764706f;
			float yTop = (yBase - 0.05514706f) + 0.036764704f;
			float yBot = yBase + 0.036764704f;
			int col;

			for (col = 0; col < 32; col++) {
				float xL = (float)col * 0.020833334f - 0.33333334f;
				float xR = xL + 0.033333335f;
				int vertBase = 12306 + (row * 32 + col) * 6;
				pocsGxmSetQuad(&g_gxmVertices[vertBase], NULL, 0U,
				               xL, xR, yTop, yBot,
				               POCS_GXM_HALF_ZERO, POCS_GXM_HALF_ONE,
				               POCS_GXM_HALF_ZERO, POCS_GXM_HALF_ONE);
			}
		}
	}

	/* Mini overlay quads [18450..18455] (inserted) and [18456..18461] (ejected) */
	pocsGxmSetQuad(&g_gxmVertices[18450], &g_gxmIndices[18], 18450U,
	               0.333332986f, -0.333332986f, -0.669117987f, 0.507353008f,
	               13653U, 14677U, 13282U, 15021U);

	pocsGxmSetQuad(&g_gxmVertices[18456], &g_gxmIndices[24], 18456U,
	               -0.333332986f, 0.333332986f, 0.507353008f, -0.669117987f,
	               13653U, 14677U, 13282U, 15021U);

	if (pspemuIsPocketStationInstalled() != 0) {
		ret = remotePocsInstallAppSupport();
		if (ret < 0)
			return ret;
	}

	g_gxmFrontBufIdx = 0U;
	g_gxmBackBufIdx = 0U;
	g_gxmCustomBgBuf = (uint32_t *)pocsGxmAllocCdramFrameBuf();
	if (g_gxmCustomBgBuf == NULL)
		return POCS_GXM_ERR_NO_MEMORY;

	sceGxmSetVertexProgram(g_gxmContext, g_gxmVertexProgram);
	sceGxmSetFragmentProgram(g_gxmContext, g_gxmFragmentProgram);
	sceGxmSetVertexStream(g_gxmContext, 0U, g_gxmVertices);

	g_gxmDrawEventFlag = sceKernelCreateEventFlag("ScePocketStationDrawLcd", 0x1000U, 1U, NULL);
	if (g_gxmDrawEventFlag < 0)
		pspemuTriggerFatalError(0);

	ret = sceKernelCreateThread("ScePocketStationDrawLcd",
	                            (SceKernelThreadEntry)pocsGxmDrawThread,
	                            125, 0x1000U, 0, 0x10000, NULL);
	if (ret < 0)
		return ret;

	ret = sceKernelStartThread(ret, 0, NULL);
	if (ret < 0)
		return ret;

	g_gxmFrameState = (PocsGxmFrameState *)gxmFramebufferState;
	return 0;
}

/* 0x8102be14 */
int pocsGxmGetChannelCount(int bgType)
{
	switch (bgType) {
	case 0:
	case 2:
	case 3:
		return g_gxmNormalBgCount;
	case 1:
		return g_gxmFullBgCount;
	default:
		return 0;
	}
}

/* 0x8102be3c */
int pocsGxmRequestRender(int bgType, int channel, int presentMode)
{
	int ret = sceKernelWaitEventFlag(g_gxmDrawEventFlag, 1U, SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
	                                 NULL, NULL);
	if (ret >= 0) {
		g_gxmReqBgMode = bgType;
		g_gxmReqChannel = (uint32_t)channel;
		g_gxmReqPresentMode = presentMode;
		return sceKernelSetEventFlag(g_gxmDrawEventFlag, 2U);
	}
	return ret;
}

/* 0x8102be94 */
int pocsGxmWaitFinish(void)
{
	sceKernelWaitEventFlag(g_gxmDrawEventFlag, 1U, 0U, NULL, NULL);
	sceGxmFinish(g_gxmContext);
	sceGxmDisplayQueueFinish();
	return 0;
}

/* 0x8102bec8 */
void remotePocsCommitBgTexture(void)
{
	int y;
	int x;

	for (y = 0; y < (int)POCS_GXM_LED_LARGE_H; y++) {
		for (x = 0; x < (int)POCS_GXM_LED_LARGE_W; x++) {
			if (g_gxmLedMaskPixels[y * (int)POCS_GXM_LED_LARGE_W + x] == 0U)
				pocsGxmPremultiplyBgPixel(x + 378, y);
		}
	}

	for (y = 0; y < (int)POCS_GXM_LED_LARGE_H; y++) {
		for (x = 0; x < 378; x++)
			pocsGxmPremultiplyBgPixel(x, y);
		for (x = 586; x < (int)POCS_GXM_DISPLAY_WIDTH; x++)
			pocsGxmPremultiplyBgPixel(x, y);
	}

	for (y = (int)POCS_GXM_LED_LARGE_H; y < (int)POCS_GXM_DISPLAY_HEIGHT; y++) {
		for (x = 0; x < (int)POCS_GXM_DISPLAY_WIDTH; x++)
			pocsGxmPremultiplyBgPixel(x, y);
	}
}
