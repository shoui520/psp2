/*
 * self_auth.c: SCE Certified File (v3) / SELF header authentication, metadata
 * decryption, segment decryption, and Bigmac HMAC-SHA256 / AES-CTR helpers
 * (0x809800..0x80b257).
 *
 * Uses cf_header, SceSelfHeader, SceSelfControlInfo, Elf32_Ehdr, and Elf32_Phdr
 * from sys/internal/include/modulemgr/{certified_file.h,self.h,elf.h}.
 * Internal helper names are ours (descriptive).
 */

#include "second_loader.h"
#include <modulemgr/certified_file.h>
#include <modulemgr/self.h>
#include <modulemgr/elf.h>

/* ============================================================================
 * Certified File (CF) v3 Metadata & Internal Structures (names ours)
 * ============================================================================ */

typedef struct SceCfMetadataHeader {
	uint64_t signatureInputLength;		/* 0x00 */
	uint32_t signatureType;			/* 0x08: 5 = RSA-2048 */
	uint32_t sectionCount;			/* 0x0c */
	uint32_t keyCount;			/* 0x10: count of 16-byte keyPool slots */
	uint32_t optHeaderSize;			/* 0x14 */
	uint64_t reserved;			/* 0x18 */
} SceCfMetadataHeader;

_Static_assert(sizeof(SceCfMetadataHeader) == 0x20, "SceCfMetadataHeader");

typedef struct SceCfMetadataSection {
	uint64_t offset;			/* 0x00 */
	uint64_t size;				/* 0x08 */
	uint32_t type;				/* 0x10: 2 = ELF PT_LOAD segment */
	uint32_t index;				/* 0x14: program header index */
	uint32_t hashAlgorithm;			/* 0x18: 6 = HMAC-SHA256 */
	uint32_t hashKeyIndex;			/* 0x1c: index in 16-byte keyPool */
	uint32_t encryptionAlgorithm;		/* 0x20: 1 = none, 3 = AES-128-CTR */
	uint32_t keyIndex;			/* 0x24: index in 16-byte keyPool */
	uint32_t ivIndex;			/* 0x28: index in 16-byte keyPool */
	uint32_t compressionAlgorithm;		/* 0x2c: 1 = plain, 2 = compressed */
} SceCfMetadataSection;

_Static_assert(sizeof(SceCfMetadataSection) == 0x30, "SceCfMetadataSection");

typedef struct CfHmacContext {
	uint32_t state;				/* 0x00 */
	uint8_t iv[40];				/* 0x04 */
	uint8_t digest[32];			/* 0x2c */
	uint8_t blockBuf[64];			/* 0x4c */
	uint32_t bufUsed;			/* 0x8c */
	uint8_t hmacKey[64];			/* 0x90 */
	uint32_t keyLen;			/* 0xd0 */
} CfHmacContext;

_Static_assert(sizeof(CfHmacContext) == 0xd4, "CfHmacContext");
_Static_assert(offsetof(CfHmacContext, digest) == 0x2c, "CfHmacContext digest");
_Static_assert(offsetof(CfHmacContext, blockBuf) == 0x4c, "CfHmacContext blockBuf");
_Static_assert(offsetof(CfHmacContext, bufUsed) == 0x8c, "CfHmacContext bufUsed");
_Static_assert(offsetof(CfHmacContext, hmacKey) == 0x90, "CfHmacContext hmacKey");
_Static_assert(offsetof(CfHmacContext, keyLen) == 0xd0, "CfHmacContext keyLen");

typedef struct CfStaticKeyParams {
	const void *metaKey;			/* 0x00 */
	const void *rootPubKey;			/* 0x04 */
	uint32_t sigType;			/* 0x08 */
} CfStaticKeyParams;

typedef int (*CfGetKeysCallback)(const uint8_t **outMetaKey, const uint8_t **outRootPubKey,
		uint32_t *outSigType, const uint8_t **outRootKey, void *arg);

typedef struct CfSegmentDecryptState {
	const SelfAuthContext *selfCtx;		/* 0x00 */
	uint32_t reserved04[3];			/* 0x04 */
	SceCfMetadataSection section;		/* 0x10 */
	SelfSegmentReadCallback readCb;		/* 0x40 */
	void *readCbArg;			/* 0x44 */
	SelfSegmentDoneCallback doneCb;		/* 0x48 */
	void *doneCbArg;			/* 0x4c */
	uint64_t curOffset;			/* 0x50 */
	uint64_t remSize;			/* 0x58 */
	uint8_t headBlock[16];			/* 0x60 */
	uint32_t headLen;			/* 0x70 */
	const uint8_t *expectedDigest;		/* 0x74 */
	CfHmacContext hmacCtx;			/* 0x78 */
	const uint8_t *cipherKey;		/* 0x14c */
	uint8_t cipherIv[16];			/* 0x150 */
} CfSegmentDecryptState;

_Static_assert(offsetof(CfSegmentDecryptState, section) == 0x10, "CfSegmentDecryptState section");
_Static_assert(offsetof(CfSegmentDecryptState, readCb) == 0x40, "CfSegmentDecryptState readCb");
_Static_assert(offsetof(CfSegmentDecryptState, curOffset) == 0x50, "CfSegmentDecryptState curOffset");
_Static_assert(offsetof(CfSegmentDecryptState, headBlock) == 0x60, "CfSegmentDecryptState headBlock");
_Static_assert(offsetof(CfSegmentDecryptState, hmacCtx) == 0x78, "CfSegmentDecryptState hmacCtx");
_Static_assert(offsetof(CfSegmentDecryptState, cipherKey) == 0x14c, "CfSegmentDecryptState cipherKey");
_Static_assert(offsetof(CfSegmentDecryptState, cipherIv) == 0x150, "CfSegmentDecryptState cipherIv");

/* ============================================================================
 * .rodata (0x80c4e0..0x80c4f3)
 * ============================================================================ */

/* Standard 20-byte constant in SceSelfControlInfo type 4 (digest info) */
static const uint8_t s_selfControlDigestConstant[20] = {
	0x62u, 0x7cu, 0xb1u, 0x80u, 0x8au, 0xb9u, 0x38u, 0xe3u,
	0x2cu, 0x8cu, 0x09u, 0x17u, 0x08u, 0x72u, 0x6au, 0x57u,
	0x9eu, 0x25u, 0x86u, 0xe4u,
};

/* Forward declarations of internal helpers */
static SceCfMetadataHeader *cfGetMetadataHeader(const SelfAuthContext *ctx);
static SceCfMetadataSection *cfGetMetadataSections(const SelfAuthContext *ctx);
static uint8_t *cfGetMetadataKeyPool(const SelfAuthContext *ctx);
static int cfGetSectionCount(const SelfAuthContext *ctx, uint32_t *outCount);
static int cfGetSectionEntry(const SelfAuthContext *ctx, uint32_t index,
		const SceCfMetadataSection **outSection);
static int cfGetExtHeaderAndMetaOffset(const SelfAuthContext *ctx,
		const uint32_t **outExtHdr, uint32_t *outMetaOffset);
static int cfGetOptHeaders(const SelfAuthContext *ctx,
		const uint8_t **outOptHdr, uint32_t *outOptHdrSize);
static int cfAesCipher(uint32_t chainSelect, uint32_t dirSelect, uint32_t keyBitsSelect,
		void *dst, const void *src, uint32_t size,
		const void *key, const void *ivIn, void *ivOut, void *unusedDesc);
static int cfCopyBytes(void *dst, const void *src, uint32_t size, void *unusedDesc);
static int cfVerifyRsaPkcs1v15(void *workBuf256, const void *signature256,
		const uint32_t *pubKeyWords, const uint8_t *digest, uint32_t hashSubType);
static int cfHmacSha256InitShortKey(CfHmacContext *ctx, const void *key, uint32_t keyLen);
static void cfBswap128(uint32_t *words4);
static int cfHmacSha256InitBlockKey(CfHmacContext *ctx, const void *key64, uint32_t keyLen);
static int cfHmacSha256Update(CfHmacContext *ctx, const void *data,
		uint32_t size, uint32_t isFinalFlag);
static int cfHmacSha256Final(uint8_t *outDigest, CfHmacContext *ctx);
static int bigmacHmacInit(uint32_t hashAlgo, CfHmacContext *ctx, const void *key64,
		uint32_t keyLen, BigmacContext *bmCtx);
static int bigmacHmacStartHw(uint32_t hashAlgo, CfHmacContext *ctx, const void *src,
		uint32_t size, uint32_t isFinal, const BigmacContext *bmCtx);
static int bigmacHmacUpdateBlock(uint32_t hashAlgo, CfHmacContext *ctx, const void *data,
		uint32_t size, uint32_t *outConsumed, uint32_t isFinal,
		const BigmacContext *bmCtx);
static int bigmacHmacExtractDigest(uint32_t hashAlgo, uint8_t *outDigest,
		const CfHmacContext *ctx, const BigmacContext *bmCtx);

/* ============================================================================
 * Functions (0x809800..0x80b257)
 * ============================================================================ */

/* 0x809800 */
int selfVerifySvnAndDecryptMetadata(const SelfAuthContext *ctx)
{
	const uint32_t *extHdr = NULL;
	uint32_t metaOffset = 0;
	uint32_t maxExtEnd;
	uint32_t elfOff;
	uint32_t appInfoOff;
	uint32_t phdrOff;
	uint32_t segInfoOff;
	uint32_t sceVerOff;
	uint32_t ctrlInfoOff;
	uint32_t ctrlInfoSize;
	const uint8_t *base;
	const Elf32_Ehdr *ehdr;
	uint32_t phnum;
	uint32_t phdrTableSize;
	uint32_t fixedHdrOverhead;
	uint32_t secCount = 0;
	const SceCfMetadataSection *sections = NULL;
	const uint8_t *optHdrBase = NULL;
	uint32_t optHdrSize = 0;
	uint32_t sIdx;
	int status;

	if (ctx == NULL)
		return (int)0x800f0616;
	if (cfGetExtHeaderAndMetaOffset(ctx, &extHdr, &metaOffset) != 0)
		return (int)0x800f0616;

	/* Extended header type must be 4 (SELF) */
	if (extHdr[0] != 4u || extHdr[1] != 0u)
		return (int)0x800f0624;

	elfOff = extHdr[4];
	if ((elfOff & 0xfu) != 0)
		return (int)0x800f0624;

	maxExtEnd = metaOffset + 0x30u;
	if (extHdr[5] != 0u || elfOff > maxExtEnd)
		return (int)0x800f0624;
	if (metaOffset < 0x40u)
		return (int)0x800f0624;
	if (elfOff + 0x40u < elfOff || elfOff + 0x40u > maxExtEnd || elfOff <= 0x7fu)
		return (int)0x800f0624;

	base = (const uint8_t *)ctx->scratchBuf;
	sceVerOff = extHdr[12];
	segInfoOff = extHdr[10];
	ehdr = (const Elf32_Ehdr *)(base + elfOff);

	if (ehdr->e_type != ET_EXEC && ehdr->e_type != 0xfe00u &&
	    ehdr->e_type != ET_SCE_FIXEDEXEC) {
		return (int)0x800f0624;
	}

	appInfoOff = extHdr[2];
	if ((appInfoOff & 0xfu) != 0)
		return (int)0x800f0624;
	if (extHdr[3] != 0u || appInfoOff > maxExtEnd ||
	    appInfoOff + 0x20u < appInfoOff || appInfoOff + 0x20u > maxExtEnd ||
	    appInfoOff <= 0x7fu) {
		return (int)0x800f0624;
	}

	if ((sceVerOff & 0xfu) != 0)
		return (int)0x800f0624;
	if (extHdr[13] != 0u || sceVerOff > maxExtEnd ||
	    sceVerOff + 0x10u < sceVerOff || sceVerOff + 0x10u > maxExtEnd ||
	    sceVerOff <= 0x7fu) {
		return (int)0x800f0624;
	}

	ctrlInfoOff = extHdr[14];
	if (ctrlInfoOff != 0u || extHdr[15] != 0u) {
		if ((ctrlInfoOff & 0xfu) != 0 || extHdr[15] != 0u ||
		    ctrlInfoOff > maxExtEnd || ctrlInfoOff + 0x10u < ctrlInfoOff ||
		    ctrlInfoOff + 0x10u > maxExtEnd || ctrlInfoOff <= 0x7fu) {
			return (int)0x800f0624;
		}
	}

	if (ehdr->e_ident[EI_CLASS] != ELFCLASS32)
		return (int)0x800f0624;

	if ((segInfoOff & 0xfu) != 0 || extHdr[11] != 0u || segInfoOff > maxExtEnd)
		return (int)0x800f0624;

	phnum = ehdr->e_phnum;
	phdrTableSize = phnum * 0x20u;
	if (phdrTableSize > metaOffset)
		return (int)0x800f0624;
	if (phdrTableSize + segInfoOff < phdrTableSize ||
	    phdrTableSize + segInfoOff > maxExtEnd || segInfoOff <= 0x7fu) {
		return (int)0x800f0624;
	}

	phdrOff = extHdr[6];
	fixedHdrOverhead = 0xb0u;
	if (phdrOff != 0u || extHdr[7] != 0u) {
		uint32_t alignedPhdrSize = (phdrTableSize + 0xfu) & ~0xfu;

		if ((phdrOff & 0xfu) != 0 || extHdr[7] != 0u || phdrOff > maxExtEnd ||
		    phdrTableSize + 0xfu < phdrTableSize || alignedPhdrSize > metaOffset ||
		    alignedPhdrSize + phdrOff < alignedPhdrSize ||
		    alignedPhdrSize + phdrOff > maxExtEnd || phdrOff <= 0x7fu) {
			return (int)0x800f0624;
		}
		fixedHdrOverhead = alignedPhdrSize + 0xb0u;
	}

	ctrlInfoSize = extHdr[16];
	{
		uint32_t step1 = ctrlInfoSize + 0x10u;
		uint32_t step2 = step1 + phdrTableSize;
		uint32_t verExtra = *(const uint32_t *)(base + sceVerOff + 4u);
		uint64_t totalExt = (uint64_t)ctrlInfoSize + 0x10ULL +
				    (uint64_t)phdrTableSize +
				    (uint64_t)verExtra * 0x30ULL +
				    (uint64_t)fixedHdrOverhead;

		(void)step2;
		if ((totalExt >> 32) != 0 || extHdr[17] != 0u ||
		    (uint32_t)totalExt > metaOffset || ctrlInfoSize > metaOffset) {
			return (int)0x800f0624;
		}
	}

	status = cfGetSectionCount(ctx, &secCount);
	if (status != 0)
		return status;
	status = cfGetSectionEntry(ctx, 0u, &sections);
	if (status != 0)
		return status;

	/* Match each metadata section against SceSelfSegmentInfo entries */
	for (sIdx = 0; sIdx < secCount; sIdx++) {
		const uint32_t *segInfo = (const uint32_t *)(base + segInfoOff);
		const uint32_t *secWords = (const uint32_t *)&sections[sIdx];
		uint32_t pIdx;

		for (pIdx = 0; pIdx < phnum; pIdx++, segInfo += 8) {
			if (secWords[0] == segInfo[0] &&
			    secWords[1] == segInfo[1] &&
			    secWords[2] == segInfo[2] &&
			    secWords[3] == segInfo[3] &&
			    secWords[4] == 2u &&
			    secWords[5] == pIdx &&
			    secWords[11] == segInfo[4]) {
				break;
			}
			if (secWords[8] != 3u || secWords[6] != 6u)
				return (int)0x800f0624;
		}
		if (pIdx == phnum)
			return (int)0x800f0602;
	}

	/* Validate SceSelfControlInfo chain if present */
	{
		const SceSelfHeader *selfHdr = (const SceSelfHeader *)ctx->scratchBuf;

		if (selfHdr->controlInfoOffset != 0ULL) {
			const uint8_t *ctrlBase = (const uint8_t *)selfHdr +
						  (uint32_t)selfHdr->controlInfoOffset;
			uint64_t totalCtrlSize = selfHdr->controlInfoSize;
			uint64_t curCtrlOff = 0ULL;

			while (curCtrlOff < totalCtrlSize) {
				const uint32_t *ci = (const uint32_t *)(ctrlBase + (uint32_t)curCtrlOff);
				uint32_t ciType = ci[0];
				uint32_t entrySize;

				if (ciType == 5u) {
					if (ci[4] != 0x4d52447fu)
						return (int)0x800f0624;
					entrySize = 0x110u;
				} else if (ciType < 6u) {
					if (ciType != 4u)
						return (int)0x800f0624;
					if (slTimingsafeBcmp(&ci[4], s_selfControlDigestConstant, 20u) != 0)
						return (int)0x800f0624;
					if (ci[17] != 0u)
						return (int)0x800f0624;
					entrySize = 0x50u;
				} else if (ciType == 6u) {
					if (ci[4] != 1u)
						return (int)0x800f0624;
					entrySize = 0x110u;
				} else if (ciType == 7u) {
					entrySize = 0x50u;
				} else {
					return (int)0x800f0624;
				}

				curCtrlOff += entrySize;
				if (curCtrlOff > totalCtrlSize)
					return (int)0x800f0624;
				if (ci[2] == 0u && ci[3] == 0u)
					break;
			}
			if (curCtrlOff != totalCtrlSize)
				return (int)0x800f0624;
		}
	}

	/* Validate optional metadata extension headers TLV chain */
	status = cfGetOptHeaders(ctx, &optHdrBase, &optHdrSize);
	if (status != 0)
		return status;

	{
		uint32_t curOptOff = 0u;

		while (curOptOff < optHdrSize) {
			const uint32_t *oh = (const uint32_t *)(optHdrBase + curOptOff);
			uint32_t ohType = oh[0];
			uint32_t nextOptOff;

			if (ohType == 2u) {
				nextOptOff = curOptOff + 0x110u;
			} else if (ohType == 1u || ohType == 3u) {
				nextOptOff = curOptOff + 0x30u;
			} else {
				return (int)0x800f0624;
			}

			if (nextOptOff < curOptOff || nextOptOff > optHdrSize)
				return (int)0x800f0624;
			curOptOff = nextOptOff;
			if (oh[2] == 0u && oh[3] == 0u)
				break;
		}
		if (curOptOff != optHdrSize)
			return (int)0x800f0624;
	}

	return 0;
}

/* 0x809c9a */
int selfGetAppInfo(const SelfAuthContext *ctx, const uint8_t **outAppInfo)
{
	if (ctx == NULL || outAppInfo == NULL)
		return (int)0x800f0616;

	*outAppInfo = (const uint8_t *)ctx->scratchBuf +
		      *(const uint32_t *)((const uint8_t *)ctx->scratchBuf + 0x38u);
	return 0;
}

/* 0x809cb4 */
int selfGetElfHeader(const SelfAuthContext *ctx, const SceElf32Ehdr **outEhdr)
{
	if (ctx == NULL || outEhdr == NULL)
		return (int)0x800f0616;

	*outEhdr = (const SceElf32Ehdr *)((const uint8_t *)ctx->scratchBuf +
		   *(const uint32_t *)((const uint8_t *)ctx->scratchBuf + 0x40u));
	return 0;
}

/* 0x809cce */
int selfGetProgramHeaders(const SelfAuthContext *ctx, const SceElf32Phdr **outPhdrs)
{
	const uint8_t *base;
	uint32_t phdrOffLo;
	uint32_t phdrOffHi;

	if (ctx == NULL || outPhdrs == NULL)
		return (int)0x800f0616;

	base = (const uint8_t *)ctx->scratchBuf;
	phdrOffLo = *(const uint32_t *)(base + 0x48u);
	phdrOffHi = *(const uint32_t *)(base + 0x4cu);
	if (phdrOffLo == 0u && phdrOffHi == 0u)
		return (int)0x800f0602;

	*outPhdrs = (const SceElf32Phdr *)(base + phdrOffLo);
	return 0;
}

/* 0x809cf8 */
int selfGetSegmentInfos(const SelfAuthContext *ctx, const SceSelfSegmentInfo **outSegInfos)
{
	if (ctx == NULL || outSegInfos == NULL)
		return (int)0x800f0616;

	*outSegInfos = (const SceSelfSegmentInfo *)((const uint8_t *)ctx->scratchBuf +
		       *(const uint32_t *)((const uint8_t *)ctx->scratchBuf + 0x58u));
	return 0;
}

/* 0x809d12 */
int selfFindExtHeader(const SelfAuthContext *ctx, const uint8_t **outExtHdr, uint32_t targetType)
{
	const uint8_t *optHdrBase = NULL;
	uint32_t optHdrSize = 0;
	uint32_t curOff;

	if (cfGetOptHeaders(ctx, &optHdrBase, &optHdrSize) != 0)
		return (int)0x800f0602;

	curOff = 0u;
	while (curOff < optHdrSize) {
		const uint32_t *oh = (const uint32_t *)(optHdrBase + curOff);
		uint32_t ohType = oh[0];
		uint32_t nextOff;

		if (ohType == targetType) {
			*outExtHdr = optHdrBase + curOff;
			return 0;
		}
		if (ohType == 2u) {
			nextOff = curOff + 0x110u;
		} else if (ohType == 1u || ohType == 3u) {
			nextOff = curOff + 0x30u;
		} else {
			return (int)0x800f0624;
		}

		if (nextOff < curOff || nextOff > optHdrSize)
			return (int)0x800f0624;
		curOff = nextOff;
		if (oh[2] == 0u && oh[3] == 0u)
			break;
	}

	if (curOff == optHdrSize)
		return (int)0x800f0602;
	return (int)0x800f0624;
}

/* 0x809db8 */
static SceCfMetadataHeader *cfGetMetadataHeader(const SelfAuthContext *ctx)
{
	uint8_t *base = (uint8_t *)ctx->scratchBuf;
	uint32_t metaOffset = *(const uint32_t *)(base + 0x0cu);

	return (SceCfMetadataHeader *)(base + metaOffset + 0x70u);
}

/* 0x809dc6 */
static SceCfMetadataSection *cfGetMetadataSections(const SelfAuthContext *ctx)
{
	return (SceCfMetadataSection *)((uint8_t *)cfGetMetadataHeader(ctx) + 0x20u);
}

/* 0x809dd8 */
static uint8_t *cfGetMetadataKeyPool(const SelfAuthContext *ctx)
{
	uint8_t *sections = (uint8_t *)cfGetMetadataSections(ctx);
	SceCfMetadataHeader *mh = cfGetMetadataHeader(ctx);

	return sections + mh->sectionCount * 0x30u;
}

/* 0x809e04 */
static int cfDefaultGetKeysCallback(const uint8_t **outMetaKey, const uint8_t **outRootPubKey,
		uint32_t *outSigType, const uint8_t **outRootKey, void *arg)
{
	const CfStaticKeyParams *params = (const CfStaticKeyParams *)arg;

	*outMetaKey = (const uint8_t *)params->metaKey;
	*outRootPubKey = (const uint8_t *)params->rootPubKey;
	*outSigType = params->sigType;
	*outRootKey = NULL;
	return 0;
}

/* 0x809e1e */
static int cfGetSignatureSize(uint32_t sigType, uint32_t *outSize)
{
	const uint32_t sigSizes[8] = {
		0xffffffffu, 0x30u, 0x20u, 0x20u, 0x10u, 0x100u, 0x20u, 0x20u
	};

	if (sigType - 1u > 6u)
		return (int)0x800f0616;

	*outSize = sigSizes[sigType];
	return 0;
}

/* 0x809e66 */
static int cfProcessUnalignedSegmentBlock(CfSegmentDecryptState *state,
		const uint64_t *reqLen, uint8_t *outBlock16, int updateHash)
{
	uint32_t lenLo = (uint32_t)*reqLen;
	uint32_t lenHi = (uint32_t)(*reqLen >> 32);
	uint64_t readOffset;
	uint64_t dstAddr = 0;
	uint64_t srcAddr = 0;
	uint32_t bytesRead = 0;
	uint32_t cbFlags = 0;
	uint32_t copyDesc[3];
	uint8_t localBlock[16];
	uint8_t *workBlock;
	int status;

	if (state == NULL || (lenLo & ~0xfu) != 0u || lenHi != 0u)
		return (int)0x800f0616;

	readOffset = state->curOffset;
	if (state->readCb(&readOffset, lenLo, &srcAddr, &dstAddr,
			&bytesRead, &cbFlags, state->readCbArg) != 0) {
		return (int)0x800f0605;
	}
	if (bytesRead != lenLo || lenHi != 0u)
		return (int)0x800f0605;

	workBlock = (outBlock16 != NULL) ? outBlock16 : localBlock;

	copyDesc[0] = 0u;
	copyDesc[1] = (uint32_t)(srcAddr >> 32);
	copyDesc[2] = cbFlags & 2u;
	status = cfCopyBytes(workBlock, (const void *)(uintptr_t)srcAddr, bytesRead, copyDesc);
	if (status != 0)
		return status;

	if (state->section.encryptionAlgorithm != 1u) {
		if (state->section.encryptionAlgorithm != 3u)
			return (int)0x800f0625;

		copyDesc[0] = 0u;
		copyDesc[1] = 0u;
		copyDesc[2] = 0u;
		status = cfAesCipher(4u, 1u, 0u, workBlock, workBlock, 16u,
				state->cipherKey, state->cipherIv, state->cipherIv, copyDesc);
		if (status != 0)
			return status;
	}

	copyDesc[0] = (uint32_t)(dstAddr >> 32);
	copyDesc[1] = 0u;
	copyDesc[2] = cbFlags & 1u;
	status = cfCopyBytes((void *)(uintptr_t)dstAddr, workBlock, bytesRead, copyDesc);
	if (status != 0)
		return status;

	if (updateHash != 0) {
		uint32_t isFinal = 0u;

		if (state->headLen == 0u &&
		    state->curOffset + bytesRead == state->section.offset + state->section.size) {
			isFinal = 0x80000000u;
		}
		status = cfHmacSha256Update(&state->hmacCtx, workBlock, bytesRead, isFinal);
		if (status != 0)
			return status;
	}

	if (state->doneCb != NULL) {
		uint64_t doneOff = state->curOffset;
		uint64_t doneSrc = srcAddr;
		uint64_t doneDst = dstAddr;

		if (state->doneCb(&doneOff, lenLo, &doneSrc, &doneDst,
				bytesRead, cbFlags, state->doneCbArg) != 0) {
			return (int)0x800f0605;
		}
	}

	state->curOffset += bytesRead;
	state->remSize -= bytesRead;
	return 0;
}

/* 0x80a06c */
static int cfVerifyRsaSignature(const void *hdrBuf, uint32_t sigInputLen,
		const void *signature256, const uint8_t *pubKeyBuf)
{
	uint32_t pubKeyDesc[4];
	uint32_t zeroPad[3];
	uint8_t workBuf[256];
	uint8_t digest[32];
	int status;

	zeroPad[0] = 0u;
	zeroPad[1] = 0u;
	zeroPad[2] = 0u;
	(void)zeroPad;

	status = bigmacSha256(digest, hdrBuf, sigInputLen);
	if (status == 0) {
		pubKeyDesc[0] = (uint32_t)(uintptr_t)pubKeyBuf;
		pubKeyDesc[1] = 0x100u;
		pubKeyDesc[2] = (uint32_t)(uintptr_t)(pubKeyBuf + 0x100u);
		pubKeyDesc[3] = 4u;
		status = cfVerifyRsaPkcs1v15(workBuf, signature256, pubKeyDesc, digest, 1u);
	}
	return status;
}

/* 0x80a0e8 */
static int cfAuthenticateAndDecryptHeaderWithKeyCb(const SelfAuthContext *ctx,
		CfGetKeysCallback getKeysCb, void *getKeysCbArg,
		int allowDevelopment, uint16_t expectedCategory,
		SelfHeaderReadCallback readCb, void *readCbArg,
		int32_t expectedExtHeaderSize, const uint64_t *expectedHeaderLen)
{
	uint64_t expHdrLen;
	uint64_t readOffset;
	uint32_t bytesRead = 0;
	cf_header *cfHdr;
	uint32_t scratchSize;
	uint32_t extHdrSize;
	uint32_t minHeaderEnd;
	uint32_t headerLenLo;
	const uint8_t *metaKey = NULL;
	const uint8_t *rootPubKey = NULL;
	uint32_t sigType = 0;
	const uint8_t *rootKey = NULL;
	uint32_t cipherDesc[3];
	uint32_t dummySigSize = 0;
	int32_t sigSize = 0;
	SceCfMetadataHeader *mh;
	SceCfMetadataSection *sections;
	uint8_t *metaKeyBlock;
	uint32_t i;
	int status;

	expHdrLen = *expectedHeaderLen;
	if (ctx == NULL || ctx->scratchBuf == NULL || getKeysCb == NULL ||
	    readCb == NULL || ctx->scratchSize < 0x30u) {
		return (int)0x800f0616;
	}

	cfHdr = (cf_header *)ctx->scratchBuf;
	readOffset = 0ULL;
	if (readCb((uintptr_t)cfHdr, &readOffset, 0x30u, &bytesRead, readCbArg) != 0 ||
	    bytesRead != 0x30u) {
		return (int)0x800f0605;
	}

	scratchSize = ctx->scratchSize;
	if (cfHdr->magic != 0x00454353u || cfHdr->version != 3u ||
	    ((uint8_t)cfHdr->attribute & 0x40u) == 0u ||
	    ((int8_t)(uint8_t)cfHdr->attribute < 0 && allowDevelopment == 0) ||
	    cfHdr->category != expectedCategory) {
		return (int)0x800f0624;
	}

	extHdrSize = cfHdr->ext_header_size;
	if ((extHdrSize & 0xfu) != 0u ||
	    (expectedExtHeaderSize >= 0 && (uint32_t)expectedExtHeaderSize != extHdrSize)) {
		return (int)0x800f0624;
	}

	if ((int64_t)expHdrLen >= 0 && cfHdr->file_offset != expHdrLen)
		return (int)0x800f0624;

	minHeaderEnd = extHdrSize + 0x90u;
	if (minHeaderEnd < extHdrSize || minHeaderEnd > scratchSize)
		return (int)0x800f0624;

	headerLenLo = (uint32_t)cfHdr->file_offset;
	if ((headerLenLo & 0xfu) != 0u)
		return (int)0x800f0624;
	if ((cfHdr->file_offset >> 32) != 0u || headerLenLo < minHeaderEnd ||
	    headerLenLo > scratchSize) {
		return (int)0x800f0624;
	}
	if (*(const uint64_t *)cfHdr->versionExtension != 0ULL)
		return (int)0x800f0624;
	if (headerLenLo > scratchSize)
		return (int)0x800f0616;

	readOffset = 0x30ULL;
	if (readCb((uintptr_t)ctx->scratchBuf + 0x30u, &readOffset,
			headerLenLo - 0x30u, &bytesRead, readCbArg) != 0 ||
	    bytesRead != headerLenLo - 0x30u) {
		return (int)0x800f0605;
	}

	status = getKeysCb(&metaKey, &rootPubKey, &sigType, &rootKey, getKeysCbArg);
	if (status != 0)
		return (int)0x800f0605;

	cipherDesc[0] = 0u;
	cipherDesc[1] = 0u;
	cipherDesc[2] = 0u;
	if (metaKey == NULL)
		return (int)0x800f0616;

	metaKeyBlock = (uint8_t *)ctx->scratchBuf + 0x30u + extHdrSize;
	if (rootKey != NULL) {
		status = cfAesCipher(1u, 0u, 0u, metaKeyBlock, metaKeyBlock, 0x40u,
				rootKey, rootKey + 0x10u, NULL, cipherDesc);
		if (status != 0)
			return status;
	}

	status = cfAesCipher(1u, 0u, 2u, metaKeyBlock, metaKeyBlock, 0x40u,
			metaKey, metaKey + 0x20u, NULL, cipherDesc);
	if (status != 0)
		return status;

	status = cfAesCipher(1u, 0u, 0u,
			(uint8_t *)ctx->scratchBuf + extHdrSize + 0x70u,
			(uint8_t *)ctx->scratchBuf + extHdrSize + 0x70u,
			headerLenLo - (extHdrSize + 0x70u),
			metaKeyBlock, metaKeyBlock + 0x20u, NULL, cipherDesc);
	if (status != 0)
		return status;

	if (rootPubKey == NULL)
		return (int)0x800f0616;
	if (cfGetSignatureSize(sigType, &dummySigSize) != 0)
		return (int)0x800f0616;
	if ((int8_t)((const uint8_t *)ctx->scratchBuf)[8] < 0)
		return (int)0x800f0625;

	mh = cfGetMetadataHeader(ctx);
	sections = cfGetMetadataSections(ctx);
	if (mh == NULL)
		return (int)0x800f0616;
	if (mh->signatureType != sigType)
		return (int)0x800f0616;
	if (cfGetSignatureSize(mh->signatureType, (uint32_t *)&sigSize) != 0)
		return (int)0x800f0616;

	if (mh->signatureInputLength == 0ULL ||
	    ((uint32_t)mh->signatureInputLength & 0xfu) != 0u ||
	    (mh->optHeaderSize & 0xfu) != 0u ||
	    mh->reserved != 0ULL) {
		return (int)0x800f0624;
	}

	{
		const cf_header *hdr = (const cf_header *)ctx->scratchBuf;
		uint64_t totalMetaEnd = (uint64_t)hdr->ext_header_size +
					(uint64_t)mh->optHeaderSize +
					0x10fULL +
					(uint64_t)(int64_t)sigSize +
					(uint64_t)mh->sectionCount * 0x30ULL +
					(uint64_t)mh->keyCount * 0x10ULL;

		if ((totalMetaEnd & ~0x7fULL) > hdr->file_offset)
			return (int)0x800f0624;
		if ((int64_t)expHdrLen >= 0 &&
		    mh->signatureInputLength != hdr->file_offset - (uint64_t)(int64_t)sigSize) {
			return (int)0x800f0624;
		}
		if (mh->signatureInputLength > hdr->file_offset - (uint64_t)(int64_t)sigSize)
			return (int)0x800f0624;
	}

	if (sections == NULL)
		return (int)0x800f0616;

	for (i = 0; i < mh->sectionCount; i++) {
		const cf_header *hdr = (const cf_header *)ctx->scratchBuf;
		const SceCfMetadataSection *sec = &sections[i];
		const uint32_t hashKeySlots[8] = {
			0xffffffffu, 7u, 6u, 2u, 2u, 0x21u, 4u, 2u
		};
		uint64_t secEnd;
		uint32_t hashNeed;

		if (sec->offset < hdr->file_offset)
			return (int)0x800f0624;
		secEnd = sec->offset + sec->size;
		if (secEnd < sec->offset || secEnd < sec->size || secEnd > hdr->cf_file_size)
			return (int)0x800f0624;

		if (sec->hashAlgorithm - 1u > 6u)
			return (int)0x800f0624;
		hashNeed = hashKeySlots[sec->hashAlgorithm];
		if ((hashNeed >> 31) != (uint32_t)(hashNeed + sec->hashKeyIndex < hashNeed) ||
		    hashNeed + sec->hashKeyIndex > mh->keyCount) {
			return (int)0x800f0624;
		}

		if (sec->encryptionAlgorithm == 1u) {
			if (sec->keyIndex != 0xffffffffu || sec->ivIndex != 0xffffffffu)
				return (int)0x800f0624;
		} else {
			const uint32_t encKeySlots[6] = {
				0xffffffffu, 0u, 1u, 1u, 2u, 2u
			};
			const uint32_t encIvSlots[6] = {
				0xffffffffu, 0u, 1u, 1u, 1u, 1u
			};
			uint32_t kNeed;
			uint32_t ivNeed;

			if (sec->encryptionAlgorithm - 1u > 4u)
				return (int)0x800f0624;
			kNeed = encKeySlots[sec->encryptionAlgorithm];
			ivNeed = encIvSlots[sec->encryptionAlgorithm];
			if ((kNeed >> 31) != (uint32_t)(kNeed + sec->keyIndex < kNeed) ||
			    kNeed + sec->keyIndex > mh->keyCount ||
			    (ivNeed >> 31) != (uint32_t)(ivNeed + sec->ivIndex < ivNeed) ||
			    ivNeed + sec->ivIndex > mh->keyCount) {
				return (int)0x800f0624;
			}
		}

		if (sec->compressionAlgorithm - 1u > 1u)
			return (int)0x800f0624;
	}

	if (mh->signatureType != 5u)
		return (int)0x800f0625;

	return cfVerifyRsaSignature(ctx->scratchBuf, (uint32_t)mh->signatureInputLength,
			(const uint8_t *)ctx->scratchBuf + (uint32_t)mh->signatureInputLength,
			rootPubKey);
}

/* 0x80a672 */
int selfInitAndVerifyHeader(const SelfAuthContext *ctx, const void *metaKey,
		const void *rootPubKey, uint32_t sigType, int allowDevelopment,
		uint16_t expectedCategory, SelfHeaderReadCallback readCb,
		void *readCbArg, int32_t expectedExtHeaderSize,
		const uint64_t *expectedHeaderLen)
{
	uint64_t expLen = *expectedHeaderLen;
	CfStaticKeyParams keyParams;

	keyParams.metaKey = metaKey;
	keyParams.rootPubKey = rootPubKey;
	keyParams.sigType = sigType;
	return cfAuthenticateAndDecryptHeaderWithKeyCb(ctx, cfDefaultGetKeysCallback,
			&keyParams, allowDevelopment, expectedCategory,
			readCb, readCbArg, expectedExtHeaderSize, &expLen);
}

/* 0x80a6b2 */
static int cfGetSectionCount(const SelfAuthContext *ctx, uint32_t *outCount)
{
	if (ctx == NULL || outCount == NULL)
		return (int)0x800f0616;

	*outCount = cfGetMetadataHeader(ctx)->sectionCount;
	return 0;
}

/* 0x80a6dc */
static int cfGetSectionEntry(const SelfAuthContext *ctx, uint32_t index,
		const SceCfMetadataSection **outSection)
{
	if (ctx == NULL || outSection == NULL ||
	    index >= cfGetMetadataHeader(ctx)->sectionCount) {
		return (int)0x800f0616;
	}

	*outSection = &cfGetMetadataSections(ctx)[index];
	return 0;
}

/* 0x80a72c */
static int cfGetExtHeaderAndMetaOffset(const SelfAuthContext *ctx,
		const uint32_t **outExtHdr, uint32_t *outMetaOffset)
{
	const uint8_t *base;

	if (ctx == NULL || outExtHdr == NULL || outMetaOffset == NULL)
		return (int)0x800f0616;

	base = (const uint8_t *)ctx->scratchBuf;
	*outExtHdr = (const uint32_t *)(base + 0x30u);
	*outMetaOffset = *(const uint32_t *)(base + 0x0cu);
	return 0;
}

/* 0x80a74c */
static int cfGetOptHeaders(const SelfAuthContext *ctx,
		const uint8_t **outOptHdr, uint32_t *outOptHdrSize)
{
	uint8_t *keyPool;

	if (ctx == NULL || outOptHdr == NULL || outOptHdrSize == NULL)
		return (int)0x800f0616;

	keyPool = cfGetMetadataKeyPool(ctx);
	*outOptHdr = keyPool + cfGetMetadataHeader(ctx)->keyCount * 0x10u;
	*outOptHdrSize = cfGetMetadataHeader(ctx)->optHeaderSize;
	return 0;
}

/* 0x80a7a0 */
static int cfDecryptAndVerifySection(const SelfAuthContext *ctx, uint32_t sectionIndex,
		SelfSegmentReadCallback readCb, void *readCbArg,
		SelfSegmentDoneCallback doneCb, void *doneCbArg)
{
	CfSegmentDecryptState state;
	uint8_t calcDigest[32];
	uint32_t copyDesc[3];
	const SceCfMetadataSection *sec;
	int status;

	if (ctx == NULL || readCb == NULL ||
	    sectionIndex >= cfGetMetadataHeader(ctx)->sectionCount) {
		return (int)0x800f0616;
	}

	sec = &cfGetMetadataSections(ctx)[sectionIndex];
	state.selfCtx = ctx;
	slMemcpy(&state.section, sec, sizeof(SceCfMetadataSection));
	state.headLen = 0u;

	if (sec->size != 0ULL) {
		uint32_t hIdx;

		if (sec->hashAlgorithm != 6u)
			return (int)0x800f0625;

		hIdx = sec->hashKeyIndex;
		state.expectedDigest = cfGetMetadataKeyPool(ctx) + hIdx * 0x10u;
		if (cfHmacSha256InitShortKey(&state.hmacCtx,
				cfGetMetadataKeyPool(ctx) + (hIdx + 2u) * 0x10u,
				0x20u) != 0) {
			return (int)0x800f0605;
		}

		if (sec->encryptionAlgorithm == 3u) {
			copyDesc[0] = 0u;
			copyDesc[1] = 0u;
			copyDesc[2] = 0u;
			state.cipherKey = cfGetMetadataKeyPool(ctx) + sec->keyIndex * 0x10u;
			status = cfCopyBytes(state.cipherIv,
					cfGetMetadataKeyPool(ctx) + sec->ivIndex * 0x10u,
					0x10u, copyDesc);
			if (status != 0)
				return status;
		} else if (sec->encryptionAlgorithm == 1u) {
			state.cipherKey = NULL;
		} else {
			return (int)0x800f0625;
		}
	}

	sec = &cfGetMetadataSections(ctx)[sectionIndex];
	if (state.section.size != 0ULL) {
		state.readCb = readCb;
		state.readCbArg = readCbArg;
		state.doneCb = doneCb;
		state.doneCbArg = doneCbArg;
		state.curOffset = sec->offset;
		state.remSize = sec->size;

		if (((uint32_t)sec->offset & 0xfu) != 0u) {
			uint64_t headReq;

			state.headLen = 0x10u - ((uint32_t)sec->offset & 0xfu);
			if (sec->size < (uint64_t)state.headLen)
				state.headLen = (uint32_t)sec->size;
			headReq = (uint64_t)state.headLen;
			status = cfProcessUnalignedSegmentBlock(&state, &headReq,
					state.headBlock, 0);
			if (status != 0)
				return status;
		}

		while ((state.remSize & ~0xfULL) != 0ULL) {
			uint64_t reqOff = state.curOffset;
			uint64_t srcAddr = 0;
			uint64_t dstAddr = 0;
			uint32_t bytesRead = 0;
			uint32_t cbFlags = 0;
			uint32_t isFinal;

			if (state.readCb(&reqOff, (uint32_t)state.remSize & ~0xfu,
					&srcAddr, &dstAddr, &bytesRead,
					&cbFlags, state.readCbArg) != 0 ||
			    bytesRead == 0u || (bytesRead & 0xfu) != 0u) {
				return (int)0x800f0605;
			}

			copyDesc[0] = (uint32_t)(dstAddr >> 32);
			copyDesc[1] = (uint32_t)(srcAddr >> 32);
			copyDesc[2] = cbFlags;
			if (state.section.encryptionAlgorithm == 1u) {
				status = cfCopyBytes((void *)(uintptr_t)dstAddr,
						(const void *)(uintptr_t)srcAddr,
						bytesRead, copyDesc);
			} else if (state.section.encryptionAlgorithm == 3u) {
				status = cfAesCipher(4u, 1u, 0u,
						(void *)(uintptr_t)dstAddr,
						(const void *)(uintptr_t)srcAddr,
						bytesRead, state.cipherKey,
						state.cipherIv, state.cipherIv,
						copyDesc);
			} else {
				return (int)0x800f0625;
			}
			if (status != 0)
				return status;

			isFinal = (cbFlags & 1u) << 1;
			if (state.headLen == 0u &&
			    state.curOffset + bytesRead ==
			    state.section.offset + state.section.size) {
				isFinal |= 0x80000000u;
			}

			status = cfHmacSha256Update(&state.hmacCtx,
					(const void *)(uintptr_t)dstAddr,
					bytesRead, isFinal & 0x80000000u);
			if (status != 0)
				return status;

			if (state.doneCb != NULL) {
				uint64_t doneOff = state.curOffset;
				uint64_t doneSrc = srcAddr;
				uint64_t doneDst = dstAddr;

				if (state.doneCb(&doneOff, (uint32_t)state.remSize & ~0xfu,
						&doneSrc, &doneDst, bytesRead,
						cbFlags, state.doneCbArg) != 0) {
					return (int)0x800f0605;
				}
			}

			state.curOffset += bytesRead;
			state.remSize -= bytesRead;
		}

		if (state.remSize != 0ULL) {
			uint64_t tailReq = state.remSize;

			status = cfProcessUnalignedSegmentBlock(&state, &tailReq, NULL, 1);
			if (status != 0)
				return status;
		}
	}

	if (state.section.size == 0ULL)
		return 0;

	if (state.headLen != 0u) {
		status = cfHmacSha256Update(&state.hmacCtx, state.headBlock,
				state.headLen, 0x80000000u);
		if (status != 0)
			return status;
	}

	status = cfHmacSha256Final(calcDigest, &state.hmacCtx);
	if (status == 0 &&
	    slTimingsafeBcmp(calcDigest, state.expectedDigest, 32u) != 0) {
		status = (int)0x800f0627;
	}
	return status;
}

/* 0x80ab5a */
int selfDecryptAndLoadSegment(const SelfAuthContext *ctx, uint32_t segType,
		uint32_t segIndex, SelfSegmentReadCallback readCb, void *readCbArg,
		SelfSegmentDoneCallback doneCb, void *doneCbArg)
{
	uint32_t secCount = cfGetMetadataHeader(ctx)->sectionCount;
	uint32_t i;

	for (i = 0; i < secCount; i++) {
		const SceCfMetadataSection *sec = &cfGetMetadataSections(ctx)[i];

		if (sec->type == segType && sec->index == segIndex) {
			return cfDecryptAndVerifySection(ctx, i, readCb, readCbArg,
					doneCb, doneCbArg);
		}
	}
	return (int)0x800f0602;
}

/* 0x80abea */
static int cfAesCipher(uint32_t chainSelect, uint32_t dirSelect, uint32_t keyBitsSelect,
		void *dst, const void *src, uint32_t size,
		const void *key, const void *ivIn, void *ivOut, void *unusedDesc)
{
	uint32_t bigmacChain;
	uint32_t bigmacDir;
	uint32_t bigmacKeySize;
	uint32_t localIv[4];
	uint32_t *ivPtr;
	BigmacContext bmCtx;
	int status;

	(void)unusedDesc;
	switch (chainSelect) {
	case 0u:
		bigmacChain = 0u;
		break;
	case 1u:
		bigmacChain = 1u;
		break;
	case 2u:
		bigmacChain = 2u;
		break;
	case 3u:
		bigmacChain = 3u;
		break;
	case 4u:
		bigmacChain = 4u;
		break;
	default:
		return (int)0x800f0616;
	}

	if (dirSelect == 0u)
		bigmacDir = 2u;
	else if (dirSelect == 1u)
		bigmacDir = 1u;
	else
		return (int)0x800f0616;

	if (keyBitsSelect == 1u)
		bigmacKeySize = 2u;
	else if (keyBitsSelect == 0u)
		bigmacKeySize = 1u;
	else if (keyBitsSelect == 2u)
		bigmacKeySize = 3u;
	else
		return (int)0x800f0616;

	if ((size & 0xfu) != 0u || (((uintptr_t)src | (uintptr_t)dst) & 3u) != 0u)
		return (int)0x800f0616;

	if (bigmacChain != 0u) {
		if (ivOut != NULL && ivIn == ivOut) {
			ivPtr = (uint32_t *)ivOut;
		} else {
			slMemcpy(localIv, ivIn, 16u);
			ivPtr = localIv;
		}
		if (bigmacChain == 4u)
			cfBswap128(ivPtr);
	} else {
		ivPtr = NULL;
	}

	bigmacInitContext(&bmCtx);
	status = bigmacStartAesCipher(dst, src, size, key, ivPtr,
			bigmacDir, bigmacChain, bigmacKeySize, &bmCtx);
	if (status != 0)
		return status;

	status = bigmacWaitChannel(&bmCtx);
	if (status != 0)
		return status;

	if (bigmacChain != 0u && ivOut != NULL) {
		if (bigmacChain == 4u)
			cfBswap128(ivPtr);
		if (ivPtr != (uint32_t *)ivOut)
			slMemcpy(ivOut, ivPtr, 16u);
	}
	return status;
}

/* 0x80ad32 */
static int cfCopyBytes(void *dst, const void *src, uint32_t size, void *unusedDesc)
{
	uint32_t offset = 0u;
	int status;

	(void)unusedDesc;
	if (size <= 16u) {
		slMemcpy(dst, src, size);
		return 0;
	}

	for (;;) {
		uint32_t chunk = (size > 0x00800000u) ? 0x00800000u : size;

		status = bigmacMemcpy((uint8_t *)dst + offset,
				(const uint8_t *)src + offset, chunk);
		if (status != 0)
			break;
		size -= chunk;
		if (size == 0u)
			break;
		offset += chunk;
	}
	return status;
}

/* 0x80ad8c */
static int cfVerifyRsaPkcs1v15(void *workBuf256, const void *signature256,
		const uint32_t *pubKeyWords, const uint8_t *digest, uint32_t hashSubType)
{
	RsaPublicKeyParam rsaKey;
	uint32_t hashAlgo;

	if (hashSubType == 1u)
		hashAlgo = 4u;
	else if (hashSubType == 0u)
		hashAlgo = 2u;
	else if (hashSubType == 2u)
		hashAlgo = 8u;
	else
		return (int)0x800f0616;

	rsaKey.modulus = (const uint32_t *)(uintptr_t)pubKeyWords[0];
	rsaKey.modulusWordCount = pubKeyWords[1] >> 2;
	rsaKey.exponent = (const uint32_t *)(uintptr_t)pubKeyWords[2];
	rsaKey.exponentWordCount = pubKeyWords[3] >> 2;
	return verifyRsaPkcs1v15Blocking(workBuf256, signature256, &rsaKey, digest, hashAlgo);
}

/* 0x80adde */
static int cfHmacSha256InitShortKey(CfHmacContext *ctx, const void *key, uint32_t keyLen)
{
	uint8_t paddedKey[64];
	const uint8_t *keyBytes = (const uint8_t *)key;
	uint32_t i;

	if (keyLen > 64u)
		return (int)0x800f0616;

	for (i = 0; i < keyLen; i++)
		paddedKey[i] = keyBytes[i];
	for (; i < 64u; i++)
		paddedKey[i] = 0u;

	return cfHmacSha256InitBlockKey(ctx, paddedKey, 64u);
}

/* 0x80ae42 */
static void cfBswap128(uint32_t *words4)
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

/* 0x80aec4 */
int cfBigmacSha256Start(void *dstDigest, const void *src, uint32_t size, BigmacContext *ctx)
{
	return bigmacStartHash(dstDigest, src, size, 2u, ctx);
}

/* 0x80aed8 */
static int cfHmacSha256InitBlockKey(CfHmacContext *ctx, const void *key64, uint32_t keyLen)
{
	BigmacContext bmCtx;

	bigmacInitContext(&bmCtx);
	return bigmacHmacInit(6u, ctx, key64, keyLen, &bmCtx);
}

/* 0x80af12 */
static int cfHmacSha256Update(CfHmacContext *ctx, const void *data,
		uint32_t size, uint32_t isFinalFlag)
{
	BigmacContext bmCtx;
	uint32_t consumed = 0u;
	uint32_t offset = 0u;
	int status;

	bigmacInitContext(&bmCtx);
	while ((int32_t)size > 0) {
		status = bigmacHmacUpdateBlock(6u, ctx, (const uint8_t *)data + offset,
				size, &consumed, isFinalFlag, &bmCtx);
		if (status != 0)
			return status;
		status = bigmacWaitChannel(&bmCtx);
		if (status != 0)
			return status;
		offset += consumed;
		size -= consumed;
	}
	return (size == 0u) ? 0 : (int)0x800f0005;
}

/* 0x80af7e */
static int cfHmacSha256Final(uint8_t *outDigest, CfHmacContext *ctx)
{
	BigmacContext bmCtx;

	bigmacInitContext(&bmCtx);
	return bigmacHmacExtractDigest(6u, outDigest, ctx, &bmCtx);
}

/* 0x80afac */
static int bigmacHmacInit(uint32_t hashAlgo, CfHmacContext *ctx, const void *key64,
		uint32_t keyLen, BigmacContext *bmCtx)
{
	uint32_t i;

	(void)hashAlgo;
	(void)bmCtx;
	if (keyLen != 64u)
		return (int)0x800f0016;

	ctx->state = 0u;
	for (i = 0; i < 32u; i++)
		ctx->digest[i] = 0u;
	for (i = 0; i < 64u; i++)
		ctx->blockBuf[i] = 0u;
	ctx->bufUsed = 0u;
	slMemcpy(ctx->hmacKey, key64, 64u);
	ctx->keyLen = 64u;
	return 0;
}

/* 0x80b012 */
static int bigmacHmacStartHw(uint32_t hashAlgo, CfHmacContext *ctx, const void *src,
		uint32_t size, uint32_t isFinal, const BigmacContext *bmCtx)
{
	uint32_t flags = bmCtx->flags;
	uint32_t ch;
	uint32_t funcVal;
	volatile uint32_t *extKey;
	const uint32_t *kWords;

	if ((isFinal == 0u && (size & 0x3fu) != 0u) ||
	    size == 0u || ((uintptr_t)src & 3u) != 0u || ctx->keyLen != 64u) {
		return (int)0x800f0016;
	}

	ch = bmCtx->channel & 1u;
	BIGMAC->ch[ch].src = (uint32_t)(uintptr_t)src;
	BIGMAC->ch[ch].dst = (uint32_t)(uintptr_t)ctx->digest;
	BIGMAC->ch[ch].length = size;

	funcVal = (flags & ~0x3fu) | 3u | ((hashAlgo & 7u) << 3);
	if (isFinal == 0u) {
		if (ctx->state != 0u) {
			funcVal |= 0xc00u;
		} else {
			funcVal = (funcVal & ~0xc00u) | 0x400u;
			ctx->state = 1u;
		}
	} else {
		funcVal &= ~0xc00u;
		if (ctx->state != 0u)
			funcVal |= 0x800u;
		ctx->state = 2u;
	}

	BIGMAC->ch[ch].func = funcVal;
	BIGMAC->ch[ch].iv = (uint32_t)(uintptr_t)ctx->iv;

	extKey = &BIGMAC->externalKey[ch * 32u];
	for (kWords = (const uint32_t *)ctx->hmacKey;
	     (uint32_t)((uintptr_t)kWords - (uintptr_t)ctx->hmacKey) < ctx->keyLen;
	     kWords++) {
		*extKey++ = *kWords;
	}
	BIGMAC->ch[ch].trigger = 1u;
	return 0;
}

/* 0x80b108 */
static int bigmacHmacUpdateBlock(uint32_t hashAlgo, CfHmacContext *ctx, const void *data,
		uint32_t size, uint32_t *outConsumed, uint32_t isFinal,
		const BigmacContext *bmCtx)
{
	uint32_t bufUsed = ctx->bufUsed;
	int status;

	if (bufUsed > 63u)
		return (int)0x800f0016;

	if (bufUsed == 0u && ((uintptr_t)data & 3u) == 0u) {
		uint32_t alignedSize;

		if (isFinal != 0u) {
			status = bigmacHmacStartHw(hashAlgo, ctx, data, size, isFinal, bmCtx);
			*outConsumed = (status == 0) ? size : 0u;
			return status;
		}

		alignedSize = size & ~0x3fu;
		if (alignedSize != 0u) {
			status = bigmacHmacStartHw(hashAlgo, ctx, data, alignedSize, 0u, bmCtx);
			if (status != 0) {
				*outConsumed = 0u;
				return status;
			}
		}
		slMemcpy(ctx->blockBuf, (const uint8_t *)data + alignedSize, size - alignedSize);
		ctx->bufUsed = size - alignedSize;
		*outConsumed = size;
		return 0;
	} else {
		uint32_t space = 64u - bufUsed;
		uint32_t copyLen = (size > space) ? space : size;
		uint32_t blockFinal;

		slMemcpy(ctx->blockBuf + bufUsed, data, copyLen);
		if (copyLen != space && (size != copyLen || isFinal == 0u)) {
			*outConsumed = copyLen;
			ctx->bufUsed += copyLen;
			return 0;
		}

		blockFinal = (size == copyLen && isFinal != 0u) ? 1u : 0u;
		status = bigmacHmacStartHw(hashAlgo, ctx, ctx->blockBuf,
				bufUsed + copyLen, blockFinal, bmCtx);
		if (status != 0) {
			*outConsumed = 0u;
			return status;
		}
		ctx->bufUsed = 0u;
		*outConsumed = copyLen;
		return 0;
	}
}

/* 0x80b208 */
static int bigmacHmacExtractDigest(uint32_t hashAlgo, uint8_t *outDigest,
		const CfHmacContext *ctx, const BigmacContext *bmCtx)
{
	const uint32_t digestSizes[3] = { 20u, 28u, 32u };
	uint32_t len;
	uint32_t i;

	(void)bmCtx;
	len = digestSizes[hashAlgo - 4u];
	for (i = 0; i < len; i++)
		outDigest[i] = ctx->digest[i];
	return 0;
}
