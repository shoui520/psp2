/*
 * Additional content: another title's add-on content mounted at addcont1:, and an add-on content
 * directory opened under its DRM (sceAppUtilDrmOpen, sceAppUtilDrmClose).
 */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <kernel/libkernel.h>
#include <libkernel/libkernel.h>
#include <rtc.h>
#include <apputil.h>
#include <appmgr/appmgr.h>
#include <appmgr/appmgr_savedata.h>
#include "apputil_private.h"

/* an add-on content mount point, "addcont0:" to "addcont2:", or NULL for addcont0:; added: one of
 * another title's (addcont1: or addcont2:, never NULL) */
int checkAddcontMountPoint(const char *mountPoint, int added)
{
	int c;

	if (mountPoint == NULL) {
		if (added) {
			paramError(33);
			return 0;
		}
		return 1;
	}
	if (sceClibStrnlen(mountPoint, SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE) != 9) {
		paramError(36);
		return 0;
	}
	if (sceClibStrncmp("addcont", mountPoint, 7) != 0 || (c = mountPoint[7]) < '0' || c > '2' ||
	    (added == 1 && c == '0') || mountPoint[8] != ':') {
		paramError(35);
		return 0;
	}
	return 1;
}

/* a title ID: four capital letters and five digits, the rest of its 16 bytes zero */
static inline int checkAddcontTitleId(const char *titleId)
{
	SceUInt32 i;

	if (sceClibStrnlen(titleId, SCE_APPUTIL_TITLE_ID_DATA_SIZE) != SCE_APPUTIL_TITLE_ID_DATA_SIZE - 1) {
		paramError(31);
		return 0;
	}
	for (i = 0; i < 4; i++) {
		if (titleId[i] < 'A' || titleId[i] > 'Z') {
			paramError(30);
			return 0;
		}
	}
	for (i = 4; i < SCE_APPUTIL_TITLE_ID_DATA_SIZE - 1; i++) {
		if (titleId[i] < '0' || titleId[i] > '9') {
			paramError(30);
			return 0;
		}
	}
	for (i = 0; i < 6; i++) {
		if (titleId[SCE_APPUTIL_TITLE_ID_DATA_SIZE + i] != '\0') {
			paramError(28);
			return 0;
		}
	}
	return 1;
}

/* a passcode: 32 letters, digits, '-' or '_' */
static inline int checkAddcontPassCode(const SceUChar8 *passCode)
{
	SceUInt32 i;

	for (i = 0; i < SCE_APPUTIL_PASSCODE_DATA_SIZE; i++) {
		if (!((passCode[i] >= 'a' && passCode[i] <= 'z') || (passCode[i] >= 'A' && passCode[i] <= 'Z') ||
		      (passCode[i] >= '0' && passCode[i] <= '9') || passCode[i] == '-' || passCode[i] == '_')) {
			paramError(21);
			return 0;
		}
	}
	return 1;
}

int addcontMount(const SceAppUtilTitleId *titleId, int reserved, const SceAppUtilPassCode *passCode,
		 int force)
{
	char mountPoint[SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE];
	AppmgrAddMountParam param;
	const char *drive;
	int ret;

	(void)reserved;
	sceClibMemset(&param, 0, sizeof(param));
	sceClibStrncpy(mountPoint, SCE_APPUTIL_ADDCONT_DRIVE_NAME_ADD, sizeof(mountPoint));
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
	if (checkAddcontTitleId(param.titleId) == 0 ||
	    checkAddcontMountPoint(param.mountPoint.drive_id, 1) == 0 ||
	    checkAddcontPassCode((const SceUChar8 *)param.passcode) == 0)
		return SCE_APPUTIL_ERROR_PARAMETER;
	ret = sceAppMgrAddContAddMount(&param, force);
	switch (ret) {
	case APPMGR_ERROR_ALREADY_MOUNTED:
		ret = SCE_APPUTIL_ERROR_BUSY;
		break;
	case APPMGR_ERROR_PASSCODE_MISMATCH:
		ret = SCE_APPUTIL_ERROR_PASSCODE_MISMATCH;
		break;
	case APPMGR_ERROR_ADDCONT_LOCATION:
		paramError(29);
		ret = SCE_APPUTIL_ERROR_PARAMETER;
		break;
	}
	return ret;
}

int addcontUmount(int reserved)
{
	char mountPoint[SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE];
	char drive[SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE];
	const char *name;
	int ret;

	(void)reserved;
	sceClibStrncpy(drive, SCE_APPUTIL_ADDCONT_DRIVE_NAME_ADD, sizeof(drive));
	name = drive;
	if (name == NULL) {
		paramError(33);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibMemcpy_safe(mountPoint, name, sizeof(mountPoint));
	if (checkAddcontMountPoint(mountPoint, 1) == 0)
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

/* an add-on content directory name: 16 capital letters or digits */
int checkDrmAddcontId(const char *dirName)
{
	SceSize i;

	if (dirName == NULL) {
		paramError(80);
		return 0;
	}
	if (sceClibStrnlen(dirName, SCE_APPUTIL_NP_DRM_ADDCONT_ID_SIZE) != SCE_APPUTIL_NP_DRM_ADDCONT_ID_SIZE - 1) {
		paramError(82);
		return 0;
	}
	for (i = 0; i < sceClibStrnlen(dirName, SCE_APPUTIL_NP_DRM_ADDCONT_ID_SIZE); i++) {
		if ((dirName[i] < 'A' || dirName[i] > 'Z') && (dirName[i] < '0' || dirName[i] > '9')) {
			paramError(81);
			return 0;
		}
	}
	return 1;
}

int drmOpen(const SceAppUtilDrmAddcontId *dirName, const SceAppUtilMountPoint *mountPoint)
{
	SceAppMgrDrmOpenParam param;
	int ret;

	if (checkDrmAddcontId((const char *)dirName) == 0 ||
	    checkAddcontMountPoint((const char *)mountPoint, 0) == 0)
		return SCE_APPUTIL_ERROR_PARAMETER;
	sceClibMemset(&param, 0, sizeof(param));
	param.size = sizeof(param);
	sceClibStrncpy(param.dirName.data, (const char *)dirName, 16);
	sceClibStrncpy(param.mountPoint,
		       mountPoint != NULL ? (const char *)mountPoint : SCE_APPUTIL_ADDCONT_DRIVE_NAME, 15);
	ret = sceAppMgrDrmOpen(&param);
	switch (ret) {
	case APPMGR_ERROR_NOT_MOUNTED:
		ret = SCE_APPUTIL_ERROR_NOT_MOUNTED;
		break;
	case APPMGR_ERROR_LICENSE_NOT_FOUND:
	case APPMGR_ERROR_LICENSE_OPEN:
	case APPMGR_ERROR_LICENSE_EXPIRED:
	case APPMGR_ERROR_LICENSE_NOT_STARTED:
	case NPDRM_ERROR_DEBUG_LICENSE:
	case NPDRM_ERROR_NO_LICENSE:
	case NPDRM_ERROR_INVALID_LICENSE:
	case NPDRM_ERROR_NOT_ACTIVATED:
	case NPDRM_ERROR_ACTIVATION_TOO_OLD:
	case NPDRM_ERROR_ACCOUNT_MISMATCH:
	case SCE_RTC_ERROR_NOT_INITIALIZED:
		ret = SCE_APPUTIL_ERROR_DRM_NO_ENTITLEMENT;
		break;
	}
	return ret;
}

int drmClose(const SceAppUtilDrmAddcontId *dirName, const SceAppUtilMountPoint *mountPoint)
{
	SceAppMgrDrmOpenParam param;
	int ret;

	if (checkDrmAddcontId((const char *)dirName) == 0 ||
	    checkAddcontMountPoint((const char *)mountPoint, 0) == 0)
		return SCE_APPUTIL_ERROR_PARAMETER;
	sceClibMemset(&param, 0, sizeof(param));
	param.size = sizeof(param);
	sceClibStrncpy(param.dirName.data, (const char *)dirName, 16);
	sceClibStrncpy(param.mountPoint,
		       mountPoint != NULL ? (const char *)mountPoint : SCE_APPUTIL_ADDCONT_DRIVE_NAME, 15);
	ret = sceAppMgrDrmClose(&param);
	switch (ret) {
	case SCE_ERROR_ERRNO_ENOENT:
	case APPMGR_ERROR_NOT_MOUNTED:
		ret = SCE_APPUTIL_ERROR_NOT_MOUNTED;
		break;
	case SCE_ERROR_ERRNO_EBUSY:
		ret = SCE_APPUTIL_ERROR_BUSY;
		break;
	}
	return ret;
}
