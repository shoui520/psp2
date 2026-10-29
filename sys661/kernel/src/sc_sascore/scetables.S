/* Generated PSP module import/export tables for sceSAScore */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelGetCompiledSdkVersion
	.type	sceKernelGetCompiledSdkVersion, @function
sceKernelGetCompiledSdkVersion: /* SysMemForKernel 0xb4f00cb5 @ 0x2aa0 */
	jr	$ra
	nop
	.globl	sceKernelDeleteSema
	.type	sceKernelDeleteSema, @function
sceKernelDeleteSema: /* ThreadManForKernel 0x28b6489c @ 0x2aa8 */
	jr	$ra
	nop
	.globl	sceKernelSignalSema
	.type	sceKernelSignalSema, @function
sceKernelSignalSema: /* ThreadManForKernel 0x3f53e640 @ 0x2ab0 */
	jr	$ra
	nop
	.globl	sceKernelWaitSema
	.type	sceKernelWaitSema, @function
sceKernelWaitSema: /* ThreadManForKernel 0x4e3a1105 @ 0x2ab8 */
	jr	$ra
	nop
	.globl	sceKernelCreateSema
	.type	sceKernelCreateSema, @function
sceKernelCreateSema: /* ThreadManForKernel 0xd6da4ba1 @ 0x2ac0 */
	jr	$ra
	nop
	.globl	sceKernelDcacheWritebackInvalidateRange
	.type	sceKernelDcacheWritebackInvalidateRange, @function
sceKernelDcacheWritebackInvalidateRange: /* UtilsForKernel 0x34b9fa9e @ 0x2ac8 */
	jr	$ra
	nop
	.globl	sceKernelDcacheWritebackRange
	.type	sceKernelDcacheWritebackRange, @function
sceKernelDcacheWritebackRange: /* UtilsForKernel 0x3ee30821 @ 0x2ad0 */
	jr	$ra
	nop
	.globl	sceKernelDcacheWritebackInvalidateAll
	.type	sceKernelDcacheWritebackInvalidateAll, @function
sceKernelDcacheWritebackInvalidateAll: /* UtilsForKernel 0xb435dec5 @ 0x2ad8 */
	jr	$ra
	nop
	.globl	sceKernelDcacheInvalidateRangeForUser
	.type	sceKernelDcacheInvalidateRangeForUser, @function
sceKernelDcacheInvalidateRangeForUser: /* UtilsForKernel 0xbfa98062 @ 0x2ae0 */
	jr	$ra
	nop
	.globl	sceMeCore_driver_635397BB
	.type	sceMeCore_driver_635397BB, @function
sceMeCore_driver_635397BB: /* sceMeCore_driver 0x635397bb @ 0x2ae8 */
	jr	$ra
	nop
	.globl	sceMeCore_driver_FA398D71
	.type	sceMeCore_driver_FA398D71, @function
sceMeCore_driver_FA398D71: /* sceMeCore_driver 0xfa398d71 @ 0x2af0 */
	jr	$ra
	nop
	.globl	sceMeFree
	.type	sceMeFree, @function
sceMeFree: /* sceMeMemory_driver 0x6ed69327 @ 0x2af8 */
	jr	$ra
	nop
	.globl	sceMeCalloc
	.type	sceMeCalloc, @function
sceMeCalloc: /* sceMeMemory_driver 0xc4eda9f4 @ 0x2b00 */
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
	.half	2
	.word	.L_exp_tbl_1
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x4001
	.byte	5, 0
	.half	32
	.word	.L_exp_tbl_2
	.word	0x00020000
	.word	.L_exp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	4, 0
	.half	2
	.word	.L_exp_tbl_3
	.word	.L_exp_lib_4
	.byte	17, 0
	.half	0x4001
	.byte	4, 0
	.half	3
	.word	.L_exp_tbl_4

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
	.half	2
	.word	.L_imp_nid_0
	.word	sceMeCore_driver_635397BB
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_1
	.word	sceMeFree
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	4
	.word	.L_imp_nid_2
	.word	sceKernelDcacheWritebackInvalidateRange
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
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_4
	.word	sceKernelGetCompiledSdkVersion

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
	.byte	10, 1
	.asciz	"sceSAScore"
	.space	17
	.word	_gp
	.word	__lib_ent_top + 4
	.word	__lib_ent_btm
	.word	__lib_stub_top + 4
	.word	__lib_stub_btm

	.section .rodata.sceResident, "a", @progbits
	.align	2
	.word	0
.L_imp_lib_0:
	.asciz	"sceMeCore_driver"
	.space	7
.L_imp_lib_1:
	.asciz	"sceMeMemory_driver"
	.space	5
.L_imp_lib_2:
	.asciz	"UtilsForKernel"
	.space	5
.L_imp_lib_3:
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_4:
	.asciz	"SysMemForKernel"
	.align	2
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
	.asciz	"sceSasCore_driver"
	.space	2
.L_exp_tbl_1:
	.word	0xb0f9f98f
	.word	0xe143a1ea
	.word	module_start /* sceSasCore_driver_B0F9F98F (0xb0f9f98f) */
	.word	module_stop /* sceSasCore_driver_E143A1EA (0xe143a1ea) */
.L_exp_lib_2:
	.asciz	"sceSasCore"
	.space	1
.L_exp_tbl_2:
	.word	0x019b25eb
	.word	0x07f58c24
	.word	0x267a6dd2
	.word	0x2c8e6ab3
	.word	0x33d4ab37
	.word	0x42778a9f
	.word	0x440ca7d8
	.word	0x4aa9ead6
	.word	0x50a14dfc
	.word	0x5f9529f6
	.word	0x68a46b95
	.word	0x7497ea85
	.word	0x74ae582a
	.word	0x76f01aca
	.word	0x787d04d5
	.word	0x99944089
	.word	0x9ec3676a
	.word	0xa0cf2fa4
	.word	0xa232cbe6
	.word	0xa3589d81
	.word	0xad84d37f
	.word	0xb7660a23
	.word	0xbd11b7c2
	.word	0xcbcd4f79
	.word	0xd1e0a01e
	.word	0xd5a229c9
	.word	0xd5ebbbcd
	.word	0xe175ef66
	.word	0xe1cd9561
	.word	0xe855bf76
	.word	0xf6107f00
	.word	0xf983b186
	.word	sceSasCore_019B25EB /* sceSasCore_019B25EB (0x019b25eb) */
	.word	sceSasCore_07F58C24 /* sceSasCore_07F58C24 (0x07f58c24) */
	.word	sceSasCore_267A6DD2 /* sceSasCore_267A6DD2 (0x267a6dd2) */
	.word	sceSasCore_2C8E6AB3 /* sceSasCore_2C8E6AB3 (0x2c8e6ab3) */
	.word	sceSasCore_33D4AB37 /* sceSasCore_33D4AB37 (0x33d4ab37) */
	.word	sceSasCore_42778A9F /* sceSasCore_42778A9F (0x42778a9f) */
	.word	sceSasCore_440CA7D8 /* sceSasCore_440CA7D8 (0x440ca7d8) */
	.word	sceSasCore_4AA9EAD6 /* sceSasCore_4AA9EAD6 (0x4aa9ead6) */
	.word	sceSasCore_50A14DFC /* sceSasCore_50A14DFC (0x50a14dfc) */
	.word	sceSasCore_5F9529F6 /* sceSasCore_5F9529F6 (0x5f9529f6) */
	.word	sceSasCore_68A46B95 /* sceSasCore_68A46B95 (0x68a46b95) */
	.word	sceSasCore_7497EA85 /* sceSasCore_7497EA85 (0x7497ea85) */
	.word	sceSasCore_74AE582A /* sceSasCore_74AE582A (0x74ae582a) */
	.word	sceSasCore_76F01ACA /* sceSasCore_76F01ACA (0x76f01aca) */
	.word	sceSasCore_787D04D5 /* sceSasCore_787D04D5 (0x787d04d5) */
	.word	sceSasCore_99944089 /* sceSasCore_99944089 (0x99944089) */
	.word	sceSasCore_9EC3676A /* sceSasCore_9EC3676A (0x9ec3676a) */
	.word	sceSasCore_A0CF2FA4 /* sceSasCore_A0CF2FA4 (0xa0cf2fa4) */
	.word	sceSasCore_A232CBE6 /* sceSasCore_A232CBE6 (0xa232cbe6) */
	.word	sceSasCore_A3589D81 /* sceSasCore_A3589D81 (0xa3589d81) */
	.word	sceSasCore_AD84D37F /* sceSasCore_AD84D37F (0xad84d37f) */
	.word	sceSasCore_B7660A23 /* sceSasCore_B7660A23 (0xb7660a23) */
	.word	sceSasCore_BD11B7C2 /* sceSasCore_BD11B7C2 (0xbd11b7c2) */
	.word	sceSasCore_CBCD4F79 /* sceSasCore_CBCD4F79 (0xcbcd4f79) */
	.word	sceSasCore_D1E0A01E /* sceSasCore_D1E0A01E (0xd1e0a01e) */
	.word	sceSasCore_D5A229C9 /* sceSasCore_D5A229C9 (0xd5a229c9) */
	.word	sceSasCore_D5EBBBCD /* sceSasCore_D5EBBBCD (0xd5ebbbcd) */
	.word	sceSasCore_E175EF66 /* sceSasCore_E175EF66 (0xe175ef66) */
	.word	sceSasCore_E1CD9561 /* sceSasCore_E1CD9561 (0xe1cd9561) */
	.word	sceSasCore_E855BF76 /* sceSasCore_E855BF76 (0xe855bf76) */
	.word	sceSasCore_F6107F00 /* sceSasCore_F6107F00 (0xf6107f00) */
	.word	sceSasCore_F983B186 /* sceSasCore_F983B186 (0xf983b186) */
	.word	0x00020000
	.word	0x00040002
	.word	0x00080005
	.word	0x000b000a
	.word	0x000f000f
	.word	0x00150011
	.word	0x00180017
	.word	0x001e001b
.L_exp_lib_3:
	.asciz	"sceP3da_driver"
	.space	1
.L_exp_tbl_3:
	.word	0x374500a5
	.word	0x43f756a2
	.word	sceP3da_driver_374500A5 /* sceP3da_driver_374500A5 (0x374500a5) */
	.word	sceP3da_driver_43F756A2 /* sceP3da_driver_43F756A2 (0x43f756a2) */
.L_exp_lib_4:
	.asciz	"sceP3da"
	.align	2
.L_exp_tbl_4:
	.word	0x013016f3
	.word	0x374500a5
	.word	0x43f756a2
	.word	sceP3da_013016F3 /* sceP3da_013016F3 (0x013016f3) */
	.word	sceP3da_driver_374500A5 /* sceP3da_374500A5 (0x374500a5) */
	.word	sceP3da_driver_43F756A2 /* sceP3da_43F756A2 (0x43f756a2) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x635397bb
	.word	0xfa398d71
.L_imp_nid_1:
	.word	0x6ed69327
	.word	0xc4eda9f4
.L_imp_nid_2:
	.word	0x34b9fa9e
	.word	0x3ee30821
	.word	0xb435dec5
	.word	0xbfa98062
.L_imp_nid_3:
	.word	0x28b6489c
	.word	0x3f53e640
	.word	0x4e3a1105
	.word	0xd6da4ba1
.L_imp_nid_4:
	.word	0xb4f00cb5

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
