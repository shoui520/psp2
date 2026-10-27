/*
 * bigmac.c: Bigmac crypto/DMA engine driver for AES-128/256 ECB/CBC, AES-CMAC,
 * hardware memcpy, and hardware memset (0x804bec..0x805167).
 *
 * Register names follow security/first_loader/boot.c; function names are ours.
 */

#include "second_loader.h"

/* ============================================================================
 * .rodata (0x80c050..0x80c05f)
 * ============================================================================ */

static const uint32_t s_bigmacKeySizeBytes[4] = {
	0u, 16u, 24u, 32u
};

/* ============================================================================
 * Functions (0x804bec..0x805167)
 * ============================================================================ */

/* 0x804bec */
int bigmacInitContext(BigmacContext *ctx)
{
	ctx->channel = 0u;
	ctx->flags = 0x2080u;
	return 0;
}

/* 0x804bfc */
int bigmacGetChannelStatus(const BigmacContext *ctx, uint32_t *outStatus)
{
	*outStatus = BIGMAC->ch[ctx->channel & 1u].status;
	return 0;
}

/* 0x804c16 */
int bigmacWaitChannel(const BigmacContext *ctx)
{
	uint32_t ch = ctx->channel & 1u;

	while ((BIGMAC->ch[ch].status & 1u) != 0)
		;
	if (BIGMAC->ch[ch].status == 0u)
		return 0;
	return (int)0x800f0005;
}

/* 0x804c44 */
int bigmacSetChannelIndex(BigmacContext *ctx, uint32_t useExternalKey)
{
	ctx->flags = (ctx->flags & ~0x80u) | ((useExternalKey & 1u) << 7);
	return 0;
}

/* 0x804c5e */
int bigmacAes128EcbEncrypt(void *dst, const void *src, uint32_t size,
		const void *key, BigmacContext *ctx)
{
	return bigmacStartAesCipher(dst, src, size, key, NULL, 1u, 0u, 1u, ctx);
}

/* 0x804c80 */
int bigmacAes128EcbDecrypt(void *dst, const void *src, uint32_t size,
		const void *key, BigmacContext *ctx)
{
	return bigmacStartAesCipher(dst, src, size, key, NULL, 2u, 0u, 1u, ctx);
}

/* 0x804ca4 */
int bigmacAes256EcbEncrypt(void *dst, const void *src, uint32_t size,
		const void *key, BigmacContext *ctx)
{
	return bigmacStartAesCipher(dst, src, size, key, NULL, 1u, 0u, 3u, ctx);
}

/* 0x804cc8 */
int bigmacAes256EcbDecrypt(void *dst, const void *src, uint32_t size,
		const void *key, BigmacContext *ctx)
{
	return bigmacStartAesCipher(dst, src, size, key, NULL, 2u, 0u, 3u, ctx);
}

/* 0x804cec */
int bigmacAes256CbcEncrypt(void *dst, const void *src, uint32_t size,
		const void *key, void *iv, BigmacContext *ctx)
{
	return bigmacStartAesCipher(dst, src, size, key, iv, 1u, 1u, 1u, ctx);
}

/* 0x804d0e */
int bigmacAes256CbcDecrypt(void *dst, const void *src, uint32_t size,
		const void *key, void *iv, BigmacContext *ctx)
{
	return bigmacStartAesCipher(dst, src, size, key, iv, 2u, 1u, 1u, ctx);
}

/* 0x804d32 */
int bigmacAes128CbcDecryptKeyslot(void *dst, const void *src, uint32_t size,
		const void *keyOrSlot, void *iv, BigmacContext *ctx)
{
	return bigmacStartAesCipher(dst, src, size, keyOrSlot, iv, 1u, 1u, 3u, ctx);
}

/* 0x804d56 */
int bigmacAes256CbcDecryptAsync(void *dst, const void *src, uint32_t size,
		const void *key, void *iv, BigmacContext *ctx)
{
	return bigmacStartAesCipher(dst, src, size, key, iv, 2u, 1u, 3u, ctx);
}

/* 0x804d7c */
int bigmacAes128CbcEncryptSync(void *dst, const void *src, uint32_t size,
		const void *key, void *iv)
{
	BigmacContext ctx;
	int status;

	bigmacInitContext(&ctx);
	status = bigmacStartAesCipher(dst, src, size, key, iv, 1u, 1u, 1u, &ctx);
	if (status == 0)
		status = bigmacWaitChannel(&ctx);
	return status;
}

/* 0x804dc4 */
int bigmacAes256CbcDecryptSync(void *dst, const void *src, uint32_t size,
		const void *key, void *iv)
{
	BigmacContext ctx;
	int status;

	bigmacInitContext(&ctx);
	status = bigmacStartAesCipher(dst, src, size, key, iv, 2u, 1u, 1u, &ctx);
	if (status == 0)
		status = bigmacWaitChannel(&ctx);
	return status;
}

/* 0x804e0e */
int bigmacAesCmacWithKey(void *dstMac16, const void *src, uint32_t size,
		const void *keyOrSlot, uint32_t keyBytes, BigmacContext *ctx)
{
	uint32_t ch;
	uint32_t funcVal;

	if ((keyBytes != 16u && keyBytes != 24u && keyBytes != 32u) ||
	    size == 0u || (size & 0xfu) != 0u) {
		return (int)0x800f0016;
	}

	ch = ctx->channel & 1u;
	BIGMAC->ch[ch].src = (uint32_t)(uintptr_t)src;
	BIGMAC->ch[ch].dst = (uint32_t)(uintptr_t)dstMac16;
	BIGMAC->ch[ch].length = size;

	if (keyBytes == 24u)
		funcVal = (ctx->flags & 0xfffffcf8u) | 0x23bu;
	else if (keyBytes == 32u)
		funcVal = (ctx->flags & 0xfffffff8u) | 0x33bu;
	else
		funcVal = (ctx->flags & 0xfffffcf8u) | 0x13bu;

	BIGMAC->ch[ch].func = funcVal & 0xfffff3ffu;
	if ((funcVal & 0x80u) == 0) {
		BIGMAC->ch[ch].keyslot = *(const uint32_t *)keyOrSlot;
	} else {
		volatile uint32_t *extKey = &BIGMAC->externalKey[ch * 32u];
		const uint32_t *kWords = (const uint32_t *)keyOrSlot;
		uint32_t i;

		for (i = 0; i < (keyBytes >> 2); i++)
			extKey[i] = kWords[i];
	}
	BIGMAC->ch[ch].trigger = 1u;
	return 0;
}

/* 0x804f04 */
int bigmacMemsetAsync(void *dst, uint32_t size, int fillByte, BigmacContext *ctx)
{
	uint32_t ch;

	if (size - 1u >= 0x00ffffffu || (((uintptr_t)dst & 3u) != 0 && size >= 0x40u))
		return (int)0x800f0016;

	ch = ctx->channel & 1u;
	BIGMAC->ch[ch].dst = (uint32_t)(uintptr_t)dst;
	BIGMAC->ch[ch].length = size;
	BIGMAC->ch[ch].fillValue = (uint32_t)fillByte;
	BIGMAC->ch[ch].func = (ctx->flags & ~0x3fu) | 0x0cu;
	BIGMAC->ch[ch].trigger = 1u;
	return 0;
}

/* 0x804f7c */
int bigmacMemcpy(void *dst, const void *src, uint32_t size)
{
	BigmacContext ctx;
	int status;

	bigmacInitContext(&ctx);
	status = bigmacMemcpyAsync(dst, src, size, &ctx);
	if (status == 0)
		status = bigmacWaitChannel(&ctx);
	return status;
}

/* 0x804fb4 */
int bigmacMemset(void *dst, uint32_t size, int fillByte)
{
	BigmacContext ctx;
	int status;

	bigmacInitContext(&ctx);
	status = bigmacMemsetAsync(dst, size, fillByte, &ctx);
	if (status == 0)
		status = bigmacWaitChannel(&ctx);
	return status;
}

/* 0x804fec */
int bigmacStartAesCipher(void *dst, const void *src, uint32_t size,
		const void *keyOrSlot, void *iv, uint32_t dirMode,
		uint32_t chainMode, uint32_t keySizeIdx, BigmacContext *ctx)
{
	uint32_t keySizes[4];
	uint32_t ch;
	uint32_t funcVal;

	slMemcpy(keySizes, s_bigmacKeySizeBytes, sizeof(keySizes));
	if (((uintptr_t)dst & 3u) != 0 || (((uintptr_t)src | (uintptr_t)iv) & 3u) != 0)
		return (int)0x800f0016;

	ch = ctx->channel & 1u;
	BIGMAC->ch[ch].src = (uint32_t)(uintptr_t)src;
	BIGMAC->ch[ch].dst = (uint32_t)(uintptr_t)dst;
	BIGMAC->ch[ch].length = size;

	funcVal = (ctx->flags & 0xfffffcc0u) |
		  (dirMode & 7u) |
		  ((chainMode & 7u) << 3) |
		  ((keySizeIdx & 3u) << 8);
	BIGMAC->ch[ch].func = funcVal;

	if (iv != NULL)
		BIGMAC->ch[ch].iv = (uint32_t)(uintptr_t)iv;

	if ((funcVal & 0x80u) == 0) {
		BIGMAC->ch[ch].keyslot = *(const uint32_t *)keyOrSlot;
	} else {
		uint32_t keyBytes = keySizes[keySizeIdx];
		volatile uint32_t *extKey = &BIGMAC->externalKey[ch * 32u];
		const uint32_t *kWords = (const uint32_t *)keyOrSlot;
		uint32_t i;

		for (i = 0; i < (keyBytes >> 2); i++)
			extKey[i] = kWords[i];
	}
	BIGMAC->ch[ch].trigger = 1u;
	return 0;
}

/* 0x805106 */
int bigmacMemcpyAsync(void *dst, const void *src, uint32_t size, BigmacContext *ctx)
{
	uint32_t ch;

	if (size - 1u > 0x00fffffeu)
		return (int)0x800f0016;

	ch = ctx->channel & 1u;
	BIGMAC->ch[ch].src = (uint32_t)(uintptr_t)src;
	BIGMAC->ch[ch].dst = (uint32_t)(uintptr_t)dst;
	BIGMAC->ch[ch].length = size;
	BIGMAC->ch[ch].func = ctx->flags & ~7u;
	BIGMAC->ch[ch].trigger = 1u;
	return 0;
}
