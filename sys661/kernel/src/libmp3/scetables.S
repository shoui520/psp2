/* Generated PSP module import/export tables for sceMp3_Library */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelCpuSuspendIntr
	.type	sceKernelCpuSuspendIntr, @function
sceKernelCpuSuspendIntr: /* Kernel_Library 0x092968f4 @ 0x28a0 */
	jr	$ra
	nop
	.globl	sceKernelMemset
	.type	sceKernelMemset, @function
sceKernelMemset: /* Kernel_Library 0xa089eca4 @ 0x28a8 */
	jr	$ra
	nop
	.globl	sceKernelMemcpy
	.type	sceKernelMemcpy, @function
sceKernelMemcpy: /* Kernel_Library 0x1839852a @ 0x28b0 */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntr
	.type	sceKernelCpuResumeIntr, @function
sceKernelCpuResumeIntr: /* Kernel_Library 0x5f10d406 @ 0x28b8 */
	jr	$ra
	nop
	.globl	sceKernelGetCompiledSdkVersion
	.type	sceKernelGetCompiledSdkVersion, @function
sceKernelGetCompiledSdkVersion: /* SysMemUserForUser 0xfc114573 @ 0x28c0 */
	jr	$ra
	nop
	.globl	sceKernelCreateFpl
	.type	sceKernelCreateFpl, @function
sceKernelCreateFpl: /* ThreadManForUser 0xc07bb470 @ 0x28c8 */
	jr	$ra
	nop
	.globl	sceKernelAllocateFpl
	.type	sceKernelAllocateFpl, @function
sceKernelAllocateFpl: /* ThreadManForUser 0xd979e9bf @ 0x28d0 */
	jr	$ra
	nop
	.globl	sceKernelDeleteFpl
	.type	sceKernelDeleteFpl, @function
sceKernelDeleteFpl: /* ThreadManForUser 0xed1410e0 @ 0x28d8 */
	jr	$ra
	nop
	.globl	sceKernelFreeFpl
	.type	sceKernelFreeFpl, @function
sceKernelFreeFpl: /* ThreadManForUser 0xf6414a71 @ 0x28e0 */
	jr	$ra
	nop
	.globl	sceAudiocodecReleaseEDRAM
	.type	sceAudiocodecReleaseEDRAM, @function
sceAudiocodecReleaseEDRAM: /* sceAudiocodec 0x29681260 @ 0x28e8 */
	jr	$ra
	nop
	.globl	sceAudiocodecGetEDRAM
	.type	sceAudiocodecGetEDRAM, @function
sceAudiocodecGetEDRAM: /* sceAudiocodec 0x3a20a200 @ 0x28f0 */
	jr	$ra
	nop
	.globl	sceAudiocodecAlcExtendParameter
	.type	sceAudiocodecAlcExtendParameter, @function
sceAudiocodecAlcExtendParameter: /* sceAudiocodec 0x59176a0f @ 0x28f8 */
	jr	$ra
	nop
	.globl	sceAudiocodecInit
	.type	sceAudiocodecInit, @function
sceAudiocodecInit: /* sceAudiocodec 0x5b37eb1d @ 0x2900 */
	jr	$ra
	nop
	.globl	sceAudiocodecDecode
	.type	sceAudiocodecDecode, @function
sceAudiocodecDecode: /* sceAudiocodec 0x70a703f8 @ 0x2908 */
	jr	$ra
	nop
	.globl	sceAudiocodecGetInfo
	.type	sceAudiocodecGetInfo, @function
sceAudiocodecGetInfo: /* sceAudiocodec 0x8aca11d5 @ 0x2910 */
	jr	$ra
	nop
	.globl	sceAudiocodecCheckNeedMem
	.type	sceAudiocodecCheckNeedMem, @function
sceAudiocodecCheckNeedMem: /* sceAudiocodec 0x9d3f790c @ 0x2918 */
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
	.half	24
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
	.half	7
	.word	.L_imp_nid_2
	.word	sceAudiocodecReleaseEDRAM
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x4000
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_3
	.word	sceKernelGetCompiledSdkVersion

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
	.byte	6, 1
	.asciz	"sceMp3_Library"
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
	.space	6
.L_imp_lib_3:
	.asciz	"SysMemUserForUser"
	.space	2
.L_exp_tbl_0:
	.word	0xd632acdb
	.word	0xcee8593c
	.word	0xf01d73a7
	.word	0x11b97506
	.word	0x0f7c276c
	.word	0xcf0cc697
	.word	module_start /* module_start (0xd632acdb) */
	.word	module_stop /* module_stop (0xcee8593c) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
	.word	module_start_thread_parameter /* module_start_thread_parameter (0x0f7c276c) */
	.word	module_stop_thread_parameter /* module_stop_thread_parameter (0xcf0cc697) */
.L_exp_lib_1:
	.asciz	"sceMp3"
	.space	1
.L_exp_tbl_1:
	.word	0x07ec321a
	.word	0x0840e808
	.word	0x0db149f4
	.word	0x1b839b83
	.word	0x2a368661
	.word	0x3548aec8
	.word	0x354d27ea
	.word	0x35750070
	.word	0x3c2fa058
	.word	0x3cef484f
	.word	0x44e07129
	.word	0x732b042a
	.word	0x7f696782
	.word	0x87677e40
	.word	0x87c263d1
	.word	0x8ab81558
	.word	0x8f450998
	.word	0xa703fe0f
	.word	0xae6d2027
	.word	0xd021c0fb
	.word	0xd0a56296
	.word	0xd8f54a51
	.word	0xe3ee2c81
	.word	0xf5478233
	.word	sceMp3ReserveMp3Handle /* sceMp3ReserveMp3Handle (0x07ec321a) */
	.word	sceMp3ResetPlayPositionByFrame /* sceMp3ResetPlayPositionByFrame (0x0840e808) */
	.word	sceMp3NotifyAddStreamData /* sceMp3NotifyAddStreamData (0x0db149f4) */
	.word	sceMp3LowLevelInit /* sceMp3LowLevelInit (0x1b839b83) */
	.word	sceMp3ResetPlayPosition /* sceMp3ResetPlayPosition (0x2a368661) */
	.word	sceMp3GetFrameNum /* sceMp3GetFrameNum (0x3548aec8) */
	.word	sceMp3GetSumDecodedSample /* sceMp3GetSumDecodedSample (0x354d27ea) */
	.word	sceMp3InitResource /* sceMp3InitResource (0x35750070) */
	.word	sceMp3TermResource /* sceMp3TermResource (0x3c2fa058) */
	.word	sceMp3SetLoopNum /* sceMp3SetLoopNum (0x3cef484f) */
	.word	sceMp3Init /* sceMp3Init (0x44e07129) */
	.word	module_stop /* sceMp3_732B042A (0x732b042a) */
	.word	sceMp3GetMp3ChannelNum /* sceMp3GetMp3ChannelNum (0x7f696782) */
	.word	sceMp3GetBitRate /* sceMp3GetBitRate (0x87677e40) */
	.word	sceMp3GetMaxOutputSample /* sceMp3GetMaxOutputSample (0x87c263d1) */
	.word	module_start /* sceMp3_8AB81558 (0x8ab81558) */
	.word	sceMp3GetSamplingRate /* sceMp3GetSamplingRate (0x8f450998) */
	.word	sceMp3GetInfoToAddStreamData /* sceMp3GetInfoToAddStreamData (0xa703fe0f) */
	.word	sceMp3GetMPEGVersion /* sceMp3GetMPEGVersion (0xae6d2027) */
	.word	sceMp3Decode /* sceMp3Decode (0xd021c0fb) */
	.word	sceMp3CheckStreamDataNeeded /* sceMp3CheckStreamDataNeeded (0xd0a56296) */
	.word	sceMp3GetLoopNum /* sceMp3GetLoopNum (0xd8f54a51) */
	.word	sceMp3LowLevelDecode /* sceMp3LowLevelDecode (0xe3ee2c81) */
	.word	sceMp3ReleaseMp3Handle /* sceMp3ReleaseMp3Handle (0xf5478233) */
	.word	0x00030000
	.word	0x00050004
	.word	0x000b000a
	.word	0x000b000b
	.word	0x0011000d
	.word	0x00130011
	.word	0x00130013
	.word	0x00170016

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
	.word	0x29681260
	.word	0x3a20a200
	.word	0x59176a0f
	.word	0x5b37eb1d
	.word	0x70a703f8
	.word	0x8aca11d5
	.word	0x9d3f790c
.L_imp_nid_3:
	.word	0xfc114573

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
