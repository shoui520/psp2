/*
 * SceGxm: library start-up and initialization.
 */

#include "gxm_internal.h"
#include "memory_util_private.h"

int sceKernelGetProcessTitleId(char *titleId, SceSize size);

#define SCE_GXM_INITIALIZE_FLAG_PB_LPDDR		0x00000001
#define SCE_GXM_INITIALIZE_FLAG_SHAREDPB_CREATE		0x00000004
#define SCE_GXM_INITIALIZE_FLAG_SHAREDPB_OPEN		0x00000008
#define SCE_GXM_INITIALIZE_FLAG_EXTENDED_FORMAT		0x00000010
#define SCE_GXM_INITIALIZE_FLAG_DISPLAY_QUEUE_CPU_MASK	0x00030000
#define SCE_GXM_INITIALIZE_FLAG_DISPLAY_QUEUE_CPU_1	0x00010000
#define SCE_GXM_INITIALIZE_FLAG_DISPLAY_QUEUE_CPU_2	0x00020000

#define SCE_KERNEL_CPU_MASK_USER_0	0x00010000
#define SCE_KERNEL_CPU_MASK_USER_1	0x00020000
#define SCE_KERNEL_CPU_MASK_USER_2	0x00040000

#define GXM_HEAP_GENERAL		0
#define GXM_HEAP_FRAGMENT_USSE		4
#define GXM_HEAP_VERTEX_USSE		5
#define GXM_HEAP_SLOTS			8
#define GXM_HEAP_14			14
#define GXM_HEAP_FRAGMENT_USSE_INTERNAL	18
#define GXM_HEAP_VERTEX_USSE_INTERNAL	19

/* where the driver put the heaps the library maps into */
int gxmQueryHeaps(void)
{
	PVRSRV_HEAP_INFO heaps[32];
	SceUInt32 count, i;
	int err;

	err = SceGpuEs4User_1E0A9C4A(&gxmGlobals.top, &gxmGlobals.top.memContext, &count, heaps);
	if (err != 0) {
		GXM_DBG_ERROR(147, "Failed due to error in driver layer (%08X).", err);
		return SCE_GXM_ERROR_DRIVER;
	}
	for (i = 0; i < count; i++) {
		switch (heaps[i].ui32HeapID & 0xffffff) {
		case GXM_HEAP_GENERAL:
			gxmGlobals.top.generalHeap = heaps[i].hDevMemHeap;
			break;
		case GXM_HEAP_FRAGMENT_USSE:
			gxmGlobals.top.usse[0].heap[1] = heaps[i].hDevMemHeap;
			gxmGlobals.top.usse[0].base[1] = heaps[i].sDevVAddrBase;
			break;
		case GXM_HEAP_VERTEX_USSE:
			gxmGlobals.top.usse[0].heap[0] = heaps[i].hDevMemHeap;
			gxmGlobals.top.usse[0].base[0] = heaps[i].sDevVAddrBase;
			break;
		case GXM_HEAP_SLOTS:
			gxmGlobals.top.slotHeap = heaps[i].hDevMemHeap;
			break;
		case GXM_HEAP_14:
			gxmGlobals.top.heap14 = heaps[i].hDevMemHeap;
			break;
		case GXM_HEAP_FRAGMENT_USSE_INTERNAL:
			gxmGlobals.top.usse[1].heap[1] = heaps[i].hDevMemHeap;
			gxmGlobals.top.usse[1].base[1] = heaps[i].sDevVAddrBase;
			break;
		case GXM_HEAP_VERTEX_USSE_INTERNAL:
			gxmGlobals.top.usse[1].heap[0] = heaps[i].hDevMemHeap;
			gxmGlobals.top.usse[1].base[0] = heaps[i].sDevVAddrBase;
			break;
		}
	}
	if (gxmGlobals.top.generalHeap == 0 || gxmGlobals.top.usse[0].heap[0] == 0 ||
	    gxmGlobals.top.usse[0].heap[1] == 0 || gxmGlobals.top.slotHeap == 0 ||
	    gxmGlobals.top.usse[1].heap[0] == 0 || gxmGlobals.top.usse[1].heap[1] == 0 ||
	    gxmGlobals.top.heap14 == 0) {
		GXM_DBG_ERROR(194, "Services general heap not found.");
		return SCE_GXM_ERROR_OUT_OF_MEMORY;
	}
	return 0;
}

/* the debug build keeps it out of line: initialization's failures and sceGxmTerminate call it */
static inline GXM_DBG_NOINLINE int gxmCloseDevice(void)
{
	int err;

	err = SceGpuEs4User_5273D097(&gxmGlobals.top, gxmGlobals.top.driverState);
	if (err != 0) {
		GXM_DBG_ERROR(124, "Failed due to error in driver layer (%08X).", err);
		return SCE_GXM_ERROR_DRIVER;
	}
	err = SceGpuEs4User_F4735A47(gxmGlobals.gpuDevice, &gxmGlobals.deviceInfo);
	if (err != 0) {
		GXM_DBG_ERROR(127, "Failed due to error in driver layer (%08X).", err);
		return SCE_GXM_ERROR_DRIVER;
	}
	err = SceGpuEs4User_A5B14369(gxmGlobals.gpuDevice);
	if (err != 0) {
		GXM_DBG_ERROR(131, "Failed due to error in driver layer (%08X).", err);
		return SCE_GXM_ERROR_DRIVER;
	}
	return 0;
}

#define GXM_DRIVER_MEMORY_SIZE		0x00100000
#define GXM_NOTIFICATION_BLOCK_SIZE	0x820
#define GXM_FENCE_BLOCK_SIZE		0x10

typedef struct SceGxmInitializeParams {
	SceUInt32 flags;
	SceUInt32 displayQueueMaxPendingCount;
	SceGxmDisplayQueueCallback *displayQueueCallback;
	SceSize displayQueueCallbackDataSize;
	SceSize parameterBufferSize;
} SceGxmInitializeParams;

/* the part of the parameter buffer the driver keeps for itself */
static inline SceSize gxmParameterBufferReserve(SceUInt32 flags, SceSize lpddr, SceSize cdram)
{
	return (flags & SCE_GXM_INITIALIZE_FLAG_PB_LPDDR) ? lpddr : cdram;
}

GXM_NOINLINE int sceGxmGetParameterBufferThreshold(SceUInt32 *parameterBufferSize)
{
	if (!gxmGlobals.initialized) {
		GXM_DBG_ERROR(254, "Function sceGxmGetParameterBufferThreshold has been called while "
			      "the library is uninitialized.");
		return SCE_GXM_ERROR_UNINITIALIZED;
	}
	if (parameterBufferSize == NULL) {
		GXM_DBG_ERROR_NULL(260, sceGxmGetParameterBufferThreshold, parameterBufferSize);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	*parameterBufferSize = gxmGlobals.parameterBufferSize - 0x40000 -
		     gxmParameterBufferReserve(gxmGlobals.initFlags, 0x15000, 0x40000);
	return 0;
}

GXM_NOINLINE static int gxmInitialize(const SceGxmInitializeParams *params)
{
	SGX_CREATERENDERCONTEXT contextParams;
	SceUInt32 deviceCount, devices[3], transferParams[2], flags, pbFlags, unreservedFlags;
	SceUInt32 offset, cpuAffinity;
	SceSize pbSize, reserved, codeSize;
	GxmChunkBuffer buffer;
	PVRSRV_CLIENT_MEM_INFO *block;
	SceUInt32 threshold;
	SceUInt8 contextOut[8];
	int err, err2;

	gxmGlobals.initialized = 1;
	gxmGlobals.initFlags = params->flags;
	gxmGlobals.frameIndex = 0;
#if SCE_DBG_ASSERTS_ENABLED
	gxmGlobals.warningMask = 0xffffffff;
#endif
	gxmGlobals.counter = 0;
	gxmGlobals.sceneIndex = 0;
	gxmGlobals.formatLevel = (params->flags & SCE_GXM_INITIALIZE_FLAG_EXTENDED_FORMAT) >> 4;
	err = gxmSceneStateInit();
	if (err != 0)
		goto out;

	/* open the device */
	err2 = SceGpuEs4User_228252A2(&gxmGlobals.gpuDevice, 0);
	if (err2 != 0) {
		GXM_DBG_ERROR(59, "Failed due to error in driver layer (%08X).", err2);
		goto driverError;
	}
	gxmGlobals.deviceInfo = 8;
	err2 = SceGpuEs4User_73E945A1(gxmGlobals.gpuDevice, &gxmGlobals.deviceInfo);
	if (err2 != 0) {
		GXM_DBG_ERROR(70, "Failed due to error in driver layer (%08X).", err2);
		goto driverError;
	}
	deviceCount = 0;
	err2 = SceGpuEs4User_4447D217(gxmGlobals.gpuDevice, &deviceCount, devices);
	if (err2 != 0) {
		GXM_DBG_ERROR(87, "Failed due to error in driver layer (%08X).", err2);
		goto driverError;
	}
	if (deviceCount == 0 || devices[0] != 7) {
		GXM_DBG_ERROR(96, "Failed to find SGX device during initialisation.");
		goto driverError;
	}
	err2 = SceGpuEs4User_70800B46(gxmGlobals.gpuDevice, devices[2], &gxmGlobals.top);
	if (err2 != 0) {
		GXM_DBG_ERROR(108, "Failed due to error in driver layer (%08X).", err2);
		goto driverError;
	}
	sceClibMemset(gxmGlobals.top.driverState, 0, sizeof(gxmGlobals.top.driverState));
	err2 = SceGpuEs4User_B000CA65(&gxmGlobals.top, gxmGlobals.top.driverState);
	if (err2 != 0) {
		GXM_DBG_ERROR(114, "Failed due to error in driver layer (%08X).", err2);
		goto driverError;
	}
	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC)
		gxmInitCallbacks.initialize();

	/* driver memory and the parameter buffer */
	flags = params->flags;
	pbSize = params->parameterBufferSize;
	gxmGlobals.top.driverBlock = sceKernelAllocMemBlock("SceGxmDriver",
							    SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
							    GXM_DRIVER_MEMORY_SIZE, NULL);
	if (gxmGlobals.top.driverBlock < 0) {
		err = SCE_GXM_ERROR_OUT_OF_MEMORY;
		goto closeDevice;
	}
	if (!(flags & SCE_GXM_INITIALIZE_FLAG_SHAREDPB_OPEN)) {
		gxmGlobals.top.parameterBufferBlock =
			sceKernelAllocMemBlock("SceGxmParamBuffer",
					       (flags & SCE_GXM_INITIALIZE_FLAG_PB_LPDDR) ?
					       SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE :
					       SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
					       pbSize, NULL);
		if (gxmGlobals.top.parameterBufferBlock < 0) {
			err = SCE_GXM_ERROR_OUT_OF_MEMORY;
			goto freeDriverMemory;
		}
		gxmGlobals.top.hMemBlockProcRef = 0;
		if (SceGpuEs4User_0EA7458D(&gxmGlobals.top, gxmGlobals.top.driverBlock,
					   &gxmGlobals.top.hMemBlockProcRef, 0) == 0) {
			if (SceGpuEs4User_0EA7458D(&gxmGlobals.top, gxmGlobals.top.parameterBufferBlock,
						   &gxmGlobals.top.hMemBlockProcRef, 1) == 0)
				goto registered;
			SceGpuEs4User_156A6B70(&gxmGlobals.top, gxmGlobals.top.driverBlock);
		}
		sceKernelFreeMemBlock(gxmGlobals.top.parameterBufferBlock);
		err = SCE_GXM_ERROR_DRIVER;
		goto freeDriverMemory;
	}
	gxmGlobals.top.hMemBlockProcRef = 0;
	if (SceGpuEs4User_0EA7458D(&gxmGlobals.top, gxmGlobals.top.driverBlock,
				   &gxmGlobals.top.hMemBlockProcRef, 0) != 0) {
		err = SCE_GXM_ERROR_DRIVER;
		goto freeDriverMemory;
	}

registered:
	err = gxmQueryHeaps();
	if (err != 0)
		goto unregisterMemory;

	/* the render context */
	flags = params->flags;
	pbSize = params->parameterBufferSize;
	contextParams.ui32VisTestResultBufferSize = 0;
	contextParams.hDevMemContext = gxmGlobals.top.memContext;
	if (!(flags & SCE_GXM_INITIALIZE_FLAG_PB_LPDDR)) {
		unreservedFlags = 2;
		pbFlags = 6;
	} else {
		unreservedFlags = 0;
		pbFlags = 4;
	}
	if (flags & 2)
		pbFlags = unreservedFlags;
	if (flags & SCE_GXM_INITIALIZE_FLAG_SHAREDPB_CREATE)
		pbFlags |= 1;
	contextParams.ui32Flags = pbFlags;
	reserved = gxmParameterBufferReserve(flags, 0x55000, 0x80000);
	contextParams.ui32PBSize = pbSize - reserved;
	contextParams.bPerContextPB =
		(flags & (SCE_GXM_INITIALIZE_FLAG_SHAREDPB_CREATE |
			  SCE_GXM_INITIALIZE_FLAG_SHAREDPB_OPEN)) == 0;
	contextParams.hDevCookie = gxmGlobals.top.devCookie;
	contextParams.hMemBlockProcRef = gxmGlobals.top.hMemBlockProcRef;
	contextParams.ui32MaxSACount = 0x80;
	gxmGlobals.parameterBufferSize = pbSize;
	contextParams.ui32PBSizeLimit = contextParams.ui32PBSize;
	err2 = SceGpuEs4User_8A997DBB(&gxmGlobals.top, &contextParams, &gxmGlobals.top.renderContext,
				      contextOut);
	if (err2 != 0) {
		err = SCE_GXM_ERROR_DRIVER;
		GXM_DBG_ERROR(306, "Failed due to error in driver layer (%08X).", err2);
		goto destroyMemContext;
	}
	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC)
		gxmInitCallbacks.parameterBuffer(pbSize, reserved);

	transferParams[0] = gxmGlobals.top.memContext;
	transferParams[1] = gxmGlobals.top.hMemBlockProcRef;
	err2 = SceGpuEs4User_BDE2B9E1(&gxmGlobals.top, 0, transferParams,
				      &gxmGlobals.top.transferQueue);
	if (err2 != 0) {
		err = SCE_GXM_ERROR_DRIVER;
		GXM_DBG_ERROR(361, "Failed due to error in driver layer (%08X).", err2);
		goto destroyRenderContext;
	}
	err = gxmSlotTableCreate(&gxmGlobals.top.contexts);
	if (err != 0)
		goto destroyTransferQueue;

	/* notifications, the fence word and the library's own code */
	err = gxmAllocGpuBlock(gxmGlobals.top.slotHeap, 0x40000f, GXM_NOTIFICATION_BLOCK_SIZE, 4,
			       &block);
	if (err != 0)
		goto destroySlots;
	gxmGlobals.top.notifications = block;
	sceClibMemset(block->pvLinAddr, 0, GXM_NOTIFICATION_BLOCK_SIZE);
	err = gxmAllocGpuBlock(gxmGlobals.top.generalHeap, 0x40000b, GXM_FENCE_BLOCK_SIZE, 1, &block);
	if (err != 0)
		goto freeNotifications;
	gxmGlobals.top.fenceBlock = block;
	sceClibMemset(block->pvLinAddr, 0, GXM_FENCE_BLOCK_SIZE);
	codeSize = ((gxmMteGetCopyProgramsSize() + 0xf) & ~0xf) +
		   ((gxmUsseGetEmptyProgramSize() + 0xf) & ~0xf) + GXM_FENCE_PROGRAM_SIZE;
	err = gxmAllocGpuBlock(gxmGlobals.top.usse[1].heap[0], 0x400009, codeSize, 0x10, &block);
	if (err != 0)
		goto freeFence;
	gxmGlobals.top.codeBlock = block;
	offset = block->sDevVAddr - gxmGlobals.top.usse[1].base[0];
	gxmChunkBufferInitFixed(&buffer, block->pvLinAddr,
				(offset & 0x7fffff) | ((offset >> 23) + 8) << 24, codeSize);
	err = gxmMteBuildCopyPrograms(gxmGlobals.top.copyPrograms, &buffer);
	if (err != 0)
		goto freeFence;
	err = gxmUsseBuildEmptyProgram(&buffer, &gxmGlobals.top.emptyProgram);
	if (err != 0)
		goto freeFence;
	err = gxmBuildFenceProgram(&buffer, &gxmGlobals.top.fenceProgram,
				   gxmGlobals.top.fenceBlock->sDevVAddr);
	if (err != 0)
		goto freeFence;

	/* the display queue thread */
	if (params->displayQueueCallback != NULL) {
		if ((params->flags & SCE_GXM_INITIALIZE_FLAG_DISPLAY_QUEUE_CPU_MASK) ==
		    SCE_GXM_INITIALIZE_FLAG_DISPLAY_QUEUE_CPU_1)
			cpuAffinity = SCE_KERNEL_CPU_MASK_USER_1;
		else if ((params->flags & SCE_GXM_INITIALIZE_FLAG_DISPLAY_QUEUE_CPU_MASK) ==
			 SCE_GXM_INITIALIZE_FLAG_DISPLAY_QUEUE_CPU_2)
			cpuAffinity = SCE_KERNEL_CPU_MASK_USER_2;
		else
			cpuAffinity = SCE_KERNEL_CPU_MASK_USER_0;
		err2 = gxmDisplayQueueCreate(params->displayQueueMaxPendingCount,
					     params->displayQueueCallback,
					     params->displayQueueCallbackDataSize,
					     &gxmGlobals.displayQueue, cpuAffinity);
		if (err2 != 0)
			goto freeCode;
	}
	if (gxmRazorMagic == GXM_CALLBACKS_MAGIC) {
		err2 = sceGxmGetParameterBufferThreshold(&threshold);
		if (err2 == 0)
			gxmRazorCallbacks.parameterBufferThreshold(threshold);
		reserved = gxmParameterBufferReserve(gxmGlobals.initFlags, 0x1000, 0x40000);
		gxmRazorCallbacks.parameterBuffer(gxmGlobals.parameterBufferSize,
						  ((0x14040 - 1 + reserved) & -reserved) + 0x40000);
		if (err2 != 0) {
			if (gxmGlobals.displayQueue != NULL) {
				gxmDisplayQueueDestroy(gxmGlobals.displayQueue);
				gxmGlobals.displayQueue = NULL;
			}
			goto freeCode;
		}
	}
	gxmGlobals.initDone = 1;
	return err;

	/* a failed program build keeps its code block, as Sony's */
freeCode:
	err = err2;
	gxmFreeGpuBlock(gxmGlobals.top.codeBlock);
freeFence:
	gxmFreeGpuBlock(gxmGlobals.top.fenceBlock);
freeNotifications:
	gxmFreeGpuBlock((PVRSRV_CLIENT_MEM_INFO *)gxmGlobals.top.notifications);
destroySlots:
	gxmSlotTableDestroy(&gxmGlobals.top.contexts);
destroyTransferQueue:
	if (gxmGlobals.top.transferQueue != 0) {
		err2 = SceGpuEs4User_8ED7D698(&gxmGlobals.top, gxmGlobals.top.transferQueue, 0);
		if (err2 != 0)
			GXM_DBG_ERROR(379, "Failed due to error in driver layer (%08X).", err2);
		else
			gxmGlobals.top.transferQueue = 0;
	}
destroyRenderContext:
	err2 = SceGpuEs4User_0BFE1685(&gxmGlobals.top, gxmGlobals.top.renderContext, 0, 0);
	if (err2 != 0)
		GXM_DBG_ERROR(343, "Failed due to error in driver layer (%08X).", err2);
destroyMemContext:
	err2 = SceGpuEs4User_B9178277(&gxmGlobals.top, gxmGlobals.top.memContext);
	if (err2 != 0)
		GXM_DBG_ERROR(222, "Failed due to error in driver layer (%08X).", err2);
unregisterMemory:
	if (!(gxmGlobals.initFlags & SCE_GXM_INITIALIZE_FLAG_SHAREDPB_OPEN))
		SceGpuEs4User_156A6B70(&gxmGlobals.top, gxmGlobals.top.parameterBufferBlock);
	SceGpuEs4User_156A6B70(&gxmGlobals.top, gxmGlobals.top.driverBlock);
	gxmGlobals.top.hMemBlockProcRef = 0;
	if (!(gxmGlobals.initFlags & SCE_GXM_INITIALIZE_FLAG_SHAREDPB_OPEN))
		sceKernelFreeMemBlock(gxmGlobals.top.parameterBufferBlock);
freeDriverMemory:
	sceKernelFreeMemBlock(gxmGlobals.top.driverBlock);
closeDevice:
	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC)
		gxmInitCallbacks.terminate();
	gxmCloseDevice();
	gxmSceneStateTerm();
	goto out;
driverError:
	err = SCE_GXM_ERROR_DRIVER;
	gxmSceneStateTerm();
out:
	gxmGlobals.initFlags = 0;
	gxmGlobals.initialized = 0;
	return err;
}

int sceGxmInitialize(const SceGxmInitializeParams *params)
{
	if (gxmGlobals.initialized) {
		GXM_DBG_ERROR(1323, "Function sceGxmInitialize has been called while the library is "
			      "already initialized.");
		return SCE_GXM_ERROR_ALREADY_INITIALIZED;
	}
	if (params == NULL) {
		GXM_DBG_ERROR_NULL(1329, sceGxmInitialize, params);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if ((params->flags & ~SCE_GXM_INITIALIZE_FLAG_DISPLAY_QUEUE_CPU_1) != 0 &&
	    params->flags != SCE_GXM_INITIALIZE_FLAG_DISPLAY_QUEUE_CPU_2) {
		GXM_DBG_ERROR(1335, "Parameter params->flags to function sceGxmInitialize has "
			      "unexpected bits set.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if ((params->parameterBufferSize & 0x3ffff) != 0) {
		GXM_DBG_ERROR(1340, "Parameter params->parameterBufferSize to function sceGxmInitialize "
			      "is not aligned to %d bytes.", 0x40000);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (params->displayQueueMaxPendingCount * params->displayQueueCallbackDataSize > 0x200) {
		GXM_DBG_ERROR(1345, "The total display queue storage in function sceGxmInitialize must "
			      "not exceed %d bytes.", 0x200);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	return gxmInitialize(params);
}

int sceGxmVshInitialize(const SceGxmInitializeParams *params)
{
	SceUInt32 shared = SCE_GXM_INITIALIZE_FLAG_SHAREDPB_CREATE | SCE_GXM_INITIALIZE_FLAG_SHAREDPB_OPEN;

	if (gxmGlobals.initialized) {
		GXM_DBG_ERROR(1357, "Function sceGxmInitializeInternal has been called while the "
			      "library is already initialized.");
		return SCE_GXM_ERROR_ALREADY_INITIALIZED;
	}
	if (params == NULL) {
		GXM_DBG_ERROR_NULL(1363, sceGxmInitializeInternal, params);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if ((params->flags & ~0x1f) != 0) {
		GXM_DBG_ERROR(1372, "Parameter params->flags to function sceGxmInitializeInternal has "
			      "unexpected bits set.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if ((params->flags & shared) == shared) {
		GXM_DBG_ERROR(1376, "Parameter params->flags to function sceGxmInitializeInternal has "
			      "unexpected SHAREDPB bits set.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if ((params->parameterBufferSize & 0x3ffff) != 0) {
		GXM_DBG_ERROR(1381, "Parameter params->parameterBufferSize to function "
			      "sceGxmInitializeInternal is not aligned to %d bytes.", 0x40000);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (params->displayQueueMaxPendingCount * params->displayQueueCallbackDataSize > 0x200) {
		GXM_DBG_ERROR(1386, "The total display queue storage in function "
			      "sceGxmInitializeInternal must not exceed %d bytes.", 0x200);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	return gxmInitialize(params);
}

int sceGxmTerminate(void)
{
	if (!gxmGlobals.initialized) {
		GXM_DBG_ERROR(1398, "Function sceGxmTerminate has been called while the library is "
			      "uninitialized.");
		return SCE_GXM_ERROR_UNINITIALIZED;
	}
	if (gxmRazorMagic == GXM_CALLBACKS_MAGIC)
		gxmRazorCallbacks.terminate();
	if (gxmGlobals.displayQueue != NULL) {
		gxmDisplayQueueDestroy(gxmGlobals.displayQueue);
		gxmGlobals.displayQueue = NULL;
	}
	gxmFreeGpuBlock(gxmGlobals.top.codeBlock);
	gxmFreeGpuBlock(gxmGlobals.top.fenceBlock);
	gxmFreeGpuBlock((PVRSRV_CLIENT_MEM_INFO *)gxmGlobals.top.notifications);
	gxmSlotTableDestroy(&gxmGlobals.top.contexts);
	gxmCloseDevice();
	gxmGlobals.top.hMemBlockProcRef = 0;
	if (!(gxmGlobals.initFlags & SCE_GXM_INITIALIZE_FLAG_SHAREDPB_OPEN))
		sceKernelFreeMemBlock(gxmGlobals.top.parameterBufferBlock);
	sceKernelFreeMemBlock(gxmGlobals.top.driverBlock);
	gxmSceneStateTerm();
	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC)
		gxmInitCallbacks.terminate();
	gxmGlobals.initFlags = 0;
	gxmGlobals.initialized = 0;
	gxmGlobals.initDone = 0;
	return 0;
}

SceBool sceGxmRenderingContextIsWithinSceneInternal(void)
{
	if (!gxmGlobals.initialized)
		return 0;
	return gxmSceneIsActive();
}

SceUInt8 sceGxmIsInitializationInternal(void)
{
	return gxmGlobals.initDone;
}

int sceGxmSetWarningEnabled(SceUInt32 warning, SceBool enable)
{
	if (!gxmGlobals.initialized) {
		GXM_DBG_ERROR(1451, "Function sceGxmSetWarningEnabled has been called while the "
			      "library is uninitialized.");
		return SCE_GXM_ERROR_UNINITIALIZED;
	}
#if SCE_DBG_ASSERTS_ENABLED
	if (enable)
		gxmGlobals.warningMask |= 1 << warning;
	else
		gxmGlobals.warningMask &= ~(1 << warning);
#endif
	return 0;
}

SceBool sceGxmIsDebugVersion(void)
{
	return SCE_DBG_ASSERTS_ENABLED;
}

/*
 * Behaviour switches by the application's SDK version: each newer SDK
 * drops more of the old behaviour. A few titles also keep finishing every
 * context when memory is unmapped.
 */
int module_start(SceSize args, void *argp)
{
	char titleId[32];
	SceUInt32 finish, sdkVersion;

	if (sceKernelGetProcessTitleId(titleId, sizeof(titleId) - 1) < 0)
		finish = 0;
	else if (sceClibStrcmp(titleId, "PCSC00006") == 0 ||
		 sceClibStrcmp(titleId, "PCSC90003") == 0 ||
		 sceClibStrcmp(titleId, "PCSA00015") == 0 ||
		 sceClibStrcmp(titleId, "PCSA00020") == 0 ||
		 sceClibStrcmp(titleId, "PCSA00025") == 0 ||
		 sceClibStrcmp(titleId, "PCSF00007") == 0 ||
		 sceClibStrcmp(titleId, "PCSD00005") == 0)
		finish = GXM_COMPAT_FINISH_ON_UNMAP;
	else
		finish = sceClibStrcmp(titleId, "PCSG00178") == 0;
	sdkVersion = SceLibKernel_9F793F84();
	if (sdkVersion < 0x01600000)
		gxmGlobals.compatFlags = 0xffffe | finish;
	else if (sdkVersion < 0x01650000)
		gxmGlobals.compatFlags = 0xffff8 | finish;
	else if (sdkVersion < 0x01800000)
		gxmGlobals.compatFlags = 0xfffc0 | finish;
	else if (sdkVersion < 0x02000000)
		gxmGlobals.compatFlags = 0xffe00 | finish;
	else if (sdkVersion < 0x02100000)
		gxmGlobals.compatFlags = 0xff000 | finish;
	else if (sdkVersion < 0x02500000)
		gxmGlobals.compatFlags = 0xfe000 | finish;
	else if (sdkVersion < 0x03100000)
		gxmGlobals.compatFlags = 0xf8000 | finish;
	else
		gxmGlobals.compatFlags = finish;
	return 0;
}

int module_stop(SceSize args, void *argp)
{
	return 0;
}

int module_exit(SceSize args, void *argp)
{
	if (gxmGlobals.displayQueue != NULL) {
		gxmDisplayQueueDestroy(gxmGlobals.displayQueue);
		gxmGlobals.displayQueue = NULL;
	}
	return 0;
}

/* the library's own strcmp: bytes compared unsigned, result -1, 0 or 1 */
int gxmStrcmp(const char *a, const char *b)
{
	const unsigned char *s = (const unsigned char *)a;
	const unsigned char *t = (const unsigned char *)b;

	while (*t == *s) {
		if (*s == 0)
			return 0;
		s++;
		t++;
	}
	return *s < *t ? -1 : 1;
}

/* a fixed chunk never grows */
static void gxmChunkBufferNoRefill(GxmChunkBuffer *buffer, SceSize size, SceSize alignment)
{
}

GXM_NOINLINE void gxmChunkBufferInitFixed(GxmChunkBuffer *buffer, void *memory, SceUInt32 gpuOffset,
					 SceSize size)
{
	buffer->end = (SceUInt32)memory + size;
	buffer->gpuEnd = size + gpuOffset;
	buffer->current = (SceUInt32)memory;
#if SCE_DBG_ASSERTS_ENABLED
	buffer->reserved = (SceUInt32)memory;
#endif
	buffer->refill = gxmChunkBufferNoRefill;
}
