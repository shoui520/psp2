/*
 * SceGxm: VDM stream commands.
 *
 * Draws and state changes reach the GPU as VDM commands written into the
 * context's VDM chunk buffer; the words are hardware encodings.
 */

#include "gxm_internal.h"

#define GXM_VDM_STATE		0x40000000
#define GXM_VDM_FENCE		0x60000000
#define GXM_VDM_PRIMITIVE	0x81c00000
#define GXM_VDM_SECONDARY	0x04000000	/* the state is for the secondary program */

/* the block's data size in 16-byte units, at bit 27 */
static inline SceUInt32 gxmVdmBlockSize(const GxmCommandBlock *block)
{
	return (SceUInt32)(block->dataSize >> 4) << 27;
}

int gxmVdmWriteFragmentState(GxmChunkBuffer *buffer, SceSize size,
			     const GxmCommandBlock *statePdsProgram, SceBool secondary)
{
	SceUInt32 address, *words;

	GXM_DBG_ASSERT(20, buffer);
	GXM_DBG_ASSERT(21, statePdsProgram);
	GXM_DBG_ASSERT(22, statePdsProgram->data);
	address = (SceUInt32)statePdsProgram->data;
	words = gxmChunkBufferReserve(buffer, 8, 4);
	if (words == NULL) {
		GXM_DBG_ERROR(38, "Could not reserve %d bytes.", 8);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	words[0] = address >> 4 | GXM_VDM_STATE;
	words[1] = gxmVdmBlockSize(statePdsProgram) | 0x02022200 | (size + 3) >> 2 |
		   (secondary ? GXM_VDM_SECONDARY : 0);
	gxmChunkBufferCommit(buffer, words + 2);
	return 0;
}

int gxmVdmWriteVertexState(GxmChunkBuffer *buffer, const GxmCommandBlock *secondaryPdsProgram,
			   SceSize size, SceBool secondary)
{
	SceUInt32 address, *words;

	GXM_DBG_ASSERT(69, secondaryPdsProgram->data);
	address = (SceUInt32)secondaryPdsProgram->data;
	words = gxmChunkBufferReserve(buffer, 8, 4);
	if (words == NULL) {
		GXM_DBG_ERROR(82, "Could not reserve %d bytes.", 8);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	words[1] = gxmVdmBlockSize(secondaryPdsProgram) | 0xe100 | (size + 3) >> 2 |
		   (secondary ? GXM_VDM_SECONDARY : 0);
	words[0] = address >> 4 | GXM_VDM_STATE;
	gxmChunkBufferCommit(buffer, words + 2);
	return 0;
}

int gxmVdmWritePrimitive(GxmChunkBuffer *buffer, const GxmCommandBlock *block,
			 const SceUInt8 *programInfo, SceUInt32 primitive, SceUInt32 indexWord,
			 SceUInt32 count, SceUInt32 flags, SceUInt32 indexAddress, SceUInt8 wordBits)
{
	SceUInt32 *words = gxmChunkBufferReserve(buffer, 0x18, 4);

	if (words == NULL) {
		GXM_DBG_ERROR(131, "Could not reserve %d bytes.", 0x18);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	words[0] = primitive | GXM_VDM_PRIMITIVE | flags;
	words[1] = wordBits | count;
	words[2] = indexWord | (programInfo[0] - 1) << 28;
	words[3] = indexAddress & 0x3fffff;
	words[4] = (SceUInt32)block->data >> 4 | (programInfo[1] - 1) << 28;
	words[5] = *(const SceUInt32 *)(programInfo + 4) | 0x100 | (block->dataSize >> 4);
	gxmChunkBufferCommit(buffer, words + 6);
	return 0;
}

int gxmVdmWriteFence(GxmChunkBuffer *buffer, const GxmCommandBlock *statePdsProgram)
{
	SceUInt32 address, *words;

	GXM_DBG_ASSERT(181, statePdsProgram->data);
	address = (SceUInt32)statePdsProgram->data;
	words = gxmChunkBufferReserve(buffer, 0xc, 4);
	if (words == NULL) {
		GXM_DBG_ERROR(194, "Could not reserve %d bytes.", 0xc);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	words[0] = address >> 4 | GXM_VDM_FENCE;
	words[1] = gxmVdmBlockSize(statePdsProgram) | 0x02022201;
	words[2] = 0xc0000000;
	gxmChunkBufferCommit(buffer, words + 3);
	return 0;
}

int gxmVdmWriteTerminate(GxmChunkBuffer *buffer, const GxmCommandBlock *vdmFencePdsProg)
{
	SceUInt32 *words;

	GXM_DBG_ASSERT(227, buffer);
	GXM_DBG_ASSERT(228, vdmFencePdsProg);
	GXM_DBG_ASSERT(229, vdmFencePdsProg->data);
	words = gxmChunkBufferReserve(buffer, 8, 4);
	if (words == NULL) {
		GXM_DBG_ERROR(239, "Could not reserve %d bytes.", 8);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	words[0] = (SceUInt32)vdmFencePdsProg->data >> 4 | GXM_VDM_STATE;
	words[1] = gxmVdmBlockSize(vdmFencePdsProg) | 0x0204e001;
	gxmChunkBufferCommit(buffer, words + 2);
	return 0;
}

/* a jump to another VDM stream, with the words it returns with */
int gxmVdmWriteLink(GxmChunkBuffer *buffer, SceUInt32 targetGpuAddr, const SceUInt32 *returnWords)
{
	SceSize size;
	SceUInt32 *words;

	GXM_DBG_ASSERT(263, buffer);
	GXM_DBG_ASSERT(264, targetGpuAddr);
	size = returnWords == NULL ? 4 : 0xc;
	words = gxmChunkBufferReserve(buffer, size, 4);
	if (words == NULL) {
		GXM_DBG_ERROR(279, "Could not reserve %d bytes.", size);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	words[0] = targetGpuAddr >> 2;
	if (returnWords != NULL) {
		words[1] = returnWords[0];
		words[2] = returnWords[1];
	}
	gxmChunkBufferCommit(buffer, (SceUInt8 *)words + size);
	return 0;
}
