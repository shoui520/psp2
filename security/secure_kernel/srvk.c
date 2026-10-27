/*
 * srvk.c: Secure Revocation List (prog_rvk.srvk) authentication, loading,
 * hardware / QA / Dipsw target queries, and SM revocation checking
 * (0x8050e2..0x805a69).
 *
 * Internal function and structure names are ours.
 */

#include "secure_kernel.h"

/* ============================================================================
 * SRVK Header & Entry Structures (names ours)
 * ============================================================================ */

typedef struct SceSrvkHeader {
	uint32_t magic;				/* 0x00: 5 */
	uint32_t version;			/* 0x04: 1 */
	uint16_t reserved08;			/* 0x08 */
	uint32_t socVersion;			/* 0x0a: unaligned 32-bit SoC version */
	uint16_t reserved0e;			/* 0x0e */
	uint32_t entryCount;			/* 0x10 */
	uint32_t reserved14[3];			/* 0x14..0x1f */
} __attribute__((packed)) SceSrvkHeader;

_Static_assert(sizeof(SceSrvkHeader) == 0x20, "SceSrvkHeader size");

typedef struct SceSrvkVersionEntry {
	uint16_t type;				/* 0x00: 1 = revoke, 3 = allow */
	uint16_t reserved02;			/* 0x02 */
	uint64_t targetAuthId;			/* 0x04 */
	uint64_t authIdMask;			/* 0x0c */
	uint64_t versionRef;			/* 0x14 */
	uint16_t cmpOp;				/* 0x1c: 0:==, 1:!=, 2:<, 3:<=, 4:>, 5:>= */
	uint16_t reserved1e;			/* 0x1e */
} __attribute__((packed)) SceSrvkVersionEntry;

_Static_assert(sizeof(SceSrvkVersionEntry) == 0x20, "SceSrvkVersionEntry size");

typedef struct SceSrvkDigestEntry {
	uint16_t type;				/* 0x00: 2 = digest revoke */
	uint16_t reserved02;			/* 0x02 */
	uint64_t targetAuthId;			/* 0x04 */
	uint64_t authIdMask;			/* 0x0c */
	uint8_t digest[32];			/* 0x14..0x33 */
} __attribute__((packed)) SceSrvkDigestEntry;

_Static_assert(sizeof(SceSrvkDigestEntry) == 0x34, "SceSrvkDigestEntry size");

typedef struct SceCfSectionHead {
	uint64_t offset;			/* 0x00 */
	uint64_t size;				/* 0x08 */
} SceCfSectionHead;

typedef struct SrvkScatterSegmentCbArg {
	const SceSkBufferDesc *srcDesc;		/* 0x00 */
	uint32_t dstVaddr;			/* 0x04 */
	uint32_t filesz;			/* 0x08 */
	uint32_t pad0c;				/* 0x0c */
	uint64_t segOffset;			/* 0x10 */
	uint64_t segSize;			/* 0x18 */
} SrvkScatterSegmentCbArg;

/* ============================================================================
 * .rodata (0x807868..0x8078a8)
 * ============================================================================ */

/*
 * 0x807886: built-in version rule requiring 0x28000000-series system SMs
 * to match firmware version 4.00 (0x0000040000000000ULL) unless exempted.
 */
static const SceSrvkVersionEntry s_smFwVersionRule = {
	1u,
	0u,
	0x2800000000000000ULL,
	0xfffffffffffff000ULL,
	0x0000040000000000ULL,
	1u,
	0u,
};

/* ============================================================================
 * .data (0x807b10..0x807c4c): SRVK Metadata Key/IV & RSA-2048 Public Key
 * ============================================================================ */

/* 0x807b10: 32-byte AES-256 key + 16-byte IV for SRVK metadata decryption */
static uint8_t s_srvkMetadataKeyIv[48] = {
	0xeau, 0xb1u, 0x4fu, 0x9bu, 0xe1u, 0x5eu, 0xaeu, 0xc1u,
	0x60u, 0x3bu, 0xe6u, 0x3cu, 0x9fu, 0xcdu, 0xe4u, 0x09u,
	0x9du, 0x60u, 0x1fu, 0xb0u, 0xe9u, 0xfcu, 0x4du, 0xf2u,
	0x50u, 0xb8u, 0xdeu, 0xc6u, 0x35u, 0x98u, 0x7au, 0x1cu,
	0x30u, 0xb9u, 0xe6u, 0x17u, 0x07u, 0x99u, 0x3bu, 0x63u,
	0x5du, 0x0eu, 0x18u, 0x24u, 0x46u, 0xdbu, 0x0bu, 0x8du,
};

/* 0x807b40: 256-byte RSA-2048 modulus for SRVK signature verification */
static uint8_t s_srvkRsaModulus[256] = {
	0x9cu, 0x8fu, 0x50u, 0x72u, 0x01u, 0x3fu, 0x91u, 0x4cu,
	0x15u, 0x40u, 0x8eu, 0xdeu, 0x85u, 0xd3u, 0x7eu, 0x4cu,
	0x0bu, 0x6cu, 0xeau, 0x08u, 0xaau, 0xa3u, 0x21u, 0x68u,
	0x0cu, 0x0eu, 0x11u, 0xddu, 0x29u, 0x9du, 0x2au, 0x45u,
	0x06u, 0x71u, 0xb8u, 0x24u, 0x7eu, 0x84u, 0x05u, 0xf5u,
	0x8eu, 0xc0u, 0xdcu, 0xb3u, 0x3eu, 0x1bu, 0xcdu, 0x8du,
	0x40u, 0xe7u, 0x90u, 0x65u, 0x01u, 0x49u, 0xe6u, 0xddu,
	0x2cu, 0x62u, 0xfeu, 0xbau, 0x19u, 0x6fu, 0xe4u, 0xfeu,
	0xaau, 0xd7u, 0x2eu, 0x69u, 0xcfu, 0x22u, 0xd2u, 0x17u,
	0x29u, 0x4fu, 0x61u, 0xe1u, 0xa8u, 0x8fu, 0xf3u, 0x3fu,
	0x45u, 0x25u, 0x5eu, 0x85u, 0x3du, 0x0cu, 0x2eu, 0x1du,
	0x20u, 0x62u, 0x87u, 0xedu, 0x76u, 0xdbu, 0x1cu, 0x8cu,
	0xd7u, 0x58u, 0x12u, 0xddu, 0x7cu, 0x36u, 0xcfu, 0xacu,
	0x0au, 0x97u, 0xf3u, 0x0fu, 0x07u, 0xf0u, 0x2eu, 0xd3u,
	0xe8u, 0xa7u, 0x9au, 0x77u, 0x80u, 0x69u, 0x6eu, 0xb4u,
	0xfdu, 0x24u, 0x82u, 0x9eu, 0x0du, 0x7fu, 0xd0u, 0x54u,
	0x1fu, 0xe4u, 0xbfu, 0x49u, 0x04u, 0x15u, 0xd9u, 0xe0u,
	0xccu, 0x21u, 0x16u, 0xb7u, 0x03u, 0xe2u, 0x94u, 0x7eu,
	0x52u, 0x69u, 0xb6u, 0x39u, 0x41u, 0xffu, 0x51u, 0x1cu,
	0x18u, 0x3eu, 0x4du, 0x2au, 0x08u, 0x18u, 0xf9u, 0x31u,
	0x98u, 0x8du, 0xf3u, 0x6eu, 0xf8u, 0xdcu, 0x97u, 0x2eu,
	0xe2u, 0x70u, 0x93u, 0x36u, 0x01u, 0x8cu, 0x9eu, 0xd5u,
	0xb7u, 0xedu, 0x68u, 0x34u, 0x1fu, 0xfbu, 0x22u, 0x9eu,
	0x82u, 0xc9u, 0x7eu, 0x5cu, 0xe9u, 0x00u, 0xacu, 0xf3u,
	0xe5u, 0xeau, 0x2eu, 0x86u, 0xbfu, 0xd3u, 0x2au, 0x4bu,
	0xd0u, 0xd6u, 0xdbu, 0xe1u, 0x77u, 0x13u, 0xdau, 0x78u,
	0x36u, 0x26u, 0x91u, 0x1cu, 0xf9u, 0x2cu, 0x05u, 0xeau,
	0x5au, 0xe8u, 0x79u, 0xa3u, 0x27u, 0x21u, 0x34u, 0xcfu,
	0x35u, 0xd5u, 0x67u, 0xd5u, 0xfbu, 0xc1u, 0x59u, 0xdeu,
	0xa2u, 0x0au, 0x1bu, 0x05u, 0x50u, 0xa1u, 0x2fu, 0x11u,
	0xe9u, 0xe5u, 0x4du, 0xaau, 0x84u, 0x88u, 0x9cu, 0xf1u,
	0x0bu, 0x72u, 0xc5u, 0x9eu, 0x75u, 0x69u, 0xddu, 0xc1u,
};

/* 0x807c40: modulus size (256), exponent (65537 BE), exponent size (4) */
static uint32_t s_srvkRsaModulusSize __attribute__((used)) = 0x100u;
static uint8_t s_srvkRsaExponent[4] = { 0x00u, 0x01u, 0x00u, 0x01u };
static uint32_t s_srvkRsaExponentSize __attribute__((used)) = 4u;

/* ============================================================================
 * Functions (0x8050e2..0x805a69)
 * ============================================================================ */

/* 0x8050e2 (name ours) */
int srvkLoadAndVerify(uint32_t rangesPaddr, uint32_t numRanges,
		const SceSkBufferDesc *dstRegion, SceSkBufferDesc *outLoadedDesc)
{
	return srvkAuthenticateAndLoad(rangesPaddr, numRanges, dstRegion, outLoadedDesc);
}

/* 0x8050e6 (name ours) */
void srvkReadKeyringSlot32(uint32_t slotIndex, uint32_t *outWords8)
{
	uint32_t off = slotIndex * 0x20u;

	outWords8[0] = REG32(0xe0058000u + off);
	outWords8[1] = REG32(0xe0058004u + off);
	outWords8[2] = REG32(0xe0058008u + off);
	outWords8[3] = REG32(0xe005800cu + off);
	outWords8[4] = REG32(0xe0058010u + off);
	outWords8[5] = REG32(0xe0058014u + off);
	outWords8[6] = REG32(0xe0058018u + off);
	outWords8[7] = REG32(0xe005801cu + off);
}

/* 0x805130 (name ours) */
int srvkGetTargetTypeClass(void)
{
	uint8_t cid[16];
	uint32_t matchCount = 0u;
	uint32_t targetClass = 0u;

	readConsoleIdRaw16Alias2(cid);
	if (aimgrIsRetailOrChord(cid) != 0) {
		matchCount = 1u;
		targetClass = 1u;
	}
	if (aimgrIsTest(cid) != 0 || aimgrIsTool(cid) != 0) {
		matchCount++;
		targetClass = 2u;
	}
	if (aimgrIsDex(cid) != 0) {
		matchCount++;
		targetClass = 3u;
	}
	if (aimgrIsCexSub16Factory36(cid) != 0) {
		matchCount++;
		targetClass = 4u;
	}
	if (matchCount != 1u)
		targetClass = 0u;

	wipeConsoleIdBuffer16(cid);
	return (int)targetClass;
}

/* 0x805198 (name ours) */
int srvkIsDevelopmentTarget(void)
{
	uint8_t cid[16];
	int res;

	readConsoleIdRaw16Alias2(cid);
	res = aimgrIsTestSub4(cid);
	wipeConsoleIdBuffer16(cid);
	return res;
}

/* 0x8051c4 (name ours) */
int srvkIsRetailOrToolTarget(void)
{
	uint8_t cid[16];
	int res;

	readConsoleIdRaw16Alias2(cid);
	res = aimgrIsRetailOrChord(cid);
	wipeConsoleIdBuffer16(cid);
	return res;
}

/* 0x8051f0 (name ours) */
int srvkCheckQafByte0Bit0(void)
{
	uint8_t slot50b[32];

	srvkReadKeyringSlot32(0x50bu, (uint32_t *)slot50b);
	return (int)(slot50b[0] & 1u);
}

/* 0x80520c (name ours) */
int srvkCheckDipswBit195(void)
{
	return (int)((REG32(0xe0062180u) & 8u) >> 3);
}

/* 0x80521e (name ours) */
int srvkCheckDipswBit197(void)
{
	return (int)((REG32(0xe0062180u) & 0x20u) >> 5);
}

/* 0x805230 (name ours) */
int srvkCheckDipswBit195Thunk(void)
{
	return srvkCheckDipswBit195();
}

/* 0x805234 (name ours) */
int srvkCheckQaFlagByte15Bit0(void)
{
	uint8_t slot50a[32];

	srvkReadKeyringSlot32(0x50au, (uint32_t *)slot50a);
	return (int)(slot50a[15] & 1u);
}

/* 0x805256 (name ours) */
int srvkCheckQaFlagByte13Bit0(void)
{
	uint8_t slot50a[32];

	srvkReadKeyringSlot32(0x50au, (uint32_t *)slot50a);
	return (int)(slot50a[13] & 1u);
}

/* 0x805278 (name ours) */
int srvkCheckQaFlagByte14Bit0(void)
{
	uint8_t slot50a[32];

	srvkReadKeyringSlot32(0x50au, (uint32_t *)slot50a);
	return (int)(slot50a[14] & 1u);
}

/* 0x80529a (name ours) */
int srvkCheckQaFlagByte13Bit1(void)
{
	uint8_t slot50a[32];

	srvkReadKeyringSlot32(0x50au, (uint32_t *)slot50a);
	return (int)(slot50a[13] & 2u);
}

/* 0x8052bc (name ours) */
int srvkCheckQaFlagByte11Bit4(void)
{
	uint8_t slot50a[32];

	srvkReadKeyringSlot32(0x50au, (uint32_t *)slot50a);
	return (int)(slot50a[11] & 0x10u);
}

/* 0x8052de (name ours) */
int srvkCheckFlagBit62(void)
{
	uint32_t slot510[8];

	srvkReadKeyringSlot32(0x510u, slot510);
	return (int)(slot510[4] >> 31);
}

/* 0x8052f8 (name ours) */
uint32_t srvkReadKeyring517Word0(void)
{
	uint32_t slot517[8];

	srvkReadKeyringSlot32(0x517u, slot517);
	return slot517[0];
}

/* 0x805310 (name ours) */
int srvkMatchVersionEntry(uint32_t entryType, const SceSrvkVersionEntry *entry,
		const uint64_t *authId, const uint64_t *progVersion)
{
	uint64_t progVer = *progVersion;
	uint64_t refVer;
	uint64_t opVal;
	uint64_t maxOp;
	uint16_t cmpOp;

	(void)entryType;
	if ((*authId & entry->authIdMask) != entry->targetAuthId)
		return 0;

	refVer = entry->versionRef;
	cmpOp = entry->cmpOp;
	maxOp = 5ULL;
	opVal = (uint64_t)cmpOp;
	if ((uint32_t)cmpU64(&opVal, &maxOp) >= 2u)
		return 1;

	switch (cmpOp) {
	case 0u:
		return (progVer == refVer);
	case 1u:
		return (progVer != refVer);
	case 2u:
		return (progVer < refVer);
	case 3u:
		return (progVer <= refVer);
	case 4u:
		return (progVer > refVer);
	case 5u:
		return (progVer >= refVer);
	default:
		return 1;
	}
}

/* 0x80548c (name ours) */
int srvkScanEntries(const uint8_t *entriesBase, uint32_t entryCount,
		uint32_t targetEntryType, const uint64_t *authId,
		const uint64_t *progVersion, const uint8_t *digest)
{
	uint64_t modAuthId = *authId;
	uint64_t modVer = *progVersion;
	const uint8_t *cur = entriesBase;
	uint32_t idx;

	for (idx = 0u; idx < entryCount; idx++) {
		uint16_t type;

		if (checkRangeInSkSram((uint32_t)(uintptr_t)cur, 2u) == 0)
			skPanic("%08x\n", 0x25u);

		type = *(const uint16_t *)cur;
		if (type == 2u) {
			const SceSrvkDigestEntry *dEntry = (const SceSrvkDigestEntry *)cur;

			if (checkRangeInSkSram((uint32_t)(uintptr_t)cur, sizeof(*dEntry)) == 0)
				skPanic("%08x\n", 9u);

			if (targetEntryType == 2u &&
			    (modAuthId & dEntry->authIdMask) == dEntry->targetAuthId) {
				uint8_t diff = 0u;
				uint32_t i;

				for (i = 0u; i < 32u; i++)
					diff |= digest[i] ^ dEntry->digest[i];
				if (diff == 0u)
					return (int)0x800f0b30;
			}
			cur += sizeof(*dEntry);
		} else if (type == 3u) {
			const SceSrvkVersionEntry *vEntry = (const SceSrvkVersionEntry *)cur;

			if (checkRangeInSkSram((uint32_t)(uintptr_t)cur, sizeof(*vEntry)) == 0)
				skPanic("%08x\n", 0x24u);

			if (targetEntryType == 3u) {
				uint64_t aId = modAuthId;
				uint64_t pVer = modVer;

				if (srvkMatchVersionEntry(3u, vEntry, &aId, &pVer) != 0)
					return 1;
			}
			cur += sizeof(*vEntry);
		} else if (type == 1u) {
			const SceSrvkVersionEntry *vEntry = (const SceSrvkVersionEntry *)cur;

			if (checkRangeInSkSram((uint32_t)(uintptr_t)cur, sizeof(*vEntry)) == 0)
				skPanic("%08x\n", 8u);

			if (targetEntryType == 1u) {
				uint64_t aId = modAuthId;
				uint64_t pVer = modVer;

				if (srvkMatchVersionEntry(1u, vEntry, &aId, &pVer) != 0)
					return (int)0x800f0b30;
			}
			cur += sizeof(*vEntry);
		} else {
			return (int)0x800f0b28;
		}
	}
	return 0;
}

/* 0x80564c (name ours) */
int srvkAuthenticateAndLoad(uint32_t rangesPaddr, uint32_t numRanges,
		const SceSkBufferDesc *dstRegion, SceSkBufferDesc *outLoadedDesc)
{
	uint64_t expectedHeaderLen;
	SrvkScatterSegmentCbArg segCbArg;
	SceSkBufferDesc srcDesc;
	SelfAuthContext selfCtx;
	const SceCfSectionHead *sec2 = NULL;
	const SceCfSectionHead *sec1 = NULL;
	const SceSkBufferDesc *hdrCbArg;
	struct {
		uint8_t modulus[256];
		uint8_t exponent[16];
	} rsaPubKey;
	uintptr_t dstBase;
	uint32_t dstSize;
	const SceSrvkHeader *srvkHdr;
	uintptr_t listBase;
	uint32_t listSize;
	uint32_t entryCount;
	uint32_t curOff;
	uint32_t i;
	int status;

	if (rangesPaddr == 0u || numRanges == 0u ||
	    dstRegion == NULL || outLoadedDesc == NULL) {
		return (int)0x800f0b16;
	}

	dstBase = dstRegion->addr;
	dstSize = dstRegion->size;
	if (dstBase == 0u || dstSize == 0u)
		return (int)0x800f0b16;

	selfCtx.scratchBuf = (void *)dstBase;
	selfCtx.scratchSize = dstSize;

	byteMemcpy(rsaPubKey.modulus, s_srvkRsaModulus, 0x100u);
	byteMemcpy(rsaPubKey.exponent, s_srvkRsaExponent, 4u);

	hdrCbArg = &srcDesc;
	BIGMAC->memMode = 3u;
	expectedHeaderLen = 0x400ULL;
	srcDesc.addr = rangesPaddr;
	srcDesc.size = numRanges;
	status = selfInitAndVerifyHeader(&selfCtx, s_srvkMetadataKeyIv, &rsaPubKey,
			5u, 0, 2u, selfScatterHeaderReadCb, &hdrCbArg,
			0, &expectedHeaderLen);
	BIGMAC->memMode = 0u;
	if (status != 0)
		return status;

	status = cfFindSectionEntryByTypeAndIndex(&selfCtx, 1u, 1u, (const void **)&sec1);
	if (status != 0)
		return status;
	status = cfFindSectionEntryByTypeAndIndex(&selfCtx, 2u, 2u, (const void **)&sec2);
	if (status != 0)
		return status;

	/* Verify sections do not overlap and fit within dstSize */
	if (sec2->offset >= sec1->offset) {
		if (sec1->offset + sec1->size > sec2->offset)
			return (int)0x800f0b24;
	} else {
		if (sec2->offset + sec2->size > sec1->offset)
			return (int)0x800f0b24;
	}

	if (sec1->offset + sec1->size > (uint64_t)dstSize ||
	    sec2->offset + sec2->size > (uint64_t)dstSize) {
		return (int)0x800f0b24;
	}

	segCbArg.srcDesc = &srcDesc;
	segCbArg.segOffset = sec1->offset;
	segCbArg.dstVaddr = (uint32_t)dstBase + (uint32_t)sec1->offset;
	segCbArg.filesz = (uint32_t)sec1->size;
	segCbArg.segSize = sec1->size;
	BIGMAC->memMode = 3u;
	status = selfDecryptAndLoadSegment(&selfCtx, 1u, 1u,
			selfScatterSegmentReadCb, &segCbArg, NULL, NULL);
	BIGMAC->memMode = 0u;
	if (status != 0)
		return status;

	srvkHdr = (const SceSrvkHeader *)(dstBase + (uint32_t)sec1->offset);
	if (srvkHdr->magic != 5u)
		return (int)0x800f0b24;
	if (checkSocVersionMatches(srvkHdr->socVersion) != 1)
		return (int)0x800f0337;
	if (srvkHdr->version != 1u)
		return (int)0x800f0b24;

	listSize = (uint32_t)sec2->size;
	listBase = dstBase + (uint32_t)sec2->offset;
	if ((sec2->size >> 32) != 0ULL || listSize > dstSize || listSize == 0u)
		return (int)0x800f0b24;

	segCbArg.srcDesc = &srcDesc;
	segCbArg.segOffset = sec2->offset;
	BIGMAC->memMode = 3u;
	segCbArg.dstVaddr = (uint32_t)listBase;
	segCbArg.filesz = listSize;
	segCbArg.segSize = sec2->size;
	status = selfDecryptAndLoadSegment(&selfCtx, 2u, 2u,
			selfScatterSegmentReadCb, &segCbArg, NULL, NULL);
	BIGMAC->memMode = 0u;
	if (status != 0)
		return status;

	entryCount = srvkHdr->entryCount;
	curOff = 0u;
	for (i = 0u; i < entryCount; i++) {
		uint16_t type;

		if (curOff >= listSize)
			return (int)0x800f0b24;
		type = *(const uint16_t *)(listBase + curOff);
		if (type == 2u)
			curOff += sizeof(SceSrvkDigestEntry);
		else if (type == 1u || type == 3u)
			curOff += sizeof(SceSrvkVersionEntry);
		else
			return (int)0x800f0b24;
	}

	if (curOff != listSize)
		return (int)0x800f0b24;

	outLoadedDesc->addr = (uint32_t)listBase;
	outLoadedDesc->size = entryCount;
	return 0;
}

/* 0x80594e (name ours) */
int srvkCheckModuleRevoked(const SceSkBufferDesc *modulePair,
		const SceSkBufferDesc *srvkDesc)
{
	const SceSkSmAuthContext *authCtx;
	const SceSkSelfAuthInfo *selfInfo;
	const uint8_t *srvkEntries;
	uint64_t authId;
	uint64_t progVer;
	uint64_t aId;
	uint64_t pVer;
	int status;

	if (modulePair == NULL || srvkDesc == NULL)
		return (int)0x800f0b16;

	authCtx = (const SceSkSmAuthContext *)(uintptr_t)modulePair->addr;
	selfInfo = (const SceSkSelfAuthInfo *)(uintptr_t)modulePair->size;
	srvkEntries = (const uint8_t *)(uintptr_t)srvkDesc->addr;
	if (authCtx == NULL || selfInfo == NULL || srvkEntries == NULL)
		return (int)0x800f0b16;

	authId = authCtx->target.authId;
	progVer = selfInfo->programVersion;

	if ((authId == 0x2800000000000030ULL || authId == 0x2800000000000032ULL) &&
	    (progVer >> 32) >= 0x100u) {
		return 0;
	}

	aId = authId;
	pVer = progVer;
	status = srvkScanEntries(srvkEntries, srvkDesc->size, 2u,
			&aId, &pVer, selfInfo->digest);
	if (status < 0)
		return status;

	aId = authId;
	pVer = progVer;
	status = srvkScanEntries(srvkEntries, srvkDesc->size, 3u,
			&aId, &pVer, selfInfo->digest);
	if (status < 0)
		return status;

	if (status == 0) {
		aId = authId;
		pVer = progVer;
		status = srvkScanEntries(srvkEntries, srvkDesc->size, 1u,
				&aId, &pVer, selfInfo->digest);
		if (status < 0)
			return status;

		aId = authId;
		pVer = progVer;
		if (srvkMatchVersionEntry(1u, &s_smFwVersionRule, &aId, &pVer) != 0)
			return (int)0x800f0b30;
	}
	return 0;
}
