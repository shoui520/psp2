/* Pervasive idle clock transitions; original privileged Thumb instructions. */
.syntax unified
.thumb
.arch armv7-a
.text
.balign 64
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
.size LowioIdleEntry, .-LowioIdleEntry
.section .note.GNU-stack,"",%progbits
