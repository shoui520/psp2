/*
 * SceGxm: creating and destroying immediate contexts.
 */

#include "gxm_internal.h"

/* a ring buffer is aligned and at least its minimum size */
static inline int gxmCheckRingBuffer(void *const *mem, const uint32_t *size, SceSize minimumSize)
{
	if (*mem == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if ((SceUInt32)*mem & 3)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (*size < minimumSize)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (*size & 3)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	return 0;
}

int sceGxmCreateContext(const SceGxmContextParams *params, SceGxmContext **context)
{
	SceGxmContext *created;
	SceUInt32 usseSegment;
	int ret, i;

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
	ret = gxmCheckRingBuffer(&params->vdmRingBufferMem, &params->vdmRingBufferMemSize,
				 GXM_MINIMUM_VDM_RING_BUFFER_SIZE);
	if (ret != 0)
		return ret;
	ret = gxmCheckRingBuffer(&params->vertexRingBufferMem, &params->vertexRingBufferMemSize,
				 GXM_MINIMUM_VERTEX_RING_BUFFER_SIZE);
	if (ret != 0)
		return ret;
	ret = gxmCheckRingBuffer(&params->fragmentRingBufferMem, &params->fragmentRingBufferMemSize,
				 GXM_MINIMUM_FRAGMENT_RING_BUFFER_SIZE);
	if (ret != 0)
		return ret;
	ret = gxmCheckRingBuffer(&params->fragmentUsseRingBufferMem,
				 &params->fragmentUsseRingBufferMemSize,
				 GXM_MINIMUM_FRAGMENT_USSE_RING_BUFFER_SIZE);
	if (ret != 0)
		return ret;
	/* the fragment USSE ring lies in a USSE segment */
	usseSegment = params->fragmentUsseRingBufferOffset >> 24;
	if (usseSegment != 9 && (usseSegment < 4 || usseSegment > 7))
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (gxmBuffersOverlap(params->vdmRingBufferMem, params->vdmRingBufferMemSize,
			      params->vertexRingBufferMem, params->vertexRingBufferMemSize))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (gxmBuffersOverlap(params->vdmRingBufferMem, params->vdmRingBufferMemSize,
			      params->fragmentRingBufferMem, params->fragmentRingBufferMemSize))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (gxmBuffersOverlap(params->vdmRingBufferMem, params->vdmRingBufferMemSize,
			      params->fragmentUsseRingBufferMem,
			      params->fragmentUsseRingBufferMemSize))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (gxmBuffersOverlap(params->vertexRingBufferMem, params->vertexRingBufferMemSize,
			      params->fragmentRingBufferMem, params->fragmentRingBufferMemSize))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (gxmBuffersOverlap(params->vertexRingBufferMem, params->vertexRingBufferMemSize,
			      params->fragmentUsseRingBufferMem,
			      params->fragmentUsseRingBufferMemSize))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (gxmBuffersOverlap(params->fragmentRingBufferMem, params->fragmentRingBufferMemSize,
			      params->fragmentUsseRingBufferMem,
			      params->fragmentUsseRingBufferMemSize))
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (context == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;

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
	if (created->slot == NULL)
		return SCE_GXM_ERROR_OUT_OF_MEMORY;
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
	created->nextScene.settings.mode = 1;
	created->nextScene.settings.modeValue = 0;
	created->nextScene.settings.yuvProfile[0] = 0;
	created->nextScene.settings.yuvProfile[1] = 1;
	created->nextScene.tuning.count = 0;
	created->nextScene.tuning.option = 0x80;
	created->nextScene.tuning.limit = 1000;
	created->nextScene.settings.offsets = 0xe9;
	gxmMteStateInit(&created->state);
	created->flags = GXM_CONTEXT_FLAG_CREATED;
	*context = created;
	return 0;
}

int sceGxmDestroyContext(SceGxmContext *context)
{
	if (!gxmGlobals.initialized)
		return SCE_GXM_ERROR_UNINITIALIZED;
	if (context == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
		return SCE_GXM_ERROR_INVALID_VALUE;
	sceGxmFinish(context);
	gxmSlotTableRemove(&gxmGlobals.top.contexts, (void *)context->slot);
	if (SceGpuEs4User_9D5C4326(&gxmGlobals.top) != 0)
		return SCE_GXM_ERROR_DRIVER;
	return 0;
}
