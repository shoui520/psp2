/*
 * Secure monitor services and the idle loop, in the original's order. A function
 * followed by alignment padding owns it (the original's unwind table ends each
 * function where the next begins), so the padding is inside its size.
 */
.syntax unified
.thumb
.arch armv7-a
.arch_extension sec
.text

.macro SERVICE name, number, align=0
	.global \name
	.type \name, %function
	.thumb_func
\name:
	movw ip, #\number
	smc #0
	bx lr
	.if \align
	.balign \align, 0
	.endif
	.size \name, .-\name
.endm

/* Service 0x11E's first argument selects the operation; these set it. */
.macro SERVICE_11E name, operation
	.global \name
	.type \name, %function
	.thumb_func
\name:
	mov.w r0, #\operation
	movw ip, #0x11e
	smc #0
	bx lr
	.size \name, .-\name
.endm

SERVICE Smc110, 0x110
SERVICE Smc111, 0x111
SERVICE Smc113, 0x113
SERVICE Smc114, 0x114, 64

/* The idle loop: privileged Thumb code (cpsid, ldrex/strex, CP15). */
.global LowioIdleEntry
.type LowioIdleEntry, %function
.thumb_func
LowioIdleEntry:
	movw r3, #:lower16:s_baseClock
	movt r3, #:upper16:s_baseClock
	movw r8, #:lower16:SceSysrootForDriver_4B8C305A
	movt r8, #:upper16:SceSysrootForDriver_4B8C305A
	ldr ip, [r3]
	mrs r0, cpsr
	cpsid aif, #18
	mov r9, sp
	msr cpsr_fc, r0
	mrc p15, 0, r5, c12, c0, 0
	ldr r6, [r5, #0x38]
	movs r3, #7
	movs r7, #8
	subs r2, r3, r7
	movs r4, #1
	/* Original loop starts at the next 64-byte cache line. */
	.rept 4
	nop.w
	.endr
.Lidle:
	cpsid i
.Lenter:
	ldrex r1, [r8, #8]
	subs r1, #1
	strex r0, r1, [r8, #8]
	cmp r0, #0
	bne .Lenter
	pli [r5]
	pld [r5, #0x20]
	pli [r6]
	pli [pc, #0x40]
	pld [r6, #-8]
	pld [r9, #-0x40]
	pld [r9, #-0x20]
	cbnz r1, .Lwait
	ldr r0, [ip]
	ldr r1, [ip, #4]
	strd r0, r1, [r8]
	str r3, [ip]
	str r4, [ip]
	str r7, [ip, #4]
	str r2, [r8, #8]
	dmb sy
.Lwait:
	wfi
.Lexit:
	ldrex r1, [r8, #8]
	cbz r1, .Lagain
	adds r1, #1
	strex r0, r1, [r8, #8]
	cbnz r0, .Lagain
	dmb sy
	cbnz r1, .Lresume
	ldrd r0, r1, [r8]
	str r3, [ip]
	str r1, [ip, #4]
	str r0, [ip]
	str r4, [r8, #8]
.Lresume:
	cpsie i
	b .Lidle
.Lagain:
	b .Lexit
.balign 4
.size LowioIdleEntry, .-LowioIdleEntry

SERVICE Smc11B, 0x11b
SERVICE Smc11C, 0x11c
SERVICE Smc120, 0x120
SERVICE Smc11D, 0x11d

/* operation 1 takes the caller's first argument as its second */
.global Smc11EOperation1
.type Smc11EOperation1, %function
.thumb_func
Smc11EOperation1:
	mov r1, r0
	mov.w r0, #1
	movw ip, #0x11e
	smc #0
	bx lr
.size Smc11EOperation1, .-Smc11EOperation1

SERVICE_11E Smc11EOperation2, 2
SERVICE_11E Smc11EOperation0, 0
SERVICE Smc11F, 0x11f
SERVICE Smc121, 0x121
SERVICE Smc117, 0x117
SERVICE SceCdramForDriver_2D728EBF, 0x118
SERVICE SceCdramForDriver_0657FC1B, 0x119, 4
.section .note.GNU-stack,"",%progbits
