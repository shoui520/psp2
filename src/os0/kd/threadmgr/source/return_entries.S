/*
 * Thread-context return ABI. Restored LR values address these Thumb branches,
 * after two reserved patch slots. Preserve the returned value, registers and
 * LR when entering the C return policy.
 */
.syntax unified
.thumb
.text

.macro THREAD_RETURN_ENTRY name, target
	nop
	nop
.global \name
.type \name, %function
.thumb_func
\name:
	b.w	\target
.size \name, .-\name
.endm

THREAD_RETURN_ENTRY kernelThreadReturnEntry, kernelThreadReturn
THREAD_RETURN_ENTRY returnFromKernelCallbackEntry, returnFromKernelCallback
THREAD_RETURN_ENTRY returnFromWorkTaskEntry, returnFromWorkTask

.section .note.GNU-stack,"",%progbits
