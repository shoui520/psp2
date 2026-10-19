/* Exact canonical function-import bindings; records are loader metadata. */
.syntax unified
.arm
#if FW_VERSION >= 0x04000000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelGetThreadCurrentPriority
.type sceKernelGetThreadCurrentPriority, %function
sceKernelGetThreadCurrentPriority:
.word 0x10, 0xe2c40624, 0x1414f0b, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelSetThreadAccessLevel
.type sceKernelSetThreadAccessLevel, %function
sceKernelSetThreadAccessLevel:
.word 0x10, 0xe2c40624, 0x2eedf17, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global SceThreadmgrForDriver_0486F239
.type SceThreadmgrForDriver_0486F239, %function
SceThreadmgrForDriver_0486F239:
.word 0x10, 0xe2c40624, 0x486f239, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelDeleteMutex
.type sceKernelDeleteMutex, %function
sceKernelDeleteMutex:
.word 0x10, 0xe2c40624, 0xa912340, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelWaitEventFlag
.type sceKernelWaitEventFlag, %function
sceKernelWaitEventFlag:
.word 0x10, 0xe2c40624, 0xc1d3f20, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelGetEventPattern
.type sceKernelGetEventPattern, %function
sceKernelGetEventPattern:
.word 0x10, 0xe2c40624, 0xc6bcb8c, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global SceThreadmgrForDriver_1096042E
.type SceThreadmgrForDriver_1096042E, %function
SceThreadmgrForDriver_1096042E:
.word 0x10, 0xe2c40624, 0x1096042e, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelFinalizeFastMutex
.type sceKernelFinalizeFastMutex, %function
sceKernelFinalizeFastMutex:
.word 0x10, 0xe2c40624, 0x11fe84a1, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelLockMutex
.type sceKernelLockMutex, %function
sceKernelLockMutex:
.word 0x10, 0xe2c40624, 0x16ac80c5, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelUnlockMutex
.type sceKernelUnlockMutex, %function
sceKernelUnlockMutex:
.word 0x10, 0xe2c40624, 0x1e82e5d0, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelStartThread
.type sceKernelStartThread, %function
sceKernelStartThread:
.word 0x10, 0xe2c40624, 0x21f5419b, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelEventObjectInit
.type sceKernelEventObjectInit, %function
sceKernelEventObjectInit:
.word 0x10, 0xe2c40624, 0x25ab8f6b, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelTryLockMutex
.type sceKernelTryLockMutex, %function
sceKernelTryLockMutex:
.word 0x10, 0xe2c40624, 0x270993a6, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global SceThreadmgrForDriver_332E127C
.type SceThreadmgrForDriver_332E127C, %function
SceThreadmgrForDriver_332E127C:
.word 0x10, 0xe2c40624, 0x332e127c, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelChangeThreadPriorityAndCpuAffinityMask
.type sceKernelChangeThreadPriorityAndCpuAffinityMask, %function
sceKernelChangeThreadPriorityAndCpuAffinityMask:
.word 0x10, 0xe2c40624, 0x3857c94e, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelWaitThreadEnd
.type sceKernelWaitThreadEnd, %function
sceKernelWaitThreadEnd:
.word 0x10, 0xe2c40624, 0x3e20216f, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelCreateEventFlag
.type sceKernelCreateEventFlag, %function
sceKernelCreateEventFlag:
.word 0x10, 0xe2c40624, 0x4336baa4, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelCancelAlarm
.type sceKernelCancelAlarm, %function
sceKernelCancelAlarm:
.word 0x10, 0xe2c40624, 0x44ccf310, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelDelayThread
.type sceKernelDelayThread, %function
sceKernelDelayThread:
.word 0x10, 0xe2c40624, 0x4b675d05, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelClearEventFlag
.type sceKernelClearEventFlag, %function
sceKernelClearEventFlag:
.word 0x10, 0xe2c40624, 0x4f1da3be, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelGetThreadId
.type sceKernelGetThreadId, %function
sceKernelGetThreadId:
.word 0x10, 0xe2c40624, 0x59d06540, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelSignalCondTo
.type sceKernelSignalCondTo, %function
sceKernelSignalCondTo:
.word 0x10, 0xe2c40624, 0x61533da9, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelGetMutexInfo
.type sceKernelGetMutexInfo, %function
sceKernelGetMutexInfo:
.word 0x10, 0xe2c40624, 0x69b78a12, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelSignalCondAll
.type sceKernelSignalCondAll, %function
sceKernelSignalCondAll:
.word 0x10, 0xe2c40624, 0x6ec78cd0, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global lockFastMutex
.type lockFastMutex, %function
lockFastMutex:
.word 0x10, 0xe2c40624, 0x70627f3a, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelDeleteEventFlag
.type sceKernelDeleteEventFlag, %function
sceKernelDeleteEventFlag:
.word 0x10, 0xe2c40624, 0x71ecb352, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelTryLockFastMutex
.type sceKernelTryLockFastMutex, %function
sceKernelTryLockFastMutex:
.word 0x10, 0xe2c40624, 0x741f4707, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelGetThreadCpuAffinityMask
.type sceKernelGetThreadCpuAffinityMask, %function
sceKernelGetThreadCpuAffinityMask:
.word 0x10, 0xe2c40624, 0x83dc703d, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global SceThreadmgrForDriver_89CA5698
.type SceThreadmgrForDriver_89CA5698, %function
SceThreadmgrForDriver_89CA5698:
.word 0x10, 0xe2c40624, 0x89ca5698, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelGetProcessId
.type sceKernelGetProcessId, %function
sceKernelGetProcessId:
.word 0x10, 0xe2c40624, 0x9dcb4b7a, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelSetEvent
.type sceKernelSetEvent, %function
sceKernelSetEvent:
.word 0x10, 0xe2c40624, 0x9ea3a45c, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelExtendKernelStackWide
.type sceKernelExtendKernelStackWide, %function
sceKernelExtendKernelStackWide:
.word 0x10, 0xe2c40624, 0xa2c801a5, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelDeleteThread
.type sceKernelDeleteThread, %function
sceKernelDeleteThread:
.word 0x10, 0xe2c40624, 0xac834f3f, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelDeleteCond
.type sceKernelDeleteCond, %function
sceKernelDeleteCond:
.word 0x10, 0xe2c40624, 0xaee0d27c, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelInitializeFastMutex
.type sceKernelInitializeFastMutex, %function
sceKernelInitializeFastMutex:
.word 0x10, 0xe2c40624, 0xaf8e1266, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global SceThreadmgrForDriver_B645C7EF
.type SceThreadmgrForDriver_B645C7EF, %function
SceThreadmgrForDriver_B645C7EF:
.word 0x10, 0xe2c40624, 0xb645c7ef, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelSetEventWithNotifyCallback
.type sceKernelSetEventWithNotifyCallback, %function
sceKernelSetEventWithNotifyCallback:
.word 0x10, 0xe2c40624, 0xb7df3edf, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global SceThreadmgrForDriver_C58DF384
.type SceThreadmgrForDriver_C58DF384, %function
SceThreadmgrForDriver_C58DF384:
.word 0x10, 0xe2c40624, 0xc58df384, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelCreateThread
.type sceKernelCreateThread, %function
sceKernelCreateThread:
.word 0x10, 0xe2c40624, 0xc6674e7d, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelWaitCond
.type sceKernelWaitCond, %function
sceKernelWaitCond:
.word 0x10, 0xe2c40624, 0xcc7e027d, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelSetEventFlag
.type sceKernelSetEventFlag, %function
sceKernelSetEventFlag:
.word 0x10, 0xe2c40624, 0xd4780c3e, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelGetFastMutexInfo
.type sceKernelGetFastMutexInfo, %function
sceKernelGetFastMutexInfo:
.word 0x10, 0xe2c40624, 0xd7af2e58, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelUnlockFastMutex
.type sceKernelUnlockFastMutex, %function
sceKernelUnlockFastMutex:
.word 0x10, 0xe2c40624, 0xdb395782, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelCreateCond
.type sceKernelCreateCond, %function
sceKernelCreateCond:
.word 0x10, 0xe2c40624, 0xdb6cd34a, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelUIDInheritedEventClassInit
.type sceKernelUIDInheritedEventClassInit, %function
sceKernelUIDInheritedEventClassInit:
.word 0x10, 0xe2c40624, 0xdbb77032, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global SceThreadmgrForDriver_E54FD746
.type SceThreadmgrForDriver_E54FD746, %function
SceThreadmgrForDriver_E54FD746:
.word 0x10, 0xe2c40624, 0xe54fd746, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelGetSystemTimeWide
.type sceKernelGetSystemTimeWide, %function
sceKernelGetSystemTimeWide:
.word 0x10, 0xe2c40624, 0xf4ee4fa9, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global SceThreadmgrForDriver_FA54D49A
.type SceThreadmgrForDriver_FA54D49A, %function
SceThreadmgrForDriver_FA54D49A:
.word 0x10, 0xe2c40624, 0xfa54d49a, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelCreateMutex
.type sceKernelCreateMutex, %function
sceKernelCreateMutex:
.word 0x10, 0xe2c40624, 0xfbaa026e, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelUserUnmap
.type sceKernelUserUnmap, %function
sceKernelUserUnmap:
.word 0x10, 0x6f25e18a, 0x575b00, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelFreeMemBlock
.type sceKernelFreeMemBlock, %function
sceKernelFreeMemBlock:
.word 0x10, 0x6f25e18a, 0x9e1c61, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceGUIDReferObjectWithClass
.type sceGUIDReferObjectWithClass, %function
sceGUIDReferObjectWithClass:
.word 0x10, 0x6f25e18a, 0xed6c14, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceGUIDClose
.type sceGUIDClose, %function
sceGUIDClose:
.word 0x10, 0x6f25e18a, 0x47d32f2, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceGUIDReleaseObject
.type sceGUIDReleaseObject, %function
sceGUIDReleaseObject:
.word 0x10, 0x6f25e18a, 0x149885c4, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelFreeHeapMemory
.type sceKernelFreeHeapMemory, %function
sceKernelFreeHeapMemory:
.word 0x10, 0x6f25e18a, 0x3ebce343, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global SceSysmemForDriver_45D22597
.type SceSysmemForDriver_45D22597, %function
SceSysmemForDriver_45D22597:
.word 0x10, 0x6f25e18a, 0x45d22597, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceGUIDKernelCreate
.type sceGUIDKernelCreate, %function
sceGUIDKernelCreate:
.word 0x10, 0x6f25e18a, 0x56a13e90, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelCopyFromUserProc
.type sceKernelCopyFromUserProc, %function
sceKernelCopyFromUserProc:
.word 0x10, 0x6f25e18a, 0x605275f8, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceUIDClassInitClass
.type sceUIDClassInitClass, %function
sceUIDClassInitClass:
.word 0x10, 0x6f25e18a, 0x61317102, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelCopyToUserProc
.type sceKernelCopyToUserProc, %function
sceKernelCopyToUserProc:
.word 0x10, 0x6f25e18a, 0x6b825479, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelCopyToUser
.type sceKernelCopyToUser, %function
sceKernelCopyToUser:
.word 0x10, 0x6f25e18a, 0x6d88ef8a, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelAllocHeapMemory
.type sceKernelAllocHeapMemory, %function
sceKernelAllocHeapMemory:
.word 0x10, 0x6f25e18a, 0x7b4cb60a, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global SceSysmemForDriver_7D4F8B5F
.type SceSysmemForDriver_7D4F8B5F, %function
SceSysmemForDriver_7D4F8B5F:
.word 0x10, 0x6f25e18a, 0x7d4f8b5f, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global SceSysmemForDriver_84A4AF5E
.type SceSysmemForDriver_84A4AF5E, %function
SceSysmemForDriver_84A4AF5E:
.word 0x10, 0x6f25e18a, 0x84a4af5e, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global SceSysmemForDriver_85336A1C
.type SceSysmemForDriver_85336A1C, %function
SceSysmemForDriver_85336A1C:
.word 0x10, 0x6f25e18a, 0x85336a1c, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelCreateHeap
.type sceKernelCreateHeap, %function
sceKernelCreateHeap:
.word 0x10, 0x6f25e18a, 0x9328e0e8, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelGetMemBlockBase
.type sceKernelGetMemBlockBase, %function
sceKernelGetMemBlockBase:
.word 0x10, 0x6f25e18a, 0xa841edda, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelCopyFromUser
.type sceKernelCopyFromUser, %function
sceKernelCopyFromUser:
.word 0x10, 0x6f25e18a, 0xbc996a7a, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global SceSysmemForDriver_BF209859
.type SceSysmemForDriver_BF209859, %function
SceSysmemForDriver_BF209859:
.word 0x10, 0x6f25e18a, 0xbf209859, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelAllocMemBlockForKernel
.type sceKernelAllocMemBlockForKernel, %function
sceKernelAllocMemBlockForKernel:
.word 0x10, 0x6f25e18a, 0xc94850c9, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelDeleteHeap
.type sceKernelDeleteHeap, %function
sceKernelDeleteHeap:
.word 0x10, 0x6f25e18a, 0xd6437637, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelStrncpyFromUser
.type sceKernelStrncpyFromUser, %function
sceKernelStrncpyFromUser:
.word 0x10, 0x6f25e18a, 0xdb3ec244, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global scePUIDGetUIDVectorByClass
.type scePUIDGetUIDVectorByClass, %function
scePUIDGetUIDVectorByClass:
.word 0x10, 0x2451f0f, 0x8c05493, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global SceSysmemForKernel_368F1C36
.type SceSysmemForKernel_368F1C36, %function
SceSysmemForKernel_368F1C36:
.word 0x10, 0x2451f0f, 0x368f1c36, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global SceSysmemForKernel_4E6D8BC3
.type SceSysmemForKernel_4E6D8BC3, %function
SceSysmemForKernel_4E6D8BC3:
.word 0x10, 0x2451f0f, 0x4e6d8bc3, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global sceGUIDGetUIDVectorByClass
.type sceGUIDGetUIDVectorByClass, %function
sceGUIDGetUIDVectorByClass:
.word 0x10, 0x2451f0f, 0x52137fa3, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global SceSysmemForKernel_7F7D5C4A
.type SceSysmemForKernel_7F7D5C4A, %function
SceSysmemForKernel_7F7D5C4A:
.word 0x10, 0x2451f0f, 0x7f7d5c4a, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global sceGUIDOpenByGUID
.type sceGUIDOpenByGUID, %function
sceGUIDOpenByGUID:
.word 0x10, 0x2451f0f, 0x89d33df0, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global SceSysmemForKernel_B591E49F
.type SceSysmemForKernel_B591E49F, %function
SceSysmemForKernel_B591E49F:
.word 0x10, 0x2451f0f, 0xb591e49f, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global sceKernelObjectHeapSetResourceLimit
.type sceKernelObjectHeapSetResourceLimit, %function
sceKernelObjectHeapSetResourceLimit:
.word 0x10, 0x2451f0f, 0xdfa1eb57, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global SceSysmemForKernel_FA6C098C
.type SceSysmemForKernel_FA6C098C, %function
SceSysmemForKernel_FA6C098C:
.word 0x10, 0x2451f0f, 0xfa6c098c, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global sceGUIDKernelCreateWithOpt
.type sceGUIDKernelCreateWithOpt, %function
sceGUIDKernelCreateWithOpt:
.word 0x10, 0x2451f0f, 0xfb6390ce, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global SceCpuForDriver_341B6E81
.type SceCpuForDriver_341B6E81, %function
SceCpuForDriver_341B6E81:
.word 0x10, 0x40ecdb0e, 0x341b6e81, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global SceCpuForDriver_5F6A8743
.type SceCpuForDriver_5F6A8743, %function
SceCpuForDriver_5F6A8743:
.word 0x10, 0x40ecdb0e, 0x5f6a8743, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global sceKernelSpinlockLowUnlockCpuResumeIntr
.type sceKernelSpinlockLowUnlockCpuResumeIntr, %function
sceKernelSpinlockLowUnlockCpuResumeIntr:
.word 0x10, 0x40ecdb0e, 0x7bb9d5df, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global SceCpuForDriver_A4884C4E
.type SceCpuForDriver_A4884C4E, %function
SceCpuForDriver_A4884C4E:
.word 0x10, 0x40ecdb0e, 0xa4884c4e, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global SceCpuForDriver_CDA96E81
.type SceCpuForDriver_CDA96E81, %function
SceCpuForDriver_CDA96E81:
.word 0x10, 0x40ecdb0e, 0xcda96e81, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global sceKernelSpinlockLowLockCpuSuspendIntr
.type sceKernelSpinlockLowLockCpuSuspendIntr, %function
sceKernelSpinlockLowLockCpuSuspendIntr:
.word 0x10, 0x40ecdb0e, 0xd32ace9e, 0xe320f000
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global SceDebugForDriver_02B04343
.type SceDebugForDriver_02B04343, %function
SceDebugForDriver_02B04343:
.word 0x10, 0x88758561, 0x2b04343, 0xe320f000
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global sceKernelAssert
.type sceKernelAssert, %function
sceKernelAssert:
.word 0x10, 0x88758561, 0x35a35322, 0xe320f000
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global SceDebugForDriver_374B7868
.type SceDebugForDriver_374B7868, %function
SceDebugForDriver_374B7868:
.word 0x10, 0x88758561, 0x374b7868, 0xe320f000
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global SceDebugForDriver_391B5B74
.type SceDebugForDriver_391B5B74, %function
SceDebugForDriver_391B5B74:
.word 0x10, 0x88758561, 0x391b5b74, 0xe320f000
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global SceDebugForDriver_391B74B7
.type SceDebugForDriver_391B74B7, %function
SceDebugForDriver_391B74B7:
.word 0x10, 0x88758561, 0x391b74b7, 0xe320f000
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global SceDebugForDriver_D9703808
.type SceDebugForDriver_D9703808, %function
SceDebugForDriver_D9703808:
.word 0x10, 0x88758561, 0xd9703808, 0xe320f000
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global SceDebugForDriver_FD753E7A
.type SceDebugForDriver_FD753E7A, %function
SceDebugForDriver_FD753E7A:
.word 0x10, 0x88758561, 0xfd753e7a, 0xe320f000
.section .vitalink.fstubs.SceDebugForKernel, "ax", %progbits
.balign 4
.global SceDebugForKernel_2AABAEDA
.type SceDebugForKernel_2AABAEDA, %function
SceDebugForKernel_2AABAEDA:
.word 0x10, 0x13d793b7, 0x2aabaeda, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global memset
.type memset, %function
memset:
.word 0x10, 0x7ee45391, 0xab9bf5c, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global strncmp
.type strncmp, %function
strncmp:
.word 0x10, 0x7ee45391, 0x12cee649, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global strchr
.type strchr, %function
strchr:
.word 0x10, 0x7ee45391, 0x38463759, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global memcpy
.type memcpy, %function
memcpy:
.word 0x10, 0x7ee45391, 0x40c88316, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global strncpy
.type strncpy, %function
strncpy:
.word 0x10, 0x7ee45391, 0x6d286146, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global __aeabi_ldivmod
.type __aeabi_ldivmod, %function
__aeabi_ldivmod:
.word 0x10, 0x7ee45391, 0x7554ab04, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global __strncpy_chk
.type __strncpy_chk, %function
__strncpy_chk:
.word 0x10, 0x7ee45391, 0x96268c53, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global strncat
.type strncat, %function
strncat:
.word 0x10, 0x7ee45391, 0xa1d1c32c, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global __aeabi_uidivmod
.type __aeabi_uidivmod, %function
__aeabi_uidivmod:
.word 0x10, 0x7ee45391, 0xa46cb7de, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global toupper
.type toupper, %function
toupper:
.word 0x10, 0x7ee45391, 0xa685dcb1, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global __aeabi_uidiv
.type __aeabi_uidiv, %function
__aeabi_uidiv:
.word 0x10, 0x7ee45391, 0xa9ff1205, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global snprintf
.type snprintf, %function
snprintf:
.word 0x10, 0x7ee45391, 0xae7a8981, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global __stack_chk_fail
.type __stack_chk_fail, %function
__stack_chk_fail:
.word 0x10, 0x7ee45391, 0xb997493d, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global strnlen
.type strnlen, %function
strnlen:
.word 0x10, 0x7ee45391, 0xcd4bd884, 0xe320f000
.section .vitalink.fstubs.SceProcessmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelRegisterKPLS
.type sceKernelRegisterKPLS, %function
sceKernelRegisterKPLS:
.word 0x10, 0x746ec971, 0x3801d7d6, 0xe320f000
.section .vitalink.fstubs.SceProcessmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelAllocRemoteCDialogHeap
.type sceKernelAllocRemoteCDialogHeap, %function
sceKernelAllocRemoteCDialogHeap:
.word 0x10, 0x746ec971, 0x8a85fa28, 0xe320f000
.section .vitalink.fstubs.SceProcessmgrForDriver, "ax", %progbits
.balign 4
.global SceProcessmgrForDriver_AF80F39C
.type SceProcessmgrForDriver_AF80F39C, %function
SceProcessmgrForDriver_AF80F39C:
.word 0x10, 0x746ec971, 0xaf80f39c, 0xe320f000
.section .vitalink.fstubs.SceProcessmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelGetProcessResourceLimit
.type sceKernelGetProcessResourceLimit, %function
sceKernelGetProcessResourceLimit:
.word 0x10, 0x746ec971, 0xb80dbd53, 0xe320f000
.section .vitalink.fstubs.SceProcessmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelFreeRemoteCDialogHeap
.type sceKernelFreeRemoteCDialogHeap, %function
sceKernelFreeRemoteCDialogHeap:
.word 0x10, 0x746ec971, 0xdd0a58d1, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global SceSblACMgrForKernel_sceSblACMgrIsGameProgram
.type SceSblACMgrForKernel_sceSblACMgrIsGameProgram, %function
SceSblACMgrForKernel_sceSblACMgrIsGameProgram:
.word 0x10, 0x11f9b314, 0x5fdc646, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global SceSblACMgrForKernel_sceSblACMgrSetFSAttributeByAttributeWithMountPoint
.type SceSblACMgrForKernel_sceSblACMgrSetFSAttributeByAttributeWithMountPoint, %function
SceSblACMgrForKernel_sceSblACMgrSetFSAttributeByAttributeWithMountPoint:
.word 0x10, 0x11f9b314, 0x1b160234, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global SceSblACMgrForKernel_sceSblACMgrIsKernelProgram
.type SceSblACMgrForKernel_sceSblACMgrIsKernelProgram, %function
SceSblACMgrForKernel_sceSblACMgrIsKernelProgram:
.word 0x10, 0x11f9b314, 0x31c23b66, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global SceSblACMgrForKernel_3F99279F
.type SceSblACMgrForKernel_3F99279F, %function
SceSblACMgrForKernel_3F99279F:
.word 0x10, 0x11f9b314, 0x3f99279f, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global SceSblACMgrForKernel_570D6AD3
.type SceSblACMgrForKernel_570D6AD3, %function
SceSblACMgrForKernel_570D6AD3:
.word 0x10, 0x11f9b314, 0x570d6ad3, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global SceSblACMgrForKernel_75AAF981
.type SceSblACMgrForKernel_75AAF981, %function
SceSblACMgrForKernel_75AAF981:
.word 0x10, 0x11f9b314, 0x75aaf981, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global SceSblACMgrForKernel_sceSblACMgrGetPaid
.type SceSblACMgrForKernel_sceSblACMgrGetPaid, %function
SceSblACMgrForKernel_sceSblACMgrGetPaid:
.word 0x10, 0x11f9b314, 0x7c2af978, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global SceSblACMgrForKernel_sceSblACMgrIsSystemProgram
.type SceSblACMgrForKernel_sceSblACMgrIsSystemProgram, %function
SceSblACMgrForKernel_sceSblACMgrIsSystemProgram:
.word 0x10, 0x11f9b314, 0x930cd037, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global SceSblACMgrForKernel_sceSblACMgrGetMediaType
.type SceSblACMgrForKernel_sceSblACMgrGetMediaType, %function
SceSblACMgrForKernel_sceSblACMgrGetMediaType:
.word 0x10, 0x11f9b314, 0xd442962e, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global SceSysrootForKernel_50FE3B4D
.type SceSysrootForKernel_50FE3B4D, %function
SceSysrootForKernel_50FE3B4D:
.word 0x10, 0x3691da45, 0x50fe3b4d, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global SceSysrootForKernel_55392965
.type SceSysrootForKernel_55392965, %function
SceSysrootForKernel_55392965:
.word 0x10, 0x3691da45, 0x55392965, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global SceSysrootForKernel_7968F091
.type SceSysrootForKernel_7968F091, %function
SceSysrootForKernel_7968F091:
.word 0x10, 0x3691da45, 0x7968f091, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global SceSysrootForKernel_834439A7
.type SceSysrootForKernel_834439A7, %function
SceSysrootForKernel_834439A7:
.word 0x10, 0x3691da45, 0x834439a7, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global SceSysrootForKernel_B0E1FC67
.type SceSysrootForKernel_B0E1FC67, %function
SceSysrootForKernel_B0E1FC67:
.word 0x10, 0x3691da45, 0xb0e1fc67, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global SceSysrootForKernel_CBD377B7
.type SceSysrootForKernel_CBD377B7, %function
SceSysrootForKernel_CBD377B7:
.word 0x10, 0x3691da45, 0xcbd377b7, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global sceKernelSysrootAllocRemoteProcessHeap
.type sceKernelSysrootAllocRemoteProcessHeap, %function
sceKernelSysrootAllocRemoteProcessHeap:
.word 0x10, 0x3691da45, 0xd351ebc8, 0xe320f000
.section .vitalink.fstubs.SceSysrootForDriver, "ax", %progbits
.balign 4
.global SceSysrootForDriver_05093E7B
.type SceSysrootForDriver_05093E7B, %function
SceSysrootForDriver_05093E7B:
.word 0x10, 0x2ed7f97a, 0x5093e7b, 0xe320f000
.section .vitalink.fstubs.SceSysrootForDriver, "ax", %progbits
.balign 4
.global sceKernelRegisterInitCallback
.type sceKernelRegisterInitCallback, %function
sceKernelRegisterInitCallback:
.word 0x10, 0x2ed7f97a, 0x778d0966, 0xe320f000
.section .vitalink.fstubs.SceSysrootForDriver, "ax", %progbits
.balign 4
.global sceKernelApiDeclareTypeInt64
.type sceKernelApiDeclareTypeInt64, %function
sceKernelApiDeclareTypeInt64:
.word 0x10, 0x2ed7f97a, 0x8b8f1d01, 0xe320f000
.section .vitalink.fstubs.SceProcEventForDriver, "ax", %progbits
.balign 4
.global sceKernelRegisterProcEventHandler
.type sceKernelRegisterProcEventHandler, %function
sceKernelRegisterProcEventHandler:
.word 0x10, 0x887f19d0, 0x2a43912d, 0xe320f000
.section .vitalink.fstubs.SceDipswForDriver, "ax", %progbits
.balign 4
.global sceKernelCheckDipsw
.type sceKernelCheckDipsw, %function
sceKernelCheckDipsw:
.word 0x10, 0xc9e26388, 0xa98fc2fd, 0xe320f000
.section .vitalink.fstubs.SceKernelSuspendForDriver, "ax", %progbits
.balign 4
.global sceKernelRegisterSysEventHandler
.type sceKernelRegisterSysEventHandler, %function
sceKernelRegisterSysEventHandler:
.word 0x10, 0x7290b21c, 0x4c05d10, 0xe320f000
.section .vitalink.fstubs.ScePfsFacadeForKernel, "ax", %progbits
.balign 4
.global ScePfsFacadeForKernel_04352D52
.type ScePfsFacadeForKernel_04352D52, %function
ScePfsFacadeForKernel_04352D52:
.word 0x10, 0xc26dc7bd, 0x4352d52, 0xe320f000
.section .vitalink.fstubs.ScePfsFacadeForKernel, "ax", %progbits
.balign 4
.global ScePfsFacadeForKernel_4238D2D2
.type ScePfsFacadeForKernel_4238D2D2, %function
ScePfsFacadeForKernel_4238D2D2:
.word 0x10, 0xc26dc7bd, 0x4238d2d2, 0xe320f000
.section .vitalink.fstubs.ScePfsFacadeForKernel, "ax", %progbits
.balign 4
.global ScePfsFacadeForKernel_58E643C5
.type ScePfsFacadeForKernel_58E643C5, %function
ScePfsFacadeForKernel_58E643C5:
.word 0x10, 0xc26dc7bd, 0x58e643c5, 0xe320f000
.section .vitalink.fstubs.ScePfsFacadeForKernel, "ax", %progbits
.balign 4
.global ScePfsFacadeForKernel_7F9F4E49
.type ScePfsFacadeForKernel_7F9F4E49, %function
ScePfsFacadeForKernel_7F9F4E49:
.word 0x10, 0xc26dc7bd, 0x7f9f4e49, 0xe320f000
.section .vitalink.fstubs.ScePfsFacadeForKernel, "ax", %progbits
.balign 4
.global ScePfsFacadeForKernel_BD5B21F6
.type ScePfsFacadeForKernel_BD5B21F6, %function
ScePfsFacadeForKernel_BD5B21F6:
.word 0x10, 0xc26dc7bd, 0xbd5b21f6, 0xe320f000
.section .vitalink.fstubs.ScePfsFacadeForKernel, "ax", %progbits
.balign 4
.global ScePfsFacadeForKernel_CB622FFE
.type ScePfsFacadeForKernel_CB622FFE, %function
ScePfsFacadeForKernel_CB622FFE:
.word 0x10, 0xc26dc7bd, 0xcb622ffe, 0xe320f000
.section .vitalink.fstubs.SceFios2KernelForDriver, "ax", %progbits
.balign 4
.global SceFios2KernelForDriver_0F456345
.type SceFios2KernelForDriver_0F456345, %function
SceFios2KernelForDriver_0F456345:
.word 0x10, 0x54d6b9eb, 0xf456345, 0xe320f000
.section .vitalink.fstubs.SceFios2KernelForDriver, "ax", %progbits
.balign 4
.global SceFios2KernelForDriver_2649408B
.type SceFios2KernelForDriver_2649408B, %function
SceFios2KernelForDriver_2649408B:
.word 0x10, 0x54d6b9eb, 0x2649408b, 0xe320f000
.section .vitalink.fstubs.SceFios2KernelForDriver, "ax", %progbits
.balign 4
.global SceFios2KernelForDriver_28E28A58
.type SceFios2KernelForDriver_28E28A58, %function
SceFios2KernelForDriver_28E28A58:
.word 0x10, 0x54d6b9eb, 0x28e28a58, 0xe320f000
.section .vitalink.fstubs.SceFios2KernelForDriver, "ax", %progbits
.balign 4
.global SceFios2KernelForDriver_35E7E75C
.type SceFios2KernelForDriver_35E7E75C, %function
SceFios2KernelForDriver_35E7E75C:
.word 0x10, 0x54d6b9eb, 0x35e7e75c, 0xe320f000
.section .vitalink.fstubs.SceFios2KernelForDriver, "ax", %progbits
.balign 4
.global SceFios2KernelForDriver_3B329E86
.type SceFios2KernelForDriver_3B329E86, %function
SceFios2KernelForDriver_3B329E86:
.word 0x10, 0x54d6b9eb, 0x3b329e86, 0xe320f000
.section .vitalink.fstubs.SceFios2KernelForDriver, "ax", %progbits
.balign 4
.global SceFios2KernelForDriver_990F46A5
.type SceFios2KernelForDriver_990F46A5, %function
SceFios2KernelForDriver_990F46A5:
.word 0x10, 0x54d6b9eb, 0x990f46a5, 0xe320f000
.section .vitalink.fstubs.SceFios2KernelForDriver, "ax", %progbits
.balign 4
.global SceFios2KernelForDriver_DD7627EC
.type SceFios2KernelForDriver_DD7627EC, %function
SceFios2KernelForDriver_DD7627EC:
.word 0x10, 0x54d6b9eb, 0xdd7627ec, 0xe320f000
#else
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelGetThreadCurrentPriority
.type sceKernelGetThreadCurrentPriority, %function
sceKernelGetThreadCurrentPriority:
.word 0x10, 0xe2c40624, 0x1414f0b, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelSetThreadAccessLevel
.type sceKernelSetThreadAccessLevel, %function
sceKernelSetThreadAccessLevel:
.word 0x10, 0xe2c40624, 0x2eedf17, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global SceThreadmgrForDriver_0486F239
.type SceThreadmgrForDriver_0486F239, %function
SceThreadmgrForDriver_0486F239:
.word 0x10, 0xe2c40624, 0x486f239, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelDeleteMutex
.type sceKernelDeleteMutex, %function
sceKernelDeleteMutex:
.word 0x10, 0xe2c40624, 0xa912340, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelWaitEventFlag
.type sceKernelWaitEventFlag, %function
sceKernelWaitEventFlag:
.word 0x10, 0xe2c40624, 0xc1d3f20, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelGetEventPattern
.type sceKernelGetEventPattern, %function
sceKernelGetEventPattern:
.word 0x10, 0xe2c40624, 0xc6bcb8c, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global SceThreadmgrForDriver_1096042E
.type SceThreadmgrForDriver_1096042E, %function
SceThreadmgrForDriver_1096042E:
.word 0x10, 0xe2c40624, 0x1096042e, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelFinalizeFastMutex
.type sceKernelFinalizeFastMutex, %function
sceKernelFinalizeFastMutex:
.word 0x10, 0xe2c40624, 0x11fe84a1, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelLockMutex
.type sceKernelLockMutex, %function
sceKernelLockMutex:
.word 0x10, 0xe2c40624, 0x16ac80c5, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelUnlockMutex
.type sceKernelUnlockMutex, %function
sceKernelUnlockMutex:
.word 0x10, 0xe2c40624, 0x1e82e5d0, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelStartThread
.type sceKernelStartThread, %function
sceKernelStartThread:
.word 0x10, 0xe2c40624, 0x21f5419b, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelEventObjectInit
.type sceKernelEventObjectInit, %function
sceKernelEventObjectInit:
.word 0x10, 0xe2c40624, 0x25ab8f6b, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelTryLockMutex
.type sceKernelTryLockMutex, %function
sceKernelTryLockMutex:
.word 0x10, 0xe2c40624, 0x270993a6, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global SceThreadmgrForDriver_332E127C
.type SceThreadmgrForDriver_332E127C, %function
SceThreadmgrForDriver_332E127C:
.word 0x10, 0xe2c40624, 0x332e127c, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelChangeThreadPriorityAndCpuAffinityMask
.type sceKernelChangeThreadPriorityAndCpuAffinityMask, %function
sceKernelChangeThreadPriorityAndCpuAffinityMask:
.word 0x10, 0xe2c40624, 0x3857c94e, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelWaitThreadEnd
.type sceKernelWaitThreadEnd, %function
sceKernelWaitThreadEnd:
.word 0x10, 0xe2c40624, 0x3e20216f, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelCreateEventFlag
.type sceKernelCreateEventFlag, %function
sceKernelCreateEventFlag:
.word 0x10, 0xe2c40624, 0x4336baa4, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelCancelAlarm
.type sceKernelCancelAlarm, %function
sceKernelCancelAlarm:
.word 0x10, 0xe2c40624, 0x44ccf310, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelDelayThread
.type sceKernelDelayThread, %function
sceKernelDelayThread:
.word 0x10, 0xe2c40624, 0x4b675d05, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelClearEventFlag
.type sceKernelClearEventFlag, %function
sceKernelClearEventFlag:
.word 0x10, 0xe2c40624, 0x4f1da3be, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelGetThreadId
.type sceKernelGetThreadId, %function
sceKernelGetThreadId:
.word 0x10, 0xe2c40624, 0x59d06540, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelSignalCondTo
.type sceKernelSignalCondTo, %function
sceKernelSignalCondTo:
.word 0x10, 0xe2c40624, 0x61533da9, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelGetMutexInfo
.type sceKernelGetMutexInfo, %function
sceKernelGetMutexInfo:
.word 0x10, 0xe2c40624, 0x69b78a12, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelSignalCondAll
.type sceKernelSignalCondAll, %function
sceKernelSignalCondAll:
.word 0x10, 0xe2c40624, 0x6ec78cd0, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global lockFastMutex
.type lockFastMutex, %function
lockFastMutex:
.word 0x10, 0xe2c40624, 0x70627f3a, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelDeleteEventFlag
.type sceKernelDeleteEventFlag, %function
sceKernelDeleteEventFlag:
.word 0x10, 0xe2c40624, 0x71ecb352, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelTryLockFastMutex
.type sceKernelTryLockFastMutex, %function
sceKernelTryLockFastMutex:
.word 0x10, 0xe2c40624, 0x741f4707, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelGetThreadCpuAffinityMask
.type sceKernelGetThreadCpuAffinityMask, %function
sceKernelGetThreadCpuAffinityMask:
.word 0x10, 0xe2c40624, 0x83dc703d, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global SceThreadmgrForDriver_89CA5698
.type SceThreadmgrForDriver_89CA5698, %function
SceThreadmgrForDriver_89CA5698:
.word 0x10, 0xe2c40624, 0x89ca5698, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelGetProcessId
.type sceKernelGetProcessId, %function
sceKernelGetProcessId:
.word 0x10, 0xe2c40624, 0x9dcb4b7a, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelSetEvent
.type sceKernelSetEvent, %function
sceKernelSetEvent:
.word 0x10, 0xe2c40624, 0x9ea3a45c, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelExtendKernelStackWide
.type sceKernelExtendKernelStackWide, %function
sceKernelExtendKernelStackWide:
.word 0x10, 0xe2c40624, 0xa2c801a5, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelDeleteThread
.type sceKernelDeleteThread, %function
sceKernelDeleteThread:
.word 0x10, 0xe2c40624, 0xac834f3f, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelDeleteCond
.type sceKernelDeleteCond, %function
sceKernelDeleteCond:
.word 0x10, 0xe2c40624, 0xaee0d27c, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelInitializeFastMutex
.type sceKernelInitializeFastMutex, %function
sceKernelInitializeFastMutex:
.word 0x10, 0xe2c40624, 0xaf8e1266, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global SceThreadmgrForDriver_B645C7EF
.type SceThreadmgrForDriver_B645C7EF, %function
SceThreadmgrForDriver_B645C7EF:
.word 0x10, 0xe2c40624, 0xb645c7ef, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelSetEventWithNotifyCallback
.type sceKernelSetEventWithNotifyCallback, %function
sceKernelSetEventWithNotifyCallback:
.word 0x10, 0xe2c40624, 0xb7df3edf, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global SceThreadmgrForDriver_C58DF384
.type SceThreadmgrForDriver_C58DF384, %function
SceThreadmgrForDriver_C58DF384:
.word 0x10, 0xe2c40624, 0xc58df384, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelCreateThread
.type sceKernelCreateThread, %function
sceKernelCreateThread:
.word 0x10, 0xe2c40624, 0xc6674e7d, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelWaitCond
.type sceKernelWaitCond, %function
sceKernelWaitCond:
.word 0x10, 0xe2c40624, 0xcc7e027d, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelSetEventFlag
.type sceKernelSetEventFlag, %function
sceKernelSetEventFlag:
.word 0x10, 0xe2c40624, 0xd4780c3e, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelGetFastMutexInfo
.type sceKernelGetFastMutexInfo, %function
sceKernelGetFastMutexInfo:
.word 0x10, 0xe2c40624, 0xd7af2e58, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelUnlockFastMutex
.type sceKernelUnlockFastMutex, %function
sceKernelUnlockFastMutex:
.word 0x10, 0xe2c40624, 0xdb395782, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelCreateCond
.type sceKernelCreateCond, %function
sceKernelCreateCond:
.word 0x10, 0xe2c40624, 0xdb6cd34a, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelUIDInheritedEventClassInit
.type sceKernelUIDInheritedEventClassInit, %function
sceKernelUIDInheritedEventClassInit:
.word 0x10, 0xe2c40624, 0xdbb77032, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global SceThreadmgrForDriver_E54FD746
.type SceThreadmgrForDriver_E54FD746, %function
SceThreadmgrForDriver_E54FD746:
.word 0x10, 0xe2c40624, 0xe54fd746, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelGetSystemTimeWide
.type sceKernelGetSystemTimeWide, %function
sceKernelGetSystemTimeWide:
.word 0x10, 0xe2c40624, 0xf4ee4fa9, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global SceThreadmgrForDriver_FA54D49A
.type SceThreadmgrForDriver_FA54D49A, %function
SceThreadmgrForDriver_FA54D49A:
.word 0x10, 0xe2c40624, 0xfa54d49a, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelCreateMutex
.type sceKernelCreateMutex, %function
sceKernelCreateMutex:
.word 0x10, 0xe2c40624, 0xfbaa026e, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelUserUnmap
.type sceKernelUserUnmap, %function
sceKernelUserUnmap:
.word 0x10, 0x6f25e18a, 0x575b00, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelFreeMemBlock
.type sceKernelFreeMemBlock, %function
sceKernelFreeMemBlock:
.word 0x10, 0x6f25e18a, 0x9e1c61, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceGUIDReferObjectWithClass
.type sceGUIDReferObjectWithClass, %function
sceGUIDReferObjectWithClass:
.word 0x10, 0x6f25e18a, 0xed6c14, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceGUIDClose
.type sceGUIDClose, %function
sceGUIDClose:
.word 0x10, 0x6f25e18a, 0x47d32f2, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceGUIDReleaseObject
.type sceGUIDReleaseObject, %function
sceGUIDReleaseObject:
.word 0x10, 0x6f25e18a, 0x149885c4, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelFreeHeapMemory
.type sceKernelFreeHeapMemory, %function
sceKernelFreeHeapMemory:
.word 0x10, 0x6f25e18a, 0x3ebce343, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global SceSysmemForDriver_45D22597
.type SceSysmemForDriver_45D22597, %function
SceSysmemForDriver_45D22597:
.word 0x10, 0x6f25e18a, 0x45d22597, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceGUIDKernelCreate
.type sceGUIDKernelCreate, %function
sceGUIDKernelCreate:
.word 0x10, 0x6f25e18a, 0x56a13e90, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelCopyFromUserProc
.type sceKernelCopyFromUserProc, %function
sceKernelCopyFromUserProc:
.word 0x10, 0x6f25e18a, 0x605275f8, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceUIDClassInitClass
.type sceUIDClassInitClass, %function
sceUIDClassInitClass:
.word 0x10, 0x6f25e18a, 0x61317102, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelCopyToUserProc
.type sceKernelCopyToUserProc, %function
sceKernelCopyToUserProc:
.word 0x10, 0x6f25e18a, 0x6b825479, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelCopyToUser
.type sceKernelCopyToUser, %function
sceKernelCopyToUser:
.word 0x10, 0x6f25e18a, 0x6d88ef8a, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelAllocHeapMemory
.type sceKernelAllocHeapMemory, %function
sceKernelAllocHeapMemory:
.word 0x10, 0x6f25e18a, 0x7b4cb60a, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global SceSysmemForDriver_7D4F8B5F
.type SceSysmemForDriver_7D4F8B5F, %function
SceSysmemForDriver_7D4F8B5F:
.word 0x10, 0x6f25e18a, 0x7d4f8b5f, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global SceSysmemForDriver_84A4AF5E
.type SceSysmemForDriver_84A4AF5E, %function
SceSysmemForDriver_84A4AF5E:
.word 0x10, 0x6f25e18a, 0x84a4af5e, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global SceSysmemForDriver_85336A1C
.type SceSysmemForDriver_85336A1C, %function
SceSysmemForDriver_85336A1C:
.word 0x10, 0x6f25e18a, 0x85336a1c, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelCreateHeap
.type sceKernelCreateHeap, %function
sceKernelCreateHeap:
.word 0x10, 0x6f25e18a, 0x9328e0e8, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelGetMemBlockBase
.type sceKernelGetMemBlockBase, %function
sceKernelGetMemBlockBase:
.word 0x10, 0x6f25e18a, 0xa841edda, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelCopyFromUser
.type sceKernelCopyFromUser, %function
sceKernelCopyFromUser:
.word 0x10, 0x6f25e18a, 0xbc996a7a, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global SceSysmemForDriver_BF209859
.type SceSysmemForDriver_BF209859, %function
SceSysmemForDriver_BF209859:
.word 0x10, 0x6f25e18a, 0xbf209859, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelAllocMemBlockForKernel
.type sceKernelAllocMemBlockForKernel, %function
sceKernelAllocMemBlockForKernel:
.word 0x10, 0x6f25e18a, 0xc94850c9, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelDeleteHeap
.type sceKernelDeleteHeap, %function
sceKernelDeleteHeap:
.word 0x10, 0x6f25e18a, 0xd6437637, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global sceKernelStrncpyFromUser
.type sceKernelStrncpyFromUser, %function
sceKernelStrncpyFromUser:
.word 0x10, 0x6f25e18a, 0xdb3ec244, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global scePUIDGetUIDVectorByClass
.type scePUIDGetUIDVectorByClass, %function
scePUIDGetUIDVectorByClass:
.word 0x10, 0x2451f0f, 0x8c05493, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global SceSysmemForKernel_368F1C36
.type SceSysmemForKernel_368F1C36, %function
SceSysmemForKernel_368F1C36:
.word 0x10, 0x2451f0f, 0x368f1c36, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global SceSysmemForKernel_4E6D8BC3
.type SceSysmemForKernel_4E6D8BC3, %function
SceSysmemForKernel_4E6D8BC3:
.word 0x10, 0x2451f0f, 0x4e6d8bc3, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global sceGUIDGetUIDVectorByClass
.type sceGUIDGetUIDVectorByClass, %function
sceGUIDGetUIDVectorByClass:
.word 0x10, 0x2451f0f, 0x52137fa3, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global SceSysmemForKernel_7F7D5C4A
.type SceSysmemForKernel_7F7D5C4A, %function
SceSysmemForKernel_7F7D5C4A:
.word 0x10, 0x2451f0f, 0x7f7d5c4a, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global sceGUIDOpenByGUID
.type sceGUIDOpenByGUID, %function
sceGUIDOpenByGUID:
.word 0x10, 0x2451f0f, 0x89d33df0, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global SceSysmemForKernel_B591E49F
.type SceSysmemForKernel_B591E49F, %function
SceSysmemForKernel_B591E49F:
.word 0x10, 0x2451f0f, 0xb591e49f, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global sceKernelObjectHeapSetResourceLimit
.type sceKernelObjectHeapSetResourceLimit, %function
sceKernelObjectHeapSetResourceLimit:
.word 0x10, 0x2451f0f, 0xdfa1eb57, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global SceSysmemForKernel_FA6C098C
.type SceSysmemForKernel_FA6C098C, %function
SceSysmemForKernel_FA6C098C:
.word 0x10, 0x2451f0f, 0xfa6c098c, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global sceGUIDKernelCreateWithOpt
.type sceGUIDKernelCreateWithOpt, %function
sceGUIDKernelCreateWithOpt:
.word 0x10, 0x2451f0f, 0xfb6390ce, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global SceCpuForDriver_341B6E81
.type SceCpuForDriver_341B6E81, %function
SceCpuForDriver_341B6E81:
.word 0x10, 0x40ecdb0e, 0x341b6e81, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global SceCpuForDriver_5F6A8743
.type SceCpuForDriver_5F6A8743, %function
SceCpuForDriver_5F6A8743:
.word 0x10, 0x40ecdb0e, 0x5f6a8743, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global sceKernelSpinlockLowUnlockCpuResumeIntr
.type sceKernelSpinlockLowUnlockCpuResumeIntr, %function
sceKernelSpinlockLowUnlockCpuResumeIntr:
.word 0x10, 0x40ecdb0e, 0x7bb9d5df, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global SceCpuForDriver_A4884C4E
.type SceCpuForDriver_A4884C4E, %function
SceCpuForDriver_A4884C4E:
.word 0x10, 0x40ecdb0e, 0xa4884c4e, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global SceCpuForDriver_CDA96E81
.type SceCpuForDriver_CDA96E81, %function
SceCpuForDriver_CDA96E81:
.word 0x10, 0x40ecdb0e, 0xcda96e81, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global sceKernelSpinlockLowLockCpuSuspendIntr
.type sceKernelSpinlockLowLockCpuSuspendIntr, %function
sceKernelSpinlockLowLockCpuSuspendIntr:
.word 0x10, 0x40ecdb0e, 0xd32ace9e, 0xe320f000
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global SceDebugForDriver_02B04343
.type SceDebugForDriver_02B04343, %function
SceDebugForDriver_02B04343:
.word 0x10, 0x88758561, 0x2b04343, 0xe320f000
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global SceDebugForDriver_391B5B74
.type SceDebugForDriver_391B5B74, %function
SceDebugForDriver_391B5B74:
.word 0x10, 0x88758561, 0x391b5b74, 0xe320f000
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global SceDebugForDriver_391B74B7
.type SceDebugForDriver_391B74B7, %function
SceDebugForDriver_391B74B7:
.word 0x10, 0x88758561, 0x391b74b7, 0xe320f000
.section .vitalink.fstubs.SceDebugForKernel, "ax", %progbits
.balign 4
.global SceDebugForKernel_2AABAEDA
.type SceDebugForKernel_2AABAEDA, %function
SceDebugForKernel_2AABAEDA:
.word 0x10, 0x13d793b7, 0x2aabaeda, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global memset
.type memset, %function
memset:
.word 0x10, 0x7ee45391, 0xab9bf5c, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global strncmp
.type strncmp, %function
strncmp:
.word 0x10, 0x7ee45391, 0x12cee649, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global strchr
.type strchr, %function
strchr:
.word 0x10, 0x7ee45391, 0x38463759, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global memcpy
.type memcpy, %function
memcpy:
.word 0x10, 0x7ee45391, 0x40c88316, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global strncpy
.type strncpy, %function
strncpy:
.word 0x10, 0x7ee45391, 0x6d286146, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global __aeabi_ldivmod
.type __aeabi_ldivmod, %function
__aeabi_ldivmod:
.word 0x10, 0x7ee45391, 0x7554ab04, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global strncat
.type strncat, %function
strncat:
.word 0x10, 0x7ee45391, 0xa1d1c32c, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global __aeabi_uidivmod
.type __aeabi_uidivmod, %function
__aeabi_uidivmod:
.word 0x10, 0x7ee45391, 0xa46cb7de, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global toupper
.type toupper, %function
toupper:
.word 0x10, 0x7ee45391, 0xa685dcb1, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global __aeabi_uidiv
.type __aeabi_uidiv, %function
__aeabi_uidiv:
.word 0x10, 0x7ee45391, 0xa9ff1205, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global snprintf
.type snprintf, %function
snprintf:
.word 0x10, 0x7ee45391, 0xae7a8981, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global __stack_chk_fail
.type __stack_chk_fail, %function
__stack_chk_fail:
.word 0x10, 0x7ee45391, 0xb997493d, 0xe320f000
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global strnlen
.type strnlen, %function
strnlen:
.word 0x10, 0x7ee45391, 0xcd4bd884, 0xe320f000
.section .vitalink.fstubs.SceProcessmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelRegisterKPLS
.type sceKernelRegisterKPLS, %function
sceKernelRegisterKPLS:
.word 0x10, 0x746ec971, 0x3801d7d6, 0xe320f000
.section .vitalink.fstubs.SceProcessmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelAllocRemoteCDialogHeap
.type sceKernelAllocRemoteCDialogHeap, %function
sceKernelAllocRemoteCDialogHeap:
.word 0x10, 0x746ec971, 0x8a85fa28, 0xe320f000
.section .vitalink.fstubs.SceProcessmgrForDriver, "ax", %progbits
.balign 4
.global SceProcessmgrForDriver_AF80F39C
.type SceProcessmgrForDriver_AF80F39C, %function
SceProcessmgrForDriver_AF80F39C:
.word 0x10, 0x746ec971, 0xaf80f39c, 0xe320f000
.section .vitalink.fstubs.SceProcessmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelGetProcessResourceLimit
.type sceKernelGetProcessResourceLimit, %function
sceKernelGetProcessResourceLimit:
.word 0x10, 0x746ec971, 0xb80dbd53, 0xe320f000
.section .vitalink.fstubs.SceProcessmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelFreeRemoteCDialogHeap
.type sceKernelFreeRemoteCDialogHeap, %function
sceKernelFreeRemoteCDialogHeap:
.word 0x10, 0x746ec971, 0xdd0a58d1, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global SceSblACMgrForKernel_sceSblACMgrIsGameProgram
.type SceSblACMgrForKernel_sceSblACMgrIsGameProgram, %function
SceSblACMgrForKernel_sceSblACMgrIsGameProgram:
.word 0x10, 0x11f9b314, 0x5fdc646, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global SceSblACMgrForKernel_sceSblACMgrSetFSAttributeByAttributeWithMountPoint
.type SceSblACMgrForKernel_sceSblACMgrSetFSAttributeByAttributeWithMountPoint, %function
SceSblACMgrForKernel_sceSblACMgrSetFSAttributeByAttributeWithMountPoint:
.word 0x10, 0x11f9b314, 0x1b160234, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global SceSblACMgrForKernel_sceSblACMgrIsKernelProgram
.type SceSblACMgrForKernel_sceSblACMgrIsKernelProgram, %function
SceSblACMgrForKernel_sceSblACMgrIsKernelProgram:
.word 0x10, 0x11f9b314, 0x31c23b66, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global SceSblACMgrForKernel_3F99279F
.type SceSblACMgrForKernel_3F99279F, %function
SceSblACMgrForKernel_3F99279F:
.word 0x10, 0x11f9b314, 0x3f99279f, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global SceSblACMgrForKernel_570D6AD3
.type SceSblACMgrForKernel_570D6AD3, %function
SceSblACMgrForKernel_570D6AD3:
.word 0x10, 0x11f9b314, 0x570d6ad3, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global SceSblACMgrForKernel_75AAF981
.type SceSblACMgrForKernel_75AAF981, %function
SceSblACMgrForKernel_75AAF981:
.word 0x10, 0x11f9b314, 0x75aaf981, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global SceSblACMgrForKernel_sceSblACMgrGetPaid
.type SceSblACMgrForKernel_sceSblACMgrGetPaid, %function
SceSblACMgrForKernel_sceSblACMgrGetPaid:
.word 0x10, 0x11f9b314, 0x7c2af978, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global SceSblACMgrForKernel_sceSblACMgrIsSystemProgram
.type SceSblACMgrForKernel_sceSblACMgrIsSystemProgram, %function
SceSblACMgrForKernel_sceSblACMgrIsSystemProgram:
.word 0x10, 0x11f9b314, 0x930cd037, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global SceSblACMgrForKernel_sceSblACMgrGetMediaType
.type SceSblACMgrForKernel_sceSblACMgrGetMediaType, %function
SceSblACMgrForKernel_sceSblACMgrGetMediaType:
.word 0x10, 0x11f9b314, 0xd442962e, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global SceSysrootForKernel_50FE3B4D
.type SceSysrootForKernel_50FE3B4D, %function
SceSysrootForKernel_50FE3B4D:
.word 0x10, 0x3691da45, 0x50fe3b4d, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global SceSysrootForKernel_55392965
.type SceSysrootForKernel_55392965, %function
SceSysrootForKernel_55392965:
.word 0x10, 0x3691da45, 0x55392965, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global SceSysrootForKernel_7968F091
.type SceSysrootForKernel_7968F091, %function
SceSysrootForKernel_7968F091:
.word 0x10, 0x3691da45, 0x7968f091, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global SceSysrootForKernel_834439A7
.type SceSysrootForKernel_834439A7, %function
SceSysrootForKernel_834439A7:
.word 0x10, 0x3691da45, 0x834439a7, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global SceSysrootForKernel_B0E1FC67
.type SceSysrootForKernel_B0E1FC67, %function
SceSysrootForKernel_B0E1FC67:
.word 0x10, 0x3691da45, 0xb0e1fc67, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global SceSysrootForKernel_CBD377B7
.type SceSysrootForKernel_CBD377B7, %function
SceSysrootForKernel_CBD377B7:
.word 0x10, 0x3691da45, 0xcbd377b7, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global sceKernelSysrootAllocRemoteProcessHeap
.type sceKernelSysrootAllocRemoteProcessHeap, %function
sceKernelSysrootAllocRemoteProcessHeap:
.word 0x10, 0x3691da45, 0xd351ebc8, 0xe320f000
.section .vitalink.fstubs.SceSysrootForDriver, "ax", %progbits
.balign 4
.global SceSysrootForDriver_05093E7B
.type SceSysrootForDriver_05093E7B, %function
SceSysrootForDriver_05093E7B:
.word 0x10, 0x2ed7f97a, 0x5093e7b, 0xe320f000
.section .vitalink.fstubs.SceSysrootForDriver, "ax", %progbits
.balign 4
.global sceKernelRegisterInitCallback
.type sceKernelRegisterInitCallback, %function
sceKernelRegisterInitCallback:
.word 0x10, 0x2ed7f97a, 0x778d0966, 0xe320f000
.section .vitalink.fstubs.SceSysrootForDriver, "ax", %progbits
.balign 4
.global sceKernelApiDeclareTypeInt64
.type sceKernelApiDeclareTypeInt64, %function
sceKernelApiDeclareTypeInt64:
.word 0x10, 0x2ed7f97a, 0x8b8f1d01, 0xe320f000
.section .vitalink.fstubs.SceProcEventForDriver, "ax", %progbits
.balign 4
.global sceKernelRegisterProcEventHandler
.type sceKernelRegisterProcEventHandler, %function
sceKernelRegisterProcEventHandler:
.word 0x10, 0x887f19d0, 0x2a43912d, 0xe320f000
.section .vitalink.fstubs.SceDipswForDriver, "ax", %progbits
.balign 4
.global sceKernelCheckDipsw
.type sceKernelCheckDipsw, %function
sceKernelCheckDipsw:
.word 0x10, 0xc9e26388, 0xa98fc2fd, 0xe320f000
.section .vitalink.fstubs.SceKernelSuspendForDriver, "ax", %progbits
.balign 4
.global sceKernelRegisterSysEventHandler
.type sceKernelRegisterSysEventHandler, %function
sceKernelRegisterSysEventHandler:
.word 0x10, 0x7290b21c, 0x4c05d10, 0xe320f000
.section .vitalink.fstubs.ScePfsFacadeForKernel, "ax", %progbits
.balign 4
.global ScePfsFacadeForKernel_04352D52
.type ScePfsFacadeForKernel_04352D52, %function
ScePfsFacadeForKernel_04352D52:
.word 0x10, 0xc26dc7bd, 0x4352d52, 0xe320f000
.section .vitalink.fstubs.ScePfsFacadeForKernel, "ax", %progbits
.balign 4
.global ScePfsFacadeForKernel_4238D2D2
.type ScePfsFacadeForKernel_4238D2D2, %function
ScePfsFacadeForKernel_4238D2D2:
.word 0x10, 0xc26dc7bd, 0x4238d2d2, 0xe320f000
.section .vitalink.fstubs.ScePfsFacadeForKernel, "ax", %progbits
.balign 4
.global ScePfsFacadeForKernel_58E643C5
.type ScePfsFacadeForKernel_58E643C5, %function
ScePfsFacadeForKernel_58E643C5:
.word 0x10, 0xc26dc7bd, 0x58e643c5, 0xe320f000
.section .vitalink.fstubs.ScePfsFacadeForKernel, "ax", %progbits
.balign 4
.global ScePfsFacadeForKernel_7F9F4E49
.type ScePfsFacadeForKernel_7F9F4E49, %function
ScePfsFacadeForKernel_7F9F4E49:
.word 0x10, 0xc26dc7bd, 0x7f9f4e49, 0xe320f000
.section .vitalink.fstubs.ScePfsFacadeForKernel, "ax", %progbits
.balign 4
.global ScePfsFacadeForKernel_BD5B21F6
.type ScePfsFacadeForKernel_BD5B21F6, %function
ScePfsFacadeForKernel_BD5B21F6:
.word 0x10, 0xc26dc7bd, 0xbd5b21f6, 0xe320f000
.section .vitalink.fstubs.ScePfsFacadeForKernel, "ax", %progbits
.balign 4
.global ScePfsFacadeForKernel_CB622FFE
.type ScePfsFacadeForKernel_CB622FFE, %function
ScePfsFacadeForKernel_CB622FFE:
.word 0x10, 0xc26dc7bd, 0xcb622ffe, 0xe320f000
.section .vitalink.fstubs.SceFios2KernelForDriver, "ax", %progbits
.balign 4
.global SceFios2KernelForDriver_0F456345
.type SceFios2KernelForDriver_0F456345, %function
SceFios2KernelForDriver_0F456345:
.word 0x10, 0x54d6b9eb, 0xf456345, 0xe320f000
.section .vitalink.fstubs.SceFios2KernelForDriver, "ax", %progbits
.balign 4
.global SceFios2KernelForDriver_2649408B
.type SceFios2KernelForDriver_2649408B, %function
SceFios2KernelForDriver_2649408B:
.word 0x10, 0x54d6b9eb, 0x2649408b, 0xe320f000
.section .vitalink.fstubs.SceFios2KernelForDriver, "ax", %progbits
.balign 4
.global SceFios2KernelForDriver_28E28A58
.type SceFios2KernelForDriver_28E28A58, %function
SceFios2KernelForDriver_28E28A58:
.word 0x10, 0x54d6b9eb, 0x28e28a58, 0xe320f000
.section .vitalink.fstubs.SceFios2KernelForDriver, "ax", %progbits
.balign 4
.global SceFios2KernelForDriver_35E7E75C
.type SceFios2KernelForDriver_35E7E75C, %function
SceFios2KernelForDriver_35E7E75C:
.word 0x10, 0x54d6b9eb, 0x35e7e75c, 0xe320f000
.section .vitalink.fstubs.SceFios2KernelForDriver, "ax", %progbits
.balign 4
.global SceFios2KernelForDriver_3B329E86
.type SceFios2KernelForDriver_3B329E86, %function
SceFios2KernelForDriver_3B329E86:
.word 0x10, 0x54d6b9eb, 0x3b329e86, 0xe320f000
.section .vitalink.fstubs.SceFios2KernelForDriver, "ax", %progbits
.balign 4
.global SceFios2KernelForDriver_990F46A5
.type SceFios2KernelForDriver_990F46A5, %function
SceFios2KernelForDriver_990F46A5:
.word 0x10, 0x54d6b9eb, 0x990f46a5, 0xe320f000
.section .vitalink.fstubs.SceFios2KernelForDriver, "ax", %progbits
.balign 4
.global SceFios2KernelForDriver_DD7627EC
.type SceFios2KernelForDriver_DD7627EC, %function
SceFios2KernelForDriver_DD7627EC:
.word 0x10, 0x54d6b9eb, 0xdd7627ec, 0xe320f000
#endif
.section .note.GNU-stack, "", %progbits
