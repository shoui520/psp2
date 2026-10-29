/* Generated PSP module import/export tables for sceModuleManager */
	.set	noreorder
	.set	noat

	.section .sceStub.text, "ax", @progbits
	.globl	sceKernelGetSyscallRA
	.type	sceKernelGetSyscallRA, @function
sceKernelGetSyscallRA: /* InterruptManagerForKernel 0xa0f88036 @ 0x8a68 */
	jr	$ra
	nop
	.globl	sceKernelIsIntrContext
	.type	sceKernelIsIntrContext, @function
sceKernelIsIntrContext: /* InterruptManagerForKernel 0xfe28c6d9 @ 0x8a70 */
	jr	$ra
	nop
	.globl	sceIoOpen
	.type	sceIoOpen, @function
sceIoOpen: /* IoFileMgrForKernel 0x109f50bc @ 0x8a78 */
	jr	$ra
	nop
	.globl	sceIoLseek
	.type	sceIoLseek, @function
sceIoLseek: /* IoFileMgrForKernel 0x27eb27b8 @ 0x8a80 */
	jr	$ra
	nop
	.globl	sceIoValidateFd
	.type	sceIoValidateFd, @function
sceIoValidateFd: /* IoFileMgrForKernel 0x2b6a9b21 @ 0x8a88 */
	jr	$ra
	nop
	.globl	sceIoIoctl
	.type	sceIoIoctl, @function
sceIoIoctl: /* IoFileMgrForKernel 0x63632449 @ 0x8a90 */
	jr	$ra
	nop
	.globl	sceIoRead
	.type	sceIoRead, @function
sceIoRead: /* IoFileMgrForKernel 0x6a638d83 @ 0x8a98 */
	jr	$ra
	nop
	.globl	sceIoClose
	.type	sceIoClose, @function
sceIoClose: /* IoFileMgrForKernel 0x810c4bc3 @ 0x8aa0 */
	jr	$ra
	nop
	.globl	sceKernelIsToolMode
	.type	sceKernelIsToolMode, @function
sceKernelIsToolMode: /* KDebugForKernel 0x47570ac5 @ 0x8aa8 */
	jr	$ra
	nop
	.globl	Kprintf
	.type	Kprintf, @function
Kprintf: /* KDebugForKernel 0x84f370bc @ 0x8ab0 */
	jr	$ra
	nop
	.globl	sceKernelDipsw
	.type	sceKernelDipsw, @function
sceKernelDipsw: /* KDebugForKernel 0x86010fcb @ 0x8ab8 */
	jr	$ra
	nop
	.globl	sceKernelIsDevelopmentToolMode
	.type	sceKernelIsDevelopmentToolMode, @function
sceKernelIsDevelopmentToolMode: /* KDebugForKernel 0xacf427dc @ 0x8ac0 */
	jr	$ra
	nop
	.globl	sceKernelDeleteModule
	.type	sceKernelDeleteModule, @function
sceKernelDeleteModule: /* LoadCoreForKernel 0x001b57bb @ 0x8ac8 */
	jr	$ra
	nop
	.globl	sceKernelUnlinkLibraryEntries
	.type	sceKernelUnlinkLibraryEntries, @function
sceKernelUnlinkLibraryEntries: /* LoadCoreForKernel 0x0295cfce @ 0x8ad0 */
	jr	$ra
	nop
	.globl	sceKernelLoadCoreLock
	.type	sceKernelLoadCoreLock, @function
sceKernelLoadCoreLock: /* LoadCoreForKernel 0x1999032f @ 0x8ad8 */
	jr	$ra
	nop
	.globl	sceKernelLoadExecutableObject
	.type	sceKernelLoadExecutableObject, @function
sceKernelLoadExecutableObject: /* LoadCoreForKernel 0x1c394885 @ 0x8ae0 */
	jr	$ra
	nop
	.globl	sceKernelCreateModule
	.type	sceKernelCreateModule, @function
sceKernelCreateModule: /* LoadCoreForKernel 0x2c44f793 @ 0x8ae8 */
	jr	$ra
	nop
	.globl	sceKernelRegisterLibraryForUser
	.type	sceKernelRegisterLibraryForUser, @function
sceKernelRegisterLibraryForUser: /* LoadCoreForKernel 0x2c60ccb8 @ 0x8af0 */
	jr	$ra
	nop
	.globl	sceKernelGetModuleIdListForKernel
	.type	sceKernelGetModuleIdListForKernel, @function
sceKernelGetModuleIdListForKernel: /* LoadCoreForKernel 0x37e6f41b @ 0x8af8 */
	jr	$ra
	nop
	.globl	sceKernelGetModuleListWithAlloc
	.type	sceKernelGetModuleListWithAlloc, @function
sceKernelGetModuleListWithAlloc: /* LoadCoreForKernel 0x3fe631f0 @ 0x8b00 */
	jr	$ra
	nop
	.globl	sceKernelFindModuleByUID
	.type	sceKernelFindModuleByUID, @function
sceKernelFindModuleByUID: /* LoadCoreForKernel 0x40972e6e @ 0x8b08 */
	jr	$ra
	nop
	.globl	sceKernelGetModuleGPByAddressForKernel
	.type	sceKernelGetModuleGPByAddressForKernel, @function
sceKernelGetModuleGPByAddressForKernel: /* LoadCoreForKernel 0x410084f9 @ 0x8b10 */
	jr	$ra
	nop
	.globl	sceKernelProbeExecutableObject
	.type	sceKernelProbeExecutableObject, @function
sceKernelProbeExecutableObject: /* LoadCoreForKernel 0x41d10899 @ 0x8b18 */
	jr	$ra
	nop
	.globl	sceKernelRegisterLibrary
	.type	sceKernelRegisterLibrary, @function
sceKernelRegisterLibrary: /* LoadCoreForKernel 0x48af96a9 @ 0x8b20 */
	jr	$ra
	nop
	.globl	sceKernelCanReleaseLibrary
	.type	sceKernelCanReleaseLibrary, @function
sceKernelCanReleaseLibrary: /* LoadCoreForKernel 0x538129f8 @ 0x8b28 */
	jr	$ra
	nop
	.globl	sceKernelSegmentChecksum
	.type	sceKernelSegmentChecksum, @function
sceKernelSegmentChecksum: /* LoadCoreForKernel 0x5fddb07a @ 0x8b30 */
	jr	$ra
	nop
	.globl	sceKernelLinkLibraryEntries
	.type	sceKernelLinkLibraryEntries, @function
sceKernelLinkLibraryEntries: /* LoadCoreForKernel 0x8eae9534 @ 0x8b38 */
	jr	$ra
	nop
	.globl	sceKernelLinkLibraryEntriesWithModule
	.type	sceKernelLinkLibraryEntriesWithModule, @function
sceKernelLinkLibraryEntriesWithModule: /* LoadCoreForKernel 0xa481e30e @ 0x8b40 */
	jr	$ra
	nop
	.globl	sceKernelReleaseModule
	.type	sceKernelReleaseModule, @function
sceKernelReleaseModule: /* LoadCoreForKernel 0xb17f5075 @ 0x8b48 */
	jr	$ra
	nop
	.globl	sceKernelLoadCoreUnlock
	.type	sceKernelLoadCoreUnlock, @function
sceKernelLoadCoreUnlock: /* LoadCoreForKernel 0xb6c037ea @ 0x8b50 */
	jr	$ra
	nop
	.globl	sceKernelFindModuleByAddress
	.type	sceKernelFindModuleByAddress, @function
sceKernelFindModuleByAddress: /* LoadCoreForKernel 0xbc99c625 @ 0x8b58 */
	jr	$ra
	nop
	.globl	sceKernelRegisterModule
	.type	sceKernelRegisterModule, @function
sceKernelRegisterModule: /* LoadCoreForKernel 0xbf2e388c @ 0x8b60 */
	jr	$ra
	nop
	.globl	sceKernelReleaseLibrary
	.type	sceKernelReleaseLibrary, @function
sceKernelReleaseLibrary: /* LoadCoreForKernel 0xcb636a90 @ 0x8b68 */
	jr	$ra
	nop
	.globl	sceKernelGetModuleFromUID
	.type	sceKernelGetModuleFromUID, @function
sceKernelGetModuleFromUID: /* LoadCoreForKernel 0xcd26e0ca @ 0x8b70 */
	jr	$ra
	nop
	.globl	sceKernelCheckExecFile
	.type	sceKernelCheckExecFile, @function
sceKernelCheckExecFile: /* LoadCoreForKernel 0xd3353ec4 @ 0x8b78 */
	jr	$ra
	nop
	.globl	sceKernelAssignModule
	.type	sceKernelAssignModule, @function
sceKernelAssignModule: /* LoadCoreForKernel 0xf3dd4808 @ 0x8b80 */
	jr	$ra
	nop
	.globl	sceKernelFindModuleByName
	.type	sceKernelFindModuleByName, @function
sceKernelFindModuleByName: /* LoadCoreForKernel 0xf6b1bf0f @ 0x8b88 */
	jr	$ra
	nop
	.globl	sceKernelMemset32
	.type	sceKernelMemset32, @function
sceKernelMemset32: /* SysMemForKernel 0x22a114dc @ 0x8b90 */
	jr	$ra
	nop
	.globl	sceKernelAllocPartitionMemory
	.type	sceKernelAllocPartitionMemory, @function
sceKernelAllocPartitionMemory: /* SysMemForKernel 0x7158ce7e @ 0x8b98 */
	jr	$ra
	nop
	.globl	sceKernelMemset
	.type	sceKernelMemset, @function
sceKernelMemset: /* SysMemForKernel 0x8ae776af @ 0x8ba0 */
	jr	$ra
	nop
	.globl	sceKernelMemmoveWithFill
	.type	sceKernelMemmoveWithFill, @function
sceKernelMemmoveWithFill: /* SysMemForKernel 0x9b20acef @ 0x8ba8 */
	jr	$ra
	nop
	.globl	sceKernelMemmove
	.type	sceKernelMemmove, @function
sceKernelMemmove: /* SysMemForKernel 0x9bac123d @ 0x8bb0 */
	jr	$ra
	nop
	.globl	sceKernelGetCompiledSdkVersion
	.type	sceKernelGetCompiledSdkVersion, @function
sceKernelGetCompiledSdkVersion: /* SysMemForKernel 0xb4f00cb5 @ 0x8bb8 */
	jr	$ra
	nop
	.globl	sceKernelFreePartitionMemory
	.type	sceKernelFreePartitionMemory, @function
sceKernelFreePartitionMemory: /* SysMemForKernel 0xc1a26c6f @ 0x8bc0 */
	jr	$ra
	nop
	.globl	sceKernelQueryMemoryPartitionInfo
	.type	sceKernelQueryMemoryPartitionInfo, @function
sceKernelQueryMemoryPartitionInfo: /* SysMemForKernel 0xc4eeaf20 @ 0x8bc8 */
	jr	$ra
	nop
	.globl	sceKernelGetId
	.type	sceKernelGetId, @function
sceKernelGetId: /* SysMemForKernel 0xd0c1460d @ 0x8bd0 */
	jr	$ra
	nop
	.globl	sceKernelQueryMemoryBlockInfo
	.type	sceKernelQueryMemoryBlockInfo, @function
sceKernelQueryMemoryBlockInfo: /* SysMemForKernel 0xe860be8f @ 0x8bd8 */
	jr	$ra
	nop
	.globl	sceKernelGetBlockHeadAddr
	.type	sceKernelGetBlockHeadAddr, @function
sceKernelGetBlockHeadAddr: /* SysMemForKernel 0xf12a62f7 @ 0x8be0 */
	jr	$ra
	nop
	.globl	sceKernelSeparateMemoryBlock
	.type	sceKernelSeparateMemoryBlock, @function
sceKernelSeparateMemoryBlock: /* SysMemForKernel 0xf7e78b33 @ 0x8be8 */
	jr	$ra
	nop
	.globl	sceKernelQueryMemoryInfo
	.type	sceKernelQueryMemoryInfo, @function
sceKernelQueryMemoryInfo: /* SysMemForKernel 0xfaf29f34 @ 0x8bf0 */
	jr	$ra
	nop
	.globl	sceKernelResizeMemoryBlock
	.type	sceKernelResizeMemoryBlock, @function
sceKernelResizeMemoryBlock: /* SysMemForKernel 0xfb5beb66 @ 0x8bf8 */
	jr	$ra
	nop
	.globl	memset
	.type	memset, @function
memset: /* SysclibForKernel 0x10f3bb61 @ 0x8c00 */
	jr	$ra
	nop
	.globl	strncmp
	.type	strncmp, @function
strncmp: /* SysclibForKernel 0x7ab35214 @ 0x8c08 */
	jr	$ra
	nop
	.globl	memcmp
	.type	memcmp, @function
memcmp: /* SysclibForKernel 0x81d0d1f7 @ 0x8c10 */
	jr	$ra
	nop
	.globl	memcpy
	.type	memcpy, @function
memcpy: /* SysclibForKernel 0xab7592ff @ 0x8c18 */
	jr	$ra
	nop
	.globl	strchr
	.type	strchr, @function
strchr: /* SysclibForKernel 0xb1dc2ae8 @ 0x8c20 */
	jr	$ra
	nop
	.globl	strncpy
	.type	strncpy, @function
strncpy: /* SysclibForKernel 0xb49a7697 @ 0x8c28 */
	jr	$ra
	nop
	.globl	sceKernelSetEventFlag
	.type	sceKernelSetEventFlag, @function
sceKernelSetEventFlag: /* ThreadManForKernel 0x1fb15a32 @ 0x8c30 */
	jr	$ra
	nop
	.globl	sceKernelWaitThreadEnd
	.type	sceKernelWaitThreadEnd, @function
sceKernelWaitThreadEnd: /* ThreadManForKernel 0x278c0df5 @ 0x8c38 */
	jr	$ra
	nop
	.globl	sceKernelGetThreadId
	.type	sceKernelGetThreadId, @function
sceKernelGetThreadId: /* ThreadManForKernel 0x293b45b8 @ 0x8c40 */
	jr	$ra
	nop
	.globl	sceKernelWaitEventFlag
	.type	sceKernelWaitEventFlag, @function
sceKernelWaitEventFlag: /* ThreadManForKernel 0x402fcf22 @ 0x8c48 */
	jr	$ra
	nop
	.globl	sceKernelCreateThread
	.type	sceKernelCreateThread, @function
sceKernelCreateThread: /* ThreadManForKernel 0x446d8de6 @ 0x8c50 */
	jr	$ra
	nop
	.globl	sceKernelCreateEventFlag
	.type	sceKernelCreateEventFlag, @function
sceKernelCreateEventFlag: /* ThreadManForKernel 0x55c20a00 @ 0x8c58 */
	jr	$ra
	nop
	.globl	sceKernelUnlockMutex
	.type	sceKernelUnlockMutex, @function
sceKernelUnlockMutex: /* ThreadManForKernel 0x6b30100f @ 0x8c60 */
	jr	$ra
	nop
	.globl	sceKernelChangeThreadPriority
	.type	sceKernelChangeThreadPriority, @function
sceKernelChangeThreadPriority: /* ThreadManForKernel 0x71bc9871 @ 0x8c68 */
	jr	$ra
	nop
	.globl	sceKernelExitDeleteThread
	.type	sceKernelExitDeleteThread, @function
sceKernelExitDeleteThread: /* ThreadManForKernel 0x809ce29b @ 0x8c70 */
	jr	$ra
	nop
	.globl	sceKernelSuspendThread
	.type	sceKernelSuspendThread, @function
sceKernelSuspendThread: /* ThreadManForKernel 0x9944f31f @ 0x8c78 */
	jr	$ra
	nop
	.globl	sceKernelDeleteThread
	.type	sceKernelDeleteThread, @function
sceKernelDeleteThread: /* ThreadManForKernel 0x9fa03cd3 @ 0x8c80 */
	jr	$ra
	nop
	.globl	sceKernelLockMutex
	.type	sceKernelLockMutex, @function
sceKernelLockMutex: /* ThreadManForKernel 0xb011b11f @ 0x8c88 */
	jr	$ra
	nop
	.globl	sceKernelCreateMutex
	.type	sceKernelCreateMutex, @function
sceKernelCreateMutex: /* ThreadManForKernel 0xb7d098c6 @ 0x8c90 */
	jr	$ra
	nop
	.globl	sceKernelStartThread
	.type	sceKernelStartThread, @function
sceKernelStartThread: /* ThreadManForKernel 0xf475845d @ 0x8c98 */
	jr	$ra
	nop
	.globl	sceKernelGetUserLevel
	.type	sceKernelGetUserLevel, @function
sceKernelGetUserLevel: /* ThreadManForKernel 0xf6427665 @ 0x8ca0 */
	jr	$ra
	nop
	.globl	sceKernelDcacheWritebackAll
	.type	sceKernelDcacheWritebackAll, @function
sceKernelDcacheWritebackAll: /* UtilsForKernel 0x79d1c3fa @ 0x8ca8 */
	jr	$ra
	nop
	.globl	sceKernelIcacheInvalidateAll
	.type	sceKernelIcacheInvalidateAll, @function
sceKernelIcacheInvalidateAll: /* UtilsForKernel 0x920f104a @ 0x8cb0 */
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
	.half	22
	.word	.L_exp_tbl_1
	.word	0x00020000
	.word	.L_exp_lib_2
	.byte	17, 0
	.half	0x0009
	.byte	5, 3
	.half	51
	.word	.L_exp_tbl_2
	.word	0x00020000
	.word	.L_exp_lib_3
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	16
	.word	.L_exp_tbl_3
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
	.word	sceKernelIsToolMode
	.word	.L_imp_lib_1
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	14
	.word	.L_imp_nid_1
	.word	sceKernelMemset32
	.word	.L_imp_lib_2
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	25
	.word	.L_imp_nid_2
	.word	sceKernelDeleteModule
	.word	.L_imp_lib_3
	.byte	17, 0
	.half	0x0009
	.byte	6, 0
	.half	2
	.word	.L_imp_nid_3
	.word	sceKernelGetSyscallRA
	.word	0
	.word	.L_imp_lib_4
	.byte	17, 0
	.half	0x0001
	.byte	5, 0
	.half	6
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
	.half	0x0001
	.byte	6, 0
	.half	6
	.word	.L_imp_nid_6
	.word	sceIoOpen
	.word	0
	.word	.L_imp_lib_7
	.byte	17, 0
	.half	0x0009
	.byte	5, 0
	.half	2
	.word	.L_imp_nid_7
	.word	sceKernelDcacheWritebackAll

	.section .lib.stub.btm, "a", @progbits
	.align	2
__lib_stub_btm:
	.word	0

	.section .rodata.sceModuleInfo, "a", @progbits
	.align	4
	.globl	module_info
	.type	module_info, @object
module_info:
	.half	0x3007
	.byte	18, 1
	.asciz	"sceModuleManager"
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
	.asciz	"KDebugForKernel"
	.space	4
.L_imp_lib_1:
	.asciz	"SysMemForKernel"
	.space	4
.L_imp_lib_2:
	.asciz	"LoadCoreForKernel"
	.space	6
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
	.asciz	"IoFileMgrForKernel"
	.space	5
.L_imp_lib_7:
	.asciz	"UtilsForKernel"
	.space	1
.L_exp_tbl_0:
	.word	0xd3744be0
	.word	0x2f064fa6
	.word	0xadf12745
	.word	0xf01d73a7
	.word	0x11b97506
	.word	module_bootstart /* module_bootstart (0xd3744be0) */
	.word	module_reboot_before /* module_reboot_before (0x2f064fa6) */
	.word	module_reboot_phase /* module_reboot_phase (0xadf12745) */
	.word	module_info /* module_info (0xf01d73a7) */
	.word	module_sdk_version /* module_sdk_version (0x11b97506) */
.L_exp_lib_1:
	.asciz	"ModuleMgrForUser"
	.space	3
.L_exp_tbl_1:
	.word	0x1196472e
	.word	0x24ec0641
	.word	0x2e0911aa
	.word	0x50f0c1ec
	.word	0x644395e2
	.word	0x710f61b5
	.word	0x748cbed9
	.word	0x8f2df740
	.word	0x977de386
	.word	0xb7f46618
	.word	0xcc1d3699
	.word	0xcde1c1fe
	.word	0xd1ff982a
	.word	0xd2fbc957
	.word	0xd675ebb8
	.word	0xd8b73127
	.word	0xe4c4211c
	.word	0xf0a26395
	.word	0xf2d8d1b4
	.word	0xf9275d98
	.word	0xfbe27467
	.word	0xfef27dc1
	.word	sceKernelLoadModuleBufferMs /* sceKernelLoadModuleBufferMs (0x1196472e) */
	.word	sceKernelLoadModuleBufferApp /* sceKernelLoadModuleBufferApp (0x24ec0641) */
	.word	sceKernelUnloadModule /* sceKernelUnloadModule (0x2e0911aa) */
	.word	sceKernelStartModule /* sceKernelStartModule (0x50f0c1ec) */
	.word	sceKernelGetModuleIdList /* sceKernelGetModuleIdList (0x644395e2) */
	.word	sceKernelLoadModuleMs /* sceKernelLoadModuleMs (0x710f61b5) */
	.word	sceKernelQueryModuleInfo /* sceKernelQueryModuleInfo (0x748cbed9) */
	.word	sceKernelStopUnloadSelfModuleWithStatus /* sceKernelStopUnloadSelfModule (0x8f2df740) */
	.word	sceKernelLoadModule /* sceKernelLoadModule (0x977de386) */
	.word	sceKernelLoadModuleByID /* sceKernelLoadModuleByID (0xb7f46618) */
	.word	sceKernelStopUnloadSelfModule /* sceKernelStopUnloadSelfModule (0xcc1d3699) */
	.word	sceKernelCheckTextSegment /* sceKernelCheckTextSegment (0xcde1c1fe) */
	.word	sceKernelStopModule /* sceKernelStopModule (0xd1ff982a) */
	.word	sceKernelGetModuleGPByAddress /* sceKernelGetModuleGPByAddress (0xd2fbc957) */
	.word	sceKernelSelfStopUnloadModule /* sceKernelSelfStopUnloadModule (0xd675ebb8) */
	.word	sceKernelGetModuleIdByAddress /* sceKernelGetModuleIdByAddress (0xd8b73127) */
	.word	sceKernelLoadModuleWithBlockOffset /* sceKernelLoadModuleWithBlockOffset (0xe4c4211c) */
	.word	sceKernelGetModuleId /* sceKernelGetModuleId (0xf0a26395) */
	.word	sceKernelLoadModuleNpDrm /* sceKernelLoadModuleNpDrm (0xf2d8d1b4) */
	.word	sceKernelLoadModuleBufferUsbWlan /* sceKernelLoadModuleBufferUsbWlan (0xf9275d98) */
	.word	sceKernelLoadModuleByIDWithBlockOffset /* sceKernelLoadModuleByIDWithBlockOffset (0xfbe27467) */
	.word	sceKernelLoadModuleDNAS /* sceKernelLoadModuleDNAS (0xfef27dc1) */
	.word	0x00000000
	.word	0x00030001
	.word	0x00030003
	.word	0x00050004
	.word	0x00080007
	.word	0x00090009
	.word	0x000c000a
	.word	0x00110010
.L_exp_lib_2:
	.asciz	"ModuleMgrForKernel"
	.space	1
.L_exp_tbl_2:
	.word	0x12f99392
	.word	0x1cf0b794
	.word	0x1cffc5de
	.word	0x22bdbeff
	.word	0x253aa17c
	.word	0x2b7fc10d
	.word	0x2c4f270d
	.word	0x2f3f9b6a
	.word	0x303fab7f
	.word	0x30727524
	.word	0x387e3ca9
	.word	0x3ff74df1
	.word	0x433d5287
	.word	0x4493e013
	.word	0x4e38ea1d
	.word	0x4e62c48a
	.word	0x4e8a2c9d
	.word	0x5805c1ca
	.word	0x5fc32087
	.word	0x5fc3b3da
	.word	0x61e3ec69
	.word	0x76f0e956
	.word	0x7bd53193
	.word	0x853a6c16
	.word	0x8dd336d4
	.word	0x9236b422
	.word	0x939e4270
	.word	0x955d6cb2
	.word	0x9b7102e2
	.word	0xa40ec254
	.word	0xc13e2de5
	.word	0xc2a5e6ca
	.word	0xc3ddabef
	.word	0xc6de0b9c
	.word	0xcab06d30
	.word	0xcae8e169
	.word	0xcba02988
	.word	0xcc873dfa
	.word	0xce0a74a5
	.word	0xd4ee2d26
	.word	0xd5ddab1f
	.word	0xd60ab6cc
	.word	0xd86dd11b
	.word	0xe5d6087b
	.word	0xe8422026
	.word	0xe8b9d19d
	.word	0xe97e0db7
	.word	0xee6e8f49
	.word	0xeec2a745
	.word	0xf7c7febc
	.word	0xfe61f16d
	.word	0x57f4b35e
	.word	0x99bffdb8
	.word	0xdb08b7ff
	.word	sceKernelSearchModuleByAddress /* sceKernelSearchModuleByAddress (0x12f99392) */
	.word	sceKernelLoadModuleBufferBootInitBtcnf /* sceKernelLoadModuleBufferBootInitBtcnf (0x1cf0b794) */
	.word	sceKernelModuleMgrMode /* sceKernelModuleMgrMode (0x1cffc5de) */
	.word	sceKernelQueryModuleInfo /* sceKernelQueryModuleInfo (0x22bdbeff) */
	.word	sceKernelLoadModuleBufferForExitVSHKernel /* sceKernelLoadModuleBufferForExitVSHKernel (0x253aa17c) */
	.word	sceKernelLoadModuleForLoadExecForUser /* sceKernelLoadModuleForLoadExecForUser (0x2b7fc10d) */
	.word	sceKernelLoadModuleForLoadExecVSHDiscDebug /* sceKernelLoadModuleForLoadExecVSHDiscDebug (0x2c4f270d) */
	.word	sceKernelLoadModuleBufferForLoadExecBufferVSHUsbWlan /* sceKernelLoadModuleBufferForLoadExecBufferVSHUsbWlan (0x2f3f9b6a) */
	.word	sceKernelGetModuleIdList /* sceKernelGetModuleIdList (0x303fab7f) */
	.word	sceKernelLoadModuleForLoadExecNpDrm /* sceKernelLoadModuleForLoadExecNpDrm (0x30727524) */
	.word	sceKernelUnloadModule /* sceKernelUnloadModule (0x387e3ca9) */
	.word	sceKernelStartModule /* sceKernelStartModule (0x3ff74df1) */
	.word	sceKernelGetModuleIdByAddress /* sceKernelGetModuleIdByAddress (0x433d5287) */
	.word	sceKernelLoadModuleDeci /* sceKernelLoadModuleDeci (0x4493e013) */
	.word	sceKernelLoadModuleBufferForRebootKernel /* sceKernelLoadModuleBufferForRebootKernel (0x4e38ea1d) */
	.word	sceKernelLoadModuleBufferForKernel /* sceKernelLoadModuleBufferForKernel (0x4e62c48a) */
	.word	sceKernelLoadModuleForLoadExecVSHMs5 /* sceKernelLoadModuleForLoadExecVSHMs5 (0x4e8a2c9d) */
	.word	sceKernelSelfStopUnloadModule /* sceKernelSelfStopUnloadModule (0x5805c1ca) */
	.word	sceKernelLoadModuleByIDBootInitConfig /* sceKernelLoadModuleByIDBootInitConfig (0x5fc32087) */
	.word	sceKernelRebootBeforeForKernel /* sceKernelRebootBeforeForKernel (0x5fc3b3da) */
	.word	sceKernelLoadModuleBufferForExitGame /* sceKernelLoadModuleBufferForExitGame (0x61e3ec69) */
	.word	sceKernelLoadModuleForLoadExecVSHMs4 /* sceKernelLoadModuleForLoadExecVSHMs4 (0x76f0e956) */
	.word	sceKernelLoadModuleForLoadExecVSHMs2 /* sceKernelLoadModuleForLoadExecVSHMs2 (0x7bd53193) */
	.word	sceKernelLoadModuleForLoadExecVSHDiscEmu /* sceKernelLoadModuleForLoadExecVSHDiscEmu (0x853a6c16) */
	.word	ModuleMgrForKernel_8DD336D4 /* ModuleMgrForKernel_8DD336D4 (0x8dd336d4) */
	.word	sceKernelLoadModuleBufferForExitVSHVSH /* sceKernelLoadModuleBufferForExitVSHVSH (0x9236b422) */
	.word	sceKernelLoadModuleForKernel /* sceKernelLoadModuleForKernel (0x939e4270) */
	.word	sceKernelLoadModuleBootInitBtcnf /* sceKernelLoadModuleBootInitBtcnf (0x955d6cb2) */
	.word	sceKernelRebootPhaseForKernel /* sceKernelRebootPhaseForKernel (0x9b7102e2) */
	.word	sceKernelSetNpDrmGetModuleKeyFunction /* sceKernelSetNpDrmGetModuleKeyFunction (0xa40ec254) */
	.word	sceKernelLoadModuleBufferForLoadExecBufferVSHUsbWlanDebug /* sceKernelLoadModuleBufferForLoadExecBufferVSHUsbWlanDebug (0xc13e2de5) */
	.word	ModuleMgrForKernel_C2A5E6CA /* ModuleMgrForKernel_C2A5E6CA (0xc2a5e6ca) */
	.word	sceKernelNpDrmGetModuleKey /* sceKernelNpDrmGetModuleKey (0xc3ddabef) */
	.word	sceKernelLoadModuleBufferVSH /* sceKernelLoadModuleBufferVSH (0xc6de0b9c) */
	.word	sceKernelGetModuleId /* sceKernelGetModuleId (0xcab06d30) */
	.word	sceKernelLoadModuleForLoadExecVSHDiscUpdater /* sceKernelLoadModuleForLoadExecVSHDiscUpdater (0xcae8e169) */
	.word	sceKernelLoadModuleVSHByID /* sceKernelLoadModuleVSHByID (0xcba02988) */
	.word	sceKernelRebootBeforeForUser /* sceKernelRebootBeforeForUser (0xcc873dfa) */
	.word	sceKernelLoadModuleForLoadExecVSHDisc /* sceKernelLoadModuleForLoadExecVSHDisc (0xce0a74a5) */
	.word	sceKernelLoadModuleToBlock /* sceKernelLoadModuleToBlock (0xd4ee2d26) */
	.word	sceKernelLoadModuleVSH /* sceKernelLoadModuleVSH (0xd5ddab1f) */
	.word	sceKernelLoadModuleForLoadExecVSHMs3 /* sceKernelLoadModuleForLoadExecVSHMs3 (0xd60ab6cc) */
	.word	sceKernelSearchModuleByName /* sceKernelSearchModuleByName (0xd86dd11b) */
	.word	sceKernelStopModule /* sceKernelStopModule (0xe5d6087b) */
	.word	sceKernelLoadModuleForLoadExecVSHMs6 /* sceKernelLoadModuleForLoadExecVSHMs6 (0xe8422026) */
	.word	sceKernelLoadModuleBufferBootInitConfig /* sceKernelLoadModuleBufferBootInitConfig (0xe8b9d19d) */
	.word	sceKernelStopUnloadSelfModule /* sceKernelStopUnloadSelfModule (0xe97e0db7) */
	.word	sceKernelStopUnloadSelfModuleWithStatus /* sceKernelStopUnloadSelfModuleWithStatus (0xee6e8f49) */
	.word	sceKernelLoadModuleByIDForKernel /* sceKernelLoadModuleByID (0xeec2a745) */
	.word	sceKernelLoadModuleBootInitConfig /* sceKernelLoadModuleBootInitConfig (0xf7c7febc) */
	.word	sceKernelLoadModuleForLoadExecVSHMs1 /* sceKernelLoadModuleForLoadExecVSHMs1 (0xfe61f16d) */
	.word	ModuleMgrForKernel_var_57F4B35E /* ModuleMgrForKernel_var_57F4B35E (0x57f4b35e) */
	.word	ModuleMgrForKernel_var_99BFFDB8 /* ModuleMgrForKernel_var_99BFFDB8 (0x99bffdb8) */
	.word	ModuleMgrForKernel_var_DB08B7FF /* ModuleMgrForKernel_var_DB08B7FF (0xdb08b7ff) */
	.word	0x00000000
	.word	0x00080003
	.word	0x0011000c
	.word	0x00150014
	.word	0x00190017
	.word	0x001e001d
	.word	0x0027001e
	.word	0x0031002b
.L_exp_lib_3:
	.asciz	"InitForKernel"
	.space	2
.L_exp_tbl_3:
	.word	0x040c934b
	.word	0x1d3256ba
	.word	0x27932388
	.word	0x2c6e9fe9
	.word	0x3acccd7e
	.word	0x5238f4cc
	.word	0x7233b5bc
	.word	0x7a2333ad
	.word	0x9d33a110
	.word	0x9f9ae99c
	.word	0xa18a4a8b
	.word	0xa6e71b93
	.word	0xcb837ac5
	.word	0xce88e870
	.word	0xd83a9bd7
	.word	0xe444cffc
	.word	sceKernelQueryInitCB /* sceKernelQueryInitCB (0x040c934b) */
	.word	sceKernelRegisterChunk /* sceKernelRegisterChunk (0x1d3256ba) */
	.word	sceKernelBootFrom /* sceKernelBootFrom (0x27932388) */
	.word	sceKernelGetChunk /* sceKernelGetChunk (0x2c6e9fe9) */
	.word	sceKernelShowIntrHandlerInfo /* sceKernelShowIntrHandlerInfo (0x3acccd7e) */
	.word	sceKernelInitLptSummary /* sceKernelInitLptSummary (0x5238f4cc) */
	.word	sceKernelApplicationType /* sceKernelApplicationType (0x7233b5bc) */
	.word	sceKernelInitApitype /* sceKernelInitApitype (0x7a2333ad) */
	.word	InitForKernel_9D33A110 /* InitForKernel_9D33A110 (0x9d33a110) */
	.word	sceKernelSetInitCallback /* sceKernelSetInitCallback (0x9f9ae99c) */
	.word	sceKernelInitDiscImage /* sceKernelInitDiscImage (0xa18a4a8b) */
	.word	sceKernelInitFileName /* sceKernelInitFileName (0xa6e71b93) */
	.word	sceKernelShowIntrMaskTime /* sceKernelShowIntrMaskTime (0xcb837ac5) */
	.word	sceKernelReleaseChunk /* sceKernelReleaseChunk (0xce88e870) */
	.word	sceKernelInitParamSfo /* sceKernelInitParamSfo (0xd83a9bd7) */
	.word	sceKernelStartIntrLogging /* sceKernelStartIntrLogging (0xe444cffc) */
	.word	0x00010000
	.word	0x00040002
	.word	0x00050005
	.word	0x00060006
	.word	0x00080008
	.word	0x000c000a
	.word	0x000e000c
	.word	0x0010000f

	.section .rodata.sceNid, "a", @progbits
	.align	2
.L_imp_nid_0:
	.word	0x47570ac5
	.word	0x84f370bc
	.word	0x86010fcb
	.word	0xacf427dc
.L_imp_nid_1:
	.word	0x22a114dc
	.word	0x7158ce7e
	.word	0x8ae776af
	.word	0x9b20acef
	.word	0x9bac123d
	.word	0xb4f00cb5
	.word	0xc1a26c6f
	.word	0xc4eeaf20
	.word	0xd0c1460d
	.word	0xe860be8f
	.word	0xf12a62f7
	.word	0xf7e78b33
	.word	0xfaf29f34
	.word	0xfb5beb66
.L_imp_nid_2:
	.word	0x001b57bb
	.word	0x0295cfce
	.word	0x1999032f
	.word	0x1c394885
	.word	0x2c44f793
	.word	0x2c60ccb8
	.word	0x37e6f41b
	.word	0x3fe631f0
	.word	0x40972e6e
	.word	0x410084f9
	.word	0x41d10899
	.word	0x48af96a9
	.word	0x538129f8
	.word	0x5fddb07a
	.word	0x8eae9534
	.word	0xa481e30e
	.word	0xb17f5075
	.word	0xb6c037ea
	.word	0xbc99c625
	.word	0xbf2e388c
	.word	0xcb636a90
	.word	0xcd26e0ca
	.word	0xd3353ec4
	.word	0xf3dd4808
	.word	0xf6b1bf0f
.L_imp_nid_3:
	.word	0xa0f88036
	.word	0xfe28c6d9
.L_imp_nid_4:
	.word	0x10f3bb61
	.word	0x7ab35214
	.word	0x81d0d1f7
	.word	0xab7592ff
	.word	0xb1dc2ae8
	.word	0xb49a7697
.L_imp_nid_5:
	.word	0x1fb15a32
	.word	0x278c0df5
	.word	0x293b45b8
	.word	0x402fcf22
	.word	0x446d8de6
	.word	0x55c20a00
	.word	0x6b30100f
	.word	0x71bc9871
	.word	0x809ce29b
	.word	0x9944f31f
	.word	0x9fa03cd3
	.word	0xb011b11f
	.word	0xb7d098c6
	.word	0xf475845d
	.word	0xf6427665
.L_imp_nid_6:
	.word	0x109f50bc
	.word	0x27eb27b8
	.word	0x2b6a9b21
	.word	0x63632449
	.word	0x6a638d83
	.word	0x810c4bc3
.L_imp_nid_7:
	.word	0x79d1c3fa
	.word	0x920f104a

	.section .rodata.sceSdkVersion, "a", @progbits
	.align	2
	.globl	module_sdk_version
	.type	module_sdk_version, @object
module_sdk_version:
	.word	0x06060110
