/* Generated PSP module import/export tables for sceUSB_Driver */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelIsIntrContext
	.type	sceKernelIsIntrContext, @function
sceKernelIsIntrContext: /* InterruptManagerForKernel 0xfe28c6d9 @ 0x1d38 */
	jr	$ra
	nop
	.globl	sceKernelMemcpy
	.type	sceKernelMemcpy, @function
sceKernelMemcpy: /* SysMemForKernel 0x181065ab @ 0x1d40 */
	jr	$ra
	nop
	.globl	sceKernelMemset
	.type	sceKernelMemset, @function
sceKernelMemset: /* SysMemForKernel 0x8ae776af @ 0x1d48 */
	jr	$ra
	nop
	.globl	strlen
	.type	strlen, @function
strlen: /* SysclibForKernel 0x52df196c @ 0x1d50 */
	jr	$ra
	nop
	.globl	strncmp
	.type	strncmp, @function
strncmp: /* SysclibForKernel 0x7ab35214 @ 0x1d58 */
	jr	$ra
	nop
	.globl	strncpy
	.type	strncpy, @function
strncpy: /* SysclibForKernel 0xb49a7697 @ 0x1d60 */
	jr	$ra
	nop
	.globl	sceKernelSetEventFlag
	.type	sceKernelSetEventFlag, @function
sceKernelSetEventFlag: /* ThreadManForKernel 0x1fb15a32 @ 0x1d68 */
	jr	$ra
	nop
	.globl	sceKernelWaitEventFlagCB
	.type	sceKernelWaitEventFlagCB, @function
sceKernelWaitEventFlagCB: /* ThreadManForKernel 0x328c546a @ 0x1d70 */
	jr	$ra
	nop
	.globl	sceKernelWaitEventFlag
	.type	sceKernelWaitEventFlag, @function
sceKernelWaitEventFlag: /* ThreadManForKernel 0x402fcf22 @ 0x1d78 */
	jr	$ra
	nop
	.globl	sceKernelCreateEventFlag
	.type	sceKernelCreateEventFlag, @function
sceKernelCreateEventFlag: /* ThreadManForKernel 0x55c20a00 @ 0x1d80 */
	jr	$ra
	nop
	.globl	sceKernelUnlockMutex
	.type	sceKernelUnlockMutex, @function
sceKernelUnlockMutex: /* ThreadManForKernel 0x6b30100f @ 0x1d88 */
	jr	$ra
	nop
	.globl	sceKernelClearEventFlag
	.type	sceKernelClearEventFlag, @function
sceKernelClearEventFlag: /* ThreadManForKernel 0x812346e4 @ 0x1d90 */
	jr	$ra
	nop
	.globl	sceKernelLockMutex
	.type	sceKernelLockMutex, @function
sceKernelLockMutex: /* ThreadManForKernel 0xb011b11f @ 0x1d98 */
	jr	$ra
	nop
	.globl	sceKernelCreateMutex
	.type	sceKernelCreateMutex, @function
sceKernelCreateMutex: /* ThreadManForKernel 0xb7d098c6 @ 0x1da0 */
	jr	$ra
	nop
	.globl	sceKernelCancelEventFlag
	.type	sceKernelCancelEventFlag, @function
sceKernelCancelEventFlag: /* ThreadManForKernel 0xcd203292 @ 0x1da8 */
	jr	$ra
	nop
	.globl	sceKernelDelayThread
	.type	sceKernelDelayThread, @function
sceKernelDelayThread: /* ThreadManForKernel 0xceadeb47 @ 0x1db0 */
	jr	$ra
	nop
	.globl	sceKernelDeleteEventFlag
	.type	sceKernelDeleteEventFlag, @function
sceKernelDeleteEventFlag: /* ThreadManForKernel 0xef9e4c70 @ 0x1db8 */
	jr	$ra
	nop
	.globl	sceKernelDcacheInvalidateRange
	.type	sceKernelDcacheInvalidateRange, @function
sceKernelDcacheInvalidateRange: /* UtilsForKernel 0xbfa98062 @ 0x1dc0 */
	jr	$ra
	nop
	.globl	sceKermitMemory_driver_AAF047AC
	.type	sceKermitMemory_driver_AAF047AC, @function
sceKermitMemory_driver_AAF047AC: /* sceKermitMemory_driver 0xaaf047ac @ 0x1dc8 */
	jr	$ra
	nop
	.globl	sceKermit_driver_36666181
	.type	sceKermit_driver_36666181, @function
sceKermit_driver_36666181: /* sceKermit_driver 0x36666181 @ 0x1dd0 */
	jr	$ra
	nop
	.globl	sceKernelRegisterSysEventHandler
	.type	sceKernelRegisterSysEventHandler, @function
sceKernelRegisterSysEventHandler: /* sceSysEventForKernel 0xcd9e4bb5 @ 0x1dd8 */
	jr	$ra
	nop
	.globl	sceKernelUnregisterSysEventHandler
	.type	sceKernelUnregisterSysEventHandler, @function
sceKernelUnregisterSysEventHandler: /* sceSysEventForKernel 0xd7d3fdcd @ 0x1de0 */
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
	.half	4
	.word	.L_exp_tbl_0
	.word	.L_exp_lib_1
	.byte	17, 0
	.half	0x4001
	.byte	4, 0
	.half	13
	.word	.L_exp_tbl_1
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 2
	.half	32
	.word	.L_exp_tbl_2
	.word	0x00020000
	.word	.L_exp_lib_3
	.byte	17, 0
	.half	0x4007
	.byte	4, 1
	.half	0
	.word	.L_exp_tbl_3

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
	.half	0x0009
	.byte	6, 0
	.half	1
	.word	.L_imp_nid_0
	.word	sceKernelIsIntrContext
	.word	0
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	3
	.word	.L_imp_nid_1
	.word	strlen
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
	.byte	6, 0
	.half	11
	.word	.L_imp_nid_3
	.word	sceKernelSetEventFlag
	.word	0
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_4
	.word	sceKernelDcacheInvalidateRange
	.word	.L_imp_lib_5
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_5
	.word	sceKernelMemcpy
	.word	.L_imp_lib_6
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_6
	.word	sceKermit_driver_36666181
	.word	.L_imp_lib_7
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_7
	.word	sceKermitMemory_driver_AAF047AC

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
	.asciz	"sceUSB_Driver"
	.space	14
	.word	_gp
	.word	__lib_ent_top + 4
	.word	__lib_ent_btm
	.word	__lib_stub_top + 4
	.word	__lib_stub_btm

	.section .rodata.sceResident, "a", @progbits
	.align	2
	.word	0
.L_imp_lib_0:
	.asciz	"InterruptManagerForKernel"
	.space	6
.L_imp_lib_1:
	.asciz	"SysclibForKernel"
	.space	7
.L_imp_lib_2:
	.asciz	"sceSysEventForKernel"
	.space	7
.L_imp_lib_3:
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_4:
	.asciz	"UtilsForKernel"
	.space	5
.L_imp_lib_5:
	.asciz	"SysMemForKernel"
	.space	4
.L_imp_lib_6:
	.asciz	"sceKermit_driver"
	.space	7
.L_imp_lib_7:
	.asciz	"sceKermitMemory_driver"
	.space	1
.L_exp_tbl_0:
	.word	0xd632acdb
	.word	0xcee8593c
	.word	0xadf12745
	.word	0x2f064fa6
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_start /* module_start (0xd632acdb) */
	.word	module_stop /* module_stop (0xcee8593c) */
	.word	module_reboot_phase /* module_reboot_phase (0xadf12745) */
	.word	module_reboot_before /* module_reboot_before (0x2f064fa6) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"sceUsb"
	.space	1
.L_exp_tbl_1:
	.word	0x112cc951
	.word	0x1c360735
	.word	0x4e537366
	.word	0x586db82c
	.word	0x5be0e002
	.word	0x616f2b61
	.word	0x89de0dc5
	.word	0x8bfc3de8
	.word	0xae5de6af
	.word	0xc21645a4
	.word	0xc2464fa0
	.word	0xc572a9c8
	.word	0xe20b23a6
	.word	sceUsbGetDrvState /* sceUsbGetDrvState (0x112cc951) */
	.word	sceUsbWaitCancel /* sceUsbWaitCancel (0x1c360735) */
	.word	sceUsbGetDrvList /* sceUsbGetDrvList (0x4e537366) */
	.word	sceUsbActivate /* sceUsbActivate (0x586db82c) */
	.word	sceUsbWaitState /* sceUsbWaitState (0x5be0e002) */
	.word	sceUsb_616F2B61 /* sceUsb_616F2B61 (0x616f2b61) */
	.word	sceUsb_89DE0DC5 /* sceUsb_89DE0DC5 (0x89de0dc5) */
	.word	sceUsb_8BFC3DE8 /* sceUsb_8BFC3DE8 (0x8bfc3de8) */
	.word	sceUsbStart /* sceUsbStart (0xae5de6af) */
	.word	sceUsbGetState /* sceUsbGetState (0xc21645a4) */
	.word	sceUsbStop /* sceUsbStop (0xc2464fa0) */
	.word	sceUsbDeactivate /* sceUsbDeactivate (0xc572a9c8) */
	.word	sceUsb_E20B23A6 /* sceUsb_E20B23A6 (0xe20b23a6) */
.L_exp_lib_2:
	.asciz	"sceUsb_driver"
	.space	2
.L_exp_tbl_2:
	.word	0x1159202a
	.word	0x18db54dc
	.word	0x292740a2
	.word	0x32627d8f
	.word	0x3b578b06
	.word	0x469c73eb
	.word	0x55494514
	.word	0x63e55ebe
	.word	0x6c68ee25
	.word	0x79545577
	.word	0x7c5791d3
	.word	0x8839766f
	.word	0x8a2d38d7
	.word	0x8c079564
	.word	0x9009ad6b
	.word	0x96a32fbc
	.word	0x9f89b84c
	.word	0xa119f065
	.word	0xa229f2cf
	.word	0xab8adcc8
	.word	0xb3e9abce
	.word	0xbda21cf7
	.word	0xbfcdfdd1
	.word	0xc99e1766
	.word	0xcb30e794
	.word	0xcca3cdf7
	.word	0xd1f264a2
	.word	0xd27edb31
	.word	0xd9ee03ed
	.word	0xe09431a0
	.word	0xed8c8695
	.word	0xf183b4ef
	.word	0x3ea085bf
	.word	0xa942d291
	.word	sceUsb_driver_1159202A /* sceUsb_driver_1159202A (0x1159202a) */
	.word	sceUsb_driver_18DB54DC /* sceUsb_driver_18DB54DC (0x18db54dc) */
	.word	sceUsb_driver_292740A2 /* sceUsb_driver_292740A2 (0x292740a2) */
	.word	sceUsb_driver_32627D8F /* sceUsb_driver_32627D8F (0x32627d8f) */
	.word	sceUsb_driver_3B578B06 /* sceUsb_driver_3B578B06 (0x3b578b06) */
	.word	sceUsb_driver_469C73EB /* sceUsb_driver_469C73EB (0x469c73eb) */
	.word	sceUsb_driver_55494514 /* sceUsb_driver_55494514 (0x55494514) */
	.word	sceUsb_driver_63E55EBE /* sceUsb_driver_63E55EBE (0x63e55ebe) */
	.word	sceUsb_driver_6C68EE25 /* sceUsb_driver_6C68EE25 (0x6c68ee25) */
	.word	sceUsb_driver_79545577 /* sceUsb_driver_79545577 (0x79545577) */
	.word	sceUsb_driver_7C5791D3 /* sceUsb_driver_7C5791D3 (0x7c5791d3) */
	.word	sceUsb_driver_8839766F /* sceUsb_driver_8839766F (0x8839766f) */
	.word	sceUsb_driver_8A2D38D7 /* sceUsb_driver_8A2D38D7 (0x8a2d38d7) */
	.word	sceUsb_driver_8C079564 /* sceUsb_driver_8C079564 (0x8c079564) */
	.word	sceUsb_driver_9009AD6B /* sceUsb_driver_9009AD6B (0x9009ad6b) */
	.word	sceUsb_driver_96A32FBC /* sceUsb_driver_96A32FBC (0x96a32fbc) */
	.word	sceUsb_driver_9F89B84C /* sceUsb_driver_9F89B84C (0x9f89b84c) */
	.word	sceUsb_driver_A119F065 /* sceUsb_driver_A119F065 (0xa119f065) */
	.word	sceUsb_driver_A229F2CF /* sceUsb_driver_A229F2CF (0xa229f2cf) */
	.word	sceUsb_driver_AB8ADCC8 /* sceUsb_driver_AB8ADCC8 (0xab8adcc8) */
	.word	sceUsb_driver_B3E9ABCE /* sceUsb_driver_B3E9ABCE (0xb3e9abce) */
	.word	sceUsb_driver_BDA21CF7 /* sceUsb_driver_BDA21CF7 (0xbda21cf7) */
	.word	sceUsb_driver_BFCDFDD1 /* sceUsb_driver_BFCDFDD1 (0xbfcdfdd1) */
	.word	sceUsb_driver_C99E1766 /* sceUsb_driver_C99E1766 (0xc99e1766) */
	.word	sceUsb_driver_CB30E794 /* sceUsb_driver_CB30E794 (0xcb30e794) */
	.word	sceUsb_driver_CCA3CDF7 /* sceUsb_driver_CCA3CDF7 (0xcca3cdf7) */
	.word	sceUsb_driver_D1F264A2 /* sceUsb_driver_D1F264A2 (0xd1f264a2) */
	.word	sceUsb_driver_D27EDB31 /* sceUsb_driver_D27EDB31 (0xd27edb31) */
	.word	sceUsb_driver_D9EE03ED /* sceUsb_driver_D9EE03ED (0xd9ee03ed) */
	.word	sceUsb_driver_E09431A0 /* sceUsb_driver_E09431A0 (0xe09431a0) */
	.word	sceUsb_driver_ED8C8695 /* sceUsb_driver_ED8C8695 (0xed8c8695) */
	.word	sceUsb_driver_F183B4EF /* sceUsb_driver_F183B4EF (0xf183b4ef) */
	.word	sceUsb_driver_var_3EA085BF /* sceUsb_driver_var_3EA085BF (0x3ea085bf) */
	.word	sceUsb_driver_var_A942D291 /* sceUsb_driver_var_A942D291 (0xa942d291) */
	.word	0x00000000
	.word	0x00030002
	.word	0x00060005
	.word	0x00090007
	.word	0x000e000b
	.word	0x00140011
	.word	0x001a0017
	.word	0x001f001d
.L_exp_lib_3:
	.asciz	"sceUsbSer"
	.space	2
.L_exp_tbl_3:
	.word	0xa02ebcbf
	.word	sceUsbSer_var_A02EBCBF /* sceUsbSer_var_A02EBCBF (0xa02ebcbf) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0xfe28c6d9
.L_imp_nid_1:
	.word	0x52df196c
	.word	0x7ab35214
	.word	0xb49a7697
.L_imp_nid_2:
	.word	0xcd9e4bb5
	.word	0xd7d3fdcd
.L_imp_nid_3:
	.word	0x1fb15a32
	.word	0x328c546a
	.word	0x402fcf22
	.word	0x55c20a00
	.word	0x6b30100f
	.word	0x812346e4
	.word	0xb011b11f
	.word	0xb7d098c6
	.word	0xcd203292
	.word	0xceadeb47
	.word	0xef9e4c70
.L_imp_nid_4:
	.word	0xbfa98062
.L_imp_nid_5:
	.word	0x181065ab
	.word	0x8ae776af
.L_imp_nid_6:
	.word	0x36666181
.L_imp_nid_7:
	.word	0xaaf047ac

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
