/* Generated PSP module import/export tables for sceCert_Loader */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceIoOpen
	.type	sceIoOpen, @function
sceIoOpen: /* IoFileMgrForKernel 0x109f50bc @ 0x11e0 */
	jr	$ra
	nop
	.globl	sceIoLseek
	.type	sceIoLseek, @function
sceIoLseek: /* IoFileMgrForKernel 0x27eb27b8 @ 0x11e8 */
	jr	$ra
	nop
	.globl	sceIoRead
	.type	sceIoRead, @function
sceIoRead: /* IoFileMgrForKernel 0x6a638d83 @ 0x11f0 */
	jr	$ra
	nop
	.globl	sceIoClose
	.type	sceIoClose, @function
sceIoClose: /* IoFileMgrForKernel 0x810c4bc3 @ 0x11f8 */
	jr	$ra
	nop
	.globl	sceKernelGetCompiledSdkVersion
	.type	sceKernelGetCompiledSdkVersion, @function
sceKernelGetCompiledSdkVersion: /* SysMemForKernel 0xb4f00cb5 @ 0x1200 */
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
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x4001
	.byte	4, 0
	.half	1
	.word	.L_exp_tbl_2

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
	.word	sceKernelGetCompiledSdkVersion
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	4
	.word	.L_imp_nid_1
	.word	sceIoOpen
	.word	0

	.section .lib.stub.btm, "a", @progbits
	.align	2
__lib_stub_btm:
	.word	0

	.section .rodata.sceModuleInfo, "a", @progbits
	.align	4
	.globl	module_info
	.type	module_info, @object
module_info:
	.half	0x1006
	.byte	2, 1
	.asciz	"sceCert_Loader"
	.space	13
	.word	_gp
	.word	__lib_ent_top + 4
	.word	__lib_ent_btm
	.word	__lib_stub_top + 4
	.word	__lib_stub_btm

	.section .rodata.sceResident, "a", @progbits
	.align	2
	.word	0
.L_imp_lib_0:
	.asciz	"SysMemForKernel"
	.space	4
.L_imp_lib_1:
	.asciz	"IoFileMgrForKernel"
	.space	1
.L_exp_tbl_0:
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"sceCertLoader_driver"
	.space	3
.L_exp_tbl_1:
	.word	0x11391041
	.word	0xe86a7dc1
	.word	sceCertLoader_driver_11391041 /* sceCertLoader_driver_11391041 (0x11391041) */
	.word	sceCertLoader_driver_E86A7DC1 /* sceCertLoader_driver_E86A7DC1 (0xe86a7dc1) */
.L_exp_lib_2:
	.asciz	"sceCertLoader"
	.space	2
.L_exp_tbl_2:
	.word	0xdd629a24
	.word	sceCertLoader_DD629A24 /* sceCertLoader_DD629A24 (0xdd629a24) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0xb4f00cb5
.L_imp_nid_1:
	.word	0x109f50bc
	.word	0x27eb27b8
	.word	0x6a638d83
	.word	0x810c4bc3

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
