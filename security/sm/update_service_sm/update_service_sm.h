/*
 * update_service_sm.h: Internal types, structures, and function declarations
 * for PlayStation Vita update_service_sm.self (SceSblUpdateServiceSm).
 *
 * Internal function, structure, and table names (except sm_main) are ours.
 */

#ifndef UPDATE_SERVICE_SM_H
#define UPDATE_SERVICE_SM_H

#include "sm_common.h"

/* ============================================================================
 * Mailbox Command IDs & Error Codes
 * ============================================================================ */

#define SCE_SBL_UPD_CMD_VERIFY_UPDATE_LIST	0x00010002u
#define SCE_SBL_UPD_CMD_CHECK_UPDATE_ENTRY	0x00020002u
#define SCE_SBL_UPD_CMD_VERIFY_ENC_IS		0x00030002u
#define SCE_SBL_UPD_CMD_VERIFY_SPKG		0x00040002u
#define SCE_SBL_UPD_CMD_DMA_COPY_SCATTER	0x00050002u
#define SCE_SBL_UPD_CMD_SNVS_WRITE_SECTORS	0x00060002u
#define SCE_SBL_UPD_CMD_SNVS_READ_SECTORS	0x00070002u
#define SCE_SBL_UPD_CMD_SNVS_WRITE_STATUS	0x00080002u
#define SCE_SBL_UPD_CMD_SNVS_READ_STATUS	0x00090002u
#define SCE_SBL_UPD_CMD_VERIFY_ENC_ID		0x000a0002u
#define SCE_SBL_UPD_CMD_SNVS_SECTOR_PROTOCOL	0x000b0002u
#define SCE_SBL_UPD_CMD_SNVS_HEADER_PROTOCOL	0x000c0002u
#define SCE_SBL_UPD_CMD_AUTH_KEYSLOT_15		0x000d0002u

#define SCE_SBL_ERROR_UPD_VERIFY_FAILED		((int)0x800f0027u)
#define SCE_SBL_ERROR_UPD_KEY_NOT_FOUND		((int)0x800f002fu)
#define SCE_SBL_ERROR_UPD_OUT_OF_RANGE		((int)0x800f0202u)
#define SCE_SBL_ERROR_UPD_DMA_IO		((int)0x800f0205u)
#define SCE_SBL_ERROR_UPD_NO_MEMORY		((int)0x800f020cu)
#define SCE_SBL_ERROR_UPD_INVALID_VALUE		((int)0x800f0216u)
#define SCE_SBL_ERROR_UPD_INVALID_ADDR		((int)0x800f0222u)
#define SCE_SBL_ERROR_UPD_BAD_HEADER		((int)0x800f0224u)
#define SCE_SBL_ERROR_UPD_PREPARE_FAILED	((int)0x800f0226u)
#define SCE_SBL_ERROR_UPD_BAD_DIGEST		((int)0x800f0227u)
#define SCE_SBL_ERROR_UPD_NOT_READY		((int)0x800f0229u)
#define SCE_SBL_ERROR_UPD_TARGET_MISMATCH	((int)0x800f0231u)
#define SCE_SBL_ERROR_UPD_VERSION_FLOOR		((int)0x800f0236u)
#define SCE_SBL_ERROR_UPD_HW_REV_FLOOR		((int)0x800f0237u)

/* ============================================================================
 * Bigmac LLI Descriptor (names ours)
 * ============================================================================ */

typedef struct BigmacLliDesc {
	uint32_t src;				/* 0x00 */
	uint32_t dst;				/* 0x04 */
	uint32_t size;				/* 0x08 */
	uint32_t func;				/* 0x0c */
	uint32_t keySlot;			/* 0x10 */
	uint32_t iv;				/* 0x14 */
	uint32_t rsvd18;			/* 0x18 */
	uint32_t next;				/* 0x1c */
} BigmacLliDesc;

_Static_assert(sizeof(BigmacLliDesc) == 0x20, "BigmacLliDesc size");

/* ============================================================================
 * SNVS & Auth Key Slot 15 Structures (names ours)
 * ============================================================================ */

typedef struct PmSnvsCipher {
	const void * const *vptr;		/* 0x00 */
	const uint8_t *dataKey;			/* 0x04 */
	const uint8_t *tweakKey;		/* 0x08 */
	const uint8_t *cmacKey;			/* 0x0c */
	uint32_t dataKeyBits;			/* 0x10 */
	uint32_t cmacKeyBits;			/* 0x14 */
} PmSnvsCipher;

_Static_assert(sizeof(PmSnvsCipher) == 0x18, "PmSnvsCipher size");

/* ============================================================================
 * Update List (SCESLB2) Verifier Structures (names ours)
 * ============================================================================ */

typedef struct UpdSlb2Header {
	uint8_t magic[8];			/* 0x00: "SCESLB2\x01" */
	uint8_t rsvd08[16];			/* 0x08..0x17 */
	uint32_t entryCountLow;			/* 0x18 */
	uint32_t entryCountHigh;		/* 0x1c */
	uint8_t rsvd20[16];			/* 0x20..0x2f */
	uint32_t keyType;			/* 0x30: 2 */
	uint32_t keyIdx;			/* 0x34: 1..10 */
	uint8_t rsvd38[72];			/* 0x38..0x7f */
} UpdSlb2Header;

_Static_assert(sizeof(UpdSlb2Header) == 0x80, "UpdSlb2Header size");

typedef struct UpdListVerifier {
	const void *vptr;			/* 0x000 */
	uint8_t hmacKey[64];			/* 0x004..0x043 */
	uint32_t keyIdx;			/* 0x044 */
	uint32_t keyType;			/* 0x048 */
	uint32_t flags;				/* 0x04c */
	UpdSlb2Header header;			/* 0x050..0x0cf */
	uint8_t *entryIds;			/* 0x0d0 */
	uint8_t *entryHashes;			/* 0x0d4 */
	uint8_t headerHmac[32];			/* 0x0d8..0x0f7 */
	CfHmacContext hmacCtx;			/* 0x0f8..0x1cb */
	uint32_t pad1cc;			/* 0x1cc */
} UpdListVerifier;

_Static_assert(sizeof(UpdListVerifier) == 0x1d0, "UpdListVerifier size");

/* ============================================================================
 * ENC IS / ENC ID Package Verifier Structures (names ours)
 * ============================================================================ */

typedef struct UpdEncRsaPubKey {
	uint8_t modulus[256];			/* 0x000..0x0ff */
	uint32_t modulusBytes;			/* 0x100: 0x100 */
	uint8_t exponent[4];			/* 0x104: 0x00, 0x01, 0x00, 0x01 */
	uint32_t exponentBytes;			/* 0x108: 4 */
} UpdEncRsaPubKey;

_Static_assert(sizeof(UpdEncRsaPubKey) == 0x10c, "UpdEncRsaPubKey size");

typedef struct UpdEncPackageVerifier {
	uint8_t aesKey[16];			/* 0x000..0x00f */
	uint8_t aesIv[16];			/* 0x010..0x01f */
	uint8_t privData[640];			/* 0x020..0x29f */
	UpdEncRsaPubKey rsaPubKey;		/* 0x2a0..0x3ab */
	uint32_t hasPrivData;			/* 0x3ac */
	uint32_t hasRsaPubKey;			/* 0x3b0 */
	uint32_t hasAesKeyIv;			/* 0x3b4 */
} UpdEncPackageVerifier;

_Static_assert(sizeof(UpdEncPackageVerifier) == 0x3b8, "UpdEncPackageVerifier size");

typedef struct SpkgScatterListDesc {
	const SceSmRangeEntry *entries;		/* 0x00 */
	uint32_t byteSize;			/* 0x04 */
} SpkgScatterListDesc;

typedef struct UpdEncScatterRequest {
	uint32_t modeLow;			/* 0x00: 1 = multi-list */
	uint32_t modeHigh;			/* 0x04 */
	uint8_t nonce[32];			/* 0x08..0x27 */
	uint32_t rsvd28[3];			/* 0x28..0x33 */
	uint32_t listCount;			/* 0x34: <= 494 */
	uint32_t rsvd38[4];			/* 0x38..0x47 */
	uint32_t inlineEntryCount;		/* 0x48 */
	uint32_t rsvd4c;			/* 0x4c */
	union {
		SpkgScatterListDesc lists[494];	/* 0x50 */
		SceSmRangeEntry inlineEntries[494];
	} u;
} UpdEncScatterRequest;

/* ============================================================================
 * SPKG Verifier Structures (names ours)
 * ============================================================================ */

typedef struct SpkgExtHeader {
	uint32_t magic;				/* 0x00: 4 */
	uint32_t pkgType;			/* 0x04: 1 or 9 */
	uint32_t flags08;			/* 0x08 */
	uint32_t rsvd0c;			/* 0x0c */
	uint32_t version;			/* 0x10 */
	uint32_t rsvd14[5];			/* 0x14..0x27 */
	uint32_t qaFlags;			/* 0x28 */
	uint32_t targetFlags;			/* 0x2c */
	uint32_t rsvd30[4];			/* 0x30..0x3f */
} SpkgExtHeader;

_Static_assert(sizeof(SpkgExtHeader) == 0x40, "SpkgExtHeader size");

typedef struct SpkgVerifyRequest {
	uint32_t rsvd00[2];			/* 0x000..0x007 */
	uint64_t mode;				/* 0x008..0x00f: 1 = multi-list */
	uint32_t rsvd10[3];			/* 0x010..0x01b */
	uint32_t listCount;			/* 0x01c: <= 481 */
	uint32_t rsvd20[4];			/* 0x020..0x02f */
	uint32_t inlineEntryCount;		/* 0x030 */
	uint32_t rsvd34;			/* 0x034 */
	union {
		SpkgScatterListDesc lists[481];	/* 0x038..0xf3f */
		SceSmRangeEntry inlineEntries[481];
	} u;
	SpkgExtHeader decryptedExtHdr1;		/* 0xf40..0xf7f */
	uint8_t decryptedSeg2[64];		/* 0xf80..0xfbf */
} SpkgVerifyRequest;

_Static_assert(sizeof(SpkgVerifyRequest) == 0xfc0, "SpkgVerifyRequest size");

typedef struct SpkgStreamCbContext {
	SpkgVerifyRequest *req;			/* 0x00 */
	uint32_t rsvd04;			/* 0x04 */
	uint64_t listIdx;			/* 0x08..0x0f */
	uint32_t segType;			/* 0x10 */
	uint32_t segIdx;			/* 0x14 */
} SpkgStreamCbContext;

_Static_assert(sizeof(SpkgStreamCbContext) == 0x18, "SpkgStreamCbContext size");

typedef struct SpkgHwRevRule {
	uint32_t minVer;			/* 0x00 */
	uint32_t minVerAlt;			/* 0x04 */
	uint32_t attrMask;			/* 0x08 */
} SpkgHwRevRule;

_Static_assert(sizeof(SpkgHwRevRule) == 0x0c, "SpkgHwRevRule size");

typedef struct SpkgHwRevMapEntry {
	uint16_t minProdCode;			/* 0x00 */
	uint16_t maxProdCode;			/* 0x02 */
	uint16_t subCode;			/* 0x04 */
	uint16_t pad06;				/* 0x06 */
	uint32_t ruleIdx;			/* 0x08 */
} SpkgHwRevMapEntry;

_Static_assert(sizeof(SpkgHwRevMapEntry) == 0x0c, "SpkgHwRevMapEntry size");

typedef struct SpkgSegmentSpec {
	uint32_t segType;			/* 0x00 */
	uint32_t segIdx;			/* 0x04 */
} SpkgSegmentSpec;

typedef struct SpkgVerifier {
	const void *vptr;			/* 0x00 */
	SpkgVerifyRequest *req;			/* 0x04 */
	uint32_t isPrepared;			/* 0x08 */
} SpkgVerifier;

_Static_assert(sizeof(SpkgVerifier) == 0x0c, "SpkgVerifier size");

/* ============================================================================
 * Function Declarations (names ours except sm_main)
 * ============================================================================ */

int sm_main(void);
void updHookInvoke(void *self, void *commBuf);
int updRegisterSimpleHook(SmServiceContext *srvCtx, uint32_t cmdId,
		void (**fnPtr)(void *), SmServiceHookNode **outNode);

void updListStaticInitOrDtor(int isInit);
void updCmdVerifyUpdateList(void *commBuf);
void updCmdCheckEntryInUpdateList(void *commBuf);
void updCmdVerifyEncIs(void *commBuf);
void updCmdVerifyEncId(void *commBuf);
void updListStaticInitThunk(void);
void updListStaticDtorThunk(void);

void spkgStaticInitOrDtor(int isInit);
void spkgCmdVerifyHeaderAndSegments(void *commBuf);
void spkgStaticInitThunk(void);
void spkgStaticDtorThunk(void);

int updDmaValidateArmRange(uint32_t addr, uint32_t size);
int updDmaCountScatterEntries(const SpkgVerifyRequest *req, uint32_t *outCount);
int updDmaBuildLliChain(const SpkgVerifyRequest *req, const void *ivPtr,
		BigmacLliDesc *lliBuf, uint32_t count);
int updDmaExecuteLliChain(BigmacLliDesc *lliBuf);
void updCmdDmaCopyScatterList(void *commBuf);

void updSnvsTransferSectors(void *commBuf, uint32_t isRead);
void updReadSnvsCounter2240(uint8_t *outCounter4);
void updCmdSnvsWriteSectors(void *commBuf);
void updCmdSnvsReadSectors(void *commBuf);
void updCmdSnvsWriteStatus(void *commBuf);
void updCmdSnvsReadStatus(void *commBuf);
void updCmdSnvsSectorProtocol(void *commBuf);
void updCmdSnvsHeaderProtocol(void *commBuf);
void updCmdAuthKeySlot15(void *commBuf);

int pmCheckKeyId(uint32_t keyId);
void pmReadKeyringSlot32(uint32_t slotIndex, uint32_t *outWords8);
void pmStaticInitOrDtor(int isInit);
int pmEnsureSnvsCryptoInitialized(void);
int pmReadSnvsSector0(void *outWord0, void *outWord1, const void *inSector32);
int pmValidateSectorRange(uint32_t sectorCount, uint32_t startSector);
int pmDecryptSnvsSectors(uint32_t startSector, const void *src,
		void *dst, uint32_t sectorCount);
uint32_t pmIsSnvsInitialized(void);
PmSnvsCipher *pmGetSnvsCipher(void);
const uint8_t *pmGetZeroIv16(void);
int pmBuildSnvsMacChallenge(uint32_t counter, uint32_t sectorIdx,
		uint8_t *outChallenge16);
int pmVerifyAndUnwrapSnvsSectorWrite(uint32_t *inOutCounter,
		uint32_t *outSectorIdx, uint8_t *outSectorData32,
		uint32_t sectorSize, uint8_t *inOutPacket48);
uint32_t pmIncrementSnvsCounter(uint32_t counter);
void pmStaticInitThunk(void);
void pmStaticDtorThunk(void);

void pmSnvsCipherDtorD2(PmSnvsCipher *c);
void pmSnvsCipherDtorD0(PmSnvsCipher *c);
void pmSnvsCipherCtorC2(PmSnvsCipher *c);
void pmSnvsCipherCtorC1(PmSnvsCipher *c);
void pmSnvsCipherDtorD1(PmSnvsCipher *c);
int pmSnvsCipherInit(PmSnvsCipher *c, const uint8_t *dataKey,
		const uint8_t *tweakKey, const uint8_t *cmacKey,
		uint32_t dataKeyBits, uint32_t cmacKeyBits);
int pmSnvsSignRecordCmac(const PmSnvsCipher *c, uint8_t *inOutRec32);
int pmFormatSectorTweak(const PmSnvsCipher *c, uint32_t sectorIdx,
		uint64_t *outTweak16);
int pmSnvsDecryptSectors(const PmSnvsCipher *c, const void *src,
		void *dst, uint32_t totalBytes, uint32_t startSector);
void pmSnvsCipherNoop1(void);
int pmXtsDecryptSector(const uint8_t *tweakKey, const uint8_t *dataKey,
		const uint8_t *tweakIv, uint32_t keyBits, uint32_t sectorBytes,
		const uint8_t *src, uint8_t *dst);
int pmIncrementTweak128(uint8_t *tweak, uint32_t len);
int pmXtsDecryptMultiSectors(const uint8_t *tweakIv, const uint8_t *dataKey,
		const uint8_t *tweakKey, uint32_t keyBits, uint32_t totalBytes,
		uint32_t wordsPerSector, const uint8_t *src, uint8_t *dst);
void pmReadHardwareKey2220(uint32_t *outWords8);
int pmEncryptSnvsSectors(uint32_t startSector, const void *src,
		void *dst, uint32_t sectorCount);
int pmWriteSnvsSector0(const void *inWord0, const void *inWord1,
		void *outSector32);
int pmWrapSnvsSectorReadResponse(uint32_t counter, uint32_t sectorIdx,
		const void *sectorData32, uint32_t sectorSize,
		uint8_t *outPacket48);
int pmVerifySnvsCommitPacket(uint32_t *inOutCounter, uint32_t *outSectorIdx,
		uint8_t *inOutPacket16);
int pmSnvsEncryptSectors(const PmSnvsCipher *c, const void *src,
		void *dst, uint32_t totalBytes, uint32_t startSector);
int pmXtsEncryptSector(const uint8_t *tweakKey, const uint8_t *dataKey,
		const uint8_t *tweakIv, uint32_t keyBits, uint32_t sectorBytes,
		const uint8_t *src, uint8_t *dst);
int pmXtsEncryptMultiSectors(const uint8_t *tweakIv, const uint8_t *dataKey,
		const uint8_t *tweakKey, uint32_t keyBits, uint32_t totalBytes,
		uint32_t wordsPerSector, const uint8_t *src, uint8_t *dst);

int pmConstTimeCompare(const uint8_t *a, const uint8_t *b, uint32_t len);
int pmResolveKeySlot(uint32_t keyId, uint32_t variant, uint32_t *outSlot);
int pmBuildAuth1InitHeader(uint8_t *out40, uint32_t keyId);
int pmBuildAuth1Challenge(uint8_t *out40, const uint8_t *in40,
		uint32_t keyId, uint32_t variant, const uint8_t *rng8);
int pmVerifyAuth1Response(const uint8_t *in40, uint32_t keyId,
		uint32_t variant, const uint8_t *challengeA8,
		const uint8_t *challengeB8, uint8_t *outSessionKey16);
int pmClearKeySlot(uint32_t keyId, uint32_t variant);
int pmAes128CbcEncrypt(uint8_t *dst, const uint8_t *src, uint32_t len,
		const void *key, const void *iv);
int pmAes128CbcDecrypt(uint8_t *dst, const uint8_t *src, uint32_t len,
		const void *key, const void *iv);
int pmGetRandomBytes(uint8_t *dst, uint32_t len);
int pmBuildSubcmd4Challenge(uint8_t *out40, uint32_t variant,
		const uint8_t *sessionKey16);
int pmVerifySubcmd4Response(const uint8_t *in40, uint32_t variant,
		const uint8_t *sessionKey16);

int updListValidateArmRange(uint32_t addr, uint32_t size);
void updListVerifierDtorD2(UpdListVerifier *v);
void updListVerifierDtorD0(UpdListVerifier *v);
void updListVerifierCtorC2(UpdListVerifier *v);
void updListVerifierCtorC1(UpdListVerifier *v);
void updListVerifierDtorD1(UpdListVerifier *v);
int updListSelectHmacKeyWithFlags(UpdListVerifier *v, uint32_t keyType,
		uint32_t keyIdx, uint32_t flags);
int updListSetFlags(UpdListVerifier *v, uint32_t flags);
int updListSelectHmacKey(UpdListVerifier *v, uint32_t keyType,
		uint32_t keyIdx);
int updListCheckMagic(UpdListVerifier *v, const UpdSlb2Header *hdr);
int updListCheckHeaderMagicAndBit(UpdListVerifier *v, const UpdSlb2Header *hdr);
int updListGetHeaderBit(UpdListVerifier *v, const UpdSlb2Header *hdr);
int updListIsKeyType2(UpdListVerifier *v, uint32_t keyType);
int updListIsKeyIdxValid(UpdListVerifier *v, uint32_t keyIdx);
const uint8_t *updListFindEntryHash(const UpdListVerifier *v,
		const uint64_t *entryId);
int updListReadScatterBytes(UpdListVerifier *v, void *dst,
		SpkgVerifyRequest *req, uint32_t offset, uint32_t size);
int updListLoadHeaderAndTables(UpdListVerifier *v, SpkgVerifyRequest *req);
int updListVerifyScatterHmac(CfHmacContext *hmacCtx,
		const SpkgVerifyRequest *req, const uint8_t *hmacKey,
		uint32_t keyLen, const uint8_t *expectedHmac,
		uint32_t skipTailBytes);
int updListVerifyEntry(UpdListVerifier *v, const SpkgVerifyRequest *req);
int updListVerifyPackage(UpdListVerifier *v, SpkgVerifyRequest *req);

int updEncIsValidateArmRange(uint32_t addr, uint32_t size);
void updEncIsCtorC2(UpdEncPackageVerifier *v);
void updEncIsCtorC1(UpdEncPackageVerifier *v);
void updEncIsDtorD2(UpdEncPackageVerifier *v);
void updEncIsDtorD1(UpdEncPackageVerifier *v);
int updEncIsNotSupported(void);
int updVerifyEncIsPackage(UpdEncPackageVerifier *v, const void *nonceBuf,
		uint32_t nonceLen, const uint8_t *pkgBuf, void *outDecrypted);
int updEncIsCopyFromScatter(UpdEncPackageVerifier *v, void *dst,
		const UpdEncScatterRequest *req, uint32_t offset, uint32_t size);

int updEncIdValidateArmRange(uint32_t addr, uint32_t size);
int updEncIdWaitChannelDone(const uint32_t *ctrl);
void updEncIdCtorC2(UpdEncPackageVerifier *v);
void updEncIdCtorC1(UpdEncPackageVerifier *v);
void updEncIdDtorD2(UpdEncPackageVerifier *v);
void updEncIdDtorD1(UpdEncPackageVerifier *v);
int updEncIdNotSupported(void);
int updVerifyEncIdPackage(UpdEncPackageVerifier *v, const void *nonceBuf,
		uint32_t nonceLen, const uint8_t *pkgBuf, void *outDecrypted);
int updEncIdCopyFromScatter(UpdEncPackageVerifier *v, void *dst,
		const UpdEncScatterRequest *req, uint32_t offset, uint32_t size);

void updEncBaseCtorC2(UpdEncPackageVerifier *v);
void updEncBaseCtorC1(UpdEncPackageVerifier *v);
void updEncBaseDtorD2(UpdEncPackageVerifier *v);
void updEncBaseDtorD1(UpdEncPackageVerifier *v);
int updEncSetAesKeyIv(UpdEncPackageVerifier *v, const uint8_t *key16,
		const uint8_t *iv16);
int updEncSetPrivData(UpdEncPackageVerifier *v, const uint8_t *priv640);
int updEncSetRsaPubKey(UpdEncPackageVerifier *v, const UpdEncRsaPubKey *pubKey);
int updEncBaseNotSupported(void);
int updEncVerifyRsa2048Sig(UpdEncPackageVerifier *v, const uint8_t *hash32,
		uint32_t hashLen, const uint8_t *sig256, uint32_t sigLen);

void spkgReadKeyringSlot32(uint32_t slotIndex, uint32_t *outWords8);
int spkgValidateArmRange(uint32_t addr, uint32_t size);
void spkgVerifierDtorD2(SpkgVerifier *v);
void spkgVerifierDtorD0(SpkgVerifier *v);
void spkgVerifierCtorC2(SpkgVerifier *v);
void spkgVerifierCtorC1(SpkgVerifier *v);
void spkgVerifierDtorD1(SpkgVerifier *v);
void spkgVerifierCleanup(SpkgVerifier *v);
int spkgCheckCfHeader(SpkgVerifier *v, const void *cfHdr);
int spkgCheckExtHeaderMagic(SpkgVerifier *v, const SpkgExtHeader *extHdr);
int spkgCheckMinVersionFloor(SpkgVerifier *v, const SpkgExtHeader *extHdr);
int spkgCheckExtHeaderField30(SpkgVerifier *v, const SpkgExtHeader *extHdr);
int spkgFindRangeEntry(const SceSmRangeEntry *entries, uint32_t count,
		const uint64_t *targetOffset, uint32_t *outIdx,
		uint32_t *outAccumLow, uint64_t *outInEntryOffset);
int spkgReadSegmentCallback(const uint64_t *offset, uint32_t size,
		uint64_t *outSrcAddr, uint64_t *outDstAddr,
		uint32_t *outChunk, uint32_t *outFlag, void *arg);
int spkgCopyFromScatterEntries(const SceSmRangeEntry *entries, uint32_t count,
		uintptr_t dstAddr, const uint64_t *offset, uint32_t size,
		uint32_t *outRead);
int spkgReadHeaderCallback(uintptr_t dstAddr, const uint64_t *offset,
		uint32_t size, uint32_t *outRead, void *arg);
int spkgReadPlainHeaderBytes(void *dst, SpkgVerifyRequest *req,
		uint32_t offset, uint32_t size);
int spkgValidateUnencryptedHeaders(SpkgVerifier *v, SpkgVerifyRequest *req);
int spkgVerifierPrepare(SpkgVerifier *v, SpkgVerifyRequest *req);
int spkgCheckTargetCompatibility(SpkgVerifier *v, const SpkgExtHeader *extHdr);
int spkgLookupHardwareRevEntry(SpkgVerifier *v, uint32_t *outIdx);
uint32_t spkgGetHardwareAttrMask(SpkgVerifier *v, uint32_t idx);
int spkgIsVersionBelowHwMin(SpkgVerifier *v, uint32_t pkgType,
		uint32_t pkgVer, uint32_t defaultMinVer);
int spkgCheckPkgVersionAndChassis(SpkgVerifier *v, const SpkgExtHeader *extHdr);
int spkgCheckFactoryUpdatePolicy(SpkgVerifier *v, const SpkgExtHeader *extHdr);
int spkgVerifierExecute(SpkgVerifier *v);
uint32_t spkgIsQaFlagSet(void);

/* Bigmac LLI & Bignum helpers in rsa_srvk.c */
int bigmacInitLliChain(BigmacLliDesc *list, uint32_t count, uint32_t funcFlags,
		uint32_t keySlot, const void *iv);
int bigmacStartAes128CbcEncryptLli(BigmacLliDesc *list, const uint32_t *ctrl);
int bigmacStartAes128CbcDecryptLli(BigmacLliDesc *list, const uint32_t *ctrl);
int bigmacStartSha256(void *dstDigest, const void *src, uint32_t size,
		const uint32_t *bmCtrl);
int bignumInitState(uint32_t *state);
int bignumWaitInterrupt(void);
int verifyRsaPkcs1v15Step(uint8_t *workBuf, const void *sig,
		const BignumPublicKey *key, const uint8_t *hash,
		uint32_t hashAlgo, uint32_t *state);

#endif /* UPDATE_SERVICE_SM_H */
