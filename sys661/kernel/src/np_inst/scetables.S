/* Generated PSP module import/export tables for sceNpInstall_Driver */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceIoOpen
	.type	sceIoOpen, @function
sceIoOpen: /* IoFileMgrForKernel 0x109f50bc @ 0x0664 */
	jr	$ra
	nop
	.globl	sceIoWrite
	.type	sceIoWrite, @function
sceIoWrite: /* IoFileMgrForKernel 0x42ec03ac @ 0x066c */
	jr	$ra
	nop
	.globl	sceIoRead
	.type	sceIoRead, @function
sceIoRead: /* IoFileMgrForKernel 0x6a638d83 @ 0x0674 */
	jr	$ra
	nop
	.globl	sceIoClose
	.type	sceIoClose, @function
sceIoClose: /* IoFileMgrForKernel 0x810c4bc3 @ 0x067c */
	jr	$ra
	nop
	.globl	sceIoRemove
	.type	sceIoRemove, @function
sceIoRemove: /* IoFileMgrForKernel 0xf27a9c51 @ 0x0684 */
	jr	$ra
	nop
	.globl	memset
	.type	memset, @function
memset: /* SysclibForKernel 0x10f3bb61 @ 0x068c */
	jr	$ra
	nop
	.globl	__udivdi3
	.type	__udivdi3, @function
__udivdi3: /* SysclibForKernel 0x7dee14de @ 0x0694 */
	jr	$ra
	nop
	.globl	memcpy
	.type	memcpy, @function
memcpy: /* SysclibForKernel 0xab7592ff @ 0x069c */
	jr	$ra
	nop
	.globl	sceKernelWaitThreadEnd
	.type	sceKernelWaitThreadEnd, @function
sceKernelWaitThreadEnd: /* ThreadManForKernel 0x278c0df5 @ 0x06a4 */
	jr	$ra
	nop
	.globl	sceKernelCreateThread
	.type	sceKernelCreateThread, @function
sceKernelCreateThread: /* ThreadManForKernel 0x446d8de6 @ 0x06ac */
	jr	$ra
	nop
	.globl	sceKernelTerminateThread
	.type	sceKernelTerminateThread, @function
sceKernelTerminateThread: /* ThreadManForKernel 0x616403ba @ 0x06b4 */
	jr	$ra
	nop
	.globl	sceKernelDeleteThread
	.type	sceKernelDeleteThread, @function
sceKernelDeleteThread: /* ThreadManForKernel 0x9fa03cd3 @ 0x06bc */
	jr	$ra
	nop
	.globl	sceKernelStartThread
	.type	sceKernelStartThread, @function
sceKernelStartThread: /* ThreadManForKernel 0xf475845d @ 0x06c4 */
	jr	$ra
	nop
	.globl	sceOpenPSIDGetPSID
	.type	sceOpenPSIDGetPSID, @function
sceOpenPSIDGetPSID: /* sceOpenPSID_driver 0x19d579f0 @ 0x06cc */
	jr	$ra
	nop
	.globl	scePcactAuth2BB
	.type	scePcactAuth2BB, @function
scePcactAuth2BB: /* scePcact_driver 0x08bb9677 @ 0x06d4 */
	jr	$ra
	nop
	.globl	scePcactAuth1BB
	.type	scePcactAuth1BB, @function
scePcactAuth1BB: /* scePcact_driver 0xf9ecfddd @ 0x06dc */
	jr	$ra
	nop
	.globl	sceNpDrmVerifyAct
	.type	sceNpDrmVerifyAct, @function
sceNpDrmVerifyAct: /* scePspNpDrm_driver 0x17e3f4bb @ 0x06e4 */
	jr	$ra
	nop
	.globl	sceNpDrmDecActivation
	.type	sceNpDrmDecActivation, @function
sceNpDrmDecActivation: /* scePspNpDrm_driver 0xebb198ed @ 0x06ec */
	jr	$ra
	nop
	.globl	sceRtc_driver_89FA4262
	.type	sceRtc_driver_89FA4262, @function
sceRtc_driver_89FA4262: /* sceRtc_driver 0x89fa4262 @ 0x06f4 */
	jr	$ra
	nop
	.globl	sceRtc_driver_CEEF238F
	.type	sceRtc_driver_CEEF238F, @function
sceRtc_driver_CEEF238F: /* sceRtc_driver 0xceef238f @ 0x06fc */
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
	.half	4
	.word	.L_exp_tbl_1
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x4001
	.byte	4, 0
	.half	4
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
	.half	5
	.word	.L_imp_nid_0
	.word	sceKernelWaitThreadEnd
	.word	0
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	3
	.word	.L_imp_nid_1
	.word	memset
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	5
	.word	.L_imp_nid_2
	.word	sceIoOpen
	.word	0
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_3
	.word	sceNpDrmVerifyAct
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_4
	.word	scePcactAuth2BB
	.word	.L_imp_lib_5
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_5
	.word	sceRtc_driver_89FA4262
	.word	.L_imp_lib_6
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_6
	.word	sceOpenPSIDGetPSID

	.section .lib.stub.btm, "a", @progbits
	.align	2
__lib_stub_btm:
	.word	0

	.section .rodata.sceModuleInfo, "a", @progbits
	.align	4
	.globl	module_info
	.type	module_info, @object
module_info:
	.half	0x5006
	.byte	1, 1
	.asciz	"sceNpInstall_Driver"
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
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_1:
	.asciz	"SysclibForKernel"
	.space	7
.L_imp_lib_2:
	.asciz	"IoFileMgrForKernel"
	.space	5
.L_imp_lib_3:
	.asciz	"scePspNpDrm_driver"
	.space	5
.L_imp_lib_4:
	.asciz	"scePcact_driver"
	.space	4
.L_imp_lib_5:
	.asciz	"sceRtc_driver"
	.space	6
.L_imp_lib_6:
	.asciz	"sceOpenPSID_driver"
	.space	1
.L_exp_tbl_0:
	.word	0xd632acdb
	.word	0xcee8593c
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_start /* module_start (0xd632acdb) */
	.word	module_stop /* module_stop (0xcee8593c) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"sceNpInstall_driver"
	.align	2
.L_exp_tbl_1:
	.word	0x0b039b36
	.word	0x5847d8c7
	.word	0x7ae4c8bc
	.word	0x91f9d50d
	.word	sceNpInstall_user_0B039B36 /* sceNpInstall_user_0B039B36 (0x0b039b36) */
	.word	sceNpInstall_user_5847D8C7 /* sceNpInstall_user_5847D8C7 (0x5847d8c7) */
	.word	sceNpInstall_user_7AE4C8BC /* sceNpInstall_user_7AE4C8BC (0x7ae4c8bc) */
	.word	sceNpInstall_user_91F9D50D /* sceNpInstall_user_91F9D50D (0x91f9d50d) */
.L_exp_lib_2:
	.asciz	"sceNpInstall_user"
	.space	2
.L_exp_tbl_2:
	.word	0x0b039b36
	.word	0x5847d8c7
	.word	0x7ae4c8bc
	.word	0x91f9d50d
	.word	sceNpInstall_user_0B039B36 /* sceNpInstall_user_0B039B36 (0x0b039b36) */
	.word	sceNpInstall_user_5847D8C7 /* sceNpInstall_user_5847D8C7 (0x5847d8c7) */
	.word	sceNpInstall_user_7AE4C8BC /* sceNpInstall_user_7AE4C8BC (0x7ae4c8bc) */
	.word	sceNpInstall_user_91F9D50D /* sceNpInstall_user_91F9D50D (0x91f9d50d) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x278c0df5
	.word	0x446d8de6
	.word	0x616403ba
	.word	0x9fa03cd3
	.word	0xf475845d
.L_imp_nid_1:
	.word	0x10f3bb61
	.word	0x7dee14de
	.word	0xab7592ff
.L_imp_nid_2:
	.word	0x109f50bc
	.word	0x42ec03ac
	.word	0x6a638d83
	.word	0x810c4bc3
	.word	0xf27a9c51
.L_imp_nid_3:
	.word	0x17e3f4bb
	.word	0xebb198ed
.L_imp_nid_4:
	.word	0x08bb9677
	.word	0xf9ecfddd
.L_imp_nid_5:
	.word	0x89fa4262
	.word	0xceef238f
.L_imp_nid_6:
	.word	0x19d579f0

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060002
