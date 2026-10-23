/* Compiler stack-protector traps. A debugger continuing past the second one falls into
 * __aeabi_lmul, linked right after this file as in Sony's layout (CMakeLists.txt).
 */
	.syntax unified
	.arch armv7-a
	.thumb
	.text
	.balign 4
	.global __stack_chk_fail
	.type __stack_chk_fail, %function
	.thumb_func
__stack_chk_fail:
	.fnstart
	bkpt #0x81
	nop
	.fnend
	.size __stack_chk_fail, .-__stack_chk_fail

	.global sceKernelStackChkFail
	.type sceKernelStackChkFail, %function
	.thumb_func
sceKernelStackChkFail:
	.fnstart
	bkpt #0x81
	nop
	.fnend
	.size sceKernelStackChkFail, .-sceKernelStackChkFail
	.section .note.GNU-stack,"",%progbits
