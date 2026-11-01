/*
 * SceGxm: float16 blend programs.
 *
 * The blend equation becomes a list of float16 operations: the source
 * colour times its factor into r0, the destination colour times its
 * factor into r1, then the blend function of r0 and r1 into o0.  Alpha
 * gets its own operations when its factor or function differs from the
 * colour's.
 */

#include "blending_private.h"

static inline GxmBlendInstr *gxmBlendEmit(GxmBlendInstr *instr, SceUInt32 op, SceUInt32 dst,
					  SceUInt32 src1, SceUInt32 src2, SceUInt32 mask,
					  SceUInt8 negate, SceUInt8 src1Alpha, SceUInt8 src2Alpha)
{
	instr->wmask = mask;
	instr->op = op;
	instr->dst = dst;
	instr->src0 = REG_NONE;
	instr->src1 = src1;
	instr->src2 = src2;
	instr->negate = negate;
	instr->src0Alpha = 0;
	instr->src1Alpha = src1Alpha;
	instr->src2Alpha = src2Alpha;
	return instr + 1;
}

/* dst = value * factor */
static inline GxmBlendInstr *gxmBlendFactor(GxmBlendInstr *instr, SceUInt32 dst, SceUInt32 value,
					    SceUInt32 factor, SceUInt32 mask)
{
	switch (factor) {
	case SCE_GXM_BLEND_FACTOR_ZERO:
		return gxmBlendEmit(instr, GXM_BLEND_OP_ADD, dst, REG_ZERO,
				    REG_ZERO, mask, 0, 0, 0);
	case SCE_GXM_BLEND_FACTOR_ONE:
		return gxmBlendEmit(instr, GXM_BLEND_OP_MUL, dst, REG_ONE, value,
				    mask, 0, 0, 0);
	case SCE_GXM_BLEND_FACTOR_SRC_COLOR:
		return gxmBlendEmit(instr, GXM_BLEND_OP_MUL, dst, REG_P0, value,
				    mask, 0, 0, 0);
	case SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
		instr = gxmBlendEmit(instr, GXM_BLEND_OP_MUL, dst, REG_P0, value,
				     mask, 0, 0, 0);
		return gxmBlendEmit(instr, GXM_BLEND_OP_ADD, dst, dst, value, mask, 1, 0, 0);
	case SCE_GXM_BLEND_FACTOR_SRC_ALPHA:
		return gxmBlendEmit(instr, GXM_BLEND_OP_MUL, dst, REG_P0, value,
				    mask, 0, 1, 0);
	case SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
		instr = gxmBlendEmit(instr, GXM_BLEND_OP_MUL, dst, REG_P0, value,
				     mask, 0, 1, 0);
		return gxmBlendEmit(instr, GXM_BLEND_OP_ADD, dst, dst, value, mask, 1, 0, 0);
	case SCE_GXM_BLEND_FACTOR_DST_COLOR:
		return gxmBlendEmit(instr, GXM_BLEND_OP_MUL, dst, REG_O0, value,
				    mask, 0, 0, 0);
	case SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
		instr = gxmBlendEmit(instr, GXM_BLEND_OP_MUL, dst, REG_O0, value,
				     mask, 1, 0, 0);
		return gxmBlendEmit(instr, GXM_BLEND_OP_ADD, dst, dst, value, mask, 0, 0, 0);
	case SCE_GXM_BLEND_FACTOR_DST_ALPHA:
		return gxmBlendEmit(instr, GXM_BLEND_OP_MUL, dst, REG_O0, value,
				    mask, 0, 1, 0);
	case SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
		instr = gxmBlendEmit(instr, GXM_BLEND_OP_MUL, dst, REG_O0, value,
				     mask, 1, 1, 0);
		return gxmBlendEmit(instr, GXM_BLEND_OP_ADD, dst, dst, value, mask, 0, 0, 0);
	case SCE_GXM_BLEND_FACTOR_SRC_ALPHA_SATURATE:
		/* min(1 - o0.a, p0.a) */
		instr = gxmBlendEmit(instr, GXM_BLEND_OP_ADD, dst, REG_O0,
				     REG_ONE, mask, 1, 1, 0);
		instr = gxmBlendEmit(instr, GXM_BLEND_OP_MIN, dst, dst, REG_P0,
				     mask, 0, 0, 1);
		return gxmBlendEmit(instr, GXM_BLEND_OP_MUL, dst, dst, value, mask, 0, 0, 0);
	case SCE_GXM_BLEND_FACTOR_DST_ALPHA_SATURATE:
		/* min(1 - p0.a, o0.a) */
		instr = gxmBlendEmit(instr, GXM_BLEND_OP_ADD, dst, REG_P0,
				     REG_ONE, mask, 1, 1, 0);
		instr = gxmBlendEmit(instr, GXM_BLEND_OP_MIN, dst, dst, REG_O0,
				     mask, 0, 0, 1);
		return gxmBlendEmit(instr, GXM_BLEND_OP_MUL, dst, dst, value, mask, 0, 0, 0);
	}
	return instr;
}

/* o0 = func(r0, r1); the slot is used even for an unknown function */
static inline GXM_DBG_NOINLINE GxmBlendInstr *gxmBlendFunc(GxmBlendInstr *instr, SceUInt32 mask, SceUInt32 func)
{
	switch (func) {
	case SCE_GXM_BLEND_FUNC_NONE:
		GXM_DBG_ASSERT(235, !"Unsupported blend mode");
		break;
	case SCE_GXM_BLEND_FUNC_ADD:
		gxmBlendEmit(instr, GXM_BLEND_OP_ADD, REG_O0, REG_R0,
			     REG_R1, mask, 0, 0, 0);
		break;
	case SCE_GXM_BLEND_FUNC_SUBTRACT:
		gxmBlendEmit(instr, GXM_BLEND_OP_ADD, REG_O0, REG_R1,
			     REG_R0, mask, 1, 0, 0);
		break;
	case SCE_GXM_BLEND_FUNC_REVERSE_SUBTRACT:
		gxmBlendEmit(instr, GXM_BLEND_OP_ADD, REG_O0, REG_R0,
			     REG_R1, mask, 1, 0, 0);
		break;
	case SCE_GXM_BLEND_FUNC_MIN:
		gxmBlendEmit(instr, GXM_BLEND_OP_MIN, REG_O0, REG_R0,
			     REG_R1, mask, 0, 0, 0);
		break;
	case SCE_GXM_BLEND_FUNC_MAX:
		gxmBlendEmit(instr, GXM_BLEND_OP_MAX, REG_O0, REG_R0,
			     REG_R1, mask, 0, 0, 0);
		break;
	}
	return instr + 1;
}

/* the factors give the same alpha */
static inline SceBool gxmBlendSameAlphaFactor(SceUInt32 color, SceUInt32 alpha)
{
	return alpha == color ||
	       (color == SCE_GXM_BLEND_FACTOR_SRC_COLOR && alpha == SCE_GXM_BLEND_FACTOR_SRC_ALPHA) ||
	       (color == SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_COLOR &&
		alpha == SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) ||
	       (color == SCE_GXM_BLEND_FACTOR_DST_COLOR && alpha == SCE_GXM_BLEND_FACTOR_DST_ALPHA) ||
	       (color == SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_COLOR &&
		alpha == SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_ALPHA) ||
	       (color == SCE_GXM_BLEND_FACTOR_SRC_ALPHA && alpha == SCE_GXM_BLEND_FACTOR_SRC_COLOR) ||
	       (color == SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA &&
		alpha == SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_COLOR) ||
	       (color == SCE_GXM_BLEND_FACTOR_DST_ALPHA && alpha == SCE_GXM_BLEND_FACTOR_DST_COLOR) ||
	       (color == SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_ALPHA &&
		alpha == SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_COLOR);
}

static inline SceBool gxmBlendFuncIsMinMax(SceUInt32 func)
{
	return func == SCE_GXM_BLEND_FUNC_MIN || func == SCE_GXM_BLEND_FUNC_MAX;
}

/* build the list, ending in a NOP; returns its length */
static GXM_NOINLINE SceUInt32 gxmBlendBuildList(SceBool optimize, GxmBlendInstr *list,
						SceGxmBlendInfo blend)
{
	SceUInt32 colorFunc, alphaFunc, colorSrc, colorDst, alphaSrc, alphaDst, mask;
	GxmBlendInstr *instr = list, *end;
	SceBool separate;

	colorFunc = blend.colorFunc;
	alphaFunc = blend.alphaFunc;
	if (colorFunc == SCE_GXM_BLEND_FUNC_NONE) {
		colorFunc = SCE_GXM_BLEND_FUNC_ADD;
		colorSrc = SCE_GXM_BLEND_FACTOR_ONE;
		colorDst = SCE_GXM_BLEND_FACTOR_ZERO;
	} else {
		colorSrc = blend.colorSrc;
		colorDst = blend.colorDst;
	}
	if (alphaFunc == SCE_GXM_BLEND_FUNC_NONE) {
		alphaFunc = SCE_GXM_BLEND_FUNC_ADD;
		alphaSrc = SCE_GXM_BLEND_FACTOR_ONE;
		alphaDst = SCE_GXM_BLEND_FACTOR_ZERO;
	} else {
		alphaSrc = blend.alphaSrc;
		alphaDst = blend.alphaDst;
	}
	/* min and max ignore the factors */
	if (gxmBlendFuncIsMinMax(colorFunc)) {
		colorSrc = SCE_GXM_BLEND_FACTOR_ONE;
		colorDst = SCE_GXM_BLEND_FACTOR_ONE;
	}
	if (gxmBlendFuncIsMinMax(alphaFunc)) {
		alphaSrc = SCE_GXM_BLEND_FACTOR_ONE;
		alphaDst = SCE_GXM_BLEND_FACTOR_ONE;
	}

	/* r0 = p0 * source factor */
	separate = !gxmBlendSameAlphaFactor(colorSrc, alphaSrc) &&
		   (blend.colorMask & SCE_GXM_COLOR_MASK_A);
	mask = separate ? blend.colorMask & ~SCE_GXM_COLOR_MASK_A : blend.colorMask;
	if (mask)
		instr = gxmBlendFactor(instr, REG_R0, REG_P0, colorSrc, mask);
	if (separate)
		instr = gxmBlendFactor(instr, REG_R0, REG_P0, alphaSrc,
				       SCE_GXM_COLOR_MASK_A);

	/* r1 = o0 * destination factor */
	separate = !gxmBlendSameAlphaFactor(colorDst, alphaDst) &&
		   (blend.colorMask & SCE_GXM_COLOR_MASK_A);
	mask = separate ? blend.colorMask & ~SCE_GXM_COLOR_MASK_A : blend.colorMask;
	if (mask)
		instr = gxmBlendFactor(instr, REG_R1, REG_O0, colorDst, mask);
	if (separate)
		instr = gxmBlendFactor(instr, REG_R1, REG_O0, alphaDst,
				       SCE_GXM_COLOR_MASK_A);

	/* o0 = func(r0, r1) */
	separate = alphaFunc != colorFunc && (blend.colorMask & SCE_GXM_COLOR_MASK_A);
	mask = separate ? blend.colorMask & ~SCE_GXM_COLOR_MASK_A : blend.colorMask;
	if (mask)
		instr = gxmBlendFunc(instr, mask, colorFunc);
	if (separate)
		instr = gxmBlendFunc(instr, SCE_GXM_COLOR_MASK_A, alphaFunc);

	instr->op = GXM_BLEND_OP_NOP;
	instr->dst = REG_NONE;
	instr->src0 = REG_NONE;
	instr->src1 = REG_NONE;
	instr->src2 = REG_NONE;
	instr->wmask = 0;
	instr->negate = 0;
	instr->src0Alpha = 0;
	instr->src1Alpha = 0;
	instr->src2Alpha = 0;
	end = instr + 1;
	if (optimize)
		end = gxmBlendOptimize(list, end);
	return (SceUInt32)((char *)end - (char *)list) / sizeof(GxmBlendInstr);
}

/* write the blend program for an output in register reg; returns its size in bytes */
SceSize gxmBlendCodeF16(SceBool optimize, const SceGxmBlendInfo *blendInfo, SceUInt32 reg,
			SceUInt32 *code, SceSize instructionBufferSize)
{
	GxmBlendInstr list[GXM_BLEND_MAX_INSTRS];
	SceUInt32 instructionCount, i;

	instructionCount = gxmBlendBuildList(optimize, list, *blendInfo);
	GXM_DBG_ASSERT(442, instructionBufferSize >=
			    (EURASIA_USE_INSTRUCTION_SIZE * instructionCount));
	for (i = 0; i < instructionCount; i++) {
		gxmBlendAssemble(&list[i], code, reg);
		code += 2;
	}
	return instructionCount * EURASIA_USE_INSTRUCTION_SIZE;
}
