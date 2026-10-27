/*
 * sm_runtime.c: Secure Module (SM) runtime service loop, ARM<->MeP hardware
 * mailbox interrupt handler (0xe0000014 / 0xe0000004), physical address range
 * validator, software interrupt (swi 0) syscall wrappers, and MeP watchdog
 * control bus helpers (0x80e7ee..0x80eb9e).
 *
 * Function and structure names are ours.
 */

#include "kprx_auth_sm.h"

/* ============================================================================
 * .data (0x81305c..0x813060) & .bss (0x814480..0x815480)
 * ============================================================================ */

/* 0x81305c: active SM service context pointer */
static SmServiceContext *s_smSrvCtxPtr = NULL;

/* 0x814480..0x815480: 4 KB local mailbox command packet buffer */
static uint8_t s_smCommPacketBuf[0x1000]
	__attribute__((section(".bss"), aligned(128)));

/* ============================================================================
 * Mailbox Validation & Interrupt Handler (0x80e7ee..0x80e990)
 * ============================================================================ */

/* 0x80e7ee (name ours) */
int smValidateMailboxRange(uint32_t addr, uint32_t size)
{
	union {
		uint8_t b[4];
		uint32_t w;
	} dramBase;

	dramBase.b[0] = REG8(0xe0062260u);
	dramBase.b[1] = REG8(0xe0062261u);
	dramBase.b[2] = REG8(0xe0062262u);
	dramBase.b[3] = REG8(0xe0062263u);

	if (addr >= 0x40200000u) {
		uint64_t dramEnd = (uint64_t)dramBase.w + 0x40000000ULL;

		if ((uint64_t)addr < dramEnd) {
			uint64_t reqEnd = (uint64_t)addr + (uint64_t)size;

			if (reqEnd <= dramEnd)
				return 1;
		}
	} else if (addr < 0x1f000000u) {
		return 0;
	}

	if (addr <= 0x1f007fffu) {
		uint32_t reqEnd = addr + size;

		if (reqEnd >= size && reqEnd <= 0x1f008000u)
			return 1;
	}

	if (addr >= 0x1f840000u && addr <= 0x1f85ffffu) {
		uint32_t reqEnd = addr + size;

		return (reqEnd >= size && reqEnd <= 0x1f860000u);
	}

	return 0;
}

/* 0x80e8c2 (name ours) */
void smMailboxIrqHandler(void)
{
	volatile uint32_t * const mbIn  = (volatile uint32_t *)(uintptr_t)0xe0000014u;
	volatile uint32_t * const mbOut = (volatile uint32_t *)(uintptr_t)0xe0000004u;
	uint32_t rawIn = *mbIn;
	uint32_t pktAddr;
	uint32_t pktSize;
	uint32_t copySize;
	uint32_t cmdId;
	uint32_t respFlags;
	SmServiceHookNode *node;

	if ((rawIn & 1u) == 0u)
		return;

	*mbIn = rawIn;
	pktAddr = rawIn & ~1u;

	if (smValidateMailboxRange(pktAddr, 64u) == 0) {
		respFlags = 4u;
		goto send_resp;
	}

	pktSize = *(const uint32_t *)(uintptr_t)pktAddr;
	if (pktSize < 64u || pktSize > 0x1000u ||
	    smValidateMailboxRange(pktAddr, pktSize) == 0) {
		respFlags = 4u;
		goto send_resp;
	}

	if (bigmacMemcpy(s_smCommPacketBuf, (const void *)(uintptr_t)pktAddr, pktSize) != 0) {
		respFlags = 8u;
		goto send_resp;
	}

	copySize = *(const uint32_t *)s_smCommPacketBuf;
	if (copySize != pktSize) {
		respFlags = 4u;
		goto send_resp;
	}

	cmdId = *(const uint32_t *)(s_smCommPacketBuf + 4u);
	if (cmdId == 0xffffffffu) {
		s_smSrvCtxPtr->running = 0u;
		return;
	}

	for (node = s_smSrvCtxPtr->head; node != NULL; ) {
		SmServiceHookNode *cur = (SmServiceHookNode *)((uintptr_t)node & ~1u);

		if (cur->cmdId == cmdId) {
			void (*invokeFn)(void *, void *) =
				*(void (**)(void *, void *))cur->vtable;

			invokeFn(&cur->vtable, s_smCommPacketBuf);
			if (bigmacMemcpy((void *)(uintptr_t)pktAddr,
					s_smCommPacketBuf, copySize) != 0) {
				respFlags = 8u;
				goto send_resp;
			}
			smMemBarrier();
			respFlags = 0u;
			goto send_resp;
		}
		node = cur->next;
	}

	respFlags = 2u;

send_resp:
	*mbOut = respFlags | 1u;
}

/* ============================================================================
 * SM Service Context & Hook Node Pool (0x80e990..0x80eb2a)
 * ============================================================================ */

/* 0x80e990 (name ours) */
void smSrvInitZero(SmServiceContext *ctx)
{
	ctx->running = 0u;
	ctx->head = NULL;
	ctx->allocMask = 0u;
	ctx->poolBase = ctx->nodes;
	s_smSrvCtxPtr = ctx;
	ctx->poolSize = sizeof(ctx->nodes);
}

/* 0x80e9b2 (name ours) */
void smSrvInit(SmServiceContext *ctx)
{
	ctx->running = 0u;
	ctx->head = NULL;
	ctx->allocMask = 0u;
	ctx->poolBase = ctx->nodes;
	s_smSrvCtxPtr = ctx;
	ctx->poolSize = sizeof(ctx->nodes);
}

/* 0x80e9d4 (name ours) */
SmServiceContext *smSrvGetContext(void)
{
	return s_smSrvCtxPtr;
}

/* 0x80e9da (name ours) */
SmServiceHookNode *smSrvAllocNode(SmServiceContext *ctx)
{
	SmServiceHookNode *node = ctx->poolBase;
	SmServiceHookNode *end = (SmServiceHookNode *)((uint8_t *)node + ctx->poolSize);
	uint32_t bitIdx = 0u;

	while (node < end) {
		uint32_t bit = 1u << bitIdx;

		if ((ctx->allocMask & bit) == 0u) {
			ctx->allocMask |= bit;
			return node;
		}
		bitIdx++;
		node++;
	}
	return NULL;
}

/* 0x80ea0e (name ours) */
int smSrvFreeNode(SmServiceContext *ctx, SmServiceHookNode *node)
{
	uintptr_t base = (uintptr_t)ctx->poolBase;
	uintptr_t end = base + ctx->poolSize;
	uintptr_t addr = (uintptr_t)node;

	if (addr < end && addr >= base) {
		int idx = (int)(addr - base) / (int)sizeof(SmServiceHookNode);

		ctx->allocMask &= ~(1u << idx);
		return 0;
	}
	return -1;
}

/* 0x80ea52 (name ours) */
void smSrvTeardown(SmServiceContext *ctx)
{
	while (ctx->head != NULL)
		smSrvUnlinkNode(ctx, ctx->head);
	s_smSrvCtxPtr = NULL;
}

/* 0x80ea76 (name ours) */
void smSrvTeardownAll(SmServiceContext *ctx)
{
	while (ctx->head != NULL)
		smSrvUnlinkNode(ctx, ctx->head);
	s_smSrvCtxPtr = NULL;
}

/* 0x80ea9a (name ours) */
int smSrvRunLoop(SmServiceContext *ctx)
{
	uint32_t irqCookie = 0u;
	int status;

	ctx->running = 1u;
	status = smSyscall4(9u, (uint32_t)(uintptr_t)smMailboxIrqHandler,
			(uint32_t)(uintptr_t)&irqCookie);
	if (status == 1) {
		while (*(volatile uint32_t *)&ctx->running == 1u)
			;
		status = smSyscall4(9u, 0u, (uint32_t)(uintptr_t)&irqCookie);
	}
	return status;
}

/* 0x80eada (name ours) */
int smSrvUnlinkNode(SmServiceContext *ctx, SmServiceHookNode *target)
{
	SmServiceHookNode **prevLink;
	SmServiceHookNode *cur;

	target->next = (SmServiceHookNode *)((uintptr_t)target->next | 1u);
	prevLink = &ctx->head;
	cur = ctx->head;

	while (cur != NULL) {
		SmServiceHookNode *node = (SmServiceHookNode *)((uintptr_t)cur & ~1u);
		SmServiceHookNode *next = node->next;

		if (((uintptr_t)next & 1u) != 0u) {
			next = (SmServiceHookNode *)((uintptr_t)next & ~1u);
			if (node == target) {
				*prevLink = next;
				smSrvFreeNode(ctx, target);
				return 0;
			}
		} else {
			prevLink = &node->next;
		}
		cur = next;
	}
	return (int)0x800f0016;
}

/* ============================================================================
 * Memory Barrier, Syscalls & Watchdog Helpers (0x80eb2a..0x80eb9e)
 * ============================================================================ */

/* 0x80eb2a (name ours) */
void smMemBarrier(void)
{
	__asm__ volatile("syncm" ::: "memory");
}

/* 0x80eb2e (name ours) */
int smSyscall1(uint32_t a1, uint32_t a2, uint32_t a3)
{
	register uint32_t r1 __asm__("$1") = a1;
	register uint32_t r2 __asm__("$2") = a2;
	register uint32_t r3 __asm__("$3") = a3;
	register uint32_t r4 __asm__("$4") = 1u;
	register int r0 __asm__("$0");

	__asm__ volatile("swi 0x0"
		: "=r"(r0)
		: "r"(r1), "r"(r2), "r"(r3), "r"(r4)
		: "memory");
	return r0;
}

/* 0x80eb34 (name ours) */
int smSyscall2(uint32_t a1, uint32_t a2, uint32_t a3)
{
	register uint32_t r1 __asm__("$1") = a1;
	register uint32_t r2 __asm__("$2") = a2;
	register uint32_t r3 __asm__("$3") = a3;
	register uint32_t r4 __asm__("$4") = 2u;
	register int r0 __asm__("$0");

	__asm__ volatile("swi 0x0"
		: "=r"(r0)
		: "r"(r1), "r"(r2), "r"(r3), "r"(r4)
		: "memory");
	return r0;
}

/* 0x80eb3a (name ours) */
int smSyscall3(uint32_t a1, uint32_t a2, uint32_t a3)
{
	register uint32_t r1 __asm__("$1") = a1;
	register uint32_t r2 __asm__("$2") = a2;
	register uint32_t r3 __asm__("$3") = a3;
	register uint32_t r4 __asm__("$4") = 3u;
	register int r0 __asm__("$0");

	__asm__ volatile("swi 0x0"
		: "=r"(r0)
		: "r"(r1), "r"(r2), "r"(r3), "r"(r4)
		: "memory");
	return r0;
}

/* 0x80eb40 (name ours) */
int smSyscall4(uint32_t a1, uint32_t a2, uint32_t a3)
{
	register uint32_t r1 __asm__("$1") = a1;
	register uint32_t r2 __asm__("$2") = a2;
	register uint32_t r3 __asm__("$3") = a3;
	register uint32_t r4 __asm__("$4") = 4u;
	register int r0 __asm__("$0");

	__asm__ volatile("swi 0x0"
		: "=r"(r0)
		: "r"(r1), "r"(r2), "r"(r3), "r"(r4)
		: "memory");
	return r0;
}

/* 0x80eb46 (name ours) */
int smSyscall5(uint32_t a1, uint32_t a2, uint32_t a3)
{
	register uint32_t r1 __asm__("$1") = a1;
	register uint32_t r2 __asm__("$2") = a2;
	register uint32_t r3 __asm__("$3") = a3;
	register uint32_t r4 __asm__("$4") = 5u;
	register int r0 __asm__("$0");

	__asm__ volatile("swi 0x0"
		: "=r"(r0)
		: "r"(r1), "r"(r2), "r"(r3), "r"(r4)
		: "memory");
	return r0;
}

/* 0x80eb4c (name ours) */
int smSyscall6(uint32_t a1, uint32_t a2, uint32_t a3)
{
	register uint32_t r1 __asm__("$1") = a1;
	register uint32_t r2 __asm__("$2") = a2;
	register uint32_t r3 __asm__("$3") = a3;
	register uint32_t r4 __asm__("$4") = 6u;
	register int r0 __asm__("$0");

	__asm__ volatile("swi 0x0"
		: "=r"(r0)
		: "r"(r1), "r"(r2), "r"(r3), "r"(r4)
		: "memory");
	return r0;
}

/* 0x80eb52 (name ours) */
int smSyscall7(uint32_t a1, uint32_t a2, uint32_t a3)
{
	register uint32_t r1 __asm__("$1") = a1;
	register uint32_t r2 __asm__("$2") = a2;
	register uint32_t r3 __asm__("$3") = a3;
	register uint32_t r4 __asm__("$4") = 7u;
	register int r0 __asm__("$0");

	__asm__ volatile("swi 0x0"
		: "=r"(r0)
		: "r"(r1), "r"(r2), "r"(r3), "r"(r4)
		: "memory");
	return r0;
}

/* 0x80eb58 (name ours) */
int smSyscall8(uint32_t a1, uint32_t a2, uint32_t a3)
{
	register uint32_t r1 __asm__("$1") = a1;
	register uint32_t r2 __asm__("$2") = a2;
	register uint32_t r3 __asm__("$3") = a3;
	register uint32_t r4 __asm__("$4") = 8u;
	register int r0 __asm__("$0");

	__asm__ volatile("swi 0x0"
		: "=r"(r0)
		: "r"(r1), "r"(r2), "r"(r3), "r"(r4)
		: "memory");
	return r0;
}

/* 0x80eb5e (name ours) */
void smWatchdogStart(void)
{
	smWatchdogStop();
	__asm__ volatile(
		"mov $12, 32\n\t"
		"stcb $12, 0x413\n\t"
		"mov $12, 5\n\t"
		"stcb $12, 0x415\n\t"
		"mov $12, 1\n\t"
		"stcb $12, 0x410\n\t"
		"mov $12, 0\n\t"
		"stcb $12, 0x411\n\t"
		"mov $12, 1\n\t"
		"stcb $12, 0x412"
		::: "$12", "memory");
}

/* 0x80eb84 (name ours) */
void smWatchdogStop(void)
{
	uint32_t enabled;

	__asm__ volatile("ldcb %0, 0x412" : "=r"(enabled));
	if (enabled != 0u) {
		uint32_t zero = 0u;

		__asm__ volatile("stcb %0, 0x412" :: "r"(zero) : "memory");
	}
}

/* 0x80eb92 (name ours) */
uint32_t smWatchdogGetControl(void)
{
	uint32_t val;

	__asm__ volatile("ldcb %0, 0x410" : "=r"(val));
	return val;
}

/* 0x80eb98 (name ours) */
uint32_t smWatchdogGetCount(void)
{
	uint32_t val;

	__asm__ volatile("ldcb %0, 0x415" : "=r"(val));
	return val;
}
