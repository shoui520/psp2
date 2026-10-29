/* Generated PSP module import/export tables for sceExceptionManager */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits

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
	.half	1
	.word	.L_exp_tbl_0
	.word	.L_exp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	4, 1
	.half	10
	.word	.L_exp_tbl_1

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
	.byte	3, 1
	.asciz	"sceExceptionManager"
	.space	8
	.word	_gp
	.word	__lib_ent_top + 4
	.word	__lib_ent_btm
	.word	__lib_stub_top + 4
	.word	__lib_stub_btm

	.section .rodata.sceResident, "a", @progbits
	.align	2
	.word	0
.L_exp_tbl_0:
	.word	0xd3744be0
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_bootstart /* module_bootstart (0xd3744be0) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"ExceptionManagerForKernel"
	.space	2
.L_exp_tbl_1:
	.word	0x0a998599
	.word	0x15adc862
	.word	0x27e6a663
	.word	0x409dbb53
	.word	0x54b0cad2
	.word	0x79454858
	.word	0x7c11759d
	.word	0x96d47f7e
	.word	0xb15357c9
	.word	0xfbc2353a
	.word	0x6437d22a
	.word	sceKernelRegisterDefaultExceptionHandler /* sceKernelRegisterDefaultExceptionHandler (0x0a998599) */
	.word	sceKernelRegisterNmiHandler /* sceKernelRegisterNmiHandler (0x15adc862) */
	.word	sceKernelGetActiveDefaultExceptionHandler /* sceKernelGetActiveDefaultExceptionHandler (0x27e6a663) */
	.word	sceKernelReleaseDefaultExceptionHandler /* sceKernelReleaseDefaultExceptionHandler (0x409dbb53) */
	.word	sceKernelReleaseExceptionHandler /* sceKernelReleaseExceptionHandler (0x54b0cad2) */
	.word	ExceptionManagerForKernel_79454858 /* ExceptionManagerForKernel_79454858 (0x79454858) */
	.word	sceKernelRegisterPriorityExceptionHandler /* sceKernelRegisterPriorityExceptionHandler (0x7c11759d) */
	.word	ExceptionManagerForKernel_96D47F7E /* ExceptionManagerForKernel_96D47F7E (0x96d47f7e) */
	.word	sceKernelReleaseNmiHandler /* sceKernelReleaseNmiHandler (0xb15357c9) */
	.word	sceKernelRegisterExceptionHandler /* sceKernelRegisterExceptionHandler (0xfbc2353a) */
	.word	ExcepManCB /* ExcepManCB (0x6437d22a) */

	.section .rodata.sceNid, "a", @progbits
	.align	2

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
