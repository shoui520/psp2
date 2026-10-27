/*
 * handler.c: ARM->MeP command IRQ dispatcher (sceSblSkHandlerCmdIrq), Secure
 * Module loader (sceSblSkLoadSecureModule / sceSblSkRunSecureModule), Secure
 * Module SWI system call dispatcher (sceSblSkHandlerSwi), and byte memory
 * helpers (0x8004f4..0x801055).
 *
 * File name ("handler.c") and function names ("sceSblSkLoadSecureModule",
 * "sceSblSkRunSecureModule", "sceSblSkHandlerSwi") are Sony's, recovered from
 * 0.995.070-TOOL debug strings. Other helper names are ours.
 */

#include "secure_kernel.h"

/* .sbss (0x808160..0x80816b) */
static uint32_t s_smCriticalSectionDepth;
static int s_socRevisionCached;
static uint32_t s_socRevisionValue;

/* .bss (0x807d00..0x807d0f, 64-byte aligned) */
static SceSkScatterListDesc s_pendingSuspendArgs __attribute__((aligned(64)));

/* .data (0x8079c4..0x8079d3): AES-128-CBC IV for command 0x0f01 */
static uint8_t s_cmd0f01AesIv[16] = {
	0xa1u, 0x32u, 0x5au, 0xd8u, 0xb9u, 0x21u, 0x2fu, 0xefu,
	0x72u, 0x16u, 0xefu, 0xfbu, 0x30u, 0xcbu, 0x4du, 0xfcu,
};

/* 0x8004f4 (name ours) */
void readKeyringSlot32(uint32_t slotIndex, uint32_t *outWords8)
{
	volatile const uint32_t *slot =
		(volatile const uint32_t *)(uintptr_t)(0xe0058000u + (slotIndex << 5));

	outWords8[0] = slot[0];
	outWords8[1] = slot[1];
	outWords8[2] = slot[2];
	outWords8[3] = slot[3];
	outWords8[4] = slot[4];
	outWords8[5] = slot[5];
	outWords8[6] = slot[6];
	outWords8[7] = slot[7];
}

/* 0x80053e (name ours) */
uint32_t getSocRevisionCached(void)
{
	if (!s_socRevisionCached) {
		s_socRevisionValue = REG32(0xe3100000u);
		s_socRevisionCached = 1;
	}
	return s_socRevisionValue;
}

/* 0x80055e (name ours) */
void triggerArmInterrupt(uint16_t code)
{
	REG32(0xe0000000u) = (uint32_t)code;
	(void)REG32(0xe0000000u);
	__asm__ volatile ("nop\n\tsyncm" ::: "memory");

	while ((REG32(0xe0000000u) & 0xffffu) != 0u)
		;
}

/* 0x80057a (name ours) */
void resetPervasiveBlock(void)
{
	REG32(0xe3101190u) = 1u;
	REG32(0xe3101190u) = 0u;
	while (REG32(0xe3101190u) != 0u)
		;
}

/* 0x800596 (name ours) */
void enableCryptoKeyringBusAccess(void)
{
	REG32(0xe0030024u) = 0x1fef020fu;
	REG32(0xe0030024u) = 0x1fef020eu;
	REG32(0xe0070008u) = 0x020e020fu;
	REG32(0xe0070000u) = 0u;
	REG32(0xe0070000u) = 1u;
}

/* 0x8005d2 (name ours) */
void abortAndClearDmac5(void)
{
	uint32_t psw;

	__asm__ volatile ("ldc %0, $psw" : "=r"(psw));
	if ((psw & 1u) != 0u) {
		setPanicArmInterruptCode(0x00200000u);
		skPanic("%08x\n", 15u);
	}

	if ((REG32(0xe0050024u) & 1u) != 0u) {
		REG32(0xe005001cu) = 0u;
		while ((REG32(0xe0050024u) & 1u) != 0u)
			;
	}

	if ((REG32(0xe00500a4u) & 1u) != 0u) {
		REG32(0xe005009cu) = 0u;
		while ((REG32(0xe00500a4u) & 1u) != 0u)
			;
	}

	REG32(0xe0050028u) = 3u;
	REG32(0xe00500a8u) = 3u;
}

/* 0x80065a (name ours) */
void writeSmReturnStatus(uint32_t statusVal)
{
	uint32_t dstAddr = getSmReturnStatusAddr();

	if (checkRangeInAnyDram(dstAddr, 4u) == 0) {
		setPanicArmInterruptCode(0x00400000u);
		skPanic("%08x\n", 4u);
	}
	byteMemcpy((void *)(uintptr_t)dstAddr, &statusVal, 4u);
}

/* 0x800698 (name ours) */
void clearSmRegionSram(void)
{
	uint32_t base = getSmSramBase();
	uint32_t size = getSmSramSize();

	(void)dmac5FillCh0((void *)(uintptr_t)base, 0u, size, 0u);
}

/* 0x8006be (name ours) */
void cleanupSmAndReenterKernel(void)
{
	abortAndClearDmac5();
	clearSmRegionSram();
	skInvalidateAndFlushIcache();

	REG32(0xe0000044u) = 0xffffffffu;
	REG32(0xe0000048u) = 0xffffffffu;
	REG32(0xe000004cu) = 0xffffffffu;
	REG32(0xe0000014u) = 0xffffffffu;
	REG32(0xe0000018u) = 0xffffffffu;
	REG32(0xe000001cu) = 0xffffffffu;

	setSkState(7u);
	triggerArmInterrupt(0x0104u);
	skReenterKernel();
}

/* 0x800720 (name ours) */
void cmdStopSmImmediate(void)
{
	setSkState(6u);
	cleanupSmAndReenterKernel();
}

/* 0x800736 (name ours) */
void cleanupFailedSmLoad(uint16_t errCode)
{
	clearSmRegionSram();
	triggerArmInterrupt(errCode);
	setSkState(3u);
	triggerArmInterrupt(0x0102u);
}

/* 0x800758 (name ours) */
void suspendSmAndReenterKernel(const SceSkScatterListDesc *sgDesc, int isFromSwi)
{
	triggerArmInterrupt(0x0107u);
	abortAndClearDmac5();
	suspendSmContext(sgDesc, isFromSwi);
	cleanupSmAndReenterKernel();
}

/* 0x800780 (name ours) */
void *readArmCommBuffer(void *dst, uint32_t size)
{
	uint32_t commAddr = getArmCommBufferAddr();

	if (checkRangeInTzDram(commAddr, size) == 0) {
		setPanicArmInterruptCode(0x00400000u);
		skPanic("%08x\n", 2u);
	}
	return byteMemcpy(dst, (const void *)(uintptr_t)commAddr, size);
}

/* 0x8007c8 (name ours) */
void cmdLoadSrvk(void)
{
	SceSkBufferDesc srvkDesc;
	int res;

	readArmCommBuffer(&srvkDesc, sizeof(srvkDesc));
	res = aclibLoadSrvk(&srvkDesc);
	if (res != 0)
		triggerArmInterrupt((uint16_t)(((uint32_t)res & 0xffu) | 0x8000u));
	else
		triggerArmInterrupt(0x0001u);
}

/* 0x8007f4 (name ours) */
void cmdSetDebugLogBuffer(void)
{
	SceSkBufferDesc logDesc;

	readArmCommBuffer(&logDesc, sizeof(logDesc));
	if ((logDesc.addr & 3u) != 0u ||
	    checkRangeInAnyDram(logDesc.addr, logDesc.size) == 0) {
		triggerArmInterrupt(0x8016u);
		return;
	}

	setDebugLogBufferAddr(logDesc.addr);
	setDebugLogBufferSize(logDesc.size);
	triggerArmInterrupt(0x0001u);
	skDebugPrintf("rev %s\n", "6862");
}

/* 0x800846 (name ours) */
void cmdStopSm(void)
{
	setSkState(6u);
	if (isSmCriticalSectionActive() != 0u) {
		setSmStopPending();
		return;
	}
	triggerArmInterrupt(0x0108u);
	cleanupSmAndReenterKernel();
}

/* 0x800872 (name ours) */
void cmdSuspendSm(void)
{
	SceSkScatterListDesc sgDesc;

	readArmCommBuffer(&sgDesc, sizeof(sgDesc));
	if ((sgDesc.statusAddr & 3u) != 0u ||
	    ((sgDesc.contextBufAddr | (uint32_t)(uintptr_t)sgDesc.entries) & 3u) != 0u ||
	    checkRangeInAnyDram(sgDesc.statusAddr, 4u) == 0 ||
	    checkRangeInAnyDram((uint32_t)(uintptr_t)sgDesc.entries, 8u) == 0 ||
	    checkRangeInAnyDram(sgDesc.contextBufAddr, 24u) == 0) {
		triggerArmInterrupt(0x8016u);
		return;
	}

	setSkState(6u);
	if (isSmCriticalSectionActive() != 0u) {
		memcpy(&s_pendingSuspendArgs, &sgDesc, sizeof(sgDesc));
		setSmSuspendPending();
	} else {
		suspendSmAndReenterKernel(&sgDesc, 0);
	}
}

/* 0x8008f0 (name ours) */
void cmdResumeSm(void)
{
	SceSkScatterListDesc sgDesc;

	readArmCommBuffer(&sgDesc, sizeof(sgDesc));
	if ((sgDesc.statusAddr & 3u) != 0u ||
	    ((sgDesc.contextBufAddr | (uint32_t)(uintptr_t)sgDesc.entries) & 3u) != 0u ||
	    checkRangeInAnyDram(sgDesc.statusAddr, 4u) == 0 ||
	    checkRangeInAnyDram((uint32_t)(uintptr_t)sgDesc.entries, 8u) == 0 ||
	    checkRangeInAnyDram(sgDesc.contextBufAddr, 24u) == 0) {
		triggerArmInterrupt(0x8016u);
		return;
	}

	setSkState(4u);
	if (resumeSmContext(&sgDesc) != 0) {
		cleanupFailedSmLoad(0x8024u);
		return;
	}

	setSmReturnStatusAddr(sgDesc.statusAddr);
	setSkState(5u);
	triggerArmInterrupt(0x0103u);
}

/*
 * sceSblSkRunSecureModule (inlined at 0x800a12..0x800a84, Sony name from
 * 0.995.070-TOOL): clears mailbox status registers, loads the 4 SM entry
 * arguments from the ARM communication buffer (+0x0c..+0x18), sets $epc to
 * the SM entry point (0x80b000), scrubs all other registers, and enters the
 * SM via reti.
 */
static inline __attribute__((always_inline)) void sceSblSkRunSecureModule(void)
{
	uint32_t commBuf = getArmCommBufferAddr();

	REG32(0xe0000044u) = 0xffffffffu;
	REG32(0xe0000048u) = 0xffffffffu;
	REG32(0xe000004cu) = 0xffffffffu;

	__asm__ volatile (
		"mov $12, %0\n\t"
		"lw $4, 12($12)\n\t"
		"lw $3, 8($12)\n\t"
		"lw $2, 4($12)\n\t"
		"lw $1, ($12)\n\t"
		"movh $12, 0x81\n\t"
		"add3 $12, $12, -20480\n\t"
		"stc $12, $epc\n\t"
		"xor $0, $0\n\t"
		"xor $5, $5\n\t"
		"xor $6, $6\n\t"
		"xor $7, $7\n\t"
		"xor $8, $8\n\t"
		"xor $9, $9\n\t"
		"xor $10, $10\n\t"
		"xor $11, $11\n\t"
		"xor $12, $12\n\t"
		"xor $tp, $tp\n\t"
		"xor $gp, $gp\n\t"
		"xor $sp, $sp\n\t"
		"stc $0, $sar\n\t"
		"stc $0, $lp\n\t"
		"stc $0, $tmp\n\t"
		"stc $0, $npc\n\t"
		"stc $0, $rpb\n\t"
		"stc $0, $rpe\n\t"
		"stc $0, $rpc\n\t"
		"nop\n\t"
		"reti"
		:
		: "r"(commBuf + 12u)
		: "memory"
	);
	skPanic("%08x\n", 5u);
}

/* 0x800970 (Sony name) */
int sceSblSkLoadSecureModule(void)
{
	SceSkLoadSmCmdArgs cmdArgs;
	SceSkSmAuthContext authCtx;
	int res;

	byteMemset(&authCtx, 0, sizeof(authCtx));
	readArmCommBuffer(&cmdArgs, sizeof(cmdArgs));

	if (((cmdArgs.rangesPaddr | cmdArgs.returnStatusPaddr) & 3u) != 0u ||
	    checkRangeInAnyDram(cmdArgs.returnStatusPaddr, 4u) == 0 ||
	    checkRangeInAnyDram(cmdArgs.rangesPaddr, 8u) == 0) {
		triggerArmInterrupt(0x8016u);
		return 0;
	}

	setSkState(4u);
	authCtx.caller.authId = cmdArgs.callerAuthId;
	byteMemcpy(authCtx.caller.cap1, cmdArgs.callerDigest, 32u);
	authCtx.moduleType = cmdArgs.flags24;
	authCtx.attrFlags = cmdArgs.flags20;

	res = selfLoaderWrapperLoadSm(&cmdArgs, &authCtx);
	if (res != 1) {
		cleanupFailedSmLoad((uint16_t)(((uint32_t)res & 0xffu) | 0x8000u));
		return res;
	}

	setSmReturnStatusAddr(cmdArgs.returnStatusPaddr);
	setSkState(5u);
	triggerArmInterrupt(0x0103u);
	sceSblSkRunSecureModule();
	return res;
}

/* 0x800aa6 (name ours) */
void cmdShutdownSk(void)
{
	setSkState(9u);
	skEnableAllIrqChannels();
}

/* 0x800abc (name ours) */
void notifyArmSmExited(void)
{
	REG32(0xe0000000u) = 0x00010000u;
	(void)REG32(0xe0000000u);
	__asm__ volatile ("nop\n\tsyncm" ::: "memory");

	while ((REG32(0xe0000000u) & 0xffff0000u) != 0u)
		;
}

/* 0x800adc (name ours; Sony handler in handler.c) */
void sceSblSkHandlerCmdIrq(void)
{
	uint32_t cmd = REG32(0xe0000010u);
	void (*handlerFn)(void);

	switch (cmd) {
	case 0x0b01u:
		resetPervasiveBlock();
		enableCryptoKeyringBusAccess();
		break;

	case 0x0c01u:
		if ((REG32(0xe0062180u) & 4u) != 0u) {
			resetPervasiveBlock();
			REG32(0xe0030024u) = 0x1fef020fu;
			REG32(0xe0030024u) = 0x1fef020eu;
			REG32(0xe0070008u) = 0x020e020fu;
			REG32(0xe0070000u) = 0u;
		}
		break;

	case 0x0d01u:
		if ((REG32(0xe0062180u) & 4u) != 0u) {
			resetPervasiveBlock();
			enableCryptoKeyringBusAccess();
		}
		break;

	case 0x0e01u:
		resetPervasiveBlock();
		enableCryptoKeyringBusAccess();
		if ((getSocRevisionCached() & 0x00ffffffu) != 0x20u &&
		    (getSocRevisionCached() & 0x00ffffffu) != 0x30u &&
		    (getSocRevisionCached() & 0x00ffffffu) != 0x32u) {
			REG32(0xe0070014u) = 6u;
		}
		break;

	case 0x0f01u:
	{
		uint32_t aesKey[8];
		uint32_t cmacKey[8];
		BigmacContext bmCtx;
		uint32_t pkt[40];
		uint8_t localIv[16];

		pkt[0] = 0xacb4acb1u;
		pkt[1] = 1u;
		pkt[2] = REG32(0xe005003cu);
		pkt[3] = 0u;
		readKeyringSlot32(0x511u, &pkt[4]);
		readKeyringSlot32(0x512u, &pkt[12]);
		readKeyringSlot32(0x517u, &pkt[20]);
		readKeyringSlot32(0x519u, &pkt[28]);
		readKeyringSlot32(0x514u, cmacKey);

		bigmacInitContext(&bmCtx);
		if (bigmacAesCmacWithKey(&pkt[36], pkt, 0x90u, cmacKey, 0x20u, &bmCtx) == 0 &&
		    bigmacWaitChannel(&bmCtx) == 0) {
			readKeyringSlot32(0x515u, aesKey);
			byteMemcpy(localIv, s_cmd0f01AesIv, 16u);
			if (bigmacAes128CbcEncryptSync(pkt, pkt, 0xa0u, aesKey, localIv) == 0) {
				byteMemcpy((void *)(uintptr_t)0x4001ff00u, pkt, 0xa0u);
				REG32(0xe0030000u) = 1u;
				REG32(0xe0030004u) = 0u;
				REG32(0xe0030008u) = 0u;
				REG32(0xe003000cu) = 0u;
				REG32(0xe0030010u) = 0u;
				REG32(0xe0030014u) = 0u;
				REG32(0xe0030018u) = 0u;
				REG32(0xe003001cu) = 0u;
				REG32(0xe0030020u) = 0x516u;
			}
		}
		break;
	}

	case 0x1001u:
	{
		uint32_t slotWords[8];
		volatile uint32_t *dst = (volatile uint32_t *)(uintptr_t)0xe04e0360u;
		uint32_t i;

		readKeyringSlot32(0x51au, slotWords);
		for (i = 0; i < 8u; i++)
			dst[i] = slotWords[i];
		break;
	}

	default:
		break;
	}

	REG32(0xe0000010u) = 0xffffffffu;

	switch (cmd) {
	case 0x0b01u:
	case 0x0c01u:
	case 0x0d01u:
	case 0x0e01u:
	case 0x0f01u:
	case 0x1001u:
		return;
	default:
		break;
	}

	switch (cmd) {
	case 0x000101u:
		handlerFn = cmdShutdownSk;
		break;
	case 0x000501u:
		handlerFn = cmdStopSm;
		break;
	case 0x000601u:
		handlerFn = cmdStopSmImmediate;
		break;
	case 0x080901u:
		handlerFn = cmdSetDebugLogBuffer;
		break;
	case 0x080a01u:
		handlerFn = cmdLoadSrvk;
		break;
	case 0x100301u:
		handlerFn = cmdResumeSm;
		break;
	case 0x100401u:
		handlerFn = cmdSuspendSm;
		break;
	case 0x500201u:
		handlerFn = (void (*)(void))sceSblSkLoadSecureModule;
		break;
	default:
		triggerArmInterrupt(0x802au);
		return;
	}

	if (!isCommandAllowedInCurrentState((cmd >> 8) & 0xffu)) {
		triggerArmInterrupt(0x8029u);
		return;
	}

	if ((REG32(0xe0000000u) & 0xffffu) != 0u ||
	    (REG32(0xe0000000u) & 0xffff0000u) != 0u) {
		triggerArmInterrupt(0x802du);
		return;
	}

	handlerFn();
}

/* 0x800e28 (Sony name) */
int sceSblSkHandlerSwi(uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t swiNum)
{
	switch (swiNum) {
	case 1u:
		setSkState(8u);
		writeSmReturnStatus(arg1);
		notifyArmSmExited();
		clearSmRegionSram();
		skInvalidateAndFlushIcache();
		skReenterKernel();
		skPanic("%08x\n", 6u);
		return (int)0x800f032c;

	case 2u:
		setSmCriticalSectionActive();
		s_smCriticalSectionDepth++;
		return 1;

	case 3u:
		if (isSmCriticalSectionActive() == 0u)
			return (int)0x800f0329;

		clearSmCriticalSectionActive();
		s_smCriticalSectionDepth--;
		if (s_smCriticalSectionDepth == 0u) {
			if (isSmStopPending() != 0u) {
				clearSmStopPending();
				triggerArmInterrupt(0x0108u);
				cleanupSmAndReenterKernel();
			}
			if (isSmSuspendPending() != 0u) {
				clearSmSuspendPending();
				suspendSmAndReenterKernel(&s_pendingSuspendArgs, 1);
			}
		}
		return 1;

	case 4u:
	{
		uint32_t smBase = getSmSramBase();
		uint32_t smEnd = getSmSramEnd();
		uint32_t prevHandler;

		if (arg1 > 11u || ((1u << arg1) & 0x0e7eu) == 0u)
			return (int)0x800f0316;
		if ((arg2 < smBase || arg2 >= smEnd) && arg2 != 0u)
			return (int)0x800f030e;
		if ((arg3 < smBase || arg3 >= smEnd) && arg3 != 0u)
			return (int)0x800f030e;
		if ((arg2 & 1u) != 0u || (arg3 & 2u) != 0u)
			return (int)0x800f0316;

		prevHandler = skGetIrqHandler(arg1);
		skRegisterIrqHandler(arg1, arg2);
		if (arg3 != 0u)
			*(uint32_t *)(uintptr_t)arg3 = prevHandler;
		return 1;
	}

	case 5u:
		if (getDebugLogBufferAddr() != 0xffffffffu)
			skDebugPrintf("%s", (const char *)(uintptr_t)arg1);
		return 1;

	case 6u:
	{
		SceSkBufferDesc srvkDesc;

		if (arg1 == 0u || arg2 == 0u ||
		    checkRangeInSmSram(arg1, 0x130u) == 0 ||
		    checkRangeInSmSram(arg2, 0x80u) == 0) {
			return (int)0x800f0b16;
		}

		srvkDesc.addr = getSrvkListAddr();
		srvkDesc.size = getSrvkListSize();
		if (srvkDesc.addr == 0xffffffffu)
			return (int)0x800f0326;

		return sceSblACLibIsLoadable(
			(const SceSkSmAuthContext *)(uintptr_t)arg1,
			(const SceSkSelfAuthInfo *)(uintptr_t)arg2,
			&srvkDesc);
	}

	case 7u:
		skDebugPrintf("%08x %08x\n", 24u, arg1 ^ SK_PC_OBFUSCATION_MASK);
		setSkState(8u);
		writeSmReturnStatus(0x800f033bu);
		notifyArmSmExited();
		clearSmRegionSram();
		skReenterKernel();
		skPanic("%08x\n", 7u);
		return (int)0x800f032c;

	case 8u:
		return (srvkCheckFlagBit62() == 1);

	default:
		return (int)0x800f032c;
	}
}

/* 0x801016 (name ours) */
void *byteMemcpy(void *dst, const void *src, uint32_t size)
{
	uint8_t *d = (uint8_t *)dst;
	const uint8_t *s = (const uint8_t *)src;
	uint32_t i;

	for (i = 0; i < size; i++)
		d[i] = s[i];
	return dst;
}

/* 0x80102e (name ours) */
void *byteMemset(void *dst, int val, uint32_t size)
{
	uint8_t *d = (uint8_t *)dst;
	uint32_t i;

	for (i = 0; i < size; i++)
		d[i] = (uint8_t)val;
	return dst;
}

/* 0x801042 (name ours) */
uint32_t strnlenSimple(const char *str, uint32_t maxLen)
{
	uint32_t len = 0;

	while (maxLen != 0u) {
		if (*str == '\0')
			break;
		len++;
		str++;
		maxLen--;
	}
	return len;
}
