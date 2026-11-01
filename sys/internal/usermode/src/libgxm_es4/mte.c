/*
 * SceGxm: MTE state blocks.
 *
 * The render state goes to the GPU's MTE as command blocks in the VDM
 * stream: a dirty mask followed by the dirty state words, the PDS program
 * that loads them, and the small USSE programs that copy registers.
 */

#include "gxm_internal.h"

/* PDS/USSE control words of the state update block (encodings undocumented) */
#define GXM_MTE_UPDATE_WORD8	0x07000103
#define GXM_MTE_UPDATE_WORD9	0x07040b25
#define GXM_MTE_UPDATE_WORD10	0xaf000000

/* the most state words one update writes: the copy programs come in this many sizes */
#define SCE_GXM_MAX_MTE_STATE_DWORD_COUNT	25

/* the words an update with this dirty mask writes, the mask included (debug checks only) */
static inline GXM_DBG_NOINLINE SceUInt32 sceGxmGetMteStateUpdateDwordCount(SceUInt32 header)
{
	return 1 +
	       (header & 1) +			/* GXM_DIRTY_FRONT_ISP */
	       (header >> 1 & 1) +		/* GXM_DIRTY_FRONT_BIAS */
	       (header >> 2 & 1) +		/* GXM_DIRTY_FRONT_STENCIL */
	       (header >> 3 & 1) +		/* GXM_DIRTY_BACK_ISP */
	       (header >> 4 & 1) +		/* GXM_DIRTY_BACK_BIAS */
	       (header >> 5 & 1) +		/* GXM_DIRTY_BACK_STENCIL */
	       (header >> 6 & 1) * 3 +		/* GXM_DIRTY_PDS_STATE */
	       (header >> 7 & 1) * 2 +		/* GXM_DIRTY_REGION_CLIP */
	       (header >> 8 & 1) * 6 +		/* GXM_DIRTY_VIEWPORT */
	       (header >> 9 & 1) +		/* GXM_DIRTY_RESERVED0 */
	       (header >> 10 & 1) +		/* GXM_DIRTY_VERTEX_OUTPUTS */
	       (header >> 11 & 1) +		/* GXM_DIRTY_W_CLAMP_VALUE */
	       (header >> 12 & 1) +		/* GXM_DIRTY_CULL */
	       (header >> 13 & 1) +		/* GXM_DIRTY_RESERVED1 */
	       (header >> 14 & 1) +		/* GXM_DIRTY_TEXCOORD_OUTPUTS */
	       (header >> 15 & 1);		/* GXM_DIRTY_VERTEX_OUTPUTS2 */
}

/* the copy program for count registers, at most 16 per instruction */
static inline GXM_DBG_NOINLINE int gxmMteBuildCopyProgram(GxmChunkBuffer *buffer, SceUInt32 count,
							  GxmCodeRef *program)
{
	SceSize size = (((count + 15) >> 4) + 6) * 8;
	SceUInt32 *code, *out, done, step;
	SceUInt32 offset;

	code = gxmChunkBufferReserve(buffer, size, 0x10);
	if (code == NULL) {
		GXM_DBG_ERROR(97, "Could not reserve %d bytes.", size);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	offset = buffer->current + buffer->gpuEnd - buffer->end;
	code[0] = 0;
	code[1] = 0xfa440700;
	out = gxmUsseWriteHeader(code + 2);
	done = 0;
	while (done < count) {
		step = count - done < 16 ? count - done : 16;
		out[0] = done << 21 | done << 7 | 0xa0000000;
		out[1] = (step - 1) << 12 | 0x50a10009;
		out += 2;
		done += step;
	}
	out[0] = 0xa0200000;
	out[1] = 0xfb274000;
	gxmChunkBufferCommit(buffer, out + 2);
	program->address = 0;
	program->taskControl = 0;
	gxmCodeRefSetAddress(program, offset);
	return 0;
}

int gxmMteWriteUpdateBlock(GxmChunkBuffer *buffer, SceUInt32 mteDataGpuAddr, SceUInt32 wordCount,
			   const GxmCodeRef *copyProgram, GxmCommandBlock *block)
{
	SceUInt32 *words;

	GXM_DBG_ASSERT(192, wordCount != 0);
	GXM_DBG_ASSERT(193, mteDataGpuAddr);
	words = gxmChunkBufferReserve(buffer, 0x2c, 0x10);
	if (words == NULL) {
		GXM_DBG_ERROR(216, "Could not reserve %d bytes.", 0x2c);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	gxmChunkBufferCommit(buffer, words + 11);
	block->dataSize = 0x20;
	block->totalSize = 0x2c;
	block->data = words;
	words[0] = mteDataGpuAddr;
	words[1] = wordCount - 1;
	words[8] = GXM_MTE_UPDATE_WORD8;
	words[9] = GXM_MTE_UPDATE_WORD9;
	words[2] = copyProgram->address;
	words[3] = copyProgram->taskControl;
	words[4] = 0;
	words[10] = GXM_MTE_UPDATE_WORD10;
	return 0;
}

typedef struct GxmPdsParams {
	SceUInt32 address;		/* out: the aligned program address */
	SceUInt32 dataSize;		/* out: the PDS data, in bytes */
	GxmCodeRef program;
	SceUInt32 dataWord2;		/* the program's data word 2; always 0 */
	SceUInt32 tiles;		/* 32-pixel tile rectangle */
} GxmPdsParams;

int gxmMteWriteRegionBlock(GxmChunkBuffer *buffer, const GxmCodeRef *program, SceUInt32 xMin,
			   SceUInt32 yMin, SceUInt32 xMax, SceUInt32 yMax,
			   GxmCommandBlock *statePdsProgram)
{
	const SceSize reserveSize = 0x80;
	GxmPdsParams params;
	void *code;
	SceUInt32 end;

	params.program = *program;
	params.tiles = yMax >> 5 | (yMin >> 5) << 8 | (xMax >> 5) << 16 | (xMin >> 5) << 24;
	params.dataWord2 = 0;
	code = gxmChunkBufferReserve(buffer, reserveSize, 0x10);
	if (code == NULL) {
		GXM_DBG_ERROR(308, "Could not reserve %d bytes.", reserveSize);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	statePdsProgram->data = code;
	end = SceGpuEs4User_A5E331FA(&params, code);
	statePdsProgram->totalSize = end - (SceUInt32)statePdsProgram->data;
	statePdsProgram->dataSize = params.dataSize;
	GXM_DBG_ASSERT(317, statePdsProgram->totalSize <= reserveSize);
	gxmChunkBufferCommit(buffer, (void *)end);
	return 0;
}

/* the dirty mask, then each dirty group of state words */
int gxmMteWriteStateWords(GxmChunkBuffer *buffer, const GxmMteState *state, SceUInt32 **words,
			  SceUInt32 *count)
{
	SceUInt32 *start = gxmChunkBufferReserve(buffer, 100, 4);
	SceUInt32 *out, header, writtenCount;

	if (start == NULL) {
		GXM_DBG_ERROR(341, "Could not reserve %d bytes.", 100);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	header = state->dirty;
	start[0] = header;
	out = start + 1;
	if (header & GXM_DIRTY_FRONT_ISP)
		*out++ = state->frontIsp;
	if (header & GXM_DIRTY_FRONT_BIAS)
		*out++ = state->frontBias;
	if (header & GXM_DIRTY_FRONT_STENCIL)
		*out++ = state->frontStencil;
	if (header & GXM_DIRTY_BACK_ISP)
		*out++ = state->backIsp;
	if (header & GXM_DIRTY_BACK_BIAS)
		*out++ = state->backBias;
	if (header & GXM_DIRTY_BACK_STENCIL)
		*out++ = state->backStencil;
	if (header & GXM_DIRTY_PDS_STATE) {
		*out++ = state->pdsState[0];
		*out++ = state->pdsState[1];
		*out++ = state->pdsState[2];
	}
	if (header & GXM_DIRTY_REGION_CLIP) {
		*out++ = state->regionClip[0];
		*out++ = state->regionClip[1];
	}
	if (header & GXM_DIRTY_VIEWPORT) {
		*out++ = state->viewport[0];
		*out++ = state->viewport[1];
		*out++ = state->viewport[2];
		*out++ = state->viewport[3];
		*out++ = state->viewport[4];
		*out++ = state->viewport[5];
	}
	if (header & GXM_DIRTY_RESERVED0)
		*out++ = state->reserved0;
	if (header & GXM_DIRTY_VERTEX_OUTPUTS)
		*out++ = state->vertexOutputs;
	if (header & GXM_DIRTY_W_CLAMP_VALUE)
		*out++ = state->wClampValue;
	if (header & GXM_DIRTY_CULL)
		*out++ = state->cull;
	if (header & GXM_DIRTY_RESERVED1)
		*out++ = state->reserved1;
	if (header & GXM_DIRTY_TEXCOORD_OUTPUTS)
		*out++ = state->texCoordOutputs;
	if (header & GXM_DIRTY_VERTEX_OUTPUTS2)
		*out++ = state->vertexOutputs2;
	writtenCount = out - start;
	gxmChunkBufferCommit(buffer, out);
	GXM_DBG_ASSERT(436, sceGxmGetMteStateUpdateDwordCount(header) == writtenCount);
	*words = start;
	*count = writtenCount;
	return 0;
}

SceSize gxmMteGetCopyProgramsSize(void)
{
	return 0x640;
}

/* program n copies n registers */
int gxmMteBuildCopyPrograms(GxmCodeRef *programs, GxmChunkBuffer *buffer)
{
	SceUInt32 count = 0;
	int err;

	do {
		err = gxmMteBuildCopyProgram(buffer, ++count, programs);
		programs++;
		if (err != 0)
			return err;
	} while (count != SCE_GXM_MAX_MTE_STATE_DWORD_COUNT);
	return 0;
}

const GxmCodeRef *gxmMteGetCopyProgram(const GxmCodeRef *programs, SceUInt32 wordCount)
{
	GXM_DBG_ASSERT(478, 1 <= wordCount && wordCount <= SCE_GXM_MAX_MTE_STATE_DWORD_COUNT);
	return &programs[wordCount - 1];
}

#define GXM_DIRTY_ALL		0x0000dfff

void gxmMteStateInvalidate(GxmMteState *mteState)
{
	GXM_DBG_ASSERT(485, mteState);
	mteState->dirty = GXM_DIRTY_ALL;
}

void gxmMteStateInit(GxmMteState *mteState)
{
	GXM_DBG_ASSERT(507, mteState);
	gxmMteStateInvalidate(mteState);
	mteState->wClampValue = 0x3727c5ac;		/* 1e-5f */
	mteState->cull = GXM_CULL_W_CLAMP;
	mteState->frontIsp = 0x00c00200;
	mteState->backIsp = 0x00c00200;
	mteState->frontBias = 0;
	mteState->backBias = 0;
	mteState->regionClip[0] = 0;
	mteState->regionClip[1] = 0;
	mteState->viewport[0] = 0;
	mteState->viewport[1] = 0;
	mteState->viewport[2] = 0;
	mteState->viewport[3] = 0;
	mteState->viewport[4] = 0;
	mteState->viewport[5] = 0;
	mteState->reserved0 = 0;
	mteState->vertexOutputs = 0;
	mteState->texCoordOutputs = 0;
	mteState->vertexOutputs2 = 0;
	mteState->frontStencil = 0x0e000000;
	mteState->backStencil = 0x0e000000;
}
