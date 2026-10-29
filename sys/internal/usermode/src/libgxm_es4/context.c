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
			context->flags = (context->flags & ~0x7c0) | vertexProgram->pdsFlags;
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
			context->flags = (context->flags & ~0x3e) | fragmentProgram->pdsFlags;
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
			context->flags |= 0x380;
		else
			context->flags |= 0x300;
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
			context->flags |= 0x2c;
		else
			context->flags |= 0x28;
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
					context->flags |= 0x380;
				else
					context->flags |= 0x300;
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
					context->flags |= 0x2c;
				else
					context->flags |= 0x28;
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
		context->flags |= 0x400;
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
					context->flags |= 0x3c0;
				else
					context->flags |= 0x380;
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
					context->flags |= 0x3c0;
				else
					context->flags |= 0x380;
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
					flags = context->flags | 0x2e;
				else
					flags = context->flags | 0x20;
				if (use & 2)
					flags |= 0xc;
				if (use & 1)
					flags |= 0x10;
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
					context->flags |= 0x380;
				else
					context->flags |= 0x300;
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
					context->flags |= 0x2c;
				else
					context->flags |= 0x28;
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
			context->flags |= 0x20;
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
			context->flags |= 0x200;
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
