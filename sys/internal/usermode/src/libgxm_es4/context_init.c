/*
 * SceGxm: creating and destroying immediate contexts.
 */

#include "gxm_internal.h"

/*
 * A ring buffer is given, aligned and at least its minimum size. The debug
 * build names the parameter, from line on; Sony's message for a misaligned
 * vertex ring prints the VDM ring's address (address).
 */
#define GXM_CHECK_RING_BUFFER(buffer, minimumSize, line, address)				\
	_SCE_MACRO_BEGIN								\
		if (params->buffer##Mem == NULL) {					\
			GXM_DBG_ERROR(line, "Parameter params->" #buffer "Mem to function "	\
				      "sceGxmCreateContext is NULL.");			\
			return SCE_GXM_ERROR_INVALID_POINTER;				\
		}									\
		if ((SceUInt32)params->buffer##Mem & 3) {				\
			GXM_DBG_ERROR((line) + 5, "Parameter params->" #buffer "Mem to "	\
				      "function sceGxmCreateContext has value %p, which is not "	\
				      "aligned to 4 bytes.", address);			\
			return SCE_GXM_ERROR_INVALID_ALIGNMENT;				\
		}									\
		if (params->buffer##MemSize < (minimumSize)) {				\
			GXM_DBG_ERROR((line) + 11, "Parameter params->" #buffer "MemSize to "	\
				      "function sceGxmCreateContext has value %u, which is less "	\
				      "than the minimum value of %d.", params->buffer##MemSize,	\
				      minimumSize);					\
			return SCE_GXM_ERROR_INVALID_VALUE;				\
		}									\
		if (params->buffer##MemSize & 3) {					\
			GXM_DBG_ERROR((line) + 16, "Parameter params->" #buffer "MemSize to "	\
				      "function sceGxmCreateContext has value %u, which is not "	\
				      "aligned to 4.", params->buffer##MemSize);		\
			return SCE_GXM_ERROR_INVALID_ALIGNMENT;				\
		}									\
	_SCE_MACRO_END

/* two ring buffers of sceGxmCreateContext overlap */
#define GXM_CHECK_RING_OVERLAP(first, second, line, text)				\
	_SCE_MACRO_BEGIN								\
		if (gxmBuffersOverlap(params->first##Mem, params->first##MemSize,	\
				      params->second##Mem, params->second##MemSize)) {	\
			GXM_DBG_ERROR(line, "The " text " ring buffers to function "		\
				      "sceGxmCreateContext are overlapping.");		\
			return SCE_GXM_ERROR_INVALID_VALUE;				\
		}									\
	_SCE_MACRO_END

int sceGxmCreateContext(const SceGxmContextParams *params, SceGxmContext **context)
{
	SceGxmContext *created;
	SceUInt32 usseSegment;
	int ret, i;

	if (!gxmGlobals.initialized) {
		GXM_DBG_ERROR(75, "Function sceGxmCreateContext has been called while the library "
			      "is uninitialized.");
		return SCE_GXM_ERROR_UNINITIALIZED;
	}
	if (params == NULL) {
		GXM_DBG_ERROR_NULL(81, sceGxmCreateContext, params);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (params->hostMem == NULL) {
		GXM_DBG_ERROR_NULL(86, sceGxmCreateContext, params->hostMem);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if ((SceUInt32)params->hostMem & 3) {
		GXM_DBG_ERROR(91, "Parameter params->hostMem to function sceGxmCreateContext has "
			      "value %p, which is not aligned to 4 bytes.", params->hostMem);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	if (params->hostMemSize < SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE) {
		GXM_DBG_ERROR(97, "Parameter params->hostMemSize to function sceGxmCreateContext "
			      "has value %u, which is less than the minimum value of %d.",
			      params->hostMemSize, SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (params->hostMemSize & 3) {
		GXM_DBG_ERROR(102, "Parameter params->hostMemSize to function sceGxmCreateContext "
			      "has value %u, which is not aligned to 4.", params->hostMemSize);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	GXM_CHECK_RING_BUFFER(vdmRingBuffer, GXM_MINIMUM_VDM_RING_BUFFER_SIZE, 107,
			      params->vdmRingBufferMem);
	GXM_CHECK_RING_BUFFER(vertexRingBuffer, GXM_MINIMUM_VERTEX_RING_BUFFER_SIZE, 128,
			      params->vdmRingBufferMem);
	GXM_CHECK_RING_BUFFER(fragmentRingBuffer, GXM_MINIMUM_FRAGMENT_RING_BUFFER_SIZE, 149,
			      params->fragmentRingBufferMem);
	GXM_CHECK_RING_BUFFER(fragmentUsseRingBuffer, GXM_MINIMUM_FRAGMENT_USSE_RING_BUFFER_SIZE,
			      170, params->fragmentUsseRingBufferMem);
	/* the fragment USSE ring lies in a USSE segment */
	usseSegment = params->fragmentUsseRingBufferOffset >> 24;
	if (usseSegment != 9 && (usseSegment < 4 || usseSegment > 7)) {
		GXM_DBG_ERROR(190, "Parameter params->fragmentUsseRingBufferOffset is not a valid "
			      "fragment USSE offset.");
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	GXM_CHECK_RING_OVERLAP(vdmRingBuffer, vertexRingBuffer, 199, "VDM and vertex");
	GXM_CHECK_RING_OVERLAP(vdmRingBuffer, fragmentRingBuffer, 207, "VDM and fragment");
	GXM_CHECK_RING_OVERLAP(vdmRingBuffer, fragmentUsseRingBuffer, 215, "VDM and fragment USSE");
	GXM_CHECK_RING_OVERLAP(vertexRingBuffer, fragmentRingBuffer, 223, "vertex and fragment");
	GXM_CHECK_RING_OVERLAP(vertexRingBuffer, fragmentUsseRingBuffer, 231,
			       "vertex and fragment USSE");
	GXM_CHECK_RING_OVERLAP(fragmentRingBuffer, fragmentUsseRingBuffer, 239,
			       "fragment and fragment USSE");
	if (context == NULL) {
		GXM_DBG_ERROR(244, "Parameter immediateContext to function sceGxmCreateContext is "
			      "NULL.");
		return SCE_GXM_ERROR_INVALID_POINTER;
	}

	ret = 0;
#if SCE_DBG_ASSERTS_ENABLED
	/* memory GXM cannot use is reported, and the context is made all the same */
	ret = gxmMemoryCheckCachedParameter(params->hostMem, params->hostMemSize,
					    "params->hostMem", "sceGxmCreateContext");
	if (ret == 0)
		ret = gxmMemoryCheckParameter(params->vdmRingBufferMem, params->vdmRingBufferMemSize,
					      "params->vdmRingBufferMem", "sceGxmCreateContext");
	if (ret == 0)
		ret = gxmMemoryCheckParameter(params->vertexRingBufferMem,
					      params->vertexRingBufferMemSize,
					      "params->vertexRingBufferMem", "sceGxmCreateContext");
	if (ret == 0)
		ret = gxmMemoryCheckParameter(params->fragmentRingBufferMem,
					      params->fragmentRingBufferMemSize,
					      "params->fragmentRingBufferMem", "sceGxmCreateContext");
#endif
	created = params->hostMem;
	sceClibMemset(created, 0, sizeof(*created));
	created->type = SCE_GXM_CONTEXT_TYPE_IMMEDIATE;
	for (i = 0; i < 16; i++)
		created->vertexStreams[i] = GXM_UNSET_VERTEX_STREAM(i);
	for (i = 0; i < 15; i++) {
		created->vertexUniformBuffers[i] = GXM_UNSET_UNIFORM_BUFFER(i);
		created->fragmentUniformBuffers[i] = GXM_UNSET_UNIFORM_BUFFER(i);
	}
	created->slot = gxmSlotTableAdd(&gxmGlobals.top.contexts, created);
	if (created->slot == NULL) {
		GXM_DBG_ERROR(280, "Failed to find a free status block for the SceGxmContext.");
		return SCE_GXM_ERROR_OUT_OF_MEMORY;
	}
	gxmRingBufferInit(&created->vdmRing, &created->vdmBuffer, params->vdmRingBufferMem, 0,
			  params->vdmRingBufferMemSize, 1, &created->slot[0]);
	gxmRingBufferInit(&created->vertexRing, &created->vertexBuffer,
			  params->vertexRingBufferMem, 0, params->vertexRingBufferMemSize, 0,
			  &created->slot[1]);
	gxmRingBufferInit(&created->fragmentRing, &created->fragmentBuffer,
			  params->fragmentRingBufferMem, 0, params->fragmentRingBufferMemSize, 0,
			  &created->slot[2]);
	gxmRingBufferInit(&created->fragmentUsseRing, &created->fragmentUsseBuffer,
			  params->fragmentUsseRingBufferMem, params->fragmentUsseRingBufferOffset,
			  params->fragmentUsseRingBufferMemSize, 0, &created->slot[3]);
	created->nextScene.settings.secondaryMode = 1;
	created->nextScene.settings.pixelSplitThreshold = 0;
	created->nextScene.settings.yuvProfile[0] = 0;
	created->nextScene.settings.yuvProfile[1] = 1;
	created->nextScene.tuning.maxDrawCallsPerCore = 0;
	created->nextScene.tuning.nearClipMode = 0x80;
	created->nextScene.tuning.primitiveSplitThreshold = 1000;
	created->nextScene.settings.depthBiasAdjust = 0xe9;
	gxmMteStateInit(&created->state);
	created->flags = GXM_CONTEXT_FLAG_BACKGROUND;
	*context = created;
	return ret;
}

int sceGxmDestroyContext(SceGxmContext *context)
{
	int err;

	if (!gxmGlobals.initialized) {
		GXM_DBG_ERROR(343, "Function sceGxmDestroyContext has been called while the library "
			      "is uninitialized.");
		return SCE_GXM_ERROR_UNINITIALIZED;
	}
	if (context == NULL) {
		GXM_DBG_ERROR(349, "Parameter immediateContext to sceGxmDestroyContext is NULL.");
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
		GXM_DBG_ERROR(353, "Parameter immediateContext to sceGxmDestroyContext is not an "
			      "immediate context.");
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	sceGxmFinish(context);
	gxmSlotTableRemove(&gxmGlobals.top.contexts, (void *)context->slot);
	err = SceGpuEs4User_9D5C4326(&gxmGlobals.top);
	if (err != 0) {
		GXM_DBG_ERROR(365, "Failed due to error in driver layer (%08X).", err);
		return SCE_GXM_ERROR_DRIVER;
	}
	return 0;
}
