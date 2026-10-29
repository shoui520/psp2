/* Generated PSP module import/export tables for sceRegistry_Service */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	memset
	.type	memset, @function
memset: /* SysclibForKernel 0x10f3bb61 @ 0x632c */
	jr	$ra
	nop
	.globl	strncmp
	.type	strncmp, @function
strncmp: /* SysclibForKernel 0x7ab35214 @ 0x6334 */
	jr	$ra
	nop
	.globl	strnlen
	.type	strnlen, @function
strnlen: /* SysclibForKernel 0x90c5573d @ 0x633c */
	jr	$ra
	nop
	.globl	memcpy
	.type	memcpy, @function
memcpy: /* SysclibForKernel 0xab7592ff @ 0x6344 */
	jr	$ra
	nop
	.globl	strncpy
	.type	strncpy, @function
strncpy: /* SysclibForKernel 0xb49a7697 @ 0x634c */
	jr	$ra
	nop
	.globl	sceKernelDelayThread
	.type	sceKernelDelayThread, @function
sceKernelDelayThread: /* ThreadManForKernel 0xceadeb47 @ 0x6354 */
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
	.half	3
	.word	.L_exp_tbl_0
	.word	.L_exp_lib_1
	.byte	17, 0
	.half	0x4001
	.byte	5, 0
	.half	20
	.word	.L_exp_tbl_1
	.word	0x00020000
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 2
	.half	21
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
	.byte	6, 0
	.half	1
	.word	.L_imp_nid_0
	.word	sceKernelDelayThread
	.word	0
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	5
	.word	.L_imp_nid_1
	.word	memset

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
	.byte	7, 1
	.asciz	"sceRegistry_Service"
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
	.space	3
.L_exp_tbl_0:
	.word	0xd632acdb
	.word	0x2f064fa6
	.word	0xadf12745
	.word	0xf01d73a7
	.word	0x11b97506
	.word	sceReg_driver_30E567D5 /* module_start (0xd632acdb) */
	.word	module_reboot_before /* module_reboot_before (0x2f064fa6) */
	.word	module_reboot_phase /* module_reboot_phase (0xadf12745) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"sceReg"
	.space	1
.L_exp_tbl_1:
	.word	0x0cae832b
	.word	0x0d69bf40
	.word	0x17768e14
	.word	0x1d8a762e
	.word	0x28a8e98a
	.word	0x2c0db9dd
	.word	0x2d211135
	.word	0x30be0259
	.word	0x3615bc87
	.word	0x39461b4d
	.word	0x4ca16893
	.word	0x57641a81
	.word	0x835ece6f
	.word	0x92e41280
	.word	0x9b25edf1
	.word	0xbe8c1263
	.word	0xc5768d02
	.word	0xd4475aa8
	.word	0xdeda92bf
	.word	0xfa8a5739
	.word	sceRegCloseCategory /* sceRegCloseCategory (0x0cae832b) */
	.word	sceRegFlushCategory /* sceRegFlushCategory (0x0d69bf40) */
	.word	sceRegSetKeyValue /* sceRegSetKeyValue (0x17768e14) */
	.word	sceRegOpenCategory /* sceRegOpenCategory (0x1d8a762e) */
	.word	sceRegGetKeyValue /* sceRegGetKeyValue (0x28a8e98a) */
	.word	sceReg_driver_D323FC94 /* sceRegGetKeysNum (0x2c0db9dd) */
	.word	sceReg_driver_BAE3371A /* sceRegGetKeys (0x2d211135) */
	.word	sceRegGetKeyValueByName /* sceRegGetKeyValueByName (0x30be0259) */
	.word	sceReg_driver_405D36C3 /* sceRegRemoveKey (0x3615bc87) */
	.word	sceRegFlushRegistry /* sceRegFlushRegistry (0x39461b4d) */
	.word	sceRegRemoveCategory /* sceRegRemoveCategory (0x4ca16893) */
	.word	sceRegCreateKey /* sceRegCreateKey (0x57641a81) */
	.word	sceReg_driver_6328E4C6 /* sceRegGetCategoryListAtRoot (0x835ece6f) */
	.word	sceRegOpenRegistry /* sceRegOpenRegistry (0x92e41280) */
	.word	sceReg_driver_BDE681CF /* sceRegExit (0x9b25edf1) */
	.word	sceReg_driver_90AA3904 /* sceRegGetCategoryNumAtRoot (0xbe8c1263) */
	.word	sceRegGetKeyInfoByName /* sceRegGetKeyInfoByName (0xc5768d02) */
	.word	sceRegGetKeyInfo /* sceRegGetKeyInfo (0xd4475aa8) */
	.word	sceReg_driver_C3DEEBC6 /* sceRegRemoveRegistry (0xdeda92bf) */
	.word	sceRegCloseRegistry /* sceRegCloseRegistry (0xfa8a5739) */
	.word	0x00020000
	.word	0x00070004
	.word	0x000b000a
	.word	0x000c000c
	.word	0x000d000c
	.word	0x000f000f
	.word	0x00110010
	.word	0x00130013
.L_exp_lib_2:
	.asciz	"sceReg_driver"
	.space	2
.L_exp_tbl_2:
	.word	0x30e567d5
	.word	0x38415b9f
	.word	0x3b6ca1e6
	.word	0x405d36c3
	.word	0x49c70163
	.word	0x49d77d65
	.word	0x4f471457
	.word	0x5fd4764a
	.word	0x61db9d06
	.word	0x6328e4c6
	.word	0x90aa3904
	.word	0x9980519f
	.word	0xbae3371a
	.word	0xbde681cf
	.word	0xc3deebc6
	.word	0xd323fc94
	.word	0xd743a608
	.word	0xdba46704
	.word	0xf2619407
	.word	0xf4a3e396
	.word	0xfc742751
	.word	0x5105e3ff
	.word	0x867d5f60
	.word	sceReg_driver_30E567D5 /* sceReg_driver_30E567D5 (0x30e567d5) */
	.word	sceRegGetKeyValueByName /* sceRegGetKeyValueByName (0x38415b9f) */
	.word	sceRegCreateKey /* sceRegCreateKey (0x3b6ca1e6) */
	.word	sceReg_driver_405D36C3 /* sceReg_driver_405D36C3 (0x405d36c3) */
	.word	sceRegSetKeyValue /* sceRegSetKeyValue (0x49c70163) */
	.word	sceRegCloseRegistry /* sceRegCloseRegistry (0x49d77d65) */
	.word	sceRegOpenCategory /* sceRegOpenCategory (0x4f471457) */
	.word	sceRegFlushRegistry /* sceRegFlushRegistry (0x5fd4764a) */
	.word	sceRegRemoveCategory /* sceRegRemoveCategory (0x61db9d06) */
	.word	sceReg_driver_6328E4C6 /* sceReg_driver_6328E4C6 (0x6328e4c6) */
	.word	sceReg_driver_90AA3904 /* sceReg_driver_90AA3904 (0x90aa3904) */
	.word	sceRegGetKeyInfo /* sceRegGetKeyInfo (0x9980519f) */
	.word	sceReg_driver_BAE3371A /* sceReg_driver_BAE3371A (0xbae3371a) */
	.word	sceReg_driver_BDE681CF /* sceReg_driver_BDE681CF (0xbde681cf) */
	.word	sceReg_driver_C3DEEBC6 /* sceReg_driver_C3DEEBC6 (0xc3deebc6) */
	.word	sceReg_driver_D323FC94 /* sceReg_driver_D323FC94 (0xd323fc94) */
	.word	sceRegFlushCategory /* sceRegFlushCategory (0xd743a608) */
	.word	sceRegOpenRegistry /* sceRegOpenRegistry (0xdba46704) */
	.word	sceRegGetKeyInfoByName /* sceRegGetKeyInfoByName (0xf2619407) */
	.word	sceRegGetKeyValue /* sceRegGetKeyValue (0xf4a3e396) */
	.word	sceRegCloseCategory /* sceRegCloseCategory (0xfc742751) */
	.word	sceReg_driver_var_5105E3FF /* sceReg_driver_var_5105E3FF (0x5105e3ff) */
	.word	sceReg_driver_var_867D5F60 /* sceReg_driver_var_867D5F60 (0x867d5f60) */
	.word	0x00000000
	.word	0x00000000
	.word	0x00070003
	.word	0x000a0008
	.word	0x000a000a
	.word	0x000c000c
	.word	0x000f000e
	.word	0x00120012

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0xceadeb47
.L_imp_nid_1:
	.word	0x10f3bb61
	.word	0x7ab35214
	.word	0x90c5573d
	.word	0xab7592ff
	.word	0xb49a7697

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
