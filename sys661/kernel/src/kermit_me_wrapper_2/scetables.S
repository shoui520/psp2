/* Generated PSP module import/export tables for sceMeCodecWrapper */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelCpuSuspendIntr
	.type	sceKernelCpuSuspendIntr, @function
sceKernelCpuSuspendIntr: /* InterruptManagerForKernel 0x092968f4 @ 0x296c */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntr
	.type	sceKernelCpuResumeIntr, @function
sceKernelCpuResumeIntr: /* InterruptManagerForKernel 0x5f10d406 @ 0x2974 */
	jr	$ra
	nop
	.globl	sceIoOpen
	.type	sceIoOpen, @function
sceIoOpen: /* IoFileMgrForKernel 0x109f50bc @ 0x297c */
	jr	$ra
	nop
	.globl	sceIoLseek
	.type	sceIoLseek, @function
sceIoLseek: /* IoFileMgrForKernel 0x27eb27b8 @ 0x2984 */
	jr	$ra
	nop
	.globl	sceIoRead
	.type	sceIoRead, @function
sceIoRead: /* IoFileMgrForKernel 0x6a638d83 @ 0x298c */
	jr	$ra
	nop
	.globl	sceIoClose
	.type	sceIoClose, @function
sceIoClose: /* IoFileMgrForKernel 0x810c4bc3 @ 0x2994 */
	jr	$ra
	nop
	.globl	Kprintf
	.type	Kprintf, @function
Kprintf: /* KDebugForKernel 0x84f370bc @ 0x299c */
	jr	$ra
	nop
	.globl	sceKernelMemset32
	.type	sceKernelMemset32, @function
sceKernelMemset32: /* SysMemForKernel 0x22a114dc @ 0x29a4 */
	jr	$ra
	nop
	.globl	sceKernelMemset
	.type	sceKernelMemset, @function
sceKernelMemset: /* SysMemForKernel 0x8ae776af @ 0x29ac */
	jr	$ra
	nop
	.globl	sceKernelGetCompiledSdkVersion
	.type	sceKernelGetCompiledSdkVersion, @function
sceKernelGetCompiledSdkVersion: /* SysMemForKernel 0xb4f00cb5 @ 0x29b4 */
	jr	$ra
	nop
	.globl	memcmp
	.type	memcmp, @function
memcmp: /* SysclibForKernel 0x81d0d1f7 @ 0x29bc */
	jr	$ra
	nop
	.globl	memcpy
	.type	memcpy, @function
memcpy: /* SysclibForKernel 0xab7592ff @ 0x29c4 */
	jr	$ra
	nop
	.globl	sceKernelTryLockMutex
	.type	sceKernelTryLockMutex, @function
sceKernelTryLockMutex: /* ThreadManForKernel 0x0ddcd2c9 @ 0x29cc */
	jr	$ra
	nop
	.globl	sceKernelUnlockMutex
	.type	sceKernelUnlockMutex, @function
sceKernelUnlockMutex: /* ThreadManForKernel 0x6b30100f @ 0x29d4 */
	jr	$ra
	nop
	.globl	sceKernelGetSystemTimeWide
	.type	sceKernelGetSystemTimeWide, @function
sceKernelGetSystemTimeWide: /* ThreadManForKernel 0x82bc5777 @ 0x29dc */
	jr	$ra
	nop
	.globl	sceKernelLockMutex
	.type	sceKernelLockMutex, @function
sceKernelLockMutex: /* ThreadManForKernel 0xb011b11f @ 0x29e4 */
	jr	$ra
	nop
	.globl	sceKernelCreateMutex
	.type	sceKernelCreateMutex, @function
sceKernelCreateMutex: /* ThreadManForKernel 0xb7d098c6 @ 0x29ec */
	jr	$ra
	nop
	.globl	sceKernelDelayThread
	.type	sceKernelDelayThread, @function
sceKernelDelayThread: /* ThreadManForKernel 0xceadeb47 @ 0x29f4 */
	jr	$ra
	nop
	.globl	sceKernelDcacheWritebackInvalidateRange
	.type	sceKernelDcacheWritebackInvalidateRange, @function
sceKernelDcacheWritebackInvalidateRange: /* UtilsForKernel 0x34b9fa9e @ 0x29fc */
	jr	$ra
	nop
	.globl	UtilsForKernel_6C6887EE
	.type	UtilsForKernel_6C6887EE, @function
UtilsForKernel_6C6887EE: /* UtilsForKernel 0x6c6887ee @ 0x2a04 */
	jr	$ra
	nop
	.globl	sceKernelIcacheInvalidateAll
	.type	sceKernelIcacheInvalidateAll, @function
sceKernelIcacheInvalidateAll: /* UtilsForKernel 0x920f104a @ 0x2a0c */
	jr	$ra
	nop
	.globl	sceKernelDcacheWritebackInvalidateAll
	.type	sceKernelDcacheWritebackInvalidateAll, @function
sceKernelDcacheWritebackInvalidateAll: /* UtilsForKernel 0xb435dec5 @ 0x2a14 */
	jr	$ra
	nop
	.globl	sceKernelDcacheInvalidateRange
	.type	sceKernelDcacheInvalidateRange, @function
sceKernelDcacheInvalidateRange: /* UtilsForKernel 0xbfa98062 @ 0x2a1c */
	jr	$ra
	nop
	.globl	sceDdrFlush
	.type	sceDdrFlush, @function
sceDdrFlush: /* sceDdr_driver 0x87d86769 @ 0x2a24 */
	jr	$ra
	nop
	.globl	sceKermitMemory_driver_80E1240A
	.type	sceKermitMemory_driver_80E1240A, @function
sceKermitMemory_driver_80E1240A: /* sceKermitMemory_driver 0x80e1240a @ 0x2a2c */
	jr	$ra
	nop
	.globl	sceKermitMemory_driver_90B662D0
	.type	sceKermitMemory_driver_90B662D0, @function
sceKermitMemory_driver_90B662D0: /* sceKermitMemory_driver 0x90b662d0 @ 0x2a34 */
	jr	$ra
	nop
	.globl	sceKermit_driver_36666181
	.type	sceKermit_driver_36666181, @function
sceKermit_driver_36666181: /* sceKermit_driver 0x36666181 @ 0x2a3c */
	jr	$ra
	nop
	.globl	sceKernelRegisterSysEventHandler
	.type	sceKernelRegisterSysEventHandler, @function
sceKernelRegisterSysEventHandler: /* sceSysEventForKernel 0xcd9e4bb5 @ 0x2a44 */
	jr	$ra
	nop
	.globl	sceKernelUnregisterSysEventHandler
	.type	sceKernelUnregisterSysEventHandler, @function
sceKernelUnregisterSysEventHandler: /* sceSysEventForKernel 0xd7d3fdcd @ 0x2a4c */
	jr	$ra
	nop
	.globl	sceSysregAvcResetEnable
	.type	sceSysregAvcResetEnable, @function
sceSysregAvcResetEnable: /* sceSysreg_driver 0x0ae8e549 @ 0x2a54 */
	jr	$ra
	nop
	.globl	sceSysregVmeResetEnable
	.type	sceSysregVmeResetEnable, @function
sceSysregVmeResetEnable: /* sceSysreg_driver 0x1a27b224 @ 0x2a5c */
	jr	$ra
	nop
	.globl	sceSysregMeBusClockEnable
	.type	sceSysregMeBusClockEnable, @function
sceSysregMeBusClockEnable: /* sceSysreg_driver 0x38527743 @ 0x2a64 */
	jr	$ra
	nop
	.globl	sceSysregMeResetEnable
	.type	sceSysregMeResetEnable, @function
sceSysregMeResetEnable: /* sceSysreg_driver 0x457feba9 @ 0x2a6c */
	jr	$ra
	nop
	.globl	sceSysregMeResetDisable
	.type	sceSysregMeResetDisable, @function
sceSysregMeResetDisable: /* sceSysreg_driver 0x48f1c4ad @ 0x2a74 */
	jr	$ra
	nop
	.globl	sceSysregAvcResetDisable
	.type	sceSysregAvcResetDisable, @function
sceSysregAvcResetDisable: /* sceSysreg_driver 0x55ff02e9 @ 0x2a7c */
	jr	$ra
	nop
	.globl	sceSysregVmeResetDisable
	.type	sceSysregVmeResetDisable, @function
sceSysregVmeResetDisable: /* sceSysreg_driver 0xb73d3619 @ 0x2a84 */
	jr	$ra
	nop
	.globl	sceSysregGetTachyonVersion
	.type	sceSysregGetTachyonVersion, @function
sceSysregGetTachyonVersion: /* sceSysreg_driver 0xe2a5d1ee @ 0x2a8c */
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
	.half	23
	.word	.L_exp_tbl_1
	.word	0x00020000
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	4, 0
	.half	7
	.word	.L_exp_tbl_2
	.word	.L_exp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	4, 0
	.half	5
	.word	.L_exp_tbl_3
	.word	.L_exp_lib_4
	.byte	17, 0
	.half	0x0001
	.byte	4, 0
	.half	3
	.word	.L_exp_tbl_4
	.word	.L_exp_lib_5
	.byte	17, 0
	.half	0x0001
	.byte	4, 4
	.half	6
	.word	.L_exp_tbl_5
	.word	.L_exp_lib_6
	.byte	17, 0
	.half	0x0001
	.byte	4, 0
	.half	5
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
	.half	8
	.word	.L_imp_nid_0
	.word	sceSysregAvcResetEnable
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	6, 2
	.half	0
	.word	0
	.word	0
	.word	.L_imp_vstub_1
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_2
	.word	sceKermit_driver_36666181
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_3
	.word	sceKermitMemory_driver_80E1240A
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	5
	.word	.L_imp_nid_4
	.word	sceKernelDcacheWritebackInvalidateRange
	.word	.L_imp_lib_5
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	4
	.word	.L_imp_nid_5
	.word	sceIoOpen
	.word	0
	.word	.L_imp_lib_6
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_6
	.word	sceKernelRegisterSysEventHandler
	.word	.L_imp_lib_7
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_7
	.word	memcmp
	.word	.L_imp_lib_8
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	3
	.word	.L_imp_nid_8
	.word	sceKernelMemset32
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
	.half	0x0001
	.byte	6, 0
	.half	6
	.word	.L_imp_nid_10
	.word	sceKernelTryLockMutex
	.word	0
	.word	.L_imp_lib_11
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_11
	.word	Kprintf
	.word	.L_imp_lib_12
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_12
	.word	sceDdrFlush

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
	.asciz	"sceMeCodecWrapper"
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
	.asciz	"sceSysreg_driver"
	.space	7
.L_imp_lib_1:
	.asciz	"sceDmacplus_driver"
	.space	5
.L_imp_lib_2:
	.asciz	"sceKermit_driver"
	.space	7
.L_imp_lib_3:
	.asciz	"sceKermitMemory_driver"
	.space	5
.L_imp_lib_4:
	.asciz	"UtilsForKernel"
	.space	5
.L_imp_lib_5:
	.asciz	"IoFileMgrForKernel"
	.space	5
.L_imp_lib_6:
	.asciz	"sceSysEventForKernel"
	.space	7
.L_imp_lib_7:
	.asciz	"SysclibForKernel"
	.space	7
.L_imp_lib_8:
	.asciz	"SysMemForKernel"
	.space	4
.L_imp_lib_9:
	.asciz	"InterruptManagerForKernel"
	.space	6
.L_imp_lib_10:
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_11:
	.asciz	"KDebugForKernel"
	.space	4
.L_imp_lib_12:
	.asciz	"sceDdr_driver"
	.space	2
.L_exp_tbl_0:
	.word	0xadf12745
	.word	0xd632acdb
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_reboot_phase /* module_reboot_phase (0xadf12745) */
	.word	sceMeWrapperInit /* module_start (0xd632acdb) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"sceMeWrapper_driver"
	.align	2
.L_exp_tbl_1:
	.word	0x0defa6a5
	.word	0x1862b784
	.word	0x21521be5
	.word	0x24317cd0
	.word	0x4d78330c
	.word	0x5f6bf6df
	.word	0x635397bb
	.word	0x6ad33f60
	.word	0x6d68b223
	.word	0x6ed69327
	.word	0x81956a0b
	.word	0x8768915d
	.word	0x8dd56014
	.word	0x92d3baa1
	.word	0x984e2608
	.word	0x9a9e21ee
	.word	0xb37562aa
	.word	0xc300d466
	.word	0xc441994c
	.word	0xc4eda9f4
	.word	0xe8cd3c75
	.word	0xe9f69acf
	.word	0xfa398d71
	.word	sceMePowerSelectAvcClock /* sceMePowerSelectAvcClock (0x0defa6a5) */
	.word	sceMePower_driver_1862B784 /* sceMePower_driver_1862B784 (0x1862b784) */
	.word	sceMeVideo_driver_21521BE5 /* sceMeVideo_driver_21521BE5 (0x21521be5) */
	.word	sceMeWrapperInit /* sceMeWrapperInit (0x24317cd0) */
	.word	sceMeVideo_driver_4D78330C /* sceMeVideo_driver_4D78330C (0x4d78330c) */
	.word	sceMeWrapperEnd /* sceMeWrapperEnd (0x5f6bf6df) */
	.word	sceMeCore_driver_635397BB /* sceMeCore_driver_635397BB (0x635397bb) */
	.word	sceMeAudio_driver_6AD33F60 /* sceMeAudio_driver_6AD33F60 (0x6ad33f60) */
	.word	sceMeVideo_driver_6D68B223 /* sceMeVideo_driver_6D68B223 (0x6d68b223) */
	.word	sceMeFree /* sceMeFree (0x6ed69327) */
	.word	sceMeAudio_driver_81956A0B /* sceMeAudio_driver_81956A0B (0x81956a0b) */
	.word	sceMeVideo_driver_8768915D /* sceMeVideo_driver_8768915D (0x8768915d) */
	.word	sceMeVideo_driver_8DD56014 /* sceMeVideo_driver_8DD56014 (0x8dd56014) */
	.word	sceMeMalloc /* sceMeMalloc (0x92d3baa1) */
	.word	sceMePower_driver_984E2608 /* sceMePower_driver_984E2608 (0x984e2608) */
	.word	sceMeAudio_driver_9A9E21EE /* sceMeAudio_driver_9A9E21EE (0x9a9e21ee) */
	.word	sceMePowerControlAvcPower /* sceMePowerControlAvcPower (0xb37562aa) */
	.word	sceMeAudio_driver_C300D466 /* sceMeAudio_driver_C300D466 (0xc300d466) */
	.word	sceMeVideo_driver_C441994C /* sceMeVideo_driver_C441994C (0xc441994c) */
	.word	sceMeCalloc /* sceMeCalloc (0xc4eda9f4) */
	.word	sceMeVideo_driver_E8CD3C75 /* sceMeVideo_driver_E8CD3C75 (0xe8cd3c75) */
	.word	sceMePower_driver_E9F69ACF /* sceMePower_driver_E9F69ACF (0xe9f69acf) */
	.word	sceMeCore_driver_FA398D71 /* sceMeCore_driver_FA398D71 (0xfa398d71) */
	.word	0x00010000
	.word	0x00040002
	.word	0x00050004
	.word	0x000a0006
	.word	0x000d000a
	.word	0x00100010
	.word	0x00140011
	.word	0x00160014
.L_exp_lib_2:
	.asciz	"sceMeVideo_driver"
	.space	2
.L_exp_tbl_2:
	.word	0x21521be5
	.word	0x4d78330c
	.word	0x6d68b223
	.word	0x8768915d
	.word	0x8dd56014
	.word	0xc441994c
	.word	0xe8cd3c75
	.word	sceMeVideo_driver_21521BE5 /* sceMeVideo_driver_21521BE5 (0x21521be5) */
	.word	sceMeVideo_driver_4D78330C /* sceMeVideo_driver_4D78330C (0x4d78330c) */
	.word	sceMeVideo_driver_6D68B223 /* sceMeVideo_driver_6D68B223 (0x6d68b223) */
	.word	sceMeVideo_driver_8768915D /* sceMeVideo_driver_8768915D (0x8768915d) */
	.word	sceMeVideo_driver_8DD56014 /* sceMeVideo_driver_8DD56014 (0x8dd56014) */
	.word	sceMeVideo_driver_C441994C /* sceMeVideo_driver_C441994C (0xc441994c) */
	.word	sceMeVideo_driver_E8CD3C75 /* sceMeVideo_driver_E8CD3C75 (0xe8cd3c75) */
.L_exp_lib_3:
	.asciz	"sceMeAudio_driver"
	.space	2
.L_exp_tbl_3:
	.word	0x6ad33f60
	.word	0x81956a0b
	.word	0x9a9e21ee
	.word	0xb57f033a
	.word	0xc300d466
	.word	sceMeAudio_driver_6AD33F60 /* sceMeAudio_driver_6AD33F60 (0x6ad33f60) */
	.word	sceMeAudio_driver_81956A0B /* sceMeAudio_driver_81956A0B (0x81956a0b) */
	.word	sceMeAudio_driver_9A9E21EE /* sceMeAudio_driver_9A9E21EE (0x9a9e21ee) */
	.word	sceMeAudio_driver_B57F033A /* sceMeAudio_driver_B57F033A (0xb57f033a) */
	.word	sceMeAudio_driver_C300D466 /* sceMeAudio_driver_C300D466 (0xc300d466) */
.L_exp_lib_4:
	.asciz	"sceMeMemory_driver"
	.space	1
.L_exp_tbl_4:
	.word	0x6ed69327
	.word	0x92d3baa1
	.word	0xc4eda9f4
	.word	sceMeFree /* sceMeFree (0x6ed69327) */
	.word	sceMeMalloc /* sceMeMalloc (0x92d3baa1) */
	.word	sceMeCalloc /* sceMeCalloc (0xc4eda9f4) */
.L_exp_lib_5:
	.asciz	"sceMeCore_driver"
	.space	3
.L_exp_tbl_5:
	.word	0x4cd5b5bf
	.word	0x5dff5c50
	.word	0x635397bb
	.word	0x905a7500
	.word	0x9eb2943b
	.word	0xfa398d71
	.word	0x1b960ab4
	.word	0x385a4cc6
	.word	0x61ff4837
	.word	0xab354ca1
	.word	sceMeCore_driver_4CD5B5BF /* sceMeCore_driver_4CD5B5BF (0x4cd5b5bf) */
	.word	sceMeBootStart /* sceMeBootStart (0x5dff5c50) */
	.word	sceMeCore_driver_635397BB /* sceMeCore_driver_635397BB (0x635397bb) */
	.word	sceMeCore_driver_905A7500 /* sceMeCore_driver_905A7500 (0x905a7500) */
	.word	sceMeCore_driver_9EB2943B /* sceMeCore_driver_9EB2943B (0x9eb2943b) */
	.word	sceMeCore_driver_FA398D71 /* sceMeCore_driver_FA398D71 (0xfa398d71) */
	.word	sceMeCore_driver_var_1B960AB4 /* sceMeCore_driver_var_1B960AB4 (0x1b960ab4) */
	.word	sceMeCore_driver_var_385A4CC6 /* sceMeCore_driver_var_385A4CC6 (0x385a4cc6) */
	.word	sceMeCore_driver_var_61FF4837 /* sceMeCore_driver_var_61FF4837 (0x61ff4837) */
	.word	sceMeCore_driver_var_AB354CA1 /* sceMeCore_driver_var_AB354CA1 (0xab354ca1) */
.L_exp_lib_6:
	.asciz	"sceMePower_driver"
	.space	2
.L_exp_tbl_6:
	.word	0x0defa6a5
	.word	0x1862b784
	.word	0x984e2608
	.word	0xb37562aa
	.word	0xe9f69acf
	.word	sceMePowerSelectAvcClock /* sceMePowerSelectAvcClock (0x0defa6a5) */
	.word	sceMePower_driver_1862B784 /* sceMePower_driver_1862B784 (0x1862b784) */
	.word	sceMePower_driver_984E2608 /* sceMePower_driver_984E2608 (0x984e2608) */
	.word	sceMePowerControlAvcPower /* sceMePowerControlAvcPower (0xb37562aa) */
	.word	sceMePower_driver_E9F69ACF /* sceMePower_driver_E9F69ACF (0xe9f69acf) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x0ae8e549
	.word	0x1a27b224
	.word	0x38527743
	.word	0x457feba9
	.word	0x48f1c4ad
	.word	0x55ff02e9
	.word	0xb73d3619
	.word	0xe2a5d1ee
.L_imp_nid_2:
	.word	0x36666181
.L_imp_nid_3:
	.word	0x80e1240a
	.word	0x90b662d0
.L_imp_nid_4:
	.word	0x34b9fa9e
	.word	0x6c6887ee
	.word	0x920f104a
	.word	0xb435dec5
	.word	0xbfa98062
.L_imp_nid_5:
	.word	0x109f50bc
	.word	0x27eb27b8
	.word	0x6a638d83
	.word	0x810c4bc3
.L_imp_nid_6:
	.word	0xcd9e4bb5
	.word	0xd7d3fdcd
.L_imp_nid_7:
	.word	0x81d0d1f7
	.word	0xab7592ff
.L_imp_nid_8:
	.word	0x22a114dc
	.word	0x8ae776af
	.word	0xb4f00cb5
.L_imp_nid_9:
	.word	0x092968f4
	.word	0x5f10d406
.L_imp_nid_10:
	.word	0x0ddcd2c9
	.word	0x6b30100f
	.word	0x82bc5777
	.word	0xb011b11f
	.word	0xb7d098c6
	.word	0xceadeb47
.L_imp_nid_11:
	.word	0x84f370bc
.L_imp_nid_12:
	.word	0x87d86769

	.section .rodata.sceVstub, "a", @progbits
	.align	2
.L_imp_vstub_1:
	.globl	sceDmacplus_driver_var_AA2E7E54
sceDmacplus_driver_var_AA2E7E54:
	.word	0
	.word	0xaa2e7e54
	.globl	sceDmacplus_driver_var_C10B947A
sceDmacplus_driver_var_C10B947A:
	.word	0
	.word	0xc10b947a

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
