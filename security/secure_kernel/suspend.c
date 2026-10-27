/*
 * suspend.c: Secure Module context suspend and resume (AES-CBC encryption and
 * HMAC-SHA256 authentication of SM CPU contexts, IRQ handlers, mailbox state,
 * and SM SRAM over scatter-gather lists in DRAM) (0x80203c..0x802767).
 *
 * Function names "setupKeyManager" (0x802096) and "hashContext" (0x8020e2) are
 * Sony's, recovered from 0.995.070-TOOL debug strings. Other names are ours.
 */

#include "secure_kernel.h"

typedef struct SceSkSuspendHeader {
	uint32_t magic;			/* 0x00: 0x534d4300 ("SMC\0") */
	uint32_t version;		/* 0x04: 1 */
	uint32_t totalSize;		/* 0x08: 0x16000 + 0x1c0 */
	uint32_t seqno;			/* 0x0c */
	uint8_t reserved[48];		/* 0x10..0x3f */
} SceSkSuspendHeader;

_Static_assert(sizeof(SceSkSuspendHeader) == 0x40, "SceSkSuspendHeader size");

typedef struct SceSkSuspendBody {
	SceSkCpuContext mainCtx;	/* 0x00..0x5f */
	SceSkCpuContext irqCtx;		/* 0x60..0xbf */
	uint32_t irqHandlers[9];	/* 0xc0..0xe3: IRQ 1..6, 9..11 */
	uint32_t mailboxRegs[6];	/* 0xe4..0xfb: 0xe0000004..0x0c, 0x14..0x1c */
	uint32_t savedIrqMask;		/* 0xfc */
	uint8_t padding[64];		/* 0x100..0x13f */
} SceSkSuspendBody;

_Static_assert(sizeof(SceSkSuspendBody) == 0x140, "SceSkSuspendBody size");

typedef struct SceSkSuspendKeySeed {
	uint32_t seqno;			/* 0x00 */
	uint8_t randomBytes[32];	/* 0x04..0x23 */
} SceSkSuspendKeySeed;

/* .rodata (0x8074b4..0x8074d3) */
static const uint8_t s_suspendMaikaSeed[16] = {
	0x62u, 0xa5u, 0x75u, 0x47u, 0x40u, 0xedu, 0x34u, 0x4cu,
	0x43u, 0x4au, 0xfdu, 0x4du, 0x61u, 0x69u, 0x6bu, 0x61u,
};

static const uint8_t s_suspendZeroSeed[16] = {
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
	0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
};

/* .bss (0x807d40..0x807f23) */
static SceSkSuspendHeader s_suspendHeader __attribute__((aligned(64)));
static SceSkSuspendBody s_suspendBody __attribute__((aligned(64)));
static uint8_t s_suspendHmacBlock[64] __attribute__((aligned(64)));
static SceSkSuspendKeySeed s_suspendKeySeed __attribute__((aligned(64)));

/* 0x80203c (name ours) */
void deriveSuspendKeyToSlot(uint32_t channel, const void *srcSeed16)
{
	uint32_t dstKeySlot;
	uint32_t srcKeySlot;
	uint32_t status;

	if (channel == 0u) {
		dstKeySlot = 0x346u;
		srcKeySlot = 0x22u;
	} else {
		dstKeySlot = 0x347u;
		srcKeySlot = 0x23u;
	}

	dmac5StartCmd(channel, (void *)(uintptr_t)dstKeySlot, srcSeed16, 16u,
		      srcKeySlot, 0u, 0x10000101u, 0u);
	status = dmac5WaitChannel(channel);
	if (status != 0u)
		skPanic("%08x %08x\n", 25u, status);
}

/* 0x802096 (Sony name from 0.995.070-TOOL) */
int setupKeyManager(uint32_t seqno, const SceSkKeySeedDesc *seedDesc)
{
	int res = getSeqnoRandomBytes(seqno, s_suspendKeySeed.randomBytes);

	if (res == 0) {
		s_suspendKeySeed.seqno = seqno;
		deriveSuspendKeyToSlot(0u, &s_suspendKeySeed);
		deriveSuspendKeyToSlot(1u, &s_suspendKeySeed);
		byteMemcpy(seedDesc->buf, s_suspendMaikaSeed, seedDesc->size);
	}
	return res;
}

/* 0x8020e2 (Sony name from 0.995.070-TOOL) */
void hashContext(void *dstDigest, const void *src, uint32_t size, uint32_t mode)
{
	uint8_t ivBuf[16];
	uint32_t modeBits;
	uint32_t status;

	memset(ivBuf, 0, sizeof(ivBuf));

	if (mode == 1u) {
		modeBits = 1u;
	} else if (mode == 0u) {
		modeBits = 0u;
	} else if (mode == 2u) {
		modeBits = 3u;
	} else if (mode == 3u) {
		modeBits = 2u;
	} else {
		skPanic("%08x %08x\n", 26u, mode);
		return;
	}

	dmac5StartCmd(1u, dstDigest, src, size, 0x23u,
		      (uint32_t)(uintptr_t)ivBuf,
		      (modeBits << 10) | 0x013bu, 2u);
	status = dmac5WaitChannel(1u);
	if (status != 0u)
		skPanic("%08x %08x\n", 25u, status);
}

/* 0x802182 (name ours) */
void transferScatterGatherRange(void *localBuf, uint32_t totalSize,
		uint32_t startOffset, const SceSkScatterListDesc *sgDesc,
		uint32_t opType, const SceSkKeySeedDesc *seedDesc)
{
	uint8_t *curLocal = (uint8_t *)localBuf;
	uint32_t remSize = totalSize;
	uint32_t skipOffset = startOffset;
	uint32_t i;

	if (sgDesc == NULL)
		skPanic("%08x\n", 32u);

	for (i = 0u; i < sgDesc->count; i++) {
		SceSkRangeEntry entry;
		uint32_t dramAddr;
		uint32_t chunkLen;
		uint32_t status;

		copyRangeEntry8(&entry, &sgDesc->entries[i]);
		dramAddr = entry.paddr;
		chunkLen = entry.size;

		if (checkRangeInTzDram(dramAddr, chunkLen) == 0)
			skPanic("%08x\n", 3u);

		if (skipOffset != 0u) {
			if (skipOffset >= chunkLen) {
				skipOffset -= chunkLen;
				continue;
			}
			dramAddr += skipOffset;
			chunkLen -= skipOffset;
		}

		if (chunkLen > remSize)
			chunkLen = remSize;

		switch (opType) {
		case 1u:
			dmac5StartCmd(0u, (void *)(uintptr_t)dramAddr, curLocal,
				      chunkLen, 0u, 0u, 0u, 1u);
			status = dmac5WaitChannel(0u);
			if (status != 0u)
				skPanic("%08x %08x\n", 25u, status);
			break;

		case 2u:
			dmac5StartCmd(0u, curLocal, (const void *)(uintptr_t)dramAddr,
				      chunkLen, 0u, 0u, 0u, 1u);
			status = dmac5WaitChannel(0u);
			if (status != 0u)
				skPanic("%08x %08x\n", 25u, status);
			break;

		case 3u:
			if ((const void *)sgDesc == (const void *)seedDesc)
				skPanic("%08x\n", 32u);
			dmac5StartCmd(0u, (void *)(uintptr_t)dramAddr, curLocal,
				      chunkLen, 0x22u, (uint32_t)(uintptr_t)seedDesc->buf,
				      0x0109u, 1u);
			status = dmac5WaitChannel(0u);
			if (status != 0u)
				skPanic("%08x %08x\n", 25u, status);
			break;

		case 4u:
			if ((const void *)sgDesc == (const void *)seedDesc)
				skPanic("%08x\n", 32u);
			dmac5StartCmd(0u, curLocal, (const void *)(uintptr_t)dramAddr,
				      chunkLen, 0x22u, (uint32_t)(uintptr_t)seedDesc->buf,
				      0x010au, 1u);
			status = dmac5WaitChannel(0u);
			if (status != 0u)
				skPanic("%08x %08x\n", 25u, status);
			break;

		case 5u:
			hashContext(curLocal, (const void *)(uintptr_t)dramAddr,
				    chunkLen, 0u);
			break;

		default:
			skPanic("%08x %08x\n", 27u, opType);
			return;
		}

		remSize -= chunkLen;
		if (remSize == 0u)
			break;
		curLocal += chunkLen;
		skipOffset = 0u;
	}
}

/* 0x8022d6 (Sony error string "error: invalidate seqno\n" in 0.995) */
void *invalidateSeqno(const SceSkKeySeedDesc *seedDesc)
{
	deriveSuspendKeyToSlot(0u, s_suspendZeroSeed);
	deriveSuspendKeyToSlot(1u, s_suspendZeroSeed);
	return byteMemset(seedDesc->buf, 0, seedDesc->size);
}

/* 0x802304 (name ours) */
void suspendSmContext(const SceSkScatterListDesc *sgDesc, int isFromSwi)
{
	uint32_t commBuf = getArmCommBufferAddr();
	uint32_t smBase = getSmSramBase();
	uint32_t smSize = getSmSramSize();
	uint32_t totalSize = smSize + 0x1c0u;
	uint8_t ivBuf[16];
	SceSkKeySeedDesc seedDesc;
	uint32_t seqno;
	uint32_t psw;
	uint32_t *armSaveArea;

	seqno = allocateSeqno();
	memset(ivBuf, 0, sizeof(ivBuf));
	seedDesc.buf = ivBuf;
	seedDesc.size = sizeof(ivBuf);

	__asm__ volatile ("ldc %0, $psw" : "=r"(psw));
	if ((psw & 1u) != 0u)
		skPanic("%08x\n", 19u);

	s_suspendHeader.magic = 0x534d4300u;
	s_suspendHeader.version = 1u;
	s_suspendHeader.seqno = seqno;
	s_suspendHeader.totalSize = totalSize;

	byteMemcpy(&s_suspendBody.mainCtx,
		   isFromSwi ? &g_swiCpuContext : &g_cmdIrqCpuContext,
		   sizeof(SceSkCpuContext));
	byteMemcpy(&s_suspendBody.irqCtx, &g_irqCpuContext,
		   sizeof(SceSkCpuContext));

	s_suspendBody.irqHandlers[0] = skGetIrqHandler(1u);
	s_suspendBody.irqHandlers[1] = skGetIrqHandler(2u);
	s_suspendBody.irqHandlers[2] = skGetIrqHandler(3u);
	s_suspendBody.irqHandlers[3] = skGetIrqHandler(4u);
	s_suspendBody.irqHandlers[4] = skGetIrqHandler(5u);
	s_suspendBody.irqHandlers[5] = skGetIrqHandler(6u);
	s_suspendBody.irqHandlers[6] = skGetIrqHandler(9u);
	s_suspendBody.irqHandlers[7] = skGetIrqHandler(10u);
	s_suspendBody.irqHandlers[8] = skGetIrqHandler(11u);

	s_suspendBody.mailboxRegs[0] = REG32(0xe0000004u);
	s_suspendBody.mailboxRegs[1] = REG32(0xe0000008u);
	/* Note: Sony's code overwrote mailboxRegs[1] instead of [2] here */
	s_suspendBody.mailboxRegs[1] = REG32(0xe000000cu);
	s_suspendBody.mailboxRegs[3] = REG32(0xe0000014u);
	s_suspendBody.mailboxRegs[4] = REG32(0xe0000018u);
	s_suspendBody.mailboxRegs[5] = REG32(0xe000001cu);

	byteMemset(s_suspendBody.padding, 0, sizeof(s_suspendBody.padding));

	armSaveArea = (uint32_t *)(uintptr_t)(*(uint32_t *)(uintptr_t)(commBuf + 12u));
	armSaveArea[0] = s_suspendBody.mailboxRegs[3];
	armSaveArea[1] = s_suspendBody.mailboxRegs[4];
	armSaveArea[2] = s_suspendBody.mailboxRegs[5];
	armSaveArea[3] = s_suspendBody.mailboxRegs[0];
	armSaveArea[4] = s_suspendBody.mailboxRegs[1];
	armSaveArea[5] = s_suspendBody.mailboxRegs[2];

	s_suspendBody.savedIrqMask = skSaveIrqMask();

	if (setupKeyManager(seqno, &seedDesc) != 0)
		skPanic("%08x\n", 29u);

	transferScatterGatherRange(&s_suspendHeader, sizeof(s_suspendHeader),
				   0u, sgDesc, 1u, NULL);
	hashContext(s_suspendHmacBlock, &s_suspendHeader, sizeof(s_suspendHeader), 1u);

	transferScatterGatherRange(&s_suspendBody, sizeof(s_suspendBody),
				   0x40u, sgDesc, 3u, &seedDesc);
	hashContext(s_suspendHmacBlock, &s_suspendBody, sizeof(s_suspendBody), 2u);

	transferScatterGatherRange((void *)(uintptr_t)smBase, smSize,
				   0x180u, sgDesc, 3u, &seedDesc);
	hashContext(s_suspendHmacBlock, (const void *)(uintptr_t)smBase, smSize, 3u);

	transferScatterGatherRange(s_suspendHmacBlock, sizeof(s_suspendHmacBlock),
				   smSize + 0x180u, sgDesc, 3u, &seedDesc);
	(void)invalidateSeqno(&seedDesc);
}

/* 0x802542 (name ours) */
int resumeSmContext(const SceSkScatterListDesc *sgDesc)
{
	uint32_t smBase = getSmSramBase();
	uint32_t smSize = getSmSramSize();
	uint8_t ivBuf[16];
	uint8_t calcDigest[16];
	SceSkKeySeedDesc seedDesc;
	uint32_t psw;
	uint32_t seqno;
	uint32_t diff;
	uint32_t i;
	const uint32_t *armSaveArea;

	memset(ivBuf, 0, sizeof(ivBuf));
	seedDesc.buf = ivBuf;
	seedDesc.size = sizeof(ivBuf);

	__asm__ volatile ("ldc %0, $psw" : "=r"(psw));
	if ((psw & 1u) != 0u)
		skPanic("%08x\n", 18u);

	transferScatterGatherRange(&s_suspendHeader, sizeof(s_suspendHeader),
				   0u, sgDesc, 2u, NULL);

	if (s_suspendHeader.magic != 0x534d4300u ||
	    s_suspendHeader.version != 1u ||
	    s_suspendHeader.totalSize != smSize + 0x1c0u) {
		return (int)0x800f0324;
	}

	seqno = s_suspendHeader.seqno;
	if (setupKeyManager(seqno, &seedDesc) != 0)
		return (int)0x800f0324;

	hashContext(calcDigest, &s_suspendHeader, sizeof(s_suspendHeader), 1u);

	transferScatterGatherRange(&s_suspendBody, sizeof(s_suspendBody),
				   0x40u, sgDesc, 4u, &seedDesc);
	hashContext(calcDigest, &s_suspendBody, sizeof(s_suspendBody), 2u);

	transferScatterGatherRange((void *)(uintptr_t)smBase, smSize,
				   0x180u, sgDesc, 4u, &seedDesc);
	hashContext(calcDigest, (const void *)(uintptr_t)smBase, smSize, 3u);

	transferScatterGatherRange(s_suspendHmacBlock, sizeof(s_suspendHmacBlock),
				   smSize + 0x180u, sgDesc, 4u, &seedDesc);
	(void)invalidateSeqno(&seedDesc);

	diff = 0u;
	for (i = 0u; i < 16u; i++)
		diff |= (uint32_t)(calcDigest[i] ^ s_suspendHmacBlock[i]);
	if (diff != 0u)
		return (int)0x800f0324;

	byteMemcpy(&g_cmdIrqCpuContext, &s_suspendBody.mainCtx, sizeof(SceSkCpuContext));
	byteMemcpy(&g_irqCpuContext, &s_suspendBody.irqCtx, sizeof(SceSkCpuContext));

	skRegisterIrqHandler(1u, s_suspendBody.irqHandlers[0]);
	skRegisterIrqHandler(2u, s_suspendBody.irqHandlers[1]);
	skRegisterIrqHandler(3u, s_suspendBody.irqHandlers[2]);
	skRegisterIrqHandler(4u, s_suspendBody.irqHandlers[3]);
	skRegisterIrqHandler(5u, s_suspendBody.irqHandlers[4]);
	skRegisterIrqHandler(6u, s_suspendBody.irqHandlers[5]);
	skRegisterIrqHandler(9u, s_suspendBody.irqHandlers[6]);
	skRegisterIrqHandler(10u, s_suspendBody.irqHandlers[7]);
	skRegisterIrqHandler(11u, s_suspendBody.irqHandlers[8]);

	REG32(0xe0000044u) = 0xffffffffu;
	REG32(0xe0000048u) = 0xffffffffu;
	REG32(0xe000004cu) = 0xffffffffu;

	armSaveArea = (const uint32_t *)(uintptr_t)sgDesc->contextBufAddr;
	REG32(0xe0000004u) = armSaveArea[3];
	REG32(0xe0000008u) = armSaveArea[4];
	REG32(0xe000000cu) = armSaveArea[5];

	skRestoreIrqMask(s_suspendBody.savedIrqMask);
	if (removeAndFreeSeqno(seqno) != 0)
		skPanic("%08x\n", 28u);

	return 0;
}
