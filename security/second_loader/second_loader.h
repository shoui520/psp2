/*
 * second_loader.h: internal interfaces, hardware registers, and data structures
 * for the PS Vita second boot loader (SLSK).
 *
 * Shared SoC register maps and ABI structures are included from sys/internal/include
 * and sys/internal/kernel/src/update_mgr. Where Sony's names are known from
 * debug records, update_mgr, SceSyscon, SceGpio, SceSdif, ScePervasive, or
 * prototype strings (0.931..0.996), those names are used; descriptive names
 * invented by us are marked "name ours".
 */
#ifndef SECOND_LOADER_H
#define SECOND_LOADER_H

/* GCC 4.5 predates C11; retain the shared headers' layout checks in C99. */
#ifndef _Static_assert
#define SL_JOIN_INNER(a, b) a##b
#define SL_JOIN(a, b) SL_JOIN_INNER(a, b)
#define _Static_assert(condition, message) \
	typedef char SL_JOIN(sl_static_assert_, __COUNTER__)[(condition) ? 1 : -1]
#endif

#include <stdint.h>
#include <stddef.h>
#include <scetypes.h>
#include <lowio/pervasive_regs.h>
#include <lowio/gpio_regs.h>
#include <lowio/spi_regs.h>
#include <lowio/uart_regs.h>
#include <cmep_regs.h>
#include <sdif_regs.h>
#include <sdstor/mbr.h>
#include <syscon/syscon_scratchpad.h>
#include <kernel_boot_loader/kbl_param.h>
#include <sbls.h>
#include <snvs.h>
#include <modulemgr/elf.h>

/* Short integer typedefs used across loader / driver units */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

/* MMIO register access helpers */
#define MMIO_R32(addr)		(*(volatile uint32_t *)(uintptr_t)(addr))
#define MMIO_W32(addr, val)	(*(volatile uint32_t *)(uintptr_t)(addr) = (uint32_t)(val))
#define MMIO_R16(addr)		(*(volatile uint16_t *)(uintptr_t)(addr))
#define MMIO_W16(addr, val)	(*(volatile uint16_t *)(uintptr_t)(addr) = (uint16_t)(val))
#define MMIO_R8(addr)		(*(volatile uint8_t *)(uintptr_t)(addr))
#define MMIO_W8(addr, val)	(*(volatile uint8_t *)(uintptr_t)(addr) = (uint8_t)(val))

/* Section placement attributes for GP-relative (.sdata / .sbss) globals */
#define SL_SDATA
#define SL_SBSS

/* SBL / second_loader error codes (names ours, values from SCE SBL error space) */
#define SCE_SBL_ERROR_COMMON_ENOENT	((int)0x800f0002)
#define SCE_SBL_ERROR_COMMON_EIO	((int)0x800f0005)
#define SCE_SBL_ERROR_COMMON_EBUSY	((int)0x800f0010)
#define SCE_SBL_ERROR_COMMON_EINVAL	((int)0x800f0016)
#define SCE_SBL_ERROR_COMMON_EILLEGAL	((int)0x800f0024)
#define SCE_SBL_ERROR_COMMON_ENOSUP	((int)0x800f0025)
#define SCE_SBL_ERROR_COMMON_EAUTH	((int)0x800f0027)
#define SCE_SBL_ERROR_QAF_CMAC		((int)0x800f0027)
#define SCE_SBL_ERROR_QAF_OPENPSID	((int)0x800f0039)
#define SCE_SBL_ERROR_SL_ENOENT		((int)0x800f0002)
#define SCE_SBL_ERROR_SL_EIO		((int)0x800f0005)
#define SCE_SBL_ERROR_SL_EINVAL		((int)0x800f0016)
#define SCE_SBL_ERROR_SL_EILLEGAL	((int)0x800f0024)

/* ============================================================================
 * Physical Memory Map & Hardware Register Blocks
 * ============================================================================ */

/* KBL parameter block in secure SRAM (0x1f000100) and warm-boot backup in DRAM */
#define KBL_PARAM_PA			0x1f000100u
#define KBL_PARAM_WARMBOOT_PA		0x4001fd00u
#define BOOT_VECTOR_SRAM_PA		0x1f000000u
#define BOOT_VECTOR_DRAM_PA		0x40000000u
#define BOOT_VECTOR_MAGIC_PA		0x400000a0u
#define BOOT_VECTOR_PAGE_MAGIC		0x9e3199b7u	/* Sony's magic in SKBL boot.c */
#define RESUME_CONTEXT_PA		0x4001ff00u
#define RESUME_PAYLOAD_MAGIC		0xacb4acb1u

#define KBL_PARAM			((SceKblParam *)(uintptr_t)KBL_PARAM_PA)
#define PERVASIVE_VID			((ScePervasiveVidRegisters *)(uintptr_t)PERVASIVE_VID_BASE)

/* Cmep Secure Bus Controller / Secure Range registers (0xe0020000; name ours) */
#define CMEP_SBOOT_MEM_BASE		0xe0020000u
#define CMEP_SBOOT_KEY_COPY_PA		0xe0020100u

/* Keyring controller (0xe0030000) and slot array (0xe0058000; names after first_loader) */
#define KEYRING_CONTROL_BASE		0xe0030000u
#define KEYRING_SLOTS_BASE		0xe0058000u

typedef struct KeyringControlRegisters {
	volatile uint32_t data;			/* 0x00 */
	volatile uint32_t reserved0[7];		/* 0x04..0x1c: words 1..7 of key write port */
	volatile uint32_t protection;		/* 0x20: slot write command */
	volatile uint32_t status;		/* 0x24: slot lock command */
} KeyringControlRegisters;

typedef struct KeyringSlotRegisters {
	volatile uint32_t words[8];		/* 0x20 bytes per key slot */
} KeyringSlotRegisters;

_Static_assert(offsetof(KeyringControlRegisters, protection) == 0x20, "Keyring protection");
_Static_assert(sizeof(KeyringSlotRegisters) == 0x20, "Keyring slot size");

#define KEYRING_CTRL			((KeyringControlRegisters *)(uintptr_t)KEYRING_CONTROL_BASE)
#define KEYRING_SLOTS			((KeyringSlotRegisters *)(uintptr_t)KEYRING_SLOTS_BASE)

/* Bignum RSA-2048 coprocessor (0xe0040000; names after first_loader) */
#define BIGNUM_BASE			0xe0040000u

/* Bigmac crypto/DMA engine (0xe0050000: channel 0 at +0x00, channel 1 at +0x80; names after first_loader) */
#define BIGMAC_BASE			0xe0050000u
#define SCE_BIGMAC_BASE			BIGMAC_BASE

#define SCE_BIGMAC_SRC			0x00u
#define SCE_BIGMAC_DST			0x04u
#define SCE_BIGMAC_LEN			0x08u
#define SCE_BIGMAC_FUNC			0x0cu
#define SCE_BIGMAC_KEYSLOT		0x10u
#define SCE_BIGMAC_IV			0x14u
#define SCE_BIGMAC_TRIGGER		0x1cu
#define SCE_BIGMAC_STATUS		0x24u
#define SCE_BIGMAC_RNG			0x3cu

#define SCE_BIGMAC_REG(ch, off) \
	(*(volatile uint32_t *)(uintptr_t)(SCE_BIGMAC_BASE + ((uint32_t)(ch) * 0x80u) + (uint32_t)(off)))

typedef struct BigmacChannelRegisters {
	volatile uint32_t src;			/* 0x00 */
	volatile uint32_t dst;			/* 0x04 */
	volatile uint32_t length;		/* 0x08 */
	volatile uint32_t func;			/* 0x0c */
	volatile uint32_t keyslot;		/* 0x10 */
	volatile uint32_t iv;			/* 0x14 */
	volatile uint32_t nextDesc;		/* 0x18 */
	volatile uint32_t trigger;		/* 0x1c */
	volatile uint32_t errorStatus;		/* 0x20 */
	volatile uint32_t status;		/* 0x24 */
	volatile uint32_t busConfig;		/* 0x28 */
	volatile uint32_t alignStatus;		/* 0x2c */
	uint32_t reserved30;			/* 0x30 */
	volatile uint32_t fillValue;		/* 0x34 */
	uint32_t reserved38;			/* 0x38 */
	volatile uint32_t rng;			/* 0x3c */
	uint32_t reserved40[16];		/* 0x40..0x7f */
} BigmacChannelRegisters;

typedef struct BigmacRegisters {
	BigmacChannelRegisters ch[2];		/* 0x000: ch0, 0x080: ch1 */
	uint32_t reserved100[64];		/* 0x100..0x1ff */
	volatile uint32_t externalKey[8];	/* 0x200..0x21f */
} BigmacRegisters;

_Static_assert(sizeof(BigmacChannelRegisters) == 0x80, "BigmacChannelRegisters");
_Static_assert(offsetof(BigmacRegisters, ch[1]) == 0x80, "Bigmac ch1");
_Static_assert(offsetof(BigmacRegisters, externalKey) == 0x200, "Bigmac externalKey");

#define BIGMAC				((BigmacRegisters *)(uintptr_t)BIGMAC_BASE)

/* Bigmac operation context (8 bytes on stack; name ours) */
typedef struct BigmacContext {
	uint32_t channel;			/* 0x00: 0 or 1 */
	uint32_t flags;				/* 0x04: 0x2080 default (bit 7 = external key) */
} BigmacContext;

typedef BigmacContext SceBigmacContext;

_Static_assert(sizeof(BigmacContext) == 8, "BigmacContext");

/* ============================================================================
 * Subsystem Structures
 * ============================================================================ */

/* Keyring lock range entry (0x80bcdc..0x80bd1b; name ours) */
typedef struct KeyringLockRange {
	uint32_t startSlot;
	uint32_t endSlot;
} KeyringLockRange;

_Static_assert(sizeof(KeyringLockRange) == 8, "KeyringLockRange");

/* Decrypted resume context payload at 0x4001ff00 (size 0xa0; name ours) */
typedef struct DecryptedResumePayload {
	uint32_t magic;				/* 0x00: RESUME_PAYLOAD_MAGIC (0xacb4acb1) */
	uint32_t version;			/* 0x04: 1 */
	uint32_t reserved08[2];			/* 0x08 */
	uint8_t slot511[32];			/* 0x10 */
	uint8_t slot512[32];			/* 0x30 */
	uint8_t slot517[32];			/* 0x50 */
	uint8_t slot519[32];			/* 0x70 */
	uint8_t cmac[16];			/* 0x90: AES-CMAC over 0x00..0x8f */
} DecryptedResumePayload;

_Static_assert(sizeof(DecryptedResumePayload) == 0xa0, "DecryptedResumePayload");
_Static_assert(offsetof(DecryptedResumePayload, cmac) == 0x90, "DecryptedResumePayload cmac");

/* SNVS AES-XTS + AES-CMAC crypto engine at 0x80cf40 (size 0x18; name ours) */
typedef struct SnvsXtsCmacEngine SnvsXtsCmacEngine;

typedef struct SnvsCryptoVtable {
	uint32_t rttiHeader[2];
	void (*destroy)(SnvsXtsCmacEngine *eng);
	void (*deleteObj)(SnvsXtsCmacEngine *eng);
} SnvsCryptoVtable;

struct SnvsXtsCmacEngine {
	const SnvsCryptoVtable *vtable;		/* 0x00: 0x80bf90 */
	const void *dataKey;			/* 0x04 */
	const void *tweakKey;			/* 0x08 */
	int xtsKeyBits;				/* 0x0c */
	const void *cmacKey;			/* 0x10 */
	int cmacKeyBits;			/* 0x14 */
};

_Static_assert(sizeof(SnvsXtsCmacEngine) == 0x18, "SnvsXtsCmacEngine");

/*
 * SceSyscon packet and driver state structures.
 * Layout matches <syscon/syscon.h> and sys/internal/driver/src/syscon/syscon_private.h
 * (without kernel spinlock/memblock/mutex fields at the end of SysconControl).
 */
typedef struct SceSysconPacket {
	struct SceSysconPacket *next;		/* 0x00 */
	uint32_t flags;				/* 0x04: status / flags */
	int status;				/* 0x08: semaId (-1 in bootloader) */
	uint32_t queueIndex;			/* 0x0c: queue (0..17) */
	uint8_t tx[32];				/* 0x10 */
	uint8_t rx[32];				/* 0x30 */
	uint8_t *extTxBuf;			/* 0x50: externalTx */
	uint8_t *extRxBuf;			/* 0x54: externalRx */
	uint32_t extRxMaxSize;			/* 0x58: externalRxSize */
	uint32_t extRxReceived;			/* 0x5c: received */
	int (*callback)(struct SceSysconPacket *pkt, void *argp); /* 0x60 */
	void *callbackArg;			/* 0x64: argp */
	uint32_t time;				/* 0x68 */
	uint32_t timeoutUs;			/* 0x6c: delay */
	uint32_t reserved[4];			/* 0x70..0x7f */
} SceSysconPacket;

_Static_assert(sizeof(SceSysconPacket) == 0x80, "SceSysconPacket");
_Static_assert(offsetof(SceSysconPacket, extTxBuf) == 0x50, "SceSysconPacket extTxBuf");
_Static_assert(offsetof(SceSysconPacket, callback) == 0x60, "SceSysconPacket callback");

typedef void (*SceSysconCallback)(int value, void *argp);

typedef struct SceSysconDebugHandlers {
	uint32_t size;
	void (*start)(SceSysconPacket *packet);
	void (*end)(SceSysconPacket *packet);
} SceSysconDebugHandlers;

typedef struct SysconCallbackEntry {
	SceSysconCallback callback;
	void *argp;
} SysconCallbackEntry;

typedef struct SysconNotificationDescriptor {
	uint8_t byteIndex;
	uint8_t mask;
	uint8_t polarities;
	uint8_t subcode;
	uint8_t *flag;
} SysconNotificationDescriptor;

_Static_assert(sizeof(SysconNotificationDescriptor) == 8, "SysconNotificationDescriptor");

typedef struct SysconDriverState {
	SceSysconPacket *activePacket;			/* 0x000: currentPacket */
	SceSysconPacket *queueHead[18];			/* 0x004: queueHeads */
	SceSysconPacket *queueTail[18];			/* 0x04c: queueTails */
	const SceSysconDebugHandlers *debugHooks;	/* 0x094: debugHandlers */
	volatile SceSpiRegisters *spiRegs;		/* 0x098: spiRegisters */
	uint32_t lastTransferTime;			/* 0x09c */
	uint32_t cmdDelayUs;				/* 0x0a0: startupPacketDelay */
	uint32_t retryDelayUs;				/* 0x0a4: commandPacketDelay */
	uint32_t statusWord;				/* 0x0a8: notificationBits */
	uint32_t extStatusWord;				/* 0x0ac: multiCnInfo */
	uint32_t command130Info;			/* 0x0b0 */
	uint32_t command884State;			/* 0x0b4 */
	uint32_t command885State;			/* 0x0b8 */
	uint8_t lastRxStatus;				/* 0x0bc: statusFlags */
	uint8_t prevRxStatus;				/* 0x0bd: pendingStatus */
	uint8_t inInterrupt;				/* 0x0be: interruptActive */
	uint8_t suspended;				/* 0x0bf: stopping */
	uint8_t nextQueueIndex;				/* 0x0c0: nextQueue */
	uint8_t pendingMultiCount;			/* 0x0c1: pendingCompletions */
	uint8_t notifFlags[31];				/* 0x0c2: notificationState[29] + pendingCommand130 + pendingMultiCnInfo */
	uint8_t reservedE1[3];				/* 0x0e1..0x0e3 */
	SysconCallbackEntry callbacks[39];		/* 0x0e4..0x21b */
	uint8_t reserved21C[8];				/* 0x21c..0x223 */
	uint32_t baryonVersion;				/* 0x224 */
	uint32_t hardwareInfo;				/* 0x228 */
	uint8_t hardwareInfoEx[16];			/* 0x22c: hardwareFlags[4] */
	uint8_t buildVersion[16];			/* 0x23c: baryonTimestampText[16] */
	uint32_t reserved24C;				/* 0x24c */
	uint32_t baryonTimestamp[2];			/* 0x250 */
	uint32_t allowQueuedFlag1;			/* 0x258: polling */
	uint32_t allowQueuedFlag2;			/* 0x25c: affirmativeRetryMode */
	uint32_t reserved260[2];			/* 0x260..0x267 */
} SysconDriverState;

_Static_assert(offsetof(SysconDriverState, notifFlags) == 0xc2, "SysconDriverState notifFlags");
_Static_assert(offsetof(SysconDriverState, callbacks) == 0xe4, "SysconDriverState callbacks");
_Static_assert(offsetof(SysconDriverState, baryonVersion) == 0x224, "SysconDriverState baryonVersion");
_Static_assert(offsetof(SysconDriverState, allowQueuedFlag1) == 0x258, "SysconDriverState allowQueuedFlag1");
_Static_assert(sizeof(SysconDriverState) == 0x268, "SysconDriverState size");

/* ASN.1 DER element & OID descriptor structures (rsa_srvk.c; names ours) */
typedef struct SceAsn1Element {
	uint8_t tag;				/* 0x00 */
	uint8_t reserved[3];			/* 0x01 */
	uint32_t totalLen;			/* 0x04 */
	uint32_t contentLen;			/* 0x08 */
	const uint8_t *data;			/* 0x0c */
} SceAsn1Element;

_Static_assert(sizeof(SceAsn1Element) == 0x10, "SceAsn1Element");

typedef struct SceAsn1DigestOid {
	const uint8_t *oid;			/* 0x00 */
	uint32_t oidLen;			/* 0x04 */
	uint32_t hashLen;			/* 0x08 */
} SceAsn1DigestOid;

_Static_assert(sizeof(SceAsn1DigestOid) == 0x0c, "SceAsn1DigestOid");

/* RSA public key descriptor passed to PKCS#1 v1.5 / Bignum verifier (size 0x10; name ours) */
typedef struct RsaPublicKeyParam {
	const uint32_t *modulus;		/* 0x00: 64 words (2048 bits) */
	uint32_t modulusWordCount;		/* 0x04: 64 */
	const uint32_t *exponent;		/* 0x08: e.g. 0x10001 */
	uint32_t exponentWordCount;		/* 0x0c: 1 */
} RsaPublicKeyParam;

typedef RsaPublicKeyParam SceBignumRsaKey;
typedef RsaPublicKeyParam SceRsaPublicKey;
#define modWordLen	modulusWordCount
#define expWordLen	exponentWordCount
#define modulus_words	modulusWordCount
#define exponent_words	exponentWordCount

_Static_assert(sizeof(RsaPublicKeyParam) == 0x10, "RsaPublicKeyParam");

/* Partition storage operations vtable at 0x80c6a0 (size 0x20; name ours) */
typedef struct PartitionStorageOps {
	int (*open)(uint32_t devIndex, const char *invFileName, int flags);
	int (*close)(int handle);
	int64_t (*lseek)(int handle, int64_t offset, int whence);
	int (*read)(int handle, void *buf, uint32_t size);
	int (*getFileSize)(int handle, void *outStat);
	uint32_t reserved[3];
} PartitionStorageOps;

_Static_assert(sizeof(PartitionStorageOps) == 0x20, "PartitionStorageOps");

/* SELF authentication & loading contexts (loader.c / self_auth.c; names ours) */
typedef Elf32_Ehdr SceElf32Ehdr;
typedef Elf32_Phdr SceElf32Phdr;

typedef struct SceSelfSegmentInfo {
	uint64_t offset;			/* 0x00 */
	uint64_t length;			/* 0x08 */
	uint32_t compression;			/* 0x10 */
	uint32_t encryption;			/* 0x14 */
	uint64_t reserved;			/* 0x18 */
} SceSelfSegmentInfo;

_Static_assert(sizeof(SceSelfSegmentInfo) == 0x20, "SceSelfSegmentInfo");

typedef struct SelfAuthContext {
	void *scratchBuf;			/* 0x00: e.g. 0x81e800 */
	uint32_t scratchSize;			/* 0x04: e.g. 0x1000 */
} SelfAuthContext;

typedef struct SelfHeaderLoadContext {
	const PartitionStorageOps *ops;		/* 0x00 */
	int devHandle;				/* 0x04 */
	uintptr_t bounceBufAddr;		/* 0x08: 0x1f840000 */
	uint32_t bounceBufSize;			/* 0x0c: 0x1000 */
} SelfHeaderLoadContext;

typedef struct SelfSegmentLoadContext {
	const PartitionStorageOps *ops;		/* 0x00 */
	int devHandle;				/* 0x04 */
	uintptr_t segStartAddr;			/* 0x08 */
	uintptr_t segEndAddr;			/* 0x0c */
	uint64_t segFileOffset;			/* 0x10 */
	uint64_t segFileSize;			/* 0x18 */
} SelfSegmentLoadContext;

typedef int (*SelfHeaderReadCallback)(uintptr_t dstBuf, const uint64_t *fileOffset,
		uint32_t totalSize, uint32_t *outBytesRead, void *arg);
typedef int (*SelfSegmentReadCallback)(const uint64_t *fileOffset, uint32_t readSize,
		uint64_t *outSrcAddr, uint64_t *outDstAddr, uint32_t *outBytesRead,
		uint32_t *outFlags, void *arg);
typedef int (*SelfSegmentDoneCallback)(const uint64_t *fileOffset, uint32_t reqSize,
		const uint64_t *srcAddr, const uint64_t *dstAddr, uint32_t bytesRead,
		uint32_t flags, void *arg);

/* Ernie (Syscon) challenge-response handshake packet (40 bytes) */
typedef struct HandshakePacket {
	uint8_t subcmd;				/* 0x00: 0x30 */
	uint8_t step;				/* 0x01: 0..5 */
	uint8_t status;				/* 0x02 */
	uint8_t keySlot;			/* 0x03: 0x0b or 0x0f */
	uint8_t revision[4];			/* 0x04 */
	uint8_t payload[32];			/* 0x08 */
} HandshakePacket;

_Static_assert(sizeof(HandshakePacket) == 40, "HandshakePacket");

/* ============================================================================
 * Cross-Unit Function Declarations & Globals
 * ============================================================================ */

/* start.s (0x800000..0x8003df) */
extern uint32_t __stack_chk_guard;
extern int g_bootExitStatus;
extern uint32_t g_isColdBoot;
void __stack_chk_fail(void);
uint32_t maskBit0(uint32_t val);
#define zeroExtendByte	maskBit0

/* boot.c (0x8003e0..0x801161) */
extern uint32_t g_bootldrRevision;
void cmepSyncm(void);
void earlyInitHook(void);
void noopHook(void);
void keyringReadSlot(unsigned int slot, uint32_t *dst);
int secondLoaderMain(void);
void secondLoaderPrintBannerOnce(void);
int isWarmBootMagicValid(void);
int copyWarmBootVector(void);
void *slMemset(void *dst, int c, size_t n);
int isIdStorageInitialized(void);
int runErnieHandshake0BAndLoadIds(void);
void printSdkBanner(void);
void printBlRevisionBanner(void);
void printExtraBannerNoop(void);
uint32_t getPervasiveSocRevision(void);
int verifySoCHardwareState(uint32_t sbootReg4, uint32_t crySboot, uint32_t secReg2020);
int verifySnvsOsVersion(uint32_t kblMinRequiredVer, uint32_t isWarmBoot);
int enableSd0Power(void);
int readSd0SectorsStub(void *dst, uint32_t sectorCount, uint32_t startSector);
int getZeroStatusStub(void);
int isSd0PowerEnabled(void);
int initEmmcStorage(void);
int readEmmcSectors(void *dst, uint32_t size, uint32_t startSector);
void lockBootKeyringRanges(void);
void setArmResetHold(int releaseArm);

#define memset			slMemset
#define memset32		slMemset
#define readBootStorageSectors0	readSd0SectorsStub
#define readBootStorageSectors1	readEmmcSectors

/* loader.c (0x801162..0x801b37) */
extern PartitionStorageOps g_partitionStorageOps;
int loadKernelBootLoaderSelf(uint32_t *outMinRequiredVer);
int isKernelBootLoaderDebugMode(void);
int validateSelfLoadRange(uintptr_t startAddr, uintptr_t endAddr);
int loadAndAuthenticateSelfFromStorage(const PartitionStorageOps *ops, uint32_t devIndex,
		const uint8_t *invFileName, uint32_t *outMinRequiredVer);
int asyncLoadBootModulesCallback(void);
void initBootModulesTable(void);
uint32_t getBootModuleLoadAddr(unsigned int index);
uint32_t getBootModuleLoadSize(unsigned int index);
const uint8_t *getBootModuleFileName(unsigned int index);
void setBootModuleLoadAddr(unsigned int index, uint32_t addr);
void setBootModuleLoadSize(unsigned int index, uint32_t size);
void setBootModuleFileName(unsigned int index, const uint8_t *invFileName);
uint32_t getBootModulesFlags(void);
int loadRawBootModuleFromStorage(const PartitionStorageOps *ops, uint32_t devIndex,
		const uint8_t *invFileName, uint32_t loadAddr, uint32_t maxLen,
		uint32_t *outSize);
int verifyStepFHook(const uint8_t *qaFlags);
int isSpecialSelfAuthAllowed(void);
int isBootFlagBit0Enabled(void);
int isBootFlagBit16Enabled(void);
int isBootFlagBit3Enabled(void);
void clearCmepSboot(void);
int verifyStepGHook(void);
int isBootFlagBit3KblEnabled(void);
uint32_t getTargetArmClockStep(void);
uint32_t getTargetCmepClockStep(void);
uint32_t getTargetBootModeFlag(void);

/* kbl_param.c (0x801b38..0x802043) */
int readIdStoragePsCodeRaw(void *dst16);
uint16_t formatPsCodeFromLeaf(const uint8_t *raw16, ScePsCode *outPsCode);
int readIdStorageOpenPsId(void *dst16);
int readEmmcMbrTotalSectors(uint32_t *outSectors);
uint32_t readBigmacRngWord(void);
int clearKblParam(void);
int restoreWarmBootKblParam(void);
int populateKblParam(const uint32_t *dipsw, const uint8_t *qaFlags,
		uint32_t wakeupFactor, int isWarmBoot, uint32_t minRequiredFwVer);
void populateSysconKblFields(void);

/* qaf.c (0x802044..0x8023a1) */
void enableDevkitFeatureFlag(void);
uint32_t isDevkitFeatureEnabled(void);
int readDipswFromScratchpad(uint32_t *outDipsw8);
int checkDipswBit(const uint32_t *dipswWords, unsigned int bit);
int initQafSeedsInKeyring(void);
int verifyQafToken(uint8_t *outQaFlags16);
int checkBitFlagInArray(const uint8_t *array, int byteOffset, uint8_t mask);
void clearBitFlagInArray(uint8_t *array, int byteOffset, uint8_t mask);
void clearBootFlagsState(void);
int loadBootFlagsFromNvs(void);
int loadHardwareFlagsFromNvs(void);
int getHwFlagByte4(void);
int isQafEnabledInNvs(void);
int getNvsBootFlagByte1(void);
void setNvsBootFlagByte1(uint8_t val);
int getNvsBootFlagByte3(void);
int getNvsBootFlagByte7(void);
int getNvsBootFlagByte6(void);

#define qafGetCachedLogEnableFlag	getNvsBootFlagByte1
#define qafSetCachedLogEnableFlag	setNvsBootFlagByte1

/* keyring.c (0x8023a2..0x8026a7) */
int initSnvsAndKeyringState(int tryResumeContext);
int isSnvsSd0FlagEnabled(void);
int isSnvsBit1FlagEnabled(void);
int isSnvsInitialized(void);
void writeKeyringSlotWords(uint32_t slot, const uint32_t *words);
int writeKeyringSlotBytes(uint32_t slot, const void *data, uint32_t size);
void initKeyringBaseSeeds(void);
void readKeyringSlotWords(unsigned int slot, uint32_t *dst);
int decryptSeedWithKeyslot214(void *dst32, const void *src32, const void *iv16);
void keyringNoop1(void);
void keyringNoop2(void);

/* resume.c (0x8026a8..0x8027df) */
int verifyAndDecryptResumeContext(void *unusedCtx, const void *srcPa,
		const void *cipherKey, uint32_t cipherKeyLen,
		const void *cmacKey, uint32_t cmacKeyLen);
int loadAndSetupResumeOrColdBootKeys(void *ctx, int tryResume);
int noopResumeHook(void);

/* snvs.c (0x8027e0..0x80363b) */
int initSnvsBaseKeyringSlots(void);
int runErnieHandshake0F(void);
int initSnvsHandshakeOrFallbackKeys(uint32_t resumeValid);
int readSnvsManagementSector(void *outFlags, void *outStatus);
int readSnvsSectorData(int sectorIndex, void *outPlaintext, void *rawSectorScratch, int sectorCount);
uint32_t getSnvsCryptoInitialized(void);
SnvsXtsCmacEngine *getSnvsCryptoEngine(void);
const uint8_t *getSnvsZeroIv(void);
void SnvsCrypto_destroy(SnvsXtsCmacEngine *eng);
void SnvsCrypto_deleteObj(SnvsXtsCmacEngine *eng);
void SnvsCrypto_initBase(SnvsXtsCmacEngine *eng);
void SnvsCrypto_init(SnvsXtsCmacEngine *eng);
void SnvsCrypto_resetVtable(SnvsXtsCmacEngine *eng);
void snvsNoop(void);

/* syscon.c (0x80363c..0x804beb) */
extern SysconDriverState g_sysconCtrl;
extern int g_sysconAsyncCallbackStatus;
uint32_t sysconReadPervasiveRevision(void);
int isSysconInitialized(void);
int sceSysconAsyncPollInit(void);
uint32_t sceSysconGetBaryonVersion(void);
uint32_t sceSysconGetHardwareInfo(void);
int sceSysconGetHardwareInfoEx(void *outBuf16);
const uint8_t *sceSysconGetBuildVersion(void);
int sceSysconCmdExecAsync(SceSysconPacket *pkt, uint32_t flags,
		int (*callback)(SceSysconPacket *, void *), void *argp);
int sceSysconCmdWait(SceSysconPacket *pkt, int noWait);
int sceSysconCmdSync(SceSysconPacket *pkt, uint32_t flags);
uint8_t sceSysconGetBaryonTimestamp(void);
uint8_t sceSysconGetBaryonModeBit0(void);
int sceSysconGetSnvsVersionInfo(uint32_t *outVer);
int sceSysconNvsReadData(uint32_t offset, void *dst, uint32_t size);
int sceSysconQueryBaryonVersion(uint32_t *outVersion);
int sceSysconQueryBuildVersion(uint8_t *outBuildVer13);
int sceSysconSetAlertLevel(uint32_t level);
int sceSysconQueryHardwareInfo(uint32_t *outHwInfo);
int sceSysconReadBootSummary(void *outSummary16);
int sceSysconGetWakeupFactor(uint32_t *outFactor);
int sceSysconReadScratchpad(uint16_t offset, void *dst, uint32_t size);
int sceSysconSetLifecycleMode(uint32_t mode);
int sceSysconSetPowerMode(uint8_t mode, uint32_t mask);
int sceSysconSendHandshakePacket(const void *txBuf40, uint32_t txSize);
int sceSysconReceiveHandshakePacket(void *rxBuf40, uint32_t rxSize);
int sceSysconStartAsyncBootCallback(int (*callback)(void));
int sceSysconPollAsyncCallbacks(void);
int sceSysconWaitAsyncBootCallback(int *outStatus);
int sceSysconResetPollTimer(void);
int sysconWaitStatusBit(uint8_t statusMask, int maxTimeoutMs);
int sceSysconExecTwoPhaseCmd(SceSysconPacket *pkt, uint32_t flags);
int sceSysconGetPoweroffFactor(uint32_t *outFactor);
int sceSysconCtrlSdPower(uint32_t enable);
int sceSysconSetRailVoltage(uint8_t railId, uint32_t vidCode);
int sceSysconGetUsbDetStatus(uint32_t *outStatus);
int sceSysconCtrlDevicePower(uint32_t param);
int sceSysconGetPowerInfo(uint32_t *outPowerInfo);
int sceSysconExchangeHandshakePacket(const void *txBuf40, uint8_t txSize,
		void *rxBuf40, uint8_t rxSize);
int sceSysconSnvsExchangePacket(const void *txBuf16, uint8_t txSize,
		void *rxBuf48, uint8_t rxSize);
int sceSysconSendShortCommand(uint32_t payload, uint16_t cmdCode, uint8_t payloadLen);

#define sysconGetHardwareVersion	sceSysconGetBaryonVersion
#define sysconEnableUartPower		sceSysconSetLifecycleMode
#define sysconControlPowerSwitch	sceSysconCtrlSdPower
#define sysconSetPowerRailVoltage	sceSysconSetRailVoltage

/* bigmac.c (0x804bec..0x805167) */
int bigmacInitContext(BigmacContext *ctx);
int bigmacGetChannelStatus(const BigmacContext *ctx, uint32_t *outStatus);
int bigmacWaitChannel(const BigmacContext *ctx);
int bigmacSetChannelIndex(BigmacContext *ctx, uint32_t useExternalKey);
int bigmacAes128EcbEncrypt(void *dst, const void *src, uint32_t size,
		const void *key, BigmacContext *ctx);
int bigmacAes128EcbDecrypt(void *dst, const void *src, uint32_t size,
		const void *key, BigmacContext *ctx);
int bigmacAes256EcbEncrypt(void *dst, const void *src, uint32_t size,
		const void *key, BigmacContext *ctx);
int bigmacAes256EcbDecrypt(void *dst, const void *src, uint32_t size,
		const void *key, BigmacContext *ctx);
int bigmacAes256CbcEncrypt(void *dst, const void *src, uint32_t size,
		const void *key, void *iv, BigmacContext *ctx);
int bigmacAes256CbcDecrypt(void *dst, const void *src, uint32_t size,
		const void *key, void *iv, BigmacContext *ctx);
int bigmacAes128CbcDecryptKeyslot(void *dst, const void *src, uint32_t size,
		const void *keyOrSlot, void *iv, BigmacContext *ctx);
int bigmacAes256CbcDecryptAsync(void *dst, const void *src, uint32_t size,
		const void *key, void *iv, BigmacContext *ctx);
int bigmacAes128CbcEncryptSync(void *dst, const void *src, uint32_t size,
		const void *key, void *iv);
int bigmacAes256CbcDecryptSync(void *dst, const void *src, uint32_t size,
		const void *key, void *iv);
int bigmacAesCmacWithKey(void *dstMac16, const void *src, uint32_t size,
		const void *keyOrSlot, uint32_t keyBytes, BigmacContext *ctx);
int bigmacMemsetAsync(void *dst, uint32_t size, int fillByte, BigmacContext *ctx);
int bigmacMemcpy(void *dst, const void *src, uint32_t size);
int bigmacMemset(void *dst, uint32_t size, int fillByte);
int bigmacStartAesCipher(void *dst, const void *src, uint32_t size,
		const void *keyOrSlot, void *iv, uint32_t dirMode,
		uint32_t chainMode, uint32_t keySizeIdx, BigmacContext *ctx);
int bigmacMemcpyAsync(void *dst, const void *src, uint32_t size, BigmacContext *ctx);

#define bigmacInit			bigmacInitContext
#define bigmacWait			bigmacWaitChannel
#define bigmacSetChannel		bigmacSetChannelIndex
#define bigmacAesCmac			bigmacAesCmacWithKey
#define bigmacAes256CbcDecryptKeyslot	bigmacAes128CbcDecryptKeyslot
#define bigmacAes128CbcEncrypt		bigmacAes128CbcEncryptSync
#define bigmacAes128CbcDecrypt		bigmacAes256CbcDecryptSync

/* timer.c (0x805168..0x805303) */
uint32_t cmepGetClockFrequencyHz(void);
void cmepDelayTicks(uint32_t ticks);
void cmepDelayUs(uint32_t microseconds);
uint16_t htole16Sl(uint16_t val);
uint16_t le16tohSl(uint16_t val);
uint32_t htole32Sl(uint32_t val);
uint32_t le32tohSl(uint32_t val);
uint64_t htole64Sl(uint64_t val);
void writeLe64(uint64_t *out, const uint64_t *in);
int16_t bswap16(uint16_t val);
int16_t htobe16Sl(uint16_t val);
int16_t be16tohSl(uint16_t val);
uint32_t bswap32(uint32_t val);
uint32_t htobe32Sl(uint32_t val);
uint32_t be32tohSl(uint32_t val);
uint64_t bswap64(uint64_t val);
uint64_t htobe64Sl(uint64_t val);
uint64_t be64tohSl(uint64_t val);
int timerNoopHook(void);

/* rsa_srvk.c (0x805304..0x805ad7) */
int qafDecryptTokenBody(const void *src, void *dst, const void *key, const void *iv);
int qafVerifyTokenCmac(void *tokenBuf, const void *key);
int qafVerifyTokenRsa2048(const void *tokenBuf, const void *sig, const SceBignumRsaKey *pubKey);
int qafVerifyTokenChallengeCmac(const void *challengeBuf, const void *key);
int qafCheckTokenByte5EqualsByte4(const void *tokenBuf, const void *refBuf);
int qafVerifyTokenOpenPsId(const void *tokenBuf, const void *openPsId);
int bigmacSha256(void *dstDigest, const void *src, uint32_t size);
int verifyRsaPkcs1v15Blocking(void *workBuf, const void *sig, const SceBignumRsaKey *key,
		const void *hash, uint32_t hashAlgo);
int bigmacStartHash(void *dstDigest, const void *src, uint32_t size, uint32_t algo,
		SceBigmacContext *ctx);
int bignumCopyWordsReverseBswap(volatile uint32_t *dst, const uint32_t *src, uint32_t wordCount);
int bignumInitState(uint32_t *state);
int bignumModExpStep(void *dst, const void *base, const SceBignumRsaKey *key, uint32_t *state);
int bignumWaitInterrupt(void);
int verifyRsaPkcs1v15Step(uint8_t *workBuf, const void *sig, const SceBignumRsaKey *key,
		const void *hash, uint32_t hashAlgo, uint32_t *state);
int asn1ParseElement(const uint8_t *buf, uint32_t bufSize, SceAsn1Element *outElem);
int asn1ParseSequence(const uint8_t *buf, uint32_t bufSize, const SceAsn1Element *seqElem,
		SceAsn1Element *outChildren, uint32_t *inOutCount);

#define qafVerifyRsaSignature		qafVerifyTokenRsa2048
#define qafDecryptToken			qafDecryptTokenBody
#define qafCheckPsCodeAndOpenPsId	qafVerifyTokenOpenPsId
#define qafVerifySnvsSector21Cmac	qafVerifyTokenChallengeCmac
#define qafCompareTokenDigest		qafCheckTokenByte5EqualsByte4

/* partition.c (0x805ad8..0x806063) */
int slb2InitDevice(u32 devId);
int slb2LookupFile(u32 devId, const char *invFileName, u32 *outStartSector, u32 *outFileSize);
int slb2IsPrimaryBootSlot(u32 devId, u32 *outIsPrimary);
int slb2GetActiveOs0PartitionId(u32 devId, u32 *outPartitionId);
u32 partitionStorageIsOpen(int fd);
int partitionStorageOpen(u32 devId, const char *invFileName, int flags);
int partitionStorageClose(int fd);
s64 partitionStorageLseek(int fd, s64 offset, int whence);
int partitionStorageRead(int fd, void *buf, u32 size);
int partitionStorageGetStat(int fd, void *outStatBuf);

#define partitionStorageGetFileSize	partitionStorageGetStat
#define readEmmcPartitionActiveBank	slb2IsPrimaryBootSlot
#define readEmmcPartitionFactoryIndex	slb2GetActiveOs0PartitionId

/* slb2.c (0x806064..0x806b77) */
int readIdStorageLeaf44(u8 outSize, void *outBuf, void *sectorBuf, u32 sectorBufSize);
int readIdStorageLeaves46And47(u8 outSize, void *outBuf, void *sectorBuf, u32 sectorBufSize);
u32 idStorageInitKeyslot204(void);
u32 idStorageComputeLeafCmac(void *dst, const void *src, u32 size);
u32 idStorageDecryptLeafMac(void *dst, const void *src, u32 size);
int idStorageVerifyLeafSection(const void *section, u32 size);
u32 idStorageExtractKeywordFlag(u32 val);
int idStorageDecryptKeyWithSlot213(void *dstKey, const void *srcKey, const void *iv);
int idStorageAes256CbcDecrypt(void *dst, const void *src, u32 size, const void *key, const void *iv);
int idStorageVerifyAndDecryptSmiLeaf(u8 *leaf);
int readIdStorageSmiLeaf80(void *outBuf, u32 outSize, void *sectorBuf, u32 sectorBufSize);
int idStorageSetSectorBuf(void *buf, u32 size);

#define readIdStoragePscodeFromPartition	readIdStorageLeaf44
#define readIdStorageOpenPsIdFromPartition	readIdStorageLeaves46And47
#define bswap32Mbr				idStorageExtractKeywordFlag
#define blockdevSetSectorBuffer			idStorageSetSectorBuf

/* blockdev.c (0x806b78..0x8070e9) */
int blockdevFindFreeFd(void);
int blockdevAllocFd(void);
int blockdevOpen(const char *path, u32 flags);
int blockdevFreeFd(int fd);
u32 blockdevIsOpen(int fd);
int blockdevClose(int fd);
s64 blockdevLseek(int fd, s64 offset, int whence);
int blockdevRead(int fd, void *buf, u32 size);

/* sdif_emmc.c (0x8070ea..0x80841b) */
int sdifSetAsyncCallback(int (*cb)(void));
int sdifPopAsyncResult(u32 *outResult);
void sdifEnableAdmaMode(void *admaBuf, u32 admaBufSize);
int sdifReadSectorsAligned(void *dst, u32 size, u32 startSector);
u32 sdifGetTotalSectors(void);
void sdifPervasiveReset(void);
void sdifInitInlineCryptoEngine(void);
int sdifInitEmmc(int isWarmBoot);
int sdifDeinitEmmc(int powerOff);
void sdifSetAdmaBuffer(void *admaBuf, u32 admaBufSize);
void sdifDelayUs(u32 us);
void sdifDelay1Ms(void);

struct SceSdifHostContext;
struct SceSdifCommand;
struct SceSdifHostContext *sdifGetHostContext(int hostId);
int sdifIsCardInserted(struct SceSdifHostContext *host);
int sdifSupplyClock(struct SceSdifHostContext *host);
int sdifGoIdleState(struct SceSdifHostContext *host);
int sdifSetBusClock(struct SceSdifHostContext *host, u32 targetClockHz, int highSpeed);
int sdifSetBusWidth(struct SceSdifHostContext *host, int busWidth);
int sdifExecuteCommand(struct SceSdifHostContext *host, struct SceSdifCommand *cmd, int retries);
int sdifExecuteCmdNoRetry(struct SceSdifHostContext *host, struct SceSdifCommand *cmd, int unusedArg);
int sdifExecuteCmdRetry3(struct SceSdifHostContext *host, struct SceSdifCommand *cmd);
int sdifCmd2AllSendCid(struct SceSdifHostContext *host, void *outCid);
int sdifCmd9SendCsd(struct SceSdifHostContext *host, void *outCsd);
int sdifCmd7SelectCard(struct SceSdifHostContext *host, int select);
int sdifCmd12StopTransmission(struct SceSdifHostContext *host, int busyWait);
u32 sdifCheckCardStatusErrors(u32 cardStatus);
int sdifCmd13SendStatus(struct SceSdifHostContext *host, u32 *outStatus);
int sdifCmd16SetBlockLen(struct SceSdifHostContext *host, u32 blockLen);
int sdifCmd23SetBlockCount(struct SceSdifHostContext *host, u32 blockCount);
int sdifBuildAdmaDescriptors(struct SceSdifCommand *cmd);
int sdifStartAdmaTransfer(struct SceSdifHostContext *host, struct SceSdifCommand *cmd, u16 cmdRegFlags);

#define sdifEmmcSetAsyncCallback	sdifSetAsyncCallback
#define sdifEmmcGetAsyncCallbackStatus(p) sdifPopAsyncResult((u32 *)(p))
#define sdifInitEmmcCard		sdifInitEmmc
#define sdifSetAdmaDescriptorBuffer	sdifEnableAdmaMode
#define sdifInitController		sdifDeinitEmmc

/* ddrc.c (0x80841c..0x808789) */
void operatorDeleteNoop(void *ptr);
void ddrcNoop(void);
u32 ddrcGetSocRevision(void);
int ddrcPervasiveInit(u32 clkMode);
int ddrcInit(u32 clkMode, int isWarmBoot);
u32 ddrcGetTotalSize(void);
int ddrcGetVendorId(void);
int ddrcGetDensityInfo(void);

#define ddrcInitLpddr2(clkMode, warm)	ddrcInit((u32)(clkMode), (warm))
#define ddrcGetTotalDramSize		ddrcGetTotalSize
#define ddrcGetChannel0Error()		((uint32_t)ddrcGetVendorId())
#define ddrcGetChannel1Error()		((uint32_t)ddrcGetDensityInfo())

/* pervasive_clk.c (0x80878a..0x808a43) */
int pervasiveConfigureArmAndCmepClocks(u32 armClkRatio, int enableDspClk, u32 cmepClkRatio);
void pervasiveSetBusClock(u32 busClkRatio);
void pervasiveEnableSysClock(void);
int setArmCoreVoltageForClockStep(u32 clockStep);
int setSocVoltageForClockStep(u32 clockStep);

#define ddrcPostInitConfigure(arm, dsp, cmep) \
	pervasiveConfigureArmAndCmepClocks((u32)(arm), (int)(dsp), (u32)(cmep))
#define pervasiveScaleArmVoltage	setArmCoreVoltageForClockStep
#define pervasiveScaleBusVoltage	setSocVoltageForClockStep

/* uart_debug.c (0x808a44..0x808e3d) */
void uartPortPutc(u32 port, u8 ch);
int uartDebugPutchar(int ch);
void uartEnablePort(u32 port);
void uartDisablePort(u32 port);
void debugEnablePostLeds(void);
u32 debugArePostLedsEnabled(void);
void debugInitPostLedGpios(void);
void debugWritePostLedCode(u8 code);
void uartDebugSetFlags(u32 flags);
int uartDebugInit(void);
void uartDebugReportEvent(u32 category, u32 subcode, u32 errCode, u32 extra);
void uartDebugSetRadix(u32 *stream, u32 radix);
void uartDebugPrintU32(u32 val);
void uartDebugWrite(const char *buf, u32 len);
void uartDebugPutc(char ch);
void uartDebugPuts(const char *str);
void uartDebugFormatUint(u32 val, u32 radix, int width);

#define setDebugUartEnabledFlag		uartEnablePort
#define setDebugUartDisabled		uartDisablePort
#define enableGpioDebugLeds		debugEnablePostLeds
#define isGpioDebugLedsEnabled		debugArePostLedsEnabled
#define initGpioDebugLeds		debugInitPostLedGpios
#define setGpioDebugLedPostcode		debugWritePostLedCode
#define setDebugLogFlags		uartDebugSetFlags
#define initDebugUartAndClock		uartDebugInit
#define debugLogError(cat, code, v1, v2) uartDebugReportEvent((u32)(cat), (u32)(code), (u32)(v1), (u32)(v2))
#define debugSetNumberBase(ctx, base)	uartDebugSetRadix((u32 *)(ctx), (u32)(base))
#define debugPrintNumber		uartDebugPrintU32
#define debugPrintString		uartDebugPuts

/* handshake.c (0x808e3e..0x809421) */
int constantTimeMemcmp(const void *s1, const void *s2, u32 len);
int handshakeMapKeySlot(u32 keySlot, int revision, u32 *outMappedSlot);
int handshakeBuildStartPacket(u8 *pkt, u32 keySlot);
int handshakeBuildChallengeResponse(u8 *outPkt, const u8 *inPkt, u32 keySlot, u32 revision, const void *hostRandom);
int handshakeVerifyAndDeriveSessionKey(const u8 *inPkt, u32 keySlot, u32 revision,
		const void *sysconChallenge, const void *hostRandom, void *outSessionKey);
int handshakeWipeKeySlot(u32 keySlot, int revision);
int handshakeAes128CbcEncrypt(void *dst, const void *src, u32 size, const void *key, const void *iv);
int handshakeAes128CbcDecrypt(void *dst, const void *src, u32 size, const void *key, const void *iv);
int bigmacRngReadBytes(void *dst, u32 size);
int handshake0fBuildAuthPacket(u8 *outPkt, int revision, const void *sessionKey);
int handshake0fVerifyResponsePacket(const u8 *inPkt, int revision, const void *sessionKey);
int handshakeCheckKeySlotValid(u32 keySlot);

#define bigmacGenerateRandom		bigmacRngReadBytes
#define ernieHandshakeBuildStartPacket(pkt, mode) \
	handshakeBuildStartPacket((u8 *)(pkt), (u32)(mode))
#define ernieHandshakeProcessChallenge(out, in, mode, rev, rnd) \
	handshakeBuildChallengeResponse((u8 *)(out), (const u8 *)(in), (u32)(mode), (u32)(rev), (rnd))
#define ernieHandshakeVerifyAndFinish(in, mode, rev, chal, rnd, key) \
	handshakeVerifyAndDeriveSessionKey((const u8 *)(in), (u32)(mode), (u32)(rev), (chal), (rnd), (key))
#define ernieHandshakeClearKeyMaterial(mode, rev) \
	handshakeWipeKeySlot((u32)(mode), (int)(rev))
#define ernieHandshakeBuildFinishPacket(pkt, rev, key) \
	handshake0fBuildAuthPacket((u8 *)(pkt), (int)(rev), (key))
#define ernieHandshakeVerifyFinalAck(pkt, rev, key) \
	handshake0fVerifyResponsePacket((const u8 *)(pkt), (int)(rev), (key))

/* gpio.c (0x809422..0x8097ff) */
int gpioInitPorts(void);
int gpioPortSetPin(u32 port, u32 pin);
int gpioPortClearPin(u32 port, u32 pin);
u32 gpioSetPortDirection(u32 port, u32 pin, u32 isInput);
int gpioSetInterruptMode(u32 port, u32 pin, u32 mode);
u32 gpioReadInterruptStatus(u32 port, u32 pin);
u32 gpioAckInterruptStatus(u32 port, u32 pin);
int gpioEnablePinInterrupt(u32 port, u32 pin);

#define sceGpioInit		gpioInitPorts
#define sceGpioPortSet		gpioPortSetPin
#define sceGpioPortClear	gpioPortClearPin
#define sceGpioSetPortMode	gpioSetPortDirection
#define sceGpioSetIntrMode	gpioSetInterruptMode
#define sceGpioQueryIntr	gpioReadInterruptStatus
#define sceGpioAcquireIntr	gpioAckInterruptStatus
#define sceGpioEnableIntr	gpioEnablePinInterrupt

/* self_auth.c (0x809800..0x80b257) */
int selfVerifySvnAndDecryptMetadata(const SelfAuthContext *ctx);
int selfGetAppInfo(const SelfAuthContext *ctx, const uint8_t **outAppInfo);
int selfGetElfHeader(const SelfAuthContext *ctx, const SceElf32Ehdr **outEhdr);
int selfGetProgramHeaders(const SelfAuthContext *ctx, const SceElf32Phdr **outPhdrs);
int selfGetSegmentInfos(const SelfAuthContext *ctx, const SceSelfSegmentInfo **outSegInfos);
int selfFindExtHeader(const SelfAuthContext *ctx, const uint8_t **outExtHdr, uint32_t targetType);
int selfInitAndVerifyHeader(const SelfAuthContext *ctx, const void *metaKey,
		const void *rootPubKey, uint32_t sigType, int allowDevelopment,
		uint16_t expectedCategory, SelfHeaderReadCallback readCb,
		void *readCbArg, int32_t expectedExtHeaderSize,
		const uint64_t *expectedHeaderLen);
int selfDecryptAndLoadSegment(const SelfAuthContext *ctx, uint32_t segType,
		uint32_t segIndex, SelfSegmentReadCallback readCb, void *readCbArg,
		SelfSegmentDoneCallback doneCb, void *doneCbArg);
int cfBigmacSha256Start(void *dstDigest, const void *src, uint32_t size, BigmacContext *ctx);

#define selfBigmacSha256Start	cfBigmacSha256Start

/* string.c (0x80b258..0x80b32b) */
int slStrncmp(const char *s1, const char *s2, size_t n);
size_t slStrnlen(const char *s, size_t maxlen);
void *memcpy(void *dst, const void *src, size_t n);
void *slMemcpy(void *dst, const void *src, size_t n);
void *slMemmove(void *dst, const void *src, size_t n);
int slTimingsafeBcmp(const void *b1, const void *b2, size_t n);

#define strncmp		slStrncmp
#define strnlen		slStrnlen
#define memmove		slMemmove
#define memcmp		slTimingsafeBcmp

#endif /* SECOND_LOADER_H */
