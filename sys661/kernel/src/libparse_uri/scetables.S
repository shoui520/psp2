/* Generated PSP module import/export tables for SceParseURI_Library */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelMemset
	.type	sceKernelMemset, @function
sceKernelMemset: /* Kernel_Library 0xa089eca4 @ 0x25f0 */
	jr	$ra
	nop
	.globl	_sceKernelCheckThreadStack
	.type	_sceKernelCheckThreadStack, @function
_sceKernelCheckThreadStack: /* Kernel_Library 0xd13bde95 @ 0x25f8 */
	jr	$ra
	nop
	.globl	sceKernelMemcpy
	.type	sceKernelMemcpy, @function
sceKernelMemcpy: /* Kernel_Library 0x1839852a @ 0x2600 */
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
	.half	6
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
	.half	3
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
	.asciz	"SceParseURI_Library"
	.space	8
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
	.asciz	"sceParseUri"
	.align	2
.L_exp_tbl_1:
	.word	0x062bb07e
	.word	0x49e950ec
	.word	0x568518c9
	.word	0x7ee318af
	.word	0x8885a782
	.word	0x8aa55d17
	.word	sceParseUri_062BB07E /* sceParseUri_062BB07E (0x062bb07e) */
	.word	sceParseUri_49E950EC /* sceParseUri_49E950EC (0x49e950ec) */
	.word	sceParseUri_568518C9 /* sceParseUri_568518C9 (0x568518c9) */
	.word	sceParseUri_7EE318AF /* sceParseUri_7EE318AF (0x7ee318af) */
	.word	sceParseUri_8885A782 /* sceParseUri_8885A782 (0x8885a782) */
	.word	sceParseUri_8AA55D17 /* sceParseUri_8AA55D17 (0x8aa55d17) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0xa089eca4
	.word	0xd13bde95
	.word	0x1839852a

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
