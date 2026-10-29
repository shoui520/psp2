/* Generated PSP module import/export tables for sceG729 */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelCpuSuspendIntr
	.type	sceKernelCpuSuspendIntr, @function
sceKernelCpuSuspendIntr: /* InterruptManagerForKernel 0x092968f4 @ 0x0dd8 */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntr
	.type	sceKernelCpuResumeIntr, @function
sceKernelCpuResumeIntr: /* InterruptManagerForKernel 0x5f10d406 @ 0x0de0 */
	jr	$ra
	nop
	.globl	memcpy
	.type	memcpy, @function
memcpy: /* SysclibForKernel 0xab7592ff @ 0x0de8 */
	jr	$ra
	nop
	.globl	sceKernelCreateFpl
	.type	sceKernelCreateFpl, @function
sceKernelCreateFpl: /* ThreadManForKernel 0xc07bb470 @ 0x0df0 */
	jr	$ra
	nop
	.globl	sceKernelAllocateFpl
	.type	sceKernelAllocateFpl, @function
sceKernelAllocateFpl: /* ThreadManForKernel 0xd979e9bf @ 0x0df8 */
	jr	$ra
	nop
	.globl	sceKernelDeleteFpl
	.type	sceKernelDeleteFpl, @function
sceKernelDeleteFpl: /* ThreadManForKernel 0xed1410e0 @ 0x0e00 */
	jr	$ra
	nop
	.globl	sceKernelFreeFpl
	.type	sceKernelFreeFpl, @function
sceKernelFreeFpl: /* ThreadManForKernel 0xf6414a71 @ 0x0e08 */
	jr	$ra
	nop
	.globl	sceKernelDcacheWritebackInvalidateRange
	.type	sceKernelDcacheWritebackInvalidateRange, @function
sceKernelDcacheWritebackInvalidateRange: /* UtilsForKernel 0x34b9fa9e @ 0x0e10 */
	jr	$ra
	nop
	.globl	sceKernelDcacheInvalidateRangeForUser
	.type	sceKernelDcacheInvalidateRangeForUser, @function
sceKernelDcacheInvalidateRangeForUser: /* UtilsForKernel 0xbfa98062 @ 0x0e18 */
	jr	$ra
	nop
	.globl	sceMeCore_driver_635397BB
	.type	sceMeCore_driver_635397BB, @function
sceMeCore_driver_635397BB: /* sceMeCore_driver 0x635397bb @ 0x0e20 */
	jr	$ra
	nop
	.globl	sceMeCore_driver_FA398D71
	.type	sceMeCore_driver_FA398D71, @function
sceMeCore_driver_FA398D71: /* sceMeCore_driver 0xfa398d71 @ 0x0e28 */
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
	.half	0
	.word	.L_exp_tbl_0
	.word	.L_exp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	4, 0
	.half	12
	.word	.L_exp_tbl_1
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x4001
	.byte	4, 0
	.half	12
	.word	.L_exp_tbl_2

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
	.half	2
	.word	.L_imp_nid_0
	.word	sceMeCore_driver_635397BB
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_1
	.word	sceKernelDcacheWritebackInvalidateRange
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0009
	.byte	6, 0
	.half	2
	.word	.L_imp_nid_2
	.word	sceKernelCpuSuspendIntr
	.word	0
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	4
	.word	.L_imp_nid_3
	.word	sceKernelCreateFpl
	.word	0
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_4
	.word	memcpy

	.section .lib.stub.btm, "a", @progbits
	.align	2
__lib_stub_btm:
	.word	0

	.section .rodata.sceModuleInfo, "a", @progbits
	.align	4
	.globl	module_info
	.type	module_info, @object
module_info:
	.half	0x1006
	.byte	3, 1
	.asciz	"sceG729"
	.space	20
	.word	_gp
	.word	__lib_ent_top + 4
	.word	__lib_ent_btm
	.word	__lib_stub_top + 4
	.word	__lib_stub_btm

	.section .rodata.sceResident, "a", @progbits
	.align	2
	.word	0
.L_imp_lib_0:
	.asciz	"sceMeCore_driver"
	.space	7
.L_imp_lib_1:
	.asciz	"UtilsForKernel"
	.space	5
.L_imp_lib_2:
	.asciz	"InterruptManagerForKernel"
	.space	6
.L_imp_lib_3:
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_4:
	.asciz	"SysclibForKernel"
	.space	3
.L_exp_tbl_0:
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"sceG729_driver"
	.space	1
.L_exp_tbl_1:
	.word	0x13f1028a
	.word	0x17c11696
	.word	0x3489d1f3
	.word	0x55e14f75
	.word	0x5a409d1b
	.word	0x74804d93
	.word	0x890b86ae
	.word	0x8c87a2ca
	.word	0x94714d50
	.word	0xaa1e5462
	.word	0xcfcd367c
	.word	0xdb7259d5
	.word	sceG729_driver_13F1028A /* sceG729_driver_13F1028A (0x13f1028a) */
	.word	sceG729_driver_17C11696 /* sceG729_driver_17C11696 (0x17c11696) */
	.word	sceG729_driver_3489D1F3 /* sceG729_driver_3489D1F3 (0x3489d1f3) */
	.word	sceG729_driver_55E14F75 /* sceG729_driver_55E14F75 (0x55e14f75) */
	.word	sceG729_driver_5A409D1B /* sceG729_driver_5A409D1B (0x5a409d1b) */
	.word	sceG729_driver_74804D93 /* sceG729_driver_74804D93 (0x74804d93) */
	.word	sceG729_driver_890B86AE /* sceG729_driver_890B86AE (0x890b86ae) */
	.word	sceG729_driver_8C87A2CA /* sceG729_driver_8C87A2CA (0x8c87a2ca) */
	.word	sceG729_driver_94714D50 /* sceG729_driver_94714D50 (0x94714d50) */
	.word	sceG729_driver_AA1E5462 /* sceG729_driver_AA1E5462 (0xaa1e5462) */
	.word	sceG729_driver_CFCD367C /* sceG729_driver_CFCD367C (0xcfcd367c) */
	.word	sceG729_driver_DB7259D5 /* sceG729_driver_DB7259D5 (0xdb7259d5) */
.L_exp_lib_2:
	.asciz	"sceG729"
	.align	2
.L_exp_tbl_2:
	.word	0x13f1028a
	.word	0x17c11696
	.word	0x3489d1f3
	.word	0x55e14f75
	.word	0x5a409d1b
	.word	0x74804d93
	.word	0x890b86ae
	.word	0x8c87a2ca
	.word	0x94714d50
	.word	0xaa1e5462
	.word	0xcfcd367c
	.word	0xdb7259d5
	.word	sceG729_driver_13F1028A /* sceG729_13F1028A (0x13f1028a) */
	.word	sceG729_driver_17C11696 /* sceG729_17C11696 (0x17c11696) */
	.word	sceG729_driver_3489D1F3 /* sceG729_3489D1F3 (0x3489d1f3) */
	.word	sceG729_driver_55E14F75 /* sceG729_55E14F75 (0x55e14f75) */
	.word	sceG729_driver_5A409D1B /* sceG729_5A409D1B (0x5a409d1b) */
	.word	sceG729_driver_74804D93 /* sceG729_74804D93 (0x74804d93) */
	.word	sceG729_driver_890B86AE /* sceG729_890B86AE (0x890b86ae) */
	.word	sceG729_driver_8C87A2CA /* sceG729_8C87A2CA (0x8c87a2ca) */
	.word	sceG729_driver_94714D50 /* sceG729_94714D50 (0x94714d50) */
	.word	sceG729_driver_AA1E5462 /* sceG729_AA1E5462 (0xaa1e5462) */
	.word	sceG729_driver_CFCD367C /* sceG729_CFCD367C (0xcfcd367c) */
	.word	sceG729_driver_DB7259D5 /* sceG729_DB7259D5 (0xdb7259d5) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x635397bb
	.word	0xfa398d71
.L_imp_nid_1:
	.word	0x34b9fa9e
	.word	0xbfa98062
.L_imp_nid_2:
	.word	0x092968f4
	.word	0x5f10d406
.L_imp_nid_3:
	.word	0xc07bb470
	.word	0xd979e9bf
	.word	0xed1410e0
	.word	0xf6414a71
.L_imp_nid_4:
	.word	0xab7592ff

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
