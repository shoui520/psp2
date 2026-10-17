.file "import_bindings.S"
.syntax unified
.cpu cortex-a9
#if FW_VERSION >= 0x04000000
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global SceSysmemForDriver_6D88EF8A
.type SceSysmemForDriver_6D88EF8A, %function
SceSysmemForDriver_6D88EF8A:
	.word 0x10, 0x6f25e18a, 0x6d88ef8a, 0xe320f000
.size SceSysmemForDriver_6D88EF8A, .-SceSysmemForDriver_6D88EF8A
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global SceSysmemForDriver_9C78064C
.type SceSysmemForDriver_9C78064C, %function
SceSysmemForDriver_9C78064C:
	.word 0x10, 0x6f25e18a, 0x9c78064c, 0xe320f000
.size SceSysmemForDriver_9C78064C, .-SceSysmemForDriver_9C78064C
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global SceSysmemForDriver_BC996A7A
.type SceSysmemForDriver_BC996A7A, %function
SceSysmemForDriver_BC996A7A:
	.word 0x10, 0x6f25e18a, 0xbc996a7a, 0xe320f000
.size SceSysmemForDriver_BC996A7A, .-SceSysmemForDriver_BC996A7A
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
.global sceKernelRegisterProcEventHandler
.type sceKernelRegisterProcEventHandler, %function
sceKernelRegisterProcEventHandler:
	.word 0x10, 0x887f19d0, 0x2a43912d, 0xe320f000
.size sceKernelRegisterProcEventHandler, .-sceKernelRegisterProcEventHandler
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
.global SceKernelUtilsForDriver_236A9097
.type SceKernelUtilsForDriver_236A9097, %function
SceKernelUtilsForDriver_236A9097:
	.word 0x10, 0x496ad8b4, 0x236a9097, 0xe320f000
.size SceKernelUtilsForDriver_236A9097, .-SceKernelUtilsForDriver_236A9097
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global SceKernelUtilsForDriver_302947B6
.type SceKernelUtilsForDriver_302947B6, %function
SceKernelUtilsForDriver_302947B6:
	.word 0x10, 0x496ad8b4, 0x302947b6, 0xe320f000
.size SceKernelUtilsForDriver_302947B6, .-SceKernelUtilsForDriver_302947B6
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global SceKernelUtilsForDriver_4899CD4B
.type SceKernelUtilsForDriver_4899CD4B, %function
SceKernelUtilsForDriver_4899CD4B:
	.word 0x10, 0x496ad8b4, 0x4899cd4b, 0xe320f000
.size SceKernelUtilsForDriver_4899CD4B, .-SceKernelUtilsForDriver_4899CD4B
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global SceKernelUtilsForDriver_72408E29
.type SceKernelUtilsForDriver_72408E29, %function
SceKernelUtilsForDriver_72408E29:
	.word 0x10, 0x496ad8b4, 0x72408e29, 0xe320f000
.size SceKernelUtilsForDriver_72408E29, .-SceKernelUtilsForDriver_72408E29
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global SceKernelUtilsForDriver_D909FA2C
.type SceKernelUtilsForDriver_D909FA2C, %function
SceKernelUtilsForDriver_D909FA2C:
	.word 0x10, 0x496ad8b4, 0xd909fa2c, 0xe320f000
.size SceKernelUtilsForDriver_D909FA2C, .-SceKernelUtilsForDriver_D909FA2C
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global SceKernelUtilsForDriver_E39CD272
.type SceKernelUtilsForDriver_E39CD272, %function
SceKernelUtilsForDriver_E39CD272:
	.word 0x10, 0x496ad8b4, 0xe39cd272, 0xe320f000
.size SceKernelUtilsForDriver_E39CD272, .-SceKernelUtilsForDriver_E39CD272
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global SceKernelUtilsForDriver_EDA97D6D
.type SceKernelUtilsForDriver_EDA97D6D, %function
SceKernelUtilsForDriver_EDA97D6D:
	.word 0x10, 0x496ad8b4, 0xeda97d6d, 0xe320f000
.size SceKernelUtilsForDriver_EDA97D6D, .-SceKernelUtilsForDriver_EDA97D6D
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
.global SceDebugForDriver_374B7868
.type SceDebugForDriver_374B7868, %function
SceDebugForDriver_374B7868:
	.word 0x10, 0x88758561, 0x374b7868, 0xe320f000
.size SceDebugForDriver_374B7868, .-SceDebugForDriver_374B7868
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global SceDebugForDriver_D9703808
.type SceDebugForDriver_D9703808, %function
SceDebugForDriver_D9703808:
	.word 0x10, 0x88758561, 0xd9703808, 0xe320f000
.size SceDebugForDriver_D9703808, .-SceDebugForDriver_D9703808
.section .vitalink.fstubs.SceDebugForDriver, "ax", %progbits
.balign 4
.global SceDebugForDriver_FD753E7A
.type SceDebugForDriver_FD753E7A, %function
SceDebugForDriver_FD753E7A:
	.word 0x10, 0x88758561, 0xfd753e7a, 0xe320f000
.size SceDebugForDriver_FD753E7A, .-SceDebugForDriver_FD753E7A
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
.global SceSysmemForDriver_6D88EF8A
.type SceSysmemForDriver_6D88EF8A, %function
SceSysmemForDriver_6D88EF8A:
	.word 0x10, 0x6f25e18a, 0x6d88ef8a, 0xe320f000
.size SceSysmemForDriver_6D88EF8A, .-SceSysmemForDriver_6D88EF8A
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global SceSysmemForDriver_9C78064C
.type SceSysmemForDriver_9C78064C, %function
SceSysmemForDriver_9C78064C:
	.word 0x10, 0x6f25e18a, 0x9c78064c, 0xe320f000
.size SceSysmemForDriver_9C78064C, .-SceSysmemForDriver_9C78064C
.section .vitalink.fstubs.SceSysmemForDriver, "ax", %progbits
.balign 4
.global SceSysmemForDriver_BC996A7A
.type SceSysmemForDriver_BC996A7A, %function
SceSysmemForDriver_BC996A7A:
	.word 0x10, 0x6f25e18a, 0xbc996a7a, 0xe320f000
.size SceSysmemForDriver_BC996A7A, .-SceSysmemForDriver_BC996A7A
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
.global sceKernelRegisterProcEventHandler
.type sceKernelRegisterProcEventHandler, %function
sceKernelRegisterProcEventHandler:
	.word 0x10, 0x887f19d0, 0x2a43912d, 0xe320f000
.size sceKernelRegisterProcEventHandler, .-sceKernelRegisterProcEventHandler
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
.global SceKernelUtilsForDriver_236A9097
.type SceKernelUtilsForDriver_236A9097, %function
SceKernelUtilsForDriver_236A9097:
	.word 0x10, 0x496ad8b4, 0x236a9097, 0xe320f000
.size SceKernelUtilsForDriver_236A9097, .-SceKernelUtilsForDriver_236A9097
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global SceKernelUtilsForDriver_302947B6
.type SceKernelUtilsForDriver_302947B6, %function
SceKernelUtilsForDriver_302947B6:
	.word 0x10, 0x496ad8b4, 0x302947b6, 0xe320f000
.size SceKernelUtilsForDriver_302947B6, .-SceKernelUtilsForDriver_302947B6
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global SceKernelUtilsForDriver_4899CD4B
.type SceKernelUtilsForDriver_4899CD4B, %function
SceKernelUtilsForDriver_4899CD4B:
	.word 0x10, 0x496ad8b4, 0x4899cd4b, 0xe320f000
.size SceKernelUtilsForDriver_4899CD4B, .-SceKernelUtilsForDriver_4899CD4B
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global SceKernelUtilsForDriver_72408E29
.type SceKernelUtilsForDriver_72408E29, %function
SceKernelUtilsForDriver_72408E29:
	.word 0x10, 0x496ad8b4, 0x72408e29, 0xe320f000
.size SceKernelUtilsForDriver_72408E29, .-SceKernelUtilsForDriver_72408E29
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global SceKernelUtilsForDriver_D909FA2C
.type SceKernelUtilsForDriver_D909FA2C, %function
SceKernelUtilsForDriver_D909FA2C:
	.word 0x10, 0x496ad8b4, 0xd909fa2c, 0xe320f000
.size SceKernelUtilsForDriver_D909FA2C, .-SceKernelUtilsForDriver_D909FA2C
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global SceKernelUtilsForDriver_E39CD272
.type SceKernelUtilsForDriver_E39CD272, %function
SceKernelUtilsForDriver_E39CD272:
	.word 0x10, 0x496ad8b4, 0xe39cd272, 0xe320f000
.size SceKernelUtilsForDriver_E39CD272, .-SceKernelUtilsForDriver_E39CD272
.section .vitalink.fstubs.SceKernelUtilsForDriver, "ax", %progbits
.balign 4
.global SceKernelUtilsForDriver_EDA97D6D
.type SceKernelUtilsForDriver_EDA97D6D, %function
SceKernelUtilsForDriver_EDA97D6D:
	.word 0x10, 0x496ad8b4, 0xeda97d6d, 0xe320f000
.size SceKernelUtilsForDriver_EDA97D6D, .-SceKernelUtilsForDriver_EDA97D6D
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
