/* Generated PSP module import/export tables for sceVideocodec_Driver */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelDcacheWritebackInvalidateRange
	.type	sceKernelDcacheWritebackInvalidateRange, @function
sceKernelDcacheWritebackInvalidateRange: /* UtilsForKernel 0x34b9fa9e @ 0x0de4 */
	jr	$ra
	nop
	.globl	sceKernelDcacheInvalidateRangeForUser
	.type	sceKernelDcacheInvalidateRangeForUser, @function
sceKernelDcacheInvalidateRangeForUser: /* UtilsForKernel 0xbfa98062 @ 0x0dec */
	jr	$ra
	nop
	.globl	sceMeFree
	.type	sceMeFree, @function
sceMeFree: /* sceMeMemory_driver 0x6ed69327 @ 0x0df4 */
	jr	$ra
	nop
	.globl	sceMeCalloc
	.type	sceMeCalloc, @function
sceMeCalloc: /* sceMeMemory_driver 0xc4eda9f4 @ 0x0dfc */
	jr	$ra
	nop
	.globl	sceMePower_driver_984E2608
	.type	sceMePower_driver_984E2608, @function
sceMePower_driver_984E2608: /* sceMePower_driver 0x984e2608 @ 0x0e04 */
	jr	$ra
	nop
	.globl	sceMePowerControlAvcPower
	.type	sceMePowerControlAvcPower, @function
sceMePowerControlAvcPower: /* sceMePower_driver 0xb37562aa @ 0x0e0c */
	jr	$ra
	nop
	.globl	sceMeVideo_driver_4D78330C
	.type	sceMeVideo_driver_4D78330C, @function
sceMeVideo_driver_4D78330C: /* sceMeVideo_driver 0x4d78330c @ 0x0e14 */
	jr	$ra
	nop
	.globl	sceMeVideo_driver_6D68B223
	.type	sceMeVideo_driver_6D68B223, @function
sceMeVideo_driver_6D68B223: /* sceMeVideo_driver 0x6d68b223 @ 0x0e1c */
	jr	$ra
	nop
	.globl	sceMeVideo_driver_8768915D
	.type	sceMeVideo_driver_8768915D, @function
sceMeVideo_driver_8768915D: /* sceMeVideo_driver 0x8768915d @ 0x0e24 */
	jr	$ra
	nop
	.globl	sceMeVideo_driver_8DD56014
	.type	sceMeVideo_driver_8DD56014, @function
sceMeVideo_driver_8DD56014: /* sceMeVideo_driver 0x8dd56014 @ 0x0e2c */
	jr	$ra
	nop
	.globl	sceMeVideo_driver_C441994C
	.type	sceMeVideo_driver_C441994C, @function
sceMeVideo_driver_C441994C: /* sceMeVideo_driver 0xc441994c @ 0x0e34 */
	jr	$ra
	nop
	.globl	sceMeVideo_driver_E8CD3C75
	.type	sceMeVideo_driver_E8CD3C75, @function
sceMeVideo_driver_E8CD3C75: /* sceMeVideo_driver 0xe8cd3c75 @ 0x0e3c */
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
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_0
	.word	sceMePower_driver_984E2608
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_1
	.word	sceMeFree
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	6
	.word	.L_imp_nid_2
	.word	sceMeVideo_driver_4D78330C
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_3
	.word	sceKernelDcacheWritebackInvalidateRange

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
	.asciz	"sceVideocodec_Driver"
	.space	7
	.word	_gp
	.word	__lib_ent_top + 4
	.word	__lib_ent_btm
	.word	__lib_stub_top + 4
	.word	__lib_stub_btm

	.section .rodata.sceResident, "a", @progbits
	.align	2
	.word	0
.L_imp_lib_0:
	.asciz	"sceMePower_driver"
	.space	6
.L_imp_lib_1:
	.asciz	"sceMeMemory_driver"
	.space	5
.L_imp_lib_2:
	.asciz	"sceMeVideo_driver"
	.space	6
.L_imp_lib_3:
	.asciz	"UtilsForKernel"
	.space	1
.L_exp_tbl_0:
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"sceVideocodec"
	.space	2
.L_exp_tbl_1:
	.word	0x17099f0a
	.word	0x26927d19
	.word	0x2d31f5b1
	.word	0x2f385e7f
	.word	0x307e6e1c
	.word	0x4f160bf4
	.word	0x627b7d42
	.word	0x745a7b7a
	.word	0xa2f0564e
	.word	0xc01ec829
	.word	0xdba273fa
	.word	sceVideocodecInit /* sceVideocodecInit (0x17099f0a) */
	.word	sceVideocodecGetVersion /* sceVideocodecGetVersion (0x26927d19) */
	.word	sceVideocodecGetEDRAM /* sceVideocodecGetEDRAM (0x2d31f5b1) */
	.word	sceVideocodecScanHeader /* sceVideocodecScanHeader (0x2f385e7f) */
	.word	sceVideocodecDelete /* sceVideocodecDelete (0x307e6e1c) */
	.word	sceVideocodecReleaseEDRAM /* sceVideocodecReleaseEDRAM (0x4f160bf4) */
	.word	sceVideocodecGetSEI /* sceVideocodecGetSEI (0x627b7d42) */
	.word	sceVideocodecSetMemory /* sceVideocodecSetMemory (0x745a7b7a) */
	.word	sceVideocodecStop /* sceVideocodecStop (0xa2f0564e) */
	.word	sceVideocodecOpen /* sceVideocodecOpen (0xc01ec829) */
	.word	sceVideocodecDecode /* sceVideocodecDecode (0xdba273fa) */
.L_exp_lib_2:
	.asciz	"sceVideocodec_driver"
	.space	3
.L_exp_tbl_2:
	.word	0xcb3312d1
	.word	sceVideocodec_driver_CB3312D1 /* sceVideocodec_driver_CB3312D1 (0xcb3312d1) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x984e2608
	.word	0xb37562aa
.L_imp_nid_1:
	.word	0x6ed69327
	.word	0xc4eda9f4
.L_imp_nid_2:
	.word	0x4d78330c
	.word	0x6d68b223
	.word	0x8768915d
	.word	0x8dd56014
	.word	0xc441994c
	.word	0xe8cd3c75
.L_imp_nid_3:
	.word	0x34b9fa9e
	.word	0xbfa98062

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
