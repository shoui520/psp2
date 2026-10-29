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

/* the vertex program's PDS block, copied with the current streams' addresses patched in */
int gxmContextWriteVertexStreams(SceGxmContext *context)
{
	const SceGxmVertexProgram *program;
	int ret;

	if (!(context->flags & GXM_CONTEXT_FLAG_VERTEX_STREAMS))
		return 0;
	program = context->vertexProgram;
	ret = gxmCommandBlockCopy(&context->vertexBuffer, &program->pds, &context->vertexStreamBlock);
	if (ret != 0)
		return ret;
	gxmPdsPatchAttributes(context->vertexStreamBlock.data, program->patches, program->patchCount,
			      (const SceUInt32 *)context->vertexStreams);
	context->flags &= ~GXM_CONTEXT_FLAG_VERTEX_STREAMS;
	return 0;
}

/* a depth function other than NEVER and ALWAYS reads the depth buffer */
static inline SceBool gxmIspReadsDepth(SceUInt32 isp)
{
	SceUInt32 func = isp & GXM_ISP_DEPTH_FUNC;

	return func != GXM_ISP_DEPTH_FUNC_ALWAYS && func != 0;
}

static inline SceBool gxmStencilReadsStencil(SceUInt32 stencil)
{
	SceUInt32 func = stencil & GXM_STENCIL_FUNC;

	return func != GXM_STENCIL_FUNC_ALWAYS && func != 0;
}

/* a command list records what it uses; a scene must store what its draws use */
static inline int gxmContextCheckDepthStencil(SceGxmContext *context)
{
	SceUInt32 isp = context->state.frontIsp;

	if (context->type != SCE_GXM_CONTEXT_TYPE_IMMEDIATE) {
		SceUInt32 usage = context->listDepthStencilUsage;

		if (gxmIspReadsDepth(isp))
			usage |= GXM_LIST_USES_DEPTH_FRONT;
		if (gxmStencilReadsStencil(context->state.frontStencil))
			usage |= GXM_LIST_USES_STENCIL_FRONT;
		if ((isp & GXM_ISP_PASS_TYPE) == GXM_ISP_PASS_TYPE_MASK_UPDATE)
			usage |= GXM_LIST_USES_MASK;
		if (isp & GXM_ISP_TWO_SIDED) {
			if (gxmIspReadsDepth(context->state.backIsp))
				usage |= GXM_LIST_USES_DEPTH_BACK;
			if (gxmStencilReadsStencil(context->state.backStencil))
				usage |= GXM_LIST_USES_STENCIL_BACK;
		}
		context->listDepthStencilUsage = usage;
	} else {
		SceUInt32 zls = context->depthStencil.zlsControl;

		if (!(zls & GXM_ZLS_STORE_DEPTH)) {
			if (gxmIspReadsDepth(isp))
				return SCE_GXM_ERROR_INVALID_DEPTH_STENCIL_CONFIGURATION;
			if ((isp & GXM_ISP_TWO_SIDED) && gxmIspReadsDepth(context->state.backIsp))
				return SCE_GXM_ERROR_INVALID_DEPTH_STENCIL_CONFIGURATION;
		}
		if (!(zls & GXM_ZLS_STORE_STENCIL)) {
			if (gxmStencilReadsStencil(context->state.frontStencil))
				return SCE_GXM_ERROR_INVALID_DEPTH_STENCIL_CONFIGURATION;
			if ((isp & GXM_ISP_TWO_SIDED) &&
			    gxmStencilReadsStencil(context->state.backStencil))
				return SCE_GXM_ERROR_INVALID_DEPTH_STENCIL_CONFIGURATION;
		}
		if (!(zls & GXM_ZLS_STORE_MASK) &&
		    (isp & GXM_ISP_PASS_TYPE) == GXM_ISP_PASS_TYPE_MASK_UPDATE)
			return SCE_GXM_ERROR_INVALID_DEPTH_STENCIL_CONFIGURATION;
	}
	return 0;
}

/* the context's texture words a program reads, into one of its blocks */
static inline void gxmProgramWriteTextureWords(SceUInt32 *block, const GxmProgramTextureRef *refs,
					       SceUInt32 count, const SceUInt32 *textures)
{
	SceUInt32 i;

	for (i = 0; i < count; i++)
		block[refs[i].dataWord] = textures[refs[i].textureWord];
}

static inline int gxmContextWriteTextureBlock(GxmChunkBuffer *buffer, void **textureBlock,
					      const SceGxmProgram *program,
					      const SceGxmTexture *textures)
{
	SceSize size = program->textureBufferCount * 4;
	SceUInt32 *block = gxmChunkBufferReserve(buffer, size, 4);

	if (block == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	*textureBlock = block;
	buffer->cur = (SceUInt32)block + size;
	gxmProgramWriteTextureWords(block, GXM_RELATIVE(program->textureRefsOffset),
				    program->textureRefCount, (const SceUInt32 *)textures);
	return 0;
}

/* uniform buffer addresses, literals and texture words */
static inline int gxmContextWriteConstantBlock(GxmChunkBuffer *buffer, void **constantBlock,
					       const SceGxmProgram *program,
					       const void *const *uniformBuffers,
					       const SceGxmTexture *textures)
{
	SceSize size = program->dataBufferCount * 4;
	SceUInt32 *block = gxmChunkBufferReserve(buffer, size, 4);
	const GxmProgramBufferRef *ref, *refsEnd;
	const GxmProgramLiteral *literal, *literalsEnd;

	if (block == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	*constantBlock = block;
	buffer->cur = (SceUInt32)block + size;
	ref = GXM_RELATIVE(program->bufferRefsOffset);
	refsEnd = ref + program->bufferRefCount;
	for (; ref != refsEnd; ref++)
		block[ref->dataWord] = (SceUInt32)uniformBuffers[ref->bufferIndex] + ref->offset;
	literal = GXM_RELATIVE(program->literalsOffset);
	literalsEnd = literal + program->literalCount;
	for (; literal != literalsEnd; literal++)
		block[literal->dataWord] = literal->value;
	gxmProgramWriteTextureWords(block, GXM_RELATIVE(program->constantTexturesOffset),
				    program->constantTextureCount, (const SceUInt32 *)textures);
	return 0;
}

/*
 * The secondary PDS program: one DMA per uniform buffer range the program
 * reads, then the secondary program's address and task control.
 */
static inline int gxmContextWriteDmaBlock(GxmChunkBuffer *buffer, GxmCommandBlock *dmaBlock,
					  const SceGxmProgram *program,
					  const void *const *uniformBuffers, SceSize size,
					  const GxmCodeRef *secondaryCode)
{
	SceUInt32 *data = gxmChunkBufferReserve(buffer, size, 16);

	if (data == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	buffer->cur = (SceUInt32)gxmProgramWriteDmaBlock(data, dmaBlock, program, uniformBuffers,
							 secondaryCode);
	return 0;
}

/* the fragment program's PDS block, with the textures it samples */
static inline int gxmContextWriteTextureStateBlock(SceGxmContext *context,
						   const SceGxmFragmentProgram *fragmentProgram)
{
	SceSize size = fragmentProgram->pds.size;
	SceGxmTexture *block = gxmChunkBufferReserve(&context->fragmentBuffer, size, 16);
	SceUInt32 i;

	if (block == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	context->fragmentBuffer.cur = (SceUInt32)block + size;
	context->fragmentTextureStateBlock.data = block;
	context->fragmentTextureStateBlock.size = size;
	context->fragmentTextureStateBlock.dataSize = fragmentProgram->pds.dataSize;
	for (i = 0; i < fragmentProgram->textureIndexCount; i++)
		block[i] = context->fragmentTextures[fragmentProgram->textureIndices[i]];
	i = fragmentProgram->textureIndexCount;
	gxmCopyWords((SceUInt32 *)&block[i], (const SceUInt32 *)fragmentProgram->pds.data + i * 4,
		     size - i * 16);
	return 0;
}

static inline int gxmContextWriteFragmentState(SceGxmContext *context)
{
	const SceGxmFragmentProgram *fragmentProgram = context->fragmentProgram;
	const SceGxmProgram *program = fragmentProgram->programHeader;
	SceUInt32 flags = context->flags;
	int ret;

	if (flags & GXM_CONTEXT_FLAG_FRAGMENT_PROGRAM) {
		if (flags & GXM_CONTEXT_FLAG_FRAGMENT_DATA) {
			if (context->fragmentDefaultUniformBufferMode == GXM_DEFAULT_UNIFORM_BUFFER_NONE &&
			    program->defaultUniformBufferCount != 0)
				return SCE_GXM_ERROR_UNIFORM_BUFFER_NOT_RESERVED;
			if (flags & GXM_CONTEXT_FLAG_FRAGMENT_TEXTURES) {
				ret = gxmContextWriteTextureBlock(&context->fragmentBuffer,
								  &context->fragmentTextureBlock,
								  program, context->fragmentTextures);
				if (ret != 0)
					return ret;
				flags = context->flags;
			}
			if (flags & GXM_CONTEXT_FLAG_FRAGMENT_CONSTANTS) {
				ret = gxmContextWriteConstantBlock(&context->fragmentBuffer,
								   &context->fragmentConstantBlock,
								   program, context->fragmentUniformBuffers,
								   context->fragmentTextures);
				if (ret != 0)
					return ret;
			}
			ret = gxmContextWriteDmaBlock(&context->fragmentBuffer,
						      &context->fragmentDmaBlock, program,
						      context->fragmentUniformBuffers,
						      fragmentProgram->secondaryPdsSize,
						      &fragmentProgram->secondaryCode);
			if (ret != 0)
				return ret;
			flags = context->flags;
		}
		if (flags & GXM_CONTEXT_FLAG_FRAGMENT_TEXTURE_STATE) {
			ret = gxmContextWriteTextureStateBlock(context, fragmentProgram);
			if (ret != 0)
				return ret;
		}
		context->state.dirty |= GXM_DIRTY_PDS_STATE;
		gxmPdsWriteStateWords(context->state.pdsState, &context->fragmentDmaBlock,
				      fragmentProgram->dms, &context->fragmentTextureStateBlock);
		flags = context->flags;
		context->appliedFragmentState = NULL;
	}
	context->flags = flags & ~GXM_CONTEXT_FLAG_FRAGMENT_ALL;
	return 0;
}

static inline int gxmContextWriteVertexState(SceGxmContext *context)
{
	const SceGxmVertexProgram *vertexProgram = context->vertexProgram;
	const SceGxmProgram *program = vertexProgram->programHeader;
	SceUInt32 flags = context->flags;
	int ret;

	if (flags & GXM_CONTEXT_FLAG_VERTEX_PROGRAM) {
		if (flags & GXM_CONTEXT_FLAG_VERTEX_DATA) {
			if (context->vertexDefaultUniformBufferMode == GXM_DEFAULT_UNIFORM_BUFFER_NONE &&
			    context->vertexProgram->programHeader->defaultUniformBufferCount != 0)
				return SCE_GXM_ERROR_UNIFORM_BUFFER_NOT_RESERVED;
			if (flags & GXM_CONTEXT_FLAG_VERTEX_TEXTURES) {
				ret = gxmContextWriteTextureBlock(&context->vertexBuffer,
								  &context->vertexTextureBlock,
								  program, context->vertexTextures);
				if (ret != 0)
					return ret;
				flags = context->flags;
			}
			if (flags & GXM_CONTEXT_FLAG_VERTEX_CONSTANTS) {
				ret = gxmContextWriteConstantBlock(&context->vertexBuffer,
								   &context->vertexConstantBlock,
								   program, context->vertexUniformBuffers,
								   context->vertexTextures);
				if (ret != 0)
					return ret;
			}
			ret = gxmContextWriteDmaBlock(&context->vertexBuffer, &context->vertexDmaBlock,
						      program, context->vertexUniformBuffers,
						      vertexProgram->secondaryPdsSize,
						      &vertexProgram->secondaryCode);
			if (ret != 0)
				return ret;
		}
		ret = gxmVdmWriteVertexState(&context->vdmBuffer, &context->vertexDmaBlock,
					     program->secondaryRegCount, SCE_FALSE);
		if (ret != 0)
			return ret;
		flags = context->flags;
		context->appliedVertexState = NULL;
	}
	context->flags = flags & ~GXM_CONTEXT_FLAG_VERTEX_ALL;
	return 0;
}

/* the dirty MTE state words, sent through the VDM stream */
static inline int gxmContextWriteMteState(SceGxmContext *context)
{
	SceUInt32 *words, count;
	const GxmCodeRef *copyProgram;
	GxmCommandBlock block;
	int ret;

	if ((context->state.dirty & GXM_DIRTY_REGION_CLIP) &&
	    context->type == SCE_GXM_CONTEXT_TYPE_DEFERRED) {
		SceUInt32 tileYMax = (SceUInt8)context->state.regionClip[1];

		if (context->listTileYMax >= tileYMax)
			tileYMax = context->listTileYMax;
		context->listTileYMax = tileYMax;
	}
	ret = gxmMteWriteStateWords(&context->vertexBuffer, &context->state, &words, &count);
	if (ret != 0)
		return ret;
	copyProgram = gxmMteGetCopyProgram(gxmGlobals.top.copyPrograms, count);
	ret = gxmMteWriteUpdateBlock(&context->vertexBuffer, (SceUInt32)words, count, copyProgram,
				     &block);
	if (ret != 0)
		return ret;
	ret = gxmVdmWriteFragmentState(&context->vdmBuffer, count, &block,
				       context->precomputedFragmentState != NULL);
	if (ret != 0)
		return ret;
	context->state.dirty = 0;
	return 0;
}

/* the state a draw needs that is not in the GPU's stream yet */
int gxmContextWriteState(SceGxmContext *context)
{
	const SceGxmPrecomputedFragmentState *fragmentState;
	const SceGxmPrecomputedVertexState *vertexState;
	SceUInt32 isp;
	int ret;

	if (context->flags & GXM_CONTEXT_FLAG_DEPTH_STENCIL_USED) {
		ret = gxmContextCheckDepthStencil(context);
		if (ret != 0)
			return ret;
		context->flags &= ~GXM_CONTEXT_FLAG_DEPTH_STENCIL_USED;
	}

	isp = context->state.frontIsp;
	if (!(isp & GXM_ISP_FRAGMENT_PROGRAM_DISABLE) ||
	    ((isp & GXM_ISP_TWO_SIDED) &&
	     !(context->state.backIsp & GXM_ISP_FRAGMENT_PROGRAM_DISABLE))) {
		fragmentState = context->precomputedFragmentState;
		if (fragmentState == NULL) {
			ret = gxmContextWriteFragmentState(context);
			if (ret != 0)
				return ret;
		} else if (fragmentState != context->appliedFragmentState) {
			context->state.dirty |= GXM_DIRTY_PDS_STATE;
			context->state.pdsState[0] = fragmentState->pdsState[0];
			context->state.pdsState[1] = fragmentState->pdsState[1];
			context->state.pdsState[2] = fragmentState->pdsState[2];
			context->appliedFragmentState = fragmentState;
		}
	}

	if (context->state.dirty != 0) {
		ret = gxmContextWriteMteState(context);
		if (ret != 0)
			return ret;
	}

	vertexState = context->precomputedVertexState;
	if (vertexState == NULL)
		return gxmContextWriteVertexState(context);
	if (vertexState != context->appliedVertexState) {
		GxmVdmState *command = gxmChunkBufferReserve(&context->vdmBuffer, sizeof(*command), 4);

		if (command == NULL)
			return SCE_GXM_ERROR_RESERVE_FAILED;
		context->vdmBuffer.cur = (SceUInt32)(command + 1);
		*command = vertexState->vdmState;
		context->appliedVertexState = vertexState;
	}
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
