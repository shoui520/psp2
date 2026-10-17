/*
 * SceSysmem: user memory access primitives.
 *
 * Every routine touches user memory only with unprivileged loads and stores
 * (LDRT/STRT), so a user pointer to kernel memory faults. Before an access the
 * routine points ip at its recovery code; the data abort handler resumes at ip
 * when the faulting PC lies in [VmaccessStart, VmaccessEnd) (see
 * SceCpuForKernel_C32687D0), and the routine returns VMACCESS_ERROR_FAULT.
 * The recovery code first undoes whatever the routine pushed at the time.
 *
 * Assembly because C has no unprivileged access with fault recovery. The
 * names are not Sony's (unknown).
 */

	.syntax unified
	.thumb
	.text

#define VMACCESS_ERROR_FAULT	0xc0022005

	.balign 4
	.global VmaccessStart
VmaccessStart:

/* int VmaccessCopyFromUser(void *dst, const void *userSrc, SceSize size) */
	.global VmaccessCopyFromUser
	.type VmaccessCopyFromUser, %function
	.thumb_func
VmaccessCopyFromUser:
	cbz	r2, 2f
	adr.w	ip, .Lfault
1:	lsls	r3, r0, #30		/* bytes until dst is word aligned */
	beq	4f
	ldrbt	r3, [r1]
	adds	r1, #1
	subs	r2, #1
	strb.w	r3, [r0], #1
	bne	1b
2:	movs	r0, #0
	bx	lr
3:	subs	r2, #4			/* words until dst is 32-byte aligned */
	bcc	7f
	ldrt	r3, [r1]
	adds	r1, #4
	stmia	r0!, {r3}
	cbz	r2, 9f
4:	lsls	r3, r0, #27
	bne	3b
	subs	r2, #32
	bcc	6f
	push.w	{r4, r5, r6, r7, r8, r9, sl}
	adr.w	ip, .Lfault7
5:	ldrt	r3, [r1]		/* 32 bytes at a time */
	ldrt	r4, [r1, #4]
	ldrt	r5, [r1, #8]
	ldrt	r6, [r1, #12]
	ldrt	r7, [r1, #16]
	ldrt	r8, [r1, #20]
	ldrt	r9, [r1, #24]
	ldrt	sl, [r1, #28]
	adds	r1, #32
	subs	r2, #32
	stmia.w	r0!, {r3, r4, r5, r6, r7, r8, r9, sl}
	bcs	5b
	pop.w	{r4, r5, r6, r7, r8, r9, sl}
	adr.w	ip, .Lfault
6:	adds	r2, #28
7:	adds	r2, #4
	beq	9f
8:	ldrbt	r3, [r1]		/* the tail */
	adds	r1, #1
	subs	r2, #1
	strb.w	r3, [r0], #1
	bne	8b
9:	movs	r0, #0
	bx	lr
	.size VmaccessCopyFromUser, . - VmaccessCopyFromUser

/* int VmaccessCopyToUser(void *userDst, const void *src, SceSize size) */
	.global VmaccessCopyToUser
	.type VmaccessCopyToUser, %function
	.thumb_func
VmaccessCopyToUser:
	cbz	r2, 2f
	adr.w	ip, .Lfault
1:	lsls	r3, r1, #30		/* bytes until src is word aligned */
	beq	4f
	ldrb.w	r3, [r1], #1
	strbt	r3, [r0]
	adds	r0, #1
	subs	r2, #1
	bne	1b
2:	movs	r0, #0
	bx	lr
3:	subs	r2, #4
	bcc	7f
	ldmia	r1!, {r3}
	strt	r3, [r0]
	adds	r0, #4
	cbz	r2, 9f
4:	lsls	r3, r1, #27
	bne	3b
	subs	r2, #32
	bcc	6f
	push.w	{r4, r5, r6, r7, r8, r9, sl}
	adr.w	ip, .Lfault7
5:	ldmia.w	r1!, {r3, r4, r5, r6, r7, r8, r9, sl}
	strt	r3, [r0]
	strt	r4, [r0, #4]
	strt	r5, [r0, #8]
	strt	r6, [r0, #12]
	strt	r7, [r0, #16]
	strt	r8, [r0, #20]
	strt	r9, [r0, #24]
	strt	sl, [r0, #28]
	adds	r0, #32
	subs	r2, #32
	bcs	5b
	pop.w	{r4, r5, r6, r7, r8, r9, sl}
	adr.w	ip, .Lfault
6:	adds	r2, #28
7:	adds	r2, #4
	beq	9f
8:	ldrb.w	r3, [r1], #1
	strbt	r3, [r0]
	adds	r0, #1
	subs	r2, #1
	bne	8b
9:	movs	r0, #0
	bx	lr
	.size VmaccessCopyToUser, . - VmaccessCopyToUser

/* int VmaccessFillUser32(SceUInt32 *userDst, SceUInt32 value, SceSize count) */
	.global VmaccessFillUser32
	.type VmaccessFillUser32, %function
	.thumb_func
VmaccessFillUser32:
	cbz	r2, 2f
	adr.w	ip, .Lfault
1:	strt	r1, [r0]
	adds	r0, #4
	subs	r2, #1
	bne	1b
2:	movs	r0, #0
	bx	lr
	.size VmaccessFillUser32, . - VmaccessFillUser32

/* int VmaccessCopyUserToUser(void *userDst, const void *userSrc, SceSize size) */
	.global VmaccessCopyUserToUser
	.type VmaccessCopyUserToUser, %function
	.thumb_func
VmaccessCopyUserToUser:
	cbz	r2, 2f
	adr.w	ip, .Lfault
1:	lsls	r3, r1, #30
	beq	4f
	ldrbt	r3, [r1]
	strbt	r3, [r0]
	adds	r1, #1
	adds	r0, #1
	subs	r2, #1
	bne	1b
2:	movs	r0, #0
	bx	lr
3:	subs	r2, #4
	bcc	7f
	ldrt	r3, [r1]
	strt	r3, [r0]
	adds	r1, #4
	adds	r0, #4
	cbz	r2, 9f
4:	lsls	r3, r1, #27
	bne	3b
	subs	r2, #32
	bcc	6f
	push.w	{r4, r5, r6, r7, r8, r9, sl}
	adr.w	ip, .Lfault7
5:	ldrt	r3, [r1]
	ldrt	r4, [r1, #4]
	ldrt	r5, [r1, #8]
	ldrt	r6, [r1, #12]
	ldrt	r7, [r1, #16]
	ldrt	r8, [r1, #20]
	ldrt	r9, [r1, #24]
	ldrt	sl, [r1, #28]
	strt	r3, [r0]
	strt	r4, [r0, #4]
	strt	r5, [r0, #8]
	strt	r6, [r0, #12]
	strt	r7, [r0, #16]
	strt	r8, [r0, #20]
	strt	r9, [r0, #24]
	strt	sl, [r0, #28]
	adds	r1, #32
	adds	r0, #32
	subs	r2, #32
	bcs	5b
	pop.w	{r4, r5, r6, r7, r8, r9, sl}
	adr.w	ip, .Lfault
6:	adds	r2, #28
7:	adds	r2, #4
	beq	9f
8:	ldrbt	r3, [r1]
	strbt	r3, [r0]
	adds	r1, #1
	adds	r0, #1
	subs	r2, #1
	bne	8b
9:	movs	r0, #0
	bx	lr
	.size VmaccessCopyUserToUser, . - VmaccessCopyUserToUser

/* SceSize VmaccessStrnlenUser(const char *userStr, SceSize max) */
	.global VmaccessStrnlenUser
	.type VmaccessStrnlenUser, %function
	.thumb_func
VmaccessStrnlenUser:
	adr.w	ip, .Lfault
	mov	r2, r0
	cbz	r1, 2f
1:	ldrbt	r3, [r0]
	cbz	r3, 2f
	adds	r0, #1
	subs	r1, #1
	bne	1b
2:	subs	r0, r0, r2
	bx	lr
	.size VmaccessStrnlenUser, . - VmaccessStrnlenUser

/*
 * SceSize VmaccessStrncpyFromUser(char *dst, const char *userSrc, SceSize n):
 * strncpy, the rest of dst zeroed; returns the string's length
 */
	.global VmaccessStrncpyFromUser
	.type VmaccessStrncpyFromUser, %function
	.thumb_func
VmaccessStrncpyFromUser:
	adr.w	ip, .Lfault
	push	{r4, lr}
	adr.w	ip, .Lfault1
	mov	r4, r0
	cbz	r2, 2f
1:	ldrbt	r3, [r1]
	adds	r1, #1
	strb	r3, [r4]
	cbz	r3, 4f
	adds	r4, #1
	subs	r2, #1
	bne	1b
2:	subs	r0, r4, r0
3:	pop	{r4, pc}
4:	subs	r0, r4, r0
	adds	r4, #1		/* zero the rest; r3 is 0 */
5:	subs	r2, #1
	beq	3b
	strb.w	r3, [r4], #1
	b	5b
	.size VmaccessStrncpyFromUser, . - VmaccessStrncpyFromUser

/* SceSize VmaccessStrncpyToUser(char *userDst, const char *src, SceSize n) */
	.global VmaccessStrncpyToUser
	.type VmaccessStrncpyToUser, %function
	.thumb_func
VmaccessStrncpyToUser:
	adr.w	ip, .Lfault
	push	{r4, lr}
	adr.w	ip, .Lfault1
	mov	r4, r0
	cbz	r2, 2f
1:	ldrb.w	r3, [r1], #1
	strbt	r3, [r4]
	cbz	r3, 4f
	adds	r4, #1
	subs	r2, #1
	bne	1b
2:	subs	r0, r4, r0
3:	pop	{r4, pc}
4:	subs	r0, r4, r0
	adds	r4, #1
5:	subs	r2, #1
	beq	3b
	strbt	r3, [r4]
	adds	r4, #1
	b	5b
	.size VmaccessStrncpyToUser, . - VmaccessStrncpyToUser

/* SceSize VmaccessCountFillUser32(const SceUInt32 *userSrc, SceUInt32 value, SceSize size) */
	.global VmaccessCountFillUser32
	.type VmaccessCountFillUser32, %function
	.thumb_func
VmaccessCountFillUser32:
	adr.w	ip, .Lfault
	push	{r4, lr}
	adr.w	ip, .Lfault1
	mov	r4, r0
	cbz	r2, 2f
1:	ldrt	r3, [r0]
	cmp	r3, r1
	bne	2f
	adds	r0, #4
	subs	r2, #4
	bne	1b
2:	subs	r0, r0, r4
	pop	{r4, pc}
	.size VmaccessCountFillUser32, . - VmaccessCountFillUser32

/*
 * SceSize VmaccessCountFillUser64(const SceUInt64 *userSrc, SceUInt32 low,
 *				    SceUInt32 high, SceSize size)
 */
	.global VmaccessCountFillUser64
	.type VmaccessCountFillUser64, %function
	.thumb_func
VmaccessCountFillUser64:
	adr.w	ip, .Lfault
	push	{r4, r5, lr}
	adr.w	ip, .Lfault2
	mov	r4, r0
	cbz	r3, 2f
1:	ldrt	r5, [r0]
	cmp	r5, r1
	bne	2f
	ldrt	r5, [r0, #4]
	cmp	r5, r2
	bne	2f
	adds	r0, #8
	subs	r3, #8
	bne	1b
2:	subs	r0, r0, r4
	pop	{r4, r5, pc}
	.size VmaccessCountFillUser64, . - VmaccessCountFillUser64

/*
 * The user-space lock words below: { lock, contention count, owner info }.
 * The process's domain access (DACR) is switched in while they are touched,
 * every domain demoted to client so the page permissions apply.
 */

/*
 * int SceCpuForKernel_75A85059(SceUInt32 *userLock, SceUInt32 owner):
 * takes the lock (0) or counts the contention (1)
 */
	.global SceCpuForKernel_75A85059
	.type SceCpuForKernel_75A85059, %function
	.thumb_func
SceCpuForKernel_75A85059:
	adr.w	ip, .Lfault
	push	{r4, lr}
	adr.w	ip, .Lfault1
	mrc	p15, 0, r2, c13, c0, 4
	ldr	r2, [r2, #12]
	ldr	r3, [r2, #68]
	and	r3, r3, #0x55555555
	adr.w	ip, .Lfault_dacr_lock
	mrc	p15, 0, lr, c3, c0, 0
	mcr	p15, 0, r3, c3, c0, 0
	ldrt	r2, [r0]		/* fault both words in */
	ldrt	r2, [r0, #4]
	dmb	sy
1:	ldrexd	r2, r3, [r0]
	cbnz	r2, 3f
	strex	r4, r1, [r0]		/* free: take it */
	cmp	r4, #0
	bne	1b
	dmb	sy
2:	movs	r0, r4
.Ldacr_restore_lock:
	mcr	p15, 0, lr, c3, c0, 0
	pop	{r4, pc}
3:	adds	r3, #1			/* held: count the contention */
	strexd	r4, r2, r3, [r0]
	rsbs	r4, r4, #1
	bne	2b
	b	1b
	.size SceCpuForKernel_75A85059, . - SceCpuForKernel_75A85059

/* int SceCpuForKernel_FE9ED689(SceUInt32 *userLock): releases the lock */
	.global SceCpuForKernel_FE9ED689
	.type SceCpuForKernel_FE9ED689, %function
	.thumb_func
SceCpuForKernel_FE9ED689:
	adr.w	ip, .Lfault
	push	{r4, lr}
	adr.w	ip, .Lfault1
	mrc	p15, 0, r2, c13, c0, 4
	ldr	r2, [r2, #12]
	ldr	r3, [r2, #68]
	and	r3, r3, #0x55555555
	adr.w	ip, .Lfault_dacr_lock
	mrc	p15, 0, lr, c3, c0, 0
	mcr	p15, 0, r3, c3, c0, 0
	movs	r1, #0
	strt	r1, [r0, #8]
	dmb	sy
	strt	r1, [r0]
	movs	r0, #0
	mcr	p15, 0, lr, c3, c0, 0
	pop	{r4, pc}
	.size SceCpuForKernel_FE9ED689, . - SceCpuForKernel_FE9ED689

/*
 * int SceCpuForKernel_9B071FD3(SceUInt32 *userLock, SceUInt32 lock,
 *				SceUInt32 count, SceUInt32 owner): sets all three words
 */
	.global SceCpuForKernel_9B071FD3
	.type SceCpuForKernel_9B071FD3, %function
	.thumb_func
SceCpuForKernel_9B071FD3:
	adr.w	ip, .Lfault
	push	{r4, r5, r6, lr}
	adr.w	ip, .Lfault3
	mrc	p15, 0, r4, c13, c0, 4
	ldr	r4, [r4, #12]
	ldr	r5, [r4, #68]
	and	r5, r5, #0x55555555
	adr.w	ip, .Lfault_dacr_set
	mrc	p15, 0, lr, c3, c0, 0
	mcr	p15, 0, r5, c3, c0, 0
	ldrt	r4, [r0]		/* fault the words in */
	strt	r3, [r0, #8]
	dmb	sy
	strd	r1, r2, [r0]
	dmb	sy
	movs	r0, #0
.Ldacr_restore_set:
	mcr	p15, 0, lr, c3, c0, 0
	pop	{r4, r5, r6, pc}

	.global VmaccessEnd
VmaccessEnd:

/*
 * recovery: undo the pushes, return the fault error. It has no unwind entry of
 * its own; SceCpuForKernel_9B071FD3's covers it.
 */
.Lfault1:
	pop.w	{r4, lr}
	b	.Lfault
.Lfault2:
	pop.w	{r4, r5, lr}
	b	.Lfault
.Lfault3:
	pop.w	{r4, r5, r6, lr}
	b	.Lfault
.Lfault7:
	pop.w	{r4, r5, r6, r7, r8, r9, sl}
.Lfault:
	movw	r0, #:lower16:VMACCESS_ERROR_FAULT
	movt	r0, #:upper16:VMACCESS_ERROR_FAULT
	bx	lr
.Lfault_dacr_lock:
	movw	r0, #:lower16:VMACCESS_ERROR_FAULT
	movt	r0, #:upper16:VMACCESS_ERROR_FAULT
	b	.Ldacr_restore_lock
.Lfault_dacr_set:
	movw	r0, #:lower16:VMACCESS_ERROR_FAULT
	movt	r0, #:upper16:VMACCESS_ERROR_FAULT
	b	.Ldacr_restore_set
	.size SceCpuForKernel_9B071FD3, . - SceCpuForKernel_9B071FD3
