/* Generated PSP module import/export tables for sceAac_Library */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelCpuSuspendIntr
	.type	sceKernelCpuSuspendIntr, @function
sceKernelCpuSuspendIntr: /* Kernel_Library 0x092968f4 @ 0x1468 */
	jr	$ra
	nop
	.globl	sceKernelMemset
	.type	sceKernelMemset, @function
sceKernelMemset: /* Kernel_Library 0xa089eca4 @ 0x1470 */
	jr	$ra
	nop
	.globl	sceKernelMemcpy
	.type	sceKernelMemcpy, @function
sceKernelMemcpy: /* Kernel_Library 0x1839852a @ 0x1478 */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntr
	.type	sceKernelCpuResumeIntr, @function
sceKernelCpuResumeIntr: /* Kernel_Library 0x5f10d406 @ 0x1480 */
	jr	$ra
	nop
	.globl	sceKernelCreateFpl
	.type	sceKernelCreateFpl, @function
sceKernelCreateFpl: /* ThreadManForUser 0xc07bb470 @ 0x1488 */
	jr	$ra
	nop
	.globl	sceKernelAllocateFpl
	.type	sceKernelAllocateFpl, @function
sceKernelAllocateFpl: /* ThreadManForUser 0xd979e9bf @ 0x1490 */
	jr	$ra
	nop
	.globl	sceKernelDeleteFpl
	.type	sceKernelDeleteFpl, @function
sceKernelDeleteFpl: /* ThreadManForUser 0xed1410e0 @ 0x1498 */
	jr	$ra
	nop
	.globl	sceKernelFreeFpl
	.type	sceKernelFreeFpl, @function
sceKernelFreeFpl: /* ThreadManForUser 0xf6414a71 @ 0x14a0 */
	jr	$ra
	nop
	.globl	sceAudiocodecAlcExtendParameter
	.type	sceAudiocodecAlcExtendParameter, @function
sceAudiocodecAlcExtendParameter: /* sceAudiocodec 0x59176a0f @ 0x14a8 */
	jr	$ra
	nop
	.globl	sceAudiocodecInit
	.type	sceAudiocodecInit, @function
sceAudiocodecInit: /* sceAudiocodec 0x5b37eb1d @ 0x14b0 */
	jr	$ra
	nop
	.globl	sceAudiocodecDecode
	.type	sceAudiocodecDecode, @function
sceAudiocodecDecode: /* sceAudiocodec 0x70a703f8 @ 0x14b8 */
	jr	$ra
	nop
	.globl	sceAudiocodecGetInfo
	.type	sceAudiocodecGetInfo, @function
sceAudiocodecGetInfo: /* sceAudiocodec 0x8aca11d5 @ 0x14c0 */
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
	.byte	4, 4
	.half	2
	.word	.L_exp_tbl_0
	.word	.L_exp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	17
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
	.half	4
	.word	.L_imp_nid_0
	.word	sceKernelCpuSuspendIntr
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x4001
	.byte	5, 0
	.half	4
	.word	.L_imp_nid_1
	.word	sceKernelCreateFpl
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x4009
	.byte	5, 0
	.half	4
	.word	.L_imp_nid_2
	.word	sceAudiocodecAlcExtendParameter

	.section .lib.stub.btm, "a", @progbits
	.align	2
__lib_stub_btm:
	.word	0

	.section .rodata.sceModuleInfo, "a", @progbits
	.align	4
	.globl	module_info
	.type	module_info, @object
module_info:
	.half	0x0006
	.byte	1, 1
	.asciz	"sceAac_Library"
	.space	13
	.word	_gp
	.word	__lib_ent_top + 4
	.word	__lib_ent_btm
	.word	__lib_stub_top + 4
	.word	__lib_stub_btm

	.section .rodata.sceResident, "a", @progbits
	.align	2
	.word	0
.L_imp_lib_0:
	.asciz	"Kernel_Library"
	.space	5
.L_imp_lib_1:
	.asciz	"ThreadManForUser"
	.space	7
.L_imp_lib_2:
	.asciz	"sceAudiocodec"
	.space	2
.L_exp_tbl_0:
	.word	0xd632acdb
	.word	0xcee8593c
	.word	0xf01d73a7
	.word	0x11b97506
	.word	0x0f7c276c
	.word	0xcf0cc697
	.word	sceAacStartEntry /* module_start (0xd632acdb) */
	.word	sceAacEndEntry /* module_stop (0xcee8593c) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
	.word	module_start_thread_parameter /* module_start_thread_parameter (0x0f7c276c) */
	.word	module_stop_thread_parameter /* module_stop_thread_parameter (0xcf0cc697) */
.L_exp_lib_1:
	.asciz	"sceAac"
	.space	1
.L_exp_tbl_1:
	.word	0x02098c69
	.word	0x23d35cae
	.word	0x33b8c009
	.word	0x506bf66c
	.word	0x523347d9
	.word	0x5cffc57c
	.word	0x61aa43c9
	.word	0x6c05813b
	.word	0x6dc7758a
	.word	0x7e4cfee4
	.word	0xac6dcbe3
	.word	0xbbdd6403
	.word	0xd2da2bba
	.word	0xd7c51541
	.word	0xe0c89aca
	.word	0xe955e83a
	.word	0xfa01fcb6
	.word	sceAacGetInfoToAddStreamData /* sceAacGetInfoToAddStreamData (0x02098c69) */
	.word	sceAacTermResource /* sceAacTermResource (0x23d35cae) */
	.word	sceAacExit /* sceAacExit (0x33b8c009) */
	.word	sceAacGetSumDecodedSample /* sceAacGetSumDecodedSample (0x506bf66c) */
	.word	sceAacGetLoopNum /* sceAacGetLoopNum (0x523347d9) */
	.word	sceAacInitResource /* sceAacInitResource (0x5cffc57c) */
	.word	sceAacEndEntry /* sceAacEndEntry (0x61aa43c9) */
	.word	sceAacStartEntry /* sceAacStartEntry (0x6c05813b) */
	.word	sceAacGetMaxOutputSample /* sceAacGetMaxOutputSample (0x6dc7758a) */
	.word	sceAacDecode /* sceAacDecode (0x7e4cfee4) */
	.word	sceAacNotifyAddStreamData /* sceAacNotifyAddStreamData (0xac6dcbe3) */
	.word	sceAacSetLoopNum /* sceAacSetLoopNum (0xbbdd6403) */
	.word	sceAacResetPlayPosition /* sceAacResetPlayPosition (0xd2da2bba) */
	.word	sceAacCheckStreamDataNeeded /* sceAacCheckStreamDataNeeded (0xd7c51541) */
	.word	sceAacInit /* sceAacInit (0xe0c89aca) */
	.word	sceAac_E955E83A /* sceAac_E955E83A (0xe955e83a) */
	.word	sceAac_FA01FCB6 /* sceAac_FA01FCB6 (0xfa01fcb6) */
	.word	0x00010000
	.word	0x00020001
	.word	0x00030003
	.word	0x00090006
	.word	0x000a000a
	.word	0x000b000a
	.word	0x000c000c
	.word	0x0010000e

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x092968f4
	.word	0xa089eca4
	.word	0x1839852a
	.word	0x5f10d406
.L_imp_nid_1:
	.word	0xc07bb470
	.word	0xd979e9bf
	.word	0xed1410e0
	.word	0xf6414a71
.L_imp_nid_2:
	.word	0x59176a0f
	.word	0x5b37eb1d
	.word	0x70a703f8
	.word	0x8aca11d5

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
