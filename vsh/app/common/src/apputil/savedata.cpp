/*
 * SceAppUtil's application calls that act: launch requests, the save data drives, slots and data,
 * and PSP save data. The checks of their arguments are savedata_check.cpp's.
 */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <kernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <kernel/iofilemgr.h>
#include <libkernel/libkernel.h>
#include <rtc.h>
#include <apputil.h>
#include <apputil/apputil_psp.h>
#include <appmgr/appmgr.h>
#include <appmgr/appmgr_savedata.h>
#include <shell_svc/shell_util.h>
#include "apputil_private.h"

/* the PSP save data root: how many callers have it mounted, and where */
int g_pspSaveDataMountCount;
char g_pspSaveDataMountPoint[SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE];

/* a launch request through SceShellSvc (SceAppUtilLaunchApp_D2338D13) */
int launchAppRequest(const SceShellUtilLaunchAppParam *param)
{
	SceShellUtilLaunchAppParam request;
	int ret;

	if (param == NULL) {
		paramError(280);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param->cmd == NULL) {
		paramError(281);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	ret = getFreeStackSize();
	if (ret < APPUTIL_LAUNCH_STACK_SIZE)
		return SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE;
	sceClibMemsetChk(&request, 0, sizeof(request), sizeof(request));
	request.cmd = param->cmd;
	ret = sceShellUtilRequestLaunchApp(&request);
	if (ret != 0) {
		if (ret == SHELL_UTIL_ERROR_LAUNCH_APP_PARAMETER) {
			paramError(282);
			ret = SCE_APPUTIL_ERROR_PARAMETER;
		} else if (ret == SHELL_UTIL_ERROR_LAUNCH_APP_REJECTED) {
			ret = APPUTIL_ERROR_LAUNCH_APP_REJECTED;
		}
	}
	return ret;
}

/* mounts another title's save data at savedata1: */
int saveDataMount(const SceAppUtilTitleId *titleId, int reserved, const SceAppUtilPassCode *passCode)
{
	char mountPoint[SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE];
	AppmgrAddMountParam param;
	const char *drive;
	int ret;

	(void)reserved;
	sceClibMemset(&param, 0, sizeof(param));
	sceClibStrncpy(mountPoint, SCE_APPUTIL_SAVEDATA_DRIVE_NAME_ADD, sizeof(mountPoint));
	drive = mountPoint;
	if (titleId == NULL) {
		paramError(26);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (drive == NULL) {
		paramError(33);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (passCode == NULL) {
		paramError(20);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibMemcpy_safe(param.titleId, titleId, sizeof(param.titleId));
	sceClibMemcpy_safe(&param.mountPoint, drive, sizeof(param.mountPoint));
	sceClibMemcpy_safe(param.passcode, passCode, sizeof(param.passcode));
	if (checkTitleId(param.titleId) == 0 ||
	    checkMountPoint(param.mountPoint.drive_id, MOUNT_POINT_ADD) == 0 ||
	    checkPassCode((const SceUChar8 *)param.passcode) == 0)
		return SCE_APPUTIL_ERROR_PARAMETER;
	ret = sceAppMgrSaveDataAddMount(&param);
	switch (ret) {
	case APPMGR_ERROR_INVALID_TITLE_ID:
		paramError(29);
		ret = SCE_APPUTIL_ERROR_PARAMETER;
		break;
	case APPMGR_ERROR_ALREADY_MOUNTED:
		ret = SCE_APPUTIL_ERROR_BUSY;
		break;
	case APPMGR_ERROR_PASSCODE_MISMATCH:
		ret = SCE_APPUTIL_ERROR_PASSCODE_MISMATCH;
		break;
	}
	return ret;
}

int saveDataUmount(void)
{
	char mountPoint[SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE];
	char drive[SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE];
	const char *name;
	int ret;

	sceClibStrncpy(drive, SCE_APPUTIL_SAVEDATA_DRIVE_NAME_ADD, sizeof(drive));
	name = drive;
	if (name == NULL) {
		paramError(33);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibMemcpy_safe(mountPoint, name, sizeof(mountPoint));
	if (checkMountPoint(mountPoint, MOUNT_POINT_ADD) == 0)
		return SCE_APPUTIL_ERROR_PARAMETER;
	ret = sceAppMgrUmount(mountPoint);
	switch (ret) {
	case APPMGR_ERROR_NOT_MOUNTED:
		ret = SCE_APPUTIL_ERROR_NOT_MOUNTED;
		break;
	case SCE_ERROR_ERRNO_EBUSY:
		ret = SCE_APPUTIL_ERROR_BUSY;
		break;
	}
	return ret;
}

int saveDataSlotCreate(SceAppUtilSaveDataSlotId slotId, const SceAppUtilSaveDataSlotParam *param,
		       const SceAppUtilMountPoint *mountPoint)
{
	SceAppMgrSaveDataSlot slot;
	int ret;

	if (checkSlotId(slotId) == 0 || checkSlotParam(param) == 0 ||
	    checkMountPoint((const char *)mountPoint, MOUNT_POINT_OWN) == 0)
		return SCE_APPUTIL_ERROR_PARAMETER;
	sceClibMemset(&slot, 0, sizeof(slot));
	slot.size = sizeof(slot);
	slot.slotId = slotId;
	copySlotParamIn(param, &slot.slotParam);
	getModifiedTime(&slot.slotParam.modifiedTime);
	sceClibStrncpy((char *)slot.mountPoint.data, SCE_APPUTIL_SAVEDATA_DRIVE_NAME,
		       sizeof(slot.mountPoint) - 1);
	ret = sceAppMgrSaveDataSlotCreate(&slot);
	if (ret == 0) {
		if (slot.slotParam.status == SCE_APPUTIL_SAVEDATA_SLOT_STATUS_AVAILABLE)
			SceAppUtil_F93C691C(SCE_APPUTIL_INTERNAL_EVENT_SAVEDATA_SLOT, slotId);
		return 0;
	}
	switch (ret) {
	case APPMGR_ERROR_SAVEDATA_SLOT_EXISTS:
		ret = SCE_APPUTIL_ERROR_SAVEDATA_SLOT_EXISTS;
		break;
	case APPMGR_ERROR_SAVEDATA_SLOT_FILE:
		ret = APPUTIL_ERROR_SAVEDATA_SLOT_FILE;
		break;
	case SCE_ERROR_ERRNO_ENODEV:
		ret = SCE_APPUTIL_ERROR_NOT_MOUNTED;
		break;
	}
	return ret;
}

int saveDataSlotDelete(SceAppUtilSaveDataSlotId slotId, const SceAppUtilMountPoint *mountPoint)
{
	SceAppMgrSaveDataSlotDelete slot;
	int ret;

	if (checkSlotId(slotId) == 0 || checkMountPoint((const char *)mountPoint, MOUNT_POINT_OWN) == 0)
		return SCE_APPUTIL_ERROR_PARAMETER;
	sceClibMemset(&slot, 0, sizeof(slot));
	slot.size = sizeof(slot);
	slot.slotId = slotId;
	sceClibStrncpy((char *)slot.mountPoint.data, SCE_APPUTIL_SAVEDATA_DRIVE_NAME,
		       sizeof(slot.mountPoint) - 1);
	ret = sceAppMgrSaveDataSlotDelete(&slot);
	if (ret == 0) {
		SceAppUtil_F93C691C(SCE_APPUTIL_INTERNAL_EVENT_SAVEDATA_SLOT, slotId);
		return 0;
	}
	switch (ret) {
	case APPMGR_ERROR_SAVEDATA_SLOT_NOT_FOUND:
		ret = SCE_APPUTIL_ERROR_SAVEDATA_SLOT_NOT_FOUND;
		break;
	case APPMGR_ERROR_SAVEDATA_SLOT_FILE:
		ret = APPUTIL_ERROR_SAVEDATA_SLOT_FILE;
		break;
	case SCE_ERROR_ERRNO_ENODEV:
		ret = SCE_APPUTIL_ERROR_NOT_MOUNTED;
		break;
	}
	return ret;
}

/*
 * The search's sort: whether the slot already at a place in the list (a) moves down for the
 * slot being inserted (b). Ties keep the lower slot ID first.
 */
int compareSlot(SceAppUtilSaveDataSlotSortType order, SceAppUtilSaveDataSlotSortKey key,
		const SceAppUtilSaveDataSlot *a, const SceDateTime *aTime, const SceSize *aSize,
		const SceAppUtilSaveDataSlot *b, const SceDateTime *bTime, const SceSize *bSize)
{
	SceRtcTick aTick;
	SceRtcTick bTick;
	int after = 0;
	int cmp = 0;

	if (key == SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_MODIFIED_TIME) {
		sceRtcGetTick(aTime, &aTick);
		sceRtcGetTick(bTime, &bTick);
		cmp = sceRtcCompareTick(&aTick, &bTick);
	}
	if (order == SCE_APPUTIL_SAVEDATA_SLOT_SORT_TYPE_DESCENT) {
		switch (key) {
		case SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_USER_PARAM:
			if (a->userParam < b->userParam)
				after = 1;
			else if (a->userParam == b->userParam && a->id > b->id)
				after = 1;
			break;
		case SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_SIZE_KIB:
			if (*aSize < *bSize)
				after = 1;
			else if (*aSize == *bSize && a->id > b->id)
				after = 1;
			break;
		case SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_MODIFIED_TIME:
			if (cmp < 0)
				after = 1;
			else if (cmp == 0 && a->id > b->id)
				after = 1;
			break;
		case SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_SLOT_ID:
		default:
			if (a->id < b->id)
				after = 1;
			break;
		}
	} else {
		switch (key) {
		case SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_USER_PARAM:
			if (a->userParam > b->userParam)
				after = 1;
			else if (a->userParam == b->userParam && a->id > b->id)
				after = 1;
			break;
		case SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_SIZE_KIB:
			if (*aSize > *bSize)
				after = 1;
			else if (*aSize == *bSize && a->id > b->id)
				after = 1;
			break;
		case SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_MODIFIED_TIME:
			if (cmp > 0)
				after = 1;
			else if (cmp == 0 && a->id > b->id)
				after = 1;
			break;
		case SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_SLOT_ID:
		default:
			if (a->id > b->id)
				after = 1;
			break;
		}
	}
	return after;
}

/*
 * Lists the slots of a search: through the slot file's occupancy table where SceAppMgr opens it,
 * else slot by slot. The work buffer holds the list, sorted by insertion, and after it the sort
 * key of each entry (sizes or modified times).
 */
int saveDataSlotSearch(SceAppUtilWorkBuffer *workBuf, const SceAppUtilSaveDataSlotSearchCond *cond,
		       SceAppUtilSlotSearchResult *result, const SceAppUtilMountPoint *mountPoint)
{
	SceAppUtilSaveDataSlot entry;
	char closeMountPoint[SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE];
	char openMountPoint[SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE];
	AppmgrSaveDataSlotFileInfo info;
	SceAppMgrSaveDataSlotParam param;
	SceAppMgrSaveDataSlot slot;
	SceAppUtilSaveDataSlot *list;
	SceAppUtilSaveDataSlot *end;
	SceAppUtilSaveDataSlot *p;
	SceDateTime *time;
	SceSize *size;
	SceUInt32 slotId;
	SceUInt32 last;
	SceUInt32 hitNum;
	SceUInt32 index;
	bool opened;
	int ret;

	if (checkSearch(workBuf, cond, result) == 0 ||
	    checkMountPoint((const char *)mountPoint, MOUNT_POINT_ANY) == 0)
		return SCE_APPUTIL_ERROR_PARAMETER;
	last = cond->from + cond->range;
	end = NULL;
	if (workBuf != NULL) {
		list = (SceAppUtilSaveDataSlot *)workBuf->buf;
		end = list + cond->range;
		for (p = list; p != end; p++) {
			p->id = SCE_APPUTIL_SAVEDATA_SLOT_ID_INVALID;
			p->status = 0;
			p->userParam = 0;
		}
	}
	sceClibMemset(&info, 0, sizeof(info));
	sceClibStrncpy(openMountPoint,
		       mountPoint != NULL ? (const char *)mountPoint : SCE_APPUTIL_SAVEDATA_DRIVE_NAME,
		       sizeof(openMountPoint) - 1);
	ret = sceAppMgrSaveDataSlotFileOpen(openMountPoint, &info);
	opened = ret == 0;
	size = NULL;
	time = NULL;
	hitNum = 0;
	for (slotId = cond->from; slotId < last; slotId++) {
		if (opened) {
			if (!((info.occupied[slotId] == 1 && cond->type == SCE_APPUTIL_SAVEDATA_SLOT_SEARCH_TYPE_EXIST_SLOT) ||
			      (info.occupied[slotId] == 0 && cond->type == SCE_APPUTIL_SAVEDATA_SLOT_SEARCH_TYPE_EMPTY_SLOT)))
				continue;
			sceClibMemset(&param, 0, sizeof(param));
			ret = sceAppMgrSaveDataSlotFileGetParam(slotId, &param);
			if (ret != 0 && ret != APPMGR_ERROR_SAVEDATA_SLOT_NOT_FOUND)
				continue;
			sceClibMemset(&slot, 0, sizeof(slot));
			slot.size = sizeof(slot);
			slot.slotId = slotId;
			sceClibMemcpy_safe(&slot.slotParam, &param, sizeof(SceAppUtilSaveDataSlotParam));
		} else {
			sceClibMemset(&slot, 0, sizeof(slot));
			sceClibStrncpy((char *)slot.mountPoint.data,
				       mountPoint != NULL ? (const char *)mountPoint : SCE_APPUTIL_SAVEDATA_DRIVE_NAME,
				       sizeof(slot.mountPoint) - 1);
			slot.size = sizeof(slot);
			slot.slotId = slotId;
			ret = sceAppMgrSaveDataSlotGetParam(&slot);
			if (ret != 0 && ret != APPMGR_ERROR_SAVEDATA_SLOT_NOT_FOUND) {
				switch (ret) {
				case SCE_ERROR_ERRNO_ENODEV:
					return SCE_APPUTIL_ERROR_NOT_MOUNTED;
				case APPMGR_ERROR_SAVEDATA_SLOT_FILE:
					return APPUTIL_ERROR_SAVEDATA_SLOT_FILE;
				}
				return ret;
			}
			if (!((ret == 0 && cond->type == SCE_APPUTIL_SAVEDATA_SLOT_SEARCH_TYPE_EXIST_SLOT) ||
			      (ret == APPMGR_ERROR_SAVEDATA_SLOT_NOT_FOUND &&
			       cond->type == SCE_APPUTIL_SAVEDATA_SLOT_SEARCH_TYPE_EMPTY_SLOT)))
				continue;
		}
		hitNum++;
		if (workBuf == NULL)
			continue;
		sceClibMemset(&entry, 0, sizeof(entry));
		entry.id = slotId;
		entry.status = slot.slotParam.status;
		entry.userParam = slot.slotParam.userParam;
		index = (hitNum > cond->range ? cond->range : hitNum) - 1;
		p = (SceAppUtilSaveDataSlot *)workBuf->buf + index;
		if (cond->key == SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_SIZE_KIB)
			size = (SceSize *)end + index;
		else if (cond->key == SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_MODIFIED_TIME)
			time = (SceDateTime *)end + index;
		while (p >= (SceAppUtilSaveDataSlot *)workBuf->buf) {
			if (p->id != SCE_APPUTIL_SAVEDATA_SLOT_ID_INVALID &&
			    compareSlot(cond->order, cond->key, p, time, size, &entry,
					&slot.slotParam.modifiedTime, &slot.slotParam.sizeKiB) == 0)
				break;
			if (p + 1 < end && p->id != SCE_APPUTIL_SAVEDATA_SLOT_ID_INVALID) {
				sceClibMemcpy_safe(p + 1, p, sizeof(*p));
				if (time != NULL)
					sceClibMemcpy_safe(time + 1, time, sizeof(*time));
				if (size != NULL)
					sceClibMemcpy_safe(size + 1, size, sizeof(*size));
			}
			sceClibMemcpy_safe(p, &entry, sizeof(*p));
			if (time != NULL)
				sceClibMemcpy_safe(time, &slot.slotParam.modifiedTime, sizeof(*time));
			if (size != NULL)
				sceClibMemcpy_safe(size, &slot.slotParam.sizeKiB, sizeof(*size));
			p--;
			if (time != NULL)
				time--;
			if (size != NULL)
				size--;
		}
	}
	if (opened) {
		sceClibStrncpy(closeMountPoint,
			       mountPoint != NULL ? (const char *)mountPoint : SCE_APPUTIL_SAVEDATA_DRIVE_NAME,
			       sizeof(closeMountPoint) - 1);
		ret = sceAppMgrSaveDataSlotFileClose(closeMountPoint);
	}
	if (result != NULL) {
		if (workBuf != NULL) {
			result->slotList = (SceAppUtilSaveDataSlot *)workBuf->buf;
			result->hitNum = hitNum > cond->range ? cond->range : hitNum;
		} else {
			result->hitNum = hitNum;
			result->slotList = NULL;
		}
	}
	return ret;
}

int saveDataSlotSetParam(SceAppUtilSaveDataSlotId slotId, const SceAppUtilSaveDataSlotParam *param,
			 const SceAppUtilMountPoint *mountPoint)
{
	SceAppMgrSaveDataSlot slot;
	int ret;

	if (checkSlotId(slotId) == 0 || checkSlotParam(param) == 0 ||
	    checkMountPoint((const char *)mountPoint, MOUNT_POINT_OWN) == 0)
		return SCE_APPUTIL_ERROR_PARAMETER;
	sceClibMemset(&slot, 0, sizeof(slot));
	slot.size = sizeof(slot);
	slot.slotId = slotId;
	copySlotParamIn(param, &slot.slotParam);
	getModifiedTime(&slot.slotParam.modifiedTime);
	sceClibStrncpy((char *)slot.mountPoint.data, SCE_APPUTIL_SAVEDATA_DRIVE_NAME,
		       sizeof(slot.mountPoint) - 1);
	ret = sceAppMgrSaveDataSlotSetParam(&slot);
	if (ret == 0) {
		if (slot.slotParam.status == SCE_APPUTIL_SAVEDATA_SLOT_STATUS_AVAILABLE)
			SceAppUtil_F93C691C(SCE_APPUTIL_INTERNAL_EVENT_SAVEDATA_SLOT, slotId);
		return 0;
	}
	switch (ret) {
	case APPMGR_ERROR_SAVEDATA_SLOT_NOT_FOUND:
		ret = SCE_APPUTIL_ERROR_SAVEDATA_SLOT_NOT_FOUND;
		break;
	case APPMGR_ERROR_SAVEDATA_SLOT_FILE:
		ret = APPUTIL_ERROR_SAVEDATA_SLOT_FILE;
		break;
	case SCE_ERROR_ERRNO_ENODEV:
		ret = SCE_APPUTIL_ERROR_NOT_MOUNTED;
		break;
	}
	return ret;
}

int saveDataSlotGetParam(SceAppUtilSaveDataSlotId slotId, SceAppUtilSaveDataSlotParam *param,
			 const SceAppUtilMountPoint *mountPoint)
{
	SceAppMgrSaveDataSlot slot;
	int ret;

	if (checkSlotId(slotId) == 0 || checkMountPoint((const char *)mountPoint, MOUNT_POINT_ANY) == 0)
		return SCE_APPUTIL_ERROR_PARAMETER;
	if (param == NULL) {
		paramError(37);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibMemset(param, 0, sizeof(*param));
	sceClibMemset(&slot, 0, sizeof(slot));
	slot.size = sizeof(slot);
	slot.slotId = slotId;
	sceClibStrncpy((char *)slot.mountPoint.data,
		       mountPoint != NULL ? (const char *)mountPoint : SCE_APPUTIL_SAVEDATA_DRIVE_NAME,
		       sizeof(slot.mountPoint) - 1);
	ret = sceAppMgrSaveDataSlotGetParam(&slot);
	if (ret == 0) {
		copySlotParamOut(&slot.slotParam, param);
		return 0;
	}
	switch (ret) {
	case APPMGR_ERROR_SAVEDATA_SLOT_NOT_FOUND:
		ret = SCE_APPUTIL_ERROR_SAVEDATA_SLOT_NOT_FOUND;
		break;
	case APPMGR_ERROR_SAVEDATA_SLOT_FILE:
		ret = APPUTIL_ERROR_SAVEDATA_SLOT_FILE;
		break;
	case SCE_ERROR_ERRNO_ENODEV:
		ret = SCE_APPUTIL_ERROR_NOT_MOUNTED;
		break;
	}
	return ret;
}

int saveDataDataSave(const SceAppUtilSaveDataDataSlot *slot, const SceAppUtilSaveDataDataSaveItem *data,
		     SceUInt32 dataNum, const SceAppUtilMountPoint *mountPoint, SceSize *requiredSizeKiB)
{
	SceAppMgrSaveDataData save;
	SceAppMgrSaveDataSlotParam slotParam;
	int ret;

	if (checkDataSlot(slot) == 0 || checkMountPoint((const char *)mountPoint, MOUNT_POINT_OWN) == 0 ||
	    checkSaveItems(data, dataNum, mountPoint) == 0 || checkRequiredSize(requiredSizeKiB) == 0)
		return SCE_APPUTIL_ERROR_PARAMETER;
	sceClibMemset(&slotParam, 0, sizeof(slotParam));
	sceClibMemset(&save, 0, sizeof(save));
	save.size = sizeof(save);
	if (slot != NULL) {
		save.slotId = slot->id;
		if (slot->slotParam != NULL) {
			if (checkSlotParam(slot->slotParam) == 0)
				return SCE_APPUTIL_ERROR_PARAMETER;
			copySlotParamIn(slot->slotParam, &slotParam);
			getModifiedTime(&slotParam.modifiedTime);
			save.slotParam = &slotParam;
		} else {
			save.slotParam = NULL;
		}
	} else {
		save.slotId = SCE_APPUTIL_SAVEDATA_SLOT_ID_INVALID;
		save.slotParam = NULL;
	}
	save.items = data;
	save.itemNum = dataNum;
	sceClibStrncpy((char *)save.mountPoint.data, SCE_APPUTIL_SAVEDATA_DRIVE_NAME,
		       sizeof(save.mountPoint) - 1);
	ret = sceAppMgrSaveDataDataSave(&save);
	if (ret == 0) {
		*requiredSizeKiB = 0;
		if (slot != NULL)
			SceAppUtil_F93C691C(SCE_APPUTIL_INTERNAL_EVENT_SAVEDATA_SLOT, slot->id);
		return 0;
	}
	*requiredSizeKiB = save.requiredSizeKiB;
	switch (ret) {
	case APPMGR_ERROR_SAVEDATA_SLOT_FILE:
		ret = APPUTIL_ERROR_SAVEDATA_SLOT_FILE;
		break;
	case APPMGR_ERROR_SAVEDATA_NO_SPACE_QUOTA:
		ret = SCE_APPUTIL_ERROR_SAVEDATA_NO_SPACE_QUOTA;
		break;
	case APPMGR_ERROR_SAVEDATA_NO_SPACE_FS:
		ret = SCE_APPUTIL_ERROR_SAVEDATA_NO_SPACE_FS;
		break;
	case APPMGR_ERROR_SAVEDATA_PARAMETER:
		paramError(50);
		ret = SCE_APPUTIL_ERROR_PARAMETER;
		break;
	case SCE_ERROR_ERRNO_ENODEV:
		ret = SCE_APPUTIL_ERROR_NOT_MOUNTED;
		break;
	}
	return ret;
}

int saveDataDataRemove(const SceAppUtilSaveDataDataSlot *slot,
		       const SceAppUtilSaveDataDataRemoveItem *data, SceUInt32 dataNum,
		       const SceAppUtilMountPoint *mountPoint)
{
	SceAppMgrSaveDataDataDelete remove;
	SceAppMgrSaveDataSlotParam slotParam;
	int ret;

	if (checkDataSlot(slot) == 0 || checkRemoveItems(data, dataNum) == 0 ||
	    checkMountPoint((const char *)mountPoint, MOUNT_POINT_OWN) == 0)
		return SCE_APPUTIL_ERROR_PARAMETER;
	if (slot != NULL && slot->slotParam != NULL &&
	    data->mode == SCE_APPUTIL_SAVEDATA_DATA_REMOVE_MODE_DEFAULT) {
		paramError(38);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibMemset(&slotParam, 0, sizeof(slotParam));
	sceClibMemset(&remove, 0, sizeof(remove));
	remove.size = sizeof(remove);
	if (slot != NULL) {
		remove.slotId = slot->id;
		if (slot->slotParam != NULL) {
			if (checkSlotParam(slot->slotParam) == 0)
				return SCE_APPUTIL_ERROR_PARAMETER;
			copySlotParamIn(slot->slotParam, &slotParam);
			getModifiedTime(&slotParam.modifiedTime);
			remove.slotParam = &slotParam;
		} else {
			remove.slotParam = NULL;
		}
	} else {
		remove.slotId = SCE_APPUTIL_SAVEDATA_SLOT_ID_INVALID;
		remove.slotParam = NULL;
	}
	remove.items = data;
	remove.itemNum = dataNum;
	sceClibStrncpy((char *)remove.mountPoint.data, SCE_APPUTIL_SAVEDATA_DRIVE_NAME,
		       sizeof(remove.mountPoint) - 1);
	ret = sceAppMgrSaveDataDataRemove(&remove);
	if (ret == 0) {
		if (slot != NULL)
			SceAppUtil_F93C691C(SCE_APPUTIL_INTERNAL_EVENT_SAVEDATA_SLOT, slot->id);
		return 0;
	}
	switch (ret) {
	case APPMGR_ERROR_SAVEDATA_PARAMETER:
		paramError(50);
		ret = SCE_APPUTIL_ERROR_PARAMETER;
		break;
	case APPMGR_ERROR_SAVEDATA_SLOT_FILE:
		ret = APPUTIL_ERROR_SAVEDATA_SLOT_FILE;
		break;
	case SCE_ERROR_ERRNO_ENODEV:
		ret = SCE_APPUTIL_ERROR_NOT_MOUNTED;
		break;
	}
	return ret;
}

int saveDataGetQuota(SceSize *quotaSizeKiB, SceSize *usedSizeKiB, const SceAppUtilMountPoint *mountPoint)
{
	SceAppMgrSaveDataQuota quota;
	int ret;

	if (checkQuota(quotaSizeKiB, usedSizeKiB) == 0 ||
	    checkMountPoint((const char *)mountPoint, MOUNT_POINT_OWN) == 0)
		return SCE_APPUTIL_ERROR_PARAMETER;
	if (quotaSizeKiB != NULL)
		*quotaSizeKiB = 0;
	if (usedSizeKiB != NULL)
		*usedSizeKiB = 0;
	sceClibMemset(&quota, 0, sizeof(quota));
	quota.size = sizeof(quota);
	sceClibStrncpy((char *)quota.mountPoint.data,
		       mountPoint != NULL ? (const char *)mountPoint : SCE_APPUTIL_SAVEDATA_DRIVE_NAME,
		       sizeof(quota.mountPoint) - 1);
	ret = sceAppMgrSaveDataGetQuota(&quota);
	if (ret == 0) {
		if (quotaSizeKiB != NULL)
			*quotaSizeKiB = quota.quotaSizeKiB;
		if (usedSizeKiB != NULL)
			*usedSizeKiB = quota.usedSizeKiB;
		return 0;
	}
	if (ret == SCE_ERROR_ERRNO_ENODEV)
		ret = SCE_APPUTIL_ERROR_NOT_MOUNTED;
	return ret;
}

int pspSaveDataInit(void)
{
	g_pspSaveDataMountCount = 0;
	sceClibMemset(g_pspSaveDataMountPoint, 0, sizeof(g_pspSaveDataMountPoint));
	return 0;
}

int pspSaveDataLoad(SceAppUtilPspSaveDataParams *loadParam)
{
	char mountPoint[SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE];
	AppmgrPspSaveDataParams read;
	AppmgrPspSaveDataParamSfo sfo;
	int umount;
	int ret;

	if (checkPspSaveDataParams(loadParam) == 0)
		return SCE_APPUTIL_ERROR_PARAMETER;
	sceClibMemset(&read, 0, sizeof(read));
	sceClibMemset(&sfo, 0, sizeof(sfo));
	sceClibStrncpy(read.dirName, (const char *)loadParam->dirName, sizeof(read.dirName) - 1);
	sceClibStrncpy(read.fileName, (const char *)loadParam->fileName, sizeof(read.fileName) - 1);
	read.fileType = loadParam->fileType;
	read.dataVersion = loadParam->dataVersion;
	if (loadParam->secureFileId != NULL)
		sceClibMemcpy_safe(read.secureFileId, loadParam->secureFileId, sizeof(read.secureFileId));
	if (loadParam->paramSfo != NULL)
		read.paramSfo = &sfo;
	read.dataBuf = loadParam->dataBuf;
	read.dataBufSize = loadParam->dataBufSize;
	sceClibMemset(mountPoint, 0, sizeof(mountPoint));
	ret = SceAppUtil_C3CB128B(APPUTIL_MOUNT_PSP_SAVEDATA_ROOT, mountPoint);
	if (ret != 0 && ret != SCE_APPUTIL_ERROR_BUSY)
		return ret;
	ret = sceAppMgrPspSaveDataRead(&read);
	umount = SceAppUtil_C0418B1C(APPUTIL_MOUNT_PSP_SAVEDATA_ROOT);
	if (ret >= 0) {
		if (umount != 0) {
			ret = umount;
		} else if (loadParam->paramSfo != NULL) {
			sceClibMemset(loadParam->paramSfo, 0, sizeof(*loadParam->paramSfo));
			sceClibStrncpy((char *)loadParam->paramSfo->saveDataDirectory, sfo.saveDataDirectory,
				       sizeof(loadParam->paramSfo->saveDataDirectory) - 1);
			sceClibStrncpy((char *)loadParam->paramSfo->title, sfo.title,
				       sizeof(loadParam->paramSfo->title) - 1);
			sceClibStrncpy((char *)loadParam->paramSfo->saveDataTitle, sfo.saveDataTitle,
				       sizeof(loadParam->paramSfo->saveDataTitle) - 1);
			sceClibStrncpy((char *)loadParam->paramSfo->detail, sfo.detail,
				       sizeof(loadParam->paramSfo->detail) - 1);
			loadParam->paramSfo->parentalLev = sfo.parentalLev;
		}
		return ret;
	}
	switch (ret) {
	case PSP_SAVEDATA_ERROR_FIRST:
	case PSP_SAVEDATA_ERROR_FIRST + 1:
	case PSP_SAVEDATA_ERROR_FIRST + 2:
	case PSP_SAVEDATA_ERROR_FIRST + 3:
	case PSP_SAVEDATA_ERROR_FIRST + 4:
	case PSP_SAVEDATA_ERROR_FIRST + 5:
	case PSP_SAVEDATA_ERROR_FIRST + 6:
	case PSP_SAVEDATA_ERROR_LAST:
	case APPMGR_ERROR_PSP_SAVEDATA_BROKEN:
	case APPMGR_ERROR_PSP_SAVEDATA_DATABUF_SIZE + 1:
	case APPMGR_ERROR_PSP_SAVEDATA_DATABUF_SIZE + 2:
	case APPMGR_ERROR_PSP_SAVEDATA_DATABUF_SIZE + 3:
	case APPMGR_ERROR_PSP_SAVEDATA_LAST:
		ret = SCE_APPUTIL_ERROR_PSP_SAVEDATA_DATA_BROKEN;
		break;
	case APPMGR_ERROR_PSP_SAVEDATA_FILETYPE:
		ret = SCE_APPUTIL_ERROR_PSP_SAVEDATA_FILETYPE_MISMATCH;
		break;
	case APPMGR_ERROR_PSP_SAVEDATA_DATABUF_SIZE:
		ret = SCE_APPUTIL_ERROR_PSP_SAVEDATA_DATABUF_SIZE;
		break;
	}
	return ret;
}

/* mounts the PSP save data root once for all callers; mountPoint gets where */
int pspSaveDataRootMount(char *mountPoint)
{
	char drive[SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE];
	int ret;

	if (mountPoint == NULL)
		return SCE_APPUTIL_ERROR_PARAMETER;
	sceClibMemset(drive, 0, sizeof(drive));
	ret = sceAppMgrPspSaveDataRootMount(drive);
	if (ret == 0 || ret == APPMGR_ERROR_ALREADY_MOUNTED)
		g_pspSaveDataMountCount++;
	if (ret == 0) {
		sceClibMemset(g_pspSaveDataMountPoint, 0, sizeof(g_pspSaveDataMountPoint));
		sceClibStrncpy(g_pspSaveDataMountPoint, drive, sizeof(g_pspSaveDataMountPoint) - 1);
		sceClibMemset(mountPoint, 0, SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE);
		sceClibStrncpy(mountPoint, g_pspSaveDataMountPoint, sizeof(g_pspSaveDataMountPoint) - 1);
	} else if (ret == APPMGR_ERROR_ALREADY_MOUNTED) {
		ret = SCE_APPUTIL_ERROR_BUSY;
		sceClibMemset(mountPoint, 0, SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE);
		sceClibStrncpy(mountPoint, g_pspSaveDataMountPoint, sizeof(g_pspSaveDataMountPoint) - 1);
	}
	return ret;
}

/* unmounts the PSP save data root when its last caller is done with it */
int pspSaveDataRootUmount(void)
{
	int ret = 0;

	if (--g_pspSaveDataMountCount <= 0) {
		ret = sceAppMgrUmount(g_pspSaveDataMountPoint);
		if (ret == 0) {
			sceClibMemset(g_pspSaveDataMountPoint, 0, sizeof(g_pspSaveDataMountPoint));
		} else if (ret == APPMGR_ERROR_NOT_MOUNTED) {
			ret = SCE_APPUTIL_ERROR_NOT_MOUNTED;
		} else if (ret == SCE_ERROR_ERRNO_EBUSY) {
			ret = SCE_APPUTIL_ERROR_BUSY;
		}
		g_pspSaveDataMountCount = 0;
	}
	return ret;
}

int pspSaveDataGetDirNameList(const SceAppUtilPspSaveDataDirName *prefix,
			      SceAppUtilPspSaveDataDirName *dirNameList, SceUInt32 dirNameListNum,
			      SceUInt32 *hitNum)
{
	char mountPoint[SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE];
	char name[SCE_APPUTIL_PSP_SAVEDATA_PARAMSFO_DIRECTORY_SIZE];
	char path[SCE_APPUTIL_PSP_SAVEDATA_PARAMSFO_DIRECTORY_SIZE];
	SceIoDirent dirent;
	SceUInt32 count = 0;
	SceSize length;
	SceUID fd;
	int ret;

	sceClibMemset(mountPoint, 0, sizeof(mountPoint));
	if (checkPspSaveDataDirName((const char *)prefix) == 0) {
		ret = SCE_APPUTIL_ERROR_PARAMETER;
	} else if (dirNameList == NULL) {
		ret = SCE_APPUTIL_ERROR_PARAMETER;
	} else if (dirNameListNum > SCE_APPUTIL_PSP_SAVEDATA_DIRNAME_LIST_MAXNUM) {
		ret = SCE_APPUTIL_ERROR_PARAMETER;
	} else if (hitNum == NULL) {
		ret = SCE_APPUTIL_ERROR_PARAMETER;
	} else {
		sceClibMemset(name, 0, sizeof(name));
		sceClibStrncpy(name, (const char *)prefix, sizeof(name) - 1);
		length = sceClibStrnlen(name, sizeof(name));
		if (length >= sizeof(name)) {
			ret = SCE_APPUTIL_ERROR_PARAMETER;
		} else {
			sceClibMemset(dirNameList, 0, dirNameListNum * sizeof(*dirNameList));
			*hitNum = 0;
			ret = pspSaveDataRootMount(mountPoint);
			if (ret == 0 || ret == SCE_APPUTIL_ERROR_BUSY) {
				sceClibSnprintf(path, sizeof(path), "%s%s", mountPoint, "PSP/SAVEDATA");
				fd = sceIoDopen(path);
				ret = fd;
				if (fd >= 0) {
					while ((ret = sceIoDread(fd, &dirent)) != 0) {
						if (sceClibStrncmp(dirent.d_name, name, length) != 0)
							continue;
						if (count < dirNameListNum)
							sceClibStrncpy((char *)dirNameList->data, dirent.d_name,
								       sizeof(dirNameList->data) - 1);
						dirNameList++;
						count++;
					}
					if (ret >= 0) {
						ret = 0;
						*hitNum = count;
					}
					if (ret < 0 || fd != -1)
						sceIoDclose(fd);
				}
			}
		}
	}
	if (mountPoint[0] != '\0')
		pspSaveDataRootUmount();
	return ret;
}

