/* Generated PSP module import/export tables for sceChnnlsv */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	memset
	.type	memset, @function
memset: /* SysclibForKernel 0x10f3bb61 @ 0x1870 */
	jr	$ra
	nop
	.globl	sceKernelDeleteSema
	.type	sceKernelDeleteSema, @function
sceKernelDeleteSema: /* ThreadManForKernel 0x28b6489c @ 0x1878 */
	jr	$ra
	nop
	.globl	sceKernelSignalSema
	.type	sceKernelSignalSema, @function
sceKernelSignalSema: /* ThreadManForKernel 0x3f53e640 @ 0x1880 */
	jr	$ra
	nop
	.globl	sceKernelWaitSema
	.type	sceKernelWaitSema, @function
sceKernelWaitSema: /* ThreadManForKernel 0x4e3a1105 @ 0x1888 */
	jr	$ra
	nop
	.globl	sceKernelCreateSema
	.type	sceKernelCreateSema, @function
sceKernelCreateSema: /* ThreadManForKernel 0xd6da4ba1 @ 0x1890 */
	jr	$ra
	nop
	.globl	sceKernelDcacheWritebackRange
	.type	sceKernelDcacheWritebackRange, @function
sceKernelDcacheWritebackRange: /* UtilsForKernel 0x3ee30821 @ 0x1898 */
	jr	$ra
	nop
	.globl	sceUtilsBufferCopyWithRange
	.type	sceUtilsBufferCopyWithRange, @function
sceUtilsBufferCopyWithRange: /* semaphore 0x4c537c72 @ 0x18a0 */
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
	.half	6
	.word	.L_exp_tbl_1
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	4, 0
	.half	6
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
	.half	1
	.word	.L_imp_nid_0
	.word	memset
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	4
	.word	.L_imp_nid_1
	.word	sceKernelDeleteSema
	.word	0
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_2
	.word	sceKernelDcacheWritebackRange
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_3
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
	.half	0x5006
	.byte	6, 1
	.asciz	"sceChnnlsv"
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
	.asciz	"SysclibForKernel"
	.space	7
.L_imp_lib_1:
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_2:
	.asciz	"UtilsForKernel"
	.space	5
.L_imp_lib_3:
	.asciz	"semaphore"
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
	.asciz	"sceChnnlsv"
	.space	1
.L_exp_tbl_1:
	.word	0x21be78b4
	.word	0x850a7fa1
	.word	0xabfdfc8b
	.word	0xc4c494f8
	.word	0xe7833020
	.word	0xf21a1fca
	.word	sceSdCleanList /* sceSdCleanList (0x21be78b4) */
	.word	sceSdSetMember /* sceSdSetMember (0x850a7fa1) */
	.word	sceSdCreateList /* sceSdCreateList (0xabfdfc8b) */
	.word	sceSdGetLastIndex /* sceSdGetLastIndex (0xc4c494f8) */
	.word	sceSdSetIndex /* sceSdSetIndex (0xe7833020) */
	.word	sceSdRemoveValue /* sceSdRemoveValue (0xf21a1fca) */
.L_exp_lib_2:
	.asciz	"sceChnnlsv_driver"
	.space	2
.L_exp_tbl_2:
	.word	0x21be78b4
	.word	0x850a7fa1
	.word	0xabfdfc8b
	.word	0xc4c494f8
	.word	0xe7833020
	.word	0xf21a1fca
	.word	sceSdCleanList /* sceSdCleanList (0x21be78b4) */
	.word	sceSdSetMember /* sceSdSetMember (0x850a7fa1) */
	.word	sceSdCreateList /* sceSdCreateList (0xabfdfc8b) */
	.word	sceSdGetLastIndex /* sceSdGetLastIndex (0xc4c494f8) */
	.word	sceSdSetIndex /* sceSdSetIndex (0xe7833020) */
	.word	sceSdRemoveValue /* sceSdRemoveValue (0xf21a1fca) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x10f3bb61
.L_imp_nid_1:
	.word	0x28b6489c
	.word	0x3f53e640
	.word	0x4e3a1105
	.word	0xd6da4ba1
.L_imp_nid_2:
	.word	0x3ee30821
.L_imp_nid_3:
	.word	0x4c537c72

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060010
