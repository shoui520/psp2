/* Generated PSP module import/export tables for sceKermitFlashfs_driver */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelCpuSuspendIntr
	.type	sceKernelCpuSuspendIntr, @function
sceKernelCpuSuspendIntr: /* InterruptManagerForKernel 0x092968f4 @ 0x1d80 */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntr
	.type	sceKernelCpuResumeIntr, @function
sceKernelCpuResumeIntr: /* InterruptManagerForKernel 0x5f10d406 @ 0x1d88 */
	jr	$ra
	nop
	.globl	sceIoGetIobUserLevel
	.type	sceIoGetIobUserLevel, @function
sceIoGetIobUserLevel: /* IoFileMgrForKernel 0x49356c12 @ 0x1d90 */
	jr	$ra
	nop
	.globl	sceIoAddDrv
	.type	sceIoAddDrv, @function
sceIoAddDrv: /* IoFileMgrForKernel 0x8e982a74 @ 0x1d98 */
	jr	$ra
	nop
	.globl	sceIoDelDrv
	.type	sceIoDelDrv, @function
sceIoDelDrv: /* IoFileMgrForKernel 0xc7f35804 @ 0x1da0 */
	jr	$ra
	nop
	.globl	sceKernelMemcpy
	.type	sceKernelMemcpy, @function
sceKernelMemcpy: /* SysMemForKernel 0x181065ab @ 0x1da8 */
	jr	$ra
	nop
	.globl	sceKernelMemset
	.type	sceKernelMemset, @function
sceKernelMemset: /* SysMemForKernel 0x8ae776af @ 0x1db0 */
	jr	$ra
	nop
	.globl	strlen
	.type	strlen, @function
strlen: /* SysclibForKernel 0x52df196c @ 0x1db8 */
	jr	$ra
	nop
	.globl	strncmp
	.type	strncmp, @function
strncmp: /* SysclibForKernel 0x7ab35214 @ 0x1dc0 */
	jr	$ra
	nop
	.globl	memcmp
	.type	memcmp, @function
memcmp: /* SysclibForKernel 0x81d0d1f7 @ 0x1dc8 */
	jr	$ra
	nop
	.globl	snprintf
	.type	snprintf, @function
snprintf: /* SysclibForKernel 0xc2145e80 @ 0x1dd0 */
	jr	$ra
	nop
	.globl	sceKernelUnlockMutex
	.type	sceKernelUnlockMutex, @function
sceKernelUnlockMutex: /* ThreadManForKernel 0x6b30100f @ 0x1dd8 */
	jr	$ra
	nop
	.globl	sceKernelLockMutex
	.type	sceKernelLockMutex, @function
sceKernelLockMutex: /* ThreadManForKernel 0xb011b11f @ 0x1de0 */
	jr	$ra
	nop
	.globl	sceKernelCreateMutex
	.type	sceKernelCreateMutex, @function
sceKernelCreateMutex: /* ThreadManForKernel 0xb7d098c6 @ 0x1de8 */
	jr	$ra
	nop
	.globl	sceKernelDeleteMutex
	.type	sceKernelDeleteMutex, @function
sceKernelDeleteMutex: /* ThreadManForKernel 0xf8170fbe @ 0x1df0 */
	jr	$ra
	nop
	.globl	sceKernelDcacheInvalidateRange
	.type	sceKernelDcacheInvalidateRange, @function
sceKernelDcacheInvalidateRange: /* UtilsForKernel 0xbfa98062 @ 0x1df8 */
	jr	$ra
	nop
	.globl	sceKermitMemory_driver_90B662D0
	.type	sceKermitMemory_driver_90B662D0, @function
sceKermitMemory_driver_90B662D0: /* sceKermitMemory_driver 0x90b662d0 @ 0x1e00 */
	jr	$ra
	nop
	.globl	sceKermitMemory_driver_AAF047AC
	.type	sceKermitMemory_driver_AAF047AC, @function
sceKermitMemory_driver_AAF047AC: /* sceKermitMemory_driver 0xaaf047ac @ 0x1e08 */
	jr	$ra
	nop
	.globl	sceKermit_driver_36666181
	.type	sceKermit_driver_36666181, @function
sceKermit_driver_36666181: /* sceKermit_driver 0x36666181 @ 0x1e10 */
	jr	$ra
	nop
	.globl	sceKernelRegisterSysEventHandler
	.type	sceKernelRegisterSysEventHandler, @function
sceKernelRegisterSysEventHandler: /* sceSysEventForKernel 0xcd9e4bb5 @ 0x1e18 */
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
	.half	1
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
	.word	sceKernelRegisterSysEventHandler
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0009
	.byte	6, 0
	.half	2
	.word	.L_imp_nid_1
	.word	sceKernelCpuSuspendIntr
	.word	0
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	4
	.word	.L_imp_nid_2
	.word	sceKernelUnlockMutex
	.word	0
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	3
	.word	.L_imp_nid_3
	.word	sceIoGetIobUserLevel
	.word	0
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_4
	.word	sceKernelMemcpy
	.word	.L_imp_lib_5
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	4
	.word	.L_imp_nid_5
	.word	strlen
	.word	.L_imp_lib_6
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_6
	.word	sceKernelDcacheInvalidateRange
	.word	.L_imp_lib_7
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_7
	.word	sceKermit_driver_36666181
	.word	.L_imp_lib_8
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_8
	.word	sceKermitMemory_driver_90B662D0

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
	.asciz	"sceKermitFlashfs_driver"
	.space	4
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
	.asciz	"InterruptManagerForKernel"
	.space	6
.L_imp_lib_2:
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_3:
	.asciz	"IoFileMgrForKernel"
	.space	5
.L_imp_lib_4:
	.asciz	"SysMemForKernel"
	.space	4
.L_imp_lib_5:
	.asciz	"SysclibForKernel"
	.space	7
.L_imp_lib_6:
	.asciz	"UtilsForKernel"
	.space	5
.L_imp_lib_7:
	.asciz	"sceKermit_driver"
	.space	7
.L_imp_lib_8:
	.asciz	"sceKermitMemory_driver"
	.space	1
.L_exp_tbl_0:
	.word	0xd3744be0
	.word	0xd632acdb
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_start /* module_bootstart (0xd3744be0) */
	.word	module_start /* module_start (0xd632acdb) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"sceKermitFlashfs_driver"
	.align	2
.L_exp_tbl_1:
	.word	0x78d76b63
	.word	module_start /* sceKermitFlashfs_driver_78D76B63 (0x78d76b63) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0xcd9e4bb5
.L_imp_nid_1:
	.word	0x092968f4
	.word	0x5f10d406
.L_imp_nid_2:
	.word	0x6b30100f
	.word	0xb011b11f
	.word	0xb7d098c6
	.word	0xf8170fbe
.L_imp_nid_3:
	.word	0x49356c12
	.word	0x8e982a74
	.word	0xc7f35804
.L_imp_nid_4:
	.word	0x181065ab
	.word	0x8ae776af
.L_imp_nid_5:
	.word	0x52df196c
	.word	0x7ab35214
	.word	0x81d0d1f7
	.word	0xc2145e80
.L_imp_nid_6:
	.word	0xbfa98062
.L_imp_nid_7:
	.word	0x36666181
.L_imp_nid_8:
	.word	0x90b662d0
	.word	0xaaf047ac

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
