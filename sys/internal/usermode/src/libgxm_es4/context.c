/*
 * SceGxm: rendering contexts (the SDK's gxm/context.h).
 *
 * Programs, uniform buffers, vertex streams and textures bound to a
 * context, and the scenes and draws that send them to the GPU.
 */

#include "gxm_internal.h"

int sceGxmGetContextType(const SceGxmContext *context, SceUInt32 *type)
{
	if (context == NULL || type == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	*type = context->type;
	return 0;
}

void sceGxmSetVertexProgram(SceGxmContext *context, const SceGxmVertexProgram *vertexProgram)
{
	if (context->vertexProgram != vertexProgram) {
		context->vertexProgram = vertexProgram;
		if (vertexProgram != NULL) {
			gxmContextUpdateVertexOutputs(context, vertexProgram);
			context->vertexProgramWords[0] = (SceUInt32)vertexProgram->literals;
			context->vertexProgramWords[1] = (SceUInt32)vertexProgram->scratch;
			context->vertexProgramWords[2] = (SceUInt32)vertexProgram->thread;
			context->flags = (context->flags & ~(GXM_CONTEXT_FLAG_VERTEX_ALL |
							    GXM_CONTEXT_FLAG_VERTEX_STREAMS)) |
					 vertexProgram->pdsFlags;
		}
	}
	if (context->vertexDefaultUniformBufferMode != GXM_DEFAULT_UNIFORM_BUFFER_SET)
		context->vertexDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_NONE;
}

void sceGxmSetFragmentProgram(SceGxmContext *context, const SceGxmFragmentProgram *fragmentProgram)
{
	if (context->fragmentProgram != fragmentProgram) {
		context->fragmentProgram = fragmentProgram;
		if (fragmentProgram != NULL) {
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
	if (context->fragmentDefaultUniformBufferMode != GXM_DEFAULT_UNIFORM_BUFFER_SET)
		context->fragmentDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_NONE;
}

int sceGxmReserveVertexDefaultUniformBuffer(SceGxmContext *context, void **uniformBuffer)
{
	const SceGxmProgram *program;
	SceSize size;
	SceUInt32 mode;
	void *buffer;

	if (context == NULL || uniformBuffer == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!context->insideScene) {
		if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
			return SCE_GXM_ERROR_NOT_WITHIN_COMMAND_LIST;
		return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
	}
	if (context->vertexProgram == NULL)
		return SCE_GXM_ERROR_NULL_PROGRAM;
	program = context->vertexProgram->programHeader;
	size = program->defaultUniformBufferCount * 4;
	if (size == 0) {
		*uniformBuffer = NULL;
		context->vertexDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_NONE;
		return 0;
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
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	context->vertexUniformBuffers[14] = buffer;
	context->vertexDefaultUniformBuffer = buffer;
	*uniformBuffer = buffer;
	context->vertexDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_RESERVED;
	return 0;
}

int sceGxmReserveFragmentDefaultUniformBuffer(SceGxmContext *context, void **uniformBuffer)
{
	const SceGxmProgram *program;
	SceSize size;
	SceUInt32 mode;
	void *buffer;

	if (context == NULL || uniformBuffer == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!context->insideScene) {
		if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
			return SCE_GXM_ERROR_NOT_WITHIN_COMMAND_LIST;
		return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
	}
	if (context->fragmentProgram == NULL)
		return SCE_GXM_ERROR_NULL_PROGRAM;
	program = context->fragmentProgram->programHeader;
	size = program->defaultUniformBufferCount * 4;
	if (size == 0) {
		*uniformBuffer = NULL;
		context->fragmentDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_NONE;
		return 0;
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
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	context->fragmentUniformBuffers[14] = buffer;
	context->fragmentDefaultUniformBuffer = buffer;
	*uniformBuffer = buffer;
	context->fragmentDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_RESERVED;
	return 0;
}

int sceGxmSetVertexDefaultUniformBuffer(SceGxmContext *context, const void *uniformBuffer)
{
	if (context == NULL || uniformBuffer == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (context->vertexUniformBuffers[14] != uniformBuffer) {
		const SceGxmVertexProgram *program = context->vertexProgram;

		context->vertexUniformBuffers[14] = uniformBuffer;
		context->vertexDefaultUniformBuffer = uniformBuffer;
		if (program != NULL) {
			SceUInt32 mode = program->programHeader->bufferFlags >> 28;

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

int sceGxmSetFragmentDefaultUniformBuffer(SceGxmContext *context, const void *uniformBuffer)
{
	if (context == NULL || uniformBuffer == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (context->fragmentUniformBuffers[14] != uniformBuffer) {
		const SceGxmFragmentProgram *program = context->fragmentProgram;

		context->fragmentUniformBuffers[14] = uniformBuffer;
		context->fragmentDefaultUniformBuffer = uniformBuffer;
		if (program != NULL) {
			SceUInt32 mode = program->programHeader->bufferFlags >> 28;

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

	if (context == NULL || streamData == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (streamIndex > 15)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (context->vertexStreams[streamIndex] == streamData)
		return 0;
	context->vertexStreams[streamIndex] = streamData;
	program = context->vertexProgram;
	if (program != NULL && ((program->streamMask >> streamIndex) & 1))
		context->flags |= GXM_CONTEXT_FLAG_VERTEX_STREAMS;
	return 0;
}

/* control word 0 as bound to a context: bits 29 and 30 are left out */
#define GXM_TEX0_BOUND_MASK	0x9fffffff

int _sceGxmSetVertexTexture(SceGxmContext *context, SceUInt32 textureIndex,
		const SceGxmTexture *texture)
{
	SceGxmTexture *bound;
	SceUInt32 word0, word1, word2, word3;
	SceUInt32 bound0, bound1, bound2, bound3;

	if (context == NULL || texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (gxmTextureFormatIsYuv(sceGxmTextureGetFormat(texture)))
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (textureIndex > 15)
		return SCE_GXM_ERROR_INVALID_VALUE;
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
			SceUInt32 use = context->vertexProgram->programHeader->texUnitFlags[bit >> 5] >> (bit & 31);

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

	if (context == NULL || texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (textureIndex > 15)
		return SCE_GXM_ERROR_INVALID_VALUE;
	format = sceGxmTextureGetFormat(texture);
	if (gxmTextureFormatIsYuv(format) || gxmTextureFormatIsPalettized(format))
		return SCE_GXM_ERROR_UNSUPPORTED;
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
			SceUInt32 use = context->vertexProgram->programHeader->texUnitFlags[bit >> 5] >> (bit & 31);

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

	if (context == NULL || texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (textureIndex > 15)
		return SCE_GXM_ERROR_INVALID_VALUE;
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
			SceUInt32 use = context->fragmentProgram->programHeader->texUnitFlags[bit >> 5] >> (bit & 31);

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
	if (context == NULL || bufferData == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (bufferIndex > 13)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (context->vertexUniformBuffers[bufferIndex] != bufferData) {
		context->vertexUniformBuffers[bufferIndex] = bufferData;
		if (context->vertexProgram != NULL) {
			SceUInt32 mode = context->vertexProgram->programHeader->bufferFlags >>
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
	if (context == NULL || bufferData == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (bufferIndex > 13)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (context->fragmentUniformBuffers[bufferIndex] != bufferData) {
		context->fragmentUniformBuffers[bufferIndex] = bufferData;
		if (context->fragmentProgram != NULL) {
			SceUInt32 mode = context->fragmentProgram->programHeader->bufferFlags >>
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

/* a reserved default uniform buffer is used by the draw: keep it in the chunk buffer */
static inline void gxmContextCommitDefaultUniformBuffers(SceGxmContext *context,
		const SceGxmVertexProgram *vertexProgram,
		const SceGxmFragmentProgram *fragmentProgram)
{
	if (context->vertexDefaultUniformBufferMode == GXM_DEFAULT_UNIFORM_BUFFER_RESERVED) {
		SceUInt32 buffer = (SceUInt32)context->vertexDefaultUniformBuffer;
		SceUInt32 count = vertexProgram->programHeader->defaultUniformBufferCount;

		context->vertexDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_USED;
		context->vertexBuffer.cur = buffer + count * 4;
	}
	if (context->fragmentDefaultUniformBufferMode == GXM_DEFAULT_UNIFORM_BUFFER_RESERVED) {
		SceUInt32 buffer = (SceUInt32)context->fragmentDefaultUniformBuffer;
		SceUInt32 count = fragmentProgram->programHeader->defaultUniformBufferCount;

		context->fragmentDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_USED;
		context->fragmentBuffer.cur = buffer + count * 4;
	}
}

/* the same, at a kick: each program is read only when its buffer is reserved */
static inline void gxmContextCommitReservedUniformBuffers(SceGxmContext *context)
{
	if (context->vertexDefaultUniformBufferMode == GXM_DEFAULT_UNIFORM_BUFFER_RESERVED) {
		SceUInt32 buffer = (SceUInt32)context->vertexDefaultUniformBuffer;
		SceUInt32 count = context->vertexProgram->programHeader->defaultUniformBufferCount;

		context->vertexDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_USED;
		context->vertexBuffer.cur = buffer + count * 4;
	}
	if (context->fragmentDefaultUniformBufferMode == GXM_DEFAULT_UNIFORM_BUFFER_RESERVED) {
		SceUInt32 buffer = (SceUInt32)context->fragmentDefaultUniformBuffer;
		SceUInt32 count = context->fragmentProgram->programHeader->defaultUniformBufferCount;

		context->fragmentDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_USED;
		context->fragmentBuffer.cur = buffer + count * 4;
	}
}

int sceGxmDrawPrecomputed(SceGxmContext *context, const SceGxmPrecomputedDraw *precomputedDraw)
{
	const SceGxmVertexProgram *vertexProgram;
	const SceGxmFragmentProgram *fragmentProgram;
	GxmVdmPrimitive *primitive;
	int ret;

	if (context == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!context->insideScene) {
		if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
			return SCE_GXM_ERROR_NOT_WITHIN_COMMAND_LIST;
		return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
	}
	if (precomputedDraw == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	vertexProgram = context->vertexProgram;
	if (vertexProgram == NULL)
		return SCE_GXM_ERROR_NULL_PROGRAM;
	fragmentProgram = context->fragmentProgram;
	if (fragmentProgram == NULL)
		return SCE_GXM_ERROR_NULL_PROGRAM;
	gxmContextCommitDefaultUniformBuffers(context, vertexProgram, fragmentProgram);
	ret = gxmContextWriteState(context);
	if (ret != 0)
		return ret;
	primitive = gxmChunkBufferReserve(&context->vdmBuffer, sizeof(*primitive), 4);
	if (primitive == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	context->vdmBuffer.cur = (SceUInt32)(primitive + 1);
	*primitive = precomputedDraw->primitive;
	if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
		return 0;
	return gxmContextKickIfFull(context);
}

int sceGxmDraw(SceGxmContext *context, SceUInt32 primType, SceUInt32 indexType,
		const void *indexData, SceUInt32 indexCount)
{
	const SceGxmVertexProgram *vertexProgram;
	const SceGxmFragmentProgram *fragmentProgram;
	int ret;

	if (context == NULL || indexData == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!context->insideScene) {
		if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
			return SCE_GXM_ERROR_NOT_WITHIN_COMMAND_LIST;
		return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
	}
	vertexProgram = context->vertexProgram;
	if (vertexProgram == NULL)
		return SCE_GXM_ERROR_NULL_PROGRAM;
	fragmentProgram = context->fragmentProgram;
	if (fragmentProgram == NULL)
		return SCE_GXM_ERROR_NULL_PROGRAM;
	gxmContextCommitDefaultUniformBuffers(context, vertexProgram, fragmentProgram);
	ret = gxmContextWriteState(context);
	if (ret != 0)
		return ret;
	ret = gxmContextWriteVertexStreams(context);
	if (ret != 0)
		return ret;
	ret = gxmVdmWritePrimitive(&context->vdmBuffer, &context->vertexStreamBlock,
				   (const SceUInt8 *)&context->vertexProgram->dms, primType, indexType,
				   (SceUInt32)indexData, indexCount, 0, 0);
	if (ret != 0 || context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
		return ret;
	return gxmContextKickIfFull(context);
}

int sceGxmDrawInstanced(SceGxmContext *context, SceUInt32 primType,
		SceUInt32 indexType, const void *indexData, SceUInt32 indexCount,
		SceUInt32 indexWrap)
{
	const SceGxmVertexProgram *vertexProgram;
	const SceGxmFragmentProgram *fragmentProgram;
	int ret;

	if (context == NULL || indexData == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!context->insideScene) {
		if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
			return SCE_GXM_ERROR_NOT_WITHIN_COMMAND_LIST;
		return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
	}
	vertexProgram = context->vertexProgram;
	if (vertexProgram == NULL)
		return SCE_GXM_ERROR_NULL_PROGRAM;
	fragmentProgram = context->fragmentProgram;
	if (fragmentProgram == NULL)
		return SCE_GXM_ERROR_NULL_PROGRAM;
	gxmContextCommitDefaultUniformBuffers(context, vertexProgram, fragmentProgram);
	ret = gxmContextWriteState(context);
	if (ret != 0)
		return ret;
	ret = gxmContextWriteVertexStreams(context);
	if (ret != 0)
		return ret;
	ret = gxmVdmWritePrimitive(&context->vdmBuffer, &context->vertexStreamBlock,
				   (const SceUInt8 *)&context->vertexProgram->dms, primType, indexType,
				   (SceUInt32)indexData, indexCount, indexWrap, 0);
	if (ret != 0 || context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
		return ret;
	return gxmContextKickIfFull(context);
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
	gxmContextCommitReservedUniformBuffers(context);
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
	gxmContextCommitReservedUniformBuffers(context);
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
	gxmContextCommitReservedUniformBuffers(context);
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

static inline int gxmContextWriteUserMarker(SceGxmContext *context, SceUInt32 marker)
{
	SceUInt32 *block = gxmChunkBufferReserve(&context->vdmBuffer, 8, 4);

	if (block == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	block[1] = marker;
	block[0] = (SceUInt32)&block[2] >> 2;
	context->vdmBuffer.cur = (SceUInt32)&block[2];
	return 0;
}

static inline int gxmContextWriteUserMarkerTag(SceGxmContext *context, SceUInt32 marker,
					       const char *tag)
{
	SceUInt32 *block = gxmChunkBufferReserve(&context->vdmBuffer,
						 8 + GXM_USER_MARKER_TAG_SIZE, 4);
	char *start, *end, *dst;
	char c;
	SceUInt32 next;

	if (block == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	start = (char *)&block[2];
	end = start + GXM_USER_MARKER_TAG_SIZE - 1;
	dst = start;
	c = *tag++;
	if (c != '\0') {
		do {
			*dst++ = c;
			if (dst >= end)
				break;
			c = *tag++;
		} while (c != '\0');
	}
	*dst = '\0';
	block[1] = marker;
	next = (SceUInt32)block + ((dst + 1 - start + 3) & ~3) + 8;
	block[0] = next >> 2;
	context->vdmBuffer.cur = next;
	return 0;
}

int sceGxmPushUserMarker(SceGxmContext *context, const char *tag)
{
	if (context == NULL || tag == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!context->insideScene) {
		if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
			return SCE_GXM_ERROR_NOT_WITHIN_COMMAND_LIST;
		return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
	}
	if (context->type == SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
		if (gxmInitCallbacksMagic != GXM_CALLBACKS_MAGIC || !gxmInitCallbacks.isCapturing())
			return 0;
		return gxmContextWriteUserMarker(context,
				gxmInitCallbacks.userMarker(context, tag) ?
				GXM_USER_MARKER_PUSH_TAKEN : GXM_USER_MARKER_PUSH);
	}
	if (gxmInitCallbacksMagic != GXM_CALLBACKS_MAGIC)
		return 0;
	return gxmContextWriteUserMarkerTag(context, GXM_USER_MARKER_PUSH_TAG, tag);
}

int sceGxmPopUserMarker(SceGxmContext *context)
{
	if (context == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!context->insideScene) {
		if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
			return SCE_GXM_ERROR_NOT_WITHIN_COMMAND_LIST;
		return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
	}
	if (context->type == SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
		if (gxmInitCallbacksMagic != GXM_CALLBACKS_MAGIC || !gxmInitCallbacks.isCapturing())
			return 0;
		return gxmContextWriteUserMarker(context, GXM_USER_MARKER_POP);
	}
	if (gxmInitCallbacksMagic != GXM_CALLBACKS_MAGIC)
		return 0;
	return gxmContextWriteUserMarker(context, GXM_USER_MARKER_POP_DEFERRED);
}

int sceGxmSetUserMarker(SceGxmContext *context, const char *tag)
{
	if (context == NULL || tag == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!context->insideScene) {
		if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE)
			return SCE_GXM_ERROR_NOT_WITHIN_COMMAND_LIST;
		return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
	}
	if (context->type == SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
		if (gxmInitCallbacksMagic != GXM_CALLBACKS_MAGIC || !gxmInitCallbacks.isCapturing())
			return 0;
		return gxmContextWriteUserMarker(context,
				gxmInitCallbacks.userMarker(context, tag) ?
				GXM_USER_MARKER_SET_TAKEN : GXM_USER_MARKER_SET);
	}
	if (gxmInitCallbacksMagic != GXM_CALLBACKS_MAGIC)
		return 0;
	return gxmContextWriteUserMarkerTag(context, GXM_USER_MARKER_SET_TAG, tag);
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
		SceUInt32 count = context->vertexProgram->programHeader->defaultUniformBufferCount;

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
