#include "pamgr_internal.h"
#include <string.h>
#include <psp2kern/kernel/sysmem/address_space.h>
#include <psp2kern/kernel/sysmem/data_transfers.h>
#include <sysmem/debug.h>
#include <processmgr/processmgr.h>
#include <threadmgr/threadmgr.h>
#include <kernel/error.h>

uint64_t ScePamgrForDriver_FE9BFD27(void);
int setTraceWritePointer(uint32_t pointer);
int setTraceInterruptStatus(uint32_t status);
int registerTraceInterruptCallback(PamgrTraceInterruptCallback callback, void *arg);
int unregisterTraceInterruptCallback(void);
int clearArmTrace(void);
int setTracePackedFilter(const struct PamgrPackedFilter *filter);
int clearTracePackedFilter(void);
int setTraceWordFilter(const uint32_t *filter);
int clearTraceWordFilter(void);
int setTraceBuffer1WritePointer(uint32_t pointer);
int configureTraceBuffer1(const struct PamgrTraceBufferConfig *config);
int setTraceRecordMask(unsigned int mask, uint32_t selection);
int setTraceBitControls(const uint32_t *controls);
int configureTraceSamples(const volatile struct PamgrSampleConfig *config);
void configureTraceControl(uint32_t config, uint32_t half);
int configurePrimaryTrace(const volatile struct PamgrPrimaryTraceConfig *config);
int SceSysmemForDriver_A7C0D1FC(const void *address);
int configureArmTrace(const volatile struct PamgrArmTraceConfig *config);
uint32_t ksceKernelCpuGetCONTEXTIDR(void);

int __attribute__((noinline)) configureArmTraceForUser(
		const struct PamgrArmTraceConfig *config)
{
	struct PamgrArmTraceConfig local;

	if (config == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	local = *config;
	if (local.modeAndContext - 1 <= 1)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (local.modeAndContext - 5 <= 1)
		local.modeAndContext |= ksceKernelCpuGetCONTEXTIDR() << 24;
	return configureArmTrace(&local);
}

int ScePamgrForDriver_5D3D46E4(uint32_t registration,
			    const struct PamgrArmTraceConfig *config)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return configureArmTrace(config);
}

int PamgrProcKill(SceUID process)
{
	int result;

	result = ksceKernelLockFastMutex(&pamgrState.registrationMutex);
	if (result < 0) {
		SCE_DBG_PRINTF(0, 82,
			"FATAL: sceKernelLockFastMutex failed (ret=0x%08x)\n", result);
		return result;
	}
	if (pamgrState.registrationProcess == process) {
		pamgrState.registrationId = 0;
		pamgrState.registrationShared = 0;
	}
	result = ksceKernelUnlockFastMutex(&pamgrState.registrationMutex);
	if (result < 0) {
		SCE_DBG_PRINTF(0, 95,
			"FATAL: sceKernelUnlockFastMutex failed (ret=0x%08x)\n", result);
		return result;
	}
	return 0;
}

int PamgrProcExit(SceUID process)
{
	int result;

	result = ksceKernelLockFastMutex(&pamgrState.registrationMutex);
	if (result < 0) {
		SCE_DBG_PRINTF(0, 52,
			"FATAL: sceKernelLockFastMutex failed (ret=0x%08x)\n", result);
		return result;
	}
	if (pamgrState.registrationProcess == process) {
		pamgrState.registrationId = 0;
		pamgrState.registrationShared = 0;
	}
	result = ksceKernelUnlockFastMutex(&pamgrState.registrationMutex);
	if (result < 0) {
		SCE_DBG_PRINTF(0, 65,
			"FATAL: sceKernelUnlockFastMutex failed (ret=0x%08x)\n", result);
		return result;
	}
	return 0;
}

/* Exact producer record also used by ModulemgrProcEvent; shared migration
 * belongs to the coordinator. Exit/kill are the only Pamgr callbacks. */
struct PamgrProcessEvents {
	SceSize size;
	void *create;
	int (*exit)(SceUID process);
	int (*kill)(SceUID process);
	void *stop;
	void *start;
	void *switchProcess;
};

int ksceKernelRegisterProcEventHandler(const char *name, const void *handlers,
				     int flags);

int sceKernelPaInitPamgrWrapper(void)
{
	struct PamgrProcessEvents events = {
		.size = sizeof(events),
		.exit = PamgrProcExit,
		.kill = PamgrProcKill,
	};
	int result;

	result = ksceKernelRegisterProcEventHandler("ScePamgr", &events, 0);
	if (result < 0) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 121);
		return result;
	}
	memset(&pamgrState.registrationMutex, 0,
	       sizeof(pamgrState) - offsetof(struct PamgrState, registrationMutex));
	result = ksceKernelInitializeFastMutex(&pamgrState.registrationMutex,
					      "SceKernelPaControlLock", 2, NULL);
	if (result < 0) {
		SCE_DBG_PRINTF(0, 130,
			"FATAL: sceKernelInitializeFastMutex failed (0x%08x)\n", result);
		return result;
	}
	return 0;
}
int configureCounterTrace(const volatile struct PamgrCounterTraceConfig *config);
int clearCounterTrace(void);
int startTrace(unsigned int channel);
int stopTrace(unsigned int channel);
int configureGpuTrace(const volatile struct PamgrGpuTraceConfig *config);
int clearGpuTrace(void);

/* Distinct user entries add the user-pointer NULL gate. */
int __attribute__((noinline)) configureGpuTraceForUser(
				const struct PamgrGpuTraceConfig *config)
{
	if (config == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return configureGpuTrace(config);
}

int __attribute__((noinline)) clearGpuTraceForUser(void)
{
	return clearGpuTrace();
}

int ScePamgrForDriver_49A43754(uint32_t registration,
			     const struct PamgrGpuTraceConfig *config)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return configureGpuTrace(config);
}

int ScePamgrForDriver_9935D8EF(uint32_t registration)
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

int sceKernelPaRemoveGpuTraceByKey(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return clearGpuTraceForUser();
}

/* Distinct driver/user ABI entries select the trace channel. */
int __attribute__((noinline)) startPrimaryTrace(void)
{
	return startTrace(0);
}

int __attribute__((noinline)) startSecondaryTrace(void)
{
	return startTrace(1);
}

int __attribute__((noinline)) stopPrimaryTrace(void)
{
	return stopTrace(0);
}

int __attribute__((noinline)) stopSecondaryTrace(void)
{
	return stopTrace(1);
}

int __attribute__((noinline)) startPrimaryTraceForUser(void)
{
	return startTrace(0);
}

int __attribute__((noinline)) stopPrimaryTraceForUser(void)
{
	return stopTrace(0);
}

int ScePamgrForDriver_17C81A92(uint32_t registration,
			     const struct PamgrCounterTraceConfig *config)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return configureCounterTrace(config);
}

int ScePamgrForDriver_0B7F6F53(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return clearCounterTrace();
}

/* User virtual buffer adapter, translation precedes validation. */
int __attribute__((noinline)) configurePrimaryTraceForUser(
				const struct PamgrPrimaryTraceConfig *config)
{
	struct PamgrPrimaryTraceConfig physicalConfig;
	uintptr_t physical;
	uint32_t virtual;
	int result;

	if (config == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	physicalConfig = *config;
	virtual = physicalConfig.base;
	result = ksceKernelVAtoPA((const void *)virtual, &physical);
	if (result < 0)
		return result;
	if (SceSysmemForDriver_A7C0D1FC((const void *)virtual) < 0)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	pamgrState.traceBufferVirtual = virtual;
	pamgrState.traceBufferPhysical = physical;
	physicalConfig.base = physical;
	return configurePrimaryTrace(&physicalConfig);
}

int ScePamgrForDriver_F2D5105A(uint32_t registration,
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

int ScePamgrForDriver_0BCD4966(uint32_t registration, unsigned int mask,
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

int ScePamgrForDriver_B9EE1620(uint32_t registration)
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

int ScePamgrForDriver_DEC5313E(uint32_t registration, const uint32_t *filter)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return setTraceWordFilter(filter);
}

int ScePamgrForDriver_DB62FEFE(uint32_t registration)
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

	result = ksceKernelLockFastMutex(&pamgrState.registrationMutex);
	if (result < 0) {
		SCE_DBG_PRINTF(0, 146,
			"FATAL: sceKernelLockFastMutex failed (ret=0x%08x)\n", result);
		return result;
	}
	result = SCE_KERNEL_ERROR_PA_NOT_AVAILABLE;
	if (pamgrState.registrationId == 0) {
		result = (uint32_t)ScePamgrForDriver_FE9BFD27() & 0x7fffffff;
		pamgrState.registrationId = result;
		pamgrState.registrationProcess = sceKernelGetProcessId();
		pamgrState.registrationShared = 0;
	}
	unlocked = ksceKernelUnlockFastMutex(&pamgrState.registrationMutex);
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

	result = ksceKernelLockFastMutex(&pamgrState.registrationMutex);
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
	unlocked = ksceKernelUnlockFastMutex(&pamgrState.registrationMutex);
	if (unlocked < 0) {
		SCE_DBG_PRINTF(0, 195,
			"FATAL: sceKernelUnlockFastMutex failed (ret=0x%08x)\n", unlocked);
		return unlocked;
	}
	return result;
}

int sceKernelPaRegisterShared(uint32_t first, uint32_t second)
{
	int result, unlocked;

	result = ksceKernelLockFastMutex(&pamgrState.registrationMutex);
	if (result < 0) {
		SCE_DBG_PRINTF(0, 211,
			"FATAL: sceKernelLockFastMutex failed (ret=0x%08x)\n", result);
		return result;
	}
	result = SCE_KERNEL_ERROR_PA_NOT_AVAILABLE;
	if (pamgrState.registrationId == 0) {
		result = (uint32_t)ScePamgrForDriver_FE9BFD27() & 0x7fffffff;
		pamgrState.registrationId = result;
		pamgrState.registrationProcess = sceKernelGetProcessId();
		pamgrState.registrationShared = 1;
		pamgrState.sharedKeyFirst = first;
		pamgrState.sharedKeySecond = second;
	}
	unlocked = ksceKernelUnlockFastMutex(&pamgrState.registrationMutex);
	if (unlocked < 0) {
		SCE_DBG_PRINTF(0, 230,
			"FATAL: sceKernelUnlockFastMutex failed (ret=0x%08x)\n", unlocked);
		return unlocked;
	}
	return result;
}

/* ForUser suffixes distinguish the separate user-library ABI entries. */
int sceKernelPaGetSharedKey(uint32_t first, uint32_t second)
{
	if (pamgrState.registrationShared != 1)
		return SCE_KERNEL_ERROR_PA_KEY_IS_NOT_SHARED;
	if (first != pamgrState.sharedKeyFirst || second != pamgrState.sharedKeySecond)
		return SCE_KERNEL_ERROR_PA_INVALID_SIGNATURE;
	return pamgrState.registrationId;
}

int ScePamgrForDriver_0D04B52A(uint32_t first, uint32_t second)
{
	if (pamgrState.registrationShared != 1)
		return SCE_KERNEL_ERROR_PA_KEY_IS_NOT_SHARED;
	if (first != pamgrState.sharedKeyFirst || second != pamgrState.sharedKeySecond)
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

int sceKernelPaRegisterSharedForUser(uint32_t first, uint32_t second)
{
	return sceKernelPaRegisterShared(first, second);
}

int ScePamgrForDriver_50FD85C8(uint32_t registration)
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

int ScePamgrForDriver_88462719(uint32_t registration)
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

void clearBusErrorLog(void);
void clearSecureBusErrorLog(void);
void enableTraceAuxControl0(void);
void enableTraceAuxControl1(void);
void disableTraceAuxControl1(void);

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

int sceKernelPaAddCounterTraceForUser(const struct PamgrCounterTraceConfig *config);
int clearCounterTraceForUser(void);
int clearArmTraceForUser(void);
int setTraceWordFilterForUser(const uint32_t *filter);
int clearTraceWordFilterForUser(void);
int setTraceRecordMaskForUser(unsigned int mask, uint32_t selection);

int __attribute__((noinline)) configureRegisteredCounterTraceForUser(uint32_t registration,
				const struct PamgrCounterTraceConfig *config)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return sceKernelPaAddCounterTraceForUser(config);
}

int sceKernelPaRemoveCounterTraceByKey(uint32_t registration)
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

int sceKernelPaRemoveArmTraceByKey(uint32_t registration)
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

int sceKernelPaRemoveBusTraceByKey(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return clearTraceWordFilterForUser();
}

int sceKernelPaStartByKey(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return startPrimaryTraceForUser();
}

int sceKernelPaStopByKey(uint32_t registration)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return stopPrimaryTraceForUser();
}

int sceKernelPaSetBookmarkChannelEnableByKey(uint32_t registration,
					 unsigned int mask, uint32_t selection)
{
	if (registration == 0 || registration != pamgrState.registrationId)
		return SCE_KERNEL_ERROR_PA_INVALID_KEY;
	return setTraceRecordMaskForUser(mask, selection);
}

int getGpuSampledDataForUser(uint32_t *values);

int _sceKernelPaGetGpuSampledData(uint32_t *values)
{
	uint32_t local[4] = { 0 };
	int result, copied;

	if (values == NULL)
		return getGpuSampledDataForUser(NULL);
	result = getGpuSampledDataForUser(local);
	copied = ksceKernelCopyToUser(values, local, sizeof(local));
	return copied < 0 ? copied : result;
}

int _sceKernelPaSetupTraceBufferByKey(uint32_t registration,
				    const struct PamgrPrimaryTraceConfig *config)
{
	struct PamgrPrimaryTraceConfig local;
	int result;

	if (config != NULL) {
		result = ksceKernelCopyFromUser(&local, config, sizeof(local));
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
		result = ksceKernelCopyFromUser(&local, config, sizeof(local));
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
		result = ksceKernelCopyFromUser(&local, config, sizeof(local));
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
		result = ksceKernelCopyFromUser(&local, filter, sizeof(local));
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
		result = ksceKernelCopyFromUser(&local, config, sizeof(local));
		if (result < 0)
			return result;
		config = &local;
	}
	return configureRegisteredGpuTraceForUser(registration, config);
}

int ksceKernelCheckDipsw(unsigned int number);
int initializeTraceHardware(void);

int module_start(void)
{
	if (ksceKernelCheckDipsw(0xd4) == 0)
		return 1;
	if (initializeTraceHardware() < 0)
		return 2;
	if (sceKernelPaInitPamgrWrapper() < 0)
		return 2;
	pamgrState.syscallLeaveObserver = NULL;
	pamgrState.syscallEnterObserver = NULL;
	pamgrState.syscallEnterCallback = NULL;
	pamgrState.syscallLeaveCallback = NULL;
	return 0;
}
