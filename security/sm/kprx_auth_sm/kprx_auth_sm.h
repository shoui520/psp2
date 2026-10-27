/*
 * kprx_auth_sm.h: Internal types, hardware MMIO definitions, and cross-unit
 * function declarations for PlayStation Vita kprx_auth_sm.self (4.00-DEBUGGER).
 */

#ifndef KPRX_AUTH_SM_H
#define KPRX_AUTH_SM_H

#include <stdint.h>
#include <stddef.h>

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
#ifndef _Static_assert
#define _STATIC_ASSERT_PASTE(a, b) a##b
#define _STATIC_ASSERT_PASTE2(a, b) _STATIC_ASSERT_PASTE(a, b)
#define _Static_assert(cond, msg) typedef char _STATIC_ASSERT_PASTE2(static_assertion_, __LINE__)[(cond) ? 1 : -1]
#endif
#endif

#include <scetypes.h>
#include <kernel/error.h>
#include "sm.h"

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define SCE_SBL_ERROR_INVALID_VALUE		((int)0x800f0016u)
#define SCE_SBL_ERROR_NO_MEMORY			((int)0x800f000cu)
#define SCE_SBL_ERROR_IO			((int)0x800f0005u)
#define SCE_SBL_ERROR_BUSY			((int)0x800f0010u)
#define SCE_SBL_ERROR_NOT_SUPPORTED		((int)0x800f0021u)
#define SCE_SBL_ERROR_INVALID_ADDR		((int)0x800f0022u)
#define SCE_SBL_ERROR_BAD_DATA			((int)0x800f0024u)
#define SCE_SBL_ERROR_OVERFLOW			((int)0x800f0025u)
#define SCE_SBL_ERROR_CRYPTO			((int)0x800f0028u)
#define SCE_SBL_ERROR_NOT_INITIALIZED		((int)0x800f0029u)
#define SCE_SBL_ERROR_NOT_ALLOWED		((int)0x800f0030u)
#define SCE_SBL_ERROR_BIGNUM_INVALID		((int)0x800f0116u)
#define SCE_SBL_ERROR_CF_NOT_FOUND		((int)0x800f0602u)
#define SCE_SBL_ERROR_CF_IO			((int)0x800f0605u)
#define SCE_SBL_ERROR_CF_INVALID_VALUE		((int)0x800f0616u)
#define SCE_SBL_ERROR_CF_BAD_HEADER		((int)0x800f0624u)
#define SCE_SBL_ERROR_CF_UNSUPPORTED_ALGO	((int)0x800f0625u)
#define SCE_SBL_ERROR_CF_BAD_DIGEST		((int)0x800f0627u)

/* ============================================================================
 * Hardware MMIO Accessors
 * ============================================================================ */

#define REG8(addr)	(*(volatile uint8_t *)(uintptr_t)(addr))
#define REG16(addr)	(*(volatile uint16_t *)(uintptr_t)(addr))
#define REG32(addr)	(*(volatile uint32_t *)(uintptr_t)(addr))

/* ============================================================================
 * Shared Structures (names ours unless noted)
 * ============================================================================ */

typedef struct SelfAuthContext {
	uint8_t *hdrBuf;			/* 0x00 */
	uint32_t hdrBufSize;			/* 0x04 */
} SelfAuthContext;

typedef struct SceSelfKblParam {
	uint32_t progAttr;			/* 0x00 */
	uint32_t progType;			/* 0x04 */
	uint32_t reserved08[2];			/* 0x08 */
	uint8_t attr[32];			/* 0x10 */
	uint8_t capability[32];			/* 0x30 */
	uint8_t sharedSecret0[16];		/* 0x50 */
	uint8_t klicensee[16];			/* 0x60 */
	uint32_t reserved70[8];			/* 0x70..0x8f */
} SceSelfKblParam;

_Static_assert(sizeof(SceSelfKblParam) == 0x90, "SceSelfKblParam size");

typedef struct SceSmSelfAuthInfo {
	uint32_t size;				/* 0x00: 0x80 */
	uint32_t version;			/* 0x04: 1 */
	uint64_t programAuthorityId;		/* 0x08 */
	uint8_t digest[32];			/* 0x10 */
	uint64_t minSysver;			/* 0x30 */
	uint32_t reserved38[2];			/* 0x38 */
	uint8_t secret0[16];			/* 0x40 */
	uint8_t secret1[16];			/* 0x50 */
	uint8_t secret2[16];			/* 0x60 */
	uint8_t secret3[16];			/* 0x70 */
} SceSmSelfAuthInfo;

_Static_assert(sizeof(SceSmSelfAuthInfo) == 0x80, "SceSmSelfAuthInfo size");

typedef struct CfHmacContext {
	uint32_t state;				/* 0x00 */
	uint8_t iv[40];				/* 0x04 */
	uint8_t digest[32];			/* 0x2c */
	uint8_t blockBuf[64];			/* 0x4c */
	uint32_t bufUsed;			/* 0x8c */
	uint8_t hmacKey[64];			/* 0x90 */
	uint32_t keyLen;			/* 0xd0 */
} CfHmacContext;

_Static_assert(sizeof(CfHmacContext) == 0xd4, "CfHmacContext size");

typedef struct SceCfMetadataHeader {
	uint64_t signatureInputLength;		/* 0x00 */
	uint32_t signatureType;			/* 0x08: 5 = RSA-2048 */
	uint32_t sectionCount;			/* 0x0c */
	uint32_t keyCount;			/* 0x10: count of 16-byte keyPool slots */
	uint32_t optHeaderSize;			/* 0x14 */
	uint64_t reserved;			/* 0x18 */
} SceCfMetadataHeader;

_Static_assert(sizeof(SceCfMetadataHeader) == 0x20, "SceCfMetadataHeader size");

typedef struct SceCfMetadataSection {
	uint64_t offset;			/* 0x00 */
	uint64_t size;				/* 0x08 */
	uint32_t type;				/* 0x10: 1 = SPSFO header, 2 = ELF segment */
	uint32_t index;				/* 0x14: program header index */
	uint32_t hashAlgorithm;			/* 0x18: 6 = HMAC-SHA256 */
	uint32_t hashKeyIndex;			/* 0x1c: index in 16-byte keyPool */
	uint32_t encryptionAlgorithm;		/* 0x20: 1 = none, 3 = AES-128-CTR */
	uint32_t keyIndex;			/* 0x24: index in 16-byte keyPool */
	uint32_t ivIndex;			/* 0x28: index in 16-byte keyPool */
	uint32_t compressionAlgorithm;		/* 0x2c: 1 = plain, 2 = compressed */
} SceCfMetadataSection;

_Static_assert(sizeof(SceCfMetadataSection) == 0x30, "SceCfMetadataSection size");

typedef int (*SelfHeaderReadCallback)(uintptr_t dstAddr, const uint64_t *offset,
		uint32_t size, uint32_t *outRead, void *arg);
typedef int (*SelfSegmentReadCallback)(const uint64_t *offset, uint32_t size,
		uint64_t *outSrcAddr, uint64_t *outDstAddr, uint32_t *outChunkSize,
		uint32_t *outFlags, void *arg);
typedef int (*SelfSegmentDoneCallback)(const uint64_t *offset, uint32_t reqSize,
		const uint64_t *srcAddr, const uint64_t *dstAddr, uint32_t chunkSize,
		uint32_t flags, void *arg);
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

_Static_assert(sizeof(CfSegmentDecryptState) == 0x160, "CfSegmentDecryptState size");

typedef struct RsaPublicKey2048 {
	const void *modulus;			/* 0x00 */
	uint32_t modulusSize;			/* 0x04: 0x100 bytes */
	const void *exponent;			/* 0x08 */
	uint32_t exponentSize;			/* 0x0c: 4 bytes */
} RsaPublicKey2048;

typedef struct BignumPublicKey {
	const uint32_t *modulus;		/* 0x00 */
	uint32_t modulusWords;			/* 0x04: words */
	const uint32_t *exponent;		/* 0x08 */
	uint32_t exponentWords;			/* 0x0c: words */
} BignumPublicKey;

typedef struct SceSmRangeEntry {
	uint32_t paddr;				/* 0x00 */
	uint32_t size;				/* 0x04 */
} SceSmRangeEntry;

typedef struct SceSmScatterList {
	int32_t count;				/* 0x00 */
	const SceSmRangeEntry *entries;		/* 0x04 */
} SceSmScatterList;

typedef struct SmServiceHookNode {
	uint32_t cmdId;				/* 0x00 */
	struct SmServiceHookNode *next;		/* 0x04 */
	const void *vtable;			/* 0x08 */
	void *obj;				/* 0x0c */
	void *fn;				/* 0x10 */
	int32_t adj;				/* 0x14 */
} SmServiceHookNode;

_Static_assert(sizeof(SmServiceHookNode) == 0x18, "SmServiceHookNode size");

typedef struct SmServiceContext {
	uint32_t running;			/* 0x00 */
	SmServiceHookNode *head;		/* 0x04 */
	uint32_t allocMask;			/* 0x08 */
	SmServiceHookNode *poolBase;		/* 0x0c */
	uint32_t poolSize;			/* 0x10: 0x180 */
	SmServiceHookNode nodes[16];		/* 0x14..0x194 */
} SmServiceContext;

_Static_assert(sizeof(SmServiceContext) == 0x194, "SmServiceContext size");

typedef struct KprxCfKeyEntry {
	uint8_t aesKeys[48];			/* 0x000..0x02f: 32B AES-256 + 16B IV */
	uint32_t rsaModulus[64];		/* 0x030..0x12f: 256-byte RSA-2048 modulus */
	uint32_t pad130;			/* 0x130: 0 */
	uint8_t rsaExponent[4];			/* 0x134..0x137: 0x00, 0x01, 0x00, 0x01 */
	uint32_t pad138;			/* 0x138: 0 */
} KprxCfKeyEntry;

_Static_assert(sizeof(KprxCfKeyEntry) == 0x13c, "KprxCfKeyEntry size");

typedef struct KprxAppType08KeyEntry {
	uint8_t prefix[32];			/* 0x000..0x01f: 32-byte key prefix */
	uint8_t aesKeys[48];			/* 0x020..0x04f: 32B AES-256 + 16B IV */
	uint32_t rsaModulus[64];		/* 0x050..0x14f: 256-byte RSA-2048 modulus */
	uint32_t pad150;			/* 0x150: 0 */
	uint8_t rsaExponent[4];			/* 0x154..0x157: 0x00, 0x01, 0x00, 0x01 */
	uint32_t pad158;			/* 0x158: 0 */
} KprxAppType08KeyEntry;

_Static_assert(sizeof(KprxAppType08KeyEntry) == 0x15c, "KprxAppType08KeyEntry size");

/* ============================================================================
 * Cross-Unit Function Declarations
 * ============================================================================ */

/* self_loader.c (0x80b09c..0x80cbf0) */
SceCfMetadataHeader *cfGetMetadataHeader(const SelfAuthContext *ctx);
SceCfMetadataSection *cfGetMetadataSections(const SelfAuthContext *ctx);
uint8_t *cfGetMetadataKeyPool(const SelfAuthContext *ctx);
int cfDefaultGetKeysCallback(const uint8_t **outMetaKey, const uint8_t **outRootPubKey,
		uint32_t *outSigType, const uint8_t **outRootKey, void *arg);
int cfGetSignatureSize(uint32_t sigType, uint32_t *outSize);
int cfValidateMetadataHeaderBounds(const uint8_t *hdr, const SceCfMetadataHeader *metaHdr,
		uint32_t expectedSigType, const int64_t *expectedHeaderLen);
int cfProcessUnalignedSegmentBlock(CfSegmentDecryptState *state,
		const uint64_t *blockSizePtr, uint8_t *scratchBuf, int updateHmac);
int cfSegmentDecryptUpdate(CfSegmentDecryptState *state,
		const uint64_t *startOffset, const uint64_t *totalSize,
		SelfSegmentReadCallback readCb, void *readCbArg,
		SelfSegmentDoneCallback doneCb, void *doneCbArg);
int cfSegmentDecryptFinal(CfSegmentDecryptState *state);
int cfSegmentDecryptInit(CfSegmentDecryptState *state,
		const SelfAuthContext *ctx, uint32_t sectionIndex,
		const SceCfMetadataSection *explicitSection);
int cfVerifyRsaSignature(const void *hdrBuf, uint32_t sigInputLen,
		const void *signature, const uint8_t *rootPubKey);
int cfAuthenticateAndDecryptHeaderWithKeyCb(const SelfAuthContext *ctx,
		CfGetKeysCallback getKeysCb, void *getKeysArg,
		int allowUnsignedOrDev, int16_t expectedAttribute,
		SelfHeaderReadCallback readCb, void *readCbArg,
		int32_t expectedHeaderSize, const int64_t *expectedFileOffset);
int selfInitAndVerifyHeader(const SelfAuthContext *ctx, const void *metaKey,
		const void *rootPubKey, uint32_t sigType, int allowUnsignedOrDev,
		uint16_t expectedAttribute, SelfHeaderReadCallback readCb,
		void *readCbArg, int32_t expectedHeaderSize,
		const int64_t *expectedFileOffset);
int cfGetSectionCount(const SelfAuthContext *ctx, uint32_t *outCount);
int cfGetSectionEntry(const SelfAuthContext *ctx, uint32_t index,
		const SceCfMetadataSection **outSection);
int cfFindSectionEntryByTypeAndIndex(const SelfAuthContext *ctx,
		uint32_t type, uint32_t index,
		const SceCfMetadataSection **outSection);
int cfGetExtHeaderAndMetaOffset(const SelfAuthContext *ctx,
		const uint8_t **outExtHdr, uint32_t *outMetaOffset);
int cfGetOptHeaders(const SelfAuthContext *ctx,
		const uint8_t **outOptHdr, uint32_t *outOptSize);
int cfDecryptAndVerifySection(const SelfAuthContext *ctx, uint32_t sectionIndex,
		SelfSegmentReadCallback readCb, void *readCbArg,
		SelfSegmentDoneCallback doneCb, void *doneCbArg);
int selfDecryptAndLoadSegment(const SelfAuthContext *ctx, uint32_t segType,
		uint32_t segIndex, SelfSegmentReadCallback readCb, void *readCbArg,
		SelfSegmentDoneCallback doneCb, void *doneCbArg);
int cfStreamSegmentInitThunk(CfSegmentDecryptState *state,
		const SelfAuthContext *ctx, uint32_t sectionIndex,
		const SceCfMetadataSection *explicitSection);
int cfStreamSegmentInitByTypeAndIndex(CfSegmentDecryptState *state,
		const SelfAuthContext *ctx, uint32_t type, uint32_t index);
int cfStreamSegmentFinal(CfSegmentDecryptState *state);
int cfStreamSegmentUpdateRange(CfSegmentDecryptState *state,
		const uint64_t *offset, const uint64_t *size,
		SelfSegmentReadCallback readCb, void *readCbArg,
		SelfSegmentDoneCallback doneCb, void *doneCbArg);
int selfVerifySvnAndDecryptMetadata(const SelfAuthContext *ctx);
int selfGetAppInfo(const SelfAuthContext *ctx, const uint8_t **outAppInfo);
int selfGetElfHeader(const SelfAuthContext *ctx, const void **outEhdr);
int selfGetProgramHeaders(const SelfAuthContext *ctx, const void **outPhdrs);
int selfGetSegmentInfos(const SelfAuthContext *ctx, const void **outSegInfos);
int selfGetControlInfo(const SelfAuthContext *ctx, const uint8_t **outCtrlInfo,
		uint64_t *outCtrlSize);
int selfFindControlInfo(const SelfAuthContext *ctx, const uint8_t **outCtrlInfo,
		uint32_t targetType);
int selfFindExtHeader(const SelfAuthContext *ctx, const uint8_t **outExtHdr,
		uint32_t targetType);
int cfAesCipher(uint32_t chainSelect, uint32_t dirSelect, uint32_t keyBitsSelect,
		void *dst, const void *src, uint32_t size, const void *key,
		void *iv, void *outIv, void *unusedDesc);
int cfCopyBytes(void *dst, const void *src, uint32_t size, void *unusedDesc);
int cfHmacSha256InitShortKey(CfHmacContext *ctx, const void *key, uint32_t keyLen);
int cfVerifyRsaPkcs1v15(void *workBuf256, const void *signature256,
		const RsaPublicKey2048 *pubKey, const void *hash32,
		uint32_t digestAlgo, void *unusedDesc);

/* sm_runtime.c (0x80e7ee..0x80eb9e) */
int smValidateMailboxRange(uint32_t addr, uint32_t size);
void smMailboxIrqHandler(void);
void smSrvInitZero(SmServiceContext *ctx);
void smSrvInit(SmServiceContext *ctx);
SmServiceContext *smSrvGetContext(void);
SmServiceHookNode *smSrvAllocNode(SmServiceContext *ctx);
int smSrvFreeNode(SmServiceContext *ctx, SmServiceHookNode *node);
void smSrvTeardown(SmServiceContext *ctx);
void smSrvTeardownAll(SmServiceContext *ctx);
int smSrvRunLoop(SmServiceContext *ctx);
int smSrvUnlinkNode(SmServiceContext *ctx, SmServiceHookNode *target);
void smMemBarrier(void);
int smSyscall1(uint32_t a1, uint32_t a2, uint32_t a3);
int smSyscall2(uint32_t a1, uint32_t a2, uint32_t a3);
int smSyscall3(uint32_t a1, uint32_t a2, uint32_t a3);
int smSyscall4(uint32_t a1, uint32_t a2, uint32_t a3);
int smSyscall5(uint32_t a1, uint32_t a2, uint32_t a3);
int smSyscall6(uint32_t a1, uint32_t a2, uint32_t a3);
int smSyscall7(uint32_t a1, uint32_t a2, uint32_t a3);
int smSyscall8(uint32_t a1, uint32_t a2, uint32_t a3);
void smWatchdogStart(void);
void smWatchdogStop(void);
uint32_t smWatchdogGetControl(void);
uint32_t smWatchdogGetCount(void);

/* self_loader_wrapper.c (0x80eb9e..0x80ef02) */
int selfExtractKblParam(const SelfAuthContext *ctx,
		SceSelfKblParam *outParam, int useQaCap);
int selfExtractSelfAuthInfo(const SelfAuthContext *ctx, SceSmSelfAuthInfo *outInfo);
int selfExtractAttrFlags(const SelfAuthContext *ctx, uint32_t *inOutFlags);
int selfLoaderWrapperGetAllSelfAuthInfo(const SelfAuthContext *ctx,
		SceSelfKblParam *outKblParam, int useQaCap,
		SceSmSelfAuthInfo *outAuthInfo, uint32_t *inOutFlags);

/* rsa_srvk.c (0x80ef02..0x80fcf4) */
int bigmacInitChannel0(uint32_t *ctrl);
int bigmacGetChannelStatus(const uint32_t *ctrl, uint32_t *outStatus);
int bigmacWaitChannelDone(const uint32_t *ctrl);
int bigmacStartAesCipher(void *dst, const void *src, uint32_t size,
		const void *key, const void *iv, uint32_t dir, uint32_t chainMode,
		uint32_t keyBits, const uint32_t *ctrl);
int bigmacStartAes128EcbEncrypt(void *dst, const void *src, uint32_t size,
		const void *key, const uint32_t *ctrl);
int bigmacStartAes128EcbDecrypt(void *dst, const void *src, uint32_t size,
		const void *key, const uint32_t *ctrl);
int bigmacStartAes128CbcEncrypt(void *dst, const void *src, uint32_t size,
		const void *key, const void *iv, const uint32_t *ctrl);
int bigmacStartAes128CbcDecrypt(void *dst, const void *src, uint32_t size,
		const void *key, const void *iv, const uint32_t *ctrl);
void bigmacReverseBswap128(uint32_t *words4);
int bigmacAes128EcbEncryptSync(void *dst, const void *src, uint32_t size,
		const void *key);
int bigmacAes128EcbDecryptSync(void *dst, const void *src, uint32_t size,
		const void *key);
int bigmacAes128CbcEncryptSync(void *dst, const void *src, uint32_t size,
		const void *key, void *iv);
int bigmacAes128CbcDecryptSync(void *dst, const void *src, uint32_t size,
		const void *key, void *iv);
int bigmacSha256(void *dstDigest, const void *src, uint32_t size);
int bigmacHmacSha256OneShotStart(void *dstDigest, const void *src, uint32_t size,
		const void *key64, uint32_t keyLen, const uint32_t *bmCtrl);
int bigmacHmacSha256Init(CfHmacContext *ctx, const void *key64, uint32_t keyLen);
int bigmacHmacSha256Update(CfHmacContext *ctx, const void *data, uint32_t size,
		uint32_t isFinalFlag);
int bigmacHmacSha256Final(void *outDigest32, CfHmacContext *ctx);
int bignumVerifyRsaPkcs1v15(void *workBuf256, const void *signature256,
		const BignumPublicKey *pubKey, const void *expectedDigest,
		uint32_t oidIndex);
int bigmacMemcpy(void *dst, const void *src, uint32_t size);

/* aimgr.c (0x80fcf4..0x8102aa) */
void aimgrReadKeyringSlot32(uint32_t slotIndex, uint32_t *outWords8);
void aimgrClearConsoleId(void *buf16);
void aimgrClearConsoleId2(void *buf16);
void aimgrReadConsoleId(void *out16);
void aimgrReadConsoleIdAlias1(void *out16);
void aimgrReadConsoleIdAlias2(void *out16);
int aimgrIsTool(const uint8_t *cid);
int aimgrIsTest(const uint8_t *cid);
int aimgrIsDex(const uint8_t *cid);
int aimgrIsTestSub4(const uint8_t *cid);
int aimgrIsCexSub2(const uint8_t *cid);
uint16_t aimgrExtractProductSubCode(const uint8_t *cid, uint32_t rawWord2);
int aimgrIsCexSub16Factory36(const uint8_t *cid);
int aimgrIsRetailOrChord(const uint8_t *cid);
int aimgrGetConsoleId(const uint8_t *rawCid, uint8_t *outCid);
int aimgrGetOpenPsId(uint32_t unused, void *outOpenPsId16);
int aimgrIsDolceHardware(void);
int aimgrIsNotDolceHardware(void);
int aimgrIsGenuineVshTarget(const uint8_t *cid);
int aimgrIsValidProductTarget(const uint8_t *cid);
uint16_t htole16Sm(uint16_t v);
uint16_t le16tohSm(uint16_t v);
uint32_t htole32Sm(uint32_t v);
uint32_t le32tohSm(uint32_t v);
uint64_t *htole64Sm(uint64_t *out, const uint64_t *in);
uint64_t *le64tohSm(uint64_t *out, const uint64_t *in);
int16_t bswap16Sm(uint16_t v);
int16_t htobe16Sm(uint16_t v);
int16_t be16tohSm(uint16_t v);
uint32_t bswap32Sm(uint32_t v);
uint32_t htobe32Sm(uint32_t v);
uint32_t be32tohSm(uint32_t v);
uint64_t *bswap64Sm(uint64_t *out, const uint64_t *in);
uint64_t *htobe64Sm(uint64_t *out, const uint64_t *in);
uint64_t *be64tohSm(uint64_t *out, const uint64_t *in);
void __cxa_pure_virtual(void);

/* string.c (0x8102aa..0x810438) */
void *memset(void *dst, int val, size_t len);
void *memcpy(void *dst, const void *src, size_t len);
int timingsafeBcmp(const void *b1, const void *b2, size_t len);
void *byteMemset(void *dst, int val, size_t len);
uint32_t __mulsi3(uint32_t a, uint32_t b);
uint32_t udivmodsi4(uint32_t num, uint32_t den, int modwanted);
int32_t __divsi3(int32_t a, int32_t b);
int32_t __modsi3(int32_t a, int32_t b);
uint32_t __udivsi3(uint32_t a, uint32_t b);
uint32_t __umodsi3(uint32_t a, uint32_t b);

#endif /* KPRX_AUTH_SM_H */
