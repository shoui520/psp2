/* Generated PSP module import/export tables for sceUSBMic_Driver */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelMemset
	.type	sceKernelMemset, @function
sceKernelMemset: /* SysMemForKernel 0x8ae776af @ 0x029c */
	jr	$ra
	nop
	.globl	sceKernelDelayThread
	.type	sceKernelDelayThread, @function
sceKernelDelayThread: /* ThreadManForKernel 0xceadeb47 @ 0x02a4 */
	jr	$ra
	nop
	.globl	sceUsb_driver_6C68EE25
	.type	sceUsb_driver_6C68EE25, @function
sceUsb_driver_6C68EE25: /* sceUsb_driver 0x6c68ee25 @ 0x02ac */
	jr	$ra
	nop
	.globl	sceUsb_driver_C99E1766
	.type	sceUsb_driver_C99E1766, @function
sceUsb_driver_C99E1766: /* sceUsb_driver 0xc99e1766 @ 0x02b4 */
	jr	$ra
	nop
	.globl	sceUsb_driver_D9EE03ED
	.type	sceUsb_driver_D9EE03ED, @function
sceUsb_driver_D9EE03ED: /* sceUsb_driver 0xd9ee03ed @ 0x02bc */
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
	.half	7
	.word	.L_exp_tbl_1
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	4, 0
	.half	7
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
	.word	sceUsb_driver_6C68EE25
	.word	0
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	1
	.word	.L_imp_nid_1
	.word	sceKernelDelayThread
	.word	0
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_2
	.word	sceKernelMemset

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
	.asciz	"sceUSBMic_Driver"
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
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_2:
	.asciz	"SysMemForKernel"
	.align	2
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
	.asciz	"sceUsbMic"
	.space	2
.L_exp_tbl_1:
	.word	0x06128e42
	.word	0x2e6dcdcd
	.word	0x45310f07
	.word	0x5f7f368d
	.word	0x63400e20
	.word	0xb8e536eb
	.word	0xf899001c
	.word	sceUsbMic_06128E42 /* sceUsbMic_06128E42 (0x06128e42) */
	.word	sceUsbMic_2E6DCDCD /* sceUsbMic_2E6DCDCD (0x2e6dcdcd) */
	.word	sceUsbMic_45310F07 /* sceUsbMic_45310F07 (0x45310f07) */
	.word	sceUsbMic_5F7F368D /* sceUsbMic_5F7F368D (0x5f7f368d) */
	.word	sceUsbMic_63400E20 /* sceUsbMic_63400E20 (0x63400e20) */
	.word	sceUsbMic_B8E536EB /* sceUsbMic_B8E536EB (0xb8e536eb) */
	.word	sceUsbMic_F899001C /* sceUsbMic_F899001C (0xf899001c) */
.L_exp_lib_2:
	.asciz	"sceUsbMic_driver"
	.space	3
.L_exp_tbl_2:
	.word	0x06128e42
	.word	0x2e6dcdcd
	.word	0x45310f07
	.word	0x5f7f368d
	.word	0x63400e20
	.word	0xb8e536eb
	.word	0xf899001c
	.word	sceUsbMic_06128E42 /* sceUsbMic_driver_06128E42 (0x06128e42) */
	.word	sceUsbMic_2E6DCDCD /* sceUsbMic_driver_2E6DCDCD (0x2e6dcdcd) */
	.word	sceUsbMic_45310F07 /* sceUsbMic_driver_45310F07 (0x45310f07) */
	.word	sceUsbMic_5F7F368D /* sceUsbMic_driver_5F7F368D (0x5f7f368d) */
	.word	sceUsbMic_63400E20 /* sceUsbMic_driver_63400E20 (0x63400e20) */
	.word	sceUsbMic_B8E536EB /* sceUsbMic_driver_B8E536EB (0xb8e536eb) */
	.word	sceUsbMic_F899001C /* sceUsbMic_driver_F899001C (0xf899001c) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x6c68ee25
	.word	0xc99e1766
	.word	0xd9ee03ed
.L_imp_nid_1:
	.word	0xceadeb47
.L_imp_nid_2:
	.word	0x8ae776af

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
