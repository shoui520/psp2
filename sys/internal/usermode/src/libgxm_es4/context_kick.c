/*
 * SceGxm: an immediate context's kicks, which hand the rings' commands to the GPU.
 */

#include <string.h>

#include "gxm_internal.h"

/* the color space conversion of each SceGxmYuvProfile: the CSC registers' words */
static const SceUInt32 gxmYuvProfileCoefficients[4][6] = {
	{ 39060052, 596, 67702583, 241173297, 507641856, 500564239 },
	{ 39060052, 596, 70913939, 250545046, 504365056, 498991258 },
	{ 33554944, 512, 59445072, 244449998, 513343488, 507117839 },
	{ 33554944, 512, 62263200, 252707622, 510459904, 505741480 },
};

static SGX_KICKTA gxmKick;

/* the GPU address of a word in a mapped block */
static inline SceUInt32 gxmMemInfoDevAddr(const PVRSRV_CLIENT_MEM_INFO *block,
					  const volatile SceUInt32 *address)
{
	return (SceUInt32)address + block->sDevVAddr - (SceUInt32)block->pvLinAddr;
}

/* the bytes written into a ring since the last kick */
static inline SceSize gxmRingBufferUsed(const GxmRingBuffer *ring, const GxmChunkBuffer *buffer)
{
	SceUInt32 offset = buffer->cur - ring->memory;

	if (ring->lastOffset <= offset)
		return offset - ring->lastOffset;
	return ring->size - ring->lastOffset + offset;
}

/* the background object's programs, rebuilt after a scene's first kick of the fragment part */
static __attribute__((noinline)) void gxmContextBuildBackground(SceGxmContext *context)
{
	SceUInt32 dms;
	GxmCommandBlock pixel;
	GxmCommandBlock texture;
	GxmCommandBlock filter;
	GxmCodeRef dout;
	GxmCodeRef nop;
	GxmCodeRef kick;
	GxmCodeRef background;
	GxmCodeRef empty;
	SceUInt32 samples;

	if (!(context->flags & GXM_CONTEXT_FLAG_BACKGROUND))
		return;

	gxmPdsBuildDoutProgram(&context->fragmentUsseBuffer, &context->colorSurface, &dout);
	gxmPdsBuildNopProgram(&context->fragmentUsseBuffer, &nop);
	gxmPdsBuildKickProgram(&context->fragmentUsseBuffer, &kick);
	gxmPdsBuildPixelProgram(&context->fragmentBuffer, context->sampleCount,
				sceGxmColorSurfaceGetScaleMode(&context->colorSurface), &dout, &nop,
				&kick, &pixel, &context->pixelState);
	if (sceGxmColorSurfaceGetScaleMode(&context->colorSurface) != SCE_GXM_COLOR_SURFACE_SCALE_NONE)
		samples = 1;
	else
		samples = context->sampleCount;
	gxmBackgroundBuildProgram(&context->fragmentUsseBuffer, &context->colorSurface, samples,
				  &background, &dms);
	gxmBackgroundBuildTextureProgram(&context->fragmentBuffer, &context->colorSurface, samples,
					 &background, &texture);
	gxmUsseBuildEmptyProgram(&context->fragmentUsseBuffer, &empty);
	gxmBackgroundBuildFilterProgram(&context->fragmentBuffer, &context->colorSurface,
					context->colorWidth, context->colorHeight, samples, &empty,
					&filter);
	gxmPdsWriteStateWords(context->backgroundState, &filter, dms, &texture);
	context->flags &= ~GXM_CONTEXT_FLAG_BACKGROUND;
}

/* end the vertex stream: an empty vertex state, then a fence behind the whole region */
static inline void gxmContextWriteKickFence(SceGxmContext *context)
{
	GxmCommandBlock empty;
	GxmCommandBlock region;

	gxmPdsBuildSimpleProgram(&context->vertexBuffer, 0, 0, &gxmGlobals.top.emptyProgram, &empty);
	gxmVdmWriteVertexState(&context->vdmBuffer, &empty, 0, SCE_FALSE);
	gxmMteWriteRegionBlock(&context->vertexBuffer,
			       gxmMteGetCopyProgram(gxmGlobals.top.copyPrograms, 2), 0, 0,
			       context->tileLimitX, context->tileLimitY, &region);
	gxmVdmWriteFence(&context->vdmBuffer, &region);
}

int sceGxmFinish(SceGxmContext *context)
{
	if (context == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (gxmDisplayQueueIsCurrentThread())
		return SCE_GXM_ERROR_INVALID_THREAD;

	gxmRazorMarker(0x13000000, gxmGlobals.frameIndex);
	gxmWaitForValue(gxmGlobals.top.contexts.block, context->vdmRing.readOffset,
			context->vdmRing.lastOffset);
	gxmWaitForValue(gxmGlobals.top.contexts.block, context->vertexRing.readOffset,
			context->vertexRing.lastOffset);
	gxmWaitForValue(gxmGlobals.top.contexts.block, context->fragmentRing.readOffset,
			context->fragmentRing.lastOffset);
	gxmWaitForValue(gxmGlobals.top.contexts.block, context->fragmentUsseRing.readOffset,
			context->fragmentUsseRing.lastOffset);
	gxmRazorMarker(0x14000000, gxmGlobals.frameIndex);
	return 0;
}

int gxmContextKick(SceGxmContext *context, SceUInt32 flags,
		   const SceGxmNotification *vertexNotification,
		   const SceGxmNotification *fragmentNotification)
{
	SGX_KICKTA_COMMON *common = &gxmKick.sKickTACommon;
	const PVRSRV_CLIENT_MEM_INFO *block;
	SceBool fragment = (flags & GXM_KICK_FRAGMENT) != 0;
	SceBool partial;
	SceUInt32 zlsControl;
	SceUInt32 ispMisc;
	SceUInt32 sceneFlags;
	SceUInt32 kickFlags;
	SceUInt32 vdmStart;
	SceUInt32 surfaceSync;
	const SceUInt32 *csc0;
	const SceUInt32 *csc1;
	SGX_KICKTA_OUTPUT output;

	gxmRazorMarker(0x1d000000 | (gxmGlobals.sceneIndex & 0xff) << 16,
		       gxmGlobals.frameIndex & 0xffff);
	if (gxmDisplayQueueIsCurrentThread())
		return SCE_GXM_ERROR_INVALID_THREAD;
	if (vertexNotification != NULL) {
		int ret = gxmNotificationCheckAddress(vertexNotification->address);

		if (ret != 0)
			return ret;
	}
	if (fragmentNotification != NULL) {
		int ret = gxmNotificationCheckAddress(fragmentNotification->address);

		if (ret != 0)
			return ret;
	}

	/* a partial render stores the depth and stencil for the next one to load */
	zlsControl = context->depthStencil.zlsControl;
	partial = SCE_FALSE;
	ispMisc = 0;
	if (fragment && (flags & GXM_KICK_PARTIAL) && (zlsControl & 0x100000)) {
		ispMisc = (zlsControl & 2) ? 0 : 0x100;
		zlsControl |= 4;
		partial = SCE_TRUE;
	}

	gxmContextWriteKickFence(context);
	gxmContextBuildBackground(context);

	if (gxmRazorMagic == GXM_CALLBACKS_MAGIC)
		gxmRazorCallbacks.kick(gxmRingBufferUsed(&context->vdmRing, &context->vdmBuffer),
				       gxmRingBufferUsed(&context->vertexRing, &context->vertexBuffer),
				       gxmRingBufferUsed(&context->fragmentRing, &context->fragmentBuffer),
				       gxmRingBufferUsed(&context->fragmentUsseRing,
							 &context->fragmentUsseBuffer));

	vdmStart = context->vdmRing.memory + context->vdmRing.lastOffset;
	gxmRingBufferKicked(&context->vdmRing, &context->vdmBuffer);
	gxmRingBufferKicked(&context->vertexRing, &context->vertexBuffer);
	if (fragment) {
		gxmRingBufferKicked(&context->fragmentRing, &context->fragmentBuffer);
		gxmRingBufferKicked(&context->fragmentUsseRing, &context->fragmentUsseBuffer);
	}

	gxmKick.hRenderContext = gxmGlobals.top.renderContext;
	common->ui16MaxDrawCallsPerCore = context->scene.tuning.count;
	common->ui16PrimitiveSplitThreshold = context->scene.tuning.limit;
	common->aui32SpecObject[0] = context->backgroundState[0];
	common->aui32SpecObject[1] = context->backgroundState[1];
	common->aui32SpecObject[2] = context->backgroundState[2];
	common->ui32BGObjWidth = context->colorWidth;
	common->ui32BGObjHeight = context->colorHeight;
	surfaceSync = (SceUInt32)context->fragmentSyncObject;
	gxmKick.pahRenderSurfSyncInfo = context->fragmentSyncObject != NULL ? &surfaceSync : NULL;
	gxmKick.sPixEventUpdateDevAddr = 0;
	gxmKick.psPixEventUpdateList = NULL;
	gxmKick.psHWBgObjPrimPDSUpdateList = NULL;
	common->ui32ISPIPFMisc = ispMisc;
	common->sISPZLoadBase = (SceUInt32)context->depthStencil.loadDepthData;
	common->sISPStencilLoadBase = (SceUInt32)context->depthStencil.loadStencilData;
	common->sISPZStoreBase = (SceUInt32)context->depthStencil.storeDepthData;
	common->sISPStencilStoreBase = (SceUInt32)context->depthStencil.storeStencilData;
	common->bSeperateStencilLoad = context->depthStencil.loadStencilData != NULL;
	common->bSeperateStencilStore = context->depthStencil.storeStencilData != NULL;
	common->ui32ZLSCtl = zlsControl;
	gxmKick.hRTDataSet = (SceUInt32)context->renderTarget;
	common->ui32ISPBGObjDepth = gxmFloatBits(context->depthStencil.backgroundDepth);
	common->ui32ISPBGObj = context->depthStencil.backgroundControl;
	common->ui32ISPSceneBGObj = context->depthStencil.backgroundControl;
	common->ui32EDMPixelPDSAddr = (SceUInt32)context->pixelState.code;
	common->ui32EDMPixelPDSDataSize = context->pixelState.dataSize;
	common->ui32EDMPixelPDSInfo = context->pixelState.flags;
	common->ui32ISPPerpendicular = 0x1e3ce508;
	common->ui32ISPCullValue = 0x1e3ce508;
	common->ui32TAScreenXMax = context->tileLimitX;
	common->ui32TAScreenYMax = context->tileLimitY;
	common->ui32ISPDBias = context->scene.settings.offsets;
	common->bPerQuadrantPixelSplit = context->scene.settings.mode == 1;
	common->ui32ISPValidId = 0;
	common->ui32PixelBE = 0;
	csc0 = gxmYuvProfileCoefficients[context->scene.settings.yuvProfile[0]];
	csc1 = gxmYuvProfileCoefficients[context->scene.settings.yuvProfile[1]];
	common->ui32CSC0Coeff01 = csc0[0];
	common->ui32CSC0Coeff23 = csc0[1];
	common->ui32CSC0Coeff45 = csc0[2];
	common->ui32CSC0Coeff67 = csc0[3];
	common->ui32CSC0Coeff89 = csc0[4];
	common->ui32CSC0Coeff1011 = csc0[5];
	common->ui32CSC1Coeff01 = csc1[0];
	common->ui32CSC1Coeff23 = csc1[1];
	common->ui32CSC1Coeff45 = csc1[2];
	common->ui32CSC1Coeff67 = csc1[3];
	common->ui32CSC1Coeff89 = csc1[4];
	common->ui32CSC1Coeff1011 = csc1[5];
	common->ui32DoublePixelPartitions = context->colorSurface.outputRegisterSize;
	common->ui32TriangleSplitPixelThreshold = context->scene.settings.modeValue;
	common->sVisTestResultMem = (SceUInt32)context->scene.settings.visibilityBuffer;
	common->ui32VisTestCount = context->scene.settings.visibilityStride >> 2;
	common->ui32USEStoreRange = 0;
	common->ui32USELoadRedirect = 0;
	common->sTABufferCtlStreamBase = vdmStart;
	common->ui32MTECtrl = (context->scene.tuning.option & 0x80) | 0x22;

	/* the GPU writes each ring's kicked offset where gxmRingBufferRefill waits for it */
	block = gxmGlobals.top.contexts.block;
	gxmKick.asTAStatusUpdate[0].statusAddress = gxmMemInfoDevAddr(block, context->vdmRing.readOffset);
	gxmKick.asTAStatusUpdate[0].statusValue = context->vdmRing.lastOffset;
	gxmKick.asTAStatusUpdate[0].memoryHandle = block->hKernelMemInfo;
	gxmKick.asTAStatusUpdate[1].statusAddress = gxmMemInfoDevAddr(block, context->vertexRing.readOffset);
	gxmKick.asTAStatusUpdate[1].statusValue = context->vertexRing.lastOffset;
	gxmKick.asTAStatusUpdate[1].memoryHandle = block->hKernelMemInfo;
	if (vertexNotification != NULL) {
		gxmKick.asTAStatusUpdate[2].statusAddress =
			gxmMemInfoDevAddr(gxmGlobals.top.notifications, vertexNotification->address);
		gxmKick.asTAStatusUpdate[2].statusValue = vertexNotification->value;
		gxmKick.asTAStatusUpdate[2].memoryHandle = gxmGlobals.top.notifications->hKernelMemInfo;
		common->ui32NumTAStatusVals = 3;
	} else {
		common->ui32NumTAStatusVals = 2;
	}
	if (fragment) {
		gxmKick.as3DStatusUpdate[0].statusAddress =
			gxmMemInfoDevAddr(block, context->fragmentRing.readOffset);
		gxmKick.as3DStatusUpdate[0].statusValue = context->fragmentRing.lastOffset;
		gxmKick.as3DStatusUpdate[0].memoryHandle = block->hKernelMemInfo;
		gxmKick.as3DStatusUpdate[1].statusAddress =
			gxmMemInfoDevAddr(block, context->fragmentUsseRing.readOffset);
		gxmKick.as3DStatusUpdate[1].statusValue = context->fragmentUsseRing.lastOffset;
		gxmKick.as3DStatusUpdate[1].memoryHandle = block->hKernelMemInfo;
		if (fragmentNotification != NULL) {
			gxmKick.as3DStatusUpdate[2].statusAddress =
				gxmMemInfoDevAddr(gxmGlobals.top.notifications,
						  fragmentNotification->address);
			gxmKick.as3DStatusUpdate[2].statusValue = fragmentNotification->value;
			gxmKick.as3DStatusUpdate[2].memoryHandle =
				gxmGlobals.top.notifications->hKernelMemInfo;
			common->ui32Num3DStatusVals = 3;
		} else {
			common->ui32Num3DStatusVals = 2;
		}
	} else {
		common->ui32Num3DStatusVals = 0;
	}
	common->ui32NumSrcSyncs = 0;

	sceneFlags = context->sceneFlags;
	if (fragment && (sceneFlags & 4))
		kickFlags = (sceneFlags & 8) ? 0x44670063 : 0x44470063;
	else
		kickFlags = (sceneFlags & 8) ? 0x44270063 : 0x44070063;
	kickFlags |= sceGxmColorSurfaceIsEnabled(&context->colorSurface) ? 8 : 0x4000;
	kickFlags |= (zlsControl & (GXM_ZLS_LOAD_DEPTH | GXM_ZLS_STORE_DEPTH)) ? 0x80000 : 0;
	kickFlags |= (zlsControl & (GXM_ZLS_LOAD_STENCIL | GXM_ZLS_STORE_STENCIL)) ? 0x100000 : 0;
	kickFlags |= context->lastKickVertexOnly ? 0 : 4;
	kickFlags |= fragment << 9;
	kickFlags |= (sceneFlags & 2) ? 0x800000 : 0;
	kickFlags |= (fragment && (sceneFlags & 1)) << 24;
	common->ui32Frame = gxmGlobals.frameIndex;
	common->ui32KickFlags = kickFlags;
	common->ui32SceneFlags = ((flags & GXM_KICK_VDM_FULL) ? 1 : 0) |
				 ((flags & GXM_KICK_VERTEX_FULL) ? 2 : 0) |
				 ((flags & GXM_KICK_FRAGMENT_FULL) ? 4 : 0) |
				 ((flags & GXM_KICK_USSE_FULL) ? 8 : 0) |
				 ((flags & GXM_KICK_MID_SCENE) ? 16 : 0);
	common->ui32Scene = gxmGlobals.sceneIndex;
	gxmKick.hTQContext = gxmGlobals.top.transferQueue;

	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC) {
		SceUInt32 frame = gxmGlobals.frameIndex;

		if (gxmInitCallbacks.isCapturing() || gxmInitCallbacks.kickCapture()) {
			GxmKickCaptureInfo info;
			int i;

			info.context = context;
			info.frame = frame;
			info.fragment = fragment;
			info.vdmStart = vdmStart;
			info.reserved = 0;
			info.kick = &gxmKick;
			memset(info.usseBases, 0, sizeof(info.usseBases));
			for (i = 0; i < 4; i++) {
				info.usseBases[i] = gxmGlobals.top.usse[0].base[0] + i * 0x800000;
				info.usseBases[4 + i] = gxmGlobals.top.usse[0].base[1] + i * 0x800000;
			}
			info.usseBases[8] = gxmGlobals.top.usse[1].base[0];
			info.usseBases[9] = gxmGlobals.top.usse[1].base[1];
			gxmInitCallbacks.kickCaptureInfo(&info);
			gxmInitCallbacks.colorSurfaceCaptured(
				gxmInitCallbacks.captureColorSurface(context->colorSurface.control,
								     &context->colorSurface.format,
								     context->colorSurface.outputRegisterSize,
								     &context->colorSurface.backgroundTex));
		}
	}

	if (SceGpuEs4User_F0CCE9DB(&gxmGlobals.top, &gxmKick, &output) != 0)
		return SCE_GXM_ERROR_DRIVER;

	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC) {
		if (gxmInitCallbacks.isCapturing()) {
			if (fragment)
				sceGxmFinish(context);
			gxmInitCallbacks.drawCapture();
		}
		gxmInitCallbacks.contextWritten(context);
	}

	gxmContextResetState(context, !fragment, flags & SCE_GXM_MIDSCENE_PRESERVE_DEFAULT_UNIFORM_BUFFERS);
	context->lastKickVertexOnly = !fragment;
	context->sceneFlags &= ~2;
	if (fragment) {
		context->flags |= GXM_CONTEXT_FLAG_BACKGROUND;
		if (partial)
			context->depthStencil.zlsControl |= 2;
	}
	gxmRazorMarker(0x1e000000 | (common->ui32SceneFlags & 0xff) << 16,
		       gxmGlobals.frameIndex & 0xffff);
	return 0;
}

int gxmContextKickIfFull(SceGxmContext *context)
{
	SceBool vdmFull = context->vdmRing.threshold <= context->vdmBuffer.cur;
	SceBool vertexFull = context->vertexRing.threshold <= context->vertexBuffer.cur;
	SceBool fragmentFull = context->fragmentRing.threshold <= context->fragmentBuffer.cur;
	SceBool usseFull = context->fragmentUsseRing.threshold <= context->fragmentUsseBuffer.cur;
	SceBool vertexStageFull = vdmFull | vertexFull;
	SceBool fragmentStageFull = fragmentFull | usseFull;
	SceUInt32 flags;

	if (!vertexStageFull && !fragmentStageFull)
		return 0;
	if (gxmRazorMagic == GXM_CALLBACKS_MAGIC)
		gxmRazorCallbacks.kickIfFull(vdmFull, vertexFull, fragmentFull, usseFull);

	flags = SCE_GXM_MIDSCENE_PRESERVE_DEFAULT_UNIFORM_BUFFERS;
	if (fragmentStageFull)
		flags |= GXM_KICK_FRAGMENT | GXM_KICK_PARTIAL;
	if (vdmFull)
		flags |= GXM_KICK_VDM_FULL;
	flags |= vertexFull << 4 | fragmentFull << 5 | usseFull << 6;
	return gxmContextKick(context, flags, NULL, NULL);
}

int sceGxmPadHeartbeat(const SceGxmColorSurface *displaySurface,
		       struct SceGxmSyncObject *displaySyncObject)
{
	if (gxmRazorMagic == GXM_CALLBACKS_MAGIC)
		gxmRazorCallbacks.padHeartbeat(displaySurface, displaySyncObject);
	return 0;
}
