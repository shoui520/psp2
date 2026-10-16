/*
 * SceGxm: hooks for the Razor GPU capture and replay tools.
 */

#include "gxm_internal.h"

SceUInt32 gxmRazorMagic;
GxmRazorCallbacks gxmRazorCallbacks;
SceUInt32 gxmInitCallbacksMagic;
GxmInitCallbacks gxmInitCallbacks;

int sceGxmInitializedInternal(void)
{
	if (!gxmGlobals.initialized)
		return SCE_GXM_ERROR_UNINITIALIZED;
	return 0;
}

void *sceGxmGetTopContextInternal(void)
{
	if (!gxmGlobals.initialized)
		return NULL;
	return &gxmGlobals.top;
}

void sceGxmSetInitializeParamInternal(SceUInt32 magic, const GxmInitCallbacks *callbacks)
{
	gxmInitCallbacksMagic = 0;
	if (magic != GXM_CALLBACKS_MAGIC || callbacks == NULL)
		return;
	sceClibMemcpy(&gxmInitCallbacks, callbacks, sizeof(gxmInitCallbacks));
	gxmInitCallbacksMagic = GXM_CALLBACKS_MAGIC;
}

void sceGxmSetCallbackInternal(SceUInt32 magic, const GxmRazorCallbacks *callbacks)
{
	gxmRazorMagic = 0;
	if (magic != GXM_CALLBACKS_MAGIC || callbacks == NULL)
		return;
	gxmRazorMagic = GXM_CALLBACKS_MAGIC;
	gxmRazorCallbacks = *callbacks;
}

/* replay: carve render target memory out of the context's chunk buffer */
int sceGxmGetReplayRenderTargetMemSize(SceGxmContext *context, SceSize size, void **memory)
{
	void *block;

	*memory = NULL;
	block = gxmChunkBufferReserve(&context->vdmBuffer, size, 4);
	if (block == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	context->vdmBuffer.cur = (SceUInt32)block + size;
	*memory = block;
	return 0;
}

void SceGxmInternalForTest_9F4A526F(void)
{
}
