/* Generated PSP module import/export tables for sceHP_Remote_Driver */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelCpuSuspendIntr
	.type	sceKernelCpuSuspendIntr, @function
sceKernelCpuSuspendIntr: /* InterruptManagerForKernel 0x092968f4 @ 0x051c */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntr
	.type	sceKernelCpuResumeIntr, @function
sceKernelCpuResumeIntr: /* InterruptManagerForKernel 0x5f10d406 @ 0x0524 */
	jr	$ra
	nop
	.globl	sceKernelGetUIDcontrolBlock
	.type	sceKernelGetUIDcontrolBlock, @function
sceKernelGetUIDcontrolBlock: /* SysMemForKernel 0xc90b0992 @ 0x052c */
	jr	$ra
	nop
	.globl	sceKernelGetThreadmanIdType
	.type	sceKernelGetThreadmanIdType, @function
sceKernelGetThreadmanIdType: /* ThreadManForKernel 0x57cf62dd @ 0x0534 */
	jr	$ra
	nop
	.globl	sceKernelRegisterSysEventHandler
	.type	sceKernelRegisterSysEventHandler, @function
sceKernelRegisterSysEventHandler: /* sceSysEventForKernel 0xcd9e4bb5 @ 0x053c */
	jr	$ra
	nop
	.globl	sceKernelUnregisterSysEventHandler
	.type	sceKernelUnregisterSysEventHandler, @function
sceKernelUnregisterSysEventHandler: /* sceSysEventForKernel 0xd7d3fdcd @ 0x0544 */
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
	.byte	4, 0
	.half	12
	.word	.L_exp_tbl_1
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	31
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
	.half	2
	.word	.L_imp_nid_1
	.word	sceKernelRegisterSysEventHandler
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
	.half	1
	.word	.L_imp_nid_3
	.word	sceKernelGetThreadmanIdType
	.word	0

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
	.byte	6, 1
	.asciz	"sceHP_Remote_Driver"
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
	.asciz	"SysMemForKernel"
	.space	4
.L_imp_lib_1:
	.asciz	"sceSysEventForKernel"
	.space	7
.L_imp_lib_2:
	.asciz	"InterruptManagerForKernel"
	.space	6
.L_imp_lib_3:
	.asciz	"ThreadManForKernel"
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
	.asciz	"sceHprm"
	.align	2
.L_exp_tbl_1:
	.word	0x1910b327
	.word	0x208db1bd
	.word	0x219c58f1
	.word	0x2bcec83e
	.word	0x3953de6b
	.word	0x396fd885
	.word	0x40d2f9f0
	.word	0x444ed0b7
	.word	0x71b5fb67
	.word	0x7e69eda4
	.word	0xc7154136
	.word	0xfd7de6cd
	.word	sceHprmPeekCurrentKey /* sceHprmPeekCurrentKey (0x1910b327) */
	.word	sceHprmIsRemoteExist /* sceHprmIsRemoteExist (0x208db1bd) */
	.word	sceHprmIsMicrophoneExist /* sceHprmIsMicrophoneExist (0x219c58f1) */
	.word	sceHprmPeekLatch /* sceHprmPeekLatch (0x2bcec83e) */
	.word	sceHprm_3953DE6B /* sceHprm_3953DE6B (0x3953de6b) */
	.word	sceHprm_396FD885 /* sceHprm_396FD885 (0x396fd885) */
	.word	sceHprmReadLatch /* sceHprmReadLatch (0x40d2f9f0) */
	.word	sceHprm_444ED0B7 /* sceHprm_444ED0B7 (0x444ed0b7) */
	.word	sceHprm_71B5FB67 /* sceHprm_71B5FB67 (0x71b5fb67) */
	.word	sceHprmIsHeadphoneExist /* sceHprmIsHeadphoneExist (0x7e69eda4) */
	.word	sceHprm_C7154136 /* sceHprm_C7154136 (0xc7154136) */
	.word	sceHprm_444ED0B7 /* sceHprm_FD7DE6CD (0xfd7de6cd) */
.L_exp_lib_2:
	.asciz	"sceHprm_driver"
	.space	1
.L_exp_tbl_2:
	.word	0x06c478d4
	.word	0x0b83352b
	.word	0x1910b327
	.word	0x1c3525dd
	.word	0x1c5bc5a0
	.word	0x1f64b227
	.word	0x2903130a
	.word	0x2c7b8b05
	.word	0x2e1a7b7c
	.word	0x2f17a548
	.word	0x301063cc
	.word	0x370ddc12
	.word	0x526bb7f4
	.word	0x588845da
	.word	0x602d5955
	.word	0x8aec8ee9
	.word	0x8c728bb4
	.word	0x9bde174b
	.word	0xa0b1a19b
	.word	0xad158331
	.word	0xb2beadb8
	.word	0xb6895c1c
	.word	0xc00de13f
	.word	0xd3f17366
	.word	0xdc895b2b
	.word	0xdfc57c88
	.word	0xdfe72e99
	.word	0xe9b776be
	.word	0xeb0cfccc
	.word	0xefcfd0c5
	.word	0xfa4a25a7
	.word	sceHprm_driver_06C478D4 /* sceHprm_driver_06C478D4 (0x06c478d4) */
	.word	sceHprm_driver_0B83352B /* sceHprm_driver_0B83352B (0x0b83352b) */
	.word	sceHprmPeekCurrentKey /* sceHprmPeekCurrentKey (0x1910b327) */
	.word	sceHprm_driver_1C3525DD /* sceHprm_driver_1C3525DD (0x1c3525dd) */
	.word	sceHprm_driver_1C5BC5A0 /* sceHprm_driver_1C5BC5A0 (0x1c5bc5a0) */
	.word	sceHprmPeekLatch /* sceHprm_driver_1F64B227 (0x1f64b227) */
	.word	sceHprm_396FD885 /* sceHprm_driver_2903130A (0x2903130a) */
	.word	sceHprm_driver_2C7B8B05 /* sceHprm_driver_2C7B8B05 (0x2c7b8b05) */
	.word	sceHprm_driver_2E1A7B7C /* sceHprm_driver_2E1A7B7C (0x2e1a7b7c) */
	.word	sceHprm_driver_2F17A548 /* sceHprm_driver_2F17A548 (0x2f17a548) */
	.word	sceHprm_driver_301063CC /* sceHprm_driver_301063CC (0x301063cc) */
	.word	sceHprm_71B5FB67 /* sceHprm_driver_370DDC12 (0x370ddc12) */
	.word	sceHprm_driver_526BB7F4 /* sceHprm_driver_526BB7F4 (0x526bb7f4) */
	.word	sceHprm_driver_588845DA /* sceHprm_driver_588845DA (0x588845da) */
	.word	sceHprm_3953DE6B /* sceHprm_driver_602D5955 (0x602d5955) */
	.word	sceHprm_driver_8AEC8EE9 /* sceHprm_driver_8AEC8EE9 (0x8aec8ee9) */
	.word	sceHprm_driver_8C728BB4 /* sceHprm_driver_8C728BB4 (0x8c728bb4) */
	.word	sceHprm_driver_9BDE174B /* sceHprm_driver_9BDE174B (0x9bde174b) */
	.word	sceHprm_driver_A0B1A19B /* sceHprm_driver_A0B1A19B (0xa0b1a19b) */
	.word	sceHprmIsMicrophoneExist /* sceHprm_driver_AD158331 (0xad158331) */
	.word	sceHprm_driver_B2BEADB8 /* sceHprm_driver_B2BEADB8 (0xb2beadb8) */
	.word	sceHprm_driver_B6895C1C /* sceHprm_driver_B6895C1C (0xb6895c1c) */
	.word	sceHprm_driver_C00DE13F /* sceHprm_driver_C00DE13F (0xc00de13f) */
	.word	sceHprm_driver_D3F17366 /* sceHprm_driver_D3F17366 (0xd3f17366) */
	.word	sceHprm_driver_DC895B2B /* sceHprm_driver_DC895B2B (0xdc895b2b) */
	.word	sceHprm_C7154136 /* sceHprm_driver_DFC57C88 (0xdfc57c88) */
	.word	sceHprm_driver_DFE72E99 /* sceHprm_driver_DFE72E99 (0xdfe72e99) */
	.word	sceHprmReadLatch /* sceHprm_driver_E9B776BE (0xe9b776be) */
	.word	sceHprm_444ED0B7 /* sceHprm_driver_EB0CFCCC (0xeb0cfccc) */
	.word	sceHprmIsRemoteExist /* sceHprm_driver_EFCFD0C5 (0xefcfd0c5) */
	.word	sceHprmIsHeadphoneExist /* sceHprm_driver_FA4A25A7 (0xfa4a25a7) */
	.word	0x00020000
	.word	0x000a0006
	.word	0x000c000c
	.word	0x000f000e
	.word	0x0011000f
	.word	0x00140012
	.word	0x00170016
	.word	0x001e001b

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0xc90b0992
.L_imp_nid_1:
	.word	0xcd9e4bb5
	.word	0xd7d3fdcd
.L_imp_nid_2:
	.word	0x092968f4
	.word	0x5f10d406
.L_imp_nid_3:
	.word	0x57cf62dd

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
