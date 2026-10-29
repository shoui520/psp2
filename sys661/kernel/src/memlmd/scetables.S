/* Generated PSP module import/export tables for sceMemlmd */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelCpuSuspendIntr
	.type	sceKernelCpuSuspendIntr, @function
sceKernelCpuSuspendIntr: /* InterruptManagerForKernel 0x092968f4 @ 0x1fa0 */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntrWithSync
	.type	sceKernelCpuResumeIntrWithSync, @function
sceKernelCpuResumeIntrWithSync: /* InterruptManagerForKernel 0x3b84732d @ 0x1fa8 */
	jr	$ra
	nop
	.globl	sceKernelEnableIntr
	.type	sceKernelEnableIntr, @function
sceKernelEnableIntr: /* InterruptManagerForKernel 0x4d6e7305 @ 0x1fb0 */
	jr	$ra
	nop
	.globl	sceKernelRegisterIntrHandler
	.type	sceKernelRegisterIntrHandler, @function
sceKernelRegisterIntrHandler: /* InterruptManagerForKernel 0x58dd8978 @ 0x1fb8 */
	jr	$ra
	nop
	.globl	sceKernelDisableIntr
	.type	sceKernelDisableIntr, @function
sceKernelDisableIntr: /* InterruptManagerForKernel 0xd774ba45 @ 0x1fc0 */
	jr	$ra
	nop
	.globl	sceKernelReleaseIntrHandler
	.type	sceKernelReleaseIntrHandler, @function
sceKernelReleaseIntrHandler: /* InterruptManagerForKernel 0xf987b1f0 @ 0x1fc8 */
	jr	$ra
	nop
	.globl	sceKernelSetEventFlag
	.type	sceKernelSetEventFlag, @function
sceKernelSetEventFlag: /* ThreadManForKernel 0x1fb15a32 @ 0x1fd0 */
	jr	$ra
	nop
	.globl	sceKernelWaitEventFlag
	.type	sceKernelWaitEventFlag, @function
sceKernelWaitEventFlag: /* ThreadManForKernel 0x402fcf22 @ 0x1fd8 */
	jr	$ra
	nop
	.globl	sceKernelCreateEventFlag
	.type	sceKernelCreateEventFlag, @function
sceKernelCreateEventFlag: /* ThreadManForKernel 0x55c20a00 @ 0x1fe0 */
	jr	$ra
	nop
	.globl	sceKernelUnlockMutex
	.type	sceKernelUnlockMutex, @function
sceKernelUnlockMutex: /* ThreadManForKernel 0x6b30100f @ 0x1fe8 */
	jr	$ra
	nop
	.globl	sceKernelLockMutex
	.type	sceKernelLockMutex, @function
sceKernelLockMutex: /* ThreadManForKernel 0xb011b11f @ 0x1ff0 */
	jr	$ra
	nop
	.globl	sceKernelCreateMutex
	.type	sceKernelCreateMutex, @function
sceKernelCreateMutex: /* ThreadManForKernel 0xb7d098c6 @ 0x1ff8 */
	jr	$ra
	nop
	.globl	sceKernelDeleteEventFlag
	.type	sceKernelDeleteEventFlag, @function
sceKernelDeleteEventFlag: /* ThreadManForKernel 0xef9e4c70 @ 0x2000 */
	jr	$ra
	nop
	.globl	sceKernelDeleteMutex
	.type	sceKernelDeleteMutex, @function
sceKernelDeleteMutex: /* ThreadManForKernel 0xf8170fbe @ 0x2008 */
	jr	$ra
	nop
	.globl	sceKernelDcacheWritebackInvalidateRange
	.type	sceKernelDcacheWritebackInvalidateRange, @function
sceKernelDcacheWritebackInvalidateRange: /* UtilsForKernel 0x34b9fa9e @ 0x2010 */
	jr	$ra
	nop
	.globl	UtilsForKernel_66463BEC
	.type	UtilsForKernel_66463BEC, @function
UtilsForKernel_66463BEC: /* UtilsForKernel 0x66463bec @ 0x2018 */
	jr	$ra
	nop
	.globl	sceKernelDcacheWritebackInvalidateAll
	.type	sceKernelDcacheWritebackInvalidateAll, @function
sceKernelDcacheWritebackInvalidateAll: /* UtilsForKernel 0xb435dec5 @ 0x2020 */
	jr	$ra
	nop
	.globl	sceKernelPowerUnlockForUser
	.type	sceKernelPowerUnlockForUser, @function
sceKernelPowerUnlockForUser: /* sceSuspendForKernel 0x3aee7261 @ 0x2028 */
	jr	$ra
	nop
	.globl	sceKernelPowerLockForUser
	.type	sceKernelPowerLockForUser, @function
sceKernelPowerLockForUser: /* sceSuspendForKernel 0xeadb1bd7 @ 0x2030 */
	jr	$ra
	nop
	.globl	sceKernelRegisterSysEventHandler
	.type	sceKernelRegisterSysEventHandler, @function
sceKernelRegisterSysEventHandler: /* sceSysEventForKernel 0xcd9e4bb5 @ 0x2038 */
	jr	$ra
	nop
	.globl	sceKernelUnregisterSysEventHandler
	.type	sceKernelUnregisterSysEventHandler, @function
sceKernelUnregisterSysEventHandler: /* sceSysEventForKernel 0xd7d3fdcd @ 0x2040 */
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
	.half	0x0009
	.byte	4, 0
	.half	9
	.word	.L_exp_tbl_1
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	4, 0
	.half	4
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
	.half	8
	.word	.L_imp_nid_1
	.word	sceKernelSetEventFlag
	.word	0
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0009
	.byte	6, 0
	.half	6
	.word	.L_imp_nid_2
	.word	sceKernelCpuSuspendIntr
	.word	0
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	3
	.word	.L_imp_nid_3
	.word	sceKernelDcacheWritebackInvalidateRange
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_4
	.word	sceKernelPowerUnlockForUser

	.section .lib.stub.btm, "a", @progbits
	.align	2
__lib_stub_btm:
	.word	0

	.section .rodata.sceModuleInfo, "a", @progbits
	.align	4
	.globl	module_info
	.type	module_info, @object
module_info:
	.half	0x7007
	.byte	37, 1
	.asciz	"sceMemlmd"
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
	.asciz	"sceSysEventForKernel"
	.space	7
.L_imp_lib_1:
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_2:
	.asciz	"InterruptManagerForKernel"
	.space	6
.L_imp_lib_3:
	.asciz	"UtilsForKernel"
	.space	5
.L_imp_lib_4:
	.asciz	"sceSuspendForKernel"
	.align	2
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
	.asciz	"memlmd"
	.space	1
.L_exp_tbl_1:
	.word	0x2ae425d2
	.word	0x2f3d7e2d
	.word	0x6192f715
	.word	0x76b7e315
	.word	0x9d36a439
	.word	0xcf03556b
	.word	0xea94592c
	.word	0xef73e85b
	.word	0xf26a33c3
	.word	memlmd_2AE425D2 /* memlmd_2AE425D2 (0x2ae425d2) */
	.word	memlmd_2F3D7E2D /* memlmd_2F3D7E2D (0x2f3d7e2d) */
	.word	memlmd_6192F715 /* memlmd_6192F715 (0x6192f715) */
	.word	memlmd_76B7E315 /* memlmd_76B7E315 (0x76b7e315) */
	.word	memlmd_9D36A439 /* memlmd_9D36A439 (0x9d36a439) */
	.word	memlmd_CF03556B /* memlmd_CF03556B (0xcf03556b) */
	.word	memlmd_EA94592C /* memlmd_EA94592C (0xea94592c) */
	.word	memlmd_EF73E85B /* memlmd_EF73E85B (0xef73e85b) */
	.word	memlmd_F26A33C3 /* memlmd_F26A33C3 (0xf26a33c3) */
.L_exp_lib_2:
	.asciz	"semaphore"
	.space	2
.L_exp_tbl_2:
	.word	0x00eec06a
	.word	0x4c537c72
	.word	0x77e97079
	.word	0x8eeb7bf2
	.word	sceUtilsBufferCopy /* sceUtilsBufferCopy (0x00eec06a) */
	.word	sceUtilsBufferCopyWithRange /* sceUtilsBufferCopyWithRange (0x4c537c72) */
	.word	sceUtilsBufferCopyByPollingWithRange /* sceUtilsBufferCopyByPollingWithRange (0x77e97079) */
	.word	sceUtilsBufferCopyByPolling /* sceUtilsBufferCopyByPolling (0x8eeb7bf2) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0xcd9e4bb5
	.word	0xd7d3fdcd
.L_imp_nid_1:
	.word	0x1fb15a32
	.word	0x402fcf22
	.word	0x55c20a00
	.word	0x6b30100f
	.word	0xb011b11f
	.word	0xb7d098c6
	.word	0xef9e4c70
	.word	0xf8170fbe
.L_imp_nid_2:
	.word	0x092968f4
	.word	0x3b84732d
	.word	0x4d6e7305
	.word	0x58dd8978
	.word	0xd774ba45
	.word	0xf987b1f0
.L_imp_nid_3:
	.word	0x34b9fa9e
	.word	0x66463bec
	.word	0xb435dec5
.L_imp_nid_4:
	.word	0x3aee7261
	.word	0xeadb1bd7

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060010
