/* Generated PSP module import/export tables for sceOpenPSID_Service */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelCpuSuspendIntr
	.type	sceKernelCpuSuspendIntr, @function
sceKernelCpuSuspendIntr: /* InterruptManagerForKernel 0x092968f4 @ 0x351c */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntr
	.type	sceKernelCpuResumeIntr, @function
sceKernelCpuResumeIntr: /* InterruptManagerForKernel 0x5f10d406 @ 0x3524 */
	jr	$ra
	nop
	.globl	sceKernelGetQTGP3
	.type	sceKernelGetQTGP3, @function
sceKernelGetQTGP3: /* SysMemForKernel 0x43e0a861 @ 0x352c */
	jr	$ra
	nop
	.globl	sceKernelGetQTGP2
	.type	sceKernelGetQTGP2, @function
sceKernelGetQTGP2: /* SysMemForKernel 0x474ca24f @ 0x3534 */
	jr	$ra
	nop
	.globl	sceKernelGetGameInfo
	.type	sceKernelGetGameInfo, @function
sceKernelGetGameInfo: /* SysMemForKernel 0xef29061c @ 0x353c */
	jr	$ra
	nop
	.globl	sceKernelSignalSema
	.type	sceKernelSignalSema, @function
sceKernelSignalSema: /* ThreadManForKernel 0x3f53e640 @ 0x3544 */
	jr	$ra
	nop
	.globl	sceKernelWaitSema
	.type	sceKernelWaitSema, @function
sceKernelWaitSema: /* ThreadManForKernel 0x4e3a1105 @ 0x354c */
	jr	$ra
	nop
	.globl	sceKernelCreateSema
	.type	sceKernelCreateSema, @function
sceKernelCreateSema: /* ThreadManForKernel 0xd6da4ba1 @ 0x3554 */
	jr	$ra
	nop
	.globl	sceIdStorageLookup
	.type	sceIdStorageLookup, @function
sceIdStorageLookup: /* sceIdStorage_driver 0x6fe062d1 @ 0x355c */
	jr	$ra
	nop
	.globl	sceRtc_driver_CEEF238F
	.type	sceRtc_driver_CEEF238F, @function
sceRtc_driver_CEEF238F: /* sceRtc_driver 0xceef238f @ 0x3564 */
	jr	$ra
	nop
	.globl	sceUtilsBufferCopyWithRange
	.type	sceUtilsBufferCopyWithRange, @function
sceUtilsBufferCopyWithRange: /* semaphore 0x4c537c72 @ 0x356c */
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
	.half	1
	.word	.L_exp_tbl_0
	.word	.L_exp_lib_1
	.byte	17, 0
	.half	0x4001
	.byte	4, 0
	.half	3
	.word	.L_exp_tbl_1
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	4, 0
	.half	4
	.word	.L_exp_tbl_2
	.word	.L_exp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	4, 0
	.half	9
	.word	.L_exp_tbl_3
	.word	.L_exp_lib_4
	.byte	17, 0
	.half	0x0001
	.byte	4, 0
	.half	2
	.word	.L_exp_tbl_4
	.word	.L_exp_lib_5
	.byte	17, 0
	.half	0x0001
	.byte	4, 0
	.half	2
	.word	.L_exp_tbl_5
	.word	.L_exp_lib_6
	.byte	17, 0
	.half	0x4001
	.byte	4, 0
	.half	1
	.word	.L_exp_tbl_6

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
	.half	3
	.word	.L_imp_nid_0
	.word	sceKernelGetQTGP3
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	3
	.word	.L_imp_nid_1
	.word	sceKernelSignalSema
	.word	0
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
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_3
	.word	sceIdStorageLookup
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_4
	.word	sceRtc_driver_CEEF238F
	.word	.L_imp_lib_5
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_5
	.word	sceUtilsBufferCopyWithRange

	.section .lib.stub.btm, "a", @progbits
	.align	2
__lib_stub_btm:
	.word	0

	.section .rodata.sceModuleInfo, "a", @progbits
	.align	4
	.globl	module_info
	.type	module_info, @object
module_info:
	.half	0x5007
	.byte	16, 1
	.asciz	"sceOpenPSID_Service"
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
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_2:
	.asciz	"InterruptManagerForKernel"
	.space	6
.L_imp_lib_3:
	.asciz	"sceIdStorage_driver"
	.space	4
.L_imp_lib_4:
	.asciz	"sceRtc_driver"
	.space	6
.L_imp_lib_5:
	.asciz	"semaphore"
	.space	2
.L_exp_tbl_0:
	.word	0xd632acdb
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_start /* module_start (0xd632acdb) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"sceOpenPSID"
	.align	2
.L_exp_tbl_1:
	.word	0xae6dd68c
	.word	0xb29330de
	.word	0xc69bebce
	.word	sceOpenPSID_AE6DD68C /* sceOpenPSID_AE6DD68C (0xae6dd68c) */
	.word	sceOpenPSIDGetProductCode /* sceOpenPSIDGetProductCode (0xb29330de) */
	.word	sceOpenPSIDGetOpenPSID /* sceOpenPSIDGetOpenPSID (0xc69bebce) */
.L_exp_lib_2:
	.asciz	"sceOpenPSID_driver"
	.space	1
.L_exp_tbl_2:
	.word	0x19d579f0
	.word	0x8546569e
	.word	0xc69bebce
	.word	0xe8316c16
	.word	sceOpenPSIDGetPSID /* sceOpenPSIDGetPSID (0x19d579f0) */
	.word	sceOpenPSID_driver_8546569E /* sceOpenPSID_driver_8546569E (0x8546569e) */
	.word	sceOpenPSIDGetOpenPSID /* sceOpenPSIDGetOpenPSID (0xc69bebce) */
	.word	sceOpenPSID_driver_E8316C16 /* sceOpenPSID_driver_E8316C16 (0xe8316c16) */
.L_exp_lib_3:
	.asciz	"sceDdrdb_driver"
	.align	2
.L_exp_tbl_3:
	.word	0x05d50f41
	.word	0x370f456a
	.word	0x40cb752a
	.word	0xb24e1391
	.word	0xb33acb44
	.word	0xb8218473
	.word	0xe27ce4cb
	.word	0xec05300a
	.word	0xf970d54e
	.word	sceDdrdbEncrypt /* sceDdrdbEncrypt (0x05d50f41) */
	.word	sceDdrdbCertvry /* sceDdrdbCertvry (0x370f456a) */
	.word	sceDdrdbHash /* sceDdrdbHash (0x40cb752a) */
	.word	sceDdrdbSiggen /* sceDdrdbSiggen (0xb24e1391) */
	.word	sceDdrdbDecrypt /* sceDdrdbDecrypt (0xb33acb44) */
	.word	sceDdrdbPrngen /* sceDdrdbPrngen (0xb8218473) */
	.word	sceDdrdbSigvry /* sceDdrdbSigvry (0xe27ce4cb) */
	.word	sceDdrdbMul2 /* sceDdrdbMul2 (0xec05300a) */
	.word	sceDdrdbMul1 /* sceDdrdbMul1 (0xf970d54e) */
.L_exp_lib_4:
	.asciz	"scePcact_driver"
	.align	2
.L_exp_tbl_4:
	.word	0x08bb9677
	.word	0xf9ecfddd
	.word	scePcactAuth2BB /* scePcactAuth2BB (0x08bb9677) */
	.word	scePcactAuth1BB /* scePcactAuth1BB (0xf9ecfddd) */
.L_exp_lib_5:
	.asciz	"sceMlnpsnl_driver"
	.space	2
.L_exp_tbl_5:
	.word	0x6885f392
	.word	0x8523e178
	.word	sceMlnpsnlAuth2BB /* sceMlnpsnlAuth2BB (0x6885f392) */
	.word	sceMlnpsnlAuth1BB /* sceMlnpsnlAuth1BB (0x8523e178) */
.L_exp_lib_6:
	.asciz	"sceDdrdb"
	.space	3
.L_exp_tbl_6:
	.word	0xf013f8bf
	.word	sceDdrdb_F013F8BF /* sceDdrdb_F013F8BF (0xf013f8bf) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x43e0a861
	.word	0x474ca24f
	.word	0xef29061c
.L_imp_nid_1:
	.word	0x3f53e640
	.word	0x4e3a1105
	.word	0xd6da4ba1
.L_imp_nid_2:
	.word	0x092968f4
	.word	0x5f10d406
.L_imp_nid_3:
	.word	0x6fe062d1
.L_imp_nid_4:
	.word	0xceef238f
.L_imp_nid_5:
	.word	0x4c537c72

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060010
