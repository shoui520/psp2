/* Generated PSP module import/export tables for sceAudio_Driver */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelDmaOpQuit
	.type	sceKernelDmaOpQuit, @function
sceKernelDmaOpQuit: /* DmacManForKernel 0x5af32783 @ 0x2bdc */
	jr	$ra
	nop
	.globl	sceKernelDmaOpFree
	.type	sceKernelDmaOpFree, @function
sceKernelDmaOpFree: /* DmacManForKernel 0x745e19ef @ 0x2be4 */
	jr	$ra
	nop
	.globl	sceKernelCpuSuspendIntr
	.type	sceKernelCpuSuspendIntr, @function
sceKernelCpuSuspendIntr: /* InterruptManagerForKernel 0x092968f4 @ 0x2bec */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntr
	.type	sceKernelCpuResumeIntr, @function
sceKernelCpuResumeIntr: /* InterruptManagerForKernel 0x5f10d406 @ 0x2bf4 */
	jr	$ra
	nop
	.globl	sceKernelReleaseIntrHandler
	.type	sceKernelReleaseIntrHandler, @function
sceKernelReleaseIntrHandler: /* InterruptManagerForKernel 0xf987b1f0 @ 0x2bfc */
	jr	$ra
	nop
	.globl	Kprintf
	.type	Kprintf, @function
Kprintf: /* KDebugForKernel 0x84f370bc @ 0x2c04 */
	jr	$ra
	nop
	.globl	sceKernelMemset
	.type	sceKernelMemset, @function
sceKernelMemset: /* SysMemForKernel 0x8ae776af @ 0x2c0c */
	jr	$ra
	nop
	.globl	sceKernelGetCompiledSdkVersion
	.type	sceKernelGetCompiledSdkVersion, @function
sceKernelGetCompiledSdkVersion: /* SysMemForKernel 0xb4f00cb5 @ 0x2c14 */
	jr	$ra
	nop
	.globl	memset
	.type	memset, @function
memset: /* SysclibForKernel 0x10f3bb61 @ 0x2c1c */
	jr	$ra
	nop
	.globl	__udivdi3
	.type	__udivdi3, @function
__udivdi3: /* SysclibForKernel 0x7dee14de @ 0x2c24 */
	jr	$ra
	nop
	.globl	sceKernelSetEventFlag
	.type	sceKernelSetEventFlag, @function
sceKernelSetEventFlag: /* ThreadManForKernel 0x1fb15a32 @ 0x2c2c */
	jr	$ra
	nop
	.globl	sceKernelGetThreadId
	.type	sceKernelGetThreadId, @function
sceKernelGetThreadId: /* ThreadManForKernel 0x293b45b8 @ 0x2c34 */
	jr	$ra
	nop
	.globl	sceKernelGetSystemTimeLow
	.type	sceKernelGetSystemTimeLow, @function
sceKernelGetSystemTimeLow: /* ThreadManForKernel 0x369ed59d @ 0x2c3c */
	jr	$ra
	nop
	.globl	sceKernelWaitEventFlag
	.type	sceKernelWaitEventFlag, @function
sceKernelWaitEventFlag: /* ThreadManForKernel 0x402fcf22 @ 0x2c44 */
	jr	$ra
	nop
	.globl	sceKernelCreateThread
	.type	sceKernelCreateThread, @function
sceKernelCreateThread: /* ThreadManForKernel 0x446d8de6 @ 0x2c4c */
	jr	$ra
	nop
	.globl	sceKernelCreateEventFlag
	.type	sceKernelCreateEventFlag, @function
sceKernelCreateEventFlag: /* ThreadManForKernel 0x55c20a00 @ 0x2c54 */
	jr	$ra
	nop
	.globl	sceKernelGetSystemTimeWide
	.type	sceKernelGetSystemTimeWide, @function
sceKernelGetSystemTimeWide: /* ThreadManForKernel 0x82bc5777 @ 0x2c5c */
	jr	$ra
	nop
	.globl	sceKernelExitThread
	.type	sceKernelExitThread, @function
sceKernelExitThread: /* ThreadManForKernel 0xaa73c935 @ 0x2c64 */
	jr	$ra
	nop
	.globl	sceKernelCreateMutex
	.type	sceKernelCreateMutex, @function
sceKernelCreateMutex: /* ThreadManForKernel 0xb7d098c6 @ 0x2c6c */
	jr	$ra
	nop
	.globl	sceKernelDelayThread
	.type	sceKernelDelayThread, @function
sceKernelDelayThread: /* ThreadManForKernel 0xceadeb47 @ 0x2c74 */
	jr	$ra
	nop
	.globl	sceKernelDeleteEventFlag
	.type	sceKernelDeleteEventFlag, @function
sceKernelDeleteEventFlag: /* ThreadManForKernel 0xef9e4c70 @ 0x2c7c */
	jr	$ra
	nop
	.globl	sceKernelStartThread
	.type	sceKernelStartThread, @function
sceKernelStartThread: /* ThreadManForKernel 0xf475845d @ 0x2c84 */
	jr	$ra
	nop
	.globl	sceKernelDcacheWritebackInvalidateRange
	.type	sceKernelDcacheWritebackInvalidateRange, @function
sceKernelDcacheWritebackInvalidateRange: /* UtilsForKernel 0x34b9fa9e @ 0x2c8c */
	jr	$ra
	nop
	.globl	sceKernelDcacheWritebackRange
	.type	sceKernelDcacheWritebackRange, @function
sceKernelDcacheWritebackRange: /* UtilsForKernel 0x3ee30821 @ 0x2c94 */
	jr	$ra
	nop
	.globl	sceKernelDcacheInvalidateRange
	.type	sceKernelDcacheInvalidateRange, @function
sceKernelDcacheInvalidateRange: /* UtilsForKernel 0xbfa98062 @ 0x2c9c */
	jr	$ra
	nop
	.globl	sceDdrFlush
	.type	sceDdrFlush, @function
sceDdrFlush: /* sceDdr_driver 0x87d86769 @ 0x2ca4 */
	jr	$ra
	nop
	.globl	sceKermitMemory_driver_AAF047AC
	.type	sceKermitMemory_driver_AAF047AC, @function
sceKermitMemory_driver_AAF047AC: /* sceKermitMemory_driver 0xaaf047ac @ 0x2cac */
	jr	$ra
	nop
	.globl	sceKermit_driver_36666181
	.type	sceKermit_driver_36666181, @function
sceKermit_driver_36666181: /* sceKermit_driver 0x36666181 @ 0x2cb4 */
	jr	$ra
	nop
	.globl	sceKermit_driver_5280B410
	.type	sceKermit_driver_5280B410, @function
sceKermit_driver_5280B410: /* sceKermit_driver 0x5280b410 @ 0x2cbc */
	jr	$ra
	nop
	.globl	sceKernelRegisterSysEventHandler
	.type	sceKernelRegisterSysEventHandler, @function
sceKernelRegisterSysEventHandler: /* sceSysEventForKernel 0xcd9e4bb5 @ 0x2cc4 */
	jr	$ra
	nop
	.globl	sceKernelUnregisterSysEventHandler
	.type	sceKernelUnregisterSysEventHandler, @function
sceKernelUnregisterSysEventHandler: /* sceSysEventForKernel 0xd7d3fdcd @ 0x2ccc */
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
	.half	0x0001
	.byte	5, 0
	.half	31
	.word	.L_exp_tbl_1
	.word	0x00020000
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x4001
	.byte	5, 0
	.half	27
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
	.half	12
	.word	.L_imp_nid_0
	.word	sceKernelSetEventFlag
	.word	0
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_1
	.word	sceKernelDmaOpQuit
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	3
	.word	.L_imp_nid_2
	.word	sceKernelDcacheWritebackInvalidateRange
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_3
	.word	memset
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0009
	.byte	6, 0
	.half	3
	.word	.L_imp_nid_4
	.word	sceKernelCpuSuspendIntr
	.word	0
	.word	.L_imp_lib_5
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_5
	.word	Kprintf
	.word	.L_imp_lib_6
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_6
	.word	sceKernelRegisterSysEventHandler
	.word	.L_imp_lib_7
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_7
	.word	sceKernelMemset
	.word	.L_imp_lib_8
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_8
	.word	sceDdrFlush
	.word	.L_imp_lib_9
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_9
	.word	sceKermit_driver_36666181
	.word	.L_imp_lib_10
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_10
	.word	sceKermitMemory_driver_AAF047AC

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
	.byte	13, 1
	.asciz	"sceAudio_Driver"
	.space	12
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
	.asciz	"DmacManForKernel"
	.space	7
.L_imp_lib_2:
	.asciz	"UtilsForKernel"
	.space	5
.L_imp_lib_3:
	.asciz	"SysclibForKernel"
	.space	7
.L_imp_lib_4:
	.asciz	"InterruptManagerForKernel"
	.space	6
.L_imp_lib_5:
	.asciz	"KDebugForKernel"
	.space	4
.L_imp_lib_6:
	.asciz	"sceSysEventForKernel"
	.space	7
.L_imp_lib_7:
	.asciz	"SysMemForKernel"
	.space	4
.L_imp_lib_8:
	.asciz	"sceDdr_driver"
	.space	6
.L_imp_lib_9:
	.asciz	"sceKermit_driver"
	.space	7
.L_imp_lib_10:
	.asciz	"sceKermitMemory_driver"
	.space	1
.L_exp_tbl_0:
	.word	0xd632acdb
	.word	0xcee8593c
	.word	0x2f064fa6
	.word	0xf01d73a7
	.word	0x11b97506
	.word	sceAudioInit /* module_start (0xd632acdb) */
	.word	sceAudioEnd /* module_stop (0xcee8593c) */
	.word	sceAudioEnd /* module_reboot_before (0x2f064fa6) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"sceAudio_driver"
	.align	2
.L_exp_tbl_1:
	.word	0x093c7a46
	.word	0x10900742
	.word	0x17d856b9
	.word	0x22e4fb37
	.word	0x42cc2d83
	.word	0x4a0fe97d
	.word	0x4a7f9e9d
	.word	0x4afa04d9
	.word	0x4f959094
	.word	0x5182b550
	.word	0x53a4fe20
	.word	0x5cdef9a4
	.word	0x60b750b2
	.word	0x63b5a460
	.word	0x671e97e8
	.word	0x76bba42e
	.word	0x837701cc
	.word	0x8a7cd9c6
	.word	0x95ae5a2b
	.word	0x9d77949e
	.word	0xac81de4f
	.word	0xb7ccf1d7
	.word	0xb7f5a1b2
	.word	0xcf62029e
	.word	0xd2f5a41a
	.word	0xda16ff61
	.word	0xe440df7d
	.word	0xf86dfdd6
	.word	0xfa05ef51
	.word	0xfad7b114
	.word	0xff298ce7
	.word	sceAudioSetChannelDataLen /* sceAudioSetChannelDataLen (0x093c7a46) */
	.word	sceAudioInputInit /* sceAudioInputInit (0x10900742) */
	.word	sceAudioOutputPanned /* sceAudioOutputPanned (0x17d856b9) */
	.word	sceAudioInit /* sceAudioInit (0x22e4fb37) */
	.word	sceAudioInputBlocking /* sceAudioInputBlocking (0x42cc2d83) */
	.word	sceAudioSetFrequency /* sceAudioSetFrequency (0x4a0fe97d) */
	.word	sceAudioEnd /* sceAudioEnd (0x4a7f9e9d) */
	.word	sceAudioInputInitEx /* sceAudioInputInitEx (0x4afa04d9) */
	.word	sceAudioChReserve /* sceAudioChReserve (0x4f959094) */
	.word	sceAudio_driver_5182B550 /* sceAudio_driver_5182B550 (0x5182b550) */
	.word	sceAudioLoopbackTest /* sceAudioLoopbackTest (0x53a4fe20) */
	.word	sceAudioOutputBlocking /* sceAudioOutputBlocking (0x5cdef9a4) */
	.word	sceAudioPollInputEnd /* sceAudioPollInputEnd (0x60b750b2) */
	.word	sceAudioChangeChannelConfig /* sceAudioChangeChannelConfig (0x63b5a460) */
	.word	sceAudioOutput /* sceAudioOutput (0x671e97e8) */
	.word	sceAudioInput /* sceAudioInput (0x76bba42e) */
	.word	sceAudioSRCChReserve /* sceAudioSRCChReserve (0x837701cc) */
	.word	sceAudioOutput2GetRestSample /* sceAudioOutput2GetRestSample (0x8a7cd9c6) */
	.word	sceAudio_driver_95AE5A2B /* sceAudio_driver_95AE5A2B (0x95ae5a2b) */
	.word	sceAudioGetChannelRestLength /* sceAudioGetChannelRestLength (0x9d77949e) */
	.word	sceAudioSRCOutputBlocking /* sceAudioSRCOutputBlocking (0xac81de4f) */
	.word	sceAudioOutputPannedBlocking /* sceAudioOutputPannedBlocking (0xb7ccf1d7) */
	.word	sceAudioSRCChRelease /* sceAudioSRCChRelease (0xb7f5a1b2) */
	.word	sceAudioGetChannelRestLen /* sceAudioGetChannelRestLen (0xcf62029e) */
	.word	sceAudioWaitInputEnd /* sceAudioWaitInputEnd (0xd2f5a41a) */
	.word	sceAudioChRelease /* sceAudioChRelease (0xda16ff61) */
	.word	sceAudioOneshotOutput /* sceAudioOneshotOutput (0xe440df7d) */
	.word	sceAudioSetVolumeOffset /* sceAudioSetVolumeOffset (0xf86dfdd6) */
	.word	sceAudioGetInputLength /* sceAudioGetInputLength (0xfa05ef51) */
	.word	sceAudioChangeChannelVolume /* sceAudioChangeChannelVolume (0xfad7b114) */
	.word	sceAudio_driver_FF298CE7 /* sceAudio_driver_FF298CE7 (0xff298ce7) */
	.word	0x00010000
	.word	0x00040003
	.word	0x00090004
	.word	0x000f000c
	.word	0x00120010
	.word	0x00150014
	.word	0x00180017
	.word	0x001b001a
.L_exp_lib_2:
	.asciz	"sceAudio"
	.space	3
.L_exp_tbl_2:
	.word	0x01562ba3
	.word	0x086e5895
	.word	0x136caf51
	.word	0x13f592bc
	.word	0x2d53f36e
	.word	0x38553111
	.word	0x41efade7
	.word	0x43196845
	.word	0x5c37c0ae
	.word	0x5ec81c55
	.word	0x63f2889c
	.word	0x647cef33
	.word	0x6d4bec68
	.word	0x6fc46853
	.word	0x7de61688
	.word	0x87b2e651
	.word	0x8c1009b2
	.word	0x95fd0c2d
	.word	0xa633048e
	.word	0xa708c6a6
	.word	0xb011922f
	.word	0xb7e1d8e7
	.word	0xcb2e439e
	.word	0xe0727056
	.word	0xe2d56b2d
	.word	0xe926d3fb
	.word	0xe9d97901
	.word	sceAudioOutput2Reserve /* sceAudioOutput2Reserve (0x01562ba3) */
	.word	sceAudioInputBlocking /* sceAudioInputBlocking (0x086e5895) */
	.word	sceAudioOutputBlocking /* sceAudioOutputBlocking (0x136caf51) */
	.word	sceAudioOutputPannedBlocking /* sceAudioOutputPannedBlocking (0x13f592bc) */
	.word	sceAudioOutput2OutputBlocking /* sceAudioOutput2OutputBlocking (0x2d53f36e) */
	.word	sceAudioSRCChReserve /* sceAudioSRCChReserve (0x38553111) */
	.word	sceAudioOneshotOutput /* sceAudioOneshotOutput (0x41efade7) */
	.word	sceAudioOutput2Release /* sceAudioOutput2Release (0x43196845) */
	.word	sceAudioSRCChRelease /* sceAudioSRCChRelease (0x5c37c0ae) */
	.word	sceAudioChReserve /* sceAudioChReserve (0x5ec81c55) */
	.word	sceAudioOutput2ChangeLength /* sceAudioOutput2ChangeLength (0x63f2889c) */
	.word	sceAudioOutput2GetRestSample /* sceAudioOutput2GetRestSample (0x647cef33) */
	.word	sceAudioInput /* sceAudioInput (0x6d4bec68) */
	.word	sceAudioChRelease /* sceAudioChRelease (0x6fc46853) */
	.word	sceAudioInputInit /* sceAudioInputInit (0x7de61688) */
	.word	sceAudioWaitInputEnd /* sceAudioWaitInputEnd (0x87b2e651) */
	.word	sceAudioOutput /* sceAudioOutput (0x8c1009b2) */
	.word	sceAudioChangeChannelConfig /* sceAudioChangeChannelConfig (0x95fd0c2d) */
	.word	sceAudioPollInputEnd /* sceAudioPollInputEnd (0xa633048e) */
	.word	sceAudioGetInputLength /* sceAudioGetInputLength (0xa708c6a6) */
	.word	sceAudioGetChannelRestLength /* sceAudioGetChannelRestLength (0xb011922f) */
	.word	sceAudioChangeChannelVolume /* sceAudioChangeChannelVolume (0xb7e1d8e7) */
	.word	sceAudioSetChannelDataLen /* sceAudioSetChannelDataLen (0xcb2e439e) */
	.word	sceAudioSRCOutputBlocking /* sceAudioSRCOutputBlocking (0xe0727056) */
	.word	sceAudioOutputPanned /* sceAudioOutputPanned (0xe2d56b2d) */
	.word	sceAudioInputInitEx /* sceAudioInputInitEx (0xe926d3fb) */
	.word	sceAudioGetChannelRestLen /* sceAudioGetChannelRestLen (0xe9d97901) */
	.word	0x00020000
	.word	0x00050004
	.word	0x00080006
	.word	0x000e000a
	.word	0x0011000f
	.word	0x00140012
	.word	0x00170016
	.word	0x001b0017

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x1fb15a32
	.word	0x293b45b8
	.word	0x369ed59d
	.word	0x402fcf22
	.word	0x446d8de6
	.word	0x55c20a00
	.word	0x82bc5777
	.word	0xaa73c935
	.word	0xb7d098c6
	.word	0xceadeb47
	.word	0xef9e4c70
	.word	0xf475845d
.L_imp_nid_1:
	.word	0x5af32783
	.word	0x745e19ef
.L_imp_nid_2:
	.word	0x34b9fa9e
	.word	0x3ee30821
	.word	0xbfa98062
.L_imp_nid_3:
	.word	0x10f3bb61
	.word	0x7dee14de
.L_imp_nid_4:
	.word	0x092968f4
	.word	0x5f10d406
	.word	0xf987b1f0
.L_imp_nid_5:
	.word	0x84f370bc
.L_imp_nid_6:
	.word	0xcd9e4bb5
	.word	0xd7d3fdcd
.L_imp_nid_7:
	.word	0x8ae776af
	.word	0xb4f00cb5
.L_imp_nid_8:
	.word	0x87d86769
.L_imp_nid_9:
	.word	0x36666181
	.word	0x5280b410
.L_imp_nid_10:
	.word	0xaaf047ac

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
