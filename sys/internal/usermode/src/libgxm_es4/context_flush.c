/*
 * SceGxm: writing a context's state into its buffers before a draw or a kick.
 */

#include "gxm_internal.h"

/* the scene keeps the depth and stencil data a command list's draws use */
int gxmContextCheckCommandList(const SceGxmContext *context, const SceGxmCommandList *list)
{
	SceUInt32 zls = context->depthStencil.zlsControl;

	if (!(zls & GXM_ZLS_STORE_DEPTH)) {
		if (list->depthStencilUsage & GXM_LIST_USES_DEPTH_FRONT) {
			GXM_DBG_ERROR(135, "During the construction of parameter commandList a depth "
				      "comparison function for front-facing primitives was specified "
				      "that was not SCE_GXM_DEPTH_FUNC_NEVER or SCE_GXM_DEPTH_FUNC_ALWAYS "
				      "when depth/stencil surface is not enabled, or depth/stencil "
				      "format does not use depth.");
			return SCE_GXM_ERROR_INVALID_DEPTH_STENCIL_CONFIGURATION;
		}
		if (list->depthStencilUsage & GXM_LIST_USES_DEPTH_BACK) {
			GXM_DBG_ERROR(140, "During the construction of parameter commandList a depth "
				      "comparison function for back-facing primitives was specified "
				      "that was not SCE_GXM_DEPTH_FUNC_NEVER or SCE_GXM_DEPTH_FUNC_ALWAYS "
				      "when depth/stencil surface is not enabled, or depth/stencil "
				      "format does not use depth.");
			return SCE_GXM_ERROR_INVALID_DEPTH_STENCIL_CONFIGURATION;
		}
	}
	if (!(zls & GXM_ZLS_STORE_STENCIL)) {
		if (list->depthStencilUsage & GXM_LIST_USES_STENCIL_FRONT) {
			GXM_DBG_ERROR(147, "During the construction of parameter commandList a stencil "
				      "comparison function for front-facing primitives was specified "
				      "that was not SCE_GXM_STENCIL_FUNC_NEVER or "
				      "SCE_GXM_STENCIL_FUNC_ALWAYS when depth/stencil surface is not "
				      "enabled, or depth/stencil format does not use stencil.");
			return SCE_GXM_ERROR_INVALID_DEPTH_STENCIL_CONFIGURATION;
		}
		if (list->depthStencilUsage & GXM_LIST_USES_STENCIL_BACK) {
			GXM_DBG_ERROR(152, "During the construction of parameter commandList a stencil "
				      "comparison function for back-facing primitives was specified "
				      "that was not SCE_GXM_STENCIL_FUNC_NEVER or "
				      "SCE_GXM_STENCIL_FUNC_ALWAYS when depth/stencil surface is not "
				      "enabled, or depth/stencil format does not use stencil.");
			return SCE_GXM_ERROR_INVALID_DEPTH_STENCIL_CONFIGURATION;
		}
	}
	if (zls & GXM_ZLS_STORE_MASK)
		return 0;
	if (list->depthStencilUsage & GXM_LIST_USES_MASK) {
		GXM_DBG_ERROR(159, "During the construction of parameter commandList a mask update "
			      "fragment program was used when the depth/stencil surface is not "
			      "enabled, or the depth/stencil format does not contain the mask bit.");
		return SCE_GXM_ERROR_INVALID_DEPTH_STENCIL_CONFIGURATION;
	}
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
			if (gxmIspReadsDepth(isp)) {
				GXM_DBG_ERROR(44, "Depth comparison function for front-facing "
					      "primitives must be SCE_GXM_DEPTH_FUNC_NEVER or "
					      "SCE_GXM_DEPTH_FUNC_ALWAYS when depth/stencil surface is "
					      "not enabled, or depth/stencil format does not use depth.");
				return SCE_GXM_ERROR_INVALID_DEPTH_STENCIL_CONFIGURATION;
			}
			if ((isp & GXM_ISP_TWO_SIDED) && gxmIspReadsDepth(context->state.backIsp)) {
				GXM_DBG_ERROR(51, "Depth comparison function for back-facing "
					      "primitives must be SCE_GXM_DEPTH_FUNC_NEVER or "
					      "SCE_GXM_DEPTH_FUNC_ALWAYS when depth/stencil surface is "
					      "not enabled, or depth/stencil format does not use depth.");
				return SCE_GXM_ERROR_INVALID_DEPTH_STENCIL_CONFIGURATION;
			}
		}
		if (!(zls & GXM_ZLS_STORE_STENCIL)) {
			if (gxmStencilReadsStencil(context->state.frontStencil)) {
				GXM_DBG_ERROR(60, "Stencil comparison function for front-facing "
					      "primitives must be SCE_GXM_STENCIL_FUNC_NEVER or "
					      "SCE_GXM_STENCIL_FUNC_ALWAYS when depth/stencil surface is "
					      "not enabled, or depth/stencil format does not use stencil.");
				return SCE_GXM_ERROR_INVALID_DEPTH_STENCIL_CONFIGURATION;
			}
			if ((isp & GXM_ISP_TWO_SIDED) &&
			    gxmStencilReadsStencil(context->state.backStencil)) {
				GXM_DBG_ERROR(67, "Stencil comparison function for back-facing "
					      "primitives must be SCE_GXM_STENCIL_FUNC_NEVER or "
					      "SCE_GXM_STENCIL_FUNC_ALWAYS when depth/stencil surface is "
					      "not enabled, or depth/stencil format does not use stencil.");
				return SCE_GXM_ERROR_INVALID_DEPTH_STENCIL_CONFIGURATION;
			}
		}
		if (!(zls & GXM_ZLS_STORE_MASK) &&
		    (isp & GXM_ISP_PASS_TYPE) == GXM_ISP_PASS_TYPE_MASK_UPDATE) {
			GXM_DBG_ERROR(76, "The mask update fragment program cannot be used when the "
				      "depth/stencil surface is not enabled, or the depth/stencil format "
				      "does not contain the mask bit.");
			return SCE_GXM_ERROR_INVALID_DEPTH_STENCIL_CONFIGURATION;
		}
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
					      const SceGxmTexture *textures, int line)
{
	SceUInt32 textureBufferRegCount = program->textureBufferCount;
	void *block;
	int ret;

	GXM_DBG_CHECK(__FILE_NAME__, line, textureBufferRegCount, "textureBufferRegCount");
	ret = gxmChunkBufferAlloc(buffer, textureBufferRegCount * 4, 4, &block);
	if (ret != 0)
		return ret;
	*textureBlock = block;
	gxmProgramWriteTextureWords(block, GXM_RELATIVE(program->textureRefsOffset),
				    program->textureRefCount, (const SceUInt32 *)textures);
	return 0;
}

/* uniform buffer addresses, literals and texture words */
static inline int gxmContextWriteConstantBlock(GxmChunkBuffer *buffer, void **constantBlock,
					       const SceGxmProgram *program,
					       const void *const *uniformBuffers,
					       const SceGxmTexture *textures, int line,
					       const char *countText)
{
	SceUInt32 dataBufferRegCount = program->dataBufferCount;
	SceUInt32 *block;
	void *data;
	const GxmProgramBufferRef *ref, *refsEnd;
	const GxmProgramLiteral *literal, *literalsEnd;
	int ret;

	GXM_DBG_CHECK(__FILE_NAME__, line, dataBufferRegCount, countText);
	ret = gxmChunkBufferAlloc(buffer, dataBufferRegCount * 4, 4, &data);
	if (ret != 0)
		return ret;
	block = data;
	*constantBlock = block;
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
					  const GxmCodeRef *secondaryCode, int line)
{
	SceUInt32 *data = gxmChunkBufferReserve(buffer, size, 16);

	if (data == NULL) {
		GXM_DBG_ERROR(line, "Could not reserve %d bytes.", size);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	gxmChunkBufferCommit(buffer, gxmProgramWriteDmaBlock(data, dmaBlock, program,
							     uniformBuffers, secondaryCode));
	return 0;
}

/* the fragment program's PDS block, with the textures it samples */
static inline int gxmContextWriteTextureStateBlock(SceGxmContext *context,
						   const SceGxmFragmentProgram *fragmentProgram)
{
	SceSize size = fragmentProgram->pds.size;
	SceGxmTexture *block;
	void *data;
	SceUInt32 i;
	int ret;

	ret = gxmChunkBufferAlloc(&context->fragmentBuffer, size, 16, &data);
	if (ret != 0)
		return ret;
	block = data;
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
	const SceGxmProgram *program = fragmentProgram->program;
	SceUInt32 flags = context->flags;
	int ret;

	if (flags & GXM_CONTEXT_FLAG_FRAGMENT_PROGRAM) {
		if (flags & GXM_CONTEXT_FLAG_FRAGMENT_DATA) {
			if (context->fragmentDefaultUniformBufferMode == GXM_DEFAULT_UNIFORM_BUFFER_NONE &&
			    program->defaultUniformBufferCount != 0) {
				GXM_DBG_ERROR(325, "The fragment default uniform buffer has not been "
					      "reserved");
				return SCE_GXM_ERROR_UNIFORM_BUFFER_NOT_RESERVED;
			}
			if (flags & GXM_CONTEXT_FLAG_FRAGMENT_TEXTURES) {
				ret = gxmContextWriteTextureBlock(&context->fragmentBuffer,
								  &context->fragmentTextureBlock,
								  program, context->fragmentTextures, 173);
				if (ret != 0)
					return ret;
				flags = context->flags;
			}
			if (flags & GXM_CONTEXT_FLAG_FRAGMENT_CONSTANTS) {
				ret = gxmContextWriteConstantBlock(&context->fragmentBuffer,
								   &context->fragmentConstantBlock,
								   program, context->fragmentUniformBuffers,
								   context->fragmentTextures, 206,
								   "dataBufferRegCount");
				if (ret != 0)
					return ret;
			}
			ret = gxmContextWriteDmaBlock(&context->fragmentBuffer,
						      &context->fragmentDmaBlock, program,
						      context->fragmentUniformBuffers,
						      fragmentProgram->secondaryPdsSize,
						      &fragmentProgram->secondaryCode, 249);
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
	const SceGxmProgram *program = vertexProgram->program;
	SceUInt32 flags = context->flags;
	int ret;

	if (flags & GXM_CONTEXT_FLAG_VERTEX_PROGRAM) {
		if (flags & GXM_CONTEXT_FLAG_VERTEX_DATA) {
			if (context->vertexDefaultUniformBufferMode == GXM_DEFAULT_UNIFORM_BUFFER_NONE &&
			    context->vertexProgram->program->defaultUniformBufferCount != 0) {
				GXM_DBG_ERROR(556, "The vertex default uniform buffer has not been "
					      "reserved");
				return SCE_GXM_ERROR_UNIFORM_BUFFER_NOT_RESERVED;
			}
			if (flags & GXM_CONTEXT_FLAG_VERTEX_TEXTURES) {
				ret = gxmContextWriteTextureBlock(&context->vertexBuffer,
								  &context->vertexTextureBlock,
								  program, context->vertexTextures, 437);
				if (ret != 0)
					return ret;
				flags = context->flags;
			}
			if (flags & GXM_CONTEXT_FLAG_VERTEX_CONSTANTS) {
				ret = gxmContextWriteConstantBlock(&context->vertexBuffer,
								   &context->vertexConstantBlock,
								   program, context->vertexUniformBuffers,
								   context->vertexTextures, 470,
								   "program->dataBufferRegCount");
				if (ret != 0)
					return ret;
			}
			ret = gxmContextWriteDmaBlock(&context->vertexBuffer, &context->vertexDmaBlock,
						      program, context->vertexUniformBuffers,
						      vertexProgram->secondaryPdsSize,
						      &vertexProgram->secondaryCode, 514);
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
		void *command;

		ret = gxmChunkBufferAlloc(&context->vdmBuffer, sizeof(GxmVdmState), 4, &command);
		if (ret != 0)
			return ret;
		*(GxmVdmState *)command = vertexState->vdmState;
		context->appliedVertexState = vertexState;
	}
	return 0;
}

/* a used default uniform buffer is copied, so the next job may change it */
static inline const void *gxmContextCopyUniformBuffer(GxmChunkBuffer *buffer, const void *source,
						     const SceGxmProgram *program)
{
	SceSize size = program->defaultUniformBufferCount * 4;
	void *copy;

	gxmChunkBufferAlloc(buffer, size, 4, &copy);
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
					vertexProgram->program);

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
					fragmentProgram->program);

			context->fragmentUniformBuffers[14] = copy;
			context->fragmentDefaultUniformBuffer = copy;
		}
	} else if (context->fragmentDefaultUniformBufferMode != GXM_DEFAULT_UNIFORM_BUFFER_SET) {
		context->fragmentDefaultUniformBufferMode = GXM_DEFAULT_UNIFORM_BUFFER_NONE;
	}
}
