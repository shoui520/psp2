/* Generated PSP module import/export tables for sceKernelLibrary */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelRegisterUserSpaceIntrStack
	.type	sceKernelRegisterUserSpaceIntrStack, @function
sceKernelRegisterUserSpaceIntrStack: /* InterruptManager 0xeee43f47 @ 0x0920 */
	jr	$ra
	nop
	.globl	sceKernelSetUsersystemLibWork
	.type	sceKernelSetUsersystemLibWork, @function
sceKernelSetUsersystemLibWork: /* SysMemUserForUser 0xa6848df8 @ 0x0928 */
	jr	$ra
	nop
	.globl	_sceKernelLockLwMutexCB
	.type	_sceKernelLockLwMutexCB, @function
_sceKernelLockLwMutexCB: /* ThreadManForUser 0x31327f19 @ 0x0930 */
	jr	$ra
	nop
	.globl	sceKernelReferLwMutexStatusByID
	.type	sceKernelReferLwMutexStatusByID, @function
sceKernelReferLwMutexStatusByID: /* ThreadManForUser 0x4c145944 @ 0x0938 */
	jr	$ra
	nop
	.globl	_sceKernelAllocateTlspl
	.type	_sceKernelAllocateTlspl, @function
_sceKernelAllocateTlspl: /* ThreadManForUser 0x65f54ffb @ 0x0940 */
	jr	$ra
	nop
	.globl	_sceKernelLockLwMutex
	.type	_sceKernelLockLwMutex, @function
_sceKernelLockLwMutex: /* ThreadManForUser 0x7cff8cf3 @ 0x0948 */
	jr	$ra
	nop
	.globl	_sceKernelUnlockLwMutex
	.type	_sceKernelUnlockLwMutex, @function
_sceKernelUnlockLwMutex: /* ThreadManForUser 0xbeed3a47 @ 0x0950 */
	jr	$ra
	nop
	.globl	sceKernelCheckThreadStack
	.type	sceKernelCheckThreadStack, @function
sceKernelCheckThreadStack: /* ThreadManForUser 0xd13bde95 @ 0x0958 */
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
	.byte	4, 3
	.half	1
	.word	.L_exp_tbl_0
	.word	.L_exp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	17
	.word	.L_exp_tbl_1
	.word	0x00020000
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	4, 0
	.half	1
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
	.half	0x4000
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_0
	.word	sceKernelSetUsersystemLibWork
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x4000
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_1
	.word	sceKernelRegisterUserSpaceIntrStack
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x4001
	.byte	5, 0
	.half	6
	.word	.L_imp_nid_2
	.word	_sceKernelLockLwMutexCB

	.section .lib.stub.btm, "a", @progbits
	.align	2
__lib_stub_btm:
	.word	0

	.section .rodata.sceModuleInfo, "a", @progbits
	.align	4
	.globl	module_info
	.type	module_info, @object
module_info:
	.half	0x0007
	.byte	6, 1
	.asciz	"sceKernelLibrary"
	.space	11
	.word	_gp
	.word	__lib_ent_top + 4
	.word	__lib_ent_btm
	.word	__lib_stub_top + 4
	.word	__lib_stub_btm

	.section .rodata.sceResident, "a", @progbits
	.align	2
	.word	0
.L_imp_lib_0:
	.asciz	"SysMemUserForUser"
	.space	6
.L_imp_lib_1:
	.asciz	"InterruptManager"
	.space	7
.L_imp_lib_2:
	.asciz	"ThreadManForUser"
	.space	3
.L_exp_tbl_0:
	.word	0xd632acdb
	.word	0xf01d73a7
	.word	0x11b97506
	.word	0x0f7c276c
	.word	module_start /* module_start (0xd632acdb) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
	.word	module_start_thread_parameter /* module_start_thread_parameter (0x0f7c276c) */
.L_exp_lib_1:
	.asciz	"Kernel_Library"
	.space	1
.L_exp_tbl_1:
	.word	0x092968f4
	.word	0x15b6446b
	.word	0x1839852a
	.word	0x1fc64e09
	.word	0x293b45b8
	.word	0x37431849
	.word	0x3ad10d4d
	.word	0x3b84732d
	.word	0x47a0b729
	.word	0x5f10d406
	.word	0xa089eca4
	.word	0xb55249d2
	.word	0xbea46419
	.word	0xc1734599
	.word	0xd13bde95
	.word	0xdc692ee3
	.word	0xfa835cde
	.word	sceKernelCpuSuspendIntr /* sceKernelCpuSuspendIntr (0x092968f4) */
	.word	sceKernelUnlockLwMutex /* sceKernelUnlockLwMutex (0x15b6446b) */
	.word	sceKernelMemcpy /* sceKernelMemcpy (0x1839852a) */
	.word	sceKernelLockLwMutexCB /* sceKernelLockLwMutexCB (0x1fc64e09) */
	.word	sceKernelGetThreadId /* sceKernelGetThreadId (0x293b45b8) */
	.word	sceKernelTryLockLwMutex_600 /* sceKernelTryLockLwMutex_600 (0x37431849) */
	.word	Kernel_Library_3AD10D4D /* Kernel_Library_3AD10D4D (0x3ad10d4d) */
	.word	sceKernelCpuResumeIntrWithSync /* sceKernelCpuResumeIntrWithSync (0x3b84732d) */
	.word	sceKernelIsCpuIntrSuspended /* sceKernelIsCpuIntrSuspended (0x47a0b729) */
	.word	sceKernelCpuResumeIntr /* sceKernelCpuResumeIntr (0x5f10d406) */
	.word	sceKernelMemset /* sceKernelMemset (0xa089eca4) */
	.word	sceKernelIsCpuIntrEnable /* sceKernelIsCpuIntrEnable (0xb55249d2) */
	.word	sceKernelLockLwMutex /* sceKernelLockLwMutex (0xbea46419) */
	.word	sceKernelReferLwMutexStatus /* sceKernelReferLwMutexStatus (0xc1734599) */
	.word	sceKernelCheckThreadStack /* sceKernelCheckThreadStack (0xd13bde95) */
	.word	sceKernelTryLockLwMutex /* sceKernelTryLockLwMutex (0xdc692ee3) */
	.word	sceKernelGetTlsAddr /* sceKernelGetTlsAddr (0xfa835cde) */
	.word	0x00010000
	.word	0x00050004
	.word	0x00090008
	.word	0x000a000a
	.word	0x000a000a
	.word	0x000b000a
	.word	0x000e000d
	.word	0x00100010
.L_exp_lib_2:
	.asciz	"sceGe_lazy"
	.space	1
.L_exp_tbl_2:
	.word	0x31129b95
	.word	sceGe_lazy_31129B95 /* sceGe_lazy_31129B95 (0x31129b95) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0xa6848df8
.L_imp_nid_1:
	.word	0xeee43f47
.L_imp_nid_2:
	.word	0x31327f19
	.word	0x4c145944
	.word	0x65f54ffb
	.word	0x7cff8cf3
	.word	0xbeed3a47
	.word	0xd13bde95

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
