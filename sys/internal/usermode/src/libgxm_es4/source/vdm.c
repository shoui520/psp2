/*
 * SceGxm: VDM stream commands.
 *
 * Draws and state changes reach the GPU as VDM commands written into the
 * context's VDM chunk buffer; the words are hardware encodings.
 */

#include "gxm_internal.h"

#define SCE_GXM_ATTRIBUTE_FORMAT_U8	0
#define SCE_GXM_ATTRIBUTE_FORMAT_S8	1
#define SCE_GXM_ATTRIBUTE_FORMAT_U16	2
#define SCE_GXM_ATTRIBUTE_FORMAT_S16	3
#define SCE_GXM_ATTRIBUTE_FORMAT_U8N	4
#define SCE_GXM_ATTRIBUTE_FORMAT_S8N	5
#define SCE_GXM_ATTRIBUTE_FORMAT_U16N	6
#define SCE_GXM_ATTRIBUTE_FORMAT_S16N	7
#define SCE_GXM_ATTRIBUTE_FORMAT_F16	8
#define SCE_GXM_ATTRIBUTE_FORMAT_F32	9
#define SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED	10

/* bytes per component */
uint32_t gxmAttributeFormatGetSize(SceUInt8 format)
{
	switch (format) {
	case SCE_GXM_ATTRIBUTE_FORMAT_U8:
	case SCE_GXM_ATTRIBUTE_FORMAT_S8:
	case SCE_GXM_ATTRIBUTE_FORMAT_U8N:
	case SCE_GXM_ATTRIBUTE_FORMAT_S8N:
		return 1;
	case SCE_GXM_ATTRIBUTE_FORMAT_U16:
	case SCE_GXM_ATTRIBUTE_FORMAT_S16:
	case SCE_GXM_ATTRIBUTE_FORMAT_U16N:
	case SCE_GXM_ATTRIBUTE_FORMAT_S16N:
	case SCE_GXM_ATTRIBUTE_FORMAT_F16:
		return 2;
	case SCE_GXM_ATTRIBUTE_FORMAT_F32:
	case SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED:
		return 4;
	default:
		return 0;
	}
}

#define GXM_VDM_STATE		0x40000000
#define GXM_VDM_FENCE		0x60000000
#define GXM_VDM_PRIMITIVE	0x81c00000
#define GXM_VDM_SECONDARY	0x04000000	/* the state is for the secondary program */

/* the block's data size in 16-byte units, at bit 27 */
static inline SceUInt32 gxmVdmBlockSize(const GxmCommandBlock *block)
{
	return (SceUInt32)(block->dataSize >> 4) << 27;
}

int gxmVdmWriteVertexState(GxmChunkBuffer *buffer, SceSize size, const GxmCommandBlock *block,
			   SceBool secondary)
{
	SceUInt32 address = (SceUInt32)block->data;
	SceUInt32 *words = gxmChunkBufferReserve(buffer, 8, 4);

	if (words == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	words[0] = address >> 4 | GXM_VDM_STATE;
	words[1] = gxmVdmBlockSize(block) | 0x02022200 | (size + 3) >> 2 |
		   (secondary ? GXM_VDM_SECONDARY : 0);
	buffer->cur = (SceUInt32)(words + 2);
	return 0;
}

int gxmVdmWriteFragmentState(GxmChunkBuffer *buffer, const GxmCommandBlock *block, SceSize size,
			     SceBool secondary)
{
	SceUInt32 address = (SceUInt32)block->data;
	SceUInt32 *words = gxmChunkBufferReserve(buffer, 8, 4);

	if (words == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	words[0] = address >> 4 | GXM_VDM_STATE;
	words[1] = gxmVdmBlockSize(block) | 0xe100 | (size + 3) >> 2 |
		   (secondary ? GXM_VDM_SECONDARY : 0);
	buffer->cur = (SceUInt32)(words + 2);
	return 0;
}

int gxmVdmWritePrimitive(GxmChunkBuffer *buffer, const GxmCommandBlock *block,
			 const SceUInt8 *programInfo, SceUInt32 primitive, SceUInt32 indexWord,
			 SceUInt32 count, SceUInt32 flags, SceUInt32 indexAddress, SceUInt8 wordBits)
{
	SceUInt32 *words = gxmChunkBufferReserve(buffer, 0x18, 4);

	if (words == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	words[0] = primitive | GXM_VDM_PRIMITIVE | flags;
	words[1] = wordBits | count;
	words[3] = indexAddress & 0x3fffff;
	words[2] = indexWord | (programInfo[0] - 1) << 28;
	words[4] = (SceUInt32)block->data >> 4 | (programInfo[1] - 1) << 28;
	words[5] = *(const SceUInt32 *)(programInfo + 4) | 0x100 | (block->dataSize >> 4);
	buffer->cur = (SceUInt32)(words + 6);
	return 0;
}

int gxmVdmWriteFence(GxmChunkBuffer *buffer, const GxmCommandBlock *block)
{
	SceUInt32 address = (SceUInt32)block->data;
	SceUInt32 *words = gxmChunkBufferReserve(buffer, 0xc, 4);

	if (words == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	words[0] = address >> 4 | GXM_VDM_FENCE;
	words[2] = 0xc0000000;
	words[1] = gxmVdmBlockSize(block) | 0x02022201;
	buffer->cur = (SceUInt32)(words + 3);
	return 0;
}

int gxmVdmWriteTerminate(GxmChunkBuffer *buffer, const GxmCommandBlock *block)
{
	SceUInt32 *words = gxmChunkBufferReserve(buffer, 8, 4);

	if (words == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	words[0] = (SceUInt32)block->data >> 4 | GXM_VDM_STATE;
	words[1] = gxmVdmBlockSize(block) | 0x0204e001;
	buffer->cur = (SceUInt32)(words + 2);
	return 0;
}

/* a jump to another VDM stream, with the words it returns with */
int gxmVdmWriteLink(GxmChunkBuffer *buffer, SceUInt32 address, const SceUInt32 *returnWords)
{
	SceSize size = returnWords == NULL ? 4 : 0xc;
	SceUInt32 *words = gxmChunkBufferReserve(buffer, size, 4);

	if (words == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	words[0] = address >> 2;
	if (returnWords != NULL) {
		words[1] = returnWords[0];
		words[2] = returnWords[1];
	}
	buffer->cur = (SceUInt32)words + size;
	return 0;
}
