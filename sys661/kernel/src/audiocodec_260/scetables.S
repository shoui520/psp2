/* Generated PSP module import/export tables for sceAudiocodec_Driver */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelDcacheWritebackInvalidateRange
	.type	sceKernelDcacheWritebackInvalidateRange, @function
sceKernelDcacheWritebackInvalidateRange: /* UtilsForKernel 0x34b9fa9e @ 0x0b20 */
	jr	$ra
	nop
	.globl	sceKernelDcacheWritebackRange
	.type	sceKernelDcacheWritebackRange, @function
sceKernelDcacheWritebackRange: /* UtilsForKernel 0x3ee30821 @ 0x0b28 */
	jr	$ra
	nop
	.globl	sceMeAudio_driver_6AD33F60
	.type	sceMeAudio_driver_6AD33F60, @function
sceMeAudio_driver_6AD33F60: /* sceMeAudio_driver 0x6ad33f60 @ 0x0b30 */
	jr	$ra
	nop
	.globl	sceMeAudio_driver_81956A0B
	.type	sceMeAudio_driver_81956A0B, @function
sceMeAudio_driver_81956A0B: /* sceMeAudio_driver 0x81956a0b @ 0x0b38 */
	jr	$ra
	nop
	.globl	sceMeAudio_driver_9A9E21EE
	.type	sceMeAudio_driver_9A9E21EE, @function
sceMeAudio_driver_9A9E21EE: /* sceMeAudio_driver 0x9a9e21ee @ 0x0b40 */
	jr	$ra
	nop
	.globl	sceMeAudio_driver_B57F033A
	.type	sceMeAudio_driver_B57F033A, @function
sceMeAudio_driver_B57F033A: /* sceMeAudio_driver 0xb57f033a @ 0x0b48 */
	jr	$ra
	nop
	.globl	sceMeAudio_driver_C300D466
	.type	sceMeAudio_driver_C300D466, @function
sceMeAudio_driver_C300D466: /* sceMeAudio_driver 0xc300d466 @ 0x0b50 */
	jr	$ra
	nop
	.globl	sceMeFree
	.type	sceMeFree, @function
sceMeFree: /* sceMeMemory_driver 0x6ed69327 @ 0x0b58 */
	jr	$ra
	nop
	.globl	sceMeMalloc
	.type	sceMeMalloc, @function
sceMeMalloc: /* sceMeMemory_driver 0x92d3baa1 @ 0x0b60 */
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
	.half	8
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
	.half	2
	.word	.L_imp_nid_0
	.word	sceMeFree
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	5
	.word	.L_imp_nid_1
	.word	sceMeAudio_driver_6AD33F60
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_2
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
	.asciz	"sceAudiocodec_Driver"
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
	.asciz	"sceMeMemory_driver"
	.space	5
.L_imp_lib_1:
	.asciz	"sceMeAudio_driver"
	.space	6
.L_imp_lib_2:
	.asciz	"UtilsForKernel"
	.space	1
.L_exp_tbl_0:
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"sceAudiocodec"
	.space	2
.L_exp_tbl_1:
	.word	0x29681260
	.word	0x3a20a200
	.word	0x3dd7ee1a
	.word	0x59176a0f
	.word	0x5b37eb1d
	.word	0x70a703f8
	.word	0x8aca11d5
	.word	0x9d3f790c
	.word	sceAudiocodecReleaseEDRAM /* sceAudiocodecReleaseEDRAM (0x29681260) */
	.word	sceAudiocodecGetEDRAM /* sceAudiocodecGetEDRAM (0x3a20a200) */
	.word	sceAudiocodec_3DD7EE1A /* sceAudiocodec_3DD7EE1A (0x3dd7ee1a) */
	.word	sceAudiocodecAlcExtendParameter /* sceAudiocodecAlcExtendParameter (0x59176a0f) */
	.word	sceAudiocodecInit /* sceAudiocodecInit (0x5b37eb1d) */
	.word	sceAudiocodecDecode /* sceAudiocodecDecode (0x70a703f8) */
	.word	sceAudiocodecGetInfo /* sceAudiocodecGetInfo (0x8aca11d5) */
	.word	sceAudiocodecCheckNeedMem /* sceAudiocodecCheckNeedMem (0x9d3f790c) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x6ed69327
	.word	0x92d3baa1
.L_imp_nid_1:
	.word	0x6ad33f60
	.word	0x81956a0b
	.word	0x9a9e21ee
	.word	0xb57f033a
	.word	0xc300d466
.L_imp_nid_2:
	.word	0x34b9fa9e
	.word	0x3ee30821

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
