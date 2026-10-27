/*
 * SceCpu (sysmem): SceCpuForKernel and SceCpuForDriver, the CPU's atomics, caches, MMU queries,
 * spinlocks and interrupt masking.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <sysmem/spinlock.h>
#include <sysmem/cpu_id.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SceCpuForKernel_B331AB16: told of each long interrupts-masked section (name unknown) */
struct CpuLongIrqHandlerRecord {
	int (*notify)(int kind, uint32_t duration, unsigned core,
		       uintptr_t lockedCaller, const void *resumeCaller);
};
struct ThreadmgrLwMutexWork;

/*
 * The four cores passing a section one at a time, in core order (corelock.c). No source names
 * it: vitasdk's type name, the fields named by their use.
 */
typedef struct SceCorelockContext {
	SceKernelSpinlock lock;		/* held from a core's Lock to its Unlock */
	int16_t coresLeft;		/* 0x04: cores still to pass; 4 again when all have */
	int16_t turn;			/* 0x06: the core whose turn it is (4: none yet) */
} SceCorelockContext;

/* written in assembly (cpu_spinlock.S, vmaccess.S): declared here from their importers */

/* spins (wfe) until the lock is free */
void sceKernelSpinlockLowLock(volatile SceKernelSpinlock *lock);
/* 0, or -1 when the lock is taken */
int sceKernelSpinlockLowTrylock(volatile SceKernelSpinlock *lock);

/* a user-space lightweight mutex, under the user's page permissions */
int SceCpuForKernel_75A85059(struct ThreadmgrLwMutexWork *work, SceUID owner);	/* trylockLwMutex */
int SceCpuForKernel_FE9ED689(struct ThreadmgrLwMutexWork *work);		/* unlockLwMutex */
int SceCpuForKernel_9B071FD3(struct ThreadmgrLwMutexWork *work, SceUID owner, int waiters,
			     int count);					/* transferLwMutex */

int16_t sceKernelAtomicGetAndOr16(int16_t *address, int16_t value);
void sceKernelDcacheInvalidateRange(const void *start, SceSize size);
int8_t sceKernelAtomicXorAndGet8(int8_t *address, int8_t value);
uint64_t sceKernelAtomicGetAndAdd64(uint64_t *address, uint64_t value);
int sceKernelAtomicAddUnless64(uint64_t *address, uint64_t value, int64_t limit);
void sceKernelAtomicSet8(int8_t *address, int8_t value);
int16_t sceKernelAtomicGetAndSet16(int16_t *address, int16_t value);
SceInt32 sceKernelAtomicGetAndSet32(volatile SceInt32 *address, SceInt32 value);
int sceKernelAtomicAddUnless16(int16_t *address, int16_t value, int16_t limit);
void sceKernelDcacheCleanRange(const void *start, SceSize size);
SceInt32 sceKernelAtomicGetAndAnd32(volatile SceInt32 *address, SceInt32 value);
uint64_t sceKernelAtomicGetAndAnd64(uint64_t *address, uint64_t value);
void sceKernelAtomicClearMask8(int8_t *address, int8_t mask);
void sceKernelAtomicClearMask16(int16_t *address, int16_t mask);
int8_t sceKernelAtomicAddAndGet8(int8_t *address, int8_t value);
SceInt32 sceKernelAtomicAddUnless32(volatile SceInt32 *address, SceInt32 value, SceInt32 limit);
uint64_t sceKernelAtomicClearAndGet64(uint64_t *address, uint64_t mask);
int16_t sceKernelAtomicGetAndAdd16(int16_t *address, int16_t value);
uint64_t sceKernelAtomicDecIfPositive64(uint64_t *address);
/* vitasdk: ksceKernelAtomicSetIfGreaterGet32 */
int32_t SceCpuForDriver_26F71995(uint32_t *address, int32_t value);
SceKernelIntrStatus sceKernelSpinlockLowTrylockCpuSuspendIntr(volatile SceKernelSpinlock *lock);
int8_t sceKernelAtomicGetAndSet8(int8_t *address, int8_t value);
SceInt32 sceKernelAtomicGetAndOr32(volatile SceInt32 *address, SceInt32 value);
SceInt32 sceKernelAtomicDecIfPositive32(volatile SceInt32 *address);
void sceKernelAtomicSet32(volatile SceInt32 *address, SceInt32 value);
int8_t sceKernelAtomicAndAndGet8(int8_t *address, int8_t value);
SceInt32 sceKernelAtomicGetAndAdd32(volatile SceInt32 *address, SceInt32 value);
int8_t sceKernelAtomicCompareAndSet8(int8_t *address, int8_t expected, int8_t replacement);
void sceKernelDcacheCleanInvalidateRange(const void *start, SceSize size);
int8_t sceKernelAtomicGetAndClear8(int8_t *address, int8_t mask);
uint64_t sceKernelAtomicOrAndGet64(uint64_t *address, uint64_t value);
int16_t sceKernelAtomicGetAndSub16(int16_t *address, int16_t value);
SceInt32 sceKernelAtomicXorAndGet32(volatile SceInt32 *address, SceInt32 value);
int8_t sceKernelAtomicDecIfPositive8(int8_t *address);
int16_t sceKernelAtomicGetAndAnd16(int16_t *address, int16_t value);
void sceKernelAtomicClearMask32(volatile SceInt32 *address, SceInt32 mask);
uint64_t sceKernelAtomicCompareAndSet64(uint64_t *address, uint64_t expected, uint64_t replacement);
/* vitasdk: ksceKernelRWSpinlockLowWriteLockCpuSuspendIntr */
SceKernelIntrStatus SceCpuForDriver_4C38CE4D(volatile SceKernelRWSpinlock *lock);
uint64_t sceKernelAtomicAddAndGet64(uint64_t *address, uint64_t value);
int16_t sceKernelAtomicSubAndGet16(int16_t *address, int16_t value);
void sceKernelAtomicSet16(int16_t *address, int16_t value);
void sceKernelAtomicClearMask64(uint64_t *address, uint64_t mask);
int16_t sceKernelAtomicAddAndGet16(int16_t *address, int16_t value);
int sceKernelAtomicAddUnless8(int8_t *address, int8_t value, int8_t limit);
int8_t sceKernelAtomicOrAndGet8(int8_t *address, int8_t value);
SceInt32 sceKernelAtomicAddAndGet32(volatile SceInt32 *address, SceInt32 value);
int16_t sceKernelAtomicXorAndGet16(int16_t *address, int16_t value);
uint64_t sceKernelAtomicXorAndGet64(uint64_t *address, uint64_t value);
int16_t sceKernelAtomicClearAndGet16(int16_t *address, int16_t mask);
int16_t sceKernelAtomicCompareAndSet16(int16_t *address, int16_t expected, int16_t replacement);
int16_t sceKernelAtomicGetAndXor16(int16_t *address, int16_t value);
SceInt32 sceKernelAtomicGetAndXor32(volatile SceInt32 *address, SceInt32 value);
SceInt32 sceKernelAtomicClearAndGet32(volatile SceInt32 *address, SceInt32 mask);
int8_t sceKernelAtomicGetAndSub8(int8_t *address, int8_t value);
int sceKernelCpuSuspendIntr(void);
/* vitasdk: ksceKernelAtomicSetIfGreaterGet16 */
int32_t SceCpuForDriver_875B094D(uint16_t *address, int32_t value);
uint64_t sceKernelAtomicGetAndClear64(uint64_t *address, uint64_t mask);
void sceKernelDcacheInvalidateRangeForL1WBWA(const void *start, SceSize size);
int8_t sceKernelAtomicClearAndGet8(int8_t *address, int8_t mask);
int16_t sceKernelAtomicGetAndClear16(int16_t *address, int16_t mask);
int16_t sceKernelAtomicDecIfPositive16(int16_t *address);
void sceKernelDcacheCleanRangeForL1WBWA(const void *start, SceSize size);
SceInt32 sceKernelAtomicSubAndGet32(volatile SceInt32 *address, SceInt32 value);
uint64_t sceKernelAtomicGetAndSub64(uint64_t *address, uint64_t value);
int16_t sceKernelAtomicOrAndGet16(int16_t *address, int16_t value);
int16_t sceKernelAtomicAndAndGet16(int16_t *address, int16_t value);
uint64_t sceKernelAtomicSubAndGet64(uint64_t *address, uint64_t value);
int8_t sceKernelAtomicGetAndXor8(int8_t *address, int8_t value);
SceInt32 sceKernelAtomicOrAndGet32(volatile SceInt32 *address, SceInt32 value);
int8_t sceKernelAtomicGetAndOr8(int8_t *address, int8_t value);
void sceKernelAtomicSet64(uint64_t *address, uint64_t value);
/* vitasdk: ksceKernelAtomicSetIfGreaterGet8 */
int32_t SceCpuForDriver_C3868071(uint8_t *address, int32_t value);
uint64_t sceKernelAtomicGetAndOr64(uint64_t *address, uint64_t value);
SceInt32 sceKernelAtomicCompareAndSet32(volatile SceInt32 *address, SceInt32 expected, SceInt32 replacement);
uint64_t sceKernelAtomicAndAndGet64(uint64_t *address, uint64_t value);
uint64_t sceKernelAtomicGetAndSet64(uint64_t *address, uint64_t value);
SceKernelIntrStatus sceKernelSpinlockLowLockCpuSuspendIntr(volatile SceKernelSpinlock *lock);
int8_t sceKernelAtomicGetAndAnd8(int8_t *address, int8_t value);
/* vitasdk: ksceKernelRWSpinlockLowTryWriteLockCpuSuspendIntr */
SceKernelIntrStatus SceCpuForDriver_DE6482C6(volatile SceKernelRWSpinlock *lock);
SceInt32 sceKernelAtomicAndAndGet32(volatile SceInt32 *address, SceInt32 value);
uint64_t sceKernelAtomicGetAndXor64(uint64_t *address, uint64_t value);
SceInt32 sceKernelAtomicGetAndClear32(volatile SceInt32 *address, SceInt32 mask);
void sceKernelDcacheCleanInvalidateRangeForL1WBWA(const void *start, SceSize size);
void SceCpuForDriver_E813EBB2(void);
int8_t sceKernelAtomicSubAndGet8(int8_t *address, int8_t value);
/* vitasdk: ksceKernelRWSpinlockLowReadLockCpuSuspendIntr */
SceKernelIntrStatus SceCpuForDriver_EC53D007(volatile SceKernelRWSpinlock *lock);
/* vitasdk: ksceKernelRWSpinlockLowTryReadLockCpuSuspendIntr */
SceKernelIntrStatus SceCpuForDriver_F02467D1(volatile SceKernelRWSpinlock *lock);
SceInt32 sceKernelAtomicGetAndSub32(volatile SceInt32 *address, SceInt32 value);
int8_t sceKernelAtomicGetAndAdd8(int8_t *address, int8_t value);
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
int sceKernelL1IcacheInvalidateEntireAllCore(void);
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
int sceKernelCpuId(void);
int sceKernelCpuResumeIntr(int flags);
/* the L2 cache controller (l2cc_maintenance.S); the secure kernel exports them as SceCpuForTZS */
void CpuL2CacheSyncCore(void);
void CpuL2CleanInvalidateAll(void);
void sceKernelIcacheInvalidateRange(const void *start, SceSize size);
SceBool sceKernelIsUncacheAddressInTmpFsGame(const void *address);
void sceKernelRWSpinlockLowReadUnlock(volatile SceKernelRWSpinlock *lock);
void sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(volatile SceKernelRWSpinlock *lock, SceKernelIntrStatus flags);
void sceKernelRWSpinlockLowWriteUnlock(volatile SceKernelRWSpinlock *lock);
void sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(volatile SceKernelRWSpinlock *lock, SceKernelIntrStatus flags);
void sceKernelSpinlockLowUnlock(volatile SceKernelSpinlock *lock);
void sceKernelSpinlockLowUnlockCpuResumeIntr(volatile SceKernelSpinlock *lock, SceKernelIntrStatus flags);

/* the secure kernel's SceCpuForTZS (the names of 0.990's import stubs; sceKernelL1IcacheInvalidateEntireAllCore,
   sceKernelAtomicCompareAndSet8 (0.990: atomicCompareAndSet8) and sceKernelAtomicOrAndGet32, the
   functions the non-secure kernel exports too, are declared above) */
void sceKernelDcacheCleanInvalidateAll(void);
void sceKernelL1DcacheCleanRange(const void *start, SceSize size);
void sceKernelL1DcacheCleanInvalidateAll(void);
void sceKernelL1IcacheInvalidateRange(const void *start, SceSize size);
void sceKernelPleFlushRequest(void);
void SceCpuForTZS_D4E7413D(void);	/* cleans the data cache before a core parks */

#ifdef __cplusplus
}
#endif
