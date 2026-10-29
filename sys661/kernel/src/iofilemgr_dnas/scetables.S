/* Generated PSP module import/export tables for sceIoFilemgrDNAS */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelApplicationType
	.type	sceKernelApplicationType, @function
sceKernelApplicationType: /* InitForKernel 0x7233b5bc @ 0x1630 */
	jr	$ra
	nop
	.globl	sceKernelInitDiscImage
	.type	sceKernelInitDiscImage, @function
sceKernelInitDiscImage: /* InitForKernel 0xa18a4a8b @ 0x1638 */
	jr	$ra
	nop
	.globl	sceIoAddHook
	.type	sceIoAddHook, @function
sceIoAddHook: /* IoFileMgrForKernel 0x22f15793 @ 0x1640 */
	jr	$ra
	nop
	.globl	sceKernelMemcpy
	.type	sceKernelMemcpy, @function
sceKernelMemcpy: /* SysMemForKernel 0x181065ab @ 0x1648 */
	jr	$ra
	nop
	.globl	sceKernelMemset
	.type	sceKernelMemset, @function
sceKernelMemset: /* SysMemForKernel 0x8ae776af @ 0x1650 */
	jr	$ra
	nop
	.globl	memcmp
	.type	memcmp, @function
memcmp: /* SysclibForKernel 0x81d0d1f7 @ 0x1658 */
	jr	$ra
	nop
	.globl	sceKernelDeleteSema
	.type	sceKernelDeleteSema, @function
sceKernelDeleteSema: /* ThreadManForKernel 0x28b6489c @ 0x1660 */
	jr	$ra
	nop
	.globl	sceKernelSignalSema
	.type	sceKernelSignalSema, @function
sceKernelSignalSema: /* ThreadManForKernel 0x3f53e640 @ 0x1668 */
	jr	$ra
	nop
	.globl	sceKernelWaitSema
	.type	sceKernelWaitSema, @function
sceKernelWaitSema: /* ThreadManForKernel 0x4e3a1105 @ 0x1670 */
	jr	$ra
	nop
	.globl	sceKernelCreateSema
	.type	sceKernelCreateSema, @function
sceKernelCreateSema: /* ThreadManForKernel 0xd6da4ba1 @ 0x1678 */
	jr	$ra
	nop
	.globl	sceDrmBBCipherUpdate
	.type	sceDrmBBCipherUpdate, @function
sceDrmBBCipherUpdate: /* sceAmctrl_driver 0x0785c974 @ 0x1680 */
	jr	$ra
	nop
	.globl	sceDrmBBCipherInit
	.type	sceDrmBBCipherInit, @function
sceDrmBBCipherInit: /* sceAmctrl_driver 0x1ccb66d2 @ 0x1688 */
	jr	$ra
	nop
	.globl	sceDrmBBMacInit
	.type	sceDrmBBMacInit, @function
sceDrmBBMacInit: /* sceAmctrl_driver 0x525b8218 @ 0x1690 */
	jr	$ra
	nop
	.globl	sceAmctrl_driver_9227EA79
	.type	sceAmctrl_driver_9227EA79, @function
sceAmctrl_driver_9227EA79: /* sceAmctrl_driver 0x9227ea79 @ 0x1698 */
	jr	$ra
	nop
	.globl	sceDrmBBCipherFinal
	.type	sceDrmBBCipherFinal, @function
sceDrmBBCipherFinal: /* sceAmctrl_driver 0x9951c50f @ 0x16a0 */
	jr	$ra
	nop
	.globl	sceAmctrl_driver_E04ADD4C
	.type	sceAmctrl_driver_E04ADD4C, @function
sceAmctrl_driver_E04ADD4C: /* sceAmctrl_driver 0xe04add4c @ 0x16a8 */
	jr	$ra
	nop
	.globl	sceDrmBBMacFinal
	.type	sceDrmBBMacFinal, @function
sceDrmBBMacFinal: /* sceAmctrl_driver 0xef95a213 @ 0x16b0 */
	jr	$ra
	nop
	.globl	sceDrmBBMacFinal2
	.type	sceDrmBBMacFinal2, @function
sceDrmBBMacFinal2: /* sceAmctrl_driver 0xf5186d8e @ 0x16b8 */
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
	.half	4
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
	.byte	5, 0
	.half	8
	.word	.L_imp_nid_0
	.word	sceDrmBBCipherUpdate
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_1
	.word	sceKernelMemcpy
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_2
	.word	memcmp
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	4
	.word	.L_imp_nid_3
	.word	sceKernelDeleteSema
	.word	0
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	1
	.word	.L_imp_nid_4
	.word	sceIoAddHook
	.word	0
	.word	.L_imp_lib_5
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_5
	.word	sceKernelApplicationType

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
	.byte	9, 1
	.asciz	"sceIoFilemgrDNAS"
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
	.asciz	"sceAmctrl_driver"
	.space	7
.L_imp_lib_1:
	.asciz	"SysMemForKernel"
	.space	4
.L_imp_lib_2:
	.asciz	"SysclibForKernel"
	.space	7
.L_imp_lib_3:
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_4:
	.asciz	"IoFileMgrForKernel"
	.space	5
.L_imp_lib_5:
	.asciz	"InitForKernel"
	.space	2
.L_exp_tbl_0:
	.word	0xd632acdb
	.word	0xcee8593c
	.word	0xadf12745
	.word	0x2f064fa6
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_start /* module_start (0xd632acdb) */
	.word	module_reboot_before /* module_stop (0xcee8593c) */
	.word	module_reboot_phase /* module_reboot_phase (0xadf12745) */
	.word	module_reboot_before /* module_reboot_before (0x2f064fa6) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x0785c974
	.word	0x1ccb66d2
	.word	0x525b8218
	.word	0x9227ea79
	.word	0x9951c50f
	.word	0xe04add4c
	.word	0xef95a213
	.word	0xf5186d8e
.L_imp_nid_1:
	.word	0x181065ab
	.word	0x8ae776af
.L_imp_nid_2:
	.word	0x81d0d1f7
.L_imp_nid_3:
	.word	0x28b6489c
	.word	0x3f53e640
	.word	0x4e3a1105
	.word	0xd6da4ba1
.L_imp_nid_4:
	.word	0x22f15793
.L_imp_nid_5:
	.word	0x7233b5bc
	.word	0xa18a4a8b

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060002
