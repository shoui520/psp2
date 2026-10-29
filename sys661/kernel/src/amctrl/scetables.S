/* Generated PSP module import/export tables for sceAmctrl_driver */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelInitDiscImage
	.type	sceKernelInitDiscImage, @function
sceKernelInitDiscImage: /* InitForKernel 0xa18a4a8b @ 0x1ae0 */
	jr	$ra
	nop
	.globl	sceKernelMemmove
	.type	sceKernelMemmove, @function
sceKernelMemmove: /* SysMemForKernel 0x9bac123d @ 0x1ae8 */
	jr	$ra
	nop
	.globl	sceKernelDeleteSema
	.type	sceKernelDeleteSema, @function
sceKernelDeleteSema: /* ThreadManForKernel 0x28b6489c @ 0x1af0 */
	jr	$ra
	nop
	.globl	sceKernelSignalSema
	.type	sceKernelSignalSema, @function
sceKernelSignalSema: /* ThreadManForKernel 0x3f53e640 @ 0x1af8 */
	jr	$ra
	nop
	.globl	sceKernelWaitSema
	.type	sceKernelWaitSema, @function
sceKernelWaitSema: /* ThreadManForKernel 0x4e3a1105 @ 0x1b00 */
	jr	$ra
	nop
	.globl	sceKernelTryAllocateFpl
	.type	sceKernelTryAllocateFpl, @function
sceKernelTryAllocateFpl: /* ThreadManForKernel 0x623ae665 @ 0x1b08 */
	jr	$ra
	nop
	.globl	sceKernelCreateFpl
	.type	sceKernelCreateFpl, @function
sceKernelCreateFpl: /* ThreadManForKernel 0xc07bb470 @ 0x1b10 */
	jr	$ra
	nop
	.globl	sceKernelCreateSema
	.type	sceKernelCreateSema, @function
sceKernelCreateSema: /* ThreadManForKernel 0xd6da4ba1 @ 0x1b18 */
	jr	$ra
	nop
	.globl	sceKernelDeleteFpl
	.type	sceKernelDeleteFpl, @function
sceKernelDeleteFpl: /* ThreadManForKernel 0xed1410e0 @ 0x1b20 */
	jr	$ra
	nop
	.globl	sceUtilsBufferCopyWithRange
	.type	sceUtilsBufferCopyWithRange, @function
sceUtilsBufferCopyWithRange: /* semaphore 0x4c537c72 @ 0x1b28 */
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
	.half	1
	.word	.L_exp_tbl_0
	.word	.L_exp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	4, 0
	.half	9
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
	.half	7
	.word	.L_imp_nid_0
	.word	sceKernelDeleteSema
	.word	0
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_1
	.word	sceKernelMemmove
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_2
	.word	sceKernelInitDiscImage
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_3
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
	.half	0x5007
	.byte	4, 1
	.asciz	"sceAmctrl_driver"
	.space	11
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
	.asciz	"SysMemForKernel"
	.space	4
.L_imp_lib_2:
	.asciz	"InitForKernel"
	.space	6
.L_imp_lib_3:
	.asciz	"semaphore"
	.space	2
.L_exp_tbl_0:
	.word	0xd632acdb
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_start /* module_start (0xd632acdb) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"sceAmctrl_driver"
	.space	3
.L_exp_tbl_1:
	.word	0x0785c974
	.word	0x1ccb66d2
	.word	0x525b8218
	.word	0x58163fbe
	.word	0x9227ea79
	.word	0x9951c50f
	.word	0xe04add4c
	.word	0xef95a213
	.word	0xf5186d8e
	.word	sceDrmBBCipherUpdate /* sceDrmBBCipherUpdate (0x0785c974) */
	.word	sceDrmBBCipherInit /* sceDrmBBCipherInit (0x1ccb66d2) */
	.word	sceDrmBBMacInit /* sceDrmBBMacInit (0x525b8218) */
	.word	sceDrmBBMacUpdate /* sceDrmBBMacUpdate (0x58163fbe) */
	.word	sceAmctrl_driver_9227EA79 /* sceAmctrl_driver_9227EA79 (0x9227ea79) */
	.word	sceDrmBBCipherFinal /* sceDrmBBCipherFinal (0x9951c50f) */
	.word	sceAmctrl_driver_E04ADD4C /* sceAmctrl_driver_E04ADD4C (0xe04add4c) */
	.word	sceDrmBBMacFinal /* sceDrmBBMacFinal (0xef95a213) */
	.word	sceDrmBBMacFinal2 /* sceDrmBBMacFinal2 (0xf5186d8e) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x28b6489c
	.word	0x3f53e640
	.word	0x4e3a1105
	.word	0x623ae665
	.word	0xc07bb470
	.word	0xd6da4ba1
	.word	0xed1410e0
.L_imp_nid_1:
	.word	0x9bac123d
.L_imp_nid_2:
	.word	0xa18a4a8b
.L_imp_nid_3:
	.word	0x4c537c72

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060002
