/*
 * SceGxm: fragment blending.
 *
 * Blending is done by USSE code appended to the fragment program; the
 * code generators are in blend_code_*.c.
 */

#include "blending_private.h"

static inline SceBool gxmBlendIsMoveOnly(const SceGxmBlendInfo *blendInfo)
{
	return blendInfo->colorMask == SCE_GXM_COLOR_MASK_ALL &&
	       blendInfo->colorFunc == SCE_GXM_BLEND_FUNC_NONE &&
	       blendInfo->alphaFunc == SCE_GXM_BLEND_FUNC_NONE;
}

/*
 * The number of instructions blending takes for an output in register reg
 * of the given type; a float16 program is built into cache.
 */
SceUInt32 gxmBlendGetInstructionCount(SceUInt32 reg, const SceGxmBlendInfo *blendInfo,
				      SceUInt32 type, GxmBlendCode *cache)
{
	SceUInt32 blended = blendInfo->colorFunc != SCE_GXM_BLEND_FUNC_NONE ||
			    blendInfo->alphaFunc != SCE_GXM_BLEND_FUNC_NONE;
	SceUInt32 masked = blendInfo->colorMask != SCE_GXM_COLOR_MASK_ALL;

	if (!masked && !blended)
		return 1;
	if (type == GXM_OUTPUT_TYPE_F16) {
		cache->size = gxmBlendCodeF16(SCE_TRUE, blendInfo, reg, cache->code, sizeof(cache->code));
		return cache->size >> 3;
	}
	if (masked)
		return blended + 1;
	return blended;
}

/* write the blending code built by gxmBlendGetInstructionCount() */
SceUInt32 *gxmBlendWriteCode(SceUInt32 *code, SceUInt32 reg, const SceGxmBlendInfo *blendInfo,
			     SceUInt32 type, const GxmBlendCode *cache)
{
	if (gxmBlendIsMoveOnly(blendInfo)) {
		if (type != GXM_OUTPUT_TYPE_F16) {
			code[0] = reg << 7 | 0xa0000000;
			code[1] = 0x50a10009;
			return code + 2;
		}
		code[1] = 0x38800541;
		code[0] = (reg >> 1) << 6 | 0x83000000;
		return code + 2;
	}
	if (type != GXM_OUTPUT_TYPE_F16)
		return gxmBlendCodeU8(code, reg, blendInfo);
	sceClibMemcpy_safe(code, cache->code, cache->size);
	return (SceUInt32 *)((char *)code + (cache->size & ~3));
}

/* the move of the output register to the output buffer */
SceUInt32 *gxmUsseWriteOutputMove(SceUInt32 *code, SceUInt32 reg, SceBool wide)
{
	if (!wide) {
		code[0] = reg << 7 | 0xa0000000;
		code[1] = 0x50a10009;
		return code + 2;
	}
	code[0] = (reg >> 1) << 6 | 0x83000000;
	code[1] = 0x38800541;
	return code + 2;
}
