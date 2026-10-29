/* Generated PSP module import/export tables for sceKermitMsfs_driver */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelCpuSuspendIntr
	.type	sceKernelCpuSuspendIntr, @function
sceKernelCpuSuspendIntr: /* InterruptManagerForKernel 0x092968f4 @ 0x29f0 */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntr
	.type	sceKernelCpuResumeIntr, @function
sceKernelCpuResumeIntr: /* InterruptManagerForKernel 0x5f10d406 @ 0x29f8 */
	jr	$ra
	nop
	.globl	sceKernelGetSyscallRA
	.type	sceKernelGetSyscallRA, @function
sceKernelGetSyscallRA: /* InterruptManagerForKernel 0xa0f88036 @ 0x2a00 */
	jr	$ra
	nop
	.globl	sceIoTerminateFd
	.type	sceIoTerminateFd, @function
sceIoTerminateFd: /* IoFileMgrForKernel 0x76da16e3 @ 0x2a08 */
	jr	$ra
	nop
	.globl	sceIoAddDrv
	.type	sceIoAddDrv, @function
sceIoAddDrv: /* IoFileMgrForKernel 0x8e982a74 @ 0x2a10 */
	jr	$ra
	nop
	.globl	sceIoDelDrv
	.type	sceIoDelDrv, @function
sceIoDelDrv: /* IoFileMgrForKernel 0xc7f35804 @ 0x2a18 */
	jr	$ra
	nop
	.globl	sceKernelMemcpy
	.type	sceKernelMemcpy, @function
sceKernelMemcpy: /* SysMemForKernel 0x181065ab @ 0x2a20 */
	jr	$ra
	nop
	.globl	sceKernelMemset
	.type	sceKernelMemset, @function
sceKernelMemset: /* SysMemForKernel 0x8ae776af @ 0x2a28 */
	jr	$ra
	nop
	.globl	sceKernelGetCompiledSdkVersion
	.type	sceKernelGetCompiledSdkVersion, @function
sceKernelGetCompiledSdkVersion: /* SysMemForKernel 0xb4f00cb5 @ 0x2a30 */
	jr	$ra
	nop
	.globl	sceKernelGetUIDcontrolBlock
	.type	sceKernelGetUIDcontrolBlock, @function
sceKernelGetUIDcontrolBlock: /* SysMemForKernel 0xc90b0992 @ 0x2a38 */
	jr	$ra
	nop
	.globl	memset
	.type	memset, @function
memset: /* SysclibForKernel 0x10f3bb61 @ 0x2a40 */
	jr	$ra
	nop
	.globl	strlen
	.type	strlen, @function
strlen: /* SysclibForKernel 0x52df196c @ 0x2a48 */
	jr	$ra
	nop
	.globl	strncpy
	.type	strncpy, @function
strncpy: /* SysclibForKernel 0xb49a7697 @ 0x2a50 */
	jr	$ra
	nop
	.globl	sceKernelWaitThreadEnd
	.type	sceKernelWaitThreadEnd, @function
sceKernelWaitThreadEnd: /* ThreadManForKernel 0x278c0df5 @ 0x2a58 */
	jr	$ra
	nop
	.globl	sceKernelResumeDispatchThread
	.type	sceKernelResumeDispatchThread, @function
sceKernelResumeDispatchThread: /* ThreadManForKernel 0x27e22ec2 @ 0x2a60 */
	jr	$ra
	nop
	.globl	sceKernelGetSystemTimeLow
	.type	sceKernelGetSystemTimeLow, @function
sceKernelGetSystemTimeLow: /* ThreadManForKernel 0x369ed59d @ 0x2a68 */
	jr	$ra
	nop
	.globl	sceKernelSuspendDispatchThread
	.type	sceKernelSuspendDispatchThread, @function
sceKernelSuspendDispatchThread: /* ThreadManForKernel 0x3ad58b8c @ 0x2a70 */
	jr	$ra
	nop
	.globl	sceKernelCreateThread
	.type	sceKernelCreateThread, @function
sceKernelCreateThread: /* ThreadManForKernel 0x446d8de6 @ 0x2a78 */
	jr	$ra
	nop
	.globl	sceKernelUnlockMutex
	.type	sceKernelUnlockMutex, @function
sceKernelUnlockMutex: /* ThreadManForKernel 0x6b30100f @ 0x2a80 */
	jr	$ra
	nop
	.globl	sceKernelChangeThreadPriority
	.type	sceKernelChangeThreadPriority, @function
sceKernelChangeThreadPriority: /* ThreadManForKernel 0x71bc9871 @ 0x2a88 */
	jr	$ra
	nop
	.globl	sceKernelReceiveMsgPipe
	.type	sceKernelReceiveMsgPipe, @function
sceKernelReceiveMsgPipe: /* ThreadManForKernel 0x74829b76 @ 0x2a90 */
	jr	$ra
	nop
	.globl	sceKernelCreateMsgPipe
	.type	sceKernelCreateMsgPipe, @function
sceKernelCreateMsgPipe: /* ThreadManForKernel 0x7c0dc2a0 @ 0x2a98 */
	jr	$ra
	nop
	.globl	sceKernelSendMsgPipe
	.type	sceKernelSendMsgPipe, @function
sceKernelSendMsgPipe: /* ThreadManForKernel 0x876dbfad @ 0x2aa0 */
	jr	$ra
	nop
	.globl	sceKernelGetThreadCurrentPriority
	.type	sceKernelGetThreadCurrentPriority, @function
sceKernelGetThreadCurrentPriority: /* ThreadManForKernel 0x94aa61ee @ 0x2aa8 */
	jr	$ra
	nop
	.globl	sceKernelDeleteThread
	.type	sceKernelDeleteThread, @function
sceKernelDeleteThread: /* ThreadManForKernel 0x9fa03cd3 @ 0x2ab0 */
	jr	$ra
	nop
	.globl	sceKernelLockMutex
	.type	sceKernelLockMutex, @function
sceKernelLockMutex: /* ThreadManForKernel 0xb011b11f @ 0x2ab8 */
	jr	$ra
	nop
	.globl	sceKernelCreateMutex
	.type	sceKernelCreateMutex, @function
sceKernelCreateMutex: /* ThreadManForKernel 0xb7d098c6 @ 0x2ac0 */
	jr	$ra
	nop
	.globl	sceKernelNotifyCallback
	.type	sceKernelNotifyCallback, @function
sceKernelNotifyCallback: /* ThreadManForKernel 0xc11ba8c4 @ 0x2ac8 */
	jr	$ra
	nop
	.globl	sceKernelDelayThread
	.type	sceKernelDelayThread, @function
sceKernelDelayThread: /* ThreadManForKernel 0xceadeb47 @ 0x2ad0 */
	jr	$ra
	nop
	.globl	sceKernelDeleteMsgPipe
	.type	sceKernelDeleteMsgPipe, @function
sceKernelDeleteMsgPipe: /* ThreadManForKernel 0xf0b7da1c @ 0x2ad8 */
	jr	$ra
	nop
	.globl	sceKernelStartThread
	.type	sceKernelStartThread, @function
sceKernelStartThread: /* ThreadManForKernel 0xf475845d @ 0x2ae0 */
	jr	$ra
	nop
	.globl	sceKernelDeleteMutex
	.type	sceKernelDeleteMutex, @function
sceKernelDeleteMutex: /* ThreadManForKernel 0xf8170fbe @ 0x2ae8 */
	jr	$ra
	nop
	.globl	sceKernelDcacheWritebackRange
	.type	sceKernelDcacheWritebackRange, @function
sceKernelDcacheWritebackRange: /* UtilsForKernel 0x3ee30821 @ 0x2af0 */
	jr	$ra
	nop
	.globl	sceKernelDcacheInvalidateRange
	.type	sceKernelDcacheInvalidateRange, @function
sceKernelDcacheInvalidateRange: /* UtilsForKernel 0xbfa98062 @ 0x2af8 */
	jr	$ra
	nop
	.globl	sceKermitMemory_driver_90B662D0
	.type	sceKermitMemory_driver_90B662D0, @function
sceKermitMemory_driver_90B662D0: /* sceKermitMemory_driver 0x90b662d0 @ 0x2b00 */
	jr	$ra
	nop
	.globl	sceKermitMemory_driver_AAF047AC
	.type	sceKermitMemory_driver_AAF047AC, @function
sceKermitMemory_driver_AAF047AC: /* sceKermitMemory_driver 0xaaf047ac @ 0x2b08 */
	jr	$ra
	nop
	.globl	sceKermit_driver_36666181
	.type	sceKermit_driver_36666181, @function
sceKermit_driver_36666181: /* sceKermit_driver 0x36666181 @ 0x2b10 */
	jr	$ra
	nop
	.globl	sceKernelRegisterSysEventHandler
	.type	sceKernelRegisterSysEventHandler, @function
sceKernelRegisterSysEventHandler: /* sceSysEventForKernel 0xcd9e4bb5 @ 0x2b18 */
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
	.byte	4, 4
	.half	2
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
	.half	1
	.word	.L_imp_nid_0
	.word	sceKernelRegisterSysEventHandler
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0009
	.byte	6, 0
	.half	3
	.word	.L_imp_nid_1
	.word	sceKernelCpuSuspendIntr
	.word	0
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	19
	.word	.L_imp_nid_2
	.word	sceKernelWaitThreadEnd
	.word	0
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	3
	.word	.L_imp_nid_3
	.word	sceIoTerminateFd
	.word	0
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	4
	.word	.L_imp_nid_4
	.word	sceKernelMemcpy
	.word	.L_imp_lib_5
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	3
	.word	.L_imp_nid_5
	.word	memset
	.word	.L_imp_lib_6
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_6
	.word	sceKernelDcacheWritebackRange
	.word	.L_imp_lib_7
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_7
	.word	sceKermit_driver_36666181
	.word	.L_imp_lib_8
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_8
	.word	sceKermitMemory_driver_90B662D0

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
	.asciz	"sceKermitMsfs_driver"
	.space	7
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
	.asciz	"IoFileMgrForKernel"
	.space	5
.L_imp_lib_4:
	.asciz	"SysMemForKernel"
	.space	4
.L_imp_lib_5:
	.asciz	"SysclibForKernel"
	.space	7
.L_imp_lib_6:
	.asciz	"UtilsForKernel"
	.space	5
.L_imp_lib_7:
	.asciz	"sceKermit_driver"
	.space	7
.L_imp_lib_8:
	.asciz	"sceKermitMemory_driver"
	.space	1
.L_exp_tbl_0:
	.word	0xd3744be0
	.word	0xd632acdb
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_start /* module_bootstart (0xd3744be0) */
	.word	module_start /* module_start (0xd632acdb) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"sceKermitMsfs_driver"
	.space	3
.L_exp_tbl_1:
	.word	0x1a4dc9b7
	.word	0xead1d2c6
	.word	0x7668d90c
	.word	0x93b61a29
	.word	0xbe954163
	.word	0xc1b4213a
	.word	sceKermitMsfs_driver_1A4DC9B7 /* sceKermitMsfs_driver_1A4DC9B7 (0x1a4dc9b7) */
	.word	module_start /* sceKermitMsfs_driver_EAD1D2C6 (0xead1d2c6) */
	.word	sceKermitMsfs_driver_var_7668D90C /* sceKermitMsfs_driver_var_7668D90C (0x7668d90c) */
	.word	sceKermitMsfs_driver_var_93B61A29 /* sceKermitMsfs_driver_var_93B61A29 (0x93b61a29) */
	.word	sceKermitMsfs_driver_var_BE954163 /* sceKermitMsfs_driver_var_BE954163 (0xbe954163) */
	.word	sceKermitMsfs_driver_var_C1B4213A /* sceKermitMsfs_driver_var_C1B4213A (0xc1b4213a) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0xcd9e4bb5
.L_imp_nid_1:
	.word	0x092968f4
	.word	0x5f10d406
	.word	0xa0f88036
.L_imp_nid_2:
	.word	0x278c0df5
	.word	0x27e22ec2
	.word	0x369ed59d
	.word	0x3ad58b8c
	.word	0x446d8de6
	.word	0x6b30100f
	.word	0x71bc9871
	.word	0x74829b76
	.word	0x7c0dc2a0
	.word	0x876dbfad
	.word	0x94aa61ee
	.word	0x9fa03cd3
	.word	0xb011b11f
	.word	0xb7d098c6
	.word	0xc11ba8c4
	.word	0xceadeb47
	.word	0xf0b7da1c
	.word	0xf475845d
	.word	0xf8170fbe
.L_imp_nid_3:
	.word	0x76da16e3
	.word	0x8e982a74
	.word	0xc7f35804
.L_imp_nid_4:
	.word	0x181065ab
	.word	0x8ae776af
	.word	0xb4f00cb5
	.word	0xc90b0992
.L_imp_nid_5:
	.word	0x10f3bb61
	.word	0x52df196c
	.word	0xb49a7697
.L_imp_nid_6:
	.word	0x3ee30821
	.word	0xbfa98062
.L_imp_nid_7:
	.word	0x36666181
.L_imp_nid_8:
	.word	0x90b662d0
	.word	0xaaf047ac

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
