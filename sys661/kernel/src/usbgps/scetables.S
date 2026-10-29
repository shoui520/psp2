/* Generated PSP module import/export tables for sceUSBGps_Driver */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKermit_driver_5280B410
	.type	sceKermit_driver_5280B410, @function
sceKermit_driver_5280B410: /* sceKermit_driver 0x5280b410 @ 0x0704 */
	jr	$ra
	nop
	.globl	sceKernelRegisterSysEventHandler
	.type	sceKernelRegisterSysEventHandler, @function
sceKernelRegisterSysEventHandler: /* sceSysEventForKernel 0xcd9e4bb5 @ 0x070c */
	jr	$ra
	nop
	.globl	sceKernelUnregisterSysEventHandler
	.type	sceKernelUnregisterSysEventHandler, @function
sceKernelUnregisterSysEventHandler: /* sceSysEventForKernel 0xd7d3fdcd @ 0x0714 */
	jr	$ra
	nop
	.globl	sceUsb_driver_55494514
	.type	sceUsb_driver_55494514, @function
sceUsb_driver_55494514: /* sceUsb_driver 0x55494514 @ 0x071c */
	jr	$ra
	nop
	.globl	sceUsb_driver_6C68EE25
	.type	sceUsb_driver_6C68EE25, @function
sceUsb_driver_6C68EE25: /* sceUsb_driver 0x6c68ee25 @ 0x0724 */
	jr	$ra
	nop
	.globl	sceUsb_driver_79545577
	.type	sceUsb_driver_79545577, @function
sceUsb_driver_79545577: /* sceUsb_driver 0x79545577 @ 0x072c */
	jr	$ra
	nop
	.globl	sceUsb_driver_7C5791D3
	.type	sceUsb_driver_7C5791D3, @function
sceUsb_driver_7C5791D3: /* sceUsb_driver 0x7c5791d3 @ 0x0734 */
	jr	$ra
	nop
	.globl	sceUsb_driver_8A2D38D7
	.type	sceUsb_driver_8A2D38D7, @function
sceUsb_driver_8A2D38D7: /* sceUsb_driver 0x8a2d38d7 @ 0x073c */
	jr	$ra
	nop
	.globl	sceUsb_driver_9009AD6B
	.type	sceUsb_driver_9009AD6B, @function
sceUsb_driver_9009AD6B: /* sceUsb_driver 0x9009ad6b @ 0x0744 */
	jr	$ra
	nop
	.globl	sceUsb_driver_AB8ADCC8
	.type	sceUsb_driver_AB8ADCC8, @function
sceUsb_driver_AB8ADCC8: /* sceUsb_driver 0xab8adcc8 @ 0x074c */
	jr	$ra
	nop
	.globl	sceUsb_driver_C99E1766
	.type	sceUsb_driver_C99E1766, @function
sceUsb_driver_C99E1766: /* sceUsb_driver 0xc99e1766 @ 0x0754 */
	jr	$ra
	nop
	.globl	sceUsb_driver_CB30E794
	.type	sceUsb_driver_CB30E794, @function
sceUsb_driver_CB30E794: /* sceUsb_driver 0xcb30e794 @ 0x075c */
	jr	$ra
	nop
	.globl	sceUsb_driver_D9EE03ED
	.type	sceUsb_driver_D9EE03ED, @function
sceUsb_driver_D9EE03ED: /* sceUsb_driver 0xd9ee03ed @ 0x0764 */
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
	.half	13
	.word	.L_exp_tbl_1
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x0001
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
	.byte	6, 0
	.half	10
	.word	.L_imp_nid_0
	.word	sceUsb_driver_55494514
	.word	0
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_1
	.word	sceKermit_driver_5280B410
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_2
	.word	sceKernelRegisterSysEventHandler

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
	.byte	7, 1
	.asciz	"sceUSBGps_Driver"
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
	.asciz	"sceUsb_driver"
	.space	6
.L_imp_lib_1:
	.asciz	"sceKermit_driver"
	.space	7
.L_imp_lib_2:
	.asciz	"sceSysEventForKernel"
	.space	3
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
	.asciz	"sceUsbGps"
	.space	2
.L_exp_tbl_1:
	.word	0x268f95ca
	.word	0x31f95cde
	.word	0x54d26aa4
	.word	0x5881c826
	.word	0x63d1f89d
	.word	0x69e4aaa8
	.word	0x6eed4811
	.word	0x7c16ac3a
	.word	0x934ec2b2
	.word	0x9d8f99e8
	.word	0x9f267d34
	.word	0xa259cd67
	.word	0xa8ed0bc2
	.word	sceUsbGps_268F95CA /* sceUsbGps_268F95CA (0x268f95ca) */
	.word	sceUsbGps_31F95CDE /* sceUsbGps_31F95CDE (0x31f95cde) */
	.word	sceUsbGps_54D26AA4 /* sceUsbGps_54D26AA4 (0x54d26aa4) */
	.word	sceUsbGps_5881C826 /* sceUsbGps_5881C826 (0x5881c826) */
	.word	sceUsbGps_63D1F89D /* sceUsbGps_63D1F89D (0x63d1f89d) */
	.word	sceUsbGps_69E4AAA8 /* sceUsbGps_69E4AAA8 (0x69e4aaa8) */
	.word	sceUsbGps_6EED4811 /* sceUsbGps_6EED4811 (0x6eed4811) */
	.word	sceUsbGps_7C16AC3A /* sceUsbGps_7C16AC3A (0x7c16ac3a) */
	.word	sceUsbGps_934EC2B2 /* sceUsbGps_934EC2B2 (0x934ec2b2) */
	.word	sceUsbGps_9D8F99E8 /* sceUsbGps_9D8F99E8 (0x9d8f99e8) */
	.word	sceUsbGps_9F267D34 /* sceUsbGps_9F267D34 (0x9f267d34) */
	.word	sceUsbGps_A259CD67 /* sceUsbGps_A259CD67 (0xa259cd67) */
	.word	sceUsbGps_A8ED0BC2 /* sceUsbGps_A8ED0BC2 (0xa8ed0bc2) */
.L_exp_lib_2:
	.asciz	"sceUsbGps_driver"
	.space	3
.L_exp_tbl_2:
	.word	0x268f95ca
	.word	0x31f95cde
	.word	0x54d26aa4
	.word	0x5881c826
	.word	0x69e4aaa8
	.word	0x6eed4811
	.word	0x7c16ac3a
	.word	0x934ec2b2
	.word	0x9d8f99e8
	.word	0x9f267d34
	.word	0xa259cd67
	.word	0xa8ed0bc2
	.word	sceUsbGps_268F95CA /* sceUsbGps_driver_268F95CA (0x268f95ca) */
	.word	sceUsbGps_31F95CDE /* sceUsbGps_driver_31F95CDE (0x31f95cde) */
	.word	sceUsbGps_54D26AA4 /* sceUsbGps_driver_54D26AA4 (0x54d26aa4) */
	.word	sceUsbGps_5881C826 /* sceUsbGps_driver_5881C826 (0x5881c826) */
	.word	sceUsbGps_69E4AAA8 /* sceUsbGps_driver_69E4AAA8 (0x69e4aaa8) */
	.word	sceUsbGps_6EED4811 /* sceUsbGps_driver_6EED4811 (0x6eed4811) */
	.word	sceUsbGps_7C16AC3A /* sceUsbGps_driver_7C16AC3A (0x7c16ac3a) */
	.word	sceUsbGps_934EC2B2 /* sceUsbGps_driver_934EC2B2 (0x934ec2b2) */
	.word	sceUsbGps_9D8F99E8 /* sceUsbGps_driver_9D8F99E8 (0x9d8f99e8) */
	.word	sceUsbGps_9F267D34 /* sceUsbGps_driver_9F267D34 (0x9f267d34) */
	.word	sceUsbGps_A259CD67 /* sceUsbGps_driver_A259CD67 (0xa259cd67) */
	.word	sceUsbGps_A8ED0BC2 /* sceUsbGps_driver_A8ED0BC2 (0xa8ed0bc2) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x55494514
	.word	0x6c68ee25
	.word	0x79545577
	.word	0x7c5791d3
	.word	0x8a2d38d7
	.word	0x9009ad6b
	.word	0xab8adcc8
	.word	0xc99e1766
	.word	0xcb30e794
	.word	0xd9ee03ed
.L_imp_nid_1:
	.word	0x5280b410
.L_imp_nid_2:
	.word	0xcd9e4bb5
	.word	0xd7d3fdcd

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
