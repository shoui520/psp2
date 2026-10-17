/*
 * SceCpuForKernel / SceCpuForDriver additions to vitasdk.
 */

#pragma once

#include <psp2kern/kernel/cpu.h>
#include <sysmem/cpu_id.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ThreadmgrLwMutexWork;

/* written in assembly (cpu_spinlock.S, vmaccess.S): declared here from their importers */

/* spins (wfe) until the lock is free */
void SceCpuForDriver_BF82DEB2(SceKernelSpinlock *lock);	/* vitasdk: ksceKernelSpinlockLowLock */
/* 0, or -1 when the lock is taken */
int SceCpuForDriver_5AC9D394(SceKernelSpinlock *lock);	/* vitasdk: ksceKernelSpinlockLowTrylock */

/* a user-space lightweight mutex, under the user's page permissions */
int SceCpuForKernel_75A85059(struct ThreadmgrLwMutexWork *work, SceUID owner);	/* trylockLwMutex */
int SceCpuForKernel_FE9ED689(struct ThreadmgrLwMutexWork *work);		/* unlockLwMutex */
int SceCpuForKernel_9B071FD3(struct ThreadmgrLwMutexWork *work, SceUID owner, int waiters,
			     int count);					/* transferLwMutex */

/* vitasdk: ksceKernelAtomicGetAndOr16 */
int16_t SceCpuForDriver_004F09D1(int16_t *address, int16_t value);
/* vitasdk: ksceKernelDcacheInvalidateRange */
void SceCpuForDriver_02796361(const void *start, SceSize size);
/* vitasdk: ksceKernelAtomicXorAndGet8 */
int8_t SceCpuForDriver_03887992(int8_t *address, int8_t value);
/* vitasdk: ksceKernelAtomicGetAndAdd64 */
uint64_t SceCpuForDriver_043FD446(uint64_t *address, uint64_t value);
/* vitasdk: ksceKernelAtomicAddUnless64 */
int SceCpuForDriver_06CCFA4B(uint64_t *address, uint64_t value, int64_t limit);
void SceCpuForDriver_0836537E(int8_t *address, int8_t value);	/* vitasdk: ksceKernelAtomicSet8 */
/* vitasdk: ksceKernelAtomicGetAndSet16 */
int16_t SceCpuForDriver_085532C8(int16_t *address, int16_t value);
/* vitasdk: ksceKernelAtomicGetAndSet32 */
SceInt32 SceCpuForDriver_0EE04C03(volatile SceInt32 *address, SceInt32 value);
/* vitasdk: ksceKernelAtomicAddUnless16 */
int SceCpuForDriver_0F84AFE9(int16_t *address, int16_t value, int16_t limit);
/* vitasdk: ksceKernelDcacheCleanRange */
void SceCpuForDriver_103872A5(const void *start, SceSize size);
/* vitasdk: ksceKernelAtomicGetAndAnd32 */
SceInt32 SceCpuForDriver_10EB35EB(volatile SceInt32 *address, SceInt32 value);
/* vitasdk: ksceKernelAtomicGetAndAnd64 */
uint64_t SceCpuForDriver_18A17E07(uint64_t *address, uint64_t value);
/* vitasdk: ksceKernelAtomicClearMask8 */
void SceCpuForDriver_1B3336B0(int8_t *address, int8_t mask);
/* vitasdk: ksceKernelAtomicClearMask16 */
void SceCpuForDriver_1BE58599(int16_t *address, int16_t mask);
/* vitasdk: ksceKernelAtomicAddAndGet8 */
int8_t SceCpuForDriver_1E850481(int8_t *address, int8_t value);
/* vitasdk: ksceKernelAtomicAddUnless32 */
SceInt32 SceCpuForDriver_1F157DC3(volatile SceInt32 *address, SceInt32 value, SceInt32 limit);
/* vitasdk: ksceKernelAtomicClearAndGet64 */
uint64_t SceCpuForDriver_2149CD4C(uint64_t *address, uint64_t mask);
/* vitasdk: ksceKernelAtomicGetAndAdd16 */
int16_t SceCpuForDriver_225DF91A(int16_t *address, int16_t value);
/* vitasdk: ksceKernelAtomicDecIfPositive64 */
uint64_t SceCpuForDriver_267D0B33(uint64_t *address);
/* vitasdk: ksceKernelAtomicSetIfGreaterGet32 */
int32_t SceCpuForDriver_26F71995(uint32_t *address, int32_t value);
/* vitasdk: ksceKernelSpinlockLowTrylockCpuSuspendIntr */
int SceCpuForDriver_27C0B340(volatile uint32_t *lock);
/* vitasdk: ksceKernelAtomicGetAndSet8 */
int8_t SceCpuForDriver_29599FC8(int8_t *address, int8_t value);
/* vitasdk: ksceKernelAtomicGetAndOr32 */
SceInt32 SceCpuForDriver_2A40BB93(volatile SceInt32 *address, SceInt32 value);
/* vitasdk: ksceKernelAtomicDecIfPositive32 */
SceInt32 SceCpuForDriver_2A71B03C(volatile SceInt32 *address);
/* vitasdk: ksceKernelAtomicSet32 */
void SceCpuForDriver_3168BC57(volatile SceInt32 *address, SceInt32 value);
/* vitasdk: ksceKernelAtomicAndAndGet8 */
int8_t SceCpuForDriver_32B62B1A(int8_t *address, int8_t value);
/* vitasdk: ksceKernelAtomicGetAndAdd32 */
SceInt32 SceCpuForDriver_341B6E81(volatile SceInt32 *address, SceInt32 value);
/* vitasdk: ksceKernelAtomicCompareAndSet8 */
int8_t SceCpuForDriver_3627F4E0(int8_t *address, int8_t expected, int8_t replacement);
/* vitasdk: ksceKernelDcacheCleanInvalidateRange */
void SceCpuForDriver_364E68A4(const void *start, SceSize size);
/* vitasdk: ksceKernelAtomicGetAndClear8 */
int8_t SceCpuForDriver_382D1466(int8_t *address, int8_t mask);
/* vitasdk: ksceKernelAtomicOrAndGet64 */
uint64_t SceCpuForDriver_3E218AF7(uint64_t *address, uint64_t value);
/* vitasdk: ksceKernelAtomicGetAndSub16 */
int16_t SceCpuForDriver_3EE9B5B8(int16_t *address, int16_t value);
/* vitasdk: ksceKernelAtomicXorAndGet32 */
SceInt32 SceCpuForDriver_4244BE65(volatile SceInt32 *address, SceInt32 value);
/* vitasdk: ksceKernelAtomicDecIfPositive8 */
int8_t SceCpuForDriver_45153D4E(int8_t *address);
/* vitasdk: ksceKernelAtomicGetAndAnd16 */
int16_t SceCpuForDriver_4A820BC5(int16_t *address, int16_t value);
/* vitasdk: ksceKernelAtomicClearMask32 */
void SceCpuForDriver_4AE1BCC0(volatile SceInt32 *address, SceInt32 mask);
/* vitasdk: ksceKernelAtomicCompareAndSet64 */
uint64_t SceCpuForDriver_4B527009(uint64_t *address, uint64_t expected, uint64_t replacement);
/* vitasdk: ksceKernelRWSpinlockLowWriteLockCpuSuspendIntr */
int SceCpuForDriver_4C38CE4D(volatile uint32_t *lock);
/* vitasdk: ksceKernelAtomicAddAndGet64 */
uint64_t SceCpuForDriver_4E459A03(uint64_t *address, uint64_t value);
/* vitasdk: ksceKernelAtomicSubAndGet16 */
int16_t SceCpuForDriver_515682C9(int16_t *address, int16_t value);
void SceCpuForDriver_532CA3E8(int16_t *address, int16_t value);	/* vitasdk: ksceKernelAtomicSet16 */
/* vitasdk: ksceKernelAtomicClearMask64 */
void SceCpuForDriver_55760309(uint64_t *address, uint64_t mask);
/* vitasdk: ksceKernelAtomicAddAndGet16 */
int16_t SceCpuForDriver_59F74E94(int16_t *address, int16_t value);
/* vitasdk: ksceKernelAtomicAddUnless8 */
int SceCpuForDriver_5CC62CEC(int8_t *address, int8_t value, int8_t limit);
/* vitasdk: ksceKernelAtomicOrAndGet8 */
int8_t SceCpuForDriver_5D515F1B(int8_t *address, int8_t value);
/* vitasdk: ksceKernelAtomicAddAndGet32 */
SceInt32 SceCpuForDriver_5F6A8743(volatile SceInt32 *address, SceInt32 value);
/* vitasdk: ksceKernelAtomicXorAndGet16 */
int16_t SceCpuForDriver_646003D6(int16_t *address, int16_t value);
/* vitasdk: ksceKernelAtomicXorAndGet64 */
uint64_t SceCpuForDriver_692C51B3(uint64_t *address, uint64_t value);
/* vitasdk: ksceKernelAtomicClearAndGet16 */
int16_t SceCpuForDriver_6B050D7C(int16_t *address, int16_t mask);
/* vitasdk: ksceKernelAtomicCompareAndSet16 */
int16_t SceCpuForDriver_6F63F56D(int16_t *address, int16_t expected, int16_t replacement);
/* vitasdk: ksceKernelAtomicGetAndXor16 */
int16_t SceCpuForDriver_711801E6(int16_t *address, int16_t value);
/* vitasdk: ksceKernelAtomicGetAndXor32 */
SceInt32 SceCpuForDriver_77E34309(volatile SceInt32 *address, SceInt32 value);
/* vitasdk: ksceKernelAtomicClearAndGet32 */
SceInt32 SceCpuForDriver_78C1F148(volatile SceInt32 *address, SceInt32 mask);
/* vitasdk: ksceKernelAtomicGetAndSub8 */
int8_t SceCpuForDriver_7B43D0D7(int8_t *address, int8_t value);
int SceCpuForDriver_821FC0EE(void);	/* vitasdk: ksceKernelCpuSuspendIntr */
/* vitasdk: ksceKernelAtomicSetIfGreaterGet16 */
int32_t SceCpuForDriver_875B094D(uint16_t *address, int32_t value);
/* vitasdk: ksceKernelAtomicGetAndClear64 */
uint64_t SceCpuForDriver_88BA6002(uint64_t *address, uint64_t mask);
/* vitasdk: ksceKernelDcacheInvalidateRangeForL1WBWA */
void SceCpuForDriver_8B4C26DF(const void *start, SceSize size);
/* vitasdk: ksceKernelAtomicClearAndGet8 */
int8_t SceCpuForDriver_8E538AB5(int8_t *address, int8_t mask);
/* vitasdk: ksceKernelAtomicGetAndClear16 */
int16_t SceCpuForDriver_8E9C086D(int16_t *address, int16_t mask);
/* vitasdk: ksceKernelAtomicDecIfPositive16 */
int16_t SceCpuForDriver_9A693F5B(int16_t *address);
/* vitasdk: ksceKernelDcacheCleanRangeForL1WBWA */
void SceCpuForDriver_9CB9F0CE(const void *start, SceSize size);
/* vitasdk: ksceKernelAtomicSubAndGet32 */
SceInt32 SceCpuForDriver_A4884C4E(volatile SceInt32 *address, SceInt32 value);
/* vitasdk: ksceKernelAtomicGetAndSub64 */
uint64_t SceCpuForDriver_A7585370(uint64_t *address, uint64_t value);
/* vitasdk: ksceKernelAtomicOrAndGet16 */
int16_t SceCpuForDriver_ADD39B84(int16_t *address, int16_t value);
/* vitasdk: ksceKernelAtomicAndAndGet16 */
int16_t SceCpuForDriver_B281D52A(int16_t *address, int16_t value);
/* vitasdk: ksceKernelAtomicSubAndGet64 */
uint64_t SceCpuForDriver_B5F8919C(uint64_t *address, uint64_t value);
/* vitasdk: ksceKernelAtomicGetAndXor8 */
int8_t SceCpuForDriver_BAF47F7B(int8_t *address, int8_t value);
/* vitasdk: ksceKernelAtomicOrAndGet32 */
SceInt32 SceCpuForDriver_BC248C30(volatile SceInt32 *address, SceInt32 value);
/* vitasdk: ksceKernelAtomicGetAndOr8 */
int8_t SceCpuForDriver_BDF6F8E4(int8_t *address, int8_t value);
/* vitasdk: ksceKernelAtomicSet64 */
void SceCpuForDriver_C381CE8C(uint64_t *address, uint64_t value);
/* vitasdk: ksceKernelAtomicSetIfGreaterGet8 */
int32_t SceCpuForDriver_C3868071(uint8_t *address, int32_t value);
/* vitasdk: ksceKernelAtomicGetAndOr64 */
uint64_t SceCpuForDriver_CB73D6D5(uint64_t *address, uint64_t value);
/* vitasdk: ksceKernelAtomicCompareAndSet32 */
SceInt32 SceCpuForDriver_CDA96E81(volatile SceInt32 *address, SceInt32 expected, SceInt32 replacement);
/* vitasdk: ksceKernelAtomicAndAndGet64 */
uint64_t SceCpuForDriver_D18E7B54(uint64_t *address, uint64_t value);
/* vitasdk: ksceKernelAtomicGetAndSet64 */
uint64_t SceCpuForDriver_D2DEE625(uint64_t *address, uint64_t value);
/* vitasdk: ksceKernelSpinlockLowLockCpuSuspendIntr */
int SceCpuForDriver_D32ACE9E(volatile uint32_t *lock);
/* vitasdk: ksceKernelAtomicGetAndAnd8 */
int8_t SceCpuForDriver_D8E675C0(int8_t *address, int8_t value);
/* vitasdk: ksceKernelRWSpinlockLowTryWriteLockCpuSuspendIntr */
int SceCpuForDriver_DE6482C6(volatile uint32_t *lock);
/* vitasdk: ksceKernelAtomicAndAndGet32 */
SceInt32 SceCpuForDriver_DF899E4B(volatile SceInt32 *address, SceInt32 value);
/* vitasdk: ksceKernelAtomicGetAndXor64 */
uint64_t SceCpuForDriver_E212ECAD(uint64_t *address, uint64_t value);
/* vitasdk: ksceKernelAtomicGetAndClear32 */
SceInt32 SceCpuForDriver_E36F3A46(volatile SceInt32 *address, SceInt32 mask);
/* vitasdk: ksceKernelDcacheCleanInvalidateRangeForL1WBWA */
void SceCpuForDriver_E551F99B(const void *start, SceSize size);
void SceCpuForDriver_E813EBB2(void);
/* vitasdk: ksceKernelAtomicSubAndGet8 */
int8_t SceCpuForDriver_EB085370(int8_t *address, int8_t value);
/* vitasdk: ksceKernelRWSpinlockLowReadLockCpuSuspendIntr */
int SceCpuForDriver_EC53D007(volatile uint32_t *lock);
/* vitasdk: ksceKernelRWSpinlockLowTryReadLockCpuSuspendIntr */
int SceCpuForDriver_F02467D1(volatile uint32_t *lock);
/* vitasdk: ksceKernelAtomicGetAndSub32 */
SceInt32 SceCpuForDriver_F891CF2A(volatile SceInt32 *address, SceInt32 value);
/* vitasdk: ksceKernelAtomicGetAndAdd8 */
int8_t SceCpuForDriver_FCDCD4DE(int8_t *address, int8_t value);
int SceCpuForKernel_050B8EF6(void);	/* vitasdk: ksceKernelCpuBranchPredictorInvalidateAll */
/* vitasdk: ksceKernelAtomicDecIfLowPositive32 */
uint32_t SceCpuForKernel_0E04BD2E(uint32_t *address);
int SceCpuForKernel_14E0B0C4(uintptr_t va, uintptr_t *pa);	/* vitasdk: ksceKernelMMUVAtoPA */
int SceCpuForKernel_15A48AB4(void);	/* vitasdk: ksceKernelL1IcacheInvalidateEntire */
/* vitasdk: ksceKernelAtomicGet32AndSet64_2 */
int32_t SceCpuForKernel_1A62AD6A(uint64_t *address, int32_t value);
/* vitasdk: ksceKernelMMUVAtoPAWithMode */
int SceCpuForKernel_1BD23326(uint32_t mode, uintptr_t va, uintptr_t *pa);
/* vitasdk: ksceKernelL1IcacheInvalidateRange */
void SceCpuForKernel_2E637B1D(const void *start, uint32_t size);
/* vitasdk: ksceKernelCorelockLock */
void SceCpuForKernel_2FF54320(SceCorelockContext *context, SceUInt32 core);
/* vitasdk: ksceKernelAtomicSubIfGreater64 */
int SceCpuForKernel_3D4FBB32(uint64_t *address, int32_t value);
/* vitasdk: ksceKernelL1DcacheInvalidateRange */
void SceCpuForKernel_4236E06A(const void *start, uint32_t size);
void SceCpuForKernel_49180814(void);	/* vitasdk: ksceKernelL1DcacheCleanAll */
void SceCpuForKernel_4BBA5C82(void);	/* vitasdk: ksceKernelL1DcacheCleanInvalidateAll */
/* vitasdk: ksceKernelL1DcacheCleanInvalidateRange */
void SceCpuForKernel_4F442396(const void *start, uint32_t size);
/* vitasdk: ksceKernelAtomicGet32AndSet64 */
int32_t SceCpuForKernel_59448055(uint64_t *address, int32_t value);
/* vitasdk: ksceKernelL1DcacheCleanInvalidate */
void SceCpuForKernel_597426D2(const void *address);
/* vitasdk: ksceKernelAtomicAdd32AndGet64InHiLoRange */
int SceCpuForKernel_61DDA758(uint64_t *address, uint32_t value, int32_t limit);
/* vitasdk: ksceKernelL1DcacheCleanRange */
void SceCpuForKernel_70350360(const void *start, uint32_t size);
void SceCpuForKernel_74B7F445(const void *address);	/* vitasdk: ksceKernelL1DcacheClean */
int SceCpuForKernel_7B0643E2(void);	/* vitasdk: ksceKernelCpuBranchPredictorInvalidateAllIS */
void SceCpuForKernel_7ED856FE(const void *address);	/* vitasdk: ksceKernelL1DcacheInvalidate */
int SceCpuForKernel_803C84BF(void);	/* vitasdk: ksceKernelL1IcacheInvalidateEntireAllCore */
int SceCpuForKernel_87F7631D(uintptr_t va);	/* vitasdk: ksceKernelMMUGetMemoryType */
/* vitasdk: ksceKernelAtomicAddAndGetPositive32InRange */
int SceCpuForKernel_8F40D0FD(uint32_t *address, uint32_t value, int32_t limit);
uint32_t SceCpuForKernel_9818A20B(uint32_t value);
/* vitasdk: ksceKernelAtomicIncrementHighwaterCounter */
uint32_t SceCpuForKernel_9DA58A9E(uint32_t *address);
void SceCpuForKernel_A0A70166(void);	/* vitasdk: ksceKernelPleFlushRequest */
/* vitasdk: ksceKernelAtomicAdd32AndGet64InRange */
int SceCpuForKernel_A12D4388(uint64_t *address, uint32_t value, int32_t limit);
/* vitasdk: ksceKernelAtomicLimit64 */
int SceCpuForKernel_A3D7140F(uint64_t *address, int32_t value);
/* vitasdk: ksceKernelCorelockInitialize */
void SceCpuForKernel_A65F6F14(SceCorelockContext *context);
void SceCpuForKernel_B331AB16(struct CpuLongIrqHandlerRecord *record);
/* vitasdk: ksceKernelGetVmaccessRange */
void SceCpuForKernel_C32687D0(uintptr_t *start, uintptr_t *end);
void SceCpuForKernel_C6426313(void);	/* vitasdk: ksceKernelCpuUpdateSCTLR */
void SceCpuForKernel_F9B0B171(void);	/* vitasdk: ksceKernelL1DcacheInvalidateAll */
uint8_t SceCpuForKernel_FD54A91D(void);	/* vitasdk: ksceKernelCpuGetCONTEXTIDR */
/* vitasdk: ksceKernelCorelockUnlock */
void SceCpuForKernel_FE2B77A3(SceCorelockContext *context);
int SceCpuForTZS_44C423D3(void);
int sceKernelCpuId(void);
int sceKernelCpuResumeIntr(int flags);
void sceKernelIcacheInvalidateRange(const void *start, SceSize size);
SceBool sceKernelIsUncacheAddressInTmpFsGame(const void *address);
void sceKernelRWSpinlockLowReadUnlock(volatile uint32_t *lock);
void sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(volatile uint32_t *lock, uint32_t flags);
void sceKernelRWSpinlockLowWriteUnlock(volatile uint32_t *lock);
void sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(volatile uint32_t *lock, uint32_t flags);
void sceKernelSpinlockLowUnlock(volatile uint32_t *lock);
void sceKernelSpinlockLowUnlockCpuResumeIntr(volatile uint32_t *lock, uint32_t flags);

#ifdef __cplusplus
}
#endif
