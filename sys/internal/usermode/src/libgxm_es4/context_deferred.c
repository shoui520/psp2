/*
 * SceGxm: deferred contexts and the command lists they record.
 */

#include "gxm_internal.h"

#define GXM_LIST_TERMINATOR	0xc0debf08	/* the last block of a command list */
#define GXM_LIST_MARKER		0xc0debf09	/* returned to after a list, while capturing */

/*
 * A buffer of sceGxmCreateDeferredContext is optional; when given it is
 * aligned and at least the minimum size. The debug build names the
 * parameter, from line on.
 */
#define GXM_CHECK_DEFERRED_BUFFER(buffer, line)						\
	_SCE_MACRO_BEGIN								\
		if (params->buffer##Mem == NULL) {					\
			if (params->buffer##MemSize != 0) {				\
				GXM_DBG_ERROR((line) + 18, "Parameter params->" #buffer	\
					      "MemSize to function sceGxmCreateDeferredContext "	\
					      "has non-zero value %d, but the buffer is NULL.",	\
					      params->buffer##MemSize);			\
				return SCE_GXM_ERROR_INVALID_VALUE;			\
			}								\
		} else if ((SceUInt32)params->buffer##Mem & 3) {			\
			GXM_DBG_ERROR(line, "Parameter params->" #buffer "Mem to function "	\
				      "sceGxmCreateDeferredContext has value %p, which is not "	\
				      "aligned to 4 bytes.", params->buffer##Mem);		\
			return SCE_GXM_ERROR_INVALID_ALIGNMENT;				\
		} else if (params->buffer##MemSize < SCE_GXM_MINIMUM_DEFERRED_CONTEXT_BUFFER_SIZE) { \
			GXM_DBG_ERROR((line) + 6, "Parameter params->" #buffer "MemSize to "	\
				      "function sceGxmCreateDeferredContext has value %u, which "	\
				      "is less than the minimum value of %d.",		\
				      params->buffer##MemSize,					\
				      SCE_GXM_MINIMUM_DEFERRED_CONTEXT_BUFFER_SIZE);		\
			return SCE_GXM_ERROR_INVALID_VALUE;				\
		} else if (params->buffer##MemSize & 3) {				\
			GXM_DBG_ERROR((line) + 11, "Parameter params->" #buffer "MemSize to "	\
				      "function sceGxmCreateDeferredContext has value %u, which "	\
				      "is not aligned to 4.", params->buffer##MemSize);	\
			return SCE_GXM_ERROR_INVALID_ALIGNMENT;				\
		}									\
	_SCE_MACRO_END

int sceGxmCreateDeferredContext(const SceGxmDeferredContextParams *params,
				SceGxmContext **deferredContext)
{
	SceGxmContext *context;
	int ret;

	if (!gxmGlobals.initialized) {
		GXM_DBG_ERROR(63, "Function sceGxmCreateDeferredContext has been called while the "
			      "library is uninitialized.");
		return SCE_GXM_ERROR_UNINITIALIZED;
	}
	if (params == NULL) {
		GXM_DBG_ERROR(68, "Parameter params to sceGxmCreateDeferredContext is NULL.");
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (params->hostMem == NULL) {
		GXM_DBG_ERROR_NULL(73, sceGxmCreateDeferredContext, params->hostMem);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if ((SceUInt32)params->hostMem & 3) {
		GXM_DBG_ERROR(78, "Parameter params->hostMem to function sceGxmCreateDeferredContext "
			      "has value %p, which is not aligned to 4 bytes.", params->hostMem);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	if (params->hostMemSize < SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE) {
		GXM_DBG_ERROR(84, "Parameter params->hostMemSize to function "
			      "sceGxmCreateDeferredContext has value %u, which is less than the "
			      "minimum value of %d.", params->hostMemSize,
			      SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (params->hostMemSize & 3) {
		GXM_DBG_ERROR(89, "Parameter params->hostMemSize to function "
			      "sceGxmCreateDeferredContext has value %u, which is not aligned to 4.",
			      params->hostMemSize);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	if (params->vdmCallback == NULL) {
		GXM_DBG_ERROR_NULL(94, sceGxmCreateDeferredContext, params->vdmCallback);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (params->vertexCallback == NULL) {
		GXM_DBG_ERROR_NULL(98, sceGxmCreateDeferredContext, params->vertexCallback);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (gxmGlobals.compatFlags & GXM_COMPAT_FRAGMENT_CALLBACK_VALUE) {
		if (params->fragmentCallback == NULL) {
			GXM_DBG_ERROR_NULL(105, sceGxmCreateDeferredContext, params->fragmentCallback);
			return SCE_GXM_ERROR_INVALID_VALUE;
		}
	} else if (params->fragmentCallback == NULL) {
		GXM_DBG_ERROR_NULL(110, sceGxmCreateDeferredContext, params->fragmentCallback);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	GXM_CHECK_DEFERRED_BUFFER(vdmBuffer, 119);
	GXM_CHECK_DEFERRED_BUFFER(vertexBuffer, 146);
	GXM_CHECK_DEFERRED_BUFFER(fragmentBuffer, 173);
	if (params->vdmBufferMem != NULL && params->vertexBufferMem != NULL &&
	    gxmBuffersOverlap(params->vdmBufferMem, params->vdmBufferMemSize,
			      params->vertexBufferMem, params->vertexBufferMemSize)) {
		GXM_DBG_ERROR(203, "The VDM and vertex buffers to function "
			      "sceGxmCreateDeferredContext are overlapping.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (params->vdmBufferMem != NULL && params->fragmentBufferMem != NULL &&
	    gxmBuffersOverlap(params->vdmBufferMem, params->vdmBufferMemSize,
			      params->fragmentBufferMem, params->fragmentBufferMemSize)) {
		GXM_DBG_ERROR(214, "The VDM and fragment buffers to function "
			      "sceGxmCreateDeferredContext are overlapping.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (params->vertexBufferMem != NULL && params->fragmentBufferMem != NULL &&
	    gxmBuffersOverlap(params->vertexBufferMem, params->vertexBufferMemSize,
			      params->fragmentBufferMem, params->fragmentBufferMemSize)) {
		GXM_DBG_ERROR(225, "The vertex and fragment buffers to function "
			      "sceGxmCreateDeferredContext are overlapping.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (deferredContext == NULL) {
		GXM_DBG_ERROR(231, "Parameter deferredContext to sceGxmCreateDeferredContext is NULL.");
		return SCE_GXM_ERROR_INVALID_POINTER;
	}

	ret = 0;
#if SCE_DBG_ASSERTS_ENABLED
	/* memory GXM cannot use is reported, and the context is made all the same */
	ret = gxmMemoryCheckCachedParameter(params->hostMem, params->hostMemSize,
					    "params->hostMem", "sceGxmCreateDeferredContext");
	if (ret == 0 && params->vdmBufferMem != NULL)
		ret = gxmMemoryCheckParameter(params->vdmBufferMem, params->vdmBufferMemSize,
					      "params->vdmBufferMem", "sceGxmCreateDeferredContext");
	if (ret == 0 && params->vertexBufferMem != NULL)
		ret = gxmMemoryCheckParameter(params->vertexBufferMem, params->vertexBufferMemSize,
					      "params->vertexBufferMem", "sceGxmCreateDeferredContext");
	if (ret == 0 && params->fragmentBufferMem != NULL)
		ret = gxmMemoryCheckParameter(params->fragmentBufferMem, params->fragmentBufferMemSize,
					      "params->fragmentBufferMem", "sceGxmCreateDeferredContext");
#endif
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
#if SCE_DBG_ASSERTS_ENABLED
	context->listFragmentPassFlags = 0;
#endif
	context->flags |= GXM_CONTEXT_FLAG_DEPTH_STENCIL_USED;
	context->listDepthStencilUsage &= ~GXM_LIST_USES_ALL;
#if SCE_DBG_ASSERTS_ENABLED
	context->listFrontVisibilityIndexMax = -1;
	context->listBackVisibilityIndexMax = -1;
#endif
	context->listSerial = gxmCounterIncrement();
	*deferredContext = context;
	return ret;
}

int sceGxmDestroyDeferredContext(SceGxmContext *deferredContext)
{
	if (!gxmGlobals.initialized) {
		GXM_DBG_ERROR(301, "Function sceGxmDestroyDeferredContext has been called while the "
			      "library is uninitialized.");
		return SCE_GXM_ERROR_UNINITIALIZED;
	}
	if (deferredContext == NULL) {
		GXM_DBG_ERROR(305, "Parameter deferredContext to sceGxmDestroyDeferredContext is NULL.");
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (deferredContext->type != SCE_GXM_CONTEXT_TYPE_DEFERRED) {
		GXM_DBG_ERROR(309, "Parameter deferredContext to sceGxmDestroyDeferredContext is not "
			      "a deferred context.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	return 0;
}

int sceGxmBeginCommandList(SceGxmContext *deferredContext)
{
	SceUInt32 *start;

	if (deferredContext == NULL) {
		GXM_DBG_ERROR_NULL(323, sceGxmBeginCommandList, deferredContext);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (deferredContext->type != SCE_GXM_CONTEXT_TYPE_DEFERRED) {
		GXM_DBG_ERROR(327, "Parameter deferredContext to sceGxmBeginCommandList is not a "
			      "deferred context.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (deferredContext->insideScene) {
		GXM_DBG_ERROR(333, "Function sceGxmBeginCommandList has been called from within a "
			      "command list.");
		return SCE_GXM_ERROR_WITHIN_COMMAND_LIST;
	}
	start = gxmChunkBufferReserve(&deferredContext->vdmBuffer, 4, 4);
	if (start == NULL) {
		GXM_DBG_ERROR(346, "Function sceGxmBeginCommandList failed to reserve memory for VDM.");
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	deferredContext->listStart = start;
	deferredContext->insideScene = 1;
	return 0;
}

int sceGxmEndCommandList(SceGxmContext *deferredContext, SceGxmCommandList *commandList)
{
	SceUInt32 *block, *start;
	SceUInt32 end, tileYMax;

	if (deferredContext == NULL) {
		GXM_DBG_ERROR_NULL(366, sceGxmEndCommandList, deferredContext);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (deferredContext->type != SCE_GXM_CONTEXT_TYPE_DEFERRED) {
		GXM_DBG_ERROR(370, "Parameter deferredContext to sceGxmEndCommandList is not a "
			      "deferred context.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (commandList == NULL) {
		GXM_DBG_ERROR_NULL(374, sceGxmEndCommandList, commandList);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (!deferredContext->insideScene) {
		GXM_DBG_ERROR(378, "Function sceGxmEndCommandList has been called without a previous "
			      "call to sceGxmBeginCommandList.");
		return SCE_GXM_ERROR_NOT_WITHIN_COMMAND_LIST;
	}

	/* the terminator goes in the space kept for a link */
	end = deferredContext->vdmBuffer.end;
	block = (SceUInt32 *)((deferredContext->vdmBuffer.current + 3) & ~3);
	deferredContext->vdmBuffer.current = (SceUInt32)&block[2];
#if SCE_DBG_ASSERTS_ENABLED
	deferredContext->vdmBuffer.reserved = (SceUInt32)&block[2];
#endif
	if ((SceUInt32)&block[2] > end)
		deferredContext->vdmBuffer.end = (SceUInt32)&block[2];
	block[0] = 0;
	block[1] = GXM_LIST_TERMINATOR;
	start = deferredContext->listStart;
	commandList->returnLink = block;
	commandList->start = start;
	commandList->depthStencilUsage = deferredContext->listDepthStencilUsage;
	tileYMax = deferredContext->listTileYMax;
#if SCE_DBG_ASSERTS_ENABLED
	commandList->tileYMax = tileYMax;
	commandList->frontVisibilityIndexMax = deferredContext->listFrontVisibilityIndexMax;
	commandList->backVisibilityIndexMax = deferredContext->listBackVisibilityIndexMax;
	commandList->fragmentPassFlags = deferredContext->listFragmentPassFlags;
#else
	commandList->reserved[0] = 0;
	commandList->reserved[1] = 0;
	commandList->reserved[2] = 0;
	commandList->tileYMax = tileYMax;
#endif
	commandList->serial = deferredContext->listSerial;
	gxmContextResetState(deferredContext, 0, 0);
	deferredContext->listTileYMax = 0;
#if SCE_DBG_ASSERTS_ENABLED
	deferredContext->listFragmentPassFlags = 0;
#endif
	deferredContext->insideScene = 0;
	deferredContext->flags |= GXM_CONTEXT_FLAG_DEPTH_STENCIL_USED;
	deferredContext->listDepthStencilUsage &= ~GXM_LIST_USES_ALL;
#if SCE_DBG_ASSERTS_ENABLED
	deferredContext->listFrontVisibilityIndexMax = -1;
	deferredContext->listBackVisibilityIndexMax = -1;
#endif
	return 0;
}

int sceGxmExecuteCommandList(SceGxmContext *immediateContext, SceGxmCommandList *commandList)
{
	SceUInt32 returnWords[2];
	const SceUInt32 *marker;
	SceUInt32 jumpTarget;
	int ret, err;

	if (immediateContext == NULL) {
		GXM_DBG_ERROR(452, "Parameter context to function sceGxmExecuteCommandList is NULL.");
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (immediateContext->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
		/* (sic) */
		GXM_DBG_ERROR(456, "Parameter context to sceGxmEndCommandList is not an immediate "
			      "context.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (commandList == NULL) {
		GXM_DBG_ERROR_NULL(460, sceGxmExecuteCommandList, commandList);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (!immediateContext->insideScene) {
		GXM_DBG_ERROR(466, "Function sceGxmExecuteCommandList has been called outside of a "
			      "scene.");
		return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
	}
	if (commandList->start == NULL || commandList->returnLink == NULL) {
		GXM_DBG_ERROR(472, "Parameter commandList is not a valid commandlist.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (commandList->tileYMax > immediateContext->tileLimitY >> 5) {
		GXM_DBG_ERROR(485, "One or more draw calls within the commandList parameter to "
			      "sceGxmExecuteCommandList use a region clip configuration where the "
			      "region clip max Y value (%d tiles) exceeds that of the scene valid "
			      "region max Y value (%d tiles).", commandList->tileYMax,
			      immediateContext->tileLimitY >> 5);
		return SCE_GXM_ERROR_INVALID_REGION_CLIP_IN_COMMAND_LIST;
	}
	ret = gxmContextCheckCommandList(immediateContext, commandList);
	if (ret != 0)
		return ret;
	err = 0;
#if SCE_DBG_ASSERTS_ENABLED
	/* reported once the list is linked */
	err = gxmDebugCheckCommandList(immediateContext, commandList);
#endif
	marker = NULL;
	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC && gxmInitCallbacks.isCapturing()) {
		returnWords[0] = GXM_LIST_MARKER;
		returnWords[1] = commandList->serial;
		marker = returnWords;
	}
	ret = gxmVdmWriteLink(&immediateContext->vdmBuffer, (SceUInt32)commandList->start, marker);
	if (ret != 0)
		return ret;
	jumpTarget = immediateContext->vdmBuffer.current;
	GXM_DBG_ASSERT(518, SCE_GXM_IS_ALIGNED(jumpTarget, 4));
	*commandList->returnLink = jumpTarget >> 2;
	gxmMteStateInvalidate(&immediateContext->state);
	immediateContext->appliedVertexState = NULL;
	immediateContext->flags |= GXM_CONTEXT_FLAG_VERTEX_PROGRAM;
#if SCE_DBG_ASSERTS_ENABLED
	gxmDebugReportError(err, "sceGxmExecuteCommandList");
#endif
	return err;
}

/*
 * The deferred context's buffers, between command lists. The debug build's
 * messages name the function, from line on.
 */
#define GXM_GET_DEFERRED_BUFFER(function, buffer, line)					\
	_SCE_MACRO_BEGIN								\
		if (deferredContext == NULL) {						\
			GXM_DBG_ERROR(line, "Parameter deferredContext to function "	\
				      #function " is NULL.");				\
			return SCE_GXM_ERROR_INVALID_POINTER;				\
		}									\
		if (deferredContext->type != SCE_GXM_CONTEXT_TYPE_DEFERRED) {		\
			GXM_DBG_ERROR((line) + 4, "Parameter deferredContext to function "	\
				      #function " is not a deferred context.");		\
			return SCE_GXM_ERROR_INVALID_VALUE;				\
		}									\
		if (mem == NULL) {							\
			GXM_DBG_ERROR((line) + 8, "Parameter mem to function " #function	\
				      " is NULL.");					\
			return SCE_GXM_ERROR_INVALID_POINTER;				\
		}									\
		if (deferredContext->insideScene) {					\
			GXM_DBG_ERROR((line) + 14, "Function " #function " has been called "	\
				      "from within a command list.");			\
			return SCE_GXM_ERROR_WITHIN_COMMAND_LIST;			\
		}									\
		*mem = (void *)deferredContext->buffer.current;				\
		return 0;								\
	_SCE_MACRO_END

#define GXM_SET_DEFERRED_BUFFER(function, source, buffer, line)				\
	_SCE_MACRO_BEGIN								\
		int err = 0;								\
										\
		if (deferredContext == NULL) {						\
			GXM_DBG_ERROR(line, "Parameter deferredContext to function "	\
				      #function " is NULL.");				\
			return SCE_GXM_ERROR_INVALID_POINTER;				\
		}									\
		if (deferredContext->type != SCE_GXM_CONTEXT_TYPE_DEFERRED) {		\
			GXM_DBG_ERROR((line) + 4, "Parameter deferredContext to " #function	\
				      " is not a deferred context.");			\
			return SCE_GXM_ERROR_INVALID_VALUE;				\
		}									\
		if (mem == NULL) {							\
			if (size != 0) {						\
				GXM_DBG_ERROR((line) + 28, "Parameter size to function "	\
					      #function " has non-zero value %d, but the "	\
					      "buffer is NULL.", size);			\
				return SCE_GXM_ERROR_INVALID_VALUE;			\
			}								\
		} else {								\
			if ((SceUInt32)mem & 3) {					\
				GXM_DBG_ERROR((line) + 11, "Parameter mem to function "	\
					      #function " has value %p, which is not "	\
					      "aligned to 4 bytes.", mem);		\
				return SCE_GXM_ERROR_INVALID_ALIGNMENT;			\
			}								\
			if (size < SCE_GXM_MINIMUM_DEFERRED_CONTEXT_BUFFER_SIZE) {	\
				GXM_DBG_ERROR((line) + 17, "Parameter size to function "	\
					      #function " has value %u, which is less than "	\
					      "the minimum value of %d.", size,		\
					      SCE_GXM_MINIMUM_DEFERRED_CONTEXT_BUFFER_SIZE);	\
				return SCE_GXM_ERROR_INVALID_VALUE;			\
			}								\
			if (size & 3) {							\
				GXM_DBG_ERROR((line) + 22, "Parameter size to function "	\
					      #function " has value %d, which is not a "	\
					      "multiple of 4 bytes.", size);		\
				return SCE_GXM_ERROR_INVALID_ALIGNMENT;			\
			}								\
		}									\
		if (deferredContext->insideScene) {					\
			GXM_DBG_ERROR((line) + 35, "Function " #function " has been called "	\
				      "from within a command list.");			\
			return SCE_GXM_ERROR_WITHIN_COMMAND_LIST;			\
		}									\
		if (mem != NULL)							\
			err = GXM_DBG_CHECK_MEMORY(mem, size, "mem", #function);	\
		gxmChunkBufferReset(&deferredContext->source, &deferredContext->buffer,	\
				    mem, size);						\
		return err;								\
	_SCE_MACRO_END

int sceGxmGetDeferredContextVdmBuffer(const SceGxmContext *deferredContext, void **mem)
{
	GXM_GET_DEFERRED_BUFFER(sceGxmGetDeferredContextVdmBuffer, vdmBuffer, 545);
}

int sceGxmSetDeferredContextVdmBuffer(SceGxmContext *deferredContext, void *mem, SceSize size)
{
	GXM_SET_DEFERRED_BUFFER(sceGxmSetDeferredContextVdmBuffer, vdmSource, vdmBuffer, 575);
}

int sceGxmGetDeferredContextVertexBuffer(const SceGxmContext *deferredContext, void **mem)
{
	GXM_GET_DEFERRED_BUFFER(sceGxmGetDeferredContextVertexBuffer, vertexBuffer, 643);
}

int sceGxmSetDeferredContextVertexBuffer(SceGxmContext *deferredContext, void *mem, SceSize size)
{
	GXM_SET_DEFERRED_BUFFER(sceGxmSetDeferredContextVertexBuffer, vertexSource, vertexBuffer,
				673);
}

int sceGxmGetDeferredContextFragmentBuffer(const SceGxmContext *deferredContext, void **mem)
{
	GXM_GET_DEFERRED_BUFFER(sceGxmGetDeferredContextFragmentBuffer, fragmentBuffer, 742);
}

int sceGxmSetDeferredContextFragmentBuffer(SceGxmContext *deferredContext, void *mem, SceSize size)
{
	GXM_SET_DEFERRED_BUFFER(sceGxmSetDeferredContextFragmentBuffer, fragmentSource,
				fragmentBuffer, 772);
}
