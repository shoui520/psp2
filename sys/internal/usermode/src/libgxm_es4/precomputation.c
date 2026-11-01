/*
 * SceGxm: precomputed states and draws, a program's blocks written once into the
 * caller's memory and patched by the setters, then sent without the context's work.
 */

#include <sceerror.h>
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
		SceUInt32 textureIndex = i * 8;

		for (; units != 0; units >>= 4, textureIndex++, texture++) {
			if (units & 0xf) {
				SceGxmTextureFormat format = sceGxmTextureGetFormat(texture);

				if (gxmTextureFormatIsYuv(format)) {
					GXM_DBG_ERROR(396, "Parameter textureArray[%d] to function "
						      "sceGxmPrecomputedVertexStateSetTexture cannot refer "
						      "to a texture with YUV texture format.", textureIndex);
					return SCE_GXM_ERROR_UNSUPPORTED;
				}
				if (gxmTextureFormatIsPalettized(format)) {
					GXM_DBG_ERROR(401, "Parameter textureArray[%d] to function "
						      "sceGxmPrecomputedVertexStateSetTexture cannot refer "
						      "to a texture with palettized texture format.",
						      textureIndex);
					return SCE_GXM_ERROR_UNSUPPORTED;
				}
			}
		}
	}
	return 0;
}

SceSize sceGxmGetPrecomputedVertexStateSize(const SceGxmVertexProgram *vertexProgram)
{
	const SceGxmProgram *program;

	GXM_DBG_ASSERT(29, vertexProgram);
	program = vertexProgram->program;
	GXM_DBG_ASSERT(32, program);
	GXM_DBG_ASSERT(33, sceGxmProgramGetProgramType(program) == SCE_GXM_VERTEX_PROGRAM);
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
	int ret, errorCode;

	if (precomputedState == NULL) {
		GXM_DBG_ERROR_NULL(58, sceGxmPrecomputedVertexStateInit, precomputedState);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (vertexProgram == NULL) {
		GXM_DBG_ERROR_NULL(62, sceGxmPrecomputedVertexStateInit, vertexProgram);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (memBlock == NULL) {
		GXM_DBG_ERROR_NULL(66, sceGxmPrecomputedVertexStateInit, memBlock);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (!SCE_GXM_PTR_IS_ALIGNED(memBlock, 16)) {
		GXM_DBG_ERROR(72, "Parameter memBlock to function sceGxmPrecomputedVertexStateInit has "
			      "value %p, which is not aligned to %d.", memBlock, 16);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}

	/* the DMA list, then the constant block, then the texture block */
	program = vertexProgram->program;
	GXM_DBG_ASSERT(75, program);
	GXM_DBG_ASSERT(76, sceGxmProgramGetProgramType(program) == SCE_GXM_VERTEX_PROGRAM);
	ret = GXM_DBG_CHECK_MEMORY(memBlock, sceGxmGetPrecomputedVertexStateSize(vertexProgram),
				   "memBlock", "sceGxmPrecomputedVertexStateInit");
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
	errorCode = gxmVdmWriteVertexState(&buffer, &dmaBlock, program->secondaryRegCount, SCE_TRUE);
	GXM_DBG_ASSERT(144, errorCode == SCE_OK);
	return ret;
}

void *sceGxmPrecomputedVertexStateGetDefaultUniformBuffer(const SceGxmPrecomputedVertexState *precomputedState)
{
	GXM_DBG_ASSERT(161, precomputedState);
	return (void *)precomputedState->defaultUniformBuffer;
}

int sceGxmPrecomputedVertexStateSetDefaultUniformBuffer(SceGxmPrecomputedVertexState *precomputedState,
							void *defaultBuffer)
{
	const SceGxmProgram *program;

	if (precomputedState == NULL) {
		GXM_DBG_ERROR_NULL(179, sceGxmPrecomputedVertexStateSetDefaultUniformBuffer,
				   precomputedState);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (defaultBuffer == NULL) {
		GXM_DBG_ERROR_NULL(183, sceGxmPrecomputedVertexStateSetDefaultUniformBuffer,
				   defaultBuffer);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	precomputedState->defaultUniformBuffer = defaultBuffer;
	program = precomputedState->program->program;
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
	int err = 0;

	if (precomputedState == NULL) {
		GXM_DBG_ERROR_NULL(222, sceGxmPrecomputedVertexStateSetTexture, precomputedState);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (textureIndex > 15) {
		GXM_DBG_ERROR_RANGE(228, sceGxmPrecomputedVertexStateSetTexture, textureIndex, 15);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(232, sceGxmPrecomputedVertexStateSetTexture, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}

	program = precomputedState->program->program;
#if SCE_DBG_ASSERTS_ENABLED
	err = gxmDebugCheckTexture(program, texture, textureIndex, "vertex");
#endif
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
#if SCE_DBG_ASSERTS_ENABLED
	gxmDebugReportError(err, "sceGxmPrecomputedVertexStateSetTexture");
#endif
	return err;
}

int sceGxmPrecomputedVertexStateSetTexture(SceGxmPrecomputedVertexState *precomputedState,
					   SceUInt32 textureIndex, const SceGxmTexture *texture)
{
	const SceGxmProgram *program;
	SceGxmTextureFormat format;
	SceUInt32 words[4];
	int err = 0;

	if (precomputedState == NULL) {
		GXM_DBG_ERROR_NULL(273, sceGxmPrecomputedVertexStateSetTexture, precomputedState);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(277, sceGxmPrecomputedVertexStateSetTexture, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (textureIndex > 15) {
		GXM_DBG_ERROR_RANGE(283, sceGxmPrecomputedVertexStateSetTexture, textureIndex, 15);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	format = sceGxmTextureGetFormat(texture);
	if (gxmTextureFormatIsYuv(format)) {
		GXM_DBG_ERROR(290, "Parameter texture to function sceGxmPrecomputedVertexStateSetTexture "
			      "cannot refer to a texture with YUV texture format.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (gxmTextureFormatIsPalettized(format)) {
		GXM_DBG_ERROR(294, "Parameter texture to function sceGxmPrecomputedVertexStateSetTexture "
			      "cannot refer to a texture with palettized texture format.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}

	program = precomputedState->program->program;
#if SCE_DBG_ASSERTS_ENABLED
	err = gxmDebugCheckTexture(program, texture, textureIndex, "vertex");
#endif
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
#if SCE_DBG_ASSERTS_ENABLED
	gxmDebugReportError(err, "sceGxmPrecomputedVertexStateSetTexture");
#endif
	return err;
}

/* the older export: no check of the textures' formats */
int SceGxm_8FF68274(SceGxmPrecomputedVertexState *precomputedState, const SceGxmTexture *textures)
{
	const SceGxmProgram *program;
	int err = 0;

	if (precomputedState == NULL) {
		GXM_DBG_ERROR_NULL(337, sceGxmPrecomputedVertexStateSetAllTextures, precomputedState);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (textures == NULL) {
		GXM_DBG_ERROR_NULL(341, sceGxmPrecomputedVertexStateSetAllTextures, textures);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	program = precomputedState->program->program;
#if SCE_DBG_ASSERTS_ENABLED
	err = gxmDebugCheckTextures(program, textures, "vertex");
#endif
	gxmPrecomputedSetAllTextureWords(precomputedState->constantBlock,
					 GXM_RELATIVE(program->constantTexturesOffset),
					 program->constantTextureCount, textures);
	gxmPrecomputedSetAllTextureWords(precomputedState->textureBlock,
					 GXM_RELATIVE(program->textureRefsOffset),
					 program->textureRefCount, textures);
#if SCE_DBG_ASSERTS_ENABLED
	gxmDebugReportError(err, "sceGxmPrecomputedVertexStateSetAllTextures");
#endif
	return err;
}

int sceGxmPrecomputedVertexStateSetAllTextures(SceGxmPrecomputedVertexState *precomputedState,
					       const SceGxmTexture *textureArray)
{
	const SceGxmProgram *program;
	int ret, err = 0;

	if (precomputedState == NULL) {
		GXM_DBG_ERROR_NULL(374, sceGxmPrecomputedVertexStateSetAllTextures, precomputedState);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (textureArray == NULL) {
		GXM_DBG_ERROR_NULL(378, sceGxmPrecomputedVertexStateSetAllTextures, textureArray);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	program = precomputedState->program->program;
	ret = gxmPrecomputedCheckTextures(program, textureArray);
	if (ret != 0)
		return ret;
#if SCE_DBG_ASSERTS_ENABLED
	err = gxmDebugCheckTextures(program, textureArray, "vertex");
#endif
	gxmPrecomputedSetAllTextureWords(precomputedState->constantBlock,
					 GXM_RELATIVE(program->constantTexturesOffset),
					 program->constantTextureCount, textureArray);
	gxmPrecomputedSetAllTextureWords(precomputedState->textureBlock,
					 GXM_RELATIVE(program->textureRefsOffset),
					 program->textureRefCount, textureArray);
#if SCE_DBG_ASSERTS_ENABLED
	gxmDebugReportError(err, "sceGxmPrecomputedVertexStateSetAllTextures");
#endif
	return err;
}

int sceGxmPrecomputedVertexStateSetAllUniformBuffers(SceGxmPrecomputedVertexState *precomputedState,
						     const void *const *bufferDataArray)
{
	const SceGxmProgram *program;

	if (precomputedState == NULL) {
		GXM_DBG_ERROR_NULL(438, sceGxmPrecomputedVertexStateSetAllUniformBuffers,
				   precomputedState);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (bufferDataArray == NULL) {
		GXM_DBG_ERROR_NULL(442, sceGxmPrecomputedVertexStateSetAllUniformBuffers,
				   bufferDataArray);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	program = precomputedState->program->program;
	gxmPrecomputedSetAllDmaBuffers(precomputedState->dmaBlock, program, bufferDataArray);
	gxmPrecomputedSetAllConstantBuffers(precomputedState->constantBlock, program, bufferDataArray);
	return 0;
}

int sceGxmPrecomputedVertexStateSetUniformBuffer(SceGxmPrecomputedVertexState *precomputedState,
						 SceUInt32 bufferIndex, const void *bufferData)
{
	const SceGxmProgram *program;

	if (precomputedState == NULL) {
		GXM_DBG_ERROR_NULL(467, sceGxmPrecomputedVertexStateSetUniformBuffer, precomputedState);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (bufferIndex > 13) {
		GXM_DBG_ERROR_RANGE(473, sceGxmPrecomputedVertexStateSetUniformBuffer, bufferIndex, 13);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (bufferData == NULL) {
		GXM_DBG_ERROR_NULL(477, sceGxmPrecomputedVertexStateSetUniformBuffer, bufferData);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	program = precomputedState->program->program;
	gxmPrecomputedSetDmaBuffer(precomputedState->dmaBlock, program, bufferIndex, bufferData);
	gxmPrecomputedSetConstantBuffer(precomputedState->constantBlock, program, bufferIndex,
					bufferData);
	return 0;
}

SceSize sceGxmGetPrecomputedFragmentStateSize(const SceGxmFragmentProgram *fragmentProgram)
{
	const SceGxmProgram *program;

	GXM_DBG_ASSERT(490, fragmentProgram);
	program = fragmentProgram->program;
	GXM_DBG_ASSERT(493, program);
	GXM_DBG_ASSERT(494, sceGxmProgramGetProgramType(program) == SCE_GXM_FRAGMENT_PROGRAM);
	return ((fragmentProgram->pds.totalSize + 15) & ~15) + fragmentProgram->secondaryPdsSize +
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
	int ret, errorCode;

	if (precomputedState == NULL) {
		GXM_DBG_ERROR_NULL(520, sceGxmPrecomputedFragmentStateInit, precomputedState);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (fragmentProgram == NULL) {
		GXM_DBG_ERROR_NULL(524, sceGxmPrecomputedFragmentStateInit, fragmentProgram);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (sceGxmFragmentProgramGetPassType(fragmentProgram) == GXM_ISP_PASS_TYPE_MASK_UPDATE) {
		GXM_DBG_ERROR(528, "Parameter fragmentProgram to function "
			      "sceGxmPrecomputedFragmentStateInit is a mask update fragment program, "
			      "which is not supported.");
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
	if (memBlock == NULL) {
		GXM_DBG_ERROR_NULL(532, sceGxmPrecomputedFragmentStateInit, memBlock);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (!SCE_GXM_PTR_IS_ALIGNED(memBlock, 16)) {
		GXM_DBG_ERROR(538, "Parameter memBlock to function sceGxmPrecomputedFragmentStateInit "
			      "has value %p, which is not aligned to %d.", memBlock, 16);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
#if SCE_DBG_ASSERTS_ENABLED
	if (!sceGxmFragmentProgramIsEnabled(fragmentProgram) &&
	    (gxmGlobals.warningMask & (1 << SCE_GXM_WARNING_PRECOMPUTING_DISABLED_FRAGMENT_STATE)))
		GXM_DBG_WARNING(547, "Precomputed fragment state at %p created unnecessarily for "
				"fragment program at %p that disables fragment shading.",
				precomputedState, fragmentProgram);
#endif

	/* the PDS program with the textures, the DMA list, the constant and texture blocks */
	program = fragmentProgram->program;
	GXM_DBG_ASSERT(551, program);
	GXM_DBG_ASSERT(552, sceGxmProgramGetProgramType(program) == SCE_GXM_FRAGMENT_PROGRAM);
	ret = GXM_DBG_CHECK_MEMORY(memBlock, sceGxmGetPrecomputedFragmentStateSize(fragmentProgram),
				   "memBlock", "sceGxmPrecomputedFragmentStateInit");
	textureStateSize = (fragmentProgram->pds.totalSize + 15) & ~15;
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
	errorCode = gxmCommandBlockCopy(&buffer, &fragmentProgram->pds, &textureState);
	GXM_DBG_ASSERT(614, errorCode == SCE_OK);
	gxmProgramWriteDmaBlock(dmaData, &dmaBlock, program, buffers, &fragmentProgram->secondaryCode);
	gxmPdsWriteStateWords(precomputedState->pdsState, &dmaBlock, fragmentProgram->dms,
			      &textureState);
	return ret;
}

int sceGxmPrecomputedFragmentStateSetDefaultUniformBuffer(SceGxmPrecomputedFragmentState *precomputedState,
							  void *defaultBuffer)
{
	const SceGxmProgram *program;

	if (precomputedState == NULL) {
		GXM_DBG_ERROR_NULL(653, sceGxmPrecomputedFragmentStateSetDefaultUniformBuffer,
				   precomputedState);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (defaultBuffer == NULL) {
		GXM_DBG_ERROR_NULL(657, sceGxmPrecomputedFragmentStateSetDefaultUniformBuffer,
				   defaultBuffer);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	precomputedState->defaultUniformBuffer = defaultBuffer;
	program = precomputedState->program->program;
	gxmPrecomputedSetDmaBuffer(precomputedState->dmaBlock, program, 14, defaultBuffer);
	gxmPrecomputedSetConstantBuffer(precomputedState->constantBlock, program, 14, defaultBuffer);
	return 0;
}

void *sceGxmPrecomputedFragmentStateGetDefaultUniformBuffer(const SceGxmPrecomputedFragmentState *precomputedState)
{
	GXM_DBG_ASSERT(687, precomputedState);
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
	int err = 0;

	if (precomputedState == NULL) {
		GXM_DBG_ERROR_NULL(705, sceGxmPrecomputedFragmentStateSetAllTextures, precomputedState);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (textureArray == NULL) {
		GXM_DBG_ERROR_NULL(709, sceGxmPrecomputedFragmentStateSetAllTextures, textureArray);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}

	/* the PDS program's texture state, one entry per texture it samples */
	fragmentProgram = precomputedState->program;
#if SCE_DBG_ASSERTS_ENABLED
	err = gxmDebugCheckTextures(fragmentProgram->program, textureArray, "fragment");
#endif
	entries = precomputedState->textureStateBlock;
	count = fragmentProgram->textureIndexCount;
	program = fragmentProgram->program;
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
#if SCE_DBG_ASSERTS_ENABLED
	gxmDebugReportError(err, "sceGxmPrecomputedFragmentStateSetAllTextures");
#endif
	return err;
}

int sceGxmPrecomputedFragmentStateSetTexture(SceGxmPrecomputedFragmentState *precomputedState,
					     SceUInt32 textureIndex, const SceGxmTexture *texture)
{
	const SceGxmFragmentProgram *fragmentProgram;
	const SceGxmProgram *program;
	const SceUInt32 *indices;
	SceGxmTexture bound, *entry;
	SceUInt32 count, i;
	int err = 0;

	if (precomputedState == NULL) {
		GXM_DBG_ERROR_NULL(751, sceGxmPrecomputedFragmentStateSetTexture, precomputedState);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (textureIndex > 15) {
		GXM_DBG_ERROR_RANGE(757, sceGxmPrecomputedFragmentStateSetTexture, textureIndex, 15);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (texture == NULL) {
		GXM_DBG_ERROR_NULL(761, sceGxmPrecomputedFragmentStateSetTexture, texture);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}

	fragmentProgram = precomputedState->program;
#if SCE_DBG_ASSERTS_ENABLED
	err = gxmDebugCheckTexture(fragmentProgram->program, texture, textureIndex, "fragment");
#endif
	bound = *texture;
	bound.controlWords[0] &= GXM_TEX0_BOUND_MASK;
	count = fragmentProgram->textureIndexCount;
	program = fragmentProgram->program;
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
#if SCE_DBG_ASSERTS_ENABLED
	gxmDebugReportError(err, "sceGxmPrecomputedFragmentStateSetTexture");
#endif
	return err;
}

int sceGxmPrecomputedFragmentStateSetAllUniformBuffers(SceGxmPrecomputedFragmentState *precomputedState,
						       const void *const *bufferDataArray)
{
	const SceGxmProgram *program;

	if (precomputedState == NULL) {
		GXM_DBG_ERROR_NULL(810, sceGxmPrecomputedFragmentStateSetAllUniformBuffers,
				   precomputedState);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (bufferDataArray == NULL) {
		GXM_DBG_ERROR_NULL(814, sceGxmPrecomputedFragmentStateSetAllUniformBuffers,
				   bufferDataArray);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	program = precomputedState->program->program;
	gxmPrecomputedSetAllDmaBuffers(precomputedState->dmaBlock, program, bufferDataArray);
	gxmPrecomputedSetAllConstantBuffers(precomputedState->constantBlock, program, bufferDataArray);
	return 0;
}

int sceGxmPrecomputedFragmentStateSetUniformBuffer(SceGxmPrecomputedFragmentState *precomputedState,
						   SceUInt32 bufferIndex, const void *bufferData)
{
	const SceGxmProgram *program;

	if (precomputedState == NULL) {
		GXM_DBG_ERROR_NULL(839, sceGxmPrecomputedFragmentStateSetUniformBuffer, precomputedState);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (bufferIndex > 13) {
		GXM_DBG_ERROR_RANGE(845, sceGxmPrecomputedFragmentStateSetUniformBuffer, bufferIndex, 13);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (bufferData == NULL) {
		GXM_DBG_ERROR_NULL(849, sceGxmPrecomputedFragmentStateSetUniformBuffer, bufferData);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	program = precomputedState->program->program;
	gxmPrecomputedSetDmaBuffer(precomputedState->dmaBlock, program, bufferIndex, bufferData);
	gxmPrecomputedSetConstantBuffer(precomputedState->constantBlock, program, bufferIndex,
					bufferData);
	return 0;
}

SceSize sceGxmGetPrecomputedDrawSize(const SceGxmVertexProgram *vertexProgram)
{
	GXM_DBG_ASSERT(862, vertexProgram);
	GXM_DBG_ASSERT(863, vertexProgram->program);
	GXM_DBG_ASSERT(864, sceGxmProgramGetProgramType(vertexProgram->program) == SCE_GXM_VERTEX_PROGRAM);
	return vertexProgram->pds.totalSize;
}

int sceGxmPrecomputedDrawInit(SceGxmPrecomputedDraw *precomputedDraw,
			      const SceGxmVertexProgram *vertexProgram, void *memBlock)
{
	SceUInt32 streams[16];
	GxmChunkBuffer buffer;
	SceUInt32 i;
	int ret, errorCode;

	if (precomputedDraw == NULL) {
		GXM_DBG_ERROR_NULL(882, sceGxmPrecomputedDrawInit, precomputedDraw);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (memBlock == NULL) {
		GXM_DBG_ERROR_NULL(886, sceGxmPrecomputedDrawInit, memBlock);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (vertexProgram == NULL) {
		GXM_DBG_ERROR_NULL(890, sceGxmPrecomputedDrawInit, vertexProgram);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (!SCE_GXM_PTR_IS_ALIGNED(memBlock, 16)) {
		GXM_DBG_ERROR(896, "Parameter memBlock to function sceGxmPrecomputedDrawInit has value "
			      "%p, which is not aligned to %d.", memBlock, 16);
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	}
	GXM_DBG_ASSERT(898, vertexProgram->program);
	GXM_DBG_ASSERT(899, sceGxmProgramGetProgramType(vertexProgram->program) == SCE_GXM_VERTEX_PROGRAM);
	ret = GXM_DBG_CHECK_MEMORY(memBlock, sceGxmGetPrecomputedDrawSize(vertexProgram), "memBlock",
				   "sceGxmPrecomputedDrawInit");

	precomputedDraw->vertexProgram = vertexProgram;
	gxmChunkBufferInitFixed(&buffer, memBlock, 0, vertexProgram->pds.totalSize);
	errorCode = gxmCommandBlockCopy(&buffer, &vertexProgram->pds, &precomputedDraw->streamBlock);
	GXM_DBG_ASSERT(922, errorCode == SCE_OK);
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_DRAW_STREAMS_UNSET)) {
		for (i = 0; i < 16; i++)
			streams[i] = (SceUInt32)GXM_UNSET_VERTEX_STREAM(i);
		gxmPdsPatchAttributes(precomputedDraw->streamBlock.data, vertexProgram->patches,
				      vertexProgram->patchCount, streams);
	}
	return ret;
}

int sceGxmPrecomputedDrawSetAllVertexStreams(SceGxmPrecomputedDraw *precomputedDraw,
					     const void *const *streamDataArray)
{
	const SceGxmVertexProgram *vertexProgram;

	if (precomputedDraw == NULL) {
		GXM_DBG_ERROR_NULL(959, sceGxmPrecomputedDrawSetAllVertexStreams, precomputedDraw);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (streamDataArray == NULL) {
		GXM_DBG_ERROR_NULL(963, sceGxmPrecomputedDrawSetAllVertexStreams, streamDataArray);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
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

	if (precomputedDraw == NULL) {
		GXM_DBG_ERROR_NULL(988, sceGxmPrecomputedDrawSetVertexStream, precomputedDraw);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (streamIndex > 15) {
		GXM_DBG_ERROR_RANGE(994, sceGxmPrecomputedDrawSetVertexStream, streamIndex, 15);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	if (streamData == NULL) {
		GXM_DBG_ERROR_NULL(998, sceGxmPrecomputedDrawSetVertexStream, streamData);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
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

	if (precomputedDraw == NULL) {
		GXM_DBG_ERROR_NULL(1026, sceGxmPrecomputedDrawSetParams, precomputedDraw);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (indexData == NULL) {
		GXM_DBG_ERROR_NULL(1030, sceGxmPrecomputedDrawSetParams, indexData);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
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

	if (precomputedDraw == NULL) {
		GXM_DBG_ERROR_NULL(1071, sceGxmPrecomputedDrawSetParamsInstanced, precomputedDraw);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (indexData == NULL) {
		GXM_DBG_ERROR_NULL(1075, sceGxmPrecomputedDrawSetParamsInstanced, indexData);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	vertexProgram = precomputedDraw->vertexProgram;
	gxmChunkBufferInitFixed(&buffer, &precomputedDraw->primitive, 0, sizeof(GxmVdmPrimitive));
	return gxmVdmWritePrimitive(&buffer, &precomputedDraw->streamBlock,
				    (const SceUInt8 *)&vertexProgram->dms, primType, indexType,
				    (SceUInt32)indexData, indexCount, indexWrap, 1);
}
