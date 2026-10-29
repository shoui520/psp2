/*
 * SceGxm: a fragment program's USSE code as the shader patcher builds it.
 *
 * The code is the program's primary code, then the conversion of its
 * output to the requested format, then blending, then a trailer naming
 * the program.  Init works out the size, Code writes it.
 */

#include "blending_private.h"

/* the phase kind field of a phase instruction */
#define GXM_USSE_PHASE_KIND(word1)	(((word1) >> 8) & 7)

static inline SceBool gxmBlendInfoIsValid(const SceGxmBlendInfo *blendInfo)
{
	return (blendInfo->colorMask & ~SCE_GXM_COLOR_MASK_ALL) == 0 &&
	       blendInfo->colorFunc <= SCE_GXM_BLEND_FUNC_MAX &&
	       blendInfo->alphaFunc <= SCE_GXM_BLEND_FUNC_MAX &&
	       blendInfo->colorSrc <= SCE_GXM_BLEND_FACTOR_DST_ALPHA_SATURATE &&
	       blendInfo->colorDst <= SCE_GXM_BLEND_FACTOR_DST_ALPHA_SATURATE &&
	       blendInfo->alphaSrc <= SCE_GXM_BLEND_FACTOR_DST_ALPHA_SATURATE &&
	       blendInfo->alphaDst <= SCE_GXM_BLEND_FACTOR_DST_ALPHA_SATURATE &&
	       /* min and max take factors of one */
	       ((blendInfo->colorFunc != SCE_GXM_BLEND_FUNC_MIN &&
		 blendInfo->colorFunc != SCE_GXM_BLEND_FUNC_MAX) ||
		(blendInfo->colorSrc == SCE_GXM_BLEND_FACTOR_ONE &&
		 blendInfo->colorDst == SCE_GXM_BLEND_FACTOR_ONE)) &&
	       ((blendInfo->alphaFunc != SCE_GXM_BLEND_FUNC_MIN &&
		 blendInfo->alphaFunc != SCE_GXM_BLEND_FUNC_MAX) ||
		(blendInfo->alphaSrc == SCE_GXM_BLEND_FACTOR_ONE &&
		 blendInfo->alphaDst == SCE_GXM_BLEND_FACTOR_ONE));
}

/* the result depends on the colour already in the output buffer */
static inline SceBool gxmBlendReadsDestination(const SceGxmBlendInfo *blendInfo)
{
	if (blendInfo->colorFunc != SCE_GXM_BLEND_FUNC_NONE &&
	    !(blendInfo->colorSrc <= SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA &&
	      blendInfo->colorDst == SCE_GXM_BLEND_FACTOR_ZERO))
		return SCE_TRUE;
	if (blendInfo->alphaFunc != SCE_GXM_BLEND_FUNC_NONE &&
	    !(blendInfo->alphaSrc <= SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA &&
	      blendInfo->alphaDst == SCE_GXM_BLEND_FACTOR_ZERO))
		return SCE_TRUE;
	return blendInfo->colorMask != SCE_GXM_COLOR_MASK_ALL;
}

/* adding with factors zero and one keeps the destination */
static inline SceBool gxmBlendKeepsColor(const SceGxmBlendInfo *blendInfo)
{
	if (!(blendInfo->colorMask & (SCE_GXM_COLOR_MASK_R | SCE_GXM_COLOR_MASK_G |
				      SCE_GXM_COLOR_MASK_B)))
		return SCE_TRUE;
	return (blendInfo->colorFunc == SCE_GXM_BLEND_FUNC_ADD ||
		blendInfo->colorFunc == SCE_GXM_BLEND_FUNC_REVERSE_SUBTRACT) &&
	       blendInfo->colorSrc == SCE_GXM_BLEND_FACTOR_ZERO &&
	       blendInfo->colorDst == SCE_GXM_BLEND_FACTOR_ONE;
}

static inline SceBool gxmBlendKeepsAlpha(const SceGxmBlendInfo *blendInfo)
{
	if (!(blendInfo->colorMask & SCE_GXM_COLOR_MASK_A))
		return SCE_TRUE;
	return (blendInfo->alphaFunc == SCE_GXM_BLEND_FUNC_ADD ||
		blendInfo->alphaFunc == SCE_GXM_BLEND_FUNC_REVERSE_SUBTRACT) &&
	       blendInfo->alphaSrc == SCE_GXM_BLEND_FACTOR_ZERO &&
	       blendInfo->alphaDst == SCE_GXM_BLEND_FACTOR_ONE;
}

int gxmFragmentBuildInit(GxmFragmentBuild *build, const SceGxmProgram *program,
			 SceUInt32 outputFormat, SceUInt32 multisampleMode,
			 const SceGxmBlendInfo *blendInfo)
{
	const GxmProgramVaryings *varyings = gxmProgramGetVaryings(program);
	SceUInt32 type = varyings->fragment.outputRegisterType;
	SceUInt32 componentCount = varyings->fragment.outputComponentCount;
	SceUInt32 outputCount, blendCount = 0, headerCount;
	int err;

	/* the instructions that write the output */
	if ((program->flags & (GXM_PROGRAM_NATIVE_COLOR | GXM_PROGRAM_UNK10000)) == 0) {
		outputCount = 0;
		err = gxmOutputFormatResolve(outputFormat, &type, &componentCount, &outputCount);
		if (err != 0)
			return err;
	} else {
		outputCount = 0;
	}

	if (blendInfo != NULL) {
		if ((program->flags & GXM_PROGRAM_NATIVE_COLOR) || !gxmBlendInfoIsValid(blendInfo))
			return SCE_GXM_ERROR_INVALID_VALUE;
		if ((type != GXM_OUTPUT_TYPE_U8 && type != GXM_OUTPUT_TYPE_F16) ||
		    componentCount != 4)
			return SCE_GXM_ERROR_UNSUPPORTED;
		blendCount = gxmBlendGetInstructionCount(gxmProgramGetVaryings(program)->fragment.outputRegister,
							 blendInfo, type, &build->blend);
		if (!gxmBlendReadsDestination(blendInfo)) {
			/* blending replaces the output write */
			outputCount += blendCount;
			blendCount = 0;
		} else if (multisampleMode != 0) {
			blendCount++;
		}
	}
	if (outputCount == 0)
		outputCount = (program->flags & GXM_PROGRAM_UNK4) ? 1 : 0;
	headerCount = (gxmGlobals.compatFlags & GXM_COMPAT_USSE_OLD_HEADER) ? 2 : 6;

	build->program = program;
	build->outputFormat = outputFormat;
	build->multisampleMode = multisampleMode;
	build->blendInfo = blendInfo;
	build->instructionCount = blendCount + outputCount + program->primaryInstructionCount +
				  headerCount;
	build->wideOutput = gxmOutputIsWide(type, componentCount);
	return 0;
}

SceSize gxmFragmentBuildGetCodeSize(const GxmFragmentBuild *build)
{
	return build->instructionCount << 3;
}

int gxmFragmentBuildCode(const GxmFragmentBuild *build, GxmChunkBuffer *buffer, GxmCodeRef *ref)
{
	const SceGxmProgram *program;
	const GxmProgramVaryings *varyings;
	const SceGxmBlendInfo *blendInfo;
	const SceInt32 *phases;
	SceUInt32 *code, *body, *out, *end, *instr;
	SceUInt32 offset, type, componentCount, flags, kind, i;
	SceBool keepsDestination, stop;

	code = gxmChunkBufferReserve(buffer, build->instructionCount * 8, gxmUsseAlignment());
	if (code == NULL)
		return SCE_GXM_ERROR_RESERVE_FAILED;
	program = build->program;
	offset = buffer->cur + buffer->gpuEnd - buffer->end;
	varyings = gxmProgramGetVaryings(program);

	/* the program, then its output in the requested format */
	body = code + program->primaryInstructionCount * 2;
	sceClibMemcpy_safe(code, GXM_RELATIVE(program->primaryProgramOffset),
		      program->primaryInstructionCount * 8);
	type = gxmProgramGetVaryings(program)->fragment.outputRegisterType;
	componentCount = gxmProgramGetVaryings(program)->fragment.outputComponentCount;
	out = body;
	if ((program->flags & (GXM_PROGRAM_NATIVE_COLOR | GXM_PROGRAM_UNK10000)) == 0)
		out = gxmOutputFormatWriteConversion(body, varyings->fragment.outputRegister,
						     build->outputFormat, &type, &componentCount,
						     build->blendInfo != NULL);

	/* blending that keeps the destination lets the later phases end early */
	blendInfo = build->blendInfo;
	if (blendInfo == NULL)
		keepsDestination = SCE_FALSE;
	else
		keepsDestination = gxmBlendKeepsColor(blendInfo) && gxmBlendKeepsAlpha(blendInfo) &&
				   !(program->flags & GXM_PROGRAM_UNK8000);

	/* chain the phases */
	phases = GXM_RELATIVE(program->entryOffset);
	instr = code + phases[0] * 2;
	for (i = 1; i < program->primaryPhaseCount; i++) {
		stop = keepsDestination && GXM_USSE_PHASE_KIND(instr[1]) == 1;
		instr[0] = ((offset + phases[i] * 8) << 9) >> 12;
		instr = code + phases[i] * 2;
		if (stop) {
			instr[0] = 0;
			instr[1] = 0xfa4c0700;
			break;
		}
	}

	if (blendInfo != NULL) {
		if (build->multisampleMode != 0 && gxmBlendReadsDestination(blendInfo)) {
			/* the last phase runs the output write as a phase of its own */
			instr[0] = 0;
			instr[1] = ((program->flags >> 1) & 1) << 13 | 0xfa440800;
			end = out;
			if ((program->flags & GXM_PROGRAM_UNK4) && out == body) {
				out[0] = 0;
				out[1] = 0xf8000140;
				end = out + 2;
			}
			if (!(gxmGlobals.compatFlags & GXM_COMPAT_UNK80000) && instr == code &&
			    end == instr + 2)
				instr[1] |= 0x80000;
			else
				end[-1] |= 0x40000;
			instr[0] = ((offset + ((char *)end - (char *)code)) << 9) >> 12;
			end[0] = 0;
			end[1] = 0xfa440700;
			out = end + 2;
		}
		out = gxmBlendWriteCode(out, varyings->fragment.outputRegister, blendInfo, type,
					&build->blend);
	}

	/* the last instruction ends the program */
	if ((program->flags & GXM_PROGRAM_UNK4) && out == body) {
		out[0] = 0;
		out[1] = 0xf8000140;
		out += 2;
	}
	out[-1] |= 0x40000;

	out[0] = GXM_PROGRAM_TRAILER;
	out[2] = (SceUInt32)program;
	if (build->blendInfo == NULL) {
		out[1] = (SceUInt32)program;
		out[3] = GXM_PROGRAM_TRAILER_VERTEX;
	} else {
		out[1] = *(const SceUInt32 *)build->blendInfo;
		out[3] = GXM_PROGRAM_TRAILER_BLEND;
	}
	buffer->cur = (SceUInt32)(out + 4);

	ref->address = 0;
	flags = program->flags;
	kind = (flags & GXM_PROGRAM_UNK2) ? 3 : 2;
	if (flags & GXM_PROGRAM_UNK800)
		kind = (flags & GXM_PROGRAM_UNK2) ? 0xb : 0xa;
	ref->taskControl = ((flags & GXM_PROGRAM_UNK1000) ? 4 : 0) | kind |
		     ((flags & GXM_PROGRAM_UNK400) ? 0x10 : 0);
	gxmCodeRefSetAddress(ref, offset + phases[0] * 8);
	return 0;
}
