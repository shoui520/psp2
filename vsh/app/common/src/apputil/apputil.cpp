/*
 * SceAppUtil's start and its exports: sceAppUtilInit, and the calls that pass on to the code of
 * each part once the library is initialized.
 */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <kernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <apputil.h>
#include <apputil/apputil_psp.h>
#include <apputil/apputil_internal.h>
#include <appmgr/appmgr.h>
#include <appmgr/appmgr_savedata.h>
#include "apputil_private.h"

int g_initialized;
/* SceAppUtil_CD0E5C59's callback, told of each save data slot change */
static int (*g_slotCallback)(int event, SceAppUtilSaveDataSlotId id, void *arg);
static void *g_slotCallbackArg;
SceUInt32 g_sdkVersion;

extern "C" int module_start(SceSize argc, const void *argp)
{
	int ret;

	(void)argc;
	(void)argp;
	g_sdkVersion = SceLibKernel_9F793F84();
	ret = umassInit();
	if (ret < 0)
		return ret;
	ret = bookInit();
	if (ret < 0)
		return ret;
	ret = musicInit();
	if (ret < 0)
		return ret;
	ret = pspSaveDataInit();
	if (ret < 0)
		return ret;
	return systemParamInit();
}

extern "C" int module_stop(SceSize argc, const void *argp)
{
	(void)argc;
	(void)argp;
	return 0;
}

extern "C" int module_exit(SceSize argc, const void *argp)
{
	(void)argc;
	(void)argp;
	return 0;
}

void paramError(int code)
{
	sceClibPrintf("\n\n****** SceAppUtil Parameter Error : %d ******\n\n", code);
}

extern "C" SceInt32 sceAppUtilInit(const SceAppUtilInitParam *initParam, SceAppUtilBootParam *bootParam)
{
	AppmgrBootParam boot;
	AppmgrSaveDataSlotInitParam slotInit;
	SceUInt32 i;
	int ret;

	/* a second init is an error from SDK 1.50 */
	if (g_sdkVersion >= 0x1500000 && g_initialized != 0)
		return SCE_APPUTIL_ERROR_BUSY;
	if (initParam == NULL) {
		paramError(1);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (bootParam == NULL) {
		paramError(4);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (initParam->workBufSize != 0) {
		paramError(3);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	for (i = 0; i < sizeof(initParam->reserved); i++) {
		if (initParam->reserved[i] != 0) {
			paramError(2);
			return SCE_APPUTIL_ERROR_PARAMETER;
		}
	}
	for (i = 0; i < sizeof(bootParam->reserved); i++) {
		if (bootParam->reserved[i] != 0) {
			paramError(5);
			return SCE_APPUTIL_ERROR_PARAMETER;
		}
	}
	sceClibMemset(&boot, 0, sizeof(boot));
	boot.size = sizeof(boot);
	ret = sceAppMgrGetBootParam(sceKernelGetProcessId(), &boot);
	if (ret != 0)
		return ret;
	bootParam->attr = boot.attribute;
	bootParam->appVersion = boot.appVersion;
	sceClibMemset(&slotInit, 0, sizeof(slotInit));
	slotInit.size = sizeof(slotInit);
	slotInit.slotCount = SCE_APPUTIL_SAVEDATA_SLOT_MAX;
	slotInit.flags = 0;
	sceClibStrncpy((char *)slotInit.mountPoint.data, SCE_APPUTIL_SAVEDATA_DRIVE_NAME,
		       sizeof(slotInit.mountPoint) - 1);
	ret = sceAppMgrSaveDataSlotInit(&slotInit);
	/* no save data drive is no error */
	if (ret == 0 || ret == SCE_ERROR_ERRNO_ENODEV) {
		g_initialized = 1;
		ret = 0;
	}
	return ret;
}

extern "C" SceInt32 sceAppUtilShutdown(void)
{
	if (g_initialized == 0)
		return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
	g_initialized = 0;
	return 0;
}

extern "C" int SceAppUtil_A3E0694F(SceAppUtilInternalState *state)
{
	if (state == NULL)
		return APPUTIL_ERROR_NULL_STATE;
	if (g_initialized == 0)
		return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
	sceClibMemset(state, 0, sizeof(*state));
	state->sdkVersion = SceLibKernel_9F793F84();
	return 0;
}


extern "C" SceInt32 sceAppUtilReceiveAppEvent(SceAppUtilAppEventParam *eventParam)
{
	if (g_initialized != 0)
		return receiveAppEvent(eventParam);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilAppEventParseNpInviteMessage(const SceAppUtilAppEventParam *eventParam, SceAppUtilNpInviteMessageParam *param)
{
	if (g_initialized != 0)
		return appEventParseNpInviteMessage(eventParam, param);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilAppEventParseNpAppDataMessage(const SceAppUtilAppEventParam *eventParam, SceAppUtilNpAppDataMessageParam *param)
{
	if (g_initialized != 0)
		return appEventParseNpAppDataMessage(eventParam, param);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilAppEventParseNpBasicJoinablePresence(const SceAppUtilAppEventParam *eventParam, SceAppUtilNpBasicJoinablePresenceParam *param)
{
	if (g_initialized != 0)
		return appEventParseNpBasicJoinablePresence(eventParam, param);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilAppEventParseNearGift(const SceAppUtilAppEventParam *eventParam, SceAppUtilNearGiftParam *param)
{
	if (g_initialized != 0)
		return appEventParseNearGift(eventParam, param);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" int SceAppUtilAppEventUserDefined_54580104(const SceAppUtilAppEventParam *eventParam, SceAppUtilUserDefinedParam *param)
{
	if (g_initialized != 0)
		return appEventParseUserDefined(eventParam, param);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilAppEventParseWebBrowser(const SceAppUtilAppEventParam *eventParam, SceAppUtilWebBrowserEventParam *param)
{
	if (g_initialized != 0)
		return appEventParseWebBrowser(eventParam, param);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilAppEventParseLiveArea(const SceAppUtilAppEventParam *eventParam, SceAppUtilLiveAreaParam *param)
{
	if (g_initialized != 0)
		return appEventParseLiveArea(eventParam, param);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilAppEventParseScreenShotNotification(const SceAppUtilAppEventParam *eventParam, SceAppUtilScreenShotNotification *param)
{
	if (g_initialized != 0)
		return appEventParseScreenShotNotification(eventParam, param);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilAppEventParseNpActivity(const SceAppUtilAppEventParam *eventParam, SceAppUtilNpActivityParam *param)
{
	if (g_initialized != 0)
		return appEventParseNpActivity(eventParam, param);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilAppEventParseTeleport(const SceAppUtilAppEventParam *eventParam, SceAppUtilTeleportParam *param)
{
	if (g_initialized != 0)
		return appEventParseTeleport(eventParam, param);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" int SceAppUtilLaunchApp_E70C7435(const SceAppUtilAppEventParam *eventParam, SceAppUtilLaunchAppParam *param)
{
	if (g_initialized != 0)
		return appEventParseLaunchApp(eventParam, param);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilSaveDataMount(const SceAppUtilTitleId *titleId, const SceAppUtilPassCode *passCode)
{
	if (g_initialized != 0)
		return saveDataMount(titleId, 0, passCode);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilSaveDataUmount(void)
{
	if (g_initialized != 0)
		return saveDataUmount();
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilSaveDataSlotCreate(SceAppUtilSaveDataSlotId slotId, const SceAppUtilSaveDataSlotParam *param, const SceAppUtilMountPoint *mountPoint)
{
	if (g_initialized != 0)
		return saveDataSlotCreate(slotId, param, mountPoint);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilSaveDataSlotDelete(SceAppUtilSaveDataSlotId slotId, const SceAppUtilMountPoint *mountPoint)
{
	if (g_initialized != 0)
		return saveDataSlotDelete(slotId, mountPoint);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilSaveDataSlotSearch(SceAppUtilWorkBuffer *workBuf, const SceAppUtilSaveDataSlotSearchCond *cond, SceAppUtilSlotSearchResult *result, const SceAppUtilMountPoint *mountPoint)
{
	if (g_initialized != 0)
		return saveDataSlotSearch(workBuf, cond, result, mountPoint);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilSaveDataSlotSetParam(SceAppUtilSaveDataSlotId slotId, const SceAppUtilSaveDataSlotParam *param, const SceAppUtilMountPoint *mountPoint)
{
	if (g_initialized != 0)
		return saveDataSlotSetParam(slotId, param, mountPoint);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilSaveDataSlotGetParam(SceAppUtilSaveDataSlotId slotId, SceAppUtilSaveDataSlotParam *param, const SceAppUtilMountPoint *mountPoint)
{
	if (g_initialized != 0)
		return saveDataSlotGetParam(slotId, param, mountPoint);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilSaveDataDataSave(const SceAppUtilSaveDataDataSlot *slot, const SceAppUtilSaveDataDataSaveItem *data, SceUInt32 dataNum, const SceAppUtilMountPoint *mountPoint, SceSize *requiredSizeKiB)
{
	if (g_initialized != 0)
		return saveDataDataSave(slot, data, dataNum, mountPoint, requiredSizeKiB);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilSaveDataDataRemove(const SceAppUtilSaveDataDataSlot *slot, const SceAppUtilSaveDataDataRemoveItem *data, SceUInt32 dataNum, const SceAppUtilMountPoint *mountPoint)
{
	if (g_initialized != 0)
		return saveDataDataRemove(slot, data, dataNum, mountPoint);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilSaveDataGetQuota(SceSize *quotaSizeKiB, SceSize *usedSizeKiB, const SceAppUtilMountPoint *mountPoint)
{
	if (g_initialized != 0)
		return saveDataGetQuota(quotaSizeKiB, usedSizeKiB, mountPoint);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilPspSaveDataGetDirNameList(const SceAppUtilPspSaveDataDirName *prefix, SceAppUtilPspSaveDataDirName *dirNameList, SceUInt32 dirNameListNum, SceUInt32 *hitNum)
{
	if (g_initialized != 0)
		return pspSaveDataGetDirNameList(prefix, dirNameList, dirNameListNum, hitNum);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilPspSaveDataLoad(SceAppUtilPspSaveDataParams *loadParam)
{
	if (g_initialized != 0)
		return pspSaveDataLoad(loadParam);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilAddcontMount(const SceAppUtilTitleId *titleId, const SceAppUtilPassCode *passCode)
{
	if (g_initialized != 0)
		return addcontMount(titleId, 0, passCode, 0);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilAddcontUmount(void)
{
	if (g_initialized != 0)
		return addcontUmount(0);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilDrmOpen(const SceAppUtilDrmAddcontId *dirName, const SceAppUtilMountPoint *mountPoint)
{
	if (g_initialized != 0)
		return drmOpen(dirName, mountPoint);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilDrmClose(const SceAppUtilDrmAddcontId *dirName, const SceAppUtilMountPoint *mountPoint)
{
	if (g_initialized != 0)
		return drmClose(dirName, mountPoint);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilPhotoMount(void)
{
	if (g_initialized != 0)
		return photoMount();
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilPhotoUmount(void)
{
	if (g_initialized != 0)
		return photoUmount();
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilMusicMount(void)
{
	if (g_initialized != 0)
		return musicMount();
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilMusicUmount(void)
{
	if (g_initialized != 0)
		return musicUmount();
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" int SceAppUtilDevice_0AA41EAD(const char *device, SceUInt64 *maxSize, SceUInt64 *freeSize)
{
	if (g_initialized != 0)
		return deviceGetDevInfo(device, maxSize, freeSize);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" int SceAppUtilUmass_CC85F96F(int type, const char *key, AppmgrLoopBackImageInfo *info)
{
	if (g_initialized != 0)
		return umassFormat(type, key, info);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" int SceAppUtilUmass_96C76886(const char *key, SceAppUtilMountPoint *mountPoint)
{
	if (g_initialized != 0)
		return umassMount(key, mountPoint);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" int SceAppUtilUmass_6D8CF67E(void)
{
	if (g_initialized != 0)
		return umassUmount();
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" int SceAppUtilUmass_AE1230D3(void)
{
	if (g_initialized != 0)
		return umassForceUmount();
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" int SceAppUtilUmass_0BC38A89(SceUInt64 *sizes, SceUInt32 num)
{
	if (g_initialized != 0)
		return umassGetFormatSizes(sizes, num);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" int sceAppUtilCacheMount(void)
{
	if (g_initialized != 0)
		return cacheMount();
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" int sceAppUtilCacheUmount(void)
{
	if (g_initialized != 0)
		return cacheUmount();
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" int sceAppUtilCacheGetDevInfo(SceUInt64 *maxSize, SceUInt64 *freeSize)
{
	if (g_initialized != 0)
		return cacheGetDevInfo(maxSize, freeSize);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" int sceAppUtilBookMount(SceAppUtilMountPoint *mountPoint)
{
	if (g_initialized != 0)
		return bookMount(mountPoint);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" int sceAppUtilBookUmount(void)
{
	if (g_initialized != 0)
		return bookUmount();
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilSystemParamGetInt(SceSystemParamId paramId, SceInt32 *value)
{
	if (g_initialized != 0)
		return systemParamGetInt(paramId, value);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilSystemParamGetString(SceSystemParamId paramId, SceChar8 *buf, SceSize bufSize)
{
	if (g_initialized != 0)
		return systemParamGetString(paramId, buf, bufSize);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilAppParamGetInt(SceAppUtilAppParamId paramId, SceInt32 *value)
{
	if (g_initialized != 0)
		return appParamGetInt(paramId, value);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" int SceAppUtilPsm_DEB51A38(const char *id)
{
	if (g_initialized != 0)
		return initSafeMemory(id);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilSaveSafeMemory(const void *buf, SceSize bufSize, SceOff offset)
{
	if (g_initialized != 0)
		return saveSafeMemory(buf, bufSize, offset);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilLoadSafeMemory(void *buf, SceSize bufSize, SceOff offset)
{
	if (g_initialized != 0)
		return loadSafeMemory(buf, bufSize, offset);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilStoreBrowse(SceAppUtilStoreBrowseParam *param)
{
	if (g_initialized != 0)
		return storeBrowse(param);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilLaunchWebBrowser(SceAppUtilWebBrowserParam *param)
{
	if (g_initialized != 0)
		return launchWebBrowser(param);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" int SceAppUtilWebBrowserCBLimited_3ED8F841(int arg)
{
	if (g_initialized != 0)
		return webBrowserCallback(arg);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilResetCookieWebBrowser(SceAppUtilWebBrowserResetCookieParam *param)
{
	if (g_initialized != 0)
		return resetCookieWebBrowser(param);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilAddCookieWebBrowser(SceAppUtilWebBrowserAddCookieParam *param)
{
	if (g_initialized != 0)
		return addCookieWebBrowser(param);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilBgdlGetStatus(SceAppUtilBgdlStatus *stat)
{
	if (g_initialized != 0)
		return bgdlGetStatus(stat);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" int SceAppUtilNpSignin_56C91A8E(void)
{
	if (g_initialized != 0)
		return npSigninState();
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" int SceAppUtilLaunchApp_D2338D13(const SceShellUtilLaunchAppParam *param)
{
	if (g_initialized != 0)
		return launchAppRequest(param);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" int SceAppUtil_7DA34311(const SceChar8 *str, SceSize size)
{
	SceUInt32 length;
	SceUInt32 bom;
	SceUInt8 last[4];
	SceUInt8 context[128];

	if (utf8Scan(str, size, &length, &bom) != 0)
		return APPUTIL_ERROR_INVALID_STRING;
	/* a string of nothing but a byte order mark */
	if (length == 3 && bom == 0xfeff)
		return APPUTIL_ERROR_INVALID_STRING;
	sceClibMemset(context, 0, sizeof(context));
	if (utf8DecoderInit(context) != 0)
		return APPUTIL_ERROR_INVALID_STRING;
	if (utf8Decode(context, str, size, &length, last) != 0)
		return APPUTIL_ERROR_INVALID_STRING;
	return 0;
}

extern "C" int SceAppUtil_CD0E5C59(int (*callback)(int event, SceAppUtilSaveDataSlotId id, void *arg),
				   void *arg)
{
	g_slotCallback = callback;
	g_slotCallbackArg = callback != NULL ? arg : NULL;
	return 0;
}

extern "C" int SceAppUtil_F93C691C(int event, SceAppUtilSaveDataSlotId id)
{
	if (g_slotCallback != NULL)
		return g_slotCallback(event, id, g_slotCallbackArg);
	return event;
}

extern "C" int SceAppUtil_C3CB128B(int type, char *mountPoint)
{
	if (g_initialized == 0)
		return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
	if (type == APPUTIL_MOUNT_PSP_SAVEDATA_ROOT)
		return pspSaveDataRootMount(mountPoint);
	return SCE_APPUTIL_ERROR_PARAMETER;
}

extern "C" int SceAppUtil_C0418B1C(int type)
{
	if (g_initialized == 0)
		return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
	if (type == APPUTIL_MOUNT_PSP_SAVEDATA_ROOT)
		return pspSaveDataRootUmount();
	return SCE_APPUTIL_ERROR_PARAMETER;
}

extern "C" int SceAppUtil_B2FE3BB1(SceInt32 *notReady, SceInt32 *ready)
{
	if (g_initialized != 0)
		return bgdlGetAppStatus(notReady, ready);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

/* sceAppUtilSaveDataDataSave a second time, for SceAppUtilExt */
extern "C" int SceAppUtil_1E2A6158(const SceAppUtilSaveDataDataSlot *slot,
				   const SceAppUtilSaveDataDataSaveItem *data, SceUInt32 dataNum,
				   const SceAppUtilMountPoint *mountPoint, SceSize *requiredSizeKiB)
{
	if (g_initialized != 0)
		return saveDataDataSave(slot, data, dataNum, mountPoint, requiredSizeKiB);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" int sceAppUtilAddcontForceAddcontMount(const SceAppUtilTitleId *titleId,
						  const SceAppUtilPassCode *passCode)
{
	if (g_initialized != 0)
		return addcontMount(titleId, 0, passCode, 1);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilAppEventParseSessionInvitation(const SceAppUtilAppEventParam *eventParam,
							     SceAppUtilSessionInvitationParam *param)
{
	if (g_initialized != 0)
		return appEventParseSessionInvitation(eventParam, param);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

extern "C" SceInt32 sceAppUtilAppEventParseGameCustomData(const SceAppUtilAppEventParam *eventParam,
							  SceAppUtilGameCustomDataParam *param)
{
	if (g_initialized != 0)
		return appEventParseGameCustomData(eventParam, param);
	return SCE_APPUTIL_ERROR_NOT_INITIALIZED;
}

void dataParamError(int code, int dataIndex)
{
	sceClibPrintf("\n\n****** SceAppUtil Parameter Error : %d [ dataIndex : %d ] ******\n\n", code,
		      dataIndex);
}
