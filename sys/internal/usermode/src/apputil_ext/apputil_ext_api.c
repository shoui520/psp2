/*
 * SceAppUtilExt's system data and save data calls. The PlayReady and Marlin IPTV calls read and
 * write their system data files through SceAppMgr; the PS Now calls save and remove the caller's
 * save data like sceAppUtilSaveDataDataSave/Remove, always in "savedata0:", and tell SceAppUtil
 * when a slot changed.
 */
#include <stddef.h>
#include <scetypes.h>
#include <apputil.h>
#include <rtc.h>
#include <sceerror.h>
#include <kernel/libkernel.h>
#include <appmgr/appmgr.h>
#include <appmgr/appmgr_savedata.h>
#include <apputil/apputil_internal.h>
#include "apputil_ext_internal.h"

int SceAppUtilExtMarlinIptv_DFFF8D1F(const void *buffer, SceSize size)
{
	int ret;

	ret = checkStack(APPUTIL_EXT_STACK_SIZE);
	if (ret < 0)
		return ret;
	if (buffer == NULL) {
		paramError(510);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (size != APPUTIL_EXT_MARLIN_IPTV_SIZE) {
		paramError(511);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	ret = sceAppMgrSetSystemDataFile(APPUTIL_EXT_MARLIN_IPTV, buffer, size);
	switch (ret) {
	case SCE_OK:
		break;
	case APPMGR_ERROR_NO_PERMISSION:
		ret = SCE_APPUTIL_ERROR_NO_PERMISSION;
		break;
	}
	return ret;
}

int SceAppUtilExtMarlinIptv_B86104DB(void *buffer, SceSize size)
{
	int ret;

	ret = checkStack(APPUTIL_EXT_STACK_SIZE);
	if (ret < 0)
		return ret;
	if (buffer == NULL) {
		paramError(510);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (size < APPUTIL_EXT_MARLIN_IPTV_SIZE) {
		paramError(511);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	ret = sceAppMgrGetSystemDataFile(APPUTIL_EXT_MARLIN_IPTV, buffer, size);
	if (ret < 0) {
		switch (ret) {
		case APPMGR_ERROR_NO_PERMISSION:
			ret = SCE_APPUTIL_ERROR_NO_PERMISSION;
			break;
		case APPMGR_ERROR_MARLIN_IPTV_DIRTY:
			ret = APPUTIL_EXT_ERROR_MARLIN_IPTV_DIRTY;
			break;
		}
	}
	return ret;
}

int SceAppUtilExtPlayReady_A9C2098C(const SceUInt64 *information)
{
	int ret;

	ret = checkStack(APPUTIL_EXT_STACK_SIZE);
	if (ret < 0)
		return ret;
	if (information == NULL) {
		paramError(500);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	ret = sceAppMgrSetSystemDataFilePlayReady(APPUTIL_EXT_PLAYREADY_INFORMATION, information,
						  NULL, 0);
	switch (ret) {
	case SCE_OK:
		break;
	case APPMGR_ERROR_NO_PERMISSION:
		ret = SCE_APPUTIL_ERROR_NO_PERMISSION;
		break;
	}
	return ret;
}

int SceAppUtilExtPlayReady_E915A695(SceUInt64 *information)
{
	int ret;

	ret = checkStack(APPUTIL_EXT_STACK_SIZE);
	if (ret < 0)
		return ret;
	if (information == NULL)
		return SCE_APPUTIL_ERROR_PARAMETER;
	ret = sceAppMgrGetSystemDataFilePlayReady(APPUTIL_EXT_PLAYREADY_INFORMATION, information,
						  NULL, 0);
	if (ret != SCE_OK) {
		if (ret == APPMGR_ERROR_PLAYREADY_NO_DATA)
			ret = APPUTIL_EXT_ERROR_PLAYREADY_NO_DATA;
		else if (ret == APPMGR_ERROR_NO_PERMISSION)
			ret = SCE_APPUTIL_ERROR_NO_PERMISSION;
	}
	return ret;
}

int SceAppUtilExtPlayReady_4C91CDB1(const void *buffer, SceSize size)
{
	int ret;

	ret = checkStack(APPUTIL_EXT_STACK_SIZE);
	if (ret < 0)
		return ret;
	if (buffer == NULL) {
		paramError(501);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (size > APPUTIL_EXT_PLAYREADY_DATA_MAXSIZE) {
		paramError(502);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	ret = sceAppMgrSetSystemDataFilePlayReady(APPUTIL_EXT_PLAYREADY_DATA, NULL, buffer, size);
	switch (ret) {
	case SCE_OK:
		break;
	case APPMGR_ERROR_NO_PERMISSION:
		ret = SCE_APPUTIL_ERROR_NO_PERMISSION;
		break;
	}
	return ret;
}

int SceAppUtilExtPlayReady_9B52C63D(void *buffer, SceSize size)
{
	int ret;

	ret = checkStack(APPUTIL_EXT_STACK_SIZE);
	if (ret < 0)
		return ret;
	if (buffer == NULL) {
		paramError(501);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (size < APPUTIL_EXT_PLAYREADY_DATA_MAXSIZE) {
		paramError(502);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	ret = sceAppMgrGetSystemDataFilePlayReady(APPUTIL_EXT_PLAYREADY_DATA, NULL, buffer, size);
	if (ret < 0) {
		switch (ret) {
		case APPMGR_ERROR_NO_PERMISSION:
			ret = SCE_APPUTIL_ERROR_NO_PERMISSION;
			break;
		case APPMGR_ERROR_PLAYREADY_NO_DATA:
			ret = APPUTIL_EXT_ERROR_PLAYREADY_NO_DATA;
			break;
		}
	}
	return ret;
}

int SceAppUtilExtPsNow_2CF451BE(const SceAppUtilSaveDataDataSlot *slot,
				const SceAppUtilSaveDataDataSaveItem *data, SceUInt32 dataNum,
				const SceAppUtilMountPoint *mountPoint, SceSize *requiredSizeKiB)
{
	SceAppMgrSaveDataData operation;
	SceAppMgrSaveDataSlotParam slotParam;
	const SceAppUtilSaveDataSlotParam *param;
	int ret;

	ret = checkStack(APPUTIL_EXT_STACK_SIZE);
	if (ret < 0)
		return ret;
	if (SceAppUtil_E8CADCA9(slot, data, dataNum, mountPoint, requiredSizeKiB) < 0)
		return SCE_APPUTIL_ERROR_PARAMETER;

	sceClibMemset(&slotParam, 0, sizeof(slotParam));
	sceClibMemset(&operation, 0, sizeof(operation));
	operation.size = sizeof(operation);
	if (slot != NULL) {
		param = slot->slotParam;
		operation.slotId = slot->id;
		if (param != NULL) {
			SceDateTime now;

			slotParam.status = param->status;
			slotParam.userParam = param->userParam;
			slotParam.sizeKiB = param->sizeKiB;
			slotParam.modifiedTime = param->modifiedTime;
			sceClibStrncpy(slotParam.title, (const char *)param->title,
				       sizeof(slotParam.title) - 1);
			sceClibStrncpy(slotParam.subTitle, (const char *)param->subTitle,
				       sizeof(slotParam.subTitle) - 1);
			sceClibStrncpy(slotParam.detail, (const char *)param->detail,
				       sizeof(slotParam.detail) - 1);
			sceClibStrncpy(slotParam.iconPath, (const char *)param->iconPath,
				       sizeof(slotParam.iconPath) - 1);
			if (sceRtcGetCurrentClockLocalTime(&now) == SCE_OK)
				slotParam.modifiedTime = now;
			operation.slotParam = &slotParam;
		} else {
			operation.slotParam = NULL;
		}
	} else {
		operation.slotId = SCE_APPUTIL_SAVEDATA_SLOT_ID_INVALID;
		operation.slotParam = NULL;
	}
	operation.items = data;
	operation.itemNum = dataNum;
	sceClibStrncpy((char *)operation.mountPoint.data, "savedata0:",
		       sizeof(operation.mountPoint.data) - 1);

	ret = sceAppMgrSaveDataDataSave2(&operation);
	if (ret != SCE_OK) {
		*requiredSizeKiB = operation.requiredSizeKiB;
		switch (ret) {
		case APPMGR_ERROR_SAVEDATA_NO_SPACE_QUOTA:
			return SCE_APPUTIL_ERROR_SAVEDATA_NO_SPACE_QUOTA;
		case APPMGR_ERROR_SAVEDATA_NO_SPACE_FS:
			return SCE_APPUTIL_ERROR_SAVEDATA_NO_SPACE_FS;
		case APPMGR_ERROR_SAVEDATA_PARAMETER:
			paramError(520);
			return SCE_APPUTIL_ERROR_PARAMETER;
		case SCE_ERROR_ERRNO_ENODEV:
			return SCE_APPUTIL_ERROR_NOT_MOUNTED;
		}
		return ret;
	}
	*requiredSizeKiB = 0;
	if (slot != NULL)
		SceAppUtil_F93C691C(SCE_APPUTIL_INTERNAL_EVENT_SAVEDATA_SLOT, slot->id);
	return SCE_OK;
}

int SceAppUtilExtPsNow_2BF9A33D(const SceAppUtilSaveDataDataSlot *slot,
				const SceAppUtilSaveDataDataRemoveItem *data, SceUInt32 dataNum,
				const SceAppUtilMountPoint *mountPoint)
{
	SceAppMgrSaveDataDataDelete operation;
	SceAppMgrSaveDataSlotParam slotParam;
	const SceAppUtilSaveDataSlotParam *param;
	int ret;

	ret = checkStack(APPUTIL_EXT_STACK_SIZE);
	if (ret < 0)
		return ret;
	if (SceAppUtil_1CFFE892(slot, data, dataNum, mountPoint) < 0)
		return SCE_APPUTIL_ERROR_PARAMETER;

	sceClibMemset(&slotParam, 0, sizeof(slotParam));
	sceClibMemset(&operation, 0, sizeof(operation));
	operation.size = sizeof(operation);
	if (slot != NULL) {
		param = slot->slotParam;
		operation.slotId = slot->id;
		if (param != NULL) {
			SceDateTime now;

			slotParam.status = param->status;
			slotParam.userParam = param->userParam;
			slotParam.sizeKiB = param->sizeKiB;
			slotParam.modifiedTime = param->modifiedTime;
			sceClibStrncpy(slotParam.title, (const char *)param->title,
				       sizeof(slotParam.title) - 1);
			sceClibStrncpy(slotParam.subTitle, (const char *)param->subTitle,
				       sizeof(slotParam.subTitle) - 1);
			sceClibStrncpy(slotParam.detail, (const char *)param->detail,
				       sizeof(slotParam.detail) - 1);
			sceClibStrncpy(slotParam.iconPath, (const char *)param->iconPath,
				       sizeof(slotParam.iconPath) - 1);
			if (sceRtcGetCurrentClockLocalTime(&now) == SCE_OK)
				slotParam.modifiedTime = now;
			operation.slotParam = &slotParam;
		} else {
			operation.slotParam = NULL;
		}
	} else {
		operation.slotId = SCE_APPUTIL_SAVEDATA_SLOT_ID_INVALID;
		operation.slotParam = NULL;
	}
	operation.items = data;
	operation.itemNum = dataNum;
	sceClibStrncpy((char *)operation.mountPoint.data, "savedata0:",
		       sizeof(operation.mountPoint.data) - 1);

	ret = sceAppMgrSaveDataDataRemove2(&operation);
	if (ret != SCE_OK) {
		switch (ret) {
		case APPMGR_ERROR_SAVEDATA_PARAMETER:
			paramError(520);
			return SCE_APPUTIL_ERROR_PARAMETER;
		case SCE_ERROR_ERRNO_ENODEV:
			return SCE_APPUTIL_ERROR_NOT_MOUNTED;
		}
		return ret;
	}
	if (slot != NULL)
		SceAppUtil_F93C691C(SCE_APPUTIL_INTERNAL_EVENT_SAVEDATA_SLOT, slot->id);
	return SCE_OK;
}
