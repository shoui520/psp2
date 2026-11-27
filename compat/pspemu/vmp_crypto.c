/*
 * vmp_crypto.c: ScePspemu PS1 VMP ("\0PMV") header HMAC-SHA1 signing,
 * verification, and AES-128-CBC/CFB key derivation (0x8103a88c..0x8103ae8f).
 */
#include "host_common.h"

#include <libaes.h>
#include <libsha1.h>
#include <string.h>

#define VMP_MAGIC               0x564D5000U /* "\0PMV" */
#define VMP_HEADER_SIZE         0x80U
#define VMP_IMAGE_TOTAL_SIZE    0x20080ULL
#define VMP_SEED_SIZE           20U
#define VMP_DIGEST_SIZE         20U

#define VMP_ERR_NULL_PTR        ((int)0x80000000)
#define VMP_ERR_VERIFY_FAIL     ((int)0x80000001)
#define VMP_ERR_UNSUPPORTED_FMT ((int)0x80000002)
#define VMP_ERR_KEY_DERIVE      ((int)0x8001050F)

typedef struct VmpFileHeader {
	uint32_t magic;
	uint32_t headerSize;
	uint32_t flags;
	uint8_t seed[VMP_SEED_SIZE];
	uint8_t digest[VMP_DIGEST_SIZE];
	uint8_t reserved[76];
} VmpFileHeader;

_Static_assert(sizeof(VmpFileHeader) == VMP_HEADER_SIZE, "VmpFileHeader size");

typedef struct VmpHmacContext {
	SceSha1Context shaCtx;
	uint8_t innerDigest[VMP_DIGEST_SIZE];
	uint8_t keyPad[64];
	uint8_t trailingPad[4];
} VmpHmacContext;

_Static_assert(offsetof(VmpHmacContext, innerDigest) == 120U, "innerDigest offset");
_Static_assert(offsetof(VmpHmacContext, keyPad) == 140U, "keyPad offset");
_Static_assert(sizeof(VmpHmacContext) == 0xD0U, "VmpHmacContext size");

typedef struct VmpAesCbcWork {
	SceAesContext aesCtx;
	uint8_t iv[32];
} VmpAesCbcWork;

_Static_assert(offsetof(VmpAesCbcWork, iv) == 976U, "VmpAesCbcWork iv offset");
_Static_assert(sizeof(VmpAesCbcWork) == 0x3F0U, "VmpAesCbcWork size");

static const uint8_t g_vmpAesIv[16] = {
	0xB3U, 0x0FU, 0xFEU, 0xEDU, 0xB7U, 0xDCU, 0x5EU, 0xB7U,
	0x13U, 0x3DU, 0xA6U, 0x0DU, 0x1BU, 0x6BU, 0x2CU, 0xDCU
};

static const uint8_t g_vmpAesKey[16] = {
	0xABU, 0x5AU, 0xBCU, 0x9FU, 0xC1U, 0xF4U, 0x9DU, 0xE6U,
	0xA0U, 0x51U, 0xDBU, 0xAEU, 0xFAU, 0x51U, 0x88U, 0x59U
};

static uint8_t g_vmpSeedCounter;

static int __attribute__((noinline, noclone)) vmpGenerateSeed(uint8_t *outSeed);
static int __attribute__((noinline, noclone)) vmpHmacSessionInit(VmpHmacContext *ctx, int keyMode, const uint8_t *seed);
static int __attribute__((noinline, noclone)) vmpHmacSessionUpdate(VmpHmacContext *ctx, uint64_t size, const void *data);
static int __attribute__((noinline, noclone)) vmpHmacSessionFinal(VmpHmacContext *ctx, uint8_t *outDigest);
static int __attribute__((noinline, noclone)) vmpHmacInitWrapper(VmpHmacContext *ctx, const uint8_t *key);
static int __attribute__((noinline, noclone)) vmpHmacUpdateWrapper(VmpHmacContext *ctx, uint32_t size, const void *data);
static int __attribute__((noinline, noclone)) vmpHmacFinalWrapper(VmpHmacContext *ctx, uint8_t *outDigest);
static int __attribute__((noinline, noclone)) vmpHmacSha1Init(VmpHmacContext *ctx, const void *key, size_t keyLen);
static int __attribute__((noinline, noclone)) vmpHmacSha1Update(VmpHmacContext *ctx, const void *data, uint32_t size);
static int __attribute__((noinline, noclone)) vmpHmacSha1Final(uint8_t *outDigest, VmpHmacContext *ctx);
static int __attribute__((noinline, noclone)) vmpFillSeedBytes(uint8_t *outBuf, int unusedType, int length);
static int __attribute__((noinline, noclone)) vmpAltKeyDeriveStub(int mode, const void *key, void *buf);
static int __attribute__((noinline, noclone)) vmpAesCbcCfbDecrypt(uint8_t *dst, const uint8_t *src, uint32_t size,
                                                                   const void *key, uint32_t keyBits, const uint8_t *iv);
static int __attribute__((noinline, noclone)) vmpDecryptSeedInPlace(uint8_t *buf, uint32_t unusedArg, uint64_t size,
                                                                     const uint8_t *iv, const uint8_t *key);

/* 0x8103a88c */
int vmpSignHeader(void *vmpBuf)
{
	VmpFileHeader *hdr = (VmpFileHeader *)vmpBuf;
	uint8_t seed[VMP_SEED_SIZE];
	uint8_t digest[VMP_DIGEST_SIZE];
	VmpHmacContext hmacCtx;
	int ret;

	if (hdr == NULL)
		return VMP_ERR_NULL_PTR;

	memset(hdr, 0, sizeof(*hdr));
	hdr->magic = VMP_MAGIC;
	hdr->headerSize = VMP_HEADER_SIZE;
	hdr->flags = 0U;

	ret = vmpGenerateSeed(seed);
	if (ret < 0)
		return ret;

	memcpy(hdr->seed, seed, sizeof(seed));

	ret = vmpHmacSessionInit(&hmacCtx, 0, seed);
	if (ret != 0)
		return ret;

	ret = vmpHmacSessionUpdate(&hmacCtx, VMP_IMAGE_TOTAL_SIZE, hdr);
	if (ret != 0)
		return ret;

	ret = vmpHmacSessionFinal(&hmacCtx, digest);
	if (ret == 0)
		memcpy(hdr->digest, digest, sizeof(digest));
	return ret;
}

/* 0x8103a940 */
int vmpVerifyHeader(void *vmpBuf, uint32_t *outHeaderSize)
{
	VmpFileHeader *hdr = (VmpFileHeader *)vmpBuf;
	uint8_t savedDigest[VMP_DIGEST_SIZE];
	uint8_t seedCopy[VMP_SEED_SIZE];
	uint8_t computedDigest[VMP_DIGEST_SIZE];
	VmpHmacContext hmacCtx;
	uint32_t hdrSize;
	int ret;

	if (hdr == NULL || outHeaderSize == NULL)
		return VMP_ERR_NULL_PTR;

	*outHeaderSize = 0U;
	if (hdr->magic != VMP_MAGIC)
		return VMP_ERR_VERIFY_FAIL;
	if (hdr->flags != 0U)
		return VMP_ERR_UNSUPPORTED_FMT;

	hdrSize = hdr->headerSize;
	memcpy(savedDigest, hdr->digest, sizeof(savedDigest));
	memset(hdr->digest, 0, sizeof(hdr->digest));
	memcpy(seedCopy, hdr->seed, sizeof(seedCopy));

	ret = vmpHmacSessionInit(&hmacCtx, 0, seedCopy);
	if (ret < 0)
		return ret;

	ret = vmpHmacSessionUpdate(&hmacCtx, VMP_IMAGE_TOTAL_SIZE, hdr);
	if (ret != 0)
		return ret;

	ret = vmpHmacSessionFinal(&hmacCtx, computedDigest);
	if (ret != 0)
		return ret;

	if (memcmp(computedDigest, savedDigest, sizeof(savedDigest)) != 0)
		return VMP_ERR_VERIFY_FAIL;

	*outHeaderSize = hdrSize;
	memcpy(hdr->digest, savedDigest, sizeof(savedDigest));
	return 0;
}

/* 0x8103aa48 */
static int __attribute__((noinline, noclone))
vmpDeriveHmacKey(uint8_t *outKey, const uint8_t *seed, int keyMode)
{
	if (keyMode == 0) {
		memcpy(outKey, seed, VMP_SEED_SIZE);
		vmpDecryptSeedInPlace(outKey, 0U, VMP_SEED_SIZE, g_vmpAesIv, g_vmpAesKey);
		return 0;
	}

	if (keyMode == 1) {
		uint8_t localIv[16];
		uint8_t workBuf[64];
		int ret;

		memcpy(outKey, seed, VMP_SEED_SIZE);
		memcpy(localIv, g_vmpAesIv, sizeof(localIv));
		memset(workBuf, 0, sizeof(workBuf));
		memcpy(workBuf, seed, VMP_SEED_SIZE);

		ret = vmpAltKeyDeriveStub(3, localIv, workBuf);
		if (ret != 0)
			return ret;
		memcpy(outKey, workBuf, VMP_SEED_SIZE);
	}

	return 0;
}

/* 0x8103ab18 */
static int __attribute__((noinline, noclone)) vmpGenerateSeed(uint8_t *outSeed)
{
	return vmpFillSeedBytes(outSeed, 0, (int)VMP_SEED_SIZE);
}

/* 0x8103ab24 */
static int __attribute__((noinline, noclone))
vmpHmacSessionInit(VmpHmacContext *ctx, int keyMode, const uint8_t *seed)
{
	uint8_t derivedKey[32];

	if (vmpDeriveHmacKey(derivedKey, seed, keyMode) != 0)
		return VMP_ERR_KEY_DERIVE;

	vmpHmacInitWrapper(ctx, derivedKey);
	return 0;
}

/* 0x8103ab6c */
static int __attribute__((noinline, noclone))
vmpHmacSessionUpdate(VmpHmacContext *ctx, uint64_t size, const void *data)
{
	vmpHmacUpdateWrapper(ctx, (uint32_t)size, data);
	return 0;
}

/* 0x8103ab7c */
static int __attribute__((noinline, noclone))
vmpHmacSessionFinal(VmpHmacContext *ctx, uint8_t *outDigest)
{
	uint8_t digest[VMP_DIGEST_SIZE];

	vmpHmacFinalWrapper(ctx, digest);
	memcpy(outDigest, digest, sizeof(digest));
	memset(ctx, 0, sizeof(*ctx));
	return 0;
}

/* 0x8103abc4 */
static int __attribute__((noinline, noclone))
vmpHmacInitWrapper(VmpHmacContext *ctx, const uint8_t *key)
{
	vmpHmacSha1Init(ctx, key, VMP_SEED_SIZE);
	return 1;
}

/* 0x8103abd0 */
static int __attribute__((noinline, noclone))
vmpHmacUpdateWrapper(VmpHmacContext *ctx, uint32_t size, const void *data)
{
	vmpHmacSha1Update(ctx, data, size);
	return 1;
}

/* 0x8103abe0 */
static int __attribute__((noinline, noclone))
vmpHmacFinalWrapper(VmpHmacContext *ctx, uint8_t *outDigest)
{
	vmpHmacSha1Final(outDigest, ctx);
	return 1;
}

/* 0x8103abf0 */
static int __attribute__((noinline, noclone))
vmpHmacSha1Init(VmpHmacContext *ctx, const void *key, size_t keyLen)
{
	int idx;

	memset(ctx, 0, sizeof(*ctx));
	if (keyLen > sizeof(ctx->keyPad))
		sceSha1Digest(key, (SceUInt32)keyLen, ctx->keyPad);
	else
		memcpy(ctx->keyPad, key, keyLen);

	for (idx = 0; idx < (int)sizeof(ctx->keyPad); idx++)
		ctx->keyPad[idx] ^= 0x36U;

	sceSha1BlockInit(&ctx->shaCtx);
	sceSha1BlockUpdate(&ctx->shaCtx, ctx->keyPad, sizeof(ctx->keyPad));
	return 0;
}

/* 0x8103ac48 */
static int __attribute__((noinline, noclone))
vmpHmacSha1Update(VmpHmacContext *ctx, const void *data, uint32_t size)
{
	sceSha1BlockUpdate(&ctx->shaCtx, data, size);
	return 0;
}

/* 0x8103ac54 */
static int __attribute__((noinline, noclone))
vmpHmacSha1Final(uint8_t *outDigest, VmpHmacContext *ctx)
{
	int idx;

	sceSha1BlockResult(&ctx->shaCtx, ctx->innerDigest);
	for (idx = 0; idx < (int)sizeof(ctx->keyPad); idx++)
		ctx->keyPad[idx] ^= 0x6AU;

	sceSha1BlockInit(&ctx->shaCtx);
	sceSha1BlockUpdate(&ctx->shaCtx, ctx->keyPad, sizeof(ctx->keyPad));
	sceSha1BlockUpdate(&ctx->shaCtx, ctx->innerDigest, sizeof(ctx->innerDigest));
	sceSha1BlockResult(&ctx->shaCtx, outDigest);
	return 0;
}

/* 0x8103aca8 */
static int __attribute__((noinline, noclone))
vmpFillSeedBytes(uint8_t *outBuf, int unusedType, int length)
{
	(void)unusedType;
	if (length > 0) {
		uint8_t base = g_vmpSeedCounter;
		int idx;

		for (idx = 0; idx < length; idx++)
			outBuf[idx] = (uint8_t)(base + (uint8_t)idx);
		g_vmpSeedCounter = (uint8_t)(base + (uint8_t)length);
	}
	return 0;
}

/* 0x8103acd0 */
static int __attribute__((noinline, noclone))
vmpAltKeyDeriveStub(int mode, const void *key, void *buf)
{
	(void)mode;
	(void)key;
	(void)buf;
	return 0;
}

/* 0x8103acd4 */
static int __attribute__((noinline, noclone))
vmpAesCbcCfbDecrypt(uint8_t *dst, const uint8_t *src, uint32_t size,
                    const void *key, uint32_t keyBits, const uint8_t *iv)
{
	VmpAesCbcWork decWork;
	VmpAesCbcWork cfbWork;
	uint8_t cfbFeedback[32];
	uint8_t blockTmp[32];
	uint32_t blockBytes;
	uint32_t fullBlocks;
	uint32_t tailChunks;
	uint32_t blkIdx;
	uint32_t byteIdx;

	(void)keyBits;
	memset(&decWork, 0, sizeof(decWork));
	if (sceAesScheduleKey(&decWork.aesCtx, 128U, 128U, key) >= 0) {
		blockBytes = decWork.aesCtx.blockWords * 4U;
		for (byteIdx = 0; byteIdx < blockBytes; byteIdx++)
			decWork.iv[byteIdx] = iv[byteIdx];
	}

	tailChunks = (size & 0x0FU) / decWork.aesCtx.blockWords;
	fullBlocks = size >> 4;

	for (blkIdx = 0; blkIdx < fullBlocks; blkIdx++) {
		const uint8_t *inBlk = &src[blkIdx * 16U];
		uint8_t *outBlk = &dst[blkIdx * 16U];

		sceAesDecrypt(&decWork.aesCtx, inBlk, blockTmp);
		blockBytes = decWork.aesCtx.blockWords * 4U;
		for (byteIdx = 0; byteIdx < blockBytes; byteIdx++) {
			uint8_t prevIv = decWork.iv[byteIdx];
			uint8_t cipherByte = inBlk[byteIdx];
			outBlk[byteIdx] = blockTmp[byteIdx] ^ prevIv;
			decWork.iv[byteIdx] = cipherByte;
		}
	}

	if (sceAesScheduleKey(&cfbWork.aesCtx, 128U, 128U, key) >= 0) {
		blockBytes = cfbWork.aesCtx.blockWords * 4U;
		for (byteIdx = 0; byteIdx < blockBytes; byteIdx++)
			cfbWork.iv[byteIdx] = decWork.iv[byteIdx];
	}

	if (tailChunks != 0U) {
		const uint8_t *tailSrc = &src[fullBlocks * 16U];
		uint8_t *tailDst = &dst[fullBlocks * 16U];
		uint32_t chunkIdx;

		memcpy(cfbFeedback, cfbWork.iv, sizeof(cfbFeedback));
		for (chunkIdx = 0; chunkIdx < tailChunks; chunkIdx++) {
			sceAesEncrypt(&cfbWork.aesCtx, cfbFeedback, blockTmp);
			blockBytes = cfbWork.aesCtx.blockWords * 4U;
			for (byteIdx = 0; byteIdx < blockBytes; byteIdx++) {
				uint8_t cipherByte = tailSrc[chunkIdx + byteIdx];
				cfbFeedback[byteIdx] = cipherByte;
				tailDst[chunkIdx + byteIdx] = cipherByte ^ blockTmp[byteIdx];
			}
		}
	}

	return 0;
}

/* 0x8103ae34 */
static int __attribute__((noinline, noclone))
vmpDecryptSeedInPlace(uint8_t *buf, uint32_t unusedArg, uint64_t size,
                      const uint8_t *iv, const uint8_t *key)
{
	uint8_t seedCopy[VMP_SEED_SIZE];

	(void)unusedArg;
	if (size != VMP_SEED_SIZE)
		return -1;

	memcpy(seedCopy, buf, sizeof(seedCopy));
	vmpAesCbcCfbDecrypt(buf, seedCopy, VMP_SEED_SIZE, key, 128U, iv);
	return 0;
}
