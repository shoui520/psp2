/*
 * SceGxm: 8-bit blending.
 *
 * uchar4 outputs blend in fixed function: one instruction when colour and
 * alpha share their factors, otherwise one instruction with separate
 * colour and alpha fields; a colour mask other than all adds a masked
 * write.  Reverse subtraction swaps the source and destination factors.
 */

#include "blending_private.h"

/* the factor seen from the other side, for reverse subtraction */
static const SceUInt32 gxmBlendSwappedFactor[12] = {
	[SCE_GXM_BLEND_FACTOR_ZERO] = SCE_GXM_BLEND_FACTOR_ZERO,
	[SCE_GXM_BLEND_FACTOR_ONE] = SCE_GXM_BLEND_FACTOR_ONE,
	[SCE_GXM_BLEND_FACTOR_SRC_COLOR] = SCE_GXM_BLEND_FACTOR_DST_COLOR,
	[SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_COLOR] = SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
	[SCE_GXM_BLEND_FACTOR_SRC_ALPHA] = SCE_GXM_BLEND_FACTOR_DST_ALPHA,
	[SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA] = SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
	[SCE_GXM_BLEND_FACTOR_DST_COLOR] = SCE_GXM_BLEND_FACTOR_SRC_COLOR,
	[SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_COLOR] = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
	[SCE_GXM_BLEND_FACTOR_DST_ALPHA] = SCE_GXM_BLEND_FACTOR_SRC_ALPHA,
	[SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_ALPHA] = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	[SCE_GXM_BLEND_FACTOR_SRC_ALPHA_SATURATE] = SCE_GXM_BLEND_FACTOR_DST_ALPHA_SATURATE,
	[SCE_GXM_BLEND_FACTOR_DST_ALPHA_SATURATE] = SCE_GXM_BLEND_FACTOR_SRC_ALPHA_SATURATE,
};

/* the shared-factor instruction: factor and function fields */
static const SceUInt32 gxmBlendSrcFactorBits[12] = {
	0x0000000, 0x1000000, 0x0000080, 0x1000080, 0x00000c0, 0x10000c0,
	0x0000180, 0x1000180, 0x00001c0, 0x10001c0, 0x0000040, 0x1000040,
};
static const SceUInt32 gxmBlendDstFactorBits[12] = {
	0x0000, 0x8000, 0x0010, 0x8010, 0x0018, 0x8018,
	0x0030, 0x8030, 0x0038, 0x8038, 0x0008, 0x8008,
};
static const SceUInt32 gxmBlendAlphaFuncBits[6] = {
	0x000, 0x000, 0x200, 0x200, 0x400, 0x600,
};
static const SceUInt32 gxmBlendColorFuncBits[6] = {
	0x000000, 0x000000, 0x100000, 0x100000, 0x200000, 0x300000,
};

/* the separate-factor instruction */
static const SceUInt32 gxmBlendColorDstBits[12] = {
	0x0000, 0x8000, 0x0008, 0x8008, 0x0018, 0x8018,
	0x0010, 0x8010, 0x0020, 0x8020, 0x0028, 0x8028,
};
static const SceUInt32 gxmBlendColorOpBits[6] = {
	0x00000, 0x00000, 0x40000, 0x40000, 0x80000, 0xc0000,
};
static const SceUInt32 gxmBlendAlphaOpBits[6] = {
	0x00000, 0x00000, 0x10000, 0x10000, 0x20000, 0x30000,
};
static const SceUInt32 gxmBlendAlphaDstBits[12] = {
	0x000, 0x004, 0x200, 0x204, 0x200, 0x204,
	0x400, 0x404, 0x400, 0x404, 0x004, 0x004,
};
static const SceUInt32 gxmBlendColorSrcBits[12] = {
	0x0000000, 0x1000000, 0x0000040, 0x1000040, 0x00000c0, 0x10000c0,
	0x0000080, 0x1000080, 0x0000100, 0x1000100, 0x0000140, 0x1000140,
};
static const SceUInt32 gxmBlendAlphaSrcBits[12] = {
	0x000000, 0x000800, 0x100000, 0x100800, 0x100000, 0x100800,
	0x200000, 0x200800, 0x200000, 0x200800, 0x000800, 0x000800,
};

/* the output register as a source of the swapped-operand form */
static inline SceUInt32 gxmBlendSwappedSource(SceUInt32 operand)
{
	return ((operand >> 7) & 0x7f) | (operand >> 30) << 28 |
	       ((operand & 0x20000) ? 0x10000 : 0);
}

SceUInt32 *gxmBlendCodeU8(SceUInt32 *code, SceUInt32 reg, const SceGxmBlendInfo *blendInfo)
{
	const SceUInt8 *bytes = (const SceUInt8 *)blendInfo;
	SceUInt32 colorMask = blendInfo->colorMask;
	SceUInt32 masked = colorMask != SCE_GXM_COLOR_MASK_ALL;
	SceUInt32 funcs = bytes[1];
	SceUInt32 colorFunc, alphaFunc, colorSrc, colorDst, alphaSrc, alphaDst, src, dst;
	SceUInt32 operand, word0, head, swapped, write;

	if (funcs == 0) {
		if (!masked)
			return code;
		write = reg | 0xa0000000 | reg << 7;
		goto mask;
	}

	operand = reg << 7 | 0x80000000;
	if (bytes[2] == bytes[3] && blendInfo->alphaSrc != SCE_GXM_BLEND_FACTOR_DST_ALPHA_SATURATE &&
	    blendInfo->alphaDst != SCE_GXM_BLEND_FACTOR_DST_ALPHA_SATURATE &&
	    funcs != (SCE_GXM_BLEND_FUNC_SUBTRACT << 4 | SCE_GXM_BLEND_FUNC_REVERSE_SUBTRACT)) {
		alphaFunc = funcs >> 4;
		if (alphaFunc != SCE_GXM_BLEND_FUNC_REVERSE_SUBTRACT) {
			/* one set of factors for colour and alpha */
			colorFunc = funcs & 0xf;
			src = blendInfo->alphaSrc;
			dst = blendInfo->alphaDst;
			if (alphaFunc == SCE_GXM_BLEND_FUNC_NONE &&
			    colorFunc == SCE_GXM_BLEND_FUNC_NONE) {
				dst = SCE_GXM_BLEND_FACTOR_ZERO;
				src = SCE_GXM_BLEND_FACTOR_ONE;
			}
			if (colorFunc == SCE_GXM_BLEND_FUNC_REVERSE_SUBTRACT) {
				swapped = gxmBlendSwappedFactor[dst];
				dst = gxmBlendSwappedFactor[src];
				src = swapped;
				word0 = gxmBlendSwappedSource(operand);
				head = 0x40000000;
			} else {
				word0 = 0x10000000;
				head = operand;
			}
			code[0] = word0 | head;
			code[1] = gxmBlendColorFuncBits[colorFunc] | gxmBlendAlphaFuncBits[alphaFunc] |
				  0x90800001 | colorMask << 11 | gxmBlendSrcFactorBits[src] |
				  gxmBlendDstFactorBits[dst];
			return code + 2;
		}
	}

	/* separate colour and alpha factors */
	colorFunc = funcs & 0xf;
	alphaFunc = funcs >> 4;
	if (colorFunc == SCE_GXM_BLEND_FUNC_NONE) {
		colorDst = SCE_GXM_BLEND_FACTOR_ZERO;
		colorSrc = SCE_GXM_BLEND_FACTOR_ONE;
	} else {
		colorDst = blendInfo->colorDst;
		colorSrc = blendInfo->colorSrc;
	}
	if (alphaFunc == SCE_GXM_BLEND_FUNC_NONE) {
		alphaDst = SCE_GXM_BLEND_FACTOR_ZERO;
		alphaSrc = SCE_GXM_BLEND_FACTOR_ONE;
	} else {
		alphaDst = blendInfo->alphaDst;
		alphaSrc = blendInfo->alphaSrc;
	}
	if (colorFunc == SCE_GXM_BLEND_FUNC_REVERSE_SUBTRACT) {
		swapped = gxmBlendSwappedFactor[colorDst];
		colorDst = gxmBlendSwappedFactor[colorSrc];
		colorSrc = swapped;
		swapped = gxmBlendSwappedFactor[alphaDst];
		alphaDst = gxmBlendSwappedFactor[alphaSrc];
		alphaSrc = swapped;
		word0 = gxmBlendSwappedSource(operand) | (alphaFunc == SCE_GXM_BLEND_FUNC_SUBTRACT) << 14;
		head = 0x40000000;
	} else {
		word0 = 0x10000000 | (alphaFunc == SCE_GXM_BLEND_FUNC_REVERSE_SUBTRACT) << 14;
		head = operand;
	}
	code[1] = (masked ^ 1) | 0x80800000 | gxmBlendColorSrcBits[colorSrc] |
		  gxmBlendColorDstBits[colorDst] | gxmBlendAlphaSrcBits[alphaSrc] |
		  gxmBlendAlphaDstBits[alphaDst];
	code[0] = gxmBlendAlphaOpBits[alphaFunc] | gxmBlendColorOpBits[colorFunc] | head | word0;
	code += 2;
	if (!masked)
		return code;
	write = 0;
	colorMask = blendInfo->colorMask;

mask:
	code[0] = write;
	code[1] = colorMask << 11 | 0x90b00601;
	return code + 2;
}
