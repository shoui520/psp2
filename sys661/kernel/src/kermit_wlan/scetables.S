/* Generated PSP module import/export tables for sceWlan_Driver */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelCpuSuspendIntr
	.type	sceKernelCpuSuspendIntr, @function
sceKernelCpuSuspendIntr: /* InterruptManagerForKernel 0x092968f4 @ 0x6eb0 */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntr
	.type	sceKernelCpuResumeIntr, @function
sceKernelCpuResumeIntr: /* InterruptManagerForKernel 0x5f10d406 @ 0x6eb8 */
	jr	$ra
	nop
	.globl	sceKernelMemcpy
	.type	sceKernelMemcpy, @function
sceKernelMemcpy: /* SysMemForKernel 0x181065ab @ 0x6ec0 */
	jr	$ra
	nop
	.globl	memset
	.type	memset, @function
memset: /* SysclibForKernel 0x10f3bb61 @ 0x6ec8 */
	jr	$ra
	nop
	.globl	memcpy
	.type	memcpy, @function
memcpy: /* SysclibForKernel 0xab7592ff @ 0x6ed0 */
	jr	$ra
	nop
	.globl	sceKernelReceiveMbx
	.type	sceKernelReceiveMbx, @function
sceKernelReceiveMbx: /* ThreadManForKernel 0x18260574 @ 0x6ed8 */
	jr	$ra
	nop
	.globl	sceKernelWaitThreadEnd
	.type	sceKernelWaitThreadEnd, @function
sceKernelWaitThreadEnd: /* ThreadManForKernel 0x278c0df5 @ 0x6ee0 */
	jr	$ra
	nop
	.globl	sceKernelDeleteSema
	.type	sceKernelDeleteSema, @function
sceKernelDeleteSema: /* ThreadManForKernel 0x28b6489c @ 0x6ee8 */
	jr	$ra
	nop
	.globl	sceKernelGetSystemTimeLow
	.type	sceKernelGetSystemTimeLow, @function
sceKernelGetSystemTimeLow: /* ThreadManForKernel 0x369ed59d @ 0x6ef0 */
	jr	$ra
	nop
	.globl	sceKernelSignalSema
	.type	sceKernelSignalSema, @function
sceKernelSignalSema: /* ThreadManForKernel 0x3f53e640 @ 0x6ef8 */
	jr	$ra
	nop
	.globl	sceKernelCreateThread
	.type	sceKernelCreateThread, @function
sceKernelCreateThread: /* ThreadManForKernel 0x446d8de6 @ 0x6f00 */
	jr	$ra
	nop
	.globl	sceKernelWaitSema
	.type	sceKernelWaitSema, @function
sceKernelWaitSema: /* ThreadManForKernel 0x4e3a1105 @ 0x6f08 */
	jr	$ra
	nop
	.globl	sceKernelCreateVpl
	.type	sceKernelCreateVpl, @function
sceKernelCreateVpl: /* ThreadManForKernel 0x56c039b5 @ 0x6f10 */
	jr	$ra
	nop
	.globl	sceKernelTerminateThread
	.type	sceKernelTerminateThread, @function
sceKernelTerminateThread: /* ThreadManForKernel 0x616403ba @ 0x6f18 */
	jr	$ra
	nop
	.globl	sceKernelSetAlarm
	.type	sceKernelSetAlarm, @function
sceKernelSetAlarm: /* ThreadManForKernel 0x6652b8ca @ 0x6f20 */
	jr	$ra
	nop
	.globl	sceKernelUnlockMutex
	.type	sceKernelUnlockMutex, @function
sceKernelUnlockMutex: /* ThreadManForKernel 0x6b30100f @ 0x6f28 */
	jr	$ra
	nop
	.globl	sceKernelChangeThreadPriority
	.type	sceKernelChangeThreadPriority, @function
sceKernelChangeThreadPriority: /* ThreadManForKernel 0x71bc9871 @ 0x6f30 */
	jr	$ra
	nop
	.globl	sceKernelCancelAlarm
	.type	sceKernelCancelAlarm, @function
sceKernelCancelAlarm: /* ThreadManForKernel 0x7e65b999 @ 0x6f38 */
	jr	$ra
	nop
	.globl	sceKernelExitDeleteThread
	.type	sceKernelExitDeleteThread, @function
sceKernelExitDeleteThread: /* ThreadManForKernel 0x809ce29b @ 0x6f40 */
	jr	$ra
	nop
	.globl	sceKernelCreateMbx
	.type	sceKernelCreateMbx, @function
sceKernelCreateMbx: /* ThreadManForKernel 0x8125221d @ 0x6f48 */
	jr	$ra
	nop
	.globl	sceKernelGetSystemTimeWide
	.type	sceKernelGetSystemTimeWide, @function
sceKernelGetSystemTimeWide: /* ThreadManForKernel 0x82bc5777 @ 0x6f50 */
	jr	$ra
	nop
	.globl	sceKernelDeleteMbx
	.type	sceKernelDeleteMbx, @function
sceKernelDeleteMbx: /* ThreadManForKernel 0x86255ada @ 0x6f58 */
	jr	$ra
	nop
	.globl	sceKernelDeleteVpl
	.type	sceKernelDeleteVpl, @function
sceKernelDeleteVpl: /* ThreadManForKernel 0x89b3d48c @ 0x6f60 */
	jr	$ra
	nop
	.globl	sceKernelTryAllocateVpl
	.type	sceKernelTryAllocateVpl, @function
sceKernelTryAllocateVpl: /* ThreadManForKernel 0xaf36d708 @ 0x6f68 */
	jr	$ra
	nop
	.globl	sceKernelLockMutex
	.type	sceKernelLockMutex, @function
sceKernelLockMutex: /* ThreadManForKernel 0xb011b11f @ 0x6f70 */
	jr	$ra
	nop
	.globl	sceKernelFreeVpl
	.type	sceKernelFreeVpl, @function
sceKernelFreeVpl: /* ThreadManForKernel 0xb736e9ff @ 0x6f78 */
	jr	$ra
	nop
	.globl	sceKernelCreateMutex
	.type	sceKernelCreateMutex, @function
sceKernelCreateMutex: /* ThreadManForKernel 0xb7d098c6 @ 0x6f80 */
	jr	$ra
	nop
	.globl	sceKernelAllocateVpl
	.type	sceKernelAllocateVpl, @function
sceKernelAllocateVpl: /* ThreadManForKernel 0xbed27435 @ 0x6f88 */
	jr	$ra
	nop
	.globl	sceKernelNotifyCallback
	.type	sceKernelNotifyCallback, @function
sceKernelNotifyCallback: /* ThreadManForKernel 0xc11ba8c4 @ 0x6f90 */
	jr	$ra
	nop
	.globl	sceKernelDelayThread
	.type	sceKernelDelayThread, @function
sceKernelDelayThread: /* ThreadManForKernel 0xceadeb47 @ 0x6f98 */
	jr	$ra
	nop
	.globl	sceKernelCreateSema
	.type	sceKernelCreateSema, @function
sceKernelCreateSema: /* ThreadManForKernel 0xd6da4ba1 @ 0x6fa0 */
	jr	$ra
	nop
	.globl	sceKernelSendMbx
	.type	sceKernelSendMbx, @function
sceKernelSendMbx: /* ThreadManForKernel 0xe9b3061e @ 0x6fa8 */
	jr	$ra
	nop
	.globl	sceKernelStartThread
	.type	sceKernelStartThread, @function
sceKernelStartThread: /* ThreadManForKernel 0xf475845d @ 0x6fb0 */
	jr	$ra
	nop
	.globl	sceKernelDcacheInvalidateRange
	.type	sceKernelDcacheInvalidateRange, @function
sceKernelDcacheInvalidateRange: /* UtilsForKernel 0xbfa98062 @ 0x6fb8 */
	jr	$ra
	nop
	.globl	sceIdStorageLookup
	.type	sceIdStorageLookup, @function
sceIdStorageLookup: /* sceIdStorage_driver 0x6fe062d1 @ 0x6fc0 */
	jr	$ra
	nop
	.globl	sceKermitMemory_driver_AAF047AC
	.type	sceKermitMemory_driver_AAF047AC, @function
sceKermitMemory_driver_AAF047AC: /* sceKermitMemory_driver 0xaaf047ac @ 0x6fc8 */
	jr	$ra
	nop
	.globl	sceKermit_driver_36666181
	.type	sceKermit_driver_36666181, @function
sceKermit_driver_36666181: /* sceKermit_driver 0x36666181 @ 0x6fd0 */
	jr	$ra
	nop
	.globl	sceKermit_driver_5280B410
	.type	sceKermit_driver_5280B410, @function
sceKermit_driver_5280B410: /* sceKermit_driver 0x5280b410 @ 0x6fd8 */
	jr	$ra
	nop
	.globl	sceNetIfhandle_driver_16042084
	.type	sceNetIfhandle_driver_16042084, @function
sceNetIfhandle_driver_16042084: /* sceNetIfhandle_driver 0x16042084 @ 0x6fe0 */
	jr	$ra
	nop
	.globl	sceNetIfhandle_driver_C9344A59
	.type	sceNetIfhandle_driver_C9344A59, @function
sceNetIfhandle_driver_C9344A59: /* sceNetIfhandle_driver 0xc9344a59 @ 0x6fe8 */
	jr	$ra
	nop
	.globl	sceNetIfhandle_driver_AE81C0CB
	.type	sceNetIfhandle_driver_AE81C0CB, @function
sceNetIfhandle_driver_AE81C0CB: /* sceNetIfhandle_driver 0xae81c0cb @ 0x6ff0 */
	jr	$ra
	nop
	.globl	sceNetIfhandle_driver_54D1AEA1
	.type	sceNetIfhandle_driver_54D1AEA1, @function
sceNetIfhandle_driver_54D1AEA1: /* sceNetIfhandle_driver 0x54d1aea1 @ 0x6ff8 */
	jr	$ra
	nop
	.globl	sceNetIfhandle_driver_F94BAF52
	.type	sceNetIfhandle_driver_F94BAF52, @function
sceNetIfhandle_driver_F94BAF52: /* sceNetIfhandle_driver 0xf94baf52 @ 0x7000 */
	jr	$ra
	nop
	.globl	sceNetIfhandle_driver_263767F6
	.type	sceNetIfhandle_driver_263767F6, @function
sceNetIfhandle_driver_263767F6: /* sceNetIfhandle_driver 0x263767f6 @ 0x7008 */
	jr	$ra
	nop
	.globl	sceNetIfhandle_driver_E2F4F1C9
	.type	sceNetIfhandle_driver_E2F4F1C9, @function
sceNetIfhandle_driver_E2F4F1C9: /* sceNetIfhandle_driver 0xe2f4f1c9 @ 0x7010 */
	jr	$ra
	nop
	.globl	sceNetIfhandle_driver_C28F6FF2
	.type	sceNetIfhandle_driver_C28F6FF2, @function
sceNetIfhandle_driver_C28F6FF2: /* sceNetIfhandle_driver 0xc28f6ff2 @ 0x7018 */
	jr	$ra
	nop
	.globl	sceNetIfhandle_driver_16246B99
	.type	sceNetIfhandle_driver_16246B99, @function
sceNetIfhandle_driver_16246B99: /* sceNetIfhandle_driver 0x16246b99 @ 0x7020 */
	jr	$ra
	nop
	.globl	sceNetIfhandle_driver_C6D14282
	.type	sceNetIfhandle_driver_C6D14282, @function
sceNetIfhandle_driver_C6D14282: /* sceNetIfhandle_driver 0xc6d14282 @ 0x7028 */
	jr	$ra
	nop
	.globl	sceNetIfhandle_driver_D5AD6DEA
	.type	sceNetIfhandle_driver_D5AD6DEA, @function
sceNetIfhandle_driver_D5AD6DEA: /* sceNetIfhandle_driver 0xd5ad6dea @ 0x7030 */
	jr	$ra
	nop
	.globl	sceNetIfhandle_driver_15CFE3C0
	.type	sceNetIfhandle_driver_15CFE3C0, @function
sceNetIfhandle_driver_15CFE3C0: /* sceNetIfhandle_driver 0x15cfe3c0 @ 0x7038 */
	jr	$ra
	nop
	.globl	sceNetIfhandle_driver_F8825DC4
	.type	sceNetIfhandle_driver_F8825DC4, @function
sceNetIfhandle_driver_F8825DC4: /* sceNetIfhandle_driver 0xf8825dc4 @ 0x7040 */
	jr	$ra
	nop
	.globl	sceNetIfhandle_driver_F56FAC82
	.type	sceNetIfhandle_driver_F56FAC82, @function
sceNetIfhandle_driver_F56FAC82: /* sceNetIfhandle_driver 0xf56fac82 @ 0x7048 */
	jr	$ra
	nop
	.globl	scePower_driver_8C6BEFD9
	.type	scePower_driver_8C6BEFD9, @function
scePower_driver_8C6BEFD9: /* scePower_driver 0x8c6befd9 @ 0x7050 */
	jr	$ra
	nop
	.globl	sceKernelRegisterSysEventHandler
	.type	sceKernelRegisterSysEventHandler, @function
sceKernelRegisterSysEventHandler: /* sceSysEventForKernel 0xcd9e4bb5 @ 0x7058 */
	jr	$ra
	nop
	.globl	sceKernelUnregisterSysEventHandler
	.type	sceKernelUnregisterSysEventHandler, @function
sceKernelUnregisterSysEventHandler: /* sceSysEventForKernel 0xd7d3fdcd @ 0x7060 */
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
	.byte	4, 0
	.half	3
	.word	.L_exp_tbl_1
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x4001
	.byte	5, 0
	.half	47
	.word	.L_exp_tbl_2
	.word	0x00020000
	.word	.L_exp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	32
	.word	.L_exp_tbl_3
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
	.half	1
	.word	.L_imp_nid_0
	.word	sceKernelMemcpy
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_1
	.word	sceKernelRegisterSysEventHandler
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_2
	.word	sceIdStorageLookup
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	1
	.word	.L_imp_nid_3
	.word	scePower_driver_8C6BEFD9
	.word	0
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_4
	.word	sceKermit_driver_36666181
	.word	.L_imp_lib_5
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_5
	.word	sceKermitMemory_driver_AAF047AC
	.word	.L_imp_lib_6
	.byte	0, 0
	.half	0x0009
	.byte	5, 0
	.half	14
	.word	.L_imp_nid_6
	.word	sceNetIfhandle_driver_16042084
	.word	.L_imp_lib_7
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_7
	.word	memset
	.word	.L_imp_lib_8
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	28
	.word	.L_imp_nid_8
	.word	sceKernelReceiveMbx
	.word	0
	.word	.L_imp_lib_9
	.byte	17, 0
	.half	0x0009
	.byte	6, 0
	.half	2
	.word	.L_imp_nid_9
	.word	sceKernelCpuSuspendIntr
	.word	0
	.word	.L_imp_lib_10
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_10
	.word	sceKernelDcacheInvalidateRange

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
	.byte	9, 1
	.asciz	"sceWlan_Driver"
	.space	13
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
	.asciz	"sceSysEventForKernel"
	.space	7
.L_imp_lib_2:
	.asciz	"sceIdStorage_driver"
	.space	4
.L_imp_lib_3:
	.asciz	"scePower_driver"
	.space	4
.L_imp_lib_4:
	.asciz	"sceKermit_driver"
	.space	7
.L_imp_lib_5:
	.asciz	"sceKermitMemory_driver"
	.space	5
.L_imp_lib_6:
	.asciz	"sceNetIfhandle_driver"
	.space	6
.L_imp_lib_7:
	.asciz	"SysclibForKernel"
	.space	7
.L_imp_lib_8:
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_9:
	.asciz	"InterruptManagerForKernel"
	.space	6
.L_imp_lib_10:
	.asciz	"UtilsForKernel"
	.space	1
.L_exp_tbl_0:
	.word	0xd632acdb
	.word	0xcee8593c
	.word	0x2f064fa6
	.word	0xf01d73a7
	.word	0x11b97506
	.word	sceWlanDrv_driver_A9E6A56D /* module_start (0xd632acdb) */
	.word	sceWlanDrv_driver_EE744B5C /* module_stop (0xcee8593c) */
	.word	module_reboot_before /* module_reboot_before (0x2f064fa6) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"sceWlanDrv"
	.space	1
.L_exp_tbl_1:
	.word	0x0c622081
	.word	0x93440b11
	.word	0xd7763699
	.word	sceWlanGetEtherAddr /* sceWlanGetEtherAddr (0x0c622081) */
	.word	sceWlanDrv_driver_0387523A /* sceWlanDevIsPowerOn (0x93440b11) */
	.word	sceWlanGetSwitchState /* sceWlanGetSwitchState (0xd7763699) */
.L_exp_lib_2:
	.asciz	"sceWlanDrv_lib"
	.space	1
.L_exp_tbl_2:
	.word	0x05fe320c
	.word	0x107e61d0
	.word	0x19602a86
	.word	0x19e51f54
	.word	0x1ae1b369
	.word	0x2d0fae4e
	.word	0x327c9fce
	.word	0x3be42bcf
	.word	0x40b0aa4a
	.word	0x482cae9a
	.word	0x4adea1cf
	.word	0x4c14baca
	.word	0x4c7f62e0
	.word	0x51ff0d5f
	.word	0x56ae09d2
	.word	0x56f467ca
	.word	0x5baa1fe5
	.word	0x5cffaab1
	.word	0x5e7c8d94
	.word	0x5ed4049a
	.word	0x6417f8e3
	.word	0x6d5c839e
	.word	0x749b813a
	.word	0x768e275b
	.word	0x78c84fec
	.word	0x7eed1723
	.word	0x7ff54bd2
	.word	0x80b7162e
	.word	0x81579d36
	.word	0x8d5f551b
	.word	0x91e04a18
	.word	0x9506f322
	.word	0x9658c9f7
	.word	0x9ed787c8
	.word	0xa2da9314
	.word	0xa447103a
	.word	0xb4d7cb74
	.word	0xb7865db5
	.word	0xbfe46c6d
	.word	0xc934d53f
	.word	0xc9a8cab7
	.word	0xcac1fb0d
	.word	0xe093a922
	.word	0xe39f7f88
	.word	0xe47015d7
	.word	0xf7713c18
	.word	0xfe8a0b46
	.word	sceWlanDrv_driver_67645D5F /* sceWlanDevGetStateGPIO (0x05fe320c) */
	.word	sceWlanDrv_lib_107E61D0 /* sceWlanDrv_lib_107E61D0 (0x107e61d0) */
	.word	sceWlanDrv_lib_19602A86 /* sceWlanDrv_lib_19602A86 (0x19602a86) */
	.word	sceWlanDevGetMacAddr /* sceWlanDevGetMacAddr (0x19e51f54) */
	.word	sceWlanDrv_driver_1AE1B369 /* sceWlanDrv_driver_1AE1B369 (0x1ae1b369) */
	.word	sceWlanDrv_driver_041810C3 /* sceWlanDrv_lib_2D0FAE4E (0x2d0fae4e) */
	.word	sceWlanDrv_lib_327C9FCE /* sceWlanDrv_lib_327C9FCE (0x327c9fce) */
	.word	sceWlanDrv_lib_3BE42BCF /* sceWlanDrv_lib_3BE42BCF (0x3be42bcf) */
	.word	sceWlanDrv_driver_A082E6A2 /* sceWlanDrv_lib_40B0AA4A (0x40b0aa4a) */
	.word	sceWlanDrv_driver_1C18F5FE /* sceWlanDevAttach (0x482cae9a) */
	.word	sceWlanDrv_driver_4ADEA1CF /* sceWlanDrv_driver_4ADEA1CF (0x4adea1cf) */
	.word	sceWlanDrv_driver_F713143F /* sceWlanDrv_lib_4C14BACA (0x4c14baca) */
	.word	sceWlanDrv_driver_54764A18 /* sceWlanGPUnRegisterCallback (0x4c7f62e0) */
	.word	sceWlanDrv_lib_51FF0D5F /* sceWlanDrv_lib_51FF0D5F (0x51ff0d5f) */
	.word	sceWlanDrv_lib_56AE09D2 /* sceWlanDrv_lib_56AE09D2 (0x56ae09d2) */
	.word	sceWlanDrv_driver_53F89D4E /* sceWlanDrv_lib_56F467CA (0x56f467ca) */
	.word	sceWlanDrv_driver_B17481BB /* sceWlanDrv_lib_5BAA1FE5 (0x5baa1fe5) */
	.word	sceWlanDrv_lib_5CFFAAB1 /* sceWlanDrv_lib_5CFFAAB1 (0x5cffaab1) */
	.word	sceWlanDrv_driver_7C0EC2A9 /* sceWlanDevIsGameMode (0x5e7c8d94) */
	.word	sceWlanDrv_driver_ED79DCD4 /* sceWlanGPPrevEstablishActive (0x5ed4049a) */
	.word	sceWlanDrv_lib_6417F8E3 /* sceWlanDrv_lib_6417F8E3 (0x6417f8e3) */
	.word	sceWlanDrv_driver_60A078E9 /* sceWlanDrv_lib_6D5C839E (0x6d5c839e) */
	.word	sceWlanDrv_driver_58EDD140 /* sceWlanSetHostDiscover (0x749b813a) */
	.word	sceWlanDrv_lib_768E275B /* sceWlanDrv_lib_768E275B (0x768e275b) */
	.word	sceWlanDrv_lib_78C84FEC /* sceWlanDrv_lib_78C84FEC (0x78c84fec) */
	.word	sceWlanDrv_lib_7EED1723 /* sceWlanDrv_lib_7EED1723 (0x7eed1723) */
	.word	sceWlanDrv_driver_C4BCD411 /* sceWlanDevSetGPIO (0x7ff54bd2) */
	.word	sceWlanDrv_lib_80B7162E /* sceWlanDrv_lib_80B7162E (0x80b7162e) */
	.word	sceWlanDrv_driver_5E3A5AD3 /* sceWlanDrv_lib_81579D36 (0x81579d36) */
	.word	sceWlanDrv_driver_561FD176 /* sceWlanDrv_lib_8D5F551B (0x8d5f551b) */
	.word	sceWlanDrv_lib_91E04A18 /* sceWlanDrv_lib_91E04A18 (0x91e04a18) */
	.word	sceWlanDrv_lib_9506F322 /* sceWlanDrv_lib_9506F322 (0x9506f322) */
	.word	sceWlanDrv_driver_02E66C38 /* sceWlanGPRegisterCallback (0x9658c9f7) */
	.word	sceWlanDrv_lib_9ED787C8 /* sceWlanDrv_lib_9ED787C8 (0x9ed787c8) */
	.word	sceWlanDrv_lib_A2DA9314 /* sceWlanDrv_lib_A2DA9314 (0xa2da9314) */
	.word	sceWlanDrv_driver_04DBA413 /* sceWlanGPRecv (0xa447103a) */
	.word	sceWlanDrv_driver_EEC2F8AB /* sceWlanGPSend (0xb4d7cb74) */
	.word	sceWlanDrv_lib_B7865DB5 /* sceWlanDrv_lib_B7865DB5 (0xb7865db5) */
	.word	sceWlanDrv_lib_BFE46C6D /* sceWlanDrv_lib_BFE46C6D (0xbfe46c6d) */
	.word	sceWlanDrv_lib_C934D53F /* sceWlanDrv_lib_C934D53F (0xc934d53f) */
	.word	sceWlanDrv_driver_8DF43D4C /* sceWlanDevDetach (0xc9a8cab7) */
	.word	sceWlanDrv_lib_CAC1FB0D /* sceWlanDrv_lib_CAC1FB0D (0xcac1fb0d) */
	.word	sceWlanDrv_lib_E093A922 /* sceWlanDrv_lib_E093A922 (0xe093a922) */
	.word	sceWlanDrv_lib_E39F7F88 /* sceWlanDrv_lib_E39F7F88 (0xe39f7f88) */
	.word	sceWlanDrv_lib_E47015D7 /* sceWlanDrv_lib_E47015D7 (0xe47015d7) */
	.word	sceWlanDrv_lib_F7713C18 /* sceWlanDrv_lib_F7713C18 (0xf7713c18) */
	.word	sceWlanDrv_driver_0CAB500F /* sceWlanSetWakeUp (0xfe8a0b46) */
	.word	0x00010000
	.word	0x00060005
	.word	0x000d0008
	.word	0x00160014
	.word	0x001e001b
	.word	0x00240022
	.word	0x002a0027
	.word	0x002d002a
.L_exp_lib_3:
	.asciz	"sceWlanDrv_driver"
	.space	2
.L_exp_tbl_3:
	.word	0x02e66c38
	.word	0x0387523a
	.word	0x041810c3
	.word	0x04dba413
	.word	0x0cab500f
	.word	0x19e51f54
	.word	0x1ae1b369
	.word	0x1c18f5fe
	.word	0x2519eaa7
	.word	0x325f7172
	.word	0x4adea1cf
	.word	0x53f89d4e
	.word	0x54764a18
	.word	0x561fd176
	.word	0x58edd140
	.word	0x5e3a5ad3
	.word	0x60a078e9
	.word	0x67645d5f
	.word	0x7c0ec2a9
	.word	0x7dd3787c
	.word	0x8df43d4c
	.word	0x90e5530f
	.word	0xa082e6a2
	.word	0xa9e6a56d
	.word	0xb17481bb
	.word	0xb6a9700d
	.word	0xc4bcd411
	.word	0xed79dcd4
	.word	0xedd207b1
	.word	0xee744b5c
	.word	0xeec2f8ab
	.word	0xf713143f
	.word	sceWlanDrv_driver_02E66C38 /* sceWlanDrv_driver_02E66C38 (0x02e66c38) */
	.word	sceWlanDrv_driver_0387523A /* sceWlanDrv_driver_0387523A (0x0387523a) */
	.word	sceWlanDrv_driver_041810C3 /* sceWlanDrv_driver_041810C3 (0x041810c3) */
	.word	sceWlanDrv_driver_04DBA413 /* sceWlanDrv_driver_04DBA413 (0x04dba413) */
	.word	sceWlanDrv_driver_0CAB500F /* sceWlanDrv_driver_0CAB500F (0x0cab500f) */
	.word	sceWlanDevGetMacAddr /* sceWlanDevGetMacAddr (0x19e51f54) */
	.word	sceWlanDrv_driver_1AE1B369 /* sceWlanDrv_driver_1AE1B369 (0x1ae1b369) */
	.word	sceWlanDrv_driver_1C18F5FE /* sceWlanDrv_driver_1C18F5FE (0x1c18f5fe) */
	.word	sceWlanDrv_driver_2519EAA7 /* sceWlanDrv_driver_2519EAA7 (0x2519eaa7) */
	.word	sceWlanDrv_driver_325F7172 /* sceWlanDrv_driver_325F7172 (0x325f7172) */
	.word	sceWlanDrv_driver_4ADEA1CF /* sceWlanDrv_driver_4ADEA1CF (0x4adea1cf) */
	.word	sceWlanDrv_driver_53F89D4E /* sceWlanDrv_driver_53F89D4E (0x53f89d4e) */
	.word	sceWlanDrv_driver_54764A18 /* sceWlanDrv_driver_54764A18 (0x54764a18) */
	.word	sceWlanDrv_driver_561FD176 /* sceWlanDrv_driver_561FD176 (0x561fd176) */
	.word	sceWlanDrv_driver_58EDD140 /* sceWlanDrv_driver_58EDD140 (0x58edd140) */
	.word	sceWlanDrv_driver_5E3A5AD3 /* sceWlanDrv_driver_5E3A5AD3 (0x5e3a5ad3) */
	.word	sceWlanDrv_driver_60A078E9 /* sceWlanDrv_driver_60A078E9 (0x60a078e9) */
	.word	sceWlanDrv_driver_67645D5F /* sceWlanDrv_driver_67645D5F (0x67645d5f) */
	.word	sceWlanDrv_driver_7C0EC2A9 /* sceWlanDrv_driver_7C0EC2A9 (0x7c0ec2a9) */
	.word	sceWlanDrv_driver_7DD3787C /* sceWlanDrv_driver_7DD3787C (0x7dd3787c) */
	.word	sceWlanDrv_driver_8DF43D4C /* sceWlanDrv_driver_8DF43D4C (0x8df43d4c) */
	.word	sceWlanDrv_driver_90E5530F /* sceWlanDrv_driver_90E5530F (0x90e5530f) */
	.word	sceWlanDrv_driver_A082E6A2 /* sceWlanDrv_driver_A082E6A2 (0xa082e6a2) */
	.word	sceWlanDrv_driver_A9E6A56D /* sceWlanDrv_driver_A9E6A56D (0xa9e6a56d) */
	.word	sceWlanDrv_driver_B17481BB /* sceWlanDrv_driver_B17481BB (0xb17481bb) */
	.word	sceWlanDrv_driver_B6A9700D /* sceWlanDrv_driver_B6A9700D (0xb6a9700d) */
	.word	sceWlanDrv_driver_C4BCD411 /* sceWlanDrv_driver_C4BCD411 (0xc4bcd411) */
	.word	sceWlanDrv_driver_ED79DCD4 /* sceWlanDrv_driver_ED79DCD4 (0xed79dcd4) */
	.word	sceWlanDrv_driver_EDD207B1 /* sceWlanDrv_driver_EDD207B1 (0xedd207b1) */
	.word	sceWlanDrv_driver_EE744B5C /* sceWlanDrv_driver_EE744B5C (0xee744b5c) */
	.word	sceWlanDrv_driver_EEC2F8AB /* sceWlanDrv_driver_EEC2F8AB (0xeec2f8ab) */
	.word	sceWlanDrv_driver_F713143F /* sceWlanDrv_driver_F713143F (0xf713143f) */
	.word	0x00050000
	.word	0x00090008
	.word	0x000b000a
	.word	0x00120010
	.word	0x00150014
	.word	0x00180016
	.word	0x001b001a
	.word	0x001f001b

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x181065ab
.L_imp_nid_1:
	.word	0xcd9e4bb5
	.word	0xd7d3fdcd
.L_imp_nid_2:
	.word	0x6fe062d1
.L_imp_nid_3:
	.word	0x8c6befd9
.L_imp_nid_4:
	.word	0x36666181
	.word	0x5280b410
.L_imp_nid_5:
	.word	0xaaf047ac
.L_imp_nid_6:
	.word	0x16042084
	.word	0xc9344a59
	.word	0xae81c0cb
	.word	0x54d1aea1
	.word	0xf94baf52
	.word	0x263767f6
	.word	0xe2f4f1c9
	.word	0xc28f6ff2
	.word	0x16246b99
	.word	0xc6d14282
	.word	0xd5ad6dea
	.word	0x15cfe3c0
	.word	0xf8825dc4
	.word	0xf56fac82
.L_imp_nid_7:
	.word	0x10f3bb61
	.word	0xab7592ff
.L_imp_nid_8:
	.word	0x18260574
	.word	0x278c0df5
	.word	0x28b6489c
	.word	0x369ed59d
	.word	0x3f53e640
	.word	0x446d8de6
	.word	0x4e3a1105
	.word	0x56c039b5
	.word	0x616403ba
	.word	0x6652b8ca
	.word	0x6b30100f
	.word	0x71bc9871
	.word	0x7e65b999
	.word	0x809ce29b
	.word	0x8125221d
	.word	0x82bc5777
	.word	0x86255ada
	.word	0x89b3d48c
	.word	0xaf36d708
	.word	0xb011b11f
	.word	0xb736e9ff
	.word	0xb7d098c6
	.word	0xbed27435
	.word	0xc11ba8c4
	.word	0xceadeb47
	.word	0xd6da4ba1
	.word	0xe9b3061e
	.word	0xf475845d
.L_imp_nid_9:
	.word	0x092968f4
	.word	0x5f10d406
.L_imp_nid_10:
	.word	0xbfa98062

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
