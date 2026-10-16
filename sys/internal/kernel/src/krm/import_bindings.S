.file "import_bindings.S"
.syntax unified
.cpu cortex-a9
#if FW_VERSION >= 0x04000000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelCopyToUser
.type ksceKernelCopyToUser, %function
ksceKernelCopyToUser:
	.word 0x10, 0x6f25e18a, 0x6d88ef8a, 0xe320f000
.size ksceKernelCopyToUser, .-ksceKernelCopyToUser
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelIsAccessibleRange
.type ksceKernelIsAccessibleRange, %function
ksceKernelIsAccessibleRange:
	.word 0x10, 0x6f25e18a, 0x9c78064c, 0xe320f000
.size ksceKernelIsAccessibleRange, .-ksceKernelIsAccessibleRange
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelCopyFromUser
.type ksceKernelCopyFromUser, %function
ksceKernelCopyFromUser:
	.word 0x10, 0x6f25e18a, 0xbc996a7a, 0xe320f000
.size ksceKernelCopyFromUser, .-ksceKernelCopyFromUser
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global memset
.type memset, %function
memset:
	.word 0x10, 0x7ee45391, 0xab9bf5c, 0xe320f000
.size memset, .-memset
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global memcpy
.type memcpy, %function
memcpy:
	.word 0x10, 0x7ee45391, 0x40c88316, 0xe320f000
.size memcpy, .-memcpy
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global __aeabi_uidivmod
.type __aeabi_uidivmod, %function
__aeabi_uidivmod:
	.word 0x10, 0x7ee45391, 0xa46cb7de, 0xe320f000
.size __aeabi_uidivmod, .-__aeabi_uidivmod
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global __aeabi_uidiv
.type __aeabi_uidiv, %function
__aeabi_uidiv:
	.word 0x10, 0x7ee45391, 0xa9ff1205, 0xe320f000
.size __aeabi_uidiv, .-__aeabi_uidiv
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global timingsafe_memcmp
.type timingsafe_memcmp, %function
timingsafe_memcmp:
	.word 0x10, 0x7ee45391, 0xb5a4d745, 0xe320f000
.size timingsafe_memcmp, .-timingsafe_memcmp
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global __stack_chk_fail
.type __stack_chk_fail, %function
__stack_chk_fail:
	.word 0x10, 0x7ee45391, 0xb997493d, 0xe320f000
.size __stack_chk_fail, .-__stack_chk_fail
.section .vitalink.fstubs.SceProcEventForDriver, "ax", %progbits
.balign 4
.global ksceKernelRegisterProcEventHandler
.type ksceKernelRegisterProcEventHandler, %function
ksceKernelRegisterProcEventHandler:
	.word 0x10, 0x887f19d0, 0x2a43912d, 0xe320f000
.size ksceKernelRegisterProcEventHandler, .-ksceKernelRegisterProcEventHandler
.section .vitalink.fstubs.SceSblACMgrForDriver, "ax", %progbits
.balign 4
.global SceSblACMgrForDriver_AE1AF154
.type SceSblACMgrForDriver_AE1AF154, %function
SceSblACMgrForDriver_AE1AF154:
	.word 0x10, 0x9ad8e213, 0xae1af154, 0xe320f000
.size SceSblACMgrForDriver_AE1AF154, .-SceSblACMgrForDriver_AE1AF154
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global SceKernelUtilsForDriver_1DF3792A
.type SceKernelUtilsForDriver_1DF3792A, %function
SceKernelUtilsForDriver_1DF3792A:
	.word 0x10, 0x496ad8b4, 0x1df3792a, 0xe320f000
.size SceKernelUtilsForDriver_1DF3792A, .-SceKernelUtilsForDriver_1DF3792A
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global ksceSha256BlockUpdate
.type ksceSha256BlockUpdate, %function
ksceSha256BlockUpdate:
	.word 0x10, 0x496ad8b4, 0x236a9097, 0xe320f000
.size ksceSha256BlockUpdate, .-ksceSha256BlockUpdate
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global ksceAesEncrypt2
.type ksceAesEncrypt2, %function
ksceAesEncrypt2:
	.word 0x10, 0x496ad8b4, 0x302947b6, 0xe320f000
.size ksceAesEncrypt2, .-ksceAesEncrypt2
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global ksceSha256BlockResult
.type ksceSha256BlockResult, %function
ksceSha256BlockResult:
	.word 0x10, 0x496ad8b4, 0x4899cd4b, 0xe320f000
.size ksceSha256BlockResult, .-ksceSha256BlockResult
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global ksceAesInit3
.type ksceAesInit3, %function
ksceAesInit3:
	.word 0x10, 0x496ad8b4, 0x72408e29, 0xe320f000
.size ksceAesInit3, .-ksceAesInit3
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global ksceSha256BlockInit
.type ksceSha256BlockInit, %function
ksceSha256BlockInit:
	.word 0x10, 0x496ad8b4, 0xd909fa2c, 0xe320f000
.size ksceSha256BlockInit, .-ksceSha256BlockInit
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global ksceAesDecrypt2
.type ksceAesDecrypt2, %function
ksceAesDecrypt2:
	.word 0x10, 0x496ad8b4, 0xe39cd272, 0xe320f000
.size ksceAesDecrypt2, .-ksceAesDecrypt2
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global ksceAesInit2
.type ksceAesInit2, %function
ksceAesInit2:
	.word 0x10, 0x496ad8b4, 0xeda97d6d, 0xe320f000
.size ksceAesInit2, .-ksceAesInit2
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelLockFastMutex
.type ksceKernelLockFastMutex, %function
ksceKernelLockFastMutex:
	.word 0x10, 0xe2c40624, 0x70627f3a, 0xe320f000
.size ksceKernelLockFastMutex, .-ksceKernelLockFastMutex
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelGetProcessId
.type sceKernelGetProcessId, %function
sceKernelGetProcessId:
	.word 0x10, 0xe2c40624, 0x9dcb4b7a, 0xe320f000
.size sceKernelGetProcessId, .-sceKernelGetProcessId
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelInitializeFastMutex
.type ksceKernelInitializeFastMutex, %function
ksceKernelInitializeFastMutex:
	.word 0x10, 0xe2c40624, 0xaf8e1266, 0xe320f000
.size ksceKernelInitializeFastMutex, .-ksceKernelInitializeFastMutex
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelUnlockFastMutex
.type ksceKernelUnlockFastMutex, %function
ksceKernelUnlockFastMutex:
	.word 0x10, 0xe2c40624, 0xdb395782, 0xe320f000
.size ksceKernelUnlockFastMutex, .-ksceKernelUnlockFastMutex
.section .vitalink.fstubs.SceSblSsMgrForDriver, "ax", %progbits
.balign 4
.global ksceSblSsDecryptWithPortability
.type ksceSblSsDecryptWithPortability, %function
ksceSblSsDecryptWithPortability:
	.word 0x10, 0x61e9428d, 0x934db6b5, 0xe320f000
.size ksceSblSsDecryptWithPortability, .-ksceSblSsDecryptWithPortability
.section .vitalink.fstubs.SceSblSsMgrForDriver, "ax", %progbits
.balign 4
.global ksceSblAimgrGetOpenPsId
.type ksceSblAimgrGetOpenPsId, %function
ksceSblAimgrGetOpenPsId:
	.word 0x10, 0x61e9428d, 0xa5b5d269, 0xe320f000
.size ksceSblAimgrGetOpenPsId, .-ksceSblAimgrGetOpenPsId
.section .vitalink.fstubs.SceSblSsMgrForDriver, "ax", %progbits
.balign 4
.global ksceSblRngGenuineRandomNumber
.type ksceSblRngGenuineRandomNumber, %function
ksceSblRngGenuineRandomNumber:
	.word 0x10, 0x61e9428d, 0xac57f4f0, 0xe320f000
.size ksceSblRngGenuineRandomNumber, .-ksceSblRngGenuineRandomNumber
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global ksceKernelGetAssertLevel
.type ksceKernelGetAssertLevel, %function
ksceKernelGetAssertLevel:
	.word 0x10, 0x88758561, 0x374b7868, 0xe320f000
.size ksceKernelGetAssertLevel, .-ksceKernelGetAssertLevel
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global ksceKernelPrintfLevelWithInfo
.type ksceKernelPrintfLevelWithInfo, %function
ksceKernelPrintfLevelWithInfo:
	.word 0x10, 0x88758561, 0xd9703808, 0xe320f000
.size ksceKernelPrintfLevelWithInfo, .-ksceKernelPrintfLevelWithInfo
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global ksceKernelAssertLevel
.type ksceKernelAssertLevel, %function
ksceKernelAssertLevel:
	.word 0x10, 0x88758561, 0xfd753e7a, 0xe320f000
.size ksceKernelAssertLevel, .-ksceKernelAssertLevel
.section .vitalink.fstubs.SceRtcForDriver, "ax", %progbits
.balign 4
.global ksceRtcGetCurrentNetworkTick
.type ksceRtcGetCurrentNetworkTick, %function
ksceRtcGetCurrentNetworkTick:
	.word 0x10, 0x351d827, 0xf1517b38, 0xe320f000
.size ksceRtcGetCurrentNetworkTick, .-ksceRtcGetCurrentNetworkTick
#else
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelCopyToUser
.type ksceKernelCopyToUser, %function
ksceKernelCopyToUser:
	.word 0x10, 0x6f25e18a, 0x6d88ef8a, 0xe320f000
.size ksceKernelCopyToUser, .-ksceKernelCopyToUser
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelIsAccessibleRange
.type ksceKernelIsAccessibleRange, %function
ksceKernelIsAccessibleRange:
	.word 0x10, 0x6f25e18a, 0x9c78064c, 0xe320f000
.size ksceKernelIsAccessibleRange, .-ksceKernelIsAccessibleRange
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global ksceKernelCopyFromUser
.type ksceKernelCopyFromUser, %function
ksceKernelCopyFromUser:
	.word 0x10, 0x6f25e18a, 0xbc996a7a, 0xe320f000
.size ksceKernelCopyFromUser, .-ksceKernelCopyFromUser
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global memset
.type memset, %function
memset:
	.word 0x10, 0x7ee45391, 0xab9bf5c, 0xe320f000
.size memset, .-memset
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global memcpy
.type memcpy, %function
memcpy:
	.word 0x10, 0x7ee45391, 0x40c88316, 0xe320f000
.size memcpy, .-memcpy
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global __aeabi_uidivmod
.type __aeabi_uidivmod, %function
__aeabi_uidivmod:
	.word 0x10, 0x7ee45391, 0xa46cb7de, 0xe320f000
.size __aeabi_uidivmod, .-__aeabi_uidivmod
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global __aeabi_uidiv
.type __aeabi_uidiv, %function
__aeabi_uidiv:
	.word 0x10, 0x7ee45391, 0xa9ff1205, 0xe320f000
.size __aeabi_uidiv, .-__aeabi_uidiv
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global timingsafe_memcmp
.type timingsafe_memcmp, %function
timingsafe_memcmp:
	.word 0x10, 0x7ee45391, 0xb5a4d745, 0xe320f000
.size timingsafe_memcmp, .-timingsafe_memcmp
.section .vitalink.fstubs.SceSysclibForDriver, "ax", %progbits
.balign 4
.global __stack_chk_fail
.type __stack_chk_fail, %function
__stack_chk_fail:
	.word 0x10, 0x7ee45391, 0xb997493d, 0xe320f000
.size __stack_chk_fail, .-__stack_chk_fail
.section .vitalink.fstubs.SceProcEventForDriver, "ax", %progbits
.balign 4
.global ksceKernelRegisterProcEventHandler
.type ksceKernelRegisterProcEventHandler, %function
ksceKernelRegisterProcEventHandler:
	.word 0x10, 0x887f19d0, 0x2a43912d, 0xe320f000
.size ksceKernelRegisterProcEventHandler, .-ksceKernelRegisterProcEventHandler
.section .vitalink.fstubs.SceSblACMgrForDriver, "ax", %progbits
.balign 4
.global SceSblACMgrForDriver_AE1AF154
.type SceSblACMgrForDriver_AE1AF154, %function
SceSblACMgrForDriver_AE1AF154:
	.word 0x10, 0x9ad8e213, 0xae1af154, 0xe320f000
.size SceSblACMgrForDriver_AE1AF154, .-SceSblACMgrForDriver_AE1AF154
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global SceKernelUtilsForDriver_1DF3792A
.type SceKernelUtilsForDriver_1DF3792A, %function
SceKernelUtilsForDriver_1DF3792A:
	.word 0x10, 0x496ad8b4, 0x1df3792a, 0xe320f000
.size SceKernelUtilsForDriver_1DF3792A, .-SceKernelUtilsForDriver_1DF3792A
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global ksceSha256BlockUpdate
.type ksceSha256BlockUpdate, %function
ksceSha256BlockUpdate:
	.word 0x10, 0x496ad8b4, 0x236a9097, 0xe320f000
.size ksceSha256BlockUpdate, .-ksceSha256BlockUpdate
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global ksceAesEncrypt2
.type ksceAesEncrypt2, %function
ksceAesEncrypt2:
	.word 0x10, 0x496ad8b4, 0x302947b6, 0xe320f000
.size ksceAesEncrypt2, .-ksceAesEncrypt2
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global ksceSha256BlockResult
.type ksceSha256BlockResult, %function
ksceSha256BlockResult:
	.word 0x10, 0x496ad8b4, 0x4899cd4b, 0xe320f000
.size ksceSha256BlockResult, .-ksceSha256BlockResult
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global ksceAesInit3
.type ksceAesInit3, %function
ksceAesInit3:
	.word 0x10, 0x496ad8b4, 0x72408e29, 0xe320f000
.size ksceAesInit3, .-ksceAesInit3
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global ksceSha256BlockInit
.type ksceSha256BlockInit, %function
ksceSha256BlockInit:
	.word 0x10, 0x496ad8b4, 0xd909fa2c, 0xe320f000
.size ksceSha256BlockInit, .-ksceSha256BlockInit
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global ksceAesDecrypt2
.type ksceAesDecrypt2, %function
ksceAesDecrypt2:
	.word 0x10, 0x496ad8b4, 0xe39cd272, 0xe320f000
.size ksceAesDecrypt2, .-ksceAesDecrypt2
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global ksceAesInit2
.type ksceAesInit2, %function
ksceAesInit2:
	.word 0x10, 0x496ad8b4, 0xeda97d6d, 0xe320f000
.size ksceAesInit2, .-ksceAesInit2
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelLockFastMutex
.type ksceKernelLockFastMutex, %function
ksceKernelLockFastMutex:
	.word 0x10, 0xe2c40624, 0x70627f3a, 0xe320f000
.size ksceKernelLockFastMutex, .-ksceKernelLockFastMutex
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global sceKernelGetProcessId
.type sceKernelGetProcessId, %function
sceKernelGetProcessId:
	.word 0x10, 0xe2c40624, 0x9dcb4b7a, 0xe320f000
.size sceKernelGetProcessId, .-sceKernelGetProcessId
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelInitializeFastMutex
.type ksceKernelInitializeFastMutex, %function
ksceKernelInitializeFastMutex:
	.word 0x10, 0xe2c40624, 0xaf8e1266, 0xe320f000
.size ksceKernelInitializeFastMutex, .-ksceKernelInitializeFastMutex
.section .vitalink.fstubs.SceThreadmgrForDriver, "ax", %progbits
.balign 4
.global ksceKernelUnlockFastMutex
.type ksceKernelUnlockFastMutex, %function
ksceKernelUnlockFastMutex:
	.word 0x10, 0xe2c40624, 0xdb395782, 0xe320f000
.size ksceKernelUnlockFastMutex, .-ksceKernelUnlockFastMutex
.section .vitalink.fstubs.SceSblSsMgrForDriver, "ax", %progbits
.balign 4
.global ksceSblSsDecryptWithPortability
.type ksceSblSsDecryptWithPortability, %function
ksceSblSsDecryptWithPortability:
	.word 0x10, 0x61e9428d, 0x934db6b5, 0xe320f000
.size ksceSblSsDecryptWithPortability, .-ksceSblSsDecryptWithPortability
.section .vitalink.fstubs.SceSblSsMgrForDriver, "ax", %progbits
.balign 4
.global ksceSblAimgrGetOpenPsId
.type ksceSblAimgrGetOpenPsId, %function
ksceSblAimgrGetOpenPsId:
	.word 0x10, 0x61e9428d, 0xa5b5d269, 0xe320f000
.size ksceSblAimgrGetOpenPsId, .-ksceSblAimgrGetOpenPsId
.section .vitalink.fstubs.SceSblSsMgrForDriver, "ax", %progbits
.balign 4
.global ksceSblRngGenuineRandomNumber
.type ksceSblRngGenuineRandomNumber, %function
ksceSblRngGenuineRandomNumber:
	.word 0x10, 0x61e9428d, 0xac57f4f0, 0xe320f000
.size ksceSblRngGenuineRandomNumber, .-ksceSblRngGenuineRandomNumber
.section .vitalink.fstubs.SceRtcForDriver, "ax", %progbits
.balign 4
.global ksceRtcGetCurrentNetworkTick
.type ksceRtcGetCurrentNetworkTick, %function
ksceRtcGetCurrentNetworkTick:
	.word 0x10, 0x351d827, 0xf1517b38, 0xe320f000
.size ksceRtcGetCurrentNetworkTick, .-ksceRtcGetCurrentNetworkTick
#endif
.section .note.GNU-stack, "", %progbits
