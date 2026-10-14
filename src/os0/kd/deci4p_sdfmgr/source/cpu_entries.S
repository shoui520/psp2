/* Exception-chain and SDIO return boundaries. C owns stored-context policy;
 * this file preserves live GPR/VFP images, banked stacks and exception return. */
.syntax unified
.cpu cortex-a9
.fpu neon
.arm
.section .text.sdfMgrCpuEntries, "ax", %progbits

/* Excpmgr patches the two-word link header; entry execution starts at +8. */
.macro SDF_EXCEPTION_ENTRY entry, handler
.balign 4
.global \entry
.type \entry, %object
\entry:
	.word 0, 0
	ldr r2, [r0, #0x44]
	mov r4, r0
	ldr r1, [r0, #0x3c]
	push {lr}
	sub sp, sp, #4
	cpsie i
	blx \handler
	cpsid i
	mov r1, r0
	mov r0, r4
	add sp, sp, #4
	pop {lr}
	and r2, r1, #15
	cmp r2, #0
	bne .Lchain\@
	push {r0, r1, r2, r3, ip, lr}
	blx sdfMgrFinishException
	pop {r0, r1, r2, r3, ip, lr}
	b .LreturnException
.Lchain\@:
	ldr r2, =\entry
	ldr r2, [r2]
	add r2, r2, #8
	mov pc, r2
.size \entry, .-\entry
.endm

SDF_EXCEPTION_ENTRY sdfMgrPrefetchAbortEntry, sdfMgrPrefetchAbortHandler
SDF_EXCEPTION_ENTRY sdfMgrDataAbortEntry, sdfMgrDataAbortHandler
SDF_EXCEPTION_ENTRY sdfMgrUndefinedEntry, sdfMgrUndefinedHandler

/* Address-space policy is C; the VFP data-register image must stay live here. */
.LreturnException:
	push {r0, r1, r2, r3, ip, lr}
	blx sdfMgrRestoreExceptionAddressSpace
	pop {r0, r1, r2, r3, ip, lr}
	ldr r1, [r0, #0x40]
	cmp r1, #0
	beq .LrestoreCpacr
	ldr r2, [r0, #0x50]
	ldr r3, [r0, #0x4c]
	vmsr fpexc, r2
	vmsr fpscr, r3
	adds r2, r0, #0x100
	vldmia r2!, {d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11, d12, d13, d14, d15}
	vldmia r2!, {d16, d17, d18, d19, d20, d21, d22, d23, d24, d25, d26, d27, d28, d29, d30, d31}
.LrestoreCpacr:
	ldr r1, [r0, #0x48]
	mcr p15, #0, r1, c1, c0, #2
	push {r0, r1, r2, r3, ip, lr}
	blx sdfMgrGetResumeBreakpointValue
	mov sb, r0
	pop {r0, r1, r2, r3, ip, lr}
	push {r0, r1, r2, r3, ip, lr}
	blx sdfMgrTakeResumeBreakpointControl
	mov sl, r0
	pop {r0, r1, r2, r3, ip, lr}
	cps #0x17
	ldr r4, [r0, #0x60]
	push {r0, r1, r2, r3, ip, lr}
	mov r0, r4
	mov r1, sb
	mov r2, sl
	mov r3, #0
	blx sdfMgrRestoreDebugContext
	pop {r0, r1, r2, r3, ip, lr}
	push {r0, r1, r2, r3, ip, lr}
	blx sdfMgrMarkTracePending
	pop {r0, r1, r2, r3, ip, lr}
	adds ip, r0, #0
	ldm ip!, {r0, r1, r2, r3, r4, r5, r6, r7, r8, sb, sl, fp}
	adds ip, ip, #4
	ldm ip, {sp, lr} ^
	subs ip, ip, #0x34
	ldr lr, [ip, #0x44]
	str lr, [sp, #-4]!
	ldr lr, [ip, #0x3c]
	str lr, [sp, #-4]!
	ldr ip, [ip, #0x30]
	clrex 
	rfeia sp!

/* This entry is called from Intrmgr's live IRQ frame, not the ordinary C ABI. */
.balign 4
.global sdioIntrEntry
.type sdioIntrEntry, %object
sdioIntrEntry:
	.word 0, 0
	stmdb sp!, {lr}
	sub sp, sp, #4
	blx sdioIntrHandler
	add sp, sp, #4
	ldm sp!, {lr}
	cmp r0, #0x10
	beq .LcompleteSdioInterrupt
	bx lr
.LcompleteSdioInterrupt:
	cpsid i
	pop {r4, ip}
	push {r0, r1, r2, r3, ip, lr}
	mov r0, ip
	blx sdfMgrCompleteInterrupt
	mov r5, r0
	pop {r0, r1, r2, r3, ip, lr}
	cmp r5, #0
	bne .LnestedInterruptReturn
	cmp r4, #0
	beq .LnestedInterruptReturn
	/* Restore the shared thread context in C; retain the live return image. */
	mcr p15, #0, r4, c13, c0, #4
	ldr r0, [r4, #0xc]
	push {r0, r1, r2, r3, ip, lr}
	blx sdfMgrRestoreSystemContext
	pop {r0, r1, r2, r3, ip, lr}
	mrc p15, #0, ip, c13, c0, #4
	ldr fp, [r0, #0xf0]
	/* Cooperative return reuses the PMU walk's r7/r8 as debug slot inputs. */
	ldr r7, [r0, #0x64]
	ldr r8, [r0, #0x68]
	/* The descending register walk ends at the saved user-DACR field. */
	add r1, r0, #0x44
	mrc p15, #0, r4, c13, c0, #4
	ldr r5, [r4, #0x10]
	cmp r5, #0
	movne r6, #0x40000000
	vmsrne fpexc, r6
	movne r6, #0
	vmsrne fpscr, r6
	vmsrne fpexc, r6
	mrcne p15, #0, r6, c12, c0, #0
	ldrne r6, [r6, #0x24]
	ldrne r7, [r4, #0x14]
	strne r7, [r6]
	ldrne r6, [r5, #0x104]
	vmsrne fpexc, r6
	ldrne r6, [r5, #0x100]
	vmsrne fpscr, r6
	ldr r2, [r0]
	cmp r2, #1
	mov sl, r2
	beq .LfullVfpReturn
	cmp r5, #0
	beq .LcooperativeFrame
	movw r6, #:lower16:(.LvfpPoison)
	movt r6, #:upper16:(.LvfpPoison)
	vldr d0, [r6]
	vorr d1, d0, d0
	vorr q2, q0, q0
	vorr q3, q0, q0
	vorr q8, q0, q0
	vorr q9, q0, q0
	vorr q10, q0, q0
	vorr q11, q0, q0
	vorr q12, q0, q0
	vorr q13, q0, q0
	vorr q14, q0, q0
	vorr q15, q0, q0
	add r5, r5, #0x40
	vldmia r5!, {d8, d9, d10, d11, d12, d13, d14, d15}
.LcooperativeFrame:
	ldmdb r1!, {r3}
	ldmdb r1!, {r2}
	ldmdb r1!, {sp}
	b .LselectReturnMode
.LfullVfpReturn:
	cmp r5, #0
	vldmiane r5!, {d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11, d12, d13, d14, d15}
	vldmiane r5!, {d16, d17, d18, d19, d20, d21, d22, d23, d24, d25, d26, d27, d28, d29, d30, d31}
	push {r0, r1, r2, r3, ip, lr}
	blx sdfMgrGetResumeBreakpointValue
	mov r7, r0
	pop {r0, r1, r2, r3, ip, lr}
	push {r0, r1, r2, r3, ip, lr}
	blx sdfMgrTakeResumeBreakpointControl
	mov r8, r0
	pop {r0, r1, r2, r3, ip, lr}
	sub r1, r1, #4
	ldmdb r1!, {lr}
	ldmdb r1!, {sp}
	ldr r2, [r0, #0xc0]
	ldr r3, [r0, #0xc4]
.LselectReturnMode:
	cps #0x12
	push {r0, r1, r2, r3, ip, lr}
	mov r1, r3
	blx sdfMgrRestoreThreadDacr
	pop {r0, r1, r2, r3, ip, lr}
	push {r0, r1, r2, r3, ip, lr}
	mov r0, r4
	mov r1, r7
	mov r2, r8
	mov r3, #1
	blx sdfMgrRestoreDebugContext
	pop {r0, r1, r2, r3, ip, lr}
.LrestoreDebugStatus:
	mrc p14, #0, r0, c0, c2, #2
	orr r0, r0, #0x8800
	mcr p14, #0, r0, c0, c2, #2
	add sp, sp, #0x38
	mcr p15, #0, fp, c9, c12, #1
	push {r0, r1, r2, r3, ip, lr}
	blx sdfMgrMarkTracePending
	pop {r0, r1, r2, r3, ip, lr}
	cmp sl, #1
	beq .LinterruptReturn
.LcooperativeReturn:
	push {r2, r3}
	sub r1, r1, #4
	ldmdb r1!, {r4, r5, r6, r7, r8, sb, sl, fp}
	mov r0, #1
	clrex 
	rfeia sp!
.LinterruptReturn:
	push {r2, r3}
	ldmdb r1!, {r0}
	stmdb sp!, {r0}
	mov ip, r1
	ldmdb ip!, {r0, r1, r2, r3, r4, r5, r6, r7, r8, sb, sl, fp}
	ldm sp!, {ip}
	clrex 
	rfeia sp!
.LnestedInterruptReturn:
	pop {r0, r1}
	vmsr fpexc, r0
	vmsr fpscr, r1
	vpop {d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11, d12, d13, d14, d15}
	vpop {d16, d17, d18, d19, d20, d21, d22, d23, d24, d25, d26, d27, d28, d29, d30, d31}
	pop {r5, r6, r7, r8, sb, sl, fp, ip}
	ldm sp!, {r1}
	ldm sp!, {lr}
	stmdb sp!, {r6}
	push {r0, r1, r2, r3, ip, lr}
	blx sdfMgrGetResumeBreakpointValue
	mov r6, r0
	pop {r0, r1, r2, r3, ip, lr}
	mcr p14, #0, r6, c0, c1, #4
	push {r0, r1, r2, r3, ip, lr}
	blx sdfMgrTakeResumeBreakpointControl
	mov r6, r0
	pop {r0, r1, r2, r3, ip, lr}
	mov r0, r6
	ldm sp!, {r6}
	mov sp, r1
	cps #0x12
	mcr p14, #0, r0, c0, c1, #5
	mrc p14, #0, r0, c0, c2, #2
	orr r0, r0, #0x8800
	mcr p14, #0, r0, c0, c2, #2
	ldr r0, [sp, #0x20]
	ldr r1, [sp, #0x28]
	mov r2, #0
	mcr p15, #0, r2, c13, c0, #1
	isb sy
	mcr p15, #0, r0, c2, c0, #1
	isb sy
	mcr p15, #0, r1, c13, c0, #1
	ldr r0, [sp, #0x24]
	mcr p15, #0, r0, c3, c0, #0
	ldr r0, [sp, #0x2c]
	mcr p15, #0, r0, c7, c4, #0
	mrc p15, #0, r0, c12, c0, #0
	/* Native nested return uses the captured thread's +0x24 word here. */
	ldr r0, [r4, #0x24]
	cmp r0, #0
	mov r1, #1
	strne r1, [r0, #0x100]
	ldr r0, [sp]
	ldr r1, [sp, #4]
	ldr r2, [sp, #8]
	add sp, sp, #0x30
	clrex 
	rfeia sp!
.size sdioIntrEntry, .-sdioIntrEntry

/* Native poison for caller-saved VFP registers on a cooperative return. */
.balign 8
.LvfpPoison:
	.word 0x7f80dead, 0x7ff8dead
.ltorg
.section .note.GNU-stack, "", %progbits

