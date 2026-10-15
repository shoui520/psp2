/* Exception-chain entry ABI. Policy handlers receive the shared saved context;
 * these veneers preserve its live pointer and pass control to the next hook.
 * Excpmgr patches each two-word link header when registering the handler.
 */
.syntax unified
.cpu cortex-a9
.arm
.text

.macro DBGP_EXCEPTION_ENTRY entry, handler
.balign 4
.global \entry
.type \entry, %object
\entry:
	.word 0, 0
\entry\()Code:
	ldr r2, [r0, #0x44]
	mov r3, r1
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
	ldr r2, =\entry
	ldr r2, [r2]
	add r2, r2, #8
	mov pc, r2
.size \entry, .-\entry
.endm

DBGP_EXCEPTION_ENTRY dbgpPrefetchAbortEntry, excpHandlerPrefetchAbort
DBGP_EXCEPTION_ENTRY dbgpDataAbortEntry, excpHandlerDataAbort
DBGP_EXCEPTION_ENTRY dbgpUndefinedEntry, excpHandlerUndefined
.ltorg
.section .note.GNU-stack, "", %progbits
