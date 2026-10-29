/* Generated PSP module import/export tables for sceSystemMemoryManager */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits

	.section .lib.ent.top, "a", @progbits
	.align	2
__lib_ent_top:
	.word	0

	.section .lib.ent, "a", @progbits
	.align	2
	.globl	__library_exports_0
__library_exports_0:
	.word	0
	.byte	0, 0
	.half	0x8000
	.byte	4, 2
	.half	1
	.word	.L_exp_tbl_0
	.globl	__library_exports_1
__library_exports_1:
	.word	.L_exp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	105
	.word	.L_exp_tbl_1
	.word	0x00040000
	.globl	__library_exports_2
__library_exports_2:
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x4000
	.byte	5, 0
	.half	27
	.word	.L_exp_tbl_2
	.word	0x00020000
	.globl	__library_exports_3
__library_exports_3:
	.word	.L_exp_lib_3
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	33
	.word	.L_exp_tbl_3
	.word	0x00020000
	.globl	__library_exports_4
__library_exports_4:
	.word	.L_exp_lib_4
	.byte	17, 0
	.half	0x0001
	.byte	4, 0
	.half	5
	.word	.L_exp_tbl_4
	.globl	__library_exports_5
__library_exports_5:
	.word	.L_exp_lib_5
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	16
	.word	.L_exp_tbl_5
	.word	0x00020000
	.globl	__library_exports_6
__library_exports_6:
	.word	.L_exp_lib_6
	.byte	17, 0
	.half	0x4000
	.byte	4, 0
	.half	6
	.word	.L_exp_tbl_6
	.globl	__library_exports_7
__library_exports_7:
	.word	.L_exp_lib_7
	.byte	17, 0
	.half	0x4001
	.byte	5, 0
	.half	48
	.word	.L_exp_tbl_7
	.word	0x00020000
	.globl	__library_exports_8
__library_exports_8:
	.word	.L_exp_lib_8
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	73
	.word	.L_exp_tbl_8
	.word	0x00040000
	.globl	__library_exports_9
__library_exports_9:
	.word	.L_exp_lib_9
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	29
	.word	.L_exp_tbl_9
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
	.byte	17, 1
	.asciz	"sceSystemMemoryManager"
	.space	5
	.word	_gp
	.word	__lib_ent_top + 4
	.word	__lib_ent_btm
	.word	__lib_stub_top + 4
	.word	__lib_stub_btm

	.section .rodata.sceResident, "a", @progbits
	.align	2
	.word	0
.L_exp_tbl_0:
	.word	0xd3744be0
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_bootstart /* module_bootstart (0xd3744be0) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"SysMemForKernel"
	.align	2
.L_exp_tbl_1:
	.word	0x00154b6d
	.word	0x0115b0f8
	.word	0x01810023
	.word	0x034129fb
	.word	0x07c586a1
	.word	0x0a34c078
	.word	0x13ee28da
	.word	0x13f4a0de
	.word	0x1404c1aa
	.word	0x181065ab
	.word	0x1a726c89
	.word	0x1ab50974
	.word	0x1e6bb8e8
	.word	0x1ecab5c4
	.word	0x21487e5b
	.word	0x22a114dc
	.word	0x235c2646
	.word	0x23d81675
	.word	0x2a8b8b2d
	.word	0x310802a9
	.word	0x35170484
	.word	0x361f0f88
	.word	0x36c503a9
	.word	0x3e384043
	.word	0x3ec451eb
	.word	0x40b744a4
	.word	0x43e0a861
	.word	0x44bdf332
	.word	0x45332d44
	.word	0x474ca24f
	.word	0x476b244f
	.word	0x476f687a
	.word	0x4972f9d1
	.word	0x4a325aa0
	.word	0x521ac5a4
	.word	0x53d50ac2
	.word	0x57fa5af4
	.word	0x58148f07
	.word	0x5fe712e9
	.word	0x6d9e2dd6
	.word	0x6e4f9f62
	.word	0x7158ce7e
	.word	0x757eac7d
	.word	0x79bd1975
	.word	0x7a7cd7bc
	.word	0x7d50ce82
	.word	0x7dda0e01
	.word	0x7ff2f35a
	.word	0x807179e7
	.word	0x83b5226d
	.word	0x845310cf
	.word	0x87c2ab85
	.word	0x8ae776af
	.word	0x8e742787
	.word	0x96a3ce2c
	.word	0x982a4779
	.word	0x9b20acef
	.word	0x9bac123d
	.word	0x9e5b1acb
	.word	0x9f205d3e
	.word	0xa03cb480
	.word	0xa0a9185a
	.word	0xa7622297
	.word	0xac12f678
	.word	0xb0ca8a4e
	.word	0xb4f00cb5
	.word	0xb9b7281a
	.word	0xbfd53fb7
	.word	0xbfe08689
	.word	0xc1a26c6f
	.word	0xc4d3a378
	.word	0xc4eeaf20
	.word	0xc5ec364c
	.word	0xc886b169
	.word	0xc90b0992
	.word	0xcc31dead
	.word	0xd0c1460d
	.word	0xd222daa7
	.word	0xd3ca555c
	.word	0xdd6512d0
	.word	0xe0eb8cbe
	.word	0xe13ddc87
	.word	0xe3f9c38e
	.word	0xe5fb2ee5
	.word	0xe73fbc0b
	.word	0xe7a3f4d5
	.word	0xe860be8f
	.word	0xea1cabf1
	.word	0xef29061c
	.word	0xf12a62f7
	.word	0xf19ba38d
	.word	0xf2284ecc
	.word	0xf3bdb718
	.word	0xf5153060
	.word	0xf5228c40
	.word	0xf7e78b33
	.word	0xf8f54a95
	.word	0xfaf29f34
	.word	0xfb5beb66
	.word	0xfc639a2b
	.word	0xfd15d334
	.word	0xfdbdeb5c
	.word	0xfdc97d28
	.word	0xfe3cf2bc
	.word	0xffc63884
	.word	sceKernelSysMemMaxFreeMemSize /* sceKernelSysMemMaxFreeMemSize (0x00154b6d) */
	.word	sceKernelPartitionTotalFreeMemSize /* sceKernelPartitionTotalFreeMemSize (0x0115b0f8) */
	.word	sceKernelHeapTotalFreeSize /* sceKernelHeapTotalFreeSize (0x01810023) */
	.word	sceKernelCreateUIDtype /* sceKernelCreateUIDtype (0x034129fb) */
	.word	sceKernelGetModel /* sceKernelGetModel (0x07c586a1) */
	.word	sceKernelCreateUID /* sceKernelCreateUID (0x0a34c078) */
	.word	SysMemForKernel_13EE28DA /* SysMemForKernel_13EE28DA (0x13ee28da) */
	.word	sceKernelPartitionMaxFreeMemSize /* sceKernelPartitionMaxFreeMemSize (0x13f4a0de) */
	.word	sceKernelSetUmdCacheOn /* sceKernelSetUmdCacheOn (0x1404c1aa) */
	.word	sceKernelMemcpy /* sceKernelMemcpy (0x181065ab) */
	.word	sceKernelSetGprofFunction /* sceKernelSetGprofFunction (0x1a726c89) */
	.word	sceKernelJointMemoryBlock /* sceKernelJointMemoryBlock (0x1ab50974) */
	.word	sceKernelMemoryExtendSize /* sceKernelMemoryExtendSize (0x1e6bb8e8) */
	.word	sceKernelDeleteUIDtype /* sceKernelDeleteUIDtype (0x1ecab5c4) */
	.word	sceKernelLookupUIDFunction /* sceKernelLookupUIDFunction (0x21487e5b) */
	.word	sceKernelMemset32 /* sceKernelMemset32 (0x22a114dc) */
	.word	sceKernelCallUIDObjCommonFunction /* sceKernelCallUIDObjCommonFunction (0x235c2646) */
	.word	sceKernelAllocHeapMemory /* sceKernelAllocHeapMemory (0x23d81675) */
	.word	SysMemForKernel_2A8B8B2D /* SysMemForKernel_2A8B8B2D (0x2a8b8b2d) */
	.word	sceKernelRegisterGetIdFunc /* sceKernelRegisterGetIdFunc (0x310802a9) */
	.word	sceKernelMemoryOpenSize /* sceKernelMemoryOpenSize (0x35170484) */
	.word	sceKernelDeleteUID /* sceKernelDeleteUID (0x361f0f88) */
	.word	sceKernelGetSystemStatus /* sceKernelGetSystemStatus (0x36c503a9) */
	.word	sceKernelPrintUidListAll /* sceKernelPrintUidListAll (0x3e384043) */
	.word	sceKernelGetUIDname /* sceKernelGetUIDname (0x3ec451eb) */
	.word	SysMemForKernel_40B744A4 /* SysMemForKernel_40B744A4 (0x40b744a4) */
	.word	sceKernelGetQTGP3 /* sceKernelGetQTGP3 (0x43e0a861) */
	.word	sceKernelGetUIDcontrolBlockWithType /* sceKernelGetUIDcontrolBlockWithType (0x44bdf332) */
	.word	sceKernelSysMemTotalFreeMemSize /* sceKernelSysMemTotalFreeMemSize (0x45332d44) */
	.word	sceKernelGetQTGP2 /* sceKernelGetQTGP2 (0x474ca24f) */
	.word	sceKernelQueryLowheapInfo /* sceKernelQueryLowheapInfo (0x476b244f) */
	.word	sceKernelSetCompilerVersion /* sceKernelSetCompilerVersion (0x476f687a) */
	.word	sceKernelGetAllowReplaceUmd /* sceKernelGetAllowReplaceUmd (0x4972f9d1) */
	.word	sceKernelGetInitialRandomValue /* sceKernelGetInitialRandomValue (0x4a325aa0) */
	.word	sceKernelSetSystemStatus /* sceKernelSetSystemStatus (0x521ac5a4) */
	.word	sceKernelPartitionTotalMemSize /* sceKernelPartitionTotalMemSize (0x53d50ac2) */
	.word	sceKernelGetSysMemoryInfo /* sceKernelGetSysMemoryInfo (0x57fa5af4) */
	.word	sceKernelCreateHeap /* sceKernelCreateHeap (0x58148f07) */
	.word	sceKernelCallGcovFunction /* sceKernelCallGcovFunction (0x5fe712e9) */
	.word	sceKernelSysMemRealMemorySize /* sceKernelSysMemRealMemorySize (0x6d9e2dd6) */
	.word	sceKernelGetCompilerVersion /* sceKernelGetCompilerVersion (0x6e4f9f62) */
	.word	sceKernelAllocPartitionMemory /* sceKernelAllocPartitionMemory (0x7158ce7e) */
	.word	sceKernelCallUIDFunction /* sceKernelCallUIDFunction (0x757eac7d) */
	.word	sceKernelQueryHeapInfo /* sceKernelQueryHeapInfo (0x79bd1975) */
	.word	sceKernelMemoryShrinkSize /* sceKernelMemoryShrinkSize (0x7a7cd7bc) */
	.word	sceKernelCallUIDObjFunction /* sceKernelCallUIDObjFunction (0x7d50ce82) */
	.word	sceKernelHoldUID /* sceKernelHoldUID (0x7dda0e01) */
	.word	SysMemUserForUser_945E45DA /* SysMemUserForUser_945E45DA (0x7ff2f35a) */
	.word	SysMemForKernel_807179E7 /* SysMemForKernel_807179E7 (0x807179e7) */
	.word	sceKernelSetDdrMemoryProtection /* sceKernelSetDdrMemoryProtection (0x83b5226d) */
	.word	sceKernelApiEvaluationReport /* sceKernelApiEvaluationReport (0x845310cf) */
	.word	sceKernelFreeHeapMemory /* sceKernelFreeHeapMemory (0x87c2ab85) */
	.word	sceKernelMemset /* sceKernelMemset (0x8ae776af) */
	.word	sceKernelSizeLockMemoryBlock /* sceKernelSizeLockMemoryBlock (0x8e742787) */
	.word	sceKernelSetRebootKernel /* sceKernelSetRebootKernel (0x96a3ce2c) */
	.word	sceKernelSetDNAS /* sceKernelSetDNAS (0x982a4779) */
	.word	sceKernelMemmoveWithFill /* sceKernelMemmoveWithFill (0x9b20acef) */
	.word	sceKernelMemmove /* sceKernelMemmove (0x9bac123d) */
	.word	sceKernelSetQTGP2 /* sceKernelSetQTGP2 (0x9e5b1acb) */
	.word	sceKernelGetUIDtype /* sceKernelGetUIDtype (0x9f205d3e) */
	.word	SysMemForKernel_A03CB480 /* SysMemForKernel_A03CB480 (0xa03cb480) */
	.word	SysMemUserForUser_D8DE5C1E /* SysMemUserForUser_D8DE5C1E (0xa0a9185a) */
	.word	sceKernelRenameUID /* sceKernelRenameUID (0xa7622297) */
	.word	sceKernelGetUidmanCB /* sceKernelGetUidmanCB (0xac12f678) */
	.word	sceKernelReleaseUID /* sceKernelReleaseUID (0xb0ca8a4e) */
	.word	sceKernelGetCompiledSdkVersion /* sceKernelGetCompiledSdkVersion (0xb4f00cb5) */
	.word	sceKernelCopyGameInfo /* sceKernelCopyGameInfo (0xb9b7281a) */
	.word	sceKernelGetDNAS /* sceKernelGetDNAS (0xbfd53fb7) */
	.word	SysMemForKernel_BFE08689 /* SysMemForKernel_BFE08689 (0xbfe08689) */
	.word	sceKernelFreePartitionMemory /* sceKernelFreePartitionMemory (0xc1a26c6f) */
	.word	sceKernelGetAWeDramSaveSize /* sceKernelGetAWeDramSaveSize (0xc4d3a378) */
	.word	sceKernelQueryMemoryPartitionInfo /* sceKernelQueryMemoryPartitionInfo (0xc4eeaf20) */
	.word	sceKernelApiEvaluationInit /* sceKernelApiEvaluationInit (0xc5ec364c) */
	.word	sceKernelDevkitVersion /* sceKernelDevkitVersion (0xc886b169) */
	.word	sceKernelGetUIDcontrolBlock /* sceKernelGetUIDcontrolBlock (0xc90b0992) */
	.word	SysMemForKernel_CC31DEAD /* SysMemForKernel_CC31DEAD (0xcc31dead) */
	.word	sceKernelGetId /* sceKernelGetId (0xd0c1460d) */
	.word	sceKernelCreateUIDtypeInherit /* sceKernelCreateUIDtypeInherit (0xd222daa7) */
	.word	sceKernelGetMEeDramSaveSize /* sceKernelGetMEeDramSaveSize (0xd3ca555c) */
	.word	sceKernelDeleteHeap /* sceKernelDeleteHeap (0xdd6512d0) */
	.word	sceKernelGetAWeDramSaveAddr /* sceKernelGetAWeDramSaveAddr (0xe0eb8cbe) */
	.word	sceKernelIsHold /* sceKernelIsHold (0xe13ddc87) */
	.word	sceKernelSearchUIDbyName /* sceKernelSearchUIDbyName (0xe3f9c38e) */
	.word	sceKernelMemoryCloseSize /* sceKernelMemoryCloseSize (0xe5fb2ee5) */
	.word	sceKernelRebootKernel /* sceKernelRebootKernel (0xe73fbc0b) */
	.word	sceKernelCallGprofFunction /* sceKernelCallGprofFunction (0xe7a3f4d5) */
	.word	sceKernelQueryMemoryBlockInfo /* sceKernelQueryMemoryBlockInfo (0xe860be8f) */
	.word	sceKernelFillFreeBlock /* sceKernelFillFreeBlock (0xea1cabf1) */
	.word	sceKernelGetGameInfo /* sceKernelGetGameInfo (0xef29061c) */
	.word	sceKernelGetBlockHeadAddr /* sceKernelGetBlockHeadAddr (0xf12a62f7) */
	.word	sceKernelSetAllowReplaceUmd /* sceKernelSetAllowReplaceUmd (0xf19ba38d) */
	.word	sceKernelAllocHeapMemoryWithOption /* sceKernelAllocHeapMemoryWithOption (0xf2284ecc) */
	.word	SysMemForKernel_F3BDB718 /* SysMemForKernel_F3BDB718 (0xf3bdb718) */
	.word	sceKernelRegisterApiEvaluation /* sceKernelRegisterApiEvaluation (0xf5153060) */
	.word	sceKernelGetSysmemIdList /* sceKernelGetSysmemIdList (0xf5228c40) */
	.word	sceKernelSeparateMemoryBlock /* sceKernelSeparateMemoryBlock (0xf7e78b33) */
	.word	sceKernelApiEvaliationAddData /* sceKernelApiEvaliationAddData (0xf8f54a95) */
	.word	sceKernelQueryMemoryInfo /* sceKernelQueryMemoryInfo (0xfaf29f34) */
	.word	sceKernelResizeMemoryBlock /* sceKernelResizeMemoryBlock (0xfb5beb66) */
	.word	sceKernelSetQTGP3 /* sceKernelSetQTGP3 (0xfc639a2b) */
	.word	sceKernelSetGcovFunction /* sceKernelSetGcovFunction (0xfd15d334) */
	.word	sceKernelSysMemMemSize /* sceKernelSysMemMemSize (0xfdbdeb5c) */
	.word	sceKernelGetMEeDramSaveAddr /* sceKernelGetMEeDramSaveAddr (0xfdc97d28) */
	.word	sceKernelGetUsersystemLibWork /* sceKernelGetUsersystemLibWork (0xfe3cf2bc) */
	.word	sceKernelIsKindOf /* sceKernelIsKindOf (0xffc63884) */
	.word	0x00040000
	.word	0x00060005
	.word	0x00080006
	.word	0x000c0009
	.word	0x0012000e
	.word	0x00130012
	.word	0x00140013
	.word	0x00170017
	.word	0x001b0019
	.word	0x00220020
	.word	0x00240022
	.word	0x00260025
	.word	0x00270027
	.word	0x00270027
	.word	0x002a0029
	.word	0x002d002b
	.word	0x00320030
	.word	0x00350034
	.word	0x00360036
	.word	0x003a0037
	.word	0x003e003c
	.word	0x003f003f
	.word	0x00410040
	.word	0x00430042
	.word	0x00460045
	.word	0x004b0049
	.word	0x004f004c
	.word	0x004f004f
	.word	0x00530050
	.word	0x00580056
	.word	0x005d0059
	.word	0x00630060
.L_exp_lib_2:
	.asciz	"SysMemUserForUser"
	.space	2
.L_exp_tbl_2:
	.word	0x057e7380
	.word	0x13a5abef
	.word	0x1b4217bc
	.word	0x237dbd4f
	.word	0x2a3e5280
	.word	0x315ad3a0
	.word	0x342061e5
	.word	0x35669d4c
	.word	0x358ca1bb
	.word	0x3fc9ae6a
	.word	0x50f61d8a
	.word	0x7591c7db
	.word	0x7893f79a
	.word	0x91de343c
	.word	0x945e45da
	.word	0x9d9a5ba1
	.word	0xa291f107
	.word	0xa6848df8
	.word	0xacbd88ca
	.word	0xb6d61d02
	.word	0xd8de5c1e
	.word	0xdb83a952
	.word	0xebd5c3e6
	.word	0xf77d77cb
	.word	0xf919f628
	.word	0xfc114573
	.word	0xfe707fdf
	.word	sceKernelSetCompiledSdkVersion410_420 /* sceKernelSetCompiledSdkVersion410_420 (0x057e7380) */
	.word	KprintfForUser /* KprintfForUser (0x13a5abef) */
	.word	sceKernelSetCompiledSdkVersion630_650 /* sceKernelSetCompiledSdkVersion630_650 (0x1b4217bc) */
	.word	sceKernelAllocPartitionMemoryForUser /* sceKernelAllocPartitionMemoryForUser (0x237dbd4f) */
	.word	sceKernelQueryMemoryInfoForUser /* sceKernelQueryMemoryInfoForUser (0x2a3e5280) */
	.word	sceKernelSetCompiledSdkVersion380_390 /* sceKernelSetCompiledSdkVersion380_390 (0x315ad3a0) */
	.word	sceKernelSetCompiledSdkVersion370 /* sceKernelSetCompiledSdkVersion370 (0x342061e5) */
	.word	sceKernelSetCompiledSdkVersion600_620 /* sceKernelSetCompiledSdkVersion600_620 (0x35669d4c) */
	.word	sceKernelSetCompiledSdkVersion660 /* sceKernelSetCompiledSdkVersion660 (0x358ca1bb) */
	.word	sceKernelDevkitVersion /* sceKernelDevkitVersion (0x3fc9ae6a) */
	.word	sceKernelFreeMemoryBlock /* sceKernelFreeMemoryBlock (0x50f61d8a) */
	.word	sceKernelSetCompiledSdkVersion100 /* sceKernelSetCompiledSdkVersion100 (0x7591c7db) */
	.word	sceKernelSetCompiledSdkVersion570 /* sceKernelSetCompiledSdkVersion570 (0x7893f79a) */
	.word	sceKernelSetCompiledSdkVersion500_550 /* sceKernelSetCompiledSdkVersion500_550 (0x91de343c) */
	.word	SysMemUserForUser_945E45DA /* SysMemUserForUser_945E45DA (0x945e45da) */
	.word	sceKernelGetBlockHeadAddrForUser /* sceKernelGetBlockHeadAddrForUser (0x9d9a5ba1) */
	.word	sceKernelPartitionMaxFreeMemSizeForUser /* sceKernelPartitionMaxFreeMemSizeForUser (0xa291f107) */
	.word	sceKernelSetUsersystemLibWork /* sceKernelSetUsersystemLibWork (0xa6848df8) */
	.word	sceKernelTotalMemSize /* sceKernelTotalMemSize (0xacbd88ca) */
	.word	sceKernelFreePartitionMemoryForUser /* sceKernelFreePartitionMemoryForUser (0xb6d61d02) */
	.word	SysMemUserForUser_D8DE5C1E /* SysMemUserForUser_D8DE5C1E (0xd8de5c1e) */
	.word	sceKernelGetMemoryBlockAddr /* sceKernelGetMemoryBlockAddr (0xdb83a952) */
	.word	sceKernelSetCompiledSdkVersion395 /* sceKernelSetCompiledSdkVersion395 (0xebd5c3e6) */
	.word	sceKernelSetCompilerVersion /* sceKernelSetCompilerVersion (0xf77d77cb) */
	.word	sceKernelPartitionTotalFreeMemSizeForUser /* sceKernelPartitionTotalFreeMemSizeForUser (0xf919f628) */
	.word	sceKernelGetCompiledSdkVersion /* sceKernelGetCompiledSdkVersion (0xfc114573) */
	.word	sceKernelAllocMemoryBlock /* sceKernelAllocMemoryBlock (0xfe707fdf) */
	.word	0x00010000
	.word	0x00050003
	.word	0x000a000a
	.word	0x000b000b
	.word	0x000d000d
	.word	0x00130010
	.word	0x00140014
	.word	0x00170016
.L_exp_lib_3:
	.asciz	"SysclibForKernel"
	.space	3
.L_exp_tbl_3:
	.word	0x097049bd
	.word	0x0d188658
	.word	0x0dfb7b6c
	.word	0x10f3bb61
	.word	0x1493ebd9
	.word	0x243665ed
	.word	0x32c767f2
	.word	0x3ec5bbf6
	.word	0x476fd94a
	.word	0x47dd934d
	.word	0x4c0e0274
	.word	0x52df196c
	.word	0x5e8e5f42
	.word	0x68a78817
	.word	0x6a7900e1
	.word	0x7661e728
	.word	0x7ab35214
	.word	0x7dee14de
	.word	0x7f8a6f23
	.word	0x81d0d1f7
	.word	0x86fefce9
	.word	0x87c78fb6
	.word	0x90c5573d
	.word	0xa48d2592
	.word	0xab7592ff
	.word	0xb1dc2ae8
	.word	0xb49a7697
	.word	0xc0ab8932
	.word	0xc2145e80
	.word	0xce2f7487
	.word	0xd1cd40e5
	.word	0xdf17f4a2
	.word	0xec6f1cf2
	.word	bcopy /* bcopy (0x097049bd) */
	.word	strstr /* strstr (0x0d188658) */
	.word	strpbrk /* strpbrk (0x0dfb7b6c) */
	.word	memset /* memset (0x10f3bb61) */
	.word	wmemset /* wmemset (0x1493ebd9) */
	.word	rindex /* rindex (0x243665ed) */
	.word	look_ctype_table /* look_ctype_table (0x32c767f2) */
	.word	tolower /* tolower (0x3ec5bbf6) */
	.word	strcat /* strcat (0x476fd94a) */
	.word	strtol /* strtol (0x47dd934d) */
	.word	strrchr /* strrchr (0x4c0e0274) */
	.word	strlen /* strlen (0x52df196c) */
	.word	__umoddi3 /* __umoddi3 (0x5e8e5f42) */
	.word	memchr /* memchr (0x68a78817) */
	.word	strtoul /* strtoul (0x6a7900e1) */
	.word	sprintf /* sprintf (0x7661e728) */
	.word	strncmp /* strncmp (0x7ab35214) */
	.word	__udivdi3 /* __udivdi3 (0x7dee14de) */
	.word	bcmp /* bcmp (0x7f8a6f23) */
	.word	memcmp /* memcmp (0x81d0d1f7) */
	.word	bzero /* bzero (0x86fefce9) */
	.word	prnt /* prnt (0x87c78fb6) */
	.word	strnlen /* strnlen (0x90c5573d) */
	.word	memmove /* memmove (0xa48d2592) */
	.word	memcpy /* memcpy (0xab7592ff) */
	.word	strchr /* strchr (0xb1dc2ae8) */
	.word	strncpy /* strncpy (0xb49a7697) */
	.word	strcmp /* strcmp (0xc0ab8932) */
	.word	snprintf /* snprintf (0xc2145e80) */
	.word	toupper /* toupper (0xce2f7487) */
	.word	index /* index (0xd1cd40e5) */
	.word	__udivmoddi4 /* __udivmoddi4 (0xdf17f4a2) */
	.word	strcpy /* strcpy (0xec6f1cf2) */
	.word	0x00030000
	.word	0x00060005
	.word	0x000b0008
	.word	0x000f000d
	.word	0x00160013
	.word	0x00190017
	.word	0x001e001b
	.word	0x00210020
.L_exp_lib_4:
	.asciz	"sceSysEventForKernel"
	.space	3
.L_exp_tbl_4:
	.word	0x36331294
	.word	0x68d55505
	.word	0xaeb300ae
	.word	0xcd9e4bb5
	.word	0xd7d3fdcd
	.word	sceKernelSysEventDispatch /* sceKernelSysEventDispatch (0x36331294) */
	.word	sceKernelReferSysEventHandler /* sceKernelReferSysEventHandler (0x68d55505) */
	.word	sceKernelIsRegisterSysEventHandler /* sceKernelIsRegisterSysEventHandler (0xaeb300ae) */
	.word	sceKernelRegisterSysEventHandler /* sceKernelRegisterSysEventHandler (0xcd9e4bb5) */
	.word	sceKernelUnregisterSysEventHandler /* sceKernelUnregisterSysEventHandler (0xd7d3fdcd) */
.L_exp_lib_5:
	.asciz	"sceSuspendForKernel"
	.align	2
.L_exp_tbl_5:
	.word	0x090ccb3f
	.word	0x0ab0c6f3
	.word	0x3aee7261
	.word	0x3e0271d3
	.word	0x67b59042
	.word	0x8f58b1ec
	.word	0x91a77137
	.word	0x98a1d061
	.word	0xa14f40b2
	.word	0xa569e425
	.word	0xb2c9640b
	.word	0xb43d1a8c
	.word	0xb53b2147
	.word	0xbde686cd
	.word	0xc7c928c7
	.word	0xeadb1bd7
	.word	sceKernelPowerTick /* sceKernelPowerTick (0x090ccb3f) */
	.word	sceKernelDispatchResumeHandlers /* sceKernelDispatchResumeHandlers (0x0ab0c6f3) */
	.word	sceKernelPowerUnlock /* sceKernelPowerUnlockForUser (0x3aee7261) */
	.word	sceKernelVolatileMemLock /* sceKernelVolatileMemLock (0x3e0271d3) */
	.word	sceSuspendForKernel_67B59042 /* sceSuspendForKernel_67B59042 (0x67b59042) */
	.word	sceKernelDispatchSuspendHandlers /* sceKernelDispatchSuspendHandlers (0x8f58b1ec) */
	.word	sceKernelRegisterSuspendHandler /* sceKernelRegisterSuspendHandler (0x91a77137) */
	.word	sceKernelPowerRebootStart /* sceKernelPowerRebootStart (0x98a1d061) */
	.word	sceKernelVolatileMemTryLock /* sceKernelVolatileMemTryLock (0xa14f40b2) */
	.word	sceKernelVolatileMemUnlock /* sceKernelVolatileMemUnlock (0xa569e425) */
	.word	sceSuspendForKernel_B2C9640B /* sceSuspendForKernel_B2C9640B (0xb2c9640b) */
	.word	sceKernelRegisterResumeHandler /* sceKernelRegisterResumeHandler (0xb43d1a8c) */
	.word	sceKernelPowerLockForUser /* sceKernelPowerLockForUser (0xb53b2147) */
	.word	sceKernelRegisterPowerHandlers /* sceKernelRegisterPowerHandlers (0xbde686cd) */
	.word	sceKernelPowerUnlockForUser /* sceKernelPowerUnlockForUser (0xc7c928c7) */
	.word	sceKernelPowerLock /* sceKernelPowerLockForUser (0xeadb1bd7) */
	.word	0x00020000
	.word	0x00020002
	.word	0x00040004
	.word	0x00050004
	.word	0x00060005
	.word	0x000a0008
	.word	0x000f000e
	.word	0x0010000f
.L_exp_lib_6:
	.asciz	"sceSuspendForUser"
	.space	2
.L_exp_tbl_6:
	.word	0x090ccb3f
	.word	0x3aee7261
	.word	0x3e0271d3
	.word	0xa14f40b2
	.word	0xa569e425
	.word	0xeadb1bd7
	.word	sceKernelPowerTick /* sceKernelPowerTick (0x090ccb3f) */
	.word	sceKernelPowerUnlockForUser /* sceKernelPowerUnlockForUser (0x3aee7261) */
	.word	sceKernelVolatileMemLock /* sceKernelVolatileMemLock (0x3e0271d3) */
	.word	sceKernelVolatileMemTryLock /* sceKernelVolatileMemTryLock (0xa14f40b2) */
	.word	sceKernelVolatileMemUnlock /* sceKernelVolatileMemUnlock (0xa569e425) */
	.word	sceKernelPowerLockForUser /* sceKernelPowerLockForUser (0xeadb1bd7) */
.L_exp_lib_7:
	.asciz	"UtilsForUser"
	.space	3
.L_exp_tbl_7:
	.word	0x004d4dee
	.word	0x06fb8a63
	.word	0x0939d45c
	.word	0x157a383a
	.word	0x16641d70
	.word	0x1b0592a3
	.word	0x27cc57f0
	.word	0x346f6da8
	.word	0x34b9fa9e
	.word	0x37fb5c42
	.word	0x39f49610
	.word	0x3ee30821
	.word	0x3fd3d324
	.word	0x43c9a8db
	.word	0x485e430a
	.word	0x4fd31c9d
	.word	0x515b4faf
	.word	0x585f1c09
	.word	0x5c7f2b1a
	.word	0x601e43e6
	.word	0x61e1e525
	.word	0x6231a71d
	.word	0x6ad345d7
	.word	0x71ec4271
	.word	0x7333e539
	.word	0x740df7f0
	.word	0x77dff087
	.word	0x79d1c3fa
	.word	0x7c5dc253
	.word	0x80001c4c
	.word	0x840259f1
	.word	0x87e81561
	.word	0x91e4f6a7
	.word	0x920f104a
	.word	0x99134c3f
	.word	0x9e5c5086
	.word	0xaf3766bb
	.word	0xb435dec5
	.word	0xb83a1e76
	.word	0xb8d24e78
	.word	0xbfa98062
	.word	0xc2df770e
	.word	0xc8186a58
	.word	0xdbbe9a46
	.word	0xe860e75e
	.word	0xf0155bca
	.word	0xf8fcd5ba
	.word	0xfb05fad0
	.word	sceKernelL1DcacheWritebackRange /* sceKernelL1DcacheWritebackRange (0x004d4dee) */
	.word	sceKernelUtilsMt19937UInt /* sceKernelUtilsMt19937UInt (0x06fb8a63) */
	.word	sceKernelL2cacheWritebackAll /* sceKernelL2cacheWritebackAll (0x0939d45c) */
	.word	sceKernelL1DcacheInvalidateRangeForUser /* sceKernelL1DcacheInvalidateRange (0x157a383a) */
	.word	sceKernelDcacheReadTagForUser /* sceKernelDcacheReadTag (0x16641d70) */
	.word	sceKernelL1DcacheWritebackInvalidateRange /* sceKernelL1DcacheWritebackInvalidateRange (0x1b0592a3) */
	.word	sceKernelLibcTime /* sceKernelLibcTime (0x27cc57f0) */
	.word	sceKernelUtilsSha1BlockUpdate /* sceKernelUtilsSha1BlockUpdate (0x346f6da8) */
	.word	sceKernelDcacheWritebackInvalidateRange /* sceKernelDcacheWritebackInvalidateRange (0x34b9fa9e) */
	.word	sceKernelGetGPI /* sceKernelGetGPI (0x37fb5c42) */
	.word	sceKernelGetPTRIG /* sceKernelGetPTRIG (0x39f49610) */
	.word	sceKernelDcacheWritebackRange /* sceKernelDcacheWritebackRange (0x3ee30821) */
	.word	sceKernelL1DcacheWritebackAll /* sceKernelL1DcacheWritebackAll (0x3fd3d324) */
	.word	sceKernelL1IcacheInvalidateRangeForUser /* sceKernelL1IcacheInvalidateRange (0x43c9a8db) */
	.word	sceKernelL2cacheWritebackInvalidateAll /* sceKernelL2cacheWritebackInvalidateAll (0x485e430a) */
	.word	sceKernelIcacheProbe /* sceKernelIcacheProbe (0x4fd31c9d) */
	.word	sceKernelDcacheReadTagForUser /* sceKernelDcacheReadTag (0x515b4faf) */
	.word	sceKernelUtilsSha1BlockResult /* sceKernelUtilsSha1BlockResult (0x585f1c09) */
	.word	sceKernelIcacheReadTagForUser /* sceKernelIcacheReadTag (0x5c7f2b1a) */
	.word	sceKernelL2cacheWritebackRange /* sceKernelL2cacheWritebackRange (0x601e43e6) */
	.word	sceKernelUtilsMd5BlockUpdate /* sceKernelUtilsMd5BlockUpdate (0x61e1e525) */
	.word	sceKernelSetPTRIG /* sceKernelSetPTRIG (0x6231a71d) */
	.word	sceKernelSetGPO /* sceKernelSetGPO (0x6ad345d7) */
	.word	sceKernelLibcGettimeofday /* sceKernelLibcGettimeofday (0x71ec4271) */
	.word	sceKernelL1IcacheInvalidateAll /* sceKernelL1IcacheInvalidateAll (0x7333e539) */
	.word	sceKernelL1DcacheProbe /* sceKernelL1DcacheProbe (0x740df7f0) */
	.word	sceKernelDcacheProbeRange /* sceKernelDcacheProbeRange (0x77dff087) */
	.word	sceKernelDcacheWritebackAll /* sceKernelDcacheWritebackAll (0x79d1c3fa) */
	.word	sceKernelL2cacheWritebackInvalidateRange /* sceKernelL2cacheWritebackInvalidateRange (0x7c5dc253) */
	.word	sceKernelDcacheProbe /* sceKernelDcacheProbe (0x80001c4c) */
	.word	sceKernelUtilsSha1Digest /* sceKernelUtilsSha1Digest (0x840259f1) */
	.word	sceKernelPutUserLog /* sceKernelPutUserLog (0x87e81561) */
	.word	sceKernelLibcClock /* sceKernelLibcClock (0x91e4f6a7) */
	.word	sceKernelIcacheInvalidateAll /* sceKernelIcacheInvalidateAll (0x920f104a) */
	.word	sceKernelDcachePurgeRangeForUser /* sceKernelDcachePurgeRange (0x99134c3f) */
	.word	sceKernelUtilsMd5BlockInit /* sceKernelUtilsMd5BlockInit (0x9e5c5086) */
	.word	sceKernelL1DcacheWritebackInvalidateAll /* sceKernelL1DcacheWritebackInvalidateAll (0xaf3766bb) */
	.word	sceKernelDcacheWritebackInvalidateAll /* sceKernelDcacheWritebackInvalidateAll (0xb435dec5) */
	.word	sceKernelDcachePurgeRangeForUser /* sceKernelDcachePurgeRange (0xb83a1e76) */
	.word	sceKernelUtilsMd5BlockResult /* sceKernelUtilsMd5BlockResult (0xb8d24e78) */
	.word	sceKernelDcacheInvalidateRangeForUser /* sceKernelDcacheInvalidateRange (0xbfa98062) */
	.word	sceKernelIcacheInvalidateRangeForUser /* sceKernelIcacheInvalidateRange (0xc2df770e) */
	.word	sceKernelUtilsMd5Digest /* sceKernelUtilsMd5Digest (0xc8186a58) */
	.word	sceKernelL1DcacheProbeRange /* sceKernelL1DcacheProbeRange (0xdbbe9a46) */
	.word	sceKernelUtilsMt19937Init /* sceKernelUtilsMt19937Init (0xe860e75e) */
	.word	sceKernelL1IcacheProbe /* sceKernelL1IcacheProbe (0xf0155bca) */
	.word	sceKernelUtilsSha1BlockInit /* sceKernelUtilsSha1BlockInit (0xf8fcd5ba) */
	.word	sceKernelIcacheReadTagForUser /* sceKernelIcacheReadTag (0xfb05fad0) */
	.word	0x00030000
	.word	0x00070006
	.word	0x0010000d
	.word	0x00170013
	.word	0x0020001d
	.word	0x00250024
	.word	0x002b0029
	.word	0x002d002c
.L_exp_lib_8:
	.asciz	"UtilsForKernel"
	.space	1
.L_exp_tbl_8:
	.word	0x004d4dee
	.word	0x06fb8a63
	.word	0x0939d45c
	.word	0x136f2419
	.word	0x157a383a
	.word	0x16641d70
	.word	0x193d4036
	.word	0x1b0592a3
	.word	0x23a0c5ba
	.word	0x23ffc828
	.word	0x27cc57f0
	.word	0x2f80288e
	.word	0x346f6da8
	.word	0x34b9fa9e
	.word	0x37fb5c42
	.word	0x39f49610
	.word	0x39ffb756
	.word	0x3ee30821
	.word	0x3fd3d324
	.word	0x41887ef4
	.word	0x43c9a8db
	.word	0x485e430a
	.word	0x4fd31c9d
	.word	0x515b4faf
	.word	0x585f1c09
	.word	0x5c7f2b1a
	.word	0x601e43e6
	.word	0x61e1e525
	.word	0x6231a71d
	.word	0x66463bec
	.word	0x6ad345d7
	.word	0x6c6887ee
	.word	0x71ec4271
	.word	0x7333e539
	.word	0x740df7f0
	.word	0x77dff087
	.word	0x78934841
	.word	0x79d1c3fa
	.word	0x7c5dc253
	.word	0x80001c4c
	.word	0x840259f1
	.word	0x864a9d72
	.word	0x87e81561
	.word	0x8c1fbe04
	.word	0x91e4f6a7
	.word	0x920f104a
	.word	0x92282a47
	.word	0x95035fef
	.word	0x99134c3f
	.word	0x9e5c5086
	.word	0xa6b0a6b8
	.word	0xaa9af5cf
	.word	0xaf0c0173
	.word	0xaf3616c0
	.word	0xaf3766bb
	.word	0xb0e9c31f
	.word	0xb435dec5
	.word	0xb83a1e76
	.word	0xb8d24e78
	.word	0xbdbfca89
	.word	0xbfa98062
	.word	0xc2df770e
	.word	0xc8186a58
	.word	0xdbbe9a46
	.word	0xe0ce3e29
	.word	0xe0e6ba96
	.word	0xe860e75e
	.word	0xe8db3ce6
	.word	0xec86074d
	.word	0xf0155bca
	.word	0xf192f2ec
	.word	0xf8fcd5ba
	.word	0xfb05fad0
	.word	sceKernelL1DcacheWritebackRange /* sceKernelL1DcacheWritebackRange (0x004d4dee) */
	.word	sceKernelUtilsMt19937UInt /* sceKernelUtilsMt19937UInt (0x06fb8a63) */
	.word	sceKernelL2cacheWritebackAll /* sceKernelL2cacheWritebackAll (0x0939d45c) */
	.word	sceKernelSetPTRIGMask /* sceKernelSetPTRIGMask (0x136f2419) */
	.word	sceKernelL1DcacheInvalidateRange /* sceKernelL1DcacheInvalidateRange (0x157a383a) */
	.word	sceKernelDcacheReadTag /* sceKernelDcacheReadTag (0x16641d70) */
	.word	sceKernelSetGPIMask /* sceKernelSetGPIMask (0x193d4036) */
	.word	sceKernelL1DcacheWritebackInvalidateRange /* sceKernelL1DcacheWritebackInvalidateRange (0x1b0592a3) */
	.word	sceKernelRegisterRtcFunc /* sceKernelRegisterRtcFunc (0x23a0c5ba) */
	.word	sceKernelGzipGetCompressedData /* sceKernelGzipGetCompressedData (0x23ffc828) */
	.word	sceKernelLibcTime /* sceKernelLibcTime (0x27cc57f0) */
	.word	sceKernelL2cacheProbeRange /* sceKernelL2cacheProbeRange (0x2f80288e) */
	.word	sceKernelUtilsSha1BlockUpdate /* sceKernelUtilsSha1BlockUpdate (0x346f6da8) */
	.word	sceKernelDcacheWritebackInvalidateRange /* sceKernelDcacheWritebackInvalidateRange (0x34b9fa9e) */
	.word	sceKernelGetGPI /* sceKernelGetGPI (0x37fb5c42) */
	.word	sceKernelGetPTRIG /* sceKernelGetPTRIG (0x39f49610) */
	.word	UtilsForKernel_39FFB756 /* UtilsForKernel_39FFB756 (0x39ffb756) */
	.word	sceKernelDcacheWritebackRange /* sceKernelDcacheWritebackRange (0x3ee30821) */
	.word	sceKernelL1DcacheWritebackAll /* sceKernelL1DcacheWritebackAll (0x3fd3d324) */
	.word	sceKernelReleaseRtcFunc /* sceKernelReleaseRtcFunc (0x41887ef4) */
	.word	sceKernelL1IcacheInvalidateRange /* sceKernelL1IcacheInvalidateRange (0x43c9a8db) */
	.word	sceKernelL2cacheWritebackInvalidateAll /* sceKernelL2cacheWritebackInvalidateAll (0x485e430a) */
	.word	sceKernelIcacheProbe /* sceKernelIcacheProbe (0x4fd31c9d) */
	.word	sceKernelDcacheReadTag /* sceKernelDcacheReadTag (0x515b4faf) */
	.word	sceKernelUtilsSha1BlockResult /* sceKernelUtilsSha1BlockResult (0x585f1c09) */
	.word	sceKernelIcacheReadTag /* sceKernelIcacheReadTag (0x5c7f2b1a) */
	.word	sceKernelL2cacheWritebackRange /* sceKernelL2cacheWritebackRange (0x601e43e6) */
	.word	sceKernelUtilsMd5BlockUpdate /* sceKernelUtilsMd5BlockUpdate (0x61e1e525) */
	.word	sceKernelSetPTRIG /* sceKernelSetPTRIG (0x6231a71d) */
	.word	UtilsForKernel_66463BEC /* UtilsForKernel_66463BEC (0x66463bec) */
	.word	sceKernelSetGPO /* sceKernelSetGPO (0x6ad345d7) */
	.word	UtilsForKernel_6C6887EE /* UtilsForKernel_6C6887EE (0x6c6887ee) */
	.word	sceKernelLibcGettimeofday /* sceKernelLibcGettimeofday (0x71ec4271) */
	.word	sceKernelL1IcacheInvalidateAll /* sceKernelL1IcacheInvalidateAll (0x7333e539) */
	.word	sceKernelL1DcacheProbe /* sceKernelL1DcacheProbe (0x740df7f0) */
	.word	sceKernelDcacheProbeRange /* sceKernelDcacheProbeRange (0x77dff087) */
	.word	sceKernelGzipDecompress /* sceKernelGzipDecompress (0x78934841) */
	.word	sceKernelDcacheWritebackAll /* sceKernelDcacheWritebackAll (0x79d1c3fa) */
	.word	sceKernelL2cacheWritebackInvalidateRange /* sceKernelL2cacheWritebackInvalidateRange (0x7c5dc253) */
	.word	sceKernelDcacheProbe /* sceKernelDcacheProbe (0x80001c4c) */
	.word	sceKernelUtilsSha1Digest /* sceKernelUtilsSha1Digest (0x840259f1) */
	.word	sceKernelDcacheInvalidateAll /* sceKernelDcacheInvalidateAll (0x864a9d72) */
	.word	sceKernelPutUserLog /* sceKernelPutUserLog (0x87e81561) */
	.word	sceKernelGzipGetComment /* sceKernelGzipGetComment (0x8c1fbe04) */
	.word	sceKernelLibcClock /* sceKernelLibcClock (0x91e4f6a7) */
	.word	sceKernelIcacheInvalidateAll /* sceKernelIcacheInvalidateAll (0x920f104a) */
	.word	sceKernelRegisterUserLogHandler /* sceKernelRegisterUserLogHandler (0x92282a47) */
	.word	sceKernelSetGPOMask /* sceKernelSetGPOMask (0x95035fef) */
	.word	sceKernelDcachePurgeRange /* sceKernelDcachePurgeRange (0x99134c3f) */
	.word	sceKernelUtilsMd5BlockInit /* sceKernelUtilsMd5BlockInit (0x9e5c5086) */
	.word	UtilsForKernel_A6B0A6B8 /* UtilsForKernel_A6B0A6B8 (0xa6b0a6b8) */
	.word	sceKernelL1DcacheInvalidateAll /* sceKernelL1DcacheInvalidateAll (0xaa9af5cf) */
	.word	sceKernelL2cacheInvalidateAll /* sceKernelL2cacheInvalidateAll (0xaf0c0173) */
	.word	sceKernelGetGPO /* sceKernelGetGPO (0xaf3616c0) */
	.word	sceKernelL1DcacheWritebackInvalidateAll /* sceKernelL1DcacheWritebackInvalidateAll (0xaf3766bb) */
	.word	sceKernelGzipGetInfo /* sceKernelGzipGetInfo (0xb0e9c31f) */
	.word	sceKernelDcacheWritebackInvalidateAll /* sceKernelDcacheWritebackInvalidateAll (0xb435dec5) */
	.word	sceKernelDcachePurgeRange /* sceKernelDcachePurgeRange (0xb83a1e76) */
	.word	sceKernelUtilsMd5BlockResult /* sceKernelUtilsMd5BlockResult (0xb8d24e78) */
	.word	sceKernelRtcGetTick /* sceKernelRtcGetTick (0xbdbfca89) */
	.word	sceKernelDcacheInvalidateRange /* sceKernelDcacheInvalidateRange (0xbfa98062) */
	.word	sceKernelIcacheInvalidateRange /* sceKernelIcacheInvalidateRange (0xc2df770e) */
	.word	sceKernelUtilsMd5Digest /* sceKernelUtilsMd5Digest (0xc8186a58) */
	.word	sceKernelL1DcacheProbeRange /* sceKernelL1DcacheProbeRange (0xdbbe9a46) */
	.word	sceKernelGzipIsValid /* sceKernelGzipIsValid (0xe0ce3e29) */
	.word	sceKernelGzipGetName /* sceKernelGzipGetName (0xe0e6ba96) */
	.word	sceKernelUtilsMt19937Init /* sceKernelUtilsMt19937Init (0xe860e75e) */
	.word	sceKernelDeflateDecompress /* sceKernelDeflateDecompress (0xe8db3ce6) */
	.word	sceKernelL2cacheProbe /* sceKernelL2cacheProbe (0xec86074d) */
	.word	sceKernelL1IcacheProbe /* sceKernelL1IcacheProbe (0xf0155bca) */
	.word	sceKernelL2cacheInvalidateRange /* sceKernelL2cacheInvalidateRange (0xf192f2ec) */
	.word	sceKernelUtilsSha1BlockInit /* sceKernelUtilsSha1BlockInit (0xf8fcd5ba) */
	.word	sceKernelIcacheReadTag /* sceKernelIcacheReadTag (0xfb05fad0) */
	.word	0x00010000
	.word	0x00030002
	.word	0x00040003
	.word	0x00080006
	.word	0x000a0008
	.word	0x000b000b
	.word	0x000c000c
	.word	0x0011000f
	.word	0x00150013
	.word	0x00160015
	.word	0x00180017
	.word	0x00190018
	.word	0x001d001a
	.word	0x001f001e
	.word	0x00220020
	.word	0x00260024
	.word	0x00280027
	.word	0x002b002b
	.word	0x002f002c
	.word	0x00310030
	.word	0x00320032
	.word	0x00340033
	.word	0x00380037
	.word	0x003b0039
	.word	0x003e003d
	.word	0x003f003e
	.word	0x003f003f
	.word	0x0040003f
	.word	0x00420040
	.word	0x00440042
	.word	0x00470045
	.word	0x00490047
.L_exp_lib_9:
	.asciz	"KDebugForKernel"
	.align	2
.L_exp_tbl_9:
	.word	0x01ffe328
	.word	0x02668c61
	.word	0x0c56f87a
	.word	0x2214799b
	.word	0x276c02e3
	.word	0x27b23800
	.word	0x2c7d85fa
	.word	0x43f0f8ab
	.word	0x47570ac5
	.word	0x48be5aa9
	.word	0x4a74a9d4
	.word	0x568dcd25
	.word	0x604276eb
	.word	0x6cb0bda4
	.word	0x84f370bc
	.word	0x86010fcb
	.word	0x8a0f4e94
	.word	0xa1c5b42f
	.word	0xaa9b2f07
	.word	0xacf427dc
	.word	0xb20cabdb
	.word	0xb41e2430
	.word	0xd4ec38c1
	.word	0xd636b827
	.word	0xe201a0d8
	.word	0xe892d9a1
	.word	0xe8fe3ee3
	.word	0xeff672d1
	.word	0xf339073c
	.word	sceKernelRegisterDebugPutchar /* sceKernelRegisterDebugPutchar (0x01ffe328) */
	.word	sceKernelSm1RegisterOperations /* sceKernelSm1RegisterOperations (0x02668c61) */
	.word	sceKernelRegisterAssertHandler /* sceKernelRegisterAssertHandler (0x0c56f87a) */
	.word	sceKernelDebugWrite /* sceKernelDebugWrite (0x2214799b) */
	.word	sceKernelGetDebugPutchar /* sceKernelGetDebugPutchar (0x276c02e3) */
	.word	sceKernelIsUMDMode /* sceKernelIsUMDMode (0x27b23800) */
	.word	sceKernelCheckDebugHandler /* sceKernelCheckDebugHandler (0x2c7d85fa) */
	.word	sceKernelDipswLow32 /* sceKernelDipswLow32 (0x43f0f8ab) */
	.word	sceKernelIsToolMode /* sceKernelIsToolMode (0x47570ac5) */
	.word	sceKernelRegisterDebugRead /* sceKernelRegisterDebugRead (0x48be5aa9) */
	.word	sceKernelRegisterDebugWrite /* sceKernelRegisterDebugWrite (0x4a74a9d4) */
	.word	sceKernelDipswCpTime /* sceKernelDipswCpTime (0x568dcd25) */
	.word	sceKernelDeci2pRegisterOperations /* sceKernelDeci2pRegisterOperations (0x604276eb) */
	.word	sceKernelDipswHigh32 /* sceKernelDipswHigh32 (0x6cb0bda4) */
	.word	Kprintf /* Kprintf (0x84f370bc) */
	.word	sceKernelDipsw /* sceKernelDipsw (0x86010fcb) */
	.word	sceKernelRegisterDebugPutcharByBootloader /* sceKernelRegisterDebugPutcharByBootloader (0x8a0f4e94) */
	.word	sceKernelDebugEchoSet /* sceKernelDebugEchoSet (0xa1c5b42f) */
	.word	sceKernelAssert /* sceKernelAssert (0xaa9b2f07) */
	.word	sceKernelIsDevelopmentToolMode /* sceKernelIsDevelopmentToolMode (0xacf427dc) */
	.word	sceKernelDipswSet /* sceKernelDipswSet (0xb20cabdb) */
	.word	sceKernelIsDVDMode /* sceKernelIsDVDMode (0xb41e2430) */
	.word	sceKernelDebugRead /* sceKernelDebugRead (0xd4ec38c1) */
	.word	sceKernelDipswAll /* sceKernelDipswAll (0xd636b827) */
	.word	sceKernelRegisterKprintfHandler /* sceKernelRegisterKprintfHandler (0xe201a0d8) */
	.word	sceKernelSm1ReferOperations /* sceKernelSm1ReferOperations (0xe892d9a1) */
	.word	sceKernelDebugEcho /* sceKernelDebugEcho (0xe8fe3ee3) */
	.word	sceKernelDipswClear /* sceKernelDipswClear (0xeff672d1) */
	.word	sceKernelDeci2pReferOperations /* sceKernelDeci2pReferOperations (0xf339073c) */
	.word	0x00030000
	.word	0x00070003
	.word	0x000b0007
	.word	0x000e000c
	.word	0x0011000e
	.word	0x00140011
	.word	0x00160016
	.word	0x001c0018

	.section .rodata.sceNid, "a", @progbits
	.align	2

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
