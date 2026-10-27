/*
 * bigmac.c: Bigmac DMA and SHA-256 hash helpers for mgkm_sm.self
 * (0x80bdaa..0x80bf40).
 *
 * Internal function names are ours.
 */

#include "mgkm_sm.h"

/* 0x80bdaa (name ours) */
int bigmacInitChannel0(uint32_t *ctrl)
{
	ctrl[0] = 0u;
	ctrl[1] = 0u;
	return 0;
}

/* 0x80bdba (name ours) */
int bigmacGetChannelStatus(const uint32_t *ctrl, uint32_t *outStatus)
{
	*outStatus = BIGMAC->ch[ctrl[0]].status;
	return 0;
}

/* 0x80bdd4 (name ours) */
int bigmacWaitChannelDone(const uint32_t *ctrl)
{
	uint32_t status = 0u;

	do {
		int err = bigmacGetChannelStatus(ctrl, &status);

		if (err != 0)
			return err;
	} while ((status & 1u) != 0u);
	return 0;
}

/* 0x80be02 (name ours) */
int bigmacSha256(void *outDigest, const void *src, uint32_t size)
{
	uint32_t bmCtrl[2];
	int status;

	bigmacInitChannel0(bmCtrl);
	status = bigmacStartHash(outDigest, src, size, 2u, 0u, bmCtrl);
	if (status == 0)
		return bigmacWaitChannelDone(bmCtrl);
	return status;
}

/* 0x80be3c (name ours) */
int bigmacStartMemcpy(void *dst, const void *src, uint32_t size,
		const uint32_t *bmCtrl)
{
	uint32_t ch = bmCtrl[0];
	BigmacChannelRegs *regs = &BIGMAC->ch[ch];

	if ((regs->status & 1u) != 0u)
		return SCE_SBL_ERROR_BUSY;

	regs->src = (uint32_t)(uintptr_t)src;
	regs->dst = (uint32_t)(uintptr_t)dst;
	regs->length = size;
	regs->func = (bmCtrl[1] & 0xf0000000u) | 0x00000800u;
	regs->rng = bmCtrl[1] >> 27;
	smMemBarrier();
	regs->trigger = 1u;
	return 0;
}

/* 0x80be9e (name ours) */
int bigmacMemcpy(void *dst, const void *src, uint32_t size)
{
	uint32_t bmCtrl[2];
	int status;

	bigmacInitChannel0(bmCtrl);
	status = bigmacStartMemcpy(dst, src, size, bmCtrl);
	if (status == 0)
		return bigmacWaitChannelDone(bmCtrl);
	return status;
}

/* 0x80bed6 (name ours) */
int bigmacStartHash(void *outDigest, const void *src, uint32_t size,
		uint32_t hashAlgo, uint32_t waitMode, const uint32_t *bmCtrl)
{
	uint32_t ch = bmCtrl[0];
	BigmacChannelRegs *regs = &BIGMAC->ch[ch];

	if ((regs->status & 1u) != 0u)
		return SCE_SBL_ERROR_BUSY;
	if (((uintptr_t)outDigest & 3u) != 0u)
		return SCE_SBL_ERROR_INVALID_VALUE;
	if (hashAlgo - 1u > 2u || waitMode > 1u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	regs->src = (uint32_t)(uintptr_t)src;
	regs->dst = (uint32_t)(uintptr_t)outDigest;
	regs->length = size;
	regs->func = (bmCtrl[1] & 0xf0000000u) | (waitMode << 9) |
		     (hashAlgo << 4) | 0x00002003u;
	regs->iv = (uint32_t)(uintptr_t)outDigest;
	regs->rng = bmCtrl[1] >> 27;
	smMemBarrier();
	regs->trigger = 1u;
	return 0;
}
