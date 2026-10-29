/*
 * SceGxm: compiled shader programs (GXP).
 *
 * A program is a blob from the shader compiler; its tables are reached
 * through offsets relative to the field holding them.
 */

#include "gxm_internal.h"
#include <stddef.h>

#define GXM_PROGRAM_MAGIC		0x00505847	/* "GXP\0" */

#define SCE_GXM_VERTEX_PROGRAM		0
#define SCE_GXM_FRAGMENT_PROGRAM	1

/* vertex program outputs, fragment program inputs */
#define GXM_VARYING_POSITION		0x00000001
#define GXM_VARYING_FOG			0x00000002
#define GXM_VARYING_COLOR0		0x00000004
#define GXM_VARYING_COLOR1		0x00000008
#define GXM_VARYING_TEXCOORD(n)		(0x00000010 << (n))
#define GXM_VARYING_PSIZE		0x00004000	/* fragment: SPRITECOORD */
#define GXM_VARYING_CLIP(n)		(0x00008000 << (n))

int sceGxmProgramCheck(const SceGxmProgram *program)
{
	if (program == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (((SceUInt32)program & 3) != 0)
		return SCE_GXM_ERROR_INVALID_ALIGNMENT;
	if (program->magic != GXM_PROGRAM_MAGIC || program->majorVersion != 1)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if ((SceUInt32)(program->minorVersion - 4) >= 2)
		return SCE_GXM_ERROR_INVALID_VALUE;
	return 0;
}

SceBool sceGxmProgramIsEquivalent(const SceGxmProgram *programA, const SceGxmProgram *programB)
{
	/* the GUIDs may differ */
	if (programA->magic != programB->magic || programA->version != programB->version ||
	    programA->size != programB->size)
		return 0;
	return sceClibMemcmp(&programA->flags, &programB->flags,
			     programA->size - offsetof(SceGxmProgram, flags)) == 0;
}

SceUInt32 sceGxmProgramGetSize(const SceGxmProgram *program)
{
	return program->size;
}

SceUInt32 sceGxmProgramGetType(const SceGxmProgram *program)
{
	return program->flags & GXM_PROGRAM_FRAGMENT;
}

SceBool sceGxmProgramIsDiscardUsed(const SceGxmProgram *program)
{
	return (program->flags & GXM_PROGRAM_DISCARD) >> 3;
}

SceBool sceGxmProgramIsDepthReplaceUsed(const SceGxmProgram *program)
{
	return (program->flags & GXM_PROGRAM_DEPTH_REPLACE) >> 4;
}

SceBool sceGxmProgramIsSpriteCoordUsed(const SceGxmProgram *program)
{
	return (program->flags & GXM_PROGRAM_SPRITE_COORD) >> 5;
}

SceBool sceGxmProgramIsNativeColorUsed(const SceGxmProgram *program)
{
	return (program->flags & GXM_PROGRAM_NATIVE_COLOR) >> 6;
}

SceBool sceGxmProgramIsFragColorUsed(const SceGxmProgram *program)
{
	return (program->flags & GXM_PROGRAM_FRAG_COLOR) >> 7;
}

int sceGxmProgramGetOutputRegisterFormat(const SceGxmProgram *program, SceUInt32 *type,
					 SceUInt32 *componentCount)
{
	const GxmProgramVaryings *varyings;

	if (program == NULL || type == NULL || componentCount == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (!(program->flags & GXM_PROGRAM_FRAGMENT))
		return SCE_GXM_ERROR_INVALID_VALUE;
	varyings = gxmProgramGetVaryings(program);
	*type = varyings->fragment.outputRegisterType;
	*componentCount = varyings->fragment.outputComponentCount;
	return 0;
}

SceUInt32 sceGxmProgramGetVertexProgramOutputs(const SceGxmProgram *program)
{
	const GxmProgramVaryings *varyings;
	SceUInt32 flags, texCoords, outputs;

	if (program->flags & GXM_PROGRAM_FRAGMENT)
		return 0;
	varyings = gxmProgramGetVaryings(program);
	flags = varyings->vertex.outputs;
	texCoords = varyings->vertex.texCoordOutputs;

	outputs = GXM_VARYING_POSITION;
	if (flags & 0x200)
		outputs |= GXM_VARYING_FOG;
	if (flags & 0x800)
		outputs |= GXM_VARYING_COLOR0;
	if (flags & 0x400)
		outputs |= GXM_VARYING_COLOR1;
	/* three format bits per texture coordinate */
	if (texCoords & (7 << 0))
		outputs |= GXM_VARYING_TEXCOORD(0);
	if (texCoords & (7 << 3))
		outputs |= GXM_VARYING_TEXCOORD(1);
	if (texCoords & (7 << 6))
		outputs |= GXM_VARYING_TEXCOORD(2);
	if (texCoords & (7 << 9))
		outputs |= GXM_VARYING_TEXCOORD(3);
	if (texCoords & (7 << 12))
		outputs |= GXM_VARYING_TEXCOORD(4);
	if (texCoords & (7 << 15))
		outputs |= GXM_VARYING_TEXCOORD(5);
	if (texCoords & (7 << 18))
		outputs |= GXM_VARYING_TEXCOORD(6);
	if (texCoords & (7 << 21))
		outputs |= GXM_VARYING_TEXCOORD(7);
	if (texCoords & (7 << 24))
		outputs |= GXM_VARYING_TEXCOORD(8);
	if (texCoords & (7 << 27))
		outputs |= GXM_VARYING_TEXCOORD(9);
	if (flags & 0x100)
		outputs |= GXM_VARYING_PSIZE;
	if (flags & 0x01)
		outputs |= GXM_VARYING_CLIP(0);
	if (flags & 0x02)
		outputs |= GXM_VARYING_CLIP(1);
	if (flags & 0x04)
		outputs |= GXM_VARYING_CLIP(2);
	if (flags & 0x08)
		outputs |= GXM_VARYING_CLIP(3);
	if (flags & 0x10)
		outputs |= GXM_VARYING_CLIP(4);
	if (flags & 0x20)
		outputs |= GXM_VARYING_CLIP(5);
	if (flags & 0x40)
		outputs |= GXM_VARYING_CLIP(6);
	if (flags & 0x80)
		outputs |= GXM_VARYING_CLIP(7);
	return outputs;
}

/*
 * Each fragment input names up to two varyings: a texture coordinate in
 * bits 0..3 and any varying in bits 12..15 (15 for none); bit 10 and
 * bit 30 mark them as the sprite coordinate.
 */
SceUInt32 sceGxmProgramGetFragmentProgramInputs(const SceGxmProgram *program)
{
	const GxmProgramVaryings *varyings;
	const GxmProgramFragmentInput *input, *end;
	SceUInt32 inputs, word;

	if (!(program->flags & GXM_PROGRAM_FRAGMENT))
		return 0;
	varyings = gxmProgramGetVaryings(program);
	input = GXM_RELATIVE(varyings->fragment.inputsOffset);
	end = input + varyings->fragment.inputCount;
	if (input == end)
		return 0;

	inputs = 0;
	do {
		word = input->word;
		if ((word & 0xf) != 0xf) {
			if (word & 0x400) {
				inputs |= GXM_VARYING_PSIZE;
			} else {
				switch (word & 0xf) {
				case 0:
					inputs |= GXM_VARYING_TEXCOORD(0);
					break;
				case 1:
					inputs |= GXM_VARYING_TEXCOORD(1);
					break;
				case 2:
					inputs |= GXM_VARYING_TEXCOORD(2);
					break;
				case 3:
					inputs |= GXM_VARYING_TEXCOORD(3);
					break;
				case 4:
					inputs |= GXM_VARYING_TEXCOORD(4);
					break;
				case 5:
					inputs |= GXM_VARYING_TEXCOORD(5);
					break;
				case 6:
					inputs |= GXM_VARYING_TEXCOORD(6);
					break;
				case 7:
					inputs |= GXM_VARYING_TEXCOORD(7);
					break;
				case 8:
					inputs |= GXM_VARYING_TEXCOORD(8);
					break;
				case 9:
					inputs |= GXM_VARYING_TEXCOORD(9);
					break;
				}
			}
		}
		if ((word & 0xf000) != 0xf000) {
			if (word & 0x40000000) {
				inputs |= GXM_VARYING_PSIZE;
			} else {
				switch (word & 0xf000) {
				case 0x0000:
					inputs |= GXM_VARYING_TEXCOORD(0);
					break;
				case 0x1000:
					inputs |= GXM_VARYING_TEXCOORD(1);
					break;
				case 0x2000:
					inputs |= GXM_VARYING_TEXCOORD(2);
					break;
				case 0x3000:
					inputs |= GXM_VARYING_TEXCOORD(3);
					break;
				case 0x4000:
					inputs |= GXM_VARYING_TEXCOORD(4);
					break;
				case 0x5000:
					inputs |= GXM_VARYING_TEXCOORD(5);
					break;
				case 0x6000:
					inputs |= GXM_VARYING_TEXCOORD(6);
					break;
				case 0x7000:
					inputs |= GXM_VARYING_TEXCOORD(7);
					break;
				case 0x8000:
					inputs |= GXM_VARYING_TEXCOORD(8);
					break;
				case 0x9000:
					inputs |= GXM_VARYING_TEXCOORD(9);
					break;
				case 0xa000:
					inputs |= GXM_VARYING_COLOR0;
					break;
				case 0xb000:
					inputs |= GXM_VARYING_COLOR1;
					break;
				case 0xc000:
					inputs |= GXM_VARYING_FOG;
					break;
				case 0xd000:
					inputs |= GXM_VARYING_POSITION;
					break;
				}
			}
		}
		input++;
	} while (input != end);
	return inputs;
}

SceUInt32 sceGxmProgramGetDefaultUniformBufferSize(const SceGxmProgram *program)
{
	return program->defaultUniformBufferCount << 2;
}

SceUInt32 sceGxmProgramGetParameterCount(const SceGxmProgram *program)
{
	return program->parameterCount;
}

const SceGxmProgramParameter *sceGxmProgramGetParameter(const SceGxmProgram *program,
							SceUInt32 index)
{
	const SceGxmProgramParameter *parameters = gxmProgramGetParameters(program);

	if (index >= program->parameterCount)
		return NULL;
	return &parameters[index];
}

const SceGxmProgramParameter *sceGxmProgramFindParameterByName(const SceGxmProgram *program,
							       const char *name)
{
	const SceGxmProgramParameter *parameter;
	const char *parameterName;
	SceUInt32 count, i;

	if (name == NULL)
		return NULL;
	parameter = gxmProgramGetParameters(program);
	count = program->parameterCount;
	for (i = 0; i < count; i++, parameter++) {
		parameterName = GXM_RELATIVE(parameter->nameOffset);
		if (parameterName != NULL && gxmStrcmp(name, parameterName) == 0)
			return parameter;
	}
	return NULL;
}

const SceGxmProgramParameter *_sceGxmProgramFindParameterBySemantic(const SceGxmProgram *program,
								    SceUInt32 semantic,
								    SceUInt32 index)
{
	const SceGxmProgramParameter *parameter = gxmProgramGetParameters(program);
	SceUInt32 count = program->parameterCount, i;

	for (i = 0; i < count; i++, parameter++) {
		if (semantic == gxmParameterGetSemantic(parameter) &&
		    index == gxmParameterGetSemanticIndex(parameter))
			return parameter;
	}
	return NULL;
}

/* only attributes carry semantics */
const SceGxmProgramParameter *sceGxmProgramFindParameterBySemantic(const SceGxmProgram *program,
								   SceUInt32 semantic,
								   SceUInt32 index)
{
	const SceGxmProgramParameter *parameter;
	SceUInt32 i;

	if (semantic == 0)
		return NULL;
	parameter = gxmProgramGetParameters(program);
	for (i = 0; i < program->parameterCount; i++, parameter++) {
		if (gxmParameterGetCategory(parameter) == SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE &&
		    semantic == gxmParameterGetSemantic(parameter) &&
		    index == gxmParameterGetSemanticIndex(parameter))
			return parameter;
	}
	return NULL;
}

SceUInt32 sceGxmProgramParameterGetIndex(const SceGxmProgram *program,
					 const SceGxmProgramParameter *parameter)
{
	return parameter - gxmProgramGetParameters(program);
}

SceUInt32 sceGxmProgramParameterGetCategory(const SceGxmProgramParameter *parameter)
{
	return gxmParameterGetCategory(parameter);
}

const char *sceGxmProgramParameterGetName(const SceGxmProgramParameter *parameter)
{
	return GXM_RELATIVE(parameter->nameOffset);
}

SceUInt8 _sceGxmProgramParameterGetSemantic(const SceGxmProgramParameter *parameter)
{
	return ((const SceUInt8 *)&parameter->info)[2];
}

SceUInt32 sceGxmProgramParameterGetSemantic(const SceGxmProgramParameter *parameter)
{
	if (gxmParameterGetCategory(parameter) != SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE)
		return 0;
	return gxmParameterGetSemantic(parameter);
}

SceUInt32 sceGxmProgramParameterGetSemanticIndex(const SceGxmProgramParameter *parameter)
{
	return ((const SceUInt8 *)&parameter->info)[3] & 0xf;
}

SceUInt32 sceGxmProgramParameterGetType(const SceGxmProgramParameter *parameter)
{
	return (parameter->info & GXM_PARAMETER_TYPE) >> 4;
}

SceUInt32 sceGxmProgramParameterGetComponentCount(const SceGxmProgramParameter *parameter)
{
	return (parameter->info & GXM_PARAMETER_COMPONENT_COUNT) >> 8;
}

SceUInt32 sceGxmProgramParameterGetArraySize(const SceGxmProgramParameter *parameter)
{
	return parameter->arraySize;
}

SceUInt32 sceGxmProgramParameterGetResourceIndex(const SceGxmProgramParameter *parameter)
{
	return parameter->resourceIndex;
}

SceUInt32 sceGxmProgramParameterGetContainerIndex(const SceGxmProgramParameter *parameter)
{
	return (parameter->info & GXM_PARAMETER_CONTAINER_INDEX) >> 12;
}

SceBool sceGxmProgramParameterIsSamplerCube(const SceGxmProgramParameter *parameter)
{
	return (parameter->info & GXM_PARAMETER_SAMPLER_CUBE) >> 28;
}

/* a vertex attribute the program reads in register format, unconverted */
SceBool sceGxmProgramParameterIsRegFormat(const SceGxmProgram *program,
					  const SceGxmProgramParameter *parameter)
{
	const GxmProgramVaryings *varyings;
	SceUInt32 index;

	if (program->flags & GXM_PROGRAM_FRAGMENT)
		return 0;
	if (gxmParameterGetCategory(parameter) != SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE)
		return 0;
	varyings = gxmProgramGetVaryings(program);
	index = parameter->resourceIndex;
	return (varyings->vertex.regFormatMask[index >> 5] & (1 << (index & 0x1f))) != 0;
}

/* --- building a program's code into USSE memory --- */

/* the DMS word of a fragment program for a pass of this many samples */
SceUInt32 gxmProgramGetFragmentDms(const SceGxmProgram *program, SceBool phased, SceUInt32 samples)
{
	SceUInt32 temps = (program->tempRegCount + 1) & ~1;
	SceUInt32 outputs, perSample, word;

	if (samples == 1) {
		outputs = 8;
		perSample = 2;
	} else if (samples == 2) {
		outputs = 16;
		perSample = 4;
	} else {
		outputs = 4;
		perSample = 1;
	}
	if (program->flags & GXM_PROGRAM_NATIVE_COLOR)
		outputs = ((program->tempRegCount2 + 1) & ~1) * perSample;
	else if (!phased)
		outputs = 0;
	if (outputs < temps)
		outputs = temps;
	gxmDmsFragmentInfo(program->primaryRegCount, program->secondaryRegCount, outputs,
			 (program->flags >> 20) & 1, &word);
	return word;
}

/* the size of a vertex program's code: the unpack code, then the program */
SceSize gxmProgramGetVertexCodeSize(const SceGxmProgram *program,
				    const SceGxmVertexAttribute *attributes, SceUInt32 attributeCount)
{
	SceUInt32 pairs = gxmVertexUnpackSize(program, attributes, attributeCount);
	SceSize alignment = gxmUsseAlignment();

	return ((alignment + pairs * 8 - 1) & -alignment) + (program->primaryInstructionCount + 2) * 8;
}

/* a leading phase instruction is replaced by the library's header */
static inline _Bool gxmUsseIsPhase(const SceUInt32 *code)
{
	return (code[1] & 0xff700000) == 0xfa400000;
}

SceSize gxmProgramGetSecondaryCodeSize(const SceGxmProgram *program)
{
	SceUInt32 count = program->secondaryInstructionCount;
	const SceUInt32 *code;

	if (count == 0)
		return gxmUsseGetEmptyProgramSize();
	code = GXM_RELATIVE(program->secondaryProgramOffset);
	if (gxmUsseIsPhase(code))
		count--;
	return (count + ((gxmGlobals.compatFlags & GXM_COMPAT_USSE_OLD_HEADER) ? 4 : 9)) * 8;
}

int gxmProgramBuildSecondaryCode(GxmChunkBuffer *buffer, const SceGxmProgram *program,
				 GxmCodeRef *ref)
{
	SceUInt32 count = program->secondaryInstructionCount, size, offset;
	const SceUInt32 *source;
	SceUInt32 *code, *out;

	if (count == 0)
		return gxmUsseBuildEmptyProgram(buffer, ref);
	source = GXM_RELATIVE(program->secondaryProgramOffset);
	size = count;
	if (gxmUsseIsPhase(source))
		size--;
	size = (size + ((gxmGlobals.compatFlags & GXM_COMPAT_USSE_OLD_HEADER) ? 4 : 9)) * 8;
	code = gxmChunkBufferReserve(buffer, size, gxmUsseAlignment());
	if (code == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	code[1] = 0xfa440700;
	code[0] = 0;
	offset = buffer->cur + buffer->gpuEnd - buffer->end;
	if (!(gxmGlobals.compatFlags & GXM_COMPAT_USSE_OLD_HEADER))
		out = gxmUsseWriteHeader(code + 2);
	else
		out = gxmUsseWriteOldHeader(code + 2);
	source = GXM_RELATIVE(program->secondaryProgramOffset);
	if (gxmUsseIsPhase(source)) {
		count--;
		source += 2;
	}
	sceClibMemcpy_safe(out, source, count * 8);
	buffer->cur = (SceUInt32)(out + count * 2);
	ref->address = 0;
	ref->taskControl = 2;
	gxmCodeRefSetAddress(ref, offset);
	return 0;
}


int gxmProgramBuildVertexCode(GxmChunkBuffer *buffer, const SceGxmProgram *program,
			      const SceGxmVertexAttribute *attributes, SceUInt32 attributeCount,
			      GxmCodeRef *ref)
{
	const GxmProgramVaryings *varyings = gxmProgramGetVaryings(program);
	SceUInt32 claimed[2], missing[2], unpackOffset, *unpackCode, bit, reg, last, i;
	SceUInt32 *code, *trailer, offset, entry;
	const SceInt32 *entryField;
	int err;

	/* every register is fed by exactly one attribute, typed or in register format */
	claimed[0] = 0;
	claimed[1] = 0;
	if (attributeCount == 0) {
		missing[0] = ~0u;
		missing[1] = ~0u;
	} else {
		for (i = 0; i < attributeCount; i++) {
			reg = attributes[i].regIndex;
			if (attributes[i].format == SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED_REG) {
				last = reg + attributes[i].componentCount;
				for (; reg < last; reg++) {
					bit = 1 << (reg & 0x1f);
					if (claimed[reg >> 5] & bit)
						return SCE_GXM_ERROR_INVALID_VALUE;
					claimed[reg >> 5] |= bit;
					if (!(varyings->vertex.regFormatMask[reg >> 5] & bit))
						return SCE_GXM_ERROR_INVALID_VALUE;
				}
			} else {
				last = reg + 4;
				for (; reg < last; reg++) {
					bit = 1 << (reg & 0x1f);
					if (claimed[reg >> 5] & bit)
						return SCE_GXM_ERROR_INVALID_VALUE;
					claimed[reg >> 5] |= bit;
					if (varyings->vertex.regFormatMask[reg >> 5] & bit)
						return SCE_GXM_ERROR_INVALID_VALUE;
				}
			}
		}
		missing[0] = ~claimed[0];
		missing[1] = ~claimed[1];
	}
	if ((missing[0] & varyings->vertex.inputMask[0]) != 0 ||
	    (missing[1] & varyings->vertex.inputMask[1]) != 0)
		return SCE_GXM_ERROR_INVALID_VALUE;

	unpackOffset = 0;
	unpackCode = NULL;
	err = gxmVertexUnpackBuild(buffer, program, attributes, attributeCount, &unpackOffset,
				   &unpackCode);
	if (err != 0)
		return err;

	/* the program itself, then a trailer naming it */
	code = gxmChunkBufferReserve(buffer, (program->primaryInstructionCount + 2) * 8,
				     gxmUsseAlignment());
	if (code == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	offset = buffer->cur + buffer->gpuEnd - buffer->end;
	sceClibMemcpy_safe(code, GXM_RELATIVE(program->primaryProgramOffset),
		      program->primaryInstructionCount << 3);
	trailer = code + program->primaryInstructionCount * 2;
	trailer[0] = GXM_PROGRAM_TRAILER;
	trailer[1] = (SceUInt32)program;
	trailer[2] = (SceUInt32)program;
	trailer[3] = GXM_PROGRAM_TRAILER_VERTEX;
	buffer->cur = (SceUInt32)(trailer + 4);

	entryField = GXM_RELATIVE(program->entryOffset);
	entry = offset + *entryField * 8;
	ref->address = 0;
	ref->taskControl = (program->flags >> 1) & 1;
	if (unpackCode == NULL) {
		gxmCodeRefSetAddress(ref, entry);
		return 0;
	}
	/* the unpack code branches on to the program */
	unpackCode[0] = (entry >> 3) & 0xfffff;
	gxmCodeRefSetAddress(ref, unpackOffset);
	return 0;
}
