/* Generated PSP module import/export tables for sceATRAC3plus_Library */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelCpuSuspendIntr
	.type	sceKernelCpuSuspendIntr, @function
sceKernelCpuSuspendIntr: /* Kernel_Library 0x092968f4 @ 0x3bb0 */
	jr	$ra
	nop
	.globl	sceKernelMemcpy
	.type	sceKernelMemcpy, @function
sceKernelMemcpy: /* Kernel_Library 0x1839852a @ 0x3bb8 */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntr
	.type	sceKernelCpuResumeIntr, @function
sceKernelCpuResumeIntr: /* Kernel_Library 0x5f10d406 @ 0x3bc0 */
	jr	$ra
	nop
	.globl	sceAudiocodecReleaseEDRAM
	.type	sceAudiocodecReleaseEDRAM, @function
sceAudiocodecReleaseEDRAM: /* sceAudiocodec 0x29681260 @ 0x3bc8 */
	jr	$ra
	nop
	.globl	sceAudiocodecGetEDRAM
	.type	sceAudiocodecGetEDRAM, @function
sceAudiocodecGetEDRAM: /* sceAudiocodec 0x3a20a200 @ 0x3bd0 */
	jr	$ra
	nop
	.globl	sceAudiocodec_3DD7EE1A
	.type	sceAudiocodec_3DD7EE1A, @function
sceAudiocodec_3DD7EE1A: /* sceAudiocodec 0x3dd7ee1a @ 0x3bd8 */
	jr	$ra
	nop
	.globl	sceAudiocodecInit
	.type	sceAudiocodecInit, @function
sceAudiocodecInit: /* sceAudiocodec 0x5b37eb1d @ 0x3be0 */
	jr	$ra
	nop
	.globl	sceAudiocodecDecode
	.type	sceAudiocodecDecode, @function
sceAudiocodecDecode: /* sceAudiocodec 0x70a703f8 @ 0x3be8 */
	jr	$ra
	nop
	.globl	sceAudiocodecCheckNeedMem
	.type	sceAudiocodecCheckNeedMem, @function
sceAudiocodecCheckNeedMem: /* sceAudiocodec 0x9d3f790c @ 0x3bf0 */
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
	.half	36
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
	.half	3
	.word	.L_imp_nid_0
	.word	sceKernelCpuSuspendIntr
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x4001
	.byte	5, 0
	.half	6
	.word	.L_imp_nid_1
	.word	sceAudiocodecReleaseEDRAM

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
	.byte	5, 1
	.asciz	"sceATRAC3plus_Library"
	.space	6
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
	.asciz	"sceAudiocodec"
	.space	2
.L_exp_tbl_0:
	.word	0xd632acdb
	.word	0xcee8593c
	.word	0xf01d73a7
	.word	0x11b97506
	.word	0x0f7c276c
	.word	0xcf0cc697
	.word	sceAtracStartEntry /* module_start (0xd632acdb) */
	.word	sceAtracEndEntry /* module_stop (0xcee8593c) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
	.word	module_start_thread_parameter /* module_start_thread_parameter (0x0f7c276c) */
	.word	module_stop_thread_parameter /* module_stop_thread_parameter (0xcf0cc697) */
.L_exp_lib_1:
	.asciz	"sceAtrac3plus"
	.space	2
.L_exp_tbl_1:
	.word	0x0c116e1b
	.word	0x0e2a73ab
	.word	0x0fae370e
	.word	0x132f1eca
	.word	0x1575d64b
	.word	0x231fc6b7
	.word	0x2dd3e298
	.word	0x31668baa
	.word	0x36faabfb
	.word	0x3f6e26b5
	.word	0x5622b7c1
	.word	0x5cf9d852
	.word	0x5d268707
	.word	0x5dd66588
	.word	0x61eb33f5
	.word	0x644e5607
	.word	0x6a8c3cd5
	.word	0x780f88d1
	.word	0x7a20e7af
	.word	0x7db31251
	.word	0x83bf7afd
	.word	0x83e85ea0
	.word	0x868120b5
	.word	0x9ae849a7
	.word	0x9cd7de03
	.word	0xa2bba8be
	.word	0xa554a158
	.word	0xb3b5d042
	.word	0xca3ca3d2
	.word	0xd1f59fdb
	.word	0xd5c28cc0
	.word	0xd6a5f2f7
	.word	0xe23e3a35
	.word	0xe88f759b
	.word	0xeca32a99
	.word	0xfaa4f89b
	.word	sceAtracLowLevelDecode /* sceAtracLowLevelDecode (0x0c116e1b) */
	.word	sceAtracSetData /* sceAtracSetData (0x0e2a73ab) */
	.word	sceAtracSetHalfwayBufferAndGetID /* sceAtracSetHalfwayBufferAndGetID (0x0fae370e) */
	.word	sceAtracReinit /* sceAtracReinit (0x132f1eca) */
	.word	sceAtracLowLevelInitDecoder /* sceAtracLowLevelInitDecoder (0x1575d64b) */
	.word	_sceAtracGetContextAddress /* _sceAtracGetContextAddress (0x231fc6b7) */
	.word	sceAtracGetBufferInfoForReseting /* sceAtracGetBufferInfoForResetting (0x2dd3e298) */
	.word	sceAtracGetChannel /* sceAtracGetChannel (0x31668baa) */
	.word	sceAtracGetNextSample /* sceAtracGetNextSample (0x36faabfb) */
	.word	sceAtracSetHalfwayBuffer /* sceAtracSetHalfwayBuffer (0x3f6e26b5) */
	.word	sceAtracSetAA3DataAndGetID /* sceAtracSetAA3DataAndGetID (0x5622b7c1) */
	.word	sceAtracSetMOutHalfwayBuffer /* sceAtracSetMOutHalfwayBuffer (0x5cf9d852) */
	.word	sceAtracGetStreamDataInfo /* sceAtracGetStreamDataInfo (0x5d268707) */
	.word	sceAtracSetAA3HalfwayBufferAndGetID /* sceAtracSetAA3HalfwayBufferAndGetID (0x5dd66588) */
	.word	sceAtracReleaseAtracID /* sceAtracReleaseAtracID (0x61eb33f5) */
	.word	sceAtracResetPlayPosition /* sceAtracResetPlayPosition (0x644e5607) */
	.word	sceAtracDecodeData /* sceAtracDecodeData (0x6a8c3cd5) */
	.word	sceAtracGetAtracID /* sceAtracGetAtracID (0x780f88d1) */
	.word	sceAtracSetDataAndGetID /* sceAtracSetDataAndGetID (0x7a20e7af) */
	.word	sceAtracAddStreamData /* sceAtracAddStreamData (0x7db31251) */
	.word	sceAtracSetSecondBuffer /* sceAtracSetSecondBuffer (0x83bf7afd) */
	.word	sceAtracGetSecondBufferInfo /* sceAtracGetSecondBufferInfo (0x83e85ea0) */
	.word	sceAtracSetLoopNum /* sceAtracSetLoopNum (0x868120b5) */
	.word	sceAtracGetRemainFrame /* sceAtracGetRemainFrame (0x9ae849a7) */
	.word	sceAtracSetMOutHalfwayBufferAndGetID /* sceAtracSetMOutHalfwayBufferAndGetID (0x9cd7de03) */
	.word	sceAtracGetSoundSample /* sceAtracGetSoundSample (0xa2bba8be) */
	.word	sceAtracGetBitrate /* sceAtracGetBitrate (0xa554a158) */
	.word	sceAtracGetOutputChannel /* sceAtracGetOutputChannel (0xb3b5d042) */
	.word	sceAtracGetBufferInfoForReseting /* sceAtracGetBufferInfoForReseting (0xca3ca3d2) */
	.word	sceAtracStartEntry /* sceAtracStartEntry (0xd1f59fdb) */
	.word	sceAtracEndEntry /* sceAtracEndEntry (0xd5c28cc0) */
	.word	sceAtracGetMaxSample /* sceAtracGetMaxSample (0xd6a5f2f7) */
	.word	sceAtracGetNextDecodePosition /* sceAtracGetNextDecodePosition (0xe23e3a35) */
	.word	sceAtracGetInternalErrorInfo /* sceAtracGetInternalErrorInfo (0xe88f759b) */
	.word	sceAtracIsSecondBufferNeeded /* sceAtracIsSecondBufferNeeded (0xeca32a99) */
	.word	sceAtracGetLoopStatus /* sceAtracGetLoopStatus (0xfaa4f89b) */
	.word	0x00030000
	.word	0x00070005
	.word	0x000a000a
	.word	0x0011000e
	.word	0x00170014
	.word	0x001b0019
	.word	0x001d001c
	.word	0x00230020

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x092968f4
	.word	0x1839852a
	.word	0x5f10d406
.L_imp_nid_1:
	.word	0x29681260
	.word	0x3a20a200
	.word	0x3dd7ee1a
	.word	0x5b37eb1d
	.word	0x70a703f8
	.word	0x9d3f790c

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
