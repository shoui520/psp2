/*
 * SceGxm: render targets, the driver's per-surface tiling and parameter state.
 */

#include "gxm_internal.h"

typedef struct SceGxmRenderTargetParams {
	SceUInt32 flags;		/* SCE_GXM_RENDER_TARGET_* */
	SceUInt16 width;
	SceUInt16 height;
	SceUInt16 scenesPerFrame;
	SceUInt16 multisampleMode;
	SceUInt32 multisampleLocations;
	SceUID driverMemBlock;		/* SCE_UID_INVALID_UID: allocated here */
} SceGxmRenderTargetParams;

#define SCE_GXM_RENDER_TARGET_CUSTOM_MULTISAMPLE_LOCATIONS	0x00000001U
#define SCE_GXM_RENDER_TARGET_MACROTILE_SYNC			0x00000002U
#define SCE_GXM_RENDER_TARGET_USE_DISPLAY_QUEUE_PARAMS		0x00000010U
#define SCE_GXM_RENDER_TARGET_MACROTILE_COUNT_X_SHIFT		8
#define SCE_GXM_RENDER_TARGET_MACROTILE_COUNT_Y_SHIFT		12

/* the system's render targets, counted by the driver apart from the application's */
#define GXM_RENDER_TARGET_SYSTEM_POOL		0x00000004U
#define GXM_RENDER_TARGET_SYSTEM_SINGLE		0x00000008U

/* what the public functions accept */
#define GXM_RENDER_TARGET_PUBLIC_MASK		0x0000ff13U
/* and the internal ones */
#define GXM_RENDER_TARGET_INTERNAL_MASK		0x0000ff1fU

static inline SceUInt32 gxmMacrotileCountX(SceUInt32 flags)
{
	return (flags >> SCE_GXM_RENDER_TARGET_MACROTILE_COUNT_X_SHIFT) & 0xf;
}

static inline SceUInt32 gxmMacrotileCountY(SceUInt32 flags)
{
	return (SceUInt16)flags >> SCE_GXM_RENDER_TARGET_MACROTILE_COUNT_Y_SHIFT;
}

static inline SceUInt32 gxmNextPowerOfTwo(SceUInt32 value)
{
	value--;
	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;
	return value + 1;
}

/* an allocation that does not cross a 4 KiB page when it fits in one */
static inline SceSize gxmPageFit(SceSize offset, SceSize size)
{
	SceSize page = (offset + 0xfff) & ~0xfff;

	if (page - offset < size)
		offset = page;
	return offset;
}

static __attribute__((noinline)) int gxmRenderTargetCheckParams(const SceGxmRenderTargetParams *params)
{
	SceUInt32 flags, width, height, multisampleMode, countX, countY, macrotileHeight;

	if (params == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	flags = params->flags;
	if (flags & ~GXM_RENDER_TARGET_INTERNAL_MASK)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if ((flags & (GXM_RENDER_TARGET_SYSTEM_POOL | GXM_RENDER_TARGET_SYSTEM_SINGLE)) ==
	    (GXM_RENDER_TARGET_SYSTEM_POOL | GXM_RENDER_TARGET_SYSTEM_SINGLE))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if ((flags & SCE_GXM_RENDER_TARGET_USE_DISPLAY_QUEUE_PARAMS) &&
	    (!gxmGlobals.initialized || gxmGlobals.displayQueue == NULL))
		return SCE_GXM_ERROR_INVALID_VALUE;
	width = params->width;
	if (width - 1 >= 0x1000)
		return SCE_GXM_ERROR_INVALID_VALUE;
	height = params->height;
	if (height - 1 >= 0x1000)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (params->scenesPerFrame - 1U > 7)
		return SCE_GXM_ERROR_INVALID_VALUE;
	multisampleMode = params->multisampleMode;
	if (multisampleMode > SCE_GXM_MULTISAMPLE_4X)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (!(flags & SCE_GXM_RENDER_TARGET_MACROTILE_SYNC)) {
		if (flags & 0xff00)
			return SCE_GXM_ERROR_INVALID_VALUE;
		return 0;
	}

	/* 2 to 16 macrotiles of whole 128-pixel tiles, at most 1024 pixels each, no MSAA */
	countX = gxmMacrotileCountX(flags);
	countY = gxmMacrotileCountY(flags);
	if (countX - 1 > 3 || countY - 1 > 3)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (countX <= 1 && countY <= 1)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (width % (countX << 7) != 0 || height % (countY << 7) != 0)
		return SCE_GXM_ERROR_INVALID_VALUE;
	macrotileHeight = height / countY;
	if (width / countX > 0x400 || macrotileHeight > 0x400)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (multisampleMode != SCE_GXM_MULTISAMPLE_NONE)
		return SCE_GXM_ERROR_INVALID_VALUE;
	return 0;
}

/* the driver memory a render target needs: the kernel's layout of its structures */
static void gxmRenderTargetGetDriverMemSize(const SceGxmRenderTargetParams *params, SceSize *size)
{
	SceUInt32 flags, rtDataCount, queuedRenders, tilesX, tilesY, samplesX, samplesY;
	SceUInt32 macrotileWidth, macrotileHeight, macrotiles, tileCountX, tileCountY;
	SceUInt32 regionsSize, stateSize, i, j;
	SceSize hostSize, renderSize, renderDetails, renderData, controlSize, regionSize, tailSize;
	SceSize rtDataSize, macrotileSize;

	flags = params->flags;
	rtDataCount = params->scenesPerFrame * 2;
	queuedRenders = 3;
	if (flags & SCE_GXM_RENDER_TARGET_USE_DISPLAY_QUEUE_PARAMS) {
		queuedRenders = gxmDisplayQueueGetMaxPending(gxmGlobals.displayQueue) + 1;
		flags = params->flags;
	}
	queuedRenders *= params->scenesPerFrame;
	tilesX = (params->width + 31U) >> 5;
	tilesY = (params->height + 31U) >> 5;
	if (params->multisampleMode == SCE_GXM_MULTISAMPLE_2X) {
		samplesX = 1;
		samplesY = 2;
	} else if (params->multisampleMode == SCE_GXM_MULTISAMPLE_4X) {
		samplesY = 2;
		samplesX = 2;
	} else {
		samplesY = 1;
		samplesX = 1;
	}
	if (flags & SCE_GXM_RENDER_TARGET_MACROTILE_SYNC) {
		SceUInt32 countX = gxmMacrotileCountX(flags);
		SceUInt32 countY = gxmMacrotileCountY(flags);

		macrotileWidth = ((tilesX - 1 + countX) / countX + 3) & ~3;
		macrotileHeight = ((tilesY - 1 + countY) / countY + 3) & ~3;
		macrotiles = (countX > 2 || countY > 2) ? 4 : 2;
	} else {
		macrotiles = 4;
		macrotileWidth = (((tilesX + 3) >> 2) + 3) & ~3;
		macrotileHeight = (((tilesY + 3) >> 2) + 3) & ~3;
	}

	/* the queued renders' details and data */
	hostSize = rtDataCount * 80 + 320;
	renderDetails = 0;
	renderData = 0;
	if (queuedRenders != 0) {
		for (i = 0; i < queuedRenders; i++) {
			renderDetails = gxmPageFit(renderDetails, 0x260) + 0x260;
			renderData = gxmPageFit(renderData, 16) + 32;
			renderDetails = gxmPageFit(renderDetails, 0x40) + 0x40;
		}
		hostSize += i * 720;
		renderSize = ((renderData + 0xfff) & ~0xfff) + ((renderDetails + 0xfff) & ~0xfff) +
			     0x2000;
	} else {
		renderSize = 0x2000;
	}
	hostSize += 1280;

	/* the render target data sets, the tail pointers and the region headers */
	controlSize = rtDataCount * 160 + 32;
	tileCountX = macrotileWidth * macrotiles * samplesX;
	if (controlSize < 16)
		controlSize = 16;
	controlSize = (controlSize + 31) & ~31;
	tileCountY = macrotileHeight * macrotiles * samplesY;
	regionSize = gxmPageFit(32, controlSize) + controlSize;
	regionsSize = tileCountX * tileCountY * 12;
	tailSize = gxmNextPowerOfTwo(tileCountX);
	if (gxmNextPowerOfTwo(tileCountY) >= tailSize)
		tailSize = gxmNextPowerOfTwo(tileCountY);
	tailSize = tailSize * tailSize * 8;
	if (tailSize < 16)
		tailSize = 16;
	tailSize = (tailSize + 63) & ~63;
	rtDataSize = 0;
	for (i = 0; i < 4; i++) {
		rtDataSize = gxmPageFit((rtDataSize + 63) & ~63, tailSize);
		rtDataSize = gxmPageFit((rtDataSize + tailSize + 15) & ~15, 0x80) + 0x80;
		rtDataSize = gxmPageFit(rtDataSize, 0x90) + 0x90;
	}
	if (tileCountX == tilesX * samplesX && tileCountY == tilesY * samplesY)
		regionsSize += 256;

	/* each data set's region arrays and other macrotile state */
	if (rtDataCount != 0) {
		stateSize = macrotiles * macrotiles * 16;
		if (stateSize < 16)
			stateSize = 16;
		if (regionsSize < 16)
			regionsSize = 16;
		stateSize = (stateSize + 31) & ~31;
		macrotileSize = (regionsSize + 63) & ~63;
		for (i = 0; i < rtDataCount; i++) {
			for (j = 0; j < 4; j++)
				rtDataSize = gxmPageFit((rtDataSize + 63) & ~63, macrotileSize) + macrotileSize;
			regionSize = gxmPageFit(regionSize, stateSize) + stateSize;
			rtDataSize = gxmPageFit((rtDataSize + 15) & ~15, 0x6f0) + 0x6f0;
		}
		hostSize += i * 480;
	}
	*size = ((hostSize + 0xfff) & ~0xfff) + ((regionSize + 0xfff) & ~0xfff) + renderSize +
		((rtDataSize + 0xfff) & ~0xfff);
}

static inline int gxmRenderTargetCreate(const SceGxmRenderTargetParams *params,
					struct SceGxmRenderTarget **renderTarget)
{
	SGX_ADDRENDTARG target;
	SceUInt32 hMemBlockProcRef;
	SceUInt32 queuedRenders, flags;
	SceSize size;
	SceUID driverMemBlock;
	int err;

	size = 0;
	gxmRenderTargetGetDriverMemSize(params, &size);
	driverMemBlock = params->driverMemBlock;
	if (driverMemBlock == SCE_UID_INVALID_UID) {
		driverMemBlock = sceKernelAllocMemBlock("SceGxmRenderTarget",
							SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, size,
							NULL);
		if (driverMemBlock < 0)
			return SCE_GXM_ERROR_OUT_OF_MEMORY;
	} else {
		SceKernelMemBlockInfo info;
		void *base;

		if (sceKernelGetMemBlockBase(driverMemBlock, &base) != 0)
			return SCE_GXM_ERROR_INVALID_VALUE;
		info.size = sizeof(info);
		if (sceKernelGetMemBlockInfoByAddr(base, &info) != 0 || size > info.mappedSize)
			return SCE_GXM_ERROR_INVALID_VALUE;
	}
	hMemBlockProcRef = 0;
	if (SceGpuEs4User_0EA7458D(&gxmGlobals.top, driverMemBlock, &hMemBlockProcRef, 1) != 0) {
		if (params->driverMemBlock == SCE_UID_INVALID_UID)
			sceKernelFreeMemBlock(driverMemBlock);
		return SCE_GXM_ERROR_DRIVER;
	}

	target.ui32Flags = 2;
	target.ui32NumRTData = params->scenesPerFrame * 2;
	queuedRenders = 3;
	if (params->flags & SCE_GXM_RENDER_TARGET_USE_DISPLAY_QUEUE_PARAMS)
		queuedRenders = gxmDisplayQueueGetMaxPending(gxmGlobals.displayQueue) + 1;
	target.ui32MaxQueuedRenders = queuedRenders * params->scenesPerFrame;
	target.hRenderContext = gxmGlobals.top.renderContext;
	target.hDevCookie = gxmGlobals.top.devCookie;
	target.ui32NumPixelsX = params->width;
	target.ui32NumPixelsY = params->height;
	if (params->multisampleMode == SCE_GXM_MULTISAMPLE_2X) {
		target.ui16MSAASamplesInX = 1;
		target.ui16MSAASamplesInY = 2;
	} else if (params->multisampleMode == SCE_GXM_MULTISAMPLE_4X) {
		target.ui16MSAASamplesInX = 2;
		target.ui16MSAASamplesInY = 2;
	} else {
		target.ui16MSAASamplesInX = 1;
		target.ui16MSAASamplesInY = 1;
	}
	target.eForceScalingInX = 0;
	target.eForceScalingInY = 0;
	target.ui32BGObjUCoord = 0x3f800000;	/* 1.0f */
	target.eRotation = 0;
	target.ui16NumRTsInArray = 1;
	flags = params->flags;
	target.ui8MacrotileCountX = gxmMacrotileCountX(flags);
	target.ui8MacrotileCountY = gxmMacrotileCountY(flags);
	target.i32DataMemblockUID = driverMemBlock;
	target.bUseExternalUID = params->driverMemBlock == SCE_UID_INVALID_UID;
	target.hMemBlockProcRef = hMemBlockProcRef;
	target.ui32MultisampleLocations = params->multisampleLocations;
	if (flags & SCE_GXM_RENDER_TARGET_CUSTOM_MULTISAMPLE_LOCATIONS)
		target.ui32Flags |= 0x10000;
	if (flags & SCE_GXM_RENDER_TARGET_MACROTILE_SYNC)
		target.ui32Flags |= 0x60000;
	if (flags & GXM_RENDER_TARGET_SYSTEM_POOL)
		target.ui32Flags |= 0x80000;
	if (flags & GXM_RENDER_TARGET_SYSTEM_SINGLE)
		target.ui32Flags |= 0x100000;

	err = SceGpuEs4User_0162A405(&gxmGlobals.top, &target, renderTarget);
	if (err != 0) {
		SceGpuEs4User_156A6B70(&gxmGlobals.top, driverMemBlock);
		if (params->driverMemBlock == SCE_UID_INVALID_UID)
			sceKernelFreeMemBlock(driverMemBlock);
		*renderTarget = NULL;
		/* PVRSRV_ERROR_OUT_OF_RENDER_TARGETS */
		return err == 184 ? SCE_GXM_ERROR_OUT_OF_RENDER_TARGETS : SCE_GXM_ERROR_DRIVER;
	}
	return 0;
}

int sceGxmCreateRenderTargetInternal(const SceGxmRenderTargetParams *params,
				     struct SceGxmRenderTarget **renderTarget)
{
	int ret;

	if (params == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!gxmGlobals.initialized)
		return SCE_GXM_ERROR_UNINITIALIZED;
	ret = gxmRenderTargetCheckParams(params);
	if (ret != 0)
		return ret;
	if (renderTarget == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	return gxmRenderTargetCreate(params, renderTarget);
}

int sceGxmCreateRenderTarget(const SceGxmRenderTargetParams *params,
			     struct SceGxmRenderTarget **renderTarget)
{
	int ret;

	if (params == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (params->flags & ~GXM_RENDER_TARGET_PUBLIC_MASK)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (!gxmGlobals.initialized)
		return SCE_GXM_ERROR_UNINITIALIZED;
	ret = gxmRenderTargetCheckParams(params);
	if (ret != 0)
		return ret;
	if (renderTarget == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	return gxmRenderTargetCreate(params, renderTarget);
}

int sceGxmRenderTargetGetDriverMemBlock(const struct SceGxmRenderTarget *renderTarget,
					SceUID *driverMemBlock)
{
	SceUID block;
	SceUInt32 allocated;

	if (renderTarget == NULL || driverMemBlock == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (SceGpuEs4User_804BA13D(&gxmGlobals.top, renderTarget, &block, &allocated) != 0)
		return SCE_GXM_ERROR_DRIVER;
	/* memory libgxm allocated is not the application's */
	*driverMemBlock = allocated ? SCE_UID_INVALID_UID : block;
	return 0;
}

int sceGxmDestroyRenderTarget(struct SceGxmRenderTarget *renderTarget)
{
	SceUID block;
	SceUInt32 allocated;

	if (!gxmGlobals.initialized)
		return SCE_GXM_ERROR_UNINITIALIZED;
	if (renderTarget == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	allocated = 0;
	if (SceGpuEs4User_804BA13D(&gxmGlobals.top, renderTarget, &block, &allocated) != 0 ||
	    SceGpuEs4User_D90B0707(&gxmGlobals.top, gxmGlobals.top.renderContext, renderTarget) != 0)
		return SCE_GXM_ERROR_DRIVER;
	SceGpuEs4User_156A6B70(&gxmGlobals.top, block);
	if (allocated == 1)
		sceKernelFreeMemBlock(block);
	return 0;
}

int sceGxmGetRenderTargetMemSize(const SceGxmRenderTargetParams *params, SceSize *driverMemSize)
{
	SceSize size;
	int ret;

	if (params == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (params->flags & ~GXM_RENDER_TARGET_PUBLIC_MASK)
		return SCE_GXM_ERROR_INVALID_VALUE;
	ret = gxmRenderTargetCheckParams(params);
	if (ret != 0)
		return ret;
	if (driverMemSize == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	gxmRenderTargetGetDriverMemSize(params, &size);
	*driverMemSize = size;
	return 0;
}

int sceGxmGetRenderTargetMemSizeInternal(const SceGxmRenderTargetParams *params,
					 SceSize *driverMemSize)
{
	SceSize size;
	int ret;

	ret = gxmRenderTargetCheckParams(params);
	if (ret != 0)
		return ret;
	if (driverMemSize == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	gxmRenderTargetGetDriverMemSize(params, &size);
	*driverMemSize = size;
	return 0;
}
