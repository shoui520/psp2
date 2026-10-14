/*
 * fw1 runtime: the common path of every interrupt, from local RAM.
 */
#include "interrupt.h"

	.section .lram.text, "ax"

/* the frame: the stub's $1 above it */
#define FRAME		84
#define SAVED_LP	0
#define SAVED_SAR	4
#define SAVED_RPB	8
#define SAVED_RPE	12
#define SAVED_RPC	16
#define SAVED_HI	20
#define SAVED_LO	24
#define SAVED_GP	28
#define SAVED_TP	32
#define SAVED_R(n)	(80 - 4 * (n))	/* $0, $2 to $12 */
#define SAVED_R1	FRAME

/*
 * Save the interrupted context, acknowledge the interrupt if the code says so,
 * call the handler of the code's slot, if any, with the code's argument, and
 * return to the interrupted code.
 */
	.global	interruptDispatch
interruptDispatch:
	add3	$sp, $sp, -FRAME
	sw	$0, SAVED_R(0)($sp)
	sw	$2, SAVED_R(2)($sp)
	sw	$3, SAVED_R(3)($sp)
	sw	$4, SAVED_R(4)($sp)
	sw	$5, SAVED_R(5)($sp)
	sw	$6, SAVED_R(6)($sp)
	sw	$7, SAVED_R(7)($sp)
	sw	$8, SAVED_R(8)($sp)
	sw	$9, SAVED_R(9)($sp)
	sw	$10, SAVED_R(10)($sp)
	sw	$11, SAVED_R(11)($sp)
	sw	$12, SAVED_R(12)($sp)
	sw	$tp, SAVED_TP($sp)
	sw	$gp, SAVED_GP($sp)
	ldc	$0, $lp
	sw	$0, SAVED_LP($sp)
	ldc	$0, $sar
	sw	$0, SAVED_SAR($sp)
	ldc	$0, $rpb
	sw	$0, SAVED_RPB($sp)
	ldc	$0, $rpe
	sw	$0, SAVED_RPE($sp)
	ldc	$0, $rpc
	sw	$0, SAVED_RPC($sp)
	ldc	$0, $hi
	sw	$0, SAVED_HI($sp)
	ldc	$0, $lo
	sw	$0, SAVED_LO($sp)

	and3	$2, $1, 0x7f
	and3	$0, $1, INTERRUPT_ACKNOWLEDGE
	beqz	$0, 1f
	mov	$3, 1
	sll	$3, $2
	nor	$3, $3
	stcb	$3, VIP_CB_INTERRUPT_CLEAR
1:	and3	$4, $1, 0xf << INTERRUPT_SLOT_SHIFT
	movu	$0, g_interruptHandlers
	srl	$4, INTERRUPT_SLOT_SHIFT - 2
	add3	$0, $0, $4
	lw	$4, 0($0)
	beqz	$4, 2f
	srl	$1, INTERRUPT_ARGUMENT_SHIFT
	jsr	$4

2:	lw	$0, SAVED_LP($sp)
	stc	$0, $lp
	lw	$0, SAVED_SAR($sp)
	stc	$0, $sar
	lw	$0, SAVED_RPB($sp)
	stc	$0, $rpb
	lw	$0, SAVED_RPE($sp)
	stc	$0, $rpe
	lw	$0, SAVED_RPC($sp)
	stc	$0, $rpc
	lw	$0, SAVED_HI($sp)
	stc	$0, $hi
	lw	$0, SAVED_LO($sp)
	stc	$0, $lo
	lw	$0, SAVED_R(0)($sp)
	lw	$1, SAVED_R1($sp)
	lw	$2, SAVED_R(2)($sp)
	lw	$3, SAVED_R(3)($sp)
	lw	$4, SAVED_R(4)($sp)
	lw	$5, SAVED_R(5)($sp)
	lw	$6, SAVED_R(6)($sp)
	lw	$7, SAVED_R(7)($sp)
	lw	$8, SAVED_R(8)($sp)
	lw	$9, SAVED_R(9)($sp)
	lw	$10, SAVED_R(10)($sp)
	lw	$11, SAVED_R(11)($sp)
	lw	$12, SAVED_R(12)($sp)
	lw	$tp, SAVED_TP($sp)
	lw	$gp, SAVED_GP($sp)
	add3	$sp, $sp, FRAME + 4
	reti

