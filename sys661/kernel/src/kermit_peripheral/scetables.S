/* Generated PSP module import/export tables for sceKermitPeripheral_Driver */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	LoadExecForKernel_CAFC9CB3
	.type	LoadExecForKernel_CAFC9CB3, @function
LoadExecForKernel_CAFC9CB3: /* LoadExecForKernel 0xcafc9cb3 @ 0x0a04 */
	jr	$ra
	nop
	.globl	sceKernelMemcpy
	.type	sceKernelMemcpy, @function
sceKernelMemcpy: /* SysMemForKernel 0x181065ab @ 0x0a0c */
	jr	$ra
	nop
	.globl	sceKernelMemset
	.type	sceKernelMemset, @function
sceKernelMemset: /* SysMemForKernel 0x8ae776af @ 0x0a14 */
	jr	$ra
	nop
	.globl	sceKernelUnlockMutex
	.type	sceKernelUnlockMutex, @function
sceKernelUnlockMutex: /* ThreadManForKernel 0x6b30100f @ 0x0a1c */
	jr	$ra
	nop
	.globl	sceKernelLockMutex
	.type	sceKernelLockMutex, @function
sceKernelLockMutex: /* ThreadManForKernel 0xb011b11f @ 0x0a24 */
	jr	$ra
	nop
	.globl	sceKernelCreateMutex
	.type	sceKernelCreateMutex, @function
sceKernelCreateMutex: /* ThreadManForKernel 0xb7d098c6 @ 0x0a2c */
	jr	$ra
	nop
	.globl	sceKernelDcacheInvalidateRange
	.type	sceKernelDcacheInvalidateRange, @function
sceKernelDcacheInvalidateRange: /* UtilsForKernel 0xbfa98062 @ 0x0a34 */
	jr	$ra
	nop
	.globl	sceKermitMemory_driver_80E1240A
	.type	sceKermitMemory_driver_80E1240A, @function
sceKermitMemory_driver_80E1240A: /* sceKermitMemory_driver 0x80e1240a @ 0x0a3c */
	jr	$ra
	nop
	.globl	sceKermitMemory_driver_AAF047AC
	.type	sceKermitMemory_driver_AAF047AC, @function
sceKermitMemory_driver_AAF047AC: /* sceKermitMemory_driver 0xaaf047ac @ 0x0a44 */
	jr	$ra
	nop
	.globl	sceKermit_driver_36666181
	.type	sceKermit_driver_36666181, @function
sceKermit_driver_36666181: /* sceKermit_driver 0x36666181 @ 0x0a4c */
	jr	$ra
	nop
	.globl	sceKernelRegisterSysEventHandler
	.type	sceKernelRegisterSysEventHandler, @function
sceKernelRegisterSysEventHandler: /* sceSysEventForKernel 0xcd9e4bb5 @ 0x0a54 */
	jr	$ra
	nop
	.globl	sceKernelUnregisterSysEventHandler
	.type	sceKernelUnregisterSysEventHandler, @function
sceKernelUnregisterSysEventHandler: /* sceSysEventForKernel 0xd7d3fdcd @ 0x0a5c */
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
	.byte	5, 0
	.half	18
	.word	.L_exp_tbl_1
	.word	0x00020000
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x4001
	.byte	4, 0
	.half	3
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
	.half	2
	.word	.L_imp_nid_0
	.word	sceKernelRegisterSysEventHandler
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	3
	.word	.L_imp_nid_1
	.word	sceKernelUnlockMutex
	.word	0
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_2
	.word	sceKernelMemcpy
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_3
	.word	sceKernelDcacheInvalidateRange
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0009
	.byte	6, 0
	.half	1
	.word	.L_imp_nid_4
	.word	LoadExecForKernel_CAFC9CB3
	.word	0
	.word	.L_imp_lib_5
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_5
	.word	sceKermit_driver_36666181
	.word	.L_imp_lib_6
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_6
	.word	sceKermitMemory_driver_80E1240A

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
	.byte	0, 1
	.asciz	"sceKermitPeripheral_Driver"
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
	.asciz	"sceSysEventForKernel"
	.space	7
.L_imp_lib_1:
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_2:
	.asciz	"SysMemForKernel"
	.space	4
.L_imp_lib_3:
	.asciz	"UtilsForKernel"
	.space	5
.L_imp_lib_4:
	.asciz	"LoadExecForKernel"
	.space	6
.L_imp_lib_5:
	.asciz	"sceKermit_driver"
	.space	7
.L_imp_lib_6:
	.asciz	"sceKermitMemory_driver"
	.space	1
.L_exp_tbl_0:
	.word	0xd632acdb
	.word	0x2f064fa6
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_start /* module_start (0xd632acdb) */
	.word	module_reboot_before /* module_reboot_before (0x2f064fa6) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"sceKermitPeripheral_driver"
	.space	1
.L_exp_tbl_1:
	.word	0x0648e1a3
	.word	0x15f5c107
	.word	0x1617e966
	.word	0x20638b97
	.word	0x4a26b7c8
	.word	0x55d690dd
	.word	0x61bea1d2
	.word	0x65d98a21
	.word	0x675db2c1
	.word	0x6d6aa215
	.word	0x76be2e28
	.word	0x8c7903e7
	.word	0x9ef7501d
	.word	0xab29c68b
	.word	0xb778db22
	.word	0xb93a88e2
	.word	0xc6f8b4e1
	.word	0xd0a69002
	.word	sceKermitPeripheral_driver_0648E1A3 /* sceKermitPeripheral_driver_0648E1A3 (0x0648e1a3) */
	.word	sceKermitPeripheral_driver_15F5C107 /* sceKermitPeripheral_driver_15F5C107 (0x15f5c107) */
	.word	sceKermitPeripheral_driver_1617E966 /* sceKermitPeripheral_driver_1617E966 (0x1617e966) */
	.word	sceKermitPeripheral_driver_20638B97 /* sceKermitPeripheral_driver_20638B97 (0x20638b97) */
	.word	sceKermitPeripheral_driver_4A26B7C8 /* sceKermitPeripheral_driver_4A26B7C8 (0x4a26b7c8) */
	.word	sceKermitPeripheral_driver_55D690DD /* sceKermitPeripheral_driver_55D690DD (0x55d690dd) */
	.word	sceKermitPeripheral_driver_61BEA1D2 /* sceKermitPeripheral_driver_61BEA1D2 (0x61bea1d2) */
	.word	sceKermitPeripheral_driver_65D98A21 /* sceKermitPeripheral_driver_65D98A21 (0x65d98a21) */
	.word	sceKermitPeripheral_driver_675DB2C1 /* sceKermitPeripheral_driver_675DB2C1 (0x675db2c1) */
	.word	sceKermitPeripheral_driver_6D6AA215 /* sceKermitPeripheral_driver_6D6AA215 (0x6d6aa215) */
	.word	sceKermitPeripheral_driver_76BE2E28 /* sceKermitPeripheral_driver_76BE2E28 (0x76be2e28) */
	.word	sceKermitPeripheral_driver_8C7903E7 /* sceKermitPeripheral_driver_8C7903E7 (0x8c7903e7) */
	.word	sceKermitPeripheral_driver_9EF7501D /* sceKermitPeripheral_driver_9EF7501D (0x9ef7501d) */
	.word	sceKermitPeripheral_driver_AB29C68B /* sceKermitPeripheral_driver_AB29C68B (0xab29c68b) */
	.word	sceKermitPeripheral_driver_B778DB22 /* sceKermitPeripheral_driver_B778DB22 (0xb778db22) */
	.word	sceKermitPeripheral_driver_B93A88E2 /* sceKermitPeripheral_driver_B93A88E2 (0xb93a88e2) */
	.word	sceKermitPeripheral_driver_C6F8B4E1 /* sceKermitPeripheral_driver_C6F8B4E1 (0xc6f8b4e1) */
	.word	sceKermitPeripheral_driver_D0A69002 /* sceKermitPeripheral_driver_D0A69002 (0xd0a69002) */
	.word	0x00010000
	.word	0x00040003
	.word	0x00050004
	.word	0x000a0006
	.word	0x000c000b
	.word	0x000e000d
	.word	0x00110010
	.word	0x00120012
.L_exp_lib_2:
	.asciz	"sceKermitPeripheral"
	.align	2
.L_exp_tbl_2:
	.word	0x4a26b7c8
	.word	0xc0ebc631
	.word	0xd27c5e03
	.word	sceKermitPeripheral_driver_4A26B7C8 /* sceKermitPeripheral_4A26B7C8 (0x4a26b7c8) */
	.word	sceKermitPeripheral_C0EBC631 /* sceKermitPeripheral_C0EBC631 (0xc0ebc631) */
	.word	sceKermitPeripheral_D27C5E03 /* sceKermitPeripheral_D27C5E03 (0xd27c5e03) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0xcd9e4bb5
	.word	0xd7d3fdcd
.L_imp_nid_1:
	.word	0x6b30100f
	.word	0xb011b11f
	.word	0xb7d098c6
.L_imp_nid_2:
	.word	0x181065ab
	.word	0x8ae776af
.L_imp_nid_3:
	.word	0xbfa98062
.L_imp_nid_4:
	.word	0xcafc9cb3
.L_imp_nid_5:
	.word	0x36666181
.L_imp_nid_6:
	.word	0x80e1240a
	.word	0xaaf047ac

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
