#include "pamgr_internal.h"
#include <string.h>
#include <sysmem/sysmem.h>
#include <sysmem/debug.h>
#include <processmgr/processmgr.h>
#include <threadmgr/threadmgr.h>
#include <kernel/error.h>
#include <sysmem/procevent.h>
#include <pamgr/pamgr.h>

int sceKernelPaAddArmTraceByKey(uint32_t registration,
			    const struct PamgrArmTraceConfig *config)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return configureArmTrace(config);
}

int PamgrProcKill(SceUID process, ProcEventParam *param, void *common)
{
	int result;

	result = lockFastMutex(&pamgrState.registrationMutex);
	if (result < 0) {
		SCE_DBG_PRINTF(0, 82,
			"FATAL: sceKernelLockFastMutex failed (ret=0x%08x)\n", result);
		return result;
	}
	if (pamgrState.registrationProcess == process) {
		pamgrState.registrationId = 0;
		pamgrState.registrationShared = 0;
	}
	result = sceKernelUnlockFastMutex(&pamgrState.registrationMutex);
	if (result < 0) {
		SCE_DBG_PRINTF(0, 95,
			"FATAL: sceKernelUnlockFastMutex failed (ret=0x%08x)\n", result);
		return result;
	}
	return 0;
}

int PamgrProcExit(SceUID process, ProcEventParam *param, void *common)
{
	int result;

	result = lockFastMutex(&pamgrState.registrationMutex);
	if (result < 0) {
		SCE_DBG_PRINTF(0, 52,
			"FATAL: sceKernelLockFastMutex failed (ret=0x%08x)\n", result);
		return result;
	}
	if (pamgrState.registrationProcess == process) {
		pamgrState.registrationId = 0;
		pamgrState.registrationShared = 0;
	}
	result = sceKernelUnlockFastMutex(&pamgrState.registrationMutex);
	if (result < 0) {
		SCE_DBG_PRINTF(0, 65,
			"FATAL: sceKernelUnlockFastMutex failed (ret=0x%08x)\n", result);
		return result;
	}
	return 0;
}

int sceKernelPaInitPamgrWrapper(void)
{
	SceKernelProcEventHandler events = {
		.size = sizeof(events),
		.exit = PamgrProcExit,
		.kill = PamgrProcKill,
	};
	int result;

	result = sceKernelRegisterProcEventHandler("ScePamgr", &events, NULL);
	if (result < 0) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 121);
		return result;
	}
	memset(&pamgrState.registrationMutex, 0,
	       sizeof(pamgrState) - offsetof(struct PamgrState, registrationMutex));
	result = sceKernelInitializeFastMutex(&pamgrState.registrationMutex,
					      "SceKernelPaControlLock", 2, NULL);
	if (result < 0) {
		SCE_DBG_PRINTF(0, 130,
			"FATAL: sceKernelInitializeFastMutex failed (0x%08x)\n", result);
		return result;
	}
	return 0;
}
int clearCounterTrace(void);

int sceKernelPaAddGpuTraceByKey(uint32_t registration,
			     const struct PamgrGpuTraceConfig *config)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return configureGpuTrace(config);
}

int sceKernelPaRemoveGpuTraceByKey(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return clearGpuTrace();
}

int __attribute__((noinline)) configureRegisteredGpuTraceForUser(uint32_t registration,
				     const struct PamgrGpuTraceConfig *config)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return configureGpuTraceForUser(config);
}

int sceKernelPaRemoveGpuTraceByKeyForUser(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return clearGpuTraceForUser();
}

int sceKernelPaAddCounterTraceByKey(uint32_t registration,
			     const struct PamgrCounterTraceConfig *config)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return configureCounterTrace(config);
}

int sceKernelPaRemoveCounterTraceByKey(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return clearCounterTrace();
}

int sceKernelPaSetupTraceBufferByKey(uint32_t registration,
			     const struct PamgrPrimaryTraceConfig *config)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return configurePrimaryTrace(config);
}

/* Registration gate around the user buffer adapter. */
int __attribute__((noinline)) configureRegisteredPrimaryTraceForUser(uint32_t registration,
				const struct PamgrPrimaryTraceConfig *config)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return configurePrimaryTraceForUser(config);
}

int ScePamgrForDriver_E9E45545(uint32_t registration, const uint32_t *controls)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return setTraceBitControls(controls);
}

int ScePamgrForDriver_A1E448E7(uint32_t registration,
			     const struct PamgrSampleConfig *config)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return configureTraceSamples(config);
}

int ScePamgrForDriver_D4FBC37D(uint32_t registration, uint32_t unused,
			     uint32_t config, uint32_t half)
{
	(void)unused;
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	configureTraceControl(config, half);
	return 0;
}

int ScePamgrForDriver_E8FF4812(uint32_t registration,
			     const struct PamgrTraceBufferConfig *config)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return configureTraceBuffer1(config);
}

int sceKernelPaSetBookmarkChannelEnableByKey(uint32_t registration, unsigned int mask,
			     uint32_t selection)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return setTraceRecordMask(mask, selection);
}

int ScePamgrForDriver_95F4079E(uint32_t registration, uint32_t pointer)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return setTraceBuffer1WritePointer(pointer);
}

int sceKernelPaRemoveArmTraceByKey(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return clearArmTrace();
}

int ScePamgrForDriver_B619D492(uint32_t registration,
			     const struct PamgrPackedFilter *filter)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return setTracePackedFilter(filter);
}

int ScePamgrForDriver_81764B21(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return clearTracePackedFilter();
}

int sceKernelPaAddBusTraceByKey(uint32_t registration, const uint32_t *filter)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return setTraceWordFilter(filter);
}

int sceKernelPaRemoveBusTraceByKey(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return clearTraceWordFilter();
}

int ScePamgrForDriver_D9EFF812(uint32_t registration, uint32_t pointer)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return setTraceWritePointer(pointer);
}

int ScePamgrForDriver_3E3E66DF(uint32_t registration, uint32_t status)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return setTraceInterruptStatus(status);
}

int ScePamgrForDriver_3C12A0CD(uint32_t registration,
			     PamgrTraceInterruptCallback callback, void *arg)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return registerTraceInterruptCallback(callback, arg);
}

int ScePamgrForDriver_FB04BBEF(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return unregisterTraceInterruptCallback();
}

int sceKernelPaRegister(void)
{
	int result, unlocked;

	result = lockFastMutex(&pamgrState.registrationMutex);
	if (result < 0) {
		SCE_DBG_PRINTF(0, 146,
			"FATAL: sceKernelLockFastMutex failed (ret=0x%08x)\n", result);
		return result;
	}
	result = SCE_KERNEL_ERROR_PA_NOT_AVAILABLE;
	if (pamgrState.registrationId == 0) {
		result = (uint32_t)sceKernelPaGetTimebaseValue() & 0x7fffffff;
		pamgrState.registrationId = result;
		pamgrState.registrationProcess = sceKernelGetProcessId();
		pamgrState.registrationShared = 0;
	}
	unlocked = sceKernelUnlockFastMutex(&pamgrState.registrationMutex);
	if (unlocked < 0) {
		SCE_DBG_PRINTF(0, 164,
			"FATAL: sceKernelUnlockFastMutex failed (ret=0x%08x)\n", unlocked);
		return unlocked;
	}
	return result;
}

int sceKernelPaUnregister(uint32_t registration)
{
	int result, unlocked;

	result = lockFastMutex(&pamgrState.registrationMutex);
	if (result < 0) {
		SCE_DBG_PRINTF(0, 180,
			"FATAL: sceKernelLockFastMutex failed (ret=0x%08x)\n", result);
		return result;
	}
	result = SCE_KERNEL_ERROR_PA_INVALID_KEY;
	if (registration != 0 && registration == pamgrState.registrationId) {
		pamgrState.registrationId = 0;
		pamgrState.registrationShared = 0;
		result = 0;
	}
	unlocked = sceKernelUnlockFastMutex(&pamgrState.registrationMutex);
	if (unlocked < 0) {
		SCE_DBG_PRINTF(0, 195,
			"FATAL: sceKernelUnlockFastMutex failed (ret=0x%08x)\n", unlocked);
		return unlocked;
	}
	return result;
}

int sceKernelPaRegisterShared(uint64_t key)
{
	int result, unlocked;

	result = lockFastMutex(&pamgrState.registrationMutex);
	if (result < 0) {
		SCE_DBG_PRINTF(0, 211,
			"FATAL: sceKernelLockFastMutex failed (ret=0x%08x)\n", result);
		return result;
	}
	result = SCE_KERNEL_ERROR_PA_NOT_AVAILABLE;
	if (pamgrState.registrationId == 0) {
		result = (uint32_t)sceKernelPaGetTimebaseValue() & 0x7fffffff;
		pamgrState.registrationId = result;
		pamgrState.registrationProcess = sceKernelGetProcessId();
		pamgrState.registrationShared = 1;
		pamgrState.sharedKey = key;
	}
	unlocked = sceKernelUnlockFastMutex(&pamgrState.registrationMutex);
	if (unlocked < 0) {
		SCE_DBG_PRINTF(0, 230,
			"FATAL: sceKernelUnlockFastMutex failed (ret=0x%08x)\n", unlocked);
		return unlocked;
	}
	return result;
}

/* ForUser suffixes distinguish the separate user-library ABI entries. */
int sceKernelPaGetSharedKeyForUser(uint64_t key)
{
	if (pamgrState.registrationShared != 1)
		return SCE_KERNEL_ERROR_PA_KEY_IS_NOT_SHARED;
	if (key != pamgrState.sharedKey)
		return SCE_KERNEL_ERROR_PA_INVALID_SIGNATURE;
	return pamgrState.registrationId;
}

int sceKernelPaGetSharedKey(uint64_t key)
{
	if (pamgrState.registrationShared != 1)
		return SCE_KERNEL_ERROR_PA_KEY_IS_NOT_SHARED;
	if (key != pamgrState.sharedKey)
		return SCE_KERNEL_ERROR_PA_INVALID_SIGNATURE;
	return pamgrState.registrationId;
}

int sceKernelPaRegisterForUser(void)
{
	return sceKernelPaRegister();
}

int sceKernelPaUnregisterForUser(uint32_t registration)
{
	return sceKernelPaUnregister(registration);
}

int sceKernelPaRegisterSharedForUser(uint64_t key)
{
	return sceKernelPaRegisterShared(key);
}

int sceKernelPaStartByKey(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return startPrimaryTrace();
}

int ScePamgrForDriver_A708D62A(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return startSecondaryTrace();
}

int sceKernelPaStopByKey(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return stopPrimaryTrace();
}

int ScePamgrForDriver_1A7733E0(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return stopSecondaryTrace();
}

int ScePamgrForDriver_E0D151F4(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	clearBusErrorLog();
	return 0;
}

int ScePamgrForDriver_48EC7C89(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	clearSecureBusErrorLog();
	return 0;
}

int ScePamgrForDriver_A8C0A636(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	enableTraceAuxControl0();
	return 0;
}

int ScePamgrForDriver_C5125E61(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	enableTraceAuxControl1();
	return 0;
}

int ScePamgrForDriver_5EFEB00D(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	disableTraceAuxControl1();
	return 0;
}

int __attribute__((noinline)) configureRegisteredCounterTraceForUser(uint32_t registration,
				const struct PamgrCounterTraceConfig *config)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return sceKernelPaAddCounterTraceForUser(config);
}

int sceKernelPaRemoveCounterTraceByKeyForUser(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return clearCounterTraceForUser();
}

int __attribute__((noinline)) configureRegisteredArmTraceForUser(uint32_t registration,
				    const struct PamgrArmTraceConfig *config)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return configureArmTraceForUser(config);
}

int sceKernelPaRemoveArmTraceByKeyForUser(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return clearArmTraceForUser();
}

int __attribute__((noinline)) configureRegisteredBusTraceForUser(
		uint32_t registration, const uint32_t *filter)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return setTraceWordFilterForUser(filter);
}

int sceKernelPaRemoveBusTraceByKeyForUser(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return clearTraceWordFilterForUser();
}

int sceKernelPaStartByKeyForUser(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return startPrimaryTraceForUser();
}

int sceKernelPaStopByKeyForUser(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return stopPrimaryTraceForUser();
}

int sceKernelPaSetBookmarkChannelEnableByKeyForUser(uint32_t registration,
					 unsigned int mask, uint32_t selection)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return setTraceRecordMaskForUser(mask, selection);
}

int _sceKernelPaGetGpuSampledData(uint32_t *values)
{
	uint32_t local[4] = { 0 };
	int result, copied;

	if (values == NULL)
		return getGpuSampledDataForUser(NULL);
	result = getGpuSampledDataForUser(local);
	copied = sceKernelCopyToUser(values, local, sizeof(local));
	return copied < 0 ? copied : result;
}

int _sceKernelPaSetupTraceBufferByKey(uint32_t registration,
				    const struct PamgrPrimaryTraceConfig *config)
{
	struct PamgrPrimaryTraceConfig local;
	int result;

	if (config != NULL) {
		result = sceKernelCopyFromUser(&local, config, sizeof(local));
		if (result < 0)
			return result;
		config = &local;
	}
	return configureRegisteredPrimaryTraceForUser(registration, config);
}

int _sceKernelPaAddCounterTraceByKey(uint32_t registration,
				   const struct PamgrCounterTraceConfig *config)
{
	struct PamgrCounterTraceConfig local;
	int result;

	if (config != NULL) {
		result = sceKernelCopyFromUser(&local, config, sizeof(local));
		if (result < 0)
			return result;
		config = &local;
	}
	return configureRegisteredCounterTraceForUser(registration, config);
}

int _sceKernelPaAddArmTraceByKey(uint32_t registration,
			       const struct PamgrArmTraceConfig *config)
{
	struct PamgrArmTraceConfig local;
	int result;

	if (config != NULL) {
		result = sceKernelCopyFromUser(&local, config, sizeof(local));
		if (result < 0)
			return result;
		config = &local;
	}
	return configureRegisteredArmTraceForUser(registration, config);
}

int _sceKernelPaAddBusTraceByKey(uint32_t registration, const uint32_t *filter)
{
	uint32_t local;
	int result;

	if (filter != NULL) {
		result = sceKernelCopyFromUser(&local, filter, sizeof(local));
		if (result < 0)
			return result;
		filter = &local;
	}
	return configureRegisteredBusTraceForUser(registration, filter);
}

int _sceKernelPaAddGpuTraceByKey(uint32_t registration,
			       const struct PamgrGpuTraceConfig *config)
{
	struct PamgrGpuTraceConfig local;
	int result;

	if (config != NULL) {
		result = sceKernelCopyFromUser(&local, config, sizeof(local));
		if (result < 0)
			return result;
		config = &local;
	}
	return configureRegisteredGpuTraceForUser(registration, config);
}

