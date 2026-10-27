/*
 * compat_sm.h: Internal types and cross-unit declarations for PlayStation Vita
 * ePSP compatibility secure module (os0:sm/compat_sm.self).
 *
 * Internal function, structure, and constant names (except sm_main) are ours.
 */

#ifndef COMPAT_SM_H
#define COMPAT_SM_H

#include "sm_common.h"

#define SCE_SBL_ERROR_COMPAT_AUTH_FAILED	((int)0x800f0f01u)
#define SCE_SBL_ERROR_COMPAT_IO_ERROR		((int)0x800f0f05u)
#define SCE_SBL_ERROR_COMPAT_LOAD_FAILED	((int)0x800f0f0cu)
#define SCE_SBL_ERROR_COMPAT_INVALID_ARG	((int)0x800f0f16u)
#define SCE_SBL_ERROR_COMPAT_BAD_CMD		((int)0x800f0f2au)

#define COMPAT_CMD_LOAD_BOOTROM			0x00010006u
#define COMPAT_CMD_VERIFY_CHALLENGE		0x00030006u

#define KERMIT_RESET_REG_ADDR			0xe8000004u
#define KERMIT_BOOT_SRAM_BASE			0xe8100000u
#define KERMIT_BOOT_SRAM_SIZE			0x00001000u
#define KERMIT_BOOT_PARAM_ADDR			0xe8100fbcu
#define KERMIT_BOOT_CHALLENGE_ADDR		0xe8100fc0u

/* Mailbox communication packet at 0x80d280 (size 0x48) */
typedef struct SceCompatCommPacket {
	uint32_t size;				/* 0x00: 0x48 */
	uint32_t cmdId;				/* 0x04: 0x10006 or 0x30006 */
	int32_t result;				/* 0x08 */
	uint32_t reserved0c[13];		/* 0x0c..0x3f */
	uint8_t bootParam[8];			/* 0x40..0x47 */
} SceCompatCommPacket;

_Static_assert(sizeof(SceCompatCommPacket) == 0x48, "SceCompatCommPacket size");

void compatReadKeyringSlot32(uint32_t slotIndex, uint32_t *outWords8);
uint32_t compatCheckKeyringSlot50bBit0(void);
uint32_t compatCheckKeyringSlot50aByte12Bit2(void);
int compatHookInvoke(void *closure, void *packetBuf);
int compatAddHook(SmServiceContext *srvCtx, uint32_t cmdId, void *mgr,
		const SmMemberFnPtr *fnPtr, SmServiceHookNode **outNode);
void compatMgrCtorC2(void *mgr);
void compatMgrCtor(void *mgr);
void compatMgrDtorD2(void *mgr);
void compatMgrDtor(void *mgr);
int compatSetKermitResetReg(void *mgr, uint32_t val);
int compatVerifySharedSramChallenge(void *mgr);
int compatCopyAndVerifySram(void *mgr, const void *src, uint32_t size,
		uintptr_t dstAddr);
int compatCheckAllowedTarget(void *mgr);
void compatCmd30006VerifyChallengeCore(void *mgr, SceCompatCommPacket *pkt);
int compatCmd30006VerifyChallenge(void *mgr, void *packetBuf);
void compatCmd10006LoadBootRomCore(void *mgr, SceCompatCommPacket *pkt);
int compatCmd10006LoadBootRom(void *mgr, void *packetBuf);

#endif /* COMPAT_SM_H */
