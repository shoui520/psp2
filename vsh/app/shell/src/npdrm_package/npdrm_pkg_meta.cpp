#include "npdrm_package_internal.h"

namespace {

static SceUInt32 readBe32(const SceUInt8 *src)
{
	return ((SceUInt32)src[0] << 24)
		| ((SceUInt32)src[1] << 16)
		| ((SceUInt32)src[2] << 8)
		| (SceUInt32)src[3];
}

static SceUInt16 readBe16(const SceUInt8 *src)
{
	return (SceUInt16)(((SceUInt32)src[0] << 8) | (SceUInt32)src[1]);
}

static SceUInt64 readBe64(const SceUInt8 *src)
{
	SceUInt64 highPart = (SceUInt64)readBe32(src);
	SceUInt64 lowPart = (SceUInt64)readBe32(&src[4]);

	return (highPart << 32) | lowPart;
}

static void cmacDoubleSubkey(SceUInt8 subkey[16])
{
	SceUInt8 carryMsb = (SceUInt8)(subkey[0] >> 7);

	for (SceSize idx = 0; idx < 15U; ++idx) {
		subkey[idx] = (SceUInt8)((subkey[idx] << 1) | (subkey[idx + 1U] >> 7));
	}
	subkey[15] = (SceUInt8)(subkey[15] << 1);
	if (carryMsb != 0) {
		subkey[15] ^= 0x87U;
	}
}

static void xorBlock16(SceUInt8 *dst, const SceUInt8 *src)
{
	for (SceSize idx = 0; idx < 16U; ++idx) {
		dst[idx] ^= src[idx];
	}
}

static int isVitaContentType(SceUInt32 contentType)
{
	return contentType == 21U || contentType == 22U || contentType == 23U || contentType == 31U;
}

static int isLegacyPspOrPs1ContentType(SceUInt32 contentType)
{
	return contentType == 6U || contentType == 7U || contentType == 8U
		|| contentType == 14U || contentType == 15U || contentType == 16U
		|| contentType == 17U || contentType == 20U;
}

static SceUInt32 countDigestOffsetAboveBit(SceUInt32 digestMask, SceUInt32 startMask)
{
	SceUInt32 byteOffset = 0;

	for (SceUInt32 bitMask = startMask; bitMask != 0; bitMask <<= 1) {
		if ((digestMask & bitMask) != 0) {
			byteOffset += 32U;
		}
	}
	return byteOffset;
}

} /* namespace */

extern "C" {

void npDrmStatComputeCmac(SceUInt8 *outMac, const SceUInt8 *input, SceSize length, SceUInt32 keySlotId)
{
	SceUInt8 derivedKey[16];
	SceUInt8 subkey[16];
	SceUInt8 lastBlock[16];
	SceAesContext aesCtx;
	SceSize remainder;
	SceSize fullBlocks;

	sceClibMemset(derivedKey, 0, sizeof(derivedKey));
	sceNpDrmPackageDecrypt(derivedKey, sizeof(derivedKey), -1, keySlotId);
	sceAesScheduleKey(&aesCtx, 128U, 128U, derivedKey);

	sceClibMemset(subkey, 0, sizeof(subkey));
	sceAesEncrypt1(&aesCtx, subkey, subkey);
	cmacDoubleSubkey(subkey);

	remainder = length & 15U;
	sceClibMemset(lastBlock, 0, sizeof(lastBlock));
	fullBlocks = (length - remainder) >> 4;

	if (length != 0 && remainder == 0) {
		const SceUInt8 *tailSrc = &input[length - 16U];

		for (SceSize idx = 0; idx < 16U; ++idx) {
			lastBlock[idx] = tailSrc[idx];
		}
		--fullBlocks;
	} else {
		const SceUInt8 *tailSrc = &input[length - remainder];

		for (SceSize idx = 0; idx < remainder; ++idx) {
			lastBlock[idx] = tailSrc[idx];
		}
		lastBlock[remainder] = 0x80U;
		cmacDoubleSubkey(subkey);
	}

	sceClibMemset(outMac, 0, 16U);
	for (SceSize blockIdx = 0; blockIdx < fullBlocks; ++blockIdx) {
		xorBlock16(outMac, &input[blockIdx * 16U]);
		sceAesEncrypt1(&aesCtx, outMac, outMac);
	}
	xorBlock16(outMac, lastBlock);
	xorBlock16(outMac, subkey);
	sceAesEncrypt1(&aesCtx, outMac, outMac);
}

void npDrmStatEncryptCbc(SceUInt8 *buffer, SceSize length, SceUInt32 keySlotId)
{
	SceUInt8 derivedKey[16];
	SceUInt8 initVector[16];
	SceAesContext aesCtx;
	const SceUInt8 *chainBlock;
	SceSize blockCount;

	sceClibMemset(derivedKey, 0, sizeof(derivedKey));
	sceNpDrmPackageDecrypt(derivedKey, sizeof(derivedKey), -1, keySlotId);
	sceClibMemset(initVector, 0, sizeof(initVector));
	sceAesScheduleKey(&aesCtx, 128U, 128U, derivedKey);

	chainBlock = initVector;
	blockCount = length >> 4;
	for (SceSize blockIdx = 0; blockIdx < blockCount; ++blockIdx) {
		SceUInt8 *blockPtr = &buffer[blockIdx * 16U];

		xorBlock16(blockPtr, chainBlock);
		sceAesEncrypt1(&aesCtx, blockPtr, blockPtr);
		chainBlock = blockPtr;
	}
}

void npDrmStatDecryptCbc(SceUInt8 *buffer, SceSize length, SceUInt32 keySlotId)
{
	SceUInt8 derivedKey[16];
	SceUInt8 initVector[16];
	SceAesContext aesCtx;
	SceSize blockCount;
	SceUInt8 *curBlock;

	sceClibMemset(derivedKey, 0, sizeof(derivedKey));
	sceNpDrmPackageDecrypt(derivedKey, sizeof(derivedKey), -1, keySlotId);
	sceClibMemset(initVector, 0, sizeof(initVector));
	sceAesScheduleKey(&aesCtx, 128U, 128U, derivedKey);

	blockCount = length >> 4;
	if (blockCount == 0) {
		return;
	}
	curBlock = &buffer[(blockCount - 1U) * 16U];
	sceAesDecrypt1(&aesCtx, curBlock, curBlock);
	for (SceSize step = blockCount - 1U; step > 0; --step) {
		SceUInt8 *prevBlock = curBlock - 16;

		xorBlock16(curBlock, prevBlock);
		curBlock = prevBlock;
		sceAesDecrypt1(&aesCtx, curBlock, curBlock);
	}
	xorBlock16(curBlock, initVector);
}

int npDrmParsePkgHeaderTlv(SceNpDrmPkgInfo *outInfo, const SceUInt8 *headerBuf, SceSize headerSize,
	NpDrmTlvPointers *outTlvs, SceUInt32 checkFlags)
{
	int err = sceNpDrmPackageCheck(headerBuf, headerSize, NULL, checkFlags);
	if (err != 0) {
		return err;
	}

	SceUInt32 metaOffset = readBe32(&headerBuf[8]);
	SceUInt32 metaCount = readBe32(&headerBuf[12]);
	SceUInt32 metaSize = readBe32(&headerBuf[16]);
	const SceUInt8 *cursor = &headerBuf[metaOffset];
	const SceUInt8 *metaLimit = cursor + metaSize - 64U;

	for (SceUInt32 idx = 0; idx < metaCount; ++idx) {
		SceUInt32 tag = readBe32(cursor);
		SceUInt32 len = readBe32(&cursor[4]);
		const SceUInt8 *valuePtr = &cursor[8];

		if (valuePtr + len > metaLimit) {
			return SCE_NPDRM_PKG_ERROR_INVALID_HEADER;
		}
		switch (tag) {
		case 0x01U: outTlvs->drmType = valuePtr; break;
		case 0x02U: outTlvs->contentType = valuePtr; break;
		case 0x03U: outTlvs->packageType = valuePtr; break;
		case 0x04U: outTlvs->installSize = valuePtr; break;
		case 0x05U: outTlvs->makePkgExe = valuePtr; break;
		case 0x06U: outTlvs->titleId = valuePtr; break;
		case 0x08U: outTlvs->appVersion = valuePtr; break;
		case 0x0AU: outTlvs->digestTable = valuePtr; break;
		case 0x0BU: outTlvs->minFwVersion = valuePtr; break;
		case 0x0DU: outTlvs->itemNamesEnd = valuePtr; break;
		case 0x0EU: outTlvs->paramDigest = valuePtr; break;
		case 0x0FU: outTlvs->selfDigestInfo = valuePtr; break;
		case 0x10U: outTlvs->contentDigestInfo = valuePtr; break;
		case 0x14U: outTlvs->installAttrTable = valuePtr; break;
		default: break;
		}
		cursor = valuePtr + len;
	}

	SceUInt32 itemCount = readBe32(&headerBuf[20]);
	if (itemCount == 0) {
		return SCE_NPDRM_PKG_ERROR_INVALID_ITEM;
	}
	if (outTlvs->drmType == NULL || outTlvs->contentType == NULL || outTlvs->packageType == NULL) {
		return SCE_NPDRM_PKG_ERROR_INVALID_HEADER;
	}

	outInfo->drmType = readBe32(outTlvs->drmType);
	outInfo->contentType = readBe32(outTlvs->contentType);
	outInfo->packageType = readBe32(outTlvs->packageType);
	if ((readBe32(&headerBuf[4]) & 0x80000000U) != 0) {
		outInfo->packageType |= 0x80000000U;
	}
	outInfo->itemCount = itemCount;
	outInfo->installSize = (outTlvs->installSize != NULL) ? readBe64(outTlvs->installSize) : 0ULL;
	outInfo->dataOffset = readBe64(&headerBuf[32]);
	outInfo->totalSize = readBe64(&headerBuf[24]);
	sceClibMemcpy_safe(outInfo->contentId, &headerBuf[48], sizeof(outInfo->contentId));
	if (outTlvs->titleId != NULL && outTlvs->titleId[0] != 0) {
		sceClibMemcpy_safe(outInfo->titleId, outTlvs->titleId, sizeof(outInfo->titleId));
	} else {
		sceClibMemset(outInfo->titleId, 0, sizeof(outInfo->titleId));
		sceClibMemcpy_safe(outInfo->titleId, &headerBuf[55], 9U);
	}
	return 0;
}

int npDrmParsePkgInfo(SceNpDrmPkgInfo *outInfo, const void *headerBuf, SceSize headerSize)
{
	NpDrmTlvPointers tlvs;

	sceClibMemset(&tlvs, 0, sizeof(tlvs));
	return npDrmParsePkgHeaderTlv(outInfo, (const SceUInt8 *)headerBuf, headerSize, &tlvs, 0U);
}

int npDrmValidateItemEntry(const SceUInt8 *rawEntry, SceUInt64 dataAreaSize)
{
	SceUInt32 nameOffset = readBe32(&rawEntry[0]);
	SceUInt32 nameSize = readBe32(&rawEntry[4]);
	SceUInt32 alignedNameEnd = nameOffset + ((nameSize + 15U) & ~15U);

	if (alignedNameEnd < nameOffset) {
		return SCE_NPDRM_PKG_ERROR_INVALID_ITEM_NAME;
	}
	SceUInt64 itemDataOffset = readBe64(&rawEntry[8]);
	SceUInt64 itemDataSize = readBe64(&rawEntry[16]);
	SceUInt64 alignedDataSize = (itemDataSize + 15ULL) & ~15ULL;
	SceUInt64 itemDataEnd = itemDataOffset + alignedDataSize;

	if (itemDataEnd < itemDataOffset) {
		return SCE_NPDRM_PKG_ERROR_INVALID_ITEM_DATA;
	}
	if (itemDataOffset < (SceUInt64)alignedNameEnd) {
		return SCE_NPDRM_PKG_ERROR_INVALID_ITEM_DATA;
	}
	if (dataAreaSize < (SceUInt64)alignedNameEnd) {
		return SCE_NPDRM_PKG_ERROR_INVALID_ITEM_NAME;
	}
	if (dataAreaSize < itemDataEnd) {
		return SCE_NPDRM_PKG_ERROR_INVALID_ITEM_DATA;
	}
	return 0;
}

int npDrmParsePkgMeta(NpDrmPkgMeta *outMeta, const void *headerBuf, SceSize headerSize, SceUInt32 slotId)
{
	const SceUInt8 *headerBytes = (const SceUInt8 *)headerBuf;
	NpDrmTlvPointers tlvs;
	int err;

	sceClibMemset(&tlvs, 0, sizeof(tlvs));
	err = npDrmParsePkgHeaderTlv(&outMeta->pkgInfo, headerBytes, headerSize, &tlvs,
		(slotId & 0xFFU) | 0x100U);
	if (err != 0) {
		return err;
	}

	SceUInt32 itemTableSize = readBe32(&headerBytes[20]) * 32U;
	SceUInt32 itemNamesEnd = 0;

	if (tlvs.itemNamesEnd != NULL) {
		itemNamesEnd = readBe32(&tlvs.itemNamesEnd[4]);
	}
	if (itemNamesEnd == 0) {
		SceUInt64 dataOffset = readBe64(&headerBytes[32]);
		SceUInt64 dataSize = readBe64(&headerBytes[40]);
		SceUInt8 firstEntry[32];

		if ((SceUInt64)headerSize < dataOffset + sizeof(firstEntry)) {
			return SCE_NPDRM_PKG_ERROR_INVALID_ITEM;
		}
		sceClibMemcpy_safe(firstEntry, &headerBytes[dataOffset], sizeof(firstEntry));
		sceNpDrmPackageDecrypt(firstEntry, sizeof(firstEntry), 0, slotId);
		if (readBe32(&firstEntry[0]) != itemTableSize) {
			return SCE_NPDRM_PKG_ERROR_INVALID_ITEM_NAME;
		}
		if (readBe32(&firstEntry[8]) != 0) {
			return SCE_NPDRM_PKG_ERROR_INVALID_ITEM_DATA;
		}
		err = npDrmValidateItemEntry(firstEntry, dataSize);
		if (err != 0) {
			return err;
		}
		itemNamesEnd = readBe32(&firstEntry[12]);
		if (itemNamesEnd == 0) {
			return SCE_NPDRM_PKG_ERROR_INVALID_ITEM;
		}
	}

	SceUInt64 dataOffset = readBe64(&headerBytes[32]);
	SceUInt64 dataSize = readBe64(&headerBytes[40]);
	SceUInt64 totalSize = readBe64(&headerBytes[24]);
	SceUInt64 dataEnd = dataOffset + dataSize;
	int isFinalized = (readBe32(&headerBytes[4]) & 0x80000000U) != 0;

	outMeta->keySlotId = slotId;
	outMeta->itemTableSize = itemTableSize;
	outMeta->itemNamesEnd = itemNamesEnd;
	outMeta->headSize = (SceUInt32)dataOffset + itemNamesEnd;
	outMeta->tailOffset = dataEnd;
	outMeta->tailSize = totalSize - dataEnd;

	if (isFinalized) {
		outMeta->sha1LimitOffset = dataEnd;
		outMeta->tailSigOffset = readBe64(&headerBytes[0xD8]);
		outMeta->tailSigSize = 384U;
	} else {
		outMeta->sha1LimitOffset = totalSize - 32ULL;
		outMeta->tailSigOffset = totalSize - 32ULL;
		outMeta->tailSigSize = 20U;
	}

	SceUInt32 extType = (readBe32(&headerBytes[0xE4]) >> 0) & 7U;
	SceUInt32 contentType = outMeta->pkgInfo.contentType;
	SceUInt32 metaFlags = outMeta->metaFlags;

	if (extType == 1U && !isVitaContentType(contentType)) {
		metaFlags |= NPDRM_META_FLAG_LEGACY_PSP;
	} else {
		metaFlags &= ~NPDRM_META_FLAG_LEGACY_PSP;
	}
	if ((metaFlags & NPDRM_META_FLAG_LEGACY_PSP) != 0 && contentType != 9U) {
		metaFlags |= NPDRM_META_FLAG_USRDIR;
	} else {
		metaFlags &= ~NPDRM_META_FLAG_USRDIR;
	}
	if (isFinalized) {
		metaFlags |= NPDRM_META_FLAG_FINALIZED;
	} else {
		metaFlags &= ~NPDRM_META_FLAG_FINALIZED;
	}
	outMeta->metaFlags = metaFlags;
	return 0;
}

int npDrmPopulateInstallMeta(SceNpDrmInstallInfo *outInfo, const SceUInt8 *headerBuf,
	const SceUInt8 *extHeaderBuf, const NpDrmTlvPointers *tlvs)
{
	if (tlvs->paramDigest != NULL) {
		sceClibMemcpy_safe(outInfo->contentDigest, &tlvs->paramDigest[24], sizeof(outInfo->contentDigest));
		outInfo->contentDigestSize = 32;
	}

	SceUInt32 extKeyType = readBe32(&extHeaderBuf[36]) & 7U;
	int requiresLicense = (extKeyType != 2U && extKeyType != 3U);
	SceUInt32 contentType = readBe32(tlvs->contentType);
	int isFinalized = (readBe32(&headerBuf[4]) & 0x80000000U) != 0;

	if (isVitaContentType(contentType)) {
		int skipRifDecrypt = 0;

		if (tlvs->selfDigestInfo != NULL) {
			skipRifDecrypt = (readBe32(&tlvs->selfDigestInfo[16]) & 0x00000100U) != 0;
		}
		if ((!isFinalized || requiresLicense) && !skipRifDecrypt) {
			SceUInt8 *lic = outInfo->licenseData;
			SceUInt32 pkgFlags = readBe32(tlvs->packageType);
			int decryptRet;

			/* Populate provisional RIF header fields before kernel decrypt */
			lic[0] = 0xFFU;
			lic[1] = 0xFFU;
			lic[2] = 0x00U;
			lic[3] = 0x01U;
			lic[4] = 0x00U;
			lic[5] = 0x01U;
			/* Store big-endian 64-bit provisional start timestamp at licenseData+96 */
			lic[96] = 0x00U;
			lic[97] = 0x00U;
			lic[98] = 0x01U;
			lic[99] = 0x2DU;
			lic[100] = 0x3EU;
			lic[101] = 0xDFU;
			lic[102] = 0xA4U;
			lic[103] = 0x00U;
			/* Store sku flag at licenseData+252 (info+372) */
			lic[252] = 0x00U;
			lic[253] = 0x00U;
			lic[254] = ((pkgFlags & 0x00001000U) != 0) ? 0x01U : 0x00U;
			lic[255] = 0x00U;

			decryptRet = sceNpDrmPackageDecrypt(outInfo->licenseData, sizeof(outInfo->licenseData),
				0, 0x80002000U);
			if (decryptRet < 0) {
				sceClibMemset(outInfo->licenseData, 0, sizeof(outInfo->licenseData));
				outInfo->licenseSize = 0;
				if (requiresLicense && decryptRet == SCE_NPDRM_PKG_ERROR_LICENSE_REQUIRED) {
					return SCE_NPDRM_PKG_ERROR_NO_LICENSE;
				}
				return decryptRet;
			}
			outInfo->licenseSize = (SceInt32)sizeof(outInfo->licenseData);
		}
	} else if (isLegacyPspOrPs1ContentType(contentType)) {
		SceUInt32 drmType = readBe32(tlvs->drmType);

		if (drmType != 2U || !isFinalized) {
			sceClibMemset(outInfo->licenseData, 0, sizeof(outInfo->licenseData));
			outInfo->licenseSize = 0;
		}
	}

	if (contentType == 22U) {
		sceClibMemcpy_safe(outInfo->pkgInfo.qaDigest, &headerBuf[68], sizeof(outInfo->pkgInfo.qaDigest));
		if (outInfo->instSize == 256) {
			int instRet = sceNpDrmPackageDecrypt(outInfo->instData, sizeof(outInfo->instData),
				0, 0x80004000U);
			if (instRet < 0) {
				sceClibMemset(outInfo->instData, 0, sizeof(outInfo->instData));
				outInfo->instSize = 0;
				return instRet;
			}
			outInfo->instSize = 96;
			sceClibMemset(&outInfo->instData[96], 0, 160U);
		}
		if (requiresLicense && outInfo->instSize < 0) {
			return SCE_NPDRM_PKG_ERROR_NO_LICENSE;
		}
	}

	if (tlvs->installAttrTable != NULL) {
		for (SceUInt32 idx = 0; idx < 8U; ++idx) {
			SceUInt8 attrKey = tlvs->installAttrTable[idx];

			if (attrKey == 0) {
				break;
			}
			if (attrKey == 5U) {
				SceUInt32 attrValue = readBe32(&tlvs->installAttrTable[8U + idx * 4U]);

				if (attrValue != 0) {
					outInfo->installAttr = attrValue;
				}
				break;
			}
		}
	}
	return 0;
}

int npDrmParseInstallInfo(SceNpDrmInstallInfo *outInfo, const void *headerBuf, SceSize headerSize)
{
	const SceUInt8 *headerBytes = (const SceUInt8 *)headerBuf;
	NpDrmTlvPointers tlvs;
	int err;

	sceClibMemset(&tlvs, 0, sizeof(tlvs));
	err = npDrmParsePkgHeaderTlv(&outInfo->pkgInfo, headerBytes, headerSize, &tlvs, 0x1000U);
	if (err == 0) {
		err = npDrmPopulateInstallMeta(outInfo, headerBytes, &headerBytes[192], &tlvs);
		sceNpDrmPackageDecrypt(outInfo->licenseData, 256U, 0, 0x2000U);
		sceNpDrmPackageDecrypt(outInfo->instData, 128U, 0, 0x4000U);
	}
	return err;
}

int npDrmParseContentInfo(SceNpDrmContentInfo *outInfo, const void *headerBuf, SceSize headerSize)
{
	const SceUInt8 *headerBytes = (const SceUInt8 *)headerBuf;
	NpDrmTlvPointers tlvs;
	int err;

	sceClibMemset(&tlvs, 0, sizeof(tlvs));
	err = npDrmParsePkgHeaderTlv(&outInfo->pkgInfo, headerBytes, headerSize, &tlvs, 0U);
	if (err != 0) {
		return err;
	}
	if (tlvs.appVersion == NULL || tlvs.selfDigestInfo == NULL || tlvs.contentDigestInfo == NULL) {
		return SCE_NPDRM_PKG_ERROR_NO_LICENSE;
	}

	outInfo->appVersion = readBe16(&tlvs.appVersion[4]);
	if (tlvs.minFwVersion != NULL) {
		SceUInt64 fwWord = readBe64(tlvs.minFwVersion);

		outInfo->minFwVersion = ((fwWord & 0x0000000200000000ULL) == 0)
			? (SceUInt16)((fwWord >> 16) & 0xFFFFU)
			: 0U;
	}

	SceUInt32 selfBaseOffset = readBe32(tlvs.selfDigestInfo);
	if (selfBaseOffset + 512U != 0 && selfBaseOffset + 544U < headerSize) {
		sceClibMemcpy_safe(outInfo->selfDigest, &headerBytes[selfBaseOffset + 512U],
			sizeof(outInfo->selfDigest));
	}

	SceUInt32 digestTableOffset = readBe32(tlvs.contentDigestInfo);
	SceUInt32 digestMask = readBe32(&tlvs.contentDigestInfo[8]);

	if ((digestMask & 0x40000000U) != 0) {
		SceUInt32 sfoOffset = digestTableOffset + countDigestOffsetAboveBit(digestMask, 0x80000000U);

		if (sfoOffset != 0 && sfoOffset + 32U < headerSize) {
			sceClibMemcpy_safe(outInfo->sfoDigest, &headerBytes[sfoOffset], sizeof(outInfo->sfoDigest));
		}
	}
	if ((digestMask & 0x00080000U) != 0) {
		SceUInt32 iconOffset = digestTableOffset + countDigestOffsetAboveBit(digestMask, 0x00100000U);

		if (iconOffset != 0 && iconOffset + 32U < headerSize) {
			sceClibMemcpy_safe(outInfo->iconDigest, &headerBytes[iconOffset], sizeof(outInfo->iconDigest));
		}
	}
	if ((digestMask & 0x00040000U) != 0) {
		SceUInt32 contentOffset = digestTableOffset + countDigestOffsetAboveBit(digestMask, 0x00080000U);

		if (contentOffset != 0 && contentOffset + 32U < headerSize) {
			sceClibMemcpy_safe(outInfo->contentDigest, &headerBytes[contentOffset],
				sizeof(outInfo->contentDigest));
		}
	}
	return 0;
}

SceUInt32 npDrmGetItemEntryOffset(SceUInt32 itemIndex, const NpDrmPkgMeta *pkgMeta)
{
	return (SceUInt32)pkgMeta->pkgInfo.dataOffset + itemIndex * 32U;
}

int npDrmDecodeItemMeta(NpDrmItemMeta *outItem, SceUInt32 itemIndex, const SceUInt8 *rawEntry,
	SceSize entrySize, const NpDrmPkgMeta *pkgMeta)
{
	if (pkgMeta->pkgInfo.itemCount <= itemIndex) {
		return SCE_NPDRM_PKG_ERROR_INVALID_ITEM;
	}
	if (entrySize != 32U) {
		return SCE_NPDRM_PKG_ERROR_BUFFER_TOO_SMALL;
	}

	SceUInt64 dataAreaSize = pkgMeta->tailOffset - pkgMeta->pkgInfo.dataOffset;
	int err = npDrmValidateItemEntry(rawEntry, dataAreaSize);
	if (err != 0) {
		return err;
	}

	SceUInt32 nameOffset = readBe32(&rawEntry[0]);
	SceUInt32 nameSize = readBe32(&rawEntry[4]);
	SceUInt32 alignedNameEnd = nameOffset + ((nameSize + 15U) & ~15U);

	if (nameOffset < pkgMeta->itemTableSize || alignedNameEnd > pkgMeta->itemNamesEnd) {
		return SCE_NPDRM_PKG_ERROR_INVALID_ITEM_NAME;
	}

	SceUInt64 itemDataOffset = readBe64(&rawEntry[8]);
	SceUInt64 itemDataSize = readBe64(&rawEntry[16]);
	if (itemDataOffset < (SceUInt64)pkgMeta->itemNamesEnd) {
		return SCE_NPDRM_PKG_ERROR_INVALID_ITEM_DATA;
	}

	SceUInt32 itemFlags = readBe32(&rawEntry[24]);
	if ((itemFlags & 0x00020000U) != 0 && ((itemFlags >> 12) & 7U) != 1U) {
		return SCE_NPDRM_PKG_ERROR_UNSUPPORTED_ITEM;
	}

	SceUInt64 baseOffset = pkgMeta->pkgInfo.dataOffset;
	SceUInt64 alignedSize = (itemDataSize + 15ULL) & ~15ULL;
	SceUInt8 lowType = (SceUInt8)(itemFlags & 0xFFU);

	outItem->itemIndex = (SceInt32)itemIndex;
	outItem->dataStart = baseOffset + itemDataOffset;
	outItem->alignedEnd = baseOffset + itemDataOffset + alignedSize;
	outItem->dataSize = itemDataSize;
	outItem->nameOffset = (SceUInt32)baseOffset + nameOffset;
	outItem->nameSize = nameSize;
	outItem->itemFlags = itemFlags;
	outItem->isDirectory = (lowType == 4U || lowType == 18U || (itemFlags & 0x00010000U) != 0) ? 1U : 0U;

	SceUInt8 extractable = ((pkgMeta->metaFlags & NPDRM_META_FLAG_LEGACY_PSP) == 0
		|| (itemFlags & 0x00020000U) != 0) ? 1U : 0U;
	if ((itemFlags & 0x00200000U) != 0) {
		extractable = 0U;
	}
	outItem->isExtractable = extractable;
	return 0;
}

int npDrmDecryptPkgRegion(void *buf, SceSize size, SceUInt64 pkgOffset,
	const NpDrmPkgMeta *pkgMeta, SceUInt8 useHwAccel)
{
	SceUInt32 identifier = pkgMeta->keySlotId & 0xFFU;

	if (useHwAccel != 0) {
		identifier |= 0x8000U;
	}
	return sceNpDrmPackageDecrypt(buf, size, (SceOff)(pkgOffset - pkgMeta->pkgInfo.dataOffset), identifier);
}

int npDrmVerifyTailSignature(const void *tailSig, SceSize sigSize,
	const SceUInt8 *sha1Digest, const NpDrmPkgMeta *pkgMeta)
{
	SceUInt8 paddedDigest[32];

	sceClibMemset(paddedDigest, 0, sizeof(paddedDigest));
	for (SceSize idx = 0; idx < 20U; ++idx) {
		paddedDigest[idx] = sha1Digest[idx];
	}
	return sceNpDrmPackageCheck(tailSig, sigSize, paddedDigest,
		(pkgMeta->keySlotId & 0xFFU) | 0x200U);
}

int npDrmDecryptWorkBin(void *workBuf, SceSize workSize)
{
	return sceNpDrmPackageDecrypt(workBuf, workSize, 0, 0x80020006U);
}

} /* extern "C" */
