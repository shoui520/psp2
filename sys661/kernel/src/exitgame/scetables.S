/* Generated PSP module import/export tables for sceBlueScreenOfDeath */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelReleaseExceptionHandler
	.type	sceKernelReleaseExceptionHandler, @function
sceKernelReleaseExceptionHandler: /* ExceptionManagerForKernel 0x54b0cad2 @ 0x064c */
	jr	$ra
	nop
	.globl	sceKernelRegisterPriorityExceptionHandler
	.type	sceKernelRegisterPriorityExceptionHandler, @function
sceKernelRegisterPriorityExceptionHandler: /* ExceptionManagerForKernel 0x7c11759d @ 0x0654 */
	jr	$ra
	nop
	.globl	sceKernelRegisterExceptionHandler
	.type	sceKernelRegisterExceptionHandler, @function
sceKernelRegisterExceptionHandler: /* ExceptionManagerForKernel 0xfbc2353a @ 0x065c */
	jr	$ra
	nop
	.globl	sceKernelCpuSuspendIntr
	.type	sceKernelCpuSuspendIntr, @function
sceKernelCpuSuspendIntr: /* InterruptManagerForKernel 0x092968f4 @ 0x0664 */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntr
	.type	sceKernelCpuResumeIntr, @function
sceKernelCpuResumeIntr: /* InterruptManagerForKernel 0x5f10d406 @ 0x066c */
	jr	$ra
	nop
	.globl	sceKernelMemset
	.type	sceKernelMemset, @function
sceKernelMemset: /* SysMemForKernel 0x8ae776af @ 0x0674 */
	jr	$ra
	nop
	.globl	sceKernelRegisterDeadThreadHandler
	.type	sceKernelRegisterDeadThreadHandler, @function
sceKernelRegisterDeadThreadHandler: /* ThreadManForKernel 0x80ca77ba @ 0x067c */
	jr	$ra
	nop

	.section .lib.ent.top, "a", @progbits
	.align	2
__lib_ent_top:
	.word	0

	.section .lib.ent, "a", @progbits
	.align	2
	.word	0
	.byte	0, 0
	.half	0x8000
	.byte	4, 2
	.half	3
	.word	.L_exp_tbl_0

	.section .lib.ent.btm, "a", @progbits
	.align	2
__lib_ent_btm:
	.word	0

	.section .lib.stub.top, "a", @progbits
	.align	2
__lib_stub_top:
	.word	0

	.section .lib.stub, "a", @progbits
	.align	2
	.word	.L_imp_lib_0
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	3
	.word	.L_imp_nid_0
	.word	sceKernelReleaseExceptionHandler
	.word	0
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0009
	.byte	6, 0
	.half	2
	.word	.L_imp_nid_1
	.word	sceKernelCpuSuspendIntr
	.word	0
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	1
	.word	.L_imp_nid_2
	.word	sceKernelRegisterDeadThreadHandler
	.word	0
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_3
	.word	sceKernelMemset

	.section .lib.stub.btm, "a", @progbits
	.align	2
__lib_stub_btm:
	.word	0

	.section .rodata.sceModuleInfo, "a", @progbits
	.align	4
	.globl	module_info
	.type	module_info, @object
module_info:
	.half	0x1007
	.byte	8, 1
	.asciz	"sceBlueScreenOfDeath"
	.space	7
	.word	_gp
	.word	__lib_ent_top + 4
	.word	__lib_ent_btm
	.word	__lib_stub_top + 4
	.word	__lib_stub_btm

	.section .rodata.sceResident, "a", @progbits
	.align	2
	.word	0
.L_imp_lib_0:
	.asciz	"ExceptionManagerForKernel"
	.space	6
.L_imp_lib_1:
	.asciz	"InterruptManagerForKernel"
	.space	6
.L_imp_lib_2:
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_3:
	.asciz	"SysMemForKernel"
	.align	2
.L_exp_tbl_0:
	.word	0xd632acdb
	.word	0xd3744be0
	.word	0x2f064fa6
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_bootstart /* module_start (0xd632acdb) */
	.word	module_bootstart /* module_bootstart (0xd3744be0) */
	.word	module_reboot_before /* module_reboot_before (0x2f064fa6) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x54b0cad2
	.word	0x7c11759d
	.word	0xfbc2353a
.L_imp_nid_1:
	.word	0x092968f4
	.word	0x5f10d406
.L_imp_nid_2:
	.word	0x80ca77ba
.L_imp_nid_3:
	.word	0x8ae776af

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
