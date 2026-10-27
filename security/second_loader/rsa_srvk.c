/*
 * security/second_loader/rsa_srvk.c
 *
 * QAF token cryptographic helpers, Bigmac SHA-256 wrapper, MeP hardware
 * Bignum modular exponentiation driver (0xe0040000), and ASN.1 DER
 * PKCS#1 v1.5 RSA signature verification (0x805304..0x805ad7).
 */

#include "second_loader.h"

/* ASN.1 DigestInfo algorithm OIDs (.rodata 0x80c078..0x80c0df, names ours) */
static const uint8_t s_oidMd2[8] __attribute__((aligned(4))) = {
	0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02, 0x02
};
static const uint8_t s_oidMd5[8] __attribute__((aligned(4))) = {
	0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02, 0x05
};
static const uint8_t s_oidSha1[5] __attribute__((aligned(4))) = {
	0x2b, 0x0e, 0x03, 0x02, 0x1a
};
static const uint8_t s_oidSha224[9] __attribute__((aligned(4))) = {
	0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x04
};
static const uint8_t s_oidSha256[9] __attribute__((aligned(4))) = {
	0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01
};
static const uint8_t s_oidSha384[9] __attribute__((aligned(4))) = {
	0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02
};
static const uint8_t s_oidSha512[9] __attribute__((aligned(4))) = {
	0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03
};
static const uint8_t s_oidHmacSha224[8] __attribute__((aligned(4))) = {
	0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02, 0x08
};
static const uint8_t s_oidHmacSha256[8] __attribute__((aligned(4))) = {
	0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02, 0x09
};
static const uint8_t s_oidHmacSha384[8] __attribute__((aligned(4))) = {
	0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02, 0x0a
};
static const uint8_t s_oidHmacSha512[8] __attribute__((aligned(4))) = {
	0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02, 0x0b
};

/* ASN.1 DigestInfo OID lookup table (.rodata 0x80c0e0..0x80c163, name ours) */
static const SceAsn1DigestOid s_digestOidTable[11] = {
	{ s_oidMd2,        8, 0x10 },
	{ s_oidMd5,        8, 0x10 },
	{ s_oidSha1,       5, 0x14 },
	{ s_oidSha224,     9, 0x1c },
	{ s_oidSha256,     9, 0x20 },
	{ s_oidSha384,     9, 0x30 },
	{ s_oidSha512,     9, 0x40 },
	{ s_oidHmacSha224, 8, 0x1c },
	{ s_oidHmacSha256, 8, 0x20 },
	{ s_oidHmacSha384, 8, 0x30 },
	{ s_oidHmacSha512, 8, 0x40 },
};

/*
 * qafDecryptTokenBody - Decrypts the 0x60-byte QAF token body at +0x20 using
 * AES-256-CBC and copies the 0x20-byte header.
 * Address: 0x805304..0x805381 (name ours)
 */
int qafDecryptTokenBody(const void *src, void *dst, const void *key, const void *iv)
{
	SceBigmacContext ctx;
	uint8_t localIv[16];
	int ret;

	slMemcpy(localIv, iv, sizeof(localIv));
	bigmacInit(&ctx);
	ret = bigmacAes256CbcDecrypt((uint8_t *)dst + 0x20, (const uint8_t *)src + 0x20,
				     0x60, key, localIv, &ctx);
	if (ret == 0) {
		ret = bigmacWait(&ctx);
		if (ret == 0)
			slMemcpy(dst, src, 0x20);
	}
	return ret;
}

/*
 * qafVerifyTokenCmac - Verifies the 16-byte AES-256-CMAC at offset +0x70 of a
 * 0x80-byte QAF token structure.
 * Address: 0x805382..0x805413 (name ours)
 */
int qafVerifyTokenCmac(void *tokenBuf, const void *key)
{
	SceBigmacContext ctx;
	uint8_t calcMac[16];
	uint8_t expectedMac[16];
	uint8_t *macField;
	int ret;

	macField = (uint8_t *)tokenBuf + 0x70;
	slMemcpy(expectedMac, macField, sizeof(expectedMac));
	memset32(macField, 0, sizeof(expectedMac));
	bigmacInit(&ctx);
	ret = bigmacAesCmac(calcMac, tokenBuf, 0x80, key, 0x20, &ctx);
	if (ret == 0) {
		ret = bigmacWait(&ctx);
		if (ret == 0) {
			if (slTimingsafeBcmp(calcMac, expectedMac, sizeof(calcMac)) != 0)
				ret = SCE_SBL_ERROR_QAF_CMAC;
		}
	}
	return ret;
}

/*
 * qafVerifyTokenRsa2048 - Verifies the RSA-2048 PKCS#1 v1.5 SHA-256 signature
 * over a 0x80-byte QAF token.
 * Address: 0x805414..0x805499 (name ours)
 */
int qafVerifyTokenRsa2048(const void *tokenBuf, const void *sig, const SceBignumRsaKey *pubKey)
{
	uint8_t workBuf[256];
	uint8_t digest[32];
	int ret;

	memset32(workBuf, 0, sizeof(workBuf));
	if (bigmacSha256(digest, tokenBuf, 0x80) != 0)
		return SCE_SBL_ERROR_COMMON_EAUTH;

	ret = verifyRsaPkcs1v15Blocking(workBuf, sig, pubKey, digest, 4);
	if (ret > 0)
		ret = 0;
	return ret;
}

/*
 * qafVerifyTokenChallengeCmac - Computes AES-256-CMAC over a 16-byte challenge
 * and checks equality with the 16-byte MAC at +0x10.
 * Address: 0x80549a..0x80550f (name ours)
 */
int qafVerifyTokenChallengeCmac(const void *challengeBuf, const void *key)
{
	SceBigmacContext ctx;
	uint8_t calcMac[16];
	int ret;

	memset32(calcMac, 0xaa, sizeof(calcMac));
	bigmacInit(&ctx);
	ret = bigmacAesCmac(calcMac, challengeBuf, 0x10, key, 0x20, &ctx);
	if (ret == 0) {
		ret = bigmacWait(&ctx);
		if (ret == 0) {
			ret = (slTimingsafeBcmp((const uint8_t *)challengeBuf + 0x10,
						calcMac, sizeof(calcMac)) == 0);
		}
	}
	return ret;
}

/*
 * qafCheckTokenByte5EqualsByte4 - Compares byte +5 of tokenBuf against byte +4
 * of refBuf.
 * Address: 0x805510..0x80551d (name ours)
 */
int qafCheckTokenByte5EqualsByte4(const void *tokenBuf, const void *refBuf)
{
	return ((const uint8_t *)tokenBuf)[5] == ((const uint8_t *)refBuf)[4];
}

/*
 * qafVerifyTokenOpenPsId - Verifies that the 16-byte OpenPSID at +0x20 in the
 * QAF token matches the console's OpenPSID.
 * Address: 0x80551e..0x805541 (name ours)
 */
int qafVerifyTokenOpenPsId(const void *tokenBuf, const void *openPsId)
{
	if (slTimingsafeBcmp((const uint8_t *)tokenBuf + 0x20, openPsId, 0x10) == 0)
		return 0;
	return SCE_SBL_ERROR_QAF_OPENPSID;
}

/*
 * bigmacSha256 - Computes a synchronous SHA-256 digest using Bigmac.
 * Address: 0x805542..0x80557b (name ours)
 */
int bigmacSha256(void *dstDigest, const void *src, uint32_t size)
{
	SceBigmacContext ctx;
	int ret;

	bigmacInit(&ctx);
	ret = bigmacStartHash(dstDigest, src, size, 2, &ctx);
	if (ret == 0)
		ret = bigmacWait(&ctx);
	return ret;
}

/*
 * verifyRsaPkcs1v15Blocking - Blocking wrapper around verifyRsaPkcs1v15Step.
 * Address: 0x80557c..0x8055cd (name ours)
 */
int verifyRsaPkcs1v15Blocking(void *workBuf, const void *sig, const SceBignumRsaKey *key,
			      const void *hash, uint32_t hashAlgo)
{
	uint32_t state;
	int ret;

	bignumInitState(&state);
	for (;;) {
		ret = verifyRsaPkcs1v15Step(workBuf, sig, key, hash, hashAlgo, &state);
		if (ret != SCE_SBL_ERROR_COMMON_EBUSY)
			break;
		bignumWaitInterrupt();
	}
	return ret;
}

/*
 * bigmacStartHash - Starts an unkeyed hash operation on the Bigmac engine.
 * Address: 0x8055ce..0x805637 (name ours)
 */
int bigmacStartHash(void *dstDigest, const void *src, uint32_t size, uint32_t algo,
		    SceBigmacContext *ctx)
{
	uint32_t flags = ctx->flags;
	uint32_t chOffset;
	volatile uint32_t *regs;

	if (((uintptr_t)src & 3) != 0)
		return SCE_SBL_ERROR_COMMON_EINVAL;

	chOffset = (ctx->channel & 1) << 7;
	regs = (volatile uint32_t *)(SCE_BIGMAC_BASE + chOffset);
	regs[0] = (uint32_t)src;
	regs[1] = (uint32_t)dstDigest;
	regs[2] = size;
	regs[3] = ((algo & 7) << 3) | (flags & 0xfffff3c0u) | 3u;
	regs[7] = 1;
	return 0;
}

/*
 * bignumCopyWordsReverseBswap - Copies up to 0x42 32-bit words in reverse order
 * with big-endian to little-endian byte swapping into Bignum SRAM.
 * Address: 0x805638..0x805681 (name ours)
 */
int bignumCopyWordsReverseBswap(volatile uint32_t *dst, const uint32_t *src, uint32_t wordCount)
{
	const uint32_t *srcPtr;
	uint32_t i;

	if (wordCount > 0x42)
		return SCE_SBL_ERROR_COMMON_EINVAL;

	srcPtr = src + (wordCount - 1);
	for (i = wordCount; i != 0; i--) {
		uint32_t val = *srcPtr--;

		*dst++ = (val >> 24) | (val << 24) |
			 ((val & 0xff00u) << 8) | ((val >> 8) & 0xff00u);
	}
	return 0;
}

/*
 * bignumInitState - Initializes the Bignum step state word to zero.
 * Address: 0x805682..0x805689 (name ours)
 */
int bignumInitState(uint32_t *state)
{
	*state = 0;
	return 0;
}

/*
 * bignumModExpStep - Executes one step of modular exponentiation on the MeP
 * hardware Bignum engine (0xe0040000).
 * Address: 0x80568a..0x80583d (name ours)
 */
int bignumModExpStep(void *dst, const void *base, const SceBignumRsaKey *key, uint32_t *state)
{
	volatile uint32_t * const irqStatus = (volatile uint32_t *)0xe0020020u;
	volatile uint32_t * const bnBase    = (volatile uint32_t *)0xe0040108u;
	volatile uint32_t * const bnModulus = (volatile uint32_t *)0xe0040400u;
	volatile uint32_t * const bnResult  = (volatile uint32_t *)0xe0040508u;
	volatile uint32_t * const bnCtrl    = (volatile uint32_t *)0xe0040800u;
	volatile uint32_t * const bnStatus  = (volatile uint32_t *)0xe0040804u;
	volatile uint32_t * const bnExpFifo = (volatile uint32_t *)0xe0040808u;
	uint32_t status;

	if (key->modWordLen <= 2)
		return SCE_SBL_ERROR_COMMON_EINVAL;

	if (*irqStatus != 0)
		return SCE_SBL_ERROR_COMMON_EBUSY;

	status = *bnStatus;
	if (*state == 0) {
		if ((int32_t)status < 0)
			return SCE_SBL_ERROR_COMMON_EBUSY;
		if (bignumCopyWordsReverseBswap(bnBase, base, key->modWordLen) != 0)
			return 0x800f0116;
		if (bignumCopyWordsReverseBswap(bnModulus, key->modulus, key->modWordLen) != 0)
			return 0x800f0116;

		*bnCtrl = ((key->modWordLen & 0xffu) << 18) |
			  ((key->expWordLen & 0x1ffu) << 9) |
			  0x90000000u;
		do {
			status = *bnStatus;
		} while ((status & 0x04000000u) == 0);
	}

	while (((status = *bnStatus) & 0x02000000u) == 0) {
		if (*state >= key->expWordLen) {
			uint32_t outWords;
			uint32_t *dstPtr;
			volatile uint32_t *srcPtr;
			uint32_t i;

			if ((int32_t)status < 0)
				return SCE_SBL_ERROR_COMMON_EBUSY;

			outWords = (*bnStatus & 0x00ff0000u) >> 16;
			if (outWords != key->modWordLen || outWords > 0x42)
				return 0x800f0116;

			dstPtr = (uint32_t *)dst + (outWords - 1);
			srcPtr = bnResult;
			for (i = outWords; i != 0; i--) {
				uint32_t val = *srcPtr++;

				*dstPtr-- = (val >> 24) | (val << 24) |
					    ((val & 0xff00u) << 8) | ((val >> 8) & 0xff00u);
			}
			return 0;
		}

		if ((status & 0x04000000u) == 0)
			return SCE_SBL_ERROR_COMMON_EBUSY;

		{
			uint32_t expWord = key->exponent[*state];

			*bnExpFifo = (expWord >> 24) | (expWord << 24) |
				     ((expWord & 0xff00u) << 8) | ((expWord >> 8) & 0xff00u);
			(*state)++;
		}
	}

	return 0x800f0116;
}

/*
 * bignumWaitInterrupt - Waits for and acknowledges the Bignum interrupt flag
 * at 0xe0020020.
 * Address: 0x80583e..0x80585b (name ours)
 */
int bignumWaitInterrupt(void)
{
	volatile uint32_t * const irqStatus = (volatile uint32_t *)0xe0020020u;
	uint32_t val;

	do {
		val = *irqStatus;
	} while (val == 0);
	*irqStatus = val;
	return 0;
}

/*
 * verifyRsaPkcs1v15Step - Performs modular exponentiation and validates
 * PKCS#1 v1.5 type-1/type-0 padding and ASN.1 DigestInfo structure.
 * Address: 0x80585c..0x8059cb (name ours)
 */
int verifyRsaPkcs1v15Step(uint8_t *workBuf, const void *sig, const SceBignumRsaKey *key,
			  const void *hash, uint32_t hashAlgo, uint32_t *state)
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
	int ret;

	if (hashAlgo > 10)
		return SCE_SBL_ERROR_COMMON_EINVAL;

	ret = bignumModExpStep(workBuf, sig, key, state);
	if (ret != 0)
		return ret;

	padTable[0] = 0x00;
	padTable[1] = 0xff;
	modByteLen = key->modWordLen * 4;
	if (modByteLen <= 11 || workBuf[0] != 0x00 || workBuf[1] > 1)
		return SCE_SBL_ERROR_COMMON_EILLEGAL;

	padByte = padTable[workBuf[1]];
	idx = 2;
	for (rem = 8; rem != 0; rem--) {
		if (workBuf[idx] != padByte)
			return SCE_SBL_ERROR_COMMON_EILLEGAL;
		idx++;
	}

	rem = (modByteLen < 11) ? 0 : (~idx + modByteLen);
	rem++;
	do {
		if (workBuf[idx] != padByte)
			break;
		idx++;
		rem--;
	} while (rem != 0);

	if (idx == modByteLen)
		return SCE_SBL_ERROR_COMMON_EILLEGAL;

	if (padByte == 0xff) {
		if (workBuf[idx] != 0x00)
			return SCE_SBL_ERROR_COMMON_EILLEGAL;
		idx++;
	}

	if (asn1ParseElement(workBuf + idx, modByteLen - idx, &rootElem) != 0 ||
	    rootElem.tag != 0x30)
		return SCE_SBL_ERROR_COMMON_EILLEGAL;

	childCount = 2;
	if (asn1ParseSequence(workBuf + idx, modByteLen - idx, &rootElem,
			      topChildren, &childCount) != 0 ||
	    childCount != 2 || topChildren[0].tag != 0x30)
		return SCE_SBL_ERROR_COMMON_EILLEGAL;

	if (asn1ParseSequence(rootElem.data, topChildren[0].totalLen, &topChildren[0],
			      digestInfoChildren, &childCount) != 0)
		return SCE_SBL_ERROR_COMMON_EILLEGAL;

	if (digestInfoChildren[0].tag == 0x06 &&
	    digestInfoChildren[0].contentLen == s_digestOidTable[hashAlgo].oidLen &&
	    slTimingsafeBcmp(digestInfoChildren[0].data,
			     s_digestOidTable[hashAlgo].oid,
			     digestInfoChildren[0].contentLen) == 0 &&
	    topChildren[1].tag == 0x04 &&
	    topChildren[1].contentLen == s_digestOidTable[hashAlgo].hashLen &&
	    slTimingsafeBcmp(topChildren[1].data, hash, topChildren[1].contentLen) == 0) {
		return 0;
	}

	return SCE_SBL_ERROR_COMMON_EILLEGAL;
}

/*
 * asn1ParseElement - Parses a single DER/BER TLV element header.
 * Address: 0x8059cc..0x805a53 (name ours)
 */
int asn1ParseElement(const uint8_t *buf, uint32_t bufSize, SceAsn1Element *outElem)
{
	uint8_t lenByte;
	uint32_t lenBytesCount;

	if (bufSize < 2)
		return SCE_SBL_ERROR_COMMON_EINVAL;

	lenByte = buf[1];
	if ((int8_t)lenByte < 0) {
		uint32_t shift;
		uint32_t i;
		const uint8_t *p;

		lenBytesCount = lenByte & 0x7fu;
		if (bufSize < lenBytesCount + 2)
			return SCE_SBL_ERROR_COMMON_EINVAL;
		if (lenBytesCount > 4)
			return SCE_SBL_ERROR_COMMON_ENOSUP;

		outElem->contentLen = 0;
		shift = lenBytesCount * 8 - 24;
		p = buf;
		for (i = lenBytesCount; i != 0; i--) {
			outElem->contentLen |= (uint32_t)p[2] << (shift & 0x1fu);
			shift -= 8;
			p++;
		}
	} else {
		outElem->contentLen = lenByte;
		lenBytesCount = 0;
	}

	outElem->tag = buf[0];
	outElem->totalLen = outElem->contentLen + 2 + lenBytesCount;
	outElem->data = buf + lenBytesCount + 2;
	return 0;
}

/*
 * asn1ParseSequence - Parses up to *inOutCount child TLV elements inside an
 * ASN.1 SEQUENCE (tag 0x30).
 * Address: 0x805a54..0x805ad7 (name ours)
 */
int asn1ParseSequence(const uint8_t *buf, uint32_t bufSize, const SceAsn1Element *seqElem,
		      SceAsn1Element *outChildren, uint32_t *inOutCount)
{
	const uint8_t *curPtr;
	uint32_t remSize;
	uint32_t count;

	if (seqElem->tag != 0x30 || seqElem->totalLen > bufSize)
		return SCE_SBL_ERROR_COMMON_EILLEGAL;

	curPtr = buf + (seqElem->totalLen - seqElem->contentLen);
	remSize = (seqElem->contentLen - seqElem->totalLen) + bufSize;
	for (count = 0; count < *inOutCount && remSize != 0; count++) {
		uint32_t elemLen;
		int ret = asn1ParseElement(curPtr, remSize, &outChildren[count]);

		if (ret != 0)
			return ret;

		elemLen = outChildren[count].totalLen;
		if (remSize < elemLen)
			return SCE_SBL_ERROR_COMMON_EILLEGAL;

		curPtr += elemLen;
		remSize -= elemLen;
	}

	*inOutCount = count;
	return 0;
}
