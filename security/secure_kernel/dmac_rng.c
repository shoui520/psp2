/*
 * dmac_rng.c: suspend/resume sequence-number & random-seed pool, Bigmac DMAC5
 * channel primitives, and hardware boot-state / SoC version checks
 * (0x805aac..0x805f2b).
 *
 * Internal function and structure names are ours.
 */

#include "secure_kernel.h"

typedef struct SceSkSeqnoNode {
	struct SceSkSeqnoNode *next;	/* 0x00 */
	uint32_t seqno;			/* 0x04 */
	uint32_t randomWords[3];	/* 0x08..0x13 */
} SceSkSeqnoNode;

_Static_assert(sizeof(SceSkSeqnoNode) == 0x14, "SceSkSeqnoNode size");

/* ============================================================================
 * .data (0x807c4c..0x807cb8)
 * ============================================================================ */

/* 0x807c4c: singly-linked list head of active sequence number nodes */
static SceSkSeqnoNode *s_seqnoListHead = NULL;

/*
 * 0x807c50..0x807ca0: 4-slot sequence number node pool (80 bytes), initialized
 * in Sony's binary with the Secure Kernel plaintext Easter-egg string.
 */
static union {
	char banner[80];
	SceSkSeqnoNode nodes[4];
} s_seqnoPool = {
	"Congratulations! At last you get the plaintext of the PS Vita Secure Kernel!"
};

/* 0x807ca0..0x807cb8: pool allocation state and temporary RNG buffer */
static uint32_t s_seqnoAllocBitmap = 0u;	/* 0x807ca0 */
static uint32_t s_seqnoAllocCount = 0u;		/* 0x807ca4 */
static uint32_t s_rngTempWords[3] = { 0u, 0u, 0u }; /* 0x807ca8..0x807cb3 */
static uint32_t s_nextSeqnoCounter = 0u;	/* 0x807cb4 */

/* ============================================================================
 * Sequence Number & Random Seed Pool (0x805aac..0x805c61)
 * ============================================================================ */

/* 0x805aac (name ours) */
void freeSeqnoNode(void *nodePtr)
{
	SceSkSeqnoNode *node = (SceSkSeqnoNode *)nodePtr;
	uint32_t idx;

	if (node == NULL)
		return;

	idx = (uint32_t)((uintptr_t)node - (uintptr_t)s_seqnoPool.nodes) /
	      (uint32_t)sizeof(SceSkSeqnoNode);
	if (idx >= 4u) {
		setPanicArmInterruptCode(0x00200000u);
		skPanic("%08x\n", 0x21u);
	}

	s_seqnoAllocBitmap &= ~(1u << idx);
	node->next = NULL;
	node->seqno = 0xffffffffu;
	node->randomWords[0] = 0xffffffffu;
	node->randomWords[1] = 0xffffffffu;
	node->randomWords[2] = 0xffffffffu;
	s_seqnoAllocCount--;
}

/* 0x805b1e (name ours) */
uint32_t allocateSeqno(void)
{
	uint32_t seqno = s_nextSeqnoCounter;
	SceSkSeqnoNode *oldHead = s_seqnoListHead;
	uint32_t r2 = s_nextSeqnoCounter;
	SceSkSeqnoNode *newNode = NULL;
	uint32_t i;

	while (r2 == 0u) {
		do {
			do {
				s_rngTempWords[0] = BIGMAC->ch[0].rng;
				s_rngTempWords[1] = BIGMAC->ch[0].rng;
				s_rngTempWords[2] = BIGMAC->ch[0].rng;
			} while (s_rngTempWords[0] == 0u);
			r2 = s_rngTempWords[2];
		} while (s_rngTempWords[1] == 0u);
	}

	if (s_seqnoAllocCount == 4u) {
		s_seqnoListHead = s_seqnoListHead->next;
		freeSeqnoNode(oldHead);
	}

	for (i = 0u; i < 4u; i++) {
		uint32_t bit = 1u << i;

		if ((s_seqnoAllocBitmap & bit) == 0u) {
			newNode = &s_seqnoPool.nodes[i];
			s_seqnoAllocBitmap |= bit;
		}
	}

	if (newNode != NULL) {
		newNode->next = NULL;
		newNode->seqno = seqno;
		memcpy(newNode->randomWords, s_rngTempWords, sizeof(s_rngTempWords));
		s_seqnoAllocCount++;
	}

	if (s_seqnoListHead != NULL) {
		SceSkSeqnoNode *cur = s_seqnoListHead;
		SceSkSeqnoNode *tail;

		do {
			tail = cur;
			cur = tail->next;
		} while (cur != NULL);
		tail->next = newNode;
	} else {
		s_seqnoListHead = newNode;
	}

	s_nextSeqnoCounter++;
	return seqno;
}

/* 0x805bee (name ours) */
int getSeqnoRandomBytes(uint32_t seqno, void *out12)
{
	const SceSkSeqnoNode *cur = s_seqnoListHead;

	while (cur != NULL) {
		if (cur->seqno == seqno) {
			memcpy(out12, cur->randomWords, 12u);
			return 0;
		}
		cur = cur->next;
	}
	return (int)0x800f0303;
}

/* 0x805c24 (name ours) */
int removeAndFreeSeqno(uint32_t seqno)
{
	SceSkSeqnoNode *prev = NULL;
	SceSkSeqnoNode *cur = s_seqnoListHead;

	while (cur != NULL) {
		if (cur->seqno == seqno) {
			if (prev != NULL)
				prev->next = cur->next;
			else
				s_seqnoListHead = cur->next;
			freeSeqnoNode(cur);
			return 0;
		}
		prev = cur;
		cur = cur->next;
	}
	return (int)0x800f0303;
}

/* ============================================================================
 * Bigmac DMAC5 Primitives (0x805c62..0x805e59)
 * ============================================================================ */

/* 0x805c62 (name ours) */
void dmac5StartCmd(uint32_t channel, void *dst, const void *src, uint32_t size,
		uint32_t keySlot, uint32_t ivAddr, uint32_t funcCode, uint32_t memMode)
{
	uint32_t psw;

	__asm__ volatile ("ldc %0, $psw" : "=r"(psw));
	if ((psw & 1u) != 0u) {
		setPanicArmInterruptCode(0x00200000u);
		skPanic("%08x\n", 0x10u);
	}

	if (channel == 0u) {
		BIGMAC->ch[0].src = (uint32_t)(uintptr_t)src;
		BIGMAC->ch[0].dst = (uint32_t)(uintptr_t)dst;
		BIGMAC->ch[0].length = size;
		BIGMAC->ch[0].keySlot = keySlot;
		if (ivAddr != 0u)
			BIGMAC->ch[0].iv = ivAddr;
		BIGMAC->ch[0].func = funcCode;
		BIGMAC->memMode = memMode;
		BIGMAC->ch[0].trigger = 1u;
	} else {
		BIGMAC->ch[1].src = (uint32_t)(uintptr_t)src;
		BIGMAC->ch[1].dst = (uint32_t)(uintptr_t)dst;
		BIGMAC->ch[1].length = size;
		BIGMAC->ch[1].keySlot = keySlot;
		if (ivAddr != 0u)
			BIGMAC->ch[1].iv = ivAddr;
		BIGMAC->ch[1].func = funcCode;
		BIGMAC->memMode = memMode;
		BIGMAC->ch[1].trigger = 1u;
	}
}

/* 0x805d54 (name ours) */
uint32_t dmac5WaitChannel(uint32_t channel)
{
	uint32_t status;

	do {
		status = (channel == 0u) ? BIGMAC->ch[0].status : BIGMAC->ch[1].status;
	} while ((status & 1u) != 0u);

	BIGMAC->memMode = 0u;
	return status;
}

/* 0x805d7e (name ours) */
uint32_t dmac5CopyCh0(void *dst, const void *src, uint32_t size)
{
	uint32_t status;

	dmac5StartCmd(0u, dst, NULL, (uint32_t)(uintptr_t)src, 0u, 0u, 4u, size);
	do {
		status = BIGMAC->ch[0].status;
	} while ((status & 1u) != 0u);

	BIGMAC->memMode = 0u;
	return status;
}

/* 0x805da8 (name ours) */
uint32_t dmac5FillCh0(void *dst, uint32_t fillVal, uint32_t size, uint32_t memMode)
{
	BIGMAC->ch[0].fillValue = fillVal;
	dmac5StartCmd(0u, dst, NULL, size, 0u, 0u, 0x0cu, memMode);
	return dmac5WaitChannel(0u);
}

/* 0x805dde (name ours) */
uint32_t dmac5MemcpyWithModeCh0(void *dst, const void *src, uint32_t size, uint32_t memMode)
{
	uint32_t psw;
	uint32_t status;

	__asm__ volatile ("ldc %0, $psw" : "=r"(psw));
	if ((psw & 1u) != 0u) {
		setPanicArmInterruptCode(0x00200000u);
		skPanic("%08x\n", 0x11u);
	}

	BIGMAC->ch[0].src = (uint32_t)(uintptr_t)src;
	BIGMAC->ch[0].dst = (uint32_t)(uintptr_t)dst;
	BIGMAC->ch[0].length = size;
	BIGMAC->ch[0].func = 0u;
	BIGMAC->memMode = memMode;
	BIGMAC->ch[0].trigger = 1u;

	do {
		status = BIGMAC->ch[0].status;
	} while ((status & 1u) != 0u);

	BIGMAC->memMode = 0u;
	return status;
}

/* ============================================================================
 * Hardware Boot State & SoC Version Checks (0x805e5a..0x805f2b)
 * ============================================================================ */

/* 0x805e5a (name ours) */
int checkHardwareBootStateAndSocVersion(void)
{
	if ((REG32(0xe0020004u) & ~0x10u) == 0u &&
	    REG32(0xe0010004u) == 0x80000005u &&
	    (REG32(0xe0062020u) & ~0x0bu) == 4u) {
		if (getExpectedSocVersion() == readHardwareSocVersion())
			return 1;
		return (int)0x800f0337;
	}
	return (int)0x800f032f;
}

/* 0x805ec4 (name ours) */
int lockKeyringSlot50eIfReady(void)
{
	uint32_t prot;

	REG32(0xe0030028u) = 0x050eu;
	prot = REG32(0xe003002cu);
	if ((prot & 2u) != 0u && (prot & 4u) == 0u) {
		REG32(0xe0030024u) = 0x0800050eu;
		return 1;
	}
	return (int)0x800f032f;
}

/* 0x805f06 (name ours) */
int checkSocVersionMatches(uint32_t version)
{
	if (version == readHardwareSocVersion())
		return 1;
	return (int)0x800f0337;
}
