/* Generated PSP module import/export tables for sceSystimer */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelCpuSuspendIntr
	.type	sceKernelCpuSuspendIntr, @function
sceKernelCpuSuspendIntr: /* InterruptManagerForKernel 0x092968f4 @ 0x0960 */
	jr	$ra
	nop
	.globl	sceKernelEnableIntr
	.type	sceKernelEnableIntr, @function
sceKernelEnableIntr: /* InterruptManagerForKernel 0x4d6e7305 @ 0x0968 */
	jr	$ra
	nop
	.globl	sceKernelRegisterIntrHandler
	.type	sceKernelRegisterIntrHandler, @function
sceKernelRegisterIntrHandler: /* InterruptManagerForKernel 0x58dd8978 @ 0x0970 */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntr
	.type	sceKernelCpuResumeIntr, @function
sceKernelCpuResumeIntr: /* InterruptManagerForKernel 0x5f10d406 @ 0x0978 */
	jr	$ra
	nop
	.globl	sceKernelDisableIntr
	.type	sceKernelDisableIntr, @function
sceKernelDisableIntr: /* InterruptManagerForKernel 0xd774ba45 @ 0x0980 */
	jr	$ra
	nop
	.globl	sceKernelReleaseIntrHandler
	.type	sceKernelReleaseIntrHandler, @function
sceKernelReleaseIntrHandler: /* InterruptManagerForKernel 0xf987b1f0 @ 0x0988 */
	jr	$ra
	nop
	.globl	sceKernelIsIntrContext
	.type	sceKernelIsIntrContext, @function
sceKernelIsIntrContext: /* InterruptManagerForKernel 0xfe28c6d9 @ 0x0990 */
	jr	$ra
	nop
	.globl	sceKernelRegisterSuspendHandler
	.type	sceKernelRegisterSuspendHandler, @function
sceKernelRegisterSuspendHandler: /* sceSuspendForKernel 0x91a77137 @ 0x0998 */
	jr	$ra
	nop
	.globl	sceKernelRegisterResumeHandler
	.type	sceKernelRegisterResumeHandler, @function
sceKernelRegisterResumeHandler: /* sceSuspendForKernel 0xb43d1a8c @ 0x09a0 */
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
	.byte	4, 0
	.half	9
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
	.half	0x0009
	.byte	6, 0
	.half	7
	.word	.L_imp_nid_0
	.word	sceKernelCpuSuspendIntr
	.word	0
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_1
	.word	sceKernelRegisterSuspendHandler

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
	.asciz	"sceSystimer"
	.space	16
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
	.asciz	"sceSuspendForKernel"
	.align	2
.L_exp_tbl_0:
	.word	0xd3744be0
	.word	0x2f064fa6
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_bootstart /* module_bootstart (0xd3744be0) */
	.word	module_reboot_before /* module_reboot_before (0x2f064fa6) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"SysTimerForKernel"
	.space	2
.L_exp_tbl_1:
	.word	0x4bc8c61c
	.word	0x6ac97b04
	.word	0x893dec1a
	.word	0x93b952d1
	.word	0x973104d0
	.word	0xcac43feb
	.word	0xdda5d3ea
	.word	0xe265669a
	.word	0xeee11ca5
	.word	sceSTimerStartCount /* sceSTimerStartCount (0x4bc8c61c) */
	.word	sceSTimerResetCount /* sceSTimerResetCount (0x6ac97b04) */
	.word	sceSTimerAlloc /* sceSTimerAlloc (0x893dec1a) */
	.word	sceSTimerFree /* sceSTimerFree (0x93b952d1) */
	.word	sceSTimerStopCount /* sceSTimerStopCount (0x973104d0) */
	.word	sceSTimerSetPrscl /* sceSTimerSetPrscl (0xcac43feb) */
	.word	sceSTimerGetCount /* sceSTimerGetCount (0xdda5d3ea) */
	.word	sceSTimerSetTMCY /* sceSTimerSetTMCY (0xe265669a) */
	.word	sceSTimerSetHandler /* sceSTimerSetHandler (0xeee11ca5) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x092968f4
	.word	0x4d6e7305
	.word	0x58dd8978
	.word	0x5f10d406
	.word	0xd774ba45
	.word	0xf987b1f0
	.word	0xfe28c6d9
.L_imp_nid_1:
	.word	0x91a77137
	.word	0xb43d1a8c

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
