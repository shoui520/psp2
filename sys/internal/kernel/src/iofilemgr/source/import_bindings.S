/* Exact canonical function-import bindings; records are loader metadata. */
.syntax unified
.arm
#if FW_VERSION >= 0x04000000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelGetThreadCurrentPriority
.type ksceKernelGetThreadCurrentPriority, %function
ksceKernelGetThreadCurrentPriority:
.word 0x10, 0xe2c40624, 0x1414f0b, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelSetPermission
.type ksceKernelSetPermission, %function
ksceKernelSetPermission:
.word 0x10, 0xe2c40624, 0x2eedf17, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelSetProcessIdToTLS
.type ksceKernelSetProcessIdToTLS, %function
ksceKernelSetProcessIdToTLS:
.word 0x10, 0xe2c40624, 0x486f239, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelDeleteMutex
.type ksceKernelDeleteMutex, %function
ksceKernelDeleteMutex:
.word 0x10, 0xe2c40624, 0xa912340, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelWaitEventFlag
.type ksceKernelWaitEventFlag, %function
ksceKernelWaitEventFlag:
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
.global ksceKernelFinalizeFastMutex
.type ksceKernelFinalizeFastMutex, %function
ksceKernelFinalizeFastMutex:
.word 0x10, 0xe2c40624, 0x11fe84a1, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelLockMutex
.type ksceKernelLockMutex, %function
ksceKernelLockMutex:
.word 0x10, 0xe2c40624, 0x16ac80c5, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelUnlockMutex
.type ksceKernelUnlockMutex, %function
ksceKernelUnlockMutex:
.word 0x10, 0xe2c40624, 0x1e82e5d0, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelStartThread
.type ksceKernelStartThread, %function
ksceKernelStartThread:
.word 0x10, 0xe2c40624, 0x21f5419b, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelEventObjectInit
.type sceKernelEventObjectInit, %function
sceKernelEventObjectInit:
.word 0x10, 0xe2c40624, 0x25ab8f6b, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelTryLockMutex
.type ksceKernelTryLockMutex, %function
ksceKernelTryLockMutex:
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
.global ksceKernelWaitThreadEnd
.type ksceKernelWaitThreadEnd, %function
ksceKernelWaitThreadEnd:
.word 0x10, 0xe2c40624, 0x3e20216f, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelCreateEventFlag
.type ksceKernelCreateEventFlag, %function
ksceKernelCreateEventFlag:
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
.global ksceKernelClearEventFlag
.type ksceKernelClearEventFlag, %function
ksceKernelClearEventFlag:
.word 0x10, 0xe2c40624, 0x4f1da3be, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelGetThreadId
.type ksceKernelGetThreadId, %function
ksceKernelGetThreadId:
.word 0x10, 0xe2c40624, 0x59d06540, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelSignalCondTo
.type ksceKernelSignalCondTo, %function
ksceKernelSignalCondTo:
.word 0x10, 0xe2c40624, 0x61533da9, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelGetMutexInfo
.type ksceKernelGetMutexInfo, %function
ksceKernelGetMutexInfo:
.word 0x10, 0xe2c40624, 0x69b78a12, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelSignalCondAll
.type ksceKernelSignalCondAll, %function
ksceKernelSignalCondAll:
.word 0x10, 0xe2c40624, 0x6ec78cd0, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelLockFastMutex
.type ksceKernelLockFastMutex, %function
ksceKernelLockFastMutex:
.word 0x10, 0xe2c40624, 0x70627f3a, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelDeleteEventFlag
.type ksceKernelDeleteEventFlag, %function
ksceKernelDeleteEventFlag:
.word 0x10, 0xe2c40624, 0x71ecb352, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelTryLockFastMutex
.type ksceKernelTryLockFastMutex, %function
ksceKernelTryLockFastMutex:
.word 0x10, 0xe2c40624, 0x741f4707, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelGetThreadCpuAffinityMask
.type ksceKernelGetThreadCpuAffinityMask, %function
ksceKernelGetThreadCpuAffinityMask:
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
.global ksceKernelSetEvent
.type ksceKernelSetEvent, %function
ksceKernelSetEvent:
.word 0x10, 0xe2c40624, 0x9ea3a45c, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelExtendKernelStackWide
.type sceKernelExtendKernelStackWide, %function
sceKernelExtendKernelStackWide:
.word 0x10, 0xe2c40624, 0xa2c801a5, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelDeleteThread
.type ksceKernelDeleteThread, %function
ksceKernelDeleteThread:
.word 0x10, 0xe2c40624, 0xac834f3f, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelDeleteCond
.type ksceKernelDeleteCond, %function
ksceKernelDeleteCond:
.word 0x10, 0xe2c40624, 0xaee0d27c, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelInitializeFastMutex
.type ksceKernelInitializeFastMutex, %function
ksceKernelInitializeFastMutex:
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
.global ksceKernelRegisterTimer
.type ksceKernelRegisterTimer, %function
ksceKernelRegisterTimer:
.word 0x10, 0xe2c40624, 0xc58df384, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelCreateThread
.type ksceKernelCreateThread, %function
ksceKernelCreateThread:
.word 0x10, 0xe2c40624, 0xc6674e7d, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelWaitCond
.type ksceKernelWaitCond, %function
ksceKernelWaitCond:
.word 0x10, 0xe2c40624, 0xcc7e027d, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelSetEventFlag
.type ksceKernelSetEventFlag, %function
ksceKernelSetEventFlag:
.word 0x10, 0xe2c40624, 0xd4780c3e, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelGetFastMutexInfo
.type ksceKernelGetFastMutexInfo, %function
ksceKernelGetFastMutexInfo:
.word 0x10, 0xe2c40624, 0xd7af2e58, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelUnlockFastMutex
.type ksceKernelUnlockFastMutex, %function
ksceKernelUnlockFastMutex:
.word 0x10, 0xe2c40624, 0xdb395782, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelCreateCond
.type ksceKernelCreateCond, %function
ksceKernelCreateCond:
.word 0x10, 0xe2c40624, 0xdb6cd34a, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelUIDInheritedEventClassInit
.type sceKernelUIDInheritedEventClassInit, %function
sceKernelUIDInheritedEventClassInit:
.word 0x10, 0xe2c40624, 0xdbb77032, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelRunWithStack
.type ksceKernelRunWithStack, %function
ksceKernelRunWithStack:
.word 0x10, 0xe2c40624, 0xe54fd746, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelGetSystemTimeWide
.type sceKernelGetSystemTimeWide, %function
sceKernelGetSystemTimeWide:
.word 0x10, 0xe2c40624, 0xf4ee4fa9, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelGetProcessIdFromTLS
.type ksceKernelGetProcessIdFromTLS, %function
ksceKernelGetProcessIdFromTLS:
.word 0x10, 0xe2c40624, 0xfa54d49a, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelCreateMutex
.type ksceKernelCreateMutex, %function
ksceKernelCreateMutex:
.word 0x10, 0xe2c40624, 0xfbaa026e, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelUserUnmap
.type ksceKernelUserUnmap, %function
ksceKernelUserUnmap:
.word 0x10, 0x6f25e18a, 0x575b00, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelFreeMemBlock
.type ksceKernelFreeMemBlock, %function
ksceKernelFreeMemBlock:
.word 0x10, 0x6f25e18a, 0x9e1c61, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceGUIDReferObjectWithClass
.type ksceGUIDReferObjectWithClass, %function
ksceGUIDReferObjectWithClass:
.word 0x10, 0x6f25e18a, 0xed6c14, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceGUIDClose
.type ksceGUIDClose, %function
ksceGUIDClose:
.word 0x10, 0x6f25e18a, 0x47d32f2, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceGUIDReleaseObject
.type ksceGUIDReleaseObject, %function
ksceGUIDReleaseObject:
.word 0x10, 0x6f25e18a, 0x149885c4, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelFreeHeapMemory
.type ksceKernelFreeHeapMemory, %function
ksceKernelFreeHeapMemory:
.word 0x10, 0x6f25e18a, 0x3ebce343, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global kscePUIDtoGUID
.type kscePUIDtoGUID, %function
kscePUIDtoGUID:
.word 0x10, 0x6f25e18a, 0x45d22597, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceGUIDKernelCreate
.type ksceGUIDKernelCreate, %function
ksceGUIDKernelCreate:
.word 0x10, 0x6f25e18a, 0x56a13e90, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelCopyFromUserProc
.type ksceKernelCopyFromUserProc, %function
ksceKernelCopyFromUserProc:
.word 0x10, 0x6f25e18a, 0x605275f8, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceUIDClassInitClass
.type ksceUIDClassInitClass, %function
ksceUIDClassInitClass:
.word 0x10, 0x6f25e18a, 0x61317102, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelCopyToUserProc
.type ksceKernelCopyToUserProc, %function
ksceKernelCopyToUserProc:
.word 0x10, 0x6f25e18a, 0x6b825479, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelCopyToUser
.type ksceKernelCopyToUser, %function
ksceKernelCopyToUser:
.word 0x10, 0x6f25e18a, 0x6d88ef8a, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelAllocHeapMemory
.type ksceKernelAllocHeapMemory, %function
ksceKernelAllocHeapMemory:
.word 0x10, 0x6f25e18a, 0x7b4cb60a, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelUserMapWithFlags
.type ksceKernelUserMapWithFlags, %function
ksceKernelUserMapWithFlags:
.word 0x10, 0x6f25e18a, 0x7d4f8b5f, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global kscePUIDClose
.type kscePUIDClose, %function
kscePUIDClose:
.word 0x10, 0x6f25e18a, 0x84a4af5e, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelGetUIDClass
.type ksceKernelGetUIDClass, %function
ksceKernelGetUIDClass:
.word 0x10, 0x6f25e18a, 0x85336a1c, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelCreateHeap
.type ksceKernelCreateHeap, %function
ksceKernelCreateHeap:
.word 0x10, 0x6f25e18a, 0x9328e0e8, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelGetMemBlockBase
.type ksceKernelGetMemBlockBase, %function
ksceKernelGetMemBlockBase:
.word 0x10, 0x6f25e18a, 0xa841edda, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelCopyFromUser
.type ksceKernelCopyFromUser, %function
ksceKernelCopyFromUser:
.word 0x10, 0x6f25e18a, 0xbc996a7a, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global kscePUIDOpenByGUID
.type kscePUIDOpenByGUID, %function
kscePUIDOpenByGUID:
.word 0x10, 0x6f25e18a, 0xbf209859, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelAllocMemBlock
.type ksceKernelAllocMemBlock, %function
ksceKernelAllocMemBlock:
.word 0x10, 0x6f25e18a, 0xc94850c9, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelDeleteHeap
.type ksceKernelDeleteHeap, %function
ksceKernelDeleteHeap:
.word 0x10, 0x6f25e18a, 0xd6437637, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelStrncpyFromUser
.type ksceKernelStrncpyFromUser, %function
ksceKernelStrncpyFromUser:
.word 0x10, 0x6f25e18a, 0xdb3ec244, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global SceSysmemForKernel_08C05493
.type SceSysmemForKernel_08C05493, %function
SceSysmemForKernel_08C05493:
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
.global ksceGUIDGetUIDVectorByClass
.type ksceGUIDGetUIDVectorByClass, %function
ksceGUIDGetUIDVectorByClass:
.word 0x10, 0x2451f0f, 0x52137fa3, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global SceSysmemForKernel_7F7D5C4A
.type SceSysmemForKernel_7F7D5C4A, %function
SceSysmemForKernel_7F7D5C4A:
.word 0x10, 0x2451f0f, 0x7f7d5c4a, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global ksceGUIDOpenByGUID
.type ksceGUIDOpenByGUID, %function
ksceGUIDOpenByGUID:
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
.global ksceGUIDKernelCreateWithOpt
.type ksceGUIDKernelCreateWithOpt, %function
ksceGUIDKernelCreateWithOpt:
.word 0x10, 0x2451f0f, 0xfb6390ce, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global ksceKernelAtomicGetAndAdd32
.type ksceKernelAtomicGetAndAdd32, %function
ksceKernelAtomicGetAndAdd32:
.word 0x10, 0x40ecdb0e, 0x341b6e81, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global ksceKernelAtomicAddAndGet32
.type ksceKernelAtomicAddAndGet32, %function
ksceKernelAtomicAddAndGet32:
.word 0x10, 0x40ecdb0e, 0x5f6a8743, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global ksceKernelSpinlockLowUnlockCpuResumeIntr
.type ksceKernelSpinlockLowUnlockCpuResumeIntr, %function
ksceKernelSpinlockLowUnlockCpuResumeIntr:
.word 0x10, 0x40ecdb0e, 0x7bb9d5df, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global ksceKernelAtomicSubAndGet32
.type ksceKernelAtomicSubAndGet32, %function
ksceKernelAtomicSubAndGet32:
.word 0x10, 0x40ecdb0e, 0xa4884c4e, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global ksceKernelAtomicCompareAndSet32
.type ksceKernelAtomicCompareAndSet32, %function
ksceKernelAtomicCompareAndSet32:
.word 0x10, 0x40ecdb0e, 0xcda96e81, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global ksceKernelSpinlockLowLockCpuSuspendIntr
.type ksceKernelSpinlockLowLockCpuSuspendIntr, %function
ksceKernelSpinlockLowLockCpuSuspendIntr:
.word 0x10, 0x40ecdb0e, 0xd32ace9e, 0xe320f000
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global ksceKernelPrintfWithInfo
.type ksceKernelPrintfWithInfo, %function
ksceKernelPrintfWithInfo:
.word 0x10, 0x88758561, 0x2b04343, 0xe320f000
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global ksceKernelAssert
.type ksceKernelAssert, %function
ksceKernelAssert:
.word 0x10, 0x88758561, 0x35a35322, 0xe320f000
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global ksceKernelGetAssertLevel
.type ksceKernelGetAssertLevel, %function
ksceKernelGetAssertLevel:
.word 0x10, 0x88758561, 0x374b7868, 0xe320f000
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global ksceKernelPanic
.type ksceKernelPanic, %function
ksceKernelPanic:
.word 0x10, 0x88758561, 0x391b5b74, 0xe320f000
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global ksceKernelPrintf
.type ksceKernelPrintf, %function
ksceKernelPrintf:
.word 0x10, 0x88758561, 0x391b74b7, 0xe320f000
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global ksceKernelPrintfLevelWithInfo
.type ksceKernelPrintfLevelWithInfo, %function
ksceKernelPrintfLevelWithInfo:
.word 0x10, 0x88758561, 0xd9703808, 0xe320f000
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global ksceKernelAssertLevel
.type ksceKernelAssertLevel, %function
ksceKernelAssertLevel:
.word 0x10, 0x88758561, 0xfd753e7a, 0xe320f000
.section .vitalink.fstubs.SceDebugForKernel, "ax", %progbits
.balign 4
.global ksceKernelDebugPutchar
.type ksceKernelDebugPutchar, %function
ksceKernelDebugPutchar:
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
.global ksceKernelCreateProcessLocalStorage
.type ksceKernelCreateProcessLocalStorage, %function
ksceKernelCreateProcessLocalStorage:
.word 0x10, 0x746ec971, 0x3801d7d6, 0xe320f000
.section .vitalink.fstubs.SceProcessmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelAllocRemoteCDialogHeap
.type sceKernelAllocRemoteCDialogHeap, %function
sceKernelAllocRemoteCDialogHeap:
.word 0x10, 0x746ec971, 0x8a85fa28, 0xe320f000
.section .vitalink.fstubs.SceProcessmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelGetProcessLocalStorageAddrForPid
.type ksceKernelGetProcessLocalStorageAddrForPid, %function
ksceKernelGetProcessLocalStorageAddrForPid:
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
.global ksceSblACMgrIsGameProgram2
.type ksceSblACMgrIsGameProgram2, %function
ksceSblACMgrIsGameProgram2:
.word 0x10, 0x11f9b314, 0x5fdc646, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global SceSblACMgrForKernel_1B160234
.type SceSblACMgrForKernel_1B160234, %function
SceSblACMgrForKernel_1B160234:
.word 0x10, 0x11f9b314, 0x1b160234, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global ksceSblACMgrIsRootProgram2
.type ksceSblACMgrIsRootProgram2, %function
ksceSblACMgrIsRootProgram2:
.word 0x10, 0x11f9b314, 0x31c23b66, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global ksceSblACMgrIsNonGameProgram2
.type ksceSblACMgrIsNonGameProgram2, %function
ksceSblACMgrIsNonGameProgram2:
.word 0x10, 0x11f9b314, 0x3f99279f, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global ksceSblACMgrIsNonGameOrGameProgram2
.type ksceSblACMgrIsNonGameOrGameProgram2, %function
ksceSblACMgrIsNonGameOrGameProgram2:
.word 0x10, 0x11f9b314, 0x570d6ad3, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global SceSblACMgrForKernel_75AAF981
.type SceSblACMgrForKernel_75AAF981, %function
SceSblACMgrForKernel_75AAF981:
.word 0x10, 0x11f9b314, 0x75aaf981, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global ksceSblACMgrGetProcessProgramAuthId2
.type ksceSblACMgrGetProcessProgramAuthId2, %function
ksceSblACMgrGetProcessProgramAuthId2:
.word 0x10, 0x11f9b314, 0x7c2af978, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global ksceSblACIsSystemProgram
.type ksceSblACIsSystemProgram, %function
ksceSblACIsSystemProgram:
.word 0x10, 0x11f9b314, 0x930cd037, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global ksceSblACMgrGetMediaType2
.type ksceSblACMgrGetMediaType2, %function
ksceSblACMgrGetMediaType2:
.word 0x10, 0x11f9b314, 0xd442962e, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global ksceSysrootIsNonRemovableCardMode
.type ksceSysrootIsNonRemovableCardMode, %function
ksceSysrootIsNonRemovableCardMode:
.word 0x10, 0x3691da45, 0x50fe3b4d, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global ksceSysrootIsManufacturingMode
.type ksceSysrootIsManufacturingMode, %function
ksceSysrootIsManufacturingMode:
.word 0x10, 0x3691da45, 0x55392965, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global SceSysrootForKernel_7968F091
.type SceSysrootForKernel_7968F091, %function
SceSysrootForKernel_7968F091:
.word 0x10, 0x3691da45, 0x7968f091, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global SceSysrootForKernel_ksceSysrootIsSafeMode
.type SceSysrootForKernel_ksceSysrootIsSafeMode, %function
SceSysrootForKernel_ksceSysrootIsSafeMode:
.word 0x10, 0x3691da45, 0x834439a7, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global SceSysrootForKernel_ksceSysrootIsUpdateMode
.type SceSysrootForKernel_ksceSysrootIsUpdateMode, %function
SceSysrootForKernel_ksceSysrootIsUpdateMode:
.word 0x10, 0x3691da45, 0xb0e1fc67, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global SceSysrootForKernel_CBD377B7
.type SceSysrootForKernel_CBD377B7, %function
SceSysrootForKernel_CBD377B7:
.word 0x10, 0x3691da45, 0xcbd377b7, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global ksceKernelAllocHeapMemory2
.type ksceKernelAllocHeapMemory2, %function
ksceKernelAllocHeapMemory2:
.word 0x10, 0x3691da45, 0xd351ebc8, 0xe320f000
.section .vitalink.fstubs.SceSysrootForDriver, "ax", %progbits
.balign 4
.global ksceKernelSysrootGetShellPid
.type ksceKernelSysrootGetShellPid, %function
ksceKernelSysrootGetShellPid:
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
.global ksceKernelRegisterProcEventHandler
.type ksceKernelRegisterProcEventHandler, %function
ksceKernelRegisterProcEventHandler:
.word 0x10, 0x887f19d0, 0x2a43912d, 0xe320f000
.section .vitalink.fstubs.SceDipswForDriver, "ax", %progbits
.balign 4
.global ksceKernelCheckDipsw
.type ksceKernelCheckDipsw, %function
ksceKernelCheckDipsw:
.word 0x10, 0xc9e26388, 0xa98fc2fd, 0xe320f000
.section .vitalink.fstubs.SceKernelSuspendForDriver, "ax", %progbits
.balign 4
.global ksceKernelRegisterSysEventHandler
.type ksceKernelRegisterSysEventHandler, %function
ksceKernelRegisterSysEventHandler:
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
.global ksceFiosKernelOverlayResolveSync
.type ksceFiosKernelOverlayResolveSync, %function
ksceFiosKernelOverlayResolveSync:
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
.global ksceKernelGetThreadCurrentPriority
.type ksceKernelGetThreadCurrentPriority, %function
ksceKernelGetThreadCurrentPriority:
.word 0x10, 0xe2c40624, 0x1414f0b, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelSetPermission
.type ksceKernelSetPermission, %function
ksceKernelSetPermission:
.word 0x10, 0xe2c40624, 0x2eedf17, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelSetProcessIdToTLS
.type ksceKernelSetProcessIdToTLS, %function
ksceKernelSetProcessIdToTLS:
.word 0x10, 0xe2c40624, 0x486f239, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelDeleteMutex
.type ksceKernelDeleteMutex, %function
ksceKernelDeleteMutex:
.word 0x10, 0xe2c40624, 0xa912340, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelWaitEventFlag
.type ksceKernelWaitEventFlag, %function
ksceKernelWaitEventFlag:
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
.global ksceKernelFinalizeFastMutex
.type ksceKernelFinalizeFastMutex, %function
ksceKernelFinalizeFastMutex:
.word 0x10, 0xe2c40624, 0x11fe84a1, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelLockMutex
.type ksceKernelLockMutex, %function
ksceKernelLockMutex:
.word 0x10, 0xe2c40624, 0x16ac80c5, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelUnlockMutex
.type ksceKernelUnlockMutex, %function
ksceKernelUnlockMutex:
.word 0x10, 0xe2c40624, 0x1e82e5d0, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelStartThread
.type ksceKernelStartThread, %function
ksceKernelStartThread:
.word 0x10, 0xe2c40624, 0x21f5419b, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelEventObjectInit
.type sceKernelEventObjectInit, %function
sceKernelEventObjectInit:
.word 0x10, 0xe2c40624, 0x25ab8f6b, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelTryLockMutex
.type ksceKernelTryLockMutex, %function
ksceKernelTryLockMutex:
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
.global ksceKernelWaitThreadEnd
.type ksceKernelWaitThreadEnd, %function
ksceKernelWaitThreadEnd:
.word 0x10, 0xe2c40624, 0x3e20216f, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelCreateEventFlag
.type ksceKernelCreateEventFlag, %function
ksceKernelCreateEventFlag:
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
.global ksceKernelClearEventFlag
.type ksceKernelClearEventFlag, %function
ksceKernelClearEventFlag:
.word 0x10, 0xe2c40624, 0x4f1da3be, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelGetThreadId
.type ksceKernelGetThreadId, %function
ksceKernelGetThreadId:
.word 0x10, 0xe2c40624, 0x59d06540, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelSignalCondTo
.type ksceKernelSignalCondTo, %function
ksceKernelSignalCondTo:
.word 0x10, 0xe2c40624, 0x61533da9, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelGetMutexInfo
.type ksceKernelGetMutexInfo, %function
ksceKernelGetMutexInfo:
.word 0x10, 0xe2c40624, 0x69b78a12, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelSignalCondAll
.type ksceKernelSignalCondAll, %function
ksceKernelSignalCondAll:
.word 0x10, 0xe2c40624, 0x6ec78cd0, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelLockFastMutex
.type ksceKernelLockFastMutex, %function
ksceKernelLockFastMutex:
.word 0x10, 0xe2c40624, 0x70627f3a, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelDeleteEventFlag
.type ksceKernelDeleteEventFlag, %function
ksceKernelDeleteEventFlag:
.word 0x10, 0xe2c40624, 0x71ecb352, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelTryLockFastMutex
.type ksceKernelTryLockFastMutex, %function
ksceKernelTryLockFastMutex:
.word 0x10, 0xe2c40624, 0x741f4707, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelGetThreadCpuAffinityMask
.type ksceKernelGetThreadCpuAffinityMask, %function
ksceKernelGetThreadCpuAffinityMask:
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
.global ksceKernelSetEvent
.type ksceKernelSetEvent, %function
ksceKernelSetEvent:
.word 0x10, 0xe2c40624, 0x9ea3a45c, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelExtendKernelStackWide
.type sceKernelExtendKernelStackWide, %function
sceKernelExtendKernelStackWide:
.word 0x10, 0xe2c40624, 0xa2c801a5, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelDeleteThread
.type ksceKernelDeleteThread, %function
ksceKernelDeleteThread:
.word 0x10, 0xe2c40624, 0xac834f3f, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelDeleteCond
.type ksceKernelDeleteCond, %function
ksceKernelDeleteCond:
.word 0x10, 0xe2c40624, 0xaee0d27c, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelInitializeFastMutex
.type ksceKernelInitializeFastMutex, %function
ksceKernelInitializeFastMutex:
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
.global ksceKernelRegisterTimer
.type ksceKernelRegisterTimer, %function
ksceKernelRegisterTimer:
.word 0x10, 0xe2c40624, 0xc58df384, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelCreateThread
.type ksceKernelCreateThread, %function
ksceKernelCreateThread:
.word 0x10, 0xe2c40624, 0xc6674e7d, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelWaitCond
.type ksceKernelWaitCond, %function
ksceKernelWaitCond:
.word 0x10, 0xe2c40624, 0xcc7e027d, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelSetEventFlag
.type ksceKernelSetEventFlag, %function
ksceKernelSetEventFlag:
.word 0x10, 0xe2c40624, 0xd4780c3e, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelGetFastMutexInfo
.type ksceKernelGetFastMutexInfo, %function
ksceKernelGetFastMutexInfo:
.word 0x10, 0xe2c40624, 0xd7af2e58, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelUnlockFastMutex
.type ksceKernelUnlockFastMutex, %function
ksceKernelUnlockFastMutex:
.word 0x10, 0xe2c40624, 0xdb395782, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelCreateCond
.type ksceKernelCreateCond, %function
ksceKernelCreateCond:
.word 0x10, 0xe2c40624, 0xdb6cd34a, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelUIDInheritedEventClassInit
.type sceKernelUIDInheritedEventClassInit, %function
sceKernelUIDInheritedEventClassInit:
.word 0x10, 0xe2c40624, 0xdbb77032, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelRunWithStack
.type ksceKernelRunWithStack, %function
ksceKernelRunWithStack:
.word 0x10, 0xe2c40624, 0xe54fd746, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelGetSystemTimeWide
.type sceKernelGetSystemTimeWide, %function
sceKernelGetSystemTimeWide:
.word 0x10, 0xe2c40624, 0xf4ee4fa9, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelGetProcessIdFromTLS
.type ksceKernelGetProcessIdFromTLS, %function
ksceKernelGetProcessIdFromTLS:
.word 0x10, 0xe2c40624, 0xfa54d49a, 0xe320f000
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelCreateMutex
.type ksceKernelCreateMutex, %function
ksceKernelCreateMutex:
.word 0x10, 0xe2c40624, 0xfbaa026e, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelUserUnmap
.type ksceKernelUserUnmap, %function
ksceKernelUserUnmap:
.word 0x10, 0x6f25e18a, 0x575b00, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelFreeMemBlock
.type ksceKernelFreeMemBlock, %function
ksceKernelFreeMemBlock:
.word 0x10, 0x6f25e18a, 0x9e1c61, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceGUIDReferObjectWithClass
.type ksceGUIDReferObjectWithClass, %function
ksceGUIDReferObjectWithClass:
.word 0x10, 0x6f25e18a, 0xed6c14, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceGUIDClose
.type ksceGUIDClose, %function
ksceGUIDClose:
.word 0x10, 0x6f25e18a, 0x47d32f2, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceGUIDReleaseObject
.type ksceGUIDReleaseObject, %function
ksceGUIDReleaseObject:
.word 0x10, 0x6f25e18a, 0x149885c4, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelFreeHeapMemory
.type ksceKernelFreeHeapMemory, %function
ksceKernelFreeHeapMemory:
.word 0x10, 0x6f25e18a, 0x3ebce343, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global kscePUIDtoGUID
.type kscePUIDtoGUID, %function
kscePUIDtoGUID:
.word 0x10, 0x6f25e18a, 0x45d22597, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceGUIDKernelCreate
.type ksceGUIDKernelCreate, %function
ksceGUIDKernelCreate:
.word 0x10, 0x6f25e18a, 0x56a13e90, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelCopyFromUserProc
.type ksceKernelCopyFromUserProc, %function
ksceKernelCopyFromUserProc:
.word 0x10, 0x6f25e18a, 0x605275f8, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceUIDClassInitClass
.type ksceUIDClassInitClass, %function
ksceUIDClassInitClass:
.word 0x10, 0x6f25e18a, 0x61317102, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelCopyToUserProc
.type ksceKernelCopyToUserProc, %function
ksceKernelCopyToUserProc:
.word 0x10, 0x6f25e18a, 0x6b825479, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelCopyToUser
.type ksceKernelCopyToUser, %function
ksceKernelCopyToUser:
.word 0x10, 0x6f25e18a, 0x6d88ef8a, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelAllocHeapMemory
.type ksceKernelAllocHeapMemory, %function
ksceKernelAllocHeapMemory:
.word 0x10, 0x6f25e18a, 0x7b4cb60a, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelUserMapWithFlags
.type ksceKernelUserMapWithFlags, %function
ksceKernelUserMapWithFlags:
.word 0x10, 0x6f25e18a, 0x7d4f8b5f, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global kscePUIDClose
.type kscePUIDClose, %function
kscePUIDClose:
.word 0x10, 0x6f25e18a, 0x84a4af5e, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelGetUIDClass
.type ksceKernelGetUIDClass, %function
ksceKernelGetUIDClass:
.word 0x10, 0x6f25e18a, 0x85336a1c, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelCreateHeap
.type ksceKernelCreateHeap, %function
ksceKernelCreateHeap:
.word 0x10, 0x6f25e18a, 0x9328e0e8, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelGetMemBlockBase
.type ksceKernelGetMemBlockBase, %function
ksceKernelGetMemBlockBase:
.word 0x10, 0x6f25e18a, 0xa841edda, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelCopyFromUser
.type ksceKernelCopyFromUser, %function
ksceKernelCopyFromUser:
.word 0x10, 0x6f25e18a, 0xbc996a7a, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global kscePUIDOpenByGUID
.type kscePUIDOpenByGUID, %function
kscePUIDOpenByGUID:
.word 0x10, 0x6f25e18a, 0xbf209859, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelAllocMemBlock
.type ksceKernelAllocMemBlock, %function
ksceKernelAllocMemBlock:
.word 0x10, 0x6f25e18a, 0xc94850c9, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelDeleteHeap
.type ksceKernelDeleteHeap, %function
ksceKernelDeleteHeap:
.word 0x10, 0x6f25e18a, 0xd6437637, 0xe320f000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelStrncpyFromUser
.type ksceKernelStrncpyFromUser, %function
ksceKernelStrncpyFromUser:
.word 0x10, 0x6f25e18a, 0xdb3ec244, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global SceSysmemForKernel_08C05493
.type SceSysmemForKernel_08C05493, %function
SceSysmemForKernel_08C05493:
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
.global ksceGUIDGetUIDVectorByClass
.type ksceGUIDGetUIDVectorByClass, %function
ksceGUIDGetUIDVectorByClass:
.word 0x10, 0x2451f0f, 0x52137fa3, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global SceSysmemForKernel_7F7D5C4A
.type SceSysmemForKernel_7F7D5C4A, %function
SceSysmemForKernel_7F7D5C4A:
.word 0x10, 0x2451f0f, 0x7f7d5c4a, 0xe320f000
.section .vitalink.fstubs.SceSysmemForKernel, "ax", %progbits
.balign 4
.global ksceGUIDOpenByGUID
.type ksceGUIDOpenByGUID, %function
ksceGUIDOpenByGUID:
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
.global ksceGUIDKernelCreateWithOpt
.type ksceGUIDKernelCreateWithOpt, %function
ksceGUIDKernelCreateWithOpt:
.word 0x10, 0x2451f0f, 0xfb6390ce, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global ksceKernelAtomicGetAndAdd32
.type ksceKernelAtomicGetAndAdd32, %function
ksceKernelAtomicGetAndAdd32:
.word 0x10, 0x40ecdb0e, 0x341b6e81, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global ksceKernelAtomicAddAndGet32
.type ksceKernelAtomicAddAndGet32, %function
ksceKernelAtomicAddAndGet32:
.word 0x10, 0x40ecdb0e, 0x5f6a8743, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global ksceKernelSpinlockLowUnlockCpuResumeIntr
.type ksceKernelSpinlockLowUnlockCpuResumeIntr, %function
ksceKernelSpinlockLowUnlockCpuResumeIntr:
.word 0x10, 0x40ecdb0e, 0x7bb9d5df, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global ksceKernelAtomicSubAndGet32
.type ksceKernelAtomicSubAndGet32, %function
ksceKernelAtomicSubAndGet32:
.word 0x10, 0x40ecdb0e, 0xa4884c4e, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global ksceKernelAtomicCompareAndSet32
.type ksceKernelAtomicCompareAndSet32, %function
ksceKernelAtomicCompareAndSet32:
.word 0x10, 0x40ecdb0e, 0xcda96e81, 0xe320f000
.section .vitalink.fstubs.SceCpuForDriver, "ax", %progbits
.balign 4
.global ksceKernelSpinlockLowLockCpuSuspendIntr
.type ksceKernelSpinlockLowLockCpuSuspendIntr, %function
ksceKernelSpinlockLowLockCpuSuspendIntr:
.word 0x10, 0x40ecdb0e, 0xd32ace9e, 0xe320f000
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global ksceKernelPrintfWithInfo
.type ksceKernelPrintfWithInfo, %function
ksceKernelPrintfWithInfo:
.word 0x10, 0x88758561, 0x2b04343, 0xe320f000
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global ksceKernelPanic
.type ksceKernelPanic, %function
ksceKernelPanic:
.word 0x10, 0x88758561, 0x391b5b74, 0xe320f000
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global ksceKernelPrintf
.type ksceKernelPrintf, %function
ksceKernelPrintf:
.word 0x10, 0x88758561, 0x391b74b7, 0xe320f000
.section .vitalink.fstubs.SceDebugForKernel, "ax", %progbits
.balign 4
.global ksceKernelDebugPutchar
.type ksceKernelDebugPutchar, %function
ksceKernelDebugPutchar:
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
.global ksceKernelCreateProcessLocalStorage
.type ksceKernelCreateProcessLocalStorage, %function
ksceKernelCreateProcessLocalStorage:
.word 0x10, 0x746ec971, 0x3801d7d6, 0xe320f000
.section .vitalink.fstubs.SceProcessmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelAllocRemoteCDialogHeap
.type sceKernelAllocRemoteCDialogHeap, %function
sceKernelAllocRemoteCDialogHeap:
.word 0x10, 0x746ec971, 0x8a85fa28, 0xe320f000
.section .vitalink.fstubs.SceProcessmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelGetProcessLocalStorageAddrForPid
.type ksceKernelGetProcessLocalStorageAddrForPid, %function
ksceKernelGetProcessLocalStorageAddrForPid:
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
.global ksceSblACMgrIsGameProgram2
.type ksceSblACMgrIsGameProgram2, %function
ksceSblACMgrIsGameProgram2:
.word 0x10, 0x11f9b314, 0x5fdc646, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global SceSblACMgrForKernel_1B160234
.type SceSblACMgrForKernel_1B160234, %function
SceSblACMgrForKernel_1B160234:
.word 0x10, 0x11f9b314, 0x1b160234, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global ksceSblACMgrIsRootProgram2
.type ksceSblACMgrIsRootProgram2, %function
ksceSblACMgrIsRootProgram2:
.word 0x10, 0x11f9b314, 0x31c23b66, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global ksceSblACMgrIsNonGameProgram2
.type ksceSblACMgrIsNonGameProgram2, %function
ksceSblACMgrIsNonGameProgram2:
.word 0x10, 0x11f9b314, 0x3f99279f, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global ksceSblACMgrIsNonGameOrGameProgram2
.type ksceSblACMgrIsNonGameOrGameProgram2, %function
ksceSblACMgrIsNonGameOrGameProgram2:
.word 0x10, 0x11f9b314, 0x570d6ad3, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global SceSblACMgrForKernel_75AAF981
.type SceSblACMgrForKernel_75AAF981, %function
SceSblACMgrForKernel_75AAF981:
.word 0x10, 0x11f9b314, 0x75aaf981, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global ksceSblACMgrGetProcessProgramAuthId2
.type ksceSblACMgrGetProcessProgramAuthId2, %function
ksceSblACMgrGetProcessProgramAuthId2:
.word 0x10, 0x11f9b314, 0x7c2af978, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global ksceSblACIsSystemProgram
.type ksceSblACIsSystemProgram, %function
ksceSblACIsSystemProgram:
.word 0x10, 0x11f9b314, 0x930cd037, 0xe320f000
.section .vitalink.fstubs.SceSblACMgrForKernel, "ax", %progbits
.balign 4
.global ksceSblACMgrGetMediaType2
.type ksceSblACMgrGetMediaType2, %function
ksceSblACMgrGetMediaType2:
.word 0x10, 0x11f9b314, 0xd442962e, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global ksceSysrootIsNonRemovableCardMode
.type ksceSysrootIsNonRemovableCardMode, %function
ksceSysrootIsNonRemovableCardMode:
.word 0x10, 0x3691da45, 0x50fe3b4d, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global ksceSysrootIsManufacturingMode
.type ksceSysrootIsManufacturingMode, %function
ksceSysrootIsManufacturingMode:
.word 0x10, 0x3691da45, 0x55392965, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global SceSysrootForKernel_7968F091
.type SceSysrootForKernel_7968F091, %function
SceSysrootForKernel_7968F091:
.word 0x10, 0x3691da45, 0x7968f091, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global SceSysrootForKernel_ksceSysrootIsSafeMode
.type SceSysrootForKernel_ksceSysrootIsSafeMode, %function
SceSysrootForKernel_ksceSysrootIsSafeMode:
.word 0x10, 0x3691da45, 0x834439a7, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global SceSysrootForKernel_ksceSysrootIsUpdateMode
.type SceSysrootForKernel_ksceSysrootIsUpdateMode, %function
SceSysrootForKernel_ksceSysrootIsUpdateMode:
.word 0x10, 0x3691da45, 0xb0e1fc67, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global SceSysrootForKernel_CBD377B7
.type SceSysrootForKernel_CBD377B7, %function
SceSysrootForKernel_CBD377B7:
.word 0x10, 0x3691da45, 0xcbd377b7, 0xe320f000
.section .vitalink.fstubs.SceSysrootForKernel, "ax", %progbits
.balign 4
.global ksceKernelAllocHeapMemory2
.type ksceKernelAllocHeapMemory2, %function
ksceKernelAllocHeapMemory2:
.word 0x10, 0x3691da45, 0xd351ebc8, 0xe320f000
.section .vitalink.fstubs.SceSysrootForDriver, "ax", %progbits
.balign 4
.global ksceKernelSysrootGetShellPid
.type ksceKernelSysrootGetShellPid, %function
ksceKernelSysrootGetShellPid:
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
.global ksceKernelRegisterProcEventHandler
.type ksceKernelRegisterProcEventHandler, %function
ksceKernelRegisterProcEventHandler:
.word 0x10, 0x887f19d0, 0x2a43912d, 0xe320f000
.section .vitalink.fstubs.SceDipswForDriver, "ax", %progbits
.balign 4
.global ksceKernelCheckDipsw
.type ksceKernelCheckDipsw, %function
ksceKernelCheckDipsw:
.word 0x10, 0xc9e26388, 0xa98fc2fd, 0xe320f000
.section .vitalink.fstubs.SceKernelSuspendForDriver, "ax", %progbits
.balign 4
.global ksceKernelRegisterSysEventHandler
.type ksceKernelRegisterSysEventHandler, %function
ksceKernelRegisterSysEventHandler:
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
.global ksceFiosKernelOverlayResolveSync
.type ksceFiosKernelOverlayResolveSync, %function
ksceFiosKernelOverlayResolveSync:
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
