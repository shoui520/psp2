/* Generated PSP module import/export tables for sceMsEmu_Driver */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceIoAddDrv
	.type	sceIoAddDrv, @function
sceIoAddDrv: /* IoFileMgrForKernel 0x8e982a74 @ 0x0ef8 */
	jr	$ra
	nop
	.globl	sceIoDelDrv
	.type	sceIoDelDrv, @function
sceIoDelDrv: /* IoFileMgrForKernel 0xc7f35804 @ 0x0f00 */
	jr	$ra
	nop
	.globl	IoFileMgrForKernel_E5323C5B
	.type	IoFileMgrForKernel_E5323C5B, @function
IoFileMgrForKernel_E5323C5B: /* IoFileMgrForKernel 0xe5323c5b @ 0x0f08 */
	jr	$ra
	nop
	.globl	sceKernelGetUIDcontrolBlock
	.type	sceKernelGetUIDcontrolBlock, @function
sceKernelGetUIDcontrolBlock: /* SysMemForKernel 0xc90b0992 @ 0x0f10 */
	jr	$ra
	nop
	.globl	sceKernelResumeDispatchThread
	.type	sceKernelResumeDispatchThread, @function
sceKernelResumeDispatchThread: /* ThreadManForKernel 0x27e22ec2 @ 0x0f18 */
	jr	$ra
	nop
	.globl	sceKernelSuspendDispatchThread
	.type	sceKernelSuspendDispatchThread, @function
sceKernelSuspendDispatchThread: /* ThreadManForKernel 0x3ad58b8c @ 0x0f20 */
	jr	$ra
	nop
	.globl	sceKernelNotifyCallback
	.type	sceKernelNotifyCallback, @function
sceKernelNotifyCallback: /* ThreadManForKernel 0xc11ba8c4 @ 0x0f28 */
	jr	$ra
	nop
	.globl	sceKermitMsfs_driver_1A4DC9B7
	.type	sceKermitMsfs_driver_1A4DC9B7, @function
sceKermitMsfs_driver_1A4DC9B7: /* sceKermitMsfs_driver 0x1a4dc9b7 @ 0x0f30 */
	jr	$ra
	nop
	.globl	sceKernelPowerUnlockForUser
	.type	sceKernelPowerUnlockForUser, @function
sceKernelPowerUnlockForUser: /* sceSuspendForKernel 0x3aee7261 @ 0x0f38 */
	jr	$ra
	nop
	.globl	sceKernelPowerLockForUser
	.type	sceKernelPowerLockForUser, @function
sceKernelPowerLockForUser: /* sceSuspendForKernel 0xeadb1bd7 @ 0x0f40 */
	jr	$ra
	nop
	.globl	sceKernelRegisterSysEventHandler
	.type	sceKernelRegisterSysEventHandler, @function
sceKernelRegisterSysEventHandler: /* sceSysEventForKernel 0xcd9e4bb5 @ 0x0f48 */
	jr	$ra
	nop
	.globl	sceKernelUnregisterSysEventHandler
	.type	sceKernelUnregisterSysEventHandler, @function
sceKernelUnregisterSysEventHandler: /* sceSysEventForKernel 0xd7d3fdcd @ 0x0f50 */
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
	.half	3
	.word	.L_imp_nid_0
	.word	sceIoAddDrv
	.word	0
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	3
	.word	.L_imp_nid_1
	.word	sceKernelResumeDispatchThread
	.word	0
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_2
	.word	sceKernelGetUIDcontrolBlock
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_3
	.word	sceKernelPowerUnlockForUser
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_4
	.word	sceKernelRegisterSysEventHandler
	.word	.L_imp_lib_5
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	1
	.word	.L_imp_nid_5
	.word	sceKermitMsfs_driver_1A4DC9B7
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
	.half	0x1007
	.byte	1, 1
	.asciz	"sceMsEmu_Driver"
	.space	12
	.word	_gp
	.word	__lib_ent_top + 4
	.word	__lib_ent_btm
	.word	__lib_stub_top + 4
	.word	__lib_stub_btm

	.section .rodata.sceResident, "a", @progbits
	.align	2
	.word	0
.L_imp_lib_0:
	.asciz	"IoFileMgrForKernel"
	.space	5
.L_imp_lib_1:
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_2:
	.asciz	"SysMemForKernel"
	.space	4
.L_imp_lib_3:
	.asciz	"sceSuspendForKernel"
	.space	4
.L_imp_lib_4:
	.asciz	"sceSysEventForKernel"
	.space	7
.L_imp_lib_5:
	.asciz	"sceKermitMsfs_driver"
	.space	3
.L_exp_tbl_0:
	.word	0xd632acdb
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_start /* module_start (0xd632acdb) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x8e982a74
	.word	0xc7f35804
	.word	0xe5323c5b
.L_imp_nid_1:
	.word	0x27e22ec2
	.word	0x3ad58b8c
	.word	0xc11ba8c4
.L_imp_nid_2:
	.word	0xc90b0992
.L_imp_nid_3:
	.word	0x3aee7261
	.word	0xeadb1bd7
.L_imp_nid_4:
	.word	0xcd9e4bb5
	.word	0xd7d3fdcd
.L_imp_nid_5:
	.word	0x1a4dc9b7

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
