/*
 * What C cannot express of the exception hooks (drivers/exception.c): the
 * jump words a hooked vector slot takes, the entries that save the state the
 * exception interrupted into this core's frame, run the hook and restore it,
 * and vkExceptionResume.
 *
 * Control registers 30 and 31 (the debug and the normal vector bases) have
 * no assembler name.
 */
#include <vnz.h>

#define VK_EXCEPTION_DEBUG	6
#define VK_EXCEPTION_STATE	0xc0		/* sizeof(VkExceptionState) */
#define VK_CONTROL(n)		(64 + (n) * 4)	/* VkExceptionState.control[n] */
#define HOOKS_FRAMES		0		/* VkExceptionHooks.frames */
#define HOOKS_HANDLERS		32		/* VkExceptionHooks.handlers */
#define HOOKS_KINDS		68		/* VkExceptionHooks.kinds */

	.section .exception.text, "ax"
	.p2align 2

/*
 * The vector words: g_vkExceptionSaved[kind] receives what the hooked slot
 * held, g_vkExceptionJumps[kind] is the word a hooked slot gets. The words
 * of unused kinds jump to themselves.
 */
	.global	g_vkExceptionSaved
g_vkExceptionSaved:
	.irp	kind, 0, 1, 2, 3, 4, 5, 6, 7, 8
0:	jmp	0b
	.endr

	.global	g_vkExceptionJumps
g_vkExceptionJumps:
0:	jmp	0b
	jmp	exceptionNmi
	jmp	exceptionReservedInstruction
	jmp	exceptionZeroDivide
	jmp	exceptionBreak
0:	jmp	0b
	jmp	exceptionDebug
0:	jmp	0b
0:	jmp	0b

	.p2align 1
#define EXCEPTION_ENTRY(name, kind)	\
name:					\
	add	$sp, -4;		\
	sw	$1, ($sp);		\
	mov	$1, kind;		\
	jmp	exceptionEntry

	EXCEPTION_ENTRY(exceptionNmi, 1)
	EXCEPTION_ENTRY(exceptionReservedInstruction, 2)
	EXCEPTION_ENTRY(exceptionZeroDivide, 3)
	EXCEPTION_ENTRY(exceptionBreak, 4)
	EXCEPTION_ENTRY(exceptionDebug, VK_EXCEPTION_DEBUG)

/*
 * $1 = the kind, the interrupted $1 on the stack. Record the kind, save
 * everything into this core's frame, call the hook on the frame as stack with
 * the kind, and return to what the state then says.
 */
exceptionEntry:
	add	$sp, -8
	sw	$0, ($sp)
	sw	$2, 4($sp)
	ldc	$0, $id
	srl	$0, 16
	and3	$0, $0, 7
	movh	$2, %hi(g_vkExceptionHooks + HOOKS_KINDS)
	add3	$2, $2, %lo(g_vkExceptionHooks + HOOKS_KINDS)
	add3	$2, $2, $0
	sb	$1, ($2)
	movh	$2, %hi(g_vkExceptionHooks + HOOKS_FRAMES)
	add3	$2, $2, %lo(g_vkExceptionHooks + HOOKS_FRAMES)
	sll	$0, 2
	add3	$2, $2, $0
	lw	$2, ($2)
	add3	$2, $2, -VK_EXCEPTION_STATE

	lw	$0, ($sp)
	sw	$0, 0($2)
	lw	$0, 8($sp)
	sw	$0, 4($2)
	lw	$0, 4($sp)
	sw	$0, 8($2)
	sw	$3, 12($2)
	sw	$4, 16($2)
	sw	$5, 20($2)
	sw	$6, 24($2)
	sw	$7, 28($2)
	sw	$8, 32($2)
	sw	$9, 36($2)
	sw	$10, 40($2)
	sw	$11, 44($2)
	sw	$12, 48($2)
	sw	$tp, 52($2)
	sw	$gp, 56($2)
	add3	$0, $sp, 12
	sw	$0, 60($2)

#define SAVE(reg, n)	ldc $0, reg; sw $0, VK_CONTROL(n)($2)
	SAVE($pc, VNZ_CR_PC)
	SAVE($lp, VNZ_CR_LP)
	SAVE($sar, VNZ_CR_SAR)
	SAVE($rpb, VNZ_CR_RPB)
	SAVE($rpe, VNZ_CR_RPE)
	SAVE($rpc, VNZ_CR_RPC)
	SAVE($hi, VNZ_CR_HI)
	SAVE($lo, VNZ_CR_LO)
	SAVE($mb0, VNZ_CR_MB0)
	SAVE($me0, VNZ_CR_ME0)
	SAVE($mb1, VNZ_CR_MB1)
	SAVE($me1, VNZ_CR_ME1)
	SAVE($psw, VNZ_CR_PSW)
	SAVE($id, VNZ_CR_ID)
	SAVE($tmp, VNZ_CR_TMP)
	SAVE($epc, VNZ_CR_EPC)
	SAVE($exc, VNZ_CR_EXC)
	SAVE($cfg, VNZ_CR_CFG)
	SAVE($vid, VNZ_CR_VID)
	SAVE($npc, VNZ_CR_NPC)
	bnei	$1, VK_EXCEPTION_DEBUG, 1f
	SAVE($dbg, VNZ_CR_DBG)
	SAVE($depc, VNZ_CR_DEPC)
1:	SAVE($opt, VNZ_CR_OPT)
	SAVE($rcfg, VNZ_CR_RCFG)
	SAVE($ccfg, VNZ_CR_CCFG)
	SAVE(30, VNZ_CR_RES30)
	SAVE(31, VNZ_CR_RES31)

	mov	$sp, $2
	movh	$2, %hi(g_vkExceptionHooks + HOOKS_HANDLERS)
	add3	$2, $2, %lo(g_vkExceptionHooks + HOOKS_HANDLERS)
	sll	$1, 2
	add3	$2, $2, $1
	lw	$2, ($2)
	jsr	$2

	/* the frame again, and the kind */
	ldc	$0, $id
	srl	$0, 16
	and3	$0, $0, 7
	movh	$2, %hi(g_vkExceptionHooks)
	add3	$1, $2, %lo(g_vkExceptionHooks + HOOKS_KINDS)
	add3	$2, $2, %lo(g_vkExceptionHooks + HOOKS_FRAMES)
	add3	$1, $1, $0
	lb	$1, ($1)
	sll	$0, 2
	add3	$2, $2, $0
	lw	$2, ($2)
	add3	$2, $2, -VK_EXCEPTION_STATE

#define RESTORE(base, reg, n)	lw $0, VK_CONTROL(n)(base); stc $0, reg
#define RESTORE_STATE(base, kind)				\
	RESTORE(base, 31, VNZ_CR_RES31);			\
	RESTORE(base, 30, VNZ_CR_RES30);			\
	RESTORE(base, $ccfg, VNZ_CR_CCFG);			\
	RESTORE(base, $rcfg, VNZ_CR_RCFG);			\
	RESTORE(base, $opt, VNZ_CR_OPT);			\
	bnei	kind, VK_EXCEPTION_DEBUG, 1f;			\
	RESTORE(base, $depc, VNZ_CR_DEPC);			\
	RESTORE(base, $dbg, VNZ_CR_DBG);			\
1:	RESTORE(base, $npc, VNZ_CR_NPC);			\
	RESTORE(base, $vid, VNZ_CR_VID);				\
	RESTORE(base, $cfg, VNZ_CR_CFG);			\
	RESTORE(base, $exc, VNZ_CR_EXC);			\
	RESTORE(base, $epc, VNZ_CR_EPC);			\
	RESTORE(base, $tmp, VNZ_CR_TMP);			\
	RESTORE(base, $id, VNZ_CR_ID);				\
	RESTORE(base, $psw, VNZ_CR_PSW);			\
	RESTORE(base, $me1, VNZ_CR_ME1);			\
	RESTORE(base, $mb1, VNZ_CR_MB1);			\
	RESTORE(base, $me0, VNZ_CR_ME0);			\
	RESTORE(base, $mb0, VNZ_CR_MB0);			\
	RESTORE(base, $lo, VNZ_CR_LO);				\
	RESTORE(base, $hi, VNZ_CR_HI);				\
	RESTORE(base, $rpc, VNZ_CR_RPC);			\
	RESTORE(base, $rpe, VNZ_CR_RPE);			\
	RESTORE(base, $rpb, VNZ_CR_RPB);			\
	RESTORE(base, $sar, VNZ_CR_SAR);			\
	RESTORE(base, $lp, VNZ_CR_LP);				\
	RESTORE(base, $pc, VNZ_CR_PC);				\
	lw	$sp, 60(base);					\
	lw	$gp, 56(base);					\
	lw	$tp, 52(base);					\
	lw	$12, 48(base);					\
	lw	$11, 44(base);					\
	lw	$10, 40(base);					\
	lw	$9, 36(base);					\
	lw	$8, 32(base);					\
	lw	$7, 28(base);					\
	lw	$6, 24(base);					\
	lw	$5, 20(base);					\
	lw	$4, 16(base);					\
	lw	$3, 12(base);					\
	lw	$0, 0(base)

	RESTORE_STATE($2, $1)
	beqi	$1, VK_EXCEPTION_DEBUG, 2f
	lw	$1, 4($2)
	lw	$2, 8($2)
	reti
2:	lw	$1, 4($2)
	lw	$2, 8($2)
	dret

/*
 * vkExceptionResume(state, target, kind): leave a hook for target with the
 * state restored, its $0 in $tmp (target is the vector word the hook
 * replaced: what it jumps to takes $0 from $tmp).
 */
	.global	vkExceptionResume
vkExceptionResume:
	RESTORE_STATE($1, $3)
	stc	$0, $tmp
	mov	$0, $2
	lw	$2, 8($1)
	lw	$1, 4($1)
	jmp	$0
