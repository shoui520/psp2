/*
 * photo0:, the safe memory, the system and application parameters: sceAppUtilSystemParamGetInt
 * and GetString from the registry and SceAppMgr, with the user name kept once read, and
 * sceAppUtilAppParamGetInt.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <system_param.h>
#include <apputil.h>
#include <regmgr/regmgr.h>
#include <appmgr.h>
#include <apputil/apputil_internal.h>
#include <appmgr/appmgr.h>
#include "apputil_private.h"

/* the user name as last read, empty until then */
static char g_userName[SCE_SYSTEM_PARAM_USER_NAME_MAXSIZE];

/* the system data file SceAppMgr keeps at location 0: the user's own NP profile */
typedef struct SystemDataMyProfile {
	SceUInt8 reserved[36];
	char onlineId[292];
} SystemDataMyProfile;

int photoMount(void)
{
	SceAppMgrPhotoMountParam param;
	char mountPoint[16];
	int ret;

	sceClibMemset(&param, 0, sizeof(param));
	param.size = sizeof(param);
	ret = sceAppMgrAppDataMount(APPMGR_MOUNT_ID_PHOTO,
				    (struct ScePfsRndDriveId *)mountPoint);
	if (ret == SCE_OK)
		return SCE_OK;
	switch (ret) {
	case APPMGR_ERROR_MOUNT_LIMIT_OVER:
		return SCE_APPUTIL_ERROR_MOUNT_LIMIT_OVER;
	case APPMGR_ERROR_ALREADY_MOUNTED:
		return SCE_APPUTIL_ERROR_BUSY;
	case SCE_ERROR_ERRNO_ENODEV:
		return SCE_APPUTIL_ERROR_PHOTO_DEVICE_NOT_FOUND;
	}
	return ret;
}

int photoUmount(void)
{
	SceAppMgrPhotoUmountParam param;
	int ret;

	sceClibMemset(&param, 0, sizeof(param));
	param.size = sizeof(param);
	ret = sceAppMgrPhotoUmount(&param);
	if (ret == SCE_OK)
		return SCE_OK;
	if (ret == APPMGR_ERROR_PHOTO_NOT_MOUNTED)
		return SCE_APPUTIL_ERROR_NOT_MOUNTED;
	return ret;
}

int initSafeMemory(const char *id)
{
	int ret;

	ret = sceAppMgrInitSafeMemoryById(id);
	switch (ret) {
	case APPMGR_ERROR_INVALID_BUFFER:
		paramError(290);
		return SCE_APPUTIL_ERROR_PARAMETER;
	case APPMGR_ERROR_SAFEMEMORY_INITIALIZED:
		return SCE_APPUTIL_ERROR_BUSY;
	case APPMGR_ERROR_NO_PERMISSION:
		return SCE_APPUTIL_ERROR_NO_PERMISSION;
	case APPMGR_ERROR_INVALID_ARGUMENT:
		paramError(291);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	return ret;
}

int saveSafeMemory(const void *buf, SceSize bufSize, SceOff offset)
{
	int ret;

	ret = sceAppMgrSaveSafeMemory(buf, bufSize, offset);
	if (ret == (int)bufSize)
		return ret;
	switch (ret) {
	case APPMGR_ERROR_INVALID_BUFFER:
		paramError(110);
		return SCE_APPUTIL_ERROR_PARAMETER;
	case APPMGR_ERROR_OUT_OF_RANGE:
		paramError(111);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	return ret;
}

int loadSafeMemory(void *buf, SceSize bufSize, SceOff offset)
{
	int ret;

	ret = sceAppMgrLoadSafeMemory(buf, bufSize, offset);
	if (ret == (int)bufSize)
		return ret;
	switch (ret) {
	case APPMGR_ERROR_INVALID_BUFFER:
		paramError(110);
		return SCE_APPUTIL_ERROR_PARAMETER;
	case APPMGR_ERROR_OUT_OF_RANGE:
		paramError(111);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	return ret;
}

int systemParamInit(void)
{
	sceClibMemset(g_userName, 0, sizeof(g_userName));
	return SCE_OK;
}

static int systemParamRegKey(SceSystemParamId paramId, int *key)
{
	switch (paramId) {
	case SCE_SYSTEM_PARAM_ID_LANG:
		*key = REGMGR_LANGUAGE;
		return SCE_OK;
	case SCE_SYSTEM_PARAM_ID_ENTER_BUTTON:
		*key = REGMGR_ENTER_BUTTON;
		return SCE_OK;
	case SCE_SYSTEM_PARAM_ID_USER_NAME:
		*key = REGMGR_SYSTEM_USERNAME;
		return SCE_OK;
	}
	*key = -1;
	paramError(100);
	return SCE_APPUTIL_ERROR_PARAMETER;
}

static int isIntParam(SceSystemParamId paramId);
static int isStringParam(SceSystemParamId paramId);

/* SceAppMgr's own number for a system parameter it keeps */
static inline int appMgrIntParam(SceSystemParamId paramId)
{
	switch (paramId) {
	case SYSTEM_PARAM_ID_IMPOSE_AUDIO_BALANCE:
		return APPMGR_SYSTEM_PARAM_IMPOSE_AUDIO_BALANCE;
	case SYSTEM_PARAM_ID_VOICE_PRIORITY:
		return APPMGR_SYSTEM_PARAM_VOICE_PRIORITY;
	case SYSTEM_PARAM_ID_HDMI_OUT_SCALING_RATIO:
		return APPMGR_SYSTEM_PARAM_HDMI_OUT_SCALING_RATIO;
	}
	return 0;
}

static inline int appMgrStringParam(SceSystemParamId paramId)
{
	switch (paramId) {
	case SYSTEM_PARAM_ID_NP_COUNTRY:
		return APPMGR_SYSTEM_PARAM_NP_COUNTRY;
	case SYSTEM_PARAM_ID_NP_ENV:
		return APPMGR_SYSTEM_PARAM_NP_ENV;
	case SYSTEM_PARAM_ID_PSNOW_APP_CACHED_URL:
		return APPMGR_SYSTEM_PARAM_PSNOW_APP_CACHED_URL;
	}
	return 0;
}

int systemParamGetInt(SceSystemParamId paramId, SceInt32 *value)
{
	SceAppMgrDateTimeConf conf;
	int key;
	int ret;

	if (!isIntParam(paramId)) {
		paramError(100);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (value == NULL) {
		paramError(101);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	switch (paramId) {
	case SCE_SYSTEM_PARAM_ID_DATE_FORMAT:
	case SCE_SYSTEM_PARAM_ID_TIME_FORMAT:
	case SCE_SYSTEM_PARAM_ID_TIME_ZONE:
	case SCE_SYSTEM_PARAM_ID_SUMMERTIME:
		ret = sceAppMgrSystemParamDateTimeGetConf(&conf);
		if (ret != SCE_OK)
			return ret;
		switch (paramId) {
		case SCE_SYSTEM_PARAM_ID_DATE_FORMAT:
			*value = conf.dateFormat;
			break;
		case SCE_SYSTEM_PARAM_ID_TIME_FORMAT:
			*value = conf.timeFormat;
			break;
		case SCE_SYSTEM_PARAM_ID_TIME_ZONE:
			*value = conf.timeZone;
			break;
		case SCE_SYSTEM_PARAM_ID_SUMMERTIME:
			*value = conf.summertime;
			break;
		default:
			return SCE_APPUTIL_ERROR_PARAMETER;
		}
		return SCE_OK;
	case SYSTEM_PARAM_ID_IMPOSE_AUDIO_BALANCE:
	case SYSTEM_PARAM_ID_VOICE_PRIORITY:
	case SYSTEM_PARAM_ID_HDMI_OUT_SCALING_RATIO:
		ret = sceAppMgrSystemParamGetInt(appMgrIntParam(paramId), value);
		if (ret != SCE_OK)
			return ret;
		return SCE_OK;
	}
	ret = systemParamRegKey(paramId, &key);
	if (ret != SCE_OK)
		return ret;
	ret = sceRegMgrUtilityGetInt(key, value);
	if (ret != SCE_OK)
		return ret;
	/* Turkish came with SDK 2.00: older applications get English (United Kingdom) */
	if (paramId == SCE_SYSTEM_PARAM_ID_LANG && *value == SCE_SYSTEM_PARAM_LANG_TURKISH &&
	    g_sdkVersion < 0x2000000)
		*value = SCE_SYSTEM_PARAM_LANG_ENGLISH_GB;
	return SCE_OK;
}

static inline int getUserName(SceSystemParamId paramId, SceChar8 *buf, SceSize bufSize)
{
	SystemDataMyProfile profile;
	SceUInt64 accountId;
	int key;
	int ret = SCE_OK;

	if (g_userName[0] != '\0') {
		sceClibStrncpy((char *)buf, g_userName, SCE_SYSTEM_PARAM_USER_NAME_MAXSIZE - 1);
		buf[SCE_SYSTEM_PARAM_USER_NAME_MAXSIZE - 1] = ret;
		return ret;
	}
	if (sceRegMgrUtilityGetBin(REGMGR_NP_ACCOUNT_ID, &accountId, sizeof(accountId)) == SCE_OK &&
	    accountId != 0) {
		sceClibMemset(&profile, 0, sizeof(profile));
		if (sceAppMgrGetSystemDataFile(0, &profile, sizeof(profile)) == sizeof(profile)) {
			sceClibStrncpy((char *)buf, profile.onlineId,
				       SCE_SYSTEM_PARAM_USER_NAME_MAXSIZE - 1);
			buf[SCE_SYSTEM_PARAM_USER_NAME_MAXSIZE - 1] = ret;
			sceClibMemset(g_userName, 0, sizeof(g_userName));
			sceClibStrncpy(g_userName, profile.onlineId,
				       SCE_SYSTEM_PARAM_USER_NAME_MAXSIZE - 1);
			return ret;
		}
	}
	if (systemParamRegKey(paramId, &key) != SCE_OK ||
	    sceRegMgrUtilityGetStr(key, (char *)buf, bufSize) != SCE_OK)
		sceClibStrncpy((char *)buf, "User999", bufSize - 1);
	return ret;
}

int systemParamGetString(SceSystemParamId paramId, SceChar8 *buf, SceSize bufSize)
{
	int ret;

	if (!isStringParam(paramId)) {
		paramError(100);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (buf == NULL) {
		paramError(101);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if ((paramId == SYSTEM_PARAM_ID_NP_COUNTRY && bufSize < 3) ||
	    (paramId == SCE_SYSTEM_PARAM_ID_USER_NAME &&
	     bufSize < SCE_SYSTEM_PARAM_USER_NAME_MAXSIZE) ||
	    (paramId == SYSTEM_PARAM_ID_NP_ENV && bufSize < 17) ||
	    (paramId == SYSTEM_PARAM_ID_PSNOW_APP_CACHED_URL && bufSize < 256)) {
		paramError(102);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	switch (paramId) {
	case SCE_SYSTEM_PARAM_ID_USER_NAME:
		return getUserName(paramId, buf, bufSize);
	case SYSTEM_PARAM_ID_NP_COUNTRY:
	case SYSTEM_PARAM_ID_NP_ENV:
	case SYSTEM_PARAM_ID_PSNOW_APP_CACHED_URL:
		ret = sceAppMgrSystemParamGetString(appMgrStringParam(paramId), (char *)buf, bufSize);
		if (ret == SCE_OK)
			return SCE_OK;
		if (ret == APPMGR_ERROR_NO_PERMISSION) {
			paramError(100);
			return SCE_APPUTIL_ERROR_PARAMETER;
		}
		return ret;
	}
	return SCE_APPUTIL_ERROR_PARAMETER;
}

static int isIntParam(SceSystemParamId paramId)
{
	switch (paramId) {
	case SCE_SYSTEM_PARAM_ID_LANG:
	case SCE_SYSTEM_PARAM_ID_ENTER_BUTTON:
	case SCE_SYSTEM_PARAM_ID_DATE_FORMAT:
	case SCE_SYSTEM_PARAM_ID_TIME_FORMAT:
	case SCE_SYSTEM_PARAM_ID_TIME_ZONE:
	case SCE_SYSTEM_PARAM_ID_SUMMERTIME:
	case SYSTEM_PARAM_ID_IMPOSE_AUDIO_BALANCE:
	case SYSTEM_PARAM_ID_VOICE_PRIORITY:
	case SYSTEM_PARAM_ID_HDMI_OUT_SCALING_RATIO:
		return 1;
	}
	return 0;
}

static int isStringParam(SceSystemParamId paramId)
{
	return paramId == SCE_SYSTEM_PARAM_ID_USER_NAME || paramId == SYSTEM_PARAM_ID_NP_COUNTRY ||
	       paramId == SYSTEM_PARAM_ID_NP_ENV || paramId == SYSTEM_PARAM_ID_PSNOW_APP_CACHED_URL;
}

int appParamGetInt(SceAppUtilAppParamId paramId, SceInt32 *value)
{
	int ret;

	if (paramId != SCE_APPUTIL_APPPARAM_ID_SKU_FLAG) {
		paramError(120);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (value == NULL) {
		paramError(121);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	ret = sceAppMgrAppParamGetInt(sceKernelGetProcessId(), 2, value);
	if (ret == SCE_OK)
		return SCE_OK;
	return ret;
}
