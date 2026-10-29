/* Generated PSP module import/export tables for sceHttpStorage_Service */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceIoOpen
	.type	sceIoOpen, @function
sceIoOpen: /* IoFileMgrForKernel 0x109f50bc @ 0x0678 */
	jr	$ra
	nop
	.globl	sceIoLseek
	.type	sceIoLseek, @function
sceIoLseek: /* IoFileMgrForKernel 0x27eb27b8 @ 0x0680 */
	jr	$ra
	nop
	.globl	sceIoWrite
	.type	sceIoWrite, @function
sceIoWrite: /* IoFileMgrForKernel 0x42ec03ac @ 0x0688 */
	jr	$ra
	nop
	.globl	sceIoRead
	.type	sceIoRead, @function
sceIoRead: /* IoFileMgrForKernel 0x6a638d83 @ 0x0690 */
	jr	$ra
	nop
	.globl	sceIoClose
	.type	sceIoClose, @function
sceIoClose: /* IoFileMgrForKernel 0x810c4bc3 @ 0x0698 */
	jr	$ra
	nop
	.globl	sceIoGetstat
	.type	sceIoGetstat, @function
sceIoGetstat: /* IoFileMgrForKernel 0xace946e8 @ 0x06a0 */
	jr	$ra
	nop
	.globl	sceKernelUnloadModule
	.type	sceKernelUnloadModule, @function
sceKernelUnloadModule: /* ModuleMgrForKernel 0x387e3ca9 @ 0x06a8 */
	jr	$ra
	nop
	.globl	sceKernelStartModule
	.type	sceKernelStartModule, @function
sceKernelStartModule: /* ModuleMgrForKernel 0x3ff74df1 @ 0x06b0 */
	jr	$ra
	nop
	.globl	sceKernelLoadModuleForKernel
	.type	sceKernelLoadModuleForKernel, @function
sceKernelLoadModuleForKernel: /* ModuleMgrForKernel 0x939e4270 @ 0x06b8 */
	jr	$ra
	nop
	.globl	sceKernelSearchModuleByName
	.type	sceKernelSearchModuleByName, @function
sceKernelSearchModuleByName: /* ModuleMgrForKernel 0xd86dd11b @ 0x06c0 */
	jr	$ra
	nop
	.globl	sceKernelStopModule
	.type	sceKernelStopModule, @function
sceKernelStopModule: /* ModuleMgrForKernel 0xe5d6087b @ 0x06c8 */
	jr	$ra
	nop
	.globl	sceKernelGetGameInfo
	.type	sceKernelGetGameInfo, @function
sceKernelGetGameInfo: /* SysMemForKernel 0xef29061c @ 0x06d0 */
	jr	$ra
	nop
	.globl	strncpy
	.type	strncpy, @function
strncpy: /* SysclibForKernel 0xb49a7697 @ 0x06d8 */
	jr	$ra
	nop
	.globl	sceChkregGetPsCode
	.type	sceChkregGetPsCode, @function
sceChkregGetPsCode: /* sceChkreg_driver 0x59f8491d @ 0x06e0 */
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
	.half	7
	.word	.L_exp_tbl_1
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x4001
	.byte	4, 0
	.half	6
	.word	.L_exp_tbl_2
	.word	.L_exp_lib_3
	.byte	17, 0
	.half	0x4001
	.byte	4, 0
	.half	2
	.word	.L_exp_tbl_3

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
	.word	sceKernelGetGameInfo
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_1
	.word	strncpy
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	6
	.word	.L_imp_nid_2
	.word	sceIoOpen
	.word	0
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0009
	.byte	6, 0
	.half	5
	.word	.L_imp_nid_3
	.word	sceKernelUnloadModule
	.word	0
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_4
	.word	sceChkregGetPsCode

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
	.asciz	"sceHttpStorage_Service"
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
	.asciz	"SysMemForKernel"
	.space	4
.L_imp_lib_1:
	.asciz	"SysclibForKernel"
	.space	7
.L_imp_lib_2:
	.asciz	"IoFileMgrForKernel"
	.space	5
.L_imp_lib_3:
	.asciz	"ModuleMgrForKernel"
	.space	5
.L_imp_lib_4:
	.asciz	"sceChkreg_driver"
	.space	3
.L_exp_tbl_0:
	.word	0xcee8593c
	.word	0x2f064fa6
	.word	0xf01d73a7
	.word	0x11b97506
	.word	sceHttpStorage_driver_C59AC98A /* module_stop (0xcee8593c) */
	.word	module_reboot_before /* module_reboot_before (0x2f064fa6) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"sceHttpStorage_driver"
	.space	2
.L_exp_tbl_1:
	.word	0x24aa94f4
	.word	0x2d8dae58
	.word	0x700aad44
	.word	0xb33389ce
	.word	0xc59ac98a
	.word	0xcda3d8f6
	.word	0xcddf1103
	.word	sceHttpStorageWrite /* sceHttpStorageWrite (0x24aa94f4) */
	.word	sceHttpStorageGetstat /* sceHttpStorageGetstat (0x2d8dae58) */
	.word	sceHttpStorageOpen /* sceHttpStorageOpen (0x700aad44) */
	.word	sceHttpStorageLseek /* sceHttpStorageLseek (0xb33389ce) */
	.word	sceHttpStorage_driver_C59AC98A /* sceHttpStorage_driver_C59AC98A (0xc59ac98a) */
	.word	sceHttpStorageClose /* sceHttpStorageClose (0xcda3d8f6) */
	.word	sceHttpStorageRead /* sceHttpStorageRead (0xcddf1103) */
.L_exp_lib_2:
	.asciz	"sceHttpStorage"
	.space	1
.L_exp_tbl_2:
	.word	0x24aa94f4
	.word	0x2d8dae58
	.word	0x700aad44
	.word	0xb33389ce
	.word	0xcda3d8f6
	.word	0xcddf1103
	.word	sceHttpStorageWrite /* sceHttpStorageWrite (0x24aa94f4) */
	.word	sceHttpStorageGetstat /* sceHttpStorageGetstat (0x2d8dae58) */
	.word	sceHttpStorageOpen /* sceHttpStorageOpen (0x700aad44) */
	.word	sceHttpStorageLseek /* sceHttpStorageLseek (0xb33389ce) */
	.word	sceHttpStorageClose /* sceHttpStorageClose (0xcda3d8f6) */
	.word	sceHttpStorageRead /* sceHttpStorageRead (0xcddf1103) */
.L_exp_lib_3:
	.asciz	"sceHttpStorage_bridge"
	.space	2
.L_exp_tbl_3:
	.word	0x04ef00f8
	.word	0xefe06a20
	.word	sceHttpStorage_bridge_04EF00F8 /* sceHttpStorage_bridge_04EF00F8 (0x04ef00f8) */
	.word	sceHttpStorage_bridge_EFE06A20 /* sceHttpStorage_bridge_EFE06A20 (0xefe06a20) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0xef29061c
.L_imp_nid_1:
	.word	0xb49a7697
.L_imp_nid_2:
	.word	0x109f50bc
	.word	0x27eb27b8
	.word	0x42ec03ac
	.word	0x6a638d83
	.word	0x810c4bc3
	.word	0xace946e8
.L_imp_nid_3:
	.word	0x387e3ca9
	.word	0x3ff74df1
	.word	0x939e4270
	.word	0xd86dd11b
	.word	0xe5d6087b
.L_imp_nid_4:
	.word	0x59f8491d

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
