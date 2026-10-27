/*
 * rsa_srvk.c: Bigmac AES-ECB / AES-CBC / AES-CTR / SHA-256 / HMAC-SHA256
 * driver, MeP hardware Bignum modular exponentiation driver (0xe0040000), and
 * ASN.1 DER PKCS#1 v1.5 RSA signature verification (0x80ef02..0x80fcf4).
 *
 * Internal function and structure names are ours.
 */

#include "kprx_auth_sm.h"

typedef struct SceAsn1Element {
	uint32_t tag;				/* 0x00 */
	uint32_t totalLen;			/* 0x04 */
	uint32_t contentLen;			/* 0x08 */
	const uint8_t *data;			/* 0x0c */
} SceAsn1Element;

typedef struct SceAsn1DigestOid {
	const uint8_t *oid;			/* 0x00 */
	uint32_t oidLen;			/* 0x04 */
	uint32_t hashLen;			/* 0x08 */
} SceAsn1DigestOid;

typedef struct BigmacChannelRegs {
	volatile uint32_t src;			/* 0x00 */
	volatile uint32_t dst;			/* 0x04 */
	volatile uint32_t length;		/* 0x08 */
	volatile uint32_t func;			/* 0x0c */
	volatile uint32_t keySlot;		/* 0x10 */
	volatile uint32_t iv;			/* 0x14 */
	volatile uint32_t rng;			/* 0x18 */
	volatile uint32_t trigger;		/* 0x1c */
	volatile uint32_t status;		/* 0x20 */
	volatile uint32_t rsvd24[23];		/* 0x24..0x7f */
} BigmacChannelRegs;

typedef struct BigmacRegs {
	BigmacChannelRegs ch[2];		/* 0x000..0x0ff */
	volatile uint32_t rsvd100[64];		/* 0x100..0x1ff */
	volatile uint32_t externalKey[64];	/* 0x200..0x2ff */
} BigmacRegs;

#define BIGMAC ((BigmacRegs *)(uintptr_t)0xe0050000u)

/* ============================================================================
 * .rodata (0x810808..0x810910)
 * ============================================================================ */

/* 0x810808: AES external key size in bytes indexed by keyBits (1=16, 2=24, 3=32) */
static const uint32_t s_bigmacAesKeySizes[4] = {
	0x00u, 0x10u, 0x18u, 0x20u
};

/* 0x810818: HMAC digest size in bytes indexed by (hashAlgo - 4) */
static const uint32_t s_hmacDigestSizes[3] = {
	20u, 28u, 32u
};

/* 0x810824..0x81088c: ASN.1 DigestInfo algorithm OIDs */
static const uint8_t s_oidMd2[8] __attribute__((aligned(4))) = {
	0x2au, 0x86u, 0x48u, 0x86u, 0xf7u, 0x0du, 0x02u, 0x02u
};
static const uint8_t s_oidMd5[8] __attribute__((aligned(4))) = {
	0x2au, 0x86u, 0x48u, 0x86u, 0xf7u, 0x0du, 0x02u, 0x05u
};
static const uint8_t s_oidSha1[5] __attribute__((aligned(4))) = {
	0x2bu, 0x0eu, 0x03u, 0x02u, 0x1au
};
static const uint8_t s_oidSha224[9] __attribute__((aligned(4))) = {
	0x60u, 0x86u, 0x48u, 0x01u, 0x65u, 0x03u, 0x04u, 0x02u, 0x04u
};
static const uint8_t s_oidSha256[9] __attribute__((aligned(4))) = {
	0x60u, 0x86u, 0x48u, 0x01u, 0x65u, 0x03u, 0x04u, 0x02u, 0x01u
};
static const uint8_t s_oidSha384[9] __attribute__((aligned(4))) = {
	0x60u, 0x86u, 0x48u, 0x01u, 0x65u, 0x03u, 0x04u, 0x02u, 0x02u
};
static const uint8_t s_oidSha512[9] __attribute__((aligned(4))) = {
	0x60u, 0x86u, 0x48u, 0x01u, 0x65u, 0x03u, 0x04u, 0x02u, 0x03u
};
static const uint8_t s_oidHmacSha224[8] __attribute__((aligned(4))) = {
	0x2au, 0x86u, 0x48u, 0x86u, 0xf7u, 0x0du, 0x02u, 0x08u
};
static const uint8_t s_oidHmacSha256[8] __attribute__((aligned(4))) = {
	0x2au, 0x86u, 0x48u, 0x86u, 0xf7u, 0x0du, 0x02u, 0x09u
};
static const uint8_t s_oidHmacSha384[8] __attribute__((aligned(4))) = {
	0x2au, 0x86u, 0x48u, 0x86u, 0xf7u, 0x0du, 0x02u, 0x0au
};
static const uint8_t s_oidHmacSha512[8] __attribute__((aligned(4))) = {
	0x2au, 0x86u, 0x48u, 0x86u, 0xf7u, 0x0du, 0x02u, 0x0bu
};

/* 0x81088c..0x810910: ASN.1 DigestInfo OID lookup table */
static const SceAsn1DigestOid s_digestOidTable[11] = {
	{ s_oidMd2,        8u, 0x10u },
	{ s_oidMd5,        8u, 0x10u },
	{ s_oidSha1,       5u, 0x14u },
	{ s_oidSha224,     9u, 0x1cu },
	{ s_oidSha256,     9u, 0x20u },
	{ s_oidSha384,     9u, 0x30u },
	{ s_oidSha512,     9u, 0x40u },
	{ s_oidHmacSha224, 8u, 0x1cu },
	{ s_oidHmacSha256, 8u, 0x20u },
	{ s_oidHmacSha384, 8u, 0x30u },
	{ s_oidHmacSha512, 8u, 0x40u },
};

/* Forward declarations of internal helpers */
int bigmacStartHash(void *dstDigest, const void *src, uint32_t size,
		uint32_t hashSubMode, const uint32_t *bmCtrl);
int bigmacStartHmacOneShot(void *dstDigest, const void *src, uint32_t size,
		const void *key64, uint32_t keyLen, uint32_t hashAlgo,
		const uint32_t *bmCtrl);
int bigmacHmacInit(uint32_t hashAlgo, CfHmacContext *ctx, const void *key64,
		uint32_t keyLen, uint32_t *bmCtrl);
int bigmacHmacStartHw(uint32_t hashAlgo, CfHmacContext *ctx, const void *src,
		uint32_t size, uint32_t isFinal, const uint32_t *bmCtrl);
int bigmacHmacUpdateBlock(uint32_t hashAlgo, CfHmacContext *ctx, const void *data,
		uint32_t size, uint32_t *outConsumed, uint32_t isFinal,
		const uint32_t *bmCtrl);
int bigmacHmacExtractDigest(uint32_t hashAlgo, uint8_t *outDigest,
		const CfHmacContext *ctx, const uint32_t *bmCtrl);
int bignumCopyWordsReverseBswap(volatile uint32_t *dst,
		const uint32_t *src, uint32_t wordCount);
int bignumInitState(uint32_t *state);
int bignumModExpStep(void *dst, const void *base,
		const BignumPublicKey *key, uint32_t *state);
int bignumWaitInterrupt(void);
int verifyRsaPkcs1v15Step(uint8_t *workBuf, const void *sig,
		const BignumPublicKey *key, const uint8_t *hash,
		uint32_t hashAlgo, uint32_t *state);
int bigmacStartMemcpy(void *dst, const void *src, uint32_t size,
		const uint32_t *bmCtrl);
int asn1ParseElement(const uint8_t *buf, uint32_t bufSize,
		SceAsn1Element *outElem);
int asn1ParseSequence(const uint8_t *buf, uint32_t bufSize,
		const SceAsn1Element *seqElem, SceAsn1Element *outChildren,
		uint32_t *inOutCount);

/* ============================================================================
 * Bigmac Crypto & RSA Functions (0x80ef02..0x80fcf4)
 * ============================================================================ */

/* 0x80ef02 (name ours) */
int bigmacInitChannel0(uint32_t *ctrl)
{
	ctrl[0] = 0u;
	ctrl[1] = 0x2080u;
	return 0;
}

/* 0x80ef12 (name ours) */
int bigmacGetChannelStatus(const uint32_t *ctrl, uint32_t *outStatus)
{
	*outStatus = BIGMAC->ch[ctrl[0] & 1u].status;
	return 0;
}

/* 0x80ef2c (name ours) */
int bigmacWaitChannelDone(const uint32_t *ctrl)
{
	uint32_t ch = ctrl[0] & 1u;

	while ((BIGMAC->ch[ch].status & 1u) != 0u)
		;
	if (BIGMAC->ch[ch].status != 0u)
		return (int)0x800f0005;
	return 0;
}

/* 0x80ef5a (name ours) */
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
		return (int)0x800f0016;

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

/* 0x80f074 (name ours) */
int bigmacStartAes128EcbEncrypt(void *dst, const void *src, uint32_t size,
		const void *key, const uint32_t *ctrl)
{
	return bigmacStartAesCipher(dst, src, size, key, NULL, 1u, 0u, 1u, ctrl);
}

/* 0x80f096 (name ours) */
int bigmacStartAes128EcbDecrypt(void *dst, const void *src, uint32_t size,
		const void *key, const uint32_t *ctrl)
{
	return bigmacStartAesCipher(dst, src, size, key, NULL, 2u, 0u, 1u, ctrl);
}

/* 0x80f0ba (name ours) */
int bigmacStartAes128CbcEncrypt(void *dst, const void *src, uint32_t size,
		const void *key, const void *iv, const uint32_t *ctrl)
{
	return bigmacStartAesCipher(dst, src, size, key, iv, 1u, 1u, 1u, ctrl);
}

/* 0x80f0dc (name ours) */
int bigmacStartAes128CbcDecrypt(void *dst, const void *src, uint32_t size,
		const void *key, const void *iv, const uint32_t *ctrl)
{
	return bigmacStartAesCipher(dst, src, size, key, iv, 2u, 1u, 1u, ctrl);
}

/* 0x80f100 (name ours) */
void bigmacReverseBswap128(uint32_t *words4)
{
	uint32_t w0 = words4[0];
	uint32_t w3 = words4[3];
	uint32_t w1;
	uint32_t w2;

	words4[3] = (w0 >> 24) | (w0 << 24) | ((w0 & 0xff00u) << 8) | ((w0 >> 8) & 0xff00u);
	w1 = words4[1];
	words4[0] = (w3 >> 24) | (w3 << 24) | ((w3 & 0xff00u) << 8) | ((w3 >> 8) & 0xff00u);
	w2 = words4[2];
	words4[2] = (w1 >> 24) | (w1 << 24) | ((w1 & 0xff00u) << 8) | ((w1 >> 8) & 0xff00u);
	words4[1] = (w2 >> 24) | (w2 << 24) | ((w2 & 0xff00u) << 8) | ((w2 >> 8) & 0xff00u);
}

/* 0x80f182 (name ours) */
int bigmacAes128EcbEncryptSync(void *dst, const void *src, uint32_t size,
		const void *key)
{
	uint32_t bmCtrl[2];
	int status;

	bigmacInitChannel0(bmCtrl);
	status = bigmacStartAesCipher(dst, src, size, key, NULL, 1u, 0u, 1u, bmCtrl);
	if (status == 0)
		status = bigmacWaitChannelDone(bmCtrl);
	return status;
}

/* 0x80f1ca (name ours) */
int bigmacAes128EcbDecryptSync(void *dst, const void *src, uint32_t size,
		const void *key)
{
	uint32_t bmCtrl[2];
	int status;

	bigmacInitChannel0(bmCtrl);
	status = bigmacStartAesCipher(dst, src, size, key, NULL, 2u, 0u, 1u, bmCtrl);
	if (status == 0)
		status = bigmacWaitChannelDone(bmCtrl);
	return status;
}

/* 0x80f214 (name ours) */
int bigmacAes128CbcEncryptSync(void *dst, const void *src, uint32_t size,
		const void *key, void *iv)
{
	uint32_t bmCtrl[2];
	int status;

	bigmacInitChannel0(bmCtrl);
	status = bigmacStartAesCipher(dst, src, size, key, iv, 1u, 1u, 1u, bmCtrl);
	if (status == 0)
		status = bigmacWaitChannelDone(bmCtrl);
	return status;
}

/* 0x80f25c (name ours) */
int bigmacAes128CbcDecryptSync(void *dst, const void *src, uint32_t size,
		const void *key, void *iv)
{
	uint32_t bmCtrl[2];
	int status;

	bigmacInitChannel0(bmCtrl);
	status = bigmacStartAesCipher(dst, src, size, key, iv, 2u, 1u, 1u, bmCtrl);
	if (status == 0)
		status = bigmacWaitChannelDone(bmCtrl);
	return status;
}

/* 0x80f2a6 (name ours) */
int bigmacSha256(void *dstDigest, const void *src, uint32_t size)
{
	uint32_t bmCtrl[2];
	int status;

	bigmacInitChannel0(bmCtrl);
	status = bigmacStartHash(dstDigest, src, size, 2u, bmCtrl);
	if (status == 0)
		status = bigmacWaitChannelDone(bmCtrl);
	return status;
}

/* 0x80f2e0 (name ours) */
int bigmacHmacSha256OneShotStart(void *dstDigest, const void *src, uint32_t size,
		const void *key64, uint32_t keyLen, const uint32_t *bmCtrl)
{
	return bigmacStartHmacOneShot(dstDigest, src, size, key64, keyLen, 6u, bmCtrl);
}

/* 0x80f2fc (name ours) */
int bigmacHmacSha256Init(CfHmacContext *ctx, const void *key64, uint32_t keyLen)
{
	uint32_t bmCtrl[2];

	bigmacInitChannel0(bmCtrl);
	return bigmacHmacInit(6u, ctx, key64, keyLen, bmCtrl);
}

/* 0x80f336 (name ours) */
int bigmacHmacSha256Update(CfHmacContext *ctx, const void *data, uint32_t size,
		uint32_t isFinalFlag)
{
	uint32_t bmCtrl[2];
	uint32_t consumed = 0u;
	uint32_t offset = 0u;
	int status;

	bigmacInitChannel0(bmCtrl);
	while ((int32_t)size > 0) {
		status = bigmacHmacUpdateBlock(6u, ctx,
				(const uint8_t *)data + offset,
				size, &consumed, isFinalFlag, bmCtrl);
		if (status != 0)
			return status;
		status = bigmacWaitChannelDone(bmCtrl);
		if (status != 0)
			return status;
		offset += consumed;
		size -= consumed;
	}
	return (size == 0u) ? 0 : (int)0x800f0005;
}

/* 0x80f3a2 (name ours) */
int bigmacHmacSha256Final(void *outDigest32, CfHmacContext *ctx)
{
	uint32_t bmCtrl[2];

	bigmacInitChannel0(bmCtrl);
	return bigmacHmacExtractDigest(6u, (uint8_t *)outDigest32, ctx, bmCtrl);
}

/* 0x80f3d0 (name ours) */
int bignumVerifyRsaPkcs1v15(void *workBuf256, const void *signature256,
		const BignumPublicKey *pubKey, const void *expectedDigest,
		uint32_t oidIndex)
{
	uint32_t state;
	int status;

	bignumInitState(&state);
	for (;;) {
		status = verifyRsaPkcs1v15Step((uint8_t *)workBuf256, signature256,
				pubKey, (const uint8_t *)expectedDigest,
				oidIndex, &state);
		if (status != (int)0x800f0010)
			break;
		bignumWaitInterrupt();
	}
	return status;
}

/* 0x80f422 (name ours) */
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

/* 0x80f45a (name ours) */
int bigmacStartHash(void *dstDigest, const void *src, uint32_t size,
		uint32_t hashSubMode, const uint32_t *bmCtrl)
{
	uint32_t ch = bmCtrl[0];
	uint32_t flags = bmCtrl[1];
	uint32_t chIdx;

	if (((uintptr_t)src & 3u) != 0u)
		return (int)0x800f0016;

	chIdx = ch & 1u;
	BIGMAC->ch[chIdx].src = (uint32_t)(uintptr_t)src;
	BIGMAC->ch[chIdx].dst = (uint32_t)(uintptr_t)dstDigest;
	BIGMAC->ch[chIdx].length = size;
	BIGMAC->ch[chIdx].func = ((hashSubMode & 7u) << 3) | (flags & 0xfffff3c0u) | 3u;
	BIGMAC->ch[chIdx].trigger = 1u;
	return 0;
}

/* 0x80f4c4 (name ours) */
int bigmacStartHmacOneShot(void *dstDigest, const void *src, uint32_t size,
		const void *key64, uint32_t keyLen, uint32_t hashAlgo,
		const uint32_t *bmCtrl)
{
	uint32_t ch = bmCtrl[0];
	uint32_t flags = bmCtrl[1];
	uint32_t chIdx;
	volatile uint32_t *extKey;
	const uint32_t *kWords = (const uint32_t *)key64;
	uint32_t i;

	if (keyLen != 64u || ((uintptr_t)src & 3u) != 0u)
		return (int)0x800f0016;

	chIdx = ch & 1u;
	BIGMAC->ch[chIdx].src = (uint32_t)(uintptr_t)src;
	BIGMAC->ch[chIdx].dst = (uint32_t)(uintptr_t)dstDigest;
	BIGMAC->ch[chIdx].length = size;
	BIGMAC->ch[chIdx].func = (((hashAlgo & 7u) << 3) | (flags & ~0x3fu) | 3u) & ~0x0c00u;

	extKey = &BIGMAC->externalKey[chIdx * 32u];
	for (i = 0u; i < 16u; i++)
		*extKey++ = *kWords++;

	BIGMAC->ch[chIdx].trigger = 1u;
	return 0;
}

/* 0x80f546 (name ours) */
int bigmacHmacInit(uint32_t hashAlgo, CfHmacContext *ctx, const void *key64,
		uint32_t keyLen, uint32_t *bmCtrl)
{
	uint32_t i;

	(void)hashAlgo;
	(void)bmCtrl;
	if (keyLen != 64u)
		return (int)0x800f0016;

	ctx->state = 0u;
	for (i = 0u; i < 32u; i++)
		ctx->digest[i] = 0u;
	for (i = 0u; i < 64u; i++)
		ctx->blockBuf[i] = 0u;
	ctx->bufUsed = 0u;
	memcpy(ctx->hmacKey, key64, 64u);
	ctx->keyLen = 64u;
	return 0;
}

/* 0x80f5ac (name ours) */
int bigmacHmacStartHw(uint32_t hashAlgo, CfHmacContext *ctx, const void *src,
		uint32_t size, uint32_t isFinal, const uint32_t *bmCtrl)
{
	uint32_t ch = bmCtrl[0];
	uint32_t flags = bmCtrl[1];
	uint32_t chIdx;
	uint32_t funcVal;
	volatile uint32_t *extKey;
	const uint32_t *kWords;

	if ((isFinal == 0u && (size & 0x3fu) != 0u) ||
	    size == 0u || ((uintptr_t)src & 3u) != 0u || ctx->keyLen != 64u) {
		return (int)0x800f0016;
	}

	chIdx = ch & 1u;
	BIGMAC->ch[chIdx].src = (uint32_t)(uintptr_t)src;
	BIGMAC->ch[chIdx].dst = (uint32_t)(uintptr_t)ctx->digest;
	BIGMAC->ch[chIdx].length = size;

	funcVal = ((hashAlgo & 7u) << 3) | (flags & ~0x3fu) | 3u;
	if (isFinal == 0u) {
		if (ctx->state != 0u) {
			funcVal |= 0x0c00u;
		} else {
			funcVal = (funcVal & ~0x0c00u) | 0x0400u;
			ctx->state = 1u;
		}
	} else {
		funcVal &= ~0x0c00u;
		if (ctx->state != 0u)
			funcVal |= 0x0800u;
		ctx->state = 2u;
	}

	BIGMAC->ch[chIdx].func = funcVal;
	BIGMAC->ch[chIdx].iv = (uint32_t)(uintptr_t)ctx->iv;

	extKey = &BIGMAC->externalKey[chIdx * 32u];
	for (kWords = (const uint32_t *)ctx->hmacKey;
	     (uint32_t)((uintptr_t)kWords - (uintptr_t)ctx->hmacKey) < ctx->keyLen;
	     kWords++) {
		*extKey++ = *kWords;
	}
	BIGMAC->ch[chIdx].trigger = 1u;
	return 0;
}

/* 0x80f6a2 (name ours) */
int bigmacHmacUpdateBlock(uint32_t hashAlgo, CfHmacContext *ctx, const void *data,
		uint32_t size, uint32_t *outConsumed, uint32_t isFinal,
		const uint32_t *bmCtrl)
{
	uint32_t bufUsed = ctx->bufUsed;
	int status;

	if (bufUsed > 63u)
		return (int)0x800f0016;

	if (bufUsed == 0u && ((uintptr_t)data & 3u) == 0u) {
		uint32_t alignedSize;

		if (isFinal != 0u) {
			status = bigmacHmacStartHw(hashAlgo, ctx, data, size, isFinal, bmCtrl);
			*outConsumed = (status == 0) ? size : 0u;
			return status;
		}

		alignedSize = size & ~0x3fu;
		if (alignedSize != 0u) {
			status = bigmacHmacStartHw(hashAlgo, ctx, data, alignedSize, 0u, bmCtrl);
			if (status != 0) {
				*outConsumed = 0u;
				return status;
			}
		}
		memcpy(ctx->blockBuf, (const uint8_t *)data + alignedSize, size - alignedSize);
		ctx->bufUsed = size - alignedSize;
		*outConsumed = size;
		return 0;
	} else {
		uint32_t space = 64u - bufUsed;
		uint32_t copyLen = (size <= space) ? size : space;
		uint32_t blockFinal;

		memcpy(ctx->blockBuf + bufUsed, data, copyLen);
		if (copyLen != space && (size != copyLen || isFinal == 0u)) {
			*outConsumed = copyLen;
			ctx->bufUsed += copyLen;
			return 0;
		}

		blockFinal = (size == copyLen && isFinal != 0u) ? 1u : 0u;
		status = bigmacHmacStartHw(hashAlgo, ctx, ctx->blockBuf,
				bufUsed + copyLen, blockFinal, bmCtrl);
		if (status != 0) {
			*outConsumed = 0u;
			return status;
		}
		ctx->bufUsed = 0u;
		*outConsumed = copyLen;
		return 0;
	}
}

/* 0x80f7a2 (name ours) */
int bigmacHmacExtractDigest(uint32_t hashAlgo, uint8_t *outDigest,
		const CfHmacContext *ctx, const uint32_t *bmCtrl)
{
	uint32_t digestSizes[3];
	uint32_t len;
	uint32_t i;

	(void)bmCtrl;
	memcpy(digestSizes, s_hmacDigestSizes, sizeof(digestSizes));
	len = digestSizes[hashAlgo - 4u];
	for (i = 0u; i < len; i++)
		outDigest[i] = ctx->digest[i];
	return 0;
}

/* 0x80f7f2 (name ours) */
int bignumCopyWordsReverseBswap(volatile uint32_t *dst,
		const uint32_t *src, uint32_t wordCount)
{
	const uint32_t *srcPtr;
	uint32_t i;

	if (wordCount > 0x42u)
		return (int)0x800f0016;

	srcPtr = src + (wordCount - 1u);
	for (i = wordCount; i != 0u; i--) {
		uint32_t val = *srcPtr--;

		*dst++ = (val >> 24) | (val << 24) |
			 ((val & 0xff00u) << 8) | ((val >> 8) & 0xff00u);
	}
	return 0;
}

/* 0x80f83c (name ours) */
int bignumInitState(uint32_t *state)
{
	*state = 0u;
	return 0;
}

/* 0x80f844 (name ours) */
int bignumModExpStep(void *dst, const void *base,
		const BignumPublicKey *key, uint32_t *state)
{
	volatile uint32_t * const irqStatus = (volatile uint32_t *)(uintptr_t)0xe0020020u;
	volatile uint32_t * const bnBase    = (volatile uint32_t *)(uintptr_t)0xe0040108u;
	volatile uint32_t * const bnModulus = (volatile uint32_t *)(uintptr_t)0xe0040400u;
	volatile uint32_t * const bnResult  = (volatile uint32_t *)(uintptr_t)0xe0040508u;
	volatile uint32_t * const bnCtrl    = (volatile uint32_t *)(uintptr_t)0xe0040800u;
	volatile uint32_t * const bnStatus  = (volatile uint32_t *)(uintptr_t)0xe0040804u;
	volatile uint32_t * const bnExpFifo = (volatile uint32_t *)(uintptr_t)0xe0040808u;
	uint32_t status;

	if (key->modulusWords <= 2u)
		return (int)0x800f0016;
	if (*irqStatus != 0u)
		return (int)0x800f0010;

	status = *bnStatus;
	if (*state == 0u) {
		if ((int32_t)status < 0)
			return (int)0x800f0010;
		if (bignumCopyWordsReverseBswap(bnBase, (const uint32_t *)base,
				key->modulusWords) != 0) {
			return (int)0x800f0116;
		}
		if (bignumCopyWordsReverseBswap(bnModulus, key->modulus,
				key->modulusWords) != 0) {
			return (int)0x800f0116;
		}

		*bnCtrl = ((key->modulusWords & 0xffu) << 18) |
			  ((key->exponentWords & 0x1ffu) << 9) |
			  0x90000000u;
		do {
			status = *bnStatus;
		} while ((status & 0x04000000u) == 0u);
	}

	while (((status = *bnStatus) & 0x02000000u) == 0u) {
		if (*state >= key->exponentWords) {
			uint32_t outWords;
			uint32_t *dstPtr;
			volatile uint32_t *srcPtr;
			uint32_t i;

			if ((int32_t)status < 0)
				return (int)0x800f0010;

			outWords = (*bnStatus & 0x00ff0000u) >> 16;
			if (outWords != key->modulusWords || outWords > 0x42u)
				return (int)0x800f0116;

			dstPtr = (uint32_t *)dst + (outWords - 1u);
			srcPtr = bnResult;
			for (i = outWords; i != 0u; i--) {
				uint32_t val = *srcPtr++;

				*dstPtr-- = (val >> 24) | (val << 24) |
					    ((val & 0xff00u) << 8) | ((val >> 8) & 0xff00u);
			}
			return 0;
		}

		if ((status & 0x04000000u) == 0u)
			return (int)0x800f0010;

		{
			uint32_t expWord = key->exponent[*state];

			*bnExpFifo = (expWord >> 24) | (expWord << 24) |
				     ((expWord & 0xff00u) << 8) | ((expWord >> 8) & 0xff00u);
			(*state)++;
		}
	}

	return (int)0x800f0116;
}

/* 0x80f9f8 (name ours) */
int bignumWaitInterrupt(void)
{
	volatile uint32_t * const irqStatus = (volatile uint32_t *)(uintptr_t)0xe0020020u;
	uint32_t val;

	do {
		val = *irqStatus;
	} while (val == 0u);
	*irqStatus = val;
	return 0;
}

/* 0x80fa16 (name ours) */
int verifyRsaPkcs1v15Step(uint8_t *workBuf, const void *sig,
		const BignumPublicKey *key, const uint8_t *hash,
		uint32_t hashAlgo, uint32_t *state)
{
	SceAsn1Element digestInfoChildren[2];
	SceAsn1Element topChildren[2];
	SceAsn1Element rootElem;
	uint32_t padTable[2];
	uint32_t childCount;
	uint32_t modByteLen;
	uint32_t padByte;
	uint32_t idx;
	int rem;
	int status;

	if (hashAlgo > 10u)
		return (int)0x800f0016;

	status = bignumModExpStep(workBuf, sig, key, state);
	if (status != 0)
		return status;

	padTable[0] = 0x00u;
	padTable[1] = 0xffu;
	modByteLen = key->modulusWords * 4u;
	if (modByteLen <= 11u || workBuf[0] != 0x00u || workBuf[1] > 1u)
		return (int)0x800f0024;

	padByte = padTable[workBuf[1]];
	idx = 2u;
	for (rem = 8; rem != 0; rem--) {
		if (workBuf[idx] != padByte)
			return (int)0x800f0024;
		idx++;
	}

	rem = (modByteLen < 11u) ? 0 : (int)(~idx + modByteLen);
	rem++;
	do {
		if (workBuf[idx] != padByte)
			break;
		idx++;
		rem--;
	} while (rem != 0);

	if (idx == modByteLen)
		return (int)0x800f0024;

	if (padByte == 0xffu) {
		if (workBuf[idx] != 0x00u)
			return (int)0x800f0024;
		idx++;
	}

	if (asn1ParseElement(workBuf + idx, modByteLen - idx, &rootElem) != 0 ||
	    rootElem.tag != 0x30u) {
		return (int)0x800f0024;
	}

	childCount = 2u;
	if (asn1ParseSequence(workBuf + idx, modByteLen - idx, &rootElem,
			topChildren, &childCount) != 0 ||
	    childCount != 2u || topChildren[0].tag != 0x30u) {
		return (int)0x800f0024;
	}

	if (asn1ParseSequence(rootElem.data, topChildren[0].totalLen, &topChildren[0],
			digestInfoChildren, &childCount) != 0) {
		return (int)0x800f0024;
	}

	if (digestInfoChildren[0].tag == 0x06u &&
	    digestInfoChildren[0].contentLen == s_digestOidTable[hashAlgo].oidLen &&
	    timingsafeBcmp(digestInfoChildren[0].data,
			s_digestOidTable[hashAlgo].oid,
			digestInfoChildren[0].contentLen) == 0 &&
	    topChildren[1].tag == 0x04u &&
	    topChildren[1].contentLen == s_digestOidTable[hashAlgo].hashLen &&
	    timingsafeBcmp(topChildren[1].data, hash, topChildren[1].contentLen) == 0) {
		return 0;
	}

	return (int)0x800f0024;
}

/* 0x80fb86 (name ours) */
int bigmacStartMemcpy(void *dst, const void *src, uint32_t size,
		const uint32_t *bmCtrl)
{
	uint32_t ch = bmCtrl[0];
	uint32_t flags = bmCtrl[1];
	uint32_t chIdx;

	if (size - 1u >= 0x00ffffffu)
		return (int)0x800f0016;

	chIdx = ch & 1u;
	BIGMAC->ch[chIdx].src = (uint32_t)(uintptr_t)src;
	BIGMAC->ch[chIdx].dst = (uint32_t)(uintptr_t)dst;
	BIGMAC->ch[chIdx].length = size;
	BIGMAC->ch[chIdx].func = flags & ~7u;
	BIGMAC->ch[chIdx].trigger = 1u;
	return 0;
}

/* 0x80fbe8 (name ours) */
int asn1ParseElement(const uint8_t *buf, uint32_t bufSize,
		SceAsn1Element *outElem)
{
	uint8_t lenByte;
	uint32_t lenBytesCount;

	if (bufSize < 2u)
		return (int)0x800f0016;

	lenByte = buf[1];
	if ((int8_t)lenByte < 0) {
		uint32_t shift;
		uint32_t i;
		const uint8_t *p;

		lenBytesCount = lenByte & 0x7fu;
		if (bufSize < lenBytesCount + 2u)
			return (int)0x800f0016;
		if (lenBytesCount > 4u)
			return (int)0x800f0025;

		outElem->contentLen = 0u;
		shift = lenBytesCount * 8u - 24u;
		p = buf;
		for (i = lenBytesCount; i != 0u; i--) {
			outElem->contentLen |= (uint32_t)p[2] << (shift & 0x1fu);
			shift -= 8u;
			p++;
		}
	} else {
		outElem->contentLen = lenByte;
		lenBytesCount = 0u;
	}

	outElem->tag = buf[0];
	outElem->totalLen = outElem->contentLen + 2u + lenBytesCount;
	outElem->data = buf + lenBytesCount + 2u;
	return 0;
}

/* 0x80fc70 (name ours) */
int asn1ParseSequence(const uint8_t *buf, uint32_t bufSize,
		const SceAsn1Element *seqElem, SceAsn1Element *outChildren,
		uint32_t *inOutCount)
{
	const uint8_t *curPtr;
	uint32_t remSize;
	uint32_t count;

	if (seqElem->tag != 0x30u || seqElem->totalLen > bufSize)
		return (int)0x800f0024;

	curPtr = buf + (seqElem->totalLen - seqElem->contentLen);
	remSize = (seqElem->contentLen - seqElem->totalLen) + bufSize;
	for (count = 0u; count < *inOutCount && remSize != 0u; count++) {
		uint32_t elemLen;
		int status = asn1ParseElement(curPtr, remSize, &outChildren[count]);

		if (status != 0)
			return status;

		elemLen = outChildren[count].totalLen;
		if (remSize < elemLen)
			return (int)0x800f0024;

		curPtr += elemLen;
		remSize -= elemLen;
	}

	*inOutCount = count;
	return 0;
}
