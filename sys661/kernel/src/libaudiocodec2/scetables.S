/* Generated PSP module import/export tables for sceAudiocodec_Library2 */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelMemset
	.type	sceKernelMemset, @function
sceKernelMemset: /* Kernel_Library 0xa089eca4 @ 0x2794 */
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
	.half	0x0009
	.byte	4, 0
	.half	3
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
	.half	0x0006
	.byte	1, 1
	.asciz	"sceAudiocodec_Library2"
	.space	5
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
	.asciz	"sceAudiocodec2"
	.space	1
.L_exp_tbl_1:
	.word	0x21d5a83f
	.word	0xaa2ed267
	.word	0xe063d402
	.word	sceAudiocodec2_21D5A83F /* sceAudiocodec2_21D5A83F (0x21d5a83f) */
	.word	sceAudiocodec2_AA2ED267 /* sceAudiocodec2_AA2ED267 (0xaa2ed267) */
	.word	sceAudiocodec2_E063D402 /* sceAudiocodec2_E063D402 (0xe063d402) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0xa089eca4

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
