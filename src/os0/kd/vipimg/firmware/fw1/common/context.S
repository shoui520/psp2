/*
 * fw1 runtime: the core's start, and the switches between the scheduler and the
 * tasks, which change stacks and so cannot be C.
 */
#include "runtime.h"

/* VipTask */
#define TASK_ARGUMENT		0
#define TASK_STACK		4
#define TASK_PC			8
#define TASK_RETURN_ADDRESS	12
#define TASK_PSW		16
#define TASK_INTERRUPT_ENABLE	20

/* VipStatus */
#define STATUS_CURRENT_TASK	24

#define CFG_LOCAL_VECTORS	0x10	/* interrupts enter at VIP_LOCAL_RAM + 0x30 */

	.text
/*
 * The core's entry from the boot block: interrupts off, the scheduler's stack,
 * vectors in local RAM, nothing pending; then tell the debugger the core is up
 * and run the scheduler.
 */
	.global	vipStart
vipStart:
	di
	mov	$12, VIP_PSW_IEP
	stc	$12, $psw
	movh	$sp, VIP_SCHEDULER_STACK >> 16
	or3	$sp, $sp, VIP_SCHEDULER_STACK & 0xffff
	ldc	$12, $cfg
	or3	$12, $12, CFG_LOCAL_VECTORS
	stc	$12, $cfg
	mov	$12, 0
	stcb	$12, VIP_CB_INTERRUPT_STATUS
	stcb	$12, VIP_CB_INTERRUPT_CLEAR
	movu	$4, VIP_DEBUG_REGISTERS + 4 * VIP_CORE
	movh	$12, VIP_DEBUG_RESET_VALUE >> 16
	or3	$12, $12, VIP_DEBUG_RESET_VALUE & 0xffff
	sw	$12, 0($4)
	bsr	schedulerMain
1:	halt
	bra	1b

/*
 * Resume the switched-in task: its interrupts, status and stack, its argument in
 * $1. Outside normal running, a task that returns from its entry yields.
 */
	.global	vipTaskResume
vipTaskResume:
	lw	$2, (g_status)
	lw	$0, (g_currentTaskId)
	sw	$0, STATUS_CURRENT_TASK($2)
	lw	$8, (g_currentTask)
	ldcb	$3, VIP_CB_INTERRUPT_ENABLE
	lw	$4, TASK_INTERRUPT_ENABLE($8)
	lw	$1, TASK_ARGUMENT($8)
	lw	$11, TASK_PC($8)
	lw	$10, TASK_PSW($8)
	or	$4, $3
	di
	stcb	$4, VIP_CB_INTERRUPT_ENABLE
	stc	$10, $psw
	lw	$sp, TASK_STACK($8)
	lw	$0, (g_runMode)
	ei
	beqz	$0, 1f
	movu	$0, vipTaskYield
	stc	$0, $lp
1:	jmp	$11

/*
 * The current task gives up the core: save what the C calling convention keeps
 * across a call, record where to resume it, and go back to the scheduler.
 */
	.global	vipTaskYield
vipTaskYield:
	ldc	$0, $lp
	add	$sp, -32
	sw	$gp, 0x14($sp)
	sw	$0, 0x18($sp)
	sw	$tp, 0x10($sp)
	sw	$8, 0x0c($sp)
	sw	$7, 0x08($sp)
	sw	$6, 0x04($sp)
	sw	$5, 0x00($sp)
	ldcb	$4, VIP_CB_INTERRUPT_ENABLE
	movu	$5, 2f
	mov	$10, $0
	ldc	$11, $psw
	lw	$0, (g_currentTask)
	sw	$sp, TASK_STACK($0)
	sw	$5, TASK_PC($0)
	sw	$10, TASK_RETURN_ADDRESS($0)
	sw	$11, TASK_PSW($0)
	sw	$4, TASK_INTERRUPT_ENABLE($0)
	lw	$2, (g_status)
	mov	$0, -1
	sw	$0, STATUS_CURRENT_TASK($2)
	jmp	schedulerTaskYielded

	/* the task is resumed here */
2:	lw	$gp, 0x14($sp)
	lw	$0, 0x18($sp)
	lw	$tp, 0x10($sp)
	stc	$0, $lp
	lw	$8, 0x0c($sp)
	lw	$7, 0x08($sp)
	lw	$6, 0x04($sp)
	lw	$5, 0x00($sp)
	add3	$sp, $sp, 32
	ret

/* vipRestartOnStack(stack, function): run function() on a new stack */
	.global	vipRestartOnStack
vipRestartOnStack:
	mov	$sp, $1
	jmp	$2

/* vipContinueOnStack(stack, function): run function(stack) on a new stack */
	.global	vipContinueOnStack
vipContinueOnStack:
	mov	$sp, $1
	jmp	$2
