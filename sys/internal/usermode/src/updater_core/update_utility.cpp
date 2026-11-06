/*
 * The update's utilities (Sony's update_utility.cpp): the console's information, the update
 * mode and the result the update leaves for the next boot (updatemode_result.dat in the working
 * directory), power control through the update service, and the console checks the update
 * makes before it starts.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <libkernel/libkernel.h>
#include <ss_mgr/ss_mgr.h>
#include <update_mgr/update_mgr.h>
#include <vshbridge/vshbridge.h>
#include <updater_core/updater_core.h>
#include "updater_log.h"
#include "partition_manager.h"
#include "update_utility.h"

#define UPDATER_FILE_ID		0xa64ec2ee

#define UPDATE_RESULT_FILE_NAME		"/updatemode_result.dat"
#define UPDATE_RESULT_PATH_SIZE		256
#define UPDATE_RESULT_FILE_MODE		0606

/* the updater's codes for the update service's errors, a block for each call */
#define UTILITY_ERROR_GET_SPKG_INFO	0x801804e0
#define UTILITY_ERROR_GET_UPDATE_MODE	0x80180550
#define UTILITY_ERROR_SET_UPDATE_MODE	0x80180553
#define UTILITY_ERROR_POWER_CONTROL_0	0x80180560
#define UTILITY_ERROR_POWER_CONTROL_1	0x80180567
#define UTILITY_ERROR_POWER_CONTROL_3	0x8018056f
#define UTILITY_ERROR_POWER_CONTROL_4	0x80180577

/* the console checks' errors */
#define UTILITY_ERROR_RELEASE_CHECK_MODE	0x80182802
#define UTILITY_ERROR_SYSTEM_VERSION		0x80182804
#define UTILITY_ERROR_DVT1			0x8018280a

/* the system versions the update installs over */
#define UTILITY_SYSTEM_VERSION_MIN	0x00996000

/* the DVT1 and manufacturing units' PS codes */
#define UTILITY_PRODUCT_CODE_DEVKIT	0x101
#define UTILITY_PRODUCT_SUB_CODE_DVT1	3
#define UTILITY_PRODUCT_SUB_CODE_MANUF	9

/*
 * The weak import's address, in a data word the loader leaves 0 where the system lacks it
 * (Sony's compiler made the word; SNC 3.57 tests the address in the code, so it is written
 * out here, as cui_view.c's are).
 */
SceUInt32 (*updateUtilityIsDevelopmentMode)(void) = vshSblSsIsDevelopmentMode;

UpdateUtility::UpdateUtility()
{
}

UpdateUtility::~UpdateUtility()
{
}

int UpdateUtility::GetDeviceInfo(SceUpdaterDeviceInfo *deviceInfo)
{
	SceSblUsSpkgInfo info;
	int ret;

	ret = sceSblUsGetSpkgInfo(SCE_SBL_US_SPKG_TYPE_SYSTEM, &info);
	if (ret != 0) {
		UPDATER_ERROR_FMT(59, "%x", ret);
		if (ret == SCE_SBL_US_ERROR_NOT_PERMITTED)
			ret = UTILITY_ERROR_GET_SPKG_INFO + 1;
		else if (ret == SCE_SBL_US_ERROR_INVALID_ARGUMENT)
			ret = UTILITY_ERROR_GET_SPKG_INFO + 2;
		else if (ret == SCE_SBL_US_ERROR_IO)
			ret = UTILITY_ERROR_GET_SPKG_INFO + 3;
		else if (ret == SCE_SBL_US_ERROR_TRY_AGAIN)
			ret = UTILITY_ERROR_GET_SPKG_INFO + 4;
		else if (ret == SCE_SBL_US_ERROR_NO_MEMORY)
			ret = UTILITY_ERROR_GET_SPKG_INFO + 5;
		return ret;
	}
	deviceInfo->m_version = info.version;
	deviceInfo->m_buildno = 0;
	deviceInfo->m_qaf = sceSblQafMgrIsAllowNonQAPup() == 0;
	deviceInfo->m_target = vshSblAimgrIsCEX() == 1 ? SCE_UPDATER_DEVICE_TYPE_CEX :
							  SCE_UPDATER_DEVICE_TYPE_DEVELOPMENT;
	return ret;
}

int UpdateUtility::GetUpdateModeState(void)
{
	return 0;
}

int UpdateUtility::GetUpdateMode(SceUInt8 *mode)
{
	int ret;

	ret = sceSblUsGetUpdateMode(mode);
	if (ret != 0) {
		UPDATER_ERROR_FMT(125, "%x", ret);
		if (ret == SCE_SBL_US_ERROR_NO_MEMORY)
			return UTILITY_ERROR_GET_UPDATE_MODE + 1;
		if (ret == SCE_SBL_US_ERROR_INVALID_ARGUMENT)
			return UTILITY_ERROR_GET_UPDATE_MODE + 2;
	}
	return ret;
}

/* a mode other than none first writes the working partition's files out */
int UpdateUtility::SetUpdateMode(SceUInt8 mode)
{
	int ret;

	if (mode != 0x10 && mode != 0x11 && mode != 0x12 && mode != 0x13 &&
	    mode != 0x20 && mode != 0x21 && mode != 0x22 && mode != 0x23 &&
	    mode != 0x30 && mode != 0x31 && mode != 0x32 && mode != 0x33 &&
	    mode != 0x40 && mode != 0x41 && mode != 0x42 && mode != 0x43 &&
	    mode != SCE_UPDATER_UPDATE_MODE_NONE) {
		UPDATER_ERROR_FMT(169, "%x", mode);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	if (mode != SCE_UPDATER_UPDATE_MODE_NONE) {
		PartitionManager partition;

		sceIoSync("ud0:", 0);
		sceIoSync(partition.GetPupPath(), 0);
		sceIoSync(partition.GetUpdaterPath(), 0);
	}
	ret = sceSblUsSetUpdateMode(mode);
	if (ret != 0) {
		UPDATER_ERROR_FMT(186, "%x : %x", mode, ret);
		if (ret == SCE_SBL_US_ERROR_NO_MEMORY)
			return UTILITY_ERROR_SET_UPDATE_MODE + 1;
	}
	return ret;
}

void UpdateUtility::getResultFilePath(char *path, SceSize size)
{
	PartitionManager partition;

	sceClibStrncpy(path, partition.GetWorkingDirectory(), size);
	sceClibStrncat(path, UPDATE_RESULT_FILE_NAME, size);
}

/* the write's error is returned with the file left open (Sony's code) */
int UpdateUtility::UpdateModeSetLastError(int error)
{
	char path[UPDATE_RESULT_PATH_SIZE];
	SceUID fd;
	int ret;

	getResultFilePath(path, sizeof(path));
	fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, UPDATE_RESULT_FILE_MODE);
	if (fd < 0) {
		UPDATER_ERROR_FMT(222, "%s %o : %x", path, UPDATE_RESULT_FILE_MODE, fd);
		return SCE_UPDATER_ERROR_RESULT_OPEN;
	}
	if (sceIoWrite(fd, &error, sizeof(error)) < 0)
		return SCE_UPDATER_ERROR_RESULT_WRITE;
	ret = sceIoClose(fd);
	if (ret < 0) {
		UPDATER_ERROR_FMT(235, "%x", ret);
		return SCE_UPDATER_ERROR_RESULT_CLOSE;
	}
	return 0;
}

int UpdateUtility::UpdateModeGetLastError(int *error)
{
	char path[UPDATE_RESULT_PATH_SIZE];
	SceSSize size;
	SceUID fd;
	int ret;

	getResultFilePath(path, sizeof(path));
	fd = sceIoOpen(path, SCE_O_RDONLY, 0);
	if (fd < 0) {
		UPDATER_ERROR_FMT(266, "%s %o : %x", path, 0, fd);
		return SCE_UPDATER_ERROR_RESULT_OPEN;
	}
	size = sceIoRead(fd, error, sizeof(*error));
	if (size < 0) {
		UPDATER_ERROR_FMT(273, "%x", size);
		sceIoClose(fd);
		return SCE_UPDATER_ERROR_RESULT_READ;
	}
	ret = sceIoClose(fd);
	if (ret < 0) {
		UPDATER_ERROR_FMT(281, "%x", ret);
		return SCE_UPDATER_ERROR_RESULT_CLOSE;
	}
	return 0;
}

int UpdateUtility::convertErrorCodePowerControl(int operation, int error)
{
	switch (operation) {
	case 0:
		if (error == SCE_SBL_US_ERROR_INVALID_ARGUMENT)
			return UTILITY_ERROR_POWER_CONTROL_0 + 1;
		return error;
	case 1:
		if (error == SCE_SBL_US_ERROR_INVALID_ARGUMENT)
			return UTILITY_ERROR_POWER_CONTROL_1 + 1;
		return error;
	case 3:
		if (error == SCE_SBL_US_ERROR_INVALID_ARGUMENT)
			return UTILITY_ERROR_POWER_CONTROL_3 + 1;
		return error;
	case 4:
		if (error == SCE_SBL_US_ERROR_INVALID_ARGUMENT)
			return UTILITY_ERROR_POWER_CONTROL_4 + 1;
		return error;
	default:
		return 0;
	}
}

/* the updater's operations 0, 1, 3 and 4 are the update service's 0 to 3; 2 does nothing */
int UpdateUtility::PowerControl(int operation, int flags)
{
	int usOperation;
	int ret;

	if (operation == 2)
		return 0;
	switch (operation) {
	case 0:
		usOperation = 0;
		break;
	case 1:
		usOperation = 1;
		break;
	case 3:
		usOperation = 2;
		break;
	case 4:
		usOperation = 3;
		break;
	default:
		UPDATER_ERROR(341);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	ret = sceSblUsPowerControl(usOperation, flags);
	if (ret != 0) {
		UPDATER_ERROR_FMT(348, "%x %x : %x", usOperation, flags, ret);
		return convertErrorCodePowerControl(operation, ret);
	}
	return ret;
}

/*
 * Whether the update may run on this console: a tool or test unit must not be in release check
 * mode, a DVT1 unit is refused, the installed system must be 0.996 or later, and a
 * manufacturing unit must not hold the manufacturing image.
 */
int UpdateUtility::CheckDeviceStatus(void)
{
	SceSblUsSpkgInfo info;
	ScePsCode pscode;
	int isTool;
	int isTest;
	int mode;
	int ret;

	ret = vshSblAimgrGetPscode(&pscode);
	if (ret < 0) {
		UPDATER_ERROR_FMT(569, "%x", ret);
		return ret;
	}
	isTool = vshSblAimgrIsTool() == 1;
	isTest = vshSblAimgrIsTest() == 1;
	if (isTool || isTest) {
		if (updateUtilityIsDevelopmentMode == NULL) {
			sceClibPrintf("[INFO]release check mode is not supported.\n");
		} else {
			mode = updateUtilityIsDevelopmentMode();
			if (mode < 0) {
				sceClibPrintf("[ERROR]release mode check failed: %08x\n", mode);
			} else {
				sceClibPrintf("[INFO]release check mode val: %x\n", mode);
				if (mode == 0)
					return UTILITY_ERROR_RELEASE_CHECK_MODE;
			}
		}
	} else {
		sceClibPrintf("[INFO]skip release check mode check.\n");
	}
	if (pscode.product_code == UTILITY_PRODUCT_CODE_DEVKIT &&
	    pscode.product_sub_code == UTILITY_PRODUCT_SUB_CODE_DVT1) {
		sceClibPrintf("[ERROR]DVT1 is not supported.\n");
		return UTILITY_ERROR_DVT1;
	}
	info.version = 0;
	ret = sceSblUsGetSpkgInfo(SCE_SBL_US_SPKG_TYPE_SYSTEM, &info);
	if (ret != 0)
		return ret;
	if (info.version != 0 && info.version < UTILITY_SYSTEM_VERSION_MIN) {
		sceClibPrintf("[ERROR]current version is lower than 0996: %08x.\n", info.version);
		return UTILITY_ERROR_SYSTEM_VERSION;
	}
	if (pscode.product_code != UTILITY_PRODUCT_CODE_DEVKIT ||
	    pscode.product_sub_code != UTILITY_PRODUCT_SUB_CODE_MANUF)
		return 0;
	info.version = 0;
	ret = sceSblUsGetSpkgInfo(SCE_SBL_US_SPKG_TYPE_MANUFACTURING, &info);
	if (ret != 0) {
		sceClibPrintf("[ERROR]device stores manufacturing image: %08x.\n", ret);
		return UTILITY_ERROR_SYSTEM_VERSION;
	}
	if (info.version == 0) {
		sceClibPrintf("[ERROR]device stores manufacturing image: ver: %08x.\n",
			      info.version);
		return UTILITY_ERROR_SYSTEM_VERSION;
	}
	return 0;
}
