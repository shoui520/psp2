/* Generated PSP module import/export tables for sceLowIO_Driver */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelDmaRegisterDdrFlush
	.type	sceKernelDmaRegisterDdrFlush, @function
sceKernelDmaRegisterDdrFlush: /* DmacManForKernel 0x32757c57 @ 0x73c0 */
	jr	$ra
	nop
	.globl	sceKernelCpuSuspendIntr
	.type	sceKernelCpuSuspendIntr, @function
sceKernelCpuSuspendIntr: /* InterruptManagerForKernel 0x092968f4 @ 0x73c8 */
	jr	$ra
	nop
	.globl	sceKernelCallSubIntrHandler
	.type	sceKernelCallSubIntrHandler, @function
sceKernelCallSubIntrHandler: /* InterruptManagerForKernel 0x0c5f7ae3 @ 0x73d0 */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntrWithSync
	.type	sceKernelCpuResumeIntrWithSync, @function
sceKernelCpuResumeIntrWithSync: /* InterruptManagerForKernel 0x3b84732d @ 0x73d8 */
	jr	$ra
	nop
	.globl	sceKernelEnableIntr
	.type	sceKernelEnableIntr, @function
sceKernelEnableIntr: /* InterruptManagerForKernel 0x4d6e7305 @ 0x73e0 */
	jr	$ra
	nop
	.globl	sceKernelRegisterIntrHandler
	.type	sceKernelRegisterIntrHandler, @function
sceKernelRegisterIntrHandler: /* InterruptManagerForKernel 0x58dd8978 @ 0x73e8 */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntr
	.type	sceKernelCpuResumeIntr, @function
sceKernelCpuResumeIntr: /* InterruptManagerForKernel 0x5f10d406 @ 0x73f0 */
	jr	$ra
	nop
	.globl	sceKernelReleaseSubIntrHandler
	.type	sceKernelReleaseSubIntrHandler, @function
sceKernelReleaseSubIntrHandler: /* InterruptManagerForKernel 0xd61e6961 @ 0x73f8 */
	jr	$ra
	nop
	.globl	sceKernelReleaseIntrHandler
	.type	sceKernelReleaseIntrHandler, @function
sceKernelReleaseIntrHandler: /* InterruptManagerForKernel 0xf987b1f0 @ 0x7400 */
	jr	$ra
	nop
	.globl	sceKernelEnableSubIntr
	.type	sceKernelEnableSubIntr, @function
sceKernelEnableSubIntr: /* InterruptManagerForKernel 0xfb8e22ec @ 0x7408 */
	jr	$ra
	nop
	.globl	sceKernelRegisterSubIntrHandler
	.type	sceKernelRegisterSubIntrHandler, @function
sceKernelRegisterSubIntrHandler: /* InterruptManagerForKernel 0xffa8b183 @ 0x7410 */
	jr	$ra
	nop
	.globl	Kprintf
	.type	Kprintf, @function
Kprintf: /* KDebugForKernel 0x84f370bc @ 0x7418 */
	jr	$ra
	nop
	.globl	sceKernelMemcpy
	.type	sceKernelMemcpy, @function
sceKernelMemcpy: /* SysMemForKernel 0x181065ab @ 0x7420 */
	jr	$ra
	nop
	.globl	sceKernelMemset
	.type	sceKernelMemset, @function
sceKernelMemset: /* SysMemForKernel 0x8ae776af @ 0x7428 */
	jr	$ra
	nop
	.globl	memset
	.type	memset, @function
memset: /* SysclibForKernel 0x10f3bb61 @ 0x7430 */
	jr	$ra
	nop
	.globl	memcpy
	.type	memcpy, @function
memcpy: /* SysclibForKernel 0xab7592ff @ 0x7438 */
	jr	$ra
	nop
	.globl	sceKernelSetEventFlag
	.type	sceKernelSetEventFlag, @function
sceKernelSetEventFlag: /* ThreadManForKernel 0x1fb15a32 @ 0x7440 */
	jr	$ra
	nop
	.globl	sceKernelDeleteSema
	.type	sceKernelDeleteSema, @function
sceKernelDeleteSema: /* ThreadManForKernel 0x28b6489c @ 0x7448 */
	jr	$ra
	nop
	.globl	sceKernelPollEventFlag
	.type	sceKernelPollEventFlag, @function
sceKernelPollEventFlag: /* ThreadManForKernel 0x30fd48f0 @ 0x7450 */
	jr	$ra
	nop
	.globl	sceKernelGetSystemTimeLow
	.type	sceKernelGetSystemTimeLow, @function
sceKernelGetSystemTimeLow: /* ThreadManForKernel 0x369ed59d @ 0x7458 */
	jr	$ra
	nop
	.globl	sceKernelSignalSema
	.type	sceKernelSignalSema, @function
sceKernelSignalSema: /* ThreadManForKernel 0x3f53e640 @ 0x7460 */
	jr	$ra
	nop
	.globl	sceKernelWaitEventFlag
	.type	sceKernelWaitEventFlag, @function
sceKernelWaitEventFlag: /* ThreadManForKernel 0x402fcf22 @ 0x7468 */
	jr	$ra
	nop
	.globl	sceKernelWaitSema
	.type	sceKernelWaitSema, @function
sceKernelWaitSema: /* ThreadManForKernel 0x4e3a1105 @ 0x7470 */
	jr	$ra
	nop
	.globl	sceKernelCreateEventFlag
	.type	sceKernelCreateEventFlag, @function
sceKernelCreateEventFlag: /* ThreadManForKernel 0x55c20a00 @ 0x7478 */
	jr	$ra
	nop
	.globl	sceKernelPollSema
	.type	sceKernelPollSema, @function
sceKernelPollSema: /* ThreadManForKernel 0x58b1f937 @ 0x7480 */
	jr	$ra
	nop
	.globl	sceKernelClearEventFlag
	.type	sceKernelClearEventFlag, @function
sceKernelClearEventFlag: /* ThreadManForKernel 0x812346e4 @ 0x7488 */
	jr	$ra
	nop
	.globl	sceKernelCreateMutex
	.type	sceKernelCreateMutex, @function
sceKernelCreateMutex: /* ThreadManForKernel 0xb7d098c6 @ 0x7490 */
	jr	$ra
	nop
	.globl	sceKernelCreateSema
	.type	sceKernelCreateSema, @function
sceKernelCreateSema: /* ThreadManForKernel 0xd6da4ba1 @ 0x7498 */
	jr	$ra
	nop
	.globl	sceKernelDeleteEventFlag
	.type	sceKernelDeleteEventFlag, @function
sceKernelDeleteEventFlag: /* ThreadManForKernel 0xef9e4c70 @ 0x74a0 */
	jr	$ra
	nop
	.globl	sceKernelDcacheWritebackInvalidateRange
	.type	sceKernelDcacheWritebackInvalidateRange, @function
sceKernelDcacheWritebackInvalidateRange: /* UtilsForKernel 0x34b9fa9e @ 0x74a8 */
	jr	$ra
	nop
	.globl	sceKernelDcacheWritebackRange
	.type	sceKernelDcacheWritebackRange, @function
sceKernelDcacheWritebackRange: /* UtilsForKernel 0x3ee30821 @ 0x74b0 */
	jr	$ra
	nop
	.globl	sceKernelDcacheWritebackAll
	.type	sceKernelDcacheWritebackAll, @function
sceKernelDcacheWritebackAll: /* UtilsForKernel 0x79d1c3fa @ 0x74b8 */
	jr	$ra
	nop
	.globl	sceKernelDcacheInvalidateRange
	.type	sceKernelDcacheInvalidateRange, @function
sceKernelDcacheInvalidateRange: /* UtilsForKernel 0xbfa98062 @ 0x74c0 */
	jr	$ra
	nop
	.globl	sceKermit_driver_36666181
	.type	sceKermit_driver_36666181, @function
sceKermit_driver_36666181: /* sceKermit_driver 0x36666181 @ 0x74c8 */
	jr	$ra
	nop
	.globl	sceKermit_driver_5280B410
	.type	sceKermit_driver_5280B410, @function
sceKermit_driver_5280B410: /* sceKermit_driver 0x5280b410 @ 0x74d0 */
	jr	$ra
	nop
	.globl	sceKermit_driver_D69C50BB
	.type	sceKermit_driver_D69C50BB, @function
sceKermit_driver_D69C50BB: /* sceKermit_driver 0xd69c50bb @ 0x74d8 */
	jr	$ra
	nop
	.globl	sceKernelRegisterSysEventHandler
	.type	sceKernelRegisterSysEventHandler, @function
sceKernelRegisterSysEventHandler: /* sceSysEventForKernel 0xcd9e4bb5 @ 0x74e0 */
	jr	$ra
	nop
	.globl	sceKernelUnregisterSysEventHandler
	.type	sceKernelUnregisterSysEventHandler, @function
sceKernelUnregisterSysEventHandler: /* sceSysEventForKernel 0xd7d3fdcd @ 0x74e8 */
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
	.half	178
	.word	.L_exp_tbl_1
	.word	0x00040000
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x4001
	.byte	4, 0
	.half	2
	.word	.L_exp_tbl_2
	.word	.L_exp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	5, 4
	.half	20
	.word	.L_exp_tbl_3
	.word	0x00020000
	.word	.L_exp_lib_4
	.byte	17, 0
	.half	0x0001
	.byte	4, 0
	.half	3
	.word	.L_exp_tbl_4
	.word	.L_exp_lib_5
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	20
	.word	.L_exp_tbl_5
	.word	0x00020000
	.word	.L_exp_lib_6
	.byte	17, 0
	.half	0x0001
	.byte	4, 0
	.half	14
	.word	.L_exp_tbl_6

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
	.word	sceKermit_driver_36666181
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
	.half	2
	.word	.L_imp_nid_2
	.word	sceKernelMemcpy
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_3
	.word	Kprintf
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0009
	.byte	6, 0
	.half	10
	.word	.L_imp_nid_4
	.word	sceKernelCpuSuspendIntr
	.word	0
	.word	.L_imp_lib_5
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_5
	.word	memset
	.word	.L_imp_lib_6
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	13
	.word	.L_imp_nid_6
	.word	sceKernelSetEventFlag
	.word	0
	.word	.L_imp_lib_7
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_7
	.word	sceKernelDmaRegisterDdrFlush
	.word	.L_imp_lib_8
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	4
	.word	.L_imp_nid_8
	.word	sceKernelDcacheWritebackInvalidateRange

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
	.asciz	"sceLowIO_Driver"
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
	.asciz	"sceKermit_driver"
	.space	7
.L_imp_lib_1:
	.asciz	"sceSysEventForKernel"
	.space	7
.L_imp_lib_2:
	.asciz	"SysMemForKernel"
	.space	4
.L_imp_lib_3:
	.asciz	"KDebugForKernel"
	.space	4
.L_imp_lib_4:
	.asciz	"InterruptManagerForKernel"
	.space	6
.L_imp_lib_5:
	.asciz	"SysclibForKernel"
	.space	7
.L_imp_lib_6:
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_7:
	.asciz	"DmacManForKernel"
	.space	7
.L_imp_lib_8:
	.asciz	"UtilsForKernel"
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
	.asciz	"sceSysreg_driver"
	.space	3
.L_exp_tbl_1:
	.word	0x0143e8a8
	.word	0x018f913a
	.word	0x0607a4c4
	.word	0x08fe40f5
	.word	0x092af6a9
	.word	0x0995f8f6
	.word	0x0ae8e549
	.word	0x0bbd4ed6
	.word	0x14eb1393
	.word	0x1561bcd2
	.word	0x15786501
	.word	0x15dc34bc
	.word	0x16909002
	.word	0x176e590a
	.word	0x187f651d
	.word	0x191d7461
	.word	0x19a6e54b
	.word	0x19f4c92d
	.word	0x1a27b224
	.word	0x1c4c4c7a
	.word	0x1d233ef9
	.word	0x1e111b75
	.word	0x1e62714e
	.word	0x1e881843
	.word	0x20388c9e
	.word	0x20b1d0a9
	.word	0x20df8278
	.word	0x231ee757
	.word	0x2458b6ac
	.word	0x258782a3
	.word	0x26fa0928
	.word	0x283d7a95
	.word	0x2d0f7755
	.word	0x2f9b03e0
	.word	0x30c0a141
	.word	0x32e02fdf
	.word	0x33ee43f0
	.word	0x35c23493
	.word	0x370419ad
	.word	0x37fbaca5
	.word	0x38527743
	.word	0x3bb0b2c8
	.word	0x3d2ce374
	.word	0x3e95ab4d
	.word	0x3f6f2cc7
	.word	0x41b0337b
	.word	0x434e8af1
	.word	0x44277d0d
	.word	0x4479c9bd
	.word	0x457feba9
	.word	0x4634c9dc
	.word	0x47c971b2
	.word	0x48124afe
	.word	0x4841b2d2
	.word	0x48cf8e69
	.word	0x48f1c4ad
	.word	0x4a433dc3
	.word	0x4b4cce80
	.word	0x4c0bed71
	.word	0x4c49a8bc
	.word	0x4db0c55d
	.word	0x4e5c86aa
	.word	0x4f46eede
	.word	0x51571e8f
	.word	0x518e3f29
	.word	0x52b74976
	.word	0x55373ee4
	.word	0x554e97f7
	.word	0x55ff02e9
	.word	0x56e95bb6
	.word	0x5d5118cd
	.word	0x5f179286
	.word	0x61fae917
	.word	0x63e1ee9c
	.word	0x6417cdd6
	.word	0x64c8e8dd
	.word	0x655c9cfc
	.word	0x6596ebc3
	.word	0x66899952
	.word	0x68ae6434
	.word	0x6a9b0426
	.word	0x6b3a3417
	.word	0x6c0ee043
	.word	0x6da9347d
	.word	0x6f3b6d7d
	.word	0x72887197
	.word	0x72c1ca96
	.word	0x73b3e52d
	.word	0x73ebc752
	.word	0x74c6f776
	.word	0x76e57dc6
	.word	0x7725ca08
	.word	0x7aa8a8be
	.word	0x7cf05e81
	.word	0x7dd0cbee
	.word	0x7e1b1f28
	.word	0x7fd7a631
	.word	0x803e5f37
	.word	0x80944c8a
	.word	0x84a279a4
	.word	0x84e0f197
	.word	0x85b74fda
	.word	0x85ba0c0b
	.word	0x866eeb74
	.word	0x867bd103
	.word	0x87b61303
	.word	0x8835d1e1
	.word	0x8b1dd83a
	.word	0x8b90b8b5
	.word	0x8c5c53de
	.word	0x8cfd0dca
	.word	0x8f4f4e96
	.word	0x9057c9e2
	.word	0x915b3772
	.word	0x9275dd37
	.word	0x9306f27b
	.word	0x93f96d8f
	.word	0x94b89638
	.word	0x95ca8aa1
	.word	0x96d74557
	.word	0x9746f3b2
	.word	0x9a6e7bb8
	.word	0x9b710d3c
	.word	0x9cd29d6c
	.word	0x9dd1f821
	.word	0x9e2f8fd5
	.word	0x9eb8c49e
	.word	0xa16e55f4
	.word	0xa3e4154c
	.word	0xa4706857
	.word	0xa5cc6025
	.word	0xa781b599
	.word	0xa7c82bdd
	.word	0xa9ee3124
	.word	0xaeb8dbd1
	.word	0xaec87dfd
	.word	0xafe47914
	.word	0xb1751b06
	.word	0xb413b041
	.word	0xb6296512
	.word	0xb6c10df0
	.word	0xb73d3619
	.word	0xbb6baa00
	.word	0xbb823481
	.word	0xbbc721ea
	.word	0xbd7b035b
	.word	0xbe03d832
	.word	0xbe1ff8bd
	.word	0xc11b5c0d
	.word	0xc1a37b37
	.word	0xc2e0e869
	.word	0xc2f3061f
	.word	0xc36775ad
	.word	0xc4c21cab
	.word	0xc60fafb4
	.word	0xc6c75585
	.word	0xc9585f8e
	.word	0xd3e23912
	.word	0xd3e8f2af
	.word	0xd507a82d
	.word	0xd6628a48
	.word	0xd7ad9705
	.word	0xd8e6cae0
	.word	0xda5b5ed9
	.word	0xdb97c70e
	.word	0xded12806
	.word	0xe2a5d1ee
	.word	0xe5764eac
	.word	0xeb5c723a
	.word	0xec03f6e2
	.word	0xf1924607
	.word	0xf288e58e
	.word	0xf4a3c03a
	.word	0xf97d9d73
	.word	0xf9c93dd4
	.word	0xfc0131a7
	.word	0xfc5cdd48
	.word	0xff0e07b1
	.word	sceSysregSemaTryLock /* sceSysregSemaTryLock (0x0143e8a8) */
	.word	sceSysregAtahddClkEnable /* sceSysregAtahddClkEnable (0x018f913a) */
	.word	sceSysreg_driver_0607A4C4 /* sceSysreg_driver_0607A4C4 (0x0607a4c4) */
	.word	sceSysregUsbhostClkEnable /* sceSysregUsbhostClkEnable (0x08fe40f5) */
	.word	sceSysregAudioClkDisable /* sceSysregAudioClkDisable (0x092af6a9) */
	.word	sceSysreg_driver_0995F8F6 /* sceSysreg_driver_0995F8F6 (0x0995f8f6) */
	.word	sceSysregAvcResetEnable /* sceSysregAvcResetEnable (0x0ae8e549) */
	.word	sceSysregEmcddrBusClockEnable /* sceSysregEmcddrBusClockEnable (0x0bbd4ed6) */
	.word	sceSysreg_driver_14EB1393 /* sceSysreg_driver_14EB1393 (0x14eb1393) */
	.word	sceSysreg_driver_1561BCD2 /* sceSysreg_driver_1561BCD2 (0x1561bcd2) */
	.word	sceSysreg_driver_15786501 /* sceSysreg_driver_15786501 (0x15786501) */
	.word	sceSysregGpioIoDisable /* sceSysregGpioIoDisable (0x15dc34bc) */
	.word	sceSysregAtaBusClockEnable /* sceSysregAtaBusClockEnable (0x16909002) */
	.word	sceSysregMsifIoDisable /* sceSysregMsifIoDisable (0x176e590a) */
	.word	sceSysregPllGetOutSelect /* sceSysregPllGetOutSelect (0x187f651d) */
	.word	sceSysregApbBusClockEnable /* sceSysregApbBusClockEnable (0x191d7461) */
	.word	sceSysregPllSetOutSelect /* sceSysregPllSetOutSelect (0x19a6e54b) */
	.word	sceSysreg_driver_19F4C92D /* sceSysreg_driver_19F4C92D (0x19f4c92d) */
	.word	sceSysregVmeResetEnable /* sceSysregVmeResetEnable (0x1a27b224) */
	.word	sceSysregEmcsmIoDisable /* sceSysregEmcsmIoDisable (0x1c4c4c7a) */
	.word	sceSysregUsbBusClockDisable /* sceSysregUsbBusClockDisable (0x1d233ef9) */
	.word	sceSysregMsifClkDisable /* sceSysregMsifClkDisable (0x1e111b75) */
	.word	sceSysreg_driver_1E62714E /* sceSysreg_driver_1E62714E (0x1e62714e) */
	.word	sceSysregIntrInit /* sceSysregIntrInit (0x1e881843) */
	.word	sceSysregSircsClkDisable /* sceSysregSircsClkDisable (0x20388c9e) */
	.word	sceSysreg_driver_20B1D0A9 /* sceSysreg_driver_20B1D0A9 (0x20b1d0a9) */
	.word	sceSysregMsifGetConnectStatus /* sceSysregMsifGetConnectStatus (0x20df8278) */
	.word	sceSysregPllUpdateFrequency /* sceSysregPllUpdateFrequency (0x231ee757) */
	.word	sceSysreg_driver_2458B6AC /* sceSysreg_driver_2458B6AC (0x2458b6ac) */
	.word	sceSysregAwEdramBusClockDisable /* sceSysregAwEdramBusClockDisable (0x258782a3) */
	.word	sceSysregSircsIoDisable /* sceSysregSircsIoDisable (0x26fa0928) */
	.word	sceSysregGpioClkEnable /* sceSysregGpioClkEnable (0x283d7a95) */
	.word	sceSysregEmcsmBusClockDisable /* sceSysregEmcsmBusClockDisable (0x2d0f7755) */
	.word	sceSysregKirkResetDisable /* sceSysregKirkResetDisable (0x2f9b03e0) */
	.word	sceSysregUsbResetEnable /* sceSysregUsbResetEnable (0x30c0a141) */
	.word	sceSysreg_driver_32E02FDF /* sceSysreg_driver_32E02FDF (0x32e02fdf) */
	.word	sceSysreg_driver_33EE43F0 /* sceSysreg_driver_33EE43F0 (0x33ee43f0) */
	.word	sceSysregUsbhostBusClockEnable /* sceSysregUsbhostBusClockEnable (0x35c23493) */
	.word	sceSysregMsifResetEnable /* sceSysregMsifResetEnable (0x370419ad) */
	.word	sceSysregGpioIoEnable /* sceSysregGpioIoEnable (0x37fbaca5) */
	.word	sceSysregMeBusClockEnable /* sceSysregMeBusClockEnable (0x38527743) */
	.word	sceSysreg_driver_3BB0B2C8 /* sceSysreg_driver_3BB0B2C8 (0x3bb0b2c8) */
	.word	sceSysregApbBusClockDisable /* sceSysregApbBusClockDisable (0x3d2ce374) */
	.word	sceSysregAtaIoEnable /* sceSysregAtaIoEnable (0x3e95ab4d) */
	.word	sceSysreg_driver_3F6F2CC7 /* sceSysreg_driver_3F6F2CC7 (0x3f6f2cc7) */
	.word	sceSysregAudioClkoutClkSelect /* sceSysregAudioClkoutClkSelect (0x41b0337b) */
	.word	sceSysreg_driver_434E8AF1 /* sceSysreg_driver_434E8AF1 (0x434e8af1) */
	.word	sceSysregAwRegBBusClockEnable /* sceSysregAwRegBBusClockEnable (0x44277d0d) */
	.word	sceSysregUsbhostBusClockDisable /* sceSysregUsbhostBusClockDisable (0x4479c9bd) */
	.word	sceSysregMeResetEnable /* sceSysregMeResetEnable (0x457feba9) */
	.word	sceSysregAudioIoEnable /* sceSysregAudioIoEnable (0x4634c9dc) */
	.word	sceSysregTopResetEnable /* sceSysregTopResetEnable (0x47c971b2) */
	.word	sceSysregMsifClkSelect /* sceSysregMsifClkSelect (0x48124afe) */
	.word	sceSysreg_driver_4841B2D2 /* sceSysreg_driver_4841B2D2 (0x4841b2d2) */
	.word	sceSysregAtahddClkSelect /* sceSysregAtahddClkSelect (0x48cf8e69) */
	.word	sceSysregMeResetDisable /* sceSysregMeResetDisable (0x48f1c4ad) */
	.word	sceSysregUsbhostResetEnable /* sceSysregUsbhostResetEnable (0x4a433dc3) */
	.word	sceSysregAudioClkoutIoEnable /* sceSysregAudioClkoutIoEnable (0x4b4cce80) */
	.word	sceSysreg_driver_4C0BED71 /* sceSysreg_driver_4C0BED71 (0x4c0bed71) */
	.word	sceSysregSircsIoEnable /* sceSysregSircsIoEnable (0x4c49a8bc) */
	.word	sceSysregMsifClkEnable /* sceSysregMsifClkEnable (0x4db0c55d) */
	.word	sceSysreg_driver_4E5C86AA /* sceSysreg_driver_4E5C86AA (0x4e5c86aa) */
	.word	sceSysregGetFuseId /* sceSysregGetFuseId (0x4f46eede) */
	.word	sceSysregAwRegABusClockEnable /* sceSysregAwRegABusClockEnable (0x51571e8f) */
	.word	sceSysregMsifIoEnable /* sceSysregMsifIoEnable (0x518e3f29) */
	.word	sceSysregAwRegABusClockDisable /* sceSysregAwRegABusClockDisable (0x52b74976) */
	.word	sceSysregAtahddClkDisable /* sceSysregAtahddClkDisable (0x55373ee4) */
	.word	sceSysreg_driver_554E97F7 /* sceSysreg_driver_554E97F7 (0x554e97f7) */
	.word	sceSysregAvcResetDisable /* sceSysregAvcResetDisable (0x55ff02e9) */
	.word	sceSysregMsifAcquireConnectIntr /* sceSysregMsifAcquireConnectIntr (0x56e95bb6) */
	.word	sceSysregAtahddIoEnable /* sceSysregAtahddIoEnable (0x5d5118cd) */
	.word	sceSysregAtahddBusClockDisable /* sceSysregAtahddBusClockDisable (0x5f179286) */
	.word	sceSysregInterruptToOther /* sceSysregInterruptToOther (0x61fae917) */
	.word	sceSysreg_driver_63E1EE9C /* sceSysreg_driver_63E1EE9C (0x63e1ee9c) */
	.word	sceSysregSircsClkEnable /* sceSysregSircsClkEnable (0x6417cdd6) */
	.word	sceSysregAtaResetEnable /* sceSysregAtaResetEnable (0x64c8e8dd) */
	.word	sceSysregScResetEnable /* sceSysregScResetEnable (0x655c9cfc) */
	.word	sceSysreg_driver_6596EBC3 /* sceSysreg_driver_6596EBC3 (0x6596ebc3) */
	.word	sceSysregAwResetEnable /* sceSysregAwResetEnable (0x66899952) */
	.word	sceSysreg_driver_68AE6434 /* sceSysreg_driver_68AE6434 (0x68ae6434) */
	.word	sceSysregAtaClkEnable /* sceSysregAtaClkEnable (0x6a9b0426) */
	.word	sceSysregPllGetBaseFrequency /* sceSysregPllGetBaseFrequency (0x6b3a3417) */
	.word	sceSysregUsbClkDisable /* sceSysregUsbClkDisable (0x6c0ee043) */
	.word	sceSysreg_driver_6DA9347D /* sceSysreg_driver_6DA9347D (0x6da9347d) */
	.word	sceSysreg_driver_6F3B6D7D /* sceSysreg_driver_6F3B6D7D (0x6f3b6d7d) */
	.word	sceSysreg_driver_72887197 /* sceSysreg_driver_72887197 (0x72887197) */
	.word	sceSysregUsbGetConnectStatus /* sceSysregUsbGetConnectStatus (0x72c1ca96) */
	.word	sceSysreg_driver_73B3E52D /* sceSysreg_driver_73B3E52D (0x73b3e52d) */
	.word	sceSysreg_driver_73EBC752 /* sceSysreg_driver_73EBC752 (0x73ebc752) */
	.word	sceSysregApbTimerClkEnable /* sceSysregApbTimerClkEnable (0x74c6f776) */
	.word	sceSysreg_driver_76E57DC6 /* sceSysreg_driver_76E57DC6 (0x76e57dc6) */
	.word	sceSysregUartClkEnable /* sceSysregUartClkEnable (0x7725ca08) */
	.word	sceSysregIntrEnd /* sceSysregIntrEnd (0x7aa8a8be) */
	.word	sceSysreg_driver_7CF05E81 /* sceSysreg_driver_7CF05E81 (0x7cf05e81) */
	.word	sceSysregMsifResetDisable /* sceSysregMsifResetDisable (0x7dd0cbee) */
	.word	sceSysregAwRegBBusClockDisable /* sceSysregAwRegBBusClockDisable (0x7e1b1f28) */
	.word	sceSysregUartIoEnable /* sceSysregUartIoEnable (0x7fd7a631) */
	.word	sceSysreg_driver_803E5F37 /* sceSysreg_driver_803E5F37 (0x803e5f37) */
	.word	sceSysreg_driver_80944C8A /* sceSysreg_driver_80944C8A (0x80944c8a) */
	.word	sceSysregUsbClkEnable /* sceSysregUsbClkEnable (0x84a279a4) */
	.word	sceSysreg_driver_84E0F197 /* sceSysreg_driver_84E0F197 (0x84e0f197) */
	.word	sceSysreg_driver_85B74FDA /* sceSysreg_driver_85B74FDA (0x85b74fda) */
	.word	sceSysregAudioBusClockDisable /* sceSysregAudioBusClockDisable (0x85ba0c0b) */
	.word	sceSysregAtahddResetEnable /* sceSysregAtahddResetEnable (0x866eeb74) */
	.word	sceSysregKirkBusClockDisable /* sceSysregKirkBusClockDisable (0x867bd103) */
	.word	sceSysreg_driver_87B61303 /* sceSysreg_driver_87B61303 (0x87b61303) */
	.word	sceSysregSpiClkEnable /* sceSysregSpiClkEnable (0x8835d1e1) */
	.word	sceSysregAudioBusClockEnable /* sceSysregAudioBusClockEnable (0x8b1dd83a) */
	.word	sceSysregSpiClkDisable /* sceSysregSpiClkDisable (0x8b90b8b5) */
	.word	sceSysregSpiIoEnable /* sceSysregSpiIoEnable (0x8c5c53de) */
	.word	sceSysregAtaResetDisable /* sceSysregAtaResetDisable (0x8cfd0dca) */
	.word	sceSysregGetFuseConfig /* sceSysregGetFuseConfig (0x8f4f4e96) */
	.word	sceSysregAtaClkSelect /* sceSysregAtaClkSelect (0x9057c9e2) */
	.word	sceSysregUsbhostAcquireIntr /* sceSysregUsbhostAcquireIntr (0x915b3772) */
	.word	sceSysreg_driver_9275DD37 /* sceSysreg_driver_9275DD37 (0x9275dd37) */
	.word	sceSysregUsbResetDisable /* sceSysregUsbResetDisable (0x9306f27b) */
	.word	sceSysregMsifBusClockEnable /* sceSysregMsifBusClockEnable (0x93f96d8f) */
	.word	sceSysregAtahddBusClockEnable /* sceSysregAtahddBusClockEnable (0x94b89638) */
	.word	sceSysregUsbIoEnable /* sceSysregUsbIoEnable (0x95ca8aa1) */
	.word	sceSysreg_driver_96D74557 /* sceSysreg_driver_96D74557 (0x96d74557) */
	.word	sceSysreg_driver_9746F3B2 /* sceSysreg_driver_9746F3B2 (0x9746f3b2) */
	.word	sceSysregUsbQueryIntr /* sceSysregUsbQueryIntr (0x9a6e7bb8) */
	.word	sceSysregUsbhostResetDisable /* sceSysregUsbhostResetDisable (0x9b710d3c) */
	.word	sceSysregSetMasterPriv /* sceSysregSetMasterPriv (0x9cd29d6c) */
	.word	sceSysregEmcsmIoEnable /* sceSysregEmcsmIoEnable (0x9dd1f821) */
	.word	sceSysreg_driver_9E2F8FD5 /* sceSysreg_driver_9E2F8FD5 (0x9e2f8fd5) */
	.word	sceSysregAtahddResetDisable /* sceSysregAtahddResetDisable (0x9eb8c49e) */
	.word	sceSysregSpiIoDisable /* sceSysregSpiIoDisable (0xa16e55f4) */
	.word	sceSysregUartClkDisable /* sceSysregUartClkDisable (0xa3e4154c) */
	.word	sceSysregApbTimerClkSelect /* sceSysregApbTimerClkSelect (0xa4706857) */
	.word	sceSysregPllGetFrequency /* sceSysregPllGetFrequency (0xa5cc6025) */
	.word	sceSysregUsbhostClkDisable /* sceSysregUsbhostClkDisable (0xa781b599) */
	.word	sceSysreg_driver_A7C82BDD /* sceSysreg_driver_A7C82BDD (0xa7c82bdd) */
	.word	sceSysregGpioClkDisable /* sceSysregGpioClkDisable (0xa9ee3124) */
	.word	sceSysregAwResetDisable /* sceSysregAwResetDisable (0xaeb8dbd1) */
	.word	sceSysregSetAwEdramSize /* sceSysregSetAwEdramSize (0xaec87dfd) */
	.word	sceSysregDoTimerEvent /* sceSysregDoTimerEvent (0xafe47914) */
	.word	sceSysregAudioClkEnable /* sceSysregAudioClkEnable (0xb1751b06) */
	.word	sceSysreg_driver_B413B041 /* sceSysreg_driver_B413B041 (0xb413b041) */
	.word	sceSysreg_driver_B6296512 /* sceSysreg_driver_B6296512 (0xb6296512) */
	.word	sceSysregAtaBusClockDisable /* sceSysregAtaBusClockDisable (0xb6c10df0) */
	.word	sceSysregVmeResetDisable /* sceSysregVmeResetDisable (0xb73d3619) */
	.word	sceSysregMsifQueryConnectIntr /* sceSysregMsifQueryConnectIntr (0xbb6baa00) */
	.word	sceSysregUartIoDisable /* sceSysregUartIoDisable (0xbb823481) */
	.word	sceSysregKirkBusClockEnable /* sceSysregKirkBusClockEnable (0xbbc721ea) */
	.word	sceSysreg_driver_BD7B035B /* sceSysreg_driver_BD7B035B (0xbd7b035b) */
	.word	sceSysregAudioClkoutClkEnable /* sceSysregAudioClkoutClkEnable (0xbe03d832) */
	.word	sceSysreg_driver_BE1FF8BD /* sceSysreg_driver_BE1FF8BD (0xbe1ff8bd) */
	.word	sceSysregUsbIoDisable /* sceSysregUsbIoDisable (0xc11b5c0d) */
	.word	sceSysregKirkResetEnable /* sceSysregKirkResetEnable (0xc1a37b37) */
	.word	sceSysregAwEdramBusClockEnable /* sceSysregAwEdramBusClockEnable (0xc2e0e869) */
	.word	sceSysreg_driver_C2F3061F /* sceSysreg_driver_C2F3061F (0xc2f3061f) */
	.word	sceSysregAudioClkSelect /* sceSysregAudioClkSelect (0xc36775ad) */
	.word	sceSysregMeBusClockDisable /* sceSysregMeBusClockDisable (0xc4c21cab) */
	.word	sceSysregAudioIoDisable /* sceSysregAudioIoDisable (0xc60fafb4) */
	.word	sceSysreg_driver_C6C75585 /* sceSysreg_driver_C6C75585 (0xc6c75585) */
	.word	sceSysreg_driver_C9585F8E /* sceSysreg_driver_C9585F8E (0xc9585f8e) */
	.word	sceSysregUsbhostQueryIntr /* sceSysregUsbhostQueryIntr (0xd3e23912) */
	.word	sceSysregMsifDelaySelect /* sceSysregMsifDelaySelect (0xd3e8f2af) */
	.word	sceSysregAudioClkoutClkDisable /* sceSysregAudioClkoutClkDisable (0xd507a82d) */
	.word	sceSysregSpiClkSelect /* sceSysregSpiClkSelect (0xd6628a48) */
	.word	sceSysregUsbBusClockEnable /* sceSysregUsbBusClockEnable (0xd7ad9705) */
	.word	sceSysregRequestIntr /* sceSysregRequestIntr (0xd8e6cae0) */
	.word	sceSysreg_driver_DA5B5ED9 /* sceSysreg_driver_DA5B5ED9 (0xda5b5ed9) */
	.word	sceSysregSemaUnlock /* sceSysregSemaUnlock (0xdb97c70e) */
	.word	sceSysregApbTimerClkDisable /* sceSysregApbTimerClkDisable (0xded12806) */
	.word	sceSysregGetTachyonVersion /* sceSysregGetTachyonVersion (0xe2a5d1ee) */
	.word	sceSysregAudioClkoutIoDisable /* sceSysregAudioClkoutIoDisable (0xe5764eac) */
	.word	sceSysregAtaIoDisable /* sceSysregAtaIoDisable (0xeb5c723a) */
	.word	sceSysregUsbAcquireIntr /* sceSysregUsbAcquireIntr (0xec03f6e2) */
	.word	sceSysregEmcddrBusClockDisable /* sceSysregEmcddrBusClockDisable (0xf1924607) */
	.word	sceSysregMsifBusClockDisable /* sceSysregMsifBusClockDisable (0xf288e58e) */
	.word	sceSysregAtahddIoDisable /* sceSysregAtahddIoDisable (0xf4a3c03a) */
	.word	sceSysregEmcsmBusClockEnable /* sceSysregEmcsmBusClockEnable (0xf97d9d73) */
	.word	sceSysreg_driver_F9C93DD4 /* sceSysreg_driver_F9C93DD4 (0xf9c93dd4) */
	.word	sceSysreg_driver_FC0131A7 /* sceSysreg_driver_FC0131A7 (0xfc0131a7) */
	.word	sceSysregAtaClkDisable /* sceSysregAtaClkDisable (0xfc5cdd48) */
	.word	sceSysreg_driver_FF0E07B1 /* sceSysreg_driver_FF0E07B1 (0xff0e07b1) */
	.word	0x00020000
	.word	0x00080003
	.word	0x00080008
	.word	0x0013000e
	.word	0x001c0018
	.word	0x0020001f
	.word	0x00250022
	.word	0x002a0028
	.word	0x002f002d
	.word	0x003a0034
	.word	0x0042003f
	.word	0x00460046
	.word	0x004a0048
	.word	0x0052004f
	.word	0x00590055
	.word	0x005d005c
	.word	0x00630061
	.word	0x006d006a
	.word	0x00750070
	.word	0x007b0079
	.word	0x0081007f
	.word	0x00860085
	.word	0x008a0089
	.word	0x0091008e
	.word	0x00990094
	.word	0x009d009c
	.word	0x009f009d
	.word	0x00a500a2
	.word	0x00a700a6
	.word	0x00a900a8
	.word	0x00ac00aa
	.word	0x00af00ad
.L_exp_lib_2:
	.asciz	"sceDmac"
	.align	2
.L_exp_tbl_2:
	.word	0x617f3fe6
	.word	0xd97f94d8
	.word	sceDmacplus_driver_2C5FC1E3 /* sceDmacMemcpy (0x617f3fe6) */
	.word	sceDmacplus_driver_D57D5BC2 /* sceDmacTryMemcpy (0xd97f94d8) */
.L_exp_lib_3:
	.asciz	"sceDmacplus_driver"
	.space	1
.L_exp_tbl_3:
	.word	0x282ca0d7
	.word	0x2c5fc1e3
	.word	0x33cd36dd
	.word	0x47d400cb
	.word	0x5fcf43bd
	.word	0x6945f1d3
	.word	0x88acb6f1
	.word	0x9c492b9b
	.word	0x9ebf7d3e
	.word	0xa3aa8d00
	.word	0xb0ec610d
	.word	0xb269eac9
	.word	0xbf0db45e
	.word	0xcccf6069
	.word	0xd126494b
	.word	0xd57d5bc2
	.word	0xdbd8526f
	.word	0xe9b746f9
	.word	0xed849260
	.word	0xf652f09d
	.word	0x374689b0
	.word	0x9d3dddf0
	.word	0xaa2e7e54
	.word	0xc10b947a
	.word	sceDmacplus_driver_282CA0D7 /* sceDmacplus_driver_282CA0D7 (0x282ca0d7) */
	.word	sceDmacplus_driver_2C5FC1E3 /* sceDmacplus_driver_2C5FC1E3 (0x2c5fc1e3) */
	.word	sceDmacplus_driver_33CD36DD /* sceDmacplus_driver_33CD36DD (0x33cd36dd) */
	.word	sceDmacplus_driver_47D400CB /* sceDmacplus_driver_47D400CB (0x47d400cb) */
	.word	sceDmacplus_driver_5FCF43BD /* sceDmacplus_driver_5FCF43BD (0x5fcf43bd) */
	.word	sceDmacplus_driver_6945F1D3 /* sceDmacplus_driver_6945F1D3 (0x6945f1d3) */
	.word	sceDmacplus_driver_88ACB6F1 /* sceDmacplus_driver_88ACB6F1 (0x88acb6f1) */
	.word	sceDmacplus_driver_9C492B9B /* sceDmacplus_driver_9C492B9B (0x9c492b9b) */
	.word	sceDmacplus_driver_9EBF7D3E /* sceDmacplus_driver_9EBF7D3E (0x9ebf7d3e) */
	.word	sceDmacplus_driver_A3AA8D00 /* sceDmacplus_driver_A3AA8D00 (0xa3aa8d00) */
	.word	sceDmacplus_driver_B0EC610D /* sceDmacplus_driver_B0EC610D (0xb0ec610d) */
	.word	sceDmacplus_driver_B269EAC9 /* sceDmacplus_driver_B269EAC9 (0xb269eac9) */
	.word	sceDmacplus_driver_BF0DB45E /* sceDmacplus_driver_BF0DB45E (0xbf0db45e) */
	.word	sceDmacplus_driver_CCCF6069 /* sceDmacplus_driver_CCCF6069 (0xcccf6069) */
	.word	sceDmacplus_driver_D126494B /* sceDmacplus_driver_D126494B (0xd126494b) */
	.word	sceDmacplus_driver_D57D5BC2 /* sceDmacplus_driver_D57D5BC2 (0xd57d5bc2) */
	.word	sceDmacplus_driver_DBD8526F /* sceDmacplus_driver_DBD8526F (0xdbd8526f) */
	.word	sceDmacplus_driver_E9B746F9 /* sceDmacplus_driver_E9B746F9 (0xe9b746f9) */
	.word	sceDmacplus_driver_ED849260 /* sceDmacplus_driver_ED849260 (0xed849260) */
	.word	sceDmacplus_driver_F652F09D /* sceDmacplus_driver_F652F09D (0xf652f09d) */
	.word	sceDmacplus_driver_var_374689B0 /* sceDmacplus_driver_var_374689B0 (0x374689b0) */
	.word	sceDmacplus_driver_var_9D3DDDF0 /* sceDmacplus_driver_var_9D3DDDF0 (0x9d3dddf0) */
	.word	sceDmacplus_driver_var_AA2E7E54 /* sceDmacplus_driver_var_AA2E7E54 (0xaa2e7e54) */
	.word	sceDmacplus_driver_var_C10B947A /* sceDmacplus_driver_var_C10B947A (0xc10b947a) */
	.word	0x00000000
	.word	0x00020000
	.word	0x00040003
	.word	0x00060005
	.word	0x00070006
	.word	0x000a0009
	.word	0x000e000d
	.word	0x00130011
.L_exp_lib_4:
	.asciz	"sceKermitMemory_driver"
	.space	1
.L_exp_tbl_4:
	.word	0x80e1240a
	.word	0x90b662d0
	.word	0xaaf047ac
	.word	sceKermitMemory_driver_80E1240A /* sceKermitMemory_driver_80E1240A (0x80e1240a) */
	.word	sceKermitMemory_driver_90B662D0 /* sceKermitMemory_driver_90B662D0 (0x90b662d0) */
	.word	sceKermitMemory_driver_AAF047AC /* sceKermitMemory_driver_AAF047AC (0xaaf047ac) */
.L_exp_lib_5:
	.asciz	"sceLcdc_driver"
	.space	1
.L_exp_tbl_5:
	.word	0x0bff31c5
	.word	0x1f01bd08
	.word	0x2d80bb2e
	.word	0x35f45b50
	.word	0x3edbfcb9
	.word	0x451fe1a1
	.word	0x525e7eb1
	.word	0x56307dbc
	.word	0x5df2f52a
	.word	0x75c4da9a
	.word	0x88c3d167
	.word	0x90aa786c
	.word	0x94f83f74
	.word	0x9d4fb8b1
	.word	0xa3dcef64
	.word	0xbaf165ce
	.word	0xbb839185
	.word	0xc004bbe0
	.word	0xc943b9a2
	.word	0xe9dbd35f
	.word	sceLcdc_driver_0BFF31C5 /* sceLcdc_driver_0BFF31C5 (0x0bff31c5) */
	.word	sceLcdc_driver_1F01BD08 /* sceLcdc_driver_1F01BD08 (0x1f01bd08) */
	.word	sceLcdc_driver_2D80BB2E /* sceLcdc_driver_2D80BB2E (0x2d80bb2e) */
	.word	sceLcdc_driver_35F45B50 /* sceLcdc_driver_35F45B50 (0x35f45b50) */
	.word	sceLcdc_driver_3EDBFCB9 /* sceLcdc_driver_3EDBFCB9 (0x3edbfcb9) */
	.word	sceLcdc_driver_451FE1A1 /* sceLcdc_driver_451FE1A1 (0x451fe1a1) */
	.word	sceLcdc_driver_525E7EB1 /* sceLcdc_driver_525E7EB1 (0x525e7eb1) */
	.word	sceLcdc_driver_56307DBC /* sceLcdc_driver_56307DBC (0x56307dbc) */
	.word	sceLcdc_driver_5DF2F52A /* sceLcdc_driver_5DF2F52A (0x5df2f52a) */
	.word	sceLcdc_driver_75C4DA9A /* sceLcdc_driver_75C4DA9A (0x75c4da9a) */
	.word	sceLcdc_driver_88C3D167 /* sceLcdc_driver_88C3D167 (0x88c3d167) */
	.word	sceLcdc_driver_90AA786C /* sceLcdc_driver_90AA786C (0x90aa786c) */
	.word	sceLcdc_driver_94F83F74 /* sceLcdc_driver_94F83F74 (0x94f83f74) */
	.word	sceLcdc_driver_9D4FB8B1 /* sceLcdc_driver_9D4FB8B1 (0x9d4fb8b1) */
	.word	sceLcdc_driver_A3DCEF64 /* sceLcdc_driver_A3DCEF64 (0xa3dcef64) */
	.word	sceLcdc_driver_BAF165CE /* sceLcdc_driver_BAF165CE (0xbaf165ce) */
	.word	sceLcdc_driver_BB839185 /* sceLcdc_driver_BB839185 (0xbb839185) */
	.word	sceLcdc_driver_C004BBE0 /* sceLcdc_driver_C004BBE0 (0xc004bbe0) */
	.word	sceLcdc_driver_C943B9A2 /* sceLcdc_driver_C943B9A2 (0xc943b9a2) */
	.word	sceLcdc_driver_E9DBD35F /* sceLcdc_driver_E9DBD35F (0xe9dbd35f) */
	.word	0x00010000
	.word	0x00030002
	.word	0x00060005
	.word	0x00090009
	.word	0x000b000a
	.word	0x000f000e
	.word	0x00130011
	.word	0x00140013
.L_exp_lib_6:
	.asciz	"sceDdr_driver"
	.space	2
.L_exp_tbl_6:
	.word	0x00e36648
	.word	0x0baae4c5
	.word	0x2aa39a80
	.word	0x397756c0
	.word	0x3d50dec9
	.word	0x4f30bfe8
	.word	0x6de74d80
	.word	0x77cd1fb3
	.word	0x87d86769
	.word	0x95b9a692
	.word	0x9f882141
	.word	0xcf07b9e2
	.word	0xe0a39d3e
	.word	0xf1005384
	.word	sceDdr_driver_00E36648 /* sceDdr_driver_00E36648 (0x00e36648) */
	.word	sceDdr_driver_0BAAE4C5 /* sceDdr_driver_0BAAE4C5 (0x0baae4c5) */
	.word	sceDdr_driver_2AA39A80 /* sceDdr_driver_2AA39A80 (0x2aa39a80) */
	.word	sceDdr_driver_397756C0 /* sceDdr_driver_397756C0 (0x397756c0) */
	.word	sceDdr_driver_3D50DEC9 /* sceDdr_driver_3D50DEC9 (0x3d50dec9) */
	.word	sceDdr_driver_4F30BFE8 /* sceDdr_driver_4F30BFE8 (0x4f30bfe8) */
	.word	sceDdr_driver_6DE74D80 /* sceDdr_driver_6DE74D80 (0x6de74d80) */
	.word	sceDdr_driver_77CD1FB3 /* sceDdr_driver_77CD1FB3 (0x77cd1fb3) */
	.word	sceDdrFlush /* sceDdrFlush (0x87d86769) */
	.word	sceDdr_driver_95B9A692 /* sceDdr_driver_95B9A692 (0x95b9a692) */
	.word	sceDdr_driver_9F882141 /* sceDdr_driver_9F882141 (0x9f882141) */
	.word	sceDdr_driver_CF07B9E2 /* sceDdr_driver_CF07B9E2 (0xcf07b9e2) */
	.word	sceDdr_driver_E0A39D3E /* sceDdr_driver_E0A39D3E (0xe0a39d3e) */
	.word	sceDdr_driver_F1005384 /* sceDdr_driver_F1005384 (0xf1005384) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x36666181
	.word	0x5280b410
	.word	0xd69c50bb
.L_imp_nid_1:
	.word	0xcd9e4bb5
	.word	0xd7d3fdcd
.L_imp_nid_2:
	.word	0x181065ab
	.word	0x8ae776af
.L_imp_nid_3:
	.word	0x84f370bc
.L_imp_nid_4:
	.word	0x092968f4
	.word	0x0c5f7ae3
	.word	0x3b84732d
	.word	0x4d6e7305
	.word	0x58dd8978
	.word	0x5f10d406
	.word	0xd61e6961
	.word	0xf987b1f0
	.word	0xfb8e22ec
	.word	0xffa8b183
.L_imp_nid_5:
	.word	0x10f3bb61
	.word	0xab7592ff
.L_imp_nid_6:
	.word	0x1fb15a32
	.word	0x28b6489c
	.word	0x30fd48f0
	.word	0x369ed59d
	.word	0x3f53e640
	.word	0x402fcf22
	.word	0x4e3a1105
	.word	0x55c20a00
	.word	0x58b1f937
	.word	0x812346e4
	.word	0xb7d098c6
	.word	0xd6da4ba1
	.word	0xef9e4c70
.L_imp_nid_7:
	.word	0x32757c57
.L_imp_nid_8:
	.word	0x34b9fa9e
	.word	0x3ee30821
	.word	0x79d1c3fa
	.word	0xbfa98062

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
