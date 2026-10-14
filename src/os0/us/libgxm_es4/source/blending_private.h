/*
 * SceGxm: fragment blending, shared by the blend code generators.
 *
 * Blending is USSE code the shader patcher appends to a fragment program.
 * 8-bit outputs blend with one or two fixed-function instructions
 * (blend_code_u8.c); half4 outputs get a short program of float16
 * instructions, built as a list (blend_code_f16.c), optimised
 * (blend_code_f16_optimizer.c) and encoded (blend_code_f16_assembler.c).
 */

#ifndef GXM_BLENDING_PRIVATE_H
#define GXM_BLENDING_PRIVATE_H

#include "gxm_internal.h"

/* SDK gxm.h */
typedef struct SceGxmBlendInfo {
	SceUInt8 colorMask;		/* SCE_GXM_COLOR_MASK_* */
	SceUInt8 colorFunc : 4;		/* SCE_GXM_BLEND_FUNC_* */
	SceUInt8 alphaFunc : 4;
	SceUInt8 colorSrc : 4;		/* SCE_GXM_BLEND_FACTOR_* */
	SceUInt8 colorDst : 4;
	SceUInt8 alphaSrc : 4;
	SceUInt8 alphaDst : 4;
} SceGxmBlendInfo;

#define SCE_GXM_COLOR_MASK_NONE		0
#define SCE_GXM_COLOR_MASK_A		(1 << 0)
#define SCE_GXM_COLOR_MASK_R		(1 << 1)
#define SCE_GXM_COLOR_MASK_G		(1 << 2)
#define SCE_GXM_COLOR_MASK_B		(1 << 3)
#define SCE_GXM_COLOR_MASK_ALL		0x0f

#define SCE_GXM_BLEND_FUNC_NONE			0
#define SCE_GXM_BLEND_FUNC_ADD			1
#define SCE_GXM_BLEND_FUNC_SUBTRACT		2
#define SCE_GXM_BLEND_FUNC_REVERSE_SUBTRACT	3
#define SCE_GXM_BLEND_FUNC_MIN			4
#define SCE_GXM_BLEND_FUNC_MAX			5

#define SCE_GXM_BLEND_FACTOR_ZERO			0
#define SCE_GXM_BLEND_FACTOR_ONE			1
#define SCE_GXM_BLEND_FACTOR_SRC_COLOR			2
#define SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_COLOR	3
#define SCE_GXM_BLEND_FACTOR_SRC_ALPHA			4
#define SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA	5
#define SCE_GXM_BLEND_FACTOR_DST_COLOR			6
#define SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_COLOR	7
#define SCE_GXM_BLEND_FACTOR_DST_ALPHA			8
#define SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_ALPHA	9
#define SCE_GXM_BLEND_FACTOR_SRC_ALPHA_SATURATE		10
#define SCE_GXM_BLEND_FACTOR_DST_ALPHA_SATURATE		11

/* the declared output register types the blend code handles */
#define GXM_OUTPUT_TYPE_F16		1	/* half4: float16 program */
#define GXM_OUTPUT_TYPE_U8		7	/* uchar4: fixed-function blend */

/* the float16 blend program, cached between building and writing */
typedef struct GxmBlendCode {
	SceSize size;			/* bytes of code */
	SceUInt32 code[30];
} GxmBlendCode;

/* --- float16 blend program list --- */

#define GXM_BLEND_OP_NOP	0
#define GXM_BLEND_OP_MAD	1	/* dst = src0 * src1 + src2 */
#define GXM_BLEND_OP_MUL	2	/* dst = src1 * src2 */
#define GXM_BLEND_OP_MIN	3
#define GXM_BLEND_OP_MAX	4
#define GXM_BLEND_OP_ADD	5	/* dst = src1 + src2 */

/* operands, gxmBlendRegisters[] */
#define GXM_BLEND_REG_NONE	0
#define GXM_BLEND_REG_O0	1	/* the colour in the output buffer */
#define GXM_BLEND_REG_P0	2	/* the fragment program's output */
#define GXM_BLEND_REG_GPI0	3
#define GXM_BLEND_REG_GPI1	4
#define GXM_BLEND_REG_GPI2	5
#define GXM_BLEND_REG_R0	6	/* temporaries */
#define GXM_BLEND_REG_R1	7
#define GXM_BLEND_REG_ZERO	8	/* constants */
#define GXM_BLEND_REG_ONE	9

typedef struct GxmBlendInstr {
	SceUInt32 op;			/* GXM_BLEND_OP_* */
	SceUInt32 dst;			/* GXM_BLEND_REG_* */
	SceUInt32 src0;
	SceUInt32 src1;
	SceUInt32 src2;
	SceUInt32 mask;			/* SCE_GXM_COLOR_MASK_* written */
	SceUInt8 negate;		/* negate src1 */
	SceUInt8 src0Alpha;		/* read the alpha component only */
	SceUInt8 src1Alpha;
	SceUInt8 src2Alpha;
} GxmBlendInstr;

/* the longest program the list builder makes, with its terminating NOP */
#define GXM_BLEND_MAX_INSTRS	15

SceUInt32 *gxmBlendCodeU8(SceUInt32 *code, SceUInt32 reg, const SceGxmBlendInfo *blendInfo);
SceSize gxmBlendCodeF16(SceBool optimize, const SceGxmBlendInfo *blendInfo, SceUInt32 reg,
			SceUInt32 *code, SceSize codeSize);
GxmBlendInstr *gxmBlendOptimize(GxmBlendInstr *first, GxmBlendInstr *end);
SceUInt32 *gxmBlendAssemble(const GxmBlendInstr *instr, SceUInt32 *code, SceUInt32 reg);

/* --- building a fragment program's USSE code (usse_builder.c) --- */

typedef struct GxmFragmentBuild {
	const SceGxmProgram *program;
	SceUInt32 outputFormat;		/* SCE_GXM_OUTPUT_REGISTER_FORMAT_* */
	SceUInt32 multisampleMode;
	const SceGxmBlendInfo *blendInfo;
	SceUInt32 instructionCount;
	SceBool wideOutput;		/* 64-bit output register */
	GxmBlendCode blend;
} GxmFragmentBuild;

int gxmFragmentBuildInit(GxmFragmentBuild *build, const SceGxmProgram *program,
			 SceUInt32 outputFormat, SceUInt32 multisampleMode,
			 const SceGxmBlendInfo *blendInfo);
SceSize gxmFragmentBuildGetCodeSize(const GxmFragmentBuild *build);
int gxmFragmentBuildCode(const GxmFragmentBuild *build, GxmChunkBuffer *buffer, GxmCodeRef *ref);

SceUInt32 gxmBlendGetInstructionCount(SceUInt32 reg, const SceGxmBlendInfo *blendInfo,
				      SceUInt32 type, GxmBlendCode *cache);
SceUInt32 *gxmBlendWriteCode(SceUInt32 *code, SceUInt32 reg, const SceGxmBlendInfo *blendInfo,
			     SceUInt32 type, const GxmBlendCode *cache);

#endif
