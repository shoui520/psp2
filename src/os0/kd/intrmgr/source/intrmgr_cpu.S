/* CPU exception and context boundaries. Internal names are unknown.
 *
 * C owns GIC/stack/scheduler policy and stored CP/debug/syscall state. These
 * veneers retain only the custom register ABI: capture live GPR/VFP state,
 * access banked SP/LR/SPSR, switch modes/stacks, and perform exception return.
 * VFP image selection stays with the restores because a C call may clobber
 * caller-saved VFP registers and cannot return with an arbitrary live image.
 * Vector patch words and instrumentation branch slots are writable ABI data.
 */
.syntax unified
.arch armv7-a
.arch_extension sec
.fpu neon
.section .text.intrmgr_cpu,"ax",%progbits
.balign 32
.arm
.LcaptureNestedInterrupt:
	/* The C policy selected a nested level. Form its banked System stack
	 * here, where changing SP and capturing the live register image belong.
	 * r3 holds nesting minus two and r4 the current CPU index.
	 */
	add	r3, r3, #1
	movw r2, #:lower16:g_intrStackGuard
	movt r2, #:upper16:g_intrStackGuard
	sub	r2, r2, r4, lsl #17
	sub	r4, r2, r3, lsl #13
	ldrd	r2, [r0, #56]
	stmdb	r4!, {r2, r3}
	stmdb	r4!, {sp, lr}
	mov	sp, r4
	push	{r5, r6, r7, r8, r9, sl, fp, ip}
	vpush	{d16-d31}
	vpush	{d0-d15}
	vmrs	r5, fpexc
	vmrs	r6, fpscr
	push	{r5, r6}
	ldr	r2, [r0, #16]
	b	.LdispatchInterrupt
.LrestoreNestedInterrupt:
	/* Restore the live nested register image before returning through IRQ. */
	pop	{r0, r1}
	vmsr	fpexc, r0
	vmsr	fpscr, r1
	vpop	{d0-d15}
	vpop	{d16-d31}
	mrc p15, 0, ip, c12, c0, 0
	ldr	ip, [ip, #36]
	mov	lr, #1
	str	lr, [ip, #256]
	pop	{r5, r6, r7, r8, r9, sl, fp, ip}
	ldmfd	sp!, {r1}
	ldmfd	sp!, {lr}
	mov	sp, r1
	cps	#18
	/* Restore the banked IRQ address-space image without a C call changing
	 * the live return registers. The four words are IntrIrqSystem fields. */
	ldm	sp, {r0, r1, r2, r3}
	mcr p15, 0, r1, c3, c0, 0
	mcr p15, 0, r3, c7, c4, 0
	mov	r1, #0
	mcr p15, 0, r1, c13, c0, 1
	isb
	mcr p15, 0, r0, c2, c0, 1
	isb
	mcr p15, 0, r2, c13, c0, 1
	add	sp, sp, #32
	pop	{r0, r1, r2, r3, r4, ip}
	clrex
	rfeia	sp!
.LreturnUnhandledInterrupt:
	pop	{r0, r1, r2, r3, r4, ip}
	add	sp, sp, #8
	movs	pc, lr
	nop
.global intrEnterHook
.type intrEnterHook, %object
intrEnterHook:
	push	{r0, r1, r4, ip}
	ldr	r2, [sp, #40]
	blx	intrCallEnterHook
	pop	{r0, r1, r4, ip}
	b	.LcaptureInterruptSystem
.global intrLeaveHook
.type intrLeaveHook, %object
intrLeaveHook:
	push	{r0, ip}
	mov	r1, ip
	blx	intrCallLeaveHook
	pop	{r0, ip}
	b	.LcompleteInterrupt
.global interruptEntry
.type interruptEntry, %function
interruptEntry:
	/* Exception manager installs the vector veneer here. */
	.word 0, 0
	sub	lr, lr, #4
	sub	sp, sp, #4
	push	{r0, r1, r2, r3, r4, ip, lr}
	/* Preserve the interrupted return PC while C validates the GIC token. */
	add	r0, sp, #28
	blx	intrLookupInterrupt
	ldr	ip, [sp, #28]
	ldr	lr, [sp, #24]
	cmp	r0, #0
	beq	.LreturnUnhandledInterrupt
	mov	r1, r0
	ubfx	r0, ip, #0, #10
.global intrEnterBranch
.type intrEnterBranch, %object
intrEnterBranch:
	nop
.LcaptureInterruptSystem:
	/* Capture the pre-switch CP state and interrupted banked registers. */
	sub	sp, sp, #8
	mrc p15, 0, lr, c13, c0, 4
	mrc p15, 0, r0, c9, c12, 1
	mcr p15, 0, r0, c9, c12, 2
	stmfd	sp!, {r0}
	mrc p15, 0, r4, c7, c4, 0
	mrc p15, 0, r3, c13, c0, 1
	mrc p15, 0, r2, c3, c0, 0
	mrc p15, 0, r0, c2, c0, 1
	push	{r0, r2, r3, r4, lr}
	mov	r0, #0
	push	{r0, r1, ip, lr}
	blx	intrDisableKernelDebug
	pop	{r0, r1, ip, lr}
	push	{r1, ip}
	mov	r0, ip
	blx	intrSwitchKernelContext
	pop	{r1, ip}
	mrs	lr, SPSR
	str	lr, [sp, #60]
	mov	r0, sp
	/* C policy runs on the IRQ stack, never on the banked user stack. */
	push	{r0, r1, ip, lr}
	mov	r1, r0
	ldr	r0, [sp, #4]
	blx	intrEnterInterrupt
	mov	r3, r0
	pop	{r0, r1, ip, lr}
	cps	#31
	cmp	r3, #0
	subne	r3, r3, #2
	mrcne p15, 0, r4, c0, c0, 5
	andne	r4, r4, #15
	bne	.LcaptureNestedInterrupt
	ldr	r2, [r0, #16]
	ldr	r3, [r2, #12]
	str	lr, [r3, #60]
	str	sp, [r3, #56]!
	ldr	lr, [r0, #52]
	stmdb	r3!, {r5, r6, r7, r8, r9, sl, fp, lr}
	/* r5-r12 are captured; C copies r0-r4 from the saved IRQ frame. */
	add	r3, r3, #52
	ldr	r4, [r2, #16]
	cmp	r4, #0
	beq	.LsaveInterruptSystem
	vstmia	r4!, {d0-d15}
	vstmia	r4!, {d16-d31}
	vmrs	r5, fpscr
	vmrs	r6, fpexc
	stmia	r4!, {r5, r6}
.LsaveInterruptSystem:
	/* User SP/LR/GPRs are already in the thread frame. Switch to the kernel
	 * scheduler stack before calling C: the interrupted user stack need not
	 * be mapped by the kernel TTBR installed above.
	 */
	mrc p15, 0, r4, c0, c0, 5
	and	r4, r4, #15
	movw sp, #:lower16:(g_intrInitialIrqStack)
	movt sp, #:upper16:(g_intrInitialIrqStack)
	sub	sp, sp, r4, lsl #17
	push	{r0, r1, r2, r3, ip, lr}
	ldr	r1, [r0, #20]
	mov	r2, r0
	sub	r0, r3, #76
	blx	intrSaveSystemContext
	pop	{r0, r1, r2, r3, ip, lr}
	/* Already on this CPU's scheduler stack after the policy call. */
	mov	r6, #15728640
	mcr p15, 0, r6, c1, c0, 2
	mov	r6, #1073741824
	vmsr	fpexc, r6
	mov	r6, #0
	vmsr	fpscr, r6
	mrc p15, 0, r5, c12, c0, 0
	ldr	r5, [r5, #36]
	mov	r6, #135
	str	r6, [r5]
	push	{r0, r1, r2, r3, ip, lr}
	mov	r0, ip
	mov	r1, r2
	blx	intrTraceInterruptedThread
	pop	{r0, r1, r2, r3, ip, lr}
.LdispatchInterrupt:
	push	{r2, ip}
	mov	r3, #50331648
	vmsr	fpscr, r3
	push	{r1, lr}
	blx	intrRestoreKernelDebug
	pop	{r1, lr}
	mov	r0, r1
	blx	intrDispatchInterrupt
	pop	{r4, ip}
.global intrLeaveBranch
.type intrLeaveBranch, %object
intrLeaveBranch:
	nop
.LcompleteInterrupt:
	/* GIC completion, guard checking and nesting policy are ordinary C. */
	push	{ip, lr}
	mov	r1, ip
	blx	intrCompleteInterrupt
	pop	{ip, lr}
	cmp	r0, #0
	bne	.LrestoreNestedInterrupt
	cmp	r4, #0
	beq	.LrestoreNestedInterrupt
	mov	r0, r4
	blx	intrResumeInterruptedThread
	/* C owns the stored-register policy; keep the live return state here. */
	push	{r0, r2, ip, lr}
	mov	r1, #1
	blx	intrRestoreSystemContext
	pop	{r0, r2, ip, lr}
	ldr	fp, [r0, #240]
	add	r1, r0, #76
	sub	r1, r1, #8
	mrc p15, 0, r4, c13, c0, 4
	ldr	r5, [r4, #16]
	cmp	r5, #0
	beq	.LselectIrqReturnImage
	mov	r6, #1073741824
	vmsr	fpexc, r6
	mov	r6, #0
	vmsr	fpscr, r6
	vmsr	fpexc, r6
	mrc p15, 0, r6, c12, c0, 0
	ldr	r6, [r6, #36]
	ldr	r7, [r4, #20]
	str	r7, [r6]
	ldr	r6, [r5, #260]
	vmsr	fpexc, r6
	ldr	r6, [r5, #256]
	vmsr	fpscr, r6
.LselectIrqReturnImage:
	/* Full exception images and cooperative d8-d15 images use different ABIs. */
	ldr	r2, [r0]
	cmp	r2, #1
	mov	sl, r2
	beq	.LrestoreIrqFullVfp
	cmp	r5, #0
	beq	.LrestoreIrqCooperativeFrame
	movw	r2, #57005
	movt	r2, #32640
	add	r3, r2, #7864320
	vmov	d0, r2, r3
	vorr	d1, d0, d0
	vorr	q2, q0, q0
	vorr	q3, q0, q0
	vorr	q8, q0, q0
	vorr	q9, q0, q0
	vorr	q10, q0, q0
	vorr	q11, q0, q0
	vorr	q12, q0, q0
	vorr	q13, q0, q0
	vorr	q14, q0, q0
	vorr	q15, q0, q0
	add	r5, r5, #64
	vldmia	r5!, {d8-d15}
.LrestoreIrqCooperativeFrame:
	ldr	r3, [r1, #-4]!
	ldr	r2, [r1, #-4]!
	ldr	sp, [r1, #-4]!
	b	.LenterIrqReturnMode
.LrestoreIrqFullVfp:
	cmp	r5, #0
	beq	.LrestoreIrqExceptionFrame
	vldmia	r5!, {d0-d15}
	vldmia	r5!, {d16-d31}
.LrestoreIrqExceptionFrame:
	sub	r1, r1, #4
	ldr	lr, [r1, #-4]!
	ldr	sp, [r1, #-4]!
	ldrd	r2, [r0, #192]
.LenterIrqReturnMode:
	cps	#18
	push	{r0, r1, r2, r3, ip, lr}
	mov	r2, r0
	mov	r0, r4
	mov	r1, r3
	blx	intrPrepareContextReturn
	pop	{r0, r1, r2, r3, ip, lr}
	add	sp, sp, #64
	mcr p15, 0, fp, c9, c12, 1
	push	{r2, r3}
	cmp	sl, #1
	beq	.LreturnIrqException
	sub	r1, r1, #4
	mrc p15, 0, r4, c12, c0, 0
	ldr	r4, [r4, #36]
	mov	r5, #1
	str	r5, [r4, #256]
	ldmdb	r1!, {r4, r5, r6, r7, r8, r9, sl, fp}
	mov	r0, #1
	clrex
	rfeia	sp!
.LreturnIrqException:
	ldmdb	r1!, {r0}
	stmfd	sp!, {r0}
	mrc p15, 0, r4, c12, c0, 0
	ldr	r4, [r4, #36]
	mov	r5, #1
	str	r5, [r4, #256]
	mov	ip, r1
	ldmdb	ip!, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, sl, fp}
	ldmfd	sp!, {ip}
	clrex
	rfeia	sp!
.global SceIntrmgrForKernel_71C5F42F
.type SceIntrmgrForKernel_71C5F42F, %function
SceIntrmgrForKernel_71C5F42F:
	/* setjmp-like ABI: capture this caller's callee-saved registers/SP/LR. */
	mov	r2, #0
	stmia	r0!, {r2}
	add	r0, r0, #16
	stmia	r0!, {r4, r5, r6, r7, r8, r9, sl, fp}
	add	r0, r0, #4
	stmia	r0!, {sp}
	stmia	r0!, {lr}
	mrs	r2, CPSR
	orr	r2, r2, #32
	stmia	r0!, {r2}
	add	r0, r0, #4
	mrc p15, 0, r2, c3, c0, 0
	str	r2, [r0], #4
	cmp	r1, #0
	beq	.LsaveCooperativeSystemContext
	tst	r1, #1
	bicne	r1, r1, #1
	addeq	r1, r1, #64
	vstmia	r1!, {d8-d15}
	addeq	r1, r1, #128
	vmrs	r2, fpscr
	vmrs	r3, fpexc
	stmia	r1!, {r2, r3}
.LsaveCooperativeSystemContext:
	/* General/VFP registers above are captured before entering C. */
	push	{r4, lr}
	sub	r0, r0, #76
	mov	r1, #0
	mov	r2, #0
	blx	intrSaveSystemContext
	pop	{r4, lr}
	mov	r0, #0
	bx	lr
.global SceIntrmgrForKernel_08FDB0CE
.type SceIntrmgrForKernel_08FDB0CE, %function
SceIntrmgrForKernel_08FDB0CE:
	/* longjmp-like ABI: replace live GPR/VFP state and return through RFE. */
	mov	lr, r2
	mov	ip, r0
	ldr	r3, [r0]
	push	{r1, r3, ip, lr}
	mov	r1, #0
	blx	intrRestoreSystemContext
	pop	{r1, r3, ip, lr}
	ldr	fp, [ip, #240]
	add	r0, ip, #76
	sub	r0, r0, #8
	mrc p15, 0, r4, c13, c0, 4
	cmp	lr, #0
	beq	.LselectCooperativeReturnImage
	mov	r6, #1073741824
	vmsr	fpexc, r6
	mov	r6, #0
	vmsr	fpscr, r6
	vmsr	fpexc, r6
	mrc p15, 0, r6, c12, c0, 0
	ldr	r6, [r6, #36]
	ldr	r7, [r4, #20]
	str	r7, [r6]
.LselectCooperativeReturnImage:
	cmp	r3, #1
	mov	sl, r3
	beq	.LrestoreCooperativeFullVfp
	cmp	lr, #0
	beq	.LrestoreCooperativeSavedFrame
	tst	lr, #1
	lsrne	lr, lr, #1
	lslne	lr, lr, #1
	ldrne	r6, [lr, #68]
	ldreq	r6, [lr, #260]
	vmsr	fpexc, r6
	ldrne	r6, [lr, #64]
	ldreq	r6, [lr, #256]
	vmsr	fpscr, r6
	movw	r2, #57005
	movt	r2, #32640
	add	r3, r2, #7864320
	vmov	d0, r2, r3
	vorr	d1, d0, d0
	vorr	q2, q0, q0
	vorr	q3, q0, q0
	vorr	q8, q0, q0
	vorr	q9, q0, q0
	vorr	q10, q0, q0
	vorr	q11, q0, q0
	vorr	q12, q0, q0
	vorr	q13, q0, q0
	vorr	q14, q0, q0
	vorr	q15, q0, q0
	addeq	lr, lr, #64
	vldmia	lr!, {d8-d15}
.LrestoreCooperativeSavedFrame:
	ldr	r3, [r0, #-4]!
	ldr	r2, [r0, #-4]!
	ldr	sp, [r0, #-4]!
	b	.LenterCooperativeReturnMode
.LrestoreCooperativeFullVfp:
	cmp	lr, #0
	beq	.LrestoreCooperativeExceptionFrame
	ldr	r6, [lr, #260]
	vmsr	fpexc, r6
	ldr	r6, [lr, #256]
	vmsr	fpscr, r6
	vldmia	lr!, {d0-d15}
	vldmia	lr!, {d16-d31}
.LrestoreCooperativeExceptionFrame:
	sub	r0, r0, #4
	ldr	lr, [r0, #-4]!
	ldr	sp, [r0, #-4]!
	ldrd	r2, [ip, #192]
.LenterCooperativeReturnMode:
	cps	#18
	push	{r0, r1, r2, r3, ip, lr}
	mov	r2, ip
	mov	r0, r4
	mov	r1, r3
	blx	intrPrepareContextReturn
	pop	{r0, r1, r2, r3, ip, lr}
	cmp	sl, #1
	mcr p15, 0, fp, c9, c12, 1
	beq	.LreturnCooperativeException
	push	{r2, r3}
	mrc p15, 0, r4, c12, c0, 0
	ldr	r4, [r4, #36]
	mov	r5, #1
	str	r5, [r4, #256]
	sub	r0, r0, #4
	ldmdb	r0!, {r4, r5, r6, r7, r8, r9, sl, fp}
	mov	r0, #1
	clrex
	rfeia	sp!
.LreturnCooperativeException:
	push	{r2, r3}
	ldmdb	r0!, {r1}
	stmfd	sp!, {r1}
	mrc p15, 0, r4, c12, c0, 0
	ldr	r4, [r4, #36]
	mov	r5, #1
	str	r5, [r4, #256]
	mov	ip, r0
	ldmdb	ip!, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, sl, fp}
	ldmfd	sp!, {ip}
	clrex
	rfeia	sp!
