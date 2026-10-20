/*
 * Reads of another processor mode's banked registers, and the secure monitor
 * call the debugger issues: what C cannot express.
 *
 * Each reader takes a register code and a pointer to store the value. The high
 * nibble of the code selects the mode (0x00 Svc, 0x10 Abt, 0x20 Und, 0x30 Irq,
 * 0x40 Fiq); a reader returns at once for any other mode. The low nibble selects
 * the register: SP, LR or SPSR (Fiq: R8-R12, SP, LR, SPSR). The reader switches
 * to the mode with interrupts masked, stores the register and switches back.
 * Like Sony's, the table jump does not bound the index by the table's size.
 */
	.syntax	unified
	.arch	armv7-a
	.arch_extension sec
	.text
	.arm

#define MODE_SELECT		0xf0
#define REGISTER_SELECT		0x0f

#define PSR_MODE_FIQ		0x11
#define PSR_MODE_IRQ		0x12
#define PSR_MODE_SVC		0x13
#define PSR_MODE_ABT		0x17
#define PSR_MODE_UND		0x1b

/* the secure monitor service the debugger calls (sdbgpCallSecureMonitor) */
#define SMC_DEBUGGER_SERVICE	0x122

	.macro	FUNCTION name
	.global	\name
	.type	\name, %function
\name:
	.endm

	.macro	END_FUNCTION name
	.size	\name, . - \name
	.endm

/* SP, LR and SPSR of one mode */
	.macro	READ_BANKED name, select, mode
FUNCTION \name
	and	r2, r0, #MODE_SELECT
	cmp	r2, #\select
	bxne	lr
	mrs	r3, cpsr
	cpsid	i, #\mode
	and	r2, r0, #REGISTER_SELECT
	cmp	r2, #15
	addlt	pc, pc, r2, lsl #2
	nop
	b	1f
	b	2f
	b	3f
1:	str	sp, [r1]
	b	9f
2:	str	lr, [r1]
	b	9f
3:	mrs	r2, spsr
	str	r2, [r1]
	b	9f
9:	nop
	msr	cpsr_fc, r3
	bx	lr
END_FUNCTION \name
	.endm

	READ_BANKED sdbgpReadSvcRegister, 0x00, PSR_MODE_SVC
	READ_BANKED sdbgpReadAbtRegister, 0x10, PSR_MODE_ABT
	READ_BANKED sdbgpReadUndRegister, 0x20, PSR_MODE_UND
	READ_BANKED sdbgpReadIrqRegister, 0x30, PSR_MODE_IRQ

/* Fiq banks R8-R12 as well */
FUNCTION sdbgpReadFiqRegister
	and	r2, r0, #MODE_SELECT
	cmp	r2, #0x40
	bxne	lr
	mrs	r3, cpsr
	cpsid	i, #PSR_MODE_FIQ
	and	r2, r0, #REGISTER_SELECT
	cmp	r2, #15
	addlt	pc, pc, r2, lsl #2
	nop
	b	1f
	b	2f
	b	3f
	b	4f
	b	5f
	b	6f
	b	7f
	b	8f
1:	str	r8, [r1]
	b	9f
2:	str	r9, [r1]
	b	9f
3:	str	r10, [r1]
	b	9f
4:	str	r11, [r1]
	b	9f
5:	str	r12, [r1]
	b	9f
6:	str	sp, [r1]
	b	9f
7:	str	lr, [r1]
	b	9f
8:	mrs	r2, spsr
	str	r2, [r1]
	b	9f
9:	nop
	msr	cpsr_fc, r3
	bx	lr
END_FUNCTION sdbgpReadFiqRegister

	.balign	16, 0
	.thumb
	.thumb_func
FUNCTION sdbgpCallSecureMonitor
	movw	ip, #SMC_DEBUGGER_SERVICE
	smc	#0
	bx	lr
END_FUNCTION sdbgpCallSecureMonitor

	.section .note.GNU-stack, "", %progbits
