/*
 * compat_sm.c: PlayStation Vita ePSP (Allegrex) compatibility secure module
 * (os0:sm/compat_sm.self) service entry point and command handlers
 * (0x80b09c..0x80b5a4, .rodata 0x80c190..0x80d230).
 *
 * Internal function, structure, and table names (except sm_main) are ours.
 */

#include "compat_sm.h"

/* ============================================================================
 * .rodata (0x80c190..0x80d230)
 * ============================================================================ */

/* 0x80c190: C++ vtable for concrete hook node (vtable pointer points to +8 = 0x80c198) */
static const uintptr_t s_compatHookVtable[4] = {
	0u,
	0u,
	(uintptr_t)compatHookInvoke,
	0u
};

/* 0x80c1a0: C++ vtable for abstract hook base node */
static const uintptr_t s_compatBaseHookVtable[4] __attribute__((used)) = {
	0u,
	0u,
	(uintptr_t)__cxa_pure_virtual,
	0u
};

/* 0x80c1b0..0x80c1f0: 64-byte expected ePSP bootROM response digest */
static const uint8_t s_compatExpectedResponse[64] = {
	0x59u, 0xadu, 0x29u, 0xd3u, 0xe6u, 0x62u, 0x79u, 0xf1u,
	0xafu, 0x53u, 0x2cu, 0x62u, 0x79u, 0x92u, 0xdeu, 0xcbu,
	0x56u, 0xa8u, 0xb9u, 0x9cu, 0x68u, 0xa5u, 0x09u, 0x58u,
	0x18u, 0xf3u, 0x52u, 0xdcu, 0x9bu, 0xc7u, 0xfbu, 0x8fu,
	0x3du, 0x43u, 0x70u, 0x7du, 0x2fu, 0xbbu, 0x72u, 0x3cu,
	0x12u, 0x36u, 0x0cu, 0x8eu, 0x81u, 0xbeu, 0x03u, 0x1eu,
	0x01u, 0x2fu, 0x20u, 0xd2u, 0x68u, 0xdau, 0x7cu, 0xcdu,
	0x20u, 0x21u, 0xd5u, 0x56u, 0x0fu, 0xe6u, 0x27u, 0x02u
};

/* 0x80c1f0..0x80d1f0: 4,096-byte ePSP (Allegrex) bootROM image (built from sys661/bootrom) */
static const uint8_t s_compatBootRom[] = {
#include "bootrom.inc"
};
typedef char assert_compatBootRom_size[(sizeof(s_compatBootRom) == 4096) ? 1 : -1];

/* 0x80d1f0..0x80d230: 64-byte initial ePSP SRAM challenge payload */
static const uint8_t s_compatBootChallenge[64] = {
	0x28u, 0x34u, 0x06u, 0x24u, 0x6au, 0x9bu, 0x9cu, 0x9fu,
	0x09u, 0x5cu, 0xf0u, 0x2du, 0x98u, 0x30u, 0x73u, 0x32u,
	0xecu, 0xaau, 0x97u, 0xcbu, 0xaau, 0xc7u, 0x0bu, 0x10u,
	0x12u, 0x35u, 0xe7u, 0x7du, 0x84u, 0xa9u, 0xfdu, 0x16u,
	0x91u, 0x42u, 0xdeu, 0xb5u, 0xebu, 0xd9u, 0x09u, 0x84u,
	0x6au, 0x18u, 0x64u, 0xd2u, 0xfeu, 0xb8u, 0x1eu, 0xcdu,
	0x07u, 0x50u, 0xd8u, 0x63u, 0x13u, 0x0bu, 0xf1u, 0x90u,
	0x27u, 0x9du, 0x46u, 0xdbu, 0xa0u, 0x44u, 0x9au, 0xf1u
};

/* ============================================================================
 * Keyring Accessors & Service Entry (0x80b09c..0x80b5a4)
 * ============================================================================ */

/* 0x80b09c (name ours) */
void compatReadKeyringSlot32(uint32_t slotIndex, uint32_t *outWords8)
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

/* 0x80b0e6 (name ours) */
uint32_t compatCheckKeyringSlot50bBit0(void)
{
	uint32_t slot50b[8];
	uint32_t bit;

	compatReadKeyringSlot32(0x50bu, slot50b);
	bit = ((const uint8_t *)slot50b)[0] & 1u;
	memset(slot50b, 0, sizeof(slot50b));
	return bit;
}

/* 0x80b116 (name ours) */
uint32_t compatCheckKeyringSlot50aByte12Bit2(void)
{
	uint32_t slot50a[8];
	uint32_t bit;

	compatReadKeyringSlot32(0x50au, slot50a);
	bit = (((const uint8_t *)slot50a)[12] >> 2) & 1u;
	memset(slot50a, 0, sizeof(slot50a));
	return bit;
}

/* 0x80b14a */
int sm_main(void)
{
	SmServiceContext srvCtx;
	SmMemberFnPtr fnPtr;
	SmServiceHookNode *node1;
	SmServiceHookNode *node2;
	uint8_t mgr;
	int ret;

	compatMgrCtor(&mgr);
	smSrvInit(&srvCtx);

	fnPtr.fn = compatCmd10006LoadBootRom;
	fnPtr.adj = 0;
	ret = compatAddHook(&srvCtx, COMPAT_CMD_LOAD_BOOTROM, &mgr, &fnPtr, &node1);
	if (ret == 0) {
		fnPtr.fn = compatCmd30006VerifyChallenge;
		fnPtr.adj = 0;
		ret = compatAddHook(&srvCtx, COMPAT_CMD_VERIFY_CHALLENGE, &mgr, &fnPtr, &node2);
		if (ret == 0) {
			ret = smSrvRunLoop(&srvCtx);
			if (ret == 1)
				ret = 0;
		}
	}

	smSrvTeardown(&srvCtx);
	compatMgrDtor(&mgr);
	return ret;
}

/* 0x80b1ee (name ours) */
int compatHookInvoke(void *closure, void *packetBuf)
{
	uintptr_t *words = (uintptr_t *)closure;
	uint8_t *obj = (uint8_t *)words[1];
	uintptr_t fnVal = words[2];
	int32_t adj = (int32_t)words[3];
	SmCmdMethod fn;

	if ((fnVal & 1u) != 0u) {
		uintptr_t vptr = *(const uintptr_t *)(const void *)(obj + adj);

		fn = *(const SmCmdMethod *)(const void *)(vptr + fnVal - 1u);
	} else {
		fn = (SmCmdMethod)fnVal;
	}
	return fn(obj + adj, packetBuf);
}

/* 0x80b222 (name ours) */
int compatAddHook(SmServiceContext *srvCtx, uint32_t cmdId, void *mgr,
		const SmMemberFnPtr *fnPtr, SmServiceHookNode **outNode)
{
	SmServiceHookNode *node;

	*outNode = NULL;
	node = smSrvAllocNode(srvCtx);
	if (node == NULL)
		return SCE_SBL_ERROR_NO_MEMORY;

	node->cmdId = cmdId;
	node->next = NULL;
	if (&node->vtable != NULL) {
		node->vtable = &s_compatHookVtable[2];
		node->handler = mgr;
		node->arg0 = (void *)(uintptr_t)fnPtr->fn;
		node->adj = fnPtr->adj;
	}
	*outNode = node;
	node->next = srvCtx->head;
	srvCtx->head = node;
	return 0;
}

/* 0x80b29c (name ours) */
void compatMgrCtorC2(void *mgr)
{
	(void)mgr;
}

/* 0x80b29e (name ours) */
void compatMgrCtor(void *mgr)
{
	(void)mgr;
}

/* 0x80b2a0 (name ours) */
void compatMgrDtorD2(void *mgr)
{
	(void)mgr;
}

/* 0x80b2a2 (name ours) */
void compatMgrDtor(void *mgr)
{
	(void)mgr;
}

/* 0x80b2a4 (name ours) */
int compatSetKermitResetReg(void *mgr, uint32_t val)
{
	(void)mgr;
	REG32(KERMIT_RESET_REG_ADDR) = val;
	smMemBarrier();
	return 0;
}

/* 0x80b2b4 (name ours) */
int compatVerifySharedSramChallenge(void *mgr)
{
	uint8_t actual[64];
	uint8_t expected[64];
	int ret;

	(void)mgr;
	memcpy(expected, s_compatExpectedResponse, sizeof(expected));
	memset(actual, 0xff, sizeof(actual));
	memcpy(actual, (const void *)(uintptr_t)KERMIT_BOOT_CHALLENGE_ADDR, sizeof(actual));

	if (timingsafeBcmp(actual, expected, sizeof(actual)) != 0)
		ret = SCE_SBL_ERROR_COMPAT_AUTH_FAILED;
	else
		ret = 0;

	byteMemset(actual, 0, sizeof(actual));
	return ret;
}

/* 0x80b338 (name ours) */
int compatCopyAndVerifySram(void *mgr, const void *src, uint32_t size,
		uintptr_t dstAddr)
{
	(void)mgr;
	if (dstAddr - KERMIT_BOOT_SRAM_BASE > 0x0fffu ||
	    dstAddr + size > KERMIT_BOOT_SRAM_BASE + KERMIT_BOOT_SRAM_SIZE ||
	    dstAddr + size < dstAddr) {
		return SCE_SBL_ERROR_COMPAT_INVALID_ARG;
	}

	memcpy((void *)dstAddr, src, size);
	if (timingsafeBcmp((const void *)dstAddr, src, size) != 0)
		return SCE_SBL_ERROR_COMPAT_IO_ERROR;
	return 0;
}

/* 0x80b39e (name ours) */
int compatCheckAllowedTarget(void *mgr)
{
	uint8_t cid[16];
	int ret = 0;

	(void)mgr;
	aimgrReadConsoleIdAlias1(cid);
	if (aimgrIsRetailOrChord(cid) == 0 &&
	    aimgrIsDex(cid) == 0 &&
	    compatCheckKeyringSlot50bBit0() == 0 &&
	    compatCheckKeyringSlot50aByte12Bit2() == 0) {
		ret = SCE_SBL_ERROR_COMPAT_AUTH_FAILED;
	}
	aimgrClearConsoleId2(cid);
	return ret;
}

/* 0x80b3e6 (name ours) */
void compatCmd30006VerifyChallengeCore(void *mgr, SceCompatCommPacket *pkt)
{
	int err = compatCheckAllowedTarget(mgr);

	if (err != 0) {
		pkt->result = SCE_SBL_ERROR_COMPAT_AUTH_FAILED;
		return;
	}
	if (pkt->size != sizeof(SceCompatCommPacket) ||
	    pkt->cmdId != COMPAT_CMD_VERIFY_CHALLENGE) {
		pkt->result = SCE_SBL_ERROR_COMPAT_BAD_CMD;
		return;
	}

	REG32(KERMIT_RESET_REG_ADDR) = 4u;
	smMemBarrier();

	if (compatVerifySharedSramChallenge(mgr) >= 0)
		pkt->result = err;
	else
		pkt->result = SCE_SBL_ERROR_COMPAT_AUTH_FAILED;

	REG32(KERMIT_RESET_REG_ADDR) = 10u;
	smMemBarrier();
}

/* 0x80b46e (name ours; mailbox command 0x30006) */
int compatCmd30006VerifyChallenge(void *mgr, void *packetBuf)
{
	smSyscall2(0u, 0u, 0u);
	compatCmd30006VerifyChallengeCore(mgr, (SceCompatCommPacket *)packetBuf);
	return smSyscall3(0u, 0u, 0u);
}

/* 0x80b48c (name ours) */
void compatCmd10006LoadBootRomCore(void *mgr, SceCompatCommPacket *pkt)
{
	uint8_t bootParam[8];
	int err = compatCheckAllowedTarget(mgr);

	if (err != 0) {
		err = SCE_SBL_ERROR_COMPAT_AUTH_FAILED;
		goto fail;
	}
	if (pkt->size != sizeof(SceCompatCommPacket) ||
	    pkt->cmdId != COMPAT_CMD_LOAD_BOOTROM) {
		err = SCE_SBL_ERROR_COMPAT_BAD_CMD;
		goto fail;
	}

	memcpy(bootParam, pkt->bootParam, sizeof(bootParam));
	REG32(KERMIT_RESET_REG_ADDR) = 4u;
	smMemBarrier();

	memset((void *)(uintptr_t)KERMIT_BOOT_SRAM_BASE, 0, KERMIT_BOOT_SRAM_SIZE);
	if (compatCopyAndVerifySram(mgr, s_compatBootRom, sizeof(s_compatBootRom),
			KERMIT_BOOT_SRAM_BASE) < 0 ||
	    compatCopyAndVerifySram(mgr, s_compatBootChallenge,
			sizeof(s_compatBootChallenge),
			KERMIT_BOOT_CHALLENGE_ADDR) < 0) {
		err = SCE_SBL_ERROR_COMPAT_LOAD_FAILED;
		goto fail;
	}

	REG32(KERMIT_BOOT_PARAM_ADDR) = *(const uint32_t *)(const void *)bootParam;
	REG32(KERMIT_RESET_REG_ADDR) = 0u;
	smMemBarrier();

	pkt->result = 0;
	byteMemset(bootParam, 0, sizeof(bootParam));
	return;

fail:
	pkt->result = err;
	memset((void *)(uintptr_t)KERMIT_BOOT_SRAM_BASE, 0, KERMIT_BOOT_SRAM_SIZE);
	byteMemset(bootParam, 0, sizeof(bootParam));
	REG32(KERMIT_RESET_REG_ADDR) = 10u;
	smMemBarrier();
}

/* 0x80b586 (name ours; mailbox command 0x10006) */
int compatCmd10006LoadBootRom(void *mgr, void *packetBuf)
{
	smSyscall2(0u, 0u, 0u);
	compatCmd10006LoadBootRomCore(mgr, (SceCompatCommPacket *)packetBuf);
	return smSyscall3(0u, 0u, 0u);
}
