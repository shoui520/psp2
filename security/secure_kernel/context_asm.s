/*
 * context_asm.s: MeP CPU context save/restore and exception return routines
 * for kernel, IRQ, command IRQ, and SWI contexts (0x802768..0x802851).
 *
 * Function names are ours.
 */

	.text

	/*
	 * skSaveKernelContext (0x802768, name ours):
	 * Saves the kernel idle loop's CPU context into g_kernelCpuContext
	 * (0x808080).
	 */
	.global skSaveKernelContext
skSaveKernelContext:
	stc	$sp, $tmp
	movh	$sp, 0x81
	add3	$sp, $sp, -0x7f80
	sw	$0, ($sp)
	ldc	$0, $lp
	jmp	skSaveCpuContext

	/*
	 * skSaveCpuContext (0x80277a, name ours):
	 * Saves $1..$12, $tp, $gp, original $sp (from $tmp), $lp (passed in $0),
	 * $sar, $rpb, $rpe, $rpc, $hi, $lo, and $epc into the 0x60-byte context
	 * frame pointed to by $sp.
	 */
	.global skSaveCpuContext
skSaveCpuContext:
	sw	$1, 0x4($sp)
	sw	$2, 0x8($sp)
	sw	$3, 0xc($sp)
	sw	$4, 0x10($sp)
	sw	$5, 0x14($sp)
	sw	$6, 0x18($sp)
	sw	$7, 0x1c($sp)
	sw	$8, 0x20($sp)
	sw	$9, 0x24($sp)
	sw	$10, 0x28($sp)
	sw	$11, 0x2c($sp)
	sw	$12, 0x30($sp)
	sw	$tp, 0x34($sp)
	sw	$gp, 0x38($sp)
	ldc	$12, $tmp
	sw	$12, 0x3c($sp)
	sw	$0, 0x40($sp)
	ldc	$12, $sar
	sw	$12, 0x44($sp)
	ldc	$12, $rpb
	sw	$12, 0x48($sp)
	ldc	$12, $rpe
	sw	$12, 0x4c($sp)
	ldc	$12, $rpc
	sw	$12, 0x50($sp)
	ldc	$12, $hi
	sw	$12, 0x54($sp)
	ldc	$12, $lo
	sw	$12, 0x58($sp)
	ldc	$12, $epc
	sw	$12, 0x5c($sp)
	ret

	/*
	 * skLoadKernelStack (0x8027ba, name ours):
	 * Loads $tp, $gp, and $sp from g_kernelCpuContext (0x808080).
	 */
	.global skLoadKernelStack
skLoadKernelStack:
	movh	$sp, 0x81
	add3	$sp, $sp, -0x7f80
	jmp	skRestoreKernelStack

	/*
	 * skSetupSmIrqStack (0x8027c6, name ours):
	 * Restores the interrupted SM's $tp and $gp from g_irqCpuContext
	 * (0x807f80) and sets $sp to the SM IRQ stack top (0x80b000).
	 */
	.global skSetupSmIrqStack
skSetupSmIrqStack:
	movh	$sp, 0x80
	add3	$sp, $sp, 0x7f80
	lw	$tp, 0x34($sp)
	lw	$gp, 0x38($sp)
	movh	$sp, 0x81
	add3	$sp, $sp, -0x5000
	ret

	/*
	 * skRestoreKernelStack (0x8027dc, name ours):
	 * Loads $tp, $gp, and $sp from the context frame in $sp.
	 */
	.global skRestoreKernelStack
skRestoreKernelStack:
	lw	$tp, 0x34($sp)
	lw	$gp, 0x38($sp)
	lw	$sp, 0x3c($sp)
	ret

	/*
	 * irqExceptionExit (0x8027e4, name ours):
	 * Restores all registers from g_irqCpuContext (0x807f80) and returns
	 * from interrupt.
	 */
	.global irqExceptionExit
irqExceptionExit:
	movh	$sp, 0x80
	add3	$sp, $sp, 0x7f80
	lw	$0, ($sp)
	jmp	skRestoreCpuContextAndReti

	/*
	 * cmdIrqExceptionExit (0x8027f2, name ours):
	 * Restores all registers from g_cmdIrqCpuContext (0x808000) and returns
	 * from interrupt.
	 */
	.global cmdIrqExceptionExit
cmdIrqExceptionExit:
	movh	$sp, 0x81
	add3	$sp, $sp, -0x8000
	lw	$0, ($sp)
	jmp	skRestoreCpuContextAndReti

	/*
	 * swiExceptionExit (0x802800, name ours):
	 * Restores registers from g_swiCpuContext (0x808100), preserving the
	 * SWI return value in $0, and returns from exception.
	 */
	.global swiExceptionExit
swiExceptionExit:
	movh	$sp, 0x81
	add3	$sp, $sp, -0x7f00
	jmp	skRestoreCpuContextAndReti

	/*
	 * skRestoreCpuContextAndReti (0x80280c, name ours):
	 * Restores control registers and $1..$sp from the context frame in $sp
	 * and executes reti.
	 */
	.global skRestoreCpuContextAndReti
skRestoreCpuContextAndReti:
	lw	$12, 0x5c($sp)
	stc	$12, $epc
	lw	$12, 0x58($sp)
	stc	$12, $lo
	lw	$12, 0x54($sp)
	stc	$12, $hi
	lw	$12, 0x50($sp)
	stc	$12, $rpc
	lw	$12, 0x4c($sp)
	stc	$12, $rpe
	lw	$12, 0x48($sp)
	stc	$12, $rpb
	lw	$12, 0x44($sp)
	stc	$12, $sar
	lw	$12, 0x40($sp)
	stc	$12, $lp
	lw	$12, 0x3c($sp)
	stc	$12, $tmp
	lw	$gp, 0x38($sp)
	lw	$tp, 0x34($sp)
	lw	$12, 0x30($sp)
	lw	$11, 0x2c($sp)
	lw	$10, 0x28($sp)
	lw	$9, 0x24($sp)
	lw	$8, 0x20($sp)
	lw	$7, 0x1c($sp)
	lw	$6, 0x18($sp)
	lw	$5, 0x14($sp)
	lw	$4, 0x10($sp)
	lw	$3, 0xc($sp)
	lw	$2, 0x8($sp)
	lw	$1, 0x4($sp)
	ldc	$sp, $tmp
	nop
	reti
