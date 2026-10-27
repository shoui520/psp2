/*
 * intr_asm.s: MeP interrupt controller (INTC) control-bus initialization,
 * hardware IRQ / command IRQ / SWI exception entry stubs, active IRQ query,
 * and ARM mailbox interrupt trigger (0x8002c0..0x8003d7).
 *
 * Function names are ours.
 */

	.text

	/*
	 * skIntcInit (0x8002c0, name ours):
	 * Initializes MeP INTC control-bus registers (IVR=0, ILR0=0x07777777,
	 * ILR1=0x777f, ISR=0, IER=0x100, MCFG=0x600) and enables EIE in $psw.
	 */
	.global skIntcInit
skIntcInit:
	di
	mov	$12, 0
	stcb	$12, 0x3
	movh	$11, 0x777
	add3	$11, $11, 0x7777
	stcb	$11, 0x4
	mov	$11, 0x777f
	stcb	$11, 0x5
	stcb	$12, 0x6
	stcb	$12, 0x7
	mov	$11, 0x600
	stcb	$11, 0x0
	mov	$11, 0x100
	stcb	$11, 0x2
	ldc	$12, $psw
	or3	$12, $12, 0x100
	stc	$12, $psw
	ret

	/*
	 * skEnableSwiException (0x8002fe, name ours):
	 * Sets bit 4 (0x10) in $psw to enable SWI exceptions from Secure Modules.
	 */
	.global skEnableSwiException
skEnableSwiException:
	di
	ldc	$12, $psw
	or3	$12, $12, 0x10
	stc	$12, $psw
	ret

	/*
	 * irqExceptionEntry (0x80030a, name ours):
	 * Saves interrupted SM context into g_irqCpuContext (0x807f80), switches
	 * to the SM IRQ stack (0x80b000), dispatches the registered IRQ handler,
	 * and restores context via irqExceptionExit.
	 */
	.global irqExceptionEntry
irqExceptionEntry:
	stc	$sp, $tmp
	movh	$sp, 0x80
	add3	$sp, $sp, 0x7f80
	sw	$0, ($sp)
	ldc	$0, $lp
	bsr	skSaveCpuContext
	bsr	skLoadKernelStack
	bsr	skGetActiveIrqNumber
	mov	$5, $0
	mov	$1, $0
	bsr	skGetIrqHandler
	mov	$6, $0
	bsr	skSetupSmIrqStack
	ldcb	$7, 0x0
	and3	$7, $7, 0xf000
	srl	$7, 4
	stcb	$7, 0x0
	nop
	nop
	nop
	ei
	mov	$1, $5
	jsr	$6
	di
	mov	$7, 0x600
	stcb	$7, 0x0
	nop
	nop
	jmp	irqExceptionExit

	/*
	 * cmdIrqExceptionEntry (0x80035c, name ours):
	 * Saves interrupted context into g_cmdIrqCpuContext (0x808000), loads the
	 * kernel stack, dispatches sceSblSkHandlerCmdIrq, and exits via
	 * cmdIrqExceptionExit.
	 */
	.global cmdIrqExceptionEntry
cmdIrqExceptionEntry:
	stc	$sp, $tmp
	movh	$sp, 0x81
	add3	$sp, $sp, -0x8000
	sw	$0, ($sp)
	ldc	$0, $lp
	bsr	skSaveCpuContext
	bsr	skLoadKernelStack
	bsr	sceSblSkHandlerCmdIrq
	jmp	cmdIrqExceptionExit

	/*
	 * swiExceptionEntry (0x80037a, name ours):
	 * Saves calling SM context into g_swiCpuContext (0x808100), clears SWI
	 * bit in $exc, dispatches sceSblSkHandlerSwi, and exits via
	 * swiExceptionExit (returning result in $0).
	 */
	.global swiExceptionEntry
swiExceptionEntry:
	stc	$sp, $tmp
	movh	$sp, 0x81
	add3	$sp, $sp, -0x7f00
	sw	$0, ($sp)
	ldc	$0, $lp
	bsr	skSaveCpuContext
	bsr	skLoadKernelStack
	ldc	$0, $exc
	mov	$12, -17
	and	$0, $12
	stc	$0, $exc
	bsr	sceSblSkHandlerSwi
	jmp	swiExceptionExit

	/*
	 * skGetActiveIrqNumber (0x8003a0, name ours):
	 * Extracts the active interrupt channel number (bits 7..3) from control
	 * bus register 0x0.
	 */
	.global skGetActiveIrqNumber
skGetActiveIrqNumber:
	ldcb	$12, 0x0
	mov	$0, 0xf8
	and	$0, $12
	srl	$0, 3
	ret

	/*
	 * skEnableAllIrqChannels (0x8003ae, name ours):
	 * Writes 0xf to control bus register 0x0.
	 */
	.global skEnableAllIrqChannels
skEnableAllIrqChannels:
	mov	$12, 15
	stcb	$12, 0x0
	nop
	nop
	nop
	ret

	/*
	 * skTriggerArmInterrupt (0x8003bc, name ours):
	 * Writes a 16-bit status/command response to 0xe0000000, issues syncm,
	 * and waits for the ARM side to acknowledge (clear low 16 bits).
	 */
	.global skTriggerArmInterrupt
skTriggerArmInterrupt:
	movh	$3, 0xe000
	extuh	$1
	sw	$1, ($3)
	lw	$2, ($3)
	nop
	syncm
	erepeat	1f
	lw	$2, ($3)
	and3	$2, $2, 0xffff
1:	beqz	$2, 2f
2:	ret
