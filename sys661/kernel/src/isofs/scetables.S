/* Generated PSP module import/export tables for sceIsofs_driver */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelCpuSuspendIntr
	.type	sceKernelCpuSuspendIntr, @function
sceKernelCpuSuspendIntr: /* InterruptManagerForKernel 0x092968f4 @ 0x6850 */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntr
	.type	sceKernelCpuResumeIntr, @function
sceKernelCpuResumeIntr: /* InterruptManagerForKernel 0x5f10d406 @ 0x6858 */
	jr	$ra
	nop
	.globl	sceIoDevctl
	.type	sceIoDevctl, @function
sceIoDevctl: /* IoFileMgrForKernel 0x54f5fb11 @ 0x6860 */
	jr	$ra
	nop
	.globl	sceIoTerminateFd
	.type	sceIoTerminateFd, @function
sceIoTerminateFd: /* IoFileMgrForKernel 0x76da16e3 @ 0x6868 */
	jr	$ra
	nop
	.globl	sceIoAddDrv
	.type	sceIoAddDrv, @function
sceIoAddDrv: /* IoFileMgrForKernel 0x8e982a74 @ 0x6870 */
	jr	$ra
	nop
	.globl	sceKernelAllocHeapMemory
	.type	sceKernelAllocHeapMemory, @function
sceKernelAllocHeapMemory: /* SysMemForKernel 0x23d81675 @ 0x6878 */
	jr	$ra
	nop
	.globl	sceKernelCreateHeap
	.type	sceKernelCreateHeap, @function
sceKernelCreateHeap: /* SysMemForKernel 0x58148f07 @ 0x6880 */
	jr	$ra
	nop
	.globl	sceKernelFreeHeapMemory
	.type	sceKernelFreeHeapMemory, @function
sceKernelFreeHeapMemory: /* SysMemForKernel 0x87c2ab85 @ 0x6888 */
	jr	$ra
	nop
	.globl	sceKernelGetCompiledSdkVersion
	.type	sceKernelGetCompiledSdkVersion, @function
sceKernelGetCompiledSdkVersion: /* SysMemForKernel 0xb4f00cb5 @ 0x6890 */
	jr	$ra
	nop
	.globl	sceKernelDeleteHeap
	.type	sceKernelDeleteHeap, @function
sceKernelDeleteHeap: /* SysMemForKernel 0xdd6512d0 @ 0x6898 */
	jr	$ra
	nop
	.globl	strstr
	.type	strstr, @function
strstr: /* SysclibForKernel 0x0d188658 @ 0x68a0 */
	jr	$ra
	nop
	.globl	memset
	.type	memset, @function
memset: /* SysclibForKernel 0x10f3bb61 @ 0x68a8 */
	jr	$ra
	nop
	.globl	look_ctype_table
	.type	look_ctype_table, @function
look_ctype_table: /* SysclibForKernel 0x32c767f2 @ 0x68b0 */
	jr	$ra
	nop
	.globl	strtol
	.type	strtol, @function
strtol: /* SysclibForKernel 0x47dd934d @ 0x68b8 */
	jr	$ra
	nop
	.globl	strrchr
	.type	strrchr, @function
strrchr: /* SysclibForKernel 0x4c0e0274 @ 0x68c0 */
	jr	$ra
	nop
	.globl	strlen
	.type	strlen, @function
strlen: /* SysclibForKernel 0x52df196c @ 0x68c8 */
	jr	$ra
	nop
	.globl	__umoddi3
	.type	__umoddi3, @function
__umoddi3: /* SysclibForKernel 0x5e8e5f42 @ 0x68d0 */
	jr	$ra
	nop
	.globl	strncmp
	.type	strncmp, @function
strncmp: /* SysclibForKernel 0x7ab35214 @ 0x68d8 */
	jr	$ra
	nop
	.globl	__udivdi3
	.type	__udivdi3, @function
__udivdi3: /* SysclibForKernel 0x7dee14de @ 0x68e0 */
	jr	$ra
	nop
	.globl	memcmp
	.type	memcmp, @function
memcmp: /* SysclibForKernel 0x81d0d1f7 @ 0x68e8 */
	jr	$ra
	nop
	.globl	memcpy
	.type	memcpy, @function
memcpy: /* SysclibForKernel 0xab7592ff @ 0x68f0 */
	jr	$ra
	nop
	.globl	strchr
	.type	strchr, @function
strchr: /* SysclibForKernel 0xb1dc2ae8 @ 0x68f8 */
	jr	$ra
	nop
	.globl	strcmp
	.type	strcmp, @function
strcmp: /* SysclibForKernel 0xc0ab8932 @ 0x6900 */
	jr	$ra
	nop
	.globl	sceKernelDeleteSema
	.type	sceKernelDeleteSema, @function
sceKernelDeleteSema: /* ThreadManForKernel 0x28b6489c @ 0x6908 */
	jr	$ra
	nop
	.globl	sceKernelSignalSema
	.type	sceKernelSignalSema, @function
sceKernelSignalSema: /* ThreadManForKernel 0x3f53e640 @ 0x6910 */
	jr	$ra
	nop
	.globl	sceKernelWaitSema
	.type	sceKernelWaitSema, @function
sceKernelWaitSema: /* ThreadManForKernel 0x4e3a1105 @ 0x6918 */
	jr	$ra
	nop
	.globl	sceKernelTryAllocateFpl
	.type	sceKernelTryAllocateFpl, @function
sceKernelTryAllocateFpl: /* ThreadManForKernel 0x623ae665 @ 0x6920 */
	jr	$ra
	nop
	.globl	sceKernelExtendKernelStack
	.type	sceKernelExtendKernelStack, @function
sceKernelExtendKernelStack: /* ThreadManForKernel 0xbc31c1b9 @ 0x6928 */
	jr	$ra
	nop
	.globl	sceKernelCreateFpl
	.type	sceKernelCreateFpl, @function
sceKernelCreateFpl: /* ThreadManForKernel 0xc07bb470 @ 0x6930 */
	jr	$ra
	nop
	.globl	sceKernelDelayThread
	.type	sceKernelDelayThread, @function
sceKernelDelayThread: /* ThreadManForKernel 0xceadeb47 @ 0x6938 */
	jr	$ra
	nop
	.globl	sceKernelCreateSema
	.type	sceKernelCreateSema, @function
sceKernelCreateSema: /* ThreadManForKernel 0xd6da4ba1 @ 0x6940 */
	jr	$ra
	nop
	.globl	sceKernelDeleteFpl
	.type	sceKernelDeleteFpl, @function
sceKernelDeleteFpl: /* ThreadManForKernel 0xed1410e0 @ 0x6948 */
	jr	$ra
	nop
	.globl	sceKernelFreeFpl
	.type	sceKernelFreeFpl, @function
sceKernelFreeFpl: /* ThreadManForKernel 0xf6414a71 @ 0x6950 */
	jr	$ra
	nop
	.globl	sceRtcTickAddTicks
	.type	sceRtcTickAddTicks, @function
sceRtcTickAddTicks: /* sceRtc_driver 0x44f45e05 @ 0x6958 */
	jr	$ra
	nop
	.globl	sceRtcGetTick
	.type	sceRtcGetTick, @function
sceRtcGetTick: /* sceRtc_driver 0x6ff40acc @ 0x6960 */
	jr	$ra
	nop
	.globl	sceRtcSetTick
	.type	sceRtcSetTick, @function
sceRtcSetTick: /* sceRtc_driver 0xe7b3abf4 @ 0x6968 */
	jr	$ra
	nop
	.globl	sceUmd_040A7090
	.type	sceUmd_040A7090, @function
sceUmd_040A7090: /* sceUmd 0x040a7090 @ 0x6970 */
	jr	$ra
	nop
	.globl	sceUmdSetDriveStatus
	.type	sceUmdSetDriveStatus, @function
sceUmdSetDriveStatus: /* sceUmd 0x982272fe @ 0x6978 */
	jr	$ra
	nop
	.globl	sceUmd9660_driver_7EB57F56
	.type	sceUmd9660_driver_7EB57F56, @function
sceUmd9660_driver_7EB57F56: /* sceUmd9660_driver 0x7eb57f56 @ 0x6980 */
	jr	$ra
	nop
	.globl	sceUmd9660_driver_887C3193
	.type	sceUmd9660_driver_887C3193, @function
sceUmd9660_driver_887C3193: /* sceUmd9660_driver 0x887c3193 @ 0x6988 */
	jr	$ra
	nop
	.globl	sceUmd9660_driver_C0933C16
	.type	sceUmd9660_driver_C0933C16, @function
sceUmd9660_driver_C0933C16: /* sceUmd9660_driver 0xc0933c16 @ 0x6990 */
	jr	$ra
	nop
	.globl	sceUmdManRegisterInsertEjectUMDCallBack
	.type	sceUmdManRegisterInsertEjectUMDCallBack, @function
sceUmdManRegisterInsertEjectUMDCallBack: /* sceUmdMan_driver 0x3c8c523d @ 0x6998 */
	jr	$ra
	nop
	.globl	sceUmdMan_driver_65E2B3E0
	.type	sceUmdMan_driver_65E2B3E0, @function
sceUmdMan_driver_65E2B3E0: /* sceUmdMan_driver 0x65e2b3e0 @ 0x69a0 */
	jr	$ra
	nop
	.globl	sceUmdManUnRegisterInsertEjectUMDCallBack
	.type	sceUmdManUnRegisterInsertEjectUMDCallBack, @function
sceUmdManUnRegisterInsertEjectUMDCallBack: /* sceUmdMan_driver 0xa7536109 @ 0x69a8 */
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
	.half	2
	.word	.L_imp_nid_0
	.word	sceKernelCpuSuspendIntr
	.word	0
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	10
	.word	.L_imp_nid_1
	.word	sceKernelDeleteSema
	.word	0
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	3
	.word	.L_imp_nid_2
	.word	sceIoDevctl
	.word	0
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	13
	.word	.L_imp_nid_3
	.word	strstr
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	5
	.word	.L_imp_nid_4
	.word	sceKernelAllocHeapMemory
	.word	.L_imp_lib_5
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	3
	.word	.L_imp_nid_5
	.word	sceRtcTickAddTicks
	.word	.L_imp_lib_6
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_6
	.word	sceUmd_040A7090
	.word	.L_imp_lib_7
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	3
	.word	.L_imp_nid_7
	.word	sceUmdManRegisterInsertEjectUMDCallBack
	.word	.L_imp_lib_8
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	3
	.word	.L_imp_nid_8
	.word	sceUmd9660_driver_7EB57F56

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
	.byte	5, 1
	.asciz	"sceIsofs_driver"
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
	.asciz	"InterruptManagerForKernel"
	.space	6
.L_imp_lib_1:
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_2:
	.asciz	"IoFileMgrForKernel"
	.space	5
.L_imp_lib_3:
	.asciz	"SysclibForKernel"
	.space	7
.L_imp_lib_4:
	.asciz	"SysMemForKernel"
	.space	4
.L_imp_lib_5:
	.asciz	"sceRtc_driver"
	.space	6
.L_imp_lib_6:
	.asciz	"sceUmd"
	.space	5
.L_imp_lib_7:
	.asciz	"sceUmdMan_driver"
	.space	7
.L_imp_lib_8:
	.asciz	"sceUmd9660_driver"
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

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x092968f4
	.word	0x5f10d406
.L_imp_nid_1:
	.word	0x28b6489c
	.word	0x3f53e640
	.word	0x4e3a1105
	.word	0x623ae665
	.word	0xbc31c1b9
	.word	0xc07bb470
	.word	0xceadeb47
	.word	0xd6da4ba1
	.word	0xed1410e0
	.word	0xf6414a71
.L_imp_nid_2:
	.word	0x54f5fb11
	.word	0x76da16e3
	.word	0x8e982a74
.L_imp_nid_3:
	.word	0x0d188658
	.word	0x10f3bb61
	.word	0x32c767f2
	.word	0x47dd934d
	.word	0x4c0e0274
	.word	0x52df196c
	.word	0x5e8e5f42
	.word	0x7ab35214
	.word	0x7dee14de
	.word	0x81d0d1f7
	.word	0xab7592ff
	.word	0xb1dc2ae8
	.word	0xc0ab8932
.L_imp_nid_4:
	.word	0x23d81675
	.word	0x58148f07
	.word	0x87c2ab85
	.word	0xb4f00cb5
	.word	0xdd6512d0
.L_imp_nid_5:
	.word	0x44f45e05
	.word	0x6ff40acc
	.word	0xe7b3abf4
.L_imp_nid_6:
	.word	0x040a7090
	.word	0x982272fe
.L_imp_nid_7:
	.word	0x3c8c523d
	.word	0x65e2b3e0
	.word	0xa7536109
.L_imp_nid_8:
	.word	0x7eb57f56
	.word	0x887c3193
	.word	0xc0933c16

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
