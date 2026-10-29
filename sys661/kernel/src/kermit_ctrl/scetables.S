/* Generated PSP module import/export tables for sceController_Service */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelApplicationType
	.type	sceKernelApplicationType, @function
sceKernelApplicationType: /* InitForKernel 0x7233b5bc @ 0x1b30 */
	jr	$ra
	nop
	.globl	sceKernelCpuSuspendIntr
	.type	sceKernelCpuSuspendIntr, @function
sceKernelCpuSuspendIntr: /* InterruptManagerForKernel 0x092968f4 @ 0x1b38 */
	jr	$ra
	nop
	.globl	sceKernelDisableSubIntr
	.type	sceKernelDisableSubIntr, @function
sceKernelDisableSubIntr: /* InterruptManagerForKernel 0x4023e1a7 @ 0x1b40 */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntr
	.type	sceKernelCpuResumeIntr, @function
sceKernelCpuResumeIntr: /* InterruptManagerForKernel 0x5f10d406 @ 0x1b48 */
	jr	$ra
	nop
	.globl	sceKernelReleaseSubIntrHandler
	.type	sceKernelReleaseSubIntrHandler, @function
sceKernelReleaseSubIntrHandler: /* InterruptManagerForKernel 0xd61e6961 @ 0x1b50 */
	jr	$ra
	nop
	.globl	sceKernelEnableSubIntr
	.type	sceKernelEnableSubIntr, @function
sceKernelEnableSubIntr: /* InterruptManagerForKernel 0xfb8e22ec @ 0x1b58 */
	jr	$ra
	nop
	.globl	sceKernelRegisterSubIntrHandler
	.type	sceKernelRegisterSubIntrHandler, @function
sceKernelRegisterSubIntrHandler: /* InterruptManagerForKernel 0xffa8b183 @ 0x1b60 */
	jr	$ra
	nop
	.globl	sceKernelGetModel
	.type	sceKernelGetModel, @function
sceKernelGetModel: /* SysMemForKernel 0x07c586a1 @ 0x1b68 */
	jr	$ra
	nop
	.globl	sceKernelMemcpy
	.type	sceKernelMemcpy, @function
sceKernelMemcpy: /* SysMemForKernel 0x181065ab @ 0x1b70 */
	jr	$ra
	nop
	.globl	sceKernelGetCompiledSdkVersion
	.type	sceKernelGetCompiledSdkVersion, @function
sceKernelGetCompiledSdkVersion: /* SysMemForKernel 0xb4f00cb5 @ 0x1b78 */
	jr	$ra
	nop
	.globl	sceSTimerStartCount
	.type	sceSTimerStartCount, @function
sceSTimerStartCount: /* SysTimerForKernel 0x4bc8c61c @ 0x1b80 */
	jr	$ra
	nop
	.globl	sceSTimerAlloc
	.type	sceSTimerAlloc, @function
sceSTimerAlloc: /* SysTimerForKernel 0x893dec1a @ 0x1b88 */
	jr	$ra
	nop
	.globl	sceSTimerFree
	.type	sceSTimerFree, @function
sceSTimerFree: /* SysTimerForKernel 0x93b952d1 @ 0x1b90 */
	jr	$ra
	nop
	.globl	sceSTimerStopCount
	.type	sceSTimerStopCount, @function
sceSTimerStopCount: /* SysTimerForKernel 0x973104d0 @ 0x1b98 */
	jr	$ra
	nop
	.globl	sceSTimerSetPrscl
	.type	sceSTimerSetPrscl, @function
sceSTimerSetPrscl: /* SysTimerForKernel 0xcac43feb @ 0x1ba0 */
	jr	$ra
	nop
	.globl	sceSTimerSetHandler
	.type	sceSTimerSetHandler, @function
sceSTimerSetHandler: /* SysTimerForKernel 0xeee11ca5 @ 0x1ba8 */
	jr	$ra
	nop
	.globl	memset
	.type	memset, @function
memset: /* SysclibForKernel 0x10f3bb61 @ 0x1bb0 */
	jr	$ra
	nop
	.globl	sceKernelSetEventFlag
	.type	sceKernelSetEventFlag, @function
sceKernelSetEventFlag: /* ThreadManForKernel 0x1fb15a32 @ 0x1bb8 */
	jr	$ra
	nop
	.globl	sceKernelGetSystemTimeLow
	.type	sceKernelGetSystemTimeLow, @function
sceKernelGetSystemTimeLow: /* ThreadManForKernel 0x369ed59d @ 0x1bc0 */
	jr	$ra
	nop
	.globl	sceKernelWaitEventFlag
	.type	sceKernelWaitEventFlag, @function
sceKernelWaitEventFlag: /* ThreadManForKernel 0x402fcf22 @ 0x1bc8 */
	jr	$ra
	nop
	.globl	sceKernelCreateEventFlag
	.type	sceKernelCreateEventFlag, @function
sceKernelCreateEventFlag: /* ThreadManForKernel 0x55c20a00 @ 0x1bd0 */
	jr	$ra
	nop
	.globl	sceKernelSetAlarm
	.type	sceKernelSetAlarm, @function
sceKernelSetAlarm: /* ThreadManForKernel 0x6652b8ca @ 0x1bd8 */
	jr	$ra
	nop
	.globl	sceKernelClearEventFlag
	.type	sceKernelClearEventFlag, @function
sceKernelClearEventFlag: /* ThreadManForKernel 0x812346e4 @ 0x1be0 */
	jr	$ra
	nop
	.globl	sceKernelDeleteEventFlag
	.type	sceKernelDeleteEventFlag, @function
sceKernelDeleteEventFlag: /* ThreadManForKernel 0xef9e4c70 @ 0x1be8 */
	jr	$ra
	nop
	.globl	sceDisplayWaitVblankStart
	.type	sceDisplayWaitVblankStart, @function
sceDisplayWaitVblankStart: /* sceDisplay_driver 0xb0942511 @ 0x1bf0 */
	jr	$ra
	nop
	.globl	sceKermitPeripheral_driver_9EF7501D
	.type	sceKermitPeripheral_driver_9EF7501D, @function
sceKermitPeripheral_driver_9EF7501D: /* sceKermitPeripheral_driver 0x9ef7501d @ 0x1bf8 */
	jr	$ra
	nop
	.globl	sceKernelPowerTick
	.type	sceKernelPowerTick, @function
sceKernelPowerTick: /* sceSuspendForKernel 0x090ccb3f @ 0x1c00 */
	jr	$ra
	nop
	.globl	sceKernelRegisterSysEventHandler
	.type	sceKernelRegisterSysEventHandler, @function
sceKernelRegisterSysEventHandler: /* sceSysEventForKernel 0xcd9e4bb5 @ 0x1c08 */
	jr	$ra
	nop
	.globl	sceKernelUnregisterSysEventHandler
	.type	sceKernelUnregisterSysEventHandler, @function
sceKernelUnregisterSysEventHandler: /* sceSysEventForKernel 0xd7d3fdcd @ 0x1c10 */
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
	.half	0x4001
	.byte	5, 0
	.half	20
	.word	.L_exp_tbl_1
	.word	0x00020000
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	36
	.word	.L_exp_tbl_2
	.word	0x00020000

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
	.half	3
	.word	.L_imp_nid_0
	.word	sceKernelGetModel
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_1
	.word	sceKernelPowerTick
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_2
	.word	sceKernelRegisterSysEventHandler
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0009
	.byte	6, 0
	.half	6
	.word	.L_imp_nid_3
	.word	sceKernelCpuSuspendIntr
	.word	0
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_4
	.word	memset
	.word	.L_imp_lib_5
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	7
	.word	.L_imp_nid_5
	.word	sceKernelSetEventFlag
	.word	0
	.word	.L_imp_lib_6
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	6
	.word	.L_imp_nid_6
	.word	sceSTimerStartCount
	.word	.L_imp_lib_7
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_7
	.word	sceKernelApplicationType
	.word	.L_imp_lib_8
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_8
	.word	sceDisplayWaitVblankStart
	.word	.L_imp_lib_9
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_9
	.word	sceKermitPeripheral_driver_9EF7501D

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
	.byte	10, 1
	.asciz	"sceController_Service"
	.space	6
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
	.asciz	"sceSuspendForKernel"
	.space	4
.L_imp_lib_2:
	.asciz	"sceSysEventForKernel"
	.space	7
.L_imp_lib_3:
	.asciz	"InterruptManagerForKernel"
	.space	6
.L_imp_lib_4:
	.asciz	"SysclibForKernel"
	.space	7
.L_imp_lib_5:
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_6:
	.asciz	"SysTimerForKernel"
	.space	6
.L_imp_lib_7:
	.asciz	"InitForKernel"
	.space	6
.L_imp_lib_8:
	.asciz	"sceDisplay_driver"
	.space	6
.L_imp_lib_9:
	.asciz	"sceKermitPeripheral_driver"
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
	.asciz	"sceCtrl"
	.align	2
.L_exp_tbl_1:
	.word	0x02baad91
	.word	0x0b588501
	.word	0x1098030b
	.word	0x1f4011e6
	.word	0x1f803938
	.word	0x239a6ba7
	.word	0x348d99d4
	.word	0x3a622550
	.word	0x5a36b1c2
	.word	0x60b81f86
	.word	0x6841be1a
	.word	0x687660fa
	.word	0x6a2774f3
	.word	0x7c3675ab
	.word	0xa68fd260
	.word	0xa7144800
	.word	0xaf5960f3
	.word	0xb1d0e5cd
	.word	0xc152080a
	.word	0xda6b76a1
	.word	sceCtrlGetSamplingCycle /* sceCtrlGetSamplingCycle (0x02baad91) */
	.word	sceCtrlReadLatch /* sceCtrlReadLatch (0x0b588501) */
	.word	sceCtrlReadBufferPositive2 /* sceCtrlReadBufferPositive2 (0x1098030b) */
	.word	sceCtrlSetSamplingMode /* sceCtrlSetSamplingMode (0x1f4011e6) */
	.word	sceCtrlReadBufferPositive /* sceCtrlReadBufferPositive (0x1f803938) */
	.word	sceCtrlPeekBufferNegative2 /* sceCtrlPeekBufferNegative2 (0x239a6ba7) */
	.word	sceCtrlSetSuspendingExtraSamples /* sceCtrlSetSuspendingExtraSamples (0x348d99d4) */
	.word	sceCtrlPeekBufferPositive /* sceCtrlPeekBufferPositive (0x3a622550) */
	.word	sceCtrlPeekBufferPositive2 /* sceCtrlPeekBufferPositive2 (0x5a36b1c2) */
	.word	sceCtrlReadBufferNegative /* sceCtrlReadBufferNegative (0x60b81f86) */
	.word	sceCtrlSetRapidFire /* sceCtrlSetRapidFire (0x6841be1a) */
	.word	sceCtrlGetIdleCancelThreshold /* sceCtrlGetIdleCancelThreshold (0x687660fa) */
	.word	sceCtrlSetSamplingCycle /* sceCtrlSetSamplingCycle (0x6a2774f3) */
	.word	sceCtrlReadBufferNegative2 /* sceCtrlReadBufferNegative2 (0x7c3675ab) */
	.word	sceCtrlClearRapidFire /* sceCtrlClearRapidFire (0xa68fd260) */
	.word	sceCtrlSetIdleCancelThreshold /* sceCtrlSetIdleCancelThreshold (0xa7144800) */
	.word	sceCtrlGetSuspendingExtraSamples /* sceCtrlGetSuspendingExtraSamples (0xaf5960f3) */
	.word	sceCtrlPeekLatch /* sceCtrlPeekLatch (0xb1d0e5cd) */
	.word	sceCtrlPeekBufferNegative /* sceCtrlPeekBufferNegative (0xc152080a) */
	.word	sceCtrlGetSamplingMode /* sceCtrlGetSamplingMode (0xda6b76a1) */
	.word	0x00020000
	.word	0x00060005
	.word	0x00080008
	.word	0x000d0009
	.word	0x000e000e
	.word	0x0011000e
	.word	0x00130012
	.word	0x00140014
.L_exp_lib_2:
	.asciz	"sceCtrl_driver"
	.space	1
.L_exp_tbl_2:
	.word	0x121097d5
	.word	0x1809b9fc
	.word	0x1a1a7d40
	.word	0x2ba616af
	.word	0x33d03fd5
	.word	0x365be224
	.word	0x37533267
	.word	0x3a6a612a
	.word	0x3bd76ede
	.word	0x41bcd9ed
	.word	0x501e0c70
	.word	0x5130dae3
	.word	0x547f89d3
	.word	0x55497589
	.word	0x5886194c
	.word	0x637cb76c
	.word	0x6a1df4cb
	.word	0x6c86af22
	.word	0x7511ccfe
	.word	0x7abdebaa
	.word	0x7f7c4e0a
	.word	0x83b15a81
	.word	0x89438c13
	.word	0x994488ec
	.word	0xbc8d1a3b
	.word	0xbe30ced0
	.word	0xd4692e77
	.word	0xdb76878d
	.word	0xdf53e160
	.word	0xe467bec8
	.word	0xe54253e7
	.word	0xe6085c33
	.word	0xf0074903
	.word	0xf6e94ea3
	.word	0xf8346777
	.word	0xf8ec18bd
	.word	sceCtrlInit /* sceCtrlInit (0x121097d5) */
	.word	sceCtrlGetButtonIntercept /* sceCtrlGetButtonIntercept (0x1809b9fc) */
	.word	sceCtrlEnd /* sceCtrlEnd (0x1a1a7d40) */
	.word	sceCtrlPeekBufferPositive /* sceCtrlPeekBufferPositive (0x2ba616af) */
	.word	sceCtrlResume /* sceCtrlResume (0x33d03fd5) */
	.word	sceCtrlUpdateCableTypeReq /* sceCtrlUpdateCableTypeReq (0x365be224) */
	.word	sceCtrlSetIdleCancelThreshold /* sceCtrlSetIdleCancelThreshold (0x37533267) */
	.word	sceCtrlReadBufferNegative /* sceCtrlReadBufferNegative (0x3a6a612a) */
	.word	sceCtrlReadBufferPositive2 /* sceCtrlReadBufferPositive2 (0x3bd76ede) */
	.word	sceCtrlPeekBufferNegative2 /* sceCtrlPeekBufferNegative2 (0x41bcd9ed) */
	.word	sceCtrlGetSamplingCycle /* sceCtrlGetSamplingCycle (0x501e0c70) */
	.word	sceCtrlSetButtonEmulation /* sceCtrlSetButtonEmulation (0x5130dae3) */
	.word	sceCtrlSetSuspendingExtraSamples /* sceCtrlSetSuspendingExtraSamples (0x547f89d3) */
	.word	sceCtrlSuspend /* sceCtrlSuspend (0x55497589) */
	.word	sceCtrl_driver_5886194C /* sceCtrl_driver_5886194C (0x5886194c) */
	.word	sceCtrlPeekLatch /* sceCtrlPeekLatch (0x637cb76c) */
	.word	sceCtrlSetIdleCancelKey /* sceCtrlSetIdleCancelKey (0x6a1df4cb) */
	.word	sceCtrl_driver_6C86AF22 /* sceCtrl_driver_6C86AF22 (0x6c86af22) */
	.word	sceCtrlGetIdleCancelKey /* sceCtrlGetIdleCancelKey (0x7511ccfe) */
	.word	sceCtrlReadBufferNegative2 /* sceCtrlReadBufferNegative2 (0x7abdebaa) */
	.word	sceCtrlReadLatch /* sceCtrlReadLatch (0x7f7c4e0a) */
	.word	sceCtrlSetSamplingCycle /* sceCtrlSetSamplingCycle (0x83b15a81) */
	.word	sceCtrlSetRapidFire /* sceCtrlSetRapidFire (0x89438c13) */
	.word	sceCtrlClearRapidFire /* sceCtrlClearRapidFire (0x994488ec) */
	.word	sceCtrlGetSuspendingExtraSamples /* sceCtrlGetSuspendingExtraSamples (0xbc8d1a3b) */
	.word	sceCtrlReadBufferPositive /* sceCtrlReadBufferPositive (0xbe30ced0) */
	.word	sceCtrlPeekBufferPositive2 /* sceCtrlPeekBufferPositive2 (0xd4692e77) */
	.word	sceCtrlSetAnalogEmulation /* sceCtrlSetAnalogEmulation (0xdb76878d) */
	.word	sceCtrlSetSpecialButtonCallback /* sceCtrlSetSpecialButtonCallback (0xdf53e160) */
	.word	sceCtrl_driver_E467BEC8 /* sceCtrl_driver_E467BEC8 (0xe467bec8) */
	.word	sceCtrlGetIdleCancelThreshold /* sceCtrlGetIdleCancelThreshold (0xe54253e7) */
	.word	sceCtrlPeekBufferNegative /* sceCtrlPeekBufferNegative (0xe6085c33) */
	.word	sceCtrlSetPollingMode /* sceCtrlSetPollingMode (0xf0074903) */
	.word	sceCtrlSetSamplingMode /* sceCtrlSetSamplingMode (0xf6e94ea3) */
	.word	sceCtrlSetButtonIntercept /* sceCtrlSetButtonIntercept (0xf8346777) */
	.word	sceCtrlGetSamplingMode /* sceCtrlGetSamplingMode (0xf8ec18bd) */
	.word	0x00000000
	.word	0x00040003
	.word	0x000a0009
	.word	0x0012000f
	.word	0x00170015
	.word	0x00180018
	.word	0x001a001a
	.word	0x0020001d

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x07c586a1
	.word	0x181065ab
	.word	0xb4f00cb5
.L_imp_nid_1:
	.word	0x090ccb3f
.L_imp_nid_2:
	.word	0xcd9e4bb5
	.word	0xd7d3fdcd
.L_imp_nid_3:
	.word	0x092968f4
	.word	0x4023e1a7
	.word	0x5f10d406
	.word	0xd61e6961
	.word	0xfb8e22ec
	.word	0xffa8b183
.L_imp_nid_4:
	.word	0x10f3bb61
.L_imp_nid_5:
	.word	0x1fb15a32
	.word	0x369ed59d
	.word	0x402fcf22
	.word	0x55c20a00
	.word	0x6652b8ca
	.word	0x812346e4
	.word	0xef9e4c70
.L_imp_nid_6:
	.word	0x4bc8c61c
	.word	0x893dec1a
	.word	0x93b952d1
	.word	0x973104d0
	.word	0xcac43feb
	.word	0xeee11ca5
.L_imp_nid_7:
	.word	0x7233b5bc
.L_imp_nid_8:
	.word	0xb0942511
.L_imp_nid_9:
	.word	0x9ef7501d

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
