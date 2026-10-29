/* Generated PSP module import/export tables for sceMesgLed */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelDeleteSema
	.type	sceKernelDeleteSema, @function
sceKernelDeleteSema: /* ThreadManForKernel 0x28b6489c @ 0x8ea0 */
	jr	$ra
	nop
	.globl	sceKernelSignalSema
	.type	sceKernelSignalSema, @function
sceKernelSignalSema: /* ThreadManForKernel 0x3f53e640 @ 0x8ea8 */
	jr	$ra
	nop
	.globl	sceKernelWaitSema
	.type	sceKernelWaitSema, @function
sceKernelWaitSema: /* ThreadManForKernel 0x4e3a1105 @ 0x8eb0 */
	jr	$ra
	nop
	.globl	sceKernelPollSema
	.type	sceKernelPollSema, @function
sceKernelPollSema: /* ThreadManForKernel 0x58b1f937 @ 0x8eb8 */
	jr	$ra
	nop
	.globl	sceKernelCreateSema
	.type	sceKernelCreateSema, @function
sceKernelCreateSema: /* ThreadManForKernel 0xd6da4ba1 @ 0x8ec0 */
	jr	$ra
	nop
	.globl	sceUtilsBufferCopyWithRange
	.type	sceUtilsBufferCopyWithRange, @function
sceUtilsBufferCopyWithRange: /* semaphore 0x4c537c72 @ 0x8ec8 */
	jr	$ra
	nop
	.globl	sceUtilsBufferCopyByPollingWithRange
	.type	sceUtilsBufferCopyByPollingWithRange, @function
sceUtilsBufferCopyByPollingWithRange: /* semaphore 0x77e97079 @ 0x8ed0 */
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
	.half	0x0009
	.byte	5, 0
	.half	25
	.word	.L_exp_tbl_1
	.word	0x00020000
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x0009
	.byte	4, 0
	.half	2
	.word	.L_exp_tbl_2
	.word	.L_exp_lib_3
	.byte	17, 0
	.half	0x0009
	.byte	4, 0
	.half	2
	.word	.L_exp_tbl_3
	.word	.L_exp_lib_4
	.byte	17, 0
	.half	0x0009
	.byte	4, 0
	.half	1
	.word	.L_exp_tbl_4
	.word	.L_exp_lib_5
	.byte	17, 0
	.half	0x0009
	.byte	4, 0
	.half	2
	.word	.L_exp_tbl_5
	.word	.L_exp_lib_6
	.byte	17, 0
	.half	0x4009
	.byte	4, 0
	.half	2
	.word	.L_exp_tbl_6
	.word	.L_exp_lib_7
	.byte	17, 0
	.half	0x0009
	.byte	4, 0
	.half	2
	.word	.L_exp_tbl_7
	.word	.L_exp_lib_8
	.byte	17, 0
	.half	0x0009
	.byte	4, 0
	.half	2
	.word	.L_exp_tbl_8
	.word	.L_exp_lib_9
	.byte	17, 0
	.half	0x0009
	.byte	4, 0
	.half	1
	.word	.L_exp_tbl_9
	.word	.L_exp_lib_10
	.byte	17, 0
	.half	0x0009
	.byte	4, 0
	.half	2
	.word	.L_exp_tbl_10
	.word	.L_exp_lib_11
	.byte	17, 0
	.half	0x0009
	.byte	4, 0
	.half	2
	.word	.L_exp_tbl_11
	.word	.L_exp_lib_12
	.byte	17, 0
	.half	0x4009
	.byte	4, 0
	.half	2
	.word	.L_exp_tbl_12

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
	.word	sceKernelDeleteSema
	.word	0
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_1
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
	.half	0x7007
	.byte	29, 1
	.asciz	"sceMesgLed"
	.space	17
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
	.asciz	"sceMesgLed_driver"
	.space	2
.L_exp_tbl_1:
	.word	0x21affaac
	.word	0x2cb700ec
	.word	0x308d37ff
	.word	0x31d6d8aa
	.word	0x337d0dd3
	.word	0x3783b0ad
	.word	0x4be02a12
	.word	0x4eab9850
	.word	0x52b6e552
	.word	0x5c3a61fe
	.word	0x66b348b2
	.word	0x6bf453d3
	.word	0x792a6126
	.word	0x91e0a9ad
	.word	0x9e3c79d9
	.word	0xb2cdac3f
	.word	0xb2d95fdf
	.word	0xb5596be4
	.word	0xc00dad75
	.word	0xc79e3488
	.word	0xc7d1c16b
	.word	0xc9abd2f2
	.word	0xced2c075
	.word	0xebb4613d
	.word	0xf8485c9c
	.word	sceUtilsGetLoadModuleMLength /* sceUtilsGetLoadModuleMLength (0x21affaac) */
	.word	sceUtilsGetLoadModuleDLength /* sceUtilsGetLoadModuleDLength (0x2cb700ec) */
	.word	sceUtilsGetLoadModuleDLengthByPolling /* sceUtilsGetLoadModuleDLengthByPolling (0x308d37ff) */
	.word	sceUtilsGetLoadModuleYLength /* sceUtilsGetLoadModuleYLength (0x31d6d8aa) */
	.word	sceUtilsGetLoadModuleILength /* sceUtilsGetLoadModuleILength (0x337d0dd3) */
	.word	sceUtilsGetLoadModuleCLengthByPolling /* sceUtilsGetLoadModuleCLengthByPolling (0x3783b0ad) */
	.word	sceUtilsGetLoadModuleJLengthByPolling /* sceUtilsGetLoadModuleJLengthByPolling (0x4be02a12) */
	.word	sceUtilsGetLoadModuleJLength /* sceUtilsGetLoadModuleJLength (0x4eab9850) */
	.word	sceUtilsGetLoadModuleMLengthByPolling /* sceUtilsGetLoadModuleMLengthByPolling (0x52b6e552) */
	.word	sceUtilsGetLoadModuleCLength /* sceUtilsGetLoadModuleCLength (0x5c3a61fe) */
	.word	sceUtilsGetLoadModuleULength /* sceUtilsGetLoadModuleULength (0x66b348b2) */
	.word	sceUtilsGetLoadModuleLLengthByPolling /* sceUtilsGetLoadModuleLLengthByPolling (0x6bf453d3) */
	.word	sceUtilsGetLoadModuleILengthByPolling /* sceUtilsGetLoadModuleILengthByPolling (0x792a6126) */
	.word	sceUtilsGetLoadModuleWLength /* sceUtilsGetLoadModuleWLength (0x91e0a9ad) */
	.word	sceUtilsGetModuleExpireTick /* sceUtilsGetModuleExpireTick (0x9e3c79d9) */
	.word	sceUtilsGetLoadModuleKLength /* sceUtilsGetLoadModuleKLength (0xb2cdac3f) */
	.word	sceUtilsGetLoadModuleVLength /* sceUtilsGetLoadModuleVLength (0xb2d95fdf) */
	.word	sceUtilsGetLoadModuleKLengthByPolling /* sceUtilsGetLoadModuleKLengthByPolling (0xb5596be4) */
	.word	sceUtilsGetLoadModuleNLength /* sceUtilsGetLoadModuleNLength (0xc00dad75) */
	.word	sceUtilsGetLoadModuleLLength /* sceUtilsGetLoadModuleLLength (0xc79e3488) */
	.word	sceUtilsGetLoadModuleSLength /* sceUtilsGetLoadModuleSLength (0xc7d1c16b) */
	.word	sceUtilsGetLoadModuleVLengthByPolling /* sceUtilsGetLoadModuleVLengthByPolling (0xc9abd2f2) */
	.word	sceUtilsGetLoadModuleRLength /* sceUtilsGetLoadModuleRLength (0xced2c075) */
	.word	sceUtilsGetLoadModuleTLength /* sceUtilsGetLoadModuleTLength (0xebb4613d) */
	.word	sceUtilsGetLoadModuleNLengthByPolling /* sceUtilsGetLoadModuleNLengthByPolling (0xf8485c9c) */
	.word	0x00000000
	.word	0x00020000
	.word	0x00080006
	.word	0x000c000a
	.word	0x000d000d
	.word	0x000f000f
	.word	0x00170012
	.word	0x00180017
.L_exp_lib_2:
	.asciz	"sceResmap_driver"
	.space	3
.L_exp_tbl_2:
	.word	0x4434e59f
	.word	0xe5659590
	.word	sceResmapPrepareByPolling /* sceResmapPrepareByPolling (0x4434e59f) */
	.word	sceResmapPrepare /* sceResmapPrepare (0xe5659590) */
.L_exp_lib_3:
	.asciz	"sceDbman_driver"
	.align	2
.L_exp_tbl_3:
	.word	0x34b53d46
	.word	0xb2b8c3f9
	.word	sceDbmanSelectByPolling /* sceDbmanSelectByPolling (0x34b53d46) */
	.word	sceDbmanSelect /* sceDbmanSelect (0xb2b8c3f9) */
.L_exp_lib_4:
	.asciz	"sceNwman_driver"
	.align	2
.L_exp_tbl_4:
	.word	0x9555d68d
	.word	sceNwman_driver_9555D68D /* sceNwman_driver_9555D68D (0x9555d68d) */
.L_exp_lib_5:
	.asciz	"sceResmgr_driver"
	.space	3
.L_exp_tbl_5:
	.word	0x8e6c62c8
	.word	0x9dc14891
	.word	sceResmgr_8E6C62C8 /* sceResmgr_8E6C62C8 (0x8e6c62c8) */
	.word	sceResmgr_9DC14891 /* sceResmgr_9DC14891 (0x9dc14891) */
.L_exp_lib_6:
	.asciz	"sceResmgr"
	.space	2
.L_exp_tbl_6:
	.word	0x8e6c62c8
	.word	0x9dc14891
	.word	sceResmgr_8E6C62C8 /* sceResmgr_8E6C62C8 (0x8e6c62c8) */
	.word	sceResmgr_9DC14891 /* sceResmgr_9DC14891 (0x9dc14891) */
.L_exp_lib_7:
	.asciz	"sceMesgd_driver"
	.align	2
.L_exp_tbl_7:
	.word	0x102dc8af
	.word	0xadd0cb66
	.word	sceMesgd_driver_102DC8AF /* sceMesgd_driver_102DC8AF (0x102dc8af) */
	.word	sceMesgd_driver_ADD0CB66 /* sceMesgd_driver_ADD0CB66 (0xadd0cb66) */
.L_exp_lib_8:
	.asciz	"sceWmd_driver"
	.space	2
.L_exp_tbl_8:
	.word	0x7a0e484c
	.word	0xb7ce9041
	.word	sceWmd_driver_7A0E484C /* sceWmd_driver_7A0E484C (0x7a0e484c) */
	.word	sceWmd_driver_B7CE9041 /* sceWmd_driver_B7CE9041 (0xb7ce9041) */
.L_exp_lib_9:
	.asciz	"sceDbsvr_driver"
	.align	2
.L_exp_tbl_9:
	.word	0x94561901
	.word	sceDbsvrGetData /* sceDbsvrGetData (0x94561901) */
.L_exp_lib_10:
	.asciz	"sceMesgIns_driver"
	.space	2
.L_exp_tbl_10:
	.word	0x4a03f940
	.word	0xd062b635
	.word	sceUtilsPrepareGetLoadModuleSLength /* sceUtilsPrepareGetLoadModuleSLength (0x4a03f940) */
	.word	sceUtilsPrepareGetLoadModuleRLength /* sceUtilsPrepareGetLoadModuleRLength (0xd062b635) */
.L_exp_lib_11:
	.asciz	"scePauth_driver"
	.align	2
.L_exp_tbl_11:
	.word	0x98b83b5d
	.word	0xf7aa47f6
	.word	scePauth_98B83B5D /* scePauth_98B83B5D (0x98b83b5d) */
	.word	scePauth_F7AA47F6 /* scePauth_F7AA47F6 (0xf7aa47f6) */
.L_exp_lib_12:
	.asciz	"scePauth"
	.space	3
.L_exp_tbl_12:
	.word	0x98b83b5d
	.word	0xf7aa47f6
	.word	scePauth_98B83B5D /* scePauth_98B83B5D (0x98b83b5d) */
	.word	scePauth_F7AA47F6 /* scePauth_F7AA47F6 (0xf7aa47f6) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x28b6489c
	.word	0x3f53e640
	.word	0x4e3a1105
	.word	0x58b1f937
	.word	0xd6da4ba1
.L_imp_nid_1:
	.word	0x4c537c72
	.word	0x77e97079

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060010
