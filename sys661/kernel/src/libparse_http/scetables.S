/* Generated PSP module import/export tables for SceParseHTTPheader_Library */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelCheckThreadStack
	.type	sceKernelCheckThreadStack, @function
sceKernelCheckThreadStack: /* Kernel_Library 0xd13bde95 @ 0x0768 */
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
	.half	0
	.word	.L_exp_tbl_0
	.word	.L_exp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	4, 0
	.half	2
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
	.word	.L_imp_lib_0
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_0
	.word	sceKernelCheckThreadStack

	.section .lib.stub.btm, "a", @progbits
	.align	2
__lib_stub_btm:
	.word	0

	.section .rodata.sceModuleInfo, "a", @progbits
	.align	4
	.globl	module_info
	.type	module_info, @object
module_info:
	.half	0x0006
	.byte	1, 1
	.asciz	"SceParseHTTPheader_Library"
	.space	1
	.word	_gp
	.word	__lib_ent_top + 4
	.word	__lib_ent_btm
	.word	__lib_stub_top + 4
	.word	__lib_stub_btm

	.section .rodata.sceResident, "a", @progbits
	.align	2
	.word	0
.L_imp_lib_0:
	.asciz	"Kernel_Library"
	.space	1
.L_exp_tbl_0:
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"sceParseHttp"
	.space	3
.L_exp_tbl_1:
	.word	0x8077a433
	.word	0xad7bfdef
	.word	sceParseHttp_8077A433 /* sceParseHttp_8077A433 (0x8077a433) */
	.word	sceParseHttp_AD7BFDEF /* sceParseHttp_AD7BFDEF (0xad7bfdef) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0xd13bde95

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
