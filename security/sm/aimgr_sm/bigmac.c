/*
 * bigmac.c: Bigmac DMA, AES-128-CBC, and HMAC-SHA-256 helpers for aimgr_sm.self
 * (0x80bd06..0x80c06a, 0x80c16e..0x80c188).
 *
 * Internal function and table names are ours.
 */

#include "aimgr_sm.h"

/* 0x80c2dc: AES key byte sizes by keySizeIndex (0..3) */
static const uint32_t s_bigmacAesKeySizes[4] = {
	0x00u, 0x10u, 0x18u, 0x20u
};

/* 0x80bd06 (name ours) */
int bigmacAes128CbcEncryptSync(void *dst, const void *src, uint32_t size,
		const void *key, void *iv)
{
	uint32_t bmCtrl[2];
	int ret;

	bigmacInitChannel0(bmCtrl);
	ret = bigmacStartAesCipher(dst, src, size, key, iv, 1u, 1u, 1u, bmCtrl);
	if (ret == 0)
		ret = bigmacWaitChannelDone(bmCtrl);
	return ret;
}

/* 0x80bd4e (name ours) */
int bigmacAes128CbcDecryptSync(void *dst, const void *src, uint32_t size,
		const void *key, void *iv)
{
	uint32_t bmCtrl[2];
	int ret;

	bigmacInitChannel0(bmCtrl);
	ret = bigmacStartAesCipher(dst, src, size, key, iv, 2u, 1u, 1u, bmCtrl);
	if (ret == 0)
		ret = bigmacWaitChannelDone(bmCtrl);
	return ret;
}

/* 0x80bd98 (name ours) */
int bigmacHmacSha256Sync(void *outMac, const void *src, uint32_t size,
		const void *key, uint32_t keyLen)
{
	uint32_t bmCtrl[2];
	int ret;

	bigmacInitChannel0(bmCtrl);
	ret = bigmacStartHmacSha256(outMac, src, size, key, keyLen, 6u, bmCtrl);
	if (ret == 0)
		ret = bigmacWaitChannelDone(bmCtrl);
	return ret;
}

/* 0x80bddc (name ours) */
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

/* 0x80be14 (name ours) */
int bigmacInitChannel0(uint32_t *ctrl)
{
	ctrl[0] = 0u;
	ctrl[1] = 0x2080u;
	return 0;
}

/* 0x80be24 (name ours) */
int bigmacGetChannelStatus(const uint32_t *ctrl, uint32_t *outStatus)
{
	*outStatus = BIGMAC->ch[ctrl[0] & 1u].status;
	return 0;
}

/* 0x80be3e (name ours) */
int bigmacWaitChannelDone(const uint32_t *ctrl)
{
	BigmacChannelRegs *regs = &BIGMAC->ch[ctrl[0] & 1u];
	uint32_t status;

	do {
		status = regs->status;
	} while ((status & 1u) != 0u);

	if (status != 0u)
		return SCE_SBL_ERROR_IO;
	return 0;
}

/* 0x80be6c (name ours) */
int bigmacStartAesCipher(void *dst, const void *src, uint32_t size,
		const void *keyOrSlot, const void *iv, uint32_t cipherMode,
		uint32_t useExternalKey, uint32_t keySizeIdx, const uint32_t *bmCtrl)
{
	uint32_t keySizes[4];
	uint32_t ch = bmCtrl[0] & 1u;
	BigmacChannelRegs *regs = &BIGMAC->ch[ch];
	uint32_t funcCmd;

	memcpy(keySizes, s_bigmacAesKeySizes, sizeof(keySizes));
	if (((uintptr_t)dst & 3u) != 0u ||
	    (((uintptr_t)iv | (uintptr_t)src) & 3u) != 0u) {
		return SCE_SBL_ERROR_INVALID_VALUE;
	}

	regs->src = (uint32_t)(uintptr_t)src;
	regs->dst = (uint32_t)(uintptr_t)dst;
	regs->length = size;

	funcCmd = ((bmCtrl[1] & ~0x3fu) | (cipherMode & 7u) |
		   ((useExternalKey << 3) & 0x38u)) & ~0x300u;
	funcCmd |= (keySizeIdx << 8) & 0x300u;
	regs->func = funcCmd;

	if (iv != NULL)
		regs->iv = (uint32_t)(uintptr_t)iv;

	if ((funcCmd & 0x80u) == 0u) {
		regs->keySlot = *(const uint32_t *)keyOrSlot;
	} else {
		uint32_t keyBytes = keySizes[keySizeIdx - 1u];
		volatile uint32_t *extKeyDst = &BIGMAC->externalKey[ch << 5];
		const uint32_t *keySrc = (const uint32_t *)keyOrSlot;
		uint32_t i;

		for (i = 0u; i < (keyBytes >> 2); i++)
			extKeyDst[i] = keySrc[i];
	}

	regs->trigger = 1u;
	return 0;
}

/* 0x80bf86 (name ours) */
int bigmacStartHmacSha256(void *outMac, const void *src, uint32_t size,
		const void *key, uint32_t keyLen, uint32_t flags,
		const uint32_t *bmCtrl)
{
	uint32_t ch = bmCtrl[0] & 1u;
	BigmacChannelRegs *regs = &BIGMAC->ch[ch];
	volatile uint32_t *extKeyDst;
	const uint32_t *keySrc = (const uint32_t *)key;
	uint32_t i;

	if (keyLen != 64u || (((uintptr_t)src) & 3u) != 0u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	regs->src = (uint32_t)(uintptr_t)src;
	regs->dst = (uint32_t)(uintptr_t)outMac;
	regs->length = size;
	regs->func = (((bmCtrl[1] & ~0x3fu) | 3u | ((flags << 3) & 0x38u)) & ~0xc00u);

	extKeyDst = &BIGMAC->externalKey[ch << 5];
	for (i = 0u; i < 16u; i++)
		extKeyDst[i] = keySrc[i];

	regs->trigger = 1u;
	return 0;
}

/* 0x80c008 (name ours) */
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
