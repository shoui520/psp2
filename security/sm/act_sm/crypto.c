/*
 * crypto.c: Bigmac AES-256-CBC / AES-CMAC / SHA-256 / DMA driver, MeP hardware
 * Bignum modular exponentiation driver (0xe0040000), and ASN.1 DER PKCS#1 v1.5
 * RSA signature verification for act_sm.self (0x80bd6a..0x80c64c, 0x80cd04..0x80cd1e).
 *
 * Internal function and structure names are ours.
 */

#include "act_sm.h"

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

/* ============================================================================
 * .rodata (0x80cfa4..0x80d0a0)
 * ============================================================================ */

/* 0x80cfa4: AES external key size in bytes indexed by keyBits (1=16, 2=24, 3=32) */
static const uint32_t s_bigmacAesKeySizes[4] = {
	0x00u, 0x10u, 0x18u, 0x20u
};

/* 0x80cfb4..0x80d01c: ASN.1 DigestInfo algorithm OIDs */
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

/* 0x80d01c..0x80d0a0: ASN.1 DigestInfo OID lookup table */
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

int asn1ParseElement(const uint8_t *buf, uint32_t bufSize,
		SceAsn1Element *outElem);
int asn1ParseSequence(const uint8_t *buf, uint32_t bufSize,
		const SceAsn1Element *seqElem, SceAsn1Element *outChildren,
		uint32_t *inOutCount);

/* ============================================================================
 * Bigmac Crypto & RSA Functions (0x80bd6a..0x80c64c)
 * ============================================================================ */

/* 0x80bd6a (name ours) */
int bigmacInitChannel0(uint32_t *ctrl)
{
	ctrl[0] = 0u;
	ctrl[1] = 0x2080u;
	return 0;
}

/* 0x80bd7a (name ours) */
int bigmacGetChannelStatus(const uint32_t *ctrl, uint32_t *outStatus)
{
	*outStatus = BIGMAC->ch[ctrl[0] & 1u].status;
	return 0;
}

/* 0x80bd94 (name ours) */
int bigmacWaitChannelDone(const uint32_t *ctrl)
{
	uint32_t ch = ctrl[0] & 1u;

	while ((BIGMAC->ch[ch].status & 1u) != 0u)
		;
	if (BIGMAC->ch[ch].status != 0u)
		return (int)0x800f0005;
	return 0;
}

/* 0x80bdc2 (name ours) */
int bigmacStartAes256CbcEncrypt(void *dst, const void *src, uint32_t size,
		const void *key, const void *iv, const uint32_t *ctrl)
{
	return bigmacStartAesCipher(dst, src, size, key, iv, 1u, 1u, 3u, ctrl);
}

/* 0x80bde6 (name ours) */
int bigmacStartAes256CbcDecrypt(void *dst, const void *src, uint32_t size,
		const void *key, const void *iv, const uint32_t *ctrl)
{
	return bigmacStartAesCipher(dst, src, size, key, iv, 2u, 1u, 3u, ctrl);
}

/* 0x80be0c (name ours) */
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

/* 0x80be46 (name ours) */
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

/* 0x80be98 (name ours) */
int bigmacStartAesCmac(void *dstMac, const void *src, uint32_t size,
		const void *key, uint32_t keyByteLen, const uint32_t *ctrl)
{
	uint32_t ch = ctrl[0];
	uint32_t flags = ctrl[1];
	uint32_t chIdx;
	uint32_t funcVal;

	if (keyByteLen != 16u && keyByteLen != 24u && keyByteLen != 32u)
		return (int)0x800f0016;
	if (size == 0u || (size & 0x0fu) != 0u)
		return (int)0x800f0016;

	chIdx = ch & 1u;
	BIGMAC->ch[chIdx].src = (uint32_t)(uintptr_t)src;
	BIGMAC->ch[chIdx].dst = (uint32_t)(uintptr_t)dstMac;
	BIGMAC->ch[chIdx].length = size;

	funcVal = (flags & ~7u) | 0x3bu;
	if (keyByteLen == 24u)
		funcVal = (funcVal & ~0x300u) | 0x200u;
	else if (keyByteLen == 32u)
		funcVal |= 0x300u;
	else if (keyByteLen == 16u)
		funcVal = (funcVal & ~0x300u) | 0x100u;

	BIGMAC->ch[chIdx].func = funcVal & ~0x0c00u;
	if ((funcVal & 0x80u) == 0u) {
		BIGMAC->ch[chIdx].keySlot = *(const uint32_t *)key;
	} else {
		volatile uint32_t *extKey = &BIGMAC->externalKey[chIdx * 32u];
		const uint32_t *kWords;

		for (kWords = (const uint32_t *)key;
		     (uint32_t)((uintptr_t)kWords - (uintptr_t)key) < keyByteLen;
		     kWords++) {
			*extKey++ = *kWords;
		}
	}

	BIGMAC->ch[chIdx].trigger = 1u;
	return 0;
}

/* 0x80bf8e (name ours) */
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

/* 0x80bfc6 (name ours) */
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

/* 0x80c0e0 (name ours) */
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

/* 0x80c14a (name ours) */
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

/* 0x80c194 (name ours) */
int bignumInitState(uint32_t *state)
{
	*state = 0u;
	return 0;
}

/* 0x80c19c (name ours) */
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

/* 0x80c350 (name ours) */
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

/* 0x80c36e (name ours) */
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

/* 0x80c4de (name ours) */
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

/* 0x80c540 (name ours) */
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

/* 0x80c5c8 (name ours) */
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

/* 0x80cd04 (name ours) */
int timingsafeBcmp(const void *b1, const void *b2, size_t len)
{
	const uint8_t *p1 = (const uint8_t *)b1;
	const uint8_t *p2 = (const uint8_t *)b2;
	const uint8_t *end = p1 + len;
	uint32_t diff = 0u;

	while (p1 != end)
		diff |= (uint32_t)(*p1++ ^ *p2++);

	return (diff != 0u) ? 1 : 0;
}
