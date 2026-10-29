/* Generated PSP module import/export tables for scePower_Service */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelApplicationType
	.type	sceKernelApplicationType, @function
sceKernelApplicationType: /* InitForKernel 0x7233b5bc @ 0x4580 */
	jr	$ra
	nop
	.globl	sceKernelSetInitCallback
	.type	sceKernelSetInitCallback, @function
sceKernelSetInitCallback: /* InitForKernel 0x9f9ae99c @ 0x4588 */
	jr	$ra
	nop
	.globl	sceKernelInitParamSfo
	.type	sceKernelInitParamSfo, @function
sceKernelInitParamSfo: /* InitForKernel 0xd83a9bd7 @ 0x4590 */
	jr	$ra
	nop
	.globl	sceKernelCpuSuspendIntr
	.type	sceKernelCpuSuspendIntr, @function
sceKernelCpuSuspendIntr: /* InterruptManagerForKernel 0x092968f4 @ 0x4598 */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntrWithSync
	.type	sceKernelCpuResumeIntrWithSync, @function
sceKernelCpuResumeIntrWithSync: /* InterruptManagerForKernel 0x3b84732d @ 0x45a0 */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntr
	.type	sceKernelCpuResumeIntr, @function
sceKernelCpuResumeIntr: /* InterruptManagerForKernel 0x5f10d406 @ 0x45a8 */
	jr	$ra
	nop
	.globl	sceKernelReleaseSubIntrHandler
	.type	sceKernelReleaseSubIntrHandler, @function
sceKernelReleaseSubIntrHandler: /* InterruptManagerForKernel 0xd61e6961 @ 0x45b0 */
	jr	$ra
	nop
	.globl	sceKernelEnableSubIntr
	.type	sceKernelEnableSubIntr, @function
sceKernelEnableSubIntr: /* InterruptManagerForKernel 0xfb8e22ec @ 0x45b8 */
	jr	$ra
	nop
	.globl	sceKernelRegisterSubIntrHandler
	.type	sceKernelRegisterSubIntrHandler, @function
sceKernelRegisterSubIntrHandler: /* InterruptManagerForKernel 0xffa8b183 @ 0x45c0 */
	jr	$ra
	nop
	.globl	Kprintf
	.type	Kprintf, @function
Kprintf: /* KDebugForKernel 0x84f370bc @ 0x45c8 */
	jr	$ra
	nop
	.globl	sceKernelDipsw
	.type	sceKernelDipsw, @function
sceKernelDipsw: /* KDebugForKernel 0x86010fcb @ 0x45d0 */
	jr	$ra
	nop
	.globl	sceKernelSm1ReferOperations
	.type	sceKernelSm1ReferOperations, @function
sceKernelSm1ReferOperations: /* KDebugForKernel 0xe892d9a1 @ 0x45d8 */
	jr	$ra
	nop
	.globl	sceKernelGetModel
	.type	sceKernelGetModel, @function
sceKernelGetModel: /* SysMemForKernel 0x07c586a1 @ 0x45e0 */
	jr	$ra
	nop
	.globl	sceKernelSetDdrMemoryProtection
	.type	sceKernelSetDdrMemoryProtection, @function
sceKernelSetDdrMemoryProtection: /* SysMemForKernel 0x83b5226d @ 0x45e8 */
	jr	$ra
	nop
	.globl	sceKernelGetCompiledSdkVersion
	.type	sceKernelGetCompiledSdkVersion, @function
sceKernelGetCompiledSdkVersion: /* SysMemForKernel 0xb4f00cb5 @ 0x45f0 */
	jr	$ra
	nop
	.globl	sceKernelQueryMemoryPartitionInfo
	.type	sceKernelQueryMemoryPartitionInfo, @function
sceKernelQueryMemoryPartitionInfo: /* SysMemForKernel 0xc4eeaf20 @ 0x45f8 */
	jr	$ra
	nop
	.globl	sceKernelGetUIDcontrolBlock
	.type	sceKernelGetUIDcontrolBlock, @function
sceKernelGetUIDcontrolBlock: /* SysMemForKernel 0xc90b0992 @ 0x4600 */
	jr	$ra
	nop
	.globl	sceKernelGetGameInfo
	.type	sceKernelGetGameInfo, @function
sceKernelGetGameInfo: /* SysMemForKernel 0xef29061c @ 0x4608 */
	jr	$ra
	nop
	.globl	memset
	.type	memset, @function
memset: /* SysclibForKernel 0x10f3bb61 @ 0x4610 */
	jr	$ra
	nop
	.globl	sceKernelSetEventFlag
	.type	sceKernelSetEventFlag, @function
sceKernelSetEventFlag: /* ThreadManForKernel 0x1fb15a32 @ 0x4618 */
	jr	$ra
	nop
	.globl	sceKernelWaitThreadEnd
	.type	sceKernelWaitThreadEnd, @function
sceKernelWaitThreadEnd: /* ThreadManForKernel 0x278c0df5 @ 0x4620 */
	jr	$ra
	nop
	.globl	sceKernelDeleteSema
	.type	sceKernelDeleteSema, @function
sceKernelDeleteSema: /* ThreadManForKernel 0x28b6489c @ 0x4628 */
	jr	$ra
	nop
	.globl	sceKernelGetThreadId
	.type	sceKernelGetThreadId, @function
sceKernelGetThreadId: /* ThreadManForKernel 0x293b45b8 @ 0x4630 */
	jr	$ra
	nop
	.globl	sceKernelGetCallbackCount
	.type	sceKernelGetCallbackCount, @function
sceKernelGetCallbackCount: /* ThreadManForKernel 0x2a3d44ff @ 0x4638 */
	jr	$ra
	nop
	.globl	sceKernelPollEventFlag
	.type	sceKernelPollEventFlag, @function
sceKernelPollEventFlag: /* ThreadManForKernel 0x30fd48f0 @ 0x4640 */
	jr	$ra
	nop
	.globl	sceKernelGetThreadExitStatus
	.type	sceKernelGetThreadExitStatus, @function
sceKernelGetThreadExitStatus: /* ThreadManForKernel 0x3b183e26 @ 0x4648 */
	jr	$ra
	nop
	.globl	sceKernelSignalSema
	.type	sceKernelSignalSema, @function
sceKernelSignalSema: /* ThreadManForKernel 0x3f53e640 @ 0x4650 */
	jr	$ra
	nop
	.globl	sceKernelWaitEventFlag
	.type	sceKernelWaitEventFlag, @function
sceKernelWaitEventFlag: /* ThreadManForKernel 0x402fcf22 @ 0x4658 */
	jr	$ra
	nop
	.globl	sceKernelCreateThread
	.type	sceKernelCreateThread, @function
sceKernelCreateThread: /* ThreadManForKernel 0x446d8de6 @ 0x4660 */
	jr	$ra
	nop
	.globl	sceKernelWaitSema
	.type	sceKernelWaitSema, @function
sceKernelWaitSema: /* ThreadManForKernel 0x4e3a1105 @ 0x4668 */
	jr	$ra
	nop
	.globl	sceKernelCreateEventFlag
	.type	sceKernelCreateEventFlag, @function
sceKernelCreateEventFlag: /* ThreadManForKernel 0x55c20a00 @ 0x4670 */
	jr	$ra
	nop
	.globl	sceKernelGetThreadmanIdType
	.type	sceKernelGetThreadmanIdType, @function
sceKernelGetThreadmanIdType: /* ThreadManForKernel 0x57cf62dd @ 0x4678 */
	jr	$ra
	nop
	.globl	sceKernelPollSema
	.type	sceKernelPollSema, @function
sceKernelPollSema: /* ThreadManForKernel 0x58b1f937 @ 0x4680 */
	jr	$ra
	nop
	.globl	sceKernelUnlockMutex
	.type	sceKernelUnlockMutex, @function
sceKernelUnlockMutex: /* ThreadManForKernel 0x6b30100f @ 0x4688 */
	jr	$ra
	nop
	.globl	sceKernelCancelAlarm
	.type	sceKernelCancelAlarm, @function
sceKernelCancelAlarm: /* ThreadManForKernel 0x7e65b999 @ 0x4690 */
	jr	$ra
	nop
	.globl	sceKernelClearEventFlag
	.type	sceKernelClearEventFlag, @function
sceKernelClearEventFlag: /* ThreadManForKernel 0x812346e4 @ 0x4698 */
	jr	$ra
	nop
	.globl	sceKernelGetSystemTimeWide
	.type	sceKernelGetSystemTimeWide, @function
sceKernelGetSystemTimeWide: /* ThreadManForKernel 0x82bc5777 @ 0x46a0 */
	jr	$ra
	nop
	.globl	sceKernelLockMutex
	.type	sceKernelLockMutex, @function
sceKernelLockMutex: /* ThreadManForKernel 0xb011b11f @ 0x46a8 */
	jr	$ra
	nop
	.globl	sceKernelCreateMutex
	.type	sceKernelCreateMutex, @function
sceKernelCreateMutex: /* ThreadManForKernel 0xb7d098c6 @ 0x46b0 */
	jr	$ra
	nop
	.globl	sceKernelNotifyCallback
	.type	sceKernelNotifyCallback, @function
sceKernelNotifyCallback: /* ThreadManForKernel 0xc11ba8c4 @ 0x46b8 */
	jr	$ra
	nop
	.globl	sceKernelDelayThread
	.type	sceKernelDelayThread, @function
sceKernelDelayThread: /* ThreadManForKernel 0xceadeb47 @ 0x46c0 */
	jr	$ra
	nop
	.globl	sceKernelCreateSema
	.type	sceKernelCreateSema, @function
sceKernelCreateSema: /* ThreadManForKernel 0xd6da4ba1 @ 0x46c8 */
	jr	$ra
	nop
	.globl	sceKernelDeleteEventFlag
	.type	sceKernelDeleteEventFlag, @function
sceKernelDeleteEventFlag: /* ThreadManForKernel 0xef9e4c70 @ 0x46d0 */
	jr	$ra
	nop
	.globl	sceKernelStartThread
	.type	sceKernelStartThread, @function
sceKernelStartThread: /* ThreadManForKernel 0xf475845d @ 0x46d8 */
	jr	$ra
	nop
	.globl	sceKernelGetUserLevel
	.type	sceKernelGetUserLevel, @function
sceKernelGetUserLevel: /* ThreadManForKernel 0xf6427665 @ 0x46e0 */
	jr	$ra
	nop
	.globl	sceKernelDeleteMutex
	.type	sceKernelDeleteMutex, @function
sceKernelDeleteMutex: /* ThreadManForKernel 0xf8170fbe @ 0x46e8 */
	jr	$ra
	nop
	.globl	sceKernelDcacheWritebackInvalidateRange
	.type	sceKernelDcacheWritebackInvalidateRange, @function
sceKernelDcacheWritebackInvalidateRange: /* UtilsForKernel 0x34b9fa9e @ 0x46f0 */
	jr	$ra
	nop
	.globl	UtilsForKernel_39FFB756
	.type	UtilsForKernel_39FFB756, @function
UtilsForKernel_39FFB756: /* UtilsForKernel 0x39ffb756 @ 0x46f8 */
	jr	$ra
	nop
	.globl	sceKernelDcacheWritebackAll
	.type	sceKernelDcacheWritebackAll, @function
sceKernelDcacheWritebackAll: /* UtilsForKernel 0x79d1c3fa @ 0x4700 */
	jr	$ra
	nop
	.globl	sceKernelUtilsSha1Digest
	.type	sceKernelUtilsSha1Digest, @function
sceKernelUtilsSha1Digest: /* UtilsForKernel 0x840259f1 @ 0x4708 */
	jr	$ra
	nop
	.globl	UtilsForKernel_A6B0A6B8
	.type	UtilsForKernel_A6B0A6B8, @function
UtilsForKernel_A6B0A6B8: /* UtilsForKernel 0xa6b0a6b8 @ 0x4710 */
	jr	$ra
	nop
	.globl	sceCtrlSetButtonIntercept
	.type	sceCtrlSetButtonIntercept, @function
sceCtrlSetButtonIntercept: /* sceCtrl_driver 0xf8346777 @ 0x4718 */
	jr	$ra
	nop
	.globl	sceDdr_driver_00E36648
	.type	sceDdr_driver_00E36648, @function
sceDdr_driver_00E36648: /* sceDdr_driver 0x00e36648 @ 0x4720 */
	jr	$ra
	nop
	.globl	sceDdr_driver_0BAAE4C5
	.type	sceDdr_driver_0BAAE4C5, @function
sceDdr_driver_0BAAE4C5: /* sceDdr_driver 0x0baae4c5 @ 0x4728 */
	jr	$ra
	nop
	.globl	sceDdr_driver_CF07B9E2
	.type	sceDdr_driver_CF07B9E2, @function
sceDdr_driver_CF07B9E2: /* sceDdr_driver 0xcf07b9e2 @ 0x4730 */
	jr	$ra
	nop
	.globl	sceDdr_driver_E0A39D3E
	.type	sceDdr_driver_E0A39D3E, @function
sceDdr_driver_E0A39D3E: /* sceDdr_driver 0xe0a39d3e @ 0x4738 */
	jr	$ra
	nop
	.globl	sceDisplayIsVblank
	.type	sceDisplayIsVblank, @function
sceDisplayIsVblank: /* sceDisplay_driver 0x572d7804 @ 0x4740 */
	jr	$ra
	nop
	.globl	sceDisplayWaitVblankStart
	.type	sceDisplayWaitVblankStart, @function
sceDisplayWaitVblankStart: /* sceDisplay_driver 0xb0942511 @ 0x4748 */
	jr	$ra
	nop
	.globl	sceDisplay_driver_CE8A328E
	.type	sceDisplay_driver_CE8A328E, @function
sceDisplay_driver_CE8A328E: /* sceDisplay_driver 0xce8a328e @ 0x4750 */
	jr	$ra
	nop
	.globl	sceDisplayAdjustAccumulatedHcount
	.type	sceDisplayAdjustAccumulatedHcount, @function
sceDisplayAdjustAccumulatedHcount: /* sceDisplay_driver 0xd81a8ff9 @ 0x4758 */
	jr	$ra
	nop
	.globl	sceDisplayGetCurrentHcount
	.type	sceDisplayGetCurrentHcount, @function
sceDisplayGetCurrentHcount: /* sceDisplay_driver 0xec80d435 @ 0x4760 */
	jr	$ra
	nop
	.globl	sceDisplayGetAccumulatedHcount
	.type	sceDisplayGetAccumulatedHcount, @function
sceDisplayGetAccumulatedHcount: /* sceDisplay_driver 0xf84d16cc @ 0x4768 */
	jr	$ra
	nop
	.globl	sceGeEdramSetRefreshParam
	.type	sceGeEdramSetRefreshParam, @function
sceGeEdramSetRefreshParam: /* sceGe_driver 0x2539d845 @ 0x4770 */
	jr	$ra
	nop
	.globl	sceKermitPeripheral_driver_0648E1A3
	.type	sceKermitPeripheral_driver_0648E1A3, @function
sceKermitPeripheral_driver_0648E1A3: /* sceKermitPeripheral_driver 0x0648e1a3 @ 0x4778 */
	jr	$ra
	nop
	.globl	sceKermitPeripheral_driver_15F5C107
	.type	sceKermitPeripheral_driver_15F5C107, @function
sceKermitPeripheral_driver_15F5C107: /* sceKermitPeripheral_driver 0x15f5c107 @ 0x4780 */
	jr	$ra
	nop
	.globl	sceKermitPeripheral_driver_65D98A21
	.type	sceKermitPeripheral_driver_65D98A21, @function
sceKermitPeripheral_driver_65D98A21: /* sceKermitPeripheral_driver 0x65d98a21 @ 0x4788 */
	jr	$ra
	nop
	.globl	sceKermitPeripheral_driver_6D6AA215
	.type	sceKermitPeripheral_driver_6D6AA215, @function
sceKermitPeripheral_driver_6D6AA215: /* sceKermitPeripheral_driver 0x6d6aa215 @ 0x4790 */
	jr	$ra
	nop
	.globl	sceKermit_driver_5280B410
	.type	sceKermit_driver_5280B410, @function
sceKermit_driver_5280B410: /* sceKermit_driver 0x5280b410 @ 0x4798 */
	jr	$ra
	nop
	.globl	sceKernelDispatchResumeHandlers
	.type	sceKernelDispatchResumeHandlers, @function
sceKernelDispatchResumeHandlers: /* sceSuspendForKernel 0x0ab0c6f3 @ 0x47a0 */
	jr	$ra
	nop
	.globl	sceSuspendForKernel_67B59042
	.type	sceSuspendForKernel_67B59042, @function
sceSuspendForKernel_67B59042: /* sceSuspendForKernel 0x67b59042 @ 0x47a8 */
	jr	$ra
	nop
	.globl	sceKernelDispatchSuspendHandlers
	.type	sceKernelDispatchSuspendHandlers, @function
sceKernelDispatchSuspendHandlers: /* sceSuspendForKernel 0x8f58b1ec @ 0x47b0 */
	jr	$ra
	nop
	.globl	sceSuspendForKernel_B2C9640B
	.type	sceSuspendForKernel_B2C9640B, @function
sceSuspendForKernel_B2C9640B: /* sceSuspendForKernel 0xb2c9640b @ 0x47b8 */
	jr	$ra
	nop
	.globl	sceKernelRegisterPowerHandlers
	.type	sceKernelRegisterPowerHandlers, @function
sceKernelRegisterPowerHandlers: /* sceSuspendForKernel 0xbde686cd @ 0x47c0 */
	jr	$ra
	nop
	.globl	sceKernelSysEventDispatch
	.type	sceKernelSysEventDispatch, @function
sceKernelSysEventDispatch: /* sceSysEventForKernel 0x36331294 @ 0x47c8 */
	jr	$ra
	nop
	.globl	sceKernelRegisterSysEventHandler
	.type	sceKernelRegisterSysEventHandler, @function
sceKernelRegisterSysEventHandler: /* sceSysEventForKernel 0xcd9e4bb5 @ 0x47d0 */
	jr	$ra
	nop
	.globl	sceKernelUnregisterSysEventHandler
	.type	sceKernelUnregisterSysEventHandler, @function
sceKernelUnregisterSysEventHandler: /* sceSysEventForKernel 0xd7d3fdcd @ 0x47d8 */
	jr	$ra
	nop
	.globl	sceSysregGpioIoDisable
	.type	sceSysregGpioIoDisable, @function
sceSysregGpioIoDisable: /* sceSysreg_driver 0x15dc34bc @ 0x47e0 */
	jr	$ra
	nop
	.globl	sceSysregPllGetOutSelect
	.type	sceSysregPllGetOutSelect, @function
sceSysregPllGetOutSelect: /* sceSysreg_driver 0x187f651d @ 0x47e8 */
	jr	$ra
	nop
	.globl	sceSysreg_driver_2458B6AC
	.type	sceSysreg_driver_2458B6AC, @function
sceSysreg_driver_2458B6AC: /* sceSysreg_driver 0x2458b6ac @ 0x47f0 */
	jr	$ra
	nop
	.globl	sceSysreg_driver_3F6F2CC7
	.type	sceSysreg_driver_3F6F2CC7, @function
sceSysreg_driver_3F6F2CC7: /* sceSysreg_driver 0x3f6f2cc7 @ 0x47f8 */
	jr	$ra
	nop
	.globl	sceSysregGetFuseId
	.type	sceSysregGetFuseId, @function
sceSysregGetFuseId: /* sceSysreg_driver 0x4f46eede @ 0x4800 */
	jr	$ra
	nop
	.globl	sceSysreg_driver_63E1EE9C
	.type	sceSysreg_driver_63E1EE9C, @function
sceSysreg_driver_63E1EE9C: /* sceSysreg_driver 0x63e1ee9c @ 0x4808 */
	jr	$ra
	nop
	.globl	sceSysreg_driver_73EBC752
	.type	sceSysreg_driver_73EBC752, @function
sceSysreg_driver_73EBC752: /* sceSysreg_driver 0x73ebc752 @ 0x4810 */
	jr	$ra
	nop
	.globl	sceSysreg_driver_7CF05E81
	.type	sceSysreg_driver_7CF05E81, @function
sceSysreg_driver_7CF05E81: /* sceSysreg_driver 0x7cf05e81 @ 0x4818 */
	jr	$ra
	nop
	.globl	sceSysreg_driver_85B74FDA
	.type	sceSysreg_driver_85B74FDA, @function
sceSysreg_driver_85B74FDA: /* sceSysreg_driver 0x85b74fda @ 0x4820 */
	jr	$ra
	nop
	.globl	sceSysregGetFuseConfig
	.type	sceSysregGetFuseConfig, @function
sceSysregGetFuseConfig: /* sceSysreg_driver 0x8f4f4e96 @ 0x4828 */
	jr	$ra
	nop
	.globl	sceSysreg_driver_96D74557
	.type	sceSysreg_driver_96D74557, @function
sceSysreg_driver_96D74557: /* sceSysreg_driver 0x96d74557 @ 0x4830 */
	jr	$ra
	nop
	.globl	sceSysregSpiIoDisable
	.type	sceSysregSpiIoDisable, @function
sceSysregSpiIoDisable: /* sceSysreg_driver 0xa16e55f4 @ 0x4838 */
	jr	$ra
	nop
	.globl	sceSysregPllGetFrequency
	.type	sceSysregPllGetFrequency, @function
sceSysregPllGetFrequency: /* sceSysreg_driver 0xa5cc6025 @ 0x4840 */
	jr	$ra
	nop
	.globl	sceSysreg_driver_B413B041
	.type	sceSysreg_driver_B413B041, @function
sceSysreg_driver_B413B041: /* sceSysreg_driver 0xb413b041 @ 0x4848 */
	jr	$ra
	nop
	.globl	sceSysregUartIoDisable
	.type	sceSysregUartIoDisable, @function
sceSysregUartIoDisable: /* sceSysreg_driver 0xbb823481 @ 0x4850 */
	jr	$ra
	nop
	.globl	sceSysregGetTachyonVersion
	.type	sceSysregGetTachyonVersion, @function
sceSysregGetTachyonVersion: /* sceSysreg_driver 0xe2a5d1ee @ 0x4858 */
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
	.half	3
	.word	.L_exp_tbl_0
	.word	.L_exp_lib_1
	.byte	17, 0
	.half	0x4001
	.byte	5, 0
	.half	60
	.word	.L_exp_tbl_1
	.word	0x00020000
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 12
	.half	88
	.word	.L_exp_tbl_2
	.word	0x00040000

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
	.half	6
	.word	.L_imp_nid_0
	.word	sceKernelGetModel
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	5
	.word	.L_imp_nid_1
	.word	sceKernelDispatchResumeHandlers
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	3
	.word	.L_imp_nid_2
	.word	sceKernelSysEventDispatch
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
	.half	27
	.word	.L_imp_nid_5
	.word	sceKernelSetEventFlag
	.word	0
	.word	.L_imp_lib_6
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	5
	.word	.L_imp_nid_6
	.word	sceKernelDcacheWritebackInvalidateRange
	.word	.L_imp_lib_7
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	3
	.word	.L_imp_nid_7
	.word	sceKernelApplicationType
	.word	.L_imp_lib_8
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	3
	.word	.L_imp_nid_8
	.word	Kprintf
	.word	.L_imp_lib_9
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	16
	.word	.L_imp_nid_9
	.word	sceSysregGpioIoDisable
	.word	.L_imp_lib_10
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	4
	.word	.L_imp_nid_10
	.word	sceDdr_driver_00E36648
	.word	.L_imp_lib_11
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_11
	.word	sceGeEdramSetRefreshParam
	.word	.L_imp_lib_12
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_12
	.word	sceCtrlSetButtonIntercept
	.word	.L_imp_lib_13
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	6
	.word	.L_imp_nid_13
	.word	sceDisplayIsVblank
	.word	.L_imp_lib_14
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_14
	.word	sceKermit_driver_5280B410
	.word	.L_imp_lib_15
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	4
	.word	.L_imp_nid_15
	.word	sceKermitPeripheral_driver_0648E1A3

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
	.byte	13, 1
	.asciz	"scePower_Service"
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
	.asciz	"UtilsForKernel"
	.space	5
.L_imp_lib_7:
	.asciz	"InitForKernel"
	.space	6
.L_imp_lib_8:
	.asciz	"KDebugForKernel"
	.space	4
.L_imp_lib_9:
	.asciz	"sceSysreg_driver"
	.space	7
.L_imp_lib_10:
	.asciz	"sceDdr_driver"
	.space	6
.L_imp_lib_11:
	.asciz	"sceGe_driver"
	.space	7
.L_imp_lib_12:
	.asciz	"sceCtrl_driver"
	.space	5
.L_imp_lib_13:
	.asciz	"sceDisplay_driver"
	.space	6
.L_imp_lib_14:
	.asciz	"sceKermit_driver"
	.space	7
.L_imp_lib_15:
	.asciz	"sceKermitPeripheral_driver"
	.space	1
.L_exp_tbl_0:
	.word	0xd632acdb
	.word	0xadf12745
	.word	0x2f064fa6
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_start /* module_start (0xd632acdb) */
	.word	module_reboot_phase /* module_reboot_phase (0xadf12745) */
	.word	module_reboot_before /* module_reboot_before (0x2f064fa6) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"scePower"
	.space	3
.L_exp_tbl_1:
	.word	0x0074ef9b
	.word	0x0442d852
	.word	0x04b7766e
	.word	0x0afd0d8b
	.word	0x0cd21b1f
	.word	0x165ce085
	.word	0x1e490401
	.word	0x2085d15d
	.word	0x23436a4a
	.word	0x23c31ffe
	.word	0x27f3292c
	.word	0x2875994b
	.word	0x28e12023
	.word	0x2b51fe2f
	.word	0x2b7c7cf4
	.word	0x34f9c463
	.word	0x3951af53
	.word	0x442bfbac
	.word	0x469989ad
	.word	0x478fe6f5
	.word	0x483ce86b
	.word	0x545a7f3c
	.word	0x737486f2
	.word	0x78a1a796
	.word	0x7f30b3b1
	.word	0x7fa406dd
	.word	0x843fbf43
	.word	0x862ae1a6
	.word	0x87440f5e
	.word	0x8efb3fa2
	.word	0x94f5a53f
	.word	0x972ce941
	.word	0x9badb3eb
	.word	0xa4e93389
	.word	0xa85880d0
	.word	0xa9d22232
	.word	0xac32c9cc
	.word	0xb1a52c83
	.word	0xb3edd801
	.word	0xb4432bc8
	.word	0xb8d7b3fb
	.word	0xb999184c
	.word	0xbafa3df0
	.word	0xbd681969
	.word	0xca3d34c1
	.word	0xcb49f5ce
	.word	0xd3075926
	.word	0xd6d016ef
	.word	0xdb62c9cf
	.word	0xdb9d28dd
	.word	0xdfa8baf8
	.word	0xe8e4e204
	.word	0xea382a27
	.word	0xebd177d6
	.word	0xedc13fe5
	.word	0xefd3c963
	.word	0xfa97a599
	.word	0xfd18a0ff
	.word	0xfdb5bfe9
	.word	0xfee03a2f
	.word	scePower_0074EF9B /* scePower_0074EF9B (0x0074ef9b) */
	.word	scePowerRequestColdReset /* scePowerRequestColdReset (0x0442d852) */
	.word	scePowerRegisterCallback /* scePowerRegisterCallback (0x04b7766e) */
	.word	scePowerIsBatteryExist /* scePowerIsBatteryExist (0x0afd0d8b) */
	.word	scePower_0CD21B1F /* scePower_0CD21B1F (0x0cd21b1f) */
	.word	scePower_165CE085 /* scePower_165CE085 (0x165ce085) */
	.word	scePowerIsBatteryCharging /* scePowerIsBatteryCharging (0x1e490401) */
	.word	scePowerGetBatteryLifePercent /* scePowerGetBatteryLifePercent (0x2085d15d) */
	.word	scePower_23436A4A /* scePower_23436A4A (0x23436a4a) */
	.word	scePower_23C31FFE /* scePower_23C31FFE (0x23c31ffe) */
	.word	scePower_27F3292C /* scePower_27F3292C (0x27f3292c) */
	.word	scePower_2875994B /* scePower_2875994B (0x2875994b) */
	.word	scePowerGetBatteryTemp /* scePowerGetBatteryTemp (0x28e12023) */
	.word	scePower_2B51FE2F /* scePower_2B51FE2F (0x2b51fe2f) */
	.word	scePowerRequestStandby /* scePowerRequestStandby (0x2b7c7cf4) */
	.word	scePower_34F9C463 /* scePower_34F9C463 (0x34f9c463) */
	.word	scePower_3951AF53 /* scePower_3951AF53 (0x3951af53) */
	.word	scePower_442BFBAC /* scePower_442BFBAC (0x442bfbac) */
	.word	scePower_469989AD /* scePower_469989AD (0x469989ad) */
	.word	scePowerGetBusClockFrequencyInt /* scePowerGetBusClockFrequency (0x478fe6f5) */
	.word	scePowerGetBatteryVolt /* scePowerGetBatteryVolt (0x483ce86b) */
	.word	scePower_545A7F3C /* scePower_545A7F3C (0x545a7f3c) */
	.word	scePowerSetClockFrequency /* scePowerSetClockFrequency (0x737486f2) */
	.word	scePowerIsSuspendRequired /* scePowerIsSuspendRequired (0x78a1a796) */
	.word	scePowerIdleTimerEnable /* scePowerIdleTimerEnable (0x7f30b3b1) */
	.word	scePower_7FA406DD /* scePower_7FA406DD (0x7fa406dd) */
	.word	scePowerSetCpuClockFrequency /* scePowerSetCpuClockFrequency (0x843fbf43) */
	.word	scePowerGetBatteryElec /* scePowerGetBatteryElec (0x862ae1a6) */
	.word	scePowerIsPowerOnline /* scePowerIsPowerOnline (0x87440f5e) */
	.word	scePowerGetBatteryLifeTime /* scePowerGetBatteryLifeTime (0x8efb3fa2) */
	.word	scePowerGetBatteryRemainCapacity /* scePowerGetBatteryRemainCapacity (0x94f5a53f) */
	.word	scePowerIdleTimerDisable /* scePowerIdleTimerDisable (0x972ce941) */
	.word	scePowerGetBusClockFrequencyFloat /* scePowerGetBusClockFrequencyFloat (0x9badb3eb) */
	.word	scePower_A4E93389 /* scePower_A4E93389 (0xa4e93389) */
	.word	scePower_A85880D0 /* scePower_A85880D0 (0xa85880d0) */
	.word	scePower_A9D22232 /* scePower_A9D22232 (0xa9d22232) */
	.word	scePowerRequestSuspend /* scePowerRequestSuspend (0xac32c9cc) */
	.word	scePowerGetCpuClockFrequencyFloat /* scePowerGetCpuClockFrequencyFloat (0xb1a52c83) */
	.word	scePower_B3EDD801 /* scePower_B3EDD801 (0xb3edd801) */
	.word	scePowerGetBatteryChargingStatus /* scePowerGetBatteryChargingStatus (0xb4432bc8) */
	.word	scePowerSetBusClockFrequency /* scePowerSetBusClockFrequency (0xb8d7b3fb) */
	.word	scePower_B999184C /* scePower_B999184C (0xb999184c) */
	.word	scePower_BAFA3DF0 /* scePower_BAFA3DF0 (0xbafa3df0) */
	.word	scePowerGetBusClockFrequencyInt /* scePowerGetBusClockFrequencyInt (0xbd681969) */
	.word	scePowerUnlock /* scePowerUnlock (0xca3d34c1) */
	.word	scePower_CB49F5CE /* scePower_CB49F5CE (0xcb49f5ce) */
	.word	scePowerIsLowBattery /* scePowerIsLowBattery (0xd3075926) */
	.word	scePowerLock /* scePowerLock (0xd6d016ef) */
	.word	scePower_DB62C9CF /* scePower_DB62C9CF (0xdb62c9cf) */
	.word	scePowerUnregisterCallback /* scePower_DB9D28DD (0xdb9d28dd) */
	.word	scePowerUnregisterCallback /* scePowerUnregisterCallback (0xdfa8baf8) */
	.word	scePower_E8E4E204 /* scePower_E8E4E204 (0xe8e4e204) */
	.word	scePower_EA382A27 /* scePower_EA382A27 (0xea382a27) */
	.word	scePower_EBD177D6 /* scePower_EBD177D6 (0xebd177d6) */
	.word	scePowerGetIdleTimer /* scePowerGetIdleTimer (0xedc13fe5) */
	.word	scePowerTick /* scePowerTick (0xefd3c963) */
	.word	scePower_FA97A599 /* scePower_FA97A599 (0xfa97a599) */
	.word	scePowerGetBatteryFullCapacity /* scePowerGetBatteryFullCapacity (0xfd18a0ff) */
	.word	scePowerGetCpuClockFrequencyInt /* scePowerGetCpuClockFrequencyInt (0xfdb5bfe9) */
	.word	scePowerGetCpuClockFrequencyInt /* scePowerGetCpuClockFrequency (0xfee03a2f) */
	.word	0x00050000
	.word	0x000f0007
	.word	0x00150011
	.word	0x00160016
	.word	0x001e001a
	.word	0x00250021
	.word	0x002e002c
	.word	0x00380033
.L_exp_lib_2:
	.asciz	"scePower_driver"
	.align	2
.L_exp_tbl_2:
	.word	0x003b1e03
	.word	0x018ab235
	.word	0x04711dfb
	.word	0x0655d7c3
	.word	0x071160b1
	.word	0x0da940d2
	.word	0x0efee60e
	.word	0x100a77a1
	.word	0x10ce273f
	.word	0x114b75ab
	.word	0x12f8302d
	.word	0x13d7cce4
	.word	0x16f965c9
	.word	0x17eea285
	.word	0x1ba2fcae
	.word	0x1e3b1fae
	.word	0x1ec2d4e4
	.word	0x1ff8da3b
	.word	0x23bddd8b
	.word	0x2509ff3b
	.word	0x2638ef48
	.word	0x29e23416
	.word	0x315b8cb6
	.word	0x31aea94c
	.word	0x3300d85a
	.word	0x40870dac
	.word	0x41adff48
	.word	0x469989ad
	.word	0x4e32e9b8
	.word	0x53808cbb
	.word	0x566b8353
	.word	0x5978b1c2
	.word	0x5c1333b7
	.word	0x5f5006d2
	.word	0x67492c52
	.word	0x67bd889b
	.word	0x693f6cf0
	.word	0x6cf50928
	.word	0x70f42744
	.word	0x72d1b53a
	.word	0x73785d34
	.word	0x75906f9a
	.word	0x766cd857
	.word	0x7a9ea6de
	.word	0x7b908caa
	.word	0x7eaa4247
	.word	0x8432901e
	.word	0x872f4ece
	.word	0x88c79735
	.word	0x8c6befd9
	.word	0x8c873aa7
	.word	0x9127e5b2
	.word	0x961a06a5
	.word	0x993b8c4a
	.word	0x9b44cfd9
	.word	0x9ce06934
	.word	0x9daf25a0
	.word	0xa641cf3f
	.word	0xa882aeb7
	.word	0xad5bb433
	.word	0xb45c9066
	.word	0xb7000c75
	.word	0xb71a8b2f
	.word	0xba566cd0
	.word	0xba8cbcbf
	.word	0xbada8332
	.word	0xc3024fe6
	.word	0xc463e7f2
	.word	0xc520f5dc
	.word	0xc730f432
	.word	0xc79f9157
	.word	0xce2032cd
	.word	0xd13377f7
	.word	0xd1fff513
	.word	0xd79b0122
	.word	0xd7dd9d38
	.word	0xdb62c9cf
	.word	0xdc4395e2
	.word	0xdf336cde
	.word	0xdf904cde
	.word	0xe11cdffa
	.word	0xe52b4362
	.word	0xebd177d6
	.word	0xeefb2acf
	.word	0xef751b4a
	.word	0xf7de0e81
	.word	0xfa651ce1
	.word	0xfdb5bfe9
	.word	0x07fc869e
	.word	0x18dfa43a
	.word	0x2a28bb48
	.word	0x2eda246c
	.word	0x3080d4c0
	.word	0x33399b15
	.word	0x4a03da50
	.word	0x8b317890
	.word	0x93e16800
	.word	0x94fa3195
	.word	0xc7998fa4
	.word	0xe59ca666
	.word	scePowerGetBatteryFullCapacity /* scePower_driver_003B1E03 (0x003b1e03) */
	.word	scePower_driver_018AB235 /* scePower_driver_018AB235 (0x018ab235) */
	.word	scePowerGetBusClockFrequencyInt /* scePower_driver_04711DFB (0x04711dfb) */
	.word	scePower_driver_0655D7C3 /* scePower_driver_0655D7C3 (0x0655d7c3) */
	.word	scePower_driver_071160B1 /* scePower_driver_071160B1 (0x071160b1) */
	.word	scePower_27F3292C /* scePower_driver_0DA940D2 (0x0da940d2) */
	.word	scePowerTick /* scePower_driver_0EFEE60E (0x0efee60e) */
	.word	scePower_driver_100A77A1 /* scePower_driver_100A77A1 (0x100a77a1) */
	.word	scePower_driver_10CE273F /* scePower_driver_10CE273F (0x10ce273f) */
	.word	scePower_driver_114B75AB /* scePower_driver_114B75AB (0x114b75ab) */
	.word	scePower_driver_12F8302D /* scePower_driver_12F8302D (0x12f8302d) */
	.word	scePower_driver_13D7CCE4 /* scePower_driver_13D7CCE4 (0x13d7cce4) */
	.word	scePower_driver_16F965C9 /* scePower_driver_16F965C9 (0x16f965c9) */
	.word	scePower_BAFA3DF0 /* scePower_driver_17EEA285 (0x17eea285) */
	.word	scePower_driver_1BA2FCAE /* scePower_driver_1BA2FCAE (0x1ba2fcae) */
	.word	scePowerIdleTimerEnable /* scePower_driver_1E3B1FAE (0x1e3b1fae) */
	.word	scePower_driver_1EC2D4E4 /* scePower_driver_1EC2D4E4 (0x1ec2d4e4) */
	.word	scePowerGetBusClockFrequencyFloat /* scePower_driver_1FF8DA3B (0x1ff8da3b) */
	.word	scePower_driver_23BDDD8B /* scePower_driver_23BDDD8B (0x23bddd8b) */
	.word	scePower_442BFBAC /* scePower_driver_2509FF3B (0x2509ff3b) */
	.word	scePower_driver_2638EF48 /* scePower_driver_2638EF48 (0x2638ef48) */
	.word	scePower_A9D22232 /* scePower_driver_29E23416 (0x29e23416) */
	.word	scePowerUnregisterCallback /* scePower_driver_315B8CB6 (0x315b8cb6) */
	.word	scePowerGetBatteryLifePercent /* scePower_driver_31AEA94C (0x31aea94c) */
	.word	scePower_3951AF53 /* scePower_driver_3300D85A (0x3300d85a) */
	.word	scePowerGetBatteryTemp /* scePower_driver_40870DAC (0x40870dac) */
	.word	scePowerGetBatteryRemainCapacity /* scePower_driver_41ADFF48 (0x41adff48) */
	.word	scePower_469989AD /* scePower_driver_469989AD (0x469989ad) */
	.word	scePower_driver_4E32E9B8 /* scePower_driver_4E32E9B8 (0x4e32e9b8) */
	.word	scePowerSetCpuClockFrequency /* scePower_driver_53808CBB (0x53808cbb) */
	.word	scePower_7FA406DD /* scePower_driver_566B8353 (0x566b8353) */
	.word	scePower_B3EDD801 /* scePower_driver_5978B1C2 (0x5978b1c2) */
	.word	scePowerRequestSuspend /* scePower_driver_5C1333B7 (0x5c1333b7) */
	.word	scePower_driver_5F5006D2 /* scePower_driver_5F5006D2 (0x5f5006d2) */
	.word	scePowerGetBatteryChargingStatus /* scePower_driver_67492C52 (0x67492c52) */
	.word	scePower_34F9C463 /* scePower_driver_67BD889B (0x67bd889b) */
	.word	scePower_A85880D0 /* scePower_driver_693F6CF0 (0x693f6cf0) */
	.word	scePower_driver_6CF50928 /* scePower_driver_6CF50928 (0x6cf50928) */
	.word	scePower_23C31FFE /* scePower_driver_70F42744 (0x70f42744) */
	.word	scePower_driver_72D1B53A /* scePower_driver_72D1B53A (0x72d1b53a) */
	.word	scePower_0CD21B1F /* scePower_driver_73785D34 (0x73785d34) */
	.word	scePower_driver_75906F9A /* scePower_driver_75906F9A (0x75906f9a) */
	.word	scePowerRegisterCallback /* scePower_driver_766CD857 (0x766cd857) */
	.word	scePowerIsBatteryCharging /* scePower_driver_7A9EA6DE (0x7a9ea6de) */
	.word	scePower_B999184C /* scePower_driver_7B908CAA (0x7b908caa) */
	.word	scePower_driver_7EAA4247 /* scePower_driver_7EAA4247 (0x7eaa4247) */
	.word	scePower_CB49F5CE /* scePower_driver_8432901E (0x8432901e) */
	.word	scePowerIsPowerOnline /* scePower_driver_872F4ECE (0x872f4ece) */
	.word	scePowerIsSuspendRequired /* scePower_driver_88C79735 (0x88c79735) */
	.word	scePower_driver_8C6BEFD9 /* scePower_driver_8C6BEFD9 (0x8c6befd9) */
	.word	scePowerIsBatteryExist /* scePower_driver_8C873AA7 (0x8c873aa7) */
	.word	scePower_driver_9127E5B2 /* scePower_driver_9127E5B2 (0x9127e5b2) */
	.word	scePowerIdleTimerDisable /* scePower_driver_961A06A5 (0x961a06a5) */
	.word	scePowerGetBatteryElec /* scePower_driver_993B8C4A (0x993b8c4a) */
	.word	scePowerRequestStandby /* scePower_driver_9B44CFD9 (0x9b44cfd9) */
	.word	scePower_driver_9CE06934 /* scePower_driver_9CE06934 (0x9ce06934) */
	.word	scePowerRequestColdReset /* scePower_driver_9DAF25A0 (0x9daf25a0) */
	.word	scePower_E8E4E204 /* scePower_driver_A641CF3F (0xa641cf3f) */
	.word	scePower_FA97A599 /* scePower_driver_A882AEB7 (0xa882aeb7) */
	.word	scePower_driver_AD5BB433 /* scePower_driver_AD5BB433 (0xad5bb433) */
	.word	scePower_0074EF9B /* scePower_driver_B45C9066 (0xb45c9066) */
	.word	scePower_driver_B7000C75 /* scePower_driver_B7000C75 (0xb7000c75) */
	.word	scePowerSetBusClockFrequency /* scePower_driver_B71A8B2F (0xb71a8b2f) */
	.word	scePower_driver_BA566CD0 /* scePower_driver_BA566CD0 (0xba566cd0) */
	.word	scePower_EA382A27 /* scePower_driver_BA8CBCBF (0xba8cbcbf) */
	.word	scePower_driver_BADA8332 /* scePower_driver_BADA8332 (0xbada8332) */
	.word	scePower_driver_C3024FE6 /* scePower_driver_C3024FE6 (0xc3024fe6) */
	.word	scePower_driver_C463E7F2 /* scePower_driver_C463E7F2 (0xc463e7f2) */
	.word	scePower_driver_C520F5DC /* scePower_driver_C520F5DC (0xc520f5dc) */
	.word	scePower_23436A4A /* scePower_driver_C730F432 (0xc730f432) */
	.word	scePowerGetBatteryLifeTime /* scePower_driver_C79F9157 (0xc79f9157) */
	.word	scePower_2B51FE2F /* scePower_driver_CE2032CD (0xce2032cd) */
	.word	scePower_driver_D13377F7 /* scePower_driver_D13377F7 (0xd13377f7) */
	.word	scePower_2875994B /* scePower_driver_D1FFF513 (0xd1fff513) */
	.word	scePower_driver_D79B0122 /* scePower_driver_D79B0122 (0xd79b0122) */
	.word	scePower_driver_D7DD9D38 /* scePower_driver_D7DD9D38 (0xd7dd9d38) */
	.word	scePower_DB62C9CF /* scePower_driver_DB62C9CF (0xdb62c9cf) */
	.word	scePowerGetCpuClockFrequencyFloat /* scePower_driver_DC4395E2 (0xdc4395e2) */
	.word	scePowerGetIdleTimer /* scePower_driver_DF336CDE (0xdf336cde) */
	.word	scePower_driver_DF904CDE /* scePower_driver_DF904CDE (0xdf904cde) */
	.word	scePower_165CE085 /* scePower_driver_E11CDFFA (0xe11cdffa) */
	.word	scePower_driver_E52B4362 /* scePower_driver_E52B4362 (0xe52b4362) */
	.word	scePower_EBD177D6 /* scePower_driver_EBD177D6 (0xebd177d6) */
	.word	scePower_driver_EEFB2ACF /* scePower_driver_EEFB2ACF (0xeefb2acf) */
	.word	scePower_driver_EF751B4A /* scePower_driver_EF751B4A (0xef751b4a) */
	.word	scePowerGetBatteryVolt /* scePower_driver_F7DE0E81 (0xf7de0e81) */
	.word	scePowerIsLowBattery /* scePower_driver_FA651CE1 (0xfa651ce1) */
	.word	scePowerGetCpuClockFrequencyInt /* scePowerGetCpuClockFrequencyInt (0xfdb5bfe9) */
	.word	scePower_driver_var_07FC869E /* scePower_driver_var_07FC869E (0x07fc869e) */
	.word	scePower_driver_var_18DFA43A /* scePower_driver_var_18DFA43A (0x18dfa43a) */
	.word	scePower_driver_var_2A28BB48 /* scePower_driver_var_2A28BB48 (0x2a28bb48) */
	.word	scePower_driver_var_2EDA246C /* scePower_driver_var_2EDA246C (0x2eda246c) */
	.word	scePower_driver_var_3080D4C0 /* scePower_driver_var_3080D4C0 (0x3080d4c0) */
	.word	scePower_driver_var_33399B15 /* scePower_driver_var_33399B15 (0x33399b15) */
	.word	scePower_driver_var_4A03DA50 /* scePower_driver_var_4A03DA50 (0x4a03da50) */
	.word	scePower_driver_var_8B317890 /* scePower_driver_var_8B317890 (0x8b317890) */
	.word	scePower_driver_var_93E16800 /* scePower_driver_var_93E16800 (0x93e16800) */
	.word	scePower_driver_var_94FA3195 /* scePower_driver_var_94FA3195 (0x94fa3195) */
	.word	scePower_driver_var_C7998FA4 /* scePower_driver_var_C7998FA4 (0xc7998fa4) */
	.word	scePower_driver_var_E59CA666 /* scePower_driver_var_E59CA666 (0xe59ca666) */
	.word	0x00020000
	.word	0x00050005
	.word	0x000c0007
	.word	0x000f000e
	.word	0x00130012
	.word	0x00160015
	.word	0x00190016
	.word	0x00190019
	.word	0x001b0019
	.word	0x001c001c
	.word	0x001e001d
	.word	0x0020001f
	.word	0x00220022
	.word	0x00250024
	.word	0x00290026
	.word	0x002d002b
	.word	0x002e002e
	.word	0x00310030
	.word	0x00340033
	.word	0x00370035
	.word	0x00390039
	.word	0x003b003a
	.word	0x003c003c
	.word	0x0042003f
	.word	0x00430042
	.word	0x00470047
	.word	0x004a0048
	.word	0x004d004c
	.word	0x00510050
	.word	0x00530052
	.word	0x00550055
	.word	0x00570056

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x07c586a1
	.word	0x83b5226d
	.word	0xb4f00cb5
	.word	0xc4eeaf20
	.word	0xc90b0992
	.word	0xef29061c
.L_imp_nid_1:
	.word	0x0ab0c6f3
	.word	0x67b59042
	.word	0x8f58b1ec
	.word	0xb2c9640b
	.word	0xbde686cd
.L_imp_nid_2:
	.word	0x36331294
	.word	0xcd9e4bb5
	.word	0xd7d3fdcd
.L_imp_nid_3:
	.word	0x092968f4
	.word	0x3b84732d
	.word	0x5f10d406
	.word	0xd61e6961
	.word	0xfb8e22ec
	.word	0xffa8b183
.L_imp_nid_4:
	.word	0x10f3bb61
.L_imp_nid_5:
	.word	0x1fb15a32
	.word	0x278c0df5
	.word	0x28b6489c
	.word	0x293b45b8
	.word	0x2a3d44ff
	.word	0x30fd48f0
	.word	0x3b183e26
	.word	0x3f53e640
	.word	0x402fcf22
	.word	0x446d8de6
	.word	0x4e3a1105
	.word	0x55c20a00
	.word	0x57cf62dd
	.word	0x58b1f937
	.word	0x6b30100f
	.word	0x7e65b999
	.word	0x812346e4
	.word	0x82bc5777
	.word	0xb011b11f
	.word	0xb7d098c6
	.word	0xc11ba8c4
	.word	0xceadeb47
	.word	0xd6da4ba1
	.word	0xef9e4c70
	.word	0xf475845d
	.word	0xf6427665
	.word	0xf8170fbe
.L_imp_nid_6:
	.word	0x34b9fa9e
	.word	0x39ffb756
	.word	0x79d1c3fa
	.word	0x840259f1
	.word	0xa6b0a6b8
.L_imp_nid_7:
	.word	0x7233b5bc
	.word	0x9f9ae99c
	.word	0xd83a9bd7
.L_imp_nid_8:
	.word	0x84f370bc
	.word	0x86010fcb
	.word	0xe892d9a1
.L_imp_nid_9:
	.word	0x15dc34bc
	.word	0x187f651d
	.word	0x2458b6ac
	.word	0x3f6f2cc7
	.word	0x4f46eede
	.word	0x63e1ee9c
	.word	0x73ebc752
	.word	0x7cf05e81
	.word	0x85b74fda
	.word	0x8f4f4e96
	.word	0x96d74557
	.word	0xa16e55f4
	.word	0xa5cc6025
	.word	0xb413b041
	.word	0xbb823481
	.word	0xe2a5d1ee
.L_imp_nid_10:
	.word	0x00e36648
	.word	0x0baae4c5
	.word	0xcf07b9e2
	.word	0xe0a39d3e
.L_imp_nid_11:
	.word	0x2539d845
.L_imp_nid_12:
	.word	0xf8346777
.L_imp_nid_13:
	.word	0x572d7804
	.word	0xb0942511
	.word	0xce8a328e
	.word	0xd81a8ff9
	.word	0xec80d435
	.word	0xf84d16cc
.L_imp_nid_14:
	.word	0x5280b410
.L_imp_nid_15:
	.word	0x0648e1a3
	.word	0x15f5c107
	.word	0x65d98a21
	.word	0x6d6aa215

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
