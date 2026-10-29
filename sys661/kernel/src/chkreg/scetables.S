/* Generated PSP module import/export tables for sceChkreg */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelDeleteSema
	.type	sceKernelDeleteSema, @function
sceKernelDeleteSema: /* ThreadManForKernel 0x28b6489c @ 0x080c */
	jr	$ra
	nop
	.globl	sceKernelSignalSema
	.type	sceKernelSignalSema, @function
sceKernelSignalSema: /* ThreadManForKernel 0x3f53e640 @ 0x0814 */
	jr	$ra
	nop
	.globl	sceKernelWaitSema
	.type	sceKernelWaitSema, @function
sceKernelWaitSema: /* ThreadManForKernel 0x4e3a1105 @ 0x081c */
	jr	$ra
	nop
	.globl	sceKernelCreateSema
	.type	sceKernelCreateSema, @function
sceKernelCreateSema: /* ThreadManForKernel 0xd6da4ba1 @ 0x0824 */
	jr	$ra
	nop
	.globl	sceIdStorageLookup
	.type	sceIdStorageLookup, @function
sceIdStorageLookup: /* sceIdStorage_driver 0x6fe062d1 @ 0x082c */
	jr	$ra
	nop
	.globl	sceIdStorageReadLeaf
	.type	sceIdStorageReadLeaf, @function
sceIdStorageReadLeaf: /* sceIdStorage_driver 0xeb00c509 @ 0x0834 */
	jr	$ra
	nop
	.globl	sceUtilsBufferCopyWithRange
	.type	sceUtilsBufferCopyWithRange, @function
sceUtilsBufferCopyWithRange: /* semaphore 0x4c537c72 @ 0x083c */
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
	.half	2
	.word	.L_exp_tbl_0
	.word	.L_exp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	4, 0
	.half	5
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
	.byte	6, 0
	.half	4
	.word	.L_imp_nid_0
	.word	sceKernelDeleteSema
	.word	0
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_1
	.word	sceIdStorageLookup
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_2
	.word	sceUtilsBufferCopyWithRange

	.section .lib.stub.btm, "a", @progbits
	.align	2
__lib_stub_btm:
	.word	0

	.section .rodata.sceModuleInfo, "a", @progbits
	.align	4
	.globl	module_info
	.type	module_info, @object
module_info:
	.half	0x5006
	.byte	9, 1
	.asciz	"sceChkreg"
	.space	18
	.word	_gp
	.word	__lib_ent_top + 4
	.word	__lib_ent_btm
	.word	__lib_stub_top + 4
	.word	__lib_stub_btm

	.section .rodata.sceResident, "a", @progbits
	.align	2
	.word	0
.L_imp_lib_0:
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_1:
	.asciz	"sceIdStorage_driver"
	.space	4
.L_imp_lib_2:
	.asciz	"semaphore"
	.space	2
.L_exp_tbl_0:
	.word	0xd632acdb
	.word	0xcee8593c
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_start /* module_start (0xd632acdb) */
	.word	module_stop /* module_stop (0xcee8593c) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"sceChkreg_driver"
	.space	3
.L_exp_tbl_1:
	.word	0x54495b19
	.word	0x59f8491d
	.word	0x6894a027
	.word	0x7939c851
	.word	0x9c6e1d34
	.word	sceChkregCheckRegion /* sceChkregCheckRegion (0x54495b19) */
	.word	sceChkregGetPsCode /* sceChkregGetPsCode (0x59f8491d) */
	.word	sceChkregGetPsFlags /* sceChkregGetPsFlags (0x6894a027) */
	.word	sceChkregGetPspModel /* sceChkregGetPspModel (0x7939c851) */
	.word	sceChkreg_driver_9C6E1D34 /* sceChkreg_driver_9C6E1D34 (0x9c6e1d34) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x28b6489c
	.word	0x3f53e640
	.word	0x4e3a1105
	.word	0xd6da4ba1
.L_imp_nid_1:
	.word	0x6fe062d1
	.word	0xeb00c509
.L_imp_nid_2:
	.word	0x4c537c72

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060002
