/*
 * SceGxm: writing a context's state into its buffers before a draw or a kick.
 */

#include "gxm_internal.h"

/* the scene keeps the depth and stencil data a command list's draws use */
int gxmContextCheckCommandList(const SceGxmContext *context, const SceGxmCommandList *list)
{
	SceUInt32 zls = context->depthStencil.zlsControl;

	if (!(zls & GXM_ZLS_STORE_DEPTH) &&
	    (list->depthStencilUsage & (GXM_LIST_USES_DEPTH_FRONT | GXM_LIST_USES_DEPTH_BACK)))
		return SCE_GXM_ERROR_INVALID_DEPTH_STENCIL_CONFIGURATION;
	if (!(zls & GXM_ZLS_STORE_STENCIL) &&
	    (list->depthStencilUsage & (GXM_LIST_USES_STENCIL_FRONT | GXM_LIST_USES_STENCIL_BACK)))
		return SCE_GXM_ERROR_INVALID_DEPTH_STENCIL_CONFIGURATION;
	if (zls & GXM_ZLS_STORE_MASK)
		return 0;
	if (list->depthStencilUsage & GXM_LIST_USES_MASK)
		return SCE_GXM_ERROR_INVALID_DEPTH_STENCIL_CONFIGURATION;
	return 0;
}

/* a used default uniform buffer is copied, so the next job may change it */
static inline const void *gxmContextCopyUniformBuffer(GxmChunkBuffer *buffer, const void *source,
						     const SceGxmProgram *program)
{
	SceSize size = program->defaultUniformBufferCount * 4;
	void *copy = gxmChunkBufferReserve(buffer, size, 4);

	if (copy != NULL)
		buffer->cur = (SceUInt32)copy + size;
	sceClibMemcpy_safe(copy, source, size);
	return copy;
}

/* everything is written again at the next draw */
void gxmContextResetState(SceGxmContext *context, SceBool vertexOnly,
			  SceBool preserveDefaultUniformBuffers)
{
	const SceGxmVertexProgram *vertexProgram;
	const SceGxmFragmentProgram *fragmentProgram;

	gxmMteStateInvalidate(&context->state);
	vertexProgram = context->vertexProgram;
	if (vertexProgram != NULL)
		context->flags |= vertexProgram->pdsFlags;
	context->appliedVertexState = NULL;
	if (preserveDefaultUniformBuffers) {
		if (context->vertexDefaultUniformBufferMode == GXM_DEFAULT_UNIFORM_BUFFER_USED) {
			const void *copy = gxmContextCopyUniformBuffer(&context->vertexBuffer,
					context->vertexDefaultUniformBuffer,
					vertexProgram->programHeader);

			context->vertexUniformBuffers[14] = copy;
			context->vertexDefaultUniformBuffer = copy;
		}
	} else if (context->vertexDefaultUniformBufferMode != GXM_DEFAULT_UNIFORM_BUFFER_SET) {
		context->vertexDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_NONE;
	}
	if (vertexOnly)
		return;

	fragmentProgram = context->fragmentProgram;
	if (fragmentProgram != NULL)
		context->flags |= fragmentProgram->pdsFlags;
	context->state.pdsState[0] = 0;
	context->state.pdsState[1] = 0;
	context->state.pdsState[2] = 0;
	context->appliedFragmentState = NULL;
	if (preserveDefaultUniformBuffers) {
		if (context->fragmentDefaultUniformBufferMode == GXM_DEFAULT_UNIFORM_BUFFER_USED) {
			const void *copy = gxmContextCopyUniformBuffer(&context->fragmentBuffer,
					context->fragmentDefaultUniformBuffer,
					fragmentProgram->programHeader);

			context->fragmentUniformBuffers[14] = copy;
			context->fragmentDefaultUniformBuffer = copy;
		}
	} else if (context->fragmentDefaultUniformBufferMode != GXM_DEFAULT_UNIFORM_BUFFER_SET) {
		context->fragmentDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_NONE;
	}
}
