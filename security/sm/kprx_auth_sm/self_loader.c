/*
 * self_loader.c: Certified File (v3) / SELF header authentication, metadata
 * decryption, and scatter-gather segment streaming for kprx_auth_sm.self
 * (0x80b09c..0x80cbf0, .rodata 0x810438..0x8104d0).
 *
 * Sony C++ / C helper names recovered from 0.995.070-TOOL debug strings:
 *   - self_loader::open                       (0x80bddc, selfInitAndVerifyHeader)
 *   - self_loader::verify_header              (0x80c290, selfVerifySvnAndDecryptMetadata)
 *   - self_loader::get_elf_header             (0x80c766, selfGetElfHeader)
 *   - self_loader::get_program_segment_info   (0x80c780 / 0x80c7aa)
 *   - self_loader::load_segment               (0x80c078, selfDecryptAndLoadSegment)
 *
 * Uses cf_header, SceSelfHeader, SceSelfSegmentInfo, Elf32_Ehdr, and Elf32_Phdr
 * from sys/internal/include/modulemgr/{certified_file.h,self.h,elf.h}.
 * Other internal helper and structure names are ours.
 */

#include "kprx_auth_sm.h"
#include <modulemgr/certified_file.h>
#include <modulemgr/self.h>
#include <modulemgr/elf.h>

_Static_assert(sizeof(SceSelfSegmentInfo) == 0x20, "SceSelfSegmentInfo size");

typedef struct CfStaticKeyParams {
	const void *metaKey;			/* 0x00 */
	const void *rootPubKey;			/* 0x04 */
	uint32_t sigType;			/* 0x08 */
} CfStaticKeyParams;

/* ============================================================================
 * .rodata (0x810438..0x8104d0)
 * ============================================================================ */

/* 0x810438: signature byte length indexed by signatureType (1..7) */
static const uint32_t s_cfSigSizes[8] = {
	0xffffffffu, 0x30u, 0x20u, 0x20u, 0x10u, 0x100u, 0x20u, 0x20u
};

/* 0x810458: 16-byte keyPool slot count required by hashAlgorithm (1..7) */
static const uint32_t s_cfHashKeySlots[8] = {
	0xffffffffu, 7u, 6u, 2u, 2u, 0x21u, 4u, 2u
};

/* 0x810478: 16-byte keyPool slot count required for IV by encryptionAlgorithm (1..5) */
static const uint32_t s_cfEncIvSlots[6] = {
	0xffffffffu, 0u, 1u, 1u, 1u, 1u
};

/* 0x810490: 16-byte keyPool slot count required for key by encryptionAlgorithm (1..5) */
static const uint32_t s_cfEncKeySlots[6] = {
	0xffffffffu, 0u, 1u, 1u, 2u, 2u
};

/* 0x8104a8: fixed 20-byte magic constant in SceSelfControlInfoDigest256 */
static const uint8_t s_selfControlDigestConstant[20] = {
	0x62u, 0x7cu, 0xb1u, 0x80u, 0x8au, 0xb9u, 0x38u, 0xe3u,
	0x2cu, 0x8cu, 0x09u, 0x17u, 0x08u, 0x72u, 0x6au, 0x57u,
	0x9eu, 0x25u, 0x86u, 0xe4u
};

/* ============================================================================
 * Certified File (CF) / SELF Core Functions (0x80b09c..0x80cbf0)
 * ============================================================================ */

/* 0x80b09c (name ours) */
SceCfMetadataHeader *cfGetMetadataHeader(const SelfAuthContext *ctx)
{
	uint8_t *base = ctx->hdrBuf;
	uint32_t metaOffset = *(const uint32_t *)(base + 0x0cu);

	return (SceCfMetadataHeader *)(base + metaOffset + 0x70u);
}

/* 0x80b0aa (name ours) */
SceCfMetadataSection *cfGetMetadataSections(const SelfAuthContext *ctx)
{
	return (SceCfMetadataSection *)((uint8_t *)cfGetMetadataHeader(ctx) + 0x20u);
}

/* 0x80b0bc (name ours) */
uint8_t *cfGetMetadataKeyPool(const SelfAuthContext *ctx)
{
	uint8_t *sections = (uint8_t *)cfGetMetadataSections(ctx);
	SceCfMetadataHeader *mh = cfGetMetadataHeader(ctx);

	return sections + mh->sectionCount * 0x30u;
}

/* 0x80b0e8 (name ours) */
int cfDefaultGetKeysCallback(const uint8_t **outMetaKey, const uint8_t **outRootPubKey,
		uint32_t *outSigType, const uint8_t **outRootKey, void *arg)
{
	const CfStaticKeyParams *params = (const CfStaticKeyParams *)arg;

	*outMetaKey = (const uint8_t *)params->metaKey;
	*outRootPubKey = (const uint8_t *)params->rootPubKey;
	*outSigType = params->sigType;
	*outRootKey = NULL;
	return 0;
}

/* 0x80b102 (name ours) */
int cfGetSignatureSize(uint32_t sigType, uint32_t *outSize)
{
	uint32_t sigSizes[8];

	memcpy(sigSizes, s_cfSigSizes, sizeof(sigSizes));
	if (sigType - 1u > 6u)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	*outSize = sigSizes[sigType];
	return 0;
}

/* 0x80b14a (name ours) */
int cfValidateMetadataHeaderBounds(const uint8_t *hdrBytes,
		const SceCfMetadataHeader *mh, uint32_t expectedSigType,
		const int64_t *expectedHeaderLen)
{
	const cf_header *hdr = (const cf_header *)hdrBytes;
	int64_t expHdrLen = *expectedHeaderLen;
	int32_t sigSize = 0;
	uint64_t totalMetaEnd;
	uint64_t maxSigInput;

	if (mh == NULL || mh->signatureType != expectedSigType ||
	    cfGetSignatureSize(mh->signatureType, (uint32_t *)&sigSize) != 0) {
		return SCE_SBL_ERROR_CF_INVALID_VALUE;
	}

	if (mh->signatureInputLength == 0ULL ||
	    ((uint32_t)mh->signatureInputLength & 0xfu) != 0u ||
	    (mh->optHeaderSize & 0xfu) != 0u ||
	    mh->reserved != 0ULL) {
		return SCE_SBL_ERROR_CF_BAD_HEADER;
	}

	totalMetaEnd = (uint64_t)hdr->ext_header_size +
		       (uint64_t)mh->optHeaderSize +
		       0x10fULL +
		       (uint64_t)(int64_t)sigSize +
		       (uint64_t)mh->sectionCount * 0x30ULL +
		       (uint64_t)mh->keyCount * 0x10ULL;

	if ((totalMetaEnd & ~0x7fULL) > hdr->file_offset)
		return SCE_SBL_ERROR_CF_BAD_HEADER;

	maxSigInput = hdr->file_offset - (uint64_t)(int64_t)sigSize;
	if (expHdrLen >= 0 && mh->signatureInputLength != maxSigInput)
		return SCE_SBL_ERROR_CF_BAD_HEADER;

	if (mh->signatureInputLength > maxSigInput)
		return SCE_SBL_ERROR_CF_BAD_HEADER;

	return 0;
}

/* 0x80b28a (name ours) */
int cfProcessUnalignedSegmentBlock(CfSegmentDecryptState *state,
		const uint64_t *reqLen, uint8_t *outBlock16, int updateHash)
{
	uint32_t lenLo = (uint32_t)*reqLen;
	uint32_t lenHi = (uint32_t)(*reqLen >> 32);
	uint64_t readOffset;
	uint64_t dstAddr = 0ULL;
	uint64_t srcAddr = 0ULL;
	uint32_t bytesRead = 0u;
	uint32_t cbFlags = 0u;
	uint32_t copyDesc[3];
	uint8_t localBlock[16];
	uint8_t *workBlock;
	int status;

	if (state == NULL || (lenLo & ~0xfu) != 0u || lenHi != 0u)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	readOffset = state->curOffset;
	if (state->readCb(&readOffset, lenLo, &srcAddr, &dstAddr,
			&bytesRead, &cbFlags, state->readCbArg) != 0) {
		return SCE_SBL_ERROR_CF_IO;
	}
	if (bytesRead != lenLo || lenHi != 0u)
		return SCE_SBL_ERROR_CF_IO;

	workBlock = (outBlock16 != NULL) ? outBlock16 : localBlock;

	copyDesc[0] = 0u;
	copyDesc[1] = (uint32_t)(srcAddr >> 32);
	copyDesc[2] = cbFlags & 2u;
	status = cfCopyBytes(workBlock, (const void *)(uintptr_t)srcAddr, bytesRead, copyDesc);
	if (status != 0)
		return status;

	if (state->section.encryptionAlgorithm != 1u) {
		if (state->section.encryptionAlgorithm != 3u)
			return SCE_SBL_ERROR_CF_UNSUPPORTED_ALGO;

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
		status = bigmacHmacSha256Update(&state->hmacCtx, workBlock,
				(int32_t)bytesRead, isFinal);
		if (status != 0)
			return status;
	}

	if (state->doneCb != NULL) {
		uint64_t doneOff = state->curOffset;
		uint64_t doneSrc = srcAddr;
		uint64_t doneDst = dstAddr;

		if (state->doneCb(&doneOff, lenLo, &doneSrc, &doneDst,
				bytesRead, cbFlags, state->doneCbArg) != 0) {
			return SCE_SBL_ERROR_CF_IO;
		}
	}

	state->curOffset += bytesRead;
	state->remSize -= bytesRead;
	return 0;
}

/* 0x80b490 (name ours) */
int cfSegmentDecryptUpdate(CfSegmentDecryptState *state,
		const uint64_t *offset, const uint64_t *size,
		SelfSegmentReadCallback readCb, void *readCbArg,
		SelfSegmentDoneCallback doneCb, void *doneCbArg)
{
	uint64_t startOff = *offset;
	uint64_t totalSize = *size;
	int status;

	if (state->section.size == 0ULL)
		return 0;

	state->curOffset = startOff;
	state->readCb = readCb;
	state->readCbArg = readCbArg;
	state->doneCb = doneCb;
	state->doneCbArg = doneCbArg;
	state->remSize = totalSize;

	if (((uint32_t)startOff & 0xfu) != 0u) {
		uint64_t headReq;

		state->headLen = 0x10u - ((uint32_t)startOff & 0xfu);
		if (totalSize < (uint64_t)state->headLen)
			state->headLen = (uint32_t)totalSize;
		headReq = (uint64_t)state->headLen;
		status = cfProcessUnalignedSegmentBlock(state, &headReq,
				state->headBlock, 0);
		if (status != 0)
			return status;
	}

	while ((state->remSize & ~0xfULL) != 0ULL) {
		uint64_t reqOff = state->curOffset;
		uint64_t srcAddr = 0ULL;
		uint64_t dstAddr = 0ULL;
		uint32_t bytesRead = 0u;
		uint32_t cbFlags = 0u;
		uint32_t copyDesc[3];
		uint32_t isFinal;

		if (state->readCb(&reqOff, (uint32_t)state->remSize & ~0xfu,
				&srcAddr, &dstAddr, &bytesRead,
				&cbFlags, state->readCbArg) != 0 ||
		    bytesRead == 0u || (bytesRead & 0xfu) != 0u) {
			return SCE_SBL_ERROR_CF_IO;
		}

		copyDesc[0] = (uint32_t)(dstAddr >> 32);
		copyDesc[1] = (uint32_t)(srcAddr >> 32);
		copyDesc[2] = cbFlags;
		if (state->section.encryptionAlgorithm == 1u) {
			status = cfCopyBytes((void *)(uintptr_t)dstAddr,
					(const void *)(uintptr_t)srcAddr,
					bytesRead, copyDesc);
		} else if (state->section.encryptionAlgorithm == 3u) {
			status = cfAesCipher(4u, 1u, 0u,
					(void *)(uintptr_t)dstAddr,
					(const void *)(uintptr_t)srcAddr,
					bytesRead, state->cipherKey,
					state->cipherIv, state->cipherIv,
					copyDesc);
		} else {
			return SCE_SBL_ERROR_CF_UNSUPPORTED_ALGO;
		}
		if (status != 0)
			return status;

		isFinal = (cbFlags & 1u) << 1;
		if (state->headLen == 0u &&
		    state->curOffset + bytesRead ==
		    state->section.offset + state->section.size) {
			isFinal |= 0x80000000u;
		}

		status = bigmacHmacSha256Update(&state->hmacCtx,
				(const uint8_t *)(uintptr_t)dstAddr,
				(int32_t)bytesRead, isFinal & 0x80000000u);
		if (status != 0)
			return status;

		if (state->doneCb != NULL) {
			uint64_t doneOff = state->curOffset;
			uint64_t doneSrc = srcAddr;
			uint64_t doneDst = dstAddr;

			if (state->doneCb(&doneOff, (uint32_t)state->remSize & ~0xfu,
					&doneSrc, &doneDst, bytesRead,
					cbFlags, state->doneCbArg) != 0) {
				return SCE_SBL_ERROR_CF_IO;
			}
		}

		state->curOffset += bytesRead;
		state->remSize -= bytesRead;
	}

	if (state->remSize != 0ULL) {
		uint64_t tailReq = state->remSize;

		return cfProcessUnalignedSegmentBlock(state, &tailReq, NULL, 1);
	}
	return 0;
}

/* 0x80b6ce (name ours) */
int cfSegmentDecryptFinal(CfSegmentDecryptState *state)
{
	uint8_t calcDigest[32];
	int status;

	if (state->section.size == 0ULL)
		return 0;

	if (state->headLen != 0u) {
		status = bigmacHmacSha256Update(&state->hmacCtx, state->headBlock,
				(int32_t)state->headLen, 0x80000000u);
		if (status != 0)
			return status;
	}

	status = bigmacHmacSha256Final(calcDigest, &state->hmacCtx);
	if (status == 0 &&
	    timingsafeBcmp(calcDigest, state->expectedDigest, 32u) != 0 &&
	    (int8_t)state->selfCtx->hdrBuf[8] >= 0) {
		status = SCE_SBL_ERROR_CF_BAD_DIGEST;
	}
	return status;
}

/* 0x80b75e (name ours) */
int cfSegmentDecryptInit(CfSegmentDecryptState *state,
		const SelfAuthContext *ctx, uint32_t sectionIndex,
		const SceCfMetadataSection *secOverride)
{
	const SceCfMetadataSection *sec = secOverride;
	uint32_t copyDesc[3];
	int status;

	if (state == NULL || ctx == NULL)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	if (sec == NULL) {
		if (sectionIndex >= cfGetMetadataHeader(ctx)->sectionCount)
			return SCE_SBL_ERROR_CF_INVALID_VALUE;
		sec = &cfGetMetadataSections(ctx)[sectionIndex];
	}

	state->selfCtx = ctx;
	memcpy(&state->section, sec, sizeof(SceCfMetadataSection));
	state->headLen = 0u;

	if (sec->size != 0ULL) {
		uint32_t hIdx;

		if (sec->hashAlgorithm != 6u)
			return SCE_SBL_ERROR_CF_UNSUPPORTED_ALGO;

		hIdx = sec->hashKeyIndex;
		state->expectedDigest = cfGetMetadataKeyPool(ctx) + hIdx * 0x10u;
		if (cfHmacSha256InitShortKey(&state->hmacCtx,
				cfGetMetadataKeyPool(ctx) + (hIdx + 2u) * 0x10u,
				0x20u) != 0) {
			return SCE_SBL_ERROR_CF_IO;
		}

		if (sec->encryptionAlgorithm == 3u) {
			copyDesc[0] = 0u;
			copyDesc[1] = 0u;
			copyDesc[2] = 0u;
			state->cipherKey = cfGetMetadataKeyPool(ctx) + sec->keyIndex * 0x10u;
			status = cfCopyBytes(state->cipherIv,
					cfGetMetadataKeyPool(ctx) + sec->ivIndex * 0x10u,
					0x10u, copyDesc);
			if (status != 0)
				return SCE_SBL_ERROR_CF_IO;
		} else if (sec->encryptionAlgorithm == 1u) {
			state->cipherKey = NULL;
		} else {
			return SCE_SBL_ERROR_CF_UNSUPPORTED_ALGO;
		}
	}
	return 0;
}

/* 0x80b864 (name ours) */
int cfVerifyRsaSignature(const void *hdrBuf, uint32_t sigInputLen,
		const void *signature256, const uint8_t *pubKeyBuf)
{
	RsaPublicKey2048 pubKeyDesc;
	uint32_t zeroPad[3];
	uint8_t workBuf[256];
	uint8_t digest[32];
	int status;

	zeroPad[0] = 0u;
	zeroPad[1] = 0u;
	zeroPad[2] = 0u;

	status = bigmacSha256(digest, hdrBuf, sigInputLen);
	if (status == 0) {
		pubKeyDesc.modulus = pubKeyBuf;
		pubKeyDesc.modulusSize = 0x100u;
		pubKeyDesc.exponent = pubKeyBuf + 0x100u;
		pubKeyDesc.exponentSize = 4u;
		status = cfVerifyRsaPkcs1v15(workBuf, signature256, &pubKeyDesc,
				digest, 1u, zeroPad);
	}
	return status;
}

/* 0x80b8e0 (name ours) */
int cfAuthenticateAndDecryptHeaderWithKeyCb(const SelfAuthContext *ctx,
		CfGetKeysCallback getKeysCb, void *getKeysCbArg,
		int allowDevelopment, int16_t expectedCategory,
		SelfHeaderReadCallback readCb, void *readCbArg,
		int32_t expectedExtHeaderSize, const int64_t *expectedHeaderLen)
{
	int64_t expHdrLen = *expectedHeaderLen;
	uint64_t readOffset;
	uint32_t bytesRead = 0u;
	cf_header *cfHdr;
	uint32_t scratchSize;
	uint32_t extHdrSize;
	uint32_t minHeaderEnd;
	uint32_t headerLenLo;
	const uint8_t *metaKey = NULL;
	const uint8_t *rootPubKey = NULL;
	uint32_t sigType = 0u;
	const uint8_t *rootKey = NULL;
	uint32_t cipherDesc[3];
	int32_t sigSize = 0;
	SceCfMetadataHeader *mh;
	uint8_t *metaKeyBlock;
	uint32_t sigInputLen;
	int status;

	if (ctx == NULL || ctx->hdrBuf == NULL || getKeysCb == NULL ||
	    readCb == NULL || ctx->hdrBufSize < 0x30u) {
		return SCE_SBL_ERROR_CF_INVALID_VALUE;
	}

	cfHdr = (cf_header *)ctx->hdrBuf;
	readOffset = 0ULL;
	if (readCb((uintptr_t)cfHdr, &readOffset, 0x30u, &bytesRead, readCbArg) != 0 ||
	    bytesRead != 0x30u) {
		return SCE_SBL_ERROR_CF_IO;
	}

	scratchSize = ctx->hdrBufSize;
	if (cfHdr->magic != 0x00454353u || cfHdr->version != 3u ||
	    ((uint8_t)cfHdr->attribute & 0x40u) == 0u ||
	    ((int8_t)(uint8_t)cfHdr->attribute < 0 && allowDevelopment == 0) ||
	    (int16_t)cfHdr->category != expectedCategory) {
		return SCE_SBL_ERROR_CF_BAD_HEADER;
	}

	extHdrSize = cfHdr->ext_header_size;
	if ((extHdrSize & 0xfu) != 0u ||
	    (expectedExtHeaderSize >= 0 && (uint32_t)expectedExtHeaderSize != extHdrSize)) {
		return SCE_SBL_ERROR_CF_BAD_HEADER;
	}

	if (expHdrLen >= 0 && (int64_t)cfHdr->file_offset != expHdrLen)
		return SCE_SBL_ERROR_CF_BAD_HEADER;

	minHeaderEnd = extHdrSize + 0x90u;
	if (minHeaderEnd < extHdrSize || minHeaderEnd > scratchSize)
		return SCE_SBL_ERROR_CF_BAD_HEADER;

	headerLenLo = (uint32_t)cfHdr->file_offset;
	if ((headerLenLo & 0xfu) != 0u)
		return SCE_SBL_ERROR_CF_BAD_HEADER;
	if ((cfHdr->file_offset >> 32) != 0u || headerLenLo < minHeaderEnd ||
	    headerLenLo > scratchSize) {
		return SCE_SBL_ERROR_CF_BAD_HEADER;
	}
	if (*(const uint64_t *)cfHdr->versionExtension != 0ULL)
		return SCE_SBL_ERROR_CF_BAD_HEADER;
	if (headerLenLo > scratchSize)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	readOffset = 0x30ULL;
	if (readCb((uintptr_t)ctx->hdrBuf + 0x30u, &readOffset,
			headerLenLo - 0x30u, &bytesRead, readCbArg) != 0 ||
	    bytesRead != headerLenLo - 0x30u) {
		return SCE_SBL_ERROR_CF_IO;
	}

	status = getKeysCb(&metaKey, &rootPubKey, &sigType, &rootKey, getKeysCbArg);
	if (status != 0)
		return SCE_SBL_ERROR_CF_IO;

	if (metaKey == NULL)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	metaKeyBlock = ctx->hdrBuf + 0x30u + extHdrSize;
	cipherDesc[0] = 0u;
	cipherDesc[1] = 0u;
	cipherDesc[2] = 0u;

	if (rootKey != NULL) {
		status = cfAesCipher(1u, 0u, 0u, metaKeyBlock, metaKeyBlock, 0x40u,
				rootKey, (void *)(rootKey + 0x10u), NULL, cipherDesc);
		if (status != 0)
			return status;
	}

	status = cfAesCipher(1u, 0u, 2u, metaKeyBlock, metaKeyBlock, 0x40u,
			metaKey, (void *)(metaKey + 0x20u), NULL, cipherDesc);
	if (status != 0)
		return status;

	status = cfAesCipher(1u, 0u, 0u,
			ctx->hdrBuf + extHdrSize + 0x70u,
			ctx->hdrBuf + extHdrSize + 0x70u,
			headerLenLo - (extHdrSize + 0x70u),
			metaKeyBlock, metaKeyBlock + 0x20u, NULL, cipherDesc);
	if (status != 0)
		return status;

	if (rootPubKey == NULL)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;
	if (cfGetSignatureSize(sigType, (uint32_t *)&sigSize) != 0)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	if ((int8_t)ctx->hdrBuf[8] < 0) {
		int64_t expCopy = expHdrLen;

		mh = cfGetMetadataHeader(ctx);
		mh->signatureInputLength = cfHdr->file_offset - (uint64_t)(int64_t)sigSize;
		mh->signatureType = sigType;
		mh->sectionCount = 0u;
		mh->keyCount = 4u;
		mh->optHeaderSize = 0u;
		mh->reserved = 0ULL;
		status = cfValidateMetadataHeaderBounds((const uint8_t *)cfHdr, mh,
				sigType, &expCopy);
		if (status != 0)
			return status;
		sigInputLen = (uint32_t)mh->signatureInputLength;
	} else {
		SceCfMetadataSection *sections;
		int64_t expCopy = expHdrLen;
		uint32_t i;

		mh = cfGetMetadataHeader(ctx);
		sections = cfGetMetadataSections(ctx);
		status = cfValidateMetadataHeaderBounds((const uint8_t *)cfHdr, mh,
				sigType, &expCopy);
		if (status != 0)
			return status;
		if (mh == NULL || sections == NULL)
			return SCE_SBL_ERROR_CF_INVALID_VALUE;

		for (i = 0u; i < mh->sectionCount; i++) {
			const SceCfMetadataSection *sec = &sections[i];
			uint32_t hashKeySlots[8];
			uint64_t secEnd;
			uint32_t hashNeed;

			if (sec->offset < cfHdr->file_offset)
				return SCE_SBL_ERROR_CF_BAD_HEADER;
			secEnd = sec->offset + sec->size;
			if (secEnd < sec->offset || secEnd < sec->size ||
			    secEnd > cfHdr->cf_file_size) {
				return SCE_SBL_ERROR_CF_BAD_HEADER;
			}

			memcpy(hashKeySlots, s_cfHashKeySlots, sizeof(hashKeySlots));
			if (sec->hashAlgorithm - 1u > 6u)
				return SCE_SBL_ERROR_CF_BAD_HEADER;
			hashNeed = hashKeySlots[sec->hashAlgorithm];
			if ((hashNeed >> 31) != (uint32_t)(hashNeed + sec->hashKeyIndex < hashNeed) ||
			    hashNeed + sec->hashKeyIndex > mh->keyCount) {
				return SCE_SBL_ERROR_CF_BAD_HEADER;
			}

			if (sec->encryptionAlgorithm == 1u) {
				if (sec->keyIndex != 0xffffffffu || sec->ivIndex != 0xffffffffu)
					return SCE_SBL_ERROR_CF_BAD_HEADER;
			} else {
				uint32_t encKeySlots[6];
				uint32_t encIvSlots[6];
				uint32_t kNeed;
				uint32_t ivNeed;

				memcpy(encKeySlots, s_cfEncKeySlots, sizeof(encKeySlots));
				memcpy(encIvSlots, s_cfEncIvSlots, sizeof(encIvSlots));
				if (sec->encryptionAlgorithm - 1u > 4u)
					return SCE_SBL_ERROR_CF_BAD_HEADER;
				kNeed = encKeySlots[sec->encryptionAlgorithm];
				ivNeed = encIvSlots[sec->encryptionAlgorithm];
				if ((kNeed >> 31) != (uint32_t)(kNeed + sec->keyIndex < kNeed) ||
				    kNeed + sec->keyIndex > mh->keyCount ||
				    (ivNeed >> 31) != (uint32_t)(ivNeed + sec->ivIndex < ivNeed) ||
				    ivNeed + sec->ivIndex > mh->keyCount) {
					return SCE_SBL_ERROR_CF_BAD_HEADER;
				}
			}

			if (sec->compressionAlgorithm - 1u > 1u)
				return SCE_SBL_ERROR_CF_BAD_HEADER;
		}
		sigInputLen = (uint32_t)mh->signatureInputLength;
	}

	if (sigType == 5u) {
		status = cfVerifyRsaSignature(ctx->hdrBuf, sigInputLen,
				ctx->hdrBuf + sigInputLen, rootPubKey);
		if (status == 0)
			return 0;
	} else {
		status = SCE_SBL_ERROR_CF_UNSUPPORTED_ALGO;
	}

	if ((int8_t)ctx->hdrBuf[8] < 0)
		return 0;
	return status;
}

/* 0x80bddc (name ours; Sony method self_loader::open) */
int selfInitAndVerifyHeader(const SelfAuthContext *ctx, const void *metaKey,
		const void *rootPubKey, uint32_t sigType, int allowDevelopment,
		uint16_t expectedCategory, SelfHeaderReadCallback readCb,
		void *readCbArg, int32_t expectedExtHeaderSize,
		const int64_t *expectedHeaderLen)
{
	int64_t expLen = *expectedHeaderLen;
	CfStaticKeyParams keyParams;

	keyParams.metaKey = metaKey;
	keyParams.rootPubKey = rootPubKey;
	keyParams.sigType = sigType;
	return cfAuthenticateAndDecryptHeaderWithKeyCb(ctx, cfDefaultGetKeysCallback,
			&keyParams, allowDevelopment, (int16_t)expectedCategory,
			readCb, readCbArg, expectedExtHeaderSize, &expLen);
}

/* 0x80be1e (name ours) */
int cfGetSectionCount(const SelfAuthContext *ctx, uint32_t *outCount)
{
	if (ctx == NULL || outCount == NULL)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	*outCount = cfGetMetadataHeader(ctx)->sectionCount;
	return 0;
}

/* 0x80be48 (name ours) */
int cfGetSectionEntry(const SelfAuthContext *ctx, uint32_t index,
		const SceCfMetadataSection **outSection)
{
	if (ctx == NULL || outSection == NULL)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;
	if ((int8_t)ctx->hdrBuf[8] < 0)
		return SCE_SBL_ERROR_CF_NOT_FOUND;
	if (index >= cfGetMetadataHeader(ctx)->sectionCount)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	*outSection = &cfGetMetadataSections(ctx)[index];
	return 0;
}

/* 0x80beae (name ours) */
int cfFindSectionEntryByTypeAndIndex(const SelfAuthContext *ctx,
		uint32_t segType, uint32_t segIndex,
		const SceCfMetadataSection **outSection)
{
	uint32_t secCount;
	uint32_t i;

	if (ctx == NULL || outSection == NULL)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;
	if ((int8_t)ctx->hdrBuf[8] < 0)
		return SCE_SBL_ERROR_CF_NOT_FOUND;

	secCount = cfGetMetadataHeader(ctx)->sectionCount;
	for (i = 0u; i < secCount; i++) {
		const SceCfMetadataSection *sec = &cfGetMetadataSections(ctx)[i];

		if (sec->type == segType && sec->index == segIndex) {
			*outSection = sec;
			return 0;
		}
	}
	return SCE_SBL_ERROR_CF_NOT_FOUND;
}

/* 0x80bf3c (name ours) */
int cfGetExtHeaderAndMetaOffset(const SelfAuthContext *ctx,
		const uint8_t **outExtHdr, uint32_t *outMetaOffset)
{
	const uint8_t *base;

	if (ctx == NULL || outExtHdr == NULL || outMetaOffset == NULL)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	base = ctx->hdrBuf;
	*outExtHdr = base + 0x30u;
	*outMetaOffset = *(const uint32_t *)(base + 0x0cu);
	return 0;
}

/* 0x80bf5c (name ours) */
int cfGetOptHeaders(const SelfAuthContext *ctx,
		const uint8_t **outOptHdr, uint32_t *outOptHdrSize)
{
	uint8_t *keyPool;

	if (ctx == NULL || outOptHdr == NULL || outOptHdrSize == NULL)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	keyPool = cfGetMetadataKeyPool(ctx);
	*outOptHdr = keyPool + cfGetMetadataHeader(ctx)->keyCount * 0x10u;
	*outOptHdrSize = cfGetMetadataHeader(ctx)->optHeaderSize;
	return 0;
}

/* 0x80bfb0 (name ours) */
int cfDecryptAndVerifySection(const SelfAuthContext *ctx, uint32_t sectionIndex,
		SelfSegmentReadCallback readCb, void *readCbArg,
		SelfSegmentDoneCallback doneCb, void *doneCbArg)
{
	CfSegmentDecryptState state;
	const SceCfMetadataSection *sec;
	uint64_t secOff;
	uint64_t secSize;
	int status;

	if (ctx == NULL || readCb == NULL ||
	    sectionIndex >= cfGetMetadataHeader(ctx)->sectionCount) {
		return SCE_SBL_ERROR_CF_INVALID_VALUE;
	}

	status = cfSegmentDecryptInit(&state, ctx, sectionIndex, NULL);
	if (status != 0)
		return status;

	sec = &cfGetMetadataSections(ctx)[sectionIndex];
	secOff = sec->offset;
	secSize = sec->size;
	status = cfSegmentDecryptUpdate(&state, &secOff, &secSize,
			readCb, readCbArg, doneCb, doneCbArg);
	if (status == 0)
		status = cfSegmentDecryptFinal(&state);
	return status;
}

/* 0x80c078 (name ours; Sony method self_loader::load_segment) */
int selfDecryptAndLoadSegment(const SelfAuthContext *ctx, uint32_t segType,
		uint32_t segIndex, SelfSegmentReadCallback readCb, void *readCbArg,
		SelfSegmentDoneCallback doneCb, void *doneCbArg)
{
	uint32_t secCount = cfGetMetadataHeader(ctx)->sectionCount;
	uint32_t i;

	for (i = 0u; i < secCount; i++) {
		const SceCfMetadataSection *sec = &cfGetMetadataSections(ctx)[i];

		if (sec->type == segType && sec->index == segIndex) {
			return cfDecryptAndVerifySection(ctx, i, readCb, readCbArg,
					doneCb, doneCbArg);
		}
	}
	return SCE_SBL_ERROR_CF_NOT_FOUND;
}

/* 0x80c108 (name ours) */
int cfStreamSegmentInitThunk(CfSegmentDecryptState *state,
		const SelfAuthContext *ctx, uint32_t sectionIndex,
		const SceCfMetadataSection *secOverride)
{
	return cfSegmentDecryptInit(state, ctx, sectionIndex, secOverride);
}

/* 0x80c10c (name ours) */
int cfStreamSegmentInitByTypeAndIndex(CfSegmentDecryptState *state,
		const SelfAuthContext *ctx, uint32_t segType, uint32_t segIndex)
{
	uint32_t secCount = cfGetMetadataHeader(ctx)->sectionCount;
	uint32_t i;

	for (i = 0u; i < secCount; i++) {
		const SceCfMetadataSection *sec = &cfGetMetadataSections(ctx)[i];

		if (sec->type == segType && sec->index == segIndex)
			return cfSegmentDecryptInit(state, ctx, i, NULL);
	}
	return SCE_SBL_ERROR_CF_NOT_FOUND;
}

/* 0x80c18c (name ours) */
int cfStreamSegmentFinal(CfSegmentDecryptState *state)
{
	if (state == NULL)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;
	return cfSegmentDecryptFinal(state);
}

/* 0x80c1a2 (name ours) */
int cfStreamSegmentUpdateRange(CfSegmentDecryptState *state,
		const uint64_t *offset, const uint64_t *size,
		SelfSegmentReadCallback readCb, void *readCbArg,
		SelfSegmentDoneCallback doneCb, void *doneCbArg)
{
	uint64_t reqOff = *offset;
	uint64_t reqSize = *size;

	if (state == NULL || readCb == NULL)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;
	if (reqOff < state->section.offset ||
	    reqOff >= state->section.offset + state->section.size ||
	    reqSize > state->section.size ||
	    reqOff - state->section.offset > state->section.size - reqSize) {
		return SCE_SBL_ERROR_CF_INVALID_VALUE;
	}

	return cfSegmentDecryptUpdate(state, &reqOff, &reqSize,
			readCb, readCbArg, doneCb, doneCbArg);
}

/* 0x80c290 (name ours; Sony method self_loader::verify_header) */
int selfVerifySvnAndDecryptMetadata(const SelfAuthContext *ctx)
{
	const uint32_t *extHdr = NULL;
	uint32_t metaOffset = 0u;
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
	uint32_t secCount = 0u;
	const SceCfMetadataSection *sections = NULL;
	const uint8_t *optHdrBase = NULL;
	uint32_t optHdrSize = 0u;
	uint32_t sIdx;
	int status;

	if (ctx == NULL)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;
	if (cfGetExtHeaderAndMetaOffset(ctx, (const uint8_t **)&extHdr, &metaOffset) != 0)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	if (extHdr[0] != 4u || extHdr[1] != 0u)
		return SCE_SBL_ERROR_CF_BAD_HEADER;

	elfOff = extHdr[4];
	if ((elfOff & 0xfu) != 0u)
		return SCE_SBL_ERROR_CF_BAD_HEADER;

	maxExtEnd = metaOffset + 0x30u;
	if (extHdr[5] != 0u || elfOff > maxExtEnd || metaOffset < 0x40u ||
	    elfOff + 0x40u < elfOff || elfOff + 0x40u > maxExtEnd || elfOff <= 0x7fu) {
		return SCE_SBL_ERROR_CF_BAD_HEADER;
	}

	base = ctx->hdrBuf;
	sceVerOff = extHdr[12];
	segInfoOff = extHdr[10];
	ehdr = (const Elf32_Ehdr *)(base + elfOff);

	if (ehdr->e_type != ET_EXEC && ehdr->e_type != 0xfe00u &&
	    ehdr->e_type != ET_SCE_FIXEDEXEC) {
		return SCE_SBL_ERROR_CF_BAD_HEADER;
	}

	appInfoOff = extHdr[2];
	if ((appInfoOff & 0xfu) != 0u || extHdr[3] != 0u || appInfoOff > maxExtEnd ||
	    appInfoOff + 0x20u < appInfoOff || appInfoOff + 0x20u > maxExtEnd ||
	    appInfoOff <= 0x7fu) {
		return SCE_SBL_ERROR_CF_BAD_HEADER;
	}

	if ((sceVerOff & 0xfu) != 0u || extHdr[13] != 0u || sceVerOff > maxExtEnd ||
	    sceVerOff + 0x10u < sceVerOff || sceVerOff + 0x10u > maxExtEnd ||
	    sceVerOff <= 0x7fu) {
		return SCE_SBL_ERROR_CF_BAD_HEADER;
	}

	ctrlInfoOff = extHdr[14];
	if (ctrlInfoOff != 0u || extHdr[15] != 0u) {
		if ((ctrlInfoOff & 0xfu) != 0u || extHdr[15] != 0u ||
		    ctrlInfoOff > maxExtEnd || ctrlInfoOff + 0x10u < ctrlInfoOff ||
		    ctrlInfoOff + 0x10u > maxExtEnd || ctrlInfoOff <= 0x7fu) {
			return SCE_SBL_ERROR_CF_BAD_HEADER;
		}
	}

	if (ehdr->e_ident[EI_CLASS] != ELFCLASS32)
		return SCE_SBL_ERROR_CF_BAD_HEADER;

	if ((segInfoOff & 0xfu) != 0u || extHdr[11] != 0u || segInfoOff > maxExtEnd)
		return SCE_SBL_ERROR_CF_BAD_HEADER;

	phnum = ehdr->e_phnum;
	phdrTableSize = phnum * 0x20u;
	if (phdrTableSize > metaOffset ||
	    phdrTableSize + segInfoOff < phdrTableSize ||
	    phdrTableSize + segInfoOff > maxExtEnd || segInfoOff <= 0x7fu) {
		return SCE_SBL_ERROR_CF_BAD_HEADER;
	}

	phdrOff = extHdr[6];
	fixedHdrOverhead = 0xb0u;
	if (phdrOff != 0u || extHdr[7] != 0u) {
		if ((phdrOff & 0xfu) != 0u || extHdr[7] != 0u || phdrOff > maxExtEnd ||
		    phdrTableSize + 0xfu < phdrTableSize || phdrTableSize > metaOffset ||
		    phdrTableSize + phdrOff < phdrTableSize ||
		    phdrTableSize + phdrOff > maxExtEnd || phdrOff <= 0x7fu) {
			return SCE_SBL_ERROR_CF_BAD_HEADER;
		}
		fixedHdrOverhead = phdrTableSize + 0xb0u;
	}

	ctrlInfoSize = extHdr[16];
	{
		uint32_t verExtra = *(const uint32_t *)(base + sceVerOff + 4u);
		uint64_t totalExt = (uint64_t)ctrlInfoSize + 0x10ULL +
				    (uint64_t)phdrTableSize +
				    (uint64_t)verExtra * 0x30ULL +
				    (uint64_t)fixedHdrOverhead;

		if ((totalExt >> 32) != 0ULL || extHdr[17] != 0u ||
		    (uint32_t)totalExt > metaOffset || ctrlInfoSize > metaOffset) {
			return SCE_SBL_ERROR_CF_BAD_HEADER;
		}
	}

	if ((int8_t)base[8] >= 0) {
		status = cfGetSectionCount(ctx, &secCount);
		if (status != 0)
			return status;
		status = cfGetSectionEntry(ctx, 0u, &sections);
		if (status != 0)
			return status;

		for (sIdx = 0u; sIdx < secCount; sIdx++) {
			const uint32_t *segInfo = (const uint32_t *)(base + segInfoOff);
			const uint32_t *secWords = (const uint32_t *)&sections[sIdx];
			uint32_t pIdx;

			for (pIdx = 0u; pIdx < phnum; pIdx++, segInfo += 8) {
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
					return SCE_SBL_ERROR_CF_BAD_HEADER;
			}
			if (pIdx == phnum)
				return SCE_SBL_ERROR_CF_NOT_FOUND;
		}
	}

	{
		const SceSelfHeader *selfHdr = (const SceSelfHeader *)ctx->hdrBuf;

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
					if ((int8_t)ctx->hdrBuf[8] < 0) {
						if (ci[4] != 0u)
							return SCE_SBL_ERROR_CF_BAD_HEADER;
					} else if (ci[4] != 0x4d52447fu) {
						return SCE_SBL_ERROR_CF_BAD_HEADER;
					}
					entrySize = 0x110u;
				} else if (ciType < 6u) {
					if (ciType != 4u)
						return SCE_SBL_ERROR_CF_BAD_HEADER;
					if (timingsafeBcmp(&ci[4], s_selfControlDigestConstant, 20u) != 0)
						return SCE_SBL_ERROR_CF_BAD_HEADER;
					if (ci[17] != 0u)
						return SCE_SBL_ERROR_CF_BAD_HEADER;
					entrySize = 0x50u;
				} else if (ciType == 6u) {
					if (ci[4] != 1u)
						return SCE_SBL_ERROR_CF_BAD_HEADER;
					entrySize = 0x110u;
				} else if (ciType == 7u) {
					entrySize = 0x50u;
				} else {
					return SCE_SBL_ERROR_CF_BAD_HEADER;
				}

				curCtrlOff += entrySize;
				if (curCtrlOff > totalCtrlSize)
					return SCE_SBL_ERROR_CF_BAD_HEADER;
				if (ci[2] == 0u && ci[3] == 0u)
					break;
			}
			if (curCtrlOff != totalCtrlSize)
				return SCE_SBL_ERROR_CF_BAD_HEADER;
		}
	}

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
				return SCE_SBL_ERROR_CF_BAD_HEADER;
			}

			if (nextOptOff < curOptOff || nextOptOff > optHdrSize)
				return SCE_SBL_ERROR_CF_BAD_HEADER;
			curOptOff = nextOptOff;
			if (oh[2] == 0u && oh[3] == 0u)
				break;
		}
		if (curOptOff != optHdrSize)
			return SCE_SBL_ERROR_CF_BAD_HEADER;
	}

	return 0;
}

/* 0x80c74c (name ours) */
int selfGetAppInfo(const SelfAuthContext *ctx, const uint8_t **outAppInfo)
{
	if (ctx == NULL || outAppInfo == NULL)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	*outAppInfo = ctx->hdrBuf + *(const uint32_t *)(ctx->hdrBuf + 0x38u);
	return 0;
}

/* 0x80c766 (name ours; Sony method self_loader::get_elf_header) */
int selfGetElfHeader(const SelfAuthContext *ctx, const void **outEhdr)
{
	if (ctx == NULL || outEhdr == NULL)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	*outEhdr = ctx->hdrBuf + *(const uint32_t *)(ctx->hdrBuf + 0x40u);
	return 0;
}

/* 0x80c780 (name ours; Sony method self_loader::get_program_segment_info) */
int selfGetProgramHeaders(const SelfAuthContext *ctx, const void **outPhdrs)
{
	const uint8_t *base;
	uint32_t phdrOffLo;
	uint32_t phdrOffHi;

	if (ctx == NULL || outPhdrs == NULL)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	base = ctx->hdrBuf;
	phdrOffLo = *(const uint32_t *)(base + 0x48u);
	phdrOffHi = *(const uint32_t *)(base + 0x4cu);
	if (phdrOffLo == 0u && phdrOffHi == 0u)
		return SCE_SBL_ERROR_CF_NOT_FOUND;

	*outPhdrs = base + phdrOffLo;
	return 0;
}

/* 0x80c7aa (name ours; Sony method self_loader::get_program_segment_info) */
int selfGetSegmentInfos(const SelfAuthContext *ctx, const void **outSegInfos)
{
	if (ctx == NULL || outSegInfos == NULL)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	*outSegInfos = ctx->hdrBuf + *(const uint32_t *)(ctx->hdrBuf + 0x58u);
	return 0;
}

/* 0x80c7c4 (name ours) */
int selfGetControlInfo(const SelfAuthContext *ctx, const uint8_t **outCtrlInfo,
		uint64_t *outCtrlInfoSize)
{
	const SceSelfHeader *selfHdr;

	if (ctx == NULL || outCtrlInfo == NULL || outCtrlInfoSize == NULL)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	selfHdr = (const SceSelfHeader *)ctx->hdrBuf;
	if (selfHdr->controlInfoOffset == 0ULL)
		return SCE_SBL_ERROR_CF_NOT_FOUND;

	*outCtrlInfo = (const uint8_t *)selfHdr + (uint32_t)selfHdr->controlInfoOffset;
	*outCtrlInfoSize = selfHdr->controlInfoSize;
	return 0;
}

/* 0x80c802 (name ours) */
int selfFindControlInfo(const SelfAuthContext *ctx, const uint8_t **outCtrlInfo,
		uint32_t targetType)
{
	const SceSelfHeader *selfHdr;
	const uint8_t *ctrlBase;
	uint64_t totalCtrlSize;
	uint64_t curOff = 0ULL;

	if (ctx == NULL || outCtrlInfo == NULL)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	selfHdr = (const SceSelfHeader *)ctx->hdrBuf;
	if (selfHdr->controlInfoOffset == 0ULL)
		return SCE_SBL_ERROR_CF_NOT_FOUND;

	ctrlBase = (const uint8_t *)selfHdr + (uint32_t)selfHdr->controlInfoOffset;
	totalCtrlSize = selfHdr->controlInfoSize;

	while (curOff < totalCtrlSize) {
		const uint32_t *ci = (const uint32_t *)(ctrlBase + (uint32_t)curOff);
		uint32_t ciType = ci[0];
		uint32_t entrySize;

		if (ciType == targetType) {
			*outCtrlInfo = ctrlBase + (uint32_t)curOff;
			return 0;
		}
		if (ciType == 5u || ciType == 6u)
			entrySize = 0x110u;
		else if (ciType == 4u || ciType == 7u)
			entrySize = 0x50u;
		else
			return SCE_SBL_ERROR_CF_BAD_HEADER;

		curOff += entrySize;
		if (curOff > totalCtrlSize)
			return SCE_SBL_ERROR_CF_BAD_HEADER;
		if (ci[2] == 0u && ci[3] == 0u)
			break;
	}

	if (curOff == totalCtrlSize)
		return SCE_SBL_ERROR_CF_NOT_FOUND;
	return SCE_SBL_ERROR_CF_BAD_HEADER;
}

/* 0x80c8f2 (name ours) */
int selfFindExtHeader(const SelfAuthContext *ctx, const uint8_t **outExtHdr,
		uint32_t targetType)
{
	const uint8_t *optHdrBase = NULL;
	uint32_t optHdrSize = 0u;
	uint32_t curOff;

	if (cfGetOptHeaders(ctx, &optHdrBase, &optHdrSize) != 0)
		return SCE_SBL_ERROR_CF_NOT_FOUND;

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
			return SCE_SBL_ERROR_CF_BAD_HEADER;
		}

		if (nextOff < curOff || nextOff > optHdrSize)
			return SCE_SBL_ERROR_CF_BAD_HEADER;
		curOff = nextOff;
		if (oh[2] == 0u && oh[3] == 0u)
			break;
	}

	if (curOff == optHdrSize)
		return SCE_SBL_ERROR_CF_NOT_FOUND;
	return SCE_SBL_ERROR_CF_BAD_HEADER;
}

/* 0x80c998 (name ours) */
int cfAesCipher(uint32_t chainSelect, uint32_t dirSelect, uint32_t keyBitsSelect,
		void *dst, const void *src, uint32_t size, const void *key,
		void *ivIn, void *ivOut, void *unusedDesc)
{
	uint32_t bigmacChain;
	uint32_t bigmacDir;
	uint32_t bigmacKeySize;
	uint32_t localIv[4];
	uint32_t *ivPtr;
	uint32_t bmCtrl[2];
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
		return SCE_SBL_ERROR_CF_INVALID_VALUE;
	}

	if (dirSelect == 0u)
		bigmacDir = 2u;
	else if (dirSelect == 1u)
		bigmacDir = 1u;
	else
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	if (keyBitsSelect == 1u)
		bigmacKeySize = 2u;
	else if (keyBitsSelect == 0u)
		bigmacKeySize = 1u;
	else if (keyBitsSelect == 2u)
		bigmacKeySize = 3u;
	else
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	if ((size & 0xfu) != 0u || (((uintptr_t)src | (uintptr_t)dst) & 3u) != 0u)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	if (bigmacChain != 0u) {
		if (ivOut != NULL && ivIn == ivOut) {
			ivPtr = (uint32_t *)ivOut;
		} else {
			memcpy(localIv, ivIn, 16u);
			ivPtr = localIv;
		}
		if (bigmacChain == 4u)
			bigmacReverseBswap128(ivPtr);
	} else {
		ivPtr = NULL;
	}

	bigmacInitChannel0(bmCtrl);
	status = bigmacStartAesCipher(dst, src, size, key, ivPtr,
			bigmacDir, bigmacChain, bigmacKeySize, bmCtrl);
	if (status != 0)
		return status;

	status = bigmacWaitChannelDone(bmCtrl);
	if (status != 0)
		return status;

	if (bigmacChain != 0u && ivOut != NULL) {
		if (bigmacChain == 4u)
			bigmacReverseBswap128(ivPtr);
		if (ivPtr != (uint32_t *)ivOut)
			memcpy(ivOut, ivPtr, 16u);
	}
	return status;
}

/* 0x80cae0 (name ours) */
int cfCopyBytes(void *dst, const void *src, uint32_t size, void *unusedDesc)
{
	uint32_t offset = 0u;
	int status;

	(void)unusedDesc;
	if (size <= 16u) {
		memcpy(dst, src, size);
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

/* 0x80cb3a (name ours) */
int cfHmacSha256InitShortKey(CfHmacContext *ctx, const void *key, uint32_t keyLen)
{
	uint8_t paddedKey[64];
	const uint8_t *keyBytes = (const uint8_t *)key;
	uint32_t i;

	if (keyLen > 64u)
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	for (i = 0u; i < keyLen; i++)
		paddedKey[i] = keyBytes[i];
	for (; i < 64u; i++)
		paddedKey[i] = 0u;

	return bigmacHmacSha256Init(ctx, paddedKey, 64u);
}

/* 0x80cb9e (name ours) */
int cfVerifyRsaPkcs1v15(void *workBuf256, const void *signature256,
		const RsaPublicKey2048 *pubKey, const void *hash32,
		uint32_t digestAlgo, void *unusedDesc)
{
	BignumPublicKey rsaKey;
	uint32_t oidIndex;

	(void)unusedDesc;
	if (digestAlgo == 1u)
		oidIndex = 4u;
	else if (digestAlgo == 0u)
		oidIndex = 2u;
	else if (digestAlgo == 2u)
		oidIndex = 8u;
	else
		return SCE_SBL_ERROR_CF_INVALID_VALUE;

	rsaKey.modulus = (const uint32_t *)pubKey->modulus;
	rsaKey.modulusWords = pubKey->modulusSize >> 2;
	rsaKey.exponent = (const uint32_t *)pubKey->exponent;
	rsaKey.exponentWords = pubKey->exponentSize >> 2;
	return bignumVerifyRsaPkcs1v15(workBuf256, signature256, &rsaKey, hash32, oidIndex);
}
