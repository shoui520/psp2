/*
 * bigmac.c: Bigmac DMA memory copy helpers and constant-time byte helpers for
 * compat_sm.self (0x80b954..0x80ba46, 0x80c08e..0x80c0bc).
 *
 * Internal function names are ours.
 */

#include "compat_sm.h"

/* 0x80b954 (name ours) */
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

/* 0x80b98c (name ours) */
int bigmacInitChannel0(uint32_t *ctrl)
{
	ctrl[0] = 0u;
	ctrl[1] = 0u;
	return 0;
}

/* 0x80b99c (name ours) */
int bigmacGetChannelStatus(const uint32_t *ctrl, uint32_t *outStatus)
{
	*outStatus = BIGMAC->ch[ctrl[0]].status;
	return 0;
}

/* 0x80b9b6 (name ours) */
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

/* 0x80b9e4 (name ours) */
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
