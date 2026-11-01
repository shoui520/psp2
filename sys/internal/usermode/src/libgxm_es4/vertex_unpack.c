/*
 * SceGxm: vertex attribute unpacking.
 *
 * Before a vertex program runs, a short USSE sequence converts the
 * attributes the PDS fetched into the registers' float format and fills
 * the components the stream does not supply. The instruction words are
 * hardware encodings.
 */

#include "gxm_internal.h"

/* the unpack instruction's source type, by attribute format */
static inline SceUInt32 gxmVertexUnpackType(SceUInt32 format)
{
	switch (format) {
	case SCE_GXM_ATTRIBUTE_FORMAT_U8:
	case SCE_GXM_ATTRIBUTE_FORMAT_U8N:
		return 0;
	case SCE_GXM_ATTRIBUTE_FORMAT_S8:
	case SCE_GXM_ATTRIBUTE_FORMAT_S8N:
		return 1;
	case SCE_GXM_ATTRIBUTE_FORMAT_U16:
	case SCE_GXM_ATTRIBUTE_FORMAT_U16N:
		return 3;
	case SCE_GXM_ATTRIBUTE_FORMAT_S16:
	case SCE_GXM_ATTRIBUTE_FORMAT_S16N:
		return 4;
	case SCE_GXM_ATTRIBUTE_FORMAT_F16:
		return 5;
	case SCE_GXM_ATTRIBUTE_FORMAT_F32:
		return 6;
	default:
		GXM_DBG_FAIL(31, "unrecognised vertex attribute format");
		return 0;
	}
}

/*
 * The program reads this primary attribute register (Sony's program_private.h). The debug
 * build reads the varyings again after each check; the release build once per attribute.
 */
static inline SceBool gxmRegisterUsed(const SceGxmProgram *program,
				      const GxmProgramVaryings *varyings, SceUInt32 regIndex)
{
	GXM_DBG_ASSERT_AT("program_private.h", 171, regIndex < 4*SCE_GXM_MAX_VERTEX_ATTRIBUTES);
#if SCE_DBG_ASSERTS_ENABLED
	varyings = gxmProgramGetVertexVaryings(program);
#endif
	return (varyings->vertex.inputMask[regIndex >> 5] & (1 << (regIndex & 0x1f))) != 0;
}

static inline SceUInt32 gxmRegisterMask(const SceGxmProgram *program,
					const GxmProgramVaryings *varyings, SceUInt32 reg)
{
	return (gxmRegisterUsed(program, varyings, reg) ? 1 : 0) |
	       (gxmRegisterUsed(program, varyings, reg + 1) ? 2 : 0) |
	       (gxmRegisterUsed(program, varyings, reg + 2) ? 4 : 0) |
	       (gxmRegisterUsed(program, varyings, reg + 3) ? 8 : 0);
}

/* the instruction pairs the unpack code takes, with its header */
SceUInt32 gxmVertexUnpackSize(const SceGxmProgram *program, const SceGxmVertexAttribute *attributes,
			      SceUInt32 attributeCount)
{
	const GxmProgramVaryings *varyings;
	SceUInt32 count, i, format, components, reg, mask, k;

	if (attributeCount == 0) {
		count = 1;
	} else {
		count = 0;
		for (i = 0; i < attributeCount; i++) {
			format = attributes[i].format;
			if (format == SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED)
				continue;
			components = attributes[i].componentCount;
			reg = attributes[i].regIndex;
			GXM_DBG_ASSERT_MSG(57, components - 1 <= 3,
					   "invalid attribute component count %u (attrib %u)",
					   components, i);
			varyings = gxmProgramGetVaryings(program);
			mask = gxmRegisterMask(program, varyings, reg);
			if (format < 9 && ((1 << format) & 0xef)) {
				for (k = reg; k < reg + components; k++) {
					if (gxmRegisterUsed(program, varyings, k))
						count++;
				}
			} else if (format < 9 && ((1 << format) & 0x110)) {
				if (components > 2 && (mask >> 2) != 0)
					count++;
				if ((mask & 3) != 0)
					count++;
			} else if (format != SCE_GXM_ATTRIBUTE_FORMAT_F32) {
				GXM_DBG_ASSERT(99, 0 && "unrecognised attribute format");
			}
			/* defaults for the components the stream lacks */
			mask &= 0xf << components;
			if ((mask >> 2) != 0)
				count++;
			if ((mask & 3) != 0)
				count++;
		}
		if (count == 0)
			count = 1;
	}
	if (gxmGlobals.compatFlags & GXM_COMPAT_USSE_OLD_HEADER)
		return count + 4;
	return count + 9;
}

#define GXM_UNPACK_DEFAULTS	0x38820542
#define GXM_USSE_END		0x00040000

int gxmVertexUnpackBuild(GxmChunkBuffer *buffer, const SceGxmProgram *program,
			 const SceGxmVertexAttribute *attributes, SceUInt32 attributeCount,
			 SceUInt32 *offset, SceUInt32 **codeOut)
{
	const GxmProgramVaryings *varyings;
	SceUInt32 *code, *start, *out, pairs, i, format, components, reg, size, type;
	SceUInt32 mask, low, high, normalize, pack, instruction, position, k, odd, end;

	pairs = gxmVertexUnpackSize(program, attributes, attributeCount);
	if (pairs == 0) {
		*offset = 0;
		*codeOut = NULL;
		return 0;
	}
	code = gxmChunkBufferReserve(buffer, pairs * 8, gxmUsseAlignment());
	if (code == NULL) {
		GXM_DBG_ERROR(181, "Could not reserve %d bytes.", pairs * 8);
		return SCE_GXM_ERROR_RESERVE_FAILED;
	}
	*offset = buffer->current + buffer->gpuEnd - buffer->end;
	*codeOut = code;
	code[0] = 0;
	code[1] = ((program->programFlags >> 1) & 1) << 13 | 0xfa440000;
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_USSE_OLD_HEADER))
		start = gxmUsseWriteHeader(code + 2);
	else
		start = gxmUsseWriteOldHeader(code + 2);

	out = start;
	for (i = 0; i < attributeCount; i++) {
		format = attributes[i].format;
		if (format == SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED)
			continue;
		components = attributes[i].componentCount;
		reg = attributes[i].regIndex;
		size = gxmAttributeFormatGetSize(format);
		type = gxmVertexUnpackType(format);
		varyings = gxmProgramGetVaryings(program);
		mask = gxmRegisterMask(program, varyings, reg);
		low = mask & 3;

		switch (format) {
		case 0:
		case 1:
		case 2:
		case 3:
			normalize = 0;
			goto convert;
		case SCE_GXM_ATTRIBUTE_FORMAT_U8N:
			pack = 0x40000;
			goto unpack;
		case 5:
		case 6:
		case 7:
			normalize = 0x40000;
			goto convert;
		case SCE_GXM_ATTRIBUTE_FORMAT_F16:
			pack = 0;
unpack:
			/* whole registers at once */
			high = mask >> 2;
			if (components > 2 && high != 0) {
				*out++ = reg << 7 | 0xa0030002 | (reg + 2) << 21 | pack;
				*out++ = type << 9 | 0x40a00182 | high << 2;
			}
			if (low != 0) {
				*out++ = reg << 7 | reg << 21 | 0xa0010000 | pack;
				*out++ = type << 9 | 0x40a00182 | low << 2;
			}
			goto defaults;
		case SCE_GXM_ATTRIBUTE_FORMAT_F32:
			goto defaults;
		default:
			GXM_DBG_ASSERT(367, 0 && "unrecognised attribute format");
			goto defaults;
		}

convert:
		/* component by component, from the last */
		if (components != 0) {
			instruction = type << 9 | 0x40a00186;
			position = (components - 1) * size;
			if ((format & ~4) - 2 < 2) {
				for (k = components; k-- > 0; position -= size) {
					odd = k & 1;
					if (gxmRegisterUsed(program, gxmProgramGetVaryings(program), reg + k)) {
						*out++ = odd | 0xa0000000 | odd << 16 | odd << 14 | odd << 19 |
							 (reg + k) << 21 | normalize |
							 (reg + (position >> 2)) << 7;
						*out++ = instruction;
					}
				}
			} else {
				for (k = components; k-- > 0; position -= size) {
					if (gxmRegisterUsed(program, gxmProgramGetVaryings(program), reg + k)) {
						*out++ = k | 0xa0000000 | k << 19 | k << 14 | k << 16 |
							 (reg + k) << 21 | normalize |
							 (reg + (position >> 2)) << 7;
						*out++ = instruction;
					}
				}
			}
		}

defaults:
		mask &= 0xf << components;
		low = mask & 3;
		high = mask >> 2;
		if (low != 0) {
			*out++ = (reg >> 1) << 18 | 0x40000000 | low << 24;
			*out++ = GXM_UNPACK_DEFAULTS;
		}
		if (high != 0) {
			*out++ = ((reg + 2) >> 1) << 18 | 0x40000040 | high << 24;
			*out++ = GXM_UNPACK_DEFAULTS;
		}
	}

	/* the last instruction ends the program; with none, a nop does */
	if (out != start) {
		end = out[-1] | GXM_USSE_END;
	} else {
		start[0] = 0;
		start[1] = 0xf8000140;
		out = start + 2;
		end = 0xf8040140;
	}
	out[-1] = end;
	gxmChunkBufferCommit(buffer, out);
	return 0;
}
