/* Generated PSP module import/export tables for sceDMAManager */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelCpuSuspendIntr
	.type	sceKernelCpuSuspendIntr, @function
sceKernelCpuSuspendIntr: /* InterruptManagerForKernel 0x092968f4 @ 0x1c90 */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntrWithSync
	.type	sceKernelCpuResumeIntrWithSync, @function
sceKernelCpuResumeIntrWithSync: /* InterruptManagerForKernel 0x3b84732d @ 0x1c98 */
	jr	$ra
	nop
	.globl	sceKernelEnableIntr
	.type	sceKernelEnableIntr, @function
sceKernelEnableIntr: /* InterruptManagerForKernel 0x4d6e7305 @ 0x1ca0 */
	jr	$ra
	nop
	.globl	sceKernelRegisterIntrHandler
	.type	sceKernelRegisterIntrHandler, @function
sceKernelRegisterIntrHandler: /* InterruptManagerForKernel 0x58dd8978 @ 0x1ca8 */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntr
	.type	sceKernelCpuResumeIntr, @function
sceKernelCpuResumeIntr: /* InterruptManagerForKernel 0x5f10d406 @ 0x1cb0 */
	jr	$ra
	nop
	.globl	sceKernelDisableIntr
	.type	sceKernelDisableIntr, @function
sceKernelDisableIntr: /* InterruptManagerForKernel 0xd774ba45 @ 0x1cb8 */
	jr	$ra
	nop
	.globl	sceKernelReleaseIntrHandler
	.type	sceKernelReleaseIntrHandler, @function
sceKernelReleaseIntrHandler: /* InterruptManagerForKernel 0xf987b1f0 @ 0x1cc0 */
	jr	$ra
	nop
	.globl	Kprintf
	.type	Kprintf, @function
Kprintf: /* KDebugForKernel 0x84f370bc @ 0x1cc8 */
	jr	$ra
	nop
	.globl	memset
	.type	memset, @function
memset: /* SysclibForKernel 0x10f3bb61 @ 0x1cd0 */
	jr	$ra
	nop
	.globl	sceKernelSetEventFlag
	.type	sceKernelSetEventFlag, @function
sceKernelSetEventFlag: /* ThreadManForKernel 0x1fb15a32 @ 0x1cd8 */
	jr	$ra
	nop
	.globl	sceKernelWaitEventFlag
	.type	sceKernelWaitEventFlag, @function
sceKernelWaitEventFlag: /* ThreadManForKernel 0x402fcf22 @ 0x1ce0 */
	jr	$ra
	nop
	.globl	sceKernelCreateEventFlag
	.type	sceKernelCreateEventFlag, @function
sceKernelCreateEventFlag: /* ThreadManForKernel 0x55c20a00 @ 0x1ce8 */
	jr	$ra
	nop
	.globl	sceKernelClearEventFlag
	.type	sceKernelClearEventFlag, @function
sceKernelClearEventFlag: /* ThreadManForKernel 0x812346e4 @ 0x1cf0 */
	jr	$ra
	nop
	.globl	sceKernelDeleteEventFlag
	.type	sceKernelDeleteEventFlag, @function
sceKernelDeleteEventFlag: /* ThreadManForKernel 0xef9e4c70 @ 0x1cf8 */
	jr	$ra
	nop
	.globl	sceKernelRegisterSuspendHandler
	.type	sceKernelRegisterSuspendHandler, @function
sceKernelRegisterSuspendHandler: /* sceSuspendForKernel 0x91a77137 @ 0x1d00 */
	jr	$ra
	nop
	.globl	sceKernelRegisterResumeHandler
	.type	sceKernelRegisterResumeHandler, @function
sceKernelRegisterResumeHandler: /* sceSuspendForKernel 0xb43d1a8c @ 0x1d08 */
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
	.half	0x0001
	.byte	5, 0
	.half	21
	.word	.L_exp_tbl_1
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
	.half	1
	.word	.L_imp_nid_1
	.word	memset
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	5
	.word	.L_imp_nid_2
	.word	sceKernelSetEventFlag
	.word	0
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_3
	.word	sceKernelRegisterSuspendHandler
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_4
	.word	Kprintf

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
	.byte	8, 1
	.asciz	"sceDMAManager"
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
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_3:
	.asciz	"sceSuspendForKernel"
	.space	4
.L_imp_lib_4:
	.asciz	"KDebugForKernel"
	.align	2
.L_exp_tbl_0:
	.word	0xd3744be0
	.word	0xd632acdb
	.word	0x2f064fa6
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_start /* module_bootstart (0xd3744be0) */
	.word	module_start /* module_start (0xd632acdb) */
	.word	sceKernelDmaExit /* module_reboot_before (0x2f064fa6) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"DmacManForKernel"
	.space	3
.L_exp_tbl_1:
	.word	0x1c46158a
	.word	0x1fc036b7
	.word	0x2e3bc333
	.word	0x32757c57
	.word	0x3bdea96c
	.word	0x3fad5844
	.word	0x59615199
	.word	0x5af32783
	.word	0x745e19ef
	.word	0x7b9634e1
	.word	0x7d21a2ef
	.word	0x904110fc
	.word	0x92700ccd
	.word	0xa84b084b
	.word	0xce467d9b
	.word	0xd0358be9
	.word	0xd3f62265
	.word	0xd8bc3120
	.word	0xdb286d65
	.word	0xe18a93a5
	.word	0xf64bab99
	.word	sceKernelDmaExit /* sceKernelDmaExit (0x1c46158a) */
	.word	sceKernelDmaOpLLIConcatenate /* sceKernelDmaOpLLIConcatenate (0x1fc036b7) */
	.word	sceKernelDmaChReserve /* sceKernelDmaChReserve (0x2e3bc333) */
	.word	sceKernelDmaRegisterDdrFlush /* sceKernelDmaRegisterDdrFlush (0x32757c57) */
	.word	sceKernelDmaOpEnQueue /* sceKernelDmaOpEnQueue (0x3bdea96c) */
	.word	sceKernelDmaOpSetupMemcpy /* sceKernelDmaOpSetupMemcpy (0x3fad5844) */
	.word	sceKernelDmaOpAlloc /* sceKernelDmaOpAlloc (0x59615199) */
	.word	sceKernelDmaOpQuit /* sceKernelDmaOpQuit (0x5af32783) */
	.word	sceKernelDmaOpFree /* sceKernelDmaOpFree (0x745e19ef) */
	.word	sceKernelDmaSoftRequest /* sceKernelDmaSoftRequest (0x7b9634e1) */
	.word	sceKernelDmaOpSetupLink /* sceKernelDmaOpSetupLink (0x7d21a2ef) */
	.word	sceKernelDmaOpAssignMultiple /* sceKernelDmaOpAssignMultiple (0x904110fc) */
	.word	sceKernelDmaOpDeQueue /* sceKernelDmaOpDeQueue (0x92700ccd) */
	.word	sceKernelDmaOpAllCancel /* sceKernelDmaOpAllCancel (0xa84b084b) */
	.word	sceKernelDmaOpSetupNormal /* sceKernelDmaOpSetupNormal (0xce467d9b) */
	.word	sceKernelDmaOpSetCallback /* sceKernelDmaOpSetCallback (0xd0358be9) */
	.word	sceKernelDmaOnDebugMode /* sceKernelDmaOnDebugMode (0xd3f62265) */
	.word	sceKernelDmaChExclude /* sceKernelDmaChExclude (0xd8bc3120) */
	.word	sceKernelDmaOpSync /* sceKernelDmaOpSync (0xdb286d65) */
	.word	sceKernelDmaOpConcatenate /* sceKernelDmaOpConcatenate (0xe18a93a5) */
	.word	sceKernelDmaOpAssign /* sceKernelDmaOpAssign (0xf64bab99) */
	.word	0x00000000
	.word	0x00030002
	.word	0x00060006
	.word	0x00080008
	.word	0x000b000b
	.word	0x000e000d
	.word	0x000f000e
	.word	0x00140013

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x092968f4
	.word	0x3b84732d
	.word	0x4d6e7305
	.word	0x58dd8978
	.word	0x5f10d406
	.word	0xd774ba45
	.word	0xf987b1f0
.L_imp_nid_1:
	.word	0x10f3bb61
.L_imp_nid_2:
	.word	0x1fb15a32
	.word	0x402fcf22
	.word	0x55c20a00
	.word	0x812346e4
	.word	0xef9e4c70
.L_imp_nid_3:
	.word	0x91a77137
	.word	0xb43d1a8c
.L_imp_nid_4:
	.word	0x84f370bc

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
