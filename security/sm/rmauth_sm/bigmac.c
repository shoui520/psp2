/*
 * bigmac.c: Bigmac DMA and AES-128-CBC crypto helpers for rmauth_sm.self
 * (0x80b764..0x80b9b6).
 *
 * Internal function and table names are ours.
 */

#include "rmauth_sm.h"

/* 0x80bca8: AES key byte sizes by keySizeIndex (0..3) */
static const uint32_t s_bigmacAesKeySizes[4] = {
	0x00u, 0x10u, 0x18u, 0x20u
};

/* 0x80b764 (name ours) */
int bigmacInitChannel0(uint32_t *ctrl)
{
	ctrl[0] = 0u;
	ctrl[1] = 0u;
	return 0;
}

/* 0x80b774 (name ours) */
int bigmacGetChannelStatus(const uint32_t *ctrl, uint32_t *outStatus)
{
	*outStatus = BIGMAC->ch[ctrl[0]].status;
	return 0;
}

/* 0x80b78e (name ours) */
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

/* 0x80b7bc (name ours) */
int bigmacStartAes128CbcEncrypt(void *dst, const void *keyOrSlot, uint32_t size,
		const void *src, const void *iv, const uint32_t *bmCtrl)
{
	return bigmacStartAesCipher(dst, src, size, keyOrSlot, iv, 0u, 1u, 128u, bmCtrl);
}

/* 0x80b7de (name ours) */
int bigmacStartAes128CbcDecrypt(void *dst, const void *keyOrSlot, uint32_t size,
		const void *src, const void *iv, const uint32_t *bmCtrl)
{
	return bigmacStartAesCipher(dst, src, size, keyOrSlot, iv, 1u, 1u, 128u, bmCtrl);
}

/* 0x80b802 (name ours) */
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

/* 0x80b864 (name ours) */
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

/* 0x80b89c (name ours) */
int bigmacStartAesCipher(void *dst, const void *src, uint32_t size,
		const void *keyOrSlot, const void *iv, uint32_t cipherMode,
		uint32_t useExternalKey, uint32_t keyBits, const uint32_t *bmCtrl)
{
	uint32_t ch = bmCtrl[0];
	BigmacChannelRegs *regs = &BIGMAC->ch[ch];
	uint32_t keySizeIdx;
	uint32_t funcCmd;

	if ((regs->status & 1u) != 0u)
		return SCE_SBL_ERROR_BUSY;
	if (((uintptr_t)keyOrSlot & 3u) != 0u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	keySizeIdx = (keyBits >> 6) - 1u;
	if (keySizeIdx == 0u || keySizeIdx > 3u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	funcCmd = (bmCtrl[1] & 0xf0000000u) | (keySizeIdx << 2) | 1u;
	if (cipherMode == 1u)
		funcCmd |= 2u;
	else if (cipherMode == 2u)
		funcCmd |= 0x80u;
	else if (cipherMode != 0u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	if (iv != NULL)
		funcCmd |= 0x200u;

	if (useExternalKey != 0u) {
		uint32_t keyWords = s_bigmacAesKeySizes[keySizeIdx] >> 2;
		volatile uint32_t *extKeyDst = &BIGMAC->externalKey[ch << 5];
		const uint32_t *keySrc = (const uint32_t *)keyOrSlot;
		uint32_t i;

		for (i = 0u; i < keyWords; i++)
			extKeyDst[i] = keySrc[i];
	} else {
		funcCmd |= 0x100u;
		regs->keySlot = (uint32_t)(uintptr_t)keyOrSlot;
	}

	regs->src = (uint32_t)(uintptr_t)src;
	regs->dst = (uint32_t)(uintptr_t)dst;
	regs->length = size;
	regs->func = funcCmd;
	regs->iv = (uint32_t)(uintptr_t)iv;
	regs->rng = bmCtrl[1] >> 27;
	smMemBarrier();
	regs->trigger = 1u;
	return 0;
}
