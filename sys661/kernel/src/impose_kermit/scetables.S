/* Generated PSP module import/export tables for sceImpose_Driver */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelBootFrom
	.type	sceKernelBootFrom, @function
sceKernelBootFrom: /* InitForKernel 0x27932388 @ 0x81a8 */
	jr	$ra
	nop
	.globl	sceKernelApplicationType
	.type	sceKernelApplicationType, @function
sceKernelApplicationType: /* InitForKernel 0x7233b5bc @ 0x81b0 */
	jr	$ra
	nop
	.globl	sceKernelSetInitCallback
	.type	sceKernelSetInitCallback, @function
sceKernelSetInitCallback: /* InitForKernel 0x9f9ae99c @ 0x81b8 */
	jr	$ra
	nop
	.globl	sceKernelInitDiscImage
	.type	sceKernelInitDiscImage, @function
sceKernelInitDiscImage: /* InitForKernel 0xa18a4a8b @ 0x81c0 */
	jr	$ra
	nop
	.globl	sceKernelCpuSuspendIntr
	.type	sceKernelCpuSuspendIntr, @function
sceKernelCpuSuspendIntr: /* InterruptManagerForKernel 0x092968f4 @ 0x81c8 */
	jr	$ra
	nop
	.globl	sceKernelDisableSubIntr
	.type	sceKernelDisableSubIntr, @function
sceKernelDisableSubIntr: /* InterruptManagerForKernel 0x4023e1a7 @ 0x81d0 */
	jr	$ra
	nop
	.globl	sceKernelCpuResumeIntr
	.type	sceKernelCpuResumeIntr, @function
sceKernelCpuResumeIntr: /* InterruptManagerForKernel 0x5f10d406 @ 0x81d8 */
	jr	$ra
	nop
	.globl	sceKernelReleaseSubIntrHandler
	.type	sceKernelReleaseSubIntrHandler, @function
sceKernelReleaseSubIntrHandler: /* InterruptManagerForKernel 0xd61e6961 @ 0x81e0 */
	jr	$ra
	nop
	.globl	sceKernelEnableSubIntr
	.type	sceKernelEnableSubIntr, @function
sceKernelEnableSubIntr: /* InterruptManagerForKernel 0xfb8e22ec @ 0x81e8 */
	jr	$ra
	nop
	.globl	sceKernelRegisterSubIntrHandler
	.type	sceKernelRegisterSubIntrHandler, @function
sceKernelRegisterSubIntrHandler: /* InterruptManagerForKernel 0xffa8b183 @ 0x81f0 */
	jr	$ra
	nop
	.globl	sceIoOpen
	.type	sceIoOpen, @function
sceIoOpen: /* IoFileMgrForKernel 0x109f50bc @ 0x81f8 */
	jr	$ra
	nop
	.globl	sceIoLseek
	.type	sceIoLseek, @function
sceIoLseek: /* IoFileMgrForKernel 0x27eb27b8 @ 0x8200 */
	jr	$ra
	nop
	.globl	sceIoDevctl
	.type	sceIoDevctl, @function
sceIoDevctl: /* IoFileMgrForKernel 0x54f5fb11 @ 0x8208 */
	jr	$ra
	nop
	.globl	sceIoRead
	.type	sceIoRead, @function
sceIoRead: /* IoFileMgrForKernel 0x6a638d83 @ 0x8210 */
	jr	$ra
	nop
	.globl	sceIoClose
	.type	sceIoClose, @function
sceIoClose: /* IoFileMgrForKernel 0x810c4bc3 @ 0x8218 */
	jr	$ra
	nop
	.globl	sceKernelIsDVDMode
	.type	sceKernelIsDVDMode, @function
sceKernelIsDVDMode: /* KDebugForKernel 0xb41e2430 @ 0x8220 */
	jr	$ra
	nop
	.globl	sceKernelInvokeExitCallback
	.type	sceKernelInvokeExitCallback, @function
sceKernelInvokeExitCallback: /* LoadExecForKernel 0x1f08547a @ 0x8228 */
	jr	$ra
	nop
	.globl	sceKernelCheckExitCallback
	.type	sceKernelCheckExitCallback, @function
sceKernelCheckExitCallback: /* LoadExecForKernel 0xb57d0dec @ 0x8230 */
	jr	$ra
	nop
	.globl	sceKernelGetModel
	.type	sceKernelGetModel, @function
sceKernelGetModel: /* SysMemForKernel 0x07c586a1 @ 0x8238 */
	jr	$ra
	nop
	.globl	sceKernelMemset32
	.type	sceKernelMemset32, @function
sceKernelMemset32: /* SysMemForKernel 0x22a114dc @ 0x8240 */
	jr	$ra
	nop
	.globl	SysMemForKernel_2A8B8B2D
	.type	SysMemForKernel_2A8B8B2D, @function
SysMemForKernel_2A8B8B2D: /* SysMemForKernel 0x2a8b8b2d @ 0x8248 */
	jr	$ra
	nop
	.globl	sceKernelPartitionTotalMemSize
	.type	sceKernelPartitionTotalMemSize, @function
sceKernelPartitionTotalMemSize: /* SysMemForKernel 0x53d50ac2 @ 0x8250 */
	jr	$ra
	nop
	.globl	memset
	.type	memset, @function
memset: /* SysclibForKernel 0x10f3bb61 @ 0x8258 */
	jr	$ra
	nop
	.globl	memcpy
	.type	memcpy, @function
memcpy: /* SysclibForKernel 0xab7592ff @ 0x8260 */
	jr	$ra
	nop
	.globl	strncpy
	.type	strncpy, @function
strncpy: /* SysclibForKernel 0xb49a7697 @ 0x8268 */
	jr	$ra
	nop
	.globl	sceKernelSetEventFlag
	.type	sceKernelSetEventFlag, @function
sceKernelSetEventFlag: /* ThreadManForKernel 0x1fb15a32 @ 0x8270 */
	jr	$ra
	nop
	.globl	sceKernelWaitThreadEnd
	.type	sceKernelWaitThreadEnd, @function
sceKernelWaitThreadEnd: /* ThreadManForKernel 0x278c0df5 @ 0x8278 */
	jr	$ra
	nop
	.globl	sceKernelGetSystemTimeLow
	.type	sceKernelGetSystemTimeLow, @function
sceKernelGetSystemTimeLow: /* ThreadManForKernel 0x369ed59d @ 0x8280 */
	jr	$ra
	nop
	.globl	sceKernelWaitEventFlag
	.type	sceKernelWaitEventFlag, @function
sceKernelWaitEventFlag: /* ThreadManForKernel 0x402fcf22 @ 0x8288 */
	jr	$ra
	nop
	.globl	sceKernelCreateThread
	.type	sceKernelCreateThread, @function
sceKernelCreateThread: /* ThreadManForKernel 0x446d8de6 @ 0x8290 */
	jr	$ra
	nop
	.globl	sceKernelCreateEventFlag
	.type	sceKernelCreateEventFlag, @function
sceKernelCreateEventFlag: /* ThreadManForKernel 0x55c20a00 @ 0x8298 */
	jr	$ra
	nop
	.globl	sceKernelTryAllocateFpl
	.type	sceKernelTryAllocateFpl, @function
sceKernelTryAllocateFpl: /* ThreadManForKernel 0x623ae665 @ 0x82a0 */
	jr	$ra
	nop
	.globl	sceKernelDeleteThread
	.type	sceKernelDeleteThread, @function
sceKernelDeleteThread: /* ThreadManForKernel 0x9fa03cd3 @ 0x82a8 */
	jr	$ra
	nop
	.globl	sceKernelExitThread
	.type	sceKernelExitThread, @function
sceKernelExitThread: /* ThreadManForKernel 0xaa73c935 @ 0x82b0 */
	jr	$ra
	nop
	.globl	sceKernelCreateFpl
	.type	sceKernelCreateFpl, @function
sceKernelCreateFpl: /* ThreadManForKernel 0xc07bb470 @ 0x82b8 */
	jr	$ra
	nop
	.globl	sceKernelDelayThread
	.type	sceKernelDelayThread, @function
sceKernelDelayThread: /* ThreadManForKernel 0xceadeb47 @ 0x82c0 */
	jr	$ra
	nop
	.globl	sceKernelDeleteFpl
	.type	sceKernelDeleteFpl, @function
sceKernelDeleteFpl: /* ThreadManForKernel 0xed1410e0 @ 0x82c8 */
	jr	$ra
	nop
	.globl	sceKernelDeleteEventFlag
	.type	sceKernelDeleteEventFlag, @function
sceKernelDeleteEventFlag: /* ThreadManForKernel 0xef9e4c70 @ 0x82d0 */
	jr	$ra
	nop
	.globl	sceKernelStartThread
	.type	sceKernelStartThread, @function
sceKernelStartThread: /* ThreadManForKernel 0xf475845d @ 0x82d8 */
	jr	$ra
	nop
	.globl	sceKernelFreeFpl
	.type	sceKernelFreeFpl, @function
sceKernelFreeFpl: /* ThreadManForKernel 0xf6414a71 @ 0x82e0 */
	jr	$ra
	nop
	.globl	sceKernelDcacheWritebackInvalidateRange
	.type	sceKernelDcacheWritebackInvalidateRange, @function
sceKernelDcacheWritebackInvalidateRange: /* UtilsForKernel 0x34b9fa9e @ 0x82e8 */
	jr	$ra
	nop
	.globl	sceAudio_driver_5182B550
	.type	sceAudio_driver_5182B550, @function
sceAudio_driver_5182B550: /* sceAudio_driver 0x5182b550 @ 0x82f0 */
	jr	$ra
	nop
	.globl	sceCtrlGetButtonIntercept
	.type	sceCtrlGetButtonIntercept, @function
sceCtrlGetButtonIntercept: /* sceCtrl_driver 0x1809b9fc @ 0x82f8 */
	jr	$ra
	nop
	.globl	sceCtrlPeekBufferPositive
	.type	sceCtrlPeekBufferPositive, @function
sceCtrlPeekBufferPositive: /* sceCtrl_driver 0x2ba616af @ 0x8300 */
	jr	$ra
	nop
	.globl	sceCtrlSetSpecialButtonCallback
	.type	sceCtrlSetSpecialButtonCallback, @function
sceCtrlSetSpecialButtonCallback: /* sceCtrl_driver 0xdf53e160 @ 0x8308 */
	jr	$ra
	nop
	.globl	sceCtrlSetButtonIntercept
	.type	sceCtrlSetButtonIntercept, @function
sceCtrlSetButtonIntercept: /* sceCtrl_driver 0xf8346777 @ 0x8310 */
	jr	$ra
	nop
	.globl	sceDisplaySetMode
	.type	sceDisplaySetMode, @function
sceDisplaySetMode: /* sceDisplay_driver 0x0e20f177 @ 0x8318 */
	jr	$ra
	nop
	.globl	sceDisplayEnable
	.type	sceDisplayEnable, @function
sceDisplayEnable: /* sceDisplay_driver 0x117c3e2c @ 0x8320 */
	jr	$ra
	nop
	.globl	sceDisplayDisable
	.type	sceDisplayDisable, @function
sceDisplayDisable: /* sceDisplay_driver 0x33b620af @ 0x8328 */
	jr	$ra
	nop
	.globl	sceDisplaySetHoldMode
	.type	sceDisplaySetHoldMode, @function
sceDisplaySetHoldMode: /* sceDisplay_driver 0x3552ab11 @ 0x8330 */
	jr	$ra
	nop
	.globl	sceDisplay_driver_3E17FE8D
	.type	sceDisplay_driver_3E17FE8D, @function
sceDisplay_driver_3E17FE8D: /* sceDisplay_driver 0x3e17fe8d @ 0x8338 */
	jr	$ra
	nop
	.globl	sceDisplay_driver_7DF044BA
	.type	sceDisplay_driver_7DF044BA, @function
sceDisplay_driver_7DF044BA: /* sceDisplay_driver 0x7df044ba @ 0x8340 */
	jr	$ra
	nop
	.globl	sceDisplayGetBacklightSel
	.type	sceDisplayGetBacklightSel, @function
sceDisplayGetBacklightSel: /* sceDisplay_driver 0x96cfac38 @ 0x8348 */
	jr	$ra
	nop
	.globl	sceDisplay_driver_996881D2
	.type	sceDisplay_driver_996881D2, @function
sceDisplay_driver_996881D2: /* sceDisplay_driver 0x996881d2 @ 0x8350 */
	jr	$ra
	nop
	.globl	sceDisplay_driver_9B18DDDD
	.type	sceDisplay_driver_9B18DDDD, @function
sceDisplay_driver_9B18DDDD: /* sceDisplay_driver 0x9b18dddd @ 0x8358 */
	jr	$ra
	nop
	.globl	sceDisplay_driver_D94EB277
	.type	sceDisplay_driver_D94EB277, @function
sceDisplay_driver_D94EB277: /* sceDisplay_driver 0xd94eb277 @ 0x8360 */
	jr	$ra
	nop
	.globl	sceDisplayGetMode
	.type	sceDisplayGetMode, @function
sceDisplayGetMode: /* sceDisplay_driver 0xdea197d4 @ 0x8368 */
	jr	$ra
	nop
	.globl	sceDisplaySetBacklightSel
	.type	sceDisplaySetBacklightSel, @function
sceDisplaySetBacklightSel: /* sceDisplay_driver 0xe55f0d50 @ 0x8370 */
	jr	$ra
	nop
	.globl	sceDisplay_driver_F455917F
	.type	sceDisplay_driver_F455917F, @function
sceDisplay_driver_F455917F: /* sceDisplay_driver 0xf455917f @ 0x8378 */
	jr	$ra
	nop
	.globl	sceHprm_driver_0B83352B
	.type	sceHprm_driver_0B83352B, @function
sceHprm_driver_0B83352B: /* sceHprm_driver 0x0b83352b @ 0x8380 */
	jr	$ra
	nop
	.globl	sceHprmPeekCurrentKey
	.type	sceHprmPeekCurrentKey, @function
sceHprmPeekCurrentKey: /* sceHprm_driver 0x1910b327 @ 0x8388 */
	jr	$ra
	nop
	.globl	sceHprm_driver_1C3525DD
	.type	sceHprm_driver_1C3525DD, @function
sceHprm_driver_1C3525DD: /* sceHprm_driver 0x1c3525dd @ 0x8390 */
	jr	$ra
	nop
	.globl	sceHprm_driver_370DDC12
	.type	sceHprm_driver_370DDC12, @function
sceHprm_driver_370DDC12: /* sceHprm_driver 0x370ddc12 @ 0x8398 */
	jr	$ra
	nop
	.globl	sceHprm_driver_9BDE174B
	.type	sceHprm_driver_9BDE174B, @function
sceHprm_driver_9BDE174B: /* sceHprm_driver 0x9bde174b @ 0x83a0 */
	jr	$ra
	nop
	.globl	sceHprm_driver_AD158331
	.type	sceHprm_driver_AD158331, @function
sceHprm_driver_AD158331: /* sceHprm_driver 0xad158331 @ 0x83a8 */
	jr	$ra
	nop
	.globl	sceHprm_driver_EFCFD0C5
	.type	sceHprm_driver_EFCFD0C5, @function
sceHprm_driver_EFCFD0C5: /* sceHprm_driver 0xefcfd0c5 @ 0x83b0 */
	jr	$ra
	nop
	.globl	sceHprm_driver_FA4A25A7
	.type	sceHprm_driver_FA4A25A7, @function
sceHprm_driver_FA4A25A7: /* sceHprm_driver 0xfa4a25a7 @ 0x83b8 */
	jr	$ra
	nop
	.globl	sceKermitPeripheral_driver_76BE2E28
	.type	sceKermitPeripheral_driver_76BE2E28, @function
sceKermitPeripheral_driver_76BE2E28: /* sceKermitPeripheral_driver 0x76be2e28 @ 0x83c0 */
	jr	$ra
	nop
	.globl	sceKermit_driver_5280B410
	.type	sceKermit_driver_5280B410, @function
sceKermit_driver_5280B410: /* sceKermit_driver 0x5280b410 @ 0x83c8 */
	jr	$ra
	nop
	.globl	sceLcdc_driver_451FE1A1
	.type	sceLcdc_driver_451FE1A1, @function
sceLcdc_driver_451FE1A1: /* sceLcdc_driver 0x451fe1a1 @ 0x83d0 */
	jr	$ra
	nop
	.globl	scePower_driver_31AEA94C
	.type	scePower_driver_31AEA94C, @function
scePower_driver_31AEA94C: /* scePower_driver 0x31aea94c @ 0x83d8 */
	jr	$ra
	nop
	.globl	scePower_driver_7A9EA6DE
	.type	scePower_driver_7A9EA6DE, @function
scePower_driver_7A9EA6DE: /* scePower_driver 0x7a9ea6de @ 0x83e0 */
	jr	$ra
	nop
	.globl	scePower_driver_872F4ECE
	.type	scePower_driver_872F4ECE, @function
scePower_driver_872F4ECE: /* scePower_driver 0x872f4ece @ 0x83e8 */
	jr	$ra
	nop
	.globl	scePower_driver_8C873AA7
	.type	scePower_driver_8C873AA7, @function
scePower_driver_8C873AA7: /* scePower_driver 0x8c873aa7 @ 0x83f0 */
	jr	$ra
	nop
	.globl	scePower_driver_FA651CE1
	.type	scePower_driver_FA651CE1, @function
scePower_driver_FA651CE1: /* scePower_driver 0xfa651ce1 @ 0x83f8 */
	jr	$ra
	nop
	.globl	sceRegSetKeyValue
	.type	sceRegSetKeyValue, @function
sceRegSetKeyValue: /* sceReg_driver 0x49c70163 @ 0x8400 */
	jr	$ra
	nop
	.globl	sceRegCloseRegistry
	.type	sceRegCloseRegistry, @function
sceRegCloseRegistry: /* sceReg_driver 0x49d77d65 @ 0x8408 */
	jr	$ra
	nop
	.globl	sceRegOpenCategory
	.type	sceRegOpenCategory, @function
sceRegOpenCategory: /* sceReg_driver 0x4f471457 @ 0x8410 */
	jr	$ra
	nop
	.globl	sceRegFlushRegistry
	.type	sceRegFlushRegistry, @function
sceRegFlushRegistry: /* sceReg_driver 0x5fd4764a @ 0x8418 */
	jr	$ra
	nop
	.globl	sceRegGetKeyInfo
	.type	sceRegGetKeyInfo, @function
sceRegGetKeyInfo: /* sceReg_driver 0x9980519f @ 0x8420 */
	jr	$ra
	nop
	.globl	sceRegFlushCategory
	.type	sceRegFlushCategory, @function
sceRegFlushCategory: /* sceReg_driver 0xd743a608 @ 0x8428 */
	jr	$ra
	nop
	.globl	sceRegOpenRegistry
	.type	sceRegOpenRegistry, @function
sceRegOpenRegistry: /* sceReg_driver 0xdba46704 @ 0x8430 */
	jr	$ra
	nop
	.globl	sceRegGetKeyValue
	.type	sceRegGetKeyValue, @function
sceRegGetKeyValue: /* sceReg_driver 0xf4a3e396 @ 0x8438 */
	jr	$ra
	nop
	.globl	sceRegCloseCategory
	.type	sceRegCloseCategory, @function
sceRegCloseCategory: /* sceReg_driver 0xfc742751 @ 0x8440 */
	jr	$ra
	nop
	.globl	sceRtc_driver_9012B140
	.type	sceRtc_driver_9012B140, @function
sceRtc_driver_9012B140: /* sceRtc_driver 0x9012b140 @ 0x8448 */
	jr	$ra
	nop
	.globl	sceRtc_driver_DFF30673
	.type	sceRtc_driver_DFF30673, @function
sceRtc_driver_DFF30673: /* sceRtc_driver 0xdff30673 @ 0x8450 */
	jr	$ra
	nop
	.globl	sceKernelRegisterSysEventHandler
	.type	sceKernelRegisterSysEventHandler, @function
sceKernelRegisterSysEventHandler: /* sceSysEventForKernel 0xcd9e4bb5 @ 0x8458 */
	jr	$ra
	nop
	.globl	sceKernelUnregisterSysEventHandler
	.type	sceKernelUnregisterSysEventHandler, @function
sceKernelUnregisterSysEventHandler: /* sceSysEventForKernel 0xd7d3fdcd @ 0x8460 */
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
	.half	0x4001
	.byte	5, 0
	.half	18
	.word	.L_exp_tbl_1
	.word	0x00020000
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	31
	.word	.L_exp_tbl_2
	.word	0x00020000
	.word	.L_exp_lib_3
	.byte	17, 0
	.half	0x4001
	.byte	4, 0
	.half	4
	.word	.L_exp_tbl_3
	.word	.L_exp_lib_4
	.byte	17, 0
	.half	0x0001
	.byte	4, 0
	.half	4
	.word	.L_exp_tbl_4

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
	.word	sceKernelRegisterSysEventHandler
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	4
	.word	.L_imp_nid_1
	.word	sceKernelGetModel
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_2
	.word	sceKernelIsDVDMode
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0009
	.byte	6, 0
	.half	6
	.word	.L_imp_nid_3
	.word	sceKernelCpuSuspendIntr
	.word	0
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	3
	.word	.L_imp_nid_4
	.word	memset
	.word	.L_imp_lib_5
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	15
	.word	.L_imp_nid_5
	.word	sceKernelSetEventFlag
	.word	0
	.word	.L_imp_lib_6
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_6
	.word	sceKernelDcacheWritebackInvalidateRange
	.word	.L_imp_lib_7
	.byte	17, 0
	.half	0x0009
	.byte	6, 0
	.half	2
	.word	.L_imp_nid_7
	.word	sceKernelInvokeExitCallback
	.word	0
	.word	.L_imp_lib_8
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	4
	.word	.L_imp_nid_8
	.word	sceKernelBootFrom
	.word	.L_imp_lib_9
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	4
	.word	.L_imp_nid_9
	.word	sceCtrlGetButtonIntercept
	.word	.L_imp_lib_10
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_10
	.word	sceLcdc_driver_451FE1A1
	.word	.L_imp_lib_11
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	13
	.word	.L_imp_nid_11
	.word	sceDisplaySetMode
	.word	.L_imp_lib_12
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	2
	.word	.L_imp_nid_12
	.word	sceRtc_driver_9012B140
	.word	0
	.word	.L_imp_lib_13
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	5
	.word	.L_imp_nid_13
	.word	scePower_driver_31AEA94C
	.word	0
	.word	.L_imp_lib_14
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	9
	.word	.L_imp_nid_14
	.word	sceRegSetKeyValue
	.word	0
	.word	.L_imp_lib_15
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	8
	.word	.L_imp_nid_15
	.word	sceHprm_driver_0B83352B
	.word	.L_imp_lib_16
	.byte	17, 0
	.half	0x0001
	.byte	6, 0
	.half	5
	.word	.L_imp_nid_16
	.word	sceIoOpen
	.word	0
	.word	.L_imp_lib_17
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_17
	.word	sceAudio_driver_5182B550
	.word	.L_imp_lib_18
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_18
	.word	sceKermit_driver_5280B410
	.word	.L_imp_lib_19
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	1
	.word	.L_imp_nid_19
	.word	sceKermitPeripheral_driver_76BE2E28

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
	.byte	7, 1
	.asciz	"sceImpose_Driver"
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
	.asciz	"sceSysEventForKernel"
	.space	7
.L_imp_lib_1:
	.asciz	"SysMemForKernel"
	.space	4
.L_imp_lib_2:
	.asciz	"KDebugForKernel"
	.space	4
.L_imp_lib_3:
	.asciz	"InterruptManagerForKernel"
	.space	6
.L_imp_lib_4:
	.asciz	"SysclibForKernel"
	.space	7
.L_imp_lib_5:
	.asciz	"ThreadManForKernel"
	.space	5
.L_imp_lib_6:
	.asciz	"UtilsForKernel"
	.space	5
.L_imp_lib_7:
	.asciz	"LoadExecForKernel"
	.space	6
.L_imp_lib_8:
	.asciz	"InitForKernel"
	.space	6
.L_imp_lib_9:
	.asciz	"sceCtrl_driver"
	.space	5
.L_imp_lib_10:
	.asciz	"sceLcdc_driver"
	.space	5
.L_imp_lib_11:
	.asciz	"sceDisplay_driver"
	.space	6
.L_imp_lib_12:
	.asciz	"sceRtc_driver"
	.space	6
.L_imp_lib_13:
	.asciz	"scePower_driver"
	.space	4
.L_imp_lib_14:
	.asciz	"sceReg_driver"
	.space	6
.L_imp_lib_15:
	.asciz	"sceHprm_driver"
	.space	5
.L_imp_lib_16:
	.asciz	"IoFileMgrForKernel"
	.space	5
.L_imp_lib_17:
	.asciz	"sceAudio_driver"
	.space	4
.L_imp_lib_18:
	.asciz	"sceKermit_driver"
	.space	7
.L_imp_lib_19:
	.asciz	"sceKermitPeripheral_driver"
	.space	1
.L_exp_tbl_0:
	.word	0xd632acdb
	.word	0xadf12745
	.word	0x2f064fa6
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_start /* module_start (0xd632acdb) */
	.word	module_reboot_phase /* module_reboot_phase (0xadf12745) */
	.word	module_reboot_before /* module_reboot_before (0x2f064fa6) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"sceImpose"
	.space	2
.L_exp_tbl_1:
	.word	0x0f341be4
	.word	0x133636dd
	.word	0x24fd7bcf
	.word	0x36aa6e91
	.word	0x381bd9e7
	.word	0x5595a71a
	.word	0x7084e72c
	.word	0x72189c48
	.word	0x8c943191
	.word	0x8f6e3518
	.word	0x967f6d4a
	.word	0x9ba61b49
	.word	0x9dbce0c4
	.word	0xa9884b00
	.word	0xbb3f5dec
	.word	0xe0887bc8
	.word	0xfcd44963
	.word	0xff1a2f07
	.word	sceImposeGetHomePopup /* sceImposeGetHomePopup (0x0f341be4) */
	.word	sceImpose_133636DD /* sceImpose_133636DD (0x133636dd) */
	.word	sceImposeGetLanguageMode /* sceImposeGetLanguageMode (0x24fd7bcf) */
	.word	sceImposeSetLanguageMode /* sceImposeSetLanguageMode (0x36aa6e91) */
	.word	sceImpose_381BD9E7 /* sceImpose_381BD9E7 (0x381bd9e7) */
	.word	sceImposeSetHomePopup /* sceImposeSetHomePopup (0x5595a71a) */
	.word	sceImpose_7084E72C /* sceImpose_7084E72C (0x7084e72c) */
	.word	sceImposeSetUMDPopup /* sceImposeSetUMDPopup (0x72189c48) */
	.word	sceImpose_8C943191 /* sceImpose_8C943191 (0x8c943191) */
	.word	sceImposeGetBacklightOffTime /* sceImposeGetBacklightOffTime (0x8f6e3518) */
	.word	sceImposeSetBacklightOffTime /* sceImposeSetBacklightOffTime (0x967f6d4a) */
	.word	sceImpose_9BA61B49 /* sceImpose_9BA61B49 (0x9ba61b49) */
	.word	sceImpose_9DBCE0C4 /* sceImpose_9DBCE0C4 (0x9dbce0c4) */
	.word	sceImpose_A9884B00 /* sceImpose_A9884B00 (0xa9884b00) */
	.word	sceImpose_BB3F5DEC /* sceImpose_BB3F5DEC (0xbb3f5dec) */
	.word	sceImposeGetUMDPopup /* sceImposeGetUMDPopup (0xe0887bc8) */
	.word	sceImpose_FCD44963 /* sceImpose_FCD44963 (0xfcd44963) */
	.word	sceImpose_FF1A2F07 /* sceImpose_FF1A2F07 (0xff1a2f07) */
	.word	0x00010000
	.word	0x00030002
	.word	0x00050005
	.word	0x00060006
	.word	0x000a0008
	.word	0x000e000d
	.word	0x000f000f
	.word	0x0010000f
.L_exp_lib_2:
	.asciz	"sceImpose_driver"
	.space	3
.L_exp_tbl_2:
	.word	0x0d5f811d
	.word	0x0f067e16
	.word	0x133db99a
	.word	0x139500a7
	.word	0x28b97035
	.word	0x37f3f1d2
	.word	0x3c318569
	.word	0x5557f4e2
	.word	0x686b040e
	.word	0x842309dd
	.word	0x9e921836
	.word	0x9ff64142
	.word	0xaabee36a
	.word	0xaf5dc020
	.word	0xb376ef7f
	.word	0xb3eae8f0
	.word	0xb497314d
	.word	0xbb12f974
	.word	0xbcf1d254
	.word	0xc08c41ef
	.word	0xcbf27b10
	.word	0xce16df6d
	.word	0xd067235e
	.word	0xd3c10f9b
	.word	0xd7b9c4c8
	.word	0xdc3becff
	.word	0xe1a76241
	.word	0xe9a42056
	.word	0xe9b0e008
	.word	0xef4d1bc5
	.word	0xefb54a28
	.word	sceImpose_7084E72C /* sceImpose_driver_0D5F811D (0x0d5f811d) */
	.word	sceImpose_driver_0F067E16 /* sceImpose_driver_0F067E16 (0x0f067e16) */
	.word	sceImpose_driver_133DB99A /* sceImpose_driver_133DB99A (0x133db99a) */
	.word	sceImpose_driver_139500A7 /* sceImpose_driver_139500A7 (0x139500a7) */
	.word	sceImpose_driver_28B97035 /* sceImpose_driver_28B97035 (0x28b97035) */
	.word	sceImposeSetBacklightOffTime /* sceImpose_driver_37F3F1D2 (0x37f3f1d2) */
	.word	sceImpose_FCD44963 /* sceImpose_driver_3C318569 (0x3c318569) */
	.word	sceImpose_8C943191 /* sceImpose_driver_5557F4E2 (0x5557f4e2) */
	.word	sceImpose_driver_686B040E /* sceImpose_driver_686B040E (0x686b040e) */
	.word	sceImposeGetLanguageMode /* sceImpose_driver_842309DD (0x842309dd) */
	.word	sceImpose_driver_9E921836 /* sceImpose_driver_9E921836 (0x9e921836) */
	.word	sceImposeSetUMDPopup /* sceImpose_driver_9FF64142 (0x9ff64142) */
	.word	sceImpose_driver_AABEE36A /* sceImpose_driver_AABEE36A (0xaabee36a) */
	.word	sceImpose_driver_AF5DC020 /* sceImpose_driver_AF5DC020 (0xaf5dc020) */
	.word	sceImposeSetLanguageMode /* sceImpose_driver_B376EF7F (0xb376ef7f) */
	.word	sceImpose_driver_B3EAE8F0 /* sceImpose_driver_B3EAE8F0 (0xb3eae8f0) */
	.word	sceImpose_driver_B497314D /* sceImpose_driver_B497314D (0xb497314d) */
	.word	sceImpose_driver_BB12F974 /* sceImpose_driver_BB12F974 (0xbb12f974) */
	.word	sceImpose_9DBCE0C4 /* sceImpose_driver_BCF1D254 (0xbcf1d254) */
	.word	sceImposeSetHomePopup /* sceImpose_driver_C08C41EF (0xc08c41ef) */
	.word	sceImpose_driver_CBF27B10 /* sceImpose_driver_CBF27B10 (0xcbf27b10) */
	.word	sceImpose_FF1A2F07 /* sceImpose_driver_CE16DF6D (0xce16df6d) */
	.word	sceImpose_driver_D067235E /* sceImpose_driver_D067235E (0xd067235e) */
	.word	sceImpose_driver_D3C10F9B /* sceImpose_driver_D3C10F9B (0xd3c10f9b) */
	.word	sceImpose_driver_D7B9C4C8 /* sceImpose_driver_D7B9C4C8 (0xd7b9c4c8) */
	.word	sceImpose_133636DD /* sceImpose_driver_DC3BECFF (0xdc3becff) */
	.word	sceImposeGetBacklightOffTime /* sceImpose_driver_E1A76241 (0xe1a76241) */
	.word	sceImposeGetHomePopup /* sceImpose_driver_E9A42056 (0xe9a42056) */
	.word	sceImpose_driver_E9B0E008 /* sceImpose_driver_E9B0E008 (0xe9b0e008) */
	.word	sceImposeGetUMDPopup /* sceImpose_driver_EF4D1BC5 (0xef4d1bc5) */
	.word	sceImpose_381BD9E7 /* sceImpose_driver_EFB54A28 (0xefb54a28) */
	.word	0x00020000
	.word	0x00050004
	.word	0x00070007
	.word	0x00090008
	.word	0x000a0009
	.word	0x000e000c
	.word	0x00160013
	.word	0x001f001a
.L_exp_lib_3:
	.asciz	"sceAudioRouting"
	.align	2
.L_exp_tbl_3:
	.word	0x28235c56
	.word	0x36fd8aa9
	.word	0x39240e7d
	.word	0xbb548475
	.word	sceAudioRoutingGetVolumeMode /* sceAudioRoutingGetVolumeMode (0x28235c56) */
	.word	sceAudioRoutingSetMode /* sceAudioRoutingSetMode (0x36fd8aa9) */
	.word	sceAudioRoutingGetMode /* sceAudioRoutingGetMode (0x39240e7d) */
	.word	sceAudioRoutingSetVolumeMode /* sceAudioRoutingSetVolumeMode (0xbb548475) */
.L_exp_lib_4:
	.asciz	"sceAudioRouting_driver"
	.space	1
.L_exp_tbl_4:
	.word	0x18b6f449
	.word	0x44b384ef
	.word	0x931abef5
	.word	0xd82d02fd
	.word	sceAudioRoutingSetMode /* sceAudioRouting_driver_18B6F449 (0x18b6f449) */
	.word	sceAudioRoutingSetVolumeMode /* sceAudioRouting_driver_44B384EF (0x44b384ef) */
	.word	sceAudioRoutingGetMode /* sceAudioRouting_driver_931ABEF5 (0x931abef5) */
	.word	sceAudioRoutingGetVolumeMode /* sceAudioRouting_driver_D82D02FD (0xd82d02fd) */

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0xcd9e4bb5
	.word	0xd7d3fdcd
.L_imp_nid_1:
	.word	0x07c586a1
	.word	0x22a114dc
	.word	0x2a8b8b2d
	.word	0x53d50ac2
.L_imp_nid_2:
	.word	0xb41e2430
.L_imp_nid_3:
	.word	0x092968f4
	.word	0x4023e1a7
	.word	0x5f10d406
	.word	0xd61e6961
	.word	0xfb8e22ec
	.word	0xffa8b183
.L_imp_nid_4:
	.word	0x10f3bb61
	.word	0xab7592ff
	.word	0xb49a7697
.L_imp_nid_5:
	.word	0x1fb15a32
	.word	0x278c0df5
	.word	0x369ed59d
	.word	0x402fcf22
	.word	0x446d8de6
	.word	0x55c20a00
	.word	0x623ae665
	.word	0x9fa03cd3
	.word	0xaa73c935
	.word	0xc07bb470
	.word	0xceadeb47
	.word	0xed1410e0
	.word	0xef9e4c70
	.word	0xf475845d
	.word	0xf6414a71
.L_imp_nid_6:
	.word	0x34b9fa9e
.L_imp_nid_7:
	.word	0x1f08547a
	.word	0xb57d0dec
.L_imp_nid_8:
	.word	0x27932388
	.word	0x7233b5bc
	.word	0x9f9ae99c
	.word	0xa18a4a8b
.L_imp_nid_9:
	.word	0x1809b9fc
	.word	0x2ba616af
	.word	0xdf53e160
	.word	0xf8346777
.L_imp_nid_10:
	.word	0x451fe1a1
.L_imp_nid_11:
	.word	0x0e20f177
	.word	0x117c3e2c
	.word	0x33b620af
	.word	0x3552ab11
	.word	0x3e17fe8d
	.word	0x7df044ba
	.word	0x96cfac38
	.word	0x996881d2
	.word	0x9b18dddd
	.word	0xd94eb277
	.word	0xdea197d4
	.word	0xe55f0d50
	.word	0xf455917f
.L_imp_nid_12:
	.word	0x9012b140
	.word	0xdff30673
.L_imp_nid_13:
	.word	0x31aea94c
	.word	0x7a9ea6de
	.word	0x872f4ece
	.word	0x8c873aa7
	.word	0xfa651ce1
.L_imp_nid_14:
	.word	0x49c70163
	.word	0x49d77d65
	.word	0x4f471457
	.word	0x5fd4764a
	.word	0x9980519f
	.word	0xd743a608
	.word	0xdba46704
	.word	0xf4a3e396
	.word	0xfc742751
.L_imp_nid_15:
	.word	0x0b83352b
	.word	0x1910b327
	.word	0x1c3525dd
	.word	0x370ddc12
	.word	0x9bde174b
	.word	0xad158331
	.word	0xefcfd0c5
	.word	0xfa4a25a7
.L_imp_nid_16:
	.word	0x109f50bc
	.word	0x27eb27b8
	.word	0x54f5fb11
	.word	0x6a638d83
	.word	0x810c4bc3
.L_imp_nid_17:
	.word	0x5182b550
.L_imp_nid_18:
	.word	0x5280b410
.L_imp_nid_19:
	.word	0x76be2e28

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
