/* Library entry points without source-file records in the originals. */
#include "library.h"

struct MsproalHandle *gMsproalHandle;
struct MsproalErrorLog gMsproalErrorLog;
const struct MsproalOperations *gMsproalOperations;

const struct MsproalOperations gMsproalProOperations = {
	.readAttributes = MsproalReadAttributesPro,
	.read = MsproalReadPro,
	.write = MsproalWritePro,
	.getModelName = MsproalGetModelNamePro,
	.setPowerClass = MsproalSetPowerClassPro,
	.format = MsproalFormatPro,
	.mount = MsproalMountPro,
	.sleep = MsproalSleepPro,
	.stopAndCheckStatus = MsproalStopAndCheckStatusPro,
	.reset = MsproalResetPro,
	.stop = MsproalStopPro,
};

static const uint32_t gMsproalLegacySectorCounts[] = {
	7904, 15840, 31680, 63424, 126848, 253696,
};

int msproal_init(void)
{
	struct MsproalErrorRecord *records;
	uint16_t errorLogConfiguration[4];
	int result;

	gMsproalHandle = 0;
	gMsproalOperations = &gMsproalDefaultOperations;
	result = MsproalUserInitialize();
	if (result == 0) {
		MsproalUserGetErrorLogBuffer(&records, errorLogConfiguration);
		gMsproalErrorLog.records = records;
		gMsproalErrorLog.capacity = errorLogConfiguration[0];
		MsproalInitializeErrorLog(gMsproalHandle,
			errorLogConfiguration[0], &gMsproalErrorLog.index, records);
	}
	return result;
}

int msproal_mount(int busMode)
{
	int result;
	int stopResult;

	if (gMsproalHandle == 0)
		return -13;
	if ((gMsproalHandle->mediaType & 0x20) == 0) {
		gMsproalOperations = &gMsproalDefaultOperations;
		return -7;
	}
	gMsproalOperations = &gMsproalProOperations;
	result = MsproalMountPro(gMsproalHandle, busMode);
	if (result == 0)
		goto mounted;

	MsproalRecordError(gMsproalHandle, 2, result,
		&gMsproalErrorLog.index,
		&gMsproalErrorLog.records[gMsproalErrorLog.index %
			gMsproalErrorLog.capacity]);
	if (result == -12)
		return result;
	stopResult = gMsproalOperations->stop(gMsproalHandle);
	if (stopResult == 0) {
		if (result == -6 || result == 1)
			goto mounted;
		return result;
	}
	MsproalRecordError(gMsproalHandle, 13, stopResult,
		&gMsproalErrorLog.index,
		&gMsproalErrorLog.records[gMsproalErrorLog.index %
			gMsproalErrorLog.capacity]);
	return stopResult;

mounted:
	gMsproalHandle->mountFlags |= 1;
	return result;
}

int msproal_start(struct MsproalHandle *handle)
{
	struct MsifRegisterWindow *registers;
	int result;

	result = MsproalUserGetRegisters(&registers);
	if (result != 0)
		return result;
	gMsproalHandle = handle;
	result = MsproalInitializeHandle(handle, registers);
	if (result != 0)
		return result;
	result = MsproalIdentifyMedia(gMsproalHandle,
		&gMsproalHandle->mediaType, &gMsproalHandle->writeEnabled,
		&gMsproalHandle->writeProtected);
	if (result != 0)
		MsproalRecordError(gMsproalHandle, 1, result,
			&gMsproalErrorLog.index,
			&gMsproalErrorLog.records[gMsproalErrorLog.index %
				gMsproalErrorLog.capacity]);
	return result;
}

int msproal_check_media(uint8_t *mediaType)
{
	uint32_t identifiedType;
	int writeEnabled;
	int writeProtected;
	int result;

	if (gMsproalHandle == 0)
		return -13;
	result = MsproalIdentifyMedia(gMsproalHandle, &identifiedType,
		&writeEnabled, &writeProtected);
	if (result == 0)
		*mediaType = identifiedType;
	else
		MsproalRecordError(gMsproalHandle, 18, result,
			&gMsproalErrorLog.index,
			&gMsproalErrorLog.records[gMsproalErrorLog.index %
				gMsproalErrorLog.capacity]);
	return result;
}

int MsproalGetMediaInformation(struct MsproalMediaInformation *information)
{
	struct MsproalHandle *handle = gMsproalHandle;
	struct MsproalSystemInformation *system;
	uint32_t mediaType;
	uint32_t sectorsPerBlock;
	uint32_t sectors;
	uint32_t blockSize;
	int blockGroups;
	int index;

	if (handle == 0 || (handle->mountFlags & 1) == 0)
		return -13;
	system = &handle->systemInformation;
	mediaType = handle->mediaType;
	if ((mediaType & 0x10) != 0) {
		blockSize = system->blockSize[1] | (system->blockSize[0] << 8);
		blockGroups = system->blockCount[0] >> 1;
		sectorsPerBlock = 2 * blockSize;
		information->userSectors = sectorsPerBlock *
			(496 * (blockGroups - 1) + 494);
		blockGroups >>= 1;
		if (blockGroups != 0) {
			index = 0;
			do {
				blockGroups >>= 1;
				++index;
			} while (blockGroups != 0);
		} else {
			index = 0;
		}
		if (blockSize == 16)
			++index;
		if (index > 5)
			return -13;
		information->totalSectors = gMsproalLegacySectorCounts[index];
	} else {
		uint32_t userBlockCount;
		uint32_t units;

		if ((mediaType & 0x20) != 0) {
			blockSize = system->blockSize[1] |
				(system->blockSize[0] << 8);
			userBlockCount = system->geometry.pro.userBlockCount[1] |
				(system->geometry.pro.userBlockCount[0] << 8);
			units = system->unitSize[0] >> 1;
			sectorsPerBlock = units * blockSize;
			sectors = units * (blockSize * userBlockCount);
			information->totalSectors = sectors;
			information->userSectors = sectors;
		} else {
			blockSize = system->xcBlockSize[3] |
				(system->xcBlockSize[2] << 8) |
				((uint32_t)(system->xcBlockSize[1] |
					(system->xcBlockSize[0] << 8)) << 16);
			userBlockCount = system->geometry.xcUserBlockCount[3] |
				(system->geometry.xcUserBlockCount[2] << 8) |
				((uint32_t)(system->geometry.xcUserBlockCount[1] |
					(system->geometry.xcUserBlockCount[0] << 8)) << 16);
			units = system->unitSize[0] >> 1;
			sectorsPerBlock = units * blockSize;
			sectors = units * (blockSize * userBlockCount);
			information->totalSectors = sectors;
			information->userSectors = sectors;
		}
	}
	information->sectorsPerBlock = sectorsPerBlock;
	information->mediaType = mediaType;
	information->writeProtected = handle->writeProtected;
	return 0;
}

int MsproalGetSystemInformation(struct MsproalSystemInformation *information)
{
	if (gMsproalHandle == 0 || (gMsproalHandle->mountFlags & 1) == 0)
		return -13;
	MsproalUserCopy(information, &gMsproalHandle->systemInformation,
		sizeof(*information));
	return 0;
}

int msproal_read(uint32_t sector, int count, const void *transfer)
{
	int result;
	int stopResult;

	if (gMsproalHandle == 0 || (gMsproalHandle->mountFlags & 1) == 0)
		return -13;
	result = gMsproalOperations->read(gMsproalHandle, sector, count,
		transfer);
	if (result == 0)
		return result;
	MsproalRecordError(gMsproalHandle, 11, result,
		&gMsproalErrorLog.index,
		&gMsproalErrorLog.records[gMsproalErrorLog.index %
			gMsproalErrorLog.capacity]);
	if (result == -12)
		return result;
	stopResult = gMsproalOperations->stop(gMsproalHandle);
	if (stopResult == 0)
		return result;
	MsproalRecordError(gMsproalHandle, 13, stopResult,
		&gMsproalErrorLog.index,
		&gMsproalErrorLog.records[gMsproalErrorLog.index %
			gMsproalErrorLog.capacity]);
	return stopResult;
}

int msproal_write(uint32_t sector, int count, const void *transfer)
{
	int result;
	int stopResult;

	if (gMsproalHandle == 0 || (gMsproalHandle->mountFlags & 1) == 0)
		return -13;
	if (gMsproalHandle->writeEnabled == 0)
		return -2;
	result = gMsproalOperations->write(gMsproalHandle, sector, count,
		transfer);
	if (result == 0)
		return result;
	MsproalRecordError(gMsproalHandle, 12, result,
		&gMsproalErrorLog.index,
		&gMsproalErrorLog.records[gMsproalErrorLog.index %
			gMsproalErrorLog.capacity]);
	if (result == -12)
		return result;
	stopResult = gMsproalOperations->stop(gMsproalHandle);
	if (stopResult == 0)
		return result;
	MsproalRecordError(gMsproalHandle, 13, stopResult,
		&gMsproalErrorLog.index,
		&gMsproalErrorLog.records[gMsproalErrorLog.index %
			gMsproalErrorLog.capacity]);
	return stopResult;
}

int MsproalFormat(int mode)
{
	int result;
	int stopResult;

	if (gMsproalHandle == 0 || (gMsproalHandle->mountFlags & 1) == 0)
		return -13;
	if (gMsproalHandle->writeEnabled == 0)
		return -2;
	result = gMsproalOperations->format(gMsproalHandle, mode);
	if (result == 0)
		return result;
	MsproalRecordError(gMsproalHandle, 5, result,
		&gMsproalErrorLog.index,
		&gMsproalErrorLog.records[gMsproalErrorLog.index %
			gMsproalErrorLog.capacity]);
	if (result == -12)
		return result;
	stopResult = gMsproalOperations->stop(gMsproalHandle);
	if (stopResult == 0)
		return result;
	MsproalRecordError(gMsproalHandle, 13, stopResult,
		&gMsproalErrorLog.index,
		&gMsproalErrorLog.records[gMsproalErrorLog.index %
			gMsproalErrorLog.capacity]);
	return stopResult;
}

int MsproalSleep(void)
{
	int result;

	if (gMsproalHandle == 0 || (gMsproalHandle->mountFlags & 1) == 0)
		return -13;
	result = gMsproalOperations->sleep(gMsproalHandle);
	if (result != 0)
		MsproalRecordError(gMsproalHandle, 8, result,
			&gMsproalErrorLog.index,
			&gMsproalErrorLog.records[gMsproalErrorLog.index %
				gMsproalErrorLog.capacity]);
	return result;
}

int msproal_control_power_supply(uint32_t value, uint32_t preserveMask)
{
	if (gMsproalHandle == 0)
		return -13;
	return MsproalUpdatePowerSupply(gMsproalHandle, value,
		preserveMask);
}

