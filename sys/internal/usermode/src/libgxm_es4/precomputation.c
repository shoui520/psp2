/*
 * SceGxm: precomputed states and draws, a program's blocks written once into the
 * caller's memory and patched by the setters, then sent without the context's work.
 */

#include "gxm_internal.h"

/* the DMA of the program's first range in uniform buffer index */
static inline void gxmPrecomputedSetDmaBuffer(SceUInt32 *dmaData, const SceGxmProgram *program,
					      SceUInt32 index, const void *buffer)
{
	const GxmProgramDmaEntry *entries = GXM_RELATIVE(program->secondaryPdsDataOffset);
	SceUInt32 count = program->secondaryPdsDataCount;
	SceUInt32 i;

	for (i = 0; i < count; i++) {
		if (entries[i].bufferIndex == index) {
			dmaData[i * 2] = (SceUInt32)buffer + entries[i].offset * 4;
			break;
		}
	}
}

/* the constant word of the program's first reference to uniform buffer index */
static inline void gxmPrecomputedSetConstantBuffer(SceUInt32 *constantBlock,
						   const SceGxmProgram *program, SceUInt32 index,
						   const void *buffer)
{
	const GxmProgramBufferRef *ref = GXM_RELATIVE(program->bufferRefsOffset);
	const GxmProgramBufferRef *refsEnd = ref + program->bufferRefCount;

	for (; ref != refsEnd; ref++) {
		if (ref->bufferIndex == index) {
			constantBlock[ref->dataWord] = (SceUInt32)buffer + ref->offset;
			break;
		}
	}
}

/* every DMA but the default buffer's, index 14 */
static inline void gxmPrecomputedSetAllDmaBuffers(SceUInt32 *dmaData, const SceGxmProgram *program,
						  const void *const *buffers)
{
	const GxmProgramDmaEntry *entries = GXM_RELATIVE(program->secondaryPdsDataOffset);
	SceUInt32 count = program->secondaryPdsDataCount;
	SceUInt32 i;

	for (i = 0; i < count; i++) {
		if (entries[i].bufferIndex < 14)
			dmaData[i * 2] = (SceUInt32)buffers[entries[i].bufferIndex] + entries[i].offset * 4;
	}
}

/* every constant word but the default buffer's */
static inline void gxmPrecomputedSetAllConstantBuffers(SceUInt32 *constantBlock,
						       const SceGxmProgram *program,
						       const void *const *buffers)
{
	const GxmProgramBufferRef *ref = GXM_RELATIVE(program->bufferRefsOffset);
	const GxmProgramBufferRef *refsEnd = ref + program->bufferRefCount;

	for (; ref != refsEnd; ref++) {
		if (ref->bufferIndex < 14)
			constantBlock[ref->dataWord] = (SceUInt32)buffers[ref->bufferIndex] + ref->offset;
	}
}

/* the words of one texture, where the block's references name texture index */
static inline void gxmPrecomputedSetTextureWords(SceUInt32 *block, const GxmProgramTextureRef *refs,
						 SceUInt32 count, SceUInt32 index,
						 const SceUInt32 *words)
{
	SceUInt32 i;

	for (i = 0; i < count; i++) {
		SceUInt32 textureWord = refs[i].textureWord;
		SceUInt32 dataWord = refs[i].dataWord;

		if (textureWord >> 2 == index)
			block[dataWord] = words[textureWord & 3];
	}
}

/* every texture word of a block */
static inline void gxmPrecomputedSetAllTextureWords(SceUInt32 *block, const GxmProgramTextureRef *refs,
						    SceUInt32 count, const SceGxmTexture *textures)
{
	SceUInt32 i;

	for (i = 0; i < count; i++) {
		SceUInt32 word = textures[refs[i].textureWord >> 2].controlWords[refs[i].textureWord & 3];

		if ((refs[i].textureWord & 3) == 0)
			word &= GXM_TEX0_BOUND_MASK;
		block[refs[i].dataWord] = word;
	}
}

/* the textures a program samples must be neither YUV nor palettized */
static inline int gxmPrecomputedCheckTextures(const SceGxmProgram *program,
					      const SceGxmTexture *textures)
{
	SceUInt32 i;

	for (i = 0; i < 2; i++) {
		const SceGxmTexture *texture = textures + i * 8;
		SceUInt32 units = program->texUnitFlags[i];

		for (; units != 0; units >>= 4, texture++) {
			if (units & 0xf) {
				SceGxmTextureFormat format = sceGxmTextureGetFormat(texture);

				if (gxmTextureFormatIsYuv(format) || gxmTextureFormatIsPalettized(format))
					return SCE_GXM_ERROR_UNSUPPORTED;
			}
		}
	}
	return 0;
}

SceSize sceGxmGetPrecomputedVertexStateSize(const SceGxmVertexProgram *vertexProgram)
{
	const SceGxmProgram *program = vertexProgram->programHeader;

	return vertexProgram->secondaryPdsSize +
	       (program->dataBufferCount + program->textureBufferCount) * 4;
}

int sceGxmPrecomputedVertexStateInit(SceGxmPrecomputedVertexState *precomputedState,
				     const SceGxmVertexProgram *vertexProgram, void *memBlock)
{
	const SceGxmProgram *program;
	const void *buffers[20];
	const GxmProgramBufferRef *ref, *refsEnd;
	const GxmProgramLiteral *literal, *literalsEnd;
	SceUInt32 *constantBlock, *textureBlock;
	SceSize offset;
	GxmCommandBlock dmaBlock;
	GxmChunkBuffer buffer;
	SceUInt32 i;

	if (precomputedState == NULL || vertexProgram == NULL || memBlock == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if ((SceUInt32)memBlock & 0xf)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;

	/* the DMA list, then the constant block, then the texture block */
	program = vertexProgram->programHeader;
	offset = vertexProgram->secondaryPdsSize;
	constantBlock = NULL;
	if (program->dataBufferCount != 0) {
		constantBlock = (SceUInt32 *)((char *)memBlock + offset);
		offset += program->dataBufferCount * 4;
	}
	textureBlock = NULL;
	if (program->textureBufferCount != 0)
		textureBlock = (SceUInt32 *)((char *)memBlock + offset);
	precomputedState->program = vertexProgram;
	precomputedState->dmaBlock = memBlock;
	precomputedState->constantBlock = constantBlock;
	precomputedState->textureBlock = textureBlock;
	precomputedState->defaultUniformBuffer = NULL;

	/* unset uniform buffers point at unmapped memory until a setter patches them */
	for (i = 0; i < 15; i++)
		buffers[i] = GXM_UNSET_UNIFORM_BUFFER(i);
	buffers[15] = textureBlock;
	buffers[16] = vertexProgram->literals;
	buffers[17] = vertexProgram->scratch;
	buffers[18] = vertexProgram->thread;
	buffers[19] = constantBlock;

	ref = GXM_RELATIVE(program->bufferRefsOffset);
	refsEnd = ref + program->bufferRefCount;
	for (; ref != refsEnd; ref++)
		constantBlock[ref->dataWord] = (SceUInt32)buffers[ref->bufferIndex] + ref->offset;
	literal = GXM_RELATIVE(program->literalsOffset);
	literalsEnd = literal + program->literalCount;
	for (; literal != literalsEnd; literal++)
		constantBlock[literal->dataWord] = literal->value;

	gxmProgramWriteDmaBlock(memBlock, &dmaBlock, program, buffers, &vertexProgram->secondaryCode);
	gxmChunkBufferInitFixed(&buffer, &precomputedState->vdmState, 0, sizeof(GxmVdmState));
	gxmVdmWriteVertexState(&buffer, &dmaBlock, program->secondaryRegCount, SCE_TRUE);
	return 0;
}

void *sceGxmPrecomputedVertexStateGetDefaultUniformBuffer(const SceGxmPrecomputedVertexState *precomputedState)
{
	return (void *)precomputedState->defaultUniformBuffer;
}

int sceGxmPrecomputedVertexStateSetDefaultUniformBuffer(SceGxmPrecomputedVertexState *precomputedState,
							void *defaultBuffer)
{
	const SceGxmProgram *program;

	if (precomputedState == NULL || defaultBuffer == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	precomputedState->defaultUniformBuffer = defaultBuffer;
	program = precomputedState->program->programHeader;
	gxmPrecomputedSetDmaBuffer(precomputedState->dmaBlock, program, 14, defaultBuffer);
	gxmPrecomputedSetConstantBuffer(precomputedState->constantBlock, program, 14, defaultBuffer);
	return 0;
}

/* the older export: no check of the texture's format */
int SceGxm_1625D348(SceGxmPrecomputedVertexState *precomputedState, SceUInt32 textureIndex,
		    const SceGxmTexture *texture)
{
	const SceGxmProgram *program;
	SceUInt32 words[4];

	if (precomputedState == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (textureIndex > 15)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;

	program = precomputedState->program->programHeader;
	words[0] = texture->controlWords[0] & GXM_TEX0_BOUND_MASK;
	words[1] = texture->controlWords[1];
	words[2] = texture->controlWords[2];
	words[3] = texture->controlWords[3];
	gxmPrecomputedSetTextureWords(precomputedState->constantBlock,
				      GXM_RELATIVE(program->constantTexturesOffset),
				      program->constantTextureCount, textureIndex, words);
	gxmPrecomputedSetTextureWords(precomputedState->textureBlock,
				      GXM_RELATIVE(program->textureRefsOffset),
				      program->textureRefCount, textureIndex, words);
	return 0;
}

int sceGxmPrecomputedVertexStateSetTexture(SceGxmPrecomputedVertexState *precomputedState,
					   SceUInt32 textureIndex, const SceGxmTexture *texture)
{
	const SceGxmProgram *program;
	SceGxmTextureFormat format;
	SceUInt32 words[4];

	if (precomputedState == NULL || texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (textureIndex > 15)
		return SCE_GXM_ERROR_INVALID_VALUE;
	format = sceGxmTextureGetFormat(texture);
	if (gxmTextureFormatIsYuv(format) || gxmTextureFormatIsPalettized(format))
		return SCE_GXM_ERROR_UNSUPPORTED;

	program = precomputedState->program->programHeader;
	words[0] = texture->controlWords[0] & GXM_TEX0_BOUND_MASK;
	words[1] = texture->controlWords[1];
	words[2] = texture->controlWords[2];
	words[3] = texture->controlWords[3];
	gxmPrecomputedSetTextureWords(precomputedState->constantBlock,
				      GXM_RELATIVE(program->constantTexturesOffset),
				      program->constantTextureCount, textureIndex, words);
	gxmPrecomputedSetTextureWords(precomputedState->textureBlock,
				      GXM_RELATIVE(program->textureRefsOffset),
				      program->textureRefCount, textureIndex, words);
	return 0;
}

/* the older export: no check of the textures' formats */
int SceGxm_8FF68274(SceGxmPrecomputedVertexState *precomputedState, const SceGxmTexture *textures)
{
	const SceGxmProgram *program;

	if (precomputedState == NULL || textures == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	program = precomputedState->program->programHeader;
	gxmPrecomputedSetAllTextureWords(precomputedState->constantBlock,
					 GXM_RELATIVE(program->constantTexturesOffset),
					 program->constantTextureCount, textures);
	gxmPrecomputedSetAllTextureWords(precomputedState->textureBlock,
					 GXM_RELATIVE(program->textureRefsOffset),
					 program->textureRefCount, textures);
	return 0;
}

int sceGxmPrecomputedVertexStateSetAllTextures(SceGxmPrecomputedVertexState *precomputedState,
					       const SceGxmTexture *textures)
{
	const SceGxmProgram *program;
	int ret;

	if (precomputedState == NULL || textures == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	program = precomputedState->program->programHeader;
	ret = gxmPrecomputedCheckTextures(program, textures);
	if (ret != 0)
		return ret;
	gxmPrecomputedSetAllTextureWords(precomputedState->constantBlock,
					 GXM_RELATIVE(program->constantTexturesOffset),
					 program->constantTextureCount, textures);
	gxmPrecomputedSetAllTextureWords(precomputedState->textureBlock,
					 GXM_RELATIVE(program->textureRefsOffset),
					 program->textureRefCount, textures);
	return 0;
}

int sceGxmPrecomputedVertexStateSetAllUniformBuffers(SceGxmPrecomputedVertexState *precomputedState,
						     const void *const *bufferDataArray)
{
	const SceGxmProgram *program;

	if (precomputedState == NULL || bufferDataArray == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	program = precomputedState->program->programHeader;
	gxmPrecomputedSetAllDmaBuffers(precomputedState->dmaBlock, program, bufferDataArray);
	gxmPrecomputedSetAllConstantBuffers(precomputedState->constantBlock, program, bufferDataArray);
	return 0;
}

int sceGxmPrecomputedVertexStateSetUniformBuffer(SceGxmPrecomputedVertexState *precomputedState,
						 SceUInt32 bufferIndex, const void *bufferData)
{
	const SceGxmProgram *program;

	if (precomputedState == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (bufferIndex > 13)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (bufferData == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	program = precomputedState->program->programHeader;
	gxmPrecomputedSetDmaBuffer(precomputedState->dmaBlock, program, bufferIndex, bufferData);
	gxmPrecomputedSetConstantBuffer(precomputedState->constantBlock, program, bufferIndex,
					bufferData);
	return 0;
}

SceSize sceGxmGetPrecomputedFragmentStateSize(const SceGxmFragmentProgram *fragmentProgram)
{
	const SceGxmProgram *program = fragmentProgram->programHeader;

	return ((fragmentProgram->pds.size + 15) & ~15) + fragmentProgram->secondaryPdsSize +
	       (program->dataBufferCount + program->textureBufferCount) * 4;
}

int sceGxmPrecomputedFragmentStateInit(SceGxmPrecomputedFragmentState *precomputedState,
				       const SceGxmFragmentProgram *fragmentProgram, void *memBlock)
{
	const SceGxmProgram *program;
	const void *buffers[20];
	const GxmProgramBufferRef *ref, *refsEnd;
	const GxmProgramLiteral *literal, *literalsEnd;
	SceUInt32 *constantBlock, *textureBlock;
	SceSize textureStateSize, offset;
	GxmCommandBlock dmaBlock, textureState;
	GxmChunkBuffer buffer;
	void *dmaData;
	SceUInt32 i;

	if (precomputedState == NULL || fragmentProgram == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (sceGxmFragmentProgramGetPassType(fragmentProgram) == GXM_ISP_PASS_TYPE_MASK_UPDATE)
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (memBlock == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if ((SceUInt32)memBlock & 0xf)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;

	/* the PDS program with the textures, the DMA list, the constant and texture blocks */
	program = fragmentProgram->programHeader;
	textureStateSize = (fragmentProgram->pds.size + 15) & ~15;
	offset = textureStateSize + fragmentProgram->secondaryPdsSize;
	constantBlock = NULL;
	if (program->dataBufferCount != 0) {
		constantBlock = (SceUInt32 *)((char *)memBlock + offset);
		offset += program->dataBufferCount * 4;
	}
	textureBlock = NULL;
	if (program->textureBufferCount != 0)
		textureBlock = (SceUInt32 *)((char *)memBlock + offset);
	dmaData = (char *)memBlock + textureStateSize;
	precomputedState->program = fragmentProgram;
	precomputedState->textureStateBlock = memBlock;
	precomputedState->dmaBlock = dmaData;
	precomputedState->constantBlock = constantBlock;
	precomputedState->textureBlock = textureBlock;
	precomputedState->defaultUniformBuffer = NULL;

	for (i = 0; i < 15; i++)
		buffers[i] = GXM_UNSET_UNIFORM_BUFFER(i);
	buffers[15] = textureBlock;
	buffers[16] = fragmentProgram->literals;
	buffers[17] = fragmentProgram->scratch;
	buffers[18] = fragmentProgram->thread;
	buffers[19] = constantBlock;

	ref = GXM_RELATIVE(program->bufferRefsOffset);
	refsEnd = ref + program->bufferRefCount;
	for (; ref != refsEnd; ref++)
		constantBlock[ref->dataWord] = (SceUInt32)buffers[ref->bufferIndex] + ref->offset;
	literal = GXM_RELATIVE(program->literalsOffset);
	literalsEnd = literal + program->literalCount;
	for (; literal != literalsEnd; literal++)
		constantBlock[literal->dataWord] = literal->value;

	gxmChunkBufferInitFixed(&buffer, memBlock, 0, textureStateSize);
	gxmCommandBlockCopy(&buffer, &fragmentProgram->pds, &textureState);
	gxmProgramWriteDmaBlock(dmaData, &dmaBlock, program, buffers, &fragmentProgram->secondaryCode);
	gxmPdsWriteStateWords(precomputedState->pdsState, &dmaBlock, fragmentProgram->dms,
			      &textureState);
	return 0;
}

int sceGxmPrecomputedFragmentStateSetDefaultUniformBuffer(SceGxmPrecomputedFragmentState *precomputedState,
							  void *defaultBuffer)
{
	const SceGxmProgram *program;

	if (precomputedState == NULL || defaultBuffer == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	precomputedState->defaultUniformBuffer = defaultBuffer;
	program = precomputedState->program->programHeader;
	gxmPrecomputedSetDmaBuffer(precomputedState->dmaBlock, program, 14, defaultBuffer);
	gxmPrecomputedSetConstantBuffer(precomputedState->constantBlock, program, 14, defaultBuffer);
	return 0;
}

void *sceGxmPrecomputedFragmentStateGetDefaultUniformBuffer(const SceGxmPrecomputedFragmentState *precomputedState)
{
	return (void *)precomputedState->defaultUniformBuffer;
}

int sceGxmPrecomputedFragmentStateSetAllTextures(SceGxmPrecomputedFragmentState *precomputedState,
						 const SceGxmTexture *textureArray)
{
	const SceGxmFragmentProgram *fragmentProgram;
	const SceGxmProgram *program;
	const SceUInt32 *indices;
	SceGxmTexture *entries;
	SceUInt32 count, i;

	if (precomputedState == NULL || textureArray == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;

	/* the PDS program's texture state, one entry per texture it samples */
	fragmentProgram = precomputedState->program;
	entries = precomputedState->textureStateBlock;
	count = fragmentProgram->textureIndexCount;
	program = fragmentProgram->programHeader;
	indices = fragmentProgram->textureIndices;
	for (i = 0; i < count; i++) {
		const SceGxmTexture *texture = &textureArray[indices[i]];

		entries[i].controlWords[0] = texture->controlWords[0] & GXM_TEX0_BOUND_MASK;
		entries[i].controlWords[1] = texture->controlWords[1];
		entries[i].controlWords[2] = texture->controlWords[2];
		entries[i].controlWords[3] = texture->controlWords[3];
	}
	gxmPrecomputedSetAllTextureWords(precomputedState->constantBlock,
					 GXM_RELATIVE(program->constantTexturesOffset),
					 program->constantTextureCount, textureArray);
	gxmPrecomputedSetAllTextureWords(precomputedState->textureBlock,
					 GXM_RELATIVE(program->textureRefsOffset),
					 program->textureRefCount, textureArray);
	return 0;
}

int sceGxmPrecomputedFragmentStateSetTexture(SceGxmPrecomputedFragmentState *precomputedState,
					     SceUInt32 textureIndex, const SceGxmTexture *texture)
{
	const SceGxmFragmentProgram *fragmentProgram;
	const SceGxmProgram *program;
	const SceUInt32 *indices;
	SceGxmTexture bound, *entry;
	SceUInt32 count, i;

	if (precomputedState == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (textureIndex > 15)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (texture == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;

	bound = *texture;
	bound.controlWords[0] &= GXM_TEX0_BOUND_MASK;
	fragmentProgram = precomputedState->program;
	count = fragmentProgram->textureIndexCount;
	program = fragmentProgram->programHeader;
	indices = fragmentProgram->textureIndices;
	entry = precomputedState->textureStateBlock;
	for (i = 0; i < count; i++, entry++) {
		if (indices[i] == textureIndex)
			*entry = bound;
	}
	gxmPrecomputedSetTextureWords(precomputedState->constantBlock,
				      GXM_RELATIVE(program->constantTexturesOffset),
				      program->constantTextureCount, textureIndex, bound.controlWords);
	gxmPrecomputedSetTextureWords(precomputedState->textureBlock,
				      GXM_RELATIVE(program->textureRefsOffset),
				      program->textureRefCount, textureIndex, bound.controlWords);
	return 0;
}

int sceGxmPrecomputedFragmentStateSetAllUniformBuffers(SceGxmPrecomputedFragmentState *precomputedState,
						       const void *const *bufferDataArray)
{
	const SceGxmProgram *program;

	if (precomputedState == NULL || bufferDataArray == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	program = precomputedState->program->programHeader;
	gxmPrecomputedSetAllDmaBuffers(precomputedState->dmaBlock, program, bufferDataArray);
	gxmPrecomputedSetAllConstantBuffers(precomputedState->constantBlock, program, bufferDataArray);
	return 0;
}

int sceGxmPrecomputedFragmentStateSetUniformBuffer(SceGxmPrecomputedFragmentState *precomputedState,
						   SceUInt32 bufferIndex, const void *bufferData)
{
	const SceGxmProgram *program;

	if (precomputedState == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (bufferIndex > 13)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (bufferData == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	program = precomputedState->program->programHeader;
	gxmPrecomputedSetDmaBuffer(precomputedState->dmaBlock, program, bufferIndex, bufferData);
	gxmPrecomputedSetConstantBuffer(precomputedState->constantBlock, program, bufferIndex,
					bufferData);
	return 0;
}

SceSize sceGxmGetPrecomputedDrawSize(const SceGxmVertexProgram *vertexProgram)
{
	return vertexProgram->pds.size;
}

int sceGxmPrecomputedDrawInit(SceGxmPrecomputedDraw *precomputedDraw,
			      const SceGxmVertexProgram *vertexProgram, void *memBlock)
{
	SceUInt32 streams[16];
	GxmChunkBuffer buffer;
	SceUInt32 i;

	if (precomputedDraw == NULL || memBlock == NULL || vertexProgram == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if ((SceUInt32)memBlock & 0xf)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;

	precomputedDraw->vertexProgram = vertexProgram;
	gxmChunkBufferInitFixed(&buffer, memBlock, 0, vertexProgram->pds.size);
	gxmCommandBlockCopy(&buffer, &vertexProgram->pds, &precomputedDraw->streamBlock);
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_DRAW_STREAMS_UNSET)) {
		for (i = 0; i < 16; i++)
			streams[i] = (SceUInt32)GXM_UNSET_VERTEX_STREAM(i);
		gxmPdsPatchAttributes(precomputedDraw->streamBlock.data, vertexProgram->patches,
				      vertexProgram->patchCount, streams);
	}
	return 0;
}

int sceGxmPrecomputedDrawSetAllVertexStreams(SceGxmPrecomputedDraw *precomputedDraw,
					     const void *const *streamDataArray)
{
	const SceGxmVertexProgram *vertexProgram;

	if (precomputedDraw == NULL || streamDataArray == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	vertexProgram = precomputedDraw->vertexProgram;
	gxmPdsPatchAttributesPrecomputed(precomputedDraw->streamBlock.data, vertexProgram->patches,
					 vertexProgram->patchCount,
					 (const SceUInt32 *)streamDataArray);
	return 0;
}

int sceGxmPrecomputedDrawSetVertexStream(SceGxmPrecomputedDraw *precomputedDraw,
					 SceUInt32 streamIndex, const void *streamData)
{
	const SceGxmVertexProgram *vertexProgram;

	if (precomputedDraw == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (streamIndex > 15)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (streamData == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	vertexProgram = precomputedDraw->vertexProgram;
	gxmPdsPatchStream(precomputedDraw->streamBlock.data, vertexProgram->patches,
			  vertexProgram->patchCount, streamIndex, (SceUInt32)streamData);
	return 0;
}

int sceGxmPrecomputedDrawSetParams(SceGxmPrecomputedDraw *precomputedDraw,
				   SceUInt32 primType, SceUInt32 indexType,
				   const void *indexData, SceUInt32 indexCount)
{
	const SceGxmVertexProgram *vertexProgram;
	GxmChunkBuffer buffer;

	if (precomputedDraw == NULL || indexData == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	vertexProgram = precomputedDraw->vertexProgram;
	gxmChunkBufferInitFixed(&buffer, &precomputedDraw->primitive, 0, sizeof(GxmVdmPrimitive));
	return gxmVdmWritePrimitive(&buffer, &precomputedDraw->streamBlock,
				    (const SceUInt8 *)&vertexProgram->dms, primType, indexType,
				    (SceUInt32)indexData, indexCount, 0, 1);
}

int sceGxmPrecomputedDrawSetParamsInstanced(SceGxmPrecomputedDraw *precomputedDraw,
					    SceUInt32 primType,
					    SceUInt32 indexType, const void *indexData,
					    SceUInt32 indexCount, SceUInt32 indexWrap)
{
	const SceGxmVertexProgram *vertexProgram;
	GxmChunkBuffer buffer;

	if (precomputedDraw == NULL || indexData == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	vertexProgram = precomputedDraw->vertexProgram;
	gxmChunkBufferInitFixed(&buffer, &precomputedDraw->primitive, 0, sizeof(GxmVdmPrimitive));
	return gxmVdmWritePrimitive(&buffer, &precomputedDraw->streamBlock,
				    (const SceUInt8 *)&vertexProgram->dms, primType, indexType,
				    (SceUInt32)indexData, indexCount, indexWrap, 1);
}
