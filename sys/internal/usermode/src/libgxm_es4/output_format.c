/*
 * SceGxm: fragment output register formats.
 *
 * A fragment program can declare its output in one format and have the
 * patcher convert it to the one the colour surface needs; the conversion
 * is a short USSE sequence after the program.
 */

#include "gxm_internal.h"

#define SCE_GXM_OUTPUT_REGISTER_FORMAT_DECLARED	0
#define SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4	1
#define SCE_GXM_OUTPUT_REGISTER_FORMAT_CHAR4	2
#define SCE_GXM_OUTPUT_REGISTER_FORMAT_USHORT2	3
#define SCE_GXM_OUTPUT_REGISTER_FORMAT_SHORT2	4
#define SCE_GXM_OUTPUT_REGISTER_FORMAT_HALF4	5
#define SCE_GXM_OUTPUT_REGISTER_FORMAT_HALF2	6
#define SCE_GXM_OUTPUT_REGISTER_FORMAT_FLOAT2	7
#define SCE_GXM_OUTPUT_REGISTER_FORMAT_FLOAT	8

typedef struct GxmOutputFormatInfo {
	SceUInt8 type;			/* SCE_GXM_PARAMETER_TYPE_* */
	SceUInt8 componentCount;
} GxmOutputFormatInfo;

static const GxmOutputFormatInfo gxmOutputFormats[12] = {
	[SCE_GXM_OUTPUT_REGISTER_FORMAT_DECLARED] = { SCE_GXM_PARAMETER_TYPE_AGGREGATE, 0 },
	[SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4] = { 7, 4 },
	[SCE_GXM_OUTPUT_REGISTER_FORMAT_CHAR4] = { 8, 4 },
	[SCE_GXM_OUTPUT_REGISTER_FORMAT_USHORT2] = { 5, 2 },
	[SCE_GXM_OUTPUT_REGISTER_FORMAT_SHORT2] = { 6, 2 },
	[SCE_GXM_OUTPUT_REGISTER_FORMAT_HALF4] = { 1, 4 },
	[SCE_GXM_OUTPUT_REGISTER_FORMAT_HALF2] = { 1, 2 },
	[SCE_GXM_OUTPUT_REGISTER_FORMAT_FLOAT2] = { 0, 2 },
	[SCE_GXM_OUTPUT_REGISTER_FORMAT_FLOAT] = { 0, 1 },
};

/* bits per component, by parameter type */
static const SceUInt8 gxmParameterTypeBits[12] = {
	32, 16, 10, 32, 32, 16, 16, 8, 8,
};

/* a 64-bit output register */
SceBool gxmOutputIsWide(SceUInt32 type, SceUInt32 componentCount)
{
	return componentCount * gxmParameterTypeBits[type] == 64;
}

/*
 * Replace the declared output type and count with the requested format's.
 * *mode is 1 when the output fits as it is and 2 when a half4 output
 * must be converted.
 */
int gxmOutputFormatResolve(SceUInt32 format, SceUInt32 *type, SceUInt32 *componentCount,
			   SceUInt32 *mode)
{
	SceUInt32 oldType, oldCount, newType, newCount;

	if (format == SCE_GXM_OUTPUT_REGISTER_FORMAT_DECLARED) {
		*mode = 1;
		return 0;
	}
	oldType = *type;
	oldCount = *componentCount;
	newType = gxmOutputFormats[format].type;
	newCount = gxmOutputFormats[format].componentCount;
	*type = newType;
	*componentCount = newCount;
	if (newCount <= oldCount && newType == oldType) {
		*mode = 1;
		return 0;
	}
	if (oldCount == 4 && oldType == SCE_GXM_PARAMETER_TYPE_F16) {
		*mode = 2;
		return 0;
	}
	return SCE_GXM_ERROR_INVALID_VALUE;
}

/* the conversion pack instructions, by format; the secondary variants differ by one */
#define GXM_PACK(n)	(0x40a00000 | (n))

SceUInt32 *gxmOutputFormatWriteConversion(SceUInt32 *code, SceUInt32 reg, SceUInt32 format,
					  SceUInt32 *type, SceUInt32 *componentCount,
					  SceUInt8 secondary)
{
	SceUInt32 oldType, oldCount, newType, newCount, head, delta;

	if (format != SCE_GXM_OUTPUT_REGISTER_FORMAT_DECLARED) {
		oldType = *type;
		oldCount = *componentCount;
		newType = gxmOutputFormats[format].type;
		newCount = gxmOutputFormats[format].componentCount;
		*type = newType;
		*componentCount = newCount;
		if (!(newCount <= oldCount && newType == oldType)) {
			if (!secondary) {
				delta = 0;
				head = 0;
			} else {
				delta = 1;
				head = reg << 21;
			}
			head |= reg << 7;
			switch (format) {
			case SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4:
				code[0] = head | 0xa01d8000;
				code[1] = GXM_PACK(0xa3d + delta);
				return code + 2;
			case SCE_GXM_OUTPUT_REGISTER_FORMAT_CHAR4:
				code[0] = head | 0xa01d8000;
				code[2] = head | 0xa01f8002;
				code[1] = GXM_PACK(0xa4d + delta);
				code[3] = GXM_PACK(0xa71 + delta);
				return code + 4;
			case SCE_GXM_OUTPUT_REGISTER_FORMAT_USHORT2:
				code[0] = head | 0xa01d8000;
				code[1] = GXM_PACK(0xacd + delta);
				return code + 2;
			case SCE_GXM_OUTPUT_REGISTER_FORMAT_SHORT2:
				code[0] = head | 0xa01d8000;
				code[1] = GXM_PACK(0xb0d + delta);
				return code + 2;
			case SCE_GXM_OUTPUT_REGISTER_FORMAT_FLOAT2:
				code[0] = head | 0xa0198000;
				code[1] = GXM_PACK(0xb8d + delta);
				return code + 2;
			case SCE_GXM_OUTPUT_REGISTER_FORMAT_FLOAT:
				code[0] = head | 0xa0198000;
				code[1] = GXM_PACK(0xb85 + delta);
				return code + 2;
			}
			return code;
		}
	}
	if (secondary)
		return code;
	return gxmUsseWriteOutputMove(code, reg,
				      gxmOutputIsWide(*type, *componentCount));
}

/* copy whole words */
void gxmCopyWords(SceUInt32 *dest, const SceUInt32 *src, SceSize size)
{
	for (; size != 0; size -= 4)
		*dest++ = *src++;
}

/* the PDS state words of a program pair */
void gxmPdsWriteStateWords(SceUInt32 *words, const GxmCommandBlock *vertex, SceUInt32 flags,
			   const GxmCommandBlock *fragment)
{
	SceUInt32 vertexAddress = (SceUInt32)vertex->data;
	SceUInt32 fragmentAddress = (SceUInt32)fragment->data;

	words[0] = (vertexAddress << 1) >> 5 | (SceUInt32)(vertex->dataSize >> 4) << 27;
	words[1] = ((SceInt32)vertexAddress < 0 ? 0x4000 : 0) | flags |
		   ((SceInt32)fragmentAddress < 0 ? 0x2000 : 0);
	words[2] = (fragmentAddress << 1) >> 5 | (SceUInt32)(fragment->dataSize >> 4) << 27;
}

/* copy a command block into the chunk buffer */
int gxmCommandBlockCopy(GxmChunkBuffer *buffer, const GxmCommandBlock *source, GxmCommandBlock *copy)
{
	SceSize size = source->size;
	SceUInt32 *data = gxmChunkBufferReserve(buffer, size, 0x10);

	if (data == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	buffer->cur = (SceUInt32)data + size;
	copy->data = data;
	copy->size = size;
	copy->dataSize = source->dataSize;
	gxmCopyWords(data, source->data, size);
	return 0;
}
