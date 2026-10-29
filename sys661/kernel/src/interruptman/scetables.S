/* Generated PSP module import/export tables for sceInterruptManager */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelReleaseExceptionHandler
	.type	sceKernelReleaseExceptionHandler, @function
sceKernelReleaseExceptionHandler: /* ExceptionManagerForKernel 0x54b0cad2 @ 0x32b4 */
	jr	$ra
	nop
	.globl	sceKernelRegisterPriorityExceptionHandler
	.type	sceKernelRegisterPriorityExceptionHandler, @function
sceKernelRegisterPriorityExceptionHandler: /* ExceptionManagerForKernel 0x7c11759d @ 0x32bc */
	jr	$ra
	nop
	.globl	sceKernelRegisterExceptionHandler
	.type	sceKernelRegisterExceptionHandler, @function
sceKernelRegisterExceptionHandler: /* ExceptionManagerForKernel 0xfbc2353a @ 0x32c4 */
	jr	$ra
	nop
	.globl	sceKernelGetModuleGPByAddressForKernel
	.type	sceKernelGetModuleGPByAddressForKernel, @function
sceKernelGetModuleGPByAddressForKernel: /* LoadCoreForKernel 0x410084f9 @ 0x32cc */
	jr	$ra
	nop
	.globl	sceKernelRegisterLibrary
	.type	sceKernelRegisterLibrary, @function
sceKernelRegisterLibrary: /* LoadCoreForKernel 0x48af96a9 @ 0x32d4 */
	jr	$ra
	nop
	.globl	sceKernelAllocHeapMemory
	.type	sceKernelAllocHeapMemory, @function
sceKernelAllocHeapMemory: /* SysMemForKernel 0x23d81675 @ 0x32dc */
	jr	$ra
	nop
	.globl	sceKernelCreateHeap
	.type	sceKernelCreateHeap, @function
sceKernelCreateHeap: /* SysMemForKernel 0x58148f07 @ 0x32e4 */
	jr	$ra
	nop
	.globl	sceKernelFreeHeapMemory
	.type	sceKernelFreeHeapMemory, @function
sceKernelFreeHeapMemory: /* SysMemForKernel 0x87c2ab85 @ 0x32ec */
	jr	$ra
	nop
	.globl	sceKernelRegisterSuspendHandler
	.type	sceKernelRegisterSuspendHandler, @function
sceKernelRegisterSuspendHandler: /* sceSuspendForKernel 0x91a77137 @ 0x32f4 */
	jr	$ra
	nop
	.globl	sceKernelRegisterResumeHandler
	.type	sceKernelRegisterResumeHandler, @function
sceKernelRegisterResumeHandler: /* sceSuspendForKernel 0xb43d1a8c @ 0x32fc */
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
	.half	0x0009
	.byte	5, 2
	.half	49
	.word	.L_exp_tbl_1
	.word	0x00020000
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x4000
	.byte	4, 0
	.half	9
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
	.byte	6, 0
	.half	3
	.word	.L_imp_nid_0
	.word	sceKernelReleaseExceptionHandler
	.word	0
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_1
	.word	sceKernelGetModuleGPByAddressForKernel
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	3
	.word	.L_imp_nid_2
	.word	sceKernelAllocHeapMemory
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_3
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
	.byte	9, 1
	.asciz	"sceInterruptManager"
	.space	8
	.word	_gp
	.word	__lib_ent_top + 4
	.word	__lib_ent_btm
	.word	__lib_stub_top + 4
	.word	__lib_stub_btm

	.section .rodata.sceResident, "a", @progbits
	.align	2
	.word	0
.L_imp_lib_0:
	.asciz	"ExceptionManagerForKernel"
	.space	6
.L_imp_lib_1:
	.asciz	"LoadCoreForKernel"
	.space	6
.L_imp_lib_2:
	.asciz	"SysMemForKernel"
	.space	4
.L_imp_lib_3:
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
	.asciz	"InterruptManagerForKernel"
	.space	2
.L_exp_tbl_1:
	.word	0x02314986
	.word	0x02ac50d5
	.word	0x092968f4
	.word	0x0c5f7ae3
	.word	0x1373f20e
	.word	0x14d4c61a
	.word	0x19596cd3
	.word	0x2412f096
	.word	0x2980ae03
	.word	0x2f3c4dcd
	.word	0x30c08374
	.word	0x3b84732d
	.word	0x4023e1a7
	.word	0x4351dd4e
	.word	0x4d6e7305
	.word	0x551097f2
	.word	0x55d18836
	.word	0x58dd8978
	.word	0x5f10d406
	.word	0x6042aee0
	.word	0x68b5ca51
	.word	0x6dda4d7b
	.word	0x6fcba912
	.word	0x8357e7fa
	.word	0x8dfbd787
	.word	0xa0f88036
	.word	0xa1b88367
	.word	0xa4c1c627
	.word	0xa6978b70
	.word	0xab1fc793
	.word	0xaf799f96
	.word	0xb941600e
	.word	0xc495f536
	.word	0xcd36eb65
	.word	0xd61e6961
	.word	0xd774ba45
	.word	0xdb14cbe0
	.word	0xdb9a5496
	.word	0xdbd52a5d
	.word	0xe526b767
	.word	0xe6fb16e3
	.word	0xe790eaed
	.word	0xf153b371
	.word	0xf2f1e983
	.word	0xf987b1f0
	.word	0xf9e06df1
	.word	0xfb8e22ec
	.word	0xfe28c6d9
	.word	0xffa8b183
	.word	0x201a1a19
	.word	0xfd6b9cab
	.word	sceKernelCpuEnableIntr /* sceKernelCpuEnableIntr (0x02314986) */
	.word	InterruptManagerForKernel_02AC50D5 /* InterruptManagerForKernel_02AC50D5 (0x02ac50d5) */
	.word	sceKernelCpuSuspendIntr /* sceKernelCpuSuspendIntr (0x092968f4) */
	.word	sceKernelCallSubIntrHandler /* sceKernelCallSubIntrHandler (0x0c5f7ae3) */
	.word	InterruptManagerForKernel_1373F20E /* InterruptManagerForKernel_1373F20E (0x1373f20e) */
	.word	sceKernelRegisterSystemCallTable /* sceKernelRegisterSystemCallTable (0x14d4c61a) */
	.word	ReleaseSubIntrruptMonitor /* ReleaseSubIntrruptMonitor (0x19596cd3) */
	.word	sceKernelSuspendIntr /* sceKernelSuspendIntr (0x2412f096) */
	.word	sceKernelResumeSubIntr /* sceKernelResumeSubIntr (0x2980ae03) */
	.word	InterruptManagerForKernel_2F3C4DCD /* InterruptManagerForKernel_2F3C4DCD (0x2f3c4dcd) */
	.word	sceKernelGetCpuClockCounter /* sceKernelGetCpuClockCounter (0x30c08374) */
	.word	sceKernelCpuResumeIntrWithSync /* sceKernelCpuResumeIntrWithSync (0x3b84732d) */
	.word	sceKernelDisableSubIntr /* sceKernelDisableSubIntr (0x4023e1a7) */
	.word	sceKernelIsSubInterruptOccured /* sceKernelIsSubInterruptOccured (0x4351dd4e) */
	.word	sceKernelEnableIntr /* sceKernelEnableIntr (0x4d6e7305) */
	.word	InterruptManagerForKernel_551097F2 /* InterruptManagerForKernel_551097F2 (0x551097f2) */
	.word	sceKernelSetPrimarySyscallHandler /* sceKernelSetPrimarySyscallHandler (0x55d18836) */
	.word	sceKernelRegisterIntrHandler /* sceKernelRegisterIntrHandler (0x58dd8978) */
	.word	sceKernelCpuResumeIntr /* sceKernelCpuResumeIntr (0x5f10d406) */
	.word	SaveThreadContext /* SaveThreadContext (0x6042aee0) */
	.word	sceKernelQueryIntrHandlerInfo /* sceKernelQueryIntrHandlerInfo (0x68b5ca51) */
	.word	QueryInterruptManCB /* QueryInterruptManCB (0x6dda4d7b) */
	.word	InterruptManagerForKernel_6FCBA912 /* InterruptManagerForKernel_6FCBA912 (0x6fcba912) */
	.word	UnSupportIntr /* UnSupportIntr (0x8357e7fa) */
	.word	InterruptManagerForKernel_8DFBD787 /* InterruptManagerForKernel_8DFBD787 (0x8dfbd787) */
	.word	sceKernelGetSyscallRA /* sceKernelGetSyscallRA (0xa0f88036) */
	.word	sceKernelSetUserModeIntrHanlerAcceptable /* sceKernelSetUserModeIntrHanlerAcceptable (0xa1b88367) */
	.word	sceKernelClearIntrLogging /* sceKernelClearIntrLogging (0xa4c1c627) */
	.word	InterruptManagerForKernel_A6978B70 /* InterruptManagerForKernel_A6978B70 (0xa6978b70) */
	.word	sceKernelSetIntrLogging /* sceKernelSetIntrLogging (0xab1fc793) */
	.word	sceKernelCallUserIntrHandler /* sceKernelCallUserIntrHandler (0xaf799f96) */
	.word	sceKernelSetIntrLevel /* sceKernelSetIntrLevel (0xb941600e) */
	.word	sceKernelSuspendSubIntr /* sceKernelSuspendSubIntr (0xc495f536) */
	.word	sceKernelGetUserIntrStack /* sceKernelGetUserIntrStack (0xcd36eb65) */
	.word	sceKernelReleaseSubIntrHandler /* sceKernelReleaseSubIntrHandler (0xd61e6961) */
	.word	sceKernelDisableIntr /* sceKernelDisableIntr (0xd774ba45) */
	.word	sceKernelResumeIntr /* sceKernelResumeIntr (0xdb14cbe0) */
	.word	ReleaseContextHooks /* ReleaseContextHooks (0xdb9a5496) */
	.word	RegisterSubIntrruptMonitor /* RegisterSubIntrruptMonitor (0xdbd52a5d) */
	.word	InterruptManagerForKernel_E526B767 /* InterruptManagerForKernel_E526B767 (0xe526b767) */
	.word	_sceKernelGetCpuClockCounterLow /* _sceKernelGetCpuClockCounterLow (0xe6fb16e3) */
	.word	InterruptManagerForKernel_E790EAED /* InterruptManagerForKernel_E790EAED (0xe790eaed) */
	.word	sceKernelQuerySystemCall /* sceKernelQuerySystemCall (0xf153b371) */
	.word	sceKernelIsInterruptOccurred /* sceKernelIsInterruptOccurred (0xf2f1e983) */
	.word	sceKernelReleaseIntrHandler /* sceKernelReleaseIntrHandler (0xf987b1f0) */
	.word	sceKernelGetCpuClockCounterWide /* sceKernelGetCpuClockCounterWide (0xf9e06df1) */
	.word	sceKernelEnableSubIntr /* sceKernelEnableSubIntr (0xfb8e22ec) */
	.word	sceKernelIsIntrContext /* sceKernelIsIntrContext (0xfe28c6d9) */
	.word	sceKernelRegisterSubIntrHandler /* sceKernelRegisterSubIntrHandler (0xffa8b183) */
	.word	intInfo + 116 /* intInfo + 116 (0x201a1a19) */
	.word	intInfo /* intInfo (0xfd6b9cab) */
	.word	0x00040000
	.word	0x000a0007
	.word	0x000f000c
	.word	0x00170013
	.word	0x00190017
	.word	0x001f0019
	.word	0x00220020
	.word	0x002a0027
.L_exp_lib_2:
	.asciz	"InterruptManager"
	.space	3
.L_exp_tbl_2:
	.word	0x5cb5a78b
	.word	0x7860e0dc
	.word	0x8a389411
	.word	0xca04a2b9
	.word	0xd2e8363f
	.word	0xd61e6961
	.word	0xeee43f47
	.word	0xfb8e22ec
	.word	0xfc4374b8
	.word	sceKernelSuspendSubIntr /* sceKernelSuspendSubIntr (0x5cb5a78b) */
	.word	sceKernelResumeSubIntr /* sceKernelResumeSubIntr (0x7860e0dc) */
	.word	sceKernelDisableSubIntr /* sceKernelDisableSubIntr (0x8a389411) */
	.word	sceKernelRegisterSubIntrHandler /* sceKernelRegisterSubIntrHandler (0xca04a2b9) */
	.word	QueryIntrHandlerInfoForUser /* QueryIntrHandlerInfoForUser (0xd2e8363f) */
	.word	sceKernelReleaseSubIntrHandler /* sceKernelReleaseSubIntrHandler (0xd61e6961) */
	.word	sceKernelRegisterUserSpaceIntrStack /* sceKernelRegisterUserSpaceIntrStack (0xeee43f47) */
	.word	sceKernelEnableSubIntr /* sceKernelEnableSubIntr (0xfb8e22ec) */
	.word	sceKernelIsSubInterruptOccured /* InterruptManager_FC4374B8 (0xfc4374b8) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x54b0cad2
	.word	0x7c11759d
	.word	0xfbc2353a
.L_imp_nid_1:
	.word	0x410084f9
	.word	0x48af96a9
.L_imp_nid_2:
	.word	0x23d81675
	.word	0x58148f07
	.word	0x87c2ab85
.L_imp_nid_3:
	.word	0x91a77137
	.word	0xb43d1a8c

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
