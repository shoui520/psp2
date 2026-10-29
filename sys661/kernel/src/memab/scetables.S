/* Generated PSP module import/export tables for sceMemab */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelDeleteSema
	.type	sceKernelDeleteSema, @function
sceKernelDeleteSema: /* ThreadManForKernel 0x28b6489c @ 0x2bf4 */
	jr	$ra
	nop
	.globl	sceKernelSignalSema
	.type	sceKernelSignalSema, @function
sceKernelSignalSema: /* ThreadManForKernel 0x3f53e640 @ 0x2bfc */
	jr	$ra
	nop
	.globl	sceKernelWaitSema
	.type	sceKernelWaitSema, @function
sceKernelWaitSema: /* ThreadManForKernel 0x4e3a1105 @ 0x2c04 */
	jr	$ra
	nop
	.globl	sceKernelCreateSema
	.type	sceKernelCreateSema, @function
sceKernelCreateSema: /* ThreadManForKernel 0xd6da4ba1 @ 0x2c0c */
	jr	$ra
	nop
	.globl	sceIdStorageLookup
	.type	sceIdStorageLookup, @function
sceIdStorageLookup: /* sceIdStorage_driver 0x6fe062d1 @ 0x2c14 */
	jr	$ra
	nop
	.globl	sceUtilsBufferCopyWithRange
	.type	sceUtilsBufferCopyWithRange, @function
sceUtilsBufferCopyWithRange: /* semaphore 0x4c537c72 @ 0x2c1c */
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
	.half	14
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
	.half	1
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
	.half	0x1006
	.byte	3, 1
	.asciz	"sceMemab"
	.space	19
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
	.asciz	"sceMemab_driver"
	.align	2
.L_exp_tbl_1:
	.word	0x16594684
	.word	0x23e4659b
	.word	0x3c15bc8c
	.word	0x4b54eaad
	.word	0x6dd7339a
	.word	0x8abe3445
	.word	0x9bf0c95d
	.word	0x9bf1a0a4
	.word	0x9de8c8cd
	.word	0xc3981ee1
	.word	0xcb5d3916
	.word	0xd47a50b1
	.word	0xd57856a7
	.word	0xf742f283
	.word	sceMemab_driver_16594684 /* sceMemab_driver_16594684 (0x16594684) */
	.word	sceMemab_driver_23E4659B /* sceMemab_driver_23E4659B (0x23e4659b) */
	.word	sceMemab_driver_3C15BC8C /* sceMemab_driver_3C15BC8C (0x3c15bc8c) */
	.word	sceMemab_driver_4B54EAAD /* sceMemab_driver_4B54EAAD (0x4b54eaad) */
	.word	sceMemab_driver_6DD7339A /* sceMemab_driver_6DD7339A (0x6dd7339a) */
	.word	sceMemab_driver_8ABE3445 /* sceMemab_driver_8ABE3445 (0x8abe3445) */
	.word	sceMemab_driver_9BF0C95D /* sceMemab_driver_9BF0C95D (0x9bf0c95d) */
	.word	sceMemab_driver_9BF1A0A4 /* sceMemab_driver_9BF1A0A4 (0x9bf1a0a4) */
	.word	sceMemab_driver_9DE8C8CD /* sceMemab_driver_9DE8C8CD (0x9de8c8cd) */
	.word	sceMemab_driver_C3981EE1 /* sceMemab_driver_C3981EE1 (0xc3981ee1) */
	.word	sceMemab_driver_CB5D3916 /* sceMemab_driver_CB5D3916 (0xcb5d3916) */
	.word	sceMemab_driver_D47A50B1 /* sceMemab_driver_D47A50B1 (0xd47a50b1) */
	.word	sceMemab_driver_D57856A7 /* sceMemab_driver_D57856A7 (0xd57856a7) */
	.word	sceMemab_driver_F742F283 /* sceMemab_driver_F742F283 (0xf742f283) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x28b6489c
	.word	0x3f53e640
	.word	0x4e3a1105
	.word	0xd6da4ba1
.L_imp_nid_1:
	.word	0x6fe062d1
.L_imp_nid_2:
	.word	0x4c537c72

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060002
