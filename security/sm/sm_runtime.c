/*
 * sm_runtime.c: Shared Secure Module (SM) runtime service loop, ARM<->MeP
 * hardware mailbox interrupt handler (0xe0000014 / 0xe0000004), and physical
 * address range validator for os0:sm modules.
 *
 * Function and structure names are ours.
 */

#include "sm_common.h"

/* Active SM service context pointer (.data) */
static SmServiceContext *s_smSrvCtxPtr = NULL;

/* 4 KB local mailbox command packet buffer (.bss, 128-byte aligned) */
static uint8_t s_smCommPacketBuf[0x1000]
	__attribute__((section(".bss.comm"), aligned(128)));

/* Validate physical mailbox buffer range (name ours) */
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

/* Mailbox interrupt handler registered via syscall 4 (name ours) */
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

	copySize = *(const uint32_t *)(const void *)s_smCommPacketBuf;
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

/* Initialize service context (C2 constructor, name ours) */
void smSrvInitZero(SmServiceContext *ctx)
{
	ctx->running = 0u;
	ctx->head = NULL;
	ctx->allocMask = 0u;
	ctx->poolBase = ctx->nodes;
	s_smSrvCtxPtr = ctx;
	ctx->poolSize = sizeof(ctx->nodes);
}

/* Initialize service context (C1 constructor, name ours) */
void smSrvInit(SmServiceContext *ctx)
{
	ctx->running = 0u;
	ctx->head = NULL;
	ctx->allocMask = 0u;
	ctx->poolBase = ctx->nodes;
	s_smSrvCtxPtr = ctx;
	ctx->poolSize = sizeof(ctx->nodes);
}

/* Return active service context pointer (name ours) */
SmServiceContext *smSrvGetContext(void)
{
	return s_smSrvCtxPtr;
}

/* Allocate a command hook node from the 16-entry pool (name ours) */
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

/* Release a command hook node back to the 16-entry pool (name ours) */
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

/* Unlink all command hook nodes (D2 destructor, name ours) */
void smSrvTeardown(SmServiceContext *ctx)
{
	while (ctx->head != NULL)
		smSrvUnlinkNode(ctx, ctx->head);
	s_smSrvCtxPtr = NULL;
}

/* Unlink all command hook nodes (D1 destructor, name ours) */
void smSrvTeardownAll(SmServiceContext *ctx)
{
	while (ctx->head != NULL)
		smSrvUnlinkNode(ctx, ctx->head);
	s_smSrvCtxPtr = NULL;
}

/* Register mailbox interrupt handler and spin until stop command (name ours) */
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

/* Unlink and free a specific command hook node (name ours) */
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
	return SCE_SBL_ERROR_INVALID_VALUE;
}

/* MeP memory barrier (name ours) */
void smMemBarrier(void)
{
	__asm__ volatile("syncm" ::: "memory");
}
