/* Generated PSP module import/export tables for sceVaudio_driver */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelApplicationType
	.type	sceKernelApplicationType, @function
sceKernelApplicationType: /* InitForKernel 0x7233b5bc @ 0x1060 */
	jr	$ra
	nop
	.globl	sceKernelMemcpy
	.type	sceKernelMemcpy, @function
sceKernelMemcpy: /* SysMemForKernel 0x181065ab @ 0x1068 */
	jr	$ra
	nop
	.globl	sceKernelMemset32
	.type	sceKernelMemset32, @function
sceKernelMemset32: /* SysMemForKernel 0x22a114dc @ 0x1070 */
	jr	$ra
	nop
	.globl	sceKernelDeleteSema
	.type	sceKernelDeleteSema, @function
sceKernelDeleteSema: /* ThreadManForKernel 0x28b6489c @ 0x1078 */
	jr	$ra
	nop
	.globl	sceKernelSignalSema
	.type	sceKernelSignalSema, @function
sceKernelSignalSema: /* ThreadManForKernel 0x3f53e640 @ 0x1080 */
	jr	$ra
	nop
	.globl	sceKernelWaitSema
	.type	sceKernelWaitSema, @function
sceKernelWaitSema: /* ThreadManForKernel 0x4e3a1105 @ 0x1088 */
	jr	$ra
	nop
	.globl	sceKernelCreateFpl
	.type	sceKernelCreateFpl, @function
sceKernelCreateFpl: /* ThreadManForKernel 0xc07bb470 @ 0x1090 */
	jr	$ra
	nop
	.globl	sceKernelCreateSema
	.type	sceKernelCreateSema, @function
sceKernelCreateSema: /* ThreadManForKernel 0xd6da4ba1 @ 0x1098 */
	jr	$ra
	nop
	.globl	sceKernelAllocateFpl
	.type	sceKernelAllocateFpl, @function
sceKernelAllocateFpl: /* ThreadManForKernel 0xd979e9bf @ 0x10a0 */
	jr	$ra
	nop
	.globl	sceKernelDeleteFpl
	.type	sceKernelDeleteFpl, @function
sceKernelDeleteFpl: /* ThreadManForKernel 0xed1410e0 @ 0x10a8 */
	jr	$ra
	nop
	.globl	sceKernelDcacheWritebackRange
	.type	sceKernelDcacheWritebackRange, @function
sceKernelDcacheWritebackRange: /* UtilsForKernel 0x3ee30821 @ 0x10b0 */
	jr	$ra
	nop
	.globl	sceKernelDcacheInvalidateRangeForUser
	.type	sceKernelDcacheInvalidateRangeForUser, @function
sceKernelDcacheInvalidateRangeForUser: /* UtilsForKernel 0xbfa98062 @ 0x10b8 */
	jr	$ra
	nop
	.globl	sceAudioSetFrequency
	.type	sceAudioSetFrequency, @function
sceAudioSetFrequency: /* sceAudio_driver 0x4a0fe97d @ 0x10c0 */
	jr	$ra
	nop
	.globl	sceAudioSRCChReserve
	.type	sceAudioSRCChReserve, @function
sceAudioSRCChReserve: /* sceAudio_driver 0x837701cc @ 0x10c8 */
	jr	$ra
	nop
	.globl	sceAudioOutput2GetRestSample
	.type	sceAudioOutput2GetRestSample, @function
sceAudioOutput2GetRestSample: /* sceAudio_driver 0x8a7cd9c6 @ 0x10d0 */
	jr	$ra
	nop
	.globl	sceAudioSRCOutputBlocking
	.type	sceAudioSRCOutputBlocking, @function
sceAudioSRCOutputBlocking: /* sceAudio_driver 0xac81de4f @ 0x10d8 */
	jr	$ra
	nop
	.globl	sceAudioSRCChRelease
	.type	sceAudioSRCChRelease, @function
sceAudioSRCChRelease: /* sceAudio_driver 0xb7f5a1b2 @ 0x10e0 */
	jr	$ra
	nop
	.globl	sceAudioSetVolumeOffset
	.type	sceAudioSetVolumeOffset, @function
sceAudioSetVolumeOffset: /* sceAudio_driver 0xf86dfdd6 @ 0x10e8 */
	jr	$ra
	nop
	.globl	sceMeCore_driver_635397BB
	.type	sceMeCore_driver_635397BB, @function
sceMeCore_driver_635397BB: /* sceMeCore_driver 0x635397bb @ 0x10f0 */
	jr	$ra
	nop
	.globl	sceMeCore_driver_FA398D71
	.type	sceMeCore_driver_FA398D71, @function
sceMeCore_driver_FA398D71: /* sceMeCore_driver 0xfa398d71 @ 0x10f8 */
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
	.half	8
	.word	.L_exp_tbl_1
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x4001
	.byte	4, 0
	.half	8
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
	.half	2
	.word	.L_imp_nid_0
	.word	sceMeCore_driver_635397BB
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_1
	.word	sceKernelDcacheWritebackRange
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_2
	.word	sceKernelApplicationType
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_3
	.word	sceKernelMemcpy
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	7
	.word	.L_imp_nid_4
	.word	sceKernelDeleteSema
	.word	0
	.word	.L_imp_lib_5
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	6
	.word	.L_imp_nid_5
	.word	sceAudioSetFrequency

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
	.byte	8, 1
	.asciz	"sceVaudio_driver"
	.space	11
	.word	_gp
	.word	__lib_ent_top + 4
	.word	__lib_ent_btm
	.word	__lib_stub_top + 4
	.word	__lib_stub_btm

	.section .rodata.sceResident, "a", @progbits
	.align	2
	.word	0
.L_imp_lib_0:
	.asciz	"sceMeCore_driver"
	.space	7
.L_imp_lib_1:
	.asciz	"UtilsForKernel"
	.space	5
.L_imp_lib_2:
	.asciz	"InitForKernel"
	.space	6
.L_imp_lib_3:
	.asciz	"SysMemForKernel"
	.space	4
.L_imp_lib_4:
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_5:
	.asciz	"sceAudio_driver"
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
	.asciz	"sceVaudio_driver"
	.space	3
.L_exp_tbl_1:
	.word	0x03b6807d
	.word	0x27acc20b
	.word	0x346fbe94
	.word	0x67585dfd
	.word	0x8986295e
	.word	0xa3b71098
	.word	0xcbd4ac51
	.word	0xdcc18f25
	.word	sceVaudioChReserve /* sceVaudioChReserve (0x03b6807d) */
	.word	sceVaudio_driver_27ACC20B /* sceVaudio_driver_27ACC20B (0x27acc20b) */
	.word	sceVaudioSetEffectType /* sceVaudioSetEffectType (0x346fbe94) */
	.word	sceVaudioChRelease /* sceVaudioChRelease (0x67585dfd) */
	.word	sceVaudioOutputBlocking /* sceVaudioOutputBlocking (0x8986295e) */
	.word	sceVaudio_driver_A3B71098 /* sceVaudio_driver_A3B71098 (0xa3b71098) */
	.word	sceVaudioSetAlcMode /* sceVaudioSetAlcMode (0xcbd4ac51) */
	.word	sceVaudio_driver_DCC18F25 /* sceVaudio_driver_DCC18F25 (0xdcc18f25) */
.L_exp_lib_2:
	.asciz	"sceVaudio"
	.space	2
.L_exp_tbl_2:
	.word	0x03b6807d
	.word	0x27acc20b
	.word	0x346fbe94
	.word	0x504e4745
	.word	0x67585dfd
	.word	0x8986295e
	.word	0xcbd4ac51
	.word	0xe8e78dc8
	.word	sceVaudioChReserve /* sceVaudioChReserve (0x03b6807d) */
	.word	sceVaudio_driver_27ACC20B /* sceVaudio_27ACC20B (0x27acc20b) */
	.word	sceVaudioSetEffectType /* sceVaudioSetEffectType (0x346fbe94) */
	.word	sceVaudio_504E4745 /* sceVaudio_504E4745 (0x504e4745) */
	.word	sceVaudioChRelease /* sceVaudioChRelease (0x67585dfd) */
	.word	sceVaudioOutputBlocking /* sceVaudioOutputBlocking (0x8986295e) */
	.word	sceVaudioSetAlcMode /* sceVaudioSetAlcMode (0xcbd4ac51) */
	.word	sceVaudio_E8E78DC8 /* sceVaudio_E8E78DC8 (0xe8e78dc8) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x635397bb
	.word	0xfa398d71
.L_imp_nid_1:
	.word	0x3ee30821
	.word	0xbfa98062
.L_imp_nid_2:
	.word	0x7233b5bc
.L_imp_nid_3:
	.word	0x181065ab
	.word	0x22a114dc
.L_imp_nid_4:
	.word	0x28b6489c
	.word	0x3f53e640
	.word	0x4e3a1105
	.word	0xc07bb470
	.word	0xd6da4ba1
	.word	0xd979e9bf
	.word	0xed1410e0
.L_imp_nid_5:
	.word	0x4a0fe97d
	.word	0x837701cc
	.word	0x8a7cd9c6
	.word	0xac81de4f
	.word	0xb7f5a1b2
	.word	0xf86dfdd6

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
