/* Generated PSP module import/export tables for sceMcctrl */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceDdrdbHash
	.type	sceDdrdbHash, @function
sceDdrdbHash: /* sceDdrdb_driver 0x40cb752a @ 0x1130 */
	jr	$ra
	nop
	.globl	sceDdrdbSigvry
	.type	sceDdrdbSigvry, @function
sceDdrdbSigvry: /* sceDdrdb_driver 0xe27ce4cb @ 0x1138 */
	jr	$ra
	nop
	.globl	sceDdrdbCertvry
	.type	sceDdrdbCertvry, @function
sceDdrdbCertvry: /* sceDdrdb_driver 0x370f456a @ 0x1140 */
	jr	$ra
	nop
	.globl	sceDdrdbEncrypt
	.type	sceDdrdbEncrypt, @function
sceDdrdbEncrypt: /* sceDdrdb_driver 0x05d50f41 @ 0x1148 */
	jr	$ra
	nop
	.globl	sceDdrdbDecrypt
	.type	sceDdrdbDecrypt, @function
sceDdrdbDecrypt: /* sceDdrdb_driver 0xb33acb44 @ 0x1150 */
	jr	$ra
	nop
	.globl	sceDdrdbMul1
	.type	sceDdrdbMul1, @function
sceDdrdbMul1: /* sceDdrdb_driver 0xf970d54e @ 0x1158 */
	jr	$ra
	nop
	.globl	sceDdrdbMul2
	.type	sceDdrdbMul2, @function
sceDdrdbMul2: /* sceDdrdb_driver 0xec05300a @ 0x1160 */
	jr	$ra
	nop
	.globl	sceDdrdbSiggen
	.type	sceDdrdbSiggen, @function
sceDdrdbSiggen: /* sceDdrdb_driver 0xb24e1391 @ 0x1168 */
	jr	$ra
	nop
	.globl	sceDdrdbPrngen
	.type	sceDdrdbPrngen, @function
sceDdrdbPrngen: /* sceDdrdb_driver 0xb8218473 @ 0x1170 */
	jr	$ra
	nop
	.globl	sceIdStorageLookup
	.type	sceIdStorageLookup, @function
sceIdStorageLookup: /* sceIdStorage_driver 0x6fe062d1 @ 0x1178 */
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
	.half	6
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
	.half	9
	.word	.L_imp_nid_0
	.word	sceDdrdbHash
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_1
	.word	sceIdStorageLookup

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
	.asciz	"sceMcctrl"
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
	.asciz	"sceDdrdb_driver"
	.space	4
.L_imp_lib_1:
	.asciz	"sceIdStorage_driver"
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
	.asciz	"sceMcctrl"
	.space	2
.L_exp_tbl_1:
	.word	0x1edfd6bb
	.word	0x3ef531db
	.word	0x61550814
	.word	0x7cac25b2
	.word	0x877cd3a5
	.word	0x9618ee57
	.word	sceMcctrl_1EDFD6BB /* sceMcctrl_1EDFD6BB (0x1edfd6bb) */
	.word	sceMcctrl_3EF531DB /* sceMcctrl_3EF531DB (0x3ef531db) */
	.word	sceMcctrl_61550814 /* sceMcctrl_61550814 (0x61550814) */
	.word	sceMcctrl_7CAC25B2 /* sceMcctrl_7CAC25B2 (0x7cac25b2) */
	.word	sceMcctrl_877CD3A5 /* sceMcctrl_877CD3A5 (0x877cd3a5) */
	.word	sceMcctrl_9618EE57 /* sceMcctrl_9618EE57 (0x9618ee57) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x40cb752a
	.word	0xe27ce4cb
	.word	0x370f456a
	.word	0x05d50f41
	.word	0xb33acb44
	.word	0xf970d54e
	.word	0xec05300a
	.word	0xb24e1391
	.word	0xb8218473
.L_imp_nid_1:
	.word	0x6fe062d1

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060002
