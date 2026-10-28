/* Generated PSP module import/export tables for sceKermit_Driver */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelCpuSuspendIntr
	.type	sceKernelCpuSuspendIntr, @function
sceKernelCpuSuspendIntr: /* InterruptManagerForKernel 0x092968f4 @ 0x0b58 */
	jr	$ra
	nop
	.globl	sceKernelEnableIntr
	.type	sceKernelEnableIntr, @function
sceKernelEnableIntr: /* InterruptManagerForKernel 0x4d6e7305 @ 0x0b60 */
	jr	$ra
	nop
	.globl	sceKernelRegisterIntrHandler
	.type	sceKernelRegisterIntrHandler, @function
sceKernelRegisterIntrHandler: /* InterruptManagerForKernel 0x58dd8978 @ 0x0b68 */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntr
	.type	sceKernelCpuResumeIntr, @function
sceKernelCpuResumeIntr: /* InterruptManagerForKernel 0x5f10d406 @ 0x0b70 */
	jr	$ra
	nop
	.globl	sceKernelReleaseIntrHandler
	.type	sceKernelReleaseIntrHandler, @function
sceKernelReleaseIntrHandler: /* InterruptManagerForKernel 0xf987b1f0 @ 0x0b78 */
	jr	$ra
	nop
	.globl	sceKernelMemcpy
	.type	sceKernelMemcpy, @function
sceKernelMemcpy: /* SysMemForKernel 0x181065ab @ 0x0b80 */
	jr	$ra
	nop
	.globl	sceKernelDeleteSema
	.type	sceKernelDeleteSema, @function
sceKernelDeleteSema: /* ThreadManForKernel 0x28b6489c @ 0x0b88 */
	jr	$ra
	nop
	.globl	sceKernelGetSystemTimeLow
	.type	sceKernelGetSystemTimeLow, @function
sceKernelGetSystemTimeLow: /* ThreadManForKernel 0x369ed59d @ 0x0b90 */
	jr	$ra
	nop
	.globl	sceKernelSignalSema
	.type	sceKernelSignalSema, @function
sceKernelSignalSema: /* ThreadManForKernel 0x3f53e640 @ 0x0b98 */
	jr	$ra
	nop
	.globl	sceKernelWaitSema
	.type	sceKernelWaitSema, @function
sceKernelWaitSema: /* ThreadManForKernel 0x4e3a1105 @ 0x0ba0 */
	jr	$ra
	nop
	.globl	sceKernelWaitSemaCB
	.type	sceKernelWaitSemaCB, @function
sceKernelWaitSemaCB: /* ThreadManForKernel 0x6d212bac @ 0x0ba8 */
	jr	$ra
	nop
	.globl	sceKernelReceiveMsgPipe
	.type	sceKernelReceiveMsgPipe, @function
sceKernelReceiveMsgPipe: /* ThreadManForKernel 0x74829b76 @ 0x0bb0 */
	jr	$ra
	nop
	.globl	sceKernelCreateMsgPipe
	.type	sceKernelCreateMsgPipe, @function
sceKernelCreateMsgPipe: /* ThreadManForKernel 0x7c0dc2a0 @ 0x0bb8 */
	jr	$ra
	nop
	.globl	sceKernelSendMsgPipe
	.type	sceKernelSendMsgPipe, @function
sceKernelSendMsgPipe: /* ThreadManForKernel 0x876dbfad @ 0x0bc0 */
	jr	$ra
	nop
	.globl	sceKernelDelayThread
	.type	sceKernelDelayThread, @function
sceKernelDelayThread: /* ThreadManForKernel 0xceadeb47 @ 0x0bc8 */
	jr	$ra
	nop
	.globl	sceKernelCreateSema
	.type	sceKernelCreateSema, @function
sceKernelCreateSema: /* ThreadManForKernel 0xd6da4ba1 @ 0x0bd0 */
	jr	$ra
	nop
	.globl	sceKernelDcacheWritebackInvalidateRange
	.type	sceKernelDcacheWritebackInvalidateRange, @function
sceKernelDcacheWritebackInvalidateRange: /* UtilsForKernel 0x34b9fa9e @ 0x0bd8 */
	jr	$ra
	nop
	.globl	sceKernelPowerUnlockForUser
	.type	sceKernelPowerUnlockForUser, @function
sceKernelPowerUnlockForUser: /* sceSuspendForKernel 0x3aee7261 @ 0x0be0 */
	jr	$ra
	nop
	.globl	sceKernelPowerLockForUser
	.type	sceKernelPowerLockForUser, @function
sceKernelPowerLockForUser: /* sceSuspendForKernel 0xeadb1bd7 @ 0x0be8 */
	jr	$ra
	nop
	.globl	sceKernelRegisterSysEventHandler
	.type	sceKernelRegisterSysEventHandler, @function
sceKernelRegisterSysEventHandler: /* sceSysEventForKernel 0xcd9e4bb5 @ 0x0bf0 */
	jr	$ra
	nop
	.globl	sceKernelUnregisterSysEventHandler
	.type	sceKernelUnregisterSysEventHandler, @function
sceKernelUnregisterSysEventHandler: /* sceSysEventForKernel 0xd7d3fdcd @ 0x0bf8 */
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
	.half	5
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
	.half	2
	.word	.L_imp_nid_0
	.word	sceKernelRegisterSysEventHandler
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0009
	.byte	6, 0
	.half	5
	.word	.L_imp_nid_1
	.word	sceKernelCpuSuspendIntr
	.word	0
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	10
	.word	.L_imp_nid_2
	.word	sceKernelDeleteSema
	.word	0
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_3
	.word	sceKernelMemcpy
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_4
	.word	sceKernelDcacheWritebackInvalidateRange
	.word	.L_imp_lib_5
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_5
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
	.half	0x1007
	.byte	0, 1
	.asciz	"sceKermit_Driver"
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
	.asciz	"sceSysEventForKernel"
	.space	7
.L_imp_lib_1:
	.asciz	"InterruptManagerForKernel"
	.space	6
.L_imp_lib_2:
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_3:
	.asciz	"SysMemForKernel"
	.space	4
.L_imp_lib_4:
	.asciz	"UtilsForKernel"
	.space	5
.L_imp_lib_5:
	.asciz	"sceSuspendForKernel"
	.align	2
.L_exp_tbl_0:
	.word	0xd632acdb
	.word	0x2f064fa6
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_start
	.word	module_reboot_before
	.word	module_info
	.word	module_sdk_version
.L_exp_lib_1:
	.asciz	"sceKermit_driver"
	.space	3
.L_exp_tbl_1:
	.word	0x36666181
	.word	0x4c3a362d
	.word	0x5280b410
	.word	0x9ddd7dca
	.word	0xd69c50bb
	.word	sceKermit_driver_36666181
	.word	sceKermit_driver_4C3A362D
	.word	sceKermit_driver_5280B410
	.word	sceKermit_driver_9DDD7DCA
	.word	sceKermit_driver_D69C50BB

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0xcd9e4bb5
	.word	0xd7d3fdcd
.L_imp_nid_1:
	.word	0x092968f4
	.word	0x4d6e7305
	.word	0x58dd8978
	.word	0x5f10d406
	.word	0xf987b1f0
.L_imp_nid_2:
	.word	0x28b6489c
	.word	0x369ed59d
	.word	0x3f53e640
	.word	0x4e3a1105
	.word	0x6d212bac
	.word	0x74829b76
	.word	0x7c0dc2a0
	.word	0x876dbfad
	.word	0xceadeb47
	.word	0xd6da4ba1
.L_imp_nid_3:
	.word	0x181065ab
.L_imp_nid_4:
	.word	0x34b9fa9e
.L_imp_nid_5:
	.word	0x3aee7261
	.word	0xeadb1bd7

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
