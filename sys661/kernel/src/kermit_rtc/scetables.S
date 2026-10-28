/* Generated PSP module import/export tables for sceRTC_Service */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelApplicationType
	.type	sceKernelApplicationType, @function
sceKernelApplicationType: /* InitForKernel 0x7233b5bc @ 0x3970 */
	jr	$ra
	nop
	.globl	sceKernelCpuSuspendIntr
	.type	sceKernelCpuSuspendIntr, @function
sceKernelCpuSuspendIntr: /* InterruptManagerForKernel 0x092968f4 @ 0x3978 */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntr
	.type	sceKernelCpuResumeIntr, @function
sceKernelCpuResumeIntr: /* InterruptManagerForKernel 0x5f10d406 @ 0x3980 */
	jr	$ra
	nop
	.globl	Kprintf
	.type	Kprintf, @function
Kprintf: /* KDebugForKernel 0x84f370bc @ 0x3988 */
	jr	$ra
	nop
	.globl	sceKernelGetUIDcontrolBlock
	.type	sceKernelGetUIDcontrolBlock, @function
sceKernelGetUIDcontrolBlock: /* SysMemForKernel 0xc90b0992 @ 0x3990 */
	jr	$ra
	nop
	.globl	memset
	.type	memset, @function
memset: /* SysclibForKernel 0x10f3bb61 @ 0x3998 */
	jr	$ra
	nop
	.globl	__udivdi3
	.type	__udivdi3, @function
__udivdi3: /* SysclibForKernel 0x7dee14de @ 0x39a0 */
	jr	$ra
	nop
	.globl	sceKernelGetThreadmanIdType
	.type	sceKernelGetThreadmanIdType, @function
sceKernelGetThreadmanIdType: /* ThreadManForKernel 0x57cf62dd @ 0x39a8 */
	jr	$ra
	nop
	.globl	sceKernelGetSystemTimeWide
	.type	sceKernelGetSystemTimeWide, @function
sceKernelGetSystemTimeWide: /* ThreadManForKernel 0x82bc5777 @ 0x39b0 */
	jr	$ra
	nop
	.globl	sceKernelNotifyCallback
	.type	sceKernelNotifyCallback, @function
sceKernelNotifyCallback: /* ThreadManForKernel 0xc11ba8c4 @ 0x39b8 */
	jr	$ra
	nop
	.globl	sceKernelRegisterRtcFunc
	.type	sceKernelRegisterRtcFunc, @function
sceKernelRegisterRtcFunc: /* UtilsForKernel 0x23a0c5ba @ 0x39c0 */
	jr	$ra
	nop
	.globl	sceKermitPeripheral_driver_1617E966
	.type	sceKermitPeripheral_driver_1617E966, @function
sceKermitPeripheral_driver_1617E966: /* sceKermitPeripheral_driver 0x1617e966 @ 0x39c8 */
	jr	$ra
	nop
	.globl	sceKermitPeripheral_driver_20638B97
	.type	sceKermitPeripheral_driver_20638B97, @function
sceKermitPeripheral_driver_20638B97: /* sceKermitPeripheral_driver 0x20638b97 @ 0x39d0 */
	jr	$ra
	nop
	.globl	sceKermitPeripheral_driver_55D690DD
	.type	sceKermitPeripheral_driver_55D690DD, @function
sceKermitPeripheral_driver_55D690DD: /* sceKermitPeripheral_driver 0x55d690dd @ 0x39d8 */
	jr	$ra
	nop
	.globl	sceKernelPowerUnlockForUser
	.type	sceKernelPowerUnlockForUser, @function
sceKernelPowerUnlockForUser: /* sceSuspendForKernel 0x3aee7261 @ 0x39e0 */
	jr	$ra
	nop
	.globl	sceKernelPowerLockForUser
	.type	sceKernelPowerLockForUser, @function
sceKernelPowerLockForUser: /* sceSuspendForKernel 0xeadb1bd7 @ 0x39e8 */
	jr	$ra
	nop
	.globl	sceKernelRegisterSysEventHandler
	.type	sceKernelRegisterSysEventHandler, @function
sceKernelRegisterSysEventHandler: /* sceSysEventForKernel 0xcd9e4bb5 @ 0x39f0 */
	jr	$ra
	nop
	.globl	sceKernelUnregisterSysEventHandler
	.type	sceKernelUnregisterSysEventHandler, @function
sceKernelUnregisterSysEventHandler: /* sceSysEventForKernel 0xd7d3fdcd @ 0x39f8 */
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
	.half	47
	.word	.L_exp_tbl_1
	.word	0x00020000
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 1
	.half	63
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
	.half	1
	.word	.L_imp_nid_0
	.word	sceKernelGetUIDcontrolBlock
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	3
	.word	.L_imp_nid_1
	.word	sceKermitPeripheral_driver_1617E966
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_2
	.word	sceKernelRegisterSysEventHandler
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_3
	.word	sceKernelPowerUnlockForUser
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
	.half	3
	.word	.L_imp_nid_5
	.word	sceKernelGetThreadmanIdType
	.word	0
	.word	.L_imp_lib_6
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_6
	.word	memset
	.word	.L_imp_lib_7
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_7
	.word	Kprintf
	.word	.L_imp_lib_8
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_8
	.word	sceKernelRegisterRtcFunc
	.word	.L_imp_lib_9
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_9
	.word	sceKernelApplicationType

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
	.byte	11, 1
	.asciz	"sceRTC_Service"
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
	.asciz	"sceKermitPeripheral_driver"
	.space	5
.L_imp_lib_2:
	.asciz	"sceSysEventForKernel"
	.space	7
.L_imp_lib_3:
	.asciz	"sceSuspendForKernel"
	.space	4
.L_imp_lib_4:
	.asciz	"InterruptManagerForKernel"
	.space	6
.L_imp_lib_5:
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_6:
	.asciz	"SysclibForKernel"
	.space	7
.L_imp_lib_7:
	.asciz	"KDebugForKernel"
	.space	4
.L_imp_lib_8:
	.asciz	"UtilsForKernel"
	.space	5
.L_imp_lib_9:
	.asciz	"InitForKernel"
	.space	2
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
	.asciz	"sceRtc"
	.space	1
.L_exp_tbl_1:
	.word	0x011f03c1
	.word	0x029ca3b3
	.word	0x0498fb3c
	.word	0x05ef322c
	.word	0x1909c99b
	.word	0x203ceb0d
	.word	0x26d25a5d
	.word	0x26d7a24a
	.word	0x27c4594c
	.word	0x27f98543
	.word	0x28e1e988
	.word	0x34885e0d
	.word	0x36075567
	.word	0x3a807cc8
	.word	0x3f7ad767
	.word	0x42307a17
	.word	0x42842c77
	.word	0x44f45e05
	.word	0x4b1b5e82
	.word	0x4cfa57b0
	.word	0x508ba64b
	.word	0x57726bc1
	.word	0x62685e98
	.word	0x6a676d2d
	.word	0x6ff40acc
	.word	0x779242a2
	.word	0x7ace4c04
	.word	0x7d1fbed3
	.word	0x7de6711b
	.word	0x7ed29e40
	.word	0x81fcda34
	.word	0x9ed0ae87
	.word	0xc2ddbeb5
	.word	0xc41c2853
	.word	0xc663b3b9
	.word	0xcf3a2ca8
	.word	0xcf561893
	.word	0xdbf74f1b
	.word	0xdfbc5f16
	.word	0xe1c93e47
	.word	0xe51b4b7a
	.word	0xe6605bca
	.word	0xe7c27d1b
	.word	0xf006f264
	.word	0xf2a4afe5
	.word	0xf5fcc995
	.word	0xfb3b18cd
	.word	sceRtc_driver_C0F36B91 /* sceRtcGetAccumulativeTime (0x011f03c1) */
	.word	sceRtc_driver_C0F36B91 /* sceRtcGetAccumlativeTime (0x029ca3b3) */
	.word	sceRtc_driver_1FCE9E23 /* sceRtcFormatRFC3339 (0x0498fb3c) */
	.word	sceRtc_driver_1DAB3CF3 /* sceRtcGetDaysInMonth (0x05ef322c) */
	.word	sceRtc_driver_CF4E0EE0 /* sceRtcSetTime64_t (0x1909c99b) */
	.word	sceRtc_driver_7C6E9610 /* sceRtcGetLastReincarnatedTime (0x203ceb0d) */
	.word	sceRtc_driver_B84AC7D7 /* sceRtcTickAddMicroseconds (0x26d25a5d) */
	.word	sceRtc_driver_8413CADC /* sceRtcTickAddHours (0x26d7a24a) */
	.word	sceRtc_driver_E86D8FC0 /* sceRtcGetTime_t (0x27c4594c) */
	.word	sceRtc_driver_8DED141A /* sceRtcFormatRFC3339LocalTime (0x27f98543) */
	.word	sceRtc_driver_BDA60897 /* sceRtcParseRFC3339 (0x28e1e988) */
	.word	sceRtc_driver_4E267E02 /* sceRtcConvertUtcToLocalTime (0x34885e0d) */
	.word	sceRtc_driver_A4A5BF1B /* sceRtcGetDosTime (0x36075567) */
	.word	sceRtc_driver_40B07E72 /* sceRtcSetTime_t (0x3a807cc8) */
	.word	sceRtcGetCurrentTick /* sceRtcGetCurrentTick (0x3f7ad767) */
	.word	sceRtc_driver_00F66D06 /* sceRtcIsLeapYear (0x42307a17) */
	.word	sceRtc_driver_AAAE90FF /* sceRtcTickAddYears (0x42842c77) */
	.word	sceRtcTickAddTicks /* sceRtcTickAddTicks (0x44f45e05) */
	.word	sceRtcCheckValid /* sceRtcCheckValid (0x4b1b5e82) */
	.word	sceRtcGetCurrentClock /* sceRtcGetCurrentClock (0x4cfa57b0) */
	.word	sceRtc_driver_508BA64B /* sceRtc_driver_508BA64B (0x508ba64b) */
	.word	sceRtc_driver_321A839A /* sceRtcGetDayOfWeek (0x57726bc1) */
	.word	sceRtc_driver_E98FEC46 /* sceRtcGetLastAdjustedTime (0x62685e98) */
	.word	sceRtc_driver_7D8E37E1 /* sceRtcUnregisterCallback (0x6a676d2d) */
	.word	sceRtcGetTick /* sceRtcGetTick (0x6ff40acc) */
	.word	sceRtc_driver_3E66CB7E /* sceRtcConvertLocalTimeToUTC (0x779242a2) */
	.word	sceRtc_driver_CEF8FE8E /* sceRtcSetWin32FileTime (0x7ace4c04) */
	.word	sceRtc_driver_E09880CF /* sceRtcSetAlarmTick (0x7d1fbed3) */
	.word	sceRtc_driver_27FAEC90 /* sceRtcFormatRFC2822LocalTime (0x7de6711b) */
	.word	sceRtcSetTick /* sceRtcSetTick (0x7ed29e40) */
	.word	sceRtc_driver_CF76CFE5 /* sceRtcIsAlarmed (0x81fcda34) */
	.word	sceRtc_driver_281144FE /* sceRtcCompareTick (0x9ed0ae87) */
	.word	sceRtc_driver_366669D6 /* sceRtcGetAlarmTick (0xc2ddbeb5) */
	.word	sceRtc_driver_C66D9686 /* sceRtcGetTickResolution (0xc41c2853) */
	.word	sceRtc_driver_1A86F5FD /* sceRtcFormatRFC2822 (0xc663b3b9) */
	.word	sceRtc_driver_80F21937 /* sceRtcTickAddWeeks (0xcf3a2ca8) */
	.word	sceRtc_driver_B2B6578C /* sceRtcGetWin32FileTime (0xcf561893) */
	.word	sceRtc_driver_E45726F6 /* sceRtcTickAddMonths (0xdbf74f1b) */
	.word	sceRtc_driver_C3A806EE /* sceRtcParseDateTime (0xdfbc5f16) */
	.word	sceRtc_driver_94225550 /* sceRtcGetTime64_t (0xe1c93e47) */
	.word	sceRtc_driver_CB0538FD /* sceRtcTickAddDays (0xe51b4b7a) */
	.word	sceRtc_driver_77138347 /* sceRtcTickAddMinutes (0xe6605bca) */
	.word	sceRtc_driver_9012B140 /* sceRtcGetCurrentClockLocalTime (0xe7c27d1b) */
	.word	sceRtc_driver_74772CCC /* sceRtcSetDosTime (0xf006f264) */
	.word	sceRtcTickAddSeconds /* sceRtcTickAddSeconds (0xf2a4afe5) */
	.word	sceRtc_driver_ED15334F /* sceRtcGetCurrentNetworkTick (0xf5fcc995) */
	.word	sceRtc_driver_530A903E /* sceRtcRegisterCallback (0xfb3b18cd) */
	.word	0x00040000
	.word	0x000b0005
	.word	0x0014000f
	.word	0x00190016
	.word	0x001f001e
	.word	0x00200020
	.word	0x00250020
	.word	0x002b0027
.L_exp_lib_2:
	.asciz	"sceRtc_driver"
	.space	2
.L_exp_tbl_2:
	.word	0x00f66d06
	.word	0x011f03c1
	.word	0x17c26c00
	.word	0x1a86f5fd
	.word	0x1c1859df
	.word	0x1dab3cf3
	.word	0x1fce9e23
	.word	0x27faec90
	.word	0x281144fe
	.word	0x321a839a
	.word	0x366669d6
	.word	0x3e66cb7e
	.word	0x3f7ad767
	.word	0x40b07e72
	.word	0x44f45e05
	.word	0x48d07d70
	.word	0x4b1b5e82
	.word	0x4cfa57b0
	.word	0x4e267e02
	.word	0x508ba64b
	.word	0x530a903e
	.word	0x66054c2a
	.word	0x6ff40acc
	.word	0x74772ccc
	.word	0x77138347
	.word	0x7c6e9610
	.word	0x7d8e37e1
	.word	0x7ed29e40
	.word	0x80f21937
	.word	0x8413cadc
	.word	0x852255b8
	.word	0x89fa4262
	.word	0x8ded141a
	.word	0x9012b140
	.word	0x912bee56
	.word	0x929620ce
	.word	0x94225550
	.word	0x9763c138
	.word	0x9cc2797e
	.word	0xa4a5bf1b
	.word	0xaaae90ff
	.word	0xb2b6578c
	.word	0xb44bdaed
	.word	0xb84ac7d7
	.word	0xbda60897
	.word	0xc0f36b91
	.word	0xc3a806ee
	.word	0xc41c2853
	.word	0xc66d9686
	.word	0xcb0538fd
	.word	0xce27de2f
	.word	0xceef238f
	.word	0xcef8fe8e
	.word	0xcf4e0ee0
	.word	0xcf76cfe5
	.word	0xdff30673
	.word	0xe09880cf
	.word	0xe45726f6
	.word	0xe7b3abf4
	.word	0xe86d8fc0
	.word	0xe98fec46
	.word	0xed15334f
	.word	0xf2a4afe5
	.word	0x5a64fa55
	.word	sceRtc_driver_00F66D06 /* sceRtc_driver_00F66D06 (0x00f66d06) */
	.word	sceRtc_driver_C0F36B91 /* sceRtcGetAccumulativeTime (0x011f03c1) */
	.word	sceRtc_driver_66054C2A /* sceRtcSetCurrentSecureTick (0x17c26c00) */
	.word	sceRtc_driver_1A86F5FD /* sceRtc_driver_1A86F5FD (0x1a86f5fd) */
	.word	sceRtc_driver_1C1859DF /* sceRtc_driver_1C1859DF (0x1c1859df) */
	.word	sceRtc_driver_1DAB3CF3 /* sceRtc_driver_1DAB3CF3 (0x1dab3cf3) */
	.word	sceRtc_driver_1FCE9E23 /* sceRtc_driver_1FCE9E23 (0x1fce9e23) */
	.word	sceRtc_driver_27FAEC90 /* sceRtc_driver_27FAEC90 (0x27faec90) */
	.word	sceRtc_driver_281144FE /* sceRtc_driver_281144FE (0x281144fe) */
	.word	sceRtc_driver_321A839A /* sceRtc_driver_321A839A (0x321a839a) */
	.word	sceRtc_driver_366669D6 /* sceRtc_driver_366669D6 (0x366669d6) */
	.word	sceRtc_driver_3E66CB7E /* sceRtc_driver_3E66CB7E (0x3e66cb7e) */
	.word	sceRtcGetCurrentTick /* sceRtcGetCurrentTick (0x3f7ad767) */
	.word	sceRtc_driver_40B07E72 /* sceRtc_driver_40B07E72 (0x40b07e72) */
	.word	sceRtcTickAddTicks /* sceRtcTickAddTicks (0x44f45e05) */
	.word	sceRtcResume /* sceRtcResume (0x48d07d70) */
	.word	sceRtcCheckValid /* sceRtcCheckValid (0x4b1b5e82) */
	.word	sceRtcGetCurrentClock /* sceRtcGetCurrentClock (0x4cfa57b0) */
	.word	sceRtc_driver_4E267E02 /* sceRtc_driver_4E267E02 (0x4e267e02) */
	.word	sceRtc_driver_508BA64B /* sceRtc_driver_508BA64B (0x508ba64b) */
	.word	sceRtc_driver_530A903E /* sceRtc_driver_530A903E (0x530a903e) */
	.word	sceRtc_driver_66054C2A /* sceRtc_driver_66054C2A (0x66054c2a) */
	.word	sceRtcGetTick /* sceRtcGetTick (0x6ff40acc) */
	.word	sceRtc_driver_74772CCC /* sceRtc_driver_74772CCC (0x74772ccc) */
	.word	sceRtc_driver_77138347 /* sceRtc_driver_77138347 (0x77138347) */
	.word	sceRtc_driver_7C6E9610 /* sceRtc_driver_7C6E9610 (0x7c6e9610) */
	.word	sceRtc_driver_7D8E37E1 /* sceRtc_driver_7D8E37E1 (0x7d8e37e1) */
	.word	sceRtcSetTick /* sceRtcSetTick (0x7ed29e40) */
	.word	sceRtc_driver_80F21937 /* sceRtc_driver_80F21937 (0x80f21937) */
	.word	sceRtc_driver_8413CADC /* sceRtc_driver_8413CADC (0x8413cadc) */
	.word	sceRtc_driver_852255B8 /* sceRtc_driver_852255B8 (0x852255b8) */
	.word	sceRtcTickAddSeconds /* sceRtc_driver_89FA4262 (0x89fa4262) */
	.word	sceRtc_driver_8DED141A /* sceRtc_driver_8DED141A (0x8ded141a) */
	.word	sceRtc_driver_9012B140 /* sceRtc_driver_9012B140 (0x9012b140) */
	.word	sceRtcInit /* sceRtcInit (0x912bee56) */
	.word	sceRtcSetCurrentTick /* sceRtc_driver_929620CE (0x929620ce) */
	.word	sceRtc_driver_94225550 /* sceRtc_driver_94225550 (0x94225550) */
	.word	sceRtcSetCurrentTick /* sceRtcSetCurrentTick (0x9763c138) */
	.word	sceRtcSuspend /* sceRtcSuspend (0x9cc2797e) */
	.word	sceRtc_driver_A4A5BF1B /* sceRtc_driver_A4A5BF1B (0xa4a5bf1b) */
	.word	sceRtc_driver_AAAE90FF /* sceRtc_driver_AAAE90FF (0xaaae90ff) */
	.word	sceRtc_driver_B2B6578C /* sceRtc_driver_B2B6578C (0xb2b6578c) */
	.word	sceRtc_driver_CEEF238F /* sceRtcGetCurrentSecureTick (0xb44bdaed) */
	.word	sceRtc_driver_B84AC7D7 /* sceRtc_driver_B84AC7D7 (0xb84ac7d7) */
	.word	sceRtc_driver_BDA60897 /* sceRtc_driver_BDA60897 (0xbda60897) */
	.word	sceRtc_driver_C0F36B91 /* sceRtc_driver_C0F36B91 (0xc0f36b91) */
	.word	sceRtc_driver_C3A806EE /* sceRtc_driver_C3A806EE (0xc3a806ee) */
	.word	sceRtc_driver_C66D9686 /* sceRtcGetTickResolution (0xc41c2853) */
	.word	sceRtc_driver_C66D9686 /* sceRtc_driver_C66D9686 (0xc66d9686) */
	.word	sceRtc_driver_CB0538FD /* sceRtc_driver_CB0538FD (0xcb0538fd) */
	.word	sceRtcEnd /* sceRtcEnd (0xce27de2f) */
	.word	sceRtc_driver_CEEF238F /* sceRtc_driver_CEEF238F (0xceef238f) */
	.word	sceRtc_driver_CEF8FE8E /* sceRtc_driver_CEF8FE8E (0xcef8fe8e) */
	.word	sceRtc_driver_CF4E0EE0 /* sceRtc_driver_CF4E0EE0 (0xcf4e0ee0) */
	.word	sceRtc_driver_CF76CFE5 /* sceRtc_driver_CF76CFE5 (0xcf76cfe5) */
	.word	sceRtc_driver_DFF30673 /* sceRtc_driver_DFF30673 (0xdff30673) */
	.word	sceRtc_driver_E09880CF /* sceRtc_driver_E09880CF (0xe09880cf) */
	.word	sceRtc_driver_E45726F6 /* sceRtc_driver_E45726F6 (0xe45726f6) */
	.word	sceRtcSetTick /* sceRtcSetTick (0xe7b3abf4) */
	.word	sceRtc_driver_E86D8FC0 /* sceRtc_driver_E86D8FC0 (0xe86d8fc0) */
	.word	sceRtc_driver_E98FEC46 /* sceRtc_driver_E98FEC46 (0xe98fec46) */
	.word	sceRtc_driver_ED15334F /* sceRtc_driver_ED15334F (0xed15334f) */
	.word	sceRtcTickAddSeconds /* sceRtcTickAddSeconds (0xf2a4afe5) */
	.word	sceRtc_driver_var_5A64FA55 /* sceRtc_driver_var_5A64FA55 (0x5a64fa55) */
	.word	0x00020000
	.word	0x00090007
	.word	0x0013000d
	.word	0x00170015
	.word	0x0021001c
	.word	0x00290027
	.word	0x0037002d
	.word	0x003e0038

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0xc90b0992
.L_imp_nid_1:
	.word	0x1617e966
	.word	0x20638b97
	.word	0x55d690dd
.L_imp_nid_2:
	.word	0xcd9e4bb5
	.word	0xd7d3fdcd
.L_imp_nid_3:
	.word	0x3aee7261
	.word	0xeadb1bd7
.L_imp_nid_4:
	.word	0x092968f4
	.word	0x5f10d406
.L_imp_nid_5:
	.word	0x57cf62dd
	.word	0x82bc5777
	.word	0xc11ba8c4
.L_imp_nid_6:
	.word	0x10f3bb61
	.word	0x7dee14de
.L_imp_nid_7:
	.word	0x84f370bc
.L_imp_nid_8:
	.word	0x23a0c5ba
.L_imp_nid_9:
	.word	0x7233b5bc

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
