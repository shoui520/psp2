/* SceSdstor device management (Sony source: sddev.c). */
#include <sysmem/sysroot.h>
#include "sdstor_internal.h"
#include <sysmem/debug.h>
#include <syscon/syscon.h>
#include <psp2kern/kernel/sysroot.h>
#include <psp2kern/kernel/gcauthmgr/gc_auth.h>
#include <psp2kern/lowio/pervasive.h>
#include <sdif/sdif.h>
#include <msif/msif.h>
#include <umass/umass.h>
#include <power/led.h>
#include <lowio/pervasive.h>

#if FW_DEBUG
static const uint8_t sddevHardwarePorts[5] = { 0, 1, 255, 255, 3 };
#endif
static const LedModeParam sddevReadEndLed = {
	.size = sizeof(LedModeParam), .blink = { 2, 3, 60, 0 }
};
#if FW_DEBUG
static const uint8_t sddevMediaTypes[5] = { 0, 0, 2, 3, 1 };
static const LedModeParam sddevWriteEndLed = {
	.size = sizeof(LedModeParam), .blink = { 2, 3, 60, 0 }
};
#endif
static const LedModeParam sddevActivityLed = {
	.size = sizeof(LedModeParam), .blink = { 2, 3, -1, 0 }
};
#if !FW_DEBUG
static const uint8_t sddevMediaTypes[5] = { 0, 0, 2, 3, 1 };
static const uint8_t sddevHardwarePorts[5] = { 0, 1, 255, 255, 3 };
static const LedModeParam sddevWriteEndLed = {
	.size = sizeof(LedModeParam), .blink = { 2, 3, 60, 0 }
};
#endif

static SceSdstorSddev sddevTables[5];

int ksceSdifGetCardInsertState1(unsigned int index);
int ksceSdifGetCardInsertState2(unsigned int index);
int ksceSdifInitializeMmcDevice(unsigned int index, SdifMmcContext **part);
int ksceSdifInitializeSdDevice(unsigned int index, SdifContextPart **part);
int sceMsifSwReset(void);
int SceUsbMassForDriver_D989A9F6(SceUsbMassDevInfo *info)
	__attribute__((weak));
int ksceSdifReadSectorMmc(SdifMmcContext *part, uint32_t sector,
			void *buffer, uint32_t count);
int ksceSdifWriteSectorMmc(SdifMmcContext *part, uint32_t sector,
			 const void *buffer, uint32_t count);
int ksceSdifReadSectorSd(SdifContextPart *part, uint32_t sector,
		       void *buffer, uint32_t count);
int ksceSdifWriteSectorSd(SdifContextPart *part, uint32_t sector,
			const void *buffer, uint32_t count);
int ksceMsifReadSector(uint32_t sector, void *buffer, unsigned int count);
int ksceMsifWriteSector(uint32_t sector, void *buffer, unsigned int count);
int SceUsbMassForDriver_B80D1DF8(uint32_t sector, void *buffer, uint32_t count)
	__attribute__((weak));
int SceUsbMassForDriver_081CA197(uint32_t sector, void *buffer, uint32_t count)
	__attribute__((weak));
int SceSdifForDriver_6CC8E28D(unsigned int index);
int SceMsifForDriver_F997286B(void);
int sendWriteProtectStatus(SdifMmcContext *part, unsigned int sector,
			   unsigned int count, int *writeProtected);
int SceSdifForDriver_E091BA2E(SdifContextPart *part, uint32_t sector,
			    uint32_t count, int *writeProtected);
int sceMsifFormat(int mode);
int SceSdifForDriver_35BA9DF8(unsigned int index, SceUID callback);

int sddev_power_off(SceSdstorSddev *device)
{
	unsigned int hport;
	int result = 0;

	SCE_DBG_ASSERT(device != NULL, 443);
	hport = device->hport;
	SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
	if (hport == 1) {
		ScePervasiveForDriver_5E20D7D5();
		result = ksceSysconCtrlSdPower(0);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 497, "sddev_power_off_gc",
				"sceSysconCtrlSdPower(0) returns 0x%08x\n", result);
			SCE_DBG_PRINTF_LEVEL(1, 15, 450,
				"sddev_power_off_gc returns 0x%08x\n", result);
		} else {
			result = 0;
		}
	}
	hport = device->hport;
	SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
	if (hport == 3)
		result = 0;
	if (device->mediaType == 2) {
		result = ksceSysconCtrlRMRPower(0);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 529, "sddev_power_off_rm",
				"sceSysconCtrlRMRPower(0) returns 0x%08x\n", result);
			SCE_DBG_PRINTF_LEVEL(1, 15, 462,
				"sddev_power_off_rm returns 0x%08x\n", result);
		} else {
			result = 0;
		}
	}
	return result;
}

int sddev_init(void)
{
	SceSdstorSddev *device = sddevTables;
	unsigned int port;
	uint8_t hport, mediaType;
	int result;

	for (port = 0; port != 5; ++port, ++device) {
		SCE_DBG_ASSERT_FUNC(1, 558, "sddev_get_object");
		SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
		hport = sddevHardwarePorts[port];
		SCE_DBG_ASSERT_FUNC(1, 596, "sddev_get_mtype");
		mediaType = sddevMediaTypes[port];
		SCE_DBG_ASSERT(1, 211);
		result = ksceKernelInitializeFastMutex(&device->fastmutex,
						     "SceSdStorDevice", 0, 0);
		if (result != 0) {
			SCE_DBG_PRINTF_LEVEL(1, 15, 220, "0x%08x\n", result);
			return result;
		}
		device->hport = hport;
		device->stat = 0;
		device->mediaType = mediaType;
		device->writeProtected = 0;
		device->mediaInfo.blockSize = 0;
		device->mediaInfo.sectorCount = 0;
		device->mediaInfo.eraseBlockCount = 0;
		device->sdif_ctx = NULL;
		if (ksceSysrootIsBsodReboot()) {
			result = sddev_power_off(device);
			if (result != 0)
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 182, "sddev_initialize",
					"sddev_power_off(%d) returns 0x%08x\n", port, result);
		}
	}
	return 0;
}

SceSdstorSddev *sddev_get_object(unsigned int port)
{
	SCE_DBG_ASSERT(port <= 4, 558);
	if (port > 4)
		return NULL;
	return &sddevTables[port];
}

int sddev_get_port(SceSdstorSddev *device)
{
#if FW_DEBUG
	SceSdstorSddev *candidate = sddevTables;
	int port;
#endif

	SCE_DBG_ASSERT(device != NULL, 570);
#if FW_DEBUG
	for (port = 0; port != 5; ++port, ++candidate) {
		SCE_DBG_ASSERT_FUNC(1, 558, "sddev_get_object");
		if (device == candidate)
			return port;
	}
#else
	if (device == &sddevTables[0])
		return 0;
	if (device == &sddevTables[1])
		return 1;
	if (device == &sddevTables[2])
		return 2;
	if (device == &sddevTables[3])
		return 3;
	if (device == &sddevTables[4])
		return 4;
#endif
	return -1;
}

int sddev_power_on(SceSdstorSddev *device)
{
	unsigned int hport, count;
	int result, powerResult, unlockResult;

	SCE_DBG_ASSERT_FUNC(device != NULL, 604, "sddev_open");
	result = ksceKernelLockFastMutex(&device->fastmutex);
	if (result < 0)
		return result;
	if (device->stat == 2)
		goto noDevice;
	if (device->stat == 1) {
		result = 0;
		goto unlock;
	}
	hport = device->hport;
	SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
	if (hport == 1) {
		if (ksceSdifGetCardInsertState1(device->hport) != 1)
			goto noDevice;
	} else if (device->mediaType == 2) {
		if (!kscePervasiveRemovableMemoryGetCardInsertState())
			goto noDevice;
	} else if (device->mediaType == 1) {
		if (ksceSdifGetCardInsertState2(device->hport) != 1)
			goto noDevice;
	}
	SCE_DBG_ASSERT(device != NULL, 414);
	hport = device->hport;
	SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
	powerResult = 0;
	if (hport == 1) {
		powerResult = ksceSysconCtrlSdPower(1);
		if (powerResult < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 477, "sddev_power_on_gc",
				"sceSysconCtrlSdPower(1) returns 0x%08x\n", powerResult);
			SCE_DBG_PRINTF_LEVEL(1, 15, 421,
				"sddev_power_on_gc returns 0x%08x\n", powerResult);
		} else {
			ScePervasiveForDriver_E6728E30();
			powerResult = 0;
		}
	}
	hport = device->hport;
	SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
	if (hport == 3)
		powerResult = 0;
	if (device->mediaType == 2) {
		powerResult = ksceSysconCtrlRMRPower(1);
		if (powerResult < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 513, "sddev_power_on_rm",
				"sceSysconCtrlRMRPower(1) returns 0x%08x\n", powerResult);
			SCE_DBG_PRINTF_LEVEL(1, 15, 433,
				"sddev_power_on_rm returns 0x%08x\n", powerResult);
		}
	}
	if (powerResult < 0) {
		result = powerResult;
		goto unlock;
	}
	hport = device->hport;
	SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
	if (hport == 1) {
		SdifMmcContext *mmc = NULL;

		for (count = 0; count != 2; ++count) {
			result = ksceSdifInitializeMmcDevice(device->hport, &mmc);
			if (result == 0) {
				device->mediaType = 0;
				device->stat = 1;
				device->mediaInfo.blockSize = mmc->blockLength;
				if (mmc->blockLength)
					device->mediaInfo.sectorCount =
						mmc->parameters.capacity / mmc->blockLength;
				device->mediaInfo.eraseBlockCount = mmc->activeEraseGroupSize;
				device->sdif_ctx = mmc;
				result = ksceSblGcAuthMgrGcAuthCartAuthentication(1);
				goto initialized;
			}
			if (result == (int)0x8032001a)
				break;
			if (result != (int)0x80320013)
				SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 284, "sddev_mmc_init",
					"init count = %d, ret_dev = 0x%08x\n", count, result);
		}
		if (!SceSysrootForDriver_ksceSysrootIsManufacturingMode() &&
		    !SceSysrootForDriver_F804F761())
			goto initialized;
		goto initializeSd;
	}
	hport = device->hport;
	SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
	if (hport == 0) {
		SdifMmcContext *mmc = NULL;

		for (count = 0; count != 2; ++count) {
			result = ksceSdifInitializeMmcDevice(device->hport, &mmc);
			if (result == 0) {
				device->mediaType = 0;
				device->stat = 1;
				device->mediaInfo.blockSize = mmc->blockLength;
				if (mmc->blockLength)
					device->mediaInfo.sectorCount =
						mmc->parameters.capacity / mmc->blockLength;
				device->mediaInfo.eraseBlockCount = mmc->activeEraseGroupSize;
				device->sdif_ctx = mmc;
				goto unlock;
			}
			if (result == (int)0x8032001a)
				break;
			if (result != (int)0x80320013)
				SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 284, "sddev_mmc_init",
					"init count = %d, ret_dev = 0x%08x\n", count, result);
		}
		goto initialized;
	}
	if (device->mediaType == 2) {
		SceMsInfo msInfo;

		for (count = 0; count != 2; ++count) {
			result = sceMsifSwReset();
			if (result == 0) {
				result = ksceMsifGetMsInfo(&msInfo);
				if (result == 0) {
					device->stat = 1;
					device->mediaType = 2;
					device->mediaInfo.blockSize = msInfo.sector_size;
					if (msInfo.sector_size)
						device->mediaInfo.sectorCount =
							msInfo.nbytes / msInfo.sector_size;
					device->mediaInfo.eraseBlockCount = msInfo.blockSize;
					device->sdif_ctx = NULL;
					if (msInfo.writeProtected)
						device->writeProtected = 1;
					goto unlock;
				}
			} else {
				if (result == (int)0x803d0007)
					break;
				if (result != (int)0x803d0021)
					SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 372, "sddev_ms_init",
						"init count = %d, ret_dev = 0x%08x\n", count, result);
			}
		}
		goto initialized;
	}
	hport = device->hport;
	SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
	if (hport == 3) {
		SdifContextPart *sd;

initializeSd:
		sd = NULL;
		for (count = 0; count != 2; ++count) {
			result = ksceSdifInitializeSdDevice(device->hport, &sd);
			if (result == 0) {
				device->mediaType = 1;
				device->stat = 1;
				device->mediaInfo.blockSize = sd->blockLength;
				if (sd->blockLength)
					device->mediaInfo.sectorCount =
						sd->parameters.capacity / sd->blockLength;
				device->mediaInfo.eraseBlockCount = sd->parameters.eraseGroupSize;
				device->sdif_ctx = sd;
				if (sd->writeProtected)
					device->writeProtected = 1;
				goto unlock;
			}
			if (result == (int)0x8032001a)
				break;
			if (result != (int)0x80320013)
				SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 322, "sddev_sd_init",
					"init count = %d, ret_dev = 0x%08x\n", count, result);
		}
		goto initialized;
	}
	if (device->mediaType == 3) {
		SceUsbMassDevInfo usbInfo;

		if (!SceUsbMassForDriver_D989A9F6) {
			result = 0x80010013;
			goto failed;
		}
		result = SceUsbMassForDriver_D989A9F6(&usbInfo);
		if (result == 0) {
			if (usbInfo.sectorCount >= UINT64_C(0x100000000)) {
				result = 0x80024902;
				goto failed;
			}
			device->mediaType = 3;
			device->mediaInfo.sectorCount = usbInfo.sectorCount;
			device->sdif_ctx = NULL;
			device->mediaInfo.blockSize = usbInfo.blockSize;
			device->mediaInfo.eraseBlockCount = usbInfo.blockSize;
			device->stat = 1;
			if (usbInfo.writeProtected)
				device->writeProtected = 1;
			goto unlock;
		}
	} else {
		result = -1;
		goto failed;
	}
initialized:
	if (result >= 0)
		goto unlock;
failed:
	powerResult = sddev_power_off(device);
	device->stat = 0;
	device->sdif_ctx = NULL;
	if (powerResult < 0)
		result = powerResult;
	goto unlock;
noDevice:
	result = 0x80010013;
unlock:
	unlockResult = ksceKernelUnlockFastMutex(&device->fastmutex);
	return unlockResult < 0 ? unlockResult : result;
}

int sddev_close(SceSdstorSddev *device)
{
	int result;

	SCE_DBG_ASSERT(device != NULL, 684);
	result = ksceKernelLockFastMutex(&device->fastmutex);
	if (result < 0)
		return result;
	device->stat = 0;
	device->mediaInfo.blockSize = 0;
	device->mediaInfo.sectorCount = 0;
	device->mediaInfo.eraseBlockCount = 0;
	device->writeProtected = 0;
	device->sdif_ctx = NULL;
	result = ksceKernelUnlockFastMutex(&device->fastmutex);
	return result < 0 ? result : 0;
}

int sddev_remove(SceSdstorSddev *device)
{
	unsigned int hport;
	int result, unlockResult;

	SCE_DBG_ASSERT(device != NULL, 703);
	result = ksceKernelLockFastMutex(&device->fastmutex);
	if (result < 0)
		return result;
	device->stat = 0;
	SCE_DBG_ASSERT_FUNC(device != NULL, 443, "sddev_power_off");
	hport = device->hport;
	SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
	result = 0;
	if (hport == 1) {
		ScePervasiveForDriver_5E20D7D5();
		result = ksceSysconCtrlSdPower(0);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 497, "sddev_power_off_gc",
				"sceSysconCtrlSdPower(0) returns 0x%08x\n", result);
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 450, "sddev_power_off",
				"sddev_power_off_gc returns 0x%08x\n", result);
		} else {
			result = 0;
		}
	}
	hport = device->hport;
	SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
	if (hport == 3)
		result = 0;
	if (device->mediaType == 2) {
		result = ksceSysconCtrlRMRPower(0);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 529, "sddev_power_off_rm",
				"sceSysconCtrlRMRPower(0) returns 0x%08x\n", result);
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 462, "sddev_power_off",
				"sddev_power_off_rm returns 0x%08x\n", result);
		} else {
			result = 0;
		}
	}
	if (result == 0) {
		result = ksceKernelUnlockFastMutex(&device->fastmutex);
		return result < 0 ? result : 0;
	}
	unlockResult = ksceKernelUnlockFastMutex(&device->fastmutex);
	return unlockResult < 0 ? unlockResult : result;
}

int sddev_insert(SceSdstorSddev *device)
{
	int result;

	SCE_DBG_ASSERT(device != NULL, 725);
	SCE_DBG_ASSERT_FUNC(device != NULL, 684, "sddev_close");
	result = ksceKernelLockFastMutex(&device->fastmutex);
	if (result < 0)
		goto closeFailed;
	device->stat = 0;
	device->mediaInfo.blockSize = 0;
	device->mediaInfo.sectorCount = 0;
	device->mediaInfo.eraseBlockCount = 0;
	device->writeProtected = 0;
	device->sdif_ctx = NULL;
	result = ksceKernelUnlockFastMutex(&device->fastmutex);
	if (result < 0)
		goto closeFailed;
	result = sddev_power_on(device);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(2, 15, 736,
			"sddev_open() returns 0x%08x\n", result);
		return result;
	}
	return 0;
closeFailed:
	SCE_DBG_PRINTF_LEVEL(2, 15, 731,
		"sddev_close() returns 0x%08x\n", result);
	return result;
}

int sddev_suspend(SceSdstorSddev *device)
{
	unsigned int hport;
	int result, unlockResult;

	SCE_DBG_ASSERT(device != NULL, 746);
	result = ksceKernelLockFastMutex(&device->fastmutex);
	if (result < 0)
		return result;
	device->stat = 2;
	SCE_DBG_ASSERT_FUNC(device != NULL, 443, "sddev_power_off");
	hport = device->hport;
	SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
	result = 0;
	if (hport == 1) {
		ScePervasiveForDriver_5E20D7D5();
		result = ksceSysconCtrlSdPower(0);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 497, "sddev_power_off_gc",
				"sceSysconCtrlSdPower(0) returns 0x%08x\n", result);
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 450, "sddev_power_off",
				"sddev_power_off_gc returns 0x%08x\n", result);
		} else {
			result = 0;
		}
	}
	hport = device->hport;
	SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
	if (hport == 3)
		result = 0;
	if (device->mediaType == 2) {
		result = ksceSysconCtrlRMRPower(0);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 529, "sddev_power_off_rm",
				"sceSysconCtrlRMRPower(0) returns 0x%08x\n", result);
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 462, "sddev_power_off",
				"sddev_power_off_rm returns 0x%08x\n", result);
		} else {
			result = 0;
		}
	}
	if (result == 0) {
		result = ksceKernelUnlockFastMutex(&device->fastmutex);
		return result < 0 ? result : 0;
	}
	unlockResult = ksceKernelUnlockFastMutex(&device->fastmutex);
	return unlockResult < 0 ? unlockResult : result;
}

int sddev_resume(SceSdstorSddev *device)
{
	unsigned int hport;
	int result;

	SCE_DBG_ASSERT(device != NULL, 768);
	hport = device->hport;
	SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
	if (hport == 0)
		return 0;
	SCE_DBG_ASSERT_FUNC(device != NULL, 684, "sddev_close");
	result = ksceKernelLockFastMutex(&device->fastmutex);
	if (result < 0)
		goto closeFailed;
	device->stat = 0;
	device->mediaInfo.blockSize = 0;
	device->mediaInfo.sectorCount = 0;
	device->mediaInfo.eraseBlockCount = 0;
	device->writeProtected = 0;
	device->sdif_ctx = NULL;
	result = ksceKernelUnlockFastMutex(&device->fastmutex);
	if (result < 0)
		goto closeFailed;
	result = sddev_power_on(device);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(2, 15, 783,
			"sddev_open() returns 0x%08x\n", result);
		return result;
	}
	return 0;
closeFailed:
	SCE_DBG_PRINTF_LEVEL(2, 15, 778,
		"sddev_close() returns 0x%08x\n", result);
	return result;
}

int sddev_read(SceSdstorSddev *device, void *buffer, uint32_t sector,
	       uint32_t sectorCount)
{
	SdifContextPart *part;
	unsigned int hport;
	uint16_t count = sectorCount;
	int result, unlockResult, initResult, attempt;

	SCE_DBG_ASSERT(device != NULL, 793);
	result = ksceKernelLockFastMutex(&device->fastmutex);
	if (result < 0)
		return result;
	if (device->stat != 1) {
		result = ksceKernelUnlockFastMutex(&device->fastmutex);
		return result < 0 ? result : (int)0x80010013;
	}
	switch (device->mediaType) {
	case 0:
		hport = device->hport;
		SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
		if (hport == 1)
			SceSysrootForDriver_9BF782CA(1, 2, &sddevActivityLed);
		result = ksceSdifReadSectorMmc(device->sdif_ctx, sector, buffer, count);
		hport = device->hport;
		SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
		if (hport == 1)
			SceSysrootForDriver_9BF782CA(1, 2, &sddevReadEndLed);
		break;
	case 1:
		hport = device->hport;
		SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
		if (hport == 1)
			SceSysrootForDriver_9BF782CA(1, 2, &sddevActivityLed);
		result = ksceSdifReadSectorSd(device->sdif_ctx, sector, buffer, count);
		if (result == (int)0x80320022) {
			part = NULL;
			for (attempt = 0; attempt < 2; attempt++) {
				initResult = ksceSdifInitializeSdDevice(device->hport, &part);
				if (initResult == (int)0x8032001a)
					break;
				if (initResult != (int)0x80320013)
					SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 322, "sddev_sd_init",
						"init count = %d, ret_dev = 0x%08x\n",
						attempt, initResult);
			}
			SCE_DBG_PRINTF_LEVEL(1, 15, 814, "ESD Hang. Tried to recover\n");
		}
		hport = device->hport;
		SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
		if (hport == 1)
			SceSysrootForDriver_9BF782CA(1, 2, &sddevReadEndLed);
		break;
	case 2:
		SceSysrootForDriver_9BF782CA(3, 2, &sddevActivityLed);
		result = ksceMsifReadSector(sector, buffer, count);
		SceSysrootForDriver_9BF782CA(3, 2, &sddevReadEndLed);
		break;
	case 3:
		if (SceUsbMassForDriver_B80D1DF8)
			result = SceUsbMassForDriver_B80D1DF8(sector, buffer, count);
		else
			result = 0x80010013;
		break;
	default:
		result = 0x80010006;
		break;
	}
	unlockResult = ksceKernelUnlockFastMutex(&device->fastmutex);
	return unlockResult < 0 ? unlockResult : result;
}

int sddev_write(SceSdstorSddev *device, const void *buffer, uint32_t sector,
	       uint32_t sectorCount)
{
	SdifContextPart *part;
	unsigned int hport;
	uint16_t count = sectorCount;
	int result, unlockResult, initResult, attempt;

	SCE_DBG_ASSERT(device != NULL, 858);
	result = ksceKernelLockFastMutex(&device->fastmutex);
	if (result < 0)
		return result;
	if (device->stat != 1) {
		result = ksceKernelUnlockFastMutex(&device->fastmutex);
		return result < 0 ? result : (int)0x80010013;
	}
	switch (device->mediaType) {
	case 0:
		hport = device->hport;
		SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
		if (hport == 1)
			SceSysrootForDriver_9BF782CA(1, 2, &sddevActivityLed);
		result = ksceSdifWriteSectorMmc(device->sdif_ctx, sector, buffer, count);
		hport = device->hport;
		SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
		if (hport == 1)
			SceSysrootForDriver_9BF782CA(1, 2, &sddevWriteEndLed);
		break;
	case 1:
		hport = device->hport;
		SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
		if (hport == 1)
			SceSysrootForDriver_9BF782CA(1, 2, &sddevActivityLed);
		result = ksceSdifWriteSectorSd(device->sdif_ctx, sector, buffer, count);
		if (result == (int)0x80320022) {
			part = NULL;
			for (attempt = 0; attempt < 2; attempt++) {
				initResult = ksceSdifInitializeSdDevice(device->hport, &part);
				if (initResult == (int)0x8032001a)
					break;
				if (initResult != (int)0x80320013)
					SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 322, "sddev_sd_init",
						"init count = %d, ret_dev = 0x%08x\n",
						attempt, initResult);
			}
			SCE_DBG_PRINTF_LEVEL(1, 15, 879, "ESD Hang. Tried to recover\n");
		}
		hport = device->hport;
		SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
		if (hport == 1)
			SceSysrootForDriver_9BF782CA(1, 2, &sddevWriteEndLed);
		break;
	case 2:
		SceSysrootForDriver_9BF782CA(3, 2, &sddevActivityLed);
		result = ksceMsifWriteSector(sector, (void *)buffer, count);
		SceSysrootForDriver_9BF782CA(3, 2, &sddevWriteEndLed);
		break;
	case 3:
		if (SceUsbMassForDriver_081CA197)
			result = SceUsbMassForDriver_081CA197(sector, (void *)buffer, count);
		else
			result = 0x80010013;
		break;
	default:
		result = 0x80010006;
		break;
	}
	unlockResult = ksceKernelUnlockFastMutex(&device->fastmutex);
	return unlockResult < 0 ? unlockResult : result;
}

int sddev_check_removal(SceSdstorSddev *device)
{
	unsigned int hport;
	int result, unlockResult;

	SCE_DBG_ASSERT(device != NULL, 922);
	result = ksceKernelLockFastMutex(&device->fastmutex);
	if (result < 0)
		return result;
	if (device->stat != 1) {
		result = ksceKernelUnlockFastMutex(&device->fastmutex);
		return result < 0 ? result : (int)0x80010013;
	}
	if (device->mediaType == 0) {
		hport = device->hport;
		SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
		if (hport == 1)
			result = SceSdifForDriver_6CC8E28D(device->hport);
		else
			result = 0x80010010;
	} else if (device->mediaType == 2) {
		result = SceMsifForDriver_F997286B();
	} else {
		result = 0x80010030;
	}
	unlockResult = ksceKernelUnlockFastMutex(&device->fastmutex);
	return unlockResult < 0 ? unlockResult : result;
}

int sddev_has_write_protect_area(SceSdstorSddev *device, uint32_t sector,
				 uint32_t count, uint8_t *writeProtected)
{
	void *part;
	int result, unlockResult, protected = 0;

	SCE_DBG_ASSERT(device != NULL, 957);
	result = ksceKernelLockFastMutex(&device->fastmutex);
	if (result < 0)
		return result;
	if (device->stat != 1) {
		result = ksceKernelUnlockFastMutex(&device->fastmutex);
		return result < 0 ? result : (int)0x80010013;
	}
	switch (device->mediaType) {
	case 0:
		part = device->sdif_ctx;
		SCE_DBG_ASSERT(part != NULL, 971);
		result = sendWriteProtectStatus(part, sector, count, &protected);
		if (result == 0)
			*writeProtected = protected;
		break;
	case 1:
		part = device->sdif_ctx;
		SCE_DBG_ASSERT(part != NULL, 981);
		result = SceSdifForDriver_E091BA2E(part, sector, count, &protected);
		if (result == 0)
			*writeProtected = protected;
		break;
	case 2:
	case 3:
		result = 0;
		*writeProtected = 0;
		break;
	default:
		result = 0x80010013;
		break;
	}
	unlockResult = ksceKernelUnlockFastMutex(&device->fastmutex);
	return unlockResult < 0 ? unlockResult : result;
}

int sddev_format_rm(SceSdstorSddev *device)
{
	int result, unlockResult;

	SCE_DBG_ASSERT(device != NULL, 1007);
	result = ksceKernelLockFastMutex(&device->fastmutex);
	if (result < 0)
		return result;
	if (device->stat != 1 || device->mediaType != 2) {
		result = ksceKernelUnlockFastMutex(&device->fastmutex);
		return result < 0 ? result : (int)0x80010013;
	}
	result = sceMsifFormat(1);
	if (result != 0)
		SCE_DBG_PRINTF_LEVEL(1, 15, 1025, "0x%08x\n", result);
	unlockResult = ksceKernelUnlockFastMutex(&device->fastmutex);
	return unlockResult < 0 ? unlockResult : result;
}

int sddev_erase_sd(SceSdstorSddev *device, SceUID callback)
{
	unsigned int hport;
	int result, unlockResult;

	SCE_DBG_ASSERT(device != NULL, 1036);
	result = ksceKernelLockFastMutex(&device->fastmutex);
	if (result < 0)
		return result;
	if (device->stat != 1 || device->mediaType != 1) {
		result = ksceKernelUnlockFastMutex(&device->fastmutex);
		return result < 0 ? result : (int)0x80010013;
	}
	hport = device->hport;
	SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
	if (hport != 1) {
		hport = device->hport;
		SCE_DBG_ASSERT_FUNC(1, 588, "sddev_get_hport");
		if (hport != 3)
			return 0x80010016;
	}
	result = SceSdifForDriver_35BA9DF8(device->hport, callback);
	if (result != 0)
		SCE_DBG_PRINTF_LEVEL(1, 15, 1060, "0x%08x\n", result);
	unlockResult = ksceKernelUnlockFastMutex(&device->fastmutex);
	return unlockResult < 0 ? unlockResult : result;
}

int sddev_is_read_write(SceSdstorSddev *device)
{
	int result, unlockResult;

	SCE_DBG_ASSERT(device != NULL, 1071);
	result = ksceKernelLockFastMutex(&device->fastmutex);
	if (result < 0)
		return result;
	if (device->stat != 1) {
		result = ksceKernelUnlockFastMutex(&device->fastmutex);
		return result < 0 ? result : (int)0x80010013;
	}
	result = device->writeProtected & 1 ? (int)0x8001000d : 0;
	unlockResult = ksceKernelUnlockFastMutex(&device->fastmutex);
	return unlockResult < 0 ? unlockResult : result;
}

int sddev_get_info(SceSdstorSddev *device, SceSdstorMediaInfo *info)
{
	int result;

	SCE_DBG_ASSERT(device != NULL && info != NULL, 1094);
	result = ksceKernelLockFastMutex(&device->fastmutex);
	if (result < 0)
		return result;
	if (device->stat != 1) {
		result = ksceKernelUnlockFastMutex(&device->fastmutex);
		return result < 0 ? result : (int)0x80010013;
	}
	*info = device->mediaInfo;
	result = ksceKernelUnlockFastMutex(&device->fastmutex);
	return result < 0 ? result : 0;
}
