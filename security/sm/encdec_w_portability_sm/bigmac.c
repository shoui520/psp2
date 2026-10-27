/*
 * bigmac.c: Bigmac AES-256-CBC and DMA memory copy primitives for
 * encdec_w_portability_sm.self (0x80b568..0x80b7be, 0x80b830..0x80b86c).
 *
 * Internal function names (except memcpy) are ours.
 */

#include "encdec_w_portability_sm.h"

/* 0x80bc40: AES external key size in bytes indexed by keyBits (1=16, 2=24, 3=32) */
static const uint32_t s_bigmacAesKeySizes[4] = {
	0x00u, 0x10u, 0x18u, 0x20u
};

/* 0x80b568 (name ours) */
int bigmacInitChannel0(uint32_t *ctrl)
{
	ctrl[0] = 0u;
	ctrl[1] = 0x2080u;
	return 0;
}

/* 0x80b578 (name ours) */
int bigmacGetChannelStatus(const uint32_t *ctrl, uint32_t *outStatus)
{
	*outStatus = BIGMAC->ch[ctrl[0] & 1u].status;
	return 0;
}

/* 0x80b592 (name ours) */
int bigmacWaitChannelDone(const uint32_t *ctrl)
{
	uint32_t ch = ctrl[0] & 1u;

	while ((BIGMAC->ch[ch].status & 1u) != 0u)
		;
	if (BIGMAC->ch[ch].status != 0u)
		return SCE_SBL_ERROR_IO;
	return 0;
}

/* 0x80b5c0 (name ours) */
int bigmacStartAes256CbcEncrypt(void *dst, const void *src, uint32_t size,
		const void *key, const void *iv, const uint32_t *ctrl)
{
	return bigmacStartAesCipher(dst, src, size, key, iv, 1u, 1u, 3u, ctrl);
}

/* 0x80b5e4 (name ours) */
int bigmacStartAes256CbcDecrypt(void *dst, const void *src, uint32_t size,
		const void *key, const void *iv, const uint32_t *ctrl)
{
	return bigmacStartAesCipher(dst, src, size, key, iv, 2u, 1u, 3u, ctrl);
}

/* 0x80b60a (name ours) */
int bigmacMemcpy(void *dst, const void *src, uint32_t size)
{
	uint32_t bmCtrl[2];
	int status;

	bigmacInitChannel0(bmCtrl);
	status = bigmacStartMemcpy(dst, src, size, bmCtrl);
	if (status == 0)
		status = bigmacWaitChannelDone(bmCtrl);
	return status;
}

/* 0x80b642 (name ours) */
int bigmacStartAesCipher(void *dst, const void *src, uint32_t size,
		const void *key, const void *iv, uint32_t dir, uint32_t chainMode,
		uint32_t keyBits, const uint32_t *ctrl)
{
	uint32_t keySizes[4];
	uint32_t ch = ctrl[0];
	uint32_t flags = ctrl[1];
	uint32_t chIdx;

	memcpy(keySizes, s_bigmacAesKeySizes, sizeof(keySizes));
	if (((uintptr_t)dst & 3u) != 0u || (((uintptr_t)iv | (uintptr_t)src) & 3u) != 0u)
		return SCE_SBL_ERROR_INVALID_VALUE;

	chIdx = ch & 1u;
	BIGMAC->ch[chIdx].src = (uint32_t)(uintptr_t)src;
	BIGMAC->ch[chIdx].dst = (uint32_t)(uintptr_t)dst;
	BIGMAC->ch[chIdx].length = size;
	BIGMAC->ch[chIdx].func = ((chainMode & 7u) << 3) |
				 (flags & 0xfffffcc0u) |
				 (dir & 7u) |
				 ((keyBits & 3u) << 8);
	if (iv != NULL)
		BIGMAC->ch[chIdx].iv = (uint32_t)(uintptr_t)iv;

	if ((flags & 0x80u) == 0u) {
		BIGMAC->ch[chIdx].keySlot = *(const uint32_t *)key;
	} else {
		uint32_t kLen = keySizes[keyBits];
		volatile uint32_t *extKey = &BIGMAC->externalKey[chIdx * 32u];
		const uint32_t *kWords;

		for (kWords = (const uint32_t *)key;
		     (uint32_t)((uintptr_t)kWords - (uintptr_t)key) < kLen;
		     kWords++) {
			*extKey++ = *kWords;
		}
	}

	BIGMAC->ch[chIdx].trigger = 1u;
	return 0;
}

/* 0x80b75c (name ours) */
int bigmacStartMemcpy(void *dst, const void *src, uint32_t size,
		const uint32_t *bmCtrl)
{
	uint32_t ch = bmCtrl[0];
	uint32_t flags = bmCtrl[1];
	uint32_t chIdx;

	if (size - 1u >= 0x00ffffffu)
		return SCE_SBL_ERROR_INVALID_VALUE;

	chIdx = ch & 1u;
	BIGMAC->ch[chIdx].src = (uint32_t)(uintptr_t)src;
	BIGMAC->ch[chIdx].dst = (uint32_t)(uintptr_t)dst;
	BIGMAC->ch[chIdx].length = size;
	BIGMAC->ch[chIdx].func = flags & ~7u;
	BIGMAC->ch[chIdx].trigger = 1u;
	return 0;
}

/* 0x80b830 */
void *memcpy(void *dst, const void *src, size_t len)
{
	if ((((uintptr_t)src | (uintptr_t)dst | (uintptr_t)len) & 3u) == 0u) {
		uint32_t *dWords = (uint32_t *)dst;
		const uint32_t *sWords = (const uint32_t *)src;
		size_t wordCount = len >> 2;
		size_t i;

		for (i = 0u; i < wordCount; i++)
			dWords[i] = sWords[i];
	} else {
		uint8_t *dBytes = (uint8_t *)dst;
		const uint8_t *sBytes = (const uint8_t *)src;
		size_t i;

		for (i = 0u; i < len; i++)
			dBytes[i] = sBytes[i];
	}
	return dst;
}
