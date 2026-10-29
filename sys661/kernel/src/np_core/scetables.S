/* Generated PSP module import/export tables for sceNpCore */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	SysMemUserForUser_945E45DA
	.type	SysMemUserForUser_945E45DA, @function
SysMemUserForUser_945E45DA: /* SysMemForKernel 0x7ff2f35a @ 0x0a2c */
	jr	$ra
	nop
	.globl	strtol
	.type	strtol, @function
strtol: /* SysclibForKernel 0x47dd934d @ 0x0a34 */
	jr	$ra
	nop
	.globl	strlen
	.type	strlen, @function
strlen: /* SysclibForKernel 0x52df196c @ 0x0a3c */
	jr	$ra
	nop
	.globl	sprintf
	.type	sprintf, @function
sprintf: /* SysclibForKernel 0x7661e728 @ 0x0a44 */
	jr	$ra
	nop
	.globl	strncmp
	.type	strncmp, @function
strncmp: /* SysclibForKernel 0x7ab35214 @ 0x0a4c */
	jr	$ra
	nop
	.globl	strcpy
	.type	strcpy, @function
strcpy: /* SysclibForKernel 0xec6f1cf2 @ 0x0a54 */
	jr	$ra
	nop
	.globl	sceOpenPSIDGetPSID
	.type	sceOpenPSIDGetPSID, @function
sceOpenPSIDGetPSID: /* sceOpenPSID_driver 0x19d579f0 @ 0x0a5c */
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
	.half	0x4001
	.byte	4, 0
	.half	11
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
	.word	SysMemUserForUser_945E45DA
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	5
	.word	.L_imp_nid_1
	.word	strtol
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_2
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
	.half	0x1006
	.byte	1, 1
	.asciz	"sceNpCore"
	.space	18
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
	.asciz	"SysclibForKernel"
	.space	7
.L_imp_lib_2:
	.asciz	"sceOpenPSID_driver"
	.space	1
.L_exp_tbl_0:
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"sceNpCore"
	.space	2
.L_exp_tbl_1:
	.word	0x0366dab6
	.word	0x04096629
	.word	0x243690ee
	.word	0x5145344f
	.word	0x515b65e8
	.word	0x52440abf
	.word	0x57e15796
	.word	0x8afab4a0
	.word	0x9218acf6
	.word	0xb13d27ca
	.word	0xe7aed5a3
	.word	sceNpCore_0366DAB6 /* sceNpCore_0366DAB6 (0x0366dab6) */
	.word	sceNpCore_04096629 /* sceNpCore_04096629 (0x04096629) */
	.word	sceNpCore_243690EE /* sceNpCore_243690EE (0x243690ee) */
	.word	sceNpCore_5145344F /* sceNpCore_5145344F (0x5145344f) */
	.word	sceNpCore_515B65E8 /* sceNpCore_515B65E8 (0x515b65e8) */
	.word	sceNpCore_52440ABF /* sceNpCore_52440ABF (0x52440abf) */
	.word	sceNpCore_57E15796 /* sceNpCore_57E15796 (0x57e15796) */
	.word	sceNpCore_8AFAB4A0 /* sceNpCore_8AFAB4A0 (0x8afab4a0) */
	.word	sceNpCore_9218ACF6 /* sceNpCore_9218ACF6 (0x9218acf6) */
	.word	sceNpCore_B13D27CA /* sceNpCore_B13D27CA (0xb13d27ca) */
	.word	sceNpCore_E7AED5A3 /* sceNpCore_E7AED5A3 (0xe7aed5a3) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x7ff2f35a
.L_imp_nid_1:
	.word	0x47dd934d
	.word	0x52df196c
	.word	0x7661e728
	.word	0x7ab35214
	.word	0xec6f1cf2
.L_imp_nid_2:
	.word	0x19d579f0

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
