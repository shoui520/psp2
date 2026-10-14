/* Live register capture and return at the SVC boundary. */
.syntax unified
.arch armv7-a
.arch_extension sec
.fpu neon
.section .text.intrmgr_syscall,"ax",%progbits
.thumb
	.balign 4
.global syscallEnterHook
.type syscallEnterHook, %object
syscallEnterHook:
	mov	r0, sp
	bl	intrCallSyscallEnterHook
	ldr.w	ip, [sp, #56]
	b	.LcallSyscall
.global syscallLeaveHook
.type syscallLeaveHook, %object
syscallLeaveHook:
	add	r0, sp, #16
	mov	r1, sp
	bl	intrCallSyscallLeaveHook
	b	.LrestoreSyscall
.LinvalidSyscallMode:
	bkpt	0
	.balign 4
.global syscallEntry
.type syscallEntry, %function
.thumb_func
syscallEntry:
	/* Capture user registers before a C prologue can alter caller-saved state. */
	.word 0, 0
	stmdb	sp, {r0, r1, r2, r3}
	mrs	r2, SPSR
	strd	lr, r2, [sp, #-24]
	and.w	r2, r2, #31
	subs	r2, #16
	bne.n	.LinvalidSyscallMode
	/* Allocate the SVC capture before C can use this banked stack. */
	sub	sp, #32
	str.w	ip, [sp]
	mov	r0, sp
	bl	intrPrepareSyscall
	mov	r2, r0
	ldr.w	ip, [sp]
	add	r0, sp, #32
	add	sp, #32
	cps	#31
	mov	r3, sp
	mov	sp, r2
	mov	r2, r3
	ldrd	r1, r3, [r0, #-24]
	strd	r1, r3, [sp, #88]
	strd	r2, lr, [sp, #80]
	mov	lr, r0
	ldmdb	lr, {r0, r1, r2, r3}
	stmia.w	sp, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, sl, fp, ip}
	ldr.w	r1, [lr, #-28]
	mov	r0, sp
	bl	intrSelectSyscall
	cmp	r0, #0
	beq.w	.LinvalidSyscallNumber
	ldr.w	ip, [sp, #56]
.global syscallEnterBranch
.type syscallEnterBranch, %object
syscallEnterBranch:
	nop
.LcallSyscall:
	ldmia.w	sp, {r0, r1, r2, r3}
	cpsie	i
	blx	ip
	cpsid	i
	push	{r0, r1, r2, r3}
	/* Results and frame policy are C; the live-register return is below. */
	add	r0, sp, #16
	mov	r1, sp
	movs	r2, #0
	bl	intrFinishSyscall
.LsyscallPolicyResult:
	cmp	r0, #0
	bne	.LretrySyscall
	mrc p15, 0, r3, c13, c0, 4
.global syscallLeaveBranch
.type syscallLeaveBranch, %object
syscallLeaveBranch:
	nop
.LrestoreSyscall:
	add	r0, sp, #16
	bl	intrRestoreSyscallContext
	mov	lr, r0
	pop	{r0, r1, r2, r3}
	tst.w	lr, #8192
	bne.n	.LrestoreSyscallCalleeRegisters
.LprepareSyscallResultRegisters:
	/* Apply the return-register ABI after the optional saved r4-r11 restore.
	 * Moving this poisoning into the saved result buffer changes that restore:
	 * Sony deliberately loads it from the preceding result words below.
	 */
	ldr.w	ip, [sp, #64]
	cmp.w	ip, #4
	bcs.n	.LrestoreSyscallUserRegisters
	ands.w	ip, ip, #1
	movw	r3, #48879
	movt	r3, #57005
	it	eq
	moveq	r1, r3
	mov	r2, r3
.LrestoreSyscallUserRegisters:
	/* Restore user FP/thread registers and banked SP/LR, then return via RFE. */
	mrc p15, 0, ip, c12, c0, 0
	ldr.w	ip, [ip, #36]
	cmp.w	ip, #0
	mov.w	lr, #1
	it	ne
	strne.w	lr, [ip, #256]
	ldr.w	ip, [sp, #68]
	mcr p15, 0, ip, c13, c0, 2
	ldr.w	ip, [sp, #60]
	vmsr	fpscr, ip
	add.w	ip, sp, #88
	ldr.w	lr, [sp, #84]
	ldr.w	sp, [sp, #80]
	cps	#19
	ldmia.w	ip, {ip, lr}
	stmdb	sp!, {ip, lr}
	movw	ip, #48879
	movt	ip, #57005
	clrex
	rfeia	sp!
.LrestoreSyscallCalleeRegisters:
	add	r4, sp, #16
	ldmdb	r4, {r4, r5, r6, r7, r8, r9, sl, fp}
	b.n	.LprepareSyscallResultRegisters
.LretrySyscall:
	add	sp, #16
	ldr.w	ip, [sp, #56]
	b.n	.LcallSyscall
.LinvalidSyscallNumber:
	sub	sp, #16
	add	r0, sp, #16
	mov	r1, sp
	movs	r2, #1
	bl	intrFinishSyscall
	b	.LsyscallPolicyResult
