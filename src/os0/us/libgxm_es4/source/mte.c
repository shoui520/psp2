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

int gxmMteWriteUpdateBlock(GxmChunkBuffer *buffer, SceUInt32 stateWords, SceUInt32 count,
			   const GxmCodeRef *copyProgram, GxmCommandBlock *block)
{
	SceUInt32 *words = gxmChunkBufferReserve(buffer, 0x2c, 0x10);

	if (words == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	buffer->cur = (SceUInt32)(words + 11);
	block->dataSize = 0x20;
	block->data = words;
	block->size = 0x2c;
	words[0] = stateWords;
	words[1] = count - 1;
	words[8] = GXM_MTE_UPDATE_WORD8;
	words[9] = GXM_MTE_UPDATE_WORD9;
	words[2] = copyProgram->address;
	words[4] = 0;
	words[10] = GXM_MTE_UPDATE_WORD10;
	words[3] = copyProgram->taskControl;
	return 0;
}

typedef struct GxmPdsParams {
	SceUInt32 address;		/* out: the aligned program address */
	SceUInt16 dataSize;		/* out: the PDS data, in bytes */
	SceUInt16 reserved;		/* out: the size's upper half */
	GxmCodeRef program;
	SceUInt32 dataWord2;		/* the program's data word 2; always 0 */
	SceUInt32 tiles;		/* 32-pixel tile rectangle */
} GxmPdsParams;

int gxmMteWriteRegionBlock(GxmChunkBuffer *buffer, const GxmCodeRef *program, SceUInt32 xMin,
			   SceUInt32 yMin, SceUInt32 xMax, SceUInt32 yMax, GxmCommandBlock *block)
{
	GxmPdsParams params;
	void *code;
	SceUInt32 end;

	params.program = *program;
	params.tiles = yMax >> 5 | (yMin >> 5) << 8 | (xMax >> 5) << 16 | (xMin >> 5) << 24;
	params.dataWord2 = 0;
	code = gxmChunkBufferReserve(buffer, 0x80, 0x10);
	if (code == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	block->data = code;
	end = SceGpuEs4User_A5E331FA(&params, code);
	block->size = end - (SceUInt32)block->data;
	block->dataSize = params.dataSize;
	buffer->cur = end;
	return 0;
}

/* the dirty mask, then each dirty group of state words */
int gxmMteWriteStateWords(GxmChunkBuffer *buffer, const GxmMteState *state, SceUInt32 **words,
			  SceUInt32 *count)
{
	SceUInt32 *start = gxmChunkBufferReserve(buffer, 100, 4);
	SceUInt32 *out, dirty;

	if (start == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	dirty = state->dirty;
	start[0] = dirty;
	out = start + 1;
	if (dirty & GXM_DIRTY_FRONT_ISP)
		*out++ = state->frontIsp;
	if (dirty & GXM_DIRTY_FRONT_BIAS)
		*out++ = state->frontBias;
	if (dirty & GXM_DIRTY_FRONT_STENCIL)
		*out++ = state->frontStencil;
	if (dirty & GXM_DIRTY_BACK_ISP)
		*out++ = state->backIsp;
	if (dirty & GXM_DIRTY_BACK_BIAS)
		*out++ = state->backBias;
	if (dirty & GXM_DIRTY_BACK_STENCIL)
		*out++ = state->backStencil;
	if (dirty & GXM_DIRTY_PDS_STATE) {
		*out++ = state->pdsState[0];
		*out++ = state->pdsState[1];
		*out++ = state->pdsState[2];
	}
	if (dirty & GXM_DIRTY_REGION_CLIP) {
		*out++ = state->regionClip[0];
		*out++ = state->regionClip[1];
	}
	if (dirty & GXM_DIRTY_VIEWPORT) {
		*out++ = state->viewport[0];
		*out++ = state->viewport[1];
		*out++ = state->viewport[2];
		*out++ = state->viewport[3];
		*out++ = state->viewport[4];
		*out++ = state->viewport[5];
	}
	if (dirty & GXM_DIRTY_RESERVED0)
		*out++ = state->reserved0;
	if (dirty & GXM_DIRTY_VERTEX_OUTPUTS)
		*out++ = state->vertexOutputs;
	if (dirty & GXM_DIRTY_W_CLAMP_VALUE)
		*out++ = state->wClampValue;
	if (dirty & GXM_DIRTY_CULL)
		*out++ = state->cull;
	if (dirty & GXM_DIRTY_RESERVED1)
		*out++ = state->reserved1;
	if (dirty & GXM_DIRTY_TEXCOORD_OUTPUTS)
		*out++ = state->texCoordOutputs;
	if (dirty & GXM_DIRTY_VERTEX_OUTPUTS2)
		*out++ = state->vertexOutputs2;
	buffer->cur = (SceUInt32)out;
	*words = start;
	*count = out - start;
	return 0;
}

#define GXM_COPY_PROGRAM_COUNT	25

SceSize gxmMteGetCopyProgramsSize(void)
{
	return 0x640;
}

/* program n copies n registers, at most 16 per instruction */
int gxmMteBuildCopyPrograms(GxmCodeRef *programs, GxmChunkBuffer *buffer)
{
	SceUInt32 *code, *out, count, done, step;
	SceUInt32 offset;

	count = 0;
	do {
		code = gxmChunkBufferReserve(buffer, (((count + 16) >> 4) + 6) * 8, 0x10);
		count++;
		if (code == NULL)
			return SCE_GXM_ERROR_RESERVE_FAILED;
		offset = buffer->cur + buffer->gpuEnd - buffer->end;
		code[0] = 0;
		code[1] = 0xfa440700;
		out = gxmUsseWriteHeader(code + 2);
		done = 0;
		do {
			step = count - done;
			if (step >= 16)
				step = 16;
			out[0] = done << 7 | done << 21 | 0xa0000000;
			out[1] = (step - 1) << 12 | 0x50a10009;
			out += 2;
			done += step;
		} while (done < count);
		out[0] = 0xa0200000;
		out[1] = 0xfb274000;
		buffer->cur = (SceUInt32)(out + 2);
		programs->address = 0;
		programs->taskControl = 0;
		gxmCodeRefSetAddress(programs, offset);
		programs++;
	} while (count != GXM_COPY_PROGRAM_COUNT);
	return 0;
}

const GxmCodeRef *gxmMteGetCopyProgram(const GxmCodeRef *programs, SceUInt32 count)
{
	return &programs[count - 1];
}

#define GXM_DIRTY_ALL		0x0000dfff

void gxmMteStateInvalidate(GxmMteState *state)
{
	state->dirty = GXM_DIRTY_ALL;
}

void gxmMteStateInit(GxmMteState *state)
{
	state->dirty = GXM_DIRTY_ALL;
	state->wClampValue = 0x3727c5ac;		/* 1e-5f */
	state->cull = GXM_CULL_W_CLAMP;
	state->frontIsp = 0x00c00200;
	state->backIsp = 0x00c00200;
	state->frontBias = 0;
	state->backBias = 0;
	state->regionClip[0] = 0;
	state->regionClip[1] = 0;
	state->viewport[0] = 0;
	state->viewport[1] = 0;
	state->viewport[2] = 0;
	state->viewport[3] = 0;
	state->viewport[4] = 0;
	state->viewport[5] = 0;
	state->reserved0 = 0;
	state->vertexOutputs = 0;
	state->texCoordOutputs = 0;
	state->vertexOutputs2 = 0;
	state->frontStencil = 0x0e000000;
	state->backStencil = 0x0e000000;
}

#define GXM_SLOT_RECORD_SIZE	0x10

int gxmSlotTableCreate(GxmSlotTable *table)
{
	PVRSRV_CLIENT_MEM_INFO *block;

	if (SceGpuEs4User_071E62F0(&gxmGlobals.top, gxmGlobals.top.slotHeap, 0x40000f, 0x400, 4,
				   gxmGlobals.top.hMemBlockProcRef, &block) != 0)
		return SCE_GXM_ERROR_DRIVER;
	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC)
		gxmInitCallbacks.memoryMapped(0, block->pvLinAddr, block->sDevVAddr, block->uAllocSize);
	table->used[0] = 0;
	table->used[1] = 0;
	table->block = block;
	return 0;
}

void *gxmSlotTableAdd(GxmSlotTable *table, void *item)
{
	SceUInt32 slot, bit;

	for (slot = 0; slot < 64; slot++) {
		bit = 1 << (slot & 31);
		if (!(table->used[slot >> 5] & bit)) {
			table->used[slot >> 5] |= bit;
			table->items[slot] = item;
			return (char *)table->block->pvLinAddr + slot * GXM_SLOT_RECORD_SIZE;
		}
	}
	return NULL;
}

void gxmSlotTableRemove(GxmSlotTable *table, void *record)
{
	SceUInt32 position = (SceUInt32)record - (SceUInt32)table->block->pvLinAddr;
	SceUInt32 slot = position >> 4;

	if (slot > 63)
		return;
	table->used[position >> 9] &= ~(1 << (slot & 31));
}

void gxmSlotTableForEach(GxmSlotTable *table, void (*callback)(void *item))
{
	SceUInt32 slot;

	for (slot = 0; slot < 64; slot++) {
		if (table->used[slot >> 5] & (1 << (slot & 31)))
			callback(table->items[slot]);
	}
}

int gxmSlotTableDestroy(GxmSlotTable *table)
{
	PVRSRV_CLIENT_MEM_INFO *block = table->block;

	if (gxmInitCallbacksMagic == GXM_CALLBACKS_MAGIC)
		gxmInitCallbacks.memoryUnmapped(0, block->pvLinAddr);
	if (SceGpuEs4User_B6CFD213(&gxmGlobals.top, block) != 0)
		return SCE_GXM_ERROR_DRIVER;
	return 0;
}

/* sleep on the GPU signal until the word holds the value */
void gxmWaitForValue(GxmSlotTable *table, const volatile SceUInt32 *address, SceUInt32 value)
{
	while (*address != value) {
		sceKernelGetTLSAddr(0x44);
		sceGpuSignalWait(sceKernelGetTLSAddr(0x44), 100000);
	}
}
