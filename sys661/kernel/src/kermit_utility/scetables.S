/* Generated PSP module import/export tables for sceUtility_Driver */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelBootFrom
	.type	sceKernelBootFrom, @function
sceKernelBootFrom: /* InitForKernel 0x27932388 @ 0x9314 */
	jr	$ra
	nop
	.globl	sceKernelApplicationType
	.type	sceKernelApplicationType, @function
sceKernelApplicationType: /* InitForKernel 0x7233b5bc @ 0x931c */
	jr	$ra
	nop
	.globl	sceKernelInitDiscImage
	.type	sceKernelInitDiscImage, @function
sceKernelInitDiscImage: /* InitForKernel 0xa18a4a8b @ 0x9324 */
	jr	$ra
	nop
	.globl	sceKernelCpuSuspendIntr
	.type	sceKernelCpuSuspendIntr, @function
sceKernelCpuSuspendIntr: /* InterruptManagerForKernel 0x092968f4 @ 0x932c */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntr
	.type	sceKernelCpuResumeIntr, @function
sceKernelCpuResumeIntr: /* InterruptManagerForKernel 0x5f10d406 @ 0x9334 */
	jr	$ra
	nop
	.globl	sceIoDevctl
	.type	sceIoDevctl, @function
sceIoDevctl: /* IoFileMgrForKernel 0x54f5fb11 @ 0x933c */
	jr	$ra
	nop
	.globl	sceKernelIsToolMode
	.type	sceKernelIsToolMode, @function
sceKernelIsToolMode: /* KDebugForKernel 0x47570ac5 @ 0x9344 */
	jr	$ra
	nop
	.globl	Kprintf
	.type	Kprintf, @function
Kprintf: /* KDebugForKernel 0x84f370bc @ 0x934c */
	jr	$ra
	nop
	.globl	sceKernelDipsw
	.type	sceKernelDipsw, @function
sceKernelDipsw: /* KDebugForKernel 0x86010fcb @ 0x9354 */
	jr	$ra
	nop
	.globl	sceKernelIsDevelopmentToolMode
	.type	sceKernelIsDevelopmentToolMode, @function
sceKernelIsDevelopmentToolMode: /* KDebugForKernel 0xacf427dc @ 0x935c */
	jr	$ra
	nop
	.globl	sceKernelUnloadModule
	.type	sceKernelUnloadModule, @function
sceKernelUnloadModule: /* ModuleMgrForKernel 0x387e3ca9 @ 0x9364 */
	jr	$ra
	nop
	.globl	sceKernelStartModule
	.type	sceKernelStartModule, @function
sceKernelStartModule: /* ModuleMgrForKernel 0x3ff74df1 @ 0x936c */
	jr	$ra
	nop
	.globl	sceKernelLoadModuleForKernel
	.type	sceKernelLoadModuleForKernel, @function
sceKernelLoadModuleForKernel: /* ModuleMgrForKernel 0x939e4270 @ 0x9374 */
	jr	$ra
	nop
	.globl	sceKernelLoadModuleToBlock
	.type	sceKernelLoadModuleToBlock, @function
sceKernelLoadModuleToBlock: /* ModuleMgrForKernel 0xd4ee2d26 @ 0x937c */
	jr	$ra
	nop
	.globl	sceKernelSearchModuleByName
	.type	sceKernelSearchModuleByName, @function
sceKernelSearchModuleByName: /* ModuleMgrForKernel 0xd86dd11b @ 0x9384 */
	jr	$ra
	nop
	.globl	sceKernelStopModule
	.type	sceKernelStopModule, @function
sceKernelStopModule: /* ModuleMgrForKernel 0xe5d6087b @ 0x938c */
	jr	$ra
	nop
	.globl	sceKernelGetModel
	.type	sceKernelGetModel, @function
sceKernelGetModel: /* SysMemForKernel 0x07c586a1 @ 0x9394 */
	jr	$ra
	nop
	.globl	sceKernelMemcpy
	.type	sceKernelMemcpy, @function
sceKernelMemcpy: /* SysMemForKernel 0x181065ab @ 0x939c */
	jr	$ra
	nop
	.globl	sceKernelJointMemoryBlock
	.type	sceKernelJointMemoryBlock, @function
sceKernelJointMemoryBlock: /* SysMemForKernel 0x1ab50974 @ 0x93a4 */
	jr	$ra
	nop
	.globl	sceKernelMemset32
	.type	sceKernelMemset32, @function
sceKernelMemset32: /* SysMemForKernel 0x22a114dc @ 0x93ac */
	jr	$ra
	nop
	.globl	sceKernelAllocPartitionMemory
	.type	sceKernelAllocPartitionMemory, @function
sceKernelAllocPartitionMemory: /* SysMemForKernel 0x7158ce7e @ 0x93b4 */
	jr	$ra
	nop
	.globl	sceKernelMemset
	.type	sceKernelMemset, @function
sceKernelMemset: /* SysMemForKernel 0x8ae776af @ 0x93bc */
	jr	$ra
	nop
	.globl	sceKernelGetCompiledSdkVersion
	.type	sceKernelGetCompiledSdkVersion, @function
sceKernelGetCompiledSdkVersion: /* SysMemForKernel 0xb4f00cb5 @ 0x93c4 */
	jr	$ra
	nop
	.globl	sceKernelFreePartitionMemory
	.type	sceKernelFreePartitionMemory, @function
sceKernelFreePartitionMemory: /* SysMemForKernel 0xc1a26c6f @ 0x93cc */
	jr	$ra
	nop
	.globl	sceKernelQueryMemoryBlockInfo
	.type	sceKernelQueryMemoryBlockInfo, @function
sceKernelQueryMemoryBlockInfo: /* SysMemForKernel 0xe860be8f @ 0x93d4 */
	jr	$ra
	nop
	.globl	sceKernelGetGameInfo
	.type	sceKernelGetGameInfo, @function
sceKernelGetGameInfo: /* SysMemForKernel 0xef29061c @ 0x93dc */
	jr	$ra
	nop
	.globl	memset
	.type	memset, @function
memset: /* SysclibForKernel 0x10f3bb61 @ 0x93e4 */
	jr	$ra
	nop
	.globl	strlen
	.type	strlen, @function
strlen: /* SysclibForKernel 0x52df196c @ 0x93ec */
	jr	$ra
	nop
	.globl	strncpy
	.type	strncpy, @function
strncpy: /* SysclibForKernel 0xb49a7697 @ 0x93f4 */
	jr	$ra
	nop
	.globl	strcmp
	.type	strcmp, @function
strcmp: /* SysclibForKernel 0xc0ab8932 @ 0x93fc */
	jr	$ra
	nop
	.globl	snprintf
	.type	snprintf, @function
snprintf: /* SysclibForKernel 0xc2145e80 @ 0x9404 */
	jr	$ra
	nop
	.globl	sceKernelSetEventFlag
	.type	sceKernelSetEventFlag, @function
sceKernelSetEventFlag: /* ThreadManForKernel 0x1fb15a32 @ 0x940c */
	jr	$ra
	nop
	.globl	sceKernelWaitThreadEnd
	.type	sceKernelWaitThreadEnd, @function
sceKernelWaitThreadEnd: /* ThreadManForKernel 0x278c0df5 @ 0x9414 */
	jr	$ra
	nop
	.globl	sceKernelGetThreadId
	.type	sceKernelGetThreadId, @function
sceKernelGetThreadId: /* ThreadManForKernel 0x293b45b8 @ 0x941c */
	jr	$ra
	nop
	.globl	sceKernelGetSystemTimeLow
	.type	sceKernelGetSystemTimeLow, @function
sceKernelGetSystemTimeLow: /* ThreadManForKernel 0x369ed59d @ 0x9424 */
	jr	$ra
	nop
	.globl	sceKernelWaitEventFlag
	.type	sceKernelWaitEventFlag, @function
sceKernelWaitEventFlag: /* ThreadManForKernel 0x402fcf22 @ 0x942c */
	jr	$ra
	nop
	.globl	sceKernelCreateThread
	.type	sceKernelCreateThread, @function
sceKernelCreateThread: /* ThreadManForKernel 0x446d8de6 @ 0x9434 */
	jr	$ra
	nop
	.globl	sceKernelCreateEventFlag
	.type	sceKernelCreateEventFlag, @function
sceKernelCreateEventFlag: /* ThreadManForKernel 0x55c20a00 @ 0x943c */
	jr	$ra
	nop
	.globl	sceKernelUnlockMutex
	.type	sceKernelUnlockMutex, @function
sceKernelUnlockMutex: /* ThreadManForKernel 0x6b30100f @ 0x9444 */
	jr	$ra
	nop
	.globl	sceKernelExitDeleteThread
	.type	sceKernelExitDeleteThread, @function
sceKernelExitDeleteThread: /* ThreadManForKernel 0x809ce29b @ 0x944c */
	jr	$ra
	nop
	.globl	sceKernelClearEventFlag
	.type	sceKernelClearEventFlag, @function
sceKernelClearEventFlag: /* ThreadManForKernel 0x812346e4 @ 0x9454 */
	jr	$ra
	nop
	.globl	sceKernelDeleteThread
	.type	sceKernelDeleteThread, @function
sceKernelDeleteThread: /* ThreadManForKernel 0x9fa03cd3 @ 0x945c */
	jr	$ra
	nop
	.globl	sceKernelLockMutex
	.type	sceKernelLockMutex, @function
sceKernelLockMutex: /* ThreadManForKernel 0xb011b11f @ 0x9464 */
	jr	$ra
	nop
	.globl	sceKernelCreateMutex
	.type	sceKernelCreateMutex, @function
sceKernelCreateMutex: /* ThreadManForKernel 0xb7d098c6 @ 0x946c */
	jr	$ra
	nop
	.globl	sceKernelExtendKernelStack
	.type	sceKernelExtendKernelStack, @function
sceKernelExtendKernelStack: /* ThreadManForKernel 0xbc31c1b9 @ 0x9474 */
	jr	$ra
	nop
	.globl	sceKernelDelayThread
	.type	sceKernelDelayThread, @function
sceKernelDelayThread: /* ThreadManForKernel 0xceadeb47 @ 0x947c */
	jr	$ra
	nop
	.globl	sceKernelDeleteEventFlag
	.type	sceKernelDeleteEventFlag, @function
sceKernelDeleteEventFlag: /* ThreadManForKernel 0xef9e4c70 @ 0x9484 */
	jr	$ra
	nop
	.globl	sceKernelStartThread
	.type	sceKernelStartThread, @function
sceKernelStartThread: /* ThreadManForKernel 0xf475845d @ 0x948c */
	jr	$ra
	nop
	.globl	sceKernelDcacheInvalidateRange
	.type	sceKernelDcacheInvalidateRange, @function
sceKernelDcacheInvalidateRange: /* UtilsForKernel 0xbfa98062 @ 0x9494 */
	jr	$ra
	nop
	.globl	sceCtrlPeekBufferPositive
	.type	sceCtrlPeekBufferPositive, @function
sceCtrlPeekBufferPositive: /* sceCtrl_driver 0x2ba616af @ 0x949c */
	jr	$ra
	nop
	.globl	sceCtrlSetSamplingMode
	.type	sceCtrlSetSamplingMode, @function
sceCtrlSetSamplingMode: /* sceCtrl_driver 0xf6e94ea3 @ 0x94a4 */
	jr	$ra
	nop
	.globl	sceCtrlGetSamplingMode
	.type	sceCtrlGetSamplingMode, @function
sceCtrlGetSamplingMode: /* sceCtrl_driver 0xf8ec18bd @ 0x94ac */
	jr	$ra
	nop
	.globl	sceKermitMemory_driver_90B662D0
	.type	sceKermitMemory_driver_90B662D0, @function
sceKermitMemory_driver_90B662D0: /* sceKermitMemory_driver 0x90b662d0 @ 0x94b4 */
	jr	$ra
	nop
	.globl	sceKermitMemory_driver_AAF047AC
	.type	sceKermitMemory_driver_AAF047AC, @function
sceKermitMemory_driver_AAF047AC: /* sceKermitMemory_driver 0xaaf047ac @ 0x94bc */
	jr	$ra
	nop
	.globl	sceKermitPeripheral_driver_61BEA1D2
	.type	sceKermitPeripheral_driver_61BEA1D2, @function
sceKermitPeripheral_driver_61BEA1D2: /* sceKermitPeripheral_driver 0x61bea1d2 @ 0x94c4 */
	jr	$ra
	nop
	.globl	sceKermit_driver_36666181
	.type	sceKermit_driver_36666181, @function
sceKermit_driver_36666181: /* sceKermit_driver 0x36666181 @ 0x94cc */
	jr	$ra
	nop
	.globl	sceMeBootStart
	.type	sceMeBootStart, @function
sceMeBootStart: /* sceMeCore_driver 0x5dff5c50 @ 0x94d4 */
	jr	$ra
	nop
	.globl	scePower_driver_315B8CB6
	.type	scePower_driver_315B8CB6, @function
scePower_driver_315B8CB6: /* scePower_driver 0x315b8cb6 @ 0x94dc */
	jr	$ra
	nop
	.globl	scePower_driver_766CD857
	.type	scePower_driver_766CD857, @function
scePower_driver_766CD857: /* scePower_driver 0x766cd857 @ 0x94e4 */
	jr	$ra
	nop
	.globl	sceNpDrmSetDebugMode
	.type	sceNpDrmSetDebugMode, @function
sceNpDrmSetDebugMode: /* scePspNpDrm_driver 0x2d88879a @ 0x94ec */
	jr	$ra
	nop
	.globl	sceRegGetKeyValueByName
	.type	sceRegGetKeyValueByName, @function
sceRegGetKeyValueByName: /* sceReg_driver 0x38415b9f @ 0x94f4 */
	jr	$ra
	nop
	.globl	sceRegCreateKey
	.type	sceRegCreateKey, @function
sceRegCreateKey: /* sceReg_driver 0x3b6ca1e6 @ 0x94fc */
	jr	$ra
	nop
	.globl	sceRegSetKeyValue
	.type	sceRegSetKeyValue, @function
sceRegSetKeyValue: /* sceReg_driver 0x49c70163 @ 0x9504 */
	jr	$ra
	nop
	.globl	sceRegCloseRegistry
	.type	sceRegCloseRegistry, @function
sceRegCloseRegistry: /* sceReg_driver 0x49d77d65 @ 0x950c */
	jr	$ra
	nop
	.globl	sceRegOpenCategory
	.type	sceRegOpenCategory, @function
sceRegOpenCategory: /* sceReg_driver 0x4f471457 @ 0x9514 */
	jr	$ra
	nop
	.globl	sceRegFlushRegistry
	.type	sceRegFlushRegistry, @function
sceRegFlushRegistry: /* sceReg_driver 0x5fd4764a @ 0x951c */
	jr	$ra
	nop
	.globl	sceRegRemoveCategory
	.type	sceRegRemoveCategory, @function
sceRegRemoveCategory: /* sceReg_driver 0x61db9d06 @ 0x9524 */
	jr	$ra
	nop
	.globl	sceRegGetKeyInfo
	.type	sceRegGetKeyInfo, @function
sceRegGetKeyInfo: /* sceReg_driver 0x9980519f @ 0x952c */
	jr	$ra
	nop
	.globl	sceRegFlushCategory
	.type	sceRegFlushCategory, @function
sceRegFlushCategory: /* sceReg_driver 0xd743a608 @ 0x9534 */
	jr	$ra
	nop
	.globl	sceRegOpenRegistry
	.type	sceRegOpenRegistry, @function
sceRegOpenRegistry: /* sceReg_driver 0xdba46704 @ 0x953c */
	jr	$ra
	nop
	.globl	sceRegGetKeyInfoByName
	.type	sceRegGetKeyInfoByName, @function
sceRegGetKeyInfoByName: /* sceReg_driver 0xf2619407 @ 0x9544 */
	jr	$ra
	nop
	.globl	sceRegGetKeyValue
	.type	sceRegGetKeyValue, @function
sceRegGetKeyValue: /* sceReg_driver 0xf4a3e396 @ 0x954c */
	jr	$ra
	nop
	.globl	sceRegCloseCategory
	.type	sceRegCloseCategory, @function
sceRegCloseCategory: /* sceReg_driver 0xfc742751 @ 0x9554 */
	jr	$ra
	nop
	.globl	sceKernelPowerUnlockForUser
	.type	sceKernelPowerUnlockForUser, @function
sceKernelPowerUnlockForUser: /* sceSuspendForKernel 0x3aee7261 @ 0x955c */
	jr	$ra
	nop
	.globl	sceKernelVolatileMemLock
	.type	sceKernelVolatileMemLock, @function
sceKernelVolatileMemLock: /* sceSuspendForKernel 0x3e0271d3 @ 0x9564 */
	jr	$ra
	nop
	.globl	sceKernelVolatileMemUnlock
	.type	sceKernelVolatileMemUnlock, @function
sceKernelVolatileMemUnlock: /* sceSuspendForKernel 0xa569e425 @ 0x956c */
	jr	$ra
	nop
	.globl	sceKernelPowerLockForUser
	.type	sceKernelPowerLockForUser, @function
sceKernelPowerLockForUser: /* sceSuspendForKernel 0xeadb1bd7 @ 0x9574 */
	jr	$ra
	nop
	.globl	sceKernelRegisterSysEventHandler
	.type	sceKernelRegisterSysEventHandler, @function
sceKernelRegisterSysEventHandler: /* sceSysEventForKernel 0xcd9e4bb5 @ 0x957c */
	jr	$ra
	nop
	.globl	sceKernelUnregisterSysEventHandler
	.type	sceKernelUnregisterSysEventHandler, @function
sceKernelUnregisterSysEventHandler: /* sceSysEventForKernel 0xd7d3fdcd @ 0x9584 */
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
	.half	104
	.word	.L_exp_tbl_2
	.word	0x00040000
	.word	.L_exp_lib_3
	.byte	17, 0
	.half	0x4001
	.byte	5, 0
	.half	60
	.word	.L_exp_tbl_3
	.word	0x00020000
	.word	.L_exp_lib_4
	.byte	17, 0
	.half	0x4001
	.byte	5, 0
	.half	9
	.word	.L_exp_tbl_4
	.word	0x01000000

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
	.half	13
	.word	.L_imp_nid_0
	.word	sceRegGetKeyValueByName
	.word	0
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	2
	.word	.L_imp_nid_1
	.word	scePower_driver_315B8CB6
	.word	0
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	3
	.word	.L_imp_nid_2
	.word	sceCtrlPeekBufferPositive
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	5
	.word	.L_imp_nid_3
	.word	memset
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0009
	.byte	6, 0
	.half	2
	.word	.L_imp_nid_4
	.word	sceKernelCpuSuspendIntr
	.word	0
	.word	.L_imp_lib_5
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	17
	.word	.L_imp_nid_5
	.word	sceKernelSetEventFlag
	.word	0
	.word	.L_imp_lib_6
	.byte	17, 0
	.half	0x0009
	.byte	6, 0
	.half	6
	.word	.L_imp_nid_6
	.word	sceKernelUnloadModule
	.word	0
	.word	.L_imp_lib_7
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	1
	.word	.L_imp_nid_7
	.word	sceIoDevctl
	.word	0
	.word	.L_imp_lib_8
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_8
	.word	sceKernelRegisterSysEventHandler
	.word	.L_imp_lib_9
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	4
	.word	.L_imp_nid_9
	.word	sceKernelPowerUnlockForUser
	.word	.L_imp_lib_10
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	4
	.word	.L_imp_nid_10
	.word	sceKernelIsToolMode
	.word	.L_imp_lib_11
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_11
	.word	sceKernelDcacheInvalidateRange
	.word	.L_imp_lib_12
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	10
	.word	.L_imp_nid_12
	.word	sceKernelGetModel
	.word	.L_imp_lib_13
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	3
	.word	.L_imp_nid_13
	.word	sceKernelBootFrom
	.word	.L_imp_lib_14
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_14
	.word	sceNpDrmSetDebugMode
	.word	.L_imp_lib_15
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_15
	.word	sceKermit_driver_36666181
	.word	.L_imp_lib_16
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_16
	.word	sceKermitMemory_driver_90B662D0
	.word	.L_imp_lib_17
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_17
	.word	sceKermitPeripheral_driver_61BEA1D2
	.word	.L_imp_lib_18
	.byte	17, 0
	.half	0x0009
	.byte	6, 0
	.half	1
	.word	.L_imp_nid_18
	.word	sceMeBootStart
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
	.byte	2, 1
	.asciz	"sceUtility_Driver"
	.space	10
	.word	_gp
	.word	__lib_ent_top + 4
	.word	__lib_ent_btm
	.word	__lib_stub_top + 4
	.word	__lib_stub_btm

	.section .rodata.sceResident, "a", @progbits
	.align	2
	.word	0
.L_imp_lib_0:
	.asciz	"sceReg_driver"
	.space	6
.L_imp_lib_1:
	.asciz	"scePower_driver"
	.space	4
.L_imp_lib_2:
	.asciz	"sceCtrl_driver"
	.space	5
.L_imp_lib_3:
	.asciz	"SysclibForKernel"
	.space	7
.L_imp_lib_4:
	.asciz	"InterruptManagerForKernel"
	.space	6
.L_imp_lib_5:
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_6:
	.asciz	"ModuleMgrForKernel"
	.space	5
.L_imp_lib_7:
	.asciz	"IoFileMgrForKernel"
	.space	5
.L_imp_lib_8:
	.asciz	"sceSysEventForKernel"
	.space	7
.L_imp_lib_9:
	.asciz	"sceSuspendForKernel"
	.space	4
.L_imp_lib_10:
	.asciz	"KDebugForKernel"
	.space	4
.L_imp_lib_11:
	.asciz	"UtilsForKernel"
	.space	5
.L_imp_lib_12:
	.asciz	"SysMemForKernel"
	.space	4
.L_imp_lib_13:
	.asciz	"InitForKernel"
	.space	6
.L_imp_lib_14:
	.asciz	"scePspNpDrm_driver"
	.space	5
.L_imp_lib_15:
	.asciz	"sceKermit_driver"
	.space	7
.L_imp_lib_16:
	.asciz	"sceKermitMemory_driver"
	.space	5
.L_imp_lib_17:
	.asciz	"sceKermitPeripheral_driver"
	.space	5
.L_imp_lib_18:
	.asciz	"sceMeCore_driver"
	.space	3
.L_exp_tbl_0:
	.word	0xd632acdb
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_start /* module_start (0xd632acdb) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"sceUtility_driver"
	.space	2
.L_exp_tbl_1:
	.word	0x20c68c34
	.word	0xf062aea6
	.word	sceUtility_driver_20C68C34 /* sceUtility_driver_20C68C34 (0x20c68c34) */
	.word	sceUtility_driver_F062AEA6 /* sceUtility_driver_F062AEA6 (0xf062aea6) */
.L_exp_lib_2:
	.asciz	"sceUtility"
	.space	1
.L_exp_tbl_2:
	.word	0x0251b134
	.word	0x05afb9e4
	.word	0x06a48659
	.word	0x0940a1b9
	.word	0x094198b8
	.word	0x0d5bc6d2
	.word	0x0e0c27af
	.word	0x0f3eeaac
	.word	0x1281da8e
	.word	0x147f7c85
	.word	0x149a7895
	.word	0x1579a159
	.word	0x16a1a8d8
	.word	0x16d02af0
	.word	0x180f7b62
	.word	0x24ac31eb
	.word	0x28d35634
	.word	0x2995d020
	.word	0x2a2b3de0
	.word	0x2ad8e239
	.word	0x2b96173b
	.word	0x32e32dcb
	.word	0x34b78343
	.word	0x3a15cd0a
	.word	0x3aad51dc
	.word	0x3ad50ae7
	.word	0x3dfaeba9
	.word	0x417bed54
	.word	0x41e30674
	.word	0x42071a83
	.word	0x434d4b3a
	.word	0x45c18506
	.word	0x4928bd96
	.word	0x4a833ba4
	.word	0x4aecd179
	.word	0x4b0a8fe5
	.word	0x4b85c861
	.word	0x4db1e739
	.word	0x4fed24d8
	.word	0x50c4cd57
	.word	0x54a5c62f
	.word	0x5eee6548
	.word	0x5ef1c24a
	.word	0x6332aa39
	.word	0x64d50c56
	.word	0x67af3428
	.word	0x6f56f9cf
	.word	0x70267adf
	.word	0x7853182d
	.word	0x81c44706
	.word	0x8326ab05
	.word	0x86a03a27
	.word	0x86abdb1b
	.word	0x8874dbe0
	.word	0x88bc7406
	.word	0x89317c8f
	.word	0x91e70e35
	.word	0x943cba46
	.word	0x946963f3
	.word	0x95fc253b
	.word	0x9790b33c
	.word	0x9a1c91d7
	.word	0x9f313d14
	.word	0xa03d29ba
	.word	0xa084e056
	.word	0xa50e5b30
	.word	0xa5da2406
	.word	0xa7bb7c67
	.word	0xab083ea9
	.word	0xb0fb7ff5
	.word	0xb57e95d9
	.word	0xb62a4061
	.word	0xb6cee597
	.word	0xb8592d5f
	.word	0xbc6b6296
	.word	0xbda7d894
	.word	0xc130d441
	.word	0xc4700fa3
	.word	0xc492f751
	.word	0xc629af26
	.word	0xcdc3aa41
	.word	0xd17a0573
	.word	0xd23665f4
	.word	0xd4b95ffb
	.word	0xd4c2bd73
	.word	0xd81957b7
	.word	0xd852cdce
	.word	0xda97f1aa
	.word	0xdde5389d
	.word	0xe19c97d6
	.word	0xe49bfe92
	.word	0xe7b778d8
	.word	0xece1d3e5
	.word	0xed0fad38
	.word	0xef3582b2
	.word	0xefc6f80f
	.word	0xf3f76017
	.word	0xf3fbc572
	.word	0xf5ce1134
	.word	0xf6269b82
	.word	0xf64910f0
	.word	0xf7d8d092
	.word	0xf88155f6
	.word	0xf9e0008c
	.word	sceUtility_0251B134 /* sceUtility_0251B134 (0x0251b134) */
	.word	sceUtilityHtmlViewerUpdate /* sceUtilityHtmlViewerUpdate (0x05afb9e4) */
	.word	sceUtility_06A48659 /* sceUtility_06A48659 (0x06a48659) */
	.word	sceUtility_0940A1B9 /* sceUtility_0940A1B9 (0x0940a1b9) */
	.word	sceUtility_094198B8 /* sceUtility_094198B8 (0x094198b8) */
	.word	sceUtilityLoadUsbModule /* sceUtilityLoadUsbModule (0x0d5bc6d2) */
	.word	sceUtility_0E0C27AF /* sceUtility_0E0C27AF (0x0e0c27af) */
	.word	sceUtility_0F3EEAAC /* sceUtility_0F3EEAAC (0x0f3eeaac) */
	.word	sceUtility_1281DA8E /* sceUtility_1281DA8E (0x1281da8e) */
	.word	sceUtility_147F7C85 /* sceUtility_147F7C85 (0x147f7c85) */
	.word	sceUtility_149A7895 /* sceUtility_149A7895 (0x149a7895) */
	.word	sceUtilityLoadNetModule /* sceUtilityLoadNetModule (0x1579a159) */
	.word	sceUtility_16A1A8D8 /* sceUtility_16A1A8D8 (0x16a1a8d8) */
	.word	sceUtility_16D02AF0 /* sceUtility_16D02AF0 (0x16d02af0) */
	.word	sceUtility_180F7B62 /* sceUtility_180F7B62 (0x180f7b62) */
	.word	sceUtility_24AC31EB /* sceUtility_24AC31EB (0x24ac31eb) */
	.word	sceUtility_28D35634 /* sceUtility_28D35634 (0x28d35634) */
	.word	sceUtility_2995D020 /* sceUtility_2995D020 (0x2995d020) */
	.word	sceUtilityLoadModule /* sceUtilityLoadModule (0x2a2b3de0) */
	.word	sceUtilityMsgDialogInitStart /* sceUtilityMsgDialogInitStart (0x2ad8e239) */
	.word	sceUtility_2B96173B /* sceUtility_2B96173B (0x2b96173b) */
	.word	sceUtility_32E32DCB /* sceUtility_32E32DCB (0x32e32dcb) */
	.word	sceUtilityGetSystemParamString /* sceUtilityGetSystemParamString (0x34b78343) */
	.word	sceUtility_3A15CD0A /* sceUtility_3A15CD0A (0x3a15cd0a) */
	.word	sceUtility_3AAD51DC /* sceUtility_3AAD51DC (0x3aad51dc) */
	.word	sceUtility_3AD50AE7 /* sceUtility_3AD50AE7 (0x3ad50ae7) */
	.word	sceUtilityOskShutdownStart /* sceUtilityOskShutdownStart (0x3dfaeba9) */
	.word	sceUtility_417BED54 /* sceUtility_417BED54 (0x417bed54) */
	.word	sceUtilitySetSystemParamString /* sceUtilitySetSystemParamString (0x41e30674) */
	.word	sceUtility_42071A83 /* sceUtility_42071A83 (0x42071a83) */
	.word	sceUtilityGetNetParam /* sceUtilityGetNetParam (0x434d4b3a) */
	.word	sceUtilitySetSystemParamInt /* sceUtilitySetSystemParamInt (0x45c18506) */
	.word	sceUtilityMsgDialogAbort /* sceUtilityMsgDialogAbort (0x4928bd96) */
	.word	sceUtility_4A833BA4 /* sceUtility_4A833BA4 (0x4a833ba4) */
	.word	sceUtility_4AECD179 /* sceUtility_4AECD179 (0x4aecd179) */
	.word	sceUtility_4B0A8FE5 /* sceUtility_4B0A8FE5 (0x4b0a8fe5) */
	.word	sceUtilityOskUpdate /* sceUtilityOskUpdate (0x4b85c861) */
	.word	sceUtilityNetconfInitStart /* sceUtilityNetconfInitStart (0x4db1e739) */
	.word	sceUtility_4FED24D8 /* sceUtility_4FED24D8 (0x4fed24d8) */
	.word	sceUtilitySavedataInitStart /* sceUtilitySavedataInitStart (0x50c4cd57) */
	.word	sceUtility_54A5C62F /* sceUtility_54A5C62F (0x54a5c62f) */
	.word	sceUtilityCheckNetParam /* sceUtilityCheckNetParam (0x5eee6548) */
	.word	sceUtility_5EF1C24A /* sceUtility_5EF1C24A (0x5ef1c24a) */
	.word	sceUtilityNetconfGetStatus /* sceUtilityNetconfGetStatus (0x6332aa39) */
	.word	sceUtilityUnloadNetModule /* sceUtilityUnloadNetModule (0x64d50c56) */
	.word	sceUtilityMsgDialogShutdownStart /* sceUtilityMsgDialogShutdownStart (0x67af3428) */
	.word	sceUtility_6F56F9CF /* sceUtility_6F56F9CF (0x6f56f9cf) */
	.word	sceUtility_70267ADF /* sceUtility_70267ADF (0x70267adf) */
	.word	sceUtilityGameSharingUpdate /* sceUtilityGameSharingUpdate (0x7853182d) */
	.word	sceUtility_81C44706 /* sceUtility_81C44706 (0x81c44706) */
	.word	sceUtility_8326AB05 /* sceUtility_8326AB05 (0x8326ab05) */
	.word	sceUtility_86A03A27 /* sceUtility_86A03A27 (0x86a03a27) */
	.word	sceUtility_86ABDB1B /* sceUtility_86ABDB1B (0x86abdb1b) */
	.word	sceUtilitySavedataGetStatus /* sceUtilitySavedataGetStatus (0x8874dbe0) */
	.word	sceUtility_88BC7406 /* sceUtility_88BC7406 (0x88bc7406) */
	.word	sceUtility_89317C8F /* sceUtility_89317C8F (0x89317c8f) */
	.word	sceUtilityNetconfUpdate /* sceUtilityNetconfUpdate (0x91e70e35) */
	.word	sceUtility_943CBA46 /* sceUtility_943CBA46 (0x943cba46) */
	.word	sceUtilityGameSharingGetStatus /* sceUtilityGameSharingGetStatus (0x946963f3) */
	.word	sceUtilityMsgDialogUpdate /* sceUtilityMsgDialogUpdate (0x95fc253b) */
	.word	sceUtilitySavedataShutdownStart /* sceUtilitySavedataShutdownStart (0x9790b33c) */
	.word	sceUtilityMsgDialogGetStatus /* sceUtilityMsgDialogGetStatus (0x9a1c91d7) */
	.word	sceUtility_9F313D14 /* sceUtility_9F313D14 (0x9f313d14) */
	.word	sceUtility_A03D29BA /* sceUtility_A03D29BA (0xa03d29ba) */
	.word	sceUtility_A084E056 /* sceUtility_A084E056 (0xa084e056) */
	.word	sceUtility_A50E5B30 /* sceUtility_A50E5B30 (0xa50e5b30) */
	.word	sceUtilityGetSystemParamInt /* sceUtilityGetSystemParamInt (0xa5da2406) */
	.word	sceUtility_A7BB7C67 /* sceUtility_A7BB7C67 (0xa7bb7c67) */
	.word	sceUtility_AB083EA9 /* sceUtility_AB083EA9 (0xab083ea9) */
	.word	sceUtility_B0FB7FF5 /* sceUtility_B0FB7FF5 (0xb0fb7ff5) */
	.word	sceUtility_B57E95D9 /* sceUtility_B57E95D9 (0xb57e95d9) */
	.word	sceUtility_B62A4061 /* sceUtility_B62A4061 (0xb62a4061) */
	.word	sceUtility_B6CEE597 /* sceUtility_B6CEE597 (0xb6cee597) */
	.word	sceUtility_B8592D5F /* sceUtility_B8592D5F (0xb8592d5f) */
	.word	sceUtility_BC6B6296 /* sceUtility_BC6B6296 (0xbc6b6296) */
	.word	sceUtilityHtmlViewerGetStatus /* sceUtilityHtmlViewerGetStatus (0xbda7d894) */
	.word	sceUtility_C130D441 /* sceUtility_C130D441 (0xc130d441) */
	.word	sceUtility_C4700FA3 /* sceUtility_C4700FA3 (0xc4700fa3) */
	.word	sceUtilityGameSharingInitStart /* sceUtilityGameSharingInitStart (0xc492f751) */
	.word	sceUtilityLoadAvModule /* sceUtilityLoadAvModule (0xc629af26) */
	.word	sceUtilityHtmlViewerInitStart /* sceUtilityHtmlViewerInitStart (0xcdc3aa41) */
	.word	sceUtility_D17A0573 /* sceUtility_D17A0573 (0xd17a0573) */
	.word	sceUtility_D23665F4 /* sceUtility_D23665F4 (0xd23665f4) */
	.word	sceUtilitySavedataUpdate /* sceUtilitySavedataUpdate (0xd4b95ffb) */
	.word	sceUtility_D4C2BD73 /* sceUtility_D4C2BD73 (0xd4c2bd73) */
	.word	sceUtility_D81957B7 /* sceUtility_D81957B7 (0xd81957b7) */
	.word	sceUtility_D852CDCE /* sceUtility_D852CDCE (0xd852cdce) */
	.word	sceUtility_DA97F1AA /* sceUtility_DA97F1AA (0xda97f1aa) */
	.word	sceUtility_DDE5389D /* sceUtility_DDE5389D (0xdde5389d) */
	.word	sceUtility_E19C97D6 /* sceUtility_E19C97D6 (0xe19c97d6) */
	.word	sceUtilityUnloadModule /* sceUtilityUnloadModule (0xe49bfe92) */
	.word	sceUtility_E7B778D8 /* sceUtility_E7B778D8 (0xe7b778d8) */
	.word	sceUtility_ECE1D3E5 /* sceUtility_ECE1D3E5 (0xece1d3e5) */
	.word	sceUtility_ED0FAD38 /* sceUtility_ED0FAD38 (0xed0fad38) */
	.word	sceUtility_EF3582B2 /* sceUtility_EF3582B2 (0xef3582b2) */
	.word	sceUtilityGameSharingShutdownStart /* sceUtilityGameSharingShutdownStart (0xefc6f80f) */
	.word	sceUtilityOskGetStatus /* sceUtilityOskGetStatus (0xf3f76017) */
	.word	sceUtility_F3FBC572 /* sceUtility_F3FBC572 (0xf3fbc572) */
	.word	sceUtilityHtmlViewerShutdownStart /* sceUtilityHtmlViewerShutdownStart (0xf5ce1134) */
	.word	sceUtilityOskInitStart /* sceUtilityOskInitStart (0xf6269b82) */
	.word	sceUtilityUnloadUsbModule /* sceUtilityUnloadUsbModule (0xf64910f0) */
	.word	sceUtilityUnloadAvModule /* sceUtilityUnloadAvModule (0xf7d8d092) */
	.word	sceUtilityNetconfShutdownStart /* sceUtilityNetconfShutdownStart (0xf88155f6) */
	.word	sceUtility_F9E0008C /* sceUtility_F9E0008C (0xf9e0008c) */
	.word	0x00010000
	.word	0x00050003
	.word	0x00090008
	.word	0x000f000e
	.word	0x000f000f
	.word	0x00150010
	.word	0x00160015
	.word	0x001a0017
	.word	0x001f001b
	.word	0x00250020
	.word	0x00280027
	.word	0x00290029
	.word	0x002c002b
	.word	0x002e002e
	.word	0x0030002f
	.word	0x00310030
	.word	0x00330031
	.word	0x00380035
	.word	0x00390038
	.word	0x003e003d
	.word	0x0041003f
	.word	0x00450044
	.word	0x00460045
	.word	0x004a0049
	.word	0x004d004c
	.word	0x00500050
	.word	0x00530051
	.word	0x00580055
	.word	0x005a0059
	.word	0x005c005c
	.word	0x00620060
	.word	0x00680066
.L_exp_lib_3:
	.asciz	"sceUtility_private"
	.space	1
.L_exp_tbl_3:
	.word	0x0027d953
	.word	0x031d0944
	.word	0x048bfc46
	.word	0x0c0ae566
	.word	0x0d6b23a4
	.word	0x10d1b3ff
	.word	0x14e94d25
	.word	0x186a2c84
	.word	0x1e82fb49
	.word	0x26d0b065
	.word	0x29afe096
	.word	0x31516141
	.word	0x34340686
	.word	0x363a12e2
	.word	0x369d25e4
	.word	0x390a738f
	.word	0x39e7692b
	.word	0x3b6d7ced
	.word	0x40fba532
	.word	0x421e2726
	.word	0x48b0fbcd
	.word	0x4f6ffc7c
	.word	0x5809b44a
	.word	0x59eb3a17
	.word	0x5c494ac6
	.word	0x5dcbd3c0
	.word	0x5ed16eba
	.word	0x69ab28e8
	.word	0x74e69d03
	.word	0x756a93b3
	.word	0x78a2fe0c
	.word	0x78ec982d
	.word	0x7e0d4bcd
	.word	0x7ebd6208
	.word	0x8024fca7
	.word	0x86aa70bc
	.word	0x8d681ee0
	.word	0x966912d2
	.word	0x981ffd72
	.word	0xa1348d82
	.word	0xa25e0194
	.word	0xa6b323e1
	.word	0xac7fd75c
	.word	0xacb67a01
	.word	0xb08b2b48
	.word	0xb34ec4ad
	.word	0xb9d9c78f
	.word	0xbaffb9d5
	.word	0xbbcd1885
	.word	0xc1916d25
	.word	0xc7d5983f
	.word	0xcdeebee2
	.word	0xce484145
	.word	0xcf6799a3
	.word	0xdf025e50
	.word	0xe0f5dec1
	.word	0xe8ee4e87
	.word	0xea446cd5
	.word	0xed4e37ca
	.word	0xfc841ac3
	.word	sceUtility_private_0027D953 /* sceUtility_private_0027D953 (0x0027d953) */
	.word	sceUtility_private_031D0944 /* sceUtility_private_031D0944 (0x031d0944) */
	.word	sceUtility_private_048BFC46 /* sceUtility_private_048BFC46 (0x048bfc46) */
	.word	sceUtility_private_0C0AE566 /* sceUtility_private_0C0AE566 (0x0c0ae566) */
	.word	sceUtility_private_0D6B23A4 /* sceUtility_private_0D6B23A4 (0x0d6b23a4) */
	.word	sceUtility_private_10D1B3FF /* sceUtility_private_10D1B3FF (0x10d1b3ff) */
	.word	sceUtility_private_14E94D25 /* sceUtility_private_14E94D25 (0x14e94d25) */
	.word	sceUtility_private_186A2C84 /* sceUtility_private_186A2C84 (0x186a2c84) */
	.word	sceUtility_private_1E82FB49 /* sceUtility_private_1E82FB49 (0x1e82fb49) */
	.word	sceUtility_private_26D0B065 /* sceUtility_private_26D0B065 (0x26d0b065) */
	.word	sceUtility_private_29AFE096 /* sceUtility_private_29AFE096 (0x29afe096) */
	.word	sceUtility_private_31516141 /* sceUtility_private_31516141 (0x31516141) */
	.word	sceUtility_private_34340686 /* sceUtility_private_34340686 (0x34340686) */
	.word	sceUtility_private_363A12E2 /* sceUtility_private_363A12E2 (0x363a12e2) */
	.word	sceUtility_private_369D25E4 /* sceUtility_private_369D25E4 (0x369d25e4) */
	.word	sceUtility_private_390A738F /* sceUtility_private_390A738F (0x390a738f) */
	.word	sceUtility_private_39E7692B /* sceUtility_private_39E7692B (0x39e7692b) */
	.word	sceUtility_private_3B6D7CED /* sceUtility_private_3B6D7CED (0x3b6d7ced) */
	.word	sceUtility_private_40FBA532 /* sceUtility_private_40FBA532 (0x40fba532) */
	.word	sceUtility_private_421E2726 /* sceUtility_private_421E2726 (0x421e2726) */
	.word	sceUtility_private_48B0FBCD /* sceUtility_private_48B0FBCD (0x48b0fbcd) */
	.word	sceUtility_private_4F6FFC7C /* sceUtility_private_4F6FFC7C (0x4f6ffc7c) */
	.word	sceUtility_private_5809B44A /* sceUtility_private_5809B44A (0x5809b44a) */
	.word	sceUtility_private_59EB3A17 /* sceUtility_private_59EB3A17 (0x59eb3a17) */
	.word	sceUtility_private_5C494AC6 /* sceUtility_private_5C494AC6 (0x5c494ac6) */
	.word	sceUtility_private_5DCBD3C0 /* sceUtility_private_5DCBD3C0 (0x5dcbd3c0) */
	.word	sceUtility_private_5ED16EBA /* sceUtility_private_5ED16EBA (0x5ed16eba) */
	.word	sceUtility_private_69AB28E8 /* sceUtility_private_69AB28E8 (0x69ab28e8) */
	.word	sceUtility_private_74E69D03 /* sceUtility_private_74E69D03 (0x74e69d03) */
	.word	sceUtility_private_756A93B3 /* sceUtility_private_756A93B3 (0x756a93b3) */
	.word	sceUtility_private_78A2FE0C /* sceUtility_private_78A2FE0C (0x78a2fe0c) */
	.word	sceUtility_private_78EC982D /* sceUtility_private_78EC982D (0x78ec982d) */
	.word	sceUtility_private_7E0D4BCD /* sceUtility_private_7E0D4BCD (0x7e0d4bcd) */
	.word	sceUtility_private_7EBD6208 /* sceUtility_private_7EBD6208 (0x7ebd6208) */
	.word	sceUtility_private_8024FCA7 /* sceUtility_private_8024FCA7 (0x8024fca7) */
	.word	sceUtility_private_86AA70BC /* sceUtility_private_86AA70BC (0x86aa70bc) */
	.word	sceUtility_private_8D681EE0 /* sceUtility_private_8D681EE0 (0x8d681ee0) */
	.word	sceUtility_private_966912D2 /* sceUtility_private_966912D2 (0x966912d2) */
	.word	sceUtility_private_981FFD72 /* sceUtility_private_981FFD72 (0x981ffd72) */
	.word	sceUtility_private_A1348D82 /* sceUtility_private_A1348D82 (0xa1348d82) */
	.word	sceUtility_private_A25E0194 /* sceUtility_private_A25E0194 (0xa25e0194) */
	.word	sceUtility_private_A6B323E1 /* sceUtility_private_A6B323E1 (0xa6b323e1) */
	.word	sceUtility_private_AC7FD75C /* sceUtility_private_AC7FD75C (0xac7fd75c) */
	.word	sceUtility_private_ACB67A01 /* sceUtility_private_ACB67A01 (0xacb67a01) */
	.word	sceUtility_private_B08B2B48 /* sceUtility_private_B08B2B48 (0xb08b2b48) */
	.word	sceUtility_private_B34EC4AD /* sceUtility_private_B34EC4AD (0xb34ec4ad) */
	.word	sceUtility_private_B9D9C78F /* sceUtility_private_B9D9C78F (0xb9d9c78f) */
	.word	sceUtility_private_BAFFB9D5 /* sceUtility_private_BAFFB9D5 (0xbaffb9d5) */
	.word	sceUtility_private_BBCD1885 /* sceUtility_private_BBCD1885 (0xbbcd1885) */
	.word	sceUtility_private_C1916D25 /* sceUtility_private_C1916D25 (0xc1916d25) */
	.word	sceUtility_private_C7D5983F /* sceUtility_private_C7D5983F (0xc7d5983f) */
	.word	sceUtility_private_CDEEBEE2 /* sceUtility_private_CDEEBEE2 (0xcdeebee2) */
	.word	sceUtility_private_CE484145 /* sceUtility_private_CE484145 (0xce484145) */
	.word	sceUtility_private_CF6799A3 /* sceUtility_private_CF6799A3 (0xcf6799a3) */
	.word	sceUtility_private_DF025E50 /* sceUtility_private_DF025E50 (0xdf025e50) */
	.word	sceUtility_private_E0F5DEC1 /* sceUtility_private_E0F5DEC1 (0xe0f5dec1) */
	.word	sceUtility_private_E8EE4E87 /* sceUtility_private_E8EE4E87 (0xe8ee4e87) */
	.word	sceUtility_private_EA446CD5 /* sceUtility_private_EA446CD5 (0xea446cd5) */
	.word	sceUtility_private_ED4E37CA /* sceUtility_private_ED4E37CA (0xed4e37ca) */
	.word	sceUtility_private_FC841AC3 /* sceUtility_private_FC841AC3 (0xfc841ac3) */
	.word	0x00050000
	.word	0x000b0009
	.word	0x00160012
	.word	0x001c001b
	.word	0x00250022
	.word	0x002c0027
	.word	0x00360031
	.word	0x003b0037
.L_exp_lib_4:
	.asciz	"sceUtility_netparam_internal"
	.space	3
.L_exp_tbl_4:
	.word	0x02598db8
	.word	0x09905636
	.word	0x239f260d
	.word	0x4cb183a4
	.word	0x5d63aa06
	.word	0x61d0686e
	.word	0x67c2105b
	.word	0x6b1bc62c
	.word	0x6d77b975
	.word	sceUtility_netparam_internal_02598DB8 /* sceUtility_netparam_internal_02598DB8 (0x02598db8) */
	.word	sceUtility_netparam_internal_09905636 /* sceUtility_netparam_internal_09905636 (0x09905636) */
	.word	sceUtility_netparam_internal_239F260D /* sceUtility_netparam_internal_239F260D (0x239f260d) */
	.word	sceUtility_netparam_internal_4CB183A4 /* sceUtility_netparam_internal_4CB183A4 (0x4cb183a4) */
	.word	sceUtility_netparam_internal_5D63AA06 /* sceUtility_netparam_internal_5D63AA06 (0x5d63aa06) */
	.word	sceUtility_netparam_internal_61D0686E /* sceUtility_netparam_internal_61D0686E (0x61d0686e) */
	.word	sceUtility_netparam_internal_67C2105B /* sceUtility_netparam_internal_67C2105B (0x67c2105b) */
	.word	sceUtility_netparam_internal_6B1BC62C /* sceUtility_netparam_internal_6B1BC62C (0x6b1bc62c) */
	.word	sceUtility_netparam_internal_6D77B975 /* sceUtility_netparam_internal_6D77B975 (0x6d77b975) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x38415b9f
	.word	0x3b6ca1e6
	.word	0x49c70163
	.word	0x49d77d65
	.word	0x4f471457
	.word	0x5fd4764a
	.word	0x61db9d06
	.word	0x9980519f
	.word	0xd743a608
	.word	0xdba46704
	.word	0xf2619407
	.word	0xf4a3e396
	.word	0xfc742751
.L_imp_nid_1:
	.word	0x315b8cb6
	.word	0x766cd857
.L_imp_nid_2:
	.word	0x2ba616af
	.word	0xf6e94ea3
	.word	0xf8ec18bd
.L_imp_nid_3:
	.word	0x10f3bb61
	.word	0x52df196c
	.word	0xb49a7697
	.word	0xc0ab8932
	.word	0xc2145e80
.L_imp_nid_4:
	.word	0x092968f4
	.word	0x5f10d406
.L_imp_nid_5:
	.word	0x1fb15a32
	.word	0x278c0df5
	.word	0x293b45b8
	.word	0x369ed59d
	.word	0x402fcf22
	.word	0x446d8de6
	.word	0x55c20a00
	.word	0x6b30100f
	.word	0x809ce29b
	.word	0x812346e4
	.word	0x9fa03cd3
	.word	0xb011b11f
	.word	0xb7d098c6
	.word	0xbc31c1b9
	.word	0xceadeb47
	.word	0xef9e4c70
	.word	0xf475845d
.L_imp_nid_6:
	.word	0x387e3ca9
	.word	0x3ff74df1
	.word	0x939e4270
	.word	0xd4ee2d26
	.word	0xd86dd11b
	.word	0xe5d6087b
.L_imp_nid_7:
	.word	0x54f5fb11
.L_imp_nid_8:
	.word	0xcd9e4bb5
	.word	0xd7d3fdcd
.L_imp_nid_9:
	.word	0x3aee7261
	.word	0x3e0271d3
	.word	0xa569e425
	.word	0xeadb1bd7
.L_imp_nid_10:
	.word	0x47570ac5
	.word	0x84f370bc
	.word	0x86010fcb
	.word	0xacf427dc
.L_imp_nid_11:
	.word	0xbfa98062
.L_imp_nid_12:
	.word	0x07c586a1
	.word	0x181065ab
	.word	0x1ab50974
	.word	0x22a114dc
	.word	0x7158ce7e
	.word	0x8ae776af
	.word	0xb4f00cb5
	.word	0xc1a26c6f
	.word	0xe860be8f
	.word	0xef29061c
.L_imp_nid_13:
	.word	0x27932388
	.word	0x7233b5bc
	.word	0xa18a4a8b
.L_imp_nid_14:
	.word	0x2d88879a
.L_imp_nid_15:
	.word	0x36666181
.L_imp_nid_16:
	.word	0x90b662d0
	.word	0xaaf047ac
.L_imp_nid_17:
	.word	0x61bea1d2
.L_imp_nid_18:
	.word	0x5dff5c50

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
