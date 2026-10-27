/*
 * sm_common.h: Shared types, hardware MMIO definitions, and runtime function
 * declarations for PlayStation Vita os0:sm secure modules (Toshiba MeP C5).
 *
 * Internal function and structure names (except standard C/C++ runtime names)
 * are ours.
 */

#ifndef SM_COMMON_H
#define SM_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include "sm.h"

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
#ifndef _Static_assert
#define _STATIC_ASSERT_PASTE(a, b) a##b
#define _STATIC_ASSERT_PASTE2(a, b) _STATIC_ASSERT_PASTE(a, b)
#define _Static_assert(cond, msg) typedef char _STATIC_ASSERT_PASTE2(static_assertion_, __LINE__)[(cond) ? 1 : -1]
#endif
#endif

/* ============================================================================
 * SBL Error Codes
 * ============================================================================ */

#define SCE_SBL_ERROR_IO			((int)0x800f0005u)
#define SCE_SBL_ERROR_NO_MEMORY			((int)0x800f000cu)
#define SCE_SBL_ERROR_BUSY			((int)0x800f0010u)
#define SCE_SBL_ERROR_INVALID_VALUE		((int)0x800f0016u)
#define SCE_SBL_ERROR_NOT_SUPPORTED		((int)0x800f0021u)
#define SCE_SBL_ERROR_INVALID_ADDR		((int)0x800f0022u)
#define SCE_SBL_ERROR_BAD_DATA			((int)0x800f0024u)
#define SCE_SBL_ERROR_OVERFLOW			((int)0x800f0025u)
#define SCE_SBL_ERROR_CRYPTO			((int)0x800f0028u)
#define SCE_SBL_ERROR_NOT_INITIALIZED		((int)0x800f0029u)
#define SCE_SBL_ERROR_NOT_ALLOWED		((int)0x800f0030u)
#define SCE_SBL_ERROR_BIGNUM_INVALID		((int)0x800f0116u)
#define SCE_SBL_ERROR_SRV_ALLOC_FAILED		((int)0x800f0216u)
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
 * SM Service Runtime Structures (names ours)
 * ============================================================================ */

typedef struct SmServiceHookVtable {
	void (*invoke)(void *self, void *commBuf);	/* 0x00 */
	uint32_t reserved;				/* 0x04 */
} SmServiceHookVtable;

typedef struct SmServiceHookNode {
	uint32_t cmdId;					/* 0x00 */
	struct SmServiceHookNode *next;			/* 0x04 */
	const void *vtable;				/* 0x08 */
	void *handler;					/* 0x0c */
	void *arg0;					/* 0x10 */
	int32_t adj;					/* 0x14 */
} SmServiceHookNode;

_Static_assert(sizeof(SmServiceHookNode) == 0x18, "SmServiceHookNode size");

typedef int (*SmCmdMethod)(void *mgr, void *packetBuf);

typedef struct SmMemberFnPtr {
	SmCmdMethod fn;					/* 0x00 */
	int32_t adj;					/* 0x04 */
} SmMemberFnPtr;

typedef struct SmHookSpec {
	uint32_t cmdId;					/* 0x00 */
	SmServiceHookNode *node;			/* 0x04 */
	SmCmdMethod fn;					/* 0x08 */
	int32_t adj;					/* 0x0c */
} SmHookSpec;

typedef struct SmServiceContext {
	uint32_t running;				/* 0x00 */
	SmServiceHookNode *head;			/* 0x04 */
	uint32_t allocMask;				/* 0x08 */
	SmServiceHookNode *poolBase;			/* 0x0c */
	uint32_t poolSize;				/* 0x10: 0x180 */
	SmServiceHookNode nodes[16];			/* 0x14..0x193 */
} SmServiceContext;

_Static_assert(sizeof(SmServiceContext) == 0x194, "SmServiceContext size");

/* ============================================================================
 * sm_runtime.c (names ours)
 * ============================================================================ */

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

/* ============================================================================
 * sm_syscall.c (names ours except __cxa_pure_virtual)
 * ============================================================================ */

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
void __cxa_pure_virtual(void);

/* ============================================================================
 * muldiv.c & string helpers
 * ============================================================================ */

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

/* ============================================================================
 * Bigmac DMA / Crypto Helpers (names ours)
 * ============================================================================ */

int bigmacInitChannel0(uint32_t *ctrl);
int bigmacGetChannelStatus(const uint32_t *ctrl, uint32_t *outStatus);
int bigmacWaitChannelDone(const uint32_t *ctrl);
int bigmacMemcpy(void *dst, const void *src, uint32_t size);
int bigmacStartAesCipher(void *dst, const void *src, uint32_t size,
		const void *keyOrSlot, const void *iv, uint32_t cipherMode,
		uint32_t useExternalKey, uint32_t keyBits, const uint32_t *bmCtrl);
int bigmacStartMemcpy(void *dst, const void *src, uint32_t size,
		const uint32_t *bmCtrl);

/* ============================================================================
 * aimgr.c: Console ID / OpenPSID & Endian Helpers (names ours)
 * ============================================================================ */

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

/* ============================================================================
 * sm_heap.c: Page & Best-Fit Heap Allocator (names ours)
 * ============================================================================ */

typedef struct SmHeapBlock {
	void *userPtr;					/* 0x00 */
	uint32_t size;					/* 0x04 */
	uint32_t pageCount;				/* 0x08 */
	uint32_t padBytes;				/* 0x0c */
	struct SmHeapBlock *prev;			/* 0x10 */
	struct SmHeapBlock *next;			/* 0x14 */
} SmHeapBlock;

_Static_assert(sizeof(SmHeapBlock) == 0x18, "SmHeapBlock size");

void *operatorNew(size_t size);
void *operatorNewNothrow(size_t size, const void *nothrow);
void *operatorNewArray(size_t size);
void *operatorNewArrayNothrow(size_t size, const void *nothrow);
void operatorDelete(void *ptr);
void operatorDeleteNothrow(void *ptr, const void *nothrow);
void operatorDeleteArray(void *ptr);
void operatorDeleteArrayNothrow(void *ptr, const void *nothrow);
void smHeapInsertSorted(SmHeapBlock *blk, SmHeapBlock **listHead);
void smHeapUnlinkBlock(SmHeapBlock *blk, SmHeapBlock **listHead);
SmHeapBlock *smHeapMergeBlocks(SmHeapBlock *b1, SmHeapBlock *b2);
uint8_t smHeapAreBlocksAdjacentInPage(const SmHeapBlock *b1, const SmHeapBlock *b2);
int smHeapFreePages(void *pageAddr, uint32_t pageCount);
int smHeapAllocPages(void **outAddr, uint32_t pageCount, int zeroMem);
void smHeapCoalesceBlock(SmHeapBlock *blk);
int smHeapInitCore(void *poolBase, uint32_t poolSize);
int smHeapInit(void *poolBase, uint32_t poolSize);
void *smHeapMalloc(size_t size);
void *smHeapCalloc(size_t nmemb, size_t size);
void smHeapFree(void *ptr);
void *smHeapAllocPagesZeroed(uint32_t pageCount);
void *smHeapAllocPagesUninit(uint32_t pageCount);
int smHeapFreePagesThunk(void *pageAddr, uint32_t pageCount);
uint32_t smHeapAlignUpShift(uint32_t val, uint32_t alignMask, uint32_t shift);

/* ============================================================================
 * Certified File (cf_loader.c) & RSA / Bigmac (rsa_srvk.c) (names ours)
 * ============================================================================ */

typedef struct SelfAuthContext {
	uint8_t *hdrBuf;			/* 0x00 */
	uint32_t hdrBufSize;			/* 0x04 */
} SelfAuthContext;

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
	uint32_t type;				/* 0x10 */
	uint32_t index;				/* 0x14 */
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
int cfAesCipher(uint32_t chainSelect, uint32_t dirSelect, uint32_t keyBitsSelect,
		void *dst, const void *src, uint32_t size, const void *key,
		void *iv, void *outIv, void *unusedDesc);
int cfCopyBytes(void *dst, const void *src, uint32_t size, void *unusedDesc);
int cfHmacSha256InitShortKey(CfHmacContext *ctx, const void *key, uint32_t keyLen);
int cfVerifyRsaPkcs1v15(void *workBuf256, const void *signature256,
		const RsaPublicKey2048 *pubKey, const void *hash32,
		uint32_t digestAlgo, void *unusedDesc);

int bigmacStartAes128EcbEncrypt(void *dst, const void *src, uint32_t size,
		const void *key, const uint32_t *ctrl);
int bigmacStartAes128EcbDecrypt(void *dst, const void *src, uint32_t size,
		const void *key, const uint32_t *ctrl);
int bigmacStartAes256EcbEncrypt(void *dst, const void *src, uint32_t size,
		const void *key, const uint32_t *ctrl);
int bigmacStartAes256EcbDecrypt(void *dst, const void *src, uint32_t size,
		const void *key, const uint32_t *ctrl);
int bigmacStartAes128CbcEncrypt(void *dst, const void *src, uint32_t size,
		const void *key, const void *iv, const uint32_t *ctrl);
int bigmacStartAes128CbcDecrypt(void *dst, const void *src, uint32_t size,
		const void *key, const void *iv, const uint32_t *ctrl);
int bigmacStartAes256CbcEncrypt(void *dst, const void *src, uint32_t size,
		const void *key, const void *iv, const uint32_t *ctrl);
int bigmacStartAes256CbcDecrypt(void *dst, const void *src, uint32_t size,
		const void *key, const void *iv, const uint32_t *ctrl);
int bigmacStartAesCmac(void *dstMac, const void *src, uint32_t size,
		const void *key, uint32_t keyByteLen, const uint32_t *ctrl);
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

#endif /* SM_COMMON_H */
