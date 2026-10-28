/* Generated PSP module import/export tables for sceIdStorage_Service */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	Kprintf
	.type	Kprintf, @function
Kprintf: /* KDebugForKernel 0x84f370bc @ 0x02b8 */
	jr	$ra
	nop
	.globl	sceKernelUnlockMutex
	.type	sceKernelUnlockMutex, @function
sceKernelUnlockMutex: /* ThreadManForKernel 0x6b30100f @ 0x02c0 */
	jr	$ra
	nop
	.globl	sceKernelLockMutex
	.type	sceKernelLockMutex, @function
sceKernelLockMutex: /* ThreadManForKernel 0xb011b11f @ 0x02c8 */
	jr	$ra
	nop
	.globl	sceKernelDelayThread
	.type	sceKernelDelayThread, @function
sceKernelDelayThread: /* ThreadManForKernel 0xceadeb47 @ 0x02d0 */
	jr	$ra
	nop
	.globl	sceKermitPeripheral_driver_C6F8B4E1
	.type	sceKermitPeripheral_driver_C6F8B4E1, @function
sceKermitPeripheral_driver_C6F8B4E1: /* sceKermitPeripheral_driver 0xc6f8b4e1 @ 0x02d8 */
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
	.byte	5, 0
	.half	18
	.word	.L_exp_tbl_1
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
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_0
	.word	Kprintf
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	3
	.word	.L_imp_nid_1
	.word	sceKernelUnlockMutex
	.word	0
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_2
	.word	sceKermitPeripheral_driver_C6F8B4E1

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
	.byte	4, 1
	.asciz	"sceIdStorage_Service"
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
	.asciz	"KDebugForKernel"
	.space	4
.L_imp_lib_1:
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_2:
	.asciz	"sceKermitPeripheral_driver"
	.space	1
.L_exp_tbl_0:
	.word	0xd632acdb
	.word	0x2f064fa6
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_start
	.word	module_reboot_before
	.word	module_info
	.word	module_sdk_version
.L_exp_lib_1:
	.asciz	"sceIdStorage_driver"
	.align	2
.L_exp_tbl_1:
	.word	0x08a471a6
	.word	0x1fa4d135
	.word	0x2c97ab36
	.word	0x2ce0be69
	.word	0x2d633688
	.word	0x31e08afb
	.word	0x37833cb8
	.word	0x3ad32523
	.word	0x683aac10
	.word	0x6fe062d1
	.word	0x958089db
	.word	0x99accb71
	.word	0xab129d20
	.word	0xb9069bad
	.word	0xeb00c509
	.word	0xeb830733
	.word	0xf4bcb3ee
	.word	0xfefa40c2
	.word	sceIdStorageCreateLeaf
	.word	sceIdStorageWriteLeaf
	.word	sceIdStorageDeleteLeaf
	.word	sceIdStorageEnd
	.word	sceIdStorageIsReadOnly
	.word	sceIdStorageEnumId
	.word	sceIdStorageGetFreeLeaves
	.word	sceIdStorageFlush
	.word	sceIdStorageUpdate
	.word	sceIdStorageLookup
	.word	sceIdStorageFormat
	.word	sceIdStorageCreateAtomicLeaves
	.word	sceIdStorageInit
	.word	sceIdStorageIsDirty
	.word	sceIdStorageReadLeaf
	.word	sceIdStorageGetLeafSize
	.word	sceIdStorageUnformat
	.word	sceIdStorageIsFormatted
	.word	0x00010000
	.word	0x00050002
	.word	0x00080008
	.word	0x000a0008
	.word	0x000a000a
	.word	0x000d000c
	.word	0x000e000e
	.word	0x0010000e

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x84f370bc
.L_imp_nid_1:
	.word	0x6b30100f
	.word	0xb011b11f
	.word	0xceadeb47
.L_imp_nid_2:
	.word	0xc6f8b4e1

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
