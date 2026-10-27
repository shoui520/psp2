/*
 * secure_kernel.h: internal definitions and prototypes for the PS Vita
 * Toshiba MeP Secure Kernel (enp.bin, 0x800000..0x808180).
 *
 * Sony file names ("main.c", "handler.c") and function names ("sceSblSkMain",
 * "sceSblSkLoadSecureModule", "sceSblSkRunSecureModule", "sceSblSkHandlerSwi",
 * "sceSblACLibIsLoadable", "setupKeyManager", "hashContext", "self_loader::*",
 * "self_loader_wrapper::*") are recovered from 0.995.070-TOOL debug strings.
 * Other internal names are ours.
 */

#ifndef PSP2_SECURITY_SECURE_KERNEL_H
#define PSP2_SECURITY_SECURE_KERNEL_H

#ifndef _Static_assert
#define SK_JOIN_INNER(a, b) a##b
#define SK_JOIN(a, b) SK_JOIN_INNER(a, b)
#define _Static_assert(condition, message) \
	typedef char SK_JOIN(sk_static_assert_, __COUNTER__)[(condition) ? 1 : -1]
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#define REG8(addr)	(*(volatile uint8_t *)(uintptr_t)(addr))
#define REG16(addr)	(*(volatile uint16_t *)(uintptr_t)(addr))
#define REG32(addr)	(*(volatile uint32_t *)(uintptr_t)(addr))

/* Panic / debug PC obfuscation mask used in 4.00-DEBUGGER */
#define SK_PC_OBFUSCATION_MASK		0xfdb4a117u

/* ============================================================================
 * MeP CPU Context Frame (0x60 bytes, saved by context_asm.s, name ours)
 * ============================================================================ */

typedef struct SceSkCpuContext {
	uint32_t r0;			/* 0x00 */
	uint32_t r1;			/* 0x04 */
	uint32_t r2;			/* 0x08 */
	uint32_t r3;			/* 0x0c */
	uint32_t r4;			/* 0x10 */
	uint32_t r5;			/* 0x14 */
	uint32_t r6;			/* 0x18 */
	uint32_t r7;			/* 0x1c */
	uint32_t r8;			/* 0x20 */
	uint32_t r9;			/* 0x24 */
	uint32_t r10;			/* 0x28 */
	uint32_t r11;			/* 0x2c */
	uint32_t r12;			/* 0x30 */
	uint32_t tp;			/* 0x34 */
	uint32_t gp;			/* 0x38 */
	uint32_t sp;			/* 0x3c */
	uint32_t lp;			/* 0x40 */
	uint32_t sar;			/* 0x44 */
	uint32_t rpb;			/* 0x48 */
	uint32_t rpe;			/* 0x4c */
	uint32_t rpc;			/* 0x50 */
	uint32_t hi;			/* 0x54 */
	uint32_t lo;			/* 0x58 */
	uint32_t epc;			/* 0x5c */
} SceSkCpuContext;

_Static_assert(sizeof(SceSkCpuContext) == 0x60, "SceSkCpuContext size");

/* ============================================================================
 * Scatter-Gather & Buffer Descriptors (names ours)
 * ============================================================================ */

typedef struct SceSkRangeEntry {
	uint32_t paddr;			/* 0x00 */
	uint32_t size;			/* 0x04 */
} SceSkRangeEntry;

typedef struct SceSkScatterListDesc {
	uint32_t count;			/* 0x00 */
	const SceSkRangeEntry *entries;	/* 0x04 */
	uint32_t statusAddr;		/* 0x08 */
	uint32_t contextBufAddr;	/* 0x0c */
} SceSkScatterListDesc;

typedef struct SceSkBufferDesc {
	uint32_t addr;			/* 0x00 */
	uint32_t size;			/* 0x04 */
} SceSkBufferDesc;

typedef struct SceSkKeySeedDesc {
	void *buf;			/* 0x00 */
	uint32_t size;			/* 0x04 */
} SceSkKeySeedDesc;

/* ============================================================================
 * Secure Module Auth Info & Load Command Arguments (names ours)
 * ============================================================================ */

/* 0x50-byte command buffer read from ARM communication buffer in sceSblSkLoadSecureModule */
typedef struct SceSkLoadSmCmdArgs {
	uint32_t numRanges;		/* 0x00 */
	uint32_t rangesPaddr;		/* 0x04 */
	uint32_t returnStatusPaddr;	/* 0x08 */
	uint32_t smArg0;		/* 0x0c */
	uint32_t smArg1;		/* 0x10 */
	uint32_t smArg2;		/* 0x14 */
	uint32_t smArg3;		/* 0x18 */
	uint32_t reserved1c;		/* 0x1c */
	uint32_t flags20;		/* 0x20 */
	uint32_t flags24;		/* 0x24 */
	uint64_t callerAuthId;		/* 0x28 */
	uint8_t callerDigest[32];	/* 0x30 */
} SceSkLoadSmCmdArgs;

_Static_assert(sizeof(SceSkLoadSmCmdArgs) == 0x50, "SceSkLoadSmCmdArgs size");

/* 0x90-byte authority / capability block for caller or target SM (name ours) */
typedef struct SceSkAuthorityInfo {
	uint64_t authId;		/* 0x00 (+0x08 / +0x98 in SceSkSmAuthContext) */
	uint64_t minVersion;		/* 0x08 (+0x10 / +0xa0 in SceSkSmAuthContext) */
	uint8_t cap1[32];		/* 0x10 (+0x18 / +0xa8 in SceSkSmAuthContext) */
	uint8_t cap3[32];		/* 0x30 (+0x38 / +0xc8 in SceSkSmAuthContext) */
	uint8_t sharedSecret0[16];	/* 0x50 (+0x58 / +0xe8 in SceSkSmAuthContext) */
	uint8_t reserved60[48];		/* 0x60 (+0x68 / +0xf8 in SceSkSmAuthContext) */
} SceSkAuthorityInfo;

_Static_assert(sizeof(SceSkAuthorityInfo) == 0x90, "SceSkAuthorityInfo size");

/* 0x130-byte SM load context / auth info populated during SM loading (name ours) */
typedef struct SceSkSmAuthContext {
	uint32_t reserved00;		/* 0x00 */
	uint32_t attrFlags;		/* 0x04 */
	SceSkAuthorityInfo caller;	/* 0x08..0x97 */
	SceSkAuthorityInfo target;	/* 0x98..0x127 */
	uint32_t moduleType;		/* 0x128 */
	uint32_t reserved12c;		/* 0x12c */
} SceSkSmAuthContext;

_Static_assert(sizeof(SceSkSmAuthContext) == 0x130, "SceSkSmAuthContext size");

/* 0x80-byte target SELF auth info extracted by selfExtractSelfAuthInfo (name ours) */
typedef struct SceSkSelfAuthInfo {
	uint32_t size;			/* 0x00: 0x80 */
	uint32_t version;		/* 0x04: 1 */
	uint64_t programVersion;	/* 0x08: from SceAppInfo + 0x10 */
	uint8_t digest[32];		/* 0x10: from SceSelfControlInfo type 4 + 0x24 */
	uint64_t requiredSysVersion;	/* 0x30: from SceSelfControlInfo type 4 + 0x48 */
	uint64_t reserved38;		/* 0x38 */
	uint8_t sharedSecret[4][16];	/* 0x40..0x7f: from SceSelfControlInfo type 7 */
} SceSkSelfAuthInfo;

_Static_assert(sizeof(SceSkSelfAuthInfo) == 0x80, "SceSkSelfAuthInfo size");

/* ============================================================================
 * Certified File (CF) / SELF Loader Context & Callbacks (names ours)
 * ============================================================================ */

typedef int (*SelfHeaderReadCallback)(uintptr_t dstAddr, const uint64_t *offset,
		uint32_t size, uint32_t *outBytesRead, void *arg);

typedef int (*SelfSegmentReadCallback)(const uint64_t *offset, uint32_t size,
		uint64_t *outSrcAddr, uint64_t *outDstAddr,
		uint32_t *outBytesRead, uint32_t *outFlags, void *arg);

typedef int (*SelfSegmentDoneCallback)(const uint64_t *offset, uint32_t reqSize,
		const uint64_t *srcAddr, const uint64_t *dstAddr,
		uint32_t bytesRead, uint32_t flags, void *arg);

typedef struct SelfAuthContext {
	void *scratchBuf;		/* 0x00 */
	uint32_t scratchSize;		/* 0x04 */
} SelfAuthContext;

/* ============================================================================
 * Bigmac Crypto Engine & RSA Bignum Structures (names ours)
 * ============================================================================ */

typedef struct BigmacChannelRegs {
	volatile uint32_t src;		/* +0x00 */
	volatile uint32_t dst;		/* +0x04 */
	volatile uint32_t length;	/* +0x08 */
	volatile uint32_t func;		/* +0x0c */
	volatile uint32_t keySlot;	/* +0x10 */
	volatile uint32_t iv;		/* +0x14 */
	volatile uint32_t nextDesc;	/* +0x18 */
	volatile uint32_t trigger;	/* +0x1c */
	volatile uint32_t statusRes;	/* +0x20 */
	volatile uint32_t status;	/* +0x24 */
	volatile uint32_t commit;	/* +0x28 */
	volatile uint32_t reserved2c[2];/* +0x2c */
	volatile uint32_t fillValue;	/* +0x34 */
	volatile uint32_t reserved38;	/* +0x38 */
	volatile uint32_t rng;		/* +0x3c */
	volatile uint32_t unk40;	/* +0x40 */
	volatile uint32_t reserved44[15];/* +0x44..0x7f */
} BigmacChannelRegs;

typedef struct BigmacRegs {
	BigmacChannelRegs ch[2];		/* 0xe0050000..0xe00500ff */
	volatile uint32_t reserved100;		/* 0xe0050100 */
	volatile uint32_t memMode;		/* 0xe0050104 */
	volatile uint32_t reserved108[62];	/* 0xe0050108..0xe00501ff */
	volatile uint32_t externalKey[64];	/* 0xe0050200..0xe00502ff */
} BigmacRegs;

#define BIGMAC ((BigmacRegs *)(uintptr_t)0xe0050000u)

typedef struct BigmacContext {
	uint32_t channel;		/* 0x00 */
	uint32_t flags;			/* 0x04 */
} BigmacContext;

typedef struct RsaPublicKeyParam {
	const uint32_t *modulus;	/* 0x00 */
	uint32_t modulusWordCount;	/* 0x04 */
	const uint32_t *exponent;	/* 0x08 */
	uint32_t exponentWordCount;	/* 0x0c */
} RsaPublicKeyParam;

/* ============================================================================
 * start.s (0x800000..0x80025f)
 * ============================================================================ */

void skReenterKernel(void);
void skFlushIcache(void);
void skInvalidateAndFlushIcache(void);

/* ============================================================================
 * exception.c (0x800260..0x8002bf)
 * ============================================================================ */

void excResetVectorHandler(void);
void excReservedInsnHandler(void);
void excZeroDivHandler(void);

/* ============================================================================
 * intr_asm.s (0x8002c0..0x8003d7)
 * ============================================================================ */

void skIntcInit(void);
void skEnableSwiException(void);
uint32_t skGetActiveIrqNumber(void);
void skEnableAllIrqChannels(void);
void skTriggerArmInterrupt(uint16_t code);

/* ============================================================================
 * main.c (0x8003d8..0x8004f3)
 * ============================================================================ */

extern uint32_t __stack_chk_guard;
int sceSblSkMain(void);
void __stack_chk_fail(uint32_t callerLp);

/* ============================================================================
 * handler.c (0x8004f4..0x801055)
 * ============================================================================ */

int sceSblSkLoadSecureModule(void);
void sceSblSkHandlerCmdIrq(void);
int sceSblSkHandlerSwi(uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t swiNum);
void *byteMemcpy(void *dst, const void *src, uint32_t size);
void *byteMemset(void *dst, int val, uint32_t size);
uint32_t strnlenSimple(const char *str, uint32_t maxLen);

/* ============================================================================
 * uart_debug.c (0x801056..0x801b09)
 * ============================================================================ */

void writeDebugLogString(const char *str);
int skDebugPrintf(const char *fmt, ...);
void setPanicArmInterruptCode(uint32_t code);
void skPanic(const char *fmt, ...);
uint32_t getExpectedSocVersion(void);
uint32_t readHardwareSocVersion(void);
uint32_t *getHardwareSocVersionPair(uint32_t *outPair);
int snprintf(char *buf, size_t size, const char *fmt, ...);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
void writeKeyringSlot32(uint16_t slotIndex, const uint32_t *words8);

/* ============================================================================
 * mem_region.c (0x801b0a..0x80203b)
 * ============================================================================ */

void setKeyringSlotProtection(uint16_t slotIndex, uint32_t protMask);
void enableBusErrorInterrupts(void);
int setupKeyringAccessPermissions(void);
void wipeAndLockAllKeyringSlots(void);
int isAddrInCmepSram(uint32_t addr);
int isAddrInSkSram(uint32_t addr);
int isAddrInSmSram(uint32_t addr);
int isAddrInTzDram(uint32_t addr);
int isAddrInNonSecureDram(uint32_t addr);
int checkMemoryRangeWithValidator(uint32_t addr, uint32_t size,
		uint32_t panicCode, int (*validator)(uint32_t));
void setArmCommBufferAddr(uint32_t addr);
uint32_t getArmCommBufferAddr(void);
void setSmReturnStatusAddr(uint32_t addr);
uint32_t getSmReturnStatusAddr(void);
uint32_t getSmSramBase(void);
uint32_t getSmSramEnd(void);
uint32_t getSmSramSize(void);
int checkRangeInCmepSram(uint32_t addr, uint32_t size);
int checkRangeInSkSram(uint32_t addr, uint32_t size);
int checkRangeInSmSram(uint32_t addr, uint32_t size);
int checkRangeInTzDram(uint32_t addr, uint32_t size);
int checkRangeInNonSecureDram(uint32_t addr, uint32_t size);
int checkRangeInAnyDram(uint32_t addr, uint32_t size);
void setDebugLogBufferAddr(uint32_t addr);
uint32_t getDebugLogBufferAddr(void);
void setDebugLogBufferSize(uint32_t size);
uint32_t getDebugLogBufferSize(void);
uint32_t getSkSrvkSramBase(void);
uint32_t getSkSrvkSramSize(void);
void setSrvkListAddr(uint32_t addr);
uint32_t getSrvkListAddr(void);
void setSrvkListSize(uint32_t size);
uint32_t getSrvkListSize(void);
void setSkState(uint32_t state);
uint32_t getSkState(void);
int isCommandAllowedInCurrentState(uint32_t cmdIndex);

/* ============================================================================
 * suspend.c (0x80203c..0x802767)
 * ============================================================================ */

int setupKeyManager(uint32_t seqno, const SceSkKeySeedDesc *seedDesc);
void hashContext(void *dstDigest, const void *src, uint32_t size, uint32_t mode);
void transferScatterGatherRange(void *localBuf, uint32_t totalSize,
		uint32_t startOffset, const SceSkScatterListDesc *sgDesc,
		uint32_t opType, const SceSkKeySeedDesc *seedDesc);
void *invalidateSeqno(const SceSkKeySeedDesc *seedDesc);
void suspendSmContext(const SceSkScatterListDesc *sgDesc, int isFromSwi);
int resumeSmContext(const SceSkScatterListDesc *sgDesc);

/* ============================================================================
 * context_asm.s (0x802768..0x802851)
 * ============================================================================ */

void skSaveKernelContext(void);

/* ============================================================================
 * intr.c (0x802852..0x802925)
 * ============================================================================ */

extern SceSkCpuContext g_irqCpuContext;
extern SceSkCpuContext g_cmdIrqCpuContext;
extern SceSkCpuContext g_kernelCpuContext;
extern SceSkCpuContext g_swiCpuContext;

void skSetIrqMaskRaw(uint32_t mask);
void skRegisterIrqHandler(uint32_t irqNum, uint32_t handlerAddr);
uint32_t skGetIrqHandler(uint32_t irqNum);
uint32_t skSaveIrqMask(void);
void skRestoreIrqMask(uint32_t savedMask);

/* ============================================================================
 * self_loader.c (0x802926..0x804c3d)
 * ============================================================================ */

int selfLoaderWrapperLoadSm(const SceSkLoadSmCmdArgs *cmdArgs, SceSkSmAuthContext *authCtx);
void copyRangeEntry8(void *dst, const void *src);
int selfScatterHeaderReadCb(uintptr_t dstAddr, const uint64_t *offset,
		uint32_t size, uint32_t *outBytesRead, void *arg);
int selfScatterSegmentReadCb(const uint64_t *offset, uint32_t size,
		uint64_t *outSrcAddr, uint64_t *outDstAddr,
		uint32_t *outBytesRead, uint32_t *outFlags, void *arg);
int selfInitAndVerifyHeader(const SelfAuthContext *ctx, const void *metaKey,
		const void *rootPubKey, uint32_t sigType, int allowDevelopment,
		uint16_t expectedCategory, SelfHeaderReadCallback readCb,
		void *readCbArg, int32_t expectedExtHeaderSize,
		const uint64_t *expectedHeaderLen);
int cfFindSectionEntryByTypeAndIndex(const SelfAuthContext *ctx,
		uint32_t segType, uint32_t segIndex, const void **outSection);
int selfDecryptAndLoadSegment(const SelfAuthContext *ctx, uint32_t segType,
		uint32_t segIndex, SelfSegmentReadCallback readCb, void *readCbArg,
		SelfSegmentDoneCallback doneCb, void *doneCbArg);
int selfVerifySvnAndDecryptMetadata(const SelfAuthContext *ctx);
int selfGetAppInfo(const SelfAuthContext *ctx, const uint8_t **outAppInfo);
int selfGetElfHeader(const SelfAuthContext *ctx, const void **outEhdr);
int selfGetProgramHeaders(const SelfAuthContext *ctx, const void **outPhdrs);
int selfGetSegmentInfos(const SelfAuthContext *ctx, const void **outSegInfos);
int selfGetControlInfo(const SelfAuthContext *ctx, const uint8_t **outCtrlInfo,
		uint64_t *outCtrlInfoSize);
int selfFindControlInfo(const SelfAuthContext *ctx, const uint8_t **outCtrlInfo,
		uint32_t targetType);
int selfFindExtHeader(const SelfAuthContext *ctx, const uint8_t **outExtHdr,
		uint32_t targetType);
int selfLoaderWrapperGetAllSelfAuthInfo(const SelfAuthContext *ctx,
		SceSkAuthorityInfo *outKblParam, uint32_t flag62,
		SceSkSelfAuthInfo *outSelfInfo, uint32_t *outAttrFlags);

/* ============================================================================
 * aclib.c (0x804c3e..0x8050e1)
 * ============================================================================ */

int aclibLoadSrvk(const SceSkBufferDesc *srvkSrcDesc);
int aclibEqual16(const uint8_t *a, const uint8_t *b);
int sceSblACLibIsLoadable(const SceSkSmAuthContext *authCtx,
		const SceSkSelfAuthInfo *targetSelfInfo, const SceSkBufferDesc *srvkDesc);

/* ============================================================================
 * srvk.c (0x8050e2..0x805a69)
 * ============================================================================ */

int srvkLoadAndVerify(uint32_t rangesPaddr, uint32_t numRanges,
		const SceSkBufferDesc *dstRegion, SceSkBufferDesc *outLoadedDesc);
int srvkGetTargetTypeClass(void);
int srvkIsDevelopmentTarget(void);
int srvkIsRetailOrToolTarget(void);
int srvkCheckQafByte0Bit0(void);
int srvkCheckDipswBit195(void);
int srvkCheckDipswBit197(void);
int srvkCheckDipswBit195Thunk(void);
int srvkCheckQaFlagByte15Bit0(void);
int srvkCheckQaFlagByte13Bit0(void);
int srvkCheckQaFlagByte14Bit0(void);
int srvkCheckQaFlagByte13Bit1(void);
int srvkCheckQaFlagByte11Bit4(void);
int srvkCheckFlagBit62(void);
uint32_t srvkReadKeyring517Word0(void);
int srvkAuthenticateAndLoad(uint32_t rangesPaddr, uint32_t numRanges,
		const SceSkBufferDesc *dstRegion, SceSkBufferDesc *outLoadedDesc);
int srvkCheckModuleRevoked(const SceSkBufferDesc *modulePair,
		const SceSkBufferDesc *srvkDesc);

/* ============================================================================
 * sm_state.c (0x805a6a..0x805aab)
 * ============================================================================ */

void clearSmCriticalSectionActive(void);
void setSmCriticalSectionActive(void);
uint32_t isSmCriticalSectionActive(void);
void setSmStopPending(void);
void clearSmStopPending(void);
uint32_t isSmStopPending(void);
void setSmSuspendPending(void);
void clearSmSuspendPending(void);
uint32_t isSmSuspendPending(void);

/* ============================================================================
 * dmac_rng.c (0x805aac..0x805f2b)
 * ============================================================================ */

void freeSeqnoNode(void *node);
uint32_t allocateSeqno(void);
int getSeqnoRandomBytes(uint32_t seqno, void *out12);
int removeAndFreeSeqno(uint32_t seqno);
void dmac5StartCmd(uint32_t channel, void *dst, const void *src, uint32_t size,
		uint32_t keySlot, uint32_t ivAddr, uint32_t funcCode, uint32_t memMode);
uint32_t dmac5WaitChannel(uint32_t channel);
uint32_t dmac5CopyCh0(void *dst, const void *src, uint32_t size);
uint32_t dmac5FillCh0(void *dst, uint32_t fillVal, uint32_t size, uint32_t memMode);
uint32_t dmac5MemcpyWithModeCh0(void *dst, const void *src, uint32_t size, uint32_t memMode);
int checkHardwareBootStateAndSocVersion(void);
int lockKeyringSlot50eIfReady(void);
int checkSocVersionMatches(uint32_t version);

/* ============================================================================
 * aimgr.c (0x805f2c..0x8064df)
 * ============================================================================ */

void clearConsoleIdBuffer16(void *buf16);
void wipeConsoleIdBuffer16(void *buf16);
void readConsoleIdRaw16(void *out16);
void readConsoleIdRaw16Alias1(void *out16);
void readConsoleIdRaw16Alias2(void *out16);
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
uint16_t htole16Sk(uint16_t v);
uint16_t le16tohSk(uint16_t v);
uint32_t htole32Sk(uint32_t v);
uint32_t le32tohSk(uint32_t v);
uint64_t *htole64Sk(uint64_t *out, const uint64_t *in);
uint64_t *le64tohSk(uint64_t *out, const uint64_t *in);
int16_t bswap16Sk(uint16_t v);
int16_t htobe16Sk(uint16_t v);
int16_t be16tohSk(uint16_t v);
uint32_t bswap32Sk(uint32_t v);
uint32_t htobe32Sk(uint32_t v);
uint32_t be32tohSk(uint32_t v);
uint64_t *bswap64Sk(uint64_t *out, const uint64_t *in);
uint64_t *htobe64Sk(uint64_t *out, const uint64_t *in);
uint64_t *be64tohSk(uint64_t *out, const uint64_t *in);

/* ============================================================================
 * rsa_srvk.c (0x8064e0..0x80720b)
 * ============================================================================ */

int bigmacInitContext(BigmacContext *ctx);
int bigmacGetChannelStatus(const BigmacContext *ctx, uint32_t *outStatus);
int bigmacWaitChannel(const BigmacContext *ctx);
int bigmacStartAesCipher(void *dst, const void *src, uint32_t size,
		const void *key, const void *iv, uint32_t dir, uint32_t chainMode,
		uint32_t keyBits, const BigmacContext *ctx);
void cfBswap128(uint32_t *words4);
int bigmacAes128CbcEncryptSync(void *dst, const void *src, uint32_t size,
		const void *key, void *iv);
int bigmacAes256CbcDecryptSync(void *dst, const void *src, uint32_t size,
		const void *key, void *iv);
int bigmacSha256(void *dstDigest, const void *src, uint32_t size);
int cfHmacSha256InitBlockKey(void *ctx, const void *key64, uint32_t keyLen);
int cfHmacSha256Update(void *ctx, const void *data, uint32_t size, uint32_t isFinalFlag);
int cfHmacSha256Final(uint8_t *outDigest, void *ctx);
int verifyRsaPkcs1v15Blocking(void *workBuf256, const void *signature256,
		const RsaPublicKeyParam *pubKey, const uint8_t *expectedDigest,
		uint32_t hashType);
int bigmacAesCmacWithKey(void *dst, const void *src, uint32_t size,
		const void *key, uint32_t keySize, const BigmacContext *ctx);
int bigmacMemcpy(void *dst, const void *src, uint32_t size);
int bigmacStartHash(void *dstDigest, const void *src, uint32_t size,
		uint32_t hashSubMode, const BigmacContext *ctx);

/* ============================================================================
 * string.c (0x80720c..0x8072e3)
 * ============================================================================ */

void *memset(void *dst, int val, size_t len);
void *memcpy(void *dst, const void *src, size_t len);
int timingsafeBcmp(const void *b1, const void *b2, size_t len);
int cmpU64(const uint64_t *a, const uint64_t *b);

#endif /* PSP2_SECURITY_SECURE_KERNEL_H */
