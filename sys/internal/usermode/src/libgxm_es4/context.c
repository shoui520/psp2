/*
 * SceGxm: rendering contexts (the SDK's gxm/context.h).
 *
 * Programs, uniform buffers, vertex streams and textures bound to a
 * context, and the scenes and draws that send them to the GPU.
 */

#include "gxm_internal.h"

int sceGxmGetContextType(const SceGxmContext *context, SceUInt32 *type)
{
	if (context == NULL || type == NULL) {
		if (context == NULL)
			GXM_DBG_ERROR_NULL(62, sceGxmGetContextType, context);
		else
			GXM_DBG_ERROR_NULL(66, sceGxmGetContextType, type);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	*type = context->type;
	return 0;
}

void sceGxmSetVertexProgram(SceGxmContext *context, const SceGxmVertexProgram *vertexProgram)
{
	GXM_DBG_ASSERT(75, context);
	if (context->vertexProgram != vertexProgram) {
		context->vertexProgram = vertexProgram;
		if (vertexProgram != NULL) {
			GXM_DBG_ASSERT(85, vertexProgram->program);
			GXM_DBG_ASSERT(86, sceGxmProgramGetProgramType(vertexProgram->program) ==
					   SCE_GXM_VERTEX_PROGRAM);
			gxmContextUpdateVertexOutputs(context, vertexProgram);
			context->vertexProgramWords[0] = (SceUInt32)vertexProgram->literals;
			context->vertexProgramWords[1] = (SceUInt32)vertexProgram->scratch;
			context->vertexProgramWords[2] = (SceUInt32)vertexProgram->thread;
			context->flags = (context->flags & ~(GXM_CONTEXT_FLAG_VERTEX_ALL |
							    GXM_CONTEXT_FLAG_VERTEX_STREAMS)) |
					 vertexProgram->pdsFlags;
		}
	}
	if (context->vertexDefaultUniformBufferMode != GXM_DEFAULT_UNIFORM_BUFFER_SET) {
		if (context->vertexDefaultUniformBufferMode == GXM_DEFAULT_UNIFORM_BUFFER_RESERVED &&
		    gxmWarningIsEnabled(SCE_GXM_WARNING_VERTEX_DEFAULT_UNIFORM_BUFFER_RECYCLED))
			GXM_DBG_WARNING(110, "The previous vertex default uniform buffer reservation "
					"has not been used by any draw calls and has been recycled.");
		context->vertexDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_NONE;
	}
}

void sceGxmSetFragmentProgram(SceGxmContext *context, const SceGxmFragmentProgram *fragmentProgram)
{
	GXM_DBG_ASSERT(119, context);
	if (context->fragmentProgram != fragmentProgram) {
		context->fragmentProgram = fragmentProgram;
		if (fragmentProgram != NULL) {
			GXM_DBG_ASSERT(129, fragmentProgram->program);
			GXM_DBG_ASSERT(130, sceGxmProgramGetProgramType(fragmentProgram->program) ==
					    SCE_GXM_FRAGMENT_PROGRAM);
			gxmContextUpdateFrontFragmentProgram(context, fragmentProgram);
			if (context->state.frontIsp & GXM_ISP_TWO_SIDED)
				gxmContextUpdateBackFragmentProgram(context, fragmentProgram);
			context->fragmentProgramWords[0] = (SceUInt32)fragmentProgram->literals;
			context->fragmentProgramWords[1] = (SceUInt32)fragmentProgram->scratch;
			context->fragmentProgramWords[2] = (SceUInt32)fragmentProgram->thread;
			context->flags = (context->flags & ~GXM_CONTEXT_FLAG_FRAGMENT_ALL) |
					 fragmentProgram->pdsFlags;
		}
	}
	if (context->fragmentDefaultUniformBufferMode != GXM_DEFAULT_UNIFORM_BUFFER_SET) {
		if (context->fragmentDefaultUniformBufferMode == GXM_DEFAULT_UNIFORM_BUFFER_RESERVED &&
		    gxmWarningIsEnabled(SCE_GXM_WARNING_FRAGMENT_DEFAULT_UNIFORM_BUFFER_RECYCLED))
			GXM_DBG_WARNING(158, "The previous fragment default uniform buffer reservation "
					"has not been used by any draw calls and has been recycled.");
		context->fragmentDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_NONE;
	}
}

/*
 * The debug build reports, after a reservation in a deferred context, memory a chunk
 * callback returned that GXM cannot use.
 */
static inline int gxmContextCheckCallbacks(SceGxmContext *context)
{
#if SCE_DBG_ASSERTS_ENABLED
	int ret;

	if (context->type != SCE_GXM_CONTEXT_TYPE_DEFERRED)
		return 0;
	ret = gxmDeferredBufferCheckCallback(&context->vdmSource, "VDM");
	if (ret != 0)
		return ret;
	ret = gxmDeferredBufferCheckCallback(&context->vertexSource, "vertex");
	if (ret != 0)
		return ret;
	return gxmDeferredBufferCheckCallback(&context->fragmentSource, "fragment");
#else
	return 0;
#endif
}

int sceGxmReserveVertexDefaultUniformBuffer(SceGxmContext *context, void **uniformBuffer)
{
	const SceGxmProgram *program;
	SceSize size;
	SceUInt32 mode;
	void *buffer;

	if (context == NULL || uniformBuffer == NULL) {
		if (context == NULL)
			GXM_DBG_ERROR_NULL(171, sceGxmReserveVertexDefaultUniformBuffer, context);
		else
			GXM_DBG_ERROR_NULL(175, sceGxmReserveVertexDefaultUniformBuffer, uniformBuffer);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (!context->insideScene) {
		GXM_DBG_ERROR_OUTSIDE_SCENE(182, sceGxmReserveVertexDefaultUniformBuffer, context);
		if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
			return SCE_GXM_ERROR_NOT_WITHIN_COMMAND_LIST;
		return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
	}
	if (context->vertexProgram == NULL) {
		GXM_DBG_ERROR(188, "Function sceGxmReserveVertexDefaultUniformBuffer has been called when the current vertex program is NULL.");
		return SCE_GXM_ERROR_NULL_PROGRAM;
	}
	if (context->vertexDefaultUniformBufferMode == GXM_DEFAULT_UNIFORM_BUFFER_RESERVED &&
	    gxmWarningIsEnabled(SCE_GXM_WARNING_VERTEX_DEFAULT_UNIFORM_BUFFER_RECYCLED))
		GXM_DBG_WARNING(194, "The previous vertex default uniform buffer reservation has not been "
				"used by any draw calls and has been recycled.");
	program = context->vertexProgram->program;
	size = program->defaultUniformBufferCount * 4;
	if (size == 0) {
		*uniformBuffer = NULL;
		context->vertexDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_NONE;
		return gxmContextCheckCallbacks(context);
	}
	mode = program->bufferFlags >> 28;
	if (mode & 3) {
		if (mode & 2)
			context->flags |= GXM_CONTEXT_FLAG_VERTEX_CONSTANT_UPDATE;
		else
			context->flags |= GXM_CONTEXT_FLAG_VERTEX_DMA_UPDATE;
	}
	buffer = gxmChunkBufferReserve(&context->vertexBuffer, size, 4);
	if (buffer == NULL) {
		context->vertexDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_NONE;
		GXM_DBG_ERROR(232, "Function sceGxmReserveVertexDefaultUniformBuffer failed to reserve %d bytes.", size);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	context->vertexUniformBuffers[14] = buffer;
	context->vertexDefaultUniformBuffer = buffer;
	*uniformBuffer = buffer;
#if SCE_DBG_ASSERTS_ENABLED
	if ((SceUInt32)buffer + size < context->vertexBuffer.end) {
		context->vertexOverrunCheckAddress = (SceUInt32 *)((SceUInt32)buffer + size);
		context->vertexOverrunCheckValue = *context->vertexOverrunCheckAddress;
	} else {
		context->vertexOverrunCheckAddress = NULL;
	}
#endif
	context->vertexDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_RESERVED;
	return gxmContextCheckCallbacks(context);
}

int sceGxmReserveFragmentDefaultUniformBuffer(SceGxmContext *context, void **uniformBuffer)
{
	const SceGxmProgram *program;
	SceSize size;
	SceUInt32 mode;
	void *buffer;

	if (context == NULL || uniformBuffer == NULL) {
		if (context == NULL)
			GXM_DBG_ERROR_NULL(283, sceGxmReserveFragmentDefaultUniformBuffer, context);
		else
			GXM_DBG_ERROR_NULL(287, sceGxmReserveFragmentDefaultUniformBuffer, uniformBuffer);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (!context->insideScene) {
		GXM_DBG_ERROR_OUTSIDE_SCENE(294, sceGxmReserveFragmentDefaultUniformBuffer, context);
		if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
			return SCE_GXM_ERROR_NOT_WITHIN_COMMAND_LIST;
		return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
	}
	if (context->fragmentProgram == NULL) {
		GXM_DBG_ERROR(300, "Function sceGxmReserveFragmentDefaultUniformBuffer has been called when the current fragment program is NULL.");
		return SCE_GXM_ERROR_NULL_PROGRAM;
	}
	if (context->fragmentDefaultUniformBufferMode == GXM_DEFAULT_UNIFORM_BUFFER_RESERVED &&
	    gxmWarningIsEnabled(SCE_GXM_WARNING_FRAGMENT_DEFAULT_UNIFORM_BUFFER_RECYCLED))
		GXM_DBG_WARNING(306, "The previous fragment default uniform buffer reservation has not been "
				"used by any draw calls and has been recycled.");
	program = context->fragmentProgram->program;
	size = program->defaultUniformBufferCount * 4;
	if (size == 0) {
		*uniformBuffer = NULL;
		context->fragmentDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_NONE;
		return gxmContextCheckCallbacks(context);
	}
	mode = program->bufferFlags >> 28;
	if (mode & 3) {
		if (mode & 2)
			context->flags |= GXM_CONTEXT_FLAG_FRAGMENT_CONSTANT_UPDATE;
		else
			context->flags |= GXM_CONTEXT_FLAG_FRAGMENT_DMA_UPDATE;
	}
	buffer = gxmChunkBufferReserve(&context->fragmentBuffer, size, 4);
	if (buffer == NULL) {
		context->fragmentDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_NONE;
		GXM_DBG_ERROR(344, "Function sceGxmReserveFragmentDefaultUniformBuffer failed to reserve %d bytes.", size);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	context->fragmentUniformBuffers[14] = buffer;
	context->fragmentDefaultUniformBuffer = buffer;
	*uniformBuffer = buffer;
#if SCE_DBG_ASSERTS_ENABLED
	if ((SceUInt32)buffer + size < context->fragmentBuffer.end) {
		context->fragmentOverrunCheckAddress = (SceUInt32 *)((SceUInt32)buffer + size);
		context->fragmentOverrunCheckValue = *context->fragmentOverrunCheckAddress;
	} else {
		context->fragmentOverrunCheckAddress = NULL;
	}
#endif
	context->fragmentDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_RESERVED;
	return gxmContextCheckCallbacks(context);
}

int sceGxmSetVertexDefaultUniformBuffer(SceGxmContext *context, const void *bufferData)
{
	if (context == NULL || bufferData == NULL) {
		if (context == NULL)
			GXM_DBG_ERROR_NULL(466, sceGxmSetVertexDefaultUniformBuffer, context);
		else
			GXM_DBG_ERROR_NULL(470, sceGxmSetVertexDefaultUniformBuffer, bufferData);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (context->vertexDefaultUniformBufferMode == GXM_DEFAULT_UNIFORM_BUFFER_RESERVED &&
	    gxmWarningIsEnabled(SCE_GXM_WARNING_VERTEX_DEFAULT_UNIFORM_BUFFER_RECYCLED))
		GXM_DBG_WARNING(476, "The previous vertex default uniform buffer reservation has not been "
				"used by any draw calls and has been recycled.");
	if (context->vertexUniformBuffers[14] != bufferData) {
		const SceGxmVertexProgram *program = context->vertexProgram;

		context->vertexUniformBuffers[14] = bufferData;
		context->vertexDefaultUniformBuffer = bufferData;
		if (program != NULL) {
			SceUInt32 mode = program->program->bufferFlags >> 28;

			if (mode & 3) {
				if (mode & 2)
					context->flags |= GXM_CONTEXT_FLAG_VERTEX_CONSTANT_UPDATE;
				else
					context->flags |= GXM_CONTEXT_FLAG_VERTEX_DMA_UPDATE;
			}
		}
	}
	context->vertexDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_SET;
	return 0;
}

int sceGxmSetFragmentDefaultUniformBuffer(SceGxmContext *context, const void *bufferData)
{
	if (context == NULL || bufferData == NULL) {
		if (context == NULL)
			GXM_DBG_ERROR_NULL(523, sceGxmSetFragmentDefaultUniformBuffer, context);
		else
			GXM_DBG_ERROR_NULL(527, sceGxmSetFragmentDefaultUniformBuffer, bufferData);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (context->fragmentDefaultUniformBufferMode == GXM_DEFAULT_UNIFORM_BUFFER_RESERVED &&
	    gxmWarningIsEnabled(SCE_GXM_WARNING_FRAGMENT_DEFAULT_UNIFORM_BUFFER_RECYCLED))
		GXM_DBG_WARNING(533, "The previous fragment default uniform buffer reservation has not been "
				"used by any draw calls and has been recycled.");
	if (context->fragmentUniformBuffers[14] != bufferData) {
		const SceGxmFragmentProgram *program = context->fragmentProgram;

		context->fragmentUniformBuffers[14] = bufferData;
		context->fragmentDefaultUniformBuffer = bufferData;
		if (program != NULL) {
			SceUInt32 mode = program->program->bufferFlags >> 28;

			if (mode & 3) {
				if (mode & 2)
					context->flags |= GXM_CONTEXT_FLAG_FRAGMENT_CONSTANT_UPDATE;
				else
					context->flags |= GXM_CONTEXT_FLAG_FRAGMENT_DMA_UPDATE;
			}
		}
	}
	context->fragmentDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_SET;
	return 0;
}

int sceGxmSetVertexStream(SceGxmContext *context, SceUInt32 streamIndex, const void *streamData)
{
	const SceGxmVertexProgram *program;

	if (context == NULL || streamData == NULL) {
		if (context == NULL)
			GXM_DBG_ERROR_NULL(581, sceGxmSetVertexStream, context);
		else
			GXM_DBG_ERROR_NULL(585, sceGxmSetVertexStream, streamData);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (streamIndex > 15) {
		GXM_DBG_ERROR_RANGE(593, sceGxmSetVertexStream, streamIndex, 15);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (context->vertexStreams[streamIndex] == streamData)
		return 0;
	context->vertexStreams[streamIndex] = streamData;
	program = context->vertexProgram;
	if (program != NULL && ((program->streamMask >> streamIndex) & 1))
		context->flags |= GXM_CONTEXT_FLAG_VERTEX_STREAMS;
	return 0;
}

int _sceGxmSetVertexTexture(SceGxmContext *context, SceUInt32 textureIndex,
		const SceGxmTexture *texture)
{
	SceGxmTexture *bound;
	SceUInt32 word0, word1, word2, word3;
	SceUInt32 bound0, bound1, bound2, bound3;

	if (context == NULL || texture == NULL) {
		if (context == NULL)
			GXM_DBG_ERROR_NULL(622, sceGxmSetVertexTexture, context);
		else
			GXM_DBG_ERROR_NULL(626, sceGxmSetVertexTexture, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (gxmTextureFormatIsYuv(sceGxmTextureGetFormat(texture))) {
		GXM_DBG_ERROR(631, "Parameter texture to function sceGxmSetVertexTexture cannot refer "
			      "to a texture that is based on a YUV texture format.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (textureIndex > 15) {
		GXM_DBG_ERROR_RANGE(639, sceGxmSetVertexTexture, textureIndex, 15);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	bound = &context->vertexTextures[textureIndex];
	word0 = texture->controlWords[0] & GXM_TEX0_BOUND_MASK;
	word1 = texture->controlWords[1];
	word2 = texture->controlWords[2];
	word3 = texture->controlWords[3];
	bound0 = bound->controlWords[0];
	bound1 = bound->controlWords[1];
	bound2 = bound->controlWords[2];
	bound3 = bound->controlWords[3];
	if (bound0 != word0 || bound1 != word1 || bound2 != word2 || bound3 != word3) {
		bound->controlWords[0] = word0;
		bound->controlWords[1] = word1;
		bound->controlWords[2] = word2;
		bound->controlWords[3] = word3;
		if (context->vertexProgram != NULL) {
			SceUInt32 bit = textureIndex * 4;
			SceUInt32 use = context->vertexProgram->program->texUnitFlags[bit >> 5] >> (bit & 31);

			if (use & 0xf) {
				if (use & 4)
					context->flags |= GXM_CONTEXT_FLAG_VERTEX_ALL;
				else
					context->flags |= GXM_CONTEXT_FLAG_VERTEX_CONSTANT_UPDATE;
			}
		}
	}
	return 0;
}

int sceGxmSetVertexTexture(SceGxmContext *context, SceUInt32 textureIndex,
		const SceGxmTexture *texture)
{
	SceGxmTextureFormat format;
	SceGxmTexture *bound;
	SceUInt32 word0, word1, word2, word3;
	SceUInt32 bound0, bound1, bound2, bound3;

	if (context == NULL || texture == NULL) {
		if (context == NULL)
			GXM_DBG_ERROR_NULL(691, sceGxmSetVertexTexture, context);
		else
			GXM_DBG_ERROR_NULL(695, sceGxmSetVertexTexture, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (textureIndex > 15) {
		GXM_DBG_ERROR_RANGE(701, sceGxmSetVertexTexture, textureIndex, 15);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	format = sceGxmTextureGetFormat(texture);
	if (gxmTextureFormatIsYuv(format)) {
		GXM_DBG_ERROR(708, "Parameter texture to function sceGxmSetVertexTexture cannot refer "
			      "to a texture with YUV texture format.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (gxmTextureFormatIsPalettized(format)) {
		GXM_DBG_ERROR(712, "Parameter texture to function sceGxmSetVertexTexture cannot refer "
			      "to a texture with palettized texture format.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	bound = &context->vertexTextures[textureIndex];
	word0 = texture->controlWords[0] & GXM_TEX0_BOUND_MASK;
	word1 = texture->controlWords[1];
	word2 = texture->controlWords[2];
	word3 = texture->controlWords[3];
	bound0 = bound->controlWords[0];
	bound1 = bound->controlWords[1];
	bound2 = bound->controlWords[2];
	bound3 = bound->controlWords[3];
	if (bound0 != word0 || bound1 != word1 || bound2 != word2 || bound3 != word3) {
		bound->controlWords[0] = word0;
		bound->controlWords[1] = word1;
		bound->controlWords[2] = word2;
		bound->controlWords[3] = word3;
		if (context->vertexProgram != NULL) {
			SceUInt32 bit = textureIndex * 4;
			SceUInt32 use = context->vertexProgram->program->texUnitFlags[bit >> 5] >> (bit & 31);

			if (use & 0xf) {
				if (use & 4)
					context->flags |= GXM_CONTEXT_FLAG_VERTEX_ALL;
				else
					context->flags |= GXM_CONTEXT_FLAG_VERTEX_CONSTANT_UPDATE;
			}
		}
	}
	return 0;
}

int sceGxmSetFragmentTexture(SceGxmContext *context, SceUInt32 textureIndex,
		const SceGxmTexture *texture)
{
	SceGxmTexture *bound;
	SceUInt32 word0, word1, word2, word3;
	SceUInt32 bound0, bound1, bound2, bound3;

	if (context == NULL || texture == NULL) {
		if (context == NULL)
			GXM_DBG_ERROR_NULL(764, sceGxmSetFragmentTexture, context);
		else
			GXM_DBG_ERROR_NULL(768, sceGxmSetFragmentTexture, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (textureIndex > 15) {
		GXM_DBG_ERROR_RANGE(776, sceGxmSetFragmentTexture, textureIndex, 15);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	bound = &context->fragmentTextures[textureIndex];
	word0 = texture->controlWords[0] & GXM_TEX0_BOUND_MASK;
	word1 = texture->controlWords[1];
	word2 = texture->controlWords[2];
	word3 = texture->controlWords[3];
	bound0 = bound->controlWords[0];
	bound1 = bound->controlWords[1];
	bound2 = bound->controlWords[2];
	bound3 = bound->controlWords[3];
	if (bound0 != word0 || bound1 != word1 || bound2 != word2 || bound3 != word3) {
		bound->controlWords[0] = word0;
		bound->controlWords[1] = word1;
		bound->controlWords[2] = word2;
		bound->controlWords[3] = word3;
		if (context->fragmentProgram != NULL) {
			SceUInt32 bit = textureIndex * 4;
			SceUInt32 use = context->fragmentProgram->program->texUnitFlags[bit >> 5] >> (bit & 31);

			if (use & 0xf) {
				SceUInt32 flags;

				if (use & 4)
					flags = context->flags | GXM_CONTEXT_FLAG_FRAGMENT_TEXTURES |
						GXM_CONTEXT_FLAG_FRAGMENT_CONSTANT_UPDATE;
				else
					flags = context->flags | GXM_CONTEXT_FLAG_FRAGMENT_PROGRAM;
				if (use & 2)
					flags |= GXM_CONTEXT_FLAG_FRAGMENT_CONSTANTS |
						 GXM_CONTEXT_FLAG_FRAGMENT_DATA;
				if (use & 1)
					flags |= GXM_CONTEXT_FLAG_FRAGMENT_TEXTURE_STATE;
				context->flags = flags;
			}
		}
	}
	return 0;
}

int sceGxmSetVertexUniformBuffer(SceGxmContext *context, SceUInt32 bufferIndex,
		const void *bufferData)
{
	if (context == NULL || bufferData == NULL) {
		if (context == NULL)
			GXM_DBG_ERROR_NULL(832, sceGxmSetVertexUniformBuffer, context);
		else
			GXM_DBG_ERROR_NULL(836, sceGxmSetVertexUniformBuffer, bufferData);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (bufferIndex > 13) {
		GXM_DBG_ERROR_RANGE(844, sceGxmSetVertexUniformBuffer, bufferIndex, 13);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (context->vertexUniformBuffers[bufferIndex] != bufferData) {
		context->vertexUniformBuffers[bufferIndex] = bufferData;
		if (context->vertexProgram != NULL) {
			SceUInt32 mode = context->vertexProgram->program->bufferFlags >>
					 (bufferIndex * 2);

			if (mode & 3) {
				if (mode & 2)
					context->flags |= GXM_CONTEXT_FLAG_VERTEX_CONSTANT_UPDATE;
				else
					context->flags |= GXM_CONTEXT_FLAG_VERTEX_DMA_UPDATE;
			}
		}
	}
	return 0;
}

int sceGxmSetFragmentUniformBuffer(SceGxmContext *context, SceUInt32 bufferIndex,
		const void *bufferData)
{
	if (context == NULL || bufferData == NULL) {
		if (context == NULL)
			GXM_DBG_ERROR_NULL(886, sceGxmSetFragmentUniformBuffer, context);
		else
			GXM_DBG_ERROR_NULL(890, sceGxmSetFragmentUniformBuffer, bufferData);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (bufferIndex > 13) {
		GXM_DBG_ERROR_RANGE(898, sceGxmSetFragmentUniformBuffer, bufferIndex, 13);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (context->fragmentUniformBuffers[bufferIndex] != bufferData) {
		context->fragmentUniformBuffers[bufferIndex] = bufferData;
		if (context->fragmentProgram != NULL) {
			SceUInt32 mode = context->fragmentProgram->program->bufferFlags >>
					 (bufferIndex * 2);

			if (mode & 3) {
				if (mode & 2)
					context->flags |= GXM_CONTEXT_FLAG_FRAGMENT_CONSTANT_UPDATE;
				else
					context->flags |= GXM_CONTEXT_FLAG_FRAGMENT_DMA_UPDATE;
			}
		}
	}
	return 0;
}

void sceGxmSetPrecomputedFragmentState(SceGxmContext *context,
		const struct SceGxmPrecomputedFragmentState *precomputedState)
{
	if (context->precomputedFragmentState == precomputedState)
		return;
	if (precomputedState != NULL) {
		context->precomputedFragmentState = precomputedState;
	} else {
		context->precomputedFragmentState = NULL;
		if (context->appliedFragmentState != NULL)
			context->flags |= GXM_CONTEXT_FLAG_FRAGMENT_PROGRAM;
	}
}

void sceGxmSetPrecomputedVertexState(SceGxmContext *context,
		const struct SceGxmPrecomputedVertexState *precomputedState)
{
	if (context->precomputedVertexState == precomputedState)
		return;
	if (precomputedState != NULL) {
		context->precomputedVertexState = precomputedState;
	} else {
		context->precomputedVertexState = NULL;
		if (context->appliedVertexState != NULL)
			context->flags |= GXM_CONTEXT_FLAG_VERTEX_PROGRAM;
	}
}

/*
 * The debug build reports a reserved default uniform buffer the application wrote past: the
 * word after it no longer holds what it held at the reservation.
 */
static inline int gxmContextCheckDefaultUniformBufferOverrun(const SceGxmContext *context)
{
#if SCE_DBG_ASSERTS_ENABLED
	if (SCE_UNLIKELY(context->vertexDefaultUniformBufferMode == GXM_DEFAULT_UNIFORM_BUFFER_RESERVED &&
			 context->vertexOverrunCheckAddress != NULL &&
			 *context->vertexOverrunCheckAddress != context->vertexOverrunCheckValue)) {
		GXM_DBG_ERROR(399, "Vertex default uniform buffer overrun check failed");
		return SCE_GXM_ERROR_BUFFER_OVERRUN;
	}
	if (SCE_UNLIKELY(context->fragmentDefaultUniformBufferMode == GXM_DEFAULT_UNIFORM_BUFFER_RESERVED &&
			 context->fragmentOverrunCheckAddress != NULL &&
			 *context->fragmentOverrunCheckAddress != context->fragmentOverrunCheckValue)) {
		GXM_DBG_ERROR(407, "Fragment default uniform buffer overrun check failed");
		return SCE_GXM_ERROR_BUFFER_OVERRUN;
	}
#endif
	return 0;
}

/*
 * A reserved default uniform buffer is used by a draw or a kick: keep it in the chunk buffer.
 * Each program is read only when its buffer is reserved.
 */
static inline void gxmContextCommitDefaultUniformBuffers(SceGxmContext *context)
{
	if (context->vertexDefaultUniformBufferMode == GXM_DEFAULT_UNIFORM_BUFFER_RESERVED) {
		const void *buffer = context->vertexDefaultUniformBuffer;
		SceUInt32 count = context->vertexProgram->program->defaultUniformBufferCount;

		GXM_DBG_ASSERT(422, sceGxmBufferGet(SCE_GXM_CTX_TA_BUFFER(context)) == buffer);
		gxmChunkBufferCommit(&context->vertexBuffer, (void *)((SceUInt32)buffer + count * 4));
		context->vertexDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_USED;
	}
	if (context->fragmentDefaultUniformBufferMode == GXM_DEFAULT_UNIFORM_BUFFER_RESERVED) {
		const void *buffer = context->fragmentDefaultUniformBuffer;
		SceUInt32 count = context->fragmentProgram->program->defaultUniformBufferCount;

		GXM_DBG_ASSERT(441, sceGxmBufferGet(SCE_GXM_CTX_RENDER_BUFFER(context)) == buffer);
		gxmChunkBufferCommit(&context->fragmentBuffer,
				     (void *)((SceUInt32)buffer + count * 4));
		context->fragmentDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_USED;
	}
}

/*
 * The end of a draw: an immediate context kicks when a ring is full; the debug build checks a
 * deferred context's callbacks and reports the error a check found, which the draw returns.
 */
static inline int gxmContextEndDraw(SceGxmContext *context, int error, const char *function)
{
	int ret;

	if (context->type == SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
		ret = gxmContextKickIfFull(context);
		if (ret != 0)
			return ret;
	}
#if SCE_DBG_ASSERTS_ENABLED
	if (context->type == SCE_GXM_CONTEXT_TYPE_DEFERRED && error == 0)
		error = gxmContextCheckCallbacks(context);
	gxmDebugReportError(error, function);
	return error;
#else
	return 0;
#endif
}

int sceGxmDrawPrecomputed(SceGxmContext *context, const SceGxmPrecomputedDraw *precomputedDraw)
{
	GxmVdmPrimitive *primitive;
	int error = 0;
	int ret;

	if (context == NULL) {
		GXM_DBG_ERROR_NULL(985, sceGxmDrawPrecomputed, context);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (!context->insideScene) {
		GXM_DBG_ERROR_OUTSIDE_SCENE(992, sceGxmDrawPrecomputed, context);
		if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
			return SCE_GXM_ERROR_NOT_WITHIN_COMMAND_LIST;
		return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
	}
	if (precomputedDraw == NULL) {
		GXM_DBG_ERROR_NULL(998, sceGxmDrawPrecomputed, precomputedDraw);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (context->vertexProgram == NULL) {
		GXM_DBG_ERROR(1004, "Function sceGxmDrawPrecomputed has been called when the current vertex program is NULL.");
		return SCE_GXM_ERROR_NULL_PROGRAM;
	}
	if (context->fragmentProgram == NULL) {
		GXM_DBG_ERROR(1008, "Function sceGxmDrawPrecomputed has been called when the current fragment program is NULL.");
		return SCE_GXM_ERROR_NULL_PROGRAM;
	}
#if SCE_DBG_ASSERTS_ENABLED
	error = gxmDebugCheckDraw(context, precomputedDraw->primitive.words[0] & 0x3c000000,
				  precomputedDraw->primitive.words[0] & 0x3fffff,
				  precomputedDraw->primitive.words[3] & 0x3fffff);
	if (error == 0) {
		if (precomputedDraw->vertexProgram != context->vertexProgram) {
			error = SCE_GXM_ERROR_INVALID_PRECOMPUTED_DRAW;
			GXM_DBG_ERROR(1025, "Precomputed draw call program does not match current vertex program.");
		} else {
			error = gxmContextCheckDefaultUniformBufferOverrun(context);
		}
	}
#endif
	gxmContextCommitDefaultUniformBuffers(context);
	ret = gxmContextWriteState(context);
	if (ret != 0)
		return ret;
	primitive = gxmChunkBufferReserveChecked(&context->vdmBuffer, sizeof(*primitive), 4);
	if (primitive == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	gxmChunkBufferCommit(&context->vdmBuffer, primitive + 1);
	*primitive = precomputedDraw->primitive;
	return gxmContextEndDraw(context, error, "sceGxmDrawPrecomputed");
}

int sceGxmDraw(SceGxmContext *context, SceUInt32 primType, SceUInt32 indexType,
		const void *indexData, SceUInt32 indexCount)
{
	int error = 0;
	int ret;

	if (context == NULL || indexData == NULL) {
		if (context == NULL)
			GXM_DBG_ERROR_NULL(1086, sceGxmDraw, context);
		else
			GXM_DBG_ERROR_NULL(1090, sceGxmDraw, indexData);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (!context->insideScene) {
		GXM_DBG_ERROR_OUTSIDE_SCENE(1097, sceGxmDraw, context);
		if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
			return SCE_GXM_ERROR_NOT_WITHIN_COMMAND_LIST;
		return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
	}
	if (context->vertexProgram == NULL) {
		GXM_DBG_ERROR(1103, "Function sceGxmDraw has been called when the current vertex program is NULL.");
		return SCE_GXM_ERROR_NULL_PROGRAM;
	}
	if (context->fragmentProgram == NULL) {
		GXM_DBG_ERROR(1107, "Function sceGxmDraw has been called when the current fragment program is NULL.");
		return SCE_GXM_ERROR_NULL_PROGRAM;
	}
#if SCE_DBG_ASSERTS_ENABLED
	error = gxmDebugCheckDraw(context, primType, indexCount, 0);
	if (error == 0)
		error = gxmContextCheckDefaultUniformBufferOverrun(context);
#endif
	gxmContextCommitDefaultUniformBuffers(context);
	ret = gxmContextWriteState(context);
	if (ret != 0)
		return ret;
	ret = gxmContextWriteVertexStreams(context);
	if (ret != 0)
		return ret;
	ret = gxmVdmWritePrimitive(&context->vdmBuffer, &context->vertexStreamBlock,
				   (const SceUInt8 *)&context->vertexProgram->dms, primType, indexType,
				   (SceUInt32)indexData, indexCount, 0, 0);
	if (ret != 0)
		return ret;
	return gxmContextEndDraw(context, error, "sceGxmDraw");
}

int sceGxmDrawInstanced(SceGxmContext *context, SceUInt32 primType,
		SceUInt32 indexType, const void *indexData, SceUInt32 indexCount,
		SceUInt32 indexWrap)
{
	int error = 0;
	int ret;

	if (context == NULL || indexData == NULL) {
		if (context == NULL)
			GXM_DBG_ERROR_NULL(1182, sceGxmDrawInstanced, context);
		else
			GXM_DBG_ERROR_NULL(1186, sceGxmDrawInstanced, indexData);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (!context->insideScene) {
		GXM_DBG_ERROR_OUTSIDE_SCENE(1193, sceGxmDrawInstanced, context);
		if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
			return SCE_GXM_ERROR_NOT_WITHIN_COMMAND_LIST;
		return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
	}
	if (context->vertexProgram == NULL) {
		GXM_DBG_ERROR(1199, "Function sceGxmDrawInstanced has been called when the current vertex program is NULL.");
		return SCE_GXM_ERROR_NULL_PROGRAM;
	}
	if (context->fragmentProgram == NULL) {
		GXM_DBG_ERROR(1203, "Function sceGxmDrawInstanced has been called when the current fragment program is NULL.");
		return SCE_GXM_ERROR_NULL_PROGRAM;
	}
#if SCE_DBG_ASSERTS_ENABLED
	error = gxmDebugCheckDraw(context, primType, indexCount, indexWrap);
	if (error == 0)
		error = gxmContextCheckDefaultUniformBufferOverrun(context);
#endif
	gxmContextCommitDefaultUniformBuffers(context);
	ret = gxmContextWriteState(context);
	if (ret != 0)
		return ret;
	ret = gxmContextWriteVertexStreams(context);
	if (ret != 0)
		return ret;
	ret = gxmVdmWritePrimitive(&context->vdmBuffer, &context->vertexStreamBlock,
				   (const SceUInt8 *)&context->vertexProgram->dms, primType, indexType,
				   (SceUInt32)indexData, indexCount, indexWrap, 0);
	if (ret != 0)
		return ret;
	return gxmContextEndDraw(context, error, "sceGxmDrawInstanced");
}

/* depth/stencil surface contents: the load bits, then the store bits */

/* sceGxmBeginSceneEx; sceGxmBeginScene loads and stores one depth/stencil surface */
static inline int gxmBeginScene(SceGxmContext *context, SceUInt32 flags,
				const struct SceGxmRenderTarget *renderTarget,
				const SceGxmValidRegion *validRegion,
				struct SceGxmSyncObject *vertexSyncObject,
				struct SceGxmSyncObject *fragmentSyncObject,
				const SceGxmColorSurface *colorSurface,
				const SceGxmDepthStencilSurface *loadDepthStencil,
				const SceGxmDepthStencilSurface *storeDepthStencil)
{
	GxmRenderTargetInfo info;
	SceBool colorEnabled;
	_Bool loadEnabled;
	SceUInt32 stride, xMax, yMax, width, height;
	int ret;

	if (context == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE || (flags & ~GXM_SCENE_FLAGS) != 0)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (renderTarget == NULL || vertexSyncObject != NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (context->insideScene)
		return SCE_GXM_ERROR_WITHIN_SCENE;

	if (colorSurface != NULL) {
		context->colorSurface = *colorSurface;
		colorEnabled = sceGxmColorSurfaceIsEnabled(colorSurface);
		if (!(gxmGlobals.compatFlags & GXM_COMPAT_COLOR_ENABLE_DATA) && colorEnabled &&
		    sceGxmColorSurfaceGetData(colorSurface) == NULL)
			return SCE_GXM_ERROR_INVALID_VALUE;
	} else {
		sceGxmColorSurfaceInitDisabled(&context->colorSurface);
		colorEnabled = SCE_FALSE;
	}

	loadEnabled = loadDepthStencil != NULL && sceGxmDepthStencilSurfaceIsEnabled(loadDepthStencil);
	if (storeDepthStencil != NULL && sceGxmDepthStencilSurfaceIsEnabled(storeDepthStencil)) {
		SceUInt32 load, store;

		if (!loadEnabled)
			return SCE_GXM_ERROR_INVALID_VALUE;
		/* what the load surface holds, the store surface must hold too */
		load = loadDepthStencil->zlsControl;
		store = storeDepthStencil->zlsControl;
		if ((load & GXM_ZLS_LOAD_DEPTH) && !(store & GXM_ZLS_STORE_DEPTH))
			return SCE_GXM_ERROR_INVALID_VALUE;
		if (!(store & GXM_ZLS_STORE_STENCIL) && (load & GXM_ZLS_LOAD_STENCIL))
			return SCE_GXM_ERROR_INVALID_VALUE;
		if (!(store & GXM_ZLS_STORE_MASK) && (load & GXM_ZLS_LOAD_MASK))
			return SCE_GXM_ERROR_INVALID_VALUE;
		stride = sceGxmDepthStencilSurfaceGetStrideInSamples(loadDepthStencil);
		if (stride != sceGxmDepthStencilSurfaceGetStrideInSamples(storeDepthStencil))
			return SCE_GXM_ERROR_INVALID_VALUE;
		context->depthStencil.zlsControl = (storeDepthStencil->zlsControl & GXM_ZLS_STORE_CONTROL) |
						   (loadDepthStencil->zlsControl & GXM_ZLS_LOAD_CONTROL);
		context->depthStencil.loadDepthData = loadDepthStencil->depthData;
		context->depthStencil.loadStencilData = loadDepthStencil->stencilData;
		context->depthStencil.storeDepthData = storeDepthStencil->depthData;
		context->depthStencil.storeStencilData = storeDepthStencil->stencilData;
		context->depthStencil.backgroundDepth = loadDepthStencil->backgroundDepth;
		context->depthStencil.backgroundControl = loadDepthStencil->backgroundControl;
	} else {
		if (loadEnabled)
			return SCE_GXM_ERROR_INVALID_VALUE;
		context->depthStencil.zlsControl = 0;
		context->depthStencil.loadDepthData = NULL;
		context->depthStencil.loadStencilData = NULL;
		context->depthStencil.storeDepthData = NULL;
		context->depthStencil.storeStencilData = NULL;
		context->depthStencil.backgroundDepth = 1.0f;
		context->depthStencil.backgroundControl = 0x300;
		/* a scene draws to color, depth/stencil or both */
		if (!colorEnabled)
			return SCE_GXM_ERROR_INVALID_VALUE;
		stride = 0;
	}

	if (SceGpuEs4User_83D972DF(&gxmGlobals.top, renderTarget, &info) != 0)
		return SCE_GXM_ERROR_DRIVER;
	if (colorEnabled && info.samplesX == 1 && info.samplesY == 2 &&
	    sceGxmColorSurfaceGetScaleMode(colorSurface) != SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE)
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (validRegion != NULL) {
		xMax = validRegion->xMax;
		if (xMax >= info.width)
			return SCE_GXM_ERROR_INVALID_VALUE;
		yMax = validRegion->yMax;
		if (yMax >= info.height)
			return SCE_GXM_ERROR_INVALID_VALUE;
	} else {
		xMax = info.width - 1;
		yMax = info.height - 1;
	}

	if (colorEnabled) {
		SceUInt32 scaleMode;

		width = sceGxmTextureGetWidth(&colorSurface->backgroundTex);
		height = sceGxmTextureGetHeight(&colorSurface->backgroundTex);
		scaleMode = sceGxmColorSurfaceGetScaleMode(colorSurface);
		if ((info.samplesX > 1 || info.samplesY > 1) &&
		    scaleMode == SCE_GXM_COLOR_SURFACE_SCALE_NONE) {
			width /= info.samplesX;
			height /= info.samplesY;
		} else if (info.samplesX == 1 && info.samplesY == 1 &&
			   scaleMode == SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE) {
			width *= 2;
			height *= 2;
		}
		if (xMax + 1 > width || yMax + 1 > height)
			return SCE_GXM_ERROR_INVALID_VALUE;
	} else {
		width = 0;
		height = 0;
	}
	if (loadEnabled) {
		SceUInt32 rowSamples;

		if (gxmGlobals.compatFlags & GXM_COMPAT_DEPTH_STRIDE_PIXELS)
			rowSamples = xMax + 1;
		else
			rowSamples = (xMax + 32) & ~31;
		if (stride < info.samplesX * rowSamples)
			return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (info.wholeRegion && (xMax != info.width - 1 || yMax != info.height - 1))
		return SCE_GXM_ERROR_INVALID_VALUE;

	ret = gxmSceneBegin();
	if (ret != 0)
		return ret;
	context->renderTarget = renderTarget;
	context->sceneFlags = flags;
	context->colorHeight = height;
	context->flags |= GXM_CONTEXT_FLAG_DEPTH_STENCIL_USED;
	context->fragmentSyncObject = fragmentSyncObject;
	context->sampleCount = info.samplesX * info.samplesY;
	context->colorWidth = width;
	context->tileLimitX = xMax;
	context->tileLimitY = yMax;
	context->insideScene = 1;
	sceClibMemcpy_safe(&context->scene.tuning, &context->nextScene.tuning, sizeof(GxmSceneTuning));
	sceClibMemcpy_safe(&context->scene.settings, &context->nextScene.settings,
			   sizeof(GxmSceneSettings));
	sceGxmSetDefaultRegionClipAndViewport(context, xMax, yMax);
	if (gxmRazorMagic == GXM_CALLBACKS_MAGIC) {
		gxmRazorCallbacks.beginScene(context->vdmRing.size, context->vertexRing.size,
					     context->fragmentRing.size, context->fragmentUsseRing.size);
		if (*gxmRazorCallbacks.captureMarker != NULL)
			(*gxmRazorCallbacks.captureMarker)[8] = gxmGlobals.frameIndex | 0x10000000;
	}
	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC)
		gxmInitCallbacks.beginScene(gxmGlobals.frameIndex);
	return 0;
}

/* the older export: a region with its minimum, and no check of the flags */
int _sceGxmBeginScene(SceGxmContext *context, SceUInt32 flags,
		      const struct SceGxmRenderTarget *renderTarget,
		      const GxmValidRegionWithMin *validRegion,
		      struct SceGxmSyncObject *vertexSyncObject,
		      struct SceGxmSyncObject *fragmentSyncObject,
		      const SceGxmColorSurface *colorSurface,
		      const SceGxmDepthStencilSurface *depthStencil)
{
	SceGxmValidRegion region;
	const SceGxmValidRegion *max = NULL;

	if (validRegion != NULL) {
		region = validRegion->max;
		max = &region;
	}
	return gxmBeginScene(context, flags & GXM_SCENE_FLAGS, renderTarget, max, vertexSyncObject,
			     fragmentSyncObject, colorSurface, depthStencil, depthStencil);
}

int sceGxmBeginScene(SceGxmContext *context, SceUInt32 flags,
		     const struct SceGxmRenderTarget *renderTarget,
		     const SceGxmValidRegion *validRegion,
		     struct SceGxmSyncObject *vertexSyncObject,
		     struct SceGxmSyncObject *fragmentSyncObject,
		     const SceGxmColorSurface *colorSurface,
		     const SceGxmDepthStencilSurface *depthStencil)
{
	return gxmBeginScene(context, flags, renderTarget, validRegion, vertexSyncObject,
			     fragmentSyncObject, colorSurface, depthStencil, depthStencil);
}

int sceGxmBeginSceneEx(SceGxmContext *context, SceUInt32 flags,
		       const struct SceGxmRenderTarget *renderTarget,
		       const SceGxmValidRegion *validRegion,
		       struct SceGxmSyncObject *vertexSyncObject,
		       struct SceGxmSyncObject *fragmentSyncObject,
		       const SceGxmColorSurface *colorSurface,
		       const SceGxmDepthStencilSurface *loadDepthStencil,
		       const SceGxmDepthStencilSurface *storeDepthStencil)
{
	return gxmBeginScene(context, flags, renderTarget, validRegion, vertexSyncObject,
			     fragmentSyncObject, colorSurface, loadDepthStencil, storeDepthStencil);
}

/* the older export: no check of the flags */
int _sceGxmMidSceneFlush(SceGxmContext *context, SceUInt32 flags,
			 struct SceGxmSyncObject *vertexSyncObject,
			 const SceGxmNotification *vertexNotification)
{
	int ret;

	if (context == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (vertexSyncObject != NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!context->insideScene)
		return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
	gxmContextCommitDefaultUniformBuffers(context);
	ret = gxmContextKick(context,
			     (flags & SCE_GXM_MIDSCENE_PRESERVE_DEFAULT_UNIFORM_BUFFERS) |
			     GXM_KICK_MID_SCENE, vertexNotification, NULL);
	if (ret != 0)
		return ret;
	gxmRazorMarker(0x12000000, gxmGlobals.frameIndex);
	sceClibMemcpy_safe(&context->scene.tuning, &context->nextScene.tuning, sizeof(GxmSceneTuning));
	return 0;
}

int sceGxmMidSceneFlush(SceGxmContext *context, SceUInt32 flags,
			struct SceGxmSyncObject *vertexSyncObject,
			const SceGxmNotification *vertexNotification)
{
	int ret;

	if (context == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE ||
	    (flags & ~SCE_GXM_MIDSCENE_PRESERVE_DEFAULT_UNIFORM_BUFFERS) != 0)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (vertexSyncObject != NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!context->insideScene)
		return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
	gxmContextCommitDefaultUniformBuffers(context);
	ret = gxmContextKick(context, flags | GXM_KICK_MID_SCENE, vertexNotification, NULL);
	if (ret != 0)
		return ret;
	gxmRazorMarker(0x12000000, gxmGlobals.frameIndex);
	sceClibMemcpy_safe(&context->scene.tuning, &context->nextScene.tuning, sizeof(GxmSceneTuning));
	return 0;
}

int sceGxmEndScene(SceGxmContext *context, const SceGxmNotification *vertexNotification,
		   const SceGxmNotification *fragmentNotification)
{
	int ret;

	if (context == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (!context->insideScene)
		return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
	gxmContextCommitDefaultUniformBuffers(context);
	ret = gxmContextKick(context, GXM_KICK_FRAGMENT, vertexNotification, fragmentNotification);
	if (ret != 0)
		return ret;
	gxmRazorMarker(0x11000000 | (gxmGlobals.sceneIndex & 0xff) << 16,
		       gxmGlobals.frameIndex & 0xffff);
	context->insideScene = 0;
	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC)
		gxmInitCallbacks.endScene();
	gxmGlobals.sceneIndex++;
	return gxmSceneEnd();
}

/* user markers in the VDM stream: a two-word block, the tag copied in a deferred context */
#define GXM_USER_MARKER_PUSH		0xc0debf00
#define GXM_USER_MARKER_POP		0xc0debf01
#define GXM_USER_MARKER_SET		0xc0debf02
#define GXM_USER_MARKER_PUSH_TAKEN	0xc0debf03	/* the userMarker callback returned nonzero */
#define GXM_USER_MARKER_SET_TAKEN	0xc0debf04
#define GXM_USER_MARKER_PUSH_TAG	0xc0debf05	/* deferred: the tag follows */
#define GXM_USER_MARKER_POP_DEFERRED	0xc0debf06
#define GXM_USER_MARKER_SET_TAG		0xc0debf07

#define GXM_USER_MARKER_TAG_SIZE	64

/*
 * Write a user marker word into the VDM stream. A deferred context's tagged markers carry
 * the tag after the word; an immediate context's take takenMarker when the capture tool's
 * userMarker callback takes the tag (or there is none).
 */
static inline int gxmContextWriteUserMarker(SceGxmContext *context, const char *tag,
					    SceUInt32 marker, SceUInt32 takenMarker)
{
	SceBool tagged = (marker & ~2) == GXM_USER_MARKER_PUSH_TAG;
	SceUInt32 *block;
	SceSize size;
	char *start, *end, *dst;
	char c;

	if (tagged) {
		size = 8 + GXM_USER_MARKER_TAG_SIZE;
	} else {
		size = 8;
		if (tag == NULL || gxmInitCallbacks.userMarker(context, tag))
			marker = takenMarker;
	}
	block = gxmChunkBufferReserve(&context->vdmBuffer, size, 4);
	if (block == NULL) {
		GXM_DBG_ERROR(1863, "Could not reserve %d bytes.", size);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	if (tagged) {
		start = (char *)&block[2];
		end = start + GXM_USER_MARKER_TAG_SIZE - 1;
		dst = start;
		if (tag != NULL) {
			c = *tag++;
			if (c != '\0') {
				do {
					*dst++ = c;
					if (dst >= end)
						break;
					c = *tag++;
				} while (c != '\0');
			}
		}
		*dst = '\0';
		size = ((dst + 1 - start + 3) & ~3) + 8;
	}
	block[1] = marker;
	block[0] = ((SceUInt32)block + size) >> 2;
	gxmChunkBufferCommit(&context->vdmBuffer, (char *)block + size);
	return 0;
}

int sceGxmPushUserMarker(SceGxmContext *context, const char *tag)
{
	if (context == NULL || tag == NULL) {
		if (context == NULL)
			GXM_DBG_ERROR_NULL(1908, sceGxmPushUserMarker, context);
		else
			GXM_DBG_ERROR_NULL(1912, sceGxmPushUserMarker, tag);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (!context->insideScene) {
		GXM_DBG_ERROR_OUTSIDE_SCENE(1919, sceGxmPushUserMarker, context);
		if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
			return SCE_GXM_ERROR_NOT_WITHIN_COMMAND_LIST;
		return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
	}
	if (context->type == SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
		if (gxmInitCallbacksMagic != GXM_CALLBACKS_MAGIC || !gxmInitCallbacks.isCapturing())
			return 0;
		return gxmContextWriteUserMarker(context, tag, GXM_USER_MARKER_PUSH,
						 GXM_USER_MARKER_PUSH_TAKEN);
	}
	if (gxmInitCallbacksMagic != GXM_CALLBACKS_MAGIC)
		return 0;
	return gxmContextWriteUserMarker(context, tag, GXM_USER_MARKER_PUSH_TAG,
					 GXM_USER_MARKER_PUSH_TAG);
}

int sceGxmPopUserMarker(SceGxmContext *context)
{
	if (context == NULL) {
		GXM_DBG_ERROR_NULL(1940, sceGxmPopUserMarker, context);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (!context->insideScene) {
		GXM_DBG_ERROR_OUTSIDE_SCENE(1947, sceGxmPopUserMarker, context);
		if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
			return SCE_GXM_ERROR_NOT_WITHIN_COMMAND_LIST;
		return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
	}
	if (context->type == SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
		if (gxmInitCallbacksMagic != GXM_CALLBACKS_MAGIC || !gxmInitCallbacks.isCapturing())
			return 0;
		return gxmContextWriteUserMarker(context, NULL, GXM_USER_MARKER_POP,
						 GXM_USER_MARKER_POP);
	}
	if (gxmInitCallbacksMagic != GXM_CALLBACKS_MAGIC)
		return 0;
	return gxmContextWriteUserMarker(context, NULL, GXM_USER_MARKER_POP_DEFERRED,
					 GXM_USER_MARKER_POP_DEFERRED);
}

int sceGxmSetUserMarker(SceGxmContext *context, const char *tag)
{
	if (context == NULL || tag == NULL) {
		if (context == NULL)
			GXM_DBG_ERROR_NULL(1966, sceGxmSetUserMarker, context);
		else
			GXM_DBG_ERROR_NULL(1970, sceGxmSetUserMarker, tag);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (!context->insideScene) {
		GXM_DBG_ERROR_OUTSIDE_SCENE(1977, sceGxmSetUserMarker, context);
		if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
			return SCE_GXM_ERROR_NOT_WITHIN_COMMAND_LIST;
		return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
	}
	if (context->type == SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
		if (gxmInitCallbacksMagic != GXM_CALLBACKS_MAGIC || !gxmInitCallbacks.isCapturing())
			return 0;
		return gxmContextWriteUserMarker(context, tag, GXM_USER_MARKER_SET,
						 GXM_USER_MARKER_SET_TAKEN);
	}
	if (gxmInitCallbacksMagic != GXM_CALLBACKS_MAGIC)
		return 0;
	return gxmContextWriteUserMarker(context, tag, GXM_USER_MARKER_SET_TAG,
					 GXM_USER_MARKER_SET_TAG);
}

int sceGxmVertexFence(SceGxmContext *context)
{
	GxmCommandBlock block;
	int ret;

	if (context == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!context->insideScene) {
		if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
			return SCE_GXM_ERROR_NOT_WITHIN_COMMAND_LIST;
		return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
	}
	if (context->vertexDefaultUniformBufferMode == GXM_DEFAULT_UNIFORM_BUFFER_RESERVED) {
		SceUInt32 buffer = (SceUInt32)context->vertexDefaultUniformBuffer;
		SceUInt32 count = context->vertexProgram->program->defaultUniformBufferCount;

		context->vertexDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_USED;
		context->vertexBuffer.cur = buffer + count * 4;
	}
	ret = gxmWriteProgramBlock(&context->vertexBuffer, &gxmGlobals.top.fenceProgram, &block);
	if (ret != 0)
		return ret;
	ret = gxmVdmWriteTerminate(&context->vdmBuffer, &block);
	if (ret != 0)
		return ret;
	context->flags |= GXM_CONTEXT_FLAG_VERTEX_PROGRAM;
	return 0;
}
