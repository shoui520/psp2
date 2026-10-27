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

#endif /* SM_COMMON_H */
