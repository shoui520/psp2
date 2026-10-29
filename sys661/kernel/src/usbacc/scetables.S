/* Generated PSP module import/export tables for sceUSB_Acc_Driver */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceUsb_driver_6C68EE25
	.type	sceUsb_driver_6C68EE25, @function
sceUsb_driver_6C68EE25: /* sceUsb_driver 0x6c68ee25 @ 0x0058 */
	jr	$ra
	nop
	.globl	sceUsb_driver_C99E1766
	.type	sceUsb_driver_C99E1766, @function
sceUsb_driver_C99E1766: /* sceUsb_driver 0xc99e1766 @ 0x0060 */
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
	.half	2
	.word	.L_exp_tbl_1
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x4001
	.byte	4, 0
	.half	2
	.word	.L_exp_tbl_2
	.word	.L_exp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	4, 0
	.half	2
	.word	.L_exp_tbl_3

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
	.half	2
	.word	.L_imp_nid_0
	.word	sceUsb_driver_6C68EE25
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
	.half	0x1006
	.byte	3, 1
	.asciz	"sceUSB_Acc_Driver"
	.space	10
	.word	_gp
	.word	__lib_ent_top + 4
	.word	__lib_ent_btm
	.word	__lib_stub_top + 4
	.word	__lib_stub_btm

	.section .rodata.sceResident, "a", @progbits
	.align	2
	.word	0
.L_imp_lib_0:
	.asciz	"sceUsb_driver"
	.space	2
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
	.asciz	"sceUsbAcc_internal"
	.space	1
.L_exp_tbl_1:
	.word	0x0cd7d4aa
	.word	0x79a1c743
	.word	sceUsbAccGetInfo /* sceUsbAccGetInfo (0x0cd7d4aa) */
	.word	sceUsbAccGetAuthStat /* sceUsbAccGetAuthStat (0x79a1c743) */
.L_exp_lib_2:
	.asciz	"sceUsbAcc"
	.space	2
.L_exp_tbl_2:
	.word	0x0cd7d4aa
	.word	0x79a1c743
	.word	sceUsbAccGetInfo /* sceUsbAccGetInfo (0x0cd7d4aa) */
	.word	sceUsbAccGetAuthStat /* sceUsbAccGetAuthStat (0x79a1c743) */
.L_exp_lib_3:
	.asciz	"sceUsbAcc_driver"
	.space	3
.L_exp_tbl_3:
	.word	0x0cd7d4aa
	.word	0x79a1c743
	.word	sceUsbAccGetInfo /* sceUsbAccGetInfo (0x0cd7d4aa) */
	.word	sceUsbAccGetAuthStat /* sceUsbAccGetAuthStat (0x79a1c743) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x6c68ee25
	.word	0xc99e1766

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
