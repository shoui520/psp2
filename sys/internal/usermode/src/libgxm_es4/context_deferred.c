/*
 * SceGxm: deferred contexts and the command lists they record.
 */

#include "gxm_internal.h"

#define GXM_LIST_TERMINATOR	0xc0debf08	/* the last block of a command list */
#define GXM_LIST_MARKER		0xc0debf09	/* returned to after a list, while capturing */

/* a buffer is optional; when given it is aligned and at least the minimum size */
static inline int gxmCheckDeferredBuffer(void *const *mem, const uint32_t *size)
{
	if (*mem == NULL)
		return *size != 0 ? SCE_GXM_ERROR_INVALID_VALUE : 0;
	if ((SceUInt32)*mem & 3)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (*size < SCE_GXM_MINIMUM_DEFERRED_CONTEXT_BUFFER_SIZE)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (*size & 3)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	return 0;
}

int sceGxmCreateDeferredContext(const SceGxmDeferredContextParams *params,
				SceGxmContext **deferredContext)
{
	SceGxmContext *context;
	int ret;

	if (!gxmGlobals.initialized)
		return SCE_GXM_ERROR_UNINITIALIZED;
	if (params == NULL || params->hostMem == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if ((SceUInt32)params->hostMem & 3)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (params->hostMemSize < SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (params->hostMemSize & 3)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (params->vdmCallback == NULL || params->vertexCallback == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (gxmGlobals.compatFlags & GXM_COMPAT_FRAGMENT_CALLBACK_VALUE) {
		if (params->fragmentCallback == NULL)
			return SCE_GXM_ERROR_INVALID_VALUE;
	} else if (params->fragmentCallback == NULL) {
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	ret = gxmCheckDeferredBuffer(&params->vdmBufferMem, &params->vdmBufferMemSize);
	if (ret != 0)
		return ret;
	ret = gxmCheckDeferredBuffer(&params->vertexBufferMem, &params->vertexBufferMemSize);
	if (ret != 0)
		return ret;
	ret = gxmCheckDeferredBuffer(&params->fragmentBufferMem, &params->fragmentBufferMemSize);
	if (ret != 0)
		return ret;
	if (params->vdmBufferMem != NULL && params->vertexBufferMem != NULL &&
	    gxmBuffersOverlap(params->vdmBufferMem, params->vdmBufferMemSize,
			      params->vertexBufferMem, params->vertexBufferMemSize))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (params->vdmBufferMem != NULL && params->fragmentBufferMem != NULL &&
	    gxmBuffersOverlap(params->vdmBufferMem, params->vdmBufferMemSize,
			      params->fragmentBufferMem, params->fragmentBufferMemSize))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (params->vertexBufferMem != NULL && params->fragmentBufferMem != NULL &&
	    gxmBuffersOverlap(params->vertexBufferMem, params->vertexBufferMemSize,
			      params->fragmentBufferMem, params->fragmentBufferMemSize))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (deferredContext == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;

	context = params->hostMem;
	sceClibMemset(context, 0, GXM_DEFERRED_CONTEXT_SIZE);
	context->type = SCE_GXM_CONTEXT_TYPE_DEFERRED;
	gxmChunkBufferInitDeferred(&context->vdmSource, &context->vdmBuffer, params->vdmBufferMem,
				   params->vdmBufferMemSize, params->vdmCallback,
				   params->userData, 1);
	gxmChunkBufferInitDeferred(&context->vertexSource, &context->vertexBuffer,
				   params->vertexBufferMem, params->vertexBufferMemSize,
				   params->vertexCallback, params->userData, 0);
	gxmChunkBufferInitDeferred(&context->fragmentSource, &context->fragmentBuffer,
				   params->fragmentBufferMem, params->fragmentBufferMemSize,
				   params->fragmentCallback, params->userData, 0);
	gxmMteStateInit(&context->state);
	context->listTileYMax = 0;
	context->flags |= GXM_CONTEXT_FLAG_DEPTH_STENCIL_USED;
	context->listDepthStencilUsage &= ~GXM_LIST_USES_ALL;
	context->listSerial = gxmCounterIncrement();
	*deferredContext = context;
	return 0;
}

int sceGxmDestroyDeferredContext(SceGxmContext *deferredContext)
{
	if (!gxmGlobals.initialized)
		return SCE_GXM_ERROR_UNINITIALIZED;
	if (deferredContext == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (deferredContext->type != SCE_GXM_CONTEXT_TYPE_DEFERRED)
		return SCE_GXM_ERROR_INVALID_VALUE;
	return 0;
}

int sceGxmBeginCommandList(SceGxmContext *deferredContext)
{
	SceUInt32 *start;

	if (deferredContext == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (deferredContext->type != SCE_GXM_CONTEXT_TYPE_DEFERRED)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (deferredContext->insideScene)
		return SCE_GXM_ERROR_WITHIN_COMMAND_LIST;
	start = gxmChunkBufferReserve(&deferredContext->vdmBuffer, 4, 4);
	if (start == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	deferredContext->listStart = start;
	deferredContext->insideScene = 1;
	return 0;
}

int sceGxmEndCommandList(SceGxmContext *deferredContext, SceGxmCommandList *commandList)
{
	SceUInt32 *block, *start;
	SceUInt32 end, tileYMax;

	if (deferredContext == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (deferredContext->type != SCE_GXM_CONTEXT_TYPE_DEFERRED)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (commandList == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!deferredContext->insideScene)
		return SCE_GXM_ERROR_NOT_WITHIN_COMMAND_LIST;

	/* the terminator goes in the space kept for a link */
	end = deferredContext->vdmBuffer.end;
	block = (SceUInt32 *)((deferredContext->vdmBuffer.cur + 3) & ~3);
	deferredContext->vdmBuffer.cur = (SceUInt32)&block[2];
	if ((SceUInt32)&block[2] > end)
		deferredContext->vdmBuffer.end = (SceUInt32)&block[2];
	block[0] = 0;
	block[1] = GXM_LIST_TERMINATOR;
	start = deferredContext->listStart;
	commandList->returnLink = block;
	commandList->start = start;
	commandList->depthStencilUsage = deferredContext->listDepthStencilUsage;
	tileYMax = deferredContext->listTileYMax;
	commandList->reserved[0] = 0;
	commandList->reserved[1] = 0;
	commandList->reserved[2] = 0;
	commandList->tileYMax = tileYMax;
	commandList->serial = deferredContext->listSerial;
	gxmContextResetState(deferredContext, 0, 0);
	deferredContext->listTileYMax = 0;
	deferredContext->insideScene = 0;
	deferredContext->flags |= GXM_CONTEXT_FLAG_DEPTH_STENCIL_USED;
	deferredContext->listDepthStencilUsage &= ~GXM_LIST_USES_ALL;
	return 0;
}

int sceGxmExecuteCommandList(SceGxmContext *immediateContext, SceGxmCommandList *commandList)
{
	SceUInt32 returnWords[2];
	const SceUInt32 *marker;
	int ret;

	if (immediateContext == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (immediateContext->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (commandList == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!immediateContext->insideScene)
		return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
	if (commandList->start == NULL || commandList->returnLink == NULL)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (commandList->tileYMax > immediateContext->tileLimitY >> 5)
		return SCE_GXM_ERROR_INVALID_REGION_CLIP_IN_COMMAND_LIST;
	ret = gxmContextCheckCommandList(immediateContext, commandList);
	if (ret != 0)
		return ret;
	marker = NULL;
	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC && gxmInitCallbacks.isCapturing()) {
		returnWords[0] = GXM_LIST_MARKER;
		returnWords[1] = commandList->serial;
		marker = returnWords;
	}
	ret = gxmVdmWriteLink(&immediateContext->vdmBuffer, (SceUInt32)commandList->start, marker);
	if (ret != 0)
		return ret;
	*commandList->returnLink = immediateContext->vdmBuffer.cur >> 2;
	gxmMteStateInvalidate(&immediateContext->state);
	immediateContext->appliedVertexState = NULL;
	immediateContext->flags |= GXM_CONTEXT_FLAG_VERTEX_PROGRAM;
	return 0;
}

static inline int gxmGetDeferredBuffer(const SceGxmContext *deferredContext,
				       const GxmChunkBuffer *buffer, void **mem)
{
	if (deferredContext == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (deferredContext->type != SCE_GXM_CONTEXT_TYPE_DEFERRED)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (mem == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (deferredContext->insideScene)
		return SCE_GXM_ERROR_WITHIN_COMMAND_LIST;
	*mem = (void *)buffer->cur;
	return 0;
}

static inline int gxmSetDeferredBuffer(SceGxmContext *deferredContext, GxmDeferredBuffer *source,
				       GxmChunkBuffer *buffer, void *mem, SceSize size)
{
	if (deferredContext == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (deferredContext->type != SCE_GXM_CONTEXT_TYPE_DEFERRED)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (mem == NULL) {
		if (size != 0)
			return SCE_GXM_ERROR_INVALID_VALUE;
	} else {
		if ((SceUInt32)mem & 3)
			return SCE_GXM_ERROR_INVALID_ALIGNMENT;
		if (size < SCE_GXM_MINIMUM_DEFERRED_CONTEXT_BUFFER_SIZE)
			return SCE_GXM_ERROR_INVALID_VALUE;
		if (size & 3)
			return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	if (deferredContext->insideScene)
		return SCE_GXM_ERROR_WITHIN_COMMAND_LIST;
	gxmChunkBufferReset(source, buffer, mem, size);
	return 0;
}

int sceGxmGetDeferredContextVdmBuffer(const SceGxmContext *deferredContext, void **mem)
{
	return gxmGetDeferredBuffer(deferredContext, &deferredContext->vdmBuffer, mem);
}

int sceGxmSetDeferredContextVdmBuffer(SceGxmContext *deferredContext, void *mem, SceSize size)
{
	return gxmSetDeferredBuffer(deferredContext, &deferredContext->vdmSource,
				    &deferredContext->vdmBuffer, mem, size);
}

int sceGxmGetDeferredContextVertexBuffer(const SceGxmContext *deferredContext, void **mem)
{
	return gxmGetDeferredBuffer(deferredContext, &deferredContext->vertexBuffer, mem);
}

int sceGxmSetDeferredContextVertexBuffer(SceGxmContext *deferredContext, void *mem, SceSize size)
{
	return gxmSetDeferredBuffer(deferredContext, &deferredContext->vertexSource,
				    &deferredContext->vertexBuffer, mem, size);
}

int sceGxmGetDeferredContextFragmentBuffer(const SceGxmContext *deferredContext, void **mem)
{
	return gxmGetDeferredBuffer(deferredContext, &deferredContext->fragmentBuffer, mem);
}

int sceGxmSetDeferredContextFragmentBuffer(SceGxmContext *deferredContext, void *mem, SceSize size)
{
	return gxmSetDeferredBuffer(deferredContext, &deferredContext->fragmentSource,
				    &deferredContext->fragmentBuffer, mem, size);
}
